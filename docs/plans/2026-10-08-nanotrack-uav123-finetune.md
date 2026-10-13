# NanoTrack V3 UAV123_10fps Fine-tuning Plan

## Goal

Use the local UAV123_10fps dataset to complete and verify the full NanoTrack V3
fine-tuning workflow, while keeping the data format extensible for additional
tracking datasets.

## Fixed inputs

- Dataset: `/home/u20/dataset/Dataset_UAV123_10fps/UAV123_10fps`
- Tracker source: `/home/u20/single-object-tracking/repos/siamtrackers/NanoTrack`
- Initial checkpoint: `models/pretrained/nanotrackv3.pth`
- Seed: `123456`
- Default batch size: `16`; smoke-test batch size: `8`

## Tasks

1. Implement and test UAV123 metadata parsing, validation, grouped split, and
   NanoTrack `crop511` conversion.
2. Generate immutable audit and split manifests. Split the 91 physical source
   videos 70/15/15, keeping all logical segments from one source together.
3. Implement and test split-aware OPE evaluation; benchmark the official V3
   checkpoint before training.
4. Add fine-tuning configurations and a single-GPU training wrapper that loads
   the full V3 checkpoint, freezes the unused neck, and supports head-only then
   low-LR backbone fine-tuning.
5. Run a 200-iteration smoke test, reload its checkpoint, and perform inference.
6. Run five epochs with 10,000 sampled pairs per epoch, validating every epoch
   and selecting the best checkpoint by vehicle/person validation AUC.
7. Evaluate the selected checkpoint on the held-out test split, run the local
   `sot-v1.mp4` regression, and verify ONNX export compatibility.
8. Produce a reproducibility report containing commands, resolved configs,
   hashes, metrics, logs, and limitations.

## Acceptance gates

- Dataset audit maps 123 logical sequences to 91 source folders and accounts
  for all 37,607 annotations, including 853 absent-target (`NaN`) frames.
- No source folder occurs in more than one split.
- Crop and JSON samples load through NanoTrack's `BANDataset`.
- CUDA tensor allocation and actual training forward/backward pass succeed.
- Smoke and full checkpoints reload for inference.
- Baseline, per-epoch validation, and final held-out test metrics exist.
- PyTorch and exported ONNX outputs are compared numerically.

## Review focus

- 10fps frame ranges must come from `configSeqs.m`, not the bundled 30fps JSON.
- NaN frames must be excluded from training metrics without resetting tracking.
- Split leakage between `_1`, `_2`, and `_3` logical segments is forbidden.
- Training success is not treated as proof of RK3566 deployment feasibility.
