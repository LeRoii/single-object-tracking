#include "yolov8_postprocess.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
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

void expect_near(float actual,
                 float expected,
                 float tolerance,
                 const std::string& message) {
  expect_true(std::fabs(actual - expected) <= tolerance,
              message + " actual=" + std::to_string(actual) +
                  " expected=" + std::to_string(expected));
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

struct TensorStorage {
  nanotrack::YoloTensorType type = nanotrack::YoloTensorType::kFloat32;
  std::vector<std::int64_t> shape;
  std::vector<float> fp32;
  std::vector<std::int8_t> int8;
  float scale = 1.0F;
  std::int32_t zero_point = 0;

  nanotrack::YoloTensorView view() const {
    const bool is_float = type == nanotrack::YoloTensorType::kFloat32;
    return nanotrack::YoloTensorView{
        type,
        nanotrack::YoloTensorLayout::kNchw,
        is_float ? static_cast<const void*>(fp32.data())
                 : static_cast<const void*>(int8.data()),
        is_float ? fp32.size() * sizeof(float)
                 : int8.size() * sizeof(std::int8_t),
        shape,
        scale,
        zero_point};
  }
};

struct ScaleStorage {
  TensorStorage box;
  TensorStorage classes;
  TensorStorage score_sum;
  int height = 0;
  int width = 0;
  int dfl_bins = 0;
  int class_count = 0;
};

std::size_t offset(int channel, int y, int x, int height, int width) {
  return static_cast<std::size_t>(channel * height * width + y * width + x);
}

std::vector<ScaleStorage> make_scales(int dfl_bins = 2,
                                      int class_count = 2) {
  const int grids[][2] = {{4, 4}, {2, 2}, {1, 1}};
  std::vector<ScaleStorage> result;
  for (const auto& grid : grids) {
    ScaleStorage scale;
    scale.height = grid[0];
    scale.width = grid[1];
    scale.dfl_bins = dfl_bins;
    scale.class_count = class_count;
    const std::size_t cells =
        static_cast<std::size_t>(scale.height * scale.width);
    scale.box.shape = {1, 4 * dfl_bins, scale.height, scale.width};
    scale.box.fp32.assign(cells * static_cast<std::size_t>(4 * dfl_bins),
                          0.0F);
    scale.classes.shape = {1, class_count, scale.height, scale.width};
    scale.classes.fp32.assign(cells * static_cast<std::size_t>(class_count),
                              0.01F);
    scale.score_sum.shape = {1, 1, scale.height, scale.width};
    scale.score_sum.fp32.assign(cells, 0.01F);
    result.push_back(std::move(scale));
  }
  return result;
}

void activate(ScaleStorage& scale,
              int class_id,
              float confidence,
              int x = 0,
              int y = 0) {
  scale.classes.fp32[offset(class_id, y, x, scale.height, scale.width)] =
      confidence;
  scale.score_sum.fp32[offset(0, y, x, scale.height, scale.width)] =
      confidence;
}

void select_dfl_bin(ScaleStorage& scale, int bin, int x = 0, int y = 0) {
  for (int side = 0; side < 4; ++side) {
    for (int index = 0; index < scale.dfl_bins; ++index) {
      scale.box.fp32[offset(side * scale.dfl_bins + index, y, x,
                              scale.height, scale.width)] =
          index == bin ? 20.0F : -20.0F;
    }
  }
}

std::vector<nanotrack::YoloV8ScaleTensors> views(
    const std::vector<ScaleStorage>& storage,
    bool include_score_sum) {
  std::vector<nanotrack::YoloV8ScaleTensors> result;
  for (const ScaleStorage& scale : storage) {
    nanotrack::YoloV8ScaleTensors tensors;
    tensors.box = scale.box.view();
    tensors.classes = scale.classes.view();
    if (include_score_sum) tensors.score_sum = scale.score_sum.view();
    result.push_back(std::move(tensors));
  }
  return result;
}

nanotrack::YoloV8DecodeOptions default_options() {
  nanotrack::YoloV8DecodeOptions options;
  options.input_width = 32;
  options.input_height = 32;
  options.confidence_threshold = 0.25F;
  options.nms_threshold = 0.45F;
  options.class_id = -1;
  options.max_candidates = 10;
  return options;
}

nanotrack::LetterboxTransform identity_transform() {
  nanotrack::LetterboxTransform transform;
  transform.roi = nanotrack::BBox{0, 0, 32, 32};
  transform.frame_width = 32;
  transform.frame_height = 32;
  transform.scale = 1.0F;
  transform.pad_x = 0.0F;
  transform.pad_y = 0.0F;
  return transform;
}

void expect_box(const nanotrack::BBox& actual,
                const nanotrack::BBox& expected,
                const std::string& message) {
  expect_near(actual.x, expected.x, 1e-3F, message + " x");
  expect_near(actual.y, expected.y, 1e-3F, message + " y");
  expect_near(actual.width, expected.width, 1e-3F, message + " width");
  expect_near(actual.height, expected.height, 1e-3F, message + " height");
}

void quantize(std::vector<ScaleStorage>& storage, float scale) {
  for (ScaleStorage& item : storage) {
    TensorStorage* tensors[] = {&item.box, &item.classes, &item.score_sum};
    for (TensorStorage* tensor : tensors) {
      tensor->type = nanotrack::YoloTensorType::kInt8;
      tensor->scale = scale;
      tensor->zero_point = 0;
      tensor->int8.clear();
      tensor->int8.reserve(tensor->fp32.size());
      for (const float value : tensor->fp32) {
        tensor->int8.push_back(static_cast<std::int8_t>(std::lround(value / scale)));
      }
      tensor->fp32.clear();
    }
  }
}

}  // namespace

int main() {
  using namespace nanotrack;

  std::vector<ScaleStorage> literal = make_scales();
  activate(literal[0], 0, 0.90F);
  activate(literal[1], 0, 0.80F);
  activate(literal[2], 0, 0.70F);
  const YoloV8DecodeOptions options = default_options();
  const LetterboxTransform identity = identity_transform();
  const std::vector<DetectorCandidate> decoded =
      decode_yolov8(views(literal, true), options, identity);
  expect_true(decoded.size() == 3,
              "three DFL scales produce three literal candidates");
  if (decoded.size() == 3) {
    expect_box(decoded[0].bbox, BBox{0, 0, 8, 8},
               "stride-8 zero-logit DFL expectation is 0.5");
    expect_box(decoded[1].bbox, BBox{0, 0, 16, 16},
               "stride-16 zero-logit DFL expectation is 0.5");
    expect_box(decoded[2].bbox, BBox{0, 0, 32, 32},
               "stride-32 zero-logit DFL expectation is 0.5");
    expect_true(decoded[0].confidence > decoded[1].confidence &&
                    decoded[1].confidence > decoded[2].confidence,
                "candidates are sorted by their own confidence");
  }

  const std::vector<DetectorCandidate> six_output =
      decode_yolov8(views(literal, false), options, identity);
  expect_true(six_output.size() == decoded.size(),
              "six-output form works without score-sum tensors");
  std::vector<ScaleStorage> score_gated = literal;
  score_gated[0].score_sum.fp32[0] = 0.10F;
  expect_true(decode_yolov8(views(score_gated, true), options, identity).size() ==
                  2 &&
                  decode_yolov8(views(score_gated, false), options, identity)
                          .size() == 3,
              "optional score-sum tensor provides an early cell gate");
  std::vector<YoloV8ScaleTensors> reversed = views(literal, true);
  std::reverse(reversed.begin(), reversed.end());
  const std::vector<DetectorCandidate> reversed_decoded =
      decode_yolov8(reversed, options, identity);
  expect_true(reversed_decoded.size() == decoded.size() &&
                  reversed_decoded.front().confidence ==
                      decoded.front().confidence,
              "explicit tensor roles do not depend on scale vector order");

  std::vector<ScaleStorage> quantized_source = make_scales();
  activate(quantized_source[0], 1, 0.80F);
  const std::vector<DetectorCandidate> fp32_candidate =
      decode_yolov8(views(quantized_source, true), options, identity);
  quantize(quantized_source, 0.1F);
  const std::vector<DetectorCandidate> int8_candidate =
      decode_yolov8(views(quantized_source, true), options, identity);
  expect_true(fp32_candidate.size() == 1 && int8_candidate.size() == 1,
              "FP32 and INT8 fixtures each decode one candidate");
  if (fp32_candidate.size() == 1 && int8_candidate.size() == 1) {
    expect_box(int8_candidate[0].bbox, fp32_candidate[0].bbox,
               "INT8 dequantization preserves decoded box");
    expect_near(int8_candidate[0].confidence,
                fp32_candidate[0].confidence, 1e-5F,
                "INT8 dequantization preserves confidence");
    expect_true(int8_candidate[0].class_id == 1,
                "INT8 dequantization preserves class");
  }

  std::vector<ScaleStorage> ranked = make_scales();
  activate(ranked[0], 0, 0.95F);
  activate(ranked[1], 0, 0.90F);
  activate(ranked[2], 1, 0.85F);
  YoloV8DecodeOptions nms_options = options;
  nms_options.nms_threshold = 0.01F;
  std::vector<DetectorCandidate> nms_result =
      decode_yolov8(views(ranked, false), nms_options, identity);
  expect_true(nms_result.size() == 2 && nms_result[0].class_id == 0 &&
                  nms_result[1].class_id == 1,
              "NMS suppresses overlap per class but preserves another class");
  const YoloV8DecodeResult counted =
      decode_yolov8_with_stats(views(ranked, false), nms_options, identity);
  expect_true(counted.raw_candidate_count == 3 &&
                  counted.filtered_candidate_count == 2 &&
                  counted.candidates.size() == 2,
              "decoder reports pre-NMS and delivered candidate counts");
  nms_options.class_id = 1;
  nms_result = decode_yolov8(views(ranked, false), nms_options, identity);
  expect_true(nms_result.size() == 1 && nms_result[0].class_id == 1,
              "class filter retains only the requested class");
  nms_options.class_id = 0;
  nms_options.max_candidates = 1;
  nms_result = decode_yolov8(views(ranked, false), nms_options, identity);
  expect_true(nms_result.size() == 1 &&
                  std::fabs(nms_result[0].confidence - 0.95F) < 1e-5F,
              "top-N truncation keeps the highest-confidence candidate");

  std::vector<ScaleStorage> untruncated = make_scales();
  activate(untruncated[0], 0, 0.99F, 0, 0);
  activate(untruncated[0], 0, 0.98F, 1, 0);
  activate(untruncated[0], 0, 0.97F, 2, 0);
  activate(untruncated[0], 0, 0.60F, 3, 0);
  YoloV8DecodeOptions untruncated_options = options;
  untruncated_options.class_id = 0;
  untruncated_options.max_candidates =
      std::numeric_limits<std::size_t>::max();
  const std::vector<DetectorCandidate> untruncated_result = decode_yolov8(
      views(untruncated, false), untruncated_options, identity);
  expect_true(untruncated_result.size() == 4,
              "unbounded detector decode preserves lower-confidence candidates "
              "for controller position gating");

  nms_options.class_id = 2;
  expect_throws(
      [&] { decode_yolov8(views(ranked, false), nms_options, identity); },
      "class", "class filter outside tensor class count is rejected");

  std::vector<ScaleStorage> horizontal = make_scales();
  activate(horizontal[0], 0, 0.90F, 0, 0);
  activate(horizontal[0], 0, 0.80F, 1, 0);
  activate(horizontal[0], 0, 0.70F, 3, 0);
  LetterboxTransform horizontal_transform;
  horizontal_transform.roi = BBox{10, 5, 16, 32};
  horizontal_transform.frame_width = 64;
  horizontal_transform.frame_height = 64;
  horizontal_transform.scale = 1.0F;
  horizontal_transform.pad_x = 8.0F;
  horizontal_transform.pad_y = 0.0F;
  std::vector<DetectorCandidate> horizontal_result = decode_yolov8(
      views(horizontal, false), options, horizontal_transform);
  expect_true(horizontal_result.size() == 1,
              "boxes entirely in horizontal padding are rejected");
  if (horizontal_result.size() == 1) {
    expect_box(horizontal_result[0].bbox, BBox{10, 5, 8, 8},
               "horizontal letterbox inversion applies ROI offset");
  }

  std::vector<ScaleStorage> vertical = make_scales();
  activate(vertical[0], 0, 0.90F, 0, 0);
  activate(vertical[0], 0, 0.80F, 0, 1);
  activate(vertical[0], 0, 0.70F, 0, 3);
  LetterboxTransform vertical_transform;
  vertical_transform.roi = BBox{4, 20, 32, 16};
  vertical_transform.frame_width = 64;
  vertical_transform.frame_height = 64;
  vertical_transform.scale = 1.0F;
  vertical_transform.pad_x = 0.0F;
  vertical_transform.pad_y = 8.0F;
  std::vector<DetectorCandidate> vertical_result =
      decode_yolov8(views(vertical, false), options, vertical_transform);
  expect_true(vertical_result.size() == 1,
              "boxes entirely in vertical padding are rejected");
  if (vertical_result.size() == 1) {
    expect_box(vertical_result[0].bbox, BBox{4, 20, 8, 8},
               "vertical letterbox inversion applies ROI offset");
  }

  std::vector<ScaleStorage> clipped = make_scales();
  activate(clipped[0], 0, 0.90F);
  select_dfl_bin(clipped[0], 1);
  const std::vector<DetectorCandidate> clipped_result =
      decode_yolov8(views(clipped, false), options, identity);
  expect_true(clipped_result.size() == 1,
              "partially out-of-frame box survives clipping");
  if (clipped_result.size() == 1) {
    expect_box(clipped_result[0].bbox, BBox{0, 0, 12, 12},
               "partially out-of-frame box is clipped to valid bounds");
  }

  std::vector<YoloV8ScaleTensors> invalid = views(literal, true);
  invalid.pop_back();
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "three",
                "decoder requires exactly three scales");
  invalid = views(literal, true);
  invalid[0].box.shape.pop_back();
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "NCHW",
                "rank other than four is rejected");
  invalid = views(literal, true);
  invalid[0].box.layout = YoloTensorLayout::kNhwc;
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "NCHW",
                "NHWC tensor is rejected");
  invalid = views(literal, true);
  invalid[0].box.shape[0] = 2;
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "batch",
                "batch size other than one is rejected");
  invalid = views(literal, true);
  invalid[0].box.shape[1] = 7;
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "DFL",
                "box channels not divisible by four are rejected");
  invalid = views(literal, true);
  invalid[1].classes.shape[1] = 1;
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "class",
                "inconsistent class counts are rejected");
  invalid = views(literal, true);
  invalid[0].score_sum.reset();
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "score-sum",
                "mixed six/nine-output form is rejected");
  invalid = views(literal, true);
  invalid[0].classes.data = nullptr;
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "null",
                "null tensor storage is rejected");
  invalid = views(literal, true);
  invalid[0].classes.byte_size = 1;
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "byte",
                "short tensor storage is rejected");
  invalid = views(quantized_source, true);
  invalid[0].classes.scale = 0.0F;
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "scale",
                "invalid INT8 dequantization scale is rejected");
  invalid = views(literal, true);
  invalid[0].box.type = static_cast<YoloTensorType>(99);
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "type",
                "unsupported tensor type is rejected");
  std::vector<ScaleStorage> nonfinite = make_scales();
  activate(nonfinite[0], 0, std::numeric_limits<float>::quiet_NaN());
  expect_throws(
      [&] { decode_yolov8(views(nonfinite, false), options, identity); },
      "finite", "non-finite tensor values are rejected");
  invalid = views(literal, true);
  invalid[0].box.shape = {1, std::numeric_limits<std::int64_t>::max(), 2, 2};
  expect_throws([&] { decode_yolov8(invalid, options, identity); }, "overflow",
                "tensor shape multiplication overflow is rejected");

  YoloV8DecodeOptions invalid_options = options;
  invalid_options.confidence_threshold = 0.0F;
  expect_throws(
      [&] { decode_yolov8(views(literal, false), invalid_options, identity); },
      "confidence", "invalid confidence threshold is rejected");
  invalid_options = options;
  invalid_options.max_candidates = 0;
  expect_throws(
      [&] { decode_yolov8(views(literal, false), invalid_options, identity); },
      "max_candidates", "zero maximum candidate count is rejected");
  LetterboxTransform invalid_transform = identity;
  invalid_transform.scale = 0.0F;
  expect_throws(
      [&] { decode_yolov8(views(literal, false), options, invalid_transform); },
      "letterbox", "non-positive letterbox scale is rejected");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "YOLOv8 postprocess tests passed\n";
  return 0;
}
