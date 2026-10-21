"""HL CDO inherited-property write and graph metadata inventory; no RPC/exec."""
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessBlueprintPropertyGraphLibrary'
BINDINGS=["set_blueprint_property", "list_blueprint_graphs"]
PARAM_FIELDS={"set_blueprint_property": ["blueprint_name", "property_name", "property_value"], "list_blueprint_graphs": ["blueprint_name", "graph_type"]}
MAX_CARRIER_UTF16_UNITS=1048576


def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def _carrier(params):
    if type(params) is not dict:raise OperationError('blueprint_property_graph_carrier_invalid','Object parameters required')
    try:
        text=json.dumps(params,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,UnicodeError,OverflowError,RecursionError) as error:
        raise OperationError('blueprint_property_graph_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:
        raise OperationError('blueprint_property_graph_carrier_bound','Carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    function=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(function):
        error=OperationError('blueprint_property_graph_native_api_unavailable','Exact HL Native binding is unavailable; no call dispatched')
        error.native_call_dispatched=False;raise error
    raw=function(text)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('blueprint_property_graph_native_result_invalid','Exact operation and typed changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result


def set_blueprint_property(params):return _invoke('set_blueprint_property',params)

def list_blueprint_graphs(params):return _invoke('list_blueprint_graphs',params)
