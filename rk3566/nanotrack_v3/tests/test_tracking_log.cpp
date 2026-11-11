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

void test_adaptive_coast_serialization() {
  using namespace nanotrack;
  RuntimeFrameResult result;
  auto& o = result.output;
  o.occlusion_reference_source = OcclusionReferenceSource::kPartialStableMedian;
  o.occlusion_suspicion_active = true;
  o.occlusion_suspicion_started = true;
  o.occlusion_suspicion_frames = 3;
  o.occlusion_clean_frames = 1;
  o.occlusion_width_shrink_frames = 2;
  o.occlusion_height_shrink_frames = 4;
  o.occlusion_severe_evidence = true;
  o.occlusion_gradual_evidence = true;
  o.occlusion_template_evidence = true;
  o.occlusion_peak_margin_evidence = true;
  o.occlusion_motion_residual_evidence = true;
  o.occlusion_trigger_reason = OcclusionTriggerReason::kGradualWidthPeak;
  o.occlusion_coast_age_frames = 5;
  o.occlusion_probe_type = OcclusionProbeType::kSearch;
  o.occlusion_probe_confirmation_hits = 1;
  o.occlusion_probe_confirmation_required = 2;
  o.motion_control_authorized = true;
  o.occlusion_probe_rejection = "CONFIRM_GEOMETRY";
  o.occlusion_probe_cross_iou = false;
  o.occlusion_probe_cross_center = true;
  o.occlusion_probe_bbox = BBox{1, 2, 3, 4};
  const auto encoded = tracking_csv_row(make_tracking_log_row(result));
  expect_true(contains(encoded,
      "PARTIAL_STABLE_MEDIAN,1,1,0,3,1,2,4,1,1,1,1,1,GRADUAL_WIDTH_PEAK,5,SEARCH,1,2,1"),
      "all adaptive diagnostics are copied from output and serialized in schema order");
  expect_true(contains(encoded, "CONFIRM_GEOMETRY,,,,,1.000000,2.000000,3.000000,4.000000") &&
                  contains(tracking_csv_header(), "occlusion_probe_cross_iou"),
              "probe reason, box, and gate columns survive CSV serialization");
  for (const auto age : {1, 5, 6, 10, 11, 12, 0}) {
    o.occlusion_coast_age_frames = age;
    o.occlusion_probe_type = age == 5 || age == 10 ? OcclusionProbeType::kSearch :
        age == 6 || age == 11 ? OcclusionProbeType::kConfirm : OcclusionProbeType::kNone;
    const auto row = make_tracking_log_row(result);
    expect_true(row.occlusion_coast_age_frames == static_cast<std::size_t>(age) &&
                    row.occlusion_probe_type == o.occlusion_probe_type,
                "presentation preserves trigger/probe/recovery/timeout ages without scheduling");
  }
  o.occlusion_suspicion_cleared = true;
  o.occlusion_reference_source = OcclusionReferenceSource::kFullStableMedian;
  expect_true(make_tracking_log_row(result).occlusion_suspicion_cleared &&
                  contains(tracking_csv_row(make_tracking_log_row(result)), "FULL_STABLE_MEDIAN"),
              "clear pulse and full reference source survive serialization");
  TrackingSessionMetadata metadata;
  ControllerConfig config;
  config.occlusion_probe_interval_frames = 7;
  config.occlusion_gradual_dimension_ratio = 0.81F;
  config.occlusion_confirmation_iou = 0.61F;
  config.max_accepted_scale_step_ratio = 0.05F;
  config.accepted_scale_deadband_ratio = 0.02F;
  config.accepted_scale_smoothing_alpha = 0.10F;
  config.accepted_scale_median_window_frames = 5;
  config.accepted_scale_direction_confirm_frames = 3;
  config.accepted_scale_output_hysteresis_px = 0.75F;
  config.accepted_center_smoothing_alpha = 0.35F;
  config.accepted_center_max_correction_ratio = 0.25F;
  config.max_reacquisition_center_distance = 0.75F;
  apply_controller_config_metadata(metadata, config);
  auto json = tracking_session_metadata_json(metadata);
  expect_true(contains(json, "\"occlusion_probe_interval_frames\": 7") &&
                  contains(json, "\"occlusion_gradual_dimension_ratio\": 0.810000") &&
                  contains(json, "\"occlusion_confirmation_iou\": 0.610000") &&
                  metadata.max_accepted_scale_step_ratio == 0.05F &&
                  metadata.accepted_scale_deadband_ratio == 0.02F &&
                  metadata.accepted_scale_smoothing_alpha == 0.10F &&
                  metadata.accepted_scale_median_window_frames == 5 &&
                  metadata.accepted_scale_direction_confirm_frames == 3 &&
                  metadata.accepted_scale_output_hysteresis_px == 0.75F &&
                  metadata.accepted_center_smoothing_alpha == 0.35F &&
                  metadata.accepted_center_max_correction_ratio == 0.25F &&
                  metadata.max_reacquisition_center_distance == 0.75F &&
                  contains(json, "\"max_accepted_scale_step_ratio\": 0.050000") &&
                  contains(json, "\"accepted_scale_deadband_ratio\": 0.020000") &&
                  contains(json, "\"accepted_scale_smoothing_alpha\": 0.100000") &&
                  contains(json, "\"accepted_scale_median_window_frames\": 5") &&
                  contains(json, "\"accepted_scale_direction_confirm_frames\": 3") &&
                  contains(json, "\"accepted_scale_output_hysteresis_px\": 0.750000") &&
                  contains(json, "\"accepted_center_smoothing_alpha\": 0.350000") &&
                  contains(json, "\"accepted_center_max_correction_ratio\": 0.250000") &&
                  contains(json, "\"max_reacquisition_center_distance\": 0.750000"),
              "metadata uses effective configuration instead of schema defaults");
  struct NonfiniteSetting {
    float TrackingSessionMetadata::*field;
    float value;
    const char* message;
  };
  const NonfiniteSetting nonfinite_settings[] = {
      {&TrackingSessionMetadata::accepted_scale_deadband_ratio,
       std::numeric_limits<float>::quiet_NaN(),
       "nonfinite accepted scale deadband is rejected before JSON output"},
      {&TrackingSessionMetadata::accepted_scale_smoothing_alpha,
       std::numeric_limits<float>::infinity(),
       "nonfinite accepted scale smoothing alpha is rejected before JSON output"},
      {&TrackingSessionMetadata::accepted_scale_output_hysteresis_px,
       std::numeric_limits<float>::quiet_NaN(),
       "nonfinite accepted scale output hysteresis is rejected before JSON output"},
      {&TrackingSessionMetadata::accepted_center_smoothing_alpha,
       std::numeric_limits<float>::quiet_NaN(),
       "nonfinite accepted center smoothing alpha is rejected before JSON output"},
      {&TrackingSessionMetadata::accepted_center_max_correction_ratio,
       std::numeric_limits<float>::infinity(),
       "nonfinite accepted center correction ratio is rejected before JSON output"},
      {&TrackingSessionMetadata::max_reacquisition_center_distance,
       -std::numeric_limits<float>::infinity(),
       "nonfinite recovery center distance is rejected before JSON output"},
  };
  for (const NonfiniteSetting& setting : nonfinite_settings) {
    TrackingSessionMetadata invalid = metadata;
    invalid.*(setting.field) = setting.value;
    bool rejected = false;
    try { tracking_session_metadata_json(invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect_true(rejected, setting.message);
  }
  metadata.occlusion_peak_margin_drop = std::numeric_limits<float>::infinity();
  bool rejected = false;
  try { tracking_session_metadata_json(metadata); }
  catch (const std::invalid_argument&) { rejected = true; }
  expect_true(rejected, "nonfinite adaptive configuration is rejected before JSON output");
}

}  // namespace

int main() {
  using namespace nanotrack;
  test_adaptive_coast_serialization();

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
  row.tracking_phase = TrackingPhase::kSearching;
  row.observation_valid = false;
  row.control_source = ControlSource::kMotionSearch;
  row.quality = QualityLevel::kWeak;
  row.control_valid = true;
  row.motion_control_authorized = true;
  row.bbox = BBox{1, 2, 3, 4};
  row.tracking_bbox = BBox{5, 6, 7, 8};
  row.display_bbox = BBox{1, 2, 3, 4};
  row.control_bbox = BBox{5, 6, 7, 8};
  row.motion_search_age_ms = 120;
  row.motion_prediction_confident = true;
  row.motion_velocity_x = 12.5F;
  row.motion_acceleration_y = -3.5F;
  row.search_hint = SearchHint{9, 6, 200, BBox{10, 20, 30, 40}, 4, 7, 1,
                               true, SearchHintRejection::kNone};
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
  row.target_class.status = TargetClassStatus::kEstablished;
  row.target_class.target_class_id = 2;
  row.target_class.reason = TargetClassReason::kUniqueMatch;
  row.target_class.target_class_allows_recovery = true;
  row.anchor_metrics = ResponseMetrics{0.8F, 0.7F, 8.0F, 9.0F, 0.1F};
  row.anchor_present = true;
  row.note = "detector, timeout \"late\"";
  const std::string header = tracking_csv_header();
  const std::string encoded = tracking_csv_row(row);
  expect_true(csv_columns(header) == csv_columns(encoded),
              "CSV header and data rows have identical column counts");
  expect_true(contains(header, "schema_version") &&
                  contains(header, "tracking_phase") &&
                  contains(header, "observation_valid") &&
                  contains(header, "control_source") &&
                  contains(header, "tracking_x") &&
                  contains(header, "control_x") &&
                  contains(header, "motion_search_age_ms") &&
                  contains(header, "motion_velocity_x") &&
                  contains(header, "search_hint_candidate_id") &&
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
        "tracker_skipped", "detector_suppressed_by_hold",
        "target_class_status", "target_class_id", "target_class_reason",
        "detector_recovery_enabled"}) {
    expect_true(contains(header, column), "CSV has exact detector column " + column);
  }
  expect_true(encoded.rfind("10,", 0) == 0 && kTrackingSchemaVersion == 10,
              "tracking rows carry schema version 10 exactly once");
  expect_true(contains(encoded, "ESTABLISHED,2,UNIQUE_MATCH,1"),
              "CSV writes established class and recovery gate");
  expect_true(contains(encoded, "\"worker, stopped\nretry \"\"disabled\"\"\""),
              "detector errors escape commas, newlines, and quotes");
  expect_true(encoded.find("\"detector, timeout \"\"late\"\"\"") !=
                  std::string::npos,
              "CSV text fields escape commas and quotes");

  const TrackingLogRow initial = make_initial_log_row(0, 0, BBox{10, 20, 30, 40});
  expect_true(initial.control_valid && initial.accepted &&
                  initial.mode == TrackingMode::kTracking &&
                  initial.tracking_phase == TrackingPhase::kTracking &&
                  initial.observation_valid &&
                  initial.control_source == ControlSource::kObservation,
              "initialization row explicitly records valid tracking state");
  expect_true(csv_columns(tracking_csv_row(initial)) == csv_columns(header),
              "initialization and tracking rows share one schema");
  expect_true(contains(tracking_csv_row(initial), "UNKNOWN,,NO_MATCH,0"),
              "default initial class is safely unknown with empty optional ID");
  TargetClassState fixed;
  fixed.status = TargetClassStatus::kFixed;
  fixed.reason = TargetClassReason::kFixedOverride;
  fixed.target_class_id = 4;
  fixed.target_class_allows_recovery = true;
  const TrackingLogRow fixed_initial = make_initial_log_row(
      0, 0, BBox{10, 20, 30, 40}, true, true,
      DetectorTargetMode::kSubtarget, fixed);
  expect_true(contains(tracking_csv_row(fixed_initial),
                       "FIXED,4,FIXED_OVERRIDE,1"),
              "initial row serializes caller-provided class state");
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
  runtime.target_class = row.target_class;
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
  expect_true(mapped.target_class.target_class_id == std::optional<int>(2) &&
                  contains(tracking_csv_row(mapped),
                           "ESTABLISHED,2,UNIQUE_MATCH,1"),
              "ordinary frame row preserves runtime class state");

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
  metadata.requested_target_class_id = -1;
  metadata.engine_class_filter = -1;
  metadata.target_class_match_config = TargetClassMatchConfig{0.6F, 0.7F, 0.2F};
  TargetClassInitializationResult first;
  first.initialization_index = 1;
  first.frame_id = 42;
  first.timestamp_ms = 1386;
  first.original_roi = BBox{10, 20, 30, 40};
  first.requested_class_id = -1;
  first.request_id = 91;
  first.batch = DetectorBatch{};
  first.batch->request_id = 91;
  first.batch->source_frame_id = 42;
  first.batch->raw_candidate_count = 2;
  first.batch->filtered_candidate_count = 1;
  first.detector_ms = 15.0F;
  first.detector_timing = DetectorTiming{1.0F, 10.0F, 2.0F, 13.0F};
  first.target_class = row.target_class;
  first.target_class.best_class_id = 2;
  first.target_class.best_score = 0.9F;
  first.target_class.runner_up_class_id = 3;
  first.target_class.runner_up_score = 0.5F;
  TargetClassCandidateMatch match;
  match.original_index = 4;
  match.candidate = DetectorCandidate{BBox{8, 18, 35, 44}, 0.8F, 2};
  match.clipped_bbox = match.candidate.bbox;
  match.roi_coverage = 0.9F;
  match.iou = 0.7F;
  match.center_ratio = 0.1F;
  match.score = 0.9F;
  match.eligible = true;
  first.target_class.selected_candidate = match;
  first.target_class.candidates.push_back(match);
  TargetClassCandidateMatch rejected = match;
  rejected.original_index = 5;
  rejected.candidate.confidence = std::numeric_limits<float>::quiet_NaN();
  rejected.eligible = false;
  rejected.rejection = TargetClassCandidateRejection::kInvalidConfidence;
  first.target_class.candidates.push_back(rejected);
  record_target_class_initialization(metadata, first);
  TargetClassInitializationResult second;
  second.initialization_index = 2;
  second.frame_id = 42;
  second.timestamp_ms = 1386;
  second.original_roi = BBox{20, 30, 40, 50};
  second.requested_class_id = -1;
  second.request_id = 92;
  second.detector_error = "worker, stopped\nretry \"disabled\"";
  second.detector_ms = 5.0F;
  second.target_class.status = TargetClassStatus::kUnknown;
  second.target_class.reason = TargetClassReason::kDetectorError;
  record_target_class_initialization(metadata, second);
  TargetClassInitializationResult unavailable;
  unavailable.initialization_index = 3;
  unavailable.frame_id = 42;
  unavailable.target_class.reason = TargetClassReason::kDetectorUnavailable;
  unavailable.detector_ms = 9.0F;
  record_target_class_initialization(metadata, unavailable);
  TargetClassInitializationResult error_without_id;
  error_without_id.initialization_index = 4;
  error_without_id.frame_id = 42;
  error_without_id.detector_error = "blocking call failed before request ID";
  error_without_id.detector_ms = 4.0F;
  error_without_id.target_class.reason = TargetClassReason::kDetectorError;
  record_target_class_initialization(metadata, error_without_id);
  expect_true(metadata.initialization_detector.request_count == 3 &&
                  metadata.initialization_detector.complete_count == 1 &&
                  metadata.initialization_detector.error_count == 2 &&
                  metadata.initialization_detector.total_detector_ms == 24.0F &&
                  metadata.detector.request_count == 8 &&
                  metadata.latency.mean_runtime_ms == 0.0F,
              "initialization requests and latency stay separate from frame counters");
  const std::string json = tracking_session_metadata_json(metadata);
  expect_true(contains(json, "\"requested_target_class_mode\": \"AUTO\"") &&
                  contains(json, "\"requested_target_class_id\": -1") &&
                  contains(json, "\"engine_class_filter\": -1") &&
                  contains(json, "\"target_class_match_config\": {\"calibrated\": false") &&
                  contains(json, "\"min_roi_coverage\": 0.600000") &&
                  contains(json, "\"max_center_ratio\": 0.700000") &&
                  contains(json, "\"ambiguity_margin\": 0.200000") &&
                  contains(json, "\"mean_detector_ms\": 8.000000") &&
                  contains(json, "\"request_count\": 3") &&
                  contains(json, "\"complete_count\": 1") &&
                  contains(json, "\"error_count\": 2"),
              "metadata records requested mode, match settings, and distinct initialization counters");
  const auto history = json.find("\"target_class_initializations\": [");
  const auto first_index = json.find("\"initialization_index\": 1", history);
  const auto second_index = json.find("\"initialization_index\": 2", history);
  const auto third_index = json.find("\"initialization_index\": 3", history);
  const auto fourth_index = json.find("\"initialization_index\": 4", history);
  expect_true(history != std::string::npos && first_index < second_index &&
                  second_index < third_index && third_index < fourth_index &&
                  contains(json, "\"request_id\": 91") &&
                  contains(json, "\"request_id\": 92") &&
                  contains(json, "\"request_id\": null") &&
                  contains(json, "\"target_class_id\": null") &&
                  contains(json, "\"best_class_id\": null, \"best_score\": null") &&
                  contains(json, "\"runner_up_class_id\": null, \"runner_up_score\": null") &&
                  contains(json, "\"runner_up_class_id\": 3") &&
                  contains(json, "\"original_index\": 4") &&
                  contains(json, "\"original_index\": 5") &&
                  contains(json, "\"confidence\": null") &&
                  contains(json, "\"detector_error\": \"worker, stopped\\nretry \\\"disabled\\\"\"") &&
                  contains(json, "\"detector_ms\": 15.000000"),
              "same-frame initialization history retains order, result diagnostics and null optionals");
  expect_true(contains(json, "\"schema_version\": 10") &&
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
              "schema-v10 metadata exposes hold, position gate, detector identity, tensors, versions, and counters");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "tracking log tests passed\n";
  return 0;
}
