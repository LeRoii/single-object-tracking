# Visible-Target Threshold and Scale Stabilization Design

**Date:** 2026-11-04
**Scope:** RK3566 NanoTrack V3 visible-light controller behavior only

## Intent

Reduce false loss transitions and rapid width/height oscillation observed in the
ten human-confirmed static-target pod trials. The change must keep the raw
NanoTrack observation available for diagnostics while ensuring that recovery
search enlargement does not directly become a displayed, controlled, or
next-frame reference scale jump.

The retained test logs are:

- `/tmp/stdout.scale-static-20261104.log`, SHA-256
  `7dff6ee141452021acbfae1246108c4b33d64df068df2a21490c1f16ac2864ba`;
- `/tmp/stderr.scale-static-20261104.log`, SHA-256
  `2e02db905df70a6efedb0a0a76151b04a478e4cafd7f027a45d872021fbf4023`.

## Evidence and constraints

- Excluding the seventh trial, 47 of 48 transitions out of accepted tracking
  began with `reliable_peak_margin < 0.06`; only one began primarily because
  `PSR < 4.5`.
- In the seventh trial, several otherwise plausible recovery frames had PSR in
  the approximate range `4.30-4.47`, below the current `4.5` boundary.
- Enlarging the search crop from `1.0` to `1.5` resizes a 50% larger source
  region back to the same `255x255` input. The scale regressor is therefore
  evaluated on a smaller, differently resampled target and its result is not
  geometrically invariant.
- The current visible configuration is explicitly `calibrated=false`. Ten
  positive trials do not provide the negative/distractor/occlusion population
  required to mark it calibrated.
- The tenth trial reacquired the same target. The target class in this test is
  unsupported by the configured detector, so lack of detector hypotheses is
  expected and is not part of this change.
- Infrared thresholds and behavior are not changed.

## Approaches considered

1. **Threshold-only adjustment.** This reduces false state transitions but
   leaves search-scale changes coupled to final scale and can still produce
   large recovery jumps.
2. **Clamp only the adapter/display rectangle.** This makes the overlay look
   stable while motion prediction and the next NanoTrack reference retain a
   different scale. It hides rather than fixes the state inconsistency.
3. **Controller-owned stabilization plus visible threshold staging.** The
   controller publishes and commits one stabilized box to the output, motion
   estimator, and NanoTrack state. This is the selected approach because it
   addresses both the trigger and the amplification path without changing the
   model or detector.

## Selected behavior

### Visible quality candidates

Change only the visible-light candidate defaults and shipped visible config:

| Setting | Current | Candidate |
| --- | ---: | ---: |
| `reliable_psr` | 4.5 | 4.2 |
| `weak_psr` | 4.5 | 4.0 |
| `reliable_peak_margin` | 0.06 | 0.04 |
| `weak_peak_margin` | 0.04 | 0.02 |

Raw-score, APCE, center, area, aspect, template-agreement, and confirmation
thresholds remain unchanged. `calibrated=false` remains unchanged. The values
are diagnostic candidates for another controlled pod trial, not product
calibration or pod acceptance.

### Scale stabilization

Add a visible-light controller setting
`max_accepted_scale_step_ratio=0.05`. Infrared and the generic controller
default retain the existing unrestricted behavior. The effective value must be
validated, included in session metadata, and covered by configuration tests.

For every accepted observation, preserve its center but limit width and height
independently to plus or minus 5% of the last accepted controller box. Use this
same stabilized box for:

- `ControllerOutput::bbox` and therefore the production `rect_show`/final box;
- motion-estimator correction or recovery reinitialization;
- `current_bbox_` and the next search plan;
- `NanoTrackV3::commit_bbox()` through the existing runtime path.

This allows genuine scale change to accumulate over consecutive reliable
frames while preventing a single crop-mode change from creating a large jump.
At 25 FPS, repeated 5% accepted changes can still exceed a threefold scale
change in one second, so the limit does not freeze normal gradual zoom.

On the first unaccepted observation, freeze motion width and height to the last
accepted controller box, clear width/height velocity, and keep predicting only
the center. Repeated `UNCERTAIN`, recovery, or `LOST` frames must retain that
scale. Expanded search crops may still be used to find the target, but their
raw regressed scale is diagnostic evidence only until a reliable observation
is accepted; the accepted result then enters through the same 5% limiter.

### Unchanged safety behavior

- `WEAK` and `REJECTED` observations remain unaccepted and cannot update the
  template or authorize control.
- Recovery still requires the existing two consistent reliable observations
  in the three-frame confirmation window.
- Detector scheduling, target-class gates, center prediction, raw NanoTrack
  dimension guard, initial `64x64` production selection, and centered
  `rect[0]` diagnostic output remain unchanged.
- No cross-compile, deployment, remote process action, or real-pod acceptance
  claim is part of this implementation.

## Verification boundary

Host tests must prove:

1. visible defaults/config expose the four staged thresholds and 5% scale cap;
2. infrared defaults remain unchanged;
3. a rejected frame freezes the last accepted width/height while allowing
   center prediction;
4. repeated recovery predictions cannot collapse toward one pixel;
5. an accepted recovery preserves the observed center but limits both
   dimensions to 5% and commits that same box as internal state;
6. consecutive reliable observations can accumulate genuine scale change;
7. existing controller, runtime, logging, schema, and configuration tests pass.

Host success is not RK3566 runtime, tracking-quality calibration, or pod
acceptance. The next pod trial must compare false loss transitions, scale-step
distribution, identity retention, and negative/distractor behavior against the
retained baseline logs.
