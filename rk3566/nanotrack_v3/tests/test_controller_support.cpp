#include "controller_config.hpp"
#include "detector_adapter.hpp"
#include "motion_estimator.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

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

void write_file(const std::string& path, const std::string& contents) {
  std::ofstream stream(path);
  if (!stream) throw std::runtime_error("cannot write test config");
  stream << contents;
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
}

}  // namespace

int main() {
  using namespace nanotrack;

  const ControllerConfig default_visible = default_controller_config("visible");
  expect_occlusion_defaults(ControllerConfig{}, "structure");
  expect_occlusion_defaults(default_visible, "visible defaults");
  expect_true(default_visible.detector_target_mode ==
                  DetectorTargetMode::kFullObject,
              "whole-object detector hypotheses remain the default");
  expect_near(default_visible.reliable_psr, 6.0F, 1e-6F,
              "visible bring-up default accepts observed RKNN PSR range");
  expect_near(default_visible.reliable_peak_margin, 0.06F, 1e-6F,
              "visible bring-up default accepts observed RKNN peak margin");
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
  expect_near(default_infrared.detector_candidate_max_center_ratio, 0.15F,
              1e-6F,
              "infrared default uses the same strict detector position gate");

  const std::string source_file = __FILE__;
  const std::string source_dir = source_file.substr(
      0, source_file.find_last_of("/\\"));
  const std::string config_dir = source_dir + "/../config/";
  expect_occlusion_defaults(
      load_controller_config(config_dir + "visible-default.conf"),
      "shipped visible configuration");
  expect_occlusion_defaults(
      load_controller_config(config_dir + "infrared-default.conf"),
      "shipped infrared configuration");
  expect_near(load_controller_config(config_dir + "visible-default.conf")
                  .detector_candidate_max_center_ratio,
              0.15F, 1e-6F,
              "shipped visible config enables the strict detector gate");
  expect_near(load_controller_config(config_dir + "infrared-default.conf")
                  .detector_candidate_max_center_ratio,
              0.15F, 1e-6F,
              "shipped infrared config enables the strict detector gate");

  const std::string visible_path = "/tmp/nanotrack_visible_test.conf";
  write_file(visible_path,
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
             "uncertain_ms=200\n"
             "local_recovery_ms=1000\n"
             "assisted_recovery_ms=3000\n"
             "motion_alpha=1.0\n"
             "motion_beta=0.5\n"
             "motion_max_dt_ms=200\n");
  const ControllerConfig visible = load_controller_config(visible_path);
  expect_occlusion_defaults(visible, "legacy configuration with missing keys");
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
  expect_true(visible.uncertain_ms == 200 &&
                  visible.assisted_recovery_ms == 3000,
              "state timing overrides");

  const std::string infrared_path = "/tmp/nanotrack_infrared_test.conf";
  write_file(infrared_path,
             "sensor=infrared\n"
             "calibrated=false\n"
             "reliable_score=0.74\n");
  const ControllerConfig infrared = load_controller_config(infrared_path);
  expect_true(infrared.sensor == "infrared",
              "infrared config keeps a separate identity");
  expect_near(infrared.reliable_score, 0.74F, 1e-6F,
              "infrared override does not inherit visible values");

  const std::string unknown_path = "/tmp/nanotrack_unknown_test.conf";
  write_file(unknown_path, "sensor=visible\nunknown_key=1\n");
  expect_throws([&] { load_controller_config(unknown_path); },
                "unknown config keys must fail");
  const std::string duplicate_path = "/tmp/nanotrack_duplicate_test.conf";
  write_file(duplicate_path, "sensor=visible\nsensor=infrared\n");
  expect_throws([&] { load_controller_config(duplicate_path); },
                "duplicate config keys must fail");
  const std::string invalid_path = "/tmp/nanotrack_invalid_test.conf";
  write_file(invalid_path, "sensor=visible\nreliable_score=nan\n");
  expect_throws([&] { load_controller_config(invalid_path); },
                "non-finite config values must fail");
  const std::string invalid_detector_ratio_nan_path =
      "/tmp/nanotrack_invalid_detector_ratio_nan_test.conf";
  write_file(invalid_detector_ratio_nan_path,
             "sensor=visible\ndetector_candidate_max_center_ratio=nan\n");
  expect_throws(
      [&] { load_controller_config(invalid_detector_ratio_nan_path); },
      "non-finite detector center ratio must fail");
  const std::string invalid_detector_ratio_negative_path =
      "/tmp/nanotrack_invalid_detector_ratio_negative_test.conf";
  write_file(invalid_detector_ratio_negative_path,
             "sensor=visible\ndetector_candidate_max_center_ratio=-0.01\n");
  expect_throws(
      [&] { load_controller_config(invalid_detector_ratio_negative_path); },
      "negative detector center ratio must fail");
  const std::string invalid_detector_ratio_large_path =
      "/tmp/nanotrack_invalid_detector_ratio_large_test.conf";
  write_file(invalid_detector_ratio_large_path,
             "sensor=visible\ndetector_candidate_max_center_ratio=1.01\n");
  expect_throws(
      [&] { load_controller_config(invalid_detector_ratio_large_path); },
      "detector center ratio above one must fail");
  const std::string timing_path = "/tmp/nanotrack_timing_test.conf";
  write_file(timing_path,
             "sensor=visible\nuncertain_ms=1000\nlocal_recovery_ms=500\n");
  expect_throws([&] { load_controller_config(timing_path); },
                "non-increasing state timing must fail");
  const std::string invalid_mode_path =
      "/tmp/nanotrack_invalid_target_mode_test.conf";
  write_file(invalid_mode_path,
             "sensor=visible\ndetector_target_mode=automatic\n");
  expect_throws([&] { load_controller_config(invalid_mode_path); },
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

  const std::string occlusion_path = "/tmp/nanotrack_occlusion_test.conf";
  write_file(occlusion_path,
             "occlusion_hold_frames=24\n"
             "occlusion_trigger_consecutive_unreliable=4\n"
             "occlusion_trigger_min_dimension_ratio=0.55\n"
             "occlusion_reference_window=16\n");
  const ControllerConfig occlusion = load_controller_config(occlusion_path);
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
  };
  for (const CountLimit& limit : occlusion_counts) {
    for (int value : {limit.minimum, limit.maximum}) {
      write_file(occlusion_path,
                 std::string(limit.key) + "=" + std::to_string(value) + "\n");
      expect_true(load_controller_config(occlusion_path).*(limit.field) == value,
                  std::string(limit.key) + " accepts both inclusive limits");
    }
    for (int value : {limit.minimum - 1, limit.maximum + 1}) {
      ControllerConfig invalid = default_visible;
      invalid.*(limit.field) = value;
      expect_throws([&] { validate_controller_config(invalid); },
                    std::string(limit.key) + " rejects values outside limits");
      write_file(occlusion_path,
                 std::string(limit.key) + "=" + std::to_string(value) + "\n");
      expect_throws([&] { load_controller_config(occlusion_path); },
                    std::string(limit.key) + " rejects invalid file values");
    }
    for (const std::string& value : {"1.5", "2147483648"}) {
      write_file(occlusion_path, std::string(limit.key) + "=" + value + "\n");
      expect_throws([&] { load_controller_config(occlusion_path); },
                    std::string(limit.key) + " requires an in-range integer");
    }
  }
  for (float ratio : {0.01F, 1.0F}) {
    write_file(occlusion_path, "occlusion_trigger_min_dimension_ratio=" +
                                   std::to_string(ratio) + "\n");
    expect_near(load_controller_config(occlusion_path)
                    .occlusion_trigger_min_dimension_ratio,
                ratio, 1e-6F, "occlusion ratio accepts (0,1] values");
  }
  for (float ratio : {-0.01F, 0.0F, 1.01F,
                      std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity()}) {
    ControllerConfig invalid = default_visible;
    invalid.occlusion_trigger_min_dimension_ratio = ratio;
    expect_throws([&] { validate_controller_config(invalid); },
                  "occlusion ratio rejects out-of-range or non-finite values");
    write_file(occlusion_path, "occlusion_trigger_min_dimension_ratio=" +
                                   std::to_string(ratio) + "\n");
    expect_throws([&] { load_controller_config(occlusion_path); },
                  "occlusion ratio rejects invalid file values");
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
