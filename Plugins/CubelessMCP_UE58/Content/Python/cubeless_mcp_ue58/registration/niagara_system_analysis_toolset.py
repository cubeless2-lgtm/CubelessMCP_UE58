"""HB original Analyze18 public carrier, one Native metadata leaf and six existing compatibility observations."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import niagara_system_analysis as domain

@unreal.uclass()
class CubelessNiagaraSystemAnalysisTools(unreal.ToolsetDefinition):
    "Original read-only Niagara system analysis with sequential section receipts and partial legacy results."
    @toolset_registry.tool_call
    @staticmethod
    def analyze_niagara_system(system_path:str,include_renderers:str="true",include_user_parameters:str="true",include_stack:str="true",include_graph:str="true",include_module_inputs:str="true",include_compile_status:str="true",include_pins:str="false",include_links:str="false",include_scratch_pads:str="true",include_resolved_stack_inputs:str="false",max_function_calls:str="200",max_nodes_per_graph:str="300",max_links_per_graph:str="0",max_modules:str="200",max_candidates_per_module:str="24",max_resolved_inputs_per_module:str="8",max_top_candidates:str="80")->CubelessPythonOperationResult:
        "Compose the original six read-only sections and retain section failures, summary and actual typed receipts."
        rejected,report,raw=_guard_admission("analyze_niagara_system")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"system_path":_decode_value(system_path),"include_renderers":_decode_value(include_renderers),"include_user_parameters":_decode_value(include_user_parameters),"include_stack":_decode_value(include_stack),"include_graph":_decode_value(include_graph),"include_module_inputs":_decode_value(include_module_inputs),"include_compile_status":_decode_value(include_compile_status),"include_pins":_decode_value(include_pins),"include_links":_decode_value(include_links),"include_scratch_pads":_decode_value(include_scratch_pads),"include_resolved_stack_inputs":_decode_value(include_resolved_stack_inputs),"max_function_calls":_decode_value(max_function_calls),"max_nodes_per_graph":_decode_value(max_nodes_per_graph),"max_links_per_graph":_decode_value(max_links_per_graph),"max_modules":_decode_value(max_modules),"max_candidates_per_module":_decode_value(max_candidates_per_module),"max_resolved_inputs_per_module":_decode_value(max_resolved_inputs_per_module),"max_top_candidates":_decode_value(max_top_candidates)}
        except Exception as error:return reflected_result(_carrier_rejected("analyze_niagara_system",error,report,raw))
        return reflected_result(domain._named_mcp("analyze_niagara_system",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraSystemAnalysisAttachmentLibrary', None), 'verify_niagara_system_analysis_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraSystemAnalysis adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraSystemAnalysisTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_system_analysis_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraSystemAnalysis adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_system_analysis_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_system_analysis_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_system_analysis_registration_carrier_invalid',str(error)))
    result['stage']='niagara_system_analysis_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
