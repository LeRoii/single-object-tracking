#include "cli.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Function>
void expect_throws(Function function, const std::string& message) {
  try {
    function();
    expect_true(false, message);
  } catch (const std::exception&) {
  }
}

}  // namespace

int main() {
  using namespace nanotrack;

  const std::vector<std::string> valid{
      "nanotrack_v3",      "--video",        "/tmp/input.mp4",
      "--bbox",            "244,161,74,70",  "--model-dir",
      "/tmp/models",       "--output-video", "/tmp/output.avi",
      "--output-boxes",    "/tmp/output.csv", "--max-frames",
      "120",               "--start-frame",  "160",
      "--end-frame",       "340",            "--output-size",
      "1920x1080"};
  const Options options = parse_options(valid);
  expect_true(options.video == "/tmp/input.mp4", "video argument");
  expect_true(options.bbox.x == 244.0F && options.bbox.height == 70.0F,
              "bbox argument");
  expect_true(options.model_dir == "/tmp/models", "model-dir argument");
  expect_true(options.max_frames == 120, "max-frames argument");
  expect_true(options.start_frame == 160 && options.end_frame == 340,
              "inclusive frame range arguments");
  expect_true(options.output_width == 1920 && options.output_height == 1080,
              "output-size argument");

  std::vector<std::string> long_term = valid;
  long_term.insert(long_term.end(),
                   {"--controller-config", "/tmp/controller.conf",
                    "--sensor", "infrared",
                    "--session-metadata", "/tmp/session.json"});
  const Options long_term_options = parse_options(long_term);
  expect_true(long_term_options.controller_config == "/tmp/controller.conf",
              "controller-config argument");
  expect_true(long_term_options.sensor == "infrared",
              "infrared sensor argument");
  expect_true(long_term_options.session_metadata == "/tmp/session.json",
              "session metadata argument");
  expect_true(parse_options(valid).sensor == "visible",
              "legacy command defaults to visible sensor");
  const Options legacy_options = parse_options(valid);
  expect_true(legacy_options.detector_model.empty() &&
                  legacy_options.detector_manifest.empty() &&
                  legacy_options.detector_labels.empty() &&
                  legacy_options.detector_class_id == -1 &&
                  std::fabs(legacy_options.detector_confidence - 0.25F) <
                      1e-6F &&
                  std::fabs(legacy_options.detector_nms - 0.45F) < 1e-6F,
              "legacy command keeps detector disabled with bring-up defaults");

  std::vector<std::string> detector_command = valid;
  detector_command.insert(
      detector_command.end(),
      {"--detector-model", "/tmp/yolov8.rknn", "--detector-manifest",
       "/tmp/detector.json", "--detector-labels", "/tmp/coco80.txt",
       "--detector-class-id", "2", "--detector-conf", "0.30",
       "--detector-nms", "0.50"});
  const Options detector_options = parse_options(detector_command);
  expect_true(detector_options.detector_model == "/tmp/yolov8.rknn" &&
                  detector_options.detector_manifest == "/tmp/detector.json" &&
                  detector_options.detector_labels == "/tmp/coco80.txt" &&
                  detector_options.detector_class_id == 2 &&
                  std::fabs(detector_options.detector_confidence - 0.30F) <
                      1e-6F &&
                  std::fabs(detector_options.detector_nms - 0.50F) < 1e-6F,
              "complete detector option set is parsed without model inspection");

  const std::vector<std::pair<std::string, std::string>> detector_files{
      {"--detector-model", "/tmp/yolov8.rknn"},
      {"--detector-manifest", "/tmp/detector.json"},
      {"--detector-labels", "/tmp/coco80.txt"}};
  for (int mask = 1; mask < 7; ++mask) {
    std::vector<std::string> partial = valid;
    for (int index = 0; index < 3; ++index) {
      if ((mask & (1 << index)) != 0) {
        partial.push_back(detector_files[index].first);
        partial.push_back(detector_files[index].second);
      }
    }
    expect_throws([&] { parse_options(partial); },
                  "every partial detector file combination must fail");
  }

  std::vector<std::string> unlimited = valid;
  const auto max_frames_position =
      std::find(unlimited.begin(), unlimited.end(), "--max-frames");
  unlimited.erase(max_frames_position, max_frames_position + 2);
  expect_true(parse_options(unlimited).max_frames == 0,
              "omitted max-frames means unlimited");

  const Options help = parse_options({"nanotrack_v3", "--help"});
  expect_true(help.help, "--help succeeds without required arguments");
  expect_true(usage().find("--model-dir") != std::string::npos,
              "usage names model-dir");

  expect_throws(
      [&] {
        std::vector<std::string> missing_video = valid;
        missing_video.erase(missing_video.begin() + 1, missing_video.begin() + 3);
        parse_options(missing_video);
      },
      "missing video must fail");
  expect_throws(
      [&] {
        std::vector<std::string> malformed = valid;
        const auto position = std::find(malformed.begin(), malformed.end(), "--max-frames");
        *(position + 1) = "12x";
        parse_options(malformed);
      },
      "malformed max-frames must fail");
  expect_throws(
      [&] {
        std::vector<std::string> zero = valid;
        const auto position = std::find(zero.begin(), zero.end(), "--max-frames");
        *(position + 1) = "0";
        parse_options(zero);
      },
      "explicit zero max-frames must fail");
  expect_throws(
      [&] {
        std::vector<std::string> reversed = valid;
        const auto position = std::find(reversed.begin(), reversed.end(), "--end-frame");
        *(position + 1) = "159";
        parse_options(reversed);
      },
      "end-frame before start-frame must fail");
  expect_throws(
      [&] {
        std::vector<std::string> malformed = valid;
        const auto position = std::find(malformed.begin(), malformed.end(), "--output-size");
        *(position + 1) = "1920*1080";
        parse_options(malformed);
      },
      "malformed output-size must fail");
  expect_throws(
      [&] {
        std::vector<std::string> unknown = valid;
        unknown.push_back("--unknown");
        unknown.push_back("value");
        parse_options(unknown);
      },
      "unknown option must fail");
  expect_throws(
      [&] {
        std::vector<std::string> invalid_sensor = valid;
        invalid_sensor.insert(invalid_sensor.end(), {"--sensor", "thermal"});
        parse_options(invalid_sensor);
      },
      "unknown sensor must fail");
  for (const std::string& value : {"0", "1", "nan", "inf", "bad"}) {
    expect_throws(
        [&] {
          std::vector<std::string> invalid = valid;
          invalid.insert(invalid.end(), {"--detector-conf", value});
          parse_options(invalid);
        },
        "detector confidence must be finite and in the open unit interval");
  }
  for (const std::string& value : {"0", "1", "-0.1", "nan"}) {
    expect_throws(
        [&] {
          std::vector<std::string> invalid = valid;
          invalid.insert(invalid.end(), {"--detector-nms", value});
          parse_options(invalid);
        },
        "detector NMS must be finite and in the open unit interval");
  }
  expect_throws(
      [&] {
        std::vector<std::string> invalid = valid;
        invalid.insert(invalid.end(), {"--detector-class-id", "-2"});
        parse_options(invalid);
      },
      "detector class IDs below -1 must fail");

  const std::string prefix = "/tmp/nanotrack_v3_cli_test";
  std::ofstream(prefix + ".mp4").put('x');
  std::ofstream(prefix + "_template.rknn").put('x');
  Options files = options;
  files.video = prefix + ".mp4";
  files.model_dir = prefix + "_missing_models";
  expect_throws([&] { validate_input_paths(files); },
                "missing model directory must fail");

  const std::string model_dir = prefix + "_models";
  std::filesystem::create_directories(model_dir);
  for (const char* name : {"template_backbone.rknn", "search_backbone.rknn",
                           "head.rknn", "manifest.json"}) {
    std::ofstream(model_dir + "/" + name).put('x');
  }
  files.model_dir = model_dir;
  files.controller_config = prefix + "_missing_controller.conf";
  expect_throws([&] { validate_input_paths(files); },
                "configured controller file must exist");

  files.controller_config.clear();
  files.detector_model = prefix + "_missing_detector.rknn";
  files.detector_manifest = prefix + "_missing_detector.json";
  files.detector_labels = prefix + "_missing_labels.txt";
  expect_throws([&] { validate_input_paths(files); },
                "configured detector files must exist");

  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "cli tests passed\n";
  return 0;
}
