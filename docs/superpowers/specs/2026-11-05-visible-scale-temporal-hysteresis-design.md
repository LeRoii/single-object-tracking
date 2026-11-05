# Visible Scale Temporal Consistency and Output Hysteresis Design

**Date:** 2026-11-05
**Scope:** RK3566 NanoTrack V3 visible-light accepted-scale behavior

## Intent

Reduce visible width/height breathing, especially repeated one-pixel height
reversals, while still allowing a real and persistent target-size change to be
followed gradually. Keep the current visible quality thresholds and recovery
center gate unchanged.

## Evidence and success criteria

The retained pod trial is
`/tmp/nanotrack-4c45110-logs/stderr.scale-smooth-20261105.log`, SHA-256
`6d8536e5b199a72490e1490b81f6b9917bbed1c14e437cd19572150ca32259f4`.
Across ten user-confirmed same-target initializations, final height changed on
24.3% of adjacent frames. Of 475 changes, 472 were one pixel, 303 reversed
direction, and 102 were direct `48<->49` transitions. 448 changes happened on
Reliable and accepted frames. The prior 5% limiter trial changed height on
6.2% of adjacent frames, so the current visual instability is real and is not
primarily a PSR or peak-margin threshold problem.

Host behavior is successful when alternating one-pixel-scale observations do
not make the emitted size reverse repeatedly, while a sustained larger or
smaller observation eventually changes the emitted size in the same direction
without exceeding the existing outer scale bound. A later pod trial remains
required to judge visual stability.

## Goals and non-goals

Goals:

- reject isolated and alternating accepted-scale noise over time;
- require persistent direction before scale starts to move;
- prevent float-to-integer boundary chatter in the box consumed by the
  adapter and committed back to NanoTrack;
- retain gradual response to persistent real growth and shrinkage;
- keep effective behavior configurable and recorded in evidence metadata.

Non-goals:

- no PSR, peak-margin, APCE, detector, or recovery-center threshold change;
- no edge, partial-visibility, or out-of-view policy;
- no center-position smoothing;
- no permanent lock of target size or aspect ratio;
- no split in which NanoTrack and the control/display path consume different
  final boxes.

## Approaches considered

1. **Accepted-observation median, direction confirmation, and sticky integer
   output.** One filtered scale state produces the same final box for control,
   display, motion state, and NanoTrack commit. This is selected because it
   attacks the observed temporal reversals without creating two divergent box
   identities.
2. **Separate NanoTrack internal scale from control/display scale.** This can
   make the visible box very stable, but crop state and displayed state may
   diverge and invalidate recovery geometry. Rejected for this iteration.
3. **Lock aspect ratio and filter only one isotropic scale.** This strongly
   suppresses height variation, but can reject legitimate viewpoint-driven
   aspect changes. Deferred until temporal filtering is tested.

## Selected configuration

Add three observable controller settings:

- `accepted_scale_median_window_frames=5` for visible light;
- `accepted_scale_direction_confirm_frames=3` for visible light;
- `accepted_scale_output_hysteresis_px=0.75` for visible light.

Generic and infrared compatibility values are `1`, `1`, and `0`. Together
with their existing unrestricted scale settings, these values preserve the
current direct-observation behavior.

Validation requires an odd median window in `[1,31]`, direction confirmation
in `[1,31]`, and a finite nonnegative hysteresis. All three settings are part
of configuration identity, session metadata, JSON serialization, and board
evidence verification.

## Selected behavior

The controller owns, independently for width and height:

- the latest five accepted Reliable raw dimensions;
- the last median direction and its consecutive confirmation count;
- a floating filtered dimension accumulator;
- the emitted final dimension used everywhere downstream.

Initialization clears both histories and seeds filtered and emitted dimensions
from the initialization box. A reacquisition clears pending history and
direction evidence before seeding the new accepted raw observation; it does
not jump the emitted scale to the raw reacquisition size. Short nonaccepted
intervals freeze scale and do not add samples, but do not discard already
collected accepted history.

On each ordinary accepted observation, for each dimension independently:

1. Append the raw dimension and retain the newest five accepted samples.
2. Until the window is full, hold filtered and emitted dimensions.
3. Take the median. If it is inside the existing 2% deadband around the
   filtered accumulator, clear direction confirmation and hold.
4. Otherwise classify the median as growth or shrinkage. A direction change
   resets its confirmation count to one; the same direction increments it.
5. Until three consecutive accepted medians agree, hold.
6. After confirmation, apply the existing plus/minus 5% outer clamp and 0.10
   smoothing alpha to the floating accumulator.
7. Compare the accumulator with the emitted dimension. If the difference is
   less than 0.75 pixel, hold the emitted dimension. If it reaches the
   boundary, move the emitted dimension by at most one pixel toward the
   accumulator. Rebuild the final box around the accepted observation center.

The 0.75-pixel Schmitt-style boundary prevents a value near an integer border
from repeatedly emitting adjacent sizes. The median and direction confirmation
prevent alternating raw regression from repeatedly driving the accumulator.
Persistent scale evidence continues moving the accumulator and therefore
eventually advances the emitted size one pixel at a time.

The final emitted box remains the single downstream truth: controller current
state, motion correction, runtime `commit_bbox()`, template staging, adapter
output, and logs continue to use it. No hidden raw-scale commit path is added.

## State and failure handling

- `initialize()` resets all temporal scale state.
- ordinary, detector, and occlusion reacquisition use the same no-jump scale
  rule but reset stale temporal evidence before collecting new samples;
- nonaccepted frames preserve the existing frozen final scale;
- aborted/failed frames must not mutate the window, direction count,
  accumulator, or emitted size;
- width and height are always finite, positive, frame-bounded, and recentered
  with the existing frame-safe helper.

## Verification boundary

Tests must first fail against the current implementation and then prove:

1. visible defaults and shipped config expose `5/3/0.75`; generic and infrared
   expose compatibility values and retain exact direct-observation behavior;
2. invalid even/out-of-range windows, invalid confirmation counts, and
   negative/non-finite hysteresis are rejected;
3. five-sample median and three-median direction confirmation hold scale before
   persistent evidence is established;
4. alternating `48/49` and alternating growth/shrink observations do not make
   emitted scale chatter;
5. sustained growth and shrinkage eventually change the emitted size in the
   correct direction, at no more than one emitted pixel per accepted frame and
   within the existing filtered outer bound;
6. nonaccepted and aborted frames do not advance temporal scale state;
7. initialization and all reacquisition paths clear stale temporal evidence;
8. the runtime commits exactly the final emitted box;
9. metadata and board schema require the three effective settings;
10. the complete RKNN-independent suite remains green.

Host tests and log replay do not prove target identity, RK3566 runtime quality,
or pod acceptance. Cross-compilation, deployment, and another pod trial require
separate explicit authorization after implementation review.
