#include "adaptive_display_box.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace nanotrack {
namespace {

float center_x(const BBox& bbox) { return bbox.x + 0.5F * bbox.width; }
float center_y(const BBox& bbox) { return bbox.y + 0.5F * bbox.height; }

float intersection_over_union(const BBox& a, const BBox& b) {
  const float left = std::max(a.x, b.x);
  const float top = std::max(a.y, b.y);
  const float right = std::min(a.x + a.width, b.x + b.width);
  const float bottom = std::min(a.y + a.height, b.y + b.height);
  const float intersection = std::max(0.0F, right - left) *
                             std::max(0.0F, bottom - top);
  const float total = a.width * a.height + b.width * b.height - intersection;
  return total > 0.0F ? intersection / total : 0.0F;
}

bool valid_box(const BBox& bbox) {
  return std::isfinite(bbox.x) && std::isfinite(bbox.y) &&
         std::isfinite(bbox.width) && std::isfinite(bbox.height) &&
         bbox.width > 0.0F && bbox.height > 0.0F;
}

float median3(float a, float b, float c) {
  return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

float outside_deadband(float error, float deadband, float alpha) {
  if (std::fabs(error) <= deadband) return 0.0F;
  return std::copysign(alpha * (std::fabs(error) - deadband), error);
}

}  // namespace

AdaptiveDisplayBox::AdaptiveDisplayBox(const ControllerConfig& config)
    : config_(config),
      refresh_interval_ms_(config.display_detector_initial_interval_ms) {}

void AdaptiveDisplayBox::initialize(const BBox& tracking_bbox,
                                    const TargetClassState& target_class,
                                    FrameSize frame_size) {
  frame_size_ = frame_size;
  initialized_ = true;
  initialized_from_detector_ = target_class.selected_candidate.has_value();
  first_update_ = true;
  class_id_ = target_class.target_class_id;
  pending_candidate_.reset();
  pending_confirmations_ = 0;
  correction_applied_ = false;
  reacquisition_blend_remaining_ = 0;
  last_request_ms_.reset();
  last_display_batch_frame_.reset();
  refresh_interval_ms_ = config_.display_detector_initial_interval_ms;
  refresh_paused_ = false;
  runtime_samples_.clear();
  runtime_p95_ms_ = 0.0F;
  runtime_updates_since_cadence_ = 0;
  stability_targets_.clear();
  stability_pending_jump_.reset();
  stability_motion_hold_frames_ = 0;
  stability_warmup_frames_ = 0;

  BBox envelope = tracking_bbox;
  if (target_class.selected_candidate) {
    envelope = padded(target_class.selected_candidate->clipped_bbox,
                      target_class.target_class_id);
  }
  if (target_class.target_class_id == 2) {
    envelope = clip_bbox(
        BBox{center_x(tracking_bbox) - 0.5F * envelope.width,
             center_y(tracking_bbox) - 0.5F * envelope.height,
             envelope.width, envelope.height},
        frame_size.width, frame_size.height);
  }
  display_bbox_ = clip_bbox(envelope, frame_size.width, frame_size.height);
  last_trusted_bbox_ = display_bbox_;
  set_transform(tracking_bbox, display_bbox_, 1.0F);
}

bool AdaptiveDisplayBox::should_request(TrackingPhase phase,
                                        std::int64_t timestamp_ms,
                                        bool detector_ready) const {
  if (!initialized_ || !config_.adaptive_display_enabled ||
      !config_.display_detector_refresh_enabled || !detector_ready ||
      refresh_paused_ || phase != TrackingPhase::kTracking) return false;
  if (!class_id_ || (*class_id_ != 0 && *class_id_ != 2)) return false;
  if (!last_request_ms_) return true;
  return timestamp_ms - *last_request_ms_ >= refresh_interval_ms_;
}

DetectorRequest AdaptiveDisplayBox::make_request(std::uint64_t frame_id,
                                                 std::int64_t timestamp_ms) const {
  return DetectorRequest{frame_id, timestamp_ms,
      BBox{0.0F, 0.0F, static_cast<float>(frame_size_.width),
           static_cast<float>(frame_size_.height)},
      std::numeric_limits<std::size_t>::max(), 0,
      DetectorRequestPurpose::kDisplayRefresh};
}

void AdaptiveDisplayBox::mark_request_submitted(std::int64_t timestamp_ms) {
  last_request_ms_ = timestamp_ms;
}

BBox AdaptiveDisplayBox::padded(const BBox& bbox,
                                std::optional<int> class_id) const {
  float padding_x = config_.display_unknown_padding_x;
  float padding_y = config_.display_unknown_padding_y;
  if (class_id && *class_id == 2) {
    padding_x = config_.display_car_padding_x;
    padding_y = config_.display_car_padding_y;
  } else if (class_id && *class_id == 0) {
    padding_x = config_.display_person_padding_x;
    padding_y = config_.display_person_padding_y;
  }
  return clip_bbox(BBox{bbox.x - bbox.width * padding_x,
                        bbox.y - bbox.height * padding_y,
                        bbox.width * (1.0F + 2.0F * padding_x),
                        bbox.height * (1.0F + 2.0F * padding_y)},
                   frame_size_.width, frame_size_.height);
}

BBox AdaptiveDisplayBox::projected(const BBox& tracking_bbox) const {
  const float width = std::clamp(tracking_bbox.width * transform_.width_ratio,
                                 1.0F, static_cast<float>(frame_size_.width));
  const float height = std::clamp(tracking_bbox.height * transform_.height_ratio,
                                  1.0F, static_cast<float>(frame_size_.height));
  const float cx = center_x(tracking_bbox) + transform_.offset_x * tracking_bbox.width;
  const float cy = center_y(tracking_bbox) + transform_.offset_y * tracking_bbox.height;
  return clip_bbox(BBox{cx - 0.5F * width, cy - 0.5F * height, width, height},
                   frame_size_.width, frame_size_.height);
}

void AdaptiveDisplayBox::set_transform(const BBox& tracking_bbox,
                                       const BBox& envelope,
                                       float alpha) {
  EnvelopeTransform next;
  next.offset_x = (center_x(envelope) - center_x(tracking_bbox)) /
                  std::max(1.0F, tracking_bbox.width);
  next.offset_y = (center_y(envelope) - center_y(tracking_bbox)) /
                  std::max(1.0F, tracking_bbox.height);
  // The detector supplies the display envelope for cars. Its center can
  // shift with occlusion or a neighboring car, so keep the tracker center.
  if (class_id_ == 2) {
    next.offset_x = 0.0F;
    next.offset_y = 0.0F;
  }
  next.width_ratio = envelope.width / std::max(1.0F, tracking_bbox.width);
  next.height_ratio = envelope.height / std::max(1.0F, tracking_bbox.height);
  transform_.offset_x += alpha * (next.offset_x - transform_.offset_x);
  transform_.offset_y += alpha * (next.offset_y - transform_.offset_y);
  transform_.width_ratio += alpha * (next.width_ratio - transform_.width_ratio);
  transform_.height_ratio += alpha * (next.height_ratio - transform_.height_ratio);
}

std::optional<BBox> AdaptiveDisplayBox::select_candidate(
    const DetectorBatch& batch, const BBox& tracking_bbox,
    std::optional<int> class_id) const {
  if (!class_id || (*class_id != 0 && *class_id != 2)) return std::nullopt;
  const float tx = center_x(tracking_bbox);
  const float ty = center_y(tracking_bbox);
  const BBox expected = projected(tracking_bbox);
  std::optional<BBox> best;
  float best_score = -1.0F;
  float runner_score = -1.0F;
  for (const DetectorCandidate& candidate : batch.candidates) {
    if (candidate.class_id != *class_id || !valid_box(candidate.bbox)) continue;
    const BBox clipped = clip_bbox(candidate.bbox, frame_size_.width, frame_size_.height);
    if (clipped.width <= 0.0F || clipped.height <= 0.0F ||
        tx < clipped.x || tx > clipped.x + clipped.width ||
        ty < clipped.y || ty > clipped.y + clipped.height) continue;
    const float score = 0.7F * intersection_over_union(expected, padded(clipped, class_id)) +
                        0.3F * candidate.confidence;
    if (score > best_score) {
      runner_score = best_score;
      best_score = score;
      best = padded(clipped, class_id);
    } else if (score > runner_score) {
      runner_score = score;
    }
  }
  if (best && runner_score >= 0.0F && best_score - runner_score < 0.10F) {
    return std::nullopt;
  }
  return best;
}

void AdaptiveDisplayBox::ingest(const DetectorBatch& batch,
                                const ControllerOutput& output,
                                const TargetClassState& target_class) {
  correction_applied_ = false;
  if (!initialized_ || batch.purpose != DetectorRequestPurpose::kDisplayRefresh ||
      output.tracking_phase != TrackingPhase::kTracking || !output.accepted) return;
  const std::int64_t age_ms = output.timestamp_ms - batch.source_timestamp_ms;
  if ((last_display_batch_frame_ &&
       batch.source_frame_id <= *last_display_batch_frame_) ||
      age_ms < 0 || age_ms > config_.detector_stale_ms) {
    return;
  }
  last_display_batch_frame_ = batch.source_frame_id;
  const auto candidate = select_candidate(batch, output.tracking_bbox,
                                          target_class.target_class_id);
  if (!candidate) {
    pending_candidate_.reset();
    pending_confirmations_ = 0;
    return;
  }
  if (pending_candidate_ &&
      intersection_over_union(*pending_candidate_, *candidate) >=
          config_.display_detection_consistency_iou) {
    ++pending_confirmations_;
  } else {
    pending_candidate_ = candidate;
    pending_confirmations_ = 1;
  }
  if (pending_confirmations_ >= config_.display_detection_confirmations) {
    set_transform(output.tracking_bbox, *candidate,
                  config_.display_detector_blend_alpha);
    pending_candidate_.reset();
    pending_confirmations_ = 0;
    correction_applied_ = true;
  }
}

BBox AdaptiveDisplayBox::smooth_toward(const BBox& target, float center_alpha) {
  const float max_ratio = config_.display_max_size_step_ratio;
  const float min_width = display_bbox_.width * (1.0F - max_ratio);
  const float max_width = display_bbox_.width * (1.0F + max_ratio);
  const float min_height = display_bbox_.height * (1.0F - max_ratio);
  const float max_height = display_bbox_.height * (1.0F + max_ratio);
  const float width = std::clamp(target.width, min_width, max_width);
  const float height = std::clamp(target.height, min_height, max_height);
  const float cx = center_x(display_bbox_) +
                   center_alpha * (center_x(target) - center_x(display_bbox_));
  const float cy = center_y(display_bbox_) +
                   center_alpha * (center_y(target) - center_y(display_bbox_));
  return clip_bbox(BBox{cx - 0.5F * width, cy - 0.5F * height, width, height},
                   frame_size_.width, frame_size_.height);
}

BBox AdaptiveDisplayBox::stabilized_target(const BBox& target) {
  // A causal three-frame median removes a solitary tracker spike. Keep the
  // first two samples at the initialized target, so the first real change
  // must persist for another frame before it moves the displayed box.
  if (stability_targets_.empty()) {
    stability_targets_.push_back(target);
    stability_targets_.push_back(target);
  }
  // A two-frame center blend is the most filtering a small target can afford:
  // a several-pixel delay consumes a large fraction of its visible extent.
  // Keep its size on the original responsive path.
  if (std::min(target.width, target.height) < 64.0F) {
    const BBox& previous = stability_targets_.back();
    const float alpha = config_.display_stability_center_alpha;
    const float cx = center_x(previous) +
        alpha * (center_x(target) - center_x(previous));
    const float cy = center_y(previous) +
        alpha * (center_y(target) - center_y(previous));
    stability_targets_.push_back(target);
    if (stability_targets_.size() > 5) stability_targets_.pop_front();
    stability_pending_jump_.reset();
    stability_motion_hold_frames_ = 0;
    return clip_bbox(BBox{cx - 0.5F * target.width, cy - 0.5F * target.height,
                          target.width, target.height},
                     frame_size_.width, frame_size_.height);
  }
  BBox sample = target;
  const BBox& previous = stability_targets_.back();
  const bool large_jump =
      std::fabs(center_x(target) - center_x(previous)) >
          0.5F * std::max(target.width, previous.width) ||
      std::fabs(center_y(target) - center_y(previous)) >
          0.5F * std::max(target.height, previous.height);
  if (large_jump) {
    const bool confirmed = stability_pending_jump_ &&
        std::fabs(center_x(target) - center_x(*stability_pending_jump_)) <=
            0.25F * std::max(target.width, stability_pending_jump_->width) &&
        std::fabs(center_y(target) - center_y(*stability_pending_jump_)) <=
            0.25F * std::max(target.height, stability_pending_jump_->height);
    if (confirmed) {
      stability_pending_jump_.reset();
    } else {
      stability_pending_jump_ = target;
      sample = previous;
    }
  } else {
    stability_pending_jump_.reset();
  }
  stability_targets_.push_back(sample);
  if (stability_targets_.size() > 5) stability_targets_.pop_front();
  const std::size_t count = stability_targets_.size();
  const BBox& a = stability_targets_[count - 3];
  const BBox& b = stability_targets_[count - 2];
  const BBox& c = stability_targets_[count - 1];
  const float target_cx = median3(center_x(a), center_x(b), center_x(c));
  const float target_cy = median3(center_y(a), center_y(b), center_y(c));
  const float target_width = median3(a.width, b.width, c.width);
  const float target_height = median3(a.height, b.height, c.height);
  bool sustained_motion = false;
  if (count == 5) {
    // The median of all pairwise slopes rejects one isolated jump and an
    // alternating left/right measurement while retaining sustained velocity.
    std::array<float, 10> slopes_x{};
    std::array<float, 10> slopes_y{};
    std::size_t index = 0;
    for (std::size_t i = 0; i < 5; ++i) {
      for (std::size_t j = i + 1; j < 5; ++j) {
        const float frames = static_cast<float>(j - i);
        slopes_x[index] = (center_x(stability_targets_[j]) -
                           center_x(stability_targets_[i])) / frames;
        slopes_y[index] = (center_y(stability_targets_[j]) -
                           center_y(stability_targets_[i])) / frames;
        ++index;
      }
    }
    std::sort(slopes_x.begin(), slopes_x.end());
    std::sort(slopes_y.begin(), slopes_y.end());
    const float velocity_x = 0.5F * (slopes_x[4] + slopes_x[5]);
    const float velocity_y = 0.5F * (slopes_y[4] + slopes_y[5]);
    // Bypass the strong filter only for unmistakably rapid motion. A slow
    // tracker drift on a static edge must not release a large accumulated
    // display offset in one frame.
    sustained_motion =
        std::fabs(velocity_x) > std::max(2.0F, 0.10F * display_bbox_.width) ||
        std::fabs(velocity_y) > std::max(2.0F, 0.10F * display_bbox_.height);
  }
  if (sustained_motion) stability_motion_hold_frames_ = 3;
  if (stability_motion_hold_frames_ > 0) {
    --stability_motion_hold_frames_;
    // When the image actually moves, preserve the pre-existing display path.
    // A static-scene filter must not leave a small moving target behind.
    return sample;
  }
  const float center_deadband_x = std::max(1.0F, display_bbox_.width *
      config_.display_stability_center_deadband_ratio);
  const float center_deadband_y = std::max(1.0F, display_bbox_.height *
      config_.display_stability_center_deadband_ratio);
  const float size_deadband_w = std::max(0.5F, display_bbox_.width *
      config_.display_stability_size_deadband_ratio);
  const float size_deadband_h = std::max(0.5F, display_bbox_.height *
      config_.display_stability_size_deadband_ratio);
  // Alternating measurements are likely box jitter. A consistent direction
  // across three raw frames is treated as real target motion. A large residual
  // also gets the fast path so a true jump does not remain far behind.
  const bool moving_x = (center_x(b) - center_x(a)) *
      (center_x(c) - center_x(b)) > 1.0F;
  const bool moving_y = (center_y(b) - center_y(a)) *
      (center_y(c) - center_y(b)) > 1.0F;
  const float error_x = target_cx - center_x(display_bbox_);
  const float error_y = target_cy - center_y(display_bbox_);
  const float alpha_x = moving_x || std::fabs(error_x) > 8.0F * center_deadband_x
      ? config_.display_stability_center_alpha
      : 0.20F * config_.display_stability_center_alpha;
  const float alpha_y = moving_y || std::fabs(error_y) > 8.0F * center_deadband_y
      ? config_.display_stability_center_alpha
      : 0.20F * config_.display_stability_center_alpha;
  const float cx = center_x(display_bbox_) + outside_deadband(
      error_x, center_deadband_x, alpha_x);
  const float cy = center_y(display_bbox_) + outside_deadband(
      error_y, center_deadband_y, alpha_y);
  const float width = display_bbox_.width + outside_deadband(
      target_width - display_bbox_.width, size_deadband_w,
      config_.display_stability_size_alpha);
  const float height = display_bbox_.height + outside_deadband(
      target_height - display_bbox_.height, size_deadband_h,
      config_.display_stability_size_alpha);
  return clip_bbox(BBox{cx - 0.5F * width, cy - 0.5F * height,
                        width, height}, frame_size_.width, frame_size_.height);
}

BBox AdaptiveDisplayBox::update(const ControllerOutput& output,
                                const TargetClassState& target_class,
                                AdaptiveDisplayDiagnostics& diagnostics) {
  if (output.reacquired && output.accepted && initialized_ &&
      config_.adaptive_display_enabled &&
      valid_box(output.tracking_bbox)) {
    // The recovered tracker box can have a different scale. Reusing the old
    // tracker-to-display ratio would magnify that scale change a second time.
    pending_candidate_.reset();
    pending_confirmations_ = 0;
    correction_applied_ = false;
    set_transform(output.tracking_bbox,
                  padded(output.tracking_bbox, target_class.target_class_id),
                  1.0F);
  }
  diagnostics.class_id = target_class.target_class_id;
  diagnostics.correction_pending = pending_candidate_.has_value();
  diagnostics.correction_applied = correction_applied_;
  diagnostics.confirmation_count = pending_confirmations_;
  diagnostics.refresh_interval_ms = refresh_interval_ms_;
  diagnostics.refresh_paused = refresh_paused_;
  diagnostics.runtime_p95_ms = runtime_p95_ms_;
  if (!config_.adaptive_display_enabled || !initialized_) {
    diagnostics.source = DisplayBoxSource::kControllerFallback;
    return output.display_bbox;
  }

  BBox target = projected(output.tracking_bbox);
  // SEARCHING publishes a motion prediction. Preserve its historical
  // smoothing; TRACKING and SUSPECTED can still contain a visible moving box.
  float center_alpha = output.tracking_phase == TrackingPhase::kSearching
      ? config_.accepted_center_smoothing_alpha
      : config_.display_center_smoothing_alpha;
  if (output.tracking_phase == TrackingPhase::kSearching) {
    target = BBox{center_x(output.control_bbox) - 0.5F * last_trusted_bbox_.width,
                  center_y(output.control_bbox) - 0.5F * last_trusted_bbox_.height,
                  last_trusted_bbox_.width, last_trusted_bbox_.height};
    target = clip_bbox(target, frame_size_.width, frame_size_.height);
    diagnostics.source = DisplayBoxSource::kSearchPredicted;
  } else if (output.tracking_phase == TrackingPhase::kSuspected) {
    target = BBox{center_x(output.control_bbox) - 0.5F * last_trusted_bbox_.width,
                  center_y(output.control_bbox) - 0.5F * last_trusted_bbox_.height,
                  last_trusted_bbox_.width, last_trusted_bbox_.height};
    target = clip_bbox(target, frame_size_.width, frame_size_.height);
    diagnostics.source = DisplayBoxSource::kSuspectedFrozen;
  } else {
    if (output.reacquired) {
      reacquisition_blend_remaining_ = config_.display_reacquisition_blend_frames;
    }
    if (reacquisition_blend_remaining_ > 0) {
      diagnostics.source = DisplayBoxSource::kReacquisitionBlend;
      center_alpha = 1.0F /
          static_cast<float>(reacquisition_blend_remaining_);
      --reacquisition_blend_remaining_;
    } else if (first_update_ && initialized_from_detector_) {
      diagnostics.source = DisplayBoxSource::kDetectorInitialized;
    } else {
      diagnostics.source = correction_applied_
          ? DisplayBoxSource::kDetectorConfirmed
          : DisplayBoxSource::kTrackerPropagated;
    }
  }

  if (config_.display_stability_enabled &&
      output.tracking_phase == TrackingPhase::kTracking && output.accepted) {
    if (first_update_ || output.reacquired) {
      stability_targets_.clear();
      stability_targets_.push_back(target);
      stability_targets_.push_back(target);
      stability_pending_jump_.reset();
      stability_motion_hold_frames_ = 0;
      stability_warmup_frames_ = output.reacquired ? 3 : 0;
    } else if (stability_warmup_frames_ > 0) {
      // Immediately after SUSPECTED or reacquisition, keep the original
      // responsive display path while gathering a reliable motion window.
      if (stability_targets_.empty()) stability_targets_.push_back(target);
      stability_targets_.push_back(target);
      if (stability_targets_.size() > 5) stability_targets_.pop_front();
      --stability_warmup_frames_;
    } else {
      target = stabilized_target(target);
    }
  } else {
    stability_targets_.clear();
    stability_pending_jump_.reset();
    stability_motion_hold_frames_ = 0;
    stability_warmup_frames_ = 3;
  }
  display_bbox_ = smooth_toward(target, center_alpha);
  if (output.tracking_phase == TrackingPhase::kTracking && output.accepted) {
    last_trusted_bbox_ = display_bbox_;
  }
  correction_applied_ = false;
  first_update_ = false;
  return display_bbox_;
}

void AdaptiveDisplayBox::observe_runtime(float runtime_ms) {
  if (!std::isfinite(runtime_ms) || runtime_ms < 0.0F) return;
  runtime_samples_.push_back(runtime_ms);
  if (runtime_samples_.size() > 60) runtime_samples_.pop_front();
  ++runtime_updates_since_cadence_;
  if (runtime_samples_.size() >= 10) {
    runtime_p95_ms_ = percentile_ms(
        std::vector<float>(runtime_samples_.begin(), runtime_samples_.end()), 0.95F);
  }
  if (runtime_updates_since_cadence_ >= 30) {
    update_cadence();
    runtime_updates_since_cadence_ = 0;
  }
}

void AdaptiveDisplayBox::update_cadence() {
  if (runtime_samples_.size() < 10) return;
  if (runtime_p95_ms_ >= config_.display_runtime_p95_pause_ms) {
    refresh_paused_ = true;
    refresh_interval_ms_ = config_.display_detector_max_interval_ms;
  } else if (runtime_p95_ms_ >= config_.display_runtime_p95_slowdown_ms) {
    refresh_paused_ = false;
    refresh_interval_ms_ = std::min(config_.display_detector_max_interval_ms,
                                    refresh_interval_ms_ * 2);
  } else if (runtime_p95_ms_ < config_.display_runtime_p95_speedup_ms) {
    refresh_paused_ = false;
    refresh_interval_ms_ = std::max(config_.display_detector_min_interval_ms,
                                    refresh_interval_ms_ / 2);
  } else {
    refresh_paused_ = false;
  }
}

}  // namespace nanotrack
