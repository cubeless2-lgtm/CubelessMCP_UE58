"""HJ original texture/material contracts through public Editor Python SDK; no business Native leaf."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import texture_material_authoring as domain

@unreal.uclass()
class CubelessTextureMaterialTools(unreal.ToolsetDefinition):
    "StaticMesh UV PNG and texture-based Material/Instance preset authoring."
    @toolset_registry.tool_call
    @staticmethod
    def get_static_mesh_uv_layout(mesh_path:str,uv_channel:str="0",output_path:str="\"\"")->CubelessPythonOperationResult:
        "Export the original LOD0 StaticMesh UV perimeter PNG at fixed1024 with actual file-write and partial read receipts. Public Engine Python getters may load/cache the mesh; no new Native worker or hidden compile/save options."
        rejected,report,raw=_guard_admission("get_static_mesh_uv_layout")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"mesh_path":_decode_value(mesh_path),"uv_channel":_decode_value(uv_channel),"output_path":_decode_value(output_path)}
        except Exception as error:return reflected_result(_carrier_rejected("get_static_mesh_uv_layout",error,report,raw))
        return reflected_result(domain._named_mcp("get_static_mesh_uv_layout",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def create_material_instance_with_texture(texture_asset_path:str,material_name:str,unreal_folder:str,base_material_path:str="\"\"")->CubelessPythonOperationResult:
        "Create the original BaseColor Material or MaterialInstance preset with seven alias attempts, actual typed parameter readback and partial create/compile/save receipts. No automatic retry or atomic rollback."
        rejected,report,raw=_guard_admission("create_material_instance_with_texture")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"texture_asset_path":_decode_value(texture_asset_path),"material_name":_decode_value(material_name),"unreal_folder":_decode_value(unreal_folder),"base_material_path":_decode_value(base_material_path)}
        except Exception as error:return reflected_result(_carrier_rejected("create_material_instance_with_texture",error,report,raw))
        return reflected_result(domain._named_mcp("create_material_instance_with_texture",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def create_material_with_texture_set(base_color_texture_asset_path:str,material_name:str,unreal_folder:str,normal_texture_asset_path:str="\"\"",roughness_texture_asset_path:str="\"\"",ambient_occlusion_texture_asset_path:str="\"\"",metallic_texture_asset_path:str="\"\"",base_material_path:str="\"\"")->CubelessPythonOperationResult:
        "Create the original five-slot PBR Material or MaterialInstance preset, preserving fixed expression positions, output channels, alias loops and branch-specific partial/save behavior."
        rejected,report,raw=_guard_admission("create_material_with_texture_set")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"base_color_texture_asset_path":_decode_value(base_color_texture_asset_path),"material_name":_decode_value(material_name),"unreal_folder":_decode_value(unreal_folder),"normal_texture_asset_path":_decode_value(normal_texture_asset_path),"roughness_texture_asset_path":_decode_value(roughness_texture_asset_path),"ambient_occlusion_texture_asset_path":_decode_value(ambient_occlusion_texture_asset_path),"metallic_texture_asset_path":_decode_value(metallic_texture_asset_path),"base_material_path":_decode_value(base_material_path)}
        except Exception as error:return reflected_result(_carrier_rejected("create_material_with_texture_set",error,report,raw))
        return reflected_result(domain._named_mcp("create_material_with_texture_set",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def import_create_and_apply_texture_set(base_color_image_path:str,output_name:str,unreal_folder:str='"/Game/AI_Generated"',normal_image_path:str='""',roughness_image_path:str='""',ambient_occlusion_image_path:str='""',metallic_image_path:str='""',target:str='""',material_slot:str='0',base_material_path:str='""')->CubelessPythonOperationResult:
        "Import up to five local texture files, reuse the original PBR Material/Instance writer and optionally apply to the original StaticMesh target. Original10 inputs/defaults; actual per-stage import/save/partial/unknown logs and no replay. Named standalone next-tick legacy import scope only; Batch/reentrant writer0 refusal. No image generation or external pipeline."
        rejected,report,raw=_guard_admission('import_create_and_apply_texture_set')
        if rejected is not None:return reflected_result(rejected)
        try:arguments={'base_color_image_path':_decode_value(base_color_image_path),'output_name':_decode_value(output_name),'unreal_folder':_decode_value(unreal_folder),'normal_image_path':_decode_value(normal_image_path),'roughness_image_path':_decode_value(roughness_image_path),'ambient_occlusion_image_path':_decode_value(ambient_occlusion_image_path),'metallic_image_path':_decode_value(metallic_image_path),'target':_decode_value(target),'material_slot':_decode_value(material_slot),'base_material_path':_decode_value(base_material_path)}
        except Exception as error:return reflected_result(_carrier_rejected('import_create_and_apply_texture_set',error,report,raw))
        return reflected_result(domain._named_mcp('import_create_and_apply_texture_set',arguments))



    @toolset_registry.tool_call
    @staticmethod
    def generate_pbr_companion_textures(base_color_image_path:str,output_name:str='""',output_dir:str='""',normal_strength:str='2.0',roughness:str='0.75',roughness_detail:str='0.15',ambient_occlusion_strength:str='0.35',metallic:str='0.0')->CubelessPythonOperationResult:
        "Generate heuristic Normal/Roughness/AO/Metallic PNG files from local BaseColor using the original Pillow RGB/L conversion and NumPy float32 recipe. Original8 inputs/defaults/clamps and N/R/AO/M save order; sequential partial files/errors are retained with no rollback/replay. Named and Batch share the same local generator. No Engine asset import, compile/save, AI request or external pipeline; maps are heuristic rather than authored/scanned PBR."
        rejected,report,raw=_guard_admission('generate_pbr_companion_textures')
        if rejected is not None:return reflected_result(rejected)
        try:arguments={'base_color_image_path':_decode_value(base_color_image_path),'output_name':_decode_value(output_name),'output_dir':_decode_value(output_dir),'normal_strength':_decode_value(normal_strength),'roughness':_decode_value(roughness),'roughness_detail':_decode_value(roughness_detail),'ambient_occlusion_strength':_decode_value(ambient_occlusion_strength),'metallic':_decode_value(metallic)}
        except Exception as error:return reflected_result(_carrier_rejected('generate_pbr_companion_textures',error,report,raw))
        return reflected_result(domain._named_mcp('generate_pbr_companion_textures',arguments))



    @toolset_registry.tool_call
    @staticmethod
    def import_texture_to_unreal(image_path:str,unreal_folder:str,asset_name:str)->CubelessPythonOperationResult:
        'Import one original local Texture2D file through the fixed Native legacy-import leaf. Original3 required inputs, imported-path/class/save readback and actual failure prefix; standalone next-tick Named only. Batch/reentrant import refuses before any Engine work. No new transport, retry or rollback.'
        rejected,report,raw=_guard_admission('import_texture_to_unreal')
        if rejected is not None:return reflected_result(rejected)
        try:arguments={'image_path':_decode_value(image_path),'unreal_folder':_decode_value(unreal_folder),'asset_name':_decode_value(asset_name)}
        except Exception as error:return reflected_result(_carrier_rejected('import_texture_to_unreal',error,report,raw))
        return reflected_result(domain._named_mcp('import_texture_to_unreal',arguments))


    @toolset_registry.tool_call
    @staticmethod
    def import_texture_set_to_unreal(base_color_image_path:str,unreal_folder:str,asset_name:str,normal_image_path:str='""',roughness_image_path:str='""',ambient_occlusion_image_path:str='""',metallic_image_path:str='""')->CubelessPythonOperationResult:
        'Import BaseColor and optional Normal/Roughness/AO/Metallic files in original order with original suffixes. Original7 inputs/defaults, shared RP import-set helper, completed-slot/actual-save/partial/unknown receipts. Standalone next-tick Named only; Batch/reentrant writer0 refusal. No replay or rollback.'
        rejected,report,raw=_guard_admission('import_texture_set_to_unreal')
        if rejected is not None:return reflected_result(rejected)
        try:arguments={'base_color_image_path':_decode_value(base_color_image_path),'unreal_folder':_decode_value(unreal_folder),'asset_name':_decode_value(asset_name),'normal_image_path':_decode_value(normal_image_path),'roughness_image_path':_decode_value(roughness_image_path),'ambient_occlusion_image_path':_decode_value(ambient_occlusion_image_path),'metallic_image_path':_decode_value(metallic_image_path)}
        except Exception as error:return reflected_result(_carrier_rejected('import_texture_set_to_unreal',error,report,raw))
        return reflected_result(domain._named_mcp('import_texture_set_to_unreal',arguments))


    @toolset_registry.tool_call
    @staticmethod
    def apply_material_to_mesh(target:str,material_asset_path:str,material_slot:str='0')->CubelessPythonOperationResult:
        'Apply an existing Material/Instance to selected actors, original label/path/name matches or a StaticMesh asset. Original3 inputs/default slot0; Named and ordinary Batch use the fixed Native synchronous leaf without import-ticker restriction. Actor component override writes do not save the map; the StaticMesh asset branch retains original exact asset save. Partial/unknown prefixes and no replay remain intact.'
        rejected,report,raw=_guard_admission('apply_material_to_mesh')
        if rejected is not None:return reflected_result(rejected)
        try:arguments={'target':_decode_value(target),'material_asset_path':_decode_value(material_asset_path),'material_slot':_decode_value(material_slot)}
        except Exception as error:return reflected_result(_carrier_rejected('apply_material_to_mesh',error,report,raw))
        return reflected_result(domain._named_mcp('apply_material_to_mesh',arguments))




    @toolset_registry.tool_call
    @staticmethod
    def generate_texture_from_prompt(prompt:str,output_name:str,output_dir:str,size:str='"1024x1024"')->CubelessPythonOperationResult:
        'Prepare the original disabled Codex image handoff: sanitize PNG name, create the output directory and return builtin_image_generation_required with expected path/prompt/size. No image or network/backend request is performed. Local mkdir failure/prefix remains visible; no asset compile/save.'
        rejected,report,raw=_guard_admission('generate_texture_from_prompt')
        if rejected is not None:return reflected_result(rejected)
        try:arguments={'prompt':_decode_value(prompt),'output_name':_decode_value(output_name),'output_dir':_decode_value(output_dir),'size':_decode_value(size)}
        except Exception as error:return reflected_result(_carrier_rejected('generate_texture_from_prompt',error,report,raw))
        return reflected_result(domain._named_mcp('generate_texture_from_prompt',arguments))


    @toolset_registry.tool_call
    @staticmethod
    def generate_texture_for_mesh_uv(mesh_path:str,prompt:str,uv_channel:str='0',output_name:str='"T_AI_Texture"',output_dir:str='""',size:str='"1024x1024"')->CubelessPythonOperationResult:
        'Export the original LOD0 UV reference PNG with the shared public UV helper, then return builtin_image_generation_required with the guided prompt and expected output path. Original6 defaults and nonblocking reentrant composite lock; UV read failure precedes mkdir. Handoff failure can follow a real UV PNG write, retained separately from original Unreal-read mutation flags. No image generation/import/compile/save.'
        rejected,report,raw=_guard_admission('generate_texture_for_mesh_uv')
        if rejected is not None:return reflected_result(rejected)
        try:arguments={'mesh_path':_decode_value(mesh_path),'prompt':_decode_value(prompt),'uv_channel':_decode_value(uv_channel),'output_name':_decode_value(output_name),'output_dir':_decode_value(output_dir),'size':_decode_value(size)}
        except Exception as error:return reflected_result(_carrier_rejected('generate_texture_for_mesh_uv',error,report,raw))
        return reflected_result(domain._named_mcp('generate_texture_for_mesh_uv',arguments))


    @toolset_registry.tool_call
    @staticmethod
    def generate_and_apply_ai_texture(target:str,prompt:str,output_name:str,unreal_folder:str='"/Game/AI_Generated"',uv_channel:str='0',size:str='"1024x1024"')->CubelessPythonOperationResult:
        'Resolve the original first selected/named actor first StaticMeshComponent or StaticMesh asset, export its UV PNG, and return the original disabled image handoff/logs/next steps. Original6 inputs/defaults, case-sensitive target matching and asset-before-actor lookup. The disabled branch never imports/creates/applies material; prior local PNG and query/load prefixes survive failure/unknown without replay.'
        rejected,report,raw=_guard_admission('generate_and_apply_ai_texture')
        if rejected is not None:return reflected_result(rejected)
        try:arguments={'target':_decode_value(target),'prompt':_decode_value(prompt),'output_name':_decode_value(output_name),'unreal_folder':_decode_value(unreal_folder),'uv_channel':_decode_value(uv_channel),'size':_decode_value(size)}
        except Exception as error:return reflected_result(_carrier_rejected('generate_and_apply_ai_texture',error,report,raw))
        return reflected_result(domain._named_mcp('generate_and_apply_ai_texture',arguments))


def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessTextureMaterialAttachmentLibrary', None), 'verify_texture_material_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact TextureMaterial adapter raw-input admission helper is unavailable')
        raw = helper(CubelessTextureMaterialTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_texture_material_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded TextureMaterial adapter delegation is required; no Engine operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('texture_material_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'texture_material_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('texture_material_authoring_registration_carrier_invalid',str(error)))
    result['stage']='texture_material_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
