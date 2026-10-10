"""Declared Blueprint variable contracts with one standalone/batch action core."""

import copy
import json
import math
import re
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import variables as api
from . import state
from .state import OperationError, fail, mutate, new_result


def _text(value, field, *, empty=False, maximum=1023, multiline=False):
    if (type(value) is not str or (not value and not empty)
            or any(0xD800 <= ord(c) <= 0xDFFF or ord(c) == 127
                   or ord(c) < 32 and not (multiline and c in "\r\n\t") for c in value)
            or len(value.encode("utf-16-le")) // 2 > maximum):
        raise OperationError("variable_input_invalid", f"{field} must be {'a' if empty else 'a nonempty'} string of at most {maximum} UTF-16 units without invalid controls/surrogates")
    return value


def _path(blueprint_name, *, write=False):
    value = _text(blueprint_name, "blueprint_name").strip()
    if not value.startswith("/"):
        if not value or any(c in value for c in "/.\\:'\""):
            raise OperationError("blueprint_path_invalid", "Use an exact Blueprint package/object path or a simple name under /Game/Blueprints")
        value = "/Game/Blueprints/" + value
    if write and not value.startswith("/Game/"):
        raise OperationError("blueprint_path_invalid", "Variable mutations require a runtime-safe /Game Blueprint")
    return _text(value, "resolved_blueprint_path")


def _json_value(text, field):
    if type(text) is not str or len(text.encode("utf-16-le", errors="surrogatepass")) // 2 > 65536:
        raise OperationError("variable_json_invalid", field + " must be a JSON string of at most 65536 UTF-16 units")

    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key: " + key)
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValueError("nonfinite constant: " + value)

    try:
        value = json.loads(text, object_pairs_hook=unique, parse_constant=invalid_constant)
        count = 0

        def visit(item, depth):
            nonlocal count
            count += 1
            if depth > 8 or count > 1024:
                raise ValueError("JSON exceeds depth 8 or 1024 values")
            if type(item) is dict:
                for key, child in item.items():
                    _text(key, "JSON key")
                    visit(child, depth + 1)
            elif type(item) is list:
                for child in item:
                    visit(child, depth + 1)
            elif type(item) is str:
                if "\x00" in item or any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                    raise ValueError("JSON strings cannot contain NUL or unpaired surrogates")
            elif type(item) in (int, float) and (abs(item) > 2**53 - 1 or not math.isfinite(item)):
                raise ValueError("JSON numbers must be finite with magnitude at most 2^53-1")

        visit(value, 0)
        return value
    except (ValueError, RecursionError) as error:
        raise OperationError("variable_json_invalid", field + ": " + str(error)) from error


def _json_text(value):
    return json.dumps(value, ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(",", ":"))


def _boolean(value, field):
    if type(value) is not bool:
        raise OperationError("variable_input_invalid", field + " must be a boolean")
    return value


def _add_arguments(blueprint_name, variable_name, variable_type, is_exposed, default_value_json,
        category, tooltip, friendly_name, type_object, is_array, metadata_json, variable_guid, is_editable):
    name = _text(variable_name, "variable_name")
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,99}", name) is None or name.lower() == "none":
        raise OperationError("variable_name_invalid", "New variable names must be ASCII identifiers of 1..100 characters and cannot be None")
    default = _json_value(default_value_json, "default_value_json") if default_value_json is not None else None
    metadata = _json_value(metadata_json, "metadata_json")
    if type(metadata) is not dict or len(metadata) > 64:
        raise OperationError("variable_metadata_invalid", "metadata_json must be an object containing at most 64 scalar fields")
    if len({key.casefold() for key in metadata}) != len(metadata):
        raise OperationError("variable_metadata_invalid", "Metadata keys cannot collide under case-insensitive FName lookup")
    for key, value in metadata.items():
        if type(value) not in (str, int, float, bool):
            raise OperationError("variable_metadata_invalid", "Metadata values must be strings, booleans or finite numbers")
        if type(value) is str:
            _text(value, "metadata value", empty=True, maximum=4096, multiline=True)
    guid = ""
    if variable_guid is not None and variable_guid != "":
        try:
            guid_value = uuid.UUID(_text(variable_guid, "variable_guid"))
            if guid_value.int == 0:
                raise ValueError("zero GUID is invalid")
            guid = guid_value.hex
        except ValueError as error:
            raise OperationError("variable_guid_invalid", "variable_guid must be a nonzero GUID; native also rejects collisions") from error
    return {"blueprint_path": _path(blueprint_name, write=True), "variable_name": name,
        "variable_type": _text(variable_type, "variable_type"), "is_exposed": _boolean(is_exposed, "is_exposed"),
        # Legacy public Any=None omits the default entirely, including explicit JSON null.
        "default_value_json": "" if default is None else _json_text(default),
        "category": _text("" if category is None else category, "category", empty=True),
        "tooltip": _text("" if tooltip is None else tooltip, "tooltip", empty=True, maximum=4096, multiline=True),
        "friendly_name": _text("" if friendly_name is None else friendly_name, "friendly_name", empty=True),
        "type_object": _text("" if type_object is None else type_object, "type_object", empty=True),
        "is_array": _boolean(is_array, "is_array"), "metadata_json": _json_text(metadata),
        "variable_guid": guid, "is_editable": False if is_editable is None else _boolean(is_editable, "is_editable")}


def _set_arguments(blueprint_name, variable_name, default_value_json, compile, save, dry_run):
    return {"blueprint_path": _path(blueprint_name, write=True), "variable_name": _text(variable_name, "variable_name"),
        "default_value_json": _json_text(_json_value(default_value_json, "default_value_json")),
        "compile": _boolean(compile, "compile"), "save": _boolean(save, "save"), "dry_run": _boolean(dry_run, "dry_run")}


def _apply_native(result, identity, arguments, native, *, read=False):
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(operation=identity, native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not native["change_state_known"],
        mutation_performed=native.get("changed") if native["change_state_known"] else None)
    result["stage"] = native.get("stage", "native_read" if read else "native_mutation")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status=native.get("compile_status", "compiled" if native["compiled"] is True else "attempted" if native["compile_attempted"] else "not_attempted" if native["compile_requested"] else "not_requested"))
    for key in ("implicit_skeleton_compile_attempted", "implicit_structural_compile"):
        if key in native:
            result["compile"][key] = copy.deepcopy(native[key])
    compiler = native["data"].get("compile")
    if type(compiler) is dict:
        for source, target in (("status", "status"), ("compile_error_count", "error_count"),
                ("compile_warning_count", "warning_count"), ("diagnostics", "diagnostics")):
            if source in compiler:
                result["compile"][target] = copy.deepcopy(compiler[source])
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    result["changes"] = [] if native["change_state_known"] and native.get("changed") is False and not native["mutation_attempted"] else [{
        "kind": identity, "asset_path": arguments["blueprint_path"], "variable_name": arguments.get("variable_name"),
        "explicit_mutation_requested": not read and not arguments.get("dry_run", False),
        "attempted": native["mutation_attempted"], "changed": native.get("changed") if native["change_state_known"] else None,
        "change_state_known": native["change_state_known"], "dirty_before": native.get("dirty_before"), "dirty_after": native.get("dirty_after"),
        "outcome": "scoped_native_observation" if native["change_state_known"] else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_variable_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], independent_value_check="not_run", disk_reload="not_run")
    requested_compile, requested_save = arguments.get("compile", False), arguments.get("save", False)
    dry = arguments.get("dry_run", False)
    if (native["compile_requested"] != requested_compile or native["save_requested"] != requested_save
            or (read or dry or not requested_compile) and native["compile_attempted"]
            or (read or dry or not requested_save) and (native["save_attempted"] or native["saved"])
            or (read or dry) and native["mutation_attempted"]):
        result["data"]["mutation_state_unknown"] = True
        raise OperationError("unexpected_variable_mutation", "Native result conflicts with the requested mutation/compile/save contract; retained observations require inspection")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_variable_operation_failed", native.get("error") or "Native variable operation failed; partial outcomes are retained")
    if (not native["change_state_known"] or not native["postcondition_passed"]
            or (read or dry) and native.get("changed") is not False
            or not dry and requested_compile and native["compiled"] is not True
            or not dry and requested_save and native["saved"] is not True):
        raise OperationError("variable_result_unverified", "Native success lacks known postconditions or the requested compile/save result")
    result["success"] = True
    result["stage"] = "complete"
    result["retry"]["duplicate_risk"] = "variable_action_would_repeat" if native.get("changed") else "none_observed"


def _run_action(result, identity, arguments, checkpoint_callback=None, *, read=False):
    result["data"].update(operation=identity, adapter_preloaded_asset=False, resolved_blueprint_path=arguments["blueprint_path"],
        journaled=not read or checkpoint_callback is not None, full_legacy_variable_parity=False,
        loading_side_effects_verified=False, generated_cdo_may_be_stale=True)
    for key in ("compile", "save"):
        result[key].update(requested=arguments.get(key, False), attempted=False)
    if not read:
        result["stage"] = "before_mutation_checkpoint"
        result["data"]["pending_mutation"] = {"operation": identity, **arguments,
            "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_asset_and_journal"}
        (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_read" if read else "native_mutation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "asset_path": arguments["blueprint_path"], "attempted": None,
        "changed": None, "change_state_known": False, "outcome": "unknown_until_native_result"}]
    for key in ("compile", "save"):
        if arguments.get(key, False) and not arguments.get("dry_run", False):
            result[key].update(attempted=None, status="unknown_until_native_result")
            result[key]["compiled" if key == "compile" else "saved"] = None
    try:
        native = getattr(api, identity)(**arguments)
    except Exception as error:
        if getattr(error, "code", "") == "native_variable_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="not_dispatched")
            result["save"].update(attempted=False, saved=False)
        raise
    _apply_native(result, identity, arguments, native, read=read)


def _mutate_prepared(identity, prepare, operation_id):
    try:
        arguments = prepare()
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate(identity, arguments, operation_id, lambda result: _run_action(result, identity, arguments))


def add_blueprint_variable(blueprint_name, variable_name, variable_type, is_exposed=False, default_value_json=None,
        category=None, tooltip=None, friendly_name=None, type_object=None, is_array=False, metadata_json="{}",
        operation_id=None, variable_guid=None, is_editable=None):
    return _mutate_prepared("add_blueprint_variable", lambda: _add_arguments(blueprint_name, variable_name, variable_type,
        is_exposed, default_value_json, category, tooltip, friendly_name, type_object, is_array, metadata_json, variable_guid, is_editable), operation_id)


def add_blueprint_variable_action(result, blueprint_name, variable_name, variable_type, is_exposed=False, default_value_json=None,
        category=None, tooltip=None, friendly_name=None, type_object=None, is_array=False, metadata_json="{}",
        variable_guid=None, is_editable=None, checkpoint_callback=None):
    _run_action(result, "add_blueprint_variable", _add_arguments(blueprint_name, variable_name, variable_type,
        is_exposed, default_value_json, category, tooltip, friendly_name, type_object, is_array, metadata_json, variable_guid, is_editable), checkpoint_callback)


def set_blueprint_variable_default(blueprint_name, variable_name, default_value_json, compile=True, save=True, dry_run=False, operation_id=None):
    return _mutate_prepared("set_blueprint_variable_default", lambda: _set_arguments(blueprint_name, variable_name, default_value_json, compile, save, dry_run), operation_id)


def set_blueprint_variable_default_action(result, blueprint_name, variable_name, default_value_json, compile=True, save=True, dry_run=False, checkpoint_callback=None):
    _run_action(result, "set_blueprint_variable_default", _set_arguments(blueprint_name, variable_name, default_value_json, compile, save, dry_run), checkpoint_callback)


def inspect_blueprint_variables_action(result, blueprint_name, checkpoint_callback=None):
    _run_action(result, "inspect_blueprint_variables", {"blueprint_path": _path(blueprint_name)}, checkpoint_callback, read=True)


def inspect_blueprint_variable_metadata_action(result, blueprint_name, variable_name, checkpoint_callback=None):
    _run_action(result, "inspect_blueprint_variable_metadata", {"blueprint_path": _path(blueprint_name),
        "variable_name": _text(variable_name, "variable_name")}, checkpoint_callback, read=True)


def _read(action, *arguments):
    result = new_result()
    try:
        action(result, *arguments)
    except Exception as error:
        fail(result, error)
    return result


def inspect_blueprint_variables(blueprint_name):
    return _read(inspect_blueprint_variables_action, blueprint_name)


def inspect_blueprint_variable_metadata(blueprint_name, variable_name):
    return _read(inspect_blueprint_variable_metadata_action, blueprint_name, variable_name)
