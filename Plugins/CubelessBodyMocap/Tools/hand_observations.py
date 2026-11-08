"""Image-space hand identity and augmentation; no anatomical pose constraints."""
import numpy as np
from hand_geometry import assess_geometry
from hand_image_support import image_support,select_supported_candidate


def undo_augmentation(uv, world, inverse, width, mirrored, offset):
    uv=np.asarray(uv)@inverse[:,:2].T+inverse[:,2]
    world=np.asarray(world).copy()
    world[:,:2]=world[:,:2]@inverse[:,:2].T
    if mirrored:
        uv[:,0]=width-1-uv[:,0]
        world[:,0]*=-1
    return uv+offset,world


def overlap_requires_anchor(side, frame, last_seen, overlap, max_age):
    # Do not spawn the occluded hand's identity from a single classification
    # flip of the visible hand. It can reacquire when the wrists separate.
    return overlap and frame-last_seen[side]>max_age and frame-last_seen[1-side]<=max_age


def observe_hands(detector, image, k, last_seen, frame, fps, geometry_history=None, diagnostics=None):
    import cv2
    import mediapipe as mp
    height,width=image.shape[:2]
    sizes=[];crops=[]
    for side,wr,base in [(0,9,91),(1,10,112)]:
        hand=k[base:base+21];pts=hand[hand[:,2]>.3,:2]
        if len(pts)>=5:
            center=(pts.min(0)+pts.max(0))/2;size=max(24.,float(np.ptp(pts,axis=0).max()))
        else:
            center=k[wr,:2]+.25*(k[wr,:2]-k[wr-2,:2]);size=max(24.,.45*float(np.linalg.norm(k[wr,:2]-k[wr-2,:2])))
        sizes.append(size);crops.append(center)
    overlap=np.linalg.norm(k[9,:2]-k[10,:2])<1.25*min(sizes)
    pool=[];attempts=0
    for crop_side in range(2):
        if k[9+crop_side,2]<.3:continue
        # Existing scale, wider context, horizontal test, and upside-down test.
        # Identity confidence alone says nothing about 3D pose quality. Only a
        # geometrically usable own observation ends the search. Quarter turns
        # are last-resort retries for edge-on/blurred hands, not pose edits.
        variants=[(1.4,False,0),(2.,False,0),(1.4,True,0),(1.4,False,180),
                  (1.4,False,90),(1.4,False,-90),
                  (2.,True,0),(2.,False,180),(2.,False,90),(2.,False,-90),
                  (2.8,False,0),(2.8,False,180)]
        quality_retry=False
        for rank,(factor,mirrored,angle) in enumerate(variants):
            # Broader context is worth retrying after a concrete bad geometry
            # observation. Do not expand the search for entirely unseen hands.
            if rank>=6 and not quality_retry:break
            attempts+=1
            radius=max(48.,factor*sizes[crop_side]);lo=np.maximum(0,np.floor(crops[crop_side]-radius)).astype(int);hi=np.minimum([width,height],np.ceil(crops[crop_side]+radius)).astype(int)
            if min(hi-lo)<16:continue
            crop=image[lo[1]:hi[1],lo[0]:hi[0]];hh,ww=crop.shape[:2]
            if mirrored:crop=cv2.flip(crop,1)
            transform=cv2.getRotationMatrix2D((ww/2,hh/2),angle,1.)
            if angle:crop=cv2.warpAffine(crop,transform,(ww,hh),borderMode=cv2.BORDER_REFLECT_101)
            result=detector.detect(mp.Image(image_format=mp.ImageFormat.SRGB,data=np.ascontiguousarray(crop[:,:,::-1])))
            own=False
            for j,landmarks in enumerate(result.hand_landmarks):
                category=result.handedness[j][0];label=category.category_name
                if mirrored:label='Left' if label=='Right' else 'Right'
                side=0 if label=='Left' else 1
                if category.score<.8 or k[9+side,2]<.3:continue
                uv=np.array([[p.x*ww,p.y*hh] for p in landmarks]);world=np.array([[p.x,p.y,p.z] for p in result.hand_world_landmarks[j]])
                uv,world=undo_augmentation(uv,world,cv2.invertAffineTransform(transform),ww,mirrored,lo)
                if not np.isfinite(world).all():continue
                cost=float(np.linalg.norm(uv[0]-k[9+side,:2])/sizes[side])
                if cost>.85:continue
                if not overlap and np.linalg.norm(uv[0]-k[10-side,:2])+3<np.linalg.norm(uv[0]-k[9+side,:2]):continue
                if overlap_requires_anchor(side,frame,last_seen,overlap,max(2,round(fps*.2))):continue
                support=image_support(uv,k[91+side*21:112+side*21])
                if support is not None and support['cost']>.2:
                    quality_retry=quality_retry or side==crop_side
                # Wrist proximity alone can select bracelets/forearm patches
                # from the other crop. Reject only a large disagreement with
                # a well-supported independent full hand, not a few noisy tips.
                if support is not None and support['cost']>.8:
                    if diagnostics is not None:
                        diagnostics['image_rejected']=diagnostics.get('image_rejected',0)+1
                    continue
                accepted,geometry = geometry_history.assess(world,side,frame) if geometry_history is not None else assess_geometry(world)[:2]
                if not accepted:
                    quality_retry=quality_retry or side==crop_side
                    if diagnostics is not None:
                        reason=geometry['reason'];counts=diagnostics['counts'];counts[reason]=counts.get(reason,0)+1
                        if diagnostics.get('rejected') is not None:
                            diagnostics['rejected'].append({'frame':frame,'side':side,'crop_factor':factor,
                                'mirrored_input':mirrored,'rotation_degrees':angle,'geometry':geometry,
                                'keypoints2d':uv.tolist(),'world_landmarks':world.tolist()})
                    continue
                meta={'crop_factor':factor,'crop_bounds':[lo.tolist(),hi.tolist()],'crop_side':crop_side,
                      'mirrored_input':mirrored,'rotation_degrees':angle,'handedness':label,
                      'handedness_score':float(category.score),'identity_policy':'chirality_overlap_anchor_v1',
                      'geometry':geometry,'image_support':support}
                pool.append((side,rank,cost,uv,world,meta))
                own=own or (side==crop_side and (support is None or support['cost']<=.2))
            if own:break
    chosen={}
    for side in range(2):
        candidates=[p for p in pool if p[0]==side]
        if candidates:
            entry=select_supported_candidate(candidates);_,rank,cost,uv,world,meta=entry
            chosen[side]=((cost,uv,world,meta),sizes[side])
    if len(chosen)==2 and np.linalg.norm(chosen[0][0][1][0]-chosen[1][0][1][0])<.35*min(sizes):
        del chosen[max(chosen,key=lambda s:chosen[s][0][0])]
    return chosen,attempts
