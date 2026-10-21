"""Original logical Chaos Cloth component9/13 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_cloth_component as operation

@unreal.uclass()
class CubelessChaosClothComponentTools(unreal.ToolsetDefinition):
    "Inspect and configure original Cloth component9/13 with distinct raw/Named presence, runtime and settings preflight, semantic restore and actual save receipts. Runtime exact-key writes require verified independently cloned native ChaosClothComponent/ClothAsset collections; unsupported ownership and recreation variants remain explicit incomplete scope."
    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_cloth_component(actor_name:str="\"\"",actor_label:str="\"\"",component_name:str="\"\"",model_index:str="0",lod_index:str="-1",world_context:str="\"auto\"",include_runtime_properties:str="true",max_properties:str="512",max_results:str="16")->CubelessPythonOperationResult:
        "Inspect original bounded components and configured/runtime snapshots; explicit writer/compile/save0 does not imply SDK/cache purity or simulation completion."
        rejected,report,raw=_guard_admission("inspect_chaos_cloth_component")
        if rejected is not None:return reflected_result(rejected)
        try:values={"actor_name":_decode_value(actor_name),"actor_label":_decode_value(actor_label),"component_name":_decode_value(component_name),"model_index":_decode_value(model_index),"lod_index":_decode_value(lod_index),"world_context":_decode_value(world_context),"include_runtime_properties":_decode_value(include_runtime_properties),"max_properties":_decode_value(max_properties),"max_results":_decode_value(max_results)}
        except Exception as error:return reflected_result(_carrier_rejected("inspect_chaos_cloth_component",error,report,raw))
        return reflected_result(operation._named_mcp("inspect_chaos_cloth_component",values))

    @toolset_registry.tool_call
    @staticmethod
    def configure_chaos_cloth_component(actor_name:str="\"\"",property_updates:str="null",component_settings:str="null",component_name:str="\"\"",actor_label:str="\"\"",model_index:str="0",lod_index:str="-1",world_context:str="\"auto\"",expected_current:str="null",allow_intrinsic_recreate:str="false",dry_run:str="true",recreate_simulation:str="\"auto\"",save_level:str="false")->CubelessPythonOperationResult:
        "Plan or apply guarded exact-key native runtime values and serialized component/teleport/proxy settings with Native readback/save/semantic-restore prefixes. Runtime values are transient and require verified independently cloned native ClothAsset collections; unsupported ownership/recreation variants refuse before writer. No automatic retry, atomic rollback or effective simulation completion is claimed."
        rejected,report,raw=_guard_admission("configure_chaos_cloth_component")
        if rejected is not None:return reflected_result(rejected)
        try:values={"actor_name":_decode_value(actor_name),"property_updates":_decode_value(property_updates),"component_settings":_decode_value(component_settings),"component_name":_decode_value(component_name),"actor_label":_decode_value(actor_label),"model_index":_decode_value(model_index),"lod_index":_decode_value(lod_index),"world_context":_decode_value(world_context),"expected_current":_decode_value(expected_current),"allow_intrinsic_recreate":_decode_value(allow_intrinsic_recreate),"dry_run":_decode_value(dry_run),"recreate_simulation":_decode_value(recreate_simulation),"save_level":_decode_value(save_level)}
        except Exception as error:return reflected_result(_carrier_rejected("configure_chaos_cloth_component",error,report,raw))
        return reflected_result(operation._named_mcp("configure_chaos_cloth_component",values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosClothComponentAttachmentLibrary', None), 'verify_chaos_cloth_component_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosClothComponent adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosClothComponentTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_cloth_component_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosClothComponent adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('chaos_cloth_component_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_cloth_component_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('chaos_cloth_component_registration_carrier_invalid',str(error)))
    result['stage']='chaos_cloth_component_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
