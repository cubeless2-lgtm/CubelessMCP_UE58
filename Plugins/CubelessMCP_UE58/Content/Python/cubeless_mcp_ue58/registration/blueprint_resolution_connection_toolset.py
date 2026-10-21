"""HM original resolve1 and general K2 connect9 guarded public carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import blueprint_resolution_connection as domain

@unreal.uclass()
class CubelessBlueprintResolutionConnectionTools(unreal.ToolsetDefinition):
    "Blueprint candidate resolution and original general K2 node connections."
    @toolset_registry.tool_call
    @staticmethod
    def resolve_blueprint(blueprint_name:str)->CubelessPythonOperationResult:
        "Resolve Blueprint name/path/class/LevelBlueprint and ordered candidate paths. resolvedFalse is a valid diagnostic; no explicit compiler/save or new graph."
        rejected,report,raw=_guard_admission("resolve_blueprint")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"blueprint_name":_decode_value(blueprint_name)}
        except Exception as error:return reflected_result(_carrier_rejected("resolve_blueprint",error,report,raw))
        return reflected_result(domain._named_mcp("resolve_blueprint",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def connect_blueprint_nodes(blueprint_name:str,source_node_id:str,source_pin:str,target_node_id:str,target_pin:str,graph_name:str="\"\"",graph_id:str="\"\"",graph_type:str="\"\"",allow_pin_link_replacement:str="false")->CubelessPythonOperationResult:
        "Connect original GUID-selected output/input pins through K2 CanCreateConnection and TryCreateConnection. Keep default allow=False with MAKE/promotion and allow=True conversion/replacement semantics, implicit EventGraph creation, actual partial prefix and no replay; no explicit compile/save."
        rejected,report,raw=_guard_admission("connect_blueprint_nodes")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"blueprint_name":_decode_value(blueprint_name),"source_node_id":_decode_value(source_node_id),"source_pin":_decode_value(source_pin),"target_node_id":_decode_value(target_node_id),"target_pin":_decode_value(target_pin),"graph_name":_decode_value(graph_name),"graph_id":_decode_value(graph_id),"graph_type":_decode_value(graph_type),"allow_pin_link_replacement":_decode_value(allow_pin_link_replacement)}
        except Exception as error:return reflected_result(_carrier_rejected("connect_blueprint_nodes",error,report,raw))
        return reflected_result(domain._named_mcp("connect_blueprint_nodes",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessBlueprintResolutionConnectionAttachmentLibrary', None), 'verify_blueprint_resolution_connection_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact BlueprintResolutionConnection adapter raw-input admission helper is unavailable')
        raw = helper(CubelessBlueprintResolutionConnectionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_blueprint_resolution_connection_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded BlueprintResolutionConnection adapter delegation is required; no Engine operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('blueprint_resolution_connection_raw_guard_admission_missing', message))
    result['stage'] = 'blueprint_resolution_connection_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('blueprint_resolution_connection_registration_carrier_invalid',str(error)))
    result['stage']='blueprint_resolution_connection_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
