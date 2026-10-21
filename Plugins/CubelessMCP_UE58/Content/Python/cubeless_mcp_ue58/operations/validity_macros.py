"""Explicit stock Is Valid exec macro authoring, separate from pure IsValid."""

import uuid

from cubeless_mcp_ue58.compatibility.ue58 import validity_macros as api
from . import functions
from .state import OperationError, fail, mutate, new_result


_OPERATION = "add_blueprint_is_valid_exec_macro_node"


def _arguments(blueprint_name, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    create = functions._bool(create_graph_if_missing, "create_graph_if_missing")
    if create:
        raise OperationError("validity_macro_graph_creation_unsupported", "Resolve the exact existing PresetLoader graph separately")
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if (type(position) is not list or len(position) != 2
            or any(type(value) is not int or abs(value) > 1000000 for value in position)):
        raise OperationError("validity_macro_position_invalid", "Use two integer coordinates in -1000000..1000000")
    values = functions._selectors(blueprint_name, graph_name, graph_id, graph_type)
    if values["graph_type"] not in ("", "any", "function"):
        raise OperationError("validity_macro_graph_type_unsupported", "Only an existing exact PresetLoader BFL function is admitted")
    return {**values, "node_position_json": functions.variables._json_text(position), "create_graph_if_missing": False}


def _guid(value):
    if type(value) is not str:
        raise ValueError("missing physical GUID")
    result = uuid.UUID(value)
    if not result.int:
        raise ValueError("zero physical GUID")
    return result.hex


def _pin_type(value):
    primary_bools = ("is_const", "is_reference", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision")
    terminal_bools = ("is_const", "is_weak_pointer", "is_uobject_wrapper")
    if (type(value) is not dict or value.get("observation_complete") is not True
            or any(type(value.get(key)) is not str for key in ("category", "subcategory", "type_object"))
            or type(value.get("container_type")) is not int or value["container_type"] != 0
            or any(type(value.get(key)) is not bool for key in primary_bools)
            or value["is_weak_pointer"] or value["is_uobject_wrapper"] or value["serialize_single_precision"]):
        raise ValueError("missing full scalar macro type")
    terminal = value.get("terminal")
    if (type(terminal) is not dict
            or set(terminal) != {"category", "subcategory", "type_object", *terminal_bools}
            or terminal["category"] != "None" or terminal["subcategory"] != "None" or terminal["type_object"] != ""
            or any(type(terminal[key]) is not bool or terminal[key] for key in terminal_bools)):
        raise ValueError("unexpected scalar macro terminal payload")
    if value["category"] not in ("exec", "object") or value["subcategory"] != "None":
        raise ValueError("only current stock exec and UObject ports are admitted")
    if value["type_object"] != ("/Script/CoreUObject.Object" if value["category"] == "object" else ""):
        raise ValueError("macro source is not exact UObject/exec")
    return value


def _macro_observation(observation):
    """Validate copied source-derived ports; performs no SDK query or dispatch."""
    if (type(observation) is not dict or observation.get("whole_macro_shape_verified") is not True
            or observation.get("runtime_validity_verified") is not False
            or observation.get("node_class") != "/Script/BlueprintGraph.K2Node_MacroInstance"
            or type(observation.get("node_path")) is not str or not observation["node_path"]
            or type(observation.get("target_context")) is not dict or not observation["target_context"]):
        raise ValueError("missing current stock validity macro roles")
    _guid(observation["node_guid"])
    source = observation.get("native_source")
    if (type(source) is not dict or not source
            or source.get("macro_graph_path") != "/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:IsValid"):
        raise ValueError("missing exact installed stock IsValid graph")
    _guid(source["macro_graph_guid"])
    ports, pins = source.get("tunnel_ports"), observation.get("macro_pins")
    if type(ports) is not list or len(ports) != 4 or type(pins) is not list or len(pins) != len(ports):
        raise ValueError("whole current tunnel and instance inventory required")
    expected = {}
    for port in ports:
        if (type(port) is not dict or type(port.get("name")) is not str or not port["name"]
                or port.get("direction") not in ("input", "output")):
            raise ValueError("invalid current tunnel port")
        key = (port["name"], "input" if port["direction"] == "output" else "output")
        if key in expected:
            raise ValueError("duplicate source tunnel role")
        if type(port.get("role")) is not str or not port["role"]:
            raise ValueError("missing observed tunnel role")
        expected[key] = _pin_type(port["pin_type"])
    if (sum(value["category"] == "object" and key[1] == "input" for key, value in expected.items()) != 1
            or sum(value["category"] == "exec" and key[1] == "input" for key, value in expected.items()) != 1
            or sum(value["category"] == "exec" and key[1] == "output" for key, value in expected.items()) != 2):
        raise ValueError("stock source exec split and Object input differ")
    source_roles = {(port["name"], "input" if port["direction"] == "output" else "output"):port["role"] for port in ports}
    def instance(value):
        if (type(value) is not dict or value.get("whole_macro_shape_verified") is not True
                or value.get("node_class") != "/Script/BlueprintGraph.K2Node_MacroInstance"
                or type(value.get("node_path")) is not str or not value["node_path"]
                or type(value.get("macro_pins")) is not list or len(value["macro_pins"]) != len(ports)):
            raise ValueError("missing whole stock macro instance")
        _guid(value["node_guid"])
        pins = value["macro_pins"]
        inventory, identities = {}, set()
        for pin in pins:
            if type(pin) is not dict:
                raise ValueError("invalid macro pin row")
            key = (pin.get("name"), pin.get("direction"))
            identity = _guid(pin["pin_id"])
            if (key not in expected or key in inventory or identity in identities
                    or type(pin.get("flags")) is not int or not 0 <= pin["flags"] < 1024
                    or pin["flags"] & ((1 << 0) | (1 << 1) | (1 << 8))
                    or pin.get("role") != source_roles[key]
                    or _pin_type(pin["pin_type"]) != expected[key]
                    or any(type(pin.get(field)) is not str for field in ("raw_default_value", "autogenerated_default_value",
                        "default_object_path", "pin_type_text", "pin_type_display", "projected_category"))
                    or pin.get("default_text_empty") is not True):
                raise ValueError("current instance pin differs from its source tunnel role")
            inventory[key] = pin
            identities.add(identity)
        if inventory.keys() != expected.keys():
            raise ValueError("source-derived macro inventory is incomplete")
    instance(observation)
    instances = observation.get("macro_instances")
    if type(instances) is not list or len(instances) not in (1, 2):
        raise ValueError("one or two current stock macro instances required")
    paths, identities = set(), set()
    aliases = ("node_path", "node_guid", "node_class", "macro_pins", "whole_macro_shape_verified")
    primary = None
    for value in instances:
        instance(value)
        if value["node_path"] in paths or _guid(value["node_guid"]) in identities:
            raise ValueError("duplicate stock macro instance")
        paths.add(value["node_path"])
        identities.add(_guid(value["node_guid"]))
        if value["node_path"] == observation["node_path"]:
            primary = value
    if primary is None or any(primary[key] != observation[key] for key in aliases):
        raise ValueError("primary macro observation differs from its current instance row")
    return observation


def _run(result, values, checkpoint_callback=None):
    result["data"].update(node_only_contract=True, implicit_data_wiring=False,
        stock_is_valid_exec_macro_requested=True, runtime_validity_verified=False,
        pure_is_valid_replacement=False, original_full_pipeline_parity=False,
        stock_source_precondition="Explicitly load stock StandardMacros through existing public asset/support APIs first; this constructor finds the fully loaded source only")
    try:
        functions._run_action(result, _OPERATION, values, checkpoint_callback, operation_api=api)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        raise
    data = result["data"]
    try:
        observation = _macro_observation(data["is_valid_exec_macro_observation"])
        if (data.get("native_is_valid_exec_macro") is not True or data.get("node_only_verified") is not True
                or data.get("stock_macro_load_attempted") is not False
                or data.get("runtime_validity_verified") is not False
                or data.get("source_unchanged") is not True or data.get("node_type") != "K2Node_MacroInstance"
                or len(observation["macro_instances"]) != 1
                or data.get("node_id") != observation["node_path"]
                or _guid(data["node_guid"]) != _guid(observation["node_guid"])
                or data.get("macro_source_before") != observation["native_source"]
                or data.get("macro_source_after") != observation["native_source"]):
            raise ValueError("created macro identity/source receipt differs")
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        result["stage"] = "verification"
        data["mutation_state_unknown"] = True
        raise OperationError("validity_macro_result_unverified", "Actual allocation prefix retained; source-derived stock macro receipt is incomplete") from error


def add_blueprint_is_valid_exec_macro_node(blueprint_name, node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    try:
        values = _arguments(blueprint_name, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate(_OPERATION, values, operation_id, lambda result: _run(result, values))


def add_blueprint_is_valid_exec_macro_node_action(result, blueprint_name, node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, _arguments(blueprint_name, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing),
        checkpoint_callback)
