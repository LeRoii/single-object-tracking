#pragma once

#include <string>

namespace nanotrack {

struct ProductionDeploymentConfig {
  std::string tracker_model_dir;
  std::string controller_config_path;
  std::string detector_model_path;
  std::string detector_manifest_path;
  std::string detector_labels_path;
};

// Paths are used as written; relative paths resolve against the process cwd.
// Invalid syntax or missing/non-regular artifacts throw std::invalid_argument.
ProductionDeploymentConfig load_production_deployment_config(const std::string& path);
void validate_production_deployment_config(const ProductionDeploymentConfig& config);

}  // namespace nanotrack
