#include "bidirectional_annotation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

namespace fs = std::filesystem;

namespace {

struct Options {
  fs::path video;
  fs::path anchors;
  fs::path output;
  fs::path review_dir;
  nanotrack::AgreementThresholds thresholds;
  bool help = false;
};

float parse_float(const std::string& name, const std::string& text) {
  std::size_t consumed = 0;
  const float value = std::stof(text, &consumed);
  if (consumed != text.size() || !std::isfinite(value)) {
    throw std::invalid_argument("invalid " + name + ": " + text);
  }
  return value;
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--help" || arg == "-h") {
      options.help = true;
      continue;
    }
    if (index + 1 >= argc) throw std::invalid_argument("missing value for " + arg);
    const std::string value = argv[++index];
    if (arg == "--video") options.video = value;
    else if (arg == "--anchors") options.anchors = value;
    else if (arg == "--output") options.output = value;
    else if (arg == "--review-dir") options.review_dir = value;
    else if (arg == "--agreement-iou")
      options.thresholds.iou_min = parse_float(arg, value);
    else if (arg == "--agreement-center")
      options.thresholds.center_max = parse_float(arg, value);
    else throw std::invalid_argument("unknown option: " + arg);
  }
  if (!options.help && (options.video.empty() || options.anchors.empty() ||
                        options.output.empty() || options.review_dir.empty())) {
    throw std::invalid_argument(
        "--video, --anchors, --output, and --review-dir are required");
  }
  return options;
}

std::vector<std::string> split_csv(const std::string& line) {
  std::vector<std::string> fields;
  std::stringstream stream(line);
  std::string field;
  while (std::getline(stream, field, ',')) fields.push_back(field);
  return fields;
}

std::vector<nanotrack::AnnotationAnchor> read_anchors(const fs::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open anchors: " + path.string());
  std::string line;
  if (!std::getline(input, line) || line != "frame_id,x,y,width,height") {
    throw std::invalid_argument("anchors header must be frame_id,x,y,width,height");
  }
  std::vector<nanotrack::AnnotationAnchor> anchors;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto fields = split_csv(line);
    if (fields.size() != 5) throw std::invalid_argument("anchor row needs five fields");
    nanotrack::AnnotationAnchor anchor;
    anchor.frame_id = std::stoi(fields[0]);
    anchor.bbox = {parse_float("anchor x", fields[1]),
                   parse_float("anchor y", fields[2]),
                   parse_float("anchor width", fields[3]),
                   parse_float("anchor height", fields[4])};
    if (!anchors.empty() && anchor.frame_id <= anchors.back().frame_id) {
      throw std::invalid_argument("anchor frame ids must be strictly increasing");
    }
    anchors.push_back(anchor);
  }
  if (anchors.size() < 2) throw std::invalid_argument("at least two anchors are required");
  return anchors;
}

std::vector<cv::Mat> read_segment(cv::VideoCapture& capture, int first, int last,
                                  int width, int height) {
  if (!capture.set(cv::CAP_PROP_POS_FRAMES, first)) {
    throw std::runtime_error("failed to seek video to segment anchor");
  }
  std::vector<cv::Mat> frames;
  for (int frame_id = first; frame_id <= last; ++frame_id) {
    cv::Mat frame;
    if (!capture.read(frame)) throw std::runtime_error("video ended before frame " + std::to_string(frame_id));
    if (frame.cols != width || frame.rows != height) {
      throw std::runtime_error("decoded frame geometry changed");
    }
    frames.push_back(frame);
  }
  return frames;
}

void write_bbox_fields(std::ostream& output,
                       const nanotrack::DirectionalObservation& observation) {
  output << (observation.success ? 1 : 0) << ',';
  if (observation.success) {
    output << observation.bbox.x << ',' << observation.bbox.y << ','
           << observation.bbox.width << ',' << observation.bbox.height;
  } else {
    output << ",,,";
  }
}

void write_candidates(const fs::path& path,
                      const std::vector<nanotrack::BidirectionalCandidate>& values) {
  std::ofstream output(path);
  if (!output) throw std::runtime_error("cannot create candidate CSV: " + path.string());
  output << "frame_id,forward_success,forward_x,forward_y,forward_width,forward_height,"
            "reverse_success,reverse_x,reverse_y,reverse_width,reverse_height,"
            "candidate_x,candidate_y,candidate_width,candidate_height,iou,"
            "normalized_center_distance,status\n";
  output << std::fixed << std::setprecision(6);
  for (const auto& value : values) {
    output << value.frame_id << ',';
    write_bbox_fields(output, value.forward);
    output << ',';
    write_bbox_fields(output, value.reverse);
    output << ',';
    if (value.consensus_bbox) {
      output << value.consensus_bbox->x << ',' << value.consensus_bbox->y << ','
             << value.consensus_bbox->width << ',' << value.consensus_bbox->height;
    } else {
      output << ",,,";
    }
    output << ',' << value.iou << ',' << value.normalized_center_distance << ','
           << nanotrack::to_string(value.status) << '\n';
  }
}

cv::Rect draw_rect(const nanotrack::BBox& bbox) {
  return {static_cast<int>(std::lround(bbox.x)),
          static_cast<int>(std::lround(bbox.y)),
          std::max(1, static_cast<int>(std::lround(bbox.width))),
          std::max(1, static_cast<int>(std::lround(bbox.height)))};
}

void annotate(cv::Mat& frame, const nanotrack::BidirectionalCandidate& value) {
  if (value.forward.success) cv::rectangle(frame, draw_rect(value.forward.bbox), {255, 100, 0}, 2);
  if (value.reverse.success) cv::rectangle(frame, draw_rect(value.reverse.bbox), {255, 0, 255}, 2);
  if (value.consensus_bbox) cv::rectangle(frame, draw_rect(*value.consensus_bbox), {0, 255, 0}, 3);
  cv::putText(frame, std::to_string(value.frame_id) + " " + nanotrack::to_string(value.status),
              {20, 40}, cv::FONT_HERSHEY_SIMPLEX, 0.8,
              value.status == nanotrack::CandidateStatus::kNeedsHumanReview
                  ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0),
              2, cv::LINE_AA);
}

void write_review_media(const fs::path& video_path, const fs::path& review_dir,
                        const std::vector<nanotrack::BidirectionalCandidate>& values,
                        int width, int height, double fps) {
  fs::create_directories(review_dir);
  cv::VideoCapture capture(video_path.string());
  cv::VideoWriter writer((review_dir / "candidate-review.avi").string(),
                         cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps,
                         {width, height});
  if (!capture.isOpened() || !writer.isOpened()) {
    throw std::runtime_error("cannot create review video");
  }
  std::vector<cv::Mat> sheet_tiles;
  int sheet_index = 0;
  for (const auto& value : values) {
    cv::Mat frame;
    if (!capture.read(frame)) throw std::runtime_error("review decode ended early");
    annotate(frame, value);
    writer.write(frame);
    if (value.status == nanotrack::CandidateStatus::kAnchor ||
        value.status == nanotrack::CandidateStatus::kNeedsHumanReview) {
      cv::Mat tile;
      cv::resize(frame, tile, {480, 270});
      sheet_tiles.push_back(tile);
      if (sheet_tiles.size() == 12) {
        cv::Mat sheet(810, 1920, CV_8UC3, cv::Scalar::all(0));
        for (std::size_t index = 0; index < sheet_tiles.size(); ++index) {
          sheet_tiles[index].copyTo(sheet(cv::Rect(
              static_cast<int>(index % 4) * 480,
              static_cast<int>(index / 4) * 270, 480, 270)));
        }
        std::ostringstream name;
        name << "contact-sheet-" << std::setw(3) << std::setfill('0')
             << sheet_index++ << ".jpg";
        cv::imwrite((review_dir / name.str()).string(), sheet);
        sheet_tiles.clear();
      }
    }
  }
  if (!sheet_tiles.empty()) {
    cv::Mat sheet(810, 1920, CV_8UC3, cv::Scalar::all(0));
    for (std::size_t index = 0; index < sheet_tiles.size(); ++index) {
      sheet_tiles[index].copyTo(sheet(cv::Rect(
          static_cast<int>(index % 4) * 480,
          static_cast<int>(index / 4) * 270, 480, 270)));
    }
    std::ostringstream name;
    name << "contact-sheet-" << std::setw(3) << std::setfill('0')
         << sheet_index << ".jpg";
    cv::imwrite((review_dir / name.str()).string(), sheet);
  }
}

void print_usage() {
  std::cout << "Usage: generate_bidirectional_candidates --video FILE --anchors CSV "
               "--output CSV --review-dir DIR [--agreement-iou 0.50] "
               "[--agreement-center 0.35]\n";
}

}  // namespace

int main(int argc, char** argv) try {
  const Options options = parse_options(argc, argv);
  if (options.help) {
    print_usage();
    return 0;
  }
  const auto anchors = read_anchors(options.anchors);
  cv::VideoCapture capture(options.video.string());
  if (!capture.isOpened()) throw std::runtime_error("cannot open video");
  const int width = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
  const int height = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
  const int frame_count = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_COUNT));
  const double fps = capture.get(cv::CAP_PROP_FPS);
  if (width <= 0 || height <= 0 || frame_count <= 0 || fps <= 0.0) {
    throw std::runtime_error("invalid video metadata");
  }
  if (anchors.front().frame_id != 0 || anchors.back().frame_id != frame_count - 1) {
    throw std::invalid_argument("anchors must cover frame 0 through the final frame");
  }
  for (const auto& anchor : anchors) nanotrack::validate_bbox(anchor.bbox, width, height);

  std::vector<std::vector<nanotrack::BidirectionalCandidate>> segments;
  for (std::size_t index = 1; index < anchors.size(); ++index) {
    auto frames = read_segment(capture, anchors[index - 1].frame_id,
                               anchors[index].frame_id, width, height);
    segments.push_back(nanotrack::track_segment_bidirectional(
        frames, anchors[index - 1], anchors[index], options.thresholds));
  }
  const auto candidates = nanotrack::stitch_candidate_segments(segments);
  if (static_cast<int>(candidates.size()) != frame_count) {
    throw std::runtime_error("candidate output does not cover every frame");
  }
  fs::create_directories(options.output.parent_path());
  write_candidates(options.output, candidates);
  write_review_media(options.video, options.review_dir, candidates, width, height, fps);
  std::cout << "frames=" << candidates.size() << " output=" << options.output
            << " review_dir=" << options.review_dir << '\n';
  return 0;
} catch (const std::exception& error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return 1;
}
