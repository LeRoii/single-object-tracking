#include "cli.hpp"
#include "controller_config.hpp"
#include "detector_adapter.hpp"
#include "interactive_control.hpp"
#include "long_term_runtime.hpp"
#include "nanotrack_v3.hpp"
#include "tracker_math.hpp"
#include "tracking_log.hpp"
#include "yolo_detector_adapter.hpp"
#include "yolo_rknn_engine.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

nanotrack::ImageView image_view(const cv::Mat& frame) {
  if (frame.empty() || frame.type() != CV_8UC3) {
    throw std::invalid_argument("video frame must be a non-empty BGR8 image");
  }
  return nanotrack::ImageView{frame.data, frame.cols, frame.rows, frame.step,
                              frame.channels()};
}

cv::Scalar state_color(const nanotrack::ControllerOutput& output) {
  if (output.control_valid) return cv::Scalar(0, 255, 0);
  switch (output.mode) {
    case nanotrack::TrackingMode::kTracking: return cv::Scalar(0, 255, 255);
    case nanotrack::TrackingMode::kUncertain: return cv::Scalar(0, 255, 255);
    case nanotrack::TrackingMode::kLocalRecovery: return cv::Scalar(0, 165, 255);
    case nanotrack::TrackingMode::kAssistedRecovery: return cv::Scalar(255, 0, 255);
    case nanotrack::TrackingMode::kLost: return cv::Scalar(0, 0, 255);
  }
  return cv::Scalar(0, 0, 255);
}

cv::Mat render_annotated(const cv::Mat& source,
                         const nanotrack::BBox& bbox,
                         const nanotrack::ControllerOutput& output,
                         int output_width,
                         int output_height) {
  cv::Mat rendered;
  if (source.cols != output_width || source.rows != output_height) {
    cv::resize(source, rendered, cv::Size(output_width, output_height), 0.0,
               0.0, cv::INTER_AREA);
  } else {
    rendered = source.clone();
  }
  const float scale_x = static_cast<float>(output_width) / source.cols;
  const float scale_y = static_cast<float>(output_height) / source.rows;
  const cv::Rect rectangle(
      static_cast<int>(std::lround(bbox.x * scale_x)),
      static_cast<int>(std::lround(bbox.y * scale_y)),
      std::max(1, static_cast<int>(std::lround(bbox.width * scale_x))),
      std::max(1, static_cast<int>(std::lround(bbox.height * scale_y))));
  const cv::Scalar color = state_color(output);
  cv::rectangle(rendered, rectangle, color, 2);
  const std::string label =
      std::string(nanotrack::to_string(output.mode)) + " " +
      nanotrack::to_string(output.quality) +
      (output.control_valid ? " CTRL" : " NO_CTRL");
  cv::putText(rendered, label,
              cv::Point(std::max(0, rectangle.x),
                        std::max(18, rectangle.y - 6)),
              cv::FONT_HERSHEY_SIMPLEX, 0.55, color, 2, cv::LINE_AA);
  return rendered;
}

cv::Mat render_interactive(const cv::Mat& source,
                           const std::optional<nanotrack::ControllerOutput>& output,
                           std::size_t frame_id,
                           bool playing) {
  cv::Mat rendered = output
                         ? render_annotated(source, output->bbox, *output,
                                            source.cols, source.rows)
                         : source.clone();
  const std::string status =
      "frame=" + std::to_string(frame_id) +
      (playing ? " PLAY" : " PAUSE") +
      (output ? "" : " NO_TARGET");
  cv::rectangle(rendered, cv::Rect(0, 0, rendered.cols, 58),
                cv::Scalar(0, 0, 0), cv::FILLED);
  cv::putText(rendered, status, cv::Point(12, 23),
              cv::FONT_HERSHEY_SIMPLEX, 0.60, cv::Scalar(0, 255, 255), 2,
              cv::LINE_AA);
  cv::putText(rendered,
              "N/Right: step  Space: play/pause  R: select/reset  Q/Esc: quit",
              cv::Point(12, 49), cv::FONT_HERSHEY_SIMPLEX, 0.52,
              cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
  return rendered;
}

bool window_visible(const std::string& window_name) noexcept {
  try {
    return nanotrack::interactive_window_visible(
        cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE));
  } catch (const cv::Exception&) {
    return false;
  }
}

void destroy_window(const std::string& window_name) noexcept {
  try {
    cv::destroyWindow(window_name);
  } catch (const cv::Exception&) {
  }
}

void roi_mouse_callback(int event, int x, int y, int, void* userdata) {
  auto* selection = static_cast<nanotrack::InteractiveRoiSelection*>(userdata);
  if (selection == nullptr) return;
  switch (event) {
    case cv::EVENT_LBUTTONDOWN:
      selection->handle_mouse(nanotrack::RoiMouseEvent::kDown, x, y);
      break;
    case cv::EVENT_MOUSEMOVE:
      selection->handle_mouse(nanotrack::RoiMouseEvent::kMove, x, y);
      break;
    case cv::EVENT_LBUTTONUP:
      selection->handle_mouse(nanotrack::RoiMouseEvent::kUp, x, y);
      break;
    default: break;
  }
}

struct RoiSelectionResult {
  std::optional<nanotrack::BBox> bbox;
  bool window_closed = false;
};

RoiSelectionResult select_target_roi(const std::string& window_name,
                                     const cv::Mat& frame) {
  nanotrack::InteractiveRoiSelection selection(frame.cols, frame.rows);
  cv::setMouseCallback(window_name, roi_mouse_callback, &selection);
  while (!selection.finished()) {
    cv::Mat rendered = frame.clone();
    if (const auto rectangle = selection.current_rect()) {
      cv::rectangle(rendered,
                    cv::Rect(rectangle->x, rectangle->y, rectangle->width,
                             rectangle->height),
                    cv::Scalar(0, 255, 255), 2);
    }
    cv::rectangle(rendered, cv::Rect(0, 0, rendered.cols, 30),
                  cv::Scalar(0, 0, 0), cv::FILLED);
    cv::putText(rendered, "Drag ROI, Enter/Space: confirm, Esc/C: cancel",
                cv::Point(12, 22), cv::FONT_HERSHEY_SIMPLEX, 0.55,
                cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    cv::imshow(window_name, rendered);
    const int key = cv::waitKeyEx(20);
    if (!window_visible(window_name)) {
      return RoiSelectionResult{std::nullopt, true};
    }
    selection.handle_key(key);
  }
  try {
    cv::setMouseCallback(window_name, nullptr, nullptr);
  } catch (const cv::Exception&) {
  }
  if (const auto rectangle = selection.confirmed_rect()) {
    return RoiSelectionResult{
        nanotrack::BBox{static_cast<float>(rectangle->x),
                        static_cast<float>(rectangle->y),
                        static_cast<float>(rectangle->width),
                        static_cast<float>(rectangle->height)},
        false};
  }
  return RoiSelectionResult{};
}

std::string fnv1a64_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("cannot hash file: " + path.string());
  std::uint64_t hash = 14695981039346656037ULL;
  char buffer[8192];
  while (stream) {
    stream.read(buffer, sizeof(buffer));
    const std::streamsize count = stream.gcount();
    for (std::streamsize index = 0; index < count; ++index) {
      hash ^= static_cast<unsigned char>(buffer[index]);
      hash *= 1099511628211ULL;
    }
  }
  std::ostringstream encoded;
  encoded << std::hex << std::setw(16) << std::setfill('0') << hash;
  return encoded.str();
}

std::string fnv1a64_text(const std::string& text) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char character : text) {
    hash ^= character;
    hash *= 1099511628211ULL;
  }
  std::ostringstream encoded;
  encoded << std::hex << std::setw(16) << std::setfill('0') << hash;
  return encoded.str();
}

std::string builtin_config_identity(const nanotrack::ControllerConfig& config) {
  std::ostringstream stream;
  stream << config.sensor << '|' << config.calibrated << '|'
         << config.reliable_score << '|' << config.reliable_psr << '|'
         << config.reliable_apce << '|' << config.reliable_peak_margin << '|'
         << config.weak_score << '|' << config.weak_psr << '|'
         << config.weak_apce << '|' << config.weak_peak_margin << '|'
         << nanotrack::to_string(config.detector_target_mode) << '|'
         << config.detector_search_scale << '|'
         << config.detector_subtarget_max_probes << '|'
         << config.detector_confirmation_iou << '|'
         << config.detector_confirmation_center_distance << '|'
         << config.detector_confirmation_max_area_log_change << '|'
         << config.detector_confirmation_max_aspect_log_change << '|'
         << config.uncertain_ms << '|' << config.local_recovery_ms << '|'
         << config.assisted_recovery_ms;
  return stream.str();
}

void write_session_metadata(const std::string& path,
                            const nanotrack::Options& options,
                            const nanotrack::ControllerConfig& config,
                            const nanotrack::NanoTrackV3& tracker,
                            const nanotrack::MonotonicVideoClock& clock,
                            double source_fps,
                            std::size_t tracked_frames,
                            const nanotrack::LatencySummary& latency,
                            const nanotrack::DetectorSessionMetadata& detector) {
  std::ofstream stream(path);
  if (!stream) throw std::runtime_error("cannot open session metadata: " + path);
  const std::filesystem::path manifest =
      std::filesystem::path(options.model_dir) / "manifest.json";
  const std::string config_hash =
      options.controller_config.empty()
          ? fnv1a64_text(builtin_config_identity(config))
          : fnv1a64_file(options.controller_config);
  const double finite_source_fps =
      std::isfinite(source_fps) && source_fps > 0.0 ? source_fps : 0.0;
  nanotrack::TrackingSessionMetadata metadata;
  metadata.video = options.video;
  metadata.model_dir = options.model_dir;
  metadata.manifest_hash_algorithm = "fnv1a64";
  metadata.manifest_hash = fnv1a64_file(manifest);
  metadata.controller_config = options.controller_config.empty()
                                   ? "builtin-defaults"
                                   : options.controller_config;
  metadata.controller_config_hash_algorithm = "fnv1a64";
  metadata.controller_config_hash = config_hash;
  metadata.sensor = config.sensor;
  metadata.calibrated = config.calibrated;
  metadata.detector_target_mode = config.detector_target_mode;
  metadata.rknn_api_version = tracker.api_version();
  metadata.rknn_driver_version = tracker.driver_version();
  metadata.source_fps = finite_source_fps;
  metadata.fps_fallback = clock.fps_fallback();
  metadata.frame_step_ms = clock.frame_step_ms();
  metadata.reliable_score = config.reliable_score;
  metadata.reliable_psr = config.reliable_psr;
  metadata.reliable_apce = config.reliable_apce;
  metadata.reliable_peak_margin = config.reliable_peak_margin;
  metadata.uncertain_ms = config.uncertain_ms;
  metadata.local_recovery_ms = config.local_recovery_ms;
  metadata.assisted_recovery_ms = config.assisted_recovery_ms;
  metadata.tracked_frames = tracked_frames;
  metadata.latency = latency;
  metadata.detector = detector;
  stream << nanotrack::tracking_session_metadata_json(metadata);
  if (!stream) throw std::runtime_error("cannot write session metadata: " + path);
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::vector<std::string> arguments(argv, argv + argc);
    const nanotrack::Options options = nanotrack::parse_options(arguments);
    if (options.help) {
      std::cout << nanotrack::usage();
      return 0;
    }
    nanotrack::validate_input_paths(options);
    nanotrack::ControllerConfig controller_config =
        options.controller_config.empty()
            ? nanotrack::default_controller_config(options.sensor)
            : nanotrack::load_controller_config(options.controller_config);
    if (controller_config.sensor != options.sensor) {
      throw std::invalid_argument("--sensor does not match controller config sensor");
    }

    cv::VideoCapture capture(options.video);
    if (!capture.isOpened()) throw std::runtime_error("cannot open input video");
    for (std::size_t index = 0; index < options.start_frame; ++index) {
      if (!capture.grab()) {
        throw std::runtime_error("start frame is beyond the decodable video");
      }
    }
    cv::Mat frame;
    if (!capture.read(frame) || frame.empty()) {
      throw std::runtime_error("input video has no decodable start frame");
    }
    if (options.has_bbox) {
      nanotrack::validate_bbox(options.bbox, frame.cols, frame.rows);
    }

    const double source_fps = capture.get(cv::CAP_PROP_FPS);
    nanotrack::MonotonicVideoClock video_clock(source_fps);
    std::int64_t timestamp_ms =
        video_clock.next(capture.get(cv::CAP_PROP_POS_MSEC));
    const double output_fps =
        std::isfinite(source_fps) && source_fps > 0.0 ? source_fps : 30.0;
    const int output_width =
        options.output_width > 0 ? options.output_width : frame.cols;
    const int output_height =
        options.output_height > 0 ? options.output_height : frame.rows;
    cv::VideoWriter writer;
    std::ofstream boxes;

    nanotrack::NanoTrackV3 tracker(options.model_dir);
    std::unique_ptr<nanotrack::DetectorAdapter> detector;
    nanotrack::YoloRknnEngine* yolo_engine = nullptr;
    if (options.detector_model.empty()) {
      detector = std::make_unique<nanotrack::NullDetectorAdapter>();
    } else {
      nanotrack::YoloRknnEngineConfig detector_config;
      detector_config.model_path = options.detector_model;
      detector_config.manifest_path = options.detector_manifest;
      detector_config.labels_path = options.detector_labels;
      detector_config.class_id = options.detector_class_id;
      detector_config.confidence_threshold = options.detector_confidence;
      detector_config.nms_threshold = options.detector_nms;
      auto engine =
          std::make_unique<nanotrack::YoloRknnEngine>(detector_config);
      yolo_engine = engine.get();
      detector = std::make_unique<nanotrack::YoloDetectorAdapter>(
          std::move(engine));
    }
    nanotrack::LongTermRuntime runtime(controller_config, tracker, *detector);

    std::vector<float> runtime_latencies;
    std::vector<float> pipeline_latencies;
    std::vector<float> tracker_latencies;
    std::size_t detector_request_count = 0;
    std::size_t detector_complete_count = 0;
    std::size_t detector_reject_count = 0;
    std::size_t detector_error_count = 0;
    std::size_t tracked_count = 0;
    std::size_t source_frame = options.start_frame;
    std::size_t initialization_count = 0;
    bool initialized = false;
    std::optional<nanotrack::ControllerOutput> displayed_output;
    const auto initialize_target = [&](const nanotrack::BBox& bbox) {
      runtime.initialize(image_view(frame), bbox, source_frame, timestamp_ms);
      nanotrack::ControllerOutput initial_output;
      initial_output.frame_id = source_frame;
      initial_output.timestamp_ms = timestamp_ms;
      initial_output.mode = nanotrack::TrackingMode::kTracking;
      initial_output.quality = nanotrack::QualityLevel::kReliable;
      initial_output.bbox = bbox;
      initial_output.control_valid = true;
      initial_output.accepted = true;
      displayed_output = initial_output;
      if (!initialized) {
        if (!options.output_video.empty()) {
          writer.open(options.output_video,
                      cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), output_fps,
                      cv::Size(output_width, output_height));
          if (!writer.isOpened()) {
            throw std::runtime_error("cannot open output video writer");
          }
          writer.write(render_annotated(frame, bbox, initial_output, output_width,
                                        output_height));
        }
        if (!options.output_boxes.empty()) {
          boxes.open(options.output_boxes);
          if (!boxes) throw std::runtime_error("cannot open output CSV");
          boxes << nanotrack::tracking_csv_header() << '\n';
          boxes << nanotrack::tracking_csv_row(nanotrack::make_initial_log_row(
                       source_frame, timestamp_ms, bbox, detector->available(),
                       detector->available() && detector->can_submit(),
                       controller_config.detector_target_mode))
                << '\n';
        }
      }
      initialized = true;
      ++initialization_count;
      std::cout << "INITIALIZED frame=" << source_frame << " bbox=" << bbox.x
                << ',' << bbox.y << ',' << bbox.width << ',' << bbox.height
                << " count=" << initialization_count << '\n';
    };

    if (options.has_bbox) initialize_target(options.bbox);

    constexpr const char* kInteractiveWindow = "NanoTrack V3 RK3566 Debug";
    nanotrack::InteractivePlayback playback;
    if (options.interactive) {
      cv::namedWindow(kInteractiveWindow, cv::WINDOW_NORMAL);
      std::cout << "INTERACTIVE keys=N/Right(step), Space(play/pause), "
                   "R(select/reset), Q/Esc(quit)\n";
    }

    while (true) {
      const bool frame_limit_reached =
          options.max_frames && tracked_count >= options.max_frames;
      if (!options.interactive && frame_limit_reached) break;
      bool advance = !options.interactive;
      if (options.interactive) {
        cv::imshow(kInteractiveWindow,
                   render_interactive(frame, displayed_output, source_frame,
                                      playback.playing()));
        const int key = cv::waitKeyEx(playback.playing() ? 1 : 0);
        advance = playback.handle_key(key);
        if (!window_visible(kInteractiveWindow)) break;
        if (playback.take_selection_request()) {
          const RoiSelectionResult selection =
              select_target_roi(kInteractiveWindow, frame);
          if (selection.window_closed) break;
          if (selection.bbox) {
            nanotrack::validate_bbox(*selection.bbox, frame.cols, frame.rows);
            initialize_target(*selection.bbox);
          } else {
            std::cout << "ROI_SELECTION_CANCELLED frame=" << source_frame
                      << '\n';
          }
          continue;
        }
        if (playback.quit_requested()) break;
        if (!advance) continue;
        if (frame_limit_reached) {
          playback.pause();
          continue;
        }
      }
      if (source_frame >= options.end_frame) break;
      const auto pipeline_start = std::chrono::steady_clock::now();
      if (!capture.read(frame)) break;
      ++source_frame;
      timestamp_ms = video_clock.next(capture.get(cv::CAP_PROP_POS_MSEC));
      if (!initialized) {
        displayed_output.reset();
        continue;
      }
      const nanotrack::RuntimeFrameResult result =
          runtime.process_frame(image_view(frame), source_frame, timestamp_ms);
      if (result.detector.request_submitted) ++detector_request_count;
      if (result.detector.batch_received) {
        ++detector_complete_count;
        if (result.output.detector_status !=
            nanotrack::DetectorBatchStatus::kAccepted) {
          ++detector_reject_count;
        }
      }
      if (!result.detector.error.empty()) ++detector_error_count;
      if (!std::isfinite(result.runtime_ms) || result.runtime_ms <= 0.0F ||
          !std::isfinite(result.tracker_ms) || result.tracker_ms <= 0.0F) {
        throw std::runtime_error("measured tracking latency is invalid");
      }
      if (writer.isOpened()) {
        writer.write(render_annotated(frame, result.output.bbox, result.output,
                                      output_width, output_height));
      }
      displayed_output = result.output;
      const float pipeline_ms = std::chrono::duration<float, std::milli>(
                                    std::chrono::steady_clock::now() -
                                    pipeline_start)
                                    .count();
      if (!std::isfinite(pipeline_ms) || pipeline_ms <= 0.0F) {
        throw std::runtime_error("measured pipeline latency is invalid");
      }
      runtime_latencies.push_back(result.runtime_ms);
      pipeline_latencies.push_back(pipeline_ms);
      tracker_latencies.push_back(result.tracker_ms);
      ++tracked_count;
      nanotrack::TrackingLogRow log_row =
          nanotrack::make_tracking_log_row(result);
      log_row.pipeline_ms = pipeline_ms;
      if (boxes.is_open()) {
        boxes << nanotrack::tracking_csv_row(log_row) << '\n';
      }
    }
    if (options.interactive) destroy_window(kInteractiveWindow);
    if (writer.isOpened()) {
      writer.release();
    }
    if (boxes.is_open()) {
      boxes.close();
    }
    if (!initialized) {
      std::cout << "SUMMARY tracked_frames=0 initialized=0\n";
      return 0;
    }
    if (runtime_latencies.empty() && !options.interactive) {
      throw std::runtime_error("no post-initialization frame tracked");
    }
    float mean_tracker = 0.0F;
    nanotrack::LatencySummary latency;
    if (!runtime_latencies.empty()) {
      const float tracker_sum = std::accumulate(
          tracker_latencies.begin(), tracker_latencies.end(), 0.0F);
      mean_tracker =
          tracker_sum / static_cast<float>(tracker_latencies.size());
      latency =
          nanotrack::summarize_latencies(runtime_latencies, pipeline_latencies);
    }
    nanotrack::DetectorSessionMetadata detector_metadata;
    detector_metadata.enabled = yolo_engine != nullptr;
    detector_metadata.calibrated = false;
    detector_metadata.model_path = options.detector_model;
    detector_metadata.manifest_path = options.detector_manifest;
    detector_metadata.labels_path = options.detector_labels;
    detector_metadata.class_id = options.detector_class_id;
    detector_metadata.confidence_threshold = options.detector_confidence;
    detector_metadata.nms_threshold = options.detector_nms;
    detector_metadata.request_count = detector_request_count;
    detector_metadata.complete_count = detector_complete_count;
    detector_metadata.reject_count = detector_reject_count;
    detector_metadata.error_count = detector_error_count;
    if (yolo_engine != nullptr) {
      detector_metadata.model_sha256 = yolo_engine->model_sha256();
      detector_metadata.manifest_sha256 = yolo_engine->manifest_sha256();
      detector_metadata.labels_sha256 = yolo_engine->labels_sha256();
      detector_metadata.input_tensor_description =
          yolo_engine->input_tensor_description();
      detector_metadata.output_tensor_descriptions =
          yolo_engine->output_tensor_descriptions();
      detector_metadata.rknn_api_version = yolo_engine->api_version();
      detector_metadata.rknn_driver_version = yolo_engine->driver_version();
    }
    const std::string metadata_path = !options.session_metadata.empty()
                                          ? options.session_metadata
                                          : options.output_boxes.empty()
                                                ? std::string()
                                                : options.output_boxes + ".metadata.json";
    if (!metadata_path.empty()) {
      write_session_metadata(metadata_path, options, controller_config, tracker,
                             video_clock, source_fps, tracked_count, latency,
                             detector_metadata);
    }
    std::cout << std::fixed << std::setprecision(3)
              << "SUMMARY tracked_frames=" << tracked_count
              << " mean_tracker_ms=" << mean_tracker
              << " mean_runtime_ms=" << latency.mean_runtime_ms
              << " p50_runtime_ms=" << latency.p50_runtime_ms
              << " p95_runtime_ms=" << latency.p95_runtime_ms
              << " runtime_fps=" << latency.runtime_fps
              << " mean_pipeline_ms=" << latency.mean_pipeline_ms
              << " p50_pipeline_ms=" << latency.p50_pipeline_ms
              << " p95_pipeline_ms=" << latency.p95_pipeline_ms
              << " end_to_end_fps=" << latency.end_to_end_fps
              << " initializations=" << initialization_count
              << " calibrated=" << (controller_config.calibrated ? 1 : 0)
              << " metadata="
              << (metadata_path.empty() ? "disabled" : metadata_path) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << '\n';
    std::cerr << nanotrack::usage();
    return 1;
  }
}
