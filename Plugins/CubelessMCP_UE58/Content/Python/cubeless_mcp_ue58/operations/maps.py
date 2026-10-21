"""Finite String -> Vector/Double Map8 nodes; actual native source/default/partial receipts retained."""

import copy
import json
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import maps as api
from . import functions, variables
from .state import OperationError, fail, mutate, new_result

_OPERATIONS = frozenset({"clear", "find", "add", "contains", "remove", "keys", "values", "length"})


def _shared(validator, *args, **kwargs):
    try:
        return validator(*args, **kwargs)
    except OperationError as error:
        raise OperationError(error.code.replace("variable_", "map_").replace("function_", "map_"), str(error)) from error


def _arguments(blueprint_name, operation, key_type, value_type, param_defaults_json, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    operation = _shared(variables._text, operation, "operation")
    if operation not in _OPERATIONS:
        raise OperationError("map_operation_unsupported", "Exact Map operations are clear/find/add/contains/remove/keys/values/length for String-Vector or String-double")
    if key_type != "string" or value_type not in ("vector", "double"):
        raise OperationError("map_type_unsupported", "Only exact key_type=string and value_type=vector or double are supported; no float conversion or arbitrary map/struct loader")
    create = _shared(variables._boolean, create_graph_if_missing, "create_graph_if_missing")
    if create:
        raise OperationError("map_graph_creation_unsupported", "Requires an existing graph; create it with a separate named action")
    literal_operation = operation in {"find", "add", "contains", "remove"}
    defaults = (_shared(variables._json_value, param_defaults_json, "param_defaults_json", allow_finite_double=True)
        if literal_operation else _shared(variables._json_value, param_defaults_json, "param_defaults_json"))
    if type(defaults) is not dict:
        raise OperationError("map_literal_defaults_unsupported", "param_defaults_json must encode an object; TargetMap remains wire-only")
    allowed = {"key", "value"} if operation == "add" else {"key"} if literal_operation else set()
    names = [name.lower() for name in defaults]
    if any(not name.isascii() or name.lower() not in allowed for name in defaults) or len(set(names)) != len(names):
        raise OperationError("map_literal_defaults_unsupported", "Only ASCII case-insensitive Key (find/add/contains/remove) and Value (add) input defaults are admitted; no TargetMap/output/unknown or case-duplicate pins")
    position = _shared(variables._json_value, node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if type(position) is not list or len(position) != 2 or any(type(value) is not int or abs(value) > 1000000 for value in position):
        raise OperationError("map_position_invalid", "node_position_json must contain two integers from -1000000 to 1000000")
    selectors = _shared(functions._selectors, blueprint_name, graph_name, graph_id, graph_type)
    return {**selectors, "operation": operation, "key_type": key_type, "value_type": value_type,
        "param_defaults_json": param_defaults_json if defaults else "{}", "node_position_json": variables._json_text(position),
        "create_graph_if_missing": create}


def _literal_receipt(result, arguments):
    """Check copied typed-map authoring data after shared known-success gates; no SDK."""
    data = result["data"]
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True, map_literal_first_mismatch={"stage": "copied_native_map_success",
                "property": field, "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("map_literal_receipt_invalid", "Actual native Map allocation/default/change prefix retained; copied literal/type receipt inconsistent; no follow-up SDK or retry")
    requested = json.loads(arguments["param_defaults_json"])
    expected = {name.title(): value for name, value in requested.items()}
    operation, double = arguments["operation"], arguments["value_type"] == "double"
    value_type = ("real", "double", "") if double else ("struct", "None", "/Script/CoreUObject.Vector")
    for key in ("native_typed_map_literal_defaults", "map_literal_defaults_supported", "typed_unlinked_map_profile",
            "specialization_pending", "map_target_requires_connection", "created", "native_map_contract_verified",
            "native_map_source_observed_unchanged", "existing_nodes_and_declarations_preserved", "transaction_endpoint_observed"):
        require(data.get(key) is True, key, True, data.get(key))
    require(data.get("map_operation") == operation and data.get("map_runtime_execution_verified") is False,
        "operation_runtime", (operation, False), (data.get("map_operation"), data.get("map_runtime_execution_verified")))
    require(data.get("map_key_requires_connection") is ("Key" not in expected)
        and data.get("map_value_requires_connection") is (operation == "add" and "Value" not in expected),
        "literal_wire_roles", "only supplied literal inputs waive their own wire", (data.get("map_key_requires_connection"), data.get("map_value_requires_connection")))
    source = data.get("native_map_source_before")
    require(type(source) is dict and source == data.get("native_map_source_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True,
        "native_source", "complete same current native source before/after", data.get("native_map_source_after"))
    node = data.get("node")
    function = {"find": "Map_Find", "add": "Map_Add", "contains": "Map_Contains", "remove": "Map_Remove"}[operation]
    require(type(node) is dict and node.get("object_path") == node.get("node_id")
        and node.get("node_class_path") == "/Script/BlueprintGraph.K2Node_CallFunction"
        and node.get("function_owner_path") == "/Script/Engine.BlueprintMapLibrary" and node.get("function_name") == function,
        "node", "exact requested stock Map function/current node", node)
    try:
        valid_guid = bool(uuid.UUID(node["node_guid"]).int)
    except (KeyError, ValueError, TypeError, AttributeError):
        valid_guid = False
    require(valid_guid, "node_guid", "nonzero actual node GUID", node.get("node_guid"))
    roles = {("TargetMap", "input"), ("Key", "input"), ("self", "input")}
    if operation in ("find", "add"):
        roles.add(("Value", "output" if operation == "find" else "input"))
    if operation != "add":
        roles.add(("ReturnValue", "output"))
    if operation in ("add", "remove"):
        roles |= {("execute", "input"), ("then", "output")}
    def inventory(rows, field):
        require(type(rows) is list and len(rows) == len(roles), field, "whole stock pin roles", rows)
        found, ids = {}, set()
        for row in rows:
            require(type(row) is dict, field, "whole pin object", row)
            key = row.get("name"), row.get("direction")
            try:
                identity = uuid.UUID(row.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
            except (ValueError, TypeError, AttributeError):
                valid = False
            require(valid and key in roles and key not in found and row.get("owner_path") == node["object_path"]
                and row.get("whole_current_verified") is True and type(row.get("pin_type")) is dict,
                field + ".identity", "unique current GUID/owner/semantic stock role", row)
            found[key] = row; ids.add(identity.hex)
        return found
    allocated = inventory(data.get("map_allocated_pin_shapes"), "map_allocated_pin_shapes")
    current = inventory(data.get("map_initial_pin_shapes"), "map_initial_pin_shapes")
    for key, actual in current.items():
        original = allocated[key]
        projected = copy.deepcopy(original["pin_type"])
        if key[0] in ("TargetMap", "Key", "Value"):
            require(projected.get("category") == "wildcard", "allocated_type", "actual native wildcard before explicit profile seed", projected)
            cat, sub, path = value_type if key[0] == "Value" else ("string", "None", "")
            projected.update(category=cat, subcategory=sub, type_object=path)
            if key[0] == "TargetMap":
                require(projected.get("container_type") == 3 and type(projected.get("terminal")) is dict,
                    "TargetMap.container", "existing allocated Map container", projected)
                projected["terminal"].update(category=value_type[0], subcategory=value_type[1], type_object=value_type[2])
        require(actual["pin_type"] == projected, "typed_pin." + str(key), "requested primary/terminal types with all allocated ref/const/other cells preserved", actual["pin_type"])
        mutable = {"pin_type", "default_value", "autogenerated_default_value"} if key[0] in ("TargetMap", "Key", "Value") else set()
        require({name: value for name, value in original.items() if name not in mutable} ==
            {name: value for name, value in actual.items() if name not in mutable},
            "pin.nonvalue_state." + str(key), "whole allocated identity/flags/object/Text and other roles preserved", actual)
        if key[0] in ("Key", "Value") and key[1] == "input" and key[0] not in expected:
            require(actual.get("default_value") == actual.get("autogenerated_default_value"),
                "unrequested_stock_default." + key[0], "same observed stock raw/Auto", actual)
    observations, applied = data.get("map_literal_default_observations"), data.get("map_literal_defaults_applied")
    require(type(observations) is dict and set(observations) == set(expected) and type(applied) is dict and set(applied) == set(expected),
        "literal_inventory", "exact canonical supplied Key/Value only", observations)
    for name, observation in observations.items():
        pin = current[name, "input"]
        require(type(observation) is dict and observation.get("pin_id") == pin["pin_id"]
            and observation.get("pin_type") == pin["pin_type"]
            and all(type(observation.get(field)) is str for field in ("requested_literal", "schema_literal", "raw_default", "default_text", "default_as_string"))
            and observation.get("raw_default") == pin.get("default_value") == applied[name]
            and observation.get("default_text") == "" and pin.get("default_text_empty") is True
            and observation.get("schema_match") is True and observation.get("current_default_valid") is True,
            "literal_observation." + name, "same current actual pin/raw/schema match receipt", observation)
        if type(expected[name]) is str:
            require(observation["requested_literal"] == expected[name], "requested_literal." + name, expected[name], observation["requested_literal"])
    data.update(map_literal_receipt_verified=True,
        map_literal_scope="Explicit typed authoring seed and actual pin literals; TargetMap requires a separate actual producer wire; no runtime map values, connected specialization or whole pipeline proof")


def _run(result, arguments, checkpoint_callback=None):
    result["data"].update({"native_map_string_double" if arguments["value_type"] == "double" else "native_map_string_vector": True})
    result["data"].update(map_operation=arguments["operation"],
        specialization_pending=True, node_only_contract=True, implicit_data_wiring=False,
        map_target_requires_connection=True, map_key_requires_connection=arguments["operation"] in {"find", "add", "contains", "remove"},
        map_value_requires_connection=arguments["operation"] == "add", map_literal_defaults_supported=False,
        map_runtime_execution_verified=False, map_typed_specialization_verified=False,
        full_original_history_pipeline_verified=False)
    functions._run_action(result, "add_blueprint_map_function_node", arguments, checkpoint_callback, operation_api=api)
    if arguments["param_defaults_json"] != "{}":
        result["success"] = False
        _literal_receipt(result, arguments)
        result["success"] = True


def add_blueprint_map_function_node(blueprint_name, operation, key_type="string", value_type="vector",
        param_defaults_json="{}", node_position_json="[0,0]", graph_name=None, graph_id=None,
        graph_type=None, create_graph_if_missing=False, operation_id=None):
    try:
        arguments = _arguments(blueprint_name, operation, key_type, value_type, param_defaults_json,
            node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("add_blueprint_map_function_node", arguments, operation_id, lambda result: _run(result, arguments))


def add_blueprint_map_function_node_action(result, blueprint_name, operation, key_type="string", value_type="vector",
        param_defaults_json="{}", node_position_json="[0,0]", graph_name=None, graph_id=None,
        graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, _arguments(blueprint_name, operation, key_type, value_type, param_defaults_json,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
