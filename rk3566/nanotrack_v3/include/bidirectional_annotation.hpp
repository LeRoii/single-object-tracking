#pragma once

#include <optional>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "tracker_math.hpp"

namespace nanotrack {

enum class CandidateStatus {
  kAnchor,
  kBidirectionalCandidate,
  kNeedsHumanReview,
};

struct AnnotationAnchor {
  int frame_id = 0;
  BBox bbox{};
};

struct AgreementThresholds {
  float iou_min = 0.50F;
  float center_max = 0.35F;
};

struct DirectionalObservation {
  bool success = false;
  BBox bbox{};
};

struct BidirectionalCandidate {
  int frame_id = 0;
  DirectionalObservation forward;
  DirectionalObservation reverse;
  std::optional<BBox> consensus_bbox;
  float iou = 0.0F;
  float normalized_center_distance = 0.0F;
  CandidateStatus status = CandidateStatus::kNeedsHumanReview;
};

BidirectionalCandidate merge_bidirectional_observations(
    int frame_id,
    const DirectionalObservation& forward,
    const DirectionalObservation& reverse,
    const AgreementThresholds& thresholds);

std::vector<BidirectionalCandidate> track_segment_bidirectional(
    const std::vector<cv::Mat>& frames,
    const AnnotationAnchor& left,
    const AnnotationAnchor& right,
    const AgreementThresholds& thresholds);

std::vector<BidirectionalCandidate> stitch_candidate_segments(
    const std::vector<std::vector<BidirectionalCandidate>>& segments);

std::string to_string(CandidateStatus status);

}  // namespace nanotrack
