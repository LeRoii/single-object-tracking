#pragma once

#include <string>
#include <unordered_map>

namespace nanotrack {

struct RuntimeConfig {
  std::unordered_map<std::string, std::string> option_values;
  bool has_interactive = false;
  bool interactive = false;
};

RuntimeConfig load_runtime_config(const std::string& path);

}  // namespace nanotrack
