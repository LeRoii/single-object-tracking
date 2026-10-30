#include "occlusion_evidence.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace nanotrack {
namespace {

void validate_input(const OcclusionEvidenceInput& input) {
  for (const BBox& bbox : {input.observation_bbox, input.motion_bbox}) {
    if (!std::isfinite(bbox.x) || !std::isfinite(bbox.y) ||
        !std::isfinite(bbox.width) || !std::isfinite(bbox.height) ||
        bbox.x < 0 || bbox.y < 0 || bbox.width <= 0 || bbox.height <= 0) {
      throw std::invalid_argument("occlusion evidence requires finite positive-size boxes");
    }
  }
  const auto& metrics = input.selected_metrics;
  for (float value : {metrics.raw_score, metrics.penalized_peak, metrics.psr,
                      metrics.apce, metrics.peak_margin}) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("occlusion evidence requires finite metrics");
    }
  }
}

float median(const std::deque<float>& history) {
  std::vector<float> sorted(history.begin(), history.end());
  std::sort(sorted.begin(), sorted.end());
  return static_cast<float>(0.5 * sorted[(sorted.size() - 1) / 2] +
                            0.5 * sorted[sorted.size() / 2]);
}

// Same convention as LongTermController: distance / sqrt(max(1, motion area)).
// Double intermediates keep finite boxes from overflowing during geometry math.
double normalized_center_distance(const BBox& observation, const BBox& motion) {
  const double dx = (static_cast<double>(observation.x) + 0.5 * observation.width) -
                    (static_cast<double>(motion.x) + 0.5 * motion.width);
  const double dy = (static_cast<double>(observation.y) + 0.5 * observation.height) -
                    (static_cast<double>(motion.y) + 0.5 * motion.height);
  return std::hypot(dx, dy) /
         std::sqrt(std::max(1.0, static_cast<double>(motion.width) * motion.height));
}

int shrink_count(int previous, float dimension, float previous_dimension,
                 float threshold) {
  return dimension / previous_dimension <= threshold
             ? std::min(previous, std::numeric_limits<int>::max() - 1) + 1
             : 0;
}

}  // namespace

OcclusionEvidenceTracker::OcclusionEvidenceTracker(const ControllerConfig& config)
    : config_(config) {
  validate_controller_config(config_);
}

void OcclusionEvidenceTracker::reset() {
  stable_widths_.clear();
  stable_heights_.clear();
  stable_peak_margins_.clear();
  frozen_reference_.reset();
  previous_bbox_.reset();
  clean_samples_.clear();
  width_shrink_frames_ = height_shrink_frames_ = 0;
}

std::optional<OcclusionEvidenceTracker::Reference>
OcclusionEvidenceTracker::reference() const {
  if (frozen_reference_) return frozen_reference_;
  if (stable_widths_.empty()) return std::nullopt;
  return Reference{{median(stable_widths_), median(stable_heights_),
                    median(stable_peak_margins_)}, stable_widths_.size()};
}

void OcclusionEvidenceTracker::append_stable(const StableSample& sample) {
  stable_widths_.push_back(sample.width);
  stable_heights_.push_back(sample.height);
  stable_peak_margins_.push_back(sample.peak_margin);
  if (stable_widths_.size() > static_cast<std::size_t>(config_.occlusion_reference_window)) {
    stable_widths_.pop_front();
    stable_heights_.pop_front();
    stable_peak_margins_.pop_front();
  }
}

void OcclusionEvidenceTracker::publish_reference(
    OcclusionEvidenceDecision& decision, const BBox& observation) const {
  const auto ref = reference();
  if (!ref || ref->samples < static_cast<std::size_t>(config_.occlusion_min_reference_samples)) {
    return;
  }
  decision.reference_source =
      ref->samples == static_cast<std::size_t>(config_.occlusion_reference_window)
          ? OcclusionReferenceSource::kFullStableMedian
          : OcclusionReferenceSource::kPartialStableMedian;
  decision.reference_width = ref->median.width;
  decision.reference_height = ref->median.height;
  decision.width_ratio = observation.width / ref->median.width;
  decision.height_ratio = observation.height / ref->median.height;
}

OcclusionEvidenceDecision OcclusionEvidenceTracker::observe(
    const OcclusionEvidenceInput& input) {
  // Validation precedes every state write, including detector diagnostic calls.
  validate_input(input);
  OcclusionEvidenceDecision decision;
  const BBox& bbox = input.observation_bbox;
  const auto ref = reference();
  decision.suspicion_active = frozen_reference_.has_value();
  decision.width_shrink_frames = width_shrink_frames_;
  decision.height_shrink_frames = height_shrink_frames_;
  decision.clean_frames = static_cast<int>(clean_samples_.size());
  if (input.detector_hypothesis) {
    publish_reference(decision, bbox);
    return decision;
  }

  decision.template_evidence = input.template_disagreement;
  decision.peak_margin_evidence = ref && input.selected_metrics.peak_margin <=
      ref->median.peak_margin - config_.occlusion_peak_margin_drop;
  decision.motion_residual_evidence = normalized_center_distance(bbox, input.motion_bbox) >=
      config_.occlusion_motion_residual_threshold;
  const bool auxiliary = decision.template_evidence || decision.peak_margin_evidence ||
                         decision.motion_residual_evidence;
  const int width_steps = previous_bbox_
      ? shrink_count(width_shrink_frames_, bbox.width, previous_bbox_->width,
                     config_.occlusion_shrink_step_ratio) : 0;
  const int height_steps = previous_bbox_
      ? shrink_count(height_shrink_frames_, bbox.height, previous_bbox_->height,
                     config_.occlusion_shrink_step_ratio) : 0;
  const bool reference_ready = ref && ref->samples >=
      static_cast<std::size_t>(config_.occlusion_min_reference_samples);
  const bool full_reference = reference_ready && ref->samples ==
      static_cast<std::size_t>(config_.occlusion_reference_window);
  const bool severe_width = reference_ready &&
      bbox.width / ref->median.width <= config_.occlusion_trigger_min_dimension_ratio;
  const bool severe_height = reference_ready &&
      bbox.height / ref->median.height <= config_.occlusion_trigger_min_dimension_ratio;
  const bool gradual_width = full_reference &&
      bbox.width / ref->median.width <= config_.occlusion_gradual_dimension_ratio &&
      width_steps >= config_.occlusion_gradual_consecutive_frames;
  const bool gradual_height = full_reference &&
      bbox.height / ref->median.height <= config_.occlusion_gradual_dimension_ratio &&
      height_steps >= config_.occlusion_gradual_consecutive_frames;
  decision.severe_evidence = severe_width || severe_height;
  decision.gradual_evidence = gradual_width || gradual_height;
  if (severe_width) decision.trigger_reason = OcclusionTriggerReason::kSevereWidth;
  else if (severe_height) decision.trigger_reason = OcclusionTriggerReason::kSevereHeight;
  else if ((gradual_width || gradual_height) && auxiliary) {
    if (gradual_width) {
      decision.trigger_reason = decision.template_evidence
          ? OcclusionTriggerReason::kGradualWidthTemplate : decision.peak_margin_evidence
          ? OcclusionTriggerReason::kGradualWidthPeak : OcclusionTriggerReason::kGradualWidthMotion;
    } else {
      decision.trigger_reason = decision.template_evidence
          ? OcclusionTriggerReason::kGradualHeightTemplate : decision.peak_margin_evidence
          ? OcclusionTriggerReason::kGradualHeightPeak : OcclusionTriggerReason::kGradualHeightMotion;
    }
  }
  decision.triggered = decision.trigger_reason != OcclusionTriggerReason::kNone;

  if (!frozen_reference_ && ref && (width_steps > 0 || height_steps > 0 ||
                                   decision.severe_evidence)) {
    frozen_reference_ = ref;
    decision.suspicion_started = true;
  }
  const StableSample current{bbox.width, bbox.height, input.selected_metrics.peak_margin};
  if (frozen_reference_) {
    width_shrink_frames_ = width_steps;
    height_shrink_frames_ = height_steps;
    previous_bbox_ = bbox;
    const bool clean = !decision.suspicion_started && input.reliable_accepted &&
        !auxiliary && !decision.severe_evidence && !decision.gradual_evidence &&
        bbox.width / frozen_reference_->median.width > config_.occlusion_shrink_step_ratio &&
        bbox.height / frozen_reference_->median.height > config_.occlusion_shrink_step_ratio;
    if (clean) clean_samples_.push_back(current);
    else clean_samples_.clear();
    decision.clean_frames = static_cast<int>(clean_samples_.size());
    if (decision.clean_frames >= config_.occlusion_suspicion_clear_frames) {
      stable_widths_.clear();
      stable_heights_.clear();
      stable_peak_margins_.clear();
      for (const auto& clean_sample : clean_samples_) append_stable(clean_sample);
      frozen_reference_.reset();
      clean_samples_.clear();
      width_shrink_frames_ = height_shrink_frames_ = 0;
      decision.suspicion_cleared = true;
    }
  } else if (input.reliable_accepted && !input.template_disagreement &&
             !decision.motion_residual_evidence) {
    append_stable(current);
    previous_bbox_ = bbox;
    width_shrink_frames_ = height_shrink_frames_ = 0;
  }
  decision.suspicion_active = frozen_reference_.has_value();
  decision.width_shrink_frames = width_shrink_frames_;
  decision.height_shrink_frames = height_shrink_frames_;
  publish_reference(decision, bbox);
  return decision;
}

}  // namespace nanotrack
