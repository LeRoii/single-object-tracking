#pragma once

#include "long_term_runtime.hpp"

#include <string>
#include <vector>

namespace nanotrack {

enum class DebugBoxKind {
  kMotionPrediction,
  kDetectorCandidate,
  kDetectorProbe,
  kNanoTrackAnchor,
  kNanoTrackRecent,
  kAcceptedControl,
};

struct DebugBoxDescriptor {
  DebugBoxKind kind = DebugBoxKind::kMotionPrediction;
  BBox bbox{};
  std::string label;
  int line_thickness = 2;
};

struct InteractiveDebugOverlay {
  std::vector<DebugBoxDescriptor> boxes;
  std::string status_label = "REJECTED NO_CTRL";
  bool control_valid = false;
};

InteractiveDebugOverlay make_interactive_debug_overlay(
    const RuntimeFrameResult& result);

}  // namespace nanotrack
