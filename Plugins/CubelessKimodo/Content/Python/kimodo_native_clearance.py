"""Check lowered hands on the imported mannequin, in its actual skinned pose."""
import math
from collections import defaultdict

import unreal as u


def _regions(mesh, component, vertex_count):
    hand, leg = {}, {}
    triangles = u.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, False)[1]
    triangles = [(t.x, t.y, t.z) for t in u.GeometryScript_List.convert_triangle_list_to_array(triangles)]
    names = {}
    dominant = []
    for i in range(vertex_count):
        _, weight, valid = mesh.get_largest_vertex_bone_weight(i)
        if not valid:
            raise ValueError("Could not read the preview mesh skin weights")
        index = weight.bone_index
        if index not in names:
            names[index] = str(component.get_bone_name(index))
        dominant.append(names[index])
    masks = {}
    body_ids = set()
    def triangle_list(ids):
        face_ids = [i for i,t in enumerate(triangles) if all(v in ids for v in t)]
        if not face_ids:
            raise ValueError("No weighted hand or leg surface on the selected mannequin mesh")
        return u.GeometryScript_List.convert_array_to_index_list(face_ids,u.GeometryScriptIndexType.TRIANGLE)
    for side in ("l", "r"):
        ipsilateral = lambda n: n.endswith("_" + side)
        hand[side] = [i for i,n in enumerate(dominant) if ipsilateral(n)
                      and n.startswith(("hand", "thumb", "index", "middle", "ring", "pinky"))]
        ids = {i for i,n in enumerate(dominant) if n == "pelvis" or ipsilateral(n) and n.startswith(("thigh", "calf"))}
        leg[side] = ([t for t in triangles if all(i in ids for i in t)], ids)
        masks[side] = triangle_list(set(hand[side]))
        body_ids.update(ids)
    masks["legs"] = triangle_list(body_ids)
    return hand, leg, masks


def _clearance(points, hand, leg, axis):
    # Project onto the plane perpendicular to the hip axis. At each hand vertex
    # find the outermost leg triangle at exactly the same height and depth.
    def project(i):
        p = points[i]
        return (-p.x*axis.y+p.y*axis.x, p.z, p.x*axis.x+p.y*axis.y)
    grid = defaultdict(list)
    for i in hand:
        x,y,z = project(i)
        grid[(math.floor(x/3), math.floor(y/3))].append((x,y,z))
    if not grid:
        return 0.
    columns, rows = zip(*grid)
    lo_x, hi_x, lo_y, hi_y = min(columns), max(columns), min(rows), max(rows)
    tris, ids = leg
    projected = {i:project(i) for i in ids}
    depth = 0.
    for ia,ib,ic in tris:
        (x,y,z),(bx,by,bz),(cx,cy,cz) = projected[ia], projected[ib], projected[ic]
        x0 = max(lo_x,math.floor(min(x,bx,cx)/3))
        x1 = min(hi_x,math.floor(max(x,bx,cx)/3))
        y0 = max(lo_y,math.floor(min(y,by,cy)/3))
        y1 = min(hi_y,math.floor(max(y,by,cy)/3))
        if x0>x1 or y0>y1:
            continue
        bx-=x; by-=y; cx-=x; cy-=y
        det=bx*cy-by*cx
        if abs(det)<1e-8:
            continue
        for col in range(x0,x1+1):
            for row in range(y0,y1+1):
                for px,py,pz in grid.get((col,row),()):
                    a=((px-x)*cy-(py-y)*cx)/det
                    b=(bx*(py-y)-by*(px-x))/det
                    if a>=0 and b>=0 and a+b<=1:
                        depth=max(depth,z+a*(bz-z)+b*(cz-z)-pz+.8)
    return depth


def _sample(clip, mesh, markers, verify_only=False):
    es=u.get_editor_subsystem(u.EditorActorSubsystem)
    selected=es.get_selected_level_actors()
    actor=es.spawn_actor_from_class(u.SkeletalMeshActor,u.Vector(0,0,20000),transient=True)
    try:
        comp=actor.skeletal_mesh_component
        comp.set_skeletal_mesh_asset(mesh)
        comp.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
        comp.set_update_animation_in_editor(True)
        comp.play_animation(clip,True)
        comp.set_play_rate(0)
        dm=u.DynamicMesh()
        opt=u.GeometryScriptCopyMeshFromComponentOptions()
        opt.want_normals=False
        opt.want_tangents=False
        count=clip.data_model_interface.get_number_of_keys()
        shifts={s:[0.]*count for s in ("l","r")}
        axes=[]
        regions=None
        hits = {"l": [], "r": []}
        legs, hand_mesh = u.DynamicMesh(), u.DynamicMesh()
        identity = u.Transform()
        for i in range(count):
            u.KimodoSession.evaluate_comparison_frame(comp,i/30.)
            joint=comp.get_socket_location
            axis=joint("thigh_l")-joint("thigh_r")
            axis.z=0.
            axis=axis.normal()
            axes.append(axis)
            nearby=[]
            for s,sign in (("l",1.),("r",-1.)):
                if any(m.get("effector")=="hand_"+s and m["start_frame"]<=i<=m["end_frame"] for m in markers):
                    continue
                w=joint("hand_"+s)-joint("thigh_"+s)
                if -1<w.dot(axis)*sign<38 and abs(w.z)<35 and abs(w.y*axis.x-w.x*axis.y)<35:
                    nearby.append((s,axis*sign))
            _,_,result=u.GeometryScript_SceneUtils.copy_mesh_from_component(comp,dm,opt,False)
            if result!=u.GeometryScriptOutcomePins.SUCCESS:
                raise RuntimeError("Could not evaluate mannequin skin for thigh clearance")
            vv=u.GeometryScript_MeshQueries.get_all_vertex_positions(dm,False)[1]
            points=u.GeometryScript_List.convert_vector_list_to_array(vv)
            if regions is None:
                regions=_regions(dm,comp,len(points))
            hands, surfaces, masks = regions
            dm.get_sub_mesh_from_mesh(legs,masks["legs"])
            for side,sign in (("l",1.),("r",-1.)):
                dm.get_sub_mesh_from_mesh(hand_mesh,masks[side])
                _, intersects = hand_mesh.is_intersecting_mesh(identity,legs,identity)
                if intersects:
                    hits[side].append(i)
                if any(m.get("effector")=="hand_"+side and m["start_frame"]<=i<=m["end_frame"] for m in markers):
                    continue
                if intersects or (not verify_only and any(s==side for s,_ in nearby)):
                    shifts[side][i]=_clearance(points,hands[side],surfaces[side],axis*sign)
                    if intersects:
                        shifts[side][i]=max(.8,shifts[side][i])
        return axes,shifts,hits
    finally:
        es.destroy_actor(actor)
        es.set_selected_level_actors(selected)


def _move_arms(clip, mesh, markers, axes, shifts, save):
    ext=u.AnimPoseExtensions
    opt=u.AnimPoseEvaluationOptions()
    opt.optional_skeletal_mesh=mesh
    raw=u.AnimPoseEvaluationOptions()
    raw.should_retarget=False
    tracks={}
    for i,axis in enumerate(axes):
        pose=ext.get_anim_pose_at_frame(clip,i,opt)
        original=ext.get_anim_pose_at_frame(clip,i,raw)
        for s,sign in (("l",1.),("r",-1.)):
            if not any(shifts[s]):
                continue
            names=("upperarm_"+s,"hand_"+s)
            keys=[ext.get_bone_pose(original,n,u.AnimPoseSpaces.LOCAL) for n in names]
            nearby=range(max(0,i-3),min(len(axes),i+4))
            shift=max(shifts[s][k]*(1-abs(k-i)/4.)**2 for k in nearby)
            if shift>0 and not any(m.get("effector")=="hand_"+s and m["start_frame"]<=i<=m["end_frame"] for m in markers):
                arm,hand=[ext.get_bone_pose(pose,n,u.AnimPoseSpaces.WORLD) for n in names]
                outward=axis*sign
                reach=hand.translation-arm.translation
                lateral=reach.dot(outward)
                length=reach.length()
                new_lateral=min(length*.985,lateral+shift)
                tangent=(reach-outward*lateral).normal()
                desired=outward*new_lateral+tangent*math.sqrt(max(0.,length**2-new_lateral**2))
                delta=reach.find_quat_between_vectors(desired)
                # Rotate the upper arm and undo that rotation at the wrist.
                for key,world,change in zip(keys,(arm,hand),(delta,delta.inversed())):
                    key.rotation=key.rotation*world.rotation.inversed()*change*world.rotation
            for name,key in zip(names,keys):
                tracks.setdefault(name,[]).append(key)
    ctrl=clip.controller
    ctrl.open_bracket("Clear mannequin thighs in the imported pose",False)
    try:
        for name,keys in tracks.items():
            if not ctrl.set_bone_track_keys(name,[t.translation for t in keys],[t.rotation for t in keys],[t.scale3d for t in keys],False):
                raise RuntimeError(f"Could not correct {name}")
    finally:
        ctrl.close_bracket(False)
    if save:
        u.EditorAssetLibrary.save_loaded_asset(clip)


def apply(clip, mesh, markers, *, save=True):
    axes, shifts, hits = _sample(clip, mesh, markers)
    initial = {side: len(frames) for side, frames in hits.items()}
    passes = 0
    while passes < 3 and any(any(values) for values in shifts.values()):
        _move_arms(clip, mesh, markers, axes, shifts, save)
        passes += 1
        axes, shifts, hits = _sample(clip, mesh, markers, verify_only=True)
    return {"frames":len(axes), "initial":initial, "remaining":hits, "passes":passes}
