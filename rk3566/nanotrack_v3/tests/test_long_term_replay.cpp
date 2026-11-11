#include "long_term_controller.hpp"
#include "target_class.hpp"

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
    const std::optional<nanotrack::BBox>& observation_bbox,
    nanotrack::TemplateKind kind = nanotrack::TemplateKind::kAnchor) {
  using namespace nanotrack;
  const SearchPlan plan = controller.begin_frame(frame_id, timestamp_ms, false);
  std::vector<TrackerObservation> observations;
  if (observation_bbox) {
    observations.push_back(TrackerObservation{
        frame_id, timestamp_ms, *observation_bbox,
        ResponseMetrics{0.93F, 0.91F, 11.0F, 14.0F, 0.22F},
        kind, frame_id});
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

void test_recent_safe_recovery_replay() {
  using namespace nanotrack;
  const BBox initial{40, 30, 20, 20};
  const BBox other{68, 30, 20, 20};
  const FrameSize frame_size{240, 160};
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.motion_search_confidence_ms = 100;
  config.detector_candidate_max_center_ratio = 0.20F;
  config.search_reacquisition_requires_detector = true;  // Ignored migration field.

  LongTermController tracker_only(config);
  tracker_only.initialize(initial, 0, 0, frame_size);
  auto plan = tracker_only.begin_frame(1, 40, true);
  auto out = tracker_only.finish_frame(plan, {}, std::nullopt);
  expect_true(out.control_source == ControlSource::kMotionSearch,
              "search starts with motion authorization and no accepted observation");
  plan = tracker_only.begin_frame(2, 80, true);
  expect_true(plan.template_run_mode == TemplateRunMode::kRecentAndSafe &&
                  !plan.request_detector,
              "unknown detector class still permits dual-template search");
  out = tracker_only.finish_frame(
      plan, {reliable_for_plan(plan, initial, 2, TemplateKind::kRecent),
             reliable_for_plan(plan, initial, 2, TemplateKind::kSafe)}, std::nullopt);
  expect_true(!out.accepted && !out.observation_present &&
                  out.template_action == TemplateAction::kNone && out.control_valid,
              "agreeing templates count as one hit and leave motion control untouched");
  plan = tracker_only.begin_frame(3, 120, true);
  out = tracker_only.finish_frame(plan, {}, std::nullopt);
  expect_true(!out.accepted && !out.control_valid && !plan.request_detector,
              "one missing hit expires motion authorization from the last trusted time");
  plan = tracker_only.begin_frame(4, 160, true);
  auto weak_safe = reliable_for_plan(plan, other, 4, TemplateKind::kSafe);
  weak_safe.metrics.psr = 2.0F;
  out = tracker_only.finish_frame(
      plan, {reliable_for_plan(plan, initial, 4, TemplateKind::kRecent), weak_safe},
      std::nullopt);
  expect_true(out.reacquired && out.accepted && out.control_valid &&
                  out.selected_template == TemplateKind::kRecent &&
                  out.template_action == TemplateAction::kStageRecent &&
                  !plan.request_detector,
              "continuous recent identity recovers across one gap without detector or safe veto");

  LongTermController safe_only(config);
  safe_only.initialize(initial, 0, 0, frame_size);
  safe_only.finish_frame(safe_only.begin_frame(1, 30, false), {}, std::nullopt);
  for (std::uint64_t frame = 2; frame <= 3; ++frame) {
    plan = safe_only.begin_frame(frame, frame * 30, true);
    out = safe_only.finish_frame(
        plan, {reliable_for_plan(plan, initial, frame, TemplateKind::kSafe)}, std::nullopt);
    expect_true(!plan.request_detector && out.reacquired == (frame == 3) &&
                    (frame == 2 || out.selected_template == TemplateKind::kSafe),
                "safe alone follows the same two-hit path without detector");
  }

  LongTermController detector(config);
  detector.initialize(initial, 0, 0, frame_size, make_fixed_target_class(0, true));
  plan = detector.begin_frame(1, 30, false);
  detector.finish_frame(plan, {}, std::nullopt);
  plan = detector.begin_frame(2, 60, true);
  const auto request = *plan.detector_request;
  DetectorBatch batch{request.frame_id, request.timestamp_ms,
                      {{other, 0.99F, 0}}};
  batch.generation = request.generation;
  out = detector.finish_frame(plan, {}, batch);
  expect_true(!out.accepted && !out.search_hint &&
                  out.control_bbox.x == plan.motion_predicted_bbox.x,
              "a detector-only candidate preserves motion control and has no verification hint");
  plan = detector.begin_frame(3, 90, false);
  out = detector.finish_frame(
      plan, {reliable_for_plan(plan, other, 3, TemplateKind::kSafe)}, std::nullopt);
  expect_true(plan.hypothesis_source == HypothesisSource::kDetector &&
                  !out.accepted && out.template_action == TemplateAction::kNone &&
                  out.control_bbox.x == plan.motion_predicted_bbox.x,
              "first safe verification of a detector seed still cannot authorize control");
  plan = detector.begin_frame(4, 120, false);
  out = detector.finish_frame(
      plan, {reliable_for_plan(plan, other, 4, TemplateKind::kSafe)}, std::nullopt);
  expect_true(out.reacquired && out.accepted &&
                  out.selected_template == TemplateKind::kSafe &&
                  out.template_action == TemplateAction::kStageRecent &&
                  out.search_hint && out.search_hint->used &&
                  out.search_hint->confirmation_count == 2,
              "the same detector-seeded identity commits through two safe verifications");

  LongTermController ambiguous(config);
  ambiguous.initialize(initial, 0, 0, frame_size, make_fixed_target_class(0, true));
  plan = ambiguous.begin_frame(1, 30, false);
  ambiguous.finish_frame(plan, {}, std::nullopt);
  plan = ambiguous.begin_frame(2, 60, true);
  const auto competing_request = *plan.detector_request;
  DetectorBatch competing{competing_request.frame_id, competing_request.timestamp_ms,
                          {{initial, 0.80F, 0}, {other, 0.99F, 0}}};
  competing.generation = competing_request.generation;
  ambiguous.finish_frame(plan, {}, competing);
  for (std::uint64_t frame = 3; frame <= 8; ++frame) {
    plan = ambiguous.begin_frame(frame, frame * 30, false);
    out = ambiguous.finish_frame(
        plan, {reliable_for_plan(plan, other, frame, TemplateKind::kRecent)},
        std::nullopt);
    expect_true(!out.accepted && out.tracking_phase == TrackingPhase::kSearching &&
                    out.recovery_conflict && out.template_action == TemplateAction::kNone,
                "repeated hits at the higher-confidence object cannot resolve identity as its rival ages out");
    if (frame > 4) {
      expect_true(out.detector_candidates_queued == 0,
                  "detector rivals age out even while their identity conflict remains");
    }
  }
}

}  // namespace

int main() {
  test_recent_safe_recovery_replay();
  using namespace nanotrack;
  const BBox initial{40, 30, 20, 20};
  const FrameSize frame_size{240, 160};
  ControllerConfig config;

  LongTermController normal(config);
  normal.initialize(initial, 0, 0, frame_size,
      make_unknown_target_class(TargetClassReason::kNoMatch));
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
  expect_true(!output.reacquired && output.control_valid &&
                  output.accepted_observation_path ==
                      AcceptedObservationPath::kShortConfirmation,
              "short gap resumes through bounded confirmation after two consistent frames");

  ControllerConfig motion_search_config = config;
  motion_search_config.motion_search_enabled = true;
  motion_search_config.motion_search_confidence_ms = 160;
  motion_search_config.motion_max_speed_diagonals_per_s = 0.75F;
  motion_search_config.motion_max_acceleration_diagonals_per_s2 = 3.0F;
  motion_search_config.motion_scale_rate_limit_per_s = 0.30F;
  motion_search_config.search_probe_interval_ms = 80;
  LongTermController motion_search(motion_search_config);
  motion_search.initialize(initial, 0, 0, frame_size,
      make_unknown_target_class(TargetClassReason::kNoMatch));
  output = step(motion_search, 1, 40, BBox{44, 30, 20, 20});
  output = step(motion_search, 2, 80, BBox{48, 30, 20, 20});
  output = step(motion_search, 3, 120, std::nullopt);
  expect_true(output.control_valid && !output.observation_valid &&
                  output.tracking_phase == TrackingPhase::kSearching &&
                  output.control_source == ControlSource::kMotionSearch,
              "candidate replay drives motion search immediately on occlusion");
  output = step(motion_search, 4, 200, std::nullopt);
  expect_true(output.control_valid && output.motion_control_authorized,
              "candidate replay continues motion control across recovery modes");
  output = step(motion_search, 5, 281, std::nullopt);
  expect_true(!output.control_valid && !output.motion_control_authorized,
              "candidate replay fails closed after time confidence expires");
  output = step(motion_search, 6, 320, initial, TemplateKind::kRecent);
  expect_true(!output.accepted && !output.reacquired,
              "candidate replay first visual return stays shadow-only");
  output = step(motion_search, 7, 360, initial, TemplateKind::kRecent);
  expect_true(output.accepted && output.reacquired &&
                  output.accepted_observation_path ==
                      AcceptedObservationPath::kSearchReacquisition,
              "candidate replay commits only after the second consistent visual hit");

  LongTermController assisted(config);
  assisted.initialize(initial, 0, 0, frame_size,
      make_unknown_target_class(TargetClassReason::kNoMatch));
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
  permanent.initialize(initial, 0, 0, frame_size,
      make_unknown_target_class(TargetClassReason::kNoMatch));
  step(permanent, 1, 40, std::nullopt);
  for (int frame = 2; frame <= 100; ++frame) {
    output = step(permanent, frame, frame * 40, std::nullopt);
  }
  expect_true(output.mode == TrackingMode::kLost && !output.control_valid,
              "permanent loss stays invalid without detector");

  LongTermController single_detector(config);
  single_detector.initialize(initial, 0, 0, frame_size,
      make_fixed_target_class(0, true));
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
  confirmed_detector.initialize(initial, 0, 0, frame_size,
      make_fixed_target_class(0, true));
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
  filtered_detector.initialize(initial, 0, 0, frame_size,
      make_fixed_target_class(0, true));
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
  invalid_detector.initialize(initial, 0, 0, frame_size,
      make_fixed_target_class(0, true));
  step(invalid_detector, 1, 100, std::nullopt);
  detector_plan = invalid_detector.begin_frame(2, 1100, true);
  DetectorBatch invalid_batch{
      2, 1100, {{BBox{-1, 40, 20, 20}, 0.99F, 0}}};
  output = invalid_detector.finish_frame(detector_plan, {}, invalid_batch);
  expect_true(output.detector_status == DetectorBatchStatus::kInvalid &&
                  !output.control_valid,
              "invalid detector geometry is discarded");

  LongTermController same_class_distractor(config);
  same_class_distractor.initialize(initial, 0, 0, frame_size,
      make_fixed_target_class(0, true));
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
  deformation.initialize(initial, 0, 0, frame_size,
      make_unknown_target_class(TargetClassReason::kNoMatch));
  const BBox deformed{35, 32, 30, 16};
  output = step(deformation, 1, 40, deformed);
  expect_true(output.accepted && output.control_valid,
              "bounded target deformation remains trackable");

  LongTermController distractor(config);
  distractor.initialize(initial, 0, 0, frame_size,
      make_unknown_target_class(TargetClassReason::kNoMatch));
  const BBox far_distractor{180, 30, 20, 20};
  output = step(distractor, 1, 40, far_distractor);
  expect_true(!output.accepted && !output.control_valid &&
                  output.quality == QualityLevel::kRejected,
              "high-response distant distractor fails the motion gate");

  // This replay catches an extra thirteenth control frame and accidental
  // adoption of a shrunken observation while the coast is open.
  struct CoastBudget { int configured; int expected; };
  for (CoastBudget budget : {CoastBudget{1, 1}, CoastBudget{12, 12}, CoastBudget{300, 12}}) {
    const int duration = budget.expected;
    ControllerConfig coast_config = config;
    coast_config.occlusion_hold_frames = budget.configured;
    LongTermController coast(coast_config);
    coast.initialize(initial, 0, 0, frame_size);
    for (int frame = 1; frame <= 8; ++frame)
      step(coast, frame, frame * 40, initial);
    SearchPlan tick = coast.begin_frame(9, 360, true);
    output = coast.finish_frame(
        tick, {reliable_for_plan(tick, BBox{43, 30, 14, 20}, 9)}, std::nullopt);
    expect_true(output.occlusion_hold_triggered && output.control_valid &&
                    output.motion_control_authorized && !output.accepted &&
                    output.occlusion_coast_age_frames == 1 &&
                    output.bbox.width == 20,
                "the severe RELIABLE trigger is the first authorized coast frame");
    for (int age = 2; age <= duration; ++age) {
      tick = coast.begin_frame(8 + age, 320 + age * 40, true);
      expect_true(tick.mode == TrackingMode::kOcclusionHold &&
                      tick.run_tracker == (age == 5 || age == 10) && !tick.request_detector &&
                      tick.occlusion_coast_age_frames == static_cast<std::size_t>(age),
                  "coast runs only periodic probes and never requests detector work");
      output = coast.finish_frame(
          tick, {reliable_for_plan(tick, BBox{43, 30, 14, 20}, 8 + age)},
          std::nullopt);
      expect_true(output.control_valid && !output.accepted &&
                      output.bbox.width == 20 && output.bbox.height == 20,
                  "unconfirmed observations cannot replace frozen motion control");
    }
    tick = coast.begin_frame(9 + duration, 360 + duration * 40, false);
    expect_true(tick.run_tracker && tick.mode != TrackingMode::kOcclusionHold &&
                    !tick.motion_control_authorized && tick.predicted_bbox.width == 20,
                "the first frame after the inclusive budget resumes frozen recovery");
    output = coast.finish_frame(tick, {}, std::nullopt);
    expect_true(!output.control_valid,
                "exhausted motion coast does not silently extend control");
    tick = coast.begin_frame(10 + duration, 400 + duration * 40, false);
    output = coast.finish_frame(
        tick, {reliable_for_plan(tick, initial, 10 + duration)}, std::nullopt);
    expect_true(!output.control_valid && !output.accepted,
                "the first visual match after coast requires confirmation");
    tick = coast.begin_frame(11 + duration, 440 + duration * 40, false);
    output = coast.finish_frame(
        tick, {reliable_for_plan(tick, initial, 11 + duration)}, std::nullopt);
    const bool reached_local_recovery = duration > 1;
    const auto expected_path = reached_local_recovery
        ? AcceptedObservationPath::kLocalOrDeeperRecovery
        : AcceptedObservationPath::kShortConfirmation;
    expect_true(output.reacquired == reached_local_recovery &&
                    output.control_valid &&
                    output.accepted_observation_path == expected_path,
                "post-coast confirmation resets only at local-or-deeper recovery");
    for (int frame = 12 + duration; frame <= 18 + duration; ++frame)
      step(coast, frame, frame * 40, initial);
    tick = coast.begin_frame(19 + duration, (19 + duration) * 40, false);
    output = coast.finish_frame(
        tick, {reliable_for_plan(tick, BBox{43, 30, 14, 20}, 19 + duration)},
        std::nullopt);
    expect_true(output.occlusion_hold_triggered && output.control_valid,
                "confirmed recovery clears old evidence and rearms a fresh coast");
  }

  // A target reappearing at age four recovers at 5/6; reappearing at age
  // seven misses probe five and recovers at 10/11. Search alone never moves
  // the control box, and a failed confirm cannot shift the periodic schedule.
  for (int reappearance_age : {4, 7}) {
    LongTermController coast(config);
    coast.initialize(initial, 0, 0, frame_size);
    for (int frame = 1; frame <= 8; ++frame)
      step(coast, frame, frame * 40, initial);
    auto plan = coast.begin_frame(9, 360, false);
    output = coast.finish_frame(plan,
        reliable_pair_for_plan(plan, BBox{41, 30, 18, 20}, 9), std::nullopt);
    expect_true(!output.occlusion_hold_triggered && output.accepted,
                "gradual replay starts with one suspicious accepted shrink");
    plan = coast.begin_frame(10, 400, false);
    auto shrink = reliable_pair_for_plan(plan, BBox{42, 30, 16, 20}, 10);
    for (auto& observation : shrink) observation.metrics.peak_margin = 0.17F;
    output = coast.finish_frame(plan, shrink, std::nullopt);
    expect_true(output.occlusion_hold_triggered && output.occlusion_coast_age_frames == 1 &&
                    output.control_valid && output.bbox.width == 20,
                "second adjacent shrink plus peak evidence starts bounded motion coast");
    const int search_age = reappearance_age == 4 ? 5 : 10;
    const BBox recovered{44, 30, 20, 20};
    for (int age = 2; age <= search_age + 1; ++age) {
      plan = coast.begin_frame(9 + age, 360 + age * 40, true);
      const bool confirming = age == search_age + 1;
      const auto expected = confirming ? OcclusionProbeType::kConfirm
          : age == 5 || age == 10 ? OcclusionProbeType::kSearch : OcclusionProbeType::kNone;
      expect_true(plan.occlusion_probe_type == expected &&
                      plan.run_tracker == (expected != OcclusionProbeType::kNone) &&
                      !plan.request_detector && plan.predicted_bbox.width == 20,
                  "reappearance replay follows five-frame search and immediate confirm plans");
      std::vector<TrackerObservation> observations;
      if (plan.run_tracker && age >= reappearance_age)
        observations = reliable_pair_for_plan(plan, recovered, plan.frame_id);
      output = coast.finish_frame(plan, observations, std::nullopt);
      expect_true(output.accepted == confirming && output.reacquired == confirming &&
                      output.control_valid && output.motion_control_authorized == !confirming &&
                      output.mode == (confirming ? TrackingMode::kTracking
                                                : TrackingMode::kOcclusionHold),
                  "only the consistent confirm changes from motion coast to accepted tracking");
      if (age == search_age) {
        expect_true(output.bbox.x < 42 && output.bbox.width == 20 &&
                        output.occlusion_probe_confirmation_hits == 1,
                    "search hit stays isolated from the motion control box");
      }
    }
    expect_true(output.bbox.x == 44 && output.bbox.width == 20,
                "confirm publishes the recovered NanoTrack box immediately");
    plan = coast.begin_frame(30, 1200, true);
    expect_true(plan.mode == TrackingMode::kTracking &&
                    plan.occlusion_coast_age_frames == 0 &&
                    plan.occlusion_probe_type == OcclusionProbeType::kNone &&
                    plan.predicted_bbox.x == 44 && !plan.request_detector,
                "early recovery resets velocity, probe schedule and loss time");
    coast.finish_frame(plan, reliable_pair_for_plan(plan, recovered, 30), std::nullopt);
  }

  for (std::int64_t timestamp : {1400, 4000}) {
    LongTermController late(config);
    late.initialize(initial, 0, 0, frame_size);
    for (int frame = 1; frame <= 8; ++frame)
      step(late, frame, frame * 40, initial);
    step(late, 9, 360, std::nullopt);
    const SearchPlan tick = late.begin_frame(10, timestamp, false);
    output = late.finish_frame(
        tick, {weak_for_plan(tick, BBox{43, 30, 14, 20}, 10)}, std::nullopt);
    expect_true(!output.occlusion_hold_triggered && !output.control_valid,
                "assisted or lost recovery cannot newly authorize a coast");
  }

  if (failures != 0) {
    std::cerr << failures << " replay test(s) failed\n";
    return 1;
  }
  std::cout << "long-term replay tests passed\n";
  return 0;
}
