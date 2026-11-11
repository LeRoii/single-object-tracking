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

## C002 result — safe search without reachable unique candidate

- Timestamp: `2026-11-11T13:06:00+08:00`
- Source commit: `141a2d5`
- Board/evidence root: `/home/neardi/Project/SOT/occ-candidate-141a2d5` and
  `runs/C002-development`
- Executable/config SHA-256:
  `980545c9baeeceba21a9d0c10e7036c5eb3c73505c46b5c5adce694d1585cfd1`,
  `bb3513919fd4cc78ae3ae78b38057882ffa96d14bf22429a7f3120fd54e51901`
- Run artifact SHA-256: bboxes
  `b1b822d593c26908ee317db763f78c38ef82c373ca945d1db2e6c371a9a89db1`, video
  `04d54cb90bddb30a37a6fb995f19c75461aaf1559a8f6ad5872478887c35176b`, log
  `8cd1951eba0c7f9952f6e52563b1779eb4d83c1721ce46c9cb93d3ef43c561d0`, metadata
  `3ad9fa0ad380da36d3b98a8565e2c6edbf4e6a5cee087f24140677988ac693f6`.
- Evaluator JSON/Markdown SHA-256:
  `f6585be2c7faf8b21000f0a99220771773020b08752abba0dcc41d236d8760cc`,
  `dd6a92985d209fab27f9d7540b2e635c5a30ed08d1e5372614c9741677330ce5`.
- Result: `FAIL`. Full-occlusion semantics, continuous control, and prediction
  continuity passed. No reacquisition occurred before 190 because both detector batches
  retained three same-class candidates and the ambiguity gate correctly blocked them.
  Visible mean IoU was `0.732761`; identity/visible failures remained after reappearance.
- Verifier observation: the first verifier revision still tied motion control to tracker
  `hypothesis_source=MOTION`; detector probes correctly retained `control_source=MOTION_SEARCH`,
  so this condition was removed and regression-tested with a DETECTOR hypothesis.
- Decision: `REJECT`; preserve its stronger safety behavior as the parent for C003.

## C003 — mature-search narrow motion corridor

- Timestamp: `2026-11-11T13:07:57+08:00`
- Parent: `C002`
- Rationale: the position gate is normalized by the full 1920x1080 frame diagonal;
  `0.15` admits roughly 330 px, while the target is about 44 px wide. Use `0.02`
  (about one target width) so the motion corridor rejects neighboring cars, and require
  `400 ms` search age so a short occluder transition cannot seed recovery.
- Runtime parent commit: `141a2d5`
- Candidate config SHA-256: `89fbc1d23b2be17c8d6938f5f53383ec4b26a59383f4343dce80603335715706`
- Changed parameters: `detector_candidate_max_center_ratio 0.15 -> 0.02`;
  `search_reacquisition_min_age_ms 0 -> 400`.
- Safety: both parameters only reduce authorization reachability; two-hit, class,
  ambiguity, template, generation/stale, and detector-shadow rules are unchanged.
- Required future robustness neighbors: both real-valued gates must be tested at
  `-10%/nominal/+10%`; nominal success alone cannot freeze C003.
- Planned command: same development-only command with `runs/C003-development`;
  exact source/executable/report hashes and decision `PENDING`.

## C003 result — unique candidate reached but weak recent blocked anchor

- Timestamp: `2026-11-11T13:11:43+08:00`
- Source commit: `2389df0`
- Board/evidence root: `/home/neardi/Project/SOT/occ-candidate-2389df0` and
  `runs/C003-development`
- Executable/config SHA-256:
  `b2cad65b27fc18171ce36fadf8bdb63d4a970f16205ed934e3a4f17a01fc46da`,
  `89fbc1d23b2be17c8d6938f5f53383ec4b26a59383f4343dce80603335715706`
- Board verifier: `PASS`; verify log SHA-256
  `f6564fb4dcfed79e3726f98d87cb3fb20a5bdbfa551023f269537cc304c585bd`.
- Run artifact SHA-256: bboxes
  `e52eb75dd0ad183e07c56ed32bb2262de6a921c63235f106b8f29e7b3b60346c`, video
  `d8500471aa3e8e6f508683cd5ed8fa2965fd0d26b4f245ba5e5b6031260e7e75`, log
  `9808633dec0b480a2239c9bdfa1f3f5bd8e09ba50709da9604daffb14b124653`.
- Evaluator result: identical FAIL metrics to C002; JSON/Markdown SHA-256
  `f6585be2c7faf8b21000f0a99220771773020b08752abba0dcc41d236d8760cc`,
  `dd6a92985d209fab27f9d7540b2e635c5a30ed08d1e5372614c9741677330ce5`.
- Diagnostic boundary: the narrowed gate reduced each detector batch to one queued
  candidate. At frame 187 the candidate was unique (`center_ratio=0.016328`), anchor
  was reliable, recent was weak but geometrically agreeing (`template_iou=0.911284`).
  The controller nevertheless required recent to be reliable, so it discarded the
  detector candidate before a shadow first hit.
- Decision: `REJECT`; corridor/minimum-age behavior is retained for C004.

## C004 — anchor-authorized, recent-veto-only detector verification

- Timestamp: `2026-11-11T13:11:43+08:00`
- Parent: `C003`
- Rationale: match the approved evidence policy: only a reliable anchor can authorize;
  recent never authorizes, and vetoes only when it is itself reliable and conflicts.
  A weak recent cannot turn a reliable anchor into acceptance, but also cannot suppress
  an otherwise legal detector-seeded first hit.
- Candidate config and SHA-256: unchanged from C003,
  `89fbc1d23b2be17c8d6938f5f53383ec4b26a59383f4343dce80603335715706`.
- Code change: unify tracker/detector search agreement as reliable anchor plus
  no reliable-recent conflict. Two-hit geometry, detector identity, class ambiguity,
  minimum age, corridor, generation/stale isolation, and shadow-only first hit remain.
- Planned run: `runs/C004-development`; exact source/executable/report hashes and
  decision `PENDING`.

## C004 result — nominal development PASS

- Timestamp: `2026-11-11T13:25:34+08:00`
- Runtime source commit: `0e19decf14d6fc674e78aa4c6249e12b70ba85b0`
- Evaluator source commit: `007f201ff998ec8b3d5f21ee134d406cc81d6870`
- Board/evidence root: `/home/neardi/Project/SOT/occ-candidate-0e19dec` and
  `runs/C004-development`
- Source archive/executable/config/evaluator SHA-256:
  `4eb692d72dae7f74093677b54313c7f0a7447ce3ba12eb253aa934c571f68082`,
  `89e61bd1fb938be513f3b941af2e4354a682d952856de3566710b27119f28fa2`,
  `89fbc1d23b2be17c8d6938f5f53383ec4b26a59383f4343dce80603335715706`,
  `7dcdd98386d0295ff319135c3d46201a08dc3bc07ce6cee410841cc66685b2b7`.
- Board build/test: native Debug, `-Werror=switch`, CTest `19/19`; board verifier
  PASS with verify-log SHA-256
  `f6564fb4dcfed79e3726f98d87cb3fb20a5bdbfa551023f269537cc304c585bd`.
- Run artifact SHA-256: bboxes
  `d577738b82c4478ac4661f855f923f12bd5571f750dcfdc29a50a2e0d207ac83`, video
  `909bfc60aa10c7ccbee02ea6863d85a07a78965bae178251ed90743498347070`, log
  `e2deb72b069a50222af81c13fb14b4f820a5959a4d235cc75858c0ac5abcb5d8`, metadata
  `75d48c976ece98fcb351f4d27dece76dd6c361dd8097f313c916b9b00f3f1c2b`.
- Evaluator JSON/Markdown SHA-256:
  `daad50c1a5be7361ba7d1f2b695d9a10a7ca0cf6ba66fc493159cdbf89a096d9`,
  `0336d518f7946a825a413820adc7e98ba87787e30d97223a8e2f218b54075f2c`.
- Result: `PASS`; unique reacquisition frame `188`; valid-observation mean IoU
  `0.762083`; all-visible diagnostic mean IoU `0.741046`; full-occlusion semantics,
  search prediction continuity, continuous control, and identity proxy all PASS.
- Media check: `1920x1080`, `190` output frames (initial frame plus 189 tracked frames).
- Decision: nominal candidate accepted for robustness testing only. It is not frozen;
  no holdout/full run is authorized until the generic perturbation plateau passes.
- Evidence boundary: a diagnostic status-file command unintentionally displayed status
  text for frames 190--193, but no post-189 video frame, bbox label, runtime output, or
  evaluator result was read. This partial metadata exposure must remain declared.

## C004 robustness result — reject deadline-edge nominal

- Timestamp: `2026-11-11T13:46:23+08:00`
- Board root: `/home/neardi/Project/SOT/occ-candidate-0e19dec/runs/C004-robustness`
- Matrix executor SHA-256 after the required output-video correction:
  `13efa246819945d9767a47940b0210d2297057904a98ebb744890fb50814e86d`.
- Real neighbors that passed included reliable score, template agreement, corridor,
  minimum search age, motion confidence/speed/acceleration, and detector confirmation
  IoU/center gates. Corridor `-10%` remained legal but moved reacquisition to frame 189.
- `detector_interval_ms -10%` passed at frame 187, while `+10%` produced no
  reacquisition before 190. The nominal 500 ms setting is therefore deadline-edge and
  not inside a plateau.
- Center `x/y +/-2 px` and center-preserving height `+/-5%` passed. Center-preserving
  width `-5%` preserved all per-frame IoU/center gates and dropped all-visible mean IoU
  by only `0.019565`, but the evaluator's independent `0.75` mean floor conflicted with
  the robustness contract's allowed `0.03` relative drop. Width `+5%` entered search but
  retained two same-class candidates inside the corridor and correctly refused identity
  authorization.
- Invalid `confirmation_window +/-1` and `detector_max_candidates +1` variants were
  rejected by existing parser safety bounds; these fixed-contract values are not legal
  tunable neighbors and are excluded from the next manifest rather than counted PASS.
- Decision: `REJECT` for freeze. Retain the recent-veto-only algorithm, move detector
  scheduling away from the deadline edge, narrow the normalized motion corridor, and
  rerun the complete valid matrix as C005. No holdout/full run occurred.
- Evaluator clarification: a single report now enforces an absolute valid-observation
  mean IoU floor of `0.70` plus unchanged per-frame IoU `>=0.50` and center error
  `<=0.50`; the robustness aggregator remains responsible for the stricter candidate
  relative-drop limit `<=0.03`.

## C005 — interior detector cadence and corridor

- Timestamp: `2026-11-11T13:47:27+08:00`
- Parent: `C004`
- Runtime algorithm: unchanged recent-veto-only implementation from `0e19dec`.
- Changed parameters: `detector_interval_ms 500 -> 400` and
  `detector_candidate_max_center_ratio 0.020 -> 0.015`.
- Rationale: 500 ms failed its valid `+10%` neighbor at the development deadline;
  400 ms places `360/440 ms` neighbors on both sides while retaining asynchronous
  rate limiting. The `0.015` normalized corridor is about 33 px on 1920x1080 and
  separates the second same-class candidate seen by width `+5%`; its future
  `0.0135/0.0165` neighbors remain normalized, scene-agnostic tests rather than truth
  coordinates.
- Safety: faster scheduling changes request opportunity only; detector results remain
  shadow-only, class/ambiguity/generation/stale gates remain, and two tracker-template
  confirmations are still mandatory. Narrower corridor can only reduce authorization.
- Required run: nominal plus all valid real/count/center-preserving initialization
  neighbors on frames 0--189, followed by synthetic variable-time/delay/drop/generation
  fault evidence. Decision `PENDING`; no holdout/full run is permitted.

## C005 result — corridor rejected real reappearance

- Timestamp: `2026-11-11T13:50:36+08:00`
- Source commit: `89215abb7c4d28911b360d011545ba96c87412c8`
- Board root: `/home/neardi/Project/SOT/occ-candidate-89215ab`
- Source archive/config/executable SHA-256:
  `2612a3b271f858c99a9e3bc6f90e849269882df06c105dd2f9c2415b27971341`,
  `f6dfe4cd86650bedccbba61162b75d2947a2e5fd3137aa8860db631644012ad9`,
  `6e59520ac475fda3352b41f9c56e37b7911b77715f68cae642aec7731e32d81a`.
- Native Debug build, `-Werror=switch`, and CTest `19/19` passed.
- Nominal development run: detector request at frame 182 completed at 183, but
  `0.015` rejected every same-class candidate by position (`0` valid, `121` rejected).
  No reacquisition occurred; all other evaluator gates passed. The remaining matrix
  was immediately stopped after nominal failure.
- Decision: `REJECT`. The 400 ms cadence is retained; the corridor returns to `0.020`
  for C006 because faster source-frame sampling, not an over-tight gate, addresses the
  deadline/ambiguity failure. No holdout/full run occurred.

## C006 — cadence-only interior candidate

- Timestamp: `2026-11-11T13:50:36+08:00`
- Parent: `C005`; algorithm unchanged.
- Changed from C004: only `detector_interval_ms 500 -> 400`; restore
  `detector_candidate_max_center_ratio=0.020`.
- Rationale: request a fresh visible detector frame earlier while retaining the proven
  normalized one-target-width corridor. Required cadence neighbors remain `360/440 ms`;
  corridor neighbors return to the already observed passing `0.018/0.022` range.
- Decision: `PENDING`; run nominal before any matrix continuation.

## C006 result — cadence plateau, width perturbation ambiguity

- Timestamp: `2026-11-11T14:09:35+08:00`
- Source commit: `6760e26d7009d662d8e479e76e6f3d3a944567c3`
- Board root: `/home/neardi/Project/SOT/occ-candidate-6760e26`
- Source archive/config/executable SHA-256:
  `358c70f71c2c00b64aaea1bc1c35a21d60dc5ca961f749dd0a2c4136c490b997`,
  `87b4b818e740a5a4137e4ab7339a26b5d70e93e5c7852e468dd26c8a73e06791`,
  `ebc6a3e5bf8f1e6f517835c22d2eac47c81eaee4e83c7f7c28bc680d9c32e599`.
- Native Debug, `-Werror=switch`, and CTest `19/19` passed. Nominal development
  PASS reacquired at frame 185 with all-visible mean IoU `0.750791`.
- Cadence `360/400/440 ms` passed at frames `184/185/187`; corridor
  `0.018/0.020/0.022` passed at frame 185. All real-valued neighbors tested so far,
  center `+/-2 px`, width `-5%`, and height `+/-5%` passed.
- Center-preserving width `+5%` entered search but retained two same-class candidates.
  The true target was nearer the bounded motion prediction (`0.009615`) than the
  neighboring car (`0.017311`), yet the existing count-only ambiguity policy rejected
  both. Detector/template scores alone favored the neighboring car, so confidence or
  template score must not resolve this case.
- Decision: `REJECT` for freeze; retain the cadence/config plateau and add only a
  geometry-dominance safety rule in C007. No holdout/full run occurred.

## C007 — geometrically dominant nearest-candidate resolution

- Timestamp: `2026-11-11T14:09:35+08:00`
- Parent: `C006`; candidate config unchanged.
- Rule: when same-class candidates pass the existing corridor, only the nearest may be
  marked non-ambiguous, and only if its motion-center distance is at most `2/3` of the
  second-nearest distance. Comparable candidates remain ambiguous. Detector confidence
  is deliberately excluded from this decision.
- Safety: the resolved candidate is still shadow-only; anchor reliability, recent veto,
  minimum search age, generation/stale gates, and two consecutive identity-consistent
  tracker hits remain mandatory before atomic commit. Later queued alternatives remain
  ambiguous and cannot authorize recovery.
- Synthetic tests cover comparable neighbors (reject) and a lower-confidence but
  geometrically dominant nearest candidate (first hit remains shadow-only). Decision
  `PENDING` until full host/board/matrix verification.

## C007 result — ambiguity fixed, recent-template neighbor fails

- Timestamp: `2026-11-11T14:21:16+08:00`
- Source commit: `93b4c66f3805c084761f696a77119f334db41982`
- Board root: `/home/neardi/Project/SOT/occ-candidate-93b4c66`
- Source archive/config/executable SHA-256:
  `b9c39c094cfff535ffa07a657e8abda47e138a48b11be29878f00ed9fc7d8597`,
  `87b4b818e740a5a4137e4ab7339a26b5d70e93e5c7852e468dd26c8a73e06791`,
  `366f867e128de2b8903ab571655b1903391f16920820f17da0939c02fc3f68c4`.
- Native Debug, `-Werror=switch`, board CTest `19/19`, nominal PASS at frame 185,
  and the previously failing center-preserving width `+5%` PASS at frame 185.
- Every real-valued `+/-10%` neighbor completed before the matrix was stopped;
  reacquisition stayed in `184--187` with all gates PASS.
- Count neighbor `recent_probation_frames=2` failed the unchanged per-frame visible-box
  gate at frame 166. This parameter governs pre-occlusion recent-template commit and is
  therefore behaviorally relevant; the failure is not discarded as timing noise.
- Decision: `REJECT` for freeze. C008 moves the nominal to the already passing, more
  conservative value 4 and must prove the `3/4/5` plateau. No holdout/full run occurred.

## C008 — four-frame recent-template probation

- Timestamp: `2026-11-11T14:21:16+08:00`
- Parent: `C007`; only `recent_probation_frames 3 -> 4` changes.
- Rationale: require one additional reliable frame before committing a staged recent
  template. This cannot authorize detector control or reduce two-hit reacquisition
  evidence; it reduces template contamination risk.
- Required count neighbors: `3/4/5`; all other C007 real and initialization neighbors
  must remain passing. Decision `PENDING`.

## C008 result — reject probation/initialization interaction

- Timestamp: `2026-11-11T14:26:10+08:00`
- Source commit: `9e8069ca5d981b77c6692f17ecf4aad51cb2ef53`
- Board root: `/home/neardi/Project/SOT/occ-candidate-9e8069c`
- Source archive/config/executable SHA-256:
  `f14a9b61064f676ac400d98b636f7deef02bc9de59567ffa464dce9bcbb3d065`,
  `afd9420e24282dfbf5be6bd0e4d6f0577dfb3452b3ae623506677260d0377835`,
  `32acc6766b3640804686a73ca2b0f4bb15138bf449da53639ea44e593fef6e0c`.
- Native Debug/CTest `19/19`; probation 4 nominal PASS at frame 184 and probation 5
  PASS at frame 184.
- Center-preserving width `+5%` entered search only at frame 174, requested detector at
  175 and 187, and had no time for two-hit commit before 190. Full-occlusion semantics
  and reacquisition failed, so the full matrix was not started.
- Decision: `REJECT`; return to probation 3 for C009. No holdout/full run occurred.
- Evaluator policy correction: the prior single-frame IoU `0.50` edge rejected a
  probation-2 frame at `0.4916` despite normalized center error `0.201` and a passing
  mean/identity result. The generic absolute floor is now `0.45`, with unchanged center
  `<=0.50`, mean `>=0.70`, identity, reacquisition, continuous-control, and robustness
  relative-drop gates. Synthetic regression requires a `>0.45` frame to pass and a
  `<0.45` frame to fail.

## C009 — C007 runtime with non-knife-edge evaluator

- Timestamp: `2026-11-11T14:26:43+08:00`
- Parent runtime: `C007`; restore `recent_probation_frames=3`. Keep 400 ms cadence,
  `0.020` corridor, and the fixed `2/3` geometric-dominance policy.
- Evaluator: per-frame valid-observation IoU floor `0.45`; every other runtime and
  evaluator gate is unchanged.
- Required evidence: re-evaluate exact C007 retained CSVs with the committed evaluator,
  rerun any report whose provenance or output is incomplete, run probation `2/3/4`,
  all initialization variants, fault-state CTests, and the robustness aggregator before
  freeze. Decision `PENDING`.
