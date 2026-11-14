#!/usr/bin/env bash
# Replay two confirmed occ_video_1 targets with small, human-like ROI changes.
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "usage: $0 BINARY CONFIG OUTPUT_DIR" >&2
  exit 2
fi

binary=$1
config=$2
output_dir=$3
for spec in \
  car001_nominal:992,582,37,39 \
  car001_user:993,585,36,36 \
  car001_left:990,580,39,41 \
  car001_right:994,584,35,37 \
  car002_nominal:970,517,38,38 \
  car002_truth:967,513,42,42 \
  car002_left:968,515,40,40 \
  car002_right:972,519,36,36; do
  label=${spec%%:*}
  roi=${spec#*:}
  run_dir="$output_dir/$label"
  mkdir -p "$run_dir"
  "$binary" --config "$config" --bbox "$roi" \
    --output-video "$run_dir/output.avi" \
    --output-boxes "$run_dir/frames.csv" \
    --session-metadata "$run_dir/session.json" \
    > "$run_dir/run.log" 2>&1
  printf '%s %s ' "$label" "$roi"
  sha256sum "$run_dir/frames.csv" "$run_dir/output.avi" "$run_dir/session.json"
done
