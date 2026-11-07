# Tracking, Display, and Control BBox Separation Design

## Intent

Prevent the accepted-center display stabilizer from feeding a lagging box back
into NanoTrack while preserving the visibly stable overlay.  The production
adapter must write the real tracking box to `rect[0]` on every active tracking
frame, including motion-predicted frames where `control_valid=false`.

## Goals

- Keep accepted width and height stabilization unchanged.
- Feed the selected observation center, with stabilized dimensions, back into
  NanoTrack and the motion estimator as `tracking_bbox`.
- Keep the existing accepted-center stabilizer as `display_bbox` and retain
  `ControllerOutput::bbox` as its compatibility alias.
- Use `control_bbox == tracking_bbox` in this first closed-loop correction; do
  not add another smoothing parameter yet.
- Write `control_bbox` to production `rect[0]` regardless of `control_valid`.
  The flag remains diagnostic because the current production C interface does
  not expose a separate validity field. Continue publishing `display_bbox`
  through `rect[1]` and `rect_show`.

## Non-goals

- No PSR/APCE threshold changes.
- No model or RKNN artifact changes.
- No new detector behavior.
- No claim of real-gimbal acceptance from host tests or cross-compilation.

## Approaches

1. Keep one smoothed box for all consumers. Rejected because the measured
   5.6-pixel-per-frame correction ceiling caused the tracker search state to
   lag gimbal-induced image motion.
2. Split tracking, display, and control boxes. Selected because it removes the
   feedback lag while preserving the overlay stabilization already accepted by
   the user.
3. Add a new control-specific low-pass filter immediately. Deferred because it
   adds an uncalibrated parameter before the corrected closed loop is measured.

## Constraints and evidence boundary

The Git implementation lives under `rk3566/nanotrack_v3`.  The production C
ABI adapter is maintained in the WSL production tree and is recorded by hash in
`docs/progress/cross-compile.md`.  Tests must prove state/output separation and
runtime feedback.  A successful A6 build proves compilation only; deployment
and real-gimbal validation remain human-only.
