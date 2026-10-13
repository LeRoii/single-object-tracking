#pragma once

#include "tracker_math.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nanotrack {

enum class TrackingMode {
  kTracking,
  kUncertain,
  kLocalRecovery,
  kAssistedRecovery,
  kLost,
};

enum class QualityLevel { kReliable, kWeak, kRejected };
enum class TemplateKind { kAnchor, kRecent };
enum class TemplateAction { kNone, kStageRecent, kCommitRecent, kRollbackRecent };
enum class HypothesisSource { kMotion, kDetector };
enum class DetectorBatchStatus {
  kNone,
  kAccepted,
  kStale,
  kFrameMismatch,
  kInvalid,
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

struct DetectorRequest {
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  BBox roi{};
  std::size_t max_candidates = 3;
};

struct DetectorBatch {
  std::uint64_t source_frame_id = 0;
  std::int64_t source_timestamp_ms = 0;
  std::vector<DetectorCandidate> candidates;
};

const char* to_string(TrackingMode value);
const char* to_string(QualityLevel value);
const char* to_string(TemplateKind value);
const char* to_string(TemplateAction value);
const char* to_string(HypothesisSource value);
const char* to_string(DetectorBatchStatus value);

}  // namespace nanotrack
