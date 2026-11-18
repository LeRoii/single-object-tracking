# occ_video_1 partial occlusion: full-object estimates v1

`full_object_estimates_v1.json` is a versioned geometry overlay for the ten confirmed car targets. It does not alter the source packages under `data/occ_video_1`. The source package identity for each target, the shared video identity, every estimated box, and its method are recorded in the overlay and in evaluator reports.

When a car is partly hidden, the overlay's `estimated_full_bbox_xywh` covers the estimated *whole car*. Existing source boxes that cover only the visible portion remain in `visible_bbox_xywh`; the original labels remain in their packages. The hidden boundary is uncertain. These frames have `ordinary_iou_allowed=false` and are checked only for a tracking-box center within one estimated car diagonal. Fully occluded and out-of-view frames remain separate states. Previously confirmed full-object visible boxes keep ordinary IoU scoring.

Coverage of estimated full boxes: car_001 frames 169–171 and 181; car_002 frames 211–212 and 217–229; car_003 frames 124–130; car_004 frames 126–132; car_009 frames 115–120; car_010 frames 149–157. car_005–car_008 have no new partial-frame estimates. Car_004 frame 42 remains visible without a measured box, so it remains unscored for geometry. Existing car_004 v4 estimates at frames 127–129 are carried forward unchanged.

Regenerate and compare the overlay before changing this version:

```sh
python3 scripts/prepare_full_object_estimates_v1.py \
  --dataset-root /home/u20/single-object-tracking/data/occ_video_1 \
  --output /tmp/full_object_estimates_v1.json
cmp scripts/truth_overlays/full_object_estimates_v1.json /tmp/full_object_estimates_v1.json
```

Evaluate one saved run with the new truth semantics:

```sh
python3 scripts/evaluate_occ_video_1.py \
  --dataset-root /home/u20/single-object-tracking/data/occ_video_1 \
  --target-id car_004 --truth-version full_object_estimates_v1 \
  --boxes /path/to/frames.csv --scope full \
  --output-json /tmp/car_004-score.json \
  --output-markdown /tmp/car_004-score.md
```

This is a scoring and annotation revision. A PASS with this overlay is not evidence that the tracking algorithm changed or that an estimated box is an exact pixel boundary. Keep the source-version score beside the new score when comparing algorithms.
