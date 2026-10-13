#include "tracker_math.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace nanotrack {
namespace {

bool finite(float value) { return std::isfinite(value); }

float change(float value) {
  if (!finite(value) || value <= 0.0F) {
    throw std::runtime_error("tracker ratio must be finite and positive");
  }
  return std::max(value, 1.0F / value);
}

float padded_size(float width, float height) {
  const float pad = 0.5F * (width + height);
  const float value = (width + pad) * (height + pad);
  if (!finite(value) || value <= 0.0F) {
    throw std::runtime_error("tracker size must be finite and positive");
  }
  return std::sqrt(value);
}

float parse_float(const std::string& token) {
  std::size_t consumed = 0;
  const float value = std::stof(token, &consumed);
  if (consumed != token.size() || !finite(value)) {
    throw std::invalid_argument("bbox contains a non-finite or malformed value");
  }
  return value;
}

}  // namespace

BBox parse_bbox(const std::string& text) {
  std::stringstream stream(text);
  std::string token;
  std::vector<float> values;
  while (std::getline(stream, token, ',')) {
    if (token.empty()) {
      throw std::invalid_argument("bbox contains an empty field");
    }
    values.push_back(parse_float(token));
  }
  if (values.size() != 4) {
    throw std::invalid_argument("bbox must have exactly four comma-separated fields");
  }
  return BBox{values[0], values[1], values[2], values[3]};
}

void validate_bbox(const BBox& bbox, int frame_width, int frame_height) {
  if (frame_width <= 0 || frame_height <= 0) {
    throw std::invalid_argument("frame dimensions must be positive");
  }
  if (!finite(bbox.x) || !finite(bbox.y) || !finite(bbox.width) ||
      !finite(bbox.height)) {
    throw std::invalid_argument("bbox values must be finite");
  }
  if (bbox.x < 0.0F || bbox.y < 0.0F || bbox.width <= 0.0F ||
      bbox.height <= 0.0F || bbox.x + bbox.width > frame_width ||
      bbox.y + bbox.height > frame_height) {
    throw std::invalid_argument("bbox must be positive and contained in the frame");
  }
}

std::vector<float> make_hann_window(int size) {
  if (size < 2) {
    throw std::invalid_argument("Hann window size must be at least two");
  }
  constexpr float kPi = 3.14159265358979323846F;
  std::vector<float> one_dimensional(static_cast<std::size_t>(size));
  for (int index = 0; index < size; ++index) {
    one_dimensional[static_cast<std::size_t>(index)] =
        0.5F - 0.5F * std::cos(2.0F * kPi * index / (size - 1));
  }
  std::vector<float> window(static_cast<std::size_t>(size * size));
  for (int row = 0; row < size; ++row) {
    for (int column = 0; column < size; ++column) {
      window[static_cast<std::size_t>(row * size + column)] =
          one_dimensional[static_cast<std::size_t>(row)] *
          one_dimensional[static_cast<std::size_t>(column)];
    }
  }
  return window;
}

BBox clip_bbox(const BBox& bbox, int frame_width, int frame_height) {
  if (frame_width <= 0 || frame_height <= 0 || !finite(bbox.x) ||
      !finite(bbox.y) || !finite(bbox.width) || !finite(bbox.height)) {
    throw std::invalid_argument("cannot clip invalid bbox or frame dimensions");
  }
  const float left = std::max(0.0F, std::min(static_cast<float>(frame_width), bbox.x));
  const float top = std::max(0.0F, std::min(static_cast<float>(frame_height), bbox.y));
  const float right = std::max(
      left, std::min(static_cast<float>(frame_width), bbox.x + bbox.width));
  const float bottom = std::max(
      top, std::min(static_cast<float>(frame_height), bbox.y + bbox.height));
  return BBox{left, top, right - left, bottom - top};
}

float percentile_ms(std::vector<float> values, float percentile) {
  if (values.empty()) {
    throw std::invalid_argument("cannot calculate a percentile of no values");
  }
  if (!finite(percentile) || percentile < 0.0F || percentile > 1.0F) {
    throw std::invalid_argument("percentile must be between zero and one");
  }
  for (const float value : values) {
    if (!finite(value)) {
      throw std::invalid_argument("percentile input must be finite");
    }
  }
  std::sort(values.begin(), values.end());
  const float position = percentile * static_cast<float>(values.size() - 1);
  const std::size_t lower = static_cast<std::size_t>(std::floor(position));
  const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
  const float fraction = position - static_cast<float>(lower);
  return values[lower] * (1.0F - fraction) + values[upper] * fraction;
}

std::vector<float> nchw_to_nhwc(const std::vector<float>& input,
                                int channels, int height, int width) {
  if (channels <= 0 || height <= 0 || width <= 0) {
    throw std::invalid_argument("tensor dimensions must be positive");
  }
  const std::size_t expected = static_cast<std::size_t>(channels) *
                               static_cast<std::size_t>(height) *
                               static_cast<std::size_t>(width);
  if (input.size() != expected) {
    throw std::invalid_argument("NCHW tensor element count does not match dimensions");
  }
  std::vector<float> output(expected);
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      for (int channel = 0; channel < channels; ++channel) {
        const std::size_t source = static_cast<std::size_t>(
            (channel * height + row) * width + column);
        const std::size_t destination = static_cast<std::size_t>(
            (row * width + column) * channels + channel);
        output[destination] = input[source];
      }
    }
  }
  return output;
}

DecodeResult decode_head(const std::vector<float>& cls,
                         const std::vector<float>& loc,
                         const TrackerState& state,
                         const TrackerConfig& config,
                         float scale_z,
                         int frame_width,
                         int frame_height) {
  if (config.output_size <= 0 || config.stride <= 0 || !finite(scale_z) ||
      scale_z <= 0.0F) {
    throw std::invalid_argument("invalid tracker configuration or scale");
  }
  const std::size_t count = static_cast<std::size_t>(
      config.output_size * config.output_size);
  if (cls.size() != 2 * count || loc.size() != 4 * count) {
    throw std::invalid_argument("head tensor element counts do not match output grid");
  }
  for (const float value : cls) {
    if (!finite(value)) throw std::invalid_argument("classification output is non-finite");
  }
  for (const float value : loc) {
    if (!finite(value)) throw std::invalid_argument("box output is non-finite");
  }

  const std::vector<float> window =
      config.output_size == 1 ? std::vector<float>{0.0F}
                              : make_hann_window(config.output_size);
  const float target_width_scaled = state.width * scale_z;
  const float target_height_scaled = state.height * scale_z;
  const float target_size = padded_size(target_width_scaled, target_height_scaled);
  const float target_ratio = target_width_scaled / target_height_scaled;
  const int origin = -(config.output_size / 2) * config.stride;

  float best_pscore = -std::numeric_limits<float>::infinity();
  std::size_t best_index = 0;
  float best_score = 0.0F;
  float best_penalty = 0.0F;
  float best_center_x = 0.0F;
  float best_center_y = 0.0F;
  float best_width = 0.0F;
  float best_height = 0.0F;
  std::vector<float> penalized_response(count, 0.0F);

  for (std::size_t index = 0; index < count; ++index) {
    const float background = cls[index];
    const float foreground = cls[count + index];
    const float maximum = std::max(background, foreground);
    const float exp_background = std::exp(background - maximum);
    const float exp_foreground = std::exp(foreground - maximum);
    const float score = exp_foreground / (exp_background + exp_foreground);

    const int row = static_cast<int>(index / config.output_size);
    const int column = static_cast<int>(index % config.output_size);
    const float point_x = static_cast<float>(origin + column * config.stride);
    const float point_y = static_cast<float>(origin + row * config.stride);
    const float x1 = point_x - loc[index];
    const float y1 = point_y - loc[count + index];
    const float x2 = point_x + loc[2 * count + index];
    const float y2 = point_y + loc[3 * count + index];
    const float width = x2 - x1;
    const float height = y2 - y1;
    if (width <= 0.0F || height <= 0.0F) {
      throw std::runtime_error("head produced a non-positive box");
    }
    const float size_change = change(padded_size(width, height) / target_size);
    const float ratio_change = change(target_ratio / (width / height));
    const float penalty =
        std::exp(-(size_change * ratio_change - 1.0F) * config.penalty_k);
    const float pscore = penalty * score * (1.0F - config.window_influence) +
                         window[index] * config.window_influence;
    penalized_response[index] = pscore;
    if (pscore > best_pscore) {
      best_pscore = pscore;
      best_index = index;
      best_score = score;
      best_penalty = penalty;
      best_center_x = 0.5F * (x1 + x2);
      best_center_y = 0.5F * (y1 + y2);
      best_width = width;
      best_height = height;
    }
  }

  const int best_row = static_cast<int>(best_index / config.output_size);
  const int best_column = static_cast<int>(best_index % config.output_size);
  float response_min = std::numeric_limits<float>::infinity();
  double energy_sum = 0.0;
  for (const float value : penalized_response) response_min = std::min(response_min, value);
  for (const float value : penalized_response) {
    const double delta = static_cast<double>(value - response_min);
    energy_sum += delta * delta;
  }
  const double peak_delta = static_cast<double>(best_pscore - response_min);
  const double mean_energy = energy_sum / static_cast<double>(count);
  const float apce = mean_energy > 1e-12
                         ? static_cast<float>((peak_delta * peak_delta) / mean_energy)
                         : 0.0F;

  std::vector<float> sidelobes;
  sidelobes.reserve(count);
  float second_peak = -std::numeric_limits<float>::infinity();
  for (std::size_t index = 0; index < count; ++index) {
    const int row = static_cast<int>(index / config.output_size);
    const int column = static_cast<int>(index % config.output_size);
    if (std::abs(row - best_row) <= 1 && std::abs(column - best_column) <= 1) {
      continue;
    }
    sidelobes.push_back(penalized_response[index]);
    second_peak = std::max(second_peak, penalized_response[index]);
  }
  float psr = 0.0F;
  float peak_margin = best_pscore;
  if (!sidelobes.empty()) {
    double sum = 0.0;
    for (const float value : sidelobes) sum += value;
    const double mean = sum / static_cast<double>(sidelobes.size());
    double squared_error = 0.0;
    for (const float value : sidelobes) {
      const double delta = static_cast<double>(value) - mean;
      squared_error += delta * delta;
    }
    const double standard_deviation =
        std::sqrt(squared_error / static_cast<double>(sidelobes.size()));
    psr = static_cast<float>((static_cast<double>(best_pscore) - mean) /
                             std::max(standard_deviation, 1e-6));
    peak_margin = best_pscore - second_peak;
  }
  if (!finite(psr) || !finite(apce) || !finite(peak_margin)) {
    throw std::runtime_error("response diagnostics are non-finite");
  }

  const float learning_rate = best_penalty * best_score * config.learning_rate;
  TrackerState updated{
      state.center_x + best_center_x / scale_z,
      state.center_y + best_center_y / scale_z,
      (best_width / scale_z) * learning_rate + state.width * (1.0F - learning_rate),
      (best_height / scale_z) * learning_rate + state.height * (1.0F - learning_rate),
  };
  BBox box{updated.center_x - 0.5F * updated.width,
           updated.center_y - 0.5F * updated.height, updated.width, updated.height};
  box = clip_bbox(box, frame_width, frame_height);
  if (box.width <= 0.0F || box.height <= 0.0F) {
    throw std::runtime_error("decoded box is outside the frame");
  }
  updated.center_x = box.x + 0.5F * box.width;
  updated.center_y = box.y + 0.5F * box.height;
  updated.width = box.width;
  updated.height = box.height;
  return DecodeResult{
      box,
      updated,
      best_score,
      ResponseMetrics{best_score, best_pscore, psr, apce, peak_margin}};
}

}  // namespace nanotrack
