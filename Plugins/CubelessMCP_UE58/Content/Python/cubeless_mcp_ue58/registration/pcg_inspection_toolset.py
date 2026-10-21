"""Original logical PCG inspection4 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import pcg_inspection as domain

@unreal.uclass()
class CubelessPCGInspectionTools(unreal.ToolsetDefinition):
    'Four dedicated PCG inspection APIs preserve graph candidates, original ordered nodes/pins/edges, Custom HLSL reflected source/declaration truncation and generated-output whole/included/sample counts. Explicit Engine writers, generation, refresh, compile and save are zero; loader/getter/cache effects remain globally unknown. Resolve retains original write/medium admission while the other three retain read policy. Existing PCG construction GC gate is unchanged. UE58 type identifiers and effective HLSL counts are reported separately from legacy/deprecated projections. No retry, atomic rollback or Cubeless runtime dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def resolve_pcg_graph(graph_path:str)->CubelessPythonOperationResult:
        'Dedicated resolve_pcg_graph preserves its original public/direct/raw inputs, legacy flat result, limits and actual observation prefix. No generation, compile, save or global purity claim; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('resolve_pcg_graph')
        if rejected is not None:return reflected_result(rejected)
        try:values={'graph_path':_decode_value(graph_path)}
        except Exception as error:return reflected_result(_carrier_rejected('resolve_pcg_graph',error,report,raw))
        return reflected_result(domain._named_mcp('resolve_pcg_graph',values))

    @toolset_registry.tool_call
    @staticmethod
    def list_pcg_graph_nodes(graph_path:str,node_type:str='""',title_contains:str='""',include_pins:str='true')->CubelessPythonOperationResult:
        'Dedicated list_pcg_graph_nodes preserves its original public/direct/raw inputs, legacy flat result, limits and actual observation prefix. No generation, compile, save or global purity claim; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('list_pcg_graph_nodes')
        if rejected is not None:return reflected_result(rejected)
        try:values={'graph_path':_decode_value(graph_path),'node_type':_decode_value(node_type),'title_contains':_decode_value(title_contains),'include_pins':_decode_value(include_pins)}
        except Exception as error:return reflected_result(_carrier_rejected('list_pcg_graph_nodes',error,report,raw))
        return reflected_result(domain._named_mcp('list_pcg_graph_nodes',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_pcg_custom_hlsl_nodes(graph_path:str,node_id:str='""',title_contains:str='""',include_pins:str='true',include_declarations:str='true',max_source_chars:str='4000')->CubelessPythonOperationResult:
        'Dedicated inspect_pcg_custom_hlsl_nodes preserves its original public/direct/raw inputs, legacy flat result, limits and actual observation prefix. No generation, compile, save or global purity claim; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_pcg_custom_hlsl_nodes')
        if rejected is not None:return reflected_result(rejected)
        try:values={'graph_path':_decode_value(graph_path),'node_id':_decode_value(node_id),'title_contains':_decode_value(title_contains),'include_pins':_decode_value(include_pins),'include_declarations':_decode_value(include_declarations),'max_source_chars':_decode_value(max_source_chars)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_pcg_custom_hlsl_nodes',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_pcg_custom_hlsl_nodes',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_pcg_component_outputs(actor_label:str='""',actor_name:str='""',actor_path:str='""',actor_tag:str='""',actor_label_prefix:str='""',component_name:str='""',component_tag:str='""',selected_only:str='false',max_components:str='16',max_tagged_data:str='64',max_points:str='8',include_point_samples:str='true')->CubelessPythonOperationResult:
        'Dedicated inspect_pcg_component_outputs preserves its original public/direct/raw inputs, legacy flat result, limits and actual observation prefix. No generation, compile, save or global purity claim; actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_pcg_component_outputs')
        if rejected is not None:return reflected_result(rejected)
        try:values={'actor_label':_decode_value(actor_label),'actor_name':_decode_value(actor_name),'actor_path':_decode_value(actor_path),'actor_tag':_decode_value(actor_tag),'actor_label_prefix':_decode_value(actor_label_prefix),'component_name':_decode_value(component_name),'component_tag':_decode_value(component_tag),'selected_only':_decode_value(selected_only),'max_components':_decode_value(max_components),'max_tagged_data':_decode_value(max_tagged_data),'max_points':_decode_value(max_points),'include_point_samples':_decode_value(include_point_samples)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_pcg_component_outputs',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_pcg_component_outputs',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessPCGInspectionAttachmentLibrary', None), 'verify_pcg_inspection_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact PCGInspection adapter raw-input admission helper is unavailable')
        raw = helper(CubelessPCGInspectionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_pcg_inspection_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded PCGInspection adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('pcg_inspection_raw_guard_admission_missing', message))
    result['stage'] = 'pcg_inspection_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('pcg_inspection_registration_carrier_invalid',str(error)))
    result['stage']='pcg_inspection_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
