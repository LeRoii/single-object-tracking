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
  std::fill(padded.begin(), padded.end(), 0);
  release_engine(copy_state);
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
