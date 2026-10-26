#include "interactive_debug_overlay.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

bool same_bbox(const nanotrack::BBox& actual,
               const nanotrack::BBox& expected) {
  return std::fabs(actual.x - expected.x) < 1e-6F &&
         std::fabs(actual.y - expected.y) < 1e-6F &&
         std::fabs(actual.width - expected.width) < 1e-6F &&
         std::fabs(actual.height - expected.height) < 1e-6F;
}

nanotrack::RuntimeFrameResult complete_result() {
  using namespace nanotrack;
  RuntimeFrameResult result;
  result.plan.frame_id = 12;
  result.plan.motion_predicted_bbox = BBox{1, 2, 30, 40};
  result.plan.predicted_bbox = BBox{50, 60, 30, 40};
  result.plan.hypothesis_source = HypothesisSource::kDetector;
  result.plan.detector_target_mode = DetectorTargetMode::kFullObject;
  result.plan.detector_candidate_id = 7;
  result.plan.detector_candidate_region = BBox{50, 60, 30, 40};
  result.plan.detector_candidate_confidence = 0.87F;
  result.plan.detector_candidate_center_ratio = 0.125F;
  result.plan.detector_probe_index = 2;
  result.observations = {
      TrackerObservation{12, 400, BBox{51, 61, 29, 39},
                         ResponseMetrics{0.91F, 0.90F, 10.0F, 11.0F, 0.2F},
                         TemplateKind::kAnchor, 9},
      TrackerObservation{12, 400, BBox{52, 62, 28, 38},
                         ResponseMetrics{0.86F, 0.84F, 9.0F, 10.0F, 0.15F},
                         TemplateKind::kRecent, 9},
  };
  result.output.frame_id = 12;
  result.output.bbox = BBox{53, 63, 27, 37};
  result.output.accepted = true;
  result.output.control_valid = true;
  result.output.hypothesis_source = HypothesisSource::kDetector;
  result.output.detector_candidate_id = 7;
  result.output.detector_probe_index = 2;
  return result;
}

}  // namespace

int main() {
  using namespace nanotrack;

  const RuntimeFrameResult result = complete_result();
  const InteractiveDebugOverlay overlay =
      make_interactive_debug_overlay(result);
  expect_true(overlay.boxes.size() == 6,
              "complete detector verification produces six ordered debug boxes");
  const DebugBoxKind expected_kinds[] = {
      DebugBoxKind::kMotionPrediction,
      DebugBoxKind::kDetectorCandidate,
      DebugBoxKind::kDetectorProbe,
      DebugBoxKind::kNanoTrackAnchor,
      DebugBoxKind::kNanoTrackRecent,
      DebugBoxKind::kAcceptedControl,
  };
  for (std::size_t index = 0;
       index < overlay.boxes.size() && index < 6; ++index) {
    expect_true(overlay.boxes[index].kind == expected_kinds[index],
                "debug descriptors preserve hypothesis-to-decision draw order");
  }
  expect_true(same_bbox(overlay.boxes[0].bbox,
                        result.plan.motion_predicted_bbox) &&
                  overlay.boxes[0].label == "MOTION PRED",
              "motion descriptor uses the pre-detector prediction");
  expect_true(same_bbox(overlay.boxes[1].bbox,
                        *result.plan.detector_candidate_region) &&
                  overlay.boxes[1].label ==
                      "DET CAND id=7 conf=0.870 d=0.125",
              "detector candidate descriptor exposes identity confidence and distance");
  expect_true(same_bbox(overlay.boxes[2].bbox, result.plan.predicted_bbox) &&
                  overlay.boxes[2].label == "DET PROBE index=2",
              "detector probe descriptor uses the NanoTrack search hypothesis");
  expect_true(overlay.boxes[3].label == "NT ANCHOR score=0.910" &&
                  overlay.boxes[4].label == "NT RECENT score=0.860",
              "NanoTrack descriptors expose raw template scores");
  expect_true(same_bbox(overlay.boxes[5].bbox, result.output.bbox) &&
                  overlay.boxes[5].label == "ACCEPTED CTRL" &&
                  overlay.status_label == "ACCEPTED CTRL" &&
                  overlay.control_valid,
              "accepted control is last and explicit");

  expect_true(same_bbox(overlay.boxes[1].bbox, overlay.boxes[2].bbox) &&
                  overlay.boxes[1].kind != overlay.boxes[2].kind,
              "FULL_OBJECT overlap preserves separate candidate and probe descriptors");

  RuntimeFrameResult rejected = complete_result();
  rejected.output.accepted = false;
  rejected.output.control_valid = false;
  const InteractiveDebugOverlay rejected_overlay =
      make_interactive_debug_overlay(rejected);
  bool contains_accepted = false;
  for (const DebugBoxDescriptor& box : rejected_overlay.boxes) {
    if (box.kind == DebugBoxKind::kAcceptedControl) contains_accepted = true;
  }
  expect_true(!contains_accepted &&
                  rejected_overlay.boxes.size() == 5 &&
                  rejected_overlay.status_label == "REJECTED NO_CTRL" &&
                  !rejected_overlay.control_valid,
              "unconfirmed hypotheses never manufacture an accepted control box");

  if (failures != 0) {
    std::cerr << failures << " overlay test(s) failed\n";
    return 1;
  }
  std::cout << "interactive debug overlay tests passed\n";
  return 0;
}
