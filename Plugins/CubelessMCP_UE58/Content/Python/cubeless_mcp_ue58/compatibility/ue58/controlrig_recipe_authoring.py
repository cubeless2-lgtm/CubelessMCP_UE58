"""HH exact UE5.8 recipe Library calls; no executor, RPC, or fallback."""
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessControlRigRecipeAuthoringLibrary'
BINDINGS=('validate_controlrig_recipe','dry_run_controlrig_recipe','build_controlrig_recipe')
MAX_CARRIER_UTF16_UNITS=1048576


def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def _invoke(method,payload):
    if type(payload) is not str:
        error=OperationError('controlrig_recipe_authoring_carrier_invalid','JSON envelope string required')
        error.native_call_dispatched=False;raise error
    try:units=len(payload.encode('utf-16-le'))//2
    except UnicodeError as cause:
        error=OperationError('controlrig_recipe_authoring_carrier_invalid','Valid Unicode JSON required')
        error.native_call_dispatched=False;raise error from cause
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in payload:
        error=OperationError('controlrig_recipe_authoring_carrier_bound','JSON envelope exceeds1Mi UTF16 or contains raw NUL')
        error.native_call_dispatched=False;raise error
    function=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(function):
        error=OperationError('controlrig_recipe_authoring_native_api_unavailable','Exact HH internal Library binding unavailable; no call dispatched')
        error.native_call_dispatched=False;raise error
    raw=function(payload)
    result=None
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('controlrig_recipe_authoring_native_result_invalid','Exact operation and typed changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw
        if type(result) is dict:
            result['_raw_native_json']=raw;error.native_partial_result=result
        raise
    result['_raw_native_json']=raw
    return result


def validate_controlrig_recipe(payload):return _invoke(BINDINGS[0],payload)


def dry_run_controlrig_recipe(payload):return _invoke(BINDINGS[1],payload)


def build_controlrig_recipe(payload):return _invoke(BINDINGS[2],payload)
