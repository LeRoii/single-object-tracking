#pragma once

#include "cli.hpp"
#include "long_term_runtime.hpp"

#include <stdexcept>

namespace nanotrack {

// A null list denotes an unconfigured detector. A non-null list must come from
// the engine after its manifest, labels identity and class count were verified.
inline TargetClassRuntimeConfig make_target_class_runtime_config(
    const Options& options,
    const std::vector<std::string>* verified_labels = nullptr) {
  if (options.detector_class_id < -1) {
    throw std::invalid_argument("detector target class must be -1 or nonnegative");
  }
  if (verified_labels && options.detector_class_id >= 0 &&
      static_cast<std::size_t>(options.detector_class_id) >= verified_labels->size()) {
    throw std::invalid_argument("fixed detector target class is outside verified labels");
  }
  TargetClassRuntimeConfig config;
  config.requested_class_id = options.detector_class_id;
  config.class_count = verified_labels ? verified_labels->size() : 0;
  config.match_config = {options.target_class_min_roi_coverage,
                        options.target_class_max_center_ratio,
                        options.target_class_ambiguity_margin};
  return config;
}

}  // namespace nanotrack
