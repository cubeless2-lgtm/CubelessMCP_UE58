"""UE58 PCG graph editing3 dedicated Native ABI; authoritative gate and actual receipt, no fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessPCGGraphAuthoringLibrary'
BINDINGS=["add_pcg_node", "connect_pcg_nodes", "set_pcg_node_setting"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"add_pcg_node": ["graph_path", "settings_class", "node_position", "node_title", "settings"], "connect_pcg_nodes": ["graph_path", "from_node_id", "from_pin", "to_node_id", "to_pin"], "set_pcg_node_setting": ["graph_path", "node_id", "property_name", "value"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('pcg_graph_authoring_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('pcg_graph_authoring_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('pcg_graph_authoring_native_api_unavailable','Build/load the exact PCGGraphAuthoring Native API; no fallback')
        error.native_call_dispatched=False;raise error
    try:
        raw=fn(*args)
    finally:
        fn=None  # Release the native bound callable before decode or exception propagation.
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('pcg_graph_authoring_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def add_pcg_node(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_pcg_node",text)

def connect_pcg_nodes(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("connect_pcg_nodes",text)

def set_pcg_node_setting(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_pcg_node_setting",text)
