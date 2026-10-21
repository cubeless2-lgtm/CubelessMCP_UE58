"""GV exact Custom HLSL replacement/graph recipe2 through original JSON carriers; Native owns compile-ID and partial restore."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_custom_hlsl_conversion as domain

@unreal.uclass()
class CubelessNiagaraCustomHlslConversionTools(unreal.ToolsetDefinition):
    "Two original public15/15 contracts. Native owns exact selectors, node/text invalidation, actual Cached/Pending compile-ID proofs and dirty partial recovery. Named final is delivered once from the same Native Job after actual compile-ID/restore/save proof. Direct/raw genuine wait stays prewriter refused. Scheduling or schema readiness is not domain completion."
    @toolset_registry.tool_call
    @staticmethod
    def apply_niagara_scratch_pad_graph_recipe(system_path:str,custom_hlsl_node_guid:str,nodes:str,links:str,output_mappings:str,scratch_pad_name:str="\"\"",scratch_pad_script_path:str="\"\"",scratch_pad_script_index:str="null",expected_hlsl_body_hash:str="\"\"",dry_run:str="true",remove_custom_hlsl:str="true",save:str="true",request_compile:str="true",timeout_seconds:str="30.0",poll_interval_seconds:str="0.05")->CubelessPythonAsyncOperationResult:
        "\n        Apply an allowlisted native-node recipe to one Custom HLSL node in a ghost system.\n\n        Args:\n            system_path: Target under /Game/_MCP_Temp/NiagaraGenerated only\n            custom_hlsl_node_guid: Exact Custom HLSL node GUID from inspection\n            nodes: Native graph-action nodes with node_id, action_id, optional x/y/defaults\n            links: Source/target endpoint objects connecting created nodes or Custom HLSL inputs\n            output_mappings: Mapping from every converted Custom HLSL output to its native source\n            scratch_pad_name: Optional exact Scratch Pad script name selector\n            scratch_pad_script_path: Optional exact private Scratch Pad object path selector\n            scratch_pad_script_index: Optional system-owned Scratch Pad index selector\n            expected_hlsl_body_hash: Required dry-run/readback hash when dry_run is false\n            dry_run: Validate selectors and action ids without mutating; defaults true\n            remove_custom_hlsl: Remove the converted node after pre-removal compile validation\n            save: Save only after final compile, pending-ID binding, and dependency checks pass\n            request_compile: Apply mode requires owning-system compile validation\n            timeout_seconds: Maximum wait for each compile phase\n            poll_interval_seconds: Niagara compilation polling interval\n\n        Apply uses non-force owning-system compiles and proves that each caller's\n        pending ComputedId becomes the post-compile CachedId/ComputedId. After any\n        post-mutation failure it restores visible graph structure, keeps the ghost\n        package dirty, and requires an exact package reload; Undo alone is unproven\n        and it never reports a clean full rollback.\n        "
        rejected,report,raw=_guard_admission("apply_niagara_scratch_pad_graph_recipe")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"system_path":_decode_value(system_path),"custom_hlsl_node_guid":_decode_value(custom_hlsl_node_guid),"nodes":_decode_value(nodes),"links":_decode_value(links),"output_mappings":_decode_value(output_mappings),"scratch_pad_name":_decode_value(scratch_pad_name),"scratch_pad_script_path":_decode_value(scratch_pad_script_path),"scratch_pad_script_index":_decode_value(scratch_pad_script_index),"expected_hlsl_body_hash":_decode_value(expected_hlsl_body_hash),"dry_run":_decode_value(dry_run),"remove_custom_hlsl":_decode_value(remove_custom_hlsl),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("apply_niagara_scratch_pad_graph_recipe",error,report,raw))
        return domain._named_mcp("apply_niagara_scratch_pad_graph_recipe",arguments)

    @toolset_registry.tool_call
    @staticmethod
    def replace_niagara_custom_hlsl(system_path:str,replacement_hlsl:str,node_guid:str="\"\"",signature_name:str="\"\"",contains_text:str="\"\"",scratch_pad_name:str="\"\"",source_path_contains:str="\"\"",expected_contains:str="\"\"",dry_run:str="true",allow_source_edit:str="false",save:str="true",request_compile:str="true",timeout_seconds:str="30.0",poll_interval_seconds:str="0.05",max_hlsl_chars:str="8000")->CubelessPythonAsyncOperationResult:
        "\n        Replace exactly one Niagara Custom HLSL node after selector and expected-text checks.\n\n        Args:\n            system_path: Niagara System package path or object path\n            replacement_hlsl: Full replacement HLSL string\n            node_guid: Optional node GUID filter\n            signature_name: Optional signature filter\n            contains_text: Optional case-sensitive substring selector\n            scratch_pad_name: Optional Scratch Pad script name filter\n            source_path_contains: Optional case-sensitive source path substring filter\n            expected_contains: Optional required substring in the current HLSL before replacing\n            dry_run: Preview replacement without mutation by default\n            allow_source_edit: Allow edits outside /Game/_MCP_Temp\n            save: Save only after postcondition verification passes\n            request_compile: Launch, wait for, and verify Niagara compiled-data replacement\n            timeout_seconds: Maximum wait for pre-existing and requested compilation\n            poll_interval_seconds: Niagara compilation polling interval\n            max_hlsl_chars: Maximum before/after HLSL characters returned\n        "
        rejected,report,raw=_guard_admission("replace_niagara_custom_hlsl")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"system_path":_decode_value(system_path),"replacement_hlsl":_decode_value(replacement_hlsl),"node_guid":_decode_value(node_guid),"signature_name":_decode_value(signature_name),"contains_text":_decode_value(contains_text),"scratch_pad_name":_decode_value(scratch_pad_name),"source_path_contains":_decode_value(source_path_contains),"expected_contains":_decode_value(expected_contains),"dry_run":_decode_value(dry_run),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds),"max_hlsl_chars":_decode_value(max_hlsl_chars)}
        except Exception as error:return domain.completed_result(_carrier_rejected("replace_niagara_custom_hlsl",error,report,raw))
        return domain._named_mcp("replace_niagara_custom_hlsl",arguments)


def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraCustomHlslConversionAttachmentLibrary', None), 'verify_niagara_custom_hlsl_conversion_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraCustomHlslConversion adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraCustomHlslConversionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_custom_hlsl_conversion_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraCustomHlslConversion adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_custom_hlsl_conversion_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_custom_hlsl_conversion_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_custom_hlsl_conversion_registration_carrier_invalid',str(error)))
    result['stage']='niagara_custom_hlsl_conversion_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
