"""CPU same-hand capsule fit in the unchanged Manny wrist/metacarpal frame.

Own implementation. Capsules approximate inner finger volume, not exact skin.
Knuckle-base pairs and adjacent same-digit segments are excluded. Normal touching
is allowed; no preset gesture, image estimator, wrist or body edits are used.
"""
import time
import numpy as np
from scipy.spatial.transform import Rotation as R


def matrix(q):
    q = np.asarray(q, float)
    return R.from_quat(q.reshape(-1, 4)).as_matrix().reshape(*q.shape[:-1], 3, 3)


def closest(a, b, c, d):
    u, v, w = b-a, d-c, a-c
    aa = np.sum(u*u, -1); bb = np.sum(u*v, -1); cc = np.sum(v*v, -1)
    dd = np.sum(u*w, -1); ee = np.sum(v*w, -1); det = aa*cc-bb*bb
    s = np.clip(np.divide(bb*ee-cc*dd, det, out=np.zeros_like(det), where=det>1e-10), 0, 1)
    t = (bb*s+ee)/np.maximum(cc, 1e-10)
    s = np.where(t<0, np.clip(-dd/np.maximum(aa, 1e-10), 0, 1), s)
    s = np.where(t>1, np.clip((bb-dd)/np.maximum(aa, 1e-10), 0, 1), s)
    t = np.clip(t, 0, 1)
    return a+s[..., None]*u, c+t[..., None]*v


class FingerCapsules:
    def __init__(self, context, basis):
        ctx = context; self.n = len(ctx['input_world_q']); n = self.n
        def both(a): return np.broadcast_to(a, (n,)+a.shape).reshape(n*2, *a.shape[1:]).copy()
        self.basis = both(basis)
        self.local = both(matrix(ctx['source_local_q']).reshape(2, 15, 3, 3))
        self.fixed = both(matrix(ctx['source_parent_q']).reshape(2, 15, 3, 3))
        self.left = both(matrix(ctx['left_q']))
        self.right = both(matrix(ctx['right_q']).reshape(2, 15, 3, 3))
        self.ends = both(np.asarray(ctx['ends'], float).reshape(2, 15, 3))
        self.radii = both(np.asarray(ctx['radii'], float).reshape(2, 15))*.95
        self.positions = np.asarray(ctx['positions'], float)[:, :30].reshape(n*2, 15, 3)
        # Derive the root positions from actual saved target rotations, including
        # animated metacarpals. Do not substitute the calibration neutral meta_q.
        world = matrix(ctx['input_world_q']); wrist = matrix(ctx['wrists_q'])
        wp = np.asarray(ctx['wrists_p'], float); pos = np.asarray(ctx['positions'], float)
        parents = ctx['parents']; points = {}; pending = set(range(38))
        while pending:
            ready = sorted(j for j in pending if parents[j]>=38 or parents[j] in points)
            if not ready: raise ValueError('Cyclic target hand hierarchy')
            for j in ready:
                p = parents[j]
                pp, pr = (wp[:, p-38], wrist[:, p-38]) if p>=38 else (points[p], world[:, p])
                points[j] = pp + (pr @ pos[:, j, :, None])[..., 0]
            pending.difference_update(ready)
        roots = np.stack([points[j] for j in range(30)], 1).reshape(n, 2, 15, 3)
        self.roots = (wrist.swapaxes(-1, -2)[:, :, None] @ (roots-wp[:, :, None])[..., None])[..., 0].reshape(n*2, 15, 3)
        self.pairs = np.array([(i,j) for i in range(15) for j in range(i+1,15)
                               if (i//3!=j//3 and not (i%3==0 and j%3==0))
                               or (i//3==j//3 and j-i==2)])
        self.i, self.j = self.pairs.T
        self.margin = self.radii[:, self.i]+self.radii[:, self.j]-.08
        self.affected=[]
        for j in range(15):
            ma=(self.i//3==j//3)&(self.i%3>=j%3)
            mb=(self.j//3==j//3)&(self.j%3>=j%3)
            ids=np.flatnonzero(ma|mb)
            self.affected.append((ids,ma[ids],mb[ids]))

    def fk(self, angles):
        a = angles.reshape(self.n*2, 15, 3)
        euler = R.from_euler('XYZ', a.reshape(-1,3)).as_matrix().reshape(self.n*2,15,3,3)
        delta = self.basis @ euler @ self.basis.swapaxes(-1,-2)
        source, target, origins, axes = [], [], [], []
        for j in range(15):
            pre = (source[j-1] if j%3 else self.fixed[:,j]) @ self.local[:,j]
            source.append(pre @ delta[:,j]); target.append(self.left @ source[j] @ self.right[:,j])
            origins.append(origins[j-1]+(target[j-1] @ self.positions[:,j,:,None])[...,0] if j%3 else self.roots[:,j])
            basis = self.left @ pre @ self.basis[:,j]
            x,y = a[:,j,0], a[:,j,1]
            ax = basis[:,:,0]
            ay = (basis @ np.stack([np.zeros_like(x),np.cos(x),np.sin(x)],-1)[...,None])[...,0]
            az = (basis @ np.stack([np.sin(y),-np.sin(x)*np.cos(y),np.cos(x)*np.cos(y)],-1)[...,None])[...,0]
            axes.append(np.stack([ax,ay,az],1))
        origins = np.stack(origins,1); axes = np.stack(axes,1)
        vectors = (np.stack(target,1) @ self.ends[...,None])[...,0]
        return origins+.10*vectors, origins+.95*vectors, origins, axes

    def evaluate(self, angles, gradient=False):
        a,b,origins,axes = self.fk(angles)
        p,q = closest(a[:,self.i],b[:,self.i],a[:,self.j],b[:,self.j])
        diff = p-q; distance = np.linalg.norm(diff,axis=-1)
        overlap = np.maximum(self.margin-distance,0)
        # Penalize deep penetration more than broad, shallow normal contact.
        depth=overlap/np.maximum(self.margin,1e-6)
        cost = np.sum(overlap**2*(1+3*depth),-1).reshape(self.n,2)
        if not gradient: return cost, overlap.reshape(self.n,2,-1)
        direction = diff/np.maximum(distance[...,None],1e-8)
        force = -(2*overlap+9*overlap*depth)[...,None]*direction
        grad = np.zeros((self.n*2,15,3))
        for j in range(15):
            ids,ma,mb=self.affected[j]
            ja = np.cross(axes[:,j,None],(p[:,ids]-origins[:,j,None])[:,:,None])*ma[None,:,None,None]
            jb = np.cross(axes[:,j,None],(q[:,ids]-origins[:,j,None])[:,:,None])*mb[None,:,None,None]
            grad[:,j] = np.einsum('bpc,bpac->ba',force[:,ids],ja-jb)
        return cost, overlap.reshape(self.n,2,-1), grad.reshape(self.n,2,15,3)


def refine(angles, context, basis, fps, project, progress=None):
    started = time.perf_counter()
    if 'input_world_q' not in context:
        return angles, dict(applied=False,reason='No target geometry'), {}
    geo = FingerCapsules(context,basis); initial, overlaps = geo.evaluate(angles)
    active = np.max(overlaps,axis=-1)>.03
    details = dict(applied=bool(active.any()),method='same-hand inner capsules + coupled range + pose/temporal residual',
                   proxy_cost_before=float(initial.sum()),proxy_max_before_cm=float(overlaps.max()),
                   active_hand_frames=int(active.sum()),normal_touch_allowance_cm=.08)
    if not active.any():
        details.update(proxy_cost_after=float(initial.sum()),proxy_max_after_cm=float(overlaps.max()),seconds=time.perf_counter()-started)
        return angles,details,dict(self_contact_before=initial,self_contact_after=initial)
    # Let the small correction enter/leave around a contact, rather than forcing
    # it to appear in one frame. The captured pose itself is never time-filtered.
    radius=max(1,round(.15*fps));support=active.copy()
    for gap in range(1,min(radius,len(active)-1)+1):
        support[gap:] |= active[:-gap];support[:-gap] |= active[gap:]
    details.update(support_hand_frames=int(support.sum()),support_radius_frames=radius)
    # Preserve flexion/gesture first; solve sideways interpenetration preferentially.
    weights = np.tile([3.,.35,.6],(15,1)); trust = np.tile(np.deg2rad([12.,30.,20.]),(15,1))
    # Thumb opposition may move out of a folded fist without opening the four
    # long fingers. Its source pose remains the prior, not a preset fist.
    weights[12:]=[1.,.35,.6];trust[12:]=np.deg2rad([25.,30.,20.])
    delta = np.zeros_like(angles); moment=delta.copy(); variance=delta.copy()
    best=angles.copy(); best_score=float(initial.sum()); mask=support[...,None,None]
    def temporal(d):
        scale=(fps/30.)**2; g=np.zeros_like(d); cost=0.
        if len(d)>1:
            v=d[1:]-d[:-1];cost+=12*scale*np.sum(v*v);g[1:]+=24*scale*v;g[:-1]-=24*scale*v
        if len(d)>2:
            v=d[2:]-2*d[1:-1]+d[:-2];cost+=24*scale*np.sum(v*v)
            g[2:]+=48*scale*v;g[1:-1]-=96*scale*v;g[:-2]+=48*scale*v
        return cost,g
    iterations=64
    for iteration in range(iterations):
        if progress and iteration%8==0: progress(iteration,iterations)
        cost,_,g=geo.evaluate(angles+delta,True)
        tc,tg=temporal(delta);score=float(cost.sum()+np.sum(weights*delta**2)+tc)
        if score<best_score: best_score=score;best=angles+delta
        g=(g+2*weights*delta+tg)*mask
        moment=.8*moment+.2*g;variance=.95*variance+.05*g*g
        step=.02*(moment/(1-.8**(iteration+1)))/(np.sqrt(variance/(1-.95**(iteration+1)))+1e-7)
        delta=np.clip(delta-step,-trust,trust)*mask
        delta=(project(angles+delta)-angles)*mask
    # Limit only the additional contact correction, not captured joint motion.
    # A symmetric offline pass prevents collision escape-direction switches
    # from introducing a new snap. Inactive hands remain exact zero anchors.
    speed=np.deg2rad(3.*30./fps)
    def continuous(d, locked=None):
        hold=mask if locked is None else mask&~locked[...,None,None]
        d=d.copy()*hold
        for _ in range(8):
            for indices in (range(1,len(d)),range(len(d)-2,-1,-1)):
                forward=indices.step>0
                for i in indices:
                    anchor=d[i-1 if forward else i+1]
                    v=d[i]-anchor;size=np.linalg.norm(v,axis=-1,keepdims=True)
                    d[i]=(anchor+v*np.minimum(1.,speed/np.maximum(size,1e-10)))*hold[i]
            d=(project(angles+d)-angles)*hold
        # Range projection can slightly reopen a speed violation at a tight
        # bound. Contract only the offending joint's added residual, keeping
        # zero/source as the anchor; never filter its captured trajectory.
        if len(d)>1:
            for _ in range(4):
                peak=np.linalg.norm(np.diff(d,axis=0),axis=-1).max(axis=0,keepdims=True)[...,None]
                d*=np.minimum(1.,speed/np.maximum(peak,1e-10))
                d=(project(angles+d)-angles)*hold
        return d
    # Never accept a worse per-hand/frame collision result. Recheck continuity
    # after reducing rejected residuals, so rollback does not create a snap.
    delta=continuous(best-angles)
    for _ in range(10):
        final,after=geo.evaluate(angles+delta)
        worse=(final>initial+1e-7)|(after.max(-1)>overlaps.max(-1)+.05)
        if not worse.any():break
        delta[worse]*=.5
        delta=continuous(delta)
    # Conflicting frames become fixed range-only anchors. Propagate a smooth
    # correction transition locally, instead of discarding a whole hand/clip.
    locked=np.zeros_like(active)
    for _ in range(10):
        final,after=geo.evaluate(angles+delta)
        worse=(final>initial+1e-7)|(after.max(-1)>overlaps.max(-1)+.05)
        if not worse.any():break
        locked |= worse;delta=continuous(delta,locked)
    final,after=geo.evaluate(angles+delta)
    if np.any(final>initial+1e-6):
        raise ValueError('Contact fit failed its nonregression check; input preserved')
    result=angles+delta;final,after=geo.evaluate(result)
    details.update(proxy_cost_after=float(final.sum()),proxy_max_after_cm=float(after.max()),iterations=iterations,
                   temporal_velocity_weight=12,temporal_acceleration_weight=24,
                   contact_correction_speed_limit_deg_per_second=90,
                   range_only_fallback_hand_frames=int(locked.sum()),
                   max_contact_correction_step_degrees=float(np.rad2deg(np.linalg.norm(np.diff(delta,axis=0),axis=-1)).max()) if len(delta)>1 else 0.,
                   max_axis_correction_degrees=float(np.rad2deg(abs(delta)).max()),seconds=time.perf_counter()-started,
                   residual_hand_frames=int((after.max(-1)>.03).sum()),proxy_note='Inner-volume proxy, not exact skin collision or gesture ground truth')
    if progress: progress(iterations,iterations)
    return result,details,dict(self_contact_before=initial,self_contact_after=final,
                               self_contact_residual_angles=delta,self_contact_max_before=overlaps.max(-1),self_contact_max_after=after.max(-1))
