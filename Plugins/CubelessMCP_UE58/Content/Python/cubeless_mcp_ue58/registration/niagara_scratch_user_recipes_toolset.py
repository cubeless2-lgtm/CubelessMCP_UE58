"""HN exact four original Scratch/User recipe carriers; Named typed async final."""
import json
import unreal
import toolset_registry
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_scratch_user_recipes as domain

@unreal.uclass()
class CubelessNiagaraScratchUserRecipeTools(unreal.ToolsetDefinition):
    "Original Scratch/User module recipes with finite leaf wait continuations."
    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_scratch_pad_vec2_user_parameter_module(target_system_path:str,scratch_pad_name:str="\"MCP_BlankUpdateScratch\"",parameter_name:str="\"Particles.SpriteSize\"",parameter_value:str="null",user_parameter_name:str="\"User.MCP_BlankScratchPad_SpriteSize\"",user_parameter_default_value:str="null",target_usage:str="\"ParticleUpdateScript\"",target_owner_kind:str="\"system\"",target_emitter_index:str="null",target_emitter_name:str="\"\"",target_stack_emitter_index:str="0",target_stack_emitter_name:str="\"\"",stack_suggested_name:str="\"\"",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="true",timeout_seconds:str="180")->CubelessPythonAsyncOperationResult:
        "Original create_niagara_scratch_pad_vec2_user_parameter_module ordered workflow. Preserve default compile/waitTrue, actual partial/saved history and same-parent final delivery; no automatic retry or rollback."
        rejected,report,raw=_guard_admission("create_niagara_scratch_pad_vec2_user_parameter_module")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"target_system_path":_decode_value(target_system_path),"scratch_pad_name":_decode_value(scratch_pad_name),"parameter_name":_decode_value(parameter_name),"parameter_value":_decode_value(parameter_value),"user_parameter_name":_decode_value(user_parameter_name),"user_parameter_default_value":_decode_value(user_parameter_default_value),"target_usage":_decode_value(target_usage),"target_owner_kind":_decode_value(target_owner_kind),"target_emitter_index":_decode_value(target_emitter_index),"target_emitter_name":_decode_value(target_emitter_name),"target_stack_emitter_index":_decode_value(target_stack_emitter_index),"target_stack_emitter_name":_decode_value(target_stack_emitter_name),"stack_suggested_name":_decode_value(stack_suggested_name),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("create_niagara_scratch_pad_vec2_user_parameter_module",error,report,raw))
        return domain._named_mcp("create_niagara_scratch_pad_vec2_user_parameter_module",arguments)

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_scratch_pad_color_user_parameter_module(target_system_path:str,scratch_pad_name:str="\"MCP_ColorScratch\"",parameter_name:str="\"Particles.Color\"",parameter_value:str="null",user_parameter_name:str="\"User.MCP_ScratchPad_Color\"",user_parameter_default_value:str="null",target_usage:str="\"ParticleUpdateScript\"",target_owner_kind:str="\"system\"",target_emitter_index:str="null",target_emitter_name:str="\"\"",target_stack_emitter_index:str="0",target_stack_emitter_name:str="\"\"",stack_suggested_name:str="\"\"",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="true",timeout_seconds:str="180")->CubelessPythonAsyncOperationResult:
        "Original create_niagara_scratch_pad_color_user_parameter_module ordered workflow. Preserve default compile/waitTrue, actual partial/saved history and same-parent final delivery; no automatic retry or rollback."
        rejected,report,raw=_guard_admission("create_niagara_scratch_pad_color_user_parameter_module")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"target_system_path":_decode_value(target_system_path),"scratch_pad_name":_decode_value(scratch_pad_name),"parameter_name":_decode_value(parameter_name),"parameter_value":_decode_value(parameter_value),"user_parameter_name":_decode_value(user_parameter_name),"user_parameter_default_value":_decode_value(user_parameter_default_value),"target_usage":_decode_value(target_usage),"target_owner_kind":_decode_value(target_owner_kind),"target_emitter_index":_decode_value(target_emitter_index),"target_emitter_name":_decode_value(target_emitter_name),"target_stack_emitter_index":_decode_value(target_stack_emitter_index),"target_stack_emitter_name":_decode_value(target_stack_emitter_name),"stack_suggested_name":_decode_value(stack_suggested_name),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("create_niagara_scratch_pad_color_user_parameter_module",error,report,raw))
        return domain._named_mcp("create_niagara_scratch_pad_color_user_parameter_module",arguments)

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_scratch_pad_bool_user_parameter_module(target_system_path:str,scratch_pad_name:str="\"MCP_BoolUserScratch\"",parameter_name:str="\"Particles.MCPBoolProbe\"",parameter_value:str="null",module_input_name:str="\"MCPBoolInput\"",module_input_default_value:str="null",user_parameter_name:str="\"User.MCP_ScratchPad_Bool\"",user_parameter_default_value:str="null",target_usage:str="\"ParticleUpdateScript\"",target_owner_kind:str="\"system\"",target_emitter_index:str="null",target_emitter_name:str="\"\"",target_stack_emitter_index:str="0",target_stack_emitter_name:str="\"\"",stack_suggested_name:str="\"\"",overwrite_existing:str="false",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="true",timeout_seconds:str="180")->CubelessPythonAsyncOperationResult:
        "Original create_niagara_scratch_pad_bool_user_parameter_module ordered workflow. Preserve default compile/waitTrue, actual partial/saved history and same-parent final delivery; no automatic retry or rollback."
        rejected,report,raw=_guard_admission("create_niagara_scratch_pad_bool_user_parameter_module")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"target_system_path":_decode_value(target_system_path),"scratch_pad_name":_decode_value(scratch_pad_name),"parameter_name":_decode_value(parameter_name),"parameter_value":_decode_value(parameter_value),"module_input_name":_decode_value(module_input_name),"module_input_default_value":_decode_value(module_input_default_value),"user_parameter_name":_decode_value(user_parameter_name),"user_parameter_default_value":_decode_value(user_parameter_default_value),"target_usage":_decode_value(target_usage),"target_owner_kind":_decode_value(target_owner_kind),"target_emitter_index":_decode_value(target_emitter_index),"target_emitter_name":_decode_value(target_emitter_name),"target_stack_emitter_index":_decode_value(target_stack_emitter_index),"target_stack_emitter_name":_decode_value(target_stack_emitter_name),"stack_suggested_name":_decode_value(stack_suggested_name),"overwrite_existing":_decode_value(overwrite_existing),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("create_niagara_scratch_pad_bool_user_parameter_module",error,report,raw))
        return domain._named_mcp("create_niagara_scratch_pad_bool_user_parameter_module",arguments)

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_scratch_pad_bool_visible_toggle_module(target_system_path:str,scratch_pad_name:str="\"MCP_BoolVisibleScratch\"",parameter_name:str="\"Particles.SpriteSize\"",parameter_value:str="null",base_module_input_name:str="\"MCPBaseSpriteSize\"",base_module_input_default_value:str="null",user_parameter_name:str="\"User.MCP_VisibleToggle\"",user_parameter_default_value:str="null",custom_hlsl_input_name:str="\"MCPVisibleToggle\"",custom_hlsl_expression:str="\"BaseValue * (MCPVisibleToggle ? 1.0 : 0.0)\"",custom_hlsl_output_pin_name:str="\"MCPVisibleSize\"",custom_hlsl_signature_name:str="\"MCP_ScratchPadBoolVisibleSize\"",target_usage:str="\"ParticleUpdateScript\"",target_owner_kind:str="\"system\"",target_emitter_index:str="null",target_emitter_name:str="\"\"",target_stack_emitter_index:str="0",target_stack_emitter_name:str="\"\"",stack_suggested_name:str="\"\"",overwrite_existing:str="false",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="true",timeout_seconds:str="180")->CubelessPythonAsyncOperationResult:
        "Original create_niagara_scratch_pad_bool_visible_toggle_module ordered workflow. Preserve default compile/waitTrue, actual partial/saved history and same-parent final delivery; no automatic retry or rollback."
        rejected,report,raw=_guard_admission("create_niagara_scratch_pad_bool_visible_toggle_module")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"target_system_path":_decode_value(target_system_path),"scratch_pad_name":_decode_value(scratch_pad_name),"parameter_name":_decode_value(parameter_name),"parameter_value":_decode_value(parameter_value),"base_module_input_name":_decode_value(base_module_input_name),"base_module_input_default_value":_decode_value(base_module_input_default_value),"user_parameter_name":_decode_value(user_parameter_name),"user_parameter_default_value":_decode_value(user_parameter_default_value),"custom_hlsl_input_name":_decode_value(custom_hlsl_input_name),"custom_hlsl_expression":_decode_value(custom_hlsl_expression),"custom_hlsl_output_pin_name":_decode_value(custom_hlsl_output_pin_name),"custom_hlsl_signature_name":_decode_value(custom_hlsl_signature_name),"target_usage":_decode_value(target_usage),"target_owner_kind":_decode_value(target_owner_kind),"target_emitter_index":_decode_value(target_emitter_index),"target_emitter_name":_decode_value(target_emitter_name),"target_stack_emitter_index":_decode_value(target_stack_emitter_index),"target_stack_emitter_name":_decode_value(target_stack_emitter_name),"stack_suggested_name":_decode_value(stack_suggested_name),"overwrite_existing":_decode_value(overwrite_existing),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("create_niagara_scratch_pad_bool_visible_toggle_module",error,report,raw))
        return domain._named_mcp("create_niagara_scratch_pad_bool_visible_toggle_module",arguments)

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_visible_scratch_pad_snow_seed(target_system_path:str,preset:str='"snow"',emitter_name:str='"Emitter"',material_path:str='""',use_project_weather_materials:str='true',scratch_pad_name:str='"MCP_VisibleSizeScratch"',parameter_name:str='"Particles.SpriteSize"',parameter_value:str='null',user_parameter_name:str='"User.MCP_VisibleScratchPad_SpriteSize"',user_parameter_default_value:str='null',target_usage:str='"ParticleUpdateScript"',spawn_rate:str='180.0',velocity:str='null',sprite_size:str='null',lifetime:str='10.0',add_spawn_box_location:str='true',spawn_box_size:str='null',spawn_box_offset:str='null',add_weather_detail_modules:str='true',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='true',timeout_seconds:str='180')->CubelessPythonAsyncOperationResult:
        "Original24 visible Snow Seed recipe: initial preset save/waitTrue, then Vec2 User Scratch Pad. Actual child receipts, partial save/compile and unknown stop share one parent; no retry or rollback."
        rejected,report,raw=_guard_admission("create_niagara_visible_scratch_pad_snow_seed")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={'target_system_path':_decode_value(target_system_path),'preset':_decode_value(preset),'emitter_name':_decode_value(emitter_name),'material_path':_decode_value(material_path),'use_project_weather_materials':_decode_value(use_project_weather_materials),'scratch_pad_name':_decode_value(scratch_pad_name),'parameter_name':_decode_value(parameter_name),'parameter_value':_decode_value(parameter_value),'user_parameter_name':_decode_value(user_parameter_name),'user_parameter_default_value':_decode_value(user_parameter_default_value),'target_usage':_decode_value(target_usage),'spawn_rate':_decode_value(spawn_rate),'velocity':_decode_value(velocity),'sprite_size':_decode_value(sprite_size),'lifetime':_decode_value(lifetime),'add_spawn_box_location':_decode_value(add_spawn_box_location),'spawn_box_size':_decode_value(spawn_box_size),'spawn_box_offset':_decode_value(spawn_box_offset),'add_weather_detail_modules':_decode_value(add_weather_detail_modules),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("create_niagara_visible_scratch_pad_snow_seed",error,report,raw))
        return domain._named_mcp("create_niagara_visible_scratch_pad_snow_seed",arguments)


def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraScratchUserRecipeAttachmentLibrary', None), 'verify_niagara_scratch_user_recipes_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraScratchUserRecipe adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraScratchUserRecipeTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_scratch_user_recipes_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraScratchUserRecipe adapter delegation is required; no Engine operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_scratch_user_recipes_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_scratch_user_recipes_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_scratch_user_recipes_registration_carrier_invalid',str(error)))
    result['stage']='niagara_scratch_user_recipes_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
