"""Bake a SOMA BVH on an exported Quinn reference, optionally solving hand/foot markers."""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

bvh, mesh, out, marker_file, *terminal_key = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 0.01
bpy.ops.import_scene.fbx(filepath=mesh, use_anim=False, ignore_leaf_bones=False)
target, = [o for o in scene.objects if o.type == "ARMATURE"]
target.animation_data_clear()
if target.name.lower() == "root" and "root" not in target.data.bones:
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.mode_set(mode="EDIT")
    roots = [b for b in target.data.edit_bones if b.parent is None]
    root = target.data.edit_bones.new("root")
    root.head,root.tail = (0,0,0),(0,1,0)
    for child in roots:
        child.parent = root
    bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.import_anim.bvh(filepath=bvh, frame_start=0, axis_forward="Z", axis_up="Y",
                        update_scene_fps=True, update_scene_duration=True)
source = bpy.context.object
source_start, source_end = map(round,source.animation_data.action.frame_range)
frames = source_end-source_start+1
if terminal_key and int(terminal_key[0]) == frames:
    # UE's terminal sample holds the last source pose. Let a marker on that
    # sample enter the same solver as any other editable frame.
    frames += 1

if source_start:
    action = source.animation_data.action
    slot = source.animation_data.action_slot
    source.animation_data.action = None
    strip = source.animation_data.nla_tracks.new().strips.new("Source",0,action)
    strip.action_slot = slot
bones = target.pose.bones
required = {"pelvis","head"}
for suffix in ("l","r"):
    required.update(f"{n}_{suffix}" for n in ("clavicle","upperarm","lowerarm","hand","thigh","calf","foot"))
missing = sorted(required.difference(bones.keys()))
if missing:
    raise ValueError(f"Target must use UE mannequin bone names; missing: {missing}")
mapping = {"root":"Root", "pelvis":"Hips", "spine_01":"Spine1", "spine_03":"Spine2",
           "spine_05":"Chest", "neck_01":"Neck1", "neck_02":"Neck2", "head":"Head"}
chains = {}
for suffix, side in [("l","Left"),("r","Right")]:
    for dst, src in [("clavicle","Shoulder"),("upperarm","Arm"),("lowerarm","ForeArm"),
                     ("hand","Hand"),("thigh","Leg"),("calf","Shin"),("foot","Foot"),("ball","ToeBase")]:
        mapping[f"{dst}_{suffix}"] = side + src
    for dst, src in [("thumb","Thumb"),("index","Index"),("middle","Middle"),("ring","Ring"),("pinky","Pinky")]:
        for i in range(1,4):
            j = i if dst == "thumb" else i + 1
            mapping[f"{dst}_0{i}_{suffix}"] = f"{side}Hand{src}{j}"
    for end, mid, top in [("hand","lowerarm","upperarm"),("foot","calf","thigh")]:
        chains[f"{end}_{suffix}"] = (f"{top}_{suffix}",f"{mid}_{suffix}",f"{end}_{suffix}")

doc = json.loads(Path(marker_file).read_text(encoding="utf-8-sig")) if marker_file else {"markers":[]}
fps = scene.render.fps / scene.render.fps_base
if doc.get("schema_version",1) != 1 or abs(doc.get("expected_fps",fps)-fps) > .01:
    raise ValueError("Unsupported schema or frame rate mismatch")
markers = doc["markers"]
ids, occupied = set(), set()
for m in markers:
    start, stop = m["start_frame"], m["end_frame"]
    span = {(m["effector"],f) for f in range(start,stop+1)}
    if (not 0 <= start <= stop < frames or m["effector"] not in chains or m["id"] in ids
        or occupied & span or not 0 <= m.get("weight",1) <= 1):
        raise ValueError(f"Invalid or overlapping marker {m}")
    ids.add(m["id"])
    occupied.update(span)
    for k in ("blend_in_frames","blend_out_frames"):
        if not isinstance(m.get(k,0),int) or m.get(k,0)<0:
            raise ValueError(k)
    if m["mode"] == "pin":
        if m.get("space") != "clip_world" or not 0 <= m["anchor_frame"] < frames:
            raise ValueError(m)
    elif m["mode"] == "offset":
        if m.get("space") != "component" or len(m["offset_cm"]) != 3 or not all(map(math.isfinite,m["offset_cm"])):
            raise ValueError(m)
    else:
        raise ValueError(m)

update = bpy.context.view_layer.update
def orient(bone, rotation):
    # Set only rotation. Decomposing a posed world matrix back onto the
    # bone also writes unkeyed scale/location that accumulate across frames.
    world_rotation = bone.matrix.to_quaternion()
    local_delta = world_rotation.inverted() @ rotation @ world_rotation
    bone.rotation_quaternion = (bone.rotation_quaternion @ local_delta).normalized()
    update()

# FBX and BVH can face opposite directions even after conversion to Blender axes.
# Align the shoulder line in the horizontal plane; retain yaw changes within the clip.
def right_axis(obj, right, left):
    line = obj.matrix_world.to_3x3() @ (obj.data.bones[right].head_local - obj.data.bones[left].head_local)
    return math.atan2(line.y, line.x)

yaw = right_axis(target,"upperarm_r","upperarm_l") - right_axis(source,"RightArm","LeftArm")
heading = Quaternion((0,0,1), yaw)
basis = target.matrix_world.to_quaternion().inverted() @ heading @ source.matrix_world.to_quaternion()
mapped = [(b,source.pose.bones[mapping[b.name]]) for b in bones if b.name in mapping]

# Calibrate each arm segment using joint centers. Imported FBX bone tails and roll
# need not point toward the anatomical child. SOMA rests in T, Quinn in A.
reference = {b.name:b.bone.matrix_local.to_quaternion() for b,s in mapped}
for side in ("r","l"):
    segments = [("clavicle","upperarm"),("upperarm","lowerarm"),("lowerarm","hand"),("hand","middle_01")]
    # Match the anatomical knee, ankle and toe directions across bind poses.
    segments.extend([("thigh","calf"),("calf","foot"),("foot","ball")])
    # Use the anatomical child center for each finger. Quinn finger axes
    # and the relaxed SOMA hand have different bind directions.
    for digit in ("thumb","index","middle","ring","pinky"):
        segments.extend([(f"{digit}_01",f"{digit}_02"), (f"{digit}_02",f"{digit}_03")])
    for parent,child in segments:
        name,tip = f"{parent}_{side}",f"{child}_{side}"
        if name not in bones or tip not in bones:
            continue
        dst = bones[tip].bone.head_local-bones[name].bone.head_local
        src = source.data.bones[mapping[tip]].head_local-source.data.bones[mapping[name]].head_local
        reference[name] = dst.rotation_difference(basis @ src) @ reference[name]
# A single wrist-to-middle direction leaves palm roll undetermined.
def palm_frame(wrist, middle, index, pinky):
    forward = (middle-wrist).normalized()
    normal = forward.cross(index-pinky).normalized()
    across = normal.cross(forward)
    return Matrix((forward,across,normal)).transposed().to_quaternion()

for side in ("r","l"):
    names = [f"{bone}_{side}" for bone in ("hand","middle_01","index_01","pinky_01")]
    if any(n not in bones for n in names):
        continue
    dst_frame = palm_frame(*(bones[n].bone.head_local for n in names))
    src_frame = palm_frame(*(basis @ source.data.bones[mapping[n]].head_local for n in names))
    wrist = bones[names[0]]
    reference[wrist.name] = src_frame @ dst_frame.inverted() @ wrist.bone.matrix_local.to_quaternion()

# The distal phalanges have no child joint on Quinn. The FBX importer
# invents their tails, so estimate the fingertip from its bound skin.
distal_names = {f"{digit}_03_{side}" for digit in ("thumb","index","middle","ring","pinky") for side in ("r","l")}
pads = {name:[] for name in distal_names}
for mesh_object in scene.objects:
    if mesh_object.type != "MESH" or mesh_object.find_armature() != target:
        continue
    to_rig = target.matrix_world.inverted() @ mesh_object.matrix_world
    groups = {g.index:g.name for g in mesh_object.vertex_groups if g.name in distal_names}
    for vertex in mesh_object.data.vertices:
        for group in vertex.groups:
            if group.group in groups and group.weight > 0.7:
                pads[groups[group.group]].append(to_rig @ vertex.co)
finger_tips = {}
for name,points in pads.items():
    if not points:
        continue
    joint = bones[name].bone
    forward = (joint.head_local-joint.parent.head_local).normalized()
    points.sort(key=lambda p:(p-joint.head_local).dot(forward),reverse=True)
    end_points = points[:max(1,len(points)//8)]
    pad = sum(end_points,Vector())/len(end_points)-joint.head_local
    digit,_,side = name.split('_')
    finger_tips[digit,side] = joint.matrix_local.to_quaternion().inverted() @ pad
    src_joint = source.data.bones[mapping[name]]
    tip = source.data.bones[mapping[name][:-1]+"End"]
    reference[name] = pad.rotation_difference(basis @ (tip.head_local-src_joint.head_local)) @ reference[name]
for f in range(frames):
    scene.frame_set(f)
    for b,s in mapped:
        rest = s.bone.matrix_local.to_quaternion()
        delta = s.matrix.to_quaternion() @ rest.inverted()
        desired = basis @ delta @ basis.inverted() @ reference[b.name]
        orient(b, desired @ b.matrix.to_quaternion().inverted())
        if b.name in ("root","pelvis"):
            if b.name == "root":
                shift = heading @ (source.matrix_world.to_3x3() @ (s.head-s.bone.head_local))
            else:
                shift = heading @ (source.matrix_world.to_3x3() @ (s.head-s.bone.head_local-source.pose.bones["Root"].head))
            b.location = (target.matrix_world.to_3x3() @ b.bone.matrix_local.to_3x3()).inverted() @ shift
            b.keyframe_insert("location",frame=f)
            update()
        b.keyframe_insert("rotation_quaternion",frame=f)

# Preserve face-relative gestures across different shoulder widths and limb
# lengths. The elbow follows the source bend plane; the hand keeps its roll.
def span(rig,top,bottom):
    linear = rig.matrix_world.to_3x3()
    return abs((linear @ (rig.data.bones[top].head_local-rig.data.bones[bottom].head_local)).z)

body_scale = span(target,"head","foot_l")/span(source,"Head","LeftFoot")
to_target = target.matrix_world.to_3x3().inverted() @ heading.to_matrix() @ source.matrix_world.to_3x3()
def chest_axes(position):
    left = (position("LeftArm") - position("RightArm")).normalized()
    up = position("Neck1") - position("Hips")
    front = up.cross(left).normalized()
    return left, front, left.cross(front).normalized()

bind = lambda name: source.data.bones[name].head_local
front_sign = 1 if chest_axes(bind)[1].dot(bind("LeftToeBase") - bind("LeftFoot")) >= 0 else -1
forearm_length = (bind("LeftHand") - bind("LeftForeArm")).length
wrist_separation = []
for f in range(frames):
    scene.frame_set(f)
    joint = lambda name: source.pose.bones[name].head
    left, front, up = chest_axes(joint)
    delta = (joint("LeftHand") - joint("RightHand")) / forearm_length
    wrist_separation.append([delta.dot(left), front_sign * delta.dot(front), delta.dot(up)])
    for suffix,side in (("l","Left"),("r","Right")):
        upper,lower,hand = (bones[n+"_"+suffix] for n in ("upperarm","lowerarm","hand"))
        shoulder,elbow,wrist = (source.pose.bones[side+n].head for n in ("Arm","ForeArm","Hand"))
        source_head = source.pose.bones["Head"].head
        clavicle = bones["clavicle_"+suffix]
        upper_rotation = upper.matrix.to_quaternion()
        desired_shoulder = bones["head"].head + body_scale*(to_target @ (shoulder-source_head))
        clav_dir = upper.head-clavicle.head
        target_dir = desired_shoulder-clavicle.head
        # Shoulder width differs across characters. Match elevation in the torso frame
        # while retaining the target clavicle length and source horizontal direction.
        up = (bones["head"].head-bones["pelvis"].head).normalized()
        planar = target_dir-up*target_dir.dot(up)
        if planar.length>1e-5 and clav_dir.length>1e-5:
            height = max(-.9*clav_dir.length,min(.9*clav_dir.length,target_dir.dot(up)))
            target_dir = planar.normalized()*math.sqrt(max(0.,clav_dir.length**2-height**2))+up*height
        if min(clav_dir.length,target_dir.length)>1e-5:
            swing = clav_dir.rotation_difference(target_dir)
            if swing.angle > math.radians(25):
                swing = Quaternion(swing.axis,math.radians(25))
            orient(clavicle,swing)
            orient(upper,upper_rotation @ upper.matrix.to_quaternion().inverted())
            clavicle.keyframe_insert("rotation_quaternion",frame=f)
            upper.keyframe_insert("rotation_quaternion",frame=f)
        origin = upper.head.copy()
        original_upper = lower.head-origin
        original_lower = hand.head-lower.head
        length_a,length_b = original_upper.length,original_lower.length
        target_hand = bones["head"].head + body_scale*(to_target @ (wrist-source_head))
        reach = target_hand-origin
        if reach.length < 1e-5:
            continue
        axis = reach.normalized()
        distance = max(abs(length_a-length_b)+1e-5,min(reach.length,length_a+length_b-1e-5))
        # Project the desired elbow onto the bend circle nearest its
        # original anatomical position, retaining the original bend side.
        source_elbow = (bones["head"].head + body_scale*(to_target @ (elbow-source_head))) - origin
        pole = source_elbow-axis*source_elbow.dot(axis)
        if pole.length < 1e-5:
            pole = original_upper-axis*original_upper.dot(axis)
        if pole.length < 1e-5:
            continue
        along = (length_a**2-length_b**2+distance**2)/(2*distance)
        first = along*axis + math.sqrt(max(0,length_a**2-along**2))*pole.normalized()
        # With unequal segment lengths, an exact wrist constraint can fold the
        # elbow far off its source position. Share the residual between joints,
        # favouring wrist precision for close hand-to-hand gestures.
        separation = (to_target @ (source.pose.bones["LeftHand"].head-source.pose.bones["RightHand"].head)).length
        elbow_error = (first-source_elbow).length
        if source_elbow.length > 1e-5 and elbow_error > 1e-5:
            elbow_goal = source_elbow.normalized()*length_a
            separation_weight = min(1.0,separation/max(length_a,1e-5))
            compromise = 0.65*separation_weight*min(1.0,elbow_error/(0.18*length_a))
            first = first.lerp(elbow_goal,compromise).normalized()*length_a
        wrist_rotation = hand.matrix.to_quaternion()
        orient(upper,original_upper.rotation_difference(first))
        orient(lower,(hand.head-lower.head).rotation_difference(origin+axis*distance-lower.head))
        orient(hand,wrist_rotation @ hand.matrix.to_quaternion().inverted())
        for bone in (upper,lower,hand):
            bone.keyframe_insert("rotation_quaternion",frame=f)



from runpy import run_path
clearance = run_path(str(Path(__file__).with_name("kimodo_thigh_clearance.py")))
clearance["clear_lowered_hands"](target, frames, orient)

# Capture original targets before any correction so pin anchors are order-independent.
base = {}
pin_rotation = {}
for f in range(frames):
    scene.frame_set(f)
    for m in markers:
        base[f,m["effector"]] = bones[m["effector"]].head.copy()
        if m.get("lock_rotation") and m["mode"]=="pin" and f==m["anchor_frame"]:
            pin_rotation[m["id"]]=bones[m["effector"]].matrix.to_quaternion()
errors = {}
for m in markers:
    top,mid,end = [bones[n] for n in chains[m["effector"]]]
    first,last = m["start_frame"],m["end_frame"]
    worst = 0
    for f in range(first,last+1):
        scene.frame_set(f)
        origin = base[f,end.name]
        if m["mode"] == "pin":
            goal = base[m["anchor_frame"],end.name]
        else:
            x,y,z = m["offset_cm"]
            # UE X forward/Y right/Z up -> Blender Y forward/X right/Z up.
            goal = origin + target.matrix_world.to_3x3().inverted() @ Vector((y,x,z))
        w = m.get("weight",1)
        for n,dist in [("blend_in_frames",f-first),("blend_out_frames",last-f)]:
            if m.get(n,0):
                w *= min(1,dist/m[n])
        goal = origin.lerp(goal,w)
        a,b,c = (p.head.copy() for p in (top,mid,end))
        axis = (goal-a).normalized()
        l1,l2 = (b-a).length,(c-b).length
        length = max(abs(l1-l2)+1e-5,min((goal-a).length,l1+l2-1e-5))
        along = (l1*l1-l2*l2+length*length)/(2*length)
        plane = (b-a)-axis*(b-a).dot(axis)
        if plane.length < 1e-6:
            plane = axis.orthogonal()
        elbow = a + along*axis + plane.normalized()*math.sqrt(max(0,l1*l1-along*along))
        if w > 0:
            end_rotation = end.matrix.to_quaternion()
            if m["id"] in pin_rotation:
                end_rotation=end_rotation.slerp(pin_rotation[m["id"]],w)
            orient(top,(b-a).rotation_difference(elbow-a))
            orient(mid,(end.head-mid.head).rotation_difference(goal-mid.head))
            if end.name.startswith("hand_"):
                orient(end,end_rotation @ end.matrix.to_quaternion().inverted())
                end.keyframe_insert("rotation_quaternion",frame=f)
        for p in (top,mid):
            p.keyframe_insert("rotation_quaternion",frame=f)
        worst = max(worst,(target.matrix_world.to_3x3() @ (end.head-goal)).length)
    errors[m["id"]] = worst
recipe = Path(bvh).with_name("recipe.json")
hand_sides = []
joint_counts = {}
finger_markers = run_path(str(Path(__file__).with_name("kimodo_finger_markers.py")))["validate"](doc.get("finger_markers",[]),frames)
if recipe.is_file() or finger_markers:
    settings=json.loads(recipe.read_text(encoding="utf-8-sig")) if recipe.is_file() else {}
    prompt=settings.get("prompt","")
    posing = run_path(str(Path(__file__).with_name("kimodo_hand_pose.py")))
    gesture = posing["gesture"](prompt)
    curl_guard = run_path(str(Path(__file__).with_name("kimodo_palm_clearance.py")))
    guarded_sides = gesture[1] if gesture and gesture[0] in ("fist","point","peace","thumbs_up","pinch") else ()
    if settings.get("handflow") or finger_markers:
        guarded_sides = ("l","r")
    baseline = curl_guard["capture"](target,scene,frames,guarded_sides) if guarded_sides else {}
    hand_sides = posing["pose_hands"](
        prompt, source, target, mapping, scene, frames, fps, orient, finger_tips)

    limits = run_path(str(Path(__file__).with_name("kimodo_joint_limits.py")))
    flow = settings.get("handflow")
    if flow:
        flow = dict(flow)
        flow["fbx"] = str(recipe.parent / flow["fbx"])
    joint_counts = limits["apply"](target,scene,frames,fps,finger_tips,flow,finger_markers)
    if baseline:
        curl_guard["limit_curls"](target,scene,frames,guarded_sides,baseline)
scene.frame_start,scene.frame_end = 0,frames-1
bpy.ops.object.select_all(action="DESELECT")
target.name = "Armature"
target.select_set(True)
bpy.context.view_layer.objects.active = target
bpy.ops.export_scene.fbx(filepath=out,use_selection=True,object_types={"ARMATURE"},add_leaf_bones=False,
                        bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
Path(out).with_suffix(".json").write_text(json.dumps({"frames":frames,"fps":fps,"max_error_cm":errors,"procedural_hand_sides":hand_sides,"joint_limits":joint_counts,"wrist_separation":wrist_separation},indent=2))












