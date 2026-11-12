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
  std::array<std::optional<RecoveryCandidateEvidence>, 2> verifications{};
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
      std::optional<BBox> subtarget_region = std::nullopt);
  const RecoveryCandidate* find(RecoveryCandidateKey key) const noexcept;
  const RecoveryCandidate* select_probe() const noexcept;
  void advance_subtarget_probe(RecoveryCandidateKey key, const BBox& bbox,
                               std::size_t index, bool final_probe) noexcept;
  bool conflicted() const noexcept;
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
  std::int64_t gap_ms_;
  std::int64_t detector_stale_ms_;
  std::uint64_t detector_window_frames_;
  float min_iou_;
  float max_center_distance_;
  float max_area_log_change_;
  float max_aspect_log_change_;
  std::vector<RecoveryCandidate> entries_;
  std::optional<FrameStamp> frame_;
  std::optional<FrameStamp> unrepresented_conflict_;
  std::uint64_t generation_ = 0;
  std::uint64_t next_id_ = 1;
};

}  // namespace nanotrack
