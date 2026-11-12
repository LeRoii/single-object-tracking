#include "response_baseline.hpp"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
  using namespace nanotrack;
  int failures = 0;
  const auto expect = [&](bool condition, const char* message) {
    if (!condition) {
      std::cerr << "FAIL: " << message << '\n';
      ++failures;
    }
  };
  ControllerConfig config;
  config.occlusion_reference_window = 4;
  config.occlusion_min_reference_samples = 3;
  config.response_psr_drop_ratio = 0.25F;
  config.response_apce_drop_ratio = 0.30F;
  config.occlusion_peak_margin_drop = 0.05F;
  const auto metrics = [](float psr, float apce, float margin) {
    return ResponseMetrics{0.99F, 0.95F, psr, apce, margin};
  };
  const auto probe = metrics(13, 23, 0.25F);
  ResponseBaseline baseline(config);
  baseline.observe_trusted(metrics(100, 24, 0.50F));
  baseline.observe_trusted(metrics(20, 70, 0.20F));
  baseline.observe_trusted(metrics(NAN, 20, 0.40F));
  expect(!baseline.evaluate(probe).reference, "cold until three finite trusted samples");
  baseline.observe_trusted(metrics(16, 30, 0.40F));
  expect(baseline.evaluate(probe).reference.has_value(), "ready at the minimum");
  baseline.observe_trusted(metrics(12, 34, 0.30F));
  baseline.observe_trusted(metrics(24, 20, 0.10F));
  const auto evaluated = baseline.evaluate(probe);
  expect(evaluated.reference && evaluated.reference->psr == 18 &&
             evaluated.reference->apce == 32 && evaluated.reference->peak_margin == 0.25F,
         "independent medians over the four latest samples evict the oldest");
  expect(evaluated.psr_drop && !evaluated.apce_drop && !evaluated.peak_margin_drop,
         "each relative drop is checked against its own reference");
  const auto drop = baseline.evaluate(metrics(15, 20, 0.19F));
  expect(!drop.psr_drop && drop.apce_drop && drop.peak_margin_drop,
         "APCE is relative and peak margin uses an absolute drop");
  baseline.freeze();
  for (int i = 0; i < 10; ++i) baseline.observe_trusted(metrics(1, 1, 0.01F));
  expect(baseline.evaluate(probe).reference->psr == 18,
         "frozen suspected, search and probation windows cannot adapt");
  baseline.resume_tracking();
  baseline.observe_trusted(metrics(30, 90, 0.80F));
  expect(baseline.evaluate(probe).reference->psr == 20,
         "clearing suspicion resumes the existing window");
  baseline.reset_after_recovery();
  baseline.observe_trusted(metrics(5, 5, 0.10F));
  baseline.observe_trusted(metrics(6, 6, 0.15F));
  expect(!baseline.evaluate(probe).reference, "recovery discards all old samples");
  baseline.observe_trusted(metrics(7, 7, 0.20F));
  expect(baseline.evaluate(probe).reference->psr == 6,
         "recovery needs a fresh minimum count");
  return failures != 0;
}
