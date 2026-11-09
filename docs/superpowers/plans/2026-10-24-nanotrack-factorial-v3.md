# NanoTrack V3 Factorial Experiment Implementation Plan

> **For the executing agent:** Use `superpowers:subagent-driven-development` (recommended)
> or `superpowers:executing-plans` to implement this plan task by task. Track steps with
> checkboxes (`- [ ]`).

**Goal:** Implement and run a reproducible batch `{8,32,64}` × GOT `{0%,10%,30%}`
NanoTrack fine-tuning experiment, correct GOT frame-pair semantics, diagnose validation
failures, and test hard-sample reinforcement sourced only from training data without
contaminating validation or hidden sets.

**Architecture:** Preserve historical v1/v2 behavior and add versioned v3 policy,
configuration, control, diagnosis, verification, and reporting modules. Continue training
with the official NanoTrack checkout, but use a runtime frame-pair policy adapter tracked
by the main repository to provide original-frame-ID pairing only for GOT. Exclude
uncommitted external-repository changes from experiment results.

**Technology stack:** Python 3.7, PyTorch 1.7.1/CUDA 10.2, NumPy, OpenCV, YAML,
`unittest`, and the official NanoTrack V3 checkout.

**Specification:** `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md`

## Global Constraints

- Work on `codex/nanotrack-factorial-v3`, created from
  `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`; do not implement on `main`.
- Do not modify historical GOT v1/v2 artifacts or reinterpret their model-selection rules.
- Do not train on validation/hidden sequences. Do not access GOT train-hidden before
  freezing a final qualified model. Do not run `sot-v1.mp4`.
- Keep generated models, logs, images, videos, and experiment JSON outside Git. Commit
  only paths, hashes, commands, reports, and verification summaries.
- Modify only NanoTrack fine-tuning code/documentation and
  `docs/progress/nanotrack-finetune.md`; do not modify shared coordination files or
  another task's progress file.
- Follow RED → GREEN for every behavior change. Commit code/documentation together
  with the task progress file at each material milestone.
- Use the equal-sample experiment as primary evidence for the full factorial comparison.
  Label equal-update diagnostics separately and never include them in the primary ranking.
- Write this plan and all subsequently added or revised specifications and plans for
  this task in English; preserve commands, paths, and code identifiers.
- Generate only the Chinese final report `REPORT.zh-CN.md` for this experiment. Preserve
  historical English and Chinese reports without overwriting them, and do not create or
  overwrite `REPORT.md` for this experiment.

## Review Focus

- After adding v3, historical v2 verification and replay must retain the original
  `0.70/0.15/0.15` semantics; Task 1 adds explicit compatibility tests.
- The new `T_v3` must assign person and vehicle 0.4 each at the category level, without
  weighting by frame or training counts. Preserve the old domain-first `target_score`;
  Task 1 tests both semantics.
- For sparse, nonconsecutive frame IDs such as `[1, 50, 200]`, GOT must filter by original
  ID gaps while historical aerial pairing still uses list positions; Task 2 tests both paths.
- GOT 0% must contain no GOT entries. Every batch/proportion configuration must total
  exactly 40,960 samples; Task 3 tests all nine cells.
- Resuming must reuse completed training/evaluation and reject mismatched
  checkpoint/config/pair-policy hashes; Task 4 tests interrupted resumes and identity mismatches.
- Validation sequence IDs and aliases/segments of the same source video must not enter
  hard-training manifests; Task 5 tests source-group isolation.

---

## Pre-Execution Checks

- The versioned code root is
  `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`.
- The large-artifact root remains `/home/u20/single-object-tracking`. Before Task 1,
  create Git-ignored worktree symlinks only for missing `.venvs`, `data`, `artifacts`,
  and `repos`; verify each using `readlink -f` before use. Do not copy generated
  artifacts between worktrees.
- Record the observed baseline test results in the ledger: `166` tests, with `5`
  failures and `1` error caused by historical absolute-path/worktree assumptions.
  The user must choose to accept this known boundary and continue, or repair compatibility
  first; the executor must not silently mark this baseline as fully passing.
- Recheck the frozen checkpoint and configuration hashes from the specification before
  any training command.

---

### Task 1: Versioned Category-First Scoring Policy

**Files:**

- Create: `scripts/nanotrack_factorial_policy.py`
- Create: `tests/test_nanotrack_factorial_policy.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**

- Input: the metric mapping produced by `scripts.run_nanotrack_got_replay.build_metrics`.
- Outputs: `target_score_v3(metrics) -> float`, `guardrail_decision(candidate, baseline) -> dict`,
  `rank_key(metrics, baseline) -> tuple`, `final_promotion(candidate_by_seed, baseline) -> dict`,
  and a scoring-version string recorded in subsequent state files.

- [ ] **Step 1: Write failing policy tests.** Cover 0.4 each for `P`/`V`, `D` as the
  four-domain overall macro-average, averaging a missing category only across domains
  containing its metric, prohibition of frame-count weighting, ranking solely by `T_v3`
  when UAV123/GOT directions conflict, every fixed-baseline gate, three-seed
  `T_v3 >= baseline + 0.005`, and nonfinite inputs. Assert that the old `target_score`
  and `composite_score` retain historical semantics.
- [ ] **Step 2: Run** `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_policy -v`.
  **Expected:** Import failure because the v3 policy module does not yet exist.
- [ ] **Step 3: Implement the minimal policy module.** Add category-first
  `target_score_v3`, reusing metric reads without changing old scoring functions.
  Add degradation gates for person, vehicle, GOT, UAV123, and each aerial domain.
- [ ] **Step 4: Run the new tests and historical controller/reporting tests.**
  **Expected:** All pass, and old v2 values remain reproducible with their original semantics.
- [ ] **Step 5: Record policy status/evidence in this task's progress file and commit**
  `feat: add category-first NanoTrack selection policy`.

### Task 2: Original-Frame-ID Pairing Policy and Audit

**Files:**

- Create: `scripts/nanotrack_pair_sampling.py`
- Create: `scripts/audit_nanotrack_frame_pairs.py`
- Create: `tests/test_nanotrack_pair_sampling.py`
- Modify: `scripts/train_nanotrack_uav123.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**

- Inputs: `SubDataset` instances exposing `name`, `frame_range`, `labels`, `videos`,
  and `get_image_anno`, plus a JSON pairing policy mapping dataset prefixes to
  `original_frame_id`.
- Outputs: `eligible_search_frames(frames, template_id, frame_range, mode) -> list`,
  `install_pair_policy(subdataset_class, policy) -> None`, the `--pair-policy` command-line
  option, and audit JSON used for provenance in Task 4.

- [ ] **Step 1: Write failing unit/integration tests.** Assert that frames `[1,50,200]`,
  range 60, and template 50 produce `[1,50]` in original-ID mode and all three frames in
  historical position mode. After installing the policy only for GOT, a fake aerial
  dataset must still use the original method. Invalid/unknown policies must fail before training.
- [ ] **Step 2: Run focused tests.** **Expected:** Import/command-line failure due to
  missing functionality.
- [ ] **Step 3: Implement pair selection and the runtime adapter, and extend the trainer
  command line.** Record pair-policy SHA-256 in checkpoints and reject a different hash on resume.
- [ ] **Step 4: Add and run the audit command**, reading existing GOT manifests and writing
  to `artifacts/nanotrack_factorial_v3/pair-audit/`. Verify P50/P90/P95/max, same-frame
  proportion, excluded old-candidate proportion, manifest hashes, and sequence/frame counts.
- [ ] **Step 5: Run training-resume tests and all frame-pair tests.** **Expected:** All pass,
  with no modified files under `repos/siamtrackers/NanoTrack`.
- [ ] **Step 6: Record audit paths/hashes in this task's progress file and commit**
  `feat: use original frame ids for GOT pair sampling`.

### Task 3: Exact-Sample Configuration Generator for Nine Cells

**Files:**

- Create: `scripts/generate_nanotrack_factorial_configs.py`
- Create: `tests/test_nanotrack_factorial_configs.py`
- Generate configurations under `configs/nanotrack_factorial_v3/`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**

- Inputs: batch levels `(8,32,64)`, GOT proportions `(0.0,0.1,0.3)`, 40,960 total
  samples, interval 2,560, learning rate `2e-5 -> 8e-6`, and the pair-policy path/hash
  from Task 2.
- Outputs: nine immutable YAML configurations and `experiment-manifest.json` containing
  exact quotas, update counts, sample counts, seeds, input hashes, and the output root.

- [ ] **Step 1: Write failing table-driven tests for all nine cells.** Manually verify
  totals, no GOT entries at GOT 0%, equal GOT category quotas within integer rounding,
  normalized aerial proportions, divisible evaluation intervals, identical learning
  rates across batch sizes, and absolute paths rooted at the experiment artifact root.
- [ ] **Step 2: Run focused tests.** **Expected:** Generator import failure.
- [ ] **Step 3: Implement quota/configuration/manifest generation.** Keep the full plan
  at 40,960 samples; the controller may initially stop at 10,240 samples.
- [ ] **Step 4: Generate configurations, instantiate a real `BANDataset` for each, and
  draw samples without optimization.** **Expected:** All nine load successfully; every
  decoded sample tensor is finite and has the correct shape.
- [ ] **Step 5: Run configuration, data-loader, and historical GOT configuration tests.**
  **Expected:** All pass; v1/v2 generators remain unchanged.
- [ ] **Step 6: Record manifest/configuration hashes in this task's progress file and commit**
  `feat: generate NanoTrack batch GOT factorial configs`.

### Task 4: Resumable Factorial Experiment Controller and Verifier

**Files:**

- Create: `scripts/run_nanotrack_factorial.py`
- Create: `scripts/verify_nanotrack_factorial.py`
- Create: `tests/test_nanotrack_factorial_controller.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**

- Inputs: the Task 1 policy, Task 2 pair-policy/audit, Task 3 manifest/configurations,
  the existing trainer, and three desktop evaluators.
- Outputs: transactional `candidate_table.json`, `cell_summary.json`, screening/extension
  decisions, exact commands/logs, checkpoint/evaluation hashes, and read-only verification results.

- [ ] **Step 1: Write failing fake end-to-end tests.** Cover nine cells × two seeds × four
  nodes, sample-to-update boundaries, interrupted reuse, identity-mismatch rejection,
  absence of hidden-set/video commands, guardrail filtering, at most three extension
  cells, diagnostic state when no safe combination exists, and binding to the `T_v3`
  scoring version.
- [ ] **Step 2: Run focused tests.** **Expected:** Controller import failure.
- [ ] **Step 3: Implement the controller using existing evaluator primitives and new
  state/schema.** Bind every persisted result to code/config/checkpoint/pair-policy/scoring-policy
  hashes; reuse completed nodes idempotently.
- [ ] **Step 4: Implement the verifier.** Without rewriting evidence, verify the full
  expected matrix, exact quotas/sample counts, two-seed coverage, finite metrics,
  artifact hashes, scoring version, and absence of hidden-set/video access.
- [ ] **Step 5: Run fake-controller tests, historical controller tests, and verifier
  command-line help/tests.** **Expected:** All pass.
- [ ] **Step 6: Update this task's progress file and commit**
  `feat: add resumable NanoTrack factorial controller`.

### Task 5: Validation Failure Diagnosis and Training-Only Hard-Sample Pool

**Files:**

- Create: `scripts/analyze_nanotrack_failures.py`
- Create: `scripts/build_nanotrack_failure_matched_pool.py`
- Create: `tests/test_nanotrack_failure_analysis.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**

- Inputs: existing per-sequence baseline/candidate predictions, validation GT, prepared
  training split/group metadata, and existing hard-reason annotations.
- Outputs: per-sequence diagnostic JSON, a bounded number of contact sheets, an ambiguity
  queue, and a training-only matched hard-sample manifest with source-group-disjoint provenance.

- [ ] **Step 1: Write failing synthetic tests.** Cover sustained drift, zero-IoU runs,
  scale/motion geometry, boundary truncation, occlusion/distractor classification with
  insufficient evidence, and rejection of validation-source aliases in training manifests.
- [ ] **Step 2: Run focused tests.** **Expected:** Analysis module missing.
- [ ] **Step 3: Implement geometric diagnosis and contact-sheet generation.** Do not
  infer occlusion or identity switching without evidence; output
  `ambiguous_requires_human_review`.
- [ ] **Step 4: Build a training-only pool from existing training annotations/reasons**,
  and perform exact source-video-disjoint checks against validation/hidden panels.
- [ ] **Step 5: Run the tools on existing g5000/v2 results.** Inspect generated contact
  sheets. Only queued identity/occlusion ambiguities require human assistance; continue
  other unambiguous work automatically.
- [ ] **Step 6: Run focused tests and the provenance verifier, record paths/hashes in
  this task's progress file, and commit**
  `feat: diagnose NanoTrack failures without validation leakage`.

### Task 6: Run Screening and Confirmation Experiments

**Files:**

- Generate only: `artifacts/nanotrack_factorial_v3/`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**

- Inputs: Tasks 1–4 and the frozen g5000 identity.
- Outputs: measured nine-cell/two-seed screening results, confirmation results for at
  most three cells/full budget/third seed, and batch/GOT decisions with uncertainty and
  timing evidence.

- [ ] **Step 1: Recheck `nvidia-smi`, CUDA/PyTorch availability, checkpoint/config/data
  hashes, free disk space, and other training processes.** Record exact output; stop
  immediately on an identity mismatch.
- [ ] **Step 2: Run real forward/backward probes for batch 8, 32, and 64 using the
  corrected pairing policy.** **Expected:** Each completes 50 finite steps, or explicitly
  records the failed cell; no silent downgrade.
- [ ] **Step 3: Run 18 screening trajectories** through 10,240 samples and evaluate the
  four fixed nodes. Resume after interruptions rather than restarting.
- [ ] **Step 4: Verify the complete screening matrix** and freeze extension decisions
  before continuing training.
- [ ] **Step 5: Continue selected cells to 40,960 samples and run seed `20261024`.
  Evaluate fixed nodes and compute source-video-grouped bootstrap intervals for `P`,
  `V`, `D`, and `T_v3`.** If no cell is safe, run only the declared diagnostic continuation
  and retain g5000.
- [ ] **Step 6: If the batch conclusion remains causally ambiguous and specification
  conditions hold, run a separately labeled equal-update diagnostic.** Never include
  its metrics in the primary ranking.
- [ ] **Step 7: Record commands, interruptions, metrics, hashes, blockers, and the
  selected batch/GOT status in this task's progress file; commit**
  `docs: record NanoTrack factorial experiment evidence`.

### Task 7: Controlled Hard-Sample Follow-Up, Chinese Report, and Final Verification

**Files:**

- Create: `scripts/report_nanotrack_factorial.py`
- Create: `tests/test_nanotrack_factorial_reporting.py`
- Generate outside Git: `artifacts/nanotrack_factorial_v3/REPORT.zh-CN.md`, verification
  JSON, and candidate artifacts
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**

- Inputs: the confirmed factorial recipe, Task 5 matched hard-sample pool, Task 1 final
  gates, and all Task 6 evidence.
- Outputs: controlled results without and with hard-sample reinforcement, a final candidate
  or explicit nonpromotion conclusion, the Chinese report, and complete evidence verification.

- [ ] **Step 1: Write failing reporting/verifier tests.** Assert Chinese-only report
  generation, no creation or overwriting of `REPORT.md` for this experiment, frozen
  identities, separation of primary and equal-update evidence, absence of hidden-set
  results without a qualified final selection, and explicit desktop/RKNN/development-board boundaries.
- [ ] **Step 2: Run focused tests.** **Expected:** Reporting module missing.
- [ ] **Step 3: Implement Chinese report generation and final verification.** The report
  must retain failed/interrupted attempts and all protocol deviations, and list category
  counts, independent source-video group counts, and `P`/`V` confidence intervals.
- [ ] **Step 4: Run recipes without and with matched hard-sample reinforcement using
  identical starting points, budgets, seeds, nodes, and evaluators. Freeze the better
  safe result according to the predeclared `T_v3` rule.**
- [ ] **Step 5: Apply the final promotion gate.** Only after passing, freeze the checkpoint
  identity and invoke GOT train-hidden exactly once. Otherwise retain g5000 and leave
  hidden data unaccessed.
- [ ] **Step 6: Run the full repository Python test suite and experiment verifier.**
  Record every failure by name; focused tests must not be called a full pass.
- [ ] **Step 7: Generate the Chinese report, hash every retained artifact, update this
  task's progress file in the same commit, and commit**
  `docs: finalize NanoTrack factorial fine-tuning evidence`.
