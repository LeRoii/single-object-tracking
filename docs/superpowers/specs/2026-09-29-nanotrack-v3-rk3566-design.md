# Fresh NanoTrack V3 RK3566 Deployment Design

## Objective

Create and deploy a new NanoTrack V3 implementation on the RK3566
LubanCat-1. The new implementation must not read, modify, link against, or
copy source code, models, build output, or configuration from the existing
board project at `/home/cat/Project/SOT/nanotrack`.

The new board project lives at `/home/cat/Project/SOT/nanotrack_v3`. Real
headless video tracking with RKNN inference is the acceptance target; model
conversion or initialization alone is insufficient.

## Authoritative Inputs

- Official/author NanoTrack PyTorch checkout on the Linux server:
  `repos/siamtrackers/NanoTrack`, commit
  `248663fde6bf7c40190cf10ee396d5662919ecd3`.
- NanoTrack V3 weight:
  `models/pretrained/nanotrackv3.pth`, SHA-256
  `4f0d25108385a67bdec5fa01ca37c74acb72343b53996dd645fe18b86e512e69`.
- Test video: the author's `bin/girl_dance.mp4`, with the known first-frame
  box `244,161,74,70` used by the verified server inference.
- Rockchip official RKNN-Toolkit2 v2.3.2, tag commit
  `42aa1d426c0a9e0869b6374edba009f7208a1926`.
- Board-wide OpenCV 4.10.0 and Rockchip RKNN Runtime 2.3.2 installations may
  be linked as platform dependencies. No file under the old NanoTrack project
  may be used.

## Target Platform

- EmbedFire LubanCat-1, RK3566, aarch64.
- Debian 10, Linux 4.19.232.
- Four CPU cores, 3.8 GiB RAM.
- `rknpu` kernel module 0.8.2 and NPU clock observed at 600 MHz.
- GCC/G++ 8.3 and CMake 3.13.4.

## Deliverables

The Linux workspace gains a self-contained deployment source directory at
`rk3566/nanotrack_v3` containing:

- deterministic V3 ONNX export and RKNN conversion scripts;
- a new C++ RKNN wrapper, NanoTrack tracker, and headless CLI runner;
- host-side unit tests for pure parsing, validation, decoding, and statistics;
- a board deployment script and verification script;
- a manifest of source commits, model hashes, converter/runtime versions, and
  exact commands.

The board gains only the new `/home/cat/Project/SOT/nanotrack_v3` tree and its
runtime artifacts. The existing `/home/cat/Project/SOT/nanotrack` tree remains
untouched.

## Model Pipeline

The official V3 PyTorch model is exported as three fixed-shape ONNX graphs and
converted independently for `rk3566`:

1. Template backbone: uint8 NHWC image `1x127x127x3`; executed once after
   initialization and cached.
2. Search backbone: uint8 NHWC image `1x255x255x3`; executed once per frame.
3. Head: float feature tensors whose shapes are taken from the freshly
   exported V3 backbone outputs; executed once per frame and producing the
   classification and box-regression tensors.

The conversion script queries and records every ONNX and RKNN tensor name,
shape, layout, and data type. The C++ implementation uses those recorded
interfaces rather than hard-coded channel counts copied from another version.

The first pass uses float feature exchange between the backbones and head to
minimize accuracy risk. Input normalization and channel order are derived from
the official PyTorch preprocessing and validated against an ONNX reference.
INT8 quantization is not introduced until the fresh float deployment matches
the reference trajectory.

## Runtime Interface

The executable is named `nanotrack_v3`:

```text
nanotrack_v3 \
  --video VIDEO \
  --bbox X,Y,W,H \
  --model-dir MODEL_DIR \
  --output-video OUTPUT.avi \
  --output-boxes OUTPUT.csv \
  [--max-frames N]
```

`MODEL_DIR` contains the freshly converted template, search, and head RKNN
files plus their manifest. `--bbox` is zero-based and must have positive width
and height within the first decoded frame.

The CLI has no calls to `selectROI`, `imshow`, `waitKey`, or other display
functions. It writes one CSV row per tracked frame:

```text
frame,x,y,width,height,score,inference_ms
```

The process returns zero only when at least one post-initialization frame is
tracked and both outputs are finalized. Invalid arguments, unreadable input,
invalid boxes, incompatible tensor metadata, RKNN errors, non-finite decoded
values, or writer failures return non-zero with a concise diagnostic.

## C++ Components

### CLI and reporting

The runner parses and validates options, drives decoding and tracking, draws
the returned box, writes CSV/video artifacts, and reports tracked frames,
mean/p50/p95 inference latency, and effective tracking FPS. Inference timing
includes the search backbone, head, and tracker post-processing but excludes
video decoding and encoding.

### RKNN model wrapper

The wrapper owns one `rknn_context`, model bytes, input/output metadata, and
output buffers. Loading queries the SDK/driver versions and all tensor
attributes. Inference validates buffer sizes before calling RKNN. RAII cleanup
releases outputs, destroys contexts, and frees model bytes on every path.

### Tracker

Initialization crops the 127x127 template and caches its feature. Tracking
crops the 255x255 search region, runs the search graph and head, applies the
official V3 softmax, scale/aspect penalties and cosine window, decodes the box,
clips it to the frame, and updates state. Crop padding, resize interpolation,
channel order, stride, output grid, and hyperparameters match the author
configuration `models/config/configv3.yaml`.

## Correctness Strategy

Before board deployment, the exported ONNX graphs must reproduce the PyTorch
model outputs for deterministic template/search tensors within documented
floating-point tolerances. The reconstructed host-side three-graph tracker
must also match the verified PyTorch trajectory on a bounded segment of
`girl_dance.mp4` closely enough that per-frame box drift is measured and
reported rather than assumed.

Host unit tests follow red-green-refactor and cover argument parsing, box
validation, crop geometry, feature-layout conversion, score/box decoding,
clipping, latency percentiles, and summary formatting.

## Board Verification

1. Assert the old project tree has not changed by recording its metadata before
   and after deployment.
2. Configure and compile the new project in a clean build directory.
3. Verify `--help` and invalid-input failures.
4. Run a bounded real-video smoke test with all three fresh V3 RKNN models.
5. Run the complete test video without a display or manual interaction.
6. Confirm CSV rows equal tracked frames, all values are finite, boxes remain
   in frame, and latency is positive.
7. Use `ffprobe` to confirm the output video is decodable and its frame count
   equals the tracked-frame count.
8. Save build logs, runtime logs, per-frame output, result video, hashes, and
   mean/p50/p95/FPS metrics under `artifacts/rk3566_nanotrack_v3` on the Linux
   server.

## Completion Standard

Completion requires fresh evidence from the RK3566 for the entire real-video
run, plus provenance showing that models and code came from the declared
official inputs. A copied old binary, old board model, NCNN/CPU fallback,
host-only inference, model conversion, or load-only test does not satisfy the
goal.
