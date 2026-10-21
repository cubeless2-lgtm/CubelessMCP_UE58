"""Original PhysicsAsset15 hidden ABI; no Python load/generator/save."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
LIBRARY='CubelessSkeletalLibrary'
BINDINGS=('create_skeletal_physics_asset',)
def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}
def create_skeletal_physics_asset(skeletal_mesh_path,target_physics_asset_path,expected_bones_json,kinematic_root_bone,root_kinematic,disable_collision,swing1_limit_degrees,swing2_limit_degrees,twist_limit_degrees,linear_damping,angular_damping,tip_mass_scale,overwrite_existing,save,dry_run):
    function=getattr(getattr(unreal,LIBRARY,None),BINDINGS[0],None)
    if not callable(function):raise OperationError('native_skeletal_physics_api_unavailable','Exact PhysicsAsset15 unavailable; no fallback/load dispatched')
    raw=function(skeletal_mesh_path,target_physics_asset_path,expected_bones_json,kinematic_root_bone,root_kinematic,disable_collision,swing1_limit_degrees,swing2_limit_degrees,twist_limit_degrees,linear_damping,angular_damping,tip_mass_scale,overwrite_existing,save,dry_run)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=BINDINGS[0]:raise OperationError('invalid_native_skeletal_physics_result','Returned native operation differs')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result
