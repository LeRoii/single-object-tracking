# Detector-assisted recapture TODO

This list separates implemented controller safety from work that still needs
labeled data or RK3566 board evidence. An unchecked item is not an accepted
capability.

## Implemented in the development branch

- [x] Keep the existing YOLOv8 detector implementation and adapter.
- [x] Support explicit `full_object` and `subtarget` recovery modes.
- [x] Preserve subtarget width/height when probing a detector search region.
- [x] Assign detector candidates distinct local ids.
- [x] Lock one candidate after agreeing anchor/recent NanoTrack evidence.
- [x] Prevent pending candidates and newer detector batches from contributing
  to an active candidate's confirmation window.
- [x] Require two consistent NanoTrack hits in a three-frame locked window.
- [x] Keep `control_valid=false` until NanoTrack confirmation succeeds.
- [x] Apply detector-origin center, IoU, area, and aspect gates.
- [x] Prevent a detector batch containing multiple candidates of one class
  from establishing a lock; prefer invalid control over an identity switch.
- [x] Add schema-v5 candidate, same-class ambiguity, probe, lock, and
  confirmation audit fields.
- [x] Cover the controller with deterministic unit and replay tests.

## Test-data preparation

Use the per-video annotation contract in
[`occlusion-video-annotation-requirements.zh-CN.md`](occlusion-video-annotation-requirements.zh-CN.md)
for occlusion, reappearance, subtarget, deformation, and distractor labels.

- [ ] Create two `girl_dance.mp4` cases: whole-person initialization and
  face/head subtarget initialization.
- [ ] Assign the tracked person a stable target id.
- [ ] Mark per-frame `none`, `partial`, and `full` occlusion state.
- [ ] Mark visibility ratio, target presence, out-of-view state, and the first
  reappearance frame for every full-occlusion event.
- [ ] Annotate the target bbox on visible frames and the visible-region bbox on
  partially occluded frames.
- [ ] Give the other person a stable distractor id and bbox over each recovery
  interval.
- [ ] Mark significant deformation/pose-change intervals.
- [ ] Add more continuous, unedited person and vehicle sequences with full
  occlusion followed by reappearance; `sot-v1.mp4` is not an occlusion-recovery
  acceptance sequence.
- [ ] Prepare infrared annotations independently from visible-light data.

## Threshold calibration and acceptance

- [ ] Calibrate reliable/weak NanoTrack response thresholds per sensor.
- [ ] Calibrate detector-origin IoU, center, area, and aspect thresholds
  separately for `full_object` and `subtarget`.
- [ ] Evaluate probe count/order and recovery delay for subtargets of different
  size and position inside a detector region.
- [ ] Report event-level recovery rate, reappearance-to-confirmation delay,
  false recovery rate, identity switches, and full-occlusion
  `control_valid` violations.
- [ ] Define pass/fail thresholds before declaring product acceptance.
- [ ] Keep `calibrated=false` until the above dataset and report are retained.

## RK3566 board verification

- [x] Build source commit `1552b7d14f67006768757dff039327253e3a60cb`
  natively on the RK3566 board with RKNN runtime enabled.
- [x] Run all 12 CTest targets on the board.
- [x] Deploy to a new immutable release; do not modify
  `releases/rk3566-yolov8-detector` in place.
- [ ] Run both `girl_dance` cases and retain configuration, hashes, schema-v5
  CSV, metadata, console log, and annotated output video.
- [ ] Manually inspect every `reacquired=1` event against the target identity.
- [ ] Measure sustained tracker/runtime/end-to-end latency, detector duty
  cycle, CPU, NPU, RSS, and peak memory.
- [ ] Repeat detector-disabled regression and a long-duration stability run.

Current unlabeled board evidence is retained under
`releases/rk3566-detector-recapture-1552b7d`:

- `artifacts/girl_dance-subtarget-300f-1552b7d`: 301 output frames passed
  schema-v5 artifact verification. All 169 detector hypothesis rows contained
  two same-class candidates; none locked or produced control.
- `artifacts/girl_dance-subtarget-full-1552b7d`: all 987 source frames passed
  artifact verification. It recorded 823 ambiguous hypothesis rows, zero
  ambiguous locks, zero detector control rows, and zero reacquisitions.
  Runtime-only throughput was 19.837 FPS; the measured decode/track/overlay/
  encode pipeline was 7.328 FPS.
- The preceding `22a15d0` diagnostic falsely recovered the right-side dancer
  at frame 287 after a valid 2/3 lock. Visual inspection of that event motivated
  the conservative same-class ambiguity quarantine in `1552b7d`.

These are safety and runtime diagnostics, not recovery-accuracy evidence. The
final full-video run avoided the observed identity switch but did not recover
the target after loss. It contained 27 single-candidate hypothesis rows; none
met the reliable dual-template and geometry gates.

## Deferred product improvements

- [ ] Add initialization-time `auto` target-mode classification after labeled
  whole-object/subtarget examples are available; retain explicit override.
- [ ] Associate detector candidates across detector batches when a lock is not
  active, using motion and geometry rather than confidence alone.
- [ ] Consider a target-specific ReID/embedding model if conservative template
  verification cannot meet same-class interference acceptance criteria.
- [ ] Replace the conservative same-class quarantine with calibrated identity
  ranking only after labeled distractor data and a target-specific appearance
  signal are available; detector confidence alone is not an identity score.
- [ ] If ReID is added, measure its RK3566 latency/memory and require it to
  remain advisory: detector/ReID evidence alone must not set `control_valid`.
- [ ] Investigate the previously observed video writer/buffering memory peaks.

