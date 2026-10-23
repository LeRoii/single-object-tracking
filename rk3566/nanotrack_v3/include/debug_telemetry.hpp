#pragma once

#include "long_term_runtime.hpp"

#include <cstddef>
#include <vector>
#include <string>

namespace nanotrack {

class DebugTelemetry {
 public:
  explicit DebugTelemetry(std::size_t heartbeat_interval = 30);

  void reset();
  std::vector<std::string> frame_events(const RuntimeFrameResult& result,
                                        float pipeline_ms,
                                        bool force_heartbeat);

 private:
  std::size_t heartbeat_interval_ = 30;
  std::size_t observed_frames_ = 0;
  bool has_previous_state_ = false;
  TrackingMode previous_mode_ = TrackingMode::kTracking;
  bool previous_control_valid_ = false;
};

}  // namespace nanotrack
