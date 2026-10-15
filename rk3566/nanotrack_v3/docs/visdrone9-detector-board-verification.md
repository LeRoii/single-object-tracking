# VisDrone9 Detector RK3566 Board Verification

Verification date: 2026-10-15

Board release:
`/home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1`

This is an isolated detector-function release. It does not overwrite
`rk3566-yolov8-detector` or either detector-recapture release, and it does not
promote the VisDrone9 model to the canonical tracking configuration.

## Platform and artifact identities

- Board: LubanCat RK3566, AArch64, Linux 4.19.232.
- Board filesystem before deployment: 29 GiB total, 18 GiB available.
- RKNN runtime API: `2.3.2 (429f97ae6b@2025-04-09T09:09:27)`.
- RKNN driver: `0.8.2`.
- Reused smoke executable: AArch64 ELF, SHA-256
  `c83f9c4dc3a64cc6af9f569e0a57c7499d6b1b68a6bae2401cae9f1b9e59407f`.
- `ldd` resolved `libopencv_world.so.410` and the RKNN 2.3.2
  `librknnrt.so`; no dependency was missing.

The target directory was confirmed absent before creation. Only the existing
smoke executable was copied from the old release. The following new files
were transferred and then hashed again on the board:

| Artifact | Size (bytes) | Board SHA-256 |
| --- | ---: | --- |
| `models/yolov8n-visdrone9-rk3566-fp16.rknn` | 7797510 | `3698bc9bea1f43a4bac2826a09e03be0439fddfbc5c5604d8ed0f92b36881df0` |
| `models/detector-manifest.json` | 3890 | `05e6d8c7c210b7f9d85b8fb712aa00b014f310e46e85f171ce1158afc90ba63e` |
| `models/visdrone9.txt` | 64 | `41942118d0d70371171f9cf215f5047a6e1ba253f5ae2cd612864085d0617992` |

All three identities match the conversion host. The current Python verifier
also ran on the board and reported `target=rk3566 outputs=9 classes=9`.

## Actual RKNN tensor contract

The first real image load reported one input and nine outputs:

```text
input  0: 1x640x640x3 FP16 NHWC
output 0: 1x64x80x80 FP16 NCHW
output 1: 1x9x80x80 FP16 NCHW
output 2: 1x1x80x80 FP16 NCHW
output 3: 1x64x40x40 FP16 NCHW
output 4: 1x9x40x40 FP16 NCHW
output 5: 1x1x40x40 FP16 NCHW
output 6: 1x64x20x20 FP16 NCHW
output 7: 1x9x20x20 FP16 NCHW
output 8: 1x1x20x20 FP16 NCHW
```

This confirms the manifest's logical box/class/score-sum ordering and the
nine-class channel count on the actual RK3566 runtime. The RKNN model metadata
exposes an FP16 input tensor; the C++ engine supplies the manifest-declared RGB
UINT8 NHWC input and lets RKNN perform its configured 1/255 conversion.

The cold connection probe produced 16 candidates and reported 20.971 ms
preprocess, 222.924 ms inference, 52.953 ms postprocess, and 296.848 ms total.
Its log SHA-256 is
`53a401a4668c88a902ce38d94b82584a03472c9311338782db2ec232f143eb26`.

## Fixed 20-image functional batch

The first 20 paths from the already-hashed, seed-42 parity manifest were
copied to the isolated release. Board-side `sha256sum -c` passed for all 20;
the relative sample checksum file SHA-256 is
`590c7ecfad03f766a42a92242ac5523c3f9a8da5dd15b8b0fd24bf323a61d164`.

Each image ran in a fresh process with confidence 0.25, class-aware NMS 0.45,
and no class filter. Results:

| Gate | Result |
| --- | --- |
| Model loads | 20/20 pass |
| Non-empty annotated JPEG | 20/20 pass |
| Non-empty candidate CSV | 20/20 pass |
| RKNN/runtime error patterns | none |
| Candidate range per image | 5 to 61 |
| Candidate total | 528 |
| Empty candidate sets | 0 |
| Candidate class IDs | all IDs 0 through 8 observed |
| CSV values | finite confidence/box values; positive width and height |

Candidate distribution was: person 97, bicycle 7, car 299, van 21, truck
18, tricycle 20, awning-tricycle 11, bus 3, and motor 52. These counts prove
that all nine output channels are usable; they are not an accuracy metric and
were not matched against VisDrone ground truth on the board.

The complete batch log SHA-256 is
`f8e11bc40d0fe02885b1b2990ad80497eb4c631da5505fe0adcb426ce32bca9e`.

## Class-filter checks

Both filters used `0000330_00801_d_0000804.jpg`:

| Filter | Candidates | CSV invariant | Total time |
| --- | ---: | --- | ---: |
| `--class-id 0` (person) | 7 | every row has class ID 0 and label `person` | 206.293 ms |
| `--class-id 2` (car) | 43 | every row has class ID 2 and label `car` | 204.338 ms |

Person and car CSV SHA-256 values are respectively
`499051ec07bbe03ecc6c7dbfa97f1df87c9882495fc52da9e0da51f72e4eee06`
and `c8becb5bec6209e42cbb0e9665ddc5f63a3e4c219f6741312ea7e8ba71434ee0`.

## Functional latency and memory boundary

The smoke executable reports model preprocessing, RKNN inference, and CPU
postprocessing. It excludes process/model initialization and output image/CSV
writing. Therefore these numbers are function measurements, not sustained
video throughput or a product performance pass line.

| Phase | Min (ms) | Mean (ms) | P50 (ms) | P95 (ms) | Max (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Preprocess | 7.439 | 9.804 | 9.355 | 13.515 | 14.864 |
| RKNN inference | 158.296 | 162.881 | 162.787 | 165.368 | 168.307 |
| Postprocess | 23.931 | 28.797 | 27.623 | 36.554 | 38.595 |
| Total | 192.572 | 201.483 | 201.142 | 208.180 | 208.589 |

The board lacks GNU `/usr/bin/time`. During each independent process, the
test polled the kernel's cumulative `/proc/<pid>/status` `VmHWM` field every
20 ms. Observed VmHWM was 77,968 to 84,028 kB, with a median of 79,530 kB.
The resource log SHA-256 is
`d7a098073afc383a8dc1dc0d5d03fd79af6be1dd1a8ab1b389bbff215bdc52da`.
This bounds single-image smoke processes; it does not replace the integrated
long-duration memory protocol in Task 9.

## Acceptance result and remaining boundary

Independent detector bring-up is **PASS**: artifact identity, manifest,
model load, actual tensor layout, 20-image inference, output creation,
class filtering, and bounded single-process resources all passed on the real
RK3566 board.

This result does not yet prove long-term tracker recovery, bounded detector
queue behavior, integrated-video memory stability, or an improvement over the
existing COCO detector. The canonical manifest remains unchanged until those
Task 9 gates pass.
