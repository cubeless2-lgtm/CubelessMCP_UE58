"""Original Niagara resource DI/scratch authoring4 through per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_resource_bindings as domain

@unreal.uclass()
class CubelessNiagaraResourceBindingTools(unreal.ToolsetDefinition):
    "Four original resource DI/scratch contracts. Named resource2 return the same Native Job final after DI/User binding, compile, optional save and independent bounded wait. Direct/raw genuine wait stays prewriter refused; Scratch2 retain their synchronous contracts. Native owns target/pin/store and actual partial prefix. No synthetic readiness or replay."
    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_render_target2d_module_input(system_path:str,input_name:str,user_parameter_name:str="\"User.RT_IF_Deform\"",render_target_asset_path:str="\"\"",emitter_index:str="null",emitter_name:str="\"\"",module_index:str="null",module_name:str="\"\"",module_node_guid:str="\"\"",inherit_user_parameter_settings:str="true",overwrite_existing:str="false",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="false",timeout_seconds:str="10.0",poll_interval_seconds:str="0.1")->CubelessPythonAsyncOperationResult:
        "\n        Bind a Niagara RenderTarget2D data-interface module input to a User render target parameter.\n\n        Args:\n            system_path: Niagara System package path or object path\n            input_name: RenderTarget2D module input name, with or without the Module. prefix\n            user_parameter_name: User render target parameter, default User.RT_IF_Deform\n            render_target_asset_path: Optional TextureRenderTarget asset to store as the User parameter default\n            emitter_index: Optional target emitter index\n            emitter_name: Optional target emitter name\n            module_index: Optional target module index from inspect_niagara_module_inputs\n            module_name: Optional target module name\n            module_node_guid: Optional target module node GUID\n            inherit_user_parameter_settings: Use size/format/settings from the User render target asset\n            overwrite_existing: Replace an existing linked override for the module input\n            allow_source_edit: Allow edits outside /Game/_MCP_Temp\n            save: Save the Niagara System after editing\n            request_compile: Request Niagara compile after editing\n            wait_for_completion: Wait for compile completion\n            timeout_seconds: Compile wait timeout\n            poll_interval_seconds: Compile wait poll interval\n         Native actual prefix is authoritative; source registration does not complete the original pipeline."
        rejected,report,raw=_guard_admission("set_niagara_render_target2d_module_input")
        if rejected is not None:return domain.completed_result(rejected)
        try:values={"system_path":_decode_value(system_path),"input_name":_decode_value(input_name),"user_parameter_name":_decode_value(user_parameter_name),"render_target_asset_path":_decode_value(render_target_asset_path),"emitter_index":_decode_value(emitter_index),"emitter_name":_decode_value(emitter_name),"module_index":_decode_value(module_index),"module_name":_decode_value(module_name),"module_node_guid":_decode_value(module_node_guid),"inherit_user_parameter_settings":_decode_value(inherit_user_parameter_settings),"overwrite_existing":_decode_value(overwrite_existing),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("set_niagara_render_target2d_module_input",error,report,raw))
        return domain._named_mcp("set_niagara_render_target2d_module_input",values)

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_texture2d_module_input(system_path:str,input_name:str,user_parameter_name:str="\"User.T_Mask\"",texture_asset_path:str="\"\"",emitter_index:str="null",emitter_name:str="\"\"",module_index:str="null",module_name:str="\"\"",module_node_guid:str="\"\"",overwrite_existing:str="false",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="false",timeout_seconds:str="10.0",poll_interval_seconds:str="0.1")->CubelessPythonAsyncOperationResult:
        "\n        Bind a Niagara Texture2D data-interface module input to a User texture parameter.\n\n        Args:\n            system_path: Niagara System package path or object path\n            input_name: Texture2D module input name, with or without the Module. prefix\n            user_parameter_name: User texture parameter, default User.T_Mask\n            texture_asset_path: Optional Texture2D asset to store as the User parameter default\n            emitter_index: Optional target emitter index\n            emitter_name: Optional target emitter name\n            module_index: Optional target module index from inspect_niagara_module_inputs\n            module_name: Optional target module name\n            module_node_guid: Optional target module node GUID\n            overwrite_existing: Replace an existing linked override for the module input\n            allow_source_edit: Allow edits outside /Game/_MCP_Temp\n            save: Save the Niagara System after editing\n            request_compile: Request Niagara compile after editing\n            wait_for_completion: Wait for compile completion\n            timeout_seconds: Compile wait timeout\n            poll_interval_seconds: Compile wait poll interval\n         Native actual prefix is authoritative; source registration does not complete the original pipeline."
        rejected,report,raw=_guard_admission("set_niagara_texture2d_module_input")
        if rejected is not None:return domain.completed_result(rejected)
        try:values={"system_path":_decode_value(system_path),"input_name":_decode_value(input_name),"user_parameter_name":_decode_value(user_parameter_name),"texture_asset_path":_decode_value(texture_asset_path),"emitter_index":_decode_value(emitter_index),"emitter_name":_decode_value(emitter_name),"module_index":_decode_value(module_index),"module_name":_decode_value(module_name),"module_node_guid":_decode_value(module_node_guid),"overwrite_existing":_decode_value(overwrite_existing),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("set_niagara_texture2d_module_input",error,report,raw))
        return domain._named_mcp("set_niagara_texture2d_module_input",values)

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_scratch_pad_function_input_default(system_path:str,function_name:str="\"\"",input_pin_name:str="\"\"",value:str="null",default_value:str="\"\"",function_node_guid:str="\"\"",function_node_index:str="null",function_call_index:str="null",scratch_pad_script_path:str="\"\"",scratch_pad_owner_kind:str="\"system\"",scratch_pad_script_index:str="null",scratch_pad_name:str="\"\"",scratch_pad_emitter_index:str="null",scratch_pad_emitter_name:str="\"\"",break_links:str="true",allow_multi_link_break:str="false",allow_source_edit:str="false",save:str="true",request_compile:str="true")->CubelessPythonOperationResult:
        "\n        Set a Scratch Pad internal function-call input pin default value.\n\n        Args:\n            system_path: Niagara System package path or object path\n            function_name: Target internal function name, for example SetRenderTargetValue\n            input_pin_name: Target function input pin name, for example Value\n            value: JSON value converted to a Niagara pin default string\n            default_value: Raw Unreal pin default string; takes precedence over value\n            function_node_guid: Optional target function node GUID\n            function_node_index: Optional graph node index from inspect_niagara_graph\n            function_call_index: Optional function-call-only index inside the Scratch Pad graph\n            scratch_pad_script_path: Direct target Scratch Pad script path; required unless name or index is provided\n            scratch_pad_owner_kind: system or emitter\n            scratch_pad_script_index: Scratch Pad script index; required unless path or name is provided\n            scratch_pad_name: Scratch Pad script name; required unless path or index is provided\n            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads\n            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads\n            break_links: Break existing links from the target input pin after setting its default\n            allow_multi_link_break: Allow breaking more than one existing link\n            allow_source_edit: Allow edits outside /Game/_MCP_Temp\n            save: Save the Niagara System after editing\n            request_compile: Request Niagara compile after editing\n         Native actual prefix is authoritative; source registration does not complete the original pipeline."
        rejected,report,raw=_guard_admission("set_niagara_scratch_pad_function_input_default")
        if rejected is not None:return reflected_result(rejected)
        try:values={"system_path":_decode_value(system_path),"function_name":_decode_value(function_name),"input_pin_name":_decode_value(input_pin_name),"value":_decode_value(value),"default_value":_decode_value(default_value),"function_node_guid":_decode_value(function_node_guid),"function_node_index":_decode_value(function_node_index),"function_call_index":_decode_value(function_call_index),"scratch_pad_script_path":_decode_value(scratch_pad_script_path),"scratch_pad_owner_kind":_decode_value(scratch_pad_owner_kind),"scratch_pad_script_index":_decode_value(scratch_pad_script_index),"scratch_pad_name":_decode_value(scratch_pad_name),"scratch_pad_emitter_index":_decode_value(scratch_pad_emitter_index),"scratch_pad_emitter_name":_decode_value(scratch_pad_emitter_name),"break_links":_decode_value(break_links),"allow_multi_link_break":_decode_value(allow_multi_link_break),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected("set_niagara_scratch_pad_function_input_default",error,report,raw))
        return reflected_result(domain._named_mcp("set_niagara_scratch_pad_function_input_default",values))

    @toolset_registry.tool_call
    @staticmethod
    def link_niagara_scratch_pad_pin_to_user_parameter(system_path:str,target_pin_name:str,user_parameter_name:str,scratch_pad_script_path:str="\"\"",scratch_pad_owner_kind:str="\"system\"",scratch_pad_script_index:str="null",scratch_pad_name:str="\"\"",scratch_pad_emitter_index:str="null",scratch_pad_emitter_name:str="\"\"",target_node_guid:str="\"\"",target_node_index:str="null",parameter_map_set_index:str="null",default_value:str="null",overwrite_existing:str="false",allow_multi_link_break:str="false",allow_source_edit:str="false",save:str="true",request_compile:str="true")->CubelessPythonOperationResult:
        "\n        Link a Scratch Pad ParameterMapSet input pin to a matching User parameter.\n\n        Args:\n            system_path: Niagara System package path or object path\n            target_pin_name: Target ParameterMapSet input pin, e.g. Local.Module.AdvectionAmount\n            user_parameter_name: User parameter name, with or without User. prefix\n            scratch_pad_script_path: Direct target Scratch Pad script path; required unless name or index is provided\n            scratch_pad_owner_kind: system or emitter\n            scratch_pad_script_index: Scratch Pad script index; required unless path or name is provided\n            scratch_pad_name: Scratch Pad script name; required unless path or index is provided\n            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads\n            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads\n            target_node_guid: Optional target ParameterMapSet node GUID\n            target_node_index: Optional graph node index from inspect_niagara_graph\n            parameter_map_set_index: Optional ParameterMapSet-only index in the Scratch Pad graph\n            default_value: Optional User parameter default value such as [0, 0] or [1, 1, 1, 1]\n            overwrite_existing: Replace existing links on the target pin\n            allow_multi_link_break: Allow breaking more than one existing link\n            allow_source_edit: Allow edits outside /Game/_MCP_Temp\n            save: Save the Niagara System after editing\n            request_compile: Request Niagara compile after editing\n         Native actual prefix is authoritative; source registration does not complete the original pipeline."
        rejected,report,raw=_guard_admission("link_niagara_scratch_pad_pin_to_user_parameter")
        if rejected is not None:return reflected_result(rejected)
        try:values={"system_path":_decode_value(system_path),"target_pin_name":_decode_value(target_pin_name),"user_parameter_name":_decode_value(user_parameter_name),"scratch_pad_script_path":_decode_value(scratch_pad_script_path),"scratch_pad_owner_kind":_decode_value(scratch_pad_owner_kind),"scratch_pad_script_index":_decode_value(scratch_pad_script_index),"scratch_pad_name":_decode_value(scratch_pad_name),"scratch_pad_emitter_index":_decode_value(scratch_pad_emitter_index),"scratch_pad_emitter_name":_decode_value(scratch_pad_emitter_name),"target_node_guid":_decode_value(target_node_guid),"target_node_index":_decode_value(target_node_index),"parameter_map_set_index":_decode_value(parameter_map_set_index),"default_value":_decode_value(default_value),"overwrite_existing":_decode_value(overwrite_existing),"allow_multi_link_break":_decode_value(allow_multi_link_break),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected("link_niagara_scratch_pad_pin_to_user_parameter",error,report,raw))
        return reflected_result(domain._named_mcp("link_niagara_scratch_pad_pin_to_user_parameter",values))


def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraResourceBindingAttachmentLibrary', None), 'verify_niagara_resource_bindings_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraResourceBinding adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraResourceBindingTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_resource_bindings_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraResourceBinding adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_resource_bindings_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_resource_bindings_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_resource_bindings_registration_carrier_invalid',str(error)))
    result['stage']='niagara_resource_bindings_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
