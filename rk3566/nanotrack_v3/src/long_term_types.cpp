#include "long_term_types.hpp"

namespace nanotrack {

const char* to_string(DetectorClassDisposition value) {
  switch (value) {
    case DetectorClassDisposition::kUnchecked: return "UNCHECKED";
    case DetectorClassDisposition::kMatch: return "MATCH";
    case DetectorClassDisposition::kMismatch: return "MISMATCH";
    case DetectorClassDisposition::kTargetUnknown: return "TARGET_UNKNOWN";
    case DetectorClassDisposition::kInvalidClass: return "INVALID_CLASS";
  }
  return "UNKNOWN";
}

const char* to_string(DetectorCandidateRejection value) {
  switch (value) {
    case DetectorCandidateRejection::kNone: return "NONE";
    case DetectorCandidateRejection::kInvalid: return "INVALID";
    case DetectorCandidateRejection::kGenerationInvalidated: return "GENERATION_INVALIDATED";
    case DetectorCandidateRejection::kFrameMismatch: return "FRAME_MISMATCH";
    case DetectorCandidateRejection::kStale: return "STALE";
    case DetectorCandidateRejection::kHoldInvalidated: return "HOLD_INVALIDATED";
    case DetectorCandidateRejection::kTargetClassUnknown: return "TARGET_CLASS_UNKNOWN";
    case DetectorCandidateRejection::kClassMismatch: return "CLASS_MISMATCH";
    case DetectorCandidateRejection::kPositionRejected: return "POSITION_REJECTED";
    case DetectorCandidateRejection::kCapacityRejected: return "CAPACITY_REJECTED";
  }
  return "UNKNOWN";
}

const char* to_string(TargetClassStatus value) {
  switch (value) {
    case TargetClassStatus::kFixed: return "FIXED";
    case TargetClassStatus::kEstablished: return "ESTABLISHED";
    case TargetClassStatus::kUnknown: return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* to_string(TargetClassReason value) {
  switch (value) {
    case TargetClassReason::kFixedOverride: return "FIXED_OVERRIDE";
    case TargetClassReason::kUniqueMatch: return "UNIQUE_MATCH";
    case TargetClassReason::kNoMatch: return "NO_MATCH";
    case TargetClassReason::kAmbiguous: return "AMBIGUOUS";
    case TargetClassReason::kDetectorUnavailable: return "DETECTOR_UNAVAILABLE";
    case TargetClassReason::kDetectorError: return "DETECTOR_ERROR";
  }
  return "UNKNOWN";
}

const char* to_string(TargetClassCandidateRejection value) {
  switch (value) {
    case TargetClassCandidateRejection::kNone: return "NONE";
    case TargetClassCandidateRejection::kInvalidBox: return "INVALID_BOX";
    case TargetClassCandidateRejection::kInvalidClass: return "INVALID_CLASS";
    case TargetClassCandidateRejection::kInvalidConfidence: return "INVALID_CONFIDENCE";
    case TargetClassCandidateRejection::kNoMatch: return "NO_MATCH";
  }
  return "UNKNOWN";
}

const char* to_string(TrackingMode value) {
  switch (value) {
    case TrackingMode::kTracking: return "TRACKING";
    case TrackingMode::kUncertain: return "UNCERTAIN";
    case TrackingMode::kLocalRecovery: return "LOCAL_RECOVERY";
    case TrackingMode::kAssistedRecovery: return "ASSISTED_RECOVERY";
    case TrackingMode::kLost: return "LOST";
    case TrackingMode::kOcclusionHold: return "OCCLUSION_HOLD";
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
    case DetectorBatchStatus::kPositionRejected: return "POSITION_REJECTED";
  }
  return "UNKNOWN";
}

}  // namespace nanotrack
