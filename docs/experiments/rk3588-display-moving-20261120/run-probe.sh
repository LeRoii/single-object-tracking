#!/bin/bash
set -euo pipefail
cd /home/neardi/Project/SOT/nt-display-moving-390b849-probe
assets=/home/neardi/Project/SOT/nt-ten-target-33f34ba/assets
model=$assets/models
detector=$assets/releases/yolov8n-visdrone9-rk3588-fp16-v1/models/yolov8n-visdrone9-rk3588-fp16.rknn
sha256sum "$model"/*.rknn "$model/manifest.json" "$assets/releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json" "$model/visdrone9.txt" "$detector" "$assets/data/occ_video_1/1.mp4" > runs/inputs.sha256
cp rk3566/nanotrack_v3/config/rk3588-display-stability-candidate.conf runs/on.conf
sed 's/^display_stability_enabled=true$/display_stability_enabled=false/' runs/on.conf > runs/off.conf
sha256sum runs/*.conf > runs/configs.sha256
for target in car001 car002; do
  if [ "$target" = car001 ]; then bbox=992,582,37,39; else bbox=967,513,42,42; fi
  for mode in off on; do
    DISPLAY_AB_CSV="runs/$target-$mode-ab.csv" build/nanotrack_v3 --video "$assets/data/occ_video_1/1.mp4" --bbox "$bbox" --model-dir "$model" --output-video "runs/$target-$mode-control.avi" --output-size 480x270 --output-boxes "runs/$target-$mode.csv" --session-metadata "runs/$target-$mode.metadata.json" --controller-config "runs/$mode.conf" --max-frames 271 --sensor visible --detector-model "$detector" --detector-manifest "$assets/releases/yolov8n-visdrone9-rk3588-fp16-v1/models/detector-manifest.json" --detector-labels "$model/visdrone9.txt" > "runs/$target-$mode.log" 2>&1
  done
done
sha256sum runs/*.csv runs/*.metadata.json runs/*.log > runs/outputs.sha256
