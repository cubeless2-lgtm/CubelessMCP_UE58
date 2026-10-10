"""Small Blueprint graph edits with legacy mutation_result_v1 observations.

This is a bounded subset: K2 graph listing and Branch/Sequence add/link/move/delete.
GUID input selectors resolve to canonical object paths. Conversion/promotion
nodes and replacement of existing links remain unsupported.
"""

import copy
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
    if functions._bool(create_graph_if_missing, "create_graph_if_missing"):
        raise OperationError("sequence_graph_creation_unsupported", "Create the graph with a separate named resolve action before adding Sequence")
    if type(output_count) is not int or not 2 <= output_count <= 64:
        raise OperationError("sequence_output_count_invalid", "output_count must be an integer from 2 to 64; legacy rounding/minimum clamping is not applied")
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
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
    if compile_requested or save_requested:
        raise OperationError("connection_compile_save_unsupported", "This connection action performs no compilation or save; use separate named actions after the chain is complete")
    fields = (expected_existing_source_link_count, expected_existing_source_link_target_node_id,
        expected_existing_source_link_target_pin, expected_schema_response)
    if (permitted and any(value is None for value in fields)) or (not permitted and any(value is not None for value in fields)):
        raise OperationError("connection_replacement_guard_required", "Replacement permission requires all four expected fields; expected fields are invalid without permission")
    guard = None
    if permitted:
        if type(fields[0]) is not int or fields[0] != 1 or fields[3] != "break_others_a":
            raise OperationError("connection_replacement_guard_invalid", "This slice requires exact integer source count 1 and expected_schema_response=break_others_a")
        guard = {"expected_existing_source_link_count": fields[0],
            "expected_existing_source_link_target_node_id": api._selector(fields[1], "expected_existing_source_link_target_node_id"),
            "expected_existing_source_link_target_pin": api._selector(fields[2], "expected_existing_source_link_target_pin"),
            "expected_schema_response": fields[3]}
    return {**selectors, "source_node_id": api._selector(source_node_id, "source_node_id"),
        "source_pin": api._selector(source_pin, "source_pin"), "target_node_id": api._selector(target_node_id, "target_node_id"),
        "target_pin": api._selector(target_pin, "target_pin"), "guard": guard}


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
    ensure_editor_idle()
    editor, selected = api.connection_editor(**{key: arguments[key] for key in ("blueprint_path", "graph_name", "graph_id", "graph_type")})
    result["data"].update(graph=selected, full_legacy_connection_parity=False, loading_side_effects_verified=False)
    _connect_action(result, editor, arguments["source_node_id"], arguments["source_pin"],
        arguments["target_node_id"], arguments["target_pin"], arguments["guard"], checkpoint_callback,
        identity="connect_blueprint_graph_pins")


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
    plan = api.connection_plan(editor, source_id, source_pin_name, target_id, target_pin_name)
    result["data"]["connection_plan"] = copy.deepcopy(plan)
    if plan.get("exact_standard_k2_schema") is not True:
        raise OperationError("connection_schema_unsupported", "Connection dispatch requires the exact standard K2 schema; custom overrides are unsupported")
    source_row, target_row = _pin_row(before, source_id, source_pin_name, "output"), _pin_row(before, target_id, target_pin_name, "input")
    source_links, target_links = _link_keys(source_row), _link_keys(target_row)
    forward, reverse = (target_id, target_pin_name) in source_links, (source_id, source_pin_name) in target_links
    if forward != reverse or plan["already_linked"] != (forward and reverse):
        raise OperationError("connection_observation_inconsistent", "Existing pin links are not bidirectional or disagree with native observation; no repair was attempted")
    existing = forward and reverse
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
        if target_links or source_row["category"] == "exec" and source_links:
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
            else:
                engine_return = api.connect(source, target)
            if engine_return is not True and dispatch_error is None:
                dispatch_error = OperationError("connection_rejected", "Blueprint schema returned false; actual graph observations are retained")
        except Exception as error:
            dispatch_error = error
            dispatch_unknown = getattr(error, "code", "") != "native_connection_api_unavailable"
            if not dispatch_unknown:
                result["changes"][-1]["attempted"] = False
                result["data"]["native_mutation_attempted"] = False
    result["stage"] = "readback"
    if dispatch_error:
        result["data"]["dispatch_error"] = {"code": getattr(dispatch_error, "code", "python_operation_failed"), "message": str(dispatch_error)}
    result["data"].update(engine_return=engine_return, dispatch_outcome_unknown=dispatch_unknown)
    try:
        after = api.graph_info(editor, exact_pin_types=True)
        result["data"]["after"] = copy.deepcopy(after)
        source_after = _link_keys(_pin_row(after, source_id, source_pin_name, "output"))
        target_after = _link_keys(_pin_row(after, target_id, target_pin_name, "input"))
        old_after = _link_keys(_pin_row(after, old_id, old_pin, "input")) if guard else None
    except Exception as readback_error:
        result["data"].update(mutation_state_unknown=True, observed_graph_change_state_known=False,
            readback_error={"code": getattr(readback_error, "code", "python_operation_failed"), "message": str(readback_error)})
        raise dispatch_error or readback_error
    previous_edges, actual_edges = _edges(before), _edges(after)
    expected_edges = previous_edges | _pair(source_id, source_pin_name, target_id, target_pin_name)
    if guard:
        expected_edges -= _pair(source_id, source_pin_name, old_id, old_pin)
    nonlinks_preserved = _nonlink_state(before) == _nonlink_state(after)
    topology_passed = nonlinks_preserved and actual_edges == expected_edges
    forward, reverse = (target_id, target_pin_name) in source_after, (source_id, source_pin_name) in target_after
    old_removed = guard is not None and not old_after and (old_id, old_pin) not in source_after
    observed_changed = (previous_edges != actual_edges or not nonlinks_preserved
        or before.get("package_dirty") != after.get("package_dirty"))
    changed = observed_changed or (native is not None and native.get("changed") is True)
    result["data"].update(topology_preserved=topology_passed, nonlink_state_preserved=nonlinks_preserved,
        mutation_state_unknown=dispatch_unknown, mutation_performed=changed, observed_graph_change_state_known=True,
        observed_graph_changed=observed_changed, source_link_count_after=len(source_after), target_link_count_after=len(target_after),
        expected_existing_link_removed=old_removed, replaced_source_link_count=int(old_removed),
        compiled=result["compile"].get("compiled", False), save_requested=False,
        saved=result["save"]["saved"], dirty_after=after.get("package_dirty"),
        edge_delta={"representation": "directed pin-link observations; a reciprocal connection has two entries",
            "removed": [list(edge) for edge in sorted(previous_edges - actual_edges)],
            "added": [list(edge) for edge in sorted(actual_edges - previous_edges)]})
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
