#include "recovery_candidates.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace nanotrack {
namespace {

constexpr std::size_t kRequiredHits = 2;

std::size_t source_index(RecoveryCandidateSource source) {
  return static_cast<std::size_t>(source);
}

bool has_evidence(const RecoveryCandidate& candidate) {
  return std::any_of(candidate.evidence.begin(), candidate.evidence.end(),
                     [](const auto& evidence) { return evidence.has_value(); });
}

bool valid_box(const BBox& bbox) {
  return std::isfinite(bbox.x) && std::isfinite(bbox.y) &&
         std::isfinite(bbox.width) && std::isfinite(bbox.height) &&
         bbox.width > 0.0F && bbox.height > 0.0F;
}

bool preferred_detector(const RecoveryCandidateEvidence& first,
                        const RecoveryCandidateEvidence& second) {
  if (first.motion_residual != second.motion_residual) {
    return first.motion_residual < second.motion_residual;
  }
  return first.confidence > second.confidence;
}

bool preferred(const RecoveryCandidate& first,
               const RecoveryCandidate& second) {
  for (const auto source : {RecoveryCandidateSource::kRecent,
                            RecoveryCandidateSource::kSafe,
                            RecoveryCandidateSource::kDetector}) {
    if (first.from(source).has_value() != second.from(source).has_value()) {
      return first.from(source).has_value();
    }
    if (first.from(source)) {
      if (source == RecoveryCandidateSource::kDetector) {
        if (preferred_detector(*first.from(source), *second.from(source))) return true;
        if (preferred_detector(*second.from(source), *first.from(source))) return false;
      }
      break;
    }
  }
  return first.key.id < second.key.id;
}

}  // namespace

const RecoveryCandidateEvidence& RecoveryCandidate::preferred_evidence() const {
  for (const auto& item : evidence) {
    if (item) return *item;
  }
  throw std::logic_error("recovery candidate has no evidence");
}

const std::optional<RecoveryCandidateEvidence>& RecoveryCandidate::from(
    RecoveryCandidateSource source) const {
  return evidence[source_index(source)];
}

std::size_t RecoveryCandidate::hits() const {
  return static_cast<std::size_t>(std::count_if(
      verifications.begin(), verifications.end(),
      [](const auto& item) { return item.has_value(); }));
}

RecoveryCandidateTable::RecoveryCandidateTable(const ControllerConfig& config)
    : detector_capacity_(config.detector_max_candidates),
      gap_ms_(config.motion_max_dt_ms),
      detector_stale_ms_(config.detector_stale_ms),
      detector_window_frames_(config.detector_target_mode == DetectorTargetMode::kSubtarget
          ? std::max<std::uint64_t>(kConfirmationWindow,
                                    config.detector_subtarget_max_probes + 1)
          : kConfirmationWindow),
      min_iou_(config.detector_confirmation_iou),
      max_center_distance_(config.detector_confirmation_center_distance),
      max_area_log_change_(config.detector_confirmation_max_area_log_change),
      max_aspect_log_change_(config.detector_confirmation_max_aspect_log_change) {
  validate_controller_config(config);
  entries_.reserve(2 + detector_capacity_);
}

bool RecoveryCandidateTable::expired(FrameStamp evidence,
                                     std::int64_t max_age_ms,
                                     std::uint64_t max_age_frames) const noexcept {
  return frame_->frame_id - evidence.frame_id >= max_age_frames ||
         static_cast<std::uint64_t>(frame_->timestamp_ms) -
                 static_cast<std::uint64_t>(evidence.timestamp_ms) >
             static_cast<std::uint64_t>(max_age_ms);
}

void RecoveryCandidateTable::expire(std::uint64_t frame_id,
                                    std::int64_t timestamp_ms,
                                    std::uint64_t generation) noexcept {
  if (frame_ && generation == generation_) {
    if (frame_id < frame_->frame_id || timestamp_ms < frame_->timestamp_ms ||
        (frame_id == frame_->frame_id) != (timestamp_ms == frame_->timestamp_ms)) {
      clear();
      return;
    }
    if (frame_id - frame_->frame_id >= kConfirmationWindow ||
        static_cast<std::uint64_t>(timestamp_ms) -
                static_cast<std::uint64_t>(frame_->timestamp_ms) >
            static_cast<std::uint64_t>(gap_ms_)) {
      clear();
    }
  } else {
    clear();
  }
  generation_ = generation;
  frame_ = FrameStamp{frame_id, timestamp_ms};
  for (auto& candidate : entries_) {
    for (std::size_t index = 0; index < candidate.evidence.size(); ++index) {
      auto& item = candidate.evidence[index];
      const bool detector = index == source_index(RecoveryCandidateSource::kDetector);
      if (item && expired({item->frame_id, item->timestamp_ms},
                          detector ? detector_stale_ms_ : gap_ms_,
                          detector ? detector_window_frames_ : kConfirmationWindow)) {
        item.reset();
      }
    }
    for (auto& hit : candidate.verifications) {
      if (hit && expired({hit->frame_id, hit->timestamp_ms}, gap_ms_)) hit.reset();
    }
  }
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                               [](const auto& entry) { return !has_evidence(entry); }),
                 entries_.end());
  if (focused_detector_ && !find(*focused_detector_)) focused_detector_.reset();
  if (unrepresented_conflict_ &&
      expired(*unrepresented_conflict_, std::min(gap_ms_, detector_stale_ms_))) {
    unrepresented_conflict_.reset();
  }
}

void RecoveryCandidateTable::clear() noexcept {
  entries_.clear();
  frame_.reset();
  unrepresented_conflict_.reset();
  focused_detector_.reset();
}

bool RecoveryCandidateTable::geometry_consistent(const BBox& reference,
                                                 const BBox& bbox) const noexcept {
  if (!valid_box(reference) || !valid_box(bbox)) return false;
  const double first_area = static_cast<double>(reference.width) * reference.height;
  const double second_area = static_cast<double>(bbox.width) * bbox.height;
  const double first_aspect = static_cast<double>(reference.width) / reference.height;
  const double second_aspect = static_cast<double>(bbox.width) / bbox.height;
  const double dx = static_cast<double>(bbox.x) - reference.x +
                    0.5 * (static_cast<double>(bbox.width) - reference.width);
  const double dy = static_cast<double>(bbox.y) - reference.y +
                    0.5 * (static_cast<double>(bbox.height) - reference.height);
  const double overlap_width = std::max(0.0, std::min(
      static_cast<double>(reference.x) + reference.width,
      static_cast<double>(bbox.x) + bbox.width) - std::max(reference.x, bbox.x));
  const double overlap_height = std::max(0.0, std::min(
      static_cast<double>(reference.y) + reference.height,
      static_cast<double>(bbox.y) + bbox.height) - std::max(reference.y, bbox.y));
  const double intersection = overlap_width * overlap_height;
  return intersection / (first_area + second_area - intersection) >= min_iou_ &&
         std::hypot(dx, dy) / std::sqrt(std::max(1.0, first_area)) <=
             max_center_distance_ &&
         std::fabs(std::log(second_area / first_area)) <= max_area_log_change_ &&
         std::fabs(std::log(second_aspect / first_aspect)) <= max_aspect_log_change_;
}

bool RecoveryCandidateTable::temporally_consistent(
    const BBox& reference, const BBox& bbox) const noexcept {
  if (!valid_box(reference) || !valid_box(bbox)) return false;
  const double first_area = static_cast<double>(reference.width) * reference.height;
  const double second_area = static_cast<double>(bbox.width) * bbox.height;
  const double first_aspect = static_cast<double>(reference.width) / reference.height;
  const double second_aspect = static_cast<double>(bbox.width) / bbox.height;
  const double dx = static_cast<double>(bbox.x) - reference.x +
                    0.5 * (static_cast<double>(bbox.width) - reference.width);
  const double dy = static_cast<double>(bbox.y) - reference.y +
                    0.5 * (static_cast<double>(bbox.height) - reference.height);
  return std::hypot(dx, dy) / std::sqrt(std::max(1.0, first_area)) <=
             max_center_distance_ &&
         std::fabs(std::log(second_area / first_area)) <= max_area_log_change_ &&
         std::fabs(std::log(second_aspect / first_aspect)) <= max_aspect_log_change_;
}

std::optional<RecoveryCandidateKey> RecoveryCandidateTable::observe(
    RecoveryCandidateSource source, const BBox& bbox, float confidence,
    std::optional<std::int64_t> source_timestamp_ms,
    std::optional<int> detector_class_id, bool tracker_verified,
    float motion_residual, std::optional<BBox> subtarget_region) {
  if (!frame_ || !valid_box(bbox) || !std::isfinite(confidence) ||
      confidence < 0.0F || confidence > 1.0F ||
      !std::isfinite(motion_residual) || motion_residual < 0.0F ||
      (subtarget_region && !valid_box(*subtarget_region))) {
    return std::nullopt;
  }
  const bool detector = source == RecoveryCandidateSource::kDetector;
  RecoveryCandidateEvidence evidence{
      bbox, frame_->frame_id, source_timestamp_ms.value_or(frame_->timestamp_ms),
      confidence, motion_residual, detector_class_id};
  if (evidence.timestamp_ms > frame_->timestamp_ms ||
      (detector && expired({evidence.frame_id, evidence.timestamp_ms}, detector_stale_ms_))) {
    return std::nullopt;
  }
  std::optional<RecoveryCandidateKey> matched;
  for (const auto& candidate : entries_) {
    const bool compare_regions = detector && subtarget_region && candidate.subtarget_carrier;
    const auto& same_source = candidate.from(source);
    const BBox& reference = compare_regions ? *candidate.subtarget_carrier :
        same_source ? same_source->bbox : candidate.preferred_evidence().bbox;
    const BBox& observed = compare_regions ? *subtarget_region : bbox;
    const bool consistent = !detector && same_source
        ? temporally_consistent(reference, observed)
        : geometry_consistent(reference, observed);
    if (!consistent) continue;
    if (matched) {
      unrepresented_conflict_ = FrameStamp{evidence.frame_id, evidence.timestamp_ms};
      mark_identity_conflict();
      return std::nullopt;
    }
    matched = candidate.key;
  }

  const auto index = source_index(source);
  if (detector && (!matched || !find(*matched)->from(source)) &&
             detector_count() == detector_capacity_) {
    // A geometrically distinct eligible detection remains an ambiguity even
    // if its metadata cannot be retained at the configured capacity.
    unrepresented_conflict_ = FrameStamp{evidence.frame_id, evidence.timestamp_ms};
    auto weakest = entries_.end();
    for (auto entry = entries_.begin(); entry != entries_.end(); ++entry) {
      if (!entry->from(source)) continue;
      if (weakest == entries_.end() || preferred(*weakest, *entry)) weakest = entry;
    }
    if (weakest == entries_.end() || !preferred_detector(evidence, *weakest->from(source)) ||
        weakest->from(RecoveryCandidateSource::kRecent) ||
        weakest->from(RecoveryCandidateSource::kSafe)) {
      mark_identity_conflict();
      return std::nullopt;
    }
    weakest->evidence[index].reset();
  }
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                               [](const auto& entry) { return !has_evidence(entry); }),
                 entries_.end());
  auto entry = std::find_if(entries_.begin(), entries_.end(),
                            [&](const auto& candidate) { return candidate.key == matched; });
  if (entry == entries_.end()) {
    if (next_id_ == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error("recovery candidate ID exhausted");
    }
    entries_.push_back(RecoveryCandidate{RecoveryCandidateKey{
        source, next_id_++, generation_}});
    entry = entries_.end() - 1;
  }
  auto& source_evidence = entry->evidence[index];
  if (!detector || !source_evidence || source_evidence->frame_id != evidence.frame_id ||
      preferred_detector(evidence, *source_evidence)) {
    if (detector && subtarget_region && source_evidence && entry->subtarget_carrier) {
      // An async refresh must preserve the probe already sent to NanoTrack.
      evidence.bbox = source_evidence->bbox;
      if (entry->subtarget_probes_exhausted &&
          entry->last_subtarget_probe_frame != frame_->frame_id &&
          source_evidence->frame_id != evidence.frame_id) {
        entry->subtarget_probes_attempted = 0;
        entry->subtarget_probes_exhausted = false;
      }
    } else if (detector && subtarget_region) {
      entry->subtarget_probes_attempted = 0;
      entry->subtarget_probes_exhausted = false;
    }
    if (detector && subtarget_region) entry->subtarget_carrier = subtarget_region;
    source_evidence = evidence;
  }
  if (!detector && tracker_verified) {
    auto& hits = entry->verifications;
    if (!hits[1] || hits[1]->frame_id != frame_->frame_id) {
      if (hits[1] && !temporally_consistent(hits[1]->bbox, bbox)) hits = {};
      hits[0] = hits[1];
      hits[1] = evidence;
    }
  }
  if (focused_detector_ && std::any_of(entries_.begin(), entries_.end(),
          [&](const RecoveryCandidate& candidate) {
            return candidate.key != *focused_detector_ &&
                   candidate.hits() == kRequiredHits;
          })) {
    mark_identity_conflict();
  } else if (conflicted()) {
    for (auto& candidate : entries_) candidate.verifications = {};
  }
  return entry->key;
}

const RecoveryCandidate* RecoveryCandidateTable::find(
    RecoveryCandidateKey key) const noexcept {
  const auto entry = std::find_if(entries_.begin(), entries_.end(),
                                  [&](const auto& candidate) { return candidate.key == key; });
  return entry == entries_.end() ? nullptr : &*entry;
}

const RecoveryCandidate* RecoveryCandidateTable::select_probe() const noexcept {
  if (focused_detector_) {
    const auto* focused = find(*focused_detector_);
    if (focused && !focused->subtarget_probes_exhausted) return focused;
  }
  const RecoveryCandidate* best = nullptr;
  for (const auto& candidate : entries_) {
    if (candidate.subtarget_probes_exhausted &&
        !candidate.from(RecoveryCandidateSource::kRecent) &&
        !candidate.from(RecoveryCandidateSource::kSafe)) {
      continue;
    }
    if (!best || preferred(candidate, *best)) best = &candidate;
  }
  return best;
}

bool RecoveryCandidateTable::focus_detector(RecoveryCandidateKey key) noexcept {
  const auto* candidate = find(key);
  if (!frame_ || unrepresented_conflict_ || !candidate ||
      !candidate->from(RecoveryCandidateSource::kDetector)) {
    return false;
  }
  if (focused_detector_ == key) return true;
  focused_detector_ = key;
  // A detector only proposes an identity. Confirmation starts with fresh
  // NanoTrack observations made after the detector batch was received.
  for (auto& entry : entries_) entry.verifications = {};
  return true;
}

void RecoveryCandidateTable::advance_subtarget_probe(
    RecoveryCandidateKey key, const BBox& bbox,
    std::size_t index, bool final_probe) noexcept {
  const auto entry = std::find_if(entries_.begin(), entries_.end(),
                                  [&](const auto& candidate) { return candidate.key == key; });
  if (!frame_ || entry == entries_.end()) return;
  auto& detector = entry->evidence[source_index(RecoveryCandidateSource::kDetector)];
  if (!detector || !entry->subtarget_carrier) return;
  detector->bbox = bbox;
  entry->subtarget_probes_attempted = index + 1;
  entry->last_subtarget_probe_frame = frame_->frame_id;
  entry->subtarget_probes_exhausted = final_probe;
}

bool RecoveryCandidateTable::conflicted() const noexcept {
  if (unrepresented_conflict_) return true;
  if (focused_detector_ && find(*focused_detector_)) {
    return std::any_of(entries_.begin(), entries_.end(),
        [&](const RecoveryCandidate& candidate) {
          return candidate.key != *focused_detector_ &&
                 candidate.hits() == kRequiredHits;
        });
  }
  return entries_.size() > 1;
}

bool RecoveryCandidateTable::confirmed(RecoveryCandidateKey key) const noexcept {
  const auto* candidate = find(key);
  return !conflicted() && candidate && candidate->hits() == kRequiredHits &&
         candidate->verifications[1]->frame_id == frame_->frame_id;
}

void RecoveryCandidateTable::mark_identity_conflict() noexcept {
  // Conflict is evidence scoped to the current candidate window. It must not
  // permanently poison an otherwise continuous NanoTrack tracklet after a
  // spatially distinct rival expires.
  if (frame_) unrepresented_conflict_ = *frame_;
  for (auto& candidate : entries_) candidate.verifications = {};
}

std::size_t RecoveryCandidateTable::detector_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [](const auto& candidate) { return candidate.from(RecoveryCandidateSource::kDetector).has_value(); }));
}

}  // namespace nanotrack
