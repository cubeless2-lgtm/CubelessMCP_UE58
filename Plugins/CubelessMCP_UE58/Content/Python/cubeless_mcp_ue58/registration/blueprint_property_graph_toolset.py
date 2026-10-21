"""HL original CDO property3 and graph metadata2 guarded public carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import blueprint_property_graph as domain

@unreal.uclass()
class CubelessBlueprintPropertyGraphTools(unreal.ToolsetDefinition):
    "CDO inherited-property writer and metadata-only graph inventory."
    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_property(blueprint_name:str,property_name:str,property_value:str)->CubelessPythonOperationResult:
        "Set one inherited CDO FProperty, then MarkBlueprintAsModified. No explicit compile/save; actual partial prefix and unknown SDK effects remain authoritative."
        rejected,report,raw=_guard_admission("set_blueprint_property")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"blueprint_name":_decode_value(blueprint_name),"property_name":_decode_value(property_name),"property_value":_decode_value(property_value)}
        except Exception as error:return reflected_result(_carrier_rejected("set_blueprint_property",error,report,raw))
        return reflected_result(domain._named_mcp("set_blueprint_property",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def list_blueprint_graphs(blueprint_name:str,graph_type:str="\"\"")->CubelessPythonOperationResult:
        "List existing Blueprint GetAllGraphs metadata in original order and graph_type filter. No explicit graph creation, compile/save; original success key is absent."
        rejected,report,raw=_guard_admission("list_blueprint_graphs")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"blueprint_name":_decode_value(blueprint_name),"graph_type":_decode_value(graph_type)}
        except Exception as error:return reflected_result(_carrier_rejected("list_blueprint_graphs",error,report,raw))
        return reflected_result(domain._named_mcp("list_blueprint_graphs",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessBlueprintPropertyGraphAttachmentLibrary', None), 'verify_blueprint_property_graph_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact BlueprintPropertyGraph adapter raw-input admission helper is unavailable')
        raw = helper(CubelessBlueprintPropertyGraphTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_blueprint_property_graph_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded BlueprintPropertyGraph adapter delegation is required; no Engine operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('blueprint_property_graph_raw_guard_admission_missing', message))
    result['stage'] = 'blueprint_property_graph_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('blueprint_property_graph_registration_carrier_invalid',str(error)))
    result['stage']='blueprint_property_graph_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
