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
    if (!emit_reliable && !emit_weak) return {};
    const nanotrack::ResponseMetrics metrics = emit_weak
        ? nanotrack::ResponseMetrics{0.45F, 0.40F, 2.5F, 3.0F, 0.04F}
        : nanotrack::ResponseMetrics{0.92F, 0.90F, 10.0F, 12.0F, 0.20F};
    const nanotrack::BBox bbox = emit_weak ? weak_bbox : plan.predicted_bbox;
    return {
        nanotrack::TrackerObservation{plan.frame_id, plan.timestamp_ms,
                                      bbox, metrics,
                                      nanotrack::TemplateKind::kAnchor,
                                      search_token},
        nanotrack::TrackerObservation{plan.frame_id, plan.timestamp_ms,
                                      bbox, metrics,
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
  bool emit_weak = false;
  nanotrack::BBox weak_bbox{43, 33, 14, 14};
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

  FakeTracker stale_tracker;
  FakeDetector stale_detector;
  stale_detector.enabled = true;
  LongTermRuntime stale_runtime(recovery_config, stale_tracker, stale_detector);
  stale_runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  stale_tracker.emit_reliable = false;
  stale_runtime.process_frame(image, 1, 10);
  RuntimeFrameResult stale_submitted =
      stale_runtime.process_frame(image, 2, 50);
  expect_true(stale_submitted.detector.request_submitted,
              "slow detector replay submits the initial request");
  stale_runtime.process_frame(image, 3, 100);
  stale_runtime.process_frame(image, 4, 160);
  DetectorBatch stale_completion;
  stale_completion.source_frame_id = 2;
  stale_completion.source_timestamp_ms = 50;
  stale_completion.candidates = {{BBox{70, 35, 20, 20}, 0.9F, 0}};
  stale_detector.next_batch = stale_completion;
  RuntimeFrameResult stale_completed =
      stale_runtime.process_frame(image, 5, 220);
  expect_true(stale_detector.submit_calls == 1 &&
                  stale_completed.detector.batch_received &&
                  stale_completed.detector.batch_frame_id == 2 &&
                  stale_completed.detector.batch_age_ms == 170 &&
                  stale_completed.output.detector_status ==
                      DetectorBatchStatus::kStale &&
                  stale_completed.output.detector_candidates_queued == 0,
              "slow completion keeps original identity and is logged then discarded");

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

  ControllerConfig hold_config = config;
  hold_config.occlusion_reference_window = 3;
  hold_config.occlusion_hold_frames = 2;
  hold_config.uncertain_ms = 1000;
  hold_config.local_recovery_ms = 2000;
  hold_config.assisted_recovery_ms = 3000;
  FakeTracker hold_tracker;
  FakeDetector hold_detector;
  hold_detector.enabled = true;
  LongTermRuntime hold_runtime(hold_config, hold_tracker, hold_detector);
  hold_runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  for (std::uint64_t frame = 1; frame <= 3; ++frame) {
    hold_runtime.process_frame(image, frame, static_cast<std::int64_t>(frame * 40));
  }
  hold_tracker.emit_reliable = false;
  hold_tracker.emit_weak = true;
  hold_runtime.process_frame(image, 4, 160);
  RuntimeFrameResult trigger = hold_runtime.process_frame(image, 5, 200);
  expect_true(trigger.output.occlusion_hold_triggered &&
                  hold_tracker.infer_calls == 5,
              "trigger frame still performs tracker inference");
  const int polls_before_hold = hold_detector.poll_calls;
  RuntimeFrameResult hold_one = hold_runtime.process_frame(image, 6, 240);
  RuntimeFrameResult hold_two = hold_runtime.process_frame(image, 7, 280);
  expect_true(hold_one.tracker_skipped && hold_two.tracker_skipped &&
                  hold_one.observations.empty() && hold_two.observations.empty() &&
                  hold_tracker.infer_calls == 5,
              "every complete hold frame skips tracker inference");
  expect_true(hold_detector.submit_calls == 0 &&
                  hold_detector.poll_calls == polls_before_hold + 2,
              "hold suppresses detector submit while continuing one poll per frame");
  RuntimeFrameResult resumed = hold_runtime.process_frame(image, 8, 320);
  expect_true(!resumed.tracker_skipped && hold_tracker.infer_calls == 6,
              "tracker resumes on the frame after the exact hold window");

  hold_runtime.initialize(image, BBox{40, 30, 20, 20}, 20, 800);
  hold_tracker.emit_reliable = true;
  hold_tracker.emit_weak = false;
  RuntimeFrameResult reinitialized = hold_runtime.process_frame(image, 21, 840);
  expect_true(!reinitialized.tracker_skipped && hold_tracker.infer_calls == 7,
              "explicit reinitialization clears runtime skip state");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "long-term runtime tests passed\n";
  return 0;
}
