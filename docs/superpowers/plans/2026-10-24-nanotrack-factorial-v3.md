# NanoTrack V3 Factorial Experiment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement and execute a reproducible batch `{8,32,64}` × GOT `{0%,10%,30%}` NanoTrack fine-tuning experiment, correct GOT frame-pair semantics, diagnose validation failures, and test training-only hard-sample remediation without contaminating validation or hidden sets.

**Architecture:** Preserve historical v1/v2 behavior and add versioned v3 policy, configuration, orchestration, diagnostics, verification, and reporting modules. Training continues to use the official NanoTrack checkout, but a tracked runtime pair-policy adapter supplies original-frame-ID pairing for GOT only, so no uncommitted external-repository edit becomes part of the result.

**Tech Stack:** Python 3.7, PyTorch 1.7.1/CUDA 10.2, NumPy, OpenCV, YAML, `unittest`, official NanoTrack V3 checkout.

**Spec:** `docs/superpowers/specs/2026-10-24-nanotrack-factorial-v3.md`

## Global Constraints

- Work on `codex/nanotrack-factorial-v3` from `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`; never implement on `main`.
- Do not modify historical GOT v1/v2 artifacts or reinterpret their selection rule.
- Do not train on validation/hidden sequences, access GOT train-hidden before a final qualified selection, or run `sot-v1.mp4`.
- Generated models, logs, images, videos, and experiment JSON remain outside Git; commit paths, hashes, commands, reports, and verification summaries only.
- Modify only NanoTrack fine-tuning code/docs and `docs/progress/nanotrack-finetune.md`; do not edit public coordination files or other task files.
- Every behavioral change follows RED → GREEN; every material milestone commits code/docs and the task progress file together.
- The full factorial comparison is equal-sample primary evidence. Any equal-update diagnostic is separately labeled and never mixed into the ranking.

## Review Focus

- A historical v2 verification or replay must retain its old `0.70/0.15/0.15` semantics after v3 is added; Task 1 adds an explicit compatibility test.
- Sparse, non-consecutive frame IDs such as `[1, 50, 200]` must be filtered by original ID gap for GOT while legacy aerial pairing remains positional; Task 2 tests both paths.
- GOT 0% must contain no GOT dataset entry, and every batch/ratio config must close exactly to 40,960 samples; Task 3 tests all nine cells.
- A resumed run must not repeat completed training/evaluation and must reject mismatched checkpoint/config/pair-policy hashes; Task 4 tests interruption and identity mismatch.
- Validation sequence IDs must never enter the generated hard-training manifest, including aliases/segments of the same source video; Task 5 tests source-group disjointness.

---

## Execution preflight

- The versioned code root is `/home/u20/single-object-tracking/.worktrees/nanotrack-factorial-v3`.
- The large-asset root remains `/home/u20/single-object-tracking`. Before Task 1, create only missing,
  Git-ignored worktree symlinks for `.venvs`, `data`, `artifacts`, and `repos`; validate every link with
  `readlink -f` before use. Never copy generated artifacts between worktrees.
- Record the observed baseline suite result (`166` tests, `5` failures and `1` error caused by historical
  absolute-path/worktree assumptions) in the ledger. The user must choose whether to proceed with this
  known boundary or first repair it; no executor may silently relabel the baseline as green.
- Recheck the frozen checkpoint and config hashes from the Spec before any training command.

---

### Task 1: Versioned target-first scoring policy

**Files:**
- Create: `scripts/nanotrack_factorial_policy.py`
- Create: `tests/test_nanotrack_factorial_policy.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**
- Consumes: metric mappings produced by `scripts.run_nanotrack_got_replay.build_metrics`.
- Produces: `guardrail_decision(candidate, baseline) -> dict`, `rank_key(metrics, baseline) -> tuple`, `final_promotion(candidate_by_seed, baseline) -> dict`, and a version string recorded by later state files.

- [ ] **Step 1: Write failing policy tests.** Cover T-only ordering when UAV123/GOT disagree, each fixed-baseline guardrail boundary, three-seed `T >= baseline + 0.005`, non-finite inputs, and a compatibility assertion showing the old `composite_score` still returns `0.70*T + 0.15*G + 0.15*U`.
- [ ] **Step 2: Run** `/home/u20/single-object-tracking/.venvs/nanotrack/bin/python -B -m unittest tests.test_nanotrack_factorial_policy -v`. **Expected:** import failure because the v3 policy module does not exist.
- [ ] **Step 3: Implement the minimal policy module.** Use the existing domain-first `target_score` and `category_macro`; do not edit old scoring functions.
- [ ] **Step 4: Run the new test and the historical controller/reporting tests.** **Expected:** all pass and old v2 values remain byte-semantically reproducible.
- [ ] **Step 5: Update the owned progress file with policy status/evidence and commit** `feat: add target-first NanoTrack selection policy`.

### Task 2: Original-frame-ID pair policy and audit

**Files:**
- Create: `scripts/nanotrack_pair_sampling.py`
- Create: `scripts/audit_nanotrack_frame_pairs.py`
- Create: `tests/test_nanotrack_pair_sampling.py`
- Modify: `scripts/train_nanotrack_uav123.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**
- Consumes: `SubDataset` instances with `name`, `frame_range`, `labels`, `videos`, and `get_image_anno`; a JSON pair-policy mapping dataset prefixes to `original_frame_id`.
- Produces: `eligible_search_frames(frames, template_id, frame_range, mode) -> list`, `install_pair_policy(subdataset_class, policy) -> None`, CLI `--pair-policy`, and an audit JSON used by Task 4 provenance.

- [ ] **Step 1: Write failing unit/integration tests.** Assert `[1,50,200]`, range 60, template 50 yields `[1,50]` in original-ID mode but all three in legacy positional mode; GOT-only installation leaves an aerial fake dataset on its original method; malformed/unknown policies fail before training.
- [ ] **Step 2: Run the focused tests.** **Expected:** import/CLI failures for missing functionality.
- [ ] **Step 3: Implement pair selection and the runtime adapter; extend the trainer CLI.** Record pair-policy SHA-256 in checkpoints and reject resume with a different hash.
- [ ] **Step 4: Add and run the audit CLI** against the existing GOT manifests, writing `artifacts/nanotrack_factorial_v3/pair-audit/`; verify P50/P90/P95/max, same-frame ratio, excluded-candidate ratio, manifest hashes, and sequence/frame counts.
- [ ] **Step 5: Run trainer resume tests plus all pair tests.** **Expected:** all pass; no file under `repos/siamtrackers/NanoTrack` is modified.
- [ ] **Step 6: Update the owned progress file with audit path/hash and commit** `feat: use original frame ids for GOT pair sampling`.

### Task 3: Nine-cell sample-exact configuration generator

**Files:**
- Create: `scripts/generate_nanotrack_factorial_configs.py`
- Create: `tests/test_nanotrack_factorial_configs.py`
- Create generated configs under: `configs/nanotrack_factorial_v3/`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**
- Consumes: batch levels `(8,32,64)`, GOT ratios `(0.0,0.1,0.3)`, total 40,960, interval 2,560, LR `2e-5 -> 8e-6`, and the Task 2 pair-policy path/hash.
- Produces: nine immutable YAML configs plus `experiment-manifest.json` containing exact quotas, steps, samples, seeds, input hashes, and output roots.

- [ ] **Step 1: Write failing table-driven tests for all nine cells.** Hand-check totals, no GOT entries at 0%, equal GOT category allocation within integer rounding, normalized aerial ratios, interval divisibility, LR equality across batches, and absolute paths rooted at the experiment asset root.
- [ ] **Step 2: Run the focused tests.** **Expected:** generator import failure.
- [ ] **Step 3: Implement quota/config/manifest generation.** Keep full schedule length 40,960 while the controller may stop first at 10,240.
- [ ] **Step 4: Generate configs and run a real `BANDataset` construction/sample smoke for every config without optimization.** **Expected:** all nine load; each decoded sample has finite tensors and expected shapes.
- [ ] **Step 5: Run config, data-loader, and historical GOT config tests.** **Expected:** all pass and v1/v2 generators remain unchanged.
- [ ] **Step 6: Update the owned progress file with manifest/config hashes and commit** `feat: generate NanoTrack batch GOT factorial configs`.

### Task 4: Resumable factorial controller and verifier

**Files:**
- Create: `scripts/run_nanotrack_factorial.py`
- Create: `scripts/verify_nanotrack_factorial.py`
- Create: `tests/test_nanotrack_factorial_controller.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**
- Consumes: Task 1 policy, Task 2 pair-policy/audit, Task 3 manifest/configs, existing trainer and three desktop evaluators.
- Produces: transactional `candidate_table.json`, `cell_summary.json`, selection/expansion decision, exact commands/logs, checkpoint/evaluation hashes, and a read-only verifier result.

- [ ] **Step 1: Write failing fake end-to-end tests.** Cover nine cells × two seeds × four nodes, exact sample-to-step boundaries, interruption reuse, identity mismatch rejection, no hidden/video commands, guardrail filtering, at-most-three expansion cells, and fallback diagnostic status when none are safe.
- [ ] **Step 2: Run focused tests.** **Expected:** controller import failure.
- [ ] **Step 3: Implement the controller using existing evaluator primitives but new state/schema.** Every persisted result binds code/config/checkpoint/pair-policy hashes; completed nodes are idempotently reused.
- [ ] **Step 4: Implement the verifier.** It must validate the full expected matrix, exact quotas/sample counts, two seed coverage, finite metrics, artifact hashes, and hidden/video absence without rewriting evidence.
- [ ] **Step 5: Run fake controller tests, historical controller tests, and verifier CLI help/tests.** **Expected:** all pass.
- [ ] **Step 6: Update the owned progress file and commit** `feat: add resumable NanoTrack factorial controller`.

### Task 5: Validation-failure diagnostics and training-only hard pool

**Files:**
- Create: `scripts/analyze_nanotrack_failures.py`
- Create: `scripts/build_nanotrack_failure_matched_pool.py`
- Create: `tests/test_nanotrack_failure_analysis.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**
- Consumes: existing baseline/candidate per-sequence predictions, validation GT, prepared training split/group metadata, and existing hard-reason annotations.
- Produces: per-sequence diagnostic JSON, bounded contact sheets, ambiguity queue, and a training-only matched hard manifest with source-group disjointness provenance.

- [ ] **Step 1: Write failing synthetic tests.** Cover sustained drift, zero-IoU runs, scale/motion geometry, boundary truncation, evidence-insufficient occlusion/distractor classification, and validation/source alias rejection in the training manifest.
- [ ] **Step 2: Run focused tests.** **Expected:** analysis modules are missing.
- [ ] **Step 3: Implement geometry diagnostics and contact-sheet generation.** Do not infer occlusion or identity switch without evidence; output `ambiguous_requires_human_review`.
- [ ] **Step 4: Implement training-only pool construction** from existing training annotations/reasons with exact source-video disjointness checks against validation and hidden panels.
- [ ] **Step 5: Run the tools on existing g5000/v2 results.** Inspect generated contact sheets; request human assistance only for queued identity/occlusion ambiguities, while continuing all unambiguous work.
- [ ] **Step 6: Run focused tests and provenance verifier, update the owned progress file with paths/hashes, and commit** `feat: diagnose NanoTrack failures without validation leakage`.

### Task 6: Execute screening and confirmation experiment

**Files:**
- Generated only: `artifacts/nanotrack_factorial_v3/`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**
- Consumes: Tasks 1–4 and frozen g5000 identity.
- Produces: observed nine-cell/two-seed screening results, up-to-three-cell/full-budget/third-seed confirmation results, and a batch/GOT decision with uncertainty and timing evidence.

- [ ] **Step 1: Recheck `nvidia-smi`, CUDA/PyTorch availability, checkpoint/config/data hashes, free disk, and absence of another training process.** Record exact output; stop on identity mismatch.
- [ ] **Step 2: Run a real forward/backward probe for batch 8, 32, and 64** using the corrected pair policy. **Expected:** 50 finite steps each or an explicit failed cell; no silent fallback.
- [ ] **Step 3: Run the 18 screening trajectories** to 10,240 samples and evaluate all four fixed nodes. Resume rather than restart after interruption.
- [ ] **Step 4: Verify the complete screening matrix** and freeze the expansion decision before further training.
- [ ] **Step 5: Continue selected cells to 40,960 samples and run seed `20261024`; evaluate fixed nodes and grouped-video bootstrap.** If no cell is safe, run only the declared diagnostic continuation and retain g5000.
- [ ] **Step 6: If the batch conclusion is causally ambiguous and the spec condition is met, run the separately labeled equal-update diagnostic.** Never add its metrics to primary ranking.
- [ ] **Step 7: Update the owned progress file with commands, interruptions, metrics, hashes, blockers, and chosen batch/GOT status; commit** `docs: record NanoTrack factorial experiment evidence`.

### Task 7: Controlled hard-sample follow-up, reporting, and final verification

**Files:**
- Create: `scripts/report_nanotrack_factorial.py`
- Create: `tests/test_nanotrack_factorial_reporting.py`
- Generate outside Git: `artifacts/nanotrack_factorial_v3/REPORT.md`, `REPORT.zh-CN.md`, verification JSON and candidate artifacts
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**
- Consumes: confirmed factorial recipe, Task 5 matched hard pool, Task 1 final gate, and all Task 6 evidence.
- Produces: no-hard vs hard-sample controlled result, final candidate or explicit no-promotion result, bilingual reports, and a complete evidence verifier.

- [ ] **Step 1: Write failing report/verifier tests.** Assert English/Chinese metric parity, frozen identities, separation of primary/equal-update evidence, no hidden result without qualified final selection, and explicit desktop/RKNN/board boundaries.
- [ ] **Step 2: Run focused tests.** **Expected:** reporting module missing.
- [ ] **Step 3: Implement report and final verification generation.** Reports must preserve failed/interrupted attempts and all protocol deviations.
- [ ] **Step 4: Run the selected recipe with and without matched hard sampling** using identical start, budget, seeds, nodes, and evaluators; freeze the better safe result by predeclared T rule.
- [ ] **Step 5: Apply the final promotion gate.** Only if it passes, freeze checkpoint identity and invoke GOT train-hidden exactly once; otherwise retain g5000 and leave hidden untouched.
- [ ] **Step 6: Run the full repository Python suite and the experiment verifier.** Record every failure by name; no partial suite may be called a full pass.
- [ ] **Step 7: Generate both reports, hash all retained artifacts, update the owned progress file in the same commit, and commit** `docs: finalize NanoTrack factorial fine-tuning evidence`.
