"""Original logical Chaos Cloth inspection5/7 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_cloth_inspection as operation

@unreal.uclass()
class CubelessChaosClothInspectionTools(unreal.ToolsetDefinition):
    'Inspect original Chaos Cloth collections/properties/Dataflow ownership and compiled simulation mesh pages, full topology/weight hashes, consumer and node input criteria. Preserve exact raw/Named input and partial load/evaluation receipts. Explicit compile/save0 is not complete SDK/cache purity; snapshot current-state support is reported by Native.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_cloth_asset(cloth_asset_path:str,lod_index:str='-1',property_name_filter:str='""',include_property_values:str='true',include_property_flags:str='true',include_dataflow_nodes:str='true',max_properties:str='512',max_nodes:str='256')->CubelessPythonOperationResult:
        'Inspect bounded native Chaos Cloth collection/property flags/values and Dataflow ownership/editable-node criteria. Budgets and current snapshot support follow the Native receipt; no explicit write, compile or save.'
        rejected,report,raw=_guard_admission('inspect_chaos_cloth_asset')
        if rejected is not None:return reflected_result(rejected)
        try:values={'cloth_asset_path':_decode_value(cloth_asset_path),'lod_index':_decode_value(lod_index),'property_name_filter':_decode_value(property_name_filter),'include_property_values':_decode_value(include_property_values),'include_property_flags':_decode_value(include_property_flags),'include_dataflow_nodes':_decode_value(include_dataflow_nodes),'max_properties':_decode_value(max_properties),'max_nodes':_decode_value(max_nodes)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_chaos_cloth_asset',error,report,raw))
        return reflected_result(operation._named_mcp('inspect_chaos_cloth_asset',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_cloth_simulation_mesh(cloth_asset_path:str,lod_index:str='0',weight_map_name:str='""',vertex_start:str='0',max_vertices:str='4096',face_start:str='0',max_faces:str='4096',include_positions:str='true',include_normals:str='false',include_indices:str='true',include_weight_values:str='true')->CubelessPythonOperationResult:
        'Inspect compiled cloth simulation mesh vertex/face pages, full topology/weight hashes, selected weights and Dataflow input topology. Partial failures and SDK effects remain visible; no automatic replay or unverified current-state equivalence.'
        rejected,report,raw=_guard_admission('inspect_chaos_cloth_simulation_mesh')
        if rejected is not None:return reflected_result(rejected)
        try:values={'cloth_asset_path':_decode_value(cloth_asset_path),'lod_index':_decode_value(lod_index),'weight_map_name':_decode_value(weight_map_name),'vertex_start':_decode_value(vertex_start),'max_vertices':_decode_value(max_vertices),'face_start':_decode_value(face_start),'max_faces':_decode_value(max_faces),'include_positions':_decode_value(include_positions),'include_normals':_decode_value(include_normals),'include_indices':_decode_value(include_indices),'include_weight_values':_decode_value(include_weight_values)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_chaos_cloth_simulation_mesh',error,report,raw))
        return reflected_result(operation._named_mcp('inspect_chaos_cloth_simulation_mesh',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosClothInspectionAttachmentLibrary', None), 'verify_chaos_cloth_inspection_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosClothInspection adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosClothInspectionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_cloth_inspection_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosClothInspection adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('chaos_cloth_inspection_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_cloth_inspection_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('chaos_cloth_inspection_registration_carrier_invalid',str(error)))
    result['stage']='chaos_cloth_inspection_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
