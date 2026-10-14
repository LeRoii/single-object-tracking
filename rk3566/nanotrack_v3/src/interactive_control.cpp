#include "interactive_control.hpp"

#include <algorithm>
#include <cmath>

namespace nanotrack {

InteractiveCommand decode_interactive_key(int key) {
  switch (key) {
    case 'n':
    case 'N':
    case 0xff53:
    case 2555904: return InteractiveCommand::kStep;
    case ' ': return InteractiveCommand::kTogglePlayback;
    case 'r':
    case 'R': return InteractiveCommand::kSelectRoi;
    case 'q':
    case 'Q':
    case 27: return InteractiveCommand::kQuit;
    default: return InteractiveCommand::kNone;
  }
}

int interactive_wait_delay_ms(bool playing, double source_fps) {
  if (!playing) return 0;
  const double fps = std::isfinite(source_fps) && source_fps > 0.0
                         ? source_fps
                         : 30.0;
  return std::max(1, static_cast<int>(std::lround(1000.0 / fps)));
}

bool InteractivePlayback::handle_key(int key) {
  switch (decode_interactive_key(key)) {
    case InteractiveCommand::kStep:
      playing_ = false;
      return true;
    case InteractiveCommand::kTogglePlayback:
      playing_ = !playing_;
      return false;
    case InteractiveCommand::kSelectRoi:
      playing_ = false;
      selection_requested_ = true;
      return false;
    case InteractiveCommand::kQuit:
      playing_ = false;
      quit_requested_ = true;
      return false;
    case InteractiveCommand::kNone: return playing_;
  }
  return false;
}

bool InteractivePlayback::take_selection_request() {
  const bool requested = selection_requested_;
  selection_requested_ = false;
  return requested;
}

}  // namespace nanotrack
