"""Original logical Niagara authoring inspection6 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_authoring_inspection as domain

@unreal.uclass()
class CubelessNiagaraAuthoringInspectionTools(unreal.ToolsetDefinition):
    'Six dedicated Editor-only Niagara authoring inspection APIs preserve stack/graph/pin/link budgets, Scratch Pad interfaces, exact HLSL text/filtering, action IDs and template-preview status, and actual script compile/pending receipts. Five observation intents do not explicitly edit source assets; loader/cache/transient SDK effects remain distinct and globally unknown. Compile status retains original optional request/write policy and explicit save0. Named compile status uses the existing HN finite Job; template preview and runtime verification remain scoped to actual Native receipts, never fake timeout/completion. No automatic retry, atomic rollback or Cubeless runtime dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_stack(system_path:str,include_pins:str='false',max_function_calls:str='200')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_stack preserves original public/direct/raw input and actual observation/compile/transient prefix contracts. Pending wait and template preview completion remain separately tracked; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_niagara_stack')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'include_pins':_decode_value(include_pins),'max_function_calls':_decode_value(max_function_calls)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_stack',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_stack',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_graph(system_path:str,include_pins:str='true',include_links:str='true',include_scratch_pads:str='true',max_nodes_per_graph:str='600',max_links_per_graph:str='2000')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_graph preserves original public/direct/raw input and actual observation/compile/transient prefix contracts. Pending wait and template preview completion remain separately tracked; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_niagara_graph')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'include_pins':_decode_value(include_pins),'include_links':_decode_value(include_links),'include_scratch_pads':_decode_value(include_scratch_pads),'max_nodes_per_graph':_decode_value(max_nodes_per_graph),'max_links_per_graph':_decode_value(max_links_per_graph)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_graph',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_graph',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_graph_actions(system_path:str,scratch_pad_name:str='""',scratch_pad_script_path:str='""',scratch_pad_script_index:str='null',safe_only:str='true',search_text:str='""',max_actions:str='600',include_canonical:str='false',include_template_pins:str='false')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_graph_actions preserves original public/direct/raw input and actual observation/compile/transient prefix contracts. Pending wait and template preview completion remain separately tracked; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_niagara_graph_actions')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'scratch_pad_name':_decode_value(scratch_pad_name),'scratch_pad_script_path':_decode_value(scratch_pad_script_path),'scratch_pad_script_index':_decode_value(scratch_pad_script_index),'safe_only':_decode_value(safe_only),'search_text':_decode_value(search_text),'max_actions':_decode_value(max_actions),'include_canonical':_decode_value(include_canonical),'include_template_pins':_decode_value(include_template_pins)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_graph_actions',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_graph_actions',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_custom_hlsl_nodes(system_path:str,node_guid:str='""',signature_name:str='""',contains_text:str='""',max_hlsl_chars:str='4000')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_custom_hlsl_nodes preserves original public/direct/raw input and actual observation/compile/transient prefix contracts. Pending wait and template preview completion remain separately tracked; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_niagara_custom_hlsl_nodes')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'node_guid':_decode_value(node_guid),'signature_name':_decode_value(signature_name),'contains_text':_decode_value(contains_text),'max_hlsl_chars':_decode_value(max_hlsl_chars)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_custom_hlsl_nodes',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_custom_hlsl_nodes',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_scratch_pad_interface(system_path:str,include_graph_summary:str='true',include_parent_scratch_pads:str='true',max_scripts:str='200',max_function_calls:str='80')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_scratch_pad_interface preserves original public/direct/raw input and actual observation/compile/transient prefix contracts. Pending wait and template preview completion remain separately tracked; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_niagara_scratch_pad_interface')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'include_graph_summary':_decode_value(include_graph_summary),'include_parent_scratch_pads':_decode_value(include_parent_scratch_pads),'max_scripts':_decode_value(max_scripts),'max_function_calls':_decode_value(max_function_calls)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_scratch_pad_interface',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_scratch_pad_interface',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_compile_status(system_path:str,request_compile:str='false',force:str='false',allow_source_compile:str='false',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Dedicated inspect_niagara_compile_status preserves original public/direct/raw input and actual observation/compile/transient prefix contracts. Named compile/wait uses the existing HN finite Native Job with one parent journal; timeout is an observation diagnostic, not compile-success proof. Direct/raw paths are unchanged; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_niagara_compile_status')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'request_compile':_decode_value(request_compile),'force':_decode_value(force),'allow_source_compile':_decode_value(allow_source_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('inspect_niagara_compile_status',error,report,raw))
        return domain._named_mcp('inspect_niagara_compile_status',values)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraAuthoringInspectionAttachmentLibrary', None), 'verify_niagara_authoring_inspection_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraAuthoringInspection adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraAuthoringInspectionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_authoring_inspection_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraAuthoringInspection adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_authoring_inspection_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_authoring_inspection_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_authoring_inspection_registration_carrier_invalid',str(error)))
    result['stage']='niagara_authoring_inspection_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
