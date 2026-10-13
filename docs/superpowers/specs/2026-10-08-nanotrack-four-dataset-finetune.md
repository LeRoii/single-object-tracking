# NanoTrack V3 Four-Dataset Fine-tuning Specification

## Objective

Fine-tune NanoTrack V3 once with the four locally available aerial tracking
datasets: UAV123, UAVDT, VisDrone-SOT, and VisDrone-MOT. Preserve source-level
train/validation/test separation and produce a checkpoint whose effect is
measured against the official NanoTrack V3 checkpoint.

## Fixed inputs

- UAV123: `/home/u20/dataset/Dataset_UAV123_10fps/UAV123_10fps`
- UAVDT: `/home/u20/dataset/UAVDT`
- VisDrone-SOT: `/home/u20/dataset/VisDrone2019-SOT`
- VisDrone-MOT: `/home/u20/dataset/VisDrone2019-MOT`
- NanoTrack source: `/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack`
- Initial checkpoint: NanoTrack V3 official checkpoint already used by the
  previous UAV123 experiment
- Reproducibility seed: `123456`
- Hardware constraint: one RTX 2060 with 6 GiB VRAM

## Dataset policy

- Reuse only the UAV123 training split from the existing source-folder grouped
  split. Keep its validation and test splits untouched for model selection and
  final evaluation.
- Use both official VisDrone-SOT train archives as native SOT training data.
  Keep the official validation and test-dev sequences out of training.
- Convert only the official VisDrone-MOT train sequences to SOT tracklets.
  Accept annotated target classes 1 through 10, reject ignore regions and
  invalid boxes, split tracks at long annotation gaps, and reject tracklets
  shorter than 30 valid frames.
- The downloaded UAVDT SOT image archive contains no SOT annotation files.
  Therefore use the official UAVDT MOT train list in `M_attr/train`, convert
  vehicle identities from `*_gt_whole.txt` into SOT tracklets, and keep the
  official test list out of training.
- Deterministically subsample at most 60 frames per converted MOT tracklet to
  bound preprocessing time and disk use while retaining every eligible track.
- Generate NanoTrack `crop511` images and PySOT-style JSON manifests. Never
  modify the source datasets.

## Sampling and training policy

- Per-epoch sample weights: UAV123 20%, VisDrone-SOT 30%, VisDrone-MOT 30%,
  UAVDT 20%.
- Use 20,000 sampled pairs per epoch for five epochs.
- Use batch size 8 for the 6 GiB GPU, keeping a smoke configuration with a
  smaller pair budget.
- Start from the official NanoTrack V3 checkpoint, train the head first, then
  allow the final backbone to update at a reduced learning-rate multiplier.
- Save every epoch and select the best epoch by the pre-existing held-out
  UAV123 validation metric. Evaluate that checkpoint on the untouched UAV123
  test split and VisDrone-SOT validation split.

## Required artifacts

- Source audit and deterministic conversion manifest for all four datasets.
- Prepared JSON annotations and crop summaries for the three newly converted
  datasets; the existing audited UAV123 preparation is referenced by hash.
- Smoke-training log and reload inference evidence.
- Five full-training checkpoints and per-step metrics.
- Baseline and fine-tuned evaluation summaries.
- A report containing exact commands, hashes, dataset coverage, metrics,
  limitations, and an explicit statement that this is not RK3566 board proof.

## Acceptance criteria

- All four named datasets contribute nonzero training samples.
- No official validation/test source video is present in a training manifest.
- Prepared samples from every source load through NanoTrack `BANDataset`.
- A real CUDA forward/backward smoke run completes within the 6 GiB limit and
  its checkpoint reloads for inference.
- The full run produces five checkpoints with finite losses.
- Baseline and selected-checkpoint evaluations are generated from the same
  evaluator and immutable held-out splits.
- The final report states whether the fine-tuned model improves or regresses;
  completion does not depend on improvement.

