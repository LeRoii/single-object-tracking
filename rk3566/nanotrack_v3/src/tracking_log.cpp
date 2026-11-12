#include "tracking_log.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nanotrack {
namespace {

std::string number(float value) {
  if (!std::isfinite(value)) return "";
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6) << value;
  return stream.str();
}

std::string csv_escape(const std::string& value) {
  if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
  std::string escaped = "\"";
  for (const char character : value) {
    if (character == '"') escaped += '"';
    escaped += character;
  }
  escaped += '"';
  return escaped;
}

std::string json_escape(const std::string& value) {
  std::ostringstream stream;
  for (const unsigned char character : value) {
    switch (character) {
      case '\\': stream << "\\\\"; break;
      case '"': stream << "\\\""; break;
      case '\n': stream << "\\n"; break;
      case '\r': stream << "\\r"; break;
      case '\t': stream << "\\t"; break;
      default:
        if (character < 0x20) {
          stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(character) << std::dec << std::setfill(' ');
        } else {
          stream << character;
        }
    }
  }
  return stream.str();
}

template <typename Value>
std::string optional_integer(const std::optional<Value>& value) {
  return value ? std::to_string(*value) : "";
}

std::string optional_number(const std::optional<float>& value) {
  return value ? number(*value) : "";
}

const char* acceptance_path_name(AcceptedObservationPath path) {
  switch (path) {
    case AcceptedObservationPath::kDirect: return "direct";
    case AcceptedObservationPath::kTemplateBridge: return "template_bridge";
    case AcceptedObservationPath::kShortConfirmation: return "short_confirmation";
    case AcceptedObservationPath::kLocalOrDeeperRecovery:
      return "local_or_deeper_recovery";
    case AcceptedObservationPath::kDetectorRecovery: return "detector_recovery";
    case AcceptedObservationPath::kOcclusionProbeRecovery:
      return "occlusion_probe_recovery";
    case AcceptedObservationPath::kSearchReacquisition:
      return "search_reacquisition";
  }
  return "unknown";
}

template <typename Value>
void append_json_optional(std::ostream& stream, const std::optional<Value>& value) {
  if (value) stream << *value;
  else stream << "null";
}

void append_json_number(std::ostream& stream, float value) {
  if (std::isfinite(value)) stream << value;
  else stream << "null";
}

void append_json_bbox(std::ostream& stream, const BBox& bbox) {
  stream << "{\"x\": ";
  append_json_number(stream, bbox.x);
  stream << ", \"y\": ";
  append_json_number(stream, bbox.y);
  stream << ", \"width\": ";
  append_json_number(stream, bbox.width);
  stream << ", \"height\": ";
  append_json_number(stream, bbox.height);
  stream << '}';
}

void append_json_candidate(std::ostream& stream,
                           const TargetClassCandidateMatch& candidate) {
  stream << "{\"original_index\": " << candidate.original_index
         << ", \"class_id\": " << candidate.candidate.class_id
         << ", \"bbox\": ";
  append_json_bbox(stream, candidate.candidate.bbox);
  stream << ", \"clipped_bbox\": ";
  append_json_bbox(stream, candidate.clipped_bbox);
  stream << ", \"confidence\": ";
  append_json_number(stream, candidate.candidate.confidence);
  stream << ", \"roi_coverage\": ";
  append_json_number(stream, candidate.roi_coverage);
  stream << ", \"iou\": ";
  append_json_number(stream, candidate.iou);
  stream << ", \"center_ratio\": ";
  append_json_number(stream, candidate.center_ratio);
  stream << ", \"score\": ";
  append_json_number(stream, candidate.score);
  stream << ", \"eligible\": " << (candidate.eligible ? "true" : "false")
         << ", \"rejection\": \"" << to_string(candidate.rejection) << "\"}";
}

void append_json_initialization(std::ostream& stream,
                                const TargetClassInitializationResult& result) {
  stream << "    {\"initialization_index\": " << result.initialization_index
         << ", \"frame_id\": " << result.frame_id
         << ", \"timestamp_ms\": " << result.timestamp_ms
         << ", \"original_roi\": ";
  append_json_bbox(stream, result.original_roi);
  stream << ", \"requested_class_id\": " << result.requested_class_id
         << ", \"request_id\": ";
  append_json_optional(stream, result.request_id);
  stream << ", \"status\": \"" << to_string(result.target_class.status)
         << "\", \"target_class_id\": ";
  append_json_optional(stream, result.target_class.target_class_id);
  stream << ", \"reason\": \"" << to_string(result.target_class.reason)
         << "\", \"detector_recovery_enabled\": "
         << (result.target_class.target_class_allows_recovery ? "true" : "false")
         << ", \"best_class_id\": ";
  append_json_optional(stream, result.target_class.best_class_id);
  stream << ", \"best_score\": ";
  if (result.target_class.best_class_id) {
    append_json_number(stream, result.target_class.best_score);
  }
  else stream << "null";
  stream << ", \"runner_up_class_id\": ";
  append_json_optional(stream, result.target_class.runner_up_class_id);
  stream << ", \"runner_up_score\": ";
  if (result.target_class.runner_up_class_id) {
    append_json_number(stream, result.target_class.runner_up_score);
  } else {
    stream << "null";
  }
  stream << ", \"selected_candidate\": ";
  if (result.target_class.selected_candidate) {
    append_json_candidate(stream, *result.target_class.selected_candidate);
  } else {
    stream << "null";
  }
  stream << ", \"candidates\": [";
  for (std::size_t index = 0; index < result.target_class.candidates.size(); ++index) {
    if (index) stream << ", ";
    append_json_candidate(stream, result.target_class.candidates[index]);
  }
  stream << "], \"batch\": ";
  if (result.batch) {
    stream << "{\"request_id\": " << result.batch->request_id
           << ", \"source_frame_id\": " << result.batch->source_frame_id
           << ", \"source_timestamp_ms\": " << result.batch->source_timestamp_ms
           << ", \"raw_candidate_count\": " << result.batch->raw_candidate_count
           << ", \"filtered_candidate_count\": "
           << result.batch->filtered_candidate_count << '}';
  } else {
    stream << "null";
  }
  stream << ", \"detector_timing\": {\"preprocess_ms\": ";
  append_json_number(stream, result.detector_timing.preprocess_ms);
  stream << ", \"inference_ms\": ";
  append_json_number(stream, result.detector_timing.inference_ms);
  stream << ", \"postprocess_ms\": ";
  append_json_number(stream, result.detector_timing.postprocess_ms);
  stream << ", \"total_ms\": ";
  append_json_number(stream, result.detector_timing.total_ms);
  stream << "}, \"detector_ms\": ";
  append_json_number(stream, result.detector_ms);
  stream << ", \"detector_error\": \"" << json_escape(result.detector_error)
         << "\"}";
}

void append_metrics(std::vector<std::string>& fields,
                    bool present,
                    const ResponseMetrics& metrics) {
  if (!present) {
    fields.insert(fields.end(), 5, "");
    return;
  }
  fields.push_back(number(metrics.raw_score));
  fields.push_back(number(metrics.penalized_peak));
  fields.push_back(number(metrics.psr));
  fields.push_back(number(metrics.apce));
  fields.push_back(number(metrics.peak_margin));
}

std::string join(const std::vector<std::string>& fields) {
  std::ostringstream stream;
  for (std::size_t index = 0; index < fields.size(); ++index) {
    if (index) stream << ',';
    stream << fields[index];
  }
  return stream.str();
}

const char* candidate_source(RecoveryCandidateSource source) {
  switch (source) {
    case RecoveryCandidateSource::kRecent: return "RECENT";
    case RecoveryCandidateSource::kSafe: return "SAFE";
    case RecoveryCandidateSource::kDetector: return "DETECTOR";
  }
  return "UNKNOWN";
}

template <typename Field>
void visit_box(const std::string& prefix, const std::optional<BBox>& bbox, Field& field) {
  field(prefix + "x", bbox ? std::optional<float>(bbox->x) : std::nullopt);
  field(prefix + "y", bbox ? std::optional<float>(bbox->y) : std::nullopt);
  field(prefix + "width", bbox ? std::optional<float>(bbox->width) : std::nullopt);
  field(prefix + "height", bbox ? std::optional<float>(bbox->height) : std::nullopt);
}

template <typename Field>
void visit_head(const std::string& name, bool executed, bool reused,
                const std::optional<BBox>& bbox, const ResponseMetrics& metrics,
                Field& field) {
  field(name + "_head_executed", executed);
  field(name + "_decode_status", bbox ? "DECODED" :
        executed || reused ? "NO_DECODE" : "NOT_RUN");
  visit_box(name + "_raw_", bbox, field);
  field(name + "_raw_score", bbox ? std::optional<float>(metrics.raw_score) : std::nullopt);
  field(name + "_raw_penalized_peak", bbox ? std::optional<float>(metrics.penalized_peak) : std::nullopt);
  field(name + "_raw_psr", bbox ? std::optional<float>(metrics.psr) : std::nullopt);
  field(name + "_raw_apce", bbox ? std::optional<float>(metrics.apce) : std::nullopt);
  field(name + "_raw_peak_margin", bbox ? std::optional<float>(metrics.peak_margin) : std::nullopt);
}

template <typename Field>
void visit_observability_fields(const TrackingLogRow& row, Field field) {
  const auto& request = row.template_transition;
  const auto& revocation = row.template_slot_revocation;
  const auto& reference = row.trusted_response_reference;
  const auto& candidate = row.recovery_candidate_key;
  const auto& probe = row.recovery_probe;
  field("observation_present", row.observation_present);
  field("trusted_for_state_update", row.trusted_for_state_update);
  field("motion_update_valid", row.motion_update_valid);
  field("state_change_reason", row.state_change_reason);
  field("rect0_source", row.rect0_source);
  field("active_template", "RECENT");
  field("safe_template_used", row.template_heads.safe || row.template_heads.safe_reused_recent);
  field("safe_reused_recent", row.template_heads.safe_reused_recent);
  visit_head("recent", row.template_heads.recent, false, row.recent_raw_bbox, row.recent_metrics, field);
  visit_head("safe", row.template_heads.safe, row.template_heads.safe_reused_recent,
             row.safe_raw_bbox, row.safe_metrics, field);
  visit_head("staged", row.template_heads.staged, false, row.staged_raw_bbox, row.staged_metrics, field);
  field("trusted_baseline_psr", reference ? std::optional<float>(reference->psr) : std::nullopt);
  field("trusted_baseline_apce", reference ? std::optional<float>(reference->apce) : std::nullopt);
  field("trusted_baseline_peak_margin", reference ? std::optional<float>(reference->peak_margin) : std::nullopt);
  field("response_psr_relative_delta", row.response_psr_relative_delta);
  field("response_apce_relative_delta", row.response_apce_relative_delta);
  field("response_peak_margin_absolute_delta", row.response_peak_margin_absolute_delta);
  field("template_transaction_id", request ? std::optional<std::uint64_t>(request->transaction_id) : std::nullopt);
  field("template_slot_id", request ? std::optional<std::uint64_t>(request->template_slot_id) : std::nullopt);
  field("template_transition_recovery", request && request->recovery);
  field("template_transition_applied", row.template_transition_applied);
  field("template_transition_result", to_string(row.template_transition_result));
  field("template_transition_reason", row.template_transition_reason);
  field("template_probation_active", row.template_probation_active);
  field("revoked_template_slot_id", revocation ? std::optional<std::uint64_t>(revocation->slot_id) : std::nullopt);
  field("template_slot_revocation_reason", revocation ? to_string(revocation->reason) : "NONE");
  field("candidate_source", candidate ? candidate_source(candidate->source) : "NONE");
  field("candidate_id", candidate ? std::optional<std::uint64_t>(candidate->id) : std::nullopt);
  field("candidate_generation", candidate ? std::optional<std::uint64_t>(candidate->generation) : std::nullopt);
  field("recovery_confirmation_hits", row.recovery_confirmation_hits);
  field("recovery_conflict", row.recovery_conflict);
  field("recovery_probe_source", probe ? candidate_source(probe->source) : "NONE");
  field("recovery_probe_id", probe ? std::optional<std::uint64_t>(probe->id) : std::nullopt);
  field("recovery_probe_generation", probe ? std::optional<std::uint64_t>(probe->generation) : std::nullopt);
  visit_box("recovery_probe_", probe ? std::optional<BBox>(row.recovery_probe_bbox) : std::nullopt, field);
  field("recovery_reason", row.recovery_reason);
}

template <typename Field>
void visit_frame_fields(const TrackingLogRow& row, Field field) {
  field("schema_version", kTrackingSchemaVersion);
  field("frame_id", row.frame_id);
  field("timestamp_ms", row.timestamp_ms);
  field("state", to_string(row.mode));
  field("tracking_phase", to_string(row.tracking_phase));
  field("observation_valid", row.observation_valid);
  field("accepted", row.accepted);
  field("reacquired", row.reacquired);
  field("control_valid", row.control_valid);
  field("control_source", to_string(row.control_source));
  field("motion_control_authorized", row.motion_control_authorized);
  field("template_action", to_string(row.template_action));
  field("selected_template", to_string(row.selected_template));
  field("detector_same_class_ambiguous", row.detector_same_class_ambiguous);
  visit_box("tracking_", row.tracking_bbox, field);
  visit_box("display_", row.display_bbox, field);
  visit_box("control_", row.control_bbox, field);
  visit_observability_fields(row, field);
}

std::string field_value(const char* value) { return value; }
std::string field_value(bool value) { return value ? "1" : "0"; }
template <typename Value>
std::string field_value(Value value) { return std::to_string(value); }
std::string field_value(const std::optional<float>& value) { return optional_number(value); }
std::string field_value(const std::optional<std::uint64_t>& value) { return optional_integer(value); }

std::filesystem::path artifact_open_path(const std::filesystem::path& path) {
  if (path.empty()) return {};
  const auto absolute = std::filesystem::absolute(path);
  // Resolve directory aliases while preserving the final extension used by
  // the video encoder and the final directory entry checked by exclusive open.
  return std::filesystem::weakly_canonical(absolute.parent_path()) / absolute.filename();
}

std::filesystem::path artifact_identity(std::filesystem::path path) {
  if (path.empty()) return path;
  // weakly_canonical also handles existing parent directories of new outputs.
  // A dangling final symlink still aliases the path it would create on open.
  for (int depth = 0; depth < 40; ++depth) {
    std::error_code error;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(path, error))) {
      return std::filesystem::weakly_canonical(path);
    }
    const auto target = std::filesystem::read_symlink(path);
    path = target.is_absolute() ? target : path.parent_path() / target;
  }
  throw std::invalid_argument("tracking artifact has a cyclic symlink");
}

}  // namespace

MonotonicVideoClock::MonotonicVideoClock(double source_fps) {
  if (!std::isfinite(source_fps) || source_fps <= 0.0) {
    source_fps = 30.0;
    fps_fallback_ = true;
  }
  frame_step_ms_ = std::max<std::int64_t>(
      1, static_cast<std::int64_t>(std::llround(1000.0 / source_fps)));
}

std::int64_t MonotonicVideoClock::next(double pts_ms) {
  const bool valid_pts = std::isfinite(pts_ms) && pts_ms >= 0.0;
  const std::int64_t candidate =
      valid_pts ? static_cast<std::int64_t>(std::llround(pts_ms)) : 0;
  std::int64_t timestamp = candidate;
  if (last_timestamp_ms_) {
    if (!valid_pts || candidate <= *last_timestamp_ms_) {
      timestamp = *last_timestamp_ms_ + frame_step_ms_;
    }
  } else if (!valid_pts) {
    timestamp = 0;
  }
  last_timestamp_ms_ = timestamp;
  return timestamp;
}

std::string tracking_csv_header() {
  std::string header = "schema_version,frame_id,timestamp_ms,state,quality,control_valid,accepted,reacquired,"
         "x,y,width,height,selected_template,template_action,"
         "anchor_score,anchor_penalized_peak,anchor_psr,anchor_apce,anchor_peak_margin,"
         "recent_score,recent_penalized_peak,recent_psr,recent_apce,recent_peak_margin,"
         "template_iou,normalized_center_residual,area_log_change,aspect_log_change,"
         "search_scale,hypothesis_source,detector_configured,detector_ready,"
         "detector_requested,detector_request_frame_id,detector_worker_status,"
         "detector_error,detector_batch_frame_id,detector_batch_age_ms,"
         "detector_raw_candidate_count,detector_filtered_candidate_count,"
         "detector_verified_class_id,detector_verified_confidence,"
         "detector_target_mode,detector_candidate_id,detector_region_x,"
         "detector_region_y,detector_region_width,detector_region_height,"
         "detector_same_class_candidates,detector_same_class_ambiguous,"
         "detector_candidate_locked,detector_probe_index,"
         "detector_confirmation_attempts,detector_confirmation_hits,"
         "detector_preprocess_ms,detector_inference_ms,detector_postprocess_ms,"
         "detector_total_ms,detector_status,detector_candidates_queued,"
         "tracker_ms,runtime_ms,pipeline_ms,note,"
         "motion_predicted_x,motion_predicted_y,motion_predicted_width,"
         "motion_predicted_height,detector_candidate_center_ratio,"
         "detector_position_valid_count,detector_position_rejected_count,"
         "occlusion_hold_triggered,occlusion_hold_remaining_frames,"
         "occlusion_reference_width,occlusion_reference_height,"
         "occlusion_width_ratio,occlusion_height_ratio,tracker_skipped,"
         "detector_suppressed_by_hold,"
         "occlusion_reference_source,"
         "occlusion_suspicion_active,"
         "occlusion_suspicion_started,"
         "occlusion_suspicion_cleared,"
         "occlusion_suspicion_frames,"
         "occlusion_clean_frames,"
         "occlusion_width_shrink_frames,"
         "occlusion_height_shrink_frames,"
         "occlusion_severe_evidence,"
         "occlusion_gradual_evidence,"
         "occlusion_template_evidence,"
         "occlusion_peak_margin_evidence,"
         "occlusion_motion_residual_evidence,"
         "occlusion_trigger_reason,"
         "occlusion_coast_age_frames,"
         "occlusion_probe_type,"
         "occlusion_probe_confirmation_hits,"
         "occlusion_probe_confirmation_required,"
         "motion_control_authorized,"
         "occlusion_probe_rejection,occlusion_probe_motion_x,occlusion_probe_motion_y,"
         "occlusion_probe_motion_width,occlusion_probe_motion_height,"
         "occlusion_probe_x,occlusion_probe_y,occlusion_probe_width,occlusion_probe_height,"
         "occlusion_probe_anchor_reliable,"
         "occlusion_probe_recent_reliable,"
         "occlusion_probe_template_gate,"
         "occlusion_probe_cross_gate,"
         "occlusion_probe_cross_iou,"
         "occlusion_probe_cross_center,"
         "occlusion_probe_cross_area,"
         "occlusion_probe_cross_aspect,"
         "occlusion_probe_anchor_score_gate,"
         "occlusion_probe_anchor_center_gate,"
         "occlusion_probe_anchor_area_gate,"
         "occlusion_probe_anchor_aspect_gate,"
         "occlusion_probe_recent_score_gate,"
         "occlusion_probe_recent_center_gate,"
         "occlusion_probe_recent_area_gate,"
         "occlusion_probe_recent_aspect_gate,"
         "target_class_status,target_class_id,"
         "target_class_reason,detector_recovery_enabled,"
         "tracking_phase,observation_valid,control_source,accepted_observation_path,"
         "tracking_x,tracking_y,tracking_width,tracking_height,"
         "display_x,display_y,display_width,display_height,"
         "control_x,control_y,control_width,control_height,"
         "motion_search_age_ms,motion_prediction_confident,"
         "motion_velocity_x,motion_velocity_y,motion_acceleration_x,motion_acceleration_y,"
         "motion_scale_velocity_width,motion_scale_velocity_height,"
         "search_hint_present,search_hint_candidate_id,search_hint_source_frame_id,"
         "search_hint_source_timestamp_ms,search_hint_projected_x,search_hint_projected_y,"
         "search_hint_projected_width,search_hint_projected_height,search_hint_generation,"
         "search_hint_first_hit_frame,search_hint_confirmation_count,search_hint_used,"
         "search_hint_rejection";
  visit_observability_fields(TrackingLogRow{}, [&](const auto& name, const auto&) {
    header += ',';
    header += name;
  });
  return header;
}

std::string tracking_csv_row(const TrackingLogRow& row) {
  std::vector<std::string> fields{
      std::to_string(kTrackingSchemaVersion), std::to_string(row.frame_id),
      std::to_string(row.timestamp_ms),
      to_string(row.mode), to_string(row.quality),
      row.control_valid ? "1" : "0", row.accepted ? "1" : "0",
      row.reacquired ? "1" : "0", number(row.bbox.x), number(row.bbox.y),
      number(row.bbox.width), number(row.bbox.height),
      to_string(row.selected_template), to_string(row.template_action)};
  append_metrics(fields, row.anchor_present, row.anchor_metrics);
  append_metrics(fields, row.recent_present, row.recent_metrics);
  fields.insert(fields.end(),
                {number(row.template_iou), number(row.normalized_center_residual),
                 number(row.area_log_change), number(row.aspect_log_change),
                 number(row.search_scale), to_string(row.hypothesis_source),
                 row.detector_configured ? "1" : "0",
                 row.detector_ready ? "1" : "0",
                 row.detector_requested ? "1" : "0",
                 optional_integer(row.detector_request_frame_id),
                 csv_escape(row.detector_worker_status),
                 csv_escape(row.detector_error),
                 optional_integer(row.detector_batch_frame_id),
                 optional_integer(row.detector_batch_age_ms),
                 std::to_string(row.detector_raw_candidate_count),
                 std::to_string(row.detector_filtered_candidate_count),
                 optional_integer(row.detector_verified_class_id),
                 optional_number(row.detector_verified_confidence),
                 to_string(row.detector_target_mode),
                 optional_integer(row.detector_candidate_id),
                 row.detector_candidate_region
                     ? number(row.detector_candidate_region->x)
                     : "",
                 row.detector_candidate_region
                     ? number(row.detector_candidate_region->y)
                     : "",
                 row.detector_candidate_region
                     ? number(row.detector_candidate_region->width)
                     : "",
                 row.detector_candidate_region
                     ? number(row.detector_candidate_region->height)
                     : "",
                 std::to_string(row.detector_same_class_candidates),
                 row.detector_same_class_ambiguous ? "1" : "0",
                 row.detector_candidate_locked ? "1" : "0",
                 std::to_string(row.detector_probe_index),
                 std::to_string(row.detector_confirmation_attempts),
                 std::to_string(row.detector_confirmation_hits),
                 number(row.detector_timing.preprocess_ms),
                 number(row.detector_timing.inference_ms),
                 number(row.detector_timing.postprocess_ms),
                 number(row.detector_timing.total_ms),
                 to_string(row.detector_status),
                 std::to_string(row.detector_candidates_queued),
                 number(row.tracker_ms), number(row.runtime_ms),
                 number(row.pipeline_ms),
                 csv_escape(row.note)});
  fields.insert(fields.end(),
                {number(row.motion_predicted_bbox.x),
                 number(row.motion_predicted_bbox.y),
                 number(row.motion_predicted_bbox.width),
                 number(row.motion_predicted_bbox.height),
                 optional_number(row.detector_candidate_center_ratio),
                 std::to_string(row.detector_position_valid_count),
                 std::to_string(row.detector_position_rejected_count),
                 row.occlusion_hold_triggered ? "1" : "0",
                 std::to_string(row.occlusion_hold_remaining_frames),
                 optional_number(row.occlusion_reference_width),
                 optional_number(row.occlusion_reference_height),
                 optional_number(row.occlusion_width_ratio),
                 optional_number(row.occlusion_height_ratio),
                 row.tracker_skipped ? "1" : "0",
                 row.detector_suppressed_by_hold ? "1" : "0",
                 to_string(row.occlusion_reference_source),
                 row.occlusion_suspicion_active ? "1" : "0",
                 row.occlusion_suspicion_started ? "1" : "0",
                 row.occlusion_suspicion_cleared ? "1" : "0",
                 std::to_string(row.occlusion_suspicion_frames),
                 std::to_string(row.occlusion_clean_frames),
                 std::to_string(row.occlusion_width_shrink_frames),
                 std::to_string(row.occlusion_height_shrink_frames),
                 row.occlusion_severe_evidence ? "1" : "0",
                 row.occlusion_gradual_evidence ? "1" : "0",
                 row.occlusion_template_evidence ? "1" : "0",
                 row.occlusion_peak_margin_evidence ? "1" : "0",
                 row.occlusion_motion_residual_evidence ? "1" : "0",
                 to_string(row.occlusion_trigger_reason),
                 std::to_string(row.occlusion_coast_age_frames),
                 to_string(row.occlusion_probe_type),
                 std::to_string(row.occlusion_probe_confirmation_hits),
                 std::to_string(row.occlusion_probe_confirmation_required),
                 row.motion_control_authorized ? "1" : "0",
                 row.occlusion_probe_rejection,
                 row.occlusion_probe_motion_bbox ? number(row.occlusion_probe_motion_bbox->x) : "",
                 row.occlusion_probe_motion_bbox ? number(row.occlusion_probe_motion_bbox->y) : "",
                 row.occlusion_probe_motion_bbox ? number(row.occlusion_probe_motion_bbox->width) : "",
                 row.occlusion_probe_motion_bbox ? number(row.occlusion_probe_motion_bbox->height) : "",
                 row.occlusion_probe_bbox ? number(row.occlusion_probe_bbox->x) : "",
                 row.occlusion_probe_bbox ? number(row.occlusion_probe_bbox->y) : "",
                 row.occlusion_probe_bbox ? number(row.occlusion_probe_bbox->width) : "",
                 row.occlusion_probe_bbox ? number(row.occlusion_probe_bbox->height) : "",
                 optional_integer(row.occlusion_probe_anchor_reliable),
                 optional_integer(row.occlusion_probe_recent_reliable),
                 optional_integer(row.occlusion_probe_template_gate),
                 optional_integer(row.occlusion_probe_cross_gate),
                 optional_integer(row.occlusion_probe_cross_iou),
                 optional_integer(row.occlusion_probe_cross_center),
                 optional_integer(row.occlusion_probe_cross_area),
                 optional_integer(row.occlusion_probe_cross_aspect),
                 optional_integer(row.occlusion_probe_anchor_score_gate),
                 optional_integer(row.occlusion_probe_anchor_center_gate),
                 optional_integer(row.occlusion_probe_anchor_area_gate),
                 optional_integer(row.occlusion_probe_anchor_aspect_gate),
                 optional_integer(row.occlusion_probe_recent_score_gate),
                 optional_integer(row.occlusion_probe_recent_center_gate),
                 optional_integer(row.occlusion_probe_recent_area_gate),
                 optional_integer(row.occlusion_probe_recent_aspect_gate),
                 to_string(row.target_class.status),
                 optional_integer(row.target_class.target_class_id),
                 to_string(row.target_class.reason),
                 row.target_class.target_class_allows_recovery ? "1" : "0"});
  const SearchHint* hint = row.search_hint ? &*row.search_hint : nullptr;
  fields.insert(fields.end(),
                {to_string(row.tracking_phase),
                 row.observation_valid ? "1" : "0",
                 to_string(row.control_source),
                 acceptance_path_name(row.accepted_observation_path),
                 number(row.tracking_bbox.x), number(row.tracking_bbox.y),
                 number(row.tracking_bbox.width), number(row.tracking_bbox.height),
                 number(row.display_bbox.x), number(row.display_bbox.y),
                 number(row.display_bbox.width), number(row.display_bbox.height),
                 number(row.control_bbox.x), number(row.control_bbox.y),
                 number(row.control_bbox.width), number(row.control_bbox.height),
                 std::to_string(row.motion_search_age_ms),
                 row.motion_prediction_confident ? "1" : "0",
                 number(row.motion_velocity_x), number(row.motion_velocity_y),
                 number(row.motion_acceleration_x), number(row.motion_acceleration_y),
                 number(row.motion_scale_velocity_width),
                 number(row.motion_scale_velocity_height),
                 hint ? "1" : "0",
                 hint ? std::to_string(hint->candidate_id) : "",
                 hint ? std::to_string(hint->source_frame_id) : "",
                 hint ? std::to_string(hint->source_timestamp_ms) : "",
                 hint ? number(hint->projected_bbox.x) : "",
                 hint ? number(hint->projected_bbox.y) : "",
                 hint ? number(hint->projected_bbox.width) : "",
                 hint ? number(hint->projected_bbox.height) : "",
                 hint ? std::to_string(hint->generation) : "",
                 hint ? std::to_string(hint->first_hit_frame) : "",
                 hint ? std::to_string(hint->confirmation_count) : "",
                 hint ? (hint->used ? "1" : "0") : "",
                 hint ? to_string(hint->rejection) : ""});
  visit_observability_fields(row, [&](const auto&, const auto& value) {
    fields.push_back(field_value(value));
  });
  return join(fields);
}

std::string tracking_frame_json(const TrackingLogRow& row) {
  std::ostringstream stream;
  stream << '{';
  bool first = true;
  visit_frame_fields(row, [&](const auto& name, const auto& value) {
    if (!first) stream << ',';
    first = false;
    stream << '"' << name << "\":";
    using Value = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<Value, const char*>) {
      stream << '"' << json_escape(value) << '"';
    } else if constexpr (std::is_same_v<Value, bool>) {
      stream << (value ? "true" : "false");
    } else {
      const auto formatted = field_value(value);
      stream << (formatted.empty() ? "null" : formatted);
    }
  });
  stream << '}';
  return stream.str();
}

std::string tracking_frame_event_fields(const TrackingLogRow& row) {
  std::ostringstream stream;
  visit_frame_fields(row, [&](const auto& name, const auto& value) {
    const auto formatted = field_value(value);
    stream << ' ' << name << '=' << (formatted.empty() ? "NA" : formatted);
  });
  return stream.str();
}

TrackingArtifactPaths resolve_tracking_artifact_paths(
    const std::filesystem::path& boxes,
    const std::filesystem::path& metadata,
    const std::filesystem::path& video) {
  TrackingArtifactPaths paths;
  paths.boxes = artifact_open_path(boxes);
  paths.metadata = artifact_open_path(metadata.empty() && !boxes.empty()
      ? std::filesystem::path(boxes.string() + ".metadata.json") : metadata);
  paths.video = artifact_open_path(video);
  if (!boxes.empty()) {
    paths.frame_json = artifact_open_path(boxes.string() + ".jsonl");
  }
  std::filesystem::path identities[] = {
      artifact_identity(paths.boxes), artifact_identity(paths.metadata),
      artifact_identity(paths.video), {},
  };
  const char* labels[] = {"CSV", "session metadata", "video"};
  constexpr auto companion_index = std::size(labels);
  try {
    identities[companion_index] = artifact_identity(paths.frame_json);
  } catch (const std::filesystem::filesystem_error&) {
    paths.frame_json.clear();
    paths.frame_json_disabled_reason = "could not resolve derived companion path";
  } catch (const std::invalid_argument&) {
    paths.frame_json.clear();
    paths.frame_json_disabled_reason = "derived companion has a cyclic symlink";
  }
  const auto aliases = [&](std::size_t first, std::size_t second) {
    if (identities[first].empty() || identities[second].empty()) return false;
    std::error_code error;
    return identities[first] == identities[second] ||
           std::filesystem::equivalent(identities[first], identities[second], error);
  };
  for (std::size_t i = 0; i < std::size(labels); ++i) {
    for (std::size_t j = i + 1; j < std::size(labels); ++j) {
      if (aliases(i, j)) {
        throw std::invalid_argument(std::string("tracking outputs share a destination: ") +
                                    labels[i] + " and " + labels[j]);
      }
    }
    if (aliases(i, companion_index)) {
      paths.frame_json.clear();
      paths.frame_json_disabled_reason = std::string("derived companion aliases ") + labels[i];
    }
  }
  if (!paths.frame_json.empty()) {
    const auto& companion = identities[companion_index];
    const auto reference = companion.lexically_relative(identities[1].parent_path());
    paths.frame_json_reference = (reference.empty() ? companion : reference).generic_string();
  }
  return paths;
}

TrackingFrameJsonWriter::TrackingFrameJsonWriter(const TrackingArtifactPaths& paths)
    : directory_(paths.frame_json.parent_path()),
      filename_(paths.frame_json.filename()), reference_(paths.frame_json_reference) {}

TrackingFrameJsonWriter::~TrackingFrameJsonWriter() { discard(); }

void TrackingFrameJsonWriter::open() noexcept {
#if defined(__linux__) && defined(O_TMPFILE) && defined(AT_EMPTY_PATH) && defined(AT_SYMLINK_FOLLOW)
  if (filename_.empty() || publication_fd_ >= 0) return;
  directory_fd_ = ::open(directory_.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (directory_fd_ < 0) return;
  publication_fd_ = ::openat(directory_fd_, ".", O_TMPFILE | O_WRONLY | O_CLOEXEC, 0666);
  if (publication_fd_ >= 0) {
    const int stream_fd = ::fcntl(publication_fd_, F_DUPFD_CLOEXEC, 0);
    if (stream_fd >= 0) {
      stream_ = ::fdopen(stream_fd, "w");
      if (!stream_) ::close(stream_fd);
    }
  }
  wrote_frame_ = false;
  if (!stream_) discard();
#endif
}

void TrackingFrameJsonWriter::append(const TrackingLogRow& row) {
  if (!stream_) return;
  const auto json = tracking_frame_json(row);
  if (std::fwrite(json.data(), 1, json.size(), stream_) != json.size() ||
      std::fputc('\n', stream_) == EOF) {
    discard();
    return;
  }
  wrote_frame_ = true;
}

std::string TrackingFrameJsonWriter::finish(bool primary_complete) {
  if (!stream_) return {};
  const bool complete = primary_complete && wrote_frame_ &&
      std::fflush(stream_) == 0 && ::fsync(publication_fd_) == 0;
  const bool closed = std::fclose(stream_) == 0;
  stream_ = nullptr;
  const bool published = complete && closed && publish();
  discard();
  return published ? reference_ : std::string{};
}

bool TrackingFrameJsonWriter::directory_matches_open_path() const noexcept {
  struct stat held{}, current{};
  return ::fstat(directory_fd_, &held) == 0 &&
         ::stat(directory_.c_str(), &current) == 0 &&
         held.st_dev == current.st_dev && held.st_ino == current.st_ino;
}

bool TrackingFrameJsonWriter::publish() const noexcept {
#if defined(__linux__) && defined(O_TMPFILE) && defined(AT_EMPTY_PATH) && defined(AT_SYMLINK_FOLLOW)
  if (!directory_matches_open_path()) return false;
  int linked = ::linkat(publication_fd_, "", directory_fd_, filename_.c_str(), AT_EMPTY_PATH);
  if (linked != 0 && (errno == ENOENT || errno == EPERM)) {
    // Unprivileged Linux processes may not use AT_EMPTY_PATH. This form
    // follows the procfs link to the same held anonymous inode. Both forms
    // create the final name only if it is absent.
    char source[64];
    std::snprintf(source, sizeof(source), "/proc/self/fd/%d", publication_fd_);
    linked = ::linkat(AT_FDCWD, source, directory_fd_, filename_.c_str(), AT_SYMLINK_FOLLOW);
  }
  struct stat opened{}, published{};
  return linked == 0 && directory_matches_open_path() &&
         ::fstat(publication_fd_, &opened) == 0 &&
         ::fstatat(directory_fd_, filename_.c_str(), &published, AT_SYMLINK_NOFOLLOW) == 0 &&
         opened.st_dev == published.st_dev && opened.st_ino == published.st_ino;
#else
  return false;
#endif
}

void TrackingFrameJsonWriter::discard() noexcept {
  if (stream_) {
    std::fclose(stream_);
    stream_ = nullptr;
  }
  if (publication_fd_ >= 0) {
    ::close(publication_fd_);
    publication_fd_ = -1;
  }
  if (directory_fd_ >= 0) {
    ::close(directory_fd_);
    directory_fd_ = -1;
  }
}

TrackingLogRow make_initial_log_row(std::uint64_t frame_id,
                                    std::int64_t timestamp_ms,
                                    const BBox& bbox,
                                    bool detector_configured,
                                    bool detector_ready,
                                    DetectorTargetMode detector_target_mode,
                                    const TargetClassState& target_class) {
  TrackingLogRow row;
  row.frame_id = frame_id;
  row.timestamp_ms = timestamp_ms;
  row.mode = TrackingMode::kTracking;
  row.tracking_phase = TrackingPhase::kTracking;
  row.observation_valid = true;
  row.observation_present = true;
  row.trusted_for_state_update = true;
  row.motion_update_valid = true;
  row.state_change_reason = "INITIALIZED";
  row.rect0_source = "INITIAL_ROI";
  row.control_source = ControlSource::kObservation;
  row.quality = QualityLevel::kReliable;
  row.control_valid = true;
  row.accepted = true;
  row.bbox = bbox;
  row.tracking_bbox = bbox;
  row.display_bbox = bbox;
  row.control_bbox = bbox;
  row.motion_predicted_bbox = bbox;
  row.anchor_present = true;
  row.recent_present = true;
  row.anchor_metrics = ResponseMetrics{1.0F, 1.0F, 0.0F, 0.0F, 1.0F};
  row.recent_metrics = row.anchor_metrics;
  row.detector_configured = detector_configured;
  row.detector_ready = detector_ready;
  row.detector_target_mode = detector_target_mode;
  row.target_class = target_class;
  row.detector_worker_status = !detector_configured
                                   ? "disabled"
                                   : (detector_ready ? "ready" : "busy");
  row.note = "INITIALIZED";
  return row;
}

TrackingLogRow make_tracking_log_row(const RuntimeFrameResult& result,
                                     const std::string& note) {
  TrackingLogRow row;
  const ControllerOutput& output = result.output;
  row.frame_id = output.frame_id;
  row.timestamp_ms = output.timestamp_ms;
  row.mode = output.mode;
  row.tracking_phase = output.tracking_phase;
  row.observation_valid = output.observation_valid;
  row.observation_present = output.observation_present;
  row.trusted_for_state_update = output.trusted_for_state_update;
  row.motion_update_valid = output.motion_update_valid;
  row.state_change_reason = output.state_change_reason;
  row.rect0_source = output.accepted ? "ACCEPTED_OBSERVATION" :
      output.observation_present ? "RECENT_OBSERVATION" : "MOTION_PREDICTION";
  row.control_source = output.control_source;
  row.accepted_observation_path = output.accepted_observation_path;
  row.quality = output.quality;
  row.control_valid = output.control_valid;
  row.accepted = output.accepted;
  row.reacquired = output.reacquired;
  row.bbox = output.bbox;
  row.tracking_bbox = output.tracking_bbox;
  row.display_bbox = output.display_bbox;
  row.control_bbox = output.control_bbox;
  row.selected_template = output.selected_template;
  row.template_action = output.template_action;
  row.template_transition = output.template_transition;
  row.template_slot_revocation = result.template_slot_revocation;
  row.template_transition_result = output.template_transition_result;
  row.template_transition_applied = output.template_transition_applied;
  row.template_probation_active = output.template_probation_active;
  row.template_transition_reason = output.template_transition_reason;
  row.template_heads = result.template_heads;
  row.trusted_response_reference = output.trusted_response_reference;
  row.response_psr_relative_delta = output.response_psr_relative_delta;
  row.response_apce_relative_delta = output.response_apce_relative_delta;
  row.response_peak_margin_absolute_delta = output.response_peak_margin_absolute_delta;
  row.recovery_candidate_key = output.recovery_candidate_key;
  row.recovery_confirmation_hits = output.recovery_confirmation_hits;
  row.recovery_conflict = output.recovery_conflict;
  row.recovery_probe = result.plan.recovery_probe;
  row.recovery_probe_bbox = result.plan.predicted_bbox;
  row.recovery_reason = output.recovery_reason;
  for (const TrackerObservation& observation : result.observations) {
    const auto bbox = observation.decoded_bbox.value_or(observation.bbox);
    switch (observation.template_kind) {
      case TemplateKind::kAnchor:
        row.anchor_present = true;
        row.anchor_metrics = observation.metrics;
        break;
      case TemplateKind::kRecent:
        row.recent_present = true;
        row.recent_metrics = observation.metrics;
        row.recent_raw_bbox = bbox;
        break;
      case TemplateKind::kSafe:
        row.safe_metrics = observation.metrics;
        row.safe_raw_bbox = bbox;
        break;
      case TemplateKind::kStaged:
        row.staged_metrics = observation.metrics;
        row.staged_raw_bbox = bbox;
        break;
    }
  }
  row.template_iou = output.template_iou;
  row.normalized_center_residual = output.normalized_center_residual;
  row.area_log_change = output.area_log_change;
  row.aspect_log_change = output.aspect_log_change;
  row.search_scale = output.search_scale;
  row.hypothesis_source = output.hypothesis_source;
  row.motion_predicted_bbox = result.plan.motion_predicted_bbox;
  row.detector_configured = result.detector.configured;
  row.detector_ready = result.detector.ready;
  row.detector_requested = result.plan.request_detector;
  if (result.plan.detector_request) {
    row.detector_request_frame_id = result.plan.detector_request->frame_id;
  } else if (result.detector.request_submitted) {
    row.detector_request_frame_id = result.detector.request_frame_id;
  }
  row.detector_error = result.detector.error;
  if (!row.detector_error.empty()) {
    row.detector_worker_status = "error";
  } else if (!row.detector_configured) {
    row.detector_worker_status = "disabled";
  } else if (result.detector.batch_received) {
    row.detector_worker_status = "completed";
  } else if (result.detector.request_submitted) {
    row.detector_worker_status = "submitted";
  } else if (row.detector_ready) {
    row.detector_worker_status = "ready";
  } else {
    row.detector_worker_status = "busy";
  }
  if (result.detector.batch_received) {
    row.detector_batch_frame_id = result.detector.batch_frame_id;
    row.detector_batch_age_ms = result.detector.batch_age_ms;
  }
  row.detector_raw_candidate_count = result.detector.raw_candidate_count;
  row.detector_filtered_candidate_count =
      result.detector.filtered_candidate_count;
  row.detector_verified_class_id = result.plan.detector_candidate_class_id;
  row.detector_verified_confidence =
      result.plan.detector_candidate_confidence;
  row.detector_candidate_center_ratio =
      result.plan.detector_candidate_center_ratio;
  row.detector_target_mode = result.plan.detector_target_mode;
  row.detector_candidate_id = result.output.detector_candidate_id;
  row.detector_candidate_region = result.plan.detector_candidate_region;
  row.detector_same_class_candidates =
      result.output.detector_same_class_candidates;
  row.detector_same_class_ambiguous =
      result.output.detector_same_class_ambiguous;
  row.detector_candidate_locked = result.output.detector_candidate_locked;
  row.detector_probe_index = result.output.detector_probe_index;
  row.detector_confirmation_attempts =
      result.output.detector_confirmation_attempts;
  row.detector_confirmation_hits = result.output.detector_confirmation_hits;
  row.detector_timing = result.detector.timing;
  row.detector_status = output.detector_status;
  row.detector_candidates_queued = output.detector_candidates_queued;
  row.detector_position_valid_count =
      output.detector_position_valid_count;
  row.detector_position_rejected_count =
      output.detector_position_rejected_count;
  row.occlusion_hold_triggered = output.occlusion_hold_triggered;
  row.occlusion_reference_source = output.occlusion_reference_source;
  row.occlusion_suspicion_active = output.occlusion_suspicion_active;
  row.occlusion_suspicion_started = output.occlusion_suspicion_started;
  row.occlusion_suspicion_cleared = output.occlusion_suspicion_cleared;
  row.occlusion_suspicion_frames = output.occlusion_suspicion_frames;
  row.occlusion_clean_frames = output.occlusion_clean_frames;
  row.occlusion_width_shrink_frames = output.occlusion_width_shrink_frames;
  row.occlusion_height_shrink_frames = output.occlusion_height_shrink_frames;
  row.occlusion_severe_evidence = output.occlusion_severe_evidence;
  row.occlusion_gradual_evidence = output.occlusion_gradual_evidence;
  row.occlusion_template_evidence = output.occlusion_template_evidence;
  row.occlusion_peak_margin_evidence = output.occlusion_peak_margin_evidence;
  row.occlusion_motion_residual_evidence = output.occlusion_motion_residual_evidence;
  row.occlusion_trigger_reason = output.occlusion_trigger_reason;
  row.occlusion_coast_age_frames = output.occlusion_coast_age_frames;
  row.occlusion_probe_type = output.occlusion_probe_type;
  row.occlusion_probe_confirmation_hits = output.occlusion_probe_confirmation_hits;
  row.occlusion_probe_confirmation_required = output.occlusion_probe_confirmation_required;
  row.motion_control_authorized = output.motion_control_authorized;
  row.motion_search_age_ms = output.motion_search_age_ms;
  row.motion_prediction_confident = output.motion_prediction_confident;
  row.motion_velocity_x = output.motion_velocity_x;
  row.motion_velocity_y = output.motion_velocity_y;
  row.motion_acceleration_x = output.motion_acceleration_x;
  row.motion_acceleration_y = output.motion_acceleration_y;
  row.motion_scale_velocity_width = output.motion_scale_velocity_width;
  row.motion_scale_velocity_height = output.motion_scale_velocity_height;
  row.search_hint = output.search_hint;
  row.occlusion_probe_rejection = output.occlusion_probe_rejection;
  row.occlusion_probe_motion_bbox = output.occlusion_probe_motion_bbox;
  row.occlusion_probe_bbox = output.occlusion_probe_bbox;
  row.occlusion_probe_anchor_reliable = output.occlusion_probe_anchor_reliable;
  row.occlusion_probe_recent_reliable = output.occlusion_probe_recent_reliable;
  row.occlusion_probe_template_gate = output.occlusion_probe_template_gate;
  row.occlusion_probe_cross_gate = output.occlusion_probe_cross_gate;
  row.occlusion_probe_cross_iou = output.occlusion_probe_cross_iou;
  row.occlusion_probe_cross_center = output.occlusion_probe_cross_center;
  row.occlusion_probe_cross_area = output.occlusion_probe_cross_area;
  row.occlusion_probe_cross_aspect = output.occlusion_probe_cross_aspect;
  row.occlusion_probe_anchor_score_gate = output.occlusion_probe_anchor_score_gate;
  row.occlusion_probe_anchor_center_gate = output.occlusion_probe_anchor_center_gate;
  row.occlusion_probe_anchor_area_gate = output.occlusion_probe_anchor_area_gate;
  row.occlusion_probe_anchor_aspect_gate = output.occlusion_probe_anchor_aspect_gate;
  row.occlusion_probe_recent_score_gate = output.occlusion_probe_recent_score_gate;
  row.occlusion_probe_recent_center_gate = output.occlusion_probe_recent_center_gate;
  row.occlusion_probe_recent_area_gate = output.occlusion_probe_recent_area_gate;
  row.occlusion_probe_recent_aspect_gate = output.occlusion_probe_recent_aspect_gate;
  row.occlusion_hold_remaining_frames =
      output.occlusion_hold_remaining_frames;
  row.occlusion_reference_width = output.occlusion_reference_width;
  row.occlusion_reference_height = output.occlusion_reference_height;
  row.occlusion_width_ratio = output.occlusion_width_ratio;
  row.occlusion_height_ratio = output.occlusion_height_ratio;
  row.tracker_skipped = result.tracker_skipped;
  row.detector_suppressed_by_hold =
      result.tracker_skipped &&
      result.plan.mode == TrackingMode::kOcclusionHold;
  row.target_class = result.target_class;
  row.tracker_ms = result.tracker_ms;
  row.runtime_ms = result.runtime_ms;
  row.note = note;
  return row;
}

LatencySummary summarize_latencies(const std::vector<float>& runtime_ms,
                                   const std::vector<float>& pipeline_ms) {
  if (runtime_ms.empty() || runtime_ms.size() != pipeline_ms.size()) {
    throw std::invalid_argument(
        "runtime and pipeline latency samples must have the same non-zero size");
  }
  const auto validate = [](const std::vector<float>& values) {
    for (const float value : values) {
      if (!std::isfinite(value) || value <= 0.0F) {
        throw std::invalid_argument("latency samples must be finite and positive");
      }
    }
  };
  validate(runtime_ms);
  validate(pipeline_ms);

  LatencySummary summary;
  summary.mean_runtime_ms =
      std::accumulate(runtime_ms.begin(), runtime_ms.end(), 0.0F) /
      static_cast<float>(runtime_ms.size());
  summary.p50_runtime_ms = percentile_ms(runtime_ms, 0.50F);
  summary.p95_runtime_ms = percentile_ms(runtime_ms, 0.95F);
  summary.runtime_fps = 1000.0F / summary.mean_runtime_ms;
  summary.mean_pipeline_ms =
      std::accumulate(pipeline_ms.begin(), pipeline_ms.end(), 0.0F) /
      static_cast<float>(pipeline_ms.size());
  summary.p50_pipeline_ms = percentile_ms(pipeline_ms, 0.50F);
  summary.p95_pipeline_ms = percentile_ms(pipeline_ms, 0.95F);
  summary.end_to_end_fps = 1000.0F / summary.mean_pipeline_ms;
  return summary;
}

void record_target_class_initialization(
    TrackingSessionMetadata& metadata,
    const TargetClassInitializationResult& initialization) {
  metadata.target_class_initializations.push_back(initialization);
  InitializationDetectorSummary& summary = metadata.initialization_detector;
  const bool requested = initialization.request_id.has_value() ||
                         !initialization.detector_error.empty();
  if (requested) {
    ++summary.request_count;
    summary.total_detector_ms += initialization.detector_ms;
  }
  if (initialization.batch) ++summary.complete_count;
  if (!initialization.detector_error.empty()) ++summary.error_count;
}

void apply_controller_config_metadata(TrackingSessionMetadata& metadata,
                                      const ControllerConfig& config) {
  metadata.calibrated = config.calibrated;
  metadata.template_policy = config.template_policy;
  metadata.recovery_policy = config.recovery_policy;
  metadata.occlusion_probe_interval_frames = config.occlusion_probe_interval_frames;
  metadata.occlusion_gradual_dimension_ratio = config.occlusion_gradual_dimension_ratio;
  metadata.occlusion_gradual_consecutive_frames = config.occlusion_gradual_consecutive_frames;
  metadata.occlusion_min_reference_samples = config.occlusion_min_reference_samples;
  metadata.occlusion_shrink_step_ratio = config.occlusion_shrink_step_ratio;
  metadata.occlusion_suspicion_clear_frames = config.occlusion_suspicion_clear_frames;
  metadata.occlusion_peak_margin_drop = config.occlusion_peak_margin_drop;
  metadata.occlusion_motion_residual_threshold = config.occlusion_motion_residual_threshold;
  metadata.occlusion_confirmation_iou = config.occlusion_confirmation_iou;
  metadata.occlusion_confirmation_center_distance = config.occlusion_confirmation_center_distance;
  metadata.occlusion_confirmation_max_area_log_change = config.occlusion_confirmation_max_area_log_change;
  metadata.occlusion_confirmation_max_aspect_log_change = config.occlusion_confirmation_max_aspect_log_change;
  metadata.max_accepted_scale_step_ratio = config.max_accepted_scale_step_ratio;
  metadata.accepted_scale_deadband_ratio = config.accepted_scale_deadband_ratio;
  metadata.accepted_scale_smoothing_alpha = config.accepted_scale_smoothing_alpha;
  metadata.accepted_scale_median_window_frames =
      config.accepted_scale_median_window_frames;
  metadata.accepted_scale_direction_confirm_frames =
      config.accepted_scale_direction_confirm_frames;
  metadata.accepted_scale_output_hysteresis_px =
      config.accepted_scale_output_hysteresis_px;
  metadata.accepted_center_smoothing_alpha =
      config.accepted_center_smoothing_alpha;
  metadata.accepted_center_max_correction_ratio =
      config.accepted_center_max_correction_ratio;
  metadata.max_reacquisition_center_distance =
      config.max_reacquisition_center_distance;
  metadata.motion_search_enabled = config.motion_search_enabled;
  metadata.search_reacquisition_requires_detector =
      config.search_reacquisition_requires_detector;
  metadata.search_reacquisition_min_age_ms =
      config.search_reacquisition_min_age_ms;
  metadata.template_bridge_min_stable_frames =
      config.template_bridge_min_stable_frames;
  metadata.template_bridge_max_duration_ms =
      config.template_bridge_max_duration_ms;
  metadata.motion_search_confidence_ms = config.motion_search_confidence_ms;
  metadata.motion_max_speed_diagonals_per_s =
      config.motion_max_speed_diagonals_per_s;
  metadata.motion_max_acceleration_diagonals_per_s2 =
      config.motion_max_acceleration_diagonals_per_s2;
  metadata.motion_scale_rate_limit_per_s =
      config.motion_scale_rate_limit_per_s;
  metadata.search_probe_interval_ms = config.search_probe_interval_ms;
  metadata.controller_config_identity = controller_config_identity(config);
}

std::string tracking_session_metadata_json(
    const TrackingSessionMetadata& metadata) {
  const double numeric_values[] = {
      metadata.source_fps,
      metadata.reliable_score,
      metadata.reliable_psr,
      metadata.reliable_apce,
      metadata.reliable_peak_margin,
      metadata.max_accepted_scale_step_ratio,
      metadata.accepted_scale_deadband_ratio,
      metadata.accepted_scale_smoothing_alpha,
      metadata.accepted_scale_output_hysteresis_px,
      metadata.accepted_center_smoothing_alpha,
      metadata.accepted_center_max_correction_ratio,
      metadata.max_reacquisition_center_distance,
      metadata.motion_max_speed_diagonals_per_s,
      metadata.motion_max_acceleration_diagonals_per_s2,
      metadata.motion_scale_rate_limit_per_s,
      metadata.detector_candidate_max_center_ratio,
      metadata.occlusion_trigger_min_dimension_ratio,
      metadata.occlusion_gradual_dimension_ratio,
      metadata.occlusion_shrink_step_ratio,
      metadata.occlusion_peak_margin_drop,
      metadata.occlusion_motion_residual_threshold,
      metadata.occlusion_confirmation_iou,
      metadata.occlusion_confirmation_center_distance,
      metadata.occlusion_confirmation_max_area_log_change,
      metadata.occlusion_confirmation_max_aspect_log_change,
      metadata.latency.mean_runtime_ms,
      metadata.latency.p50_runtime_ms,
      metadata.latency.p95_runtime_ms,
      metadata.latency.runtime_fps,
      metadata.latency.mean_pipeline_ms,
      metadata.latency.p50_pipeline_ms,
      metadata.latency.p95_pipeline_ms,
      metadata.latency.end_to_end_fps,
      metadata.detector.confidence_threshold,
      metadata.detector.nms_threshold,
      metadata.target_class_match_config.min_roi_coverage,
      metadata.target_class_match_config.max_center_ratio,
      metadata.target_class_match_config.ambiguity_margin,
      metadata.initialization_detector.total_detector_ms,
  };
  for (const double value : numeric_values) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("session metadata numeric values must be finite");
    }
  }

  const DetectorSessionMetadata& detector = metadata.detector;
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6)
         << "{\n"
         << "  \"schema_version\": " << kTrackingSchemaVersion << ",\n"
         << "  \"observation_valid_semantics\": \"ACCEPTED_OBSERVATION\",\n"
         << "  \"rect0_bbox\": \"tracking_bbox\",\n"
         << "  \"frame_json_file\": ";
  if (metadata.frame_json_file.empty()) stream << "null";
  else stream << '"' << json_escape(metadata.frame_json_file) << '"';
  stream << ",\n"
         << "  \"video\": \"" << json_escape(metadata.video) << "\",\n"
         << "  \"model_dir\": \"" << json_escape(metadata.model_dir) << "\",\n"
         << "  \"manifest_hash_algorithm\": \""
         << json_escape(metadata.manifest_hash_algorithm) << "\",\n"
         << "  \"manifest_hash\": \"" << json_escape(metadata.manifest_hash)
         << "\",\n"
         << "  \"controller_config\": \""
         << json_escape(metadata.controller_config) << "\",\n"
         << "  \"controller_config_hash_algorithm\": \""
         << json_escape(metadata.controller_config_hash_algorithm) << "\",\n"
         << "  \"controller_config_hash\": \""
         << json_escape(metadata.controller_config_hash) << "\",\n"
         << "  \"sensor\": \"" << json_escape(metadata.sensor) << "\",\n"
         << "  \"calibrated\": " << (metadata.calibrated ? "true" : "false")
         << ",\n"
         << "  \"template_policy\": \"" << to_string(metadata.template_policy)
         << "\",\n"
         << "  \"recovery_policy\": \"" << to_string(metadata.recovery_policy)
         << "\",\n"
         << "  \"requested_target_class_mode\": \""
         << (metadata.requested_target_class_id < 0 ? "AUTO" : "FIXED")
         << "\",\n"
         << "  \"requested_target_class_id\": "
         << metadata.requested_target_class_id << ",\n"
         << "  \"engine_class_filter\": " << metadata.engine_class_filter
         << ",\n"
         << "  \"target_class_match_config\": {\"calibrated\": false, \"min_roi_coverage\": "
         << metadata.target_class_match_config.min_roi_coverage
         << ", \"max_center_ratio\": "
         << metadata.target_class_match_config.max_center_ratio
         << ", \"ambiguity_margin\": "
         << metadata.target_class_match_config.ambiguity_margin << "},\n"
         << "  \"initialization_detector\": {\"request_count\": "
         << metadata.initialization_detector.request_count
         << ", \"complete_count\": "
         << metadata.initialization_detector.complete_count
         << ", \"error_count\": "
         << metadata.initialization_detector.error_count
         << ", \"total_detector_ms\": "
         << metadata.initialization_detector.total_detector_ms
         << ", \"mean_detector_ms\": "
         << (metadata.initialization_detector.request_count == 0 ? 0.0F :
             metadata.initialization_detector.total_detector_ms /
             static_cast<float>(metadata.initialization_detector.request_count))
         << "},\n"
         << "  \"target_class_initializations\": [\n";
  for (std::size_t index = 0;
       index < metadata.target_class_initializations.size(); ++index) {
    if (index) stream << ",\n";
    append_json_initialization(stream, metadata.target_class_initializations[index]);
  }
  stream << "\n  ],\n"
         << "  \"detector_target_mode\": \""
         << to_string(metadata.detector_target_mode) << "\",\n"
         << "  \"detector_candidate_max_center_ratio\": "
         << metadata.detector_candidate_max_center_ratio << ",\n"
         << "  \"occlusion_hold_frames\": "
         << metadata.occlusion_hold_frames << ",\n"
         << "  \"occlusion_trigger_consecutive_unreliable\": "
         << metadata.occlusion_trigger_consecutive_unreliable << ",\n"
         << "  \"occlusion_trigger_min_dimension_ratio\": "
         << metadata.occlusion_trigger_min_dimension_ratio << ",\n"
         << "  \"occlusion_reference_window\": "
         << metadata.occlusion_reference_window << ",\n"
         << "  \"occlusion_probe_interval_frames\": " << metadata.occlusion_probe_interval_frames << ",\n"
         << "  \"occlusion_gradual_dimension_ratio\": " << metadata.occlusion_gradual_dimension_ratio << ",\n"
         << "  \"occlusion_gradual_consecutive_frames\": " << metadata.occlusion_gradual_consecutive_frames << ",\n"
         << "  \"occlusion_min_reference_samples\": " << metadata.occlusion_min_reference_samples << ",\n"
         << "  \"occlusion_shrink_step_ratio\": " << metadata.occlusion_shrink_step_ratio << ",\n"
         << "  \"occlusion_suspicion_clear_frames\": " << metadata.occlusion_suspicion_clear_frames << ",\n"
         << "  \"occlusion_peak_margin_drop\": " << metadata.occlusion_peak_margin_drop << ",\n"
         << "  \"occlusion_motion_residual_threshold\": " << metadata.occlusion_motion_residual_threshold << ",\n"
         << "  \"occlusion_confirmation_iou\": " << metadata.occlusion_confirmation_iou << ",\n"
         << "  \"occlusion_confirmation_center_distance\": " << metadata.occlusion_confirmation_center_distance << ",\n"
         << "  \"occlusion_confirmation_max_area_log_change\": " << metadata.occlusion_confirmation_max_area_log_change << ",\n"
         << "  \"occlusion_confirmation_max_aspect_log_change\": " << metadata.occlusion_confirmation_max_aspect_log_change << ",\n"
         << "  \"rknn_api_version\": \""
         << json_escape(metadata.rknn_api_version) << "\",\n"
         << "  \"rknn_driver_version\": \""
         << json_escape(metadata.rknn_driver_version) << "\",\n"
         << "  \"source_fps\": " << metadata.source_fps << ",\n"
         << "  \"fps_fallback\": "
         << (metadata.fps_fallback ? "true" : "false") << ",\n"
         << "  \"frame_step_ms\": " << metadata.frame_step_ms << ",\n"
         << "  \"reliable_score\": " << metadata.reliable_score << ",\n"
         << "  \"reliable_psr\": " << metadata.reliable_psr << ",\n"
         << "  \"reliable_apce\": " << metadata.reliable_apce << ",\n"
         << "  \"reliable_peak_margin\": " << metadata.reliable_peak_margin
         << ",\n"
         << "  \"max_accepted_scale_step_ratio\": "
         << metadata.max_accepted_scale_step_ratio << ",\n"
         << "  \"accepted_scale_deadband_ratio\": "
         << metadata.accepted_scale_deadband_ratio << ",\n"
         << "  \"accepted_scale_smoothing_alpha\": "
         << metadata.accepted_scale_smoothing_alpha << ",\n"
         << "  \"accepted_scale_median_window_frames\": "
         << metadata.accepted_scale_median_window_frames << ",\n"
         << "  \"accepted_scale_direction_confirm_frames\": "
         << metadata.accepted_scale_direction_confirm_frames << ",\n"
         << "  \"accepted_scale_output_hysteresis_px\": "
         << metadata.accepted_scale_output_hysteresis_px << ",\n"
         << "  \"accepted_center_smoothing_alpha\": "
         << metadata.accepted_center_smoothing_alpha << ",\n"
         << "  \"accepted_center_max_correction_ratio\": "
         << metadata.accepted_center_max_correction_ratio << ",\n"
         << "  \"max_reacquisition_center_distance\": "
         << metadata.max_reacquisition_center_distance << ",\n"
         << "  \"uncertain_ms\": " << metadata.uncertain_ms << ",\n"
         << "  \"local_recovery_ms\": " << metadata.local_recovery_ms << ",\n"
         << "  \"assisted_recovery_ms\": " << metadata.assisted_recovery_ms
         << ",\n"
         << "  \"motion_search_enabled\": "
         << (metadata.motion_search_enabled ? "true" : "false") << ",\n"
         << "  \"search_reacquisition_requires_detector\": "
         << (metadata.search_reacquisition_requires_detector ? "true" : "false")
         << ",\n"
         << "  \"search_reacquisition_min_age_ms\": "
         << metadata.search_reacquisition_min_age_ms << ",\n"
         << "  \"template_bridge_min_stable_frames\": "
         << metadata.template_bridge_min_stable_frames << ",\n"
         << "  \"template_bridge_max_duration_ms\": "
         << metadata.template_bridge_max_duration_ms << ",\n"
         << "  \"motion_search_confidence_ms\": "
         << metadata.motion_search_confidence_ms << ",\n"
         << "  \"motion_max_speed_diagonals_per_s\": "
         << metadata.motion_max_speed_diagonals_per_s << ",\n"
         << "  \"motion_max_acceleration_diagonals_per_s2\": "
         << metadata.motion_max_acceleration_diagonals_per_s2 << ",\n"
         << "  \"motion_scale_rate_limit_per_s\": "
         << metadata.motion_scale_rate_limit_per_s << ",\n"
         << "  \"search_probe_interval_ms\": "
         << metadata.search_probe_interval_ms << ",\n"
         << "  \"controller_config_identity\": \""
         << json_escape(metadata.controller_config_identity) << "\",\n"
         << "  \"tracked_frames\": " << metadata.tracked_frames << ",\n"
         << "  \"mean_runtime_ms\": " << metadata.latency.mean_runtime_ms << ",\n"
         << "  \"p50_runtime_ms\": " << metadata.latency.p50_runtime_ms << ",\n"
         << "  \"p95_runtime_ms\": " << metadata.latency.p95_runtime_ms << ",\n"
         << "  \"runtime_fps\": " << metadata.latency.runtime_fps << ",\n"
         << "  \"mean_pipeline_ms\": " << metadata.latency.mean_pipeline_ms
         << ",\n"
         << "  \"p50_pipeline_ms\": " << metadata.latency.p50_pipeline_ms << ",\n"
         << "  \"p95_pipeline_ms\": " << metadata.latency.p95_pipeline_ms << ",\n"
         << "  \"end_to_end_fps\": " << metadata.latency.end_to_end_fps << ",\n"
         << "  \"detector\": {\n"
         << "    \"enabled\": " << (detector.enabled ? "true" : "false") << ",\n"
         << "    \"calibrated\": "
         << (detector.calibrated ? "true" : "false") << ",\n"
         << "    \"model_path\": \"" << json_escape(detector.model_path) << "\",\n"
         << "    \"model_sha256\": \"" << json_escape(detector.model_sha256)
         << "\",\n"
         << "    \"manifest_path\": \"" << json_escape(detector.manifest_path)
         << "\",\n"
         << "    \"manifest_sha256\": \""
         << json_escape(detector.manifest_sha256) << "\",\n"
         << "    \"labels_path\": \"" << json_escape(detector.labels_path)
         << "\",\n"
         << "    \"labels_sha256\": \"" << json_escape(detector.labels_sha256)
         << "\",\n"
         << "    \"class_id\": " << detector.class_id << ",\n"
         << "    \"confidence_threshold\": " << detector.confidence_threshold
         << ",\n"
         << "    \"nms_threshold\": " << detector.nms_threshold << ",\n"
         << "    \"input_tensor_description\": \""
         << json_escape(detector.input_tensor_description) << "\",\n"
         << "    \"output_tensor_descriptions\": [";
  for (std::size_t index = 0;
       index < detector.output_tensor_descriptions.size(); ++index) {
    if (index != 0) stream << ", ";
    stream << '"' << json_escape(detector.output_tensor_descriptions[index]) << '"';
  }
  stream << "],\n"
         << "    \"rknn_api_version\": \""
         << json_escape(detector.rknn_api_version) << "\",\n"
         << "    \"rknn_driver_version\": \""
         << json_escape(detector.rknn_driver_version) << "\",\n"
         << "    \"request_count\": " << detector.request_count << ",\n"
         << "    \"complete_count\": " << detector.complete_count << ",\n"
         << "    \"reject_count\": " << detector.reject_count << ",\n"
         << "    \"error_count\": " << detector.error_count << "\n"
         << "  }\n"
         << "}\n";
  return stream.str();
}

}  // namespace nanotrack
