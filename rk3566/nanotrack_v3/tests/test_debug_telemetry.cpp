#include "debug_telemetry.hpp"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

bool contains(const std::vector<std::string>& lines,
              const std::string& expected) {
  for (const std::string& line : lines) {
    if (line.find(expected) != std::string::npos) return true;
  }
  return false;
}

std::size_t event_count(const std::vector<std::string>& lines,
                        const std::string& event) {
  std::size_t count = 0;
  const std::string marker = "event=" + event;
  for (const std::string& line : lines) {
    if (line.find(marker) != std::string::npos) ++count;
  }
  return count;
}

nanotrack::RuntimeFrameResult frame_result(std::uint64_t frame_id) {
  nanotrack::RuntimeFrameResult result;
  result.plan.frame_id = frame_id;
  result.plan.timestamp_ms = static_cast<std::int64_t>(frame_id * 33);
  result.plan.search_scale = 2.0F;
  result.output.frame_id = frame_id;
  result.output.timestamp_ms = result.plan.timestamp_ms;
  result.output.mode = nanotrack::TrackingMode::kTracking;
  result.output.quality = nanotrack::QualityLevel::kReliable;
  result.output.bbox = nanotrack::BBox{10.0F, 20.0F, 30.0F, 40.0F};
  result.output.control_valid = true;
  result.output.accepted = true;
  result.output.tracking_phase = nanotrack::TrackingPhase::kTracking;
  result.output.observation_valid = true;
  result.output.control_source = nanotrack::ControlSource::kObservation;
  result.output.tracking_bbox = result.output.bbox;
  result.output.display_bbox = result.output.bbox;
  result.output.control_bbox = result.output.bbox;
  result.output.selected_metrics =
      nanotrack::ResponseMetrics{0.81F, 9.25F, 31.5F, 0.17F};
  result.output.search_scale = 2.0F;
  result.output.hypothesis_source = nanotrack::HypothesisSource::kMotion;
  result.tracker_ms = 4.5F;
  result.runtime_ms = 6.25F;
  return result;
}

void test_recent_safe_telemetry() {
  using namespace nanotrack;
  auto frame = frame_result(17);
  auto& output = frame.output;
  output.mode = TrackingMode::kSuspected;
  output.tracking_phase = TrackingPhase::kSuspected;
  output.observation_valid = false;
  output.observation_present = true;
  output.accepted = false;
  output.template_action = TemplateAction::kRollbackRecent;
  output.template_transition = TemplateTransitionRequest{13, output.template_action, output.tracking_bbox, false, 10};
  output.template_transition_result = TemplateTransitionResult::kApplied;
  output.template_transition_applied = true;
  output.template_transition_reason = "PROBATION_REJECTED";
  frame.template_heads = {true, false, true, false};
  frame.observations.push_back({17, frame.plan.timestamp_ms, output.tracking_bbox,
                               {0.8F, 0.7F, 5.0F, 8.0F, 0.1F}, TemplateKind::kRecent, 1});
  DebugTelemetry telemetry(1000);
  const auto events = telemetry.frame_events(frame, 10.0F, false);
  expect_true(contains(events, "event=frame_observation schema_version=11 frame_id=17") &&
                  contains(events, "tracking_phase=SUSPECTED") &&
                  contains(events, "observation_present=1 trusted_for_state_update=0") &&
                  contains(events, "rect0_source=RECENT_OBSERVATION") &&
                  contains(events, "recent_raw_psr=5.000000") &&
                  contains(events, "template_transaction_id=13 template_slot_id=10") &&
                  contains(events, "template_transition_result=APPLIED"),
              "tentative frames report full v11 evidence outside heartbeat cadence");
}

void test_adaptive_coast_events() {
  using namespace nanotrack;
  DebugTelemetry telemetry(1000);
  auto result = frame_result(1);
  auto& o = result.output;
  o.occlusion_suspicion_started = true;
  auto events = telemetry.frame_events(result, 1, false);
  expect_true(contains(events, "event=occlusion_suspicion_start frame=1"),
              "controller suspicion pulse is visible off heartbeat cadence");
  o.occlusion_suspicion_started = false;
  o.occlusion_suspicion_cleared = true;
  events = telemetry.frame_events(result, 1, false);
  expect_true(contains(events, "event=occlusion_suspicion_clear frame=1"),
              "controller clear pulse is visible");
  o.occlusion_suspicion_cleared = false;
  o.mode = TrackingMode::kOcclusionHold;
  o.accepted = false;
  o.occlusion_hold_triggered = true;
  o.occlusion_coast_age_frames = 1;
  o.occlusion_reference_source = OcclusionReferenceSource::kFullStableMedian;
  o.occlusion_reference_width = 30;
  o.occlusion_reference_height = 40;
  o.occlusion_width_ratio = 0.7F;
  o.occlusion_height_ratio = 0.9F;
  o.occlusion_suspicion_frames = 3;
  o.occlusion_clean_frames = 1;
  o.occlusion_width_shrink_frames = 2;
  o.occlusion_height_shrink_frames = 4;
  o.occlusion_hold_remaining_frames = 12;
  o.occlusion_severe_evidence = true;
  o.occlusion_trigger_reason = OcclusionTriggerReason::kSevereWidth;
  o.motion_control_authorized = true;
  result.plan.motion_predicted_bbox = BBox{10, 20, 30, 40};
  o.bbox = result.plan.motion_predicted_bbox;
  events = telemetry.frame_events(result, 1, false);
  expect_true(contains(events, "event=occlusion_coast_enter frame=1 coast_age=1") &&
                  contains(events, "reference_source=FULL_STABLE_MEDIAN") &&
                  contains(events, "width_ratio=0.700") &&
                  contains(events, "height_ratio=0.900") &&
                  contains(events, "suspicion_frames=3") &&
                  contains(events, "clean_frames=1") &&
                  contains(events, "width_shrink_frames=2") &&
                  contains(events, "height_shrink_frames=4") &&
                  contains(events, "remaining=12") &&
                  contains(events, "trigger_reason=SEVERE_WIDTH") &&
                  contains(events, "motion_control_authorized=1") &&
                  contains(events, "control_bbox=10.00,20.00,30.00,40.00"),
              "coast entry binds age/reference/evidence/control box and authorization");
  o.occlusion_hold_triggered = false;
  for (const auto age : {5, 6, 10, 11}) {
    o.frame_id = age;
    o.occlusion_coast_age_frames = age;
    o.occlusion_probe_type = age == 5 || age == 10 ? OcclusionProbeType::kSearch : OcclusionProbeType::kConfirm;
    o.occlusion_probe_confirmation_required = 2;
    o.occlusion_probe_confirmation_hits = 0;
    o.occlusion_probe_rejection = "MISSING_TEMPLATE";
    events = telemetry.frame_events(result, 1, false);
    expect_true(contains(events, std::string("event=occlusion_probe_") +
                    (age == 5 || age == 10 ? "search" : "confirm") + " frame=" + std::to_string(age)) &&
                    contains(events, "event=occlusion_probe_rejected") &&
                    contains(events, "event=occlusion_coast_tick"),
                "search/confirm/rejected events follow output type rather than a UI schedule");
  }
  o.mode = TrackingMode::kTracking;
  o.reacquired = o.accepted = true;
  o.occlusion_probe_confirmation_hits = 2;
  o.occlusion_probe_rejection = "NONE";
  o.motion_control_authorized = false;
  events = telemetry.frame_events(result, 1, false);
  expect_true(contains(events, "event=occlusion_reacquired") &&
                  contains(events, "event=occlusion_coast_exit") &&
                  !contains(events, "event=occlusion_probe_rejected"),
              "confirmed early recovery reports recovery and exit, never rejection");
  telemetry.reset();
  o.mode = TrackingMode::kOcclusionHold;
  o.reacquired = false;
  o.occlusion_probe_type = OcclusionProbeType::kNone;
  o.occlusion_coast_age_frames = 12;
  telemetry.frame_events(result, 1, false);
  o.mode = TrackingMode::kLocalRecovery;
  o.occlusion_coast_age_frames = 0;
  events = telemetry.frame_events(result, 1, false);
  expect_true(contains(events, "event=occlusion_coast_exit") &&
                  contains(events, "exit_reason=TIMEOUT_OR_ABORT"),
              "coast exit is visible after the last authorized frame");
}

void test_initialization_events() {
  using namespace nanotrack;
  DebugTelemetry telemetry(2);
  TargetClassInitializationResult initialization;
  initialization.initialization_index = 3;
  initialization.frame_id = 42;
  initialization.timestamp_ms = 1386;
  initialization.original_roi = BBox{1, 2, 30, 40};
  initialization.request_id = 91;
  initialization.detector_ms = 15.5F;
  initialization.detector_timing = DetectorTiming{1, 10, 2, 13};
  auto& state = initialization.target_class;
  state.status = TargetClassStatus::kEstablished;
  state.reason = TargetClassReason::kUniqueMatch;
  state.target_class_id = 2;
  state.target_class_allows_recovery = true;
  state.best_class_id = 2;
  state.best_score = 0.9F;
  state.runner_up_class_id = 0;
  state.runner_up_score = 0.6F;
  TargetClassCandidateMatch match;
  match.original_index = 4;
  match.candidate = DetectorCandidate{BBox{0, 1, 50, 60}, 0.8F, 2};
  match.clipped_bbox = BBox{0, 1, 50, 60};
  match.roi_coverage = 0.95F;
  match.center_ratio = 0.125F;
  match.iou = 0.4F;
  match.score = 0.9F;
  match.eligible = true;
  state.selected_candidate = match;
  state.candidates.push_back(match);
  match.original_index = 7;
  match.candidate.class_id = 0;
  match.score = 0.6F;
  match.eligible = false;
  match.rejection = TargetClassCandidateRejection::kNoMatch;
  state.candidates.push_back(match);
  const auto events = telemetry.initialization_events(initialization,
                                                      {"person", "bicycle", "car"});
  expect_true(events.size() == 4 &&
                  events.front() == "DEBUG_EVENT event=target_class_init_start frame=42 timestamp_ms=1386 initialization_index=3 request_id=91 roi=1.00,2.00,30.00,40.00 mode=AUTO requested_class_id=-1 candidate_count=2",
              "initialization start binds the exact ROI mode request and sequence identity");
  expect_true(events[1] == "DEBUG_EVENT event=target_class_candidate frame=42 initialization_index=3 request_id=91 index=4 class_id=2 class_name=car bbox=0.00,1.00,50.00,60.00 clipped_bbox=0.00,1.00,50.00,60.00 confidence=0.800 roi_coverage=0.950 center_ratio=0.125 iou=0.400 score=0.900 eligible=1 selected=1 rejection=NONE" &&
                  contains(events, "index=7 class_id=0 class_name=person") &&
                  contains(events, "score=0.600 eligible=0 selected=0 rejection=NO_MATCH"),
              "each initialization candidate retains canonical metrics rejection and selected identity");
  expect_true(events.back() == "DEBUG_EVENT event=target_class_init_result frame=42 initialization_index=3 request_id=91 status=ESTABLISHED class_id=2 class_name=car reason=UNIQUE_MATCH candidate_count=2 best_class_id=2 best_score=0.900 runner_up_class_id=0 runner_up_score=0.600 target_class_allows_recovery=1 detector_ms=15.500 preprocess_ms=1.000 inference_ms=10.000 postprocess_ms=2.000 total_ms=13.000 error=none",
              "initialization result records state best and runner-up evidence and timing");
  expect_true(!contains(events, "event=detector_") &&
                  !contains(events, "control_valid=") &&
                  !contains(events, "event=reacquired"),
              "initialization candidate evidence cannot masquerade as detector recovery events");
  RuntimeFrameResult frame = frame_result(42);
  frame.initialization = initialization;
  auto frame_events = telemetry.frame_events(frame, 1.0F, false);
  expect_true(!contains(frame_events, "event=target_class_") &&
                  !contains(frame_events, "event=detector_") &&
                  !contains(frame_events, "event=frame_status"),
              "initialization events stay separate and do not consume heartbeat cadence");
  frame_events = telemetry.frame_events(frame_result(43), 1.0F, false);
  expect_true(event_count(frame_events, "frame_status") == 1,
              "initialization event emission does not change frame telemetry cadence");

  initialization.requested_class_id = 9;
  initialization.request_id.reset();
  initialization.target_class = TargetClassState{};
  initialization.target_class.status = TargetClassStatus::kFixed;
  initialization.target_class.reason = TargetClassReason::kFixedOverride;
  initialization.target_class.target_class_id = 9;
  auto fixed = telemetry.initialization_events(initialization, {"person"});
  expect_true(fixed.size() == 2 &&
                  contains(fixed, "request_id=none roi=1.00,2.00,30.00,40.00 mode=FIXED requested_class_id=9 candidate_count=0") &&
                  contains(fixed, "class_id=9 class_name=cls=9 reason=FIXED_OVERRIDE") &&
                  contains(fixed, "best_class_id=none best_score=0.000 runner_up_class_id=none runner_up_score=0.000 target_class_allows_recovery=0"),
              "fixed mode without initialization detection retains explicit missing fields and fallback name");
  initialization.requested_class_id = -1;
  initialization.target_class = TargetClassState{};
  initialization.target_class.reason = TargetClassReason::kDetectorError;
  initialization.detector_error = "inference failed\nretry";
  auto unknown = telemetry.initialization_events(initialization, {});
  expect_true(contains(unknown, "status=UNKNOWN class_id=none class_name=none reason=DETECTOR_ERROR") &&
                  contains(unknown, "error=inference_failed_retry"),
              "unknown detector errors retain no invented class and stay single-line");
  initialization.target_class.target_class_id = 0;
  auto named = telemetry.initialization_events(initialization, {"traffic light"});
  expect_true(contains(named, "class_name=traffic_light"),
              "verified multiword class labels remain one structured field");
}

void test_center_filter_events() {
  using namespace nanotrack;
  DebugTelemetry telemetry(1000);
  RuntimeFrameResult accepted = frame_result(90);
  accepted.plan.motion_predicted_bbox = BBox{8, 18, 30, 40};
  accepted.output.selected_observation_bbox = BBox{12, 22, 30, 40};
  accepted.output.center_innovation_x = 4.0F;
  accepted.output.center_innovation_y = 4.0F;
  accepted.output.center_correction_x = 2.0F;
  accepted.output.center_correction_y = 2.0F;
  accepted.output.center_correction_limited = true;
  accepted.output.accepted_observation_path =
      AcceptedObservationPath::kShortConfirmation;
  auto events = telemetry.frame_events(accepted, 7.0F, false);
  expect_true(event_count(events, "center_filter") == 1 &&
                  contains(events, "event=center_filter frame=90") &&
                  contains(events, "raw_bbox=12.00,22.00,30.00,40.00") &&
                  contains(events, "motion_bbox=8.00,18.00,30.00,40.00") &&
                  contains(events, "innovation=4.000,4.000") &&
                  contains(events, "correction=2.000,2.000") &&
                  contains(events, "limited=1") &&
                  contains(events, "acceptance_path=short_confirmation") &&
                  contains(events, "reacquired=0") &&
                  contains(events, "final_bbox=10.00,20.00,30.00,40.00"),
              "accepted center event retains raw prediction correction and final box");

  RuntimeFrameResult recovery = accepted;
  recovery.output.frame_id = 91;
  recovery.plan.frame_id = 91;
  recovery.output.reacquired = true;
  recovery.output.accepted_observation_path =
      AcceptedObservationPath::kDetectorRecovery;
  events = telemetry.frame_events(recovery, 7.0F, false);
  expect_true(contains(events, "acceptance_path=detector_recovery") &&
                  contains(events, "reacquired=1"),
              "true recovery center event identifies its reset path");

  RuntimeFrameResult bridged = accepted;
  bridged.output.frame_id = 93;
  bridged.plan.frame_id = 93;
  bridged.output.accepted_observation_path =
      AcceptedObservationPath::kTemplateBridge;
  events = telemetry.frame_events(bridged, 7.0F, false);
  expect_true(contains(events, "acceptance_path=template_bridge") &&
                  contains(events, "reacquired=0"),
              "template bridge remains visible as a non-recovery acceptance path");

  RuntimeFrameResult rejected = accepted;
  rejected.output.frame_id = 92;
  rejected.plan.frame_id = 92;
  rejected.output.accepted = false;
  rejected.output.control_valid = false;
  events = telemetry.frame_events(rejected, 7.0F, false);
  expect_true(event_count(events, "center_filter") == 0,
              "unaccepted frames never publish a center correction event");
}

void test_search_semantic_events() {
  using namespace nanotrack;
  DebugTelemetry telemetry(1000);
  auto tracked = frame_result(100);
  telemetry.frame_events(tracked, 7.0F, false);

  auto searching = frame_result(101);
  searching.output.tracking_phase = TrackingPhase::kSearching;
  searching.output.observation_valid = false;
  searching.output.accepted = false;
  searching.output.control_source = ControlSource::kMotionSearch;
  searching.output.motion_control_authorized = true;
  searching.output.motion_prediction_confident = true;
  searching.output.motion_search_age_ms = 33;
  searching.output.search_hint = SearchHint{
      7, 99, 3267, BBox{11, 21, 30, 40}, 3, 101, 1, false,
      SearchHintRejection::kNone};
  auto events = telemetry.frame_events(searching, 7.0F, false);
  expect_true(contains(events, "event=search_enter frame=101") &&
                  contains(events, "control_source=MOTION_SEARCH") &&
                  contains(events, "event=hint_first_hit frame=101 candidate_id=7"),
              "search entry and first shadow hit use explicit search semantics");

  auto expired = searching;
  expired.output.frame_id = 102;
  expired.plan.frame_id = 102;
  expired.output.motion_prediction_confident = false;
  expired.output.motion_search_age_ms = 1033;
  expired.output.search_hint->rejection = SearchHintRejection::kConflict;
  events = telemetry.frame_events(expired, 7.0F, false);
  expect_true(contains(events, "event=search_expired frame=102 search_age_ms=1033") &&
                  contains(events, "event=hint_reject frame=102 candidate_id=7 reason=CONFLICT"),
              "search expiry and rejected hints remain independently visible");

  auto reacquired = frame_result(103);
  reacquired.output.reacquired = true;
  reacquired.output.accepted_observation_path =
      AcceptedObservationPath::kSearchReacquisition;
  reacquired.output.search_hint = SearchHint{
      7, 102, 3366, BBox{12, 22, 30, 40}, 3, 101, 2, true,
      SearchHintRejection::kNone};
  events = telemetry.frame_events(reacquired, 7.0F, false);
  expect_true(contains(events, "event=reacquired frame=103 confirmation_count=2") &&
                  contains(events, "control_source=OBSERVATION"),
              "two-hit search reacquisition is reported as observation control");
}

}  // namespace

int main() {
  test_recent_safe_telemetry();
  using namespace nanotrack;
  test_adaptive_coast_events();
  test_center_filter_events();
  test_search_semantic_events();

  DebugTelemetry telemetry(3);
  RuntimeFrameResult first = frame_result(1);
  std::vector<std::string> events =
      telemetry.frame_events(first, 8.75F, false);
  expect_true(contains(events, "DEBUG_EVENT event=state_change frame=1") &&
                  contains(events, "from=none to=TRACKING") &&
                  contains(events, "control_valid=1"),
              "first frame publishes an explicit state baseline");
  expect_true(!contains(events, "event=frame_status"),
              "first frame is not an implicit heartbeat");

  RuntimeFrameResult second = frame_result(2);
  events = telemetry.frame_events(second, 8.5F, true);
  expect_true(event_count(events, "frame_status") == 1 &&
                  contains(events, "frame=2 mode=TRACKING quality=RELIABLE") &&
                  contains(events, "bbox=10.00,20.00,30.00,40.00") &&
                  contains(events, "score=0.810") &&
                  contains(events, "tracker_ms=4.500 runtime_ms=6.250 pipeline_ms=8.500"),
              "manual step forces one detailed frame heartbeat");
  expect_true(!contains(events, "event=state_change") &&
                  !contains(events, "event=control_change"),
              "unchanged state is not repeated");

  RuntimeFrameResult third = frame_result(3);
  events = telemetry.frame_events(third, 9.0F, false);
  expect_true(event_count(events, "frame_status") == 1,
              "forced heartbeat does not postpone the periodic cadence");

  RuntimeFrameResult lost = frame_result(4);
  lost.output.mode = TrackingMode::kLost;
  lost.output.quality = QualityLevel::kRejected;
  lost.output.control_valid = false;
  lost.output.accepted = false;
  events = telemetry.frame_events(lost, 7.0F, false);
  expect_true(contains(events,
                       "event=state_change frame=4 from=TRACKING to=LOST") &&
                  contains(events, "control_valid=0"),
              "mode changes are emitted immediately with control state");

  RuntimeFrameResult control_change = lost;
  control_change.output.frame_id = 5;
  control_change.plan.frame_id = 5;
  control_change.output.control_valid = true;
  events = telemetry.frame_events(control_change, 7.0F, false);
  expect_true(contains(events,
                       "event=control_change frame=5 mode=LOST from=0 to=1"),
              "control-only transitions are emitted immediately");

  RuntimeFrameResult detector = frame_result(6);
  detector.detector.request_submitted = true;
  detector.detector.request_frame_id = 6;
  detector.detector.batch_received = true;
  detector.detector.batch_frame_id = 5;
  detector.detector.batch_age_ms = 42;
  detector.detector.raw_candidate_count = 7;
  detector.detector.filtered_candidate_count = 2;
  detector.detector.timing = DetectorTiming{1.0F, 5.0F, 2.0F, 8.0F};
  detector.output.detector_status = DetectorBatchStatus::kAccepted;
  detector.output.detector_position_valid_count = 2;
  detector.output.detector_position_rejected_count = 3;
  detector.output.hypothesis_source = HypothesisSource::kDetector;
  detector.output.detector_candidate_id = 19;
  detector.output.detector_same_class_candidates = 2;
  detector.output.detector_same_class_ambiguous = true;
  detector.output.detector_candidate_locked = true;
  detector.output.detector_probe_index = 1;
  detector.output.detector_confirmation_attempts = 3;
  detector.output.detector_confirmation_hits = 2;
  detector.output.template_iou = 0.72F;
  detector.plan.detector_candidate_center_ratio = 0.125F;
  events = telemetry.frame_events(detector, 11.0F, false);
  expect_true(contains(events,
                       "event=detector_request frame=6 request_frame=6") &&
                  contains(events,
                           "event=detector_batch frame=6 source_frame=5 age_ms=42 raw=7 filtered=2 position_valid=2 position_rejected=3 status=ACCEPTED total_ms=8.000") &&
                  contains(events,
                           "event=detector_verify frame=6 candidate_id=19") &&
                  contains(events,
                           "candidate_region=none center_ratio=0.125 probe=1 same_class=2 ambiguous=1 locked=1 confirmation=2/3") &&
                  contains(events, "template_iou=0.720"),
              "detector request, completion, and verification are visible");

  RuntimeFrameResult detector_rejected = frame_result(61);
  detector_rejected.detector.batch_received = true;
  detector_rejected.detector.batch_frame_id = 60;
  detector_rejected.detector.batch_age_ms = 31;
  detector_rejected.detector.raw_candidate_count = 4;
  detector_rejected.detector.filtered_candidate_count = 2;
  detector_rejected.output.detector_status =
      DetectorBatchStatus::kPositionRejected;
  detector_rejected.output.detector_position_valid_count = 0;
  detector_rejected.output.detector_position_rejected_count = 2;
  events = telemetry.frame_events(detector_rejected, 9.0F, false);
  expect_true(contains(events,
                       "event=detector_batch frame=61 source_frame=60 age_ms=31 raw=4 filtered=2 position_valid=0 position_rejected=2 status=POSITION_REJECTED"),
              "position-only rejection is explicit in detector batch telemetry");

  RuntimeFrameResult detector_missing = frame_result(7);
  detector_missing.output.hypothesis_source = HypothesisSource::kDetector;
  events = telemetry.frame_events(detector_missing, 9.0F, false);
  expect_true(contains(events,
                       "event=detector_verify frame=7 candidate_id=none"),
              "missing detector candidate identity is explicit");

  RuntimeFrameResult detector_error = frame_result(8);
  detector_error.detector.error = "inference failed\nretry";
  events = telemetry.frame_events(detector_error, 9.0F, false);
  expect_true(contains(events,
                       "event=detector_error frame=8 message=inference_failed_retry"),
              "detector errors remain single-line and visible off cadence");

  RuntimeFrameResult reacquired = frame_result(9);
  reacquired.output.reacquired = true;
  reacquired.output.hypothesis_source = HypothesisSource::kDetector;
  reacquired.output.detector_candidate_id = 21;
  reacquired.output.detector_confirmation_attempts = 3;
  reacquired.output.detector_confirmation_hits = 2;
  events = telemetry.frame_events(reacquired, 9.0F, false);
  expect_true(contains(events,
                       "event=reacquired frame=9 source=DETECTOR candidate_id=21 confirmation=2/3 control_valid=1"),
              "reacquisition identifies its source and confirmation evidence");

  RuntimeFrameResult templated = frame_result(10);
  templated.output.template_action = TemplateAction::kRollbackRecent;
  events = telemetry.frame_events(templated, 9.0F, false);
  expect_true(contains(events,
                       "event=template_action frame=10 action=ROLLBACK_RECENT"),
              "recent-template lifecycle actions are visible");

  RuntimeFrameResult hold_enter = frame_result(20);
  hold_enter.output.mode = TrackingMode::kOcclusionHold;
  hold_enter.output.control_valid = false;
  hold_enter.output.accepted = false;
  hold_enter.output.occlusion_hold_triggered = true;
  hold_enter.output.occlusion_hold_remaining_frames = 12;
  hold_enter.output.occlusion_reference_width = 80.0F;
  hold_enter.output.occlusion_reference_height = 100.0F;
  hold_enter.output.occlusion_width_ratio = 0.65F;
  hold_enter.output.occlusion_height_ratio = 0.90F;
  hold_enter.output.occlusion_consecutive_unreliable_frames = 2;
  hold_enter.output.occlusion_observation_width = 52.0F;
  hold_enter.output.occlusion_observation_height = 90.0F;
  events = telemetry.frame_events(hold_enter, 9.0F, false);
  expect_true(contains(events,
                       "event=occlusion_hold_enter frame=20 remaining=12 consecutive_unreliable=2 reference=80.00,100.00 observation=52.00,90.00 width_ratio=0.650 height_ratio=0.900") &&
                  contains(events, "tracker_skipped=0 detector_suppressed=0"),
              "hold entry records canonical reference ratios and skip flags");

  RuntimeFrameResult hold_tick = hold_enter;
  hold_tick.output.frame_id = 21;
  hold_tick.plan.frame_id = 21;
  hold_tick.output.occlusion_hold_triggered = false;
  hold_tick.output.occlusion_hold_remaining_frames = 11;
  hold_tick.plan.motion_predicted_bbox = BBox{11, 21, 80, 100};
  hold_tick.plan.request_detector = false;
  hold_tick.tracker_skipped = true;
  hold_tick.tracker_ms = 0.0F;
  events = telemetry.frame_events(hold_tick, 7.0F, false);
  expect_true(contains(events,
                       "event=occlusion_hold_tick frame=21 remaining=11 motion=11.00,21.00,80.00,100.00 tracker_skipped=1 detector_suppressed=1"),
              "hold tick exposes frozen motion and intentional suppression");

  RuntimeFrameResult hold_exit = frame_result(22);
  hold_exit.output.mode = TrackingMode::kLocalRecovery;
  hold_exit.output.control_valid = false;
  hold_exit.output.accepted = false;
  events = telemetry.frame_events(hold_exit, 8.0F, false);
  expect_true(contains(events,
                       "event=occlusion_hold_exit frame=22 next_state=LOCAL_RECOVERY loss_duration_ms=66"),
              "leaving hold reports next state and observed loss duration");

  RuntimeFrameResult diagnostics = frame_result(23);
  diagnostics.detector.batch_received = true;
  diagnostics.detector.batch_frame_id = 19;
  diagnostics.detector.batch_age_ms = 132;
  diagnostics.output.detector_candidate_diagnostics = {
      DetectorCandidateDiagnostic{DetectorCandidate{BBox{1, 2, 30, 40}, 0.88F, 2},
                                  0.125F, 33, true, true, true, true,
                                  DetectorCandidateRejection::kNone},
      DetectorCandidateDiagnostic{DetectorCandidate{BBox{3, 4, 31, 41}, 0.77F, 2},
                                  0.400F, std::nullopt, true, false, false, true,
                                  DetectorCandidateRejection::kPositionRejected},
  };
  events = telemetry.frame_events(diagnostics, 8.0F, false);
  expect_true(event_count(events, "detector_candidate") == 2 &&
                  contains(events,
                           "event=detector_candidate frame=23 index=0 bbox=1.00,2.00,30.00,40.00 class_id=2 confidence=0.880 center_ratio=0.125 source_frame=19 age_ms=132 eligible=1 position_pass=1 queued=1 candidate_id=33 rejection=NONE") &&
                  contains(events, "index=1 bbox=3.00,4.00,31.00,41.00") &&
                  contains(events, "rejection=POSITION_REJECTED"),
              "each canonical detector diagnostic produces one complete event");

  for (const auto& item : {
           std::pair{DetectorCandidateRejection::kGenerationInvalidated, "GENERATION_INVALIDATED"},
           std::pair{DetectorCandidateRejection::kTargetClassUnknown, "TARGET_CLASS_UNKNOWN"},
           std::pair{DetectorCandidateRejection::kClassMismatch, "CLASS_MISMATCH"}}) {
    diagnostics.output.detector_candidate_diagnostics[0].rejection = item.first;
    events = telemetry.frame_events(diagnostics, 8.0F, false);
    expect_true(contains(events, std::string("rejection=") + item.second),
                "extended controller rejection reaches structured telemetry");
  }

  diagnostics.output.detector_candidate_diagnostics[0].class_disposition =
      DetectorClassDisposition::kMismatch;
  diagnostics.output.detector_candidate_diagnostics[0].rejection =
      DetectorCandidateRejection::kStale;
  events = telemetry.frame_events(diagnostics, 8.0F, false);
  expect_true(contains(events,
                       "rejection=STALE class_disposition=MISMATCH final_rejection=STALE"),
              "candidate telemetry preserves independent class disposition and priority rejection");

  test_initialization_events();

  telemetry.reset();
  RuntimeFrameResult after_reset = frame_result(11);
  events = telemetry.frame_events(after_reset, 9.0F, false);
  expect_true(contains(events,
                       "event=state_change frame=11 from=none to=TRACKING"),
              "reinitialization reset publishes a new state baseline");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "debug telemetry tests passed\n";
  return 0;
}
