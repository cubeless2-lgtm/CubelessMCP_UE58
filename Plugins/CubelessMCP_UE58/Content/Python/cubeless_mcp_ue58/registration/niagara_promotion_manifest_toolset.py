"""HC original promotion manifest plan/execute through exact JSON value carriers; domain owns the actual ordered workflow."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import niagara_promotion_manifest as domain

@unreal.uclass()
class CubelessNiagaraPromotionManifestTools(unreal.ToolsetDefinition):
    "Original manifest planning, confirmation and full revalidation before ordered promotion with actual partial receipts."
    @toolset_registry.tool_call
    @staticmethod
    def plan_niagara_promotion_manifest(source_system_path:str="\"\"",source_system_paths:str="null",target_root:str="\"/Game/MCPWeather/Generated\"",target_system_paths:str="null",asset_name_prefix:str="\"\"",asset_name_suffix:str="\"\"",forbidden_dependency_prefixes:str="null",forbidden_source_roots:str="null",allow_non_temp_source:str="false",allow_temp_target:str="false",allow_temp_dependencies:str="false",write_manifest:str="false",manifest_name:str="\"\"",timeout_seconds:str="120.0")->CubelessPythonOperationResult:
        "Plan ordered source/target promotions, preserve per-entry blockers and confirmation token; optional Saved manifest file write."
        rejected,report,raw=_guard_admission("plan_niagara_promotion_manifest")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"source_system_path":_decode_value(source_system_path),"source_system_paths":_decode_value(source_system_paths),"target_root":_decode_value(target_root),"target_system_paths":_decode_value(target_system_paths),"asset_name_prefix":_decode_value(asset_name_prefix),"asset_name_suffix":_decode_value(asset_name_suffix),"forbidden_dependency_prefixes":_decode_value(forbidden_dependency_prefixes),"forbidden_source_roots":_decode_value(forbidden_source_roots),"allow_non_temp_source":_decode_value(allow_non_temp_source),"allow_temp_target":_decode_value(allow_temp_target),"allow_temp_dependencies":_decode_value(allow_temp_dependencies),"write_manifest":_decode_value(write_manifest),"manifest_name":_decode_value(manifest_name),"timeout_seconds":_decode_value(timeout_seconds)}
        except Exception as error:return reflected_result(_carrier_rejected("plan_niagara_promotion_manifest",error,report,raw))
        return reflected_result(domain._named_mcp("plan_niagara_promotion_manifest",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def execute_niagara_promotion_manifest(manifest:str="null",manifest_path:str="\"\"",confirmation_token:str="\"\"",require_confirmation:str="true",save:str="true",request_compile:str="true",write_execution_report:str="false",execution_report_name:str="\"\"",timeout_seconds:str="180.0")->CubelessPythonOperationResult:
        "Revalidate the full confirmed manifest before sequential promotions; preserve partial saved receipts and stop on uncertainty."
        rejected,report,raw=_guard_admission("execute_niagara_promotion_manifest")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"manifest":_decode_value(manifest),"manifest_path":_decode_value(manifest_path),"confirmation_token":_decode_value(confirmation_token),"require_confirmation":_decode_value(require_confirmation),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"write_execution_report":_decode_value(write_execution_report),"execution_report_name":_decode_value(execution_report_name),"timeout_seconds":_decode_value(timeout_seconds)}
        except Exception as error:return reflected_result(_carrier_rejected("execute_niagara_promotion_manifest",error,report,raw))
        return reflected_result(domain._named_mcp("execute_niagara_promotion_manifest",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraPromotionManifestAttachmentLibrary', None), 'verify_niagara_promotion_manifest_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraPromotionManifest adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraPromotionManifestTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_promotion_manifest_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraPromotionManifest adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_promotion_manifest_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_promotion_manifest_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_promotion_manifest_registration_carrier_invalid',str(error)))
    result['stage']='niagara_promotion_manifest_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
