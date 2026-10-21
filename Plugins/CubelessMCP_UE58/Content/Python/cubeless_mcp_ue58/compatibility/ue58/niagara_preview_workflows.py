"""UE5.8 dedicated Niagara Preview Lab Native1 x4. No transport, fallback, readiness or hidden progress."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraPreviewWorkflowLibrary'
BINDINGS=["open_niagara_preview_player", "get_niagara_preview_player_state", "preview_niagara_component_asset_swap", "sample_niagara_system_in_preview_lab"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"open_niagara_preview_player": ["system_path"], "get_niagara_preview_player_state": [], "preview_niagara_component_asset_swap": ["replacement_system_path", "actor_name", "actor_label", "component_name", "system_path", "parameter_name_contains", "restore_original_asset", "preserve_override_parameters", "activate_after_swap", "advance_time", "advance_tick_delta", "max_parameters", "max_emitter_instances", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_emitter_runtime", "max_object_properties", "max_array_elements", "max_property_value_chars"], "sample_niagara_system_in_preview_lab": ["system_path", "output_dir", "label", "warmup_times", "views", "warmup_tick_delta", "cleanup_before", "cleanup_after_each", "cleanup_after_all", "location", "scale", "auto_frame", "camera_location", "camera_rotation", "camera_fov", "user_parameter_overrides", "preserve_override_parameters", "allow_current_map_preview", "view_location", "view_rotation", "view_fov"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_preview_workflows_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_preview_workflows_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_preview_workflows_native_api_unavailable','Build/load the exact NiagaraPreviewWorkflow Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_preview_workflows_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def open_niagara_preview_player(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("open_niagara_preview_player",text)

def get_niagara_preview_player_state(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("get_niagara_preview_player_state",text)

def preview_niagara_component_asset_swap(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("preview_niagara_component_asset_swap",text)

def sample_niagara_system_in_preview_lab(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("sample_niagara_system_in_preview_lab",text)

# The same domain owns only these three internal Named-MCP continuations.
import math
JOB_STARTS={"open_niagara_preview_player": "start_open_player_job", "preview_niagara_component_asset_swap": "start_preview_component_swap_job", "sample_niagara_system_in_preview_lab": "start_sample_lab_job", "preview_niagara_system_in_preview_lab": "start_preview_lab_job"}
JOB_CLASS_PATH='/Script/CubelessMCPUE58Editor.CubelessNiagaraPreviewWorkflowJob'
def job_binding_status():
    library=getattr(unreal,LIBRARY,None);job=getattr(unreal,'CubelessNiagaraPreviewWorkflowJob',None)
    return dict(start_callable=all(callable(getattr(library,name,None)) for name in JOB_STARTS.values()),job_class_available=job is not None,job_snapshot_callable=callable(getattr(job,'get_snapshot_json',None)),job_cancel_callable=callable(getattr(job,'cancel_job',None)))

def _job_carrier(params):
    if type(params) is not dict:raise OperationError('niagara_preview_workflow_job_input_invalid','Native raw params object required')
    return _carrier(params)

def start_job(method,params):
    text=_job_carrier(params);function=getattr(getattr(unreal,LIBRARY,None),JOB_STARTS.get(method,''),None)
    if not callable(function):
        error=OperationError('niagara_preview_workflow_job_native_unavailable','Dedicated Native start is unavailable; no fallback or start dispatched');error.native_start_dispatched=False;raise error
    if method=='preview_niagara_system_in_preview_lab':
        try:job=function(text)
        finally:function=None
    else:job=function(text)
    try:
        cls=getattr(unreal,'CubelessNiagaraPreviewWorkflowJob',None)
        if cls is None or not isinstance(job,cls) or job.get_class().get_path_name()!=JOB_CLASS_PATH or not callable(getattr(job,'get_snapshot_json',None)) or not callable(getattr(job,'cancel_job',None)):
            raise OperationError('niagara_preview_workflow_job_identity_invalid','Returned job differs; dispatched start must not be repeated')
    except Exception as error:error.native_start_returned=True;error.native_returned_job=job;raise
    return job

def snapshot(job,method):
    raw=job.get_snapshot_json();value=None
    try:
        value=_decode_result(raw);data=value['data'];revision=data.get('native_checkpoint_revision')
        if (value.get('operation')!=method or type(data.get('native_job_id')) is not str or not data['native_job_id'] or type(data.get('native_job_terminal')) is not bool or type(data.get('operation_complete')) is not bool or data['operation_complete']!=data['native_job_terminal'] or type(revision) not in (int,float) or not math.isfinite(revision) or revision<0 or revision!=int(revision) or revision>9007199254740991):
            raise OperationError('niagara_preview_workflow_job_snapshot_invalid','Exact operation/job/revision/terminal Native receipt required')
        public_terminal=job.get_editor_property('is_terminal')
        if type(public_terminal) is not bool or public_terminal is not data['native_job_terminal']:
            raise OperationError('niagara_preview_workflow_job_publication_mismatch','Job terminal publication differs from returned snapshot; no replay')
    except Exception as error:
        error.native_snapshot_returned=True;error.native_raw_result=raw
        if value is not None:error.native_decoded_result=value
        raise
    value['_raw_native_json']=raw;return value
