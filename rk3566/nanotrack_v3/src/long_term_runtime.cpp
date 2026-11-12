#include "long_term_runtime.hpp"
#include "scoped_cleanup.hpp"
#include "target_class.hpp"

#include <cassert>
#include <chrono>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace nanotrack {

const char* to_string(TemplateSlotRevocationReason reason) {
  switch (reason) {
    case TemplateSlotRevocationReason::kFrameAborted: return "FRAME_ABORTED";
    case TemplateSlotRevocationReason::kFrameApplicationFailed: return "FRAME_APPLICATION_FAILED";
    case TemplateSlotRevocationReason::kReinitialized: return "REINITIALIZED";
  }
  return "UNKNOWN";
}

LongTermRuntime::LongTermRuntime(const ControllerConfig& config,
                                 ShortTermTracker& tracker,
                                 DetectorAdapter& detector,
                                 const TargetClassRuntimeConfig& target_class_config)
    : tracker_(tracker),
      detector_(detector),
      controller_(config),
      target_class_config_(target_class_config),
      detector_configured_(detector.available()) {}

void LongTermRuntime::validate_image(const ImageView& image) {
  if (image.data == nullptr || image.width <= 0 || image.height <= 0 ||
      image.channels != 3 ||
      image.stride_bytes <
          static_cast<std::size_t>(image.width * image.channels)) {
    throw std::invalid_argument("runtime image must be a valid BGR8 view");
  }
}

TargetClassInitializationResult LongTermRuntime::initialize(
    const ImageView& image, const BBox& initial_bbox,
    std::uint64_t frame_id, std::int64_t timestamp_ms) {
  if (frame_open_) throw std::logic_error("cannot initialize an open runtime frame");
  if (revocation_waiting_for_ack_) throw std::logic_error("template slot revocation has not been logged");
  validate_image(image);
  validate_bbox(initial_bbox, image.width, image.height);

  TargetClassInitializationResult result;
  result.initialization_index = last_initialization_.initialization_index + 1;
  result.frame_id = frame_id;
  result.timestamp_ms = timestamp_ms;
  result.original_roi = initial_bbox;
  result.requested_class_id = target_class_config_.requested_class_id;
  result.target_class = make_unknown_target_class(TargetClassReason::kDetectorUnavailable);
  if (target_class_config_.requested_class_id >= 0) {
    result.target_class = make_fixed_target_class(
        target_class_config_.requested_class_id, detector_.available());
  } else if (target_class_config_.class_count > 0) {
    // The adapter owns request IDs and quiesces old async work. Initialization
    // never borrows a controller recovery request or its eligibility generation.
    const DetectorRequest request{frame_id, timestamp_ms,
        BBox{0, 0, float(image.width), float(image.height)},
        std::numeric_limits<std::size_t>::max()};
    std::optional<DetectorBlockingResult> detected;
    const auto detector_start = std::chrono::steady_clock::now();
    try {
      // Even a disabled adapter may still hold an engine error to propagate.
      detected = detector_.detect_blocking(image, request);
    } catch (const std::exception& error) {
      result.detector_error = error.what();
      result.target_class = make_unknown_target_class(TargetClassReason::kDetectorError);
    } catch (...) {
      result.detector_error = "unknown detector initialization error";
      result.target_class = make_unknown_target_class(TargetClassReason::kDetectorError);
    }
    result.detector_ms = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - detector_start).count();
    if (detected) {
      result.request_id = detected->request_id;
      result.batch = std::move(detected->batch);
      result.detector_timing = result.batch->timing;
      result.target_class = establish_target_class(
          initial_bbox, FrameSize{image.width, image.height},
          result.batch->candidates, target_class_config_.class_count,
          target_class_config_.match_config);
    }
  }

  initialized_ = false;
  abandon_staged_recent(TemplateSlotRevocationReason::kReinitialized);
  controller_.initialize(initial_bbox, frame_id, timestamp_ms,
                         FrameSize{image.width, image.height}, result.target_class);
  tracker_.initialize(image, initial_bbox);
  last_initialization_ = result;
  initialized_ = true;
  return result;
}

RuntimeFrameResult LongTermRuntime::process_frame(const ImageView& image,
                                                  std::uint64_t frame_id,
                                                  std::int64_t timestamp_ms) {
  if (!initialized_) throw std::logic_error("long-term runtime is not initialized");
  if (frame_open_) throw std::logic_error("previous runtime frame is unfinished");
  if (revocation_waiting_for_ack_) throw std::logic_error("template slot revocation has not been logged");
  const auto total_start = std::chrono::steady_clock::now();
  RuntimeFrameResult result;
  result.target_class = last_initialization_.target_class;
  result.detector.configured = detector_configured_;
  bool completed = false;
  const auto slot_before_frame = staged_template_slot_id_;
  std::optional<LongTermController> application_checkpoint;
  std::optional<BBox> bbox_restore;
  const auto rollback_application = [&](std::optional<TemplateSlotRevocationReason> reason) noexcept {
    // A prepare can fail after allocating a new slot in this frame. Only a
    // slot that predated the failed frame could already be visible in the log.
    abandon_staged_recent(staged_template_slot_id_ == slot_before_frame
                              ? reason : std::nullopt);
    if (bbox_restore) {
      tracker_.restore_bbox(*bbox_restore);
      bbox_restore.reset();
    }
    if (application_checkpoint) {
      controller_.rollback_frame_application(std::move(*application_checkpoint));
      application_checkpoint.reset();
    } else {
      controller_.abort_frame();
      controller_.discard_template_probation();
    }
  };
  frame_open_ = true;
  ScopedCleanup frame_cleanup(
      [&]() noexcept {
        if (!completed) {
          rollback_application(application_checkpoint
              ? TemplateSlotRevocationReason::kFrameApplicationFailed
              : TemplateSlotRevocationReason::kFrameAborted);
        }
        frame_open_ = false;
        return 0;
      },
      [](int) noexcept {});
  validate_image(image);
  result.detector.ready = detector_.available() && detector_.can_submit();
  result.plan = controller_.begin_frame(frame_id, timestamp_ms,
                                        result.detector.ready);

  if (result.plan.request_detector && result.plan.detector_request) {
    result.detector.request_frame_id =
        result.plan.detector_request->frame_id;
    try {
      detector_.submit(image, *result.plan.detector_request);
      result.detector.request_submitted = true;
    } catch (const std::exception& error) {
      result.detector.error = error.what();
    }
  }

  std::optional<DetectorBatch> detector_batch;
  if (detector_.available() && result.detector.error.empty()) {
    try {
      detector_batch = detector_.poll();
    } catch (const std::exception& error) {
      result.detector.error = error.what();
    }
    if (detector_batch) {
      result.detector.batch_received = true;
      result.detector.batch_frame_id = detector_batch->source_frame_id;
      result.detector.batch_age_ms =
          timestamp_ms - detector_batch->source_timestamp_ms;
      result.detector.raw_candidate_count =
          detector_batch->raw_candidate_count;
      result.detector.filtered_candidate_count =
          detector_batch->filtered_candidate_count;
      result.detector.timing = detector_batch->timing;
    }
  }

  if (result.plan.run_tracker) {
    const auto tracker_start = std::chrono::steady_clock::now();
    result.observations = tracker_.infer(image, result.plan, result.template_heads);
    result.tracker_ms = std::chrono::duration<float, std::milli>(
                            std::chrono::steady_clock::now() - tracker_start)
                            .count();
  } else {
    result.tracker_skipped = true;
  }
  // The finish call updates bounded tracking state in place. Keep its input
  // state until every fallible tracker callback has completed. The controller
  // already holds the pre-prediction checkpoint for the open frame.
  application_checkpoint.emplace(controller_);
  result.output =
      controller_.finish_frame(result.plan, result.observations, detector_batch);
  const auto& transition = result.output.template_transition;
  const bool recovery_prepare = transition && transition->recovery &&
      transition->action == TemplateAction::kStageRecent;
  if (result.output.accepted) {
    bbox_restore = application_checkpoint->current_bbox();
    if (!recovery_prepare) tracker_.commit_bbox(result.output.tracking_bbox);
  }
  if (transition) {
    const auto applied = apply_template_transition(image, *transition);
    if (transition->recovery && transition->action == TemplateAction::kCommitRecent &&
        applied == TemplateTransitionResult::kTrackerRejected) {
      const auto rejected = *transition;
      if (!controller_.resolve_template_transition(rejected.transaction_id, false)) {
        throw std::logic_error("template transition acknowledgement did not match its request");
      }
      const char* reason = result.output.template_transition_reason;
      rollback_application(std::nullopt);
      // Evaluate an observation-free frame from the last trusted motion state.
      // This advances the timestamp once and rechecks search authorization.
      const auto search = controller_.begin_frame(frame_id, timestamp_ms, false);
      result.output = controller_.finish_frame(search, {}, std::nullopt);
      result.output.template_action = rejected.action;
      result.output.template_transition = rejected;
      result.output.template_transition_result = TemplateTransitionResult::kTrackerRejected;
      result.output.template_transition_reason = reason;
      result.output.state_change_reason = "RECOVERY_COMMIT_REJECTED";
      result.output.recovery_reason = "RECOVERY_COMMIT_REJECTED";
    } else if (!controller_.publish_template_transition(result.output, applied)) {
      throw std::logic_error("template transition acknowledgement did not match its request");
    }
  }
  result.runtime_ms = std::chrono::duration<float, std::milli>(
                          std::chrono::steady_clock::now() - total_start)
                          .count();
  result.template_slot_revocation = pending_template_slot_revocation_;
  revocation_waiting_for_ack_ = result.template_slot_revocation.has_value();
  application_checkpoint.reset();
  bbox_restore.reset();
  completed = true;
  return result;
}

bool LongTermRuntime::acknowledge_template_slot_revocation(std::uint64_t slot_id) noexcept {
  if (!revocation_waiting_for_ack_ ||
      pending_template_slot_revocation_->slot_id != slot_id) return false;
  pending_template_slot_revocation_.reset();
  revocation_waiting_for_ack_ = false;
  return true;
}

void LongTermRuntime::abandon_staged_recent(
    std::optional<TemplateSlotRevocationReason> reason) noexcept {
  if (staged_template_slot_id_) {
    if (reason) {
      // A pending undelivered revocation owns no physical slot. Once a new
      // slot is returned alongside it, the writer gate prevents further work.
      assert(!pending_template_slot_revocation_);
      pending_template_slot_revocation_ = TemplateSlotRevocation{*staged_template_slot_id_, *reason};
    }
    if (!tracker_.abort_recent(*staged_template_slot_id_)) {
      tracker_.discard_staged_recent();
    }
    staged_template_slot_id_.reset();
  }
}

TemplateTransitionResult LongTermRuntime::apply_template_transition(
    const ImageView& image, const TemplateTransitionRequest& request) {
  const auto slot = request.template_slot_id;
  auto result = TemplateTransitionResult::kTrackerRejected;
  switch (request.action) {
    case TemplateAction::kStageRecent:
      // Own the slot before extraction so even a throw after allocation is
      // cleaned up using the exact same ID.
      staged_template_slot_id_ = slot;
      try {
        tracker_.prepare_recent(image, request.bbox, slot);
      } catch (...) {
        abandon_staged_recent();
        result = TemplateTransitionResult::kPrepareFailed;
        break;
      }
      if (request.recovery) tracker_.commit_bbox(request.bbox);
      result = TemplateTransitionResult::kApplied;
      break;
    case TemplateAction::kCommitRecent:
      if (tracker_.commit_recent(slot)) {
        result = TemplateTransitionResult::kApplied;
        staged_template_slot_id_.reset();
      } else {
        if (!tracker_.abort_recent(slot)) tracker_.discard_staged_recent();
        staged_template_slot_id_.reset();
      }
      break;
    case TemplateAction::kRollbackRecent:
      if (tracker_.abort_recent(slot)) result = TemplateTransitionResult::kApplied;
      else tracker_.discard_staged_recent();
      staged_template_slot_id_.reset();
      break;
    case TemplateAction::kNone: break;
  }
  return result;
}

}  // namespace nanotrack
