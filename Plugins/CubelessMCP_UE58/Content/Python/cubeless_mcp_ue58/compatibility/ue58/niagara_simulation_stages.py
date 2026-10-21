"""Find-only UE58 stage bindings; no loading, wait or transport fallback."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessNiagaraSimulationStageLibrary"
BINDINGS = ("add_niagara_grid_simulation_stage", "inspect_niagara_simulation_stage_target",
    "set_niagara_simulation_stage_settings", "inspect_niagara_simulation_stage_settings_target")
NATIVE_ARGUMENTS = ("system_path", "emitter_index_json", "emitter_name", "stage_name",
    "data_interface_name", "insert_index_json", "skip_if_duplicate", "enabled",
    "options_json", "allow_source_edit", "save", "request_compile")
SETTINGS_NATIVE_ARGUMENTS = ("system_path", "emitter_index_json", "emitter_name", "stage_index_json",
    "stage_name", "options_json", "allow_source_edit", "save", "request_compile")


def _binding(name):
    function = getattr(getattr(unreal, LIBRARY, None), name, None)
    if not callable(function):
        raise OperationError("native_niagara_stage_api_unavailable",
            "Build/load the exact CubelessNiagaraSimulationStageLibrary binding; no fallback or asset load executed")
    return function


def add_niagara_grid_simulation_stage(system_path, emitter_index_json, emitter_name,
        stage_name, data_interface_name, insert_index_json, skip_if_duplicate,
        enabled, options_json, allow_source_edit, save, request_compile):
    return _decode_result(_binding(BINDINGS[0])(system_path, emitter_index_json,
        emitter_name, stage_name, data_interface_name, insert_index_json,
        skip_if_duplicate, enabled, options_json, allow_source_edit, save, request_compile))


def inspect_niagara_simulation_stage_target(system_path, emitter_index_json, emitter_name):
    """Hidden prerequisite/readback only; this is not another public Toolset tool."""
    return _decode_result(_binding(BINDINGS[1])(system_path, emitter_index_json, emitter_name))


def set_niagara_simulation_stage_settings(system_path, emitter_index_json, emitter_name,
        stage_index_json, stage_name, options_json, allow_source_edit, save, request_compile):
    return _decode_result(_binding(BINDINGS[2])(system_path, emitter_index_json,
        emitter_name, stage_index_json, stage_name, options_json, allow_source_edit, save, request_compile))


def inspect_niagara_simulation_stage_settings_target(system_path, emitter_index_json,
        emitter_name, stage_index_json, stage_name):
    """Hidden joint-selector readback only; no public inspection alias is added."""
    return _decode_result(_binding(BINDINGS[3])(system_path, emitter_index_json,
        emitter_name, stage_index_json, stage_name))


def async_binding_status():
    from . import niagara_event_stage_authoring as shared
    status=shared.async_binding_status()
    return {name:status.get(name) is True for name in ("add_niagara_grid_simulation_stage","set_niagara_simulation_stage_settings")}
