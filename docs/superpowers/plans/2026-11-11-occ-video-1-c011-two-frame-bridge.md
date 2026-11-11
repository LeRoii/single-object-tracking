# occ_video_1 C011 Two-Frame Bridge Plan

**Goal:** Extend the already-safe temporal bridge from one to at most two
consecutive eligible frames, then verify the complete video on RK3588.

**Spec:** `docs/superpowers/specs/2026-11-11-occ-video-1-c011-two-frame-bridge-design.md`

## Task 1: Controller and evidence contracts

- Add failing controller tests for two eligible bridge frames and third-frame
  rejection, plus configuration, identity, metadata, telemetry, and verifier
  coverage for `template_bridge_max_consecutive_frames`.
- Implement the consecutive bridge counter and all reset rules. Default to one;
  set only the visible occlusion candidate to two.
- Run focused tests, all 19 CTests, evaluator/robustness Python tests, and
  `git diff --check`; commit the implementation.

## Task 2: RK3588 full acceptance

- Archive the exact committed source to a new immutable board root, verify
  SHA-256, build Debug/RKNN with `-Werror=switch`, and run 19 CTests.
- Run full frames 0--270 once, then verifier and full evaluator.
- If and only if the first full run passes, repeat twice and compare semantic
  events and evaluator gates.
- Record hashes, commands, boundaries, and final result in the owned board
  progress file and evaluation documents. Do not claim real-gimbal acceptance.
