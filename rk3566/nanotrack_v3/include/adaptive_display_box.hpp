#pragma once

#include "controller_config.hpp"
#include "long_term_controller.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace nanotrack {

struct AdaptiveDisplayDiagnostics {
  DisplayBoxSource source = DisplayBoxSource::kControllerFallback;
  std::optional<int> class_id;
  bool correction_pending = false;
  bool correction_applied = false;
  int confirmation_count = 0;
  std::int64_t refresh_interval_ms = 0;
  bool refresh_paused = false;
  float runtime_p95_ms = 0.0F;
};

class AdaptiveDisplayBox {
 public:
  explicit AdaptiveDisplayBox(const ControllerConfig& config);

  void initialize(const BBox& tracking_bbox,
                  const TargetClassState& target_class,
                  FrameSize frame_size,
                  const ImageView* image = nullptr);
  bool should_request(TrackingPhase phase,
                      std::int64_t timestamp_ms,
                      bool detector_ready) const;
  DetectorRequest make_request(std::uint64_t frame_id,
                               std::int64_t timestamp_ms) const;
  void mark_request_submitted(std::int64_t timestamp_ms);
  void ingest(const DetectorBatch& batch,
              const ControllerOutput& output,
              const TargetClassState& target_class);
  BBox update(const ControllerOutput& output,
              const TargetClassState& target_class,
              AdaptiveDisplayDiagnostics& diagnostics,
              const ImageView* image = nullptr);
  void observe_runtime(float runtime_ms);
  const BBox& current() const { return display_bbox_; }

 private:
  struct EnvelopeTransform {
    float offset_x = 0.0F;
    float offset_y = 0.0F;
    float width_ratio = 1.0F;
    float height_ratio = 1.0F;
  };

  BBox padded(const BBox& bbox, std::optional<int> class_id) const;
  BBox projected(const BBox& tracking_bbox) const;
  BBox smooth_toward(const BBox& target, float center_alpha);
  BBox stabilized_target(const BBox& target);
  void capture_static_anchor(const ImageView& image,
                             const BBox& tracking_bbox);
  bool static_anchor_holds(const ImageView& image,
                           const ControllerOutput& output);
  void set_transform(const BBox& tracking_bbox, const BBox& envelope, float alpha);
  std::optional<BBox> select_candidate(const DetectorBatch& batch,
                                       const BBox& tracking_bbox,
                                       std::optional<int> class_id) const;
  void update_cadence();

  ControllerConfig config_;
  FrameSize frame_size_{};
  bool initialized_ = false;
  bool initialized_from_detector_ = false;
  bool first_update_ = false;
  std::optional<int> class_id_;
  BBox display_bbox_{};
  BBox last_trusted_bbox_{};
  std::deque<BBox> stability_targets_;
  std::optional<BBox> stability_pending_jump_;
  int stability_motion_hold_frames_ = 0;
  int stability_warmup_frames_ = 0;
  struct AnchorPixel {
    int x = 0;
    int y = 0;
    std::uint8_t gray = 0;
    std::uint8_t tile = 0;
  };
  std::vector<AnchorPixel> static_anchor_pixels_;
  int static_anchor_same_frames_ = 0;
  int static_anchor_changed_frames_ = 0;
  bool static_anchor_active_ = false;
  bool static_anchor_retired_ = false;
  BBox static_anchor_box_{};
  EnvelopeTransform transform_{};
  std::optional<BBox> pending_candidate_;
  int pending_confirmations_ = 0;
  bool correction_applied_ = false;
  int reacquisition_blend_remaining_ = 0;
  std::optional<std::int64_t> last_request_ms_;
  std::optional<std::uint64_t> last_display_batch_frame_;
  std::int64_t refresh_interval_ms_ = 200;
  bool refresh_paused_ = false;
  std::deque<float> runtime_samples_;
  float runtime_p95_ms_ = 0.0F;
  int runtime_updates_since_cadence_ = 0;
};

}  // namespace nanotrack
