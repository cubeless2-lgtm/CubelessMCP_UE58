"""Offline target-derived limb contact correction. Never runs a pose estimator.
Only limb-local rotations change; pelvis/spine, fingers, all local positions/scales
and timing are preserved. Capsules approximate skin; inspect the rendered result.
"""
import argparse,json,time
from pathlib import Path
import numpy as np
from scipy.spatial.transform import Rotation

EDIT=[f'{b}_{s}' for s in ['l','r'] for b in ['thigh','calf','foot','upperarm','lowerarm','hand']]

def solve(request,iterations=400,device='cuda'):
 if request.get('version')!=1 or type(request.get('frames')) is not int or not 2<=request['frames']<=3600:raise ValueError('Unsupported contact request version/frame count')
 if not 1<=iterations<=5000:raise ValueError('Iterations must be 1..5000')
 import torch
 started=time.perf_counter();d=request;n=d['frames'];names=d['names'];parents=d['parents'];b=len(names)
 if len(set(names))!=b or b>1024 or len(parents)!=b or any(type(x) is not int or x < -1 or x>=i for i,x in enumerate(parents)):raise ValueError('Invalid contact hierarchy')
 q=np.asarray(d['local_q']).reshape(n,b,4);p=np.asarray(d['local_p']).reshape(n,b,3)
 scale=np.asarray(d['local_s']).reshape(n,b,3)
 if not all(np.isfinite(x).all() for x in [q,p,scale]) or not np.allclose(np.linalg.norm(q,axis=-1),1,atol=1e-4):raise ValueError('Invalid source transforms')
 if not np.allclose(scale,1,atol=1e-5):raise ValueError('Non-unit scale needs a scale-aware contact model')
 # Keep only ancestors of the measured limb surfaces; finger pose itself is fixed.
 ends={f'{a}_{s}':f'{z}_{s}' for s in ['l','r'] for a,z in [('thigh','calf'),('calf','foot'),('foot','ball'),('upperarm','lowerarm'),('lowerarm','hand'),('hand','middle_03')]}
 need=set()
 for name in EDIT+list(ends.values()):
  i=names.index(name)
  while i>=0:need.add(i);i=parents[i]
 keep=sorted(need);lookup={i:j for j,i in enumerate(keep)};nn=[names[i] for i in keep];pp=[lookup.get(parents[i],-1) for i in keep]
 def tensor(a):return torch.as_tensor(a,dtype=torch.float32,device=device)
 lr=tensor(Rotation.from_quat(q[:,keep].reshape(-1,4)).as_matrix().reshape(n,len(keep),3,3));lp=tensor(p[:,keep])
 def fk(local,locks=None):
  ro=[];pos=[]
  for i,pa in enumerate(pp):
   rr=local[:,i] if pa<0 else ro[pa]@local[:,i]
   if locks is not None and nn[i] in locks:rr=locks[nn[i]]
   ro.append(rr);pos.append(lp[:,i] if pa<0 else pos[pa]+(ro[pa]@lp[:,i,:,None])[...,0])
  return torch.stack(ro,1),torch.stack(pos,1)
 with torch.no_grad():base_r,base_p=fk(lr)
 locked={name:base_r[:,nn.index(name)] for name in EDIT if name.startswith('foot') or (name.startswith('hand') and not d.get('free_hand_orientation',False))}
 joints=[name for name in EDIT if name not in locked and not (d.get('freeze_legs',False) and name.startswith(('thigh','calf')))];ids=[nn.index(x) for x in joints]
 bone_ids=[nn.index(x) for x in EDIT];end_ids=[nn.index(ends[x]) for x in EDIT]
 geometry=[]
 for name,j,k in zip(EDIT,bone_ids,end_ids):
  points=np.asarray(d['skin_samples'][name],float).reshape(-1,3)
  v=((base_r[0,j].T@(base_p[0,k]-base_p[0,j])).cpu().numpy());length=np.linalg.norm(v);axis=v/max(length,1e-8)
  along=points@axis;radial=np.linalg.norm(points-along[:,None]*axis,axis=1)
  sample=radial[(along>length*.20)&(along<length*.80)]
  if len(sample)<12:sample=radial
  radius=float(np.quantile(sample,.90))
  if name.startswith('hand'):radius=min(radius,3.)
  if name.startswith('foot'):radius=min(radius,4.)
  if not 0.4<radius<18:raise ValueError(f'Bad target contact geometry {name}: {radius}')
  lo,hi=(.24,.86) if name.startswith('thigh') else (.15,.85)
  geometry.append(dict(name=name,radius_cm=radius,start=lo,end=hi))
 pairs=[]
 for i in range(3):
  for j in range(6,9):pairs.append((i,j))
 for i in [4,5,10,11]:
  for j in [0,1,2,6,7,8]:pairs.append((i,j))
 pairs.extend([(5,11),(5,10),(11,4)])
 ia=torch.tensor([x[0] for x in pairs],device=device);ib=torch.tensor([x[1] for x in pairs],device=device)
 radii=tensor([x['radius_cm'] for x in geometry]);margin=radii[ia]+radii[ib]+.15
 lo=tensor([x['start'] for x in geometry])[None,:,None];hi=tensor([x['end'] for x in geometry])[None,:,None]
 def collision(points):
  a=points[:,bone_ids];z=points[:,end_ids];v=z-a;aa=a+lo*v;bb=a+hi*v
  a,b,c,e=aa[:,ia],bb[:,ia],aa[:,ib],bb[:,ib]
  u,v,w=b-a,e-c,a-c
  A=(u*u).sum(-1);B=(u*v).sum(-1);C=(v*v).sum(-1);D=(u*w).sum(-1);E=(v*w).sum(-1)
  den=A*C-B*B;s=torch.where(den>1e-6,(B*E-C*D)/den.clamp_min(1e-6),torch.zeros_like(den)).clamp(0,1)
  t=(B*s+E)/C.clamp_min(1e-6)
  s=torch.where(t<0,(-D/A.clamp_min(1e-6)).clamp(0,1),s)
  s=torch.where(t>1,((B-D)/A.clamp_min(1e-6)).clamp(0,1),s)
  t=((B*s+E)/C.clamp_min(1e-6)).clamp(0,1)
  distance=((a+s[...,None]*u-c-t[...,None]*v).square().sum(-1)+1e-8).sqrt()
  return (margin-distance).clamp_min(0)
 before=collision(base_p).detach();active=before.max(1).values>.05
 # Reference-guided contact side is explicit: collision avoidance alone has two
 # equally valid escape sides. Never infer a visible/front side from penetration.
 surface=[]
 for hint in d.get('surface_hints',[]):
  hand=hint['hand'];segments=hint['segments']
  if hand not in ('hand_l','hand_r') or not segments or any(x not in EDIT for x in segments):raise ValueError('Invalid hand surface association')
  for key,default in [('outward',None),('view_outward',[0,1,0])]:
   vector=np.asarray(hint.get(key,default),dtype=float)
   if vector.shape!=(3,) or not np.isfinite(vector).all() or np.linalg.norm(vector)<1e-6:raise ValueError('Invalid reference surface direction')
  if not np.isfinite(float(hint.get('weight',200.))) or float(hint.get('weight',200.))<=0 or not np.isfinite(float(hint.get('axis_offset_cm',0))):raise ValueError('Invalid reference surface weight/offset')
  direction=tensor(hint['outward']);direction=direction/torch.linalg.vector_norm(direction).clamp_min(1e-8)
  view=tensor(hint.get('view_outward',[0,1,0]));view=view/torch.linalg.vector_norm(view).clamp_min(1e-8)
  h=EDIT.index(hand);hand0=(base_p[:,bone_ids[h]]+base_p[:,end_ids[h]])*.5
  candidates=[];scores=[]
  for segment in segments:
   j=EDIT.index(segment);a=base_p[:,bone_ids[j]];v=base_p[:,end_ids[j]]-a
   projected=v-(v*view).sum(-1,keepdim=True)*view;diff=hand0-a;diff=diff-(diff*view).sum(-1,keepdim=True)*view
   # Choose a reachable point along the reference-associated leg surface.
   # This avoids forcing an anatomically impossible depth target at the ankle.
   shoulder=base_p[:,nn.index(hand.replace('hand','upperarm'))]
   elbow=base_p[:,nn.index(hand.replace('hand','lowerarm'))]
   wrist=base_p[:,nn.index(hand)]
   reach=(torch.linalg.vector_norm(elbow-shoulder,dim=-1)+torch.linalg.vector_norm(wrist-elbow,dim=-1))*.97
   palm_offset=hand0-wrist
   for t in torch.linspace(.1,.9,17,device=device):
    axis=a+t*v;candidate=axis+direction[None]*(radii[j]+radii[h]+.15)+v/torch.linalg.vector_norm(v,dim=-1,keepdim=True).clamp_min(1e-8)*float(hint.get('axis_offset_cm',0))
    error=hand0-axis;error=error-(error*view).sum(-1,keepdim=True)*view
    excess=(torch.linalg.vector_norm(candidate-palm_offset-shoulder,dim=-1)-reach).clamp_min(0)
    scores.append(error.square().sum(-1)+50*excess.square());candidates.append(candidate)
  weights=torch.softmax(-torch.stack(scores,1)/16,1)
  target=(torch.stack(candidates,1)*weights[:,:,None]).sum(1).detach()
  error=torch.linalg.vector_norm(hand0-target,dim=-1);active=active|(error>.25)
  surface.append((h,target,float(hint.get('weight',200.)),direction,((torch.stack(candidates,1)-direction[None,None]*(torch.stack([radii[EDIT.index(seg)]+radii[h]+.15 for seg in segments]).repeat_interleave(17)[None,:,None]))*weights[:,:,None]).sum(1).detach(),(torch.stack([radii[EDIT.index(seg)]+radii[h]+.15 for seg in segments]).repeat_interleave(17)[None,:]*weights).sum(1).detach()))
 support=torch.nn.functional.max_pool1d(active.float()[None,None],11,stride=1,padding=5).flatten().detach()
 def rotvec(v):
  x,y,z=v.unbind(-1);zero=torch.zeros_like(x)
  k=torch.stack([zero,-z,y,z,zero,-x,-y,x,zero],-1).reshape(*v.shape[:-1],3,3)
  a=(v.square().sum(-1)+1e-8).sqrt()[...,None,None]
  return torch.eye(3,device=device)+torch.sinc(a/torch.pi)*k+((1-a.cos())/a.square())*(k@k)
 raw=torch.zeros(n,len(ids),3,device=device,requires_grad=True);opt=torch.optim.Adam([raw],lr=.025)
 targetids=[nn.index(x) for x in ['hand_l','hand_r','foot_l','foot_r']]
 best=None;bestcost=float('inf')
 for step in range(iterations):
  opt.zero_grad();delta=torch.tanh(raw)*.58*support[:,None,None]
  local=lr.clone();local[:,ids]=lr[:,ids]@rotvec(delta)
  r,points=fk(local,locked);depth=collision(points)
  collision_loss=(float(d.get('collision_weight',3000)) if surface else 30)*(depth/5).square().sum(1).mean()
  prior=delta.square().sum((1,2)).mean()*2
  position=((points[:,targetids]-base_p[:,targetids])/10).square().sum((1,2)).mean()*.4
  temporal=5*(delta[1:]-delta[:-1]).square().sum((1,2)).mean()
  if n>2:temporal=temporal+40*(delta[2:]-2*delta[1:-1]+delta[:-2]).square().sum((1,2)).mean()
  surface_loss=delta.sum()*0
  for h,target,weight,direction,axis,clearance in surface:
   center=(points[:,bone_ids[h]]+points[:,end_ids[h]])*.5
   surface_loss=surface_loss+weight*((center-target)/5).square().sum(-1).mean()
   for fraction in [.15,.5,.85]:
    point=points[:,bone_ids[h]]*(1-fraction)+points[:,end_ids[h]]*fraction
    side=((point-axis)*direction[None]).sum(-1)
    surface_loss=surface_loss+10000*((clearance-side).clamp_min(0)/5).square().mean()
  image_loss=delta.sum()*0
  if d.get('image_observations'):
   image=d['image_observations'];cam=tensor(image['fit']);origin=tensor(image['origin']);focal=float(image['focal'])
   def project(x):
    v=x[:,:,[0,2,1]]*.01+origin[None,None];cc=v@cam[:9].reshape(3,3).T+cam[9:];return focal*cc[:,:,:2]/cc[:,:,2:3].clamp_min(.1)
   for side in ['l','r']:
    indices=[nn.index(x+'_'+side) for x in ['upperarm','lowerarm','hand']];uv=project(points[:,indices]);obs=tensor(image[side]);confidence=((obs[:,:,2]-.3)/.6).clamp(0,1)
    error=(uv[:,1:]-uv[:,:1])-(obs[:,1:,:2]-obs[:,:1,:2]);image_loss=image_loss+400*((error/60).square().sum(-1)*confidence[:,1:]*confidence[:,:1]).mean()
  loss=collision_loss+prior+position+temporal+surface_loss+image_loss
  if not torch.isfinite(loss):raise ValueError('Nonfinite contact solve')
  if float(loss.detach())<bestcost:bestcost=float(loss.detach());best=delta.detach().clone()
  loss.backward();opt.step()
  if step%40==0:print(json.dumps(dict(iteration=step,loss=float(loss.detach()),max_penetration_cm=float(depth.max().detach()))),flush=True)
 with torch.no_grad():
  local=lr.clone();local[:,ids]=lr[:,ids]@rotvec(best);r,points=fk(local,locked);after=collision(points)
  final=[]
  for name in EDIT:
   i=nn.index(name);pa=pp[i]
   final.append(r[:,i] if pa<0 else r[:,pa].transpose(-1,-2)@r[:,i])
  matrices=torch.stack(final,1).cpu().numpy();out=Rotation.from_matrix(matrices.reshape(-1,3,3)).as_quat().reshape(n,12,4)
  if d.get('freeze_legs',False):
   legs=[i for i,x in enumerate(EDIT) if x.startswith(('thigh','calf','foot'))];out[:,legs]=q[:,[names.index(EDIT[i]) for i in legs]]
  out[support.cpu().numpy()==0]=q[:,[names.index(x) for x in EDIT]][support.cpu().numpy()==0]
  angles=Rotation.from_quat(q[:,[names.index(x) for x in EDIT]].reshape(-1,4)).inv()*Rotation.from_quat(out.reshape(-1,4))
  if np.max(angles.magnitude())>np.deg2rad(59):raise ValueError('Correction exceeds native safety bound')
  if not surface and after.square().sum()>before.square().sum():raise ValueError('Contact score regressed')
  if surface:
   limit=float(d.get('max_proxy_penetration_cm',1.))
   if not np.isfinite(limit) or limit<=0:raise ValueError('Invalid reference contact penetration limit')
   if float(after.max())>limit:raise ValueError('Reference contact remains above penetration limit; no result published')
 details=dict(method='target LOD0 skin-derived limb capsules + source pose and temporal residual',geometry=geometry,pairs=[[EDIT[a],EDIT[b]] for a,b in pairs],proxy_max_before_cm=float(before.max()),proxy_max_after_cm=float(after.max()),proxy_squared_cost_before=float(before.square().sum()),proxy_squared_cost_after=float(after.square().sum()),active_frames=int(active.sum()),iterations=iterations,source_max_correction_degrees=float(np.rad2deg(angles.magnitude()).max()),seconds=time.perf_counter()-started,body_root_spine_and_finger_locals_preserved=True,hand_foot_global_orientations_preserved=not d.get('free_hand_orientation',False),caveat='Capsule proxy, not a proof of exact skin separation; render and inspect')
 details['reference_surface_hints']=d.get('surface_hints',[])
 details['freeze_legs']=bool(d.get('freeze_legs',False))
 if surface:
  details['surface_error_cm']={EDIT[h]:dict(mean=float(torch.linalg.vector_norm((points[:,bone_ids[h]]+points[:,end_ids[h]])*.5-target,dim=-1).mean()),maximum=float(torch.linalg.vector_norm((points[:,bone_ids[h]]+points[:,end_ids[h]])*.5-target,dim=-1).max())) for h,target,*_ in surface}
 result={k:d[k] for k in ['version','source_asset','source_guid','target_mesh','frames','fps']};result.update(edit_names=EDIT,rotations=out.reshape(-1).tolist())
 if d.get('debug_run'):result['debug_run']=d['debug_run']
 return result,details,dict(before=before.cpu().numpy(),after=after.cpu().numpy(),delta=best.cpu().numpy())

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--config',type=Path,required=True);p.add_argument('--iterations',type=int,default=400);p.add_argument('--output',type=Path);p.add_argument('--device',default='cuda');a=p.parse_args()
 d=json.loads(a.config.read_bytes());output=a.output or a.config.with_name('corrected.json')
 if output.exists():raise FileExistsError(output)
 result,details,evidence=solve(d,a.iterations,a.device)
 output.write_text(json.dumps(result),encoding='utf-8');output.with_suffix('.metrics.json').write_text(json.dumps(details,indent=2),encoding='utf-8');np.savez_compressed(output.with_suffix('.evidence.npz'),**evidence)
 print(json.dumps(details),flush=True)
if __name__=='__main__':main()
