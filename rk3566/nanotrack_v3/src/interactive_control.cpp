#include "interactive_control.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nanotrack {
namespace {

int normalize_highgui_key(int key) {
  if (key <= 0xffff) return key;
  const int keyval = key & 0xffff;
  if ((keyval > 0 && keyval <= 0xff) || keyval >= 0xff00) return keyval;
  return key;
}

}  // namespace

InteractiveRoiSelection::InteractiveRoiSelection(int frame_width,
                                                 int frame_height)
    : frame_width_(frame_width), frame_height_(frame_height) {
  if (frame_width_ <= 0 || frame_height_ <= 0) {
    throw std::invalid_argument("ROI frame dimensions must be positive");
  }
}

int InteractiveRoiSelection::clamp_x(int x) const {
  return std::clamp(x, 0, frame_width_ - 1);
}

int InteractiveRoiSelection::clamp_y(int y) const {
  return std::clamp(y, 0, frame_height_ - 1);
}

void InteractiveRoiSelection::handle_mouse(RoiMouseEvent event, int x, int y) {
  if (finished()) return;
  const int clipped_x = clamp_x(x);
  const int clipped_y = clamp_y(y);
  switch (event) {
    case RoiMouseEvent::kDown:
      anchor_x_ = clipped_x;
      anchor_y_ = clipped_y;
      cursor_x_ = clipped_x;
      cursor_y_ = clipped_y;
      has_anchor_ = true;
      dragging_ = true;
      break;
    case RoiMouseEvent::kMove:
      if (dragging_) {
        cursor_x_ = clipped_x;
        cursor_y_ = clipped_y;
      }
      break;
    case RoiMouseEvent::kUp:
      if (dragging_) {
        cursor_x_ = clipped_x;
        cursor_y_ = clipped_y;
        dragging_ = false;
      }
      break;
  }
}

std::optional<InteractiveRect> InteractiveRoiSelection::current_rect() const {
  if (!has_anchor_) return std::nullopt;
  const int left = std::min(anchor_x_, cursor_x_);
  const int top = std::min(anchor_y_, cursor_y_);
  const int width = std::abs(cursor_x_ - anchor_x_);
  const int height = std::abs(cursor_y_ - anchor_y_);
  if (width <= 0 || height <= 0) return std::nullopt;
  return InteractiveRect{left, top, width, height};
}

void InteractiveRoiSelection::handle_key(int key) {
  if (finished()) return;
  key = normalize_highgui_key(key);
  if (key == 27 || key == 'c' || key == 'C') {
    cancelled_ = true;
    confirmed_rect_.reset();
    return;
  }
  if (key == 10 || key == 13 || key == ' ') {
    confirmed_rect_ = current_rect();
  }
}

InteractiveCommand decode_interactive_key(int key) {
  key = normalize_highgui_key(key);
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

const char* to_string(InteractiveCommand command) {
  switch (command) {
    case InteractiveCommand::kNone: return "none";
    case InteractiveCommand::kStep: return "step";
    case InteractiveCommand::kTogglePlayback: return "play_pause";
    case InteractiveCommand::kSelectRoi: return "select_roi";
    case InteractiveCommand::kQuit: return "quit";
  }
  return "unknown";
}

bool interactive_window_visible(double property_value) {
  return std::isfinite(property_value) && property_value != 0.0;
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
