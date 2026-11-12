#include "recent_safe_templates.hpp"
#include "short_term_tracker.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using nanotrack::RecentSafeTemplates;
using nanotrack::TemplateKind;
using nanotrack::TemplateRunMode;
using Feature = RecentSafeTemplates::Feature;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename Exception, typename Operation>
void require_throws(Operation operation, const char* message) {
  try {
    operation();
  } catch (const Exception&) {
    return;
  }
  throw std::runtime_error(message);
}

void expect_slots(const RecentSafeTemplates& templates, const Feature& recent,
                  const Feature& safe, const Feature& staged = {}) {
  require(templates.recent() == recent, "unexpected recent feature");
  require(templates.safe() == safe, "unexpected safe feature");
  require(templates.staged() == staged, "unexpected staged feature");
}

void test_rotation_and_abort() {
  RecentSafeTemplates templates;
  static_assert(noexcept(templates.commit(0)));
  static_assert(noexcept(templates.abort(0)));
  Feature first{1, 2};
  const Feature second{3, 4};
  const Feature third{5, 6};
  templates.initialize(first);
  require(templates.recent().data() != templates.safe().data(),
          "initial committed slots own separate storage");
  first[0] = 10;
  expect_slots(templates, {1, 2}, {1, 2});

  auto source = second;
  templates.prepare(source, 1);
  source[0] = 20;
  expect_slots(templates, {1, 2}, {1, 2}, second);
  require(!templates.commit(2) && !templates.abort(2),
          "mismatched ids must leave a pending transaction intact");
  expect_slots(templates, {1, 2}, {1, 2}, second);
  require(templates.abort(1), "matching abort must discard staged");
  require(!templates.abort(1) && !templates.commit(1),
          "an aborted transaction cannot be resolved again");
  require_throws<std::invalid_argument>([&] { templates.prepare(second, 1); },
                                       "an aborted id cannot be prepared again");
  expect_slots(templates, {1, 2}, {1, 2});

  templates.prepare(second, 2);
  const auto* first_storage = templates.recent().data();
  const auto* second_storage = templates.staged().data();
  require(templates.commit(2), "matching commit must rotate committed slots");
  expect_slots(templates, second, {1, 2});
  require(templates.safe().data() == first_storage &&
              templates.recent().data() == second_storage,
          "commit rotates the existing buffers by swap");
  require(!templates.commit(2) && !templates.abort(2),
          "a committed transaction cannot be resolved again");

  templates.prepare(third, 3);
  require(!templates.commit(2) && !templates.abort(2),
          "a stale transaction cannot resolve newer staged state");
  require(templates.commit(3), "the third transaction must still commit");
  expect_slots(templates, third, second);
  require(templates.recent().data() != templates.safe().data(),
          "rotated committed slots own separate storage");
}

void test_prepare_failure_and_reset() {
  RecentSafeTemplates templates;
  require_throws<std::logic_error>([&] { templates.prepare({1, 2}, 0); },
                                  "prepare requires initialization");
  templates.initialize({1, 2});
  const auto extract = []() -> Feature {
    throw std::runtime_error("feature extraction failed");
  };
  require_throws<std::runtime_error>([&] { templates.prepare(extract(), 0); },
                                    "extraction failure must propagate");
  expect_slots(templates, {1, 2}, {1, 2});
  require(!templates.commit(0), "extraction failure does not stage a feature");
  require_throws<std::invalid_argument>([&] { templates.prepare({1}, 0); },
                                       "prepare requires the initialized shape");
  require_throws<std::invalid_argument>(
      [&] { templates.prepare({1, std::numeric_limits<float>::infinity()}, 0); },
      "prepare requires a finite feature");
  expect_slots(templates, {1, 2}, {1, 2});

  templates.prepare({3, 4}, 0);
  require_throws<std::logic_error>([&] { templates.prepare({5, 6}, 1); },
                                  "a second prepare cannot overwrite pending state");
  require_throws<std::invalid_argument>([&] { templates.initialize({}); },
                                       "initialize requires a nonempty feature");
  expect_slots(templates, {1, 2}, {1, 2}, {3, 4});
  templates.initialize({5, 6});
  require(!templates.commit(0) && !templates.abort(0),
          "reset discards the pending transaction");
  require_throws<std::invalid_argument>([&] { templates.prepare({3, 4}, 0); },
                                       "reset does not make a stale ID reusable");
  expect_slots(templates, {5, 6}, {5, 6});
}

void test_head_requests_and_reuse() {
  require(nanotrack::SearchPlan{}.template_run_mode == TemplateRunMode::kRecentOnly,
          "default plan must request recent only");
  RecentSafeTemplates templates;
  templates.initialize({1});
  std::vector<Feature> calls;
  std::vector<std::pair<TemplateKind, float>> results;
  const auto run = [&](TemplateRunMode mode) {
    calls.clear();
    results.clear();
    nanotrack::run_template_heads(
        templates, mode,
        [&](const Feature& feature) {
          calls.push_back(feature);
          return feature.front();
        },
        [&](TemplateKind kind, float value) { results.emplace_back(kind, value); });
  };
  run(TemplateRunMode::kRecentOnly);
  require(calls == std::vector<Feature>{{1}} &&
              results == decltype(results){{TemplateKind::kRecent, 1}},
          "recent-only requests and emits one head");
  run(TemplateRunMode::kRecentAndSafe);
  require(calls == std::vector<Feature>{{1}} &&
              results == decltype(results){{TemplateKind::kRecent, 1},
                                          {TemplateKind::kSafe, 1}},
          "equal recent/safe reuse one inference and emit both observations");

  require_throws<std::logic_error>([&] { run(TemplateRunMode::kStagedAndRecent); },
                                  "staged mode requires a prepared feature");
  require(calls.empty(), "missing staged data must fail before head inference");
  templates.prepare({2}, 1);
  run(TemplateRunMode::kRecentOnly);
  require(calls == std::vector<Feature>{{1}} &&
              results == decltype(results){{TemplateKind::kRecent, 1}},
          "recent-only inference ignores a pending staged feature");
  run(TemplateRunMode::kStagedAndRecent);
  require(calls == std::vector<Feature>{{2}, {1}} &&
              results == decltype(results){{TemplateKind::kStaged, 2},
                                          {TemplateKind::kRecent, 1}},
          "probation requests staged and committed recent only");
  require(templates.commit(1), "prepare a distinct recent/safe pair");
  run(TemplateRunMode::kRecentAndSafe);
  require(calls == std::vector<Feature>{{2}, {1}} &&
              results == decltype(results){{TemplateKind::kRecent, 2},
                                          {TemplateKind::kSafe, 1}},
          "distinct recent and safe each execute one head");

  templates.prepare({2}, 2);
  require(templates.commit(2), "prepare an equal recent/safe pair after rotation");
  run(TemplateRunMode::kRecentAndSafe);
  require(calls == std::vector<Feature>{{2}} && results.size() == 2,
          "equal generations after rotation also reuse inference");
}

void test_rolling_dimension_bounds() {
  const nanotrack::FrameSize frame{1000, 1000};
  const nanotrack::BBox initial{400, 400, 100, 100};
  const nanotrack::BBox larger{300, 300, 300, 300};
  const auto next = nanotrack::constrain_tracker_observation_dimensions(
      larger, initial, frame);
  const auto later = nanotrack::constrain_tracker_observation_dimensions(
      larger, next, frame);
  require(std::fabs(next.width - 133.0F) < 0.001F &&
              std::fabs(later.width - next.width * 1.33F) < 0.001F &&
              later.height > initial.height * 1.33F,
          "bounded commits can grow past the old initialization envelope");

  const nanotrack::BBox collapse{990, 990, 2, 2};
  const auto bounded = nanotrack::constrain_tracker_observation_dimensions(
      collapse, later, frame);
  require(std::fabs(bounded.width - later.width * 0.75F) < 0.001F &&
              std::fabs(bounded.height - later.height * 0.75F) < 0.001F &&
              bounded.x + bounded.width <= frame.width &&
              bounded.y + bounded.height <= frame.height,
          "rolling anti-collapse bound holds at the frame edge");
}

}  // namespace

int main() {
  try {
    test_rotation_and_abort();
    test_prepare_failure_and_reset();
    test_head_requests_and_reuse();
    test_rolling_dimension_bounds();
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
