#pragma once

#include "controller_config.hpp"
#include "tracker_math.hpp"

#include <cstdint>

namespace nanotrack {

class MotionEstimator {
 public:
  explicit MotionEstimator(const ControllerConfig& config);

  void initialize(const BBox& bbox,
                  std::int64_t timestamp_ms,
                  int frame_width,
                  int frame_height);
  BBox predict(std::int64_t timestamp_ms);
  void correct(const BBox& observation, std::int64_t timestamp_ms);
  void freeze_scale(float width, float height);
  const BBox& current_bbox() const { return current_bbox_; }
  bool initialized() const { return initialized_; }
  bool scale_frozen() const { return scale_frozen_; }

 private:
  BBox state_to_bbox() const;
  void set_state_from_bbox(const BBox& bbox);
  void clamp_state_to_frame();

  ControllerConfig config_;
  BBox current_bbox_{};
  float center_x_ = 0.0F;
  float center_y_ = 0.0F;
  float width_ = 0.0F;
  float height_ = 0.0F;
  float velocity_x_ = 0.0F;
  float velocity_y_ = 0.0F;
  float velocity_width_ = 0.0F;
  float velocity_height_ = 0.0F;
  std::int64_t last_timestamp_ms_ = 0;
  float last_dt_seconds_ = 0.0F;
  int frame_width_ = 0;
  int frame_height_ = 0;
  bool initialized_ = false;
  bool correction_allowed_ = false;
  bool scale_frozen_ = false;
};

}  // namespace nanotrack
