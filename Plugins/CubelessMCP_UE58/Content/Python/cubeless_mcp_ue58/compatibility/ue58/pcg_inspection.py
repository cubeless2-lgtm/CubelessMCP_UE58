"""UE5.8 dedicated PCGInspection Native1 x4. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessPCGInspectionLibrary'
BINDINGS=["resolve_pcg_graph","list_pcg_graph_nodes","inspect_pcg_custom_hlsl_nodes","inspect_pcg_component_outputs"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"resolve_pcg_graph":["graph_path"],"list_pcg_graph_nodes":["graph_path","node_type","title_contains","include_pins"],"inspect_pcg_custom_hlsl_nodes":["graph_path","node_id","title_contains","include_pins","include_declarations","max_source_chars"],"inspect_pcg_component_outputs":["actor_label","actor_name","actor_path","actor_tag","actor_label_prefix","component_name","component_tag","selected_only","max_components","max_tagged_data","max_points","include_point_samples"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('pcg_inspection_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('pcg_inspection_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('pcg_inspection_native_api_unavailable','Build/load the exact PCGInspection Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('pcg_inspection_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def resolve_pcg_graph(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("resolve_pcg_graph",text)

def list_pcg_graph_nodes(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("list_pcg_graph_nodes",text)

def inspect_pcg_custom_hlsl_nodes(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_pcg_custom_hlsl_nodes",text)

def inspect_pcg_component_outputs(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_pcg_component_outputs",text)
