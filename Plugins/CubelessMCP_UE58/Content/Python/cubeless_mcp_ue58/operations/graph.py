"""Small Blueprint graph edits with legacy mutation_result_v1 observations.

This is a bounded subset: K2 graph listing and Branch/Sequence add/link/move/delete.
GUID input selectors resolve to canonical object paths. Conversion/promotion
nodes and replacement of existing links remain unsupported.
"""

import copy
import math
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import graph as api
from cubeless_mcp_ue58.compatibility.ue58.blueprint import ensure_editor_idle
from . import state
from .state import OperationError, fail, mutate, new_result


def _contract(result, before, after, kind, *, affected=1, created=0, updated=0, removed=0, skipped=0, passed=True):
    result["data"].update({"contract_version": "mutation_result_v1", "verified_success": passed,
        "postcondition_verified": passed, "success_semantics": "observed_postcondition",
        "mutation": kind, "before": before, "after": after,
        "delta": {"affected_count": affected, "created_count": created, "updated_count": updated,
                  "removed_count": removed, "skipped_count": skipped, "failed_count": 0 if passed else 1},
        "verification": {"status": "passed" if passed else "failed", "method": "independent_graph_api_readback"}})
    result["validation"].update({"status": "loaded_graph_readback_passed" if passed else "graph_postcondition_failed"})
    if not passed:
        raise OperationError("postcondition_failed", "The requested graph postcondition was not observed; no retry or rollback was performed")
    result["success"] = True
    result["stage"] = "complete"
    result["retry"]["duplicate_risk"] = "graph_change_would_repeat"


def inspect_graph(blueprint_name, graph_name="EventGraph"):
    result = new_result()
    try:
        inspect_graph_action(result, blueprint_name, graph_name)
    except Exception as error:
        fail(result, error)
    return result



def inspect_graph_action(result, blueprint_name, graph_name, checkpoint_callback=None):
    """Internal loaded-state read; no compile/save or asset mutation."""
    result["stage"] = "readback"
    result["data"] = api.graph_info(api.graph_editor(blueprint_name, graph_name))
    result["validation"]["status"] = "loaded_graph_readback_passed"
    result["success"] = True
    result["stage"] = "complete"


def _branch_arguments(blueprint_name, node_position, graph_name, graph_id, graph_type, create_graph_if_missing):
    from . import functions, variables
    return {**functions._selectors(blueprint_name, graph_name, graph_id, graph_type),
        "node_position_json": variables._json_text([0, 0] if node_position is None else node_position),
        "create_graph_if_missing": functions._bool(create_graph_if_missing, "create_graph_if_missing")}


def add_blueprint_branch_node(blueprint_name, node_position=None, graph_name="", graph_id="", graph_type="", create_graph_if_missing=False):
    from . import functions
    try:
        arguments = _branch_arguments(blueprint_name, node_position, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return fail(new_result(), error)
    return mutate("add_blueprint_branch_node", arguments, None,
        lambda result: functions._run_action(result, "add_blueprint_branch_node", arguments, operation_api=api))


def add_blueprint_branch_node_action(result, blueprint_name, node_position=None, graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, checkpoint_callback=None):
    from . import functions
    return functions._run_action(result, "add_blueprint_branch_node",
        _branch_arguments(blueprint_name, node_position, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback, operation_api=api)


def add_branch_node(blueprint_name, graph_name, x, y, operation_id):
    arguments = {"blueprint_name": blueprint_name, "graph_name": graph_name, "x": x, "y": y}

    return mutate('add_branch_node', arguments, operation_id, lambda result: add_branch_node_action(result, blueprint_name, graph_name, x, y))


def add_branch_node_action(result, blueprint_name, graph_name, x, y, checkpoint_callback=None):
    """Internal guarded action; standalone/batch callers own journaling."""
    ensure_editor_idle()
    editor = api.graph_editor(blueprint_name, graph_name)
    before = _begin_node_add(result, editor, "add_branch", checkpoint_callback)
    node = api.add_branch(editor, x, y)
    result["stage"] = "readback"
    after = api.graph_info(editor)
    result["data"]["graph_name"] = graph_name
    _finish_node_add(result, before, after, api.node_id(node), "add_branch", "K2Node_IfThenElse", x, y)


def _begin_node_add(result, editor, kind, checkpoint_callback=None):
    """One shared intent/checkpoint boundary for Branch and exact Sequence adds."""
    before = copy.deepcopy(api.graph_info(editor))
    result["data"]["before"] = before
    if before.get("total_node_count", len(before["nodes"])) >= 2048:
        raise OperationError("graph_node_limit_reached", "Adding a node would exceed the 2048 total-node observation limit, including comments; no mutation performed")
    result["stage"] = kind
    result["changes"].append({"kind": kind, "graph_path": before["graph_path"], "attempted": False,
        "outcome": "not_yet_dispatched_at_checkpoint"})
    (checkpoint_callback or state.checkpoint)(result)
    result["changes"][-1].update(attempted=True, outcome="unknown_until_graph_readback")
    result["data"].update(mutation_state_unknown=True, mutation_performed=None)
    return before


def _finish_node_add(result, before, after, new_id, kind, node_class, x, y, extra_verified=True):
    old = {row["node_id"]: row for row in before["nodes"]}
    current = {row["node_id"]: row for row in after["nodes"]}
    observed = current.get(new_id, {})
    preserved = (set(current) - set(old) == {new_id}
        and all(current.get(path) == value for path, value in old.items())
        and after.get("total_node_count", len(current)) == before.get("total_node_count", len(old)) + 1)
    passed = (preserved and observed.get("class") == node_class
        and observed.get("x") == x and observed.get("y") == y and extra_verified)
    result["data"].update(observed, topology_preserved=preserved, mutation_state_unknown=not passed,
        mutation_performed=True if new_id in current and new_id not in old else None)
    result["changes"][-1].update(created_node_id=new_id, created=new_id in current and new_id not in old,
        outcome="observed_new_node" if passed else "postcondition_mismatch_inspect_graph")
    _contract(result, before, after, kind, created=1 if passed else 0, passed=passed)


def _sequence_arguments(blueprint_name, node_position_json, output_count, graph_name, graph_id, graph_type, create_graph_if_missing):
    from . import functions
    selectors = functions._selectors(blueprint_name, graph_name, graph_id, graph_type)
    create = functions._bool(create_graph_if_missing, "create_graph_if_missing")
    if type(output_count) is not int or not 2 <= output_count <= 64:
        raise OperationError("sequence_output_count_invalid", "output_count must be an integer from 2 to 64; legacy rounding/minimum clamping is not applied")
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if create:
        from . import variables
        return {**selectors, "node_position_json": variables._json_text(position), "output_count": output_count, "create_graph_if_missing": True}
    if type(position) is not list or len(position) != 2 or any(type(value) is not int or abs(value) > 1000000 for value in position):
        raise OperationError("sequence_position_invalid", "node_position_json must contain two integers from -1000000 to 1000000")
    return {**selectors, "x": position[0], "y": position[1], "output_count": output_count}


def add_blueprint_sequence_node(blueprint_name, node_position_json="[0,0]", output_count=2, graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    try:
        arguments = _sequence_arguments(blueprint_name, node_position_json, output_count, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("add_blueprint_sequence_node", arguments, operation_id, lambda result: _sequence_action(result, arguments))


def add_blueprint_sequence_node_action(result, blueprint_name, node_position_json="[0,0]", output_count=2,
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    arguments = _sequence_arguments(blueprint_name, node_position_json, output_count, graph_name, graph_id, graph_type, create_graph_if_missing)
    _sequence_action(result, arguments, checkpoint_callback)


def _sequence_action(result, arguments, checkpoint_callback=None):
    if arguments.get("create_graph_if_missing") is True:
        from . import functions
        values = dict(arguments)
        result["data"].update(full_legacy_sequence_parity=False, requested_output_count=arguments["output_count"])
        return functions._run_action(result, "add_blueprint_sequence_node", values, checkpoint_callback, operation_api=api)
    ensure_editor_idle()
    editor, selected = api.sequence_editor(**{key: arguments[key] for key in ("blueprint_path", "graph_name", "graph_id", "graph_type")})
    result["data"].update(graph=selected, full_legacy_sequence_parity=False,
        loading_side_effects_verified=False, requested_output_count=arguments["output_count"])
    before = _begin_node_add(result, editor, "add_sequence", checkpoint_callback)
    try:
        native = api.add_sequence(editor, arguments["x"], arguments["y"], arguments["output_count"])
    except OperationError as error:
        if error.code == "native_sequence_api_unavailable":
            result["changes"] = []
            result["data"].update(mutation_state_unknown=False, mutation_performed=False)
        raise
    result["data"]["native_outcome"] = copy.deepcopy(native)
    result["data"].update(native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not native["change_state_known"], mutation_performed=native.get("changed"))
    result["compile"].update(attempted=native["compiled"], compiled=native["compiled"])
    result["save"].update(attempted=native["saved"], saved=native["saved"])
    if not native["mutation_attempted"] and native.get("changed") is False and native["change_state_known"]:
        result["changes"] = []
    else:
        result["changes"][-1].update(attempted=native["mutation_attempted"], changed=native.get("changed"),
            change_state_known=native["change_state_known"])
    result["stage"] = native.get("stage", "add_sequence")
    if native["compiled"] or native["saved"]:
        result["data"]["mutation_state_unknown"] = True
        raise OperationError("unexpected_sequence_compile_save", "Sequence helper reported unrequested compiler/save work")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "sequence_native_failed", native.get("error") or "Sequence native operation failed")
    if not native["change_state_known"] or not native["postcondition_passed"] or not native["mutation_attempted"]:
        raise OperationError("sequence_native_unverified", "Sequence native success lacks known mutation/postconditions")
    result["stage"] = "readback"
    result["data"]["mutation_state_unknown"] = True
    after = api.graph_info(editor)
    identity = native["data"]["object_path"]
    observed = next((row for row in after["nodes"] if row["node_id"] == identity), {})
    pins = observed.get("pins", [])
    expected = [("execute", "input")] + [("then_" + str(index), "output") for index in range(arguments["output_count"])]
    pins_verified = (sorted((pin["name"], pin["direction"]) for pin in pins) == sorted(expected)
        and all(pin["category"] == "exec" and not pin["links"] for pin in pins)
        and observed.get("node_guid") == uuid.UUID(native["data"]["node_guid"]).hex)
    _finish_node_add(result, before, after, identity, "add_sequence", "K2Node_ExecutionSequence",
        arguments["x"], arguments["y"], pins_verified)
    # Preserve the original named Sequence tool's GUID identity, while the
    # existing Branch/inspect contracts retain path-valued node_id.
    result["data"].update(native["data"])
    result["data"].update(node_id=observed["node_guid"], node_guid=observed["node_guid"], object_path=identity,
        node_identifier_kind="guid", before=before, after=after, mutation_state_unknown=False)


def set_node_position(blueprint_name, node_id, x, y, graph_name, expected_x, expected_y, operation_id):
    arguments = {"blueprint_name": blueprint_name, "node_id": node_id, "x": x, "y": y, "graph_name": graph_name,
                 "expected_x": expected_x, "expected_y": expected_y}

    return mutate('set_node_position', arguments, operation_id, lambda result: set_node_position_action(result, blueprint_name, node_id, x, y, graph_name, expected_x, expected_y))


def set_node_position_action(result, blueprint_name, node_id, x, y, graph_name, expected_x, expected_y, checkpoint_callback=None):
    """Internal guarded action; standalone/batch callers own journaling."""
    ensure_editor_idle()
    editor = api.graph_editor(blueprint_name, graph_name)
    node = api.find_node(editor, node_id)
    canonical_id = api.node_id(node)
    before = api.node_info(node)
    result["data"]["before"] = copy.deepcopy(before)
    if expected_x is not None and before["x"] != expected_x or expected_y is not None and before["y"] != expected_y:
        raise OperationError("precondition_mismatch", "Node position differs from the expected position; no mutation performed")
    result["stage"] = "move_node"
    changed = before["x"] != x or before["y"] != y
    if changed:
        result["changes"].append({"kind": "move_node", "node_id": canonical_id, "requested_node_id": node_id, "attempted": True})
        (checkpoint_callback or state.checkpoint)(result)
        api.move_node(node, x, y)
    result["stage"] = "readback"
    after = api.node_info(api.find_node(editor, canonical_id))
    _contract(result, before, after, "set_node_position", updated=int(changed), skipped=int(not changed),
              passed=after["x"] == x and after["y"] == y)


def delete_node(blueprint_name, node_id, graph_name, allow_exec_linked_delete, operation_id):
    arguments = {"blueprint_name": blueprint_name, "node_id": node_id, "graph_name": graph_name,
                 "allow_exec_linked_delete": allow_exec_linked_delete}

    return mutate('delete_node', arguments, operation_id, lambda result: delete_node_action(result, blueprint_name, node_id, graph_name, allow_exec_linked_delete))


def delete_node_action(result, blueprint_name, node_id, graph_name, allow_exec_linked_delete, checkpoint_callback=None):
    """Internal guarded action; standalone/batch callers own journaling."""
    ensure_editor_idle()
    editor = api.graph_editor(blueprint_name, graph_name)
    node = api.find_node(editor, node_id)
    canonical_id = api.node_id(node)
    before = api.graph_info(editor)
    result["data"]["before"] = copy.deepcopy(before)
    target = api.node_info(node)
    if not allow_exec_linked_delete and any(pin["category"] == "exec" and pin["links"] for pin in target["pins"]):
        raise OperationError("linked_exec_delete_rejected", "Node has linked execution pins; explicit allow_exec_linked_delete is required")
    result["stage"] = "delete_node"
    result["changes"].append({"kind": "delete_node", "node_id": canonical_id, "requested_node_id": node_id, "attempted": True})
    (checkpoint_callback or state.checkpoint)(result)
    api.remove_node(editor, node)
    result["stage"] = "readback"
    after = api.graph_info(editor)
    passed = canonical_id not in {item["node_id"] for item in after["nodes"]}
    no_dangling = not any(link["node_id"] == canonical_id for item in after["nodes"] for pin in item["pins"] for link in pin["links"])
    _contract(result, before, after, "delete_node", removed=1 if passed else 0, passed=passed and no_dangling)


def connect_graph_pins(blueprint_name, source_node_id, source_pin_name, target_node_id, target_pin_name, graph_name, operation_id):
    arguments = {"blueprint_name": blueprint_name, "source_node_id": source_node_id, "source_pin_name": source_pin_name,
                 "target_node_id": target_node_id, "target_pin_name": target_pin_name, "graph_name": graph_name}

    return mutate('connect_graph_pins', arguments, operation_id, lambda result: connect_graph_pins_action(result, blueprint_name, source_node_id, source_pin_name, target_node_id, target_pin_name, graph_name))


def connect_graph_pins_action(result, blueprint_name, source_node_id, source_pin_name, target_node_id, target_pin_name, graph_name, checkpoint_callback=None):
    """Internal guarded action; standalone/batch callers own journaling."""
    ensure_editor_idle()
    editor = api.graph_editor(blueprint_name, graph_name)
    _connect_action(result, editor, source_node_id, source_pin_name, target_node_id, target_pin_name,
        checkpoint_callback=checkpoint_callback)


def _connection_arguments(blueprint_name, source_node_id, source_pin, target_node_id, target_pin,
        graph_name, graph_id, graph_type, allow_pin_link_replacement, compile, save_asset,
        expected_existing_source_link_count, expected_existing_source_link_target_node_id,
        expected_existing_source_link_target_pin, expected_schema_response):
    from . import functions
    selectors = functions._selectors(blueprint_name, graph_name, graph_id, graph_type)
    permitted = functions._bool(allow_pin_link_replacement, "allow_pin_link_replacement")
    compile_requested = functions._bool(compile, "compile")
    save_requested = functions._bool(save_asset, "save_asset")
    fields = (expected_existing_source_link_count, expected_existing_source_link_target_node_id,
        expected_existing_source_link_target_pin, expected_schema_response)
    has_expectation = any(value is not None for value in fields)
    if has_expectation and (compile_requested or save_requested):
        raise OperationError("connection_compile_save_unsupported", "The existing guarded exec action performs no compilation or save; use separate named actions after the chain is complete")
    if has_expectation and (not permitted or any(value is None for value in fields)):
        raise OperationError("connection_replacement_guard_required", "Replacement permission requires all four expected fields; expected fields are invalid without permission")
    guard = None
    if has_expectation:
        if type(fields[0]) is not int or fields[0] != 1 or fields[3] != "break_others_a":
            raise OperationError("connection_replacement_guard_invalid", "This slice requires exact integer source count 1 and expected_schema_response=break_others_a")
        guard = {"expected_existing_source_link_count": fields[0],
            "expected_existing_source_link_target_node_id": api._selector(fields[1], "expected_existing_source_link_target_node_id"),
            "expected_existing_source_link_target_pin": api._selector(fields[2], "expected_existing_source_link_target_pin"),
            "expected_schema_response": fields[3]}
    arguments = {**selectors, "source_node_id": api._selector(source_node_id, "source_node_id"),
        "source_pin": api._selector(source_pin, "source_pin"), "target_node_id": api._selector(target_node_id, "target_node_id"),
        "target_pin": api._selector(target_pin, "target_pin"), "guard": guard}
    if not has_expectation and (permitted or compile_requested or save_requested):
        arguments["scalar_native_request"] = {
            "blueprint_name": selectors["blueprint_path"],
            **{key: arguments[key] for key in ("source_node_id", "source_pin", "target_node_id", "target_pin", "graph_name", "graph_id", "graph_type")},
            "allow_pin_link_replacement": permitted, "compile": compile_requested, "save_asset": save_requested}
    return arguments


def connect_blueprint_graph_pins(blueprint_name, source_node_id, source_pin, target_node_id, target_pin,
        graph_name=None, graph_id=None, graph_type=None, allow_pin_link_replacement=False, compile=False, save_asset=False,
        expected_existing_source_link_count=None, expected_existing_source_link_target_node_id=None,
        expected_existing_source_link_target_pin=None, expected_schema_response=None, operation_id=None):
    try:
        arguments = _connection_arguments(blueprint_name, source_node_id, source_pin, target_node_id, target_pin,
            graph_name, graph_id, graph_type, allow_pin_link_replacement, compile, save_asset,
            expected_existing_source_link_count, expected_existing_source_link_target_node_id,
            expected_existing_source_link_target_pin, expected_schema_response)
    except Exception as error:
        result = new_result(operation_id)
        result["compile"]["requested"] = compile is True
        result["save"]["requested"] = save_asset is True
        return fail(result, error)
    return mutate("connect_blueprint_graph_pins", arguments, operation_id, lambda result: _legacy_connection_action(result, arguments))


def connect_blueprint_graph_pins_action(result, blueprint_name, source_node_id, source_pin, target_node_id, target_pin,
        graph_name=None, graph_id=None, graph_type=None, allow_pin_link_replacement=False, compile=False, save_asset=False,
        expected_existing_source_link_count=None, expected_existing_source_link_target_node_id=None,
        expected_existing_source_link_target_pin=None, expected_schema_response=None, checkpoint_callback=None):
    result["compile"]["requested"] = compile is True
    result["save"]["requested"] = save_asset is True
    arguments = _connection_arguments(blueprint_name, source_node_id, source_pin, target_node_id, target_pin,
        graph_name, graph_id, graph_type, allow_pin_link_replacement, compile, save_asset,
        expected_existing_source_link_count, expected_existing_source_link_target_node_id,
        expected_existing_source_link_target_pin, expected_schema_response)
    _legacy_connection_action(result, arguments, checkpoint_callback)


def _legacy_connection_action(result, arguments, checkpoint_callback=None):
    if "scalar_native_request" in arguments:
        return _scalar_native_connection_action(result, arguments["scalar_native_request"], checkpoint_callback)
    ensure_editor_idle()
    editor, selected = api.connection_editor(**{key: arguments[key] for key in ("blueprint_path", "graph_name", "graph_id", "graph_type")})
    result["data"].update(graph=selected, full_legacy_connection_parity=False, loading_side_effects_verified=False)
    _connect_action(result, editor, arguments["source_node_id"], arguments["source_pin"],
        arguments["target_node_id"], arguments["target_pin"], arguments["guard"], checkpoint_callback,
        identity="connect_blueprint_graph_pins")


def _scalar_native_connection_action(result, params, checkpoint_callback=None):
    """Original11 via the existing concrete Native connection leaf; no second writer."""
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_resolution_connection as scalar_api
    ensure_editor_idle()
    callback = checkpoint_callback or state.checkpoint
    result["data"].update(operation="connect_blueprint_graph_pins", requested_arguments=copy.deepcopy(params),
        scalar_connection_native_route=None, concrete_connection_native_route=True, native_dispatched=False, native_result_returned=False,
        mutation_state_unknown=False, automatic_action_retry=False, full_legacy_connection_parity=False)
    result["compile"].update(requested=params["compile"], attempted=False, compiled=None, status="not_dispatched" if params["compile"] else "not_requested")
    result["save"].update(requested=params["save_asset"], attempted=False, saved=False)
    result["data"]["pending_mutation"] = {"operation": "connect_blueprint_graph_pins", **copy.deepcopy(params), "dispatch_state_at_checkpoint": "not_yet_dispatched"}
    result["stage"] = "before_scalar_connection_checkpoint"
    callback(result)
    result["stage"] = "native_scalar_connection"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": "connect_blueprint_graph_pins", "attempted": None, "changed": None, "change_state_known": False}]
    if params["compile"]:
        result["compile"].update(attempted=None, status="unknown_until_native_result")
    if params["save_asset"]:
        result["save"]["attempted"] = None
    try:
        native = scalar_api.connect_blueprint_graph_pins(params)
    except Exception as error:
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, mutation_state_unknown=False)
            result["changes"] = []
            result["compile"].update(attempted=False, status="not_dispatched" if params["compile"] else "not_requested")
            result["save"]["attempted"] = False
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        raise
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(
        scalar_connection_native_route=native["data"].get("same_scalar_graph_pins_contract") is True,
        exec_connection_native_route=native["data"].get("exec_graph_pins_contract") is True,
        array_connection_native_route=native["data"].get("array_graph_pins_contract") is True,
        set_connection_native_route=native["data"].get("set_graph_pins_contract") is True)
    known = native["change_state_known"]
    result["data"].update(native_result_returned=True,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        mutation_state_unknown=not known, native_mutation_attempted=native["mutation_attempted"],
        mutation_performed=native.get("changed") if known else None)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status=native.get("compile_status", "native_result" if native["compile_requested"] else "not_requested"))
    for key in ("compile_error_count", "compile_warning_count", "compile_errors", "compile_warnings", "diagnostics"):
        if key in native:
            result["compile"][key] = copy.deepcopy(native[key])
    result["save"].update(requested=native["save_requested"], attempted=native.get("save_attempted"), saved=native["saved"])
    result["changes"] = [] if known and native.get("changed") is False and not native["mutation_attempted"] else [{
        "kind": "connect_blueprint_graph_pins", "attempted": native["mutation_attempted"],
        "changed": native.get("changed") if known else None, "change_state_known": known}]
    result["validation"].update(native_postcondition_passed=native["postcondition_passed"],
        status="native_scalar_connection_postcondition" if native["postcondition_passed"] else "native_scalar_connection_failed",
        disk_reload="not_run", runtime_execution="not_run")
    result["stage"] = "returned_scalar_connection_operation"
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_scalar_connection_failed", native.get("error") or "Connection failed; actual prefix retained")
    if not known or not native["postcondition_passed"]:
        raise OperationError("scalar_connection_unverified", "Native success lacks known verified connection state; do not replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_native_connection_prefix_before_new_operation" if native["mutation_attempted"] else "none_observed"


def _pin_row(snapshot, node_path, name, direction):
    matches = [pin for row in snapshot["nodes"] if row["node_id"] == node_path
        for pin in row["pins"] if pin["name"] == name and pin["direction"] == direction]
    if len(matches) != 1:
        raise OperationError("connection_pin_observation_missing", "Expected one independently observed endpoint pin")
    return matches[0]


def _link_keys(pin):
    keys = [(link["node_id"], link["pin_name"]) for link in pin["links"]]
    if len(keys) != len(set(keys)):
        raise OperationError("connection_observation_inconsistent", "Duplicate pin links were observed; no repair was attempted")
    return set(keys)


def _nonlink_state(snapshot):
    rows = copy.deepcopy(snapshot)
    rows.pop("package_dirty", None)  # A successful link edit may dirty its package.
    for row in rows["nodes"]:
        for pin in row["pins"]:
            pin.pop("links", None)
    rows["nodes"] = {row["node_id"]: row for row in rows["nodes"]}
    return rows


def _pair(source_id, source_pin, target_id, target_pin):
    return {(source_id, "output", source_pin, target_id, target_pin),
        (target_id, "input", target_pin, source_id, source_pin)}


def _numeric_target_default_reset(editor, before, after, source_id, source_pin, target_id, target_pin, plan):
    """UE58 K2Node.cpp:515-531 resets a newly connected input to its auto default.

    Only exact scalar real/int32, Core.Rotator or Name input types with independently observed
    auto defaults qualify. Actual snapshots keep changed values; all other
    observed fields and the native before/after default state remain checked.
    """
    state_before = api.connection_target_default_state(plan)
    if state_before is None:
        return False, {"verified": False, "reason": "native_target_default_state_unavailable"}
    source_type, target_type = plan.get("source_pin_type", {}), plan.get("target_pin_type", {})
    exact_numeric = all(value.get("category") == "real" and value.get("subcategory") in {"float", "double"}
        and value.get("container_type") == 0 and value.get("is_const") is False and value.get("is_reference") is False
        and value.get("type_object") == "" for value in (source_type, target_type))
    exact_rotator = all(value.get("category") == "struct" and value.get("subcategory") == "None"
        and value.get("container_type") == 0 and value.get("is_const") is False and value.get("is_reference") is False
        and value.get("type_object") == "/Script/CoreUObject.Rotator" for value in (source_type, target_type))
    exact_name = all(value.get("category") == "name" and value.get("subcategory") == "None"
        and value.get("container_type") == 0 and value.get("is_const") is False and value.get("is_reference") is False
        and value.get("type_object") == "" for value in (source_type, target_type))
    exact_int = source_type == target_type and all(value.get("category") == "int" and value.get("subcategory") == "None"
        and value.get("container_type") == 0 and value.get("is_const") is False and value.get("is_reference") is False
        and value.get("type_object") == "" for value in (source_type, target_type))
    old = _pin_row(before, target_id, target_pin, "input")
    current = _pin_row(after, target_id, target_pin, "input")
    if exact_int:
        source = _pin_row(before, source_id, source_pin, "output")
        exact_int = (plan.get("type_exactly_equal") is True
            and all(row.get("category") == "integer" and row.get("category_encoding") == "UE58_pin_json_schema_type"
                for row in (source, old, current))
            and type(old.get("native_pin_type_text")) is str and bool(old["native_pin_type_text"])
            and source.get("native_pin_type_text") == old["native_pin_type_text"] == current.get("native_pin_type_text"))
    eligible = ((exact_numeric or exact_rotator or exact_name or exact_int) and plan.get("exact_standard_k2_schema") is True
        and plan.get("direct_connection_allowed") is True and plan.get("schema_response_name") == "make"
        and plan.get("already_linked") is False and not old["links"]
        and old["category"] == current["category"] == ("number" if exact_numeric else "string" if exact_name else "integer" if exact_int else "object")
        and _link_keys(current) == {(source_id, source_pin)}
        and state_before["is_orphaned"] is False and state_before["default_value_is_ignored"] is False
        and state_before["default_value_is_read_only"] is False and state_before["default_object_path"] == ""
        and state_before["default_text_empty"] is True and state_before["raw_default_value"] == old["value"]
        and current["value"] == state_before["autogenerated_default_value"] and current["value"] != old["value"])
    try:
        values = (old["value"], state_before["autogenerated_default_value"])
        if exact_name:
            finite_values = (all(type(value) is str for value in values) and bool(old["value"])
                and state_before["autogenerated_default_value"] == "None")
        elif exact_int:
            digits = [(value[1:] if value[:1] in ("+", "-") else value) if type(value) is str else "" for value in values]
            finite_values = (all(bool(value) and value.isascii() and value.isdecimal() for value in digits)
                and all(-2147483648 <= int(value, 10) <= 2147483647 for value in values))
        elif exact_rotator:
            parts = [value.split(",") for value in values]
            finite_values = all(len(row) == 3 and all(math.isfinite(float(part)) for part in row) for row in parts)
        else:
            finite_values = all(math.isfinite(float(value)) for value in values)
        eligible = eligible and finite_values
    except (ValueError, TypeError, OverflowError):
        eligible = False
    if not eligible:
        return False, {"verified": False, "reason": "not_exact_numeric_input_auto_default_transition", "before": state_before}
    try:
        plan_after = api.connection_plan(editor, source_id, source_pin, target_id, target_pin)
        state_after = api.connection_target_default_state(plan_after)
        expected = dict(state_before, raw_default_value=state_before["autogenerated_default_value"])
        verified = (state_after == expected and plan_after.get("already_linked") is True
            and plan_after.get("exact_standard_k2_schema") is True
            and plan_after.get("source_pin_type") == source_type and plan_after.get("target_pin_type") == target_type
            and (not exact_int or plan_after.get("type_exactly_equal") is True))
        normalized = copy.deepcopy(after)
        _pin_row(normalized, target_id, target_pin, "input")["value"] = old["value"]
        verified = verified and _nonlink_state(normalized) == _nonlink_state(before)
        return verified, {"verified": verified, "before": state_before, "after": state_after,
            "plan_after": plan_after, "target_node_id": target_id, "target_pin_name": target_pin,
            "scope": "Only this new exact scalar real/int32, Core.Rotator or Name input's raw DefaultValue changed to its preobserved AutogeneratedDefaultValue; every other observed graph/default/type/identity field preserved",
            "accepted_value_kind": "Core.Rotator_finite_CSV3" if exact_rotator else "Name_auto_None" if exact_name else "scalar_int32" if exact_int else "scalar_real",
            "api_basis": "Installed K2Node.cpp:515-531, EdGraphSchema.cpp:521-522, EdGraphSchema_K2.cpp:5219-5221,5249-5251,5312-5324,4843-4847"}
    except Exception as error:
        # The ordinary independent graph readback remains authoritative. Missing
        # supplementary native proof is a known failed postcondition, not a retry.
        return False, {"verified": False, "reason": "native_after_default_observation_failed", "before": state_before,
            "error_code": getattr(error, "code", "python_operation_failed"), "error": str(error)}


def _dynamic_cast_source_retype(editor, before, after, source_id, source_pin, target_id, target_pin, plan, observed_after=None):
    """Observe only the installed finite cast Object-input callback transition."""
    initial = plan.get("dynamic_cast_source_observation")
    if type(initial) is not dict:
        return False, {"verified": False, "reason": "native_cast_source_observation_unavailable"}
    old = _pin_row(before, target_id, target_pin, "input")
    current = _pin_row(after, target_id, target_pin, "input")
    source_old = _pin_row(before, source_id, source_pin, "output")
    pin_type = initial.get("pin_type", {})
    native_source = initial.get("native_source_before", {})
    default = initial.get("default_state", {})
    expected_type = {"category": "wildcard", "subcategory": "None", "type_object": "/Script/CoreUObject.Object",
        "container_type": 0, "is_reference": False, "is_const": False, "is_weak_pointer": False,
        "is_uobject_wrapper": False, "serialize_as_single_precision_float": False}
    mid_source = initial.get("target_class_path") == "/Script/Engine.MaterialInstanceDynamic"
    data_table_source = initial.get("target_class_path") == "/Script/Engine.DataTable"
    if mid_source or data_table_source:
        expected_type["type_object"] = ""
    eligible = (initial.get("eligible") is True and plan.get("exact_standard_k2_schema") is True
        and plan.get("direct_connection_allowed") is True and plan.get("schema_response_name") == "make"
        and plan.get("already_linked") is False and not old["links"]
        and initial.get("node_path") == target_id and initial.get("pin_name") == target_pin
        and initial.get("node_class_path") == "/Script/BlueprintGraph.K2Node_DynamicCast"
        and initial.get("target_class_path") in ("/Script/Engine.ChildActorComponent", "/Script/Engine.SceneComponent",
            "/Script/Engine.MaterialInstanceDynamic", "/Script/Engine.DataTable")
        and type(initial.get("pure")) is bool and initial.get("all_pins_readable") is True
        and initial.get("cast_source_pin_identity_verified") is True and default.get("pin_friendly_name") == ""
        and type(initial.get("linked_source_count")) is int and initial["linked_source_count"] == 0
        and initial.get("all_links_reciprocal") is True and initial.get("source_output_link_present") is False
        and initial.get("source_output_node_path") == source_id and initial.get("source_output_pin_name") == source_pin
        and initial.get("source_output_pin_type_text") == source_old.get("native_pin_type_text")
        and type(pin_type) is dict and pin_type == expected_type
        and initial.get("pin_type_text") == old.get("native_pin_type_text")
        and initial.get("pin_type_display") == old.get("type_display")
        and old["category"] == initial.get("projected_category") == "integer"
        and current["category"] == initial.get("expected_object_projected_category") == "string"
        and current.get("native_pin_type_text") == initial.get("expected_object_pin_type_text")
        and current.get("type_display") == initial.get("expected_object_pin_type_display")
        and _link_keys(current) == {(source_id, source_pin)}
        and type(native_source) is dict and native_source == initial.get("native_source_after")
        and native_source.get("observations_complete") is True and native_source.get("native_identity_verified") is True
        and native_source.get("class_path") == initial.get("target_class_path")
        and initial.get("native_source_preserved") is True
        and type(default) is dict and default.get("hidden") is False and default.get("not_connectable") is False
        and default.get("is_orphaned") is False and default.get("default_object_path") == ""
        and default.get("default_text_empty") is True
        and default.get("raw_default_value") in ("", "None") and default.get("autogenerated_default_value") in ("", "None"))
    if mid_source:
        material = initial.get("mid_material_source_before", {})
        eligible = (eligible and type(material) is dict and material == initial.get("mid_material_source_after")
            and material.get("observations_complete") is True and material.get("native_identity_verified") is True
            and material.get("class_path") == "/Script/Engine.MaterialInterface"
            and initial.get("mid_material_source_preserved") is True)
    try:
        eligible = eligible and bool(uuid.UUID(initial.get("node_guid", "")).int) and bool(uuid.UUID(initial.get("pin_id", "")).int)
    except (TypeError, ValueError, AttributeError):
        eligible = False
    if not eligible:
        return False, {"verified": False, "reason": "not_exact_finite_cast_source_transition", "before": initial}
    try:
        plan_after = observed_after if observed_after is not None else api.connection_plan(editor, source_id, source_pin, target_id, target_pin)
        final = plan_after.get("dynamic_cast_source_observation", {})
        if type(initial.get("bfl_target_context")) is dict and initial["bfl_target_context"] != final.get("bfl_target_context"):
            error = OperationError("connection_bfl_datatable_context_changed", "Exact current BFL cast target roles changed across dispatch")
            error.native_graph_result = plan_after
            raise error
        stable = ("eligible", "node_path", "node_guid", "node_class_path", "target_class_path", "pure", "pin_name",
            "pin_id", "persistent_guid", "expected_object_pin_type_text", "expected_object_pin_type_display",
            "expected_object_projected_category", "projected_category_basis", "source_output_node_path", "source_output_pin_name",
            "source_output_pin_id", "source_output_pin_type_text", "all_pins_readable", "cast_source_pin_identity_verified",
            "default_state", "native_source_before", "native_source_after", "native_source_preserved",
            "mid_material_source_before", "mid_material_source_after", "mid_material_source_preserved", "bfl_target_context")
        verified = (type(final) is dict and all(final.get(key) == initial.get(key) for key in stable)
            and plan_after.get("already_linked") is True and plan_after.get("exact_standard_k2_schema") is True
            and plan_after.get("source_pin_type") == plan.get("source_pin_type")
            and final.get("pin_type") == dict(pin_type, category="object", type_object="/Script/CoreUObject.Object")
            and final.get("pin_type_text") == initial["expected_object_pin_type_text"]
            and final.get("pin_type_display") == initial["expected_object_pin_type_display"]
            and final.get("projected_category") == "string" and final.get("linked_source_count") == 1
            and type(final.get("linked_source_count")) is int and final.get("all_links_reciprocal") is True
            and final.get("source_output_link_present") is True)
        normalized = copy.deepcopy(after)
        normalized_pin = _pin_row(normalized, target_id, target_pin, "input")
        for key in ("category", "type_display", "native_pin_type_text"):
            normalized_pin[key] = old[key]
        verified = verified and _nonlink_state(normalized) == _nonlink_state(before)
        return verified, {"verified": verified, "before": initial, "after": final, "plan_after": plan_after,
            "target_node_id": target_id, "target_pin_name": target_pin,
            "scope": "Only exact native CAC/Scene wildcard/UObject or MID/DataTable wildcard/null Object input retyped to object/UObject after one new reciprocal link; full defaults, native source, raw input identity and all other observed visible node/pin nonlink fields preserved",
            "hidden_pin_content_independently_observed": False, "runtime_invocation_verified": False}
    except Exception as error:
        if type(initial.get("bfl_target_context")) is dict:
            raise  # This finite current failure must stop the caller without another SDK.
        return False, {"verified": False, "reason": "native_after_cast_source_observation_failed", "before": initial,
            "error_code": getattr(error, "code", "python_operation_failed"), "error": str(error)}


def _map_specialization_projection(before, after, source_id, source_pin, target_id, target_pin, plan, final):
    """Check copied evidence for the stock first TargetMap callback; no engine access."""
    initial, current = plan.get("map_connection_observation"), final.get("map_connection_observation")
    receipt = {"verified": False, "runtime_map_operation_verified": False}
    if type(initial) is not dict or type(current) is not dict:
        return False, dict(receipt, reason="map_observation_missing")
    double_map = plan.get("connection_scope") == "native_string_double_map"
    try:
        operation = initial["operation"]
        key_only = operation in ("contains", "remove")
        array_result = operation in ("keys", "values")
        length_result = operation == "length"
        stable = ("node_path", "node_guid", "function_name", "operation", "native_function", "native_source")
        eligible = (plan.get("connection_scope") == final.get("connection_scope")
            and plan.get("connection_scope") in ("native_string_vector_map", "native_string_double_map")
            and plan.get("exact_standard_k2_schema") is True and final.get("exact_standard_k2_schema") is True
            and plan.get("direct_connection_allowed") is True and plan.get("schema_response_name") == "make"
            and plan.get("already_linked") is False and final.get("already_linked") is True
            and initial.get("eligible") is True and current.get("eligible") is True
            and initial.get("native_source_current_verified") is True and current.get("native_source_current_verified") is True
            and initial.get("first_target_map_connection") is True and current.get("first_target_map_connection") is False
            and initial.get("specialization_pending") is True and current.get("specialization_pending") is False
            and current.get("specialized_from_actual_target_link") is True
            and target_pin == "TargetMap" and initial.get("node_path") == target_id
            and operation in ("clear", "find", "add", "contains", "remove", "keys", "values", "length")
            and (not array_result or initial.get("array_output_contract_verified") is True
                and current.get("array_output_contract_verified") is True and initial.get("pure") is False and current.get("pure") is False
                and initial.get("array_output_name") == ("Keys" if operation == "keys" else "Values")
                and initial.get("array_output_name") == current.get("array_output_name")
                and initial.get("map_connection_role") == current.get("map_connection_role") == "target_input")
            and (not key_only or initial.get("key_only_contract_verified") is True
                and current.get("key_only_contract_verified") is True and initial.get("map_value_pin_absent") is True
                and current.get("map_value_pin_absent") is True and initial.get("pure") is (operation == "contains")
                and current.get("pure") is (operation == "contains"))
            and (not length_result or initial.get("length_contract_verified") is True
                and current.get("length_contract_verified") is True and initial.get("map_value_pin_absent") is True
                and current.get("map_value_pin_absent") is True and initial.get("pure") is True and current.get("pure") is True
                and initial.get("map_connection_role") == current.get("map_connection_role") == "target_input")
            and initial["function_name"] == {"clear": "Map_Clear", "find": "Map_Find", "add": "Map_Add", "contains": "Map_Contains", "remove": "Map_Remove", "keys": "Map_Keys", "values": "Map_Values", "length": "Map_Length"}.get(operation)
            and all(initial.get(key) == current.get(key) for key in stable)
            and initial["native_source"].get("observations_complete") is True
            and initial["native_source"].get("native_identity_verified") is True
            and (initial["native_source"].get("struct_path") == "/Script/CoreUObject.Vector" if not double_map else
                initial["native_source"].get("struct_path") == "" and initial["native_source"].get("scalar_property_class") == "DoubleProperty"
                and initial["native_source"].get("scalar_category") == "real" and initial["native_source"].get("scalar_subcategory") == "double"
                and initial["native_source"].get("size") == 8 and initial["native_source"].get("alignment") == 8)
            and bool(uuid.UUID(initial["node_guid"]).int))
        producer, producer_after = plan["map_producer_member_plan"], final["map_producer_member_plan"]
        member_keys = ("member_scope", "native_string_double_map_member" if double_map else "native_string_vector_map_member", "variable_guid", "property_path", "property_owner_class_path",
            "property_flags_string", "expected_pin_type_text", "pin_type", "key_property_path", "key_property_flags_string",
            "inner_property_path", "inner_property_flags_string", "stored_default_before", "stored_default_after",
            "native_value_source_before", "native_value_source_after", "native_value_source_observed_unchanged")
        if double_map and producer.get("member_scope") == "local_native_string_double_map":
            from .graph_variables import _local_map_plan, _LOCAL_MAP_IDENTITY
            _local_map_plan(producer, source_pin, False, "")
            _local_map_plan(producer_after, source_pin, False, "")
            eligible = (eligible and producer["graph_path"] == before["graph_path"]
                and all(producer.get(key) == producer_after.get(key) for key in _LOCAL_MAP_IDENTITY))
        else:
            eligible = (eligible and producer.get("success") is True and producer_after.get("success") is True
                and producer.get("member_scope") == ("own_native_string_double_map" if double_map else "own_native_string_vector_map")
                and producer.get("native_string_double_map_member" if double_map else "native_string_vector_map_member") is True
                and producer.get("native_value_source_observed_unchanged") is True
                and all(producer.get(key) == producer_after.get(key) for key in member_keys))
        old_pins, new_pins = initial["pins"], current["pins"]
        old_rows = {(row["name"], row["direction"]): row for row in old_pins}
        new_rows = {(row["name"], row["direction"]): row for row in new_pins}
        eligible = eligible and len(old_rows) == len(old_pins) and len(new_rows) == len(new_pins) and old_rows.keys() == new_rows.keys()
        names = [("TargetMap", "input")]
        if array_result:
            output_name = "Keys" if operation == "keys" else "Values"
            names.append((output_name, "output"))
            roles = {("TargetMap", "input"), (output_name, "output"), ("self", "input"), ("execute", "input"), ("then", "output")}
            eligible = (eligible and old_rows.keys() == roles
                and initial["native_source"].get("array_inner_class") == "IntProperty"
                and initial["native_source"].get("array_inner_direct_owner_slot_verified") is True
                and initial["native_source"].get("parameter_count") == 2
                and initial["native_function"].get("key_param") == ("Keys" if operation == "keys" else "")
                and initial["native_function"].get("value_param") == ("Values" if operation == "values" else ""))
        elif operation not in ("clear", "length"):
            names.append(("Key", "input"))
            if not key_only:
                names.append(("Value", "output" if operation == "find" else "input"))
        if length_result:
            roles = {("TargetMap", "input"), ("self", "input"), ("ReturnValue", "output")}
            integer = old_rows[("ReturnValue", "output")]
            integer_type = {"category": "int", "subcategory": "None", "type_object": "", "container_type": 0,
                "is_reference": False, "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False,
                "serialize_single_precision": False, "terminal": {"category": "None", "subcategory": "None",
                    "type_object": "", "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False}}
            eligible = (eligible and old_rows.keys() == roles and integer["pin_type"] == integer_type
                and integer["raw_default_value"] == "0" and integer["autogenerated_default_value"] == "0"
                and integer["default_object_path"] == "" and integer["default_text_empty"] is True
                and initial["native_function"].get("key_param") == "" and initial["native_function"].get("value_param") == ""
                and initial["native_source"].get("parameter_count") == 2)
        if key_only:
            roles = {("TargetMap", "input"), ("Key", "input"), ("self", "input"), ("ReturnValue", "output")}
            if operation == "remove":
                roles |= {("execute", "input"), ("then", "output")}
            boolean = old_rows[("ReturnValue", "output")]
            boolean_type = {"category": "bool", "subcategory": "None", "type_object": "", "container_type": 0,
                "is_reference": False, "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False,
                "serialize_single_precision": False, "terminal": {"category": "None", "subcategory": "None",
                    "type_object": "", "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False}}
            eligible = (eligible and old_rows.keys() == roles and boolean["pin_type"] == boolean_type
                and boolean["raw_default_value"] == "false" and boolean["autogenerated_default_value"] == "false"
                and boolean["default_object_path"] == "" and boolean["default_text_empty"] is True
                and initial["native_function"].get("value_param") == ""
                and initial["native_function"].get("key_param") == "Key")
        normalized = copy.deepcopy(after)
        for identity, old in old_rows.items():
            new = new_rows[identity]
            if identity not in names:
                eligible = eligible and old == new
                continue
            name, direction = identity
            expected = copy.deepcopy(old["pin_type"])
            eligible = (eligible and expected["category"] == "wildcard" and expected["subcategory"] == "None"
                and expected["type_object"] == "" and expected["is_weak_pointer"] is False
                and expected["is_uobject_wrapper"] is False and expected["serialize_single_precision"] is False
                and expected["terminal"]["is_const"] is False and expected["terminal"]["is_weak_pointer"] is False
                and expected["terminal"]["is_uobject_wrapper"] is False)
            expected.update(category=("real" if double_map else "struct") if name in ("Value", "Values") else "string", subcategory="double" if double_map and name in ("Value", "Values") else "None", type_object="/Script/CoreUObject.Vector" if name in ("Value", "Values") and not double_map else "")
            if name == "TargetMap":
                eligible = (eligible and expected["container_type"] == 3 and expected["is_reference"] is True
                    and expected["is_const"] is True and expected["terminal"]["category"] == "wildcard"
                    and expected["terminal"]["subcategory"] == "None" and expected["terminal"]["type_object"] == "")
                expected["terminal"].update(category="real" if double_map else "struct", subcategory="double" if double_map else "None", type_object="" if double_map else "/Script/CoreUObject.Vector")
            else:
                reference = name == "Key" or operation == "add"
                eligible = (eligible and expected["container_type"] == (1 if array_result else 0) and expected["is_reference"] is reference
                    and expected["is_const"] is reference)
            if array_result and name != "TargetMap":
                empty_terminal = {"category": "None", "subcategory": "None", "type_object": "",
                    "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False}
                eligible = (eligible and expected["terminal"] == empty_terminal
                    and old["projected_category"] == new["projected_category"] == "array"
                    and type(old["flags"]) is int and old["flags"] & (1 | 2 | 4 | 8 | 16 | 32 | 256) == 0)
            immutable = ("name", "direction", "pin_id", "flags", "autogenerated_default_value", "default_object_path", "default_text_empty")
            eligible = (eligible and all(old[key] == new[key] for key in immutable)
                and bool(uuid.UUID(old["pin_id"]).int) and old["default_object_path"] == "" and old["default_text_empty"] is True
                and new["pin_type"] == expected
                and new["raw_default_value"] in (old["raw_default_value"], old["autogenerated_default_value"]))
            b, a = _pin_row(before, target_id, name, direction), _pin_row(after, target_id, name, direction)
            eligible = (eligible and b.get("native_pin_type_text") == old["pin_type_text"] and a.get("native_pin_type_text") == new["pin_type_text"]
                and b.get("type_display") == old["pin_type_display"] and a.get("type_display") == new["pin_type_display"]
                and b.get("category") == old["projected_category"] and a.get("category") == new["projected_category"]
                and b.get("value") == old["raw_default_value"] and a.get("value") == new["raw_default_value"])
            for key in ("native_pin_type_text", "type_display", "category", "value"):
                _pin_row(normalized, target_id, name, direction)[key] = b[key]
        eligible = (eligible and set(names).issubset(old_rows)
            and not _link_keys(_pin_row(before, target_id, target_pin, "input"))
            and _link_keys(_pin_row(after, target_id, target_pin, "input")) == {(source_id, source_pin)}
            and _nonlink_state(normalized) == _nonlink_state(before))
    except (KeyError, TypeError, ValueError, AttributeError, OperationError):
        eligible = False
    receipt.update(verified=bool(eligible), before=initial, after=current,
        scope="First own String/double Map producer with exact DoubleProperty terminal; no float coercion" if double_map else "First own String/Vector Map producer to exact finite Map8.TargetMap only; full types/identity/flags/Auto and every other observed nonlink field preserved; schema reset-to-Auto defaults only")
    return bool(eligible), receipt


def _string1_row_observation(plan, snapshot, source_id, source_pin, target_id, target_pin):
    """Validate copied current native roles only; this function calls no SDK."""
    obs = plan.get("row_connection_observation")
    scalar = plan.get("connection_scope") == "scalar_uds_datatable_row"
    if (plan.get("connection_scope") not in ("string1_datatable_row", "scalar_uds_datatable_row") or type(obs) is not dict
            or obs.get("eligible") is not True or obs.get("native_source_current_verified") is not True
            or plan.get("exact_standard_k2_schema") is not True):
        raise ValueError("missing current Row/Break native roles")
    role = obs["connection_role"]
    if role not in ("row_result_to_break", "datatable_input"):
        raise ValueError("unknown Row connection role")
    def guid(value):
        parsed = uuid.UUID(value)
        if not parsed.int:
            raise ValueError("zero GUID")
        return parsed.hex
    row_path, break_path = obs["row_node_path"], obs["break_node_path"]
    nodes = {row["node_id"]: row for row in snapshot["nodes"]}
    if (nodes[row_path]["class"] != "K2Node_GetDataTableRow" or nodes[break_path]["class"] != "K2Node_BreakStruct"
            or guid(nodes[row_path]["node_guid"]) != guid(obs["row_node_guid"])
            or guid(nodes[break_path]["node_guid"]) != guid(obs["break_node_guid"])
            or nodes[row_path]["title"] != obs["row_node_title"]
            or obs["source_node_path"] != source_id or obs["target_node_path"] != target_id
            or guid(nodes[source_id]["node_guid"]) != guid(obs["source_node_guid"])
            or guid(nodes[target_id]["node_guid"]) != guid(obs["target_node_guid"])):
        raise ValueError("native/public node role differs")
    for key in (("row_node_guid", "break_node_guid", "struct_guid") if scalar else ("row_node_guid", "break_node_guid", "struct_guid", "field_guid")):
        if not uuid.UUID(obs[key]).int:
            raise ValueError("zero GUID")
    if (type(obs["row_struct_source"]) is not dict or not obs["row_struct_source"]
            or not obs["row_struct_path"].startswith("/Game/")
            or (not scalar and obs["property_name"] != obs["actual_output_pin_name"])):
        raise ValueError("missing authored UDS source/property role")
    def public_pin(pin, path):
        guid = uuid.UUID(pin["pin_id"])
        if not guid.int or type(pin["flags"]) is not int or not 0 <= pin["flags"] < 1024:
            raise ValueError("invalid physical pin identity/flags")
        public = _pin_row(snapshot, path, pin["name"], pin["direction"])
        if (public.get("native_pin_type_text") != pin["pin_type_text"]
                or public.get("type_display") != pin["pin_type_display"]
                or public.get("category") != pin["projected_category"]
                or public.get("value") != pin["raw_default_value"]):
            raise ValueError("native/public physical pin descriptor or default differs")
        return guid.hex
    inventories = []
    for key, path in (("row_pins", row_path), ("break_pins", break_path)):
        rows = obs[key]
        values = {(pin["name"], pin["direction"]): pin for pin in rows}
        if not rows or len(values) != len(rows):
            raise ValueError("duplicate/empty physical pin inventory")
        ids = set()
        for pin in rows:
            guid = public_pin(pin, path)
            if scalar:
                uuid.UUID(pin["persistent_guid"])
            if guid in ids:
                raise ValueError("duplicate physical pin identity")
            ids.add(guid)
        inventories.append(values)
    row_pins, break_pins = inventories
    output, input_pin = obs["row_result_pin"], obs["break_input_pin"]
    if (row_pins[("ReturnValue", "output")] != output
            or break_pins[(input_pin["name"], "input")] != input_pin
            or output["direction"] != "output" or input_pin["direction"] != "input"):
        raise ValueError("endpoint physical pin role differs")
    def plain(value):
        terminal = value["terminal"]
        return (value["container_type"] == 0 and value["subcategory"] == "None"
            and value["is_weak_pointer"] is False and value["is_uobject_wrapper"] is False
            and value["serialize_single_precision"] is False
            and terminal == {"category": "None", "subcategory": "None", "type_object": "",
                "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False})
    rt, bt = output["pin_type"], input_pin["pin_type"]
    pending = obs["specialization_pending"]
    if (type(pending) is not bool or not plain(rt) or not plain(bt)
            or rt["is_reference"] is not False or rt["is_const"] is not False
            or bt["category"] != "struct" or bt["type_object"] != obs["row_struct_path"]
            or bt["is_reference"] is not True or bt["is_const"] is not True
            or rt["category"] != ("wildcard" if pending else "struct")
            or rt["type_object"] != ("" if pending else obs["row_struct_path"])):
        raise ValueError("row value / break constref descriptor differs")
    if scalar:
        from .struct_breaks import _scalar_field_inventory
        _scalar_field_inventory(obs, obs["break_pins"], obs["row_struct_source"])
        if uuid.UUID(obs["row_struct_source"]["struct_guid"]) != uuid.UUID(obs["struct_guid"]):
            raise ValueError("scalar source struct GUID differs")
    else:
        field = break_pins[(obs["actual_output_pin_name"], "output")]["pin_type"]
        if not plain(field) or field["category"] != "string" or field["type_object"] != "":
            raise ValueError("authored GUID output is not FString")
    linked = plan["already_linked"]
    if (type(linked) is not bool or obs["first_connection"] is not (not linked) or obs["actual_link_verified"] is not linked
            or obs["row_break_link_verified"] is not (not pending)
            or (role == "row_result_to_break" and (source_id != row_path or source_pin != "ReturnValue"
                or target_id != break_path or target_pin != input_pin["name"] or pending is linked))
            or (role == "datatable_input" and (target_id != row_path or target_pin != "DataTable" or pending))):
        raise ValueError("first-link/current type role differs")
    for pin, name, direction, path, type_key in (
            (obs["source_pin"], source_pin, "output", source_id, "source_pin_type"),
            (obs["target_pin"], target_pin, "input", target_id, "target_pin_type")):
        if pin["name"] != name or pin["direction"] != direction or pin["pin_type"] != plan[type_key]:
            raise ValueError("requested endpoint identity/type differs")
        public_pin(pin, path)
        if path in (row_path, break_path):
            inventory = row_pins if path == row_path else break_pins
            if inventory[(name, direction)] != pin:
                raise ValueError("requested endpoint differs from current physical inventory")
    if role == "datatable_input":
        for value in (plan["source_pin_type"], plan["target_pin_type"]):
            if value["category"] != "object" or value["container_type"] != 0 or value["type_object"] != "/Script/Engine.DataTable":
                raise ValueError("exact typed DataTable wire required")
    return obs


def _string1_row_projection(before, after, source_id, source_pin, target_id, target_pin, plan, final):
    """Project SDK Row result type cells or one locale-aware DataTable title only."""
    receipt = {"verified": False, "runtime_row_read_verified": False}
    try:
        initial = _string1_row_observation(plan, before, source_id, source_pin, target_id, target_pin)
        current = _string1_row_observation(final, after, source_id, source_pin, target_id, target_pin)
        stable = ("connection_role", "row_node_path", "row_node_guid", "break_node_path", "break_node_guid",
            "source_node_path", "source_node_guid", "target_node_path", "target_node_guid", "row_struct_path",
            "struct_guid", "row_struct_source")
        scalar = plan.get("connection_scope") == "scalar_uds_datatable_row"
        stable += (("scalar_uds_graph_profile", "scalar_uds_field_count", "scalar_fields", "scalar_default_values_json")
            if scalar else ("field_guid", "property_name", "actual_output_pin_name"))
        if plan.get("connection_scope") != final.get("connection_scope"):
            raise ValueError("Row profile changed across connection")
        existing = plan.get("already_linked") is True
        eligible = (all(initial[key] == current[key] for key in stable)
            and final.get("already_linked") is True
            and (existing or (plan.get("direct_connection_allowed") is True and plan.get("schema_response_name") == "make")))
        normalized = copy.deepcopy(after)
        for key in ("row_pins", "break_pins"):
            old = {(pin["name"], pin["direction"]): pin for pin in initial[key]}
            new = {(pin["name"], pin["direction"]): pin for pin in current[key]}
            eligible = eligible and old.keys() == new.keys()
            for identity, value in old.items():
                actual = new[identity]
                changes = not existing and initial["connection_role"] == "row_result_to_break" and key == "row_pins" and identity == ("ReturnValue", "output")
                if not changes:
                    eligible = eligible and value == actual
                    continue
                expected_type = dict(value["pin_type"], category="struct", type_object=initial["row_struct_path"])
                expected = dict(value, pin_type=expected_type, pin_type_text=actual["pin_type_text"],
                    pin_type_display=actual["pin_type_display"], projected_category=actual["projected_category"])
                eligible = eligible and expected == actual
                public_old = _pin_row(before, initial["row_node_path"], "ReturnValue", "output")
                public_after = _pin_row(normalized, initial["row_node_path"], "ReturnValue", "output")
                for field in ("native_pin_type_text", "type_display", "category"):
                    public_after[field] = public_old[field]
        if not existing and initial["connection_role"] == "datatable_input":
            row = next(node for node in normalized["nodes"] if node["node_id"] == initial["row_node_path"])
            row["title"] = initial["row_node_title"]
        else:
            eligible = eligible and initial["row_node_title"] == current["row_node_title"]
        # Producer endpoints can be outside the Row/Break inventories. Their
        # copied full native fields must remain exact across this connection.
        for key in ("source_pin", "target_pin"):
            if existing or initial["connection_role"] == "datatable_input" or key == "target_pin":
                eligible = eligible and initial[key] == current[key]
        eligible = (eligible and (existing or not _link_keys(_pin_row(before, target_id, target_pin, "input")))
            and _link_keys(_pin_row(after, target_id, target_pin, "input")) == {(source_id, source_pin)}
            and (existing or initial["connection_role"] != "row_result_to_break"
                or not _link_keys(_pin_row(before, source_id, source_pin, "output")))
            and _nonlink_state(normalized) == _nonlink_state(before))
    except (KeyError, TypeError, ValueError, AttributeError, StopIteration, OperationError):
        eligible = False
    receipt.update(verified=bool(eligible), before=plan.get("row_connection_observation"),
        after=final.get("row_connection_observation"), scope=("Exact bounded scalar UDS whole authored GUID/full PinType/output inventory; only Row result category/typeobject or first typed DataTable SDK title changes; source/defaults/all other full pins preserved" if plan.get("connection_scope") == "scalar_uds_datatable_row" else "Exact String1 UDS Row/Break roles; only Row result category/typeobject or first typed DataTable SDK title changes; Auto/default/flags/full terminal/other nodes preserved"))
    return bool(eligible), receipt


def _asset_data_break_observation(plan, snapshot, source_id, source_pin, target_id, target_pin):
    """Only copied current native metadata; no SDK or runtime struct value read."""
    obs = plan.get("asset_data_break_observation")
    if (plan.get("connection_scope") != "native_asset_data_break" or type(obs) is not dict
            or obs.get("eligible") is not True or obs.get("native_source_current_verified") is not True
            or plan.get("exact_standard_k2_schema") is not True):
        raise ValueError("missing current AssetData Break roles")
    def guid(value):
        parsed = uuid.UUID(value)
        if not parsed.int:
            raise ValueError("zero GUID")
        return parsed.hex
    nodes = {node["node_id"]: node for node in snapshot["nodes"]}
    path = obs["node_path"]
    if (obs["node_class"] != "/Script/BlueprintGraph.K2Node_BreakStruct"
            or nodes[path]["class"] != "K2Node_BreakStruct"
            or guid(nodes[path]["node_guid"]) != guid(obs["node_guid"])
            or obs["source_node_path"] != source_id or obs["target_node_path"] != target_id
            or guid(nodes[source_id]["node_guid"]) != guid(obs["source_node_guid"])
            or guid(nodes[target_id]["node_guid"]) != guid(obs["target_node_guid"])
            or path not in (source_id, target_id) or type(obs["native_source"]) is not dict or not obs["native_source"]):
        raise ValueError("native/public node or source role differs")
    def pin_current(pin, node_path):
        guid(pin["pin_id"])
        if type(pin["flags"]) is not int or not 0 <= pin["flags"] < 1024:
            raise ValueError("invalid physical flags")
        public = _pin_row(snapshot, node_path, pin["name"], pin["direction"])
        if (public.get("native_pin_type_text") != pin["pin_type_text"]
                or public.get("type_display") != pin["pin_type_display"]
                or public.get("category") != pin["projected_category"]
                or public.get("value") != pin["raw_default_value"]
                or type(pin["default_text_empty"]) is not bool
                or type(pin["autogenerated_default_value"]) is not str
                or type(pin["default_object_path"]) is not str):
            raise ValueError("native/public physical pin differs")
    pins = obs["break_pins"]
    inventory = {(pin["name"], pin["direction"]): pin for pin in pins}
    if (not pins or len(inventory) != len(pins)
            or len({guid(pin["pin_id"]) for pin in pins}) != len(pins)
            or len(nodes[path]["pins"]) != len(pins)):
        raise ValueError("whole physical inventory differs")
    for pin in pins:
        pin_current(pin, path)
    fields = obs["exposed_fields"]
    names = {field["property_name"] for field in fields}
    if (len(fields) != 5 or names != {"PackageName", "PackagePath", "AssetName", "AssetClass", "AssetClassPath"}
            or type(obs["advanced_pin_display"]) is not int):
        raise ValueError("whole BlueprintVisible field inventory differs")
    expected = {(obs["actual_input_pin_name"], "input")}
    for field in fields:
        if (not field["property_path"] or not field["property_identity"] or field["blueprint_visible"] is not True
                or any(type(field[key]) is not bool for key in ("deprecated", "show_pin", "can_toggle_visibility", "marked_advanced"))):
            raise ValueError("missing current native property/optional role")
        if field["show_pin"]:
            pin = field["pin"]
            if (field["pin_name"] != field["property_name"] or pin["name"] != field["pin_name"]
                    or pin["direction"] != "output" or guid(pin["pin_id"]) != guid(field["pin_id"])
                    or inventory[(pin["name"], "output")] != pin
                    or type(field.get("physical_advanced")) is not bool
                    or bool(pin["flags"] & 16) is not field["physical_advanced"]):
                raise ValueError("native property-to-physical output differs")
            expected.add((pin["name"], "output"))
        elif any(key in field for key in ("pin", "pin_name", "pin_id", "physical_advanced")):
            raise ValueError("hidden optional property invented a physical output")
    if inventory.keys() != expected:
        raise ValueError("extra/missing physical pin")
    value = inventory[(obs["actual_input_pin_name"], "input")]["pin_type"]
    terminal = {"category":"None", "subcategory":"None", "type_object":"",
        "is_const":False, "is_weak_pointer":False, "is_uobject_wrapper":False}
    if (value["category"] != "struct" or value["subcategory"] != "None"
            or value["type_object"] != "/Script/CoreUObject.AssetData" or type(value["container_type"]) is not int or value["container_type"] != 0
            or value["is_reference"] is not True or value["is_const"] is not True
            or value["is_weak_pointer"] is not False or value["is_uobject_wrapper"] is not False
            or value["serialize_single_precision"] is not False or value["terminal"] != terminal):
        raise ValueError("exact stock native constref input required")
    for pin, name, direction, endpoint, type_key in (
            (obs["source_pin"], source_pin, "output", source_id, "source_pin_type"),
            (obs["target_pin"], target_pin, "input", target_id, "target_pin_type")):
        pin_current(pin, endpoint)
        if pin["name"] != name or pin["direction"] != direction or pin["pin_type"] != plan[type_key]:
            raise ValueError("requested endpoint/type differs")
        if endpoint == path and inventory[(name, direction)] != pin:
            raise ValueError("requested endpoint differs from whole Break inventory")
    linked = plan["already_linked"]
    if (type(linked) is not bool or obs["actual_link_verified"] is not linked
            or obs["first_connection"] is not (not linked)):
        raise ValueError("actual request link role differs")
    return obs


def _asset_data_connection_preserved(before, after, source_id, source_pin, target_id, target_pin, plan, final):
    """Whole native roles stay exact; reuse only the existing Name auto-None reset."""
    receipt = {"verified": False, "runtime_asset_data_values_verified": False}
    try:
        initial = _asset_data_break_observation(plan, before, source_id, source_pin, target_id, target_pin)
        current = _asset_data_break_observation(final, after, source_id, source_pin, target_id, target_pin)
        expected = copy.deepcopy(initial)
        expected.update(actual_link_verified=True, first_connection=False)
        normalized = copy.deepcopy(after)
        reset = False
        old, new = initial["target_pin"], current["target_pin"]
        # The pre-existing Name input default-reset contract is value-only and
        # independently closed by the two fresh native plans already obtained.
        source_type, target_type = plan["source_pin_type"], plan["target_pin_type"]
        exact_name = all(value.get("category") == "name" and value.get("subcategory") == "None"
            and value.get("container_type") == 0 and value.get("is_const") is False
            and value.get("is_reference") is False and value.get("type_object") == ""
            for value in (source_type, target_type))
        state_before, state_after = plan["target_pin_default_state"], final["target_pin_default_state"]
        if (plan["already_linked"] is False and target_id != initial["node_path"] and exact_name
                and state_before["is_orphaned"] is False and state_before["default_value_is_ignored"] is False
                and state_before["default_value_is_read_only"] is False and state_before["default_object_path"] == ""
                and state_before["default_text_empty"] is True and type(old["raw_default_value"]) is str
                and bool(old["raw_default_value"]) and old["raw_default_value"] != "None"
                and old["autogenerated_default_value"] == "None"
                and state_before["raw_default_value"] == old["raw_default_value"]
                and state_before["autogenerated_default_value"] == "None"
                and state_after == dict(state_before, raw_default_value="None")
                and new == dict(old, raw_default_value="None")):
            expected["target_pin"] = copy.deepcopy(new)
            _pin_row(normalized, target_id, target_pin, "input")["value"] = old["raw_default_value"]
            reset = True
        eligible = (expected == current and final["already_linked"] is True
            and (plan["already_linked"] or (plan["direct_connection_allowed"] is True and plan["schema_response_name"] == "make"))
            and plan["source_pin_type"] == final["source_pin_type"] and plan["target_pin_type"] == final["target_pin_type"]
            and _nonlink_state(normalized) == _nonlink_state(before))
    except (KeyError, ValueError, TypeError, AttributeError, OperationError):
        eligible, reset = False, False
    receipt.update(verified=bool(eligible), name_target_default_reset_verified=bool(eligible and reset),
        before=plan.get("asset_data_break_observation"), after=final.get("asset_data_break_observation"),
        scope="Exact native whole AssetData Break/source/property/optional/pin roles and unchanged type/title/default fields; only existing external scalar Name raw default may reset to preobserved auto None; whole reciprocal edge delta is checked separately")
    return bool(eligible), receipt


def _validity_macro_observation(plan, snapshot, source_id, source_pin, target_id, target_pin):
    """Read copied native stock roles; never a caller-supplied approval token."""
    from .validity_macros import _macro_observation, _guid
    obs = plan.get("is_valid_exec_macro_observation")
    if (plan.get("connection_scope") != "native_is_valid_exec_macro"
            or plan.get("exact_standard_k2_schema") is not True
            or plan.get("applicable") is not True or plan.get("current_verified") is not True
            or any(plan.get(key) is not False for key in ("mutation_attempted", "saved", "compiled", "loaded_during_read"))
            or type(obs) is not dict or obs.get("eligible") is not True
            or obs.get("native_source_current_verified") is not True):
        raise ValueError("missing current native validity macro connection roles")
    _macro_observation(obs)
    nodes = {node["node_id"]:node for node in snapshot["nodes"]}
    path = obs["node_path"]
    if (path not in (source_id, target_id) or source_id == target_id
            or nodes[path]["class"] != "K2Node_MacroInstance"
            or _guid(nodes[path]["node_guid"]) != _guid(obs["node_guid"])
            or obs["source_node_path"] != source_id or obs["target_node_path"] != target_id
            or _guid(nodes[source_id]["node_guid"]) != _guid(obs["source_node_guid"])
            or _guid(nodes[target_id]["node_guid"]) != _guid(obs["target_node_guid"])
            or len(nodes[path]["pins"]) != len(obs["macro_pins"])):
        raise ValueError("current macro/source/target identity differs")
    instances = {value["node_path"]:value for value in obs["macro_instances"]}
    if not instances.keys() <= {source_id, target_id}:
        raise ValueError("macro instance lies outside the current request")
    for endpoint in (source_id, target_id):
        if nodes[endpoint]["class"] == "K2Node_MacroInstance" and endpoint not in instances:
            raise ValueError("current macro endpoint has no verified stock source row")
    for endpoint, instance in instances.items():
        if (nodes[endpoint]["class"] != "K2Node_MacroInstance"
                or _guid(nodes[endpoint]["node_guid"]) != _guid(instance["node_guid"])
                or len(nodes[endpoint]["pins"]) != len(instance["macro_pins"])):
            raise ValueError("current whole macro instance differs")
    def current_pin(pin, endpoint):
        _guid(pin["pin_id"])
        public = _pin_row(snapshot, endpoint, pin["name"], pin["direction"])
        if any(public.get(a) != pin[b] for a,b in (("native_pin_type_text","pin_type_text"),
                ("type_display","pin_type_display"), ("category","projected_category"), ("value","raw_default_value"))):
            raise ValueError("native/public current macro endpoint differs")
    for endpoint, instance in instances.items():
        for pin in instance["macro_pins"]:
            current_pin(pin, endpoint)
    for key,name,direction,endpoint,type_key in (("source_pin",source_pin,"output",source_id,"source_pin_type"),
            ("target_pin",target_pin,"input",target_id,"target_pin_type")):
        pin = obs[key]
        current_pin(pin, endpoint)
        if pin["name"] != name or pin["direction"] != direction or pin["pin_type"] != plan[type_key]:
            raise ValueError("requested current endpoint differs")
        if endpoint in instances:
            inventory = {(value["name"],value["direction"]):value for value in instances[endpoint]["macro_pins"]}
            if inventory[(name,direction)] != pin:
                raise ValueError("endpoint is outside its whole source-derived macro inventory")
    role = obs.get("role")
    if (role == "object_input" and (target_id not in instances or obs["target_pin"]["pin_type"]["category"] != "object")
            or role == "exec_input" and (target_id not in instances or obs["target_pin"]["pin_type"]["category"] != "exec")
            or role == "exec_output" and (source_id not in instances or obs["source_pin"]["pin_type"]["category"] != "exec")
            or role not in ("object_input", "exec_input", "exec_output")):
        raise ValueError("request is not an observed stock macro role")
    linked = plan["already_linked"]
    if (type(linked) is not bool or obs.get("actual_link_verified") is not linked
            or obs.get("first_connection") is not (not linked)):
        raise ValueError("current reciprocal macro edge role differs")
    return obs


def _validity_macro_preserved(before, after, source_id, source_pin, target_id, target_pin, plan, final):
    receipt = {"verified":False, "runtime_validity_verified":False}
    try:
        initial = _validity_macro_observation(plan,before,source_id,source_pin,target_id,target_pin)
        current = _validity_macro_observation(final,after,source_id,source_pin,target_id,target_pin)
        stable = ("node_path", "node_guid", "node_class", "native_source", "target_context", "macro_pins", "macro_instances",
            "source_node_path", "source_node_guid", "target_node_path", "target_node_guid", "source_pin", "target_pin", "role")
        verified = (all(initial[key] == current[key] for key in stable) and final["already_linked"] is True
            and (plan["already_linked"] or plan["direct_connection_allowed"] is True and plan["schema_response_name"] == "make")
            and _nonlink_state(before) == _nonlink_state(after))
    except (KeyError,TypeError,ValueError,AttributeError,StopIteration,OperationError):
        verified = False
    receipt.update(verified=bool(verified), before=plan.get("is_valid_exec_macro_observation"),
        after=final.get("is_valid_exec_macro_observation"),
        scope="Whole source-derived typed macro ports and current source/context preserved; only the requested reciprocal edge is added")
    return bool(verified), receipt


def _datatable_select_is_valid_object_consumer(obs, external_type):
    """Accept only the copied current native IsValid.Object parameter role."""
    source = obs["native_source"]
    if not any(key in obs for key in ("consumer_role", "is_valid_object_consumer")) and "is_valid_object_consumer" not in source:
        return False
    function = "/Script/Engine.KismetSystemLibrary:IsValid"
    owner = "/Script/Engine.KismetSystemLibrary"
    receipt = obs.get("is_valid_object_consumer")
    native = source.get("is_valid_object_consumer")
    if (obs.get("role") != "table_return" or obs.get("consumer_role") != "native_is_valid_object"
            or type(receipt) is not dict or type(native) is not dict
            or obs["target_pin"]["name"] != "Object" or obs["target_pin"]["direction"] != "input"
            or receipt.get("node_path") != obs["target_node_path"]
            or uuid.UUID(receipt["node_guid"]) != uuid.UUID(obs["target_node_guid"])
            or native.get("function_path") != function or native.get("owner_path") != owner
            or native.get("source_signature_match") is not True
            or native.get("runtime_invocation_verified") is not False
            or native.get("world_context") != ""
            or type(native.get("native_types")) is not dict or not native["native_types"]):
        raise ValueError("missing exact current native IsValid consumer source")
    terminal = {"category":"None", "subcategory":"None", "type_object":"",
        "is_const":False, "is_weak_pointer":False, "is_uobject_wrapper":False}
    expected = {"observation_complete":True, "category":"object", "subcategory":"None",
        "type_object":"/Script/CoreUObject.Object", "container_type":0, "is_reference":False,
        "is_const":True, "is_weak_pointer":False, "is_uobject_wrapper":False,
        "serialize_single_precision":False, "terminal":terminal}
    def exact(value, wanted):
        if type(value) is not type(wanted):
            return False
        if type(wanted) is dict:
            return value.keys() == wanted.keys() and all(exact(value[key], item) for key, item in wanted.items())
        return value == wanted
    def canonical(value):
        value = copy.deepcopy(value)
        if type(value) is not dict or type(value.get("terminal")) is not dict:
            raise ValueError("missing full current native parameter type")
        for part in (value, value["terminal"]):
            # NativeState retains its original weak spelling. Never merge two
            # descriptions or query the SDK to fill a missing parameter role.
            if "is_weak" in part:
                if "is_weak_pointer" in part:
                    raise ValueError("ambiguous native weak-pointer role")
                part["is_weak_pointer"] = part.pop("is_weak")
        return value
    def positive_identity(value):
        return type(value) is str and value.isascii() and value.isdecimal() and int(value) > 0
    parameters = native.get("parameters")
    if type(parameters) is not list or len(parameters) != 2 or any(type(row) is not dict for row in parameters):
        raise ValueError("exact IsValid signature inventory required")
    fields = {row.get("name"):row for row in parameters}
    if len(fields) != 2 or fields.keys() != {"Object", "ReturnValue"}:
        raise ValueError("exact IsValid parameter roles required")
    current, result = fields["Object"], fields["ReturnValue"]
    bool_type = copy.deepcopy(expected)
    bool_type.update(category="bool", type_object="", is_const=False)
    if (receipt.get("function_path") != function or receipt.get("property_name") != "Object"
            or type(receipt.get("property_path")) is not str or not receipt["property_path"]
            or current.get("property_path") != receipt["property_path"]
            or current.get("direction") != "input" or result.get("direction") != "output"
            or not positive_identity(native.get("function_flags_string"))
            or not positive_identity(receipt.get("identity")) or not positive_identity(result.get("identity"))
            or receipt["identity"] == result["identity"] or current.get("identity") != receipt["identity"]
            or not positive_identity(receipt.get("property_flags_string"))
            or current.get("flags_string") != receipt["property_flags_string"]
            or receipt.get("schema_converted_current_parameter") is not True
            or receipt.get("native_reference_current_verified") is not True
            or receipt.get("node_pure_verified") is not True
            or receipt.get("data_table_subtype_compatible") is not True
            or receipt.get("parameter_type_exactly_equals_select_value_type") is not False
            or not exact(receipt.get("pin_type"), expected) or not exact(external_type, expected)
            or not exact(canonical(current.get("type")), expected)
            or not exact(canonical(result.get("type")), bool_type)):
        raise ValueError("current IsValid property/source/endpoint type differs")
    return True


def _datatable_select_observation(plan, snapshot, source_id, source_pin, target_id, target_pin):
    """Validate only freshly copied exact stock Select metadata; no SDK call."""
    obs = plan.get("datatable_select_observation")
    if (plan.get("connection_scope") != "native_datatable_bool_select" or type(obs) is not dict
            or obs.get("eligible") is not True or obs.get("native_source_current_verified") is not True
            or plan.get("exact_standard_k2_schema") is not True):
        raise ValueError("missing current native DataTable Select roles")
    def guid(value):
        parsed = uuid.UUID(value)
        if not parsed.int:
            raise ValueError("zero physical identity")
        return parsed.hex
    nodes = {node["node_id"]: node for node in snapshot["nodes"]}
    path = obs["node_path"]
    if (obs["node_class"] != "/Script/BlueprintGraph.K2Node_Select" or nodes[path]["class"] != "K2Node_Select"
            or guid(nodes[path]["node_guid"]) != guid(obs["node_guid"])
            or obs["source_node_path"] != source_id or obs["target_node_path"] != target_id
            or guid(nodes[source_id]["node_guid"]) != guid(obs["source_node_guid"])
            or guid(nodes[target_id]["node_guid"]) != guid(obs["target_node_guid"])
            or path not in (source_id, target_id)
            or any(type(obs[key]) is not dict or not obs[key] for key in ("native_source", "target_context"))
            or any(type(obs[key]) is not bool for key in ("index_typed", "value_typed"))):
        raise ValueError("current Select/source/target identity differs")
    def pin_matches(pin, endpoint):
        guid(pin["pin_id"])
        if (type(pin["flags"]) is not int or not 0 <= pin["flags"] < 1024
                or type(pin["autogenerated_default_value"]) is not str
                or type(pin["default_object_path"]) is not str or type(pin["default_text_empty"]) is not bool):
            raise ValueError("invalid physical default/identity role")
        public = _pin_row(snapshot, endpoint, pin["name"], pin["direction"])
        if (public.get("native_pin_type_text") != pin["pin_type_text"]
                or public.get("type_display") != pin["pin_type_display"]
                or public.get("category") != pin["projected_category"]
                or public.get("value") != pin["raw_default_value"]):
            raise ValueError("native/public current pin differs")
    def plain(value):
        return (type(value["container_type"]) is int and value["container_type"] == 0
            and value["is_reference"] is False and value["is_const"] is False
            and value["is_weak_pointer"] is False and value["is_uobject_wrapper"] is False
            and value["serialize_single_precision"] is False
            and value["terminal"] == {"category":"None","subcategory":"None","type_object":"",
                "is_const":False,"is_weak_pointer":False,"is_uobject_wrapper":False})
    pins = obs["select_pins"]
    inventory = {(pin["name"],pin["direction"]):pin for pin in pins}
    roles = {("Option 0","input"),("Option 1","input"),("Index","input"),("ReturnValue","output")}
    if (len(pins) != 4 or inventory.keys() != roles or len(nodes[path]["pins"]) != 4
            or len({guid(pin["pin_id"]) for pin in pins}) != 4):
        raise ValueError("whole four-pin Select role differs")
    for identity,pin in inventory.items():
        pin_matches(pin,path)
        value = pin["pin_type"]
        index = identity == ("Index","input")
        typed = obs["index_typed"] if index else obs["value_typed"]
        if (not plain(value) or value["category"] != ("bool" if index else "object") and typed
                or value["category"] != "wildcard" and not typed
                or value["type_object"] != ("/Script/Engine.DataTable" if typed and not index else "")
                or value["subcategory"] != ("index" if index and not typed else "None")):
            raise ValueError("exact Bool/DataTable or initial wildcard type required")
    role = obs["role"]
    if (role not in ("bool_index","table_option","table_return")
            or role == "bool_index" and (target_id != path or target_pin != "Index")
            or role == "table_option" and (target_id != path or target_pin not in ("Option 0","Option 1") or not obs["index_typed"])
            or role == "table_return" and (source_id != path or source_pin != "ReturnValue" or not obs["value_typed"])):
        raise ValueError("request is not an exact Select endpoint")
    for pin,name,direction,endpoint,type_key in (
            (obs["source_pin"],source_pin,"output",source_id,"source_pin_type"),
            (obs["target_pin"],target_pin,"input",target_id,"target_pin_type")):
        pin_matches(pin,endpoint)
        if pin["name"] != name or pin["direction"] != direction or pin["pin_type"] != plan[type_key]:
            raise ValueError("requested native endpoint differs")
        if endpoint == path and inventory[(name,direction)] != pin:
            raise ValueError("endpoint is not in the whole Select inventory")
    external_type = obs["source_pin" if role != "table_return" else "target_pin"]["pin_type"]
    is_valid_consumer = _datatable_select_is_valid_object_consumer(obs, external_type)
    if not is_valid_consumer and (external_type["category"] != ("bool" if role == "bool_index" else "object")
            or external_type["type_object"] != ("" if role == "bool_index" else "/Script/Engine.DataTable")
            or type(external_type["container_type"]) is not int or external_type["container_type"] != 0):
        raise ValueError("only actual Bool or exact DataTable wire producers/consumers admitted")
    linked = plan["already_linked"]
    if (type(linked) is not bool or obs["actual_link_verified"] is not linked
            or obs["first_connection"] is not (not linked)
            or linked and not obs["index_typed" if role == "bool_index" else "value_typed"]):
        raise ValueError("actual request edge role differs")
    return obs


def _datatable_select_projection(before, after, source_id, source_pin, target_id, target_pin, plan, final):
    """Use native source-backed expected semantic rows, never an adopted after baseline."""
    receipt = {"verified":False,"runtime_selection_verified":False}
    try:
        initial = _datatable_select_observation(plan,before,source_id,source_pin,target_id,target_pin)
        current = _datatable_select_observation(final,after,source_id,source_pin,target_id,target_pin)
        stable = ("node_path","node_guid","node_class","native_source","target_context",
            "source_node_path","source_node_guid","target_node_path","target_node_guid","role")
        role,existing = initial["role"],plan["already_linked"]
        transition = not existing and ((role == "bool_index" and not initial["index_typed"])
            or (role in ("table_option","table_return") and not initial["value_typed"]))
        expected_index = initial["index_typed"] or role == "bool_index"
        expected_value = initial["value_typed"] or role in ("table_option","table_return")
        old = {(pin["name"],pin["direction"]):pin for pin in initial["select_pins"]}
        new = {(pin["name"],pin["direction"]):pin for pin in current["select_pins"]}
        def semantics(pin):
            return {key:value for key,value in pin.items() if key != "pin_id"}
        expected = {(pin["name"],pin["direction"]):pin for pin in initial["expected_after_select_pins"]}
        verified = (type(initial["allowed_reconstruction"]) is bool and initial["allowed_reconstruction"] is transition
            and len(expected)==4 and expected.keys()==old.keys()==new.keys()
            and all(initial[key]==current[key] for key in stable)
            and all(initial.get(key)==current.get(key) for key in ("consumer_role", "is_valid_object_consumer"))
            and current["index_typed"] is expected_index and current["value_typed"] is expected_value
            and final["already_linked"] is True
            and (existing or (plan["direct_connection_allowed"] is True and plan["schema_response_name"]=="make")))
        for key,pin in old.items():
            # Expected rows are a before-dispatch native SDK projection, including
            # exact default/Auto/flags/type and locale metadata, without new IDs.
            verified = verified and "pin_id" not in expected[key] and expected[key]==semantics(new[key])
            if not transition:
                verified = verified and pin==new[key]
        for name in ("source_pin","target_pin"):
            endpoint = source_id if name=="source_pin" else target_id
            if endpoint != initial["node_path"]:
                verified = verified and initial[name]==current[name]
        normalized = copy.deepcopy(after)
        old_public = next(node for node in before["nodes"] if node["node_id"]==initial["node_path"])
        new_public = next(node for node in normalized["nodes"] if node["node_id"]==initial["node_path"])
        current_public = {(pin["name"],pin["direction"]):pin for pin in new_public["pins"]}
        normalized_pins = []
        for pin in old_public["pins"]:
            actual = copy.deepcopy(current_public[(pin["name"],pin["direction"])])
            for key in ("category","native_pin_type_text","type_display","value"):
                actual[key] = pin[key]
            normalized_pins.append(actual)
        new_public["pins"] = normalized_pins
        verified = verified and _nonlink_state(normalized)==_nonlink_state(before)
    except (KeyError,TypeError,ValueError,AttributeError,StopIteration,OperationError):
        verified = False
    receipt.update(verified=bool(verified),before=plan.get("datatable_select_observation"),
        after=final.get("datatable_select_observation"),
        scope="Exact four semantic pin roles; only before-projected Bool/DataTable source/type/default/Auto/flags under SDK reconstruction, node GUID/other roles/source/full reciprocal edges preserved")
    return bool(verified),receipt


def _name_foreach_observation(plan, snapshot, source_id, source_pin, target_id, target_pin):
    """Fresh native metadata and copied public values only; never an approval token."""
    obs = plan.get("name_foreach_observation")
    make_array = type(obs) is dict and obs.get("producer_kind") == "name_make_array"
    if type(obs) is dict and "producer_kind" in obs and not make_array:
        raise ValueError("unsupported Name ForEach producer kind")
    if (plan.get("connection_scope") != "native_name_foreach_connection" or type(obs) is not dict
            or obs.get("eligible") is not True or obs.get("native_source_current_verified") is not True
            or obs.get("whole_macro_shape_verified") is not True or plan.get("exact_standard_k2_schema") is not True
            or plan.get("current_verified") is not True or plan.get("applicable") is not True
            or obs.get("role") != "name_array_input" or source_pin != ("Array" if make_array else "OutRowNames") or target_pin != "Array"):
        raise ValueError("missing current exact Name producer/ForEach roles")
    def guid(value):
        value = uuid.UUID(value)
        if not value.int:
            raise ValueError("zero current identity")
        return value.hex
    nodes = {node["node_id"]: node for node in snapshot["nodes"]}
    if (obs["source_node_path"] != source_id or obs["target_node_path"] != target_id
            or obs["macro_node_path"] != target_id
            or obs["macro_node_class"] != "/Script/BlueprintGraph.K2Node_MacroInstance"
            or nodes[target_id]["class"] != "K2Node_MacroInstance" or nodes[source_id]["class"] != ("K2Node_MakeArray" if make_array else "K2Node_CallFunction")
            or guid(nodes[target_id]["node_guid"]) != guid(obs["macro_node_guid"])
            or guid(nodes[target_id]["node_guid"]) != guid(obs["target_node_guid"])
            or guid(nodes[source_id]["node_guid"]) != guid(obs["source_node_guid"])
            or obs["macro_graph_path"] != "/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForEachLoop"
            or any(type(obs[key]) is not dict or not obs[key] for key in ("native_source","target_context"))
            or obs.get("resolved_wildcard_type_cross_plan_equality_required") is not False
            or type(obs.get("resolved_wildcard_type")) is not dict):
        raise ValueError("current producer/macro identity differs")
    native = obs["native_source"]
    if not make_array:
        if (native.get("function_path") != "/Script/Engine.DataTableFunctionLibrary:GetDataTableRowNames"
                or type(native.get("num_parameters")) is not int or native["num_parameters"] != 2
                or type(native.get("properties")) is not list or len(native["properties"]) != 3):
            raise ValueError("exact native producer signature metadata missing")
        if (type(native.get("function_flags_string")) is not str or not native["function_flags_string"].isdigit()
                or any(type(native.get(key)) is not int or native[key] < 0 for key in ("parameter_size","return_value_offset","size","alignment"))
                or any(type(native.get(key)) is not dict or not native[key] for key in ("stock_macro_source","stock_native_calls"))):
            raise ValueError("native producer/source frame incomplete")
    elif native.get("producer_kind") != "name_make_array":
        raise ValueError("exact native MakeArray source kind missing")
    if any(type(native.get(key)) is not dict or not native[key] for key in ("stock_macro_source","stock_native_calls")):
        raise ValueError("stock macro/native source frame incomplete")
    context = obs["target_context"]
    guid(context["graph_guid"])
    if (context["graph_path"] != plan["graph_path"] or any(type(context.get(key)) is not str or not context[key]
            for key in ("blueprint_path","parent_class_path","generated_class_path","skeleton_class_path"))
            or type(context.get("held_class_roles")) is not list or not context["held_class_roles"]):
        raise ValueError("current target class/context frame incomplete")
    for row in context["held_class_roles"]:
        if (type(row.get("class_path")) is not str or not row["class_path"]
                or type(row.get("class_identity")) is not str or not row["class_identity"].isdigit() or int(row["class_identity"]) == 0
                or type(row.get("existing_cdo_identity")) is not str or not row["existing_cdo_identity"].isdigit()
                or type(row.get("existing_cdo_path")) is not str
                or bool(int(row["existing_cdo_identity"])) != bool(row["existing_cdo_path"])):
            raise ValueError("current shallow class/CDO identity frame malformed")
    if not make_array:
        props = native["properties"]
        if (len({prop.get("identity") for prop in props}) != 3 or any(
                type(prop.get("identity")) is not str or not prop["identity"].isdigit() or int(prop["identity"]) == 0
                or type(prop.get("flags_string")) is not str or not prop["flags_string"].isdigit()
                or any(type(prop.get(key)) is not int or prop[key] < 0 for key in ("offset","size","array_dim"))
                or prop["size"] == 0 or prop["array_dim"] != 1 for prop in props)):
            raise ValueError("current native property metadata frame malformed")
        if (sum(prop.get("name")=="Table" and prop.get("kind")=="ObjectProperty" and prop.get("direct_owner_role")=="function" for prop in props) != 1
                or sum(prop.get("name")=="OutRowNames" and prop.get("kind")=="ArrayProperty" and prop.get("direct_owner_role")=="function" for prop in props) != 1
                or sum(prop.get("kind")=="NameProperty" and prop.get("direct_owner_role")=="array" for prop in props) != 1):
            raise ValueError("native parameter and array-child owner roles differ")
    def pin_matches(pin, endpoint):
        guid(pin["pin_id"])
        if (type(pin["flags"]) is not int or not 0 <= pin["flags"] < 1024
                or type(pin["raw_default_value"]) is not str or type(pin["autogenerated_default_value"]) is not str
                or type(pin["default_object_path"]) is not str or type(pin["default_text_empty"]) is not bool
                or type(pin["links"]) is not list):
            raise ValueError("malformed current physical pin")
        public = _pin_row(snapshot, endpoint, pin["name"], pin["direction"])
        if (public.get("native_pin_type_text") != pin["pin_type_text"] or public.get("type_display") != pin["pin_type_display"]
                or public.get("category") != pin["projected_category"] or public.get("value") != pin["raw_default_value"]):
            raise ValueError("native/public full pin values differ")
        links = set()
        for link in pin["links"]:
            guid(link["pin_id"])
            pair = (link["node_path"],link["pin_name"])
            if pair in links:
                raise ValueError("duplicate native link")
            links.add(pair)
        if links != _link_keys(public):
            raise ValueError("native/public current links differ")
    terminal = {"category":"None","subcategory":"None","type_object":"", "is_const":False,"is_weak_pointer":False,"is_uobject_wrapper":False}
    def exact_type(value, category, container):
        return (value["category"] == category and value["subcategory"] == "None" and value["type_object"] == ""
            and type(value["container_type"]) is int and value["container_type"] == container
            and all(value[key] is False for key in ("is_reference","is_const","is_weak_pointer","is_uobject_wrapper","serialize_single_precision"))
            and value["terminal"] == terminal)
    def make_array_output_type(value):
        # Only the two full Name[] output shapes supported by stock reconstruction.
        if value["is_reference"] is False and value["is_const"] is False:
            return exact_type(value,"name",1)
        return (value["is_reference"] is True and value["is_const"] is True
            and exact_type(dict(value,is_reference=False,is_const=False),"name",1))
    if make_array:
        producer = obs.get("make_array_producer")
        if (type(producer) is not dict or producer.get("current_verified") is not True
                or producer.get("node_path") != source_id or guid(producer["node_guid"]) != guid(obs["source_node_guid"])
                or producer.get("node_class") != "/Script/BlueprintGraph.K2Node_MakeArray"
                or type(producer.get("num_inputs")) is not int or not 0 <= producer["num_inputs"] <= 128
                or type(producer.get("pins")) is not list):
            raise ValueError("current exact MakeArray producer frame missing")
        source = producer.get("native_class_source")
        if (type(source) is not dict or source != native.get("native_class_source")
                or source.get("class_path") != "/Script/BlueprintGraph.K2Node_MakeArray"
                or source.get("super_class_path") != "/Script/BlueprintGraph.K2Node_MakeContainer"
                or source.get("package_path") != "/Script/BlueprintGraph"
                or any(source.get(key) is not True for key in ("current_verified","native","class_generated_by_null"))
                or source.get("package_dirty") is not False
                or any(type(source.get(key)) is not str or not source[key].isdigit() or int(source[key]) == 0
                    for key in ("identity","super_class_identity"))
                or any(type(source.get(key)) is not str or not source[key].isdigit()
                    for key in ("class_flags_string","package_flags_string"))
                or any(type(source.get(key)) is not int or source[key] <= 0 for key in ("class_size","class_alignment"))
                or type(source.get("metadata")) is not dict
                or any(type(key) is not str or type(value) is not str for key,value in source["metadata"].items())):
            raise ValueError("current native MakeArray class source differs")
        source_pins = producer["pins"]
        source_roles = {(f"[{index}]","input") for index in range(producer["num_inputs"])} | {("Array","output")}
        actual_source = {(pin["name"],pin["direction"]):pin for pin in source_pins}
        if (len(source_pins) != len(source_roles) or actual_source.keys() != source_roles
                or len(nodes[source_id]["pins"]) != len(source_roles)
                or len({guid(pin["pin_id"]) for pin in source_pins}) != len(source_roles)):
            raise ValueError("whole current MakeArray pin roles differ")
        for (name,direction),pin in actual_source.items():
            pin_matches(pin,source_id)
            if (not (make_array_output_type(pin["pin_type"]) if direction=="output" else exact_type(pin["pin_type"],"name",0))
                    or pin["default_object_path"] != "" or pin["default_text_empty"] is not True):
                raise ValueError("exact Name MakeArray physical type/default role required")
        if obs["source_pin"] != actual_source[("Array","output")]:
            raise ValueError("requested source is not the current whole MakeArray output")
    pins = obs["macro_pins"]
    roles = {("Exec","input"),("Array","input"),("LoopBody","output"),("Array Element","output"),("Array Index","output"),("Completed","output")}
    inventory = {(pin["name"],pin["direction"]):pin for pin in pins}
    if (len(pins) != 6 or inventory.keys() != roles or len(nodes[target_id]["pins"]) != 6
            or len({guid(pin["pin_id"]) for pin in pins}) != 6
            or any(type(obs.get(key)) is not bool for key in ("specialization_pending","actual_link_verified","first_connection","allowed_reconstruction"))):
        raise ValueError("whole six macro roles differ")
    typed = not obs["specialization_pending"]
    for (name,direction),pin in inventory.items():
        pin_matches(pin,target_id)
        category = ("name" if typed else "wildcard") if name in ("Array","Array Element") else "int" if name == "Array Index" else "exec"
        if (pin["flags"] & (1|2|4|8|256) or not exact_type(pin["pin_type"],category,1 if name=="Array" else 0)
                or pin["raw_default_value"] != "" or pin["autogenerated_default_value"] != ""
                or pin["default_object_path"] != "" or pin["default_text_empty"] is not True):
            raise ValueError("exact whole Name/wildcard/default role required")
    for pin,name,direction,endpoint,key in ((obs["source_pin"],source_pin,"output",source_id,"source_pin_type"),
            (obs["target_pin"],target_pin,"input",target_id,"target_pin_type")):
        pin_matches(pin,endpoint)
        if pin["name"] != name or pin["direction"] != direction or pin["pin_type"] != plan[key]:
            raise ValueError("requested full native endpoint differs")
    if obs["target_pin"] != inventory[("Array","input")] or not (make_array_output_type(obs["source_pin"]["pin_type"]) if make_array else exact_type(obs["source_pin"]["pin_type"],"name",1)):
        raise ValueError("actual Name[] producer/whole macro input differs")
    linked = plan["already_linked"]
    if (type(linked) is not bool or obs["actual_link_verified"] is not linked or obs["first_connection"] is not (not linked)
            or linked and not typed or obs["allowed_reconstruction"] is not (not linked and not typed)):
        raise ValueError("actual request link/pending state differs")
    return obs


def _name_foreach_projection(before, after, source_id, source_pin, target_id, target_pin, plan, final, native):
    """Verify before-derived six-pin expectations plus exactly one reciprocal pair."""
    receipt = {"verified":False,"runtime_iteration_verified":False}
    try:
        initial = _name_foreach_observation(plan,before,source_id,source_pin,target_id,target_pin)
        current = _name_foreach_observation(final,after,source_id,source_pin,target_id,target_pin)
        existing = plan["already_linked"]
        reconstructed = False
        if not existing:
            data = native["data"]
            if (native["success"] is not True or native["postcondition_passed"] is not True or native["change_state_known"] is not True
                    or native["mutation_attempted"] is not True or native["engine_return"] is not True
                    or native["compiled"] is not False or native["saved"] is not False
                    or data.get("schema_intermediate_current_verified") is not True or data.get("actual_link_verified") is not True
                    or data.get("expected_macro_semantics_verified") is not True
                    or type(data.get("observed_intermediate_reconstruction_pending")) is not bool
                    or any(type(data.get(key)) is not int or data[key] != expected for key,expected in (
                        ("schema_connection_attempts",1),("schema_connection_returns",1),("node_notification_attempts",1),("node_notification_returns",1),
                        ("expected_macro_pin_count",6),("verified_macro_pin_count",6),("actual_forward_pair_count",1),("actual_reverse_pair_count",1)))
                    or type(data.get("macro_pin_pointer_replacements")) is not int or not 0 <= data["macro_pin_pointer_replacements"] <= 6):
                raise ValueError("native schema/notification actual prefix incomplete")
            reconstructed = data["observed_intermediate_reconstruction_pending"]
            if reconstructed and not initial["allowed_reconstruction"] or not reconstructed and data["macro_pin_pointer_replacements"] != 0:
                raise ValueError("unapproved physical reconstruction")
        stable = ("macro_node_path","macro_node_guid","macro_node_class","macro_graph_path","native_source","target_context",
            "source_node_path","source_node_guid","target_node_path","target_node_guid","role")
        verified = (all(initial[key]==current[key] for key in stable) and current["specialization_pending"] is False
            and final["already_linked"] is True and (existing or plan["direct_connection_allowed"] is True and plan["schema_response_name"]=="make"))
        old = {(pin["name"],pin["direction"]):pin for pin in initial["macro_pins"]}
        new = {(pin["name"],pin["direction"]):pin for pin in current["macro_pins"]}
        key = "expected_after_macro_pins" if reconstructed else "expected_after_macro_pins_without_reconstruction"
        expected = {(pin["name"],pin["direction"]):pin for pin in initial[key]}
        verified = verified and len(expected)==6 and expected.keys()==old.keys()==new.keys()
        def semantics(pin):
            return {key:value for key,value in pin.items() if key not in ("pin_id","links")}
        for role,pin in old.items():
            projected = copy.deepcopy(semantics(pin))
            if role[0] in ("Array","Array Element"):
                projected["pin_type"].update(category="name",subcategory="None",type_object="")
                # Exported type/locale display are native source-backed projections;
                # full type and only these presentation cells are independently bounded.
                for cell in ("pin_type_text","pin_type_display"):
                    projected[cell] = expected[role][cell]
                projected["projected_category"] = "array" if role[0]=="Array" else "string"
            if reconstructed:
                projected.update(flags=pin["flags"]|512,persistent_guid=uuid.UUID(int=0).hex,
                    pin_friendly_name="",pin_tooltip="",deprecation_message="")
            verified = verified and "pin_id" not in expected[role] and "links" not in expected[role]
            verified = verified and projected==expected[role]==semantics(new[role])
            if not reconstructed:
                verified = verified and pin["pin_id"]==new[role]["pin_id"]
        if initial.get("producer_kind") == "name_make_array":
            producer_before,producer_after = initial["make_array_producer"],current["make_array_producer"]
            verified = verified and current.get("producer_kind") == "name_make_array"
            verified = verified and all(producer_before[key]==producer_after[key] for key in
                ("node_path","node_guid","node_class","num_inputs","native_class_source","current_verified"))
            source_before = {(pin["name"],pin["direction"]):pin for pin in producer_before["pins"]}
            source_after = {(pin["name"],pin["direction"]):pin for pin in producer_after["pins"]}
            verified = verified and source_before.keys()==source_after.keys()
            for role,pin in source_before.items():
                verified = verified and semantics(pin)==semantics(source_after[role]) and pin["pin_id"]==source_after[role]["pin_id"]
        elif "producer_kind" in current:
            verified = False
        verified = verified and semantics(initial["source_pin"])==semantics(current["source_pin"])
        verified = verified and initial["source_pin"]["pin_id"]==current["source_pin"]["pin_id"]
        verified = verified and _edges(after)==(_edges(before)|_pair(source_id,source_pin,target_id,target_pin))
        normalized = copy.deepcopy(after)
        old_public = next(node for node in before["nodes"] if node["node_id"]==target_id)
        new_public = next(node for node in normalized["nodes"] if node["node_id"]==target_id)
        public = {(pin["name"],pin["direction"]):pin for pin in new_public["pins"]}
        ordered = []
        for pin in old_public["pins"]:
            actual = copy.deepcopy(public[(pin["name"],pin["direction"])])
            if pin["name"] in ("Array","Array Element"):
                for cell in ("category","native_pin_type_text","type_display"):
                    actual[cell] = pin[cell]
            ordered.append(actual)
        new_public["pins"] = ordered
        verified = verified and _nonlink_state(normalized)==_nonlink_state(before)
    except (KeyError,TypeError,ValueError,AttributeError,StopIteration,OperationError):
        verified = False
    receipt.update(verified=bool(verified),before=plan.get("name_foreach_observation"),after=final.get("name_foreach_observation"),
        scope="Exact native RowNames or Name MakeArray/stock Name ForEach; six current semantic ports, before-derived type/default/Auto/flags, no old raw pins after native notification; source/context/all other nonlink roles and reciprocal edges preserved")
    return bool(verified),receipt


def _connect_action(result, editor, source_node_id, source_pin_name, target_node_id, target_pin_name,
        guard=None, checkpoint_callback=None, *, identity="connect_graph_pins"):
    source_node = api.find_node(editor, source_node_id)
    target_node = api.find_node(editor, target_node_id)
    source_id, target_id = api.node_id(source_node), api.node_id(target_node)
    source = api.find_pin(source_node, source_pin_name, "output")
    target = api.find_pin(target_node, target_pin_name, "input")
    before = copy.deepcopy(api.graph_info(editor, exact_pin_types=True))
    result["data"].update(before=before, replacement_guard_requested=guard is not None,
        replacement_guard_verified=False, expected_existing_link_removed=False, replaced_source_link_count=0,
        observation_scope="loaded K2 nodes and reflected visible pins: GUID/position/native pin type/default/links; comments, hidden pins and arbitrary callbacks are not completely audited")
    try:
        plan = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name)
    except Exception as error:
        native_failure = getattr(error, "native_graph_result", None)
        if type(native_failure) is dict and native_failure.get("connection_scope") in ("native_string_vector_map", "native_string_double_map"):
            result["data"]["native_map_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") in ("string1_datatable_row", "scalar_uds_datatable_row"):
            result["data"]["native_row_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") == "native_asset_data_break":
            result["data"]["native_asset_data_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") == "native_datatable_bool_select":
            result["data"]["native_datatable_select_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") == "native_name_foreach_connection":
            result["data"]["native_name_foreach_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") == "native_is_valid_exec_macro":
            result["data"]["native_validity_macro_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and ("bfl_name_array_context_failure" in native_failure or "bfl_name_array_context" in native_failure):
            result["data"]["native_bfl_name_array_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and ("bfl_target_context" in native_failure
                or type(native_failure.get("dynamic_cast_source_observation")) is dict
                and "bfl_target_context" in native_failure["dynamic_cast_source_observation"]):
            result["data"]["native_bfl_datatable_connection_failure"] = copy.deepcopy(native_failure)
        raise
    result["data"]["connection_plan"] = copy.deepcopy(plan)
    map_connection = plan.get("connection_scope") in ("native_string_vector_map", "native_string_double_map")
    row_connection = plan.get("connection_scope") in ("string1_datatable_row", "scalar_uds_datatable_row")
    asset_data_connection = plan.get("connection_scope") == "native_asset_data_break"
    select_connection = plan.get("connection_scope") == "native_datatable_bool_select"
    validity_macro_connection = plan.get("connection_scope") == "native_is_valid_exec_macro"
    if validity_macro_connection:
        if guard is not None:
            raise OperationError("connection_validity_macro_replacement_unsupported", "Existing exec link replacement is outside this exact stock macro connection scope")
        try:
            _validity_macro_observation(plan, before, source_id, source_pin_name, target_id, target_pin_name)
        except (KeyError, ValueError, TypeError, AttributeError, OperationError) as error:
            raise OperationError("connection_validity_macro_observation_invalid", "Current stock macro/source/full-port plan differs from copied graph roles") from error
    name_foreach_connection = plan.get("connection_scope") == "native_name_foreach_connection"
    if name_foreach_connection:
        try:
            _name_foreach_observation(plan, before, source_id, source_pin_name, target_id, target_pin_name)
        except (KeyError, ValueError, TypeError, AttributeError, OperationError) as error:
            raise OperationError("connection_name_foreach_observation_invalid", "Native whole Name ForEach/source plan differs from copied current graph roles") from error
    if select_connection:
        try:
            _datatable_select_observation(plan, before, source_id, source_pin_name, target_id, target_pin_name)
        except (KeyError, ValueError, TypeError, AttributeError, OperationError) as error:
            raise OperationError("connection_datatable_select_observation_invalid", "Native current Select/source/full-pin plan differs from copied graph roles") from error
    cast_observation = plan.get("dynamic_cast_source_observation")
    bfl_cast_connection = type(cast_observation) is dict and "bfl_target_context" in cast_observation
    if bfl_cast_connection and (type(cast_observation["bfl_target_context"]) is not dict or not cast_observation["bfl_target_context"]):
        raise OperationError("connection_bfl_datatable_context_invalid", "Native exact BFL cast target role JSON is missing")
    if asset_data_connection:
        try:
            _asset_data_break_observation(plan, before, source_id, source_pin_name, target_id, target_pin_name)
        except (KeyError, ValueError, TypeError, AttributeError, OperationError) as error:
            raise OperationError("connection_asset_data_observation_invalid", "Native whole AssetData Break plan differs from copied current graph roles") from error
    if row_connection:
        try:
            _string1_row_observation(plan, before, source_id, source_pin_name, target_id, target_pin_name)
        except (KeyError, TypeError, ValueError, AttributeError, OperationError) as error:
            raise OperationError("connection_row_observation_invalid", "Native Row/Break preflight does not match the copied current graph roles") from error
    if plan.get("exact_standard_k2_schema") is not True:
        raise OperationError("connection_schema_unsupported", "Connection dispatch requires the exact standard K2 schema; custom overrides are unsupported")
    source_row, target_row = _pin_row(before, source_id, source_pin_name, "output"), _pin_row(before, target_id, target_pin_name, "input")
    source_links, target_links = _link_keys(source_row), _link_keys(target_row)
    forward, reverse = (target_id, target_pin_name) in source_links, (source_id, source_pin_name) in target_links
    if forward != reverse or plan["already_linked"] != (forward and reverse):
        raise OperationError("connection_observation_inconsistent", "Existing pin links are not bidirectional or disagree with native observation; no repair was attempted")
    existing = forward and reverse
    # UE58 K2 treats exec inputs as convergence points. Only an empty exec
    # output with a native direct MAKE decision may add to an occupied exec
    # input; data-input replacement and exec-output fanout remain forbidden.
    exec_input_fan_in = (not existing and guard is None and bool(target_links) and not source_links
        and source_row["category"] == target_row["category"] == "exec"
        and plan["direct_connection_allowed"] and plan.get("schema_response_name") == "make")
    result["data"]["exec_input_fan_in"] = exec_input_fan_in
    old_id = None
    if guard is not None:
        # Never let an already-connected result hide a stale expectation from a
        # new operation. Journal replay is handled before reaching this action.
        result["data"].update(copy.deepcopy(guard))
        try:
            old_node = api.find_node(editor, guard["expected_existing_source_link_target_node_id"])
            old_id = api.node_id(old_node)
            old_pin = guard["expected_existing_source_link_target_pin"]
            api.find_pin(old_node, old_pin, "input")
            old_row = _pin_row(before, old_id, old_pin, "input")
            valid = ((old_id, old_pin) != (target_id, target_pin_name)
                and all(pin["category"] == "exec" for pin in (source_row, target_row, old_row))
                and source_links == {(old_id, old_pin)} and _link_keys(old_row) == {(source_id, source_pin_name)}
                and not target_links and plan.get("schema_response_name") == "break_others_a")
        except OperationError as error:
            raise OperationError("connection_replacement_guard_mismatch", "Expected old endpoint could not be verified: " + str(error)) from error
        if not valid:
            raise OperationError("connection_replacement_guard_mismatch", "Current exec endpoints/counts or schema do not match the exact expected old link; no dispatch")
        result["data"].update(replacement_guard_preflight_verified=True, expected_existing_source_link_target_object_path=old_id)
    elif not existing:
        if target_links and not exec_input_fan_in or source_row["category"] == "exec" and source_links:
            raise OperationError("connection_would_replace_links", "Input replacement and exec-output fanout are unsupported; existing links were preserved")
        if not plan["direct_connection_allowed"]:
            raise OperationError("conversion_contract_not_implemented", "Only the schema MAKE response is supported; conversion/promotion nodes, replacement and rejected links are not dispatched")
    result["data"].update(already_connected=existing, source_node_id=source_node_id, target_node_id=target_node_id,
        source_pin=source_pin_name, target_pin=target_pin_name, source_node_object_path=source_id, target_node_object_path=target_id,
        schema="EdGraphSchema_K2", schema_response=plan.get("schema_response_name"),
        schema_message=plan.get("schema_message", ""), source_link_count_before=len(source_links), target_link_count_before=len(target_links))
    dispatch_error, native, engine_return, dispatch_unknown = None, None, None, False
    result["stage"] = "replace_exec_link" if guard else "connect_pins"
    if not existing:
        result["changes"].append({"kind": "replace_exec_link" if guard else "connect_exec_pins" if source_row["category"] == "exec" else "connect_data_pins",
            "source_node_id": source_id, "target_node_id": target_id,
            "requested_source_node_id": source_node_id, "requested_target_node_id": target_node_id, "attempted": False,
            "dispatch_state_at_checkpoint": "not_yet_dispatched"})
        (checkpoint_callback or state.checkpoint)(result)
        result["changes"][-1]["attempted"] = True
        result["data"]["mutation_state_unknown"] = True
        try:
            if guard is not None:
                native = api.replace_exec_link(editor, source_id, source_pin_name, target_id, target_pin_name, old_id, old_pin)
                result["data"]["native_outcome"] = copy.deepcopy(native)
                result["data"]["native_mutation_attempted"] = native["mutation_attempted"]
                result["data"]["replacement_guard_verified"] = native["data"].get("replacement_guard_verified") is True
                result["changes"][-1]["attempted"] = native["mutation_attempted"]
                dispatch_unknown = not native["change_state_known"]
                engine_return = native["engine_return"]
                if native["compiled"] or native["saved"]:
                    result["compile"].update(attempted=native["compiled"], compiled=native["compiled"])
                    result["save"].update(attempted=native["saved"], saved=native["saved"])
                    dispatch_unknown = True
                    raise OperationError("unexpected_connection_compile_save", "Native helper reported unrequested compile/save")
                if not native["success"] or not native["postcondition_passed"]:
                    dispatch_error = OperationError(native.get("error_code") or "connection_rejected", native.get("error") or "Native endpoint verification failed")
            elif select_connection or name_foreach_connection:
                if name_foreach_connection:
                    # Native dispatch uses names only; release unneeded pin wrappers before reconstruction.
                    source = target = None
                    native = api.connect_name_foreach_pins(editor, source_id, source_pin_name, target_id, target_pin_name)
                else:
                    native = api.connect_data_table_bool_select_pins(editor, source_id, source_pin_name, target_id, target_pin_name)
                result["data"]["native_outcome"] = copy.deepcopy(native)
                result["data"]["native_mutation_attempted"] = native["mutation_attempted"]
                result["changes"][-1]["attempted"] = native["mutation_attempted"]
                dispatch_unknown = not native["change_state_known"]
                engine_return = native["engine_return"]
                if native["compiled"] or native["saved"]:
                    result["compile"].update(attempted=native["compiled"], compiled=native["compiled"])
                    result["save"].update(attempted=native["saved"], saved=native["saved"])
                    dispatch_unknown = True
                    raise OperationError("unexpected_connection_compile_save", "Finite Name ForEach writer reported unrequested compile/save" if name_foreach_connection else "Finite Select writer reported unrequested compile/save")
                if not native["success"] or not native["postcondition_passed"]:
                    dispatch_error = OperationError(native.get("error_code") or "connection_rejected", native.get("error") or ("Native Name ForEach notification/current verification failed" if name_foreach_connection else "Native Select notification/current verification failed"))
            else:
                engine_return = api.connect(source, target)
            if engine_return is not True and dispatch_error is None:
                dispatch_error = OperationError("connection_rejected", "Blueprint schema returned false; actual graph observations are retained")
        except Exception as error:
            dispatch_error = error
            if select_connection:
                if hasattr(error, "native_connection_raw"):
                    result["data"]["native_select_dispatch_raw"] = error.native_connection_raw
                if hasattr(error, "native_connection_result"):
                    result["data"]["native_select_dispatch_result"] = copy.deepcopy(error.native_connection_result)
            if name_foreach_connection:
                if hasattr(error, "native_connection_raw"):
                    result["data"]["native_name_foreach_dispatch_raw"] = error.native_connection_raw
                if hasattr(error, "native_connection_result"):
                    result["data"]["native_name_foreach_dispatch_result"] = copy.deepcopy(error.native_connection_result)
            dispatch_unknown = getattr(error, "code", "") != "native_connection_api_unavailable"
            if not dispatch_unknown:
                result["changes"][-1]["attempted"] = False
                result["data"]["native_mutation_attempted"] = False
    if validity_macro_connection and dispatch_unknown:
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            dispatch_error={"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)})
        raise dispatch_error or OperationError("validity_macro_dispatch_unknown", "Unknown stock macro connection callback; no subsequent SDK read or retry")
    if name_foreach_connection and dispatch_unknown:
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            dispatch_error={"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)})
        raise dispatch_error or OperationError("name_foreach_dispatch_unknown", "Unknown Name ForEach callback; no subsequent SDK read or retry")
    if select_connection and dispatch_unknown:
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            dispatch_error={"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)})
        raise dispatch_error or OperationError("datatable_select_dispatch_unknown", "Unknown Select callback; no subsequent SDK read or retry")
    if bfl_cast_connection and dispatch_unknown:
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            dispatch_error={"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)})
        raise dispatch_error or OperationError("bfl_datatable_connection_dispatch_unknown", "Unknown BFL cast connection; no follow-up SDK read or retry")
    if asset_data_connection and dispatch_unknown:
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            dispatch_error={"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)})
        raise dispatch_error or OperationError("asset_data_connection_dispatch_unknown", "Unknown native AssetData connection; no follow-up SDK read or retry")
    if row_connection and dispatch_unknown:
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            dispatch_error={"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)})
        raise dispatch_error or OperationError("row_connection_dispatch_unknown", "Unknown Row dispatch; no follow-up SDK read or retry")
    if map_connection and dispatch_unknown:
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            dispatch_error={"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)})
        raise dispatch_error or OperationError("map_connection_dispatch_unknown", "Unknown Map dispatch; no follow-up SDK read or retry")
    result["stage"] = "readback"
    if dispatch_error:
        result["data"]["dispatch_error"] = {"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)}
    result["data"].update(engine_return=engine_return, dispatch_outcome_unknown=dispatch_unknown)
    try:
        # Fence a returned Row callback before public wrapper traversal, then
        # close the public reads with a second fresh native current observation.
        row_barrier = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if row_connection else None
        asset_data_barrier = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if asset_data_connection else None
        bfl_cast_barrier = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if bfl_cast_connection else None
        select_barrier = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if select_connection else None
        name_foreach_barrier = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if name_foreach_connection else None
        validity_macro_barrier = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if validity_macro_connection else None
        after = api.graph_info(editor, exact_pin_types=True)
        validity_macro_plan_after = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if validity_macro_connection else None
        if validity_macro_connection:
            result["data"]["is_valid_exec_macro_connection_plan_after"] = copy.deepcopy(validity_macro_plan_after)
            if validity_macro_barrier != validity_macro_plan_after:
                raise OperationError("connection_validity_macro_current_changed", "Native stock macro/source roles changed during public graph readback")
        name_foreach_plan_after = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if name_foreach_connection else None
        if name_foreach_connection:
            result["data"]["name_foreach_connection_plan_after"] = copy.deepcopy(name_foreach_plan_after)
            if name_foreach_barrier != name_foreach_plan_after:
                raise OperationError("connection_name_foreach_current_changed", "Native six-port/source roles changed during public graph readback")
        select_plan_after = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if select_connection else None
        if select_connection:
            result["data"]["datatable_select_connection_plan_after"] = copy.deepcopy(select_plan_after)
            if select_barrier != select_plan_after:
                raise OperationError("connection_datatable_select_current_changed", "Native current Select/source roles changed during public graph readback")
        bfl_cast_plan_after = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if bfl_cast_connection else None
        if bfl_cast_connection:
            result["data"]["bfl_datatable_connection_plan_after"] = copy.deepcopy(bfl_cast_plan_after)
            if bfl_cast_barrier != bfl_cast_plan_after:
                raise OperationError("connection_bfl_datatable_current_changed", "Native BFL cast roles changed during public graph readback")
        asset_data_plan_after = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if asset_data_connection else None
        if asset_data_connection:
            result["data"]["asset_data_connection_plan_after"] = copy.deepcopy(asset_data_plan_after)
            if asset_data_barrier != asset_data_plan_after:
                raise OperationError("connection_asset_data_current_changed", "Native AssetData Break roles changed during public graph readback")
        row_plan_after = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if row_connection else None
        if row_connection:
            result["data"]["row_connection_plan_after"] = copy.deepcopy(row_plan_after)
            if row_barrier != row_plan_after:
                raise OperationError("connection_row_current_changed", "Native Row roles changed during public graph readback")
        result["data"]["after"] = copy.deepcopy(after)
        map_plan_after = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name) if map_connection else None
        if map_connection:
            result["data"]["map_connection_plan_after"] = copy.deepcopy(map_plan_after)
            map_before = plan.get("map_connection_observation", {})
            if map_before.get("operation") in ("contains", "remove", "keys", "values", "length"):
                map_after = map_plan_after.get("map_connection_observation", {})
                array_result = map_before["operation"] in ("keys", "values")
                length_result = map_before["operation"] == "length"
                stable_map = ("node_path", "node_guid", "function_name", "operation", "native_function", "native_source", "pure") + (
                    ("array_output_contract_verified", "array_output_name", "map_connection_role") if array_result else
                    ("length_contract_verified", "map_value_pin_absent", "map_connection_role") if length_result else
                    ("key_only_contract_verified", "map_value_pin_absent"))
                source_verified = (plan.get("connection_scope") == map_plan_after.get("connection_scope")
                    and plan.get("connection_scope") in ("native_string_double_map", "native_string_vector_map")
                    and map_before.get("eligible") is True and map_after.get("eligible") is True
                    and map_before.get("native_source_current_verified") is True and map_after.get("native_source_current_verified") is True
                    and (map_before.get("array_output_contract_verified") is True if array_result else
                        map_before.get("length_contract_verified") is True and map_before.get("map_value_pin_absent") is True and map_before.get("pure") is True if length_result else
                        map_before.get("key_only_contract_verified") is True and map_before.get("map_value_pin_absent") is True)
                    and all(map_before.get(key) == map_after.get(key) for key in stable_map))
                if (array_result or length_result) and map_before.get("first_target_map_connection") is not True:
                    source_verified = source_verified and map_before.get("pins") == map_after.get("pins")
                result["data"]["native_array_output_map_source_preserved" if array_result else "native_length_map_source_preserved" if length_result else "native_key_only_map_source_preserved"] = source_verified
                if not source_verified:
                    error = OperationError("connection_map_array_output_source_changed" if array_result else "connection_map_length_source_changed" if length_result else "connection_map_key_only_source_changed", "Current finite Map source/roles changed across dispatch")
                    error.native_graph_result = map_plan_after
                    raise error
        source_after = _link_keys(_pin_row(after, source_id, source_pin_name, "output"))
        target_after = _link_keys(_pin_row(after, target_id, target_pin_name, "input"))
        old_after = _link_keys(_pin_row(after, old_id, old_pin, "input")) if guard else None
    except Exception as readback_error:
        native_failure = getattr(readback_error, "native_graph_result", None)
        if type(native_failure) is dict and native_failure.get("connection_scope") in ("native_string_vector_map", "native_string_double_map"):
            result["data"]["native_map_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") in ("string1_datatable_row", "scalar_uds_datatable_row"):
            result["data"]["native_row_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") == "native_asset_data_break":
            result["data"]["native_asset_data_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") == "native_datatable_bool_select":
            result["data"]["native_datatable_select_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") == "native_name_foreach_connection":
            result["data"]["native_name_foreach_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and native_failure.get("connection_scope") == "native_is_valid_exec_macro":
            result["data"]["native_validity_macro_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and ("bfl_name_array_context_failure" in native_failure or "bfl_name_array_context" in native_failure):
            result["data"]["native_bfl_name_array_connection_failure"] = copy.deepcopy(native_failure)
        if type(native_failure) is dict and ("bfl_target_context" in native_failure
                or type(native_failure.get("dynamic_cast_source_observation")) is dict
                and "bfl_target_context" in native_failure["dynamic_cast_source_observation"]):
            result["data"]["native_bfl_datatable_connection_failure"] = copy.deepcopy(native_failure)
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            readback_error={"code": getattr(readback_error, "code", "python_operation_failed"), "message": str(readback_error)})
        raise dispatch_error or readback_error
    if bfl_cast_connection:
        final_cast = bfl_cast_plan_after.get("dynamic_cast_source_observation", {})
        if cast_observation["bfl_target_context"] != final_cast.get("bfl_target_context"):
            result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False)
            result["data"]["native_bfl_datatable_connection_failure"] = copy.deepcopy(bfl_cast_plan_after)
            raise OperationError("connection_bfl_datatable_context_changed", "Exact BFL cast target roles changed across dispatch")
    previous_edges, actual_edges = _edges(before), _edges(after)
    expected_edges = previous_edges | _pair(source_id, source_pin_name, target_id, target_pin_name)
    if guard:
        expected_edges -= _pair(source_id, source_pin_name, old_id, old_pin)
    nonlinks_preserved = _nonlink_state(before) == _nonlink_state(after)
    default_reset_verified, default_reset = False, None
    if (not validity_macro_connection and not name_foreach_connection and not select_connection and not asset_data_connection and not row_connection and not nonlinks_preserved and not existing and guard is None and dispatch_error is None and not dispatch_unknown):
        default_reset_verified, default_reset = _numeric_target_default_reset(editor, before, after,
            source_id, source_pin_name, target_id, target_pin_name, plan)
    cast_retype_verified, cast_retype = False, None
    if (not validity_macro_connection and not name_foreach_connection and not select_connection and not asset_data_connection and not row_connection and not nonlinks_preserved and not default_reset_verified and not existing and guard is None
            and dispatch_error is None and not dispatch_unknown):
        try:
            cast_retype_verified, cast_retype = _dynamic_cast_source_retype(editor, before, after,
                source_id, source_pin_name, target_id, target_pin_name, plan,
                observed_after=bfl_cast_plan_after if bfl_cast_connection else None)
        except Exception as current_error:
            if bfl_cast_connection:
                native_failure = getattr(current_error, "native_graph_result", None)
                if type(native_failure) is dict:
                    result["data"]["native_bfl_datatable_connection_failure"] = copy.deepcopy(native_failure)
                result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False)
            raise
    map_retype_verified, map_retype = False, None
    if map_connection and not existing and guard is None and dispatch_error is None and not dispatch_unknown:
        map_retype_verified, map_retype = _map_specialization_projection(before, after, source_id, source_pin_name,
            target_id, target_pin_name, plan, map_plan_after)
    row_retype_verified, row_retype = False, None
    if row_connection and guard is None and dispatch_error is None and not dispatch_unknown:
        row_retype_verified, row_retype = _string1_row_projection(before, after, source_id, source_pin_name,
            target_id, target_pin_name, plan, row_plan_after)
    asset_data_roles_verified, asset_data_roles = False, None
    if asset_data_connection and guard is None and dispatch_error is None and not dispatch_unknown:
        asset_data_roles_verified, asset_data_roles = _asset_data_connection_preserved(before, after,
            source_id, source_pin_name, target_id, target_pin_name, plan, asset_data_plan_after)
    select_roles_verified, select_roles = False, None
    if select_connection and guard is None and dispatch_error is None and not dispatch_unknown:
        select_roles_verified, select_roles = _datatable_select_projection(before, after,
            source_id, source_pin_name, target_id, target_pin_name, plan, select_plan_after)
    name_foreach_roles_verified, name_foreach_roles = False, None
    if name_foreach_connection and guard is None and dispatch_error is None and not dispatch_unknown:
        name_foreach_roles_verified, name_foreach_roles = _name_foreach_projection(before, after,
            source_id, source_pin_name, target_id, target_pin_name, plan, name_foreach_plan_after, native)
    validity_macro_roles_verified, validity_macro_roles = False, None
    if validity_macro_connection and guard is None and dispatch_error is None and not dispatch_unknown:
        validity_macro_roles_verified, validity_macro_roles = _validity_macro_preserved(before, after,
            source_id, source_pin_name, target_id, target_pin_name, plan, validity_macro_plan_after)
    nonlink_contract_verified = (validity_macro_roles_verified if validity_macro_connection else name_foreach_roles_verified if name_foreach_connection else select_roles_verified if select_connection else asset_data_roles_verified if asset_data_connection else
        row_retype_verified if row_connection else (nonlinks_preserved or default_reset_verified or cast_retype_verified or map_retype_verified))
    topology_passed = nonlink_contract_verified and actual_edges == expected_edges
    forward, reverse = (target_id, target_pin_name) in source_after, (source_id, source_pin_name) in target_after
    old_removed = guard is not None and not old_after and (old_id, old_pin) not in source_after
    observed_changed = (previous_edges != actual_edges or not nonlinks_preserved
        or before.get("package_dirty") != after.get("package_dirty"))
    changed = observed_changed or (native is not None and native.get("changed") is True)
    result["data"].update(topology_preserved=topology_passed, nonlink_state_preserved=nonlinks_preserved,
        nonlink_contract_verified=nonlink_contract_verified, target_autogenerated_default_reset_verified=default_reset_verified,
        target_default_reset=default_reset, dynamic_cast_source_retype_verified=cast_retype_verified,
        dynamic_cast_source_retype=cast_retype,
        native_map_specialization_verified=map_retype_verified, native_map_specialization=map_retype,
        native_row_specialization_verified=row_retype_verified, native_row_specialization=row_retype,
        native_asset_data_roles_verified=asset_data_roles_verified, native_asset_data_roles=asset_data_roles,
        native_datatable_select_roles_verified=select_roles_verified, native_datatable_select_roles=select_roles,
        native_name_foreach_roles_verified=name_foreach_roles_verified, native_name_foreach_roles=name_foreach_roles,
        mutation_state_unknown=dispatch_unknown, mutation_performed=changed, observed_graph_change_state_known=True,
        observed_graph_changed=observed_changed, source_link_count_after=len(source_after), target_link_count_after=len(target_after),
        expected_existing_link_removed=old_removed, replaced_source_link_count=int(old_removed),
        compiled=result["compile"].get("compiled", False), save_requested=False,
        saved=result["save"]["saved"], dirty_after=after.get("package_dirty"),
        edge_delta={"representation": "directed pin-link observations; a reciprocal connection has two entries",
            "removed": [list(edge) for edge in sorted(previous_edges - actual_edges)],
            "added": [list(edge) for edge in sorted(actual_edges - previous_edges)]})
    if validity_macro_connection:
        result["data"].update(native_is_valid_exec_macro_roles_verified=validity_macro_roles_verified, native_is_valid_exec_macro_roles=validity_macro_roles)
    if result["changes"]:
        result["changes"][-1].update(changed=changed, change_state_known=not dispatch_unknown,
            observed_graph_change_state_known=True)
        if not result["changes"][-1]["attempted"] and not changed and not dispatch_unknown:
            result["changes"] = []
    passed = forward and reverse and topology_passed and (not guard or old_removed) and dispatch_error is None and not dispatch_unknown
    result["stage"] = "verification"  # After snapshot is complete; known partial failures are not lost-readback failures.
    count = 1 if identity == "connect_blueprint_graph_pins" else 2
    if identity == "connect_blueprint_graph_pins":
        # Legacy endpoint observations remain additive to the richer snapshots.
        before = dict(before, connected=existing, source_link_count=len(source_links), target_link_count=len(target_links))
        after = dict(after, connected=forward and reverse, source_link_count=len(source_after), target_link_count=len(target_after), graph_error="")
    try:
        _contract(result, before, after, identity, affected=count, updated=0 if existing else count,
            skipped=count if existing else 0, passed=passed)
    except OperationError:
        if dispatch_error:
            raise dispatch_error
        raise


def _edges(snapshot):
    """Observe both directions, so losing an unrelated reverse edge also fails."""
    return {(node["node_id"], pin["direction"], pin["name"], link["node_id"], link["pin_name"])
            for node in snapshot["nodes"] for pin in node["pins"] for link in pin["links"]}
