#include "controller_config.hpp"
#include "long_term_types.hpp"
#include "target_class.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#define private public
#include "long_term_controller.hpp"
#undef private
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
  LongTermController controller(config);
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
  LongTermController coast(coast_config);
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
  LongTermController controller(config);
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

  LongTermController rounded_boundary(config);
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
  LongTermController rounded_shrink(config);
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

  LongTermController growth(config);
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

  LongTermController shrink(config);
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

  LongTermController edge(config);
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
  LongTermController unrestricted(config);
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
  LongTermController delayed(config);
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

  LongTermController alternating(config);
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
  LongTermController just_inside(boundary);
  just_inside.initialize(BBox{100, 80, 149, 80}, 0, 0, {600, 400});
  output = accept_scale_frame(just_inside, 1, BBox{24.5F, 80, 300, 80});
  expect_near(output.bbox.width, 149.0F, 1e-5F,
              "filtered movement just inside 0.75 pixels holds emitted size");

  LongTermController exact_boundary(boundary);
  exact_boundary.initialize(BBox{100, 80, 150, 80}, 0, 0, {600, 400});
  output = accept_scale_frame(exact_boundary, 1, BBox{25, 80, 300, 80});
  expect_near(output.bbox.width, 151.0F, 1e-5F,
              "exact 0.75-pixel boundary advances one emitted pixel");

  ControllerConfig zero_hysteresis = config;
  zero_hysteresis.accepted_scale_output_hysteresis_px = 0.0F;
  LongTermController monotonic(zero_hysteresis);
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
  LongTermController sustained(config);
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

  LongTermController frozen(config);
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

  LongTermController aborted(config);
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

  LongTermController recovery(config);
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
  LongTermController direct(compatibility);
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
  LongTermController direct(compatibility);
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

  LongTermController alternating(center_filter_config());
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

  LongTermController exact_limit(center_filter_config());
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

  LongTermController over_limit(center_filter_config());
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

  LongTermController diagonal(center_filter_config());
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

  LongTermController moving(center_filter_config());
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

  LongTermController tiny(center_filter_config());
  tiny.initialize(BBox{50, 50, 1, 1}, 0, 0, {100, 100});
  plan = tiny.begin_frame(1, 40, false);
  output = tiny.finish_frame(
      plan, reliable_pair(plan, BBox{51, 51, 1, 1}, 60), std::nullopt);
  expect_true(std::isfinite(output.bbox.x) && std::isfinite(output.bbox.y) &&
                  output.bbox.width == 1.0F && output.bbox.height == 1.0F,
              "one-pixel target produces a finite positive bounded correction");

  LongTermController edge(center_filter_config());
  edge.initialize(BBox{90, 90, 10, 10}, 0, 0, {100, 100});
  plan = edge.begin_frame(1, 40, false);
  output = edge.finish_frame(
      plan, reliable_pair(plan, BBox{0, 0, 10, 10}, 61), std::nullopt);
  expect_true(output.bbox.x >= 0.0F && output.bbox.y >= 0.0F &&
                  output.bbox.x + output.bbox.width <= 100.0F &&
                  output.bbox.y + output.bbox.height <= 100.0F,
              "center state estimation remains frame-safe near an image edge");

  LongTermController isolated(center_filter_config());
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

  LongTermController short_confirmation(center_filter_config());
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

  LongTermController recovery(center_filter_config());
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
  LongTermController controller(config);
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

  LongTermController template_conflict(config);
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
  LongTermController compatibility(config);
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
  LongTermController controller(config);
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
  LongTermController controller{ControllerConfig{}};
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

  LongTermController clean{ControllerConfig{}};
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
  LongTermController controller(ControllerConfig{});
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
  LongTermController missed(config);
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
  LongTermController isolated(config);
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
    LongTermController controller(configured);
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
  LongTermController final_search(final_search_config);
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
      LongTermController controller{ControllerConfig{}};
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
    LongTermController controller(config);
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
    LongTermController controller{ControllerConfig{}};
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
      LongTermController controller{ControllerConfig{}};
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

  LongTermController reset{ControllerConfig{}};
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

  LongTermController duplicate{ControllerConfig{}};
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

  LongTermController stale_plan{ControllerConfig{}};
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
    LongTermController controller(ControllerConfig{});
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
    LongTermController controller(config);
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
    LongTermController controller(config);
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
      LongTermController controller(config);
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

  LongTermController controller(config);
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
    LongTermController holding(hold_config);
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
  LongTermController controller(config);
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
  expect_true(!plan.motion_control_authorized,
              "search plan fails closed one millisecond after confidence expiry");
  output = controller.finish_frame(plan, {}, std::nullopt);
  expect_true(!output.control_valid && !output.motion_control_authorized &&
                  output.tracking_phase == TrackingPhase::kSearching &&
                  output.control_source == ControlSource::kNone,
              "expired prediction stays search state but cannot drive control");

  plan = controller.begin_frame(6, 334, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 6), std::nullopt);
  expect_true(!output.accepted && !output.control_valid,
              "first visual return remains confirmation-only after expiry");
  plan = controller.begin_frame(7, 367, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 7), std::nullopt);
  expect_true(output.accepted && output.control_valid &&
                  output.control_source == ControlSource::kObservation &&
                  !controller.motion_.prediction_confident(367),
              "confirmed visual recovery ends motion search and restores observation control");

  ControllerConfig occlusion_config = config;
  occlusion_config.occlusion_reference_window = 3;
  LongTermController partial_occlusion(occlusion_config);
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

void test_search_detector_shadow_hint_isolation() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.search_reacquisition_requires_detector = true;
  config.motion_search_confidence_ms = 1000;
  config.detector_interval_ms = 100;
  config.detector_stale_ms = 150;
  config.detector_candidate_max_center_ratio = 0.30F;
  config.max_center_distance = 10.0F;
  config.max_area_log_change = 10.0F;
  config.max_aspect_log_change = 10.0F;
  LongTermController controller(config);
  const BBox initial{40, 30, 20, 20};
  controller.initialize(initial, 0, 0, {240, 160},
                        make_fixed_target_class(0, true));

  auto plan = controller.begin_frame(1, 40, true);
  auto output = controller.finish_frame(plan, {}, std::nullopt);
  expect_true(output.control_valid && !plan.request_detector,
              "entry frame starts search before detector scheduling");

  plan = controller.begin_frame(2, 80, true);
  expect_true(plan.request_detector && plan.detector_request &&
                  plan.tracking_phase == TrackingPhase::kSearching,
              "first SEARCHING frame requests an eligible detector");
  const DetectorRequest request = *plan.detector_request;
  const BBox source_candidate = plan.motion_predicted_bbox;
  output = controller.finish_frame(plan, {}, std::nullopt);

  plan = controller.begin_frame(3, 140, false);
  DetectorBatch delayed{request.frame_id, request.timestamp_ms,
                        {{source_candidate, 0.99F, 0}}};
  delayed.generation = request.generation;
  delayed.request_id = 77;
  output = controller.finish_frame(plan, {}, delayed);
  expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                  output.detector_candidates_queued == 1 &&
                  output.detector_candidate_diagnostics.size() == 1 &&
                  output.detector_candidate_diagnostics[0].projected_bbox.has_value(),
              "delayed matching detector batch is projected and queued during search");

  plan = controller.begin_frame(4, 160, true);
  expect_true(!plan.request_detector &&
                  plan.hypothesis_source == HypothesisSource::kDetector,
              "timestamp interval suppresses an early repeat while probing the hint");
  const BBox committed_before_hint = controller.current_bbox();
  const BBox motion_before_hint = plan.motion_predicted_bbox;
  output = controller.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 4), std::nullopt);
  expect_true(!output.accepted && output.search_hint &&
                  output.search_hint->confirmation_count == 1 &&
                  output.control_valid &&
                  output.control_source == ControlSource::kMotionSearch &&
                  output.control_bbox.x == motion_before_hint.x &&
                  controller.current_bbox().x == committed_before_hint.x,
              "first detector/anchor hit creates a shadow hint without control or state commit");

  plan = controller.begin_frame(5, 180, false);
  expect_true(plan.search_hint && plan.search_hint->used &&
                  plan.search_hint->candidate_id == output.search_hint->candidate_id &&
                  plan.predicted_bbox.x == plan.search_hint->projected_bbox.x,
              "shadow hint biases exactly the next processed tracker ROI");
  output = controller.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 5), std::nullopt);
  expect_true(output.accepted && output.reacquired &&
                  output.accepted_observation_path ==
                      AcceptedObservationPath::kSearchReacquisition &&
                  output.control_bbox.x == plan.predicted_bbox.x &&
                  output.search_hint &&
                  output.search_hint->confirmation_count == 2 &&
                  output.search_hint->used,
              "second identity-consistent shadow observation commits atomically");
  plan = controller.begin_frame(6, 181, true);
  expect_true(!plan.request_detector && !plan.search_hint &&
                  plan.tracking_phase == TrackingPhase::kTracking,
              "successful hint expires into ordinary tracking without detector work");
  controller.abort_frame();
  plan = controller.begin_frame(7, 220, false);
  expect_true(!plan.search_hint,
              "aborting a frame cannot retain or replay a consumed shadow hint");
  controller.finish_frame(plan, {}, std::nullopt);

  LongTermController conflict(config);
  conflict.initialize(initial, 0, 0, {240, 160},
                      make_fixed_target_class(0, true));
  conflict.finish_frame(conflict.begin_frame(1, 40, false), {}, std::nullopt);
  plan = conflict.begin_frame(2, 80, true);
  const DetectorRequest conflict_request = *plan.detector_request;
  DetectorBatch conflict_batch{conflict_request.frame_id,
                               conflict_request.timestamp_ms,
                               {{plan.motion_predicted_bbox, 0.99F, 0}}};
  conflict_batch.generation = conflict_request.generation;
  conflict.finish_frame(plan, {}, conflict_batch);
  plan = conflict.begin_frame(3, 120, false);
  output = conflict.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 3), std::nullopt);
  plan = conflict.begin_frame(4, 160, false);
  output = conflict.finish_frame(
      plan, reliable_pair(plan, BBox{180, 100, 20, 20}, 4), std::nullopt);
  expect_true(!output.accepted && output.search_hint &&
                  output.search_hint->rejection == SearchHintRejection::kConflict &&
                  output.control_valid &&
                  output.control_source == ControlSource::kMotionSearch,
              "conflicting next hit discards hint without disturbing motion control");

  LongTermController ambiguous(config);
  ambiguous.initialize(initial, 0, 0, {240, 160},
                       make_fixed_target_class(0, true));
  ambiguous.finish_frame(ambiguous.begin_frame(1, 40, false), {}, std::nullopt);
  plan = ambiguous.begin_frame(2, 80, true);
  const DetectorRequest ambiguous_request = *plan.detector_request;
  DetectorBatch ambiguous_batch{
      ambiguous_request.frame_id, ambiguous_request.timestamp_ms,
      {{plan.motion_predicted_bbox, 0.99F, 0},
       {BBox{42, 30, 20, 20}, 0.98F, 0}}};
  ambiguous_batch.generation = ambiguous_request.generation;
  ambiguous.finish_frame(plan, {}, ambiguous_batch);
  plan = ambiguous.begin_frame(3, 120, false);
  expect_true(plan.detector_same_class_ambiguous,
              "neighboring same-class candidates remain explicitly ambiguous");
  output = ambiguous.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 3), std::nullopt);
  expect_true(!output.accepted && !output.search_hint && output.control_valid,
              "same-class ambiguity cannot seed search confirmation or control");
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
  ControllerConfig detector_required = config;
  detector_required.search_reacquisition_requires_detector = true;
  LongTermController tracker_only_rejected(detector_required);
  const BBox initial{40, 30, 20, 20};
  tracker_only_rejected.initialize(initial, 0, 0, {240, 160});
  reject_frame(tracker_only_rejected, 1, 40);
  for (std::uint64_t frame : {2U, 3U}) {
    auto tracker_plan = tracker_only_rejected.begin_frame(
        frame, static_cast<std::int64_t>(frame * 40), false);
    const auto tracker_output = tracker_only_rejected.finish_frame(
        tracker_plan,
        {reliable_observation(tracker_plan, TemplateKind::kAnchor,
                              tracker_plan.predicted_bbox, frame)},
        std::nullopt);
    expect_true(!tracker_output.accepted && !tracker_output.search_hint &&
                    tracker_output.control_source == ControlSource::kMotionSearch,
                "detector-required search recovery rejects tracker-only confirmations");
  }
  LongTermController controller(config);
  controller.initialize(initial, 0, 0, {240, 160});
  auto output = reject_frame(controller, 1, 40);
  const BBox committed_before = controller.current_bbox();

  auto plan = controller.begin_frame(2, 80, false);
  output = controller.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kAnchor,
                            plan.predicted_bbox, 2)},
      std::nullopt);
  expect_true(!output.accepted && output.control_valid &&
                  output.template_action == TemplateAction::kNone &&
                  controller.current_bbox().x == committed_before.x,
              "one anchor hit remains shadow-only with motion control");

  plan = controller.begin_frame(3, 120, false);
  const BBox confirmed = plan.predicted_bbox;
  output = controller.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kAnchor, confirmed, 3)},
      std::nullopt);
  expect_true(output.accepted && output.reacquired &&
                  output.accepted_observation_path ==
                      AcceptedObservationPath::kSearchReacquisition &&
                  output.control_source == ControlSource::kObservation &&
                  output.template_action == TemplateAction::kStageRecent &&
                  controller.current_bbox().x == confirmed.x &&
                  !controller.motion_search_active_,
              "second identity-consistent anchor hit commits atomically and stages recent probation");

  plan = controller.begin_frame(4, 160, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 4), std::nullopt);
  expect_true(output.accepted && !output.reacquired &&
                  output.template_action == TemplateAction::kNone,
              "first post-commit reliable probation frame does not commit recent");
  plan = controller.begin_frame(5, 200, false);
  output = controller.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 5), std::nullopt);
  expect_true(output.accepted && !output.reacquired &&
                  output.template_action == TemplateAction::kCommitRecent,
              "configured reliable probation frames commit recent exactly once");

  LongTermController rollback_probation(config);
  rollback_probation.initialize(initial, 0, 0, {240, 160});
  reject_frame(rollback_probation, 1, 40);
  for (std::uint64_t frame : {2U, 3U}) {
    plan = rollback_probation.begin_frame(frame, frame * 40, false);
    output = rollback_probation.finish_frame(
        plan,
        {reliable_observation(plan, TemplateKind::kAnchor,
                              plan.predicted_bbox, frame)},
        std::nullopt);
  }
  expect_true(output.template_action == TemplateAction::kStageRecent,
              "atomic search recovery stages a probationary recent template");
  plan = rollback_probation.begin_frame(4, 160, false);
  output = rollback_probation.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kAnchor,
                            plan.predicted_bbox, 4),
       rejected_observation(plan, TemplateKind::kRecent,
                            plan.predicted_bbox, 4)},
      std::nullopt);
  expect_true(output.accepted && output.template_action ==
                                     TemplateAction::kRollbackRecent,
              "probation disagreement rolls recent back without losing anchor tracking");

  LongTermController recent_only(config);
  recent_only.initialize(initial, 0, 0, {240, 160});
  reject_frame(recent_only, 1, 40);
  for (std::uint64_t frame : {2U, 3U}) {
    plan = recent_only.begin_frame(frame, frame * 40, false);
    output = recent_only.finish_frame(
        plan,
        {reliable_observation(plan, TemplateKind::kRecent,
                              plan.predicted_bbox, frame)},
        std::nullopt);
    expect_true(!output.accepted && !output.reacquired,
                "recent-only search hit can never authorize reacquisition");
  }

  LongTermController disagreement(config);
  disagreement.initialize(initial, 0, 0, {240, 160});
  reject_frame(disagreement, 1, 40);
  plan = disagreement.begin_frame(2, 80, false);
  output = disagreement.finish_frame(
      plan, reliable_pair(plan, plan.predicted_bbox, 2), std::nullopt);
  plan = disagreement.begin_frame(3, 120, false);
  output = disagreement.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kAnchor,
                            plan.predicted_bbox, 3),
       reliable_observation(plan, TemplateKind::kRecent,
                            BBox{180, 100, 20, 20}, 3)},
      std::nullopt);
  expect_true(!output.accepted && output.control_valid &&
                  output.template_action == TemplateAction::kNone,
              "anchor/recent disagreement discards shadow candidate without template mutation");

  LongTermController expired(config);
  expired.initialize(initial, 0, 0, {240, 160});
  reject_frame(expired, 1, 40);
  plan = expired.begin_frame(2, 80, false);
  expired.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kAnchor,
                            plan.predicted_bbox, 2)},
      std::nullopt);
  plan = expired.begin_frame(5, 200, false);
  output = expired.finish_frame(
      plan,
      {reliable_observation(plan, TemplateKind::kAnchor,
                            plan.predicted_bbox, 5)},
      std::nullopt);
  expect_true(!output.accepted && !output.reacquired,
              "second hit outside confirmation window cannot commit");
}

}  // namespace

int main() {
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
  test_search_detector_shadow_hint_isolation();
  test_atomic_search_reacquisition_and_template_probation();

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
  LongTermController boundaries(config);
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

  LongTermController normal(config);
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

  LongTermController conflict(config);
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

  LongTermController recent_only(config);
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

  LongTermController detector_controller(config);
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
  LongTermController candidate_lock(lock_config);
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

  LongTermController same_class_ambiguity(lock_config);
  same_class_ambiguity.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120},
      make_fixed_target_class(0, true));
  reject_frame(same_class_ambiguity, 1, 100);
  plan = same_class_ambiguity.begin_frame(2, 1100, true);
  DetectorBatch ambiguous_batch{
      2, 1100,
      {{BBox{20, 20, 30, 40}, 0.95F, 0},
       {BBox{120, 20, 30, 40}, 0.90F, 0}}};
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
  LongTermController subtarget(subtarget_config);
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

  LongTermController edge_subtarget(subtarget_config);
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

  LongTermController opposite_edge_subtarget(subtarget_config);
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

  LongTermController strict_initial_geometry(lock_config);
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

  LongTermController strict_geometry(lock_config);
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

  LongTermController detector_identity(config);
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

  LongTermController boundary_gate(config);
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

  LongTermController rejected_gate(config);
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

  LongTermController mixed_gate(config);
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

  LongTermController lost_gate(config);
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
  LongTermController templates(template_config);
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

  LongTermController rollback(config);
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

  LongTermController degraded_recent(config);
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

  LongTermController bounded(config);
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

  LongTermController direct(coast_config);
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
  LongTermController probation(coast_config);
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
    LongTermController gradual(coast_config);
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
  LongTermController interrupted_shrink(coast_config);
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
    LongTermController template_evidence(coast_config);
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
    LongTermController partial(coast_config);
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
    LongTermController bounded_coast(coast_config);
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
  LongTermController pending(coast_config);
  seed(pending);
  pending.confirmations_.push_back({8, stable});
  pending.detector_candidates_.push_back({1, {stable, 0.99F, 0}, 0, {stable}, 1});
  output = collapse(pending, 9, 360);
  expect_true(output.occlusion_hold_triggered && pending.confirmations_.empty() &&
                  pending.detector_candidates_.empty(),
              "coast trigger clears pending confirmations and detector candidates");

  // Explicit reselect works with an open trigger frame, mid coast and pending loss.
  for (int reset_age : {0, 1, 6, 13}) {
    LongTermController reset(coast_config);
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
  LongTermController same_frame_batch(same_frame_config);
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

  LongTermController abandoned(coast_config);
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
  LongTermController clipped(coast_config);
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
    LongTermController invalid_motion(coast_config);
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
