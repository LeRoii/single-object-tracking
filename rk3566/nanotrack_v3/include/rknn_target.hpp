#pragma once

#include <string>

namespace nanotrack {

inline bool is_supported_rknn_target(const std::string& target) {
  return target == "rk3566" || target == "rk3588";
}

}  // namespace nanotrack
