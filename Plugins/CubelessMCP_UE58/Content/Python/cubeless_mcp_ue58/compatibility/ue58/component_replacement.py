"""Exact finite UE58 local-SCS replacement EngineAPI; no Python loader."""
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessComponentReplacementLibrary"
BINDINGS = ("replace_blueprint_component_class",)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def replace_blueprint_component_class(blueprint_path, component_name, new_component_class,
        sphere_radius, dry_run, compile, save, refresh_nodes):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_component_replacement_api_unavailable", "Build/load the exact replacement helper; no loader, mutation or fallback ran")
    try:
        raw = function(blueprint_path, component_name, new_component_class,
            sphere_radius, dry_run, compile, save, refresh_nodes)
    finally:
        function = None  # Release the common bound callable before decode/validation/raise.
    try:
        return _decode_result(raw)
    except Exception as error:
        error.native_raw_result = raw
        error.native_call_returned = True
        raise


def replace_blueprint_component_class_raw(blueprint_path, component_name, new_component_class,
        sphere_radius, dry_run, compile, save, refresh_nodes, require_scene_compatibility):
    function = getattr(getattr(unreal, LIBRARY, None), "replace_blueprint_component_class_raw", None)
    if not callable(function):
        raise OperationError("native_component_replacement_raw_api_unavailable", "Fixed Raw9 helper unavailable; no replacement or fallback dispatched")
    try:
        raw = function(blueprint_path, component_name, new_component_class, sphere_radius,
            dry_run, compile, save, refresh_nodes, require_scene_compatibility)
    finally:
        function = None
    try:
        return _decode_result(raw)
    except Exception as error:
        error.native_raw_result = raw
        error.native_call_returned = True
        raise
