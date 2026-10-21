"""UE5.8 dedicated NiagaraParameterCollection Native1 x6. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraParameterCollectionLibrary'
BINDINGS=["inspect_niagara_parameter_collection_values", "set_niagara_parameter_collection_values", "repair_niagara_parameter_collection", "remap_niagara_parameter_collection_references", "remap_niagara_script_parameter_collection_references", "recompile_niagara_scripts_in_root"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_niagara_parameter_collection_values": ["collection_path", "parameter_names", "namespace", "max_parameters"], "set_niagara_parameter_collection_values": ["collection_path", "values", "namespace", "allow_source_edit", "save"], "repair_niagara_parameter_collection": ["collection_path", "allow_source_edit", "save"], "remap_niagara_parameter_collection_references": ["system_path", "new_collection_path", "old_collection_path", "old_namespace", "allow_source_edit", "save", "request_compile", "replace_object_references"], "remap_niagara_script_parameter_collection_references": ["new_collection_path", "old_collection_path", "old_namespace", "script_path", "script_paths", "root_path", "allow_source_edit", "save", "request_compile", "wait_for_completion"], "recompile_niagara_scripts_in_root": ["root_path", "script_paths", "allow_source_compile", "force", "save"]}


def binding_status():
    result={name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}
    from . import niagara_scratch_user_recipes as shared
    status=shared.binding_status()
    result['remap_niagara_script_parameter_collection_references']=result['remap_niagara_script_parameter_collection_references'] and all(status.get(key) is True for key in ('remap_niagara_script_parameter_collection_references','job_class_available','job_snapshot_callable','job_cancel_callable'))
    return result


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_parameter_collections_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_parameter_collections_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_parameter_collections_native_api_unavailable','Build/load the exact NiagaraParameterCollection Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_parameter_collections_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_niagara_parameter_collection_values(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_parameter_collection_values",text)

def set_niagara_parameter_collection_values(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_parameter_collection_values",text)

def repair_niagara_parameter_collection(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("repair_niagara_parameter_collection",text)

def remap_niagara_parameter_collection_references(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remap_niagara_parameter_collection_references",text)

def remap_niagara_script_parameter_collection_references(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remap_niagara_script_parameter_collection_references",text)

def recompile_niagara_scripts_in_root(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("recompile_niagara_scripts_in_root",text)
