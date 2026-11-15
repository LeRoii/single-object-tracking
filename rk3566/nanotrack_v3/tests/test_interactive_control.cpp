#include "interactive_control.hpp"

#include <cmath>
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

  expect_true(kInteractiveWindowWidth == 1280 &&
                  kInteractiveWindowHeight == 720,
              "interactive debug window defaults to 1280x720");

  const auto fitted = interactive_image_rect(1920, 1080, 1280, 720);
  expect_true(fitted.x == 0 && fitted.y == 0 && fitted.width == 1280 &&
                  fitted.height == 720,
              "16:9 source fills click canvas");
  const auto mapped = map_interactive_click(640, 360, 1920, 1080, 1280, 720);
  expect_true(mapped && mapped->x == 960 && mapped->y == 540,
              "display click maps to original frame");
  const auto pillarbox = interactive_image_rect(100, 100, 1280, 720);
  expect_true(pillarbox.x == 280 && pillarbox.width == 720 &&
                  !map_interactive_click(100, 360, 100, 100, 1280, 720),
              "click in image padding is ignored");
  const std::vector<DetectorCandidate> overlapping{
      {{20, 20, 80, 80}, 0.8F, 2},
      {{40, 40, 20, 20}, 0.9F, 5},
      {{40, 40, 16, 16}, 0.9F, 4},
      {{40, 40, 16, 16}, 0.9F, 3}};
  const auto chosen = choose_interactive_click_target(
      {48, 48}, {120, 100}, overlapping, 9, 64, 64);
  expect_true(chosen.candidate_index == 2 && chosen.bbox.x == 40 &&
                  chosen.bbox.width == 16,
              "overlap chooses confidence then smaller area then original order");
  const auto fallback = choose_interactive_click_target(
      {80, 60}, {120, 100}, {}, 9, 64, 64);
  expect_true(!fallback.candidate_index && fallback.bbox.x == 48 &&
                  fallback.bbox.y == 28 && fallback.bbox.width == 64 &&
                  fallback.bbox.height == 64,
              "blank click gets centered default box");
  const auto edge = choose_interactive_click_target(
      {1, 1}, {120, 100}, {}, 9, 64, 64);
  expect_true(edge.bbox.x == 0 && edge.bbox.y == 0 &&
                  edge.bbox.width == 33 && edge.bbox.height == 33,
              "fallback box clips at image edge while retaining click center");
  const auto invalid_class = choose_interactive_click_target(
      {48, 48}, {120, 100}, {{{40, 40, 16, 16}, 0.9F, 10}}, 9, 64, 64);
  expect_true(!invalid_class.candidate_index,
              "unverified detector class cannot initialize selected target");
  const BBox partial_detector{973.034729F, 520.489929F, 34.091492F, 31.423828F};
  const BBox expanded = minimum_detector_tracking_bbox(
      partial_detector, {1920, 1080}, 36, 36);
  expect_true(std::abs(expanded.x - 972.080475F) < 0.001F &&
                  std::abs(expanded.y - 518.201843F) < 0.001F &&
                  expanded.width == 36.0F && expanded.height == 36.0F,
              "detector click minimum keeps the detector center");
  const BBox full_detector{994.000916F, 581.063293F, 36.0F, 39.486084F};
  const BBox unchanged = minimum_detector_tracking_bbox(
      full_detector, {1920, 1080}, 36, 36);
  expect_true(unchanged.x == full_detector.x && unchanged.y == full_detector.y &&
                  unchanged.width == full_detector.width &&
                  unchanged.height == full_detector.height,
              "detector boxes above the minimum remain unchanged");
  const BBox clipped_detector = minimum_detector_tracking_bbox(
      {0, 0, 20, 20}, {100, 100}, 36, 36);
  expect_true(clipped_detector.x == 0 && clipped_detector.y == 0 &&
                  clipped_detector.width == 28 && clipped_detector.height == 28,
              "minimum detector bbox clips at image edges");

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
  expect_true(std::string(to_string(InteractiveCommand::kStep)) == "step" &&
                  std::string(to_string(InteractiveCommand::kTogglePlayback)) ==
                      "play_pause" &&
                  std::string(to_string(InteractiveCommand::kSelectRoi)) ==
                      "select_roi" &&
                  std::string(to_string(InteractiveCommand::kQuit)) == "quit" &&
                  std::string(to_string(InteractiveCommand::kNone)) == "none",
              "interactive commands expose stable debug labels");

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
