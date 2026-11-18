#pragma once

#include "adaptive_display_box.hpp"
#include "detector_adapter.hpp"
#include "long_term_controller.hpp"
#include "short_term_tracker.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace nanotrack {

struct TargetClassRuntimeConfig {
  int requested_class_id = -1;
  // Zero means no verified labels/manifest has been supplied. Automatic
  // initialization stays UNKNOWN and does not invoke the detector in that case.
  std::size_t class_count = 0;
  TargetClassMatchConfig match_config{};
};

enum class InitializationClassMode {
  kConfigured,
  kClickedDetector,
  kUnknown,
};

// The default preserves CLI initialization. Interactive selection can reuse
// its one full-frame detector result without submitting a second request.
struct InitializationClassSelection {
  InitializationClassMode mode = InitializationClassMode::kConfigured;
  std::optional<DetectorBlockingResult> detection;
  std::size_t selected_candidate_index = 0;
  std::string detector_error;
};

struct TargetClassInitializationResult {
  std::uint64_t initialization_index = 0;
  std::uint64_t frame_id = 0;
  std::int64_t timestamp_ms = 0;
  BBox original_roi{};
  int requested_class_id = -1;
  std::optional<std::uint64_t> request_id;
  std::optional<DetectorBatch> batch;
  TargetClassState target_class{};
  DetectorTiming detector_timing{};
  // Wall-clock duration of detect_blocking(), including old-request quiescing.
  float detector_ms = 0.0F;
  std::string detector_error;
  BBox display_bbox{};
};

enum class TemplateSlotRevocationReason {
  kFrameAborted,
  kFrameApplicationFailed,
  kReinitialized,
};

struct TemplateSlotRevocation {
  std::uint64_t slot_id = 0;
  TemplateSlotRevocationReason reason = TemplateSlotRevocationReason::kFrameAborted;
};

const char* to_string(TemplateSlotRevocationReason reason);

struct RuntimeFrameResult {
  SearchPlan plan{};
  std::vector<TrackerObservation> observations;
  TemplateHeadExecution template_heads;
  std::optional<TemplateSlotRevocation> template_slot_revocation;
  ControllerOutput output{};
  DetectorDiagnostics detector{};
  bool suspected_detector_guard_applied = false;
  bool detector_size_output_applied = false;
  bool detector_center_output_applied = false;
  bool search_output_smoothed = false;
  bool detector_tracking_size_confirmed = false;
  bool visual_probe_ran = false;
  bool visual_probe_hit = false;
  bool visual_probe_response_guarded = false;
  int visual_probe_nearby_candidates = 0;
  std::optional<float> visual_probe_nearest_distance_px;
  std::optional<float> visual_probe_brightness_ratio;
  int suspected_detector_guard_hits = 0;
  std::optional<float> suspected_detector_guard_confidence;
  float tracker_ms = 0.0F;
  float runtime_ms = 0.0F;
  bool tracker_skipped = false;
  TargetClassState target_class{};
  AdaptiveDisplayDiagnostics adaptive_display{};
  // Set by the caller only for the paused initialization-frame presentation.
  std::optional<TargetClassInitializationResult> initialization;
};

class LongTermRuntime {
 public:
  LongTermRuntime(const ControllerConfig& config,
                  ShortTermTracker& tracker,
                  DetectorAdapter& detector,
                  const TargetClassRuntimeConfig& target_class_config = {});

  TargetClassInitializationResult initialize(const ImageView& image,
                                             const BBox& initial_bbox,
                                             std::uint64_t frame_id,
                                             std::int64_t timestamp_ms,
                                             InitializationClassSelection selection = {});
  RuntimeFrameResult process_frame(const ImageView& image,
                                   std::uint64_t frame_id,
                                   std::int64_t timestamp_ms,
                                   bool lower_band_crossing = false);

  // After returning a revocation-bearing frame, processing and reinitialization
  // wait until the writer acknowledges that exact slot.
  bool acknowledge_template_slot_revocation(std::uint64_t slot_id) noexcept;

  const LongTermController& controller() const { return controller_; }
  const TargetClassInitializationResult& last_initialization() const {
    return last_initialization_;
  }

 private:
  TemplateTransitionResult apply_template_transition(
      const ImageView& image, const TemplateTransitionRequest& request);
  void abandon_staged_recent(
      std::optional<TemplateSlotRevocationReason> reason = std::nullopt) noexcept;
  static void validate_image(const ImageView& image);

  ShortTermTracker& tracker_;
  DetectorAdapter& detector_;
  LongTermController controller_;
  AdaptiveDisplayBox adaptive_display_;
  TargetClassRuntimeConfig target_class_config_;
  TargetClassInitializationResult last_initialization_{};
  bool detector_configured_ = false;
  bool initialized_ = false;
  bool frame_open_ = false;
  std::optional<std::uint64_t> staged_template_slot_id_;
  std::optional<TemplateSlotRevocation> pending_template_slot_revocation_;
  bool revocation_waiting_for_ack_ = false;
  float visual_suspect_bright_ratio_ = 0.0F;
  bool lower_band_visibility_veto_ = false;
  float visual_suspect_min_confidence_ = 0.40F;
  float visual_suspect_max_center_px_ = 30.0F;
  int visual_suspect_consecutive_frames_ = 2;
  std::deque<float> trusted_brightness_;
  std::optional<std::uint64_t> last_visual_hit_frame_;
  int visual_hit_streak_ = 0;
  bool visual_streak_ambiguous_ = false;
  struct GuardObservation {
    std::uint64_t frame_id = 0;
    BBox recent_bbox{};
  };
  std::deque<GuardObservation> guard_observations_;
  std::optional<std::int64_t> last_guard_request_ms_;
  std::optional<std::uint64_t> last_guard_batch_frame_id_;
  std::optional<std::int64_t> last_guard_source_ms_;
  std::optional<BBox> last_guard_candidate_bbox_;
  std::optional<BBox> published_tracking_bbox_;
  std::optional<BBox> last_published_output_bbox_;
  bool search_output_smoothing_active_ = false;
  float published_center_offset_x_ = 0.0F;
  float published_center_offset_y_ = 0.0F;
  std::optional<BBox> last_supported_published_bbox_;
  std::optional<std::int64_t> last_published_support_ms_;
  std::optional<BBox> pending_tracking_size_candidate_;
  int tracking_size_candidate_hits_ = 0;
  std::optional<BBox> confirmed_tracking_size_candidate_;
  std::optional<std::int64_t> confirmed_tracking_size_source_ms_;
  std::optional<std::uint64_t> last_tracking_size_batch_frame_id_;
  std::optional<float> last_guard_confidence_;
  struct DetectorMotionSupport {
    std::int64_t timestamp_ms = 0;
    BBox tracking_bbox{};
  };
  std::optional<DetectorMotionSupport> last_display_motion_support_;
  int guard_hits_ = 0;
  bool guard_stream_active_ = false;
  bool guard_protection_active_ = false;
  int guarded_visual_misses_ = 0;
  TrackingPhase previous_output_phase_ = TrackingPhase::kInvalid;
};

}  // namespace nanotrack
