"""Model-free support-only ankle/forefoot fitting to the target shoe surface.

No angle bounds, foot model or guessed bone axes. Airborne corrections are zero.
"""
import time
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.sparse import lil_matrix
from temporal_solver import temporal_least_squares
from lower_body import globals_from_local

VERSION = 'target_support_rotation_v4_contact_transitions'

def fit_support_rotation(state, context, progress=lambda *a:None):
    start=time.perf_counter();local=state['local'];names=state['names'];parents=state['parents']
    wp,wq=globals_from_local(local,parents);n=len(local);reports=[]
    support=state['surface_support'];ground=float(context['ground_z_cm'])
    def child_of(bone, root):
        while bone>=0:
            if bone==root:return True
            bone=parents[bone]
        return False
    for side,suffix in enumerate(['l','r']):
        progress('Support ankle/forefoot '+suffix,n//2)
        foot=names.index('foot_'+suffix);ball=names.index('ball_'+suffix)
        if parents[ball]!=foot:raise ValueError('Support fit requires forefoot parent ankle')
        vertices=context['vertices'][side];rest=np.array([v['rest'] for v in vertices])
        sole=np.flatnonzero(rest[:,2]<rest[:,2].min()+1.5)
        if len(sole)<8:raise ValueError('Insufficient target sole geometry')
        chosen=[int(sole[np.argmin(rest[sole,2])])];distance=np.full(len(sole),np.inf)
        for _ in range(min(63,len(sole)-1)):
            distance=np.minimum(distance,np.linalg.norm(rest[sole]-rest[chosen[-1]],axis=1));chosen.append(int(sole[np.argmax(distance)]))
        count=len(chosen);depfoot=np.zeros((n,count,3));depball=depfoot.copy();constant=depfoot.copy()
        weightfoot=np.zeros(count);weightball=np.zeros(count)
        for vi,idx in enumerate(chosen):
            for bi,weight,point in vertices[idx]['weights']:
                bone=names.index(context['bones'][bi]);world=np.einsum('fij,j->fi',wq[:,bone],point)+wp[:,bone]
                if child_of(bone,ball):
                    depball[:,vi]+=np.einsum('fji,fj->fi',wq[:,ball],world-wp[:,ball])*weight;weightball[vi]+=weight
                elif child_of(bone,foot):
                    depfoot[:,vi]+=np.einsum('fji,fj->fi',wq[:,foot],world-wp[:,foot])*weight;weightfoot[vi]+=weight
                else:constant[:,vi]+=world*weight
        basefoot=wq[:,foot];balllocal=R.from_quat(local[:,ball,3:]).as_matrix();ballpos=local[:,ball,:3]
        alpha=support[:,side];target=rest[chosen,2]-rest[:,2].min()+ground+.03
        links=np.sqrt(np.minimum(alpha[:-1],alpha[1:]))
        stationary=state.get('stationary_foot_weights',np.zeros((n,2)))[:,side]
        stationary_links=np.minimum(stationary[:-1],stationary[1:])
        # Include contact entry/exit neighbours. Their airborne poses remain
        # fixed, but the supported endpoint must not introduce a new jump.
        triples=np.maximum.reduce([alpha[:-2],alpha[1:-1],alpha[2:]])
        source_rotation=np.stack([basefoot,basefoot@balllocal],axis=1)
        source_step=np.linalg.norm(np.diff(source_rotation,axis=0),axis=(2,3))
        def pose(x):
            delta=R.from_rotvec((x.reshape(n,2,3)*alpha[:,None,None]).reshape(-1,3)).as_matrix().reshape(n,2,3,3)
            fq=delta[:,0]@basefoot;bq=fq@balllocal@delta[:,1]
            bp=wp[:,foot]+np.einsum('fij,fj->fi',fq,ballpos)
            xyz=constant+np.einsum('fij,fvj->fvi',fq,depfoot)+wp[:,foot,None]*weightfoot[None,:,None]+np.einsum('fij,fvj->fvi',bq,depball)+bp[:,None]*weightball[None,:,None]
            return delta,fq,bq,xyz
        # Centroid-only stabilization leaves rotations about that centroid
        # unconstrained. Use near-ground sole samples only when the video also
        # supports stationarity. Keep weights fixed during this solve so a foot
        # cannot evade the observation by lifting its contact points.
        patch_count=2*count if np.any(stationary_links) else 0
        patch_weights=None
        if patch_count:
            initial_xyz=pose(np.zeros(n*6))[3]
            patch_weights=np.clip((ground+1.5-initial_xyz[:,:,2])/1.5,0,1)
            patch_weights=np.minimum(patch_weights[:-1],patch_weights[1:])*stationary_links[:,None]*links[:,None]
        sole_curvature_weight=5*triples+40*np.maximum.reduce([stationary[:-2],stationary[1:-1],stationary[2:]])
        def residual(x):
            d,fq,bq,xyz=pose(x);z=xyz[:,:,2]
            prior=np.c_[((d[:,0]-np.eye(3))*6).reshape(n,-1),((d[:,1]-np.eye(3))*10).reshape(n,-1),x.reshape(n,6)*2]
            contact=(z-target[None])*alpha[:,None]*10/np.sqrt(count)
            penetration=np.minimum(z-ground,0)*75/np.sqrt(count)
            # Smooth the actual output, not only its correction. A changing
            # source orientation can otherwise move the sole under a still ankle.
            # Retain the same observed stationary-foot evidence after ankle/toe
            # fitting; otherwise rotations can reintroduce sole drift.
            slip=np.diff(xyz[:,:,:2].mean(1),axis=0)*links[:,None]*22
            if np.any(stationary_links):slip*= (1+5*stationary_links)[:,None]
            patch=np.empty((n-1,0)) if not patch_count else (np.diff(xyz[:,:,:2],axis=0)*patch_weights[:,:,None]*(60/np.sqrt(count))).reshape(n-1,patch_count)
            sole_curvature=np.diff(xyz[:,:,:2].mean(1),n=2,axis=0)*sole_curvature_weight[:,None]
            rotation=np.stack([fq,bq],axis=1)
            curvature=np.diff(rotation,n=2,axis=0)*triples[:,None,None,None]*30
            # Soft temporal evidence: do not create a faster rotation than the
            # source at contact entry/exit. No angle clipping or joint bounds.
            step=np.linalg.norm(np.diff(rotation,axis=0),axis=(2,3))
            # The correction already carries contact confidence. Weighting this
            # residual by confidence again let weak landing frames jump sharply.
            excess=np.maximum(step-source_step,0)*80
            return np.r_[np.c_[prior,contact,penetration].ravel(),np.c_[(np.diff(d,axis=0)*30).reshape(n-1,18),slip,excess,patch].ravel(),np.c_[(np.diff(d,n=2,axis=0)*20).reshape(n-2,18),curvature.reshape(n-2,18),sole_curvature].ravel()]
        per=24+count*2;pair_rows=22+patch_count;triple_rows=38
        rows=n*per+(n-1)*pair_rows+(n-2)*triple_rows;spars=lil_matrix((rows,n*6),dtype=np.int8)
        for f in range(n):spars[f*per:(f+1)*per,f*6:(f+1)*6]=1
        row=n*per
        for f in range(n-1):spars[row+f*pair_rows:row+(f+1)*pair_rows,f*6:(f+2)*6]=1
        row+=(n-1)*pair_rows
        for f in range(n-2):spars[row+f*triple_rows:row+(f+1)*triple_rows,f*6:(f+3)*6]=1
        x0=np.zeros(n*6);initial=float(residual(x0)@residual(x0)/2)
        fit=temporal_least_squares(residual,x0,spars.tocsr(),6,max_nfev=30,ftol=1e-4)
        if not np.isfinite(fit.x).all() or fit.cost>initial:raise RuntimeError('Support fit did not improve finite objective')
        delta,fq,bq,xyz=pose(fit.x)
        active=alpha>0
        if active.any():
            local[active,foot,3:]=R.from_matrix(wq[active,parents[foot]].transpose(0,2,1)@fq[active]).as_quat()
            local[active,ball,3:]=R.from_matrix(balllocal[active]@delta[active,1]).as_quat()
        angles=np.degrees(R.from_matrix(delta.reshape(-1,3,3)).magnitude()).reshape(n,2)
        reports.append(dict(side=suffix,support_frames=int(active.sum()),strong_support_frames=int((alpha>.5).sum()),correction_degrees_p50_p95_max=np.percentile(angles,[50,95,100],axis=0).tolist(),solver=fit.solver_report,initial_cost=initial,final_cost=float(fit.cost)))
    state['support_rotation']=True
    return dict(version=VERSION,seconds=time.perf_counter()-start,sides=reports,support_weights=support.reshape(-1).tolist(),joint_limits=False,foot_model_used=False,airborne_world_orientation_preserved=True,output_sole_motion=True,output_rotation_curvature=True,source_rotation_step_soft_prior=True,stationary_contact_patch_motion=True,output_sole_curvature=True,support_slip_weight=22,contact_patch_weight=60,sole_curvature_weights=[5,40])

