"""MANO-free MediaPipe finger refinement; body/wrist motion stays in the input asset.

21 estimated 3D landmarks -> palm-relative joint rotations -> existing UE retargeter.
Finger axial twist is not measured. Parent-frame minimal swings preserve segment directions.
"""
import argparse
import contextlib
import json
from pathlib import Path
import sys
import time
import traceback

# -I excludes the script directory. Import only our adjacent shared filtering module.
sys.path.insert(0,str(Path(__file__).resolve().parent))
from refine_wilor import atomic_json, clean_rotations
from hand_observations import observe_hands
from hand_geometry import GeometryHistory,reject_palm_flips


def normalize(v):
    import numpy as np
    n=np.linalg.norm(v)
    if n<1e-7:
        raise ValueError('Degenerate hand geometry')
    return v/n


def palm_basis(index,middle,pinky):
    import numpy as np
    x=normalize(index-pinky)
    y=normalize(middle-x*(middle@x))
    return np.column_stack((x,y,np.cross(x,y)))


def swing_rotation(source,target,normal):
    import numpy as np
    from scipy.spatial.transform import Rotation
    a,b=normalize(source),normalize(target)
    axis=np.cross(a,b);s=np.linalg.norm(axis);c=np.clip(a@b,-1,1)
    if s<1e-7:
        return np.eye(3) if c>0 else Rotation.from_rotvec(normalize(np.cross(a,normal))*np.pi).as_matrix()
    return Rotation.from_rotvec(axis/s*np.arctan2(s,c)).as_matrix()


def solve_fingers(world,rest):
    import numpy as np
    from scipy.spatial.transform import Rotation
    # Coordinate reflection into UE axes before palm-relative alignment.
    p=world[:,[0,2,1]]-world[0,[0,2,1]]
    src=palm_basis(rest[0],rest[3],rest[6])
    obs=palm_basis(p[5],p[9],p[17])
    p=p@(src@obs.T).T
    rotations=[]
    for chain,start in enumerate([5,9,17,13,1]):
        parent=np.eye(3)
        for segment in range(3):
            b=chain*3+segment
            delta=rest[b+1]-rest[b] if segment<2 else rest[b]-rest[b-1]
            target=p[start+segment+1]-p[start+segment]
            # Express the observed segment in its actual parent frame. This
            # reproduces landmark directions without imposing a curl sign or
            # anatomical clamp. Joint limits are a separate, later stage.
            local=swing_rotation(delta,parent.T@target,src[:,2])
            rotations.append(Rotation.from_matrix(local).as_quat())
            parent=parent@local
    return np.asarray(rotations)


def stabilize_fingers(q,valid,fps):
    """Per-joint spike rejection; no averaging across the other 14 fingers.

    Work on accepted samples before the shared gap/fallback policy. A genuine
    transition persists in its neighbors; an isolated excursion is replaced by
    their midpoint. Three passes remove short one/two-frame detector glitches.
    """
    import numpy as np
    from scipy.spatial.transform import Rotation,Slerp
    q=q.copy();replaced=0
    def distance(a,b):
        return np.degrees(2*np.arccos(np.clip(abs(a@b),0,1)))
    for s in range(2):
        ids=np.flatnonzero(valid[:,s])
        for _ in range(3):
            old=q.copy()
            for p,i,j in zip(ids[:-2],ids[1:-1],ids[2:]):
                if i-p>3 or j-i>3:continue
                for b in range(15):
                    if min(distance(old[i,s,b],old[p,s,b]),distance(old[i,s,b],old[j,s,b]))>35 and distance(old[p,s,b],old[j,s,b])<35:
                        q[i,s,b]=Slerp([p,j],Rotation.from_quat(old[[p,j],s,b]))([i]).as_quat()[0]
                        replaced+=1
    q,weights,state=clean_rotations(q,valid,fps)
    return q,weights,state,replaced


def infer(c):
    import numpy as np
    import cv2
    import mediapipe as mp
    work=Path(c['work'])
    n=c['frames']
    keys=np.fromfile(work/'keypoints.f32',dtype='<f4').reshape(n,133,3)
    rest=np.asarray(c['source_rest_fingers_ue']).reshape(2,15,3)
    q=np.tile([0.,0.,0.,1.],(n,2,15,1))
    valid=np.zeros((n,2),bool)
    world_landmarks=np.full((n,2,21,3),np.nan)
    image_landmarks=np.full((n,2,21,2),np.nan)
    metadata=[]
    options=mp.tasks.vision.HandLandmarkerOptions(
        base_options=mp.tasks.BaseOptions(model_asset_path=c['model']),
        running_mode=mp.tasks.vision.RunningMode.IMAGE,num_hands=2,
        min_hand_detection_confidence=.5,min_hand_presence_confidence=.5)
    start=time.perf_counter()
    retry_attempts=0
    retry_recovered=0
    last_seen=np.full(2,-100000,dtype=int)
    geometry_history=GeometryHistory(c['fps'])
    geometry_diagnostics={'counts':{},'rejected':[] if c.get('diagnostics',False) else None}
    with mp.tasks.vision.HandLandmarker.create_from_options(options) as detector:
        for f in range(n):
            atomic_json(work/'worker_status.json',{'stage':'MediaPipe hands','done':f,'frames':n})
            im=cv2.imread(str(Path(c['frame_dir'])/f'{f:08d}.png'))
            if im is None:
                raise ValueError(f'Missing frame {f}')
            h,w=im.shape[:2]
            k=keys[f].copy();k[:,:2]/=min(896/h,1.)
            frame_meta={'frame':f,'hands':[]}
            chosen,attempts=observe_hands(detector,im,k,last_seen,f,c['fps'],geometry_history,geometry_diagnostics)
            retry_attempts+=max(0,attempts-2)
            for s,(best,size) in chosen.items():
                cost,uv,world,evidence=best
                try:
                    q[f,s]=solve_fingers(world,rest[s])
                except ValueError:
                    continue
                valid[f,s]=True
                last_seen[s]=f
                geometry_history.accept(world,s,f)
                retry_recovered+=int(bool(evidence['crop_factor']>1.4 or evidence['mirrored_input'] or evidence['rotation_degrees']))
                world_landmarks[f,s]=world
                image_landmarks[f,s]=uv
                frame_meta['hands'].append({'side':s,'wrist_cost':cost,'keypoints2d':uv.tolist(),**evidence})
            metadata.append(frame_meta)
    observed_valid=valid.copy()
    valid,palm_flips=reject_palm_flips(world_landmarks,image_landmarks,valid,c['fps'])
    for f,s in zip(*np.nonzero(palm_flips)):
        rejected=[h for h in metadata[f]['hands'] if h['side']==s]
        for hand in rejected:hand['rejection_reason']='isolated_depth_normal_flip'
        metadata[f].setdefault('rejected_hands',[]).extend(rejected)
        metadata[f]['hands']=[h for h in metadata[f]['hands'] if h['side']!=s]
    np.savez_compressed(work/'raw_hands.npz',world_landmarks=world_landmarks,image_landmarks=image_landmarks,
                        quaternions_ue=q,valid=valid,observed_valid=observed_valid,palm_flips_rejected=palm_flips)
    q,weights,state,replaced=stabilize_fingers(q,valid,c['fps'])
    if not (weights>0).any():
        raise RuntimeError('No trustworthy hands detected; original animation preserved.')
    metrics={'backend':'MediaPipe','solver':'landmark_parent_transport_v3','isolated_joint_spikes_replaced':replaced,'frames':n,'seconds':time.perf_counter()-start,
             'crop_policy':'independent_image_support_v4','retry_attempts':retry_attempts,'retry_recovered':retry_recovered,
             'geometry_rejected_candidates':geometry_diagnostics['counts'],
             'image_support_rejected_candidates':geometry_diagnostics.get('image_rejected',0),
             'palm_flips_rejected_LR':palm_flips.sum(0).tolist(),
             'detected_frames_LR':valid.sum(0).tolist(),'used_frames_LR':(state==1).sum(0).tolist(),
             'interpolated_frames_LR':(state==2).sum(0).tolist(),'faded_frames_LR':(state==3).sum(0).tolist(),
             'original_fallback_frames_LR':(state==0).sum(0).tolist(),
             'note':'Detection coverage is not accuracy. Axial finger twist is inferred. Body/wrist unchanged.'}
    np.savez_compressed(work/'hand_diagnostics.npz',quaternions_ue=q,weights=weights,state=state)
    atomic_json(work/'hands.json',{'version':1,'backend':'MediaPipe','frames':n,'fps':c['fps'],'source_asset':c['source_asset'],
                                  'quaternions_ue':q.reshape(-1).tolist(),'weights':weights.reshape(-1).tolist()})
    atomic_json(work/'metrics.json',metrics)
    atomic_json(work/'detection.json',metadata)
    if c.get('diagnostics',False):
        atomic_json(work/'geometry_rejections.json',geometry_diagnostics['rejected'])
    # Original-resolution overlays for quick source-space inspection.
    edges=[(0,1),(1,2),(2,3),(3,4),(0,5),(5,6),(6,7),(7,8),(0,9),(9,10),(10,11),(11,12),(0,13),(13,14),(14,15),(15,16),(0,17),(17,18),(18,19),(19,20)]
    for f in sorted(set([0,min(n-1,60),min(n-1,120),n-1])):
        im=cv2.imread(str(Path(c['frame_dir'])/f'{f:08d}.png'))
        for hand in metadata[f]['hands']:
            uv=np.asarray(hand['keypoints2d']).astype(int)
            for a,b in edges:
                cv2.line(im,tuple(uv[a]),tuple(uv[b]),(40,220,255) if hand['side']==0 else (255,170,40),1,cv2.LINE_AA)
        cv2.imwrite(str(work/f'overlay_{f:04d}.png'),im)
    atomic_json(work/'worker_status.json',{'stage':'Completed','done':n,'frames':n})


def main():
    p=argparse.ArgumentParser();p.add_argument('--config',type=Path,required=True);args=p.parse_args()
    c=json.loads(args.config.read_text(encoding='utf-8-sig'));work=Path(c['work'])
    with (work/'worker.log').open('w',encoding='utf-8',buffering=1) as log,contextlib.redirect_stdout(log),contextlib.redirect_stderr(log):
        try:
            infer(c)
        except Exception as e:
            traceback.print_exc();atomic_json(work/'worker_status.json',{'stage':'Failed','error':str(e)});return 1
    return 0


if __name__=='__main__':
    raise SystemExit(main())
