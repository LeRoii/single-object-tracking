#include "adaptive_display_box.hpp"

#include <algorithm>
#include <cmath>
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

void test_detector_envelope_and_initialization() {
  using namespace nanotrack;
  const BBox roi{80, 80, 20, 20};
  AdaptiveDisplayBox car(config());
  car.initialize(roi, established(2, BBox{70, 75, 40, 30}), FrameSize{200, 200});
  expect_true(close(car.current().x, 70.0F) && close(car.current().y, 75.0F) &&
                  close(car.current().width, 40.0F) && close(car.current().height, 30.0F),
              "car initialization uses the detector full-object box without added margins");

  AdaptiveDisplayBox person(config());
  person.initialize(roi, established(0, BBox{2, 4, 20, 80}), FrameSize{200, 200});
  expect_true(close(person.current().x, 2.0F) && close(person.current().y, 4.0F) &&
                  close(person.current().width, 20.0F) && close(person.current().height, 80.0F),
              "person initialization uses the detector box without added margins");

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

void test_confirmed_detector_box_has_no_extra_margin() {
  using namespace nanotrack;
  auto value = config();
  value.display_detector_blend_alpha = 1.0F;
  value.display_max_size_step_ratio = 1.0F;
  value.display_center_smoothing_alpha = 1.0F;
  AdaptiveDisplayBox display(value);
  TargetClassState target = established(2, BBox{80, 80, 40, 30});
  target.selected_candidate.reset();
  const BBox tracked{90, 85, 20, 20};
  display.initialize(tracked, target, FrameSize{200, 200});
  ControllerOutput output = tracking_output(tracked);
  DetectorBatch batch;
  batch.purpose = DetectorRequestPurpose::kDisplayRefresh;
  batch.candidates = {DetectorCandidate{BBox{80, 80, 40, 30}, 0.9F, 2}};
  AdaptiveDisplayDiagnostics diagnostics;
  batch.source_frame_id = 1;
  display.ingest(batch, output, target);
  display.update(output, target, diagnostics);
  batch.source_frame_id = 2;
  display.ingest(batch, output, target);
  const BBox confirmed = display.update(output, target, diagnostics);
  expect_true(diagnostics.correction_applied && close(confirmed.x, 80.0F) &&
                  close(confirmed.y, 80.0F) && close(confirmed.width, 40.0F) &&
                  close(confirmed.height, 30.0F),
              "confirmed detector correction targets the unpadded detector box");
}

void test_car_detector_changes_size_without_shifting_center() {
  using namespace nanotrack;
  auto value = config();
  value.display_detector_blend_alpha = 1.0F;
  value.display_max_size_step_ratio = 1.0F;
  value.display_center_smoothing_alpha = 1.0F;
  AdaptiveDisplayBox display(value);
  const BBox tracked{90, 85, 20, 20};
  TargetClassState target = established(2, BBox{75, 80, 40, 30});
  display.initialize(tracked, target, FrameSize{200, 200});
  expect_true(close(center_x(display.current()), center_x(tracked)) &&
                  close(display.current().width, 40.0F),
              "car detector initialization supplies size while tracker supplies center");
  target.selected_candidate.reset();
  ControllerOutput output = tracking_output(tracked);
  DetectorBatch batch;
  batch.purpose = DetectorRequestPurpose::kDisplayRefresh;
  batch.candidates = {DetectorCandidate{BBox{75, 80, 40, 30}, 0.9F, 2}};
  AdaptiveDisplayDiagnostics diagnostics;
  batch.source_frame_id = 1;
  display.ingest(batch, output, target);
  display.update(output, target, diagnostics);
  batch.source_frame_id = 2;
  display.ingest(batch, output, target);
  const BBox confirmed = display.update(output, target, diagnostics);
  expect_true(diagnostics.correction_applied &&
                  close(center_x(confirmed), center_x(tracked)) &&
                  close(confirmed.width, 40.0F),
              "confirmed detector refresh cannot offset the car display center");
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

void test_display_center_is_independent_of_tracking_smoothing() {
  using namespace nanotrack;
  auto value = config();
  value.accepted_center_smoothing_alpha = 0.35F;
  value.display_center_smoothing_alpha = 0.80F;
  const BBox original{80, 80, 20, 20};
  const BBox moved{100, 80, 20, 20};
  AdaptiveDisplayBox display(value);
  display.initialize(original, TargetClassState{}, FrameSize{200, 200});
  AdaptiveDisplayDiagnostics diagnostics;
  display.update(tracking_output(original), TargetClassState{}, diagnostics);
  const BBox result = display.update(tracking_output(moved), TargetClassState{}, diagnostics);
  expect_true(close(center_x(result), 106.0F) &&
                  close(result.width, 20.0F) &&
                  close(display.current().x, 96.0F),
              "display follows 80 percent of a fast tracker move without resizing");
  expect_true(close(value.accepted_center_smoothing_alpha, 0.35F),
              "display-only tuning does not change tracker center smoothing");

  ControllerOutput suspected = tracking_output(moved);
  suspected.tracking_phase = TrackingPhase::kSuspected;
  suspected.accepted = false;
  suspected.control_bbox = BBox{120, 80, 20, 20};
  const BBox suspected_box = display.update(suspected, TargetClassState{}, diagnostics);
  expect_true(close(center_x(suspected_box), 125.2F),
              "visible suspected output follows the faster display center setting");

  AdaptiveDisplayBox search_display(value);
  search_display.initialize(original, TargetClassState{}, FrameSize{200, 200});
  search_display.update(tracking_output(original), TargetClassState{}, diagnostics);
  ControllerOutput searching = suspected;
  searching.tracking_phase = TrackingPhase::kSearching;
  const BBox predicted = search_display.update(searching, TargetClassState{}, diagnostics);
  expect_true(close(center_x(predicted), 104.0F),
              "search prediction retains the original 35 percent center smoothing");
}

void test_display_stability_rejects_static_jitter_and_single_spike() {
  using namespace nanotrack;
  auto value = config();
  value.display_center_smoothing_alpha = 1.0F;
  value.display_stability_enabled = true;
  const BBox initial{400, 300, 100, 100};
  AdaptiveDisplayBox stable(value);
  AdaptiveDisplayDiagnostics diagnostics;
  stable.initialize(initial, TargetClassState{}, FrameSize{1000, 800});
  stable.update(tracking_output(initial), TargetClassState{}, diagnostics);

  const BBox spike{470, 360, 70, 130};
  const BBox after_spike = stable.update(tracking_output(spike),
                                         TargetClassState{}, diagnostics);
  expect_true(close(center_x(after_spike), center_x(initial)) &&
                  close(after_spike.width, initial.width),
              "one-frame tracker spike cannot displace the display box");

  float min_cx = center_x(initial);
  float max_cx = min_cx;
  float min_width = initial.width;
  float max_width = min_width;
  float max_step = 0.0F;
  float previous_cx = min_cx;
  for (int frame = 0; frame < 60; ++frame) {
    const float sign = frame % 2 == 0 ? 1.0F : -1.0F;
    const BBox noisy{400 + sign * 6, 300 - sign * 6,
                     100 + sign * 8, 100 - sign * 8};
    const BBox shown = stable.update(tracking_output(noisy),
                                     TargetClassState{}, diagnostics);
    const float cx = center_x(shown);
    min_cx = std::min(min_cx, cx);
    max_cx = std::max(max_cx, cx);
    min_width = std::min(min_width, shown.width);
    max_width = std::max(max_width, shown.width);
    max_step = std::max(max_step, std::fabs(cx - previous_cx));
    previous_cx = cx;
  }
  expect_true(max_cx - min_cx < 5.0F && max_step < 4.0F &&
                  max_width - min_width < 4.0F,
              "stationary alternating center and size noise stays visually small: center_range=" +
                  std::to_string(max_cx - min_cx) + " max_step=" +
                  std::to_string(max_step) + " width_range=" +
                  std::to_string(max_width - min_width));

  value.display_stability_enabled = false;
  AdaptiveDisplayBox baseline(value);
  baseline.initialize(initial, TargetClassState{}, FrameSize{1000, 800});
  baseline.update(tracking_output(initial), TargetClassState{}, diagnostics);
  const BBox baseline_spike = baseline.update(tracking_output(spike),
                                              TargetClassState{}, diagnostics);
  expect_true(center_x(baseline_spike) - center_x(initial) > 50.0F,
              "disabled switch preserves the previous immediate center output");
}

void test_display_stability_follows_persistent_motion_and_scale() {
  using namespace nanotrack;
  auto value = config();
  value.display_center_smoothing_alpha = 1.0F;
  value.display_stability_enabled = true;
  const BBox initial{400, 300, 100, 100};
  AdaptiveDisplayBox stable(value);
  AdaptiveDisplayDiagnostics diagnostics;
  stable.initialize(initial, TargetClassState{}, FrameSize{1000, 800});
  stable.update(tracking_output(initial), TargetClassState{}, diagnostics);
  BBox shown = initial;
  for (int frame = 1; frame <= 20; ++frame) {
    const BBox moving{400.0F + 4.0F * frame, 300, 100, 100};
    shown = stable.update(tracking_output(moving), TargetClassState{}, diagnostics);
  }
  expect_true(center_x(shown) > 510.0F && center_x(shown) <= 530.0F,
              "persistent four-pixel-per-frame motion is followed without large lag");
  for (int frame = 0; frame < 30; ++frame) {
    const BBox grown{480, 300, 120, 110};
    shown = stable.update(tracking_output(grown), TargetClassState{}, diagnostics);
  }
  expect_true(shown.width > 115.0F && shown.height > 105.0F &&
                  center_x(shown) > 535.0F,
              "persistent target scale and center changes eventually reach the display");

  ControllerOutput searching = tracking_output(BBox{480, 300, 120, 110});
  searching.tracking_phase = TrackingPhase::kSearching;
  searching.accepted = false;
  searching.control_bbox = BBox{500, 300, 120, 110};
  shown = stable.update(searching, TargetClassState{}, diagnostics);
  expect_true(diagnostics.source == DisplayBoxSource::kSearchPredicted &&
                  center_x(shown) > 540.0F && center_x(shown) < 560.0F,
              "search prediction still uses its existing display behavior");
}

void test_small_moving_target_does_not_lag() {
  using namespace nanotrack;
  auto value = config();
  value.display_center_smoothing_alpha = 1.0F;
  value.display_stability_enabled = true;
  const BBox initial{100, 100, 40, 40};
  AdaptiveDisplayBox display(value);
  AdaptiveDisplayDiagnostics diagnostics;
  display.initialize(initial, TargetClassState{}, FrameSize{500, 500});
  display.update(tracking_output(initial), TargetClassState{}, diagnostics);
  for (int frame = 1; frame <= 12; ++frame) {
    const BBox moving{100.0F + 4.0F * frame,
                      100.0F - 3.0F * frame, 40, 40};
    const BBox shown = display.update(tracking_output(moving),
                                       TargetClassState{}, diagnostics);
    expect_true(std::fabs(center_x(shown) - center_x(moving)) < 1.5F &&
                    std::fabs(shown.y - moving.y) < 1.1F,
                "small target uses only a two-frame center blend");
  }
  float min_cx = 1000.0F;
  float max_cx = -1000.0F;
  for (int frame = 0; frame < 20; ++frame) {
    const BBox noisy{148.0F + (frame % 2 ? 6.0F : -6.0F), 64, 40, 40};
    const BBox shown = display.update(tracking_output(noisy),
                                      TargetClassState{}, diagnostics);
    if (frame >= 2) {
      min_cx = std::min(min_cx, center_x(shown));
      max_cx = std::max(max_cx, center_x(shown));
    }
  }
  expect_true(max_cx - min_cx < 4.0F,
              "small stationary target still reduces alternating center jitter");
}

void test_persistent_large_jump_is_eventually_followed() {
  using namespace nanotrack;
  auto value = config();
  value.display_center_smoothing_alpha = 1.0F;
  value.display_stability_enabled = true;
  const BBox initial{100, 100, 100, 100};
  const BBox jumped{280, 100, 100, 100};
  AdaptiveDisplayBox display(value);
  AdaptiveDisplayDiagnostics diagnostics;
  display.initialize(initial, TargetClassState{}, FrameSize{500, 500});
  display.update(tracking_output(initial), TargetClassState{}, diagnostics);
  const BBox first = display.update(tracking_output(jumped),
                                     TargetClassState{}, diagnostics);
  expect_true(close(center_x(first), center_x(initial)),
              "one large measurement is held until another frame confirms it");
  display.update(tracking_output(jumped), TargetClassState{}, diagnostics);
  const BBox persistent = display.update(tracking_output(jumped),
                                          TargetClassState{}, diagnostics);
  expect_true(center_x(persistent) > center_x(initial) &&
                  center_x(persistent) - center_x(initial) <= 150.01F,
              "confirmed large displacement catches up in bounded steps");
  BBox settled = persistent;
  for (int frame = 0; frame < 8; ++frame)
    settled = display.update(tracking_output(jumped),
                             TargetClassState{}, diagnostics);
  expect_true(std::fabs(center_x(settled) - center_x(jumped)) < 8.0F,
              "persistent displacement reaches its target after bounded catch-up");
}

void test_fast_motion_and_suspected_recovery_have_bounded_display_steps() {
  using namespace nanotrack;
  auto value = config();
  value.display_center_smoothing_alpha = 1.0F;
  value.display_stability_enabled = true;
  value.display_stability_max_center_step_ratio = 0.75F;
  const BBox initial{368, 1000, 128, 128};
  AdaptiveDisplayDiagnostics diagnostics;
  AdaptiveDisplayBox moving(value);
  moving.initialize(initial, TargetClassState{}, FrameSize{3840, 2160});
  moving.update(tracking_output(initial), TargetClassState{}, diagnostics);
  float prior = center_x(moving.current());
  for (int frame = 1; frame <= 20; ++frame) {
    const BBox tracked{368.0F + 70.0F * frame, 1000, 128, 128};
    const BBox shown = moving.update(tracking_output(tracked),
                                     TargetClassState{}, diagnostics);
    expect_true(center_x(shown) - prior <= 96.01F,
                "rapid motion never releases accumulated display lag in one frame");
    if (frame >= 4) {
      expect_true(center_x(tracked) - center_x(shown) < 128.1F,
                  "sustained raw motion bypasses the stale large-jump hold");
    }
    prior = center_x(shown);
  }

  AdaptiveDisplayBox recovering(value);
  recovering.initialize(initial, TargetClassState{}, FrameSize{3840, 2160});
  recovering.update(tracking_output(initial), TargetClassState{}, diagnostics);
  for (int frame = 1; frame <= 12; ++frame) {
    ControllerOutput suspected = tracking_output(
        BBox{368.0F + 70.0F * frame, 1000, 128, 128});
    suspected.tracking_phase = TrackingPhase::kSuspected;
    suspected.accepted = false;
    suspected.observation_present = true;
    suspected.control_bbox = BBox{368.0F + 30.0F * frame, 1000, 128, 128};
    const float previous = center_x(recovering.current());
    const BBox shown = recovering.update(suspected, TargetClassState{}, diagnostics);
    expect_true(center_x(shown) - previous <= 96.01F,
                "provisional display correction remains step-limited");
    if (frame >= 4) {
      expect_true(center_x(suspected.tracking_bbox) - center_x(shown) <= 128.1F,
                  "provisional display remains within one box width of observation");
    }
  }
  const float before_recovery = center_x(recovering.current());
  const BBox tracked{368.0F + 70.0F * 13.0F, 1000, 128, 128};
  const BBox first = recovering.update(tracking_output(tracked),
                                       TargetClassState{}, diagnostics);
  expect_true(center_x(first) - before_recovery <= 96.01F &&
                  center_x(first) > before_recovery,
              "SUSPECTED to TRACKING catches up without a one-frame snap");
  for (int frame = 0; frame < 30; ++frame)
    recovering.update(tracking_output(tracked), TargetClassState{}, diagnostics);
  expect_true(std::fabs(center_x(recovering.current()) - center_x(tracked)) < 8.0F,
              "display reaches the true tracking position after recovery: shown=" +
                  std::to_string(center_x(recovering.current())) +
                  " tracked=" + std::to_string(center_x(tracked)));
}

void test_narrow_tall_handle_filters_size_jitter() {
  using namespace nanotrack;
  auto value = config();
  value.display_center_smoothing_alpha = 1.0F;
  value.display_stability_enabled = true;
  const BBox initial{100, 100, 55, 195};
  AdaptiveDisplayBox display(value);
  AdaptiveDisplayDiagnostics diagnostics;
  display.initialize(initial, TargetClassState{}, FrameSize{500, 500});
  display.update(tracking_output(initial), TargetClassState{}, diagnostics);
  float min_width = initial.width;
  float max_width = initial.width;
  for (int frame = 0; frame < 30; ++frame) {
    const float sign = frame % 2 ? 1.0F : -1.0F;
    const BBox noisy{100 + sign * 2.0F, 100,
                     55 + sign * 2.0F, 195 - sign * 3.0F};
    const BBox shown = display.update(tracking_output(noisy),
                                      TargetClassState{}, diagnostics);
    min_width = std::min(min_width, shown.width);
    max_width = std::max(max_width, shown.width);
  }
  expect_true(max_width - min_width < 2.0F,
              "long narrow textured target uses the size stability path");
}

void test_static_image_anchor_rejects_tracker_drift_and_releases_on_change() {
  using namespace nanotrack;
  auto value = config();
  value.display_center_smoothing_alpha = 1.0F;
  value.display_stability_enabled = true;
  value.display_static_anchor_enabled = true;
  std::vector<std::uint8_t> pixels(400 * 400 * 3, 80);
  for (int y = 100; y < 200; ++y) {
    for (int x = 100; x < 200; ++x) {
      const std::uint8_t gray = x < 150 ? 20 : 220;
      for (int channel = 0; channel < 3; ++channel)
        pixels[(y * 400 + x) * 3 + channel] = gray;
    }
  }
  const ImageView image{pixels.data(), 400, 400, 400 * 3, 3};
  AdaptiveDisplayBox display(value);
  AdaptiveDisplayDiagnostics diagnostics;
  const BBox initial{100, 100, 100, 100};
  display.initialize(initial, TargetClassState{}, FrameSize{400, 400}, &image);
  float held_x = 0.0F;
  for (int frame = 0; frame < 30; ++frame) {
    const BBox drifting{100.0F + 2.0F * frame, 100, 100, 100};
    const BBox shown = display.update(tracking_output(drifting),
                                      TargetClassState{}, diagnostics, &image);
    if (frame == 20) held_x = shown.x;
    if (frame > 20) {
      expect_true(close(shown.x, held_x, 1e-3F) &&
                      diagnostics.source == DisplayBoxSource::kStaticImageAnchor,
                  "unchanged textured image holds display despite tracker drift");
    }
  }
  for (int y = 100; y < 200; ++y) {
    for (int x = 100; x < 200; ++x) {
      for (int channel = 0; channel < 3; ++channel)
        pixels[(y * 400 + x) * 3 + channel] = 100;
    }
  }
  display.update(tracking_output(BBox{160, 100, 100, 100}),
                 TargetClassState{}, diagnostics, &image);
  const BBox released = display.update(
      tracking_output(BBox{162, 100, 100, 100}),
      TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source != DisplayBoxSource::kStaticImageAnchor &&
                  released.x > held_x + 2.0F,
              "persistent visual change releases the static display anchor");

  std::fill(pixels.begin(), pixels.end(), 80);
  AdaptiveDisplayBox flat(value);
  flat.initialize(initial, TargetClassState{}, FrameSize{400, 400}, &image);
  for (int frame = 0; frame < 20; ++frame)
    flat.update(tracking_output(BBox{100.0F + 2.0F * frame, 100, 100, 100}),
                TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source != DisplayBoxSource::kStaticImageAnchor,
              "a textureless patch cannot establish an image anchor");

  for (int y = 100; y < 200; ++y) {
    for (int x = 100; x < 200; ++x) {
      const std::uint8_t gray = x < 150 ? 20 : 220;
      for (int channel = 0; channel < 3; ++channel)
        pixels[(y * 400 + x) * 3 + channel] = gray;
    }
  }
  const TargetClassState known = established(2, initial);
  AdaptiveDisplayBox classified(value);
  classified.initialize(initial, known, FrameSize{400, 400}, &image);
  for (int frame = 0; frame < 20; ++frame)
    classified.update(tracking_output(BBox{100.0F + 2.0F * frame,
                                            100, 100, 100}),
                      known, diagnostics, &image);
  expect_true(diagnostics.source != DisplayBoxSource::kStaticImageAnchor,
              "classified targets retain their existing display path");

  AdaptiveDisplayBox lost(value);
  lost.initialize(initial, TargetClassState{}, FrameSize{400, 400}, &image);
  for (int frame = 0; frame < 20; ++frame)
    lost.update(tracking_output(initial), TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source == DisplayBoxSource::kStaticImageAnchor,
              "unchanged image initially establishes an anchor");
  ControllerOutput searching = tracking_output(initial);
  searching.tracking_phase = TrackingPhase::kSearching;
  searching.accepted = false;
  lost.update(searching, TargetClassState{}, diagnostics, &image);
  for (int frame = 0; frame < 20; ++frame)
    lost.update(tracking_output(initial), TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source != DisplayBoxSource::kStaticImageAnchor,
              "searching retires the previous image anchor");
}

void test_static_image_anchor_releases_on_local_edge_motion() {
  using namespace nanotrack;
  auto value = config();
  value.display_center_smoothing_alpha = 1.0F;
  value.display_stability_enabled = true;
  value.display_static_anchor_enabled = true;
  std::vector<std::uint8_t> pixels(400 * 400 * 3, 80);
  for (int y = 100; y < 200; ++y) {
    for (int x = 100; x < 200; ++x) {
      const std::uint8_t gray = x < 150 ? 20 : 220;
      for (int channel = 0; channel < 3; ++channel)
        pixels[(y * 400 + x) * 3 + channel] = gray;
    }
  }
  const ImageView image{pixels.data(), 400, 400, 400 * 3, 3};
  const BBox initial{100, 100, 100, 100};
  AdaptiveDisplayBox display(value);
  AdaptiveDisplayDiagnostics diagnostics;
  display.initialize(initial, TargetClassState{}, FrameSize{400, 400}, &image);
  for (int frame = 0; frame < 20; ++frame)
    display.update(tracking_output(BBox{100.0F + 2.0F * frame, 100, 100, 100}),
                   TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source == DisplayBoxSource::kStaticImageAnchor,
              "unchanged edge establishes a display anchor");

  auto set_corner = [&](std::uint8_t gray) {
    for (int y = 100; y < 125; ++y) {
      for (int x = 100; x < 125; ++x) {
        for (int channel = 0; channel < 3; ++channel)
          pixels[(y * 400 + x) * 3 + channel] = gray;
      }
    }
  };
  // This patch changes 5% of the ROI mean but one spatial tile by 80 levels.
  set_corner(100);
  display.update(tracking_output(BBox{140, 100, 100, 100}),
                 TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source == DisplayBoxSource::kStaticImageAnchor,
              "one changed frame does not release the anchor");
  set_corner(20);
  display.update(tracking_output(BBox{142, 100, 100, 100}),
                 TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source == DisplayBoxSource::kStaticImageAnchor,
              "a recovered frame resets the release counter");
  set_corner(100);
  display.update(tracking_output(BBox{144, 100, 100, 100}),
                 TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source == DisplayBoxSource::kStaticImageAnchor,
              "local change requires confirmation");
  display.update(tracking_output(BBox{146, 100, 100, 100}),
                 TargetClassState{}, diagnostics, &image);
  expect_true(diagnostics.source != DisplayBoxSource::kStaticImageAnchor,
              "persistent local edge change releases the anchor");
}

}  // namespace

int main() {
  test_detector_envelope_and_initialization();
  test_confirmation_and_search_freeze();
  test_confirmed_detector_box_has_no_extra_margin();
  test_car_detector_changes_size_without_shifting_center();
  test_adaptive_cadence();
  test_reacquisition_rebases_display_scale();
  test_display_center_is_independent_of_tracking_smoothing();
  test_display_stability_rejects_static_jitter_and_single_spike();
  test_display_stability_follows_persistent_motion_and_scale();
  test_small_moving_target_does_not_lag();
  test_persistent_large_jump_is_eventually_followed();
  test_fast_motion_and_suspected_recovery_have_bounded_display_steps();
  test_narrow_tall_handle_filters_size_jitter();
  test_static_image_anchor_rejects_tracker_drift_and_releases_on_change();
  test_static_image_anchor_releases_on_local_edge_motion();
  if (failures != 0) return 1;
  std::cout << "adaptive display box tests passed\n";
  return 0;
}
