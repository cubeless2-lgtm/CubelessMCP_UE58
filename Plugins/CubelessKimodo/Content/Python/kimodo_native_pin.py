"""Remove hand-pin drift caused by the target mesh's import-time retargeting."""
import math

import unreal as u


def refine(clip, mesh, markers, knob=None, *, save=True):
    pins = [m for m in markers if m.get("mode") == "pin" and m.get("lock_rotation")
            and m.get("weight", 1) == 1 and m["effector"] in ("hand_l", "hand_r")]
    if not pins:
        return
    ext = u.AnimPoseExtensions
    options = u.AnimPoseEvaluationOptions()
    options.optional_skeletal_mesh = mesh
    count = clip.data_model_interface.get_number_of_keys()
    poses = [ext.get_anim_pose_at_frame(clip, i, options) for i in range(count)]
    raw_options = u.AnimPoseEvaluationOptions()
    raw_options.should_retarget = False
    raw_poses = [ext.get_anim_pose_at_frame(clip, i, raw_options) for i in range(count)]
    tracks = {}
    for marker in pins:
        side = marker["effector"][-1]
        names = [f"{part}_{side}" for part in ("upperarm", "lowerarm", "hand")]
        for name in names:
            if name not in tracks:
                tracks[name] = [ext.get_bone_pose(p, name, u.AnimPoseSpaces.LOCAL) for p in raw_poses]
        a, b = marker["start_frame"], marker["end_frame"]
        first = a + marker.get("blend_in_frames", 0)
        last = b - marker.get("blend_out_frames", 0)
        if first > last:
            continue
        def world(i, name):
            return ext.get_bone_pose(poses[i], name, u.AnimPoseSpaces.WORLD)
        anchor = world(marker["anchor_frame"], names[2])
        identity = u.Quat(0, 0, 0, 1)
        for i in range(a, b + 1):
            weight = min(1., (i-a)/max(1,first-a) if first>a else 1.,
                         (b-i)/max(1,b-last) if last<b else 1.)
            if weight == 0:
                continue
            reference = world(min(last, max(first, i)), names[2])
            offset = (anchor.translation - reference.translation) * weight
            if knob and (i < first or i > last):
                key = "approach_offset_cm" if i < first else "release_offset_cm"
                offset += u.Vector(*knob.get(key, (0, 0, 0))) * math.sin(math.pi * weight)
            upper, fore, hand = [world(i, name) for name in names]
            old_upper = fore.translation - upper.translation
            old_fore = hand.translation - fore.translation
            reach = hand.translation + offset - upper.translation
            axis = reach.normal()
            l1, l2 = old_upper.length(), old_fore.length()
            distance = max(abs(l1-l2)+1e-5,min(l1+l2-1e-5, reach.length()))
            along = (l1*l1 - l2*l2 + distance*distance)/(2*distance)
            bend = old_upper-axis*old_upper.dot(axis)
            if bend.length() < 1e-6:
                continue
            upper_vec = axis*along + bend.normal()*math.sqrt(max(0.,l1*l1-along*along))
            upper_delta = old_upper.find_quat_between_vectors(upper_vec)
            fore_delta = old_fore.find_quat_between_vectors(axis*distance-upper_vec)
            wrist_delta = identity.slerp_quat(anchor.rotation * reference.rotation.inversed(), weight)
            rotations = [upper_delta*upper.rotation, fore_delta*fore.rotation, wrist_delta*hand.rotation]
            local = [ext.get_bone_pose(poses[i], n, u.AnimPoseSpaces.LOCAL).rotation for n in names]
            parent = upper.rotation*local[0].inversed()
            for name, rotation, previous in zip(names, rotations, local):
                key = tracks[name][i]
                key.rotation = key.rotation * previous.inversed() * parent.inversed() * rotation
                parent = rotation
    controller = clip.controller
    controller.open_bracket("Retain Kimodo hand contact on the target mesh", False)
    try:
        for name, keys in tracks.items():
            if not controller.set_bone_track_keys(name, [k.translation for k in keys],
                    [k.rotation for k in keys], [k.scale3d for k in keys], False):
                raise RuntimeError(f"Could not update hand pin track {name}")
    finally:
        controller.close_bracket(False)
    if save:
        u.EditorAssetLibrary.save_loaded_asset(clip)
