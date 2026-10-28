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
         "detector_suppressed_by_hold";
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
                 row.detector_suppressed_by_hold ? "1" : "0"});
  return join(fields);
}

TrackingLogRow make_initial_log_row(std::uint64_t frame_id,
                                    std::int64_t timestamp_ms,
                                    const BBox& bbox,
                                    bool detector_configured,
                                    bool detector_ready,
                                    DetectorTargetMode detector_target_mode) {
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

std::string tracking_session_metadata_json(
    const TrackingSessionMetadata& metadata) {
  const double numeric_values[] = {
      metadata.source_fps,
      metadata.reliable_score,
      metadata.reliable_psr,
      metadata.reliable_apce,
      metadata.reliable_peak_margin,
      metadata.detector_candidate_max_center_ratio,
      metadata.occlusion_trigger_min_dimension_ratio,
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
