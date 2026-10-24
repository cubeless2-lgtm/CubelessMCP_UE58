"""Original logical Niagara cleanup6 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_user_parameter_async as user_async
from cubeless_mcp_ue58.operations import niagara_graph_cleanup_async as graph_async
from cubeless_mcp_ue58.operations import niagara_cleanup as domain

@unreal.uclass()
class CubelessNiagaraCleanupTools(unreal.ToolsetDefinition):
    'Ordinary Batch with requested waiting now reuses the same held Native cleanup Job and existing Batch parent continuation for graph cleanup5. Original raw inputs/default wait=False, destructive partial/compile/save prefixes, exact Job/revision/current/cancel ownership and unknown-stop behavior remain preserved. Direct synchronous domain calls stay separate. This is API-owned composition, not an external pipeline. Real Batch cleanup wait/cancel/whole original scope remain usage validation pending.\n\nSix dedicated Editor-only Niagara cleanup APIs preserve original name/GUID/index selectors, nullable lists and raw aliases, candidate and exact removed/failed rows, ParameterMap bypass/orphan criteria, target-local Scratch Pads and user-store recreation gates. Compound mutation, compile/readback and actual save prefixes are preserved before any parent failure. Mandatory compile wait cannot be bypassed with waitFalse or scheduling-only success. Safe deferred final completion and save resume remain separately tracked until actual Toolset async lifetime is verified. Original destructive/high policy and confirmation facts are retained. No automatic retry, atomic rollback, private Engine headers or Cubeless runtime asset dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def remove_niagara_modules(system_path:str,module_names:str='null',module_name_prefixes:str='null',module_name_contains:str='null',module_name:str='""',emitter_index:str='null',emitter_name:str='""',include_system_modules:str='false',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Ordinary Batch with requested waiting now reuses the same held Native cleanup Job and existing Batch parent continuation for graph cleanup5. Original raw inputs/default wait=False, destructive partial/compile/save prefixes, exact Job/revision/current/cancel ownership and unknown-stop behavior remain preserved. Direct synchronous domain calls stay separate. This is API-owned composition, not an external pipeline. Real Batch cleanup wait/cancel/whole original scope remain usage validation pending.\n\nOriginal graph cleanup preserves selectors/defaults and returns final std11 through one held Native Job and parent journal. Native owns genuine compile wait and optional save resume, including timeout/partial prefixes. Direct Python and ordinary Raw Batch keep synchronous prewriter wait restrictions; no start replay or rollback.'
        rejected,report,raw=_guard_admission('remove_niagara_modules')
        if rejected is not None:return graph_async.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'module_names':_decode_value(module_names),'module_name_prefixes':_decode_value(module_name_prefixes),'module_name_contains':_decode_value(module_name_contains),'module_name':_decode_value(module_name),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'include_system_modules':_decode_value(include_system_modules),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return graph_async.completed_result(_carrier_rejected('remove_niagara_modules',error,report,raw))
        return graph_async.begin('remove_niagara_modules',values)

    @toolset_registry.tool_call
    @staticmethod
    def remove_niagara_parameter_map_set_nodes(system_path:str,parameter_pin_names:str='null',parameter_pin_name:str='""',node_guid:str='""',emitter_index:str='null',emitter_name:str='""',dry_run:str='true',allow_multiple:str='false',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Ordinary Batch with requested waiting now reuses the same held Native cleanup Job and existing Batch parent continuation for graph cleanup5. Original raw inputs/default wait=False, destructive partial/compile/save prefixes, exact Job/revision/current/cancel ownership and unknown-stop behavior remain preserved. Direct synchronous domain calls stay separate. This is API-owned composition, not an external pipeline. Real Batch cleanup wait/cancel/whole original scope remain usage validation pending.\n\nOriginal graph cleanup preserves selectors/defaults and returns final std11 through one held Native Job and parent journal. Native owns genuine compile wait and optional save resume, including timeout/partial prefixes. Direct Python and ordinary Raw Batch keep synchronous prewriter wait restrictions; no start replay or rollback.'
        rejected,report,raw=_guard_admission('remove_niagara_parameter_map_set_nodes')
        if rejected is not None:return graph_async.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'parameter_pin_names':_decode_value(parameter_pin_names),'parameter_pin_name':_decode_value(parameter_pin_name),'node_guid':_decode_value(node_guid),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'dry_run':_decode_value(dry_run),'allow_multiple':_decode_value(allow_multiple),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return graph_async.completed_result(_carrier_rejected('remove_niagara_parameter_map_set_nodes',error,report,raw))
        return graph_async.begin('remove_niagara_parameter_map_set_nodes',values)

    @toolset_registry.tool_call
    @staticmethod
    def remove_niagara_orphan_parameter_map_get_nodes(system_path:str,parameter_pin_names:str='null',parameter_pin_name:str='""',node_guid:str='""',emitter_index:str='null',emitter_name:str='""',dry_run:str='true',allow_multiple:str='false',only_if_unlinked:str='true',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Ordinary Batch with requested waiting now reuses the same held Native cleanup Job and existing Batch parent continuation for graph cleanup5. Original raw inputs/default wait=False, destructive partial/compile/save prefixes, exact Job/revision/current/cancel ownership and unknown-stop behavior remain preserved. Direct synchronous domain calls stay separate. This is API-owned composition, not an external pipeline. Real Batch cleanup wait/cancel/whole original scope remain usage validation pending.\n\nOriginal graph cleanup preserves selectors/defaults and returns final std11 through one held Native Job and parent journal. Native owns genuine compile wait and optional save resume, including timeout/partial prefixes. Direct Python and ordinary Raw Batch keep synchronous prewriter wait restrictions; no start replay or rollback.'
        rejected,report,raw=_guard_admission('remove_niagara_orphan_parameter_map_get_nodes')
        if rejected is not None:return graph_async.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'parameter_pin_names':_decode_value(parameter_pin_names),'parameter_pin_name':_decode_value(parameter_pin_name),'node_guid':_decode_value(node_guid),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'dry_run':_decode_value(dry_run),'allow_multiple':_decode_value(allow_multiple),'only_if_unlinked':_decode_value(only_if_unlinked),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return graph_async.completed_result(_carrier_rejected('remove_niagara_orphan_parameter_map_get_nodes',error,report,raw))
        return graph_async.begin('remove_niagara_orphan_parameter_map_get_nodes',values)

    @toolset_registry.tool_call
    @staticmethod
    def remove_niagara_user_parameters(system_path:str,parameter_names:str='null',parameter_name:str='""',dry_run:str='true',remove_graph_references:str='true',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='true',timeout_seconds:str='10',poll_interval_seconds:str='0.1',continue_on_error:str='false')->CubelessPythonAsyncOperationResult:
        'Dedicated remove_niagara_user_parameters preserves original Named/direct/raw selectors and compound removal/verification/failure/compile/save ordering. Actual savedTrue and unknown prefixes survive later failures. Final standard11 is delivered only after actual Native terminal compile/absence/conditional-save and durable journal reconciliation. Pending retains the exclusive project slot and one Job; Raw Batch still uses the original prewriter unsupported path. No retry or rollback.'
        rejected,report,raw=_guard_admission('remove_niagara_user_parameters')
        if rejected is not None:return user_async.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'parameter_names':_decode_value(parameter_names),'parameter_name':_decode_value(parameter_name),'dry_run':_decode_value(dry_run),'remove_graph_references':_decode_value(remove_graph_references),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds),'continue_on_error':_decode_value(continue_on_error)}
        except Exception as error:return user_async.completed_result(_carrier_rejected('remove_niagara_user_parameters',error,report,raw))
        return user_async.begin(values)

    @toolset_registry.tool_call
    @staticmethod
    def remove_niagara_simulation_stage(system_path:str,emitter_index:str='null',emitter_name:str='""',stage_index:str='null',stage_name:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Ordinary Batch with requested waiting now reuses the same held Native cleanup Job and existing Batch parent continuation for graph cleanup5. Original raw inputs/default wait=False, destructive partial/compile/save prefixes, exact Job/revision/current/cancel ownership and unknown-stop behavior remain preserved. Direct synchronous domain calls stay separate. This is API-owned composition, not an external pipeline. Real Batch cleanup wait/cancel/whole original scope remain usage validation pending.\n\nDedicated remove_niagara_simulation_stage preserves original Named/direct/raw selectors and compound removal/verification/failure/compile/save ordering. Actual savedTrue and unknown prefixes survive later failures. One held Native Job owns compile, then wait, then optional save and terminal std11; request_compile=False keeps the original ineffective wait flag. Timeout is an original observation, not an automatic failure. Direct domain calls stay synchronous; Batch waiting uses the parent continuation above; no replay or rollback.'
        rejected,report,raw=_guard_admission('remove_niagara_simulation_stage')
        if rejected is not None:return graph_async.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'stage_index':_decode_value(stage_index),'stage_name':_decode_value(stage_name),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return graph_async.completed_result(_carrier_rejected('remove_niagara_simulation_stage',error,report,raw))
        return graph_async.begin('remove_niagara_simulation_stage',values)

    @toolset_registry.tool_call
    @staticmethod
    def remove_niagara_scratch_pad_script(system_path:str,scratch_pad_name:str='""',scratch_pad_owner_kind:str='"any"',scratch_pad_script_index:str='null',scratch_pad_emitter_index:str='null',scratch_pad_emitter_name:str='""',force:str='false',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Ordinary Batch with requested waiting now reuses the same held Native cleanup Job and existing Batch parent continuation for graph cleanup5. Original raw inputs/default wait=False, destructive partial/compile/save prefixes, exact Job/revision/current/cancel ownership and unknown-stop behavior remain preserved. Direct synchronous domain calls stay separate. This is API-owned composition, not an external pipeline. Real Batch cleanup wait/cancel/whole original scope remain usage validation pending.\n\nDedicated remove_niagara_scratch_pad_script preserves original Named/direct/raw selectors and compound removal/verification/failure/compile/save ordering. Actual savedTrue and unknown prefixes survive later failures. One held Native Job owns compile, then optional save, then wait and terminal std11; wait is independent of the compile request. Timeout retains actual saved history and the original legacy observation. Direct domain calls stay synchronous; Batch waiting uses the parent continuation above; no replay or rollback.'
        rejected,report,raw=_guard_admission('remove_niagara_scratch_pad_script')
        if rejected is not None:return graph_async.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'scratch_pad_name':_decode_value(scratch_pad_name),'scratch_pad_owner_kind':_decode_value(scratch_pad_owner_kind),'scratch_pad_script_index':_decode_value(scratch_pad_script_index),'scratch_pad_emitter_index':_decode_value(scratch_pad_emitter_index),'scratch_pad_emitter_name':_decode_value(scratch_pad_emitter_name),'force':_decode_value(force),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return graph_async.completed_result(_carrier_rejected('remove_niagara_scratch_pad_script',error,report,raw))
        return graph_async.begin('remove_niagara_scratch_pad_script',values)

    @toolset_registry.tool_call
    @staticmethod
    def cleanup_generated_niagara_assets_offline(package_paths:str='null',name_prefix:str='""',older_than_minutes:str='null',dry_run:str='true',require_editor_closed:str='true',allow_all:str='false')->CubelessPythonOperationResult:
        'Original local-file cleanup6 under the generated Niagara Content root: explicit packages/prefix/age, six recognized sidecars, dry-run selection, process tri-state and immediate pre-delete recheck, per-file partial unlink errors. Default require_editor_closed=True refuses real deletion in the running Editor; explicit False is retained as the original opt-in branch. No UObject delete/load, Registry rescan, compile/save, server or RPC. The returned prefix is not global filesystem atomicity or loaded-reference cleanup.'
        rejected,report,raw=_guard_admission('cleanup_generated_niagara_assets_offline')
        if rejected is not None:return reflected_result(rejected)
        try:values={ 'package_paths':_decode_value(package_paths),'name_prefix':_decode_value(name_prefix),'older_than_minutes':_decode_value(older_than_minutes),'dry_run':_decode_value(dry_run),'require_editor_closed':_decode_value(require_editor_closed),'allow_all':_decode_value(allow_all) }
        except Exception as error:return reflected_result(_carrier_rejected('cleanup_generated_niagara_assets_offline',error,report,raw))
        return reflected_result(domain._named_mcp('cleanup_generated_niagara_assets_offline',values))


def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraCleanupAttachmentLibrary', None), 'verify_niagara_cleanup_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraCleanup adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraCleanupTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_cleanup_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraCleanup adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_cleanup_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_cleanup_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_cleanup_registration_carrier_invalid',str(error)))
    result['stage']='niagara_cleanup_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
