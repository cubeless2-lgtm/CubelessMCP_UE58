"""Named typed User parameter contracts with one journal and native partial outcomes."""

import copy
import json

from cubeless_mcp_ue58.compatibility.ue58 import niagara_parameters as api
from . import state, variables
from .state import OperationError


def _text(value, field):
    try:
        return variables._text(value, field)
    except OperationError as error:
        raise OperationError("niagara_parameter_input_invalid", str(error)) from error


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("niagara_parameter_input_invalid", field + " must be an exact boolean")
    return value


def _json(value, field, *, array=False):
    try:
        parsed = variables._json_value(value, field)
    except OperationError as error:
        raise OperationError("niagara_parameter_json_invalid", str(error)) from error
    if array and type(parsed) is not list:
        raise OperationError("niagara_parameter_json_invalid", field + " must encode an array of ordered entries")
    # Preserve null, omission, aliases and row order. Per-row validity belongs to
    # native processing so a later bad row cannot erase stop/continue semantics.
    return json.dumps(parsed, ensure_ascii=False, allow_nan=False, separators=(",", ":"))


def _system(value):
    path = _text(value, "system_path")
    if not path.startswith("/") or any(char in path for char in ("\\", ":", "'", '"')):
        raise OperationError("niagara_parameter_path_invalid", "Use the exact already-loaded Niagara system package or object path")
    return path


def _set_arguments(system_path, parameter_name, value_json, allow_source_edit, save):
    return {"system_path": _system(system_path), "parameter_name": _text(parameter_name, "parameter_name"),
        "value_json": _json(value_json, "value_json"),
        "allow_source_edit": _bool(allow_source_edit, "allow_source_edit"), "save": _bool(save, "save")}


def _upsert_arguments(system_path, parameters_json, continue_on_error, allow_source_edit, save, request_compile):
    return {"system_path": _system(system_path), "parameters_json": _json(parameters_json, "parameters_json", array=True),
        "continue_on_error": _bool(continue_on_error, "continue_on_error"),
        "allow_source_edit": _bool(allow_source_edit, "allow_source_edit"), "save": _bool(save, "save"),
        "request_compile": _bool(request_compile, "request_compile")}


def _apply_native(result, identity, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(operation=identity,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None,
        full_legacy_niagara_user_parameter_parity=False,
        particle_export_transport_verified=False)
    result["stage"] = native.get("stage", "native_niagara_parameter_operation")
    compiled = native["compiled"]
    status = "passed" if compiled is True else "failed" if compiled is False else (
        "requested_completion_unobserved" if native["compile_requested"] else "not_requested")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=compiled, status=status)
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_change = known and native.get("changed") is False and not native["mutation_attempted"]
    result["changes"] = [] if no_change else [{"kind": identity, "asset_path": arguments["system_path"],
        "attempted": native["mutation_attempted"], "changed": native.get("changed") if known else None,
        "change_state_known": known, "outcome": "native_parameter_store_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_parameter_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        particle_export_transport="not_run", packaged_runtime="not_run")


def _run(result, identity, arguments, checkpoint_callback=None, *, read=False):
    result["data"].update(operation=identity, requested_system_path=arguments["system_path"],
        adapter_preloaded_asset=False, adapter_load_calls=0,
        full_legacy_niagara_user_parameter_parity=False, particle_export_transport_verified=False)
    if not read:
        result["data"]["pending_mutation"] = {"operation": identity, **arguments,
            "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_system_and_journal"}
        result["stage"] = "before_mutation_checkpoint"
        (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_niagara_parameter_read" if read else "native_niagara_parameter_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "asset_path": arguments["system_path"],
        "attempted": None, "changed": None, "change_state_known": False, "outcome": "unknown_until_native_result"}]
    try:
        native = getattr(api, identity)(**arguments)
    except Exception as error:
        if getattr(error, "code", "") == "native_niagara_parameter_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    if read and (native["mutation_attempted"] or native.get("changed") is True or any(native[key] for key in (
            "compile_requested", "compile_attempted", "save_requested", "save_attempted", "saved")) or native["compiled"] is True):
        result["data"]["mutation_state_unknown"] = True
        raise OperationError("niagara_parameter_read_changed_state", "Inspection reported modification/compile/save; native observations retained")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_niagara_parameter_operation_failed",
            native.get("error") or "Native parameter operation failed; partial rows and actual work are retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("niagara_parameter_result_unverified", "Native success lacks known parameter-store postconditions")
    result.update(success=True, stage="complete")
    repeated_work = (native["mutation_attempted"] or native["compile_attempted"]
        or native["save_attempted"] or native.get("changed") is True)
    result["retry"]["duplicate_risk"] = "parameter_action_would_repeat" if repeated_work else "none_observed"


def _prepared(identity, prepare, operation_id):
    try:
        arguments = prepare()
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    return state.mutate(identity, arguments, operation_id, lambda result: _run(result, identity, arguments))


def inspect_niagara_user_parameters(system_path):
    result = state.new_result()
    try:
        inspect_niagara_user_parameters_action(result, system_path)
    except Exception as error:
        state.fail(result, error)
    return result


def inspect_niagara_user_parameters_action(result, system_path, checkpoint_callback=None):
    _run(result, "inspect_niagara_user_parameters", {"system_path": _system(system_path)},
        checkpoint_callback, read=True)


def set_niagara_user_parameter(system_path, parameter_name, value_json, allow_source_edit=False, save=True,
        operation_id=None):
    return _prepared("set_niagara_user_parameter", lambda: _set_arguments(system_path, parameter_name, value_json,
        allow_source_edit, save), operation_id)


def set_niagara_user_parameter_action(result, system_path, parameter_name, value_json, allow_source_edit=False,
        save=True, checkpoint_callback=None):
    _run(result, "set_niagara_user_parameter", _set_arguments(system_path, parameter_name, value_json,
        allow_source_edit, save), checkpoint_callback)


def upsert_niagara_user_parameters(system_path, parameters_json, continue_on_error=False,
        allow_source_edit=False, save=True, request_compile=True, operation_id=None):
    return _prepared("upsert_niagara_user_parameters", lambda: _upsert_arguments(system_path, parameters_json,
        continue_on_error, allow_source_edit, save, request_compile), operation_id)


def upsert_niagara_user_parameters_action(result, system_path, parameters_json, continue_on_error=False,
        allow_source_edit=False, save=True, request_compile=True, checkpoint_callback=None):
    _run(result, "upsert_niagara_user_parameters", _upsert_arguments(system_path, parameters_json,
        continue_on_error, allow_source_edit, save, request_compile), checkpoint_callback)
