#include "production_deployment_config.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace nanotrack {
namespace {

struct Field {
  const char* key;
  std::string ProductionDeploymentConfig::*value;
};

constexpr Field fields[] = {
    {"tracker_model_dir", &ProductionDeploymentConfig::tracker_model_dir},
    {"controller_config_path", &ProductionDeploymentConfig::controller_config_path},
    {"detector_model_path", &ProductionDeploymentConfig::detector_model_path},
    {"detector_manifest_path", &ProductionDeploymentConfig::detector_manifest_path},
    {"detector_labels_path", &ProductionDeploymentConfig::detector_labels_path}};

std::string trim(std::string text) {
  const auto not_space = [](unsigned char value) { return std::isspace(value) == 0; };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
  text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
  return text;
}

void require_file(const std::filesystem::path& path, const std::string& key) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error)) {
    throw std::invalid_argument("production deployment " + key +
                                " must be a regular file: " + path.string());
  }
}

}  // namespace

void validate_production_deployment_config(const ProductionDeploymentConfig& config) {
  for (const auto& field : fields) {
    if (trim(config.*(field.value)).empty()) {
      throw std::invalid_argument(std::string("empty production deployment key: ") + field.key);
    }
  }
  const std::filesystem::path tracker_dir(config.tracker_model_dir);
  for (const char* model : {"template_backbone.rknn", "search_backbone.rknn", "head.rknn"}) {
    require_file(tracker_dir / model, "tracker_model_dir");
  }
  for (const auto& field : fields) {
    if (field.value != &ProductionDeploymentConfig::tracker_model_dir) {
      require_file(config.*(field.value), field.key);
    }
  }
}

ProductionDeploymentConfig load_production_deployment_config(const std::string& path) {
  require_file(path, "config");
  std::ifstream stream(path);
  if (!stream) throw std::invalid_argument("cannot open production deployment config: " + path);

  ProductionDeploymentConfig config;
  std::unordered_set<std::string> seen;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(stream, line)) {
    ++line_number;
    line = trim(line.substr(0, line.find('#')));
    if (line.empty()) continue;
    const auto separator = line.find('=');
    if (separator == std::string::npos) {
      throw std::invalid_argument("expected key=value in production deployment config at line " +
                                  std::to_string(line_number));
    }
    const std::string key = trim(line.substr(0, separator));
    const std::string value = trim(line.substr(separator + 1));
    const auto field = std::find_if(std::begin(fields), std::end(fields),
                                    [&](const Field& item) { return key == item.key; });
    if (field == std::end(fields)) {
      throw std::invalid_argument("unknown production deployment key: " + key);
    }
    if (!seen.insert(key).second) {
      throw std::invalid_argument("duplicate production deployment key: " + key);
    }
    if (value.empty()) throw std::invalid_argument("empty production deployment key: " + key);
    config.*(field->value) = value;
  }
  if (stream.bad()) throw std::invalid_argument("cannot read production deployment config: " + path);
  for (const auto& field : fields) {
    if (seen.count(field.key) == 0) {
      throw std::invalid_argument(std::string("missing production deployment key: ") + field.key);
    }
  }
  validate_production_deployment_config(config);
  return config;
}

}  // namespace nanotrack
