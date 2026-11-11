#include "bidirectional_annotation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <opencv2/tracking.hpp>

namespace nanotrack {
namespace {

void validate_thresholds(const AgreementThresholds& thresholds) {
  if (!std::isfinite(thresholds.iou_min) || thresholds.iou_min < 0.0F ||
      thresholds.iou_min > 1.0F || !std::isfinite(thresholds.center_max) ||
      thresholds.center_max < 0.0F) {
    throw std::invalid_argument("invalid bidirectional agreement thresholds");
  }
}

bool valid_bbox(const BBox& bbox) {
  return std::isfinite(bbox.x) && std::isfinite(bbox.y) &&
         std::isfinite(bbox.width) && std::isfinite(bbox.height) &&
         bbox.x >= 0.0F && bbox.y >= 0.0F && bbox.width > 0.0F &&
         bbox.height > 0.0F;
}

float intersection_over_union(const BBox& lhs, const BBox& rhs) {
  const float x1 = std::max(lhs.x, rhs.x);
  const float y1 = std::max(lhs.y, rhs.y);
  const float x2 = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
  const float y2 = std::min(lhs.y + lhs.height, rhs.y + rhs.height);
  const float intersection = std::max(0.0F, x2 - x1) *
                             std::max(0.0F, y2 - y1);
  const float union_area = lhs.width * lhs.height + rhs.width * rhs.height -
                           intersection;
  return union_area > 0.0F ? intersection / union_area : 0.0F;
}

float center_distance(const BBox& lhs, const BBox& rhs) {
  const float dx = (lhs.x + 0.5F * lhs.width) -
                   (rhs.x + 0.5F * rhs.width);
  const float dy = (lhs.y + 0.5F * lhs.height) -
                   (rhs.y + 0.5F * rhs.height);
  const float reference_area = std::max(1.0F, 0.5F *
      (lhs.width * lhs.height + rhs.width * rhs.height));
  return std::hypot(dx, dy) / std::sqrt(reference_area);
}

cv::Rect to_rect(const BBox& bbox, const cv::Size& size) {
  if (!valid_bbox(bbox)) throw std::invalid_argument("invalid annotation bbox");
  const int x = std::clamp(static_cast<int>(std::lround(bbox.x)), 0,
                           std::max(0, size.width - 1));
  const int y = std::clamp(static_cast<int>(std::lround(bbox.y)), 0,
                           std::max(0, size.height - 1));
  const int width = std::clamp(static_cast<int>(std::lround(bbox.width)), 1,
                               size.width - x);
  const int height = std::clamp(static_cast<int>(std::lround(bbox.height)), 1,
                                size.height - y);
  return {x, y, width, height};
}

BBox from_rect(const cv::Rect& rect) {
  return {static_cast<float>(rect.x), static_cast<float>(rect.y),
          static_cast<float>(rect.width), static_cast<float>(rect.height)};
}

}  // namespace

BidirectionalCandidate merge_bidirectional_observations(
    int frame_id,
    const DirectionalObservation& forward,
    const DirectionalObservation& reverse,
    const AgreementThresholds& thresholds) {
  validate_thresholds(thresholds);
  BidirectionalCandidate result;
  result.frame_id = frame_id;
  result.forward = forward;
  result.reverse = reverse;
  if (!forward.success || !reverse.success || !valid_bbox(forward.bbox) ||
      !valid_bbox(reverse.bbox)) {
    return result;
  }
  result.iou = intersection_over_union(forward.bbox, reverse.bbox);
  result.normalized_center_distance = center_distance(forward.bbox, reverse.bbox);
  if (result.iou < thresholds.iou_min ||
      result.normalized_center_distance > thresholds.center_max) {
    return result;
  }
  result.consensus_bbox = BBox{
      0.5F * (forward.bbox.x + reverse.bbox.x),
      0.5F * (forward.bbox.y + reverse.bbox.y),
      0.5F * (forward.bbox.width + reverse.bbox.width),
      0.5F * (forward.bbox.height + reverse.bbox.height)};
  result.status = CandidateStatus::kBidirectionalCandidate;
  return result;
}

std::vector<BidirectionalCandidate> track_segment_bidirectional(
    const std::vector<cv::Mat>& frames,
    const AnnotationAnchor& left,
    const AnnotationAnchor& right,
    const AgreementThresholds& thresholds) {
  validate_thresholds(thresholds);
  if (left.frame_id >= right.frame_id || frames.size() !=
      static_cast<std::size_t>(right.frame_id - left.frame_id + 1)) {
    throw std::invalid_argument("annotation segment does not match anchors");
  }
  if (frames.empty() || frames.front().empty()) {
    throw std::invalid_argument("annotation segment has no frames");
  }
  const cv::Size size = frames.front().size();
  for (const cv::Mat& frame : frames) {
    if (frame.empty() || frame.size() != size) {
      throw std::invalid_argument("annotation segment frame geometry changed");
    }
  }

  std::vector<DirectionalObservation> forward(frames.size());
  std::vector<DirectionalObservation> reverse(frames.size());
  cv::Ptr<cv::TrackerCSRT> forward_tracker = cv::TrackerCSRT::create();
  cv::Rect forward_rect = to_rect(left.bbox, size);
  forward_tracker->init(frames.front(), forward_rect);
  forward.front() = {true, left.bbox};
  for (std::size_t index = 1; index < frames.size(); ++index) {
    const bool success = forward_tracker->update(frames[index], forward_rect);
    forward[index] = {success, success ? from_rect(forward_rect) : BBox{}};
  }

  cv::Ptr<cv::TrackerCSRT> reverse_tracker = cv::TrackerCSRT::create();
  cv::Rect reverse_rect = to_rect(right.bbox, size);
  reverse_tracker->init(frames.back(), reverse_rect);
  reverse.back() = {true, right.bbox};
  for (std::size_t index = frames.size() - 1; index > 0; --index) {
    const bool success = reverse_tracker->update(frames[index - 1], reverse_rect);
    reverse[index - 1] = {success, success ? from_rect(reverse_rect) : BBox{}};
  }

  std::vector<BidirectionalCandidate> results;
  results.reserve(frames.size());
  for (std::size_t index = 0; index < frames.size(); ++index) {
    auto result = merge_bidirectional_observations(
        left.frame_id + static_cast<int>(index), forward[index], reverse[index],
        thresholds);
    if (index == 0 || index + 1 == frames.size()) {
      const BBox anchor_bbox = index == 0 ? left.bbox : right.bbox;
      result.forward = {true, anchor_bbox};
      result.reverse = {true, anchor_bbox};
      result.consensus_bbox = anchor_bbox;
      result.iou = 1.0F;
      result.normalized_center_distance = 0.0F;
      result.status = CandidateStatus::kAnchor;
    }
    results.push_back(result);
  }
  return results;
}

std::vector<BidirectionalCandidate> stitch_candidate_segments(
    const std::vector<std::vector<BidirectionalCandidate>>& segments) {
  std::vector<BidirectionalCandidate> result;
  for (const auto& segment : segments) {
    if (segment.empty()) continue;
    std::size_t start = 0;
    if (!result.empty()) {
      if (segment.front().frame_id != result.back().frame_id) {
        throw std::invalid_argument("annotation segments do not share one anchor");
      }
      start = 1;
    }
    for (std::size_t index = start; index < segment.size(); ++index) {
      if (!result.empty() && segment[index].frame_id != result.back().frame_id + 1) {
        throw std::invalid_argument("annotation segment frame ids are not continuous");
      }
      result.push_back(segment[index]);
    }
  }
  return result;
}

std::string to_string(CandidateStatus status) {
  switch (status) {
    case CandidateStatus::kAnchor: return "anchor";
    case CandidateStatus::kBidirectionalCandidate:
      return "bidirectional_candidate";
    case CandidateStatus::kNeedsHumanReview: return "needs_human_review";
  }
  throw std::invalid_argument("unknown candidate status");
}

}  // namespace nanotrack
