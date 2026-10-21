"""GU original public25 through bounded per-field JSON carriers; compound stages and partial receipts are Native owned."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import niagara_sprite_preset_authoring as domain
from cubeless_mcp_ue58.operations import niagara_sprite_preset_async as preset_async
from .async_interop_toolset import CubelessPythonAsyncOperationResult

@unreal.uclass()
class CubelessNiagaraSpritePresetTools(unreal.ToolsetDefinition):
    "One original sprite preset compound API. Default wait intent, factory and module stages, actual compile/save/partial results remain Native authoritative."
    @toolset_registry.tool_call
    @staticmethod
    def create_sprite_niagara_preset_system(target_system_path:str,preset:str="\"basic\"",emitter_name:str="\"Emitter\"",material_path:str="\"\"",spawn_rate:str="null",velocity:str="null",alignment:str="\"\"",facing_mode:str="\"\"",sub_image_size:str="null",fixed_bounds_min:str="null",fixed_bounds_max:str="null",local_space:str="null",sprite_size:str="null",lifetime:str="null",use_project_weather_materials:str="false",add_spawn_box_location:str="null",spawn_box_size:str="null",spawn_box_offset:str="null",add_weather_detail_modules:str="null",allow_non_temp_path:str="false",overwrite_existing:str="false",save:str="true",wait_for_completion:str="true",timeout_seconds:str="10.0",poll_interval_seconds:str="0.1")->CubelessPythonAsyncOperationResult:
        "\n        Create a saved Niagara System from a conservative factory sprite preset.\n\n        Args:\n            target_system_path: Target Niagara System package/object path, default-safe under /Game/_MCP_Temp/NiagaraGenerated\n            preset: One of basic, rain, snow, dust, or sparks\n            emitter_name: Name for the generated emitter handle/source copy\n            material_path: Optional material to apply to the generated sprite renderer\n            spawn_rate: Optional override for the preset SpawnRate module input\n            velocity: Optional [x, y, z] override for the preset AddVelocity module input\n            alignment: Optional sprite alignment override\n            facing_mode: Optional sprite facing mode override\n            sub_image_size: Optional [x, y] SubUV tile count override\n            fixed_bounds_min: Optional fixed bounds minimum [x, y, z]\n            fixed_bounds_max: Optional fixed bounds maximum [x, y, z]\n            local_space: Optional local-space setting for the generated emitter\n            sprite_size: Optional InitializeParticle sprite size [x, y]\n            lifetime: Optional InitializeParticle particle lifetime in seconds\n            use_project_weather_materials: Use project-owned /Game/MCPWeather rain/snow particle materials when material_path is empty\n            add_spawn_box_location: Enable/disable BoxLocation insertion; rain/snow enable it by default\n            spawn_box_size: Optional BoxLocation size [x, y, z]\n            spawn_box_offset: Optional BoxLocation offset [x, y, z]\n            add_weather_detail_modules: Enable/disable rain/snow force/noise/color/size detail modules\n            allow_non_temp_path: Explicitly allow creation outside the generated temp Niagara root\n            overwrite_existing: Reserved; currently refused by the C++ command\n            save: Save the created package\n            wait_for_completion: Wait for Niagara compilation after preset edits\n            timeout_seconds: Compile wait timeout\n            poll_interval_seconds: Compile wait poll interval\n        "
        rejected,report,raw=_guard_admission("create_sprite_niagara_preset_system")
        if rejected is not None:return preset_async.completed_result(rejected)
        try:arguments={"target_system_path":_decode_value(target_system_path),"preset":_decode_value(preset),"emitter_name":_decode_value(emitter_name),"material_path":_decode_value(material_path),"spawn_rate":_decode_value(spawn_rate),"velocity":_decode_value(velocity),"alignment":_decode_value(alignment),"facing_mode":_decode_value(facing_mode),"sub_image_size":_decode_value(sub_image_size),"fixed_bounds_min":_decode_value(fixed_bounds_min),"fixed_bounds_max":_decode_value(fixed_bounds_max),"local_space":_decode_value(local_space),"sprite_size":_decode_value(sprite_size),"lifetime":_decode_value(lifetime),"use_project_weather_materials":_decode_value(use_project_weather_materials),"add_spawn_box_location":_decode_value(add_spawn_box_location),"spawn_box_size":_decode_value(spawn_box_size),"spawn_box_offset":_decode_value(spawn_box_offset),"add_weather_detail_modules":_decode_value(add_weather_detail_modules),"allow_non_temp_path":_decode_value(allow_non_temp_path),"overwrite_existing":_decode_value(overwrite_existing),"save":_decode_value(save),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return preset_async.completed_result(_carrier_rejected("create_sprite_niagara_preset_system",error,report,raw))
        return preset_async.begin(arguments)


def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraSpritePresetAttachmentLibrary', None), 'verify_niagara_sprite_preset_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraSpritePreset adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraSpritePresetTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_sprite_preset_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraSpritePreset adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_sprite_preset_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_sprite_preset_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_sprite_preset_authoring_registration_carrier_invalid',str(error)))
    result['stage']='niagara_sprite_preset_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
