#include "interactive_debug_overlay.hpp"

#include <cmath>
#include <iostream>
#include <limits>
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
  result.detector.batch_received = true;
  result.detector.batch_frame_id = 10;
  result.detector.batch_age_ms = 66;
  result.output.detector_candidate_diagnostics = {
      DetectorCandidateDiagnostic{DetectorCandidate{BBox{5, 6, 20, 30}, 0.91F, 0},
                                  0.10F, 10, true, true, true, true,
                                  DetectorCandidateRejection::kNone},
      DetectorCandidateDiagnostic{DetectorCandidate{BBox{15, 16, 21, 31}, 0.81F, 0},
                                  0.40F, std::nullopt, true, false, false, true,
                                  DetectorCandidateRejection::kPositionRejected},
      DetectorCandidateDiagnostic{DetectorCandidate{BBox{25, 26, 22, 32}, 0.71F, 0},
                                  0.12F, std::nullopt, true, true, false, true,
                                  DetectorCandidateRejection::kCapacityRejected},
      DetectorCandidateDiagnostic{DetectorCandidate{BBox{35, 36, 23, 33}, 0.61F, 0},
                                  0.13F, std::nullopt, false, false, false, true,
                                  DetectorCandidateRejection::kHoldInvalidated},
      DetectorCandidateDiagnostic{
          DetectorCandidate{BBox{std::numeric_limits<float>::quiet_NaN(), 1, 2, 3},
                            0.51F, 0},
          std::nullopt, std::nullopt, false, false, false, false,
          DetectorCandidateRejection::kInvalid},
  };
  return result;
}

}  // namespace

int main() {
  using namespace nanotrack;

  const RuntimeFrameResult result = complete_result();
  const InteractiveDebugOverlay overlay =
      make_interactive_debug_overlay(result);
  expect_true(overlay.boxes.size() == 10,
              "all four drawable raw candidates plus active verification boxes are retained");
  const DebugBoxKind expected_kinds[] = {
      DebugBoxKind::kMotionPrediction,
      DebugBoxKind::kDetectorCandidatePass,
      DebugBoxKind::kDetectorCandidateRejected,
      DebugBoxKind::kDetectorCandidateRejected,
      DebugBoxKind::kDetectorCandidateRejected,
      DebugBoxKind::kDetectorCandidate,
      DebugBoxKind::kDetectorProbe,
      DebugBoxKind::kNanoTrackAnchor,
      DebugBoxKind::kNanoTrackRecent,
      DebugBoxKind::kAcceptedControl,
  };
  for (std::size_t index = 0;
       index < overlay.boxes.size() && index < 10; ++index) {
    expect_true(overlay.boxes[index].kind == expected_kinds[index],
                "debug descriptors preserve hypothesis-to-decision draw order");
  }
  expect_true(same_bbox(overlay.boxes[0].bbox,
                        result.plan.motion_predicted_bbox) &&
                  overlay.boxes[0].label == "MOTION PRED",
              "motion descriptor uses the pre-detector prediction");
  expect_true(overlay.boxes[1].label.find(
                  "DET RAW[0] PASS cls=0 conf=0.910 d=0.100 src=10 age=66 queued=1") !=
                  std::string::npos &&
                  overlay.boxes[2].label.find("REJECT_POS") != std::string::npos &&
                  overlay.boxes[3].label.find("CAPACITY") != std::string::npos &&
                  overlay.boxes[4].label.find("HOLD_INVALIDATED") != std::string::npos,
              "raw detector labels expose class confidence distance provenance queue and reason");
  expect_true(overlay.texts.size() == 1 &&
                  overlay.texts[0].label.find("DET RAW[4] INVALID") != std::string::npos,
              "invalid geometry is retained as text only");
  expect_true(same_bbox(overlay.boxes[6].bbox, result.plan.predicted_bbox) &&
                  overlay.boxes[6].label == "DET PROBE index=2",
              "detector probe descriptor uses the NanoTrack search hypothesis");
  expect_true(overlay.boxes[7].label == "NT ANCHOR score=0.910" &&
                  overlay.boxes[8].label == "NT RECENT score=0.860",
              "NanoTrack descriptors expose raw template scores");
  expect_true(same_bbox(overlay.boxes[9].bbox, result.output.bbox) &&
                  overlay.boxes[9].label == "ACCEPTED CTRL" &&
                  overlay.status_label == "ACCEPTED CTRL" &&
                  overlay.control_valid,
              "accepted control is last and explicit");

  expect_true(same_bbox(overlay.boxes[5].bbox, overlay.boxes[6].bbox) &&
                  overlay.boxes[5].kind != overlay.boxes[6].kind &&
                  overlay.boxes[5].line_thickness >
                      overlay.boxes[6].line_thickness,
              "FULL_OBJECT overlap preserves separately visible candidate and probe borders");

  for (const DebugBoxDescriptor& box : overlay.boxes) {
    if (box.kind != DebugBoxKind::kAcceptedControl) {
      expect_true(box.label.find("ACCEPTED CTRL") == std::string::npos,
                  "raw detector and probe descriptors never claim control acceptance");
    }
  }

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
                  rejected_overlay.boxes.size() == 9 &&
                  rejected_overlay.status_label == "REJECTED NO_CTRL" &&
                  !rejected_overlay.control_valid,
              "unconfirmed hypotheses never manufacture an accepted control box");

  RuntimeFrameResult hold = complete_result();
  hold.plan.mode = TrackingMode::kOcclusionHold;
  hold.plan.hypothesis_source = HypothesisSource::kMotion;
  hold.plan.hold_remaining_frames = 5;
  hold.plan.run_tracker = false;
  hold.observations.clear();
  hold.output.mode = TrackingMode::kOcclusionHold;
  hold.output.accepted = false;
  hold.output.control_valid = false;
  hold.output.occlusion_hold_remaining_frames = 5;
  hold.tracker_skipped = true;
  hold.tracker_ms = 0.0F;
  const InteractiveDebugOverlay hold_overlay =
      make_interactive_debug_overlay(hold);
  bool has_anchor_or_recent = false;
  for (const DebugBoxDescriptor& box : hold_overlay.boxes) {
    has_anchor_or_recent = has_anchor_or_recent ||
                           box.kind == DebugBoxKind::kNanoTrackAnchor ||
                           box.kind == DebugBoxKind::kNanoTrackRecent;
  }
  expect_true(!has_anchor_or_recent &&
                  hold_overlay.status_label == "OCCLUSION HOLD remaining=5" &&
                  !hold_overlay.control_valid &&
                  !hold_overlay.boxes.empty() &&
                  same_bbox(hold_overlay.boxes.front().bbox,
                            hold.plan.motion_predicted_bbox),
              "hold renders only frozen motion with explicit remaining count and no control");

  if (failures != 0) {
    std::cerr << failures << " overlay test(s) failed\n";
    return 1;
  }
  std::cout << "interactive debug overlay tests passed\n";
  return 0;
}
