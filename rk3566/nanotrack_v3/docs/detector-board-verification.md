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
  `a1bf9773ed2c042f542ffdd28b78ef75a69b3d9a051f3b564aa7a1c8c2c108c4`.
- `yolo_detector_smoke` SHA-256:
  `c83f9c4dc3a64cc6af9f569e0a57c7499d6b1b68a6bae2401cae9f1b9e59407f`.

Both executables are AArch64 ELF binaries. `ldd` resolves
`libopencv_world.so.410` and `librknnrt.so` from the declared board SDK paths
with no missing libraries. The final board CTest result is 11/11 passed; the
retained log is `ctest-final-cleanup.log`, SHA-256
`666d76800eda9c0b3e7d00495bbadcf71d2e71ab6e46881bcf135f2418308c7d`.

## Acceptance matrix

| Gate | Result | Evidence |
| --- | --- | --- |
| Clean board-native build | PASS | `build-board-acceptance` |
| Board CTest | PASS | 11/11, `ctest-final-cleanup.log` |
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

### Final cleanup binary, detector exercised

Directory: `final-detector-250`

- 251 output frames and 250 tracked frames passed `verify_board.sh`.
- Detector request/complete/reject/error: 9/8/0/0. The bounded worker had one
  request still in flight when the deliberately limited run ended.
- Output, CSV, metadata and log SHA-256 values are respectively
  `204c0170c7bbec5329cabfcf38e16a1b81e263429a1bf229a6c59e3bd4095fec`,
  `fe59aa495ba5005125ce4d90d11e8656df0921fe2be3c17402d9d7e2a58ac032`,
  `15a0fce8943d4fd7b71dd7d43998a7e82629f674402578c3cd07ab013bc21c01`,
  and `93b0aa0d9bb1940d0d2e74021d9846ce53d17800d74b4ee59c78cb5e24063d7f`.

The earlier `final-smoke-120` had detector configuration and model startup but
request/complete counters of 0/0. It is retained as startup/NanoTrack evidence,
not described as detector-inference evidence.

## Stability run

Directory: `stability-4200-v2`

The stability binary SHA-256 was
`e95ef3d17b2673ec4fb5b26f9d2c2875806ffa101ef4f5deb9f69bcbacc36f5c`.
The final handoff binary changes only RKNN cleanup/error paths relative to the
normal inference logic used here. It was separately verified with the
detector-exercising `final-detector-250` run and board CTest 11/11.

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
