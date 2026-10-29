#pragma once

#include "long_term_types.hpp"

#include <optional>

namespace nanotrack {

class DetectorAdapter {
 public:
  virtual ~DetectorAdapter() = default;
  virtual bool available() const = 0;
  virtual bool can_submit() const = 0;
  virtual void submit(const ImageView& image, const DetectorRequest& request) = 0;
  virtual std::optional<DetectorBatch> poll() = 0;
  virtual std::optional<DetectorBlockingResult> detect_blocking(
      const ImageView&, const DetectorRequest&) { return std::nullopt; }
};

class NullDetectorAdapter final : public DetectorAdapter {
 public:
  bool available() const override { return false; }
  bool can_submit() const override { return false; }
  void submit(const ImageView&, const DetectorRequest&) override {}
  std::optional<DetectorBatch> poll() override { return std::nullopt; }
  std::optional<DetectorBlockingResult> detect_blocking(
      const ImageView&, const DetectorRequest&) override { return std::nullopt; }
};

}  // namespace nanotrack
