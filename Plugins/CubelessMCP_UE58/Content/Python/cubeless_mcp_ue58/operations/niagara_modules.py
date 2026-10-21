"""Legacy stock module insertion and duplicate skip with native stage/outcome evidence."""

import copy
import json

from cubeless_mcp_ue58.compatibility.ue58 import niagara_modules as api
from . import state, variables
from .state import OperationError


IDENTITY = "add_niagara_stock_module_to_stack"


def _text(value, field, *, empty=False):
    try:
        return variables._text(value, field, empty=empty)
    except OperationError as error:
        raise OperationError("niagara_module_input_invalid", str(error)) from error


def _optional_text(value, field):
    # UE Registry treats an empty string default as required. Public null/omission
    # represents the old empty default; no other falsey/type value is coerced.
    return _text("" if value is None else value, field, empty=True)


def _path(value, field):
    path = _text(value, field)
    if not path.startswith("/") or any(char in path for char in ("\\", ":", "'", '"')):
        raise OperationError("niagara_module_path_invalid",
            field + " must be the exact already-loaded package or asset object path")
    return path


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("niagara_module_input_invalid", field + " must be an exact boolean")
    return value


def _index_json(value, field, *, minimum=-1):
    if value is not None and (type(value) is not int or not minimum <= value <= 2147483647):
        raise OperationError("niagara_module_input_invalid",
            field + " must be null or an exact int32 from " + str(minimum)
            + " through 2147483647; booleans/fractions are not indices")
    return json.dumps(value, allow_nan=False, separators=(",", ":"))


def _arguments(target_system_path, module_script_path, target_usage, target_usage_id,
        target_emitter_index, target_emitter_name, target_index, suggested_name,
        skip_if_duplicate, allow_source_edit, save, request_compile):
    return {"target_system_path": _path(target_system_path, "target_system_path"),
        "module_script_path": _path(module_script_path, "module_script_path"),
        "target_usage": _text(target_usage, "target_usage"),
        "target_usage_id": _optional_text(target_usage_id, "target_usage_id"),
        "target_emitter_index_json": _index_json(target_emitter_index, "target_emitter_index"),
        "target_emitter_name": _optional_text(target_emitter_name, "target_emitter_name"),
        "target_index_json": _index_json(target_index, "target_index", minimum=-2147483648),
        "suggested_name": _optional_text(suggested_name, "suggested_name"),
        "skip_if_duplicate": _bool(skip_if_duplicate, "skip_if_duplicate"),
        "allow_source_edit": _bool(allow_source_edit, "allow_source_edit"),
        "save": _bool(save, "save"), "request_compile": _bool(request_compile, "request_compile")}


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(operation=IDENTITY,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None,
        full_legacy_niagara_module_parity=False, emitter_attach_verified=False,
        particle_export_transport_verified=False)
    result["stage"] = native.get("stage", "native_niagara_module_operation")
    compiled = native["compiled"]
    status = "passed" if compiled is True else "failed" if compiled is False else (
        "requested_completion_unobserved" if native["compile_requested"] else "not_requested")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=compiled, status=status)
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_change = known and native.get("changed") is False and not native["mutation_attempted"]
    result["changes"] = [] if no_change else [{"kind": IDENTITY,
        "asset_path": arguments["target_system_path"], "attempted": native["mutation_attempted"],
        "changed": native.get("changed") if known else None, "change_state_known": known,
        "outcome": "native_stock_stack_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_stack_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        particle_export_transport="not_run", packaged_runtime="not_run")


def _action(result, arguments, checkpoint_callback=None):
    result["data"].update(operation=IDENTITY,
        requested_system_path=arguments["target_system_path"], adapter_preloaded_asset=False,
        adapter_load_calls=0, full_legacy_niagara_module_parity=False,
        emitter_attach_verified=False, particle_export_transport_verified=False)
    result["data"]["pending_mutation"] = {"operation": IDENTITY, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_system_stack_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_niagara_module_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": IDENTITY, "asset_path": arguments["target_system_path"],
        "attempted": None, "changed": None, "change_state_known": False,
        "outcome": "unknown_until_native_result"}]
    try:
        native = api.add_niagara_stock_module_to_stack(**arguments)
    except Exception as error:
        if getattr(error, "code", "") == "native_niagara_module_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_niagara_module_operation_failed",
            native.get("error") or "Native module insertion failed; exact partial work and stack observations retained")
    if not known_native_postcondition(native):
        raise OperationError("niagara_module_result_unverified",
            "Native success lacks known stack postconditions; do not retry insertion")
    result.update(success=True, stage="complete")
    repeated_work = (native["mutation_attempted"] or native["compile_attempted"]
        or native["save_attempted"] or native.get("changed") is True)
    result["retry"]["duplicate_risk"] = "stock_module_action_would_repeat" if repeated_work else "none_observed"


def known_native_postcondition(native):
    return native["change_state_known"] and native["postcondition_passed"]


def add_niagara_stock_module_to_stack(target_system_path, module_script_path,
        target_usage="ParticleSpawnScriptInterpolated", target_usage_id="",
        target_emitter_index=None, target_emitter_name="", target_index=None,
        suggested_name="", skip_if_duplicate=True, allow_source_edit=False,
        save=True, request_compile=True, operation_id=None):
    try:
        arguments = _arguments(target_system_path, module_script_path, target_usage,
            target_usage_id, target_emitter_index, target_emitter_name, target_index,
            suggested_name, skip_if_duplicate, allow_source_edit, save, request_compile)
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    return state.mutate(IDENTITY, arguments, operation_id, lambda result: _action(result, arguments))


def add_niagara_stock_module_to_stack_action(result, target_system_path, module_script_path,
        target_usage="ParticleSpawnScriptInterpolated", target_usage_id="",
        target_emitter_index=None, target_emitter_name="", target_index=None,
        suggested_name="", skip_if_duplicate=True, allow_source_edit=False,
        save=True, request_compile=True, checkpoint_callback=None):
    arguments = _arguments(target_system_path, module_script_path, target_usage,
        target_usage_id, target_emitter_index, target_emitter_name, target_index,
        suggested_name, skip_if_duplicate, allow_source_edit, save, request_compile)
    _action(result, arguments, checkpoint_callback)
