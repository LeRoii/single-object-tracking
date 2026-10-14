#include "scoped_cleanup.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  int explicit_calls = 0;
  int destructor_reports = 0;
  {
    nanotrack::ScopedCleanup cleanup(
        [&] {
          ++explicit_calls;
          return -7;
        },
        [&](int) { ++destructor_reports; });
    expect_true(cleanup.release() == -7,
                "explicit release returns cleanup status");
    expect_true(cleanup.release() == 0,
                "second explicit release is a no-op");
  }
  expect_true(explicit_calls == 1,
              "failed explicit cleanup is attempted exactly once");
  expect_true(destructor_reports == 0,
              "destructor does not retry explicit cleanup failure");

  int fallback_calls = 0;
  int fallback_status = 0;
  {
    nanotrack::ScopedCleanup cleanup(
        [&] {
          ++fallback_calls;
          return -11;
        },
        [&](int status) { fallback_status = status; });
    expect_true(cleanup.armed(), "new guard is armed");
  }
  expect_true(fallback_calls == 1,
              "destructor performs one fallback cleanup attempt");
  expect_true(fallback_status == -11,
              "destructor reports fallback cleanup failure");

  if (failures != 0) return 1;
  std::cout << "scoped cleanup tests passed\n";
  return 0;
}
