#include "long_term_runtime.hpp"
#include "target_class_session.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
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
    ++initialize_calls;
    initialized = true;
    committed_bbox = bbox;
    if (on_initialize) on_initialize();
  }

  std::vector<nanotrack::TrackerObservation> infer(
      const nanotrack::ImageView&,
      const nanotrack::SearchPlan& plan) override {
    ++infer_calls;
    if (on_infer) on_infer();
    ++search_token;
    if (!emit_reliable && !emit_weak) return {};
    const nanotrack::ResponseMetrics metrics = emit_weak
        ? nanotrack::ResponseMetrics{0.45F, 0.40F, 2.5F, 3.0F, 0.04F}
        : nanotrack::ResponseMetrics{0.92F, 0.90F, 10.0F, 12.0F, 0.20F};
    const nanotrack::BBox bbox = emit_weak ? weak_bbox :
        reliable_bbox.value_or(plan.predicted_bbox);
    const auto observed_frame = invalid_observation ? plan.frame_id + 1 : plan.frame_id;
    return {
        nanotrack::TrackerObservation{observed_frame, plan.timestamp_ms,
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
    if (on_commit) on_commit();
  }
  void stage_recent(const nanotrack::ImageView&,
                    const nanotrack::BBox&) override {
    ++stage_calls;
    if (on_stage) on_stage();
  }
  void commit_recent() override {
    ++recent_commit_calls;
    if (on_recent_commit) on_recent_commit();
  }
  void rollback_recent() override {
    ++rollback_calls;
    if (on_rollback) on_rollback();
  }

  bool initialized = false;
  bool emit_reliable = true;
  bool emit_weak = false;
  bool invalid_observation = false;
  std::optional<nanotrack::BBox> reliable_bbox;
  nanotrack::BBox weak_bbox{43, 33, 14, 14};
  int infer_calls = 0;
  int initialize_calls = 0;
  int commit_calls = 0;
  int stage_calls = 0;
  int recent_commit_calls = 0;
  int rollback_calls = 0;
  std::uint64_t search_token = 0;
  nanotrack::BBox committed_bbox{};
  std::function<void()> on_initialize;
  std::function<void()> on_infer;
  std::function<void()> on_commit;
  std::function<void()> on_stage;
  std::function<void()> on_recent_commit;
  std::function<void()> on_rollback;
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
  std::optional<nanotrack::DetectorBlockingResult> detect_blocking(
      const nanotrack::ImageView&,
      const nanotrack::DetectorRequest& request) override {
    ++blocking_calls;
    blocking_request = request;
    // Model the adapter's quiesce/consume boundary; worker details have their
    // own real-adapter tests. Nothing left here may leak into runtime polling.
    next_batch.reset();
    ready = true;
    if (fail_blocking) throw std::runtime_error("initialization inference failed");
    if (!enabled || !blocking_batch) return std::nullopt;
    auto batch = *blocking_batch;
    batch.source_frame_id = request.frame_id;
    batch.source_timestamp_ms = request.timestamp_ms;
    batch.generation = request.generation;
    batch.request_id = next_request_id++;
    return nanotrack::DetectorBlockingResult{batch.request_id, batch};
  }

  bool enabled = false;
  bool ready = true;
  bool fail_next_poll = false;
  int submit_calls = 0;
  int poll_calls = 0;
  int blocking_calls = 0;
  bool fail_blocking = false;
  std::uint64_t next_request_id = 9001;
  nanotrack::DetectorRequest last_request{};
  nanotrack::DetectorRequest blocking_request{};
  std::optional<nanotrack::DetectorBatch> next_batch;
  std::optional<nanotrack::DetectorBatch> blocking_batch;
};

bool same_bbox(const nanotrack::BBox& a, const nanotrack::BBox& b) {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

void test_frame_lifetime(const nanotrack::ImageView& image) {
  using namespace nanotrack;
  const ControllerConfig config;
  const TargetClassRuntimeConfig target_config{4, 9, {}};
  const BBox roi{40, 30, 20, 20};
  const BBox reselected_roi{80, 40, 10, 10};
  NullDetectorAdapter detector;
  FakeTracker tracker;
  LongTermRuntime runtime(config, tracker, detector, target_config);
  runtime.initialize(image, roi, 0, 0);
  auto reject_reentrant_initialize = [&] {
    const auto index = runtime.last_initialization().initialization_index;
    const int calls = tracker.initialize_calls;
    bool rejected = false;
    try { runtime.initialize(image, reselected_roi, 1, 40); }
    catch (const std::logic_error&) { rejected = true; }
    expect_true(rejected && runtime.last_initialization().initialization_index == index &&
                    tracker.initialize_calls == calls &&
                    same_bbox(runtime.controller().current_bbox(), roi),
                "tracker commit/template callback rejects reselect until process_frame returns");
  };
  tracker.on_commit = reject_reentrant_initialize;
  runtime.process_frame(image, 1, 40);
  tracker.on_commit = {};
  expect_true(runtime.initialize(image, roi, 1, 100).initialization_index == 2,
              "reselect becomes available after commit callback frame returns");

  tracker.on_stage = reject_reentrant_initialize;
  tracker.on_recent_commit = reject_reentrant_initialize;
  for (std::uint64_t frame = 2; frame <= 19; ++frame) {
    runtime.process_frame(image, frame, frame * 100);
  }
  expect_true(tracker.stage_calls == 1 && tracker.recent_commit_calls == 1,
              "reentrant guards are exercised by reachable stage and recent-commit actions");
  expect_true(runtime.initialize(image, roi, 19, 1900).initialization_index == 3,
              "reselect becomes available after template commit frame returns");
  for (std::uint64_t frame = 20; frame <= 34; ++frame) {
    runtime.process_frame(image, frame, frame * 100);
  }
  tracker.on_rollback = reject_reentrant_initialize;
  tracker.emit_reliable = false;
  runtime.process_frame(image, 35, 3500);
  expect_true(tracker.rollback_calls == 1,
              "reentrant guard is exercised by a reachable template rollback");
  expect_true(runtime.initialize(image, roi, 35, 3500).initialization_index == 4,
              "reselect becomes available after template rollback frame returns");

  FakeTracker nested_tracker;
  LongTermRuntime nested_runtime(config, nested_tracker, detector, target_config);
  nested_runtime.initialize(image, roi, 0, 0);
  bool in_callback = false;
  nested_tracker.on_commit = [&] {
    if (in_callback) return;
    in_callback = true;
    bool rejected = false;
    try { nested_runtime.process_frame(image, 2, 80); }
    catch (const std::logic_error&) { rejected = true; }
    in_callback = false;
    expect_true(rejected, "commit callback cannot open a nested runtime frame");
  };
  nested_runtime.process_frame(image, 1, 40);
  nested_tracker.on_commit = {};
  expect_true(nested_runtime.process_frame(image, 2, 80).output.accepted,
              "rejected nested frame leaves next outer frame identity available");

  struct TrackerFailure { int id; };
  FakeTracker infer_tracker;
  LongTermRuntime infer_runtime(config, infer_tracker, detector, target_config);
  infer_runtime.initialize(image, roi, 0, 0);
  infer_tracker.on_infer = [] { throw TrackerFailure{71}; };
  bool original_infer_error = false;
  try { infer_runtime.process_frame(image, 1, 40); }
  catch (const TrackerFailure& error) { original_infer_error = error.id == 71; }
  infer_tracker.on_infer = {};
  bool controller_abandoned = false;
  try { controller_abandoned = infer_runtime.process_frame(image, 2, 80).output.accepted; }
  catch (const std::exception&) {}
  bool infer_reinitialized = false;
  try { infer_reinitialized = infer_runtime.initialize(image, roi, 2, 80).initialization_index == 2; }
  catch (const std::exception&) {}
  expect_true(original_infer_error && controller_abandoned && infer_reinitialized,
              "infer exception preserves original error and abandons runtime/controller frame");

  FakeTracker finish_tracker;
  LongTermRuntime finish_runtime(config, finish_tracker, detector, target_config);
  finish_runtime.initialize(image, roi, 0, 0);
  finish_tracker.invalid_observation = true;
  bool finish_error = false;
  try { finish_runtime.process_frame(image, 1, 40); }
  catch (const std::invalid_argument& error) {
    finish_error = std::string(error.what()) == "tracker observation frame identity mismatch";
  }
  finish_tracker.invalid_observation = false;
  bool finish_reinitialized = false;
  try { finish_reinitialized = finish_runtime.initialize(image, roi, 1, 40).initialization_index == 2; }
  catch (const std::exception&) {}
  expect_true(finish_error && finish_reinitialized && finish_tracker.commit_calls == 0,
              "finish validation exception permits immediate reinitialize without tracker commit");

  for (bool template_failure : {false, true}) {
    FakeTracker action_tracker;
    LongTermRuntime action_runtime(config, action_tracker, detector, target_config);
    action_runtime.initialize(image, roi, 0, 0);
    action_tracker.reliable_bbox = BBox{41, 30, 20, 20};
    auto fail_action = [] { throw TrackerFailure{93}; };
    if (template_failure) action_tracker.on_stage = fail_action;
    else action_tracker.on_commit = fail_action;
    bool original_action_error = false;
    try {
      for (std::uint64_t frame = 1; frame <= 15; ++frame) {
        action_runtime.process_frame(image, frame, frame * 100);
      }
    } catch (const TrackerFailure& error) { original_action_error = error.id == 93; }
    expect_true(original_action_error &&
                    same_bbox(action_runtime.controller().current_bbox(), BBox{41, 30, 20, 20}),
                "tracker action exception preserves the original error and committed controller bbox");
    action_tracker.on_commit = {};
    action_tracker.on_stage = {};
    bool action_reinitialized = false;
    try { action_reinitialized = action_runtime.initialize(image, roi, 15, 1500).initialization_index == 2; }
    catch (const std::exception&) {}
    expect_true(action_reinitialized && same_bbox(action_runtime.controller().current_bbox(), roi),
                "tracker action exception releases runtime guard for manual reinitialization");
  }
}

void test_target_class_initialization(const nanotrack::ImageView& image) {
  using namespace nanotrack;
  ControllerConfig config;
  config.uncertain_ms = 10;
  config.local_recovery_ms = 20;
  config.assisted_recovery_ms = 30;
  config.detector_interval_ms = 20;
  const BBox roi{43, 33, 10, 10};
  const std::vector<std::string> verified_labels{
      "person", "bicycle", "car", "van", "truck", "bus", "motor", "tricycle", "awning"};
  const auto auto_config = make_target_class_runtime_config(Options{}, &verified_labels);
  FakeTracker tracker;
  FakeDetector detector;
  detector.enabled = true;
  DetectorBatch batch;
  batch.candidates = {{BBox{40, 30, 20, 20}, 0.9F, 2}};
  batch.timing = DetectorTiming{1, 7, 2, 10};
  batch.raw_candidate_count = 5;
  batch.filtered_candidate_count = 1;
  detector.blocking_batch = batch;
  LongTermRuntime runtime(config, tracker, detector, auto_config);
  tracker.on_initialize = [&] {
    expect_true(same_bbox(runtime.controller().current_bbox(), roi),
                "controller receives manual ROI before tracker initializes");
  };
  const TargetClassInitializationResult initialized = runtime.initialize(image, roi, 7, 280);
  expect_true(initialized.initialization_index == 1 && initialized.frame_id == 7 &&
                  initialized.timestamp_ms == 280 && initialized.request_id == 9001 &&
                  initialized.batch && initialized.batch->request_id == 9001 &&
                  initialized.detector_timing.total_ms == 10 &&
                  initialized.detector_error.empty(),
              "auto initialization retains its own identity, batch and timing");
  expect_true(initialized.target_class.status == TargetClassStatus::kEstablished &&
                  initialized.target_class.target_class_id == 2 &&
                  initialized.target_class.target_class_allows_recovery &&
                  initialized.target_class.candidates.size() == 1,
              "unique initialization match establishes class with diagnostics");
  expect_true(same_bbox(initialized.original_roi, roi) &&
                  same_bbox(tracker.committed_bbox, roi) && tracker.commit_calls == 0 &&
                  tracker.infer_calls == 0 && detector.submit_calls == 0,
              "initialization never substitutes detector geometry or commits tracking");
  expect_true(same_bbox(detector.blocking_request.roi, BBox{0, 0, 200, 120}) &&
                  detector.blocking_request.max_candidates ==
                      std::numeric_limits<std::size_t>::max() &&
                  detector.blocking_request.generation == 0,
              "class initialization requests every full-frame candidate independently of generation");
  tracker.emit_reliable = false;
  auto frame = runtime.process_frame(image, 8, 290);
  expect_true(frame.target_class.target_class_id == 2 &&
                  frame.output.target_class.target_class_id == 2 &&
                  frame.output.detector_status == DetectorBatchStatus::kNone &&
                  frame.output.detector_candidates_queued == 0 &&
                  !frame.output.detector_candidate_id && !frame.output.control_valid &&
                  !frame.output.accepted && !frame.initialization && tracker.commit_calls == 0,
              "initialization batch never enters finish_frame or grants recovery identity");

  // Leave a normal async completion behind, then reselect on the same frame/time.
  const auto requested = runtime.process_frame(image, 9, 330);
  expect_true(requested.detector.request_submitted &&
                  detector.last_request.generation == 0,
              "adapter request ID does not become controller generation");
  DetectorBatch old_batch;
  old_batch.source_frame_id = 9;
  old_batch.source_timestamp_ms = 330;
  old_batch.generation = 0;
  old_batch.candidates = {{BBox{40, 30, 20, 20}, 0.9F, 2}};
  detector.next_batch = old_batch;
  detector.blocking_batch->candidates[0].class_id = 3;
  const auto reselected = runtime.initialize(image, roi, 7, 280);
  expect_true(reselected.initialization_index == 2 && reselected.request_id == 9002 &&
                  reselected.target_class.target_class_id == 3 &&
                  initialized.target_class.target_class_id == 2 &&
                  runtime.last_initialization().request_id == 9002,
              "same-frame reselect replaces class with distinct initialization identity");
  frame = runtime.process_frame(image, 8, 290);
  expect_true(!frame.detector.batch_received && !frame.output.detector_candidate_id &&
                  frame.target_class.target_class_id == 3,
              "blocking initialization consumes old async state outside the new session");
  frame = runtime.process_frame(image, 9, 330);
  expect_true(detector.last_request.generation == 1,
              "same-frame reselect advances controller generation independently");
  old_batch.candidates[0].class_id = 3;
  detector.next_batch = old_batch;
  frame = runtime.process_frame(image, 10, 340);
  expect_true(frame.output.detector_status == DetectorBatchStatus::kInvalid &&
                  frame.output.detector_candidates_queued == 0 &&
                  frame.output.detector_candidate_diagnostics.size() == 1 &&
                  !frame.output.detector_candidate_diagnostics[0].candidate_id &&
                  frame.output.detector_candidate_diagnostics[0].rejection ==
                      DetectorCandidateRejection::kGenerationInvalidated,
              "old async generation cannot enter reselected recovery even with matching class");
  DetectorBatch current_batch = old_batch;
  current_batch.source_frame_id = 11;
  current_batch.source_timestamp_ms = 350;
  current_batch.generation = 1;
  current_batch.candidates.push_back({BBox{40, 30, 20, 20}, 0.9F, 2});
  detector.next_batch = current_batch;
  frame = runtime.process_frame(image, 11, 350);
  expect_true(frame.output.detector_candidates_queued == 1 &&
                  frame.output.detector_candidate_diagnostics.size() == 2 &&
                  frame.output.detector_candidate_diagnostics[0].queued &&
                  frame.output.detector_candidate_diagnostics[1].rejection ==
                      DetectorCandidateRejection::kClassMismatch &&
                  !frame.output.accepted && !frame.output.control_valid &&
                  tracker.commit_calls == 0,
              "established runtime class gates later candidates without granting control");

  struct UnknownCase { std::vector<DetectorCandidate> candidates; TargetClassReason reason; };
  const std::vector<UnknownCase> unknown_cases{
      {{{BBox{40, 30, 20, 20}, 0.9F, 2}, {BBox{40, 30, 20, 20}, 0.9F, 3}},
       TargetClassReason::kAmbiguous},
      {{{BBox{150, 80, 20, 20}, 0.9F, 2}}, TargetClassReason::kNoMatch}};
  tracker.on_initialize = {};
  for (const auto& test : unknown_cases) {
    detector.blocking_batch->candidates = test.candidates;
    const auto unknown = runtime.initialize(image, roi, 7, 280);
    expect_true(unknown.target_class.status == TargetClassStatus::kUnknown &&
                    unknown.target_class.reason == test.reason &&
                    !unknown.target_class.target_class_allows_recovery &&
                    !unknown.target_class.target_class_id &&
                    same_bbox(tracker.committed_bbox, roi) &&
                    same_bbox(runtime.controller().current_bbox(), roi),
                "ambiguous and no-match initialization retain manual ROI with UNKNOWN");
    tracker.emit_reliable = false;
    runtime.process_frame(image, 8, 300);
    const auto diagnostic_request = runtime.process_frame(image, 9, 400);
    expect_true(diagnostic_request.detector.request_submitted &&
                    diagnostic_request.plan.hypothesis_source == HypothesisSource::kMotion &&
                    !diagnostic_request.output.control_valid,
                "UNKNOWN initialization still schedules diagnostic detector requests");
    DetectorBatch diagnostic_batch;
    diagnostic_batch.source_frame_id = detector.last_request.frame_id;
    diagnostic_batch.source_timestamp_ms = detector.last_request.timestamp_ms;
    diagnostic_batch.generation = detector.last_request.generation;
    diagnostic_batch.candidates = {{roi, 0.99F, 2}};
    detector.next_batch = diagnostic_batch;
    const auto received = runtime.process_frame(image, 10, 410);
    const auto next = runtime.process_frame(image, 11, 420);
    expect_true(received.detector.batch_received &&
                    received.output.detector_status == DetectorBatchStatus::kAccepted &&
                    received.output.detector_candidates_queued == 0 &&
                    !received.output.control_valid && !received.output.accepted &&
                    next.plan.hypothesis_source == HypothesisSource::kMotion &&
                    !next.plan.detector_candidate_id && tracker.commit_calls == 0,
                "UNKNOWN full runtime diagnostics cannot create a probe or authorize control");
  }
  tracker.emit_reliable = true;

  detector.enabled = false;
  const auto unavailable = runtime.initialize(image, roi, 7, 280);
  expect_true(unavailable.target_class.reason == TargetClassReason::kDetectorUnavailable &&
                  !unavailable.request_id && !unavailable.batch &&
                  same_bbox(tracker.committed_bbox, roi),
              "unavailable detector keeps tracker initialization working");
  // A disabled adapter may still own an old async engine error; always consume
  // the blocking boundary so unavailable() cannot mask DETECTOR_ERROR.
  detector.fail_blocking = true;
  const auto error = runtime.initialize(image, roi, 7, 280);
  expect_true(error.target_class.reason == TargetClassReason::kDetectorError &&
                  error.detector_error == "initialization inference failed" &&
                  !error.target_class.target_class_allows_recovery &&
                  same_bbox(tracker.committed_bbox, roi) && tracker.commit_calls == 0,
              "engine error becomes UNKNOWN without stopping NanoTrack initialization");
  detector.enabled = true;
  expect_true(runtime.initialize(image, roi, 7, 280).target_class.reason ==
                  TargetClassReason::kDetectorError,
              "current initialization engine failure also becomes UNKNOWN");
  frame = runtime.process_frame(image, 8, 290);
  expect_true(frame.target_class.reason == TargetClassReason::kDetectorError &&
                  tracker.infer_calls > 0,
              "UNKNOWN error state propagates while normal tracker execution continues");

  TargetClassRuntimeConfig fixed_config = auto_config;
  fixed_config.requested_class_id = 4;
  for (bool available : {false, true}) {
    FakeTracker fixed_tracker;
    FakeDetector fixed_detector;
    fixed_detector.enabled = available;
    LongTermRuntime fixed_runtime(config, fixed_tracker, fixed_detector, fixed_config);
    const auto fixed = fixed_runtime.initialize(image, roi, 7, 280);
    expect_true(fixed.target_class.status == TargetClassStatus::kFixed &&
                    fixed.target_class.target_class_id == 4 &&
                    fixed.target_class.target_class_allows_recovery == available &&
                    fixed_detector.blocking_calls == 0 && !fixed.request_id && !fixed.batch &&
                    same_bbox(fixed_tracker.committed_bbox, roi) &&
                    same_bbox(fixed_runtime.controller().current_bbox(), roi) &&
                    fixed_tracker.commit_calls == 0,
                "fixed override skips detector inference and preserves original manual ROI");
  }

  FakeTracker legacy_tracker;
  FakeDetector legacy_detector;
  legacy_detector.enabled = true;
  legacy_detector.blocking_batch = batch;
  LongTermRuntime legacy_runtime(config, legacy_tracker, legacy_detector,
                                TargetClassRuntimeConfig{});
  const auto unconfigured = legacy_runtime.initialize(image, roi, 7, 280);
  expect_true(unconfigured.target_class.reason == TargetClassReason::kDetectorUnavailable &&
                  !unconfigured.target_class.target_class_allows_recovery &&
                  legacy_detector.blocking_calls == 0,
              "missing verified class count safely skips automatic inference");
  NullDetectorAdapter null_detector;
  LongTermRuntime null_runtime(config, legacy_tracker, null_detector, auto_config);
  expect_true(null_runtime.initialize(image, roi, 7, 280).target_class.reason ==
                  TargetClassReason::kDetectorUnavailable,
              "null adapter explicitly yields detector unavailable");

  FakeTracker strict_tracker;
  FakeDetector strict_detector;
  strict_detector.enabled = true;
  strict_detector.blocking_batch = batch;
  strict_detector.blocking_batch->candidates = {{BBox{45, 33, 10, 10}, 0.9F, 2}};
  TargetClassRuntimeConfig strict_config = auto_config;
  strict_config.match_config.min_roi_coverage = 0.99F;
  strict_config.match_config.max_center_ratio = 0.01F;
  LongTermRuntime strict_runtime(config, strict_tracker, strict_detector, strict_config);
  expect_true(strict_runtime.initialize(image, roi, 7, 280).target_class.reason ==
                  TargetClassReason::kNoMatch,
              "runtime passes caller match thresholds to class establishment");

  tracker.on_infer = [&] {
    bool rejected = false;
    const int calls_before = detector.blocking_calls;
    const int tracker_calls_before = tracker.initialize_calls;
    try { runtime.initialize(image, roi, 8, 290); }
    catch (const std::logic_error&) { rejected = true; }
    expect_true(rejected && detector.blocking_calls == calls_before &&
                    tracker.initialize_calls == tracker_calls_before,
                "open frame rejects initialization before detector or tracker mutation");
  };
  runtime.process_frame(image, 9, 330);
  tracker.on_infer = {};
  const auto index_before = runtime.last_initialization().initialization_index;
  const int calls_before = detector.blocking_calls;
  const int tracker_calls_before = tracker.initialize_calls;
  for (const BBox invalid : {BBox{-1, 0, 10, 10}, BBox{0, 0, 0, 10}}) {
    bool rejected = false;
    try { runtime.initialize(image, invalid, 9, 330); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect_true(rejected && detector.blocking_calls == calls_before &&
                    tracker.initialize_calls == tracker_calls_before &&
                    runtime.last_initialization().initialization_index == index_before,
                "invalid ROI fails before any initialization mutation");
  }
  expect_true(runtime.initialize(image, roi, 9, 330).initialization_index == index_before + 1,
              "completed frame permits a subsequent initialization");
}

// These fixtures catch accepting a fixed class equal to labels.size(), losing
// caller thresholds, and accidentally requiring labels with a disabled detector.
void test_target_class_session_config(const nanotrack::ImageView& image) {
  using namespace nanotrack;
  const std::vector<std::string> labels{"person", "car", "bus"};
  Options options;
  options.target_class_min_roi_coverage = 0.65F;
  options.target_class_max_center_ratio = 0.8F;
  options.target_class_ambiguity_margin = 0.25F;
  const auto automatic = make_target_class_runtime_config(options, &labels);
  expect_true(automatic.requested_class_id == -1 && automatic.class_count == 3 &&
                  automatic.match_config.min_roi_coverage == 0.65F &&
                  automatic.match_config.max_center_ratio == 0.8F &&
                  automatic.match_config.ambiguity_margin == 0.25F,
              "verified label count and CLI thresholds reach automatic runtime config");
  options.detector_class_id = 2;
  expect_true(make_target_class_runtime_config(options, &labels).requested_class_id == 2,
              "last verified label is a valid fixed target class");
  const std::vector<std::string> empty_labels;
  for (const auto* verified : {&labels, &empty_labels}) {
    options.detector_class_id = static_cast<int>(verified->size());
    bool rejected = false;
    try { (void)make_target_class_runtime_config(options, verified); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect_true(rejected, "fixed target class at verified label count is rejected");
  }
  for (const auto* verified : {&labels, static_cast<const std::vector<std::string>*>(nullptr)}) {
    options.detector_class_id = -2;
    bool rejected = false;
    try { (void)make_target_class_runtime_config(options, verified); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect_true(rejected, "negative target class other than automatic -1 is rejected");
  }
  options.detector_class_id = 80;
  const auto disabled = make_target_class_runtime_config(options);
  expect_true(disabled.requested_class_id == 80 && disabled.class_count == 0,
              "unconfigured detector preserves fixed semantics without verified classes");
  FakeTracker tracker;
  NullDetectorAdapter detector;
  LongTermRuntime runtime(ControllerConfig{}, tracker, detector, disabled);
  const auto initialization = runtime.initialize(image, BBox{40, 30, 20, 20}, 0, 0);
  expect_true(initialization.target_class.status == TargetClassStatus::kFixed &&
                  initialization.target_class.target_class_id == 80 &&
                  !initialization.target_class.target_class_allows_recovery &&
                  !initialization.request_id && !initialization.batch,
              "fixed target with no detector initializes NanoTrack without detector recovery");
}

}  // namespace

int main() {
  using namespace nanotrack;

  std::vector<std::uint8_t> pixels(200 * 120 * 3, 0);
  const ImageView image{pixels.data(), 200, 120, 200 * 3, 3};
  test_target_class_initialization(image);
  test_target_class_session_config(image);
  test_frame_lifetime(image);
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
  const TargetClassRuntimeConfig hold_target_class{4, 9, {}};
  LongTermRuntime hold_runtime(hold_config, hold_tracker, hold_detector, hold_target_class);
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
  expect_true(hold_one.target_class.target_class_id == 4 &&
                  hold_two.target_class.target_class_id == 4 &&
                  hold_two.output.target_class.target_class_id == 4,
              "runtime preserves established session class through skipped HOLD frames");
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
