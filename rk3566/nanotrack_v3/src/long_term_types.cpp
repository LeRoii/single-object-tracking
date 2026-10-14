#include "long_term_types.hpp"

namespace nanotrack {

const char* to_string(TrackingMode value) {
  switch (value) {
    case TrackingMode::kTracking: return "TRACKING";
    case TrackingMode::kUncertain: return "UNCERTAIN";
    case TrackingMode::kLocalRecovery: return "LOCAL_RECOVERY";
    case TrackingMode::kAssistedRecovery: return "ASSISTED_RECOVERY";
    case TrackingMode::kLost: return "LOST";
  }
  return "UNKNOWN";
}

const char* to_string(QualityLevel value) {
  switch (value) {
    case QualityLevel::kReliable: return "RELIABLE";
    case QualityLevel::kWeak: return "WEAK";
    case QualityLevel::kRejected: return "REJECTED";
  }
  return "UNKNOWN";
}

const char* to_string(TemplateKind value) {
  return value == TemplateKind::kAnchor ? "ANCHOR" : "RECENT";
}

const char* to_string(TemplateAction value) {
  switch (value) {
    case TemplateAction::kNone: return "NONE";
    case TemplateAction::kStageRecent: return "STAGE_RECENT";
    case TemplateAction::kCommitRecent: return "COMMIT_RECENT";
    case TemplateAction::kRollbackRecent: return "ROLLBACK_RECENT";
  }
  return "UNKNOWN";
}

const char* to_string(HypothesisSource value) {
  return value == HypothesisSource::kMotion ? "MOTION" : "DETECTOR";
}

const char* to_string(DetectorTargetMode value) {
  return value == DetectorTargetMode::kFullObject ? "FULL_OBJECT" : "SUBTARGET";
}

const char* to_string(DetectorBatchStatus value) {
  switch (value) {
    case DetectorBatchStatus::kNone: return "NONE";
    case DetectorBatchStatus::kAccepted: return "ACCEPTED";
    case DetectorBatchStatus::kStale: return "STALE";
    case DetectorBatchStatus::kFrameMismatch: return "FRAME_MISMATCH";
    case DetectorBatchStatus::kInvalid: return "INVALID";
  }
  return "UNKNOWN";
}

}  // namespace nanotrack
