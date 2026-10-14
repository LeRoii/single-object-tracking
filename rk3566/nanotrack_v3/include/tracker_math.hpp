#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nanotrack {

struct BBox {
  float x;
  float y;
  float width;
  float height;
};

struct TrackerState {
  float center_x;
  float center_y;
  float width;
  float height;
};

struct TrackerConfig {
  int stride = 16;
  int output_size = 15;
  float penalty_k = 0.138F;
  float window_influence = 0.455F;
  float learning_rate = 0.348F;
};

struct ResponseMetrics {
  float raw_score = 0.0F;
  float penalized_peak = 0.0F;
  float psr = 0.0F;
  float apce = 0.0F;
  float peak_margin = 0.0F;
};

struct DecodeResult {
  BBox bbox;
  TrackerState state;
  float score;
  ResponseMetrics metrics;
};

BBox parse_bbox(const std::string& text);
void validate_bbox(const BBox& bbox, int frame_width, int frame_height);
std::vector<float> make_hann_window(int size);
BBox clip_bbox(const BBox& bbox, int frame_width, int frame_height);
float percentile_ms(std::vector<float> values, float percentile);
std::vector<float> nchw_to_nhwc(const std::vector<float>& input,
                                int channels,
                                int height,
                                int width);
DecodeResult decode_head(const std::vector<float>& cls,
                         const std::vector<float>& loc,
                         const TrackerState& state,
                         const TrackerConfig& config,
                         float scale_z,
                         int frame_width,
                         int frame_height);
std::optional<DecodeResult> try_decode_head(const std::vector<float>& cls,
                                            const std::vector<float>& loc,
                                            const TrackerState& state,
                                            const TrackerConfig& config,
                                            float scale_z,
                                            int frame_width,
                                            int frame_height);

}  // namespace nanotrack
