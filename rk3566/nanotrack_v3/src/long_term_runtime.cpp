#include "long_term_runtime.hpp"

#include <chrono>
#include <exception>
#include <optional>
#include <stdexcept>

namespace nanotrack {

LongTermRuntime::LongTermRuntime(const ControllerConfig& config,
                                 ShortTermTracker& tracker,
                                 DetectorAdapter& detector)
    : tracker_(tracker), detector_(detector), controller_(config) {}

void LongTermRuntime::validate_image(const ImageView& image) {
  if (image.data == nullptr || image.width <= 0 || image.height <= 0 ||
      image.channels != 3 ||
      image.stride_bytes <
          static_cast<std::size_t>(image.width * image.channels)) {
    throw std::invalid_argument("runtime image must be a valid BGR8 view");
  }
}

void LongTermRuntime::initialize(const ImageView& image,
                                 const BBox& initial_bbox,
                                 std::uint64_t frame_id,
                                 std::int64_t timestamp_ms) {
  validate_image(image);
  validate_bbox(initial_bbox, image.width, image.height);
  tracker_.initialize(image, initial_bbox);
  controller_.initialize(initial_bbox, frame_id, timestamp_ms,
                         FrameSize{image.width, image.height});
  initialized_ = true;
}

RuntimeFrameResult LongTermRuntime::process_frame(const ImageView& image,
                                                  std::uint64_t frame_id,
                                                  std::int64_t timestamp_ms) {
  if (!initialized_) throw std::logic_error("long-term runtime is not initialized");
  validate_image(image);
  const auto total_start = std::chrono::steady_clock::now();
  RuntimeFrameResult result;
  result.plan =
      controller_.begin_frame(frame_id, timestamp_ms, detector_.available());

  if (result.plan.request_detector && result.plan.detector_request) {
    const auto detector_start = std::chrono::steady_clock::now();
    try {
      detector_.submit(image, *result.plan.detector_request);
    } catch (const std::exception& error) {
      result.detector_error = true;
      result.detector_error_message = error.what();
    }
    result.detector_ms += std::chrono::duration<float, std::milli>(
                              std::chrono::steady_clock::now() - detector_start)
                              .count();
  }

  std::optional<DetectorBatch> detector_batch;
  if (detector_.available() && !result.detector_error) {
    const auto detector_start = std::chrono::steady_clock::now();
    try {
      detector_batch = detector_.poll();
    } catch (const std::exception& error) {
      result.detector_error = true;
      result.detector_error_message = error.what();
    }
    result.detector_ms += std::chrono::duration<float, std::milli>(
                              std::chrono::steady_clock::now() - detector_start)
                              .count();
  }

  const auto tracker_start = std::chrono::steady_clock::now();
  result.observations = tracker_.infer(image, result.plan);
  result.tracker_ms = std::chrono::duration<float, std::milli>(
                          std::chrono::steady_clock::now() - tracker_start)
                          .count();
  result.output =
      controller_.finish_frame(result.plan, result.observations, detector_batch);
  if (result.output.accepted) tracker_.commit_bbox(result.output.bbox);
  switch (result.output.template_action) {
    case TemplateAction::kNone: break;
    case TemplateAction::kStageRecent:
      tracker_.stage_recent(image, result.output.bbox);
      break;
    case TemplateAction::kCommitRecent: tracker_.commit_recent(); break;
    case TemplateAction::kRollbackRecent: tracker_.rollback_recent(); break;
  }
  result.runtime_ms = std::chrono::duration<float, std::milli>(
                          std::chrono::steady_clock::now() - total_start)
                          .count();
  return result;
}

}  // namespace nanotrack
