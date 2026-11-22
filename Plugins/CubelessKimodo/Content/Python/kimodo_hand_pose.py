"""Add explicit hand poses to generated clips whose source hand is stationary."""

import math
import re

from mathutils import Quaternion, Vector

DIGITS = ("thumb", "index", "middle", "ring", "pinky")


def gesture(prompt):
    text = prompt.casefold()
    if re.search(
        r"\b(no|not|without|never|don't)\b|않|말고|손가락.{0,8}고정|fingers? (still|fixed|stationary)",
        text,
    ):
        return ("unchanged", (), False, False)
    for name, terms in (
        ("open_palm", ("open palms", "open palm", "손바닥을 펴", "손바닥을 펼")),
        ("pinch", ("pinch", "ok circle", "집기", "집었다", "오케이")),
        (
            "point",
            ("point with", "pointing with", "point the", "검지로 가리", "검지로 가르"),
        ),
        ("peace", ("peace sign", "v sign", "브이")),
        ("thumbs_up", ("thumbs-up", "thumbs up", "엄지 척", "엄지를 올")),
        ("fist", ("fist", "주먹")),
    ):
        if any(term in text for term in terms):
            left = bool(re.search(r"\bleft\b|왼", text))
            right = bool(re.search(r"\bright\b|오른", text))
            both = (left and right) or any(
                t in text for t in ("both", "alternate", "양손", "두 손", "번갈")
            )
            sides = ("l", "r") if both else (("l",) if left else ("r",))
            repeat = any(t in text for t in ("repeated", "alternate", "반복", "번갈"))
            alternate = "alternate" in text or "번갈" in text
            return name, sides, repeat, alternate
    return None


def pose_hands(prompt, source, target, mapping, scene, frames, fps, orient, tips):
    request = gesture(prompt)
    if not request:
        request = ("relaxed", ("l", "r"), False, False)
    name, sides, repeat, alternate = request
    changed = []
    for side in sides:
        bones = target.pose.bones
        names = [f"{digit}_0{i}_{side}" for digit in DIGITS for i in (1, 2, 3)]
        if any(n not in mapping or n not in bones for n in names):
            continue
        if any((digit, side) not in tips for digit in DIGITS[1:]):
            continue
        # Do not override authored local keys, even if only one digit moves.
        original = [source.pose.bones[mapping[n]] for n in names]
        scene.frame_set(0)
        rest = [b.matrix_basis.to_quaternion() for b in original]
        for frame in range(1, frames):
            scene.frame_set(frame)
            if any(
                abs(q.dot(b.matrix_basis.to_quaternion())) < 0.999999
                for q, b in zip(rest, original)
            ):
                break
        else:
            changed.append(side)
            bend_state = [[0.0]*3 for _ in range(4)]
            previous_position = None
            previous_rotation = None
            response = 0.0
            for frame in range(frames):
                scene.frame_set(frame)
                time = frame / fps
                w = min(1.0, time / 0.32)
                w = w * w * (3 - 2 * w)
                amount = (
                    0.5 - 0.5 * math.cos(2 * math.pi * time / 1.8) if repeat else 1.0
                )
                if repeat and alternate and side == "r":
                    amount = 1 - amount
                hand = bones[f"hand_{side}"]

                def chain(digit):
                    return [bones[f"{digit}_0{i}_{side}"] for i in (1, 2, 3)]

                forward = (chain("middle")[0].head - hand.head).normalized()
                across = chain("index")[0].head - chain("pinky")[0].head
                normal = forward.cross(across).normalized()
                # Infer the palm-facing side from the untouched relaxed finger arc.
                curve = sum(
                    (chain(d)[2].head - chain(d)[0].head for d in DIGITS[1:]), Vector()
                )
                if normal.dot(curve) < 0:
                    normal.negate()
                if name == "relaxed":
                    # Filter wrist speed relative to the torso. A stationary arm keeps its original pose.
                    torso = bones["pelvis"]
                    position = torso.matrix.to_quaternion().inverted() @ (
                        hand.head - torso.head
                    )
                    palm_length = (chain("middle")[0].head - hand.head).length
                    speed = (
                        (position - previous_position).length
                        * fps
                        / max(palm_length, 0.0001)
                        if previous_position is not None
                        else 0.0
                    )
                    wrist_rotation = (
                        torso.matrix.to_quaternion().inverted()
                        @ hand.matrix.to_quaternion()
                    )
                    angular_speed = 0.0
                    if previous_rotation is not None:
                        cosine = min(1.0, abs(previous_rotation.dot(wrist_rotation)))
                        angular_speed = 2.0 * math.acos(cosine) * fps
                    previous_rotation = wrist_rotation
                    travel = position-previous_position if previous_position is not None else position*0
                    palm_normal = torso.matrix.to_quaternion().inverted() @ normal
                    palm_across = torso.matrix.to_quaternion().inverted() @ across.normalized()
                    velocity = travel * (fps/max(palm_length,.0001))
                    previous_position = position.copy()
                    activity = max(0.0, speed - 0.25) + 0.75 * angular_speed
                    drive = activity / (activity + 5.0)
                    # Track signed motion through the palm, with proximal motion leading the fingertips.
                    for i,digit in enumerate(DIGITS[1:]):
                        incoming = drive + .52*math.tanh(velocity.dot(palm_normal)*.3)
                        incoming += (i-1.5)*.13*math.tanh(velocity.dot(palm_across)*.3)
                        joints = chain(digit)
                        for j,b in enumerate(joints):
                            lag = .043+.017*i+.025*j
                            bend_state[i][j] += (incoming-bend_state[i][j])*(-math.expm1(-1/(fps*lag)))
                            incoming = bend_state[i][j]
                            degrees = (4+i)*incoming*(1,.68,.38)[j]
                            direction = joints[j+1].head-b.head if j<2 else b.matrix.to_quaternion() @ tips[digit,side]
                            axis = direction.cross(normal)
                            if axis.length>.0001 and abs(degrees)>.0001:
                                orient(b,Quaternion(axis, math.radians(degrees)))
                                b.keyframe_insert("rotation_quaternion",frame=frame)
                    continue
                if name == "pinch":
                    if ("thumb", side) not in tips:
                        continue
                    pair = [chain(d) for d in ("thumb", "index")]

                    def fingertip(joints, digit):
                        end = joints[-1]
                        return end.head + end.matrix.to_quaternion() @ tips[digit, side]

                    start = [
                        fingertip(joints, d)
                        for joints, d in zip(pair, ("thumb", "index"))
                    ]
                    contact = start[0].lerp(start[1], 0.48)
                    # Keep contact inside both reach spheres, including for short mannequin thumbs.
                    for _ in range(3):
                        for joints, tip in zip(pair, start):
                            segments = [j.head.copy() for j in joints] + [tip]
                            reach = 0.92 * sum(
                                (b - a).length for a, b in zip(segments, segments[1:])
                            )
                            offset = contact - joints[0].head
                            if offset.length > reach:
                                contact = joints[0].head + offset.normalized() * reach
                    for n, (joints, digit) in enumerate(zip(pair, ("thumb", "index"))):
                        goal = start[n].lerp(contact, w * amount)
                        for _ in range(10):
                            tip = fingertip(joints, digit)
                            if (goal - tip).length < 0.04:
                                break
                            for joint in joints:
                                tip = fingertip(joints, digit)
                                delta = (tip - joint.head).rotation_difference(
                                    goal - joint.head
                                )
                                delta.angle = min(delta.angle, math.radians(9))
                                orient(joint, delta)
                        for joint in joints:
                            joint.keyframe_insert("rotation_quaternion", frame=frame)
                    continue
                if name == "open_palm":
                    upper = bones[f"upperarm_{side}"].head
                    elbow = bones[f"lowerarm_{side}"].head
                    length = (elbow - upper).length + (hand.head - elbow).length
                    extension = (hand.head - upper).length / max(length, 0.0001)
                    drive = max(0.0, min(1.0, (extension - 0.45) / 0.5))
                    response += (drive - response) * (1 - math.exp(-1 / (fps * 0.12)))
                    w *= 0.3 + 0.7 * response
                for digit in DIGITS[1:]:
                    joints = chain(digit)
                    extended = (
                        name == "open_palm"
                        or (name == "point" and digit == "index")
                        or (name == "peace" and digit in ("index", "middle"))
                    )
                    bend = 0.0 if extended else amount
                    ray = joints[1].head - joints[0].head
                    flat = (ray - normal * ray.dot(normal)).normalized()
                    side_offset = (DIGITS.index(digit) - 2.5) * 2
                    for index, b in enumerate(joints):
                        distal = (
                            joints[index + 1].head - b.head
                            if index < 2
                            else (b.matrix.to_quaternion() @ tips[digit, side])
                        )
                        degrees = (4, 8, 12)[index] + bend * (
                            (57, 132, 189)[index] + side_offset
                        )
                        angle = math.radians(degrees)
                        desired = flat * math.cos(angle) + normal * math.sin(angle)
                        rotation = distal.rotation_difference(desired)
                        rotation.angle *= w
                        orient(b, rotation)
                        b.keyframe_insert("rotation_quaternion", frame=frame)
                # Tuck the thumb across the folded fingers while retaining its natural joint axes.
                if name in ("fist", "peace", "point"):
                    thumb = chain("thumb")
                    aim = chain("index")[1].head - thumb[0].head
                    swing = (thumb[1].head - thumb[0].head).rotation_difference(aim)
                    swing.angle *= 0.55 * amount * w
                    orient(thumb[0], swing)
                    thumb[0].keyframe_insert("rotation_quaternion", frame=frame)
                elif name == "thumbs_up" and ("thumb", side) in tips:
                    thumb = chain("thumb")
                    outward = (across - forward * across.dot(forward)).normalized()
                    aim = (outward * 0.86 + forward * 0.5 + normal * 0.08).normalized()
                    for i, b in enumerate(thumb):
                        segment = (
                            thumb[i + 1].head - b.head
                            if i < 2
                            else b.matrix.to_quaternion() @ tips["thumb", side]
                        )
                        swing = segment.rotation_difference(aim)
                        swing.angle *= w * (0.65 if i == 0 else 0.9)
                        orient(b, swing)
                        b.keyframe_insert("rotation_quaternion", frame=frame)
    return changed

