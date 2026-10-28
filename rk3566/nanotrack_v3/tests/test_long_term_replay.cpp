#include "long_term_controller.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
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

nanotrack::ControllerOutput step(
    nanotrack::LongTermController& controller,
    std::uint64_t frame_id,
    std::int64_t timestamp_ms,
    const std::optional<nanotrack::BBox>& observation_bbox) {
  using namespace nanotrack;
  const SearchPlan plan = controller.begin_frame(frame_id, timestamp_ms, false);
  std::vector<TrackerObservation> observations;
  if (observation_bbox) {
    observations.push_back(TrackerObservation{
        frame_id, timestamp_ms, *observation_bbox,
        ResponseMetrics{0.93F, 0.91F, 11.0F, 14.0F, 0.22F},
        TemplateKind::kAnchor, frame_id});
  }
  return controller.finish_frame(plan, observations, std::nullopt);
}

nanotrack::TrackerObservation reliable_for_plan(
    const nanotrack::SearchPlan& plan,
    const nanotrack::BBox& bbox,
    std::uint64_t token,
    nanotrack::TemplateKind kind = nanotrack::TemplateKind::kAnchor) {
  return nanotrack::TrackerObservation{
      plan.frame_id, plan.timestamp_ms, bbox,
      nanotrack::ResponseMetrics{0.93F, 0.91F, 11.0F, 14.0F, 0.22F},
      kind, token};
}

nanotrack::TrackerObservation weak_for_plan(
    const nanotrack::SearchPlan& plan,
    const nanotrack::BBox& bbox,
    std::uint64_t token) {
  return nanotrack::TrackerObservation{
      plan.frame_id, plan.timestamp_ms, bbox,
      nanotrack::ResponseMetrics{0.60F, 0.58F, 6.0F, 5.0F, 0.05F},
      nanotrack::TemplateKind::kAnchor, token};
}

std::vector<nanotrack::TrackerObservation> reliable_pair_for_plan(
    const nanotrack::SearchPlan& plan,
    const nanotrack::BBox& bbox,
    std::uint64_t token) {
  return {
      reliable_for_plan(plan, bbox, token, nanotrack::TemplateKind::kAnchor),
      reliable_for_plan(plan, bbox, token, nanotrack::TemplateKind::kRecent),
  };
}

}  // namespace

int main() {
  using namespace nanotrack;
  const BBox initial{40, 30, 20, 20};
  const FrameSize frame_size{240, 160};
  ControllerConfig config;

  LongTermController normal(config);
  normal.initialize(initial, 0, 0, frame_size);
  ControllerOutput output;
  for (int frame = 1; frame <= 10; ++frame) {
    output = step(normal, frame, frame * 40, initial);
    expect_true(output.control_valid && output.mode == TrackingMode::kTracking,
                "normal replay remains valid tracking");
  }

  output = step(normal, 11, 440, std::nullopt);
  expect_true(output.mode == TrackingMode::kUncertain && !output.control_valid,
              "short occlusion invalidates control immediately");
  output = step(normal, 12, 480, initial);
  expect_true(!output.control_valid,
              "first post-occlusion match remains under confirmation");
  output = step(normal, 13, 520, initial);
  expect_true(output.reacquired && output.control_valid,
              "short occlusion recovers after two consistent frames");

  LongTermController assisted(config);
  assisted.initialize(initial, 0, 0, frame_size);
  step(assisted, 1, 100, std::nullopt);
  output = step(assisted, 2, 1100, std::nullopt);
  expect_true(output.mode == TrackingMode::kAssistedRecovery,
              "one-second loss reaches assisted recovery without detector");
  output = step(assisted, 3, 1140, initial);
  expect_true(!output.control_valid,
              "assisted first match remains unconfirmed");
  output = step(assisted, 4, 1180, initial);
  expect_true(output.reacquired && output.control_valid,
              "template-only assisted recovery can reacquire");

  LongTermController permanent(config);
  permanent.initialize(initial, 0, 0, frame_size);
  step(permanent, 1, 40, std::nullopt);
  for (int frame = 2; frame <= 100; ++frame) {
    output = step(permanent, frame, frame * 40, std::nullopt);
  }
  expect_true(output.mode == TrackingMode::kLost && !output.control_valid,
              "permanent loss stays invalid without detector");

  LongTermController single_detector(config);
  single_detector.initialize(initial, 0, 0, frame_size);
  step(single_detector, 1, 100, std::nullopt);
  SearchPlan detector_plan = single_detector.begin_frame(2, 1100, true);
  DetectorBatch single_batch{
      2, 1100, {{BBox{70, 40, 20, 20}, 0.99F, 0}}};
  output = single_detector.finish_frame(detector_plan, {}, single_batch);
  expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                  !output.control_valid,
              "detector batch only queues a hypothesis");
  detector_plan = single_detector.begin_frame(3, 1140, false);
  expect_true(detector_plan.hypothesis_source == HypothesisSource::kDetector &&
                  detector_plan.detector_candidate_class_id ==
                      std::optional<int>(0),
              "queued candidate becomes an auditable detector hypothesis");
  output = single_detector.finish_frame(
      detector_plan,
      {reliable_for_plan(detector_plan, detector_plan.predicted_bbox, 3)},
      std::nullopt);
  expect_true(!output.accepted && !output.control_valid && !output.reacquired,
              "one high-confidence candidate plus one reliable verification cannot reacquire");

  LongTermController confirmed_detector(config);
  confirmed_detector.initialize(initial, 0, 0, frame_size);
  step(confirmed_detector, 1, 100, std::nullopt);
  detector_plan = confirmed_detector.begin_frame(2, 1100, true);
  const BBox recovered{70, 40, 20, 20};
  DetectorBatch consistent_batch{
      2, 1100,
      {{recovered, 0.99F, 0}, {BBox{71, 40, 20, 20}, 0.98F, 1}}};
  confirmed_detector.finish_frame(detector_plan, {}, consistent_batch);
  detector_plan = confirmed_detector.begin_frame(3, 1140, false);
  output = confirmed_detector.finish_frame(
      detector_plan,
      reliable_pair_for_plan(detector_plan, detector_plan.predicted_bbox, 3),
      std::nullopt);
  expect_true(!output.control_valid,
              "first detector-guided NanoTrack verification stays invalid");
  detector_plan = confirmed_detector.begin_frame(4, 1180, false);
  output = confirmed_detector.finish_frame(
      detector_plan,
      reliable_pair_for_plan(detector_plan, detector_plan.predicted_bbox, 4),
      std::nullopt);
  expect_true(output.accepted && output.reacquired && output.control_valid,
              "two spatially consistent verifications in three frames reacquire");

  LongTermController filtered_detector(config);
  filtered_detector.initialize(initial, 0, 0, frame_size);
  step(filtered_detector, 1, 100, std::nullopt);
  detector_plan = filtered_detector.begin_frame(2, 1100, true);
  DetectorBatch empty_after_class_filter{2, 1100, {}};
  output = filtered_detector.finish_frame(detector_plan, {},
                                          empty_after_class_filter);
  expect_true(output.detector_status == DetectorBatchStatus::kInvalid &&
                  output.detector_candidates_queued == 0 &&
                  !output.control_valid,
              "wrong-class detections filtered by the engine cannot enter recovery");

  LongTermController invalid_detector(config);
  invalid_detector.initialize(initial, 0, 0, frame_size);
  step(invalid_detector, 1, 100, std::nullopt);
  detector_plan = invalid_detector.begin_frame(2, 1100, true);
  DetectorBatch invalid_batch{
      2, 1100, {{BBox{-1, 40, 20, 20}, 0.99F, 0}}};
  output = invalid_detector.finish_frame(detector_plan, {}, invalid_batch);
  expect_true(output.detector_status == DetectorBatchStatus::kInvalid &&
                  !output.control_valid,
              "invalid detector geometry is discarded");

  LongTermController same_class_distractor(config);
  same_class_distractor.initialize(initial, 0, 0, frame_size);
  step(same_class_distractor, 1, 100, std::nullopt);
  detector_plan = same_class_distractor.begin_frame(2, 1100, true);
  const BBox distractor_box{180, 100, 20, 20};
  DetectorBatch distractor_batch{
      2, 1100,
      {{distractor_box, 0.99F, 0}, {distractor_box, 0.98F, 0}}};
  output = same_class_distractor.finish_frame(
      detector_plan, {}, distractor_batch);
  expect_true(output.detector_status ==
                      DetectorBatchStatus::kPositionRejected &&
                  output.detector_position_valid_count == 0 &&
                  output.detector_position_rejected_count == 2 &&
                  output.detector_candidates_queued == 0 &&
                  !output.accepted && !output.reacquired &&
                  !output.control_valid,
              "far same-class distractors are rejected before NanoTrack verification");
  detector_plan = same_class_distractor.begin_frame(3, 1140, false);
  expect_true(detector_plan.hypothesis_source == HypothesisSource::kMotion &&
                  !detector_plan.detector_candidate_id.has_value(),
              "far distractors cannot become queued detector hypotheses");
  output = same_class_distractor.finish_frame(
      detector_plan,
      {reliable_for_plan(detector_plan, initial, 3)}, std::nullopt);
  expect_true(!output.accepted && !output.control_valid,
              "position rejection cannot bypass recovery confirmation");

  LongTermController deformation(config);
  deformation.initialize(initial, 0, 0, frame_size);
  const BBox deformed{35, 32, 30, 16};
  output = step(deformation, 1, 40, deformed);
  expect_true(output.accepted && output.control_valid,
              "bounded target deformation remains trackable");

  LongTermController distractor(config);
  distractor.initialize(initial, 0, 0, frame_size);
  const BBox far_distractor{180, 30, 20, 20};
  output = step(distractor, 1, 40, far_distractor);
  expect_true(!output.accepted && !output.control_valid &&
                  output.quality == QualityLevel::kRejected,
              "high-response distant distractor fails the motion gate");

  const auto accept_reliable = [](LongTermController& controller,
                                  std::uint64_t frame_id,
                                  std::int64_t timestamp_ms,
                                  const BBox& bbox) {
    const SearchPlan plan =
        controller.begin_frame(frame_id, timestamp_ms, false);
    return controller.finish_frame(
        plan, {reliable_for_plan(plan, bbox, frame_id)}, std::nullopt);
  };
  const auto reject_shrunken = [](LongTermController& controller,
                                  std::uint64_t frame_id,
                                  std::int64_t timestamp_ms,
                                  const BBox& bbox) {
    const SearchPlan plan =
        controller.begin_frame(frame_id, timestamp_ms, false);
    return controller.finish_frame(
        plan, {weak_for_plan(plan, bbox, frame_id)}, std::nullopt);
  };

  ControllerConfig hold_one_config = config;
  hold_one_config.occlusion_hold_frames = 1;
  hold_one_config.uncertain_ms = 200;
  hold_one_config.local_recovery_ms = 1000;
  hold_one_config.assisted_recovery_ms = 3000;
  LongTermController one_frame_hold(hold_one_config);
  one_frame_hold.initialize(initial, 0, 0, frame_size);
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    accept_reliable(one_frame_hold, frame, frame * 40, initial);
  }
  reject_shrunken(one_frame_hold, 9, 360, BBox{43, 33, 14, 14});
  SearchPlan trigger_plan = one_frame_hold.begin_frame(10, 400, false);
  expect_true(trigger_plan.run_tracker,
              "the trigger frame still permits NanoTrack inference");
  output = one_frame_hold.finish_frame(
      trigger_plan,
      {weak_for_plan(trigger_plan, BBox{43, 33, 14, 14}, 10)},
      std::nullopt);
  expect_true(output.occlusion_hold_triggered && !output.control_valid,
              "triggering hold never authorizes motion-only control");
  SearchPlan hold_plan = one_frame_hold.begin_frame(11, 440, true);
  expect_true(hold_plan.mode == TrackingMode::kOcclusionHold &&
                  !hold_plan.run_tracker && !hold_plan.request_detector &&
                  !hold_plan.detector_request &&
                  hold_plan.hold_remaining_frames == 1 &&
                  hold_plan.predicted_bbox.width == 20.0F &&
                  hold_plan.predicted_bbox.height == 20.0F,
              "N=1 produces exactly one complete frozen-scale hold plan");
  output = one_frame_hold.finish_frame(hold_plan, {}, std::nullopt);
  expect_true(!output.control_valid && !output.accepted &&
                  output.mode == TrackingMode::kOcclusionHold,
              "hold output remains explicitly invalid");
  SearchPlan resumed_plan = one_frame_hold.begin_frame(12, 480, false);
  expect_true(resumed_plan.mode == TrackingMode::kUncertain &&
                  resumed_plan.run_tracker,
              "the frame after N=1 resumes the timestamp-derived uncertain mode");
  one_frame_hold.finish_frame(resumed_plan, {}, std::nullopt);

  ControllerConfig hold_twelve_config = hold_one_config;
  hold_twelve_config.occlusion_hold_frames = 12;
  LongTermController twelve_frame_hold(hold_twelve_config);
  twelve_frame_hold.initialize(initial, 0, 0, frame_size);
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    accept_reliable(twelve_frame_hold, frame, frame * 40, initial);
  }
  reject_shrunken(twelve_frame_hold, 9, 360, BBox{43, 33, 14, 14});
  reject_shrunken(twelve_frame_hold, 10, 400, BBox{43, 33, 14, 14});
  for (std::uint64_t offset = 1; offset <= 12; ++offset) {
    const SearchPlan tick = twelve_frame_hold.begin_frame(
        10 + offset, 400 + static_cast<std::int64_t>(offset) * 40, true);
    expect_true(tick.mode == TrackingMode::kOcclusionHold &&
                    !tick.run_tracker && !tick.request_detector &&
                    tick.hold_remaining_frames == 13 - offset &&
                    tick.predicted_bbox.width == 20.0F &&
                    tick.predicted_bbox.height == 20.0F,
                "N=12 keeps every complete hold frame frozen and detector-free");
    output = twelve_frame_hold.finish_frame(tick, {}, std::nullopt);
    expect_true(!output.control_valid,
                "every N=12 hold frame keeps control invalid");
  }
  resumed_plan = twelve_frame_hold.begin_frame(23, 920, false);
  expect_true(resumed_plan.mode == TrackingMode::kLocalRecovery &&
                  resumed_plan.run_tracker,
              "the thirteenth frame resumes after exactly twelve hold frames");
  twelve_frame_hold.finish_frame(resumed_plan, {}, std::nullopt);

  reject_shrunken(twelve_frame_hold, 24, 960, BBox{43, 33, 14, 14});
  output = reject_shrunken(twelve_frame_hold, 25, 1000,
                           BBox{43, 33, 14, 14});
  expect_true(!output.occlusion_hold_triggered &&
                  twelve_frame_hold.mode() != TrackingMode::kOcclusionHold,
              "one loss episode cannot enter hold twice");

  LongTermController assisted_no_hold(hold_one_config);
  assisted_no_hold.initialize(initial, 0, 0, frame_size);
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    accept_reliable(assisted_no_hold, frame, frame * 40, initial);
  }
  step(assisted_no_hold, 9, 360, std::nullopt);
  trigger_plan = assisted_no_hold.begin_frame(10, 1400, false);
  expect_true(trigger_plan.mode == TrackingMode::kAssistedRecovery,
              "boundary test reaches assisted recovery before shrink observation");
  assisted_no_hold.finish_frame(
      trigger_plan,
      {weak_for_plan(trigger_plan, BBox{43, 33, 14, 14}, 10)},
      std::nullopt);
  trigger_plan = assisted_no_hold.begin_frame(11, 1440, false);
  output = assisted_no_hold.finish_frame(
      trigger_plan,
      {weak_for_plan(trigger_plan, BBox{43, 33, 14, 14}, 11)},
      std::nullopt);
  expect_true(!output.occlusion_hold_triggered &&
                  assisted_no_hold.mode() == TrackingMode::kAssistedRecovery,
              "assisted recovery cannot first enter occlusion hold");

  LongTermController lost_no_hold(hold_one_config);
  lost_no_hold.initialize(initial, 0, 0, frame_size);
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    accept_reliable(lost_no_hold, frame, frame * 40, initial);
  }
  step(lost_no_hold, 9, 360, std::nullopt);
  trigger_plan = lost_no_hold.begin_frame(10, 3400, false);
  expect_true(trigger_plan.mode == TrackingMode::kLost,
              "boundary test reaches lost before shrink observation");
  lost_no_hold.finish_frame(
      trigger_plan,
      {weak_for_plan(trigger_plan, BBox{43, 33, 14, 14}, 10)},
      std::nullopt);
  trigger_plan = lost_no_hold.begin_frame(11, 3440, false);
  output = lost_no_hold.finish_frame(
      trigger_plan,
      {weak_for_plan(trigger_plan, BBox{43, 33, 14, 14}, 11)},
      std::nullopt);
  expect_true(!output.occlusion_hold_triggered &&
                  lost_no_hold.mode() == TrackingMode::kLost,
              "lost cannot first enter occlusion hold");

  LongTermController rearmed(hold_one_config);
  rearmed.initialize(initial, 0, 0, frame_size);
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    accept_reliable(rearmed, frame, frame * 40, initial);
  }
  reject_shrunken(rearmed, 9, 360, BBox{43, 33, 14, 14});
  reject_shrunken(rearmed, 10, 400, BBox{43, 33, 14, 14});
  hold_plan = rearmed.begin_frame(11, 440, false);
  rearmed.finish_frame(hold_plan, {}, std::nullopt);
  resumed_plan = rearmed.begin_frame(12, 480, false);
  output = rearmed.finish_frame(
      resumed_plan, {reliable_for_plan(resumed_plan, initial, 12)},
      std::nullopt);
  expect_true(!output.accepted,
              "first reliable post-hold observation remains under confirmation");
  resumed_plan = rearmed.begin_frame(13, 520, false);
  output = rearmed.finish_frame(
      resumed_plan, {reliable_for_plan(resumed_plan, initial, 13)},
      std::nullopt);
  expect_true(output.reacquired && output.control_valid,
              "accepted reacquisition re-arms the hold episode");
  for (std::uint64_t frame = 14; frame <= 20; ++frame) {
    accept_reliable(rearmed, frame, frame * 40, initial);
  }
  reject_shrunken(rearmed, 21, 840, BBox{43, 33, 14, 14});
  output = reject_shrunken(rearmed, 22, 880, BBox{43, 33, 14, 14});
  expect_true(output.occlusion_hold_triggered,
              "reacquisition seeds a fresh reference window and permits a later hold");

  rearmed.initialize(initial, 30, 1200, frame_size);
  for (std::uint64_t frame = 31; frame <= 38; ++frame) {
    accept_reliable(rearmed, frame, frame * 40, initial);
  }
  reject_shrunken(rearmed, 39, 1560, BBox{43, 33, 14, 14});
  output = reject_shrunken(rearmed, 40, 1600, BBox{43, 33, 14, 14});
  expect_true(output.occlusion_hold_triggered,
              "explicit initialize clears and re-arms hold without seeding the manual box");

  if (failures != 0) {
    std::cerr << failures << " replay test(s) failed\n";
    return 1;
  }
  std::cout << "long-term replay tests passed\n";
  return 0;
}
