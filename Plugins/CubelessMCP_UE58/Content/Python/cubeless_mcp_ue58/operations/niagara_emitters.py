"""Original emitter attach selectors with one durable mutation journal."""

import copy
import json

from cubeless_mcp_ue58.compatibility.ue58 import niagara_emitters as api
from . import state, variables
from .state import OperationError


IDENTITY = "duplicate_or_attach_emitter_from_source"


def _text(value, field, *, empty=False):
    try:
        return variables._text(value, field, empty=empty)
    except OperationError as error:
        raise OperationError("niagara_emitter_input_invalid", str(error)) from error


def _optional_text(value, field):
    # Registry null represents the historical empty string default. Other
    # falsey/type inputs are retained or rejected, never silently coerced.
    return _text("" if value is None else value, field, empty=True)


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("niagara_emitter_input_invalid", field + " must be an exact boolean")
    return value


def _arguments(target_system_path, source_emitter_path, source_system_path,
        source_emitter_index, source_emitter_name, new_emitter_name, enabled,
        copy_source_user_parameters, allow_source_edit, save, request_compile):
    target = _text(target_system_path, "target_system_path")
    if not target.startswith("/") or any(c in target for c in ("\\", ":", "'", '"')):
        raise OperationError("niagara_emitter_path_invalid",
            "target_system_path must be the exact already-loaded package or asset object path")
    emitter = _optional_text(source_emitter_path, "source_emitter_path")
    system = _optional_text(source_system_path, "source_system_path")
    if not emitter and not system:
        raise OperationError("niagara_emitter_source_path_invalid",
            "Provide source_emitter_path or source_system_path; an emitter asset takes precedence")
    if source_emitter_index is not None and (type(source_emitter_index) is not int
            or not -1 <= source_emitter_index <= 2147483647):
        raise OperationError("niagara_emitter_input_invalid",
            "source_emitter_index must be null, -1 or a nonnegative exact int32")
    if enabled is not None and type(enabled) is not bool:
        raise OperationError("niagara_emitter_enabled_invalid",
            "enabled must be null (omit override) or an exact boolean")
    copy_user = _bool(copy_source_user_parameters, "copy_source_user_parameters")
    return {"target_system_path": target, "source_emitter_path": emitter,
        "source_system_path": system,
        "source_emitter_index_json": json.dumps(source_emitter_index, allow_nan=False),
        "source_emitter_name": _optional_text(source_emitter_name, "source_emitter_name"),
        "new_emitter_name": _optional_text(new_emitter_name, "new_emitter_name"),
        "enabled_json": json.dumps(enabled, allow_nan=False),
        "copy_source_user_parameters": copy_user,
        "allow_source_edit": _bool(allow_source_edit, "allow_source_edit"),
        "save": _bool(save, "save"), "request_compile": _bool(request_compile, "request_compile")}


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(operation=IDENTITY,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None,
        full_legacy_emitter_attach_parity=False,
        source_user_copy_verified=bool(arguments["copy_source_user_parameters"] and known
            and native["success"] and native["postcondition_passed"]
            and native["data"].get("user_copy_selected_prefix_verified") is True),
        particle_export_transport_verified=False)
    result["stage"] = native.get("stage", "native_niagara_emitter_operation")
    compiled = native["compiled"]
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=compiled, status="passed" if compiled is True else "failed" if compiled is False
        else "requested_completion_unobserved" if native["compile_requested"] else "not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_change = known and native.get("changed") is False and not native["mutation_attempted"]
    result["changes"] = [] if no_change else [{"kind": IDENTITY,
        "asset_path": arguments["target_system_path"], "attempted": native["mutation_attempted"],
        "changed": native.get("changed") if known else None, "change_state_known": known,
        "outcome": "native_emitter_attach_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_emitter_postcondition" if native["postcondition_passed"]
        else "native_postcondition_failed", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", particle_export_transport="not_run", packaged_runtime="not_run")


def _action(result, arguments, checkpoint_callback=None):
    result["data"].update(operation=IDENTITY,
        requested_system_path=arguments["target_system_path"], adapter_preloaded_asset=False,
        adapter_load_calls=0, full_legacy_emitter_attach_parity=False,
        source_user_copy_verified=False, particle_export_transport_verified=False)
    result["data"]["pending_mutation"] = {"operation": IDENTITY, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_source_target_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_niagara_emitter_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": IDENTITY, "asset_path": arguments["target_system_path"],
        "attempted": None, "changed": None, "change_state_known": False,
        "outcome": "unknown_until_native_result"}]
    try:
        native = api.duplicate_or_attach_emitter_from_source(**arguments)
    except Exception as error:
        if getattr(error, "code", "") == "native_niagara_emitter_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    result["retry"]["duplicate_risk"] = "emitter_attachment_would_repeat" if (
        native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]
        or native.get("changed") is True) else "none_observed"
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_niagara_emitter_operation_failed",
            native.get("error") or "Emitter attachment failed; exact partial work is retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("niagara_emitter_result_unverified",
            "Native success lacks known emitter postconditions; do not retry attachment")
    result.update(success=True, stage="complete")


def duplicate_or_attach_emitter_from_source(target_system_path, source_emitter_path="",
        source_system_path="", source_emitter_index=None, source_emitter_name="",
        new_emitter_name="", enabled=None, copy_source_user_parameters=False,
        allow_source_edit=False, save=True, request_compile=True, operation_id=None):
    try:
        arguments = _arguments(target_system_path, source_emitter_path, source_system_path,
            source_emitter_index, source_emitter_name, new_emitter_name, enabled,
            copy_source_user_parameters, allow_source_edit, save, request_compile)
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    return state.mutate(IDENTITY, arguments, operation_id, lambda result: _action(result, arguments))


def duplicate_or_attach_emitter_from_source_action(result, target_system_path,
        source_emitter_path="", source_system_path="", source_emitter_index=None,
        source_emitter_name="", new_emitter_name="", enabled=None,
        copy_source_user_parameters=False, allow_source_edit=False, save=True,
        request_compile=True, checkpoint_callback=None):
    arguments = _arguments(target_system_path, source_emitter_path, source_system_path,
        source_emitter_index, source_emitter_name, new_emitter_name, enabled,
        copy_source_user_parameters, allow_source_edit, save, request_compile)
    _action(result, arguments, checkpoint_callback)
