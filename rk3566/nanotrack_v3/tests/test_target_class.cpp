#include "target_class.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace nanotrack;
int failures = 0;
void expect(bool condition, const std::string& message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void near(float value, float wanted, const std::string& message) {
  expect(std::fabs(value - wanted) <= 1e-6F, message);
}
TargetClassState match(const BBox& roi,
                       const std::vector<DetectorCandidate>& candidates,
                       TargetClassMatchConfig config = {}) {
  return establish_target_class(roi, {200, 200}, candidates, 4, config);
}
void established(const TargetClassState& state, int id, std::size_t index,
                 const std::string& name) {
  expect(state.status == TargetClassStatus::kEstablished &&
             state.reason == TargetClassReason::kUniqueMatch &&
             state.target_class_id == id && state.target_class_allows_recovery,
         name + " establishes exact class");
  expect(state.selected_candidate && state.selected_candidate->original_index == index,
         name + " selects exact original index");
}
void unknown(const TargetClassState& state, TargetClassReason reason,
             const std::string& name) {
  expect(state.status == TargetClassStatus::kUnknown && state.reason == reason &&
             !state.target_class_id && !state.target_class_allows_recovery &&
             !state.selected_candidate,
         name + " stays unknown without recovery or selection");
}

// These fixtures catch coverage/IoU swaps and loss of local-ROI support.
void full_object_and_arbitrary_local_roi() {
  const auto full = match({20, 20, 20, 20}, {{{20, 20, 20, 20}, 0.8F, 2}});
  established(full, 2, 0, "full-object ROI");
  const auto& d = full.candidates.at(0);
  expect(d.eligible && d.rejection == TargetClassCandidateRejection::kNone,
         "full-object eligible");
  near(d.roi_coverage, 1, "full coverage"); near(d.iou, 1, "full IoU");
  near(d.center_ratio, 0, "full center"); near(d.score, 0.97F, "full score");
  const auto local = match({20, 20, 10, 10}, {{{10, 10, 40, 40}, 0.8F, 1}});
  established(local, 1, 0, "arbitrary local ROI");
  near(local.candidates[0].roi_coverage, 1, "local coverage");
  near(local.candidates[0].iou, 0.0625F, "local IoU");
  near(local.candidates[0].center_ratio, 0.125F, "local center ratio");
  near(local.candidates[0].score, 0.834583333F, "local score");
}
void no_match_and_same_class_duplicates() {
  const auto none = match({20, 20, 20, 20}, {{{100, 100, 20, 20}, 1, 0}});
  unknown(none, TargetClassReason::kNoMatch, "no match");
  expect(!none.best_class_id && !none.runner_up_class_id &&
             !none.candidates[0].eligible &&
             none.candidates[0].rejection == TargetClassCandidateRejection::kNoMatch,
         "no match diagnostic");
  const auto duplicates = match({20, 20, 20, 20},
      {{{20, 20, 20, 20}, 0.8F, 1}, {{20, 20, 20, 20}, 0.9F, 1}});
  established(duplicates, 1, 1, "same-class duplicates");
  expect(!duplicates.runner_up_class_id && duplicates.candidates.size() == 2,
         "duplicates are one class with all diagnostics retained");
}
void cross_class_winner_and_ambiguity() {
  auto clear = match({20, 20, 20, 20},
      {{{20, 20, 20, 20}, 1, 2}, {{10, 10, 40, 40}, 0, 1}});
  established(clear, 2, 0, "clear cross-class winner");
  expect(clear.best_class_id == 2 && clear.runner_up_class_id == 1,
         "cross-class ranking");
  near(clear.best_score, 1, "winner score"); near(clear.runner_up_score, 0.775F, "runner score");
  auto tied = match({20, 20, 20, 20},
      {{{20, 20, 20, 20}, 1, 2}, {{20, 20, 20, 20}, 1, 1}});
  unknown(tied, TargetClassReason::kAmbiguous, "cross-class ambiguity");
  expect(tied.best_class_id == 1 && tied.runner_up_class_id == 2,
         "cross-class ties sort by class ID");
  unknown(match({20, 20, 20, 20}, {
      {{20, 20, 20, 20}, 1, 2}, {{20, 20, 20, 20}, 1, 1}}, {0.5F, 0.6F, 0}),
      TargetClassReason::kAmbiguous, "exact cross-class ties remain ambiguous at zero margin");
}
void three_class_epsilon_chain_preserves_strongest_competitor() {
  const auto state = match({20, 20, 20, 20}, {
      {{20, 20, 20, 20}, 0.5F, 0},
      {{20, 20, 20, 20}, 0.500005F, 1},
      {{20, 20, 20, 20}, 0.500010F, 2}}, {0.5F, 0.6F, 0});
  unknown(state, TargetClassReason::kAmbiguous, "three-class epsilon chain");
  expect(state.best_class_id == 2 && state.runner_up_class_id == 1,
         "epsilon chain retains strongest competing class");
  near(state.best_score, 0.925001502F, "epsilon chain best score");
  near(state.runner_up_score, 0.9250007272F, "epsilon chain runner score");
  expect(state.best_score - state.runner_up_score <= 1e-6F,
         "strongest competitor is within ambiguity epsilon");
}
void clipped_boxes_and_invalid_candidates() {
  const auto clipped = match({-10, -10, 20, 20}, {{{-20, -20, 30, 30}, 1, 0}});
  established(clipped, 0, 0, "clipped boxes");
  const auto& d = clipped.candidates[0];
  near(d.clipped_bbox.x, 0, "clip x"); near(d.clipped_bbox.width, 10, "clip width");
  near(d.iou, 1, "clipped IoU"); near(d.score, 1, "clipped score");
  near(d.candidate.bbox.x, -20, "original box retained");
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const auto invalid = match({20, 20, 20, 20}, {
      {{20, 20, 0, 20}, 1, 0}, {{nan, 20, 20, 20}, 1, 0},
      {{250, 20, 20, 20}, 1, 0}, {{20, 20, 20, 20}, 1, -1},
      {{20, 20, 20, 20}, 1, 4}, {{20, 20, 20, 20}, nan, 0}});
  unknown(invalid, TargetClassReason::kNoMatch, "invalid candidates");
  for (std::size_t i = 0; i < invalid.candidates.size(); ++i) {
    const auto& c = invalid.candidates[i];
    expect(c.original_index == i && !c.eligible, "invalid diagnostic index/eligibility");
    expect(c.rejection == (i < 3 ? TargetClassCandidateRejection::kInvalidBox :
        i < 5 ? TargetClassCandidateRejection::kInvalidClass :
                TargetClassCandidateRejection::kInvalidConfidence), "specific rejection");
  }
}
void deterministic_same_class_ties() {
  // Confidence difference smaller than the shared epsilon: then larger area wins.
  auto area = match({20, 20, 20, 20}, {
      {{20, 20, 20, 20}, 0.5F, 0}, {{19.99999F, 20, 20.00002F, 20}, 0.5F, 0}});
  established(area, 0, 1, "same-class area tie-break");
  auto index = match({20, 20, 20, 20}, {
      {{20, 20, 20, 20}, 0.5F, 0}, {{20, 20, 20, 20}, 0.5F, 0}});
  established(index, 0, 0, "same-class input-index tie-break");
  // Scores tie within epsilon while confidence differs by more than epsilon.
  auto confidence = match({20, 20, 20, 20}, {
      {{20, 20, 20, 20}, 0.5F, 0}, {{20, 20, 20, 20}, 0.500002F, 0}});
  established(confidence, 0, 1, "same-class confidence tie-break");
}
void equality_at_all_three_thresholds() {
  // ROI center is outside D; exactly half the ROI is covered.
  auto coverage = match({20, 20, 20, 20}, {{{20, 20, 10, 20}, 1, 0}}, {0.5F, 0.01F, 0.1F});
  established(coverage, 0, 0, "coverage equality");
  near(coverage.candidates[0].roi_coverage, 0.5F, "coverage boundary");
  // D diagonal=50; centers differ by 15 => ratio=0.30. Coverage gate disabled.
  auto center = match({35, 30, 20, 10}, {{{10, 20, 40, 30}, 1, 0}}, {1, 0.3F, 0.1F});
  established(center, 0, 0, "center equality");
  near(center.candidates[0].center_ratio, 0.3F, "center boundary");
  near(center.candidates[0].roi_coverage, 0.75F, "center fixture cannot use coverage gate");
  unknown(match({35, 30, 20, 10}, {{{10, 20, 40, 30}, 1, 0}}, {1, 0.299F, 0.1F}),
          TargetClassReason::kNoMatch, "above center threshold");
  auto margin = match({20, 20, 20, 20}, {
      {{20, 20, 20, 20}, 1, 0}, {{20, 20, 20, 20}, 1.0F / 3.0F, 1}});
  established(margin, 0, 0, "ambiguity margin equality");
  near(margin.best_score - margin.runner_up_score, 0.1F, "margin boundary");
  auto below = match({20, 20, 20, 20}, {
      {{20, 20, 20, 20}, 1, 0}, {{20, 20, 20, 20}, 0.34F, 1}});
  unknown(below, TargetClassReason::kAmbiguous, "below ambiguity margin");
  auto rejected = match({20, 20, 20, 20}, {{{20, 20, 9.9F, 20}, 1, 0}}, {0.5F, 0.01F, 0.1F});
  unknown(rejected, TargetClassReason::kNoMatch, "below coverage threshold");
}
void construction_and_invalid_roi() {
  for (bool available : {false, true}) {
    const auto fixed = make_fixed_target_class(2, available);
    expect(fixed.status == TargetClassStatus::kFixed && fixed.target_class_id == 2 &&
               fixed.reason == TargetClassReason::kFixedOverride &&
               fixed.target_class_allows_recovery == available, "fixed override availability");
  }
  unknown(make_unknown_target_class(TargetClassReason::kDetectorError),
          TargetClassReason::kDetectorError, "unknown constructor");
  for (BBox roi : {BBox{0, 0, 0, 1}, BBox{300, 0, 1, 1},
                   BBox{0, 0, std::numeric_limits<float>::quiet_NaN(), 1}}) {
    bool threw = false;
    try { match(roi, {}); } catch (const std::invalid_argument&) { threw = true; }
    expect(threw, "invalid ROI rejects initialization");
  }
  unknown(match({20, 20, 20, 20}, {}), TargetClassReason::kNoMatch, "empty candidates");
}
}  // namespace
int main() {
  full_object_and_arbitrary_local_roi(); no_match_and_same_class_duplicates();
  cross_class_winner_and_ambiguity(); clipped_boxes_and_invalid_candidates();
  three_class_epsilon_chain_preserves_strongest_competitor();
  deterministic_same_class_ties(); equality_at_all_three_thresholds();
  construction_and_invalid_roi();
  if (failures) return 1;
  std::cout << "target class tests passed\n";
}
