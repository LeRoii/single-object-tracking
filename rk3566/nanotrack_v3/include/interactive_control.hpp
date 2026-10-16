#pragma once

#include <optional>

namespace nanotrack {

enum class InteractiveCommand {
  kNone,
  kStep,
  kTogglePlayback,
  kSelectRoi,
  kQuit,
};

enum class RoiMouseEvent { kDown, kMove, kUp };

struct InteractiveRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

class InteractiveRoiSelection {
 public:
  InteractiveRoiSelection(int frame_width, int frame_height);

  void handle_mouse(RoiMouseEvent event, int x, int y);
  void handle_key(int key);
  std::optional<InteractiveRect> current_rect() const;
  const std::optional<InteractiveRect>& confirmed_rect() const {
    return confirmed_rect_;
  }
  bool cancelled() const { return cancelled_; }
  bool finished() const { return cancelled_ || confirmed_rect_.has_value(); }

 private:
  int clamp_x(int x) const;
  int clamp_y(int y) const;

  int frame_width_ = 0;
  int frame_height_ = 0;
  int anchor_x_ = 0;
  int anchor_y_ = 0;
  int cursor_x_ = 0;
  int cursor_y_ = 0;
  bool has_anchor_ = false;
  bool dragging_ = false;
  bool cancelled_ = false;
  std::optional<InteractiveRect> confirmed_rect_;
};

InteractiveCommand decode_interactive_key(int key);
bool interactive_window_visible(double property_value);

class InteractivePlayback {
 public:
  bool handle_key(int key);
  void pause() { playing_ = false; }
  bool playing() const { return playing_; }
  bool selection_requested() const { return selection_requested_; }
  bool take_selection_request();
  bool quit_requested() const { return quit_requested_; }

 private:
  bool playing_ = false;
  bool selection_requested_ = false;
  bool quit_requested_ = false;
};

}  // namespace nanotrack
