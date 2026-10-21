"""UE5.8 named variable bindings. Path conversion never loads an asset."""

import json
import unreal

from cubeless_mcp_ue58.operations.state import OperationError


def _invoke(method, *arguments):
    function = getattr(getattr(unreal, "CubelessVariableLibrary", None), method, None)
    if function is None:
        raise OperationError("native_variable_api_unavailable", "Build/load the named CubelessVariableLibrary helper; no fallback executed")
    raw = function(*arguments)
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_variable_result", "Native variable result is not valid JSON; inspect the asset and journal before another action") from error
    flags = ("success", "mutation_attempted", "change_state_known", "postcondition_passed",
             "compile_requested", "compile_attempted", "save_requested", "save_attempted", "saved")
    if (type(value) is not dict or type(value.get("data")) is not dict
            or any(type(value.get(key)) is not bool for key in flags)
            or "compiled" not in value or value["compiled"] is not None and type(value["compiled"]) is not bool
            or value["change_state_known"] and type(value.get("changed")) is not bool):
        raise OperationError("invalid_native_variable_result", "Native variable result lacks its typed outcome envelope; the dispatched outcome remains unknown")
    return value


def add_blueprint_variable(blueprint_path, variable_name, variable_type, is_exposed, default_value_json,
        category, tooltip, friendly_name, type_object, is_array, metadata_json, variable_guid, is_editable, is_transient=False):
    arguments = (blueprint_path, variable_name, variable_type, is_exposed,
        default_value_json, category, tooltip, friendly_name, type_object, is_array, metadata_json, variable_guid, is_editable)
    # Keep the old binding call shape when the native additive default applies.
    if is_transient:
        arguments += (True,)
    return _invoke("add_blueprint_variable", *arguments)


def inspect_blueprint_variables(blueprint_path):
    return _invoke("inspect_blueprint_variables", blueprint_path)


def inspect_blueprint_variable_metadata(blueprint_path, variable_name):
    return _invoke("inspect_blueprint_variable_metadata", blueprint_path, variable_name)


def set_blueprint_variable_default(blueprint_path, variable_name, default_value_json, compile, save, dry_run):
    return _invoke("set_blueprint_variable_default", blueprint_path, variable_name, default_value_json, compile, save, dry_run)
