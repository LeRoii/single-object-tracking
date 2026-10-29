#include "long_term_runtime.hpp"
#include "scoped_cleanup.hpp"
#include "target_class.hpp"

#include <chrono>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace nanotrack {

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
  validate_image(image);
  const auto total_start = std::chrono::steady_clock::now();
  RuntimeFrameResult result;
  result.target_class = last_initialization_.target_class;
  result.detector.configured = detector_configured_;
  result.detector.ready = detector_.available() && detector_.can_submit();
  result.plan = controller_.begin_frame(frame_id, timestamp_ms,
                                        result.detector.ready);
  frame_open_ = true;
  ScopedCleanup frame_cleanup(
      [this]() noexcept {
        controller_.abort_frame();
        frame_open_ = false;
        return 0;
      },
      [](int) noexcept {});

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
    result.observations = tracker_.infer(image, result.plan);
    result.tracker_ms = std::chrono::duration<float, std::milli>(
                            std::chrono::steady_clock::now() - tracker_start)
                            .count();
  } else {
    result.tracker_skipped = true;
  }
  result.output =
      controller_.finish_frame(result.plan, result.observations, detector_batch);
  if (result.output.accepted) tracker_.commit_bbox(result.output.bbox);
  switch (result.output.template_action) {
    case TemplateAction::kNone: break;
    case TemplateAction::kStageRecent:
      tracker_.stage_recent(image, result.output.bbox);
      break;
    case TemplateAction::kCommitRecent: tracker_.commit_recent(); break;
    case TemplateAction::kRollbackRecent: tracker_.rollback_recent(); break;
  }
  result.runtime_ms = std::chrono::duration<float, std::milli>(
                          std::chrono::steady_clock::now() - total_start)
                          .count();
  return result;
}

}  // namespace nanotrack
