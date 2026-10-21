"""UE5.8 dedicated ContentReconstruction Native1 x4. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessContentReconstructionLibrary'
BINDINGS=["recreate_content_folder_mcp", "postprocess_content_folder_mcp", "repair_world_actor_instances_mcp", "run_content_validation_pipeline_mcp"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"recreate_content_folder_mcp": ["source_root", "target_root", "suffix", "allow_fallback_duplicate", "dry_run", "overwrite_existing", "delete_target_root_first", "remap_references", "compile_blueprints", "log_archive_remap_objects", "refresh_blueprint_nodes", "refresh_failed_blueprint_nodes", "save_assets", "repair_level_actors", "force_repair_level_actors", "compile_passes", "check_editor_log_health", "fail_on_editor_log_issues", "suppress_editor_prompts", "accepted_fallback_rules"], "postprocess_content_folder_mcp": ["source_root", "target_root", "suffix", "dry_run", "remap_references", "compile_blueprints", "refresh_blueprint_nodes", "refresh_failed_blueprint_nodes", "save_assets", "repair_level_actors", "force_repair_level_actors", "compile_passes", "check_editor_log_health", "fail_on_editor_log_issues", "suppress_editor_prompts"], "repair_world_actor_instances_mcp": ["map_path", "source_root", "target_root", "suffix", "dry_run", "save_map", "remap_level_blueprint", "repair_actor_classes", "repair_component_classes", "remap_actor_references", "remove_source_object_map_keys", "check_editor_log_health", "fail_on_editor_log_issues", "suppress_editor_prompts"], "run_content_validation_pipeline_mcp": ["source_root", "target_root", "suffix", "map_path", "continue_on_failure", "run_world_repair", "allow_fallback_duplicate", "dry_run", "overwrite_existing", "delete_target_root_first", "remap_references", "compile_blueprints", "refresh_blueprint_nodes", "refresh_failed_blueprint_nodes", "save_assets", "repair_level_actors", "force_repair_level_actors", "compile_passes", "check_editor_log_health", "fail_on_editor_log_issues", "suppress_editor_prompts", "save_map", "repair_actor_classes", "repair_component_classes", "remap_actor_references", "remove_source_object_map_keys", "accepted_fallback_rules", "log_archive_remap_objects", "remap_level_blueprint"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('content_reconstruction_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('content_reconstruction_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('content_reconstruction_native_api_unavailable','Build/load the exact ContentReconstruction Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('content_reconstruction_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def recreate_content_folder_mcp(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("recreate_content_folder_mcp",text)

def postprocess_content_folder_mcp(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("postprocess_content_folder_mcp",text)

def repair_world_actor_instances_mcp(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("repair_world_actor_instances_mcp",text)

def run_content_validation_pipeline_mcp(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("run_content_validation_pipeline_mcp",text)

