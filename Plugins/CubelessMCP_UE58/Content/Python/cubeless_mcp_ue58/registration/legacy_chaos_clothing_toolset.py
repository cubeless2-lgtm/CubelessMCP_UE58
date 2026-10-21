"""Original logical legacy clothing7 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import legacy_chaos_clothing as operation

@unreal.uclass()
class CubelessLegacyChaosClothingTools(unreal.ToolsetDefinition):
    'Create owned SkeletalMesh legacy Chaos clothing from1..8 sections after complete preflight. Preserve exact material slots, original binary PinGuide/local-Z fallback, optional source or VRoid PhysicsAsset, native factory/config/paint/bind/post-edit/readback, actual SDK save versus vacuous legacy save flags and partial owned cleanup. The source mesh is independently observed. No atomic rollback or uncertain retry; no Cubeless/MCP runtime class dependency. SDK synchronous build/cook/cache/cleanup modal limits remain explicit.'
    @toolset_registry.tool_call
    @staticmethod
    def create_legacy_chaos_clothing_from_skeletal_sections(source_skeletal_mesh_path:str,output_skeletal_mesh_path:str,section_configs:str,dry_run:str='true',save_assets:str='true',use_source_physics_asset:str='true',create_vroid_lower_body_cloth_physics_asset:str='false')->CubelessPythonOperationResult:
        'Create owned SkeletalMesh legacy Chaos clothing from1..8 sections after complete preflight. Preserve exact material slots, original binary PinGuide/local-Z fallback, optional source or VRoid PhysicsAsset, native factory/config/paint/bind/post-edit/readback, actual SDK save versus vacuous legacy save flags and partial owned cleanup. The source mesh is independently observed. No atomic rollback or uncertain retry; no Cubeless/MCP runtime class dependency. SDK synchronous build/cook/cache/cleanup modal limits remain explicit.'
        rejected,report,raw=_guard_admission('create_legacy_chaos_clothing_from_skeletal_sections')
        if rejected is not None:return reflected_result(rejected)
        try:values={'source_skeletal_mesh_path':_decode_value(source_skeletal_mesh_path),'output_skeletal_mesh_path':_decode_value(output_skeletal_mesh_path),'section_configs':_decode_value(section_configs),'dry_run':_decode_value(dry_run),'save_assets':_decode_value(save_assets),'use_source_physics_asset':_decode_value(use_source_physics_asset),'create_vroid_lower_body_cloth_physics_asset':_decode_value(create_vroid_lower_body_cloth_physics_asset)}
        except Exception as error:return reflected_result(_carrier_rejected('create_legacy_chaos_clothing_from_skeletal_sections',error,report,raw))
        return reflected_result(operation._named_mcp('create_legacy_chaos_clothing_from_skeletal_sections',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessLegacyChaosClothingAttachmentLibrary', None), 'verify_legacy_chaos_clothing_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact LegacyChaosClothing adapter raw-input admission helper is unavailable')
        raw = helper(CubelessLegacyChaosClothingTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_legacy_chaos_clothing_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded LegacyChaosClothing adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('legacy_chaos_clothing_raw_guard_admission_missing', message))
    result['stage'] = 'legacy_chaos_clothing_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('legacy_chaos_clothing_registration_carrier_invalid',str(error)))
    result['stage']='legacy_chaos_clothing_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
