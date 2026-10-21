"""HE original public PCG contracts; Native audit/spawner and Python read composition own actual state."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import pcg_contract_authoring as domain


@unreal.uclass()
class CubelessPCGContractAuthoringTools(unreal.ToolsetDefinition):
    "PCG contract audit, node read and typed spawner configuration with preserved partial/save receipts."
    @toolset_registry.tool_call
    @staticmethod
    def audit_pcg_graph_contract(graph_path:str,include_dependencies:str="true",forbidden_dependency_prefixes:str="null",fail_on_forbidden_dependencies:str="true")->CubelessPythonOperationResult:
        "Audit authored PCG nodes, pins, exact edge endpoints, spawner/actor selector state and direct package dependencies with original warning/fail policy."
        rejected,report,raw=_guard_admission("audit_pcg_graph_contract")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"graph_path":_decode_value(graph_path),"include_dependencies":_decode_value(include_dependencies),"forbidden_dependency_prefixes":_decode_value(forbidden_dependency_prefixes),"fail_on_forbidden_dependencies":_decode_value(fail_on_forbidden_dependencies)}
        except Exception as error:return reflected_result(_carrier_rejected("audit_pcg_graph_contract",error,report,raw))
        return reflected_result(domain._named_mcp("audit_pcg_graph_contract",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def read_pcg_node_contract(graph_path:str,node_id:str="\"\"",include_dependencies:str="false")->CubelessPythonOperationResult:
        "Filter the audit result by exact case-insensitive node name, authored title or settings class; preserve available_nodes on missing match."
        rejected,report,raw=_guard_admission("read_pcg_node_contract")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"graph_path":_decode_value(graph_path),"node_id":_decode_value(node_id),"include_dependencies":_decode_value(include_dependencies)}
        except Exception as error:return reflected_result(_carrier_rejected("read_pcg_node_contract",error,report,raw))
        return reflected_result(domain._named_mcp("read_pcg_node_contract",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def set_pcg_static_mesh_spawner_entries(graph_path:str,node_id:str,mesh_entries:str="null",selector_type:str="\"weighted\"",attribute_name:str="\"Meshes\"",material_override_attributes:str="null",save:str="true")->CubelessPythonOperationResult:
        "Preflight and configure weighted or by-attribute PCG Static Mesh Spawner entries; verify independent typed readback before optional save and retain actual partial state."
        rejected,report,raw=_guard_admission("set_pcg_static_mesh_spawner_entries")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"graph_path":_decode_value(graph_path),"node_id":_decode_value(node_id),"mesh_entries":_decode_value(mesh_entries),"selector_type":_decode_value(selector_type),"attribute_name":_decode_value(attribute_name),"material_override_attributes":_decode_value(material_override_attributes),"save":_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected("set_pcg_static_mesh_spawner_entries",error,report,raw))
        return reflected_result(domain._named_mcp("set_pcg_static_mesh_spawner_entries",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessPCGContractAuthoringAttachmentLibrary', None), 'verify_pcg_contract_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact PCGContractAuthoring adapter raw-input admission helper is unavailable')
        raw = helper(CubelessPCGContractAuthoringTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_pcg_contract_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded PCGContractAuthoring adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('pcg_contract_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'pcg_contract_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('pcg_contract_authoring_registration_carrier_invalid',str(error)))
    result['stage']='pcg_contract_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
