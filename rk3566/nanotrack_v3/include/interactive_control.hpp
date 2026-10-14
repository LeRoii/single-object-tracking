#pragma once

namespace nanotrack {

enum class InteractiveCommand {
  kNone,
  kStep,
  kTogglePlayback,
  kSelectRoi,
  kQuit,
};

InteractiveCommand decode_interactive_key(int key);
int interactive_wait_delay_ms(bool playing, double source_fps);

class InteractivePlayback {
 public:
  bool handle_key(int key);
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
