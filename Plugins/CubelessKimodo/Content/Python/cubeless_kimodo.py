"""Kimodo authoring jobs. Call poll() on the editor thread to import a finished bake."""
import json
import math
import os
import re
from pathlib import Path
import shutil
import subprocess
import uuid

import unreal

PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
ROOT = PROJECT / "Saved" / "Kimodo"
QUINN = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
JOBS = {}
LIB = unreal.EditorAssetLibrary
SOURCE_TAG = "KimodoSourceJob"
REVISION_TAG = "KimodoRevisionJob"

def output_folder(value):
    path = (value or "/Game/Kimodo").strip().replace("\\", "/").rstrip("/")
    if path == "/All/Game" or path.startswith("/All/Game/"):
        path = path[4:]
    if path == "Content" or path.startswith("Content/"):
        path = "/Game" + path[7:]
    if not re.fullmatch(r"/Game(?:/[\w-]+)*", path):
        raise ValueError("프로젝트 Content 안의 저장 폴더를 선택하세요.")
    return path

def read_result(jid):
    if not isinstance(jid, str) or not re.fullmatch(r"[a-f0-9]{32}", jid):
        raise ValueError("Invalid Kimodo job ID")
    return json.loads((ROOT/jid/"result.json").read_text(encoding="utf-8-sig"))

def source_of(clip):
    jid = LIB.get_metadata_tag(clip, SOURCE_TAG)
    if not jid:
        match = re.search(r"/J_([a-f0-9]{32})/", clip.get_path_name())
        if not match or read_result(match[1])["asset"] != clip.get_path_name():
            raise ValueError("키모도 원본과 연결된 애니메이션이 아닙니다.")
        jid = match[1]
    return jid

def source_clip(jid):
    record = read_result(jid)
    owner = record.get("source_job",jid)
    clip = unreal.load_asset(record["asset"])
    if not isinstance(clip, unreal.AnimSequence) or source_of(clip) != owner:
        raise ValueError("원본 작업에 연결된 애니메이션을 찾을 수 없습니다.")
    return clip

def _settings():
    config = PROJECT / "Config" / "Kimodo.json"
    return json.loads(config.read_text(encoding="utf-8-sig")) if config.exists() else {}

def _tool_path(value):
    path = Path(os.path.expandvars(str(value))).expanduser()
    return path if path.is_absolute() else (PROJECT / path).resolve()

def _job():
    jid = uuid.uuid4().hex
    path = ROOT / jid
    path.mkdir(parents=True)
    return jid, path

def _launch(job, args, cwd, phase):
    env = dict(os.environ, HF_HUB_OFFLINE="1", TRANSFORMERS_OFFLINE="1", TEXT_ENCODER_DEVICE="cpu")
    with (job["path"] / (phase+".log")).open("wb") as log:
        job["process"] = subprocess.Popen(list(map(str,args)), cwd=cwd, env=env, stdout=log,
                                          stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    job["phase"] = phase

def generate(prompt, duration=2.0, seed=1218, folder=None):
    if not prompt.strip() or not math.isfinite(duration) or not 1 <= duration <= 30:
        raise ValueError("Expected a prompt and duration of 1 to 30 seconds")
    settings = _settings()
    root = _tool_path(os.environ.get("KIMODO_ROOT") or settings.get("kimodo_root") or PROJECT.parent.parent / "kimodo")
    python = _tool_path(os.environ.get("KIMODO_PYTHON") or settings.get("python") or root/".venv/Scripts/python.exe")
    if not python.is_file():
        raise ValueError(f"Kimodo Python not found at {python}. Set python in Config/Kimodo.json.")
    mesh_path = settings.get("target_mesh", QUINN)
    if not isinstance(unreal.load_asset(mesh_path),unreal.SkeletalMesh):
        raise ValueError(f"Set target_mesh in Config/Kimodo.json to a mannequin-compatible skeletal mesh: {mesh_path}")
    if folder is None:
        picker = unreal.KimodoSession.get_session().get_editor_property("output_folder")
        folder = picker.get_editor_property("path")
    folder = output_folder(folder)
    jid, path = _job()
    job = {"path":path, "bvh":path/"motion.bvh", "markers":"","target_mesh":mesh_path}
    job.update(folder=folder,source_job=jid)
    (path/"recipe.json").write_text(json.dumps(dict(prompt=prompt,duration=duration,seed=seed),indent=2))
    worker = Path(__file__).resolve().parents[2] / "ThirdParty/Kimodo59/run_58.py"
    _launch(job,[python,worker,path],root if root.is_dir() else python.parent,"generate")
    JOBS[jid] = job
    return jid

def correct(source_job, markers_json):
    if len(source_job) != 32 or any(c not in "0123456789abcdef" for c in source_job):
        raise ValueError("Invalid source job ID")
    saved = read_result(source_job)
    source_job = saved.get("source_job",source_job)
    clip = source_clip(source_job)
    if any(j.get("source_job")==source_job and j["phase"] not in ("done","failed") for j in JOBS.values()):
        raise ValueError("이 애니메이션의 보정 작업이 진행 중입니다.")
    mesh_path = saved.get("target_mesh",QUINN)
    bvh = (ROOT/saved.get("bvh_relative",saved["bvh"])).resolve()
    if not bvh.is_relative_to(ROOT) or not bvh.is_file():
        raise ValueError("Original BVH missing")
    doc = json.loads(markers_json)
    comparison = {}
    if "door_knob" in doc:
        knob=doc["door_knob"]
        for field in ("center_component_cm","outward","approach_offset_cm","release_offset_cm"):
            vector = knob.get(field, [0,0,0] if field.endswith("offset_cm") else None)
            if not isinstance(vector,list) or len(vector)!=3 or not all(type(x) in (int,float) and math.isfinite(x) for x in vector):
                raise ValueError(field)
        if not .1<=knob["radius_cm"]<=12 or not .99<=sum(x*x for x in knob["outward"])<=1.01:raise ValueError("Invalid knob dimensions or axis")
        if not 2<=knob.get("stem_length_cm",4)<=20:raise ValueError("Invalid knob stem length")
        pin=next((m for m in doc["markers"] if m["mode"]=="pin" and m["effector"] in ("hand_r","hand_l")),None)
        if not pin:raise ValueError("A hand pin is required for doorknob preview")
        comparison={"baseline_job":bvh.parent.name,"hand_review":{"side":pin["effector"][-1],"first":max(0,pin["start_frame"]-9),"last":pin["end_frame"]+9,"doorknob":knob}}
    jid, path = _job()
    markers = path/"markers.json"
    markers.write_text(json.dumps(doc,indent=2))
    job = {"path":path,"bvh":bvh,"markers":markers,"target_mesh":mesh_path,"comparison":comparison}
    job.update(source_job=source_job,target_asset=clip.get_path_name(),previous_revision=LIB.get_metadata_tag(clip,REVISION_TAG))
    last_key=clip.data_model_interface.get_number_of_keys()-1
    if any(m.get("end_frame")==last_key or m.get("anchor_frame")==last_key
           for m in [*doc.get("markers",[]),*doc.get("finger_markers",[])]):
        job["marker_last_key"]=last_key
    _bake(job)
    JOBS[jid] = job
    return jid

def _bake(job):
    path = job["path"]
    ref = path/"reference.fbx"
    mesh_path = job["target_mesh"] if "target_mesh" in job else _settings().get("target_mesh",QUINN)
    job["target_mesh"] = mesh_path
    mesh = unreal.load_asset(mesh_path)
    if not isinstance(mesh,unreal.SkeletalMesh):
        raise RuntimeError(f"Missing target skeletal mesh: {mesh_path}")
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = str(ref)
    task.automated = True
    task.prompt = False
    task.exporter = unreal.SkeletalMeshExporterFBX()
    task.options = unreal.FbxExportOption()
    task.options.level_of_detail = False
    if not unreal.Exporter.run_asset_export_task(task) or not ref.is_file():
        raise RuntimeError("Target skeletal mesh export failed")
    bundled = PROJECT.parent.parent/"_tools/blender-4.5.12-windows-x64/blender.exe"
    blender = _tool_path(os.environ.get("KIMODO_BLENDER") or _settings().get("blender") or (bundled if bundled.is_file() else shutil.which("blender") or bundled))
    if not blender.is_file():
        raise ValueError(f"Blender not found at {blender}. Set blender in Config/Kimodo.json.")
    _launch(job,[blender,"-b","-t",2,"--python-exit-code",1,"-P",Path(__file__).with_name("bake_quinn.py"),
                 "--",job["bvh"],ref,path/"motion.fbx",job["markers"],job.get("marker_last_key",-1)],path,"bake")

def _process_take(job, name):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.original_import_type = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_animations = True
    options.skeleton = unreal.load_asset(job["target_mesh"]).skeleton
    task = unreal.AssetImportTask()
    task.factory = unreal.FbxFactory()
    task.filename = str(job["path"]/"motion.fbx")
    task.destination_path = "/KimodoWork"
    task.destination_name = name
    task.options,task.automated,task.save = options,True,False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    clip, = [x for x in task.get_objects() if isinstance(x,unreal.AnimSequence)]
    if clip.get_editor_property("skeleton") != options.skeleton:
        raise RuntimeError("Imported clip has unexpected skeleton")
    mesh = unreal.load_asset(job["target_mesh"])
    doc = {}
    if job.get("markers"):
        from kimodo_native_pin import refine
        doc = json.loads(Path(job["markers"]).read_text(encoding="utf-8-sig"))
        refine(clip, mesh, doc.get("markers", []), doc.get("door_knob"), save=False)
    from kimodo_foot_grounding import apply as plant_feet
    job["foot_check"] = plant_feet(clip, mesh, doc.get("markers", []),save=False)
    from kimodo_native_clearance import apply
    job["thigh_check"] = apply(clip, mesh, doc.get("markers", []),save=False)
    from kimodo_wrist_clearance import apply as clear_wrists
    bake_record = json.loads((job["path"]/"motion.json").read_text())
    job["wrist_check"] = clear_wrists(clip,mesh,doc.get("markers",[]),bake_record.get("wrist_separation"))
    check = job["thigh_check"]
    remaining = []
    for side, label in (("l","왼손"),("r","오른손")):
        frames = check["remaining"][side]
        if frames:
            spans = []
            for frame in frames:
                if spans and spans[-1][1] == frame-1:
                    spans[-1][1] = frame
                else:
                    spans.append([frame,frame])
            remaining.append(label+" "+", ".join(str(a) if a==b else f"{a}–{b}" for a,b in spans))
    job["review"] = ("허벅지 관통 잔여 프레임 · "+" / ".join(remaining) if remaining else
                     f"허벅지 검사 {check['frames']}프레임 · 양손 관통 0")
    foot = job["foot_check"]
    if foot["status"] == "applied":
        supports = foot["support"]
        job["review"] += f" · 발접지 구간 왼발 {len(supports['l'])}개 / 오른발 {len(supports['r'])}개"
    else:
        job["review"] += " · 대상 메시에서 발바닥 표면을 찾지 못해 발접지 생략"
    if job["wrist_check"]["status"] == "checked":
        job["review"] += f" · 손목 관통·앞뒤 순서 잔여 {len(job['wrist_check']['remaining'])}프레임"
    clip.set_preview_skeletal_mesh(mesh)
    return clip

def baseline_for_preview(jid):
    record=read_result(jid)
    name="Baseline_"+uuid.uuid4().hex
    try:
        baked=_process_take(dict(path=ROOT/jid,target_mesh=record.get("target_mesh",QUINN)),name)
        return unreal.KimodoSession.snapshot_motion(baked)
    finally:
        if LIB.does_asset_exist("/KimodoWork/"+name):
            LIB.delete_asset("/KimodoWork/"+name)

def _import(job):
    jid = job["path"].name
    name = "Work_"+jid
    work_path = "/KimodoWork/"+name
    try:
        baked = _process_take(job,name)
        target = None
        if "target_asset" in job:
            target = source_clip(job["source_job"])
            if target.get_path_name()!=job["target_asset"] or LIB.get_metadata_tag(target,REVISION_TAG)!=job["previous_revision"]:
                raise ValueError("보정 중에 대상 애니메이션이 변경되었습니다.")
        # UE stores intervals, while the generator counts samples. Preserve an
        # existing clip's timing, including older clips without a terminal key.
        frames=baked.data_model_interface.get_number_of_frames()
        expected=target.data_model_interface.get_number_of_frames() if target else frames+1
        if expected==frames+1:
            baked.controller.set_number_of_frames(unreal.FrameNumber(expected),False)
        if target:
            asset=target
        else:
            tools=unreal.AssetToolsHelpers.get_asset_tools()
            folder=job["folder"]
            package,leaf=tools.create_unique_asset_name(folder+"/AS_Kimodo","")
            asset=tools.duplicate_asset(leaf,folder,baked)
            if not asset:
                raise RuntimeError(f"Could not create {package}")
        record=dict(asset=asset.get_path_name(),source_job=job["source_job"],bvh=str(job["bvh"]),
                    bvh_relative=Path(job["bvh"]).relative_to(ROOT).as_posix(),target_mesh=job["target_mesh"],
                    thigh_check=job["thigh_check"],foot_check=job["foot_check"],wrist_check=job["wrist_check"],**job.get("comparison",{}))
        backup=unreal.KimodoSession.snapshot_motion(asset) if target else None
        old_tags={tag:LIB.get_metadata_tag(asset,tag) for tag in (SOURCE_TAG,REVISION_TAG)}
        try:
            if target and not unreal.KimodoSession.copy_motion_tracks(baked,target):
                raise RuntimeError("보정본의 본 트랙 또는 재생 길이가 대상과 일치하지 않습니다.")
            LIB.set_metadata_tag(asset,SOURCE_TAG,job["source_job"])
            LIB.set_metadata_tag(asset,REVISION_TAG,jid)
            (job["path"]/"result.json").write_text(json.dumps(record,indent=2),encoding="utf8")
            if not LIB.save_loaded_asset(asset,False):
                raise RuntimeError("애니메이션을 저장하지 못했습니다.")
        except Exception:
            if backup:
                unreal.KimodoSession.copy_motion_tracks(backup,asset)
                for tag,value in old_tags.items():
                    if value: LIB.set_metadata_tag(asset,tag,value)
                    else: LIB.remove_metadata_tag(asset,tag)
            elif not target:
                LIB.delete_loaded_asset(asset)
            (job["path"]/"result.json").unlink(missing_ok=True)
            raise
        return asset.get_path_name()
    finally:
        if LIB.does_asset_exist(work_path):
            LIB.delete_asset(work_path)

def poll(jid):
    job = JOBS[jid]
    phase = job["phase"]
    if phase in ("generate","bake"):
        code = job["process"].poll()
        if code is not None:
            try:
                if code:
                    raise RuntimeError(f"{phase} failed ({code}), see {job['path']}")
                if phase == "generate":
                    job["phase"] = "starting_bake"
                    _bake(job)
                else:
                    job["phase"] = "importing"
                    job["asset"] = _import(job)
                    job["phase"] = "done"
            except Exception as exc:
                job["phase"],job["error"] = "failed",str(exc)
    return json.dumps({k:str(v) for k,v in job.items() if k in ("phase","asset","error","path","review")})






