#pragma once

#include "controller_config.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nanotrack {

struct RecoveryCandidateEvidence {
  BBox bbox{};
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  float confidence = 0.0F;
  float motion_residual = 0.0F;
  std::optional<int> detector_class_id;
};

struct RecoveryCandidate {
  RecoveryCandidateKey key{};
  std::array<std::optional<RecoveryCandidateEvidence>, 3> evidence{};
  std::array<std::optional<RecoveryCandidateEvidence>, 3> verifications{};
  // A detector must still be fresh when one of the current NanoTrack hits
  // occurs. Keep that association through the second hit even if the detector
  // metadata ages out between the two frames.
  std::optional<std::uint64_t> detector_supported_hit_frame = std::nullopt;
  // The detector may leave the candidate table's short frame window while
  // still within its timestamp limit. This handoff belongs only to a
  // candidate that already has matching tracker evidence.
  std::optional<std::int64_t> detector_handoff_timestamp_ms = std::nullopt;
  // Geometry for associating a refreshed detector region with a live
  // tracker-verified subtarget. Only live detector evidence can seed offsets.
  std::optional<BBox> subtarget_carrier = std::nullopt;
  std::size_t subtarget_probes_attempted = 0;
  std::optional<std::uint64_t> last_subtarget_probe_frame = std::nullopt;
  bool subtarget_probes_exhausted = false;

  const RecoveryCandidateEvidence& preferred_evidence() const;
  const std::optional<RecoveryCandidateEvidence>& from(
      RecoveryCandidateSource source) const;
  std::size_t hits() const;
  bool has_detector_supported_hit() const;
};

class RecoveryCandidateTable {
 public:
  explicit RecoveryCandidateTable(const ControllerConfig& config);

  // Open the current frame before observing evidence. A frame-number delta of
  // two permits one missing frame. Source timestamps additionally bound age.
  void expire(std::uint64_t frame_id, std::int64_t timestamp_ms,
              std::uint64_t generation) noexcept;
  void clear() noexcept;
  std::optional<RecoveryCandidateKey> observe(
      RecoveryCandidateSource source, const BBox& bbox, float confidence = 0.0F,
      std::optional<std::int64_t> source_timestamp_ms = std::nullopt,
      std::optional<int> detector_class_id = std::nullopt,
      bool tracker_verified = true, float motion_residual = 0.0F,
      std::optional<BBox> subtarget_region = std::nullopt,
      std::optional<RecoveryCandidateKey> triad_key = std::nullopt);
  const RecoveryCandidate* find(RecoveryCandidateKey key) const noexcept;
  const RecoveryCandidate* select_probe() const noexcept;
  bool focus_detector(RecoveryCandidateKey key) noexcept;
  // A full-object detector candidate supported by both tracker templates may
  // be confirmed after two consecutive frames, even with distant detections.
  std::optional<RecoveryCandidateKey> unique_detector_agreement(
      const BBox& recent, const BBox& safe) const noexcept;
  std::optional<RecoveryCandidateKey> focus_triad(
      const BBox& recent, const BBox& safe) noexcept;
  void observe_triad(RecoveryCandidateKey key) noexcept;
  bool triad_confirmed(RecoveryCandidateKey key) const noexcept;
  void advance_subtarget_probe(RecoveryCandidateKey key, const BBox& bbox,
                               std::size_t index, bool final_probe) noexcept;
  bool conflicted() const noexcept;
  const char* conflict_cause() const noexcept;
  bool confirmed(RecoveryCandidateKey key) const noexcept;
  void mark_identity_conflict() noexcept;
  bool geometry_consistent(const BBox& reference, const BBox& bbox) const noexcept;
  std::size_t detector_count() const noexcept;
  std::size_t size() const noexcept { return entries_.size(); }

 private:
  struct FrameStamp {
    std::uint64_t frame_id;
    std::int64_t timestamp_ms;
  };

  static constexpr std::uint64_t kConfirmationWindow = 3;

  bool expired(FrameStamp evidence, std::int64_t max_age_ms,
               std::uint64_t max_age_frames = kConfirmationWindow) const noexcept;
  bool temporally_consistent(const BBox& reference,
                             const BBox& bbox) const noexcept;

  std::size_t detector_capacity_;
  std::size_t required_hits_;
  std::int64_t gap_ms_;
  std::int64_t detector_stale_ms_;
  std::uint64_t detector_window_frames_;
  bool conflict_detector_handoff_;
  float min_iou_;
  float max_center_distance_;
  float max_area_log_change_;
  float max_aspect_log_change_;
  std::vector<RecoveryCandidate> entries_;
  std::optional<FrameStamp> frame_;
  std::optional<FrameStamp> unrepresented_conflict_;
  std::vector<RecoveryCandidateEvidence> overflow_detectors_;
  bool triad_overflow_override_ = false;
  std::optional<RecoveryCandidateKey> focused_detector_;
  std::optional<RecoveryCandidateKey> triad_candidate_;
  std::optional<std::uint64_t> triad_last_frame_;
  std::size_t triad_streak_ = 0;
  std::uint64_t generation_ = 0;
  std::uint64_t next_id_ = 1;
};

}  // namespace nanotrack
