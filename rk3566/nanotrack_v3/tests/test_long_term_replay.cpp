#include "resolved_controller_fixture.hpp"
#include "target_class.hpp"

#include <algorithm>
#include <cmath>
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

void test_stable_size_occlusion_replay() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.occlusion_min_reference_samples = 3;
  config.suspicion_enter_ms = 80;
  config.uncertain_ms = 120;
  config.motion_search_confidence_ms = 180;
  config.stable_frames_for_update = 50;
  const BBox initial{70, 50, 30, 30};
  ResolvedControllerFixture controller(config);
  controller.initialize(initial, 0, 0, {320, 240});
  std::uint64_t frame = 0;
  const auto track = [&](const BBox& bbox, const ResponseMetrics& metrics) {
    ++frame;
    const auto plan = controller.begin_frame(frame, frame * 40, false);
    auto recent = reliable_for_plan(plan, bbox, frame, TemplateKind::kRecent);
    recent.metrics = metrics;
    std::vector<TrackerObservation> observations{recent};
    if (plan.template_run_mode == TemplateRunMode::kRecentAndSafe) {
      observations.push_back(reliable_for_plan(plan, initial, frame, TemplateKind::kSafe));
    }
    return controller.finish_frame(plan, observations, std::nullopt);
  };
  const ResponseMetrics healthy{0.98F, 0.96F, 20, 30, 0.28F};
  const ResponseMetrics degraded{0.98F, 0.96F, 11, 16, 0.15F};
  for (int step = 1; step <= config.occlusion_min_reference_samples; ++step) {
    const auto out = track({initial.x + step, initial.y, initial.width, initial.height}, healthy);
    expect_true(out.trusted_for_state_update && out.control_valid,
                "a stable-size moving target establishes trusted response history");
  }
  const BBox occluded{77, 50, 30, 30};
  const auto tentative = track(occluded, degraded);
  expect_true(tentative.tracking_phase == TrackingPhase::kSuspected &&
                  tentative.observation_present && !tentative.trusted_for_state_update &&
                  !tentative.accepted && tentative.control_valid &&
                  tentative.control_source == ControlSource::kObservation &&
                  tentative.tracking_bbox.x == occluded.x &&
                  tentative.tracking_bbox.width == initial.width &&
                  tentative.template_action == TemplateAction::kNone,
              "stable-size high-score degradation publishes the recent box without learning");
  bool entered_suspected = false;
  bool expired = false;
  bool searched = false;
  for (int step = 0; step < 6; ++step) {
    const auto out = track(occluded, degraded);
    entered_suspected |= out.mode == TrackingMode::kSuspected;
    expired |= !out.control_valid;
    searched |= out.tracking_phase == TrackingPhase::kSearching;
    expect_true(!out.accepted && !out.trusted_for_state_update &&
                    out.template_action == TemplateAction::kNone &&
                    (!expired || (!out.control_valid && out.control_source == ControlSource::kNone)) &&
                    (!searched || !out.observation_present),
                "sustained degradation freezes learning and never renews expired control");
  }
  expect_true(entered_suspected && expired && searched,
              "sustained stable-size occlusion enters suspicion and search and revokes control");
}

void test_viewpoint_and_turn_replay() {
  using namespace nanotrack;
  ControllerConfig config;
  config.stable_frames_for_update = 4;
  config.recent_update_interval_ms = 1;
  config.recent_probation_frames = 2;
  config.motion_scale_rate_limit_per_s = 0.60F;
  config.max_accepted_scale_step_ratio = 0.10F;
  const BBox initial{80, 75, 20, 20};
  ResolvedControllerFixture controller(config);
  controller.initialize(initial, 0, 0, {320, 240});
  int rotations = 0;
  float final_width = 0.0F;
  for (std::uint64_t frame = 1; frame <= 24; ++frame) {
    const auto plan = controller.begin_frame(frame, frame * 100, false);
    const auto step = static_cast<float>(frame);
    const bool turning = frame > 12;
    const auto turn_step = turning ? step - 12.0F : 0.0F;
    const BBox recent_bbox{initial.x + std::min(step, 12.0F) * 2.0F - turn_step,
                           initial.y + turn_step * 2.0F,
                           initial.width + step * 0.50F,
                           initial.height + std::min(step, 12.0F) * 0.40F - turn_step * 0.30F};
    auto recent = reliable_for_plan(plan, recent_bbox, frame, TemplateKind::kRecent);
    std::vector<TrackerObservation> observations{recent};
    if (plan.template_run_mode == TemplateRunMode::kStagedAndRecent) {
      auto staged = recent;
      staged.template_kind = TemplateKind::kStaged;
      observations.push_back(staged);
    }
    expect_true(plan.template_run_mode != TemplateRunMode::kRecentAndSafe,
                "normal viewpoint and large-angle turn do not wake the safe head");
    const auto out = controller.finish_frame(plan, observations, std::nullopt);
    expect_true(out.tracking_phase == TrackingPhase::kTracking &&
                    out.accepted && out.observation_present && out.trusted_for_state_update &&
                    out.control_valid && !out.recovery_conflict && !out.reacquired,
                "reliable recent geometry stays trusted through scale change and direction reversal");
    rotations += out.template_action == TemplateAction::kCommitRecent;
    final_width = out.tracking_bbox.width;
  }
  expect_true(rotations >= 2 && final_width > initial.width * 1.33F,
              "multiple probation commits preserve tracking beyond the initial scale envelope");
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

  ResolvedControllerFixture tracker_only(config);
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
                  out.template_action == TemplateAction::kNone && out.control_valid &&
                  out.recovery_candidate_key &&
                  out.recovery_candidate_key->source == RecoveryCandidateSource::kRecent &&
                  out.recovery_confirmation_hits == 1,
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
                  out.trusted_for_state_update && out.recovery_confirmation_hits == 2 &&
                  out.recovery_candidate_key &&
                  out.recovery_candidate_key->source == RecoveryCandidateSource::kRecent &&
                  out.selected_template == TemplateKind::kRecent &&
                  out.template_action == TemplateAction::kStageRecent &&
                  !plan.request_detector,
              "continuous recent identity recovers across one gap without detector or safe veto");

  ResolvedControllerFixture safe_only(config);
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

  ResolvedControllerFixture detector(config);
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

  ResolvedControllerFixture ambiguous(config);
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

void test_same_class_crossing_replay() {
  using namespace nanotrack;
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.motion_search_confidence_ms = 500;
  config.detector_candidate_max_center_ratio = 0.30F;
  const BBox initial{80, 50, 20, 20};
  ResolvedControllerFixture controller(config);
  controller.initialize(initial, 0, 0, {320, 240}, make_fixed_target_class(0, true));
  std::uint64_t frame = 0;
  const auto open = [&](bool detector) {
    ++frame;
    return controller.begin_frame(frame, frame * 30, detector);
  };
  for (float x : {84.0F, 88.0F, 92.0F}) {
    const auto plan = open(false);
    const auto out = controller.finish_frame(
        plan, {reliable_for_plan(plan, {x, 50, 20, 20}, frame, TemplateKind::kRecent)},
        std::nullopt);
    expect_true(out.trusted_for_state_update, "the approaching target has a trusted track");
  }
  const auto overlap = controller.finish_frame(open(false), {}, std::nullopt);
  expect_true(overlap.tracking_phase == TrackingPhase::kSearching && overlap.control_valid &&
                  overlap.control_source == ControlSource::kMotionSearch,
              "indistinguishable overlap keeps bounded motion control");

  const BBox target_after_crossing{112, 50, 20, 20};
  const BBox distractor_after_crossing{80, 50, 20, 20};
  auto plan = open(true);
  expect_true(plan.detector_request.has_value(), "search can ask for same-class candidates");
  if (!plan.detector_request) return;
  DetectorBatch batch{plan.frame_id, plan.timestamp_ms,
                      {{target_after_crossing, 0.80F, 0},
                       {distractor_after_crossing, 0.99F, 0}}};
  batch.generation = plan.detector_request->generation;
  const auto seeded = controller.finish_frame(plan, {}, batch);
  expect_true(!seeded.accepted && !seeded.observation_present &&
                  seeded.control_source == ControlSource::kMotionSearch,
              "separated detections cannot authorize either identity at the crossing");
  for (int step = 0; step < 3; ++step) {
    plan = open(false);
    const auto out = controller.finish_frame(
        plan, {reliable_for_plan(plan, distractor_after_crossing, frame, TemplateKind::kRecent)},
        std::nullopt);
    expect_true(out.recovery_conflict && out.recovery_confirmation_hits == 0 &&
                    out.tracking_phase == TrackingPhase::kSearching &&
                    !out.reacquired && !out.trusted_for_state_update &&
                    out.template_action == TemplateAction::kNone &&
                    out.control_source == ControlSource::kMotionSearch,
                "repeated high-score distractor hits do not resolve a same-class crossing");
  }
}

void test_delayed_detector_recovery_replay() {
  using namespace nanotrack;
  const BBox initial{40, 30, 20, 20};
  const BBox far{190, 60, 20, 20};
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.motion_search_confidence_ms = 80;
  config.detector_interval_ms = 100;
  config.detector_candidate_max_center_ratio = 0.02F;
  ResolvedControllerFixture controller(config);
  controller.initialize(initial, 0, 0, {320, 200}, make_fixed_target_class(0, true));
  step(controller, 1, 20, BBox{43, 30, 20, 20}, TemplateKind::kRecent);
  step(controller, 2, 40, BBox{46, 30, 20, 20}, TemplateKind::kRecent);
  step(controller, 3, 80, std::nullopt);
  auto plan = controller.begin_frame(4, 110, true);
  expect_true(plan.detector_request.has_value(), "search schedules the eligible detector");
  if (!plan.detector_request) return;
  const auto request = *plan.detector_request;
  const BBox request_motion = plan.motion_predicted_bbox;
  controller.finish_frame(plan, {}, std::nullopt);

  plan = controller.begin_frame(5, 140, true);
  const float projection_dx = plan.motion_predicted_bbox.x - request_motion.x;
  const float expected_x = far.x + projection_dx;
  DetectorBatch delayed{request.frame_id, request.timestamp_ms, {{far, 0.99F, 0}}};
  delayed.generation = request.generation;
  auto out = controller.finish_frame(plan, {}, delayed);
  expect_true(!plan.request_detector && out.detector_status == DetectorBatchStatus::kAccepted &&
                  out.detector_candidates_queued == 1 &&
                  out.detector_candidate_diagnostics.size() == 1 &&
                  out.detector_candidate_diagnostics[0].center_ratio.value_or(0.0F) > 0.02F &&
                  std::fabs(projection_dx) > 0.001F &&
                  out.detector_candidate_diagnostics[0].projected_bbox &&
                  std::fabs(out.detector_candidate_diagnostics[0].projected_bbox->x -
                            expected_x) < 0.001F &&
                  !out.accepted && !out.control_valid && !out.search_hint,
              "delayed far detection projects from the request frame without authorizing control");

  plan = controller.begin_frame(6, 170, true);
  const BBox probe_bbox = plan.predicted_bbox;
  expect_true(!plan.request_detector && plan.hypothesis_source == HypothesisSource::kDetector &&
                  std::fabs(probe_bbox.x - expected_x) < 0.001F,
              "next frame probes the projected far detector region at normal request cadence");
  out = controller.finish_frame(
      plan, {reliable_for_plan(plan, probe_bbox, 6, TemplateKind::kRecent)}, std::nullopt);
  expect_true(!out.accepted && !out.control_valid && out.recovery_confirmation_hits == 1,
              "far detector still requires two NanoTrack hits before control can resume");
  plan = controller.begin_frame(7, 200, true);
  out = controller.finish_frame(
      plan, {reliable_for_plan(plan, probe_bbox, 7, TemplateKind::kRecent)}, std::nullopt);
  expect_true(!plan.request_detector && out.reacquired && out.control_valid &&
                  out.recovery_confirmation_hits == 2,
              "far detector identity commits through the common NanoTrack gate");
}

void test_unified_subtarget_offset_replay() {
  using namespace nanotrack;
  const BBox initial{40, 30, 20, 20};
  const BBox detected_region{110, 60, 100, 80};
  const BBox offset_target{180, 90, 20, 20};
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.detector_interval_ms = 40;
  config.detector_candidate_max_center_ratio = 0.70F;
  config.detector_target_mode = DetectorTargetMode::kSubtarget;
  config.detector_subtarget_max_probes = 5;
  ResolvedControllerFixture controller(config);
  controller.initialize(initial, 0, 0, {400, 240}, make_fixed_target_class(0, true));
  step(controller, 1, 20, std::nullopt);
  auto plan = controller.begin_frame(2, 40, true);
  if (!plan.detector_request) {
    expect_true(false, "subtarget fixture has a real detector request");
    return;
  }
  const auto request = *plan.detector_request;
  DetectorBatch batch{request.frame_id, request.timestamp_ms, {{detected_region, 0.99F, 0}}};
  batch.generation = request.generation;
  controller.finish_frame(plan, {}, batch);

  constexpr float expected_centers[][2] = {
      {150, 90}, {150, 66}, {150, 114}, {120, 90}, {180, 90},
  };
  for (std::uint64_t frame = 3; frame <= 7; ++frame) {
    const auto index = static_cast<std::size_t>(frame - 3);
    plan = controller.begin_frame(frame, frame * 20, frame == 7);
    const bool at_target = plan.predicted_bbox.x == offset_target.x &&
                           plan.predicted_bbox.y == offset_target.y;
    expect_true(plan.hypothesis_source == HypothesisSource::kDetector &&
                    plan.template_run_mode == TemplateRunMode::kRecentAndSafe &&
                    plan.detector_probe_index == index &&
                    plan.detector_candidate_region &&
                    plan.detector_candidate_region->width == detected_region.width &&
                    plan.predicted_bbox.x == expected_centers[index][0] &&
                    plan.predicted_bbox.y == expected_centers[index][1] &&
                    plan.predicted_bbox.width == initial.width,
                "subtarget advances one deterministic trusted-size center per frame");
    std::vector<TrackerObservation> observations;
    if (at_target || frame == 3) {
      observations.push_back(reliable_for_plan(plan, offset_target, frame, TemplateKind::kRecent));
    }
    std::optional<DetectorBatch> refresh;
    if (frame == 7 && plan.detector_request) {
      const auto fresh_request = *plan.detector_request;
      BBox moved_region = detected_region;
      moved_region.x += 1;
      refresh = DetectorBatch{fresh_request.frame_id, fresh_request.timestamp_ms,
                              {{moved_region, 0.95F, 0}}};
      refresh->generation = fresh_request.generation;
    }
    const auto out = controller.finish_frame(plan, observations, refresh);
    if (frame == 7) {
      expect_true(refresh && out.detector_status == DetectorBatchStatus::kAccepted &&
                      out.detector_candidates_queued == 1,
                  "a fresh detector update preserves the selected offset and its identity");
    }
    expect_true(!out.accepted && !out.recovery_conflict &&
                    out.recovery_confirmation_hits == (frame == 7 ? 1U : 0U),
                "a response outside the chosen offset cannot count as a verification hit");
  }
  plan = controller.begin_frame(8, 160, false);
  const auto out = controller.finish_frame(
      plan, {reliable_for_plan(plan, offset_target, 8, TemplateKind::kRecent)}, std::nullopt);
  expect_true(plan.predicted_bbox.x == offset_target.x && plan.recovery_probe &&
                  plan.recovery_probe->source == RecoveryCandidateSource::kDetector &&
                  plan.search_hint && plan.search_hint->confirmation_count == 1 &&
                  out.reacquired && out.recovery_confirmation_hits == 2,
              "offset subtarget locks and confirms through the shared two-hit path");
}

void test_subtarget_recovery_identity_and_eligibility() {
  using namespace nanotrack;
  const BBox initial{40, 30, 20, 20};
  const BBox carrier{110, 60, 100, 80};
  const BBox center{150, 90, 20, 20};
  const BBox alternate{178, 90, 20, 20};
  const BBox top{150, 66, 20, 20};
  ControllerConfig config;
  config.motion_search_enabled = true;
  config.detector_target_mode = DetectorTargetMode::kSubtarget;
  config.detector_subtarget_max_probes = 2;
  config.detector_interval_ms = 20;
  const auto batch_for = [&](const SearchPlan& plan) {
    DetectorBatch batch{plan.frame_id, plan.timestamp_ms, {{carrier, 0.95F, 0}}};
    if (plan.detector_request) batch.generation = plan.detector_request->generation;
    return batch;
  };
  const auto start = [&](LongTermController& controller) {
    controller.initialize(initial, 0, 0, {400, 240}, make_fixed_target_class(0, true));
    step(controller, 1, 20, std::nullopt);
    const auto plan = controller.begin_frame(2, 40, true);
    expect_true(plan.detector_request.has_value(), "subtarget starts with an eligible detector request");
    return controller.finish_frame(plan, {}, batch_for(plan));
  };

  for (bool both_off_probe : {false, true}) {
    ResolvedControllerFixture conflicted(config);
    start(conflicted);
    auto plan = conflicted.begin_frame(3, 60, false);
    const auto recent_bbox = both_off_probe ? BBox{112, 62, 20, 20} : center;
    const auto safe_bbox = both_off_probe ? BBox{186, 110, 20, 20} : alternate;
    auto out = conflicted.finish_frame(
        plan, {reliable_for_plan(plan, recent_bbox, 3, TemplateKind::kRecent),
               reliable_for_plan(plan, safe_bbox, 3, TemplateKind::kSafe)}, std::nullopt);
    expect_true(!out.accepted && out.recovery_conflict && out.recovery_confirmation_hits == 0,
                "two incompatible reliable parts of one carrier latch conflict without a hit");
    const auto repeated_bbox = both_off_probe ? top : center;
    for (std::uint64_t frame = 4; frame <= 7; ++frame) {
      plan = conflicted.begin_frame(frame, frame * 20, false);
      out = conflicted.finish_frame(
          plan, {reliable_for_plan(plan, repeated_bbox, frame, TemplateKind::kRecent)}, std::nullopt);
      expect_true(!out.accepted && out.recovery_conflict &&
                      out.tracking_phase == TrackingPhase::kSearching,
                  "repeated recent-only responses cannot erase the carrier conflict");
    }
  }

  ControllerConfig age_config = config;
  age_config.detector_subtarget_max_probes = 1;
  age_config.search_reacquisition_min_age_ms = 60;
  ResolvedControllerFixture immature(age_config);
  start(immature);
  auto plan = immature.begin_frame(3, 60, false);
  auto out = immature.finish_frame(plan, {}, std::nullopt);
  expect_true(!plan.recovery_probe && !out.accepted,
              "an immature frame cannot use the single available detector offset");
  plan = immature.begin_frame(4, 80, false);
  out = immature.finish_frame(
      plan, {reliable_for_plan(plan, center, 4, TemplateKind::kRecent)}, std::nullopt);
  expect_true(plan.recovery_probe && plan.detector_probe_index == 0 &&
                  plan.predicted_bbox.x == center.x && !out.accepted &&
                  out.recovery_confirmation_hits == 1,
              "first eligible frame keeps the fresh single offset for verification");
  plan = immature.begin_frame(5, 100, false);
  out = immature.finish_frame(
      plan, {reliable_for_plan(plan, center, 5, TemplateKind::kRecent)}, std::nullopt);
  expect_true(out.reacquired && out.recovery_confirmation_hits == 2,
              "the preserved single offset can confirm only on the next NanoTrack frame");

  ControllerConfig refresh_config = config;
  refresh_config.detector_stale_ms = 60;
  ResolvedControllerFixture refreshed(refresh_config);
  start(refreshed);
  plan = refreshed.begin_frame(3, 70, false);
  refreshed.finish_frame(plan, {}, std::nullopt);
  plan = refreshed.begin_frame(4, 100, false);
  const auto first = refreshed.finish_frame(
      plan, {reliable_for_plan(plan, top, 4, TemplateKind::kRecent)}, std::nullopt);
  expect_true(plan.detector_probe_index == 1 && plan.detector_candidate_id &&
                  first.recovery_candidate_key && first.recovery_confirmation_hits == 1,
              "first offset hit occurs at the detector freshness limit");
  plan = refreshed.begin_frame(5, 120, true);
  out = refreshed.finish_frame(
      plan, {reliable_for_plan(plan, top, 5, TemplateKind::kRecent)}, batch_for(plan));
  expect_true(plan.detector_request && !plan.detector_candidate_id &&
                  plan.recovery_probe == first.recovery_candidate_key &&
                  plan.predicted_bbox.y == top.y && out.detector_status == DetectorBatchStatus::kAccepted &&
                  out.detector_candidate_diagnostics.size() == 1 && first.recovery_candidate_key &&
                  out.detector_candidate_diagnostics[0].candidate_id == first.recovery_candidate_key->id &&
                  !out.recovery_conflict && out.reacquired && out.recovery_confirmation_hits == 2,
              "fresh same-carrier detection merges with the verified offset after detector expiry");
}

}  // namespace

int main() {
  test_stable_size_occlusion_replay();
  test_viewpoint_and_turn_replay();
  test_recent_safe_recovery_replay();
  test_same_class_crossing_replay();
  test_delayed_detector_recovery_replay();
  test_unified_subtarget_offset_replay();
  test_subtarget_recovery_identity_and_eligibility();
  using namespace nanotrack;
  const BBox initial{40, 30, 20, 20};
  const FrameSize frame_size{240, 160};
  ControllerConfig config;

  ResolvedControllerFixture normal(config);
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
  ResolvedControllerFixture motion_search(motion_search_config);
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

  ResolvedControllerFixture assisted(config);
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

  ResolvedControllerFixture permanent(config);
  permanent.initialize(initial, 0, 0, frame_size,
      make_unknown_target_class(TargetClassReason::kNoMatch));
  step(permanent, 1, 40, std::nullopt);
  for (int frame = 2; frame <= 100; ++frame) {
    output = step(permanent, frame, frame * 40, std::nullopt);
  }
  expect_true(output.mode == TrackingMode::kLost && !output.control_valid,
              "permanent loss stays invalid without detector");

  ResolvedControllerFixture single_detector(config);
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

  ResolvedControllerFixture confirmed_detector(config);
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

  ResolvedControllerFixture filtered_detector(config);
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

  ResolvedControllerFixture invalid_detector(config);
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

  ResolvedControllerFixture same_class_distractor(config);
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

  ResolvedControllerFixture deformation(config);
  deformation.initialize(initial, 0, 0, frame_size,
      make_unknown_target_class(TargetClassReason::kNoMatch));
  const BBox deformed{35, 32, 30, 16};
  output = step(deformation, 1, 40, deformed);
  expect_true(output.accepted && output.control_valid,
              "bounded target deformation remains trackable");

  ResolvedControllerFixture distractor(config);
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
    ResolvedControllerFixture coast(coast_config);
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
    ResolvedControllerFixture coast(config);
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
    ResolvedControllerFixture late(config);
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
