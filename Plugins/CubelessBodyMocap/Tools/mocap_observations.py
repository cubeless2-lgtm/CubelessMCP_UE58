"""Source-video observation validation and skeleton helpers (no generated reference)."""

import json,hashlib

from pathlib import Path

import numpy as np

from scipy.spatial.transform import Rotation

MAPPING={'upperarm_l':5,'upperarm_r':6,'lowerarm_l':7,'lowerarm_r':8,
         'hand_l':9,'hand_r':10,'thigh_l':11,'thigh_r':12,'calf_l':13,'calf_r':14,
         'foot_l':15,'foot_r':16,'ball_l':17,'ball_r':20,
         'middle_01_l':100,'middle_01_r':121,'index_01_l':96,'index_01_r':117,'pinky_01_l':108,'pinky_01_r':129}

IK={'ik_foot_l':'foot_l','ik_foot_r':'foot_r','ik_hand_gun':'hand_r','ik_hand_l':'hand_l','ik_hand_r':'hand_r'}

def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def observations(path):
    d=json.loads(Path(path).read_bytes());rows=d['frames']
    if Path(d['source']).suffix.lower() not in {'.mp4','.mov','.mkv','.avi','.webm','.m4v','.wmv'}:raise ValueError('Original-video observations required')
    if d.get('schema')!='cbm_pose_observations_v1' or d.get('keypoint_layout')!='coco_wholebody_133':
        raise ValueError('Need explicit COCO whole-body observations')
    t=np.array([r['seconds'] for r in rows],float);k=np.array([r['keypoints'] for r in rows],float)
    if len(t)<2 or k.shape!=(len(t),133,3) or not np.isfinite(k).all() or not np.isfinite(t).all() or np.any(np.diff(t)<=0):
        raise ValueError('Invalid observation timing/keypoints')
    if not Path(d['source']).is_file() or digest(d['source'])!=d['source_sha256']:
        raise ValueError('Observation source has changed')
    if any(r['width']!=rows[0]['width'] or r['height']!=rows[0]['height'] for r in rows):
        raise ValueError('Changing image dimensions require explicit normalization')
    return d,t,k

def resample(t,k,target):
    result=np.empty((len(target),133,3))
    for j in range(133):
        for c in range(2):result[:,j,c]=np.interp(target,t,k[:,j,c])
    hi=np.searchsorted(t,target).clip(0,len(t)-1);lo=(hi-1).clip(0,len(t)-1)
    result[:,:,2]=np.minimum(k[lo,:,2],k[hi,:,2])
    return result

def fk_numpy(d):
    n,b=d['frames'],len(d['names']);q=np.array(d['local_q']).reshape(n,b,4);p=np.array(d['local_p']).reshape(n,b,3)
    L=Rotation.from_quat(q.reshape(-1,4)).as_matrix().reshape(n,b,3,3);R=[];P=[]
    for i,pa in enumerate(d['parents']):
        R.append(L[:,i] if pa<0 else R[pa]@L[:,i]);P.append(p[:,i] if pa<0 else P[pa]+(R[pa]@p[:,i,:,None])[...,0])
    return np.stack(R,1),np.stack(P,1)
