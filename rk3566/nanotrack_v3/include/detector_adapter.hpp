#pragma once

#include "long_term_types.hpp"

#include <optional>

namespace nanotrack {

class DetectorAdapter {
 public:
  virtual ~DetectorAdapter() = default;
  virtual bool available() const = 0;
  virtual void submit(const ImageView& image, const DetectorRequest& request) = 0;
  virtual std::optional<DetectorBatch> poll() = 0;
};

class NullDetectorAdapter final : public DetectorAdapter {
 public:
  bool available() const override { return false; }
  void submit(const ImageView&, const DetectorRequest&) override {}
  std::optional<DetectorBatch> poll() override { return std::nullopt; }
};

}  // namespace nanotrack
