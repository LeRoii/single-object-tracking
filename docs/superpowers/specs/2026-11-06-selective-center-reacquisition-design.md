# Selective Center Reacquisition Design

**Date:** 2026-11-06
**Scope:** RK3566 NanoTrack V3 acceptance semantics after uncertain frames

## Intent

Remove the periodic center jumps observed after one- or two-frame uncertain
gaps, while preserving immediate center reset for genuine reacquisition. The
pod evidence showed that continuous accepted frames were already stabilized,
but every confirmed acceptance after any gap was marked `reacquired=true` and
therefore bypassed center smoothing and reset motion.

## Goals and boundaries

- Keep ordinary direct acceptance unchanged.
- Treat confirmation in `kTracking` or `kUncertain` as a short confirmation:
  clear loss/confirmation counters, but keep scale history, use bounded center
  correction, correct rather than reinitialize motion, and do not roll back a
  recent template solely because of that short gap.
- Preserve the existing raw-center reset, motion reinitialization, scale
  evidence reset, coast cleanup, and recent-template rollback for confirmed
  `kLocalRecovery`, `kAssistedRecovery`, `kLost`, detector recovery, and
  occlusion-probe recovery.
- Expose which acceptance path was taken in `ControllerOutput` and debug
  telemetry so the next pod run can distinguish direct, short-confirmation,
  local-or-deeper, detector, and occlusion-probe acceptance.
- Keep visible center parameters `0.35/0.25`, all quality/template thresholds,
  scale filtering, detector behavior, and servo output unchanged.
- Do not add adaptive catch-up in this iteration; it remains a separate
  single-variable experiment after the recovery jump is removed.

## Approaches considered

1. **Source-aware acceptance semantics (selected).** Classify by controller
   mode and recovery source. Short tracking/uncertain confirmation stays on the
   normal filtered path; true recovery keeps the existing reset path. This
   matches the controller's states and does not depend on frame rate.
2. **Gap-length rule.** Smooth gaps of at most two frames and reset after longer
   gaps. This is smaller but can misclassify a fast real recovery or a longer
   template-confirmation episode, and its meaning changes with frame rate.
3. **Always smooth recovery.** This removes every jump but can drag a genuinely
   reacquired target toward stale pre-loss motion, violating the existing
   recovery safety boundary.

## Selected behavior

Add an acceptance-path enum with these observable values:

- `kDirect`: reliable observation accepted without confirmation;
- `kShortConfirmation`: confirmation accepted while the plan mode is
  `kTracking` or `kUncertain` and the source is not detector/occlusion;
- `kLocalOrDeeperRecovery`: confirmation accepted from `kLocalRecovery`,
  `kAssistedRecovery`, or `kLost`;
- `kDetectorRecovery`: detector candidate reaches its existing confirmation
  requirement;
- `kOcclusionProbeRecovery`: existing occlusion search/confirm probes succeed.

Only the last three values set `ControllerOutput::reacquired=true` and request
the recovery-reset branch in `accept_observation()`. `kDirect` and
`kShortConfirmation` use the ordinary scale/center/motion correction branch.
All accepted paths still clear `loss_start_ms_`, confirmation buffers,
unreliable counters, and return the controller to `kTracking`.

No new threshold is introduced. The distinction is derived from the existing
`SearchPlan::mode`, detector source, and occlusion-probe branch. This avoids
changing behavior based on an arbitrary number of frames.

## Diagnostics

`ControllerOutput` records the acceptance path on every accepted frame. The
existing `center_filter` debug event adds `acceptance_path` and `reacquired` to
its raw/predicted/correction/final fields. During A6 integration, the production
`AI_TRACK_DIAG` line should expose the same two fields plus the already
available selected raw center, applied correction, and limited flag; that WSL
adapter is not repository-owned product source and must not be treated as the
authoritative implementation.

## Verification

Tests must fail against the current implementation before product changes and
then prove:

1. a one-frame unreliable gap followed by existing confirmation produces
   `kShortConfirmation`, `reacquired=false`, and a bounded/blended final center
   rather than the raw-center snap;
2. the short-confirmation path clears loss state and returns to direct accepted
   tracking on the next reliable frame;
3. local-or-deeper confirmation still reports true reacquisition, emits the
   raw confirmed center once, and reinitializes motion;
4. detector and occlusion-probe confirmation retain their current true-reset
   behavior and report their respective paths;
5. direct acceptance remains `kDirect` and existing compatibility settings
   still emit the selected center exactly;
6. debug telemetry includes acceptance path and reacquired status only for the
   accepted center event;
7. runtime still commits exactly the controller's one final box and the full
   RKNN-independent suite remains green.

## Evidence boundary

Host tests can establish controller semantics and prevent regression, but they
cannot prove visual improvement, target accuracy, or safe tracking under real
pod motion. After review, the exact source commit must be cross-compiled and
deployed as an isolated candidate. A human must repeat the small-target test;
the expected log change is removal of large raw-center snaps after short
uncertain gaps without increasing lost or local-recovery behavior.
