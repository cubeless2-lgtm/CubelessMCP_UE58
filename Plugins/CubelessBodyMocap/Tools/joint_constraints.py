"""Optional offline finger constraints in calibrated anatomical frames.

Own implementation. Common UE physical-flat-hand rotations, L/R then
index/middle/pinky/ring/thumb, proximal to distal. No mean-pose correction,
estimator blending, body/wrist edits, or model inference. Profiles are animation
envelopes with slack, not universal clinical ROM. Axes come from the neutral
source skeleton, never from an already malformed estimated pose.
"""
import numpy as np
from scipy.spatial.transform import Rotation as R

VERSION = 'anatomical_finger_post_v4'


def unit(x):
    x = np.asarray(x, float)
    norm = np.linalg.norm(x, axis=-1, keepdims=True)
    if np.any(norm < 1e-7):
        raise ValueError('Degenerate anatomical calibration')
    return x / norm


def anatomical_frames(calibration):
    p = np.asarray(calibration['source_rest_positions'], float).reshape(2, 15, 3)
    wrists = np.asarray(calibration['source_rest_wrists'], float)
    q = np.asarray(calibration['source_rest_rotations'], float).reshape(2, 15, 4)
    frames = np.zeros((2, 15, 3, 3))
    for side in range(2):
        radial = p[side, 0] - p[side, 6]
        distal = p[side, 3] - wrists[side]
        # UE reflected SMPL T-pose: both palms face down. Anatomical side
        # reverses the radial/distal cross product, not the pose quaternions.
        volar = unit(np.cross(radial, distal)) * (1 if side == 0 else -1)
        for j in range(15):
            z = unit(p[side, j+1]-p[side, j] if j % 3 < 2 else p[side, j]-p[side, j-1])
            # Thumb flexion is oblique, toward the ulnar palm, unlike the four
            # long fingers. Use a fixed neutral-palm direction (about 27 deg
            # out of its plane), never a fitted angle from the input clip.
            # This approximate animation axis is not a measured anatomical axis.
            flex = unit(-unit(radial) + .5*volar) if j >= 12 else volar
            x = unit(np.cross(z, flex))  # positive bend takes tip toward palm
            y = unit(np.cross(z, x))
            frames[side, j] = R.from_quat(q[side, j]).as_matrix().T @ np.stack([x, y, z], axis=1)
    if not np.allclose(np.linalg.det(frames), 1, atol=1e-6):
        raise ValueError('Anatomical frame is not a proper rotation')
    return frames


def limits():
    # Flexion, spread, axial twist, degrees relative to neutral source pose.
    # Wide CMC thumb envelope deliberately preserves opposition.
    long = [[[-35, 105], [-35, 35], [-25, 25]],
            [[-15, 125], [-12, 12], [-15, 15]],
            [[-20, 105], [-12, 12], [-15, 15]]]
    thumb = [[[-80, 100], [-75, 75], [-80, 80]],
             [[-30, 100], [-25, 25], [-30, 30]],
             [[-30, 105], [-15, 15], [-20, 20]]]
    a = np.asarray(long*4+thumb, float)
    return np.deg2rad(a[:, :, 0]), np.deg2rad(a[:, :, 1])


def angles_from_matrices(m):
    # R = Rx(bend) Ry(spread) Rz(twist), expressed in anatomical axes.
    # This is not clamping Unreal Rotator X/Y/Z. Direction encodes swing;
    # residual rotation about the longitudinal axis encodes twist.
    spread = np.arcsin(np.clip(m[..., 0, 2], -1, 1))
    bend = np.arctan2(-m[..., 1, 2], m[..., 2, 2])
    twist = np.arctan2(-m[..., 0, 1], m[..., 0, 0])
    return np.stack([bend, spread, twist], axis=-1)


def coupled_limits(angles):
    """Animation envelope: flexed long fingers have less lateral/twist freedom.

    No prescribed curl and no PIP/DIP ratio. Thumb opposition retains its own
    broad envelope. The limits depend on pose, never clip identity or time.
    """
    lo, hi = limits()
    lo = np.broadcast_to(lo, angles.shape).copy(); hi = np.broadcast_to(hi, angles.shape).copy()
    for j in range(12):
        level = j % 3
        bend = np.maximum(angles[...,j,0], 0)
        amount = np.clip(bend/np.deg2rad([90.,100.,85.][level]),0,1)
        # IP joints are primarily hinges. Retain modest oblique-axis slack,
        # rather than giving every phalanx an independent large lateral roll.
        spread = np.deg2rad([35.,6.,6.][level])*(1-amount)+np.deg2rad([8.,3.,3.][level])*amount
        twist = np.deg2rad([15.,8.,8.][level])*(1-amount)+np.deg2rad([7.,4.,4.][level])*amount
        lo[...,j,1]=-spread;hi[...,j,1]=spread
        lo[...,j,2]=-twist;hi[...,j,2]=twist
    return lo,hi


def matrices_from_angles(a):
    return R.from_euler('XYZ', a.reshape(-1, 3)).as_matrix().reshape(*a.shape[:-1], 3, 3)


def soft_project(a, lo, hi, margin=np.deg2rad(4)):
    margin = np.minimum(margin, (hi-lo)*.5)
    out = a.copy()
    low_zone = a < lo + margin
    low = lo + np.maximum(a-lo+margin, 0)**2/(4*margin)
    high_zone = a > hi - margin
    high = hi - np.maximum(hi+margin-a, 0)**2/(4*margin)
    out = np.where(low_zone, low, out)
    out = np.where(high_zone, high, out)
    return np.clip(out, lo, hi)


def rotation_steps(q):
    rot = R.from_quat(q.reshape(-1, 4)).as_matrix().reshape(*q.shape[:-1], 3, 3)
    d = np.swapaxes(rot[:-1], -1, -2) @ rot[1:]
    return np.rad2deg(R.from_matrix(d.reshape(-1, 3, 3)).magnitude()).reshape(q.shape[0]-1, 2, 15)


def constrain(quaternions, calibration, strength=1., fps=30., confidence=None, progress=None):
    q = np.asarray(quaternions, float)
    if q.ndim != 4 or q.shape[1:] != (2, 15, 4) or len(q)<2 or not np.isfinite(q).all():
        raise ValueError('Expected finite N x 2 x 15 x 4 rotations')
    if np.max(abs(np.linalg.norm(q, axis=-1)-1)) > 1e-4:
        raise ValueError('Input quaternions must be normalized')
    if not 0 <= strength <= 1 or not 1 <= fps <= 120:
        raise ValueError('Invalid correction strength/FPS')
    frames = anatomical_frames(calibration)
    matrices = R.from_quat(q.reshape(-1, 4)).as_matrix().reshape(len(q), 2, 15, 3, 3)
    local = np.swapaxes(frames, -1, -2) @ matrices @ frames
    angles = angles_from_matrices(local)
    lo, hi = limits()
    violation = np.maximum(lo-angles, 0)+np.maximum(angles-hi, 0)
    bad = np.max(violation, axis=-1) > 1e-6
    # Resolve +/-pi only through invalid stretches. Valid gestures reanchor
    # immediately, so a rejected spin cannot leave a persistent winding offset.
    for side in range(2):
        for joint in range(15):
            for axis in (0, 2):
                prev = 0.
                for i in range(len(q)):
                    v = angles[i, side, joint, axis]
                    if not lo[joint, axis] <= v <= hi[joint, axis]:
                        v += 2*np.pi*round((prev-v)/(2*np.pi))
                    angles[i, side, joint, axis] = v; prev = v
    coupled_lo,coupled_hi=coupled_limits(angles)
    coupled_bad=np.any((angles<coupled_lo-1e-6)|(angles>coupled_hi+1e-6),axis=-1)
    bounded = soft_project(angles, coupled_lo, coupled_hi)
    temporal = bounded.copy()
    confidence_available = confidence is not None
    if confidence is None:
        confidence = np.ones((len(q), 2))
    confidence = np.asarray(confidence, float)
    if confidence.shape != (len(q), 2) or not np.isfinite(confidence).all():
        raise ValueError('Invalid confidence array')
    confidence = np.clip(confidence, 0, 1)
    flagged = np.zeros((len(q), 2, 15), bool)
    # Short isolated reversals only. Never globally low-pass normal motion.
    # A valid fast gesture is protected unless a matching observation is weak.
    radius = max(1, round(fps/30))
    for gap in (radius, 2*radius):
        for i in range(gap, len(q)-gap):
            left, right = bounded[i-gap], bounded[i+gap]
            anchor_gap = np.linalg.norm(right-left, axis=-1)
            midpoint = (left+right)*.5
            deviation = np.linalg.norm(bounded[i]-midpoint, axis=-1)
            axis_bad=(angles[i]<coupled_lo[i]-1e-6)|(angles[i]>coupled_hi[i]+1e-6)
            weak=confidence[i,:,None]<.5
            uncertain = axis_bad.any(-1) | weak
            mask = uncertain & (anchor_gap < np.deg2rad(18)) & (deviation > np.deg2rad(28))
            weight = np.where(bad[i], .8, .6)[..., None]
            candidate = bounded[i]+weight*(midpoint-bounded[i])
            # A bad spread/twist must not smooth a valid fast flexion gesture.
            # Without weak matching observations only offending axes may change.
            axis_mask=axis_bad|weak[...,None]
            temporal[i] = np.where(mask[..., None]&axis_mask, candidate, temporal[i]);flagged[i] |= mask
    def project(a):
        lower,upper=coupled_limits(a)
        return np.clip(a,lower,upper)
    temporal=project(temporal)
    from joint_contact import refine
    constrained,contact,contact_diagnostics=refine(temporal,calibration,frames,fps,project,progress) if strength else (temporal,dict(applied=False,reason='Zero strength'),{})
    corrected_angles = angles+strength*(constrained-angles)
    corrected = frames @ matrices_from_angles(corrected_angles) @ np.swapaxes(frames, -1, -2)
    result = R.from_matrix(corrected.reshape(-1, 3, 3)).as_quat().reshape(q.shape)
    untouched = (np.max(abs(corrected_angles-angles), axis=-1) < 1e-12) | (strength == 0)
    result[untouched] = q[untouched]
    # Nearest quaternion hemisphere; no discontinuity introduced by q/-q.
    result *= np.where(np.sum(result*q, axis=-1, keepdims=True)<0, -1, 1)
    error = np.rad2deg((R.from_quat(q.reshape(-1,4)).inv()*R.from_quat(result.reshape(-1,4))).magnitude()).reshape(len(q),2,15)
    after = angles_from_matrices(np.swapaxes(frames,-1,-2) @ R.from_quat(result.reshape(-1,4)).as_matrix().reshape(len(q),2,15,3,3) @ frames)
    after_bad = np.any((after<lo-1e-5)|(after>hi+1e-5),axis=-1)
    after_lo,after_hi=coupled_limits(after)
    coupled_after=np.any((after<after_lo-1e-5)|(after>after_hi+1e-5),axis=-1)
    before_step, after_step = rotation_steps(q), rotation_steps(result)
    metrics = dict(version=VERSION,frames=len(q),fps=fps,strength=strength,limits_degrees=np.rad2deg(np.stack([lo,hi],-1)).tolist(),
                   confidence_available=confidence_available,range_violations_before=int(bad.sum()),range_violations_after=int(after_bad.sum()),
                   coupled_violations_before=int(coupled_bad.sum()),coupled_violations_after=int(coupled_after.sum()),same_hand_contact=contact,
                   temporal_flags=int(flagged.sum()),edited_joint_frames=int((error>1e-4).sum()),mean_correction_degrees=float(error.mean()),
                   p95_correction_degrees=float(np.percentile(error,95)),max_correction_degrees=float(error.max()),
                   step_p95_before=float(np.percentile(before_step,95)),step_p95_after=float(np.percentile(after_step,95)),
                   step_max_before=float(before_step.max()),step_max_after=float(after_step.max()),
                   no_frame_shift=True,whole_clip_offline=True,body_wrists_metacarpals_preserved=True,
                   profile_note='Conservative animation envelope, not a universal clinical ROM or collision guarantee')
    return result, metrics, dict(angles_before=angles,angles_after=corrected_angles,correction_degrees=error,temporal_flags=flagged,basis=frames,**contact_diagnostics)
