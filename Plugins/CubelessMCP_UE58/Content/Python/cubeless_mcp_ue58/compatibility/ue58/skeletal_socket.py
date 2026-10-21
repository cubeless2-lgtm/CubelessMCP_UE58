"""Original socket3 native bindings; no loader, fallback or second save."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
LIBRARY='CubelessSkeletalLibrary'
BINDINGS=('add_skeletal_socket','set_skeletal_socket','remove_skeletal_socket')
PREPARATION='prepare_skeletal_socket_toolset'
def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}
def _call(name,*args):
    function=getattr(getattr(unreal,LIBRARY,None),name,None)
    if not callable(function):raise OperationError('native_skeletal_socket_api_unavailable','Exact socket binding unavailable; no fallback/load dispatched')
    raw=function(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=name:raise OperationError('invalid_native_skeletal_socket_result','Returned native identity differs')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result
def add_skeletal_socket(skeleton_path,socket_name,bone_name,location_json,rotation_json,scale_json,save):
    return _call(BINDINGS[0],skeleton_path,socket_name,bone_name,location_json,rotation_json,scale_json,save)
def set_skeletal_socket(skeleton_path,socket_name,bone_name,new_socket_name,location_json,rotation_json,scale_json,save):
    return _call(BINDINGS[1],skeleton_path,socket_name,bone_name,new_socket_name,location_json,rotation_json,scale_json,save)
def remove_skeletal_socket(skeleton_path,socket_name,save):return _call(BINDINGS[2],skeleton_path,socket_name,save)
