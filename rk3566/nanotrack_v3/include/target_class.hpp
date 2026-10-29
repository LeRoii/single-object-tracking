#pragma once

#include "long_term_types.hpp"

namespace nanotrack {

// Invalid frame, ROI (including an empty clipped ROI), or config throws
// std::invalid_argument. Invalid candidates remain in the diagnostics.
TargetClassState establish_target_class(
    const BBox& roi, FrameSize frame_size,
    const std::vector<DetectorCandidate>& candidates,
    std::size_t class_count, const TargetClassMatchConfig& config);
TargetClassState make_fixed_target_class(int class_id, bool detector_available);
TargetClassState make_unknown_target_class(TargetClassReason reason);

}  // namespace nanotrack
