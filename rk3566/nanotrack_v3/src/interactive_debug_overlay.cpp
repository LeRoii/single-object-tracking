#include "interactive_debug_overlay.hpp"

#include <iomanip>
#include <locale>
#include <sstream>

namespace nanotrack {
namespace {

std::string formatted_float(float value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(3) << value;
  return stream.str();
}

std::string optional_id(const std::optional<std::uint64_t>& value) {
  return value ? std::to_string(*value) : "none";
}

std::string optional_float(const std::optional<float>& value) {
  return value ? formatted_float(*value) : "none";
}

}  // namespace

InteractiveDebugOverlay make_interactive_debug_overlay(
    const RuntimeFrameResult& result) {
  InteractiveDebugOverlay overlay;
  overlay.boxes.push_back(DebugBoxDescriptor{
      DebugBoxKind::kMotionPrediction,
      result.plan.motion_predicted_bbox,
      "MOTION PRED"});

  if (result.plan.hypothesis_source == HypothesisSource::kDetector &&
      result.plan.detector_candidate_region) {
    overlay.boxes.push_back(DebugBoxDescriptor{
        DebugBoxKind::kDetectorCandidate,
        *result.plan.detector_candidate_region,
        "DET CAND id=" + optional_id(result.plan.detector_candidate_id) +
            " conf=" +
            optional_float(result.plan.detector_candidate_confidence) +
            " d=" +
            optional_float(result.plan.detector_candidate_center_ratio)});
  }

  if (result.plan.hypothesis_source == HypothesisSource::kDetector) {
    overlay.boxes.push_back(DebugBoxDescriptor{
        DebugBoxKind::kDetectorProbe,
        result.plan.predicted_bbox,
        "DET PROBE index=" +
            std::to_string(result.plan.detector_probe_index)});
  }

  for (const TemplateKind kind :
       {TemplateKind::kAnchor, TemplateKind::kRecent}) {
    for (const TrackerObservation& observation : result.observations) {
      if (observation.template_kind != kind) continue;
      overlay.boxes.push_back(DebugBoxDescriptor{
          kind == TemplateKind::kAnchor
              ? DebugBoxKind::kNanoTrackAnchor
              : DebugBoxKind::kNanoTrackRecent,
          observation.bbox,
          std::string(kind == TemplateKind::kAnchor ? "NT ANCHOR score="
                                                    : "NT RECENT score=") +
              formatted_float(observation.metrics.raw_score)});
      break;
    }
  }

  overlay.control_valid = result.output.accepted &&
                          result.output.control_valid;
  if (overlay.control_valid) {
    overlay.boxes.push_back(DebugBoxDescriptor{
        DebugBoxKind::kAcceptedControl,
        result.output.bbox,
        "ACCEPTED CTRL"});
    overlay.status_label = "ACCEPTED CTRL";
  } else {
    overlay.status_label = "REJECTED NO_CTRL";
  }
  return overlay;
}

}  // namespace nanotrack
