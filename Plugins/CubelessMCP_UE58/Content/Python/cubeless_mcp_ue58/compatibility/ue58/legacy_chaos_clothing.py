"""UE5.8 dedicated LegacyChaosClothing Native1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessLegacyChaosClothingLibrary'
BINDINGS=["create_legacy_chaos_clothing_from_skeletal_sections"]
MAX_CARRIER_UTF16_UNITS=1048576
MAX_JSON_DEPTH=64
MAX_VALUE_VISITS=262144
PARAM_FIELDS={"create_legacy_chaos_clothing_from_skeletal_sections":["source_skeletal_mesh_path","output_skeletal_mesh_path","section_configs","dry_run","save_assets","use_source_physics_asset","create_vroid_lower_body_cloth_physics_asset"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    # Value visits differ from Native ReadNext notation tokens (which include container ends).
    # Native validates its own notation-token bound and strict section batch.
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
        raise OperationError('legacy_chaos_clothing_carrier_invalid','Bounded finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('legacy_chaos_clothing_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('legacy_chaos_clothing_native_api_unavailable','Build/load the exact LegacyChaosClothing Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('legacy_chaos_clothing_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result






def create_legacy_chaos_clothing_from_skeletal_sections(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('create_legacy_chaos_clothing_from_skeletal_sections',text)
