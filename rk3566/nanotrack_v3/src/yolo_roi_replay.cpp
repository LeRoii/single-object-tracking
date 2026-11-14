#include "yolo_rknn_engine.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  try {
    if (argc != 7) {
      throw std::invalid_argument(
          "usage: yolo_roi_replay MODEL MANIFEST LABELS VIDEO ROIS OUTPUT.csv");
    }
    nanotrack::YoloRknnEngineConfig config;
    config.model_path = argv[1];
    config.manifest_path = argv[2];
    config.labels_path = argv[3];
    config.confidence_threshold = 0.10F;
    config.nms_threshold = 0.45F;
    config.class_id = 2;
    nanotrack::YoloRknnEngine engine(config);

    cv::VideoCapture video(argv[4]);
    if (!video.isOpened()) throw std::runtime_error("cannot open video");
    std::ifstream rois(argv[5]);
    if (!rois) throw std::runtime_error("cannot open ROI trace");
    std::ofstream output(argv[6]);
    if (!output) throw std::runtime_error("cannot open output CSV");
    output << "frame,rank,confidence,x,y,width,height,preprocess_ms,"
              "inference_ms,postprocess_ms,total_ms\n";
    output << std::fixed << std::setprecision(6);

    std::uint64_t frame_id = 0;
    cv::Mat frame;
    while (video.read(frame)) {
      std::uint64_t roi_frame = 0;
      float x = 0.0F, y = 0.0F, width = 0.0F, height = 0.0F;
      if (!(rois >> roi_frame >> x >> y >> width >> height) ||
          roi_frame != frame_id || width <= 0.0F || height <= 0.0F ||
          !std::isfinite(x) || !std::isfinite(y) ||
          !std::isfinite(width) || !std::isfinite(height)) {
        throw std::runtime_error("missing or invalid ROI at frame " +
                                 std::to_string(frame_id));
      }
      const float center_x = x + 0.5F * width;
      const float center_y = y + 0.5F * height;
      const int size = std::max(
          120, static_cast<int>(std::lround(3.0F * std::max(width, height))));
      if (size > frame.cols || size > frame.rows) {
        throw std::runtime_error("ROI crop exceeds frame dimensions");
      }
      const int left = std::clamp(
          static_cast<int>(std::lround(center_x - 0.5F * size)),
          0, frame.cols - size);
      const int top = std::clamp(
          static_cast<int>(std::lround(center_y - 0.5F * size)),
          0, frame.rows - size);
      cv::Mat canvas(640, 640, CV_8UC3, cv::Scalar(114, 114, 114));
      cv::resize(frame(cv::Rect(left, top, size, size)),
                 canvas(cv::Rect(160, 160, 320, 320)),
                 cv::Size(320, 320));
      const nanotrack::ImageView view{
          canvas.data, canvas.cols, canvas.rows, canvas.step,
          canvas.channels()};
      const nanotrack::DetectorRequest request{
          frame_id, static_cast<std::int64_t>(frame_id * 1000 / 30),
          nanotrack::BBox{0.0F, 0.0F, 640.0F, 640.0F}, 100,
          0, nanotrack::DetectorRequestPurpose::kDisplayRefresh};
      const auto result = engine.infer(view, request);
      std::size_t rank = 0;
      for (const auto& candidate : result.candidates) {
        const auto& box = candidate.bbox;
        const float factor = static_cast<float>(size) / 320.0F;
        output << frame_id << ',' << rank++ << ','
               << candidate.confidence << ','
               << left + (box.x - 160.0F) * factor << ','
               << top + (box.y - 160.0F) * factor << ','
               << box.width * factor << ',' << box.height * factor << ','
               << result.timing.preprocess_ms << ','
               << result.timing.inference_ms << ','
               << result.timing.postprocess_ms << ','
               << result.timing.total_ms << '\n';
      }
      ++frame_id;
    }
    std::uint64_t trailing_frame = 0;
    if (rois >> trailing_frame || !rois.eof() || !output || frame_id != 271) {
      throw std::runtime_error("video and ROI trace length mismatch");
    }
    std::cout << "frames=" << frame_id << "\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << '\n';
    return 1;
  }
}
