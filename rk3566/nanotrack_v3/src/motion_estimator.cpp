#include "motion_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nanotrack {

MotionEstimator::MotionEstimator(const ControllerConfig& config) : config_(config) {
  validate_controller_config(config_);
}

void MotionEstimator::initialize(const BBox& bbox,
                                 std::int64_t timestamp_ms,
                                 int frame_width,
                                 int frame_height) {
  validate_bbox(bbox, frame_width, frame_height);
  frame_width_ = frame_width;
  frame_height_ = frame_height;
  set_state_from_bbox(bbox);
  velocity_x_ = velocity_y_ = velocity_width_ = velocity_height_ = 0.0F;
  acceleration_x_ = acceleration_y_ = 0.0F;
  last_timestamp_ms_ = timestamp_ms;
  last_dt_seconds_ = 0.0F;
  initialized_ = true;
  correction_allowed_ = false;
  scale_frozen_ = false;
  correction_count_ = 0;
  search_start_ms_.reset();
}

BBox MotionEstimator::predict(std::int64_t timestamp_ms) {
  if (!initialized_) throw std::logic_error("motion estimator is not initialized");
  if (timestamp_ms <= last_timestamp_ms_) {
    throw std::invalid_argument("motion timestamp must be strictly increasing");
  }
  const std::int64_t elapsed_ms = timestamp_ms - last_timestamp_ms_;
  const std::int64_t capped_ms = std::min(elapsed_ms, config_.motion_max_dt_ms);
  last_dt_seconds_ = static_cast<float>(capped_ms) / 1000.0F;
  clamp_derivatives();
  center_x_ += velocity_x_ * last_dt_seconds_ +
               0.5F * acceleration_x_ * last_dt_seconds_ * last_dt_seconds_;
  center_y_ += velocity_y_ * last_dt_seconds_ +
               0.5F * acceleration_y_ * last_dt_seconds_ * last_dt_seconds_;
  velocity_x_ += acceleration_x_ * last_dt_seconds_;
  velocity_y_ += acceleration_y_ * last_dt_seconds_;
  if (!scale_frozen_) {
    width_ = std::max(1.0F, width_ + velocity_width_ * last_dt_seconds_);
    height_ = std::max(1.0F, height_ + velocity_height_ * last_dt_seconds_);
  }
  clamp_state_to_frame();
  // Prediction can shrink the box and therefore its allowed scale velocity.
  clamp_derivatives();
  last_timestamp_ms_ = timestamp_ms;
  correction_allowed_ = true;
  return current_bbox_;
}

void MotionEstimator::correct(const BBox& observation, std::int64_t timestamp_ms) {
  if (!initialized_ || !correction_allowed_) {
    throw std::logic_error("motion correction requires a preceding prediction");
  }
  if (timestamp_ms != last_timestamp_ms_) {
    throw std::invalid_argument("motion correction timestamp must match prediction");
  }
  validate_bbox(observation, frame_width_, frame_height_);
  const float observed_center_x = observation.x + 0.5F * observation.width;
  const float observed_center_y = observation.y + 0.5F * observation.height;
  const float residual_x = observed_center_x - center_x_;
  const float residual_y = observed_center_y - center_y_;
  const float residual_width = observation.width - width_;
  const float residual_height = observation.height - height_;
  center_x_ += config_.motion_alpha * residual_x;
  center_y_ += config_.motion_alpha * residual_y;
  if (!scale_frozen_) {
    width_ = std::max(1.0F, width_ + config_.motion_alpha * residual_width);
    height_ = std::max(1.0F, height_ + config_.motion_alpha * residual_height);
  }
  if (last_dt_seconds_ > 0.0F) {
    const float gain = config_.motion_beta / last_dt_seconds_;
    const float previous_velocity_x = velocity_x_;
    const float previous_velocity_y = velocity_y_;
    velocity_x_ += gain * residual_x;
    velocity_y_ += gain * residual_y;
    if (!scale_frozen_) {
      velocity_width_ += gain * residual_width;
      velocity_height_ += gain * residual_height;
    }
    clamp_derivatives();
    if (correction_count_ > 0) {
      acceleration_x_ =
          (velocity_x_ - previous_velocity_x) / last_dt_seconds_;
      acceleration_y_ =
          (velocity_y_ - previous_velocity_y) / last_dt_seconds_;
    } else {
      acceleration_x_ = 0.0F;
      acceleration_y_ = 0.0F;
    }
  }
  if (scale_frozen_) {
    velocity_width_ = 0.0F;
    velocity_height_ = 0.0F;
  }
  clamp_derivatives();
  clamp_state_to_frame();
  correction_allowed_ = false;
  ++correction_count_;
}

void MotionEstimator::freeze_scale(float width, float height) {
  if (!initialized_) {
    throw std::logic_error("motion estimator is not initialized");
  }
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0F ||
      height <= 0.0F) {
    throw std::invalid_argument(
        "frozen motion scale dimensions must be finite and positive");
  }
  width_ = width;
  height_ = height;
  velocity_width_ = 0.0F;
  velocity_height_ = 0.0F;
  scale_frozen_ = true;
  const float preserved_velocity_x = velocity_x_;
  const float preserved_velocity_y = velocity_y_;
  clamp_state_to_frame();
  velocity_x_ = preserved_velocity_x;
  velocity_y_ = preserved_velocity_y;
  clamp_derivatives();
}

void MotionEstimator::freeze_open_loop_dynamics() noexcept {
  acceleration_x_ = 0.0F;
  acceleration_y_ = 0.0F;
  velocity_width_ = 0.0F;
  velocity_height_ = 0.0F;
}

void MotionEstimator::begin_search(std::int64_t timestamp_ms) {
  if (!initialized_) throw std::logic_error("motion estimator is not initialized");
  if (timestamp_ms != last_timestamp_ms_) {
    throw std::invalid_argument(
        "motion search must start at the latest prediction timestamp");
  }
  if (!search_start_ms_) search_start_ms_ = timestamp_ms;
}

void MotionEstimator::begin_search(
    std::int64_t timestamp_ms, const MotionSnapshot& trusted_trend) {
  begin_search(timestamp_ms);
  if (!std::isfinite(trusted_trend.velocity_x) ||
      !std::isfinite(trusted_trend.velocity_y)) {
    throw std::invalid_argument("trusted search velocity must be finite");
  }
  velocity_x_ = trusted_trend.velocity_x;
  velocity_y_ = trusted_trend.velocity_y;
  acceleration_x_ = 0.0F;
  acceleration_y_ = 0.0F;
  clamp_derivatives();
}

void MotionEstimator::end_search() noexcept { search_start_ms_.reset(); }

bool MotionEstimator::prediction_confident(std::int64_t timestamp_ms) const {
  if (!initialized_ || !search_start_ms_) return false;
  if (timestamp_ms < *search_start_ms_) {
    throw std::invalid_argument("motion search timestamp moved backwards");
  }
  return timestamp_ms - *search_start_ms_ <=
         config_.motion_search_confidence_ms;
}

MotionSnapshot MotionEstimator::snapshot(std::int64_t timestamp_ms) const {
  if (!initialized_) throw std::logic_error("motion estimator is not initialized");
  MotionSnapshot result;
  result.bbox = current_bbox_;
  result.velocity_x = velocity_x_;
  result.velocity_y = velocity_y_;
  result.acceleration_x = acceleration_x_;
  result.acceleration_y = acceleration_y_;
  result.scale_velocity_width = velocity_width_;
  result.scale_velocity_height = velocity_height_;
  if (search_start_ms_) {
    if (timestamp_ms < *search_start_ms_) {
      throw std::invalid_argument("motion snapshot timestamp moved backwards");
    }
    result.search_age_ms = timestamp_ms - *search_start_ms_;
    result.prediction_confident = prediction_confident(timestamp_ms);
  }
  return result;
}

void MotionEstimator::clamp_derivatives() {
  const float diagonal = std::hypot(static_cast<float>(frame_width_),
                                    static_cast<float>(frame_height_));
  const auto clamp_vector = [](float& x, float& y, float limit) {
    const float magnitude = std::hypot(x, y);
    if (!std::isfinite(magnitude)) {
      throw std::runtime_error("motion derivatives became non-finite");
    }
    if (magnitude > limit && magnitude > 0.0F) {
      const float scale = limit / magnitude;
      x *= scale;
      y *= scale;
    }
  };
  clamp_vector(velocity_x_, velocity_y_,
               config_.motion_max_speed_diagonals_per_s * diagonal);
  clamp_vector(acceleration_x_, acceleration_y_,
               config_.motion_max_acceleration_diagonals_per_s2 * diagonal);
  if (!std::isfinite(velocity_width_) || !std::isfinite(velocity_height_)) {
    throw std::runtime_error("motion scale derivatives became non-finite");
  }
  const float width_limit =
      std::max(1.0F, width_) * config_.motion_scale_rate_limit_per_s;
  const float height_limit =
      std::max(1.0F, height_) * config_.motion_scale_rate_limit_per_s;
  velocity_width_ = std::clamp(velocity_width_, -width_limit, width_limit);
  velocity_height_ = std::clamp(velocity_height_, -height_limit, height_limit);
}

BBox MotionEstimator::state_to_bbox() const {
  return BBox{center_x_ - 0.5F * width_, center_y_ - 0.5F * height_, width_, height_};
}

void MotionEstimator::set_state_from_bbox(const BBox& bbox) {
  current_bbox_ = bbox;
  center_x_ = bbox.x + 0.5F * bbox.width;
  center_y_ = bbox.y + 0.5F * bbox.height;
  width_ = bbox.width;
  height_ = bbox.height;
}

void MotionEstimator::clamp_state_to_frame() {
  if (!std::isfinite(center_x_) || !std::isfinite(center_y_) ||
      !std::isfinite(width_) || !std::isfinite(height_)) {
    throw std::runtime_error("motion state became non-finite");
  }

  const float clamped_width =
      std::clamp(width_, 1.0F, static_cast<float>(frame_width_));
  const float clamped_height =
      std::clamp(height_, 1.0F, static_cast<float>(frame_height_));
  if (clamped_width != width_) velocity_width_ = 0.0F;
  if (clamped_height != height_) velocity_height_ = 0.0F;
  width_ = clamped_width;
  height_ = clamped_height;

  const float half_width = 0.5F * width_;
  const float half_height = 0.5F * height_;
  const float clamped_center_x =
      std::clamp(center_x_, half_width,
                 static_cast<float>(frame_width_) - half_width);
  const float clamped_center_y =
      std::clamp(center_y_, half_height,
                 static_cast<float>(frame_height_) - half_height);
  if (clamped_center_x != center_x_) velocity_x_ = 0.0F;
  if (clamped_center_y != center_y_) velocity_y_ = 0.0F;
  center_x_ = clamped_center_x;
  center_y_ = clamped_center_y;
  current_bbox_ = clip_bbox(state_to_bbox(), frame_width_, frame_height_);
  if (current_bbox_.width <= 0.0F || current_bbox_.height <= 0.0F) {
    throw std::runtime_error("motion state cannot be projected into the frame");
  }
  set_state_from_bbox(current_bbox_);
}

}  // namespace nanotrack
