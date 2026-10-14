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

- [ ] Build the branch natively on the RK3566 board with RKNN runtime enabled.
- [ ] Run all CTest targets on the board.
- [ ] Deploy to a new immutable release; do not modify
  `releases/rk3566-yolov8-detector` in place.
- [ ] Run both `girl_dance` cases and retain configuration, hashes, schema-v5
  CSV, metadata, console log, and annotated output video.
- [ ] Manually inspect every `reacquired=1` event against the target identity.
- [ ] Measure sustained tracker/runtime/end-to-end latency, detector duty
  cycle, CPU, NPU, RSS, and peak memory.
- [ ] Repeat detector-disabled regression and a long-duration stability run.

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

