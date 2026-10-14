# RK3566 Detector Board Verification

Verification date: 2026-10-14  
Board release: `/home/cat/Project/SOT/nanotrack_v3_longterm/releases/rk3566-yolov8-detector`  
Acceptance root: `artifacts/20261014-acceptance`

## Platform and identities

- Board: `lubancat`, AArch64, Linux 4.19.232, 3,990,208 kB RAM.
- Compiler/runtime: GCC 8.3.0, OpenCV 4.10.0, RKNN API 2.3.2, driver 0.8.2.
- Detector model: `models/yolov8n-rk3566-fp16.rknn`, 8,097,993 bytes,
  SHA-256 `4d53a9b1e12996556294be8e099fe2dc0815d5c9cfdc327edfe350832baf8cf6`.
- Detector manifest SHA-256:
  `8adcb1026803d688a7b85c55977d533eac6c15070ac4260bedb09d1da49d6061`.
- COCO labels SHA-256:
  `bd17f1ee35d5f3c862a4894605855abbb9dda4b0621fdb0ac4c2c8c7bb7e730a`.
- Final `nanotrack_v3` SHA-256:
  `a1ef2e47af35a1a849e347ff73871541b8562f81f6530024551d12a2db9f22e5`.
- `yolo_detector_smoke` SHA-256:
  `47a54eb7713f1fb54b44a2ae892bb43b6a0a7ce75a8071a0cdc6f7f3482eaba1`.

Both executables are AArch64 ELF binaries. `ldd` resolves
`libopencv_world.so.410` and `librknnrt.so` from the declared board SDK paths
with no missing libraries. The final board CTest result is 10/10 passed.

## Acceptance matrix

| Gate | Result | Evidence |
| --- | --- | --- |
| Clean board-native build | PASS | `build-board-acceptance` |
| Board CTest | PASS | 10/10, `ctest-after-stability.log` |
| Model and tensor contract | PASS | 1 NHWC input, 9 NCHW outputs, RK3566 target |
| Official image inference | PASS | 5 plausible candidates: 4 person, 1 bus |
| Person video inference | PASS | 987 output frames, 58/58 detector batches |
| Car loop inference | PASS | 701 output frames, 32/32 detector batches |
| Detector-disabled regression | PASS | 987 frames, detector counters all zero |
| Control safety while LOST | PASS | no `LOST && control_valid=1` rows |
| Post-loss target recovery | **FAIL** | person and second-loop car were not reacquired |
| Continuous process/queue run | PASS | 1267 s, 272/272 batches, 9 threads, gap <= 1 |
| Memory peak behavior | **RISK** | RSS max 446,320 kB; VmHWM 476,684 kB |

## Official-image smoke

Input: Ultralytics official `bus.jpg`, 810x1080, SHA-256
`c02019c4979c191eb739ddd944445ef408dad5679acab6fd520ef9d434bfbc63`.

- `official-smoke/bus-annotated.jpg` SHA-256:
  `b710b489f7f097975b6b2b6453e6b9706f344f357201c3d1faaac6289cc87e08`.
- `official-smoke/candidates.csv` SHA-256:
  `34d3c06ab1ca0b56540f8443c8a2e7aae6b31cb2b7c11f71a91fce9373e2a235`.
- One-frame timing: preprocess 19.595 ms, inference 192.158 ms,
  postprocess 45.591 ms, total 257.344 ms.

The timing is a function check, not a performance pass line.

## Video results

### Person, detector enabled

Directory: `person-video`

- 987 output frames and 986 tracked frames passed `verify_board.sh`.
- Detector request/complete/reject/error: 58/58/0/0.
- 113 rows used detector hypotheses, distinct from 58 completed batches.
- Only the first 99 rows remained valid tracking. No reacquisition occurred.

### Car disappearance and reappearance loop

Directory: `car-loop-700`

- 701 output frames and 700 tracked frames passed `verify_board.sh`.
- Detector request/complete/error: 32/32/0.
- 429 rows were LOST and all had `control_valid=false`.
- Four early motion-based reacquisitions occurred in the first loop. The target
  was not recovered after it reappeared at the second-loop boundary.

### Detector disabled

Directory: `person-no-detector`

- 987 output frames passed `verify_board.sh`.
- Metadata has `detector.enabled=false`; all CSV rows have
  `detector_configured=0`; all detector counters are zero.

## Stability run

Directory: `stability-4200-v2`

The stability binary SHA-256 was
`e95ef3d17b2673ec4fb5b26f9d2c2875806ffa101ef4f5deb9f69bcbacc36f5c`.
The final handoff binary differs only by RKNN cleanup-return logging and was
separately verified with the 120-frame `final-smoke-120` run and board CTest.

- Wall time: 1267 seconds.
- Output/tracked frames: 4201/4200.
- Detector request/complete/reject/error: 272/272/0/0.
- Thread count: always 9; maximum sampled request/complete gap: 1.
- Temperature: 28.888 to 31.875 degrees C.
- Sampled NPU load: 2% to 18% (mean 7.22%).
- Runtime boundary: mean 214.973 ms, 4.652 FPS; excludes video I/O,
  rendering and encoding.
- End-to-end boundary: mean 295.998 ms, 3.378 FPS; includes the complete
  decode-track-overlay-encode path.
- Warm RSS: first 147,152 kB, last 149,328 kB, median 171,772 kB.
- Peak RSS: 446,320 kB; VmHWM: 476,684 kB.

The process, detector worker and bounded queue completed without errors, but
the growing periodic memory peaks require a separate writer/buffer/allocator
investigation before claiming memory stability.

The first stability attempt is retained in `stability-4200`. It stopped after
543 seconds because a finite NanoTrack proposal lay wholly outside the frame.
The fix converts only that proposal class to no observation; the v2 run starts
from frame zero and is not a continuation of the failed run.

## Remaining product work

1. Collect and label the visible/infrared calibration dataset specified in
   `docs/superpowers/specs/2026-10-13-nanotrack-threshold-calibration-data-requirements.md`.
2. Diagnose why detector hypotheses fail NanoTrack anchor/recent verification;
   evaluate target-specific retrieval or ReID for same-class ambiguity.
3. Isolate long-run memory peaks from OpenCV AVI writing, frame buffers and the
   allocator, then repeat the same resource protocol.
4. Replace the generic FP16 COCO model only after preserving the current
   manifest, tensor-contract and board-acceptance gates.
