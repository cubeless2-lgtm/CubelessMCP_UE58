"""Original three Break helpers, node-only effects and one shared mutation journal."""

import json
import math
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import struct_breaks as api
from . import functions, variables
from .state import OperationError, fail, mutate, new_result


_HELPERS = {"vector": "BreakVector", "rotator": "BreakRotator", "transform": "BreakTransform"}


def _shared(validator, *args, **kwargs):
    try:
        return validator(*args, **kwargs)
    except OperationError as error:
        raise OperationError(error.code.replace("function_", "struct_break_").replace("variable_", "struct_break_"), str(error)) from error


def _arguments(blueprint_name, struct_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    requested = _shared(variables._text, struct_type, "struct_type")
    canonical = requested.lower().strip().replace(" ", "").replace("_", "").replace("-", "")
    helper = _HELPERS.get(canonical)
    if helper is None:
        raise OperationError("struct_break_type_unsupported", "Legacy helper accepts vector, rotator or transform only; generic/native/user-defined K2 BreakStruct is a separate incomplete contract")
    create = _shared(variables._boolean, create_graph_if_missing, "create_graph_if_missing")
    call = _shared(functions._call_arguments, blueprint_name, "/Script/Engine.KismetMathLibrary", helper,
        "{}", node_position_json, graph_name, graph_id, graph_type, create, False, False, False, False, False)
    if call["graph_type"] not in ("", "any", "event", "function"):
        raise OperationError("struct_break_graph_type_unsupported", "This call-core cohort requires an existing owned event/function graph; macro/delegate variants remain incomplete")
    return {"blueprint_path": call["blueprint_path"], "struct_type": requested,
        **{key: call[key] for key in ("node_position_json", "graph_name", "graph_id", "graph_type", "create_graph_if_missing")}}


def _run(result, values, checkpoint_callback=None):
    result["data"].update(node_only_contract=not values["create_graph_if_missing"], implicit_data_wiring=False,
        full_legacy_break_struct_parity=False, generic_break_struct_supported=False,
        struct_break_runtime_execution_verified=False,
        native_construction_verification="requires actual registered call and saved/runtime evidence",
        incomplete_variants=["broader implicit graph creation" if values["create_graph_if_missing"] else "implicit graph creation", "macro/delegate targets", "generic native or user-defined K2 BreakStruct"])
    functions._run_action(result, "add_blueprint_break_struct_node", values, checkpoint_callback, operation_api=api)


def _typed_arguments(blueprint_name, struct_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    from . import data_table_nodes
    values = data_table_nodes._arguments(blueprint_name, struct_type, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing)
    values["struct_type"] = values.pop("row_struct_path")
    return values


def _scalar_field_inventory(observation, break_pins, source):
    """Check the finite NR copied DTO only; never read Unreal objects or call SDK."""
    fields = observation["scalar_fields"]
    count = observation["scalar_uds_field_count"]
    if (observation.get("scalar_uds_graph_profile") is not True or type(count) is not int
            or not 1 <= count <= 32 or type(fields) is not list or len(fields) != count
            or type(source) is not dict or type(source.get("fields")) is not list
            or len(source["fields"]) != count):
        raise ValueError("missing bounded scalar source/full field inventory")
    defaults = observation["scalar_default_values_json"]
    if type(defaults) is not str or defaults != source.get("default_scalar_values_json"):
        raise ValueError("compiled scalar default capture differs from source")
    values = json.loads(defaults)
    if type(values) is not list or len(values) != count:
        raise ValueError("compiled default field count differs")
    categories = {"StrProperty": ("string", "None"), "NameProperty": ("name", "None"),
        "BoolProperty": ("bool", "None"), "IntProperty": ("int", "None"),
        "Int64Property": ("int64", "None"), "FloatProperty": ("real", "float"),
        "DoubleProperty": ("real", "double")}
    type_keys = {"category", "subcategory", "type_object", "container_type", "is_reference", "is_const",
        "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision", "terminal"}
    terminal = {"category": "None", "subcategory": "None", "type_object": "", "is_const": False,
        "is_weak_pointer": False, "is_uobject_wrapper": False}
    def guid(value, nonzero=True):
        if type(value) is not str:
            raise ValueError("GUID must be copied text")
        parsed = uuid.UUID(value)
        if nonzero and not parsed.int:
            raise ValueError("zero authored/physical GUID")
        return parsed.hex
    def full_type(value):
        if (type(value) is not dict or set(value) != type_keys
                or any(type(value[key]) is not str for key in ("category", "subcategory", "type_object"))
                or type(value["container_type"]) is not int
                or any(type(value[key]) is not bool for key in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
                or type(value["terminal"]) is not dict or value["terminal"] != terminal
                or any(type(value["terminal"].get(key)) is not bool for key in ("is_const", "is_weak_pointer", "is_uobject_wrapper"))):
            raise ValueError("incomplete full PinType descriptor")
    if type(break_pins) is not list or len(break_pins) != count + 1:
        raise ValueError("Break physical pin count differs from all fields plus input")
    physical, pin_ids = {}, set()
    for pin in break_pins:
        if (type(pin) is not dict or type(pin.get("name")) is not str
                or pin.get("direction") not in ("input", "output")
                or type(pin.get("flags")) is not int or not 0 <= pin["flags"] < 1024
                or any(type(pin.get(key)) is not str for key in ("raw_default_value", "autogenerated_default_value", "default_object_path", "pin_type_text", "pin_type_display", "projected_category"))
                or type(pin.get("default_text_empty")) is not bool):
            raise ValueError("incomplete physical pin/default inventory")
        key = pin["name"], pin["direction"]
        identity = guid(pin["pin_id"])
        guid(pin["persistent_guid"], nonzero=False)
        if key in physical or identity in pin_ids:
            raise ValueError("duplicate physical pin role or PinId")
        physical[key] = pin; pin_ids.add(identity); full_type(pin["pin_type"])
    names, authored, field_guids = set(), set(), set()
    for field, original, default in zip(fields, source["fields"], values):
        if type(field) is not dict or type(original) is not dict:
            raise ValueError("field record is not copied JSON")
        name, friendly = field["property_name"], field["authored_name"]
        identity = guid(field["field_guid"])
        if (type(name) is not str or not name or type(friendly) is not str or not friendly
                or name in names or friendly in authored or identity in field_guids
                or field["actual_output_pin_name"] != name or original["name"] != name
                or original["authored_name"] != friendly or guid(original["field_guid"]) != identity):
            raise ValueError("whole authored GUID/name source mapping differs")
        names.add(name); authored.add(friendly); field_guids.add(identity)
        kind = original["class"]
        if kind not in categories:
            raise ValueError("field kind exceeds bounded NQ scalar scope")
        expected = field["expected_pin_type"]; full_type(expected)
        if ((expected["category"], expected["subcategory"]) != categories[kind]
                or expected["container_type"] != 0 or expected["type_object"] != ""
                or expected["is_weak_pointer"] or expected["is_uobject_wrapper"]):
            raise ValueError("converted scalar full PinType differs from source kind")
        output = physical[(name, "output")]
        if (field["output_pin"] != output or output["pin_type"] != expected
                or guid(output["persistent_guid"]) != identity):
            raise ValueError("actual GUID output/full PinType differs from complete inventory")
        if kind in ("StrProperty", "NameProperty"):
            valid = type(default) is str
        elif kind == "BoolProperty":
            valid = type(default) is bool
        elif kind in ("IntProperty", "Int64Property"):
            valid = type(default) is str and default.lstrip("-").isascii() and default.lstrip("-").isdigit()
            if valid:
                bits = 32 if kind == "IntProperty" else 64
                valid = -(2 ** (bits - 1)) <= int(default) < 2 ** (bits - 1)
        else:
            valid = type(default) in (int, float) and abs(default) <= 1.7976931348623157e308 and math.isfinite(default)
        if not valid:
            raise ValueError("compiled scalar default capture has wrong exact value kind")
    if {name for name, direction in physical if direction == "output"} != names:
        raise ValueError("output inventory omits or adds an authored field")
    inputs = [pin for (name, direction), pin in physical.items() if direction == "input"]
    row_type = inputs[0]["pin_type"] if len(inputs) == 1 else {}
    if (row_type.get("category") != "struct" or row_type.get("subcategory") != "None"
            or row_type.get("container_type") != 0 or row_type.get("type_object") != observation["row_struct_path"]
            or row_type.get("is_reference") is not True or row_type.get("is_const") is not True
            or row_type.get("is_weak_pointer") is not False or row_type.get("is_uobject_wrapper") is not False):
        raise ValueError("current Break input is not the exact struct const-reference")
    return fields


def _run_typed(result, values, checkpoint_callback=None):
    result["data"].update(node_only_contract=True, implicit_data_wiring=False,
        full_legacy_break_struct_parity=False, generic_break_struct_supported=False,
        struct_break_runtime_execution_verified=False,
        typed_string_struct_scope="exact saved-clean SourceString FString1 UDS; actual authored GUID pin")
    functions._run_action(result, "add_blueprint_break_struct_node", values, checkpoint_callback, operation_api=api)
    data = result["data"]
    if "native_scalar_uds_graph_node" in data or data.get("scalar_uds_graph_profile") is True:
        data.update(typed_string_struct_scope="not applicable to explicit scalar UDS profile",
            typed_scalar_struct_scope="saved-clean compiled 1..32 FString/Name/bool/int32/int64/float/double UDS fields; whole authored GUID/full PinType inventory",
            scalar_uds_runtime_values_verified=False)
        try:
            source = data["row_struct_source_before"]
            if (data.get("native_scalar_uds_graph_node") is not True or data.get("scalar_uds_graph_profile") is not True
                    or data.get("native_string1_graph_node") is True
                    or data.get("row_struct_source_unchanged") is not True or source != data["row_struct_source_after"]
                    or data.get("row_struct_path") != values["struct_type"]
                    or data.get("observations_complete") is not True or data.get("source_preserved") is not True
                    or data.get("unselected_state_preserved") is not True or data.get("independent_readback_passed") is not True
                    or not uuid.UUID(data["struct_guid"]).int
                    or uuid.UUID(source["struct_guid"]) != uuid.UUID(data["struct_guid"])):
                raise ValueError("scalar Break source/current result is incomplete")
            _scalar_field_inventory(data, data["node"]["pins"], source)
        except (KeyError, TypeError, ValueError, AttributeError, OverflowError) as error:
            result["stage"] = "verification"
            raise OperationError("struct_break_scalar_result_unverified", "Actual creation prefix retained; scalar source/GUID/full PinType inventory is incomplete") from error
        data["whole_scalar_field_inventory_verified"] = True


def _asset_data_arguments(blueprint_name, struct_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    # One native type, without routing the source selector through UDS /Game
    # validation or accepting arbitrary script structs.
    if type(struct_type) is not str or struct_type != "/Script/CoreUObject.AssetData":
        raise OperationError("struct_break_type_unsupported", "Only exact native AssetData is admitted by this branch")
    create = _shared(variables._boolean, create_graph_if_missing, "create_graph_if_missing")
    if create:
        raise OperationError("struct_break_graph_creation_unsupported", "Resolve the exact existing graph separately")
    position = _shared(functions._json, node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if (type(position) is not list or len(position) != 2
            or any(type(value) is not int or abs(value) > 1000000 for value in position)):
        raise OperationError("struct_break_position_invalid", "Use two integer coordinates in -1000000..1000000")
    values = _shared(functions._selectors, blueprint_name, graph_name, graph_id, graph_type)
    if values["graph_type"] not in ("", "any", "function"):
        raise OperationError("struct_break_asset_data_graph_unsupported", "Exact existing PresetLoader BFL function is required; native independently validates current roles")
    return {**values, "struct_type": struct_type, "node_position_json": variables._json_text(position),
        "create_graph_if_missing": False}


def _run_asset_data(result, values, checkpoint_callback=None):
    result["data"].update(node_only_contract=True, implicit_data_wiring=False,
        full_legacy_break_struct_parity=False, generic_break_struct_supported=False,
        asset_data_break_requested=True, asset_data_literal_inputs_supported=False,
        asset_data_runtime_values_verified=False,
        typed_native_struct_scope="exact native FAssetData whole BlueprintVisible fields; exact PresetLoader BFL")
    functions._run_action(result, "add_blueprint_break_struct_node", values, checkpoint_callback, operation_api=api)
    data = result["data"]
    fields = data.get("exposed_fields")
    if (data.get("native_asset_data_break") is not True
            or data.get("struct_type") != "/Script/CoreUObject.AssetData"
            or data.get("source_unchanged") is not True
            or type(data.get("native_source_before")) is not dict
            or not data["native_source_before"] or data["native_source_before"] != data.get("native_source_after")
            or data.get("whole_exposed_shape_verified") is not True or data.get("node_only_verified") is not True
            or data.get("specialization_pending") is not False or data.get("producer_connection_verified") is not False
            or type(fields) is not list or len(fields) != 5
            or any(type(field) is not dict or type(field.get("property_name")) is not str for field in fields)
            or {field.get("property_name") for field in fields} != {"PackageName", "PackagePath", "AssetName", "AssetClass", "AssetClassPath"}):
        result["stage"] = "verification"
        raise OperationError("struct_break_asset_data_result_unverified", "Actual native creation prefix retained; whole current AssetData/source/node-only receipt is incomplete")



def add_blueprint_break_struct_node(blueprint_name, struct_type, node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    if type(struct_type) is str and struct_type == "/Script/CoreUObject.AssetData":
        try:
            values = _asset_data_arguments(blueprint_name, struct_type, node_position_json,
                graph_name, graph_id, graph_type, create_graph_if_missing)
        except Exception as error:
            return fail(new_result(operation_id), error)
        return mutate("add_blueprint_break_struct_node", values, operation_id, lambda result: _run_asset_data(result, values))
    if type(struct_type) is str and struct_type.startswith("/"):
        try:
            values = _typed_arguments(blueprint_name, struct_type, node_position_json,
                graph_name, graph_id, graph_type, create_graph_if_missing)
        except Exception as error:
            return fail(new_result(operation_id), error)
        return mutate("add_blueprint_break_struct_node", values, operation_id, lambda result: _run_typed(result, values))
    try:
        values = _arguments(blueprint_name, struct_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("add_blueprint_break_struct_node", values, operation_id, lambda result: _run(result, values))


def add_blueprint_break_struct_node_action(result, blueprint_name, struct_type, node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    if type(struct_type) is str and struct_type == "/Script/CoreUObject.AssetData":
        _run_asset_data(result, _asset_data_arguments(blueprint_name, struct_type, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
        return
    if type(struct_type) is str and struct_type.startswith("/"):
        _run_typed(result, _typed_arguments(blueprint_name, struct_type, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
        return
    _run(result, _arguments(blueprint_name, struct_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
