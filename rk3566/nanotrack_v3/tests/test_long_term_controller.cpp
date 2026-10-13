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
  plan = detector_controller.begin_frame(4, 1600, true);
  expect_true(plan.request_detector, "detector interval permits next request");
  DetectorBatch stale{4, 1400, {{BBox{10, 10, 20, 20}, 0.9F, 1}}};
  output = detector_controller.finish_frame(plan, {}, stale);
  expect_true(output.detector_status == DetectorBatchStatus::kStale,
              "detector batch older than 150 ms is rejected");

  plan = detector_controller.begin_frame(5, 2100, true);
  expect_true(plan.request_detector, "later detector request is scheduled");
  DetectorBatch valid{5,
                      2100,
                      {{BBox{10, 10, 20, 20}, 0.7F, 1},
                       {BBox{30, 10, 20, 20}, 0.95F, 1},
                       {BBox{50, 10, 20, 20}, 0.8F, 1},
                       {BBox{70, 10, 20, 20}, 0.99F, 1}}};
  output = detector_controller.finish_frame(plan, {}, valid);
  expect_true(output.detector_status == DetectorBatchStatus::kAccepted &&
                  output.detector_candidates_queued == 3,
              "valid detector batch is confidence-sorted and capped at three");
  plan = detector_controller.begin_frame(6, 2140, true);
  expect_true(plan.hypothesis_source == HypothesisSource::kDetector,
              "next frame verifies one queued detector hypothesis");
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
