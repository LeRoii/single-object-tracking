#include "recovery_candidates.hpp"

#include <iostream>
#include <string>

namespace {
int failures = 0;

void expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
}  // namespace

int main() {
  using namespace nanotrack;
  using Source = RecoveryCandidateSource;
  ControllerConfig config;
  config.detector_max_candidates = 1;
  RecoveryCandidateTable table(config);
  const BBox first{40, 30, 20, 20};
  const BBox moved{42, 30, 20, 20};
  const BBox other{66, 30, 20, 20};
  const BBox third{92, 30, 20, 20};

  table.expire(1, 40, 1);
  const auto key = table.observe(Source::kRecent, first);
  expect(key.has_value(), "reliable recent seeds a candidate");
  expect(table.observe(Source::kSafe, moved) == key && table.size() == 1,
         "safe agreement merges into the same recent identity");
  expect(!table.confirmed(*key), "two sources on one frame count as one hit");
  table.expire(2, 80, 1);
  table.expire(3, 120, 1);
  expect(table.observe(Source::kRecent, moved) == key && table.confirmed(*key),
         "a stable candidate confirms on the third frame with one miss");

  table.expire(4, 160, 1);
  table.expire(5, 200, 1);
  table.expire(6, 240, 1);
  expect(table.size() == 0, "unrefreshed candidates age out in three frames");
  table.expire(7, 280, 1);
  const auto detector = table.observe(Source::kDetector, other, 0.90F);
  const auto overflow = table.observe(Source::kDetector, third, 0.99F);
  expect(detector && overflow && !table.find(*detector) && table.size() == 1 &&
             table.conflicted() && table.select_probe()->key == *overflow,
         "detector overflow keeps the preferred detector and remembers ambiguity");
  const auto recent = table.observe(Source::kRecent, first);
  table.observe(Source::kSafe, other);
  expect(table.size() == 3 && table.conflicted() &&
             table.select_probe()->key == *recent,
         "three disjoint sources are ambiguous and recent outranks detector confidence");
  table.expire(8, 320, 1);
  expect(table.observe(Source::kRecent, moved) == recent && !table.confirmed(*recent),
         "competing identity evidence prevents temporal acceptance");

  table.expire(9, 360, 1);
  table.observe(Source::kRecent, moved);
  table.expire(10, 400, 1);
  expect(table.observe(Source::kRecent, first) == recent && table.size() == 1 &&
             !table.conflicted() && !table.confirmed(*recent) &&
             table.find(*recent)->hits() == 1,
         "rival expiry starts a fresh confirmation window for the surviving candidate");
  table.expire(11, 440, 2);
  expect(table.size() == 0 && !table.conflicted() && !table.find(*recent),
         "generation changes discard all candidates and overflow ambiguity");
  const auto seeded = table.observe(Source::kDetector, first, 0.90F);
  table.expire(12, 480, 2);
  expect(table.observe(Source::kDetector, moved, 0.92F) == seeded &&
             !table.confirmed(*seeded),
         "detector refresh preserves the key and supplies no verification hit");
  expect(table.observe(Source::kRecent, moved) == seeded && !table.confirmed(*seeded),
         "detector identity can collect a NanoTrack verification hit");
  table.expire(13, 520, 2);
  expect(table.observe(Source::kRecent, first) == seeded && table.confirmed(*seeded),
         "the same detector-seeded key confirms after a second NanoTrack frame");

  table.clear();
  expect(table.size() == 0 && !table.find(*seeded) && !table.select_probe(),
         "reset and exception cleanup discard candidate evidence");
  table.expire(14, 560, 2);
  const auto after_reset = table.observe(Source::kRecent, first);
  expect(after_reset && after_reset != seeded && !table.confirmed(*after_reset),
         "cleanup cannot replay a prior key or hit");
  table.expire(15, 560 + config.motion_max_dt_ms + 1, 2);
  expect(!table.find(*after_reset), "a time gap invalidates the prior track");
  table.observe(Source::kRecent, first);
  table.expire(18, 800, 2);
  expect(table.size() == 0, "a gap longer than the frame window clears tracks");
  table.observe(Source::kRecent, first);
  table.expire(17, 840, 2);
  expect(table.size() == 0, "a stale frame clears candidate state");

  table.expire(19, 880, 3);
  const auto nearer = table.observe(Source::kDetector, first, 0.75F, std::nullopt,
                                    0, false, 0.02F);
  const auto farther = table.observe(Source::kDetector, other, 0.99F, std::nullopt,
                                     0, false, 0.12F);
  expect(nearer && !farther && table.select_probe()->key == *nearer &&
             table.conflicted() && !table.confirmed(*nearer),
         "motion proximity can order detector probes without resolving identity");

  {
  ControllerConfig focus_config = config;
  focus_config.detector_max_candidates = 3;
  RecoveryCandidateTable table(focus_config);
  table.expire(20, 920, 3);
  const auto focused = table.observe(Source::kDetector, first, 0.80F,
                                     std::nullopt, 0, false, 0.002F);
  const auto distant = table.observe(Source::kDetector, other, 0.95F,
                                     std::nullopt, 0, false, 0.030F);
  const auto older_recent = table.observe(Source::kRecent, third, 0.90F,
                                          std::nullopt, std::nullopt, true, 0.012F);
  expect(focused && distant && older_recent && table.focus_detector(*focused) &&
             table.select_probe()->key == *focused && !table.conflicted(),
         "a dominant nearby detector is probed before single-hit recent and distant cars");
  table.expire(21, 960, 3);
  table.observe(Source::kRecent, first, 0.90F, std::nullopt,
                std::nullopt, true, 0.002F);
  table.observe(Source::kRecent, third, 0.90F, std::nullopt,
                std::nullopt, true, 0.012F);
  expect(!table.confirmed(*focused) && !table.conflicted(),
         "one NanoTrack hit on the focused candidate does not authorize recovery");
  table.expire(22, 1000, 3);
  table.observe(Source::kRecent, moved, 0.90F, std::nullopt,
                std::nullopt, true, 0.003F);
  expect(table.confirmed(*focused),
         "two consistent focused hits confirm despite one-hit distant alternatives");

  table.clear();
  table.expire(23, 1040, 4);
  const auto disputed = table.observe(Source::kDetector, first, 0.90F,
                                      std::nullopt, 0, false, 0.002F);
  table.observe(Source::kDetector, other, 0.90F,
                std::nullopt, 0, false, 0.030F);
  expect(disputed && table.focus_detector(*disputed),
         "focus starts a fresh detector verification episode");
  for (std::uint64_t frame = 24; frame <= 25; ++frame) {
    table.expire(frame, 1040 + static_cast<std::int64_t>(frame - 23) * 40, 4);
    table.observe(Source::kRecent, first, 0.90F, std::nullopt,
                  std::nullopt, true, 0.002F);
    table.observe(Source::kSafe, other, 0.90F, std::nullopt,
                  std::nullopt, true, 0.030F);
  }
  expect(table.conflicted() && !table.confirmed(*disputed),
         "two tracker-verified identities still block a focused recovery");
  }
  return failures == 0 ? 0 : 1;
}
