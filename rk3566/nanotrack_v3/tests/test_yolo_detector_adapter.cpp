#include "yolo_detector_adapter.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace nanotrack {

// Establish otherwise fleeting worker states without adding public test APIs.
class YoloDetectorAdapterTestPeer {
 public:
  static void seed_pending(YoloDetectorAdapter& adapter, const ImageView& image,
                           const DetectorRequest& request) {
    // Park the idle worker to make pending cancellation deterministic, even if
    // the operating system would otherwise schedule the worker immediately.
    {
      std::lock_guard<std::mutex> lock(adapter.mutex_);
      adapter.stopping_ = true;
      adapter.changed_.notify_all();
    }
    adapter.worker_.join();
    std::lock_guard<std::mutex> lock(adapter.mutex_);
    adapter.stopping_ = false;
    auto work = adapter.copy_work(image, request);
    work.identity = {adapter.next_request_id_++,
                     YoloDetectorAdapter::RequestKind::kAsync};
    adapter.pending_ = std::move(work);
  }

  static void resume_worker(YoloDetectorAdapter& adapter) {
    adapter.worker_ = std::thread(&YoloDetectorAdapter::worker_loop, &adapter);
  }

  static bool wait_exclusive(YoloDetectorAdapter& adapter) {
    std::unique_lock<std::mutex> lock(adapter.mutex_);
    return adapter.changed_.wait_for(lock, std::chrono::seconds(1), [&] {
      return adapter.initialization_exclusive_;
    });
  }

  static bool wait_completed(YoloDetectorAdapter& adapter) {
    std::unique_lock<std::mutex> lock(adapter.mutex_);
    return adapter.changed_.wait_for(lock, std::chrono::seconds(1), [&] {
      return adapter.completed_.has_value() || adapter.pending_error_.has_value();
    });
  }

  static bool wait_blocking_callers(YoloDetectorAdapter& adapter,
                                    std::size_t count) {
    std::unique_lock<std::mutex> lock(adapter.mutex_);
    return adapter.changed_.wait_for(lock, std::chrono::seconds(1), [&] {
      return adapter.blocking_callers_ >= count;
    });
  }
};

}  // namespace nanotrack

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Function>
void expect_throws(Function function,
                   const std::string& expected_text,
                   const std::string& message) {
  try {
    function();
    expect_true(false, message + " (no exception)");
  } catch (const std::exception& error) {
    expect_true(std::string(error.what()).find(expected_text) !=
                    std::string::npos,
                message + " (unexpected text: " + error.what() + ")");
  }
}

struct EngineState {
  std::mutex mutex;
  std::condition_variable changed;
  bool entered = false;
  bool release = true;
  bool finished = false;
  bool throw_error = false;
  int calls = 0;
  int width = 0;
  int height = 0;
  int channels = 0;
  std::size_t stride_bytes = 0;
  nanotrack::DetectorRequest request{};
  std::vector<std::uint8_t> observed_active_pixels;
};

class FakeEngine final : public nanotrack::DetectorEngine {
 public:
  explicit FakeEngine(std::shared_ptr<EngineState> state)
      : state_(std::move(state)) {}

  nanotrack::DetectorEngineResult infer(
      const nanotrack::ImageView& image,
      const nanotrack::DetectorRequest& request) override {
    {
      std::unique_lock<std::mutex> lock(state_->mutex);
      ++state_->calls;
      state_->entered = true;
      state_->width = image.width;
      state_->height = image.height;
      state_->channels = image.channels;
      state_->stride_bytes = image.stride_bytes;
      state_->request = request;
      state_->changed.notify_all();
      state_->changed.wait(lock, [&] { return state_->release; });
    }

    std::vector<std::uint8_t> observed;
    const std::size_t row_bytes =
        static_cast<std::size_t>(image.width * image.channels);
    for (int row = 0; row < image.height; ++row) {
      const std::uint8_t* begin =
          image.data + static_cast<std::size_t>(row) * image.stride_bytes;
      observed.insert(observed.end(), begin, begin + row_bytes);
    }

    bool throw_error = false;
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      state_->observed_active_pixels = std::move(observed);
      state_->finished = true;
      throw_error = state_->throw_error;
      state_->changed.notify_all();
    }
    if (throw_error) {
      throw std::runtime_error("fake engine inference stage failed");
    }

    nanotrack::DetectorEngineResult result;
    result.raw_candidate_count = 2;
    result.filtered_candidate_count = 1;
    result.timing = nanotrack::DetectorTiming{1.0F, 4.0F, 2.0F, 7.0F};
    result.candidates = {
        {nanotrack::BBox{1.0F, 1.0F, 2.0F, 2.0F}, 0.75F, 0}};
    return result;
  }

 private:
  std::shared_ptr<EngineState> state_;
};

bool wait_for(const std::shared_ptr<EngineState>& state,
              bool EngineState::*field) {
  std::unique_lock<std::mutex> lock(state->mutex);
  return state->changed.wait_for(lock, std::chrono::seconds(1),
                                 [&] { return state.get()->*field; });
}

void release_engine(const std::shared_ptr<EngineState>& state) {
  std::lock_guard<std::mutex> lock(state->mutex);
  state->release = true;
  state->changed.notify_all();
}

std::optional<nanotrack::DetectorBatch> poll_until_batch(
    nanotrack::YoloDetectorAdapter& adapter) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (std::chrono::steady_clock::now() < deadline) {
    std::optional<nanotrack::DetectorBatch> batch = adapter.poll();
    if (batch) return batch;
    std::this_thread::yield();
  }
  return std::nullopt;
}

void test_blocking_protocol(const nanotrack::ImageView& image,
                            const nanotrack::DetectorRequest& request) {
  using namespace nanotrack;
  NullDetectorAdapter null_adapter;
  expect_true(!null_adapter.detect_blocking(image, request),
              "null detector has no blocking result");
  auto state = std::make_shared<EngineState>();
  YoloDetectorAdapter adapter(std::make_unique<FakeEngine>(state));
  adapter.submit(image, request);
  auto first = poll_until_batch(adapter);
  auto second = adapter.detect_blocking(image, request);
  auto third = adapter.detect_blocking(image, request);
  adapter.submit(image, request);
  auto fourth = poll_until_batch(adapter);
  expect_true(first && second && third && fourth,
              "mixed asynchronous and blocking requests complete");
  if (first && second && third && fourth) {
    expect_true(first->request_id == 1 && second->request_id == 2 &&
                    third->request_id == 3 && fourth->request_id == 4,
                "adapter assigns increasing unique IDs independent of generation");
    expect_true(second->batch.request_id == second->request_id &&
                    third->batch.request_id == third->request_id &&
                    second->batch.source_frame_id == 42 &&
                    third->batch.source_frame_id == 42 &&
                    second->batch.source_timestamp_ms == 1200 &&
                    second->batch.generation == 17,
                "same-frame reinitializations consume their exact request identity");
  }
  expect_true(!adapter.poll(), "blocking result is consumed only once");
  const ImageView invalid{nullptr, 2, 2, 6, 3};
  expect_throws([&] { adapter.detect_blocking(invalid, request); }, "BGR8",
                "blocking validation rejects invalid input");
  expect_true(adapter.can_submit(),
              "validation exception releases initialization exclusivity");

  adapter.submit(image, DetectorRequest{90, 2000, request.roi, 3, 200});
  expect_true(YoloDetectorAdapterTestPeer::wait_completed(adapter),
              "old asynchronous result reaches completed state");
  auto replacement = adapter.detect_blocking(image, request);
  expect_true(replacement && replacement->batch.source_frame_id == 42 &&
                  replacement->request_id == 6 && !adapter.poll(),
              "blocking initialization discards an old completed result");

  auto pending_state = std::make_shared<EngineState>();
  YoloDetectorAdapter pending(std::make_unique<FakeEngine>(pending_state));
  YoloDetectorAdapterTestPeer::seed_pending(pending, image,
                                          DetectorRequest{91, 2100, request.roi, 3, 201});
  auto pending_call = std::async(std::launch::async, [&] {
    return pending.detect_blocking(image, request);
  });
  expect_true(YoloDetectorAdapterTestPeer::wait_exclusive(pending),
              "blocking caller cancels pending work while the worker is parked");
  YoloDetectorAdapterTestPeer::resume_worker(pending);
  auto pending_result = pending_call.get();
  expect_true(pending_result && pending_result->request_id == 2 &&
                  pending_result->batch.source_frame_id == 42,
              "unclaimed pending request is cancelled without ID reuse");
  {
    std::lock_guard<std::mutex> lock(pending_state->mutex);
    expect_true(pending_state->calls == 1 && pending_state->request.frame_id == 42,
                "cancelled pending work never enters inference");
  }
}

void test_inflight_exclusivity(const nanotrack::ImageView& image,
                              const nanotrack::DetectorRequest& request) {
  using namespace nanotrack;
  auto state = std::make_shared<EngineState>();
  state->release = false;
  YoloDetectorAdapter adapter(std::make_unique<FakeEngine>(state));
  adapter.submit(image, DetectorRequest{90, 2000, request.roi, 3, 200});
  expect_true(wait_for(state, &EngineState::entered), "old request is in flight");
  auto initialization = std::async(std::launch::async, [&] {
    return adapter.detect_blocking(image, request);
  });
  expect_true(YoloDetectorAdapterTestPeer::wait_exclusive(adapter),
              "initialization obtains exclusivity while waiting for old inference");
  expect_true(!adapter.can_submit() && !adapter.poll(),
              "async operations cannot access initialization-owned state");
  expect_throws([&] { adapter.submit(image, request); }, "busy",
                "async submit cannot race initialization");
  expect_true(initialization.wait_for(std::chrono::milliseconds(20)) ==
                  std::future_status::timeout,
              "initialization waits for existing inference");
  release_engine(state);
  expect_true(initialization.wait_for(std::chrono::seconds(1)) ==
                  std::future_status::ready,
              "old inference completion wakes initialization without deadlock");
  auto result = initialization.get();
  expect_true(result && result->request_id == 2 &&
                  result->batch.source_frame_id == 42 && !adapter.poll(),
              "old in-flight result cannot contaminate initialization");

  // Poll continuously across initialization success/error publication.
  for (bool fail : {false, true}) {
    auto isolated_state = std::make_shared<EngineState>();
    isolated_state->release = false;
    isolated_state->throw_error = fail;
    YoloDetectorAdapter isolated(std::make_unique<FakeEngine>(isolated_state));
    auto blocking = std::async(std::launch::async, [&] {
      try {
        auto found = isolated.detect_blocking(image, request);
        return !fail && found && found->request_id == 1;
      } catch (const std::exception& error) {
        return fail && std::string(error.what()).find("inference stage failed") !=
                           std::string::npos;
      }
    });
    expect_true(wait_for(isolated_state, &EngineState::entered),
                "initialization inference starts");
    auto concurrent = std::async(std::launch::async, [&] {
      return isolated.detect_blocking(image, request);
    });
    release_engine(isolated_state);
    bool poll_stole = false;
    while (blocking.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
      try { poll_stole = isolated.poll().has_value() || poll_stole; }
      catch (...) { poll_stole = true; }
      std::this_thread::yield();
    }
    expect_true(blocking.get() && !poll_stole,
                "poll cannot consume initialization result or error");
    auto next = concurrent.get();
    expect_true(fail ? !next : (next && next->request_id == 2),
                "concurrent initialization calls serialize and honor disabled state");
    if (fail) {
      expect_true(!isolated.available() && !isolated.can_submit() &&
                      !isolated.detect_blocking(image, request),
                  "initialization engine error permanently disables detector");
      expect_throws([&] { isolated.submit(image, request); }, "unavailable",
                    "initialization engine error rejects later async submit");
    }
  }
}

void test_failure_and_stop(const nanotrack::ImageView& image,
                           const nanotrack::DetectorRequest& request) {
  using namespace nanotrack;
  auto state = std::make_shared<EngineState>();
  state->release = false;
  state->throw_error = true;
  YoloDetectorAdapter adapter(std::make_unique<FakeEngine>(state));
  adapter.submit(image, request);
  expect_true(wait_for(state, &EngineState::entered), "failing async inference starts");
  auto blocking = std::async(std::launch::async, [&] {
    try { adapter.detect_blocking(image, request); }
    catch (const std::exception& error) {
      return std::string(error.what()).find("inference stage failed") != std::string::npos;
    }
    return false;
  });
  expect_true(YoloDetectorAdapterTestPeer::wait_exclusive(adapter),
              "initialization waits for failing old request");
  release_engine(state);
  expect_true(blocking.get(), "old engine failure propagates through initialization");
  expect_true(!adapter.available() && !adapter.can_submit() &&
                  !adapter.detect_blocking(image, request) && !adapter.poll(),
              "old engine error is consumed once and permanently disables detector");
  expect_throws([&] { adapter.submit(image, request); }, "unavailable",
                "disabled adapter rejects subsequent async work");

  auto completed_error_state = std::make_shared<EngineState>();
  completed_error_state->throw_error = true;
  YoloDetectorAdapter completed_error(std::make_unique<FakeEngine>(completed_error_state));
  completed_error.submit(image, request);
  expect_true(YoloDetectorAdapterTestPeer::wait_completed(completed_error),
              "async failure reaches stored-error state");
  expect_true(!completed_error.available() && !completed_error.can_submit(),
              "engine failure disables adapter before any error consumer runs");
  expect_throws([&] { completed_error.detect_blocking(image, request); },
                "inference stage failed", "blocking call propagates already-stored async error");
  expect_true(!completed_error.detect_blocking(image, request) && !completed_error.poll(),
              "already-stored async error is surfaced exactly once");

  auto stop_state = std::make_shared<EngineState>();
  stop_state->release = false;
  auto owned = std::make_unique<YoloDetectorAdapter>(std::make_unique<FakeEngine>(stop_state));
  auto* raw = owned.get();
  auto waiting = std::async(std::launch::async, [&] {
    return raw->detect_blocking(image, request);
  });
  expect_true(wait_for(stop_state, &EngineState::entered), "stopping initialization enters inference");
  auto queued = std::async(std::launch::async, [&] {
    return raw->detect_blocking(image, request);
  });
  expect_true(YoloDetectorAdapterTestPeer::wait_blocking_callers(*raw, 2),
              "second initialization waits for exclusive owner");
  auto destruction = std::async(std::launch::async, [&] { owned.reset(); });
  expect_true(waiting.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
              "destructor wakes blocking caller before inference finishes");
  expect_true(!waiting.get(), "stopped blocking request has no result");
  expect_true(queued.wait_for(std::chrono::seconds(1)) == std::future_status::ready &&
                  !queued.get(),
              "destructor wakes caller waiting for initialization exclusivity");
  expect_true(destruction.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout,
              "destructor still joins engine after waking caller");
  release_engine(stop_state);
  expect_true(destruction.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
              "destructor finishes after inference and blocking callers leave");
  destruction.get();
}

}  // namespace

int main() {
  using namespace nanotrack;

  auto copy_state = std::make_shared<EngineState>();
  copy_state->release = false;
  YoloDetectorAdapter adapter(std::make_unique<FakeEngine>(copy_state));
  expect_true(adapter.available(), "valid engine makes adapter available");
  expect_true(adapter.can_submit(), "new adapter accepts one request");
  expect_true(!adapter.poll().has_value(), "empty poll is non-blocking");

  std::vector<std::uint8_t> padded{
      1, 2, 3, 4, 5, 6, 99, 99,
      7, 8, 9, 10, 11, 12, 88, 88};
  const ImageView padded_view{padded.data(), 2, 2, 8, 3};
  const DetectorRequest request{42, 1200, BBox{0, 0, 2, 2}, 3, 17};
  adapter.submit(padded_view, request);
  expect_true(!adapter.can_submit(),
              "pending or in-flight request closes the bounded slot");
  expect_throws([&] { adapter.submit(padded_view, request); }, "busy",
                "second active request is rejected");
  const ImageView invalid_while_busy{nullptr, 2, 2, 6, 3};
  expect_throws([&] { adapter.submit(invalid_while_busy, request); }, "busy",
                "busy state is rejected before making another frame copy");
  expect_true(wait_for(copy_state, &EngineState::entered),
              "fake engine enters within timeout");
  // A coast runtime keeps polling even while the old detector inference is
  // blocked. Poll must not wait for that inference or consume its identity.
  auto blocked_poll = std::async(std::launch::async, [&] { return adapter.poll(); });
  const bool poll_ready = blocked_poll.wait_for(std::chrono::milliseconds(100)) ==
      std::future_status::ready;
  std::fill(padded.begin(), padded.end(), 0);
  release_engine(copy_state);
  const auto polled_while_blocked = blocked_poll.get();
  expect_true(poll_ready && !polled_while_blocked,
              "poll returns empty without waiting for an in-flight pre-coast request");
  expect_true(wait_for(copy_state, &EngineState::finished),
              "fake engine finishes within timeout");
  expect_throws([&] { adapter.submit(padded_view, request); }, "busy",
                "unconsumed result keeps bounded slot closed");

  const std::optional<DetectorBatch> batch = poll_until_batch(adapter);
  expect_true(batch.has_value(), "completed result becomes pollable");
  if (batch) {
    expect_true(batch->source_frame_id == 42 &&
                    batch->source_timestamp_ms == 1200 &&
                    batch->generation == 17,
                "batch preserves source request identity and eligibility generation");
    expect_true(batch->raw_candidate_count == 2 &&
                    batch->filtered_candidate_count == 1 &&
                    batch->timing.total_ms == 7.0F &&
                    batch->candidates.size() == 1,
                "batch preserves engine result diagnostics");
  }
  const std::vector<std::uint8_t> expected_pixels{
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  {
    std::lock_guard<std::mutex> lock(copy_state->mutex);
    expect_true(copy_state->observed_active_pixels == expected_pixels,
                "worker reads an owned active-pixel copy after caller mutation");
    expect_true(copy_state->width == 2 && copy_state->height == 2 &&
                    copy_state->channels == 3 &&
                    copy_state->stride_bytes == 6,
                "worker receives tightly packed owned BGR rows");
    expect_true(copy_state->request.frame_id == 42,
                "worker receives the complete detector request");
  }
  expect_true(adapter.can_submit(),
              "consuming the result reopens the bounded slot");

  const ImageView null_image{nullptr, 2, 2, 6, 3};
  expect_throws([&] { adapter.submit(null_image, request); }, "BGR8",
                "null image is rejected before allocation");
  const ImageView short_stride{padded.data(), 2, 2, 5, 3};
  expect_throws([&] { adapter.submit(short_stride, request); }, "stride",
                "short source stride is rejected");
  const DetectorRequest outside{43, 1300, BBox{1, 1, 2, 2}, 3, 18};
  expect_throws([&] { adapter.submit(padded_view, outside); }, "ROI",
                "request ROI outside the image is rejected");

  auto error_state = std::make_shared<EngineState>();
  error_state->throw_error = true;
  YoloDetectorAdapter error_adapter(
      std::make_unique<FakeEngine>(error_state));
  std::vector<std::uint8_t> image_bytes(12, 9);
  const ImageView image{image_bytes.data(), 2, 2, 6, 3};
  test_blocking_protocol(image, request);
  test_inflight_exclusivity(image, request);
  test_failure_and_stop(image, request);
  error_adapter.submit(image, request);
  expect_true(wait_for(error_state, &EngineState::finished),
              "throwing fake engine finishes within timeout");
  bool observed_error = false;
  const auto error_deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (!observed_error && std::chrono::steady_clock::now() < error_deadline) {
    try {
      error_adapter.poll();
    } catch (const std::exception& error) {
      observed_error =
          std::string(error.what()).find("inference stage failed") !=
          std::string::npos;
    }
    std::this_thread::yield();
  }
  expect_true(observed_error, "worker exception is surfaced exactly by poll");
  expect_true(!error_adapter.available() && !error_adapter.can_submit(),
              "worker exception permanently disables the adapter");
  expect_true(!error_adapter.poll().has_value(),
              "worker exception is consumed exactly once");

  auto join_state = std::make_shared<EngineState>();
  join_state->release = false;
  auto joining_adapter = std::make_unique<YoloDetectorAdapter>(
      std::make_unique<FakeEngine>(join_state));
  joining_adapter->submit(image, request);
  expect_true(wait_for(join_state, &EngineState::entered),
              "destructor test inference enters within timeout");
  auto destruction = std::async(
      std::launch::async,
      [owned = std::move(joining_adapter)]() mutable { owned.reset(); });
  expect_true(destruction.wait_for(std::chrono::milliseconds(20)) ==
                  std::future_status::timeout,
              "destructor waits while inference is active");
  release_engine(join_state);
  expect_true(destruction.wait_for(std::chrono::seconds(1)) ==
                  std::future_status::ready,
              "destructor joins promptly after inference exits");
  destruction.get();
  expect_true(wait_for(join_state, &EngineState::finished) &&
                  join_state->calls == 1,
              "joined worker completes once and cannot outlive adapter");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "YOLO detector adapter tests passed\n";
  return 0;
}
