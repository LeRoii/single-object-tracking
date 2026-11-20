import cv2,csv,json,sys
import numpy as np
from pathlib import Path
root=Path(sys.argv[1]);data=Path(sys.argv[2]);out=Path(sys.argv[3]);out.mkdir(exist_ok=True)
def box(r,p):return tuple(float(r[p+'_'+k]) for k in ('x','y','width','height'))
def gt_for(target):
 base=data if target=='car001' else data/'ground_truth/car_002';status={int(r['frame_id']):r for r in csv.DictReader((base/'annotation_status.csv').open())};gt={}
 for f,r in status.items():
  if target=='car001':valid=r['state']=='visible'
  else:valid=r['target_state']=='visible' and r['review_status']=='manual_confirmed_visible'
  if valid:
   _,x,y,w,h=map(float,(base/f'labels/frame_{f:06d}.txt').read_text().split());gt[f]=((x-w/2)*1920,(y-h/2)*1080,w*1920,h*1080)
 return gt
for target in ('car001','car002'):
 rows=list(csv.DictReader((root/(target+'-on-ab.csv')).open()));gt=gt_for(target)
 cap=cv2.VideoCapture(str(data/'1.mp4'));writer=cv2.VideoWriter(str(out/(target+'-compare.mp4')),cv2.VideoWriter_fourcc(*'mp4v'),30,(1440,560));shots=[];last=gt[0]
 for r in rows:
  ok,img=cap.read();assert ok;f=int(r['frame_id']);last=gt.get(f,last);cx=last[0]+last[2]/2;cy=last[1]+last[3]/2;x0=max(0,min(1680,int(cx-120)));y0=max(0,min(920,int(cy-80)));panels=[]
  for mode,color in [('off',(50,50,255)),('on',(0,255,0))]:
   crop=img[y0:y0+160,x0:x0+240].copy()
   for b,col in [(gt.get(f),(0,255,255)),(box(r,mode),color)]:
    if b is not None:cv2.rectangle(crop,(round(b[0]-x0),round(b[1]-y0)),(round(b[0]+b[2]-x0),round(b[1]+b[3]-y0)),col,1)
   panel=np.zeros((560,720,3),np.uint8);panel[80:]=cv2.resize(crop,(720,480),interpolation=cv2.INTER_NEAREST)
   cv2.putText(panel,f'{target} {mode.upper()}  frame {f}  {r["tracking_phase"]}',(12,26),cv2.FONT_HERSHEY_SIMPLEX,.65,(255,255,255),1,cv2.LINE_AA)
   cv2.putText(panel,'Yellow: visible GT; colored: display box',(12,54),cv2.FONT_HERSHEY_SIMPLEX,.6,(0,255,255),1,cv2.LINE_AA);panels.append(panel)
  frame=np.concatenate(panels,axis=1);writer.write(frame)
  if f in [30,90,150,168,184,230]:shots.append(cv2.resize(frame,(960,373)))
 cap.release();writer.release();cv2.imwrite(str(out/(target+'-contact.jpg')),np.concatenate(shots,axis=0))
