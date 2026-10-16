# VisDrone9 Detector RK3566 Board Verification

Verification date: 2026-10-15

Board release:
`/home/cat/Project/SOT/nanotrack_v3_longterm/releases/yolov8n-visdrone9-rk3566-fp16-v1`

This is an isolated detector-function candidate release. It does not overwrite
`rk3566-yolov8-detector` or either detector-recapture release. The Task 9
functional and control-safety checks documented below passed, but the memory
promotion gate did not; the repository's canonical tracking configuration
therefore remains unchanged.

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

This detector-only result did not by itself prove long-term tracker recovery,
bounded detector queue behavior, integrated-video memory stability, or an
improvement over the existing COCO detector. Those separate integration gates
are evaluated below.

## Long-term controller integration

The integration runs reused the schema-v5 long-term controller executable from
the accepted detector-recapture release. Its board SHA-256 was
`0ff789b900a8c227596df562e3fc91c798d655750c5a7fab76d6f258f84d68ad`;
the controller source identity was
`1552b7d14f67006768757dff039327253e3a60cb`. NanoTrack model, controller
configuration, input-video, executable, detector, labels, and manifest hashes
were retained with the board artifacts.

All retained runs passed `scripts/verify_board.sh`, which checks output frame
count, schema-v5 metadata, sequential timestamps, finite source-coordinate
boxes, model/label/manifest hashes, detector tensor descriptions, detector
enabled/disabled state, and RKNN/runtime error patterns.

| Run | Output/tracked frames | Request/complete/reject/error | Detector hypothesis rows | Reacquired | LOST with valid control |
| --- | ---: | ---: | ---: | ---: | ---: |
| person, class 0 | 987/986 | 58/58/37/0 | 31 | 0 | 0 |
| car, class 2 | 702/701 | 32/32/0/0 | 96 | 4 motion-based | 0 |
| person, detector disabled | 987/986 | 0/0/0/0 | 0 | 0 | 0 |
| person stability | 1002/1001 | 59/59/37/0 | 32 | 0 | 0 |

`hypothesis_source=DETECTOR` is the criterion for the fourth column. It proves
that accepted detector candidates entered NanoTrack verification; it does not
mean that target identity was confirmed. The four car reacquisitions occurred
at frames 162, 173, 177, and 180, and every one has
`hypothesis_source=MOTION`. They therefore cannot be credited to the detector.
No detector candidate bypassed verification to authorize control.

Person and car used the same input videos, initial source-coordinate boxes,
visible controller configuration, confidence 0.25, and NMS 0.45 as the COCO
baseline. The car input is 3840x2160; its retained preview was explicitly
encoded at 960x540 while its CSV coordinates remained at source resolution.

### Queue, safety, and disabled-path gates

- Final metadata request/complete gaps were zero for every retained run.
- The maximum sampled gap was one in the car run and zero in the other runs.
- No detector or RKNN error was reported.
- Every `LOST` row had `control_valid=false`.
- The disabled run reported `detector.enabled=false`, worker state `disabled`,
  no configured detector rows, and zero request, completion, candidate, reject,
  and error counts.
- Enabled runs reached nine threads after startup and did not create additional
  threads over time; the disabled run remained at five.

### Integrated timing and sampled resources

The runtime boundary excludes decode, overlay, and encoding. The end-to-end
boundary includes the entire per-frame video path. No performance pass line was
defined for this function-first FP16 model.

| Run | Runtime mean / FPS | End-to-end mean / FPS | Warm RSS first to last | Last VmHWM |
| --- | --- | --- | ---: | ---: |
| person | 81.888 ms / 12.212 | 165.238 ms / 6.052 | 112920 to 113636 kB | 160360 kB |
| car | 83.177 ms / 12.023 | 283.397 ms / 3.529 | 252260 to 286384 kB | 338652 kB |
| detector disabled | 74.562 ms / 13.412 | 158.432 ms / 6.312 | 68604 to 68616 kB | 118772 kB |
| 1001-frame stability | 81.377 ms / 12.288 | 165.187 ms / 6.054 | 113116 to 113832 kB | 160556 kB |

During the 1001-tracked-frame run, 30-second samples after startup held RSS
between 113116 and 113832 kB, all detector requests completed, and thread count
remained nine. However, VmHWM increased at every sample: 120768, 128868,
138108, 148672, and 160556 kB. The approved Task 9 gate treats continuing
monotonic growth in either RSS or VmHWM as a promotion failure. Current RSS,
queue, and thread behavior are bounded in this window, but peak-memory behavior
is **RISK/FAIL for promotion** and remains consistent with the earlier
1267-second writer/buffer concern.

### COCO comparison on the same person/car inputs

| Input | Model | Request/complete/error | Accepted batches | Detector hypothesis rows | Confirmed reacquisition | Runtime / end-to-end FPS |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| person | COCO80 | 58/58/0 | 58 | 113 | 0 | 13.986 / 6.539 |
| person | VisDrone9 | 58/58/0 | 21 | 31 | 0 | 12.212 / 6.052 |
| car | COCO80 | 32/32/0 | 32 | 96 | 0 detector-confirmed | 11.356 / 3.476 |
| car | VisDrone9 | 32/32/0 | 32 | 96 | 0 detector-confirmed | 12.023 / 3.529 |

The new person model produced fewer usable hypotheses at the fixed 0.25
threshold than the COCO baseline. This is not an accuracy improvement claim;
it is direct evidence that threshold/domain calibration remains necessary. The
car behavior is effectively unchanged over the common 701-frame prefix. The
VisDrone9 run contains one additional final LOST row because it processed one
additional tracked frame.

The earlier short COCO person/car protocol did not retain periodic RSS, VmHWM,
thread, or queue resource logs, so a same-input resource comparison cannot be
made from existing artifacts. The separate 4200-frame COCO stability run
recorded 9 threads, maximum queue gap 1, warm RSS 147152 to 149328 kB, maximum
RSS 446320 kB, and VmHWM 476684 kB, but it is not the same input or duration.
This protocol mismatch is an additional reason not to mark the full Task 9
comparison/promotion stage complete.

### Artifact evidence and protocol deviation

Retained artifact SHA-256 values:

| Run | CSV | Metadata | Resource log | Run log |
| --- | --- | --- | --- | --- |
| person | `f17a5a74ffe79d0cb02c1ababef3764b876d6294c3e045df319968213a642383` | `f646eeb4536d3b1bc6c287689b9a11975f245c691ec1d0dafc76a15bc39e181b` | `9c1b8e3d25b8aaa0458b54de33f6f0bbabe4f0733051edcfd6f9db73c626a0e0` | `7c37f56ffccfeaa6a1418f8cff62942790e32961b80df5a8c0bad14296ba04a7` |
| car | `703dbd21ef634862488a42057625d31368c1a51930fc32432c3cf8d4c5fc0446` | `023bbaec437dd3d23776568638b17fe4bcb7e44ddbd5ee62bef105b583e7f3cc` | `43093c7fef4d364ddf3a77679903ed4d34558a9d74a6ac01926d6360629d582a` | `eb12acfc9447e6407ddc50d84029b42cb58470f5262c1be728019e45fb2f4870` |
| disabled | `447e4e528d7caa21999e947d365d32d2e63e42807cffffcccbbee6efcde71bd0` | `3ee6aef3a035f9e5f2e9650bd0421d4f4cadc712965d560608e19bc2f0524a47` | `f246dffb3adca659a1bdaa8f0fca393a93dfe05311492cac6fec0251883c23cf` | `6055c4743632ad747a3d35b995f72a7276c730f36aec4679f55efed16d0bde9b` |
| stability | `e01619dd0037a56339d801a54d0df516ff59adfac4b9e3851c7a18bfa4546c65` | `c19442f840cdaa10b8c43df713f5ccd9a109ebea8ab5b42434170cd13ea73eb0` | `79bc6cf3a950e515d5ed08fae08b0cdf9ecfde99e39d1ee1e9778c68d79fb4bd` | `d6f45087f0335ea955f031559b07ec5dd378a0f76cddebc2be8fa97cbd0bad7f` |

An initial car command omitted the required 960x540 preview size and began
encoding the 3840x2160 source. It was terminated after 250 CSV rows and retained
as `car-visdrone9-700`; it is not acceptance evidence. The complete corrected
run is `car-visdrone9-700-v2` and is the only car run reported above.

## Promotion decision and remaining limits

Artifact identity, tensor contract, real RK3566 inference, finite outputs,
candidate-to-NanoTrack routing, queue completion, and the control safety
boundary all passed. Formal promotion is nevertheless **held** because VmHWM
grew monotonically throughout the required stability sample and the old/new
same-input resource protocols are not directly comparable. The canonical
repository manifest and README remain on COCO; neither old board releases nor
the isolated VisDrone9 candidate release was modified or removed.

Promotion requires a corrected or isolated writer/buffer path followed by a
longer run in which RSS and VmHWM demonstrably plateau, plus a same-protocol
COCO/VisDrone9 resource comparison. Product acceptance also still requires
visible/infrared threshold calibration, identity-aware handling of same-class
ambiguity, and annotated occlusion/reappearance evaluation. The person sample
had no confirmed recovery, and the four car reacquisitions were motion-based.
