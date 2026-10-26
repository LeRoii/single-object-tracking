#!/usr/bin/env python3
"""Build a deterministic training-only pool using audited per-frame hard reasons."""
import argparse
import json
import re
from collections import Counter
from pathlib import Path

try:
    from scripts.analyze_nanotrack_failures import atomic_json, box_xywh, file_identity, verify_inputs
except ModuleNotFoundError:
    from analyze_nanotrack_failures import atomic_json, box_xywh, file_identity, verify_inputs


REASONS = dict(tiny_target='small', rapid_motion='motion', scale_change='scale',
               boundary_truncation='truncation', occlusion='occlusion', distractor='distractor')


def canonical_group(source, video):
    """Conservative exclusion identity; exact split membership is checked separately."""
    if source not in ('UAV123','UAVDT','VisDrone-MOT','VisDrone-SOT') or not isinstance(video,str):
        raise ValueError('unknown source identity')
    name=video.replace('\\','/').rstrip('/').split('/')[-1].lower()
    name=re.sub(r'\.(mp4|avi|mov|mkv|json|txt|jpg|png)$','',name)
    if source=='UAVDT':
        match=re.fullmatch(r'(?:uavdt__)?(m\d{4})(?:__(?:id\d+|seg\d+))*',name)
        if not match: raise ValueError('unknown UAVDT source group: '+video)
        return 'uavdt:'+match.group(1)
    if source.startswith('VisDrone'):
        match=re.fullmatch(r'(?:(?:visdrone_(?:mot|sot)(?:_p\d+)?)__)?(uav\d{7})(?:_\d+_[vs])?(?:__(?:id\d+|seg\d+))*',name)
        if not match: raise ValueError('unknown VisDrone source group: '+video)
        return 'visdrone:'+match.group(1)
    match=re.fullmatch(r'([a-z]+\d+)(?:(?:_s)|(?:_\d+))*(?:__seg\d+)?',name)
    if not match: raise ValueError('unknown UAV123 source group: '+video)
    return 'uav123:'+match.group(1)


def _load(path): return json.loads(Path(path).read_text())


def build_pool(audit_paths, train_paths, split_path, validation_panel, requests, cap_per_request=128):
    if len(audit_paths)!=len(train_paths) or not audit_paths or not isinstance(cap_per_request,int) or cap_per_request<1:
        raise ValueError('invalid pool source lists/cap')
    splits=_load(split_path)
    panel=_load(validation_panel)
    if panel.get('split')!='validation': raise ValueError('wrong exclusion panel split')
    protected=set()
    for source, parts in splits['sources'].items():
        for split in ('validation','hidden-test'):
            protected.update(canonical_group(source,v) for v in parts[split])
    for seq in panel['sequences']:
        if seq['source_video'] not in splits['sources'][seq['source']]['validation']:
            raise ValueError('validation source missing from split manifest')
        protected.add(canonical_group(seq['source'],seq['source_video']))
    requests=sorted(requests,key=lambda r:(r['source'],r['category'],r['reason']))
    if len({(r['source'],r['category'],r['reason']) for r in requests})!=len(requests):
        raise ValueError('duplicate failure request')
    for request in requests:
        if request['reason'] not in REASONS or request['source'] not in splits['sources']:
            raise ValueError('unsupported failure request')
    inputs=[file_identity(split_path),file_identity(validation_panel),file_identity(__file__),
            file_identity(Path(__file__).with_name('analyze_nanotrack_failures.py'))]
    eligible, excluded=[],[]
    source_pairs=[]
    for audit_path,train_path in zip(audit_paths,train_paths):
        audit_identity,train_identity=file_identity(audit_path),file_identity(train_path)
        inputs.extend([audit_identity,train_identity]); source_pairs.append(dict(audit=audit_identity,train=train_identity))
        audit,train=_load(audit_path),_load(train_path)
        source=audit['source']
        if source not in splits['sources']: raise ValueError('unknown source identity')
        exact_train=set(splits['sources'][source]['train'])
        if set(audit['train_videos']) != exact_train: raise ValueError('audit/split exact train identities differ')
        if set(train)-set(audit['provenance']): raise ValueError('training member lacks provenance')
        for track, provenance in sorted(audit['provenance'].items()):
            video=provenance['source_video']; group=canonical_group(source,video)
            if video not in exact_train: raise ValueError('non-train or unknown source-video identity: '+video)
            if group in protected:
                excluded.append(dict(source=source,track=track,source_video=video,canonical_group=group,
                                     reason='canonical_parent_overlaps_validation_or_historical_hidden'))
                continue
            for frame, reasons in sorted(audit['hard_reasons'].get(track,{}).items(),key=lambda x:int(x[0])):
                raw=provenance['frames'].get(str(frame))
                if raw is None: raise ValueError('hard frame lacks source annotation')
                frame_key='{:06d}'.format(int(frame))
                # Hard pools may retain neighboring references; they are not treated as a matching reason.
                matches=[t[frame_key] for t in train.get(track,{}).values() if frame_key in t]
                if not matches: continue
                box=box_xywh(raw['bbox_xywh'])
                expected=[box[0],box[1],box[0]+box[2],box[1]+box[3]]
                if any(list(map(float,m))!=expected for m in matches): raise ValueError('source/training box mismatch')
                for request in requests:
                    if request['source']==source and request['category']==provenance['category'] and REASONS[request['reason']] in reasons:
                        eligible.append(dict(track=track,frame_id=int(frame),track_id=provenance['track_id'],
                            category=provenance['category'],source=source,source_video=video,canonical_group=group,
                            matched_reason=request['reason'],audit_reason=REASONS[request['reason']],
                            bbox_xywh=box,image_path=raw['image_path'],source_annotation=train_identity,
                            hard_reason_annotation=audit_identity))
    members=[]
    counts=Counter()
    for member in sorted(eligible,key=lambda m:(m['source'],m['category'],m['matched_reason'],m['track'],m['frame_id'])):
        key=(member['source'],member['category'],member['matched_reason'])
        if counts[key]<cap_per_request:
            member['image']=file_identity(member['image_path'])
            members.append(member); counts[key]+=1
    inputs.extend(m['image'] for m in members)
    inputs=sorted({i['path']:i for i in inputs}.values(),key=lambda i:i['path'])
    return dict(schema_version=1, protocol='training-only-failure-matched-v1',inputs=inputs,
        source_pairs=source_pairs,split=file_identity(split_path),validation_panel=file_identity(validation_panel),
        requests=requests,cap_per_request=cap_per_request,
        deterministic_selection='sort source/category/reason/track/frame; first cap_per_request; no validation members',
        canonical_policy='exact split membership first; additional parent-ID exclusion across VisDrone MOT/SOT; UAV123 segment/_s collapse',
        protected_canonical_groups=sorted(protected),members=members,excluded_training_tracks=excluded,
        request_counts=[dict(**r,selected=counts[(r['source'],r['category'],r['reason'])],
                            available=sum(1 for e in eligible if all(e[k]==r[k] for k in ('source','category')) and e['matched_reason']==r['reason'])) for r in requests],
        boundaries=['Only split/group membership for historical hidden panels is read; no hidden prediction/metric access',
                    'hard reasons retain upstream annotation/geometry meaning; neighbor is not a failure label',
                    'canonical parent exclusion is conservative and does not relabel the historical split'])


def verify_pool(path):
    """Read-only: recompute provenance and exact deterministic membership, without writing."""
    pool=_load(path)
    verify_inputs(pool['inputs'])
    rebuilt=build_pool([p['audit']['path'] for p in pool['source_pairs']],
                       [p['train']['path'] for p in pool['source_pairs']],pool['split']['path'],
                       pool['validation_panel']['path'],pool['requests'],pool['cap_per_request'])
    if pool.get('diagnosis'):
        verify_inputs([pool['diagnosis']])
        if _load(pool['diagnosis']['path'])['training_requests'] != pool['requests']:
            raise ValueError('diagnosis request identity mismatch')
        rebuilt['diagnosis']=pool['diagnosis']
    if rebuilt!=pool: raise ValueError('pool provenance/membership mismatch')
    return dict(verified=True,members=len(pool['members']),
                canonical_train_groups=len({m['canonical_group'] for m in pool['members']}),
                excluded_training_tracks=len(pool['excluded_training_tracks']))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path)
    parser.add_argument('--analysis',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--verify',type=Path)
    parser.add_argument('--cap-per-request',type=int,default=128)
    args=parser.parse_args()
    if args.verify:
        print(json.dumps(verify_pool(args.verify),sort_keys=True)); return
    if not args.root or not args.analysis or not args.output:
        parser.error('--root, --analysis and --output required unless --verify')
    if args.output.exists(): verify_pool(args.output)
    analysis=_load(args.analysis); verify_inputs(analysis['inputs'])
    base=args.root.resolve()/'data/nanotrack_conservative_three_stage'
    domains=('uav123','uavdt','visdrone_mot','visdrone_sot')
    pool=build_pool([base/d/'audit.json' for d in domains],[base/d/'train.json' for d in domains],
                    base/'source-video-splits.json',base/'val-aerial.json',analysis['training_requests'],args.cap_per_request)
    # Bind diagnosis requests to their source, not to validation members.
    pool['diagnosis']=file_identity(args.analysis)
    # verify_pool supports this optional provenance link and recomputes request identity below.
    atomic_json(args.output,pool)
    print(json.dumps(dict(members=len(pool['members']),excluded_training_tracks=len(pool['excluded_training_tracks']))))


if __name__=='__main__': main()
