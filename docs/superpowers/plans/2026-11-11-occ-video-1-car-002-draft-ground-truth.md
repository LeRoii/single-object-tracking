# occ_video_1 car_002 Draft Ground Truth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate a fully indexed 271-frame, strictly layered draft annotation package for the second car selected in `occ_video_1`, with uncertain frames isolated for user review.

**Architecture:** A host-only C++ OpenCV tool produces forward/backward tracking candidates and visual review material from manually chosen anchors. A Python standard-library packager keeps candidate, confirmed, and non-observable estimated boxes in separate schemas and YOLO directories, validates all 271 frame mappings, and hashes the deliverable. The generated draft stays outside Git; Git records only tools, tests, design/provenance, hashes, and reproduction commands.

**Tech Stack:** C++17, OpenCV 4 tracking/videoio/imgproc/imgcodecs, Python 3 standard library, CMake/CTest, unittest, ffprobe/sha256sum.

**Spec:** `docs/superpowers/specs/2026-11-11-occ-video-1-car-002-draft-ground-truth-design.md`

## Global Constraints

- Input is `/home/u20/single-object-tracking/data/occ_video_1/1.mp4`, SHA-256 `eb319be66c7b5ae32a02025954ecc6a0f99fb860108f8d2ad4eda509f4c42c7d`, 1920x1080, 30 FPS, frames 0--270.
- Initial candidate anchor is frame 0 bbox `(967, 513, 42, 42)`; episode ID is `occ_video_1_car_002_full_object`, target ID `car_002`, class ID `0`, scope `full_object`.
- Algorithm output is preannotation evidence only. `target_bbox_xywh` and `labels/` remain empty until explicit user confirmation; candidates use `candidate_bbox_xywh` and `candidate_labels/`; non-observable estimates use `estimated_bbox_xywh` and `estimated_labels/`.
- Every YOLO layer contains exactly `frame_000000.txt` through `frame_000270.txt`; empty files preserve mapping without claiming a box.
- `target_state` and `review_status` are independent. Formal evaluators must reject a draft with unresolved `assistant_visual_candidate` or `needs_human_review` rows.
- Do not modify existing `car_001` labels, `docs/progress/STATUS.md`, or `docs/progress/integration-contract.yaml`.
- Generated media and 271-frame artifacts remain under `artifacts/ground_truth/occ_video_1_car_002_draft/` outside Git. Commit only reproducible tools/tests and owned `docs/progress/board-validation.md` provenance.

## Review Focus

- Tracker failure or forward/backward disagreement must produce `needs_human_review`, never a silently averaged candidate; pin in Task 1 tests.
- Segment endpoints and frame 0/270 must be emitted once with no gaps or duplicate anchors; pin in Task 1 tests.
- Candidate, confirmed, and estimated boxes must never cross schema or YOLO directory boundaries; pin in Task 2 tests.
- Full-occlusion/ambiguous rows must have `target_bbox_xywh=null` and an empty formal label even when an estimate exists; pin in Task 2 tests.
- Same-class distractor IDs and review-state enums must be stable and valid across all referenced frames; pin in Task 2 tests.

---

### Task 1: Bidirectional OpenCV Candidate Generator

**Files:**
- Create: `rk3566/nanotrack_v3/include/bidirectional_annotation.hpp`
- Create: `rk3566/nanotrack_v3/src/bidirectional_annotation.cpp`
- Create: `rk3566/nanotrack_v3/tools/generate_bidirectional_candidates.cpp`
- Create: `rk3566/nanotrack_v3/tests/test_bidirectional_annotation.cpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`

**Interfaces:**
- Consumes: decoded BGR frames for one bounded segment, left/right `AnnotationAnchor{frame_id, BBox}`, and `AgreementThresholds{iou_min=0.50, center_max=0.35}`.
- Produces: pure `BidirectionalCandidate merge_bidirectional_observations(...)` plus `std::vector<BidirectionalCandidate> track_segment_bidirectional(...)`; each result contains frame ID, forward/reverse success and boxes, consensus candidate, IoU, normalized center distance, and status `anchor|bidirectional_candidate|needs_human_review`.
- CLI: `generate_bidirectional_candidates --video FILE --anchors CSV --output CSV --review-dir DIR --agreement-iou 0.50 --agreement-center 0.35`.

- [ ] **Step 1: Write the failing C++ test**

Test the pure merge function with fixed boxes so agreement, disagreement, and tracker-failure behavior is deterministic. Add one short synthetic moving-rectangle integration case for ordered CSRT output. Assert adjacent segments emit a shared anchor only once and output spans exact first/last frame IDs.

- [ ] **Step 2: Configure and run the test to verify RED**

Run: `cmake -S rk3566/nanotrack_v3 -B /tmp/nanotrack-car002-tools -DBUILD_ANNOTATION_TOOLS=ON -DENABLE_RKNN_RUNTIME=OFF && cmake --build /tmp/nanotrack-car002-tools --target test_bidirectional_annotation -j4 && ctest --test-dir /tmp/nanotrack-car002-tools -R '^bidirectional_annotation$' --output-on-failure`

Expected: build/test fails because the interface is not implemented.

- [ ] **Step 3: Implement the minimal library and CLI**

Use `cv::TrackerCSRT` independently in each direction, while keeping merge/gating logic pure and separately testable. Decode only one anchor-bounded segment at a time to bound memory. Consensus is the coordinate-wise mean only when both trackers succeed and both agreement gates pass; otherwise preserve both boxes and mark review. The CLI validates strictly increasing anchors, in-range positive boxes, exact decode geometry/count, writes deterministic CSV ordering, an annotated review video, and contact sheets containing every anchor plus all review frames. `BUILD_ANNOTATION_TOOLS=ON` finds only host OpenCV `core,imgproc,imgcodecs,videoio,tracking` and does not enable or link RKNN.

- [ ] **Step 4: Run focused GREEN and CLI validation**

Run the Task 1 CMake/build/CTest command, then invoke `--help` and a synthetic-video CLI smoke. Expected: CTest PASS; CLI output has continuous frame IDs and review media decodes.

- [ ] **Step 5: Commit Task 1**

```bash
git add rk3566/nanotrack_v3/CMakeLists.txt \
  rk3566/nanotrack_v3/include/bidirectional_annotation.hpp \
  rk3566/nanotrack_v3/src/bidirectional_annotation.cpp \
  rk3566/nanotrack_v3/tools/generate_bidirectional_candidates.cpp \
  rk3566/nanotrack_v3/tests/test_bidirectional_annotation.cpp
git commit -m "feat: add bidirectional annotation candidate tool"
```

### Task 2: Strict Draft Packager and Validator

**Files:**
- Create: `rk3566/nanotrack_v3/tools/build_occ_car002_draft.py`
- Create: `rk3566/nanotrack_v3/tests/test_build_occ_car002_draft.py`

**Interfaces:**
- Consumes: candidate CSV from Task 1; a review-policy CSV with `frame_id,target_state,review_status,visibility_ratio,identity_status,annotation_quality`; events JSON; stable distractor JSON; exact video identity.
- Produces: the design-specified directory with `frames.jsonl`, metadata/status/events/review manifests, three 271-file YOLO layers, per-layer manifests, and `checksums.sha256`.
- Commands: `build_occ_car002_draft.py build ...` and `build_occ_car002_draft.py validate --root DIR --require-confirmed false|true`.

- [ ] **Step 1: Write failing Python tests**

Cover a five-frame fixture with visible candidate, manual confirmation, full occlusion estimate, ambiguous identity, and stable same-class distractor. Assert exact layer separation, 271-mode filename continuity via a generated 271-row fixture, null formal bbox on non-observable frames, empty formal labels for unconfirmed frames, invalid enum/unstable distractor rejection, manifest verification, and `--require-confirmed true` rejection while review items remain.

- [ ] **Step 2: Run tests to verify RED**

Run: `python3 -m unittest -v rk3566.nanotrack_v3.tests.test_build_occ_car002_draft`

Expected: FAIL because the packager module is absent.

- [ ] **Step 3: Implement the packager and validator**

Use only Python standard library modules. Define explicit enums and schema version `1.0-draft`; normalize pixel xywh against 1920x1080 with six decimals; write empty files where a layer has no box; hash files in lexical relative-path order. Refuse overwriting a nonempty output unless `--replace-draft` is supplied and the existing metadata identifies the same video/episode draft.

- [ ] **Step 4: Run focused and repository regression tests**

Run: `python3 -m unittest -v rk3566.nanotrack_v3.tests.test_build_occ_car002_draft`

Run: `ctest --test-dir /tmp/nanotrack-occ-c010-host --output-on-failure`

Expected: Python tests PASS; existing CTest remains 19/19 PASS.

- [ ] **Step 5: Commit Task 2**

```bash
git add rk3566/nanotrack_v3/tools/build_occ_car002_draft.py \
  rk3566/nanotrack_v3/tests/test_build_occ_car002_draft.py
git commit -m "feat: package layered tracking annotation drafts"
```

### Task 3: Generate and Review the 271-Frame car_002 Draft

**Files:**
- Create outside Git: `artifacts/ground_truth/occ_video_1_car_002_draft/**`
- Create outside Git: `artifacts/ground_truth/occ_video_1_car_002_work/anchors.csv`
- Create outside Git: `artifacts/ground_truth/occ_video_1_car_002_work/review-policy.csv`
- Create outside Git: `artifacts/ground_truth/occ_video_1_car_002_work/distractors.json`
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Consumes: Task 1 CLI, Task 2 packager, approved design, source video, and the preserved RK3588 run evidence hashes.
- Produces: a validated draft artifact plus explicit contiguous user-review ranges; no formal-accuracy claim.

- [ ] **Step 1: Verify immutable inputs**

Run SHA-256 and ffprobe checks on the source video and preserved RK3588 CSV/log/metadata/video. Expected: source video identity and 271-frame geometry match Global Constraints; preserved evidence matches the hashes recorded during diagnosis.

- [ ] **Step 2: Establish sparse visual anchors**

Inspect clean source frames alongside the preserved debug video. Write anchors only where `car_002` identity and full-object boundary are visually defensible. Record each anchor as `assistant_visual_candidate`, never `manual_confirmed_visible`; include the user-selected frame 0 box as `user_initialization_candidate` until explicit bbox confirmation.

- [ ] **Step 3: Generate bidirectional candidates and inspect all contact sheets**

Run Task 1 CLI across frames 0--270. Review every contact sheet, not only disagreement frames. Split segments or add anchors where a candidate visibly drifts. Mark every occlusion, boundary ambiguity, or same-class identity competition as `needs_human_review`; never tune agreement thresholds to force a pass.

- [ ] **Step 4: Define events, states, estimates, and distractors**

Populate the review policy from clean source video. Full occlusion/identity ambiguity gets no formal or candidate observable bbox; optional bidirectional estimates include basis frames and uncertainty. Assign stable IDs to relevant same-class distractors, including the wrong-recovery vehicle near frames 259--264.

- [ ] **Step 5: Build and validate the draft**

Run Task 2 `build`, then `validate --require-confirmed false`. Verify three 271-file layers, hashes, continuous JSONL/status rows, artifact video/contact-sheet decode, and a nonempty review manifest. Also run `validate --require-confirmed true` and require it to FAIL, proving the draft cannot masquerade as confirmed truth.

- [ ] **Step 6: Record provenance and evidence boundaries**

Append the owned `docs/progress/board-validation.md` with timestamp, branch/base/result commits, source and diagnostic hashes, exact artifact path and hashes, commands, status counts, review ranges, and the explicit boundary that no formal IoU/calibration is authorized before user confirmation. Do not edit STATUS or integration-contract.

- [ ] **Step 7: Run final verification and commit provenance**

Run Task 1 CTest, Task 2 unittest, existing 19-test CTest, `git diff --check`, artifact validator, and `sha256sum -c` manifests. Expected: all software tests and draft-mode validation PASS; confirmed-mode validation intentionally FAILS only because unresolved review rows remain.

```bash
git add docs/progress/board-validation.md
git commit -m "docs: record car 002 annotation draft provenance"
```
