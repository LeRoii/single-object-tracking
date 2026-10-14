#include "yolo_rknn_engine.hpp"

#include "scoped_cleanup.hpp"
#include "sha256.hpp"
#include "yolov8_postprocess.hpp"

#include <rknn_api.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nanotrack {
namespace {

constexpr int kInputSize = 640;
constexpr int kPadValue = 114;

struct TensorContract {
  std::string name;
  std::string role;
  std::vector<std::int64_t> shape;
  std::string dtype;
  std::string layout;
};

struct DetectorManifest {
  std::string model_file;
  std::string model_sha256;
  std::uintmax_t model_size = 0;
  std::string labels_file;
  std::string labels_sha256;
  int class_count = 0;
  int dfl_bins = 0;
  std::vector<TensorContract> outputs;
};

bool finite_open_unit(float value) {
  return std::isfinite(value) && value > 0.0F && value < 1.0F;
}

void check_rknn(int status,
                const std::string& stage,
                const std::string& model_path) {
  if (status != 0) {
    throw std::runtime_error("RKNN stage=" + stage + " status=" +
                             std::to_string(status) + " model=" + model_path);
  }
}

std::vector<std::uint8_t> read_binary_file(const std::string& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) throw std::runtime_error("cannot open detector model: " + path);
  const std::streamsize size = stream.tellg();
  if (size <= 0 ||
      static_cast<std::uint64_t>(size) >
          std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("detector model size is invalid: " + path);
  }
  stream.seekg(0, std::ios::beg);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  if (!stream.read(reinterpret_cast<char*>(bytes.data()), size)) {
    throw std::runtime_error("cannot read complete detector model: " + path);
  }
  return bytes;
}

std::vector<std::string> read_labels(const std::string& path) {
  std::ifstream stream(path);
  if (!stream) throw std::runtime_error("cannot open detector labels: " + path);
  std::vector<std::string> labels;
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) {
      throw std::runtime_error("detector labels contain an empty line: " + path);
    }
    labels.push_back(line);
  }
  if (!stream.eof() || labels.empty()) {
    throw std::runtime_error("detector labels are empty or unreadable: " + path);
  }
  return labels;
}

cv::FileNode require_node(const cv::FileNode& parent,
                          const std::string& key,
                          const std::string& context) {
  const cv::FileNode node = parent[key];
  if (node.empty()) {
    throw std::runtime_error("detector manifest missing " + context + "." + key);
  }
  return node;
}

std::string read_string(const cv::FileNode& parent,
                        const std::string& key,
                        const std::string& context) {
  const cv::FileNode node = require_node(parent, key, context);
  std::string value;
  node >> value;
  if (value.empty()) {
    throw std::runtime_error("detector manifest " + context + "." + key +
                             " must be a non-empty string");
  }
  return value;
}

int read_int(const cv::FileNode& parent,
             const std::string& key,
             const std::string& context) {
  const cv::FileNode node = require_node(parent, key, context);
  int value = 0;
  node >> value;
  return value;
}

bool read_bool(const cv::FileNode& parent,
               const std::string& key,
               const std::string& context) {
  const int value = read_int(parent, key, context);
  if (value != 0 && value != 1) {
    throw std::runtime_error("detector manifest " + context + "." + key +
                             " must be boolean");
  }
  return value != 0;
}

std::vector<std::int64_t> read_shape(const cv::FileNode& parent,
                                     const std::string& context) {
  const cv::FileNode shape = require_node(parent, "shape", context);
  if (!shape.isSeq() || shape.size() != 4) {
    throw std::runtime_error("detector manifest " + context +
                             ".shape must contain four dimensions");
  }
  std::vector<std::int64_t> result;
  for (const cv::FileNode& dimension : shape) {
    int value = 0;
    dimension >> value;
    if (value <= 0) {
      throw std::runtime_error("detector manifest tensor dimensions must be positive");
    }
    result.push_back(value);
  }
  return result;
}

void require_float_triplet(const cv::FileNode& conversion,
                           const std::string& key,
                           float expected) {
  const cv::FileNode outer = require_node(conversion, key, "conversion");
  if (!outer.isSeq() || outer.size() != 1 || !outer[0].isSeq() ||
      outer[0].size() != 3) {
    throw std::runtime_error("detector manifest conversion." + key +
                             " must be a nested RGB triplet");
  }
  for (const cv::FileNode& item : outer[0]) {
    float value = 0.0F;
    item >> value;
    if (!std::isfinite(value) || std::fabs(value - expected) > 1e-6F) {
      throw std::runtime_error("detector manifest conversion." + key +
                               " has an unexpected value");
    }
  }
}

DetectorManifest load_manifest(const YoloRknnEngineConfig& config) {
  cv::FileStorage storage(config.manifest_path,
                          cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  if (!storage.isOpened()) {
    throw std::runtime_error("cannot open detector manifest: " +
                             config.manifest_path);
  }
  const cv::FileNode root = storage.root();
  if (read_int(root, "schema_version", "manifest") != 1) {
    throw std::runtime_error("detector manifest schema_version must be 1");
  }
  if (read_bool(root, "calibrated", "manifest")) {
    throw std::runtime_error("detector manifest calibrated must be false");
  }
  if (read_string(root, "deployment_status", "manifest") !=
      "function_only_board_unverified") {
    throw std::runtime_error("detector manifest deployment_status is unsupported");
  }

  const cv::FileNode artifact = require_node(root, "artifact", "manifest");
  const cv::FileNode labels = require_node(root, "labels", "manifest");
  const cv::FileNode conversion = require_node(root, "conversion", "manifest");
  const cv::FileNode input = require_node(root, "input", "manifest");
  DetectorManifest manifest;
  manifest.model_file = read_string(artifact, "file", "artifact");
  manifest.model_sha256 = read_string(artifact, "sha256", "artifact");
  const int model_size = read_int(artifact, "size_bytes", "artifact");
  if (model_size <= 0) {
    throw std::runtime_error("detector manifest artifact.size_bytes must be positive");
  }
  manifest.model_size = static_cast<std::uintmax_t>(model_size);
  manifest.labels_file = read_string(labels, "file", "labels");
  manifest.labels_sha256 = read_string(labels, "sha256", "labels");
  manifest.class_count = read_int(root, "class_count", "manifest");
  manifest.dfl_bins = read_int(root, "dfl_bins", "manifest");
  if (manifest.class_count <= 0 || manifest.dfl_bins <= 1 ||
      read_int(labels, "count", "labels") != manifest.class_count) {
    throw std::runtime_error("detector manifest class or DFL count is invalid");
  }

  if (read_string(conversion, "target_platform", "conversion") != "rk3566") {
    throw std::runtime_error("detector manifest target_platform must be rk3566");
  }
  if (read_bool(conversion, "quantized", "conversion")) {
    throw std::runtime_error("detector manifest quantized must be false");
  }
  require_float_triplet(conversion, "mean_values", 0.0F);
  require_float_triplet(conversion, "std_values", 255.0F);

  if (read_shape(input, "input") !=
          std::vector<std::int64_t>({1, kInputSize, kInputSize, 3}) ||
      read_string(input, "dtype", "input") != "uint8" ||
      read_string(input, "layout", "input") != "NHWC" ||
      read_string(input, "color_order", "input") != "RGB" ||
      read_int(input, "pad_value", "input") != kPadValue) {
    throw std::runtime_error(
        "detector manifest input must be RGB UINT8 NHWC 1x640x640x3 with pad 114");
  }

  const cv::FileNode outputs = require_node(root, "outputs", "manifest");
  if (!outputs.isSeq() || (outputs.size() != 6 && outputs.size() != 9)) {
    throw std::runtime_error("detector manifest must declare six or nine outputs");
  }
  const bool score_sum = outputs.size() == 9;
  std::size_t output_index = 0;
  for (const int stride : {8, 16, 32}) {
    const int grid = kInputSize / stride;
    const std::vector<std::pair<std::string, int>> roles = score_sum
        ? std::vector<std::pair<std::string, int>>{
              {"box_s" + std::to_string(stride), 4 * manifest.dfl_bins},
              {"class_s" + std::to_string(stride), manifest.class_count},
              {"score_sum_s" + std::to_string(stride), 1}}
        : std::vector<std::pair<std::string, int>>{
              {"box_s" + std::to_string(stride), 4 * manifest.dfl_bins},
              {"class_s" + std::to_string(stride), manifest.class_count}};
    for (const auto& role : roles) {
      const cv::FileNode node = outputs[static_cast<int>(output_index)];
      TensorContract tensor;
      tensor.name = read_string(node, "name", "outputs");
      tensor.role = read_string(node, "role", "outputs");
      tensor.shape = read_shape(node, "outputs");
      tensor.dtype = read_string(node, "dtype", "outputs");
      tensor.layout = read_string(node, "layout", "outputs");
      if (tensor.role != role.first ||
          tensor.shape !=
              std::vector<std::int64_t>({1, role.second, grid, grid}) ||
          tensor.dtype != "float32" || tensor.layout != "NCHW") {
        throw std::runtime_error("detector manifest output contract mismatch at " +
                                 role.first);
      }
      manifest.outputs.push_back(std::move(tensor));
      ++output_index;
    }
  }
  return manifest;
}

std::string tensor_description(const rknn_tensor_attr& attr) {
  std::ostringstream stream;
  stream << attr.name << ',';
  for (std::uint32_t index = 0; index < attr.n_dims; ++index) {
    if (index) stream << 'x';
    stream << attr.dims[index];
  }
  stream << ',' << get_type_string(attr.type) << ','
         << get_format_string(attr.fmt) << ",bytes=" << attr.size;
  return stream.str();
}

void print_tensor(const char* direction, const rknn_tensor_attr& attr) {
  std::cout << "YOLO_RKNN_TENSOR direction=" << direction
            << " index=" << attr.index
            << " description=" << tensor_description(attr)
            << " qnt=" << get_qnt_type_string(attr.qnt_type)
            << " zp=" << attr.zp << " scale=" << attr.scale << '\n';
}

void validate_actual_shape(const rknn_tensor_attr& attr,
                           const std::vector<std::int64_t>& expected,
                           rknn_tensor_format expected_format,
                           const std::string& role) {
  if (attr.n_dims != expected.size() || attr.fmt != expected_format) {
    throw std::runtime_error("RKNN " + role + " rank/layout mismatch: expected " +
                             get_format_string(expected_format) + ", got " +
                             get_format_string(attr.fmt));
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (attr.dims[index] != static_cast<std::uint32_t>(expected[index])) {
      throw std::runtime_error("RKNN " + role +
                               " shape does not match detector manifest");
    }
  }
}

float milliseconds(std::chrono::steady_clock::duration duration) {
  return std::chrono::duration<float, std::milli>(duration).count();
}

}  // namespace

class YoloRknnEngine::Impl {
 public:
  explicit Impl(const YoloRknnEngineConfig& config) : config_(config) {
    if (config_.model_path.empty() || config_.manifest_path.empty() ||
        config_.labels_path.empty()) {
      throw std::invalid_argument("detector model, manifest, and labels are required");
    }
    if (!finite_open_unit(config_.confidence_threshold) ||
        !finite_open_unit(config_.nms_threshold) || config_.class_id < -1) {
      throw std::invalid_argument("detector thresholds or class filter are invalid");
    }

    manifest_ = load_manifest(config_);
    const std::filesystem::path model_path(config_.model_path);
    const std::filesystem::path labels_path(config_.labels_path);
    if (model_path.filename() != manifest_.model_file ||
        labels_path.filename() != manifest_.labels_file) {
      throw std::runtime_error("detector artifact filenames do not match manifest");
    }
    if (!std::filesystem::is_regular_file(model_path) ||
        std::filesystem::file_size(model_path) != manifest_.model_size) {
      throw std::runtime_error("detector model size does not match manifest");
    }
    model_sha256_ = sha256_file(config_.model_path);
    labels_sha256_ = sha256_file(config_.labels_path);
    manifest_sha256_ = sha256_file(config_.manifest_path);
    if (model_sha256_ != manifest_.model_sha256) {
      throw std::runtime_error("detector model SHA-256 does not match manifest");
    }
    if (labels_sha256_ != manifest_.labels_sha256) {
      throw std::runtime_error("detector labels SHA-256 does not match manifest");
    }
    labels_ = read_labels(config_.labels_path);
    if (labels_.size() != static_cast<std::size_t>(manifest_.class_count)) {
      throw std::runtime_error("detector labels count does not match manifest");
    }
    if (config_.class_id >= manifest_.class_count) {
      throw std::invalid_argument("detector class filter exceeds manifest class count");
    }

    model_bytes_ = read_binary_file(config_.model_path);
    try {
      check_rknn(rknn_init(&context_, model_bytes_.data(),
                           static_cast<std::uint32_t>(model_bytes_.size()), 0,
                           nullptr),
                 "init", config_.model_path);
      query_and_validate_tensors();
    } catch (...) {
      if (context_ != 0) {
        const int status = rknn_destroy(context_);
        if (status != 0) {
          std::cerr << "ERROR: RKNN stage=destroy_after_init_failure status="
                    << status << " model=" << config_.model_path << '\n';
        }
        context_ = 0;
      }
      throw;
    }
  }

  ~Impl() {
    if (context_ != 0) {
      const int status = rknn_destroy(context_);
      if (status != 0) {
        std::cerr << "ERROR: RKNN stage=destroy status=" << status
                  << " model=" << config_.model_path << '\n';
      }
    }
  }

  DetectorEngineResult infer(const ImageView& image,
                             const DetectorRequest& request) {
    if (image.data == nullptr || image.width <= 0 || image.height <= 0 ||
        image.channels != 3 ||
        image.stride_bytes < static_cast<std::size_t>(image.width * 3)) {
      throw std::invalid_argument("YOLO engine input must be a valid BGR8 image");
    }
    validate_bbox(request.roi, image.width, image.height);
    if (request.max_candidates == 0) {
      throw std::invalid_argument("YOLO request max_candidates must be positive");
    }

    const auto total_start = std::chrono::steady_clock::now();
    const int left = std::max(0, static_cast<int>(std::floor(request.roi.x)));
    const int top = std::max(0, static_cast<int>(std::floor(request.roi.y)));
    const int right = std::min(
        image.width,
        static_cast<int>(std::ceil(request.roi.x + request.roi.width)));
    const int bottom = std::min(
        image.height,
        static_cast<int>(std::ceil(request.roi.y + request.roi.height)));
    if (right <= left || bottom <= top) {
      throw std::invalid_argument("YOLO request ROI has zero integer area");
    }
    const cv::Mat frame(image.height, image.width, CV_8UC3,
                        const_cast<std::uint8_t*>(image.data),
                        image.stride_bytes);
    const cv::Rect roi_rect(left, top, right - left, bottom - top);
    const cv::Mat crop = frame(roi_rect);
    const float scale = std::min(
        static_cast<float>(kInputSize) / static_cast<float>(crop.cols),
        static_cast<float>(kInputSize) / static_cast<float>(crop.rows));
    const int resized_width = std::clamp(
        static_cast<int>(std::lround(crop.cols * scale)), 1, kInputSize);
    const int resized_height = std::clamp(
        static_cast<int>(std::lround(crop.rows * scale)), 1, kInputSize);
    const int pad_left = (kInputSize - resized_width) / 2;
    const int pad_top = (kInputSize - resized_height) / 2;
    cv::Mat resized_bgr;
    cv::resize(crop, resized_bgr, cv::Size(resized_width, resized_height), 0.0,
               0.0, cv::INTER_LINEAR);
    cv::Mat resized_rgb;
    cv::cvtColor(resized_bgr, resized_rgb, cv::COLOR_BGR2RGB);
    cv::Mat input(kInputSize, kInputSize, CV_8UC3,
                  cv::Scalar(kPadValue, kPadValue, kPadValue));
    resized_rgb.copyTo(
        input(cv::Rect(pad_left, pad_top, resized_width, resized_height)));
    if (!input.isContinuous()) input = input.clone();
    const auto preprocess_end = std::chrono::steady_clock::now();

    rknn_input native_input{};
    native_input.index = 0;
    native_input.buf = input.data;
    native_input.size = static_cast<std::uint32_t>(input.total() * input.elemSize());
    native_input.pass_through = 0;
    native_input.type = RKNN_TENSOR_UINT8;
    native_input.fmt = RKNN_TENSOR_NHWC;
    check_rknn(rknn_inputs_set(context_, 1, &native_input), "inputs_set",
               config_.model_path);
    check_rknn(rknn_run(context_, nullptr), "run", config_.model_path);

    std::vector<rknn_output> native_outputs(output_attrs_.size());
    for (std::size_t index = 0; index < native_outputs.size(); ++index) {
      native_outputs[index] = {};
      native_outputs[index].index = static_cast<std::uint32_t>(index);
      native_outputs[index].want_float = 1;
      native_outputs[index].is_prealloc = 0;
    }
    check_rknn(rknn_outputs_get(context_, native_outputs.size(),
                                native_outputs.data(), nullptr),
               "outputs_get", config_.model_path);
    ScopedCleanup output_cleanup(
        [&] {
          return rknn_outputs_release(
              context_, static_cast<std::uint32_t>(native_outputs.size()),
              native_outputs.data());
        },
        [&](int status) {
          if (status != 0) {
            std::cerr << "ERROR: RKNN stage=outputs_release_fallback status="
                      << status << " model=" << config_.model_path << '\n';
          }
        });
    const auto inference_end = std::chrono::steady_clock::now();

    DetectorEngineResult result;
    try {
      std::vector<YoloV8ScaleTensors> scales;
      const std::size_t per_scale = native_outputs.size() / 3;
      for (std::size_t scale_index = 0; scale_index < 3; ++scale_index) {
        const std::size_t base = scale_index * per_scale;
        YoloV8ScaleTensors scale_tensors;
        scale_tensors.box = output_view(native_outputs[base],
                                        manifest_.outputs[base]);
        scale_tensors.classes = output_view(native_outputs[base + 1],
                                            manifest_.outputs[base + 1]);
        if (per_scale == 3) {
          scale_tensors.score_sum = output_view(
              native_outputs[base + 2], manifest_.outputs[base + 2]);
        }
        scales.push_back(std::move(scale_tensors));
      }
      YoloV8DecodeOptions options;
      options.input_width = kInputSize;
      options.input_height = kInputSize;
      options.confidence_threshold = config_.confidence_threshold;
      options.nms_threshold = config_.nms_threshold;
      options.class_id = config_.class_id;
      options.max_candidates = request.max_candidates;
      LetterboxTransform transform;
      transform.roi = BBox{static_cast<float>(left), static_cast<float>(top),
                            static_cast<float>(right - left),
                            static_cast<float>(bottom - top)};
      transform.frame_width = image.width;
      transform.frame_height = image.height;
      transform.scale = scale;
      transform.pad_x = static_cast<float>(pad_left);
      transform.pad_y = static_cast<float>(pad_top);
      YoloV8DecodeResult decoded =
          decode_yolov8_with_stats(scales, options, transform);
      result.candidates = std::move(decoded.candidates);
      result.raw_candidate_count = decoded.raw_candidate_count;
      result.filtered_candidate_count = decoded.filtered_candidate_count;
      check_rknn(output_cleanup.release(), "outputs_release",
                 config_.model_path);
    } catch (...) {
      const std::exception_ptr original = std::current_exception();
      if (output_cleanup.armed()) {
        const int status = output_cleanup.release();
        if (status != 0) {
          std::cerr << "ERROR: RKNN stage=outputs_release_after_inference_failure status="
                    << status << " model=" << config_.model_path << '\n';
        }
      }
      std::rethrow_exception(original);
    }
    const auto postprocess_end = std::chrono::steady_clock::now();
    result.timing.preprocess_ms = milliseconds(preprocess_end - total_start);
    result.timing.inference_ms = milliseconds(inference_end - preprocess_end);
    result.timing.postprocess_ms = milliseconds(postprocess_end - inference_end);
    result.timing.total_ms = milliseconds(postprocess_end - total_start);
    return result;
  }

  const std::string& model_sha256() const { return model_sha256_; }
  const std::string& manifest_sha256() const { return manifest_sha256_; }
  const std::string& labels_sha256() const { return labels_sha256_; }
  const std::string& api_version() const { return api_version_; }
  const std::string& driver_version() const { return driver_version_; }
  const std::string& input_tensor_description() const {
    return input_tensor_description_;
  }
  const std::vector<std::string>& output_tensor_descriptions() const {
    return output_tensor_descriptions_;
  }
  const std::vector<std::string>& labels() const { return labels_; }

 private:
  void query_and_validate_tensors() {
    rknn_sdk_version version{};
    check_rknn(rknn_query(context_, RKNN_QUERY_SDK_VERSION, &version,
                          sizeof(version)),
               "query_sdk_version", config_.model_path);
    api_version_ = version.api_version;
    driver_version_ = version.drv_version;

    rknn_input_output_num counts{};
    check_rknn(rknn_query(context_, RKNN_QUERY_IN_OUT_NUM, &counts,
                          sizeof(counts)),
               "query_input_output_count", config_.model_path);
    if (counts.n_input != 1 || counts.n_output != manifest_.outputs.size()) {
      throw std::runtime_error("RKNN input/output count does not match manifest");
    }
    input_attrs_.resize(counts.n_input);
    output_attrs_.resize(counts.n_output);
    for (std::uint32_t index = 0; index < counts.n_input; ++index) {
      input_attrs_[index] = {};
      input_attrs_[index].index = index;
      check_rknn(rknn_query(context_, RKNN_QUERY_INPUT_ATTR,
                            &input_attrs_[index], sizeof(rknn_tensor_attr)),
                 "query_input_attr[" + std::to_string(index) + "]",
                 config_.model_path);
      print_tensor("input", input_attrs_[index]);
    }
    for (std::uint32_t index = 0; index < counts.n_output; ++index) {
      output_attrs_[index] = {};
      output_attrs_[index].index = index;
      check_rknn(rknn_query(context_, RKNN_QUERY_OUTPUT_ATTR,
                            &output_attrs_[index], sizeof(rknn_tensor_attr)),
                 "query_output_attr[" + std::to_string(index) + "]",
                 config_.model_path);
      print_tensor("output", output_attrs_[index]);
    }

    const rknn_tensor_attr& input = input_attrs_.front();
    validate_actual_shape(input, {1, kInputSize, kInputSize, 3},
                          RKNN_TENSOR_NHWC, "input");
    if ((input.type != RKNN_TENSOR_FLOAT32 &&
         input.type != RKNN_TENSOR_FLOAT16)) {
      throw std::runtime_error("RKNN input type is not the expected FP model input");
    }
    input_tensor_description_ = tensor_description(input);

    for (std::size_t index = 0; index < output_attrs_.size(); ++index) {
      const rknn_tensor_attr& output = output_attrs_[index];
      validate_actual_shape(output, manifest_.outputs[index].shape,
                            RKNN_TENSOR_NCHW,
                            manifest_.outputs[index].role);
      if ((output.type != RKNN_TENSOR_FLOAT32 &&
           output.type != RKNN_TENSOR_FLOAT16)) {
        throw std::runtime_error("RKNN output type does not match FP manifest");
      }
      output_tensor_descriptions_.push_back(tensor_description(output));
    }
    std::cout << "YOLO_RKNN_MODEL path=" << config_.model_path
              << " manifest=" << config_.manifest_path
              << " labels=" << config_.labels_path
              << " api=" << api_version_ << " driver=" << driver_version_
              << " target=rk3566 outputs=" << output_attrs_.size() << '\n';
  }

  YoloTensorView output_view(const rknn_output& output,
                             const TensorContract& contract) const {
    std::size_t elements = 1;
    for (const std::int64_t dimension : contract.shape) {
      elements *= static_cast<std::size_t>(dimension);
    }
    const std::size_t expected_bytes = elements * sizeof(float);
    if (output.buf == nullptr || output.size != expected_bytes) {
      throw std::runtime_error("RKNN float output size mismatch for " +
                               contract.role);
    }
    return YoloTensorView{YoloTensorType::kFloat32,
                          YoloTensorLayout::kNchw,
                          output.buf,
                          output.size,
                          contract.shape,
                          1.0F,
                          0};
  }

  YoloRknnEngineConfig config_;
  DetectorManifest manifest_;
  std::vector<std::string> labels_;
  std::vector<std::uint8_t> model_bytes_;
  rknn_context context_ = 0;
  std::vector<rknn_tensor_attr> input_attrs_;
  std::vector<rknn_tensor_attr> output_attrs_;
  std::string model_sha256_;
  std::string manifest_sha256_;
  std::string labels_sha256_;
  std::string api_version_;
  std::string driver_version_;
  std::string input_tensor_description_;
  std::vector<std::string> output_tensor_descriptions_;
};

YoloRknnEngine::YoloRknnEngine(const YoloRknnEngineConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

YoloRknnEngine::~YoloRknnEngine() = default;

DetectorEngineResult YoloRknnEngine::infer(const ImageView& image,
                                           const DetectorRequest& request) {
  return impl_->infer(image, request);
}

const std::string& YoloRknnEngine::model_sha256() const {
  return impl_->model_sha256();
}
const std::string& YoloRknnEngine::manifest_sha256() const {
  return impl_->manifest_sha256();
}
const std::string& YoloRknnEngine::labels_sha256() const {
  return impl_->labels_sha256();
}
const std::string& YoloRknnEngine::api_version() const {
  return impl_->api_version();
}
const std::string& YoloRknnEngine::driver_version() const {
  return impl_->driver_version();
}
const std::string& YoloRknnEngine::input_tensor_description() const {
  return impl_->input_tensor_description();
}
const std::vector<std::string>&
YoloRknnEngine::output_tensor_descriptions() const {
  return impl_->output_tensor_descriptions();
}
const std::vector<std::string>& YoloRknnEngine::labels() const {
  return impl_->labels();
}

}  // namespace nanotrack
