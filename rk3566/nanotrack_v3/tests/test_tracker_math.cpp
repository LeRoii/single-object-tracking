#include "tracker_math.hpp"

#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void expect_near(float actual, float expected, float tolerance, const std::string& message) {
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

}  // namespace

int main() {
  using namespace nanotrack;

  const BBox parsed = parse_bbox("244,161,74,70");
  expect_near(parsed.x, 244.0F, 1e-6F, "bbox x");
  expect_near(parsed.y, 161.0F, 1e-6F, "bbox y");
  expect_near(parsed.width, 74.0F, 1e-6F, "bbox width");
  expect_near(parsed.height, 70.0F, 1e-6F, "bbox height");
  expect_throws([] { parse_bbox("1,2,3"); }, "three bbox fields must fail");
  expect_throws([] { parse_bbox("1,2,3,4,5"); }, "five bbox fields must fail");
  expect_throws([] { parse_bbox("1,2,nan,4"); }, "non-finite bbox must fail");

  validate_bbox(BBox{0, 0, 20, 10}, 100, 80);
  expect_throws([] { validate_bbox(BBox{0, 0, 0, 10}, 100, 80); },
                "zero width must fail");
  expect_throws([] { validate_bbox(BBox{90, 70, 20, 20}, 100, 80); },
                "out-of-frame bbox must fail");

  const std::vector<float> hann = make_hann_window(3);
  const std::vector<float> expected_hann{0, 0, 0, 0, 1, 0, 0, 0, 0};
  expect_true(hann == expected_hann, "3x3 Hann window literal values");
  expect_throws([] { make_hann_window(1); }, "Hann size below two must fail");

  const BBox clipped = clip_bbox(BBox{-5, 75, 200, 20}, 100, 80);
  expect_near(clipped.x, 0.0F, 1e-6F, "clipped x");
  expect_near(clipped.y, 75.0F, 1e-6F, "clipped y");
  expect_near(clipped.width, 100.0F, 1e-6F, "clipped width");
  expect_near(clipped.height, 5.0F, 1e-6F, "clipped height");

  const BBox stabilized = constrain_bbox_dimensions(
      BBox{720.0F, 380.0F, 109.0F, 1.0F},
      BBox{700.0F, 320.0F, 64.0F, 64.0F}, 0.75F, 1.33F, 1920, 1080);
  expect_near(stabilized.x + 0.5F * stabilized.width, 774.5F, 1e-5F,
              "dimension guard preserves candidate center x");
  expect_near(stabilized.y + 0.5F * stabilized.height, 380.5F, 1e-5F,
              "dimension guard preserves candidate center y");
  expect_near(stabilized.width, 85.12F, 1e-4F,
              "dimension guard caps cumulative width growth");
  expect_near(stabilized.height, 48.0F, 1e-4F,
              "dimension guard prevents cumulative height collapse");
  const BBox stabilized_at_edge = constrain_bbox_dimensions(
      BBox{0.0F, 0.0F, 20.0F, 20.0F},
      BBox{10.0F, 10.0F, 64.0F, 64.0F}, 0.75F, 1.33F, 100, 80);
  expect_near(stabilized_at_edge.x, 0.0F, 1e-6F,
              "dimension guard keeps resized box inside left edge");
  expect_near(stabilized_at_edge.y, 0.0F, 1e-6F,
              "dimension guard keeps resized box inside top edge");
  expect_near(stabilized_at_edge.width, 48.0F, 1e-6F,
              "dimension guard preserves bounded width at frame edge");
  expect_near(stabilized_at_edge.height, 48.0F, 1e-6F,
              "dimension guard preserves bounded height at frame edge");
  expect_throws(
      [] {
        constrain_bbox_dimensions(BBox{0, 0, 10, 10}, BBox{0, 0, 10, 10},
                                  1.1F, 1.0F, 100, 100);
      },
      "dimension guard rejects inverted scale bounds");

  BBox rolling{80.0F, 60.0F, 20.0F, 20.0F};
  for (float size : {23.0F, 26.0F, 29.0F, 32.0F}) {
    const BBox at_edge{100.0F - size, 80.0F - size, size, size};
    rolling = constrain_bbox_dimensions(at_edge, rolling, 0.75F, 1.33F, 100, 80);
    expect_near(rolling.x + rolling.width, 100.0F, 1e-6F,
                "rolling scale retains right-edge contact");
    expect_near(rolling.y + rolling.height, 80.0F, 1e-6F,
                "rolling scale retains lower-edge contact");
  }
  expect_near(rolling.width, 32.0F, 1e-6F,
              "rolling growth passes the original-size ceiling");
  const auto anti_collapse = constrain_bbox_dimensions(
      BBox{99, 79, 1, 1}, rolling, 0.75F, 1.33F, 100, 80);
  expect_near(anti_collapse.width, 24.0F, 1e-6F,
              "the collapse floor follows the last committed width");
  expect_near(anti_collapse.x + anti_collapse.width, 100.0F, 1e-6F,
              "the collapsed observation stays clipped to the frame");

  expect_near(percentile_ms({1, 2, 3, 4}, 0.5F), 2.5F, 1e-6F, "p50");
  expect_near(percentile_ms({1, 2, 3, 4}, 0.95F), 3.85F, 1e-5F, "p95");
  expect_throws([] { percentile_ms({}, 0.5F); }, "empty percentile must fail");

  const std::vector<float> nhwc =
      nchw_to_nhwc({1, 2, 3, 4, 5, 6, 7, 8}, 2, 2, 2);
  expect_true(nhwc == std::vector<float>({1, 5, 2, 6, 3, 7, 4, 8}),
              "NCHW feature converts to literal NHWC order");
  expect_throws([] { nchw_to_nhwc({1, 2, 3}, 2, 2, 2); },
                "layout conversion rejects wrong element count");

  TrackerConfig config;
  config.output_size = 1;
  config.stride = 16;
  config.penalty_k = 0.0F;
  config.window_influence = 0.0F;
  config.learning_rate = 1.0F;
  const DecodeResult decoded = decode_head(
      {0.0F, 2.0F}, {4.0F, 3.0F, 6.0F, 5.0F},
      TrackerState{50.0F, 50.0F, 20.0F, 10.0F}, config, 1.0F, 100, 100);
  expect_near(decoded.score, 0.880797F, 1e-5F, "softmax foreground score");
  expect_near(decoded.state.center_x, 51.0F, 1e-5F, "decoded center x");
  expect_near(decoded.state.center_y, 51.0F, 1e-5F, "decoded center y");
  expect_near(decoded.state.width, 11.19203F, 1e-4F, "smoothed width");
  expect_near(decoded.state.height, 8.238406F, 1e-4F, "smoothed height");
  expect_near(decoded.metrics.raw_score, 0.880797F, 1e-5F,
              "single-cell raw response score");
  expect_true(std::isfinite(decoded.metrics.psr),
              "single-cell PSR has a finite convention");

  TrackerConfig diagnostic_config;
  diagnostic_config.output_size = 5;
  diagnostic_config.stride = 16;
  diagnostic_config.penalty_k = 0.0F;
  diagnostic_config.window_influence = 0.0F;
  diagnostic_config.learning_rate = 0.0F;
  std::vector<float> diagnostic_cls(50, 0.0F);
  const auto set_probability = [&](std::size_t index, float probability) {
    diagnostic_cls[25 + index] = std::log(probability / (1.0F - probability));
  };
  for (std::size_t index = 0; index < 25; ++index) set_probability(index, 0.1F);
  set_probability(0, 0.5F);
  set_probability(12, 0.9F);
  std::vector<float> diagnostic_loc(100, 5.0F);
  const DecodeResult diagnostic = decode_head(
      diagnostic_cls, diagnostic_loc,
      TrackerState{100.0F, 100.0F, 10.0F, 10.0F}, diagnostic_config,
      1.0F, 200, 200);
  expect_near(diagnostic.metrics.raw_score, 0.9F, 1e-5F,
              "diagnostic raw peak");
  expect_near(diagnostic.metrics.penalized_peak, 0.9F, 1e-5F,
              "diagnostic penalized peak");
  expect_near(diagnostic.metrics.peak_margin, 0.4F, 1e-5F,
              "diagnostic non-neighbour peak margin");
  expect_near(diagnostic.metrics.psr, 8.004166F, 1e-4F,
              "diagnostic peak-to-sidelobe ratio");
  expect_near(diagnostic.metrics.apce, 20.0F, 1e-4F,
              "diagnostic average peak-to-correlation energy");

  for (std::size_t index = 0; index < 25; ++index) set_probability(index, 0.1F);
  set_probability(12, 0.9F);
  const DecodeResult flat_sidelobes = decode_head(
      diagnostic_cls, diagnostic_loc,
      TrackerState{100.0F, 100.0F, 10.0F, 10.0F}, diagnostic_config,
      1.0F, 200, 200);
  expect_true(std::isfinite(flat_sidelobes.metrics.psr) &&
                  flat_sidelobes.metrics.psr > 0.0F,
              "zero-variance sidelobes produce a finite positive PSR");
  expect_true(std::isfinite(flat_sidelobes.metrics.apce),
              "zero-variance sidelobes produce a finite APCE");
  expect_throws(
      [&] {
        decode_head({0.0F, NAN}, {4, 3, 6, 5},
                    TrackerState{50, 50, 20, 10}, config, 1.0F, 100, 100);
      },
      "non-finite head output must fail");

  const std::optional<DecodeResult> outside = try_decode_head(
      {0.0F, 2.0F}, {-1000.0F, 0.0F, 1100.0F, 10.0F},
      TrackerState{50.0F, 50.0F, 20.0F, 10.0F}, config, 1.0F, 100, 100);
  expect_true(!outside,
              "a finite head proposal wholly outside the frame becomes no observation");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "tracker_math tests passed\n";
  return 0;
}
