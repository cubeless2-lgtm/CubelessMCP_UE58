"""Two original promotion contracts through bounded per-field JSON carriers; Native actual receipts precede strict semantic validation."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import niagara_promotion as domain
from cubeless_mcp_ue58.operations import niagara_source_template as source_template
from .async_interop_toolset import CubelessPythonAsyncOperationResult

@unreal.uclass()
class CubelessNiagaraPromotionTools(unreal.ToolsetDefinition):
    "Dedicated audit/promotion2. Exact source/target policy, registry audit scope, strict response binding and actual partial/save receipt; no transport or retry."
    @toolset_registry.tool_call
    @staticmethod
    def audit_niagara_promotion_candidate(system_path:str,proposed_target_path:str="\"\"",forbidden_dependency_prefixes:str="null",forbidden_source_roots:str="null",allow_temp_target:str="false",allow_temp_dependencies:str="false",timeout_seconds:str="60.0")->CubelessPythonOperationResult:
        "\n        Read-only preflight for promoting a generated Niagara System outside temp.\n\n        Args:\n            system_path: Niagara System package/object path to audit\n            proposed_target_path: Optional future destination package path; must be outside /Game/_MCP_Temp by default\n            forbidden_dependency_prefixes: Additional dependency roots to reject\n            forbidden_source_roots: Source/template roots to reject when the promoted asset must be independent\n            allow_temp_target: Allow proposed_target_path under /Game/_MCP_Temp\n            allow_temp_dependencies: Allow dependencies under /Game/_MCP_Temp\n            timeout_seconds: Unreal bridge timeout\n        "
        rejected,report,raw=_guard_admission("audit_niagara_promotion_candidate")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"system_path":_decode_value(system_path),"proposed_target_path":_decode_value(proposed_target_path),"forbidden_dependency_prefixes":_decode_value(forbidden_dependency_prefixes),"forbidden_source_roots":_decode_value(forbidden_source_roots),"allow_temp_target":_decode_value(allow_temp_target),"allow_temp_dependencies":_decode_value(allow_temp_dependencies),"timeout_seconds":_decode_value(timeout_seconds)}
        except Exception as error:return reflected_result(_carrier_rejected("audit_niagara_promotion_candidate",error,report,raw))
        result=domain._named_mcp("audit_niagara_promotion_candidate",arguments)
        return reflected_result(result)

    @toolset_registry.tool_call
    @staticmethod
    def promote_niagara_system_candidate(source_system_path:str,target_system_path:str,dry_run:str="true",forbidden_dependency_prefixes:str="null",forbidden_source_roots:str="null",allow_non_temp_source:str="false",allow_temp_target:str="false",allow_temp_dependencies:str="false",overwrite_existing:str="false",save:str="true",request_compile:str="true",timeout_seconds:str="120.0")->CubelessPythonOperationResult:
        "\n        Duplicate a generated Niagara System to a promotion target after dependency preflight.\n\n        Args:\n            source_system_path: Source Niagara System package/object path, generated temp only by default\n            target_system_path: New destination package/object path, non-temp /Game by default\n            dry_run: Validate only by default; pass false to duplicate and save\n            forbidden_dependency_prefixes: Additional dependency roots to reject before/after promotion\n            forbidden_source_roots: Source/template roots to reject when the promoted asset must be independent\n            allow_non_temp_source: Allow source outside generated temp\n            allow_temp_target: Allow target under /Game/_MCP_Temp, mainly for smoke tests\n            allow_temp_dependencies: Allow dependencies under /Game/_MCP_Temp\n            overwrite_existing: Reserved; currently refused\n            save: Save promoted package when dry_run is false\n            request_compile: Request Niagara compile on the promoted copy\n            timeout_seconds: Unreal bridge timeout\n        "
        rejected,report,raw=_guard_admission("promote_niagara_system_candidate")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"source_system_path":_decode_value(source_system_path),"target_system_path":_decode_value(target_system_path),"dry_run":_decode_value(dry_run),"forbidden_dependency_prefixes":_decode_value(forbidden_dependency_prefixes),"forbidden_source_roots":_decode_value(forbidden_source_roots),"allow_non_temp_source":_decode_value(allow_non_temp_source),"allow_temp_target":_decode_value(allow_temp_target),"allow_temp_dependencies":_decode_value(allow_temp_dependencies),"overwrite_existing":_decode_value(overwrite_existing),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"timeout_seconds":_decode_value(timeout_seconds)}
        except Exception as error:return reflected_result(_carrier_rejected("promote_niagara_system_candidate",error,report,raw))
        result=domain._named_mcp("promote_niagara_system_candidate",arguments)
        return reflected_result(result)

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_system_from_source_template(target_system_path:str,source_system_path:str,source_emitter_indices:str="null",copy_source_user_parameters:str="true",save:str="true",request_compile:str="true",wait_for_completion:str="true",timeout_seconds:str="180")->CubelessPythonAsyncOperationResult:
        """Duplicate the source system, retain selected emitters, save/compile only at the original last stage, await actual HN terminal status, then analyze. copy_source_user_parameters is compatibility-only; whole-system duplication preserves its defaults. Original8 logical inputs use guarded JSON-value carriers. Actual partial/save/unknown recovery is retained; no automatic cleanup/retry or runtime/GPU proof. Existing promotion/source/entry bounds remain; arbitrary callback purity is not proved."""
        rejected,report,raw=_guard_admission("create_niagara_system_from_source_template")
        if rejected is not None:return source_template.completed_result(rejected)
        try:
            values={"target_system_path":_decode_value(target_system_path),"source_system_path":_decode_value(source_system_path),
                "source_emitter_indices":_decode_value(source_emitter_indices),"copy_source_user_parameters":_decode_value(copy_source_user_parameters),
                "save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds)}
            source_template.activate()
        except Exception as error:return source_template.completed_result(_carrier_rejected("create_niagara_system_from_source_template",error,report,raw))
        return source_template._named_mcp("create_niagara_system_from_source_template",values)


def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraPromotionAttachmentLibrary', None), 'verify_niagara_promotion_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraPromotion adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraPromotionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_promotion_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraPromotion adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_promotion_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_promotion_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_promotion_registration_carrier_invalid',str(error)))
    result['stage']='niagara_promotion_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
