"""Bake flat-floor support from the target mannequin's weighted heel, ball and toe surface."""
import math
from statistics import median

import unreal as u


SIDES = ("l", "r")
PADS = ("heel", "ball", "toe")


def _physical_probes(mesh):
    es = u.get_editor_subsystem(u.EditorActorSubsystem)
    selected = es.get_selected_level_actors()
    actor = es.spawn_actor_from_class(u.SkeletalMeshActor, u.Vector(0, 0, 20000), transient=True)
    try:
        comp = actor.skeletal_mesh_component
        comp.set_skeletal_mesh_asset(mesh)
        comp.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
        names = [str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())]
        if any(f"{bone}_{s}" not in names for s in SIDES for bone in ("ball", "foot", "calf", "thigh")):
            return None
        ref = [comp.get_socket_transform(n, u.RelativeTransformSpace.RTS_COMPONENT) for n in names]
        dm = u.DynamicMesh()
        _, _, result = u.GeometryScript_SceneUtils.copy_mesh_from_component(
            comp, dm, u.GeometryScriptCopyMeshFromComponentOptions(), False)
        if result != u.GeometryScriptOutcomePins.SUCCESS:
            raise ValueError("Could not read the target foot surface")
        positions = u.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)[1]
        vertices = u.GeometryScript_List.convert_vector_list_to_array(positions)
        foot_ids = {s: [] for s in SIDES}
        for i, v in enumerate(vertices):
            _, weight, valid = dm.get_largest_vertex_bone_weight(i)
            if valid:
                n = names[weight.bone_index]
                if n in ("foot_l", "ball_l", "foot_r", "ball_r"):
                    foot_ids[n[-1]].append(i)
        pads, skin = {}, {}
        for s, ids in foot_ids.items():
            if not ids:
                return None
            ankle, ball = [ref[names.index(f"{b}_{s}")].translation for b in ("foot", "ball")]
            forward = ball - ankle
            forward.z = 0
            forward = forward.normal()
            length = (ball - ankle).dot(forward)
            if length < 1:
                return None
            groups = {p: [] for p in PADS}
            for i in ids:
                t = (vertices[i] - ankle).dot(forward) / length
                groups["heel" if t < .48 else "ball" if t < 1.18 else "toe"].append(i)
            pads[s] = {}
            for pad, group in groups.items():
                if not group:
                    return None
                low = min(vertices[i].z for i in group)
                sole = [i for i in group if vertices[i].z < low + max(2., .22 * length)]
                # Support hull samples also track tilted soles. Use a fixed material
                # point near each pad centre for slip, independent of which edge is lowest.
                center = sum((vertices[i] for i in sole), u.Vector()) / len(sole)
                center_id = min(sole, key=lambda i: (vertices[i] - center).length())
                hull = {min(sole, key=lambda i: vertices[i].z), center_id}
                for k in range(12):
                    x, y = math.cos(k*math.pi/6), math.sin(k*math.pi/6)
                    for slope in (.15, .65, 2.0):
                        hull.add(min(group, key=lambda i: vertices[i].z + slope*(x*vertices[i].x+y*vertices[i].y)))
                pads[s][pad] = (center_id, sorted(hull))
                for i in hull:
                    _, weights, valid = dm.get_vertex_bone_weights(i)
                    if not valid:
                        raise ValueError("Missing sole vertex weights")
                    skin[i] = [(names[b.bone_index], b.weight, ref[b.bone_index].inverse_transform_location(vertices[i]))
                               for b in weights if b.weight > 0]
        return pads, skin
    finally:
        es.destroy_actor(actor)
        es.set_selected_level_actors(selected)


def _smooth(t):
    t = min(1., max(0., t))
    return t*t*(3-2*t)


def _xy(v):
    return math.hypot(v.x, v.y)


def _pitch(axis, angle):
    half = math.radians(angle)*.5
    k = math.sin(half)
    return u.Quat(axis.x*k, axis.y*k, axis.z*k, math.cos(half))


def _flat_stance(samples, spans, fps):
    """Fade out forefoot flattening at heel strikes, intentional toe raises and takeoff."""
    count = len(samples)
    supported = [False]*count
    for span in spans:
        for i in range(span["first"],span["last"]+1):
            supported[i] = True
    result = [0.]*count
    ramp = max(1,round(.17*fps))
    for i,frame in enumerate(samples):
        if not supported[i]:
            continue
        heel, ball, toe = [frame[p][1] for p in PADS]
        distance = min((abs(k-i) for k in range(max(0,i-ramp),min(count,i+ramp+1))
                        if not supported[k]),default=ramp+1)
        result[i] = (_smooth((distance-1)/ramp) * _smooth((5.5-(toe-heel))/2.5)
                     * _smooth((.8-(heel-min(ball,toe)))/.6))
    return result


def _settle_pads(world, points, pads, toe_weights, side, gain):
    """Fit the ball and toe pad against the heel using the target's two anatomical pivots."""
    ankle = world["foot_"+side].translation
    ball = world["ball_"+side].translation
    direction = ball-ankle
    axis = u.Vector(-direction.y,direction.x,0).normal()
    indices = set().union(*(hull for _,hull in pads.values()))
    def fit(gap,limit):
        # gap(angle) is locally decreasing with positive (toe-down) pitch.
        lo,hi = -limit,limit
        for _ in range(11):
            mid = (lo+hi)*.5
            if gap(mid) > 0:
                lo=mid
            else:
                hi=mid
        return .5*(lo+hi)*gain
    def pitched_z(i,angle):
        v=points[i]-ankle
        return ankle.z+v.z*math.cos(angle)+(axis.x*v.y-axis.y*v.x)*math.sin(angle)
    def ball_gap(degrees):
        a=math.radians(degrees)
        return min(pitched_z(i,a) for i in pads["ball"][1])-min(pitched_z(i,a) for i in pads["heel"][1])-.045
    ankle_q = _pitch(axis,fit(ball_gap,12.))
    posed = {i:ankle+ankle_q.rotate_vector(points[i]-ankle) for i in indices}
    joint = ankle+ankle_q.rotate_vector(ball-ankle)
    def tip_z(i,angle):
        v=posed[i]-joint
        return posed[i].z+toe_weights[i]*(v.z*(math.cos(angle)-1)+(axis.x*v.y-axis.y*v.x)*math.sin(angle))
    def tip_gap(degrees):
        a=math.radians(degrees)
        return min(tip_z(i,a) for i in pads["toe"][1])-min(tip_z(i,a) for i in pads["ball"][1])-.12
    toe_q = _pitch(axis,fit(tip_gap,16.))
    for i,p in posed.items():
        v=p-joint
        points[i]=p+(toe_q.rotate_vector(v)-v)*toe_weights[i]
    return ankle_q,toe_q


def _support(samples, fps, markers, side):
    count = len(samples)
    protected = [False]*count
    for m in markers:
        if m.get("effector") in (f"foot_{side}", f"ball_{side}"):
            for i in range(max(0, m["start_frame"]), min(count, m["end_frame"]+1)):
                protected[i] = True
    floor = [min(v[1] for v in frame.values()) for frame in samples]
    offsets = [u.Vector(0, 0, 0 if protected[i] else max(0., .045-z)) for i,z in enumerate(floor)]
    labels = []
    for i, frame in enumerate(samples):
        a,b = max(0, i-1), min(count-1, i+1)
        candidates = []
        for pad in PADS:
            speed = _xy(samples[b][pad][0]-samples[a][pad][0])*fps/max(1,b-a)
            if frame[pad][1] <= floor[i]+.65 and speed < 15:
                # Give preference to low, stationary surface points during foot roll.
                candidates.append((speed + 6*(frame[pad][1]-floor[i]), pad))
        labels.append(min(candidates)[1] if candidates and floor[i] < 2.1 and not protected[i] else None)
    spans = []
    i = 0
    ramp = max(1, round(.13*fps))
    while i < count:
        if not labels[i]:
            i += 1
            continue
        a = i
        while i < count and labels[i]:
            i += 1
        b = i-1
        if b-a+1 < max(3, round(.15*fps)):
            continue
        # A planted foot can roll to the toes. Switch pivots only on sustained
        # changes and transfer the previous offset at that instant for continuity.
        active = max(PADS, key=lambda p: labels[a:min(b+1,a+4)].count(p))
        pivot = samples[a][active][0]
        # If a long stance sways within one small footprint, centre the anchor
        # within that footprint. Starting at an extreme needlessly uses up the
        # glide allowance halfway through the stance.
        if b-a >= fps:
            path = [samples[k][active][0] for k in range(a,b+1)]
            lo_x,hi_x = min(p.x for p in path),max(p.x for p in path)
            lo_y,hi_y = min(p.y for p in path),max(p.y for p in path)
            if math.hypot(hi_x-lo_x,hi_y-lo_y) <= 7.5 and any(_xy(p-pivot)>3 for p in path):
                pivot = u.Vector((lo_x+hi_x)*.5,(lo_y+hi_y)*.5,pivot.z)
        start = a
        for f in range(a,b+1):
            nxt = labels[f]
            if nxt != active and (samples[f][active][1] > floor[f]+.9
                                  or labels[f:min(b+1,f+5)].count(nxt) >= 5):
                carried = pivot - samples[f][active][0]
                if f > start:
                    spans.append(dict(first=start,last=f-1,pad=active))
                active, start = nxt, f
                pivot = samples[f][active][0] + carried
            d = pivot - samples[f][active][0]
            # Sustained horizontal travel may be an intentional glide. Bound
            # the correction even on clips too slow to pass the speed gate.
            radial = _xy(d)
            if radial > 3.0:
                d *= 3.0/radial
                pivot = samples[f][active][0]+d
            blend = min(_smooth((f-a)/ramp) if a else 1., _smooth((b-f)/ramp) if b<count-1 else 1.)
            offsets[f] = u.Vector(d.x*blend, d.y*blend,
                                  max(.045-floor[f],0.) + min(.045-floor[f],0.)*blend)
        spans.append(dict(first=start,last=b,pad=active))
    return offsets, spans


def apply(clip, mesh, markers, destination=None, *, save=True):
    geometry = _physical_probes(mesh)
    if not geometry:
        return {"status": "no weighted mannequin sole"}
    pads, skin = geometry
    model = clip.data_model_interface
    count = model.get_number_of_keys()
    fps = (count-1)/clip.get_play_length() if count>1 and clip.get_play_length()>0 else 30.
    ext = u.AnimPoseExtensions
    opt = u.AnimPoseEvaluationOptions()
    opt.optional_skeletal_mesh = mesh
    raw = u.AnimPoseEvaluationOptions()
    raw.should_retarget = False
    tracked = ["pelvis"] + [f"{b}_{s}" for s in SIDES for b in ("thigh","calf","foot","ball")]
    world_names = set(tracked) | {n for weights in skin.values() for n,_,_ in weights}
    poses, keys, local = [], {n:[] for n in tracked}, {n:[] for n in tracked}
    samples = {s:[] for s in SIDES}
    surfaces = []
    def pad_samples(s, points):
        return {p:(points[c], min(points[j].z for j in hull)) for p,(c,hull) in pads[s].items()}
    for i in range(count):
        pose = ext.get_anim_pose_at_frame(clip, i, opt)
        source = ext.get_anim_pose_at_frame(clip, i, raw)
        world = {n:ext.get_bone_pose(pose,n,u.AnimPoseSpaces.WORLD) for n in world_names}
        poses.append(world)
        for n in tracked:
            keys[n].append(ext.get_bone_pose(source,n,u.AnimPoseSpaces.LOCAL))
            local[n].append(ext.get_bone_pose(pose,n,u.AnimPoseSpaces.LOCAL))
        points = {j:sum((world[n].transform_location(v)*w for n,w,v in weights), u.Vector())
                  for j,weights in skin.items()}
        surfaces.append(points)
        for s in SIDES:
            samples[s].append(pad_samples(s,points))
    shifts, spans = {}, {}
    identity = u.Quat(0,0,0,1)
    rolls = {s:[(identity,identity)]*count for s in SIDES}
    for s in SIDES:
        _, initial = _support(samples[s],fps,markers,s)
        toe_weights = {i:sum(w for n,w,_ in weights if n=="ball_"+s) for i,weights in skin.items()}
        for i,gain in enumerate(_flat_stance(samples[s],initial,fps)):
            if gain > .001:
                rolls[s][i]=_settle_pads(poses[i],surfaces[i],pads[s],toe_weights,s,gain)
                samples[s][i]=pad_samples(s,surfaces[i])
        shifts[s], spans[s] = _support(samples[s],fps,markers,s)
    drops = []
    for i,world in enumerate(poses):
        down = 0.
        for s in SIDES:
            hip,knee,ankle = [world[f"{b}_{s}"].translation for b in ("thigh","calf","foot")]
            target = ankle + shifts[s][i]
            length = (knee-hip).length()+(ankle-knee).length()-.015
            dz = math.sqrt(max(0., length*length-_xy(target-hip)**2))
            down = max(down, hip.z-target.z-dz)
        drops.append(min(down, 4.))
    for i,world in enumerate(poses):
        # Ramp the pelvis before full extension instead of popping the knee.
        drop = max(drops[j]*_smooth(1-abs(i-j)/5) for j in range(max(0,i-4),min(count,i+5)))
        for m in markers:
            if m.get("effector"):
                distance = max(m["start_frame"]-i,i-m["end_frame"],0)
                drop *= _smooth(distance/4)
        if drop <= .00001 and all(shifts[s][i].length() < .00001 and rolls[s][i]==(identity,identity) for s in SIDES):
            continue
        shift = u.Vector(0,0,-drop)
        pelvis = world["pelvis"]
        parent = pelvis.rotation * local["pelvis"][i].rotation.inversed()
        src = keys["pelvis"][i]
        # UE's AnimationScaled pelvis may have a different local translation length.
        ratio = src.translation.length()/max(.001,local["pelvis"][i].translation.length())
        src.translation = src.translation + parent.inversed().rotate_vector(shift)*ratio
        for s in SIDES:
            if drop <= .00001 and shifts[s][i].length() < .00001 and rolls[s][i]==(identity,identity):
                continue
            names = [f"{b}_{s}" for b in ("thigh","calf","foot")]
            hip,knee,ankle = [world[n] for n in names]
            thigh,calf = knee.translation-hip.translation, ankle.translation-knee.translation
            baseline = ankle.translation-hip.translation
            reach = baseline + shifts[s][i] - shift
            axis = reach.normal()
            l1,l2 = thigh.length(),calf.length()
            d = max(abs(l1-l2)+1e-5,min(l1+l2-1e-5,reach.length()))
            along = (l1*l1-l2*l2+d*d)/(2*d)
            old_axis = baseline.normal()
            pole = thigh-old_axis*thigh.dot(old_axis)
            if pole.length() < .0001:
                pole = pelvis.rotation.rotate_vector(u.Vector(0,1,0))
                pole -= old_axis*pole.dot(old_axis)
            pole = old_axis.find_quat_between_vectors(axis).rotate_vector(pole.normal())
            upper = axis*along + pole*math.sqrt(max(0.,l1*l1-along*along))
            foot_q,toe_q = rolls[s][i]
            names.append("ball_"+s)
            rotations = [thigh.find_quat_between_vectors(upper)*hip.rotation,
                         calf.find_quat_between_vectors(axis*d-upper)*knee.rotation, foot_q*ankle.rotation,
                         toe_q*foot_q*world[names[-1]].rotation]
            parent = hip.rotation*local[names[0]][i].rotation.inversed()
            for n,rot in zip(names,rotations):
                key = keys[n][i]
                key.rotation = key.rotation*local[n][i].rotation.inversed()*parent.inversed()*rot
                parent = rot
    output = destination or clip
    ctrl = output.controller
    ctrl.open_bracket("Plant heel, forefoot and toes on the floor",False)
    try:
        for n,track in keys.items():
            if not output.data_model_interface.is_valid_bone_track_name(n):
                ctrl.add_bone_curve(n,False)
            if not ctrl.set_bone_track_keys(n,[t.translation for t in track],[t.rotation for t in track],
                                            [t.scale3d for t in track],False):
                raise RuntimeError(f"Could not write foot support track {n}")
    finally:
        ctrl.close_bracket(False)
    if save and not u.EditorAssetLibrary.save_loaded_asset(output):
        raise RuntimeError("Could not save grounded animation")
    return dict(status="applied", frames=count, support=spans,
                pad_center_vertices={s:{p:ids[0] for p,ids in pads[s].items()} for s in SIDES})
