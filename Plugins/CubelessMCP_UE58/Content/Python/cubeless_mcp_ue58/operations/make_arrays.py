"""Original MakeArray inputs and internal alias, one durable node-only journal."""
import json
import copy
import uuid

from . import arrays, functions, variables
from .state import OperationError, fail, mutate, new_result
from cubeless_mcp_ue58.compatibility.ue58 import make_arrays as api


def decode_registry_list(value, field, *, allow_finite_double=False):
    """The Registry accepts encoded JSON text; the domain accepts typed lists."""
    if value is None:
        return None
    if type(value) is not str:
        raise OperationError("make_array_json_invalid", field + " must be an encoded JSON array string or omitted/null")
    decoded = (variables._json_value(value, field, allow_finite_double=True)
        if allow_finite_double else variables._json_value(value, field))
    if decoded is not None and type(decoded) is not list:
        raise OperationError("make_array_values_invalid", field + " must decode to an array or null")
    return decoded


def _arguments(blueprint_name, element_type, input_count, values, type_object,
        enum_type, node_position, graph_name, graph_id, graph_type,
        create_graph_if_missing, element_defaults=None):
    element = functions._text(element_type, "element_type")
    normalized = arrays._normalize(element)
    canonical = (normalized if normalized in {"object", "class", "rotator", "transform"} else "struct" if normalized in {"struct", "scriptstruct"}
        else "enum" if normalized in {"enum", "byteenum"} else "vector" if normalized == "vector" else arrays._ELEMENTS.get(normalized))
    object_name = functions._text("" if type_object is None else type_object, "type_object", empty=True)
    enum_name = functions._text("" if enum_type is None else enum_type, "enum_type", empty=True)
    if canonical is None or (canonical not in {"enum", "object", "class", "rotator", "transform", "struct"} and (object_name or enum_name)):
        raise OperationError("make_array_element_type_unsupported", "Supported primitive/Vector/enum or native-checked object/class/Rotator/Transform/struct descriptors")
    if canonical == "struct" and not object_name:
        raise OperationError("make_array_type_descriptor_missing", "Struct/scriptstruct requires type_object; Native owns original path/name resolution and source admission")
    if canonical == "enum" and not (object_name.strip() or enum_name.strip()):
        raise OperationError("make_array_type_descriptor_missing", "Enum requires type_object or enum_type; Native validates the exact read-only UserDefinedEnum asset")
    supplied_values = values if values is not None else element_defaults
    if supplied_values is not None and (type(supplied_values) is not list or len(supplied_values) > 128):
        raise OperationError("make_array_values_invalid", "values/element_defaults must be a list of at most128 elements")
    if input_count is not None and (type(input_count) is not int or not 0 <= input_count <= 128):
        raise OperationError("make_array_input_count_invalid", "input_count must be null or an exact integer from0 through128")
    count = input_count if input_count is not None else (len(supplied_values) if supplied_values is not None else 1)
    if supplied_values is not None:
        if len(supplied_values) > count:
            raise OperationError("make_array_values_invalid", "values cannot contain more entries than explicit input_count")
        for value in supplied_values:
            if canonical == "vector" and not arrays._vector_literal(value):
                raise OperationError("make_array_value_invalid", "Vector elements need finite arrays, lowercase axis objects, or schema-valid literal strings")
            if canonical == "enum" and not ((type(value) is str and value.strip() and len(value) <= 512) or (type(value) is int and 0 <= value <= 255)):
                raise OperationError("make_array_value_invalid", "Enum elements require an entry name or exact byte integer; null/bool/fraction are rejected")
            if canonical == "bool" and type(value) is not bool:
                raise OperationError("make_array_value_invalid", "Bool elements require exact JSON booleans")
            if canonical == "int" and (type(value) is not int or not -(2**31) <= value <= 2**31-1):
                raise OperationError("make_array_value_invalid", "Int elements require exact signed int32; no fractional truncation")
    create = functions._bool(create_graph_if_missing, "create_graph_if_missing")
    if create:
        raise OperationError("make_array_graph_creation_unsupported", "Create the exact target graph separately before this node-only constructor")
    position = [0, 0] if node_position is None else node_position
    if type(position) is not list or len(position) != 2 or any(type(v) is not int or abs(v)>1000000 for v in position):
        raise OperationError("make_array_position_invalid", "node_position must contain two bounded integer coordinates")
    return {**functions._selectors(blueprint_name, graph_name, graph_id, graph_type),
        "element_type": element, "input_count_json": variables._json_text(input_count),
        "values_json": (json.dumps(supplied_values, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
            if canonical in {"name", "string", "int64", "float", "double", "text"} else variables._json_text(supplied_values)), "type_object": object_name,
        "enum_type": enum_name, "node_position_json": variables._json_text(position),
        "create_graph_if_missing": create}


def _descriptor_receipt(result, arguments):
    """Validate only copied known-success MakeArray data; no SDK query or value evaluation."""
    data = result["data"]
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True,
                descriptor_first_mismatch={"stage": "copied_native_make_array_success", "property": field,
                    "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                    "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("make_array_descriptor_receipt_invalid", "Actual native allocation/default/change prefix retained; copied MakeArray receipt inconsistent; no SDK follow-up or retry")
    normalized = arrays._normalize(arguments["element_type"])
    element = "struct" if normalized in {"struct", "scriptstruct"} else normalized
    category, subcategory = (element if element in {"object", "class"} else "struct"), "None"
    source = data.get("make_array_descriptor_source_before")
    require(type(source) is dict and source == data.get("make_array_descriptor_source_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("descriptor_kind") == element and type(source.get("type_path")) is str and bool(source["type_path"])
        and source.get("default_values_sampled") is False and type(source.get("roots")) is list and bool(source["roots"])
        and type(source.get("fields")) is list and type(source.get("files")) is list
        and data.get("make_array_descriptor_source_observed_unchanged") is True,
        "descriptor_source", "same complete current Native descriptor source without assumed CDO/owner/layout/file constants", source)
    type_path = source["type_path"]
    if element in {"rotator", "transform"}:
        require(type_path == "/Script/CoreUObject." + element.title(), "implicit_native_type", "implicit native Core type", type_path)
    values = json.loads(arguments["values_json"])
    supplied = values is not None
    requested = [] if values is None else values
    explicit_count = json.loads(arguments["input_count_json"])
    count = explicit_count if explicit_count is not None else len(requested) if supplied else 1
    require(type(count) is int and 0 <= count <= 128 and len(requested) <= count,
        "request.count", "existing bounded MakeArray precedence", count)
    require(data.get("native_descriptor_make_array") is True and data.get("element_type") == element
        and data.get("descriptor_runtime_invocation_verified") is False,
        "element_profile", (element, False), {key: data.get(key) for key in (
            "native_descriptor_make_array", "element_type", "descriptor_runtime_invocation_verified")})
    for key in ("allocated_native_shape_verified", "typed_node_shape_verified", "requested_defaults_verified",
            "node_only_verified", "created", "existing_nodes_preserved", "declarations_preserved", "native_source_observed_unchanged"):
        require(data.get(key) is True, key, True, data.get(key))
    for key, expected in (("input_count", count), ("values_requested_count", len(requested)), ("values_applied_count", len(requested))):
        require(type(data.get(key)) in (int, float) and data[key] == expected, key, expected, data.get(key))
    require(data.get("values_supplied") is supplied and data.get("input_count_supplied") is (explicit_count is not None),
        "supplied_inputs", (supplied, explicit_count is not None), (data.get("values_supplied"), data.get("input_count_supplied")))
    require(type(data.get("native_source_before")) is dict and data["native_source_before"] == data.get("native_source_after"),
        "native_source", "same copied native source before/after", data.get("native_source_after"))
    def pin_type(value, array, field):
        require(type(value) is dict and value.get("category") == category and value.get("subcategory") == subcategory
            and value.get("is_array") is array and type(value.get("is_reference")) is bool
            and value.get("subcategory_object") == type_path, field,
            {"category": category, "subcategory": subcategory, "is_array": array, "subcategory_object": type_path}, value)
    pin_type(data.get("element_pin_type"), False, "element_pin_type")
    node, typed = data.get("node"), data.get("typed_node_before_values")
    require(type(node) is dict and type(typed) is dict and node.get("node_class") == "/Script/BlueprintGraph.K2Node_MakeArray"
        and node.get("object_path") == data.get("object_path") and node.get("input_count") == count,
        "node", "same exact native MakeArray node/input count", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (ValueError, TypeError, AttributeError, KeyError):
        valid = False
    require(valid, "node_guid", "same nonzero returned node identity", {key: node.get(key) for key in ("node_id", "node_guid")})
    require({key: value for key, value in typed.items() if key != "pins"} == {key: value for key, value in node.items() if key != "pins"},
        "typed_node_identity", "all pre-value node fields preserved", node)
    def pins(snapshot, field):
        rows = snapshot.get("pins")
        require(type(rows) is list and len(rows) == count + 1, field, count + 1, rows)
        found, ids = {}, set()
        for row in rows:
            require(type(row) is dict, field, "whole copied pin object", row)
            key = row.get("name"), row.get("direction")
            allowed = key == ("Array", "output") or key in {(f"[{index}]", "input") for index in range(count)}
            try:
                identity = uuid.UUID(row.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
            except (ValueError, TypeError, AttributeError):
                valid = False
            require(valid and allowed and key not in found, field + ".identity", "unique current GUID and expected semantic port", row)
            ids.add(identity.hex); found[key] = row
            require(all(type(row.get(k)) is str for k in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
                and row.get("links") == [] and row.get("sub_pin_ids") == [] and row.get("parent_pin_id") == "",
                field + ".default_links", "actual whole unlinked/unsplit descriptor pin", row)
            pin_type(row.get("pin_type"), key == ("Array", "output"), field + ".type")
        return found
    before, after = pins(typed, "typed_node_before_values.pins"), pins(node, "node.pins")
    require(data.get("pins") == node["pins"], "pins_alias", "same final whole pins", data.get("pins"))
    receipts, applied = data.get("value_default_observations"), data.get("values_applied")
    expected_keys = {f"[{index}]" for index in range(len(requested))}
    require(type(receipts) is dict and set(receipts) == expected_keys and type(applied) is dict and set(applied) == expected_keys,
        "value_default_observations", "exact supplied input keys; omission/[] has none", receipts)
    for key, current in after.items():
        initial = before[key]
        ignored = {"default_value", "default_object", "default_text"} if key[0] in expected_keys else set()
        require({name: value for name, value in initial.items() if name not in ignored} ==
            {name: value for name, value in current.items() if name not in ignored},
            "pin.nonvalue_state." + str(key), "whole original state except supplied raw/Object/Text values", current)
        if key[0] not in expected_keys:
            continue
        observation = receipts[key[0]]
        require(type(observation) is dict and observation.get("pin_id") == current["pin_id"]
            and all(type(observation.get(name)) is str for name in ("requested_literal", "schema_literal", "raw_default", "default_object", "default_text", "default_as_string", "applied_value", "applied_kind"))
            and observation.get("raw_default") == current["default_value"]
            and observation.get("default_object") == current["default_object"]
            and observation.get("applied_kind") in {"object_path", "raw_default"}
            and observation.get("applied_value") == applied[key[0]] == current["default_object" if observation["applied_kind"] == "object_path" else "default_value"]
            and (observation["applied_kind"] != "object_path" or element in {"object", "class"})
            and observation.get("default_text") == current["default_text"]
            and observation.get("schema_match") is True and observation.get("current_default_valid") is True,
            "value.observation." + key[0], "same actual raw/Object/Text pin, typed applied result and successful schema/current receipt", observation)
        require(type(observation.get("pin_type")) is dict and all(observation["pin_type"].get(name) == current["pin_type"].get(name)
            for name in ("category", "subcategory", "subcategory_object", "is_array", "is_reference")),
            "value.pin_type." + key[0], "same actual copied type", observation.get("pin_type"))
        source_value = requested[int(key[0][1:-1])]
        if type(source_value) is str:
            require(observation["requested_literal"] == source_value, "value.literal." + key[0], source_value, observation["requested_literal"])
    data.update(descriptor_receipt_verified=True,
        descriptor_value_scope="Copied current source/node/default observations only; resolved Object/Class path differs from request aliases and raw schema text; no runtime array initialization or full loader/private state proof")


def _extended_primitive_receipt(result, arguments):
    """Validate only copied known-success MakeArray data; no SDK query or value evaluation."""
    data = result["data"]
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True,
                extended_primitive_first_mismatch={"stage": "copied_native_make_array_success", "property": field,
                    "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                    "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("make_array_extended_primitive_receipt_invalid", "Actual native allocation/default/change prefix retained; copied MakeArray receipt inconsistent; no SDK follow-up or retry")
    element = arrays._ELEMENTS.get(arrays._normalize(arguments["element_type"]))
    category, subcategory = {"int64": ("int64", "None"), "float": ("real", "float"),
        "double": ("real", "double"), "text": ("text", "None")}[element]
    values = json.loads(arguments["values_json"])
    supplied = values is not None
    requested = [] if values is None else values
    explicit_count = json.loads(arguments["input_count_json"])
    count = explicit_count if explicit_count is not None else len(requested) if supplied else 1
    require(type(count) is int and 0 <= count <= 128 and len(requested) <= count,
        "request.count", "existing bounded MakeArray precedence", count)
    require(data.get("native_extended_primitive_make_array") is True and data.get("element_type") == element
        and data.get("extended_primitive_runtime_invocation_verified") is False,
        "element_profile", (element, False), {key: data.get(key) for key in (
            "native_extended_primitive_make_array", "element_type", "extended_primitive_runtime_invocation_verified")})
    for key in ("allocated_native_shape_verified", "typed_node_shape_verified", "requested_defaults_verified",
            "node_only_verified", "created", "existing_nodes_preserved", "declarations_preserved", "native_source_observed_unchanged"):
        require(data.get(key) is True, key, True, data.get(key))
    for key, expected in (("input_count", count), ("values_requested_count", len(requested)), ("values_applied_count", len(requested))):
        require(type(data.get(key)) in (int, float) and data[key] == expected, key, expected, data.get(key))
    require(data.get("values_supplied") is supplied and data.get("input_count_supplied") is (explicit_count is not None),
        "supplied_inputs", (supplied, explicit_count is not None), (data.get("values_supplied"), data.get("input_count_supplied")))
    require(type(data.get("native_source_before")) is dict and data["native_source_before"] == data.get("native_source_after"),
        "native_source", "same copied native source before/after", data.get("native_source_after"))
    def pin_type(value, array, field):
        require(type(value) is dict and value.get("category") == category and value.get("subcategory") == subcategory
            and value.get("is_array") is array and type(value.get("is_reference")) is bool
            and value.get("subcategory_object") == "", field,
            {"category": category, "subcategory": subcategory, "is_array": array, "subcategory_object": ""}, value)
    pin_type(data.get("element_pin_type"), False, "element_pin_type")
    node, typed = data.get("node"), data.get("typed_node_before_values")
    require(type(node) is dict and type(typed) is dict and node.get("node_class") == "/Script/BlueprintGraph.K2Node_MakeArray"
        and node.get("object_path") == data.get("object_path") and node.get("input_count") == count,
        "node", "same exact native MakeArray node/input count", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (ValueError, TypeError, AttributeError, KeyError):
        valid = False
    require(valid, "node_guid", "same nonzero returned node identity", {key: node.get(key) for key in ("node_id", "node_guid")})
    require({key: value for key, value in typed.items() if key != "pins"} == {key: value for key, value in node.items() if key != "pins"},
        "typed_node_identity", "all pre-value node fields preserved", node)
    def pins(snapshot, field):
        rows = snapshot.get("pins")
        require(type(rows) is list and len(rows) == count + 1, field, count + 1, rows)
        found, ids = {}, set()
        for row in rows:
            require(type(row) is dict, field, "whole copied pin object", row)
            key = row.get("name"), row.get("direction")
            allowed = key == ("Array", "output") or key in {(f"[{index}]", "input") for index in range(count)}
            try:
                identity = uuid.UUID(row.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
            except (ValueError, TypeError, AttributeError):
                valid = False
            require(valid and allowed and key not in found, field + ".identity", "unique current GUID and expected semantic port", row)
            ids.add(identity.hex); found[key] = row
            require(all(type(row.get(k)) is str for k in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
                and row["default_object"] == "" and row.get("links") == [] and row.get("sub_pin_ids") == [] and row.get("parent_pin_id") == "",
                field + ".default_links", "actual whole unlinked/unsplit primitive pin", row)
            pin_type(row.get("pin_type"), key == ("Array", "output"), field + ".type")
        return found
    before, after = pins(typed, "typed_node_before_values.pins"), pins(node, "node.pins")
    require(data.get("pins") == node["pins"], "pins_alias", "same final whole pins", data.get("pins"))
    receipts, applied = data.get("value_default_observations"), data.get("values_applied")
    expected_keys = {f"[{index}]" for index in range(len(requested))}
    require(type(receipts) is dict and set(receipts) == expected_keys and type(applied) is dict and set(applied) == expected_keys,
        "value_default_observations", "exact supplied input keys; omission/[] has none", receipts)
    for key, current in after.items():
        initial = before[key]
        ignored = {"default_value", "default_text"} if key[0] in expected_keys else set()
        require({name: value for name, value in initial.items() if name not in ignored} ==
            {name: value for name, value in current.items() if name not in ignored},
            "pin.nonvalue_state." + str(key), "whole original state except supplied value raw/Text", current)
        if key[0] not in expected_keys:
            continue
        observation = receipts[key[0]]
        require(type(observation) is dict and observation.get("pin_id") == current["pin_id"]
            and all(type(observation.get(name)) is str for name in ("requested_literal", "schema_literal", "raw_default", "default_text", "default_as_string"))
            and observation.get("raw_default") == current["default_value"] == applied[key[0]]
            and observation.get("default_text") == current["default_text"]
            and observation.get("schema_match") is True and observation.get("current_default_valid") is True,
            "value.observation." + key[0], "same actual raw/FText pin and successful schema/current receipt", observation)
        require(type(observation.get("pin_type")) is dict and all(observation["pin_type"].get(name) == current["pin_type"].get(name)
            for name in ("category", "subcategory", "subcategory_object", "is_array", "is_reference")),
            "value.pin_type." + key[0], "same actual copied type", observation.get("pin_type"))
        source_value = requested[int(key[0][1:-1])]
        if type(source_value) is str:
            require(observation["requested_literal"] == source_value, "value.literal." + key[0], source_value, observation["requested_literal"])
    data.update(extended_primitive_receipt_verified=True,
        extended_primitive_value_scope="Copied node/default receipt only; raw/Text/schema literals are distinct, int64 numeric transport remains binary64, no runtime array initialization or evaluation")


def _run(result, arguments, checkpoint_callback=None):
    result["data"].update(node_only_contract=True, implicit_data_wiring=False,
        full_legacy_make_array_parity=False, array_runtime_execution_verified=False,
        supported_element_types=["int", "bool", "name", "string", "vector", "enum"], dependency_level="EngineAPI",
        input_representation="Domain values/element_defaults/node_position typed lists; Registry JSON strings",
        values_precedence="values before internal element_defaults; explicit input_count overrides inferred count")
    functions._run_action(result, "add_blueprint_make_array_node", arguments, checkpoint_callback, operation_api=api)
    # Existing shared native failure/partial/unknown gates run before copied success checks.
    if arrays._normalize(arguments["element_type"]) in {"object", "class", "rotator", "transform", "struct", "scriptstruct"}:
        result["success"] = False
        _descriptor_receipt(result, arguments)
        result["success"] = True
        result["data"].update(supported_element_types=["struct" if arrays._normalize(arguments["element_type"]) == "scriptstruct" else arrays._normalize(arguments["element_type"])])
    if arrays._ELEMENTS.get(arrays._normalize(arguments["element_type"])) in {"int64", "float", "double", "text"}:
        result["success"] = False
        _extended_primitive_receipt(result, arguments)
        result["success"] = True
        result["data"].update(supported_element_types=[arrays._ELEMENTS[arrays._normalize(arguments["element_type"])]])


def add_blueprint_make_array_node(blueprint_name, element_type, input_count=None,
        values=None, type_object="", enum_type="", node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False,
        operation_id=None, element_defaults=None):
    try:
        arguments = _arguments(blueprint_name, element_type, input_count, values,
            type_object, enum_type, node_position, graph_name, graph_id, graph_type,
            create_graph_if_missing, element_defaults)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("add_blueprint_make_array_node", arguments, operation_id,
        lambda result: _run(result, arguments))


def add_blueprint_make_array_node_action(result, blueprint_name, element_type,
        input_count=None, values=None, type_object="", enum_type="", node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False,
        checkpoint_callback=None, element_defaults=None):
    _run(result, _arguments(blueprint_name, element_type, input_count, values,
        type_object, enum_type, node_position, graph_name, graph_id, graph_type,
        create_graph_if_missing, element_defaults), checkpoint_callback)
