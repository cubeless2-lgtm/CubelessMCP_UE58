"""Reject unreliable landmark measurements before rotation conversion.

Checks concern 3D basis conditioning and measured bone-length consistency.
No joint-angle limits, preferred curl directions, or replacement poses.
"""
from collections import deque
import numpy as np

SEGMENTS = [(start+i, start+i+1) for start in [5,9,17,13,1] for i in range(3)]


def assess_geometry(world, reference=None):
    p = np.asarray(world, dtype=float)
    if p.shape != (21,3) or not np.isfinite(p).all():
        return False, {'reason':'nonfinite_landmarks'}, None
    lengths = np.array([np.linalg.norm(p[b]-p[a]) for a,b in SEGMENTS])
    scale = float(np.median(lengths))
    across, forward = p[5]-p[17], p[9]-p[0]
    width, depth = np.linalg.norm(across), np.linalg.norm(forward)
    if min(scale, width, depth) < 1e-9:
        return False, {'reason':'collapsed_geometry'}, None
    signature = lengths/scale
    sine = float(np.linalg.norm(np.cross(across,forward))/(width*depth))
    info = {'palm_width_ratio':float(width/depth), 'palm_basis_sine':sine,
            'shortest_segment_ratio':float(signature.min()), 'reason':'accepted'}
    # World landmarks are in 3D: foreshortening in the image must not collapse
    # their palm or bone lengths. Allow broad shape variation, reject loss of
    # usable geometry rather than rotating a collapsed measurement into a pose.
    if width/depth < .25 or sine < .6:
        info['reason'] = 'ill_conditioned_palm'
    elif signature.min() < .25:
        info['reason'] = 'collapsed_segment'
    elif reference is not None:
        ratios = signature/np.asarray(reference)
        info.update(length_ratio_min=float(ratios.min()), length_ratio_max=float(ratios.max()))
        # Rigid motion and finger bending preserve each bone's length. Normalize
        # scale first, so distance/scale errors do not masquerade as deformation.
        # A single moderately noisy depth estimate must not veto the entire
        # hand. Require corroboration from multiple bones, or one collapse far
        # beyond the normal estimator variation.
        inconsistent=(ratios<.45)|(ratios>2.2)
        if np.count_nonzero(inconsistent)>=2 or ratios.min()<.3 or ratios.max()>3.:
            info['reason'] = 'inconsistent_bone_lengths'
    return info['reason']=='accepted', info, signature


class GeometryHistory:
    def __init__(self, fps):
        self.max_age = max(1, round(fps))
        self.samples = [deque(), deque()]

    def _recent(self, side, frame):
        samples = self.samples[side]
        while samples and frame-samples[0][0] > self.max_age:
            samples.popleft()
        return samples

    def assess(self, world, side, frame):
        samples = self._recent(side, frame)
        reference = np.median([signature for _,signature in samples],axis=0) if len(samples)>=5 else None
        accepted, info, _ = assess_geometry(world, reference)
        info['reference_samples'] = len(samples)
        return accepted, info

    def accept(self, world, side, frame):
        # Called only after identity selection, geometry validation and a
        # successful rotation solve. Rejected candidates never update history.
        _, _, signature = assess_geometry(world)
        if signature is not None:
            self._recent(side,frame).append((frame,signature))


def reject_palm_flips(world, image_points, valid, fps):
    """Reject a depth-normal outlier only with agreeing temporal AND image evidence.

    An isolated front/back ambiguity can survive otherwise usable bone lengths.
    A brief missing interval must not make that lone sample the interpolation
    anchor. A persistent turn, moving image pose, or unobserved long gap is not
    repaired by this check. Accepted rotations themselves are never clamped.
    """
    valid = valid.copy()
    rejected = np.zeros_like(valid)
    max_span = max(2, round(fps/3)+1)
    anchors = [0,5,9,13,17]
    def normal(p):
        n = np.cross(p[5]-p[17],p[9]-p[0])
        return n/max(np.linalg.norm(n),1e-12)
    def normalized_image(a):
        # Translation is removed, but rotation is not: a real in-image turn
        # must not be mistaken for unchanged observations. Scale normalization
        # handles differing crop size and small camera/person distance changes.
        a=a[anchors]-a[0];scale=np.linalg.norm(a,axis=1).max()
        return a/scale if np.isfinite(a).all() and scale>=1e-6 else None
    def distance(a,b):return float(np.sqrt(np.mean(np.sum((a-b)**2,axis=1))))
    for side in range(2):
        ids=np.flatnonzero(valid[:,side])
        for p,i,j in zip(ids[:-2],ids[1:-1],ids[2:]):
            if j-p>max_span:continue
            a,b,c=[normal(world[f,side]) for f in [p,i,j]]
            if a@c<=.5 or max(a@b,c@b)>=-.5:continue
            a2,b2,c2=[normalized_image(image_points[f,side]) for f in [p,i,j]]
            if any(x is None for x in [a2,b2,c2]):continue
            expected=a2+(c2-a2)*((i-p)/(j-p))
            if distance(a2,c2)>.35 or distance(b2,expected)>.12:continue
            rejected[i,side]=True
    valid[rejected]=False
    return valid,rejected
