"""HF public PCG utilities through existing Python Editor SDK; no new Native leaf."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import pcg_utilities as domain


@unreal.uclass()
class CubelessPCGUtilityTools(unreal.ToolsetDefinition):
    "PCG asset/component inventory, nine-property debug best-effort and per-asset save receipts."
    @toolset_registry.tool_call
    @staticmethod
    def list_pcg_assets(root_path:str="\"/Game\"")->CubelessPythonOperationResult:
        "List recursive PCG-related AssetData in original Registry order; no asset load or PCG graph construction."
        rejected,report,raw=_guard_admission("list_pcg_assets")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"root_path":_decode_value(root_path)}
        except Exception as error:return reflected_result(_carrier_rejected("list_pcg_assets",error,report,raw))
        return reflected_result(domain._named_mcp("list_pcg_assets",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def list_pcg_components()->CubelessPythonOperationResult:
        "List PCG-like ActorComponents in the Editor level; retain the original actor label getter and its possible lazy label mutation."
        rejected,report,raw=_guard_admission("list_pcg_components")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={}
        except Exception as error:return reflected_result(_carrier_rejected("list_pcg_components",error,report,raw))
        return reflected_result(domain._named_mcp("list_pcg_components",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def set_pcg_debug_enabled(enabled:str="false",actor_name:str="\"\"",selected_only:str="false")->CubelessPythonOperationResult:
        "Try the original nine debug property aliases in order; preserve best-effort success names separately from observed property changes and partial failures."
        rejected,report,raw=_guard_admission("set_pcg_debug_enabled")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"enabled":_decode_value(enabled),"actor_name":_decode_value(actor_name),"selected_only":_decode_value(selected_only)}
        except Exception as error:return reflected_result(_carrier_rejected("set_pcg_debug_enabled",error,report,raw))
        return reflected_result(domain._named_mcp("set_pcg_debug_enabled",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def resave_pcg_assets(root_path:str="\"/Game\"")->CubelessPythonOperationResult:
        "Resave PCG-related assets in Registry order; preserve silent false results, exception rows and actual earlier save history."
        rejected,report,raw=_guard_admission("resave_pcg_assets")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"root_path":_decode_value(root_path)}
        except Exception as error:return reflected_result(_carrier_rejected("resave_pcg_assets",error,report,raw))
        return reflected_result(domain._named_mcp("resave_pcg_assets",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessPCGUtilityAttachmentLibrary', None), 'verify_pcg_utilities_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact PCGUtility adapter raw-input admission helper is unavailable')
        raw = helper(CubelessPCGUtilityTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_pcg_utilities_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded PCGUtility adapter delegation is required; no Engine operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('pcg_utilities_raw_guard_admission_missing', message))
    result['stage'] = 'pcg_utilities_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('pcg_utilities_registration_carrier_invalid',str(error)))
    result['stage']='pcg_utilities_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
