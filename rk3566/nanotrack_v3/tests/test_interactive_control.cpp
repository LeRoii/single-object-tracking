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
  expect_true(decode_interactive_key(0x100072) ==
                  InteractiveCommand::kSelectRoi,
              "GTK-prefixed R starts or resets ROI selection");
  expect_true(decode_interactive_key(0x10006e) == InteractiveCommand::kStep,
              "GTK-prefixed N advances exactly one frame");
  expect_true(decode_interactive_key(0x100020) ==
                  InteractiveCommand::kTogglePlayback,
              "GTK-prefixed space toggles continuous playback");
  expect_true(decode_interactive_key(0x100071) == InteractiveCommand::kQuit &&
                  decode_interactive_key(0x10001b) ==
                      InteractiveCommand::kQuit,
              "GTK-prefixed q and escape quit");
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

  InteractivePlayback limited_playback;
  limited_playback.handle_key(' ');
  limited_playback.pause();
  expect_true(!limited_playback.playing(),
              "a decode limit can pause continuous playback on its last frame");

  expect_true(interactive_window_visible(1.0),
              "a supported visible window remains active");
  expect_true(!interactive_window_visible(0.0),
              "a supported closed window stops interaction");
  expect_true(interactive_window_visible(-1.0),
              "an unsupported GTK visibility property is not a close event");

  InteractiveRoiSelection cancelled(100, 80);
  cancelled.handle_mouse(RoiMouseEvent::kDown, 10, 15);
  cancelled.handle_mouse(RoiMouseEvent::kMove, 50, 45);
  cancelled.handle_mouse(RoiMouseEvent::kUp, 50, 45);
  expect_true(cancelled.current_rect().has_value(),
              "completed drag exposes a pending rectangle");
  cancelled.handle_key(27);
  expect_true(cancelled.cancelled() && !cancelled.confirmed_rect().has_value(),
              "escape cancels even after a non-empty rectangle was drawn");

  InteractiveRoiSelection gtk_cancelled(100, 80);
  gtk_cancelled.handle_mouse(RoiMouseEvent::kDown, 10, 15);
  gtk_cancelled.handle_mouse(RoiMouseEvent::kUp, 50, 45);
  gtk_cancelled.handle_key(0x10001b);
  expect_true(gtk_cancelled.cancelled(),
              "GTK-prefixed escape cancels ROI selection");

  InteractiveRoiSelection confirmed(100, 80);
  confirmed.handle_mouse(RoiMouseEvent::kDown, 70, 60);
  confirmed.handle_mouse(RoiMouseEvent::kMove, 20, 10);
  confirmed.handle_mouse(RoiMouseEvent::kUp, 20, 10);
  confirmed.handle_key(13);
  const auto confirmed_rect = confirmed.confirmed_rect();
  expect_true(confirmed_rect.has_value() && confirmed_rect->x == 20 &&
                  confirmed_rect->y == 10 && confirmed_rect->width == 50 &&
                  confirmed_rect->height == 50,
              "enter confirms a normalized reverse-drag rectangle");

  InteractiveRoiSelection gtk_confirmed(100, 80);
  gtk_confirmed.handle_mouse(RoiMouseEvent::kDown, 10, 15);
  gtk_confirmed.handle_mouse(RoiMouseEvent::kUp, 50, 45);
  gtk_confirmed.handle_key(0x10000d);
  expect_true(gtk_confirmed.confirmed_rect().has_value(),
              "GTK-prefixed enter confirms ROI selection");

  InteractiveRoiSelection clipped(100, 80);
  clipped.handle_mouse(RoiMouseEvent::kDown, -10, -20);
  clipped.handle_mouse(RoiMouseEvent::kUp, 130, 95);
  clipped.handle_key(' ');
  const auto clipped_rect = clipped.confirmed_rect();
  expect_true(clipped_rect.has_value() && clipped_rect->x == 0 &&
                  clipped_rect->y == 0 && clipped_rect->width == 99 &&
                  clipped_rect->height == 79,
              "mouse coordinates are clipped to source-frame boundaries");

  InteractiveRoiSelection empty(100, 80);
  empty.handle_key(13);
  expect_true(!empty.finished() && !empty.confirmed_rect().has_value(),
              "confirm without a non-empty drag keeps selection active");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "interactive control tests passed\n";
  return 0;
}
