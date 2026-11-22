"""Compare the source SOMA skin and the selected baked clip in the level viewport."""
import hashlib
import os
from pathlib import Path
import shutil
import tempfile
import time
import unreal as u
import cubeless_kimodo as jobs

state = None
worker = Path(__file__).with_name("kimodo_compare_cache.py")


def message(value):
    u.KimodoSession.get_session().set_message(value)


def stop():
    global state
    if state is None:
        return
    current,state = state,None
    if current.get("handle") is not None:
        u.unregister_slate_post_tick_callback(current["handle"])
    proc = current.get("process")
    if proc and proc.poll() is None:
        proc.terminate()
        proc.wait()
    if current.get("temp"):
        current["temp"].cleanup()
    level = u.get_editor_subsystem(u.LevelEditorSubsystem)
    if current.get("camera") and level.get_pilot_level_actor() == current["camera"]:
        level.eject_pilot_level_actor()
        old_pilot=current.get("previous_pilot")
        if old_pilot and u.SystemLibrary.is_valid(old_pilot):
            level.pilot_level_actor(old_pilot)
        elif current.get("previous_view"):
            u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(*current["previous_view"])
    es = u.get_editor_subsystem(u.EditorActorSubsystem)
    for actor in reversed(current["actors"]):
        if u.SystemLibrary.is_valid(actor):
            es.destroy_actor(actor)


def play(clip):
    if not isinstance(clip,u.AnimSequence):
        raise ValueError("먼저 비교할 키모도 클립을 선택하세요.")
    global state
    if state and state["clip"]==clip:
        stop()
        message("비교 재생을 종료했습니다.")
        return
    owner = jobs.source_of(clip)
    result = jobs.read_result(jobs.LIB.get_metadata_tag(clip,jobs.REVISION_TAG) or owner)
    bvh = ((jobs.ROOT/result["bvh_relative"]) if "bvh_relative" in result else Path(result["bvh"])).resolve()
    if not bvh.is_relative_to(jobs.ROOT) or not bvh.is_file():
        raise ValueError("키모도 원본 BVH를 찾을 수 없습니다.")
    mesh = u.load_asset(result.get("target_mesh",jobs.QUINN))
    if not isinstance(mesh,u.SkeletalMesh) or mesh.skeleton != clip.get_editor_property("skeleton"):
        raise ValueError("애니메이션에 맞는 대상 메시를 찾을 수 없습니다.")
    key = hashlib.sha256(bvh.read_bytes()).hexdigest()[:24]
    dest = "/KimodoWork"
    cache_path = dest+"/Soma_"+key
    legacy_cache = "/Game/Kimodo/Comparison/S_"+key+"/Soma"
    stop()
    state = dict(actors=[],clip=clip,mesh=mesh,dest=dest,cache_path=cache_path,cache_key=key)
    try:
        if result.get("before_hand_asset") or result.get("baseline_job"):
            before = (jobs.baseline_for_preview(result["baseline_job"]) if result.get("baseline_job")
                      else u.load_asset(result["before_hand_asset"]))
            if not isinstance(before,u.AnimSequence) or before.get_editor_property("skeleton") != mesh.skeleton:
                raise ValueError("손 보정 전 클립을 열 수 없습니다.")
            state["hand_review"] = result.get("hand_review")
            start_stage(before)
        elif u.EditorAssetLibrary.does_asset_exist(cache_path):
            start_stage(u.load_asset(cache_path))
        elif u.EditorAssetLibrary.does_asset_exist(legacy_cache):
            start_stage(u.load_asset(legacy_cache))
        else:
            tmp=tempfile.TemporaryDirectory(dir=jobs.ROOT,prefix="compare_")
            path=Path(tmp.name)
            state.update(temp=tmp,path=path)
            settings=jobs._settings()
            root=jobs._tool_path(os.environ.get("KIMODO_ROOT") or settings.get("kimodo_root") or jobs.PROJECT.parent.parent/"kimodo")
            python=jobs._tool_path(os.environ.get("KIMODO_PYTHON") or settings.get("python") or root/".venv/Scripts/python.exe")
            bundled=jobs.PROJECT.parent.parent/"_tools/blender-4.5.12-windows-x64/blender.exe"
            blender=jobs._tool_path(os.environ.get("KIMODO_BLENDER") or settings.get("blender") or (bundled if bundled.exists() else shutil.which("blender") or bundled))
            if not python.is_file() or not blender.is_file():
                raise ValueError("Config/Kimodo.json에서 Python과 Blender 경로를 확인하세요.")
            state["blender"]=blender
            jobs._launch(state,[python,worker,bvh,path],root if root.is_dir() else path,"skin")
            message("원본 메시 준비 중…")
        state["handle"]=u.register_slate_post_tick_callback(tick)
    except Exception:
        stop()
        raise


def start_stage(cache):
    if not isinstance(cache,(u.GeometryCache,u.AnimSequence)):
        raise ValueError("원본 메시 캐시를 열 수 없습니다.")
    es=u.get_editor_subsystem(u.EditorActorSubsystem)
    def spawn(cls,position,yaw=0):
        actor=es.spawn_actor_from_class(cls,u.Vector(*position),u.Rotator(0,0,yaw),transient=True)
        actor.set_actor_location_and_rotation(u.Vector(*position),u.Rotator(0,0,yaw),False,True)
        state["actors"].append(actor)
        return actor
    level=u.get_editor_subsystem(u.LevelEditorSubsystem)
    def animated(clip,offset,label):
        actor=spawn(u.SkeletalMeshActor,(0,offset,14000),90)
        component=actor.skeletal_mesh_component
        component.set_skeletal_mesh_asset(state["mesh"])
        component.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
        component.set_update_animation_in_editor(True)
        component.play_animation(clip,True)
        component.set_play_rate(0.0)
        actor.set_actor_label(label)
        return component
    if isinstance(cache,u.AnimSequence):
        sc=animated(cache,-90,"퀸 · 손 보정 전")
        qc=animated(state["clip"],90,"퀸 · 손 보정 후")
        state["paired_quinn"]=True
    else:
        src=spawn(u.GeometryCacheActor,(0,-90,14000))
        sc=src.get_component_by_class(u.GeometryCacheComponent)
        sc.set_geometry_cache(cache)
        sc.set_editor_property("manual_tick",True)
        sc.tick_at_this_time(0.0,True,False,True)
        src.set_actor_label("Kimodo 원본")
        qc=animated(state["clip"],90,"Kimodo 리타기팅 결과")
    review=state.get("hand_review")
    if review and state.get("paired_quinn"):
        if review.get("doorknob"):
            setup_doorknob(spawn,review["doorknob"],sc,qc)
        setup_hand_views(spawn)
    camera=spawn(u.CameraActor,(-1600,0,12090 if review else 14090))
    camera.camera_component.set_field_of_view(16)
    state["previous_pilot"]=level.get_pilot_level_actor()
    state["previous_view"]=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_level_viewport_camera_info()
    level.pilot_level_actor(camera)
    state.update(camera=camera,target=qc,source=sc,phase="playing",started=time.monotonic(),last_frame=-1)
    message("퀸 손 보정 비교 · 왼쪽 보정 전 / 오른쪽 보정 후" if state.get("paired_quinn") else "비교 재생 중 · 왼쪽 원본 / 오른쪽 리타기팅 · 다시 누르면 종료")


def hand_screen_material():
    path="/Game/Kimodo/Comparison/M_HandReviewScreen"
    if u.EditorAssetLibrary.does_asset_exist(path):
        return u.load_asset(path)
    material=u.AssetToolsHelpers.get_asset_tools().create_asset("M_HandReviewScreen","/Game/Kimodo/Comparison",u.Material,u.MaterialFactoryNew())
    material.set_editor_property("two_sided",True)
    material.set_editor_property("shading_model",u.MaterialShadingModel.MSM_UNLIT)
    edit=u.MaterialEditingLibrary
    tex=edit.create_material_expression(material,u.MaterialExpressionTextureSampleParameter2D,-400,0)
    tex.set_editor_property("parameter_name","HandImage")
    tex.set_editor_property("texture",u.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    power=edit.create_material_expression(material,u.MaterialExpressionPower,-180,0)
    power.set_editor_property("const_exponent",.55)
    edit.connect_material_expressions(tex,"RGB",power,"Base")
    edit.connect_material_property(power,"",u.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(material)
    u.EditorAssetLibrary.save_loaded_asset(material)
    return material


def setup_doorknob(spawn, prop, before, after):
    """Fixed preview geometry in clip component space, shared by both takes."""
    centers=[]
    def shape(kind,label,point,rotation,scale,color):
        actor=spawn(u.StaticMeshActor,(point.x,point.y,point.z))
        comp=actor.static_mesh_component
        comp.set_mobility(u.ComponentMobility.MOVABLE)
        comp.set_static_mesh(u.load_asset("/Engine/BasicShapes/"+kind))
        actor.set_actor_label(label)
        actor.set_actor_rotation(rotation,False)
        actor.set_actor_scale3d(scale)
        mid=comp.create_dynamic_material_instance(0,u.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
        if mid:mid.set_vector_parameter_value("Color",color)
    for comp in (before,after):
        transform=comp.get_owner().get_actor_transform()
        center=transform.transform_location(u.Vector(*prop["center_component_cm"]))
        axis=transform.transform_direction(u.Vector(*prop["outward"])).normal()
        radius=prop["radius_cm"]
        stem=prop.get("stem_length_cm",4.)
        brass=u.LinearColor(.53,.29,.09,1)
        tangent=u.Vector(0,0,1).cross(axis).normal()
        light_pos=center-axis*24-tangent*18+u.Vector(0,0,18)
        light=spawn(u.PointLight,(light_pos.x,light_pos.y,light_pos.z))
        light.light_component.set_mobility(u.ComponentMobility.MOVABLE)
        light.light_component.set_intensity(.65)
        light.light_component.set_attenuation_radius(110.)
        shape("Sphere","고정 문고리",center,u.Rotator(),u.Vector(radius/50,radius/50,radius/50),brass)
        shape("Cylinder","문고리 축",center+axis*(radius-1.+stem/2),u.MathLibrary.make_rot_from_z(axis),u.Vector(.016,.016,stem/100.),brass)
        shape("Cube","문짝",center+axis*(radius+stem+.5),u.MathLibrary.make_rot_from_x(axis),u.Vector(.04,.76,1.9),u.LinearColor(.15,.09,.055,1))
        centers.append((center,axis))
    state["knob_centers"]=centers


def setup_hand_views(spawn):
    """Show two matching live hand cameras on adjacent panels in the level viewport."""
    plane=u.load_asset("/Engine/BasicShapes/Plane")
    material=hand_screen_material()
    views=[]
    for y in (-52,52):
        capture=spawn(u.SceneCapture2D,(0,0,14000))
        cc=capture.get_component_by_class(u.SceneCaptureComponent2D)
        rt=u.RenderingLibrary.create_render_target2d(capture,512,384,u.TextureRenderTargetFormat.RTF_RGBA8)
        cc.set_editor_property("texture_target",rt)
        cc.set_editor_property("capture_source",u.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        cc.set_editor_property("capture_every_frame",False)
        cc.set_editor_property("capture_on_movement",False)
        if state.get("knob_centers"):
            cc.set_editor_property("always_persist_rendering_state",True)
        cc.set_editor_property("fov_angle",43)
        panel=spawn(u.StaticMeshActor,(-800,y,12090))
        sm=panel.static_mesh_component
        sm.set_mobility(u.ComponentMobility.MOVABLE)
        sm.set_static_mesh(plane)
        panel.set_actor_rotation(u.MathLibrary.make_rot_from_xy(u.Vector(0,1,0),u.Vector(0,0,1)),False)
        # The basic plane's texture V increases along its local Y axis.
        panel.set_actor_scale3d(u.Vector(1.03,-.773,1))
        dyn=sm.create_dynamic_material_instance(0,material)
        dyn.set_texture_parameter_value("HandImage",rt)
        views.append((capture,cc))
    state["hand_views"]=views


def update_hand_views(current):
    side=current["hand_review"]["side"]
    before=current["source"]
    def bone(component,name):
        return component.get_socket_location(name+"_"+side)
    wrist=bone(before,"hand")
    middle=bone(before,"middle_01")
    across=(bone(before,"index_01")-bone(before,"pinky_01")).normal()
    forward=(middle-wrist).normal()
    normal=forward.cross(across).normal()
    if normal.dot(bone(before,"middle_03")-middle)<0:
        normal=normal*-1
    center=wrist+(middle-wrist)*.94
    offset=normal*42+across*11+forward*5
    torso=before.get_socket_location("spine_03")
    if (center+offset-torso).length()<(center-offset-torso).length()-8:
        offset=offset*-1
    for (actor,cc),component in zip(current["hand_views"],(before,current["target"])):
        focus=center+(bone(component,"hand")-wrist)
        if current.get("knob_centers"):
            idx=0 if component==before else 1
            focus,axis=current["knob_centers"][idx]
            tangent=u.Vector(0,0,1).cross(axis).normal()
            offset=tangent*-30-axis*15+u.Vector(0,0,-8)
        eye=focus+offset
        actor.set_actor_location_and_rotation(eye,u.MathLibrary.find_look_at_rotation(eye,focus),False,True)
        cc.capture_scene()


def tick(dt):
    current=state
    if not current or current.get("busy"):
        return
    current["busy"]=True
    try:
        phase=current["phase"]
        if phase=="playing":
            if not all(u.SystemLibrary.is_valid(a) for a in current["actors"]):
                stop()
                return
            frame_count=int(round(current["clip"].get_play_length()*30))+1
            frame=int((time.monotonic()-current["started"])*30) % max(1,frame_count)
            if current.get("hand_views"):
                review=current["hand_review"]
                first=max(0,min(frame_count-1,int(review["first"])))
                last=max(first,min(frame_count-1,int(review["last"])))
                frame=first+int((time.monotonic()-current["started"])*30)%(last-first+1)
            if frame==current["last_frame"]:
                return
            current["last_frame"]=frame
            t=frame/30.0
            if current.get("paired_quinn"):
                u.KimodoSession.evaluate_comparison_frame(current["source"],t)
            else:
                current["source"].tick_at_this_time(t,True,False,True)
            u.KimodoSession.evaluate_comparison_frame(current["target"],t)
            if current.get("hand_views"):
                update_hand_views(current)
            return
        status=current["process"].poll()
        if status is None:
            return
        path=current["path"]
        if status:
            log=(path/(phase+".log")).read_text(errors="replace")[-600:]
            raise RuntimeError(f"원본 비교 캐시 생성 실패 ({phase}) {log}")
        if phase=="skin":
            jobs._launch(current,[current["blender"],"-b","-t","2","--python-exit-code","1","-P",worker,"--",path],path,"alembic")
            return
        opt=u.AbcImportSettings()
        opt.import_type=u.AlembicImportType.GEOMETRY_CACHE
        conv=opt.conversion_settings
        conv.preset=u.AbcConversionPreset.CUSTOM
        conv.scale=u.Vector(1,1,1)
        conv.rotation=u.Vector(0,0,0)
        opt.conversion_settings=conv
        task=u.AssetImportTask()
        archive=jobs.ROOT/"ComparisonCache"/current["cache_key"]
        archive.mkdir(parents=True,exist_ok=True)
        abc=archive/"Soma.abc"
        os.replace(path/"Soma.abc",abc)
        task.filename=str(abc)
        task.destination_path=current["dest"]
        task.destination_name=current["cache_path"].rsplit("/",1)[1]
        task.automated=task.save=True
        task.options=opt
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        start_stage(u.load_asset(current["cache_path"]))
        current.pop("temp").cleanup()
    except Exception as exc:
        stop()
        message(str(exc))
        u.log_error(str(exc))
    finally:
        current["busy"]=False










