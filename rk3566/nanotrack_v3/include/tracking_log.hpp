#pragma once

#include "long_term_runtime.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nanotrack {

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
  bool detector_requested = false;
  DetectorBatchStatus detector_status = DetectorBatchStatus::kNone;
  std::size_t detector_candidates_queued = 0;
  float tracker_ms = 0.0F;
  float detector_ms = 0.0F;
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

std::string tracking_csv_header();
std::string tracking_csv_row(const TrackingLogRow& row);
TrackingLogRow make_initial_log_row(std::uint64_t frame_id,
                                    std::int64_t timestamp_ms,
                                    const BBox& bbox);
TrackingLogRow make_tracking_log_row(const RuntimeFrameResult& result,
                                     const std::string& note = "");
LatencySummary summarize_latencies(const std::vector<float>& runtime_ms,
                                   const std::vector<float>& pipeline_ms);

}  // namespace nanotrack
