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
  DetectorTargetMode detector_target_mode = DetectorTargetMode::kFullObject;
  std::optional<std::uint64_t> detector_candidate_id;
  std::optional<BBox> detector_candidate_region;
  std::optional<int> detector_candidate_class_id;
  std::optional<float> detector_candidate_confidence;
  std::size_t detector_same_class_candidates = 0;
  bool detector_same_class_ambiguous = false;
  bool detector_candidate_locked = false;
  std::size_t detector_probe_index = 0;
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
  std::optional<std::uint64_t> detector_candidate_id;
  std::size_t detector_same_class_candidates = 0;
  bool detector_same_class_ambiguous = false;
  bool detector_candidate_locked = false;
  std::size_t detector_probe_index = 0;
  std::size_t detector_confirmation_attempts = 0;
  std::size_t detector_confirmation_hits = 0;
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

  struct QueuedDetectorCandidate {
    std::uint64_t id = 0;
    DetectorCandidate candidate{};
    std::vector<BBox> probes;
    std::size_t same_class_candidates = 0;
  };

  struct ActiveDetectorCandidate {
    QueuedDetectorCandidate queued{};
    std::size_t next_probe = 0;
    bool locked = false;
    std::size_t locked_probe_index = 0;
    BBox locked_bbox{};
    std::size_t confirmation_attempts = 0;
    std::vector<BBox> confirmation_boxes;
  };

  TrackingMode mode_for_timestamp(std::int64_t timestamp_ms) const;
  float search_scale_for(TrackingMode mode, std::int64_t timestamp_ms) const;
  EvaluatedObservation evaluate(const TrackerObservation& observation,
                                const SearchPlan& plan) const;
  bool add_confirmation(std::uint64_t frame_id, const BBox& bbox);
  std::vector<BBox> detector_probes(const DetectorCandidate& candidate) const;
  void activate_next_detector_candidate();
  bool detector_geometry_consistent(const BBox& reference,
                                    const BBox& bbox) const;
  bool detector_confirmation_consistent(const BBox& bbox) const;
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
  std::deque<QueuedDetectorCandidate> detector_candidates_;
  std::optional<ActiveDetectorCandidate> active_detector_candidate_;
  std::uint64_t next_detector_candidate_id_ = 1;
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
