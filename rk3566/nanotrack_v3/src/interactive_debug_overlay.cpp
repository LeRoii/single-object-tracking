#include "interactive_debug_overlay.hpp"

#include <cmath>
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

bool drawable_bbox(const BBox& bbox) {
  return std::isfinite(bbox.x) && std::isfinite(bbox.y) &&
         std::isfinite(bbox.width) && std::isfinite(bbox.height) &&
         bbox.x >= 0.0F && bbox.y >= 0.0F && bbox.width > 0.0F &&
         bbox.height > 0.0F;
}

const char* rejection_label(DetectorCandidateRejection rejection) {
  switch (rejection) {
    case DetectorCandidateRejection::kNone: return "PASS";
    case DetectorCandidateRejection::kInvalid: return "INVALID";
    case DetectorCandidateRejection::kFrameMismatch: return "FRAME_MISMATCH";
    case DetectorCandidateRejection::kStale: return "STALE";
    case DetectorCandidateRejection::kHoldInvalidated:
      return "HOLD_INVALIDATED";
    case DetectorCandidateRejection::kPositionRejected: return "REJECT_POS";
    case DetectorCandidateRejection::kCapacityRejected: return "CAPACITY";
  }
  return "UNKNOWN";
}

std::string diagnostic_label(const DetectorCandidateDiagnostic& diagnostic,
                             std::size_t index,
                             const DetectorDiagnostics& batch) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(3)
         << "DET RAW[" << index << "] "
         << rejection_label(diagnostic.rejection)
         << " cls=" << diagnostic.candidate.class_id
         << " conf=" << diagnostic.candidate.confidence
         << " d=" << optional_float(diagnostic.center_ratio)
         << " src="
         << (batch.batch_received ? std::to_string(batch.batch_frame_id)
                                  : "none")
         << " age="
         << (batch.batch_received ? std::to_string(batch.batch_age_ms)
                                  : "none")
         << " queued=" << (diagnostic.queued ? 1 : 0)
         << " reason=" << rejection_label(diagnostic.rejection);
  return stream.str();
}

}  // namespace

InteractiveDebugOverlay make_interactive_debug_overlay(
    const RuntimeFrameResult& result) {
  InteractiveDebugOverlay overlay;
  overlay.boxes.push_back(DebugBoxDescriptor{
      DebugBoxKind::kMotionPrediction,
      result.plan.motion_predicted_bbox,
      "MOTION PRED",
      2});

  for (std::size_t index = 0;
       index < result.output.detector_candidate_diagnostics.size(); ++index) {
    const DetectorCandidateDiagnostic& diagnostic =
        result.output.detector_candidate_diagnostics[index];
    const std::string label = diagnostic_label(diagnostic, index,
                                               result.detector);
    if (!diagnostic.drawable || !drawable_bbox(diagnostic.candidate.bbox)) {
      overlay.texts.push_back(DebugTextDescriptor{label});
      continue;
    }
    overlay.boxes.push_back(DebugBoxDescriptor{
        diagnostic.rejection == DetectorCandidateRejection::kNone &&
                diagnostic.position_gate_pass
            ? DebugBoxKind::kDetectorCandidatePass
            : DebugBoxKind::kDetectorCandidateRejected,
        diagnostic.candidate.bbox,
        label,
        2});
  }

  if (result.plan.hypothesis_source == HypothesisSource::kDetector &&
      result.plan.detector_candidate_region) {
    overlay.boxes.push_back(DebugBoxDescriptor{
        DebugBoxKind::kDetectorCandidate,
        *result.plan.detector_candidate_region,
        "DET CAND id=" + optional_id(result.plan.detector_candidate_id) +
            " conf=" +
            optional_float(result.plan.detector_candidate_confidence) +
            " d=" +
            optional_float(result.plan.detector_candidate_center_ratio),
        3});
  }

  if (result.plan.hypothesis_source == HypothesisSource::kDetector) {
    overlay.boxes.push_back(DebugBoxDescriptor{
        DebugBoxKind::kDetectorProbe,
        result.plan.predicted_bbox,
        "DET PROBE index=" +
            std::to_string(result.plan.detector_probe_index),
        1});
  }

  if (result.output.mode != TrackingMode::kOcclusionHold) {
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
                formatted_float(observation.metrics.raw_score),
            2});
        break;
      }
    }
  }

  overlay.control_valid = result.output.accepted &&
                          result.output.control_valid;
  if (overlay.control_valid) {
    overlay.boxes.push_back(DebugBoxDescriptor{
        DebugBoxKind::kAcceptedControl,
        result.output.bbox,
        "ACCEPTED CTRL",
        3});
    overlay.status_label = "ACCEPTED CTRL";
  } else if (result.output.mode == TrackingMode::kOcclusionHold) {
    overlay.status_label = "OCCLUSION HOLD remaining=" +
                           std::to_string(
                               result.output.occlusion_hold_remaining_frames);
  } else {
    overlay.status_label = "REJECTED NO_CTRL";
  }
  return overlay;
}

}  // namespace nanotrack
