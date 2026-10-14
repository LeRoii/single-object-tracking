#include "cli.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace nanotrack {
namespace {

std::size_t parse_positive_size(const std::string& text) {
  std::size_t consumed = 0;
  const unsigned long long value = std::stoull(text, &consumed);
  if (consumed != text.size() || value == 0 ||
      value > std::numeric_limits<std::size_t>::max()) {
    throw std::invalid_argument("--max-frames must be a positive integer");
  }
  return static_cast<std::size_t>(value);
}

std::size_t parse_frame_index(const std::string& text, const char* option) {
  if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
      })) {
    throw std::invalid_argument(std::string(option) + " must be a non-negative integer");
  }
  std::size_t consumed = 0;
  const unsigned long long value = std::stoull(text, &consumed);
  if (consumed != text.size() || value > std::numeric_limits<std::size_t>::max()) {
    throw std::invalid_argument(std::string(option) + " is out of range");
  }
  return static_cast<std::size_t>(value);
}

std::pair<int, int> parse_output_size(const std::string& text) {
  const std::size_t separator = text.find('x');
  if (separator == std::string::npos || separator == 0 ||
      separator + 1 >= text.size() || text.find('x', separator + 1) != std::string::npos) {
    throw std::invalid_argument("--output-size must use WIDTHxHEIGHT");
  }
  const std::size_t width = parse_frame_index(text.substr(0, separator), "output width");
  const std::size_t height = parse_frame_index(text.substr(separator + 1), "output height");
  if (width == 0 || height == 0 || width > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
      height > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument("--output-size dimensions must be positive integers");
  }
  return {static_cast<int>(width), static_cast<int>(height)};
}

int parse_class_id(const std::string& text) {
  std::size_t consumed = 0;
  const long long value = std::stoll(text, &consumed);
  if (consumed != text.size() || value < -1 ||
      value > std::numeric_limits<int>::max()) {
    throw std::invalid_argument("--detector-class-id must be -1 or a non-negative integer");
  }
  return static_cast<int>(value);
}

float parse_open_unit(const std::string& text, const char* option) {
  std::size_t consumed = 0;
  const float value = std::stof(text, &consumed);
  if (consumed != text.size() || !std::isfinite(value) || value <= 0.0F ||
      value >= 1.0F) {
    throw std::invalid_argument(std::string(option) +
                                " must be finite and in the open interval (0,1)");
  }
  return value;
}

}  // namespace

std::string usage() {
  return
      "Usage: nanotrack_v3 --video VIDEO [--bbox X,Y,W,H] --model-dir DIR "
      "--output-video OUTPUT.avi --output-boxes OUTPUT.csv [--max-frames N] "
      "[--start-frame N --end-frame N] [--output-size WIDTHxHEIGHT] "
      "[--interactive] "
      "[--sensor visible|infrared] [--controller-config FILE] "
      "[--session-metadata FILE] "
      "[--detector-model MODEL.rknn --detector-manifest MANIFEST.json "
      "--detector-labels LABELS.txt] [--detector-class-id ID] "
      "[--detector-conf FLOAT] [--detector-nms FLOAT]\n";
}

Options parse_options(const std::vector<std::string>& arguments) {
  Options options;
  if (arguments.size() == 2 && arguments[1] == "--help") {
    options.help = true;
    return options;
  }
  std::unordered_map<std::string, std::string> values;
  bool interactive_seen = false;
  for (std::size_t index = 1; index < arguments.size();) {
    const std::string& option = arguments[index];
    if (option == "--help") {
      throw std::invalid_argument("--help must be used alone");
    }
    if (option == "--interactive") {
      if (interactive_seen) {
        throw std::invalid_argument("duplicate option: --interactive");
      }
      interactive_seen = true;
      options.interactive = true;
      ++index;
      continue;
    }
    if (option != "--video" && option != "--bbox" && option != "--model-dir" &&
        option != "--output-video" && option != "--output-boxes" &&
        option != "--max-frames" && option != "--start-frame" &&
        option != "--end-frame" && option != "--output-size" &&
        option != "--controller-config" && option != "--sensor" &&
        option != "--session-metadata" && option != "--detector-model" &&
        option != "--detector-manifest" && option != "--detector-labels" &&
        option != "--detector-class-id" && option != "--detector-conf" &&
        option != "--detector-nms") {
      throw std::invalid_argument("unknown option: " + option);
    }
    if (index + 1 >= arguments.size()) {
      throw std::invalid_argument("missing value for option: " + option);
    }
    if (!values.emplace(option, arguments[index + 1]).second) {
      throw std::invalid_argument("duplicate option: " + option);
    }
    index += 2;
  }

  const char* required[] = {"--video", "--model-dir", "--output-video",
                            "--output-boxes"};
  for (const char* option : required) {
    if (values.find(option) == values.end()) {
      throw std::invalid_argument(std::string("missing required option: ") + option);
    }
  }
  if (!options.interactive && values.find("--bbox") == values.end()) {
    throw std::invalid_argument("missing required option: --bbox");
  }
  options.video = values.at("--video");
  const auto bbox = values.find("--bbox");
  if (bbox != values.end()) {
    options.bbox = parse_bbox(bbox->second);
    options.has_bbox = true;
  }
  options.model_dir = values.at("--model-dir");
  options.output_video = values.at("--output-video");
  options.output_boxes = values.at("--output-boxes");
  const auto max_frames = values.find("--max-frames");
  if (max_frames != values.end()) {
    options.max_frames = parse_positive_size(max_frames->second);
  }
  const auto start_frame = values.find("--start-frame");
  if (start_frame != values.end()) {
    options.start_frame = parse_frame_index(start_frame->second, "--start-frame");
  }
  const auto end_frame = values.find("--end-frame");
  if (end_frame != values.end()) {
    options.end_frame = parse_frame_index(end_frame->second, "--end-frame");
  }
  if (options.end_frame < options.start_frame) {
    throw std::invalid_argument("--end-frame must be greater than or equal to --start-frame");
  }
  const auto output_size = values.find("--output-size");
  if (output_size != values.end()) {
    const auto dimensions = parse_output_size(output_size->second);
    options.output_width = dimensions.first;
    options.output_height = dimensions.second;
  }
  const auto controller_config = values.find("--controller-config");
  if (controller_config != values.end()) {
    options.controller_config = controller_config->second;
  }
  const auto sensor = values.find("--sensor");
  if (sensor != values.end()) options.sensor = sensor->second;
  if (options.sensor != "visible" && options.sensor != "infrared") {
    throw std::invalid_argument("--sensor must be visible or infrared");
  }
  const auto session_metadata = values.find("--session-metadata");
  if (session_metadata != values.end()) {
    options.session_metadata = session_metadata->second;
  }
  const auto detector_model = values.find("--detector-model");
  const auto detector_manifest = values.find("--detector-manifest");
  const auto detector_labels = values.find("--detector-labels");
  const int detector_file_count =
      static_cast<int>(detector_model != values.end()) +
      static_cast<int>(detector_manifest != values.end()) +
      static_cast<int>(detector_labels != values.end());
  if (detector_file_count != 0 && detector_file_count != 3) {
    throw std::invalid_argument(
        "--detector-model, --detector-manifest, and --detector-labels must be supplied together");
  }
  if (detector_file_count == 3) {
    options.detector_model = detector_model->second;
    options.detector_manifest = detector_manifest->second;
    options.detector_labels = detector_labels->second;
  }
  const auto detector_class_id = values.find("--detector-class-id");
  if (detector_class_id != values.end()) {
    options.detector_class_id = parse_class_id(detector_class_id->second);
  }
  const auto detector_confidence = values.find("--detector-conf");
  if (detector_confidence != values.end()) {
    options.detector_confidence =
        parse_open_unit(detector_confidence->second, "--detector-conf");
  }
  const auto detector_nms = values.find("--detector-nms");
  if (detector_nms != values.end()) {
    options.detector_nms = parse_open_unit(detector_nms->second, "--detector-nms");
  }
  return options;
}

void validate_input_paths(const Options& options) {
  namespace fs = std::filesystem;
  if (!fs::is_regular_file(options.video)) {
    throw std::invalid_argument("video is not a readable regular file: " + options.video);
  }
  if (!fs::is_directory(options.model_dir)) {
    throw std::invalid_argument("model directory is not readable: " + options.model_dir);
  }
  const char* required_models[] = {"template_backbone.rknn", "search_backbone.rknn",
                                   "head.rknn", "manifest.json"};
  for (const char* file : required_models) {
    const fs::path path = fs::path(options.model_dir) / file;
    if (!fs::is_regular_file(path)) {
      throw std::invalid_argument("required model artifact is missing: " + path.string());
    }
  }
  if (!options.controller_config.empty() &&
      !fs::is_regular_file(options.controller_config)) {
    throw std::invalid_argument("controller config is not a readable regular file: " +
                                options.controller_config);
  }
  if (!options.detector_model.empty()) {
    const std::pair<const char*, const std::string*> detector_files[] = {
        {"detector model", &options.detector_model},
        {"detector manifest", &options.detector_manifest},
        {"detector labels", &options.detector_labels},
    };
    for (const auto& file : detector_files) {
      if (!fs::is_regular_file(*file.second)) {
        throw std::invalid_argument(std::string(file.first) +
                                    " is not a readable regular file: " +
                                    *file.second);
      }
    }
  }
}

}  // namespace nanotrack
