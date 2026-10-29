#include "target_class.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace nanotrack {
namespace {
// Shared tolerance for eligibility, scoring ties, and ambiguity boundaries.
constexpr double kEpsilon = 1e-6;

bool clip(const BBox& box, FrameSize frame, BBox& clipped) {
  if (!std::isfinite(box.x) || !std::isfinite(box.y) ||
      !std::isfinite(box.width) || !std::isfinite(box.height) ||
      box.width <= 0 || box.height <= 0) return false;
  const double x = std::clamp<double>(box.x, 0, frame.width);
  const double y = std::clamp<double>(box.y, 0, frame.height);
  const double right = std::clamp<double>(double(box.x) + box.width, 0, frame.width);
  const double bottom = std::clamp<double>(double(box.y) + box.height, 0, frame.height);
  clipped = {float(x), float(y), float(right - x), float(bottom - y)};
  return clipped.width > 0 && clipped.height > 0;
}
double area(const BBox& box) { return double(box.width) * box.height; }
bool better_same_class(const TargetClassCandidateMatch& a,
                       const TargetClassCandidateMatch& b) {
  if (std::fabs(double(a.score) - b.score) > kEpsilon) return a.score > b.score;
  if (std::fabs(double(a.candidate.confidence) - b.candidate.confidence) > kEpsilon)
    return a.candidate.confidence > b.candidate.confidence;
  if (std::fabs(area(a.clipped_bbox) - area(b.clipped_bbox)) > kEpsilon)
    return area(a.clipped_bbox) > area(b.clipped_bbox);
  return a.original_index < b.original_index;
}
bool better_class(const TargetClassCandidateMatch& a,
                  const TargetClassCandidateMatch& b) {
  if (std::fabs(double(a.score) - b.score) > kEpsilon) return a.score > b.score;
  return a.candidate.class_id < b.candidate.class_id;
}
}  // namespace

TargetClassState make_unknown_target_class(TargetClassReason reason) {
  TargetClassState state;
  state.reason = reason;
  return state;
}

TargetClassState make_fixed_target_class(int class_id, bool detector_available) {
  if (class_id < 0) throw std::invalid_argument("fixed target class must be nonnegative");
  TargetClassState state;
  state.status = TargetClassStatus::kFixed;
  state.reason = TargetClassReason::kFixedOverride;
  state.target_class_id = class_id;
  state.target_class_allows_recovery = detector_available;
  return state;
}

TargetClassState establish_target_class(
    const BBox& roi, FrameSize frame_size,
    const std::vector<DetectorCandidate>& candidates,
    std::size_t class_count, const TargetClassMatchConfig& config) {
  BBox r{};
  if (frame_size.width <= 0 || frame_size.height <= 0 || !clip(roi, frame_size, r))
    throw std::invalid_argument("target class requires a valid clipped ROI and frame");
  if (!std::isfinite(config.min_roi_coverage) || config.min_roi_coverage <= 0 ||
      config.min_roi_coverage > 1 || !std::isfinite(config.max_center_ratio) ||
      config.max_center_ratio <= 0 || config.max_center_ratio > 2 ||
      !std::isfinite(config.ambiguity_margin) || config.ambiguity_margin < 0 ||
      config.ambiguity_margin > 1)
    throw std::invalid_argument("invalid target class match config");

  auto state = make_unknown_target_class(TargetClassReason::kNoMatch);
  state.candidates.reserve(candidates.size());
  std::map<int, std::size_t> class_winners;
  const double rx = double(r.x) + r.width / 2.0;
  const double ry = double(r.y) + r.height / 2.0;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    TargetClassCandidateMatch match;
    match.original_index = i;
    match.candidate = candidates[i];
    const auto& c = candidates[i];
    if (!clip(c.bbox, frame_size, match.clipped_bbox)) {
      match.rejection = TargetClassCandidateRejection::kInvalidBox;
    } else if (c.class_id < 0 || std::size_t(c.class_id) >= class_count) {
      match.rejection = TargetClassCandidateRejection::kInvalidClass;
    } else if (!std::isfinite(c.confidence)) {
      match.rejection = TargetClassCandidateRejection::kInvalidConfidence;
    } else {
      const auto& d = match.clipped_bbox;
      const double intersection =
          std::max(0.0, std::min(double(r.x) + r.width, double(d.x) + d.width) -
                            std::max(double(r.x), double(d.x))) *
          std::max(0.0, std::min(double(r.y) + r.height, double(d.y) + d.height) -
                            std::max(double(r.y), double(d.y)));
      const double coverage = intersection / area(r);
      const double iou = intersection / (area(r) + area(d) - intersection);
      const double ratio = std::hypot(rx - (double(d.x) + d.width / 2.0),
                                      ry - (double(d.y) + d.height / 2.0)) /
                           std::hypot(double(d.width), double(d.height));
      const bool inside = rx + kEpsilon >= d.x && ry + kEpsilon >= d.y &&
          rx <= double(d.x) + d.width + kEpsilon &&
          ry <= double(d.y) + d.height + kEpsilon;
      match.roi_coverage = float(coverage);
      match.iou = float(iou);
      match.center_ratio = float(ratio);
      match.score = float(0.55 * coverage +
          0.20 * std::clamp(1.0 - ratio / config.max_center_ratio, 0.0, 1.0) +
          0.10 * iou + 0.15 * c.confidence);
      match.eligible = (inside && ratio <= config.max_center_ratio + kEpsilon) ||
                      coverage + kEpsilon >= config.min_roi_coverage;
      if (!match.eligible) match.rejection = TargetClassCandidateRejection::kNoMatch;
    }
    state.candidates.push_back(match);
    if (match.eligible) {
      auto winner = class_winners.find(c.class_id);
      if (winner == class_winners.end()) class_winners.emplace(c.class_id, i);
      else if (better_same_class(match, state.candidates[winner->second])) winner->second = i;
    }
  }
  // Select without an epsilon-based std::sort comparator (not a strict ordering).
  std::optional<std::size_t> best, runner;
  for (const auto& entry : class_winners) {
    const auto index = entry.second;
    if (!best || better_class(state.candidates[index], state.candidates[*best])) {
      best = index;
    }
  }
  if (!best) return state;
  // Compare the final winner with the numerically strongest competing class.
  // Epsilon ties are not transitive: carrying the previous best as runner can
  // discard a closer competitor and incorrectly authorize recovery.
  // Exact runner-score ties retain the lower class ID from map iteration.
  for (const auto& entry : class_winners) {
    const auto index = entry.second;
    if (index != *best &&
        (!runner || state.candidates[index].score > state.candidates[*runner].score)) {
      runner = index;
    }
  }
  const auto& chosen = state.candidates[*best];
  state.best_class_id = chosen.candidate.class_id;
  state.best_score = chosen.score;
  if (runner) {
    state.runner_up_class_id = state.candidates[*runner].candidate.class_id;
    state.runner_up_score = state.candidates[*runner].score;
    const double gap = double(state.best_score) - state.runner_up_score;
    // Class-ID ordering is diagnostic only; tied classes never authorize recovery,
    // including when the caller sets the ambiguity margin to zero.
    if (std::fabs(gap) <= kEpsilon || gap + kEpsilon < config.ambiguity_margin) {
      state.reason = TargetClassReason::kAmbiguous;
      return state;
    }
  }
  state.status = TargetClassStatus::kEstablished;
  state.reason = TargetClassReason::kUniqueMatch;
  state.target_class_id = chosen.candidate.class_id;
  state.target_class_allows_recovery = true;
  state.selected_candidate = chosen;
  return state;
}

}  // namespace nanotrack
