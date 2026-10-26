#!/usr/bin/env python3
"""Read persisted validation tracks; diagnose geometry without guessing failure semantics."""
import argparse
import hashlib
import json
import math
import os
import re
import tempfile
from collections import Counter
from pathlib import Path

import cv2
import numpy as np


def file_identity(path):
    path = Path(path).resolve(strict=True)
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return {'path': str(path), 'sha256': digest.hexdigest()}


def verify_inputs(identities):
    for identity in identities:
        if file_identity(identity['path']) != identity:
            raise ValueError('frozen input hash drift: ' + identity['path'])


def atomic_json(path, value):
    payload = (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2,
                          allow_nan=False) + '\n').encode('utf-8')
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, tmp = tempfile.mkstemp(prefix='.' + path.name, dir=str(path.parent))
    try:
        with os.fdopen(handle, 'wb') as stream:
            stream.write(payload); stream.flush(); os.fsync(stream.fileno())
        os.replace(tmp, str(path))
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def box_xywh(value):
    if not isinstance(value, (list, tuple)) or len(value) != 4:
        raise ValueError('box must be xywh')
    box = [float(v) for v in value]
    if not all(math.isfinite(v) for v in box) or min(box[2:]) <= 0:
        raise ValueError('box must be finite with positive size')
    return box


def overlap(a, b):
    iw = max(0., min(a[0]+a[2], b[0]+b[2])-max(a[0], b[0]))
    ih = max(0., min(a[1]+a[3], b[1]+b[3])-max(a[1], b[1]))
    area = iw * ih
    return area / (a[2]*a[3] + b[2]*b[3] - area)


def runs(mask, minimum=1):
    intervals, start = [], None
    for i, active in enumerate(list(mask) + [False]):
        if active and start is None:
            start = i
        if not active and start is not None:
            if i-start >= minimum:
                intervals.append([start, i-1])
            start = None
    return intervals


def _image_info(path, cache):
    path = str(Path(path).resolve(strict=True))
    if path not in cache:
        image = cv2.imread(path)
        if image is None:
            raise ValueError('cannot decode image: ' + path)
        cache[path] = (image.shape[1], image.shape[0], file_identity(path))
    return cache[path]


def analyze_sequence(sequence, predictions, image_cache=None):
    """Indices in run intervals are zero-based; frame_id always identifies source frame."""
    for key in ('name', 'source', 'source_video', 'category'):
        if not isinstance(sequence.get(key), str) or not sequence[key].strip():
            raise ValueError('missing sequence metadata: ' + key)
    boxes = sequence.get('boxes_xywh', [])
    paths = sequence.get('image_paths', [])
    n = len(boxes)
    if n == 0 or len(predictions) != n or len(paths) != n:
        raise ValueError('GT/prediction/image lengths do not match')
    mask=sequence.get('scored_frame_mask',[b is not None for b in boxes])
    if (len(mask)!=n or any(not isinstance(v,bool) for v in mask) or
            any(valid != (box is not None) for valid,box in zip(mask,boxes)) or not any(mask)):
        raise ValueError('invalid GT scoring mask')
    ids = sequence.get('frame_ids')
    if ids is None:
        ids = []
        for path in paths:
            match = re.search(r'(\d+)$', Path(path).stem)
            if not match:
                raise ValueError('image lacks numeric frame identity')
            ids.append(int(match.group(1)))
    if len(ids) != n or any(not isinstance(v, int) for v in ids) or any(b <= a for a,b in zip(ids, ids[1:])):
        raise ValueError('frame identities must be strictly increasing')
    if 'window_start' in sequence and (ids[0] != sequence['window_start'] or ids[-1] != sequence['window_end']):
        raise ValueError('source frame window identity mismatch')
    frames, labels = [], set()
    image_cache = {} if image_cache is None else image_cache
    first_area = None
    previous_gt = previous_pred = None
    for i, (gt_raw, pred_raw, path) in enumerate(zip(boxes, predictions, paths)):
        pred = box_xywh(pred_raw)
        width, height, identity = _image_info(path, image_cache)
        if gt_raw is None:
            frames.append(dict(index=i,frame_id=ids[i],image=identity,image_size_wh=[width,height],
                gt_xywh=None,pred_xywh=pred,iou=None,target_area_pixels=None,target_area_fraction=None,
                pred_gt_area_ratio=None,gt_scale_from_initial=None,gt_normalized_displacement=None,
                pred_normalized_displacement=None,gt_displacement_per_source_frame=None,
                boundary_clue=None,geometry_labels=[],unscored_reason='missing_GT_in_frozen_panel'))
            previous_gt=previous_pred=None
            continue
        gt=box_xywh(gt_raw)
        area = gt[2]*gt[3]
        first_area = area if first_area is None else first_area
        scale = math.sqrt(area / first_area)
        def displacement(current, previous):
            if previous is None:
                return 0.
            return math.hypot(current[0]+current[2]/2-previous[0]-previous[2]/2,
                              current[1]+current[3]/2-previous[1]-previous[3]/2) / math.sqrt(previous[2]*previous[3])
        motion = displacement(gt, previous_gt)
        boundary = (gt[0] <= 1 or gt[1] <= 1 or gt[0]+gt[2] >= width-1 or gt[1]+gt[3] >= height-1)
        clues = []
        if min(gt[2:]) < 16 or area / (width*height) < .0005:
            clues.append('tiny_target')
        if motion > .5:
            clues.append('rapid_motion')
        if scale >= 1.5 or scale <= 1/1.5:
            clues.append('scale_change')
        if boundary:
            clues.append('boundary_truncation')
        labels.update(clues)
        frames.append(dict(index=i, frame_id=ids[i], image=identity, image_size_wh=[width,height],
            gt_xywh=gt, pred_xywh=pred, iou=overlap(gt,pred), target_area_pixels=area,
            target_area_fraction=area/(width*height), pred_gt_area_ratio=pred[2]*pred[3]/area,
            gt_scale_from_initial=scale, gt_normalized_displacement=motion,
            pred_normalized_displacement=displacement(pred, previous_pred),
            gt_displacement_per_source_frame=motion/(ids[i]-ids[i-1]) if i else 0.,
            boundary_clue=boundary, geometry_labels=clues))
        previous_gt, previous_pred = gt, pred
    zero = runs([f['iou'] == 0 for f in frames])
    drift = runs([f['iou'] is not None and f['iou'] < .2 for f in frames], 5)
    if zero: labels.add('zero_overlap')
    if drift: labels.add('sustained_drift')
    explicit = []
    for event in sequence.get('explicit_events', []):
        if event.get('label') not in ('occlusion', 'distractor', 'identity_switch') or not event.get('annotation'):
            raise ValueError('semantic label requires explicit annotation provenance')
        verify_inputs([event['annotation']])
        raw = json.loads(Path(event['annotation']['path']).read_text())
        if (raw.get('event') != event['label'] or raw.get('sequence') != sequence['name'] or
                raw.get('frame_ids') != event.get('frame_ids') or not event.get('frame_ids') or
                not set(event['frame_ids']) <= set(ids)):
            raise ValueError('explicit annotation event/frame identity mismatch')
        labels.add(event['label']); explicit.append(event)
    # An explicit event does not prove the cause of every low-overlap frame.
    annotated_ids = {v for e in explicit for v in e['frame_ids']}
    ambiguous_frames = [f['frame_id'] for f in frames if f['iou'] is not None and f['iou'] < .2 and f['frame_id'] not in annotated_ids]
    ambiguity = None
    if ambiguous_frames:
        ambiguity = dict(label='ambiguous_requires_human_review', frame_ids=ambiguous_frames,
                         reasons=['low_overlap_does_not_establish_occlusion_distractor_or_identity'],
                         requested_review=['occlusion', 'similar_target_interference', 'identity_switch'])
    return dict(**{k: sequence[k] for k in ('name','source','source_video','category')},
                frames=frames, labels=sorted(labels), zero_iou_runs=zero,
                sustained_drift_intervals=drift, first_sustained_drift=drift[0] if drift else None,
                mean_iou=sum(f['iou'] for f in frames if f['iou'] is not None)/sum(mask), ambiguity=ambiguity,
                explicit_events=explicit)


def make_contact_sheets(records, destination, limit=24):
    if not isinstance(limit, int) or not 0 <= limit <= 24:
        raise ValueError('contact-sheet limit must be 0..24')
    priorities = sorted(records, key=lambda r: (-sum(b-a+1 for a,b in r['sustained_drift_intervals']),
                        r['mean_iou'], r.get('model_key',''), r['name']))[:limit]
    output = []
    for number, record in enumerate(priorities):
        frames = record['frames']
        pivot = record['first_sustained_drift'][0] if record['first_sustained_drift'] else min(
            [i for i,f in enumerate(frames) if f['iou'] is not None],key=lambda i:frames[i]['iou'])
        indices = sorted(set([0, max(0,pivot-1), pivot, min(len(frames)-1,pivot+2),
                              min(len(frames)-1,pivot+5), len(frames)-1]))
        tiles, inputs = [], []
        for idx in indices:
            frame = frames[idx]; verify_inputs([frame['image']])
            image = cv2.imread(frame['image']['path'])
            if image is None: raise ValueError('image decode failed')
            for key, color in [('gt_xywh',(0,255,0)), ('pred_xywh',(0,0,255))]:
                if frame[key] is None: continue
                x,y,w,h = frame[key]
                cv2.rectangle(image, (int(x),int(y)), (int(x+w),int(y+h)), color, 2)
            ratio = min(480/image.shape[1], 300/image.shape[0])
            resized = cv2.resize(image, (max(1,round(image.shape[1]*ratio)), max(1,round(image.shape[0]*ratio))))
            tile = np.zeros((350,480,3), dtype=np.uint8)
            tile[:resized.shape[0], :resized.shape[1]] = resized
            metric='unscored (GT absent)' if frame['iou'] is None else 'IoU={:.3f} scale={:.2f}'.format(frame['iou'],frame['gt_scale_from_initial'])
            cv2.putText(tile, 'frame={} {}'.format(frame['frame_id'],metric),
                        (5,320),cv2.FONT_HERSHEY_SIMPLEX,.45,(255,255,255),1)
            cv2.putText(tile, 'GT green / prediction red', (5,340),cv2.FONT_HERSHEY_SIMPLEX,.45,(255,255,255),1)
            tiles.append(tile); inputs.append(frame['image'])
        while len(tiles) < 6: tiles.append(np.zeros((350,480,3),np.uint8))
        sheet = np.vstack([np.hstack(tiles[:3]), np.hstack(tiles[3:6])])
        destination = Path(destination); destination.mkdir(parents=True,exist_ok=True)
        path = destination / ('{:02d}_{}_{}.png'.format(number, record.get('model_key','model'), record['name']))
        ok, payload = cv2.imencode('.png',sheet)
        if not ok: raise RuntimeError('contact sheet encoding failed')
        fd, tmp = tempfile.mkstemp(prefix='.sheet',dir=str(destination))
        try:
            with os.fdopen(fd,'wb') as stream: stream.write(payload.tobytes()); stream.flush(); os.fsync(stream.fileno())
            os.replace(tmp,str(path))
        finally:
            if os.path.exists(tmp): os.unlink(tmp)
        output.append(dict(sheet=file_identity(path), model_key=record.get('model_key','model'),
                           sequence=record['name'], frame_indices=indices, inputs=inputs))
    return output


def failure_requests(records):
    """Request audited train reasons only when clues co-occur with IoU < 0.5.

    Co-occurrence identifies a useful diagnostic stratum, not a causal explanation.
    """
    requests=set()
    for record in records:
        for frame in record['frames']:
            if frame['iou'] is not None and frame['iou'] < .5:
                for reason in frame['geometry_labels']:
                    requests.add((record['source'],record['category'],reason))
        for event in record['explicit_events']:
            if event['label'] in ('occlusion','distractor') and any(
                    f['frame_id'] in event['frame_ids'] and f['iou'] is not None and f['iou'] < .5 for f in record['frames']):
                requests.add((record['source'],record['category'],event['label']))
    return [dict(source=s,category=c,reason=r) for s,c,r in sorted(requests)]


def run_analysis(root, destination, sheet_limit=24):
    root, destination = Path(root).resolve(), Path(destination).resolve()
    output = destination / 'analysis.json'
    if output.exists():
        verify_inputs(json.loads(output.read_text())['inputs'])
    panel_path = root / 'data/nanotrack_conservative_three_stage/val-aerial.json'
    table_path = root / 'artifacts/nanotrack_got_replay_v2/run/candidate_table.json'
    table = json.loads(table_path.read_text())
    candidates = table['candidates']
    if not candidates or any(not math.isfinite(float(c['metrics']['composite_score'])) for c in candidates):
        raise ValueError('invalid historical candidate scores')
    best = sorted(candidates,key=lambda c:(-c['metrics']['composite_score'],c['key']))[0]
    if best['final_gate']['qualified']:
        raise ValueError('expected persisted unpromoted v2 diagnosis candidate')
    panel = json.loads(panel_path.read_text())
    if panel['split'] != 'validation': raise ValueError('only validation diagnosis is allowed')
    sequence_by_name = {s['name']:s for s in panel['sequences']}
    if len(sequence_by_name) != len(panel['sequences']) or sorted(panel['members']) != sorted(sequence_by_name):
        raise ValueError('validation membership mismatch')
    models = [('g5000',root/'artifacts/nanotrack_got_replay_v2/baseline/baseline/aerial.json'),
              (best['key'],root/'artifacts/nanotrack_got_replay_v2/run/validation'/best['key']/'aerial.json')]
    inputs = [file_identity(panel_path),file_identity(table_path),file_identity(__file__)]
    records, identities, cache = [], [], {}
    for model_key, path in models:
        result = json.loads(path.read_text()); inputs.append(file_identity(path))
        for key in ('snapshot','config'):
            identity = file_identity(result[key])
            if identity['sha256'] != result[key+'_sha256']: raise ValueError('result model/config identity mismatch')
            inputs.append(identity)
        if result['panel_sha256'] != file_identity(panel_path)['sha256'] or result['panel_split'] != 'validation':
            raise ValueError('prediction panel identity mismatch')
        if model_key != 'g5000' and (result['snapshot_sha256'] != best['checkpoint_sha256'] or
                                    file_identity(best['checkpoint'])['sha256'] != best['checkpoint_sha256']):
            raise ValueError('candidate table checkpoint mismatch')
        if sorted(s['name'] for s in result['sequences']) != sorted(sequence_by_name):
            raise ValueError('prediction membership mismatch')
        identities.append(dict(key=model_key, promoted=False if model_key!='g5000' else None,
            snapshot=file_identity(result['snapshot']), result=file_identity(path), config=file_identity(result['config']),
            historical_selection='max persisted v2 composite_score; not v3 ranking' if model_key!='g5000' else 'frozen initialization'))
        for predicted in result['sequences']:
            seq = sequence_by_name[predicted['name']]
            for key in ('source','source_video','category','window_start','window_end'):
                if predicted[key] != seq[key]: raise ValueError('prediction source/frame identity mismatch: '+key)
            if predicted['frame_count'] != len(seq['boxes_xywh']): raise ValueError('frame count mismatch')
            seq=dict(seq,scored_frame_mask=predicted['scored_frame_mask'])
            record = analyze_sequence(seq,predicted['predicted_xywh'],cache)
            record['model_key'] = model_key; records.append(record)
    inputs.extend(v[2] for _,v in sorted(cache.items()))
    inputs = sorted({v['path']:v for v in inputs}.values(),key=lambda v:v['path'])
    sheets = make_contact_sheets(records,destination/'contact-sheets',sheet_limit)
    ambiguity = [dict(model_key=r['model_key'],sequence=r['name'],source=r['source'],
                      source_video=r['source_video'],**r['ambiguity']) for r in records if r['ambiguity']]
    counts = Counter(label for r in records for label in r['labels'])
    report = dict(schema_version=1, protocol='geometry-diagnosis-v1', inputs=inputs, models=identities,
        records=records, counts=dict(counts), contact_sheets=sheets,
        human_assistance_sequence_model_count=len(ambiguity),
        training_requests=failure_requests(records),
        training_request_rule='geometry clue or proven event at same scored frame as IoU < 0.5; co-occurrence is not causation',
        thresholds=dict(tiny_short_side_px=16,tiny_image_area_fraction=.0005,rapid_normalized_displacement=.5,
                        scale_linear_ratio=1.5,drift_iou=.2,drift_min_consecutive_frames=5,boundary_margin_px=1),
        boundaries=['paired diagnosis covers UAVDT/VisDrone-MOT/VisDrone-SOT persisted aerial trajectories',
                    'UAV123 baseline folder retains summary only; paired per-frame baseline predictions unavailable there',
                    'GOT-val is guardrail evidence, not an aerial failure-label source',
                    'boundary_truncation is a geometric edge clue, not proof of semantic truncation',
                    'low overlap alone cannot establish occlusion, distractor, or identity switch',
                    'historical hidden predictions and metrics were not read'])
    atomic_json(destination/'ambiguity-queue.json',dict(schema_version=1,items=ambiguity))
    atomic_json(output,report)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--sheet-limit',type=int,default=24)
    args=parser.parse_args()
    report=run_analysis(args.root,args.output_dir,args.sheet_limit)
    print(json.dumps(dict(records=len(report['records']),sheets=len(report['contact_sheets']),
                          human_assistance=report['human_assistance_sequence_model_count'])))


if __name__=='__main__': main()
