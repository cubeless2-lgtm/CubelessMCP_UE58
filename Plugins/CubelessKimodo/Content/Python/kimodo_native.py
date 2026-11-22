"""Connect the Slate details view to editor jobs and frame-based marker documents."""
import json
import os
import re
import tempfile
import time
from pathlib import Path
import unreal
import cubeless_kimodo as jobs

session = unreal.KimodoSession.get_session
active = None
source = None
finger_markers = []
rotation_pins = set()
door_knob = None
next_poll = 0
polling = False
handle = None
preferences = jobs.ROOT/"authoring.json"

def put(name,value):
    if name == "status":
        session().set_message(value)
    else:
        session().set_editor_property(name,value)

def get(name):
    return session().get_editor_property(name)

def save_json(path,doc):
    path.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile("w",encoding="utf-8",dir=path.parent,delete=False) as f:
        json.dump(doc,f,ensure_ascii=False,indent=2,allow_nan=False)
    os.replace(f.name,path)

def resolve_source(jid):
    if not isinstance(jid,str) or not re.fullmatch("[0-9a-f]{32}",jid):
        raise ValueError("유효한 원본 작업 ID가 필요합니다.")
    return jobs.source_clip(jid)

def set_source(jid):
    global source,finger_markers,door_knob
    clip=resolve_source(jid)
    door_knob=None
    source=jid
    finger_markers=[]
    rotation_pins.clear()
    session().set_source_asset(clip)
    put("comparison_clip",None)
    put("json_file",str(jobs.ROOT/jid/"markers.json"))
    put("markers",[])

def restore():
    if preferences.exists():
        try:
            jid=json.loads(preferences.read_text())["source_job"]
            set_source(jid)
            latest=jobs.LIB.get_metadata_tag(get("source_clip"),jobs.REVISION_TAG)
            applied=jobs.ROOT/(latest or jid)/"markers.json"
            if applied.is_file():
                put("json_file",str(applied))
                load()
                put("json_file",str(jobs.ROOT/jid/"markers.json"))
            elif Path(get("json_file")).is_file(): load()
        except Exception as exc:
            put("status",str(exc))

def document():
    if not source:
        raise ValueError("먼저 모션을 생성하세요.")
    markers=[]
    for m in get("markers"):
        prop=m.get_editor_property
        mode=prop("mode").name.lower()
        d=dict(id=str(prop("id")),effector=prop("effector").name.lower(),mode=mode,
               start_frame=prop("start_frame"),end_frame=prop("end_frame"),weight=prop("weight"),
               blend_in_frames=prop("blend_in_frames"),blend_out_frames=prop("blend_out_frames"))
        if mode=="offset":
            offset=prop("offset_cm")
            d.update(space="component",offset_cm=[offset.x,offset.y,offset.z])
        else:
            d.update(space="clip_world",anchor_frame=prop("anchor_frame"))
        if mode=="pin" and d["id"] in rotation_pins: d["lock_rotation"]=True
        markers.append(d)
    doc=dict(schema_version=1,source_job=source,expected_fps=30,markers=markers,finger_markers=finger_markers)
    if door_knob is not None: doc["door_knob"]=door_knob
    return doc

def load():
    global finger_markers,door_knob
    from kimodo_finger_markers import validate
    path=Path(get("json_file"))
    doc=json.loads(path.read_text(encoding="utf-8-sig"))
    if doc["schema_version"]!=1 or doc.get("expected_fps",30)!=30:
        raise ValueError("지원하지 않는 JSON 버전 또는 프레임률입니다.")
    jid=doc.get("source_job",source)
    clip=resolve_source(jid)
    fingers=validate(doc.get("finger_markers",[]),round(clip.get_play_length()*30)+1)
    items=[]
    for d in doc["markers"]:
        m=unreal.KimodoFrameMarker()
        fields=dict(id=d["id"],effector=getattr(unreal.KimodoEffector,d["effector"].upper()),
                    mode=getattr(unreal.KimodoMode,d["mode"].upper()),start_frame=d["start_frame"],
                    end_frame=d["end_frame"],anchor_frame=d.get("anchor_frame",d["start_frame"]),
                    offset_cm=unreal.Vector(*d.get("offset_cm",[0,0,0])),weight=d.get("weight",1),
                    blend_in_frames=d.get("blend_in_frames",0),blend_out_frames=d.get("blend_out_frames",0))
        for key,value in fields.items():
            m.set_editor_property(key,value)
        items.append(m)
    if jid!=source: set_source(jid)
    rotation_pins.clear()
    rotation_pins.update(d["id"] for d in doc["markers"] if d.get("lock_rotation") and d["mode"]=="pin")
    door_knob=doc.get("door_knob")
    finger_markers=fingers
    put("json_file",str(path))
    put("markers",items)
    put("status",f"JSON 불러오기 완료 · 손/발 IK {len(items)}개 · 손가락 {len(fingers)}개")

def dispatch(action):
    global active,handle
    try:
        if active:
            raise ValueError("진행 중인 작업을 기다려 주세요.")
        if action=="compare":
            import kimodo_compare
            kimodo_compare.play(get("comparison_clip") or get("preview_clip"))
            return
        if action=="save":
            save_json(Path(get("json_file")),document())
            put("status","JSON을 저장했습니다.")
            return
        if action=="load":
            load()
            return
        if action=="generate":
            jid=jobs.generate(get("prompt"),get("duration"),get("seed"))
        elif action=="apply":
            doc=document()
            jid=jobs.correct(source,json.dumps(doc))
        else:
            raise ValueError(action)
        active=(jid,action)
        put("status","생성 중" if action=="generate" else "IK 보정 중")
        handle=unreal.register_slate_post_tick_callback(tick)
    except Exception as exc:
        put("status",str(exc))
        unreal.log_warning(str(exc))

def tick(dt):
    global active,handle,next_poll,polling
    if polling or not active or time.monotonic()<next_poll: return
    polling=True
    next_poll=time.monotonic()+0.4
    try:
        jid,action=active
        result=json.loads(jobs.poll(jid))
        if result["phase"] not in ("done","failed"): return
        unreal.unregister_slate_post_tick_callback(handle)
        handle=active=None
        if result["phase"]=="failed": raise RuntimeError(result["error"])
        if action=="generate":
            set_source(jid)
        save_json(preferences,{"source_job":source})
        clip=unreal.load_asset(result["asset"])
        put("comparison_clip",None)
        session().preview(clip)
        label = "기본 보정 애니메이션 저장 완료" if action=="generate" else "현재 애니메이션 보정 저장 완료"
        put("status",label+" · "+result["review"] if result.get("review") else label)
    except Exception as exc:
        put("status",str(exc))
        unreal.log_error(str(exc))
    finally:
        polling=False








