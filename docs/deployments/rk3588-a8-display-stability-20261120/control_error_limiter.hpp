#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Limits the position error sent to the host. Never feed this result back to
// the tracker, search predictor, or display path.
class ControlErrorLimiter {
 public:
  struct Output { int x; int y; bool limited; };

  void reset() noexcept { x_ = y_ = 0.0F; last_ms_ = -1; }

  Output apply(int x, int y, int half_width, int half_height,
               std::int64_t now_ms, bool available) noexcept {
    if (!available) {
      reset();
      return {0, 0, x != 0 || y != 0};
    }
    const float max_x = 0.10F * half_width;
    const float max_y = 0.10F * half_height;
    const float target_x = std::clamp(static_cast<float>(x), -max_x, max_x);
    const float target_y = std::clamp(static_cast<float>(y), -max_y, max_y);
    if (last_ms_ < 0 || now_ms <= last_ms_ || now_ms - last_ms_ > 200) {
      x_ = y_ = 0.0F;
      last_ms_ = now_ms;
    } else {
      const float dt = static_cast<float>(now_ms - last_ms_);
      x_ = advance(x_, target_x, max_x * dt / 300.0F);
      y_ = advance(y_, target_y, max_y * dt / 300.0F);
      last_ms_ = now_ms;
    }
    const int out_x = static_cast<int>(std::lround(x_));
    const int out_y = static_cast<int>(std::lround(y_));
    return {out_x, out_y, out_x != x || out_y != y};
  }

 private:
  static float advance(float previous, float target, float step) noexcept {
    if ((previous > 0 && target < 0) || (previous < 0 && target > 0))
      previous = 0.0F;
    if (std::abs(target) <= std::abs(previous)) return target;
    return previous + std::copysign(std::min(step, std::abs(target - previous)), target);
  }
  float x_ = 0.0F;
  float y_ = 0.0F;
  std::int64_t last_ms_ = -1;
};
