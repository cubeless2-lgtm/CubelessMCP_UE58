"""UE5.8.3 named local-SCS bindings; no asset preloading or registry."""

import json
import unreal

from cubeless_mcp_ue58.operations.state import OperationError


def _invoke(method, *arguments):
    library = getattr(unreal, "CubelessComponentLibrary", None)
    function = getattr(library, method, None) if library is not None else None
    if function is None:
        raise OperationError("native_component_api_unavailable", "Build/load CubelessComponentLibrary for UE5.8 before using component reads; no fallback executed")
    raw = function(*arguments)
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_component_result", "Native component read returned invalid JSON; loading/observation outcome is unknown") from error
    if (type(value) is not dict or type(value.get("success")) is not bool or type(value.get("data")) is not dict
            or any(type(value.get(key)) is not bool for key in ("mutation_attempted", "compile_requested", "save_requested", "saved", "loaded_during_read", "change_state_known", "postcondition_passed", "auto_retry_safe"))):
        raise OperationError("invalid_native_component_result", "Native component read lacks its typed observation envelope")
    return value


def list_blueprint_components(blueprint_path, component_name, max_components):
    return _invoke("list_blueprint_components", blueprint_path, component_name, max_components)


def get_component_property(blueprint_path, component_name, property_name, max_components, max_value_chars):
    return _invoke("get_component_property", blueprint_path, component_name, property_name, max_components, max_value_chars)


def _invoke_mutation(method, *arguments):
    library = getattr(unreal, "CubelessComponentLibrary", None)
    function = getattr(library, method, None) if library is not None else None
    if function is None:
        raise OperationError("native_component_api_unavailable", "Build/load the named CubelessComponentLibrary mutation helper; no fallback executed")
    try:
        raw = function(*arguments)
    finally:
        function = None
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_component_result", "Native component mutation returned invalid JSON; mutation and compilation outcomes are unknown") from error
    if (type(value) is not dict or type(value.get("success")) is not bool or type(value.get("data")) is not dict
            or any(type(value.get(key)) is not bool for key in ("mutation_attempted", "compile_requested", "compile_attempted", "save_requested", "saved", "change_state_known", "postcondition_passed"))
            or "compiled" not in value or value.get("compiled") is not None and type(value["compiled"]) is not bool
            or value.get("change_state_known") is True and type(value.get("changed")) is not bool):
        raise OperationError("invalid_native_component_result", "Native component mutation lacks its typed outcome envelope; inspect asset and journal before another mutation")
    return value


def add_component_to_blueprint(blueprint_path, component_type, component_name, transform_json, parent_component, component_properties_json, compile_blueprint):
    return _invoke_mutation("add_component_to_blueprint", blueprint_path, component_type, component_name,
                            transform_json, parent_component, component_properties_json, compile_blueprint)


def set_component_property(blueprint_path, component_name, property_name, property_value_json):
    return _invoke_mutation("set_component_property", blueprint_path, component_name, property_name, property_value_json)


def set_static_mesh_properties(blueprint_path, component_name, static_mesh, material=""):
    """Preserve the mesh-only route; combined edits prevalidate both references."""
    if material:
        result = _invoke_mutation("set_static_mesh_properties", blueprint_path, component_name, static_mesh, material)
    else:
        result = set_component_property(blueprint_path, component_name, "StaticMesh", json.dumps(static_mesh, ensure_ascii=False))
    result["data"].update(component=component_name, requested_static_mesh=static_mesh,
        legacy_internal_material_supported=True, full_legacy_command_parity=False,
        material_requested=bool(material), legacy_internal_material_scope="checked material slot 0 on supported local StaticMeshComponent templates",
        legacy_invalid_mesh_behavior="explicit_failure_instead_of_silent_success")
    if material:
        result["data"]["requested_material"] = material
    return result


def set_physics_properties(blueprint_path, component_name, simulate_physics, gravity_enabled, mass, linear_damping, angular_damping):
    return _invoke_mutation("set_physics_properties", blueprint_path, component_name,
                            simulate_physics, gravity_enabled, mass, linear_damping, angular_damping)
