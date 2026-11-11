#pragma once

#include "controller_config.hpp"
#include "long_term_types.hpp"
#include "motion_estimator.hpp"
#include "occlusion_evidence.hpp"
#include "response_baseline.hpp"
#include "recovery_candidates.hpp"

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
  TrackingPhase tracking_phase = TrackingPhase::kInvalid;
  bool observation_valid = false;
  ControlSource control_source = ControlSource::kNone;
  BBox motion_predicted_bbox{};
  BBox predicted_bbox{};
  float search_scale = 1.0F;
  HypothesisSource hypothesis_source = HypothesisSource::kMotion;
  DetectorTargetMode detector_target_mode = DetectorTargetMode::kFullObject;
  std::optional<std::uint64_t> detector_candidate_id;
  std::optional<BBox> detector_candidate_region;
  std::optional<int> detector_candidate_class_id;
  std::optional<float> detector_candidate_confidence;
  std::optional<float> detector_candidate_center_ratio;
  std::size_t detector_same_class_candidates = 0;
  bool detector_same_class_ambiguous = false;
  bool detector_candidate_locked = false;
  std::size_t detector_probe_index = 0;
  bool request_detector = false;
  std::optional<DetectorRequest> detector_request;
  bool run_tracker = true;
  std::size_t hold_remaining_frames = 0;
  std::size_t occlusion_coast_age_frames = 0;
  OcclusionProbeType occlusion_probe_type = OcclusionProbeType::kNone;
  bool motion_control_authorized = false;
  std::optional<SearchHint> search_hint;
  std::int64_t motion_search_age_ms = -1;
  bool motion_prediction_confident = false;
  float motion_velocity_x = 0.0F;
  float motion_velocity_y = 0.0F;
  float motion_acceleration_x = 0.0F;
  float motion_acceleration_y = 0.0F;
  float motion_scale_velocity_width = 0.0F;
  float motion_scale_velocity_height = 0.0F;
  std::optional<RecoveryCandidateKey> recovery_probe;
  TemplateRunMode template_run_mode = TemplateRunMode::kRecentOnly;
};

enum class AcceptedObservationPath {
  kDirect,
  kTemplateBridge,  // Historical telemetry value.
  kShortConfirmation,
  kLocalOrDeeperRecovery,
  kDetectorRecovery,
  kOcclusionProbeRecovery,
  kSearchReacquisition,
};

struct ControllerOutput {
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  TrackingMode mode = TrackingMode::kTracking;
  TrackingPhase tracking_phase = TrackingPhase::kInvalid;
  bool observation_valid = false;
  bool observation_present = false;
  bool trusted_for_state_update = false;
  ControlSource control_source = ControlSource::kNone;
  QualityLevel quality = QualityLevel::kRejected;
  BBox bbox{};
  BBox tracking_bbox{};
  BBox display_bbox{};
  BBox control_bbox{};
  bool control_valid = false;
  bool accepted = false;
  bool reacquired = false;
  AcceptedObservationPath accepted_observation_path =
      AcceptedObservationPath::kDirect;
  TemplateKind selected_template = TemplateKind::kAnchor;
  TemplateAction template_action = TemplateAction::kNone;
  ResponseMetrics selected_metrics{};
  std::optional<BBox> selected_observation_bbox;
  float center_innovation_x = 0.0F;
  float center_innovation_y = 0.0F;
  float center_correction_x = 0.0F;
  float center_correction_y = 0.0F;
  bool center_correction_limited = false;
  float template_iou = 0.0F;
  float normalized_center_residual = 0.0F;
  float area_log_change = 0.0F;
  float aspect_log_change = 0.0F;
  float search_scale = 1.0F;
  HypothesisSource hypothesis_source = HypothesisSource::kMotion;
  DetectorBatchStatus detector_status = DetectorBatchStatus::kNone;
  std::size_t detector_candidates_queued = 0;
  std::size_t detector_position_valid_count = 0;
  std::size_t detector_position_rejected_count = 0;
  std::optional<std::uint64_t> detector_candidate_id;
  std::size_t detector_same_class_candidates = 0;
  bool detector_same_class_ambiguous = false;
  bool detector_candidate_locked = false;
  std::size_t detector_probe_index = 0;
  std::size_t detector_confirmation_attempts = 0;
  std::size_t detector_confirmation_hits = 0;
  bool occlusion_hold_triggered = false;
  std::size_t occlusion_hold_remaining_frames = 0;
  std::optional<float> occlusion_reference_width;
  std::optional<float> occlusion_reference_height;
  std::optional<float> occlusion_width_ratio;
  std::optional<float> occlusion_height_ratio;
  std::size_t occlusion_consecutive_unreliable_frames = 0;
  std::optional<float> occlusion_observation_width;
  std::optional<float> occlusion_observation_height;
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
  bool motion_control_authorized = false;
  std::optional<SearchHint> search_hint;
  std::int64_t motion_search_age_ms = -1;
  bool motion_prediction_confident = false;
  float motion_velocity_x = 0.0F;
  float motion_velocity_y = 0.0F;
  float motion_acceleration_x = 0.0F;
  float motion_acceleration_y = 0.0F;
  float motion_scale_velocity_width = 0.0F;
  float motion_scale_velocity_height = 0.0F;
  std::optional<RecoveryCandidateKey> recovery_candidate_key;
  std::size_t recovery_confirmation_hits = 0;
  bool recovery_conflict = false;
  std::vector<DetectorCandidateDiagnostic> detector_candidate_diagnostics;
  TargetClassState target_class{};
};

class LongTermController {
 public:
  explicit LongTermController(const ControllerConfig& config);

  void initialize(const BBox& initial_bbox,
                  std::uint64_t frame_id,
                  std::int64_t timestamp_ms,
                  FrameSize frame_size,
                  const TargetClassState& target_class = TargetClassState{});
  SearchPlan begin_frame(std::uint64_t frame_id,
                         std::int64_t timestamp_ms,
                         bool detector_ready);
  ControllerOutput finish_frame(
      const SearchPlan& plan,
      const std::vector<TrackerObservation>& observations,
      const std::optional<DetectorBatch>& detector_batch);
  // Abandon an unfinished frame and revoke its coast budget. Already-finished
  // controller state is retained.
  void abort_frame() noexcept;

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
    bool score_gate = false;
    bool center_gate = false;
    bool area_gate = false;
    bool aspect_gate = false;
  };

  struct Confirmation {
    std::uint64_t frame_id = 0;
    BBox bbox{};
  };

  struct CoastProbeCandidate {
    std::uint64_t frame_id = 0;
    BBox bbox{};
  };

  struct QueuedDetectorCandidate {
    std::uint64_t id = 0;
    DetectorCandidate candidate{};
    float center_ratio = 0.0F;
    std::vector<BBox> probes;
    std::size_t same_class_candidates = 0;
    bool same_class_ambiguous = false;
    std::uint64_t source_frame_id = 0;
    std::int64_t source_timestamp_ms = 0;
    std::uint64_t generation = 0;
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

  struct DetectorIngestResult {
    DetectorBatchStatus status = DetectorBatchStatus::kNone;
    std::size_t position_valid_count = 0;
    std::size_t position_rejected_count = 0;
    std::vector<DetectorCandidateDiagnostic> diagnostics;
  };

  struct ScaleReference {
    float width = 0.0F;
    float height = 0.0F;
  };

  struct DimensionScaleState {
    std::deque<float> samples;
    float filtered = 0.0F;
    float emitted = 0.0F;
    int direction = 0;
    int confirmations = 0;
  };

  TrackingMode mode_for_timestamp(std::int64_t timestamp_ms) const;
  float search_scale_for(TrackingMode mode, std::int64_t timestamp_ms) const;
  EvaluatedObservation evaluate(const TrackerObservation& observation,
                                const SearchPlan& plan) const;
  enum class RecentDisposition { kDirect, kTentative, kSearch };
  RecentDisposition assess_recent(const SearchPlan& plan,
                                  const EvaluatedObservation& recent);
  void publish_tentative(const SearchPlan& plan,
                         const EvaluatedObservation& recent,
                         ControllerOutput& output);
  void clear_suspicion() noexcept;
  void promote_shadow_prediction() noexcept;
  void restore_prediction_checkpoint() noexcept;
  void complete_frame(const SearchPlan& plan, ControllerOutput& output);
  BBox constrain_tentative_control(const BBox& observation,
                                  std::int64_t timestamp_ms) const;
  float trusted_elapsed_seconds(std::int64_t timestamp_ms) const;
  bool trusted_control_live(std::int64_t timestamp_ms) const;
  bool search_control_live(std::int64_t timestamp_ms) const;
  BBox bound_recent_scale(const BBox& bbox, std::int64_t timestamp_ms) const;
  bool add_confirmation(std::uint64_t frame_id, const BBox& bbox);
  bool uses_unified_recovery() const noexcept;
  void invalidate_unified_recovery() noexcept;
  void cancel_probation() noexcept;
  SearchHint search_hint_for(const RecoveryCandidate& candidate) const;
  std::optional<EvaluatedObservation> confirm_recovery_candidate(
      const SearchPlan& plan,
      const std::optional<EvaluatedObservation>& recent,
      const std::optional<EvaluatedObservation>& safe,
      ControllerOutput& output);
  std::vector<BBox> detector_probes(const BBox& region) const;
  void activate_next_detector_candidate();
  bool detector_geometry_consistent(const BBox& reference,
                                    const BBox& bbox) const;
  bool detector_confirmation_consistent(const BBox& bbox) const;
  void reset_scale_state(const BBox& bbox);
  static void reset_scale_evidence(DimensionScaleState& state);
  BBox stabilize_accepted_bbox(const BBox& observation,
                               DimensionScaleState& width_state,
                               DimensionScaleState& height_state) const;
  BBox stabilize_accepted_center(const SearchPlan& plan,
                                 const BBox& observation,
                                 const BBox& scaled_bbox,
                                 bool reacquired,
                                 ControllerOutput& output) const;
  void accept_observation(const SearchPlan& plan,
                          const EvaluatedObservation& selected,
                          AcceptedObservationPath acceptance_path,
                          ControllerOutput& output);
  DetectorIngestResult ingest_detector_batch(
      const SearchPlan& plan,
      const std::optional<DetectorBatch>& batch);
  void publish_occlusion_evidence(ControllerOutput& output);
  static void validate_output_semantics(const ControllerOutput& output);
  static void finalize_output_semantics(ControllerOutput& output);

  struct TentativeControl {
    BBox bbox;
    std::int64_t timestamp_ms;
  };

  struct PredictionCheckpoint {
    MotionEstimator motion;
    std::optional<MotionEstimator> shadow_motion;
    std::optional<TentativeControl> tentative_control;
    std::optional<std::int64_t> degraded_since_ms;
    std::optional<std::int64_t> suspected_since_ms;
    std::optional<std::int64_t> clean_since_ms;
    TrackingMode mode;
  };

  ControllerConfig config_;
  MotionEstimator motion_;
  std::optional<MotionEstimator> shadow_motion_;
  std::optional<TentativeControl> tentative_control_;
  std::optional<PredictionCheckpoint> prediction_checkpoint_;
  ResponseBaseline response_baseline_;
  std::optional<std::int64_t> response_degraded_since_ms_;
  std::optional<std::int64_t> suspected_since_ms_;
  std::optional<std::int64_t> response_clean_since_ms_;
  std::int64_t last_trusted_timestamp_ms_ = 0;
  bool legacy_anchor_observations_ = false;
  bool recent_safe_observations_ = false;
  RecoveryCandidateTable recovery_candidates_;
  OcclusionEvidenceTracker occlusion_evidence_;
  OcclusionEvidenceDecision occlusion_decision_{};
  FrameSize frame_size_{};
  BBox current_bbox_{};
  BBox display_bbox_{};
  DimensionScaleState width_scale_state_{};
  DimensionScaleState height_scale_state_{};
  TargetClassState target_class_{};
  TrackingMode mode_ = TrackingMode::kTracking;
  std::optional<std::int64_t> loss_start_ms_;
  std::optional<std::int64_t> last_detector_request_ms_;
  std::optional<std::uint64_t> last_detector_request_frame_;
  std::optional<BBox> last_detector_request_motion_bbox_;
  std::deque<QueuedDetectorCandidate> detector_candidates_;
  std::optional<ActiveDetectorCandidate> active_detector_candidate_;
  std::uint64_t next_detector_candidate_id_ = 1;
  std::deque<Confirmation> confirmations_;
  std::optional<std::uint64_t> last_consumed_detector_frame_;
  std::optional<CoastProbeCandidate> coast_probe_candidate_;
  std::uint64_t last_frame_id_ = 0;
  std::int64_t last_timestamp_ms_ = 0;
  SearchPlan active_plan_{};
  int stable_frames_ = 0;
  std::int64_t last_recent_update_ms_ = 0;
  bool recent_probation_ = false;
  bool recovery_probation_ = false;
  bool rollback_pending_ = false;
  bool resume_search_after_rollback_ = false;
  int probation_reliable_frames_ = 0;
  int consecutive_unreliable_frames_ = 0;
  std::size_t hold_remaining_frames_ = 0;
  std::size_t occlusion_coast_age_frames_ = 0;
  std::size_t occlusion_suspicion_frames_ = 0;
  bool hold_used_for_loss_ = false;
  std::optional<ScaleReference> frozen_scale_reference_;
  std::uint64_t detector_eligibility_generation_ = 0;
  // Within one initialized session only coast entry advances eligibility.
  // Keep that interval so late completions retain their invalidation reason
  // after coast expiry/recovery, without retaining an unbounded generation set.
  std::uint64_t detector_session_generation_ = 0;
  bool motion_search_active_ = false;
  bool initialized_ = false;
  bool frame_open_ = false;
};

float bbox_iou(const BBox& first, const BBox& second);

}  // namespace nanotrack
