"""Original Retarget5 binding; native owns load/setter/forced save."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
LIBRARY='CubelessSkeletalLibrary'
BINDINGS=('set_bone_translation_retargeting',)
def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}
def set_bone_translation_retargeting(skeleton_path,bone_name,mode,recursive,save):
    function=getattr(getattr(unreal,LIBRARY,None),BINDINGS[0],None)
    if not callable(function):raise OperationError('native_skeletal_retarget_api_unavailable','Exact Retarget5 unavailable; no fallback/load dispatched')
    raw=function(skeleton_path,bone_name,mode,recursive,save)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=BINDINGS[0]:raise OperationError('invalid_native_skeletal_retarget_result','Returned native identity differs')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result
