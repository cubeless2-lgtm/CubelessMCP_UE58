"""Finite UE58 module-input bindings; no loading or transport fallback."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessNiagaraModuleInputLibrary"
BINDINGS = (
    "set_niagara_module_input_literal_override",
    "set_niagara_data_interface_module_input",
    "set_niagara_module_input_linked_parameter",
)
SELECTOR_ARGUMENTS = ("system_path", "emitter_index_json", "emitter_name",
    "module_index_json", "module_name", "module_node_guid", "input_name")
COMMON_ARGUMENTS = ("emitter_index_json", "emitter_name", "module_index_json",
    "module_name", "module_node_guid", "overwrite_existing",
    "allow_source_edit", "save", "request_compile")


def _invoke(name, *arguments):
    function = getattr(getattr(unreal, LIBRARY, None), name, None)
    if not callable(function):
        raise OperationError("native_niagara_module_input_api_unavailable",
            "Build/load the exact CubelessNiagaraModuleInputLibrary binding; no fallback or asset load executed")
    return _decode_result(function(*arguments))


def inspect_niagara_module_input_target(system_path, emitter_index_json,
        emitter_name, module_index_json, module_name, module_node_guid, input_name):
    """Hidden bounded prerequisite/readback observer, not a public executor."""
    return _invoke("inspect_niagara_module_input_target", system_path,
        emitter_index_json, emitter_name, module_index_json, module_name,
        module_node_guid, input_name)


def set_niagara_module_input_literal_override(system_path, input_name, value_json,
        emitter_index_json, emitter_name, module_index_json, module_name,
        module_node_guid, overwrite_existing, allow_source_edit, save,
        request_compile):
    return _invoke(BINDINGS[0], system_path, input_name, value_json,
        emitter_index_json, emitter_name, module_index_json, module_name,
        module_node_guid, overwrite_existing, allow_source_edit, save, request_compile)


def set_niagara_data_interface_module_input(system_path, input_name, properties_json,
        emitter_index_json, emitter_name, module_index_json, module_name,
        module_node_guid, overwrite_existing, allow_source_edit, save,
        request_compile):
    return _invoke(BINDINGS[1], system_path, input_name, properties_json,
        emitter_index_json, emitter_name, module_index_json, module_name,
        module_node_guid, overwrite_existing, allow_source_edit, save, request_compile)


def set_niagara_module_input_linked_parameter(system_path, input_name,
        linked_parameter_name, emitter_index_json, emitter_name, module_index_json,
        module_name, module_node_guid, overwrite_existing, allow_source_edit, save,
        request_compile):
    return _invoke(BINDINGS[2], system_path, input_name, linked_parameter_name,
        emitter_index_json, emitter_name, module_index_json, module_name,
        module_node_guid, overwrite_existing, allow_source_edit, save, request_compile)
