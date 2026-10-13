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
  void submit(const nanotrack::ImageView&,
              const nanotrack::DetectorRequest& request) override {
    ++submit_calls;
    last_request = request;
  }
  std::optional<nanotrack::DetectorBatch> poll() override {
    ++poll_calls;
    auto result = next_batch;
    next_batch.reset();
    return result;
  }

  bool enabled = false;
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
  active_detector.next_batch = DetectorBatch{
      2, 1100, {{BBox{60, 30, 20, 20}, 0.9F, 1}}};
  result = detector_runtime.process_frame(image, 2, 1100);
  expect_true(active_detector.submit_calls == 1 &&
                  result.output.detector_status == DetectorBatchStatus::kAccepted,
              "runtime submits and polls detector only when policy requests it");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "long-term runtime tests passed\n";
  return 0;
}
