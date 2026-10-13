# SDD ledger — plan: docs/superpowers/plans/2026-10-13-rk3566-nanotrack-long-term-controller.md

Execution mode: inline, current workspace. The workspace is not a valid Git repository, so Git worktrees, commits, and the Git-dependent SDD helper scripts are unavailable. Each task records RED/GREEN evidence here instead.

Baseline: `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-longterm-baseline -DBUILD_TESTING=ON -DENABLE_RKNN_RUNTIME=OFF`, build, and CTest passed 2/2 on 2026-10-13.

Pre-flight interfaces:

- Task 1 `ResponseMetrics` is consumed by Tasks 2–5: keep `DecodeResult::score` for source compatibility and add diagnostics without changing the existing decode signature.
- Task 2 types/config/motion/detector contracts are consumed by Tasks 3–5: keep them free of OpenCV/RKNN headers so host CTest remains available.
- Task 3 `SearchPlan`/`ControllerOutput` are consumed by Tasks 4–5: policy owns decisions; runtime owns pixels/model features.
- Task 4 runtime output is consumed by Task 5: one search backbone call per frame and controller-confirmed commits are hard invariants.
- Task 5 artifacts are consumed by Task 6 verification: log raw metrics and timing rather than only rendered video.

Ruling: The approved design spec is authoritative; threshold defaults are bring-up values only and every config/log must expose `calibrated=false` until labeled calibration is completed.

Task 1: complete (no commits: workspace has no Git repository; RED: build failed because `DecodeResult` had no `metrics`; GREEN: `ctest --test-dir /tmp/nanotrack-longterm-build --output-on-failure` passed 2/2 on 2026-10-13).

Task 2: complete (no commits; RED: build failed on missing `controller_config.hpp`; GREEN: CTest passed 3/3, including strict config parsing, alpha-beta prediction, timestamp rejection, clipping, and null detector behavior).

Task 3: complete (no commits; RED: build failed on missing `long_term_controller.hpp`; GREEN: CTest passed 4/4, including exact state boundaries, 2-of-3 reacquisition, anchor/recent conflict, detector freshness/rate/cap, recent probation/rollback, and a 10,000-frame detector-free LOST loop).

Task 4: runtime core complete, RKNN compilation pending board (no commits; RED: build failed on missing `long_term_runtime.hpp`; GREEN: CTest passed 5/5 with one-search-token/two-template observations, commit gating, template actions, and detector scheduling. `NanoTrackV3` was refactored to immutable anchor plus staged recent and one search feature reused by two heads. Host has no `rknn_api.h`, so its compilation is explicitly deferred to Task 6 board evidence).

Task 5: complete (no commits; RED: build failed on missing long-term CLI fields and `tracking_log.hpp`; GREEN: CTest passed 6/6. Video main now uses monotonic timestamps, `LongTermRuntime`, state-colored output, a unified raw-metrics CSV, config/manifest fingerprints, and session metadata. README and board verifier describe the new safety contract and schema).

Task 6: host portion complete and visible-light board runtime verified; infrared and labeled algorithm acceptance pending. Final clean host build `/tmp/nanotrack-longterm-boardfix-final` passed CTest 7/7 and `bash -n scripts/verify_board.sh`. Board-native build in isolated `/home/cat/Project/SOT/nanotrack_v3_longterm` passed after adding GCC 8 `stdc++fs` linkage; board CTest passed 7/7 and the three RKNN models loaded with API 2.3.2 / driver 0.8.2.

Board RED/GREEN evidence: the initial schema called controller runtime `total_ms`, which was not end-to-end video latency; schema v2 now logs `runtime_ms` and actual decode+runtime+overlay+encode `pipeline_ms`, and the verifier requires both. Original visible thresholds rejected both real videos immediately; a two-video bring-up experiment selected PSR 6.0 and peak margin 0.06 while retaining `calibrated=false`. Two board boundary failures were reproduced and fixed: prediction could leave the feasible center domain, then float rounding could make `y + height` exceed the frame by a tiny amount. Velocity cancellation, canonical clipping/state rewrite, and a 10,000-case fractional boundary regression now pass host and board tests.

Visible board artifacts: `final-visible-4k-350-540p` passed 350/350 rows with runtime mean/P50/P95 66.036/39.578/109.815 ms and end-to-end 3.773 FPS; `final-visible-girl-987-240p-v3` passed 987/987 rows with runtime 68.794/67.593/102.079 ms and end-to-end 9.968 FPS. The car sequence has four confirmed reacquisitions and later loses a target that exits/occludes out of view. The girl remains visible after the single bounded search center drifts away and never reacquires under `NullDetectorAdapter`, proving global recovery is not accepted without a detector candidate source.

Visible stability run: `artifacts/stability-visible-4k-loop20` completed 7,000 output frames / 6,999 tracked frames in one process over 2,207 s (36 min 47 s), exited 0, and passed the board artifact verifier. Mean/max RSS was 256,594/295,892 KiB with 295,896 KiB HWM; first-vs-last steady-state RSS windows differed by only 6 KiB. Mean/max CPU was 127.3/137.0% and temperature was 30.625-36.666 C. Runtime mean/P50/P95 was 113.609/114.907/127.318 ms and full pipeline 314.109/313.013/337.078 ms (3.184 FPS). It spent 6,728/7,000 rows in `LOST` after the first loop and had no detector results, so this passes process and bounded-LOST-path stability only, not continuous tracking or global recovery. Infrared video/initial box and labeled recovery/deformation data are still missing, so Task 6 Steps 6-7 and product threshold/algorithm acceptance remain incomplete.

Final review: self-review only because current execution policy prohibits dispatching subagents and the workspace has no Git range. Important finding fixed RED→GREEN: probation previously could commit a degraded recent template when anchor alone remained reliable; it now requires both templates reliable and mutually consistent. Additional fixes: identical anchor/recent reuse one head, invalid FPS cannot produce invalid JSON, config fingerprint is logged, and the legacy `NanoTrackV3::track()` bypass was removed.

Ruling: `pytest` is unavailable on the host, so the pre-existing Python model-manifest tests were not rerun. No model, conversion tool, or manifest code changed; C++ implementation verification remains authoritative for this plan. Board runtime remains mandatory.

Board connection audit, 2026-10-13: reverse SSH through `127.0.0.1:10022` connected to `cat@lubancat` (`aarch64`). The board clock reports 2019, so mtimes are not treated as chronological evidence; local/board source parity was checked by SHA-256. The pre-existing short-term deployment was preserved.
