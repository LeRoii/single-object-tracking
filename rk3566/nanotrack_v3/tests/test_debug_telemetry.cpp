#include "debug_telemetry.hpp"

#include <iostream>
#include <string>
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
  result.output.selected_metrics =
      nanotrack::ResponseMetrics{0.81F, 9.25F, 31.5F, 0.17F};
  result.output.search_scale = 2.0F;
  result.output.hypothesis_source = nanotrack::HypothesisSource::kMotion;
  result.tracker_ms = 4.5F;
  result.runtime_ms = 6.25F;
  return result;
}

}  // namespace

int main() {
  using namespace nanotrack;

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
