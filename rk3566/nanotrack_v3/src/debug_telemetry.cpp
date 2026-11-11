#include "debug_telemetry.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace nanotrack {
namespace {
std::string optional_integer(const std::optional<bool>& value) {
  return value ? (*value ? "1" : "0") : "none";
}

std::ostringstream event_stream(const char* event) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(3)
         << "DEBUG_EVENT event=" << event;
  return stream;
}

std::string optional_id(const std::optional<std::uint64_t>& value) {
  return value ? std::to_string(*value) : "none";
}

std::string optional_bbox(const std::optional<BBox>& value) {
  if (!value) return "none";
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(2) << value->x << ',' << value->y
         << ',' << value->width << ',' << value->height;
  return stream.str();
}

std::string optional_ratio(const std::optional<float>& value) {
  if (!value) return "none";
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(3) << *value;
  return stream.str();
}

std::string bbox_value(const BBox& bbox) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(2) << bbox.x << ',' << bbox.y
         << ',' << bbox.width << ',' << bbox.height;
  return stream.str();
}

const char* rejection_name(DetectorCandidateRejection rejection) {
  switch (rejection) {
    case DetectorCandidateRejection::kNone: return "NONE";
    case DetectorCandidateRejection::kInvalid: return "INVALID";
    case DetectorCandidateRejection::kGenerationInvalidated:
      return "GENERATION_INVALIDATED";
    case DetectorCandidateRejection::kFrameMismatch: return "FRAME_MISMATCH";
    case DetectorCandidateRejection::kStale: return "STALE";
    case DetectorCandidateRejection::kHoldInvalidated:
      return "HOLD_INVALIDATED";
    case DetectorCandidateRejection::kTargetClassUnknown:
      return "TARGET_CLASS_UNKNOWN";
    case DetectorCandidateRejection::kClassMismatch:
      return "CLASS_MISMATCH";
    case DetectorCandidateRejection::kPositionRejected:
      return "POSITION_REJECTED";
    case DetectorCandidateRejection::kCapacityRejected:
      return "CAPACITY_REJECTED";
  }
  return "UNKNOWN";
}

const char* acceptance_path_name(AcceptedObservationPath path) {
  switch (path) {
    case AcceptedObservationPath::kDirect: return "direct";
    case AcceptedObservationPath::kShortConfirmation:
      return "short_confirmation";
    case AcceptedObservationPath::kLocalOrDeeperRecovery:
      return "local_or_deeper_recovery";
    case AcceptedObservationPath::kDetectorRecovery:
      return "detector_recovery";
    case AcceptedObservationPath::kOcclusionProbeRecovery:
      return "occlusion_probe_recovery";
    case AcceptedObservationPath::kSearchReacquisition:
      return "search_reacquisition";
  }
  return "unknown";
}

std::string sanitized_value(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (const unsigned char character : value) {
    sanitized += std::isspace(character) ? '_' : static_cast<char>(character);
  }
  return sanitized.empty() ? "none" : sanitized;
}

std::string optional_class_id(const std::optional<int>& value) {
  return value ? std::to_string(*value) : "none";
}

std::string class_name(const std::optional<int>& class_id,
                       const std::vector<std::string>& labels) {
  if (!class_id) return "none";
  if (*class_id >= 0 && static_cast<std::size_t>(*class_id) < labels.size() &&
      !labels[*class_id].empty()) {
    return sanitized_value(labels[*class_id]);
  }
  return "cls=" + std::to_string(*class_id);
}

}  // namespace

DebugTelemetry::DebugTelemetry(std::size_t heartbeat_interval)
    : heartbeat_interval_(heartbeat_interval) {
  if (heartbeat_interval_ == 0) {
    throw std::invalid_argument("debug heartbeat interval must be positive");
  }
}

void DebugTelemetry::reset() {
  observed_frames_ = 0;
  has_previous_state_ = false;
  previous_mode_ = TrackingMode::kTracking;
  previous_phase_ = TrackingPhase::kInvalid;
  previous_control_source_ = ControlSource::kNone;
  previous_motion_prediction_confident_ = false;
  previous_control_valid_ = false;
  loss_start_timestamp_ms_.reset();
}

std::vector<std::string> DebugTelemetry::initialization_events(
    const TargetClassInitializationResult& result,
    const std::vector<std::string>& labels) const {
  std::vector<std::string> events;
  const TargetClassState& state = result.target_class;
  std::ostringstream start = event_stream("target_class_init_start");
  start << " frame=" << result.frame_id
        << " timestamp_ms=" << result.timestamp_ms
        << " initialization_index=" << result.initialization_index
        << " request_id=" << optional_id(result.request_id)
        << " roi=" << bbox_value(result.original_roi)
        << " mode=" << (result.requested_class_id == -1 ? "AUTO" : "FIXED")
        << " requested_class_id=" << result.requested_class_id
        << " candidate_count=" << state.candidates.size();
  events.push_back(start.str());

  for (const TargetClassCandidateMatch& match : state.candidates) {
    const bool selected = state.selected_candidate &&
        state.selected_candidate->original_index == match.original_index;
    std::ostringstream candidate = event_stream("target_class_candidate");
    candidate << " frame=" << result.frame_id
              << " initialization_index=" << result.initialization_index
              << " request_id=" << optional_id(result.request_id)
              << " index=" << match.original_index
              << " class_id=" << match.candidate.class_id
              << " class_name=" << class_name(match.candidate.class_id, labels)
              << " bbox=" << bbox_value(match.candidate.bbox)
              << " clipped_bbox=" << bbox_value(match.clipped_bbox)
              << " confidence=" << match.candidate.confidence
              << " roi_coverage=" << match.roi_coverage
              << " center_ratio=" << match.center_ratio
              << " iou=" << match.iou
              << " score=" << match.score
              << " eligible=" << (match.eligible ? 1 : 0)
              << " selected=" << (selected ? 1 : 0)
              << " rejection=" << to_string(match.rejection);
    events.push_back(candidate.str());
  }

  std::ostringstream summary = event_stream("target_class_init_result");
  summary << " frame=" << result.frame_id
          << " initialization_index=" << result.initialization_index
          << " request_id=" << optional_id(result.request_id)
          << " status=" << to_string(state.status)
          << " class_id=" << optional_class_id(state.target_class_id)
          << " class_name=" << class_name(state.target_class_id, labels)
          << " reason=" << to_string(state.reason)
          << " candidate_count=" << state.candidates.size()
          << " best_class_id=" << optional_class_id(state.best_class_id)
          << " best_score=" << state.best_score
          << " runner_up_class_id=" << optional_class_id(state.runner_up_class_id)
          << " runner_up_score=" << state.runner_up_score
          << " target_class_allows_recovery="
          << (state.target_class_allows_recovery ? 1 : 0)
          << " detector_ms=" << result.detector_ms
          << " preprocess_ms=" << result.detector_timing.preprocess_ms
          << " inference_ms=" << result.detector_timing.inference_ms
          << " postprocess_ms=" << result.detector_timing.postprocess_ms
          << " total_ms=" << result.detector_timing.total_ms
          << " error=" << sanitized_value(result.detector_error);
  events.push_back(summary.str());
  return events;
}

std::vector<std::string> DebugTelemetry::frame_events(
    const RuntimeFrameResult& result,
    float pipeline_ms,
    bool force_heartbeat) {
  std::vector<std::string> events;
  ++observed_frames_;
  const bool was_hold = has_previous_state_ &&
                        previous_mode_ == TrackingMode::kOcclusionHold;
  const auto emit_coast = [&](const char* name, const char* extra = "") {
    const auto& o = result.output;
    std::ostringstream stream = event_stream(name);
    stream << " frame=" << o.frame_id
           << " coast_age=" << o.occlusion_coast_age_frames
           << " reference_source=" << to_string(o.occlusion_reference_source)
           << " reference_width=" << optional_ratio(o.occlusion_reference_width)
           << " reference_height=" << optional_ratio(o.occlusion_reference_height)
           << " width_ratio=" << optional_ratio(o.occlusion_width_ratio)
           << " height_ratio=" << optional_ratio(o.occlusion_height_ratio)
           << " suspicion_active=" << o.occlusion_suspicion_active
           << " suspicion_started=" << o.occlusion_suspicion_started
           << " suspicion_cleared=" << o.occlusion_suspicion_cleared
           << " suspicion_frames=" << o.occlusion_suspicion_frames
           << " clean_frames=" << o.occlusion_clean_frames
           << " width_shrink_frames=" << o.occlusion_width_shrink_frames
           << " height_shrink_frames=" << o.occlusion_height_shrink_frames
           << " remaining=" << o.occlusion_hold_remaining_frames
           << " severe=" << o.occlusion_severe_evidence
           << " gradual=" << o.occlusion_gradual_evidence
           << " template=" << o.occlusion_template_evidence
           << " peak=" << o.occlusion_peak_margin_evidence
           << " motion_residual=" << o.occlusion_motion_residual_evidence
           << " trigger_reason=" << to_string(o.occlusion_trigger_reason)
           << " probe=" << to_string(o.occlusion_probe_type)
           << " confirmation=" << o.occlusion_probe_confirmation_hits
           << '/' << o.occlusion_probe_confirmation_required
           << " rejection=" << o.occlusion_probe_rejection
           << " probe_bbox=" << optional_bbox(o.occlusion_probe_bbox)
           << " anchor_reliable=" << optional_integer(o.occlusion_probe_anchor_reliable)
           << " recent_reliable=" << optional_integer(o.occlusion_probe_recent_reliable)
           << " template_gate=" << optional_integer(o.occlusion_probe_template_gate)
           << " cross_gate=" << optional_integer(o.occlusion_probe_cross_gate)
           << " cross_iou=" << optional_integer(o.occlusion_probe_cross_iou)
           << " cross_center=" << optional_integer(o.occlusion_probe_cross_center)
           << " cross_area=" << optional_integer(o.occlusion_probe_cross_area)
           << " cross_aspect=" << optional_integer(o.occlusion_probe_cross_aspect)
           << " occlusion_probe_anchor_score_gate=" << optional_integer(o.occlusion_probe_anchor_score_gate)
           << " occlusion_probe_anchor_center_gate=" << optional_integer(o.occlusion_probe_anchor_center_gate)
           << " occlusion_probe_anchor_area_gate=" << optional_integer(o.occlusion_probe_anchor_area_gate)
           << " occlusion_probe_anchor_aspect_gate=" << optional_integer(o.occlusion_probe_anchor_aspect_gate)
           << " occlusion_probe_recent_score_gate=" << optional_integer(o.occlusion_probe_recent_score_gate)
           << " occlusion_probe_recent_center_gate=" << optional_integer(o.occlusion_probe_recent_center_gate)
           << " occlusion_probe_recent_area_gate=" << optional_integer(o.occlusion_probe_recent_area_gate)
           << " occlusion_probe_recent_aspect_gate=" << optional_integer(o.occlusion_probe_recent_aspect_gate)
           << " quality=" << to_string(o.quality)
           << " template_iou=" << o.template_iou
           << " center_residual=" << o.normalized_center_residual
           << " area_change=" << o.area_log_change
           << " aspect_change=" << o.aspect_log_change
           << " motion_control_authorized=" << o.motion_control_authorized
           << " control_valid=" << o.control_valid
           << " motion_bbox=" << bbox_value(result.plan.motion_predicted_bbox)
           << " control_bbox=" << bbox_value(o.bbox)
           << extra;
    for (const auto& observation : result.observations) {
      stream << (observation.template_kind == TemplateKind::kAnchor
                     ? " anchor_bbox=" : " recent_bbox=")
             << bbox_value(observation.bbox);
    }
    events.push_back(stream.str());
  };
  const auto& o = result.output;
  if ((!has_previous_state_ || previous_phase_ != TrackingPhase::kSearching) &&
      o.tracking_phase == TrackingPhase::kSearching) {
    std::ostringstream stream = event_stream("search_enter");
    stream << " frame=" << o.frame_id
           << " control_source=" << to_string(o.control_source)
           << " search_age_ms=" << o.motion_search_age_ms
           << " confident=" << (o.motion_prediction_confident ? 1 : 0);
    events.push_back(stream.str());
  }
  if (has_previous_state_ &&
      previous_phase_ == TrackingPhase::kSearching &&
      previous_motion_prediction_confident_ &&
      o.tracking_phase == TrackingPhase::kSearching &&
      !o.motion_prediction_confident) {
    std::ostringstream stream = event_stream("search_expired");
    stream << " frame=" << o.frame_id
           << " search_age_ms=" << o.motion_search_age_ms;
    events.push_back(stream.str());
  }
  if (o.search_hint && o.search_hint->confirmation_count == 1 &&
      o.search_hint->rejection == SearchHintRejection::kNone) {
    std::ostringstream stream = event_stream("hint_first_hit");
    stream << " frame=" << o.frame_id
           << " candidate_id=" << o.search_hint->candidate_id
           << " source_frame=" << o.search_hint->source_frame_id;
    events.push_back(stream.str());
  }
  if (o.search_hint &&
      o.search_hint->rejection != SearchHintRejection::kNone) {
    std::ostringstream stream = event_stream("hint_reject");
    stream << " frame=" << o.frame_id
           << " candidate_id=" << o.search_hint->candidate_id
           << " reason=" << to_string(o.search_hint->rejection);
    events.push_back(stream.str());
  }
  if (o.reacquired && o.accepted_observation_path ==
                          AcceptedObservationPath::kSearchReacquisition) {
    std::ostringstream stream = event_stream("reacquired");
    stream << " frame=" << o.frame_id
           << " confirmation_count="
           << (o.search_hint ? o.search_hint->confirmation_count : 0)
           << " control_source=" << to_string(o.control_source);
    events.push_back(stream.str());
  }
  if (o.accepted && o.selected_observation_bbox) {
    std::ostringstream stream = event_stream("center_filter");
    stream << " frame=" << o.frame_id
           << " raw_bbox=" << bbox_value(*o.selected_observation_bbox)
           << " motion_bbox=" << bbox_value(result.plan.motion_predicted_bbox)
           << " innovation=" << o.center_innovation_x << ','
           << o.center_innovation_y
           << " correction=" << o.center_correction_x << ','
           << o.center_correction_y
           << " limited=" << (o.center_correction_limited ? 1 : 0)
           << " acceptance_path="
           << acceptance_path_name(o.accepted_observation_path)
           << " reacquired=" << (o.reacquired ? 1 : 0)
           << " final_bbox=" << bbox_value(o.bbox);
    events.push_back(stream.str());
  }
  if (o.occlusion_suspicion_started) emit_coast("occlusion_suspicion_start");
  if (o.occlusion_suspicion_cleared) emit_coast("occlusion_suspicion_clear");
  if (o.occlusion_hold_triggered) emit_coast("occlusion_coast_enter");
  if (o.mode == TrackingMode::kOcclusionHold && !o.occlusion_hold_triggered)
    emit_coast("occlusion_coast_tick");
  if (o.occlusion_probe_type != OcclusionProbeType::kNone) {
    emit_coast(o.occlusion_probe_type == OcclusionProbeType::kSearch
                   ? "occlusion_probe_search" : "occlusion_probe_confirm");
    if (o.occlusion_probe_rejection != "NONE")
      emit_coast("occlusion_probe_rejected");
  }
  if (was_hold && o.reacquired) emit_coast("occlusion_reacquired");
  if (was_hold && o.mode != TrackingMode::kOcclusionHold)
    emit_coast("occlusion_coast_exit",
               o.reacquired ? " exit_reason=REACQUIRED" : " exit_reason=TIMEOUT_OR_ABORT");
  if (!result.output.control_valid && !loss_start_timestamp_ms_) {
    loss_start_timestamp_ms_ = result.output.timestamp_ms;
  }

  if (!has_previous_state_ || result.output.mode != previous_mode_) {
    std::ostringstream stream = event_stream("state_change");
    stream << " frame=" << result.output.frame_id
           << " from="
           << (has_previous_state_ ? to_string(previous_mode_) : "none")
           << " to=" << to_string(result.output.mode)
           << " quality=" << to_string(result.output.quality)
           << " control_valid=" << (result.output.control_valid ? 1 : 0);
    events.push_back(stream.str());
  } else if (result.output.control_valid != previous_control_valid_) {
    std::ostringstream stream = event_stream("control_change");
    stream << " frame=" << result.output.frame_id
           << " mode=" << to_string(result.output.mode)
           << " from=" << (previous_control_valid_ ? 1 : 0)
           << " to=" << (result.output.control_valid ? 1 : 0);
    events.push_back(stream.str());
  }
  has_previous_state_ = true;
  previous_mode_ = result.output.mode;
  previous_phase_ = result.output.tracking_phase;
  previous_control_source_ = result.output.control_source;
  previous_motion_prediction_confident_ =
      result.output.motion_prediction_confident;
  previous_control_valid_ = result.output.control_valid;

  const bool detector_suppressed = result.tracker_skipped &&
                                   !result.plan.request_detector;
  if (result.output.occlusion_hold_triggered) {
    std::ostringstream stream = event_stream("occlusion_hold_enter");
    stream << " frame=" << result.output.frame_id
           << " remaining=" << result.output.occlusion_hold_remaining_frames
           << " consecutive_unreliable="
           << result.output.occlusion_consecutive_unreliable_frames
           << " reference=" << std::setprecision(2)
           << result.output.occlusion_reference_width.value_or(0.0F) << ','
           << result.output.occlusion_reference_height.value_or(0.0F)
           << " observation="
           << result.output.occlusion_observation_width.value_or(0.0F) << ','
           << result.output.occlusion_observation_height.value_or(0.0F)
           << std::setprecision(3)
           << " width_ratio="
           << optional_ratio(result.output.occlusion_width_ratio)
           << " height_ratio="
           << optional_ratio(result.output.occlusion_height_ratio)
           << " tracker_skipped=" << (result.tracker_skipped ? 1 : 0)
           << " detector_suppressed=" << (detector_suppressed ? 1 : 0);
    events.push_back(stream.str());
  }
  if (result.output.mode == TrackingMode::kOcclusionHold &&
      !result.output.occlusion_hold_triggered) {
    std::ostringstream stream = event_stream("occlusion_hold_tick");
    stream << " frame=" << result.output.frame_id
           << " remaining=" << result.output.occlusion_hold_remaining_frames
           << " motion=" << bbox_value(result.plan.motion_predicted_bbox)
           << " tracker_skipped=" << (result.tracker_skipped ? 1 : 0)
           << " detector_suppressed=" << (detector_suppressed ? 1 : 0);
    events.push_back(stream.str());
  }
  if (was_hold && result.output.mode != TrackingMode::kOcclusionHold) {
    const std::int64_t loss_duration = loss_start_timestamp_ms_
        ? std::max<std::int64_t>(0, result.output.timestamp_ms -
                                       *loss_start_timestamp_ms_)
        : 0;
    std::ostringstream stream = event_stream("occlusion_hold_exit");
    stream << " frame=" << result.output.frame_id
           << " next_state=" << to_string(result.output.mode)
           << " loss_duration_ms=" << loss_duration;
    events.push_back(stream.str());
  }

  for (std::size_t index = 0;
       index < result.output.detector_candidate_diagnostics.size(); ++index) {
    const DetectorCandidateDiagnostic& diagnostic =
        result.output.detector_candidate_diagnostics[index];
    std::ostringstream stream = event_stream("detector_candidate");
    stream << " frame=" << result.output.frame_id
           << " index=" << index
           << " bbox=" << bbox_value(diagnostic.candidate.bbox)
           << " class_id=" << diagnostic.candidate.class_id
           << " confidence=" << diagnostic.candidate.confidence
           << " center_ratio=" << optional_ratio(diagnostic.center_ratio)
           << " source_frame="
           << (result.detector.batch_received
                   ? std::to_string(result.detector.batch_frame_id)
                   : "none")
           << " age_ms="
           << (result.detector.batch_received
                   ? std::to_string(result.detector.batch_age_ms)
                   : "none")
           << " eligible=" << (diagnostic.batch_eligible ? 1 : 0)
           << " position_pass=" << (diagnostic.position_gate_pass ? 1 : 0)
           << " queued=" << (diagnostic.queued ? 1 : 0)
           << " candidate_id=" << optional_id(diagnostic.candidate_id)
           << " rejection=" << rejection_name(diagnostic.rejection)
           << " class_disposition=" << to_string(diagnostic.class_disposition)
           << " final_rejection=" << rejection_name(diagnostic.rejection);
    events.push_back(stream.str());
  }

  if (result.detector.request_submitted) {
    std::ostringstream stream = event_stream("detector_request");
    stream << " frame=" << result.output.frame_id
           << " request_frame=" << result.detector.request_frame_id;
    events.push_back(stream.str());
  }
  if (result.detector.batch_received) {
    std::ostringstream stream = event_stream("detector_batch");
    stream << " frame=" << result.output.frame_id
           << " source_frame=" << result.detector.batch_frame_id
           << " age_ms=" << result.detector.batch_age_ms
           << " raw=" << result.detector.raw_candidate_count
           << " filtered=" << result.detector.filtered_candidate_count
           << " position_valid="
           << result.output.detector_position_valid_count
           << " position_rejected="
           << result.output.detector_position_rejected_count
           << " status=" << to_string(result.output.detector_status)
           << " total_ms=" << result.detector.timing.total_ms;
    events.push_back(stream.str());
  }
  if (!result.detector.error.empty()) {
    std::ostringstream stream = event_stream("detector_error");
    stream << " frame=" << result.output.frame_id
           << " message=" << sanitized_value(result.detector.error);
    events.push_back(stream.str());
  }

  if (result.output.hypothesis_source == HypothesisSource::kDetector) {
    std::ostringstream stream = event_stream("detector_verify");
    stream << " frame=" << result.output.frame_id
           << " candidate_id=" << optional_id(result.output.detector_candidate_id)
           << " candidate_region="
           << optional_bbox(result.plan.detector_candidate_region)
           << " center_ratio="
           << optional_ratio(result.plan.detector_candidate_center_ratio)
           << " probe=" << result.output.detector_probe_index
           << " same_class=" << result.output.detector_same_class_candidates
           << " ambiguous="
           << (result.output.detector_same_class_ambiguous ? 1 : 0)
           << " locked=" << (result.output.detector_candidate_locked ? 1 : 0)
           << " confirmation=" << result.output.detector_confirmation_hits
           << '/' << result.output.detector_confirmation_attempts
           << " quality=" << to_string(result.output.quality)
           << " template_iou=" << result.output.template_iou
           << " accepted=" << (result.output.accepted ? 1 : 0)
           << " control_valid=" << (result.output.control_valid ? 1 : 0);
    events.push_back(stream.str());
  }

  if (result.output.reacquired) {
    std::ostringstream stream = event_stream("reacquired");
    stream << " frame=" << result.output.frame_id
           << " source=" << to_string(result.output.hypothesis_source)
           << " candidate_id=" << optional_id(result.output.detector_candidate_id)
           << " confirmation=" << result.output.detector_confirmation_hits
           << '/' << result.output.detector_confirmation_attempts
           << " control_valid=" << (result.output.control_valid ? 1 : 0)
           << std::setprecision(2)
           << " bbox=" << result.output.bbox.x << ',' << result.output.bbox.y
           << ',' << result.output.bbox.width << ',' << result.output.bbox.height;
    events.push_back(stream.str());
  }

  if (result.output.template_action != TemplateAction::kNone) {
    std::ostringstream stream = event_stream("template_action");
    stream << " frame=" << result.output.frame_id
           << " action=" << to_string(result.output.template_action);
    events.push_back(stream.str());
  }

  if (force_heartbeat || observed_frames_ % heartbeat_interval_ == 0) {
    std::ostringstream stream = event_stream("frame_status");
    stream << " frame=" << result.output.frame_id
           << " mode=" << to_string(result.output.mode)
           << " quality=" << to_string(result.output.quality)
           << " control_valid=" << (result.output.control_valid ? 1 : 0)
           << " accepted=" << (result.output.accepted ? 1 : 0)
           << " source=" << to_string(result.output.hypothesis_source)
           << std::setprecision(2)
           << " bbox=" << result.output.bbox.x << ',' << result.output.bbox.y
           << ',' << result.output.bbox.width << ',' << result.output.bbox.height
           << std::setprecision(3)
           << " score=" << result.output.selected_metrics.raw_score
           << " psr=" << result.output.selected_metrics.psr
           << " apce=" << result.output.selected_metrics.apce
           << " peak_margin=" << result.output.selected_metrics.peak_margin
           << " tracker_ms=" << result.tracker_ms
           << " runtime_ms=" << result.runtime_ms
           << " pipeline_ms=" << pipeline_ms;
    events.push_back(stream.str());
  }

  if (result.output.control_valid) loss_start_timestamp_ms_.reset();

  return events;
}

}  // namespace nanotrack
