"""Named own/inherited/component member nodes sharing one observed spawn core."""

import copy
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import graph as graph_api
from cubeless_mcp_ue58.compatibility.ue58 import graph_variables as api
from cubeless_mcp_ue58.compatibility.ue58.blueprint import ensure_editor_idle
from . import state
from .blueprint import parent_comparison_state
from .graph import _contract
from .state import OperationError, mutate


def _options(target_class, graph_id, graph_type, create_graph_if_missing, node_position_json, x, y):
    from . import functions
    if functions._bool(create_graph_if_missing, "create_graph_if_missing"):
        raise OperationError("graph_member_creation_unsupported", "Resolve the graph explicitly before adding a member node")
    options = {}
    for name, value in (("target_class", target_class), ("graph_id", graph_id), ("graph_type", graph_type)):
        if value is not None:
            value = functions._text(value, name, empty=True)
            if value:
                options[name] = value
    if node_position_json is not None:
        position = functions._json(node_position_json, "node_position_json")
        if position is None:
            position = [0, 0]
        if type(position) is not list or len(position) != 2 or any(type(v) is not int or abs(v) > 1000000 for v in position):
            raise OperationError("node_position_invalid", "node_position_json requires two integers from -1000000 to 1000000")
        if (x, y) != (0, 0) and [x, y] != position:
            raise OperationError("node_position_conflict", "Explicit node_position_json conflicts with existing x/y arguments")
        from .variables import _json_text
        options["node_position_json"] = _json_text(position)
    return options


def _source_preserved(before, after, *, allow_load=False):
    if not before or not after or not after.get("loaded") or after.get("dirty"):
        return False
    if before.get("file") != after.get("file") or before.get("class_path") != after.get("class_path"):
        return False
    if before.get("loaded"):
        return before == after
    return allow_load and not before.get("dirty")


def _add_action(result, blueprint_name, variable_name, graph_name, x, y, setter, checkpoint_callback=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    if type(variable_name) is not str or not variable_name or any(ord(c) < 32 or ord(c) == 127 for c in variable_name):
        raise OperationError("variable_name_invalid", "An exact nonempty member name is required")
    if any(type(v) is not int or abs(v) > 1000000 for v in (x, y)):
        raise OperationError("node_position_invalid", "Node coordinates must be integers between -1000000 and 1000000")
    options = _options(target_class, graph_id, graph_type, create_graph_if_missing, node_position_json, x, y)
    target_class = options.get("target_class", "")
    if "node_position_json" in options:
        from .functions import _json
        x, y = _json(options["node_position_json"], "node_position_json")
    ensure_editor_idle()
    if options.get("graph_id") or options.get("graph_type"):
        from .functions import _selectors
        selectors = _selectors(blueprint_name, graph_name, graph_id, graph_type)
        editor, selection = graph_api.sequence_editor(**selectors)
        result["data"]["graph_selection"] = selection
    else:
        editor = graph_api.graph_editor(blueprint_name, graph_name)
    before = graph_api.graph_info(editor)
    result["data"].update(before=copy.deepcopy(before),
        full_legacy_member_scope_parity=False, mutation_state_unknown=False)
    if before["total_node_count"] >= 2048:
        raise OperationError("graph_node_limit_reached", "Member node would exceed the bounded graph observation limit")
    source_baseline = None
    external_class = None  # Retain the loaded class through spawn and readback.
    if target_class:
        result["stage"] = "external_class_preflight"
        source_before = api.external_source_state(target_class)
        result["data"]["external_source_before_load"] = source_before
        if (not source_before["file"]["exists"] or not source_before["file"]["sha256"] or source_before["dirty"]):
            raise OperationError("graph_external_source_not_saved_clean", "External class requires a clean existing saved Game Blueprint")
        result["data"]["pending_external_load"] = {"class_path": target_class,
            "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_source_and_target"}
        if not source_before["loaded"]:
            # A cold class load can run asset callbacks. Preserve this earlier
            # boundary even though the actual member spawn has its own intent.
            (checkpoint_callback or state.checkpoint)(result)
        result["data"]["external_class_load_attempted"] = True
        result["data"]["mutation_state_unknown"] = True
        try:
            external_class = api.load_external_class(target_class)
        finally:
            source_baseline = api.external_source_state(target_class)
            result["data"]["external_source_after_load"] = source_baseline
            after_load = graph_api.graph_info(editor)
            result["data"]["after_external_load"] = after_load
            if not _source_preserved(source_before, source_baseline, allow_load=True) or after_load != before:
                result["data"]["mutation_state_unknown"] = True
                result["changes"].append({"kind": "external_class_load", "attempted": True,
                    "outcome": "source_or_target_changed_during_load", "graph_changed": after_load != before})
                raise OperationError("graph_external_load_changed_state", "Loading changed source bytes/state or target graph; no node spawn dispatched")
            result["data"]["mutation_state_unknown"] = False
    plan = api.member_plan(editor, variable_name, setter, target_class) if target_class else api.member_plan(editor, variable_name, setter)
    result["data"]["member_preflight"] = plan
    if target_class:
        source_preflight = api.external_source_state(target_class)
        result["data"]["external_source_before_spawn"] = source_preflight
        if not _source_preserved(source_baseline, source_preflight):
            result["data"]["mutation_state_unknown"] = True
            raise OperationError("graph_external_source_changed", "External source changed during member preflight; no node spawn dispatched")
    kind = "add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node"
    result["stage"] = "before_member_node_spawn"
    result["changes"].append({"kind": kind, "graph_path": before["graph_path"], "variable_name": plan["variable_name"],
        "attempted": False, "outcome": "not_yet_dispatched_at_checkpoint"})
    result["data"]["interrupted_outcome"] = "unknown_inspect_graph_and_journal"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "spawn_member_node"
    result["changes"][-1].update(attempted=True, outcome="unknown_until_graph_readback")
    result["data"]["mutation_state_unknown"] = True
    node, dispatch_error = None, None
    try:
        node = (api.add_member(editor, plan["variable_name"], setter, x, y, target_class)
                if target_class else api.add_member(editor, plan["variable_name"], setter, x, y))
    except Exception as error:
        dispatch_error = error
        result["data"]["member_dispatch_error"] = str(error)
    result["stage"] = "readback"
    try:
        after = graph_api.graph_info(editor)
    finally:
        if target_class:
            source_after = api.external_source_state(target_class)
            result["data"].update(external_source_after=source_after,
                external_source_preserved=_source_preserved(source_baseline, source_after))
    result["data"]["after"] = copy.deepcopy(after)
    if dispatch_error is not None:
        result["changes"][-1].update(outcome="dispatch_failed_after_graph_observation", graph_changed=before != after)
        # A failed/absent reply stays uncertain even when current graph changes
        # are observable; retain evidence and never dispatch another spawn.
        raise dispatch_error
    created_id = graph_api.node_id(node)
    old = {row["node_id"]: row for row in before["nodes"]}
    current = {row["node_id"]: row for row in after["nodes"]}
    observed = current.get(created_id, {})
    expected_class = "K2Node_VariableSet" if setter else "K2Node_VariableGet"
    passed = (set(current) - set(old) == {created_id} and all(current.get(key) == value for key, value in old.items())
        and observed.get("class") == expected_class and observed.get("x") == x and observed.get("y") == y
        and observed.get("variable_name") == plan["variable_name"] and observed.get("self_context") is (not bool(target_class))
        and observed.get("variable_property_path") == plan["property_path"])
    if target_class:
        after_plan = api.member_plan(editor, variable_name, setter, target_class)
        result["data"]["member_after"] = after_plan
        source_preserved = result["data"]["external_source_preserved"]
        plan_preserved = (parent_comparison_state(after_plan.get("external_source_validation", {}))
            == parent_comparison_state(plan.get("external_source_validation", {})) and
            all(after_plan.get(key) == plan.get(key) for key in (
                "variable_guid", "target_class_path", "declaring_class_path", "declaring_blueprint_path",
                "property_path", "compiled_property_path", "expected_pin_type_text", "expected_self_pin_type_text")))
        pin_types = {"variable": api.member_pin_type(node, plan["variable_name"], "input" if setter else "output"),
                     "self": api.member_pin_type(node, "self", "input")}
        typed_passed = (bool(plan.get("expected_pin_type_text")) and bool(plan.get("expected_self_pin_type_text"))
            and pin_types["variable"] == plan["expected_pin_type_text"] and pin_types["self"] == plan["expected_self_pin_type_text"])
        try:
            observed_guid = uuid.UUID(observed.get("variable_member_guid", ""))
            guid_passed = bool(observed_guid.int) and observed_guid == uuid.UUID(plan["variable_guid"])
        except (TypeError, ValueError, AttributeError, KeyError):
            guid_passed = False
        result["data"].update(external_source_preserved=source_preserved and plan_preserved,
            member_pin_types=pin_types, member_output_type_verified=typed_passed, external_member_guid_verified=guid_passed)
        passed = (passed and source_preserved and plan_preserved and typed_passed and guid_passed
            and observed.get("variable_member_parent_path") == target_class)
    elif plan.get("member_scope") == "own_runtime_object":
        observed_type = api.member_pin_type(node, plan["variable_name"], "input" if setter else "output")
        typed_passed = bool(plan.get("expected_pin_type_text")) and observed_type == plan["expected_pin_type_text"]
        result["data"].update(member_pin_type_text=observed_type, member_output_type_verified=typed_passed)
        passed = passed and typed_passed
    if plan.get("member_scope") in ("local_scs_generated_component", "inherited_actor_boolean"):
        observed_type = api.member_output_type(node, plan["variable_name"])
        typed_passed = bool(plan.get("expected_pin_type_text")) and observed_type == plan["expected_pin_type_text"]
        result["data"].update(output_pin_type_text=observed_type, member_output_type_verified=typed_passed)
        if plan["member_scope"] == "local_scs_generated_component":
            result["data"]["component_output_type_verified"] = typed_passed
        else:
            after_plan = api.member_plan(editor, variable_name, False)
            result["data"]["member_after"] = after_plan
            parent_preserved = (parent_comparison_state(after_plan.get("declaring_parent_validation", {}))
                == parent_comparison_state(plan.get("declaring_parent_validation", {})) and
                all(after_plan.get(key) == plan.get(key) for key in (
                    "variable_guid", "declaring_class_path", "declaring_blueprint_path", "property_path", "expected_pin_type_text")))
            result["data"]["declaring_parent_preserved"] = parent_preserved
            passed = passed and parent_preserved
        passed = passed and not setter and typed_passed
    result["data"].update(observed, member_preflight=plan, mutation_state_unknown=not passed)
    result["changes"][-1].update(outcome="observed_new_member_node" if passed else "postcondition_mismatch_inspect_graph",
        created_node_id=created_id)
    _contract(result, before, after, kind, created=1 if passed else 0, passed=passed)


def _add(blueprint_name, variable_name, graph_name, x, y, setter, operation_id, **options):
    name = "add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node"
    args = {"blueprint_name": blueprint_name, "variable_name": variable_name, "graph_name": graph_name, "x": x, "y": y}
    try:
        extras = _options(**options, x=x, y=y)
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    args.update(extras)
    return mutate(name, args, operation_id, lambda result: _add_action(result, blueprint_name, variable_name, graph_name, x, y, setter, **extras))


def add_blueprint_variable_get_node(blueprint_name, variable_name, graph_name="EventGraph", x=0, y=0, operation_id=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    return _add(blueprint_name, variable_name, graph_name, x, y, False, operation_id, target_class=target_class,
        graph_id=graph_id, graph_type=graph_type, create_graph_if_missing=create_graph_if_missing, node_position_json=node_position_json)


def add_blueprint_variable_set_node(blueprint_name, variable_name, graph_name="EventGraph", x=0, y=0, operation_id=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    return _add(blueprint_name, variable_name, graph_name, x, y, True, operation_id, target_class=target_class,
        graph_id=graph_id, graph_type=graph_type, create_graph_if_missing=create_graph_if_missing, node_position_json=node_position_json)


def add_blueprint_variable_get_node_action(result, blueprint_name, variable_name, graph_name="EventGraph", x=0, y=0, checkpoint_callback=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    _add_action(result, blueprint_name, variable_name, graph_name, x, y, False, checkpoint_callback,
        target_class, graph_id, graph_type, create_graph_if_missing, node_position_json)


def add_blueprint_variable_set_node_action(result, blueprint_name, variable_name, graph_name="EventGraph", x=0, y=0, checkpoint_callback=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    _add_action(result, blueprint_name, variable_name, graph_name, x, y, True, checkpoint_callback,
        target_class, graph_id, graph_type, create_graph_if_missing, node_position_json)
