#include "occlusion_evidence.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace nanotrack;

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

void near(const std::optional<float>& actual, float expected) {
  require(actual && std::fabs(*actual - expected) < 0.0001F,
          "unexpected optional numeric value");
}

OcclusionEvidenceInput sample(float width = 100.0F, float height = 100.0F,
                              float margin = 0.08F) {
  // All sizes share a center so scale evidence cannot accidentally use motion.
  return {{200.0F - width / 2, 200.0F - height / 2, width, height},
          {150, 150, 100, 100}, {0.95F, 0.90F, 12, 15, margin},
          true, false, false};
}

void seed(OcclusionEvidenceTracker& tracker, int count = 8) {
  for (int i = 0; i < count; ++i) tracker.observe(sample());
}

void partial_reference_requires_three_stable_samples() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  auto weak = sample();
  weak.reliable_accepted = false;
  require(tracker.observe(weak).reference_source == OcclusionReferenceSource::kNone,
          "zero stable samples have no reference");
  for (int i = 0; i < 8; ++i) {
    // Width grows while height falls less than the noise threshold per frame.
    auto decision = tracker.observe(sample(100 + i, 200 - 2 * i));
    if (i < 2) {
      require(decision.reference_source == OcclusionReferenceSource::kNone &&
                  !decision.reference_width && !decision.width_ratio,
              "fewer than three stable samples must not publish a reference");
    } else if (i == 2) {
      require(decision.reference_source ==
                  OcclusionReferenceSource::kPartialStableMedian,
              "third sample establishes partial reference");
      near(decision.reference_width, 101);
      near(decision.reference_height, 198);
    } else if (i == 7) {
      require(decision.reference_source ==
                  OcclusionReferenceSource::kFullStableMedian,
              "eighth sample establishes full reference");
      near(decision.reference_width, 103.5F);
      near(decision.reference_height, 193);
    }
  }
}

void first_real_shrink_freezes_reference() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  auto decision = tracker.observe(sample(96));
  require(decision.suspicion_started && decision.suspicion_active &&
              decision.width_shrink_frames == 1,
          "0.96 starts suspicion before accepting the small box");
  for (float width : {92.0F, 88.0F, 84.0F}) {
    decision = tracker.observe(sample(width));
    near(decision.reference_width, 100);
    require(!decision.suspicion_started && decision.suspicion_active,
            "later reliable boxes cannot replace the frozen reference");
  }
}

void two_clean_frames_clear_suspicion() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  tracker.observe(sample(96));
  auto decision = tracker.observe(sample(101));
  require(decision.suspicion_active && decision.clean_frames == 1,
          "first clean observation leaves reference frozen");
  near(decision.reference_width, 100);
  decision = tracker.observe(sample(102));
  require(!decision.suspicion_active && decision.suspicion_cleared &&
              decision.clean_frames == 2 && !decision.reference_width,
          "second clean frame clears suspicion and seeds a fresh history");
  decision = tracker.observe(sample(103));
  near(decision.reference_width, 102);
  require(decision.reference_source == OcclusionReferenceSource::kPartialStableMedian,
          "both clean samples must be retained chronologically");
}

void single_noise_band_change_does_not_start_suspicion() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  auto decision = tracker.observe(sample(97.01F));
  require(!decision.suspicion_active && decision.width_shrink_frames == 0,
          "noise-band variation must not freeze stable history");
  for (int i = 0; i < 7; ++i) decision = tracker.observe(sample(97.01F));
  near(decision.reference_width, 97.01F);
}

void reset_clears_reference_suspicion_and_counters() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  tracker.observe(sample(90));
  tracker.observe(sample(84));
  tracker.reset();
  auto decision = tracker.observe(sample(60));
  require(!decision.suspicion_active && !decision.triggered &&
              !decision.reference_width && decision.width_shrink_frames == 0 &&
              decision.height_shrink_frames == 0 && decision.clean_frames == 0,
          "reset removes all old size, trend and suspicion state");
  tracker.observe(sample(60));
  decision = tracker.observe(sample(60));
  near(decision.reference_width, 60);
  near(decision.reference_height, 100);
}

void severe_threshold_is_inclusive_and_axis_specific() {
  for (bool width : {true, false}) {
    for (float dimension : {70.0F, 70.01F}) {
      OcclusionEvidenceTracker tracker(ControllerConfig{});
      seed(tracker, 3);
      auto decision = tracker.observe(width ? sample(dimension) : sample(100, dimension));
      require(decision.triggered == (dimension == 70) &&
                  decision.severe_evidence == (dimension == 70),
              "0.70 severe boundary applies even to reliable accepted boxes");
      require(decision.trigger_reason == (dimension == 70
                  ? (width ? OcclusionTriggerReason::kSevereWidth
                           : OcclusionTriggerReason::kSevereHeight)
                  : OcclusionTriggerReason::kNone), "exact severe reason");
    }
  }
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker, 2);
  require(!tracker.observe(sample(60)).triggered,
          "severe evidence cannot trigger before three stable samples");
}

void gradual_requires_full_reference_and_an_auxiliary_signal() {
  for (bool width : {true, false}) {
    for (int count : {3, 8}) {
      for (int auxiliary = 0; auxiliary < 4; ++auxiliary) {
        OcclusionEvidenceTracker tracker(ControllerConfig{});
        seed(tracker, count);
        tracker.observe(width ? sample(95) : sample(100, 95));
        auto input = width ? sample(85) : sample(100, 85);
        if (auxiliary == 1) input.template_disagreement = true;
        if (auxiliary == 2) input.selected_metrics.peak_margin = 0.06F;
        if (auxiliary == 3) input.observation_bbox.x += 75;
        auto decision = tracker.observe(input);
        const bool expected = count == 8 && auxiliary != 0;
        require(decision.triggered == expected && decision.gradual_evidence == (count == 8),
                "gradual trigger needs full reference, two steps and auxiliary");
        const OcclusionTriggerReason reasons[2][4] = {
            {OcclusionTriggerReason::kNone, OcclusionTriggerReason::kGradualWidthTemplate,
             OcclusionTriggerReason::kGradualWidthPeak, OcclusionTriggerReason::kGradualWidthMotion},
            {OcclusionTriggerReason::kNone, OcclusionTriggerReason::kGradualHeightTemplate,
             OcclusionTriggerReason::kGradualHeightPeak, OcclusionTriggerReason::kGradualHeightMotion}};
        require(decision.trigger_reason == (expected ? reasons[width ? 0 : 1][auxiliary]
                                                     : OcclusionTriggerReason::kNone),
                "each auxiliary has its own axis-specific reason");
        require(decision.template_evidence == (auxiliary == 1) &&
                    decision.peak_margin_evidence == (auxiliary == 2) &&
                    decision.motion_residual_evidence == (auxiliary == 3),
                "raw auxiliary diagnostics are independent");
      }
    }
  }
}

void evidence_boundaries_and_reason_priority() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  tracker.observe(sample(97, 97));
  auto input = sample(85, 85, 0.0601F);
  input.observation_bbox.x += 74.99F;
  auto decision = tracker.observe(input);
  require(!decision.triggered && decision.width_shrink_frames == 2 &&
              decision.height_shrink_frames == 2 && !decision.peak_margin_evidence &&
              !decision.motion_residual_evidence, "auxiliary boundaries must not round up");
  tracker.reset();
  seed(tracker);
  tracker.observe(sample(95, 95));
  input = sample(85, 85, 0.06F);
  input.template_disagreement = true;
  input.observation_bbox.x += 75;
  decision = tracker.observe(input);
  require(decision.trigger_reason == OcclusionTriggerReason::kGradualWidthTemplate,
          "width wins, then template before peak and motion");
  tracker.reset();
  seed(tracker);
  input = sample(70, 60, 0.04F);
  input.template_disagreement = true;
  decision = tracker.observe(input);
  require(decision.trigger_reason == OcclusionTriggerReason::kSevereWidth,
          "severe width wins over severe height and gradual auxiliaries");
}

void normal_motion_and_uniform_scaling_do_not_trigger() {
  for (bool slow : {true, false}) {
    OcclusionEvidenceTracker tracker(ControllerConfig{});
    seed(tracker);
    for (float dimension : (slow ? std::vector<float>{99, 98, 97, 96, 95}
                                 : std::vector<float>{95, 90, 85, 80})) {
      require(!tracker.observe(sample(dimension, dimension)).triggered,
              "target moving away and camera zoom need auxiliary evidence");
    }
  }
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  for (int i = 0; i < 12; ++i) {
    auto input = sample();
    input.observation_bbox.x += 200;
    auto decision = tracker.observe(input);
    require(!decision.triggered && !decision.suspicion_active &&
                decision.motion_residual_evidence, "translation alone cannot freeze or trigger");
  }
}

void detector_hypotheses_never_mutate_evidence() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  auto input = sample(50, 50, 0.01F);
  input.detector_hypothesis = true;
  input.template_disagreement = true;
  auto decision = tracker.observe(input);
  require(!decision.triggered && !decision.suspicion_active &&
              decision.width_shrink_frames == 0 && decision.height_shrink_frames == 0,
          "detector boxes cannot create evidence or trends");
  near(decision.reference_width, 100);
  tracker.observe(sample(96));
  tracker.observe(input);
  decision = tracker.observe(sample(90));
  require(decision.width_shrink_frames == 2,
          "detector must not replace the previous suspicion box");
  tracker.reset();
  for (int i = 0; i < 8; ++i) tracker.observe(input);
  require(!tracker.observe(sample()).reference_width,
          "detector observations cannot seed stable history");
}

void invalid_inputs_throw_without_mutating_state() {
  using Mutator = std::function<void(OcclusionEvidenceInput&)>;
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  const std::vector<Mutator> invalid = {
      [=](auto& i) { i.observation_bbox.x = nan; },
      [=](auto& i) { i.observation_bbox.y = inf; },
      [](auto& i) { i.observation_bbox.x = -1; },
      [](auto& i) { i.observation_bbox.y = -1; },
      [=](auto& i) { i.observation_bbox.width = inf; },
      [=](auto& i) { i.observation_bbox.height = nan; },
      [](auto& i) { i.observation_bbox.width = 0; },
      [](auto& i) { i.observation_bbox.height = -1; },
      [=](auto& i) { i.motion_bbox.x = inf; },
      [=](auto& i) { i.motion_bbox.y = nan; },
      [](auto& i) { i.motion_bbox.x = -1; },
      [](auto& i) { i.motion_bbox.y = -1; },
      [=](auto& i) { i.motion_bbox.width = nan; },
      [=](auto& i) { i.motion_bbox.height = inf; },
      [](auto& i) { i.motion_bbox.width = -1; },
      [](auto& i) { i.motion_bbox.height = 0; },
      [=](auto& i) { i.selected_metrics.raw_score = nan; },
      [=](auto& i) { i.selected_metrics.penalized_peak = inf; },
      [=](auto& i) { i.selected_metrics.psr = nan; },
      [=](auto& i) { i.selected_metrics.apce = inf; },
      [=](auto& i) { i.selected_metrics.peak_margin = nan; }};
  for (bool suspicious : {false, true}) {
    for (const auto& mutate : invalid) {
      OcclusionEvidenceTracker tracker(ControllerConfig{});
      seed(tracker);
      if (suspicious) tracker.observe(sample(96));
      auto input = sample(50, 50);
      mutate(input);
      bool threw = false;
      try { tracker.observe(input); } catch (const std::invalid_argument&) { threw = true; }
      require(threw, "invalid box or metric must throw invalid_argument");
      auto decision = tracker.observe(suspicious ? sample(90) : sample(96));
      near(decision.reference_width, 100);
      require(decision.width_shrink_frames == (suspicious ? 2 : 1) &&
                  decision.suspicion_started == !suspicious && !decision.triggered,
              "failed observe cannot mutate stable history, previous size or counters");
    }
  }
}

void noise_boundary_is_not_a_clean_recovery() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  tracker.observe(sample(96));
  tracker.observe(sample(97));
  auto decision = tracker.observe(sample(97));
  require(decision.suspicion_active && decision.clean_frames == 0,
          "exact 0.97 is still a shrink boundary, not clean recovery");
}

void only_stable_accepted_observations_enter_history() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker, 2);
  auto input = sample(101);
  input.reliable_accepted = false;
  for (int i = 0; i < 8; ++i) tracker.observe(input);
  input.reliable_accepted = true;
  input.template_disagreement = true;
  for (int i = 0; i < 8; ++i) tracker.observe(input);
  input.template_disagreement = false;
  input.observation_bbox.x += 75;
  for (int i = 0; i < 8; ++i) tracker.observe(input);
  auto decision = tracker.observe(sample());
  require(decision.reference_source == OcclusionReferenceSource::kPartialStableMedian,
          "unreliable, disagreeing or motion-inconsistent boxes cannot grow history");
  near(decision.reference_width, 100);
}

void peak_baseline_is_the_selected_margin_median() {
  auto config = ControllerConfig{};
  config.occlusion_reference_window = 4;
  config.occlusion_peak_margin_drop = 0.03125F;
  OcclusionEvidenceTracker tracker(config);
  for (float margin : {0.125F, 0.75F, 0.25F, 0.375F}) {
    tracker.observe(sample(100, 100, margin));
  }
  tracker.observe(sample(95, 100, 0.3125F));
  auto decision = tracker.observe(sample(85, 100, 0.30F));
  require(!decision.triggered && !decision.peak_margin_evidence,
          "mean or latest margin must not replace the median baseline");
  decision = tracker.observe(sample(80, 100, 0.28125F));
  require(decision.trigger_reason == OcclusionTriggerReason::kGradualWidthPeak,
          "absolute drop uses even-window margin median, not last margin or mean");
}

void noise_and_reversal_reset_each_axis_trend() {
  OcclusionEvidenceTracker tracker(ControllerConfig{});
  seed(tracker);
  tracker.observe(sample(95, 95));
  auto decision = tracker.observe(sample(94, 90));
  require(decision.width_shrink_frames == 0 && decision.height_shrink_frames == 2,
          "width noise resets only width trend");
  auto input = sample(85, 91);
  input.template_disagreement = true;
  decision = tracker.observe(input);
  require(!decision.triggered && decision.width_shrink_frames == 1 &&
              decision.height_shrink_frames == 0,
          "reversal breaks consecutive height trend and width needs a second step");
  input = sample(80, 91);
  input.template_disagreement = true;
  decision = tracker.observe(input);
  require(decision.trigger_reason == OcclusionTriggerReason::kGradualWidthTemplate,
          "a fresh two-step same-axis run can trigger");
}

}  // namespace

int main() {
  const std::vector<std::pair<const char*, std::function<void()>>> tests = {
      {"partial_reference_requires_three_stable_samples", partial_reference_requires_three_stable_samples},
      {"first_real_shrink_freezes_reference", first_real_shrink_freezes_reference},
      {"two_clean_frames_clear_suspicion", two_clean_frames_clear_suspicion},
      {"single_noise_band_change_does_not_start_suspicion", single_noise_band_change_does_not_start_suspicion},
      {"reset_clears_reference_suspicion_and_counters", reset_clears_reference_suspicion_and_counters},
      {"severe_threshold_is_inclusive_and_axis_specific", severe_threshold_is_inclusive_and_axis_specific},
      {"gradual_requires_full_reference_and_an_auxiliary_signal", gradual_requires_full_reference_and_an_auxiliary_signal},
      {"evidence_boundaries_and_reason_priority", evidence_boundaries_and_reason_priority},
      {"normal_motion_and_uniform_scaling_do_not_trigger", normal_motion_and_uniform_scaling_do_not_trigger},
      {"detector_hypotheses_never_mutate_evidence", detector_hypotheses_never_mutate_evidence},
      {"invalid_inputs_throw_without_mutating_state", invalid_inputs_throw_without_mutating_state},
      {"noise_boundary_is_not_a_clean_recovery", noise_boundary_is_not_a_clean_recovery},
      {"only_stable_accepted_observations_enter_history", only_stable_accepted_observations_enter_history},
      {"peak_baseline_is_the_selected_margin_median", peak_baseline_is_the_selected_margin_median},
      {"noise_and_reversal_reset_each_axis_trend", noise_and_reversal_reset_each_axis_trend}};
  int failures = 0;
  for (const auto& test : tests) {
    try { test.second(); std::cout << "PASS: " << test.first << '\n'; }
    catch (const std::exception& error) {
      std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n';
      ++failures;
    }
  }
  return failures == 0 ? 0 : 1;
}
