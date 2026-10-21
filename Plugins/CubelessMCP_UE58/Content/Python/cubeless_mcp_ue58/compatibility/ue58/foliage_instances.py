"""UE5.8 foliage current-level Native0/2/5. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessFoliageInstanceLibrary'
BINDINGS=('list_foliage_types','add_foliage_instances','remove_foliage_instances')
MAX_CARRIER_UTF16_UNITS=1048576
RAW_REMOVE_FIELDS=('instance_indices','remove_all','max_remove_count')


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('foliage_instances_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('foliage_instances_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _private_remove_context(legacy):
    if legacy is None:return None
    if type(legacy) is not dict or any(k not in RAW_REMOVE_FIELDS for k in legacy):
        raise OperationError('foliage_instances_raw_context_invalid','Only original indices/remove_all/max raw presence3 are allowed')
    return copy.deepcopy(legacy)


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('foliage_instances_native_api_unavailable','Build/load the exact foliage Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('foliage_instances_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result


def list_foliage_types():return _invoke('list_foliage_types')


def add_foliage_instances(foliage_type_path,transforms):
    try:text=_carrier(transforms)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('add_foliage_instances',foliage_type_path,text)


def remove_foliage_instances(foliage_type_path,instance_indices=None,remove_all=False,max_remove_count=5000,*,_legacy_command_params=None):
    try:
        context=_private_remove_context(_legacy_command_params)
        if context is None:args=(foliage_type_path,_carrier(instance_indices),remove_all,max_remove_count,'')
        else:args=(foliage_type_path,'[]',False,5000,_carrier(context))
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('remove_foliage_instances',*args)
