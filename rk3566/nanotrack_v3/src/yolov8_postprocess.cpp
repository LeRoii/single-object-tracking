#include "yolov8_postprocess.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nanotrack {
namespace {

constexpr int kMaxInputDimension = 8192;
constexpr std::size_t kMaxTensorElements = 100000000;

struct TensorInfo {
  std::size_t channels = 0;
  std::size_t height = 0;
  std::size_t width = 0;
  std::size_t elements = 0;
};

struct ValidatedScale {
  const YoloV8ScaleTensors* tensors = nullptr;
  TensorInfo box{};
  TensorInfo classes{};
  std::optional<TensorInfo> score_sum;
  std::size_t dfl_bins = 0;
  int stride = 0;
};

bool finite_open_unit(float value) {
  return std::isfinite(value) && value > 0.0F && value < 1.0F;
}

std::size_t checked_multiply(std::size_t first,
                             std::size_t second,
                             const std::string& context) {
  if (second != 0 && first > std::numeric_limits<std::size_t>::max() / second) {
    throw std::overflow_error(context + " shape multiplication overflow");
  }
  return first * second;
}

std::size_t element_size(YoloTensorType type) {
  switch (type) {
    case YoloTensorType::kFloat32: return sizeof(float);
    case YoloTensorType::kInt8: return sizeof(std::int8_t);
  }
  throw std::invalid_argument("unsupported YOLO tensor type");
}

TensorInfo validate_tensor(const YoloTensorView& tensor,
                           const std::string& role) {
  if (tensor.layout != YoloTensorLayout::kNchw || tensor.shape.size() != 4) {
    throw std::invalid_argument(role + " tensor must use rank-4 NCHW layout");
  }
  if (tensor.shape[0] != 1) {
    throw std::invalid_argument(role + " tensor batch size must be one");
  }
  for (const std::int64_t dimension : tensor.shape) {
    if (dimension <= 0) {
      throw std::invalid_argument(role + " tensor shape must be positive");
    }
  }
  const std::size_t item_bytes = element_size(tensor.type);
  if (tensor.type == YoloTensorType::kInt8 &&
      (!std::isfinite(tensor.scale) || tensor.scale <= 0.0F)) {
    throw std::invalid_argument(role + " INT8 scale must be finite and positive");
  }
  if (tensor.data == nullptr) {
    throw std::invalid_argument(role + " tensor data is null");
  }

  std::size_t elements = 1;
  for (const std::int64_t dimension : tensor.shape) {
    if (static_cast<std::uint64_t>(dimension) >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
      throw std::overflow_error(role + " tensor shape multiplication overflow");
    }
    elements = checked_multiply(elements,
                                static_cast<std::size_t>(dimension), role);
  }
  if (elements > kMaxTensorElements) {
    throw std::invalid_argument(role + " tensor exceeds bounded element limit");
  }
  const std::size_t required_bytes =
      checked_multiply(elements, item_bytes, role + " byte size");
  if (tensor.byte_size < required_bytes) {
    throw std::invalid_argument(role + " tensor byte storage is too short");
  }

  return TensorInfo{static_cast<std::size_t>(tensor.shape[1]),
                    static_cast<std::size_t>(tensor.shape[2]),
                    static_cast<std::size_t>(tensor.shape[3]), elements};
}

float tensor_value(const YoloTensorView& tensor, std::size_t index) {
  float value = 0.0F;
  switch (tensor.type) {
    case YoloTensorType::kFloat32:
      value = static_cast<const float*>(tensor.data)[index];
      break;
    case YoloTensorType::kInt8:
      value =
          (static_cast<int>(static_cast<const std::int8_t*>(tensor.data)[index]) -
           tensor.zero_point) *
          tensor.scale;
      break;
    default: throw std::invalid_argument("unsupported YOLO tensor type");
  }
  if (!std::isfinite(value)) {
    throw std::invalid_argument("YOLO tensor values must be finite");
  }
  return value;
}

void validate_all_finite(const YoloTensorView& tensor,
                         const TensorInfo& info) {
  for (std::size_t index = 0; index < info.elements; ++index) {
    static_cast<void>(tensor_value(tensor, index));
  }
}

std::size_t nchw_offset(std::size_t channel,
                        std::size_t y,
                        std::size_t x,
                        const TensorInfo& info) {
  return (channel * info.height + y) * info.width + x;
}

float dfl_distance(const YoloTensorView& tensor,
                   const TensorInfo& info,
                   std::size_t side,
                   std::size_t bins,
                   std::size_t y,
                   std::size_t x) {
  float maximum = -std::numeric_limits<float>::infinity();
  for (std::size_t bin = 0; bin < bins; ++bin) {
    maximum = std::max(maximum,
                       tensor_value(tensor,
                                    nchw_offset(side * bins + bin, y, x, info)));
  }
  float denominator = 0.0F;
  float weighted = 0.0F;
  for (std::size_t bin = 0; bin < bins; ++bin) {
    const float logit = tensor_value(
        tensor, nchw_offset(side * bins + bin, y, x, info));
    const float probability = std::exp(logit - maximum);
    denominator += probability;
    weighted += static_cast<float>(bin) * probability;
  }
  if (!std::isfinite(denominator) || denominator <= 0.0F ||
      !std::isfinite(weighted)) {
    throw std::invalid_argument("DFL softmax produced a non-finite value");
  }
  return weighted / denominator;
}

float intersection_over_union(const BBox& first, const BBox& second) {
  const float left = std::max(first.x, second.x);
  const float top = std::max(first.y, second.y);
  const float right =
      std::min(first.x + first.width, second.x + second.width);
  const float bottom =
      std::min(first.y + first.height, second.y + second.height);
  const float intersection_width = std::max(0.0F, right - left);
  const float intersection_height = std::max(0.0F, bottom - top);
  const float intersection = intersection_width * intersection_height;
  const float union_area = first.width * first.height +
                           second.width * second.height - intersection;
  return union_area > 0.0F ? intersection / union_area : 0.0F;
}

std::optional<BBox> inverse_letterbox(float model_x1,
                                      float model_y1,
                                      float model_x2,
                                      float model_y2,
                                      const LetterboxTransform& transform) {
  const float global_x1 =
      transform.roi.x + (model_x1 - transform.pad_x) / transform.scale;
  const float global_y1 =
      transform.roi.y + (model_y1 - transform.pad_y) / transform.scale;
  const float global_x2 =
      transform.roi.x + (model_x2 - transform.pad_x) / transform.scale;
  const float global_y2 =
      transform.roi.y + (model_y2 - transform.pad_y) / transform.scale;
  const float values[] = {global_x1, global_y1, global_x2, global_y2};
  for (const float value : values) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("letterbox inversion produced non-finite box");
    }
  }

  const float roi_right = transform.roi.x + transform.roi.width;
  const float roi_bottom = transform.roi.y + transform.roi.height;
  const float clipped_x1 =
      std::max({0.0F, transform.roi.x, global_x1});
  const float clipped_y1 =
      std::max({0.0F, transform.roi.y, global_y1});
  const float clipped_x2 =
      std::min({static_cast<float>(transform.frame_width), roi_right, global_x2});
  const float clipped_y2 =
      std::min({static_cast<float>(transform.frame_height), roi_bottom, global_y2});
  if (!(clipped_x2 > clipped_x1) || !(clipped_y2 > clipped_y1)) {
    return std::nullopt;
  }
  return BBox{clipped_x1, clipped_y1, clipped_x2 - clipped_x1,
              clipped_y2 - clipped_y1};
}

void validate_options(const YoloV8DecodeOptions& options,
                      const LetterboxTransform& transform) {
  if (options.input_width <= 0 || options.input_height <= 0 ||
      options.input_width > kMaxInputDimension ||
      options.input_height > kMaxInputDimension) {
    throw std::invalid_argument("YOLO input dimensions are invalid");
  }
  if (!finite_open_unit(options.confidence_threshold)) {
    throw std::invalid_argument(
        "confidence threshold must be finite and in (0,1)");
  }
  if (!finite_open_unit(options.nms_threshold)) {
    throw std::invalid_argument("NMS threshold must be finite and in (0,1)");
  }
  if (options.class_id < -1) {
    throw std::invalid_argument("class filter must be -1 or non-negative");
  }
  if (options.max_candidates == 0) {
    throw std::invalid_argument("max_candidates must be positive");
  }
  if (transform.frame_width <= 0 || transform.frame_height <= 0 ||
      !std::isfinite(transform.scale) || transform.scale <= 0.0F ||
      !std::isfinite(transform.pad_x) || !std::isfinite(transform.pad_y) ||
      transform.pad_x < 0.0F || transform.pad_y < 0.0F) {
    throw std::invalid_argument("letterbox transform is invalid");
  }
  try {
    validate_bbox(transform.roi, transform.frame_width, transform.frame_height);
  } catch (const std::exception& error) {
    throw std::invalid_argument(std::string("letterbox ROI is invalid: ") +
                                error.what());
  }
}

std::vector<ValidatedScale> validate_scales(
    const std::vector<YoloV8ScaleTensors>& scales,
    const YoloV8DecodeOptions& options) {
  if (scales.size() != 3) {
    throw std::invalid_argument("YOLOv8 decoder requires exactly three scales");
  }
  const bool score_sum_present = scales.front().score_sum.has_value();
  std::vector<ValidatedScale> validated;
  validated.reserve(scales.size());
  std::optional<std::size_t> class_count;
  std::optional<std::size_t> dfl_bins;
  std::set<int> strides;
  for (const YoloV8ScaleTensors& tensors : scales) {
    if (tensors.score_sum.has_value() != score_sum_present) {
      throw std::invalid_argument(
          "score-sum tensors must be present for all scales or none");
    }
    ValidatedScale scale;
    scale.tensors = &tensors;
    scale.box = validate_tensor(tensors.box, "box");
    scale.classes = validate_tensor(tensors.classes, "class");
    if (tensors.score_sum) {
      scale.score_sum = validate_tensor(*tensors.score_sum, "score-sum");
    }
    if (scale.box.channels < 8 || scale.box.channels % 4 != 0) {
      throw std::invalid_argument(
          "DFL box channel count must be divisible by four with at least two bins");
    }
    scale.dfl_bins = scale.box.channels / 4;
    if (scale.box.height != scale.classes.height ||
        scale.box.width != scale.classes.width) {
      throw std::invalid_argument("box and class tensor grids must match");
    }
    if (scale.score_sum &&
        (scale.score_sum->channels != 1 ||
         scale.score_sum->height != scale.box.height ||
         scale.score_sum->width != scale.box.width)) {
      throw std::invalid_argument(
          "score-sum tensor must have one channel and match the box grid");
    }
    if (class_count && *class_count != scale.classes.channels) {
      throw std::invalid_argument("class tensor counts must match across scales");
    }
    if (dfl_bins && *dfl_bins != scale.dfl_bins) {
      throw std::invalid_argument("DFL bin counts must match across scales");
    }
    class_count = scale.classes.channels;
    dfl_bins = scale.dfl_bins;
    if (scale.box.width > static_cast<std::size_t>(options.input_width) ||
        scale.box.height > static_cast<std::size_t>(options.input_height) ||
        options.input_width % static_cast<int>(scale.box.width) != 0 ||
        options.input_height % static_cast<int>(scale.box.height) != 0) {
      throw std::invalid_argument("tensor grid does not divide YOLO input");
    }
    const int stride_x =
        options.input_width / static_cast<int>(scale.box.width);
    const int stride_y =
        options.input_height / static_cast<int>(scale.box.height);
    if (stride_x != stride_y || stride_x <= 0 ||
        !strides.insert(stride_x).second) {
      throw std::invalid_argument(
          "three YOLO scales must have distinct square strides");
    }
    scale.stride = stride_x;
    validate_all_finite(tensors.box, scale.box);
    validate_all_finite(tensors.classes, scale.classes);
    if (tensors.score_sum) {
      validate_all_finite(*tensors.score_sum, *scale.score_sum);
    }
    validated.push_back(scale);
  }
  if (!class_count || *class_count == 0) {
    throw std::invalid_argument("class tensor must contain at least one class");
  }
  if (options.class_id >= 0 &&
      static_cast<std::size_t>(options.class_id) >= *class_count) {
    throw std::invalid_argument("class filter is outside tensor class count");
  }
  return validated;
}

}  // namespace

std::vector<DetectorCandidate> decode_yolov8(
    const std::vector<YoloV8ScaleTensors>& scales,
    const YoloV8DecodeOptions& options,
    const LetterboxTransform& transform) {
  validate_options(options, transform);
  const std::vector<ValidatedScale> validated =
      validate_scales(scales, options);

  std::size_t total_cells = 0;
  for (const ValidatedScale& scale : validated) {
    const std::size_t cells =
        checked_multiply(scale.box.height, scale.box.width, "candidate grid");
    if (total_cells > std::numeric_limits<std::size_t>::max() - cells) {
      throw std::overflow_error("candidate grid size overflow");
    }
    total_cells += cells;
  }
  std::vector<DetectorCandidate> candidates;
  candidates.reserve(total_cells);

  for (const ValidatedScale& scale : validated) {
    const YoloV8ScaleTensors& tensors = *scale.tensors;
    for (std::size_t y = 0; y < scale.box.height; ++y) {
      for (std::size_t x = 0; x < scale.box.width; ++x) {
        if (tensors.score_sum) {
          const float score_sum = tensor_value(
              *tensors.score_sum, nchw_offset(0, y, x, *scale.score_sum));
          if (score_sum < 0.0F) {
            throw std::invalid_argument("score-sum values must be non-negative");
          }
          if (score_sum < options.confidence_threshold) continue;
        }

        int class_id = options.class_id;
        float confidence = -std::numeric_limits<float>::infinity();
        if (class_id >= 0) {
          confidence = tensor_value(
              tensors.classes,
              nchw_offset(static_cast<std::size_t>(class_id), y, x,
                          scale.classes));
        } else {
          for (std::size_t candidate_class = 0;
               candidate_class < scale.classes.channels; ++candidate_class) {
            const float score = tensor_value(
                tensors.classes,
                nchw_offset(candidate_class, y, x, scale.classes));
            if (score > confidence) {
              confidence = score;
              class_id = static_cast<int>(candidate_class);
            }
          }
        }
        if (confidence < 0.0F || confidence > 1.0F) {
          throw std::invalid_argument(
              "class confidence must be finite and in [0,1]");
        }
        if (confidence < options.confidence_threshold) continue;

        const float left = dfl_distance(tensors.box, scale.box, 0,
                                        scale.dfl_bins, y, x);
        const float top = dfl_distance(tensors.box, scale.box, 1,
                                       scale.dfl_bins, y, x);
        const float right = dfl_distance(tensors.box, scale.box, 2,
                                         scale.dfl_bins, y, x);
        const float bottom = dfl_distance(tensors.box, scale.box, 3,
                                          scale.dfl_bins, y, x);
        const float center_x =
            (static_cast<float>(x) + 0.5F) * static_cast<float>(scale.stride);
        const float center_y =
            (static_cast<float>(y) + 0.5F) * static_cast<float>(scale.stride);
        const float stride = static_cast<float>(scale.stride);
        const std::optional<BBox> bbox = inverse_letterbox(
            center_x - left * stride, center_y - top * stride,
            center_x + right * stride, center_y + bottom * stride, transform);
        if (!bbox) continue;
        candidates.push_back(DetectorCandidate{*bbox, confidence, class_id});
      }
    }
  }

  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const DetectorCandidate& first,
                      const DetectorCandidate& second) {
                     return first.confidence > second.confidence;
                   });
  std::vector<DetectorCandidate> kept;
  kept.reserve(std::min(candidates.size(), options.max_candidates));
  for (const DetectorCandidate& candidate : candidates) {
    bool suppressed = false;
    for (const DetectorCandidate& accepted : kept) {
      if (candidate.class_id == accepted.class_id &&
          intersection_over_union(candidate.bbox, accepted.bbox) >
              options.nms_threshold) {
        suppressed = true;
        break;
      }
    }
    if (!suppressed) {
      kept.push_back(candidate);
      if (kept.size() == options.max_candidates) break;
    }
  }
  return kept;
}

}  // namespace nanotrack
