#include "cli.hpp"

#include <algorithm>
#include <cctype>
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

}  // namespace

std::string usage() {
  return
      "Usage: nanotrack_v3 --video VIDEO --bbox X,Y,W,H --model-dir DIR "
      "--output-video OUTPUT.avi --output-boxes OUTPUT.csv [--max-frames N] "
      "[--start-frame N --end-frame N] [--output-size WIDTHxHEIGHT] "
      "[--sensor visible|infrared] [--controller-config FILE] "
      "[--session-metadata FILE]\n";
}

Options parse_options(const std::vector<std::string>& arguments) {
  Options options;
  if (arguments.size() == 2 && arguments[1] == "--help") {
    options.help = true;
    return options;
  }
  std::unordered_map<std::string, std::string> values;
  for (std::size_t index = 1; index < arguments.size();) {
    const std::string& option = arguments[index];
    if (option == "--help") {
      throw std::invalid_argument("--help must be used alone");
    }
    if (option != "--video" && option != "--bbox" && option != "--model-dir" &&
        option != "--output-video" && option != "--output-boxes" &&
        option != "--max-frames" && option != "--start-frame" &&
        option != "--end-frame" && option != "--output-size" &&
        option != "--controller-config" && option != "--sensor" &&
        option != "--session-metadata") {
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

  const char* required[] = {"--video", "--bbox", "--model-dir", "--output-video",
                            "--output-boxes"};
  for (const char* option : required) {
    if (values.find(option) == values.end()) {
      throw std::invalid_argument(std::string("missing required option: ") + option);
    }
  }
  options.video = values.at("--video");
  options.bbox = parse_bbox(values.at("--bbox"));
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
}

}  // namespace nanotrack
