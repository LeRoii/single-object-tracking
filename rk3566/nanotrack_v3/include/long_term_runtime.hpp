#pragma once

#include "detector_adapter.hpp"
#include "long_term_controller.hpp"
#include "short_term_tracker.hpp"

#include <cstdint>
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
  float tracker_ms = 0.0F;
  float runtime_ms = 0.0F;
  bool tracker_skipped = false;
  TargetClassState target_class{};
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
                                             std::int64_t timestamp_ms);
  RuntimeFrameResult process_frame(const ImageView& image,
                                   std::uint64_t frame_id,
                                   std::int64_t timestamp_ms);

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
  TargetClassRuntimeConfig target_class_config_;
  TargetClassInitializationResult last_initialization_{};
  bool detector_configured_ = false;
  bool initialized_ = false;
  bool frame_open_ = false;
  std::optional<std::uint64_t> staged_template_slot_id_;
  std::optional<TemplateSlotRevocation> pending_template_slot_revocation_;
  bool revocation_waiting_for_ack_ = false;
};

}  // namespace nanotrack
