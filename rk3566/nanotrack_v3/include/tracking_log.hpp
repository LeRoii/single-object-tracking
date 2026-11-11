#pragma once

#include "long_term_runtime.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nanotrack {

constexpr int kTrackingSchemaVersion = 10;

class MonotonicVideoClock {
 public:
  explicit MonotonicVideoClock(double source_fps);
  std::int64_t next(double pts_ms);
  bool fps_fallback() const { return fps_fallback_; }
  std::int64_t frame_step_ms() const { return frame_step_ms_; }

 private:
  std::optional<std::int64_t> last_timestamp_ms_;
  std::int64_t frame_step_ms_ = 33;
  bool fps_fallback_ = false;
};

struct TrackingLogRow {
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  TrackingMode mode = TrackingMode::kTracking;
  TrackingPhase tracking_phase = TrackingPhase::kInvalid;
  bool observation_valid = false;
  ControlSource control_source = ControlSource::kNone;
  AcceptedObservationPath accepted_observation_path =
      AcceptedObservationPath::kDirect;
  QualityLevel quality = QualityLevel::kRejected;
  bool control_valid = false;
  bool accepted = false;
  bool reacquired = false;
  BBox bbox{};
  BBox tracking_bbox{};
  BBox display_bbox{};
  BBox control_bbox{};
  TemplateKind selected_template = TemplateKind::kAnchor;
  TemplateAction template_action = TemplateAction::kNone;
  bool anchor_present = false;
  ResponseMetrics anchor_metrics{};
  bool recent_present = false;
  ResponseMetrics recent_metrics{};
  float template_iou = 0.0F;
  float normalized_center_residual = 0.0F;
  float area_log_change = 0.0F;
  float aspect_log_change = 0.0F;
  float search_scale = 1.0F;
  HypothesisSource hypothesis_source = HypothesisSource::kMotion;
  BBox motion_predicted_bbox{};
  bool detector_configured = false;
  bool detector_ready = false;
  bool detector_requested = false;
  std::optional<std::uint64_t> detector_request_frame_id;
  std::string detector_worker_status = "disabled";
  std::string detector_error;
  std::optional<std::uint64_t> detector_batch_frame_id;
  std::optional<std::int64_t> detector_batch_age_ms;
  std::size_t detector_raw_candidate_count = 0;
  std::size_t detector_filtered_candidate_count = 0;
  std::optional<int> detector_verified_class_id;
  std::optional<float> detector_verified_confidence;
  std::optional<float> detector_candidate_center_ratio;
  DetectorTargetMode detector_target_mode = DetectorTargetMode::kFullObject;
  std::optional<std::uint64_t> detector_candidate_id;
  std::optional<BBox> detector_candidate_region;
  std::size_t detector_same_class_candidates = 0;
  bool detector_same_class_ambiguous = false;
  bool detector_candidate_locked = false;
  std::size_t detector_probe_index = 0;
  std::size_t detector_confirmation_attempts = 0;
  std::size_t detector_confirmation_hits = 0;
  DetectorTiming detector_timing{};
  DetectorBatchStatus detector_status = DetectorBatchStatus::kNone;
  std::size_t detector_candidates_queued = 0;
  std::size_t detector_position_valid_count = 0;
  std::size_t detector_position_rejected_count = 0;
  bool occlusion_hold_triggered = false;
  OcclusionReferenceSource occlusion_reference_source = OcclusionReferenceSource::kNone;
  bool occlusion_suspicion_active = false;
  bool occlusion_suspicion_started = false;
  bool occlusion_suspicion_cleared = false;
  std::size_t occlusion_suspicion_frames = 0;
  std::size_t occlusion_clean_frames = 0;
  std::size_t occlusion_width_shrink_frames = 0;
  std::size_t occlusion_height_shrink_frames = 0;
  bool occlusion_severe_evidence = false;
  bool occlusion_gradual_evidence = false;
  bool occlusion_template_evidence = false;
  bool occlusion_peak_margin_evidence = false;
  bool occlusion_motion_residual_evidence = false;
  OcclusionTriggerReason occlusion_trigger_reason = OcclusionTriggerReason::kNone;
  std::size_t occlusion_coast_age_frames = 0;
  OcclusionProbeType occlusion_probe_type = OcclusionProbeType::kNone;
  std::size_t occlusion_probe_confirmation_hits = 0;
  std::size_t occlusion_probe_confirmation_required = 0;
  bool motion_control_authorized = false;
  std::int64_t motion_search_age_ms = -1;
  bool motion_prediction_confident = false;
  float motion_velocity_x = 0.0F;
  float motion_velocity_y = 0.0F;
  float motion_acceleration_x = 0.0F;
  float motion_acceleration_y = 0.0F;
  float motion_scale_velocity_width = 0.0F;
  float motion_scale_velocity_height = 0.0F;
  std::optional<SearchHint> search_hint;
  std::string occlusion_probe_rejection = "NONE";
  std::optional<BBox> occlusion_probe_motion_bbox;
  std::optional<BBox> occlusion_probe_bbox;
  std::optional<bool> occlusion_probe_anchor_reliable;
  std::optional<bool> occlusion_probe_recent_reliable;
  std::optional<bool> occlusion_probe_template_gate;
  std::optional<bool> occlusion_probe_cross_gate;
  std::optional<bool> occlusion_probe_cross_iou;
  std::optional<bool> occlusion_probe_cross_center;
  std::optional<bool> occlusion_probe_cross_area;
  std::optional<bool> occlusion_probe_cross_aspect;
  std::optional<bool> occlusion_probe_anchor_score_gate;
  std::optional<bool> occlusion_probe_anchor_center_gate;
  std::optional<bool> occlusion_probe_anchor_area_gate;
  std::optional<bool> occlusion_probe_anchor_aspect_gate;
  std::optional<bool> occlusion_probe_recent_score_gate;
  std::optional<bool> occlusion_probe_recent_center_gate;
  std::optional<bool> occlusion_probe_recent_area_gate;
  std::optional<bool> occlusion_probe_recent_aspect_gate;
  std::size_t occlusion_hold_remaining_frames = 0;
  std::optional<float> occlusion_reference_width;
  std::optional<float> occlusion_reference_height;
  std::optional<float> occlusion_width_ratio;
  std::optional<float> occlusion_height_ratio;
  bool tracker_skipped = false;
  bool detector_suppressed_by_hold = false;
  TargetClassState target_class{};
  float tracker_ms = 0.0F;
  float runtime_ms = 0.0F;
  float pipeline_ms = 0.0F;
  std::string note;
};

struct LatencySummary {
  float mean_runtime_ms = 0.0F;
  float p50_runtime_ms = 0.0F;
  float p95_runtime_ms = 0.0F;
  float runtime_fps = 0.0F;
  float mean_pipeline_ms = 0.0F;
  float p50_pipeline_ms = 0.0F;
  float p95_pipeline_ms = 0.0F;
  float end_to_end_fps = 0.0F;
};

struct DetectorSessionMetadata {
  bool enabled = false;
  bool calibrated = false;
  std::string model_path;
  std::string model_sha256;
  std::string manifest_path;
  std::string manifest_sha256;
  std::string labels_path;
  std::string labels_sha256;
  int class_id = -1;
  float confidence_threshold = 0.25F;
  float nms_threshold = 0.45F;
  std::string input_tensor_description;
  std::vector<std::string> output_tensor_descriptions;
  std::string rknn_api_version;
  std::string rknn_driver_version;
  std::size_t request_count = 0;
  std::size_t complete_count = 0;
  std::size_t reject_count = 0;
  std::size_t error_count = 0;
};

struct InitializationDetectorSummary {
  std::size_t request_count = 0;
  std::size_t complete_count = 0;
  std::size_t error_count = 0;
  float total_detector_ms = 0.0F;
};

struct TrackingSessionMetadata {
  std::string video;
  std::string model_dir;
  std::string manifest_hash_algorithm;
  std::string manifest_hash;
  std::string controller_config;
  std::string controller_config_hash_algorithm;
  std::string controller_config_hash;
  std::string sensor;
  bool calibrated = false;
  int requested_target_class_id = -1;
  int engine_class_filter = -1;
  TargetClassMatchConfig target_class_match_config{};
  std::vector<TargetClassInitializationResult> target_class_initializations;
  InitializationDetectorSummary initialization_detector{};
  DetectorTargetMode detector_target_mode = DetectorTargetMode::kFullObject;
  float detector_candidate_max_center_ratio = 0.15F;
  int occlusion_hold_frames = 12;
  int occlusion_trigger_consecutive_unreliable = 2;
  float occlusion_trigger_min_dimension_ratio = 0.70F;
  int occlusion_reference_window = 8;
  int occlusion_probe_interval_frames = 5;
  float occlusion_gradual_dimension_ratio = 0.85F;
  int occlusion_gradual_consecutive_frames = 2;
  int occlusion_min_reference_samples = 3;
  float occlusion_shrink_step_ratio = 0.97F;
  int occlusion_suspicion_clear_frames = 2;
  float occlusion_peak_margin_drop = 0.02F;
  float occlusion_motion_residual_threshold = 0.75F;
  float occlusion_confirmation_iou = 0.50F;
  float occlusion_confirmation_center_distance = 0.35F;
  float occlusion_confirmation_max_area_log_change = 0.50F;
  float occlusion_confirmation_max_aspect_log_change = 0.40F;
  std::string rknn_api_version;
  std::string rknn_driver_version;
  double source_fps = 0.0;
  bool fps_fallback = false;
  std::int64_t frame_step_ms = 0;
  float reliable_score = 0.0F;
  float reliable_psr = 0.0F;
  float reliable_apce = 0.0F;
  float reliable_peak_margin = 0.0F;
  float max_accepted_scale_step_ratio = 1.0F;
  float accepted_scale_deadband_ratio = 0.0F;
  float accepted_scale_smoothing_alpha = 1.0F;
  int accepted_scale_median_window_frames = 1;
  int accepted_scale_direction_confirm_frames = 1;
  float accepted_scale_output_hysteresis_px = 0.0F;
  float accepted_center_smoothing_alpha = 1.0F;
  float accepted_center_max_correction_ratio = 0.0F;
  float max_reacquisition_center_distance = 0.0F;
  std::int64_t uncertain_ms = 0;
  std::int64_t local_recovery_ms = 0;
  std::int64_t assisted_recovery_ms = 0;
  bool motion_search_enabled = false;
  bool search_reacquisition_requires_detector = false;
  std::int64_t search_reacquisition_min_age_ms = 0;
  int template_bridge_min_stable_frames = 0;
  std::int64_t template_bridge_max_duration_ms = 0;
  std::int64_t motion_search_confidence_ms = 0;
  float motion_max_speed_diagonals_per_s = 0.0F;
  float motion_max_acceleration_diagonals_per_s2 = 0.0F;
  float motion_scale_rate_limit_per_s = 0.0F;
  std::int64_t search_probe_interval_ms = 0;
  std::string controller_config_identity;
  std::size_t tracked_frames = 0;
  LatencySummary latency{};
  DetectorSessionMetadata detector{};
};

std::string tracking_csv_header();
void apply_controller_config_metadata(TrackingSessionMetadata& metadata,
                                      const ControllerConfig& config);
std::string tracking_csv_row(const TrackingLogRow& row);
TrackingLogRow make_initial_log_row(std::uint64_t frame_id,
                                    std::int64_t timestamp_ms,
                                    const BBox& bbox,
                                    bool detector_configured = false,
                                    bool detector_ready = false,
                                    DetectorTargetMode detector_target_mode =
                                        DetectorTargetMode::kFullObject,
                                    const TargetClassState& target_class =
                                        TargetClassState{});
TrackingLogRow make_tracking_log_row(const RuntimeFrameResult& result,
                                     const std::string& note = "");
LatencySummary summarize_latencies(const std::vector<float>& runtime_ms,
                                   const std::vector<float>& pipeline_ms);
void record_target_class_initialization(
    TrackingSessionMetadata& metadata,
    const TargetClassInitializationResult& initialization);
std::string tracking_session_metadata_json(
    const TrackingSessionMetadata& metadata);

}  // namespace nanotrack
