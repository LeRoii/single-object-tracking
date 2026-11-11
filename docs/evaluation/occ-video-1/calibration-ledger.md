# `occ_video_1` development calibration ledger

This file is append-only. Candidate selection may use frames 0--189 only. Frames
190--270 are frozen holdout and must not be inspected until a candidate is frozen
in Git. Large outputs remain on the RK3588; this ledger stores identities, commands,
compact report hashes, and decisions.

## Required entry fields

Every new entry records: candidate ID and parent; timestamp; rationale; source
commit; executable, config, NanoTrack model/manifest, detector model/manifest/labels,
and video SHA-256; exact board path and 0--189 command; evaluator report paths and
hashes; changed parameters; decision and evidence boundary. Missing evidence is
written as `NOT_RECORDED` or `PENDING`, never inferred.

## C000 — historical failed baseline

- Timestamp: `2026-11-11T10:53:22+08:00`
- Parent: none
- Rationale: preserve the pre-change failure as the immutable comparison point.
- Source commit: `201991a92bba95b542c97505c37053d1c4fe1da8`
- Board root: `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f`
- Evidence root: `/home/neardi/Project/SOT/nanotrack_v3_debug_main-1bf657f/runs/occ-baseline-full-20261111`
- Executable SHA-256: `NOT_RECORDED`
- Config: `config/visible-default.conf`
- Config SHA-256: `54dcc973feed11ad3e9e7838c8f35e86b0d587d5aa143863a625c849fa5dae19`
- NanoTrack template/search/head SHA-256:
  `eaf7f80f134d4ba5016b2c33620a631111a4143d5f4fdcea744ddc77bf137d55`,
  `d3cb0e3879df6a1272adb59bf501324f1f8f55b58309d3c4da4a731b600973ee`,
  `4da379f54093d79f02775e90bfea33ebee0837c6fc73a324499279b4c57807c5`
- NanoTrack manifest SHA-256: `3599d7339ee4e3a8062e130549fdc868c29a1eeb3549745380f144e7fd985261`
- Detector model/manifest/labels SHA-256:
  `8d35dc59ef004bb97c0d24982e0d54c6196a18cb301856506f923ebf6b59c85c`,
  `ac2b536c3a46761138ecc67a2be865890b8bc0bbe2fb7195c2bb8b12d34049c9`,
  `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992`
- Video SHA-256: `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`
- Command: the historical run used `--max-frames 270`; it predates the development-only
  rule and is retained only as a failed baseline. The exact command is in
  `docs/evaluation/occ-video-1/baseline-201991a.md`.
- Report: `docs/evaluation/occ-video-1/baseline-201991a.json`; full generated report
  SHA-256 `6ed8b993bd825b38dc033515a286e51e6283906f7e1449689d6fc9128ee8ce6a`.
- Changed parameters: none.
- Decision: `FAIL_BASELINE`; schema 9, discontinuous control, no legal reacquisition,
  and visible/identity gates failed. The evidence directory lacks `manifest.json`, so
  it is not a board-verifier PASS.

## C001 — generic bounded motion-search seed

- Timestamp: `2026-11-11T12:43:43+08:00`
- Parent: `C000`
- Rationale: enable time-bounded motion control and search-wide two-hit recovery with
  conservative physical caps while retaining all existing visible thresholds. No
  frame number, truth coordinate, bridge feature, or holdout result selected a value.
- Runtime source commit: `15f32fe8d52fee0b53e2c15d961b87f1c006ccd5`
- Candidate config: `rk3566/nanotrack_v3/config/visible-occlusion-calibrated.conf`
- Candidate config SHA-256: `495d5a0bc74d077cb333ae25affc512005b701fc432552f0994ed55607aab6c9`
- Board root: `PENDING`
- Evidence root: `PENDING`
- Executable SHA-256: `PENDING`
- NanoTrack and detector identities: unchanged from `C000`; they must be re-hashed on
  the board before a run rather than trusted from this statement.
- Video SHA-256: `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`
- Planned 0--189 command:

```bash
./build/nanotrack_v3 \
  --video data/occ_video_1/1.mp4 --bbox 992,582,37,39 --model-dir models \
  --sensor visible --controller-config config/visible-occlusion-calibrated.conf \
  --max-frames 189 \
  --detector-model releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn \
  --detector-manifest releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json \
  --detector-labels releases/yolov8n-visdrone9-rk3588-fp16-v1/models/visdrone9.txt \
  --detector-class-id 2 --detector-conf 0.25 --detector-nms 0.45 \
  --output-video runs/C001-development/output.avi \
  --output-boxes runs/C001-development/bboxes.csv \
  --session-metadata runs/C001-development/session-metadata.json
```

- Evaluator scope/report: `development`; paths and hashes `PENDING`.
- Changed parameters: `calibrated false -> true`; `motion_search_enabled false -> true`;
  confidence `2500 ms`; speed `1.0 diagonal/s`; acceleration `2.0 diagonal/s^2`;
  scale-rate cap `0.50/s`; probe interval `100 ms`. `confirmation_required=2` and
  `confirmation_window=3` remain unchanged.
- Decision: `PENDING_NOT_RUN`. This entry authorizes only development-scope iteration;
  it is not a calibrated threshold claim, holdout result, or board acceptance.

## C001 result — rejected tracker-only reacquisition

- Timestamp: `2026-11-11T12:58:00+08:00`
- Board root: `/home/neardi/Project/SOT/occ-candidate-cbd38a5`
- Evidence root: `/home/neardi/Project/SOT/occ-candidate-cbd38a5/runs/C001-development`
- Runtime source commit: `cbd38a5`
- Executable SHA-256: `f58f22bb9e2bb6ba5821841d1402d3869e9978097d921d7bfb0cd5933199439d`
- Config SHA-256: `495d5a0bc74d077cb333ae25affc512005b701fc432552f0994ed55607aab6c9`
- Model, manifest, detector, labels, and video hashes: rechecked before the run and
  identical to C000; exact lines are retained in `pre-run.sha256`.
- Run artifact SHA-256: bboxes
  `124b3789568f841cd571984dff28668ab3327c3d230fb04321457a9005943776`, video
  `0ddd1840ffe3b67b07f94e2d4256e4c56c240b4b673fdf0d294a18383875c8c9`, log
  `0921bd015662b9354615cfed7c27708302624235de62cf82949d4e235864daf0`, metadata
  `4cde24ae30523c7c2db29c9356064a7cb5d1827dec17c1789e33b3b7a0f86f4a`.
- Evaluator development report SHA-256: JSON
  `4aeb6e574db8c3fbbb466e176f9a373b559f725fefe1c419ad10bd1d9f290771`, Markdown
  `e616374ebb4b9432d2e5c31c1ffea14b6333ea1a5624e99637160034cb5dac15`.
- Result: `FAIL`. Continuous control passed. Tracker-only two-hit candidates falsely
  reacquired at frames 171, 183, and 186; full-occlusion semantics failed at 172--173,
  visible mean IoU was `0.731970`, and post-reappearance identity/box gates failed.
- Verifier observation: the committed verifier rejected frame 169 because its legacy
  coast authorization rule incorrectly required `OCCLUSION_HOLD`; this is a verifier
  defect, not permission to ignore the evaluator failure.
- Decision: `REJECT`; do not use C001 for perturbation or holdout.

## C002 — detector-required search authorization

- Timestamp: `2026-11-11T13:00:00+08:00`
- Parent: `C001`
- Rationale: the failed frames show that two locally consistent anchor responses can
  still be bridge/occluder texture. Require a class-filtered, generation-safe detector
  candidate to seed search confirmation; NanoTrack anchor/recent evidence remains the
  final verifier and detector still never directly authorizes control.
- Runtime parent commit: `cbd38a5`
- Candidate config SHA-256: `bb3513919fd4cc78ae3ae78b38057882ffa96d14bf22429a7f3120fd54e51901`
- Changed parameter: `search_reacquisition_requires_detector false -> true`.
- Code correction: a detector-required non-detector frame stays on motion control and
  cannot fall through to legacy ordinary recovery; a committed two-hit hint records
  `used=true`. Verifier authorization now recognizes semantic motion search outside
  legacy HOLD.
- Safety invariants: confirmation remains two hits; detector supplies only a search
  hypothesis; anchor reliability/template geometry, ambiguity, stale/generation, and
  fault gates remain mandatory.
- Planned command and development scope: identical to C001 with run directory
  `runs/C002-development`; board paths, executable/report hashes, and decision `PENDING`.
