#include "tracking_log.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

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
  return "schema_version,frame_id,timestamp_ms,state,quality,control_valid,accepted,reacquired,"
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
         "target_class_reason,detector_recovery_enabled";
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
  return join(fields);
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
  row.quality = QualityLevel::kReliable;
  row.control_valid = true;
  row.accepted = true;
  row.bbox = bbox;
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
  row.quality = output.quality;
  row.control_valid = output.control_valid;
  row.accepted = output.accepted;
  row.reacquired = output.reacquired;
  row.bbox = output.bbox;
  row.selected_template = output.selected_template;
  row.template_action = output.template_action;
  for (const TrackerObservation& observation : result.observations) {
    if (observation.template_kind == TemplateKind::kAnchor) {
      row.anchor_present = true;
      row.anchor_metrics = observation.metrics;
    } else {
      row.recent_present = true;
      row.recent_metrics = observation.metrics;
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
