"""Cubeless image-conditioned refinement of HandFlow fingers.

HaMeR supplies observations and a nuisance projection frame, never replacement
finger rotations. Optimize a residual around HandFlow's physical MANO pose using
2D reprojection, confidence-weighted 3D bone directions, a pose prior and
temporal continuity of the residual. No joint
limits, WiLoR input, mean-pose correction or wrist/body output is involved.
"""
import time


def observation_confidence(scores, uv_crop, wrists, boxes, crop_intrinsics, camera_k):
    """Downweight ambiguous wrist ownership only when the two wrists are close.

    Uses existing ViTPose wrist observations and HaMeR's projected root. It does
    not swap sides or manufacture a detection for the occluded hand.
    """
    import numpy as np
    center = (boxes[:, :, :2]+boxes[:, :, 2:])/2
    size = np.divide(camera_k[0, 0]*256, crop_intrinsics[:, :, 0],
                     out=np.ones_like(scores), where=crop_intrinsics[:, :, 0] > 0)
    uv = (uv_crop*256-128)*size[:, :, None, None]/256
    uv[:, 0, :, 0] *= -1  # Invert the left canonical crop only for image evidence.
    uv += center[:, :, None, :]
    span = np.linalg.norm(uv[:, :, 9]-uv[:, :, 0], axis=-1)
    own = np.linalg.norm(uv[:, :, 0]-wrists[:, :, :2], axis=-1)
    other = np.linalg.norm(uv[:, :, 0]-wrists[:, ::-1, :2], axis=-1)
    separation = np.linalg.norm(wrists[:, 0, :2]-wrists[:, 1, :2], axis=-1)
    wrist_confidence = np.clip(np.minimum(wrists[:, 0, 2], wrists[:, 1, 2]), 0, 1)
    close = np.exp(-.5*(separation[:, None]/np.maximum(span*2, 12))**2)
    ambiguity = 1/(1+np.exp(np.clip((other-own)/np.maximum(span*.3, 3), -30, 30)))
    reliability = (1-.95*close*ambiguity*wrist_confidence[:, None]).astype(scores.dtype)
    effective = scores*reliability
    return effective, dict(reliability=reliability, own_wrist_error=own,
                          other_wrist_error=other, projected_palm_length=span)


def rotation_acceleration(poses):
    """Soft SO(3) acceleration energy; constant fast angular velocity is free.

    No absolute-angle or speed threshold is used. Rotation matrices avoid
    treating equivalent axis-angle branches as discontinuous poses.
    """
    from smplx.lbs import batch_rodrigues
    if len(poses) < 3:
        return poses.sum()*0
    rotations = batch_rodrigues(poses.reshape(-1, 3)).reshape(*poses.shape[:-1], 3, 3)
    velocity = rotations[:-1].transpose(-1, -2) @ rotations[1:]
    return (velocity[1:]-velocity[:-1]).square().mean()


def refine(poses, betas, hamer_joints, hamer_uv, scores, model_root, crop_focal, emit=None):
    import numpy as np
    import torch
    from hamer.models import MANO
    from smplx.lbs import batch_rodrigues

    started = time.perf_counter()
    n = len(poses)
    if poses.shape != (n, 2, 48) or not all(np.isfinite(a).all() for a in
            (poses, betas, hamer_joints, hamer_uv, scores)):
        raise ValueError('Invalid HandFlow image-guidance observations')
    if not np.any(scores > 0):
        return poses.copy(), dict(applied=False, reason='No image observations')
    device = 'cuda'
    model = MANO(model_path=str(model_root), is_rhand=True, use_pca=False,
                 flat_hand_mean=True).to(device).eval()
    model.requires_grad_(False)
    base = torch.tensor(poses[:, :, 3:].reshape(-1, 15, 3), device=device)
    beta = torch.tensor(betas.reshape(-1, 10), device=device)
    count = len(base)
    chunk = 256  # Bound differentiable MANO vertex buffers, including long clips.

    def fk(aa, shape):
        identity = torch.eye(3, device=device).expand(len(aa), 1, 3, 3)
        rotations = batch_rodrigues(aa.reshape(-1, 3)).reshape(-1, 15, 3, 3)
        return model(global_orient=identity, hand_pose=rotations, betas=shape,
                     pose2rot=False).joints

    with torch.no_grad():
        j0 = torch.cat([fk(base[i:i+chunk], beta[i:i+chunk]) for i in range(0, count, chunk)])
        hj = torch.tensor(hamer_joints.reshape(count, 21, 3), device=device)
        uv = torch.tensor(hamer_uv.reshape(count, 21, 2), device=device)
        score = torch.tensor(scores.reshape(count), device=device)
        # Palm-only rigid alignment removes global hand/camera orientation from
        # the finger fitting problem. These parameters are NEVER exported.
        palm = [0, 5, 9, 13, 17]
        src, dst = j0[:, palm], hj[:, palm]
        sc, dc = src.mean(1, keepdim=True), dst.mean(1, keepdim=True)
        u, _, vh = torch.linalg.svd((src-sc).transpose(1, 2) @ (dst-dc))
        sign = torch.ones(count, 3, device=device)
        sign[:, -1] = torch.linalg.det(u @ vh)
        rot = u @ torch.diag_embed(sign) @ vh
        shift = dc - sc @ rot
        # Recover HaMeR's crop camera from its own 3D/2D observations. The
        # calibrated full-video camera is still used by HandFlow conditioning.
        q = (uv-.5) / crop_focal
        a = torch.zeros(count, 21, 2, 3, device=device)
        a[:, :, 0, 0], a[:, :, 1, 1], a[:, :, :, 2] = 1, 1, -q
        rhs = q*hj[:, :, 2:] - hj[:, :, :2]
        # Missing observations are zero-filled and rank deficient. Never solve
        # their camera system, even though their reprojection weight is zero.
        observed = score > 0
        cam = torch.zeros(count, 1, 3, device=device)
        cam[:, :, 2] = 1
        cam[observed] = torch.linalg.lstsq(a[observed].reshape(-1, 42, 3),
            rhs[observed].reshape(-1, 42, 1)).solution.transpose(1, 2)
        scale = (uv[:, 5]-uv[:, 17]).norm(dim=-1).clamp_min(.08)
        total_score = score.sum().clamp_min(1)
        # 2D alone cannot distinguish bent fingers from depth-aligned straight
        # fingers. Use HaMeR's own 3D observations as a weak direction cue.
        # Both hands are still canonical RIGHT MANO here; left mirroring occurs
        # once, downstream. The palm fit above is fixed, not re-estimated from
        # fingers, and no HaMeR joint rotations or wrist rotation are copied.
        children = torch.tensor([2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15, 16, 18, 19, 20], device=device)
        parents = children-1  # HaMeR 21-joint order: wrist, thumb, index, middle, ring, little.
        target_directions = torch.nn.functional.normalize(hj[:, children]-hj[:, parents], dim=-1)
        # Depth estimates become unreliable under overlap/occlusion. Scores
        # already include detection confidence and wrist-ownership reliability.
        # Smoothly suppress this additional cue for uncertain observations;
        # retain the existing 2D/temporal objective and missing-data behavior.
        direction_confidence = ((score-.6)/.2).clamp(0, 1).square()
        direction_weight = .03

    def project(joints, start, end):
        p = joints @ rot[start:end] + shift[start:end] + cam[start:end]
        return p[:, :, :2] / p[:, :, 2:].clamp_min(.1) * crop_focal + .5

    with torch.no_grad():
        before = project(j0, 0, count)
    delta = torch.zeros_like(base, requires_grad=True)
    optimizer = torch.optim.Adam([delta], lr=.025)
    for iteration in range(100):
        optimizer.zero_grad()
        # Each chunk releases its autograd graph immediately; gradients are
        # accumulated before one global update, preserving temporal continuity.
        for i in range(0, count, chunk):
            end = min(i+chunk, count)
            local = fk(base[i:end]+delta[i:end], beta[i:end])
            pred = project(local, i, end)
            aligned = local @ rot[i:end]
            directions = torch.nn.functional.normalize(aligned[:, children]-aligned[:, parents], dim=-1)
            direction_error = (directions-target_directions[i:end]).square().mean((1, 2))
            residual = (pred-uv[i:end])/scale[i:end, None, None]
            error = torch.nn.functional.smooth_l1_loss(
                residual, torch.zeros_like(residual), beta=.15, reduction='none').mean((1, 2))
            (((error+direction_weight*direction_error*direction_confidence[i:end])*score[i:end]).sum()/total_score).backward()
        correction = delta.reshape(n, 2, 15, 3)
        continuity = (correction[1:]-correction[:-1]).square().mean() if n > 1 else delta.sum()*0
        # Residual continuity alone can preserve a sudden change in the base
        # pose. Also penalize changes of final angular velocity, softly and
        # symmetrically in time, while retaining the image evidence term.
        acceleration = rotation_acceleration((base+delta).reshape(n, 2, 15, 3))
        (.03*delta.square().mean() + 3.*continuity + 6.*acceleration).backward()
        optimizer.step()
        if emit and iteration % 25 == 0:
            emit(iteration, 100)
    with torch.no_grad():
        final = base+delta
        joints = torch.cat([fk(final[i:i+chunk], beta[i:i+chunk]) for i in range(0, count, chunk)])
        after = project(joints, 0, count)
        error_before = ((before-uv)/scale[:, None, None]).norm(dim=-1).mean(-1)
        error_after = ((after-uv)/scale[:, None, None]).norm(dim=-1).mean(-1)
        result = poses.copy()
        result[:, :, 3:] = final.cpu().numpy().reshape(n, 2, 45)
        details = dict(applied=True, method='HaMeR 2D/3D observations + HandFlow residual prior',
                       version='confidence_bone_directions_v1',
                       bone_direction_weight=direction_weight,
                       bone_direction_confidence_range=[.6, .8],
                       bone_direction_confidence_power=2,
                       iterations=100, temporal_residual_weight=3., pose_prior_weight=.03,
                       final_rotation_acceleration_weight=6.,
                       reprojection_before=float((error_before*score).sum()/total_score),
                       reprojection_after=float((error_after*score).sum()/total_score),
                       error_units='HaMeR palm width; pseudo-observation, not ground truth',
                       seconds=time.perf_counter()-started, body_wrist_output=False,
                       joint_limits=False, estimator_rotation_blending=False)
    if not np.isfinite(result).all():
        raise ValueError('Non-finite HandFlow image-guidance result')
    return result, details
