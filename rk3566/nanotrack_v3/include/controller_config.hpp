#pragma once

#include "long_term_types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nanotrack {

struct ControllerConfig {
  std::string sensor = "visible";
  bool calibrated = false;

  float reliable_score = 0.80F;
  float reliable_psr = 8.0F;
  float reliable_apce = 8.0F;
  float reliable_peak_margin = 0.10F;
  float weak_score = 0.55F;
  float weak_psr = 5.0F;
  float weak_apce = 4.0F;
  float weak_peak_margin = 0.04F;
  float template_agreement_iou = 0.30F;
  float max_center_distance = 1.50F;
  float max_area_log_change = 1.00F;
  float max_aspect_log_change = 0.70F;

  std::int64_t uncertain_ms = 200;
  std::int64_t local_recovery_ms = 1000;
  std::int64_t assisted_recovery_ms = 3000;
  int occlusion_hold_frames = 12;
  int occlusion_trigger_consecutive_unreliable = 2;
  float occlusion_trigger_min_dimension_ratio = 0.70F;
  int occlusion_reference_window = 8;
  std::int64_t detector_interval_ms = 500;
  std::int64_t detector_stale_ms = 150;
  std::size_t detector_max_candidates = 3;
  float detector_candidate_max_center_ratio = 0.15F;
  DetectorTargetMode detector_target_mode = DetectorTargetMode::kFullObject;
  float detector_search_scale = 1.5F;
  std::size_t detector_subtarget_max_probes = 9;
  float detector_confirmation_iou = 0.50F;
  float detector_confirmation_center_distance = 0.35F;
  float detector_confirmation_max_area_log_change = 0.50F;
  float detector_confirmation_max_aspect_log_change = 0.40F;

  int stable_frames_for_update = 15;
  std::int64_t recent_update_interval_ms = 1000;
  int recent_probation_frames = 3;
  int confirmation_window = 3;
  int confirmation_required = 2;

  float search_tracking = 1.0F;
  float search_uncertain = 1.5F;
  float search_local_initial = 2.0F;
  float search_local_late = 3.0F;
  float search_assisted = 4.0F;
  float search_lost = 4.0F;

  float motion_alpha = 0.85F;
  float motion_beta = 0.05F;
  std::int64_t motion_max_dt_ms = 200;
};

ControllerConfig default_controller_config(const std::string& sensor);
ControllerConfig load_controller_config(const std::string& path);
void validate_controller_config(const ControllerConfig& config);

}  // namespace nanotrack
