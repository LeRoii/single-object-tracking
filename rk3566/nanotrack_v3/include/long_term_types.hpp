#pragma once

#include "tracker_math.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nanotrack {

enum class TrackingMode {
  kTracking,
  kUncertain,
  kLocalRecovery,
  kAssistedRecovery,
  kLost,
  kOcclusionHold,
};

enum class TrackingPhase { kTracking, kSearching, kInvalid };
enum class ControlSource { kNone, kObservation, kMotionSearch };

enum class QualityLevel { kReliable, kWeak, kRejected };
enum class OcclusionReferenceSource {
  kNone, kPartialStableMedian, kFullStableMedian
};
enum class OcclusionTriggerReason {
  kNone,
  kSevereWidth,
  kSevereHeight,
  kGradualWidthTemplate,
  kGradualWidthPeak,
  kGradualWidthMotion,
  kGradualHeightTemplate,
  kGradualHeightPeak,
  kGradualHeightMotion,
};
enum class OcclusionProbeType { kNone, kSearch, kConfirm };
enum class TemplateKind { kAnchor, kRecent };
enum class TemplateAction { kNone, kStageRecent, kCommitRecent, kRollbackRecent };
enum class HypothesisSource { kMotion, kDetector };
enum class DetectorTargetMode { kFullObject, kSubtarget };
enum class DetectorBatchStatus {
  kNone,
  kAccepted,
  kStale,
  kFrameMismatch,
  kInvalid,
  kPositionRejected,
};

struct FrameSize {
  int width = 0;
  int height = 0;
};

struct ImageView {
  const std::uint8_t* data = nullptr;
  int width = 0;
  int height = 0;
  std::size_t stride_bytes = 0;
  int channels = 0;
};

struct TrackerObservation {
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  BBox bbox{};
  ResponseMetrics metrics{};
  TemplateKind template_kind = TemplateKind::kAnchor;
  std::uint64_t search_token = 0;
};

struct DetectorCandidate {
  BBox bbox{};
  float confidence = 0.0F;
  int class_id = -1;
};

enum class TargetClassStatus { kFixed, kEstablished, kUnknown };
enum class TargetClassReason {
  kFixedOverride, kUniqueMatch, kNoMatch, kAmbiguous,
  kDetectorUnavailable, kDetectorError
};
enum class TargetClassCandidateRejection {
  kNone, kInvalidBox, kInvalidClass, kInvalidConfidence, kNoMatch
};

struct TargetClassMatchConfig {
  float min_roi_coverage = 0.50F;
  float max_center_ratio = 0.60F;
  float ambiguity_margin = 0.10F;
};

struct TargetClassCandidateMatch {
  std::size_t original_index = 0;
  DetectorCandidate candidate{};
  BBox clipped_bbox{};
  float roi_coverage = 0.0F;
  float iou = 0.0F;
  float center_ratio = 0.0F;
  float score = 0.0F;
  bool eligible = false;
  TargetClassCandidateRejection rejection = TargetClassCandidateRejection::kNone;
};

struct TargetClassState {
  TargetClassStatus status = TargetClassStatus::kUnknown;
  TargetClassReason reason = TargetClassReason::kNoMatch;
  std::optional<int> target_class_id;
  bool target_class_allows_recovery = false;
  std::optional<int> best_class_id;
  float best_score = 0.0F;
  std::optional<int> runner_up_class_id;
  float runner_up_score = 0.0F;
  std::optional<TargetClassCandidateMatch> selected_candidate;
  std::vector<TargetClassCandidateMatch> candidates;
};

enum class DetectorClassDisposition {
  kUnchecked, kMatch, kMismatch, kTargetUnknown, kInvalidClass
};

enum class DetectorCandidateRejection {
  kNone, kInvalid, kGenerationInvalidated, kFrameMismatch, kStale,
  kHoldInvalidated, kTargetClassUnknown, kClassMismatch,
  kPositionRejected, kCapacityRejected
};

struct DetectorCandidateDiagnostic {
  DetectorCandidate candidate{};
  std::optional<float> center_ratio;
  std::optional<std::uint64_t> candidate_id;
  bool batch_eligible = false;
  bool position_gate_pass = false;
  bool queued = false;
  bool drawable = false;
  DetectorCandidateRejection rejection = DetectorCandidateRejection::kNone;
  DetectorClassDisposition class_disposition = DetectorClassDisposition::kUnchecked;
};

struct DetectorRequest {
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  BBox roi{};
  std::size_t max_candidates = 3;
  std::uint64_t generation = 0;
};

struct DetectorTiming {
  float preprocess_ms = 0.0F;
  float inference_ms = 0.0F;
  float postprocess_ms = 0.0F;
  float total_ms = 0.0F;
};

struct DetectorBatch {
  std::uint64_t source_frame_id = 0;
  std::int64_t source_timestamp_ms = 0;
  std::vector<DetectorCandidate> candidates;
  std::size_t raw_candidate_count = 0;
  std::size_t filtered_candidate_count = 0;
  DetectorTiming timing{};
  std::uint64_t generation = 0;
  std::uint64_t request_id = 0;
};

struct DetectorBlockingResult {
  std::uint64_t request_id = 0;
  DetectorBatch batch{};
};

struct DetectorDiagnostics {
  bool configured = false;
  bool ready = false;
  bool request_submitted = false;
  bool batch_received = false;
  std::uint64_t request_frame_id = 0;
  std::uint64_t batch_frame_id = 0;
  std::int64_t batch_age_ms = 0;
  std::size_t raw_candidate_count = 0;
  std::size_t filtered_candidate_count = 0;
  DetectorTiming timing{};
  std::string error;
};

const char* to_string(TrackingMode value);
const char* to_string(TrackingPhase value);
const char* to_string(ControlSource value);
const char* to_string(OcclusionReferenceSource value);
const char* to_string(OcclusionTriggerReason value);
const char* to_string(OcclusionProbeType value);
const char* to_string(QualityLevel value);
const char* to_string(TemplateKind value);
const char* to_string(TemplateAction value);
const char* to_string(HypothesisSource value);
const char* to_string(DetectorTargetMode value);
const char* to_string(DetectorBatchStatus value);
const char* to_string(TargetClassStatus value);
const char* to_string(TargetClassReason value);
const char* to_string(TargetClassCandidateRejection value);
const char* to_string(DetectorClassDisposition value);
const char* to_string(DetectorCandidateRejection value);

}  // namespace nanotrack
