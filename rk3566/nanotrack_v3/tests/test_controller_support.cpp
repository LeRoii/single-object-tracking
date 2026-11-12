#include "controller_config.hpp"
#include "detector_adapter.hpp"
#include "motion_estimator.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void expect_near(float actual, float expected, float tolerance,
                 const std::string& message) {
  expect_true(std::fabs(actual - expected) <= tolerance,
              message + " actual=" + std::to_string(actual) +
                  " expected=" + std::to_string(expected));
}

template <typename Function>
void expect_throws(Function function, const std::string& message) {
  try {
    function();
    expect_true(false, message);
  } catch (const std::exception&) {
  }
}

const std::string template_policy_line = "template_policy=recent_safe_v1\n";
const std::string recovery_policy_line = "recovery_policy=unified_candidates_v1\n";

class TemporaryConfigFile {
 public:
  TemporaryConfigFile() {
    const int fd = ::mkstemp(path_.data());
    if (fd == -1) throw std::runtime_error("cannot create test config");
    ::close(fd);
  }

  ~TemporaryConfigFile() { ::unlink(path_.c_str()); }

  TemporaryConfigFile(const TemporaryConfigFile&) = delete;
  TemporaryConfigFile& operator=(const TemporaryConfigFile&) = delete;

  const std::string& path() const { return path_; }

 private:
  std::string path_ = "/tmp/nanotrack-controller-support-XXXXXX";
};

void write_raw_file(const std::string& path, const std::string& contents) {
  std::ofstream stream(path);
  if (!stream) throw std::runtime_error("cannot write test config");
  stream << contents;
}

void write_file(const std::string& path, const std::string& contents) {
  write_raw_file(path, template_policy_line + recovery_policy_line + contents);
}

void expect_config_error(const std::string& path, const std::string& key,
                         const std::string& detail = "") {
  try {
    nanotrack::load_controller_config(path);
    expect_true(false, key + " must fail");
  } catch (const std::invalid_argument& error) {
    const std::string message = error.what();
    expect_true(message.find(key) != std::string::npos &&
                    message.find(detail) != std::string::npos,
                "unexpected error for " + key + ": " + message);
  }
}

std::uint64_t file_fingerprint(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("cannot read historical config: " + path);
  // FNV-1a over the original bytes, including comments and line endings.
  std::uint64_t fingerprint = 14695981039346656037ULL;
  for (char byte; stream.get(byte);) {
    fingerprint ^= static_cast<unsigned char>(byte);
    fingerprint *= 1099511628211ULL;
  }
  return fingerprint;
}

void expect_occlusion_defaults(const nanotrack::ControllerConfig& config,
                               const std::string& context) {
  expect_true(config.occlusion_hold_frames == 12,
              context + " defaults to twelve hold frames");
  expect_true(config.occlusion_trigger_consecutive_unreliable == 2,
              context + " defaults to two consecutive unreliable frames");
  expect_near(config.occlusion_trigger_min_dimension_ratio, 0.70F, 1e-6F,
              context + " defaults to a 0.70 dimension ratio");
  expect_true(config.occlusion_reference_window == 8,
              context + " defaults to eight reference observations");
  expect_true(config.occlusion_probe_interval_frames == 5 &&
                  config.occlusion_gradual_consecutive_frames == 2 &&
                  config.occlusion_min_reference_samples == 3 &&
                  config.occlusion_suspicion_clear_frames == 2,
              context + " defaults to adaptive coast frame counts");
  expect_near(config.occlusion_gradual_dimension_ratio, 0.85F, 1e-6F,
              context + " defaults to a 0.85 gradual dimension ratio");
  expect_near(config.occlusion_shrink_step_ratio, 0.97F, 1e-6F,
              context + " defaults to a 0.97 shrink step ratio");
  expect_near(config.occlusion_peak_margin_drop, 0.02F, 1e-6F,
              context + " defaults to a 0.02 peak margin drop");
  expect_near(config.occlusion_motion_residual_threshold, 0.75F, 1e-6F,
              context + " defaults to a 0.75 motion residual threshold");
  expect_near(config.occlusion_confirmation_iou, 0.50F, 1e-6F,
              context + " defaults to a 0.50 coast confirmation IoU");
  expect_near(config.occlusion_confirmation_center_distance, 0.35F, 1e-6F,
              context + " defaults to a 0.35 coast confirmation center delta");
  expect_near(config.occlusion_confirmation_max_area_log_change, 0.50F, 1e-6F,
              context + " defaults to a 0.50 coast confirmation area delta");
  expect_near(config.occlusion_confirmation_max_aspect_log_change, 0.40F, 1e-6F,
              context + " defaults to a 0.40 coast confirmation aspect delta");
}

}  // namespace

int main() {
  using namespace nanotrack;

  const ControllerConfig default_visible = default_controller_config("visible");
  expect_true(default_visible.template_policy == TemplatePolicy::kRecentSafeV1 &&
                  default_visible.recovery_policy ==
                      RecoveryPolicy::kUnifiedCandidatesV1,
              "programmatic defaults select the current policies");
  expect_occlusion_defaults(ControllerConfig{}, "structure");
  expect_occlusion_defaults(default_visible, "visible defaults");
  expect_true(default_visible.detector_target_mode ==
                  DetectorTargetMode::kFullObject,
              "whole-object detector hypotheses remain the default");
  expect_near(default_visible.reliable_psr, 4.2F, 1e-6F,
              "visible candidate accepts confirmed static-target PSR range");
  expect_near(default_visible.weak_psr, 4.0F, 1e-6F,
              "visible weak PSR retains a lower staging boundary");
  expect_near(default_visible.reliable_peak_margin, 0.04F, 1e-6F,
              "visible candidate accepts confirmed static-target peak margins");
  expect_near(default_visible.weak_peak_margin, 0.02F, 1e-6F,
              "visible weak peak margin retains a lower staging boundary");
  expect_near(default_visible.max_accepted_scale_step_ratio, 0.05F, 1e-6F,
              "visible accepted dimensions are limited to five percent per frame");
  expect_near(default_visible.accepted_scale_deadband_ratio, 0.02F, 1e-6F,
              "visible accepted scale suppresses changes through two percent");
  expect_near(default_visible.accepted_scale_smoothing_alpha, 0.10F, 1e-6F,
              "visible accepted scale moves ten percent toward its bounded target");
  expect_true(default_visible.accepted_scale_median_window_frames == 5 &&
                  default_visible.accepted_scale_direction_confirm_frames == 3,
              "visible accepted scale requires five samples and three directions");
  expect_near(default_visible.accepted_scale_output_hysteresis_px, 0.75F,
              1e-6F, "visible accepted scale uses subpixel output hysteresis");
  expect_near(default_visible.accepted_center_smoothing_alpha, 0.35F, 1e-6F,
              "visible accepted centers blend bounded motion innovation");
  expect_near(default_visible.accepted_center_max_correction_ratio, 0.25F,
              1e-6F, "visible accepted centers bound one-frame correction");
  expect_near(default_visible.max_reacquisition_center_distance, 0.75F,
              1e-6F,
              "visible ordinary recovery uses the fixed center residual gate");
  expect_near(ControllerConfig{}.max_accepted_scale_step_ratio, 1.0F, 1e-6F,
              "generic controller behavior remains unrestricted");
  expect_true(ControllerConfig{}.accepted_scale_deadband_ratio == 0.0F &&
                  ControllerConfig{}.accepted_scale_smoothing_alpha == 1.0F &&
                  ControllerConfig{}.accepted_scale_median_window_frames == 1 &&
                  ControllerConfig{}.accepted_scale_direction_confirm_frames == 1 &&
                  ControllerConfig{}.accepted_scale_output_hysteresis_px == 0.0F &&
                  ControllerConfig{}.accepted_center_smoothing_alpha == 1.0F &&
                  ControllerConfig{}.accepted_center_max_correction_ratio == 0.0F &&
                  ControllerConfig{}.max_reacquisition_center_distance == 0.0F,
              "generic controller defaults preserve legacy scale, center and recovery behavior");
  expect_near(default_visible.detector_candidate_max_center_ratio, 0.15F,
              1e-6F,
              "visible default strictly limits detector center jumps");
  const ControllerConfig default_infrared =
      default_controller_config("infrared");
  expect_occlusion_defaults(default_infrared, "infrared defaults");
  expect_near(default_infrared.reliable_psr, 8.0F, 1e-6F,
              "visible evidence does not alter infrared PSR default");
  expect_near(default_infrared.reliable_peak_margin, 0.10F, 1e-6F,
              "visible evidence does not alter infrared peak-margin default");
  expect_near(default_infrared.weak_psr, 5.0F, 1e-6F,
              "visible evidence does not alter infrared weak PSR default");
  expect_near(default_infrared.weak_peak_margin, 0.04F, 1e-6F,
              "visible evidence does not alter infrared weak peak-margin default");
  expect_near(default_infrared.max_accepted_scale_step_ratio, 1.0F, 1e-6F,
              "infrared accepted dimensions retain existing unrestricted behavior");
  expect_true(default_infrared.accepted_scale_deadband_ratio == 0.0F &&
                  default_infrared.accepted_scale_smoothing_alpha == 1.0F &&
                  default_infrared.accepted_scale_median_window_frames == 1 &&
                  default_infrared.accepted_scale_direction_confirm_frames == 1 &&
                  default_infrared.accepted_scale_output_hysteresis_px == 0.0F &&
                  default_infrared.accepted_center_smoothing_alpha == 1.0F &&
                  default_infrared.accepted_center_max_correction_ratio == 0.0F &&
                  default_infrared.max_reacquisition_center_distance == 0.0F,
              "infrared retains compatibility scale, center and recovery settings");
  expect_near(default_infrared.detector_candidate_max_center_ratio, 0.15F,
              1e-6F,
              "infrared default uses the same strict detector position gate");

  const std::string source_file = __FILE__;
  const std::string source_dir = source_file.substr(
      0, source_file.find_last_of("/\\"));
  const std::string config_dir = source_dir + "/../config/";
  const std::string profile = "visible-recent-safe-v1.conf";
  const TemporaryConfigFile fixture;
  const std::string& config_path = fixture.path();
  write_raw_file(config_path, recovery_policy_line + "sensor=visible\n");
  expect_config_error(config_path, "template_policy", profile);
  write_raw_file(config_path, template_policy_line + "sensor=visible\n");
  expect_config_error(config_path, "recovery_policy", profile);
  write_raw_file(config_path,
                 "template_policy=anchor\n" + recovery_policy_line);
  expect_config_error(config_path, "template_policy", "recent_safe_v1");
  write_raw_file(config_path,
                 template_policy_line + "recovery_policy=detector_only\n");
  expect_config_error(config_path, "recovery_policy", "unified_candidates_v1");
  for (const std::string& key : {"search_reacquisition_requires_detector",
                                 "template_bridge_min_stable_frames",
                                 "template_bridge_max_duration_ms"}) {
    const std::string value =
        key == "search_reacquisition_requires_detector" ? "false" : "0";
    write_raw_file(config_path, key + "=" + value + "\n");
    expect_config_error(config_path, key, profile);
    write_file(config_path, key + "=" + value + "\n");
    expect_config_error(config_path, key, profile);
  }

  struct HistoricalConfig {
    const char* name;
    std::uint64_t fingerprint;
  };
  for (const auto& historical : {
           HistoricalConfig{"infrared-default.conf", 0xcb5995cda4869e12ULL},
           HistoricalConfig{"interactive-debug.conf", 0x07bda556c831c022ULL},
           HistoricalConfig{"rk3588-interactive-debug.conf", 0x582f36558a411bceULL},
           HistoricalConfig{"visible-default.conf", 0xb5a8c0288eef353fULL},
           HistoricalConfig{"visible-occlusion-calibrated.conf",
                            0x4f851040d9bcfc9bULL}}) {
    expect_true(file_fingerprint(config_dir + historical.name) ==
                    historical.fingerprint,
                std::string(historical.name) + " retains its original bytes");
  }
  expect_config_error(config_dir + "visible-default.conf",
                      "template_policy", profile);
  expect_config_error(config_dir + "infrared-default.conf",
                      "template_policy", profile);
  expect_config_error(config_dir + "visible-occlusion-calibrated.conf",
                      "template_bridge_min_stable_frames", profile);

  try {
    const ControllerConfig candidate =
        load_controller_config(config_dir + profile);
    expect_true(candidate.template_policy == TemplatePolicy::kRecentSafeV1 &&
                    candidate.recovery_policy ==
                        RecoveryPolicy::kUnifiedCandidatesV1 &&
                    std::string(to_string(candidate.template_policy)) ==
                        "recent_safe_v1" &&
                    std::string(to_string(candidate.recovery_policy)) ==
                        "unified_candidates_v1",
                "recent-safe profile explicitly selects both versioned policies");
    expect_occlusion_defaults(candidate, "visible recent-safe profile");
    expect_true(candidate.sensor == "visible" && !candidate.calibrated &&
                    candidate.motion_search_enabled,
                "recent-safe profile enables motion search as an uncalibrated candidate");
    expect_true(candidate.confirmation_required == 2 &&
                    candidate.confirmation_window == 3 &&
                    candidate.search_reacquisition_min_age_ms == 400,
                "recent-safe profile preserves the existing recovery gates");
    expect_true(!candidate.search_reacquisition_requires_detector,
                "recent-safe profile leaves the legacy detector requirement inactive");
    expect_true(candidate.template_bridge_min_stable_frames == 0 &&
                    candidate.template_bridge_max_duration_ms == 0,
                "recent-safe profile leaves the legacy template bridge inactive");
    expect_near(candidate.detector_candidate_max_center_ratio, 0.02F, 1e-6F,
                "recent-safe profile preserves the calibrated scalar position gate");
  } catch (const std::exception& error) {
    expect_true(false, std::string("recent-safe profile must load: ") + error.what());
  }
  try {
    const ControllerConfig adaptive =
        load_controller_config(config_dir + "visible-adaptive-display.conf");
    expect_true(adaptive.adaptive_display_enabled &&
                    adaptive.display_detector_refresh_enabled &&
                    adaptive.display_detection_confirmations == 2 &&
                    adaptive.display_detector_initial_interval_ms == 200 &&
                    adaptive.display_runtime_p95_pause_ms == 30.0F,
                "adaptive display profile enables the bounded display-only policy");
  } catch (const std::exception& error) {
    expect_true(false, std::string("adaptive display profile failed to load: ") +
                           error.what());
  }
  if (failures != 0) return 1;

  write_file(config_path,
             "sensor=visible\n"
             "calibrated=false\n"
             "detector_target_mode=subtarget\n"
             "detector_candidate_max_center_ratio=0.15\n"
             "detector_search_scale=1.6\n"
             "detector_subtarget_max_probes=7\n"
             "detector_confirmation_iou=0.55\n"
             "detector_confirmation_center_distance=0.30\n"
             "detector_confirmation_max_area_log_change=0.45\n"
             "detector_confirmation_max_aspect_log_change=0.35\n"
             "reliable_score=0.82\n"
             "reliable_psr=8.5\n"
             "weak_score=0.57\n"
             "max_accepted_scale_step_ratio=0.25\n"
             "accepted_scale_deadband_ratio=0.03\n"
             "accepted_scale_smoothing_alpha=0.20\n"
             "accepted_scale_median_window_frames=7\n"
             "accepted_scale_direction_confirm_frames=4\n"
             "accepted_scale_output_hysteresis_px=0.65\n"
             "accepted_center_smoothing_alpha=0.45\n"
             "accepted_center_max_correction_ratio=0.30\n"
             "max_reacquisition_center_distance=0.80\n"
             "uncertain_ms=200\n"
             "local_recovery_ms=1000\n"
             "assisted_recovery_ms=3000\n"
             "suspicion_enter_ms=120\n"
             "suspicion_clear_ms=180\n"
             "response_psr_drop_ratio=0.15\n"
             "response_apce_drop_ratio=0.35\n"
             "motion_alpha=1.0\n"
             "motion_beta=0.5\n"
             "motion_max_dt_ms=200\n"
             "motion_search_enabled=true\n"
             "search_reacquisition_min_age_ms=300\n"
             "motion_search_confidence_ms=900\n"
             "motion_max_speed_diagonals_per_s=0.75\n"
             "motion_max_acceleration_diagonals_per_s2=2.5\n"
             "motion_scale_rate_limit_per_s=0.30\n"
             "search_probe_interval_ms=120\n");
  const ControllerConfig visible = load_controller_config(config_path);
  expect_occlusion_defaults(visible, "configuration with omitted occlusion keys");
  expect_true(visible.sensor == "visible" && !visible.calibrated,
              "visible config identity and calibration marker");
  expect_true(visible.detector_target_mode == DetectorTargetMode::kSubtarget,
              "subtarget detector mode parses explicitly");
  expect_near(visible.detector_search_scale, 1.6F, 1e-6F,
              "detector search scale override");
  expect_true(visible.detector_subtarget_max_probes == 7,
              "subtarget probe budget override");
  expect_near(visible.detector_confirmation_iou, 0.55F, 1e-6F,
              "detector confirmation IoU override");
  expect_near(visible.detector_candidate_max_center_ratio, 0.15F, 1e-6F,
              "detector center ratio parses at the boundary");
  expect_near(visible.reliable_score, 0.82F, 1e-6F,
              "visible reliable score override");
  expect_near(visible.max_accepted_scale_step_ratio, 0.25F, 1e-6F,
              "accepted scale-step ratio parses explicitly");
  expect_near(visible.accepted_scale_deadband_ratio, 0.03F, 1e-6F,
              "accepted scale deadband parses explicitly");
  expect_near(visible.accepted_scale_smoothing_alpha, 0.20F, 1e-6F,
              "accepted scale smoothing alpha parses explicitly");
  expect_true(visible.accepted_scale_median_window_frames == 7 &&
                  visible.accepted_scale_direction_confirm_frames == 4,
              "accepted scale temporal counts parse explicitly");
  expect_near(visible.accepted_scale_output_hysteresis_px, 0.65F, 1e-6F,
              "accepted scale output hysteresis parses explicitly");
  expect_near(visible.accepted_center_smoothing_alpha, 0.45F, 1e-6F,
              "accepted center smoothing alpha parses explicitly");
  expect_near(visible.accepted_center_max_correction_ratio, 0.30F, 1e-6F,
              "accepted center correction ratio parses explicitly");
  expect_near(visible.max_reacquisition_center_distance, 0.80F, 1e-6F,
              "ordinary recovery center distance parses explicitly");
  expect_true(visible.uncertain_ms == 200 &&
                  visible.assisted_recovery_ms == 3000,
              "state timing overrides");
  expect_true(visible.suspicion_enter_ms == 120 &&
                  visible.suspicion_clear_ms == 180,
              "candidate suspicion timings parse explicitly");
  expect_near(visible.response_psr_drop_ratio, 0.15F, 1e-6F,
              "candidate PSR drop ratio parses explicitly");
  expect_near(visible.response_apce_drop_ratio, 0.35F, 1e-6F,
              "candidate APCE drop ratio parses explicitly");

  for (const std::string& key : {"suspicion_enter_ms", "suspicion_clear_ms"}) {
    for (const std::string& value : {"0", "-1", "1.5"}) {
      write_file(config_path, key + "=" + value + "\n");
      expect_config_error(config_path, key);
    }
  }
  for (const std::string& key : {"response_psr_drop_ratio",
                                 "response_apce_drop_ratio"}) {
    for (const std::string& value : {"0", "-0.1", "1.01", "nan"}) {
      write_file(config_path, key + "=" + value + "\n");
      expect_config_error(config_path, key);
    }
  }
  expect_true(visible.motion_search_enabled &&
                  visible.search_reacquisition_min_age_ms == 300 &&
                  visible.motion_search_confidence_ms == 900 &&
                  visible.search_probe_interval_ms == 120,
              "time-based motion search settings parse explicitly");
  expect_near(visible.motion_max_speed_diagonals_per_s, 0.75F, 1e-6F,
              "motion speed cap parses explicitly");
  expect_near(visible.motion_max_acceleration_diagonals_per_s2, 2.5F, 1e-6F,
              "motion acceleration cap parses explicitly");
  expect_near(visible.motion_scale_rate_limit_per_s, 0.30F, 1e-6F,
              "motion scale-rate cap parses explicitly");

  write_file(config_path,
             "sensor=infrared\n"
             "calibrated=false\n"
             "motion_search_enabled=true\n"
             "reliable_score=0.74\n");
  const ControllerConfig infrared = load_controller_config(config_path);
  expect_true(infrared.sensor == "infrared" && infrared.motion_search_enabled,
              "infrared config keeps a separate identity");
  expect_true(!infrared.search_reacquisition_requires_detector &&
                  infrared.template_bridge_min_stable_frames == 0 &&
                  infrared.template_bridge_max_duration_ms == 0,
              "infrared motion search leaves the legacy recovery settings inactive");
  expect_near(infrared.reliable_score, 0.74F, 1e-6F,
              "infrared override does not inherit visible values");

  write_file(config_path, "sensor=visible\nmotion_search_enabled=false\n");
  const ControllerConfig no_motion_search = load_controller_config(config_path);
  expect_true(!no_motion_search.motion_search_enabled &&
                  !no_motion_search.search_reacquisition_requires_detector &&
                  no_motion_search.template_bridge_min_stable_frames == 0 &&
                  no_motion_search.template_bridge_max_duration_ms == 0,
              "visible profiles with motion search disabled leave the legacy recovery settings inactive");

  const std::string default_identity = controller_config_identity(default_visible);
  ControllerConfig identity_variant = default_visible;
  identity_variant.accepted_scale_deadband_ratio = 0.03F;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes accepted scale deadband");
  identity_variant = default_visible;
  identity_variant.accepted_scale_smoothing_alpha = 0.20F;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes accepted scale smoothing alpha");
  identity_variant = default_visible;
  identity_variant.accepted_scale_median_window_frames = 7;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes accepted scale median window");
  identity_variant = default_visible;
  identity_variant.accepted_scale_direction_confirm_frames = 4;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes accepted scale direction confirmation");
  identity_variant = default_visible;
  identity_variant.accepted_scale_output_hysteresis_px = 0.5F;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes accepted scale output hysteresis");
  identity_variant = default_visible;
  identity_variant.accepted_center_smoothing_alpha = 0.45F;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes accepted center smoothing alpha");
  identity_variant = default_visible;
  identity_variant.accepted_center_max_correction_ratio = 0.30F;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes accepted center correction ratio");
  identity_variant = default_visible;
  identity_variant.max_reacquisition_center_distance = 0.80F;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes ordinary recovery center distance");
  identity_variant = default_visible;
  identity_variant.template_bridge_min_stable_frames = 3;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes template bridge history");
  identity_variant = default_visible;
  identity_variant.template_bridge_max_duration_ms = 200;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes template bridge duration");
  identity_variant = default_visible;
  identity_variant.motion_search_enabled = true;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes motion search enablement");
  identity_variant = default_visible;
  identity_variant.search_reacquisition_requires_detector = true;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes detector-required search recovery");
  identity_variant = default_visible;
  identity_variant.search_reacquisition_min_age_ms = 100;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes minimum search recovery age");
  identity_variant = default_visible;
  identity_variant.motion_search_confidence_ms += 1;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes motion search confidence time");
  identity_variant = default_visible;
  identity_variant.motion_max_speed_diagonals_per_s *= 0.5F;
  expect_true(controller_config_identity(identity_variant) != default_identity,
              "built-in identity includes normalized motion speed cap");

  write_file(config_path, "sensor=visible\nunknown_key=1\n");
  expect_throws([&] { load_controller_config(config_path); },
                "unknown config keys must fail");
  write_file(config_path, "sensor=visible\nsensor=infrared\n");
  expect_throws([&] { load_controller_config(config_path); },
                "duplicate config keys must fail");
  write_file(config_path, "sensor=visible\nreliable_score=nan\n");
  expect_throws([&] { load_controller_config(config_path); },
                "non-finite config values must fail");
  write_file(config_path,
             "sensor=visible\ndetector_candidate_max_center_ratio=nan\n");
  expect_throws(
      [&] { load_controller_config(config_path); },
      "non-finite detector center ratio must fail");
  write_file(config_path,
             "sensor=visible\ndetector_candidate_max_center_ratio=-0.01\n");
  expect_throws(
      [&] { load_controller_config(config_path); },
      "negative detector center ratio must fail");
  write_file(config_path,
             "sensor=visible\ndetector_candidate_max_center_ratio=1.01\n");
  expect_throws(
      [&] { load_controller_config(config_path); },
      "detector center ratio above one must fail");
  write_file(config_path,
             "sensor=visible\nuncertain_ms=1000\nlocal_recovery_ms=500\n");
  expect_throws([&] { load_controller_config(config_path); },
                "non-increasing state timing must fail");
  write_file(config_path,
             "sensor=visible\ndetector_target_mode=automatic\n");
  expect_throws([&] { load_controller_config(config_path); },
                "unsupported detector target mode must fail");
  ControllerConfig invalid_detector_geometry = default_visible;
  invalid_detector_geometry.detector_confirmation_iou = 1.1F;
  expect_throws([&] { validate_controller_config(invalid_detector_geometry); },
                "detector confirmation IoU must stay in unit interval");
  invalid_detector_geometry = default_visible;
  invalid_detector_geometry.detector_subtarget_max_probes = 0;
  expect_throws([&] { validate_controller_config(invalid_detector_geometry); },
                "subtarget probe budget must be positive");
  ControllerConfig unsafe_confirmation = default_visible;
  unsafe_confirmation.confirmation_required = 1;
  expect_throws([&] { validate_controller_config(unsafe_confirmation); },
                "recovery confirmation policy must remain two of three");
  ControllerConfig invalid_bridge = default_visible;
  invalid_bridge.template_bridge_min_stable_frames = 61;
  expect_throws([&] { validate_controller_config(invalid_bridge); },
                "template bridge history has a bounded maximum");
  invalid_bridge.template_bridge_min_stable_frames = -1;
  expect_throws([&] { validate_controller_config(invalid_bridge); },
                "template bridge history cannot be negative");
  invalid_bridge = default_visible;
  invalid_bridge.template_bridge_max_duration_ms = -1;
  expect_throws([&] { validate_controller_config(invalid_bridge); },
                "template bridge duration cannot be negative");
  invalid_bridge.template_bridge_max_duration_ms = 1001;
  expect_throws([&] { validate_controller_config(invalid_bridge); },
                "template bridge duration has a bounded maximum");

  write_file(config_path,
             "occlusion_hold_frames=24\n"
             "occlusion_trigger_consecutive_unreliable=4\n"
             "occlusion_trigger_min_dimension_ratio=0.55\n"
             "occlusion_reference_window=16\n");
  const ControllerConfig occlusion = load_controller_config(config_path);
  expect_true(occlusion.occlusion_hold_frames == 24 &&
                  occlusion.occlusion_trigger_consecutive_unreliable == 4 &&
                  occlusion.occlusion_reference_window == 16,
              "occlusion count overrides parse independently");
  expect_near(occlusion.occlusion_trigger_min_dimension_ratio, 0.55F, 1e-6F,
              "occlusion dimension ratio override parses");

  struct CountLimit {
    const char* key;
    int ControllerConfig::*field;
    int minimum;
    int maximum;
  };
  const CountLimit occlusion_counts[] = {
      {"occlusion_hold_frames", &ControllerConfig::occlusion_hold_frames, 1, 300},
      {"occlusion_trigger_consecutive_unreliable",
       &ControllerConfig::occlusion_trigger_consecutive_unreliable, 1, 10},
      {"occlusion_reference_window", &ControllerConfig::occlusion_reference_window,
       3, 60},
      {"occlusion_probe_interval_frames",
       &ControllerConfig::occlusion_probe_interval_frames, 1, 300},
      {"occlusion_gradual_consecutive_frames",
       &ControllerConfig::occlusion_gradual_consecutive_frames, 1, 60},
      {"occlusion_min_reference_samples",
       &ControllerConfig::occlusion_min_reference_samples, 1, 60},
      {"occlusion_suspicion_clear_frames",
       &ControllerConfig::occlusion_suspicion_clear_frames, 1, 60},
  };
  for (const CountLimit& limit : occlusion_counts) {
    for (int value : {limit.minimum, limit.maximum}) {
      write_file(config_path,
                 std::string(limit.key) + "=" + std::to_string(value) + "\n");
      expect_true(load_controller_config(config_path).*(limit.field) == value,
                  std::string(limit.key) + " accepts both inclusive limits");
    }
    for (int value : {limit.minimum - 1, limit.maximum + 1}) {
      ControllerConfig invalid = default_visible;
      invalid.*(limit.field) = value;
      expect_throws([&] { validate_controller_config(invalid); },
                    std::string(limit.key) + " rejects values outside limits");
      write_file(config_path,
                 std::string(limit.key) + "=" + std::to_string(value) + "\n");
      expect_throws([&] { load_controller_config(config_path); },
                    std::string(limit.key) + " rejects invalid file values");
    }
    for (const std::string& value : {"1.5", "2147483648"}) {
      write_file(config_path, std::string(limit.key) + "=" + value + "\n");
      expect_throws([&] { load_controller_config(config_path); },
                    std::string(limit.key) + " requires an in-range integer");
    }
  }
  struct FloatLimit {
    const char* key;
    float ControllerConfig::*field;
    bool allow_zero;
    bool bounded_at_one;
  };
  const FloatLimit occlusion_floats[] = {
      {"max_accepted_scale_step_ratio",
       &ControllerConfig::max_accepted_scale_step_ratio, false, true},
      {"occlusion_trigger_min_dimension_ratio",
       &ControllerConfig::occlusion_trigger_min_dimension_ratio, false, true},
      {"occlusion_gradual_dimension_ratio",
       &ControllerConfig::occlusion_gradual_dimension_ratio, false, true},
      {"occlusion_shrink_step_ratio",
       &ControllerConfig::occlusion_shrink_step_ratio, false, true},
      {"occlusion_confirmation_iou",
       &ControllerConfig::occlusion_confirmation_iou, false, true},
      {"occlusion_peak_margin_drop",
       &ControllerConfig::occlusion_peak_margin_drop, true, false},
      {"occlusion_motion_residual_threshold",
       &ControllerConfig::occlusion_motion_residual_threshold, false, false},
      {"occlusion_confirmation_center_distance",
       &ControllerConfig::occlusion_confirmation_center_distance, false, false},
      {"occlusion_confirmation_max_area_log_change",
       &ControllerConfig::occlusion_confirmation_max_area_log_change, false, false},
      {"occlusion_confirmation_max_aspect_log_change",
       &ControllerConfig::occlusion_confirmation_max_aspect_log_change, false, false},
  };
  for (const FloatLimit& limit : occlusion_floats) {
    for (float value : {0.0F, 0.01F, 0.63F, 1.0F, 1.01F, 100.0F, -0.01F,
                        std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
      const bool valid = std::isfinite(value) &&
                         (limit.allow_zero ? value >= 0.0F : value > 0.0F) &&
                         (!limit.bounded_at_one || value <= 1.0F);
      ControllerConfig candidate = default_visible;
      candidate.*(limit.field) = value;
      write_file(config_path,
                 std::string(limit.key) + "=" + std::to_string(value) + "\n");
      if (valid) {
        validate_controller_config(candidate);
        expect_near(load_controller_config(config_path).*(limit.field), value,
                    1e-6F, std::string(limit.key) + " accepts valid file values");
      } else {
        expect_throws([&] { validate_controller_config(candidate); },
                      std::string(limit.key) + " rejects invalid direct values");
        expect_throws([&] { load_controller_config(config_path); },
                      std::string(limit.key) + " rejects invalid file values");
      }
    }
  }

  struct NewFloatLimit {
    const char* key;
    float ControllerConfig::*field;
  };
  const NewFloatLimit new_unit_limits[] = {
      {"accepted_scale_deadband_ratio",
       &ControllerConfig::accepted_scale_deadband_ratio},
      {"accepted_scale_smoothing_alpha",
       &ControllerConfig::accepted_scale_smoothing_alpha},
      {"accepted_center_smoothing_alpha",
       &ControllerConfig::accepted_center_smoothing_alpha},
  };
  for (const NewFloatLimit& limit : new_unit_limits) {
    for (float value : {0.0F, 0.01F, 0.99F, 1.0F, 1.01F, -0.01F,
                        std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
      const bool deadband = std::string(limit.key) ==
                            "accepted_scale_deadband_ratio";
      const bool valid = std::isfinite(value) &&
                         (deadband ? value >= 0.0F : value > 0.0F) &&
                         (deadband ? value < 1.0F : value <= 1.0F);
      ControllerConfig candidate = default_visible;
      candidate.*(limit.field) = value;
      write_file(config_path,
                 std::string(limit.key) + "=" + std::to_string(value) + "\n");
      if (valid) {
        validate_controller_config(candidate);
        expect_near(load_controller_config(config_path).*(limit.field), value,
                    1e-6F, std::string(limit.key) + " accepts its exact range");
      } else {
        expect_throws([&] { validate_controller_config(candidate); },
                      std::string(limit.key) + " rejects invalid direct values");
        expect_throws([&] { load_controller_config(config_path); },
                      std::string(limit.key) + " rejects invalid file values");
      }
    }
  }
  for (int value : {1, 3, 31}) {
    ControllerConfig candidate = default_visible;
    candidate.accepted_scale_median_window_frames = value;
    validate_controller_config(candidate);
  }
  for (int value : {0, 2, 32}) {
    ControllerConfig candidate = default_visible;
    candidate.accepted_scale_median_window_frames = value;
    expect_throws([&] { validate_controller_config(candidate); },
                  "accepted scale median window must be odd and within [1,31]");
  }
  for (int value : {1, 31}) {
    ControllerConfig candidate = default_visible;
    candidate.accepted_scale_direction_confirm_frames = value;
    validate_controller_config(candidate);
  }
  for (int value : {0, 32}) {
    ControllerConfig candidate = default_visible;
    candidate.accepted_scale_direction_confirm_frames = value;
    expect_throws([&] { validate_controller_config(candidate); },
                  "accepted scale direction confirmation must stay within [1,31]");
  }
  for (float value : {0.0F, 0.75F, 100.0F}) {
    ControllerConfig candidate = default_visible;
    candidate.accepted_scale_output_hysteresis_px = value;
    validate_controller_config(candidate);
  }
  for (float value : {-0.01F, std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity()}) {
    ControllerConfig candidate = default_visible;
    candidate.accepted_scale_output_hysteresis_px = value;
    expect_throws([&] { validate_controller_config(candidate); },
                  "accepted scale output hysteresis must be finite and nonnegative");
  }
  for (float value : {0.0F, 0.25F, 100.0F}) {
    ControllerConfig candidate = default_visible;
    candidate.accepted_center_max_correction_ratio = value;
    validate_controller_config(candidate);
  }
  for (float value : {-0.01F, std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity()}) {
    ControllerConfig candidate = default_visible;
    candidate.accepted_center_max_correction_ratio = value;
    expect_throws([&] { validate_controller_config(candidate); },
                  "accepted center correction ratio must be finite and nonnegative");
  }
  for (float value : {0.0F, 0.01F, 0.75F, 100.0F, -0.01F,
                      std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity()}) {
    const bool valid = std::isfinite(value) && value >= 0.0F;
    ControllerConfig candidate = default_visible;
    candidate.max_reacquisition_center_distance = value;
    write_file(config_path,
               "max_reacquisition_center_distance=" + std::to_string(value) +
                   "\n");
    if (valid) {
      validate_controller_config(candidate);
      expect_near(load_controller_config(config_path)
                      .max_reacquisition_center_distance,
                  value, 1e-6F,
                  "ordinary recovery center distance accepts zero or positive finite values");
    } else {
      expect_throws([&] { validate_controller_config(candidate); },
                    "ordinary recovery center distance rejects invalid direct values");
      expect_throws([&] { load_controller_config(config_path); },
                    "ordinary recovery center distance rejects invalid file values");
    }
  }

  ControllerConfig motion_config;
  motion_config.motion_alpha = 1.0F;
  motion_config.motion_beta = 0.5F;
  motion_config.motion_max_dt_ms = 200;
  MotionEstimator motion(motion_config);
  motion.initialize(BBox{10.0F, 20.0F, 10.0F, 10.0F}, 0, 100, 80);
  const BBox first_prediction = motion.predict(100);
  expect_near(first_prediction.x, 10.0F, 1e-6F,
              "initial zero-velocity prediction");
  motion.correct(BBox{20.0F, 20.0F, 10.0F, 10.0F}, 100);
  const BBox second_prediction = motion.predict(200);
  expect_near(second_prediction.x, 25.0F, 1e-5F,
              "alpha-beta model predicts learned velocity");
  expect_near(second_prediction.y, 20.0F, 1e-5F,
              "alpha-beta model keeps unmoved axis");
  expect_throws([&] { motion.predict(200); },
                "repeated prediction timestamp must fail");
  expect_throws([&] { motion.correct(BBox{20, 20, 10, 10}, 199); },
                "correction timestamp must match latest prediction");

  ControllerConfig bounded_motion_config = motion_config;
  bounded_motion_config.motion_search_confidence_ms = 200;
  bounded_motion_config.motion_max_speed_diagonals_per_s = 0.20F;
  bounded_motion_config.motion_max_acceleration_diagonals_per_s2 = 0.40F;
  bounded_motion_config.motion_scale_rate_limit_per_s = 0.25F;
  MotionEstimator bounded_motion(bounded_motion_config);
  bounded_motion.initialize(BBox{100, 80, 40, 30}, 0, 1000, 600);
  std::int64_t sample_time = 0;
  float sample_x = 100.0F;
  for (const std::int64_t dt_ms : {16, 33, 67, 200}) {
    sample_time += dt_ms;
    sample_x += 0.04F * static_cast<float>(dt_ms);
    bounded_motion.predict(sample_time);
    bounded_motion.correct(BBox{sample_x, 80, 80, 15}, sample_time);
    const MotionSnapshot snapshot = bounded_motion.snapshot(sample_time);
    const float diagonal = std::hypot(1000.0F, 600.0F);
    expect_true(std::isfinite(snapshot.velocity_x) &&
                    std::isfinite(snapshot.acceleration_x) &&
                    std::hypot(snapshot.velocity_x, snapshot.velocity_y) <=
                        0.20F * diagonal + 1e-3F &&
                    std::hypot(snapshot.acceleration_x, snapshot.acceleration_y) <=
                        0.40F * diagonal + 1e-3F,
                "variable-dt accepted observations keep finite bounded derivatives");
    expect_true(std::fabs(snapshot.scale_velocity_width) <=
                        snapshot.bbox.width * 0.25F + 1e-3F &&
                    std::fabs(snapshot.scale_velocity_height) <=
                        snapshot.bbox.height * 0.25F + 1e-3F,
                "accepted scale changes obey the per-second rate limit");
  }
  bounded_motion.begin_search(sample_time);
  expect_true(bounded_motion.prediction_confident(sample_time) &&
                  bounded_motion.prediction_confident(sample_time + 200) &&
                  !bounded_motion.prediction_confident(sample_time + 201),
              "motion confidence is inclusive and expires by elapsed time");
  const MotionSnapshot searching = bounded_motion.snapshot(sample_time + 67);
  expect_true(searching.search_age_ms == 67 && searching.prediction_confident,
              "motion snapshot publishes time-based search age and confidence");
  bounded_motion.end_search();
  expect_true(!bounded_motion.prediction_confident(sample_time + 67) &&
                  bounded_motion.snapshot(sample_time + 67).search_age_ms == -1,
              "ending search removes motion-control confidence");

  MotionEstimator frozen_motion(motion_config);
  frozen_motion.initialize(BBox{40, 30, 20, 20}, 0, 200, 160);
  frozen_motion.predict(100);
  frozen_motion.correct(BBox{50, 20, 24, 18}, 100);
  const auto before_freeze = frozen_motion.snapshot(100);
  frozen_motion.freeze_open_loop_dynamics();
  const auto after_freeze = frozen_motion.snapshot(100);
  expect_near(after_freeze.velocity_x, before_freeze.velocity_x, 1e-6F,
              "freezing updates preserves horizontal clean velocity");
  expect_near(after_freeze.velocity_y, before_freeze.velocity_y, 1e-6F,
              "freezing updates preserves vertical clean velocity");
  expect_near(after_freeze.acceleration_x, 0.0F, 1e-6F,
              "freezing updates clears horizontal acceleration");
  expect_near(after_freeze.acceleration_y, 0.0F, 1e-6F,
              "freezing updates clears vertical acceleration");
  expect_near(after_freeze.scale_velocity_width, 0.0F, 1e-6F,
              "freezing updates clears width velocity");
  expect_near(after_freeze.scale_velocity_height, 0.0F, 1e-6F,
              "freezing updates clears height velocity");

  MotionEstimator uninitialized_motion(bounded_motion_config);
  expect_throws([&] { uninitialized_motion.snapshot(0); },
                "uninitialized motion snapshots fail closed");

  for (int invalid_case = 0; invalid_case < 5; ++invalid_case) {
    ControllerConfig invalid_motion_config;
    if (invalid_case == 0) invalid_motion_config.motion_search_confidence_ms = 0;
    if (invalid_case == 1) invalid_motion_config.motion_max_speed_diagonals_per_s = 0.0F;
    if (invalid_case == 2) invalid_motion_config.motion_max_acceleration_diagonals_per_s2 = -1.0F;
    if (invalid_case == 3) invalid_motion_config.motion_scale_rate_limit_per_s =
        std::numeric_limits<float>::quiet_NaN();
    if (invalid_case == 4) invalid_motion_config.search_probe_interval_ms = 0;
    expect_throws([&] { validate_controller_config(invalid_motion_config); },
                  "invalid motion-search limits fail configuration validation");
  }

  MotionEstimator clipped(motion_config);
  clipped.initialize(BBox{90.0F, 70.0F, 10.0F, 10.0F}, 0, 100, 80);
  clipped.predict(100);
  clipped.correct(BBox{95.0F, 75.0F, 5.0F, 5.0F}, 100);
  const BBox clipped_prediction = clipped.predict(1000);
  expect_true(clipped_prediction.x >= 0.0F && clipped_prediction.y >= 0.0F &&
                  clipped_prediction.x + clipped_prediction.width <= 100.0F &&
                  clipped_prediction.y + clipped_prediction.height <= 80.0F,
              "long-dt prediction is capped and clipped to the frame");

  MotionEstimator boundary(motion_config);
  boundary.initialize(BBox{80.0F, 30.0F, 10.0F, 10.0F}, 0, 100, 80);
  boundary.predict(100);
  boundary.correct(BBox{90.0F, 30.0F, 10.0F, 10.0F}, 100);
  const BBox boundary_prediction = boundary.predict(300);
  expect_true(boundary_prediction.x == 90.0F &&
                  boundary_prediction.width == 10.0F,
              "prediction crossing the frame edge preserves a valid boundary box");
  const BBox repeated_boundary_prediction = boundary.predict(500);
  expect_true(repeated_boundary_prediction.x == 90.0F &&
                  repeated_boundary_prediction.width == 10.0F,
              "clamped motion does not keep accelerating outside the frame");

  ControllerConfig precision_config = motion_config;
  precision_config.motion_beta = 0.0F;
  bool fractional_edges_are_valid = true;
  for (int index = 1; index <= 10000; ++index) {
    const float height = 1.0F + 478.0F * static_cast<float>(index) / 10000.0F;
    const BBox edge =
        clip_bbox(BBox{10.0F, 480.0F - height, 10.0F, height}, 856, 480);
    try {
      MotionEstimator precision_boundary(precision_config);
      precision_boundary.initialize(
          BBox{10.0F, 0.0F, 10.0F, edge.height}, 0, 856, 480);
      precision_boundary.predict(100);
      precision_boundary.correct(edge, 100);
      validate_bbox(precision_boundary.predict(200), 856, 480);
    } catch (const std::exception&) {
      fractional_edges_are_valid = false;
      break;
    }
  }
  expect_true(fractional_edges_are_valid,
              "floating-point edge projection remains strictly inside frame");

  MotionEstimator frozen_scale(motion_config);
  frozen_scale.initialize(BBox{100.0F, 100.0F, 80.0F, 60.0F}, 0, 500, 400);
  frozen_scale.predict(100);
  frozen_scale.correct(BBox{120.0F, 115.0F, 60.0F, 40.0F}, 100);
  frozen_scale.freeze_scale(40.0F, 30.0F);
  expect_true(frozen_scale.scale_frozen(),
              "explicit scale freeze reports active state");
  const BBox first_frozen_prediction = frozen_scale.predict(200);
  expect_near(first_frozen_prediction.x + 0.5F * first_frozen_prediction.width,
              155.0F, 1e-5F,
              "scale freeze preserves learned horizontal center velocity");
  expect_near(first_frozen_prediction.y + 0.5F * first_frozen_prediction.height,
              137.5F, 1e-5F,
              "scale freeze preserves learned vertical center velocity");
  expect_near(first_frozen_prediction.width, 40.0F, 1e-6F,
              "first frozen prediction retains requested width");
  expect_near(first_frozen_prediction.height, 30.0F, 1e-6F,
              "first frozen prediction retains requested height");
  frozen_scale.correct(BBox{145.0F, 130.0F, 20.0F, 10.0F}, 200);
  const BBox second_frozen_prediction = frozen_scale.predict(300);
  expect_true(second_frozen_prediction.x +
                      0.5F * second_frozen_prediction.width >
                  first_frozen_prediction.x +
                      0.5F * first_frozen_prediction.width,
              "frozen scale does not stop corrected center motion");
  expect_near(second_frozen_prediction.width, 40.0F, 1e-6F,
              "correction cannot reintroduce frozen width motion");
  expect_near(second_frozen_prediction.height, 30.0F, 1e-6F,
              "correction cannot reintroduce frozen height motion");

  MotionEstimator frozen_boundary(motion_config);
  frozen_boundary.initialize(BBox{90.0F, 70.0F, 10.0F, 10.0F}, 0, 100, 80);
  frozen_boundary.freeze_scale(40.0F, 30.0F);
  const BBox frozen_boundary_prediction = frozen_boundary.predict(100);
  validate_bbox(frozen_boundary_prediction, 100, 80);
  expect_near(frozen_boundary_prediction.x, 60.0F, 1e-6F,
              "frozen scale clamps horizontal center to frame");
  expect_near(frozen_boundary_prediction.y, 50.0F, 1e-6F,
              "frozen scale clamps vertical center to frame");
  frozen_boundary.initialize(BBox{10.0F, 10.0F, 20.0F, 20.0F}, 200, 100, 80);
  expect_true(!frozen_boundary.scale_frozen(),
              "initialization clears scale freeze state");

  MotionEstimator freeze_clamped_velocity(motion_config);
  freeze_clamped_velocity.initialize(
      BBox{80.0F, 30.0F, 10.0F, 10.0F}, 0, 100, 80);
  freeze_clamped_velocity.predict(100);
  freeze_clamped_velocity.correct(
      BBox{70.0F, 30.0F, 10.0F, 10.0F}, 100);
  freeze_clamped_velocity.freeze_scale(80.0F, 30.0F);
  const BBox freeze_clamped_prediction =
      freeze_clamped_velocity.predict(200);
  expect_near(freeze_clamped_prediction.x +
                  0.5F * freeze_clamped_prediction.width,
              55.0F, 1e-5F,
              "freeze-induced boundary clamp preserves learned inward center velocity");

  MotionEstimator invalid_freeze(motion_config);
  invalid_freeze.initialize(BBox{10.0F, 10.0F, 20.0F, 20.0F}, 0, 100, 80);
  for (float invalid_width : {0.0F, -1.0F,
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity()}) {
    expect_throws([&] { invalid_freeze.freeze_scale(invalid_width, 20.0F); },
                  "scale freeze rejects invalid width");
  }
  for (float invalid_height : {0.0F, -1.0F,
                               std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity()}) {
    expect_throws([&] { invalid_freeze.freeze_scale(20.0F, invalid_height); },
                  "scale freeze rejects invalid height");
  }
  expect_true(!invalid_freeze.scale_frozen(),
              "rejected dimensions do not activate scale freeze");

  NullDetectorAdapter detector;
  expect_true(!detector.available(), "null detector reports unavailable");
  expect_true(!detector.can_submit(),
              "null detector never reports submission readiness");
  const ImageView empty_image{};
  detector.submit(empty_image,
                  DetectorRequest{1, 100, BBox{0, 0, 10, 10}, 3});
  expect_true(!detector.poll().has_value(),
              "null detector never produces a batch");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "controller support tests passed\n";
  return 0;
}
