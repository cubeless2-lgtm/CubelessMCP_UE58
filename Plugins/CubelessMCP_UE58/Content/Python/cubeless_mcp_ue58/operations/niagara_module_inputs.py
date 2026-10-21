"""Three legacy module-input behaviors with one shared durable mutation core.

The typed domain preserves value Any/properties dict|None. Registry envelopes
keep those key names but encode their JSON as strings; full public IO parity
and every original DI/replacement variant are intentionally not claimed.
"""

import copy
import json
import math
from typing import Any

from cubeless_mcp_ue58.compatibility.ue58 import niagara_module_inputs as api
from . import state, variables
from .state import OperationError


LITERAL = "set_niagara_module_input_literal_override"
DATA_INTERFACE = "set_niagara_data_interface_module_input"
LINKED = "set_niagara_module_input_linked_parameter"
PAYLOAD_FIELDS = {LITERAL: "value_json", DATA_INTERFACE: "properties_json",
    LINKED: "linked_parameter_name"}


def _text(value, field, *, empty=False):
    try:
        return variables._text(value, field, empty=empty)
    except OperationError as error:
        raise OperationError("niagara_module_input_invalid", str(error)) from error


def _optional_text(value, field):
    return _text("" if value is None else value, field, empty=True)


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("niagara_module_input_invalid", field + " must be an exact boolean")
    return value


def _index_json(value, field):
    if value is not None and (type(value) is not int or not -2147483648 <= value <= 2147483647):
        raise OperationError("niagara_module_input_invalid", field + " must be null or an exact int32")
    # Only -1/null means unspecified in the original native selector. Other
    # negative int32 values remain supplied and produce the native no-match.
    return json.dumps(value, allow_nan=False)


def _duration(value, field):
    if type(value) not in (int, float) or not math.isfinite(value) or value <= 0:
        raise OperationError("niagara_module_input_timing_invalid", field + " must be a positive finite number")
    return value


def _json_text(value, field):
    try:
        def check_strings(item):
            if type(item) is str:
                if any(0xD800 <= ord(c) <= 0xDFFF or c == "\x00" for c in item):
                    raise ValueError("NUL/unpaired surrogate is unsupported")
            elif type(item) is dict:
                for key, child in item.items():
                    if type(key) is not str:
                        raise ValueError("JSON object keys must be strings")
                    check_strings(key)
                    check_strings(child)
            elif type(item) in (list, tuple):
                for child in item:
                    check_strings(child)

        check_strings(value)
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False,
            separators=(",", ":"))
        if len(encoded.encode("utf-16-le")) // 2 > 65536:
            raise ValueError("JSON exceeds 65536 UTF-16 units")
        return encoded
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("niagara_module_input_json_invalid", field + ": " + str(error)) from error


def decode_registry_json(value, field, *, optional_object=False):
    """Decode the finite reflected representation before domain/native entry."""
    if optional_object and value is None:
        return None
    if type(value) is not str:
        raise OperationError("niagara_module_input_json_invalid",
            field + " must be a JSON string" + (" or null/omission" if optional_object else ""))

    def unique(pairs):
        output = {}
        for key, item in pairs:
            if key in output:
                raise ValueError("duplicate JSON key: " + key)
            output[key] = item
        return output

    def invalid_constant(item):
        raise ValueError("nonfinite JSON constant: " + item)

    try:
        if len(value.encode("utf-16-le", errors="surrogatepass")) // 2 > 65536:
            raise ValueError("JSON exceeds 65536 UTF-16 units")
        decoded = json.loads(value, object_pairs_hook=unique, parse_constant=invalid_constant)
        _json_text(decoded, field)
        if optional_object and decoded is not None and type(decoded) is not dict:
            raise ValueError("properties JSON must be an object or null")
        return decoded
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("niagara_module_input_json_invalid", field + ": " + str(error)) from error


def _arguments(identity, system_path, input_name, payload, emitter_index,
        emitter_name, module_index, module_name, module_node_guid,
        overwrite_existing, allow_source_edit, save, request_compile,
        wait_for_completion, timeout_seconds, poll_interval_seconds):
    system = _text(system_path, "system_path")
    if not system.startswith("/") or any(c in system for c in ("\\", ":", "'", '"')):
        raise OperationError("niagara_module_input_path_invalid",
            "system_path must be an exact already-loaded package or asset object path")
    wait = _bool(wait_for_completion, "wait_for_completion")
    timeout = _duration(timeout_seconds, "timeout_seconds")
    poll = _duration(poll_interval_seconds, "poll_interval_seconds")
    if wait:
        raise OperationError("niagara_module_input_blocking_wait_unsupported",
            "wait_for_completion=True cannot block the official game thread; no native call or mutation dispatched. Observe readiness and save the exact target without replaying an input edit")
    if identity == DATA_INTERFACE:
        if payload is not None and (type(payload) is not dict or any(type(key) is not str for key in payload)):
            raise OperationError("niagara_module_input_json_invalid", "properties must be a dictionary with string keys or None")
        # The historical Python wrapper omitted None and {}. Both mean no
        # requested fields; native receives the same empty property map.
        native_payload = _json_text({} if payload is None else payload, "properties")
    elif identity == LITERAL:
        native_payload = _json_text(payload, "value")
    else:
        native_payload = _text(payload, "linked_parameter_name")
    return {"system_path": system, "input_name": _text(input_name, "input_name"),
        PAYLOAD_FIELDS[identity]: native_payload,
        "emitter_index_json": _index_json(emitter_index, "emitter_index"),
        "emitter_name": _optional_text(emitter_name, "emitter_name"),
        "module_index_json": _index_json(module_index, "module_index"),
        "module_name": _optional_text(module_name, "module_name"),
        "module_node_guid": _optional_text(module_node_guid, "module_node_guid"),
        "overwrite_existing": _bool(overwrite_existing, "overwrite_existing"),
        "allow_source_edit": _bool(allow_source_edit, "allow_source_edit"),
        "save": _bool(save, "save"), "request_compile": _bool(request_compile, "request_compile"),
        "timeout_seconds": timeout, "poll_interval_seconds": poll}


def _rejected(error, operation_id=None):
    result = state.fail(state.new_result(operation_id), error)
    result["data"].update(admission_rejected=True, native_dispatched=False,
        native_mutation_attempted=False, mutation_state_unknown=False,
        mutation_performed=False, full_legacy_module_input_parity=False,
        full_public_input_representation_parity=False, blocking_wait_supported=False)
    return result


def registry_json_failure(error, operation_id=None):
    return _rejected(error, operation_id)


def _apply_native(result, identity, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(operation=identity,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None,
        full_legacy_module_input_parity=False, full_public_input_representation_parity=False,
        registry_json_representation="Original value/properties keys use JSON strings only at the reflected Registry boundary; typed domain Any/dict is retained",
        blocking_wait_supported=False, timeout_seconds=arguments["timeout_seconds"],
        poll_interval_seconds=arguments["poll_interval_seconds"],
        timing_scope="Caller readiness context only; no game-thread polling/wait/flush",
        particle_export_transport_verified=False, gpu_execution_verified=False)
    result["stage"] = native.get("stage", "native_niagara_module_input_operation")
    compiled = native["compiled"]
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=compiled, status="passed" if compiled is True else "failed" if compiled is False
        else "requested_completion_unobserved" if native["compile_requested"] else "not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_change = known and native.get("changed") is False and not native["mutation_attempted"]
    result["changes"] = [] if no_change else [{"kind": identity,
        "asset_path": arguments["system_path"], "attempted": native["mutation_attempted"],
        "changed": native.get("changed") if known else None, "change_state_known": known,
        "outcome": "native_module_input_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_module_input_postcondition" if native["postcondition_passed"]
        else "native_postcondition_failed", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", particle_export_transport="not_run", gpu_execution="not_run", packaged_runtime="not_run")


def _action(result, identity, arguments, checkpoint_callback=None):
    result["data"].update(operation=identity, requested_system_path=arguments["system_path"],
        adapter_preloaded_asset=False, adapter_load_calls=0, full_legacy_module_input_parity=False,
        full_public_input_representation_parity=False, blocking_wait_supported=False,
        particle_export_transport_verified=False)
    result["data"]["pending_mutation"] = {"operation": identity, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_selected_input_graph_DI_store_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_niagara_module_input_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "asset_path": arguments["system_path"],
        "attempted": None, "changed": None, "change_state_known": False,
        "outcome": "unknown_until_native_result"}]
    try:
        native = getattr(api, identity)(**{key: value for key, value in arguments.items()
            if key not in ("timeout_seconds", "poll_interval_seconds")})
    except Exception as error:
        if getattr(error, "code", "") == "native_niagara_module_input_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    result["retry"]["duplicate_risk"] = "input_mutation_compile_or_save_would_repeat" if (
        native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]
        or native.get("changed") is True) else "none_observed"
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_niagara_module_input_operation_failed",
            native.get("error") or "Module input operation failed; exact partial work is retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("niagara_module_input_result_unverified",
            "Native success lacks known input postconditions; inspect actual state, do not retry")
    result.update(success=True, stage="complete")


def _run(identity, operation_id, *parameters):
    try:
        arguments = _arguments(identity, *parameters)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(identity, arguments, operation_id,
        lambda result: _action(result, identity, arguments))


def set_niagara_module_input_literal_override(system_path: str, input_name: str, value: Any,
        emitter_index=None, emitter_name="", module_index=None, module_name="",
        module_node_guid="", overwrite_existing=False, allow_source_edit=False,
        save=True, request_compile=True, wait_for_completion=False,
        timeout_seconds=10.0, poll_interval_seconds=0.1, operation_id=None):
    return _run(LITERAL, operation_id, system_path, input_name, value, emitter_index,
        emitter_name, module_index, module_name, module_node_guid, overwrite_existing,
        allow_source_edit, save, request_compile, wait_for_completion, timeout_seconds,
        poll_interval_seconds)


def set_niagara_data_interface_module_input(system_path: str, input_name: str,
        properties: dict[str, Any] | None = None, emitter_index=None, emitter_name="",
        module_index=None, module_name="", module_node_guid="", overwrite_existing=False,
        allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False,
        timeout_seconds=10.0, poll_interval_seconds=0.1, operation_id=None):
    return _run(DATA_INTERFACE, operation_id, system_path, input_name, properties,
        emitter_index, emitter_name, module_index, module_name, module_node_guid,
        overwrite_existing, allow_source_edit, save, request_compile, wait_for_completion,
        timeout_seconds, poll_interval_seconds)


def set_niagara_module_input_linked_parameter(system_path: str, input_name: str,
        linked_parameter_name: str, emitter_index=None, emitter_name="", module_index=None,
        module_name="", module_node_guid="", overwrite_existing=False, allow_source_edit=False,
        save=True, request_compile=True, wait_for_completion=False,
        timeout_seconds=10.0, poll_interval_seconds=0.1, operation_id=None):
    return _run(LINKED, operation_id, system_path, input_name, linked_parameter_name,
        emitter_index, emitter_name, module_index, module_name, module_node_guid,
        overwrite_existing, allow_source_edit, save, request_compile, wait_for_completion,
        timeout_seconds, poll_interval_seconds)


def set_niagara_module_input_literal_override_action(result, system_path, input_name, value,
        emitter_index=None, emitter_name="", module_index=None, module_name="",
        module_node_guid="", overwrite_existing=False, allow_source_edit=False,
        save=True, request_compile=True, wait_for_completion=False,
        timeout_seconds=10.0, poll_interval_seconds=0.1, checkpoint_callback=None):
    """Batch child uses reflected value JSON text; no independent journal."""
    typed_value = decode_registry_json(value, "value")
    arguments = _arguments(LITERAL, system_path, input_name, typed_value, emitter_index,
        emitter_name, module_index, module_name, module_node_guid, overwrite_existing,
        allow_source_edit, save, request_compile, wait_for_completion, timeout_seconds,
        poll_interval_seconds)
    _action(result, LITERAL, arguments, checkpoint_callback)


def set_niagara_data_interface_module_input_action(result, system_path, input_name,
        properties=None, emitter_index=None, emitter_name="", module_index=None,
        module_name="", module_node_guid="", overwrite_existing=False,
        allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False,
        timeout_seconds=10.0, poll_interval_seconds=0.1, checkpoint_callback=None):
    """Batch child uses reflected properties JSON text/null; no own journal."""
    typed_properties = decode_registry_json(properties, "properties", optional_object=True)
    arguments = _arguments(DATA_INTERFACE, system_path, input_name, typed_properties,
        emitter_index, emitter_name, module_index, module_name, module_node_guid,
        overwrite_existing, allow_source_edit, save, request_compile, wait_for_completion,
        timeout_seconds, poll_interval_seconds)
    _action(result, DATA_INTERFACE, arguments, checkpoint_callback)


def set_niagara_module_input_linked_parameter_action(result, system_path, input_name,
        linked_parameter_name, emitter_index=None, emitter_name="", module_index=None,
        module_name="", module_node_guid="", overwrite_existing=False,
        allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False,
        timeout_seconds=10.0, poll_interval_seconds=0.1, checkpoint_callback=None):
    arguments = _arguments(LINKED, system_path, input_name, linked_parameter_name,
        emitter_index, emitter_name, module_index, module_name, module_node_guid,
        overwrite_existing, allow_source_edit, save, request_compile, wait_for_completion,
        timeout_seconds, poll_interval_seconds)
    _action(result, LINKED, arguments, checkpoint_callback)
