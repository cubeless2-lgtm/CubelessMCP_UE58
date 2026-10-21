"""Original authoring10/16 required3/3 through public logical schemas and private JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_cloth_authoring as operation

@unreal.uclass()
class CubelessChaosClothAuthoringTools(unreal.ToolsetDefinition):
    "Guarded original node and weight-map authoring with Native current-token/readback/regeneration/save/semantic-restore authority. Semantic compensation is not atomic rollback; no uncertain replay."
    @toolset_registry.tool_call
    @staticmethod
    def configure_chaos_cloth_dataflow_node(cloth_asset_path:str,node_name:str,property_values:str,expected_node_type:str="\"\"",expected_dataflow_asset_path:str="\"\"",expected_current:str="null",allow_connected_inputs:str="false",regenerate_asset:str="true",save_assets:str="false",dry_run:str="true")->CubelessPythonOperationResult:
        "Preserve original authoring inputs, Native current token and actual partial/compile/regenerate/save/semantic-restore receipts; no atomic rollback or automatic retry."
        rejected,report,raw=_guard_admission('configure_chaos_cloth_dataflow_node')
        if rejected is not None:return reflected_result(rejected)
        try:values={"cloth_asset_path":_decode_value(cloth_asset_path),"node_name":_decode_value(node_name),"property_values":_decode_value(property_values),"expected_node_type":_decode_value(expected_node_type),"expected_dataflow_asset_path":_decode_value(expected_dataflow_asset_path),"expected_current":_decode_value(expected_current),"allow_connected_inputs":_decode_value(allow_connected_inputs),"regenerate_asset":_decode_value(regenerate_asset),"save_assets":_decode_value(save_assets),"dry_run":_decode_value(dry_run)}
        except Exception as error:return reflected_result(_carrier_rejected('configure_chaos_cloth_dataflow_node',error,report,raw))
        return reflected_result(operation._named_mcp('configure_chaos_cloth_dataflow_node',values))

    @toolset_registry.tool_call
    @staticmethod
    def configure_chaos_cloth_weight_map(cloth_asset_path:str,node_name:str,weight_map_name:str,weights:str="null",fill_value:str="null",lod_index:str="0",expected_node_type:str="\"\"",expected_dataflow_asset_path:str="\"\"",expected_vertex_count:str="-1",expected_topology_md5:str="\"\"",expected_weight_map_md5:str="\"\"",expected_node_state_md5:str="\"\"",expected_path_md5:str="\"\"",require_runtime_binding:str="true",save_assets:str="false",dry_run:str="true")->CubelessPythonOperationResult:
        "Preserve original authoring inputs, Native current token and actual partial/compile/regenerate/save/semantic-restore receipts; no atomic rollback or automatic retry."
        rejected,report,raw=_guard_admission('configure_chaos_cloth_weight_map')
        if rejected is not None:return reflected_result(rejected)
        try:values={"cloth_asset_path":_decode_value(cloth_asset_path),"node_name":_decode_value(node_name),"weight_map_name":_decode_value(weight_map_name),"weights":_decode_value(weights),"fill_value":_decode_value(fill_value),"lod_index":_decode_value(lod_index),"expected_node_type":_decode_value(expected_node_type),"expected_dataflow_asset_path":_decode_value(expected_dataflow_asset_path),"expected_vertex_count":_decode_value(expected_vertex_count),"expected_topology_md5":_decode_value(expected_topology_md5),"expected_weight_map_md5":_decode_value(expected_weight_map_md5),"expected_node_state_md5":_decode_value(expected_node_state_md5),"expected_path_md5":_decode_value(expected_path_md5),"require_runtime_binding":_decode_value(require_runtime_binding),"save_assets":_decode_value(save_assets),"dry_run":_decode_value(dry_run)}
        except Exception as error:return reflected_result(_carrier_rejected('configure_chaos_cloth_weight_map',error,report,raw))
        return reflected_result(operation._named_mcp('configure_chaos_cloth_weight_map',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosClothAuthoringAttachmentLibrary', None), 'verify_chaos_cloth_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosClothAuthoring adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosClothAuthoringTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_cloth_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosClothAuthoring adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('chaos_cloth_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_cloth_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>67108864 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('chaos_cloth_authoring_registration_carrier_invalid',str(error)))
    result['stage']='chaos_cloth_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
