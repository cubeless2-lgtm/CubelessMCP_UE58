"""Limit a generated finger curl where distal skin crosses the mannequin palm."""

import bpy
import numpy as np
from mathutils.bvhtree import BVHTree

DIGITS = ("thumb", "index", "middle", "ring", "pinky")


def capture(rig, scene, frames, sides):
    rotations = {}
    for side in sides:
        for digit in DIGITS:
            joints = [rig.pose.bones.get(f"{digit}_0{i}_{side}") for i in (1, 2, 3)]
            if all(joints):
                rotations[side, digit] = (joints, [])
    for frame in range(frames):
        scene.frame_set(frame)
        for joints, samples in rotations.values():
            samples.append([joint.rotation_quaternion.copy() for joint in joints])
    return rotations


def triangles(faces, mask):
    indices = np.flatnonzero(mask)
    return indices, np.searchsorted(indices, faces[mask[faces].all(axis=1)])


def limit_curls(rig, scene, frames, sides, original):
    meshes = [o for o in scene.objects if o.type == "MESH" and o.find_armature() == rig]
    if not meshes or not sides:
        return
    dg = bpy.context.evaluated_depsgraph_get()
    for mesh in meshes:
        mesh.data.calc_loop_triangles()
        faces = np.array(
            [tuple(t.vertices) for t in mesh.data.loop_triangles], dtype=int
        )
        if not len(faces):
            continue
        groups = [g.name for g in mesh.vertex_groups]
        dom = [
            groups[max(v.groups, key=lambda g: g.weight).group] if v.groups else ""
            for v in mesh.data.vertices
        ]
        positions = np.empty(len(dom) * 3, dtype=np.float32)

        def vertices():
            mesh.evaluated_get(dg).data.vertices.foreach_get("co", positions)
            return positions.reshape(-1, 3)

        def tree(coords, part):
            ids, tri = part
            return BVHTree.FromPolygons(coords[ids], tri, all_triangles=True)

        for side in sides:
            palm = triangles(
                faces,
                np.array(
                    [
                        n == "hand_" + side or n.endswith("metacarpal_" + side)
                        for n in dom
                    ]
                ),
            )
            if not len(palm[1]):
                continue
            parts = {}
            for digit in DIGITS:
                part = triangles(
                    faces,
                    np.array(
                        [n in (f"{digit}_02_{side}", f"{digit}_03_{side}") for n in dom]
                    ),
                )
                if len(part[1]) and (side, digit) in original:
                    parts[digit] = part
            limits = {digit: np.ones(frames) for digit in parts}
            for frame in range(frames):
                scene.frame_set(frame)
                coords = vertices()
                surface = tree(coords, palm)
                affected = [
                    digit
                    for digit, part in parts.items()
                    if surface.overlap(tree(coords, part))
                ]
                for digit in affected:
                    joints, baseline = original[side, digit]
                    desired = [j.rotation_quaternion.copy() for j in joints]

                    def apply(weight):
                        for j, a, b in zip(joints, baseline[frame], desired):
                            j.rotation_quaternion = a.slerp(b, weight)
                        bpy.context.view_layer.update()

                    # Start from the known input pose and stop just before first contact.
                    low, high = 0.0, 1.0
                    for _ in range(7):
                        mid = (low + high) * 0.5
                        apply(mid)
                        coords = vertices()
                        if tree(coords, palm).overlap(tree(coords, parts[digit])):
                            high = mid
                        else:
                            low = mid
                    limits[digit][frame] = max(0.0, low - 0.04)
                    apply(1.0)
            for digit, weights in limits.items():
                if weights.min() >= 1.0:
                    continue
                joints, baseline = original[side, digit]
                for frame in range(frames):
                    weight = min(
                        weights[t] + 0.15 * abs(frame - t)
                        for t in range(max(0, frame - 6), min(frames, frame + 7))
                    )
                    if weight >= 1.0:
                        continue
                    scene.frame_set(frame)
                    for j, a in zip(joints, baseline[frame]):
                        j.rotation_quaternion = a.slerp(j.rotation_quaternion, weight)
                        j.keyframe_insert("rotation_quaternion", frame=frame)
