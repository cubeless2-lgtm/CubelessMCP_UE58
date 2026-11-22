"""Original-video priority, target-skin hand contact and short-window SO(3) cleanup.

Inputs are a clean target-skeleton export and timestamped source observations.
Contact intervals are explicit, reviewed original-video hints.
No Unreal objects or asset mutations occur in this worker.
"""
import argparse,json,time
from pathlib import Path
import numpy as np
from scipy.ndimage import gaussian_filter1d
from scipy.spatial.transform import Rotation
from mocap_observations import observations,resample,fk_numpy,MAPPING,IK,digest


def repair_contact_observations(keys,times,width,hints):
    """Reject short occlusion hallucinations during reviewed two-hand contact.
    Raw measurements remain untouched. A weak hand suddenly tracked at the hip
    while the other grips the same garment is interpolated between reliable
    bracketing observations, with reduced confidence and an explicit receipt.
    """
    from scipy.ndimage import binary_dilation
    out=np.array(keys,float,copy=True);repairs={};separation=np.linalg.norm(keys[:,9,:2]-keys[:,10,:2],axis=-1)
    for side,wrist,other,mcp,other_mcp,start,elbow in [('l',9,10,100,121,91,7),('r',10,9,121,100,112,8)]:
        h=hints.get(side,{});curve=h.get('weight_keys',[])
        if not curve:continue
        active=np.interp(times,[x[0] for x in curve],[x[1] for x in curve])>.5
        score=np.minimum(keys[:,wrist,2],keys[:,mcp,2]);other_score=np.minimum(keys[:,other,2],keys[:,other_mcp,2])
        bad=active&(separation>max(150,width*.10))&(other_score>.7)&((score<.65)|(score<other_score-.15))
        bad=binary_dilation(bad,iterations=2)&active
        accepted=np.zeros(len(keys),bool);edges=np.diff(np.r_[False,bad,False].astype(int));begins=np.flatnonzero(edges==1);ends=np.flatnonzero(edges==-1)
        for a,z in zip(begins,ends):
            if a>0 and z<len(keys) and times[z]-times[a-1]<=1.:
                accepted[a:z]=True
        if not accepted.any():continue
        inds=[elbow,wrist]+list(range(start,start+21));good=np.flatnonzero(~accepted);bad_rows=np.flatnonzero(accepted)
        for j in inds:
            for axis in range(2):out[accepted,j,axis]=np.interp(times[accepted],times[good],keys[good,j,axis])
            out[accepted,j,2]=np.minimum(out[accepted,j,2],.45)
        repairs[side]=dict(frames=bad_rows.tolist(),method='short low-confidence hand separation during reviewed shared-leg garment contact; bracket interpolation',max_gap_seconds=1.,confidence_cap=.45)
    return out,repairs


def smooth_quaternions(q,sigma=1.5,max_degrees=8.,pin_first=True):
    """Sign-invariant short-window averaging, bounded in physical rotation space."""
    q=np.asarray(q,float).copy()
    if q.ndim!=3 or q.shape[-1]!=4 or not np.isfinite(q).all() or not np.allclose(np.linalg.norm(q,axis=-1),1,atol=1e-4):
        raise ValueError('Need normalized frame x bone x xyzw quaternions')
    if not 0<sigma<=4 or not 0<max_degrees<=20:raise ValueError('Invalid temporal smoothing bounds')
    for i in range(1,len(q)):
        flip=np.sum(q[i-1]*q[i],axis=-1)<0;q[i,flip]*=-1
    average=gaussian_filter1d(q,sigma,axis=0,mode='nearest');average/=np.linalg.norm(average,axis=-1,keepdims=True)
    original=Rotation.from_quat(q.reshape(-1,4));delta=(original.inv()*Rotation.from_quat(average.reshape(-1,4))).as_rotvec()
    length=np.linalg.norm(delta,axis=-1);delta*=np.minimum(1,np.deg2rad(max_degrees)/np.maximum(length,1e-9))[:,None]
    result=(original*Rotation.from_rotvec(delta)).as_quat().reshape(q.shape)
    if pin_first:result[0]=q[0]
    return result


def rotation_metrics(q):
    n,b=q.shape[:2];r=Rotation.from_quat(q.reshape(-1,4)).as_matrix().reshape(n,b,3,3)
    velocity=Rotation.from_matrix((r[:-1].transpose(0,1,3,2)@r[1:]).reshape(-1,3,3)).as_rotvec().reshape(n-1,b,3)
    acceleration=np.diff(velocity,axis=0)
    return dict(max_step_degrees=float(np.rad2deg(np.linalg.norm(velocity,axis=-1)).max()),
                angular_acceleration_rms_degrees=float(np.rad2deg(np.sqrt(np.mean(acceleration**2)))),
                angular_step_p95_degrees=float(np.rad2deg(np.quantile(np.linalg.norm(velocity,axis=-1),.95))))


def solve(job):
    import torch
    started=time.perf_counter();d=json.loads(Path(job['request']).read_text());n,b=d['frames'],len(d['names']);names=d['names'];parents=d['parents'];fps=float(d['fps'])
    if d.get('version')!=1 or not 3<=n<=3600 or len(set(names))!=b or len(parents)!=b or any(x>=i or x< -1 for i,x in enumerate(parents)):
        raise ValueError('Invalid target hierarchy')
    original_q=np.array(d['local_q']).reshape(n,b,4);p=np.array(d['local_p']).reshape(n,b,3);sc=np.array(d['local_s']).reshape(n,b,3)
    if not np.isfinite(p).all() or not np.allclose(sc,1):raise ValueError('Need finite unit-scale transforms')
    sigma=float(job.get('smoothing_seconds',.05))*fps
    q=smooth_quaternions(original_q,sigma=sigma,max_degrees=8)
    finger_ids=[i for i,x in enumerate(names) if x.startswith(('thumb_','index_','middle_','ring_','pinky_'))]
    q[:,finger_ids]=smooth_quaternions(original_q[:,finger_ids],sigma=sigma,max_degrees=12,pin_first=False)
    # IK helpers are rebuilt after all deforming tracks have been fitted.
    for i,x in enumerate(names):
        if x.startswith('ik_'):q[:,i]=original_q[:,i]
    baseline=dict(d,local_q=q.reshape(-1).tolist());baseG,baseP=fk_numpy(baseline);oldG,oldP=fk_numpy(d)
    sd,st,sk=observations(job['source_observations']);timeline=np.arange(n)/fps
    if timeline[-1]>st[-1]+.05:raise ValueError('Source observations do not cover the animation')
    raw=resample(st,sk,timeline)
    raw,observation_repairs=repair_contact_observations(raw,timeline,sd['frames'][0]['width'],job.get('contact_hints',{}))
    camera=json.loads(Path(job['source_camera']).read_text());cam=np.array(camera['world_to_camera']);origin=np.array(camera['camera'][7:]);focal=float(camera['camera'][0]);center=np.array(camera['camera'][1:3])/2
    raw[:,:,:2]*=np.array(camera['camera'][1:3])/[sd['frames'][0]['width'],sd['frames'][0]['height']]
    observed=raw.copy();observed[:,:,:2]=gaussian_filter1d(observed[:,:,:2],1.2,axis=0)
    device=job.get('device','cuda')
    def ten(x):return torch.as_tensor(x,dtype=torch.float32,device=device)
    C,O,ctr=ten(cam),ten(origin),ten(center)
    def project(P):
        X=(P[:,:,[0,2,1]]/100+O)@C[:9].reshape(3,3).T+C[9:]
        return focal*X[:,:,:2]/X[:,:,2:3].clamp_min(.1)+ctr
    cameraP=ten((-cam[:9].reshape(3,3).T@cam[9:]-origin)[[0,2,1]]*100)
    editable=['spine_01','spine_03','spine_05','clavicle_l','clavicle_r']+[
        x+'_'+s for s in ['l','r'] for x in ['upperarm','lowerarm','hand']]
    mapping=dict(MAPPING);map_names=list(mapping);key_ids=list(mapping.values());map_full=[names.index(x) for x in map_names]
    need=set()
    for x in editable+map_names+['head']+[x+'_'+s for s in ['l','r'] for x in ['thigh','calf','foot','ball','middle_03']]:
        i=names.index(x)
        while i>=0:need.add(i);i=parents[i]
    keep=sorted(need);nn=[names[i] for i in keep];lookup={x:i for i,x in enumerate(keep)};pa=[lookup.get(parents[i],-1) for i in keep];ids=[nn.index(x) for x in editable];mi=[nn.index(x) for x in map_names]
    lr=ten(Rotation.from_quat(q[:,keep].reshape(-1,4)).as_matrix().reshape(n,len(keep),3,3));lp=ten(p[:,keep]);GP=ten(baseP[:,keep]);GR=ten(baseG[:,keep]);ROW=torch.arange(n,device=device)
    def fk(local):
        rr=[];pp=[]
        for i,parent in enumerate(pa):
            rr.append(local[:,i] if parent<0 else rr[parent]@local[:,i]);pp.append(lp[:,i] if parent<0 else pp[parent]+(rr[parent]@lp[:,i,:,None])[...,0])
        return torch.stack(rr,1),torch.stack(pp,1)
    def rot(v):
        x,y,z=v.unbind(-1);zz=torch.zeros_like(x);K=torch.stack([zz,-z,y,z,zz,-x,-y,x,zz],-1).reshape(*v.shape[:-1],3,3);a=(v.square().sum(-1)+1e-8).sqrt()[...,None,None]
        return torch.eye(3,device=device)+torch.sinc(a/torch.pi)*K+((1-a.cos())/a.square())*(K@K)
    uv=ten(observed[:,key_ids,:2]);confidence=ten(np.clip((observed[:,key_ids,2]-.2)/.65,0,1));weights=ten([2 if x.startswith('hand') else .8 if x.startswith(('middle','index','pinky')) else 1 if x.startswith(('upperarm','lowerarm')) else .12 for x in map_names])[None]
    confidence*=weights
    segment_end={'calf_l':'foot_l','calf_r':'foot_r','thigh_l':'calf_l','thigh_r':'calf_r'}
    segments=job.get('contact_segments',list(segment_end));assert segments and all(x in segment_end for x in segments)
    skin=[];normals=[];segment_uv=[];world_lengths=[]
    for name in segments:
        i,j=nn.index(name),nn.index(segment_end[name]);points=np.array(d['skin_samples'][name]).reshape(-1,3)
        if not 20<=len(points)<=20000 or not np.isfinite(points).all():raise ValueError('Invalid target skin samples')
        points=points[::max(1,len(points)//500)]
        local_axis=(baseG[0,names.index(name)].T@(baseP[0,names.index(segment_end[name])]-baseP[0,names.index(name)]));length=np.linalg.norm(local_axis);axis=local_axis/length;along=points@axis
        valid=(along>.07*length)&(along<.98*length);points=points[valid];along=along[valid]
        radial=points-along[:,None]*axis;normal=radial/np.maximum(np.linalg.norm(radial,axis=-1,keepdims=True),1e-6)
        world=GR[:,i,None]@ten(points)[None,:,:,None];world=world[...,0]+GP[:,i,None]
        N=(GR[:,i,None]@ten(normal)[None,:,:,None])[...,0];skin.append(world);normals.append(N);segment_uv.append(project(world));world_lengths.append(length)
    plans=[]
    for side in ['l','r']:
        wi,fi=map_names.index('hand_'+side),map_names.index('middle_01_'+side);palm_uv=(uv[:,wi]+uv[:,fi])*.5
        h,m=nn.index('hand_'+side),nn.index('middle_01_'+side);palm_offset=(GP[:,h]+GP[:,m])*.5-GP[:,h]
        shoulder=GP[:,nn.index('upperarm_'+side)];elbow=GP[:,nn.index('lowerarm_'+side)];reach=(torch.linalg.vector_norm(elbow-shoulder,dim=-1)+torch.linalg.vector_norm(GP[:,h]-elbow,dim=-1))*.99
        costs=[];positions=[];directions=[];distances=[]
        for name,world,N,projection in zip(segments,skin,normals,segment_uv):
            target=world+N*2.1;screen=torch.linalg.vector_norm(projection-palm_uv[:,None],dim=-1)
            facing=((cameraP[None,None]-world)*N).sum(-1)>0
            excess=(torch.linalg.vector_norm(target-palm_offset[:,None]-shoulder[:,None],dim=-1)-reach[:,None]-2).clamp_min(0)
            cost=(screen/25).square()+3*(excess/5).square()+(~facing).float()*100
            best=cost.argmin(-1);costs.append(cost[ROW,best]);positions.append(target[ROW,best]);directions.append(N[ROW,best]);distances.append(screen[ROW,best])
        # Smooth segment identity with a two-state (shin/thigh) temporal prior.
        scores=torch.stack(costs,1).cpu().numpy();K=len(segments);dp=scores.copy();prev=np.zeros((n,K),int)
        hints=job.get('contact_hints',{}).get(side,{})
        for interval in hints.get('segments',[]):
            mask=(timeline>=interval['start'])&(timeline<interval['end'])
            for i,name in enumerate(segments):
                if name not in interval['allowed']:dp[mask,i]+=100
        local_cost=dp.copy()
        for f in range(1,n):
            transition=dp[f-1,:,None]+np.where(np.eye(K),0,3)
            prev[f]=transition.argmin(0);dp[f]+=transition.min(0)
        chosen=np.zeros(n,int);chosen[-1]=dp[-1].argmin()
        for f in range(n-1,0,-1):chosen[f-1]=prev[f,chosen[f]]
        selected=torch.tensor(chosen,device=device);surface_target=torch.stack(positions,1)[ROW,selected].cpu().numpy();normal=torch.stack(directions,1)[ROW,selected].cpu().numpy();screen=torch.stack(distances,1)[ROW,selected]
        # A hand pulling fabric need not lie on the leg silhouette. Keep its
        # observed image ray, using the visible skin to resolve DEPTH only.
        # Moving it to the nearest 3D skin vertex would overwrite source X/Y.
        camera_xyz=(surface_target[:,[0,2,1]]/100+origin)@cam[:9].reshape(3,3).T+cam[9:]
        depth=gaussian_filter1d(camera_xyz[:,2],3.,axis=0)
        palm_image=palm_uv.cpu().numpy();camera_target=np.c_[(palm_image-center)/focal*depth[:,None],depth]
        world=(camera_target-cam[9:])@cam[:9].reshape(3,3)-origin
        target=world[:,[0,2,1]]*100
        normal=gaussian_filter1d(normal,1.5,axis=0);normal/=np.maximum(np.linalg.norm(normal,axis=-1,keepdims=True),1e-6)
        strength=((55-screen)/30).clamp(0,1).cpu().numpy()
        if hints.get('weight_keys'):strength=np.interp(timeline,[x[0] for x in hints['weight_keys']],[x[1] for x in hints['weight_keys']])
        strength=gaussian_filter1d(strength,1.5);strength[:min(n,12)]=np.maximum(strength[:min(n,12)],.98)
        visible=np.clip((np.minimum(observed[:,mapping['hand_'+side],2],observed[:,mapping['middle_01_'+side],2])-.25)/.6,0,1)
        plans.append(dict(side=side,hand=h,middle=m,target=ten(target),normal=ten(normal),strength=ten(strength),visible=ten(visible),segments=[segments[i] for i in chosen],screen_distance=screen.cpu().numpy()))
    raw_delta=torch.zeros(n,len(ids),3,device=device,requires_grad=True);limit=ten([np.deg2rad(16 if name.startswith('spine') else 20 if name.startswith('clavicle') else 75)/np.sqrt(3) for name in editable])[None,:,None]
    def evaluate(raw_delta):
        delta=torch.tanh(raw_delta)*limit;local=lr.clone();local[:,ids]=lr[:,ids]@rot(delta);R,P=fk(local);return local,R,P,delta
    def source_cost(P):
        error=project(P[:,mi])-uv;loss=torch.nn.functional.huber_loss(error/20,torch.zeros_like(error),delta=2,reduction='none').sum(-1)
        frame=(loss*confidence).sum(-1)/confidence.sum(-1).clamp_min(1)
        return frame.mean()+frame[0],torch.linalg.vector_norm(error,dim=-1)
    def contact_cost(P):
        costs=[];errors=[];sides=[]
        for plan in plans:
            palm=(P[:,plan['hand']]+P[:,plan['middle']])*.5;difference=palm-plan['target'];error=torch.linalg.vector_norm(difference,dim=-1)
            normal=(difference*plan['normal']).sum(-1)
            weighted=(error/4).square()*plan['strength'];costs.append(weighted.mean()+weighted[0]*2+torch.topk(weighted,max(1,n//20)).values.mean()*.25)
            sides.append(((-normal-.5).clamp_min(0)/3).square().mul(plan['strength']).mean());errors.append(error)
            # Releasing fabric removes contact, not the evidence that a visible
            # hand is in front of the leg. Preserve a one-sided depth bound even
            # after contact strength reaches zero; allow free motion toward view.
            actual_camera=(palm[:,[0,2,1]]/100+O)@C[:9].reshape(3,3).T+C[9:]
            target_camera=(plan['target'][:,[0,2,1]]/100+O)@C[:9].reshape(3,3).T+C[9:]
            behind=((actual_camera[:,2]-target_camera[:,2])*100-1.).clamp_min(0)
            visibility=(behind/3).square()*plan['visible']
            costs.append(visibility.mean()*4+torch.topk(visibility,max(1,n//20)).values.mean())
        return torch.stack(costs).sum()+torch.stack(sides).sum()*2,torch.stack(errors,1)
    optimizer=torch.optim.Adam([raw_delta],lr=.018);best=None;best_cost=float('inf');iterations=int(job.get('iterations',1600))
    for step in range(iterations):
        optimizer.zero_grad();local,R,P,delta=evaluate(raw_delta);image_loss,_=source_cost(P);touch,contact=contact_cost(P)
        vel=delta[1:]-delta[:-1];acc=delta[2:]-2*delta[1:-1]+delta[:-2]
        # A short-window smooth baseline plus acceleration/jerk constraints.
        motion=15*vel.square().sum((1,2)).mean()+180*acc.square().sum((1,2)).mean()
        if n>3:motion+=500*(acc[1:]-acc[:-1]).square().sum((1,2)).mean()
        wrist_positions=P[:,[nn.index('hand_l'),nn.index('hand_r')]]
        position_acc=wrist_positions[2:]-2*wrist_positions[1:-1]+wrist_positions[:-2]
        motion+=2*(position_acc/.3).square().mean()
        if n>3:motion+=3*((position_acc[1:]-position_acc[:-1])/.2).square().mean()
        prior=.4*delta.square().sum((1,2)).mean()
        cost=float(job.get('source_weight',30))*image_loss+float(job.get('contact_weight',20))*touch+motion+prior
        if not torch.isfinite(cost):raise ValueError('Nonfinite source-priority solve')
        if float(cost.detach())<best_cost:best_cost=float(cost.detach());best=raw_delta.detach().clone()
        cost.backward();optimizer.step()
        if step%250==0:print(json.dumps(dict(step=step,source=float(image_loss.detach()),first_contact_cm=contact[0].detach().cpu().tolist(),max_contact_cm=float(contact.detach().max()))),flush=True)
    with torch.no_grad():local,R,P,delta=evaluate(best)
    final_q=q.copy();final_q[:,[names.index(x) for x in editable]]=Rotation.from_matrix(local[:,ids].cpu().numpy().reshape(-1,3,3)).as_quat().reshape(n,len(ids),4)
    # Remove remaining high-frequency arm changes without averaging frame zero.
    final_q=smooth_quaternions(final_q,sigma=1.25,max_degrees=4.,pin_first=True)
    out=dict(d,local_q=final_q.reshape(-1).tolist());G,PP=fk_numpy(out)
    head=names.index('head');head_parent=parents[head];final_q[:,head]=Rotation.from_matrix(G[:,head_parent].transpose(0,2,1)@baseG[:,head]).as_quat();out['local_q']=final_q.reshape(-1).tolist();G,PP=fk_numpy(out)
    final_p=p.copy()
    for name,target in sorted(IK.items(),key=lambda pair:names.index(pair[0])):
        i,j=names.index(name),names.index(target);parent=parents[i];final_q[:,i]=Rotation.from_matrix(G[:,parent].transpose(0,2,1)@G[:,j]).as_quat();final_p[:,i]=(G[:,parent].transpose(0,2,1)@(PP[:,j]-PP[:,parent])[...,None])[...,0];G[:,i]=G[:,j];PP[:,i]=PP[:,j]
    changed=[x for x in names if not x.startswith('ik_')]+list(IK)
    out.update(local_q=final_q.reshape(-1).tolist(),local_p=final_p.reshape(-1).tolist(),edit_names=changed,position_edit_names=list(IK),algorithm='source_priority_contact_v1')
    full_finalP=ten(PP[:,keep]);old_reducedP=ten(oldP[:,keep])
    with torch.no_grad():_,e_before=source_cost(old_reducedP);_,e_after=source_cost(full_finalP);_,gap_before=contact_cost(old_reducedP);_,gap_after=contact_cost(full_finalP)
    weights=confidence.cpu().numpy()
    def stats(e):
        e=e.cpu().numpy();return dict(mean_px=float((e*weights).sum()/weights.sum()),first_px=float((e[0]*weights[0]).sum()/weights[0].sum()),joint_px={name:float((e[:,i]*weights[:,i]).sum()/max(weights[:,i].sum(),1e-8)) for i,name in enumerate(map_names)})
    deform=[i for i,name in enumerate(names) if not name.startswith('ik_')];wrists=[names.index('hand_'+s) for s in ['l','r']];finger=[i for i,name in enumerate(names) if name.startswith(('thumb_','index_','middle_','ring_','pinky_'))]
    def position_jitter(points):return float(np.sqrt(np.mean((points-gaussian_filter1d(points,2,axis=0))**2)))
    temporal=dict(rotation_before=rotation_metrics(original_q[:,deform]),rotation_after=rotation_metrics(final_q[:,deform]),
                  finger_before=rotation_metrics(original_q[:,finger]),finger_after=rotation_metrics(final_q[:,finger]),
                  wrist_high_frequency_rms_cm_before_after=[position_jitter(oldP[:,wrists]),position_jitter(PP[:,wrists])])
    contacts={}
    for i,plan in enumerate(plans):
        active=plan['strength'].cpu().numpy()>.5;before=gap_before[:,i].cpu().numpy();after=gap_after[:,i].cpu().numpy()
        contacts[plan['side']]=dict(first_target_error_cm_before_after=[float(before[0]),float(after[0])],mean_active_error_cm_before_after=[float(before[active].mean()),float(after[active].mean())],p95_active_error_cm=float(np.quantile(after[active],.95)),max_active_error_cm=float(after[active].max()),active_frames=int(active.sum()),segments=plan['segments'])
    report=dict(version=1,quality_contract='source_priority_contact',contact_target_definition='Original observed palm image ray at visible target-leg surface depth; observed fabric/silhouette gap retained',source_before=stats(e_before),source_after=stats(e_after),
        source_contact_authority=True,source_observation_repairs=observation_repairs,contact=contacts,temporal=temporal,frames=n,fps=fps,seconds=time.perf_counter()-started,
        source_observations_sha256=digest(job['source_observations']),request_sha256=digest(job['request']),worker_sha256=digest(__file__),source_guid=d['source_guid'])
    report['first_wrist_error_px_before_after']={s:[float(e_before[0,map_names.index('hand_'+s)]),float(e_after[0,map_names.index('hand_'+s)])] for s in ['l','r']}
    report['max_deforming_step_degrees_before_after']=[temporal['rotation_before']['max_step_degrees'],temporal['rotation_after']['max_step_degrees']]
    report['numerical_acceptance']=bool(contacts['l']['first_target_error_cm_before_after'][1]<3.5 and report['source_after']['mean_px']<=report['source_before']['mean_px']+5 and temporal['rotation_after']['angular_acceleration_rms_degrees']<temporal['rotation_before']['angular_acceleration_rms_degrees'])
    out['source_contact_report']=report
    evidence=dict(before=oldP[:,map_full],after=PP[:,map_full],source_uv=observed[:,key_ids,:2],source_before=project(ten(oldP[:,map_full])).cpu().numpy(),source_after=project(ten(PP[:,map_full])).cpu().numpy(),names=np.array(map_names),times=timeline,contact_before=gap_before.cpu().numpy(),contact_after=gap_after.cpu().numpy(),contact_targets=np.stack([plan['target'].cpu().numpy() for plan in plans],1),contact_strength=np.stack([plan['strength'].cpu().numpy() for plan in plans],1))
    return out,report,evidence


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--job',required=True);p.add_argument('--output',required=True);a=p.parse_args();output=Path(a.output)
    if output.exists():raise FileExistsError(output)
    result,report,evidence=solve(json.loads(Path(a.job).read_text()));output.write_text(json.dumps(result,allow_nan=False));output.with_suffix('.report.json').write_text(json.dumps(report,indent=2,allow_nan=False));np.savez_compressed(output.with_suffix('.evidence.npz'),**evidence);print(json.dumps(report))


if __name__=='__main__':main()
