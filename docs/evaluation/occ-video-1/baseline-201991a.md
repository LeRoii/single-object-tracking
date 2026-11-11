# `occ_video_1` RK3588 baseline (`201991a`)

- Verdict: **FAIL**
- Source: `201991a92bba95b542c97505c37053d1c4fe1da8`
- Input: canonical 271-frame `occ_video_1`, video SHA-256
  `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`
- Tracking CSV SHA-256:
  `b2ba9d7328f0554d1806a36f817ece8a42a5cdd0e4275b9cae44f1b87271eb6f`
- Evaluator: `scripts/evaluate_occ_video_1.py` SHA-256
  `7338a2b8a114df5f50d73c9f43e3c60fedfb7700e902bca8cf4dbfc054684d4d`

## Reproduced metrics

| Stratum | Frames | Mean IoU |
| --- | ---: | ---: |
| Ordinary visible | 258 | 0.578565 |
| Pre-occlusion visible, 0--168 | 169 | 0.631145 |
| Post-reappearance visible, 182--270 | 89 | 0.478722 |

Frames 169--181 were excluded from ordinary IoU. Their estimated-trajectory maximum normalized center
error was `0.304675`; this diagnostic baseline passed only the current prediction-continuity proxy.

## Failed gates

- Schema is legacy version 9 and lacks explicit observation/control-source semantics.
- 78 ordinary visible frames violate the per-frame box gate.
- Frames 172--180 do not publish the required `SEARCHING / MOTION_SEARCH` semantics.
- `control_valid=false` occurs at frames 168, 174--178, 180--184, 243, 248, 265, and 268.
- No legal `reacquired` event occurs in `[182,190)`.
- The automatic identity/continuity proxy flags frames 53--63.

## Evidence boundary

The original board directory remains at
`/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/runs/occ-baseline-full-20261111`.
It has no `manifest.json`, so `verify_board.sh` fails by design. This is a reproducible failed algorithm
baseline, not a board-verifier PASS, model promotion, production handoff, or gimbal-pod acceptance.
