#pragma once

#include "controller_config.hpp"

#include <optional>
#include <vector>

namespace nanotrack {

struct ResponseReference {
  float psr = 0.0F;
  float apce = 0.0F;
  float peak_margin = 0.0F;
};

struct ResponseBaselineEvaluation {
  std::optional<ResponseReference> reference;
  bool psr_drop = false;
  bool apce_drop = false;
  bool peak_margin_drop = false;
};

class ResponseBaseline {
 public:
  explicit ResponseBaseline(const ControllerConfig& config);

  void observe_trusted(const ResponseMetrics& metrics) noexcept;
  ResponseBaselineEvaluation evaluate(const ResponseMetrics& metrics) const;
  void freeze() noexcept { frozen_ = true; }
  void resume_tracking() noexcept { frozen_ = false; }
  void reset_after_recovery() noexcept;

 private:
  std::size_t minimum_samples_;
  float psr_drop_ratio_;
  float apce_drop_ratio_;
  float peak_margin_drop_;
  bool frozen_ = false;
  std::vector<ResponseReference> samples_;
  std::size_t sample_count_ = 0;
  std::size_t next_sample_ = 0;
};

}  // namespace nanotrack
