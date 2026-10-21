"""Finite UE58 Scratch Pad bindings; no server, loading or execution fallback."""
import unreal

from cubeless_mcp_ue58.operations.state import OperationError
from .functions import _decode_result

LIBRARY = "CubelessNiagaraScratchLibrary"
BINDINGS = ("create_or_duplicate_scratch_pad_module",
    "add_niagara_scratch_pad_parameter_write", "add_scratch_pad_module_to_stack",
    "insert_niagara_scratch_pad_custom_hlsl_for_pin",
    "rewire_niagara_custom_hlsl_input_to_module_parameter")


def _invoke(name, *arguments):
    if name not in BINDINGS + ("inspect_niagara_scratch_pad_target",):
        raise OperationError("native_niagara_scratch_operation_invalid", "No named finite Scratch binding")
    library = getattr(unreal, LIBRARY, None)
    function = getattr(library, name, None) if library is not None else None
    if not callable(function):
        raise OperationError("native_niagara_scratch_api_unavailable",
            LIBRARY + "." + name + " is unavailable; no load or mutation dispatched")
    return _decode_result(function(*arguments))


def inspect_niagara_scratch_pad_target(system_path, script_path, owner_kind,
        script_index_json, script_name, emitter_index_json, emitter_name):
    return _invoke("inspect_niagara_scratch_pad_target", system_path, script_path,
        owner_kind, script_index_json, script_name, emitter_index_json, emitter_name)


def create_or_duplicate_scratch_pad_module(target_system_path, source_script_path,
        source_system_path, source_owner_kind, source_script_index_json,
        source_scratch_pad_name, source_emitter_index_json, source_emitter_name,
        target_owner_kind, target_emitter_index_json, target_emitter_name,
        new_script_name, target_supported_usage, allow_source_edit, save, request_compile):
    return _invoke(BINDINGS[0], target_system_path, source_script_path, source_system_path,
        source_owner_kind, source_script_index_json, source_scratch_pad_name,
        source_emitter_index_json, source_emitter_name, target_owner_kind,
        target_emitter_index_json, target_emitter_name, new_script_name,
        target_supported_usage, allow_source_edit, save, request_compile)


def add_niagara_scratch_pad_parameter_write(system_path, parameter_name, value_json,
        parameter_type, default_value, scratch_pad_script_path, scratch_pad_owner_kind,
        scratch_pad_script_index_json, scratch_pad_name, scratch_pad_emitter_index_json,
        scratch_pad_emitter_name, skip_if_exists, allow_source_edit, save, request_compile):
    return _invoke(BINDINGS[1], system_path, parameter_name, value_json, parameter_type,
        default_value, scratch_pad_script_path, scratch_pad_owner_kind,
        scratch_pad_script_index_json, scratch_pad_name, scratch_pad_emitter_index_json,
        scratch_pad_emitter_name, skip_if_exists, allow_source_edit, save, request_compile)


def add_scratch_pad_module_to_stack(target_system_path, scratch_pad_script_path,
        scratch_pad_owner_kind, scratch_pad_script_index_json, scratch_pad_name,
        scratch_pad_emitter_index_json, scratch_pad_emitter_name, target_usage,
        target_usage_id, target_emitter_index_json, target_emitter_name, target_index_json,
        suggested_name, skip_if_duplicate, allow_source_edit, save, request_compile):
    return _invoke(BINDINGS[2], target_system_path, scratch_pad_script_path,
        scratch_pad_owner_kind, scratch_pad_script_index_json, scratch_pad_name,
        scratch_pad_emitter_index_json, scratch_pad_emitter_name, target_usage,
        target_usage_id, target_emitter_index_json, target_emitter_name, target_index_json,
        suggested_name, skip_if_duplicate, allow_source_edit, save, request_compile)


def insert_niagara_scratch_pad_custom_hlsl_for_pin(system_path, target_pin_name,
        expression, scratch_pad_script_path, scratch_pad_owner_kind,
        scratch_pad_script_index_json, scratch_pad_name, scratch_pad_emitter_index_json,
        scratch_pad_emitter_name, target_node_guid, target_node_name, target_node_index_json,
        preserve_existing_link_as, output_pin_name, signature_name, custom_hlsl_script_usage,
        inputs_json, user_parameter_inputs_json, skip_if_already_inserted,
        replace_existing_custom, rebuild_existing_custom, requires_context,
        delete_unlinked_custom_nodes, delete_unlinked_custom_input_source_nodes,
        allow_source_edit, save, request_compile, parameter_inputs_json):
    return _invoke(BINDINGS[3], system_path, target_pin_name, expression,
        scratch_pad_script_path, scratch_pad_owner_kind, scratch_pad_script_index_json,
        scratch_pad_name, scratch_pad_emitter_index_json, scratch_pad_emitter_name,
        target_node_guid, target_node_name, target_node_index_json, preserve_existing_link_as,
        output_pin_name, signature_name, custom_hlsl_script_usage, inputs_json,
        user_parameter_inputs_json, skip_if_already_inserted, replace_existing_custom,
        rebuild_existing_custom, requires_context, delete_unlinked_custom_nodes,
        delete_unlinked_custom_input_source_nodes, allow_source_edit, save, request_compile,
        parameter_inputs_json)


def rewire_niagara_custom_hlsl_input_to_module_parameter(system_path, input_name,
        scratch_pad_name, custom_node_signature_name, module_parameter_name,
        delete_orphaned_source_node, allow_source_edit, save, request_compile):
    return _invoke(BINDINGS[4], system_path, input_name, scratch_pad_name,
        custom_node_signature_name, module_parameter_name, delete_orphaned_source_node,
        allow_source_edit, save, request_compile)
