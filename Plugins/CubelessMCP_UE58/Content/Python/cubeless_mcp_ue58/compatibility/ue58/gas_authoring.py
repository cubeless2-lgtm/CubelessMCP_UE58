"""UE5.8 dedicated GASAuthoring Native1 x6. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessGASAuthoringLibrary'
BINDINGS=["inspect_gas_blueprint", "create_gameplay_ability_blueprint", "create_gameplay_effect_blueprint", "create_attribute_set_blueprint", "add_ability_system_component_to_blueprint", "add_gameplay_attribute_to_attribute_set"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_gas_blueprint": ["blueprint_path"], "create_gameplay_ability_blueprint": ["asset_name", "package_path", "save"], "create_gameplay_effect_blueprint": ["asset_name", "package_path", "save"], "create_attribute_set_blueprint": ["asset_name", "package_path", "save"], "add_ability_system_component_to_blueprint": ["blueprint_path", "component_name", "save"], "add_gameplay_attribute_to_attribute_set": ["attribute_set_path", "attribute_name", "category", "save"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('gas_authoring_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('gas_authoring_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('gas_authoring_native_api_unavailable','Build/load the exact GASAuthoring Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('gas_authoring_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_gas_blueprint(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_gas_blueprint",text)

def create_gameplay_ability_blueprint(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("create_gameplay_ability_blueprint",text)

def create_gameplay_effect_blueprint(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("create_gameplay_effect_blueprint",text)

def create_attribute_set_blueprint(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("create_attribute_set_blueprint",text)

def add_ability_system_component_to_blueprint(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_ability_system_component_to_blueprint",text)

def add_gameplay_attribute_to_attribute_set(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_gameplay_attribute_to_attribute_set",text)

