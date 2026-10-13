#pragma once

#include "controller_config.hpp"
#include "long_term_types.hpp"
#include "motion_estimator.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace nanotrack {

struct SearchPlan {
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  TrackingMode mode = TrackingMode::kTracking;
  BBox predicted_bbox{};
  float search_scale = 1.0F;
  HypothesisSource hypothesis_source = HypothesisSource::kMotion;
  std::optional<int> detector_candidate_class_id;
  std::optional<float> detector_candidate_confidence;
  bool request_detector = false;
  std::optional<DetectorRequest> detector_request;
};

struct ControllerOutput {
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  TrackingMode mode = TrackingMode::kTracking;
  QualityLevel quality = QualityLevel::kRejected;
  BBox bbox{};
  bool control_valid = false;
  bool accepted = false;
  bool reacquired = false;
  TemplateKind selected_template = TemplateKind::kAnchor;
  TemplateAction template_action = TemplateAction::kNone;
  ResponseMetrics selected_metrics{};
  float template_iou = 0.0F;
  float normalized_center_residual = 0.0F;
  float area_log_change = 0.0F;
  float aspect_log_change = 0.0F;
  float search_scale = 1.0F;
  HypothesisSource hypothesis_source = HypothesisSource::kMotion;
  DetectorBatchStatus detector_status = DetectorBatchStatus::kNone;
  std::size_t detector_candidates_queued = 0;
};

class LongTermController {
 public:
  explicit LongTermController(const ControllerConfig& config);

  void initialize(const BBox& initial_bbox,
                  std::uint64_t frame_id,
                  std::int64_t timestamp_ms,
                  FrameSize frame_size);
  SearchPlan begin_frame(std::uint64_t frame_id,
                         std::int64_t timestamp_ms,
                         bool detector_ready);
  ControllerOutput finish_frame(
      const SearchPlan& plan,
      const std::vector<TrackerObservation>& observations,
      const std::optional<DetectorBatch>& detector_batch);

  TrackingMode mode() const { return mode_; }
  const BBox& current_bbox() const { return current_bbox_; }
  const ControllerConfig& config() const { return config_; }

 private:
  struct EvaluatedObservation {
    TrackerObservation observation{};
    QualityLevel quality = QualityLevel::kRejected;
    float center_residual = 0.0F;
    float area_log_change = 0.0F;
    float aspect_log_change = 0.0F;
  };

  struct Confirmation {
    std::uint64_t frame_id = 0;
    BBox bbox{};
  };

  TrackingMode mode_for_timestamp(std::int64_t timestamp_ms) const;
  float search_scale_for(TrackingMode mode, std::int64_t timestamp_ms) const;
  EvaluatedObservation evaluate(const TrackerObservation& observation,
                                const SearchPlan& plan) const;
  bool add_confirmation(std::uint64_t frame_id, const BBox& bbox);
  DetectorBatchStatus ingest_detector_batch(
      const SearchPlan& plan,
      const std::optional<DetectorBatch>& batch);

  ControllerConfig config_;
  MotionEstimator motion_;
  FrameSize frame_size_{};
  BBox current_bbox_{};
  TrackingMode mode_ = TrackingMode::kTracking;
  std::optional<std::int64_t> loss_start_ms_;
  std::optional<std::int64_t> last_detector_request_ms_;
  std::optional<std::uint64_t> last_detector_request_frame_;
  std::deque<DetectorCandidate> detector_candidates_;
  std::deque<Confirmation> confirmations_;
  std::uint64_t last_frame_id_ = 0;
  std::int64_t last_timestamp_ms_ = 0;
  SearchPlan active_plan_{};
  int stable_frames_ = 0;
  std::int64_t last_recent_update_ms_ = 0;
  bool recent_probation_ = false;
  int probation_reliable_frames_ = 0;
  bool initialized_ = false;
  bool frame_open_ = false;
};

float bbox_iou(const BBox& first, const BBox& second);

}  // namespace nanotrack
