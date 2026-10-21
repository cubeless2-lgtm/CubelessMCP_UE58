"""UE5.8 dedicated ChaosClothAuthoring Native1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessChaosClothAuthoringLibrary'
BINDINGS=["configure_chaos_cloth_dataflow_node","configure_chaos_cloth_weight_map"]
MAX_CARRIER_UTF16_UNITS=67108864
MAX_JSON_DEPTH=64
MAX_VALUE_VISITS=2004096
PARAM_FIELDS={"configure_chaos_cloth_dataflow_node":["cloth_asset_path","node_name","property_values","expected_node_type","expected_dataflow_asset_path","expected_current","allow_connected_inputs","regenerate_asset","save_assets","dry_run"],"configure_chaos_cloth_weight_map":["cloth_asset_path","node_name","weight_map_name","weights","fill_value","lod_index","expected_node_type","expected_dataflow_asset_path","expected_vertex_count","expected_topology_md5","expected_weight_map_md5","expected_node_state_md5","expected_path_md5","require_runtime_binding","save_assets","dry_run"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    # Value visits differ from Native ReadNext notation tokens (which include container ends).
    # Keep both bounded without truncating the original normalized 1,000,000-weight input.
    pending=[(value,0)];visits=0
    try:
        while pending:
            row,depth=pending.pop();visits+=1
            if visits>MAX_VALUE_VISITS:raise ValueError('JSON value-visit bound exceeded')
            if isinstance(row,(dict,list,tuple)):
                depth+=1
                if depth>MAX_JSON_DEPTH:raise ValueError('JSON container depth bound exceeded')
                pending.extend((item,depth) for item in (row.values() if isinstance(row,dict) else row))
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('chaos_cloth_authoring_carrier_invalid','Bounded finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('chaos_cloth_authoring_carrier_bound','Private carrier exceeds64Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('chaos_cloth_authoring_native_api_unavailable','Build/load the exact ChaosClothAuthoring Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('chaos_cloth_authoring_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result






def configure_chaos_cloth_dataflow_node(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('configure_chaos_cloth_dataflow_node',text)

def configure_chaos_cloth_weight_map(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('configure_chaos_cloth_weight_map',text)
