#pragma once

#include "long_term_controller.hpp"

#include <vector>

namespace nanotrack {

class ShortTermTracker {
 public:
  virtual ~ShortTermTracker() = default;
  virtual void initialize(const ImageView& image, const BBox& bbox) = 0;
  virtual std::vector<TrackerObservation> infer(const ImageView& image,
                                                const SearchPlan& plan) = 0;
  virtual void commit_bbox(const BBox& bbox) = 0;
  virtual void prepare_recent(const ImageView& image, const BBox& bbox,
                              std::uint64_t transaction_id) = 0;
  virtual bool commit_recent(std::uint64_t transaction_id) noexcept = 0;
  virtual bool abort_recent(std::uint64_t transaction_id) noexcept = 0;

  // Compatibility entry points for the runtime before transaction IDs are wired.
  virtual void stage_recent(const ImageView& image, const BBox& bbox) = 0;
  virtual void commit_recent() = 0;
  virtual void rollback_recent() = 0;
};

}  // namespace nanotrack
