#pragma once

#include <utility>

namespace nanotrack {

// Owns one cleanup obligation. The obligation is disarmed before invoking the
// callback, so even a failing or throwing cleanup is never attempted twice.
template <typename Cleanup, typename Reporter>
class ScopedCleanup {
 public:
  ScopedCleanup(Cleanup cleanup, Reporter reporter)
      : cleanup_(std::move(cleanup)), reporter_(std::move(reporter)) {}

  ~ScopedCleanup() noexcept {
    if (!armed_) return;
    try {
      const int status = release();
      reporter_(status);
    } catch (...) {
      // Destructors cannot propagate cleanup/reporting failures.
    }
  }

  ScopedCleanup(const ScopedCleanup&) = delete;
  ScopedCleanup& operator=(const ScopedCleanup&) = delete;
  ScopedCleanup(ScopedCleanup&&) = delete;
  ScopedCleanup& operator=(ScopedCleanup&&) = delete;

  int release() {
    if (!armed_) return 0;
    armed_ = false;
    return cleanup_();
  }

  bool armed() const noexcept { return armed_; }

 private:
  Cleanup cleanup_;
  Reporter reporter_;
  bool armed_ = true;
};

}  // namespace nanotrack
