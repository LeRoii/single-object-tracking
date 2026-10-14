#pragma once

#include "rknn_model.hpp"
#include "short_term_tracker.hpp"
#include "tracker_math.hpp"

#include <opencv2/core.hpp>

#include <memory>
#include <string>
#include <vector>

namespace nanotrack {

class NanoTrackV3 final : public ShortTermTracker {
 public:
  explicit NanoTrackV3(const std::string& model_dir);
  void initialize(const cv::Mat& frame, const BBox& bbox);

  void initialize(const ImageView& image, const BBox& bbox) override;
  std::vector<TrackerObservation> infer(const ImageView& image,
                                        const SearchPlan& plan) override;
  void commit_bbox(const BBox& bbox) override;
  void stage_recent(const ImageView& image, const BBox& bbox) override;
  void commit_recent() override;
  void rollback_recent() override;
  const std::string& api_version() const { return template_model_->api_version(); }
  const std::string& driver_version() const { return template_model_->driver_version(); }

 private:
  cv::Mat crop(const cv::Mat& frame, float center_x, float center_y,
               int output_size, int original_size) const;
  void validate_model_interfaces();
  std::vector<float> extract_template_feature(const cv::Mat& frame,
                                              const BBox& bbox);
  std::optional<DecodeResult> run_head(
      const std::vector<float>& template_feature,
      const std::vector<float>& search_feature,
      const TrackerState& reference_state,
      float scale_z);
  cv::Mat mat_view(const ImageView& image) const;

  std::unique_ptr<RknnModel> template_model_;
  std::unique_ptr<RknnModel> search_model_;
  std::unique_ptr<RknnModel> head_model_;
  std::vector<float> anchor_feature_;
  std::vector<float> recent_feature_;
  std::vector<float> recent_backup_feature_;
  TrackerState state_{};
  TrackerConfig config_{};
  cv::Scalar channel_average_{};
  int frame_width_ = 0;
  int frame_height_ = 0;
  std::uint64_t search_token_ = 0;
  bool recent_matches_anchor_ = true;
  bool recent_backup_matches_anchor_ = true;
  bool recent_staged_ = false;
  bool initialized_ = false;
};

}  // namespace nanotrack
