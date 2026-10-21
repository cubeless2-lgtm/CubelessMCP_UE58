"""HG internal profile Engine metadata and original pure C++ preset JSON leaves.
Public recipe composition stays Python; no server, script executor, or transport.
"""
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessControlRigRecipePlanningLibrary'
BINDINGS=('profile_controlrig_skeleton_metadata','make_controlrig_preset_recipe_data')
MAX_CARRIER_UTF16_UNITS=1048576


def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def _invoke(method,payload):
    if type(payload) is not str:
        error=OperationError('controlrig_recipe_planning_carrier_invalid','JSON body string required')
        error.native_call_dispatched=False;raise error
    try:units=len(payload.encode('utf-16-le'))//2
    except UnicodeError as cause:
        error=OperationError('controlrig_recipe_planning_carrier_invalid','Valid Unicode JSON required')
        error.native_call_dispatched=False;raise error from cause
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in payload:
        error=OperationError('controlrig_recipe_planning_carrier_bound','JSON body exceeds1Mi UTF16 or contains raw NUL')
        error.native_call_dispatched=False;raise error
    function=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(function):
        error=OperationError('controlrig_recipe_planning_native_api_unavailable','Exact HG internal Library binding is unavailable; no call dispatched')
        error.native_call_dispatched=False;raise error
    raw=function(payload)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('controlrig_recipe_planning_native_result_invalid','Exact operation and typed changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result


def profile_controlrig_skeleton_metadata(payload):return _invoke(BINDINGS[0],payload)


def make_controlrig_preset_recipe_data(payload):return _invoke(BINDINGS[1],payload)
