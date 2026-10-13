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
  return "frame_id,timestamp_ms,state,quality,control_valid,accepted,reacquired,"
         "x,y,width,height,selected_template,template_action,"
         "anchor_score,anchor_penalized_peak,anchor_psr,anchor_apce,anchor_peak_margin,"
         "recent_score,recent_penalized_peak,recent_psr,recent_apce,recent_peak_margin,"
         "template_iou,normalized_center_residual,area_log_change,aspect_log_change,"
         "search_scale,hypothesis_source,detector_requested,detector_status,"
         "detector_candidates_queued,tracker_ms,detector_ms,runtime_ms,pipeline_ms,note";
}

std::string tracking_csv_row(const TrackingLogRow& row) {
  std::vector<std::string> fields{
      std::to_string(row.frame_id), std::to_string(row.timestamp_ms),
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
                 row.detector_requested ? "1" : "0",
                 to_string(row.detector_status),
                 std::to_string(row.detector_candidates_queued),
                 number(row.tracker_ms), number(row.detector_ms),
                 number(row.runtime_ms), number(row.pipeline_ms),
                 csv_escape(row.note)});
  return join(fields);
}

TrackingLogRow make_initial_log_row(std::uint64_t frame_id,
                                    std::int64_t timestamp_ms,
                                    const BBox& bbox) {
  TrackingLogRow row;
  row.frame_id = frame_id;
  row.timestamp_ms = timestamp_ms;
  row.mode = TrackingMode::kTracking;
  row.quality = QualityLevel::kReliable;
  row.control_valid = true;
  row.accepted = true;
  row.bbox = bbox;
  row.anchor_present = true;
  row.recent_present = true;
  row.anchor_metrics = ResponseMetrics{1.0F, 1.0F, 0.0F, 0.0F, 1.0F};
  row.recent_metrics = row.anchor_metrics;
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
  row.detector_requested = result.plan.request_detector;
  row.detector_status = output.detector_status;
  row.detector_candidates_queued = output.detector_candidates_queued;
  row.tracker_ms = result.tracker_ms;
  row.detector_ms = result.detector.timing.total_ms;
  row.runtime_ms = result.runtime_ms;
  row.note = result.detector.error.empty() ? note : result.detector.error;
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

}  // namespace nanotrack
