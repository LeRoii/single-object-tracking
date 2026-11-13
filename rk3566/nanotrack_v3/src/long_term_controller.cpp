#include "long_term_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace nanotrack {
namespace {

constexpr float kPositionGateEpsilon = 1e-6F;
constexpr float kDominantNearestCandidateRatio = 2.0F / 3.0F;

bool finite_bbox(const BBox& bbox) {
  return std::isfinite(bbox.x) && std::isfinite(bbox.y) &&
         std::isfinite(bbox.width) && std::isfinite(bbox.height);
}

bool contains_center(const BBox& region, const BBox& bbox) {
  const float x = bbox.x + 0.5F * bbox.width;
  const float y = bbox.y + 0.5F * bbox.height;
  return x >= region.x && y >= region.y &&
         x <= region.x + region.width && y <= region.y + region.height;
}

bool valid_candidate(const DetectorCandidate& candidate, FrameSize frame_size) {
  const BBox& bbox = candidate.bbox;
  return finite_bbox(bbox) && bbox.x >= 0.0F && bbox.y >= 0.0F &&
         bbox.width > 0.0F && bbox.height > 0.0F &&
         bbox.x + bbox.width <= frame_size.width &&
         bbox.y + bbox.height <= frame_size.height &&
         std::isfinite(candidate.confidence) && candidate.confidence >= 0.0F &&
         candidate.confidence <= 1.0F;
}

float normalized_center_distance(const BBox& first, const BBox& second) {
  const float first_x = first.x + 0.5F * first.width;
  const float first_y = first.y + 0.5F * first.height;
  const float second_x = second.x + 0.5F * second.width;
  const float second_y = second.y + 0.5F * second.height;
  const float scale = std::sqrt(std::max(1.0F, second.width * second.height));
  return std::hypot(first_x - second_x, first_y - second_y) / scale;
}

float frame_normalized_center_distance(const BBox& first,
                                       const BBox& second,
                                       FrameSize frame_size) {
  const float first_x = first.x + 0.5F * first.width;
  const float first_y = first.y + 0.5F * first.height;
  const float second_x = second.x + 0.5F * second.width;
  const float second_y = second.y + 0.5F * second.height;
  const float frame_diagonal = std::hypot(
      static_cast<float>(frame_size.width),
      static_cast<float>(frame_size.height));
  return std::hypot(first_x - second_x, first_y - second_y) /
         frame_diagonal;
}

float area_log_delta(const BBox& first, const BBox& second) {
  const float ratio = (first.width * first.height) /
                      std::max(1e-6F, second.width * second.height);
  return std::fabs(std::log(ratio));
}

float aspect_log_delta(const BBox& first, const BBox& second) {
  const float first_aspect = first.width / std::max(1e-6F, first.height);
  const float second_aspect = second.width / std::max(1e-6F, second.height);
  return std::fabs(std::log(first_aspect / second_aspect));
}

int quality_rank(QualityLevel value) {
  switch (value) {
    case QualityLevel::kReliable: return 2;
    case QualityLevel::kWeak: return 1;
    case QualityLevel::kRejected: return 0;
  }
  return 0;
}

BBox centered_box(float center_x, float center_y, const BBox& reference,
                  FrameSize frame_size) {
  const float max_x = static_cast<float>(frame_size.width) - reference.width;
  const float max_y = static_cast<float>(frame_size.height) - reference.height;
  return BBox{std::clamp(center_x - 0.5F * reference.width, 0.0F, max_x),
              std::clamp(center_y - 0.5F * reference.height, 0.0F, max_y),
              reference.width, reference.height};
}

}  // namespace

float bbox_iou(const BBox& first, const BBox& second) {
  if (!finite_bbox(first) || !finite_bbox(second) || first.width <= 0.0F ||
      first.height <= 0.0F || second.width <= 0.0F || second.height <= 0.0F) {
    return 0.0F;
  }
  const float left = std::max(first.x, second.x);
  const float top = std::max(first.y, second.y);
  const float right = std::min(first.x + first.width, second.x + second.width);
  const float bottom = std::min(first.y + first.height, second.y + second.height);
  const float intersection =
      std::max(0.0F, right - left) * std::max(0.0F, bottom - top);
  const float union_area = first.width * first.height +
                           second.width * second.height - intersection;
  return union_area > 0.0F ? intersection / union_area : 0.0F;
}

LongTermController::LongTermController(const ControllerConfig& config)
    : config_(config), motion_(config), response_baseline_(config),
      recovery_candidates_(config), occlusion_evidence_(config) {}

void LongTermController::initialize(const BBox& initial_bbox,
                                    std::uint64_t frame_id,
                                    std::int64_t timestamp_ms,
                                    FrameSize frame_size,
                                    const TargetClassState& target_class) {
  if (frame_size.width <= 0 || frame_size.height <= 0) {
    throw std::invalid_argument("controller frame size must be positive");
  }
  validate_bbox(initial_bbox, frame_size.width, frame_size.height);
  if (initialized_) ++detector_eligibility_generation_;
  detector_session_generation_ = detector_eligibility_generation_;
  frame_size_ = frame_size;
  target_class_ = target_class;
  current_bbox_ = initial_bbox;
  display_bbox_ = initial_bbox;
  reset_scale_state(initial_bbox);
  motion_.initialize(initial_bbox, timestamp_ms, frame_size.width, frame_size.height);
  reset_motion_input_gate();
  motion_last_clean_trend_ = motion_.snapshot(timestamp_ms);
  mode_ = TrackingMode::kTracking;
  last_output_mode_ = mode_;
  last_output_phase_ = TrackingPhase::kTracking;
  prediction_checkpoint_.reset();
  shadow_motion_.reset();
  tentative_control_.reset();
  response_baseline_.reset_after_recovery();
  clear_suspicion();
  search_motion_reference_ = motion_.snapshot(timestamp_ms);
  search_motion_reference_held_ = false;
  search_motion_clean_since_ms_.reset();
  last_trusted_timestamp_ms_ = timestamp_ms;
  legacy_anchor_observations_ = false;
  recent_safe_observations_ = false;
  loss_start_ms_.reset();
  last_detector_request_ms_.reset();
  last_detector_request_frame_.reset();
  last_detector_request_motion_bbox_.reset();
  detector_candidates_.clear();
  active_detector_candidate_.reset();
  next_detector_candidate_id_ = 1;
  confirmations_.clear();
  recovery_candidates_.clear();
  last_consumed_detector_frame_.reset();
  coast_probe_candidate_.reset();
  last_frame_id_ = frame_id;
  last_timestamp_ms_ = timestamp_ms;
  stable_frames_ = 0;
  last_recent_update_ms_ = timestamp_ms;
  pending_template_transition_.reset();
  pending_recovery_acceptance_.reset();
  staged_template_slot_id_.reset();
  recent_probation_ = false;
  recovery_probation_ = false;
  rollback_pending_ = false;
  resume_search_after_rollback_ = false;
  probation_reliable_frames_ = 0;
  occlusion_evidence_.reset();
  occlusion_decision_ = {};
  occlusion_suspicion_frames_ = 0;
  occlusion_coast_age_frames_ = 0;
  consecutive_unreliable_frames_ = 0;
  hold_remaining_frames_ = 0;
  hold_used_for_loss_ = false;
  frozen_scale_reference_.reset();
  motion_search_active_ = false;
  initialized_ = true;
  frame_open_ = false;
}

TrackingMode LongTermController::mode_for_timestamp(
    std::int64_t timestamp_ms) const {
  if (!loss_start_ms_) {
    return suspected_since_ms_ ? TrackingMode::kSuspected : TrackingMode::kTracking;
  }
  const std::int64_t elapsed = timestamp_ms - *loss_start_ms_;
  if (elapsed < config_.uncertain_ms) return TrackingMode::kUncertain;
  if (elapsed < config_.local_recovery_ms) return TrackingMode::kLocalRecovery;
  if (elapsed < config_.assisted_recovery_ms) {
    return TrackingMode::kAssistedRecovery;
  }
  if (config_.motion_search_enabled) {
    return TrackingMode::kAssistedRecovery;
  }
  return TrackingMode::kLost;
}

float LongTermController::search_scale_for(TrackingMode mode,
                                           std::int64_t timestamp_ms) const {
  switch (mode) {
    case TrackingMode::kTracking:
    case TrackingMode::kSuspected: return config_.search_tracking;
    case TrackingMode::kUncertain: return config_.search_uncertain;
    case TrackingMode::kLocalRecovery: {
      const std::int64_t elapsed = timestamp_ms - *loss_start_ms_;
      const std::int64_t midpoint =
          (config_.uncertain_ms + config_.local_recovery_ms) / 2;
      return elapsed < midpoint ? config_.search_local_initial
                                : config_.search_local_late;
    }
    case TrackingMode::kAssistedRecovery: return config_.search_assisted;
    case TrackingMode::kLost: return config_.search_lost;
    case TrackingMode::kOcclusionHold: return config_.search_local_initial;
  }
  return config_.search_lost;
}

SearchPlan LongTermController::begin_frame(std::uint64_t frame_id,
                                           std::int64_t timestamp_ms,
                                           bool detector_ready) {
  if (!initialized_) throw std::logic_error("controller is not initialized");
  if (frame_open_) throw std::logic_error("previous controller frame is unfinished");
  if (pending_template_transition_) {
    resolve_template_transition(pending_template_transition_->transaction_id, false);
  }
  if (frame_id <= last_frame_id_ || timestamp_ms <= last_timestamp_ms_) {
    if (uses_unified_recovery()) {
      invalidate_unified_recovery();
      cancel_probation();
    }
    throw std::invalid_argument(frame_id <= last_frame_id_
        ? "frame id must be strictly increasing"
        : "frame timestamp must be strictly increasing");
  }
  if (uses_unified_recovery()) {
    if (frame_id - last_frame_id_ > 2 ||
        static_cast<std::uint64_t>(timestamp_ms) -
                static_cast<std::uint64_t>(last_timestamp_ms_) >
            static_cast<std::uint64_t>(config_.motion_max_dt_ms)) {
      invalidate_unified_recovery();
      cancel_probation();
    }
    if (resume_search_after_rollback_) {
      if (!loss_start_ms_) {
        loss_start_ms_ = last_timestamp_ms_;
        last_detector_request_ms_.reset();
        if (config_.motion_search_enabled) {
          motion_.begin_search(last_timestamp_ms_);
          motion_search_active_ = true;
        }
      }
      if (!rollback_pending_) resume_search_after_rollback_ = false;
    }
    recovery_candidates_.expire(frame_id, timestamp_ms,
                                detector_eligibility_generation_);
  }

  const TrackingMode timestamp_mode = mode_for_timestamp(timestamp_ms);
  BBox motion_predicted;
  MotionEstimator& predictor = shadow_motion_ ? *shadow_motion_ : motion_;
  try {
    prediction_checkpoint_.emplace(PredictionCheckpoint{
        motion_, shadow_motion_, tentative_control_, response_degraded_since_ms_,
        suspected_since_ms_, response_clean_since_ms_, motion_residual_history_,
        motion_recent_residuals_,
        motion_freeze_since_ms_, motion_clean_since_ms_,
        motion_frozen_response_reference_, motion_last_clean_trend_,
        motion_risk_streak_, motion_response_drop_since_ms_, mode_});
    // MotionEstimator clips finite predictions while retaining the frozen size.
    // Internal prediction failures abort this frame; no output may authorize it.
    motion_predicted = predictor.predict(timestamp_ms);
    validate_bbox(motion_predicted, frame_size_.width, frame_size_.height);
  } catch (...) {
    restore_prediction_checkpoint();
    hold_remaining_frames_ = 0;
    occlusion_coast_age_frames_ = 0;
    coast_probe_candidate_.reset();
    active_detector_candidate_.reset();
    if (uses_unified_recovery()) {
      invalidate_unified_recovery();
      cancel_probation();
    }
    motion_search_active_ = false;
    motion_.end_search();
    shadow_motion_.reset();
    tentative_control_.reset();
    clear_suspicion();
    reset_motion_input_gate();
    mode_ = mode_for_timestamp(timestamp_ms);
    throw;
  }
  if (hold_remaining_frames_ > 0) {
    mode_ = TrackingMode::kOcclusionHold;
    SearchPlan plan;
    plan.frame_id = frame_id;
    plan.timestamp_ms = timestamp_ms;
    plan.mode = mode_;
    plan.tracking_phase = TrackingPhase::kSearching;
    plan.observation_valid = false;
    plan.control_source = ControlSource::kMotionSearch;
    plan.motion_predicted_bbox = motion_predicted;
    plan.predicted_bbox = motion_predicted;
    plan.search_scale = search_scale_for(timestamp_mode, timestamp_ms);
    plan.hypothesis_source = HypothesisSource::kMotion;
    plan.detector_target_mode = config_.detector_target_mode;
    plan.hold_remaining_frames = hold_remaining_frames_;
    plan.occlusion_coast_age_frames = occlusion_coast_age_frames_ + 1;
    // Confirmation belongs to the immediately next processed frame. Decoded
    // frame IDs and timestamps need not be consecutive.
    if (coast_probe_candidate_ && coast_probe_candidate_->frame_id != last_frame_id_)
      coast_probe_candidate_.reset();
    if (coast_probe_candidate_) {
      plan.occlusion_probe_type = OcclusionProbeType::kConfirm;
    } else if (plan.occlusion_coast_age_frames %
                   static_cast<std::size_t>(config_.occlusion_probe_interval_frames) == 0) {
      plan.occlusion_probe_type = OcclusionProbeType::kSearch;
    }
    plan.run_tracker = plan.occlusion_probe_type != OcclusionProbeType::kNone;
    plan.motion_control_authorized = true;
    const MotionSnapshot snapshot = motion_.snapshot(timestamp_ms);
    plan.motion_search_age_ms = loss_start_ms_
        ? timestamp_ms - *loss_start_ms_ : 0;
    plan.motion_prediction_confident = true;
    plan.motion_velocity_x = snapshot.velocity_x;
    plan.motion_velocity_y = snapshot.velocity_y;
    plan.motion_acceleration_x = snapshot.acceleration_x;
    plan.motion_acceleration_y = snapshot.acceleration_y;
    plan.motion_scale_velocity_width = snapshot.scale_velocity_width;
    plan.motion_scale_velocity_height = snapshot.scale_velocity_height;
    active_plan_ = plan;
    frame_open_ = true;
    return plan;
  }

  coast_probe_candidate_.reset();
  mode_ = timestamp_mode;
  BBox predicted = motion_predicted;
  // Keep NanoTrack's next crop tied to the last accepted visual box while the
  // independent motion state coasts. This does not correct motion derivatives.
  if (motion_freeze_since_ms_ && mode_ == TrackingMode::kTracking &&
      !motion_search_active_) {
    predicted = current_bbox_;
  }
  HypothesisSource source = HypothesisSource::kMotion;
  const bool recovery_uses_detector = config_.motion_search_enabled
      ? motion_search_active_
      : mode_ == TrackingMode::kAssistedRecovery || mode_ == TrackingMode::kLost;
  const bool unified = uses_unified_recovery();
  const bool legacy_detector = recovery_uses_detector && !unified;
  if (legacy_detector && !active_detector_candidate_) {
    activate_next_detector_candidate();
  }
  if (legacy_detector && active_detector_candidate_) {
    ActiveDetectorCandidate& active = *active_detector_candidate_;
    if (!active.locked && active.next_probe >= active.queued.probes.size()) {
      active_detector_candidate_.reset();
      activate_next_detector_candidate();
    }
  }
  if (legacy_detector && active_detector_candidate_) {
    ActiveDetectorCandidate& active = *active_detector_candidate_;
    predicted = active.locked ? active.locked_bbox
                              : active.queued.probes[active.next_probe++];
    source = HypothesisSource::kDetector;
  }
  SearchPlan plan;
  const bool can_plan_probe = unified && loss_start_ms_ &&
      (config_.detector_target_mode != DetectorTargetMode::kSubtarget ||
       search_reacquisition_eligible(timestamp_ms));
  const RecoveryCandidate* probe = can_plan_probe ? recovery_candidates_.select_probe() : nullptr;
  if (probe) {
    predicted = probe->preferred_evidence().bbox;
    const auto& detector = probe->from(RecoveryCandidateSource::kDetector);
    if (detector && probe->subtarget_carrier &&
        !probe->from(RecoveryCandidateSource::kRecent) &&
        !probe->from(RecoveryCandidateSource::kSafe)) {
      const auto probes = detector_probes(*probe->subtarget_carrier);
      const auto index = std::min(probe->subtarget_probes_attempted, probes.size() - 1);
      predicted = probes[index];
      plan.detector_probe_index = index;
      plan.subtarget_probe_is_final = index + 1 == probes.size();
    }
    if (probe->key.source == RecoveryCandidateSource::kDetector) {
      source = HypothesisSource::kDetector;
    }
  }

  plan.frame_id = frame_id;
  plan.timestamp_ms = timestamp_ms;
  plan.mode = mode_;
  if (mode_ == TrackingMode::kSuspected) {
    plan.tracking_phase = TrackingPhase::kSuspected;
    plan.template_run_mode = TemplateRunMode::kRecentAndSafe;
  } else if (mode_ == TrackingMode::kTracking && !motion_search_active_) {
    plan.tracking_phase = TrackingPhase::kTracking;
    plan.template_run_mode = recent_probation_ ? TemplateRunMode::kStagedAndRecent
                                               : TemplateRunMode::kRecentOnly;
  } else {
    plan.tracking_phase = TrackingPhase::kSearching;
    plan.template_run_mode = TemplateRunMode::kRecentAndSafe;
  }
  // A plan is created before tracker inference and therefore never claims a
  // current-frame visual observation or observation-derived control.
  plan.observation_valid = false;
  plan.motion_control_authorized = search_control_live(timestamp_ms);
  plan.control_source = plan.motion_control_authorized
                            ? ControlSource::kMotionSearch
                            : ControlSource::kNone;
  const MotionSnapshot motion_snapshot = predictor.snapshot(timestamp_ms);
  plan.motion_search_age_ms = motion_snapshot.search_age_ms;
  plan.motion_prediction_confident =
      motion_snapshot.prediction_confident;
  plan.motion_velocity_x = motion_snapshot.velocity_x;
  plan.motion_velocity_y = motion_snapshot.velocity_y;
  plan.motion_acceleration_x = motion_snapshot.acceleration_x;
  plan.motion_acceleration_y = motion_snapshot.acceleration_y;
  plan.motion_scale_velocity_width =
      motion_snapshot.scale_velocity_width;
  plan.motion_scale_velocity_height =
      motion_snapshot.scale_velocity_height;
  plan.motion_predicted_bbox = motion_predicted;
  plan.predicted_bbox = predicted;
  plan.search_scale = source == HypothesisSource::kDetector
                          ? config_.detector_search_scale
                          : search_scale_for(mode_, timestamp_ms);
  plan.hypothesis_source = source;
  plan.detector_target_mode = config_.detector_target_mode;
  if (probe) {
    plan.recovery_probe = probe->key;
    if (probe->hits() > 0) {
      plan.search_hint = search_hint_for(*probe);
      plan.search_hint->used = true;
    }
    const auto& detector = probe->from(RecoveryCandidateSource::kDetector);
    if (detector) {
      plan.detector_candidate_id = probe->key.id;
      plan.detector_candidate_region = probe->subtarget_carrier.value_or(detector->bbox);
      plan.detector_candidate_class_id = detector->detector_class_id;
      plan.detector_candidate_confidence = detector->confidence;
      plan.detector_candidate_center_ratio = frame_normalized_center_distance(
          *plan.detector_candidate_region, motion_predicted, frame_size_);
      plan.detector_same_class_candidates = recovery_candidates_.detector_count();
      plan.detector_same_class_ambiguous = recovery_candidates_.conflicted();
      plan.detector_candidate_locked = probe->hits() > 0;
      if (!plan.subtarget_probe_is_final.has_value() && probe->subtarget_probes_attempted > 0) {
        plan.detector_probe_index = probe->subtarget_probes_attempted - 1;
      }
    }
  }
  if (active_detector_candidate_ && legacy_detector &&
      source == HypothesisSource::kDetector) {
    const ActiveDetectorCandidate& active = *active_detector_candidate_;
    plan.detector_candidate_id = active.queued.id;
    plan.detector_candidate_region = active.queued.candidate.bbox;
    plan.detector_candidate_class_id = active.queued.candidate.class_id;
    plan.detector_candidate_confidence = active.queued.candidate.confidence;
    plan.detector_candidate_center_ratio = active.queued.center_ratio;
    plan.detector_same_class_candidates =
        active.queued.same_class_candidates;
    plan.detector_same_class_ambiguous = active.queued.same_class_ambiguous;
    plan.detector_candidate_locked = active.locked;
    plan.detector_probe_index = active.locked ? active.locked_probe_index
                                              : active.next_probe - 1;
  }
  const bool interval_ready =
      !last_detector_request_ms_ ||
      timestamp_ms - *last_detector_request_ms_ >= config_.detector_interval_ms;
  if (detector_ready && recovery_uses_detector && interval_ready &&
      !rollback_pending_ && target_class_.target_class_allows_recovery) {
    plan.request_detector = true;
    plan.detector_request = DetectorRequest{
        frame_id,
        timestamp_ms,
        BBox{0.0F, 0.0F, static_cast<float>(frame_size_.width),
             static_cast<float>(frame_size_.height)},
        std::numeric_limits<std::size_t>::max(),
        detector_eligibility_generation_};
    last_detector_request_ms_ = timestamp_ms;
    last_detector_request_frame_ = frame_id;
    last_detector_request_motion_bbox_ = motion_predicted;
  }
  active_plan_ = plan;
  frame_open_ = true;
  return plan;
}

void LongTermController::abort_frame() noexcept {
  if (pending_template_transition_) {
    resolve_template_transition(pending_template_transition_->transaction_id, false);
  }
  if (!frame_open_) {
    pending_recovery_acceptance_.reset();
    return;
  }
  pending_recovery_acceptance_.reset();
  if (uses_unified_recovery()) invalidate_unified_recovery();
  cancel_probation();
  frame_open_ = false;
  restore_prediction_checkpoint();
  reset_motion_input_gate();
  if (active_plan_.mode == TrackingMode::kOcclusionHold) {
    hold_remaining_frames_ = 0;
    occlusion_coast_age_frames_ = 0;
    coast_probe_candidate_.reset();
    mode_ = mode_for_timestamp(active_plan_.timestamp_ms);
  }
}

void LongTermController::rollback_frame_application(
    LongTermController&& checkpoint) noexcept {
  static_assert(std::is_nothrow_move_assignable_v<LongTermController>);
  const auto transaction_high_water_mark = last_template_transaction_id_;
  *this = std::move(checkpoint);
  last_template_transaction_id_ = transaction_high_water_mark;
  abort_frame();
  discard_template_probation();
}

void LongTermController::publish_occlusion_evidence(ControllerOutput& output) {
  const auto& decision = occlusion_decision_;
  output.occlusion_reference_source = decision.reference_source;
  output.occlusion_reference_width = decision.reference_width;
  output.occlusion_reference_height = decision.reference_height;
  output.occlusion_width_ratio = decision.width_ratio;
  output.occlusion_height_ratio = decision.height_ratio;
  output.occlusion_suspicion_active = decision.suspicion_active;
  output.occlusion_suspicion_started = decision.suspicion_started;
  output.occlusion_suspicion_cleared = decision.suspicion_cleared;
  output.occlusion_suspicion_frames = occlusion_suspicion_frames_;
  output.occlusion_clean_frames = static_cast<std::size_t>(decision.clean_frames);
  output.occlusion_width_shrink_frames =
      static_cast<std::size_t>(decision.width_shrink_frames);
  output.occlusion_height_shrink_frames =
      static_cast<std::size_t>(decision.height_shrink_frames);
  output.occlusion_severe_evidence = decision.severe_evidence;
  output.occlusion_gradual_evidence = decision.gradual_evidence;
  output.occlusion_template_evidence = decision.template_evidence;
  output.occlusion_peak_margin_evidence = decision.peak_margin_evidence;
  output.occlusion_motion_residual_evidence = decision.motion_residual_evidence;
  output.occlusion_trigger_reason = decision.trigger_reason;
  // The reference and reason persist through coast and recovery; event pulses
  // belong only to the output frame on which they were first published.
  occlusion_decision_.suspicion_started = false;
  occlusion_decision_.suspicion_cleared = false;
}

void LongTermController::validate_output_semantics(
    const ControllerOutput& output) {
  if (output.tracking_phase == TrackingPhase::kInvalid) {
    throw std::logic_error("controller output has no tracking phase");
  }
  if (output.trusted_for_state_update != output.accepted ||
      (output.trusted_for_state_update && !output.observation_present)) {
    throw std::logic_error("trusted output requires an accepted observation");
  }
  if (output.motion_update_valid &&
      (!output.accepted || !output.observation_present ||
       output.tracking_phase != TrackingPhase::kTracking)) {
    throw std::logic_error(
        "motion update requires an accepted tracking observation");
  }
  if (output.observation_valid != output.accepted) {
    throw std::logic_error(
        "observation validity must match accepted tracker evidence");
  }
  if (output.control_valid ==
      (output.control_source == ControlSource::kNone)) {
    throw std::logic_error(
        "control validity must agree with a non-NONE control source");
  }
  if (output.tracking_phase == TrackingPhase::kTracking &&
      (!output.observation_valid || !output.control_valid ||
       output.control_source != ControlSource::kObservation)) {
    throw std::logic_error(
        "TRACKING requires accepted observation-derived control");
  }
  if (output.tracking_phase == TrackingPhase::kSuspected &&
      (!output.observation_present || output.trusted_for_state_update)) {
    throw std::logic_error("SUSPECTED requires a tentative observation");
  }
  if (output.tracking_phase == TrackingPhase::kSearching &&
      output.observation_present) {
    throw std::logic_error("SEARCHING cannot claim a valid observation");
  }
  if (output.control_source == ControlSource::kObservation &&
      (!output.observation_present ||
       (output.tracking_phase != TrackingPhase::kTracking &&
        output.tracking_phase != TrackingPhase::kSuspected))) {
    throw std::logic_error("OBSERVATION control requires a current observation");
  }
  if (output.control_source == ControlSource::kMotionSearch &&
      (output.tracking_phase != TrackingPhase::kSearching ||
       output.observation_valid || !output.motion_control_authorized)) {
    throw std::logic_error(
        "MOTION_SEARCH requires authorized observation-free search");
  }
}

void LongTermController::finalize_output_semantics(ControllerOutput& output) {
  if (output.accepted) {
    output.tracking_phase = TrackingPhase::kTracking;
    output.observation_valid = true;
    output.observation_present = true;
    output.trusted_for_state_update = true;
    output.control_source = ControlSource::kObservation;
  } else if (output.observation_present) {
    output.motion_update_valid = false;
    output.tracking_phase = TrackingPhase::kSuspected;
    output.control_source = output.control_valid ? ControlSource::kObservation
                                                 : ControlSource::kNone;
  } else {
    output.motion_update_valid = false;
    output.tracking_phase = TrackingPhase::kSearching;
    output.observation_valid = false;
    output.control_source = output.control_valid && output.motion_control_authorized
                                ? ControlSource::kMotionSearch
                                : ControlSource::kNone;
  }
  validate_output_semantics(output);
}

LongTermController::EvaluatedObservation LongTermController::evaluate(
    const TrackerObservation& observation,
    const SearchPlan& plan) const {
  if (observation.frame_id != plan.frame_id ||
      observation.timestamp_ms != plan.timestamp_ms) {
    throw std::invalid_argument("tracker observation frame identity mismatch");
  }
  validate_bbox(observation.bbox, frame_size_.width, frame_size_.height);
  const ResponseMetrics& metrics = observation.metrics;
  const float values[] = {metrics.raw_score, metrics.penalized_peak, metrics.psr,
                          metrics.apce, metrics.peak_margin};
  for (const float value : values) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("tracker observation metrics must be finite");
    }
  }

  EvaluatedObservation result;
  result.observation = observation;
  result.center_residual =
      normalized_center_distance(observation.bbox, plan.predicted_bbox);
  result.area_log_change = area_log_delta(observation.bbox, plan.predicted_bbox);
  result.aspect_log_change = aspect_log_delta(observation.bbox, plan.predicted_bbox);
  result.center_gate = result.center_residual <= config_.max_center_distance * plan.search_scale;
  result.area_gate = result.area_log_change <= config_.max_area_log_change;
  result.aspect_gate = result.aspect_log_change <= config_.max_aspect_log_change;
  result.score_gate = metrics.raw_score >= config_.reliable_score &&
                      metrics.psr >= config_.reliable_psr &&
                      metrics.apce >= config_.reliable_apce &&
                      metrics.peak_margin >= config_.reliable_peak_margin;
  const bool hard_gate = result.center_gate && result.area_gate && result.aspect_gate;
  bool decoded_collapse = false;
  if (observation.decoded_bbox) {
    const auto& decoded = *observation.decoded_bbox;
    const auto bounded =
        constrain_tracker_observation_dimensions(decoded, current_bbox_, frame_size_);
    decoded_collapse = decoded.width < bounded.width || decoded.height < bounded.height;
  }
  if (!hard_gate || decoded_collapse) {
    result.quality = QualityLevel::kRejected;
  } else if (result.score_gate) {
    result.quality = QualityLevel::kReliable;
  } else if (metrics.raw_score >= config_.weak_score &&
             metrics.psr >= config_.weak_psr &&
             metrics.apce >= config_.weak_apce &&
             metrics.peak_margin >= config_.weak_peak_margin) {
    result.quality = QualityLevel::kWeak;
  } else {
    result.quality = QualityLevel::kRejected;
  }
  return result;
}

void LongTermController::clear_suspicion() noexcept {
  response_degraded_since_ms_.reset();
  suspected_since_ms_.reset();
  response_clean_since_ms_.reset();
}

void LongTermController::promote_shadow_prediction() noexcept {
  if (shadow_motion_) {
    motion_ = std::move(*shadow_motion_);
    shadow_motion_.reset();
  }
}

void LongTermController::restore_prediction_checkpoint() noexcept {
  if (!prediction_checkpoint_) return;
  auto& checkpoint = *prediction_checkpoint_;
  motion_ = std::move(checkpoint.motion);
  shadow_motion_ = std::move(checkpoint.shadow_motion);
  tentative_control_ = checkpoint.tentative_control;
  response_degraded_since_ms_ = checkpoint.degraded_since_ms;
  suspected_since_ms_ = checkpoint.suspected_since_ms;
  response_clean_since_ms_ = checkpoint.clean_since_ms;
  motion_residual_history_ = std::move(checkpoint.motion_residual_history);
  motion_recent_residuals_ = std::move(checkpoint.motion_recent_residuals);
  motion_freeze_since_ms_ = checkpoint.motion_freeze_since_ms;
  motion_clean_since_ms_ = checkpoint.motion_clean_since_ms;
  motion_frozen_response_reference_ = checkpoint.motion_frozen_response_reference;
  motion_last_clean_trend_ = checkpoint.motion_last_clean_trend;
  motion_risk_streak_ = checkpoint.motion_risk_streak;
  motion_response_drop_since_ms_ = checkpoint.motion_response_drop_since_ms;
  mode_ = checkpoint.mode;
  prediction_checkpoint_.reset();
}

void LongTermController::complete_frame(const SearchPlan& plan,
                                        ControllerOutput& output) {
  output.motion_input_frozen = motion_freeze_since_ms_.has_value();
  if (motion_freeze_since_ms_) {
    output.motion_freeze_age_ms = plan.timestamp_ms - *motion_freeze_since_ms_;
    if (!output.accepted && std::string(output.motion_gate_reason) == "NONE") {
      output.motion_gate_reason = "FROZEN_NO_ACCEPTED_OBSERVATION";
    }
  }
  finalize_output_semantics(output);
  if (output.tracking_phase != TrackingPhase::kSuspected) tentative_control_.reset();
  if (output.tracking_phase == TrackingPhase::kSuspected) {
    if (last_output_phase_ != TrackingPhase::kSuspected) {
      output.state_change_reason = "RESPONSE_DEGRADED";
    } else if (last_output_mode_ != output.mode) {
      output.state_change_reason = "SUSPICION_ENTERED";
    } else {
      output.state_change_reason = "SUSPICION_CONTINUES";
    }
  } else if (output.accepted && last_output_phase_ == TrackingPhase::kSuspected) {
    output.state_change_reason = "SUSPICION_CLEARED";
  } else if (output.accepted && last_output_phase_ == TrackingPhase::kSearching) {
    output.state_change_reason = "OBSERVATION_CONFIRMED";
  } else if (output.tracking_phase == TrackingPhase::kSearching &&
             last_output_phase_ != TrackingPhase::kSearching) {
    output.state_change_reason = last_output_phase_ == TrackingPhase::kSuspected
        ? "SUSPICION_ESCALATED" : "OBSERVATION_LOST";
  } else if (output.mode != last_output_mode_) {
    output.state_change_reason = "SEARCH_MODE_CHANGED";
  }
  output.template_probation_active = recent_probation_;
  if (output.template_action != TemplateAction::kNone) {
    if (last_template_transaction_id_ == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error("template transaction IDs exhausted");
    }
    const auto id = ++last_template_transaction_id_;
    const bool stage = output.template_action == TemplateAction::kStageRecent;
    const bool recovery = stage ? pending_recovery_acceptance_ != nullptr
                                : recovery_probation_ || resume_search_after_rollback_;
    const auto slot = stage ? id : staged_template_slot_id_.value();
    pending_template_transition_ = TemplateTransitionRequest{
        id, output.template_action, output.tracking_bbox, recovery, slot};
    output.template_transition = pending_template_transition_;
    output.template_transition_result = TemplateTransitionResult::kPending;
    switch (output.template_action) {
      case TemplateAction::kStageRecent:
        output.template_transition_reason = recovery ? "RECOVERY_CONFIRMED" : "STABLE_TRACKING";
        break;
      case TemplateAction::kCommitRecent:
        output.template_transition_reason = "PROBATION_PASSED";
        break;
      case TemplateAction::kRollbackRecent:
        output.template_transition_reason = rollback_pending_ ? "FRAME_INVALIDATED" : "PROBATION_REJECTED";
        break;
      case TemplateAction::kNone: break;
    }
  }
  if (!pending_recovery_acceptance_) {
    last_output_mode_ = output.mode;
    last_output_phase_ = output.tracking_phase;
  }
  prediction_checkpoint_.reset();
  last_frame_id_ = plan.frame_id;
  last_timestamp_ms_ = plan.timestamp_ms;
  frame_open_ = false;
}

const char* to_string(TemplateTransitionResult result) {
  switch (result) {
    case TemplateTransitionResult::kNone: return "NONE";
    case TemplateTransitionResult::kPending: return "PENDING";
    case TemplateTransitionResult::kApplied: return "APPLIED";
    case TemplateTransitionResult::kPrepareFailed: return "PREPARE_FAILED";
    case TemplateTransitionResult::kTrackerRejected: return "TRACKER_REJECTED";
  }
  return "UNKNOWN";
}

void LongTermController::reset_template_probation() noexcept {
  staged_template_slot_id_.reset();
  recent_probation_ = false;
  recovery_probation_ = false;
  rollback_pending_ = false;
  probation_reliable_frames_ = 0;
  stable_frames_ = 0;
}

void LongTermController::discard_template_probation() noexcept {
  if (!staged_template_slot_id_) return;
  resume_search_after_rollback_ |= recovery_probation_;
  reset_template_probation();
  if (uses_unified_recovery()) invalidate_unified_recovery();
}

bool LongTermController::resolve_template_transition(
    std::uint64_t transaction_id, bool applied) noexcept {
  if (!pending_template_transition_ ||
      pending_template_transition_->transaction_id != transaction_id) return false;

  const auto request = *pending_template_transition_;
  pending_template_transition_.reset();
  if (request.action == TemplateAction::kStageRecent && applied) {
    if (request.recovery) apply_recovery_acceptance();
    staged_template_slot_id_ = request.template_slot_id;
    recent_probation_ = true;
    recovery_probation_ = request.recovery;
    response_baseline_.freeze();
    probation_reliable_frames_ = 0;
    stable_frames_ = 0;
  } else {
    reset_template_probation();
    if (request.action == TemplateAction::kCommitRecent && applied) {
      last_recent_update_ms_ = last_timestamp_ms_;
    } else if (request.recovery) {
      invalidate_unified_recovery();
      response_baseline_.freeze();
      if (request.action != TemplateAction::kStageRecent) {
        resume_search_after_rollback_ = true;
      }
    }
  }
  pending_recovery_acceptance_.reset();
  return true;
}

bool LongTermController::publish_template_transition(
    ControllerOutput& output, TemplateTransitionResult result) noexcept {
  if (!output.template_transition || !pending_template_transition_ ||
      result == TemplateTransitionResult::kNone || result == TemplateTransitionResult::kPending) {
    return false;
  }
  const auto& request = *output.template_transition;
  if (request.action != pending_template_transition_->action ||
      request.template_slot_id != pending_template_transition_->template_slot_id ||
      request.recovery != pending_template_transition_->recovery) return false;
  const bool applied = result == TemplateTransitionResult::kApplied;
  const auto freeze_since = motion_freeze_since_ms_;
  if (!resolve_template_transition(request.transaction_id, applied)) return false;

  output.template_transition_result = result;
  output.template_transition_applied = applied;
  output.template_probation_active = recent_probation_;
  if (request.recovery && request.action == TemplateAction::kStageRecent) {
    if (applied) {
      if (output.motion_input_frozen) {
        output.motion_freeze_cleared = true;
        output.motion_freeze_age_ms =
            output.timestamp_ms - freeze_since.value_or(output.timestamp_ms);
        output.motion_input_frozen = false;
        output.motion_gate_reason = "REACQUIRED";
      }
      output.mode = mode_;
      output.recovery_reason = "RECOVERY_STAGED";
      output.state_change_reason = "OBSERVATION_CONFIRMED";
    } else {
      const auto& motion_bbox = active_plan_.motion_predicted_bbox;
      output.mode = mode_;
      output.accepted = false;
      output.reacquired = false;
      output.observation_valid = false;
      output.observation_present = false;
      output.trusted_for_state_update = false;
      output.accepted_observation_path = AcceptedObservationPath::kDirect;
      output.tracking_phase = TrackingPhase::kSearching;
      output.tracking_bbox = motion_bbox;
      output.display_bbox = motion_bbox;
      output.control_bbox = motion_bbox;
      output.bbox = motion_bbox;
      output.motion_control_authorized = search_control_live(output.timestamp_ms);
      output.control_valid = output.motion_control_authorized;
      output.control_source = output.control_valid ? ControlSource::kMotionSearch
                                                   : ControlSource::kNone;
      output.selected_observation_bbox.reset();
      output.center_innovation_x = output.center_innovation_y = 0.0F;
      output.center_correction_x = output.center_correction_y = 0.0F;
      output.center_correction_limited = false;
      output.state_change_reason = "RECOVERY_PREPARE_FAILED";
      output.recovery_reason = "RECOVERY_PREPARE_FAILED";
      last_output_mode_ = output.mode;
      last_output_phase_ = output.tracking_phase;
    }
  } else if (request.recovery) {
    if (request.action == TemplateAction::kCommitRecent) {
      output.recovery_reason = applied ? "RECOVERY_COMMITTED" : "RECOVERY_COMMIT_REJECTED";
    } else {
      output.recovery_reason = "RECOVERY_PROBATION_REJECTED";
    }
  }
  return true;
}

float LongTermController::trusted_elapsed_seconds(std::int64_t timestamp_ms) const {
  return static_cast<float>(
      std::min(timestamp_ms - last_trusted_timestamp_ms_, config_.motion_max_dt_ms)) /
      1000.0F;
}

bool LongTermController::trusted_control_live(std::int64_t timestamp_ms) const {
  return timestamp_ms - last_trusted_timestamp_ms_ <= config_.motion_search_confidence_ms;
}

bool LongTermController::search_control_live(std::int64_t timestamp_ms) const {
  return config_.motion_search_enabled && motion_search_active_ &&
         mode_for_timestamp(timestamp_ms) != TrackingMode::kLost;
}

LongTermController::RecentDisposition LongTermController::assess_recent(
    const SearchPlan& plan, const EvaluatedObservation& recent,
    const ResponseBaselineEvaluation& response) {
  const auto& observation = recent.observation;
  const auto& metrics = observation.metrics;
  const auto bounded =
      constrain_tracker_observation_dimensions(observation.bbox, current_bbox_, frame_size_);
  if (observation.bbox.width < bounded.width ||
      observation.bbox.height < bounded.height) {
    return RecentDisposition::kSearch;
  }

  const int reliable_failures =
      static_cast<int>(metrics.raw_score < config_.reliable_score) +
      static_cast<int>(metrics.psr < config_.reliable_psr) +
      static_cast<int>(metrics.apce < config_.reliable_apce) +
      static_cast<int>(metrics.peak_margin < config_.reliable_peak_margin);
  const int weak_failures =
      static_cast<int>(metrics.raw_score < config_.weak_score) +
      static_cast<int>(metrics.psr < config_.weak_psr) +
      static_cast<int>(metrics.apce < config_.weak_apce) +
      static_cast<int>(metrics.peak_margin < config_.weak_peak_margin);
  const bool isolated_response_dip =
      reliable_failures == 1 && weak_failures <= 1;
  if (recent.quality == QualityLevel::kRejected && !isolated_response_dip) {
    return RecentDisposition::kSearch;
  }

  const int drops = static_cast<int>(response.psr_drop) +
                    static_cast<int>(response.apce_drop) +
                    static_cast<int>(response.peak_margin_drop);
  const bool motion_inconsistent =
      recent.center_residual >= config_.occlusion_motion_residual_threshold;
  const bool response_weak = metrics.psr < config_.reliable_psr ||
                             metrics.apce < config_.reliable_apce ||
                             metrics.peak_margin < config_.reliable_peak_margin;
  const bool degraded = isolated_response_dip || drops >= 2 ||
                        (motion_inconsistent && (drops > 0 || response_weak));
  if (degraded || (suspected_since_ms_ && recent.quality != QualityLevel::kReliable)) {
    response_clean_since_ms_.reset();
    if (!response_degraded_since_ms_) response_degraded_since_ms_ = plan.timestamp_ms;
    if (!suspected_since_ms_ &&
        plan.timestamp_ms - *response_degraded_since_ms_ >= config_.suspicion_enter_ms) {
      suspected_since_ms_ = plan.timestamp_ms;
    }
    // Reuse the uncertainty budget after entering SUSPECTED. An intervening
    // clean sample restarts the duration of continuous degradation.
    if (suspected_since_ms_ &&
        plan.timestamp_ms - std::max(*suspected_since_ms_, *response_degraded_since_ms_) >=
            config_.uncertain_ms) {
      return RecentDisposition::kSearch;
    }
    return RecentDisposition::kTentative;
  }

  response_degraded_since_ms_.reset();
  if (recent.quality != QualityLevel::kReliable) return RecentDisposition::kSearch;
  if (!suspected_since_ms_) return RecentDisposition::kDirect;
  if (!response_clean_since_ms_) response_clean_since_ms_ = plan.timestamp_ms;
  if (plan.timestamp_ms - *response_clean_since_ms_ < config_.suspicion_clear_ms) {
    return RecentDisposition::kTentative;
  }
  return RecentDisposition::kDirect;
}

BBox LongTermController::bound_recent_scale(const BBox& bbox,
                                           std::int64_t timestamp_ms) const {
  const float dt = trusted_elapsed_seconds(timestamp_ms);
  const float step = std::min(config_.max_accepted_scale_step_ratio,
                              config_.motion_scale_rate_limit_per_s * dt);
  const BBox bounded = constrain_tracker_observation_dimensions(
      bbox, current_bbox_, frame_size_);
  return constrain_bbox_dimensions(
      bounded, current_bbox_, std::max(0.5F, 1.0F - step), 1.0F + step,
      frame_size_.width, frame_size_.height);
}

BBox LongTermController::constrain_tentative_control(
    const BBox& observation, std::int64_t timestamp_ms) const {
  const auto previous = tentative_control_.value_or(
      TentativeControl{current_bbox_, last_trusted_timestamp_ms_});
  const double seconds = static_cast<double>(std::min(
      timestamp_ms - previous.timestamp_ms, config_.motion_max_dt_ms)) / 1000.0;
  const double step = std::min(
      static_cast<double>(config_.max_accepted_scale_step_ratio),
      config_.motion_scale_rate_limit_per_s * seconds);
  const double minimum_width = previous.bbox.width * std::max(0.5, 1.0 - step);
  const double minimum_height = previous.bbox.height * std::max(0.5, 1.0 - step);
  const double previous_x = previous.bbox.x + 0.5 * previous.bbox.width;
  const double previous_y = previous.bbox.y + 0.5 * previous.bbox.height;
  double dx = observation.x + 0.5 * observation.width - previous_x;
  double dy = observation.y + 0.5 * observation.height - previous_y;
  const double diagonal = std::hypot(frame_size_.width, frame_size_.height);
  const double speed_limit =
      config_.motion_max_speed_diagonals_per_s * diagonal * seconds;
  const double correction_ratio = config_.accepted_center_max_correction_ratio;
  double center_limit = speed_limit;
  if (correction_ratio > 0.0) {
    // Any feasible final size is at least this large, including at the edge.
    center_limit = std::min(center_limit,
                            correction_ratio * std::sqrt(minimum_width * minimum_height));
  }
  const double distance = std::hypot(dx, dy);
  if (distance > center_limit) {
    dx *= center_limit / distance;
    dy *= center_limit / distance;
  }

  // This rectangle contains the preceding center, so projecting onto it
  // cannot increase the displacement. Fit the dimensions at that center.
  const double x = std::clamp(previous_x + dx, 0.5 * minimum_width,
                              frame_size_.width - 0.5 * minimum_width);
  const double y = std::clamp(previous_y + dy, 0.5 * minimum_height,
                              frame_size_.height - 0.5 * minimum_height);
  const double available_width = 2.0 * std::min(x, frame_size_.width - x);
  const double available_height = 2.0 * std::min(y, frame_size_.height - y);
  const double maximum_width = previous.bbox.width * (1.0 + step);
  const double maximum_height = previous.bbox.height * (1.0 + step);
  const auto bounded = constrain_tracker_observation_dimensions(
      observation, current_bbox_, frame_size_);
  const auto fit_dimension = [](float observed, double minimum, double maximum,
                                double available) {
    const double upper = std::max(minimum, std::min(maximum, available));
    float size = static_cast<float>(
        std::clamp(static_cast<double>(observed), minimum, upper));
    if (size < minimum) size = std::nextafter(size, std::numeric_limits<float>::infinity());
    if (size > upper) size = std::nextafter(size, 0.0F);
    return size;
  };
  const float width = fit_dimension(bounded.width, minimum_width, maximum_width,
                                    available_width);
  const float height = fit_dimension(bounded.height, minimum_height, maximum_height,
                                     available_height);
  const BBox result = centered_box(static_cast<float>(x), static_cast<float>(y),
                                   BBox{0.0F, 0.0F, width, height}, frame_size_);
  const double emitted_distance = std::hypot(
      result.x + 0.5 * result.width - previous_x,
      result.y + 0.5 * result.height - previous_y);
  // A float conversion or the final frame clip can move the center or size.
  // Keep the preceding box if the emitted geometry cannot satisfy the limits.
  if (emitted_distance > speed_limit ||
      (correction_ratio > 0.0 &&
       emitted_distance > correction_ratio * std::sqrt(
           static_cast<double>(result.width) * result.height)) ||
      result.width < minimum_width || result.width > maximum_width ||
      result.height < minimum_height || result.height > maximum_height) {
    return previous.bbox;
  }
  return result;
}

void LongTermController::publish_tentative(
    const SearchPlan& plan, const EvaluatedObservation& recent,
    ControllerOutput& output) {
  motion_risk_streak_ = 0;
  motion_response_drop_since_ms_.reset();
  motion_recent_residuals_.clear();
  motion_clean_since_ms_.reset();
  if (!shadow_motion_) {
    shadow_motion_.emplace(std::move(motion_));
    motion_ = prediction_checkpoint_.value().motion;
  }
  response_baseline_.freeze();

  const auto& bbox = recent.observation.bbox;
  output.selected_template = recent.observation.template_kind;
  output.selected_observation_bbox = bbox;
  output.selected_metrics = recent.observation.metrics;
  output.quality = recent.quality;
  output.normalized_center_residual = recent.center_residual;
  output.area_log_change = recent.area_log_change;
  output.aspect_log_change = recent.aspect_log_change;
  output.tracking_bbox = bbox;
  output.control_bbox = constrain_tentative_control(bbox, plan.timestamp_ms);
  tentative_control_ = TentativeControl{output.control_bbox, plan.timestamp_ms};
  output.display_bbox = stabilize_accepted_center(
      plan, bbox, output.control_bbox, false, output);
  output.bbox = output.display_bbox;
  output.observation_present = true;
  output.control_valid = config_.motion_search_enabled ||
                         trusted_control_live(plan.timestamp_ms);
  mode_ = mode_for_timestamp(plan.timestamp_ms);
  output.mode = mode_;
  if (recent_probation_ && !recovery_probation_) {
    output.template_action = TemplateAction::kRollbackRecent;
  }
}

bool LongTermController::add_confirmation(std::uint64_t frame_id,
                                          const BBox& bbox) {
  confirmations_.push_back(Confirmation{frame_id, bbox});
  while (!confirmations_.empty() &&
         frame_id - confirmations_.front().frame_id >=
             static_cast<std::uint64_t>(config_.confirmation_window)) {
    confirmations_.pop_front();
  }
  int consistent = 0;
  for (const Confirmation& candidate : confirmations_) {
    const bool spatially_consistent =
        bbox_iou(candidate.bbox, bbox) >= config_.template_agreement_iou ||
        normalized_center_distance(candidate.bbox, bbox) <= 0.5F;
    const bool shape_consistent =
        area_log_delta(candidate.bbox, bbox) <= config_.max_area_log_change &&
        aspect_log_delta(candidate.bbox, bbox) <= config_.max_aspect_log_change;
    if (spatially_consistent && shape_consistent) ++consistent;
  }
  return consistent >= config_.confirmation_required;
}

bool LongTermController::uses_unified_recovery() const noexcept {
  return config_.motion_search_enabled || recent_safe_observations_;
}

bool LongTermController::search_reacquisition_eligible(
    std::int64_t timestamp_ms) const noexcept {
  return loss_start_ms_ && !rollback_pending_ &&
         timestamp_ms - *loss_start_ms_ >= config_.search_reacquisition_min_age_ms;
}

void LongTermController::invalidate_unified_recovery() noexcept {
  recovery_candidates_.clear();
  ++detector_eligibility_generation_;
  last_detector_request_frame_.reset();
  last_detector_request_motion_bbox_.reset();
  last_consumed_detector_frame_.reset();
}

void LongTermController::cancel_probation() noexcept {
  if (!recent_probation_) return;
  rollback_pending_ = true;
  resume_search_after_rollback_ = recovery_probation_;
  recent_probation_ = false;
  recovery_probation_ = false;
  probation_reliable_frames_ = 0;
  stable_frames_ = 0;
}

SearchHint LongTermController::search_hint_for(
    const RecoveryCandidate& candidate) const {
  const auto& evidence = candidate.preferred_evidence();
  const auto& first_hit = candidate.verifications[0]
      ? candidate.verifications[0] : candidate.verifications[1];
  SearchHint hint;
  hint.candidate_id = candidate.key.id;
  hint.source_frame_id = evidence.frame_id;
  hint.source_timestamp_ms = evidence.timestamp_ms;
  hint.projected_bbox = evidence.bbox;
  hint.generation = candidate.key.generation;
  hint.first_hit_frame = first_hit ? first_hit->frame_id : 0;
  hint.confirmation_count = candidate.hits();
  return hint;
}

std::optional<LongTermController::EvaluatedObservation>
LongTermController::confirm_recovery_candidate(
    const SearchPlan& plan,
    const std::optional<EvaluatedObservation>& recent,
    const std::optional<EvaluatedObservation>& safe,
    ControllerOutput& output) {
  if (!search_reacquisition_eligible(plan.timestamp_ms)) {
    output.recovery_reason = "SEARCH_MIN_AGE";
    return std::nullopt;
  }
  const auto* planned_candidate = plan.recovery_probe
      ? recovery_candidates_.find(*plan.recovery_probe) : nullptr;
  if (plan.recovery_probe && !planned_candidate) {
    if (output.search_hint) output.search_hint->rejection = SearchHintRejection::kExpired;
    output.recovery_reason = "PROBE_EXPIRED";
    return std::nullopt;
  }
  const auto carrier = planned_candidate ? planned_candidate->subtarget_carrier : std::nullopt;
  const bool probe_verified = planned_candidate && planned_candidate->hits() > 0;
  const auto reliable_in_carrier = [&](const std::optional<EvaluatedObservation>& observation) {
    if (!carrier || !observation ||
        !contains_center(*carrier, observation->observation.bbox)) {
      return false;
    }
    if (observation->quality == QualityLevel::kReliable) return true;
    if (!observation->score_gate || !observation->area_gate || !observation->aspect_gate) {
      return false;
    }
    // Evaluate the same response and shape gates without letting the planned
    // part's center hide contradictory evidence from another part.
    auto recentered = plan;
    const auto& bbox = observation->observation.bbox;
    recentered.predicted_bbox = centered_box(
        bbox.x + 0.5F * bbox.width, bbox.y + 0.5F * bbox.height,
        plan.predicted_bbox, frame_size_);
    return evaluate(observation->observation, recentered).quality == QualityLevel::kReliable;
  };
  const bool carrier_disagreement = reliable_in_carrier(recent) && reliable_in_carrier(safe) &&
      !recovery_candidates_.geometry_consistent(recent->observation.bbox, safe->observation.bbox);
  const bool probe_conflict = carrier_disagreement;
  const auto observe = [&](RecoveryCandidateSource source,
                           const std::optional<EvaluatedObservation>& observation) {
    if (!observation) return std::optional<RecoveryCandidateKey>{};
    const BBox& bbox = observation->observation.bbox;
    const bool consistent_with_probe = !plan.recovery_probe ||
        recovery_candidates_.geometry_consistent(plan.predicted_bbox, bbox);
    if (!consistent_with_probe && carrier && !probe_verified && contains_center(*carrier, bbox)) {
      // This response may be a different part of the same detector region.
      // Wait for a matching offset probe before admitting it as evidence.
      return std::optional<RecoveryCandidateKey>{};
    }
    // An off-probe hit can reveal a competing identity at the motion center
    // even if the chosen probe had a narrower crop than the general search.
    if (observation->quality != QualityLevel::kReliable) {
      if (consistent_with_probe || !plan.recovery_probe)
        return std::optional<RecoveryCandidateKey>{};
      auto motion_plan = plan;
      motion_plan.predicted_bbox = plan.motion_predicted_bbox;
      motion_plan.search_scale = search_scale_for(plan.mode, plan.timestamp_ms);
      if (evaluate(observation->observation, motion_plan).quality != QualityLevel::kReliable)
        return std::optional<RecoveryCandidateKey>{};
    }
    return recovery_candidates_.observe(source, bbox, 0.0F, std::nullopt,
        std::nullopt, !carrier_disagreement,
        frame_normalized_center_distance(bbox, plan.motion_predicted_bbox, frame_size_));
  };
  const auto recent_key = observe(RecoveryCandidateSource::kRecent, recent);
  const auto safe_key = observe(RecoveryCandidateSource::kSafe, safe);
  if (probe_conflict) recovery_candidates_.mark_identity_conflict();
  output.recovery_conflict = probe_conflict || recovery_candidates_.conflicted();
  const bool recent_confirmed = !output.recovery_conflict && recent_key &&
      recovery_candidates_.confirmed(*recent_key);
  const bool safe_confirmed = !output.recovery_conflict && safe_key &&
      recovery_candidates_.confirmed(*safe_key);
  const auto* candidate = recent_confirmed ? recovery_candidates_.find(*recent_key) :
      safe_confirmed ? recovery_candidates_.find(*safe_key) : recovery_candidates_.select_probe();
  if (candidate) {
    output.recovery_candidate_key = candidate->key;
    output.recovery_confirmation_hits = candidate->hits();
    if (candidate->hits() > 0) {
      output.search_hint = search_hint_for(*candidate);
      output.search_hint->used = candidate->key == plan.recovery_probe;
    }
    if (candidate->from(RecoveryCandidateSource::kDetector)) {
      output.detector_candidate_id = candidate->key.id;
      output.detector_same_class_candidates = recovery_candidates_.detector_count();
      output.detector_same_class_ambiguous = output.recovery_conflict;
      output.detector_candidate_locked = candidate->hits() > 0;
      output.detector_confirmation_hits = candidate->hits();
    }
  }
  if (output.recovery_conflict) {
    output.recovery_reason = "IDENTITY_CONFLICT";
    if (output.search_hint) output.search_hint->rejection = SearchHintRejection::kConflict;
    return std::nullopt;
  }
  if (recent_confirmed || safe_confirmed) {
    output.recovery_reason = "RECOVERY_CONFIRMED";
    return recent_confirmed ? recent : safe;
  }
  output.recovery_reason = candidate ? "AWAITING_CONFIRMATION" : "NO_ELIGIBLE_CANDIDATE";
  return std::nullopt;
}

std::vector<BBox> LongTermController::detector_probes(
    const BBox& region) const {
  if (config_.detector_target_mode == DetectorTargetMode::kFullObject) {
    return {region};
  }

  static constexpr float kProbeCoordinates[][2] = {
      {0.50F, 0.50F}, {0.50F, 0.20F}, {0.50F, 0.80F},
      {0.20F, 0.50F}, {0.80F, 0.50F}, {0.20F, 0.20F},
      {0.80F, 0.20F}, {0.20F, 0.80F}, {0.80F, 0.80F},
  };
  std::vector<BBox> probes;
  probes.reserve(config_.detector_subtarget_max_probes);
  for (const auto& coordinate : kProbeCoordinates) {
    if (probes.size() >= config_.detector_subtarget_max_probes) break;
    const float center_x = region.x + coordinate[0] * region.width;
    const float center_y = region.y + coordinate[1] * region.height;
    const BBox probe = centered_box(center_x, center_y, current_bbox_, frame_size_);
    const bool duplicate = std::any_of(
        probes.begin(), probes.end(), [&](const BBox& existing) {
          return std::fabs(existing.x - probe.x) < 1e-4F &&
                 std::fabs(existing.y - probe.y) < 1e-4F;
        });
    if (!duplicate) probes.push_back(probe);
  }
  return probes;
}

void LongTermController::activate_next_detector_candidate() {
  if (active_detector_candidate_ || detector_candidates_.empty()) return;
  ActiveDetectorCandidate active;
  active.queued = std::move(detector_candidates_.front());
  detector_candidates_.pop_front();
  active_detector_candidate_ = std::move(active);
  confirmations_.clear();
}

bool LongTermController::detector_confirmation_consistent(
    const BBox& bbox) const {
  if (!active_detector_candidate_ ||
      active_detector_candidate_->confirmation_boxes.empty()) {
    return true;
  }
  const BBox& reference =
      active_detector_candidate_->confirmation_boxes.front();
  return detector_geometry_consistent(reference, bbox);
}

bool LongTermController::detector_geometry_consistent(
    const BBox& reference, const BBox& bbox) const {
  return bbox_iou(reference, bbox) >= config_.detector_confirmation_iou &&
         normalized_center_distance(bbox, reference) <=
             config_.detector_confirmation_center_distance &&
         area_log_delta(bbox, reference) <=
             config_.detector_confirmation_max_area_log_change &&
         aspect_log_delta(bbox, reference) <=
             config_.detector_confirmation_max_aspect_log_change;
}

void LongTermController::reset_scale_state(const BBox& bbox) {
  width_scale_state_ = DimensionScaleState{};
  width_scale_state_.filtered = bbox.width;
  width_scale_state_.emitted = bbox.width;
  height_scale_state_ = DimensionScaleState{};
  height_scale_state_.filtered = bbox.height;
  height_scale_state_.emitted = bbox.height;
}

void LongTermController::reset_scale_evidence(DimensionScaleState& state) {
  state.samples.clear();
  state.direction = 0;
  state.confirmations = 0;
}

BBox LongTermController::stabilize_accepted_bbox(
    const BBox& observation, DimensionScaleState& width_state,
    DimensionScaleState& height_state) const {
  const float ratio = config_.max_accepted_scale_step_ratio;
  const float deadband = config_.accepted_scale_deadband_ratio;
  const float alpha = config_.accepted_scale_smoothing_alpha;
  const bool compatibility_mode =
      config_.accepted_scale_median_window_frames == 1 &&
      config_.accepted_scale_direction_confirm_frames == 1 &&
      config_.accepted_scale_output_hysteresis_px == 0.0F;
  const auto legacy_dimension = [&](float observed, float current,
                                    float frame_limit,
                                    DimensionScaleState& state) {
    if (ratio >= 1.0F && deadband == 0.0F && alpha >= 1.0F) {
      state.samples.clear();
      state.samples.push_back(observed);
      state.filtered = observed;
      state.emitted = observed;
      state.direction = 0;
      state.confirmations = 0;
      return observed;
    }
    const float allowance = current * deadband;
    float emitted = current;
    if (observed < current - allowance || observed > current + allowance) {
      const float target = std::clamp(
          observed, current * (1.0F - ratio),
          std::min(frame_limit, current * (1.0F + ratio)));
      emitted = current + alpha * (target - current);
    }
    state.samples.clear();
    state.samples.push_back(observed);
    state.filtered = emitted;
    state.emitted = emitted;
    state.direction = 0;
    state.confirmations = 0;
    return emitted;
  };
  const auto temporal_dimension = [&](float observed, float frame_limit,
                                      DimensionScaleState& state) {
    const auto emit_toward_filtered = [&]() {
      const float hysteresis = config_.accepted_scale_output_hysteresis_px;
      const float delta = state.filtered - state.emitted;
      if (std::fabs(delta) < hysteresis) return state.emitted;
      const float step = hysteresis > 0.5F
          ? (delta > 0.0F ? 1.0F : delta < 0.0F ? -1.0F : 0.0F)
          : std::clamp(delta, -1.0F, 1.0F);
      state.emitted = std::clamp(state.emitted + step,
                                 std::numeric_limits<float>::epsilon(),
                                 frame_limit);
      return state.emitted;
    };
    state.samples.push_back(observed);
    const auto window = static_cast<std::size_t>(
        config_.accepted_scale_median_window_frames);
    while (state.samples.size() > window) state.samples.pop_front();
    if (state.samples.size() < window) return state.emitted;

    std::vector<float> ordered(state.samples.begin(), state.samples.end());
    const auto middle = ordered.begin() + ordered.size() / 2;
    std::nth_element(ordered.begin(), middle, ordered.end());
    const float median = *middle;
    const float allowance = state.filtered * deadband;
    if (median >= state.filtered - allowance &&
        median <= state.filtered + allowance) {
      state.direction = 0;
      state.confirmations = 0;
      return emit_toward_filtered();
    }

    const int direction = median > state.filtered ? 1 : -1;
    if (direction != state.direction) {
      state.direction = direction;
      state.confirmations = 1;
    } else if (state.confirmations <
               config_.accepted_scale_direction_confirm_frames) {
      ++state.confirmations;
    }
    if (state.confirmations <
        config_.accepted_scale_direction_confirm_frames) {
      return state.emitted;
    }

    const float target = std::clamp(
        median, state.filtered * (1.0F - ratio),
        std::min(frame_limit, state.filtered * (1.0F + ratio)));
    state.filtered += alpha * (target - state.filtered);
    return emit_toward_filtered();
  };

  float width;
  float height;
  if (compatibility_mode) {
    width = legacy_dimension(observation.width, current_bbox_.width,
                             static_cast<float>(frame_size_.width), width_state);
    height = legacy_dimension(observation.height, current_bbox_.height,
                              static_cast<float>(frame_size_.height), height_state);
  } else {
    width = temporal_dimension(observation.width,
                               static_cast<float>(frame_size_.width), width_state);
    height = temporal_dimension(observation.height,
                                static_cast<float>(frame_size_.height), height_state);
  }
  const float center_x = observation.x + 0.5F * observation.width;
  const float center_y = observation.y + 0.5F * observation.height;
  return centered_box(center_x, center_y, BBox{0.0F, 0.0F, width, height},
                      frame_size_);
}

BBox LongTermController::stabilize_accepted_center(
    const SearchPlan& plan, const BBox& observation, const BBox& scaled_bbox,
    bool reacquired, ControllerOutput& output) const {
  output.selected_observation_bbox = observation;
  const float predicted_center_x =
      plan.motion_predicted_bbox.x + 0.5F * plan.motion_predicted_bbox.width;
  const float predicted_center_y =
      plan.motion_predicted_bbox.y + 0.5F * plan.motion_predicted_bbox.height;
  const float observed_center_x = observation.x + 0.5F * observation.width;
  const float observed_center_y = observation.y + 0.5F * observation.height;
  output.center_innovation_x = observed_center_x - predicted_center_x;
  output.center_innovation_y = observed_center_y - predicted_center_y;

  const float display_center_x =
      display_bbox_.x + 0.5F * display_bbox_.width;
  const float display_center_y =
      display_bbox_.y + 0.5F * display_bbox_.height;
  float bounded_x = observed_center_x - display_center_x;
  float bounded_y = observed_center_y - display_center_y;
  if (!reacquired && config_.accepted_center_max_correction_ratio > 0.0F) {
    const float maximum = config_.accepted_center_max_correction_ratio *
                          std::sqrt(scaled_bbox.width * scaled_bbox.height);
    const float magnitude = std::hypot(bounded_x, bounded_y);
    if (magnitude > maximum) {
      const float factor = maximum / magnitude;
      bounded_x *= factor;
      bounded_y *= factor;
      output.center_correction_limited = true;
    }
  }

  const float alpha = reacquired ? 1.0F
                                 : config_.accepted_center_smoothing_alpha;
  const float requested_center_x = display_center_x + alpha * bounded_x;
  const float requested_center_y = display_center_y + alpha * bounded_y;
  const BBox stabilized = centered_box(
      requested_center_x, requested_center_y,
      BBox{0.0F, 0.0F, scaled_bbox.width, scaled_bbox.height}, frame_size_);
  output.center_correction_x =
      stabilized.x + 0.5F * stabilized.width - display_center_x;
  output.center_correction_y =
      stabilized.y + 0.5F * stabilized.height - display_center_y;
  return stabilized;
}

void LongTermController::defer_recovery_acceptance(
    const SearchPlan& plan, const EvaluatedObservation& selected,
    ControllerOutput& output) {
  auto recovery = std::make_shared<RecoveryAcceptance>(config_);
  const BBox bbox = selected.observation.bbox;
  recovery->bbox = bbox;
  recovery->display_bbox = stabilize_accepted_center(plan, bbox, bbox, true, output);
  recovery->motion.initialize(bbox, plan.timestamp_ms, frame_size_.width, frame_size_.height);
  recovery->occlusion_decision = recovery->occlusion_evidence.observe(
      OcclusionEvidenceInput{bbox, bbox, selected.observation.metrics, true, false, false});
  recovery->width_scale.filtered = recovery->width_scale.emitted = bbox.width;
  recovery->height_scale.filtered = recovery->height_scale.emitted = bbox.height;
  output.tracking_bbox = bbox;
  output.display_bbox = recovery->display_bbox;
  output.control_bbox = bbox;
  output.bbox = output.display_bbox;
  pending_recovery_acceptance_ = std::move(recovery);
  output.accepted = true;
  output.reacquired = true;
  output.motion_update_valid = true;
  output.control_valid = true;
  output.accepted_observation_path = AcceptedObservationPath::kSearchReacquisition;
  output.template_action = TemplateAction::kStageRecent;
  output.mode = TrackingMode::kTracking;
  output.motion_control_authorized = false;
}

void LongTermController::apply_recovery_acceptance() noexcept {
  auto& recovery = *pending_recovery_acceptance_;
  static_assert(std::is_nothrow_move_assignable_v<MotionEstimator>);
  static_assert(std::is_nothrow_move_assignable_v<OcclusionEvidenceTracker>);
  static_assert(std::is_nothrow_move_assignable_v<DimensionScaleState>);
  motion_ = std::move(recovery.motion);
  occlusion_evidence_ = std::move(recovery.occlusion_evidence);
  occlusion_decision_ = recovery.occlusion_decision;
  width_scale_state_ = std::move(recovery.width_scale);
  height_scale_state_ = std::move(recovery.height_scale);
  current_bbox_ = recovery.bbox;
  display_bbox_ = recovery.display_bbox;
  last_trusted_timestamp_ms_ = last_timestamp_ms_;
  motion_search_active_ = false;
  loss_start_ms_.reset();
  hold_used_for_loss_ = false;
  hold_remaining_frames_ = 0;
  occlusion_coast_age_frames_ = 0;
  coast_probe_candidate_.reset();
  frozen_scale_reference_.reset();
  occlusion_suspicion_frames_ = 0;
  response_baseline_.reset_after_recovery();
  reset_motion_input_gate();
  motion_last_clean_trend_ = motion_.snapshot(last_timestamp_ms_);
  search_motion_reference_ = motion_.snapshot(last_timestamp_ms_);
  search_motion_reference_held_ = false;
  search_motion_clean_since_ms_.reset();
  confirmations_.clear();
  active_detector_candidate_.reset();
  detector_candidates_.clear();
  invalidate_unified_recovery();
  mode_ = TrackingMode::kTracking;
  last_output_mode_ = mode_;
  last_output_phase_ = TrackingPhase::kTracking;
  consecutive_unreliable_frames_ = 0;
}

void LongTermController::reset_motion_input_gate() noexcept {
  motion_residual_history_.clear();
  motion_recent_residuals_.clear();
  motion_freeze_since_ms_.reset();
  motion_clean_since_ms_.reset();
  motion_frozen_response_reference_.reset();
  motion_last_clean_trend_.reset();
  motion_risk_streak_ = 0;
  motion_response_drop_since_ms_.reset();
}

bool LongTermController::motion_input_allows_correction(
    const SearchPlan& plan, const EvaluatedObservation& selected,
    const ResponseBaselineEvaluation& response, ControllerOutput& output) {
  if (selected.observation.template_kind != TemplateKind::kRecent) {
    motion_risk_streak_ = 0;
    return !motion_freeze_since_ms_;
  }
  if (!response.reference && !motion_frozen_response_reference_) {
    motion_residual_history_.push_back(selected.center_residual);
    if (motion_residual_history_.size() >
        static_cast<std::size_t>(config_.occlusion_reference_window)) {
      motion_residual_history_.erase(motion_residual_history_.begin());
    }
    motion_risk_streak_ = 0;
    motion_response_drop_since_ms_.reset();
    return !motion_freeze_since_ms_;
  }
  const auto& metrics = selected.observation.metrics;
  const ResponseReference& reference = motion_frozen_response_reference_
      ? *motion_frozen_response_reference_ : *response.reference;
  const bool psr_drop = reference.psr > 0.0F &&
      metrics.psr <= reference.psr * (1.0F - config_.response_psr_drop_ratio);
  const bool apce_drop = reference.apce > 0.0F &&
      metrics.apce <= reference.apce * (1.0F - config_.response_apce_drop_ratio);
  const bool margin_drop = metrics.peak_margin < reference.peak_margin &&
      reference.peak_margin - metrics.peak_margin >= config_.occlusion_peak_margin_drop;
  const int drops = static_cast<int>(psr_drop) + static_cast<int>(apce_drop) +
                    static_cast<int>(margin_drop);
  if (motion_freeze_since_ms_) {
    if (drops != 0) {
      motion_clean_since_ms_.reset();
      output.motion_gate_reason = "FROZEN_RESPONSE_DEGRADED";
    } else {
      if (!motion_clean_since_ms_) motion_clean_since_ms_ = plan.timestamp_ms;
      output.motion_gate_reason = "FROZEN_CLEAN_PENDING";
      if (plan.timestamp_ms - *motion_clean_since_ms_ >= config_.suspicion_clear_ms) {
        output.motion_freeze_age_ms = plan.timestamp_ms - *motion_freeze_since_ms_;
        motion_.reanchor_preserving_velocity(output.tracking_bbox,
            *motion_last_clean_trend_, plan.timestamp_ms);
        reset_motion_input_gate();
        motion_last_clean_trend_ = motion_.snapshot(plan.timestamp_ms);
        output.motion_update_valid = true;
        output.motion_freeze_cleared = true;
        output.motion_gate_reason = "REANCHORED";
      }
    }
    return false;
  }
  bool innovation_outlier = false;
  if (motion_residual_history_.size() >=
      static_cast<std::size_t>(config_.occlusion_min_reference_samples)) {
    std::vector<float> sorted(motion_residual_history_.begin(),
                              motion_residual_history_.end());
    std::sort(sorted.begin(), sorted.end());
    const float median = sorted[sorted.size() / 2];
    innovation_outlier = selected.center_residual > 2.0F * median;
  }
  if (drops == 0) motion_response_drop_since_ms_.reset();
  else if (!motion_response_drop_since_ms_) {
    motion_response_drop_since_ms_ = plan.timestamp_ms;
  }
  motion_recent_residuals_.push_back(selected.center_residual);
  if (motion_recent_residuals_.size() > 3) {
    motion_recent_residuals_.erase(motion_recent_residuals_.begin());
  }
  const bool innovation_rising = motion_recent_residuals_.size() == 3 &&
      motion_recent_residuals_[0] < motion_recent_residuals_[1] &&
      motion_recent_residuals_[1] < motion_recent_residuals_[2];
  const float box_scale = std::sqrt(std::max(1.0F,
      plan.predicted_bbox.width * plan.predicted_bbox.height));
  const float elapsed_s = static_cast<float>(std::min(
      plan.timestamp_ms - last_timestamp_ms_, config_.motion_max_dt_ms)) / 1000.0F;
  const float predicted_travel =
      std::hypot(plan.motion_velocity_x, plan.motion_velocity_y) * elapsed_s;
  const bool innovation_exceeds_travel =
      selected.center_residual * box_scale >
      3.0F * std::max(predicted_travel, 0.01F * box_scale);
  if (drops > 0 && innovation_outlier) {
    ++motion_risk_streak_;
    output.motion_gate_reason = "RESPONSE_AND_INNOVATION_RISK";
  } else {
    motion_risk_streak_ = 0;
    if (drops > 0) output.motion_gate_reason = "RESPONSE_RISK_ONLY";
    else if (innovation_outlier) output.motion_gate_reason = "INNOVATION_RISK_ONLY";
  }
  if (motion_response_drop_since_ms_ &&
      plan.timestamp_ms - *motion_response_drop_since_ms_ >=
          config_.suspicion_enter_ms &&
      motion_risk_streak_ >= 2 &&
      innovation_rising && innovation_exceeds_travel &&
      motion_last_clean_trend_ && response.reference) {
    motion_freeze_since_ms_ = plan.timestamp_ms;
    motion_frozen_response_reference_ = *response.reference;
    search_motion_reference_ = *motion_last_clean_trend_;
    search_motion_reference_held_ = true;
    search_motion_clean_since_ms_.reset();
    motion_.hold_trusted_trend(*motion_last_clean_trend_);
    output.motion_freeze_started = true;
    output.motion_gate_reason = "FREEZE_PERSISTENT_RESPONSE_AND_INNOVATION";
    return false;
  }
  if (drops == 0) {
    motion_residual_history_.push_back(selected.center_residual);
    if (motion_residual_history_.size() >
        static_cast<std::size_t>(config_.occlusion_reference_window)) {
      motion_residual_history_.erase(motion_residual_history_.begin());
    }
  }
  return true;
}

void LongTermController::accept_observation(
    const SearchPlan& plan, const EvaluatedObservation& selected,
    AcceptedObservationPath acceptance_path, ControllerOutput& output,
    const ResponseBaselineEvaluation& response) {
  const bool reacquired =
      acceptance_path != AcceptedObservationPath::kDirect &&
      acceptance_path != AcceptedObservationPath::kShortConfirmation;
  DimensionScaleState next_width = width_scale_state_;
  DimensionScaleState next_height = height_scale_state_;
  if (reacquired) {
    reset_scale_evidence(next_width);
    reset_scale_evidence(next_height);
  }
  BBox scaled_bbox = stabilize_accepted_bbox(
      selected.observation.bbox, next_width, next_height);
  if (!legacy_anchor_observations_ &&
      selected.observation.template_kind == TemplateKind::kRecent) {
    scaled_bbox = bound_recent_scale(scaled_bbox, plan.timestamp_ms);
    next_width.emitted = scaled_bbox.width;
    next_height.emitted = scaled_bbox.height;
  }
  output.tracking_bbox = scaled_bbox;
  output.display_bbox = stabilize_accepted_center(
      plan, selected.observation.bbox, scaled_bbox, reacquired, output);
  output.control_bbox = output.tracking_bbox;
  output.bbox = output.display_bbox;
  output.accepted = true;
  output.control_valid = true;
  output.reacquired = reacquired;
  output.accepted_observation_path = acceptance_path;
  // Any accepted observation closes an exhausted coast episode. A true
  // reacquisition additionally resets its evidence and motion state below.
  hold_used_for_loss_ = false;
  hold_remaining_frames_ = 0;
  occlusion_coast_age_frames_ = 0;
  coast_probe_candidate_.reset();
  frozen_scale_reference_.reset();
  if (loss_start_ms_) response_baseline_.reset_after_recovery();
  if (reacquired || (!legacy_anchor_observations_ && loss_start_ms_)) {
    if (motion_freeze_since_ms_) {
      output.motion_freeze_cleared = true;
      output.motion_freeze_age_ms = plan.timestamp_ms - *motion_freeze_since_ms_;
      output.motion_gate_reason = "REACQUIRED";
    }
    motion_.initialize(output.tracking_bbox, plan.timestamp_ms,
                       frame_size_.width, frame_size_.height);
    reset_motion_input_gate();
    motion_last_clean_trend_ = motion_.snapshot(plan.timestamp_ms);
    output.motion_update_valid = true;
    occlusion_evidence_.reset();
    occlusion_suspicion_frames_ = 0;
    occlusion_decision_ = occlusion_evidence_.observe(OcclusionEvidenceInput{
        output.tracking_bbox, output.tracking_bbox,
        selected.observation.metrics, true, false, false});
  } else {
    if (motion_input_allows_correction(plan, selected, response, output)) {
      motion_.correct(output.tracking_bbox, plan.timestamp_ms);
      output.motion_update_valid = true;
      output.motion_correction_applied = true;
      if (!response.psr_drop && !response.apce_drop &&
          !response.peak_margin_drop) {
        motion_last_clean_trend_ = motion_.snapshot(plan.timestamp_ms);
      }
    }
  }
  motion_.end_search();
  motion_search_active_ = false;
  width_scale_state_ = std::move(next_width);
  height_scale_state_ = std::move(next_height);
  current_bbox_ = output.tracking_bbox;
  last_trusted_timestamp_ms_ = plan.timestamp_ms;
  display_bbox_ = output.display_bbox;
  loss_start_ms_.reset();
  confirmations_.clear();
  active_detector_candidate_.reset();
  detector_candidates_.clear();
  if (uses_unified_recovery() && reacquired) invalidate_unified_recovery();
  mode_ = TrackingMode::kTracking;
  output.mode = mode_;
  ++stable_frames_;
  consecutive_unreliable_frames_ = 0;
  // As in ordinary recovery, an old staged recent template cannot be
  // committed using evidence from a different tracking episode.
  if (reacquired && recent_probation_) {
    output.template_action = TemplateAction::kRollbackRecent;
  }
}

LongTermController::DetectorIngestResult
LongTermController::ingest_detector_batch(
    const SearchPlan& plan,
    const std::optional<DetectorBatch>& batch) {
  DetectorIngestResult result;
  if (!batch) return result;

  result.diagnostics.reserve(batch->candidates.size());
  for (const DetectorCandidate& candidate : batch->candidates) {
    DetectorCandidateDiagnostic diagnostic;
    diagnostic.candidate = candidate;
    diagnostic.drawable = valid_candidate(candidate, frame_size_);
    if (candidate.class_id < 0) {
      diagnostic.class_disposition = DetectorClassDisposition::kInvalidClass;
    } else if (target_class_.status == TargetClassStatus::kUnknown ||
               !target_class_.target_class_allows_recovery ||
               !target_class_.target_class_id || *target_class_.target_class_id < 0) {
      diagnostic.class_disposition = DetectorClassDisposition::kTargetUnknown;
    } else {
      diagnostic.class_disposition = candidate.class_id == *target_class_.target_class_id
          ? DetectorClassDisposition::kMatch : DetectorClassDisposition::kMismatch;
    }
    if (!diagnostic.drawable || candidate.class_id < 0) {
      diagnostic.rejection = DetectorCandidateRejection::kInvalid;
    }
    result.diagnostics.push_back(diagnostic);
  }

  const auto reject_eligible_candidates =
      [&](DetectorCandidateRejection rejection) {
        for (DetectorCandidateDiagnostic& diagnostic : result.diagnostics) {
          if (diagnostic.rejection != DetectorCandidateRejection::kInvalid) {
            diagnostic.rejection = rejection;
          }
        }
      };

  const bool unified = uses_unified_recovery();
  if (batch->generation != detector_eligibility_generation_) {
    const bool invalidated_by_coast = !unified &&
        batch->generation >= detector_session_generation_ &&
        batch->generation < detector_eligibility_generation_;
    reject_eligible_candidates(invalidated_by_coast
        ? DetectorCandidateRejection::kHoldInvalidated
        : DetectorCandidateRejection::kGenerationInvalidated);
    result.status = DetectorBatchStatus::kInvalid;
    return result;
  }
  if (!last_detector_request_frame_ || !last_detector_request_ms_ ||
      (unified && batch->source_frame_id == last_consumed_detector_frame_) ||
      batch->source_frame_id != *last_detector_request_frame_ ||
      batch->source_timestamp_ms != *last_detector_request_ms_) {
    reject_eligible_candidates(DetectorCandidateRejection::kFrameMismatch);
    result.status = DetectorBatchStatus::kFrameMismatch;
    return result;
  }
  const std::int64_t age = plan.timestamp_ms - batch->source_timestamp_ms;
  if (age < 0 || age > config_.detector_stale_ms) {
    reject_eligible_candidates(DetectorCandidateRejection::kStale);
    result.status = DetectorBatchStatus::kStale;
    if (unified) {
      invalidate_unified_recovery();
      recovery_candidates_.expire(plan.frame_id, plan.timestamp_ms,
                                  detector_eligibility_generation_);
    }
    return result;
  }
  if (plan.mode == TrackingMode::kOcclusionHold) {
    reject_eligible_candidates(DetectorCandidateRejection::kHoldInvalidated);
    result.status = DetectorBatchStatus::kInvalid;
    return result;
  }
  if (unified) {
    if (!loss_start_ms_) {
      reject_eligible_candidates(DetectorCandidateRejection::kGenerationInvalidated);
      result.status = DetectorBatchStatus::kInvalid;
      return result;
    }
    last_consumed_detector_frame_ = batch->source_frame_id;
  }
  struct PositionedCandidate {
    std::size_t diagnostic_index = 0;
    float center_ratio = 0.0F;
  };
  std::vector<PositionedCandidate> positioned;
  std::size_t geometry_valid_count = 0;
  std::size_t class_valid_count = 0;
  positioned.reserve(batch->candidates.size());
  for (std::size_t index = 0; index < batch->candidates.size(); ++index) {
    const DetectorCandidate& candidate = batch->candidates[index];
    DetectorCandidateDiagnostic& diagnostic = result.diagnostics[index];
    if (diagnostic.rejection == DetectorCandidateRejection::kInvalid) continue;
    ++geometry_valid_count;
    diagnostic.batch_eligible = true;
    if (diagnostic.class_disposition == DetectorClassDisposition::kTargetUnknown) {
      diagnostic.rejection = DetectorCandidateRejection::kTargetClassUnknown;
      continue;
    }
    if (diagnostic.class_disposition == DetectorClassDisposition::kMismatch) {
      diagnostic.rejection = DetectorCandidateRejection::kClassMismatch;
      continue;
    }
    ++class_valid_count;
    const BBox request_motion = last_detector_request_motion_bbox_.value_or(
        plan.motion_predicted_bbox);
    const float request_center_x =
        request_motion.x + 0.5F * request_motion.width;
    const float request_center_y =
        request_motion.y + 0.5F * request_motion.height;
    const float current_center_x =
        plan.motion_predicted_bbox.x + 0.5F * plan.motion_predicted_bbox.width;
    const float current_center_y =
        plan.motion_predicted_bbox.y + 0.5F * plan.motion_predicted_bbox.height;
    BBox projected = candidate.bbox;
    projected.x += current_center_x - request_center_x;
    projected.y += current_center_y - request_center_y;
    projected = clip_bbox(projected, frame_size_.width, frame_size_.height);
    if (projected.width <= 0.0F || projected.height <= 0.0F) {
      diagnostic.rejection = DetectorCandidateRejection::kPositionRejected;
      ++result.position_rejected_count;
      continue;
    }
    diagnostic.projected_bbox = projected;
    const float center_ratio = frame_normalized_center_distance(
        projected, plan.motion_predicted_bbox, frame_size_);
    diagnostic.center_ratio = center_ratio;
    if (unified || center_ratio <=
        config_.detector_candidate_max_center_ratio + kPositionGateEpsilon) {
      diagnostic.position_gate_pass = true;
      positioned.push_back(PositionedCandidate{index, center_ratio});
    } else {
      diagnostic.rejection = DetectorCandidateRejection::kPositionRejected;
      ++result.position_rejected_count;
    }
  }
  if (geometry_valid_count == 0) {
    result.status = DetectorBatchStatus::kInvalid;
    return result;
  }
  if (class_valid_count == 0) {
    result.status = DetectorBatchStatus::kAccepted;
    return result;
  }
  result.position_valid_count = positioned.size();
  if (positioned.empty()) {
    result.status = DetectorBatchStatus::kPositionRejected;
    return result;
  }
  std::sort(positioned.begin(), positioned.end(),
            [&](const PositionedCandidate& first,
                const PositionedCandidate& second) {
    if (first.center_ratio != second.center_ratio) {
      return first.center_ratio < second.center_ratio;
    }
    return result.diagnostics[first.diagnostic_index].candidate.confidence >
           result.diagnostics[second.diagnostic_index].candidate.confidence;
  });
  if (unified) {
    // A near detection with a clear spatial lead gets a short NanoTrack
    // verification window. The closest other detections remain alternatives;
    // distant overflow cannot veto that verification without tracker evidence.
    const bool dominant_near_detector = plan.motion_prediction_confident &&
        positioned.size() > 1 &&
        positioned.front().center_ratio <= config_.detector_candidate_max_center_ratio &&
        positioned[1].center_ratio - positioned[0].center_ratio >
            config_.detector_candidate_max_center_ratio;
    const std::size_t considered = dominant_near_detector
        ? std::min(positioned.size(), config_.detector_max_candidates)
        : positioned.size();
    std::vector<std::optional<RecoveryCandidateKey>> keys(positioned.size());
    for (std::size_t index = 0; index < considered; ++index) {
      const auto& item = positioned[index];
      auto& diagnostic = result.diagnostics[item.diagnostic_index];
      DetectorCandidate candidate = diagnostic.candidate;
      candidate.bbox = *diagnostic.projected_bbox;
      std::optional<BBox> subtarget_region;
      BBox probe = candidate.bbox;
      if (config_.detector_target_mode == DetectorTargetMode::kSubtarget) {
        subtarget_region = candidate.bbox;
        probe = centered_box(candidate.bbox.x + 0.5F * candidate.bbox.width,
                             candidate.bbox.y + 0.5F * candidate.bbox.height,
                             current_bbox_, frame_size_);
      }
      keys[index] = recovery_candidates_.observe(
          RecoveryCandidateSource::kDetector, probe, candidate.confidence,
          batch->source_timestamp_ms, candidate.class_id, false, item.center_ratio,
          subtarget_region);
    }
    if (dominant_near_detector && keys.front()) {
      recovery_candidates_.focus_detector(*keys.front());
    }
    for (std::size_t index = 0; index < positioned.size(); ++index) {
      auto& diagnostic = result.diagnostics[positioned[index].diagnostic_index];
      const auto* retained = keys[index] ? recovery_candidates_.find(*keys[index]) : nullptr;
      if (retained && retained->from(RecoveryCandidateSource::kDetector)) {
        diagnostic.queued = true;
        diagnostic.candidate_id = retained->key.id;
      } else {
        diagnostic.rejection = DetectorCandidateRejection::kCapacityRejected;
      }
    }
    result.status = DetectorBatchStatus::kAccepted;
    return result;
  }
  if (positioned.size() > config_.detector_max_candidates) {
    for (std::size_t index = config_.detector_max_candidates;
         index < positioned.size(); ++index) {
      result.diagnostics[positioned[index].diagnostic_index].rejection =
          DetectorCandidateRejection::kCapacityRejected;
    }
    positioned.resize(config_.detector_max_candidates);
  }
  const bool dominant_nearest_candidate =
      positioned.size() >= 2 &&
      positioned[0].center_ratio <=
          kDominantNearestCandidateRatio * positioned[1].center_ratio;
  detector_candidates_.clear();
  for (std::size_t positioned_index = 0;
       positioned_index < positioned.size(); ++positioned_index) {
    const PositionedCandidate& positioned_candidate = positioned[positioned_index];
    DetectorCandidateDiagnostic& diagnostic =
        result.diagnostics[positioned_candidate.diagnostic_index];
    DetectorCandidate candidate = diagnostic.candidate;
    candidate.bbox = *diagnostic.projected_bbox;
    QueuedDetectorCandidate queued;
    queued.id = next_detector_candidate_id_++;
    queued.candidate = candidate;
    queued.center_ratio = positioned_candidate.center_ratio;
    queued.same_class_candidates = static_cast<std::size_t>(std::count_if(
        positioned.begin(), positioned.end(),
        [&](const PositionedCandidate& other) {
          return result.diagnostics[other.diagnostic_index]
                     .candidate.class_id == candidate.class_id;
        }));
    queued.same_class_ambiguous =
        queued.same_class_candidates > 1 &&
        !(positioned_index == 0 && dominant_nearest_candidate);
    queued.source_frame_id = batch->source_frame_id;
    queued.source_timestamp_ms = batch->source_timestamp_ms;
    queued.generation = batch->generation;
    queued.probes = detector_probes(candidate.bbox);
    if (!queued.probes.empty()) {
      diagnostic.candidate_id = queued.id;
      diagnostic.queued = true;
      detector_candidates_.push_back(std::move(queued));
    } else {
      diagnostic.rejection = DetectorCandidateRejection::kCapacityRejected;
    }
  }
  result.status = DetectorBatchStatus::kAccepted;
  return result;
}

ControllerOutput LongTermController::finish_frame(
    const SearchPlan& plan,
    const std::vector<TrackerObservation>& observations,
    const std::optional<DetectorBatch>& detector_batch) try {
  // Consume the candidate before validating input: stale plans or malformed
  // template/frame identities must never retain a hit for a later confirm.
  const auto pending_coast_candidate = coast_probe_candidate_;
  coast_probe_candidate_.reset();
  if (!frame_open_ || plan.frame_id != active_plan_.frame_id ||
      plan.timestamp_ms != active_plan_.timestamp_ms) {
    if (pending_template_transition_) {
      resolve_template_transition(pending_template_transition_->transaction_id, false);
    }
    if (!frame_open_ && uses_unified_recovery()) {
      invalidate_unified_recovery();
      cancel_probation();
    }
    throw std::logic_error("finish_frame does not match active search plan");
  }
  ControllerOutput output;
  output.frame_id = plan.frame_id;
  output.target_class = target_class_;
  output.timestamp_ms = plan.timestamp_ms;
  output.mode = plan.mode;
  output.bbox = plan.predicted_bbox;
  output.tracking_bbox = plan.predicted_bbox;
  output.display_bbox = plan.predicted_bbox;
  output.control_bbox = plan.predicted_bbox;
  output.search_scale = plan.search_scale;
  output.hypothesis_source = plan.hypothesis_source;
  output.detector_candidate_id = plan.detector_candidate_id;
  output.detector_same_class_candidates =
      plan.detector_same_class_candidates;
  output.detector_same_class_ambiguous =
      plan.detector_same_class_ambiguous;
  output.detector_candidate_locked = plan.detector_candidate_locked;
  output.detector_probe_index = plan.detector_probe_index;
  output.search_hint = plan.search_hint;
  if (rollback_pending_) output.template_action = TemplateAction::kRollbackRecent;
  output.motion_search_age_ms = plan.motion_search_age_ms;
  output.motion_prediction_confident = plan.motion_prediction_confident;
  output.motion_velocity_x = plan.motion_velocity_x;
  output.motion_velocity_y = plan.motion_velocity_y;
  output.motion_acceleration_x = plan.motion_acceleration_x;
  output.motion_acceleration_y = plan.motion_acceleration_y;
  output.motion_scale_velocity_width = plan.motion_scale_velocity_width;
  output.motion_scale_velocity_height = plan.motion_scale_velocity_height;

  std::optional<EvaluatedObservation> anchor;
  std::optional<EvaluatedObservation> recent;
  std::optional<EvaluatedObservation> safe;
  std::optional<EvaluatedObservation> staged;
  std::optional<std::uint64_t> token;
  for (const TrackerObservation& observation : observations) {
    // Skipped coast frames retain their existing observation-isolation contract.
    if (plan.mode == TrackingMode::kOcclusionHold && !plan.run_tracker) break;
    if (token && observation.search_token != *token) {
      throw std::invalid_argument("template observations must share one search token");
    }
    token = observation.search_token;
    EvaluatedObservation evaluated = evaluate(observation, plan);
    const auto record = [&](std::optional<EvaluatedObservation>& slot) {
      if (slot) throw std::invalid_argument("duplicate template observation");
      slot = evaluated;
    };
    switch (observation.template_kind) {
      case TemplateKind::kAnchor: record(anchor); break;
      case TemplateKind::kRecent: record(recent); break;
      case TemplateKind::kSafe: record(safe); break;
      case TemplateKind::kStaged: record(staged); break;
    }
  }

  ResponseBaselineEvaluation response;
  if (recent) {
    const auto& metrics = recent->observation.metrics;
    response = response_baseline_.evaluate(metrics);
    output.trusted_response_reference = response.reference;
    if (const auto& reference = response.reference) {
      if (reference->psr > 0.0F) {
        output.response_psr_relative_delta = (metrics.psr - reference->psr) / reference->psr;
      }
      if (reference->apce > 0.0F) {
        output.response_apce_relative_delta = (metrics.apce - reference->apce) / reference->apce;
      }
      output.response_peak_margin_absolute_delta = metrics.peak_margin - reference->peak_margin;
    }
  }

  if (anchor) {
    legacy_anchor_observations_ = true;
    promote_shadow_prediction();
    clear_suspicion();
  }
  if (!anchor && (recent || safe) && !legacy_anchor_observations_) {
    recent_safe_observations_ = true;
    recovery_candidates_.expire(plan.frame_id, plan.timestamp_ms,
                                detector_eligibility_generation_);
  }
  if (plan.run_tracker && plan.recovery_probe && plan.subtarget_probe_is_final.has_value() &&
      search_reacquisition_eligible(plan.timestamp_ms)) {
    recovery_candidates_.advance_subtarget_probe(
        *plan.recovery_probe, plan.predicted_bbox, plan.detector_probe_index,
        *plan.subtarget_probe_is_final);
  }
  const DetectorIngestResult ingest_result =
      ingest_detector_batch(plan, detector_batch);
  output.detector_status = ingest_result.status;
  output.detector_position_valid_count =
      ingest_result.position_valid_count;
  output.detector_position_rejected_count =
      ingest_result.position_rejected_count;
  output.detector_candidate_diagnostics = ingest_result.diagnostics;

  const bool ordinary_recent =
      (!legacy_anchor_observations_ || recovery_probation_) && !loss_start_ms_ &&
      (plan.mode == TrackingMode::kTracking || plan.mode == TrackingMode::kSuspected);
  const auto& probation_template =
      legacy_anchor_observations_ && !recovery_probation_ ? anchor : staged;
  const bool probation_templates_agree =
      probation_template && recent &&
      probation_template->quality == QualityLevel::kReliable &&
      recent->quality == QualityLevel::kReliable &&
      bbox_iou(probation_template->observation.bbox, recent->observation.bbox) >=
          config_.template_agreement_iou;
  std::optional<EvaluatedObservation> selected;
  bool force_confirmation = false;
  if (ordinary_recent) {
    selected = recent;
    const auto& other = staged ? staged : safe;
    if (recent && other) {
      output.template_iou = bbox_iou(recent->observation.bbox, other->observation.bbox);
    }
    const bool probation_geometry_allows_acceptance = !recent_probation_ ||
        (probation_templates_agree &&
         recovery_candidates_.geometry_consistent(
             probation_template->observation.bbox, recent->observation.bbox));
    RecentDisposition disposition = RecentDisposition::kSearch;
    if (recovery_probation_) {
      // The staged recovery template has already passed the two-hit identity
      // gate. Keep it authoritative while recent proves it can follow the same
      // object; one divergent recent response must not discard the confirmed
      // recovery immediately.
      if (staged) {
        selected = staged;
        disposition = assess_recent(plan, *staged, response);
      }
    } else if (recent && probation_geometry_allows_acceptance) {
      disposition = assess_recent(plan, *recent, response);
    }
    if (disposition == RecentDisposition::kTentative) {
      publish_tentative(plan, *selected, output);
      complete_frame(plan, output);
      return output;
    }
    if (disposition == RecentDisposition::kSearch && selected) {
      selected->quality = QualityLevel::kRejected;
    }
    promote_shadow_prediction();
    clear_suspicion();
  } else if (anchor && recent) {
    output.template_iou = bbox_iou(anchor->observation.bbox, recent->observation.bbox);
    if (anchor->quality == QualityLevel::kReliable &&
        recent->quality == QualityLevel::kReliable) {
      if (output.template_iou < config_.template_agreement_iou) {
        selected = anchor;
        force_confirmation = true;
      } else {
        selected = anchor->observation.metrics.raw_score >=
                           recent->observation.metrics.raw_score
                       ? anchor
                       : recent;
      }
    } else if (anchor->quality == QualityLevel::kReliable) {
      selected = anchor;
    } else if (recent->quality == QualityLevel::kReliable) {
      selected = recent;
      force_confirmation = true;
    } else {
      selected = quality_rank(anchor->quality) >= quality_rank(recent->quality)
                     ? anchor
                     : recent;
    }
  } else if (anchor) {
    selected = anchor;
  } else if (recent) {
    selected = recent;
    if (recent->quality == QualityLevel::kReliable) force_confirmation = true;
  }

  if (plan.mode == TrackingMode::kOcclusionHold) {
    output.motion_control_authorized = plan.motion_control_authorized;
    output.control_valid = output.motion_control_authorized;
    output.occlusion_coast_age_frames = plan.occlusion_coast_age_frames;
    output.occlusion_hold_remaining_frames = plan.hold_remaining_frames;
    output.occlusion_probe_type = plan.occlusion_probe_type;
    publish_occlusion_evidence(output);
    output.occlusion_suspicion_started = false;
    output.occlusion_suspicion_cleared = false;
    if (selected) {
      output.quality = selected->quality;
      output.selected_template = selected->observation.template_kind;
      output.selected_metrics = selected->observation.metrics;
      output.normalized_center_residual = selected->center_residual;
      output.area_log_change = selected->area_log_change;
      output.aspect_log_change = selected->aspect_log_change;
    }
    const bool probe_hit = selected && anchor && recent &&
        anchor->quality == QualityLevel::kReliable &&
        recent->quality == QualityLevel::kReliable &&
        output.template_iou >= config_.template_agreement_iou;
    if (plan.occlusion_probe_type != OcclusionProbeType::kNone) {
      output.occlusion_probe_motion_bbox = plan.motion_predicted_bbox;
      if (selected) output.occlusion_probe_bbox = selected->observation.bbox;
      if (anchor) output.occlusion_probe_anchor_reliable = anchor->quality == QualityLevel::kReliable;
      if (recent) output.occlusion_probe_recent_reliable = recent->quality == QualityLevel::kReliable;
      if (anchor) {
        output.occlusion_probe_anchor_score_gate = anchor->score_gate;
        output.occlusion_probe_anchor_center_gate = anchor->center_gate;
        output.occlusion_probe_anchor_area_gate = anchor->area_gate;
        output.occlusion_probe_anchor_aspect_gate = anchor->aspect_gate;
      }
      if (recent) {
        output.occlusion_probe_recent_score_gate = recent->score_gate;
        output.occlusion_probe_recent_center_gate = recent->center_gate;
        output.occlusion_probe_recent_area_gate = recent->area_gate;
        output.occlusion_probe_recent_aspect_gate = recent->aspect_gate;
      }
      if (anchor && recent)
        output.occlusion_probe_template_gate = output.template_iou >= config_.template_agreement_iou;
      if (!anchor || !recent) output.occlusion_probe_rejection = "MISSING_TEMPLATE";
      else if (anchor->quality != QualityLevel::kReliable ||
               recent->quality != QualityLevel::kReliable)
        output.occlusion_probe_rejection = "QUALITY_OR_MOTION_GEOMETRY";
      else if (!*output.occlusion_probe_template_gate)
        output.occlusion_probe_rejection = "TEMPLATE_AGREEMENT";
      if (plan.occlusion_probe_type == OcclusionProbeType::kConfirm && probe_hit) {
        if (!pending_coast_candidate || pending_coast_candidate->frame_id != last_frame_id_) {
          output.occlusion_probe_rejection = "MISSING_CANDIDATE";
        } else {
          const auto& reference = pending_coast_candidate->bbox;
          const auto& bbox = selected->observation.bbox;
          output.occlusion_probe_cross_iou = bbox_iou(reference, bbox) >= config_.occlusion_confirmation_iou;
          output.occlusion_probe_cross_center = normalized_center_distance(bbox, reference) <= config_.occlusion_confirmation_center_distance;
          output.occlusion_probe_cross_area = area_log_delta(bbox, reference) <= config_.occlusion_confirmation_max_area_log_change;
          output.occlusion_probe_cross_aspect = aspect_log_delta(bbox, reference) <= config_.occlusion_confirmation_max_aspect_log_change;
          output.occlusion_probe_cross_gate = *output.occlusion_probe_cross_iou &&
              *output.occlusion_probe_cross_center && *output.occlusion_probe_cross_area &&
              *output.occlusion_probe_cross_aspect;
          if (!*output.occlusion_probe_cross_gate) output.occlusion_probe_rejection = "CONFIRM_GEOMETRY";
        }
      }
    }
    if (plan.occlusion_probe_type != OcclusionProbeType::kNone)
      output.occlusion_probe_confirmation_required = 2;
    if (plan.occlusion_probe_type == OcclusionProbeType::kSearch && probe_hit) {
      coast_probe_candidate_ = CoastProbeCandidate{plan.frame_id, selected->observation.bbox};
      output.occlusion_probe_confirmation_hits = 1;
    } else if (plan.occlusion_probe_type == OcclusionProbeType::kConfirm && probe_hit &&
               output.occlusion_probe_cross_gate.value_or(false)) {
      accept_observation(plan, *selected,
                         AcceptedObservationPath::kOcclusionProbeRecovery,
                         output, response);
      output.motion_control_authorized = false;
      output.occlusion_hold_remaining_frames = 0;
      output.occlusion_probe_confirmation_hits = 2;
      complete_frame(plan, output);
      return output;
    }
    // No observation-side effects are allowed before two consistent hits.
    occlusion_coast_age_frames_ = plan.occlusion_coast_age_frames;
    if (hold_remaining_frames_ > 0) --hold_remaining_frames_;
    if (hold_remaining_frames_ == 0) coast_probe_candidate_.reset();
    mode_ = hold_remaining_frames_ > 0 ? TrackingMode::kOcclusionHold
                                      : mode_for_timestamp(plan.timestamp_ms);
    complete_frame(plan, output);
    return output;
  }

  bool accepted = false;
  AcceptedObservationPath acceptance_path = AcceptedObservationPath::kDirect;
  const bool detector_verification_active =
      plan.hypothesis_source == HypothesisSource::kDetector ||
      active_detector_candidate_.has_value();
  const bool hold_source_mode =
      plan.mode == TrackingMode::kTracking ||
      plan.mode == TrackingMode::kUncertain ||
      plan.mode == TrackingMode::kLocalRecovery;
  if (!ordinary_recent && selected && !hold_used_for_loss_) {
    // Only normal, accepted tracking observations may enter stable history.
    // Confirmation and all motion/template effects happen after evidence.
    const bool directly_accepted =
        selected->quality == QualityLevel::kReliable && !loss_start_ms_ &&
        plan.mode == TrackingMode::kTracking && !force_confirmation &&
        !detector_verification_active;
    occlusion_decision_ = occlusion_evidence_.observe(OcclusionEvidenceInput{
        selected->observation.bbox, plan.motion_predicted_bbox,
        selected->observation.metrics, directly_accepted,
        anchor && recent && output.template_iou < config_.template_agreement_iou,
        detector_verification_active});
    occlusion_suspicion_frames_ = occlusion_decision_.suspicion_active
        ? occlusion_suspicion_frames_ + 1 : 0;
  } else if (!selected || ordinary_recent) {
    occlusion_decision_ = occlusion_evidence_.observe_gap();
  }
  publish_occlusion_evidence(output);
  if (!uses_unified_recovery() && selected && !hold_used_for_loss_ && hold_source_mode &&
      !detector_verification_active && occlusion_decision_.triggered) {
    const ScaleReference reference{*occlusion_decision_.reference_width,
                                   *occlusion_decision_.reference_height};
    motion_.freeze_scale(reference.width, reference.height);
    validate_bbox(motion_.current_bbox(), frame_size_.width, frame_size_.height);
    frozen_scale_reference_ = reference;
    // A configuration retained for compatibility may exceed twelve; it cannot
    // expand the control authorization ceiling.
    const auto budget = static_cast<std::size_t>(
        std::min(12, config_.occlusion_hold_frames));
    occlusion_coast_age_frames_ = 1;
    hold_remaining_frames_ = budget - 1;
    hold_used_for_loss_ = true;
    loss_start_ms_ = plan.timestamp_ms;
    ++detector_eligibility_generation_;
    confirmations_.clear();
    coast_probe_candidate_.reset();
    detector_candidates_.clear();
    active_detector_candidate_.reset();
    stable_frames_ = 0;
    mode_ = TrackingMode::kOcclusionHold;
    output.mode = mode_;
    output.quality = selected->quality;
    output.selected_template = selected->observation.template_kind;
    output.selected_metrics = selected->observation.metrics;
    output.normalized_center_residual = selected->center_residual;
    output.area_log_change = selected->area_log_change;
    output.aspect_log_change = selected->aspect_log_change;
    output.tracking_bbox = motion_.current_bbox();
    output.display_bbox = output.tracking_bbox;
    output.control_bbox = output.tracking_bbox;
    output.bbox = output.display_bbox;
    output.hypothesis_source = HypothesisSource::kMotion;
    output.motion_control_authorized = true;
    output.control_valid = true;
    output.occlusion_hold_triggered = true;
    output.occlusion_coast_age_frames = 1;
    output.occlusion_hold_remaining_frames = budget;
    output.occlusion_consecutive_unreliable_frames =
        selected->quality == QualityLevel::kReliable
            ? 0 : static_cast<std::size_t>(consecutive_unreliable_frames_ + 1);
    output.occlusion_observation_width = selected->observation.bbox.width;
    output.occlusion_observation_height = selected->observation.bbox.height;
    for (auto& diagnostic : output.detector_candidate_diagnostics) {
      if (!diagnostic.batch_eligible) continue;
      diagnostic.batch_eligible = false;
      diagnostic.position_gate_pass = false;
      diagnostic.queued = false;
      diagnostic.candidate_id.reset();
      diagnostic.rejection = DetectorCandidateRejection::kHoldInvalidated;
    }
    complete_frame(plan, output);
    return output;
  }
  const bool candidate_search_path = uses_unified_recovery() && loss_start_ms_;
  if (candidate_search_path) {
    if (recent && safe) {
      output.template_iou = bbox_iou(recent->observation.bbox, safe->observation.bbox);
    }
    selected = confirm_recovery_candidate(plan, recent, safe, output);
    if (selected) {
      accepted = true;
      acceptance_path = AcceptedObservationPath::kSearchReacquisition;
    }
  }
  if (selected) {
    output.quality = selected->quality;
    output.selected_template = selected->observation.template_kind;
    output.selected_metrics = selected->observation.metrics;
    output.normalized_center_residual = selected->center_residual;
    output.area_log_change = selected->area_log_change;
    output.aspect_log_change = selected->aspect_log_change;
    if (ordinary_recent) {
      accepted = selected->quality == QualityLevel::kReliable;
    } else if (!candidate_search_path && selected->quality == QualityLevel::kReliable &&
        !(config_.motion_search_enabled && occlusion_decision_.triggered) &&
        plan.hypothesis_source != HypothesisSource::kDetector) {
      const bool recovery = loss_start_ms_.has_value() ||
                            plan.mode != TrackingMode::kTracking ||
                            force_confirmation;
      if (!recovery) {
        accepted = true;
      } else {
        const bool fixed_center_gate =
            config_.max_reacquisition_center_distance == 0.0F ||
            selected->center_residual <=
                config_.max_reacquisition_center_distance;
        if (!fixed_center_gate) {
          confirmations_.clear();
        } else if (add_confirmation(plan.frame_id,
                                    selected->observation.bbox)) {
          accepted = true;
          acceptance_path =
              plan.mode == TrackingMode::kLocalRecovery ||
                      plan.mode == TrackingMode::kAssistedRecovery ||
                      plan.mode == TrackingMode::kLost
                  ? AcceptedObservationPath::kLocalOrDeeperRecovery
                  : AcceptedObservationPath::kShortConfirmation;
        }
      }
    }
  }

  const bool detector_plan =
      !uses_unified_recovery() && plan.hypothesis_source == HypothesisSource::kDetector &&
      plan.detector_candidate_id && active_detector_candidate_ &&
      active_detector_candidate_->queued.id == *plan.detector_candidate_id;
  bool discard_active_detector = false;
  if (detector_plan) {
    ActiveDetectorCandidate& active = *active_detector_candidate_;
    const bool templates_agree =
        anchor && anchor->quality == QualityLevel::kReliable &&
        (!recent || recent->quality != QualityLevel::kReliable ||
         output.template_iou >= config_.template_agreement_iou);
    if (!active.locked) {
      if (!plan.detector_same_class_ambiguous && selected && templates_agree &&
          selected->quality == QualityLevel::kReliable &&
          detector_geometry_consistent(plan.predicted_bbox,
                                       selected->observation.bbox)) {
        active.locked = true;
        active.locked_probe_index = plan.detector_probe_index;
        active.locked_bbox = selected->observation.bbox;
        active.confirmation_attempts = 1;
        active.confirmation_boxes = {selected->observation.bbox};
      } else if (active.next_probe >= active.queued.probes.size()) {
        discard_active_detector = true;
      }
    } else {
      ++active.confirmation_attempts;
      if (selected && templates_agree &&
          selected->quality == QualityLevel::kReliable &&
          detector_confirmation_consistent(selected->observation.bbox)) {
        active.locked_bbox = selected->observation.bbox;
        active.confirmation_boxes.push_back(selected->observation.bbox);
      }
    }
    output.detector_candidate_locked = active.locked;
    output.detector_confirmation_attempts = active.confirmation_attempts;
    output.detector_confirmation_hits = active.confirmation_boxes.size();
    if (active.locked &&
        active.confirmation_boxes.size() >=
            static_cast<std::size_t>(config_.confirmation_required)) {
      accepted = true;
      acceptance_path = AcceptedObservationPath::kDetectorRecovery;
    } else if (active.locked &&
               active.confirmation_attempts >=
                   static_cast<std::size_t>(config_.confirmation_window)) {
      discard_active_detector = true;
    }
  }

  if (accepted && acceptance_path == AcceptedObservationPath::kSearchReacquisition) {
    defer_recovery_acceptance(plan, *selected, output);
  } else if (accepted) {
    accept_observation(plan, *selected, acceptance_path, output, response);
    const bool response_degraded = response.psr_drop || response.apce_drop ||
                                   response.peak_margin_drop;
    if (response_degraded) {
      search_motion_reference_held_ = true;
      search_motion_clean_since_ms_.reset();
    } else if (search_motion_reference_held_) {
      if (!search_motion_clean_since_ms_) {
        search_motion_clean_since_ms_ = plan.timestamp_ms;
      }
      if (plan.timestamp_ms - *search_motion_clean_since_ms_ >=
          config_.suspicion_clear_ms) {
        search_motion_reference_ = motion_.snapshot(plan.timestamp_ms);
        search_motion_reference_held_ = false;
        search_motion_clean_since_ms_.reset();
      }
    } else {
      search_motion_reference_ = motion_.snapshot(plan.timestamp_ms);
    }
    if (ordinary_recent && !recent_probation_) {
      response_baseline_.resume_tracking();
      response_baseline_.observe_trusted(selected->observation.metrics);
    }
  } else {
    motion_risk_streak_ = 0;
    motion_response_drop_since_ms_.reset();
    motion_recent_residuals_.clear();
    motion_clean_since_ms_.reset();
    response_baseline_.freeze();
    motion_.freeze_scale(current_bbox_.width, current_bbox_.height);
    output.tracking_bbox = motion_.current_bbox();
    output.display_bbox = output.tracking_bbox;
    output.control_bbox = output.tracking_bbox;
    output.bbox = output.display_bbox;
    ++consecutive_unreliable_frames_;
    const bool starting_search = !loss_start_ms_;
    if (starting_search) loss_start_ms_ = plan.timestamp_ms;
    mode_ = mode_for_timestamp(plan.timestamp_ms);
    output.mode = mode_;
    if (starting_search && uses_unified_recovery()) {
      invalidate_unified_recovery();
      last_detector_request_ms_.reset();
      confirmations_.clear();
      coast_probe_candidate_.reset();
      detector_candidates_.clear();
      active_detector_candidate_.reset();
    }
    if (config_.motion_search_enabled && !motion_search_active_) {
      const MotionSnapshot current_motion = motion_.snapshot(plan.timestamp_ms);
      const float trend_dot = search_motion_reference_
          ? current_motion.velocity_x * search_motion_reference_->velocity_x +
                current_motion.velocity_y * search_motion_reference_->velocity_y
          : 0.0F;
      if (search_motion_reference_held_ && search_motion_reference_ &&
          trend_dot < 0.0F) {
        motion_.begin_search(plan.timestamp_ms, *search_motion_reference_);
      } else {
        motion_.begin_search(plan.timestamp_ms);
      }
      motion_search_active_ = true;
    }
    if (config_.motion_search_enabled && motion_search_active_) {
      const MotionSnapshot snapshot = motion_.snapshot(plan.timestamp_ms);
      output.motion_search_age_ms = snapshot.search_age_ms;
      output.motion_prediction_confident = snapshot.prediction_confident;
      output.motion_velocity_x = snapshot.velocity_x;
      output.motion_velocity_y = snapshot.velocity_y;
      output.motion_acceleration_x = snapshot.acceleration_x;
      output.motion_acceleration_y = snapshot.acceleration_y;
      output.motion_scale_velocity_width = snapshot.scale_velocity_width;
      output.motion_scale_velocity_height = snapshot.scale_velocity_height;
    }
    output.motion_control_authorized = search_control_live(plan.timestamp_ms);
    output.control_valid = output.motion_control_authorized;
    output.accepted = false;
    stable_frames_ = 0;
  }

  if (!accepted && discard_active_detector) {
    active_detector_candidate_.reset();
    confirmations_.clear();
  }

  if (accepted && acceptance_path ==
                      AcceptedObservationPath::kSearchReacquisition) {
    // The commit frame stages the confirmed target. Probation evidence starts
    // on subsequent ordinary tracking frames, never on the reacquisition hit.
  } else if (rollback_pending_) {
    // The committed heads ran this frame. Retire the abandoned staged slot
    // before a subsequent frame can request a new stage.
  } else if (recent_probation_) {
    if (accepted && probation_templates_agree &&
        output.quality == QualityLevel::kReliable && !output.reacquired &&
        !force_confirmation) {
      ++probation_reliable_frames_;
      if (probation_reliable_frames_ >= config_.recent_probation_frames) {
        output.template_action = TemplateAction::kCommitRecent;
      }
    } else if (recovery_probation_ && accepted && staged &&
               output.selected_template == TemplateKind::kStaged) {
      // A reliable staged head keeps the recovered target alive, but recent
      // disagreement contributes no evidence toward committing recent.
      probation_reliable_frames_ = 0;
    } else {
      output.template_action = TemplateAction::kRollbackRecent;
    }
  } else if (accepted && !output.reacquired && !force_confirmation &&
             stable_frames_ >= config_.stable_frames_for_update &&
             plan.timestamp_ms - last_recent_update_ms_ >=
                 config_.recent_update_interval_ms) {
    output.template_action = TemplateAction::kStageRecent;
  }

  output.detector_candidates_queued = uses_unified_recovery()
      ? recovery_candidates_.detector_count() : detector_candidates_.size();
  complete_frame(plan, output);
  return output;
} catch (...) {
  // Keep the frame open until it has a complete result so malformed probe
  // identities and validation errors revoke coast just like inference errors.
  abort_frame();
  throw;
}

}  // namespace nanotrack
