# NanoTrack V3 Batch × GOT Factorial Comparison and Hard-Sample Fine-Tuning Specification

## Goals and Evidence Boundaries

Use the frozen Stage 1 / g5000 checkpoint as the only initialization, without accessing the
historical hidden sets, training on validation sequences, or running `sot-v1.mp4` regression:

1. Correct model selection for the next experiment by removing the duplicate UAV123 weight
   from the composite score;
2. Use a controlled factorial comparison to select a more suitable batch size and GOT-10K
   proportion for the current task;
3. Audit and correct positive-pair semantics for the sparse GOT frame manifests;
4. Analyze failure types in existing validation trajectories and construct matching hard
   samples exclusively from the training split;
5. Freeze a new model identity and enter one-time final evaluation only after the new
   candidate passes the frozen final gates.

This specification produces evidence only for desktop CUDA training and evaluation. It
does not establish ONNX, RKNN, RK3566, or real gimbal-pod acceptance. Historical GOT v1/v2
code semantics, artifacts, reports, and conclusions remain unchanged.

## Fixed Inputs, Outputs, and Identities

- Coordinator baseline: `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`.
- Initialization checkpoint:
  `artifacts/nanotrack_conservative_three_stage/run/stage1/candidate_stage1_g00005000_e001_s02500.pth`,
  SHA-256 `40ebbe4296938bf5d53c8605ab05e729b306fc0bce65921a57ab6613030410d1`.
- Inference configuration: `configs/nanotrack_v3_three_stage_stage1.yaml`, SHA-256
  `cb3065982818a701a7ebb469c41b894b0a6c11e7aac6cb91dc42e5ed50547f2e`.
- Reuse the prepared and audited GOT train, GOT val, UAV123, UAVDT, VisDrone-MOT, and
  VisDrone-SOT data; exclude validation and historical hidden sequences from training.
- New configuration directory: `configs/nanotrack_factorial_v3/`.
- New generated artifact directory: `artifacts/nanotrack_factorial_v3/`, outside Git;
  Git tracks only code, specifications, plans, report text, hashes, commands, and
  verification summaries.
- Random seeds: `123456` and `20261011` for screening; add `20261024` for confirmation.

## Scoring and Gates

### Category Count Audit

Identifiable categories in the current four aerial training manifests account for
240,622 valid frame references in the normal manifests:

| Category | Tracks | Valid frame references |
| --- | ---: | ---: |
| person | 1,718 | 52,198 |
| vehicle | 3,959 | 102,046 |
| bike | 823 | 20,878 |
| other | 25 | 5,141 |
| unknown | 70 | 60,359 |

Among identifiable labels, person appears only in UAV123 and VisDrone-MOT. All entries
in the current UAVDT manifest are vehicle, and all entries in the current VisDrone-SOT
manifest are unknown. This establishes only that identifiable person labels are fewer
than vehicle labels; it does not establish that the original VisDrone-SOT imagery lacks people.

The current fixed validation evidence contains 35 person sequences, 6,045 scored frames,
and approximately 5 independent source-video groups; vehicle has 57 sequences, 9,086
scored frames, and approximately 7 independent source-video groups. Consecutive frames
are highly correlated; scoring and confidence intervals must not treat frame counts as
independent sample sizes.

### T for the New Experiment

Training-sample frequency reflects data availability rather than business importance.
When ground vehicles and people are both formal drone-tracking targets, do not reduce
the person weight based on training counts. The old domain-first T uses person 0.4,
vehicle 0.4, and overall 0.2 within each domain, but renormalization of missing terms
gives approximate direct nominal weights of person 0.20, vehicle 0.3667, and overall
0.4333 in the four-domain average. It therefore does not give categories equal global weight.

Add a versioned category-first score for the new experiment without changing historical
scoring functions:

- `P`: macro-average across aerial domains with a person metric;
- `V`: macro-average across aerial domains with a vehicle metric;
- `D`: four-domain macro-average of overall AUC for UAV123, UAVDT, VisDrone-MOT, and VisDrone-SOT;
- `T_v3 = 0.4 * P + 0.4 * V + 0.2 * D`.

Compute `P`, `V`, and `D` at the sequence level first, then macro-average across domains;
never weight by frame counts, track counts, or training-manifest frequency. UAV123 is no
longer an additional weighted term. GOT and UAV123 constrain degradation against the fixed
initialization baseline rather than contributing again to the ranking score. Reports must
include `P`, `V`, `D`, `T_v3`, and 95% bootstrap intervals grouped by source video. These
intervals express uncertainty in the current small validation panel; correlated frames
must not be presented as independent observations.

A candidate is guardrail-qualified only when all of the following hold:

- GOT-val AO degrades by no more than 0.01 relative to the fixed initialization baseline;
- UAV123-val AUC degrades by no more than 0.01 relative to the fixed initialization baseline;
- Cross-domain person and vehicle macro-averages each degrade by no more than 0.015;
- Overall AUC for UAVDT, VisDrone-MOT, and VisDrone-SOT each degrades by no more than
  0.015; UAV123 retains the stricter 0.01 gate above.

For stage ranking, filter by guardrails first, then sort by descending `T_v3`. Replacing
g5000 additionally requires an improvement of at least 0.005 in `T_v3` aggregated over
three seeds. A single-seed result, training loss, or memory usage cannot trigger replacement
alone. Retain the old `composite_score` field and domain-first `target_score` only for
historical compatibility reads; new experiment state uses `target_score_v3`, the scoring
version, and explicit guardrail results.

## GOT Frame-Pair Semantics and Audit

First produce a read-only audit from existing manifests to quantify actual original-frame
gaps when the old implementation applies `FRAME_RANGE` to filtered-list indices. Enable
the original-frame-ID policy only for subpools whose names start with `GOT_`:

- Select the template uniformly from the filtered, ascending list of original frame IDs;
- Search candidates must satisfy `abs(search_id - template_id) <= FRAME_RANGE`;
- The template frame itself is always eligible;
- Select the search frame using the NumPy random state already frozen by the trainer,
  preserving reproducibility by sample index;
- Aerial subpools retain the historical list-position policy to avoid changing their
  training distribution at the same time.

Keep the implementation in controlled code in the main repository; do not directly commit
or depend on uncommitted changes in the external NanoTrack checkout. The training entry
point installs an explicit pair-policy before dataset construction. Checkpoints and run
provenance must record the policy-file hash. The audit reports at least eligible candidate
counts, P50/P90/P95/max of actual frame gaps, same-frame proportion, and the proportion
of old candidates excluded by the new policy.

## 3 × 3 Equal-Sample Factorial Comparison

The two factors and their levels are:

- Batch size: `8`, `32`, `64`;
- GOT proportion: `0%`, `10%`, `30%`.

Each cell starts from the same g5000 checkpoint and a new optimizer. Apart from batch
size and GOT proportion, keep frozen layers, BN, loss, augmentation, seeds, sample order,
evaluation panels, and inference configuration identical. Do not construct GOT subpools
at 0% GOT. At a nonzero GOT proportion, divide the GOT quota equally among person,
vehicle, and other. Renormalize the remaining aerial quota using the previous Stage 1
four-domain ratio `16.25:22.5:22.5:8.75`, and use the largest-remainder method to close
the integer total exactly.

The primary comparison uses an equal-sample budget; distinguish equal sample counts
from equal update counts:

- Configure a total budget of 40,960 sample pairs per seed. Decay LR logarithmically
  from `2e-5` to `8e-6` by the fraction of samples seen, without scaling LR by batch size;
- Screen only through 10,240 sample pairs, saving and evaluating every 2,560 pairs;
  batch 8/32/64 gives candidate intervals of 320/80/40 optimizer steps;
- For the same seed/GOT proportion, every batch size consumes the same ordered 10,240
  samples and the same per-sample augmentation seeds;
- Each training segment records actual batch size, sample count, update count, initial
  and final LR, loss finiteness, wall-clock time, and peak CUDA memory. Record any
  OOM/NaN/Inf as a failure; do not silently change batch size within a segment.

Screen cells at the 10,240-sample node for both seeds. Only cells whose two seeds both
pass guardrails enter the normal candidate set. If all fail, report that no safe
combination exists. The least-degraded cell may continue for diagnosis, but this is not
promotion. Extend the cell with the best mean T and cells within 0.002 of it, at most
three cells, to 40,960 samples and add the third seed. During confirmation, choose among
the four pre-existing sample nodes by three-seed mean `T_v3`; do not choose different
training durations for individual seeds.

If two cells differ by less than 0.002 in three-seed mean `T_v3` and their 95% bootstrap
intervals grouped by independent video overlap heavily, conclude that accuracy is
indistinguishable. Also compare grouped intervals for `P` and `V`; overall intervals must
not hide clear degradation in a single category. Subject to the memory gate and
reproducibility, choose the batch with shorter measured training wall-clock time. Add
an equal-update diagnostic at the same GOT proportion only when needed to explain
whether update count causes a batch effect. Label its different sample count explicitly
and exclude it from the primary ranking.

## Validation Failure Analysis and Hard Samples

Use existing validation predictions only for diagnosis. For each sequence, the tools
output per-frame IoU, the first sustained-drift interval, zero-IoU runs, scale ratios,
normalized displacement, and boundary-truncation clues, and generate small contact sheets
for priority sequences. Automatic classification uses only demonstrable geometric or
annotation evidence. If existing annotations cannot establish occlusion, similar-target
interference, or identity switching, label the case `ambiguous_requires_human_review`
instead of forcing a classification.

Select hard samples afresh only from the training split. Reuse existing
small/scale/motion/occlusion/distractor source annotations and geometric rules; output an
independent manifest and provenance, and verify disjoint video IDs against all
validation/hidden data. After determining the winning factorial recipe, compare branches
without extra hard samples and with matching hard samples using the same starting
point, sample budget, seeds, and evaluation gates. The hard-sample branch may enter the
final candidate set only if the controlled comparison demonstrates improved `T_v3` and
all guardrails pass.

## Hidden Sets, Reports, and Stop Conditions

- Access to GOT train-hidden is prohibited until the final model identity is frozen
  and the final replacement gate passes. If no model passes, retain g5000 and do not
  run hidden-set evaluation.
- The previously viewed historical aerial hidden panel is no longer an untouched test
  set; final aerial generalization claims require a separately confirmed independent holdout.
- Generate only the Chinese final report `REPORT.zh-CN.md` for this experiment. Preserve
  all historical English and Chinese reports without overwriting them; do not create or
  overwrite `REPORT.md` for this experiment.
- Write all subsequently added or revised specifications and implementation plans for
  this task in English; preserve commands, paths, code identifiers, and existing interface names.
- Reports must distinguish completed training, desktop evaluation, model promotion,
  conversion, development-board evidence, and gimbal-pod evidence.
- At each material milestone, commit code/documentation together with
  `docs/progress/nanotrack-finetune.md` and return the commit ID to the coordinating chat.
  Do not modify `STATUS.md`, `integration-contract.yaml`, or another task's progress file.
