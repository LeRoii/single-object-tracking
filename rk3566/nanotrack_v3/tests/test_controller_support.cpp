#include "controller_config.hpp"
#include "detector_adapter.hpp"
#include "motion_estimator.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
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

}  // namespace

int main() {
  using namespace nanotrack;

  const ControllerConfig default_visible = default_controller_config("visible");
  expect_near(default_visible.reliable_psr, 6.0F, 1e-6F,
              "visible bring-up default accepts observed RKNN PSR range");
  expect_near(default_visible.reliable_peak_margin, 0.06F, 1e-6F,
              "visible bring-up default accepts observed RKNN peak margin");
  const ControllerConfig default_infrared =
      default_controller_config("infrared");
  expect_near(default_infrared.reliable_psr, 8.0F, 1e-6F,
              "visible evidence does not alter infrared PSR default");
  expect_near(default_infrared.reliable_peak_margin, 0.10F, 1e-6F,
              "visible evidence does not alter infrared peak-margin default");

  const std::string visible_path = "/tmp/nanotrack_visible_test.conf";
  write_file(visible_path,
             "sensor=visible\n"
             "calibrated=false\n"
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
  expect_true(visible.sensor == "visible" && !visible.calibrated,
              "visible config identity and calibration marker");
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
  const std::string timing_path = "/tmp/nanotrack_timing_test.conf";
  write_file(timing_path,
             "sensor=visible\nuncertain_ms=1000\nlocal_recovery_ms=500\n");
  expect_throws([&] { load_controller_config(timing_path); },
                "non-increasing state timing must fail");

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
