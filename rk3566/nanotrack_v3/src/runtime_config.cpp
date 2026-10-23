#include "runtime_config.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace nanotrack {
namespace {

std::string trim(std::string text) {
  const auto not_space = [](unsigned char value) {
    return std::isspace(value) == 0;
  };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
  text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(),
             text.end());
  return text;
}

bool parse_bool(const std::string& key, const std::string& text) {
  if (text == "true") return true;
  if (text == "false") return false;
  throw std::invalid_argument("runtime config key " + key +
                              " must be true or false");
}

const std::unordered_map<std::string, std::string>& option_names() {
  static const std::unordered_map<std::string, std::string> names{
      {"video", "--video"},
      {"bbox", "--bbox"},
      {"model_dir", "--model-dir"},
      {"output_video", "--output-video"},
      {"output_boxes", "--output-boxes"},
      {"max_frames", "--max-frames"},
      {"start_frame", "--start-frame"},
      {"end_frame", "--end-frame"},
      {"output_size", "--output-size"},
      {"controller_config", "--controller-config"},
      {"sensor", "--sensor"},
      {"session_metadata", "--session-metadata"},
      {"detector_model", "--detector-model"},
      {"detector_manifest", "--detector-manifest"},
      {"detector_labels", "--detector-labels"},
      {"detector_class_id", "--detector-class-id"},
      {"detector_conf", "--detector-conf"},
      {"detector_nms", "--detector-nms"},
  };
  return names;
}

bool is_path_key(const std::string& key) {
  static const std::unordered_set<std::string> keys{
      "video",          "model_dir",        "output_video",
      "output_boxes",   "controller_config", "session_metadata",
      "detector_model", "detector_manifest", "detector_labels",
  };
  return keys.find(key) != keys.end();
}

std::string resolve_path(const std::filesystem::path& directory,
                         const std::string& text) {
  const std::filesystem::path path(text);
  return (path.is_absolute() ? path : directory / path)
      .lexically_normal()
      .string();
}

}  // namespace

RuntimeConfig load_runtime_config(const std::string& path) {
  namespace fs = std::filesystem;
  const fs::path config_path = fs::absolute(fs::path(path)).lexically_normal();
  std::ifstream stream(config_path);
  if (!stream) {
    throw std::invalid_argument("cannot open runtime config: " +
                                config_path.string());
  }

  RuntimeConfig config;
  std::unordered_set<std::string> seen;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(stream, line)) {
    ++line_number;
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) line.erase(comment);
    line = trim(line);
    if (line.empty()) continue;

    const std::size_t separator = line.find('=');
    if (separator == std::string::npos ||
        line.find('=', separator + 1) != std::string::npos) {
      throw std::invalid_argument("malformed runtime config line " +
                                  std::to_string(line_number));
    }
    const std::string key = trim(line.substr(0, separator));
    const std::string value = trim(line.substr(separator + 1));
    if (key.empty() || value.empty()) {
      throw std::invalid_argument("empty runtime config key or value");
    }
    if (!seen.insert(key).second) {
      throw std::invalid_argument("duplicate runtime config key: " + key);
    }

    if (key == "interactive") {
      config.has_interactive = true;
      config.interactive = parse_bool(key, value);
      continue;
    }

    const auto option = option_names().find(key);
    if (option == option_names().end()) {
      throw std::invalid_argument("unknown runtime config key: " + key);
    }
    config.option_values.emplace(
        option->second,
        is_path_key(key) ? resolve_path(config_path.parent_path(), value)
                         : value);
  }
  return config;
}

}  // namespace nanotrack
