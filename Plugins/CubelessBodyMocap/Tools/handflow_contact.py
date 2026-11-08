"""Target-aware, finger-only soft contact fit after HandFlow image refinement.

Native code supplies the SAME measured palm/terminal/FK calibration used by the
Manny bake. This module predicts its geometry; it does not apply that calibration
a second time to the output. Only shared-contract local finger quaternions change.
No joint limits, wrist movement, estimator mixing or model weights are involved.
"""
import time
import numpy as np


def geometry(context, device='cuda'):
    import torch
    from scipy.spatial.transform import Rotation
    def tensor(x):return torch.tensor(x, dtype=torch.float32, device=device)
    def matrix(q):
        q=np.asarray(q);return tensor(Rotation.from_quat(q.reshape(-1,4)).as_matrix().reshape(*q.shape[:-1],3,3))
    parents=context['parents'];sp=context['source_parents']
    local=matrix(context['source_local_q']);fixed=matrix(context['source_parent_q'])
    left=matrix(context['left_q']);right=matrix(context['right_q']);meta=matrix(context['meta_q'])
    wristq=matrix(context['wrists_q']);wristp=tensor(context['wrists_p']);pos=tensor(context['positions'])
    ends=tensor(context['ends']);widths=tensor(context['radii']).reshape(2,15)
    # Conservative surface allowance for the pads between the sparse samples.
    # This is a collision proxy radius, not a joint-angle or speed bound.
    radii=torch.cat([widths.repeat_interleave(3,1)*1.1,widths[:,[0,3,6,9]].repeat_interleave(4,1)*1.4],1)
    order=[];pending=set(range(38))
    while pending:
        ready=sorted(i for i in pending if parents[i]>=38 or parents[i] in order)
        if not ready:raise ValueError('Cyclic hand contact hierarchy')
        order.extend(ready);pending.difference_update(ready)
    def fk(rotations, return_bones=False, start=0):
        stop=start+len(rotations);wq=wristq[start:stop];wp=wristp[start:stop];pp=pos[start:stop]
        source=[];world=[]
        for i in range(30):
            source.append((source[sp[i]] if sp[i]>=0 else fixed[i])@local[i]@rotations[:,i])
            world.append(wq[:,i//15]@left[i//15]@source[i]@right[i])
        world.extend(wq[:,int(context['names'][i].endswith('_r'))]@meta[i-30] for i in range(30,38))
        world.extend([wq[:,0],wq[:,1]])
        points={38:wp[:,0],39:wp[:,1]}
        for i in order:points[i]=points[parents[i]]+(world[parents[i]]@pp[:,i,:,None]).squeeze(-1)
        if return_bones:return torch.stack(world[:38],1),torch.stack([points[i] for i in range(38)],1)
        centers=[]
        for i in range(30):
            v=(world[i]@ends[i,:,None]).squeeze(-1)
            centers.append(points[i][:,None,:]+v[:,None,:]*tensor([.2,.5,.8])[None,:,None])
        fingers=torch.stack(centers,1).reshape(len(rotations),2,45,3)
        palms=[]
        for side in range(2):
            for digit in [0,3,6,9]:
                root=points[side*15+digit];wrist=points[38+side]
                palms.append(wrist[:,None,:]+(root-wrist)[:,None,:]*tensor([.35,.5,.65,.8])[None,:,None])
        # Palm anchors are fixed by the preserved wrist/metacarpals. Including
        # their volume prevents solving finger/finger overlap by pushing a
        # finger through the opposite palm. Palm/palm pairs have no movable DOF.
        return torch.cat([fingers,torch.stack(palms,1).reshape(len(rotations),2,16,3)],2)
    return fk,radii


def refine(quaternions, weights, config, emit=None):
    import torch
    from scipy.spatial.transform import Rotation
    from smplx.lbs import batch_rodrigues
    from pathlib import Path
    started=time.perf_counter();context=config.get('hand_contact')
    if not context:
        return quaternions,dict(applied=False,reason=config.get('hand_contact_unavailable','Target contact calibration unavailable'))
    if context.get('version')!=1:raise ValueError('Unsupported hand contact calibration')
    if context['strength']!=1 or not np.all(weights==1):
        return quaternions,dict(applied=False,reason='Contact fit requires full finger influence on both hands')
    n=len(quaternions);fk,radii=geometry(context)
    base=torch.tensor(Rotation.from_quat(quaternions.reshape(-1,4)).as_matrix().reshape(n,30,3,3),dtype=torch.float32,device='cuda')
    margin=radii[0,:,None]+radii[1,None,:]
    movable=torch.ones_like(margin);movable[45:,45:]=0
    def collision(rot,start=0):
        points=fk(rot,start=start)
        return (torch.relu(margin-torch.cdist(points[:,0],points[:,1])).square()*movable).sum((1,2))
    chunk=256
    with torch.no_grad():initial=torch.cat([collision(base[i:i+chunk],i) for i in range(0,n,chunk)])
    if not (initial>0).any():return quaternions,dict(applied=False,reason='No opposing finger proxy overlap')
    # A short symmetric support neighborhood avoids abrupt contact entry/exit.
    # It is measured in seconds, never in test-clip frame numbers.
    radius=max(1,round(.1*config['fps']));mask=torch.nn.functional.max_pool1d((initial>0).float()[None,None],2*radius+1,stride=1,padding=radius).flatten()
    delta=torch.zeros(n,30,3,device='cuda',requires_grad=True);optimizer=torch.optim.Adam([delta],lr=.015)
    for iteration in range(120):
        optimizer.zero_grad()
        for i in range(0,n,chunk):
            end=min(i+chunk,n);dd=delta[i:end]*mask[i:end,None,None]
            rot=base[i:end]@batch_rodrigues(dd.reshape(-1,3)).reshape(end-i,30,3,3)
            (collision(rot,i).sum()/n).backward()
        dd=delta*mask[:,None,None]
        prior=.5*dd.square().sum((1,2)).mean()
        temporal=12*(dd[1:]-dd[:-1]).square().sum((1,2)).mean() if n>1 else dd.sum()*0
        (prior+temporal).backward();optimizer.step()
        if emit and iteration%30==0:emit(iteration,120)
    with torch.no_grad():
        dd=delta*mask[:,None,None];rot=base@batch_rodrigues(dd.reshape(-1,3)).reshape(n,30,3,3);final=torch.cat([collision(rot[i:i+chunk],i) for i in range(0,n,chunk)]);angles=dd.norm(dim=-1).cpu().numpy()
        if not torch.isfinite(rot).all() or final.sum()>initial.sum():raise ValueError('Hand contact fit failed its finite/cost check')
        result=Rotation.from_matrix(rot.cpu().numpy().reshape(-1,3,3)).as_quat().reshape(n,2,15,4)
        # Guarantee exact unchanged contract outside contact support, avoiding
        # unnecessary matrix/quaternion round-trip noise.
        result[mask.cpu().numpy()==0]=quaternions[mask.cpu().numpy()==0]
    details=dict(applied=True,method='target-derived finger/palm sphere contact + pose prior + temporal residual',
                 iterations=120,proxy_cost_before=float(initial.sum()),proxy_cost_after=float(final.sum()),
                 max_correction_degrees=float(np.degrees(angles).max()),support_frames=np.flatnonzero(mask.cpu().numpy()).tolist(),
                 seconds=time.perf_counter()-started,joint_limits=False,wrists_changed=False,
                 proxy_caveat='Finger surface approximation; not a guarantee of mesh separation or correct hidden gesture')
    np.savez_compressed(Path(config['work'])/'contact_evidence.npz',before_ue=quaternions,after_ue=result,
                        cost_before=initial.cpu().numpy(),cost_after=final.cpu().numpy(),correction_radians=angles)
    return result,details
