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

Training has not started at this evidence point. CUDA framework validation,
the 5-epoch smoke run, the 100-epoch run, export, RKNN conversion, and board
verification will be appended only after their respective commands complete.
