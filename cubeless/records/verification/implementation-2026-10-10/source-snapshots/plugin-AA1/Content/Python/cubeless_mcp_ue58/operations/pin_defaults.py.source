"""Legacy named pin-default contract with exact graph delta and shared journal."""

import copy

from cubeless_mcp_ue58.compatibility.ue58 import graph as graph_api
from cubeless_mcp_ue58.compatibility.ue58 import pin_defaults as api
from cubeless_mcp_ue58.compatibility.ue58.blueprint import ensure_editor_idle
from . import functions, state, variables
from .graph import _contract
from .state import OperationError


def _arguments(blueprint_name, node_id, pin_name, value_json, direction, graph_name, graph_id, graph_type):
    direction = functions._text("" if direction is None else direction, "direction", empty=True).lower()
    if direction not in ("", "input"):
        raise OperationError("pin_default_direction_unsupported", "This slice supports unlinked input pins only; output defaults remain unsupported")
    value = variables._json_value(value_json, "value_json")
    return {**functions._selectors(blueprint_name, graph_name, graph_id, graph_type),
        "node_id": functions._text(node_id, "node_id"), "pin_name": functions._text(pin_name, "pin_name"),
        "value_json": variables._json_text(value), "direction": direction}


def _without_target_value(snapshot, node_path, pin_name, *, expected_type_text=None):
    value = copy.deepcopy(snapshot)
    value.pop("package_dirty", None)
    matches = [pin for node in value["nodes"] if node["node_id"] == node_path
               for pin in node["pins"] if pin["name"] == pin_name and pin["direction"] == "input"]
    if len(matches) != 1:
        raise OperationError("pin_default_observation_invalid", "Expected one exact input pin in the complete graph observation")
    if expected_type_text is not None:
        if not expected_type_text or matches[0].get("native_pin_type_text") != expected_type_text:
            raise OperationError("pin_default_type_observation_mismatch", "Native and independent pin type observations disagree")
        # The engine explicitly clears only the legacy real-number serialization
        # bit during TrySetDefaultValue. Native verifies the complete expected
        # FEdGraphPinType, and this adapter independently matches both raw texts.
        matches[0].pop("native_pin_type_text")
    matches[0].pop("value", None)
    return value


def _run(result, arguments, checkpoint_callback=None):
    ensure_editor_idle()
    editor, selected = graph_api.sequence_editor(arguments["blueprint_path"], arguments["graph_name"],
        arguments["graph_id"], arguments["graph_type"])
    node = graph_api.find_node(editor, arguments["node_id"])
    node_path = graph_api.node_id(node)
    # Resolves uniqueness before the journal checkpoint; native independently
    # rechecks type/default/direction/links at its mutation boundary.
    graph_api.find_pin(node, arguments["pin_name"], "input")
    before = graph_api.graph_info(editor, exact_pin_types=True)
    stable_before = _without_target_value(before, node_path, arguments["pin_name"])
    result["data"].update(graph_before=copy.deepcopy(before), graph_selection=selected,
        requested_node_id=arguments["node_id"], full_legacy_pin_default_parity=False)
    values = {"blueprint_path": arguments["blueprint_path"], "graph_path": selected["object_path"],
        "node_path": node_path, "pin_name": arguments["pin_name"],
        "value_json": arguments["value_json"], "direction": arguments["direction"]}
    failure = None
    try:
        functions._run_action(result, "set_blueprint_pin_default", values, checkpoint_callback, operation_api=api)
    except Exception as error:
        failure = error
    try:
        after = graph_api.graph_info(editor, exact_pin_types=True)
    except Exception:
        result["data"]["mutation_state_unknown"] = bool(result["data"].get("native_dispatched"))
        result["success"] = False
        raise
    clear_legacy_float = result["data"].get("legacy_float_serialization_flag_cleared") is True
    if clear_legacy_float:
        stable_before = _without_target_value(before, node_path, arguments["pin_name"],
            expected_type_text=result["data"].get("previous", {}).get("pin_type_text", ""))
        stable_after = _without_target_value(after, node_path, arguments["pin_name"],
            expected_type_text=result["data"].get("pin", {}).get("pin_type_text", ""))
    else:
        stable_after = _without_target_value(after, node_path, arguments["pin_name"])
    preserved = stable_before == stable_after
    result["data"].update(graph_after=copy.deepcopy(after), other_graph_state_preserved=preserved)
    if failure is not None:
        # Preserve both native partial outcomes and independent observations.
        result["success"] = False
        if not preserved:
            result["data"]["mutation_state_unknown"] = True
        raise failure
    if not preserved:
        result["data"]["mutation_state_unknown"] = True
    changed = before != after
    _contract(result, before, after, "set_blueprint_pin_default", updated=1 if changed else 0,
        skipped=0 if changed else 1, passed=preserved)
    result["retry"]["duplicate_risk"] = "pin_default_action_would_repeat" if changed else "none_observed"


def set_blueprint_pin_default(blueprint_name, node_id, pin_name, value_json, direction="", graph_name="EventGraph",
        graph_id=None, graph_type=None, operation_id=None):
    try:
        arguments = _arguments(blueprint_name, node_id, pin_name, value_json, direction, graph_name, graph_id, graph_type)
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    return state.mutate("set_blueprint_pin_default", arguments, operation_id, lambda result: _run(result, arguments))


def set_blueprint_pin_default_action(result, blueprint_name, node_id, pin_name, value_json, direction="", graph_name="EventGraph",
        graph_id=None, graph_type=None, checkpoint_callback=None):
    _run(result, _arguments(blueprint_name, node_id, pin_name, value_json, direction, graph_name, graph_id, graph_type), checkpoint_callback)
