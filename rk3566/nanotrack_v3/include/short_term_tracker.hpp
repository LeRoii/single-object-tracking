#pragma once

#include "long_term_controller.hpp"

#include <vector>

namespace nanotrack {

struct TemplateHeadExecution {
  bool recent = false;
  bool safe = false;
  bool staged = false;
  bool safe_reused_recent = false;
};

class ShortTermTracker {
 public:
  virtual ~ShortTermTracker() = default;
  virtual void initialize(const ImageView& image, const BBox& bbox) = 0;
  virtual std::vector<TrackerObservation> infer(const ImageView& image,
                                                const SearchPlan& plan,
                                                TemplateHeadExecution& heads) = 0;
  virtual void commit_bbox(const BBox& bbox) = 0;
  // Restore an already-validated committed bbox after an interrupted frame.
  // Must undo any geometry side effects of a failed or superseded commit_bbox.
  virtual void restore_bbox(const BBox& bbox) noexcept = 0;
  virtual void prepare_recent(const ImageView& image, const BBox& bbox,
                              std::uint64_t transaction_id) = 0;
  virtual bool commit_recent(std::uint64_t transaction_id) noexcept = 0;
  virtual bool abort_recent(std::uint64_t transaction_id) noexcept = 0;

  // Recovery after an unapplied operation or failed frame. Committed slots
  // and the transaction ID high-water mark are preserved.
  virtual void discard_staged_recent() noexcept = 0;
};

}  // namespace nanotrack
