"""Exact skeleton read bindings; no loaders or mutation fallback in Python."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
LIBRARY='CubelessSkeletalLibrary'
BINDINGS=('list_skeleton_bones','list_skeletal_sockets')
PREPARATION='prepare_skeletal_read_toolset'

def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}

def _call(name,*args):
    function=getattr(getattr(unreal,LIBRARY,None),name,None)
    if not callable(function):raise OperationError('native_skeletal_read_unavailable','Exact native read binding unavailable; no load or fallback executed')
    raw=function(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=name:raise OperationError('invalid_native_skeletal_read','Returned operation identity differs')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result

def list_skeleton_bones(skeleton_path,bone_name_contains,max_bones):return _call(BINDINGS[0],skeleton_path,bone_name_contains,max_bones)
def list_skeletal_sockets(skeleton_path):return _call(BINDINGS[1],skeleton_path)
