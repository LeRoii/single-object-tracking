#include "long_term_runtime.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
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

class FakeTracker final : public nanotrack::ShortTermTracker {
 public:
  void initialize(const nanotrack::ImageView&,
                  const nanotrack::BBox& bbox) override {
    initialized = true;
    committed_bbox = bbox;
  }

  std::vector<nanotrack::TrackerObservation> infer(
      const nanotrack::ImageView&,
      const nanotrack::SearchPlan& plan) override {
    ++infer_calls;
    ++search_token;
    if (!emit_reliable) return {};
    const nanotrack::ResponseMetrics metrics{0.92F, 0.90F, 10.0F, 12.0F,
                                              0.20F};
    return {
        nanotrack::TrackerObservation{plan.frame_id, plan.timestamp_ms,
                                      plan.predicted_bbox, metrics,
                                      nanotrack::TemplateKind::kAnchor,
                                      search_token},
        nanotrack::TrackerObservation{plan.frame_id, plan.timestamp_ms,
                                      plan.predicted_bbox, metrics,
                                      nanotrack::TemplateKind::kRecent,
                                      search_token},
    };
  }

  void commit_bbox(const nanotrack::BBox& bbox) override {
    ++commit_calls;
    committed_bbox = bbox;
  }
  void stage_recent(const nanotrack::ImageView&,
                    const nanotrack::BBox&) override {
    ++stage_calls;
  }
  void commit_recent() override { ++recent_commit_calls; }
  void rollback_recent() override { ++rollback_calls; }

  bool initialized = false;
  bool emit_reliable = true;
  int infer_calls = 0;
  int commit_calls = 0;
  int stage_calls = 0;
  int recent_commit_calls = 0;
  int rollback_calls = 0;
  std::uint64_t search_token = 0;
  nanotrack::BBox committed_bbox{};
};

class FakeDetector final : public nanotrack::DetectorAdapter {
 public:
  bool available() const override { return enabled; }
  bool can_submit() const override { return enabled && ready; }
  void submit(const nanotrack::ImageView&,
              const nanotrack::DetectorRequest& request) override {
    if (!can_submit()) throw std::logic_error("fake detector is busy");
    ++submit_calls;
    last_request = request;
    ready = false;
  }
  std::optional<nanotrack::DetectorBatch> poll() override {
    ++poll_calls;
    if (fail_next_poll) {
      fail_next_poll = false;
      enabled = false;
      throw std::runtime_error("fake detector inference failed");
    }
    auto result = next_batch;
    next_batch.reset();
    if (result) ready = true;
    return result;
  }

  bool enabled = false;
  bool ready = true;
  bool fail_next_poll = false;
  int submit_calls = 0;
  int poll_calls = 0;
  nanotrack::DetectorRequest last_request{};
  std::optional<nanotrack::DetectorBatch> next_batch;
};

}  // namespace

int main() {
  using namespace nanotrack;

  std::vector<std::uint8_t> pixels(200 * 120 * 3, 0);
  const ImageView image{pixels.data(), 200, 120, 200 * 3, 3};
  ControllerConfig config;
  FakeTracker tracker;
  FakeDetector detector;
  LongTermRuntime runtime(config, tracker, detector);
  runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  expect_true(tracker.initialized, "runtime initializes short-term tracker");

  RuntimeFrameResult result = runtime.process_frame(image, 1, 40);
  expect_true(tracker.infer_calls == 1,
              "one frame invokes exactly one short-term search operation");
  expect_true(result.observations.size() == 2 &&
                  result.observations[0].search_token ==
                      result.observations[1].search_token,
              "anchor and recent observations share one search token");
  expect_true(result.output.accepted && tracker.commit_calls == 1,
              "accepted controller output commits tracker state");

  tracker.emit_reliable = false;
  result = runtime.process_frame(image, 2, 80);
  expect_true(!result.output.accepted && tracker.commit_calls == 1,
              "unaccepted output cannot commit tracker state");

  tracker.emit_reliable = true;
  runtime.process_frame(image, 3, 120);
  result = runtime.process_frame(image, 4, 160);
  expect_true(result.output.reacquired && tracker.commit_calls == 2,
              "confirmed recovery commits exactly once");

  FakeTracker template_tracker;
  NullDetectorAdapter null_detector;
  LongTermRuntime template_runtime(config, template_tracker, null_detector);
  template_runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  for (int index = 1; index <= 15; ++index) {
    result = template_runtime.process_frame(image, index, index * 100);
  }
  expect_true(result.output.template_action == TemplateAction::kStageRecent &&
                  template_tracker.stage_calls == 1,
              "runtime stages recent template on policy action");
  for (int index = 16; index <= 18; ++index) {
    result = template_runtime.process_frame(image, index, index * 100);
  }
  expect_true(result.output.template_action == TemplateAction::kCommitRecent &&
                  template_tracker.recent_commit_calls == 1,
              "runtime commits recent template after probation");

  FakeTracker rollback_tracker;
  LongTermRuntime rollback_runtime(config, rollback_tracker, null_detector);
  rollback_runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  for (int index = 1; index <= 15; ++index) {
    rollback_runtime.process_frame(image, index, index * 100);
  }
  rollback_tracker.emit_reliable = false;
  result = rollback_runtime.process_frame(image, 16, 1600);
  expect_true(result.output.template_action == TemplateAction::kRollbackRecent &&
                  rollback_tracker.rollback_calls == 1,
              "runtime rolls recent template back on policy action");

  FakeTracker detector_tracker;
  FakeDetector active_detector;
  active_detector.enabled = true;
  LongTermRuntime detector_runtime(config, detector_tracker, active_detector);
  detector_runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  detector_tracker.emit_reliable = false;
  detector_runtime.process_frame(image, 1, 100);
  DetectorBatch immediate_batch;
  immediate_batch.source_frame_id = 2;
  immediate_batch.source_timestamp_ms = 1100;
  immediate_batch.candidates = {{BBox{60, 30, 20, 20}, 0.9F, 1}};
  active_detector.next_batch = immediate_batch;
  result = detector_runtime.process_frame(image, 2, 1100);
  expect_true(active_detector.submit_calls == 1 &&
                  result.output.detector_status == DetectorBatchStatus::kAccepted,
              "runtime submits and polls detector only when policy requests it");

  ControllerConfig recovery_config = config;
  recovery_config.uncertain_ms = 10;
  recovery_config.local_recovery_ms = 20;
  recovery_config.assisted_recovery_ms = 30;
  recovery_config.detector_interval_ms = 20;
  FakeTracker busy_tracker;
  FakeDetector busy_detector;
  busy_detector.enabled = true;
  LongTermRuntime busy_runtime(recovery_config, busy_tracker, busy_detector);
  busy_runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  busy_tracker.emit_reliable = false;
  busy_runtime.process_frame(image, 1, 10);
  RuntimeFrameResult submitted = busy_runtime.process_frame(image, 2, 50);
  expect_true(submitted.plan.request_detector &&
                  submitted.detector.request_submitted &&
                  submitted.detector.request_frame_id == 2,
              "ready detector records one accepted request identity");
  RuntimeFrameResult while_busy = busy_runtime.process_frame(image, 3, 80);
  expect_true(!while_busy.plan.request_detector &&
                  busy_detector.submit_calls == 1,
              "busy detector suppresses new controller requests");
  while_busy = busy_runtime.process_frame(image, 4, 110);
  expect_true(!while_busy.plan.request_detector &&
                  busy_detector.submit_calls == 1,
              "busy detector does not advance request identity over intervals");
  DetectorBatch delayed_batch;
  delayed_batch.source_frame_id = 2;
  delayed_batch.source_timestamp_ms = 50;
  delayed_batch.raw_candidate_count = 4;
  delayed_batch.filtered_candidate_count = 1;
  delayed_batch.timing = DetectorTiming{1.0F, 7.0F, 2.0F, 10.0F};
  delayed_batch.candidates = {{BBox{70, 35, 20, 20}, 0.8F, 0}};
  busy_detector.next_batch = delayed_batch;
  RuntimeFrameResult completed = busy_runtime.process_frame(image, 5, 140);
  expect_true(busy_detector.submit_calls == 1 &&
                  completed.output.detector_status == DetectorBatchStatus::kAccepted,
              "delayed batch keeps original request identity without resubmission");
  expect_true(completed.detector.batch_received &&
                  completed.detector.batch_frame_id == 2 &&
                  completed.detector.batch_age_ms == 90 &&
                  completed.detector.raw_candidate_count == 4 &&
                  completed.detector.filtered_candidate_count == 1 &&
                  completed.detector.timing.total_ms == 10.0F,
              "runtime exposes detector batch diagnostics without changing policy");

  FakeTracker error_tracker;
  FakeDetector error_detector;
  error_detector.enabled = true;
  LongTermRuntime error_runtime(recovery_config, error_tracker, error_detector);
  error_runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  error_tracker.emit_reliable = false;
  error_runtime.process_frame(image, 1, 10);
  error_detector.fail_next_poll = true;
  RuntimeFrameResult detector_error = error_runtime.process_frame(image, 2, 50);
  expect_true(!detector_error.detector.error.empty() &&
                  detector_error.detector.error ==
                      "fake detector inference failed" &&
                  error_tracker.infer_calls == 2,
              "detector poll failure is recorded while NanoTrack continues");
  RuntimeFrameResult after_error = error_runtime.process_frame(image, 3, 80);
  expect_true(!after_error.plan.request_detector &&
                  error_detector.submit_calls == 1 &&
                  error_tracker.infer_calls == 3,
              "disabled detector remains out of later NanoTrack frames");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "long-term runtime tests passed\n";
  return 0;
}
