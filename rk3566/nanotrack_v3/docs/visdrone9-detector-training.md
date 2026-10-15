# VisDrone9 YOLOv8n Detector Training Record

Date: 2026-10-15

This document records the reproducible data, environment, training, export,
and host-side conversion evidence for the RK3566 VisDrone9 detector. Board
evidence is recorded separately in `visdrone9-detector-board-verification.md`.

## Scope and evidence boundary

- Source dataset: `/home/u20/dataset/VisDrone2019-DET`
- Generated dataset: `artifacts/yolov8-visdrone9/dataset` (Git ignored)
- Classes: `person,bicycle,car,van,truck,tricycle,awning-tricycle,bus,motor`
- Source categories 1 and 2 are merged into `person=0`; categories 0 and 11
  are excluded.
- This section proves deterministic conversion and an independent label audit.
  It does not prove model training, RKNN conversion, board inference, or target
  reacquisition.

## Source inventory

The source root was read without modification. Its directory mtime remained
`1792041701` before and after conversion.

| Split | Images | Annotations | Image filename/size manifest SHA-256 | Annotation filename/size manifest SHA-256 |
| --- | ---: | ---: | --- | --- |
| train | 6471 | 6471 | `d10abb491dfa60ce8356c3d9180352e0d723874fb538c5992154af889335a794` | `9a28197a7e8a2b5f674cac560169dd4917dd09bf5a871e743117f2f2e7b644af` |
| val | 548 | 548 | `bd1d8764d2e143ae755c5c5240a333745974f3076fed93ffec398bc78ea5563e` | `f61daf5cfba9f02dcc7bbc003fedb03cbf9ee4e5490e63ef2ffa3552e50ab6b4` |

The complete source directory occupies 3.4 GiB. The manifests hash sorted
`filename size` rows rather than the image contents; the generated JSON and
all model artifacts use content SHA-256.

## Conversion execution

Command:

```bash
python3 rk3566/nanotrack_v3/tools/convert_visdrone9.py \
  --source-root /home/u20/dataset/VisDrone2019-DET \
  --output-root artifacts/yolov8-visdrone9/dataset \
  --report artifacts/yolov8-visdrone9/dataset-conversion.json \
  --image-mode symlink
```

The generated view contains 7019 relative image symlinks and 7019 label
files. Conversion report:

- File: `artifacts/yolov8-visdrone9/dataset-conversion.json`
- Size: 1477 bytes
- SHA-256: `c6c808f3e1c2b57eac55f316685994effc74aec587b766d661e843024e00eda3`
- Status: `pass`
- Converted instances: 381963
- Clipped boxes: 0
- Excluded category 0/11 records: 11755
- Invalid positive-category boxes: 1

The single invalid positive-category record is
`VisDrone2019-DET-train/annotations/9999985_00000_d_0000020.txt:12`, whose
height is zero. It is excluded as `non_positive_extent`.

One source file,
`VisDrone2019-DET-train/annotations/9999974_00000_d_0000053.txt`, contains 34
otherwise valid eight-field rows with a trailing comma. The parser accepts
exactly one trailing empty field; these 34 instances are retained. Multiple
extra fields remain invalid.

## Per-split and per-class result

| Split | Instances | person | bicycle | car | van | truck | tricycle | awning-tricycle | bus | motor |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| train | 343204 | 106396 | 10480 | 144866 | 24956 | 12875 | 4812 | 3246 | 5926 | 29647 |
| val | 38759 | 13969 | 1287 | 14064 | 1975 | 750 | 1045 | 532 | 251 | 4886 |
| total | 381963 | 120365 | 11767 | 158930 | 26931 | 13625 | 5857 | 3778 | 6177 | 34533 |

## Independent audit

Command:

```bash
python3 rk3566/nanotrack_v3/tools/audit_visdrone9.py \
  --dataset-root artifacts/yolov8-visdrone9/dataset \
  --report artifacts/yolov8-visdrone9/dataset-audit.json
```

- File: `artifacts/yolov8-visdrone9/dataset-audit.json`
- Size: 528 bytes
- SHA-256: `879d0d0bc7d890747c26ea1ec4bc1ae643e3e815570c448269d61f2d3692fc8e`
- Status: `pass`
- Images/labels/instances: 7019/7019/381963
- Errors: 0

The audit re-read the generated files independently. It verified that all
image links resolve, every image and label has a peer, classes are 0..8, all
coordinates are finite, and all normalized boxes remain inside the image.
The audited instance count exactly matches the converter report.

## Deterministic visual sample

Python `random.Random(42)` selected 20 train images and
`random.Random(43)` selected 20 val images from sorted file lists.

- Sample list SHA-256:
  `823b786337dad9922a7b0ca919be6c5137a8c5f4ef9146204149dcda0fe2fe28`
- Contact sheet SHA-256:
  `3611713882ad4e89bc2d8da4b60bc64bc44361bd3b52f038a7f529726fce2309`
- Preview directory: `artifacts/yolov8-visdrone9/dataset-preview` (Git ignored)

The 40-image review includes daylight, night, glare, rotated views, sparse
roads, dense intersections, parking areas, pedestrians, bicycles, cars,
trucks, buses, and tricycles. The rendered boxes align with visible targets;
the merged person boxes and vehicle class colors are spatially consistent.
Very small and heavily occluded objects remain present as required. No
systematic coordinate offset, axis swap, or class-index shift was observed.

Sample identities:

```text
train/9999985_00000_d_0000006.jpg
train/0000303_00801_d_0000187.jpg
train/0000134_01332_d_0000146.jpg
train/9999999_00049_d_0000011.jpg
train/9999950_00000_d_0000011.jpg
train/9999943_00000_d_0000076.jpg
train/9999942_00000_d_0000168.jpg
train/0000339_03725_d_0000180.jpg
train/9999998_00408_d_0000360.jpg
train/0000281_02201_d_0000638.jpg
train/9999994_00000_d_0000035.jpg
train/9999999_00031_d_0000004.jpg
train/9999974_00000_d_0000016.jpg
train/0000257_04349_d_0000067.jpg
train/9999982_00000_d_0000037.jpg
train/9999960_00000_d_0000005.jpg
train/0000143_00681_d_0000051.jpg
train/0000142_01658_d_0000031.jpg
train/0000263_04401_d_0000189.jpg
train/9999942_00000_d_0000131.jpg
val/0000069_00713_d_0000003.jpg
val/0000287_01401_d_0000766.jpg
val/0000242_01912_d_0000008.jpg
val/0000335_03921_d_0000063.jpg
val/0000300_03201_d_0000154.jpg
val/0000163_00359_d_0000001.jpg
val/0000335_00981_d_0000048.jpg
val/0000359_00589_d_0000702.jpg
val/0000023_01233_d_0000011.jpg
val/0000360_02353_d_0000725.jpg
val/0000330_03201_d_0000816.jpg
val/0000301_01001_d_0000161.jpg
val/0000330_04201_d_0000821.jpg
val/0000316_01201_d_0000525.jpg
val/0000249_02900_d_0000009.jpg
val/0000103_04122_d_0000033.jpg
val/0000193_02725_d_0000118.jpg
val/0000215_00000_d_0000256.jpg
val/0000308_04401_d_0000327.jpg
val/0000242_06452_d_0000018.jpg
```

## Training status

### Reproducible environment

The training environment is isolated at `.venvs/yolov8-visdrone` (Git
ignored). Its complete dependency lock is
`rk3566/nanotrack_v3/training/requirements-yolov8-visdrone.lock`.

| Component | Verified value |
| --- | --- |
| Python | 3.10.12 |
| Rockchip YOLOv8 repository | `https://github.com/airockchip/ultralytics_yolov8` |
| Rockchip commit | `4674fe6e003dfbc5f2250d3b39dd31faaf7a9877` |
| Ultralytics package | 8.2.82, editable from the pinned checkout |
| PyTorch / torchvision | 2.4.0+cu121 / 0.19.0+cu121 |
| CUDA runtime reported by PyTorch | 12.1 |
| cuDNN reported by PyTorch | 90100 (9.1.0) |
| GPU | NVIDIA GeForce RTX 2060, 6144 MiB |
| NVIDIA driver | 580.178.04 |

Validation did not stop at device enumeration. A tensor was allocated on
`cuda:0`, multiplied on the GPU, synchronized, and returned `2.0` from input
`1.0`. The fixed training entrypoint also rejects a missing CUDA device or a
failed CUDA tensor operation before constructing the model.

Initial weights:

- URL: `https://github.com/ultralytics/assets/releases/download/v8.2.0/yolov8n.pt`
- Local file: `artifacts/yolov8-visdrone9/source/yolov8n.pt` (Git ignored)
- Size: 6549796 bytes
- SHA-256: `f59b3d833e2ff32e194b5bb8e08d211dc7c5bdf144b90d2c8412c47ccfc83b36`
- Load check: Rockchip `YOLO` constructed a `DetectionModel` successfully.

The entrypoint is `rk3566/nanotrack_v3/tools/train_visdrone9.py`. Both modes
use image size 640, batch 8, seed 42, AMP, deterministic mode, CUDA device 0,
four workers, patience 20, checkpoint saving, and plots. Smoke mode runs 5
epochs; full mode runs at most 100 epochs. Before a fresh run it records an
identity containing the mode, repository commit, dataset audit hash, data YAML
hash, initial-weight hash, class list, and seed. Resume is refused if any
identity field differs.

### Five-epoch smoke training

The smoke run used the complete 6471-image training split and the untouched
548-image validation split. It is a pipeline check, not a release candidate.

```bash
env MPLCONFIGDIR=/tmp/mplcache-visdrone9 \
  YOLO_CONFIG_DIR=/tmp/yolo-config-visdrone9 \
  .venvs/yolov8-visdrone/bin/python \
  rk3566/nanotrack_v3/tools/train_visdrone9.py \
  --mode smoke \
  --repo .tools/ultralytics_yolov8 \
  --weights artifacts/yolov8-visdrone9/source/yolov8n.pt \
  --data rk3566/nanotrack_v3/training/visdrone9-data.yaml \
  --dataset-audit artifacts/yolov8-visdrone9/dataset-audit.json \
  --project artifacts/yolov8-visdrone9/runs
```

- Run directory: `artifacts/yolov8-visdrone9/runs/visdrone9-smoke`
- Wall-clock evidence: 2026-10-15 14:43:40 to 14:53:15 +08:00
- Trainer-reported duration: 0.133 hours
- Device: NVIDIA GeForce RTX 2060; AMP enabled
- Highest `GPU_mem` value observed in the trainer progress: 4.93 GiB
- Transferred pretrained parameters: 319/355
- Data scan: 6471 train images and 548 val images, zero corrupt images
- Run identity SHA-256:
  `6e61bc757f3e8708200a264bf59359fa7975a842e3133b8ac6ea1ad709f7df00`

Epoch metrics from `results.csv`:

| Epoch | train box | train cls | train DFL | Precision | Recall | mAP50 | mAP50-95 |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1.8356 | 2.1855 | 1.0373 | 0.32094 | 0.17246 | 0.14486 | 0.07950 |
| 2 | 1.6896 | 1.5220 | 0.99523 | 0.27836 | 0.19256 | 0.17501 | 0.09786 |
| 3 | 1.6322 | 1.4037 | 0.97903 | 0.29424 | 0.22786 | 0.20640 | 0.11815 |
| 4 | 1.6002 | 1.3396 | 0.97007 | 0.31583 | 0.23900 | 0.22542 | 0.13017 |
| 5 | 1.5752 | 1.2779 | 0.96255 | 0.33684 | 0.24893 | 0.23975 | 0.13879 |

Final best-checkpoint validation by class:

| Class | Precision | Recall | AP50 | AP50-95 |
| --- | ---: | ---: | ---: | ---: |
| person | 0.393 | 0.325 | 0.297 | 0.109 |
| bicycle | 0.0745 | 0.00855 | 0.0242 | 0.0096 |
| car | 0.464 | 0.710 | 0.657 | 0.427 |
| van | 0.365 | 0.240 | 0.241 | 0.156 |
| truck | 0.379 | 0.185 | 0.199 | 0.133 |
| tricycle | 0.315 | 0.146 | 0.125 | 0.0671 |
| awning-tricycle | 0.250 | 0.062 | 0.0722 | 0.0451 |
| bus | 0.449 | 0.315 | 0.327 | 0.226 |
| motor | 0.339 | 0.248 | 0.216 | 0.0768 |

Artifact identities:

| Artifact | SHA-256 |
| --- | --- |
| `weights/best.pt` | `87e615378cd45ce8f015255811d8d93b5a690ee3b3af0c77670d23ef673512e3` |
| `weights/last.pt` | `8bfc0f2ab461280181ca1340cc38a4059823967af89fdecffd16611d2809802d` |
| `results.csv` | `47187083ca1e0cf08ff2d1b4c91b7f3ac4811fc15f14d24d550e78b8f86f60f1` |
| `args.yaml` | `fddded5dba60908518e40150cb83bcdf34008e6e5eff376f28a08dbd335feff9` |
| `confusion_matrix.png` | `4b9ff0e4643ac4427aa67635b0f30f455356b5b12e8393b8dc5d1df46b569d27` |
| `PR_curve.png` | `ae8a0645c4f24cec77e34e607ac21efb871cd8bc83e0231254e3f4d3ac768a22` |

The run identity accepted `last.pt`, and both checkpoints loaded with the
exact nine-class name table. A fixed inference on
`val/0000069_00713_d_0000003.jpg` produced 50 finite detections (classes 0
and 8), with confidence range 0.252935..0.947168. No NaN, Inf, corrupt image,
out-of-memory event, or training failure occurred. PyTorch emitted only a
future-deprecation warning for the legacy AMP scaler used by the pinned
Rockchip fork.

### Full 100-epoch training

The release-candidate training started from the original pretrained
`yolov8n.pt`, not from the smoke checkpoint. It used the same data,
environment, and fixed hyperparameters described above, with `--mode full`:

```bash
env MPLCONFIGDIR=/tmp/mplcache-visdrone9 \
  YOLO_CONFIG_DIR=/tmp/yolo-config-visdrone9 \
  .venvs/yolov8-visdrone/bin/python \
  rk3566/nanotrack_v3/tools/train_visdrone9.py \
  --mode full \
  --repo .tools/ultralytics_yolov8 \
  --weights artifacts/yolov8-visdrone9/source/yolov8n.pt \
  --data rk3566/nanotrack_v3/training/visdrone9-data.yaml \
  --dataset-audit artifacts/yolov8-visdrone9/dataset-audit.json \
  --project artifacts/yolov8-visdrone9/runs
```

- Run directory: `artifacts/yolov8-visdrone9/runs/visdrone9-full`
- Wall-clock evidence: 2026-10-15 14:55:14 to 17:07:56 +08:00
  (approximately 2 hours 13 minutes)
- Completed epochs: 100/100; early stopping did not trigger. The best epoch
  was 89, leaving only 11 non-improving epochs, below patience 20.
- Highest trainer `GPU_mem` value directly observed during monitoring:
  5.30 GiB. The terminal session was later detached, so this is an observed
  value rather than a proven whole-run maximum.
- Run identity SHA-256:
  `4c022374d5fde26f095b488496ef990370407b9ab3f37a21114e19555ab284f0`

Ultralytics removed four exact duplicate label rows while constructing the
training cache, one from each of the following images:

```text
train/0000137_02220_d_0000163.jpg
train/0000140_00118_d_0000002.jpg
train/9999945_00000_d_0000114.jpg
train/9999987_00000_d_0000049.jpg
```

The converter and independent audit correctly count all 381963 valid source
rows; the training loss therefore consumed four fewer rows after duplicate
suppression. This does not alter the source conversion audit, but it prevents
interpreting that count as the number of unique training objects.

Selected metrics from `results.csv`:

| Epoch | train box | train cls | train DFL | Precision | Recall | mAP50 | mAP50-95 | val box | val cls | val DFL |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 89 (best) | 1.3284 | 0.91438 | 0.90555 | 0.45610 | 0.34616 | 0.34867 | 0.20644 | 1.3218 | 0.92294 | 0.90655 |
| 100 (last) | 1.2850 | 0.85739 | 0.89782 | 0.45508 | 0.34241 | 0.34672 | 0.20501 | 1.3270 | 0.92434 | 0.90782 |

Artifact identities:

| Artifact | Size (bytes) | SHA-256 |
| --- | ---: | --- |
| `runs/visdrone9-full/weights/best.pt` | 6235363 | `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d` |
| `runs/visdrone9-full/weights/last.pt` | 6235363 | `014d577db2acb1aefa5ea68d20c0d969f0529f2892fb2be12b4b5fa86008c559` |
| `runs/visdrone9-full/results.csv` | - | `b2b3798d8bc66f873fe9b46cff35e1c466255d13030f3a2904161564977733f6` |
| `runs/visdrone9-full/args.yaml` | - | `78bcce30fa02b6b54cceabffaa85c7702f50f18712a55a915d7a74e8aa5648c6` |
| `export/yolov8n-visdrone9-best.pt` | 6235363 | `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d` |

The exported PT is a byte-identical copy of `best.pt`. All listed model and
run artifacts are Git ignored.

#### Independent best-checkpoint validation

The exported best checkpoint was loaded afresh and evaluated on all 548 val
images with batch 8. This independent pass reported overall precision
0.45418, recall 0.34786, mAP50 0.34907, and mAP50-95 0.20716. Its small
difference from the epoch-89 training row is retained rather than hidden;
release decisions use this independent result.

| Class | Precision | Recall | AP50 | AP50-95 |
| --- | ---: | ---: | ---: | ---: |
| person | 0.50770 | 0.37934 | 0.40287 | 0.15937 |
| bicycle | 0.24293 | 0.10023 | 0.07922 | 0.03147 |
| car | 0.65119 | 0.74566 | 0.75608 | 0.51914 |
| van | 0.51130 | 0.37620 | 0.38726 | 0.26576 |
| truck | 0.46355 | 0.31333 | 0.31353 | 0.20913 |
| tricycle | 0.40677 | 0.25837 | 0.23856 | 0.12605 |
| awning-tricycle | 0.26536 | 0.15414 | 0.12232 | 0.07621 |
| bus | 0.54945 | 0.44223 | 0.47479 | 0.32737 |
| motor | 0.48934 | 0.36124 | 0.36699 | 0.14994 |

The car class is materially stronger than the other target classes. Bicycle,
awning-tricycle, and small-person performance remain weak, so downstream
long-term tracking must not treat a missing detector candidate as proof that
the tracked object has left the scene.

#### Qualitative failure review

Four deterministic val images were selected directly from their annotation
statistics and inferred at confidence 0.25. Rendered images are under
`artifacts/yolov8-visdrone9/failure-review/best` and are Git ignored.

| Stress case | Image stem | Valid GT rows | Predictions | Observation |
| --- | --- | ---: | ---: | --- |
| Dense | `0000295_02400_d_0000033` | 317 | 184 | Central and near-field cars/persons are found, while the most crowded and distant region has substantial misses and overlapping display labels. |
| All small | `0000244_06388_d_0000014` | 46 | 23 | All 46 GT boxes are below 32x32 pixels; roughly half as many predictions remain, mostly cars and some at low confidence. |
| Heavy occlusion | `0000154_00001_d_0000001` | 51 | 30 | Vehicles and pedestrians partially hidden by trees are frequently absent, although unobstructed road users are localized plausibly. |
| Truncation | `0000289_00201_d_0000812` | 43 | 37 | Coverage is better, but clipped edge targets and tiny instances are still missed in the bright scene. |

Prediction count minus GT count is not an evaluated false-positive or
false-negative count: class-aware IoU matching was not run for this visual
review. The count gap is only a qualitative warning. Quantitative validation
remains the complete validation-set metrics above.

## Rockchip-optimized ONNX export

The byte-identified best checkpoint was exported through the official
`YOLO.export(format="rknn")` path in the pinned Rockchip fork. This invokes
that fork's `Exporter.export_rknn`, which removes DFL decoding from the graph
and emits box, sigmoid class, and score-sum tensors for CPU postprocessing.

```bash
env PYTHONPATH=/home/u20/single-object-tracking/.tools/ultralytics_yolov8 \
  .venvs/yolov8-visdrone/bin/python -c \
  'from ultralytics import YOLO; YOLO("artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt").export(format="rknn", imgsz=640, batch=1, dynamic=False, device="cpu", opset=12)'
```

Export identity and contract:

| Field | Value |
| --- | --- |
| Rockchip repository commit | `4674fe6e003dfbc5f2250d3b39dd31faaf7a9877` |
| Source PT SHA-256 | `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d` |
| ONNX file | `artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rkopt.onnx` |
| ONNX size | 12072229 bytes |
| ONNX SHA-256 | `b2948072f01da7fe3b686392a47fe742380c98775c4149111527b7ddf26a5cc8` |
| ONNX opset | 12 |
| Input | float32 NCHW `1x3x640x640`, name `images` |
| Outputs | 9 float32 NCHW tensors |
| Contract report SHA-256 | `a878698354b8f7457b519430786709d8043d64422d79b7c6d927ef642e739f28` |

Output order is fixed and was verified both statically and with a finite
ONNX Runtime zero-input execution:

```text
box_s8 [1,64,80,80], class_s8 [1,9,80,80], score_sum_s8 [1,1,80,80]
box_s16 [1,64,40,40], class_s16 [1,9,40,40], score_sum_s16 [1,1,40,40]
box_s32 [1,64,20,20], class_s32 [1,9,20,20], score_sum_s32 [1,1,20,20]
```

The verification tool rejects missing or reordered outputs, dynamic batch,
wrong class count, wrong grids, and non-finite model initializers or runtime
outputs before the model may enter RKNN conversion.

### PT/ONNX numerical parity

Fifty unique validation images were sampled from the sorted val file list by
`random.Random(42).sample(..., 50)`. The manifest was hashed before inference:

- Sample manifest SHA-256:
  `329373dcf809e3fbd462ea7d6276ac05ba58f3871886c7f9f82484c932e5a47e`
- Parity report SHA-256:
  `a5ab965772a6077ad6a9b92f3ea90623253ae643b979e4af2fb82d1dc1a33096`
- Runtime: ONNX Runtime `CPUExecutionProvider`; PyTorch CPU, fused evaluation
  model with the same Rockchip RKOPT head path.

Both paths consumed the same RGB float32 640x640 centered-letterbox tensor
(pad 114). Both used confidence 0.25 and class-aware NMS 0.45. The gate uses
only PT reference detections with confidence at least 0.35 and greedily
requires the same class and IoU at least 0.90.

| Metric | Required | Observed | Result |
| --- | ---: | ---: | --- |
| PT reference detections | - | 1643 | information |
| ONNX candidate detections | - | 2155 | information |
| Matched detections | - | 1643 | information |
| Matched recall | >=0.98 | 1.000000 | pass |
| Matched median IoU | >=0.99 | 0.9999991 | pass |
| Confidence absolute delta p95 | <=0.02 | 0.00000113 | pass |

All three parity gates pass. The higher ONNX candidate total is expected
because it includes detections down to 0.25 while the PT gate denominator is
filtered at 0.35; it is not evidence of detector-quality improvement.

## RK3566 FP16 RKNN conversion

Conversion used the isolated `.venvs/rknn-convert` environment with pinned
RKNN-Toolkit2 2.3.2 and target platform `rk3566`. No quantization or
calibration dataset was enabled.

```bash
.venvs/rknn-convert/bin/python \
  rk3566/nanotrack_v3/tools/convert_yolov8_rk3566.py \
  --onnx artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rkopt.onnx \
  --output artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-rk3566-fp16.rknn \
  --manifest artifacts/yolov8-visdrone9/export/detector-manifest.json \
  --labels artifacts/yolov8-visdrone9/export/visdrone9.txt \
  --source-url local://yolov8n-visdrone9-rkopt.onnx \
  --source-repository https://github.com/airockchip/ultralytics_yolov8 \
  --source-commit 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877 \
  --output-count 9 \
  --training-checkpoint artifacts/yolov8-visdrone9/export/yolov8n-visdrone9-best.pt \
  --training-repository https://github.com/airockchip/ultralytics_yolov8 \
  --training-commit 4674fe6e003dfbc5f2250d3b39dd31faaf7a9877 \
  --training-config artifacts/yolov8-visdrone9/runs/visdrone9-full/args.yaml \
  --dataset-audit artifacts/yolov8-visdrone9/dataset-audit.json
```

| Artifact | Size (bytes) | SHA-256 |
| --- | ---: | --- |
| RKNN | 7797510 | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| Detector manifest | 3890 | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| `visdrone9.txt` | 64 | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |

The schema remains version 1 and the existing C++ loader ignores the new
optional `training` object. Old manifests without it remain valid. When the
object is present, the verifier requires all four provenance groups:
checkpoint, Rockchip repository/commit, expanded `args.yaml`, and dataset
audit. The generated manifest records these identities:

| Provenance item | SHA-256 |
| --- | --- |
| Training checkpoint | `6744672ac5e35debba2a647ad570e9fe84a1cabd1c45c9840080d0a4127fe36d` |
| RKOPT ONNX | `b2948072f01da7fe3b686392a47fe742380c98775c4149111527b7ddf26a5cc8` |
| Expanded training arguments | `78bcce30fa02b6b54cceabffaa85c7702f50f18712a55a915d7a74e8aa5648c6` |
| Dataset audit | `879d0d0bc7d890747c26ea1ec4bc1ae643e3e815570c448269d61f2d3692fc8e` |

Manifest verification passes with `class_count=9`, nine ordered outputs,
`quantized=false`, `calibrated=false`, and
`deployment_status=function_only_board_unverified`. Toolkit2 reported FP16
defaults for internal tensors and two empty ONNX Resize optional constants;
the build and export still completed successfully. These host-side facts do
not prove that the board runtime exposes the declared logical NCHW tensors;
that is a Task 8 acceptance condition.

### Pending stages

Board verification will be appended only after its commands complete.
