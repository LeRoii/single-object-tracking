#include "recent_safe_templates.hpp"

#include <algorithm>
#include <cmath>

namespace nanotrack {
namespace {

void validate_feature(const RecentSafeTemplates::Feature& feature) {
  if (feature.empty() ||
      !std::all_of(feature.begin(), feature.end(),
                   [](float value) { return std::isfinite(value); })) {
    throw std::invalid_argument("template feature must be nonempty and finite");
  }
}

}  // namespace

void RecentSafeTemplates::initialize(Feature feature) {
  validate_feature(feature);
  Feature safe = feature;
  recent_.swap(feature);
  safe_.swap(safe);
  staged_.clear();
  recent_matches_safe_ = true;
  staged_matches_recent_ = false;
}

void RecentSafeTemplates::prepare(Feature feature,
                                  std::uint64_t transaction_id) {
  if (recent_.empty()) {
    throw std::logic_error("template state has not been initialized");
  }
  if (!staged_.empty()) {
    throw std::logic_error("a template transaction is already pending");
  }
  if (last_prepared_transaction_id_ &&
      transaction_id <= *last_prepared_transaction_id_) {
    throw std::invalid_argument("stale template transaction id");
  }
  if (feature.size() != recent_.size()) {
    throw std::invalid_argument("template feature size changed");
  }
  validate_feature(feature);
  staged_matches_recent_ = feature == recent_;
  staged_.swap(feature);
  last_prepared_transaction_id_ = transaction_id;
}

std::optional<std::uint64_t>
RecentSafeTemplates::pending_transaction_id() const noexcept {
  return staged_.empty() ? std::nullopt : last_prepared_transaction_id_;
}

bool RecentSafeTemplates::commit(std::uint64_t transaction_id) noexcept {
  if (pending_transaction_id() != transaction_id) return false;
  safe_.swap(recent_);
  recent_.swap(staged_);
  staged_.clear();
  recent_matches_safe_ = staged_matches_recent_;
  staged_matches_recent_ = false;
  return true;
}

bool RecentSafeTemplates::abort(std::uint64_t transaction_id) noexcept {
  if (pending_transaction_id() != transaction_id) return false;
  staged_.clear();
  staged_matches_recent_ = false;
  return true;
}

}  // namespace nanotrack
