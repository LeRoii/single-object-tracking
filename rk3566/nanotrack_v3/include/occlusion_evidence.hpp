#pragma once

#include "controller_config.hpp"

#include <deque>
#include <optional>

namespace nanotrack {

struct OcclusionEvidenceInput {
  BBox observation_bbox{};
  BBox motion_bbox{};
  ResponseMetrics selected_metrics{};
  bool reliable_accepted = false;
  bool template_disagreement = false;
  bool detector_hypothesis = false;
};

struct OcclusionEvidenceDecision {
  bool suspicion_active = false;
  bool suspicion_started = false;
  bool suspicion_cleared = false;
  bool severe_evidence = false;
  bool gradual_evidence = false;
  bool template_evidence = false;
  bool peak_margin_evidence = false;
  bool motion_residual_evidence = false;
  bool triggered = false;
  OcclusionReferenceSource reference_source = OcclusionReferenceSource::kNone;
  OcclusionTriggerReason trigger_reason = OcclusionTriggerReason::kNone;
  std::optional<float> reference_width, reference_height;
  std::optional<float> width_ratio, height_ratio;
  int width_shrink_frames = 0;
  int height_shrink_frames = 0;
  int clean_frames = 0;
};

// Pure evidence state: never commits tracker, template, motion or control state.
class OcclusionEvidenceTracker {
 public:
  explicit OcclusionEvidenceTracker(const ControllerConfig& config);
  void reset();
  OcclusionEvidenceDecision observe(const OcclusionEvidenceInput& input);

 private:
  struct StableSample {
    float width, height, peak_margin;
  };
  struct Reference {
    StableSample median;
    std::size_t samples;
  };

  std::optional<Reference> reference() const;
  void append_stable(const StableSample& sample);
  void publish_reference(OcclusionEvidenceDecision& decision,
                         const BBox& observation) const;

  ControllerConfig config_;
  std::deque<float> stable_widths_, stable_heights_, stable_peak_margins_;
  std::optional<Reference> frozen_reference_;
  std::optional<BBox> previous_bbox_;
  std::deque<StableSample> clean_samples_;
  int width_shrink_frames_ = 0;
  int height_shrink_frames_ = 0;
};

}  // namespace nanotrack
