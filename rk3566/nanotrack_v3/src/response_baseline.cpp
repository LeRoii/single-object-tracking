#include "response_baseline.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace nanotrack {
namespace {

float median(const std::vector<ResponseReference>& samples,
             std::size_t sample_count, float ResponseReference::*field,
             std::vector<float>& sorted) {
  sorted.clear();
  for (std::size_t i = 0; i < sample_count; ++i) {
    sorted.push_back(samples[i].*field);
  }
  std::sort(sorted.begin(), sorted.end());
  const std::size_t middle = sorted.size() / 2;
  if (sorted.size() % 2 != 0) return sorted[middle];
  return 0.5F * sorted[middle - 1] + 0.5F * sorted[middle];
}

bool relative_drop(float value, float reference, float ratio) {
  return reference > 0.0F && value <= reference * (1.0F - ratio);
}

}  // namespace

ResponseBaseline::ResponseBaseline(const ControllerConfig& config)
    : minimum_samples_(config.occlusion_min_reference_samples),
      psr_drop_ratio_(config.response_psr_drop_ratio),
      apce_drop_ratio_(config.response_apce_drop_ratio),
      peak_margin_drop_(config.occlusion_peak_margin_drop) {
  validate_controller_config(config);
  samples_.resize(config.occlusion_reference_window);
}

void ResponseBaseline::observe_trusted(const ResponseMetrics& metrics) noexcept {
  if (frozen_ || !std::isfinite(metrics.psr) || !std::isfinite(metrics.apce) ||
      !std::isfinite(metrics.peak_margin)) {
    return;
  }
  samples_[next_sample_] = {metrics.psr, metrics.apce, metrics.peak_margin};
  next_sample_ = (next_sample_ + 1) % samples_.size();
  sample_count_ = std::min(sample_count_ + 1, samples_.size());
}

ResponseBaselineEvaluation ResponseBaseline::evaluate(
    const ResponseMetrics& metrics) const {
  ResponseBaselineEvaluation result;
  if (sample_count_ < minimum_samples_) return result;

  std::vector<float> sorted;
  sorted.reserve(sample_count_);
  const ResponseReference reference{
      median(samples_, sample_count_, &ResponseReference::psr, sorted),
      median(samples_, sample_count_, &ResponseReference::apce, sorted),
      median(samples_, sample_count_, &ResponseReference::peak_margin, sorted)};
  result.reference = reference;
  result.psr_drop = relative_drop(metrics.psr, reference.psr, psr_drop_ratio_);
  result.apce_drop = relative_drop(metrics.apce, reference.apce, apce_drop_ratio_);
  result.peak_margin_drop = metrics.peak_margin < reference.peak_margin &&
      reference.peak_margin - metrics.peak_margin >= peak_margin_drop_;
  return result;
}

void ResponseBaseline::reset_after_recovery() noexcept {
  sample_count_ = 0;
  next_sample_ = 0;
  resume_tracking();
}

}  // namespace nanotrack
