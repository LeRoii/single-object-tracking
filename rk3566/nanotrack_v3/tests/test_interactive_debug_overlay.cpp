#include "interactive_debug_overlay.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

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
  result.output.tracking_phase = TrackingPhase::kTracking;
  result.output.observation_valid = true;
  result.output.control_source = ControlSource::kObservation;
  result.output.tracking_bbox = result.output.bbox;
  result.output.display_bbox = result.output.bbox;
  result.output.control_bbox = result.output.bbox;
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
  result.output.detector_candidate_diagnostics[0].class_disposition =
      DetectorClassDisposition::kMatch;
  return result;
}

bool has_text(const nanotrack::InteractiveDebugOverlay& overlay,
              const std::string& text) {
  for (const auto& descriptor : overlay.texts) {
    if (descriptor.label == text) return true;
  }
  return false;
}

void test_target_class_presentation() {
  using namespace nanotrack;
  const std::vector<std::string> labels{"person", "bicycle", "car"};
  RuntimeFrameResult result = complete_result();
  result.target_class.status = TargetClassStatus::kEstablished;
  result.target_class.reason = TargetClassReason::kUniqueMatch;
  result.target_class.target_class_id = 2;
  result.target_class.target_class_allows_recovery = true;
  auto overlay = make_interactive_debug_overlay(result, labels);
  expect_true(has_text(overlay, "TARGET CLS car(2) AUTO") &&
                  overlay.status_label == "TRACKING / OBSERVATION" &&
                  overlay.boxes.back().kind == DebugBoxKind::kAcceptedControl,
              "established shared class does not change accepted-control rendering");
  // The raw candidate is class 0; it must never replace the shared class 2.
  expect_true(has_text(make_interactive_debug_overlay(result, {}),
                       "TARGET CLS cls=2 AUTO"),
              "unavailable verified names use the numeric fallback");
  result.target_class.target_class_id = 7;
  expect_true(has_text(make_interactive_debug_overlay(result, labels),
                       "TARGET CLS cls=7 AUTO"),
              "out-of-range class names safely use the numeric fallback");
  result.target_class.target_class_id = 1;
  expect_true(has_text(make_interactive_debug_overlay(result, {"person", ""}),
                       "TARGET CLS cls=1 AUTO"),
              "empty label entries use the numeric fallback");
  result.target_class.status = TargetClassStatus::kFixed;
  result.target_class.target_class_id = 2;
  result.target_class.target_class_allows_recovery = false;
  expect_true(has_text(make_interactive_debug_overlay(result, labels),
                       "TARGET CLS car(2) FIXED - DET RECOVERY OFF"),
              "fixed override and disabled recovery are both explicit");
  result.target_class = TargetClassState{};
  expect_true(has_text(make_interactive_debug_overlay(result, labels),
                       "TARGET CLS UNKNOWN - DET RECOVERY OFF"),
              "unknown target class visibly disables detector recovery");

  for (const auto disposition : {DetectorClassDisposition::kMismatch,
                                  DetectorClassDisposition::kTargetUnknown,
                                  DetectorClassDisposition::kUnchecked}) {
    auto& diagnostic = result.output.detector_candidate_diagnostics[0];
    diagnostic.class_disposition = disposition;
    // Deliberately leave the other pass flags set: disposition is authoritative.
    diagnostic.rejection = DetectorCandidateRejection::kNone;
    overlay = make_interactive_debug_overlay(result, labels);
    bool raw_candidate_visible = false;
    for (const auto& box : overlay.boxes) {
      raw_candidate_visible |= box.label.find("DET RAW") != std::string::npos;
    }
    expect_true(!raw_candidate_visible,
                "nonmatching detector candidates are omitted from the concise overlay");
  }
  auto& stale = result.output.detector_candidate_diagnostics[0];
  stale.class_disposition = DetectorClassDisposition::kMismatch;
  stale.rejection = DetectorCandidateRejection::kStale;
  overlay = make_interactive_debug_overlay(result, labels);
  bool stale_visible = false;
  for (const auto& box : overlay.boxes) {
    stale_visible |= box.label.find("STALE") != std::string::npos;
  }
  expect_true(!stale_visible,
              "stale detector candidates are omitted from the concise overlay");

  RuntimeFrameResult initialization;
  initialization.plan.motion_predicted_bbox = BBox{1, 1, 5, 5};
  initialization.initialization.emplace();
  auto& state = initialization.initialization->target_class;
  TargetClassCandidateMatch selected;
  selected.original_index = 5;
  selected.candidate = DetectorCandidate{BBox{1, 2, 30, 40}, 0.9F, 2};
  selected.clipped_bbox = BBox{1, 2, 30, 40};
  selected.roi_coverage = 0.8F;
  selected.iou = 0.7F;
  selected.center_ratio = 0.1F;
  selected.score = 0.85F;
  selected.eligible = true;
  TargetClassCandidateMatch other = selected;
  other.original_index = 9;
  other.candidate.class_id = 0;
  other.score = 0.4F;
  other.eligible = false;
  other.rejection = TargetClassCandidateRejection::kNoMatch;
  TargetClassCandidateMatch invalid = other;
  invalid.original_index = 11;
  invalid.clipped_bbox = BBox{};
  invalid.rejection = TargetClassCandidateRejection::kInvalidBox;
  state.candidates = {other, selected, invalid};
  state.selected_candidate = selected;
  state.status = TargetClassStatus::kEstablished;
  state.target_class_id = 2;
  state.target_class_allows_recovery = true;
  initialization.target_class = state;
  overlay = make_interactive_debug_overlay(initialization, labels);
  expect_true(overlay.boxes.size() == 2 &&
                  overlay.boxes[1].kind == DebugBoxKind::kTargetClassInitializationSelected &&
                  same_bbox(overlay.boxes[1].bbox, selected.clipped_bbox),
              "only the selected initialization candidate is drawn");
  expect_true(overlay.boxes[1].label.find("SELECTED car(2)") != std::string::npos &&
                  overlay.boxes[1].label.find("score=0.850 eligible=1 reason=NONE") != std::string::npos,
              "selected initialization label retains its decision evidence");
  expect_true(overlay.texts.size() == 1 &&
                  !overlay.control_valid && overlay.status_label == "REJECTED NO_CTRL",
              "unselected initialization candidates are hidden and selection never grants control");
}

}  // namespace

int main() {
  {
    using namespace nanotrack;
    auto frame = complete_result();
    frame.output.tracking_phase = TrackingPhase::kSuspected;
    frame.output.observation_present = true;
    frame.output.observation_valid = false;
    frame.output.accepted = false;
    frame.output.control_bbox = {54, 64, 27, 37};
    const auto tentative = make_interactive_debug_overlay(frame);
    expect_true(tentative.status_label == "SUSPECTED / RECENT OBSERVATION" &&
                    tentative.control_valid &&
                    tentative.boxes.back().kind == DebugBoxKind::kTentativeControl &&
                    same_bbox(tentative.boxes.back().bbox, frame.output.control_bbox),
                "suspected recent displays independent bounded control");
    frame.output.control_source = ControlSource::kNone;
    frame.output.control_valid = false;
    const auto expired = make_interactive_debug_overlay(frame);
    expect_true(expired.status_label == "SUSPECTED / RECENT OBSERVATION / NO_CONTROL" &&
                    !expired.control_valid,
                "expired tentative observation remains visible without control");
  }

  using namespace nanotrack;

  const RuntimeFrameResult result = complete_result();
  const InteractiveDebugOverlay overlay =
      make_interactive_debug_overlay(result);
  expect_true(overlay.boxes.size() == 7,
              "only the key detector candidate plus active verification boxes are retained");
  const DebugBoxKind expected_kinds[] = {
      DebugBoxKind::kMotionPrediction,
      DebugBoxKind::kDetectorCandidatePass,
      DebugBoxKind::kDetectorCandidate,
      DebugBoxKind::kDetectorProbe,
      DebugBoxKind::kNanoTrackAnchor,
      DebugBoxKind::kNanoTrackRecent,
      DebugBoxKind::kAcceptedControl,
  };
  for (std::size_t index = 0;
       index < overlay.boxes.size() && index < 7; ++index) {
    expect_true(overlay.boxes[index].kind == expected_kinds[index],
                "debug descriptors preserve hypothesis-to-decision draw order");
  }
  expect_true(same_bbox(overlay.boxes[0].bbox,
                        result.plan.motion_predicted_bbox) &&
                  overlay.boxes[0].label == "MOTION PRED",
              "motion descriptor uses the pre-detector prediction");
  expect_true(overlay.boxes[1].label.find(
                  "DET RAW[0] PASS cls=0 conf=0.910 d=0.100 src=10 age=66 queued=1") !=
                  std::string::npos,
              "key detector label exposes class confidence distance provenance and queue state");
  expect_true(overlay.texts.size() == 1,
              "rejected and invalid detector candidates are omitted from text and boxes");
  expect_true(same_bbox(overlay.boxes[3].bbox, result.plan.predicted_bbox) &&
                  overlay.boxes[3].label == "DET PROBE index=2",
              "detector probe descriptor uses the NanoTrack search hypothesis");
  expect_true(overlay.boxes[4].label == "NT ANCHOR score=0.910" &&
                  overlay.boxes[5].label == "NT RECENT score=0.860",
              "NanoTrack descriptors expose raw template scores");
  expect_true(same_bbox(overlay.boxes[6].bbox, result.output.bbox) &&
                  overlay.boxes[6].label == "TRACKING / OBSERVATION" &&
                  overlay.status_label == "TRACKING / OBSERVATION" &&
                  overlay.control_valid,
              "accepted control is last and explicit");

  expect_true(same_bbox(overlay.boxes[2].bbox, overlay.boxes[3].bbox) &&
                  overlay.boxes[2].kind != overlay.boxes[3].kind &&
                  overlay.boxes[2].line_thickness >
                      overlay.boxes[3].line_thickness,
              "FULL_OBJECT overlap preserves separately visible candidate and probe borders");

  for (const DebugBoxDescriptor& box : overlay.boxes) {
    if (box.kind != DebugBoxKind::kAcceptedControl) {
      expect_true(box.label.find("TRACKING / OBSERVATION") == std::string::npos,
                  "raw detector and probe descriptors never claim control acceptance");
    }
  }

  for (const auto rejection : {
           DetectorCandidateRejection::kGenerationInvalidated,
           DetectorCandidateRejection::kTargetClassUnknown,
           DetectorCandidateRejection::kClassMismatch}) {
    RuntimeFrameResult diagnostic = complete_result();
    diagnostic.output.detector_candidate_diagnostics[0].rejection = rejection;
    const auto diagnostic_overlay = make_interactive_debug_overlay(diagnostic);
    bool raw_candidate_visible = false;
    for (const auto& box : diagnostic_overlay.boxes) {
      raw_candidate_visible |= box.label.find("DET RAW") != std::string::npos;
    }
    expect_true(!raw_candidate_visible,
                "rejected detector diagnostics stay out of the concise overlay");
  }

  RuntimeFrameResult rejected = complete_result();
  rejected.output.accepted = false;
  rejected.output.control_valid = false;
  rejected.output.tracking_phase = TrackingPhase::kSearching;
  rejected.output.observation_valid = false;
  rejected.output.control_source = ControlSource::kNone;
  const InteractiveDebugOverlay rejected_overlay =
      make_interactive_debug_overlay(rejected);
  bool contains_accepted = false;
  for (const DebugBoxDescriptor& box : rejected_overlay.boxes) {
    if (box.kind == DebugBoxKind::kAcceptedControl) contains_accepted = true;
  }
  expect_true(!contains_accepted &&
                  rejected_overlay.boxes.size() == 6 &&
                  rejected_overlay.status_label == "SEARCHING / NO_CONTROL" &&
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
  hold.output.tracking_phase = TrackingPhase::kSearching;
  hold.output.observation_valid = false;
  hold.output.control_source = ControlSource::kNone;
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
                  hold_overlay.status_label == "SEARCHING / NO_CONTROL" &&
                  !hold_overlay.control_valid &&
                  !hold_overlay.boxes.empty() &&
                  same_bbox(hold_overlay.boxes.front().bbox,
                            hold.plan.motion_predicted_bbox),
              "hold renders only frozen motion with explicit remaining count and no control");

  test_target_class_presentation();

  hold.output.control_valid = true;
  hold.output.motion_control_authorized = true;
  hold.output.bbox = BBox{10, 20, 30, 40};
  hold.output.control_bbox = hold.output.bbox;
  hold.output.control_source = ControlSource::kMotionSearch;
  hold.output.motion_prediction_confident = true;
  hold.output.occlusion_probe_type = OcclusionProbeType::kSearch;
  hold.observations = result.observations;
  const auto coast = make_interactive_debug_overlay(hold);
  bool motion_control = false;
  bool search_probe = false;
  for (const auto& box : coast.boxes) {
    motion_control |= box.label == "SEARCHING / MOTION_SEARCH PREDICTION" &&
                      box.kind == DebugBoxKind::kSearchControlPrediction;
    search_probe |= box.label.find("COAST SEARCH") != std::string::npos &&
                    box.kind != DebugBoxKind::kAcceptedControl;
  }
  expect_true(coast.control_valid && motion_control && search_probe,
              "motion coast is green control while search is an independent non-control probe");
  for (const auto& box : coast.boxes) {
    if (box.kind == DebugBoxKind::kCoastSearchProbe)
      expect_true(box.label.find("selected_quality=") != std::string::npos &&
                      box.label.find(" quality=") == std::string::npos,
                  "probe label identifies aggregate quality as selected, not template quality");
  }
  hold.output.occlusion_probe_type = OcclusionProbeType::kConfirm;
  hold.observations[0].bbox.width = -1;
  const auto confirm = make_interactive_debug_overlay(hold);
  bool confirm_box = false, invalid_text = false;
  for (const auto& box : confirm.boxes)
    confirm_box |= box.kind == DebugBoxKind::kCoastConfirmProbe;
  for (const auto& item : confirm.texts)
    invalid_text |= item.label.find("COAST CONFIRM") != std::string::npos;
  expect_true(confirm_box && invalid_text,
              "confirm probes use their distinct kind and invalid observations stay text only");

  if (failures != 0) {
    std::cerr << failures << " overlay test(s) failed\n";
    return 1;
  }
  std::cout << "interactive debug overlay tests passed\n";
  return 0;
}
