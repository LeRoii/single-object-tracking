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

  {
  ControllerConfig supported_config = config;
  supported_config.recovery_conflict_requires_detector = true;
  RecoveryCandidateTable supported(supported_config);
  supported.expire(30, 1200, 5);
  const auto candidate = supported.observe(Source::kDetector, first, 0.90F,
                                            1200, 2, false);
  for (std::uint64_t frame = 31; frame <= 32; ++frame) {
    supported.expire(frame, 1200 + static_cast<std::int64_t>(frame - 30) * 40, 5);
  }
  expect(candidate && supported.observe(Source::kRecent, first, 0.90F,
                                         std::nullopt, std::nullopt, false) == candidate,
         "matching unverified tracker evidence associates the detector candidate");
  supported.expire(33, 1320, 5);
  expect(supported.find(*candidate) &&
             !supported.find(*candidate)->from(Source::kDetector) &&
             supported.find(*candidate)->detector_handoff_timestamp_ms,
         "detector-only rivals expire while the matched candidate keeps a fresh handoff");
  expect(supported.observe(Source::kRecent, first) == candidate &&
             supported.find(*candidate)->has_detector_supported_hit(),
         "first NanoTrack hit binds the still-fresh detector to this identity");
  supported.expire(34, 1360, 5);
  expect(!supported.find(*candidate)->detector_handoff_timestamp_ms &&
             supported.find(*candidate)->has_detector_supported_hit(),
         "detector expiry between two tracker hits preserves only the bound hit");
  expect(supported.observe(Source::kRecent, moved) == candidate &&
             supported.confirmed(*candidate) &&
             supported.find(*candidate)->has_detector_supported_hit(),
         "second NanoTrack hit confirms the supported identity after detector expiry");
  supported.mark_identity_conflict();
  expect(!supported.find(*candidate)->has_detector_supported_hit(),
         "a later identity conflict clears the detector binding and hits");

  supported.clear();
  supported.expire(40, 1600, 6);
  const auto stale = supported.observe(Source::kDetector, first, 0.90F,
                                       1440, 2, false);
  expect(!stale && supported.size() == 0,
         "detector older than the time limit cannot seed a supported hit");
  }
  {
  ControllerConfig three_hit_config = config;
  three_hit_config.recovery_confirmation_required = 3;
  RecoveryCandidateTable three_hit(three_hit_config);
  for (std::uint64_t frame = 1; frame <= 3; ++frame) {
    three_hit.expire(frame, static_cast<std::int64_t>(frame * 40), 7);
    const auto candidate = three_hit.observe(Source::kRecent, first);
    expect(candidate && three_hit.find(*candidate)->hits() == frame &&
               three_hit.confirmed(*candidate) == (frame == 3),
           "three-hit profile confirms only after three tracker observations");
  }
  }
  {
  ControllerConfig triad_config = config;
  triad_config.detector_max_candidates = 3;
  triad_config.recovery_confirmation_required = 3;
  RecoveryCandidateTable triad(triad_config);
  triad.expire(50, 2000, 8);
  const auto target = triad.observe(Source::kDetector, first, 0.70F, 2000, 2,
                                    false, 0.015F);
  triad.observe(Source::kDetector, other, 0.90F, 2000, 2, false, 0.033F);
  triad.observe(Source::kDetector, third, 0.92F, 2000, 2, false, 0.052F);
  expect(target && triad.conflicted() &&
             triad.unique_detector_agreement(first, moved) == target,
         "two tracker templates uniquely identify one of three detector cars");
  expect(triad.focus_detector(*target), "unique triad can focus its detector");
  for (std::uint64_t frame = 51; frame <= 52; ++frame) {
    triad.expire(frame, 2000 + static_cast<std::int64_t>(frame - 50) * 33, 8);
    const auto match = triad.unique_detector_agreement(first, moved);
    expect(match == target, "same detector remains the unique triad match");
    expect(triad.observe(Source::kRecent, first) == target &&
               triad.observe(Source::kSafe, moved) == target,
           "both tracker heads attach to the same detector identity");
    triad.observe_triad(*target);
    expect(triad.triad_confirmed(*target) == (frame == 52),
           "triad confirms after two fresh frames, without weakening the ordinary three-hit gate");
  }
  triad.clear();
  triad.expire(60, 2400, 9);
  const BBox shared{44, 30, 20, 20};
  const BBox rival{48, 30, 20, 20};
  triad.observe(Source::kDetector, first, 0.80F, 2400, 2, false);
  triad.observe(Source::kDetector, rival, 0.85F, 2400, 2, false);
  expect(!triad.unique_detector_agreement(shared, moved),
         "overlapping rival detectors cannot be resolved by the same tracker pair");
  }
  {
  ControllerConfig duplicate_config = config;
  duplicate_config.detector_max_candidates = 3;
  duplicate_config.recovery_confirmation_required = 3;
  RecoveryCandidateTable duplicate(duplicate_config);
  const BBox detector_box{40, 30, 20, 20};
  const BBox tracker_only_box{52, 30, 20, 20};
  const BBox shared_tracker_box{46, 30, 20, 20};
  duplicate.expire(90, 3600, 12);
  const auto target = duplicate.observe(Source::kDetector, detector_box,
                                        0.85F, 3600, 2, false);
  const auto duplicate_key = duplicate.observe(Source::kRecent, tracker_only_box);
  expect(target && duplicate_key && target != duplicate_key &&
             duplicate.size() == 2,
         "a tracker-only hypothesis can coexist with a distinct detector key");
  RecoveryCandidateTable ordinary_ambiguity(duplicate_config);
  ordinary_ambiguity.expire(90, 3600, 12);
  ordinary_ambiguity.observe(Source::kDetector, detector_box,
                             0.85F, 3600, 2, false);
  ordinary_ambiguity.observe(Source::kRecent, tracker_only_box);
  expect(!ordinary_ambiguity.observe(Source::kSafe, shared_tracker_box) &&
             ordinary_ambiguity.conflicted() &&
             std::string(ordinary_ambiguity.conflict_cause()) == "UNREPRESENTED_MATCH",
         "ordinary association must inspect the second matching identity");
  for (std::uint64_t frame = 91; frame <= 92; ++frame) {
    duplicate.expire(frame, 3600 + static_cast<std::int64_t>(frame - 90) * 33, 12);
    expect(duplicate.focus_triad(shared_tracker_box, shared_tracker_box) == target,
           "both tracker heads uniquely focus the one detector key");
    const auto observe_triad = [&](Source source) {
      return duplicate.observe(source, shared_tracker_box, 0.0F,
          std::nullopt, std::nullopt, true, 0.0F, std::nullopt, target);
    };
    expect(observe_triad(Source::kRecent) == target &&
               observe_triad(Source::kSafe) == target && !duplicate.conflicted(),
           "a duplicate tracker-only key cannot veto a unique current-frame triad");
    duplicate.observe_triad(*target);
  }
  expect(duplicate.triad_confirmed(*target),
         "two fresh triads still authorize the ordinary confirmed recovery");

  RecoveryCandidateTable rival_detector(duplicate_config);
  rival_detector.expire(90, 3600, 12);
  rival_detector.observe(Source::kDetector, detector_box, 0.85F, 3600, 2, false);
  rival_detector.observe(Source::kDetector, tracker_only_box, 0.86F,
                         3600, 2, false);
  expect(!rival_detector.focus_triad(shared_tracker_box, shared_tracker_box),
         "two detector identities matching the tracker pair remain ambiguous");
  }
  {
  ControllerConfig overflow_config = config;
  overflow_config.recovery_confirmation_required = 3;
  RecoveryCandidateTable overflow(overflow_config);
  overflow.expire(70, 2800, 10);
  const auto target = overflow.observe(Source::kDetector, first, 0.70F,
                                        2800, 2, false, 0.015F);
  overflow.observe(Source::kDetector, third, 0.95F,
                   2800, 2, false, 0.052F);
  expect(target && overflow.conflicted() &&
             overflow.focus_triad(first, moved) == target,
         "distant detector overflow remains ambiguous until both trackers identify the retained car");
  for (std::uint64_t frame = 71; frame <= 72; ++frame) {
    overflow.expire(frame, 2800 + static_cast<std::int64_t>(frame - 70) * 33, 10);
    if (frame == 72) {
      overflow.observe(Source::kDetector, third, 0.95F, 2800 + 66, 2, false);
    }
    expect(overflow.focus_triad(first, moved) == target,
           "fresh triad evidence rechecks detector overflow without clearing a verified hit");
    overflow.observe(Source::kRecent, first);
    overflow.observe(Source::kSafe, moved);
    overflow.observe_triad(*target);
    expect(overflow.triad_confirmed(*target) == (frame == 72),
           "two detector-supported tracker frames resolve only distant overflow");
  }
  overflow.clear();
  overflow.expire(80, 3200, 11);
  const BBox shared{44, 30, 20, 20};
  const BBox rival{48, 30, 20, 20};
  overflow.observe(Source::kDetector, first, 0.70F, 3200, 2, false, 0.015F);
  overflow.observe(Source::kDetector, rival, 0.95F, 3200, 2, false, 0.052F);
  expect(!overflow.focus_triad(shared, moved) && overflow.conflicted(),
         "an overflow detector overlapping either tracker still vetoes identity");
  }
  return failures == 0 ? 0 : 1;
}
