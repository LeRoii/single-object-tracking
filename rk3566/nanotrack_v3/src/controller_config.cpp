#include "controller_config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace nanotrack {
namespace {

std::string trim(std::string text) {
  const auto not_space = [](unsigned char value) { return std::isspace(value) == 0; };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
  text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
  return text;
}

float parse_float(const std::string& key, const std::string& text) {
  std::size_t consumed = 0;
  const float value = std::stof(text, &consumed);
  if (consumed != text.size() || !std::isfinite(value)) {
    throw std::invalid_argument("config key " + key + " must be a finite number");
  }
  return value;
}

std::int64_t parse_integer(const std::string& key, const std::string& text) {
  std::size_t consumed = 0;
  const long long value = std::stoll(text, &consumed);
  if (consumed != text.size()) {
    throw std::invalid_argument("config key " + key + " must be an integer");
  }
  return static_cast<std::int64_t>(value);
}

int parse_int(const std::string& key, const std::string& text) {
  const std::int64_t value = parse_integer(key, text);
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    throw std::invalid_argument("config key " + key + " is out of range");
  }
  return static_cast<int>(value);
}

bool parse_bool(const std::string& key, const std::string& text) {
  if (text == "true") return true;
  if (text == "false") return false;
  throw std::invalid_argument("config key " + key + " must be true or false");
}

DetectorTargetMode parse_detector_target_mode(const std::string& text) {
  if (text == "full_object") return DetectorTargetMode::kFullObject;
  if (text == "subtarget") return DetectorTargetMode::kSubtarget;
  throw std::invalid_argument(
      "detector_target_mode must be full_object or subtarget");
}

void set_value(ControllerConfig& config,
               const std::string& key,
               const std::string& value) {
  if (key == "sensor") config.sensor = value;
  else if (key == "calibrated") config.calibrated = parse_bool(key, value);
  else if (key == "reliable_score") config.reliable_score = parse_float(key, value);
  else if (key == "reliable_psr") config.reliable_psr = parse_float(key, value);
  else if (key == "reliable_apce") config.reliable_apce = parse_float(key, value);
  else if (key == "reliable_peak_margin") config.reliable_peak_margin = parse_float(key, value);
  else if (key == "weak_score") config.weak_score = parse_float(key, value);
  else if (key == "weak_psr") config.weak_psr = parse_float(key, value);
  else if (key == "weak_apce") config.weak_apce = parse_float(key, value);
  else if (key == "weak_peak_margin") config.weak_peak_margin = parse_float(key, value);
  else if (key == "template_agreement_iou") config.template_agreement_iou = parse_float(key, value);
  else if (key == "max_center_distance") config.max_center_distance = parse_float(key, value);
  else if (key == "max_area_log_change") config.max_area_log_change = parse_float(key, value);
  else if (key == "max_aspect_log_change") config.max_aspect_log_change = parse_float(key, value);
  else if (key == "uncertain_ms") config.uncertain_ms = parse_integer(key, value);
  else if (key == "local_recovery_ms") config.local_recovery_ms = parse_integer(key, value);
  else if (key == "assisted_recovery_ms") config.assisted_recovery_ms = parse_integer(key, value);
  else if (key == "occlusion_hold_frames") config.occlusion_hold_frames = parse_int(key, value);
  else if (key == "occlusion_trigger_consecutive_unreliable") config.occlusion_trigger_consecutive_unreliable = parse_int(key, value);
  else if (key == "occlusion_trigger_min_dimension_ratio") config.occlusion_trigger_min_dimension_ratio = parse_float(key, value);
  else if (key == "occlusion_reference_window") config.occlusion_reference_window = parse_int(key, value);
  else if (key == "detector_interval_ms") config.detector_interval_ms = parse_integer(key, value);
  else if (key == "detector_stale_ms") config.detector_stale_ms = parse_integer(key, value);
  else if (key == "detector_max_candidates") config.detector_max_candidates = static_cast<std::size_t>(parse_integer(key, value));
  else if (key == "detector_candidate_max_center_ratio") config.detector_candidate_max_center_ratio = parse_float(key, value);
  else if (key == "detector_target_mode") config.detector_target_mode = parse_detector_target_mode(value);
  else if (key == "detector_search_scale") config.detector_search_scale = parse_float(key, value);
  else if (key == "detector_subtarget_max_probes") config.detector_subtarget_max_probes = static_cast<std::size_t>(parse_integer(key, value));
  else if (key == "detector_confirmation_iou") config.detector_confirmation_iou = parse_float(key, value);
  else if (key == "detector_confirmation_center_distance") config.detector_confirmation_center_distance = parse_float(key, value);
  else if (key == "detector_confirmation_max_area_log_change") config.detector_confirmation_max_area_log_change = parse_float(key, value);
  else if (key == "detector_confirmation_max_aspect_log_change") config.detector_confirmation_max_aspect_log_change = parse_float(key, value);
  else if (key == "stable_frames_for_update") config.stable_frames_for_update = parse_int(key, value);
  else if (key == "recent_update_interval_ms") config.recent_update_interval_ms = parse_integer(key, value);
  else if (key == "recent_probation_frames") config.recent_probation_frames = parse_int(key, value);
  else if (key == "confirmation_window") config.confirmation_window = parse_int(key, value);
  else if (key == "confirmation_required") config.confirmation_required = parse_int(key, value);
  else if (key == "search_tracking") config.search_tracking = parse_float(key, value);
  else if (key == "search_uncertain") config.search_uncertain = parse_float(key, value);
  else if (key == "search_local_initial") config.search_local_initial = parse_float(key, value);
  else if (key == "search_local_late") config.search_local_late = parse_float(key, value);
  else if (key == "search_assisted") config.search_assisted = parse_float(key, value);
  else if (key == "search_lost") config.search_lost = parse_float(key, value);
  else if (key == "motion_alpha") config.motion_alpha = parse_float(key, value);
  else if (key == "motion_beta") config.motion_beta = parse_float(key, value);
  else if (key == "motion_max_dt_ms") config.motion_max_dt_ms = parse_integer(key, value);
  else throw std::invalid_argument("unknown controller config key: " + key);
}

bool in_unit_interval(float value) { return value >= 0.0F && value <= 1.0F; }

}  // namespace

ControllerConfig default_controller_config(const std::string& sensor) {
  ControllerConfig config;
  config.sensor = sensor;
  if (sensor == "visible") {
    config.reliable_psr = 6.0F;
    config.reliable_peak_margin = 0.06F;
  }
  validate_controller_config(config);
  return config;
}

void validate_controller_config(const ControllerConfig& config) {
  if (config.sensor != "visible" && config.sensor != "infrared") {
    throw std::invalid_argument("sensor must be visible or infrared");
  }
  const float probabilities[] = {
      config.reliable_score, config.weak_score, config.template_agreement_iou,
      config.detector_confirmation_iou,
      config.detector_candidate_max_center_ratio};
  for (const float value : probabilities) {
    if (!std::isfinite(value) || !in_unit_interval(value)) {
      throw std::invalid_argument("probability-like controller values must be in [0,1]");
    }
  }
  const float nonnegative[] = {
      config.reliable_psr, config.reliable_apce, config.reliable_peak_margin,
      config.weak_psr, config.weak_apce, config.weak_peak_margin,
      config.max_center_distance, config.max_area_log_change,
      config.max_aspect_log_change,
      config.detector_confirmation_center_distance,
      config.detector_confirmation_max_area_log_change,
      config.detector_confirmation_max_aspect_log_change};
  for (const float value : nonnegative) {
    if (!std::isfinite(value) || value < 0.0F) {
      throw std::invalid_argument("controller thresholds must be finite and non-negative");
    }
  }
  if (config.reliable_score < config.weak_score ||
      config.reliable_psr < config.weak_psr ||
      config.reliable_apce < config.weak_apce ||
      config.reliable_peak_margin < config.weak_peak_margin) {
    throw std::invalid_argument("reliable thresholds must not be weaker than weak thresholds");
  }
  if (config.uncertain_ms <= 0 ||
      config.local_recovery_ms <= config.uncertain_ms ||
      config.assisted_recovery_ms <= config.local_recovery_ms) {
    throw std::invalid_argument("controller state timing must be strictly increasing");
  }
  if (config.detector_interval_ms <= 0 || config.detector_stale_ms < 0 ||
      config.detector_max_candidates == 0 || config.detector_max_candidates > 3) {
    throw std::invalid_argument("detector limits are invalid");
  }
  if (config.occlusion_hold_frames < 1 || config.occlusion_hold_frames > 300 ||
      config.occlusion_trigger_consecutive_unreliable < 1 ||
      config.occlusion_trigger_consecutive_unreliable > 10 ||
      config.occlusion_reference_window < 3 ||
      config.occlusion_reference_window > 60) {
    throw std::invalid_argument("occlusion hold counts are invalid");
  }
  if (!std::isfinite(config.occlusion_trigger_min_dimension_ratio) ||
      config.occlusion_trigger_min_dimension_ratio <= 0.0F ||
      config.occlusion_trigger_min_dimension_ratio > 1.0F) {
    throw std::invalid_argument("occlusion dimension ratio must be in (0,1]");
  }
  if (!std::isfinite(config.detector_search_scale) ||
      config.detector_search_scale < 1.0F ||
      config.detector_subtarget_max_probes == 0 ||
      config.detector_subtarget_max_probes > 9) {
    throw std::invalid_argument("detector recovery search limits are invalid");
  }
  if (config.stable_frames_for_update <= 0 ||
      config.recent_update_interval_ms <= 0 ||
      config.recent_probation_frames <= 0 || config.confirmation_window <= 0 ||
      config.confirmation_required <= 0 ||
      config.confirmation_required > config.confirmation_window) {
    throw std::invalid_argument("template or confirmation counts are invalid");
  }
  if (config.confirmation_window != 3 || config.confirmation_required != 2) {
    throw std::invalid_argument("recovery confirmation policy must be two of three");
  }
  const float search_scales[] = {
      config.search_tracking, config.search_uncertain, config.search_local_initial,
      config.search_local_late, config.search_assisted, config.search_lost};
  for (const float value : search_scales) {
    if (!std::isfinite(value) || value < 1.0F) {
      throw std::invalid_argument("search scales must be finite and at least one");
    }
  }
  if (!in_unit_interval(config.motion_alpha) ||
      !in_unit_interval(config.motion_beta) || config.motion_max_dt_ms <= 0) {
    throw std::invalid_argument("motion filter parameters are invalid");
  }
}

ControllerConfig load_controller_config(const std::string& path) {
  std::ifstream stream(path);
  if (!stream) throw std::invalid_argument("cannot open controller config: " + path);
  ControllerConfig config;
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
    if (separator == std::string::npos || line.find('=', separator + 1) != std::string::npos) {
      throw std::invalid_argument("malformed controller config line " +
                                  std::to_string(line_number));
    }
    const std::string key = trim(line.substr(0, separator));
    const std::string value = trim(line.substr(separator + 1));
    if (key.empty() || value.empty()) {
      throw std::invalid_argument("empty controller config key or value");
    }
    if (!seen.insert(key).second) {
      throw std::invalid_argument("duplicate controller config key: " + key);
    }
    set_value(config, key, value);
  }
  validate_controller_config(config);
  return config;
}

}  // namespace nanotrack
