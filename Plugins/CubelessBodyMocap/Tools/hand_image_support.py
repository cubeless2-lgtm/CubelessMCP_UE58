"""Independent 2D evidence for hand-crop selection, not a pose constraint."""
import numpy as np


def image_support(points, body_hand):
    uv=np.asarray(points);k=np.asarray(body_hand)
    if uv.shape!=(21,2) or k.shape!=(21,3) or not np.isfinite(uv).all():return None
    good=np.isfinite(k).all(1)&(k[:,2]>.5)
    if good.sum()<12 or good[[0,5,9,13,17]].sum()<3:return None
    scale=max(24.,float(np.ptp(k[good,:2],axis=0).max()))
    distance=np.linalg.norm(uv[good]-k[good,:2],axis=1)
    return {'cost':float(np.average(distance,weights=k[good,2])/scale),
            'supported_points':int(good.sum()),'scale_pixels':scale}


def select_supported_candidate(candidates):
    """Keep the existing preference unless an alternative materially agrees better."""
    baseline=min(candidates,key=lambda p:(p[1],p[2]))
    reference=baseline[5].get('image_support')
    supported=[p for p in candidates if p[5].get('image_support') is not None]
    if reference is None or not supported:return baseline
    best=min(supported,key=lambda p:(p[5]['image_support']['cost'],p[1],p[2]))
    before=reference['cost'];after=best[5]['image_support']['cost']
    return best if before-after>=.08 and after<=.75*before else baseline
