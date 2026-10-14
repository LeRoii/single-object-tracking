#include "yolo_rknn_engine.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::string usage() {
  return "Usage: yolo_detector_smoke --model MODEL.rknn --manifest MANIFEST.json "
         "--labels LABELS.txt --image IMAGE --conf FLOAT --nms FLOAT "
         "--class-id ID --output-image OUTPUT.jpg --output-csv OUTPUT.csv\n";
}

float parse_threshold(const std::string& text, const char* name) {
  std::size_t consumed = 0;
  const float value = std::stof(text, &consumed);
  if (consumed != text.size() || !std::isfinite(value) || value <= 0.0F ||
      value >= 1.0F) {
    throw std::invalid_argument(std::string(name) + " must be finite and in (0,1)");
  }
  return value;
}

int parse_class_id(const std::string& text) {
  std::size_t consumed = 0;
  const long long value = std::stoll(text, &consumed);
  if (consumed != text.size() || value < -1 ||
      value > std::numeric_limits<int>::max()) {
    throw std::invalid_argument("--class-id must be -1 or a non-negative integer");
  }
  return static_cast<int>(value);
}

std::unordered_map<std::string, std::string> parse_arguments(
    const std::vector<std::string>& arguments) {
  if (arguments.size() == 2 && arguments[1] == "--help") return {{"--help", ""}};
  const std::vector<std::string> allowed{
      "--model", "--manifest", "--labels", "--image", "--conf",
      "--nms", "--class-id", "--output-image", "--output-csv"};
  std::unordered_map<std::string, std::string> values;
  for (std::size_t index = 1; index < arguments.size(); index += 2) {
    if (index + 1 >= arguments.size()) {
      throw std::invalid_argument("missing value for " + arguments[index]);
    }
    const std::string& option = arguments[index];
    if (std::find(allowed.begin(), allowed.end(), option) == allowed.end()) {
      throw std::invalid_argument("unknown option: " + option);
    }
    if (!values.emplace(option, arguments[index + 1]).second) {
      throw std::invalid_argument("duplicate option: " + option);
    }
  }
  for (const std::string& option : allowed) {
    if (values.find(option) == values.end()) {
      throw std::invalid_argument("missing required option: " + option);
    }
  }
  return values;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const std::vector<std::string> arguments(argv, argv + argc);
    const auto values = parse_arguments(arguments);
    if (values.find("--help") != values.end()) {
      std::cout << usage();
      return 0;
    }

    nanotrack::YoloRknnEngineConfig config;
    config.model_path = values.at("--model");
    config.manifest_path = values.at("--manifest");
    config.labels_path = values.at("--labels");
    config.confidence_threshold =
        parse_threshold(values.at("--conf"), "--conf");
    config.nms_threshold = parse_threshold(values.at("--nms"), "--nms");
    config.class_id = parse_class_id(values.at("--class-id"));
    nanotrack::YoloRknnEngine engine(config);

    cv::Mat image = cv::imread(values.at("--image"), cv::IMREAD_COLOR);
    if (image.empty()) throw std::runtime_error("cannot decode smoke input image");
    const nanotrack::ImageView view{
        image.data, image.cols, image.rows, image.step, image.channels()};
    const nanotrack::DetectorRequest request{
        0,
        0,
        nanotrack::BBox{0.0F, 0.0F, static_cast<float>(image.cols),
                        static_cast<float>(image.rows)},
        100};
    const nanotrack::DetectorEngineResult result = engine.infer(view, request);

    cv::Mat annotated = image.clone();
    const std::vector<std::string>& labels = engine.labels();
    for (const nanotrack::DetectorCandidate& candidate : result.candidates) {
      const cv::Rect rectangle(
          static_cast<int>(std::lround(candidate.bbox.x)),
          static_cast<int>(std::lround(candidate.bbox.y)),
          std::max(1, static_cast<int>(std::lround(candidate.bbox.width))),
          std::max(1, static_cast<int>(std::lround(candidate.bbox.height))));
      cv::rectangle(annotated, rectangle, cv::Scalar(0, 255, 0), 2);
      const std::string label =
          labels.at(static_cast<std::size_t>(candidate.class_id)) + " " +
          std::to_string(candidate.confidence).substr(0, 5);
      cv::putText(annotated, label,
                  cv::Point(std::max(0, rectangle.x),
                            std::max(18, rectangle.y - 5)),
                  cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 0), 2,
                  cv::LINE_AA);
    }
    if (!cv::imwrite(values.at("--output-image"), annotated)) {
      throw std::runtime_error("cannot write annotated smoke image");
    }

    std::ofstream csv(values.at("--output-csv"));
    if (!csv) throw std::runtime_error("cannot open smoke candidate CSV");
    csv << "class_id,label,confidence,x,y,width,height\n";
    csv << std::fixed << std::setprecision(6);
    for (const nanotrack::DetectorCandidate& candidate : result.candidates) {
      csv << candidate.class_id << ','
          << labels.at(static_cast<std::size_t>(candidate.class_id)) << ','
          << candidate.confidence << ',' << candidate.bbox.x << ','
          << candidate.bbox.y << ',' << candidate.bbox.width << ','
          << candidate.bbox.height << '\n';
    }
    csv.close();
    if (!csv || !std::filesystem::is_regular_file(values.at("--output-image")) ||
        !std::filesystem::is_regular_file(values.at("--output-csv")) ||
        std::filesystem::file_size(values.at("--output-image")) == 0 ||
        std::filesystem::file_size(values.at("--output-csv")) == 0) {
      throw std::runtime_error("smoke output artifact is missing or empty");
    }

    std::cout << std::fixed << std::setprecision(3)
              << "SMOKE candidates=" << result.candidates.size()
              << " raw=" << result.raw_candidate_count
              << " filtered=" << result.filtered_candidate_count
              << " preprocess_ms=" << result.timing.preprocess_ms
              << " inference_ms=" << result.timing.inference_ms
              << " postprocess_ms=" << result.timing.postprocess_ms
              << " total_ms=" << result.timing.total_ms
              << " output_image=" << values.at("--output-image")
              << " output_csv=" << values.at("--output-csv") << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << '\n' << usage();
    return 1;
  }
}
