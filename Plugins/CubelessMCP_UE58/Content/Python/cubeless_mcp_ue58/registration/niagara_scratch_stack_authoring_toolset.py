"""Original Scratch input/assignment/StackContext3 through exact per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_scratch_stack_authoring as domain

@unreal.uclass()
class CubelessNiagaraScratchStackAuthoringTools(unreal.ToolsetDefinition):
    "Native owns graph transition/readback, module compile-wait-save, assignment Native save plus Python followup, and actual partial prefixes."
    @toolset_registry.tool_call
    @staticmethod
    def add_niagara_scratch_pad_module_input(system_path:str,input_name:str,default_value:str,input_type:str="\"bool\"",consumer_pin_name:str="\"\"",scratch_pad_script_path:str="\"\"",scratch_pad_owner_kind:str="\"system\"",scratch_pad_script_index:str="null",scratch_pad_name:str="\"\"",scratch_pad_emitter_index:str="null",scratch_pad_emitter_name:str="\"\"",require_consumer_link:str="true",overwrite_existing_link:str="false",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="false",timeout_seconds:str="10.0",poll_interval_seconds:str="0.1")->CubelessPythonAsyncOperationResult:
        "\n        Add a reachable Module.* input read inside a target-local Scratch Pad module graph.\n\n        Args:\n            system_path: Niagara System package path or object path\n            input_name: Module input name, with or without Module. prefix\n            default_value: JSON default value for the input\n            input_type: Niagara type alias, such as bool, float, int, vec2, vec3, vec4, color, or position\n            consumer_pin_name: Existing Scratch Pad ParameterMapSet value pin to consume the Module.* read\n            scratch_pad_script_path: Direct target Scratch Pad script path; required unless name or index is provided\n            scratch_pad_owner_kind: system or emitter\n            scratch_pad_script_index: Scratch Pad script index; required unless path or name is provided\n            scratch_pad_name: Scratch Pad script name; required unless path or index is provided\n            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads\n            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads\n            require_consumer_link: Require consumer_pin_name so the read is reachable by stack traversal\n            overwrite_existing_link: Replace existing links on the consumer pin\n            allow_source_edit: Allow edits outside /Game/_MCP_Temp\n            save: Save the Niagara System after editing\n            request_compile: Request Niagara compile after editing\n            wait_for_completion: Wait for compile completion\n            timeout_seconds: Compile wait timeout\n            poll_interval_seconds: Compile wait poll interval\n         Native actual prefixes are authoritative; source registration is not full feature completion."
        rejected,report,raw=_guard_admission("add_niagara_scratch_pad_module_input")
        if rejected is not None:return domain.completed_result(rejected)
        try:values={"system_path":_decode_value(system_path),"input_name":_decode_value(input_name),"default_value":_decode_value(default_value),"input_type":_decode_value(input_type),"consumer_pin_name":_decode_value(consumer_pin_name),"scratch_pad_script_path":_decode_value(scratch_pad_script_path),"scratch_pad_owner_kind":_decode_value(scratch_pad_owner_kind),"scratch_pad_script_index":_decode_value(scratch_pad_script_index),"scratch_pad_name":_decode_value(scratch_pad_name),"scratch_pad_emitter_index":_decode_value(scratch_pad_emitter_index),"scratch_pad_emitter_name":_decode_value(scratch_pad_emitter_name),"require_consumer_link":_decode_value(require_consumer_link),"overwrite_existing_link":_decode_value(overwrite_existing_link),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("add_niagara_scratch_pad_module_input",error,report,raw))
        return domain._named_mcp("add_niagara_scratch_pad_module_input",values)

    @toolset_registry.tool_call
    @staticmethod
    def add_niagara_scratch_pad_assignment_module_to_stack(target_system_path:str,parameter_name:str="\"Particles.SpriteSize\"",parameter_type:str="\"vec2\"",default_value:str="\"50.000000,50.000000\"",value:str="null",target_usage:str="\"ParticleUpdateScript\"",target_usage_id:str="\"\"",target_emitter_index:str="null",target_emitter_name:str="\"\"",target_index:str="null",skip_if_duplicate:str="true",allow_assignment_module_route:str="\"\"",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="true",timeout_seconds:str="10.0",poll_interval_seconds:str="0.1")->CubelessPythonAsyncOperationResult:
        "\n        Add a guarded temp-only Niagara assignment module to the target stack.\n\n        Args:\n            target_system_path: Target Niagara System package path or object path\n            parameter_name: Admitted target parameter; currently Particles.SpriteSize, Particles.Color, Particles.Lifetime, or Particles.Velocity\n            parameter_type: Admitted type; vec2 / Vector2D for SpriteSize, color / Linear Color for Color, float for Lifetime, vec3 / Vector for Velocity\n            default_value: Raw Unreal pin default string; takes precedence over value\n            value: JSON value converted to a Niagara pin default string when default_value is empty\n            target_usage: Target stack usage, such as ParticleUpdateScript\n            target_usage_id: Optional target output usage GUID\n            target_emitter_index: Optional target emitter index for emitter/particle stacks\n            target_emitter_name: Optional target emitter name for emitter/particle stacks\n            target_index: Optional stack insertion index; omit or -1 to append\n            skip_if_duplicate: Skip without mutation if the same assignment target already exists\n            allow_assignment_module_route: Required proof token for this not-yet-release-ready route\n            allow_source_edit: Allow editing target systems outside /Game/_MCP_Temp/NiagaraGenerated/\n            save: Save the target system after stack insertion\n            request_compile: Request Niagara compile after stack insertion\n            wait_for_completion: Wait for compile completion\n            timeout_seconds: Compile wait timeout\n            poll_interval_seconds: Compile wait poll interval\n         Native actual prefixes are authoritative; source registration is not full feature completion."
        rejected,report,raw=_guard_admission("add_niagara_scratch_pad_assignment_module_to_stack")
        if rejected is not None:return domain.completed_result(rejected)
        try:values={"target_system_path":_decode_value(target_system_path),"parameter_name":_decode_value(parameter_name),"parameter_type":_decode_value(parameter_type),"default_value":_decode_value(default_value),"value":_decode_value(value),"target_usage":_decode_value(target_usage),"target_usage_id":_decode_value(target_usage_id),"target_emitter_index":_decode_value(target_emitter_index),"target_emitter_name":_decode_value(target_emitter_name),"target_index":_decode_value(target_index),"skip_if_duplicate":_decode_value(skip_if_duplicate),"allow_assignment_module_route":_decode_value(allow_assignment_module_route),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("add_niagara_scratch_pad_assignment_module_to_stack",error,report,raw))
        return domain._named_mcp("add_niagara_scratch_pad_assignment_module_to_stack",values)

    @toolset_registry.tool_call
    @staticmethod
    def wrap_niagara_scratch_pad_output_with_stack_context(system_path:str,target_pin_name:str="\"StackContext.RGBA\"",scratch_pad_script_path:str="\"\"",scratch_pad_owner_kind:str="\"system\"",scratch_pad_script_index:str="null",scratch_pad_name:str="\"RenderCircleToGrid\"",scratch_pad_emitter_index:str="null",scratch_pad_emitter_name:str="\"\"",target_node_guid:str="\"\"",target_node_index:str="null",parameter_map_set_index:str="null",previous_stack_value_pin_name:str="\"PreviousStackValue\"",local_value_pin_name:str="\"LocalStampValue\"",expression:str="\"max(PreviousStackValue, LocalStampValue)\"",skip_if_already_wrapped:str="true",replace_existing_custom:str="true",allow_source_edit:str="false",save:str="true",request_compile:str="true")->CubelessPythonOperationResult:
        "\n        Wrap a Scratch Pad ParameterMapSet value input with a Custom HLSL accumulator.\n\n        The existing linked value becomes local_value_pin_name, while the expression can\n        read the incoming StackContext through the dynamic input's ParameterMap.\n\n        Args:\n            system_path: Niagara System package path or object path\n            target_pin_name: ParameterMapSet input pin to wrap, defaults to StackContext.RGBA\n            scratch_pad_script_path: Direct target Scratch Pad script path\n            scratch_pad_owner_kind: system or emitter\n            scratch_pad_script_index: Scratch Pad script index\n            scratch_pad_name: Scratch Pad script name, defaults to RenderCircleToGrid\n            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads\n            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads\n            target_node_guid: Optional target ParameterMapSet node GUID\n            target_node_index: Optional graph node index from inspect_niagara_graph\n            parameter_map_set_index: Optional ParameterMapSet-only index in the Scratch Pad graph\n            previous_stack_value_pin_name: Custom HLSL input name for the incoming StackContext value\n            local_value_pin_name: Custom HLSL input name for the previous linked value\n            expression: Dynamic Custom HLSL expression, defaults to max(PreviousStackValue, LocalStampValue)\n            skip_if_already_wrapped: Return success when the same Custom HLSL wrapper already exists\n            replace_existing_custom: Replace an existing Custom HLSL wrapper while preserving local_value_pin_name\n            allow_source_edit: Allow edits outside /Game/_MCP_Temp\n            save: Save the Niagara System after editing\n            request_compile: Request Niagara compile after editing\n         Native actual prefixes are authoritative; source registration is not full feature completion."
        rejected,report,raw=_guard_admission("wrap_niagara_scratch_pad_output_with_stack_context")
        if rejected is not None:return reflected_result(rejected)
        try:values={"system_path":_decode_value(system_path),"target_pin_name":_decode_value(target_pin_name),"scratch_pad_script_path":_decode_value(scratch_pad_script_path),"scratch_pad_owner_kind":_decode_value(scratch_pad_owner_kind),"scratch_pad_script_index":_decode_value(scratch_pad_script_index),"scratch_pad_name":_decode_value(scratch_pad_name),"scratch_pad_emitter_index":_decode_value(scratch_pad_emitter_index),"scratch_pad_emitter_name":_decode_value(scratch_pad_emitter_name),"target_node_guid":_decode_value(target_node_guid),"target_node_index":_decode_value(target_node_index),"parameter_map_set_index":_decode_value(parameter_map_set_index),"previous_stack_value_pin_name":_decode_value(previous_stack_value_pin_name),"local_value_pin_name":_decode_value(local_value_pin_name),"expression":_decode_value(expression),"skip_if_already_wrapped":_decode_value(skip_if_already_wrapped),"replace_existing_custom":_decode_value(replace_existing_custom),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected("wrap_niagara_scratch_pad_output_with_stack_context",error,report,raw))
        return reflected_result(domain._named_mcp("wrap_niagara_scratch_pad_output_with_stack_context",values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraScratchStackAuthoringAttachmentLibrary', None), 'verify_niagara_scratch_stack_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraScratchStackAuthoring adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraScratchStackAuthoringTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_scratch_stack_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraScratchStackAuthoring adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_scratch_stack_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_scratch_stack_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_scratch_stack_authoring_registration_carrier_invalid',str(error)))
    result['stage']='niagara_scratch_stack_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
