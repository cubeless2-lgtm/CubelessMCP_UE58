"""Registration lifetime, using the same public API as Epic's Python toolsets."""

import atexit
import json
import unreal

from .toolsets import CubelessBlueprintTools, CubelessPythonDiagnostics
from .graph_toolset import CubelessBlueprintGraphTools
from .material_toolset import CubelessMaterialTools
from .batch_toolset import CubelessBatchTools
from .actor_toolset import CubelessActorTools
from .component_toolset import CubelessComponentTools

_CLASSES = (CubelessPythonDiagnostics, CubelessBlueprintTools, CubelessBlueprintGraphTools, CubelessMaterialTools, CubelessBatchTools, CubelessActorTools, CubelessComponentTools)
_OWNED = set()
_shutdown_hook_registered = False
_MATERIAL_PREPARATION = {"ready": False, "stage": "not_attempted", "error": ""}


def _validate_material_schema(schema):
    if not isinstance(schema, dict):
        raise RuntimeError("Material schema must be a JSON object")
    tools = {tool["name"].rsplit(".", 1)[-1]: tool["inputSchema"] for tool in schema.get("tools", [])}
    for name, parameter, default, item_type in (
        ("add_material_node", "node_position", [0, 0], "integer"),
        ("get_material_parameter_collection_values", "parameter_names", [], "string"),
    ):
        inputs = tools.get(name, {})
        item = inputs.get("properties", {}).get(parameter, {})
        if (parameter in inputs.get("required", []) or item.get("default") != default
                or item.get("type") != "array" or item.get("items", {}).get("type") != item_type):
            raise RuntimeError(f"Material schema does not preserve the {name}.{parameter} array/default contract")


def _prepare_material(registry):
    """Prepare our generated class before its executable schema is cached."""
    _MATERIAL_PREPARATION.clear()
    _MATERIAL_PREPARATION.update(ready=False, stage="invalidate_cached_schema", error="")
    try:
        if registry.is_toolset_class_registered(CubelessMaterialTools):
            registry.unregister_toolset_class(CubelessMaterialTools)
            if registry.is_toolset_class_registered(CubelessMaterialTools):
                raise RuntimeError("Could not invalidate the cached Material toolset; restart after repair")
            _OWNED.discard(CubelessMaterialTools)
        _MATERIAL_PREPARATION["stage"] = "prepare_native_metadata"
        library = getattr(unreal, "CubelessRegistrationLibrary", None)
        helper = getattr(library, "prepare_material_toolset", None)
        if helper is None:
            raise RuntimeError("Build/load the native Material registration helper before registering Material tools")
        native = json.loads(helper(CubelessMaterialTools.static_class()))
        _MATERIAL_PREPARATION["native"] = native
        if (not isinstance(native, dict) or native.get("success") is not True
                or native.get("both_exact_converters_active") is not True
                or native.get("node_position_default") != [0, 0]
                or native.get("parameter_names_default") != []):
            raise RuntimeError("Material registration preparation failed: " + str(native))
        _MATERIAL_PREPARATION.update(ready=True, stage="prepared")
    except Exception as error:
        _MATERIAL_PREPARATION.update(ready=False, error=str(error))
    return _MATERIAL_PREPARATION["ready"]


def registration_status():
    registry = unreal.ToolsetRegistry
    if not registry.is_available():
        return {"available": False, "ready": False, "toolsets": [], "material_preparation": dict(_MATERIAL_PREPARATION)}
    checks = []
    for cls in _CLASSES:
        registered = registry.is_toolset_class_registered(cls)
        schema = registry.get_toolset_json_schema(cls) if registered else ""
        schema_valid = False
        schema_error = ""
        if schema:
            try:
                parsed = json.loads(schema)
                schema_valid = bool(parsed)
                if cls is CubelessMaterialTools:
                    _validate_material_schema(parsed)
            except (ValueError, TypeError, KeyError, RuntimeError) as error:
                schema_valid = False
                schema_error = str(error)
        checks.append({"python_class": cls.__module__ + "." + cls.__name__,
                       "registered": registered, "schema_valid": schema_valid, "schema_error": schema_error})
    return {"available": True, "ready": bool(_MATERIAL_PREPARATION.get("ready") and all(item["registered"] and item["schema_valid"] for item in checks)),
            "toolsets": checks, "material_preparation": dict(_MATERIAL_PREPARATION)}


def register():
    global _shutdown_hook_registered
    registry = unreal.ToolsetRegistry
    if not registry.is_available():
        raise RuntimeError("Official Toolset Registry is unavailable")
    material_ready = _prepare_material(registry)
    for cls in _CLASSES:
        if cls is CubelessMaterialTools and not material_ready:
            continue
        try:
            if not registry.is_toolset_class_registered(cls):
                registry.register_toolset_class(cls)
                _OWNED.add(cls)
            if not registry.is_toolset_class_registered(cls):
                raise RuntimeError(f"Official Registry rejected {cls.__name__}")
            schema = registry.get_toolset_json_schema(cls)
            if not schema or not json.loads(schema):
                raise RuntimeError(f"Official Registry returned no schema for {cls.__name__}")
            if cls is CubelessMaterialTools:
                _validate_material_schema(json.loads(schema))
                _MATERIAL_PREPARATION["stage"] = "registered_schema_verified"
        except Exception as error:
            if cls is not CubelessMaterialTools:
                raise
            _MATERIAL_PREPARATION.update(ready=False, stage="material_registration_failed", error=str(error))
            # Advertise the remaining toolsets, but do not leave a Material
            # function with an unverified schema available for mutation.
            try:
                if registry.is_toolset_class_registered(cls):
                    registry.unregister_toolset_class(cls)
                if registry.is_toolset_class_registered(cls):
                    _MATERIAL_PREPARATION["unregister_error"] = "Material registry entry remains present; restart after repair"
                else:
                    _OWNED.discard(cls)
            except Exception as unregister_error:
                _MATERIAL_PREPARATION["unregister_error"] = str(unregister_error)
    if not _shutdown_hook_registered:
        atexit.register(unregister)
        _shutdown_hook_registered = True
    status = registration_status()
    if status["ready"]:
        unreal.log("CubelessMCP_UE58 Python registered: " + json.dumps(status))
    else:
        unreal.log_warning("CubelessMCP_UE58 Python registration is partial: " + json.dumps(status))
    return status


def unregister():
    registry = unreal.ToolsetRegistry
    if registry.is_available():
        for cls in reversed(_CLASSES):
            if cls in _OWNED and registry.is_toolset_class_registered(cls):
                registry.unregister_toolset_class(cls)
    _OWNED.clear()
