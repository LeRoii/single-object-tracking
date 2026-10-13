#pragma once

#include "detector_adapter.hpp"
#include "long_term_controller.hpp"
#include "short_term_tracker.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nanotrack {

struct RuntimeFrameResult {
  SearchPlan plan{};
  std::vector<TrackerObservation> observations;
  ControllerOutput output{};
  bool detector_error = false;
  std::string detector_error_message;
  float tracker_ms = 0.0F;
  float detector_ms = 0.0F;
  float runtime_ms = 0.0F;
};

class LongTermRuntime {
 public:
  LongTermRuntime(const ControllerConfig& config,
                  ShortTermTracker& tracker,
                  DetectorAdapter& detector);

  void initialize(const ImageView& image,
                  const BBox& initial_bbox,
                  std::uint64_t frame_id,
                  std::int64_t timestamp_ms);
  RuntimeFrameResult process_frame(const ImageView& image,
                                   std::uint64_t frame_id,
                                   std::int64_t timestamp_ms);

  const LongTermController& controller() const { return controller_; }

 private:
  static void validate_image(const ImageView& image);

  ShortTermTracker& tracker_;
  DetectorAdapter& detector_;
  LongTermController controller_;
  bool initialized_ = false;
};

}  // namespace nanotrack
