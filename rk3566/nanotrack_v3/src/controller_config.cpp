#include "controller_config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
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
  else if (key == "max_accepted_scale_step_ratio") config.max_accepted_scale_step_ratio = parse_float(key, value);
  else if (key == "accepted_scale_deadband_ratio") config.accepted_scale_deadband_ratio = parse_float(key, value);
  else if (key == "accepted_scale_smoothing_alpha") config.accepted_scale_smoothing_alpha = parse_float(key, value);
  else if (key == "accepted_scale_median_window_frames") config.accepted_scale_median_window_frames = parse_int(key, value);
  else if (key == "accepted_scale_direction_confirm_frames") config.accepted_scale_direction_confirm_frames = parse_int(key, value);
  else if (key == "accepted_scale_output_hysteresis_px") config.accepted_scale_output_hysteresis_px = parse_float(key, value);
  else if (key == "accepted_center_smoothing_alpha") config.accepted_center_smoothing_alpha = parse_float(key, value);
  else if (key == "accepted_center_max_correction_ratio") config.accepted_center_max_correction_ratio = parse_float(key, value);
  else if (key == "max_reacquisition_center_distance") config.max_reacquisition_center_distance = parse_float(key, value);
  else if (key == "uncertain_ms") config.uncertain_ms = parse_integer(key, value);
  else if (key == "local_recovery_ms") config.local_recovery_ms = parse_integer(key, value);
  else if (key == "assisted_recovery_ms") config.assisted_recovery_ms = parse_integer(key, value);
  else if (key == "occlusion_hold_frames") config.occlusion_hold_frames = parse_int(key, value);
  else if (key == "occlusion_trigger_consecutive_unreliable") config.occlusion_trigger_consecutive_unreliable = parse_int(key, value);
  else if (key == "occlusion_trigger_min_dimension_ratio") config.occlusion_trigger_min_dimension_ratio = parse_float(key, value);
  else if (key == "occlusion_reference_window") config.occlusion_reference_window = parse_int(key, value);
  else if (key == "occlusion_probe_interval_frames") config.occlusion_probe_interval_frames = parse_int(key, value);
  else if (key == "occlusion_gradual_dimension_ratio") config.occlusion_gradual_dimension_ratio = parse_float(key, value);
  else if (key == "occlusion_gradual_consecutive_frames") config.occlusion_gradual_consecutive_frames = parse_int(key, value);
  else if (key == "occlusion_min_reference_samples") config.occlusion_min_reference_samples = parse_int(key, value);
  else if (key == "occlusion_shrink_step_ratio") config.occlusion_shrink_step_ratio = parse_float(key, value);
  else if (key == "occlusion_suspicion_clear_frames") config.occlusion_suspicion_clear_frames = parse_int(key, value);
  else if (key == "occlusion_peak_margin_drop") config.occlusion_peak_margin_drop = parse_float(key, value);
  else if (key == "occlusion_motion_residual_threshold") config.occlusion_motion_residual_threshold = parse_float(key, value);
  else if (key == "occlusion_confirmation_iou") config.occlusion_confirmation_iou = parse_float(key, value);
  else if (key == "occlusion_confirmation_center_distance") config.occlusion_confirmation_center_distance = parse_float(key, value);
  else if (key == "occlusion_confirmation_max_area_log_change") config.occlusion_confirmation_max_area_log_change = parse_float(key, value);
  else if (key == "occlusion_confirmation_max_aspect_log_change") config.occlusion_confirmation_max_aspect_log_change = parse_float(key, value);
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
  else if (key == "template_bridge_min_stable_frames") config.template_bridge_min_stable_frames = parse_int(key, value);
  else if (key == "template_bridge_max_duration_ms") config.template_bridge_max_duration_ms = parse_integer(key, value);
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
  else if (key == "motion_search_enabled") config.motion_search_enabled = parse_bool(key, value);
  else if (key == "search_reacquisition_requires_detector") config.search_reacquisition_requires_detector = parse_bool(key, value);
  else if (key == "search_reacquisition_min_age_ms") config.search_reacquisition_min_age_ms = parse_integer(key, value);
  else if (key == "motion_search_confidence_ms") config.motion_search_confidence_ms = parse_integer(key, value);
  else if (key == "motion_max_speed_diagonals_per_s") config.motion_max_speed_diagonals_per_s = parse_float(key, value);
  else if (key == "motion_max_acceleration_diagonals_per_s2") config.motion_max_acceleration_diagonals_per_s2 = parse_float(key, value);
  else if (key == "motion_scale_rate_limit_per_s") config.motion_scale_rate_limit_per_s = parse_float(key, value);
  else if (key == "search_probe_interval_ms") config.search_probe_interval_ms = parse_integer(key, value);
  else throw std::invalid_argument("unknown controller config key: " + key);
}

bool in_unit_interval(float value) { return value >= 0.0F && value <= 1.0F; }

}  // namespace

ControllerConfig default_controller_config(const std::string& sensor) {
  ControllerConfig config;
  config.sensor = sensor;
  if (sensor == "visible") {
    config.reliable_psr = 4.2F;
    config.weak_psr = 4.0F;
    config.reliable_peak_margin = 0.04F;
    config.weak_peak_margin = 0.02F;
    config.max_accepted_scale_step_ratio = 0.05F;
    config.accepted_scale_deadband_ratio = 0.02F;
    config.accepted_scale_smoothing_alpha = 0.10F;
    config.accepted_scale_median_window_frames = 5;
    config.accepted_scale_direction_confirm_frames = 3;
    config.accepted_scale_output_hysteresis_px = 0.75F;
    config.accepted_center_smoothing_alpha = 0.35F;
    config.accepted_center_max_correction_ratio = 0.25F;
    config.max_reacquisition_center_distance = 0.75F;
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
  if (!std::isfinite(config.max_accepted_scale_step_ratio) ||
      config.max_accepted_scale_step_ratio <= 0.0F ||
      config.max_accepted_scale_step_ratio > 1.0F) {
    throw std::invalid_argument(
        "accepted scale step ratio must be finite and in (0,1]");
  }
  if (!std::isfinite(config.accepted_scale_deadband_ratio) ||
      config.accepted_scale_deadband_ratio < 0.0F ||
      config.accepted_scale_deadband_ratio >= 1.0F) {
    throw std::invalid_argument(
        "accepted scale deadband ratio must be finite and in [0,1)");
  }
  if (!std::isfinite(config.accepted_scale_smoothing_alpha) ||
      config.accepted_scale_smoothing_alpha <= 0.0F ||
      config.accepted_scale_smoothing_alpha > 1.0F) {
    throw std::invalid_argument(
        "accepted scale smoothing alpha must be finite and in (0,1]");
  }
  if (config.accepted_scale_median_window_frames < 1 ||
      config.accepted_scale_median_window_frames > 31 ||
      config.accepted_scale_median_window_frames % 2 == 0) {
    throw std::invalid_argument(
        "accepted scale median window must be odd and in [1,31]");
  }
  if (config.accepted_scale_direction_confirm_frames < 1 ||
      config.accepted_scale_direction_confirm_frames > 31) {
    throw std::invalid_argument(
        "accepted scale direction confirmation must be in [1,31]");
  }
  if (!std::isfinite(config.accepted_scale_output_hysteresis_px) ||
      config.accepted_scale_output_hysteresis_px < 0.0F) {
    throw std::invalid_argument(
        "accepted scale output hysteresis must be finite and non-negative");
  }
  if (!std::isfinite(config.accepted_center_smoothing_alpha) ||
      config.accepted_center_smoothing_alpha <= 0.0F ||
      config.accepted_center_smoothing_alpha > 1.0F) {
    throw std::invalid_argument(
        "accepted center smoothing alpha must be finite and in (0,1]");
  }
  if (!std::isfinite(config.accepted_center_max_correction_ratio) ||
      config.accepted_center_max_correction_ratio < 0.0F) {
    throw std::invalid_argument(
        "accepted center correction ratio must be finite and non-negative");
  }
  if (!std::isfinite(config.max_reacquisition_center_distance) ||
      config.max_reacquisition_center_distance < 0.0F) {
    throw std::invalid_argument(
        "reacquisition center distance must be finite and non-negative");
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
  if (config.occlusion_probe_interval_frames < 1 ||
      config.occlusion_probe_interval_frames > 300) {
    throw std::invalid_argument("occlusion probe interval must be in [1,300]");
  }
  const int occlusion_counts[] = {
      config.occlusion_gradual_consecutive_frames,
      config.occlusion_min_reference_samples,
      config.occlusion_suspicion_clear_frames};
  for (const int value : occlusion_counts) {
    if (value < 1 || value > 60) {
      throw std::invalid_argument("occlusion evidence counts must be in [1,60]");
    }
  }
  const float occlusion_ratios[] = {
      config.occlusion_trigger_min_dimension_ratio,
      config.occlusion_gradual_dimension_ratio,
      config.occlusion_shrink_step_ratio,
      config.occlusion_confirmation_iou};
  for (const float value : occlusion_ratios) {
    if (!std::isfinite(value) || value <= 0.0F || value > 1.0F) {
      throw std::invalid_argument("occlusion ratios must be in (0,1]");
    }
  }
  if (!std::isfinite(config.occlusion_peak_margin_drop) ||
      config.occlusion_peak_margin_drop < 0.0F) {
    throw std::invalid_argument("occlusion peak margin drop must be finite and non-negative");
  }
  const float occlusion_positive[] = {
      config.occlusion_motion_residual_threshold,
      config.occlusion_confirmation_center_distance,
      config.occlusion_confirmation_max_area_log_change,
      config.occlusion_confirmation_max_aspect_log_change};
  for (const float value : occlusion_positive) {
    if (!std::isfinite(value) || value <= 0.0F) {
      throw std::invalid_argument("occlusion residual and confirmation deltas must be finite and positive");
    }
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
  if (config.template_bridge_min_stable_frames < 0 ||
      config.template_bridge_min_stable_frames > 60) {
    throw std::invalid_argument("template bridge stable history is invalid");
  }
  if (config.template_bridge_max_duration_ms < 0 ||
      config.template_bridge_max_duration_ms > 1000) {
    throw std::invalid_argument("template bridge duration is invalid");
  }
  if (config.template_bridge_min_stable_frames > 0 &&
      config.template_bridge_max_duration_ms == 0) {
    throw std::invalid_argument("enabled template bridge requires a duration");
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
  const float motion_limits[] = {
      config.motion_max_speed_diagonals_per_s,
      config.motion_max_acceleration_diagonals_per_s2,
      config.motion_scale_rate_limit_per_s};
  for (const float value : motion_limits) {
    if (!std::isfinite(value) || value <= 0.0F) {
      throw std::invalid_argument(
          "motion search limits must be finite and positive");
    }
  }
  if (config.motion_search_confidence_ms <= 0 ||
      config.search_reacquisition_min_age_ms < 0 ||
      config.search_reacquisition_min_age_ms >
          config.motion_search_confidence_ms ||
      config.search_probe_interval_ms <= 0) {
    throw std::invalid_argument("motion search timing must be positive");
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

std::string controller_config_identity(const ControllerConfig& config) {
  std::ostringstream stream;
  stream << config.sensor << '|' << config.calibrated << '|'
         << config.reliable_score << '|' << config.reliable_psr << '|'
         << config.reliable_apce << '|' << config.reliable_peak_margin << '|'
         << config.weak_score << '|' << config.weak_psr << '|'
         << config.weak_apce << '|' << config.weak_peak_margin << '|'
         << config.max_accepted_scale_step_ratio << '|'
         << config.accepted_scale_deadband_ratio << '|'
         << config.accepted_scale_smoothing_alpha << '|'
         << config.accepted_scale_median_window_frames << '|'
         << config.accepted_scale_direction_confirm_frames << '|'
         << config.accepted_scale_output_hysteresis_px << '|'
         << config.accepted_center_smoothing_alpha << '|'
         << config.accepted_center_max_correction_ratio << '|'
         << config.max_reacquisition_center_distance << '|'
         << config.template_bridge_min_stable_frames << '|'
         << config.template_bridge_max_duration_ms << '|'
         << config.detector_candidate_max_center_ratio << '|'
         << config.occlusion_hold_frames << '|'
         << config.occlusion_trigger_consecutive_unreliable << '|'
         << config.occlusion_trigger_min_dimension_ratio << '|'
         << config.occlusion_reference_window << '|'
         << config.occlusion_probe_interval_frames << '|'
         << config.occlusion_gradual_dimension_ratio << '|'
         << config.occlusion_gradual_consecutive_frames << '|'
         << config.occlusion_min_reference_samples << '|'
         << config.occlusion_shrink_step_ratio << '|'
         << config.occlusion_suspicion_clear_frames << '|'
         << config.occlusion_peak_margin_drop << '|'
         << config.occlusion_motion_residual_threshold << '|'
         << config.occlusion_confirmation_iou << '|'
         << config.occlusion_confirmation_center_distance << '|'
         << config.occlusion_confirmation_max_area_log_change << '|'
         << config.occlusion_confirmation_max_aspect_log_change << '|'
         << to_string(config.detector_target_mode) << '|'
         << config.detector_search_scale << '|'
         << config.detector_subtarget_max_probes << '|'
         << config.detector_confirmation_iou << '|'
         << config.detector_confirmation_center_distance << '|'
         << config.detector_confirmation_max_area_log_change << '|'
         << config.detector_confirmation_max_aspect_log_change << '|'
         << config.uncertain_ms << '|' << config.local_recovery_ms << '|'
         << config.assisted_recovery_ms << '|'
         << config.motion_alpha << '|' << config.motion_beta << '|'
         << config.motion_max_dt_ms << '|'
         << config.motion_search_enabled << '|'
         << config.search_reacquisition_requires_detector << '|'
         << config.search_reacquisition_min_age_ms << '|'
         << config.motion_search_confidence_ms << '|'
         << config.motion_max_speed_diagonals_per_s << '|'
         << config.motion_max_acceleration_diagonals_per_s2 << '|'
         << config.motion_scale_rate_limit_per_s << '|'
         << config.search_probe_interval_ms;
  return stream.str();
}

}  // namespace nanotrack
