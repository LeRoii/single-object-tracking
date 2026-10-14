#include "interactive_control.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  using namespace nanotrack;

  expect_true(decode_interactive_key('n') == InteractiveCommand::kStep,
              "n advances exactly one frame");
  expect_true(decode_interactive_key('N') == InteractiveCommand::kStep,
              "uppercase N advances exactly one frame");
  expect_true(decode_interactive_key(0xff53) == InteractiveCommand::kStep,
              "Linux right arrow advances exactly one frame");
  expect_true(decode_interactive_key(2555904) == InteractiveCommand::kStep,
              "Qt right arrow advances exactly one frame");
  expect_true(decode_interactive_key(' ') ==
                  InteractiveCommand::kTogglePlayback,
              "space toggles continuous playback");
  expect_true(decode_interactive_key('r') ==
                  InteractiveCommand::kSelectRoi,
              "r starts or resets ROI selection");
  expect_true(decode_interactive_key('R') ==
                  InteractiveCommand::kSelectRoi,
              "uppercase R starts or resets ROI selection");
  expect_true(decode_interactive_key('q') == InteractiveCommand::kQuit &&
                  decode_interactive_key(27) == InteractiveCommand::kQuit,
              "q and escape quit");
  expect_true(decode_interactive_key(-1) == InteractiveCommand::kNone &&
                  decode_interactive_key('x') == InteractiveCommand::kNone,
              "timeout and unrelated keys have no command");

  InteractivePlayback playback;
  expect_true(!playback.playing(), "interactive playback starts paused");
  expect_true(!playback.handle_key(-1),
              "paused timeout does not advance a frame");
  expect_true(playback.handle_key('n') && !playback.playing(),
              "step advances once and remains paused");
  expect_true(!playback.handle_key(' ' ) && playback.playing(),
              "space enters continuous playback without an immediate step");
  expect_true(playback.handle_key(-1) && playback.playing(),
              "continuous timeout advances a frame");
  expect_true(!playback.handle_key('r') && !playback.playing() &&
                  playback.selection_requested(),
              "ROI selection pauses playback and raises one request");
  expect_true(playback.take_selection_request() &&
                  !playback.selection_requested(),
              "ROI request is consumed exactly once");
  expect_true(!playback.handle_key('q') && playback.quit_requested(),
              "quit stops advancement and records exit request");

  expect_true(interactive_wait_delay_ms(false, 30.0) == 0,
              "paused playback blocks for a key");
  expect_true(interactive_wait_delay_ms(true, 25.0) == 40,
              "continuous playback uses source frame period");
  expect_true(interactive_wait_delay_ms(true, 0.0) == 33,
              "invalid source FPS uses 30 FPS fallback");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "interactive control tests passed\n";
  return 0;
}
