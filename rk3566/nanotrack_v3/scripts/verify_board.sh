#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 ]]; then
  echo "usage: $0 ARTIFACT_DIR MODEL_DIR EXPECTED_OUTPUT_FRAMES FRAME_WIDTH FRAME_HEIGHT" >&2
  exit 2
fi

artifact_dir=$1
model_dir=$2
expected_frames=$3
frame_width=$4
frame_height=$5

for file in run.log bboxes.csv output.avi manifest.json session-metadata.json; do
  if [[ ! -s "$artifact_dir/$file" ]]; then
    echo "missing or empty artifact: $artifact_dir/$file" >&2
    exit 1
  fi
done

for file in template_backbone.rknn search_backbone.rknn head.rknn; do
  if [[ ! -s "$model_dir/$file" ]]; then
    echo "missing or empty model: $model_dir/$file" >&2
    exit 1
  fi
done

if grep -Eqi 'GTK|selectROI|imshow|cannot open display|RKNN:.*(error|unsupported)|rknn_.*failed' "$artifact_dir/run.log"; then
  echo "runtime log contains a GUI/display or RKNN error" >&2
  exit 1
fi

python3 - "$artifact_dir" "$model_dir" "$expected_frames" "$frame_width" "$frame_height" <<'PY'
import csv
import hashlib
import json
import math
import pathlib
import re
import sys

artifact_dir = pathlib.Path(sys.argv[1])
model_dir = pathlib.Path(sys.argv[2])
expected = int(sys.argv[3])
width = int(sys.argv[4])
height = int(sys.argv[5])

manifest = json.loads((artifact_dir / "manifest.json").read_text())
for model in manifest["models"].values():
    path = model_dir / model["file"]
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != model["sha256"]:
        raise SystemExit(f"model hash mismatch: {path}")

with (artifact_dir / "bboxes.csv").open(newline="") as stream:
    rows = list(csv.DictReader(stream))
if len(rows) != expected:
    raise SystemExit(f"CSV row count {len(rows)} != expected {expected}")
required = {
    "frame_id", "timestamp_ms", "state", "quality", "control_valid",
    "accepted", "reacquired", "x", "y", "width", "height",
    "anchor_score", "anchor_psr", "anchor_apce", "recent_score",
    "recent_psr", "recent_apce", "search_scale", "detector_status",
    "tracker_ms", "runtime_ms", "pipeline_ms",
}
missing = required - set(rows[0])
if missing:
    raise SystemExit(f"CSV header is missing: {sorted(missing)}")
first_frame_id = int(rows[0]["frame_id"])
last_timestamp = None
for index, row in enumerate(rows):
    values = {key: float(row[key]) for key in {"x", "y", "width", "height"}}
    if int(row["frame_id"]) != first_frame_id + index:
        raise SystemExit(f"non-sequential frame id at row {index}")
    timestamp = int(row["timestamp_ms"])
    if last_timestamp is not None and timestamp <= last_timestamp:
        raise SystemExit(f"non-monotonic timestamp at row {index}")
    last_timestamp = timestamp
    if row["control_valid"] not in {"0", "1"}:
        raise SystemExit(f"invalid control_valid at row {index}")
    if not all(math.isfinite(value) for value in values.values()):
        raise SystemExit(f"non-finite value at row {index}")
    if values["width"] <= 0 or values["height"] <= 0:
        raise SystemExit(f"non-positive box size at row {index}")
    if index > 0:
        if (float(row["tracker_ms"]) <= 0 or
                float(row["runtime_ms"]) <= 0 or
                float(row["pipeline_ms"]) <= 0):
            raise SystemExit(f"non-positive latency at row {index}")
    if values["x"] < 0 or values["y"] < 0:
        raise SystemExit(f"negative box origin at row {index}")
    if values["x"] + values["width"] > width + 1e-3:
        raise SystemExit(f"box exceeds frame width at row {index}")
    if values["y"] + values["height"] > height + 1e-3:
        raise SystemExit(f"box exceeds frame height at row {index}")

metadata = json.loads((artifact_dir / "session-metadata.json").read_text())
for key in {
    "schema_version", "manifest_hash", "controller_config_hash", "sensor",
    "calibrated", "rknn_api_version", "rknn_driver_version",
    "mean_runtime_ms", "p50_runtime_ms", "p95_runtime_ms", "runtime_fps",
    "mean_pipeline_ms", "p50_pipeline_ms", "p95_pipeline_ms",
    "end_to_end_fps",
}:
    if key not in metadata:
        raise SystemExit(f"session metadata missing {key}")
if metadata["tracked_frames"] != expected - 1:
    raise SystemExit("session metadata tracked frame count mismatch")

log = (artifact_dir / "run.log").read_text(errors="replace")
match = re.search(
    r"SUMMARY tracked_frames=(\d+) mean_tracker_ms=([0-9.]+) "
    r"mean_runtime_ms=([0-9.]+) p50_runtime_ms=([0-9.]+) "
    r"p95_runtime_ms=([0-9.]+) runtime_fps=([0-9.]+) "
    r"mean_pipeline_ms=([0-9.]+) p50_pipeline_ms=([0-9.]+) "
    r"p95_pipeline_ms=([0-9.]+) end_to_end_fps=([0-9.]+) "
    r"calibrated=([01]) metadata=(\S+)",
    log,
)
if not match:
    raise SystemExit("summary metrics missing from run.log")
if int(match.group(1)) != expected - 1:
    raise SystemExit("summary tracked frame count mismatch")
if any(float(value) <= 0 for value in match.groups()[1:10]):
    raise SystemExit("invalid summary metrics")
if pathlib.Path(match.group(12)).name != "session-metadata.json":
    raise SystemExit("summary metadata path does not name session-metadata.json")
PY

video_frames=$(ffprobe -v error -select_streams v:0 -count_frames \
  -show_entries stream=nb_read_frames -of default=nokey=1:noprint_wrappers=1 \
  "$artifact_dir/output.avi")
if [[ "$video_frames" != "$expected_frames" ]]; then
  echo "output video frame count $video_frames != expected $expected_frames" >&2
  exit 1
fi

echo "PASS board artifacts output_frames=$expected_frames tracked_frames=$((expected_frames - 1))"
