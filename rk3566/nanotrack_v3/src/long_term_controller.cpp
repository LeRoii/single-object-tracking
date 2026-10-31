#include "long_term_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace nanotrack {
namespace {

constexpr float kPositionGateEpsilon = 1e-6F;

bool finite_bbox(const BBox& bbox) {
  return std::isfinite(bbox.x) && std::isfinite(bbox.y) &&
         std::isfinite(bbox.width) && std::isfinite(bbox.height);
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
    : config_(config), motion_(config), occlusion_evidence_(config) {
  validate_controller_config(config_);
}

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
  motion_.initialize(initial_bbox, timestamp_ms, frame_size.width, frame_size.height);
  mode_ = TrackingMode::kTracking;
  loss_start_ms_.reset();
  last_detector_request_ms_.reset();
  last_detector_request_frame_.reset();
  detector_candidates_.clear();
  active_detector_candidate_.reset();
  next_detector_candidate_id_ = 1;
  confirmations_.clear();
  coast_probe_candidate_.reset();
  last_frame_id_ = frame_id;
  last_timestamp_ms_ = timestamp_ms;
  stable_frames_ = 0;
  last_recent_update_ms_ = timestamp_ms;
  recent_probation_ = false;
  probation_reliable_frames_ = 0;
  occlusion_evidence_.reset();
  occlusion_decision_ = {};
  occlusion_suspicion_frames_ = 0;
  occlusion_coast_age_frames_ = 0;
  consecutive_unreliable_frames_ = 0;
  hold_remaining_frames_ = 0;
  hold_used_for_loss_ = false;
  frozen_scale_reference_.reset();
  initialized_ = true;
  frame_open_ = false;
}

TrackingMode LongTermController::mode_for_timestamp(
    std::int64_t timestamp_ms) const {
  if (!loss_start_ms_) return TrackingMode::kTracking;
  const std::int64_t elapsed = timestamp_ms - *loss_start_ms_;
  if (elapsed < config_.uncertain_ms) return TrackingMode::kUncertain;
  if (elapsed < config_.local_recovery_ms) return TrackingMode::kLocalRecovery;
  if (elapsed < config_.assisted_recovery_ms) {
    return TrackingMode::kAssistedRecovery;
  }
  return TrackingMode::kLost;
}

float LongTermController::search_scale_for(TrackingMode mode,
                                           std::int64_t timestamp_ms) const {
  switch (mode) {
    case TrackingMode::kTracking: return config_.search_tracking;
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
  if (frame_id <= last_frame_id_) {
    throw std::invalid_argument("frame id must be strictly increasing");
  }
  if (timestamp_ms <= last_timestamp_ms_) {
    throw std::invalid_argument("frame timestamp must be strictly increasing");
  }

  const TrackingMode timestamp_mode = mode_for_timestamp(timestamp_ms);
  BBox motion_predicted;
  try {
    // MotionEstimator clips finite predictions while retaining the frozen size.
    // Internal prediction failures abort this frame; no output may authorize it.
    motion_predicted = motion_.predict(timestamp_ms);
    validate_bbox(motion_predicted, frame_size_.width, frame_size_.height);
  } catch (...) {
    hold_remaining_frames_ = 0;
    occlusion_coast_age_frames_ = 0;
    coast_probe_candidate_.reset();
    mode_ = timestamp_mode;
    throw;
  }
  if (hold_remaining_frames_ > 0) {
    mode_ = TrackingMode::kOcclusionHold;
    SearchPlan plan;
    plan.frame_id = frame_id;
    plan.timestamp_ms = timestamp_ms;
    plan.mode = mode_;
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
    active_plan_ = plan;
    frame_open_ = true;
    return plan;
  }

  coast_probe_candidate_.reset();
  mode_ = timestamp_mode;
  BBox predicted = motion_predicted;
  HypothesisSource source = HypothesisSource::kMotion;
  const bool recovery_uses_detector =
      mode_ == TrackingMode::kAssistedRecovery || mode_ == TrackingMode::kLost;
  if (recovery_uses_detector && !active_detector_candidate_) {
    activate_next_detector_candidate();
  }
  if (recovery_uses_detector && active_detector_candidate_) {
    ActiveDetectorCandidate& active = *active_detector_candidate_;
    if (!active.locked && active.next_probe >= active.queued.probes.size()) {
      active_detector_candidate_.reset();
      activate_next_detector_candidate();
    }
  }
  if (recovery_uses_detector && active_detector_candidate_) {
    ActiveDetectorCandidate& active = *active_detector_candidate_;
    predicted = active.locked ? active.locked_bbox
                              : active.queued.probes[active.next_probe++];
    source = HypothesisSource::kDetector;
  }

  SearchPlan plan;
  plan.frame_id = frame_id;
  plan.timestamp_ms = timestamp_ms;
  plan.mode = mode_;
  plan.motion_predicted_bbox = motion_predicted;
  plan.predicted_bbox = predicted;
  plan.search_scale = source == HypothesisSource::kDetector
                          ? config_.detector_search_scale
                          : search_scale_for(mode_, timestamp_ms);
  plan.hypothesis_source = source;
  plan.detector_target_mode = config_.detector_target_mode;
  if (active_detector_candidate_ && source == HypothesisSource::kDetector) {
    const ActiveDetectorCandidate& active = *active_detector_candidate_;
    plan.detector_candidate_id = active.queued.id;
    plan.detector_candidate_region = active.queued.candidate.bbox;
    plan.detector_candidate_class_id = active.queued.candidate.class_id;
    plan.detector_candidate_confidence = active.queued.candidate.confidence;
    plan.detector_candidate_center_ratio = active.queued.center_ratio;
    plan.detector_same_class_candidates =
        active.queued.same_class_candidates;
    plan.detector_same_class_ambiguous =
        active.queued.same_class_candidates > 1;
    plan.detector_candidate_locked = active.locked;
    plan.detector_probe_index = active.locked ? active.locked_probe_index
                                              : active.next_probe - 1;
  }
  const bool interval_ready =
      !last_detector_request_ms_ ||
      timestamp_ms - *last_detector_request_ms_ >= config_.detector_interval_ms;
  if (detector_ready && recovery_uses_detector && interval_ready &&
      target_class_.target_class_allows_recovery) {
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
  }
  active_plan_ = plan;
  frame_open_ = true;
  return plan;
}

void LongTermController::abort_frame() noexcept {
  if (!frame_open_) return;
  frame_open_ = false;
  if (active_plan_.mode == TrackingMode::kOcclusionHold) {
    hold_remaining_frames_ = 0;
    occlusion_coast_age_frames_ = 0;
    coast_probe_candidate_.reset();
    mode_ = mode_for_timestamp(active_plan_.timestamp_ms);
  }
}

void LongTermController::publish_occlusion_evidence(ControllerOutput& output) const {
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
  if (!hard_gate) {
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

std::vector<BBox> LongTermController::detector_probes(
    const DetectorCandidate& candidate) const {
  if (config_.detector_target_mode == DetectorTargetMode::kFullObject) {
    return {candidate.bbox};
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
    const float center_x = candidate.bbox.x + coordinate[0] * candidate.bbox.width;
    const float center_y = candidate.bbox.y + coordinate[1] * candidate.bbox.height;
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

void LongTermController::accept_observation(
    const SearchPlan& plan, const EvaluatedObservation& selected,
    bool reacquired, ControllerOutput& output) {
  output.bbox = selected.observation.bbox;
  output.accepted = true;
  output.control_valid = true;
  output.reacquired = reacquired;
  if (reacquired) {
    motion_.initialize(output.bbox, plan.timestamp_ms, frame_size_.width,
                       frame_size_.height);
    hold_used_for_loss_ = false;
    hold_remaining_frames_ = 0;
    occlusion_coast_age_frames_ = 0;
    coast_probe_candidate_.reset();
    frozen_scale_reference_.reset();
    occlusion_evidence_.reset();
    occlusion_suspicion_frames_ = 0;
    occlusion_decision_ = occlusion_evidence_.observe(OcclusionEvidenceInput{
        output.bbox, output.bbox, selected.observation.metrics, true, false, false});
  } else {
    motion_.correct(output.bbox, plan.timestamp_ms);
  }
  current_bbox_ = output.bbox;
  loss_start_ms_.reset();
  confirmations_.clear();
  active_detector_candidate_.reset();
  detector_candidates_.clear();
  mode_ = TrackingMode::kTracking;
  output.mode = mode_;
  ++stable_frames_;
  consecutive_unreliable_frames_ = 0;
  // As in ordinary recovery, an old staged recent template cannot be
  // committed using evidence from a different tracking episode.
  if (reacquired && recent_probation_) {
    output.template_action = TemplateAction::kRollbackRecent;
    recent_probation_ = false;
    probation_reliable_frames_ = 0;
    stable_frames_ = 0;
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

  if (batch->generation != detector_eligibility_generation_) {
    const bool invalidated_by_coast =
        batch->generation >= detector_session_generation_ &&
        batch->generation < detector_eligibility_generation_;
    reject_eligible_candidates(invalidated_by_coast
        ? DetectorCandidateRejection::kHoldInvalidated
        : DetectorCandidateRejection::kGenerationInvalidated);
    result.status = DetectorBatchStatus::kInvalid;
    return result;
  }
  if (!last_detector_request_frame_ || !last_detector_request_ms_ ||
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
    return result;
  }
  if (plan.mode == TrackingMode::kOcclusionHold) {
    reject_eligible_candidates(DetectorCandidateRejection::kHoldInvalidated);
    result.status = DetectorBatchStatus::kInvalid;
    return result;
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
    const float center_ratio = frame_normalized_center_distance(
        candidate.bbox, plan.motion_predicted_bbox, frame_size_);
    diagnostic.center_ratio = center_ratio;
    if (center_ratio <=
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
  if (positioned.size() > config_.detector_max_candidates) {
    for (std::size_t index = config_.detector_max_candidates;
         index < positioned.size(); ++index) {
      result.diagnostics[positioned[index].diagnostic_index].rejection =
          DetectorCandidateRejection::kCapacityRejected;
    }
    positioned.resize(config_.detector_max_candidates);
  }
  detector_candidates_.clear();
  for (const PositionedCandidate& positioned_candidate : positioned) {
    DetectorCandidateDiagnostic& diagnostic =
        result.diagnostics[positioned_candidate.diagnostic_index];
    const DetectorCandidate& candidate = diagnostic.candidate;
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
    queued.probes = detector_probes(candidate);
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
    throw std::logic_error("finish_frame does not match active search plan");
  }
  ControllerOutput output;
  output.frame_id = plan.frame_id;
  output.target_class = target_class_;
  output.timestamp_ms = plan.timestamp_ms;
  output.mode = plan.mode;
  output.bbox = plan.predicted_bbox;
  output.search_scale = plan.search_scale;
  output.hypothesis_source = plan.hypothesis_source;
  output.detector_candidate_id = plan.detector_candidate_id;
  output.detector_same_class_candidates =
      plan.detector_same_class_candidates;
  output.detector_same_class_ambiguous =
      plan.detector_same_class_ambiguous;
  output.detector_candidate_locked = plan.detector_candidate_locked;
  output.detector_probe_index = plan.detector_probe_index;

  const DetectorIngestResult ingest_result =
      ingest_detector_batch(plan, detector_batch);
  output.detector_status = ingest_result.status;
  output.detector_position_valid_count =
      ingest_result.position_valid_count;
  output.detector_position_rejected_count =
      ingest_result.position_rejected_count;
  output.detector_candidate_diagnostics = ingest_result.diagnostics;

  std::optional<EvaluatedObservation> anchor;
  std::optional<EvaluatedObservation> recent;
  std::optional<std::uint64_t> token;
  for (const TrackerObservation& observation : observations) {
    // Skipped coast frames retain their existing observation-isolation contract.
    if (plan.mode == TrackingMode::kOcclusionHold && !plan.run_tracker) break;
    if (token && observation.search_token != *token) {
      throw std::invalid_argument("template observations must share one search token");
    }
    token = observation.search_token;
    EvaluatedObservation evaluated = evaluate(observation, plan);
    if (observation.template_kind == TemplateKind::kAnchor) {
      if (anchor) throw std::invalid_argument("duplicate anchor observation");
      anchor = evaluated;
    } else {
      if (recent) throw std::invalid_argument("duplicate recent observation");
      recent = evaluated;
    }
  }

  std::optional<EvaluatedObservation> selected;
  bool force_confirmation = false;
  if (anchor && recent) {
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
      accept_observation(plan, *selected, true, output);
      output.motion_control_authorized = false;
      output.occlusion_hold_remaining_frames = 0;
      output.occlusion_probe_confirmation_hits = 2;
      last_frame_id_ = plan.frame_id;
      last_timestamp_ms_ = plan.timestamp_ms;
      frame_open_ = false;
      return output;
    }
    // No observation-side effects are allowed before two consistent hits.
    occlusion_coast_age_frames_ = plan.occlusion_coast_age_frames;
    if (hold_remaining_frames_ > 0) --hold_remaining_frames_;
    if (hold_remaining_frames_ == 0) coast_probe_candidate_.reset();
    mode_ = hold_remaining_frames_ > 0 ? TrackingMode::kOcclusionHold
                                      : mode_for_timestamp(plan.timestamp_ms);
    last_frame_id_ = plan.frame_id;
    last_timestamp_ms_ = plan.timestamp_ms;
    frame_open_ = false;
    return output;
  }

  bool accepted = false;
  bool reacquired = false;
  const bool detector_verification_active =
      plan.hypothesis_source == HypothesisSource::kDetector ||
      active_detector_candidate_.has_value();
  const bool hold_source_mode =
      plan.mode == TrackingMode::kTracking ||
      plan.mode == TrackingMode::kUncertain ||
      plan.mode == TrackingMode::kLocalRecovery;
  if (selected && !hold_used_for_loss_) {
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
  } else if (!selected) {
    occlusion_decision_ = occlusion_evidence_.observe_gap();
  }
  publish_occlusion_evidence(output);
  if (selected && !hold_used_for_loss_ && hold_source_mode &&
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
    output.bbox = motion_.current_bbox();
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
    last_frame_id_ = plan.frame_id;
    last_timestamp_ms_ = plan.timestamp_ms;
    frame_open_ = false;
    return output;
  }
  if (selected) {
    output.quality = selected->quality;
    output.selected_template = selected->observation.template_kind;
    output.selected_metrics = selected->observation.metrics;
    output.normalized_center_residual = selected->center_residual;
    output.area_log_change = selected->area_log_change;
    output.aspect_log_change = selected->aspect_log_change;
    if (selected->quality == QualityLevel::kReliable &&
        plan.hypothesis_source != HypothesisSource::kDetector) {
      const bool recovery = loss_start_ms_.has_value() ||
                            plan.mode != TrackingMode::kTracking ||
                            force_confirmation;
      if (!recovery) {
        accepted = true;
      } else if (add_confirmation(plan.frame_id, selected->observation.bbox)) {
        accepted = true;
        reacquired = true;
      }
    }
  }

  const bool detector_plan =
      plan.hypothesis_source == HypothesisSource::kDetector &&
      plan.detector_candidate_id && active_detector_candidate_ &&
      active_detector_candidate_->queued.id == *plan.detector_candidate_id;
  bool discard_active_detector = false;
  if (detector_plan) {
    ActiveDetectorCandidate& active = *active_detector_candidate_;
    const bool templates_agree =
        anchor && recent && anchor->quality == QualityLevel::kReliable &&
        recent->quality == QualityLevel::kReliable &&
        output.template_iou >= config_.template_agreement_iou;
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
      reacquired = true;
    } else if (active.locked &&
               active.confirmation_attempts >=
                   static_cast<std::size_t>(config_.confirmation_window)) {
      discard_active_detector = true;
    }
  }

  if (accepted) {
    accept_observation(plan, *selected, reacquired, output);
  } else {
    ++consecutive_unreliable_frames_;
    if (!loss_start_ms_) loss_start_ms_ = plan.timestamp_ms;
    mode_ = mode_for_timestamp(plan.timestamp_ms);
    output.mode = mode_;
    output.control_valid = false;
    output.accepted = false;
    stable_frames_ = 0;
  }

  if (!accepted && discard_active_detector) {
    active_detector_candidate_.reset();
    confirmations_.clear();
  }

  if (recent_probation_) {
    const bool templates_confirm_staged_recent =
        anchor && recent && anchor->quality == QualityLevel::kReliable &&
        recent->quality == QualityLevel::kReliable &&
        output.template_iou >= config_.template_agreement_iou;
    if (accepted && templates_confirm_staged_recent &&
        output.quality == QualityLevel::kReliable && !reacquired &&
        !force_confirmation) {
      ++probation_reliable_frames_;
      if (probation_reliable_frames_ >= config_.recent_probation_frames) {
        output.template_action = TemplateAction::kCommitRecent;
        recent_probation_ = false;
        probation_reliable_frames_ = 0;
        stable_frames_ = 0;
        last_recent_update_ms_ = plan.timestamp_ms;
      }
    } else {
      output.template_action = TemplateAction::kRollbackRecent;
      recent_probation_ = false;
      probation_reliable_frames_ = 0;
      stable_frames_ = 0;
    }
  } else if (accepted && !reacquired && !force_confirmation &&
             stable_frames_ >= config_.stable_frames_for_update &&
             plan.timestamp_ms - last_recent_update_ms_ >=
                 config_.recent_update_interval_ms) {
    output.template_action = TemplateAction::kStageRecent;
    recent_probation_ = true;
    probation_reliable_frames_ = 0;
    stable_frames_ = 0;
  }

  output.detector_candidates_queued = detector_candidates_.size();
  last_frame_id_ = plan.frame_id;
  last_timestamp_ms_ = plan.timestamp_ms;
  frame_open_ = false;
  return output;
} catch (...) {
  // Keep the frame open until it has a complete result so malformed probe
  // identities and validation errors revoke coast just like inference errors.
  abort_frame();
  throw;
}

}  // namespace nanotrack
