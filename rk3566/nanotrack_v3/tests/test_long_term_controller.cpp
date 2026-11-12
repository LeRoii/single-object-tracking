#include "controller_config.hpp"
#include "long_term_types.hpp"
#include "response_baseline.hpp"
#include "target_class.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#define private public
#include "long_term_controller.hpp"
#undef private
#include "resolved_controller_fixture.hpp"
#include "debug_telemetry.hpp"
#include "interactive_debug_overlay.hpp"
#include "tracking_log.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <tuple>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void expect_near(float actual, float expected, float tolerance,
                 const std::string& message) {
  expect_true(std::fabs(actual - expected) <= tolerance,
              message + " actual=" + std::to_string(actual) +
                  " expected=" + std::to_string(expected));
}

nanotrack::TrackerObservation reliable_observation(
    const nanotrack::SearchPlan& plan,
    nanotrack::TemplateKind kind,
    const nanotrack::BBox& bbox,
    std::uint64_t token = 1) {
  return nanotrack::TrackerObservation{
      plan.frame_id,
      plan.timestamp_ms,
      bbox,
      nanotrack::ResponseMetrics{0.92F, 0.90F, 10.0F, 12.0F, 0.20F},
      kind,
      token};
}

nanotrack::TrackerObservation rejected_observation(
    const nanotrack::SearchPlan& plan,
    nanotrack::TemplateKind kind,
    const nanotrack::BBox& bbox,
    std::uint64_t token = 1) {
  return nanotrack::TrackerObservation{
      plan.frame_id,
      plan.timestamp_ms,
      bbox,
      nanotrack::ResponseMetrics{0.20F, 0.20F, 1.0F, 1.0F, 0.01F},
      kind,
      token};
}

nanotrack::TrackerObservation weak_observation(
    const nanotrack::SearchPlan& plan,
    nanotrack::TemplateKind kind,
    const nanotrack::BBox& bbox,
    std::uint64_t token = 1) {
  return nanotrack::TrackerObservation{
      plan.frame_id,
      plan.timestamp_ms,
      bbox,
      nanotrack::ResponseMetrics{0.60F, 0.58F, 6.0F, 5.0F, 0.05F},
      kind,
      token};
}

std::vector<nanotrack::TrackerObservation> reliable_pair(
    const nanotrack::SearchPlan& plan,
    const nanotrack::BBox& bbox,
    std::uint64_t token) {
  return {
      reliable_observation(plan, nanotrack::TemplateKind::kAnchor, bbox, token),
      reliable_observation(plan, nanotrack::TemplateKind::kRecent, bbox, token),
  };
}

nanotrack::ControllerOutput reject_frame(nanotrack::LongTermController& controller,
                                         std::uint64_t frame_id,
                                         std::int64_t timestamp_ms,
                                         bool detector_available = false) {
  const nanotrack::SearchPlan plan =
      controller.begin_frame(frame_id, timestamp_ms, detector_available);
  return controller.finish_frame(plan, {}, std::nullopt);
}

// Removing probe scheduling, allowing one-template hits, or committing a search
// box must fail these real controller fixtures.
void start_probe_coast(nanotrack::LongTermController& controller) {
  using namespace nanotrack;
  const BBox stable{60, 40, 20, 20};
  controller.initialize(stable, 0, 0, {240, 160}, make_fixed_target_class(0, true));
  for (int id = 1; id <= 8; ++id) {
    const auto plan = controller.begin_frame(id, id * 40, false);
    controller.finish_frame(plan, reliable_pair(plan, stable, id), std::nullopt);
  }
  const auto plan = controller.begin_frame(9, 360, true);
  expect_true(controller.finish_frame(plan,
      reliable_pair(plan, BBox{63, 40, 14, 20}, 9), std::nullopt)
      .occlusion_hold_triggered, "probe fixture enters coast at age one");
}

nanotrack::SearchPlan probe_plan(nanotrack::LongTermController& controller, int age) {
  return controller.begin_frame(8 + age, 320 + age * 40, true);
}

void advance_probe_coast(nanotrack::LongTermController& controller, int first, int last) {
  for (int age = first; age <= last; ++age) {
    const auto plan = probe_plan(controller, age);
    controller.finish_frame(plan, {}, std::nullopt);
  }
}

void test_external_observation_and_control_contract() {
  using namespace nanotrack;
  expect_true(std::string(to_string(TrackingPhase::kTracking)) == "TRACKING" &&
                  std::string(to_string(TrackingPhase::kSearching)) == "SEARCHING" &&
                  std::string(to_string(TrackingPhase::kInvalid)) == "INVALID",
              "tracking phases have stable external names");
  expect_true(std::string(to_string(ControlSource::kNone)) == "NONE" &&
                  std::string(to_string(ControlSource::kObservation)) == "OBSERVATION" &&
                  std::string(to_string(ControlSource::kMotionSearch)) == "MOTION_SEARCH",
              "control sources have stable external names");

  ControllerConfig config;
  config.max_center_distance = 10.0F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  config.accepted_center_smoothing_alpha = 0.5F;
  ResolvedControllerFixture controller(config);
  const BBox initial{100, 70, 100, 80};
  controller.initialize(initial, 0, 0, {400, 300});

  auto plan = controller.begin_frame(1, 40, false);
  expect_true(plan.tracking_phase == TrackingPhase::kTracking &&
                  !plan.observation_valid &&
                  plan.control_source == ControlSource::kNone,
              "pre-inference tracking plan does not claim an observation");
  const BBox observed{120, 70, 100, 80};
  auto output = controller.finish_frame(
      plan, reliable_pair(plan, observed, 1), std::nullopt);
  expect_true(output.accepted && output.observation_valid && output.control_valid &&
                  output.tracking_phase == TrackingPhase::kTracking &&
                  output.control_source == ControlSource::kObservation,
              "accepted tracker evidence publishes observation control");
  expect_true(output.tracking_bbox.x == output.control_bbox.x &&
                  output.tracking_bbox.y == output.control_bbox.y &&
                  output.tracking_bbox.width == output.control_bbox.width &&
                  output.tracking_bbox.height == output.control_bbox.height &&
                  output.tracking_bbox.x != output.display_bbox.x &&
                  output.bbox.x == output.display_bbox.x &&
                  output.bbox.y == output.display_bbox.y,
              "tracking/control box stays real while display smoothing is independent");

  plan = controller.begin_frame(2, 80, false);
  output = controller.finish_frame(plan, {}, std::nullopt);
  expect_true(!output.accepted && !output.observation_valid && !output.control_valid &&
                  output.tracking_phase == TrackingPhase::kSearching &&
                  output.control_source == ControlSource::kNone,
              "legacy rejected observation explicitly reports search without authorization");

  ControllerConfig coast_config;
  coast_config.occlusion_reference_window = 3;
  ResolvedControllerFixture coast(coast_config);
  const BBox stable{40, 30, 20, 20};
  coast.initialize(stable, 0, 0, {160, 120});
  for (std::uint64_t id = 1; id <= 3; ++id) {
    const auto stable_plan = coast.begin_frame(id, id * 40, false);
    coast.finish_frame(stable_plan, reliable_pair(stable_plan, stable, id),
                       std::nullopt);
  }
  const auto trigger_plan = coast.begin_frame(4, 160, false);
  const auto trigger = coast.finish_frame(
      trigger_plan,
      {weak_observation(trigger_plan, TemplateKind::kAnchor,
                        BBox{43, 33, 14, 14})},
      std::nullopt);
  expect_true(trigger.occlusion_hold_triggered && !trigger.observation_valid &&
                  trigger.control_valid &&
                  trigger.tracking_phase == TrackingPhase::kSearching &&
                  trigger.control_source == ControlSource::kMotionSearch,
              "authorized occlusion motion publishes search control without observation");
  const auto coast_plan = coast.begin_frame(5, 200, false);
  expect_true(coast_plan.tracking_phase == TrackingPhase::kSearching &&
                  !coast_plan.observation_valid &&
                  coast_plan.control_source == ControlSource::kMotionSearch,
              "coast search plan exposes its intended motion-control source");
  const auto coast_output = coast.finish_frame(coast_plan, {}, std::nullopt);
  expect_true(coast_output.control_valid && !coast_output.observation_valid &&
                  coast_output.control_source == ControlSource::kMotionSearch,
              "coast frame keeps observation and control validity separate");

  ControllerOutput invalid = coast_output;
  invalid.accepted = false;
  invalid.observation_valid = true;
  bool rejected_invalid_semantics = false;
  try {
    LongTermController::validate_output_semantics(invalid);
  } catch (const std::logic_error&) {
    rejected_invalid_semantics = true;
  }
  expect_true(rejected_invalid_semantics,
              "semantic validator rejects observation validity without acceptance");
}

void test_accepted_scale_deadband_smoothing_and_compatibility() {
  using namespace nanotrack;
  ControllerConfig config;
  config.max_accepted_scale_step_ratio = 0.05F;
  config.accepted_scale_deadband_ratio = 0.02F;
  config.accepted_scale_smoothing_alpha = 0.10F;
  config.max_center_distance = 10.0F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  ResolvedControllerFixture controller(config);
  controller.initialize(BBox{100, 70, 100, 80}, 0, 0, {400, 300});

  auto plan = controller.begin_frame(1, 40, false);
  auto output = controller.finish_frame(
      plan, reliable_pair(plan, BBox{99, 70.8F, 102, 78.4F}, 1), std::nullopt);
  expect_true(output.accepted && output.control_valid,
              "reliable deadband observation remains accepted");
  expect_near(output.bbox.width, 100.0F, 1e-5F,
              "exact two-percent width growth remains inside the deadband");
  expect_near(output.bbox.height, 80.0F, 1e-5F,
              "exact two-percent height shrink remains inside the deadband");
  expect_near(output.bbox.x + 0.5F * output.bbox.width, 150.0F, 1e-5F,
              "deadband preserves the raw horizontal center");
  expect_near(output.bbox.y + 0.5F * output.bbox.height, 110.0F, 1e-5F,
              "deadband preserves the raw vertical center");

  ResolvedControllerFixture rounded_boundary(config);
  rounded_boundary.initialize(BBox{100, 70, 60, 60}, 0, 0, {400, 300});
  plan = rounded_boundary.begin_frame(1, 40, false);
  output = rounded_boundary.finish_frame(
      plan, reliable_pair(plan, BBox{99.4F, 70, 61.2F, 60}, 20),
      std::nullopt);
  expect_near(output.bbox.width, 60.0F, 1e-5F,
              "float-rounded exact two-percent growth remains in the deadband");
  plan = rounded_boundary.begin_frame(2, 80, false);
  output = rounded_boundary.finish_frame(
      plan, reliable_pair(plan, BBox{100.6F, 70, 58.8F, 60}, 21),
      std::nullopt);
  expect_near(output.bbox.width, 60.0F, 1e-5F,
              "float-rounded exact two-percent shrink remains in the deadband");

  plan = rounded_boundary.begin_frame(3, 120, false);
  output = rounded_boundary.finish_frame(
      plan, reliable_pair(plan, BBox{99.3999F, 70, 61.2002F, 60}, 22),
      std::nullopt);
  expect_true(output.bbox.width > 60.0F,
              "growth just outside the rounded deadband starts smoothing");
  ResolvedControllerFixture rounded_shrink(config);
  rounded_shrink.initialize(BBox{100, 70, 60, 60}, 0, 0, {400, 300});
  plan = rounded_shrink.begin_frame(1, 40, false);
  output = rounded_shrink.finish_frame(
      plan, reliable_pair(plan, BBox{100.6001F, 70, 58.7998F, 60}, 23),
      std::nullopt);
  expect_true(output.bbox.width < 60.0F,
              "shrink just outside the rounded deadband starts smoothing");

  plan = controller.begin_frame(2, 80, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, BBox{98.5F, 69, 103, 82}, 2), std::nullopt);
  expect_near(output.bbox.width, 100.3F, 1e-4F,
              "width just outside the deadband begins a smoothed update");
  expect_near(output.bbox.height, 80.2F, 1e-4F,
              "height just outside the deadband begins a smoothed update");

  ResolvedControllerFixture growth(config);
  growth.initialize(BBox{100, 70, 100, 80}, 0, 0, {400, 300});
  plan = growth.begin_frame(1, 40, false);
  output = growth.finish_frame(
      plan, reliable_pair(plan, BBox{75, 30, 150, 160}, 3), std::nullopt);
  expect_near(output.bbox.width, 100.5F, 1e-4F,
              "five-percent outer width target is smoothed to a half-percent step");
  expect_near(output.bbox.height, 80.4F, 1e-4F,
              "five-percent outer height target is smoothed to a half-percent step");
  expect_near(output.bbox.x + 0.5F * output.bbox.width, 150.0F, 1e-5F,
              "large growth preserves the raw horizontal center");
  expect_near(output.bbox.y + 0.5F * output.bbox.height, 110.0F, 1e-5F,
              "large growth preserves the raw vertical center");

  ResolvedControllerFixture shrink(config);
  shrink.initialize(BBox{100, 70, 100, 80}, 0, 0, {400, 300});
  plan = shrink.begin_frame(1, 40, false);
  output = shrink.finish_frame(
      plan, reliable_pair(plan, BBox{125, 90, 50, 40}, 4), std::nullopt);
  expect_near(output.bbox.width, 99.5F, 1e-4F,
              "large shrink is smoothed to a half-percent width step");
  expect_near(output.bbox.height, 79.6F, 1e-4F,
              "large shrink is smoothed to a half-percent height step");

  float previous_width = 100.5F;
  for (int frame = 2; frame <= 200; ++frame) {
    plan = growth.begin_frame(frame, frame * 40, false);
    output = growth.finish_frame(
        plan, reliable_pair(plan, BBox{75, 30, 150, 160}, frame + 10),
        std::nullopt);
    expect_true(output.bbox.width + 1e-5F >= previous_width &&
                    output.bbox.width - previous_width <=
                        previous_width * 0.005F + 1e-4F,
                "persistent growth converges monotonically within the half-percent cap");
    previous_width = output.bbox.width;
  }
  expect_true(std::fabs(output.bbox.width - 150.0F) / output.bbox.width <=
                  0.020001F,
              "persistent real scale change converges until it reaches the deadband");

  ResolvedControllerFixture edge(config);
  edge.initialize(BBox{100, 70, 100, 80}, 0, 0, {400, 300});
  plan = edge.begin_frame(1, 40, false);
  output = edge.finish_frame(
      plan, reliable_pair(plan, BBox{0, 0, 20, 20}, 1), std::nullopt);
  expect_true(output.accepted && output.bbox.x == 0.0F && output.bbox.y == 0.0F &&
                  output.bbox.x + output.bbox.width <= 400.0F &&
                  output.bbox.y + output.bbox.height <= 300.0F,
              "smoothed scale remains frame-safe through the centering helper");

  config.max_accepted_scale_step_ratio = 1.0F;
  config.accepted_scale_deadband_ratio = 0.0F;
  config.accepted_scale_smoothing_alpha = 1.0F;
  ResolvedControllerFixture unrestricted(config);
  unrestricted.initialize(BBox{40, 30, 20, 10}, 0, 0, {240, 160});
  plan = unrestricted.begin_frame(1, 40, false);
  output = unrestricted.finish_frame(
      plan, reliable_pair(plan, BBox{70, 50, 60, 40}, 1), std::nullopt);
  expect_true(output.accepted && output.bbox.width == 60.0F &&
                  output.bbox.height == 40.0F,
              "scale-step ratio one preserves legacy unrestricted acceptance");
}

nanotrack::ControllerConfig temporal_scale_config() {
  nanotrack::ControllerConfig config;
  config.max_accepted_scale_step_ratio = 0.05F;
  config.accepted_scale_deadband_ratio = 0.02F;
  config.accepted_scale_smoothing_alpha = 0.10F;
  config.accepted_scale_median_window_frames = 5;
  config.accepted_scale_direction_confirm_frames = 3;
  config.accepted_scale_output_hysteresis_px = 0.75F;
  config.max_center_distance = 10.0F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  return config;
}

nanotrack::ControllerOutput accept_scale_frame(
    nanotrack::LongTermController& controller, int frame,
    const nanotrack::BBox& bbox, std::uint64_t token = 1) {
  const auto plan = controller.begin_frame(frame, frame * 40, false);
  return controller.finish_frame(plan, reliable_pair(plan, bbox, token),
                                 std::nullopt);
}

void test_temporal_scale_window_direction_and_hysteresis() {
  using namespace nanotrack;
  const ControllerConfig config = temporal_scale_config();
  ResolvedControllerFixture delayed(config);
  delayed.initialize(BBox{200, 150, 200, 100}, 0, 0, {1000, 600});
  const BBox growth{100, 150, 400, 100};
  ControllerOutput output;
  for (int frame = 1; frame <= 6; ++frame) {
    output = accept_scale_frame(delayed, frame, growth, frame);
    expect_near(output.bbox.width, 200.0F, 1e-5F,
                "window fill and first two full-window directions hold scale");
  }
  output = accept_scale_frame(delayed, 7, growth, 7);
  expect_near(output.bbox.width, 201.0F, 1e-5F,
              "third consecutive full-window growth median advances one pixel");
  expect_near(output.bbox.x + 0.5F * output.bbox.width, 300.0F, 1e-5F,
              "temporal filtering preserves the accepted raw center");

  ResolvedControllerFixture alternating(config);
  alternating.initialize(BBox{100, 80, 100, 48}, 0, 0, {500, 300});
  for (int frame = 1; frame <= 20; ++frame) {
    const float height = frame % 2 == 0 ? 48.0F : 49.0F;
    output = accept_scale_frame(
        alternating, frame, BBox{100, 104.0F - 0.5F * height, 100, height},
        100 + frame);
    expect_near(output.bbox.height, 48.0F, 1e-5F,
                "alternating 48/49 accepted heights do not chatter");
  }

  ControllerConfig boundary = config;
  boundary.accepted_scale_median_window_frames = 1;
  boundary.accepted_scale_direction_confirm_frames = 1;
  boundary.accepted_scale_deadband_ratio = 0.0F;
  ResolvedControllerFixture just_inside(boundary);
  just_inside.initialize(BBox{100, 80, 149, 80}, 0, 0, {600, 400});
  output = accept_scale_frame(just_inside, 1, BBox{24.5F, 80, 300, 80});
  expect_near(output.bbox.width, 149.0F, 1e-5F,
              "filtered movement just inside 0.75 pixels holds emitted size");

  ResolvedControllerFixture exact_boundary(boundary);
  exact_boundary.initialize(BBox{100, 80, 150, 80}, 0, 0, {600, 400});
  output = accept_scale_frame(exact_boundary, 1, BBox{25, 80, 300, 80});
  expect_near(output.bbox.width, 151.0F, 1e-5F,
              "exact 0.75-pixel boundary advances one emitted pixel");

  ControllerConfig zero_hysteresis = config;
  zero_hysteresis.accepted_scale_output_hysteresis_px = 0.0F;
  ResolvedControllerFixture monotonic(zero_hysteresis);
  monotonic.initialize(BBox{100, 80, 100, 80}, 0, 0, {600, 400});
  float previous = 100.0F;
  for (int frame = 1; frame <= 40; ++frame) {
    output = accept_scale_frame(monotonic, frame, BBox{50, 80, 200, 80});
    expect_true(output.bbox.width + 1e-5F >= previous,
                "zero-hysteresis temporal growth cannot reverse");
    previous = output.bbox.width;
  }
}

void test_temporal_scale_persistence_and_state_isolation() {
  using namespace nanotrack;
  const ControllerConfig config = temporal_scale_config();
  ResolvedControllerFixture sustained(config);
  sustained.initialize(BBox{200, 150, 200, 160}, 0, 0, {1000, 700});
  float previous_width = 200.0F;
  float previous_height = 160.0F;
  ControllerOutput output;
  for (int frame = 1; frame <= 600; ++frame) {
    const BBox raw{100, 190, 400, 80};
    output = accept_scale_frame(sustained, frame, raw, frame);
    expect_true(output.bbox.width + 1e-5F >= previous_width &&
                    output.bbox.width - previous_width <= 1.00001F &&
                    output.bbox.height <= previous_height + 1e-5F &&
                    previous_height - output.bbox.height <= 1.00001F,
                "persistent scale change moves monotonically by at most one pixel");
    expect_near(output.bbox.x + 0.5F * output.bbox.width, 300.0F, 1e-5F,
                "persistent width response preserves raw center");
    expect_near(output.bbox.y + 0.5F * output.bbox.height, 230.0F, 1e-5F,
                "persistent height response preserves raw center");
    previous_width = output.bbox.width;
    previous_height = output.bbox.height;
  }
  expect_true(output.bbox.width > 200.0F && output.bbox.height < 160.0F,
              "persistent growth and shrink evidence eventually changes output");
  expect_true(std::fabs(output.bbox.width - 400.0F) <= 400.0F * 0.02F + 0.75F &&
                  std::fabs(output.bbox.height - 80.0F) <= 80.0F * 0.02F + 0.75F,
              "persistent scale evidence converges through emission backlog");

  ResolvedControllerFixture frozen(config);
  frozen.initialize(BBox{200, 150, 200, 100}, 0, 0, {1000, 600});
  const BBox growth{100, 150, 400, 100};
  for (int frame = 1; frame <= 4; ++frame) {
    accept_scale_frame(frozen, frame, growth, frame);
  }
  auto gap_plan = frozen.begin_frame(5, 200, false);
  output = frozen.finish_frame(gap_plan, {}, std::nullopt);
  expect_true(!output.accepted, "fixture creates a nonaccepted scale gap");
  frozen.loss_start_ms_.reset();
  frozen.mode_ = TrackingMode::kTracking;
  for (int frame = 6; frame <= 8; ++frame) {
    output = accept_scale_frame(frozen, frame, growth, frame);
    expect_near(output.bbox.width, frame < 8 ? 200.0F : 201.0F, 1e-5F,
                "nonaccepted gap adds no sample and preserves prior history");
  }

  ResolvedControllerFixture aborted(config);
  aborted.initialize(BBox{200, 150, 200, 100}, 0, 0, {1000, 600});
  for (int frame = 1; frame <= 4; ++frame) {
    accept_scale_frame(aborted, frame, growth, 20 + frame);
  }
  const auto abort_plan = aborted.begin_frame(5, 200, false);
  (void)abort_plan;
  aborted.abort_frame();
  for (int frame = 6; frame <= 8; ++frame) {
    output = accept_scale_frame(aborted, frame, growth, 30 + frame);
    expect_near(output.bbox.width, frame < 8 ? 200.0F : 201.0F, 1e-5F,
                "aborted frame adds no sample and preserves prior history");
  }

  ResolvedControllerFixture recovery(config);
  recovery.initialize(BBox{200, 150, 200, 100}, 0, 0, {1000, 600});
  for (int frame = 1; frame <= 6; ++frame) {
    accept_scale_frame(recovery, frame, growth, 50 + frame);
  }
  reject_frame(recovery, 7, 280);
  accept_scale_frame(recovery, 8, growth, 58);
  output = accept_scale_frame(recovery, 9, growth, 59);
  expect_true(output.accepted && !output.reacquired,
              "short confirmation returns to tracking without a recovery reset");
  expect_near(output.bbox.width, 201.0F, 1e-5F,
              "short confirmation preserves accepted scale history");

  ControllerConfig compatibility = config;
  compatibility.max_accepted_scale_step_ratio = 1.0F;
  compatibility.accepted_scale_deadband_ratio = 0.0F;
  compatibility.accepted_scale_smoothing_alpha = 1.0F;
  compatibility.accepted_scale_median_window_frames = 1;
  compatibility.accepted_scale_direction_confirm_frames = 1;
  compatibility.accepted_scale_output_hysteresis_px = 0.0F;
  ResolvedControllerFixture direct(compatibility);
  direct.initialize(BBox{40, 30, 20, 10}, 0, 0, {240, 160});
  output = accept_scale_frame(direct, 1, BBox{70, 50, 60, 40});
  expect_true(output.bbox.width == 60.0F && output.bbox.height == 40.0F,
              "1/1/0 temporal settings emit raw accepted dimensions exactly");
}

nanotrack::ControllerConfig center_filter_config() {
  nanotrack::ControllerConfig config;
  config.accepted_center_smoothing_alpha = 0.35F;
  config.accepted_center_max_correction_ratio = 0.25F;
  config.max_center_distance = 100.0F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  config.max_reacquisition_center_distance = 100.0F;
  return config;
}

float center_x(const nanotrack::BBox& bbox) {
  return bbox.x + 0.5F * bbox.width;
}

float center_y(const nanotrack::BBox& bbox) {
  return bbox.y + 0.5F * bbox.height;
}

void test_motion_aware_accepted_center_filter() {
  using namespace nanotrack;
  const BBox initial{80, 80, 40, 40};

  ControllerConfig compatibility = center_filter_config();
  compatibility.accepted_center_smoothing_alpha = 1.0F;
  compatibility.accepted_center_max_correction_ratio = 0.0F;
  ResolvedControllerFixture direct(compatibility);
  direct.initialize(initial, 0, 0, {400, 300});
  auto plan = direct.begin_frame(1, 40, false);
  const BBox direct_raw{110, 70, 40, 40};
  auto output = direct.finish_frame(
      plan, reliable_pair(plan, direct_raw, 1), std::nullopt);
  expect_true(output.accepted && output.selected_observation_bbox.has_value(),
              "accepted output retains the selected raw bbox");
  expect_near(center_x(output.bbox), 130.0F, 1e-5F,
              "1/0 center compatibility emits the exact raw horizontal center");
  expect_near(center_y(output.bbox), 90.0F, 1e-5F,
              "1/0 center compatibility emits the exact raw vertical center");
  expect_near(output.center_innovation_x, 30.0F, 1e-5F,
              "compatibility diagnostics retain raw horizontal innovation");
  expect_near(output.center_correction_x, 30.0F, 1e-5F,
              "compatibility diagnostics report the full applied correction");
  expect_true(!output.center_correction_limited,
              "zero correction ratio disables limiting");

  ResolvedControllerFixture alternating(center_filter_config());
  alternating.initialize(initial, 0, 0, {400, 300});
  float previous_final = center_x(initial);
  float maximum_final_step = 0.0F;
  for (int frame = 1; frame <= 12; ++frame) {
    plan = alternating.begin_frame(frame, frame * 40, false);
    const float raw_center = frame % 2 == 0 ? 80.0F : 120.0F;
    const BBox raw{raw_center - 20.0F, 80, 40, 40};
    output = alternating.finish_frame(
        plan, reliable_pair(plan, raw, 10 + frame), std::nullopt);
    maximum_final_step =
        std::max(maximum_final_step, std::fabs(center_x(output.bbox) - previous_final));
    previous_final = center_x(output.bbox);
  }
  expect_true(maximum_final_step < 10.0F,
              "motion-aware blend materially reduces alternating forty-pixel raw steps");

  ResolvedControllerFixture exact_limit(center_filter_config());
  exact_limit.initialize(initial, 0, 0, {400, 300});
  plan = exact_limit.begin_frame(1, 40, false);
  output = exact_limit.finish_frame(
      plan, reliable_pair(plan, BBox{90, 80, 40, 40}, 30), std::nullopt);
  expect_near(output.center_innovation_x, 10.0F, 1e-5F,
              "exact limit fixture has a ten-pixel innovation");
  expect_near(output.center_correction_x, 3.5F, 1e-5F,
              "exact limit applies alpha after preserving the full innovation");
  expect_true(!output.center_correction_limited,
              "innovation exactly at the target-relative limit is not clipped");

  ResolvedControllerFixture over_limit(center_filter_config());
  over_limit.initialize(initial, 0, 0, {400, 300});
  plan = over_limit.begin_frame(1, 40, false);
  output = over_limit.finish_frame(
      plan, reliable_pair(plan, BBox{90.001F, 80, 40, 40}, 31),
      std::nullopt);
  expect_near(center_x(output.tracking_bbox), 110.001F, 1e-4F,
              "tracking state follows the accepted raw center without display limiting");
  expect_near(center_y(output.tracking_bbox), 100.0F, 1e-5F,
              "tracking state preserves the accepted raw vertical center");
  expect_true(output.control_bbox.x == output.tracking_bbox.x &&
                  output.control_bbox.y == output.tracking_bbox.y &&
                  output.control_bbox.width == output.tracking_bbox.width &&
                  output.control_bbox.height == output.tracking_bbox.height,
              "authorized control box uses the fast tracking state");
  expect_true(output.display_bbox.x == output.bbox.x &&
                  output.display_bbox.y == output.bbox.y &&
                  output.display_bbox.width == output.bbox.width &&
                  output.display_bbox.height == output.bbox.height,
              "legacy bbox remains the smoothed display box");
  expect_true(center_x(output.display_bbox) < center_x(output.tracking_bbox),
              "display smoothing cannot slow the tracking feedback state");
  expect_near(output.center_innovation_x, 10.001F, 1e-4F,
              "over-limit diagnostics retain the unbounded raw innovation");
  expect_near(output.center_correction_x, 3.5F, 1e-4F,
              "over-limit innovation is isotropically bounded before blending");
  expect_near(output.center_correction_y, 0.0F, 1e-5F,
              "horizontal limiting does not invent vertical correction");
  expect_true(output.center_correction_limited,
              "just-over-boundary innovation reports limiting");
  expect_true(output.accepted_observation_path ==
                  AcceptedObservationPath::kDirect,
              "ordinary reliable tracking reports the direct acceptance path");

  ResolvedControllerFixture diagonal(center_filter_config());
  diagonal.initialize(initial, 0, 0, {400, 300});
  plan = diagonal.begin_frame(1, 40, false);
  output = diagonal.finish_frame(
      plan, reliable_pair(plan, BBox{96, 92, 40, 40}, 32), std::nullopt);
  expect_true(output.center_correction_limited,
              "large diagonal innovation activates one vector-magnitude bound");
  expect_near(std::hypot(output.center_correction_x, output.center_correction_y),
              3.5F, 1e-4F,
              "isotropic correction magnitude equals alpha times the ten-pixel bound");
  expect_near(output.center_correction_x / output.center_correction_y,
              8.0F / 6.0F, 1e-4F,
              "isotropic limiting preserves innovation direction");

  ResolvedControllerFixture moving(center_filter_config());
  moving.initialize(initial, 0, 0, {500, 300});
  float moving_center = center_x(initial);
  for (int frame = 1; frame <= 8; ++frame) {
    plan = moving.begin_frame(frame, frame * 40, false);
    const float raw_center = 100.0F + 5.0F * frame;
    output = moving.finish_frame(
        plan,
        reliable_pair(plan, BBox{raw_center - 20.0F, 80, 40, 40}, 40 + frame),
        std::nullopt);
    expect_true(center_x(output.bbox) > moving_center,
                "persistent same-direction measurements advance every accepted frame");
    moving_center = center_x(output.bbox);
  }
  expect_true(moving_center > 110.0F,
              "motion-aware filtering follows sustained target motion with bounded lag");

  ResolvedControllerFixture tiny(center_filter_config());
  tiny.initialize(BBox{50, 50, 1, 1}, 0, 0, {100, 100});
  plan = tiny.begin_frame(1, 40, false);
  output = tiny.finish_frame(
      plan, reliable_pair(plan, BBox{51, 51, 1, 1}, 60), std::nullopt);
  expect_true(std::isfinite(output.bbox.x) && std::isfinite(output.bbox.y) &&
                  output.bbox.width == 1.0F && output.bbox.height == 1.0F,
              "one-pixel target produces a finite positive bounded correction");

  ResolvedControllerFixture edge(center_filter_config());
  edge.initialize(BBox{90, 90, 10, 10}, 0, 0, {100, 100});
  plan = edge.begin_frame(1, 40, false);
  output = edge.finish_frame(
      plan, reliable_pair(plan, BBox{0, 0, 10, 10}, 61), std::nullopt);
  expect_true(output.bbox.x >= 0.0F && output.bbox.y >= 0.0F &&
                  output.bbox.x + output.bbox.width <= 100.0F &&
                  output.bbox.y + output.bbox.height <= 100.0F,
              "center state estimation remains frame-safe near an image edge");

  ResolvedControllerFixture isolated(center_filter_config());
  isolated.initialize(initial, 0, 0, {400, 300});
  plan = isolated.begin_frame(1, 40, false);
  output = isolated.finish_frame(
      plan,
      {rejected_observation(plan, TemplateKind::kAnchor, BBox{130, 80, 40, 40})},
      std::nullopt);
  expect_true(!output.accepted && !output.selected_observation_bbox.has_value() &&
                  output.center_correction_x == 0.0F &&
                  output.center_correction_y == 0.0F &&
                  isolated.current_bbox().x == initial.x,
              "rejected observations cannot apply or publish a center correction");
  isolated.begin_frame(2, 80, false);
  isolated.abort_frame();
  expect_true(isolated.current_bbox().x == initial.x &&
                  isolated.current_bbox().y == initial.y,
              "aborting an open frame cannot mutate the accepted center state");

  ResolvedControllerFixture short_confirmation(center_filter_config());
  short_confirmation.initialize(initial, 0, 0, {400, 300});
  reject_frame(short_confirmation, 1, 40);
  const BBox short_confirmed_raw{120, 80, 40, 40};
  plan = short_confirmation.begin_frame(2, 80, false);
  output = short_confirmation.finish_frame(
      plan, reliable_pair(plan, short_confirmed_raw, 68), std::nullopt);
  expect_true(!output.accepted,
              "first reliable short-gap hit only enters confirmation");
  plan = short_confirmation.begin_frame(3, 120, false);
  output = short_confirmation.finish_frame(
      plan, reliable_pair(plan, short_confirmed_raw, 69), std::nullopt);
  expect_true(output.accepted && !output.reacquired &&
                  output.center_correction_limited &&
                  output.accepted_observation_path ==
                      AcceptedObservationPath::kShortConfirmation,
              "short uncertain confirmation remains on the bounded center path");
  expect_true(center_x(output.bbox) < center_x(short_confirmed_raw) &&
                  center_x(output.bbox) > center_x(initial),
              "short uncertain confirmation blends instead of snapping to raw center");
  plan = short_confirmation.begin_frame(4, 160, false);
  output = short_confirmation.finish_frame(
      plan, reliable_pair(plan, short_confirmed_raw, 70), std::nullopt);
  expect_true(output.accepted && !output.reacquired &&
                  short_confirmation.confirmations_.empty(),
              "short confirmation clears loss state for ordinary direct tracking");

  ResolvedControllerFixture recovery(center_filter_config());
  recovery.initialize(initial, 0, 0, {400, 300});
  reject_frame(recovery, 1, 40);
  recovery.motion_.velocity_x_ = 100.0F;
  const BBox recovered_raw{120, 80, 40, 40};
  plan = recovery.begin_frame(2, 240, false);
  expect_true(plan.mode == TrackingMode::kLocalRecovery,
              "stale-motion recovery fixture reaches local recovery");
  output = recovery.finish_frame(
      plan, reliable_pair(plan, recovered_raw, 70), std::nullopt);
  expect_true(!output.accepted,
              "first reliable recovery hit only enters confirmation");
  plan = recovery.begin_frame(3, 280, false);
  output = recovery.finish_frame(
      plan, reliable_pair(plan, recovered_raw, 71), std::nullopt);
  expect_true(output.accepted && output.reacquired &&
                  output.accepted_observation_path ==
                      AcceptedObservationPath::kLocalOrDeeperRecovery,
              "second consistent recovery hit confirms reacquisition");
  expect_near(center_x(output.bbox), center_x(recovered_raw), 1e-5F,
              "confirmed reacquisition uses the raw center instead of stale prediction");
  expect_near(center_y(output.bbox), center_y(recovered_raw), 1e-5F,
              "confirmed reacquisition resets both center coordinates from raw evidence");
}

void test_ordinary_recovery_uses_fixed_center_gate() {
  using namespace nanotrack;
  ControllerConfig config;
  config.max_reacquisition_center_distance = 0.75F;
  config.max_center_distance = 1.50F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  ResolvedControllerFixture controller(config);
  const BBox stable{40, 40, 20, 20};
  const BBox in_view_recovery{48, 40, 20, 20};
  const BBox center_jump{56, 40, 20, 20};
  controller.initialize(stable, 0, 0, {240, 160});

  auto output = reject_frame(controller, 1, 40);
  expect_true(!output.accepted && !output.control_valid,
              "recovery fixture first enters an unaccepted interval");
  auto plan = controller.begin_frame(2, 80, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, in_view_recovery, 2), std::nullopt);
  expect_true(!output.accepted && controller.confirmations_.size() == 1 &&
                  output.normalized_center_residual <= 0.453F,
              "observed in-view residual can enter ordinary confirmation");

  plan = controller.begin_frame(3, 120, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, center_jump, 3), std::nullopt);
  expect_true(!output.accepted && controller.confirmations_.empty() &&
                  output.quality == QualityLevel::kReliable &&
                  output.normalized_center_residual > 0.75F,
              "fixed center gate clears a reliable but displaced recovery hit");

  plan = controller.begin_frame(4, 160, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, in_view_recovery, 4), std::nullopt);
  expect_true(!output.accepted && controller.confirmations_.size() == 1,
              "first good hit after a center jump starts a fresh queue");
  plan = controller.begin_frame(5, 200, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, in_view_recovery, 5), std::nullopt);
  expect_true(output.accepted && !output.reacquired && output.control_valid,
              "two fresh in-view hits complete short confirmation without reset");

  ResolvedControllerFixture template_conflict(config);
  template_conflict.initialize(stable, 0, 0, {240, 160});
  plan = template_conflict.begin_frame(1, 40, false);
  const std::vector<TrackerObservation> conflicting{
      reliable_observation(plan, TemplateKind::kAnchor, center_jump, 9),
      reliable_observation(plan, TemplateKind::kRecent,
                           BBox{40, 60, 20, 20}, 9)};
  output = template_conflict.finish_frame(plan, conflicting, std::nullopt);
  expect_true(!output.accepted && template_conflict.confirmations_.empty() &&
                  output.normalized_center_residual > 0.75F,
              "template-conflict confirmation also uses the fixed center gate");

  config.max_reacquisition_center_distance = 0.0F;
  ResolvedControllerFixture compatibility(config);
  compatibility.initialize(stable, 0, 0, {240, 160});
  reject_frame(compatibility, 1, 40);
  for (int frame : {2, 3}) {
    plan = compatibility.begin_frame(frame, frame * 40, false);
    output = compatibility.finish_frame(
        plan, reliable_pair(plan, center_jump, frame), std::nullopt);
  }
  expect_true(output.accepted && !output.reacquired,
              "zero sentinel preserves the confirmation gate without forcing reset");
}

void test_loss_freezes_scale_and_recovery_uses_limited_bbox() {
  using namespace nanotrack;
  ControllerConfig config;
  config.uncertain_ms = 80;
  config.max_accepted_scale_step_ratio = 0.05F;
  config.max_center_distance = 10.0F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  ResolvedControllerFixture controller(config);
  controller.initialize(BBox{40, 30, 20, 10}, 0, 0, {240, 160});

  auto plan = controller.begin_frame(1, 40, false);
  auto output = controller.finish_frame(
      plan, reliable_pair(plan, BBox{50, 40, 40, 20}, 1), std::nullopt);
  expect_near(output.bbox.width, 21.0F, 1e-5F,
              "fixture establishes a limited last accepted width");
  expect_true(controller.motion_.velocity_width_ != 0.0F &&
                  controller.motion_.velocity_height_ != 0.0F,
              "fixture establishes nonzero scale velocity before loss");

  plan = controller.begin_frame(2, 80, false);
  const float first_predicted_center_x =
      plan.predicted_bbox.x + 0.5F * plan.predicted_bbox.width;
  output = controller.finish_frame(plan, {}, std::nullopt);
  expect_true(!output.accepted && !output.control_valid &&
                  controller.motion_.scale_frozen(),
              "first unaccepted frame freezes motion scale without authorizing control");
  expect_near(output.bbox.width, 21.0F, 1e-5F,
              "first unaccepted output restores the last accepted width");
  expect_near(output.bbox.height, 10.5F, 1e-5F,
              "first unaccepted output restores the last accepted height");
  expect_near(output.bbox.x + 0.5F * output.bbox.width,
              first_predicted_center_x, 1e-5F,
              "scale freeze retains the motion-predicted center");

  plan = controller.begin_frame(3, 120, false);
  const float second_predicted_center_x =
      plan.predicted_bbox.x + 0.5F * plan.predicted_bbox.width;
  output = controller.finish_frame(plan, {}, std::nullopt);
  expect_near(output.bbox.width, 21.0F, 1e-5F,
              "repeated recovery prediction keeps frozen width");
  expect_near(output.bbox.height, 10.5F, 1e-5F,
              "repeated recovery prediction keeps frozen height");
  expect_true(std::fabs(second_predicted_center_x - first_predicted_center_x) >
                  1e-5F,
              "frozen scale does not freeze center motion");

  const BBox recovery_observation{80, 55, 40, 30};
  plan = controller.begin_frame(4, 160, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, recovery_observation, 4), std::nullopt);
  expect_true(!output.accepted && controller.confirmations_.size() == 1 &&
                  controller.motion_.scale_frozen(),
              "first reliable recovery hit remains unaccepted and frozen");
  expect_near(output.bbox.width, 21.0F, 1e-5F,
              "unconfirmed enlarged-crop scale cannot change final width");

  plan = controller.begin_frame(5, 200, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, recovery_observation, 5), std::nullopt);
  expect_true(output.accepted && output.reacquired && output.control_valid &&
                  !controller.motion_.scale_frozen(),
              "second consistent recovery hit accepts and unfreezes stabilized scale");
  expect_near(output.bbox.width, 22.05F, 1e-4F,
              "accepted recovery width is limited to five percent");
  expect_near(output.bbox.height, 11.025F, 1e-4F,
              "accepted recovery height is limited to five percent");
  expect_near(output.bbox.x + 0.5F * output.bbox.width, 100.0F, 1e-5F,
              "accepted recovery keeps the observed horizontal center");
  expect_near(output.bbox.y + 0.5F * output.bbox.height, 70.0F, 1e-5F,
              "accepted recovery keeps the observed vertical center");
  expect_true(controller.current_bbox_.x == output.tracking_bbox.x &&
                  controller.current_bbox_.y == output.tracking_bbox.y &&
                  controller.current_bbox_.width == output.tracking_bbox.width &&
                  controller.current_bbox_.height == output.tracking_bbox.height &&
                  controller.motion_.current_bbox().x == output.tracking_bbox.x &&
                  controller.motion_.current_bbox().y == output.tracking_bbox.y &&
                  controller.motion_.current_bbox().width == output.tracking_bbox.width &&
                  controller.motion_.current_bbox().height == output.tracking_bbox.height,
              "accepted recovery installs the fast tracking box into controller and motion state");
}

void test_suspicion_event_pulses_are_single_shot() {
  using namespace nanotrack;
  ResolvedControllerFixture controller{ControllerConfig{}};
  DebugTelemetry telemetry(1000);
  const BBox stable{60, 40, 20, 20};
  controller.initialize(stable, 0, 0, {240, 160}, make_fixed_target_class(0, true));
  const auto process = [&](const std::vector<TrackerObservation>& observations,
                           const SearchPlan& plan) {
    RuntimeFrameResult frame;
    frame.plan = plan;
    frame.observations = observations;
    frame.output = controller.finish_frame(plan, observations, std::nullopt);
    const auto events = telemetry.frame_events(frame, 1, false);
    std::size_t starts = 0, clears = 0;
    for (const auto& event : events) {
      starts += event.find("event=occlusion_suspicion_start ") != std::string::npos;
      clears += event.find("event=occlusion_suspicion_clear ") != std::string::npos;
    }
    return std::make_tuple(frame.output, starts, clears);
  };
  for (int id = 1; id <= 8; ++id) {
    auto plan = controller.begin_frame(id, id * 40, false);
    process(reliable_pair(plan, stable, id), plan);
  }
  auto plan = controller.begin_frame(9, 360, false);
  auto [trigger, starts, clears] = process(reliable_pair(plan, BBox{63, 40, 14, 20}, 9), plan);
  expect_true(trigger.occlusion_hold_triggered && trigger.occlusion_suspicion_started &&
                  starts == 1 && clears == 0,
              "severe trigger emits exactly one suspicion start at age one");
  for (int age = 2; age <= 12; ++age) {
    plan = probe_plan(controller, age);
    auto [output, frame_starts, frame_clears] = process({}, plan);
    expect_true(output.occlusion_coast_age_frames == static_cast<std::size_t>(age) &&
                    output.control_valid && !output.occlusion_suspicion_started &&
                    frame_starts == 0 && frame_clears == 0,
                "each hold frame keeps motion control without repeating pulses");
  }
  for (int id = 21; id <= 23; ++id) {
    plan = controller.begin_frame(id, id * 40, false);
    auto observations = std::vector<TrackerObservation>{
        weak_observation(plan, TemplateKind::kAnchor, stable, id),
        weak_observation(plan, TemplateKind::kRecent, stable, id)};
    auto [output, frame_starts, frame_clears] = process(observations, plan);
    expect_true(output.mode == TrackingMode::kLocalRecovery &&
                    !output.occlusion_suspicion_started && frame_starts == 0 &&
                    frame_clears == 0 &&
                    output.occlusion_reference_width == 20 &&
                    output.occlusion_trigger_reason == OcclusionTriggerReason::kSevereWidth,
                "selected recovery observations preserve reference without replaying pulses");
  }

  ResolvedControllerFixture clean{ControllerConfig{}};
  DebugTelemetry clean_telemetry(1000);
  clean.initialize(stable, 0, 0, {240, 160}, make_fixed_target_class(0, true));
  for (int id = 1; id <= 12; ++id) {
    auto clean_plan = clean.begin_frame(id, id * 40, false);
    const auto bbox = id == 9 ? BBox{60.4F, 40, 19.2F, 20} : stable;
    auto observations = reliable_pair(clean_plan, bbox, id);
    RuntimeFrameResult frame;
    frame.plan = clean_plan;
    frame.observations = observations;
    frame.output = clean.finish_frame(clean_plan, observations, std::nullopt);
    const auto events = clean_telemetry.frame_events(frame, 1, false);
    std::size_t clear_count = 0;
    for (const auto& event : events)
      clear_count += event.find("event=occlusion_suspicion_clear ") != std::string::npos;
    expect_true(clear_count == static_cast<std::size_t>(id == 11) &&
                    frame.output.occlusion_suspicion_cleared == (id == 11),
                "normal clean observations emit the clear pulse exactly once");
    if (id == 11) {
      expect_true(!clean.occlusion_decision_.suspicion_cleared,
                  "published clear pulse is consumed from the retained decision");
    }
  }
}

void test_coast_generation_provenance() {
  using namespace nanotrack;
  ResolvedControllerFixture controller(ControllerConfig{});
  start_probe_coast(controller);  // Generation 0 was invalidated by coast.
  advance_probe_coast(controller, 2, 4);
  auto plan = probe_plan(controller, 5);
  controller.finish_frame(plan, reliable_pair(plan, {60, 40, 20, 20}, 13), std::nullopt);
  plan = probe_plan(controller, 6);
  expect_true(controller.finish_frame(plan, reliable_pair(plan, {60, 40, 20, 20}, 14),
                                      std::nullopt).reacquired,
              "provenance fixture recovers before a second coast");
  for (int frame = 15; frame <= 22; ++frame) {
    plan = controller.begin_frame(frame, frame * 40, false);
    controller.finish_frame(plan, reliable_pair(plan, {60, 40, 20, 20}, frame), std::nullopt);
  }
  plan = controller.begin_frame(23, 920, false);
  expect_true(controller.finish_frame(plan, reliable_pair(plan, {63, 40, 14, 20}, 23),
                                      std::nullopt).occlusion_hold_triggered,
              "second coast invalidates generation one without forgetting generation zero");
  std::uint64_t frame = 24;
  for (std::uint64_t generation : {0, 1, 99}) {
    plan = controller.begin_frame(frame, frame * 40, false);
    // Feed stale and forged identities: coast provenance must beat age/frame
    // mismatch, while an unrelated future generation stays GENERATION_INVALIDATED.
    DetectorBatch batch{1, 1, {{{60, 40, 20, 20}, 0.99F, 0}}};
    batch.generation = generation;
    const auto output = controller.finish_frame(plan, {}, batch);
    expect_true(output.detector_candidate_diagnostics[0].rejection ==
                    (generation == 99 ? DetectorCandidateRejection::kGenerationInvalidated :
                                        DetectorCandidateRejection::kHoldInvalidated) &&
                    output.detector_candidates_queued == 0,
                "coast provenance survives earlier recovery and multiple coast generations: " +
                    std::to_string(generation) + " got " +
                    to_string(output.detector_candidate_diagnostics[0].rejection));
    ++frame;
  }
  controller.initialize({60, 40, 20, 20}, 30, 1200, {240, 160},
                        make_fixed_target_class(0, true));
  for (std::uint64_t generation : {0, 1, 2}) {
    plan = controller.begin_frame(31 + generation, 1240 + generation * 40, false);
    DetectorBatch batch{1, 1, {{{60, 40, 20, 20}, 0.99F, 0}}};
    batch.generation = generation;
    const auto output = controller.finish_frame(plan, {}, batch);
    expect_true(output.detector_candidate_diagnostics[0].rejection ==
                    DetectorCandidateRejection::kGenerationInvalidated &&
                    output.detector_candidates_queued == 0 && !output.control_valid,
                "explicit reinitialize takes precedence over old coast provenance");
  }
}

void test_coast_probe_schedule_and_isolation() {
  using namespace nanotrack;
  ControllerConfig config;
  ResolvedControllerFixture missed(config);
  start_probe_coast(missed);
  for (int age = 2; age <= 12; ++age) {
    const auto plan = probe_plan(missed, age);
    const auto expected = age == 5 || age == 10
        ? OcclusionProbeType::kSearch : OcclusionProbeType::kNone;
    expect_true(plan.occlusion_probe_type == expected &&
                    plan.run_tracker == (age == 5 || age == 10) &&
                    !plan.request_detector && !plan.detector_request &&
                    plan.motion_control_authorized &&
                    plan.predicted_bbox.x == 60 && plan.predicted_bbox.width == 20,
                "misses schedule search only at ages five and ten from frozen motion");
    const auto output = missed.finish_frame(plan, {}, std::nullopt);
    expect_true(output.occlusion_probe_type == expected && output.control_valid &&
                    !output.accepted && !output.reacquired &&
                    output.hypothesis_source == HypothesisSource::kMotion,
                "missed probes retain motion control and report their type");
  }

  // A pending recent template and an off-center hit expose any accidental
  // normal-acceptance, motion-correction, or template/reference update path.
  ResolvedControllerFixture isolated(config);
  start_probe_coast(isolated);
  isolated.recent_probation_ = true;
  isolated.probation_reliable_frames_ = 1;
  const auto recent_update = isolated.last_recent_update_ms_;
  const auto reference_widths = isolated.occlusion_evidence_.stable_widths_;
  const auto reference_heights = isolated.occlusion_evidence_.stable_heights_;
  const auto reference_peaks = isolated.occlusion_evidence_.stable_peak_margins_;
  const auto evidence_previous = isolated.occlusion_evidence_.previous_bbox_;
  advance_probe_coast(isolated, 2, 4);
  auto plan = probe_plan(isolated, 5);
  auto output = isolated.finish_frame(
      plan, reliable_pair(plan, BBox{62, 39, 22, 22}, 13), std::nullopt);
  expect_true(output.occlusion_probe_type == OcclusionProbeType::kSearch &&
                  output.occlusion_probe_confirmation_hits == 1 &&
                  output.occlusion_probe_confirmation_required == 2 &&
                  output.bbox.x == 60 && output.bbox.width == 20 &&
                  !output.accepted && !output.reacquired && output.control_valid &&
                  output.motion_control_authorized &&
                  output.template_action == TemplateAction::kNone &&
                  isolated.current_bbox().x == 60 &&
                  isolated.recent_probation_ && isolated.probation_reliable_frames_ == 1 &&
                  isolated.last_recent_update_ms_ == recent_update &&
                  isolated.occlusion_evidence_.stable_widths_ == reference_widths &&
                  isolated.occlusion_evidence_.stable_heights_ == reference_heights &&
                  isolated.occlusion_evidence_.stable_peak_margins_ == reference_peaks &&
                  isolated.occlusion_evidence_.previous_bbox_->width == evidence_previous->width &&
                  isolated.occlusion_decision_.reference_width == std::optional<float>(20),
              "search stores one hit without changing control, templates or reference");
  expect_near(isolated.motion_.velocity_x_, 0, 1e-6F,
              "search does not correct motion velocity");
  plan = probe_plan(isolated, 6);
  expect_true(plan.occlusion_probe_type == OcclusionProbeType::kConfirm &&
                  plan.run_tracker && !plan.request_detector &&
                  plan.predicted_bbox.x == 60,
              "search schedules immediate next-frame confirm without moving the search center");
  output = isolated.finish_frame(plan, {}, std::nullopt);
  expect_true(!output.accepted && output.control_valid && output.bbox.x == 60 &&
                  output.template_action == TemplateAction::kNone &&
                  isolated.recent_probation_ && isolated.probation_reliable_frames_ == 1 &&
                  output.occlusion_probe_confirmation_hits == 0,
              "failed confirmation discards the hit without any acceptance effects");
  for (int age = 7; age <= 10; ++age) {
    plan = probe_plan(isolated, age);
    expect_true(plan.occlusion_probe_type == (age == 10 ? OcclusionProbeType::kSearch
                                                       : OcclusionProbeType::kNone),
                "failed confirm returns to the original five-frame schedule");
    isolated.finish_frame(plan, {}, std::nullopt);
  }

  for (int interval : {1, 4, 12, 300}) {
    ControllerConfig configured;
    configured.occlusion_probe_interval_frames = interval;
    ResolvedControllerFixture controller(configured);
    start_probe_coast(controller);
    for (int age = 2; age <= 12; ++age) {
      plan = probe_plan(controller, age);
      const bool search = interval == 1 ||
          (interval == 4 && (age == 4 || age == 8 || age == 12)) ||
          (interval == 12 && age == 12);
      expect_true(plan.run_tracker == search && plan.occlusion_probe_type ==
                      (search ? OcclusionProbeType::kSearch : OcclusionProbeType::kNone) &&
                      !plan.request_detector,
                  "probe schedule honors configured intervals within the twelve-frame cap");
      controller.finish_frame(plan, {}, std::nullopt);
    }
  }

  ControllerConfig final_search_config;
  final_search_config.occlusion_probe_interval_frames = 12;
  ResolvedControllerFixture final_search(final_search_config);
  start_probe_coast(final_search);
  advance_probe_coast(final_search, 2, 11);
  plan = probe_plan(final_search, 12);
  output = final_search.finish_frame(plan,
      reliable_pair(plan, BBox{63, 40, 20, 20}, 20), std::nullopt);
  expect_true(!output.accepted && output.control_valid &&
                  output.occlusion_probe_confirmation_hits == 1 &&
                  !final_search.coast_probe_candidate_,
              "a search hit on the last authorized age cannot extend the coast budget");
  plan = probe_plan(final_search, 13);
  output = final_search.finish_frame(plan,
      reliable_pair(plan, BBox{63, 40, 20, 20}, 21), std::nullopt);
  expect_true(plan.occlusion_probe_type == OcclusionProbeType::kNone &&
                  !plan.motion_control_authorized && !output.accepted && !output.control_valid,
              "age thirteen needs fresh ordinary confirmation after a last-age search hit");
}

void test_coast_probe_gates_and_recovery() {
  using namespace nanotrack;
  const BBox stable{60, 40, 20, 20};
  // Each missing per-frame gate must prevent both a search candidate and a
  // confirmation, even though the other frame is a perfect dual-template hit.
  for (bool on_confirm : {false, true}) {
    for (int failure = 0; failure < 8; ++failure) {
      ResolvedControllerFixture controller{ControllerConfig{}};
      start_probe_coast(controller);
      advance_probe_coast(controller, 2, 4);
      auto plan = probe_plan(controller, 5);
      if (on_confirm) {
        controller.finish_frame(plan, reliable_pair(plan, stable, 13), std::nullopt);
        plan = probe_plan(controller, 6);
      }
      expect_true(plan.run_tracker && plan.occlusion_probe_type ==
                      (on_confirm ? OcclusionProbeType::kConfirm : OcclusionProbeType::kSearch),
                  "per-frame negative fixture exercises an actual search or confirm probe");
      auto observations = reliable_pair(plan, stable, plan.frame_id);
      switch (failure) {
        case 0: observations.erase(observations.begin()); break;
        case 1: observations.pop_back(); break;
        case 2: observations[0] = weak_observation(plan, TemplateKind::kAnchor,
                                                   stable, plan.frame_id); break;
        case 3: observations[1] = weak_observation(plan, TemplateKind::kRecent,
                                                   stable, plan.frame_id); break;
        case 4: observations[1].bbox.x = 78; break; // template IoU < 0.30
        case 5:
          observations = reliable_pair(plan, BBox{160, 40, 20, 20}, plan.frame_id);
          break; // motion center gate
        case 6:
          observations = reliable_pair(plan, BBox{52, 32, 36, 36}, plan.frame_id);
          break; // motion area gate
        case 7:
          observations = reliable_pair(plan, BBox{55, 45, 30, 10}, plan.frame_id);
          break; // motion aspect gate
      }
      const auto output = controller.finish_frame(plan, observations, std::nullopt);
      expect_true(output.occlusion_probe_rejection != "NONE" &&
                      output.occlusion_probe_motion_bbox.has_value() &&
                      (failure == 0 || failure == 1 || output.occlusion_probe_bbox.has_value()),
                  "rejected probe exposes its reason and available observation and motion boxes");
      if (failure == 2) expect_true(!output.occlusion_probe_anchor_score_gate.value_or(true), "weak anchor exposes score gate");
      if (failure == 3) expect_true(!output.occlusion_probe_recent_score_gate.value_or(true), "weak recent exposes score gate");
      if (failure == 3) {
        RuntimeFrameResult frame;
        frame.plan = plan;
        frame.output = output;
        frame.observations = observations;
        const auto overlay = make_interactive_debug_overlay(frame);
        bool saw_recent = false;
        for (const auto& box : overlay.boxes) {
          if (box.label.find("COAST ") != std::string::npos &&
              box.label.find("RECENT") != std::string::npos) {
            saw_recent = box.label.find("selected_quality=RELIABLE") != std::string::npos &&
                         box.label.find("anchor=1 recent=0") != std::string::npos &&
                         box.label.find(" quality=RELIABLE") == std::string::npos;
          }
        }
        expect_true(saw_recent, "bad recent overlay labels selected quality and both template gates truthfully");
      }
      if (failure == 4) expect_true(!output.occlusion_probe_template_gate.value_or(true), "template disagreement exposes gate");
      if (failure == 5) expect_true(!output.occlusion_probe_anchor_center_gate.value_or(true), "motion center rejection exposes gate");
      if (failure == 6) expect_true(!output.occlusion_probe_anchor_area_gate.value_or(true), "motion area rejection exposes gate");
      if (failure == 7) expect_true(!output.occlusion_probe_anchor_aspect_gate.value_or(true), "motion aspect rejection exposes gate");
      expect_true(!output.accepted && !output.reacquired && output.control_valid &&
                      output.bbox.x == 60 && output.bbox.width == 20 &&
                      output.template_action == TemplateAction::kNone &&
                      output.occlusion_probe_confirmation_hits == 0,
                  "all per-frame probe gates are mandatory on search and confirm");
      plan = probe_plan(controller, on_confirm ? 7 : 6);
      expect_true(plan.occlusion_probe_type == OcclusionProbeType::kNone && !plan.run_tracker,
                  "failed per-frame gates cannot leave a pending confirm");
      controller.finish_frame(plan, {}, std::nullopt);
    }
  }

  struct GeometryCase { BBox search; BBox confirm; bool loose_iou; };
  const GeometryCase inconsistent[] = {
      {{56.6F, 40, 20, 20}, {63.4F, 40, 20, 20}, false}, // IoU .493, center .34
      {{60, 40, 20, 20}, {67.1F, 40, 20, 20}, true}, // center .355
      {{62.5F, 42.5F, 15, 15}, stable, false}, // area log 0.575
      {{61, 39, 18, 22}, {59, 41, 22, 18}, false}, // aspect log 0.401
  };
  for (const auto& geometry : inconsistent) {
    ControllerConfig config;
    if (geometry.loose_iou) config.occlusion_confirmation_iou = 0.30F;
    // Deliberately loosen detector gates: coast must use its own four defaults.
    config.detector_confirmation_iou = 0.10F;
    config.detector_confirmation_center_distance = 2.0F;
    config.detector_confirmation_max_area_log_change = 2.0F;
    config.detector_confirmation_max_aspect_log_change = 2.0F;
    ResolvedControllerFixture controller(config);
    start_probe_coast(controller);
    advance_probe_coast(controller, 2, 4);
    auto plan = probe_plan(controller, 5);
    auto output = controller.finish_frame(plan,
        reliable_pair(plan, geometry.search, 13), std::nullopt);
    expect_true(output.occlusion_probe_rejection == "NONE" &&
                    output.occlusion_probe_template_gate.value_or(false) &&
                    output.occlusion_probe_anchor_reliable.value_or(false) &&
                    output.occlusion_probe_recent_reliable.value_or(false),
                "search success exposes evaluated gates");
    expect_true(output.occlusion_probe_confirmation_hits == 1,
                "cross-frame fixture first passes every per-frame search gate");
    plan = probe_plan(controller, 6);
    output = controller.finish_frame(plan,
        reliable_pair(plan, geometry.confirm, 14), std::nullopt);
    expect_true(output.occlusion_probe_rejection == "CONFIRM_GEOMETRY" &&
                    output.occlusion_probe_cross_gate.has_value() &&
                    !*output.occlusion_probe_cross_gate,
                "confirm mismatch exposes cross-frame gate failure");
    expect_true(output.quality == QualityLevel::kReliable && output.template_iou == 1 &&
                    !output.accepted && !output.reacquired &&
                    output.mode == TrackingMode::kOcclusionHold && output.bbox.x == 60 &&
                    output.occlusion_probe_confirmation_hits == 0,
                "different nearby boxes fail the independent coast cross-frame gates");
  }

  for (int search_age : {5, 10}) {
    ResolvedControllerFixture controller{ControllerConfig{}};
    start_probe_coast(controller);
    advance_probe_coast(controller, 2, search_age - 1);
    auto plan = probe_plan(controller, search_age);
    controller.finish_frame(plan, reliable_pair(plan, BBox{63, 40, 20, 20}, 100),
                            std::nullopt);
    // The next processed frame may skip decoded IDs and timestamps; coast ages
    // count completed controller frames, not numeric frame-ID differences.
    plan = controller.begin_frame(100, 5000, true);
    expect_true(plan.occlusion_probe_type == OcclusionProbeType::kConfirm &&
                    plan.occlusion_coast_age_frames == static_cast<std::size_t>(search_age + 1) &&
                    !plan.request_detector && plan.run_tracker,
                "ages six and eleven confirm on the next processed frame despite timestamp gaps");
    const auto output = controller.finish_frame(plan,
        reliable_pair(plan, BBox{64, 40, 20, 20}, 101), std::nullopt);
    expect_true(output.occlusion_reference_source == OcclusionReferenceSource::kFullStableMedian &&
                    output.occlusion_reference_width == 20 && output.occlusion_reference_height == 20 &&
                    output.occlusion_trigger_reason == OcclusionTriggerReason::kSevereWidth,
                "recovery output retains pre-reset coast reference and reason");
    RuntimeFrameResult event_frame;
    event_frame.plan = plan;
    event_frame.output = output;
    DebugTelemetry telemetry;
    auto previous = event_frame;
    previous.output.mode = TrackingMode::kOcclusionHold;
    telemetry.frame_events(previous, 1, false);
    const auto events = telemetry.frame_events(event_frame, 1, false);
    bool event_snapshot = false;
    for (const auto& event : events) {
      if (event.find("event=occlusion_reacquired") != std::string::npos &&
          event.find("reference_source=FULL_STABLE_MEDIAN") != std::string::npos &&
          event.find("reference_width=20.000") != std::string::npos &&
          event.find("reference_height=20.000") != std::string::npos &&
          event.find("trigger_reason=SEVERE_WIDTH") != std::string::npos) event_snapshot = true;
    }
    const auto row = make_tracking_log_row(event_frame);
    expect_true(event_snapshot && row.occlusion_reference_width == 20 &&
                    row.occlusion_trigger_reason == OcclusionTriggerReason::kSevereWidth,
                "real controller recovery retains reference and reason in event and CSV row");
    expect_true(output.occlusion_probe_rejection == "NONE" &&
                    output.occlusion_probe_cross_gate.value_or(false),
                "confirmed recovery exposes successful cross-frame gate");
    expect_true(output.accepted && output.reacquired && output.control_valid &&
                    output.accepted_observation_path ==
                        AcceptedObservationPath::kOcclusionProbeRecovery &&
                    !output.motion_control_authorized &&
                    output.mode == TrackingMode::kTracking && output.bbox.x == 64 &&
                    output.occlusion_probe_type == OcclusionProbeType::kConfirm &&
                    output.occlusion_probe_confirmation_hits == 2 &&
                    output.occlusion_probe_confirmation_required == 2 &&
                    controller.hold_remaining_frames_ == 0 &&
                    controller.occlusion_coast_age_frames_ == 0 &&
                    !controller.hold_used_for_loss_ && !controller.loss_start_ms_ &&
                    !controller.motion_.scale_frozen() &&
                    !controller.frozen_scale_reference_ &&
                    controller.confirmations_.empty() && controller.detector_candidates_.empty() &&
                    !controller.active_detector_candidate_,
                "only a validated second hit exits coast and clears recovery state");
    expect_near(controller.motion_.velocity_x_, 0, 1e-6F,
                "confirmed recovery reinitializes motion rather than correcting it");
    expect_true(!controller.occlusion_decision_.reference_width &&
                    !controller.occlusion_decision_.suspicion_active,
                "confirmed recovery clears old stable reference and seeds only the new box");
    plan = controller.begin_frame(101, 5040, true);
    expect_true(plan.mode == TrackingMode::kTracking && plan.predicted_bbox.x == 64 &&
                    plan.occlusion_probe_type == OcclusionProbeType::kNone &&
                    plan.occlusion_coast_age_frames == 0 && !plan.request_detector,
                "confirmed box becomes the next-frame motion center with no stale probe");
    controller.finish_frame(plan, reliable_pair(plan, BBox{64, 40, 20, 20}, 102), std::nullopt);
  }
}

void test_coast_probe_stale_identity_and_reset() {
  using namespace nanotrack;
  for (bool on_confirm : {false, true}) {
    for (int mismatch = 0; mismatch < 3; ++mismatch) {
      ResolvedControllerFixture controller{ControllerConfig{}};
      start_probe_coast(controller);
      advance_probe_coast(controller, 2, 4);
      auto plan = probe_plan(controller, 5);
      if (on_confirm) {
        controller.finish_frame(plan, reliable_pair(plan, BBox{60, 40, 20, 20}, 13),
                                std::nullopt);
        plan = probe_plan(controller, 6);
      }
      auto observations = reliable_pair(plan, BBox{60, 40, 20, 20}, plan.frame_id);
      if (mismatch == 0) --observations[0].frame_id;
      if (mismatch == 1) --observations[0].timestamp_ms;
      if (mismatch == 2) ++observations[1].search_token;
      bool threw = false;
      try { controller.finish_frame(plan, observations, std::nullopt); }
      catch (const std::invalid_argument&) { threw = true; }
      expect_true(threw, "stale probe frame/timestamp or mismatched template token is rejected");
      expect_true(!controller.frame_open_ && controller.hold_remaining_frames_ == 0,
                  "invalid probe identity aborts the failed coast session without an output");
      plan = probe_plan(controller, on_confirm ? 7 : 6);
      expect_true(plan.occlusion_probe_type != OcclusionProbeType::kConfirm &&
                      !plan.motion_control_authorized,
                  "invalid probe identity cannot survive as a pending candidate");
      controller.finish_frame(plan, {}, std::nullopt);
    }
  }

  ResolvedControllerFixture reset{ControllerConfig{}};
  start_probe_coast(reset);
  advance_probe_coast(reset, 2, 4);
  auto plan = probe_plan(reset, 5);
  reset.finish_frame(plan, reliable_pair(plan, BBox{60, 40, 20, 20}, 13), std::nullopt);
  const auto old_confirm = probe_plan(reset, 6);
  reset.initialize(BBox{100, 60, 30, 30}, 14, 560, {240, 160});
  bool threw = false;
  try { reset.finish_frame(old_confirm,
      reliable_pair(old_confirm, BBox{60, 40, 20, 20}, 14), std::nullopt); }
  catch (const std::logic_error&) { threw = true; }
  plan = reset.begin_frame(15, 600, true);
  expect_true(threw && plan.mode == TrackingMode::kTracking &&
                  plan.occlusion_probe_type == OcclusionProbeType::kNone &&
                  plan.predicted_bbox.x == 100 && !plan.motion_control_authorized,
              "same-frame reinitialize discards the pending probe and old open plan");
  reset.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture duplicate{ControllerConfig{}};
  start_probe_coast(duplicate);
  advance_probe_coast(duplicate, 2, 4);
  plan = probe_plan(duplicate, 5);
  duplicate.finish_frame(plan, reliable_pair(plan, BBox{60, 40, 20, 20}, 13), std::nullopt);
  threw = false;
  try { duplicate.finish_frame(plan, {}, std::nullopt); }
  catch (const std::logic_error&) { threw = true; }
  plan = probe_plan(duplicate, 6);
  expect_true(threw && !duplicate.coast_probe_candidate_ &&
                  plan.occlusion_probe_type == OcclusionProbeType::kNone,
              "a duplicate stale result discards the search candidate even with no open frame");
  duplicate.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture stale_plan{ControllerConfig{}};
  start_probe_coast(stale_plan);
  advance_probe_coast(stale_plan, 2, 4);
  plan = probe_plan(stale_plan, 5);
  stale_plan.finish_frame(plan, reliable_pair(plan, BBox{60, 40, 20, 20}, 13),
                          std::nullopt);
  const auto active_confirm = probe_plan(stale_plan, 6);
  threw = false;
  try { stale_plan.finish_frame(plan, reliable_pair(plan, BBox{60, 40, 20, 20}, 13),
                                std::nullopt); }
  catch (const std::logic_error&) { threw = true; }
  expect_true(threw && !stale_plan.frame_open_ && stale_plan.hold_remaining_frames_ == 0,
              "a stale search plan cannot retain or complete the active confirm session");
  if (!stale_plan.frame_open_) {
    plan = stale_plan.begin_frame(active_confirm.frame_id + 1, active_confirm.timestamp_ms + 40, false);
    expect_true(!plan.motion_control_authorized && plan.occlusion_probe_type == OcclusionProbeType::kNone,
                "the frame after a stale plan has no probe or coast authorization");
    stale_plan.finish_frame(plan, {}, std::nullopt);
  }
}

// These cases catch class-gate bypasses, mutable session state, and rejection
// precedence changes without relying on detector or rendering test doubles.
void test_target_class_recovery_request_gate() {
  using namespace nanotrack;
  const BBox roi{40, 30, 20, 20};
  const FrameSize frame{200, 120};
  const auto established = establish_target_class(roi, frame, {{roi, 0.9F, 2}}, 3, {});
  for (const TargetClassState& state : {
           make_unknown_target_class(TargetClassReason::kNoMatch),
           make_unknown_target_class(TargetClassReason::kAmbiguous),
           make_unknown_target_class(TargetClassReason::kDetectorError),
           make_unknown_target_class(TargetClassReason::kDetectorUnavailable),
           make_fixed_target_class(2, false), make_fixed_target_class(2, true), established}) {
    ResolvedControllerFixture controller(ControllerConfig{});
    controller.initialize(roi, 0, 0, frame, state);
    reject_frame(controller, 1, 100);
    struct Step { std::int64_t ms; TrackingMode mode; bool request; };
    const Step steps[] = {
        {299, TrackingMode::kUncertain, false},
        {300, TrackingMode::kLocalRecovery, false},
        {1099, TrackingMode::kLocalRecovery, false},
        {1100, TrackingMode::kAssistedRecovery, true},
        {1599, TrackingMode::kAssistedRecovery, false},
        {1600, TrackingMode::kAssistedRecovery, true},
        {3099, TrackingMode::kAssistedRecovery, true},
        {3100, TrackingMode::kLost, false},
        {3600, TrackingMode::kLost, true}};
    std::uint64_t id = 2;
    for (const auto& step : steps) {
      const auto plan = controller.begin_frame(id++, step.ms, true);
      const bool expected = state.target_class_allows_recovery && step.request;
      expect_true(plan.mode == step.mode && plan.request_detector == expected &&
                      plan.detector_request.has_value() == expected,
                  "recovery request uses session class eligibility and existing interval at " +
                      std::to_string(step.ms) + " ms for " + to_string(state.status));
      const auto output = controller.finish_frame(plan, {}, std::nullopt);
      expect_true(!output.control_valid && !output.accepted && !output.reacquired &&
                      output.detector_candidates_queued == 0,
                  "request gating never creates a candidate or authorizes control");
    }
    if (!state.target_class_allows_recovery)
      expect_true(!controller.last_detector_request_ms_ && !controller.last_detector_request_frame_,
                  "disabled recovery leaves detector request history empty");
  }
}

void test_target_class_gate() {
  using namespace nanotrack;
  const BBox roi{40, 30, 20, 20};
  const FrameSize frame{200, 120};
  ControllerConfig config;
  config.detector_max_candidates = 1;
  const TargetClassState unknown = make_unknown_target_class(TargetClassReason::kNoMatch);
  const TargetClassState fixed = make_fixed_target_class(2, true);
  TargetClassState established = fixed;
  established.status = TargetClassStatus::kEstablished;
  established.reason = TargetClassReason::kUniqueMatch;
  for (const TargetClassState& state : {unknown, fixed, established,
                                       make_fixed_target_class(2, false)}) {
    ResolvedControllerFixture controller(config);
    controller.initialize(roi, 0, 0, frame, state);
    reject_frame(controller, 1, 100);
    const SearchPlan plan = controller.begin_frame(2, 1100, true);
    expect_true(plan.request_detector == state.target_class_allows_recovery &&
                    plan.hypothesis_source == HypothesisSource::kMotion,
                "only eligible initialized classes schedule recovery detector work");
    // Ingestion remains independently guarded even if an old worker completion
    // has a matching request identity. Disabled sessions cannot create this
    // request themselves; seed only its identity to exercise the ingestion gate.
    if (!state.target_class_allows_recovery) {
      controller.last_detector_request_frame_ = 2;
      controller.last_detector_request_ms_ = 1100;
    }
    DetectorBatch batch{2, 1100, {{roi, 0.9F, 2}, {roi, 0.99F, 3},
                                  {roi, 0.8F, -1}}};
    batch.generation = controller.detector_eligibility_generation_;
    batch.request_id = 9001;  // Adapter identity is independent of generation.
    const ControllerOutput output = controller.finish_frame(plan, {}, batch);
    const bool enabled = state.target_class_allows_recovery;
    expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                    output.detector_candidates_queued == (enabled ? 1U : 0U) &&
                    !output.accepted && !output.reacquired && !output.control_valid &&
                    output.hypothesis_source == HypothesisSource::kMotion &&
                    !output.detector_candidate_id,
                "batch reception and class matching never authorize control or a current-frame probe");
    expect_true(output.target_class.status == state.status &&
                    output.target_class.target_class_id == state.target_class_id &&
                    output.target_class.target_class_allows_recovery == enabled,
                "every output carries the immutable initialization class");
    const auto& match = output.detector_candidate_diagnostics[0];
    const auto& mismatch = output.detector_candidate_diagnostics[1];
    const auto& invalid = output.detector_candidate_diagnostics[2];
    expect_true(match.class_disposition == (enabled ? DetectorClassDisposition::kMatch
                                                   : DetectorClassDisposition::kTargetUnknown) &&
                    match.queued == enabled &&
                    match.rejection == (enabled ? DetectorCandidateRejection::kNone
                                                : DetectorCandidateRejection::kTargetClassUnknown),
                "only a permitted matching class enters the queue");
    expect_true(mismatch.class_disposition == (enabled ? DetectorClassDisposition::kMismatch
                                                      : DetectorClassDisposition::kTargetUnknown) &&
                    !mismatch.queued && !mismatch.candidate_id && mismatch.drawable &&
                    mismatch.rejection == (enabled ? DetectorCandidateRejection::kClassMismatch
                                                   : DetectorCandidateRejection::kTargetClassUnknown),
                "mismatch or UNKNOWN remains drawable and cannot acquire a candidate identity");
    expect_true(invalid.class_disposition == DetectorClassDisposition::kInvalidClass &&
                    invalid.rejection == DetectorCandidateRejection::kInvalid &&
                    !invalid.queued && !invalid.batch_eligible && invalid.drawable,
                "negative class remains drawable geometry but is structurally invalid");
    const SearchPlan next = controller.begin_frame(3, 1140, false);
    expect_true((next.hypothesis_source == HypothesisSource::kDetector) == enabled,
                "UNKNOWN and mismatched classes never become detector probes");
    controller.finish_frame(next, {}, std::nullopt);
  }

  for (const TargetClassState& state : {unknown, fixed, established}) {
    ResolvedControllerFixture controller(config);
    controller.initialize(roi, 0, 0, frame, state);
    reject_frame(controller, 1, 100);
    const auto plan = controller.begin_frame(2, 1100, true);
    if (!state.target_class_allows_recovery) {
      controller.last_detector_request_frame_ = 2;
      controller.last_detector_request_ms_ = 1100;
    }
    DetectorBatch batch{2, 1100, {{roi, 0.99F, 3}}};
    batch.generation = controller.detector_eligibility_generation_;
    const auto output = controller.finish_frame(plan, {}, batch);
    expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                    output.detector_candidates_queued == 0 &&
                    !output.accepted && !output.control_valid,
                "all-class-suppressed batch is accepted with zero queued candidates");
    const auto next = controller.begin_frame(3, 1140, false);
    expect_true(next.hypothesis_source == HypothesisSource::kMotion &&
                    !next.detector_candidate_id &&
                    output.target_class.target_class_id == state.target_class_id,
                "all mismatches cannot create a probe or change the session class");
    controller.finish_frame(next, {}, std::nullopt);
  }

  // Every structural rejection must dominate class/position/capacity failures;
  // class disposition must still be available even when the batch is rejected.
  for (const TargetClassState& state : {unknown, fixed}) {
    for (int stage = 0; stage < 6; ++stage) {
      ResolvedControllerFixture controller(config);
      controller.initialize(roi, 0, 0, frame, state);
      reject_frame(controller, 1, 100);
      SearchPlan request = controller.begin_frame(2, 1100, true);
      if (!state.target_class_allows_recovery) {
        controller.last_detector_request_frame_ = 2;
        controller.last_detector_request_ms_ = 1100;
      }
      controller.finish_frame(request, {}, std::nullopt);
      SearchPlan plan = controller.begin_frame(3, stage <= 2 ? 1300 : 1140, false);
      DetectorBatch batch{2, 1100, {{BBox{160, 70, 20, 20}, 0.9F, 3},
                                    {BBox{-1, 0, 20, 20}, 0.9F, 2},
                                    {roi, 0.9F, -1}}};
      batch.generation = controller.detector_eligibility_generation_;
      if (stage == 0) --batch.generation;
      if (stage <= 1) batch.source_frame_id = 999;
      // Inject HOLD at this public boundary to exercise simultaneous causes;
      // reachable HOLD entry/exit is covered independently below and in replay.
      if (stage <= 3) plan.mode = TrackingMode::kOcclusionHold;
      if (stage == 5) batch.candidates[0].class_id = 2;
      const auto output = controller.finish_frame(plan, {}, batch);
      const DetectorCandidateRejection expected[] = {
          DetectorCandidateRejection::kGenerationInvalidated,
          DetectorCandidateRejection::kFrameMismatch,
          DetectorCandidateRejection::kStale,
          DetectorCandidateRejection::kHoldInvalidated,
          state.target_class_allows_recovery ? DetectorCandidateRejection::kClassMismatch
                                            : DetectorCandidateRejection::kTargetClassUnknown,
          state.target_class_allows_recovery ? DetectorCandidateRejection::kPositionRejected
                                            : DetectorCandidateRejection::kTargetClassUnknown};
      const auto& candidate = output.detector_candidate_diagnostics[0];
      expect_true(candidate.rejection == expected[stage] && !candidate.queued &&
                      !output.control_valid,
                  "structural, HOLD, class and position rejection priority stage " + std::to_string(stage));
      expect_true(candidate.class_disposition == (!state.target_class_allows_recovery
                      ? DetectorClassDisposition::kTargetUnknown
                      : stage == 5 ? DetectorClassDisposition::kMatch
                                   : DetectorClassDisposition::kMismatch),
                  "class disposition stays orthogonal to structural rejection");
      expect_true(output.detector_candidate_diagnostics[1].rejection ==
                      DetectorCandidateRejection::kInvalid &&
                      output.detector_candidate_diagnostics[2].rejection ==
                          DetectorCandidateRejection::kInvalid &&
                      output.detector_candidate_diagnostics[2].class_disposition ==
                          DetectorClassDisposition::kInvalidClass,
                  "invalid geometry and class outrank all other rejection causes");
    }
  }

  ResolvedControllerFixture controller(config);
  TargetClassState caller_state = fixed;
  controller.initialize(roi, 0, 0, frame, caller_state);
  caller_state.target_class_id = 3;
  reject_frame(controller, 1, 100);
  auto plan = controller.begin_frame(2, 1100, true);
  DetectorBatch batch{2, 1100, {{roi, 0.9F, 2}, {BBox{41, 30, 20, 20}, 0.8F, 2}}};
  batch.generation = plan.detector_request->generation;
  auto output = controller.finish_frame(plan, {}, batch);
  expect_true(output.target_class.target_class_id == 2 &&
                  output.detector_candidates_queued == 1 &&
                  output.detector_candidate_diagnostics[1].class_disposition == DetectorClassDisposition::kMatch &&
                  output.detector_candidate_diagnostics[1].rejection == DetectorCandidateRejection::kCapacityRejected,
              "controller copies class state and applies capacity only after class/position");
  plan = controller.begin_frame(3, 1140, false);
  output = controller.finish_frame(plan, reliable_pair(plan, plan.predicted_bbox, 3), std::nullopt);
  expect_true(output.detector_candidate_locked && output.detector_confirmation_hits == 1 &&
                  !output.control_valid,
              "matching class still requires anchor/recent and a second confirmation");
  const auto generation = batch.generation;
  controller.initialize(roi, 3, 1140, frame, unknown);
  controller.initialize(roi, 3, 1140, frame, unknown);
  expect_true(controller.confirmations_.empty() && !controller.active_detector_candidate_ &&
                  controller.detector_candidates_.empty(),
              "same-frame reinitialize atomically clears all recovery state");
  reject_frame(controller, 4, 1180);
  plan = controller.begin_frame(5, 2180, true);
  expect_true(!plan.request_detector && !plan.detector_request &&
                  controller.detector_eligibility_generation_ == generation + 2 &&
                  plan.hypothesis_source == HypothesisSource::kMotion,
              "each same-frame reinitialize advances eligibility generation");
  batch.source_frame_id = 5;
  batch.source_timestamp_ms = 2180;
  output = controller.finish_frame(plan, {}, batch);
  expect_true(output.target_class.status == TargetClassStatus::kUnknown &&
                  output.detector_candidate_diagnostics[0].rejection == DetectorCandidateRejection::kGenerationInvalidated &&
                  output.detector_candidate_diagnostics[0].class_disposition == DetectorClassDisposition::kTargetUnknown &&
                  !output.control_valid && output.detector_candidates_queued == 0,
              "old generation cannot re-enter a newly UNKNOWN session");

  ControllerConfig hold_config = config;
  hold_config.occlusion_reference_window = 3;
  for (const auto& state : {unknown, fixed}) {
    ResolvedControllerFixture holding(hold_config);
    holding.initialize(roi, 0, 0, frame, state);
    for (std::uint64_t id = 1; id <= 3; ++id) {
      const auto reliable_plan = holding.begin_frame(id, id * 40, false);
      holding.finish_frame(reliable_plan, reliable_pair(reliable_plan, roi, id), std::nullopt);
    }
    auto trigger_plan = holding.begin_frame(4, 160, false);
    DetectorBatch old_batch{1, 40, {{roi, 0.9F, 3}}};
    old_batch.generation = std::numeric_limits<std::uint64_t>::max();
    const auto trigger = holding.finish_frame(trigger_plan,
        {weak_observation(trigger_plan, TemplateKind::kAnchor, BBox{43, 33, 14, 14})}, old_batch);
    expect_true(trigger.occlusion_hold_triggered &&
                    trigger.detector_candidate_diagnostics[0].rejection ==
                        DetectorCandidateRejection::kGenerationInvalidated &&
                    trigger.detector_candidate_diagnostics[0].class_disposition ==
                        (state.target_class_allows_recovery ? DetectorClassDisposition::kMismatch
                                                           : DetectorClassDisposition::kTargetUnknown) &&
                    trigger.control_valid && trigger.motion_control_authorized &&
                    trigger.occlusion_coast_age_frames == 1,
                "same-frame HOLD preserves earlier structural rejection and class disposition");
  }
}

void test_time_based_motion_search_control() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.motion_search_confidence_ms = 180;
  config.motion_max_speed_diagonals_per_s = 0.75F;
  config.motion_max_acceleration_diagonals_per_s2 = 3.0F;
  config.motion_scale_rate_limit_per_s = 0.30F;
  config.search_probe_interval_ms = 80;
  config.max_center_distance = 10.0F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  ResolvedControllerFixture controller(config);
  controller.initialize(BBox{40, 30, 20, 20}, 0, 0, {240, 160});

  auto plan = controller.begin_frame(1, 40, false);
  auto output = controller.finish_frame(
      plan, reliable_pair(plan, BBox{44, 30, 20, 20}, 1), std::nullopt);
  expect_true(output.accepted, "motion-search fixture learns from accepted motion");
  plan = controller.begin_frame(2, 80, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, BBox{48, 30, 20, 20}, 2), std::nullopt);
  expect_true(output.accepted, "motion-search fixture has two accepted samples");

  const auto generation_before = controller.detector_eligibility_generation_;
  plan = controller.begin_frame(3, 120, false);
  const BBox predicted_at_entry = plan.motion_predicted_bbox;
  output = controller.finish_frame(
      plan,
      {rejected_observation(plan, TemplateKind::kAnchor,
                            BBox{180, 100, 5, 5}, 3)},
      std::nullopt);
  expect_true(!output.accepted && !output.observation_valid &&
                  output.control_valid && output.motion_control_authorized &&
                  output.tracking_phase == TrackingPhase::kSearching &&
                  output.control_source == ControlSource::kMotionSearch &&
                  output.tracking_bbox.x == predicted_at_entry.x &&
                  output.control_bbox.x == predicted_at_entry.x,
              "first rejected observation enters SEARCHING with motion control");
  expect_true(controller.detector_eligibility_generation_ ==
                      generation_before + 1 &&
                  controller.confirmations_.empty() &&
                  controller.detector_candidates_.empty(),
              "search entry invalidates old recovery work exactly once");

  plan = controller.begin_frame(4, 200, false);
  expect_true(plan.tracking_phase == TrackingPhase::kSearching &&
                  plan.control_source == ControlSource::kMotionSearch &&
                  plan.motion_control_authorized,
              "confident search plan advertises motion control");
  output = controller.finish_frame(plan, {}, std::nullopt);
  expect_true(output.control_valid && output.motion_control_authorized &&
                  controller.detector_eligibility_generation_ ==
                      generation_before + 1,
              "continued search remains authorized without repeating invalidation");

  plan = controller.begin_frame(5, 301, false);
  expect_true(plan.motion_control_authorized && !plan.motion_prediction_confident,
              "search stays authorized after prediction confidence decays");
  output = controller.finish_frame(plan, {}, std::nullopt);
  expect_true(output.control_valid && output.motion_control_authorized &&
                  output.tracking_phase == TrackingPhase::kSearching &&
                  output.control_source == ControlSource::kMotionSearch,
              "decayed prediction continues bounded search control");

  plan = controller.begin_frame(6, 334, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 6), std::nullopt);
  expect_true(!output.accepted && output.control_valid &&
                  output.control_source == ControlSource::kMotionSearch,
              "first visual return remains confirmation-only while search control continues");
  plan = controller.begin_frame(7, 367, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 7), std::nullopt);
  expect_true(output.accepted && output.control_valid &&
                  output.control_source == ControlSource::kObservation &&
                  !controller.motion_.prediction_confident(367),
              "confirmed visual recovery ends motion search and restores observation control");

  ControllerConfig occlusion_config = config;
  occlusion_config.occlusion_reference_window = 3;
  ResolvedControllerFixture partial_occlusion(occlusion_config);
  const BBox stable{60, 40, 20, 20};
  partial_occlusion.initialize(stable, 0, 0, {240, 160});
  for (std::uint64_t frame = 1; frame <= 3; ++frame) {
    plan = partial_occlusion.begin_frame(frame, frame * 40, false);
    output = partial_occlusion.finish_frame(
        plan, reliable_pair(plan, stable, frame), std::nullopt);
  }
  plan = partial_occlusion.begin_frame(4, 160, false);
  output = partial_occlusion.finish_frame(
      plan, reliable_pair(plan, BBox{63, 40, 14, 20}, 4), std::nullopt);
  expect_true(!output.accepted && !output.occlusion_hold_triggered &&
                  output.control_valid && output.motion_control_authorized &&
                  output.control_source == ControlSource::kMotionSearch &&
                  output.tracking_bbox.width == stable.width,
              "reliable partial-occlusion collapse starts time-based search without committing scale");
}

void test_unified_recovery_candidate_cleanup() {
  using namespace nanotrack;
  const BBox initial{40, 30, 20, 20};
  ControllerConfig config;
  config.motion_search_enabled = true;
  ResolvedControllerFixture controller(config);
  for (int disruption = 0; disruption < 6; ++disruption) {
    controller.initialize(initial, 0, 0, {240, 160});
    controller.finish_frame(controller.begin_frame(1, 40, false), {}, std::nullopt);
    auto plan = controller.begin_frame(2, 80, false);
    const auto seeded = controller.finish_frame(
        plan, {reliable_observation(plan, TemplateKind::kRecent, initial, 2)},
        std::nullopt);
    expect_true(seeded.recovery_candidate_key &&
                    seeded.recovery_confirmation_hits == 1 && !seeded.accepted,
                "cleanup fixture starts with one verified hit");

    std::uint64_t next_frame = 3;
    std::int64_t next_time = 120;
    if (disruption == 0) {
      plan = controller.begin_frame(next_frame, next_time, false);
      expect_true(plan.recovery_probe == seeded.recovery_candidate_key,
                  "selected hit is used for one search center");
      controller.abort_frame();
    } else if (disruption == 1) {
      plan = controller.begin_frame(next_frame, next_time, false);
      auto malformed = reliable_observation(plan, TemplateKind::kRecent, initial, 3);
      ++malformed.frame_id;
      bool threw = false;
      try {
        controller.finish_frame(plan, {malformed}, std::nullopt);
      } catch (const std::invalid_argument&) {
        threw = true;
      }
      expect_true(threw, "malformed observation invalidates the active probe");
    } else if (disruption == 2) {
      next_frame = 5;
    } else if (disruption == 3) {
      next_time = 80 + config.motion_max_dt_ms + 1;
    } else if (disruption == 4) {
      bool threw = false;
      try {
        controller.begin_frame(2, 90, false);
      } catch (const std::invalid_argument&) {
        threw = true;
      }
      expect_true(threw, "a repeated frame cannot open a probe");
    } else {
      controller.initialize(initial, 0, 0, {240, 160});
      controller.finish_frame(controller.begin_frame(1, 40, false), {}, std::nullopt);
      next_frame = 2;
      next_time = 80;
    }

    plan = controller.begin_frame(next_frame, next_time, false);
    const auto fresh = controller.finish_frame(
        plan, {reliable_observation(plan, TemplateKind::kRecent, initial, next_frame)},
        std::nullopt);
    expect_true(!plan.recovery_probe && !fresh.accepted &&
                    fresh.recovery_confirmation_hits == 1 &&
                    fresh.recovery_candidate_key != seeded.recovery_candidate_key,
                "abort, exception, gap, stale frame and reset require a fresh identity and two new hits");
  }

  ResolvedControllerFixture anchor_only(config);
  anchor_only.initialize(initial, 0, 0, {240, 160});
  anchor_only.finish_frame(anchor_only.begin_frame(1, 40, false), {}, std::nullopt);
  for (std::uint64_t frame : {2U, 3U}) {
    const auto plan = anchor_only.begin_frame(frame, frame * 40, false);
    const auto out = anchor_only.finish_frame(
        plan, {reliable_observation(plan, TemplateKind::kAnchor, initial, frame)},
        std::nullopt);
    expect_true(!out.recovery_candidate_key && !out.accepted,
                "an anchor-only observation cannot seed or confirm unified search");
  }

  ResolvedControllerFixture stale(config);
  stale.initialize(initial, 0, 0, {240, 160}, make_fixed_target_class(0, true));
  stale.finish_frame(stale.begin_frame(1, 40, false), {}, std::nullopt);
  auto plan = stale.begin_frame(2, 80, true);
  const auto request = *plan.detector_request;
  stale.finish_frame(plan, {}, std::nullopt);
  plan = stale.begin_frame(3, 200, false);
  const auto seeded = stale.finish_frame(
      plan, {reliable_observation(plan, TemplateKind::kRecent, initial, 3)},
      std::nullopt);
  plan = stale.begin_frame(4, 240, false);
  DetectorBatch batch{request.frame_id, request.timestamp_ms,
                      {{initial, 0.9F, 0}}};
  batch.generation = request.generation;
  const auto expired = stale.finish_frame(
      plan, {reliable_observation(plan, TemplateKind::kRecent, initial, 4)}, batch);
  expect_true(expired.detector_status == DetectorBatchStatus::kStale &&
                  !expired.accepted && expired.recovery_confirmation_hits == 0 &&
                  expired.recovery_candidate_key != seeded.recovery_candidate_key,
              "a stale completion cannot carry either candidate or detector history through the frame");
}

void test_subtarget_probe_abort_does_not_advance_cursor() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.detector_target_mode = DetectorTargetMode::kSubtarget;
  config.detector_subtarget_max_probes = 2;
  config.detector_interval_ms = 20;
  ResolvedControllerFixture controller(config);
  controller.initialize({40, 30, 20, 20}, 0, 0, {400, 240}, make_fixed_target_class(0, true));
  controller.finish_frame(controller.begin_frame(1, 20, false), {}, std::nullopt);

  const auto seed = [&](const SearchPlan& plan) {
    expect_true(plan.detector_request.has_value(), "abort fixture has an eligible seed");
    DetectorBatch batch{plan.frame_id, plan.timestamp_ms, {{{110, 60, 100, 80}, 0.9F, 0}}};
    if (plan.detector_request) batch.generation = plan.detector_request->generation;
    controller.finish_frame(plan, {}, batch);
  };
  seed(controller.begin_frame(2, 40, true));
  const auto planned = controller.begin_frame(3, 60, false);
  const auto* candidate = planned.recovery_probe
      ? controller.recovery_candidates_.find(*planned.recovery_probe) : nullptr;
  expect_true(candidate && candidate->subtarget_probes_attempted == 0 &&
                  planned.detector_probe_index == 0,
              "opening an eligible subtarget frame plans without consuming the cursor");
  controller.abort_frame();
  const auto retry = controller.begin_frame(3, 60, true);
  expect_true(!retry.recovery_probe, "aborted candidate cannot replay on the retried frame");
  seed(retry);
  const auto fresh = controller.begin_frame(4, 80, false);
  expect_true(fresh.recovery_probe && fresh.recovery_probe != planned.recovery_probe &&
                  fresh.detector_probe_index == 0,
              "the fresh seed starts at the initial offset after abort");
  controller.abort_frame();
}

void test_atomic_search_reacquisition_and_template_probation() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.motion_search_confidence_ms = 1000;
  config.recent_probation_frames = 2;
  config.max_center_distance = 10.0F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  ControllerConfig delayed_recovery = config;
  delayed_recovery.search_reacquisition_min_age_ms = 100;
  ResolvedControllerFixture minimum_age(delayed_recovery);
  const BBox initial{40, 30, 20, 20};
  minimum_age.initialize(initial, 0, 0, {240, 160});
  reject_frame(minimum_age, 1, 40);
  for (std::uint64_t frame : {2U, 3U}) {
    auto early_plan = minimum_age.begin_frame(frame, frame * 40, false);
    const auto early = minimum_age.finish_frame(
        early_plan,
        {reliable_observation(early_plan, TemplateKind::kRecent,
                              early_plan.predicted_bbox, frame)},
        std::nullopt);
    expect_true(!early.accepted && !early.search_hint,
                "visual confirmations before minimum search age cannot seed recovery");
  }
  for (std::uint64_t frame : {4U, 5U}) {
    auto mature_plan = minimum_age.begin_frame(frame, frame * 40, false);
    const auto mature = minimum_age.finish_frame(
        mature_plan,
        {reliable_observation(mature_plan, TemplateKind::kRecent,
                              mature_plan.predicted_bbox, frame)},
        std::nullopt);
    expect_true((frame == 4 && !mature.accepted && mature.search_hint) ||
                    (frame == 5 && mature.accepted && mature.reacquired),
                "mature search still requires two consistent visual confirmations");
  }
  ResolvedControllerFixture controller(config);
  controller.initialize(initial, 0, 0, {240, 160});
  auto output = reject_frame(controller, 1, 40);
  const BBox committed_before = controller.current_bbox();

  auto plan = controller.begin_frame(2, 80, false);
  output = controller.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent,
                            plan.predicted_bbox, 2)},
      std::nullopt);
  expect_true(!output.accepted && output.control_valid &&
                  output.template_action == TemplateAction::kNone &&
                  controller.current_bbox().x == committed_before.x,
              "one recent hit remains shadow-only with motion control");

  plan = controller.begin_frame(3, 120, false);
  const BBox confirmed = plan.predicted_bbox;
  output = controller.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent, confirmed, 3)},
      std::nullopt);
  expect_true(output.accepted && output.reacquired &&
                  output.accepted_observation_path ==
                      AcceptedObservationPath::kSearchReacquisition &&
                  output.control_source == ControlSource::kObservation &&
                  output.template_action == TemplateAction::kStageRecent &&
                  controller.current_bbox().x == confirmed.x &&
                  !controller.motion_search_active_,
              "second identity-consistent recent hit commits atomically and stages recent probation");

  plan = controller.begin_frame(4, 160, false);
  output = controller.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent, plan.predicted_bbox, 4),
       reliable_observation(plan, TemplateKind::kStaged, plan.predicted_bbox, 4)},
      std::nullopt);
  expect_true(output.accepted && !output.reacquired &&
                  output.template_action == TemplateAction::kNone,
              "first post-commit reliable probation frame does not commit recent");
  plan = controller.begin_frame(5, 200, false);
  output = controller.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent, plan.predicted_bbox, 5),
       reliable_observation(plan, TemplateKind::kStaged, plan.predicted_bbox, 5)},
      std::nullopt);
  expect_true(output.accepted && !output.reacquired &&
                  output.template_action == TemplateAction::kCommitRecent,
              "configured reliable probation frames commit recent exactly once");

  ResolvedControllerFixture rollback_probation(config);
  rollback_probation.initialize(initial, 0, 0, {240, 160});
  reject_frame(rollback_probation, 1, 40);
  for (std::uint64_t frame : {2U, 3U}) {
    plan = rollback_probation.begin_frame(frame, frame * 40, false);
    output = rollback_probation.finish_frame(
        plan,
        {reliable_observation(plan, TemplateKind::kRecent,
                              plan.predicted_bbox, frame)},
        std::nullopt);
  }
  expect_true(output.template_action == TemplateAction::kStageRecent,
              "atomic search recovery stages a probationary recent template");
  plan = rollback_probation.begin_frame(4, 160, false);
  output = rollback_probation.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent,
                            plan.predicted_bbox, 4),
       rejected_observation(plan, TemplateKind::kStaged,
                            plan.predicted_bbox, 4)},
      std::nullopt);
  expect_true(!output.accepted &&
                  output.tracking_phase == TrackingPhase::kSearching &&
                  output.template_action == TemplateAction::kRollbackRecent,
              "recovery probation failure rolls back and resumes search");

  ControllerConfig strict_probation = config;
  strict_probation.detector_confirmation_iou = 0.50F;
  strict_probation.template_agreement_iou = 0.80F;
  ResolvedControllerFixture threshold_split(strict_probation);
  threshold_split.initialize(initial, 0, 0, {240, 160});
  reject_frame(threshold_split, 1, 40);
  for (std::uint64_t frame : {2U, 3U}) {
    plan = threshold_split.begin_frame(frame, frame * 40, false);
    output = threshold_split.finish_frame(
        plan, {reliable_observation(plan, TemplateKind::kRecent, initial, frame)},
        std::nullopt);
  }
  expect_true(output.template_action == TemplateAction::kStageRecent,
              "strict probation fixture stages a candidate after two valid observations");
  plan = threshold_split.begin_frame(4, 160, false);
  BBox recent_shifted = initial;
  BBox staged_shifted = initial;
  recent_shifted.x += 2;
  staged_shifted.x += 6;
  output = threshold_split.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent, recent_shifted, 4),
       reliable_observation(plan, TemplateKind::kStaged, staged_shifted, 4)},
      std::nullopt);
  expect_true(output.template_iou > 0.66F && output.template_iou < 0.67F &&
                  output.accepted && output.trusted_for_state_update &&
                  output.tracking_phase == TrackingPhase::kTracking &&
                  output.selected_template == TemplateKind::kStaged &&
                  output.template_action == TemplateAction::kNone &&
                  threshold_split.current_bbox().x == staged_shifted.x &&
                  threshold_split.last_trusted_timestamp_ms_ == 160,
              "reliable staged recovery survives one divergent recent response");
  plan = threshold_split.begin_frame(5, 200, false);
  expect_true(plan.tracking_phase == TrackingPhase::kTracking &&
                  plan.template_run_mode == TemplateRunMode::kStagedAndRecent &&
                  !plan.recovery_probe,
              "recovery probation keeps recent and staged heads active");
  recent_shifted.x += 8;
  auto staged_dip = reliable_observation(
      plan, TemplateKind::kStaged, plan.predicted_bbox, 5);
  staged_dip.metrics.peak_margin = 0.01F;
  output = threshold_split.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent, recent_shifted, 5),
       staged_dip},
      std::nullopt);
  expect_true(output.tracking_phase == TrackingPhase::kSuspected &&
                  output.selected_template == TemplateKind::kStaged &&
                  output.observation_present && !output.accepted &&
                  output.control_valid,
              "an isolated staged response dip remains tentative");
  expect_true(output.template_action == TemplateAction::kNone,
              "a tentative staged response does not request rollback");
  expect_true(output.template_probation_active,
              "a tentative staged response preserves recovery probation");
  plan = threshold_split.begin_frame(6, 240, false);
  expect_true(plan.template_run_mode == TemplateRunMode::kStagedAndRecent,
              "tentative staged control retains staged mode, actual=" +
                  std::to_string(static_cast<int>(plan.template_run_mode)));
  threshold_split.abort_frame();

  ResolvedControllerFixture off_probe(config);
  off_probe.initialize(initial, 0, 0, {240, 160});
  reject_frame(off_probe, 1, 40);
  plan = off_probe.begin_frame(2, 80, false);
  output = off_probe.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent, initial, 2)},
      std::nullopt);
  expect_true(!output.accepted && output.recovery_confirmation_hits == 1,
              "first independent NanoTrack recovery hit awaits confirmation");
  plan = off_probe.begin_frame(3, 120, false);
  expect_true(plan.recovery_probe.has_value(),
              "first NanoTrack hit becomes the next recovery probe");
  const BBox evolved{43.5F, 27.5F, 25.0F, 25.0F};
  expect_true(!off_probe.recovery_candidates_.geometry_consistent(
                  plan.predicted_bbox, evolved),
              "fixture is outside strict probe geometry");
  output = off_probe.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent, evolved, 3)},
      std::nullopt);
  expect_true(!output.recovery_conflict,
              "temporally associated off-probe hit is not an identity conflict");
  expect_true(output.recovery_confirmation_hits == 2,
              "temporally associated off-probe hit preserves the tracklet");
  expect_true(output.accepted && output.reacquired,
              "second temporally associated NanoTrack hit commits recovery");
  expect_true(std::string(output.recovery_reason) == "RECOVERY_STAGED",
              std::string("off-probe recovery reason, actual=") +
                  output.recovery_reason);

  ResolvedControllerFixture disagreement(config);
  disagreement.initialize(initial, 0, 0, {240, 160});
  reject_frame(disagreement, 1, 40);
  plan = disagreement.begin_frame(2, 80, false);
  output = disagreement.finish_frame(
      plan, {reliable_observation(plan, TemplateKind::kRecent, plan.predicted_bbox, 2)},
      std::nullopt);
  plan = disagreement.begin_frame(3, 120, false);
  output = disagreement.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent,
                            plan.predicted_bbox, 3),
       reliable_observation(plan, TemplateKind::kSafe,
                            BBox{180, 100, 20, 20}, 3)},
      std::nullopt);
  expect_true(!output.accepted && output.control_valid && output.recovery_conflict &&
                  output.template_action == TemplateAction::kNone,
              "reliable recent/safe disagreement keeps both hypotheses out of control");

  ResolvedControllerFixture expired(config);
  expired.initialize(initial, 0, 0, {240, 160});
  reject_frame(expired, 1, 40);
  plan = expired.begin_frame(2, 80, false);
  expired.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent,
                            plan.predicted_bbox, 2)},
      std::nullopt);
  plan = expired.begin_frame(5, 200, false);
  output = expired.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kRecent,
                            plan.predicted_bbox, 5)},
      std::nullopt);
  expect_true(!output.accepted && !output.reacquired,
              "second hit outside confirmation window cannot commit");
}

void test_recovery_probation_cleanup() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.recent_probation_frames = 1;
  config.template_bridge_min_stable_frames = 1;  // Ignored migration fields.
  config.template_bridge_max_duration_ms = 200;
  const BBox initial{40, 30, 20, 20};

  for (int disruption = 0; disruption < 3; ++disruption) {
    ResolvedControllerFixture controller(config);
    controller.initialize(initial, 0, 0, {240, 160});
    controller.finish_frame(controller.begin_frame(1, 40, false), {}, std::nullopt);
    ControllerOutput output;
    for (std::uint64_t frame : {2U, 3U}) {
      const auto plan = controller.begin_frame(frame, frame * 40, false);
      output = controller.finish_frame(
          plan, {reliable_observation(plan, TemplateKind::kRecent, initial, frame)},
          std::nullopt);
    }
    expect_true(output.template_action == TemplateAction::kStageRecent,
                "cleanup fixture staged a recovered observation");
    std::int64_t retry_time = 120 + config.motion_max_dt_ms + 1;
    if (disruption == 1) {
      retry_time = 160;
      const auto plan = controller.begin_frame(4, retry_time, false);
      expect_true(plan.template_run_mode == TemplateRunMode::kStagedAndRecent,
                  "staged and recent are used only while probation is live");
      controller.abort_frame();
    }
    if (disruption == 2) {
      controller.initialize(initial, 0, 0, {240, 160});
      const auto reset = controller.begin_frame(1, 40, false);
      output = controller.finish_frame(
          reset, {reliable_observation(reset, TemplateKind::kRecent, initial, 1)},
          std::nullopt);
      expect_true(reset.template_run_mode == TemplateRunMode::kRecentOnly &&
                      !reset.recovery_probe && output.accepted &&
                      output.template_action == TemplateAction::kNone,
                  "reinitialization cannot resume an earlier recovery probation");
      continue;
    }
    const auto plan = controller.begin_frame(4, retry_time, false);
    expect_true(plan.template_run_mode == TemplateRunMode::kRecentAndSafe &&
                    !plan.recovery_probe,
                "gap or abort cannot replay a staged template or a recovery probe");
    output = controller.finish_frame(
        plan, {reliable_observation(plan, TemplateKind::kRecent, initial, 4)},
        std::nullopt);
    expect_true(!output.accepted && !output.recovery_candidate_key &&
                    output.template_action == TemplateAction::kRollbackRecent,
                "abandoned staging requests one rollback and fresh search");
    const auto fresh = controller.begin_frame(5, retry_time + 40, false);
    output = controller.finish_frame(
        fresh, {reliable_observation(fresh, TemplateKind::kRecent, initial, 5)},
        std::nullopt);
    expect_true(!output.accepted && output.recovery_confirmation_hits == 1 &&
                    output.template_action == TemplateAction::kNone,
                "a fresh search starts without prior verification or repeated rollback");
  }
}

}  // namespace

namespace {

void test_recent_safe_tentative_tracking() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.occlusion_min_reference_samples = 3;
  config.suspicion_enter_ms = 80;
  config.suspicion_clear_ms = 80;
  config.uncertain_ms = 120;
  config.motion_search_confidence_ms = 180;
  config.stable_frames_for_update = 50;
  config.accepted_center_max_correction_ratio = 0.10F;
  const BBox initial{70, 50, 30, 30};
  const BBox shifted{77, 50, 30, 30};
  const ResponseMetrics healthy{0.98F, 0.96F, 20, 30, 0.28F};
  const ResponseMetrics degraded{0.98F, 0.96F, 11, 16, 0.15F};
  const auto track = [&](LongTermController& controller, std::uint64_t id,
                         const BBox& bbox, const ResponseMetrics& metrics) {
    const auto plan = controller.begin_frame(id, id * 40, false);
    auto observation = reliable_observation(plan, TemplateKind::kRecent, bbox);
    observation.metrics = metrics;
    std::vector<TrackerObservation> observations{observation};
    if (plan.template_run_mode == TemplateRunMode::kRecentAndSafe) {
      observations.push_back(rejected_observation(
          plan, TemplateKind::kSafe, initial));
    }
    return controller.finish_frame(plan, observations, std::nullopt);
  };

  ResolvedControllerFixture clears(config);
  ResolvedControllerFixture expires(config);
  ResolvedControllerFixture collapses(config);
  for (auto* controller : {&clears, &expires, &collapses}) {
    controller->initialize(initial, 0, 0, {320, 240});
    for (std::uint64_t id = 1; id <= 3; ++id) {
      const BBox moving{initial.x + static_cast<float>(id), initial.y, 30, 30};
      const auto out = track(*controller, id, moving, healthy);
      expect_true(out.accepted && out.trusted_for_state_update && out.observation_present,
                  "ordinary recent-only observations are direct and trusted");
    }
    const auto trusted = controller->motion_.snapshot(120);
    float previous_control_x = controller->current_bbox().x;
    for (std::uint64_t id = 4; id <= 6; ++id) {
      const auto out = track(*controller, id, shifted, degraded);
      expect_true(out.mode == (id == 6 ? TrackingMode::kSuspected
                                      : TrackingMode::kTracking),
                  "stable-size, high-score response degradation waits for the enter gate");
      expect_true(out.tracking_phase == TrackingPhase::kSuspected &&
                      !out.accepted && !out.trusted_for_state_update &&
                      out.observation_present && out.control_valid &&
                      out.control_source == ControlSource::kObservation &&
                      out.template_action == TemplateAction::kNone &&
                      out.tracking_bbox.x == shifted.x && out.tracking_bbox.width == 30,
                  "tentative output carries the exact recent observation without accepting it");
      expect_true(out.control_bbox.x >= previous_control_x &&
                      out.control_bbox.x <= previous_control_x + 3.001F &&
                      out.control_bbox.x <= shifted.x,
                  "tentative control applies the existing center correction limit per output");
      previous_control_x = out.control_bbox.x;
      const auto saved = controller->motion_.snapshot(120);
      expect_true(saved.bbox.x == trusted.bbox.x &&
                      saved.velocity_x == trusted.velocity_x &&
                      saved.acceleration_x == 0.0F &&
                      controller->stable_frames_ == 3 &&
                      controller->response_baseline_.evaluate(healthy).reference->psr == 20,
                  "tentative frames preserve motion velocity, freeze acceleration, and leave other trusted state untouched");
    }
  }

  for (std::uint64_t id : {7U, 8U, 9U}) {
    const auto out = track(clears, id, shifted, healthy);
    expect_true(out.accepted == (id == 9) &&
                    out.mode == (id == 9 ? TrackingMode::kTracking : TrackingMode::kSuspected),
                "clean recent must meet the clear gate even when safe is weak");
  }
  expect_true(clears.begin_frame(10, 400, false).template_run_mode ==
                  TemplateRunMode::kRecentOnly,
              "clear returns to the single recent head");

  for (std::uint64_t id : {7U, 8U, 9U}) {
    const auto out = track(expires, id, shifted, degraded);
    expect_true(out.control_valid,
                "tentative and search states keep bounded control active");
    if (id == 8) {
      expect_true(out.tracking_phase == TrackingPhase::kSuspected &&
                      out.observation_present &&
                      out.control_source == ControlSource::kObservation,
                  "tentative control keeps publishing and driving from recent observation");
    }
    if (id == 9) {
      expect_true(out.tracking_phase == TrackingPhase::kSearching &&
                      out.motion_search_age_ms == 0 && !out.observation_present,
                  "continuous degradation escalates without renewing control");
    }
  }
  const auto search_plan = expires.begin_frame(10, 400, false);
  expect_true(search_plan.template_run_mode == TemplateRunMode::kRecentAndSafe &&
                  search_plan.motion_control_authorized,
              "search runs recent and safe with bounded motion control");

  const auto suspect_plan = collapses.begin_frame(7, 280, false);
  expect_true(suspect_plan.template_run_mode == TemplateRunMode::kRecentAndSafe,
              "suspected requests safe for diagnostics");
  const auto collapse = collapses.finish_frame(suspect_plan,
      {rejected_observation(suspect_plan, TemplateKind::kRecent, initial),
       reliable_observation(suspect_plan, TemplateKind::kSafe, initial)}, std::nullopt);
  expect_true(collapse.tracking_phase == TrackingPhase::kSearching &&
                  collapse.control_source == ControlSource::kMotionSearch &&
                  !collapse.accepted && !collapse.observation_present,
              "collapsed recent escalates even if safe is strong");
}

void test_recent_safe_single_metric_dip_is_tentative() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.occlusion_min_reference_samples = 3;
  config.suspicion_enter_ms = 80;
  config.suspicion_clear_ms = 80;
  config.uncertain_ms = 120;
  config.stable_frames_for_update = 50;
  ResolvedControllerFixture controller(config);
  const BBox bbox{70, 50, 30, 30};
  const ResponseMetrics healthy{0.98F, 0.96F, 20, 30, 0.28F};
  controller.initialize(bbox, 0, 0, {320, 240});
  for (std::uint64_t id = 1; id <= 3; ++id) {
    const auto plan = controller.begin_frame(id, id * 40, false);
    auto recent = reliable_observation(plan, TemplateKind::kRecent, bbox);
    recent.metrics = healthy;
    expect_true(controller.finish_frame(plan, {recent}, std::nullopt).accepted,
                "healthy recent seeds the trusted response baseline");
  }

  auto plan = controller.begin_frame(4, 160, false);
  auto margin_dip = reliable_observation(plan, TemplateKind::kRecent, bbox);
  margin_dip.metrics = healthy;
  margin_dip.metrics.peak_margin = 0.018F;
  auto output = controller.finish_frame(plan, {margin_dip}, std::nullopt);
  expect_true(output.tracking_phase == TrackingPhase::kSuspected,
              "an isolated peak-margin dip enters SUSPECTED");
  expect_true(output.mode == TrackingMode::kTracking,
              "a pre-entry isolated dip retains tracking mode");
  expect_true(output.observation_present && !output.accepted &&
                  !output.trusted_for_state_update,
              "an isolated dip is visible but cannot update trusted state");
  expect_true(output.control_valid &&
                  output.control_source == ControlSource::kObservation,
              "an isolated dip retains bounded observation control");
  expect_true(std::string(output.state_change_reason) == "RESPONSE_DEGRADED",
              std::string("an isolated dip records response degradation, actual=") +
                  output.state_change_reason);
  expect_true(output.template_action == TemplateAction::kNone,
              "an isolated dip does not update templates");

  plan = controller.begin_frame(5, 200, false);
  auto recovered = reliable_observation(plan, TemplateKind::kRecent, bbox);
  recovered.metrics = healthy;
  output = controller.finish_frame(plan, {recovered}, std::nullopt);
  expect_true(output.tracking_phase == TrackingPhase::kTracking && output.accepted &&
                  output.trusted_for_state_update && output.control_valid,
              "one healthy frame clears an isolated pre-entry response dip");
}

void test_motion_updates_freeze_before_search() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.occlusion_min_reference_samples = 3;
  config.stable_frames_for_update = 50;
  config.suspicion_clear_ms = 100;
  config.motion_alpha = 1.0F;
  config.motion_beta = 0.5F;
  config.motion_max_speed_diagonals_per_s = 10.0F;
  config.motion_max_acceleration_diagonals_per_s2 = 100.0F;
  ResolvedControllerFixture controller(config);
  const ResponseMetrics healthy{0.98F, 0.96F, 20.0F, 30.0F, 0.60F};
  controller.initialize({100, 80, 20, 20}, 0, 0, {400, 300});

  for (std::uint64_t id = 1; id <= 4; ++id) {
    const auto plan = controller.begin_frame(id, id * 40, false);
    auto observation = reliable_observation(
        plan, TemplateKind::kRecent,
        {100.0F - 2.0F * static_cast<float>(id),
         80.0F + 2.0F * static_cast<float>(id), 20, 20});
    observation.metrics = healthy;
    expect_true(controller.finish_frame(plan, {observation}, std::nullopt).accepted,
                "clean left-down observations establish a search trend");
  }
  const MotionSnapshot clean = controller.motion_.snapshot(160);
  expect_true(clean.velocity_x < 0.0F && clean.velocity_y > 0.0F,
              "fixture retains the clean left-down trend");

  for (std::uint64_t id = 5; id <= 7; ++id) {
    const auto plan = controller.begin_frame(id, id * 40, false);
    auto degraded = reliable_observation(
        plan, TemplateKind::kRecent, plan.predicted_bbox);
    degraded.metrics = healthy;
    degraded.metrics.peak_margin = 0.01F;
    const auto tentative =
        controller.finish_frame(plan, {degraded}, std::nullopt);
    expect_true(!tentative.accepted && tentative.observation_present &&
                    tentative.control_valid && !tentative.motion_update_valid &&
                    tentative.tracking_phase == TrackingPhase::kSuspected,
                "degraded observations keep control while motion updates are forbidden");
  }
  expect_true(controller.motion_updates_held_ &&
                  controller.shadow_motion_.has_value(),
              "sustained tentative tracking holds the clean motion state");
  const MotionSnapshot frozen = controller.shadow_motion_->snapshot(280);
  expect_true(frozen.velocity_x < 0.0F && frozen.velocity_y > 0.0F &&
                  frozen.acceleration_x == 0.0F &&
                  frozen.acceleration_y == 0.0F,
              "the pre-search hold preserves velocity and clears acceleration");

  ControllerOutput recovered;
  for (std::uint64_t id = 8; id <= 11; ++id) {
    const auto plan = controller.begin_frame(id, id * 40, false);
    auto clean_observation = reliable_observation(
        plan, TemplateKind::kRecent, plan.predicted_bbox);
    clean_observation.metrics = healthy;
    recovered = controller.finish_frame(
        plan, {clean_observation}, std::nullopt);
  }
  expect_true(recovered.accepted && recovered.motion_update_valid &&
                  !controller.motion_updates_held_ &&
                  !controller.shadow_motion_.has_value(),
              "the existing clean-observation hysteresis releases and reinitializes motion");
}

void test_recent_safe_transient_and_probation() {
  using namespace nanotrack;
  ControllerConfig config;
  config.suspicion_enter_ms = 80;
  config.occlusion_min_reference_samples = 3;
  config.stable_frames_for_update = 4;
  config.recent_update_interval_ms = 1;
  config.recent_probation_frames = 2;
  ResolvedControllerFixture controller(config);
  const BBox bbox{70, 50, 30, 30};
  const ResponseMetrics healthy{0.98F, 0.96F, 20, 30, 0.28F};
  const ResponseMetrics degraded{0.98F, 0.96F, 11, 16, 0.15F};
  const ResponseMetrics stronger{0.98F, 0.96F, 40, 60, 0.40F};
  controller.initialize(bbox, 0, 0, {320, 240});
  const auto track = [&](std::uint64_t id, const ResponseMetrics& metrics) {
    const auto plan = controller.begin_frame(id, id * 40, false);
    auto recent = reliable_observation(plan, TemplateKind::kRecent, bbox);
    recent.metrics = metrics;
    std::vector<TrackerObservation> observations{recent};
    if (id == 6 || id == 7 || id == 11 || id == 12) {
      expect_true(plan.template_run_mode == TemplateRunMode::kStagedAndRecent,
                  "probation requests only staged and recent heads");
      auto staged = recent;
      staged.template_kind = TemplateKind::kStaged;
      observations.push_back(staged);
    }
    return controller.finish_frame(plan, observations, std::nullopt);
  };
  for (std::uint64_t id = 1; id <= 3; ++id) track(id, healthy);
  const auto spike = track(4, degraded);
  expect_true(spike.mode == TrackingMode::kTracking &&
                  spike.tracking_phase == TrackingPhase::kSuspected &&
                  !spike.accepted && spike.template_action == TemplateAction::kNone,
              "an isolated degraded sample freezes learning before the enter duration");
  const auto clears = track(5, healthy);
  expect_true(clears.accepted && clears.mode == TrackingMode::kTracking &&
                  clears.template_action == TemplateAction::kStageRecent,
              "a single clean observation clears the pending entry and can stage");
  expect_true(track(6, stronger).template_action == TemplateAction::kNone &&
                  track(7, stronger).template_action == TemplateAction::kCommitRecent &&
                  controller.response_baseline_.evaluate(healthy).reference->psr == 20,
              "staged probation leaves the trusted response baseline untouched");

  auto multi_metric_collapse = healthy;
  multi_metric_collapse.raw_score = 0.6F;
  multi_metric_collapse.peak_margin = 0.01F;
  const auto searching = track(8, multi_metric_collapse);
  expect_true(searching.tracking_phase == TrackingPhase::kSearching &&
                  searching.mode != TrackingMode::kSuspected,
              "multiple absolute response failures enter search without softening");
  expect_true(!track(9, healthy).accepted && track(10, healthy).accepted &&
                  !controller.response_baseline_.evaluate(healthy).reference,
              "recovery from short search also requires a fresh response baseline");
  expect_true(track(11, healthy).template_action == TemplateAction::kNone &&
                  track(12, healthy).template_action == TemplateAction::kCommitRecent,
              "recovery probation uses staged/recent and commits once after two clean frames");
  track(13, healthy);
  track(14, healthy);
  expect_true(!controller.response_baseline_.evaluate(healthy).reference,
              "recovery and probation hits cannot count as ordinary baseline samples");
  track(15, healthy);
  expect_true(controller.response_baseline_.evaluate(healthy).reference.has_value(),
              "response gating resumes after a fresh ordinary minimum");
}

void test_production_observation_preserves_collapse() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.stable_frames_for_update = 1;
  config.recent_update_interval_ms = 1;
  ResolvedControllerFixture controller(config);
  const FrameSize frame{400, 300};
  const BBox committed{50, 50, 100, 100};
  controller.initialize(committed, 0, 0, frame);
  const auto plan = controller.begin_frame(1, 40, false);
  const auto decoded = reliable_observation(
      plan, TemplateKind::kRecent, {95, 95, 10, 10});
  const auto bounded = constrain_tracker_observation(decoded, committed, frame);
  expect_true(bounded.bbox.width == 75 && bounded.bbox.height == 75 &&
                  bounded.decoded_bbox && bounded.decoded_bbox->width == 10,
              "production geometry preserves pre-clamp collapse alongside bounded bbox");
  const auto out = controller.finish_frame(plan, {bounded}, std::nullopt);
  expect_true(!out.accepted && !out.observation_present &&
                  out.tracking_phase == TrackingPhase::kSearching &&
                  out.template_action == TemplateAction::kNone &&
                  controller.current_bbox().width == 100,
              "production-clamped high-response collapse cannot update bbox or stage");
}

void test_tentative_control_envelopes() {
  using namespace nanotrack;
  ControllerConfig config;
  config.occlusion_min_reference_samples = 3;
  config.stable_frames_for_update = 50;
  config.max_accepted_scale_step_ratio = 0.05F;
  config.motion_scale_rate_limit_per_s = 0.50F;
  config.motion_max_speed_diagonals_per_s = 0.03F;
  config.accepted_center_max_correction_ratio = 0.01F;
  const FrameSize frame{400, 300};
  const BBox initial{0, 50, 100, 100};
  const ResponseMetrics healthy{0.98F, 0.96F, 20, 30, 0.28F};
  const ResponseMetrics degraded{0.98F, 0.96F, 11, 16, 0.15F};
  const auto track = [&](LongTermController& controller, std::uint64_t id,
                         std::int64_t time, BBox bbox, ResponseMetrics metrics) {
    const auto plan = controller.begin_frame(id, time, false);
    auto observation = reliable_observation(plan, TemplateKind::kRecent, bbox);
    observation.metrics = metrics;
    return controller.finish_frame(plan, {observation}, std::nullopt);
  };
  const auto check_step = [&](const BBox& previous, const ControllerOutput& out,
                              float elapsed_s) {
    const auto& next = out.control_bbox;
    expect_true(out.tracking_phase == TrackingPhase::kSuspected && out.control_valid,
                "fixture emits authorized tentative control");
    expect_true(next.x >= 0 && next.y >= 0 && next.x + next.width <= frame.width &&
                    next.y + next.height <= frame.height,
                "the final tentative control box fits in-frame");
    const float scale_limit = std::min(0.05F, 0.50F * elapsed_s);
    expect_true(std::fabs(next.width / previous.width - 1.0F) <= scale_limit + 1e-6F &&
                    std::fabs(next.height / previous.height - 1.0F) <= scale_limit + 1e-6F,
                "consecutive tentative dimensions meet the per-step and elapsed-rate limits");
    const float dx = next.x + next.width / 2 - previous.x - previous.width / 2;
    const float dy = next.y + next.height / 2 - previous.y - previous.height / 2;
    const float center_limit = config.accepted_center_max_correction_ratio *
                               std::sqrt(next.width * next.height);
    const float speed_limit = config.motion_max_speed_diagonals_per_s *
                              std::hypot(frame.width, frame.height) * elapsed_s;
    expect_true(std::hypot(dx, dy) <= std::min(center_limit, speed_limit) + 1e-5F,
                "the final clipped center meets the correction and elapsed-speed limits");
  };
  for (const BBox trusted : {initial, BBox{130, 80, 100, 100}}) {
    ResolvedControllerFixture controller(config);
    controller.initialize(trusted, 0, 0, frame);
    for (std::uint64_t id = 1; id <= 3; ++id) {
      track(controller, id, id * 40, trusted, healthy);
    }
    const BBox first = trusted.x == 0 ? BBox{0, 50, 110, 100}
                                     : BBox{135, 85, 90, 90};
    const BBox second = trusted.x == 0 ? BBox{0, 50, 90, 100}
                                      : BBox{125, 75, 110, 110};
    const auto previous = track(controller, 4, 220, first, degraded);
    check_step(trusted, previous, 0.100F);
    expect_true(trusted.x == 0 ? previous.control_bbox.width > 100
                               : previous.control_bbox.width < 100,
                "the feasible control width moves toward the observed width");
    const auto next = track(controller, 5, 221, second, degraded);
    check_step(previous.control_bbox, next, 0.001F);
    if (trusted.x != 0) {
      expect_true(next.control_bbox.width > previous.control_bbox.width,
                  "the elapsed-time bound permits a small scale reversal");
    }
    expect_true(next.tracking_bbox.width == second.width &&
                    controller.current_bbox().width == 100,
                "the control envelope preserves observation output and trusted geometry");
  }
}

void test_recent_safe_rolling_scale() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_scale_rate_limit_per_s = 0.50F;
  config.max_accepted_scale_step_ratio = 0.08F;
  config.stable_frames_for_update = 99;
  ResolvedControllerFixture controller(config);
  controller.initialize({150, 60, 20, 20}, 0, 0, {180, 120});
  float previous_width = 20.0F;
  for (std::uint64_t id = 1; id <= 14; ++id) {
    const auto plan = controller.begin_frame(id, id * 100, false);
    const float width = 20.0F + static_cast<float>(id);
    const BBox recent{180 - width, 60, width, width};
    const auto out = controller.finish_frame(plan,
        {reliable_observation(plan, TemplateKind::kRecent, recent),
         rejected_observation(plan, TemplateKind::kSafe, plan.predicted_bbox)},
        std::nullopt);
    expect_true(out.accepted && out.selected_template == TemplateKind::kRecent &&
                    out.tracking_bbox.width <= previous_width * 1.05F + 0.001F &&
                    out.tracking_bbox.x + out.tracking_bbox.width <= 180.0F,
                "trusted recent keeps tracking with weak safe under rolling rate and edge limits");
    previous_width = out.tracking_bbox.width;
  }
  expect_true(previous_width > 20.0F * 1.33F,
              "gradual scale grows beyond the permanent initialization envelope");
  auto plan = controller.begin_frame(15, 1500, false);
  const BBox collapsed{164, 74, 3, 3};
  const auto out = controller.finish_frame(plan,
      {reliable_observation(plan, TemplateKind::kRecent, collapsed)}, std::nullopt);
  expect_true(!out.accepted && out.tracking_bbox.width >= previous_width * 0.75F,
              "a high-response scale collapse does not become trusted");

  MotionEstimator motion(config);
  motion.initialize({150, 60, 20, 20}, 0, 180, 120);
  motion.velocity_width_ = -10.0F;
  const auto predicted = motion.predict(100);
  const auto snapshot = motion.snapshot(100);
  expect_near(predicted.width, 19.0F, 1e-6F, "prediction uses the prior scale rate");
  expect_near(snapshot.scale_velocity_width, -9.5F, 1e-6F,
              "scale velocity stays bounded by the newly projected width");
}

}  // namespace

int main() {
  test_recent_safe_tentative_tracking();
  test_recent_safe_single_metric_dip_is_tentative();
  test_motion_updates_freeze_before_search();
  test_recent_safe_transient_and_probation();
  test_recent_safe_rolling_scale();
  test_production_observation_preserves_collapse();
  test_tentative_control_envelopes();
  using namespace nanotrack;
  test_external_observation_and_control_contract();
  test_accepted_scale_deadband_smoothing_and_compatibility();
  test_temporal_scale_window_direction_and_hysteresis();
  test_temporal_scale_persistence_and_state_isolation();
  test_motion_aware_accepted_center_filter();
  test_ordinary_recovery_uses_fixed_center_gate();
  test_loss_freezes_scale_and_recovery_uses_limited_bbox();
  test_target_class_recovery_request_gate();
  test_coast_generation_provenance();
  test_suspicion_event_pulses_are_single_shot();
  test_coast_probe_schedule_and_isolation();
  test_coast_probe_gates_and_recovery();
  test_coast_probe_stale_identity_and_reset();

  test_target_class_gate();
  test_time_based_motion_search_control();
  test_unified_recovery_candidate_cleanup();
  test_subtarget_probe_abort_does_not_advance_cursor();
  test_atomic_search_reacquisition_and_template_probation();
  test_recovery_probation_cleanup();

  const auto expect_enum_name = [](auto value, const char* expected) {
    expect_true(std::string(to_string(value)) == expected,
                std::string("stable coast enum name: ") + expected);
  };
  expect_enum_name(OcclusionReferenceSource::kNone, "NONE");
  expect_enum_name(OcclusionReferenceSource::kPartialStableMedian,
                   "PARTIAL_STABLE_MEDIAN");
  expect_enum_name(OcclusionReferenceSource::kFullStableMedian,
                   "FULL_STABLE_MEDIAN");
  expect_enum_name(OcclusionTriggerReason::kNone, "NONE");
  expect_enum_name(OcclusionTriggerReason::kSevereWidth, "SEVERE_WIDTH");
  expect_enum_name(OcclusionTriggerReason::kSevereHeight, "SEVERE_HEIGHT");
  expect_enum_name(OcclusionTriggerReason::kGradualWidthTemplate,
                   "GRADUAL_WIDTH_TEMPLATE");
  expect_enum_name(OcclusionTriggerReason::kGradualWidthPeak, "GRADUAL_WIDTH_PEAK");
  expect_enum_name(OcclusionTriggerReason::kGradualWidthMotion,
                   "GRADUAL_WIDTH_MOTION");
  expect_enum_name(OcclusionTriggerReason::kGradualHeightTemplate,
                   "GRADUAL_HEIGHT_TEMPLATE");
  expect_enum_name(OcclusionTriggerReason::kGradualHeightPeak,
                   "GRADUAL_HEIGHT_PEAK");
  expect_enum_name(OcclusionTriggerReason::kGradualHeightMotion,
                   "GRADUAL_HEIGHT_MOTION");
  expect_enum_name(OcclusionProbeType::kNone, "NONE");
  expect_enum_name(OcclusionProbeType::kSearch, "SEARCH");
  expect_enum_name(OcclusionProbeType::kConfirm, "CONFIRM");

  SearchPlan coast_plan;
  expect_true(coast_plan.occlusion_coast_age_frames == 0 &&
                  coast_plan.occlusion_probe_type == OcclusionProbeType::kNone &&
                  !coast_plan.motion_control_authorized,
              "unset coast plans do not authorize motion control or probes");
  coast_plan.occlusion_coast_age_frames = 10;
  coast_plan.occlusion_probe_type = OcclusionProbeType::kSearch;
  coast_plan.motion_control_authorized = true;
  const SearchPlan copied_coast_plan = coast_plan;
  expect_true(copied_coast_plan.occlusion_coast_age_frames == 10 &&
                  copied_coast_plan.occlusion_probe_type == OcclusionProbeType::kSearch &&
                  copied_coast_plan.motion_control_authorized,
              "search plan preserves coast age, probe and motion authorization");

  ControllerOutput coast_output;
  expect_true(coast_output.occlusion_reference_source == OcclusionReferenceSource::kNone &&
                  !coast_output.occlusion_suspicion_active &&
                  !coast_output.occlusion_suspicion_started &&
                  !coast_output.occlusion_suspicion_cleared &&
                  coast_output.occlusion_suspicion_frames == 0 &&
                  coast_output.occlusion_clean_frames == 0 &&
                  coast_output.occlusion_width_shrink_frames == 0 &&
                  coast_output.occlusion_height_shrink_frames == 0 &&
                  !coast_output.occlusion_severe_evidence &&
                  !coast_output.occlusion_gradual_evidence &&
                  !coast_output.occlusion_template_evidence &&
                  !coast_output.occlusion_peak_margin_evidence &&
                  !coast_output.occlusion_motion_residual_evidence &&
                  coast_output.occlusion_trigger_reason == OcclusionTriggerReason::kNone &&
                  coast_output.occlusion_coast_age_frames == 0 &&
                  coast_output.occlusion_probe_type == OcclusionProbeType::kNone &&
                  coast_output.occlusion_probe_confirmation_hits == 0 &&
                  coast_output.occlusion_probe_confirmation_required == 0 &&
                  !coast_output.motion_control_authorized,
              "unset coast diagnostics have no evidence or control authority");
  coast_output.occlusion_reference_source = OcclusionReferenceSource::kFullStableMedian;
  coast_output.occlusion_suspicion_active = true;
  coast_output.occlusion_suspicion_started = true;
  coast_output.occlusion_suspicion_cleared = true;
  coast_output.occlusion_suspicion_frames = 4;
  coast_output.occlusion_clean_frames = 1;
  coast_output.occlusion_width_shrink_frames = 2;
  coast_output.occlusion_height_shrink_frames = 3;
  coast_output.occlusion_severe_evidence = true;
  coast_output.occlusion_gradual_evidence = true;
  coast_output.occlusion_template_evidence = true;
  coast_output.occlusion_peak_margin_evidence = true;
  coast_output.occlusion_motion_residual_evidence = true;
  coast_output.occlusion_trigger_reason = OcclusionTriggerReason::kGradualWidthMotion;
  coast_output.occlusion_coast_age_frames = 11;
  coast_output.occlusion_probe_type = OcclusionProbeType::kConfirm;
  coast_output.occlusion_probe_confirmation_hits = 1;
  coast_output.occlusion_probe_confirmation_required = 2;
  coast_output.motion_control_authorized = true;
  const ControllerOutput copied_coast_output = coast_output;
  expect_true(copied_coast_output.occlusion_reference_source == OcclusionReferenceSource::kFullStableMedian &&
                  copied_coast_output.occlusion_suspicion_active &&
                  copied_coast_output.occlusion_suspicion_started &&
                  copied_coast_output.occlusion_suspicion_cleared &&
                  copied_coast_output.occlusion_suspicion_frames == 4 &&
                  copied_coast_output.occlusion_clean_frames == 1 &&
                  copied_coast_output.occlusion_width_shrink_frames == 2 &&
                  copied_coast_output.occlusion_height_shrink_frames == 3 &&
                  copied_coast_output.occlusion_severe_evidence &&
                  copied_coast_output.occlusion_gradual_evidence &&
                  copied_coast_output.occlusion_template_evidence &&
                  copied_coast_output.occlusion_peak_margin_evidence &&
                  copied_coast_output.occlusion_motion_residual_evidence &&
                  copied_coast_output.occlusion_trigger_reason == OcclusionTriggerReason::kGradualWidthMotion &&
                  copied_coast_output.occlusion_coast_age_frames == 11 &&
                  copied_coast_output.occlusion_probe_type == OcclusionProbeType::kConfirm &&
                  copied_coast_output.occlusion_probe_confirmation_hits == 1 &&
                  copied_coast_output.occlusion_probe_confirmation_required == 2 &&
                  copied_coast_output.motion_control_authorized,
              "controller output preserves all explicit coast diagnostic values");

  expect_true(std::string(to_string(TrackingMode::kOcclusionHold)) ==
                  "OCCLUSION_HOLD",
              "occlusion hold has a stable external mode name");
  SearchPlan hold_plan;
  hold_plan.mode = TrackingMode::kOcclusionHold;
  hold_plan.run_tracker = false;
  hold_plan.hold_remaining_frames = 11;
  const SearchPlan copied_hold_plan = hold_plan;
  expect_true(copied_hold_plan.mode == TrackingMode::kOcclusionHold &&
                  !copied_hold_plan.run_tracker &&
                  copied_hold_plan.hold_remaining_frames == 11,
              "search plan carries the explicit hold execution contract");

  ControllerOutput hold_output;
  expect_true(!hold_output.occlusion_hold_triggered &&
                  hold_output.occlusion_hold_remaining_frames == 0 &&
                  !hold_output.occlusion_reference_width &&
                  !hold_output.occlusion_reference_height &&
                  !hold_output.occlusion_width_ratio &&
                  !hold_output.occlusion_height_ratio,
              "unset hold diagnostics remain distinguishable from zero ratios");
  hold_output.occlusion_hold_triggered = true;
  hold_output.occlusion_hold_remaining_frames = 12;
  hold_output.occlusion_reference_width = 20.0F;
  hold_output.occlusion_reference_height = 30.0F;
  hold_output.occlusion_width_ratio = 0.65F;
  hold_output.occlusion_height_ratio = 0.90F;
  const ControllerOutput copied_hold_output = hold_output;
  expect_true(copied_hold_output.occlusion_hold_triggered &&
                  copied_hold_output.occlusion_hold_remaining_frames == 12 &&
                  copied_hold_output.occlusion_reference_width == 20.0F &&
                  copied_hold_output.occlusion_reference_height == 30.0F &&
                  copied_hold_output.occlusion_width_ratio == 0.65F &&
                  copied_hold_output.occlusion_height_ratio == 0.90F,
              "controller output preserves explicit hold diagnostic values");

  DetectorCandidateDiagnostic diagnostic;
  expect_true(!diagnostic.center_ratio && !diagnostic.candidate_id &&
                  !diagnostic.batch_eligible && !diagnostic.position_gate_pass &&
                  !diagnostic.queued && !diagnostic.drawable &&
                  diagnostic.rejection == DetectorCandidateRejection::kNone,
              "candidate diagnostics default to unevaluated and unqueued");
  diagnostic.candidate = DetectorCandidate{BBox{1, 2, 3, 4}, 0.85F, 7};
  diagnostic.center_ratio = 0.12F;
  diagnostic.candidate_id = 42;
  diagnostic.batch_eligible = true;
  diagnostic.position_gate_pass = true;
  diagnostic.queued = true;
  diagnostic.drawable = true;
  for (DetectorCandidateRejection rejection : {
           DetectorCandidateRejection::kNone,
           DetectorCandidateRejection::kInvalid,
           DetectorCandidateRejection::kFrameMismatch,
           DetectorCandidateRejection::kStale,
           DetectorCandidateRejection::kHoldInvalidated,
           DetectorCandidateRejection::kPositionRejected,
           DetectorCandidateRejection::kCapacityRejected}) {
    diagnostic.rejection = rejection;
    const DetectorCandidateDiagnostic copied = diagnostic;
    expect_true(copied.candidate.bbox.x == 1.0F &&
                    copied.candidate.bbox.y == 2.0F &&
                    copied.candidate.bbox.width == 3.0F &&
                    copied.candidate.bbox.height == 4.0F &&
                    copied.candidate.confidence == 0.85F &&
                    copied.candidate.class_id == 7 &&
                    copied.center_ratio == 0.12F && copied.candidate_id == 42 &&
                    copied.batch_eligible && copied.position_gate_pass &&
                    copied.queued && copied.drawable && copied.rejection == rejection,
                "candidate diagnostics retain payload, flags, and rejection reason");
  }

  ControllerConfig config;
  ResolvedControllerFixture boundaries(config);
  boundaries.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_unknown_target_class(TargetClassReason::kNoMatch));
  ControllerOutput output = reject_frame(boundaries, 1, 100);
  expect_true(output.mode == TrackingMode::kUncertain && !output.control_valid,
              "first rejected frame enters uncertain with invalid control");

  SearchPlan plan = boundaries.begin_frame(2, 299, false);
  expect_true(plan.mode == TrackingMode::kUncertain,
              "199 ms loss remains uncertain");
  expect_near(plan.search_scale, 1.5F, 1e-6F, "uncertain search scale");
  boundaries.finish_frame(plan, {}, std::nullopt);

  plan = boundaries.begin_frame(3, 300, false);
  expect_true(plan.mode == TrackingMode::kLocalRecovery,
              "200 ms loss enters local recovery");
  expect_near(plan.search_scale, 2.0F, 1e-6F, "early local search scale");
  boundaries.finish_frame(plan, {}, std::nullopt);

  plan = boundaries.begin_frame(4, 1099, false);
  expect_true(plan.mode == TrackingMode::kLocalRecovery,
              "999 ms loss remains local recovery");
  expect_near(plan.search_scale, 3.0F, 1e-6F, "late local search scale");
  boundaries.finish_frame(plan, {}, std::nullopt);

  plan = boundaries.begin_frame(5, 1100, false);
  expect_true(plan.mode == TrackingMode::kAssistedRecovery,
              "1000 ms loss enters assisted recovery");
  expect_near(plan.search_scale, 4.0F, 1e-6F, "assisted search scale");
  boundaries.finish_frame(plan, {}, std::nullopt);

  plan = boundaries.begin_frame(6, 3099, false);
  expect_true(plan.mode == TrackingMode::kAssistedRecovery,
              "2999 ms loss remains assisted recovery");
  boundaries.finish_frame(plan, {}, std::nullopt);
  plan = boundaries.begin_frame(7, 3100, false);
  expect_true(plan.mode == TrackingMode::kLost,
              "3000 ms loss enters lost");
  expect_near(plan.search_scale, 4.0F, 1e-6F, "lost search remains bounded");
  boundaries.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture normal(config);
  normal.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_unknown_target_class(TargetClassReason::kNoMatch));
  plan = normal.begin_frame(1, 40, false);
  output = normal.finish_frame(
      plan, {reliable_observation(plan, TemplateKind::kAnchor, plan.predicted_bbox)},
      std::nullopt);
  expect_true(output.accepted && output.control_valid &&
                  output.mode == TrackingMode::kTracking,
              "reliable normal tracking commits immediately");

  output = reject_frame(normal, 2, 80);
  expect_true(!output.accepted && output.mode == TrackingMode::kUncertain,
              "rejected observation does not commit");
  plan = normal.begin_frame(3, 120, false);
  output = normal.finish_frame(
      plan, {reliable_observation(plan, TemplateKind::kAnchor, plan.predicted_bbox)},
      std::nullopt);
  expect_true(!output.control_valid && !output.accepted,
              "first recovery candidate remains unconfirmed");
  plan = normal.begin_frame(4, 160, false);
  output = normal.finish_frame(
      plan, {reliable_observation(plan, TemplateKind::kAnchor, plan.predicted_bbox)},
      std::nullopt);
  expect_true(output.control_valid && output.accepted && !output.reacquired,
              "second consistent short-gap candidate resumes tracking without reset");

  ResolvedControllerFixture conflict(config);
  conflict.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_unknown_target_class(TargetClassReason::kNoMatch));
  plan = conflict.begin_frame(1, 40, false);
  const BBox anchor_box = plan.predicted_bbox;
  const BBox recent_box{anchor_box.x + 15.0F, anchor_box.y,
                        anchor_box.width, anchor_box.height};
  output = conflict.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kAnchor, anchor_box, 9),
       reliable_observation(plan, TemplateKind::kRecent, recent_box, 9)},
      std::nullopt);
  expect_true(!output.control_valid && !output.accepted &&
                  output.selected_template == TemplateKind::kAnchor,
              "high-score template conflict prefers anchor but requires confirmation");
  expect_true(output.template_iou < config.template_agreement_iou,
              "template conflict reports low IoU");

  ResolvedControllerFixture recent_only(config);
  recent_only.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_unknown_target_class(TargetClassReason::kNoMatch));
  plan = recent_only.begin_frame(1, 40, false);
  output = recent_only.finish_frame(
      plan,
      {rejected_observation(plan, TemplateKind::kAnchor, plan.predicted_bbox, 10),
       reliable_observation(plan, TemplateKind::kRecent, plan.predicted_bbox, 10)},
      std::nullopt);
  expect_true(!output.control_valid,
              "recent-only reliable response cannot directly control");

  ResolvedControllerFixture detector_controller(config);
  detector_controller.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(1, true));
  reject_frame(detector_controller, 1, 100);
  plan = detector_controller.begin_frame(2, 1100, true);
  expect_true(plan.request_detector,
              "assisted recovery schedules first detector request");
  expect_true(
      plan.detector_request &&
          plan.detector_request->max_candidates ==
          std::numeric_limits<std::size_t>::max(),
      "detector postprocess does not cap by confidence before position gating");
  DetectorBatch mismatched{999, 1100, {{BBox{10, 10, 20, 20}, 0.9F, 1}}};
  output = detector_controller.finish_frame(plan, {}, mismatched);
  expect_true(output.detector_status == DetectorBatchStatus::kFrameMismatch,
              "mismatched detector request frame is rejected");

  plan = detector_controller.begin_frame(3, 1200, true);
  expect_true(!plan.request_detector,
              "detector requests are limited to once per 500 ms");
  detector_controller.finish_frame(plan, {}, std::nullopt);

  ControllerConfig lock_config = config;
  lock_config.detector_candidate_max_center_ratio = 1.0F;
  ResolvedControllerFixture candidate_lock(lock_config);
  candidate_lock.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(candidate_lock, 1, 100);
  plan = candidate_lock.begin_frame(2, 1100, true);
  DetectorBatch lock_batch{
      2, 1100,
      {{BBox{20, 20, 30, 40}, 0.95F, 0},
       {BBox{120, 20, 30, 40}, 0.90F, 1}}};
  output = candidate_lock.finish_frame(plan, {}, lock_batch);
  expect_true(output.detector_candidates_queued == 1 &&
                  output.detector_candidate_diagnostics[1].rejection == DetectorCandidateRejection::kClassMismatch,
              "only matching whole-object candidates queue before verification");

  plan = candidate_lock.begin_frame(3, 1140, false);
  const std::optional<std::uint64_t> locked_id = plan.detector_candidate_id;
  expect_true(locked_id.has_value() &&
                  plan.detector_target_mode == DetectorTargetMode::kFullObject &&
                  plan.detector_candidate_region.has_value(),
              "detector plan exposes candidate identity, mode, and source region");
  expect_near(plan.predicted_bbox.width, 30.0F, 1e-6F,
              "whole-object mode uses detector target width");
  output = candidate_lock.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 31), std::nullopt);
  expect_true(!output.control_valid && output.detector_candidate_locked &&
                  output.detector_confirmation_hits == 1 &&
                  output.detector_confirmation_attempts == 1,
              "first agreeing NanoTrack pair locks but cannot control");

  plan = candidate_lock.begin_frame(4, 1640, true);
  expect_true(plan.detector_candidate_id == locked_id &&
                  plan.detector_candidate_locked &&
                  plan.detector_probe_index == 0 && plan.request_detector,
              "locked candidate survives while a new detector request is submitted");
  DetectorBatch replacement{
      4, 1640, {{BBox{130, 60, 20, 20}, 0.99F, 0}}};
  output = candidate_lock.finish_frame(plan, {}, replacement);
  expect_true(!output.control_valid && output.detector_candidate_locked &&
                  output.detector_confirmation_attempts == 2 &&
                  output.detector_confirmation_hits == 1,
              "one miss in the locked three-frame window remains invalid");

  plan = candidate_lock.begin_frame(5, 1680, false);
  expect_true(plan.detector_candidate_id == locked_id,
              "new batch cannot replace a candidate under confirmation");
  output = candidate_lock.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 32), std::nullopt);
  expect_true(output.control_valid && output.accepted && output.reacquired &&
                  output.accepted_observation_path ==
                      AcceptedObservationPath::kDetectorRecovery &&
                  output.detector_confirmation_hits == 2 &&
                  output.detector_confirmation_attempts == 3,
              "two agreeing hits in three locked frames reacquire");

  ResolvedControllerFixture same_class_ambiguity(lock_config);
  same_class_ambiguity.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(same_class_ambiguity, 1, 100);
  plan = same_class_ambiguity.begin_frame(2, 1100, true);
  DetectorBatch ambiguous_batch{
      2, 1100,
      {{BBox{20, 20, 30, 40}, 0.95F, 0},
       {BBox{50, 20, 30, 40}, 0.90F, 0}}};
  same_class_ambiguity.finish_frame(plan, {}, ambiguous_batch);
  plan = same_class_ambiguity.begin_frame(3, 1140, false);
  expect_true(plan.detector_same_class_candidates == 2 &&
                  plan.detector_same_class_ambiguous,
              "same-class alternatives are exposed as ambiguous");
  output = same_class_ambiguity.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 33), std::nullopt);
  expect_true(!output.control_valid && !output.detector_candidate_locked,
              "same-class alternatives cannot establish a detector lock");
  plan = same_class_ambiguity.begin_frame(4, 1180, false);
  output = same_class_ambiguity.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 34), std::nullopt);
  expect_true(!output.control_valid && !output.reacquired,
              "same-class ambiguity cannot pass two-of-three confirmation");

  ControllerConfig subtarget_config = config;
  subtarget_config.detector_target_mode = DetectorTargetMode::kSubtarget;
  subtarget_config.detector_candidate_max_center_ratio = 1.0F;
  ResolvedControllerFixture subtarget(subtarget_config);
  subtarget.initialize(BBox{40, 30, 20, 10}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(subtarget, 1, 100);
  plan = subtarget.begin_frame(2, 1100, true);
  DetectorBatch region_batch{
      2, 1100, {{BBox{20, 10, 100, 90}, 0.95F, 0}}};
  subtarget.finish_frame(plan, {}, region_batch);
  plan = subtarget.begin_frame(3, 1140, false);
  const std::optional<std::uint64_t> subtarget_id = plan.detector_candidate_id;
  expect_true(plan.detector_target_mode == DetectorTargetMode::kSubtarget &&
                  plan.detector_candidate_region.has_value() &&
                  plan.detector_candidate_region->x == 20.0F &&
                  plan.detector_candidate_region->y == 10.0F &&
                  plan.detector_candidate_region->width == 100.0F &&
                  plan.detector_candidate_region->height == 90.0F &&
                  plan.detector_probe_index == 0,
              "subtarget probe identifies its enclosing detector region");
  expect_near(plan.predicted_bbox.width, 20.0F, 1e-6F,
              "subtarget probe preserves target width");
  expect_near(plan.predicted_bbox.height, 10.0F, 1e-6F,
              "subtarget probe preserves target height");
  expect_near(plan.search_scale, subtarget_config.detector_search_scale, 1e-6F,
              "subtarget probe uses bounded detector search scale");
  subtarget.finish_frame(plan, {}, std::nullopt);
  plan = subtarget.begin_frame(4, 1180, false);
  expect_true(plan.detector_candidate_id == subtarget_id &&
                  plan.detector_probe_index == 1 &&
                  (plan.predicted_bbox.x != 20.0F ||
                   plan.predicted_bbox.y != 10.0F),
              "failed subtarget probe advances within the same candidate");
  subtarget.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture edge_subtarget(subtarget_config);
  edge_subtarget.initialize(BBox{40, 30, 20, 10}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(edge_subtarget, 1, 100);
  plan = edge_subtarget.begin_frame(2, 1100, true);
  DetectorBatch edge_region_batch{
      2, 1100, {{BBox{0, 0, 30, 60}, 0.95F, 0}}};
  edge_subtarget.finish_frame(plan, {}, edge_region_batch);
  for (std::uint64_t frame = 3; frame <= 5; ++frame) {
    plan = edge_subtarget.begin_frame(frame, 1020 + frame * 40, false);
    edge_subtarget.finish_frame(plan, {}, std::nullopt);
  }
  plan = edge_subtarget.begin_frame(6, 1260, false);
  expect_true(plan.detector_probe_index == 3,
              "edge subtarget test reaches the left-side probe");
  expect_near(plan.predicted_bbox.width, 20.0F, 1e-6F,
              "edge subtarget probe preserves width by shifting inside frame");
  expect_near(plan.predicted_bbox.height, 10.0F, 1e-6F,
              "edge subtarget probe preserves height by shifting inside frame");
  expect_true(plan.predicted_bbox.x >= 0.0F && plan.predicted_bbox.y >= 0.0F,
              "edge subtarget probe stays inside frame");
  edge_subtarget.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture opposite_edge_subtarget(subtarget_config);
  opposite_edge_subtarget.initialize(BBox{40, 30, 20, 30}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(opposite_edge_subtarget, 1, 100);
  plan = opposite_edge_subtarget.begin_frame(2, 1100, true);
  DetectorBatch opposite_edge_batch{
      2, 1100, {{BBox{170, 90, 30, 30}, 0.95F, 0}}};
  opposite_edge_subtarget.finish_frame(plan, {}, opposite_edge_batch);
  for (std::uint64_t frame = 3; frame <= 4; ++frame) {
    plan = opposite_edge_subtarget.begin_frame(
        frame, 1020 + static_cast<std::int64_t>(frame) * 40, false);
    opposite_edge_subtarget.finish_frame(plan, {}, std::nullopt);
  }
  plan = opposite_edge_subtarget.begin_frame(5, 1220, false);
  expect_true(plan.detector_probe_index == 2 &&
                  plan.predicted_bbox.y + plan.predicted_bbox.height <= 120.0F,
              "bottom probe shifts inside frame without shrinking geometry");
  expect_near(plan.predicted_bbox.height, 30.0F, 1e-6F,
              "bottom probe preserves subtarget height");
  opposite_edge_subtarget.finish_frame(plan, {}, std::nullopt);
  plan = opposite_edge_subtarget.begin_frame(6, 1260, false);
  opposite_edge_subtarget.finish_frame(plan, {}, std::nullopt);
  plan = opposite_edge_subtarget.begin_frame(7, 1300, false);
  expect_true(plan.detector_probe_index == 4 &&
                  plan.predicted_bbox.x + plan.predicted_bbox.width <= 200.0F,
              "right probe shifts inside frame without shrinking geometry");
  expect_near(plan.predicted_bbox.width, 20.0F, 1e-6F,
              "right probe preserves subtarget width");
  opposite_edge_subtarget.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture strict_initial_geometry(lock_config);
  strict_initial_geometry.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(strict_initial_geometry, 1, 100);
  plan = strict_initial_geometry.begin_frame(2, 1100, true);
  DetectorBatch strict_initial_batch{
      2, 1100, {{BBox{50, 30, 20, 20}, 0.95F, 0}}};
  strict_initial_geometry.finish_frame(plan, {}, strict_initial_batch);
  plan = strict_initial_geometry.begin_frame(3, 1140, false);
  const BBox oversized_first{plan.predicted_bbox.x - 5.0F,
                             plan.predicted_bbox.y - 5.0F, 30.0F, 30.0F};
  output = strict_initial_geometry.finish_frame(
      plan, reliable_pair(plan, oversized_first, 40), std::nullopt);
  expect_true(!output.control_valid && !output.detector_candidate_locked &&
                  output.detector_confirmation_hits == 0,
              "excess geometry drift cannot establish the first detector lock");

  ResolvedControllerFixture strict_geometry(lock_config);
  strict_geometry.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(strict_geometry, 1, 100);
  plan = strict_geometry.begin_frame(2, 1100, true);
  DetectorBatch strict_batch{
      2, 1100, {{BBox{50, 30, 20, 20}, 0.95F, 0}}};
  strict_geometry.finish_frame(plan, {}, strict_batch);
  plan = strict_geometry.begin_frame(3, 1140, false);
  output = strict_geometry.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 41), std::nullopt);
  expect_true(output.detector_candidate_locked && !output.control_valid,
              "strict geometry test establishes one locked hit");
  plan = strict_geometry.begin_frame(4, 1180, false);
  const BBox oversized{plan.predicted_bbox.x - 5.0F,
                       plan.predicted_bbox.y - 5.0F, 30.0F, 30.0F};
  output = strict_geometry.finish_frame(
      plan, reliable_pair(plan, oversized, 42), std::nullopt);
  expect_true(!output.control_valid &&
                  output.detector_confirmation_hits == 1 &&
                  output.detector_confirmation_attempts == 2,
              "excess detector-origin area drift cannot count as confirmation");

  ResolvedControllerFixture detector_identity(config);
  detector_identity.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(detector_identity, 1, 100);
  plan = detector_identity.begin_frame(2, 1100, true);
  expect_true(plan.request_detector,
              "identity test schedules a detector request");
  DetectorBatch wrong_timestamp{
      2, 1099, {{BBox{10, 10, 20, 20}, 0.9F, 0}}};
  output = detector_identity.finish_frame(plan, {}, wrong_timestamp);
  expect_true(output.detector_status == DetectorBatchStatus::kFrameMismatch &&
                  output.detector_candidates_queued == 0,
              "detector result must preserve both frame and timestamp identity");
  plan = detector_controller.begin_frame(4, 1600, true);
  expect_true(plan.request_detector, "detector interval permits next request");
  detector_controller.finish_frame(plan, {}, std::nullopt);
  plan = detector_controller.begin_frame(5, 1800, false);
  DetectorBatch stale{4, 1600, {{BBox{10, 10, 20, 20}, 0.9F, 1}}};
  output = detector_controller.finish_frame(plan, {}, stale);
  expect_true(output.detector_status == DetectorBatchStatus::kStale,
              "detector batch older than 150 ms is rejected");

  plan = detector_controller.begin_frame(6, 2100, true);
  expect_true(plan.request_detector, "later detector request is scheduled");
  DetectorBatch valid{6,
                      2100,
                      {{BBox{10, 30, 20, 20}, 0.7F, 1},
                       {BBox{30, 30, 20, 20}, 0.95F, 1},
                       {BBox{50, 30, 20, 20}, 0.8F, 1},
                       {BBox{70, 30, 20, 20}, 0.99F, 1}}};
  output = detector_controller.finish_frame(plan, {}, valid);
  expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                  output.detector_candidates_queued == 3,
              "valid detector batch is confidence-sorted and capped at three");
  plan = detector_controller.begin_frame(7, 2140, true);
  expect_true(plan.hypothesis_source == HypothesisSource::kDetector,
              "next frame verifies one queued detector hypothesis");
  expect_true(plan.detector_candidate_class_id == std::optional<int>(1) &&
                  plan.detector_candidate_confidence ==
                      std::optional<float>(0.95F),
              "nearest detector hypothesis preserves candidate identity for audit logs");
  expect_near(plan.predicted_bbox.x, 30.0F, 1e-6F,
              "nearer detector candidate outranks a farther high-confidence box");
  detector_controller.finish_frame(plan, {}, std::nullopt);

  const FrameSize gate_frame{200, 120};
  const BBox gate_initial{40, 30, 20, 20};
  const float frame_diagonal = std::hypot(
      static_cast<float>(gate_frame.width),
      static_cast<float>(gate_frame.height));

  ResolvedControllerFixture boundary_gate(config);
  boundary_gate.initialize(gate_initial, 0, 0, gate_frame,
      make_fixed_target_class(0, true));
  reject_frame(boundary_gate, 1, 100);
  plan = boundary_gate.begin_frame(2, 1100, true);
  const BBox exact_boundary{
      gate_initial.x + 0.15F * frame_diagonal,
      gate_initial.y, gate_initial.width, gate_initial.height};
  DetectorBatch exact_boundary_batch{
      2, 1100, {{exact_boundary, 0.70F, 0}}};
  output = boundary_gate.finish_frame(plan, {}, exact_boundary_batch);
  expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                  output.detector_position_valid_count == 1 &&
                  output.detector_position_rejected_count == 0 &&
                  output.detector_candidates_queued == 1,
              "candidate exactly at the 0.15 center-ratio boundary is admitted");
  plan = boundary_gate.begin_frame(3, 1140, false);
  expect_true(plan.hypothesis_source == HypothesisSource::kDetector &&
                  plan.detector_candidate_center_ratio.has_value(),
              "admitted boundary candidate becomes a detector hypothesis");
  expect_near(*plan.detector_candidate_center_ratio, 0.15F, 1e-5F,
              "active detector hypothesis exposes its normalized center distance");
  boundary_gate.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture rejected_gate(config);
  rejected_gate.initialize(gate_initial, 0, 0, gate_frame,
      make_fixed_target_class(0, true));
  reject_frame(rejected_gate, 1, 100);
  plan = rejected_gate.begin_frame(2, 1100, true);
  const BBox just_over_boundary{
      gate_initial.x + 0.15001F * frame_diagonal,
      gate_initial.y, gate_initial.width, gate_initial.height};
  DetectorBatch rejected_batch{
      2, 1100, {{just_over_boundary, 0.99F, 0}}};
  output = rejected_gate.finish_frame(plan, {}, rejected_batch);
  expect_true(output.detector_status ==
                      DetectorBatchStatus::kPositionRejected &&
                  output.detector_position_valid_count == 0 &&
                  output.detector_position_rejected_count == 1 &&
                  output.detector_candidates_queued == 0 &&
                  !output.accepted && !output.reacquired &&
                  !output.control_valid,
              "otherwise-valid candidate just over the hard gate is rejected safely");
  expect_true(std::string(to_string(output.detector_status)) ==
                  "POSITION_REJECTED",
              "position-only rejection has a distinct auditable status");
  plan = rejected_gate.begin_frame(3, 1140, false);
  expect_true(plan.hypothesis_source == HypothesisSource::kMotion &&
                  !plan.detector_candidate_id.has_value(),
              "position-rejected batch cannot create a later detector hypothesis");
  rejected_gate.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture mixed_gate(config);
  mixed_gate.initialize(gate_initial, 0, 0, gate_frame,
      make_fixed_target_class(0, true));
  reject_frame(mixed_gate, 1, 100);
  plan = mixed_gate.begin_frame(2, 1100, true);
  DetectorBatch mixed_batch{
      2, 1100,
      {{BBox{50, 30, 20, 20}, 0.70F, 0},
       {BBox{70, 30, 20, 20}, 0.99F, 0},
       {BBox{120, 30, 20, 20}, 1.00F, 0}}};
  output = mixed_gate.finish_frame(plan, {}, mixed_batch);
  expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                  output.detector_position_valid_count == 2 &&
                  output.detector_position_rejected_count == 1 &&
                  output.detector_candidates_queued == 2,
              "mixed batch reports pre-cap position admission and rejection counts");
  plan = mixed_gate.begin_frame(3, 1140, false);
  expect_near(plan.predicted_bbox.x, 50.0F, 1e-6F,
              "near lower-confidence candidate is verified before farther candidate");
  expect_true(plan.detector_candidate_confidence ==
                  std::optional<float>(0.70F),
              "distance ordering takes priority over detector confidence");
  mixed_gate.finish_frame(plan, {}, std::nullopt);

  ResolvedControllerFixture lost_gate(config);
  lost_gate.initialize(gate_initial, 0, 0, gate_frame,
      make_fixed_target_class(0, true));
  reject_frame(lost_gate, 1, 100);
  plan = lost_gate.begin_frame(2, 3100, true);
  expect_true(plan.mode == TrackingMode::kLost,
              "strict gate test reaches LOST without relaxing policy");
  DetectorBatch lost_far_batch{
      2, 3100, {{BBox{120, 30, 20, 20}, 0.99F, 0}}};
  output = lost_gate.finish_frame(plan, {}, lost_far_batch);
  expect_true(output.detector_status ==
                      DetectorBatchStatus::kPositionRejected &&
                  output.detector_position_rejected_count == 1 &&
                  !output.control_valid,
              "LOST uses the same strict position gate and cannot authorize control");

  ControllerConfig template_config = config;
  ResolvedControllerFixture templates(template_config);
  templates.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_unknown_target_class(TargetClassReason::kNoMatch));
  for (int index = 1; index <= 15; ++index) {
    plan = templates.begin_frame(static_cast<std::uint64_t>(index), index * 100,
                                 false);
    output = templates.finish_frame(
        plan, {reliable_observation(plan, TemplateKind::kAnchor,
                                    plan.predicted_bbox)},
        std::nullopt);
  }
  expect_true(output.template_action == TemplateAction::kStageRecent,
              "15 stable frames and one second stage recent template");
  for (int index = 16; index <= 18; ++index) {
    plan = templates.begin_frame(static_cast<std::uint64_t>(index), index * 100,
                                 false);
    output = templates.finish_frame(
        plan,
        {reliable_observation(plan, TemplateKind::kAnchor,
                              plan.predicted_bbox, index),
         reliable_observation(plan, TemplateKind::kRecent,
                              plan.predicted_bbox, index)},
        std::nullopt);
  }
  expect_true(output.template_action == TemplateAction::kCommitRecent,
              "three reliable probation frames commit recent template");

  ResolvedControllerFixture rollback(config);
  rollback.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_unknown_target_class(TargetClassReason::kNoMatch));
  for (int index = 1; index <= 15; ++index) {
    plan = rollback.begin_frame(static_cast<std::uint64_t>(index), index * 100,
                                false);
    output = rollback.finish_frame(
        plan, {reliable_observation(plan, TemplateKind::kAnchor,
                                    plan.predicted_bbox)},
        std::nullopt);
  }
  output = reject_frame(rollback, 16, 1600);
  expect_true(output.template_action == TemplateAction::kRollbackRecent,
              "loss during probation rolls recent template back");

  ResolvedControllerFixture degraded_recent(config);
  degraded_recent.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_unknown_target_class(TargetClassReason::kNoMatch));
  for (int index = 1; index <= 15; ++index) {
    plan = degraded_recent.begin_frame(static_cast<std::uint64_t>(index),
                                       index * 100, false);
    output = degraded_recent.finish_frame(
        plan, {reliable_observation(plan, TemplateKind::kAnchor,
                                    plan.predicted_bbox)},
        std::nullopt);
  }
  plan = degraded_recent.begin_frame(16, 1600, false);
  output = degraded_recent.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kAnchor, plan.predicted_bbox, 16),
       rejected_observation(plan, TemplateKind::kRecent, plan.predicted_bbox, 16)},
      std::nullopt);
  expect_true(output.template_action == TemplateAction::kRollbackRecent,
              "probation rolls back when staged recent degrades despite reliable anchor");

  ResolvedControllerFixture bounded(config);
  bounded.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_unknown_target_class(TargetClassReason::kNoMatch));
  reject_frame(bounded, 1, 40);
  for (int index = 2; index <= 10001; ++index) {
    plan = bounded.begin_frame(static_cast<std::uint64_t>(index), index * 40,
                               false);
    output = bounded.finish_frame(plan, {}, std::nullopt);
  }
  expect_true(output.mode == TrackingMode::kLost && !output.control_valid &&
                  output.detector_candidates_queued == 0,
              "long detector-free loss remains bounded and invalid");

  // A correction-before-evidence bug would move the coast center/velocity,
  // commit a template update, and accept the collapsed visual box.
  ControllerConfig coast_config = config;
  coast_config.stable_frames_for_update = 9;
  coast_config.recent_update_interval_ms = 1;
  const BBox stable{60, 40, 20, 20};
  const FrameSize coast_frame{240, 160};
  const auto seed = [&](LongTermController& controller, int count = 8) {
    controller.initialize(stable, 0, 0, coast_frame,
                          make_fixed_target_class(0, true));
    for (int frame = 1; frame <= count; ++frame) {
      const SearchPlan tick = controller.begin_frame(frame, frame * 40, false);
      expect_true(controller.finish_frame(
          tick, reliable_pair(tick, stable, frame), std::nullopt).accepted,
          "stable observations establish the target");
    }
  };
  const auto collapse = [&](LongTermController& controller, int frame,
                            std::int64_t timestamp) {
    const SearchPlan tick = controller.begin_frame(frame, timestamp, true);
    return controller.finish_frame(
        tick, reliable_pair(tick, BBox{66, 40, 14, 20}, frame), std::nullopt);
  };

  ResolvedControllerFixture direct(coast_config);
  seed(direct);
  plan = direct.begin_frame(9, 360, true);
  MotionEstimator untouched = direct.motion_;
  const BBox predicted_before_trigger = direct.motion_.current_bbox();
  const BBox accepted_before_trigger = direct.current_bbox();
  const auto generation_before = direct.detector_eligibility_generation_;
  output = direct.finish_frame(
      plan, reliable_pair(plan, BBox{66, 40, 14, 20}, 9), std::nullopt);
  expect_true(plan.mode == TrackingMode::kTracking &&
                  output.mode == TrackingMode::kOcclusionHold &&
                  output.quality == QualityLevel::kReliable &&
                  output.occlusion_hold_triggered &&
                  output.occlusion_trigger_reason == OcclusionTriggerReason::kSevereWidth &&
                  output.occlusion_coast_age_frames == 1 &&
                  output.occlusion_hold_remaining_frames == 12 &&
                  !output.accepted && output.control_valid &&
                  output.motion_control_authorized &&
                  output.hypothesis_source == HypothesisSource::kMotion,
              "RELIABLE severe width triggers directly and authorizes coast age one");
  expect_near(output.bbox.x, predicted_before_trigger.x, 1e-5F,
              "trigger uses the motion center rather than the collapsed observation");
  expect_near(output.bbox.width, 20, 1e-5F, "trigger freezes the stable width");
  expect_near(direct.current_bbox().x, accepted_before_trigger.x, 1e-5F,
              "trigger never commits the abnormal tracker bbox");
  expect_true(output.template_action == TemplateAction::kNone &&
                  !direct.recent_probation_ &&
                  direct.detector_eligibility_generation_ == generation_before + 1 &&
                  direct.confirmations_.empty() &&
                  direct.detector_candidates_.empty() &&
                  !direct.active_detector_candidate_ &&
                  direct.loss_start_ms_ == std::optional<std::int64_t>(360),
              "trigger precedes template effects and clears loss eligibility");
  untouched.freeze_scale(20, 20);
  const BBox next_expected = untouched.predict(400);
  plan = direct.begin_frame(10, 400, true);
  expect_near(plan.predicted_bbox.x, next_expected.x, 1e-5F,
              "trigger does not correct motion position or velocity");
  direct.finish_frame(plan, {}, std::nullopt);

  // A staged template must not be committed or rolled back by a trigger.
  ResolvedControllerFixture probation(coast_config);
  seed(probation);
  plan = probation.begin_frame(9, 360, false);
  output = probation.finish_frame(plan, reliable_pair(plan, stable, 9), std::nullopt);
  expect_true(output.template_action == TemplateAction::kStageRecent,
              "fixture has a staged recent template");
  output = collapse(probation, 10, 400);
  expect_true(output.occlusion_hold_triggered &&
                  output.template_action == TemplateAction::kNone &&
                  probation.recent_probation_ && probation.probation_reliable_frames_ == 0,
              "trigger cannot commit or roll back a staged template");

  for (bool start_uncertain : {false, true}) {
    ResolvedControllerFixture gradual(coast_config);
    seed(gradual);
    int frame = 9;
    if (start_uncertain) {
      plan = gradual.begin_frame(frame, frame * 40, true);
      output = gradual.finish_frame(
          plan, {rejected_observation(plan, TemplateKind::kAnchor, stable)}, std::nullopt);
      expect_true(output.mode == TrackingMode::kUncertain && !output.control_valid,
                  "ordinary one-frame quality loss enters uncertain");
      ++frame;
    }
    plan = gradual.begin_frame(frame, frame * 40, true);
    expect_true(plan.run_tracker && !plan.request_detector,
                "tracking and uncertain continue per-frame NanoTrack without detector");
    auto pair = reliable_pair(plan, BBox{61, 40, 18, 20}, frame);
    for (auto& observation : pair) observation.metrics.peak_margin = 0.16F;
    output = gradual.finish_frame(plan, pair, std::nullopt);
    expect_true(!output.occlusion_hold_triggered &&
                    output.occlusion_suspicion_active &&
                    output.occlusion_reference_width == std::optional<float>(20),
                "first shrink freezes the stable reference before accepting smaller scale");
    ++frame;
    plan = gradual.begin_frame(frame, frame * 40, true);
    pair = reliable_pair(plan, BBox{62, 40, 16, 20}, frame);
    for (auto& observation : pair) observation.metrics.peak_margin = 0.16F;
    output = gradual.finish_frame(plan, pair, std::nullopt);
    expect_true(output.occlusion_hold_triggered &&
                    output.occlusion_trigger_reason == OcclusionTriggerReason::kGradualWidthPeak &&
                    output.occlusion_width_shrink_frames == 2 &&
                    output.occlusion_reference_width == std::optional<float>(20) &&
                    output.bbox.width == 20 && output.control_valid,
                "two shrinking RELIABLE frames with peak drop trigger from tracking or uncertain");
  }

  // A missing frame must not join shrinking observations into one consecutive
  // run, but two real adjacent shrink steps after the gap may still trigger.
  ResolvedControllerFixture interrupted_shrink(coast_config);
  seed(interrupted_shrink);
  plan = interrupted_shrink.begin_frame(9, 360, false);
  interrupted_shrink.finish_frame(
      plan, reliable_pair(plan, BBox{61, 40, 18, 20}, 9), std::nullopt);
  output = reject_frame(interrupted_shrink, 10, 400);
  expect_true(output.occlusion_suspicion_active &&
                  output.occlusion_width_shrink_frames == 0 &&
                  output.occlusion_height_shrink_frames == 0 &&
                  output.occlusion_clean_frames == 0 &&
                  output.occlusion_reference_width == std::optional<float>(20) &&
                  !output.occlusion_width_ratio && !output.occlusion_height_ratio &&
                  !output.occlusion_severe_evidence && !output.occlusion_gradual_evidence,
              "empty frame preserves frozen reference but interrupts evidence continuity");
  plan = interrupted_shrink.begin_frame(11, 440, false);
  auto after_gap = reliable_pair(plan, BBox{62, 40, 16, 20}, 11);
  for (auto& observation : after_gap) observation.metrics.peak_margin = 0.16F;
  output = interrupted_shrink.finish_frame(plan, after_gap, std::nullopt);
  expect_true(!output.occlusion_hold_triggered && !output.control_valid &&
                  output.occlusion_width_shrink_frames == 0 &&
                  output.occlusion_peak_margin_evidence,
              "width18 empty width16 cannot trigger a nonconsecutive gradual coast");
  for (int frame : {12, 13}) {
    plan = interrupted_shrink.begin_frame(frame, frame * 40, false);
    const BBox shrinking = frame == 12 ? BBox{62.5F, 40, 15, 20}
                                      : BBox{62.8F, 40, 14.4F, 20};
    output = interrupted_shrink.finish_frame(
        plan, {weak_observation(plan, TemplateKind::kAnchor, shrinking)}, std::nullopt);
    expect_true(output.occlusion_hold_triggered == (frame == 13) &&
                    output.occlusion_width_shrink_frames ==
                        static_cast<std::size_t>(frame - 11) &&
                    output.occlusion_reference_width == std::optional<float>(20),
                "two adjacent shrink steps after the new baseline can trigger using frozen reference");
  }

  // Geometry disagreement is evidence even when the second template is weak;
  // a lone recent template does not constitute anchor/recent disagreement.
  for (bool weak_disagreeing_recent : {false, true}) {
    ResolvedControllerFixture template_evidence(coast_config);
    seed(template_evidence);
    for (int frame : {9, 10}) {
      plan = template_evidence.begin_frame(frame, frame * 40, false);
      const BBox shrinking = frame == 9 ? BBox{61, 40, 18, 20}
                                       : BBox{62, 40, 16, 20};
      std::vector<TrackerObservation> observations;
      if (weak_disagreeing_recent) {
        observations = {reliable_observation(plan, TemplateKind::kAnchor, shrinking),
                        weak_observation(plan, TemplateKind::kRecent, BBox{90, 40, 20, 20})};
      } else {
        observations = {reliable_observation(plan, TemplateKind::kRecent, shrinking)};
      }
      output = template_evidence.finish_frame(plan, observations, std::nullopt);
    }
    expect_true(output.occlusion_template_evidence == weak_disagreeing_recent &&
                    output.occlusion_hold_triggered == weak_disagreeing_recent,
                "template evidence requires actual anchor/recent bbox disagreement");
  }

  for (int sample_count : {2, 3, 7, 8}) {
    ResolvedControllerFixture partial(coast_config);
    seed(partial, sample_count);
    output = collapse(partial, sample_count + 1, (sample_count + 1) * 40);
    expect_true(output.occlusion_hold_triggered == (sample_count >= 3),
                "severe trigger needs at least three stable samples");
    if (sample_count >= 3) {
      expect_true(output.occlusion_reference_source ==
                      (sample_count == 8 ? OcclusionReferenceSource::kFullStableMedian
                                         : OcclusionReferenceSource::kPartialStableMedian),
                  "trigger publishes partial or full reference provenance");
    }
  }

  // Frame count governs coast; the resumed state is independently time-driven.
  const std::int64_t irregular_offsets[] =
      {0, 1, 7, 100, 101, 1000, 1001, 1020, 5000, 5100, 9000, 9001, 12000};
  for (std::int64_t gap : {1, 40, 1000}) {
    ResolvedControllerFixture bounded_coast(coast_config);
    seed(bounded_coast);
    output = collapse(bounded_coast, 9, 360);
    int valid_ages = output.control_valid ? 1 : 0;
    for (int age = 2; age <= 12; ++age) {
      const std::int64_t timestamp = 360 +
          (gap == 1000 ? irregular_offsets[age - 1] : (age - 1) * gap);
      plan = bounded_coast.begin_frame(8 + age, timestamp, true);
      expect_true(plan.mode == TrackingMode::kOcclusionHold &&
                      plan.run_tracker == (age == 5 || age == 10) && !plan.request_detector &&
                      !plan.detector_request && plan.motion_control_authorized &&
                      plan.occlusion_coast_age_frames == static_cast<std::size_t>(age) &&
                      plan.hold_remaining_frames == static_cast<std::size_t>(13 - age),
                  "ages two through twelve plan frozen motion with periodic probes and no detector");
      DetectorBatch old_batch{1, 40, {{stable, 0.99F, 0}}, 0};
      output = bounded_coast.finish_frame(
          plan, {reliable_observation(plan, TemplateKind::kAnchor,
                                      BBox{66, 40, 14, 20}, 8 + age)}, old_batch);
      valid_ages += output.control_valid ? 1 : 0;
      expect_true(output.control_valid && output.motion_control_authorized &&
                      !output.accepted && output.bbox.width == 20 &&
                      output.bbox.height == 20 &&
                      output.occlusion_coast_age_frames == static_cast<std::size_t>(age) &&
                      output.template_action == TemplateAction::kNone &&
                      output.detector_candidates_queued == 0,
                  "single-template observations and stale detector boxes cannot replace coast control");
    }
    expect_true(valid_ages == 12, "inclusive coast authorizes exactly twelve frames");
    plan = bounded_coast.begin_frame(
        21, 360 + (gap == 1000 ? irregular_offsets[12] : 12 * gap), true);
    const TrackingMode expected = gap == 1 ? TrackingMode::kUncertain
        : gap == 40 ? TrackingMode::kLocalRecovery : TrackingMode::kLost;
    expect_true(plan.mode == expected && plan.run_tracker &&
                    !plan.motion_control_authorized &&
                    plan.occlusion_coast_age_frames == 0 &&
                    plan.predicted_bbox.width == 20 && plan.predicted_bbox.height == 20,
                "age thirteen resumes the elapsed-time mode with frozen search scale");
    output = bounded_coast.finish_frame(plan, {}, std::nullopt);
    expect_true(!output.control_valid && !output.motion_control_authorized,
                "age thirteen revokes coast even with detector work enabled");
  }

  // A trigger clears already queued hypotheses and pending NanoTrack confirmation.
  ResolvedControllerFixture pending(coast_config);
  seed(pending);
  pending.confirmations_.push_back({8, stable});
  pending.detector_candidates_.push_back({1, {stable, 0.99F, 0}, 0, {stable}, 1});
  output = collapse(pending, 9, 360);
  expect_true(output.occlusion_hold_triggered && pending.confirmations_.empty() &&
                  pending.detector_candidates_.empty(),
              "coast trigger clears pending confirmations and detector candidates");

  // Explicit reselect works with an open trigger frame, mid coast and pending loss.
  for (int reset_age : {0, 1, 6, 13}) {
    ResolvedControllerFixture reset(coast_config);
    seed(reset);
    if (reset_age == 0) {
      reset.begin_frame(9, 360, false);
    } else {
      collapse(reset, 9, 360);
      for (int age = 2; age <= reset_age; ++age)
        reject_frame(reset, 8 + age, 360 + (age - 1) * 40);
    }
    reset.initialize(BBox{20, 20, 30, 30}, 50, 5000, coast_frame,
                     make_fixed_target_class(0, true));
    expect_true(!reset.loss_start_ms_ && !reset.motion_.scale_frozen() &&
                    reset.confirmations_.empty() && reset.detector_candidates_.empty(),
                "reselect clears loss, frozen scale and pending recovery");
    plan = reset.begin_frame(51, 5040, true);
    output = reset.finish_frame(plan, reliable_pair(plan, BBox{26, 20, 18, 30}, 51),
                                std::nullopt);
    expect_true(plan.mode == TrackingMode::kTracking &&
                    plan.occlusion_coast_age_frames == 0 &&
                    !plan.motion_control_authorized &&
                    !output.occlusion_hold_triggered && !output.occlusion_reference_width,
                "reselect discards old evidence and coast authorization");
  }

  ControllerConfig same_frame_config = coast_config;
  same_frame_config.occlusion_hold_frames = 1;
  ResolvedControllerFixture same_frame_batch(same_frame_config);
  same_frame_batch.initialize(stable, 0, 0, coast_frame,
                              make_fixed_target_class(0, true));
  reject_frame(same_frame_batch, 1, 40);
  plan = same_frame_batch.begin_frame(2, 1100, true);
  const DetectorRequest previous_request = *plan.detector_request;
  same_frame_batch.finish_frame(plan, {}, std::nullopt);
  for (int frame = 3; frame <= 11; ++frame) {
    plan = same_frame_batch.begin_frame(frame, 1176 + frame, false);
    same_frame_batch.finish_frame(plan, reliable_pair(plan, stable, frame), std::nullopt);
  }
  plan = same_frame_batch.begin_frame(12, 1188, false);
  DetectorBatch late_batch{previous_request.frame_id,
                          previous_request.timestamp_ms,
                          {{stable, 0.99F, 0}}, previous_request.generation};
  output = same_frame_batch.finish_frame(
      plan, reliable_pair(plan, BBox{66, 40, 14, 20}, 12), late_batch);
  expect_true(output.occlusion_hold_triggered && output.control_valid &&
                  output.detector_status == DetectorBatchStatus::kAccepted &&
                  output.detector_candidates_queued == 0 &&
                  output.detector_candidate_diagnostics.size() == 1 &&
                  !output.detector_candidate_diagnostics[0].queued &&
                  output.detector_candidate_diagnostics[0].rejection ==
                      DetectorCandidateRejection::kHoldInvalidated,
              "coast invalidates a still-fresh candidate ingested on its trigger frame");
  plan = same_frame_batch.begin_frame(13, 5000, true);
  expect_true(plan.hypothesis_source == HypothesisSource::kMotion &&
                  plan.detector_request->generation == previous_request.generation + 1,
              "post-coast detector requests use the incremented generation");
  late_batch.source_frame_id = plan.frame_id;
  late_batch.source_timestamp_ms = plan.timestamp_ms;
  output = same_frame_batch.finish_frame(plan, {}, late_batch);
  expect_true(!output.control_valid && output.detector_candidates_queued == 0 &&
                  output.detector_candidate_diagnostics[0].rejection ==
                      DetectorCandidateRejection::kHoldInvalidated,
              "pre-coast generations remain ineligible after the coast budget expires");

  ResolvedControllerFixture abandoned(coast_config);
  seed(abandoned);
  collapse(abandoned, 9, 360);
  abandoned.begin_frame(10, 400, false);
  abandoned.abort_frame();
  plan = abandoned.begin_frame(11, 440, false);
  expect_true(plan.mode == TrackingMode::kUncertain &&
                  !plan.motion_control_authorized && plan.run_tracker,
              "aborting an unfinished coast frame immediately ends its authorization");
  abandoned.finish_frame(plan, {}, std::nullopt);

  // Finite out-of-frame predictions are clipped before control is authorized.
  ResolvedControllerFixture clipped(coast_config);
  seed(clipped);
  collapse(clipped, 9, 360);
  clipped.motion_.velocity_x_ = 100000;
  plan = clipped.begin_frame(10, 400, true);
  output = clipped.finish_frame(plan, {}, std::nullopt);
  expect_true(output.control_valid && output.bbox.x >= 0 &&
                  output.bbox.x + output.bbox.width <= coast_frame.width &&
                  output.bbox.width == 20,
              "finite off-frame motion is safely clipped at frozen scale");

  for (float bad_velocity : {std::numeric_limits<float>::quiet_NaN(),
                             std::numeric_limits<float>::infinity()}) {
    ResolvedControllerFixture invalid_motion(coast_config);
    seed(invalid_motion);
    collapse(invalid_motion, 9, 360);
    invalid_motion.motion_.velocity_x_ = bad_velocity;
    bool threw = false;
    bool produced_output = false;
    try {
      plan = invalid_motion.begin_frame(10, 400, true);
      invalid_motion.finish_frame(plan, {}, std::nullopt);
      produced_output = true;
    } catch (const std::runtime_error&) {
      threw = true;
    }
    expect_true(threw && !produced_output && !invalid_motion.frame_open_ &&
                    invalid_motion.hold_remaining_frames_ == 0,
                "non-finite motion aborts the frame with no output and revokes coast");
    invalid_motion.initialize(stable, 20, 800, coast_frame);
    output = reject_frame(invalid_motion, 21, 840);
    expect_true(!output.control_valid,
                "motion exception cannot leave control authority after reinitialization");
  }

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "long-term controller tests passed\n";
  return 0;
}
