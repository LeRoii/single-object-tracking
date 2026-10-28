#include "long_term_controller.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
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

}  // namespace

int main() {
  using namespace nanotrack;

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
  subtarget_config.detector_candidate_max_center_ratio = 1.0F;
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
  boundary_gate.initialize(gate_initial, 0, 0, gate_frame);
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
  rejected_gate.initialize(gate_initial, 0, 0, gate_frame);
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
  mixed_gate.initialize(gate_initial, 0, 0, gate_frame);
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
  lost_gate.initialize(gate_initial, 0, 0, gate_frame);
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

  const auto feed_reliable_box = [](LongTermController& controller,
                                    std::uint64_t frame_id,
                                    const BBox& bbox) {
    const SearchPlan reliable_plan =
        controller.begin_frame(frame_id, frame_id * 40, false);
    return controller.finish_frame(
        reliable_plan,
        {reliable_observation(reliable_plan, TemplateKind::kAnchor, bbox,
                              frame_id)},
        std::nullopt);
  };
  const auto feed_weak_box = [](LongTermController& controller,
                                std::uint64_t frame_id,
                                const BBox& bbox) {
    const SearchPlan weak_plan =
        controller.begin_frame(frame_id, frame_id * 40, false);
    return controller.finish_frame(
        weak_plan,
        {weak_observation(weak_plan, TemplateKind::kAnchor, bbox, frame_id)},
        std::nullopt);
  };

  ControllerConfig hold_config = config;
  hold_config.uncertain_ms = 1000;
  hold_config.local_recovery_ms = 2000;
  hold_config.assisted_recovery_ms = 3000;

  LongTermController incomplete_reference(hold_config);
  incomplete_reference.initialize(BBox{60, 40, 20, 20}, 0, 0,
                                  FrameSize{240, 160});
  for (std::uint64_t frame = 1; frame <= 7; ++frame) {
    expect_true(feed_reliable_box(incomplete_reference, frame,
                                  BBox{60, 40, 20, 20})
                    .accepted,
                "reliable pre-occlusion sample is accepted");
  }
  feed_weak_box(incomplete_reference, 8, BBox{63, 43, 14, 14});
  output = feed_weak_box(incomplete_reference, 9, BBox{63, 43, 14, 14});
  expect_true(!output.occlusion_hold_triggered &&
                  incomplete_reference.mode() != TrackingMode::kOcclusionHold,
              "fewer than a full reliable reference window cannot trigger hold");

  LongTermController median_boundary(hold_config);
  median_boundary.initialize(BBox{70, 50, 20, 40}, 0, 0,
                             FrameSize{300, 220});
  const float reference_widths[] = {16, 18, 20, 22, 24, 26, 28, 30};
  for (std::uint64_t index = 0; index < 8; ++index) {
    const float width = reference_widths[index];
    feed_reliable_box(median_boundary, index + 1,
                      BBox{80.0F - 0.5F * width, 50, width, 40});
  }
  output = feed_weak_box(median_boundary, 9,
                         BBox{71.95F, 56, 16.10F, 28.0F});
  expect_true(!output.occlusion_hold_triggered,
              "one unreliable frame cannot trigger hold");
  output = feed_weak_box(median_boundary, 10,
                         BBox{71.95F, 56, 16.10F, 28.0F});
  expect_true(output.occlusion_hold_triggered &&
                  output.mode == TrackingMode::kOcclusionHold &&
                  output.occlusion_hold_remaining_frames == 12 &&
                  output.occlusion_consecutive_unreliable_frames == 2 &&
                  output.occlusion_observation_width ==
                      std::optional<float>(16.10F) &&
                  output.occlusion_observation_height ==
                      std::optional<float>(28.0F),
              "second unreliable observation at the inclusive ratio boundary triggers hold");
  expect_true(output.occlusion_reference_width.has_value() &&
                  output.occlusion_reference_height.has_value(),
              "trigger exposes the robust scale reference");
  if (output.occlusion_reference_width && output.occlusion_reference_height) {
    expect_near(*output.occlusion_reference_width, 23.0F, 1e-5F,
                "even reliable window averages the two middle widths");
    expect_near(*output.occlusion_reference_height, 40.0F, 1e-5F,
                "even reliable window averages the two middle heights");
  }
  if (output.occlusion_width_ratio && output.occlusion_height_ratio) {
    expect_near(*output.occlusion_width_ratio, 0.70F, 1e-5F,
                "width shrink ratio is measured against the robust reference");
    expect_near(*output.occlusion_height_ratio, 0.70F, 1e-5F,
                "height shrink ratio is measured against the robust reference");
  } else {
    expect_true(false, "trigger exposes both observed shrink ratios");
  }

  LongTermController above_boundary(hold_config);
  above_boundary.initialize(BBox{60, 40, 20, 20}, 0, 0,
                            FrameSize{240, 160});
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    feed_reliable_box(above_boundary, frame, BBox{60, 40, 20, 20});
  }
  feed_weak_box(above_boundary, 9, BBox{62.999F, 42.999F, 14.002F, 14.002F});
  output = feed_weak_box(above_boundary, 10,
                         BBox{62.999F, 42.999F, 14.002F, 14.002F});
  expect_true(!output.occlusion_hold_triggered &&
                  above_boundary.mode() != TrackingMode::kOcclusionHold,
              "a minimum dimension ratio of 0.7001 does not trigger hold");

  LongTermController no_observation_counter(hold_config);
  no_observation_counter.initialize(BBox{60, 40, 20, 20}, 0, 0,
                                    FrameSize{240, 160});
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    feed_reliable_box(no_observation_counter, frame, BBox{60, 40, 20, 20});
  }
  output = reject_frame(no_observation_counter, 9, 360);
  expect_true(!output.occlusion_hold_triggered,
              "a no-observation frame cannot trigger hold by itself");
  output = feed_weak_box(no_observation_counter, 10,
                         BBox{63, 43, 14, 14});
  expect_true(output.occlusion_hold_triggered,
              "a no-observation frame still counts toward consecutive unreliability");

  LongTermController fast_translation(hold_config);
  fast_translation.initialize(BBox{60, 40, 20, 20}, 0, 0,
                              FrameSize{240, 160});
  for (std::uint64_t frame = 1; frame <= 8; ++frame) {
    feed_reliable_box(fast_translation, frame, BBox{60, 40, 20, 20});
  }
  feed_weak_box(fast_translation, 9, BBox{85, 40, 20, 20});
  output = feed_weak_box(fast_translation, 10, BBox{85, 40, 20, 20});
  expect_true(!output.occlusion_hold_triggered &&
                  fast_translation.mode() != TrackingMode::kOcclusionHold,
              "fast translation at stable scale does not trigger occlusion hold");

  ControllerConfig diagnostic_config = hold_config;
  diagnostic_config.occlusion_reference_window = 3;
  diagnostic_config.occlusion_hold_frames = 1;
  diagnostic_config.uncertain_ms = 100;
  diagnostic_config.local_recovery_ms = 200;
  diagnostic_config.assisted_recovery_ms = 1000;
  diagnostic_config.detector_max_candidates = 1;
  diagnostic_config.detector_candidate_max_center_ratio = 0.20F;
  LongTermController diagnostics(diagnostic_config);
  diagnostics.initialize(BBox{60, 40, 20, 20}, 0, 0, FrameSize{240, 160});
  for (std::uint64_t frame = 1; frame <= 3; ++frame) {
    feed_reliable_box(diagnostics, frame, BBox{60, 40, 20, 20});
  }
  feed_weak_box(diagnostics, 4, BBox{63, 43, 14, 14});
  output = feed_weak_box(diagnostics, 5, BBox{63, 43, 14, 14});
  expect_true(output.occlusion_hold_triggered,
              "diagnostic fixture enters hold before detector recovery");
  plan = diagnostics.begin_frame(6, 240, true);
  DetectorBatch during_hold_batch;
  during_hold_batch.source_frame_id = 5;
  during_hold_batch.source_timestamp_ms = 200;
  during_hold_batch.generation = 0;
  during_hold_batch.candidates = {{BBox{65, 45, 20, 20}, 0.9F, 0}};
  output = diagnostics.finish_frame(plan, {}, during_hold_batch);
  expect_true(plan.mode == TrackingMode::kOcclusionHold,
              "single configured complete hold frame is explicit");
  expect_true(output.detector_candidate_diagnostics.size() == 1 &&
                  output.detector_candidate_diagnostics[0].rejection ==
                      DetectorCandidateRejection::kHoldInvalidated &&
                  !output.detector_candidate_diagnostics[0].queued &&
                  !output.control_valid,
              "late pre-hold batch is drained for diagnostics during hold only");
  plan = diagnostics.begin_frame(7, 1200, true);
  expect_true(plan.request_detector && plan.detector_request.has_value(),
              "post-hold assisted recovery schedules a new generation");
  DetectorBatch invalidated_batch;
  invalidated_batch.source_frame_id = plan.frame_id;
  invalidated_batch.source_timestamp_ms = plan.timestamp_ms;
  invalidated_batch.generation = plan.detector_request->generation - 1;
  invalidated_batch.candidates = {{BBox{65, 45, 20, 20}, 0.9F, 0}};
  output = diagnostics.finish_frame(plan, {}, invalidated_batch);
  expect_true(output.detector_candidate_diagnostics.size() == 1 &&
                  !output.detector_candidate_diagnostics[0].batch_eligible &&
                  !output.detector_candidate_diagnostics[0].queued &&
                  output.detector_candidate_diagnostics[0].drawable &&
                  output.detector_candidate_diagnostics[0].rejection ==
                      DetectorCandidateRejection::kHoldInvalidated &&
                  !output.control_valid,
              "pre-hold generation remains diagnostic-only after hold exits");

  plan = diagnostics.begin_frame(8, 1800, true);
  expect_true(plan.request_detector && plan.detector_request.has_value(),
              "next detector interval provides a current-generation request");
  DetectorBatch mixed_diagnostics;
  mixed_diagnostics.source_frame_id = plan.frame_id;
  mixed_diagnostics.source_timestamp_ms = plan.timestamp_ms;
  mixed_diagnostics.generation = plan.detector_request->generation;
  mixed_diagnostics.candidates = {
      {BBox{61, 41, 20, 20}, 0.90F, 0},
      {BBox{200, 120, 20, 20}, 0.80F, 0},
      {BBox{70, 45, 20, 20}, 0.70F, 1},
      {BBox{std::numeric_limits<float>::quiet_NaN(), 0, 20, 20}, 0.60F, 0},
  };
  output = diagnostics.finish_frame(plan, {}, mixed_diagnostics);
  expect_true(output.detector_candidate_diagnostics.size() == 4,
              "every post-NMS candidate receives one canonical diagnostic");
  if (output.detector_candidate_diagnostics.size() == 4) {
    const auto& pass = output.detector_candidate_diagnostics[0];
    const auto& position_reject = output.detector_candidate_diagnostics[1];
    const auto& capacity_reject = output.detector_candidate_diagnostics[2];
    const auto& invalid = output.detector_candidate_diagnostics[3];
    expect_true(pass.batch_eligible && pass.position_gate_pass && pass.queued &&
                    pass.drawable && pass.candidate_id.has_value() &&
                    pass.rejection == DetectorCandidateRejection::kNone,
                "nearest eligible candidate is queued with an assigned id");
    expect_true(position_reject.batch_eligible &&
                    !position_reject.position_gate_pass &&
                    !position_reject.queued && position_reject.drawable &&
                    position_reject.rejection ==
                        DetectorCandidateRejection::kPositionRejected,
                "large position jump remains drawable but cannot queue");
    expect_true(capacity_reject.batch_eligible &&
                    capacity_reject.position_gate_pass &&
                    !capacity_reject.queued && capacity_reject.drawable &&
                    capacity_reject.rejection ==
                        DetectorCandidateRejection::kCapacityRejected,
                "top-N exclusion has a distinct canonical reason");
    expect_true(!invalid.batch_eligible && !invalid.position_gate_pass &&
                    !invalid.queued && !invalid.drawable &&
                    invalid.rejection == DetectorCandidateRejection::kInvalid,
                "non-finite geometry is text-only and never control eligible");
  }

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "long-term controller tests passed\n";
  return 0;
}
