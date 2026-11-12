#pragma once

#include "long_term_controller.hpp"

#include <stdexcept>

// Controller-only fixtures model successful tracker operations. Runtime tests
// exercise the same acknowledgement with actual slot ownership and failures.
class ResolvedControllerFixture : public nanotrack::LongTermController {
 public:
  using LongTermController::LongTermController;

  nanotrack::ControllerOutput finish_frame(
      const nanotrack::SearchPlan& plan,
      const std::vector<nanotrack::TrackerObservation>& observations,
      const std::optional<nanotrack::DetectorBatch>& batch) {
    auto output = LongTermController::finish_frame(plan, observations, batch);
    if (output.template_transition &&
        !publish_template_transition(output, nanotrack::TemplateTransitionResult::kApplied)) {
      throw std::logic_error("controller fixture could not acknowledge template transition");
    }
    return output;
  }
};
