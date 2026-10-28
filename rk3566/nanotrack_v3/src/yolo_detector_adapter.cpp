#include "yolo_detector_adapter.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace nanotrack {

ImageView YoloDetectorAdapter::OwnedWork::view() const {
  return ImageView{pixels.data(), width, height,
                   static_cast<std::size_t>(width) *
                       static_cast<std::size_t>(channels),
                   channels};
}

YoloDetectorAdapter::YoloDetectorAdapter(
    std::unique_ptr<DetectorEngine> engine)
    : engine_(std::move(engine)) {
  if (!engine_) throw std::invalid_argument("detector engine must not be null");
  worker_ = std::thread(&YoloDetectorAdapter::worker_loop, this);
}

YoloDetectorAdapter::~YoloDetectorAdapter() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    pending_.reset();
  }
  changed_.notify_one();
  if (worker_.joinable()) worker_.join();
}

bool YoloDetectorAdapter::available() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !stopping_ && !disabled_;
}

bool YoloDetectorAdapter::can_submit_locked() const {
  return !stopping_ && !disabled_ && !pending_error_ && !pending_ &&
         !in_flight_ && !completed_;
}

bool YoloDetectorAdapter::can_submit() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return can_submit_locked();
}

YoloDetectorAdapter::OwnedWork YoloDetectorAdapter::copy_work(
    const ImageView& image,
    const DetectorRequest& request) {
  if (image.data == nullptr || image.width <= 0 || image.height <= 0 ||
      image.channels != 3) {
    throw std::invalid_argument("detector input must be a valid BGR8 image");
  }
  const std::size_t width = static_cast<std::size_t>(image.width);
  const std::size_t height = static_cast<std::size_t>(image.height);
  const std::size_t channels = static_cast<std::size_t>(image.channels);
  if (width > std::numeric_limits<std::size_t>::max() / channels) {
    throw std::overflow_error("detector input row size overflows");
  }
  const std::size_t row_bytes = width * channels;
  if (image.stride_bytes < row_bytes) {
    throw std::invalid_argument("detector input stride is shorter than a BGR row");
  }
  if (height > std::numeric_limits<std::size_t>::max() / row_bytes) {
    throw std::overflow_error("detector input byte size overflows");
  }
  try {
    validate_bbox(request.roi, image.width, image.height);
  } catch (const std::exception& error) {
    throw std::invalid_argument(std::string("detector request ROI is invalid: ") +
                                error.what());
  }
  if (request.max_candidates == 0) {
    throw std::invalid_argument(
        "detector request max_candidates must be positive");
  }

  OwnedWork work;
  work.width = image.width;
  work.height = image.height;
  work.channels = image.channels;
  work.request = request;
  work.pixels.resize(row_bytes * height);
  for (std::size_t row = 0; row < height; ++row) {
    std::memcpy(work.pixels.data() + row * row_bytes,
                image.data + row * image.stride_bytes, row_bytes);
  }
  return work;
}

void YoloDetectorAdapter::submit(const ImageView& image,
                                 const DetectorRequest& request) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!can_submit_locked()) {
      throw std::logic_error("detector adapter is busy or unavailable");
    }
    pending_ = copy_work(image, request);
  }
  changed_.notify_one();
}

std::optional<DetectorBatch> YoloDetectorAdapter::poll() {
  std::exception_ptr error;
  std::optional<DetectorBatch> batch;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_error_) {
      error = pending_error_;
      pending_error_ = nullptr;
      disabled_ = true;
    } else if (completed_) {
      batch = std::move(completed_);
      completed_.reset();
    }
  }
  if (error) std::rethrow_exception(error);
  return batch;
}

void YoloDetectorAdapter::worker_loop() {
  while (true) {
    OwnedWork work;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      changed_.wait(lock, [&] { return stopping_ || pending_.has_value(); });
      if (stopping_) return;
      work = std::move(*pending_);
      pending_.reset();
      in_flight_ = true;
    }

    try {
      DetectorEngineResult engine_result = engine_->infer(work.view(), work.request);
      DetectorBatch batch;
      batch.source_frame_id = work.request.frame_id;
      batch.source_timestamp_ms = work.request.timestamp_ms;
      batch.generation = work.request.generation;
      batch.candidates = std::move(engine_result.candidates);
      batch.raw_candidate_count = engine_result.raw_candidate_count;
      batch.filtered_candidate_count = engine_result.filtered_candidate_count;
      batch.timing = engine_result.timing;
      std::lock_guard<std::mutex> lock(mutex_);
      in_flight_ = false;
      if (stopping_) return;
      completed_ = std::move(batch);
    } catch (...) {
      std::lock_guard<std::mutex> lock(mutex_);
      in_flight_ = false;
      if (stopping_) return;
      pending_error_ = std::current_exception();
    }
  }
}

}  // namespace nanotrack
