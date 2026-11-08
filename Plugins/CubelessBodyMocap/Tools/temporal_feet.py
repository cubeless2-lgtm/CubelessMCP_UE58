"""Centred SO(3) cleanup of uneven foot timing, without joint-angle bounds.

Only detected high-curvature frames and their immediate neighbours are changed.
The quadratic five-frame filter preserves constant angular velocity and has no
causal delay. All other bones and the clip endpoints remain unchanged.
"""
import copy
import numpy as np
from scipy.spatial.transform import Rotation as R
from lower_body import globals_from_local

VERSION = 'foot_temporal_outliers_v1'

def stabilize(context):
    c=copy.deepcopy(context);s=c['lower_body'];names=s['bones'];parents=s['parents']
    local=np.array(s['local']).reshape(c['frames'],len(names),7);n=len(local)
    wp,wq=globals_from_local(local,parents);mask=np.zeros((n,2));reports=[]
    for side,suffix in enumerate(['l','r']):
        foot=names.index('foot_'+suffix);source=R.from_matrix(wq[:,foot])
        if n<5:
            reports.append(dict(side=suffix,changed_frames=0));continue
        midpoint=source[:-2]*R.from_rotvec((source[:-2].inv()*source[2:]).as_rotvec()*.5)
        error=np.degrees((midpoint.inv()*source[1:-1]).magnitude())
        strength=np.zeros(n);strength[1:-1]=np.clip((error-4.)/8.,0,1)
        # Neighbour support avoids a discontinuity at the edge of a detected event.
        strength=np.maximum.reduce([strength,np.r_[0,strength[:-1]*.5],np.r_[strength[1:]*.5,0]])
        strength[:2]=0;strength[-2:]=0
        correction=np.zeros((n,3))
        for offset,weight in [(-2,-3/35),(-1,12/35),(1,12/35),(2,-3/35)]:
            correction[2:-2]+=(source[2:-2].inv()*source[2+offset:n-2+offset]).as_rotvec()*weight
        correction*=strength[:,None]
        active=(np.linalg.norm(correction,axis=1)>1e-7)&(strength>0)
        filtered=source*R.from_rotvec(correction)
        local[active,foot,3:]=R.from_matrix(wq[active,parents[foot]].transpose(0,2,1)@filtered.as_matrix()[active]).as_quat()
        mask[active,side]=1
        reports.append(dict(side=suffix,changed_frames=int(active.sum()),frames=np.flatnonzero(active).tolist(),source_midpoint_error_degrees_mean_p95_max=[float(error.mean()),*np.percentile(error,[95,100]).tolist()],correction_degrees_mean_p95_max=[float(np.degrees(np.linalg.norm(correction,axis=1)).mean()),*np.percentile(np.degrees(np.linalg.norm(correction,axis=1)),[95,100]).tolist()]))
    s['local']=local.reshape(-1).tolist()
    return c,dict(version=VERSION,frame_mask=mask.reshape(-1).tolist(),sides=reports,centred_window_frames=5,nominal_time_shift_frames=0,joint_limits=False)
