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
  virtual void stage_recent(const ImageView& image, const BBox& bbox) = 0;
  virtual void commit_recent() = 0;
  virtual void rollback_recent() = 0;
};

}  // namespace nanotrack
