#pragma once

#include "long_term_runtime.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nanotrack {

constexpr int kTrackingSchemaVersion = 6;

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
  QualityLevel quality = QualityLevel::kRejected;
  bool control_valid = false;
  bool accepted = false;
  bool reacquired = false;
  BBox bbox{};
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
  DetectorTargetMode detector_target_mode = DetectorTargetMode::kFullObject;
  float detector_candidate_max_center_ratio = 0.15F;
  std::string rknn_api_version;
  std::string rknn_driver_version;
  double source_fps = 0.0;
  bool fps_fallback = false;
  std::int64_t frame_step_ms = 0;
  float reliable_score = 0.0F;
  float reliable_psr = 0.0F;
  float reliable_apce = 0.0F;
  float reliable_peak_margin = 0.0F;
  std::int64_t uncertain_ms = 0;
  std::int64_t local_recovery_ms = 0;
  std::int64_t assisted_recovery_ms = 0;
  std::size_t tracked_frames = 0;
  LatencySummary latency{};
  DetectorSessionMetadata detector{};
};

std::string tracking_csv_header();
std::string tracking_csv_row(const TrackingLogRow& row);
TrackingLogRow make_initial_log_row(std::uint64_t frame_id,
                                    std::int64_t timestamp_ms,
                                    const BBox& bbox,
                                    bool detector_configured = false,
                                    bool detector_ready = false,
                                    DetectorTargetMode detector_target_mode =
                                        DetectorTargetMode::kFullObject);
TrackingLogRow make_tracking_log_row(const RuntimeFrameResult& result,
                                     const std::string& note = "");
LatencySummary summarize_latencies(const std::vector<float>& runtime_ms,
                                   const std::vector<float>& pipeline_ms);
std::string tracking_session_metadata_json(
    const TrackingSessionMetadata& metadata);

}  // namespace nanotrack
