"""Physical flat-hand SMPL/MANO local rotations -> existing UE hand contract.

Side order L,R; joints index/middle/pinky/ring/thumb, three proximal-to-distal
segments each. MANO wrist/global orientation is excluded. No mean is added:
HandFlow denormalizes its learned statistics itself and uses flat_hand_mean=True.
WiLoR already returns physical rotation matrices. Left mirroring happens in the
model adapter once, before this reflection C(x,y,z)=(x,z,y).
"""
def smpl_quaternions_to_ue(quaternions):
    import numpy as np
    q = np.asarray(quaternions)
    if q.ndim != 4 or q.shape[1:] != (2, 15, 4) or not np.isfinite(q).all():
        raise ValueError('Expected finite N x 2 x 15 x 4 xyzw local rotations')
    if np.max(np.abs(np.linalg.norm(q, axis=-1)-1)) > 1e-4:
        raise ValueError('Hand rotations must be normalized')
    ue = q.copy()
    ue[..., :3] = -q[..., [0, 2, 1]]
    return ue
