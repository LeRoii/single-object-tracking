#pragma once

#include "long_term_types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nanotrack {

enum class YoloTensorType { kFloat32, kInt8 };
enum class YoloTensorLayout { kNchw, kNhwc };

struct YoloTensorView {
  YoloTensorType type = YoloTensorType::kFloat32;
  YoloTensorLayout layout = YoloTensorLayout::kNchw;
  const void* data = nullptr;
  std::size_t byte_size = 0;
  std::vector<std::int64_t> shape;
  float scale = 1.0F;
  std::int32_t zero_point = 0;
};

struct YoloV8ScaleTensors {
  YoloTensorView box;
  YoloTensorView classes;
  std::optional<YoloTensorView> score_sum;
};

struct YoloV8DecodeOptions {
  int input_width = 640;
  int input_height = 640;
  float confidence_threshold = 0.25F;
  float nms_threshold = 0.45F;
  int class_id = -1;
  std::size_t max_candidates = 3;
};

struct LetterboxTransform {
  BBox roi{};
  int frame_width = 0;
  int frame_height = 0;
  float scale = 1.0F;
  float pad_x = 0.0F;
  float pad_y = 0.0F;
};

struct YoloV8DecodeResult {
  std::vector<DetectorCandidate> candidates;
  std::size_t raw_candidate_count = 0;
  std::size_t filtered_candidate_count = 0;
};

YoloV8DecodeResult decode_yolov8_with_stats(
    const std::vector<YoloV8ScaleTensors>& scales,
    const YoloV8DecodeOptions& options,
    const LetterboxTransform& transform);

std::vector<DetectorCandidate> decode_yolov8(
    const std::vector<YoloV8ScaleTensors>& scales,
    const YoloV8DecodeOptions& options,
    const LetterboxTransform& transform);

}  // namespace nanotrack
