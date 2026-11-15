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

InteractiveRect interactive_image_rect(int source_width, int source_height,
                                       int display_width, int display_height) {
  if (source_width <= 0 || source_height <= 0 ||
      display_width <= 0 || display_height <= 0)
    throw std::invalid_argument("interactive image dimensions must be positive");
  const double scale = std::min(double(display_width) / source_width,
                                double(display_height) / source_height);
  const int width = std::clamp(int(std::lround(source_width * scale)), 1,
                               display_width);
  const int height = std::clamp(int(std::lround(source_height * scale)), 1,
                                display_height);
  return {(display_width - width) / 2, (display_height - height) / 2,
          width, height};
}

std::optional<InteractiveClickPoint> map_interactive_click(
    int x, int y, int source_width, int source_height,
    int display_width, int display_height) {
  const auto image = interactive_image_rect(source_width, source_height,
                                            display_width, display_height);
  if (x < image.x || y < image.y || x >= image.x + image.width ||
      y >= image.y + image.height) return std::nullopt;
  return InteractiveClickPoint{
      std::min(source_width - 1, (x - image.x) * source_width / image.width),
      std::min(source_height - 1, (y - image.y) * source_height / image.height)};
}

InteractiveClickTarget choose_interactive_click_target(
    InteractiveClickPoint point, FrameSize frame,
    const std::vector<DetectorCandidate>& candidates,
    std::size_t class_count, int fallback_width, int fallback_height) {
  if (frame.width <= 0 || frame.height <= 0 || fallback_width <= 0 ||
      fallback_height <= 0 || point.x < 0 || point.y < 0 ||
      point.x >= frame.width || point.y >= frame.height)
    throw std::invalid_argument("invalid interactive click or fallback size");
  std::optional<std::size_t> selected;
  BBox selected_box{};
  double selected_area = 0.0;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    const auto& candidate = candidates[i];
    const auto& box = candidate.bbox;
    if (candidate.class_id < 0 ||
        static_cast<std::size_t>(candidate.class_id) >= class_count ||
        !std::isfinite(candidate.confidence) ||
        !std::isfinite(box.x) || !std::isfinite(box.y) ||
        !std::isfinite(box.width) || !std::isfinite(box.height) ||
        box.width <= 0 || box.height <= 0) continue;
    const double x0 = std::clamp<double>(box.x, 0, frame.width);
    const double y0 = std::clamp<double>(box.y, 0, frame.height);
    const double x1 = std::clamp<double>(double(box.x) + box.width, 0, frame.width);
    const double y1 = std::clamp<double>(double(box.y) + box.height, 0, frame.height);
    if (x1 <= x0 || y1 <= y0 || point.x < x0 || point.x >= x1 ||
        point.y < y0 || point.y >= y1) continue;
    const double area = (x1 - x0) * (y1 - y0);
    if (!selected || candidate.confidence > candidates[*selected].confidence ||
        (candidate.confidence == candidates[*selected].confidence &&
         area < selected_area)) {
      selected = i;
      selected_box = {float(x0), float(y0), float(x1 - x0), float(y1 - y0)};
      selected_area = area;
    }
  }
  if (selected) return {selected_box, selected};
  const double x0 = std::clamp<double>(double(point.x) - fallback_width / 2.0,
                                       0, frame.width);
  const double y0 = std::clamp<double>(double(point.y) - fallback_height / 2.0,
                                       0, frame.height);
  const double x1 = std::clamp<double>(double(point.x) + fallback_width / 2.0,
                                       0, frame.width);
  const double y1 = std::clamp<double>(double(point.y) + fallback_height / 2.0,
                                       0, frame.height);
  return {{float(x0), float(y0), float(x1 - x0), float(y1 - y0)}, std::nullopt};
}

BBox minimum_detector_tracking_bbox(BBox detector_bbox, FrameSize frame,
                                    int min_width, int min_height) {
  if (frame.width <= 0 || frame.height <= 0 || min_width <= 0 || min_height <= 0 ||
      !std::isfinite(detector_bbox.x) || !std::isfinite(detector_bbox.y) ||
      !std::isfinite(detector_bbox.width) || !std::isfinite(detector_bbox.height) ||
      detector_bbox.width <= 0.0F || detector_bbox.height <= 0.0F)
    throw std::invalid_argument("invalid detector tracking bbox or minimum size");
  const float center_x = detector_bbox.x + 0.5F * detector_bbox.width;
  const float center_y = detector_bbox.y + 0.5F * detector_bbox.height;
  if (detector_bbox.width < min_width) {
    const float left = std::clamp(center_x - 0.5F * min_width, 0.0F,
                                  static_cast<float>(frame.width));
    const float right = std::clamp(center_x + 0.5F * min_width, 0.0F,
                                   static_cast<float>(frame.width));
    detector_bbox.x = left;
    detector_bbox.width = right - left;
  }
  if (detector_bbox.height < min_height) {
    const float top = std::clamp(center_y - 0.5F * min_height, 0.0F,
                                 static_cast<float>(frame.height));
    const float bottom = std::clamp(center_y + 0.5F * min_height, 0.0F,
                                    static_cast<float>(frame.height));
    detector_bbox.y = top;
    detector_bbox.height = bottom - top;
  }
  return detector_bbox;
}

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
