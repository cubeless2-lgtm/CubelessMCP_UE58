"""UE5.8 dedicated NiagaraAuthoringInspection Native1 x6. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraAuthoringInspectionLibrary'
BINDINGS=["inspect_niagara_stack", "inspect_niagara_graph", "inspect_niagara_graph_actions", "inspect_niagara_custom_hlsl_nodes", "inspect_niagara_scratch_pad_interface", "inspect_niagara_compile_status"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_niagara_stack": ["system_path", "include_pins", "max_function_calls"], "inspect_niagara_graph": ["system_path", "include_pins", "include_links", "include_scratch_pads", "max_nodes_per_graph", "max_links_per_graph"], "inspect_niagara_graph_actions": ["system_path", "scratch_pad_name", "scratch_pad_script_path", "scratch_pad_script_index", "safe_only", "search_text", "max_actions", "include_canonical", "include_template_pins"], "inspect_niagara_custom_hlsl_nodes": ["system_path", "node_guid", "signature_name", "contains_text", "max_hlsl_chars"], "inspect_niagara_scratch_pad_interface": ["system_path", "include_graph_summary", "include_parent_scratch_pads", "max_scripts", "max_function_calls"], "inspect_niagara_compile_status": ["system_path", "request_compile", "force", "allow_source_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_authoring_inspection_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_authoring_inspection_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_authoring_inspection_native_api_unavailable','Build/load the exact NiagaraAuthoringInspection Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_authoring_inspection_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_niagara_stack(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_stack",text)

def inspect_niagara_graph(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_graph",text)

def inspect_niagara_graph_actions(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_graph_actions",text)

def inspect_niagara_custom_hlsl_nodes(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_custom_hlsl_nodes",text)

def inspect_niagara_scratch_pad_interface(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_scratch_pad_interface",text)

def inspect_niagara_compile_status(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_compile_status",text)


# HX restores only public Named compile status using the already installed HN Job.
from . import niagara_scratch_user_recipes as _compile_jobs
ASYNC_METHODS=('inspect_niagara_compile_status',)
def async_binding_status():
    status=_compile_jobs.binding_status()
    return {key:value for key,value in status.items() if key=='inspect_niagara_compile_status' or key.startswith('job_')}

def start_job(method,params):
    if method not in ASYNC_METHODS:
        error=OperationError('niagara_authoring_inspection_job_method_invalid','Only compile-status uses the existing HN Job')
        error.native_start_dispatched=False;raise error
    return _compile_jobs.start_job(method,params)

def snapshot(job,method):
    if method not in ASYNC_METHODS:raise OperationError('niagara_authoring_inspection_job_method_invalid','Only compile-status uses the existing HN Job')
    return _compile_jobs.snapshot(job,method)
