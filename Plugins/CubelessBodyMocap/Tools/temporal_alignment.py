import json,time
from pathlib import Path
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.optimize import least_squares
from scipy.sparse import lil_matrix
from scipy.ndimage import gaussian_filter1d, distance_transform_edt
import argparse,hashlib,os
parser=argparse.ArgumentParser();parser.add_argument('--input',required=True);parser.add_argument('--output',required=True);args=parser.parse_args()
b=Path(args.output).parent;d=json.loads(Path(args.input).read_text(encoding='utf-8-sig'));assert d['version']==1
n=d['frames'];fps=float(d['fps']);assert 3<=n<=14400 and 1<=fps<=120
F=np.array(d['camera'][:9]).reshape(3,3);T=np.array(d['camera'][9:]);origin=np.array(d['origin']);kp=np.array(d['keypoints']).reshape(n,25,3);P=np.array([[v['p'] for v in f] for f in d['world']]);Q=np.array([[v['q'] for v in f] for f in d['world']]);L=np.array([[v['q'] for v in f] for f in d['local']]);contact=np.array(d['contacts']).reshape(n,6)
for v in [F,T,origin,kp,P,Q,L,contact]:
 if not np.isfinite(v).all():raise ValueError('Nonfinite alignment input')
assert P.shape==(n,len(d['names']),3) and Q.shape==(n,len(d['names']),4)
assert np.max(abs(np.linalg.norm(Q,axis=-1)-1))<1e-3
meta={'camera':[d['focal'],d['center'][0]*2,d['center'][1]*2]}

def proj(p):
 c=(p[..., [0,2,1]]/100+origin)@F.T+T
 return c[...,:2]/c[...,2:]*meta['camera'][0]+np.array(meta['camera'][1:3])/2
def unit(v):return v/np.maximum(np.linalg.norm(v,axis=-1,keepdims=True),1e-12)
def swing(a,c):
 a=unit(a);c=unit(c);v=np.cross(a,c);norm=np.linalg.norm(v,axis=-1);angle=np.arctan2(norm,np.sum(a*c,axis=-1));return R.from_rotvec(v* (angle/np.maximum(norm,1e-12))[:,None])
new=L.copy();stats={};positions=P.copy();all_offsets=[];t=time.monotonic()
for side,(h,k,a,ids) in enumerate([(1,2,3,[12,13,14]),(5,6,7,[9,10,11]),(9,10,11,[5,6,7]),(12,13,14,[2,3,4])]):
 hip,knee,ankle=[P[:,j] for j in [h,k,a]];l1=np.linalg.norm(knee-hip,axis=1);l2=np.linalg.norm(ankle-knee,axis=1);uv=proj(P[:,[h,k,a]]);obs=kp[:,ids,:2].copy();conf=np.min(kp[:,ids,2],axis=1)
 good=(conf>.45)&np.all(np.linalg.norm(np.diff(obs,axis=1),axis=2)>25,axis=1)
 if np.count_nonzero(good)<3:
  stats[str(side)]={"applied":False,"reason":"insufficient detections"};continue
 for j in range(3):
  for c in range(2):obs[:,j,c]=np.interp(np.arange(n),np.flatnonzero(good),obs[good,j,c])
 obs=gaussian_filter1d(obs,.6*fps/30,axis=0,mode='nearest');dirs=unit(np.diff(obs,axis=1));length=np.linalg.norm(np.diff(uv,axis=1),axis=2);target=np.stack([uv[:,0]+dirs[:,0]*length[:,0,None],uv[:,0]+dirs[:,0]*length[:,0,None]+dirs[:,1]*length[:,1,None]],axis=1);weight=np.clip((conf-.35)/.4,0,1);weight=gaussian_filter1d(weight,fps/30)
 if side<2:
  cp=np.min(1/(1+np.exp(-np.clip(contact[:,side*2:side*2+2],-30,30))),axis=1);speed=np.linalg.norm(np.gradient(ankle,axis=0),axis=1)*fps;support=((cp>.8)&(speed<30))|((np.array(d['sole_min'])[:,side]<3)&(speed<25));interval=support[:-1]&support[1:];ramp=np.clip(distance_transform_edt(~support)/(fps/6),0,1);free=ramp*ramp*(3-2*ramp)
 else:
  support=np.ones(n,dtype=bool);interval=np.ones(n-1,dtype=bool);free=np.zeros(n)
 parent=0 if side<2 else 15+side-2

 def calc(x):
  x=x.reshape(n,3);aa=ankle.copy();aa[:,:2]+=x[:,:2]*free[:,None];v=aa-hip;maxxy=np.sqrt(np.maximum((l1+l2-.001)**2-v[:,2]**2,.001));xy=np.linalg.norm(v[:,:2],axis=1);v[:,:2]*=np.minimum(1,maxxy/np.maximum(xy,1e-9))[:,None];aa=hip+v;dist=np.linalg.norm(v,axis=1);axis=unit(v);pole=unit((knee-hip)-axis*np.sum((knee-hip)*axis,axis=1)[:,None]);phi=x[:,2]/10;pole=pole*np.cos(phi)[:,None]+np.cross(axis,pole)*np.sin(phi)[:,None];along=(l1*l1-l2*l2+dist*dist)/(2*dist);radius=np.sqrt(np.maximum(l1*l1-along*along,0));ka=hip+axis*along[:,None]+pole*radius[:,None];return ka,aa
 oldh=R.from_quat(Q[:,h]);oldk=R.from_quat(Q[:,k]);oldvel=[(r[:-1].inv()*r[1:]).as_rotvec() for r in [oldh,oldk]];limstep=np.array([np.linalg.norm(v,axis=1).max() for v in oldvel]);limacc=np.array([np.linalg.norm(np.diff(v,axis=0),axis=1).max() for v in oldvel])
 def rotational_penalty(ka,aa):
  rh=swing(knee-hip,ka-hip)*oldh;rk=swing(ankle-knee,aa-ka)*oldk;vel=[(r[:-1].inv()*r[1:]).as_rotvec() for r in [rh,rk]]
  step=np.stack([np.linalg.norm(v,axis=1) for v in vel],axis=1);acc=np.stack([np.linalg.norm(np.diff(v,axis=0),axis=1) for v in vel],axis=1)
  return np.r_[(np.maximum(step-limstep*.99,0)*1500).ravel(),(np.maximum(acc-limacc*.99,0)*1500).ravel()]
 def fun(flat):
  x=flat.reshape(n,3);ka,aa=calc(x);delta=aa-ankle
  return np.r_[(((proj(np.stack([ka,aa],axis=1))-target)*weight[:,None,None])/3).ravel(),(ka[:,1]-knee[:,1])*.5,(x*.10).ravel(),(np.diff(delta[:,:2],axis=0)*(interval[:,None]*20+.12)).ravel(),(np.diff(x,n=2,axis=0)*.85).ravel(),(np.diff(x,n=3,axis=0)*.4).ravel(),(np.diff(np.concatenate([unit(ka-hip),unit(aa-ka)],axis=1),n=2,axis=0)*(60*(fps/30)**2)).ravel(),rotational_penalty(ka,aa)]
 sizes=[(n,4,0),(n,1,0),(n,3,0),(n-1,2,1),(n-2,3,2),(n-3,3,3),(n-2,6,2),(n-1,2,1),(n-2,2,2)];S=lil_matrix((sum(c*w for c,w,_ in sizes),n*3),dtype=int);row=0
 for count,width,span in sizes:
  for f in range(count):S[row+f*width:row+(f+1)*width,f*3:(f+span+1)*3]=1
  row+=count*width
 fit=least_squares(fun,np.zeros(n*3),jac_sparsity=S.tocsr(),max_nfev=80,ftol=2e-5,xtol=2e-5,gtol=1e-4,verbose=0);ka,aa=calc(fit.x);dh=swing(knee-hip,ka-hip);dk=swing(ankle-knee,aa-ka);wh=dh*R.from_quat(Q[:,h]);wk=dk*R.from_quat(Q[:,k]);new[:,h]=(R.from_quat(Q[:,parent]).inv()*wh).as_quat();new[:,k]=(wh.inv()*wk).as_quat();new[:,a]=(wk.inv()*R.from_quat(Q[:,a])).as_quat();positions[:,k]=ka;positions[:,a]=aa
 vnew=[(r[:-1].inv()*r[1:]).as_rotvec() for r in [wh,wk]]
 safe=all(np.linalg.norm(v,axis=1).max()<=limstep[j]+2e-5 and np.linalg.norm(np.diff(v,axis=0),axis=1).max()<=limacc[j]+2e-5 for j,v in enumerate(vnew))
 old_error=np.linalg.norm(uv[:,1:]-target,axis=2).mean(axis=1);new_error=np.linalg.norm(proj(np.stack([ka,aa],axis=1))-target,axis=2).mean(axis=1)
 safe=safe and new_error[good].mean()<=old_error[good].mean()+1e-4
 rawdirs=unit(np.diff(kp[:,ids,:2],axis=1));before_direction=unit(np.diff(uv,axis=1));after_direction=unit(np.diff(proj(np.stack([hip,ka,aa],axis=1)),axis=1));raw_before=np.arccos(np.clip(np.sum(before_direction*rawdirs,axis=-1),-1,1));raw_after=np.arccos(np.clip(np.sum(after_direction*rawdirs,axis=-1),-1,1));safe=safe and raw_after[good].mean()<=raw_before[good].mean()+1e-6
 if not safe:
  new[:,[h,k,a]]=L[:,[h,k,a]];positions[:,[k,a]]=P[:,[k,a]];ka=knee.copy();aa=ankle.copy()

 err0=np.linalg.norm(uv[:,1:]-target,axis=2).mean(axis=1);err1=np.linalg.norm(proj(np.stack([ka,aa],axis=1))-target,axis=2).mean(axis=1);slip0=np.linalg.norm(np.diff(ankle[:,:2],axis=0),axis=1)*fps;slip1=np.linalg.norm(np.diff(aa[:,:2],axis=0),axis=1)*fps
 def angle_error(v):
  u=unit(np.diff(proj(v),axis=1));return np.rad2deg(np.arccos(np.clip(np.sum(u*dirs,axis=-1),-1,1)))
 angles0=angle_error(P[:,[h,k,a]]);angles1=angle_error(np.stack([hip,ka,aa],axis=1));stats[str(side)]={'mean_screen_angle_deg_before_after':[angles0[good].mean(),angles1[good].mean()],'mean_target_pixels_before_after':[err0[good].mean(),err1[good].mean()],'last_target_pixels_before_after':[err0[-1],err1[-1]],'contact_intervals':int(interval.sum()),'contact_ankle_speed_mean_before_after':[float(slip0[interval].mean()) if interval.any() else None,float(slip1[interval].mean()) if interval.any() else None],'max_ankle_height_delta_cm':float(np.max(np.abs(aa[:,2]-ankle[:,2]))),'applied':bool(safe),'fallback_reason':None if safe else 'screen direction, projection or motion regression','nfev':fit.nfev,'cost':fit.cost,'invalid_detection_frames':int((~good).sum())};all_offsets.append(fit.x.reshape(n,3));print('side',side,stats[str(side)],flush=True)
tracks={}
for j in [1,2,3,5,6,7,9,10,11,12,13,14]:
 q=new[:,j]
 for f in range(1,n):
  if q[f]@q[f-1]<0:q[f]*=-1
 tracks[d['names'][j]]={'p':[v[j]['p'] for v in d['local']],'q':q.tolist(),'s':[v[j]['s'] for v in d['local']]}

# Torso and pelvis remain unchanged: 2D evidence alone cannot establish a safe
# depth/tilt correction while preserving both wrists and planted feet.
audit={}
for label,inds,keys in [('pelvis_axis',[5,1],[9,12]),('shoulder_axis',[12,9],[2,5])]:
 u=unit(np.diff(proj(P[:,inds]),axis=1)[:,0]);v=unit(kp[:,keys[1],:2]-kp[:,keys[0],:2]);ok=np.min(kp[:,keys,2],axis=1)>.5
 audit[label]={'preserved':True,'evidence_frames':int(ok.sum()),'screen_axis_error_degrees':float(np.rad2deg(np.arccos(np.clip(np.sum(u*v,axis=1),-1,1)))[ok].mean()) if ok.any() else None}
result={'version':1,'algorithm':'temporal_v1','source_guid':d['source_guid'],'frames':n,'fps':fps,'rotations':{name:v['q'] for name,v in tracks.items()},'report':{'chains':stats,'torso_pelvis':audit,'seconds':time.monotonic()-t,'code_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'input_sha256':hashlib.sha256(Path(args.input).read_bytes()).hexdigest()}}
out=Path(args.output);tmp=out.with_suffix('.tmp');tmp.write_text(json.dumps(result,allow_nan=False),encoding='utf-8');os.replace(tmp,out)
