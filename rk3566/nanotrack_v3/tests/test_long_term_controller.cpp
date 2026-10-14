#include "long_term_controller.hpp"

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

}  // namespace

int main() {
  using namespace nanotrack;

  ControllerConfig config;
  LongTermController boundaries(config);
  boundaries.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120});
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
  normal.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120});
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
  expect_true(output.control_valid && output.accepted && output.reacquired,
              "second consistent candidate in three frames reacquires");

  LongTermController conflict(config);
  conflict.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120});
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
  recent_only.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120});
  plan = recent_only.begin_frame(1, 40, false);
  output = recent_only.finish_frame(
      plan,
      {rejected_observation(plan, TemplateKind::kAnchor, plan.predicted_bbox, 10),
       reliable_observation(plan, TemplateKind::kRecent, plan.predicted_bbox, 10)},
      std::nullopt);
  expect_true(!output.control_valid,
              "recent-only reliable response cannot directly control");

  LongTermController detector_controller(config);
  detector_controller.initialize(BBox{40, 30, 20, 20}, 0, 0,
                                 FrameSize{200, 120});
  reject_frame(detector_controller, 1, 100);
  plan = detector_controller.begin_frame(2, 1100, true);
  expect_true(plan.request_detector,
              "assisted recovery schedules first detector request");
  DetectorBatch mismatched{999, 1100, {{BBox{10, 10, 20, 20}, 0.9F, 1}}};
  output = detector_controller.finish_frame(plan, {}, mismatched);
  expect_true(output.detector_status == DetectorBatchStatus::kFrameMismatch,
              "mismatched detector request frame is rejected");

  plan = detector_controller.begin_frame(3, 1200, true);
  expect_true(!plan.request_detector,
              "detector requests are limited to once per 500 ms");
  detector_controller.finish_frame(plan, {}, std::nullopt);

  ControllerConfig lock_config = config;
  LongTermController candidate_lock(lock_config);
  candidate_lock.initialize(BBox{40, 30, 20, 20}, 0, 0,
                            FrameSize{200, 120});
  reject_frame(candidate_lock, 1, 100);
  plan = candidate_lock.begin_frame(2, 1100, true);
  DetectorBatch lock_batch{
      2, 1100,
      {{BBox{20, 20, 30, 40}, 0.95F, 0},
       {BBox{120, 20, 30, 40}, 0.90F, 1}}};
  output = candidate_lock.finish_frame(plan, {}, lock_batch);
  expect_true(output.detector_candidates_queued == 2,
              "whole-object candidates are queued before verification");

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
                  output.detector_confirmation_hits == 2 &&
                  output.detector_confirmation_attempts == 3,
              "two agreeing hits in three locked frames reacquire");

  LongTermController same_class_ambiguity(lock_config);
  same_class_ambiguity.initialize(BBox{40, 30, 20, 20}, 0, 0,
                                  FrameSize{200, 120});
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
  LongTermController subtarget(subtarget_config);
  subtarget.initialize(BBox{40, 30, 20, 10}, 0, 0, FrameSize{200, 120});
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
  edge_subtarget.initialize(BBox{40, 30, 20, 10}, 0, 0,
                            FrameSize{200, 120});
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
  opposite_edge_subtarget.initialize(BBox{40, 30, 20, 30}, 0, 0,
                                     FrameSize{200, 120});
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
  strict_initial_geometry.initialize(BBox{40, 30, 20, 20}, 0, 0,
                                     FrameSize{200, 120});
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
  strict_geometry.initialize(BBox{40, 30, 20, 20}, 0, 0,
                             FrameSize{200, 120});
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
  detector_identity.initialize(BBox{40, 30, 20, 20}, 0, 0,
                               FrameSize{200, 120});
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
                      {{BBox{10, 10, 20, 20}, 0.7F, 1},
                       {BBox{30, 10, 20, 20}, 0.95F, 1},
                       {BBox{50, 10, 20, 20}, 0.8F, 1},
                       {BBox{70, 10, 20, 20}, 0.99F, 1}}};
  output = detector_controller.finish_frame(plan, {}, valid);
  expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                  output.detector_candidates_queued == 3,
              "valid detector batch is confidence-sorted and capped at three");
  plan = detector_controller.begin_frame(7, 2140, true);
  expect_true(plan.hypothesis_source == HypothesisSource::kDetector,
              "next frame verifies one queued detector hypothesis");
  expect_true(plan.detector_candidate_class_id == std::optional<int>(1) &&
                  plan.detector_candidate_confidence ==
                      std::optional<float>(0.99F),
              "detector hypothesis preserves candidate identity for audit logs");
  expect_near(plan.predicted_bbox.x, 70.0F, 1e-6F,
              "highest-confidence detector candidate is verified first");
  detector_controller.finish_frame(plan, {}, std::nullopt);

  ControllerConfig template_config = config;
  LongTermController templates(template_config);
  templates.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120});
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
  rollback.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120});
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
  degraded_recent.initialize(BBox{40, 30, 20, 20}, 0, 0,
                             FrameSize{200, 120});
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
  bounded.initialize(BBox{40, 30, 20, 20}, 0, 0, FrameSize{200, 120});
  reject_frame(bounded, 1, 40);
  for (int index = 2; index <= 10001; ++index) {
    plan = bounded.begin_frame(static_cast<std::uint64_t>(index), index * 40,
                               false);
    output = bounded.finish_frame(plan, {}, std::nullopt);
  }
  expect_true(output.mode == TrackingMode::kLost && !output.control_valid &&
                  output.detector_candidates_queued == 0,
              "long detector-free loss remains bounded and invalid");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "long-term controller tests passed\n";
  return 0;
}
