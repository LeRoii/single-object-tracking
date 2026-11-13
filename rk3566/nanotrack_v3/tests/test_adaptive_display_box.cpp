#include "adaptive_display_box.hpp"

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

bool close(float actual, float expected, float tolerance = 1e-4F) {
  return std::fabs(actual - expected) <= tolerance;
}

float center_x(const nanotrack::BBox& bbox) {
  return bbox.x + 0.5F * bbox.width;
}

nanotrack::ControllerConfig config() {
  auto value = nanotrack::default_controller_config("visible");
  value.adaptive_display_enabled = true;
  value.display_detector_refresh_enabled = true;
  return value;
}

nanotrack::TargetClassState established(int class_id,
                                        const nanotrack::BBox& full_object) {
  using namespace nanotrack;
  TargetClassState state;
  state.status = TargetClassStatus::kEstablished;
  state.reason = TargetClassReason::kUniqueMatch;
  state.target_class_id = class_id;
  TargetClassCandidateMatch match;
  match.candidate = DetectorCandidate{full_object, 0.9F, class_id};
  match.clipped_bbox = full_object;
  match.eligible = true;
  state.selected_candidate = match;
  return state;
}

nanotrack::ControllerOutput tracking_output(const nanotrack::BBox& bbox) {
  using namespace nanotrack;
  ControllerOutput output;
  output.tracking_phase = TrackingPhase::kTracking;
  output.tracking_bbox = bbox;
  output.control_bbox = bbox;
  output.display_bbox = bbox;
  output.accepted = true;
  return output;
}

void test_class_padding_and_initialization() {
  using namespace nanotrack;
  const BBox roi{80, 80, 20, 20};
  AdaptiveDisplayBox car(config());
  car.initialize(roi, established(2, BBox{70, 75, 40, 30}), FrameSize{200, 200});
  expect_true(close(car.current().x, 67.6F) && close(car.current().y, 72.6F) &&
                  close(car.current().width, 44.8F) && close(car.current().height, 34.8F),
              "car initialization expands to the padded full-object detector box");

  AdaptiveDisplayBox person(config());
  person.initialize(roi, established(0, BBox{2, 4, 20, 80}), FrameSize{200, 200});
  expect_true(close(person.current().x, 0.0F) && close(person.current().width, 24.0F) &&
                  close(person.current().height, 88.8F),
              "person padding is independent by axis and clipped to the frame");

  AdaptiveDisplayBox fallback(config());
  fallback.initialize(roi, TargetClassState{}, FrameSize{200, 200});
  expect_true(close(fallback.current().x, roi.x) &&
                  close(fallback.current().width, roi.width),
              "unknown initialization preserves the mouse ROI");
  expect_true(!fallback.should_request(TrackingPhase::kTracking, 0, true),
              "unknown classes do not schedule display detector refreshes");
}

void test_confirmation_and_search_freeze() {
  using namespace nanotrack;
  auto value = config();
  value.display_detector_blend_alpha = 1.0F;
  AdaptiveDisplayBox display(value);
  TargetClassState target = established(2, BBox{80, 80, 20, 20});
  target.selected_candidate.reset();
  const BBox tracked{80, 80, 20, 20};
  display.initialize(tracked, target, FrameSize{200, 200});
  ControllerOutput output = tracking_output(tracked);
  DetectorBatch batch;
  batch.purpose = DetectorRequestPurpose::kDisplayRefresh;
  batch.candidates = {DetectorCandidate{BBox{70, 75, 40, 30}, 0.9F, 2}};

  display.ingest(batch, output, target);
  AdaptiveDisplayDiagnostics diagnostics;
  const BBox first = display.update(output, target, diagnostics);
  expect_true(diagnostics.correction_pending && !diagnostics.correction_applied &&
                  close(first.width, 20.0F),
              "one detector hit cannot move or resize the display box");

  DetectorBatch stale = batch;
  stale.source_frame_id = 1;
  stale.source_timestamp_ms = -1000;
  output.timestamp_ms = 1000;
  display.ingest(stale, output, target);
  expect_true(display.update(output, target, diagnostics).width <= 20.001F,
              "stale display detections are ignored");
  output.timestamp_ms = 0;
  batch.source_frame_id = 2;

  display.ingest(batch, output, target);
  const BBox second = display.update(output, target, diagnostics);
  expect_true(diagnostics.correction_applied && second.width <= 21.001F &&
                  second.height <= 21.001F,
              "two consistent hits apply correction with a five-percent size step limit");

  const BBox trusted = second;
  output.tracking_phase = TrackingPhase::kSearching;
  output.accepted = false;
  output.control_bbox = BBox{120, 100, 20, 20};
  const BBox search = display.update(output, target, diagnostics);
  expect_true(diagnostics.source == DisplayBoxSource::kSearchPredicted &&
                  close(search.width, trusted.width) &&
                  close(search.height, trusted.height) &&
                  center_x(search) > center_x(trusted),
              "search follows the predicted center while freezing trusted dimensions");
}

void test_adaptive_cadence() {
  using namespace nanotrack;
  AdaptiveDisplayBox display(config());
  TargetClassState target = established(2, BBox{20, 20, 20, 20});
  display.initialize(BBox{20, 20, 20, 20}, target, FrameSize{100, 100});
  expect_true(display.should_request(TrackingPhase::kTracking, 0, true),
              "first normal-tracking display refresh is immediately eligible");
  display.mark_request_submitted(0);
  expect_true(!display.should_request(TrackingPhase::kTracking, 199, true) &&
                  display.should_request(TrackingPhase::kTracking, 200, true) &&
                  !display.should_request(TrackingPhase::kSearching, 200, true),
              "initial five-hertz cadence and SEARCH suppression are enforced");
  for (int i = 0; i < 30; ++i) display.observe_runtime(31.0F);
  expect_true(!display.should_request(TrackingPhase::kTracking, 5000, true),
              "display refresh pauses before the 33.3ms algorithm budget is exceeded");
}

void test_reacquisition_rebases_display_scale() {
  using namespace nanotrack;
  AdaptiveDisplayBox display(config());
  TargetClassState target = established(2, BBox{80, 80, 40, 40});
  const BBox original_tracker{90, 90, 20, 20};
  display.initialize(original_tracker, target, FrameSize{200, 200});
  const float old_width = display.current().width;

  ControllerOutput output = tracking_output(original_tracker);
  output.tracking_phase = TrackingPhase::kSearching;
  output.accepted = false;
  AdaptiveDisplayDiagnostics diagnostics;
  display.update(output, target, diagnostics);

  const BBox recovered_tracker{95, 95, 30, 30};
  output = tracking_output(recovered_tracker);
  output.reacquired = true;
  const BBox first = display.update(output, target, diagnostics);
  expect_true(diagnostics.source == DisplayBoxSource::kReacquisitionBlend &&
                  first.width < old_width,
              "reacquisition must not multiply the old display scale by the new tracker size");
  output.reacquired = false;
  BBox settled = first;
  for (int i = 0; i < 4; ++i) {
    settled = display.update(output, target, diagnostics);
  }
  expect_true(settled.width < 36.0F && settled.height < 37.0F &&
                  close(output.tracking_bbox.width, recovered_tracker.width),
              "reacquired display converges toward the padded new tracker without changing it");
}

}  // namespace

int main() {
  test_class_padding_and_initialization();
  test_confirmation_and_search_freeze();
  test_adaptive_cadence();
  test_reacquisition_rebases_display_scale();
  if (failures != 0) return 1;
  std::cout << "adaptive display box tests passed\n";
  return 0;
}
