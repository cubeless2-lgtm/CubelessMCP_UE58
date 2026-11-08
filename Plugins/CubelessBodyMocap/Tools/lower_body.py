"""Optional target-space video evidence fit. No angle bounds or model mixing.

Pelvis and leg rotations may change. Spine_01 compensates to preserve the
original upper-body world pose; every hand-local track remains untouched.
"""
import copy
import time
import numpy as np
from scipy.ndimage import uniform_filter1d, minimum_filter1d
from temporal_solver import temporal_least_squares
from scipy.sparse import lil_matrix
from scipy.spatial.transform import Rotation as R

VERSION = 'target_video_legs_only_v7_contact_transitions'
NAMES = ['pelvis','thigh_l','calf_l','foot_l','thigh_r','calf_r','foot_r']

def globals_from_local(local, parents):
    n,b,_=local.shape
    q=R.from_quat(local[:,:,3:].reshape(-1,4)).as_matrix().reshape(n,b,3,3)
    p=local[:,:,:3].copy()
    for j,parent in enumerate(parents):
        if parent>=0:
            p[:,j]=p[:,parent]+np.einsum('fij,fj->fi',q[:,parent],p[:,j])
            q[:,j]=q[:,parent]@q[:,j]
    return p,q

def fit_lower_body(context, keypoints, camera_values, progress=lambda *a:None):
    start=time.perf_counter();c=copy.deepcopy(context);n=int(c['frames'])
    skeleton=c['lower_body'];names=skeleton['bones'];parents=skeleton['parents']
    local=np.array(skeleton['local']).reshape(n,len(names),7)
    if not np.isfinite(local).all() or not np.isfinite(keypoints).all():raise ValueError('Nonfinite lower-body input')
    indices=[names.index(name) for name in NAMES];spine=names.index('spine_01')
    p0,q0=globals_from_local(local,parents);p=p0[:,indices];q=q0[:,indices]
    lp=local[:,indices,:3];lq=R.from_quat(local[:,indices,3:].reshape(-1,4)).as_matrix().reshape(n,7,3,3)
    for j,parent in enumerate([-1,0,1,2,0,4,5]):
        if parent>=0 and parents[indices[j]]!=indices[parent]:raise ValueError('Unsupported lower-body hierarchy')
    if parents[spine]!=indices[0]:raise ValueError('Spine compensation requires spine_01 parent pelvis')
    _,_,_,pitch,roll=camera_values[:5]
    cp,sp=np.cos(-pitch),np.sin(-pitch);cr,sr=np.cos(roll),np.sin(roll)
    camera=np.array([[cr,cp*sr,-sp*sr],[0,-sp,-cp],[sr,-cp*cr,sp*cr]])
    obs=keypoints[:,[12,13,14,9,10,11],:2]
    conf=np.clip((keypoints[:,[12,13,14,9,10,11],2]-.35)/.45,0,1)
    lengths=[];straight=[]
    for j in [0,3]:
        u=obs[:,j]-obs[:,j+1];w=obs[:,j+2]-obs[:,j+1]
        lu=np.linalg.norm(u,axis=1);lw=np.linalg.norm(w,axis=1)
        valid=(conf[:,j:j+3]>.5).all(1)&(lu>1)&(lw>1)
        lengths.extend((lu+lw)[valid].tolist())
        bend=180-np.degrees(np.arccos(np.clip((u*w).sum(-1)/np.maximum(lu*lw,1),-1,1)))
        straight.append(np.clip((30-bend)/20,0,1)*conf[:,j:j+3].min(1))
    if len(lengths)<max(12,n//10):raise ValueError('Too few confident leg observations; keep the original body result')
    length=np.mean([np.linalg.norm(lp[:,2],axis=1).mean()+np.linalg.norm(lp[:,3],axis=1).mean(),np.linalg.norm(lp[:,5],axis=1).mean()+np.linalg.norm(lp[:,6],axis=1).mean()])
    scale=float(np.median(lengths)/length)
    # Relative foot-centred image evidence avoids camera/root re-estimation and
    # accounts for target limb length. This is an approximate weak-perspective fit.
    anchor=p[:,[3,6]].mean(1)@camera[:2].T
    target=(obs-obs[:,[2,5]].mean(1)[:,None])/scale+anchor[:,None]
    anchor_conf=conf[:,[2,5]].min(1);conf*=anchor_conf[:,None]
    straight=uniform_filter1d(np.array(straight).T,7,axis=0,mode='nearest')*anchor_conf[:,None]
    # An edge-on pelvis makes the two legs overlap in the image. Pixel fit
    # cannot recover their depth ordering; anchor each joint's relative camera
    # depth to the original body estimate instead of anchoring only the pelvis.
    hip_axis=p[:,1]-p[:,4]
    side_view=np.abs(hip_axis@camera[2])/np.maximum(np.linalg.norm(hip_axis,axis=1),1e-6)
    side_view=uniform_filter1d(side_view,7,mode='nearest')
    depth_weight=.15+3.*side_view**2
    relative_depth=(p[:,1:]-p[:,:1])@camera[2]
    original_separation=(p[:,[1,2,3]]-p[:,[4,5,6]])@camera[2]
    order_sign=original_separation/np.sqrt(original_separation**2+1.)
    order_confidence=np.abs(original_separation)/(np.abs(original_separation)+5.)
    order_margin=np.minimum(np.abs(original_separation),3.)
    # A nearly straight 2D projection is weak evidence of a straight 3D knee
    # when viewed from the side/under overlap. This is not an angle constraint.
    straight*=1-side_view[:,None]**2
    foot=p[:,[3,6]];speed=np.linalg.norm(np.diff(foot[:,:,:2],axis=0,prepend=foot[:1,:,:2]),axis=-1)*30
    stance=np.clip((16-(foot[:,:,2]-float(c['ground_z_cm'])))/6,0,1)*np.clip((35-speed)/20,0,1)
    stance=uniform_filter1d(stance,7,axis=0,mode='nearest');links=np.minimum(stance[:-1],stance[1:])
    # Existing ankle heights may already place the target shoe below the floor.
    # Measure actual weighted skin, rather than perpetuating that height error.
    # This is a soft positional observation, never a joint-angle clamp.
    ground=float(c['ground_z_cm']);clearance=[];shoe_floor_height=[];surface_support=[];sole_offsets=[]
    used=[names.index(name) for name in c['bones']]
    for side in range(2):
        vertices=c['vertices'][side];xyz=np.zeros((n,len(vertices),3))
        for bone,j in enumerate(used):
            ids=[];weights=[];points=[]
            for vi,vertex in enumerate(vertices):
                for bi,w,point in vertex['weights']:
                    if bi==bone:ids.append(vi);weights.append(w);points.append(point)
            if ids:xyz[:,ids]+=(np.einsum('fij,vj->fvi',q0[:,j],points)+p0[:,j,None])*np.array(weights)[None,:,None]
        minimum_z=xyz[:,:,2].min(1)
        rest=np.array([v['rest'] for v in vertices])
        sole=rest[:,2]<rest[:,2].min()+1.5
        sole_offsets.append(xyz[:,sole,:2].mean(1)-foot[:,side,:2])
        clearance.append(.15*np.logaddexp(0.,(ground+.05-minimum_z)/.15))
        shoe_floor_height.append(foot[:,side,2]+ground+.05-minimum_z)
        vertical_speed=np.abs(np.diff(foot[:,side,2],prepend=foot[0,side,2]))*30
        # Horizontal drift in the input is not evidence that a foot left the
        # ground. Use sole height and vertical motion for positional support.
        surface_support.append(np.clip((2.5-(minimum_z-ground))/2.5,0,1)*np.clip((35-vertical_speed)/20,0,1))
    lift=uniform_filter1d(np.array(clearance).T,5,axis=0,mode='nearest')
    shoe_floor_height=np.array(shoe_floor_height).T
    target_height=foot[:,:,2]+lift
    surface_support=uniform_filter1d(np.array(surface_support).T,5,axis=0,mode='nearest')
    surface_support=np.clip(surface_support,0.,1.)
    surface_support=np.where(surface_support>1e-8,surface_support,0.)
    # Contact must follow the shoe rather than the ankle pivot. An input foot
    # sliding horizontally is still supported; retain its height/motion evidence.
    contact=np.maximum(stance,surface_support)
    links=np.minimum(contact[:-1],contact[1:])
    sole_offsets=np.stack(sole_offsets,axis=1)
    # Distinguish observed stationary feet from merely supported moving feet.
    # Use ankle/toes/heel together so a planted toe alone cannot freeze a pivot.
    # Confidence and consecutive-frame persistence suppress occlusion/step edges.
    foot_keys=keypoints[:,[[14,19,20,21],[11,22,23,24]]]
    observed=uniform_filter1d(foot_keys[:,:,:,:2],7,axis=0,mode='nearest')
    point_speed=np.linalg.norm(np.diff(observed,axis=0,prepend=observed[:1]),axis=-1)*30/scale
    # Require three of four landmarks to agree; one noisy heel/toe is not motion.
    # A true pivot moves both ankle and heel and therefore still fails this cue.
    image_speed=np.sort(point_speed,axis=2)[:,:,2]
    observed_conf=np.clip((foot_keys[:,:,:,2].min(2)-.55)/.25,0,1)
    stationary=minimum_filter1d(np.clip((6-image_speed)/4,0,1)*observed_conf,5,axis=0,mode='nearest')
    # Brief pauses/noisy detections are insufficient for a planted-foot cue.
    # Nine samples are 0.3 s in this worker's validated 30 FPS input contract.
    for side in range(2):
        ids=np.flatnonzero(stationary[:,side]>0)
        for segment in np.split(ids,np.flatnonzero(np.diff(ids)>1)+1):
            if len(segment)<9:stationary[segment,side]=0
    # Fade qualified evidence across entry/exit, rather than suddenly changing
    # the velocity cost and displacing motion into the neighbouring frames.
    # Support still gates the cost; this does not lock an airborne foot.
    stationary=uniform_filter1d(stationary,7,axis=0,mode='nearest')
    stationary_links=np.minimum(stationary[:-1],stationary[1:])
    contact_triples=np.maximum.reduce([contact[:-2],contact[1:-1],contact[2:]])
    target_height=(1-surface_support)*target_height+surface_support*shoe_floor_height
    var=[0,1,2,4,5];chain=[-1,0,1,2,0,4,5]
    def fk(x):
        x=x.reshape(n,18);rot=R.from_rotvec(x[:,3:].reshape(-1,3)).as_matrix().reshape(n,5,3,3)
        g=q.copy();v=p.copy();g[:,0]=rot[:,0]@q[:,0];v[:,0]+=x[:,:3]
        for j in range(1,7):
            parent=chain[j];r=lq[:,j]
            if j in var:r=rot[:,var.index(j)]@r
            g[:,j]=g[:,parent]@r;v[:,j]=v[:,parent]+np.einsum('fij,fj->fi',g[:,parent],lp[:,j])
        return v,g
    def residual(x):
        v,g=fk(x);x=x.reshape(n,18)
        data=(((v[:,1:]@camera[:2].T)-target)*conf[:,:,None]*2).reshape(n,-1)
        # Video weak-perspective observations cannot determine camera depth.
        # Anchor that null direction while allowing the observed pelvic shift.
        prior=np.c_[(x[:,:3]@camera.T)*np.array([.4,.4,2.]),x[:,3:]*10]
        floor=(v[:,[3,6],2]-target_height)*(1+stance*3+surface_support*6)
        # Strong consecutive support is better evidence for a stable sole than
        # the drifting input trajectory. Increase only that soft observation;
        # weak transitions keep the previous weight, with no hard foot locking.
        slip=np.diff(v[:,[3,6],:2]+sole_offsets,axis=0)*(links*(22+5*links+100*stationary_links))[:,:,None]
        smooth=np.diff(x,n=2,axis=0)*np.r_[np.ones(3)*2,np.ones(15)*20]
        continuity=np.diff(x,axis=0)*np.r_[np.ones(3)*2,np.ones(15)*40]
        pose_curvature=np.diff(g[:,var],n=2,axis=0)*30
        # Penalize jitter in the resulting trajectory, not only in its correction.
        # Constant-speed motion remains a zero-curvature trajectory.
        position_curvature=np.diff(v,n=2,axis=0)*2.5
        # A small contact-weighted output-trajectory cost controls the extra
        # entry/exit acceleration without imposing constant foot positions.
        position_curvature[:,[3,6]]*=1+.3*contact_triples[:,:,None]
        depth=np.array([(v[:,j+1]-(v[:,j]+v[:,j+2])*.5)@camera[2] for j in [1,4]]).T
        depth_observation=(((v[:,1:]-v[:,:1])@camera[2])-relative_depth)*depth_weight[:,None]
        separation=(v[:,[1,2,3]]-v[:,[4,5,6]])@camera[2]
        # Soft ordering evidence only; no hard constraint or joint-angle bound.
        order=np.minimum(order_sign*separation-order_margin,0)*order_confidence*4
        # Fixed foot world orientation means its original shoe clearance gives
        # a positional floor observation. Do not rotate the ankle to reach it.
        penetration=np.minimum(v[:,[3,6],2]-shoe_floor_height,0)*12
        return np.r_[np.c_[data,prior,floor,depth*straight*4,depth_observation,order,penetration].ravel(),slip.ravel(),smooth.ravel(),continuity.ravel(),np.c_[pose_curvature.reshape(n-2,45),position_curvature.reshape(n-2,21)].ravel()]
    rows=n*45+(n-1)*4+(n-2)*18+(n-1)*18+(n-2)*66;spars=lil_matrix((rows,n*18),dtype=np.int8)
    for i in range(n):spars[i*45:(i+1)*45,i*18:(i+1)*18]=1
    row=n*45
    for i in range(n-1):spars[row+i*4:row+(i+1)*4,i*18:(i+2)*18]=1
    row+=(n-1)*4
    for i in range(n-2):spars[row+i*18:row+(i+1)*18,i*18:(i+3)*18]=1
    row+=(n-2)*18
    for i in range(n-1):spars[row+i*18:row+(i+1)*18,i*18:(i+2)*18]=1
    row+=(n-1)*18
    for i in range(n-2):spars[row+i*66:row+(i+1)*66,i*18:(i+3)*18]=1
    progress('Video pelvis/legs fit',n//2)
    x0=np.zeros(n*18);initial=float(residual(x0)@residual(x0)/2)
    fit=temporal_least_squares(residual,x0,jac_sparsity=spars.tocsr(),block_size=18,max_nfev=45,ftol=1e-4,
        progress=lambda i,total,cost:progress('Video pelvis/legs fit %d/%d'%(i+1,total),n//2))
    if not np.isfinite(fit.x).all() or fit.cost>initial:raise RuntimeError('Lower-body fit did not improve its finite objective')
    v,g=fk(fit.x);new=local.copy()
    for j in var:
        bone=indices[j];parent=parents[bone];parentq=q0[:,parent] if j==0 else g[:,chain[j]];parentp=p0[:,parent] if j==0 else v[:,chain[j]]
        if j==0:new[:,bone,:3]=np.einsum('fji,fj->fi',parentq,v[:,j]-parentp)
        new[:,bone,3:]=R.from_matrix(parentq.transpose(0,2,1)@g[:,j]).as_quat()
    # Exact upper-body world pose preservation, including hands/wrists.
    new[:,spine,:3]=np.einsum('fji,fj->fi',g[:,0],p0[:,spine]-v[:,0])
    new[:,spine,3:]=R.from_matrix(g[:,0].transpose(0,2,1)@q0[:,spine]).as_quat()
    # Hold original ankle world orientation while moving pelvis and legs.
    for j in [3,6]:new[:,indices[j],3:]=R.from_matrix(g[:,chain[j]].transpose(0,2,1)@q0[:,indices[j]]).as_quat()
    wp,wq=globals_from_local(new,parents);used=[names.index(name) for name in c['bones']]
    c['world']=np.concatenate([wp[:,used],R.from_matrix(wq[:,used].reshape(-1,3,3)).as_quat().reshape(n,len(used),4)],axis=-1).reshape(-1).tolist()
    state=dict(local=new,original_world_q=q0,names=names,parents=parents,world_q=wq,surface_support=surface_support,stationary_foot_weights=stationary)
    report=dict(version=VERSION,seconds=time.perf_counter()-start,initial_cost=initial,final_cost=float(fit.cost),nfev=fit.nfev,termination=int(fit.status),target_pixels_per_cm=scale,pelvis_translation_cm_p50_p95_max=np.percentile(np.linalg.norm(fit.x.reshape(n,18)[:,:3],axis=1),[50,95,100]).tolist(),joint_limits=False,upper_body_world_preserved=True,hand_local_tracks_preserved=True,projection='foot-centred weak perspective with target leg lengths; camera orientation fixed')
    report.update(solver=fit.solver_report,soft_shoe_clearance_lift_cm_p50_p95_max=np.percentile(lift,[50,95,100],axis=0).tolist(),camera_depth_prior=True,output_rotation_curvature=True)
    report.update(relative_joint_depth_prior=True,soft_depth_order_prior=True,soft_positional_shoe_floor=True,support_height_uses_vertical_motion=True,side_view_weight_p50_p95_max=np.percentile(depth_weight,[50,95,100]).tolist())
    report.update(slip_space='target sole centroid',sliding_input_retains_support=True)
    report.update(output_position_curvature=True)
    report.update(support_slip_weight='22 + 5 * consecutive_support + 100 * observed_stationarity; multiplied by support',stationary_foot_weights=stationary.reshape(-1).tolist(),stationary_evidence='confident ankle/toes/heel image motion; five-frame minimum, nine-frame qualification, seven-frame symmetric fade',contact_position_curvature_multiplier=.3,stationary_minimum_samples=9,stationary_fade_samples=7,nominal_time_shift_frames=0)
    return c,state,report

def finish_legs_only(result, state, report):
    local=state['local'];names=state['names']
    _,world=globals_from_local(local,state['parents'])
    feet=[names.index(name) for name in ['foot_l','foot_r']]
    error=R.from_matrix((state['original_world_q'][:,feet].transpose(0,1,3,2)@world[:,feet]).reshape(-1,3,3)).magnitude()
    error=error.reshape(len(local),2)
    support_rotation=state.get('support_rotation',False)
    if support_rotation:error=error[state['surface_support']==0]
    maximum=float(np.max(error,initial=0.))
    if maximum>1e-6:raise RuntimeError('Ankle world orientation was not preserved')
    changed=['pelvis','spine_01','thigh_l','calf_l','foot_l','thigh_r','calf_r','foot_r']
    if support_rotation:changed+=['ball_l','ball_r']
    result['lower_body_local']={name:local[:,names.index(name)].reshape(-1).tolist() for name in changed}
    report.update(ankle_orientation_space='world/component',ankle_world_max_radians=maximum,ankle_local_counter_rotation=True,foot_model_used=False,ankle_contact_rotation_fit=False)
    report.update(ankle_world_error_scope='airborne' if support_rotation else 'all_frames',ankle_contact_rotation_fit=support_rotation)
    result['support_rotation']=support_rotation
    result['lower_body_fit']=report
    return result
