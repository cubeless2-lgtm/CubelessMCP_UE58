"""Keep a lowered hand outside the same-side thigh before user IK is applied."""
import math

import bpy
from mathutils.bvhtree import BVHTree


def skin_regions(rig, side):
    regions = []
    for mesh in bpy.context.scene.objects:
        if mesh.type != "MESH" or mesh.find_armature() != rig:
            continue
        names = [g.name for g in mesh.vertex_groups]
        dominant = [names[max(v.groups, key=lambda g: g.weight).group] if v.groups else ""
                    for v in mesh.data.vertices]
        hand = {i for i,n in enumerate(dominant) if n.endswith("_"+side)
                and n.startswith(("hand","thumb","index","middle","ring","pinky"))}
        leg = {i for i,n in enumerate(dominant) if n == "pelvis"
               or n.endswith("_"+side) and n.startswith(("thigh","calf"))}
        mesh.data.calc_loop_triangles()
        faces = [tuple(t.vertices) for t in mesh.data.loop_triangles if all(i in leg for i in t.vertices)]
        if faces or hand:
            regions.append((mesh, faces, sorted(hand)))
    return regions


def outward_shift(regions, axis):
    vertices, faces, samples = [], [], []
    for mesh, triangles, hand in regions:
        obj = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        start = len(vertices)
        vertices.extend(obj.matrix_world @ v.co for v in obj.data.vertices)
        faces.extend(tuple(start+i for i in tri) for tri in triangles)
        samples.extend(vertices[start+i] for i in hand)
    if not faces or not samples:
        return 0
    tree = BVHTree.FromPolygons(vertices, faces, all_triangles=True)
    shift = 0.0
    for p in samples:
        # From outside in, find the outer surface at this exact height and depth.
        hit, normal, _, distance = tree.ray_cast(p + axis*40, -axis, 80)
        if hit is not None and normal.dot(axis) > 0.08:
            shift = max(shift, 40.5-distance)
    return shift


def clear_lowered_hands(rig, frames, orient):
    scene = bpy.context.scene
    bones = rig.pose.bones
    linear = rig.matrix_world.to_3x3()
    for side, other in (("l","r"),("r","l")):
        arm, hand, hip, opposite = (bones[n] for n in
            ("upperarm_"+side,"hand_"+side,"thigh_"+side,"thigh_"+other))
        regions = skin_regions(rig, side)
        shifts = []
        axes = []
        for frame in range(frames):
            scene.frame_set(frame)
            axis = linear @ (hip.head-opposite.head)
            axis.z = 0
            axis.normalize()
            axes.append(axis)
            wrist = linear @ (hand.head-hip.head)
            # Restrict clearance to lowered arms alongside the hip, leaving
            # raised, crossed-body and forward-reaching gestures alone.
            arm_length = (linear @ (arm.head-hand.head)).length
            lateral = wrist.dot(axis)
            forward = abs(wrist.x*axis.y-wrist.y*axis.x)
            eligible = (0 < lateral < 35 and abs(wrist.z) < 0.48*arm_length and forward < 30)
            shifts.append(outward_shift(regions,axis) if eligible else 0.0)
        if not any(shifts):
            continue
        for f in range(frames):
            # Extend clearance into adjacent samples so entering/leaving contact
            # does not create an isolated correction key.
            shift = max(shifts[i] * (1-abs(f-i)/4)**2
                        for i in range(max(0,f-3),min(frames,f+4)))
            if shift <= 0:
                continue
            scene.frame_set(f)
            axis = axes[f]
            reach = linear @ (hand.head-arm.head)
            length = reach.length
            lateral = min(length*0.98,reach.dot(axis)+shift)
            perpendicular = (reach-axis*reach.dot(axis)).normalized()
            desired = axis*lateral + perpendicular*math.sqrt(max(0,length**2-lateral**2))
            local = linear.inverted()
            wrist_orientation = hand.matrix.to_quaternion()
            orient(arm,(local@reach).rotation_difference(local@desired))
            orient(hand,wrist_orientation @ hand.matrix.to_quaternion().inverted())
            for bone in (arm,hand):
                bone.keyframe_insert("rotation_quaternion",frame=f)
