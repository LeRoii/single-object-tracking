"""Paired shadow-display evaluation; GT-estimated/absent frames never enter IoU."""
import csv, hashlib, json, math, statistics, sys
from pathlib import Path
ROOT=Path(sys.argv[1]);DATA=Path(sys.argv[2])
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return {int(r['frame_id']):r for r in csv.DictReader(p.open())}
def box(r,p):return tuple(float(r[p+'_'+k]) for k in ('x','y','width','height'))
def center(b):return (b[0]+b[2]/2,b[1]+b[3]/2)
def iou(a,b):
 x=max(0,min(a[0]+a[2],b[0]+b[2])-max(a[0],b[0]));y=max(0,min(a[1]+a[3],b[1]+b[3])-max(a[1],b[1]));inter=x*y
 return inter/(a[2]*a[3]+b[2]*b[3]-inter)
def pct(a,q):
 if not a:return None
 a=sorted(a);p=(len(a)-1)*q;l=int(p);return a[l]+(a[min(l+1,len(a)-1)]-a[l])*(p-l)
def second(a,b,c):return math.hypot(c[0]-2*b[0]+a[0],c[1]-2*b[1]+a[1])
def truth(target):
 base=DATA if target=='car001' else DATA/'ground_truth/car_002'
 expected='9ec01854884126350f93e1af3a4aee002d38ce7c2d50c71c78de0f1a8dc0a648' if target=='car001' else '2f0e78ab1123cb035c87efa06171f1057c108f38b06fcc5c1ed3a3ec63fdbe98'
 assert sha(base/'labels.sha256')==expected
 for line in (base/'labels.sha256').read_text().splitlines():
  digest,path=line.split(maxsplit=1);assert sha(base/path)==digest,path
 status=rows(base/'annotation_status.csv');out={}
 for f,r in status.items():
  if target=='car001':valid=r['state']=='visible' and r['review_status'] in ('confirmed','accepted_candidate')
  else:valid=r['review_status']=='manual_confirmed_visible' and r['target_state']=='visible'
  if not valid:continue
  v=list(map(float,(base/f'labels/frame_{f:06d}.txt').read_text().split()))
  assert len(v)==5 and v[0]==0
  _,x,y,w,h=v;out[f]=((x-w/2)*1920,(y-h/2)*1080,w*1920,h*1080)
 return out,{'labels_manifest_sha256':sha(base/'labels.sha256'),'annotation_status_sha256':sha(base/'annotation_status.csv'),'annotation_metadata_sha256':sha(base/'annotation_metadata.json'),'visible_labeled_frames':len(out)}
def metrics(r,gt,frames,prefix):
 bs={i:box(r[i],prefix) for i in frames};cs={i:center(b) for i,b in bs.items()};gs={i:center(gt[i]) for i in frames}
 errors={i:(cs[i][0]-gs[i][0],cs[i][1]-gs[i][1]) for i in frames}
 overlaps=[iou(bs[i],gt[i]) for i in frames];d=[math.hypot(*e) for e in errors.values()]
 triples=[i for i in frames if i-1 in bs and i-2 in bs]
 accel=[second(cs[i-2],cs[i-1],cs[i]) for i in triples]
 residual=[second(errors[i-2],errors[i-1],errors[i]) for i in triples]
 size_accel=[max(abs(bs[i][a]-2*bs[i-1][a]+bs[i-2][a])/max(1,bs[i-1][a]) for a in (2,3)) for i in triples]
 lag=[]
 for i in frames:
  if i-2 not in gs or i+2 not in gs:continue
  vx=gs[i+2][0]-gs[i-2][0];vy=gs[i+2][1]-gs[i-2][1];norm=math.hypot(vx,vy)
  if norm<2:continue
  lag.append(-(errors[i][0]*vx+errors[i][1]*vy)/norm)
 return {'frames':len(frames),'iou_mean':statistics.mean(overlaps),'iou_below_05':sum(v<.5 for v in overlaps),'center_error_mean_px':statistics.mean(d),'center_error_p95_px':pct(d,.95),'center_second_difference_p95_px':pct(accel,.95),'gt_residual_second_difference_p95_px':pct(residual,.95),'size_second_difference_p95_ratio':pct(size_accel,.95),'lag_along_gt_motion_median_px':pct(lag,.5),'lag_along_gt_motion_p95_px':pct(lag,.95),'triplets':len(triples)}
result={'source':'390b849f7e95cc778fdee6aa6e7b441f73056e55','video_sha256':sha(DATA/'1.mp4'),'method':'Same-frame shadow outputs; positive lag is behind GT motion. IoU uses visible labeled frames; smoothness uses consecutive visible accepted TRACKING triplets. Residual jitter includes annotation noise. No human GT for people.','runs':{}}
for target in ('car001','car002'):
 gt,identity=truth(target)
 for feeder in ('off','on'):
  name=target+'-'+feeder;r=rows(ROOT/(name+'-ab.csv'));native=rows(ROOT/(name+'.csv'))
  assert len(r)==len(native)==271
  assert all(x['target_class_status']=='ESTABLISHED' and x['target_class_id']=='2' for x in native.values())
  delta=max(abs(box(v,feeder)[j]-box(v,'actual')[j]) for v in r.values() for j in range(4));assert delta<.00002,delta
  visible=[i for i in gt if i in r];frames=[i for i in visible if r[i]['tracking_phase']=='TRACKING' and r[i]['accepted']=='1']
  result['runs'][name]={'gt':identity,'csv_sha256':sha(ROOT/(name+'-ab.csv')),'live_shadow_max_delta_px':delta,'native_phase_counts':{k:sum(x['tracking_phase']==k for x in r.values()) for k in ['TRACKING','SUSPECTED','SEARCHING']},'accepted_tracking':{p:metrics(r,gt,frames,p) for p in ('off','on')},'all_visible':{p:metrics(r,gt,visible,p) for p in ('off','on')},'pre_occlusion':{p:metrics(r,gt,[i for i in frames if i<169],p) for p in ('off','on')}}
print(json.dumps(result,indent=2))
