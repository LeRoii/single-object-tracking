#include "long_term_runtime.hpp"
#include "scoped_cleanup.hpp"
#include "target_class.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace nanotrack {

namespace {

float bright_fraction(const ImageView& image, const BBox& box) {
  const int left = std::clamp(static_cast<int>(std::lround(box.x)), 0, image.width - 1);
  const int top = std::clamp(static_cast<int>(std::lround(box.y)), 0, image.height - 1);
  const int right = std::clamp(left + std::max(1, static_cast<int>(std::lround(box.width))),
                               left + 1, image.width);
  const int bottom = std::clamp(top + std::max(1, static_cast<int>(std::lround(box.height))),
                                top + 1, image.height);
  std::size_t bright = 0;
  for (int y = top; y < bottom; ++y) {
    const auto* row = image.data + static_cast<std::size_t>(y) * image.stride_bytes;
    for (int x = left; x < right; ++x) {
      const auto* pixel = row + static_cast<std::size_t>(x) * 3;
      const int gray = static_cast<int>(std::lround(
          0.114 * pixel[0] + 0.587 * pixel[1] + 0.299 * pixel[2]));
      bright += gray > 150;
    }
  }
  return static_cast<float>(bright) /
      static_cast<float>((right - left) * (bottom - top));
}

float median_brightness(const std::deque<float>& history) {
  std::vector<float> values(history.begin(), history.end());
  std::sort(values.begin(), values.end());
  const std::size_t middle = values.size() / 2;
  return values.size() % 2 ? values[middle] :
      0.5F * (values[middle - 1] + values[middle]);
}

std::optional<BBox> select_tracking_size_candidate(
    const DetectorBatch& batch, const BBox& tracker_bbox,
    std::optional<int> target_class_id, const ControllerConfig& config) {
  if (config.detector_target_mode != DetectorTargetMode::kFullObject ||
      !target_class_id || (*target_class_id != 0 && *target_class_id != 2)) {
    return std::nullopt;
  }
  const float cx = tracker_bbox.x + 0.5F * tracker_bbox.width;
  const float cy = tracker_bbox.y + 0.5F * tracker_bbox.height;
  const float diagonal = std::hypot(tracker_bbox.width, tracker_bbox.height);
  std::optional<BBox> unique;
  for (const auto& candidate : batch.candidates) {
    const auto& box = candidate.bbox;
    if (candidate.class_id != *target_class_id ||
        !std::isfinite(candidate.confidence) ||
        candidate.confidence < config.suspected_detector_guard_min_confidence ||
        !std::isfinite(box.x) || !std::isfinite(box.y) ||
        !std::isfinite(box.width) || !std::isfinite(box.height) ||
        box.width <= 0.0F || box.height <= 0.0F ||
        cx < box.x || cx > box.x + box.width ||
        cy < box.y || cy > box.y + box.height ||
        std::hypot(box.x + 0.5F * box.width - cx,
                   box.y + 0.5F * box.height - cy) >
            config.detector_confirmation_center_distance * diagonal ||
        // A detector box 1.5x larger in each dimension has IoU near 0.44
        // with a centered tracker box. Keep that valid scale evidence while
        // the independent center and temporal-confirmation gates protect identity.
        bbox_iou(box, tracker_bbox) <
            0.8F * config.detector_confirmation_iou) {
      continue;
    }
    if (unique) return std::nullopt;
    unique = box;
  }
  return unique;
}

struct VisualCrop {
  std::vector<std::uint8_t> pixels;
  int left = 0;
  int top = 0;
  int side = 0;
};

VisualCrop make_visual_crop(const ImageView& image, const BBox& predicted) {
  VisualCrop crop;
  crop.side = std::min({image.width, image.height,
      std::max(120, static_cast<int>(std::lround(
          3.0F * std::max(predicted.width, predicted.height))))});
  crop.left = std::clamp(static_cast<int>(std::lround(
      predicted.x + predicted.width * 0.5F - crop.side * 0.5F)),
      0, image.width - crop.side);
  crop.top = std::clamp(static_cast<int>(std::lround(
      predicted.y + predicted.height * 0.5F - crop.side * 0.5F)),
      0, image.height - crop.side);
  crop.pixels.assign(640U * 640U * 3U, 114U);
  // Match the existing ROI replay: a 3x target crop is resized into the
  // center 320x320 region, leaving 160px of gray padding on each side.
  for (int y = 0; y < 320; ++y) {
    const float source_y = (static_cast<float>(y) + 0.5F) * crop.side / 320.0F - 0.5F;
    const int y0 = std::clamp(static_cast<int>(std::floor(source_y)), 0, crop.side - 1);
    const int y1 = std::min(y0 + 1, crop.side - 1);
    const float fy = std::clamp(source_y - std::floor(source_y), 0.0F, 1.0F);
    for (int x = 0; x < 320; ++x) {
      const float source_x = (static_cast<float>(x) + 0.5F) * crop.side / 320.0F - 0.5F;
      const int x0 = std::clamp(static_cast<int>(std::floor(source_x)), 0, crop.side - 1);
      const int x1 = std::min(x0 + 1, crop.side - 1);
      const float fx = std::clamp(source_x - std::floor(source_x), 0.0F, 1.0F);
      auto* target = crop.pixels.data() + ((y + 160) * 640 + x + 160) * 3;
      for (int channel = 0; channel < 3; ++channel) {
        const auto sample = [&](int sx, int sy) {
          return image.data[static_cast<std::size_t>(crop.top + sy) * image.stride_bytes +
                            static_cast<std::size_t>(crop.left + sx) * 3 + channel];
        };
        const float upper = sample(x0, y0) * (1.0F - fx) + sample(x1, y0) * fx;
        const float lower = sample(x0, y1) * (1.0F - fx) + sample(x1, y1) * fx;
        target[channel] = static_cast<std::uint8_t>(std::lround(
            upper * (1.0F - fy) + lower * fy));
      }
    }
  }
  return crop;
}

}  // namespace

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
      adaptive_display_(config),
      target_class_config_(target_class_config),
      detector_configured_(detector.available()),
      visual_suspect_bright_ratio_(config.visual_suspect_bright_ratio),
      visual_negative_clear_veto_bright_ratio_(config.visual_negative_clear_veto_bright_ratio),
      lower_band_visibility_veto_(config.lower_band_visibility_veto),
      visual_suspect_min_confidence_(config.visual_suspect_min_confidence),
      visual_suspect_max_center_px_(config.visual_suspect_max_center_px),
      visual_suspect_consecutive_frames_(config.visual_suspect_consecutive_frames) {}

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
    std::uint64_t frame_id, std::int64_t timestamp_ms,
    InitializationClassSelection selection) {
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
  if (selection.mode != InitializationClassMode::kConfigured) {
    result.requested_class_id = -1;
    result.detector_error = std::move(selection.detector_error);
    if (selection.detection) {
      result.request_id = selection.detection->request_id;
      result.batch = std::move(selection.detection->batch);
      result.detector_timing = result.batch->timing;
    }
    if (selection.mode == InitializationClassMode::kClickedDetector) {
      if (!result.batch ||
          selection.selected_candidate_index >= result.batch->candidates.size()) {
        throw std::invalid_argument("clicked detector candidate is missing");
      }
      const DetectorCandidate& candidate =
          result.batch->candidates[selection.selected_candidate_index];
      result.target_class = establish_target_class(
          initial_bbox, FrameSize{image.width, image.height}, {candidate},
          target_class_config_.class_count, target_class_config_.match_config);
      if (!result.target_class.selected_candidate) {
        throw std::invalid_argument("clicked detector candidate is invalid");
      }
      result.target_class.selected_candidate->original_index =
          selection.selected_candidate_index;
      result.target_class.candidates.front().original_index =
          selection.selected_candidate_index;
    } else {
      result.target_class = make_unknown_target_class(
          result.detector_error.empty() ? TargetClassReason::kNoMatch
                                        : TargetClassReason::kDetectorError);
    }
  } else if (target_class_config_.requested_class_id >= 0) {
    result.target_class = make_fixed_target_class(
        target_class_config_.requested_class_id, detector_.available());
  } else if (target_class_config_.class_count > 0) {
    // The adapter owns request IDs and quiesces old async work. Initialization
    // never borrows a controller recovery request or its eligibility generation.
    const DetectorRequest request{frame_id, timestamp_ms,
        BBox{0, 0, float(image.width), float(image.height)},
        std::numeric_limits<std::size_t>::max(), 0,
        DetectorRequestPurpose::kInitialization};
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
  adaptive_display_.initialize(initial_bbox, result.target_class,
                               FrameSize{image.width, image.height});
  result.display_bbox = adaptive_display_.current();
  tracker_.initialize(image, initial_bbox);
  trusted_brightness_.clear();
  if (visual_suspect_bright_ratio_ > 0.0F || lower_band_visibility_veto_)
    trusted_brightness_.push_back(bright_fraction(image, initial_bbox));
  last_visual_hit_frame_.reset();
  visual_hit_streak_ = 0;
  visual_streak_ambiguous_ = false;
  guard_observations_.clear();
  last_guard_request_ms_.reset();
  last_guard_batch_frame_id_.reset();
  last_guard_source_ms_.reset();
  last_guard_candidate_bbox_.reset();
  published_tracking_bbox_.reset();
  published_center_offset_x_ = 0.0F;
  published_center_offset_y_ = 0.0F;
  last_supported_published_bbox_.reset();
  last_published_support_ms_.reset();
  pending_tracking_size_candidate_.reset();
  tracking_size_candidate_hits_ = 0;
  confirmed_tracking_size_candidate_.reset();
  confirmed_tracking_size_source_ms_.reset();
  last_tracking_size_batch_frame_id_.reset();
  last_guard_confidence_.reset();
  last_display_motion_support_.reset();
  guard_hits_ = 0;
  guard_stream_active_ = false;
  guard_protection_active_ = false;
  guarded_visual_misses_ = 0;
  previous_output_phase_ = TrackingPhase::kTracking;
  last_initialization_ = result;
  initialized_ = true;
  return result;
}

RuntimeFrameResult LongTermRuntime::process_frame(const ImageView& image,
                                                  std::uint64_t frame_id,
                                                  std::int64_t timestamp_ms,
                                                  bool lower_band_crossing) {
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
  const auto& guard_config = controller_.config();
  const bool guard_class = result.target_class.target_class_id &&
      (*result.target_class.target_class_id == 0 ||
       *result.target_class.target_class_id == 2);
  const bool guard_stream = guard_config.suspected_detector_guard_enabled &&
      guard_class && (result.plan.tracking_phase == TrackingPhase::kSuspected ||
                      guard_stream_active_);

  std::optional<DetectorRequest> display_request;
  if (result.plan.request_detector && result.plan.detector_request) {
    result.plan.detector_request->purpose = DetectorRequestPurpose::kSearchRecovery;
    result.detector.request_frame_id =
        result.plan.detector_request->frame_id;
    result.detector.request_purpose = result.plan.detector_request->purpose;
    try {
      detector_.submit(image, *result.plan.detector_request);
      result.detector.request_submitted = true;
    } catch (const std::exception& error) {
      result.detector.error = error.what();
    }
  } else if (!guard_stream &&
             !(visual_suspect_bright_ratio_ > 0.0F &&
               previous_output_phase_ == TrackingPhase::kSuspected) &&
             adaptive_display_.should_request(result.plan.tracking_phase,
                                              timestamp_ms,
                                              result.detector.ready)) {
    display_request = adaptive_display_.make_request(frame_id, timestamp_ms);
    result.detector.request_frame_id = frame_id;
    result.detector.request_purpose = display_request->purpose;
    try {
      detector_.submit(image, *display_request);
      adaptive_display_.mark_request_submitted(timestamp_ms);
      result.detector.request_submitted = true;
    } catch (const std::exception& error) {
      result.detector.error = error.what();
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
  if (lower_band_visibility_veto_ && lower_band_crossing &&
      result.target_class.target_class_id == 2 && trusted_brightness_.size() >= 3) {
    const auto recent = std::find_if(result.observations.begin(),
                                     result.observations.end(),
                                     [](const TrackerObservation& observation) {
                                       return observation.template_kind == TemplateKind::kRecent;
                                     });
    if (recent != result.observations.end() &&
        bright_fraction(image, recent->bbox) <
            0.9F * median_brightness(trusted_brightness_)) {
      result.plan.visual_occlusion_search = true;
    }
  }
  bool visual_hit = false;
  bool visual_ambiguous = false;
  bool visual_blocking_used = false;
  bool visual_batch_valid = false;
  if (visual_suspect_bright_ratio_ > 0.0F &&
      previous_output_phase_ == TrackingPhase::kSuspected &&
      result.target_class.target_class_id == 2 &&
      detector_.available() && trusted_brightness_.size() >= 3) {
    const auto recent = std::find_if(result.observations.begin(),
                                     result.observations.end(),
                                     [](const TrackerObservation& observation) {
                                       return observation.template_kind == TemplateKind::kRecent;
                                     });
    const float brightness_reference = median_brightness(trusted_brightness_);
    const float observed_brightness = recent != result.observations.end()
        ? bright_fraction(image, recent->bbox) : 0.0F;
    if (recent != result.observations.end() &&
        observed_brightness < visual_suspect_bright_ratio_ * brightness_reference) {
      if (brightness_reference > 0.0F) {
        result.visual_probe_brightness_ratio = observed_brightness / brightness_reference;
      }
      result.visual_probe_ran = true;
      const auto crop = make_visual_crop(image, result.plan.motion_predicted_bbox);
      const ImageView canvas{crop.pixels.data(), 640, 640, 640U * 3U, 3};
      const DetectorRequest request{frame_id, timestamp_ms,
          BBox{0.0F, 0.0F, 640.0F, 640.0F}, 100, 0,
          DetectorRequestPurpose::kVisualOcclusion};
      visual_blocking_used = true;
      result.detector.request_frame_id = frame_id;
      result.detector.request_purpose = request.purpose;
      try {
        const auto detected = detector_.detect_blocking(canvas, request);
        if (detected) {
          visual_batch_valid = detected->batch.source_frame_id == frame_id &&
              detected->batch.purpose == DetectorRequestPurpose::kVisualOcclusion;
          result.detector.request_submitted = true;
          result.detector.batch_received = true;
          result.detector.batch_frame_id = detected->batch.source_frame_id;
          result.detector.batch_purpose = detected->batch.purpose;
          result.detector.batch_age_ms = timestamp_ms - detected->batch.source_timestamp_ms;
          result.detector.raw_candidate_count = detected->batch.raw_candidate_count;
          result.detector.filtered_candidate_count = detected->batch.filtered_candidate_count;
          result.detector.timing = detected->batch.timing;
          float nearest_distance = std::numeric_limits<float>::infinity();
          float nearest_confidence = 0.0F;
          int nearby_candidates = 0;
          for (const auto& candidate : detected->batch.candidates) {
            if (candidate.class_id != 2) continue;
            const float factor = static_cast<float>(crop.side) / 320.0F;
            const float center_x = crop.left +
                (candidate.bbox.x + 0.5F * candidate.bbox.width - 160.0F) * factor;
            const float center_y = crop.top +
                (candidate.bbox.y + 0.5F * candidate.bbox.height - 160.0F) * factor;
            const auto& predicted = result.plan.motion_predicted_bbox;
            const float distance = std::hypot(
                center_x - predicted.x - 0.5F * predicted.width,
                center_y - predicted.y - 0.5F * predicted.height);
            if (distance < nearest_distance) {
              nearest_distance = distance;
              nearest_confidence = candidate.confidence;
            }
            if (distance <= visual_suspect_max_center_px_ &&
                candidate.confidence >= visual_suspect_min_confidence_) {
              ++nearby_candidates;
            }
          }
          visual_hit = detected->batch.source_frame_id == frame_id &&
              detected->batch.purpose == DetectorRequestPurpose::kVisualOcclusion &&
              nearest_distance <= visual_suspect_max_center_px_ &&
              nearest_confidence >= visual_suspect_min_confidence_;
          visual_ambiguous = nearby_candidates > 1;
          result.visual_probe_hit = visual_hit;
          result.visual_probe_nearby_candidates = nearby_candidates;
          if (std::isfinite(nearest_distance)) {
            result.visual_probe_nearest_distance_px = nearest_distance;
          }
        }
      } catch (const std::exception& error) {
        result.detector.error = error.what();
      }
    }
  }
  if (visual_hit) {
    const bool consecutive = last_visual_hit_frame_ &&
        *last_visual_hit_frame_ + 1 == frame_id;
    const int next_streak = consecutive ? visual_hit_streak_ + 1 : 1;
    const bool ambiguous_streak = visual_ambiguous ||
        (consecutive && visual_streak_ambiguous_);
    // A decoded Safe observation agreeing with Recent supplies an independent
    // tracker crosscheck for this visual hit. A mere request to run Safe is
    // not sufficient. The first confirmed visual hit can stop observation.
    const auto recent = std::find_if(result.observations.begin(),
                                     result.observations.end(),
                                     [frame_id](const TrackerObservation& observation) {
                                       return observation.frame_id == frame_id &&
                                           observation.template_kind == TemplateKind::kRecent;
                                     });
    const auto safe = std::find_if(result.observations.begin(),
                                   result.observations.end(),
                                   [frame_id](const TrackerObservation& observation) {
                                     return observation.frame_id == frame_id &&
                                         observation.template_kind == TemplateKind::kSafe;
                                   });
    const bool safe_crosscheck = recent != result.observations.end() &&
        safe != result.observations.end() &&
        bbox_iou(recent->bbox, safe->bbox) >= 0.5F;
    result.plan.visual_occlusion_search = safe_crosscheck ||
        next_streak >= visual_suspect_consecutive_frames_ +
            (ambiguous_streak ? 1 : 0);
  }
  if (visual_batch_valid && !visual_hit && result.visual_probe_brightness_ratio &&
      visual_negative_clear_veto_bright_ratio_ > 0.0F &&
      *result.visual_probe_brightness_ratio <=
          visual_negative_clear_veto_bright_ratio_) {
    result.plan.visual_negative_clear_veto = true;
  }
  // Detector support that rescued a visible track must not keep protecting
  // it after the visual probe has failed to find nearby evidence twice in
  // consecutive SUSPECTED frames. Unprotected tracks keep the existing
  // uncertainty budget, since a detector miss alone is not proof of loss.
  if (visual_batch_valid) {
    guarded_visual_misses_ = visual_hit ? 0 : guarded_visual_misses_ + 1;
    if (guard_protection_active_ && guarded_visual_misses_ >= 2 &&
        previous_output_phase_ == TrackingPhase::kSuspected) {
      result.plan.visual_occlusion_search = true;
    }
  } else {
    guarded_visual_misses_ = 0;
  }
  if (guard_stream && !visual_blocking_used &&
      !result.plan.request_detector && result.detector.ready &&
      (!last_guard_request_ms_ ||
       timestamp_ms - *last_guard_request_ms_ >= guard_config.detector_interval_ms)) {
    const DetectorRequest request{frame_id, timestamp_ms,
        BBox{0.0F, 0.0F, float(image.width), float(image.height)},
        std::numeric_limits<std::size_t>::max(), 0,
        DetectorRequestPurpose::kSuspectedValidation};
    result.detector.request_frame_id = frame_id;
    result.detector.request_purpose = request.purpose;
    try {
      detector_.submit(image, request);
      result.detector.request_submitted = true;
      last_guard_request_ms_ = timestamp_ms;
    } catch (const std::exception& error) {
      result.detector.error = error.what();
    }
  }
  // Poll after inference so a detector result completed while NanoTrack ran
  // can still participate in this frame's recovery decision without waiting.
  std::optional<DetectorBatch> detector_batch;
  if (!visual_blocking_used && detector_.available() && result.detector.error.empty()) {
    try {
      detector_batch = detector_.poll();
    } catch (const std::exception& error) {
      result.detector.error = error.what();
    }
    if (detector_batch) {
      result.detector.batch_received = true;
      result.detector.batch_frame_id = detector_batch->source_frame_id;
      result.detector.batch_purpose = detector_batch->purpose;
      result.detector.batch_age_ms =
          timestamp_ms - detector_batch->source_timestamp_ms;
      result.detector.raw_candidate_count =
          detector_batch->raw_candidate_count;
      result.detector.filtered_candidate_count =
          detector_batch->filtered_candidate_count;
      result.detector.timing = detector_batch->timing;
    }
  }

  std::optional<DetectorBatch> recovery_batch;
  std::optional<DetectorBatch> display_batch;
  std::optional<DetectorBatch> guard_batch;
  if (detector_batch) {
    if (detector_batch->purpose == DetectorRequestPurpose::kDisplayRefresh) {
      display_batch = detector_batch;
    } else if (detector_batch->purpose == DetectorRequestPurpose::kSearchRecovery) {
      recovery_batch = detector_batch;
    } else if (detector_batch->purpose == DetectorRequestPurpose::kSuspectedValidation) {
      guard_batch = detector_batch;
    }
  }
  auto next_guard_hits = guard_hits_;
  auto next_guard_batch_frame_id = last_guard_batch_frame_id_;
  auto next_guard_source_ms = last_guard_source_ms_;
  auto next_guard_candidate_bbox = last_guard_candidate_bbox_;
  auto next_guard_confidence = last_guard_confidence_;
  const auto recent = std::find_if(result.observations.begin(),
      result.observations.end(), [](const TrackerObservation& observation) {
        return observation.template_kind == TemplateKind::kRecent;
      });
  if (guard_batch) {
    std::optional<BBox> source_recent;
    if (guard_batch->source_frame_id == frame_id &&
        recent != result.observations.end()) {
      source_recent = recent->bbox;
    } else {
      const auto source = std::find_if(guard_observations_.begin(),
          guard_observations_.end(), [&](const GuardObservation& observation) {
            return observation.frame_id == guard_batch->source_frame_id;
          });
      if (source != guard_observations_.end()) source_recent = source->recent_bbox;
    }
    const auto age = timestamp_ms - guard_batch->source_timestamp_ms;
    const bool fresh = guard_class && age >= 0 &&
        age <= guard_config.detector_stale_ms &&
        guard_batch->source_frame_id <= frame_id &&
        (!next_guard_batch_frame_id ||
         guard_batch->source_frame_id > *next_guard_batch_frame_id);
    const DetectorCandidate* match = nullptr;
    int matches = 0;
    if (fresh && source_recent && recent != result.observations.end()) {
      for (const auto& candidate : guard_batch->candidates) {
        if (candidate.class_id != *result.target_class.target_class_id ||
            !std::isfinite(candidate.confidence) ||
            candidate.confidence < guard_config.suspected_detector_guard_min_confidence ||
            bbox_iou(candidate.bbox, *source_recent) <
                guard_config.suspected_detector_guard_min_iou ||
            bbox_iou(candidate.bbox, recent->bbox) <
                guard_config.suspected_detector_guard_min_iou * 0.6F) continue;
        match = &candidate;
        ++matches;
      }
    }
    next_guard_batch_frame_id = guard_batch->source_frame_id;
    if (matches == 1) {
      next_guard_hits = std::min(next_guard_hits + 1,
          guard_config.suspected_detector_guard_required_hits);
      next_guard_source_ms = guard_batch->source_timestamp_ms;
      next_guard_candidate_bbox = match->bbox;
      next_guard_confidence = match->confidence;
    } else {
      next_guard_hits = 0;
      next_guard_source_ms.reset();
      next_guard_candidate_bbox.reset();
      next_guard_confidence.reset();
    }
  }
  if (guard_stream && recent != result.observations.end() &&
      next_guard_hits >= guard_config.suspected_detector_guard_required_hits &&
      next_guard_source_ms && next_guard_candidate_bbox &&
      timestamp_ms >= *next_guard_source_ms &&
      timestamp_ms - *next_guard_source_ms <= guard_config.detector_stale_ms &&
      bbox_iou(*next_guard_candidate_bbox, recent->bbox) >=
          guard_config.suspected_detector_guard_min_iou * 0.6F) {
    result.plan.suspected_detector_guard = true;
  }
  result.suspected_detector_guard_hits = next_guard_hits;
  result.suspected_detector_guard_confidence = next_guard_confidence;
  // The finish call updates bounded tracking state in place. Keep its input
  // state until every fallible tracker callback has completed. The controller
  // already holds the pre-prediction checkpoint for the open frame.
  application_checkpoint.emplace(controller_);
  result.output =
      controller_.finish_frame(result.plan, result.observations, recovery_batch);
  result.suspected_detector_guard_applied =
      result.plan.suspected_detector_guard && result.output.accepted;
  if (result.plan.visual_occlusion_search &&
      result.output.tracking_phase == TrackingPhase::kSearching) {
    result.output.state_change_reason = "VISUAL_OCCLUSION_SEARCH";
  }
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
  if (display_batch) {
    adaptive_display_.ingest(*display_batch, result.output, result.target_class);
  }
  result.output.display_bbox = adaptive_display_.update(
      result.output, result.target_class, result.adaptive_display);
  result.output.bbox = result.output.display_bbox;
  // Reuse the periodic detector batch, but arbitrate tracking size separately
  // from the display envelope. Two unique, consistent full-object detections
  // can adjust the published size; NanoTrack still owns the center and crop.
  if (!result.output.accepted || result.output.reacquired ||
      result.output.tracking_phase != TrackingPhase::kTracking) {
    pending_tracking_size_candidate_.reset();
    tracking_size_candidate_hits_ = 0;
    confirmed_tracking_size_candidate_.reset();
    confirmed_tracking_size_source_ms_.reset();
  } else if (display_batch &&
             (!last_tracking_size_batch_frame_id_ ||
              display_batch->source_frame_id > *last_tracking_size_batch_frame_id_)) {
    last_tracking_size_batch_frame_id_ = display_batch->source_frame_id;
    const auto age_ms = timestamp_ms - display_batch->source_timestamp_ms;
    const auto candidate =
        result.output.selected_template == TemplateKind::kRecent &&
        result.output.quality == QualityLevel::kReliable && age_ms >= 0 &&
        age_ms <= controller_.config().detector_stale_ms
        ? select_tracking_size_candidate(*display_batch,
            result.output.tracking_bbox, result.target_class.target_class_id,
            controller_.config()) : std::nullopt;
    if (candidate) {
      tracking_size_candidate_hits_ = pending_tracking_size_candidate_ &&
          bbox_iou(*pending_tracking_size_candidate_, *candidate) >=
              controller_.config().detector_confirmation_iou
          ? std::min(tracking_size_candidate_hits_ + 1,
              controller_.config().confirmation_required) : 1;
      pending_tracking_size_candidate_ = candidate;
      if (tracking_size_candidate_hits_ >=
          controller_.config().confirmation_required) {
        confirmed_tracking_size_candidate_ = candidate;
        confirmed_tracking_size_source_ms_ = display_batch->source_timestamp_ms;
        result.detector_tracking_size_confirmed = true;
      }
    } else {
      pending_tracking_size_candidate_.reset();
      tracking_size_candidate_hits_ = 0;
    }
  }
  // Two detector-confirmed display updates bound a tracker displacement
  // without adding detector requests or feeding detector box size into motion.
  // This can replace a stale frozen velocity only when both accepted tracker
  // boxes still overlap and their measured speed is physically bounded.
  if (!result.output.accepted || result.output.reacquired ||
      result.output.tracking_phase != TrackingPhase::kTracking) {
    last_display_motion_support_.reset();
  } else if (result.adaptive_display.source == DisplayBoxSource::kDetectorConfirmed &&
             result.output.selected_template == TemplateKind::kRecent) {
    if (controller_.motion_freeze_since_ms_ &&
        controller_.motion_last_clean_trend_ &&
        last_display_motion_support_) {
      const auto& previous = *last_display_motion_support_;
      const auto& current = result.output.tracking_bbox;
      const auto& config = controller_.config();
      const auto elapsed_ms = timestamp_ms - previous.timestamp_ms;
      if (elapsed_ms > 0 && elapsed_ms <= 2 * config.detector_stale_ms &&
          bbox_iou(previous.tracking_bbox, current) >=
              config.display_detection_consistency_iou) {
        const float elapsed_s = static_cast<float>(elapsed_ms) / 1000.0F;
        const float vx = (current.x + 0.5F * current.width -
                          previous.tracking_bbox.x -
                          0.5F * previous.tracking_bbox.width) / elapsed_s;
        const float vy = (current.y + 0.5F * current.height -
                          previous.tracking_bbox.y -
                          0.5F * previous.tracking_bbox.height) / elapsed_s;
        const float diagonal = std::hypot(current.width, current.height);
        if (std::hypot(vx, vy) <=
            config.motion_max_speed_diagonals_per_s * diagonal) {
          MotionSnapshot trend = *controller_.motion_last_clean_trend_;
          trend.velocity_x += config.motion_alpha * (vx - trend.velocity_x);
          trend.velocity_y += config.motion_alpha * (vy - trend.velocity_y);
          controller_.motion_.reanchor_preserving_velocity(current, trend,
                                                           timestamp_ms);
          controller_.motion_last_clean_trend_ =
              controller_.motion_.snapshot(timestamp_ms);
          controller_.search_motion_reference_ =
              *controller_.motion_last_clean_trend_;
          controller_.search_motion_reference_held_ = true;
          result.output.motion_update_valid = true;
          result.output.motion_gate_reason = "DETECTOR_CORROBORATED_TREND";
        }
      }
    }
    last_display_motion_support_ = DetectorMotionSupport{
        timestamp_ms, result.output.tracking_bbox};
  }
  result.runtime_ms = std::chrono::duration<float, std::milli>(
                          std::chrono::steady_clock::now() - total_start)
                          .count();
  adaptive_display_.observe_runtime(result.runtime_ms);
  if (visual_hit) {
    const bool consecutive = last_visual_hit_frame_ &&
        *last_visual_hit_frame_ + 1 == frame_id;
    visual_hit_streak_ = consecutive ? visual_hit_streak_ + 1 : 1;
    visual_streak_ambiguous_ = visual_ambiguous ||
        (consecutive && visual_streak_ambiguous_);
    last_visual_hit_frame_ = frame_id;
  } else {
    visual_hit_streak_ = 0;
    visual_streak_ambiguous_ = false;
    last_visual_hit_frame_.reset();
  }
  if ((visual_suspect_bright_ratio_ > 0.0F || lower_band_visibility_veto_) &&
      result.output.accepted &&
      result.output.tracking_phase == TrackingPhase::kTracking) {
    trusted_brightness_.push_back(bright_fraction(image, result.output.tracking_bbox));
    if (trusted_brightness_.size() > 8) trusted_brightness_.pop_front();
  }
  previous_output_phase_ = result.output.tracking_phase;
  if (recent != result.observations.end()) {
    guard_observations_.push_back(GuardObservation{frame_id, recent->bbox});
    while (guard_observations_.size() > 8) guard_observations_.pop_front();
  }
  guard_stream_active_ = result.output.tracking_phase == TrackingPhase::kSuspected ||
                         result.suspected_detector_guard_applied;
  if (result.output.tracking_phase == TrackingPhase::kSearching ||
      result.output.reacquired) {
    guard_protection_active_ = false;
    guarded_visual_misses_ = 0;
  } else if (result.suspected_detector_guard_applied && result.output.accepted) {
    guard_protection_active_ = true;
  }
  if (result.output.tracking_phase == TrackingPhase::kTracking) {
    guarded_visual_misses_ = 0;
  }
  if (!guard_stream_active_) {
    next_guard_hits = 0;
    next_guard_source_ms.reset();
    next_guard_candidate_bbox.reset();
    next_guard_confidence.reset();
  }
  guard_hits_ = next_guard_hits;
  last_guard_batch_frame_id_ = next_guard_batch_frame_id;
  last_guard_source_ms_ = next_guard_source_ms;
  last_guard_candidate_bbox_ = next_guard_candidate_bbox;
  last_guard_confidence_ = next_guard_confidence;
  // The controller and NanoTrack have already committed their own box. Only
  // the published geometry uses detector evidence corroborated by an accepted
  // Recent observation. The next search crop and visual-occlusion reference
  // continue to use the internal tracker box.
  if (result.output.accepted && !result.output.reacquired &&
      result.output.accepted_observation_path == AcceptedObservationPath::kDirect &&
      result.output.selected_template == TemplateKind::kRecent) {
    const BBox tracker_box = result.output.tracking_bbox;
    const BBox previous = published_tracking_bbox_.value_or(tracker_box);
    const bool guard_size_supported = result.suspected_detector_guard_applied &&
        next_guard_candidate_bbox.has_value();
    const bool tracking_size_supported = confirmed_tracking_size_candidate_ &&
        confirmed_tracking_size_source_ms_ &&
        timestamp_ms >= *confirmed_tracking_size_source_ms_ &&
        timestamp_ms - *confirmed_tracking_size_source_ms_ <=
            controller_.config().detector_stale_ms;
    const bool size_supported = guard_size_supported || tracking_size_supported;
    const bool support_recent = last_published_support_ms_ &&
        timestamp_ms >= *last_published_support_ms_ &&
        timestamp_ms - *last_published_support_ms_ <=
            controller_.config().detector_stale_ms &&
        last_supported_published_bbox_;
    BBox target = guard_size_supported ? *next_guard_candidate_bbox :
        tracking_size_supported ? *confirmed_tracking_size_candidate_ :
        support_recent ? *last_supported_published_bbox_ : tracker_box;
    if (tracking_size_supported && !guard_size_supported) {
      // Two sensors agree on identity, but their size estimates can still
      // differ. Give the detector half of the size correction and retain the
      // NanoTrack center and the existing per-frame scale limit.
      target.width = 0.5F * (tracker_box.width + target.width);
      target.height = 0.5F * (tracker_box.height + target.height);
    }
    const float limit = std::min(0.10F,
        controller_.config().max_accepted_scale_step_ratio);
    const auto next_dimension = [&](float current, float raw, float detector) {
      const float desired = std::max(raw, detector);
      return std::clamp(desired, current * (1.0F - limit),
                        current * (1.0F + limit));
    };
    const float width = next_dimension(previous.width, tracker_box.width,
        target.width);
    const float height = next_dimension(previous.height, tracker_box.height,
        target.height);
    const float tracker_center_x = tracker_box.x + 0.5F * tracker_box.width;
    const float tracker_center_y = tracker_box.y + 0.5F * tracker_box.height;
    float desired_offset_x = 0.0F;
    float desired_offset_y = 0.0F;
    if (tracking_size_supported && !guard_size_supported) {
      const auto& detector_box = *confirmed_tracking_size_candidate_;
      const float tracker_area = tracker_box.width * tracker_box.height;
      const float detector_area = detector_box.width * detector_box.height;
      // A larger detector envelope can expose a tracker center bias caused
      // by undersizing. Equal or smaller envelopes keep the tracker center.
      // The two-hit identity gate above must already have passed.
      const float weight = std::clamp(detector_area / tracker_area - 1.0F,
                                      0.0F, 0.5F);
      desired_offset_x = weight *
          (detector_box.x + 0.5F * detector_box.width - tracker_center_x);
      desired_offset_y = weight *
          (detector_box.y + 0.5F * detector_box.height - tracker_center_y);
    }
    const float max_offset = controller_.config().accepted_center_max_correction_ratio *
        std::hypot(tracker_box.width, tracker_box.height);
    const float alpha = controller_.config().accepted_center_smoothing_alpha;
    published_center_offset_x_ = std::clamp(
        published_center_offset_x_ + alpha *
            (desired_offset_x - published_center_offset_x_),
        -max_offset, max_offset);
    published_center_offset_y_ = std::clamp(
        published_center_offset_y_ + alpha *
            (desired_offset_y - published_center_offset_y_),
        -max_offset, max_offset);
    const float center_x = tracker_center_x + published_center_offset_x_;
    const float center_y = tracker_center_y + published_center_offset_y_;
    const BBox published = clip_bbox(
        BBox{center_x - 0.5F * width, center_y - 0.5F * height,
             width, height}, image.width, image.height);
    result.detector_size_output_applied =
        std::fabs(published.width - tracker_box.width) > 0.01F ||
        std::fabs(published.height - tracker_box.height) > 0.01F;
    result.detector_center_output_applied =
        std::hypot(published_center_offset_x_,
                   published_center_offset_y_) > 0.01F;
    result.output.tracking_bbox = published;
    result.output.control_bbox = published;
    published_tracking_bbox_ = published;
    if (size_supported) {
      last_supported_published_bbox_ = published;
      last_published_support_ms_ = timestamp_ms;
    }
  } else {
    published_tracking_bbox_.reset();
    published_center_offset_x_ = 0.0F;
    published_center_offset_y_ = 0.0F;
    last_supported_published_bbox_.reset();
    last_published_support_ms_.reset();
  }
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
