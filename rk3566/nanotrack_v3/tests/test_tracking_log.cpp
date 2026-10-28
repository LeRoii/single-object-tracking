#include "tracking_log.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
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

bool contains(const std::string& value, const std::string& expected) {
  return value.find(expected) != std::string::npos;
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
  row.motion_predicted_bbox = BBox{5, 6, 7, 8};
  row.detector_configured = true;
  row.detector_ready = false;
  row.detector_requested = true;
  row.detector_request_frame_id = 7;
  row.detector_worker_status = "error";
  row.detector_error = "worker, stopped\nretry \"disabled\"";
  row.detector_batch_frame_id = 6;
  row.detector_batch_age_ms = 73;
  row.detector_raw_candidate_count = 11;
  row.detector_filtered_candidate_count = 3;
  row.detector_verified_class_id = 2;
  row.detector_verified_confidence = 0.88F;
  row.detector_candidate_center_ratio = 0.12F;
  row.detector_target_mode = DetectorTargetMode::kSubtarget;
  row.detector_candidate_id = 42;
  row.detector_candidate_region = BBox{10, 20, 80, 100};
  row.detector_same_class_candidates = 2;
  row.detector_same_class_ambiguous = true;
  row.detector_candidate_locked = true;
  row.detector_probe_index = 3;
  row.detector_confirmation_attempts = 2;
  row.detector_confirmation_hits = 1;
  row.detector_timing = DetectorTiming{1.0F, 7.0F, 2.0F, 10.0F};
  row.detector_status = DetectorBatchStatus::kStale;
  row.detector_position_valid_count = 2;
  row.detector_position_rejected_count = 4;
  row.occlusion_hold_triggered = true;
  row.occlusion_hold_remaining_frames = 12;
  row.occlusion_reference_width = 80.0F;
  row.occlusion_reference_height = 100.0F;
  row.occlusion_width_ratio = 0.65F;
  row.occlusion_height_ratio = 0.90F;
  row.tracker_skipped = true;
  row.detector_suppressed_by_hold = true;
  row.pipeline_ms = 50.0F;
  row.anchor_metrics = ResponseMetrics{0.8F, 0.7F, 8.0F, 9.0F, 0.1F};
  row.anchor_present = true;
  row.note = "detector, timeout \"late\"";
  const std::string header = tracking_csv_header();
  const std::string encoded = tracking_csv_row(row);
  expect_true(csv_columns(header) == csv_columns(encoded),
              "CSV header and data rows have identical column counts");
  expect_true(contains(header, "schema_version") &&
                  contains(header, "control_valid") &&
                  contains(header, "anchor_psr") &&
                  contains(header, "recent_apce") &&
                  contains(header, "tracker_ms") &&
                  contains(header, "pipeline_ms"),
              "CSV exposes schema, control, raw metrics, and timing");
  for (const std::string& column :
       {"detector_configured", "detector_ready", "detector_requested",
        "detector_request_frame_id", "detector_worker_status",
        "detector_error", "detector_batch_frame_id", "detector_batch_age_ms",
        "detector_raw_candidate_count", "detector_filtered_candidate_count",
        "detector_verified_class_id", "detector_verified_confidence",
        "detector_target_mode", "detector_candidate_id",
        "detector_region_x", "detector_region_y", "detector_region_width",
        "detector_region_height", "detector_same_class_candidates",
        "detector_same_class_ambiguous", "detector_candidate_locked",
        "detector_probe_index", "detector_confirmation_attempts",
        "detector_confirmation_hits",
        "detector_preprocess_ms", "detector_inference_ms",
        "detector_postprocess_ms", "detector_total_ms",
        "motion_predicted_x", "motion_predicted_y",
        "motion_predicted_width", "motion_predicted_height",
        "detector_candidate_center_ratio",
        "detector_position_valid_count",
        "detector_position_rejected_count",
        "occlusion_hold_triggered", "occlusion_hold_remaining_frames",
        "occlusion_reference_width", "occlusion_reference_height",
        "occlusion_width_ratio", "occlusion_height_ratio",
        "tracker_skipped", "detector_suppressed_by_hold"}) {
    expect_true(contains(header, column), "CSV has exact detector column " + column);
  }
  expect_true(encoded.rfind("7,", 0) == 0,
              "tracking rows carry schema version 7 exactly once");
  expect_true(contains(encoded, "\"worker, stopped\nretry \"\"disabled\"\"\""),
              "detector errors escape commas, newlines, and quotes");
  expect_true(encoded.find("\"detector, timeout \"\"late\"\"\"") !=
                  std::string::npos,
              "CSV text fields escape commas and quotes");

  const TrackingLogRow initial = make_initial_log_row(0, 0, BBox{10, 20, 30, 40});
  expect_true(initial.control_valid && initial.accepted &&
                  initial.mode == TrackingMode::kTracking,
              "initialization row explicitly records valid tracking state");
  expect_true(csv_columns(tracking_csv_row(initial)) == csv_columns(header),
              "initialization and tracking rows share one schema");
  const TrackingLogRow detector_initial =
      make_initial_log_row(0, 0, BBox{10, 20, 30, 40}, true, true,
                           DetectorTargetMode::kSubtarget);
  expect_true(detector_initial.detector_configured &&
                  detector_initial.detector_ready &&
                  detector_initial.detector_target_mode ==
                      DetectorTargetMode::kSubtarget &&
                  detector_initial.detector_worker_status == "ready",
              "initialization row records detector readiness and target mode");

  const LatencySummary latency =
      summarize_latencies({10.0F, 20.0F, 30.0F}, {40.0F, 50.0F, 60.0F});
  expect_true(std::fabs(latency.mean_runtime_ms - 20.0F) < 1e-6F &&
                  std::fabs(latency.runtime_fps - 50.0F) < 1e-6F,
              "runtime throughput is derived from runtime latency");
  expect_true(std::fabs(latency.mean_pipeline_ms - 50.0F) < 1e-6F &&
                  std::fabs(latency.end_to_end_fps - 20.0F) < 1e-6F,
              "end-to-end throughput is derived from pipeline latency");

  RuntimeFrameResult runtime;
  runtime.plan.frame_id = 12;
  runtime.plan.timestamp_ms = 440;
  runtime.plan.request_detector = true;
  runtime.plan.motion_predicted_bbox = BBox{11, 22, 33, 44};
  runtime.plan.detector_candidate_class_id = 2;
  runtime.plan.detector_candidate_confidence = 0.91F;
  runtime.plan.detector_candidate_center_ratio = 0.125F;
  runtime.plan.detector_target_mode = DetectorTargetMode::kSubtarget;
  runtime.plan.detector_candidate_id = 88;
  runtime.plan.detector_candidate_region = BBox{30, 40, 100, 80};
  runtime.plan.detector_same_class_candidates = 2;
  runtime.plan.detector_same_class_ambiguous = true;
  runtime.plan.detector_candidate_locked = true;
  runtime.plan.detector_probe_index = 4;
  runtime.output.frame_id = 12;
  runtime.output.timestamp_ms = 440;
  runtime.output.detector_status = DetectorBatchStatus::kAccepted;
  runtime.output.detector_candidate_id = 88;
  runtime.output.detector_same_class_candidates = 2;
  runtime.output.detector_same_class_ambiguous = true;
  runtime.output.detector_candidate_locked = true;
  runtime.output.detector_probe_index = 4;
  runtime.output.detector_confirmation_attempts = 3;
  runtime.output.detector_confirmation_hits = 2;
  runtime.output.detector_position_valid_count = 2;
  runtime.output.detector_position_rejected_count = 1;
  runtime.output.occlusion_hold_triggered = true;
  runtime.output.occlusion_hold_remaining_frames = 6;
  runtime.output.occlusion_reference_width = 33.0F;
  runtime.output.occlusion_reference_height = 44.0F;
  runtime.output.occlusion_width_ratio = 0.65F;
  runtime.output.occlusion_height_ratio = 0.90F;
  runtime.tracker_skipped = true;
  runtime.tracker_ms = 0.0F;
  runtime.plan.mode = TrackingMode::kOcclusionHold;
  runtime.detector.configured = true;
  runtime.detector.ready = true;
  runtime.detector.request_submitted = true;
  runtime.detector.request_frame_id = 12;
  runtime.detector.batch_received = true;
  runtime.detector.batch_frame_id = 10;
  runtime.detector.batch_age_ms = 67;
  runtime.detector.raw_candidate_count = 9;
  runtime.detector.filtered_candidate_count = 3;
  runtime.detector.timing = DetectorTiming{1.0F, 4.0F, 2.0F, 7.0F};
  const TrackingLogRow mapped = make_tracking_log_row(runtime);
  expect_true(mapped.detector_configured && mapped.detector_ready &&
                  mapped.detector_requested &&
                  mapped.detector_request_frame_id == std::optional<std::uint64_t>(12) &&
                  mapped.detector_worker_status == "completed" &&
                  mapped.detector_batch_frame_id == std::optional<std::uint64_t>(10) &&
                  mapped.detector_batch_age_ms == std::optional<std::int64_t>(67) &&
                  mapped.detector_verified_class_id == std::optional<int>(2) &&
                  mapped.detector_verified_confidence == std::optional<float>(0.91F) &&
                  mapped.detector_target_mode == DetectorTargetMode::kSubtarget &&
                  mapped.detector_candidate_id ==
                      std::optional<std::uint64_t>(88) &&
                  mapped.detector_candidate_region.has_value() &&
                  mapped.detector_same_class_candidates == 2 &&
                  mapped.detector_same_class_ambiguous &&
                  mapped.detector_candidate_locked &&
                  mapped.detector_probe_index == 4 &&
                  mapped.detector_confirmation_attempts == 3 &&
                  mapped.detector_confirmation_hits == 2 &&
                  mapped.motion_predicted_bbox.x == 11.0F &&
                  mapped.motion_predicted_bbox.y == 22.0F &&
                  mapped.motion_predicted_bbox.width == 33.0F &&
                  mapped.motion_predicted_bbox.height == 44.0F &&
                  mapped.detector_candidate_center_ratio ==
                      std::optional<float>(0.125F) &&
                  mapped.detector_position_valid_count == 2 &&
                  mapped.detector_position_rejected_count == 1 &&
                  mapped.occlusion_hold_triggered &&
                  mapped.occlusion_hold_remaining_frames == 6 &&
                  mapped.occlusion_reference_width == std::optional<float>(33.0F) &&
                  mapped.occlusion_reference_height == std::optional<float>(44.0F) &&
                  mapped.occlusion_width_ratio == std::optional<float>(0.65F) &&
                  mapped.occlusion_height_ratio == std::optional<float>(0.90F) &&
                  mapped.tracker_skipped && mapped.detector_suppressed_by_hold &&
                  mapped.detector_timing.total_ms == 7.0F,
              "runtime detector diagnostics and verified candidate are mapped");

  TrackingSessionMetadata metadata;
  metadata.video = "input,video.mp4";
  metadata.model_dir = "models";
  metadata.manifest_hash_algorithm = "fnv1a64";
  metadata.manifest_hash = "nano-hash";
  metadata.controller_config = "builtin-defaults";
  metadata.controller_config_hash_algorithm = "fnv1a64";
  metadata.controller_config_hash = "controller-hash";
  metadata.sensor = "visible";
  metadata.detector_target_mode = DetectorTargetMode::kSubtarget;
  metadata.detector_candidate_max_center_ratio = 0.15F;
  metadata.occlusion_hold_frames = 12;
  metadata.occlusion_trigger_consecutive_unreliable = 2;
  metadata.occlusion_trigger_min_dimension_ratio = 0.70F;
  metadata.occlusion_reference_window = 8;
  metadata.source_fps = 30.0;
  metadata.frame_step_ms = 33;
  metadata.tracked_frames = 18;
  metadata.detector.enabled = true;
  metadata.detector.calibrated = false;
  metadata.detector.model_path = "models/yolov8.rknn";
  metadata.detector.model_sha256 = "model-sha256";
  metadata.detector.manifest_path = "models/detector-manifest.json";
  metadata.detector.manifest_sha256 = "manifest-sha256";
  metadata.detector.labels_path = "models/coco80.txt";
  metadata.detector.labels_sha256 = "labels-sha256";
  metadata.detector.class_id = 2;
  metadata.detector.confidence_threshold = 0.25F;
  metadata.detector.nms_threshold = 0.45F;
  metadata.detector.input_tensor_description = "images,1x640x640x3,UINT8,NHWC";
  metadata.detector.output_tensor_descriptions = {
      "box_s8,1x64x80x80,INT8,NCHW", "cls_s8,1x80x80x80,INT8,NCHW"};
  metadata.detector.rknn_api_version = "2.3.2\napi";
  metadata.detector.rknn_driver_version = "2.3.2\"driver";
  metadata.detector.request_count = 8;
  metadata.detector.complete_count = 7;
  metadata.detector.reject_count = 2;
  metadata.detector.error_count = 1;
  const std::string json = tracking_session_metadata_json(metadata);
  expect_true(contains(json, "\"schema_version\": 7") &&
                  contains(json, "\"detector_target_mode\": \"SUBTARGET\"") &&
                  contains(json,
                           "\"detector_candidate_max_center_ratio\": 0.150000") &&
                  contains(json, "\"occlusion_hold_frames\": 12") &&
                  contains(json,
                           "\"occlusion_trigger_consecutive_unreliable\": 2") &&
                  contains(json,
                           "\"occlusion_trigger_min_dimension_ratio\": 0.700000") &&
                  contains(json, "\"occlusion_reference_window\": 8") &&
                  contains(json, "\"enabled\": true") &&
                  contains(json, "\"calibrated\": false") &&
                  contains(json, "\"model_path\": \"models/yolov8.rknn\"") &&
                  contains(json, "\"model_sha256\": \"model-sha256\"") &&
                  contains(json, "\"manifest_sha256\": \"manifest-sha256\"") &&
                  contains(json, "\"labels_sha256\": \"labels-sha256\"") &&
                  contains(json, "\"class_id\": 2") &&
                  contains(json, "\"confidence_threshold\": 0.250000") &&
                  contains(json, "\"nms_threshold\": 0.450000") &&
                  contains(json, "\"input_tensor_description\"") &&
                  contains(json, "\"output_tensor_descriptions\"") &&
                  contains(json, "\"rknn_api_version\": \"2.3.2\\napi\"") &&
                  contains(json, "\"rknn_driver_version\": \"2.3.2\\\"driver\"") &&
                  contains(json, "\"request_count\": 8") &&
                  contains(json, "\"complete_count\": 7") &&
                  contains(json, "\"reject_count\": 2") &&
                  contains(json, "\"error_count\": 1"),
              "schema-v7 metadata exposes hold, position gate, detector identity, tensors, versions, and counters");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "tracking log tests passed\n";
  return 0;
}
