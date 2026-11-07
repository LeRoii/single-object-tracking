# Visible Scale Smoothing and Recovery Center Gate Design

**Date:** 2026-11-04
**Scope:** RK3566 NanoTrack V3 visible-light controller behavior

## Intent

Reduce high-frequency width/height reversals while still allowing a real,
persistent scale change to be followed gradually. Reject ordinary motion-path
recovery observations whose center jumps too far from the motion prediction.

This change deliberately does not add image-edge, partial-visibility, or
out-of-view handling. The visible quality thresholds remain
`4.2/4.0` PSR and `0.04/0.02` peak margin.

## Evidence

The retained pod trial logs are:

- `/tmp/stdout.visible-scale-20261104.log`, SHA-256
  `f712859acd84d3c6e2693ac07b523f39c6ae7f09fc560a67b2601457eeea0f1c`;
- `/tmp/stderr.visible-scale-20261104.log`, SHA-256
  `94d4a168bee0f11ef7fc32a3c4bb62a44ff48dfbff1582cda00553631b5abb3a`.

After excluding the user-identified end of the sixth trial where the target
left the image, final width changed on 56.6% of adjacent frames, with a median
one-pixel change, two-pixel P95, four-pixel maximum, and 866 direction
reversals. The existing 5% limiter reduces large jumps but passes small
accepted-frame oscillations directly.

For accepted frames following an unaccepted interval, the maximum in-view
normalized center residual was `0.452925`. The anomalous sixth-trial recovery
had residual `1.316290` and caused an approximately 78-pixel output-center
jump. The existing evaluation gate is multiplied by recovery search scale, so
it becomes less restrictive precisely when search expands.

## Approaches considered

1. **Deadband plus exponential smoothing.** Suppress sub-pixel/small-pixel
   noise and move only partway toward a bounded observation on each accepted
   frame. Persistent changes still converge. This is selected.
2. **Three-frame median.** Strong single-frame rejection, but adds fixed delay
   and step-like changes.
3. **Same-direction trend confirmation.** Responsive after confirmation, but
   requires more state and has awkward behavior around zero trend.

## Selected behavior

### Accepted scale

Add three observable controller settings:

- `accepted_scale_deadband_ratio=0.02` for visible light;
- `accepted_scale_smoothing_alpha=0.10` for visible light;
- retain `max_accepted_scale_step_ratio=0.05` as the outer bound.

Generic and infrared behavior remain compatible: deadband `0`, smoothing
alpha `1`, and their existing unrestricted scale-step setting.

For width and height independently on every accepted observation:

1. Compare the raw observed dimension with the current accepted dimension.
2. If the relative difference is at most 2%, keep the current dimension.
3. Otherwise clamp the raw dimension to the existing plus/minus 5% interval.
4. Move 10% of the distance from the current dimension toward that clamped
   target.
5. Rebuild the box around the observed center with the existing frame-safe
   centering helper.

Thus a visible dimension changes by at most 0.5% per accepted frame. A
persistent real scale change continues to converge until it is within the 2%
deadband; for the observed 48-to-64-pixel boxes that residual is about one
pixel. Nonaccepted frames retain the existing frozen scale behavior.

A simplified replay of the retained accepted/raw widths with these values
reduced rounded width-changing frames from 56.6% to approximately 14.9%, the
maximum rounded step from four pixels to one, and direction reversals from 866
to approximately 94. This is parameter-selection evidence only, not a true
controller replay or accuracy proof.

### Ordinary recovery center gate

Add `max_reacquisition_center_distance=0.75` for visible light. A value of
`0` is the generic/infrared compatibility sentinel meaning that the extra
gate is disabled.

For the ordinary motion-hypothesis recovery path, including template-conflict
confirmation, a Reliable observation may enter the existing two-of-three
confirmation queue only when its already-computed normalized center residual
against the motion-predicted box is at most `0.75`. A failed fixed gate clears
pending ordinary confirmations and remains unaccepted. The gate is not
multiplied by search scale.

Detector confirmation and occlusion-hold probe confirmation keep their own
existing geometry gates and are not changed.

## Configuration and observability

The three effective settings must be parsed, finite-validated, included in
built-in configuration identity and session metadata, serialized to JSON, and
required by the board evidence verifier. Validation ranges are:

- deadband ratio: `[0, 1)`;
- smoothing alpha: `(0, 1]`;
- reacquisition center distance: `0` sentinel or a positive finite value.

The shipped visible config contains all three values. No threshold,
detector, image-size, adapter control-box, or infrared value changes.

## Verification boundary

Host tests must prove:

1. visible and shipped configuration expose the exact three values while
   generic/infrared retain compatibility behavior;
2. invalid and non-finite values are rejected and metadata/schema include the
   effective values;
3. a one-frame small accepted scale oscillation remains inside the deadband;
4. persistent larger scale changes converge gradually, never exceed the
   smoothed outer step, and preserve the observed center;
5. nonaccepted scale freeze and runtime commit consistency remain intact;
6. an ordinary recovery residual above `0.75` cannot enter or complete
   confirmation, while the observed in-view range below `0.453` can still
   complete the existing two-hit recovery;
7. detector and occlusion-hold confirmation tests remain unchanged and pass.

Host success does not prove RK3566 runtime behavior, object identity,
calibration, or pod acceptance. Cross-compilation, deployment, and another pod
trial require separate explicit authorization after the implementation is
reviewed.
