#include "bidirectional_annotation.hpp"

#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

bool near(float actual, float expected, float tolerance = 1e-4F) {
  return std::fabs(actual - expected) <= tolerance;
}

nanotrack::BidirectionalCandidate candidate(int frame_id) {
  nanotrack::BidirectionalCandidate value;
  value.frame_id = frame_id;
  value.status = nanotrack::CandidateStatus::kBidirectionalCandidate;
  value.consensus_bbox = nanotrack::BBox{static_cast<float>(frame_id), 5.0F, 10.0F, 8.0F};
  return value;
}

void test_merge_accepts_only_agreed_real_observations() {
  const nanotrack::AgreementThresholds thresholds{0.50F, 0.35F};
  const auto agreed = nanotrack::merge_bidirectional_observations(
      7,
      nanotrack::DirectionalObservation{true, {10.0F, 20.0F, 40.0F, 20.0F}},
      nanotrack::DirectionalObservation{true, {12.0F, 20.0F, 40.0F, 20.0F}},
      thresholds);
  expect_true(agreed.status == nanotrack::CandidateStatus::kBidirectionalCandidate,
              "agreed tracks produce a candidate");
  expect_true(agreed.consensus_bbox.has_value() &&
                  near(agreed.consensus_bbox->x, 11.0F) &&
                  near(agreed.consensus_bbox->width, 40.0F),
              "candidate is the coordinate mean of independent observations");
  expect_true(agreed.iou > 0.90F && agreed.normalized_center_distance < 0.08F,
              "agreement diagnostics use geometry rather than tracker status alone");

  const auto disagreement = nanotrack::merge_bidirectional_observations(
      8,
      nanotrack::DirectionalObservation{true, {10.0F, 20.0F, 40.0F, 20.0F}},
      nanotrack::DirectionalObservation{true, {100.0F, 20.0F, 40.0F, 20.0F}},
      thresholds);
  expect_true(disagreement.status == nanotrack::CandidateStatus::kNeedsHumanReview &&
                  !disagreement.consensus_bbox,
              "identity-scale disagreement is never silently averaged");

  const auto failed = nanotrack::merge_bidirectional_observations(
      9,
      nanotrack::DirectionalObservation{false, {0.0F, 0.0F, 0.0F, 0.0F}},
      nanotrack::DirectionalObservation{true, {10.0F, 20.0F, 40.0F, 20.0F}},
      thresholds);
  expect_true(failed.status == nanotrack::CandidateStatus::kNeedsHumanReview &&
                  !failed.consensus_bbox,
              "one tracker failure cannot become a candidate");
}

void test_stitch_emits_shared_anchor_once() {
  const auto stitched = nanotrack::stitch_candidate_segments(
      {{candidate(0), candidate(1), candidate(2)},
       {candidate(2), candidate(3), candidate(4)}});
  expect_true(stitched.size() == 5, "shared segment anchor is emitted once");
  for (int frame_id = 0; frame_id <= 4; ++frame_id) {
    expect_true(stitched[static_cast<std::size_t>(frame_id)].frame_id == frame_id,
                "stitched frame ids are continuous and ordered");
  }
}

void test_csrt_tracks_short_real_sequence_in_both_directions() {
  std::vector<cv::Mat> frames;
  for (int frame_id = 0; frame_id <= 6; ++frame_id) {
    cv::Mat frame(120, 180, CV_8UC3, cv::Scalar(20, 20, 20));
    const cv::Rect object(30 + frame_id * 3, 45, 28, 22);
    cv::rectangle(frame, object, cv::Scalar(240, 240, 240), cv::FILLED);
    cv::line(frame, object.tl(), object.br(), cv::Scalar(20, 20, 20), 2);
    cv::circle(frame, cv::Point(object.x + 8, object.y + 7), 3,
               cv::Scalar(10, 10, 10), cv::FILLED);
    frames.push_back(frame);
  }
  const auto tracked = nanotrack::track_segment_bidirectional(
      frames,
      nanotrack::AnnotationAnchor{0, {30.0F, 45.0F, 28.0F, 22.0F}},
      nanotrack::AnnotationAnchor{6, {48.0F, 45.0F, 28.0F, 22.0F}},
      nanotrack::AgreementThresholds{0.35F, 0.50F});
  expect_true(tracked.size() == 7, "real CSRT segment covers both anchors");
  expect_true(tracked.front().frame_id == 0 && tracked.back().frame_id == 6,
              "real CSRT output preserves endpoint frame ids");
  expect_true(tracked.front().status == nanotrack::CandidateStatus::kAnchor &&
                  tracked.back().status == nanotrack::CandidateStatus::kAnchor,
              "explicit anchors remain anchors");
  for (std::size_t index = 1; index + 1 < tracked.size(); ++index) {
    expect_true(tracked[index].forward.success && tracked[index].reverse.success,
                "both real trackers produced an observation");
  }
}

}  // namespace

int main() {
  test_merge_accepts_only_agreed_real_observations();
  test_stitch_emits_shared_anchor_once();
  test_csrt_tracks_short_real_sequence_in_both_directions();
  if (failures != 0) {
    std::cerr << failures << " bidirectional annotation assertions failed\n";
    return 1;
  }
  std::cout << "bidirectional annotation tests passed\n";
  return 0;
}
