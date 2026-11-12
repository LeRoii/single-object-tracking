#pragma once

#include "long_term_types.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace nanotrack {

class RecentSafeTemplates {
 public:
  using Feature = std::vector<float>;

  void initialize(Feature feature);
  // Transaction IDs increase for the lifetime of this state, including resets.
  void prepare(Feature feature, std::uint64_t transaction_id);
  bool commit(std::uint64_t transaction_id) noexcept;
  bool abort(std::uint64_t transaction_id) noexcept;

  const Feature& recent() const noexcept { return recent_; }
  const Feature& safe() const noexcept { return safe_; }
  const Feature& staged() const noexcept { return staged_; }
  bool recent_matches_safe() const noexcept { return recent_matches_safe_; }
  std::optional<std::uint64_t> pending_transaction_id() const noexcept;
  std::optional<std::uint64_t> last_prepared_transaction_id() const noexcept {
    return last_prepared_transaction_id_;
  }

 private:
  Feature recent_;
  Feature safe_;
  Feature staged_;
  std::optional<std::uint64_t> last_prepared_transaction_id_;
  bool recent_matches_safe_ = false;
  bool staged_matches_recent_ = false;
};

template <typename RunHead, typename Observe>
void run_template_heads(const RecentSafeTemplates& templates,
                        TemplateRunMode mode, RunHead run_head,
                        Observe observe) {
  if (templates.recent().empty()) {
    throw std::logic_error("template state has not been initialized");
  }
  switch (mode) {
    case TemplateRunMode::kRecentOnly:
      observe(TemplateKind::kRecent, run_head(templates.recent()));
      return;
    case TemplateRunMode::kStagedAndRecent:
      if (templates.staged().empty()) {
        throw std::logic_error("staged template mode requires a pending feature");
      }
      observe(TemplateKind::kStaged, run_head(templates.staged()));
      observe(TemplateKind::kRecent, run_head(templates.recent()));
      return;
    case TemplateRunMode::kRecentAndSafe: {
      const auto recent = run_head(templates.recent());
      observe(TemplateKind::kRecent, recent);
      if (templates.recent_matches_safe()) {
        observe(TemplateKind::kSafe, recent);
      } else {
        observe(TemplateKind::kSafe, run_head(templates.safe()));
      }
      return;
    }
  }
  throw std::invalid_argument("unknown template run mode");
}

}  // namespace nanotrack
