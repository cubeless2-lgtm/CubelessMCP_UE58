"""Exact original Pawn6 on one guarded native Pawn library binding."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
LIBRARY="CubelessPawnLibrary"
BINDINGS=("set_pawn_properties",)
PREPARATION="prepare_pawn_properties_toolset"


def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def set_pawn_properties(blueprint_path,auto_possess_player,use_controller_rotation_yaw_json,
        use_controller_rotation_pitch_json,use_controller_rotation_roll_json,can_be_damaged_json):
    function=getattr(getattr(unreal,LIBRARY,None),BINDINGS[0],None)
    if not callable(function):
        raise OperationError("native_pawn_api_unavailable","Exact native Pawn binding unavailable; no load/fallback/dispatch")
    raw=function(blueprint_path,auto_possess_player,use_controller_rotation_yaw_json,
        use_controller_rotation_pitch_json,use_controller_rotation_roll_json,can_be_damaged_json)
    try:
        native=_decode_result(raw)
        if native.get("operation")!=BINDINGS[0] or "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_native_pawn_result","Wrong operation/changed type; full raw prefix retained")
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw
        raise
    native["_raw_native_json"]=raw
    return native



def set_pawn_properties_raw(blueprint_path,params_json):
    function=getattr(getattr(unreal,LIBRARY,None),'set_pawn_properties_raw',None)
    if not callable(function):raise OperationError('native_pawn_api_unavailable','Exact original raw Pawn helper unavailable; no fallback or dispatch')
    raw=function(blueprint_path,params_json)
    try:
        native=_decode_result(raw)
        if native.get('operation')!='set_pawn_properties' or 'changed' not in native or native['changed'] is not None and type(native['changed']) is not bool:
            raise OperationError('invalid_native_pawn_result','Wrong raw Pawn operation/changed type; full raw prefix retained')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    native['_raw_native_json']=raw
    return native
