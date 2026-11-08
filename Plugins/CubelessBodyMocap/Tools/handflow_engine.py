"""Cubeless adapter of Chamiseul handflow_engine.py, MIT, revision 4e09cfc2.
Copyright (c) 2026 RGB MoCap contributors and Chamiseul. MIT license retained at
ThirdParty/HandFlow/CHAMISEUL_LICENSE.txt. Only HaMeR features + HandFlow
inference; no WiLoR pose inputs. See ThirdParty/HandFlow/README.md.
"""
import contextlib,gc,io,json,os,sys,time
from pathlib import Path


def assign_hands(boxes,confidence,classes,wrists,actor):
    import numpy as np
    result=np.zeros((2,4),np.float32);valid=np.zeros(2,bool);scores=np.zeros(2,np.float32)
    span=max(float(actor[3]-actor[1]),40);pairs=[]
    for side in range(2):
        if wrists[side,2]<.1:continue
        for j,box in enumerate(boxes):
            distance=np.linalg.norm((box[:2]+box[2:])/2-wrists[side,:2])/span
            if distance<.22:pairs.append((distance+.035*(classes[j]!=side)-.015*confidence[j],side,j))
    used=set()
    for _,side,j in sorted(pairs):
        if valid[side] or j in used:continue
        result[side]=boxes[j];valid[side]=True;scores[side]=confidence[j];used.add(j)
    for side in range(2):
        if not valid[side]:
            radius=max(span*.055,12);result[side]=[* (wrists[side,:2]-radius),*(wrists[side,:2]+radius)]
    return result,valid,scores


def infer(request,emit,n):
    import cv2,numpy as np,torch
    from types import SimpleNamespace
    runtime=Path(request['handflow_root']).resolve()
    project=Path(request['work']).resolve()
    UPSTREAM=runtime/'runtime';MODELS=runtime
    BY_KEY={m['key']:SimpleNamespace(path=runtime/m['path']) for m in json.loads((runtime/'manifest.json').read_text())['models']}
    def retry_cuda(predict,size,on_change):
        while True:
            try:return predict(size)
            except torch.cuda.OutOfMemoryError:
                if size<=1:raise
                gc.collect();torch.cuda.empty_cache();size=max(1,size//2);on_change(size)
    class Frames:
        def __init__(self):self.i=0
        def read(self):
            image=cv2.imread(str(Path(request['frame_dir'])/f'{self.i:08d}.png'));self.i+=1
            return image is not None,image
        def release(self):pass
    import inspect
    if not hasattr(inspect,'getargspec'):inspect.getargspec=inspect.getfullargspec
    for name,value in {'bool':bool,'int':int,'float':float,'complex':complex,'object':object,'unicode':str,'str':str}.items():
        if name not in np.__dict__:setattr(np,name,value)
    torch.set_num_threads(4)
    os.environ['PYOPENGL_PLATFORM']='win32'
    os.environ['WANDB_MODE']='disabled'
    os.environ['YOLO_CONFIG_DIR']=str(project/'yolo_settings')
    # Only pinned/checksummed official weights enter this worker. Upstream
    # Ultralytics 8.2 uses torch.load without the post-2.6 explicit argument.
    os.environ['TORCH_FORCE_NO_WEIGHTS_ONLY_LOAD']='1'
    torch.manual_seed(int(request.get('handflow_seed',0)));np.random.seed(int(request.get('handflow_seed',0)))
    torch.backends.cuda.matmul.allow_tf32=False;torch.backends.cudnn.allow_tf32=False
    root=UPSTREAM/'HandFlow';sys.path.insert(0,str(root));sys.path.insert(0,str(UPSTREAM/'HaMeR'))
    started=time.monotonic()
    keys=np.fromfile(project/'keypoints.f32',dtype=np.float32).reshape(n,133,3)
    actors=np.zeros((n,4),np.float32)
    image=cv2.imread(str(Path(request['frame_dir'])/'00000000.png'))
    H,W=image.shape[:2];keys[:,:,:2]/=min(896/H,1.)
    wrists=keys[:,[9,10]].copy()
    focal=float(request.get('camera_focal_pixels',0))
    if focal<=0:focal=1.2*max(W,H)
    else:focal/=min(896/H,1.)  # Body calibration is expressed on the <=896-high raster.
    K=np.array([[focal,0,W/2],[0,focal,H/2],[0,0,1]],np.float32)
    for i,k in enumerate(keys):
        body=k[:17];good=body[body[:,2]>.3,:2]
        actors[i]=[*good.min(0),*good.max(0)] if len(good)>3 else [0,0,W,H]
    if actors.shape!=(n,4) or wrists.shape!=(n,2,3):raise ValueError('HandFlow observations have the wrong frame count.')
    allboxes=np.zeros((n,2,4),np.float32);valid=np.zeros((n,2),bool);scores=np.zeros((n,2),np.float32)
    # Match the validated independent pipeline: finish detection before loading
    # the large hand networks. Only small boxes/scores survive this phase.
    from ultralytics import YOLO
    detector=YOLO(str(BY_KEY['hand_detector'].path)).to('cuda')
    capture=Frames()
    candidates={}
    try:
        for i in range(n):
            ok,image=capture.read()
            if not ok:raise ValueError('Cannot read HandFlow video frame '+str(i))
            prediction=detector.predict(image,conf=.3,verbose=False,imgsz=640,device=0,half=False)[0].boxes
            allboxes[i],valid[i],scores[i]=assign_hands(prediction.xyxy.cpu().numpy(),prediction.conf.cpu().numpy(),prediction.cls.cpu().numpy(),wrists[i],actors[i])
            if not valid[i].all():
                weak=detector.predict(image,conf=.15,verbose=False,imgsz=640,device=0,half=False)[0].boxes
                candidates[i]=(weak.xyxy.cpu().numpy(),weak.conf.cpu().numpy(),weak.cls.cpu().numpy())
                del weak
            if i%30==0:emit('progress',stage='handflow',current=i,total=n,message='HandFlow: detecting this actor\'s hands')
    finally:capture.release()
    del detector,prediction,image;gc.collect();torch.cuda.empty_cache()
    from handflow_detection import recover
    allboxes,valid,scores,recovered=recover(allboxes,valid,scores,candidates,wrists,request['fps'])
    strong=valid & ~recovered
    del candidates
    from omegaconf import OmegaConf
    from yacs.config import CfgNode
    from model.feature_extractors.image_refiner import FrameCompressor
    cfg=OmegaConf.merge(OmegaConf.load(root/'configs/model.yaml'),OmegaConf.load(root/'configs/inference.yaml'))
    os.environ['HANDFLOW_NORMALIZATION_STATS']=str(BY_KEY['handflow_stats'].path)
    os.environ['HAMER_CKPT']=str(BY_KEY['hand_hamer'].path)
    os.environ['MANO_ROOT']=str(BY_KEY['mano_right'].path.parent)
    # Compress each frame immediately; never cache gigabytes of patch tokens.
    compressor=FrameCompressor(512,input_dim=1280,num_heads=16,num_layers=8).eval()
    state=torch.load(BY_KEY['handflow'].path,map_location='cpu',weights_only=False)
    state=state.get('module',state);state={k.removeprefix('module.'):v for k,v in state.items()}
    compressor.load_state_dict({k.removeprefix('frame_compressor.'):v for k,v in state.items() if k.startswith('frame_compressor.')},strict=True)
    del state;gc.collect();compressor.cuda()
    from hamer.models import HAMER
    from hamer.datasets.vitdet_dataset import ViTDetDataset
    config=CfgNode.load_cfg(BY_KEY['hand_config'].path.read_text(encoding='utf-8'))
    config.MANO.MODEL_PATH=str(BY_KEY['mano_right'].path.parent);config.MANO.MEAN_PARAMS=str(BY_KEY['hand_mean'].path)
    config.MODEL.BBOX_SHAPE=[192,256];config.MODEL.BACKBONE.pop('PRETRAINED_WEIGHTS',None)
    hamer=HAMER(config,init_renderer=False).eval()
    checkpoint=torch.load(BY_KEY['hand_hamer'].path,map_location='cpu',weights_only=False)
    hamer.load_state_dict(checkpoint['state_dict'],strict=True);del checkpoint;gc.collect();hamer.cuda()
    tokens=np.empty((n,2,512),np.float32);landmarks=np.zeros((n,2,21,2),np.float32)
    intrinsics=np.zeros((n,2,4),np.float32)
    hamer_joints=np.zeros((n,2,21,3),np.float32)
    hamer_uv=np.zeros((n,2,21,2),np.float32)
    with torch.inference_mode():empty=compressor(torch.zeros(1,1,192,1280,device='cuda')).cpu().numpy()[0,0]
    tokens[:]=empty
    captured=[];hook=hamer.backbone.register_forward_hook(lambda m,i,o:captured.append(o))
    free,_=torch.cuda.mem_get_info();batch_limit=int(request.get('handflow_batch',2))
    pending=[]
    def execute(entries):
        images=torch.from_numpy(np.stack([e[4]['img'] for e in entries])).cuda()
        def predict(size):
            outputs=[]
            for first in range(0,len(entries),size):
                captured.clear();prediction=hamer({'img':images[first:first+size]})
                feature=captured[-1].flatten(2).transpose(1,2)
                compressed=compressor(feature[None]).squeeze(0).cpu().numpy()
                outputs.extend([(compressed[k],prediction['pred_cam'][k].cpu().numpy(),prediction['pred_keypoints_3d'][k].cpu().numpy(),prediction['pred_keypoints_2d'][k].cpu().numpy()+.5) for k in range(len(compressed))])
                captured.clear();del prediction
            return outputs
        predictions=retry_cuda(predict,min(batch_limit,len(entries)),lambda size:None)
        for (i,side,H,W,item),(token,pc,points,uv_crop) in zip(entries,predictions):
            hamer_joints[i,side]=points;hamer_uv[i,side]=uv_crop
            tokens[i,side]=token;center=item['box_center'];size=float(item['box_size']);scale=256/size
            cx=(float(K[0,2])-center[0])*scale+128;cy=(float(K[1,2])-center[1])*scale+128
            if side==0:cx=256-cx
            intrinsics[i,side]=[K[0,0]*scale,K[1,1]*scale,cx,cy]
            bc=center.copy();sourceK=K.copy()
            if side==0:bc[0]=W-1-bc[0];sourceK[0,2]=W-1-sourceK[0,2]
            focal=config.EXTRA.FOCAL_LENGTH/config.MODEL.IMAGE_SIZE*max(W,H);bs=size*pc[0]+1e-9
            camera=np.array([2*(bc[0]-W/2)/bs+pc[1],2*(bc[1]-H/2)/bs+pc[2],2*focal/bs]);points=points+camera
            uv=points[:,:2]/points[:,2:]*focal+np.array([W/2,H/2]);uv=np.clip(uv/[W,H],0,1)*[W,H]
            landmarks[i,side]=(uv-sourceK[:2,2])/np.array([sourceK[0,0],sourceK[1,1]])*intrinsics[i,side,:2]/256+intrinsics[i,side,2:]/256
    capture=Frames()
    try:
        with torch.inference_mode():
            for i in range(n):
                ok,image=capture.read()
                if not ok:raise ValueError('Cannot read HandFlow video frame '+str(i))
                boxes=allboxes[i]
                sides=np.flatnonzero(valid[i])
                if len(sides):
                    dataset=ViTDetDataset(config,image,boxes[sides],sides.astype(np.float32),rescale_factor=2.)
                    with contextlib.redirect_stdout(io.StringIO()):
                        pending.extend((i,int(side),image.shape[0],image.shape[1],dataset[k]) for k,side in enumerate(sides))
                if len(pending)>=batch_limit:execute(pending);pending=[]
                if i%30==0:emit('progress',stage='handflow',current=i,total=n,message='HandFlow: detecting and encoding this actor\'s hands')
            if pending:execute(pending)
    finally:capture.release();hook.remove();captured.clear()
    del hamer,compressor,pending;gc.collect();torch.cuda.empty_cache()
    # Cached image tokens bypass the frozen image encoder. Do not allocate a
    # second unused HaMeR copy while loading the official temporal denoiser.
    import model.flow_matching.denoiser as architecture
    architecture.HaMeRBackbone=lambda **kwargs:torch.nn.Identity()
    model=architecture.HandPoseDenoiser(cfg).eval()
    state=torch.load(BY_KEY['handflow'].path,map_location='cpu',weights_only=False);state=state.get('module',state)
    state={k.removeprefix('module.'):v for k,v in state.items() if not k.removeprefix('module.').startswith('hamer_backbone.')}
    model.load_state_dict(state,strict=True);del state;gc.collect();model.cuda()
    from utils.inference_utils import split_sequence_into_windows,run_fm_inference_with_overlap
    class WindowBatches(torch.nn.Module):
        def __init__(self):super().__init__();self.network=model;self.limit=16
        def forward(self,x,t,batch):
            def predict(size):
                return torch.cat([self.network(x[i:i+size],t[i:i+size],{k:(v[i:i+size] if torch.is_tensor(v) and v.shape[0]==len(x) else v) for k,v in batch.items()}) for i in range(0,len(x),size)])
            return retry_cuda(predict,min(self.limit,len(x)),lambda size:setattr(self,'limit',size))
    wrapper=WindowBatches();rotations=np.zeros((n,2,15,3,3),np.float32)
    # Weak recovered crops are image evidence for the local residual fit, not
    # trusted temporal feature anchors. Feeding uncertain features to the flow
    # model can perturb otherwise stable neighboring windows.
    temporal_tokens=tokens.copy();temporal_tokens[~strong]=empty
    temporal_landmarks=landmarks.copy();temporal_landmarks[~strong]=0
    temporal_intrinsics=intrinsics.copy();temporal_intrinsics[~strong]=0
    temporal_scores=scores*strong
    from scipy.spatial.transform import Rotation
    poses=[];translations=[];betas_all=[]
    with torch.inference_mode():
        for side in range(2):
            torch.manual_seed(int(request.get('handflow_seed',0)))
            batch=dict(mano_params=torch.zeros(1,n,48),mano_trans=torch.zeros(1,n,3),mano_betas=torch.zeros(1,10),
                padding_mask=torch.zeros(1,n,dtype=torch.bool),images=torch.zeros(1,n,3,1,1,dtype=torch.uint8),
                hamer_landmarks=torch.from_numpy(temporal_landmarks[:,side])[None],crop_intrinsics=torch.from_numpy(temporal_intrinsics[:,side])[None],
                hamer_confidence=torch.from_numpy(temporal_scores[:,side])[None],image_tokens=torch.from_numpy(temporal_tokens[:,side])[None],side=['right'],source=['custom'])
            # Half-window overlap gives boundary frames central context in
            # another window; blend model velocities, never estimator poses.
            windows,_=split_sequence_into_windows(batch,16,8,torch.device('cuda'))
            windows={k:v.cuda() if torch.is_tensor(v) else v for k,v in windows.items()}
            pose,translation,beta=run_fm_inference_with_overlap(wrapper,model,windows,16,8,int(request.get('handflow_steps',3)),torch.device('cuda'),overlap_method='vblend')
            poses.append(pose[:n].cpu().numpy());translations.append(translation[:n].cpu().numpy());betas_all.append(beta[:n].cpu().numpy())
            r=Rotation.from_rotvec(pose[:n,3:].cpu().numpy().reshape(-1,3)).as_matrix().reshape(n,15,3,3)
            if side==0:
                mirror=np.diag([-1.,1.,1.]);r=mirror@r@mirror
            rotations[:,side]=r
            emit('progress',stage='handflow',current=side+1,total=2,message='HandFlow: refining finger motion over time')
    temporal_poses=np.stack(poses,1);betas=np.stack(betas_all,1);window_batch=wrapper.limit
    del wrapper,model;gc.collect();torch.cuda.empty_cache()
    from handflow_image_guidance import refine,observation_confidence
    guidance_scores,ownership=observation_confidence(scores,hamer_uv,wrists,allboxes,intrinsics,K)
    poses,guidance=refine(temporal_poses,betas,hamer_joints,hamer_uv,guidance_scores,BY_KEY['mano_right'].path.parent,
        float(config.EXTRA.FOCAL_LENGTH/config.MODEL.IMAGE_SIZE),
        lambda current,total:emit('progress',stage='handflow',current=current,total=total,message='HandFlow: preserving image-supported finger gestures'))
    rotations=Rotation.from_rotvec(poses[:,:,3:].reshape(-1,3)).as_matrix().reshape(n,2,15,3,3)
    mirror=np.diag([-1.,1.,1.]);rotations[:,0]=mirror@rotations[:,0]@mirror
    if not np.isfinite(rotations).all():raise ValueError('HandFlow produced invalid finger rotations.')
    guidance['ownership_confidence']='ViTPose wrists vs HaMeR projected root; close-hands ambiguity only, no side swapping'
    guidance['ownership_downweighted_frames_LR']=((ownership['reliability']<.9)&valid).sum(0).tolist()
    guidance['ownership_min_reliability_LR']=np.where(valid,ownership['reliability'],1).min(0).tolist()
    np.savez_compressed(project/'model_evidence.npz',rotations=rotations,valid=valid,recovered=recovered,boxes=allboxes,scores=scores,guidance_scores=guidance_scores,ownership_reliability=ownership['reliability'],ownership_wrist_error=ownership['own_wrist_error'],hamer_landmarks=landmarks,crop_intrinsics=intrinsics,camera_K=K,poses=poses,temporal_poses=temporal_poses,hamer_joints=hamer_joints,hamer_uv=hamer_uv,tokens=tokens,translations=np.stack(translations,1),betas=betas)
    return rotations,valid,dict(fp32=True,seed=int(request.get('handflow_seed',0)),window=16,overlap=8,ode_steps=int(request.get('handflow_steps',3)),window_batch=window_batch,image_guidance=guidance,
        crop_batch=batch_limit,seconds=time.monotonic()-started,body_and_wrists_unchanged=True,
        detection_recovery=dict(method='weak YOLO box + own wrist + two strong temporal anchors; image residual only, not temporal feature anchors',recovered_frames_LR=[np.flatnonzero(recovered[:,s]).tolist() for s in range(2)],score_floor=.15,max_anchor_seconds=.35,original_observations_preserved=True),
        missing='Temporal prediction; unavailable detections remain flagged',image_cache='Per-frame 512-D compressed tokens; no full-video image or patch cache')
