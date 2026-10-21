"""Original fixture13/inspector3 ABI; native owns world and component work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
LIBRARY='CubelessSkeletalLibrary'
BINDINGS=('create_skeletal_bone_constraint_fixture','inspect_skeletal_bone_constraint_fixture')

def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}

def _invoke(name,*arguments):
    function=getattr(getattr(unreal,LIBRARY,None),name,None)
    if not callable(function):raise OperationError('native_skeletal_constraint_api_unavailable','Exact fixture/inspector binding unavailable; no fallback or lifecycle action dispatched')
    raw=function(*arguments)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=name:raise OperationError('invalid_native_skeletal_constraint_result','Returned native operation differs')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result

def create_skeletal_bone_constraint_fixture(skeletal_mesh_path,actor_name,bone_name1,bone_name2,world_context,location_json,constraint_component_name,swing1_limit_degrees,swing2_limit_degrees,twist_limit_degrees,disable_collision,simulate_all_bodies,dry_run):
    return _invoke(BINDINGS[0],skeletal_mesh_path,actor_name,bone_name1,bone_name2,world_context,location_json,constraint_component_name,swing1_limit_degrees,swing2_limit_degrees,twist_limit_degrees,disable_collision,simulate_all_bodies,dry_run)

def inspect_skeletal_bone_constraint_fixture(actor_name,world_context,constraint_component_name):
    return _invoke(BINDINGS[1],actor_name,world_context,constraint_component_name)
