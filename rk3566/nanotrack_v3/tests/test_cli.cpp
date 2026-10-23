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

void write_text_file(const std::filesystem::path& path,
                     const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path);
  stream << contents;
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

  std::vector<std::string> interactive = valid;
  const auto bbox_position =
      std::find(interactive.begin(), interactive.end(), "--bbox");
  interactive.erase(bbox_position, bbox_position + 2);
  interactive.push_back("--interactive");
  const Options interactive_options = parse_options(interactive);
  expect_true(interactive_options.interactive,
              "interactive flag enables board GUI mode");
  expect_true(!interactive_options.has_bbox,
              "interactive mode permits mouse initialization without bbox");

  const Options interactive_without_outputs = parse_options(
      {"nanotrack_v3", "--video", "/tmp/input.mp4", "--model-dir",
       "/tmp/models", "--interactive"});
  expect_true(interactive_without_outputs.output_video.empty() &&
                  interactive_without_outputs.output_boxes.empty(),
              "interactive mode defaults to no saved video or CSV");

  expect_throws(
      [&] {
        std::vector<std::string> missing_bbox = valid;
        const auto position =
            std::find(missing_bbox.begin(), missing_bbox.end(), "--bbox");
        missing_bbox.erase(position, position + 2);
        parse_options(missing_bbox);
      },
      "headless mode still requires bbox");

  expect_throws(
      [&] {
        std::vector<std::string> missing_outputs = valid;
        for (const std::string& option : {"--output-video", "--output-boxes"}) {
          const auto position =
              std::find(missing_outputs.begin(), missing_outputs.end(), option);
          missing_outputs.erase(position, position + 2);
        }
        parse_options(missing_outputs);
      },
      "headless mode still requires output paths");

  std::vector<std::string> interactive_with_bbox = valid;
  interactive_with_bbox.push_back("--interactive");
  const Options interactive_bbox_options =
      parse_options(interactive_with_bbox);
  expect_true(interactive_bbox_options.interactive &&
                  interactive_bbox_options.has_bbox,
              "interactive mode may initialize from a supplied bbox");

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

  const std::filesystem::path config_root =
      "/tmp/nanotrack_v3_runtime_config_test";
  std::filesystem::remove_all(config_root);
  const std::filesystem::path config_dir = config_root / "config files";
  const std::filesystem::path full_config = config_dir / "full.conf";
  write_text_file(
      full_config,
      "video=../videos/input sample.mp4\n"
      "bbox=10,20,30,40\n"
      "model_dir=../models\n"
      "output_video=../output/debug.avi\n"
      "output_boxes=../output/debug.csv\n"
      "max_frames=120\n"
      "start_frame=5\n"
      "end_frame=124\n"
      "output_size=428x240\n"
      "interactive=false\n"
      "sensor=visible\n"
      "controller_config=visible-default.conf\n"
      "session_metadata=../output/session.json\n"
      "detector_model=../detector/model.rknn\n"
      "detector_manifest=../detector/manifest.json\n"
      "detector_labels=../detector/labels.txt\n"
      "detector_class_id=0\n"
      "detector_conf=0.30\n"
      "detector_nms=0.50\n");
  const Options configured =
      parse_options({"nanotrack_v3", "--config", full_config.string()});
  expect_true(configured.video ==
                  (config_root / "videos/input sample.mp4").string(),
              "config video is relative to the config directory");
  expect_true(configured.model_dir == (config_root / "models").string() &&
                  configured.controller_config ==
                      (config_dir / "visible-default.conf").string(),
              "config paths normalize relative and absolute locations");
  expect_true(configured.has_bbox && configured.bbox.x == 10.0F &&
                  configured.bbox.height == 40.0F &&
                  configured.max_frames == 120 &&
                  configured.start_frame == 5 && configured.end_frame == 124 &&
                  configured.output_width == 428 &&
                  configured.output_height == 240 && !configured.interactive,
              "all non-path runtime values load from config");
  expect_true(configured.output_video ==
                      (config_root / "output/debug.avi").string() &&
                  configured.output_boxes ==
                      (config_root / "output/debug.csv").string() &&
                  configured.session_metadata ==
                      (config_root / "output/session.json").string(),
              "output paths load from config");
  expect_true(configured.detector_model ==
                      (config_root / "detector/model.rknn").string() &&
                  configured.detector_manifest ==
                      (config_root / "detector/manifest.json").string() &&
                  configured.detector_labels ==
                      (config_root / "detector/labels.txt").string() &&
                  configured.detector_class_id == 0 &&
                  std::fabs(configured.detector_confidence - 0.30F) < 1e-6F &&
                  std::fabs(configured.detector_nms - 0.50F) < 1e-6F,
              "detector settings load as one complete config set");

  const Options overridden = parse_options(
      {"nanotrack_v3", "--config", full_config.string(), "--video",
       "/tmp/override.mp4", "--start-frame", "7", "--interactive"});
  expect_true(overridden.video == "/tmp/override.mp4" &&
                  overridden.start_frame == 7 && overridden.interactive,
              "explicit CLI values override config values");

  const std::filesystem::path interactive_config =
      config_dir / "interactive.conf";
  write_text_file(interactive_config,
                  "video=../videos/input.mp4\n"
                  "model_dir=../models\n"
                  "interactive=true\n");
  const Options configured_interactive = parse_options(
      {"nanotrack_v3", "--config", interactive_config.string()});
  expect_true(configured_interactive.interactive &&
                  !configured_interactive.has_bbox &&
                  configured_interactive.output_video.empty() &&
                  configured_interactive.output_boxes.empty(),
              "interactive config permits mouse initialization and no outputs");

  const std::filesystem::path incomplete_detector =
      config_dir / "incomplete-detector.conf";
  write_text_file(incomplete_detector,
                  "video=../videos/input.mp4\n"
                  "model_dir=../models\n"
                  "interactive=true\n"
                  "detector_model=../detector/model.rknn\n"
                  "detector_manifest=../detector/manifest.json\n");
  expect_throws(
      [&] {
        parse_options(
            {"nanotrack_v3", "--config", incomplete_detector.string()});
      },
      "partial detector config must fail after merge");
  const Options completed_detector = parse_options(
      {"nanotrack_v3", "--config", incomplete_detector.string(),
       "--detector-labels", "/tmp/override-labels.txt"});
  expect_true(completed_detector.detector_labels ==
                  "/tmp/override-labels.txt",
              "CLI may complete a detector triplet from config");

  const std::filesystem::path headless_missing =
      config_dir / "headless-missing.conf";
  write_text_file(headless_missing,
                  "video=../videos/input.mp4\n"
                  "model_dir=../models\n"
                  "interactive=false\n");
  expect_throws(
      [&] {
        parse_options({"nanotrack_v3", "--config", headless_missing.string()});
      },
      "interactive false keeps headless bbox and output requirements");

  const std::vector<std::pair<std::string, std::string>> invalid_configs{
      {"unknown.conf", "video=input.mp4\nunknown=value\n"},
      {"duplicate.conf", "video=input.mp4\nvideo=again.mp4\n"},
      {"malformed.conf", "video input.mp4\n"},
      {"invalid-bool.conf",
       "video=input.mp4\nmodel_dir=models\ninteractive=yes\n"}};
  for (const auto& invalid : invalid_configs) {
    const std::filesystem::path path = config_dir / invalid.first;
    write_text_file(path, invalid.second);
    expect_throws(
        [&] { parse_options({"nanotrack_v3", "--config", path.string()}); },
        "invalid runtime config must fail strictly: " + invalid.first);
  }

  const std::filesystem::path original_working_directory =
      std::filesystem::current_path();
  std::filesystem::current_path(config_root);
  const std::filesystem::path shipped_config =
      std::filesystem::path(NANOTRACK_SOURCE_DIR) /
      "config/interactive-debug.conf";
  const Options shipped =
      parse_options({"nanotrack_v3", "--config", shipped_config.string()});
  std::filesystem::current_path(original_working_directory);
  const std::filesystem::path source_dir = NANOTRACK_SOURCE_DIR;
  expect_true(shipped.interactive && !shipped.has_bbox,
              "shipped debug config starts interactive for mouse selection");
  expect_true(shipped.video ==
                      (source_dir / "data/girl_dance.mp4").string() &&
                  shipped.model_dir == (source_dir / "models").string(),
              "shipped config resolves board paths independent of cwd");
  expect_true(shipped.output_video.empty() && shipped.output_boxes.empty() &&
                  shipped.session_metadata.empty(),
              "shipped debug config disables output persistence by default");
  expect_true(!shipped.detector_model.empty() &&
                  shipped.detector_class_id == 0,
              "shipped debug config enables the person detector candidate");

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
  expect_true(usage().find("--interactive") != std::string::npos,
              "usage documents board GUI mode");
  expect_true(usage().find("--config") != std::string::npos,
              "usage documents runtime config mode");

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
