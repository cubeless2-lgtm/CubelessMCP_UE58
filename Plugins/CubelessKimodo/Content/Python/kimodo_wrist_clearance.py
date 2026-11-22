"""Keep the retargeted wrists clear and retain SOMA's hand ordering at crossings."""
import math
import unreal as u

SIDES = ("l", "r")
LIMB = ("upperarm", "lowerarm", "hand")


def _copy_skin(component, mesh, options):
    if u.GeometryScript_SceneUtils.copy_mesh_from_component(component, mesh, options, False)[2] != u.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("Could not evaluate target wrist skin")


def _regions(dm, component):
    points = u.GeometryScript_List.convert_vector_list_to_array(u.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)[1])
    triangles = u.GeometryScript_List.convert_triangle_list_to_array(u.GeometryScript_MeshQueries.get_all_triangle_indices(dm, False)[1])
    names = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    groups = {k: set() for k in ("body", "wl", "wr", "al", "ar")}
    def joint(name):
        return component.get_socket_transform(name,u.RelativeTransformSpace.RTS_COMPONENT).translation
    wrists = {s:joint("hand_"+s) for s in SIDES}
    radii = {s:(wrists[s]-joint("lowerarm_"+s)).length()*.30 for s in SIDES}
    for i, p in enumerate(points):
        _, weight, valid = dm.get_largest_vertex_bone_weight(i)
        if not valid:
            continue
        name = names[weight.bone_index]
        if name.startswith(("head","neck","spine","pelvis","clavicle","thigh","calf")):
            groups["body"].add(i)
        elif name.endswith(("_l","_r")) and name.startswith(("lowerarm","upperarm","hand","index","middle","ring","pinky","thumb")):
            s = name[-1]
            groups["a"+s].add(i)
            if not name.startswith("upperarm") and (p-wrists[s]).length() <= radii[s]:
                groups["w"+s].add(i)
    masks = {}
    for region, vertices in groups.items():
        ids = [i for i,t in enumerate(triangles) if t.x in vertices and t.y in vertices and t.z in vertices]
        if not ids:
            raise ValueError("No weighted target surface for Kimodo wrist check: "+region)
        masks[region] = u.GeometryScript_List.convert_array_to_index_list(ids, u.GeometryScriptIndexType.TRIANGLE)
    lateral = joint("upperarm_l")-joint("upperarm_r")
    front = (joint("head")-joint("pelvis")).cross(lateral)
    sign = 1 if front.dot(joint("ball_l")-joint("foot_l")) >= 0 else -1
    return masks, sign


def _scan(clip, component, dm, options, masks, sign, reference, indices, locked):
    pieces = {k:u.DynamicMesh() for k in masks}
    identity = u.Transform()
    hits, offsets = {}, {s:{} for s in SIDES}
    component.play_animation(clip,False)
    component.set_play_rate(0)
    for f in indices:
        u.KimodoSession.evaluate_comparison_frame(component,f/30.)
        joint = component.get_socket_location
        left = (joint("upperarm_l")-joint("upperarm_r")).normal()
        front = (joint("head")-joint("pelvis")).cross(left).normal()*sign
        up = left.cross(front)*sign
        wrist = {s:joint("hand_"+s) for s in SIDES}
        scale = (wrist["l"]-joint("lowerarm_l")).length()
        sep = wrist["l"]-wrist["r"]
        first = min(int(f),len(reference)-1)
        last = min(first+1,len(reference)-1)
        dx, dy, dz = (a+(b-a)*(f-int(f)) for a,b in zip(reference[first],reference[last]))
        depth = sep.dot(front)
        # Compare signed depth only where the projected wrist regions can overlap.
        crossed = abs(dx)<.65 and abs(dz)<.65
        reversed_order = crossed and abs(dy)>.0001 and depth*(1 if dy>0 else -1)<0
        _copy_skin(component,dm,options)
        built = set()
        def intersects(a,b):
            for k in (a,b):
                if k not in built:
                    dm.get_sub_mesh_from_mesh(pieces[k],masks[k])
                    built.add(k)
            return pieces[a].is_intersecting_mesh(identity,pieces[b],identity)[1]
        body = {s:intersects("w"+s,"body") for s in SIDES}
        arm = {s:intersects("w"+s,"a"+other) for s,other in (("l","r"),("r","l"))}
        events = [s+kind for s in SIDES for kind,found in (("_body",body[s]),("_arm",arm[s])) if found]
        if reversed_order:
            events.append("order")
        if not events:
            continue
        hits[f] = events
        step = .12*scale
        for s in SIDES:
            if body[s] and f not in locked[s]:
                # The chest-facing direction rotates with the character.
                toward = front if (wrist[s]-joint("spine_04")).dot(front)>=0 else -front
                offsets[s][f] = toward*step
        if reversed_order and not any(arm.values()):
            lead,trail = ("l","r") if dy>=0 else ("r","l")
            correction = front*min(step,abs(depth)+min(abs(dy)*scale,.8)+.1)
            if f not in locked[lead]:
                offsets[lead][f] = offsets[lead].get(f,u.Vector())+correction
            elif f not in locked[trail]:
                offsets[trail][f] = offsets[trail].get(f,u.Vector())-correction
            continue
        if any(arm.values()) or reversed_order:
            # At a depth tie the source lateral/vertical separation supplies a
            # continuous escape direction without inventing a front hand.
            planar = left*dx+up*dz
            mix = min(1.,abs(dy)/.10)
            direction = (front*(mix if dy>=0 else -mix)+planar.normal()*(1-mix)).normal()
            if direction.length()<.01:
                direction = sep.normal() if sep.length()>.01 else left
            lead, trail = ("l","r") if dy>=0 else ("r","l")
            direction *= 1 if lead=="l" else -1
            room = (wrist[lead]-wrist[trail]).dot(direction)
            if (room<scale*.34 or reversed_order) and f not in locked[lead]:
                offsets[lead][f] = offsets[lead].get(f,u.Vector())+direction*step
            else:
                # Once the two hands are stacked, moving the leading hand farther
                # can drag its elbow through the trailing wrist. Resolve the
                # contact at the wrist that actually touches the opposing limb.
                for s,other in (("l","r"),("r","l")):
                    if not arm[s] or f in locked[s]:
                        continue
                    chain = [joint(n+"_"+other) for n in LIMB]
                    if component.get_bone_index("middle_03_"+other)>=0:
                        chain.append(joint("middle_03_"+other))
                    nearest = []
                    for a,b in zip(chain,chain[1:]):
                        segment = b-a
                        frac = max(0.,min(1.,(wrist[s]-a).dot(segment)/max(.001,segment.dot(segment))))
                        nearest.append(wrist[s]-(a+segment*frac))
                    normal = min(nearest,key=lambda v:v.length())
                    # Resolve across the image plane to retain the signed depth.
                    normal -= front*normal.dot(front)
                    side = 1 if s=="l" else -1
                    source_plane = planar*side
                    escape = normal.normal() if normal.length()>.5 else source_plane.normal()
                    if escape.length()<.1:
                        escape = left*side
                    offsets[s][f] = offsets[s].get(f,u.Vector())+escape*step
                if reversed_order and f in locked[lead] and f not in locked[trail]:
                    offsets[trail][f] = offsets[trail].get(f,u.Vector())-direction*step
    return hits, offsets


def _shift(clip, mesh, offsets, locked, count):
    ext = u.AnimPoseExtensions
    opt = u.AnimPoseEvaluationOptions()
    opt.optional_skeletal_mesh = mesh
    raw = u.AnimPoseEvaluationOptions()
    raw.should_retarget = False
    local, world = u.AnimPoseSpaces.LOCAL,u.AnimPoseSpaces.WORLD
    affected = set()
    tracks = {}
    for s in SIDES:
        if not offsets[s]:
            continue
        names = [stem+"_"+s for stem in LIMB]
        for f in range(count):
            pose = ext.get_anim_pose_at_frame(clip,f,raw)
            keys = [ext.get_bone_pose(pose,n,local) for n in names]
            if f not in locked[s]:
                near = [k for k in range(max(0,f-3),min(count,f+4)) if k in offsets[s]]
            else:
                near = []
            if near:
                # Taper into adjacent clear frames, filtering isolated direction changes.
                weights = [1-abs(f-k)/4 for k in near]
                offset = sum((offsets[s][k]*w for k,w in zip(near,weights)),u.Vector())*(max(weights)/sum(weights))
                pose = ext.get_anim_pose_at_frame(clip,f,opt)
                upper, fore, hand = [ext.get_bone_pose(pose,n,world) for n in names]
                a = fore.translation-upper.translation
                b = hand.translation-fore.translation
                reach = a+b+offset
                axis = reach.normal()
                l1,l2 = a.length(),b.length()
                length = max(abs(l1-l2)+.001,min(l1+l2-.001,reach.length()))
                bend = a-axis*a.dot(axis)
                if bend.length()>.001:
                    along = (l1*l1-l2*l2+length*length)/(2*length)
                    elbow = axis*along+bend.normal()*math.sqrt(max(0.,l1*l1-along*along))
                    qa = a.find_quat_between_vectors(elbow)
                    qb = b.find_quat_between_vectors(axis*length-elbow)
                    for key,t,change in zip(keys,(upper,fore,hand),(qa,qa.inversed()*qb,qb.inversed())):
                        key.rotation = key.rotation*t.rotation.inversed()*change*t.rotation
                    affected.add(f)
            for n,key in zip(names,keys):
                tracks.setdefault(n,[]).append(key)
    ctrl = clip.controller
    ctrl.open_bracket("Preserve Kimodo wrist clearance and crossing order",False)
    try:
        for n,keys in tracks.items():
            if not ctrl.set_bone_track_keys(n,[k.translation for k in keys],[k.rotation for k in keys],[k.scale3d for k in keys],False):
                raise RuntimeError("Could not write wrist correction: "+n)
    finally:
        ctrl.close_bracket(False)
    return affected


def apply(clip, mesh, markers, reference):
    count = clip.data_model_interface.get_number_of_keys()
    if not reference:
        return {"status":"no_source_depth"}
    es = u.get_editor_subsystem(u.EditorActorSubsystem)
    selection = es.get_selected_level_actors()
    actor = es.spawn_actor_from_class(u.SkeletalMeshActor,u.Vector(0,0,20000),transient=True)
    try:
        component = actor.skeletal_mesh_component
        component.set_skeletal_mesh_asset(mesh)
        component.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
        component.set_update_animation_in_editor(True)
        dm = u.DynamicMesh()
        options = u.GeometryScriptCopyMeshFromComponentOptions()
        options.want_normals = options.want_tangents = False
        _copy_skin(component,dm,options)
        masks,sign = _regions(dm,component)
        locked = {s:{i for m in markers if m["effector"]=="hand_"+s for i in range(m["start_frame"],m["end_frame"]+1)} for s in SIDES}
        indices = range(count)
        remaining = {}
        for iteration in range(13):
            hits, offsets = _scan(clip,component,dm,options,masks,sign,reference,indices,locked)
            for f in indices:
                remaining.pop(f,None)
            remaining.update(hits)
            if iteration==0:
                initial = dict(hits)
            if iteration==12 or not any(offsets.values()):
                break
            indices = _shift(clip,mesh,offsets,locked,count)
            if not indices:
                break
        # A front-hand change can pass through the other forearm between two
        # clean keys. Inspect the interpolated pose, then recheck both adjacent
        # intervals whenever either endpoint moved.
        for side in SIDES:
            locked[side].update([f+.5 for f in locked[side] if f+1 in locked[side]])
        indices = {f+.5 for f in range(count-1)}
        for subpass in range(7):
            hits, offsets = _scan(clip,component,dm,options,masks,sign,reference,indices,locked)
            for f in indices:
                remaining.pop(f,None)
            remaining.update(hits)
            if subpass==6 or not any(offsets.values()):
                break
            keys = {s:{} for s in SIDES}
            for s in SIDES:
                for f,offset in offsets[s].items():
                    for k in {int(f),math.ceil(f)}:
                        keys[s].setdefault(k,[]).append(offset)
                keys[s] = {k:sum(v,u.Vector())*(.65/len(v)) for k,v in keys[s].items()}
            touched = _shift(clip,mesh,keys,locked,count)
            indices = {k+d for k in touched for d in (-.5,0,.5) if 0<=k+d<count and k+d<=count-1}
            if not indices:
                break
        return {"status":"checked","frames":count,"initial":initial,"remaining":remaining,
                "passes":iteration,"interpolation_passes":subpass}
    finally:
        es.destroy_actor(actor)
        es.set_selected_level_actors(selection)
