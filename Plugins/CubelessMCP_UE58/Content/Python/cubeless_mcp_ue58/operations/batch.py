"""Explicit Blueprint/graph/component capabilities, one root journal, no nested mutation wrappers.

This does not implement arbitrary legacy commands or specialized domain batches.
All admission checks precede execution; asset-state guards run at each step so an
earlier create can supply the asset for a later compile. Never retries an action.
"""

import copy
import json
import math
import time

from cubeless_mcp_ue58.compatibility.ue58 import batch as engine
from cubeless_mcp_ue58.compatibility.ue58.blueprint import ensure_editor_idle
from . import blueprint, graph, component, variables, graph_variables, functions, blueprint_duplicates, events, pin_defaults
from .state import OperationError, checkpoint, fail, mutate, new_result

# Keep the v1 response/admission contract for existing Blueprint/graph callers;
# the additive component allowlist is identified separately in each response.
MANIFEST_VERSION = "blueprint-graph-v1"
MAX_COMMANDS = 100
MAX_INPUT_BYTES = 1024 * 1024
MAX_OBSERVED_RESULT_BYTES = 8 * 1024 * 1024
BETWEEN_STEP_BUDGET_SECONDS = 15.0
_REQUIRED = object()


def _entry(action, effect, fields, fixed=None):
    return {"action": action, "effect": effect, "fields": fields, "fixed": fixed or {}}


# Each field: (exact accepted Python types, default or _REQUIRED). bool is not int.
_S = ((str,), _REQUIRED)
_I = ((int,), _REQUIRED)
_GRAPH = ((str,), "EventGraph")
_CAPABILITIES = {
    "CubelessBlueprintGraphTools.add_blueprint_event_node": _entry(events.add_blueprint_event_node_action, "write", {
        "blueprint_name": _S, "event_name": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_parent_function_call": _entry(events.add_blueprint_parent_function_call_action, "write", {
        "blueprint_name": _S, "event_node_id": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintTools.duplicate_blueprint_to_temp": _entry(blueprint_duplicates.duplicate_blueprint_to_temp_action, "write", {
        "blueprint_name": _S, "target_path": _S, "required_root": ((str,), "/Game/_MCP_Temp"),
        "dry_run": ((bool,), True), "overwrite_existing": ((bool,), False), "allow_dirty_source": ((bool,), False),
        "compile": ((bool,), False), "save": ((bool,), True)}),
    "CubelessBlueprintGraphTools.resolve_blueprint_graph": _entry(functions.resolve_blueprint_graph_action, "write", {
        "blueprint_name": _S, "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_function_parameter": _entry(functions.add_blueprint_function_parameter_action, "write", {
        "blueprint_name": _S, "parameter_name": _S, "parameter_type": _S, "direction": ((str,), "input"),
        "default_value_json": ((str, type(None)), None), "type_object": ((str, type(None)), None), "is_array": ((bool,), False),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str,), "function"),
        "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_call_function_node": _entry(functions.add_blueprint_call_function_node_action, "write", {
        "blueprint_name": _S, "function_class": _S, "function_name": _S, "param_defaults_json": ((str,), "{}"),
        "node_position_json": ((str,), "[0,0]"), "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False),
        "allow_latent": ((bool,), False), "allow_editor_only": ((bool,), False), "allow_wildcard": ((bool,), False),
        "allow_deprecated": ((bool,), False), "allow_internal": ((bool,), False)}),
    "CubelessBlueprintTools.add_blueprint_variable": _entry(variables.add_blueprint_variable_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "variable_type": _S, "is_exposed": ((bool,), False),
        "default_value_json": ((str, type(None)), None), "category": ((str, type(None)), None),
        "tooltip": ((str, type(None)), None), "friendly_name": ((str, type(None)), None),
        "type_object": ((str, type(None)), None), "is_array": ((bool,), False), "metadata_json": ((str,), "{}"),
        "variable_guid": ((str, type(None)), None), "is_editable": ((bool, type(None)), None)}),
    "CubelessBlueprintTools.inspect_blueprint_variables": _entry(variables.inspect_blueprint_variables_action, "read_load", {"blueprint_name": _S}),
    "CubelessBlueprintTools.inspect_blueprint_variable_metadata": _entry(variables.inspect_blueprint_variable_metadata_action, "read_load", {
        "blueprint_name": _S, "variable_name": _S}),
    "CubelessBlueprintTools.set_blueprint_variable_default": _entry(variables.set_blueprint_variable_default_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "default_value_json": _S,
        "compile": ((bool,), True), "save": ((bool,), True), "dry_run": ((bool,), False)}),
    "CubelessBlueprintTools.create_blueprint": _entry(blueprint.create_blueprint_action, "write", {
        "name": _S, "parent_class": _S, "package_path": ((str,), "/Game/Blueprints")}),
    "CubelessBlueprintTools.inspect_blueprint": _entry(blueprint.inspect_blueprint_action, "read_load", {"blueprint_name": _S}),
    "CubelessBlueprintTools.compile_blueprint": _entry(blueprint.compile_blueprint_action, "write", {"blueprint_name": _S}, {"save": False, "refresh_nodes": False}),
    "CubelessBlueprintTools.compile_and_save_blueprint": _entry(blueprint.compile_blueprint_action, "write", {
        "blueprint_name": _S, "save": ((bool,), True)}, {"refresh_nodes": False}),
    "CubelessBlueprintTools.compile_and_validate_blueprint": _entry(blueprint.compile_blueprint_action, "write", {
        "blueprint_name": _S, "save": ((bool,), False), "refresh_nodes": ((bool,), True)}),
    "CubelessBlueprintGraphTools.inspect_graph": _entry(graph.inspect_graph_action, "read_load", {"blueprint_name": _S, "graph_name": _GRAPH}),
    "CubelessBlueprintGraphTools.add_blueprint_variable_get_node": _entry(graph_variables.add_blueprint_variable_get_node_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "graph_name": _GRAPH, "x": ((int,), 0), "y": ((int,), 0),
        "target_class": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None),
        "create_graph_if_missing": ((bool,), False), "node_position_json": ((str, type(None)), None)}),
    "CubelessBlueprintGraphTools.add_blueprint_variable_set_node": _entry(graph_variables.add_blueprint_variable_set_node_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "graph_name": _GRAPH, "x": ((int,), 0), "y": ((int,), 0),
        "target_class": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None),
        "create_graph_if_missing": ((bool,), False), "node_position_json": ((str, type(None)), None)}),
    "CubelessBlueprintGraphTools.set_blueprint_pin_default": _entry(pin_defaults.set_blueprint_pin_default_action, "write", {
        "blueprint_name": _S, "node_id": _S, "pin_name": _S, "value_json": _S, "direction": ((str, type(None)), None),
        "graph_name": _GRAPH, "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None)}),
    "CubelessBlueprintGraphTools.add_branch_node": _entry(graph.add_branch_node_action, "write", {
        "blueprint_name": _S, "graph_name": _GRAPH, "x": ((int,), 0), "y": ((int,), 0)}),
    "CubelessBlueprintGraphTools.add_blueprint_sequence_node": _entry(graph.add_blueprint_sequence_node_action, "write", {
        "blueprint_name": _S, "node_position_json": ((str,), "[0,0]"), "output_count": ((int,), 2),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.set_node_position": _entry(graph.set_node_position_action, "write", {
        "blueprint_name": _S, "node_id": _S, "x": _I, "y": _I, "graph_name": _GRAPH,
        "expected_x": ((int, type(None)), None), "expected_y": ((int, type(None)), None)}),
    "CubelessBlueprintGraphTools.delete_node": _entry(graph.delete_node_action, "write", {
        "blueprint_name": _S, "node_id": _S, "graph_name": _GRAPH, "allow_exec_linked_delete": ((bool,), False)}),
    "CubelessBlueprintGraphTools.connect_graph_pins": _entry(graph.connect_graph_pins_action, "write", {
        "blueprint_name": _S, "source_node_id": _S, "source_pin_name": _S,
        "target_node_id": _S, "target_pin_name": _S, "graph_name": _GRAPH}),
    "CubelessBlueprintGraphTools.connect_blueprint_graph_pins": _entry(graph.connect_blueprint_graph_pins_action, "write", {
        "blueprint_name": _S, "source_node_id": _S, "source_pin": _S, "target_node_id": _S, "target_pin": _S,
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None),
        "allow_pin_link_replacement": ((bool,), False), "compile": ((bool,), False), "save_asset": ((bool,), False),
        "expected_existing_source_link_count": ((int, type(None)), None),
        "expected_existing_source_link_target_node_id": ((str, type(None)), None),
        "expected_existing_source_link_target_pin": ((str, type(None)), None), "expected_schema_response": ((str, type(None)), None)}),
    "CubelessComponentTools.list_blueprint_components": _entry(component.list_blueprint_components_action, "read_load", {
        "blueprint_name": _S, "component_name": ((str, type(None)), None), "max_components": ((int,), 10000)}),
    "CubelessComponentTools.get_component_property": _entry(component.get_component_property_action, "read_load", {
        "blueprint_name": _S, "component_name": _S, "property_name": _S,
        "max_components": ((int,), 10000), "max_value_chars": ((int,), 65536)}),
    "CubelessComponentTools.add_component_to_blueprint": _entry(component.add_component_to_blueprint_action, "write", {
        "blueprint_name": _S, "component_type": _S, "component_name": _S,
        "location_json": ((str,), "[0,0,0]"), "rotation_json": ((str,), "[0,0,0]"), "scale_json": ((str,), "[1,1,1]"),
        "parent_component": ((str, type(None)), None), "component_properties_json": ((str,), "{}"), "compile_blueprint": ((bool,), True)}),
    "CubelessComponentTools.set_component_property": _entry(component.set_component_property_action, "write", {
        "blueprint_name": _S, "component_name": _S, "property_name": _S, "property_value_json": _S}),
    "CubelessComponentTools.set_static_mesh_properties": _entry(component.set_static_mesh_properties_action, "write", {
        "blueprint_name": _S, "component_name": _S, "static_mesh": ((str,), "/Engine/BasicShapes/Cube.Cube"), "material": ((str, type(None)), None)}),
    "CubelessComponentTools.set_physics_properties": _entry(component.set_physics_properties_action, "write", {
        "blueprint_name": _S, "component_name": _S, "simulate_physics": ((bool,), True), "gravity_enabled": ((bool,), True),
        "mass": ((int, float), 1.0), "linear_damping": ((int, float), 0.01), "angular_damping": ((int, float), 0.0)}),
}


def _unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise OperationError("batch_duplicate_key", "Duplicate JSON key: " + key)
        result[key] = value
    return result


def _bad_constant(value):
    raise OperationError("batch_invalid_json", "Non-finite JSON constant: " + value)


def _normalize(commands_json, stop_on_error):
    if type(stop_on_error) is not bool:
        raise OperationError("batch_invalid_stop_on_error", "stop_on_error must be a boolean")
    if type(commands_json) is not str:
        raise OperationError("batch_invalid_json", "commands_json must be a JSON string")
    if len(commands_json.encode("utf-8")) > MAX_INPUT_BYTES:
        raise OperationError("batch_input_too_large", "commands_json exceeds 1 MiB")
    try:
        commands = json.loads(commands_json, object_pairs_hook=_unique_keys, parse_constant=_bad_constant)
    except (ValueError, RecursionError) as error:
        raise OperationError("batch_invalid_json", str(error)) from error
    if type(commands) is not list or not commands:
        raise OperationError("batch_invalid_commands", "commands must be a nonempty JSON array")
    if len(commands) > MAX_COMMANDS:
        raise OperationError("batch_too_large", "commands must contain at most 100 entries")
    normalized = []
    for index, command in enumerate(commands):
        if type(command) is not dict or set(command) - {"type", "params"}:
            raise OperationError("batch_invalid_entry", f"commands[{index}] must contain only type and optional params")
        name = command.get("type")
        if name in ("batch", "execute_batch", "CubelessBatchTools.execute_batch"):
            raise OperationError("batch_nested", f"commands[{index}]: nested batches are not supported")
        if type(name) is not str or not name:
            raise OperationError("batch_invalid_entry", f"commands[{index}].type must be a nonempty string")
        if name not in _CAPABILITIES:
            raise OperationError("batch_unsupported_command", f"commands[{index}]: unsupported command {name}; no action executed")
        params = command.get("params")
        if params is None:
            params = {}
        fields = _CAPABILITIES[name]["fields"]
        if type(params) is not dict:
            raise OperationError("batch_invalid_entry_params", f"commands[{index}].params must be an object or null")
        unexpected = set(params) - set(fields)
        if unexpected:
            raise OperationError("batch_unknown_parameter", f"commands[{index}]: unsupported parameters {sorted(unexpected)}; child operation_id is forbidden")
        values = {}
        for key, (types, default) in fields.items():
            value = params.get(key, default)
            if value is _REQUIRED:
                raise OperationError("batch_missing_parameter", f"commands[{index}].params.{key} is required")
            if type(value) not in types:
                raise OperationError("batch_invalid_parameter_type", f"commands[{index}].params.{key} has the wrong type")
            if name == "CubelessComponentTools.set_static_mesh_properties" and key == "material" and value is None:
                # Keep pre-R mesh-only batch fingerprints unchanged. Explicit
                # null and omission both mean no material assignment.
                continue
            if name in ("CubelessBlueprintGraphTools.add_blueprint_variable_get_node", "CubelessBlueprintGraphTools.add_blueprint_variable_set_node"):
                # AA additive options must not invalidate pre-AA batch replay.
                if key in ("target_class", "graph_id", "graph_type") and value in (None, ""):
                    continue
                if key == "node_position_json" and value is None or key == "create_graph_if_missing" and value is False:
                    continue
            if float in types:
                try:
                    value = float(value)
                except OverflowError as error:
                    raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must be finite") from error
                if not math.isfinite(value):
                    raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must be finite")
            elif type(value) is int and not -(2 ** 31) <= value < 2 ** 31:
                raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must fit a reflected int32")
            values[key] = value
        normalized.append({"type": name, "params": values})
    return normalized


def _summary(commands, stop_on_error):
    return {"manifest_version": MANIFEST_VERSION, "capability_extension": "local-scs-component-v1", "variable_capability_extension": "declared-variable-state-v1", "success": False, "total": len(commands),
            "executed": 0, "dispatched_count": 0, "succeeded": 0, "failed": 0,
            "stopped_at_index": -1, "stop_on_error": stop_on_error, "results": [],
            "not_run": list(range(len(commands))), "forced_stop": False, "stop_reason": "",
            "mutation_attempted": False, "mutation_performed": False,
            "mutation_state_unknown": False, "partial_mutation": False,
            "command_may_still_be_in_flight": False, "do_not_retry": False,
            "transaction": {"scope_requested": any(_CAPABILITIES[c["type"]]["effect"] == "write" for c in commands),
                            "atomic_rollback": False, "group_undo_verified": False, "disk_save_rollback": False},
            "limits": {"max_commands": MAX_COMMANDS, "input_bytes": MAX_INPUT_BYTES,
                       "observed_result_bytes_stop_threshold": MAX_OBSERVED_RESULT_BYTES,
                       "between_step_budget_seconds": BETWEEN_STEP_BUDGET_SECONDS,
                       "engine_call_preemption": False}, "checkpoint_sequence": 0}


def _observation(row):
    child = row["response"]
    # Component helpers distinguish entering their guarded native operation
    # from actually editing an engine object (preflight rejection / no-op).
    if type(child["data"].get("native_mutation_attempted")) is bool:
        row["mutation_attempted"] = child["data"]["native_mutation_attempted"]
    attempted = row["mutation_attempted"]
    known_native = type(child["data"].get("mutation_performed")) is bool
    performed = child["data"].get("mutation_performed") if known_native else bool(attempted) if child["success"] else None if attempted else False
    if any(change.get("created") is True for change in child["changes"]):
        performed = True
    in_flight = child["data"].get("command_may_still_be_in_flight") is True
    unknown = bool(in_flight or child["data"].get("mutation_state_unknown") is True or
                   attempted and not child["success"] and (not known_native or child["stage"] == "readback"))
    row.update({"status": "success" if child["success"] else "error", "mutation_performed": performed,
                "mutation_state_unknown": unknown, "command_may_still_be_in_flight": in_flight,
                "error": child["error"], "error_code": child["error_code"]})


def _aggregate(result, summary):
    rows = summary["results"]
    result["changes"] = [dict(change, batch_index=row["index"], canonical_tool=row["type"])
                         for row in rows for change in row["response"]["changes"]]
    for field in ("compile", "save"):
        entries = [{"index": row["index"], "type": row["type"],
                    "blueprint_name": row["params"].get("blueprint_name"), **copy.deepcopy(row["response"][field])}
                   for row in rows if row["response"][field].get("requested")]
        result[field]["entries"] = entries
        result[field]["requested"] = bool(entries)
        attempts = [entry.get("attempted") for entry in entries]
        result[field]["attempted"] = True if True in attempts else None if None in attempts else False
        if field == "save":
            result[field]["saved"] = all(e.get("saved") is True for e in entries) if entries else False
        result[field]["status"] = "per_entry" if entries else "not_requested"
    summary["mutation_attempted"] = any(row["mutation_attempted"] for row in rows)
    values = [row.get("mutation_performed") for row in rows]
    summary["mutation_performed"] = True if True in values else None if None in values else False
    summary["mutation_state_unknown"] = any(row.get("mutation_state_unknown", False) for row in rows)
    summary["command_may_still_be_in_flight"] = any(row.get("command_may_still_be_in_flight", False) for row in rows)
    summary["partial_mutation"] = bool((summary["failed"] or summary["forced_stop"] or summary["not_run"]) and
                                       (summary["mutation_attempted"] or summary["mutation_state_unknown"]))
    summary["do_not_retry"] = bool(summary["mutation_attempted"] or summary["mutation_state_unknown"])
    result["retry"].update({"automatic_retry": False,
        "duplicate_risk": "unknown_outcome" if summary["mutation_state_unknown"] else "batch_mutations_would_repeat" if summary["mutation_attempted"] else "none_observed",
        "policy": "Same ID returns the historical complete response. Interrupted IDs never resume; inspect journal and assets before a new ID."})


def _run(result, commands, stop_on_error):
    summary = _summary(commands, stop_on_error)
    result["data"]["batch"] = summary
    started = time.perf_counter()
    ensure_editor_idle()

    def persist(stage):
        result["stage"] = stage
        # Sequence in the snapshot names the checkpoint being persisted.
        summary["checkpoint_sequence"] += 1
        checkpoint(result)

    def run_steps():
        for index, command in enumerate(commands):
            if index and time.perf_counter() - started >= BETWEEN_STEP_BUDGET_SECONDS:
                summary.update(forced_stop=True, stop_reason="between_step_time_budget", stopped_at_index=index)
                break
            child = new_result(result["operation_id"] + ":" + str(index))
            capability = _CAPABILITIES[command["type"]]
            row = {"index": index, "type": command["type"], "params": command["params"], "effect": capability["effect"],
                   "phase": "step_intent", "status": "pending", "mutation_attempted": False,
                   "mutation_performed": False, "mutation_state_unknown": False,
                   "response": child, "elapsed_seconds": 0.0}
            summary["results"].append(row)
            summary["executed"] += 1
            summary["not_run"] = list(range(index + 1, len(commands)))
            child_started = time.perf_counter()

            def before_mutation(observed):
                row["phase"] = "mutation_intent"
                # The core prepares its legacy attempted fields immediately
                # before this callback. The durable snapshot is only intent;
                # a failed checkpoint must not claim engine dispatch happened.
                for change in observed["changes"]:
                    change["attempted"] = False
                    change["intent_prepared"] = True
                compile_state = copy.deepcopy(observed["compile"])
                save_state = copy.deepcopy(observed["save"])
                if observed["compile"]["requested"]:
                    observed["compile"].update(attempted=False, status="intent_prepared_not_dispatched")
                if observed["save"]["requested"]:
                    observed["save"].update(attempted=False, saved=False, status="intent_prepared_not_dispatched")
                _aggregate(result, summary)
                persist(f"batch[{index}]:before:{observed['stage']}")
                for change in observed["changes"]:
                    change["attempted"] = True
                observed["compile"].update(compile_state)
                observed["save"].update(save_state)
                row["mutation_attempted"] = True
                row["phase"] = "engine_call_or_readback"

            try:
                persist(f"batch[{index}]:step_intent")
                ensure_editor_idle()
                summary["dispatched_count"] += 1
                row["phase"] = "action_preflight"
                arguments = {**command["params"], **capability["fixed"]}
                capability["action"](child, **arguments, checkpoint_callback=before_mutation)
            except Exception as error:
                fail(child, error)
            row["elapsed_seconds"] = round(time.perf_counter() - child_started, 6)
            row["phase"] = "returned"
            _observation(row)
            summary["succeeded" if child["success"] else "failed"] += 1
            _aggregate(result, summary)
            forced = row["mutation_state_unknown"] or child["error_code"].startswith("journal_") or child["error_code"] == "play_session_active"
            if forced or (not child["success"] and stop_on_error):
                reason = child["error_code"] or ("mutation_outcome_unknown" if row["mutation_state_unknown"] else "child_failed")
                summary.update(forced_stop=bool(forced), stop_reason=reason, stopped_at_index=index)
            try:
                persist(f"batch[{index}]:result")
            except Exception as error:
                summary.update(forced_stop=True, stop_reason="journal_checkpoint_failed", stopped_at_index=index)
                fail(result, error)
                break
            if forced or (not child["success"] and stop_on_error):
                break
            # Keep evidence even if the last bounded action produces a large
            # result, but do not add further actions. This is not silent truncation.
            if len(json.dumps(result, ensure_ascii=False).encode("utf-8")) > MAX_OBSERVED_RESULT_BYTES:
                summary.update(forced_stop=True, stop_reason="observed_result_size_limit", stopped_at_index=index)
                break

    if summary["transaction"]["scope_requested"]:
        with engine.transaction():
            run_steps()
    else:
        run_steps()
    summary["elapsed_seconds"] = round(time.perf_counter() - started, 6)
    summary["success"] = summary["failed"] == 0 and not summary["not_run"] and not summary["forced_stop"]
    result["success"] = summary["success"]
    result["validation"]["status"] = "all_child_postconditions_passed" if result["success"] else "batch_incomplete_or_failed"
    result["stage"] = "complete" if result["success"] else "batch_stopped" if summary["stop_reason"] else "batch_failed"
    _aggregate(result, summary)
    if not result["success"] and not result["error_code"]:
        fail(result, OperationError("batch_incomplete" if summary["not_run"] else "batch_child_failed",
                                     summary["stop_reason"] or f"{summary['failed']} child actions failed"))


def execute_batch(commands_json, stop_on_error=True, operation_id=None):
    try:
        commands = _normalize(commands_json, stop_on_error)
    except Exception as error:
        result = fail(new_result(operation_id), error)
        result["data"] = {"admission_rejected": True, "actions_executed": 0,
                          "supported_commands": sorted(_CAPABILITIES), "manifest_version": MANIFEST_VERSION}
        return result
    arguments = {"manifest_version": MANIFEST_VERSION, "commands": commands, "stop_on_error": stop_on_error}
    result = mutate("execute_batch", arguments, operation_id, lambda output: _run(output, commands, stop_on_error))
    if not result["success"] and "batch" in result["data"]:
        result["data"]["batch"]["success"] = False
    return result
