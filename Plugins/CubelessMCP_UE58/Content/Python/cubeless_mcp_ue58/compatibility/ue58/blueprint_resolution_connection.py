"""HM Blueprint resolution and original general K2 connection; no RPC/exec."""
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessBlueprintResolutionConnectionLibrary'
BINDINGS=["resolve_blueprint", "connect_blueprint_nodes"]
PARAM_FIELDS={"resolve_blueprint": ["blueprint_name"], "connect_blueprint_nodes": ["blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin", "graph_name", "graph_id", "graph_type", "allow_pin_link_replacement", "create_graph_if_missing"]}
MAX_CARRIER_UTF16_UNITS=1048576


def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def _carrier(params):
    if type(params) is not dict:raise OperationError('blueprint_resolution_connection_carrier_invalid','Object parameters required')
    try:
        text=json.dumps(params,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,UnicodeError,OverflowError,RecursionError) as error:
        raise OperationError('blueprint_resolution_connection_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:
        raise OperationError('blueprint_resolution_connection_carrier_bound','Carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    function=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(function):
        error=OperationError('blueprint_resolution_connection_native_api_unavailable','Exact HL Native binding is unavailable; no call dispatched')
        error.native_call_dispatched=False;raise error
    try:
        raw=function(text)
    finally:
        function=None
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('blueprint_resolution_connection_native_result_invalid','Exact operation and typed changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result


def resolve_blueprint(params):return _invoke('resolve_blueprint',params)

def connect_blueprint_nodes(params):return _invoke('connect_blueprint_nodes',params)


def connect_blueprint_graph_pins(params):return _invoke('connect_blueprint_graph_pins',params)
