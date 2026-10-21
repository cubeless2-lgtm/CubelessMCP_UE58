"""Original logical editor level session6/1/6/8 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import editor_level_session as operation

@unreal.uclass()
class CubelessEditorLevelSessionTools(unreal.ToolsetDefinition):
    'Editor level sessions: guarded preflight/deferred level open, latest request status, all-target dirty flag admission, and safe temporary preview map creation/save. Scheduling is not LoadMap completion. Preserve original raw aliases and public MCP conversions, partial directory/map/save/dirty receipts, global SDK unknown, and no uncertain retry. Status opens no mutation journal. Weather driver ownership cleanup parity remains incomplete without game asset dependencies.'
    @toolset_registry.tool_call
    @staticmethod
    def open_editor_level(level_path:str,dry_run:str='true',allow_dirty_packages:str='false',load_as_template:str='false',show_progress:str='true',deferred_load_delay_seconds:str='1.0')->CubelessPythonOperationResult:
        'Preflight or schedule opening an Editor level. Native request_id links scheduling to latest status; actual LoadMap runs synchronously later and is not completed by schedule success. Default dry_run protects dirty content.'
        rejected,report,raw=_guard_admission('open_editor_level')
        if rejected is not None:return reflected_result(rejected)
        try:values={'level_path':_decode_value(level_path),'dry_run':_decode_value(dry_run),'allow_dirty_packages':_decode_value(allow_dirty_packages),'load_as_template':_decode_value(load_as_template),'show_progress':_decode_value(show_progress),'deferred_load_delay_seconds':_decode_value(deferred_load_delay_seconds)}
        except Exception as error:return reflected_result(_carrier_rejected('open_editor_level',error,report,raw))
        return reflected_result(operation._named_mcp('open_editor_level',values))

    @toolset_registry.tool_call
    @staticmethod
    def get_editor_level_open_status(request_id:str='""')->CubelessPythonOperationResult:
        'Observe latest Native-owned request status and callback receipt without a mutation journal. A request_id mismatch is diagnostic and does not retrieve old history; no returned journal is automatically updated.'
        rejected,report,raw=_guard_admission('get_editor_level_open_status')
        if rejected is not None:return reflected_result(rejected)
        try:values={'request_id':_decode_value(request_id)}
        except Exception as error:return reflected_result(_carrier_rejected('get_editor_level_open_status',error,report,raw))
        return reflected_result(operation._named_mcp('get_editor_level_open_status',values))

    @toolset_registry.tool_call
    @staticmethod
    def clear_dirty_package_flags(package_paths:str='null',package_path:str='""',clear_all_dirty:str='false',dry_run:str='true',allow_non_temp_path:str='false',required_root:str='"/Game/_MCP_Temp"')->CubelessPythonOperationResult:
        'Clear only admitted loaded package dirty flags after all-target checks. Preserve blocked/no-op diagnostics and separate legacy cleared call markers from actual IsDirty postconditions. This does not save, reload or roll back content.'
        rejected,report,raw=_guard_admission('clear_dirty_package_flags')
        if rejected is not None:return reflected_result(rejected)
        try:values={'package_paths':_decode_value(package_paths),'package_path':_decode_value(package_path),'clear_all_dirty':_decode_value(clear_all_dirty),'dry_run':_decode_value(dry_run),'allow_non_temp_path':_decode_value(allow_non_temp_path),'required_root':_decode_value(required_root)}
        except Exception as error:return reflected_result(_carrier_rejected('clear_dirty_package_flags',error,report,raw))
        return reflected_result(operation._named_mcp('clear_dirty_package_flags',values))

    @toolset_registry.tool_call
    @staticmethod
    def safe_new_preview_map(target_path:str='""',map_path:str='""',dry_run:str='true',allow_dirty_packages:str='false',overwrite_existing:str='false',allow_non_temp_path:str='false',required_root:str='"/Game/_MCP_Temp"',is_partitioned_world:str='false')->CubelessPythonOperationResult:
        'Preflight or create/save a preview map with original alias/default/root semantics. Track directory/NewMap/SaveMap prefix and actual saved/current world. New PIE/pending safety restrictions and Weather cleanup parity limits are explicit; no atomic rollback or uncertain replay.'
        rejected,report,raw=_guard_admission('safe_new_preview_map')
        if rejected is not None:return reflected_result(rejected)
        try:values={'target_path':_decode_value(target_path),'map_path':_decode_value(map_path),'dry_run':_decode_value(dry_run),'allow_dirty_packages':_decode_value(allow_dirty_packages),'overwrite_existing':_decode_value(overwrite_existing),'allow_non_temp_path':_decode_value(allow_non_temp_path),'required_root':_decode_value(required_root),'is_partitioned_world':_decode_value(is_partitioned_world)}
        except Exception as error:return reflected_result(_carrier_rejected('safe_new_preview_map',error,report,raw))
        return reflected_result(operation._named_mcp('safe_new_preview_map',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessEditorLevelSessionAttachmentLibrary', None), 'verify_editor_level_session_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact EditorLevelSession adapter raw-input admission helper is unavailable')
        raw = helper(CubelessEditorLevelSessionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_editor_level_session_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded EditorLevelSession adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('editor_level_session_raw_guard_admission_missing', message))
    result['stage'] = 'editor_level_session_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('editor_level_session_registration_carrier_invalid',str(error)))
    result['stage']='editor_level_session_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
