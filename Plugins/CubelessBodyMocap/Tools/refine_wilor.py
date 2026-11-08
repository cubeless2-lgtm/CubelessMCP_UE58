"""Cubeless finger-only WiLoR worker. No Unreal imports; one frame resident at a time.

References: GVHMR3 kit v13 hands_detect / hands_smplx (internal R&D),
rolpotamias/WiLoR fcb911312a38fa8badd30d9656a167485d61b8f9 (unmodified).
MANO and WiLoR licenses apply; model files are installed separately.
Output: SMPL flat-hand local quaternions (xyzw), plus per-hand blend weights.
Wrist/global orientation is deliberately NOT exported or applied.
"""
import argparse
import contextlib
import json
import os
from pathlib import Path
import sys
import time
import traceback


def atomic_json(path, data):
    p = Path(path)
    tmp = p.with_suffix(p.suffix + '.tmp')
    tmp.write_text(json.dumps(data, allow_nan=False), encoding='utf-8')
    # UE's short-lived Windows read handle can temporarily deny delete/rename.
    # Retry only this idempotent publication; never rerun inference or asset writes.
    for attempt in range(30):
        try:
            tmp.replace(p)
            break
        except PermissionError:
            if attempt == 29:
                raise
            time.sleep(.01)


def clean_rotations(q, valid, fps):
    """Reject isolated spikes, slerp short gaps, fade long gaps to ORIGINAL via weight.

    Unlike v13 we never replace a hidden hand with an invented relaxed mean pose.
    Smoothing is restricted to supported intervals; original fallback remains exact.
    """
    import numpy as np
    from scipy.spatial.transform import Rotation, Slerp
    from scipy.ndimage import gaussian_filter1d
    q = q.copy()
    valid = valid.copy()
    n = len(q)
    weights = np.zeros((n, 2), dtype=np.float64)
    state = np.zeros((n, 2), dtype=np.int32)
    max_gap = max(1, round(fps / 3))
    ease = max(1, round(fps / 4))
    for s in range(2):
        ids = np.flatnonzero(valid[:, s])
        drops = []
        for k in range(1, len(ids) - 1):
            p, i, j = ids[k - 1:k + 2]
            if i-p > 3 or j-i > 3:
                continue
            angles = [np.degrees(2*np.arccos(np.clip(np.abs((q[i,s]*q[x,s]).sum(-1)),0,1))).mean() for x in [p,j]]
            neighbors = np.degrees(2*np.arccos(np.clip(np.abs((q[p,s]*q[j,s]).sum(-1)),0,1))).mean()
            # A large step on both sides is not enough: during real movement
            # all three poses may differ. Only reject a return-to-neighbor
            # excursion, otherwise interpolation would erase the observation.
            if min(angles) > 40 and neighbors < 40:
                drops.append(i)
        valid[drops, s] = False
        ids = np.flatnonzero(valid[:, s])
        if not len(ids):
            q[:, s] = [0, 0, 0, 1]
            continue
        weights[ids, s] = 1
        state[ids, s] = 1
        for p, j in zip(ids[:-1], ids[1:]):
            if 0 < j-p-1 <= max_gap:
                for b in range(15):
                    q[p+1:j, s, b] = Slerp([p,j], Rotation.from_quat(q[[p,j],s,b]))(np.arange(p+1,j)).as_quat()
                weights[p+1:j,s] = 1
                state[p+1:j,s] = 2
        for i in np.flatnonzero(weights[:,s] == 0):
            nearest = ids[np.argmin(abs(ids-i))]
            dist = abs(nearest-i)
            q[i,s] = q[nearest,s]
            if dist <= ease:
                t = dist / (ease+1)
                weights[i,s] = 1-t*t*(3-2*t)
                state[i,s] = 3
        # Hemisphere alignment precedes averaging (q and -q encode the same rotation).
        for i in range(1,n):
            q[i,s] *= np.where((q[i,s]*q[i-1,s]).sum(-1,keepdims=True)<0,-1,1)
        supported = weights[:,s] > 0
        starts = np.flatnonzero(supported & ~np.r_[False,supported[:-1]])
        ends = np.flatnonzero(supported & ~np.r_[supported[1:],False])+1
        for lo,hi in zip(starts,ends):
            q[lo:hi,s] = gaussian_filter1d(q[lo:hi,s], sigma=max(.4,fps*.035), axis=0, mode='nearest')
            q[lo:hi,s] /= np.linalg.norm(q[lo:hi,s],axis=-1,keepdims=True)
    return q, weights, state


def infer(config):
    import numpy as np
    import cv2
    import torch
    from scipy.spatial.transform import Rotation
    from scipy.optimize import linear_sum_assignment
    c = config
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from hand_crop_consensus import prepare_reference, select_crop
    repo = Path(c['repo']).resolve()
    work = Path(c['work']).resolve()
    def progress(stage, done=0):
        atomic_json(work/'worker_status.json', {'stage':stage,'done':done,'frames':c['frames']})
    reference,reference_seconds=prepare_reference(c,progress)
    selected_tight=0
    for rel in ['mano_data/MANO_RIGHT.pkl','pretrained_models/wilor_final.ckpt','pretrained_models/detector.pt']:
        if not (repo/rel).is_file():
            raise FileNotFoundError(str(repo/rel))
    if not torch.cuda.is_available():
        raise RuntimeError('WiLoR CUDA is unavailable. Check the hand Python environment; CPU fallback is not enabled.')
    # Legacy MANO pickles use numpy aliases and chumpy's getargspec on Python 3.12.
    import inspect
    if not hasattr(inspect,'getargspec'):
        inspect.getargspec = inspect.getfullargspec
    aliases={'bool':bool,'int':int,'float':float,'complex':complex,'object':object,'unicode':str,'str':str}
    for k,v in aliases.items():
        if k not in np.__dict__:
            setattr(np,k,v)
    os.environ['PYOPENGL_PLATFORM'] = ''
    sys.path.insert(0,str(repo))
    os.chdir(repo)
    from ultralytics import YOLO
    from wilor.models.wilor import WiLoR
    from wilor.configs import get_config
    from wilor.datasets.vitdet_dataset import ViTDetDataset
    cfg = get_config(str(repo/'pretrained_models/model_config.yaml'), update_cachedir=True)
    cfg.defrost()
    cfg.MODEL.BBOX_SHAPE = [192,256]
    cfg.MODEL.BACKBONE.pop('PRETRAINED_WEIGHTS',None)
    cfg.MANO.DATA_DIR = str(repo/'mano_data')
    cfg.MANO.MODEL_PATH = str(repo/'mano_data')
    cfg.MANO.MEAN_PARAMS = str(repo/'mano_data/mano_mean_params.npz')
    cfg.freeze()
    # Official checkpoint, equivalent to load_wilor, with unused rendering disabled.
    progress('Loading WiLoR')
    model = WiLoR.load_from_checkpoint(str(repo/'pretrained_models/wilor_final.ckpt'), strict=False,
                                      cfg=cfg, init_renderer=False, weights_only=False).eval().cuda()
    detector = YOLO(str(repo/'pretrained_models/detector.pt'))
    keys = np.fromfile(work/'keypoints.f32',dtype='<f4').reshape(c['frames'],133,3)
    n = len(keys)
    q = np.tile([0.,0.,0.,1.],(n,2,15,1))
    valid = np.zeros((n,2),bool)
    metadata = []
    # Opt-in source-space evidence, before filtering or Unreal retargeting.
    # Projection follows the official demo; never fit the mesh to landmarks.
    diagnostics = bool(c.get('diagnostics', False))
    evidence = []
    mirror = np.diag([-1.,1.,1.])
    begin = time.perf_counter()
    torch.set_num_threads(min(4,os.cpu_count() or 4))
    with torch.inference_mode():
        for f in range(n):
            progress('WiLoR hands',f)
            im = cv2.imread(str(Path(c['frame_dir'])/f'{f:08d}.png'))
            if im is None:
                raise ValueError(f'Missing frame {f}')
            h,w = im.shape[:2]
            # UE ViTPose coordinates are on its height<=896 raster.
            k = keys[f].copy()
            k[:,:2] /= min(896/h,1.)
            centers=[]; sizes=[]; wrist_ok=[]
            for s,wr,base in [(0,9,91),(1,10,112)]:
                wrist_ok.append(bool(k[wr,2]>=.3))
                hand=k[base:base+21]
                reliable=hand[:,2]>=.3
                if reliable.sum()>=5:
                    points=hand[reliable,:2]
                    centers.append((points.min(0)+points.max(0))/2)
                    sizes.append(max(20.,float(np.ptp(points,axis=0).max())))
                else:
                    centers.append(k[wr,:2]+.25*(k[wr,:2]-k[wr-2,:2]))
                    sizes.append(max(20.,.45*float(np.linalg.norm(k[wr,:2]-k[wr-2,:2]))))
            frame_reference={} if reference is None else {x['side']:x for x in reference[f]['hands']}
            for side,observation in frame_reference.items():
                uv=np.asarray(observation['keypoints2d'])
                centers[side]=(uv.min(0)+uv.max(0))/2
                sizes[side]=max(20.,float(np.ptp(uv,axis=0).max()))
            centers=np.asarray(centers); sizes=np.asarray(sizes)
            result=detector(im,conf=.35,verbose=False,device=0)[0]
            boxes=result.boxes.xyxy.cpu().numpy()
            labels=result.boxes.cls.cpu().numpy()
            conf=result.boxes.conf.cpu().numpy()
            # Unique left/right assignment using body wrist cues, with handedness only a penalty.
            cost=np.full((2,len(boxes)+2),1.2)
            if len(boxes):
                cost[:,:len(boxes)] = np.linalg.norm(centers[:,None]-(boxes[:,:2]+boxes[:,2:])/2,axis=2)/sizes[:,None]
                cost[:,:len(boxes)] += .25*(labels[None,:] != np.arange(2)[:,None])
            rows,cols=linear_sum_assignment(cost)
            chosen={int(s):int(b) for s,b in zip(rows,cols) if b<len(boxes) and cost[s,b]<=.8 and wrist_ok[s]}
            if len(chosen)==2:
                a,b=boxes[chosen[0]],boxes[chosen[1]]
                overlap=np.maximum(0,np.minimum(a[2:],b[2:])-np.maximum(a[:2],b[:2])).prod()
                if overlap>.3*min(np.prod(a[2:]-a[:2]),np.prod(b[2:]-b[:2])):
                    bad=max(chosen,key=lambda s:cost[s,chosen[s]])
                    del chosen[bad]
            rec={'frame':f,'hands':[]}
            if chosen:
                sides=sorted(chosen)
                trial_sides=sides*2 if frame_reference else sides
                pads=np.repeat([2.,1.6],len(sides)) if frame_reference else np.full(len(sides),2.)
                ds=ViTDetDataset(cfg,im,np.stack([boxes[chosen[s]] for s in trial_sides]),np.array(trial_sides),rescale_factor=pads[:,None])
                batch=torch.utils.data.default_collate([ds[i] for i in range(len(ds))])
                batch={k:(v.cuda() if isinstance(v,torch.Tensor) else v) for k,v in batch.items()}
                out=model(batch)
                hp=out['pred_mano_params']['hand_pose'].detach().float().cpu().numpy().reshape(-1,15,3,3)
                joints=out['pred_keypoints_3d'].detach().float().cpu().numpy()
                cam=out['pred_cam'].detach().float().cpu().numpy().copy()
                centers_b=batch['box_center'].detach().float().cpu().numpy()
                sizes_b=batch['box_size'].detach().float().cpu().numpy()
                img_sizes=batch['img_size'].detach().float().cpu().numpy()
                focal=float(cfg.EXTRA.FOCAL_LENGTH/cfg.MODEL.IMAGE_SIZE*img_sizes.max())
                cameras=[];projected=[];camera_joints=[]
                for j,side in enumerate(trial_sides):
                    sign=2*side-1;cam[j,1]*=sign
                    bs=sizes_b[j]*cam[j,0]+1e-9
                    ct=np.array([2*(centers_b[j,0]-img_sizes[j,0]/2)/bs+cam[j,1],2*(centers_b[j,1]-img_sizes[j,1]/2)/bs+cam[j,2],2*focal/bs])
                    pts=joints[j].copy();pts[:,0]*=sign;pts+=ct
                    cameras.append(ct);camera_joints.append(pts);projected.append(pts[:,:2]/pts[:,2:]*focal+img_sizes[j]/2)
                verts=out['pred_vertices'].detach().float().cpu().numpy() if diagnostics else None
                for index,side in enumerate(sides):
                    choices=[j for j,sample_side in enumerate(trial_sides) if sample_side==side]
                    best,scores=select_crop([projected[j] for j in choices],frame_reference.get(side))
                    j=choices[best];selected_tight+=int(pads[j]<2.)
                    if diagnostics:
                        v=verts[j].copy();v[:,0]*=2*side-1;v+=cameras[j]
                        evidence.append({'frame':f,'side':side,'vertices_camera':v,'joints_camera':camera_joints[j],
                                         'vertices_uv':v[:,:2]/v[:,2:]*focal+img_sizes[j]/2,'joints_uv':projected[j],
                                         'hand_pose':hp[j].copy(),'padding':pads[j]})
                    r=hp[j]
                    if side==0:r=mirror@r@mirror
                    if not np.isfinite(r).all():continue
                    q[f,side]=Rotation.from_matrix(r).as_quat();valid[f,side]=True
                    rec['hands'].append({'side':side,'box':boxes[chosen[side]].tolist(),'confidence':float(conf[chosen[side]]),
                                         'cost':float(cost[side,chosen[side]]),'padding':float(pads[j]),'reference_scores':scores})
            metadata.append(rec)
    raw_valid=valid.copy()
    if diagnostics:
        fields={key:np.asarray([e[key] for e in evidence]) for key in ['frame','side','vertices_camera','joints_camera','vertices_uv','joints_uv','hand_pose','padding']}
        np.savez_compressed(work/'model_evidence.npz',faces=np.asarray(model.mano.faces),raw_quaternions_smpl=q,raw_valid=valid,**fields)
    q,weights,state=clean_rotations(q,valid,c['fps'])
    if not (weights>0).any():
        raise RuntimeError('No trustworthy hands detected; original animation preserved.')
    # UE mapping C(x,y,z)=(x,z,y), reflection: quaternion vector transforms as -C*v.
    from hand_contract import smpl_quaternions_to_ue
    ue=smpl_quaternions_to_ue(q)
    metrics={'landmark_reference_available':reference is not None,'landmark_reference_seconds':reference_seconds,'tight_crop_selected':selected_tight,'frames':n,'seconds':time.perf_counter()-begin,'source_asset':c['source_asset'],
             'detected_frames_LR':raw_valid.sum(0).tolist(),'used_frames_LR':(state==1).sum(0).tolist(),
             'interpolated_frames_LR':(state==2).sum(0).tolist(),'faded_frames_LR':(state==3).sum(0).tolist(),
             'original_fallback_frames_LR':(state==0).sum(0).tolist(),
             'cuda_peak_allocated_mib':torch.cuda.max_memory_allocated()/1048576.,
             'note':'Detection coverage is not ground-truth accuracy. Wrist and body tracks are untouched.'}
    np.savez_compressed(work/'hand_diagnostics.npz',quaternions_smpl=q,weights=weights,state=state)
    atomic_json(work/'hands.json',{'version':1,'backend':'WiLoR','frames':n,'fps':c['fps'],'source_asset':c['source_asset'],
                                 'quaternions_ue':ue.reshape(-1).tolist(),'weights':weights.reshape(-1).tolist()})
    atomic_json(work/'detection.json',metadata)
    atomic_json(work/'metrics.json',metrics)
    progress('Completed',n)


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--config',type=Path,required=True)
    args=p.parse_args()
    c=json.loads(args.config.read_text(encoding='utf-8-sig'))
    work=Path(c['work'])
    with (work/'worker.log').open('w',encoding='utf-8',buffering=1) as log,contextlib.redirect_stdout(log),contextlib.redirect_stderr(log):
        try:
            infer(c)
        except Exception as e:
            traceback.print_exc()
            atomic_json(work/'worker_status.json',{'stage':'Failed','error':str(e)})
            return 1
    return 0


if __name__=='__main__':
    raise SystemExit(main())
