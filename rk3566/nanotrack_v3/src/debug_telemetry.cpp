#include "debug_telemetry.hpp"

#include <cctype>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace nanotrack {
namespace {

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

std::string sanitized_value(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (const unsigned char character : value) {
    sanitized += std::isspace(character) ? '_' : static_cast<char>(character);
  }
  return sanitized.empty() ? "none" : sanitized;
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
  previous_control_valid_ = false;
}

std::vector<std::string> DebugTelemetry::frame_events(
    const RuntimeFrameResult& result,
    float pipeline_ms,
    bool force_heartbeat) {
  std::vector<std::string> events;
  ++observed_frames_;

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
  previous_control_valid_ = result.output.control_valid;

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

  return events;
}

}  // namespace nanotrack
