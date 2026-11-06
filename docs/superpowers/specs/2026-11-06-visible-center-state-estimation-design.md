# Visible Center State Estimation Design

**Date:** 2026-11-06
**Scope:** RK3566 NanoTrack V3 accepted-center behavior for visible light

## Intent

Reduce visibly distracting accepted-box position jitter without hiding the
network measurement or changing quality thresholds. The controller will treat
the selected tracker center as a noisy measurement of a motion-predicted state,
then emit one bounded, blended center that is used consistently by output,
motion correction, template staging, and the next NanoTrack crop.

## Evidence and success criteria

The retained new-target pod run showed that, after excluding the cabinet-handle
initialization, the user-declared occlusion, and an independent failed segment,
accepted center steps had approximately 6.0/20.25/46.62 pixel P50/P95/maximum
and a 29.83-pixel P95 second difference. The earlier target was approximately
3.61/10.77/23.77 pixels. Template switches amplify the problem, but same-template
accepted steps still had an 18.10-pixel P95. Scale was already stable, so this
change addresses center position only.

Host behavior is successful when alternating or isolated reliable center
measurements produce materially smaller final-center excursions, while a
consistent moving target continues in the correct direction with bounded lag.
Every accepted frame must retain enough diagnostics to compare selected raw,
motion-predicted, and final centers. A later human pod trial remains required to
judge visual stability.

## Goals, non-goals, and anti-goals

Goals:

- filter only the center of accepted observations after template selection;
- use motion prediction as the state prior so genuine motion is followed rather
  than treated as stationary noise;
- bound a single-frame correction relative to target size;
- retain the raw selected box and report the innovation, applied correction,
  and whether the bound was active;
- preserve one final box for every downstream consumer.

Non-goals:

- no PSR, peak-margin, APCE, detector, reacquisition, or template-selection
  threshold change;
- no new template arbitration or template stickiness;
- no change to the accepted-scale temporal filter;
- no servo-command or host-integration change;
- no claim of pod acceptance from host tests.

Anti-goals:

- do not create a smooth display/control box while committing a different raw
  box to NanoTrack;
- do not overwrite or omit raw measurements in diagnostics;
- do not update center state from rejected, coasted, or aborted frames;
- do not slowly drag a confirmed reacquisition from a stale pre-loss center.

## Approaches considered

1. **Plain EMA after output.** This is small but ignores the existing motion
   state, adds lag during genuine movement, and can conceal large raw errors.
   Rejected.
2. **Motion-aware bounded innovation blend after template selection.** Use the
   existing motion prediction as the prior, blend the selected-center residual,
   bound the applied correction by target size, and expose all values in debug
   telemetry. This is selected because it addresses both template-switch and
   same-template jitter without changing the network or quality decision.
3. **Template-selection stickiness first.** This could reduce switch-amplified
   jumps, but the retained run still had substantial same-template jitter. It is
   deferred so this iteration changes only one cause at a time.

## Selected configuration

Add two observable controller settings:

- `accepted_center_smoothing_alpha=0.35` for visible light;
- `accepted_center_max_correction_ratio=0.25` for visible light.

The correction ratio is measured against
`sqrt(final_width * final_height)`. Generic and infrared compatibility values
are `1.0` and `0.0`; a zero maximum ratio means no correction bound, so those
profiles preserve direct selected-center output exactly.

Validation requires finite smoothing alpha in `(0,1]` and a finite nonnegative
maximum correction ratio. Both settings are part of configuration identity,
session metadata, JSON serialization, and board-evidence verification. The
tracking schema version remains `9`, following the existing effective-setting
extension precedent; per-frame center diagnostics go to debug telemetry rather
than changing the CSV schema.

## Selected behavior

For each ordinary accepted observation:

1. Run the existing scale stabilizer to obtain final width and height, without
   using its observation-derived center.
2. Let `P` be the center of `SearchPlan::motion_predicted_bbox` and `R` be the
   center of the selected raw observation.
3. Compute innovation `D = R - P`.
4. If `accepted_center_max_correction_ratio > 0`, limit the Euclidean magnitude
   of `D` to
   `accepted_center_max_correction_ratio * sqrt(final_width * final_height)`.
   Preserve direction while limiting; record whether limiting occurred.
5. Emit center `E = P + accepted_center_smoothing_alpha * D_limited`.
6. Rebuild a frame-safe final box with center `E` and the already stabilized
   dimensions.

With compatibility values `1.0/0.0`, `E` equals the raw selected center. With
visible values, a large one-frame measurement cannot directly become a large
final-box jump, but a prediction that already carries consistent velocity is
not forced back toward the previous center.

The final box remains the single downstream truth: controller current state,
motion correction, runtime `commit_bbox()`, template staging, adapter output,
and existing logs all consume it. No raw-center commit path is added.

## State, recovery, and diagnostics

This filter uses the existing motion estimator rather than adding a second
velocity state:

- initialization outputs the initialization center unchanged;
- rejected, uncertain, coasted, and aborted frames do not apply a measurement
  correction;
- ordinary accepted frames use the bounded innovation blend;
- after existing confirmation declares a reacquisition, output the selected raw
  center once and reinitialize motion from that one final box, avoiding stale
  pre-loss drag;
- frame clipping remains the responsibility of the existing centered-box helper.

`ControllerOutput` records an optional selected raw bbox plus center innovation
X/Y, applied correction X/Y, and a correction-limited flag. Debug telemetry
emits these fields on accepted-frame detail events together with predicted and
final boxes. The raw anchor and recent observations remain available as they
are today. This makes the smoothing effect auditable and keeps a network or
template-selection defect visible.

## Verification boundary

Tests must first fail against the current implementation and then prove:

1. visible defaults and shipped config expose `0.35/0.25`; generic and infrared
   expose `1.0/0.0` and retain exact direct-center behavior;
2. invalid zero/negative/non-finite alpha and negative/non-finite ratio are
   rejected, and configuration identity changes with either setting;
3. alternating reliable center measurements have a smaller final excursion
   than the raw measurements;
4. a single large accepted innovation is bounded isotropically and reported as
   limited;
5. sustained constant-direction motion advances every accepted frame and does
   not remain frozen;
6. nonaccepted and aborted frames cannot introduce a center correction;
7. confirmed reacquisition uses the confirmed raw center and reinitializes
   motion rather than blending with stale prediction;
8. runtime commits exactly the stabilized final box;
9. debug telemetry exposes raw, predicted, innovation, correction, limited, and
   final values without changing the tracking CSV schema;
10. metadata and board schema require both effective settings;
11. the complete RKNN-independent suite remains green.

Host tests and retained-log comparisons do not prove target identity, RK3566
runtime quality, servo behavior, or pod acceptance. Cross-compilation,
deployment, remote process operation, and a pod trial require separate explicit
authorization after local implementation review.
