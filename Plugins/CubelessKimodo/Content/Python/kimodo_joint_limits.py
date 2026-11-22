"""Quinn bind-space limits. Degree bounds are animation defaults relative to bind pose."""
from math import radians
from pathlib import Path
import bpy
from mathutils import Vector, Quaternion

DIGITS = ("index","middle","ring","pinky","thumb")


def frame_axes(rig, side, tips):
    bones = rig.data.bones
    def origin(stem):
        return bones[stem+"_"+side].head_local
    forward = (origin("middle_01")-origin("hand")).normalized()
    across = (origin("index_01")-origin("pinky_01")).normalized()
    normal = forward.cross(across).normalized()
    if normal.dot(origin("middle_03")-origin("middle_01"))<0:
        normal.negate()
    axes = {}
    for digit in DIGITS:
        for i in (1,2,3):
            bone=bones[f"{digit}_0{i}_{side}"]
            inv=bone.matrix_local.to_quaternion().inverted()
            line=(origin(f"{digit}_0{i+1}")-bone.head_local if i<3 else bone.matrix_local.to_quaternion()@tips[digit,side]).normalized()
            inward=(origin("ring_01")-bone.head_local).normalized() if digit=="thumb" else normal
            flex=line.cross(inward).normalized()
            axes[bone.name]=(inv@flex,inv@line.cross(flex).normalized(),inv@line)
    wrist=bones["hand_"+side]
    inv=wrist.matrix_local.to_quaternion().inverted()
    flex=forward.cross(normal).normalized()
    axes[wrist.name]=(inv@flex,inv@forward.cross(flex).normalized(),inv@forward)
    return axes


def bounded(q, axes, bounds):
    q=q.normalized()
    if q.w<0:q.negate()
    v=q.to_exponential_map()
    limited=Vector((0,0,0))
    for axis,(low,high) in zip(axes,bounds):
        limited+=axis*max(radians(low),min(radians(high),v.dot(axis)))
    return Quaternion(limited)


def apply(rig,scene,frames,fps,tips,flow=None,markers=()):
    sides=[s for s in ("l","r") if all((d,s) in tips for d in DIGITS)]
    axes={n:v for s in sides for n,v in frame_axes(rig,s,tips).items()}
    if not axes:return {}
    donor=None
    if flow:
        existing=set(scene.objects)
        bpy.ops.import_scene.fbx(filepath=str(Path(flow["fbx"]).resolve(strict=True)),use_anim=True,anim_offset=0)
        imported=set(scene.objects)-existing
        donor,=[o for o in imported if o.type=="ARMATURE"]
        keys=flow["source_frames"];weight=float(flow["weight"])
        if len(keys)!=frames or not 0<=weight<=1:raise ValueError("Invalid HandFlow timing or weight")
        first,last=donor.animation_data.action.frame_range
        if any(not first<=k<=last for k in keys):raise ValueError("HandFlow frame outside donor clip")
        tracks={}
        for f in keys:
            scene.frame_set(int(f),subframe=f-int(f))
            tracks[f]={n:donor.pose.bones[n].matrix_basis.to_quaternion() for n in axes if not n.startswith("hand_")}
        for o in imported:bpy.data.objects.remove(o,do_unlink=True)
    from runpy import run_path
    marker_targets=run_path(str(Path(__file__).with_name("kimodo_finger_markers.py")))["targets"]
    previous={};counts={"wrist":0,"base":0,"after_flow":0,"speed":0}
    for f in range(frames):
        scene.frame_set(f)
        goals=marker_targets(markers,f)
        for n,axes_local in axes.items():
            wrist=n.startswith("hand_")
            if wrist:bounds=((-62,65),(-30,30),(-75,75))
            else:
                digit,idx,_=n.split("_");j=int(idx)-1
                bend=([(-38,55),(-16,64),(-15,78)] if digit=="thumb" else [(-30,78),(-16,95),(-16,72)])[j]
                spread=([38,18,12] if digit=="thumb" else [22,7,5])[j]
                twist=([32,16,10] if digit=="thumb" else [18,7,5])[j]
                bounds=(bend,(-spread,spread),(-twist,twist))
            b=rig.pose.bones[n];q=b.rotation_quaternion.copy()
            result=bounded(q,axes_local,bounds)
            counts["wrist" if wrist else "base"]+=int(abs(result.dot(q))<.999999)
            if donor is not None and not wrist:
                mixed=result.slerp(tracks[keys[f]][n],weight)
                result=bounded(mixed,axes_local,bounds)
                counts["after_flow"]+=int(abs(result.dot(mixed))<.999999)
            if n in goals:
                flex,blend=goals[n]
                v=result.to_exponential_map()
                aim=Quaternion(v+axes_local[0]*(radians(flex)-v.dot(axes_local[0])))
                result=bounded(result.slerp(aim,blend),axes_local,bounds)
            if n in previous:
                result.make_compatible(previous[n])
                distance=previous[n].rotation_difference(result).angle
                cap=radians(650 if wrist else 360)/fps
                if distance>cap:
                    result=bounded(previous[n].slerp(result,cap/distance),axes_local,bounds)
                    counts["speed"]+=1
            b.rotation_quaternion=result;b.keyframe_insert("rotation_quaternion",frame=f)
            previous[n]=result.copy()
    return counts

