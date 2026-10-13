#include "tracking_log.hpp"

#include <cmath>
#include <iostream>
#include <limits>
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

std::size_t csv_columns(const std::string& line) {
  std::size_t columns = 1;
  bool quoted = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    if (line[index] == '"') {
      if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
        ++index;
      } else {
        quoted = !quoted;
      }
    } else if (line[index] == ',' && !quoted) {
      ++columns;
    }
  }
  return columns;
}

}  // namespace

int main() {
  using namespace nanotrack;

  MonotonicVideoClock clock(30.0);
  expect_true(clock.next(10.0) == 10, "valid first PTS is preserved");
  expect_true(clock.next(50.0) == 50, "increasing PTS is preserved");
  expect_true(clock.next(50.0) == 83, "duplicate PTS advances by FPS step");
  expect_true(clock.next(20.0) == 116, "backward PTS advances monotonically");
  expect_true(!clock.fps_fallback(), "valid source FPS does not use fallback");

  MonotonicVideoClock fallback(0.0);
  expect_true(fallback.next(std::numeric_limits<double>::quiet_NaN()) == 0,
              "invalid initial PTS starts at zero");
  expect_true(fallback.next(std::numeric_limits<double>::quiet_NaN()) == 33,
              "invalid subsequent PTS uses 30 FPS step");
  expect_true(fallback.fps_fallback(), "invalid FPS is auditable as fallback");

  TrackingLogRow row;
  row.frame_id = 7;
  row.timestamp_ms = 233;
  row.mode = TrackingMode::kAssistedRecovery;
  row.quality = QualityLevel::kWeak;
  row.bbox = BBox{1, 2, 3, 4};
  row.search_scale = 4.0F;
  row.hypothesis_source = HypothesisSource::kDetector;
  row.detector_requested = true;
  row.detector_status = DetectorBatchStatus::kStale;
  row.pipeline_ms = 50.0F;
  row.anchor_metrics = ResponseMetrics{0.8F, 0.7F, 8.0F, 9.0F, 0.1F};
  row.anchor_present = true;
  row.note = "detector, timeout \"late\"";
  const std::string header = tracking_csv_header();
  const std::string encoded = tracking_csv_row(row);
  expect_true(csv_columns(header) == csv_columns(encoded),
              "CSV header and data rows have identical column counts");
  expect_true(header.find("control_valid") != std::string::npos &&
                  header.find("anchor_psr") != std::string::npos &&
                  header.find("recent_apce") != std::string::npos &&
                  header.find("tracker_ms") != std::string::npos &&
                  header.find("pipeline_ms") != std::string::npos,
              "CSV exposes control, raw metrics, and timing");
  expect_true(encoded.find("\"detector, timeout \"\"late\"\"\"") !=
                  std::string::npos,
              "CSV text fields escape commas and quotes");

  const TrackingLogRow initial = make_initial_log_row(0, 0, BBox{10, 20, 30, 40});
  expect_true(initial.control_valid && initial.accepted &&
                  initial.mode == TrackingMode::kTracking,
              "initialization row explicitly records valid tracking state");
  expect_true(csv_columns(tracking_csv_row(initial)) == csv_columns(header),
              "initialization and tracking rows share one schema");

  const LatencySummary latency =
      summarize_latencies({10.0F, 20.0F, 30.0F}, {40.0F, 50.0F, 60.0F});
  expect_true(std::fabs(latency.mean_runtime_ms - 20.0F) < 1e-6F &&
                  std::fabs(latency.runtime_fps - 50.0F) < 1e-6F,
              "runtime throughput is derived from runtime latency");
  expect_true(std::fabs(latency.mean_pipeline_ms - 50.0F) < 1e-6F &&
                  std::fabs(latency.end_to_end_fps - 20.0F) < 1e-6F,
              "end-to-end throughput is derived from pipeline latency");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "tracking log tests passed\n";
  return 0;
}
