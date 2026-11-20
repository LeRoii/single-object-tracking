// Render actual display_bbox values from two board CSVs on the same input video.
// Build on a host with OpenCV: c++ -O2 -std=c++17 this_file.cpp -o render_display \
//   $(pkg-config --cflags --libs opencv4)
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::vector<std::string> csv_fields(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  bool quoted = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    const char ch = line[index];
    if (ch == '"') {
      if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
        field += '"';
        ++index;
      } else {
        quoted = !quoted;
      }
    } else if (ch == ',' && !quoted) {
      fields.push_back(field);
      field.clear();
    } else {
      field += ch;
    }
  }
  if (quoted) throw std::runtime_error("unterminated CSV quote");
  fields.push_back(field);
  return fields;
}

std::size_t column(const std::vector<std::string>& header, const std::string& name) {
  const auto found = std::find(header.begin(), header.end(), name);
  if (found == header.end()) throw std::runtime_error("missing CSV column: " + name);
  return static_cast<std::size_t>(found - header.begin());
}

std::unordered_map<int, cv::Rect2f> display_boxes(const std::string& path) {
  std::ifstream csv(path);
  if (!csv) throw std::runtime_error("cannot open CSV: " + path);
  std::string line;
  if (!std::getline(csv, line)) throw std::runtime_error("empty CSV: " + path);
  const auto header = csv_fields(line);
  const auto frame_col = column(header, "frame_id");
  const auto x_col = column(header, "display_x");
  const auto y_col = column(header, "display_y");
  const auto w_col = column(header, "display_width");
  const auto h_col = column(header, "display_height");
  std::unordered_map<int, cv::Rect2f> result;
  while (std::getline(csv, line)) {
    const auto fields = csv_fields(line);
    if (fields.size() != header.size()) throw std::runtime_error("CSV column count mismatch");
    const int frame = std::stoi(fields[frame_col]);
    const cv::Rect2f box{std::stof(fields[x_col]), std::stof(fields[y_col]),
                         std::stof(fields[w_col]), std::stof(fields[h_col])};
    if (!result.emplace(frame, box).second) {
      throw std::runtime_error("duplicate frame ID in " + path);
    }
  }
  return result;
}

cv::Mat panel(const cv::Mat& source, const cv::Rect& crop,
              const cv::Rect2f& box, const cv::Scalar& color,
              const std::string& label) {
  cv::Mat region = source(crop).clone();
  cv::rectangle(region,
                cv::Rect(cvRound(box.x - crop.x), cvRound(box.y - crop.y),
                         std::max(1, cvRound(box.width)),
                         std::max(1, cvRound(box.height))),
                color, 2, cv::LINE_AA);
  cv::Mat enlarged;
  cv::resize(region, enlarged, cv::Size(crop.width * 2, crop.height * 2));
  cv::putText(enlarged, label, {15, 35}, cv::FONT_HERSHEY_SIMPLEX,
              0.8, color, 2, cv::LINE_AA);
  return enlarged;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 7) {
      std::cerr << "usage: render_display_comparison INPUT.mp4 OFF.csv ON.csv "
                   "CROP_X,Y,W,H MAX_FRAMES OUTPUT.avi\n";
      return 2;
    }
    const auto off = display_boxes(argv[2]);
    const auto on = display_boxes(argv[3]);
    int x = 0, y = 0, width = 0, height = 0;
    if (std::sscanf(argv[4], "%d,%d,%d,%d", &x, &y, &width, &height) != 4) {
      throw std::runtime_error("invalid crop rectangle");
    }
    const cv::Rect crop{x, y, width, height};
    cv::VideoCapture reader(argv[1]);
    if (!reader.isOpened()) throw std::runtime_error("cannot open input video");
    const int max_frames = std::stoi(argv[5]);
    if (max_frames <= 0 || width <= 0 || height <= 0 || x < 0 || y < 0 ||
        x + width > reader.get(cv::CAP_PROP_FRAME_WIDTH) ||
        y + height > reader.get(cv::CAP_PROP_FRAME_HEIGHT)) {
      throw std::runtime_error("crop or frame count outside video");
    }
    cv::VideoWriter writer(argv[6], cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           reader.get(cv::CAP_PROP_FPS),
                           cv::Size(width * 4, height * 2));
    if (!writer.isOpened()) throw std::runtime_error("cannot write output video");
    cv::Mat source;
    int rendered = 0;
    while (rendered < max_frames && reader.read(source)) {
      const auto old = off.find(rendered);
      const auto updated = on.find(rendered);
      if (old == off.end() || updated == on.end()) {
        throw std::runtime_error("CSV missing video frame " + std::to_string(rendered));
      }
      const cv::Mat left = panel(source, crop, old->second,
                                 cv::Scalar(0, 0, 255), "OFF");
      const cv::Mat right = panel(source, crop, updated->second,
                                  cv::Scalar(0, 255, 0), "ON");
      cv::Mat joined(height * 2, width * 4, CV_8UC3);
      left.copyTo(joined(cv::Rect(0, 0, width * 2, height * 2)));
      right.copyTo(joined(cv::Rect(width * 2, 0, width * 2, height * 2)));
      writer.write(joined);
      ++rendered;
    }
    std::cout << "rendered_frames=" << rendered << '\n';
    return rendered == max_frames ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
