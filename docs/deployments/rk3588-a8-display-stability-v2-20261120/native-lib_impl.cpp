#include "native-lib.h"

#include "control_output_safety.hpp"
#include "control_error_limiter.hpp"
#include "host_tracking_status.hpp"
#include "controller_config.hpp"
#include "long_term_runtime.hpp"
#include "nanotrack_v3.hpp"
#include "production_deployment_config.hpp"
#include "tracker_math.hpp"
#include "tracking_log.hpp"
#include "yolo_detector_adapter.hpp"
#include "yolo_rknn_engine.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr char kDeploymentConfig[] =
    "/app/nanotrack_rk3588_display_stability_v2_20261120/config/production-deployment.conf";
constexpr char kAlgorithmVersion[] = "nanotrack-a8-display-stability-v2";
constexpr char kFrameLog[] =
    "/app/nanotrack_rk3588_display_stability_v2_20261120/log/frames.csv";

struct AlgorithmContext {
  std::unique_ptr<nanotrack::NanoTrackV3> tracker;
  std::unique_ptr<nanotrack::YoloDetectorAdapter> detector;
  std::unique_ptr<nanotrack::LongTermRuntime> runtime;
  cv::Mat bgr_frame;
  int frame_width = 0;
  int frame_height = 0;
  std::size_t frame_stride = 0;
  std::uint64_t frame_id = 0;
  std::int64_t last_timestamp_ms = 0;
  bool task_active = false;
  bool control_limit_enabled = true;
  ControlErrorLimiter control_limiter;
  std::ofstream frame_log;
  nanotrack::DetectorTargetMode detector_target_mode = nanotrack::DetectorTargetMode::kFullObject;
};

std::unique_ptr<AlgorithmContext> g_context;

void clear_outputs(AI_TRACK_INFO* info, RECT_SIZE* rect_show) {
  if (info != nullptr) {
    info->total_num = 0;
    info->car_cnt = 0;
    info->person_cnt = 0;
    info->is_reinit_track = false;
    info->is_track_base_ai = false;
    info->track_box_result = TRACK_RESULT{};
  }
  if (rect_show != nullptr) *rect_show = RECT_SIZE{};
}

bool valid_frame_contract(const AI_TRACK_INFO& info) {
  return info.platform_type == RK_A8 && !info.is_ir_model &&
         info.rgb_img_size.width > 0 && info.rgb_img_size.height > 0;
}

std::int64_t next_timestamp_ms(AlgorithmContext& context) {
  const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
  context.last_timestamp_ms = std::max(now, context.last_timestamp_ms + 1);
  return context.last_timestamp_ms;
}

nanotrack::BBox sanitize_input_bbox(const RECT_SIZE& input, int frame_width,
                                    int frame_height) {
  const std::int64_t raw_left = input.x;
  const std::int64_t raw_top = input.y;
  const std::int64_t raw_right = raw_left + std::max(input.width, 1);
  const std::int64_t raw_bottom = raw_top + std::max(input.height, 1);
  const int left = static_cast<int>(
      std::max<std::int64_t>(0, std::min<std::int64_t>(raw_left, frame_width - 1)));
  const int top = static_cast<int>(
      std::max<std::int64_t>(0, std::min<std::int64_t>(raw_top, frame_height - 1)));
  const int right = static_cast<int>(std::max<std::int64_t>(
      left + 1, std::min<std::int64_t>(raw_right, frame_width)));
  const int bottom = static_cast<int>(std::max<std::int64_t>(
      top + 1, std::min<std::int64_t>(raw_bottom, frame_height)));
  return nanotrack::BBox{static_cast<float>(left), static_cast<float>(top),
                         static_cast<float>(right - left),
                         static_cast<float>(bottom - top)};
}

RECT_SIZE to_rect(const nanotrack::BBox& input, int frame_width,
                  int frame_height) {
  const nanotrack::BBox clipped =
      nanotrack::clip_bbox(input, frame_width, frame_height);
  const int left = std::max(0, std::min(frame_width - 1,
                                       static_cast<int>(std::lround(clipped.x))));
  const int top = std::max(0, std::min(frame_height - 1,
                                      static_cast<int>(std::lround(clipped.y))));
  const int right = std::max(
      left + 1,
      std::min(frame_width,
               static_cast<int>(std::lround(clipped.x + clipped.width))));
  const int bottom = std::max(
      top + 1,
      std::min(frame_height,
               static_cast<int>(std::lround(clipped.y + clipped.height))));
  return RECT_SIZE{left, top, right - left, bottom - top};
}

nanotrack::BBox scaled_search_box(const nanotrack::BBox& bbox, float scale,
                                  int frame_width, int frame_height) {
  const float safe_scale = std::isfinite(scale) ? std::max(1.0F, scale) : 1.0F;
  const float center_x = bbox.x + bbox.width * 0.5F;
  const float center_y = bbox.y + bbox.height * 0.5F;
  const float width = bbox.width * safe_scale;
  const float height = bbox.height * safe_scale;
  return nanotrack::clip_bbox(
      nanotrack::BBox{center_x - width * 0.5F, center_y - height * 0.5F,
                      width, height},
      frame_width, frame_height);
}

const char* accepted_path_name(nanotrack::AcceptedObservationPath path) {
  switch (path) {
    case nanotrack::AcceptedObservationPath::kDirect:
      return "direct";
    case nanotrack::AcceptedObservationPath::kTemplateBridge:
      return "template_bridge";
    case nanotrack::AcceptedObservationPath::kShortConfirmation:
      return "short_confirmation";
    case nanotrack::AcceptedObservationPath::kLocalOrDeeperRecovery:
      return "local_or_deeper_recovery";
    case nanotrack::AcceptedObservationPath::kDetectorRecovery:
      return "detector_recovery";
    case nanotrack::AcceptedObservationPath::kOcclusionProbeRecovery:
      return "occlusion_probe_recovery";
    case nanotrack::AcceptedObservationPath::kSearchReacquisition:
      return "search_reacquisition";
  }
  return "unknown";
}

void set_frame_outputs(AlgorithmContext& context, AI_TRACK_INFO& info,
                       RECT_SIZE& rect_show,
                       const nanotrack::RuntimeFrameResult& result) {
  const RECT_SIZE display_box = to_rect(
      result.output.display_bbox, context.frame_width, context.frame_height);
  const RECT_SIZE tracking_box = to_rect(
      result.output.tracking_bbox, context.frame_width, context.frame_height);
  const nanotrack::BBox raw_box = result.output.selected_observation_bbox
      .value_or(result.observations.empty() ? result.plan.predicted_bbox
                                            : result.observations.front().bbox);
  const RECT_SIZE raw_rect =
      to_rect(raw_box, context.frame_width, context.frame_height);
  const RECT_SIZE legacy_center_rect =
      nanotrack_adapter::centered_control_box(
          tracking_box, context.frame_width, context.frame_height);
  const int real_x = tracking_box.x + tracking_box.width / 2 - context.frame_width / 2;
  const int real_y = tracking_box.y + tracking_box.height / 2 - context.frame_height / 2;
  const bool available = nanotrack_adapter::host_tracking_available(result.output);
  if (result.output.reacquired) context.control_limiter.reset();
  const auto limited = context.control_limit_enabled
      ? context.control_limiter.apply(real_x, real_y, context.frame_width / 2,
                                     context.frame_height / 2, result.output.timestamp_ms,
                                     available)
      : ControlErrorLimiter::Output{real_x, real_y, false};
  RECT_SIZE control_box = tracking_box;
  if (context.control_limit_enabled) {
    control_box.x = std::clamp(context.frame_width / 2 + limited.x - tracking_box.width / 2,
                               0, context.frame_width - tracking_box.width);
    control_box.y = std::clamp(context.frame_height / 2 + limited.y - tracking_box.height / 2,
                               0, context.frame_height - tracking_box.height);
  }
  info.track_box_result.rect[0] = control_box;
  const RECT_SIZE& emitted_control_rect = info.track_box_result.rect[0];
  std::fprintf(stderr,
      "AI_CONTROL_LIMIT frame=%llu enabled=%d available=%d real_err=(%d,%d) "
      "output_err=(%d,%d) limited=%d reacquired=%d phase=%s\n",
      static_cast<unsigned long long>(result.output.frame_id),
      context.control_limit_enabled ? 1 : 0, available ? 1 : 0, real_x, real_y,
      emitted_control_rect.x + emitted_control_rect.width / 2 - context.frame_width / 2,
      emitted_control_rect.y + emitted_control_rect.height / 2 - context.frame_height / 2,
      limited.limited ? 1 : 0, result.output.reacquired ? 1 : 0,
      nanotrack::to_string(result.output.tracking_phase));
  info.track_box_result.rect[1] = display_box;
  info.track_box_result.rect[2] = to_rect(
      result.plan.predicted_bbox, context.frame_width, context.frame_height);
  info.track_box_result.rect[3] =
      to_rect(scaled_search_box(result.plan.predicted_bbox,
                                result.plan.search_scale,
                                context.frame_width, context.frame_height),
              context.frame_width, context.frame_height);
  if (result.plan.detector_candidate_region.has_value()) {
    info.track_box_result.rect[4] =
        to_rect(*result.plan.detector_candidate_region, context.frame_width,
                context.frame_height);
  } else {
    info.track_box_result.rect[4] = RECT_SIZE{};
  }
  info.is_track_base_ai =
      result.output.accepted &&
      result.output.hypothesis_source == nanotrack::HypothesisSource::kDetector;
  info.is_reinit_track = false;
  rect_show = display_box;
  const RECT_SIZE& predicted_rect = info.track_box_result.rect[2];
  const nanotrack::ResponseMetrics& selected = result.output.selected_metrics;
  std::fprintf(
      stderr,
      "AI_TRACK_DIAG frame=%llu state=DO_TRACK mode=%d quality=%d "
      "accepted=%d control_valid=%d phase=%s api_return=%d "
      "raw=(%d,%d,%d,%d) control=(%d,%d,%d,%d) "
      "final=(%d,%d,%d,%d) pred=(%d,%d,%d,%d) "
      "raw_err=(%d,%d) control_err=(%d,%d) final_err=(%d,%d) "
      "selected_score=%.6f selected_pscore=%.6f selected_psr=%.6f "
      "selected_apce=%.6f selected_margin=%.6f center_residual=%.6f "
      "area_log_change=%.6f aspect_log_change=%.6f "
      "observations=%zu source=%d acceptance_path=%s reacquired=%d "
      "center_innovation=(%.3f,%.3f) center_correction=(%.3f,%.3f) "
      "center_limited=%d legacy_center=(%d,%d,%d,%d)\n",
      // "nfo.track_box_result.rect[0]:%d,%d,%d,%d\n",
      static_cast<unsigned long long>(result.output.frame_id),
      static_cast<int>(result.output.mode),
      static_cast<int>(result.output.quality), result.output.accepted ? 1 : 0,
      result.output.control_valid ? 1 : 0,
      nanotrack::to_string(result.output.tracking_phase),
      nanotrack_adapter::host_tracking_available(result.output) ? 1 : 0,
      raw_rect.x, raw_rect.y,
      raw_rect.width, raw_rect.height, emitted_control_rect.x,
      emitted_control_rect.y, emitted_control_rect.width,
      emitted_control_rect.height, display_box.x, display_box.y,
      display_box.width, display_box.height, predicted_rect.x, predicted_rect.y,
      predicted_rect.width, predicted_rect.height,
      raw_rect.x + raw_rect.width / 2 - context.frame_width / 2,
      raw_rect.y + raw_rect.height / 2 - context.frame_height / 2,
      emitted_control_rect.x + emitted_control_rect.width / 2 -
          context.frame_width / 2,
      emitted_control_rect.y + emitted_control_rect.height / 2 -
          context.frame_height / 2,
      display_box.x + display_box.width / 2 - context.frame_width / 2,
      display_box.y + display_box.height / 2 - context.frame_height / 2,
      selected.raw_score, selected.penalized_peak, selected.psr, selected.apce,
      selected.peak_margin, result.output.normalized_center_residual,
      result.output.area_log_change, result.output.aspect_log_change,
      result.observations.size(),
      static_cast<int>(result.output.hypothesis_source),
      accepted_path_name(result.output.accepted_observation_path),
      result.output.reacquired ? 1 : 0, result.output.center_innovation_x,
      result.output.center_innovation_y, result.output.center_correction_x,
      result.output.center_correction_y,
      result.output.center_correction_limited ? 1 : 0, legacy_center_rect.x,
      legacy_center_rect.y, legacy_center_rect.width,
      legacy_center_rect.height);
  for (const nanotrack::TrackerObservation& observation : result.observations) {
    const RECT_SIZE observation_rect = to_rect(
        observation.bbox, context.frame_width, context.frame_height);
    std::fprintf(
        stderr,
        "AI_TRACK_OBS frame=%llu template=%d bbox=(%d,%d,%d,%d) "
        "raw_score=%.6f penalized_peak=%.6f psr=%.6f apce=%.6f "
        "peak_margin=%.6f\n",
        static_cast<unsigned long long>(observation.frame_id),
        static_cast<int>(observation.template_kind), observation_rect.x,
        observation_rect.y, observation_rect.width, observation_rect.height,
        observation.metrics.raw_score, observation.metrics.penalized_peak,
        observation.metrics.psr, observation.metrics.apce,
        observation.metrics.peak_margin);
  }
}

nanotrack::ImageView make_bgr_view(AlgorithmContext& context,
                                   unsigned char* rgb_data) {
  cv::Mat rgb(context.frame_height, context.frame_width, CV_8UC3, rgb_data,
              context.frame_stride);
  cv::cvtColor(rgb, context.bgr_frame, cv::COLOR_RGB2BGR);
  return nanotrack::ImageView{
      context.bgr_frame.data, context.bgr_frame.cols, context.bgr_frame.rows,
      context.bgr_frame.step, context.bgr_frame.channels()};
}

void report_error(const char* operation, const std::exception& error) {
  std::cerr << "AI_TRACKER operation=" << operation
            << " error=" << error.what() << '\n';
}

}  // namespace

extern "C" bool tracker_detector_init(AI_TRACK_INFO* info) {
  clear_outputs(info, nullptr);
  if (info == nullptr || !valid_frame_contract(*info)) return false;

  info->is_bad_vis_model = true;
  info->is_bad_ir_model = false;
  try {
    const nanotrack::ProductionDeploymentConfig deployment_config =
        nanotrack::load_production_deployment_config(kDeploymentConfig);
    auto context = std::make_unique<AlgorithmContext>();
    context->frame_width = info->rgb_img_size.width;
    context->frame_height = info->rgb_img_size.height;
    context->frame_stride = static_cast<std::size_t>(context->frame_width) * 3U;
    std::fprintf(stderr,
                 "AI_TRACK_INIT platform=%d width=%d height=%d stride=%zu\n",
                 static_cast<int>(info->platform_type), context->frame_width,
                 context->frame_height, context->frame_stride);
    const nanotrack::ControllerConfig controller_config =
        nanotrack::load_controller_config(deployment_config.controller_config_path);
    context->detector_target_mode = controller_config.detector_target_mode;
    if (const char* setting = std::getenv("AI_CONTROL_LIMIT")) {
      if (std::string(setting) != "0" && std::string(setting) != "1")
        throw std::runtime_error("AI_CONTROL_LIMIT must be 0 or 1");
      context->control_limit_enabled = std::string(setting) == "1";
    }
    const bool write_header = !std::filesystem::exists(kFrameLog) ||
                             std::filesystem::file_size(kFrameLog) == 0;
    context->frame_log.open(kFrameLog, std::ios::app);
    if (!context->frame_log) throw std::runtime_error("cannot open production frame log");
    if (write_header) context->frame_log << nanotrack::tracking_csv_header() << '\n';
    context->frame_log.flush();
    context->tracker = std::make_unique<nanotrack::NanoTrackV3>(
        deployment_config.tracker_model_dir);

    nanotrack::YoloRknnEngineConfig detector_config;
    detector_config.model_path = deployment_config.detector_model_path;
    detector_config.manifest_path = deployment_config.detector_manifest_path;
    detector_config.labels_path = deployment_config.detector_labels_path;
    detector_config.confidence_threshold = 0.25F;
    detector_config.nms_threshold = 0.45F;
    detector_config.class_id = -1;
    auto engine =
        std::make_unique<nanotrack::YoloRknnEngine>(detector_config);
    const std::size_t detector_class_count = engine->labels().size();
    context->detector = std::make_unique<nanotrack::YoloDetectorAdapter>(
        std::move(engine));

    nanotrack::TargetClassRuntimeConfig target_class_config;
    target_class_config.requested_class_id = -1;
    target_class_config.class_count = detector_class_count;
    context->runtime = std::make_unique<nanotrack::LongTermRuntime>(
        controller_config, *context->tracker, *context->detector,
        target_class_config);
    context->bgr_frame.create(context->frame_height, context->frame_width,
                              CV_8UC3);
    g_context = std::move(context);

    std::snprintf(info->eo_model_ver, sizeof(info->eo_model_ver), "%s",
                  kAlgorithmVersion);
    info->ir_model_ver[0] = '\0';
    info->is_bad_vis_model = false;
    std::fprintf(stderr, "AI_READY source=ba465d4b17bba2946e82fafdf283f6c911293ea8 config=%s csv=%s\n",
                 kDeploymentConfig, kFrameLog);
    return true;
  } catch (const std::exception& error) {
    g_context.reset();
    report_error("tracker_detector_init", error);
    return false;
  }
}

extern "C" bool do_tracker_detector(unsigned char* img_in,
                                     AI_TRACK_INFO* info,
                                     RECT_SIZE* rect_show) {
  clear_outputs(info, rect_show);
  if (info == nullptr || g_context == nullptr) return false;

  AlgorithmContext& context = *g_context;
  if (info->is_ir_model || info->is_ir_change_mode) {
    context.task_active = false;
    context.control_limiter.reset();
    return false;
  }
  if (info->cmd == STATE_TRACK_CANCEL) {
    if (context.task_active) std::fprintf(stderr, "AI_TRACK_CANCEL frame=%llu\n",
        static_cast<unsigned long long>(context.frame_id));
    context.task_active = false;
    context.control_limiter.reset();
    return false;
  }
  if (info->cmd == STATE_NO_ACTION || info->cmd == STATE_AI_DETECT) {
    return false;
  }
  if (!valid_frame_contract(*info) ||
      info->rgb_img_size.width != context.frame_width ||
      info->rgb_img_size.height != context.frame_height || img_in == nullptr ||
      rect_show == nullptr) {
    return false;
  }

  try {
    const nanotrack::ImageView image = make_bgr_view(context, img_in);
    const std::int64_t timestamp_ms = next_timestamp_ms(context);
    if (info->cmd == STATE_INIT_TRACK) {
      const nanotrack::BBox initial_bbox =
          sanitize_input_bbox(info->init_track_box, context.frame_width,
                              context.frame_height);
      const RECT_SIZE initial_rect =
          to_rect(initial_bbox, context.frame_width, context.frame_height);
      const auto initialization = context.runtime->initialize(
          image, initial_bbox, ++context.frame_id, timestamp_ms);
      const auto initial_row = nanotrack::make_initial_log_row(
          context.frame_id, timestamp_ms, initial_bbox, true,
          context.detector->can_submit(), context.detector_target_mode,
          initialization.target_class, initialization.display_bbox);
      context.frame_log << nanotrack::tracking_csv_row(initial_row) << '\n';
      context.frame_log.flush();
      if (!context.frame_log) throw std::runtime_error("production initialization log write failed");
      context.task_active = true;
      context.control_limiter.reset();
      const RECT_SIZE control_rect = nanotrack_adapter::centered_control_box(
          initial_rect, context.frame_width, context.frame_height);
      info->init_track_box = initial_rect;
      // Keep the clicked box dimensions for the virtual control output. The
      // current A8 host overwrites this INIT value after the callback.
      info->track_box_result.rect[0] = context.control_limit_enabled
          ? RECT_SIZE{context.frame_width / 2 - initial_rect.width / 2,
                      context.frame_height / 2 - initial_rect.height / 2,
                      initial_rect.width, initial_rect.height}
          : initial_rect;
      const RECT_SIZE& emitted_control_rect = info->track_box_result.rect[0];
      info->track_box_result.rect[1] = initial_rect;
      info->track_box_result.rect[2] = initial_rect;
      *rect_show = initial_rect;
      std::fprintf(
          stderr,
          "AI_TRACK_DIAG frame=%llu state=INIT real=(%d,%d,%d,%d) "
          "control=(%d,%d,%d,%d) control_err=(%d,%d) "
          "legacy_center=(%d,%d,%d,%d)\n",
          static_cast<unsigned long long>(context.frame_id), initial_rect.x,
          initial_rect.y, initial_rect.width, initial_rect.height,
          emitted_control_rect.x, emitted_control_rect.y,
          emitted_control_rect.width, emitted_control_rect.height,
          emitted_control_rect.x + emitted_control_rect.width / 2 -
              context.frame_width / 2,
          emitted_control_rect.y + emitted_control_rect.height / 2 -
              context.frame_height / 2,
          control_rect.x, control_rect.y, control_rect.width,
          control_rect.height);
      return true;
    }
    if (info->cmd != STATE_DO_TRACK || !context.task_active) return false;

    const nanotrack::RuntimeFrameResult result = context.runtime->process_frame(
        image, ++context.frame_id, timestamp_ms);
    context.frame_log << nanotrack::tracking_csv_row(
        nanotrack::make_tracking_log_row(result)) << '\n';
    context.frame_log.flush();
    if (!context.frame_log) throw std::runtime_error("production frame log write failed");
    if (result.template_slot_revocation) {
      context.runtime->acknowledge_template_slot_revocation(
          result.template_slot_revocation->slot_id);
    }
    set_frame_outputs(context, *info, *rect_show, result);
    // A false host status does not cancel the runtime: subsequent callbacks must
    // continue searching until recovery or an explicit STATE_TRACK_CANCEL.
    return nanotrack_adapter::host_tracking_available(result.output);
  } catch (const std::exception& error) {
    if (info->cmd == STATE_INIT_TRACK) context.task_active = false;
    clear_outputs(info, rect_show);
    report_error("do_tracker_detector", error);
    return false;
  }
}

extern "C" bool sr_model_init(const char* model_path) {
  (void)model_path;
  return false;
}

extern "C" bool sr_infer_frame(const unsigned char* img_in,
                               IMG_SIZE input_size,
                               unsigned char* sr_img_out,
                               IMG_SIZE output_size) {
  (void)img_in;
  (void)input_size;
  (void)sr_img_out;
  (void)output_size;
  return false;
}
