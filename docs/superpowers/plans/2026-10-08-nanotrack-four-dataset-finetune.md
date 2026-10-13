# NanoTrack V3 Four-Dataset Fine-tuning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prepare UAV123, UAVDT, VisDrone-SOT, and VisDrone-MOT for NanoTrack V3, run one verified four-source fine-tune, and evaluate the selected checkpoint.

**Architecture:** Keep the existing UAV123 converter and evaluator intact. Add one aerial-dataset preparation module that normalizes native SOT sequences and MOT identities into the exact `crop511` plus JSON contract consumed by NanoTrack. Use independent dataset entries and `NUM_USE` weights in YACS so every epoch has deterministic 20/30/30/20 source balance.

**Tech Stack:** Python 3.7, OpenCV, NumPy, PyTorch 1.7.1 CUDA 10.2, NanoTrack/PySOT dataset loader, unittest.

**Spec:** `docs/superpowers/specs/2026-10-08-nanotrack-four-dataset-finetune.md`

## Global Constraints

- Seed is `123456`.
- Source datasets under `/home/u20/dataset` are read-only inputs.
- Batch size is `8`; the GPU has 6 GiB VRAM.
- Train sources are UAV123 train, both VisDrone-SOT train parts, VisDrone-MOT train, and UAVDT `M_attr/train` only.
- Converted MOT tracklets require at least 30 valid frames and retain at most 60 deterministic frames.
- Full training is five epochs with 20,000 pairs per epoch and 20/30/30/20 source sampling.
- An accuracy regression is a valid experimental result and must be reported, not hidden.

## Review Focus

- VisDrone ignored regions or zero-confidence rows must never become SOT targets; covered by Task 1 parser tests.
- MOT target IDs from different source videos must never collide; covered by Task 1 naming tests.
- Long MOT annotation gaps must not form false positive pairs; covered by Task 1 segmentation tests.
- Duplicate VisDrone-SOT train roots must not overwrite one another; covered by Task 1 collision tests.
- A missing crop or malformed annotation must fail the audit before training; covered by Task 2 integration tests.

---

### Task 1: Normalize aerial annotations into tracklets

**Files:**
- Create: `scripts/aerial_sot_nanotrack.py`
- Create: `tests/test_aerial_sot_nanotrack.py`

**Interfaces:**
- Consumes: official VisDrone-SOT, VisDrone-MOT, and UAVDT directory layouts.
- Produces: `Tracklet`, parser functions, deterministic gap splitting and frame subsampling.

- [ ] Write synthetic parser tests for VisDrone-SOT, VisDrone-MOT filtering, UAVDT train-list filtering, gap splitting, and collision-free names.
- [ ] Run the tests and confirm they fail because the module does not exist.
- [ ] Implement the minimal parser and normalization interfaces.
- [ ] Run the focused tests and the complete unittest suite.

### Task 2: Build audited crop511 datasets and manifests

**Files:**
- Modify: `scripts/aerial_sot_nanotrack.py`
- Modify: `tests/test_aerial_sot_nanotrack.py`
- Create: `data/nanotrack_four_dataset/` generated artifacts

**Interfaces:**
- Consumes: Task 1 `Tracklet` values.
- Produces: per-source `crop511`, `train.json`, `audit.json`, and `prepare_summary.json`.

- [ ] Add failing integration tests for crop geometry, PySOT JSON naming, missing-image failure, and nonzero per-source coverage.
- [ ] Implement resumable parallel crop generation and immutable manifests.
- [ ] Run focused and full tests.
- [ ] Audit and prepare all three new datasets; reference the existing UAV123 prepared dataset without copying it.

### Task 3: Configure balanced training and held-out evaluation

**Files:**
- Create: `configs/nanotrack_v3_four_dataset_smoke.yaml`
- Create: `configs/nanotrack_v3_four_dataset_finetune.yaml`
- Create: `scripts/evaluate_visdrone_sot_nanotrack.py`
- Create: `tests/test_visdrone_sot_evaluation.py`

**Interfaces:**
- Consumes: Task 2 prepared roots and the existing training/evaluation entry points.
- Produces: balanced four-source `BANDataset` configs and VisDrone-SOT OPE evaluation.

- [ ] Write failing tests for four-source loading weights and VisDrone sequence evaluation.
- [ ] Implement the configs and evaluator.
- [ ] Run focused and full tests.
- [ ] Verify one sample from every source can be decoded by `BANDataset`.

### Task 4: Run smoke and full fine-tuning

**Files:**
- Create: `artifacts/nanotrack_four_dataset_finetune/smoke/` generated artifacts
- Create: `artifacts/nanotrack_four_dataset_finetune/full/` generated artifacts

**Interfaces:**
- Consumes: Task 3 configs and the official NanoTrack V3 checkpoint.
- Produces: smoke checkpoint, five full checkpoints, metrics JSONL, resolved configs, and logs.

- [ ] Run a bounded CUDA smoke train and confirm finite loss and peak memory below 6 GiB.
- [ ] Reload the smoke checkpoint and run held-out inference.
- [ ] Run the five-epoch full training with persistent logs and checkpoints.
- [ ] Verify checkpoint count, finite losses, and checkpoint reload.

### Task 5: Select, evaluate, and report

**Files:**
- Create: `artifacts/nanotrack_four_dataset_finetune/validation/` generated artifacts
- Create: `artifacts/nanotrack_four_dataset_finetune/final/` generated artifacts
- Create: `artifacts/nanotrack_four_dataset_finetune/REPORT.md`

**Interfaces:**
- Consumes: Task 4 checkpoints and held-out UAV123/VisDrone-SOT splits.
- Produces: baseline comparison, best-checkpoint selection, final metrics, hashes, and reproducibility report.

- [ ] Evaluate the official baseline and every epoch on UAV123 validation.
- [ ] Select the best epoch without looking at test metrics.
- [ ] Evaluate baseline and selected checkpoint on UAV123 test and VisDrone-SOT validation.
- [ ] Run the complete verifier, inspect all promised artifacts, and write the report with limitations.

