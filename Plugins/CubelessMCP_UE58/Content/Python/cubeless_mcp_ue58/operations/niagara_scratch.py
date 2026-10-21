"""Named Scratch Pad operations; one journal, no transport coupling.

Typed standalone value is retained. Only the reflected boundary encodes value
as JSON text. Finite HLSL attachment/rewire retain the original names; full
Position/LWC/GPU producer and destructive variants remain separate open scope.
"""
import copy
import json
import math
from typing import Any

from cubeless_mcp_ue58.compatibility.ue58 import niagara_scratch as api
from . import state, variables
from .state import OperationError

CREATE, WRITE, INSERT = api.BINDINGS[:3]
HLSL, REWIRE = api.BINDINGS[3:]


def _text(value, field, *, optional=False):
    try:
        return variables._text("" if optional and value is None else value,
            field, empty=optional, maximum=16384 if field == "default_value" else 1023)
    except OperationError as error:
        raise OperationError("niagara_scratch_invalid", str(error)) from error


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("niagara_scratch_invalid", field + " must be an exact boolean")
    return value


def _index_json(value, field):
    if value is None:
        return "null"
    if type(value) is not int or not -(2**31) <= value < 2**31:
        raise OperationError("niagara_scratch_index_invalid", field + " must be exact int32 or None")
    return str(value)


def _value_json(value):
    def check(item):
        if isinstance(item, str):
            if "\0" in item or any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise ValueError("NUL/unpaired surrogate is unsupported")
        elif isinstance(item, dict):
            if any(type(k) is not str for k in item):
                raise ValueError("JSON object keys must be strings")
            for key, child in item.items():
                check(key)
                check(child)
        elif isinstance(item, (list, tuple)):
            for child in item:
                check(child)
    try:
        check(value)
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        if len(encoded.encode("utf-16-le")) // 2 > 65536:
            raise ValueError("value exceeds 65536 UTF-16 units")
        return encoded
    except (ValueError, TypeError, RecursionError) as error:
        raise OperationError("niagara_scratch_value_json_invalid", str(error)) from error


def decode_registry_value(value):
    """The optional reflected string decodes once; raw default precedence is external."""
    if value is None:
        return None
    if type(value) is not str:
        raise OperationError("niagara_scratch_value_json_invalid", "value must be JSON text or None at the Registry boundary")
    def pairs(rows):
        result = {}
        for key, child in rows:
            if key in result:
                raise ValueError("Duplicate JSON object key")
            result[key] = child
        return result
    try:
        if len(value.encode("utf-16-le", errors="surrogatepass")) // 2 > 65536:
            raise ValueError("value exceeds 65536 UTF-16 units")
        decoded = json.loads(value, object_pairs_hook=pairs,
            parse_constant=lambda token: (_ for _ in ()).throw(ValueError("Nonfinite JSON: " + token)))
        _value_json(decoded)
        return decoded
    except (ValueError, TypeError, RecursionError) as error:
        raise OperationError("niagara_scratch_value_json_invalid", str(error)) from error


def rejected(error, operation_id=None):
    result = state.fail(state.new_result(operation_id), error)
    result["data"].update(admission_rejected=True, native_dispatched=False,
        native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False,
        full_legacy_scratch_parity=False, full_public_input_representation_parity=False,
        hlsl_producer_verified=False, source_rewiring_verified=False)
    return result


def _arguments(identity, values):
    if identity == HLSL:
        return _hlsl_arguments(values)
    if identity == REWIRE:
        return _rewire_arguments(values)
    arguments = {}
    optional = {"source_script_path", "source_system_path", "source_scratch_pad_name",
        "source_emitter_name", "target_emitter_name", "new_script_name", "default_value",
        "scratch_pad_script_path", "scratch_pad_name", "scratch_pad_emitter_name",
        "target_usage_id", "suggested_name"}
    booleans = {"allow_source_edit", "save", "request_compile", "skip_if_exists", "skip_if_duplicate"}
    for field, value in values.items():
        if field == "value":
            continue
        if field in booleans:
            arguments[field] = _bool(value, field)
        elif field.endswith("_index"):
            arguments[field + "_json"] = _index_json(value, field)
        else:
            arguments[field] = _text(value, field, optional=field in optional)
    if identity == WRITE:
        # Original Python sends only raw default when it is nonempty. Ignored
        # value need not be JSON-serializable and never changes the fingerprint.
        arguments["value_json"] = "null" if arguments["default_value"] else _value_json(values["value"])
    return arguments


def _apply_native(result, identity, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(operation=identity,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None,
        full_legacy_scratch_parity=False, full_public_input_representation_parity=False,
        registry_json_representation="Original optional value key is JSON text only at the Registry boundary; typed domain Any|None and raw-default precedence are retained",
        hlsl_producer_verified=False, source_rewiring_verified=False,
        particle_export_transport_verified=False, gpu_execution_verified=False)
    if identity == HLSL:
        result["data"]["registry_json_representation"] = "Original inputs/user_parameter_inputs and additive parameter_inputs keys are optional JSON-text lists at Registry/batch only; typed domain lists retain null versus explicit []"
    elif identity == REWIRE:
        result["data"]["registry_json_representation"] = "No JSON input representation change; optional reflected null strings normalize to legacy empty strings. timeout_seconds is retained caller context, not a native wait or applied transport timeout."
        # Native's local typed rewire proof is distinct from the deliberately
        # unproved full System Position/LWC/GPU producer scope above.
        result["data"]["source_rewiring_verified"] = copy.deepcopy(native["data"].get("source_rewiring_verified"))
    result["stage"] = native.get("stage", "native_niagara_scratch_operation")
    compiled = native["compiled"]
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=compiled, status="passed" if compiled is True else "failed" if compiled is False
        else "requested_completion_unobserved" if native["compile_requested"] else "not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_change = known and native.get("changed") is False and not native["mutation_attempted"]
    result["changes"] = [] if no_change else [{"kind": identity,
        "asset_path": arguments.get("system_path", arguments.get("target_system_path")),
        "attempted": native["mutation_attempted"], "changed": native.get("changed") if known else None,
        "change_state_known": known, "outcome": "native_scratch_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_scratch_postcondition" if native["postcondition_passed"]
        else "native_postcondition_failed", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", full_position_producer="not_run", gpu_execution="not_run", packaged_runtime="not_run")


def _action(result, identity, arguments, checkpoint_callback=None):
    result["data"].update(operation=identity, adapter_preloaded_asset=False, adapter_load_calls=0,
        full_legacy_scratch_parity=False, full_public_input_representation_parity=False,
        hlsl_producer_verified=False, source_rewiring_verified=False)
    result["data"]["pending_mutation"] = {"operation": identity, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_owned_scratch_script_graph_stack_store_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_niagara_scratch_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity,
        "asset_path": arguments.get("system_path", arguments.get("target_system_path")),
        "attempted": None, "changed": None, "change_state_known": False,
        "outcome": "unknown_until_native_result"}]
    try:
        native = getattr(api, identity)(**arguments)
    except Exception as error:
        if getattr(error, "code", "") == "native_niagara_scratch_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    result["retry"]["duplicate_risk"] = "scratch_write_compile_or_save_would_repeat" if (
        native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]
        or native.get("changed") is True) else "none_observed"
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_niagara_scratch_operation_failed",
            native.get("error") or "Scratch operation failed; exact partial work retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("niagara_scratch_result_unverified", "Native success lacks known postconditions; do not retry")
    result.update(success=True, stage="complete")


def _run(identity, operation_id, values):
    try:
        arguments = _arguments(identity, values)
    except Exception as error:
        return rejected(error, operation_id)
    return state.mutate(identity, arguments, operation_id, lambda result: _action(result, identity, arguments))


def create_or_duplicate_scratch_pad_module(target_system_path: str,
        source_script_path: str = "", source_system_path: str = "", source_owner_kind: str = "system",
        source_script_index: int | None = None, source_scratch_pad_name: str = "",
        source_emitter_index: int | None = None, source_emitter_name: str = "",
        target_owner_kind: str = "system", target_emitter_index: int | None = None,
        target_emitter_name: str = "", new_script_name: str = "",
        target_supported_usage: str = "ParticleUpdateScript", allow_source_edit: bool = False,
        save: bool = True, request_compile: bool = True, operation_id: str | None = None):
    return _run(CREATE, operation_id, {key: value for key, value in locals().items() if key != "operation_id"})


def add_niagara_scratch_pad_parameter_write(system_path: str, parameter_name: str,
        value: Any | None = None, parameter_type: str = "vec2", default_value: str = "",
        scratch_pad_script_path: str = "", scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None, scratch_pad_name: str = "",
        scratch_pad_emitter_index: int | None = None, scratch_pad_emitter_name: str = "",
        skip_if_exists: bool = True, allow_source_edit: bool = False, save: bool = True,
        request_compile: bool = True, operation_id: str | None = None):
    return _run(WRITE, operation_id, {key: value for key, value in locals().items() if key != "operation_id"})


def add_scratch_pad_module_to_stack(target_system_path: str, scratch_pad_script_path: str = "",
        scratch_pad_owner_kind: str = "system", scratch_pad_script_index: int | None = None,
        scratch_pad_name: str = "", scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "", target_usage: str = "ParticleUpdateScript",
        target_usage_id: str = "", target_emitter_index: int | None = None,
        target_emitter_name: str = "", target_index: int | None = None, suggested_name: str = "",
        skip_if_duplicate: bool = True, allow_source_edit: bool = False, save: bool = True,
        request_compile: bool = True, operation_id: str | None = None):
    return _run(INSERT, operation_id, {key: value for key, value in locals().items() if key != "operation_id"})


def create_or_duplicate_scratch_pad_module_action(result, target_system_path,
        source_script_path="", source_system_path="", source_owner_kind="system",
        source_script_index=None, source_scratch_pad_name="", source_emitter_index=None,
        source_emitter_name="", target_owner_kind="system", target_emitter_index=None,
        target_emitter_name="", new_script_name="", target_supported_usage="ParticleUpdateScript",
        allow_source_edit=False, save=True, request_compile=True, checkpoint_callback=None):
    values = {key: value for key, value in locals().items() if key not in ("result", "checkpoint_callback")}
    _action(result, CREATE, _arguments(CREATE, values), checkpoint_callback)


def add_niagara_scratch_pad_parameter_write_action(result, system_path, parameter_name,
        value=None, parameter_type="vec2", default_value="", scratch_pad_script_path="",
        scratch_pad_owner_kind="system", scratch_pad_script_index=None, scratch_pad_name="",
        scratch_pad_emitter_index=None, scratch_pad_emitter_name="", skip_if_exists=True,
        allow_source_edit=False, save=True, request_compile=True, checkpoint_callback=None):
    values = {key: child for key, child in locals().items() if key not in ("result", "checkpoint_callback")}
    if not _text(default_value, "default_value", optional=True):
        values["value"] = decode_registry_value(value)
    _action(result, WRITE, _arguments(WRITE, values), checkpoint_callback)


def add_scratch_pad_module_to_stack_action(result, target_system_path, scratch_pad_script_path="",
        scratch_pad_owner_kind="system", scratch_pad_script_index=None, scratch_pad_name="",
        scratch_pad_emitter_index=None, scratch_pad_emitter_name="", target_usage="ParticleUpdateScript",
        target_usage_id="", target_emitter_index=None, target_emitter_name="", target_index=None,
        suggested_name="", skip_if_duplicate=True, allow_source_edit=False, save=True,
        request_compile=True, checkpoint_callback=None):
    values = {key: value for key, value in locals().items() if key not in ("result", "checkpoint_callback")}
    _action(result, INSERT, _arguments(INSERT, values), checkpoint_callback)


def _input_list_json(value, field):
    if value is not None and (type(value) is not list or any(type(row) is not dict for row in value)):
        raise OperationError("niagara_scratch_input_list_invalid", field + " must be a list of JSON objects or None")
    return _value_json(value)


def decode_registry_input_list(value, field):
    """Decode only the three named HLSL list inputs; null and [] remain distinct."""
    decoded = decode_registry_value(value)
    _input_list_json(decoded, field)
    return decoded


def _hlsl_arguments(values):
    lists = {"inputs", "user_parameter_inputs", "parameter_inputs"}
    booleans = {"skip_if_already_inserted", "replace_existing_custom", "rebuild_existing_custom",
        "requires_context", "delete_unlinked_custom_nodes", "delete_unlinked_custom_input_source_nodes",
        "allow_source_edit", "save", "request_compile"}
    optional = {"scratch_pad_script_path", "scratch_pad_name", "scratch_pad_emitter_name",
        "target_node_guid", "target_node_name", "preserve_existing_link_as"}
    arguments = {}
    for field, value in values.items():
        if field in lists:
            arguments[field + "_json"] = _input_list_json(value, field)
        elif field in booleans:
            arguments[field] = _bool(value, field)
        elif field.endswith("_index"):
            arguments[field + "_json"] = _index_json(value, field)
        elif field == "expression":
            try:
                arguments[field] = variables._text(value, field, maximum=65536, multiline=True)
            except OperationError as error:
                raise OperationError("niagara_scratch_invalid", str(error)) from error
        else:
            arguments[field] = _text(value, field, optional=field in optional)
    return arguments


def _rewire_arguments(values):
    optional = {"scratch_pad_name", "custom_node_signature_name", "module_parameter_name"}
    booleans = {"delete_orphaned_source_node", "allow_source_edit", "save", "request_compile"}
    return {field: _bool(value, field) if field in booleans else _text(value, field, optional=field in optional)
        for field, value in values.items() if field != "timeout_seconds"}


def _external_timeout(value):
    if type(value) not in (int, float) or not math.isfinite(value) or value <= 0:
        raise OperationError("niagara_scratch_timeout_invalid", "timeout_seconds must be a finite positive number; no game-thread wait is performed")
    return float(value)


def _rewire_dispatch(result, arguments, timeout_seconds, checkpoint_callback=None):
    result["data"].update(requested_external_timeout_seconds=timeout_seconds,
        timeout_scope="Retained original caller context only; official MCP controls its transport timeout. Native dispatch never blocks or receives this value.",
        transport_timeout_applied=False, game_thread_wait_invoked=False)
    _action(result, REWIRE, arguments, checkpoint_callback)


def insert_niagara_scratch_pad_custom_hlsl_for_pin(system_path: str, target_pin_name: str,
        expression: str, scratch_pad_script_path: str = "", scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None, scratch_pad_name: str = "RenderGrid",
        scratch_pad_emitter_index: int | None = None, scratch_pad_emitter_name: str = "",
        target_node_guid: str = "", target_node_name: str = "", target_node_index: int | None = None,
        preserve_existing_link_as: str = "BaseValue", output_pin_name: str = "CustomHLSLOutput",
        signature_name: str = "IF_CustomPinValue", custom_hlsl_script_usage: str = "DynamicInput",
        inputs: list[dict[str, Any]] | None = None, user_parameter_inputs: list[dict[str, Any]] | None = None,
        skip_if_already_inserted: bool = True, replace_existing_custom: bool = False,
        rebuild_existing_custom: bool = False, requires_context: bool = False,
        delete_unlinked_custom_nodes: bool = False, delete_unlinked_custom_input_source_nodes: bool = False,
        allow_source_edit: bool = False, save: bool = True, request_compile: bool = True,
        parameter_inputs: list[dict[str, Any]] | None = None, operation_id: str | None = None):
    return _run(HLSL, operation_id, {key: value for key, value in locals().items() if key != "operation_id"})


def rewire_niagara_custom_hlsl_input_to_module_parameter(system_path: str, input_name: str,
        scratch_pad_name: str = "", custom_node_signature_name: str = "", module_parameter_name: str = "",
        delete_orphaned_source_node: bool = True, allow_source_edit: bool = False, save: bool = True,
        request_compile: bool = True, timeout_seconds: float = 60.0, operation_id: str | None = None):
    try:
        context = _external_timeout(timeout_seconds)
        arguments = _arguments(REWIRE, {key: value for key, value in locals().items()
            if key not in ("operation_id", "context")})
    except Exception as error:
        return rejected(error, operation_id)
    return state.mutate(REWIRE, arguments, operation_id,
        lambda result: _rewire_dispatch(result, arguments, context))


def insert_niagara_scratch_pad_custom_hlsl_for_pin_action(result, system_path, target_pin_name,
        expression, scratch_pad_script_path="", scratch_pad_owner_kind="system",
        scratch_pad_script_index=None, scratch_pad_name="RenderGrid", scratch_pad_emitter_index=None,
        scratch_pad_emitter_name="", target_node_guid="", target_node_name="", target_node_index=None,
        preserve_existing_link_as="BaseValue", output_pin_name="CustomHLSLOutput", signature_name="IF_CustomPinValue",
        custom_hlsl_script_usage="DynamicInput", inputs=None, user_parameter_inputs=None,
        skip_if_already_inserted=True, replace_existing_custom=False, rebuild_existing_custom=False,
        requires_context=False, delete_unlinked_custom_nodes=False, delete_unlinked_custom_input_source_nodes=False,
        allow_source_edit=False, save=True, request_compile=True, parameter_inputs=None, checkpoint_callback=None):
    values = {key: value for key, value in locals().items() if key not in ("result", "checkpoint_callback")}
    for field in ("inputs", "user_parameter_inputs", "parameter_inputs"):
        values[field] = decode_registry_input_list(values[field], field)
    _action(result, HLSL, _arguments(HLSL, values), checkpoint_callback)


def rewire_niagara_custom_hlsl_input_to_module_parameter_action(result, system_path, input_name,
        scratch_pad_name="", custom_node_signature_name="", module_parameter_name="",
        delete_orphaned_source_node=True, allow_source_edit=False, save=True,
        request_compile=True, timeout_seconds=60.0, checkpoint_callback=None):
    values = {key: value for key, value in locals().items() if key not in ("result", "checkpoint_callback")}
    context = _external_timeout(timeout_seconds)
    _rewire_dispatch(result, _arguments(REWIRE, values), context, checkpoint_callback)
