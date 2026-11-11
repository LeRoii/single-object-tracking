# occ_video_1 Post-Holdout C010 Design

## Intent

Make the RK3588 debug tracker satisfy the complete `occ_video_1` tracking
contract after the frozen C009 holdout exposed a visible-period false-loss
episode. The runtime must keep a reasonable motion trajectory and gimbal
control during full occlusion, reacquire promptly after the target reappears,
and then remain on the same target with accurate boxes through frame 270.

This is explicitly post-holdout development. `occ_video_1` is the optimization
standard requested by the user; no later result may be represented as a blind
holdout result.

## Evidence and root cause

C009 already satisfies the original occlusion behavior: frames 172--180 publish
`SEARCHING / observation_valid=false / MOTION_SEARCH`, and the target is
reacquired at frame 185. It fails later because frame 200 has a reliable recent
template and anchor/recent IoU `0.950447`, but the anchor peak margin briefly
drops to `0.016007`. The controller treats a recent-only reliable observation
as requiring confirmation, and the first confirmation failure immediately
starts global search. Because search recovery is detector-seeded, the otherwise
reliable tracker cannot return to tracking until a second detector-confirmed
recovery at frame 216.

The superficially similar frame 169 is distinguishable without frame-specific
logic: the recent template was weak for frames 166--168 before the anchor became
weak. Before frame 200, both templates had a long reliable, geometrically
consistent run. This temporal distinction is the basis of C010.

Two independent tooling defects were also exposed:

- a locked detector candidate that is ineligible only because search age is too
  small creates a default zero-valued `CONFLICT` hint and violates schema 10;
- a holdout scope with no full-occlusion frames is failed by a gate that is not
  applicable to that scope.

## Goals

- Preserve the existing correct full-occlusion search and motion-control
  semantics.
- Prevent an isolated anchor-quality dip from starting global search when a
  mature dual-template history and current geometry corroborate the recent
  observation.
- Keep the bridge bounded: it may cover one isolated frame, but repeated
  anchor degradation must still enter search.
- Preserve detector isolation, two-hit search recovery, generation/stale
  handling, and direct-detector-control prohibition.
- Emit only fully formed search hints; an absent hint remains absent.
- Make evaluator gates scope-aware without weakening gates that are applicable.
- Validate the complete 271-frame result on RK3588 and continue with a new
  evidence-driven design if box accuracy still fails.

## Non-goals and anti-goals

- Do not encode frame numbers, truth boxes, pixel offsets, or video-specific
  coordinates in runtime code or configuration.
- Do not lower the anchor peak-margin threshold merely to pass frame 200.
- Do not allow recent-only tracking without current anchor geometry
  corroboration.
- Do not let detector output directly authorize control.
- Do not relabel post-holdout evidence as blind validation or claim real-gimbal
  acceptance.

## Approaches considered

### Selected: temporal dual-template bridge

Track a consecutive streak of accepted frames on which anchor and recent are
both reliable and geometrically consistent. After at least three such frames,
one current frame may be accepted from the reliable recent template when the
anchor still passes all geometry gates but fails its score gate, the two
template boxes agree, no occlusion trigger is active, no detector hypothesis is
being verified, and recent probation is complete. Consuming the bridge resets
the streak, so a second consecutive anchor failure enters ordinary search.

This separates frame 200 from frame 169 using temporal evidence and changes no
threshold that defines tracker reliability.

### Deferred: continuous detector-assisted geometry correction

Run the detector periodically while tracking and use NanoTrack-verified
candidates to correct center/scale. This could improve geometry but adds compute
load and identity risk. It is considered only if C010 eliminates false search
but the complete evaluator still fails box accuracy.

### Rejected: lower `reliable_peak_margin`

This is a one-number fit that affects the real occlusion transition and the
benign frame alike. It is the least general and least diagnosable option.

## Controller design

Add `template_bridge_min_stable_frames` to `ControllerConfig`:

- default `0`, which disables the new behavior for historical configurations;
- C010 visible configuration value `3`;
- valid range `0..60`;
- included in configuration identity and session metadata.

Maintain `dual_template_reliable_streak_`. Increment it only after an accepted,
ordinary tracking frame where anchor and recent are both reliable and their IoU
meets `template_agreement_iou`. Reset it on initialization, search entry,
reacquisition, recent probation, a non-accepted frame, or any frame without
dual-template reliability.

A template bridge is eligible only when all of the following are true:

- the feature is enabled and the saved streak reaches the configured minimum;
- mode is ordinary tracking with no active loss or motion search;
- recent probation is not active;
- anchor and recent observations are both present;
- recent is reliable;
- anchor is not reliable solely because its score gate failed: its center, area,
  and aspect gates all pass;
- anchor/recent IoU meets `template_agreement_iou`;
- there is no detector verification and no occlusion trigger.

The accepted path is `TEMPLATE_BRIDGE`. It is an observation, not a
reacquisition. It commits the selected tracking box normally, authorizes
observation control, does not mutate templates, and resets the streak after the
frame. A second consecutive single-template frame therefore cannot bridge.

## Hint and evaluator design

When a detector candidate conflicts before a real `SearchHint` exists, record
the conflict only in detector candidate diagnostics and leave
`output.search_hint` empty. When a real plan hint exists, preserve its complete
identity/box/count and set its rejection to `CONFLICT`.

Evaluator gates receive an `applicable` flag. A gate with no relevant frames in
the selected scope is reported as not applicable and excluded from verdict
reduction. This applies to full-occlusion semantics in scope 190--270. The
global exactly-one-reacquisition contract remains applicable because the full
CSV is supplied and a second recovery anywhere is a real failure.

## Acceptance contract

A candidate is complete only when all of the following are observed on RK3588:

- native Debug build and all CTests pass with warnings-as-errors;
- board verifier passes with target `rk3588` and schema 10;
- output video is 1920x1080 with 271 decoded frames and CSV has 271 rows;
- frames 172--180 are `SEARCHING`, observation-invalid, control-valid, and
  `MOTION_SEARCH`;
- the estimated trajectory passes continuity gates;
- exactly one reacquisition occurs in `[182,190)`;
- no later visible frame enters search;
- visible observation per-frame IoU/center gates and mean IoU `>=0.70` pass;
- identity proxy passes and a human review artifact is produced;
- three exact complete runs agree on semantic events and all pass.

If C010 removes the false search but still misses the geometry gate, it is not
complete. The retained full-run evidence becomes the input to a separately
identified C011 geometry design; thresholds are not silently weakened.

## Files and ownership

- Controller/config/runtime/tests live under `rk3566/nanotrack_v3/`.
- Evaluator and its tests live under `scripts/` and `tests/`.
- This task updates only `docs/progress/board-validation.md`; coordinator-owned
  `STATUS.md` and `integration-contract.yaml` remain untouched.
- Generated models, binaries, videos, CSVs, and logs stay outside Git; Git stores
  provenance, hashes, commands, and compact reports.
