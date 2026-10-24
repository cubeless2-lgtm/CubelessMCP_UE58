"""Original Actor Blueprint networking inspection and strict nullable authoring.

Native owns CDO resolution, original validation, changed-field setters, compiler,
reacquisition/readback, dependency audits and forced-save receipts. Python retains
returned complete raw/outcome prefixes before any failure; it never repeats work.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_networking as api
from . import state
from .state import OperationError

INSPECT, SET = api.BINDINGS
_PUBLIC_FIELDS = {
    INSPECT: ("blueprint_name",),
    SET: ("blueprint_name", "replicates", "replicate_movement", "net_dormancy", "always_relevant",
        "only_relevant_to_owner", "net_update_frequency", "min_net_update_frequency", "net_priority",
        "net_cull_distance_squared", "dry_run"),
}
_OPTIONAL_BOOLEANS = ("replicates", "replicate_movement", "always_relevant", "only_relevant_to_owner")
_OPTIONAL_NUMBERS = ("net_update_frequency", "min_net_update_frequency", "net_priority", "net_cull_distance_squared")
_FORBIDDEN_REFERENCE_TOKENS = ("/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("invalid_networking_request", "Only this original method's exact fields are admitted")
    if type(public["blueprint_name"]) is not str:
        raise OperationError("invalid_networking_request", "blueprint_name requires an exact JSON string")
    candidate = public["blueprint_name"].strip().strip("\"'").replace("\\", "/").lower()
    if any(token in candidate for token in _FORBIDDEN_REFERENCE_TOKENS):
        raise OperationError("invalid_networking_request", "Refusing to persist MCP-only reference in 'blueprint_name': "
            + public["blueprint_name"] + ". UnrealMCP is an editor authoring bridge; finished assets must not depend on it.")
    hidden = {"blueprint_name": public["blueprint_name"]}
    if method == SET:
        if type(public["dry_run"]) is not bool:
            raise OperationError("invalid_networking_request", "dry_run requires an exact JSON boolean")
        for field in _PUBLIC_FIELDS[SET][1:-1]:
            value = public[field]
            required = (bool,) if field in _OPTIONAL_BOOLEANS else (int, float) if field in _OPTIONAL_NUMBERS else (str,)
            if value is not None and type(value) not in required:
                raise OperationError("invalid_networking_request", "Original strict nullable JSON field type differs: " + field)
            # Original Python omits None values. Empty internal text preserves that
            # omission; bare native JSON null remains a supplied invalid value.
            encoded = "" if value is None else json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
            try:
                encoded.encode("utf-8")
            except UnicodeError as error:
                raise OperationError("invalid_networking_request", "Bounded native JSON carrier needs valid Unicode") from error
            hidden[field + "_json"] = encoded
        hidden["dry_run"] = public["dry_run"]
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]), read_only=arguments["method"] == INSPECT,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        original_none_fields_omitted_native_empty_string_sentinel=not arguments.get("raw_input", False),
        original_raw_networking_params_delegated=arguments.get("raw_input", False),
        original_dependency_guard_tokens_and_normalization_preserved=True,
        native_CDO_validation_setter_compile_readback_dependency_and_save_authoritative=True,
        no_added_public_compile_or_save_options=True, implicit_compile_save_decision_native_owned=True,
        inspection_load_and_CDO_effects_not_total_side_effect_free=True,
        requested_range_dormancy_relevancy_and_matching_rules_not_reimplemented_in_Python=True,
        full_original_multiplayer_cold_or_external_pipeline_verified=False)


def _apply_native(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_raw_result=native["_raw_native_json"],
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_not_attempted" if not native["compile_attempted"] else "native_returned_compiled" if native["compiled"] is True else "native_returned_compile_failed" if native["compiled"] is False else "native_returned_compile_unverified")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"].get("blueprint_name"),
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_blueprint_networking_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", multiplayer="not_run", native_dependency_audit="actual_native_prefix",
        CDO_reacquisition_readback="actual_native_prefix", total_editor_side_effects="not_verified")
    result["stage"] = "returned_blueprint_networking_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_decision_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_decision_not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_networking_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"].get("blueprint_name"),
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments.get("native_method", arguments["method"]))(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_blueprint_networking_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_networking_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_networking_native_failed", native.get("error") or "Native actual CDO/compiler/save/audit prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_networking_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_current_CDO_settings_compile_save_audit_receipts_and_journal_no_accepted_writer_replay"


def _raw_input_action(result, method, params, checkpoint_callback=None):
    """Exact original Batch raw2; Native resolves before validating settings.

    In particular, supplied null is not the Named API's None/omission, and the
    raw dry_run is never cast to bool. Unknown fields remain Native-owned.
    """
    if method not in (INSPECT, SET) or type(params) is not dict:
        raise OperationError("batch_networking_raw_input_invalid", "Only the two original networking commands with object params are admitted")
    public = copy.deepcopy(params)
    encoded = json.dumps(public, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    arguments = {"method": method, "public": public, "hidden": {"params_json": encoded},
        "native_method": method + "_raw_input", "raw_input": True}
    _run_action(result, arguments, checkpoint_callback)


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def inspect_blueprint_networking_settings(blueprint_name):
    try:
        arguments = _arguments(INSPECT, {"blueprint_name": blueprint_name})
    except Exception as error:
        return _rejected(INSPECT, error)
    result = state.new_result()
    try:
        _run_action(result, arguments, checkpoint_callback=lambda observed: None)
        return result
    except Exception as error:
        return state.fail(result, error)


def inspect_blueprint_networking_settings_action(result, blueprint_name, checkpoint_callback=None):
    _run_action(result, _arguments(INSPECT, {"blueprint_name": blueprint_name}), checkpoint_callback)


def set_blueprint_networking_settings(blueprint_name, replicates=None, replicate_movement=None, net_dormancy=None,
        always_relevant=None, only_relevant_to_owner=None, net_update_frequency=None, min_net_update_frequency=None,
        net_priority=None, net_cull_distance_squared=None, dry_run=True):
    try:
        arguments = _arguments(SET, dict(blueprint_name=blueprint_name, replicates=replicates,
            replicate_movement=replicate_movement, net_dormancy=net_dormancy, always_relevant=always_relevant,
            only_relevant_to_owner=only_relevant_to_owner, net_update_frequency=net_update_frequency,
            min_net_update_frequency=min_net_update_frequency, net_priority=net_priority,
            net_cull_distance_squared=net_cull_distance_squared, dry_run=dry_run))
    except Exception as error:
        return _rejected(SET, error)
    return state.mutate(SET, arguments, None, lambda result: _run_action(result, arguments))


def set_blueprint_networking_settings_action(result, blueprint_name, replicates=None, replicate_movement=None,
        net_dormancy=None, always_relevant=None, only_relevant_to_owner=None, net_update_frequency=None,
        min_net_update_frequency=None, net_priority=None, net_cull_distance_squared=None, dry_run=True,
        checkpoint_callback=None):
    arguments = _arguments(SET, dict(blueprint_name=blueprint_name, replicates=replicates,
        replicate_movement=replicate_movement, net_dormancy=net_dormancy, always_relevant=always_relevant,
        only_relevant_to_owner=only_relevant_to_owner, net_update_frequency=net_update_frequency,
        min_net_update_frequency=min_net_update_frequency, net_priority=net_priority,
        net_cull_distance_squared=net_cull_distance_squared, dry_run=dry_run))
    _run_action(result, arguments, checkpoint_callback)
