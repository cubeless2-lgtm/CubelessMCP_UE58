"""Legacy node-only arrays, including finite native component object nine-operation authoring."""

import json
import math
import copy
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import arrays as api
from . import functions, state, variables
from .state import OperationError, fail, mutate, new_result

_ALIASES = {
    "length": "length", "num": "length", "count": "length", "size": "length",
    "get": "get", "set": "set", "setarrayelem": "set", "add": "add",
    "addunique": "add_unique", "unique": "add_unique", "insert": "insert", "insertitem": "insert",
    "contains": "contains", "containsitem": "contains", "remove": "remove", "removeitem": "remove",
    "clear": "clear", "empty": "clear", "reset": "clear",
}
_ELEMENTS = {"int": "int", "integer": "int", "bool": "bool", "boolean": "bool", "name": "name", "string": "string",
    "int64": "int64", "integer64": "int64", "float": "float", "single": "float",
    "double": "double", "real": "double", "text": "text"}
_OBJECT_TYPES = frozenset({"/Script/Engine.ActorComponent", "/Script/Engine.ChildActorComponent"})
_OBJECT_OPERATIONS = frozenset({"length", "get", "set", "add", "add_unique", "insert", "contains", "remove", "clear"})
_SKELETAL_COMPONENT_TYPE = "/Script/Engine.SkeletalMeshComponent"
_SKELETAL_COMPONENT_OPERATIONS = _OBJECT_OPERATIONS
_DESCRIPTOR_LENGTH_TYPES = frozenset({"/Script/CoreUObject.Object", "/Script/Engine.Actor",
    "/Script/Engine.ActorComponent", "/Script/Engine.ChildActorComponent", "/Script/Engine.SkeletalMeshComponent"})
_VALUE_STRUCT_LENGTH_TYPES = {"rotator": "/Script/CoreUObject.Rotator", "transform": "/Script/CoreUObject.Transform"}
_DEFAULTS = {
    "length": {}, "get": {"index": "int"}, "set": {"index": "int", "item": "element", "bsizetofit": "bool"},
    "add": {"newitem": "element"}, "add_unique": {"newitem": "element"},
    "insert": {"newitem": "element", "index": "int"}, "contains": {"itemtofind": "element"},
    "remove": {"item": "element"}, "clear": {},
}


def _shared(validator, *args, **kwargs):
    try:
        return validator(*args, **kwargs)
    except OperationError as error:
        raise OperationError(error.code.replace("variable_", "array_").replace("function_", "array_"), str(error)) from error


def _normalize(value):
    # Exact legacy NormalizeTypeName: trim/lower and remove spaces, "_" and "-".
    return value.lower().strip().replace(" ", "").replace("_", "").replace("-", "")


def _vector_literal(value):
    """Validate finite original Vector shapes; Native owns string syntax and schema readback."""
    def number(axis):
        if type(axis) not in (int, float):
            return False
        try:
            return math.isfinite(axis)
        except OverflowError:
            return False
    if type(value) is str:
        return bool(value) and len(value) <= 65536 and "\x00" not in value
    if type(value) is list:
        return all(number(axis) for axis in value[:3])
    if type(value) is dict:
        return all(number(value[key]) for key in ("x", "y", "z") if key in value)
    return False


def _arguments(blueprint_name, operation, element_type, param_defaults_json, type_object, enum_type,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    operation = _shared(variables._text, operation, "operation")
    canonical_operation = _ALIASES.get(_normalize(operation))
    if canonical_operation is None:
        raise OperationError("array_operation_unsupported", "Supported operations: length, get, set, add, add_unique, insert, contains, remove, clear; remove is RemoveItem, not RemoveIndex")
    element_type = _shared(variables._text, element_type, "element_type")
    canonical_element = "vector4" if _normalize(element_type) in {"struct", "scriptstruct"} else _normalize(element_type) if _normalize(element_type) in {"object", "class", "rotator", "transform"} else "enum" if _normalize(element_type) in {"enum", "byteenum"} else "vector" if _normalize(element_type) == "vector" else _ELEMENTS.get(_normalize(element_type))
    if canonical_element is None:
        raise OperationError("array_element_type_unsupported", "Supported int/integer, int64/integer64, float/single, double/real, bool/boolean, name/string/text, enum/byteenum, and Vector across the existing nine operations; exact native Rotator/Transform use the same nine operations")
    type_object = _shared(variables._text, "" if type_object is None else type_object, "type_object", empty=True)
    enum_type = _shared(variables._text, "" if enum_type is None else enum_type, "enum_type", empty=True)
    if canonical_element == "vector4" and not (_normalize(element_type) == "struct" and type_object == "/Script/CoreUObject.Vector4"):
        canonical_element = "uds_length"
    if canonical_element == "uds_length":
        if not type_object:
            raise OperationError("array_uds_type_missing", "Struct/scriptstruct requires type_object; Native validates an exact loaded saved-clean scalar UserDefinedStruct")
    if canonical_element not in {"enum", "object", "class", "vector4", "rotator", "transform", "uds_length"} and (type_object or enum_type):
        raise OperationError("array_type_selector_unsupported", "Type selectors apply only to enum; Vector resolves exact native CoreUObject.Vector")
    if canonical_element == "vector4":
        if type_object != "/Script/CoreUObject.Vector4" or enum_type:
            raise OperationError("array_vector4_type_unsupported", "Struct requires exact /Script/CoreUObject.Vector4 and empty enum_type; f/d aliases and other structs remain unsupported")
    descriptor_length = (canonical_element == "class" or canonical_element == "object"
        and (type_object or "/Script/CoreUObject.Object") in {"/Script/CoreUObject.Object", "/Script/Engine.Actor"})
    if descriptor_length:
        if (type_object or "/Script/CoreUObject.Object") not in _DESCRIPTOR_LENGTH_TYPES or enum_type:
            raise OperationError("array_descriptor_type_unsupported", "This finite Object/Class scope requires an exact loaded native Object/Actor/ActorComponent/ChildActorComponent/SkeletalMeshComponent constraint and empty enum_type")
    if canonical_element == "object" and not descriptor_length:
        if canonical_operation not in _OBJECT_OPERATIONS and not (type_object == _SKELETAL_COMPONENT_TYPE and not enum_type and canonical_operation in _SKELETAL_COMPONENT_OPERATIONS):
            raise OperationError("array_object_operation_unsupported", "Finite native component object arrays admit the existing nine operations only")
        if type_object == _SKELETAL_COMPONENT_TYPE and not enum_type:
            if canonical_operation not in _SKELETAL_COMPONENT_OPERATIONS:
                raise OperationError("array_object_operation_unsupported", "Exact SkeletalMeshComponent arrays admit the existing nine operations only")
        elif type_object not in _OBJECT_TYPES or enum_type:
            raise OperationError("array_object_type_unsupported", "Object requires exact ActorComponent/ChildActorComponent or finite SkeletalMeshComponent type_object and empty enum_type; no aliases/generated classes")
    if canonical_element == "enum" and not (type_object.strip() or enum_type.strip()):
        raise OperationError("array_type_selector_missing", "Enum requires type_object or enum_type; Native validates the exact read-only UserDefinedEnum asset")
    create = _shared(variables._boolean, create_graph_if_missing, "create_graph_if_missing")
    if canonical_element in {"int64", "float", "double", "text", "rotator", "transform"} or (descriptor_length or canonical_element == "uds_length") and canonical_operation != "length":
        defaults = _shared(variables._json_value, param_defaults_json, "param_defaults_json", allow_finite_double=True)
    else:
        defaults = _shared(variables._json_value, param_defaults_json, "param_defaults_json")
    if defaults is None:
        defaults = {}  # Legacy param_defaults=None.
    if type(defaults) is not dict or len(defaults) > 64 or len({name.casefold() for name in defaults}) != len(defaults):
        raise OperationError("array_defaults_invalid", "param_defaults_json must be an object of at most 64 distinct case-insensitive pin names")
    for name, value in defaults.items():
        key = name.casefold()
        if key == "targetarray":
            raise OperationError("array_target_literal_unsupported", "TargetArray must be connected later; array literal defaults are not supported")
        kind = _DEFAULTS[canonical_operation].get(key)
        if kind is None:
            raise OperationError("array_default_pin_unsupported", "Unknown or unsupported input default for this operation: " + name)
        kind = canonical_element if kind == "element" else kind
        if kind == "vector4" and (type(value) is not str or not value or len(value) > 65536 or "\x00" in value):
            raise OperationError("array_vector4_literal_unsupported", "Vector4 item literals accept bounded complete finite XYZW strings only; empty/partial forms remain outside this slice and JSON arrays/objects/null are not converted. Native owns exact grammar, public ImportText and current checks before target lookup/writer.")
        if kind == "object" and not (descriptor_length and canonical_operation != "length"):
            raise OperationError("array_object_literal_unsupported", "Object " + name + " is wire-only; literal/null never loads an object or dispatches a node writer")
        if kind == "vector" and not _vector_literal(value):
            raise OperationError("array_default_type_invalid", name + " must be a finite Vector array, lowercase axis object, or schema-valid literal string")
        if kind == "enum" and not ((type(value) is str and value.strip() and len(value) <= 512) or (type(value) is int and 0 <= value <= 255)):
            raise OperationError("array_default_type_invalid", name + " must be an enum entry name or exact byte integer; Native checks visible membership")
        if kind == "bool" and type(value) is not bool:
            raise OperationError("array_default_type_invalid", name + " must be a JSON boolean")
        if kind == "int" and (type(value) is not int or not -(2**31) <= value <= 2**31 - 1):
            raise OperationError("array_default_type_invalid", name + " must be a JSON int32 integer; no fractional truncation")
    position = _shared(variables._json_value, node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]  # Legacy node_position=None.
    if type(position) is not list or len(position) != 2 or any(type(value) is not int or abs(value) > 1000000 for value in position):
        raise OperationError("array_position_invalid", "node_position_json must contain two integers from -1000000 to 1000000")
    # Preserve original operation/element alias text and pin-key spelling in the
    # native request; normalization is admission logic, not an input drop.
    return {**_shared(functions._selectors, blueprint_name, graph_name, graph_id, graph_type),
        "operation": operation, "element_type": element_type, "param_defaults_json": (json.dumps(defaults, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
            if canonical_element in {"name", "string", "int64", "float", "double", "text", "rotator", "transform"} or (descriptor_length or canonical_element == "uds_length") and canonical_operation != "length" else variables._json_text(defaults)),
        "type_object": type_object, "enum_type": enum_type, "node_position_json": variables._json_text(position),
        "create_graph_if_missing": create}


def _uds_length_required(values):
    return (_ALIASES[_normalize(values["operation"])] == "length"
        and _normalize(values["element_type"]) in {"struct", "scriptstruct"}
        and not (_normalize(values["element_type"]) == "struct" and values["type_object"] == "/Script/CoreUObject.Vector4"))


def _uds_length_receipt(result, values):
    """Copied saved-clean scalar UDS Length proof; no loading or value sampling."""
    data = result["data"]
    path = data.get("scalar_uds_array_length_path")
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True, scalar_uds_length_first_mismatch={
                "stage": "copied_native_scalar_uds_length_success", "property": field,
                "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("array_scalar_uds_length_receipt_invalid", "Actual native allocation/change prefix retained; copied UDS Length receipt inconsistent; no SDK follow-up or retry")
    require(_uds_length_required(values) and data.get("native_scalar_uds_array_length") is True
        and type(path) is str and bool(path) and path.casefold() == values["type_object"].casefold()
        and data.get("canonical_operation") == "length", "uds_length_profile", "request resolves to the actual canonical scalar UDS Length source",
        {key: data.get(key) for key in ("native_scalar_uds_array_length", "scalar_uds_array_length_path", "canonical_operation")})
    for key in ("typed_node_shape_verified", "node_only_verified", "created", "existing_nodes_preserved", "declarations_preserved",
            "native_source_observed_unchanged", "requested_defaults_verified", "scalar_uds_array_length_source_preserved",
            "scalar_uds_length_target_source_queries_preserved", "scalar_uds_array_length_pin_shape_verified"):
        require(data.get(key) is True, key, True, data.get(key))
    require(data.get("scalar_uds_array_length_runtime_invocation_verified") is False,
        "runtime_invocation", False, data.get("scalar_uds_array_length_runtime_invocation_verified"))
    require(type(data.get("native_signature_before")) is dict and data["native_signature_before"] == data.get("native_signature_after"),
        "native_signature", "same whole actual native signature", data.get("native_signature_after"))
    source, after = data.get("native_scalar_uds_array_length_source_before"), data.get("native_scalar_uds_array_length_source_after")
    require(type(source) is dict and type(after) is dict
        and json.dumps(source, sort_keys=True, ensure_ascii=False, separators=(",", ":")) == json.dumps(after, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True and source.get("struct_path") == path,
        "uds_source", "same complete source/metadata/file with exact actual casing and JSON types", source)
    require(all(source.get(key) is False for key in ("values_sampled", "default_buffer_sampled", "cdo_creation_requested", "runtime_invocation_verified"))
        and source.get("package_dirty") is False and type(source.get("status")) is int
        and all(type(source.get(key)) is int and source[key] >= 0 for key in ("struct_size", "struct_alignment"))
        and all(type(source.get(key)) is str and bool(source[key]) for key in ("package_path", "editor_data_path"))
        and all(type(source.get(key)) is str and source[key].isascii() and source[key].isdigit() for key in ("struct_flags_string", "package_flags_string"))
        and all(type(source.get(key)) is dict and all(type(k) is str and type(v) is str for k, v in source[key].items()) for key in ("root_metadata", "editor_data_metadata")),
        "source.shape", "typed saved-clean layout/status/metadata; no fixed byte/flag/status-number constants", source)
    try:
        valid = bool(uuid.UUID(source.get("struct_guid", "")).int)
    except (ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "struct_guid", "actual nonzero UDS GUID", source.get("struct_guid"))
    fields = source.get("fields")
    require(type(fields) is list and 1 <= len(fields) <= 32 and all(type(field) is dict for field in fields),
        "source.fields", "actual finite own scalar field inventory", fields)
    scalar = {"StrProperty": ("string", "None"), "IntProperty": ("int", "None"), "Int64Property": ("int64", "None"),
        "FloatProperty": ("real", "float"), "DoubleProperty": ("real", "double"), "BoolProperty": ("bool", "None"), "NameProperty": ("name", "None")}
    names, guids = set(), set()
    for field in fields:
        try:
            identity = uuid.UUID(field.get("field_guid", "")); valid = bool(identity.int) and identity.hex not in guids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and type(field.get("internal_name")) is str and bool(field["internal_name"]) and field["internal_name"] not in names
            and type(field.get("authored_name")) is str and field.get("owner_path") == path
            and type(field.get("class")) is str and field["class"] in scalar
            and (field.get("category"), field.get("subcategory")) == scalar[field["class"]]
            and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit()
            and all(type(field.get(key)) is int and field[key] >= 0 for key in ("offset", "size"))
            and type(field.get("array_dim")) is int and field["array_dim"] == 1
            and type(field.get("metadata")) is dict and all(type(k) is str and type(v) is str for k, v in field["metadata"].items()),
            "source.field", "unique GUID/name, canonical direct owner and actual scalar layout/type/metadata", field)
        names.add(field["internal_name"]); guids.add(identity.hex)
    file = source.get("file")
    require(type(file) is dict and file.get("exists") is True and type(file.get("filename")) is str and bool(file["filename"])
        and type(file.get("bytes")) is int and file["bytes"] >= 0 and type(file.get("sha1")) is str and len(file["sha1"]) == 40
        and all(c in "0123456789abcdefABCDEF" for c in file["sha1"]),
        "source.file", "actual saved source file observation unchanged across authoring", file)
    node = data.get("node")
    require(type(node) is dict and node.get("object_path") == data.get("object_path")
        and node.get("node_class") == "/Script/BlueprintGraph.K2Node_CallArrayFunction"
        and node.get("function_member_parent_path") == "/Script/Engine.KismetArrayLibrary"
        and node.get("function_name") == data.get("function_name") == "Array_Length", "node", "actual stock Array_Length call", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (KeyError, ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "node_guid", "same nonzero actual native node identity", node)
    roles = {("TargetArray", "input"): ("struct", True, path),
        ("self", "input"): ("object", False, "/Script/Engine.KismetArrayLibrary"), ("ReturnValue", "output"): ("int", False, "")}
    pins = node.get("pins")
    require(type(pins) is list and len(pins) == 3, "node.pins", "whole stock pure TargetArray/self/ReturnValue inventory", pins)
    found, ids = set(), set()
    for pin in pins:
        require(type(pin) is dict, "pin", "actual whole pin object", pin)
        key = pin.get("name"), pin.get("direction")
        try:
            identity = uuid.UUID(pin.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and key in roles and key not in found, "pin.identity", "unique actual stock semantic port/GUID", pin)
        found.add(key); ids.add(identity.hex)
        cat, array, binding = roles[key]; descriptor = pin.get("pin_type")
        require(type(descriptor) is dict and descriptor.get("category") == cat and descriptor.get("subcategory") == "None"
            and descriptor.get("subcategory_object") == binding and descriptor.get("is_array") is array
            and all(type(descriptor.get(flag)) is bool for flag in ("is_reference", "is_const"))
            and type(pin.get("pin_type_text")) is str and bool(pin["pin_type_text"]),
            "pin.type", "actual native binding/container/ref/const plus full native type export", pin)
        require(all(type(pin.get(key)) is str for key in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
            and pin.get("links") == [] and pin.get("sub_pin_ids") == [] and pin.get("parent_pin_id") == "",
            "pin.defaults_links", "actual unlinked unsplit stock defaults, distinct from runtime size", pin)
    require(json.loads(values["param_defaults_json"]) == {} and data.get("param_defaults_applied") == {},
        "defaults", "existing empty Length defaults; TargetArray is wire-only", data.get("param_defaults_applied"))
    data.update(scalar_uds_length_receipt_verified=True,
        scalar_uds_length_validation_scope="Actual scalar UDS GUID/metadata/layout/file and whole stock pins; request casing may differ, canonical source is exact, no values/default-buffer/runtime proof")


def _uds_operation_required(values):
    return (_ALIASES[_normalize(values["operation"])] != "length"
        and _normalize(values["element_type"]) in {"struct", "scriptstruct"}
        and not (_normalize(values["element_type"]) == "struct" and values["type_object"] == "/Script/CoreUObject.Vector4"))


def _uds_operation_receipt(result, values):
    """Copied scalar UDS authoring proof; generic literal import is deferred to compilation."""
    data, operation = result["data"], _ALIASES[_normalize(values["operation"])]
    path = data.get("scalar_uds_array_operation_path")
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True, scalar_uds_operation_first_mismatch={
                "stage": "copied_native_scalar_uds_operation_success", "property": field,
                "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("array_scalar_uds_operation_receipt_invalid", "Actual native allocation/default/change prefix retained; copied UDS operation receipt inconsistent; no SDK follow-up or retry")
    require(_uds_operation_required(values) and data.get("native_scalar_uds_array_operation_parity") is True
        and type(path) is str and bool(path) and path.casefold() == values["type_object"].casefold()
        and data.get("scalar_uds_array_operation") == data.get("canonical_operation") == operation,
        "uds_operation_profile", "request resolves to the actual canonical scalar UDS operation source",
        {key: data.get(key) for key in ("native_scalar_uds_array_operation_parity", "scalar_uds_array_operation_path", "scalar_uds_array_operation", "canonical_operation")})
    for key in ("typed_node_shape_verified", "node_only_verified", "created", "existing_nodes_preserved", "declarations_preserved",
            "native_source_observed_unchanged", "requested_defaults_verified", "scalar_uds_array_operation_source_preserved",
            "scalar_uds_array_operation_parity_verified", "scalar_uds_array_operation_pin_shape_verified",
            "scalar_uds_array_operation_target_source_queries_preserved"):
        require(data.get(key) is True, key, True, data.get(key))
    require(data.get("scalar_uds_array_operation_runtime_invocation_verified") is False
        and data.get("scalar_uds_array_literal_compile_validation_verified") is False,
        "runtime_compile_literal", "no runtime or generic struct literal ImportText/compile validation proof",
        {key: data.get(key) for key in ("scalar_uds_array_operation_runtime_invocation_verified", "scalar_uds_array_literal_compile_validation_verified")})
    require(type(data.get("native_signature_before")) is dict and data["native_signature_before"] == data.get("native_signature_after"),
        "native_signature", "same whole actual native signature", data.get("native_signature_after"))
    source, after = data.get("native_scalar_uds_array_operation_source_before"), data.get("native_scalar_uds_array_operation_source_after")
    require(type(source) is dict and type(after) is dict
        and json.dumps(source, sort_keys=True, ensure_ascii=False, separators=(",", ":")) == json.dumps(after, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True and source.get("struct_path") == path,
        "uds_source", "same complete source/metadata/file with exact actual casing and JSON types", source)
    require(all(source.get(key) is False for key in ("values_sampled", "default_buffer_sampled", "cdo_creation_requested", "runtime_invocation_verified"))
        and source.get("package_dirty") is False and type(source.get("status")) is int
        and all(type(source.get(key)) is int and source[key] >= 0 for key in ("struct_size", "struct_alignment"))
        and all(type(source.get(key)) is str and bool(source[key]) for key in ("package_path", "editor_data_path"))
        and all(type(source.get(key)) is str and source[key].isascii() and source[key].isdigit() for key in ("struct_flags_string", "package_flags_string"))
        and all(type(source.get(key)) is dict and all(type(k) is str and type(v) is str for k, v in source[key].items()) for key in ("root_metadata", "editor_data_metadata")),
        "source.shape", "typed saved-clean layout/status/metadata; no fixed byte/flag/status-number constants", source)
    try:
        valid = bool(uuid.UUID(source.get("struct_guid", "")).int)
    except (ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "struct_guid", "actual nonzero UDS GUID", source.get("struct_guid"))
    fields = source.get("fields")
    require(type(fields) is list and 1 <= len(fields) <= 32 and all(type(field) is dict for field in fields),
        "source.fields", "actual finite own scalar field inventory", fields)
    scalar = {"StrProperty": ("string", "None"), "IntProperty": ("int", "None"), "Int64Property": ("int64", "None"),
        "FloatProperty": ("real", "float"), "DoubleProperty": ("real", "double"), "BoolProperty": ("bool", "None"), "NameProperty": ("name", "None")}
    names, guids = set(), set()
    for field in fields:
        try:
            identity = uuid.UUID(field.get("field_guid", "")); valid = bool(identity.int) and identity.hex not in guids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and type(field.get("internal_name")) is str and bool(field["internal_name"]) and field["internal_name"] not in names
            and type(field.get("authored_name")) is str and field.get("owner_path") == path
            and type(field.get("class")) is str and field["class"] in scalar
            and (field.get("category"), field.get("subcategory")) == scalar[field["class"]]
            and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit()
            and all(type(field.get(key)) is int and field[key] >= 0 for key in ("offset", "size"))
            and type(field.get("array_dim")) is int and field["array_dim"] == 1
            and type(field.get("metadata")) is dict and all(type(k) is str and type(v) is str for k, v in field["metadata"].items()),
            "source.field", "unique GUID/name, canonical direct owner and actual scalar layout/type/metadata", field)
        names.add(field["internal_name"]); guids.add(identity.hex)
    file = source.get("file")
    require(type(file) is dict and file.get("exists") is True and type(file.get("filename")) is str and bool(file["filename"])
        and type(file.get("bytes")) is int and file["bytes"] >= 0 and type(file.get("sha1")) is str and len(file["sha1"]) == 40
        and all(c in "0123456789abcdefABCDEF" for c in file["sha1"]),
        "source.file", "actual saved source file observation unchanged across authoring", file)
    node = data.get("node")
    function = {"get": "Array_Get", "set": "Array_Set", "add": "Array_Add", "add_unique": "Array_AddUnique",
        "insert": "Array_Insert", "contains": "Array_Contains", "remove": "Array_RemoveItem", "clear": "Array_Clear"}[operation]
    require(type(node) is dict and node.get("object_path") == data.get("object_path")
        and node.get("node_class") == "/Script/BlueprintGraph.K2Node_CallArrayFunction"
        and node.get("function_member_parent_path") == "/Script/Engine.KismetArrayLibrary"
        and node.get("function_name") == data.get("function_name") == function,
        "node", "actual stock array call for the requested operation", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (KeyError, ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "node_guid", "same nonzero actual native node identity", node)
    roles = {("TargetArray", "input"): ("struct", True, path), ("self", "input"): ("object", False, "/Script/Engine.KismetArrayLibrary")}
    table = {"get": [("Index", "input", "int"), ("Item", "output", "element")],
        "set": [("Index", "input", "int"), ("Item", "input", "element"), ("bSizeToFit", "input", "bool")],
        "add": [("NewItem", "input", "element"), ("ReturnValue", "output", "int")],
        "add_unique": [("NewItem", "input", "element"), ("ReturnValue", "output", "int")],
        "insert": [("NewItem", "input", "element"), ("Index", "input", "int")],
        "contains": [("ItemToFind", "input", "element"), ("ReturnValue", "output", "bool")],
        "remove": [("Item", "input", "element"), ("ReturnValue", "output", "bool")], "clear": []}
    for name, direction, category in table[operation]:
        roles[name, direction] = ("struct", False, path) if category == "element" else (category, False, "")
    if operation not in {"get", "contains"}:
        roles.update({("execute", "input"): ("exec", False, ""), ("then", "output"): ("exec", False, "")})
    pins = node.get("pins")
    require(type(pins) is list and len(pins) == len(roles), "node.pins", "whole stock array operation pin inventory", pins)
    found, ids = {}, set()
    for pin in pins:
        require(type(pin) is dict, "pin", "actual whole pin object", pin)
        key = pin.get("name"), pin.get("direction")
        try:
            identity = uuid.UUID(pin.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and key in roles and key not in found, "pin.identity", "unique actual stock semantic port/GUID", pin)
        found[key] = pin; ids.add(identity.hex)
        category, array, binding = roles[key]; descriptor = pin.get("pin_type")
        require(type(descriptor) is dict and descriptor.get("category") == category and descriptor.get("subcategory") == "None"
            and descriptor.get("subcategory_object") == binding and descriptor.get("is_array") is array
            and all(type(descriptor.get(flag)) is bool for flag in ("is_reference", "is_const"))
            and type(pin.get("pin_type_text")) is str and bool(pin["pin_type_text"]),
            "pin.type", "actual native binding/container/ref/const plus full native type export", pin)
        require(all(type(pin.get(key)) is str for key in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
            and pin.get("links") == [] and pin.get("sub_pin_ids") == [] and pin.get("parent_pin_id") == "",
            "pin.defaults_links", "actual unlinked unsplit stock default cells", pin)
    before = data.get("typed_node_before_defaults")
    require(type(before) is dict and type(before.get("pins")) is list
        and {key: value for key, value in before.items() if key not in {"pins", "title"}} == {key: value for key, value in node.items() if key not in {"pins", "title"}},
        "typed_before.node", "same typed node identity and non-pin fields before defaults", before)
    initial = {pin.get("pin_id"): pin for pin in before["pins"] if type(pin) is dict}
    require(len(initial) == len(before["pins"]) == len(pins) and set(initial) == {pin["pin_id"] for pin in pins},
        "typed_before.pins", "same unique whole actual pin identities", before)
    defaults, applied, observations = json.loads(values["param_defaults_json"]), data.get("param_defaults_applied"), data.get("param_default_observations")
    require(type(applied) is dict and type(observations) is dict and set(applied) == set(observations) == set(defaults),
        "default_inventory", "exact requested pin spelling and actual raw observations", observations)
    requested_ids = set()
    for name, observation in observations.items():
        selected = [pin for (label, direction), pin in found.items() if direction == "input" and label.casefold() == name.casefold()]
        require(len(selected) == 1 and type(observation) is dict, "default.pin", "one current input pin for the requested default", observation)
        pin = selected[0]; requested_ids.add(pin["pin_id"])
        require(observation.get("pin_id") == pin["pin_id"]
            and all(type(observation.get(key)) is str for key in ("requested_literal", "schema_literal", "raw_default", "default_text", "default_as_string", "autogenerated_default_value", "default_object"))
            and observation.get("raw_default") == pin["default_value"] == applied[name]
            and all(observation.get(key) == pin[key] for key in ("default_text", "autogenerated_default_value", "default_object"))
            and observation.get("schema_match") is True and observation.get("current_default_valid") is True,
            "default.actual", "same actual raw/FText/Object/Auto and successful schema match; requested text need not equal raw", observation)
        require(type(observation.get("pin_type")) is dict and all(observation["pin_type"].get(key) == pin["pin_type"].get(key)
            for key in ("category", "subcategory", "subcategory_object", "is_array", "is_reference")),
            "default.type", "same current copied pin descriptor", observation.get("pin_type"))
    for pin in pins:
        omit = {"default_value", "default_text", "default_object"} if pin["pin_id"] in requested_ids else set()
        require({key: value for key, value in initial[pin["pin_id"]].items() if key not in omit}
            == {key: value for key, value in pin.items() if key not in omit},
            "typed_before.pin", "all actual non-requested defaults and whole roles/types/links/Auto preserved", pin)
    data.update(scalar_uds_operation_receipt_verified=True,
        scalar_uds_operation_validation_scope="Exact loaded saved-clean scalar UDS source/GUID/metadata/file and actual stock pins/default cells; original Any conversion with schema-stage acceptance only, generic struct literal ImportText remains deferred to compile, no runtime buffer/value proof")


def _value_struct_length_required(values):
    return (_ALIASES[_normalize(values["operation"])] == "length"
        and _normalize(values["element_type"]) in _VALUE_STRUCT_LENGTH_TYPES)


def _value_struct_length_receipt(result, values):
    """Copied native math-struct Length proof; no SDK, metadata or value sampling."""
    data, kind = result["data"], _normalize(values["element_type"])
    path = _VALUE_STRUCT_LENGTH_TYPES.get(kind)
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True, value_struct_length_first_mismatch={
                "stage": "copied_native_value_struct_length_success", "property": field,
                "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("array_value_struct_length_receipt_invalid", "Actual native allocation/change prefix retained; copied struct Length receipt inconsistent; no SDK follow-up or retry")
    require(_value_struct_length_required(values) and path is not None
        and data.get("native_value_struct_array_length") is True
        and data.get("value_struct_array_length_kind") == kind
        and data.get("value_struct_array_length_type_path") == path
        and data.get("canonical_operation") == "length", "value_struct_length_profile", (kind, path, "length"),
        {key: data.get(key) for key in ("native_value_struct_array_length", "value_struct_array_length_kind", "value_struct_array_length_type_path", "canonical_operation")})
    for key in ("typed_node_shape_verified", "node_only_verified", "created", "existing_nodes_preserved",
            "declarations_preserved", "native_source_observed_unchanged", "requested_defaults_verified", "value_struct_array_length_source_preserved"):
        require(data.get(key) is True, key, True, data.get(key))
    require(data.get("value_struct_array_length_runtime_invocation_verified") is False,
        "runtime_invocation", False, data.get("value_struct_array_length_runtime_invocation_verified"))
    require(type(data.get("native_signature_before")) is dict and data["native_signature_before"] == data.get("native_signature_after"),
        "native_signature", "same whole current native signature", data.get("native_signature_after"))
    source = data.get("native_value_struct_array_length_source_before")
    require(type(source) is dict and source == data.get("native_value_struct_array_length_source_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("struct_path") == path and source.get("metadata_observed") is False
        and source.get("values_sampled") is False and source.get("cdo_creation_requested") is False,
        "value_struct_source", "same held native source/bindings without metadata or value sampling", source)
    rows = source.get("structs")
    expected = {path} if kind == "rotator" else {path, "/Script/CoreUObject.Quat", "/Script/CoreUObject.Vector"}
    require(type(rows) is list and all(type(row) is dict for row in rows), "source.structs", "actual direct native source rows", rows)
    paths = []
    for row in rows:
        owner = row.get("path"); paths.append(owner)
        require(type(owner) is str and owner in expected and type(row.get("package_path")) is str and bool(row["package_path"])
            and all(type(row.get(key)) is str and row[key].isascii() and row[key].isdigit() for key in ("package_flags_string", "struct_flags_string"))
            and type(row.get("package_dirty")) is bool
            and all(type(row.get(key)) is int and row[key] >= 0 for key in ("struct_size", "struct_alignment")),
            "source.struct", "typed actual native layout/package observation; no byte-size constants", row)
        fields = row.get("native_fields")
        require(type(fields) is list and all(type(field) is dict for field in fields), "source.native_fields", "actual direct-owner field inventory", fields)
        names = []
        for field in fields:
            names.append(field.get("name"))
            require(type(field.get("name")) is str and bool(field["name"]) and field.get("owner_path") == owner
                and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit()
                and all(type(field.get(key)) is int and field[key] >= 0 for key in ("offset", "size"))
                and type(field.get("array_dim")) is int and field["array_dim"] == 1
                and field.get("class") == ("StructProperty" if owner == "/Script/CoreUObject.Transform" else "DoubleProperty")
                and type(field.get("struct_path")) is str
                and (field["struct_path"] == {"Rotation": "/Script/CoreUObject.Quat", "Translation": "/Script/CoreUObject.Vector", "Scale3D": "/Script/CoreUObject.Vector"}.get(field["name"])
                    if owner == "/Script/CoreUObject.Transform" else field["struct_path"] == ""),
                "source.field", "actual direct owner, scalar/nested binding and current field layout", field)
        required_names = {"Pitch", "Yaw", "Roll"} if owner.endswith(".Rotator") else {"Rotation", "Translation", "Scale3D"} if owner.endswith(".Transform") else {"X", "Y", "Z", "W"} if owner.endswith(".Quat") else {"X", "Y", "Z"}
        require(len(names) == len(required_names) and set(names) == required_names,
            "source.field_inventory", sorted(required_names), names)
    require(len(paths) == len(expected) and set(paths) == expected, "source.identity", sorted(expected), paths)
    node = data.get("node")
    require(type(node) is dict and node.get("object_path") == data.get("object_path")
        and node.get("node_class") == "/Script/BlueprintGraph.K2Node_CallArrayFunction"
        and node.get("function_member_parent_path") == "/Script/Engine.KismetArrayLibrary"
        and node.get("function_name") == data.get("function_name") == "Array_Length", "node", "actual stock Array_Length call", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (KeyError, ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "node_guid", "same nonzero actual native node identity", node)
    roles = {("TargetArray", "input"): ("struct", True, path),
        ("self", "input"): ("object", False, "/Script/Engine.KismetArrayLibrary"), ("ReturnValue", "output"): ("int", False, "")}
    pins = node.get("pins")
    require(type(pins) is list and len(pins) == 3, "node.pins", "whole stock pure TargetArray/self/ReturnValue inventory", pins)
    found, ids = set(), set()
    for pin in pins:
        require(type(pin) is dict, "pin", "actual whole pin object", pin)
        key = pin.get("name"), pin.get("direction")
        try:
            identity = uuid.UUID(pin.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and key in roles and key not in found, "pin.identity", "unique actual stock semantic port/GUID", pin)
        found.add(key); ids.add(identity.hex)
        cat, array, binding = roles[key]; descriptor = pin.get("pin_type")
        require(type(descriptor) is dict and descriptor.get("category") == cat and descriptor.get("subcategory") == "None"
            and descriptor.get("subcategory_object") == binding and descriptor.get("is_array") is array
            and all(type(descriptor.get(flag)) is bool for flag in ("is_reference", "is_const"))
            and type(pin.get("pin_type_text")) is str and bool(pin["pin_type_text"]),
            "pin.type", "actual native binding/container/ref/const plus full native type export", pin)
        require(all(type(pin.get(key)) is str for key in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
            and pin.get("links") == [] and pin.get("sub_pin_ids") == [] and pin.get("parent_pin_id") == "",
            "pin.defaults_links", "actual unlinked unsplit stock defaults, distinct from runtime size", pin)
    require(json.loads(values["param_defaults_json"]) == {} and data.get("param_defaults_applied") == {},
        "defaults", "existing empty Length defaults; TargetArray is wire-only", data.get("param_defaults_applied"))
    data.update(value_struct_length_receipt_verified=True,
        value_struct_length_validation_scope="Exact native source/direct bindings and stock three-pin authoring; no metadata, value/default-buffer sampling or runtime length proof")


def _value_struct_operation_required(values):
    return (_normalize(values["element_type"]) in _VALUE_STRUCT_LENGTH_TYPES
        and _ALIASES[_normalize(values["operation"])] != "length")


def _value_struct_operation_receipt(result, values):
    """Copied native math-struct operation/default proof after the shared failure gate."""
    data, kind = result["data"], _normalize(values["element_type"])
    path, operation = _VALUE_STRUCT_LENGTH_TYPES.get(kind), _ALIASES[_normalize(values["operation"])]
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True, value_struct_operation_first_mismatch={
                "stage": "copied_native_value_struct_operation_success", "property": field,
                "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("array_value_struct_operation_receipt_invalid", "Actual native allocation/default/change prefix retained; copied struct operation receipt inconsistent; no SDK follow-up or retry")
    require(_value_struct_operation_required(values) and path is not None
        and data.get("native_value_struct_array_operation_parity") is True
        and data.get("value_struct_array_operation") == data.get("canonical_operation") == operation
        and data.get("value_struct_array_kind") == kind and data.get("value_struct_array_type_path") == path,
        "operation_profile", (operation, kind, path), {key: data.get(key) for key in (
            "native_value_struct_array_operation_parity", "value_struct_array_operation", "canonical_operation", "value_struct_array_kind", "value_struct_array_type_path")})
    for key in ("typed_node_shape_verified", "node_only_verified", "created", "existing_nodes_preserved",
            "declarations_preserved", "native_source_observed_unchanged", "requested_defaults_verified",
            "value_struct_array_operation_parity_verified", "value_struct_array_operation_source_preserved"):
        require(data.get(key) is True, key, True, data.get(key))
    require(data.get("value_struct_array_runtime_invocation_verified") is False,
        "runtime_invocation", False, data.get("value_struct_array_runtime_invocation_verified"))
    require(type(data.get("native_signature_before")) is dict and data["native_signature_before"] == data.get("native_signature_after"),
        "native_signature", "same whole current native signature", data.get("native_signature_after"))
    source = data.get("native_value_struct_array_operation_source_before")
    require(type(source) is dict and source == data.get("native_value_struct_array_operation_source_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("struct_path") == path and source.get("metadata_observed") is False
        and source.get("values_sampled") is False and source.get("cdo_creation_requested") is False,
        "value_struct_source", "same held native source/bindings without metadata or value sampling", source)
    rows = source.get("structs")
    expected = {path} if kind == "rotator" else {path, "/Script/CoreUObject.Quat", "/Script/CoreUObject.Vector"}
    require(type(rows) is list and all(type(row) is dict for row in rows), "source.structs", "actual direct native source rows", rows)
    paths = []
    for row in rows:
        owner = row.get("path"); paths.append(owner)
        require(type(owner) is str and owner in expected and type(row.get("package_path")) is str and bool(row["package_path"])
            and all(type(row.get(key)) is str and row[key].isascii() and row[key].isdigit() for key in ("package_flags_string", "struct_flags_string"))
            and type(row.get("package_dirty")) is bool
            and all(type(row.get(key)) is int and row[key] >= 0 for key in ("struct_size", "struct_alignment")),
            "source.struct", "typed actual native layout/package observation; no byte-size constants", row)
        fields = row.get("native_fields")
        require(type(fields) is list and all(type(field) is dict for field in fields), "source.native_fields", "actual direct-owner field inventory", fields)
        names = []
        for field in fields:
            names.append(field.get("name"))
            require(type(field.get("name")) is str and bool(field["name"]) and field.get("owner_path") == owner
                and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit()
                and all(type(field.get(key)) is int and field[key] >= 0 for key in ("offset", "size"))
                and type(field.get("array_dim")) is int and field["array_dim"] == 1
                and field.get("class") == ("StructProperty" if owner == "/Script/CoreUObject.Transform" else "DoubleProperty")
                and type(field.get("struct_path")) is str
                and (field["struct_path"] == {"Rotation": "/Script/CoreUObject.Quat", "Translation": "/Script/CoreUObject.Vector", "Scale3D": "/Script/CoreUObject.Vector"}.get(field["name"])
                    if owner == "/Script/CoreUObject.Transform" else field["struct_path"] == ""),
                "source.field", "actual direct owner, scalar/nested binding and current field layout", field)
        required_names = {"Pitch", "Yaw", "Roll"} if owner.endswith(".Rotator") else {"Rotation", "Translation", "Scale3D"} if owner.endswith(".Transform") else {"X", "Y", "Z", "W"} if owner.endswith(".Quat") else {"X", "Y", "Z"}
        require(len(names) == len(required_names) and set(names) == required_names,
            "source.field_inventory", sorted(required_names), names)
    require(len(paths) == len(expected) and set(paths) == expected, "source.identity", sorted(expected), paths)
    node = data.get("node")
    function = {"get": "Array_Get", "set": "Array_Set", "add": "Array_Add", "add_unique": "Array_AddUnique",
        "insert": "Array_Insert", "contains": "Array_Contains", "remove": "Array_RemoveItem", "clear": "Array_Clear"}[operation]
    require(type(node) is dict and node.get("object_path") == data.get("object_path")
        and node.get("node_class") == "/Script/BlueprintGraph.K2Node_CallArrayFunction"
        and node.get("function_member_parent_path") == "/Script/Engine.KismetArrayLibrary"
        and node.get("function_name") == data.get("function_name") == function,
        "node", "actual stock array call for the requested operation", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (KeyError, ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "node_guid", "same nonzero actual native node identity", node)
    roles = {("TargetArray", "input"): ("struct", True, path), ("self", "input"): ("object", False, "/Script/Engine.KismetArrayLibrary")}
    table = {"get": [("Index", "input", "int"), ("Item", "output", "element")],
        "set": [("Index", "input", "int"), ("Item", "input", "element"), ("bSizeToFit", "input", "bool")],
        "add": [("NewItem", "input", "element"), ("ReturnValue", "output", "int")],
        "add_unique": [("NewItem", "input", "element"), ("ReturnValue", "output", "int")],
        "insert": [("NewItem", "input", "element"), ("Index", "input", "int")],
        "contains": [("ItemToFind", "input", "element"), ("ReturnValue", "output", "bool")],
        "remove": [("Item", "input", "element"), ("ReturnValue", "output", "bool")], "clear": []}
    for name, direction, category in table[operation]:
        roles[name, direction] = ("struct", False, path) if category == "element" else (category, False, "")
    if operation not in {"get", "contains"}:
        roles.update({("execute", "input"): ("exec", False, ""), ("then", "output"): ("exec", False, "")})
    pins = node.get("pins")
    require(type(pins) is list and len(pins) == len(roles), "node.pins", "whole stock array operation pin inventory", pins)
    found, ids = {}, set()
    for pin in pins:
        require(type(pin) is dict, "pin", "actual whole pin object", pin)
        key = pin.get("name"), pin.get("direction")
        try:
            identity = uuid.UUID(pin.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and key in roles and key not in found, "pin.identity", "unique actual stock semantic port/GUID", pin)
        found[key] = pin; ids.add(identity.hex)
        category, array, binding = roles[key]; descriptor = pin.get("pin_type")
        require(type(descriptor) is dict and descriptor.get("category") == category and descriptor.get("subcategory") == "None"
            and descriptor.get("subcategory_object") == binding and descriptor.get("is_array") is array
            and all(type(descriptor.get(flag)) is bool for flag in ("is_reference", "is_const"))
            and type(pin.get("pin_type_text")) is str and bool(pin["pin_type_text"]),
            "pin.type", "actual native binding/container/ref/const plus full native type export", pin)
        require(all(type(pin.get(key)) is str for key in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
            and pin.get("links") == [] and pin.get("sub_pin_ids") == [] and pin.get("parent_pin_id") == "",
            "pin.defaults_links", "actual unlinked unsplit stock default cells", pin)
    before = data.get("typed_node_before_defaults")
    require(type(before) is dict and type(before.get("pins")) is list
        and {key: value for key, value in before.items() if key not in {"pins", "title"}} == {key: value for key, value in node.items() if key not in {"pins", "title"}},
        "typed_before.node", "same typed node identity and non-pin fields before defaults", before)
    initial = {pin.get("pin_id"): pin for pin in before["pins"] if type(pin) is dict}
    require(len(initial) == len(before["pins"]) == len(pins) and set(initial) == {pin["pin_id"] for pin in pins},
        "typed_before.pins", "same unique whole actual pin identities", before)
    defaults, applied, observations = json.loads(values["param_defaults_json"]), data.get("param_defaults_applied"), data.get("param_default_observations")
    require(type(applied) is dict and type(observations) is dict and set(applied) == set(observations) == set(defaults),
        "default_inventory", "exact requested pin spelling and actual raw observations", observations)
    requested_ids = set()
    for name, observation in observations.items():
        selected = [pin for (label, direction), pin in found.items() if direction == "input" and label.casefold() == name.casefold()]
        require(len(selected) == 1 and type(observation) is dict, "default.pin", "one current input pin for the requested default", observation)
        pin = selected[0]; requested_ids.add(pin["pin_id"])
        require(observation.get("pin_id") == pin["pin_id"]
            and all(type(observation.get(key)) is str for key in ("requested_literal", "schema_literal", "raw_default", "default_text", "default_as_string", "autogenerated_default_value", "default_object"))
            and observation.get("raw_default") == pin["default_value"] == applied[name]
            and all(observation.get(key) == pin[key] for key in ("default_text", "autogenerated_default_value", "default_object"))
            and observation.get("schema_match") is True and observation.get("current_default_valid") is True,
            "default.actual", "same actual raw/FText/Object/Auto and successful schema match; requested text need not equal raw", observation)
        require(type(observation.get("pin_type")) is dict and all(observation["pin_type"].get(key) == pin["pin_type"].get(key)
            for key in ("category", "subcategory", "subcategory_object", "is_array", "is_reference")),
            "default.type", "same current copied pin descriptor", observation.get("pin_type"))
    for pin in pins:
        omit = {"default_value", "default_text", "default_object"} if pin["pin_id"] in requested_ids else set()
        require({key: value for key, value in initial[pin["pin_id"]].items() if key not in omit}
            == {key: value for key, value in pin.items() if key not in omit},
            "typed_before.pin", "all actual non-requested defaults and whole roles/types/links/Auto preserved", pin)
    data.update(value_struct_operation_receipt_verified=True,
        value_struct_operation_validation_scope="Current native source/direct bindings and actual stock node/default observations; original Any conversion and SDK acceptance, no runtime buffer/equality/mutation proof")


def _descriptor_operation_required(values):
    category = _normalize(values["element_type"])
    path = values["type_object"] or "/Script/CoreUObject.Object"
    return (_ALIASES[_normalize(values["operation"])] != "length" and
        (category == "class" or category == "object" and path in {"/Script/CoreUObject.Object", "/Script/Engine.Actor"}))


def _descriptor_operation_receipt(result, values):
    """Copied finite native Object/Class operation proof after the shared failure gate."""
    data, category = result["data"], _normalize(values["element_type"])
    path, operation = data.get("descriptor_array_operation_class_path"), _ALIASES[_normalize(values["operation"])]
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True, descriptor_operation_first_mismatch={
                "stage": "copied_native_descriptor_operation_success", "property": field,
                "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("array_descriptor_operation_receipt_invalid", "Actual native allocation/default/change prefix retained; copied descriptor operation receipt inconsistent; no SDK follow-up or retry")
    require(_descriptor_operation_required(values) and data.get("native_descriptor_array_operation_parity") is True
        and data.get("descriptor_array_operation") == data.get("canonical_operation") == operation
        and data.get("descriptor_array_operation_category") == category and path in _DESCRIPTOR_LENGTH_TYPES
        and path.casefold() == (values["type_object"] or "/Script/CoreUObject.Object").casefold(),
        "descriptor_operation_profile", (operation, category, values["type_object"]), {key: data.get(key) for key in (
            "native_descriptor_array_operation_parity", "descriptor_array_operation", "canonical_operation", "descriptor_array_operation_category", "descriptor_array_operation_class_path")})
    for key in ("typed_node_shape_verified", "node_only_verified", "created", "existing_nodes_preserved", "declarations_preserved",
            "native_source_observed_unchanged", "requested_defaults_verified", "descriptor_array_operation_parity_verified",
            "descriptor_array_operation_source_preserved", "descriptor_array_default_sources_preserved"):
        require(data.get(key) is True, key, True, data.get(key))
    require(data.get("descriptor_array_operation_runtime_invocation_verified") is False,
        "runtime_invocation", False, data.get("descriptor_array_operation_runtime_invocation_verified"))
    require(type(data.get("native_signature_before")) is dict and data["native_signature_before"] == data.get("native_signature_after"),
        "native_signature", "same whole actual native signature", data.get("native_signature_after"))
    source = data.get("native_descriptor_array_operation_source_before")
    require(type(source) is dict and type(data.get("native_descriptor_array_operation_source_after")) is dict
        and json.dumps(source, sort_keys=True, ensure_ascii=False, separators=(",", ":")) == json.dumps(data["native_descriptor_array_operation_source_after"], sort_keys=True, ensure_ascii=False, separators=(",", ":"))
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("class_path") == path and source.get("element_category") == category
        and source.get("cdo_values_sampled") is False and source.get("cdo_creation_requested") is False
        and source.get("existing_cdo_resolved") is True and type(source.get("existing_cdo_is_null")) is bool
        and type(source.get("existing_cdo_path")) is str,
        "descriptor_source", "same actual held native class/hierarchy and optional shallow CDO observation", source)
    require((source["existing_cdo_is_null"] and source["existing_cdo_path"] == "")
        or (not source["existing_cdo_is_null"] and bool(source["existing_cdo_path"])),
        "optional_cdo", "null CDO has no sampled object path; no existence requirement", source)
    rows = source.get("hierarchy")
    require(type(rows) is list and bool(rows) and all(type(row) is dict for row in rows), "hierarchy", "actual nonempty native class hierarchy", rows)
    paths = []
    for row in rows:
        require(all(type(row.get(key)) is str for key in ("path", "class_flags_string", "package_path", "package_flags_string",
                "super_path", "within_path", "existing_cdo_path")) and bool(row["path"]) and bool(row["package_path"])
            and all(row[key].isascii() and row[key].isdigit() for key in ("class_flags_string", "package_flags_string"))
            and all(type(row.get(key)) is int and row[key] >= 0 for key in ("class_size", "class_alignment"))
            and type(row.get("package_dirty")) is bool and type(row.get("existing_cdo_is_null")) is bool
            and ((row["existing_cdo_is_null"] and row["existing_cdo_path"] == "")
                or (not row["existing_cdo_is_null"] and bool(row["existing_cdo_path"])))
            and row.get("metadata_observed") is False and row.get("metadata") == {},
            "hierarchy.row", "typed actual hierarchy/layout/optional CDO; metadata placeholder is not preservation proof", row)
        paths.append(row["path"])
    require(len(set(paths)) == len(paths) and path in paths, "hierarchy.identity", "unique captured paths including the requested class", paths)
    node = data.get("node")
    function = {"get": "Array_Get", "set": "Array_Set", "add": "Array_Add", "add_unique": "Array_AddUnique",
        "insert": "Array_Insert", "contains": "Array_Contains", "remove": "Array_RemoveItem", "clear": "Array_Clear"}[operation]
    require(type(node) is dict and node.get("object_path") == data.get("object_path")
        and node.get("node_class") == "/Script/BlueprintGraph.K2Node_CallArrayFunction"
        and node.get("function_member_parent_path") == "/Script/Engine.KismetArrayLibrary"
        and node.get("function_name") == data.get("function_name") == function,
        "node", "actual stock array call for the requested operation", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (KeyError, ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "node_guid", "same nonzero actual native node identity", node)
    roles = {("TargetArray", "input"): (category, True, path), ("self", "input"): ("object", False, "/Script/Engine.KismetArrayLibrary")}
    table = {"get": [("Index", "input", "int"), ("Item", "output", "element")],
        "set": [("Index", "input", "int"), ("Item", "input", "element"), ("bSizeToFit", "input", "bool")],
        "add": [("NewItem", "input", "element"), ("ReturnValue", "output", "int")],
        "add_unique": [("NewItem", "input", "element"), ("ReturnValue", "output", "int")],
        "insert": [("NewItem", "input", "element"), ("Index", "input", "int")],
        "contains": [("ItemToFind", "input", "element"), ("ReturnValue", "output", "bool")],
        "remove": [("Item", "input", "element"), ("ReturnValue", "output", "bool")], "clear": []}
    for name, direction, role_category in table[operation]:
        roles[name, direction] = (category, False, path) if role_category == "element" else (role_category, False, "")
    if operation not in {"get", "contains"}:
        roles.update({("execute", "input"): ("exec", False, ""), ("then", "output"): ("exec", False, "")})
    pins = node.get("pins")
    require(type(pins) is list and len(pins) == len(roles), "node.pins", "whole stock array operation pin inventory", pins)
    found, ids = {}, set()
    for pin in pins:
        require(type(pin) is dict, "pin", "actual whole pin object", pin)
        key = pin.get("name"), pin.get("direction")
        try:
            identity = uuid.UUID(pin.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and key in roles and key not in found, "pin.identity", "unique actual stock semantic port/GUID", pin)
        found[key] = pin; ids.add(identity.hex)
        category, array, binding = roles[key]; descriptor = pin.get("pin_type")
        require(type(descriptor) is dict and descriptor.get("category") == category and descriptor.get("subcategory") == "None"
            and descriptor.get("subcategory_object") == binding and descriptor.get("is_array") is array
            and all(type(descriptor.get(flag)) is bool for flag in ("is_reference", "is_const"))
            and type(pin.get("pin_type_text")) is str and bool(pin["pin_type_text"]),
            "pin.type", "actual native binding/container/ref/const plus full native type export", pin)
        require(all(type(pin.get(key)) is str for key in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
            and pin.get("links") == [] and pin.get("sub_pin_ids") == [] and pin.get("parent_pin_id") == "",
            "pin.defaults_links", "actual unlinked unsplit stock default cells", pin)
    before = data.get("typed_node_before_defaults")
    require(type(before) is dict and type(before.get("pins")) is list
        and {key: value for key, value in before.items() if key not in {"pins", "title"}} == {key: value for key, value in node.items() if key not in {"pins", "title"}},
        "typed_before.node", "same typed node identity and non-pin fields before defaults", before)
    initial = {pin.get("pin_id"): pin for pin in before["pins"] if type(pin) is dict}
    require(len(initial) == len(before["pins"]) == len(pins) and set(initial) == {pin["pin_id"] for pin in pins},
        "typed_before.pins", "same unique whole actual pin identities", before)
    defaults, applied, observations = json.loads(values["param_defaults_json"]), data.get("param_defaults_applied"), data.get("param_default_observations")
    require(type(applied) is dict and type(observations) is dict and set(applied) == set(observations) == set(defaults),
        "default_inventory", "exact requested pin spelling and actual raw observations", observations)
    requested_ids = set()
    for name, observation in observations.items():
        selected = [pin for (label, direction), pin in found.items() if direction == "input" and label.casefold() == name.casefold()]
        require(len(selected) == 1 and type(observation) is dict, "default.pin", "one current input pin for the requested default", observation)
        pin = selected[0]; requested_ids.add(pin["pin_id"])
        require(observation.get("pin_id") == pin["pin_id"]
            and all(type(observation.get(key)) is str for key in ("requested_literal", "schema_literal", "raw_default", "default_text", "default_as_string", "autogenerated_default_value", "default_object", "applied_kind", "applied_value"))
            and observation.get("raw_default") == pin["default_value"]
            and all(observation.get(key) == pin[key] for key in ("default_text", "autogenerated_default_value", "default_object"))
            and observation.get("schema_match") is True and observation.get("current_default_valid") is True,
            "default.actual", "same actual raw/FText/Object/Auto and successful schema match; requested text need not equal raw", observation)
        applied_kind = observation.get("applied_kind")
        actual_applied = pin["default_object"] if applied_kind == "object_path" else pin["default_value"]
        require(applied_kind in {"object_path", "raw_default"} and observation.get("applied_value") == applied[name] == actual_applied
            and (applied_kind == "raw_default" or (pin["pin_type"]["category"] in {"object", "class"} and bool(pin["default_object"]))),
            "default.applied_kind", "actual DefaultObject path for object_path, actual raw for raw_default", observation)
        require(type(observation.get("pin_type")) is dict and all(observation["pin_type"].get(key) == pin["pin_type"].get(key)
            for key in ("category", "subcategory", "subcategory_object", "is_array", "is_reference")),
            "default.type", "same current copied pin descriptor", observation.get("pin_type"))
    sources, after_sources = data.get("descriptor_array_default_sources_before"), data.get("descriptor_array_default_sources_after")
    expected_sources = {found_pin["name"]: observation["applied_value"] for name, observation in observations.items()
        if observation["applied_kind"] == "object_path"
        for (label, direction), found_pin in found.items() if direction == "input" and label.casefold() == name.casefold()}
    require(type(sources) is dict and type(after_sources) is dict and set(sources) == set(expected_sources)
        and json.dumps(sources, sort_keys=True, ensure_ascii=False, separators=(",", ":")) == json.dumps(after_sources, sort_keys=True, ensure_ascii=False, separators=(",", ":")),
        "default.sources", "canonical resolved item pins and unchanged whole loaded native class source observations", sources)
    for name, default_source in sources.items():
        require(type(default_source) is dict and default_source.get("class_path") == expected_sources[name]
            and default_source.get("class_path") in _DESCRIPTOR_LENGTH_TYPES
            and default_source.get("observations_complete") is True and default_source.get("native_identity_verified") is True
            and default_source.get("existing_cdo_resolved") is True
            and default_source.get("cdo_values_sampled") is False and default_source.get("cdo_creation_requested") is False,
            "default.source", "exact loaded native default class identity; optional CDO handle only, no value sampling", default_source)
    for pin in pins:
        omit = {"default_value", "default_text", "default_object"} if pin["pin_id"] in requested_ids else set()
        require({key: value for key, value in initial[pin["pin_id"]].items() if key not in omit}
            == {key: value for key, value in pin.items() if key not in omit},
            "typed_before.pin", "all actual non-requested defaults and whole roles/types/links/Auto preserved", pin)
    data.update(descriptor_operation_receipt_verified=True,
        descriptor_operation_validation_scope="Exact loaded native constraint/default class sources and stock node/default cells; original Any/SDK acceptance, no loader/runtime buffer/equality/mutation proof")


def _descriptor_length_required(values):
    category = _normalize(values["element_type"])
    path = values["type_object"] or "/Script/CoreUObject.Object"
    return (_ALIASES[_normalize(values["operation"])] == "length" and
        (category == "class" or category == "object" and path in {"/Script/CoreUObject.Object", "/Script/Engine.Actor"}))


def _descriptor_length_receipt(result, values):
    """Copied exact loaded native Object/Class Length; no class/CDO/array value query."""
    data, category = result["data"], _normalize(values["element_type"])
    path = values["type_object"] or "/Script/CoreUObject.Object"
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True, descriptor_length_first_mismatch={
                "stage": "copied_native_descriptor_length_success", "property": field,
                "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("array_descriptor_length_receipt_invalid", "Actual native node/change prefix retained; copied descriptor Length receipt inconsistent; no SDK follow-up or retry")
    require(_descriptor_length_required(values) and path in _DESCRIPTOR_LENGTH_TYPES
        and data.get("native_descriptor_array_length") is True
        and data.get("descriptor_array_length_category") == category
        and data.get("descriptor_array_length_class_path") == path and data.get("canonical_operation") == "length",
        "descriptor_length_profile", (category, path, "length"), {key: data.get(key) for key in (
            "native_descriptor_array_length", "descriptor_array_length_category", "descriptor_array_length_class_path", "canonical_operation")})
    for key in ("typed_node_shape_verified", "node_only_verified", "created", "existing_nodes_preserved",
            "declarations_preserved", "native_source_observed_unchanged", "requested_defaults_verified", "descriptor_array_length_source_preserved"):
        require(data.get(key) is True, key, True, data.get(key))
    require(data.get("descriptor_array_length_runtime_invocation_verified") is False,
        "runtime_invocation", False, data.get("descriptor_array_length_runtime_invocation_verified"))
    require(type(data.get("native_signature_before")) is dict and data["native_signature_before"] == data.get("native_signature_after"),
        "native_signature", "same whole current native signature", data.get("native_signature_after"))
    source = data.get("native_descriptor_array_length_source_before")
    require(type(source) is dict and source == data.get("native_descriptor_array_length_source_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("class_path") == path and source.get("element_category") == category
        and source.get("cdo_values_sampled") is False and source.get("cdo_creation_requested") is False
        and source.get("existing_cdo_resolved") is True and type(source.get("existing_cdo_is_null")) is bool
        and type(source.get("existing_cdo_path")) is str,
        "descriptor_source", "same actual held native class/hierarchy and optional shallow CDO observation", source)
    require((source["existing_cdo_is_null"] and source["existing_cdo_path"] == "")
        or (not source["existing_cdo_is_null"] and bool(source["existing_cdo_path"])),
        "optional_cdo", "null CDO has no sampled object path; no existence requirement", source)
    rows = source.get("hierarchy")
    require(type(rows) is list and bool(rows) and all(type(row) is dict for row in rows), "hierarchy", "actual nonempty native class hierarchy", rows)
    paths = []
    for row in rows:
        require(all(type(row.get(key)) is str for key in ("path", "class_flags_string", "package_path", "package_flags_string",
                "super_path", "within_path", "existing_cdo_path")) and bool(row["path"]) and bool(row["package_path"])
            and all(row[key].isascii() and row[key].isdigit() for key in ("class_flags_string", "package_flags_string"))
            and all(type(row.get(key)) is int and row[key] >= 0 for key in ("class_size", "class_alignment"))
            and type(row.get("package_dirty")) is bool and type(row.get("existing_cdo_is_null")) is bool
            and ((row["existing_cdo_is_null"] and row["existing_cdo_path"] == "")
                or (not row["existing_cdo_is_null"] and bool(row["existing_cdo_path"])))
            and row.get("metadata_observed") is False and row.get("metadata") == {},
            "hierarchy.row", "typed actual hierarchy/layout/optional CDO; metadata placeholder is not preservation proof", row)
        paths.append(row["path"])
    require(len(set(paths)) == len(paths) and path in paths, "hierarchy.identity", "unique captured paths including the requested class", paths)
    node = data.get("node")
    require(type(node) is dict and node.get("object_path") == data.get("object_path")
        and node.get("node_class") == "/Script/BlueprintGraph.K2Node_CallArrayFunction"
        and node.get("function_member_parent_path") == "/Script/Engine.KismetArrayLibrary"
        and node.get("function_name") == data.get("function_name") == "Array_Length", "node", "actual stock Array_Length call", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (KeyError, ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "node_guid", "same nonzero actual native node identity", node)
    roles = {("TargetArray", "input"): (category, True, path),
        ("self", "input"): ("object", False, "/Script/Engine.KismetArrayLibrary"), ("ReturnValue", "output"): ("int", False, "")}
    pins = node.get("pins")
    require(type(pins) is list and len(pins) == 3, "node.pins", "whole stock pure TargetArray/self/ReturnValue inventory", pins)
    found, ids = set(), set()
    for pin in pins:
        require(type(pin) is dict, "pin", "actual whole pin object", pin)
        key = pin.get("name"), pin.get("direction")
        try:
            identity = uuid.UUID(pin.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and key in roles and key not in found, "pin.identity", "unique actual stock semantic port/GUID", pin)
        found.add(key); ids.add(identity.hex)
        cat, array, constraint = roles[key]; descriptor = pin.get("pin_type")
        require(type(descriptor) is dict and descriptor.get("category") == cat and descriptor.get("subcategory") == "None"
            and descriptor.get("subcategory_object") == constraint and descriptor.get("is_array") is array
            and all(type(descriptor.get(flag)) is bool for flag in ("is_reference", "is_const"))
            and type(pin.get("pin_type_text")) is str and bool(pin["pin_type_text"]),
            "pin.type", "actual category/constraint/container/ref/const plus full native type export", pin)
        require(all(type(pin.get(key)) is str for key in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
            and pin.get("links") == [] and pin.get("sub_pin_ids") == [] and pin.get("parent_pin_id") == "",
            "pin.defaults_links", "actual unlinked unsplit stock defaults, distinct from runtime size", pin)
    require(json.loads(values["param_defaults_json"]) == {} and data.get("param_defaults_applied") == {},
        "defaults", "existing empty Length defaults; TargetArray is wire-only", data.get("param_defaults_applied"))
    data.update(descriptor_length_receipt_verified=True,
        descriptor_length_validation_scope="Exact loaded native type, optional shallow CDO handle, actual hierarchy and stock three-pin authoring; no class metadata/CDO values, loader or runtime length proof")


def _object_operation_parity_required(values):
    if _normalize(values["element_type"]) != "object" or _descriptor_operation_required(values):
        return False
    operation = _ALIASES[_normalize(values["operation"]) ]
    previous = {"length", "get", "clear", "contains", "add_unique"} if values["type_object"] == _SKELETAL_COMPONENT_TYPE else {"length", "get", "clear", "add"}
    return operation not in previous


def _object_operation_receipt(result, values):
    """Check copied new-operation authoring evidence after shared Native success; no SDK."""
    data, operation = result["data"], _ALIASES[_normalize(values["operation"]) ]
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            data.update(mutation_state_unknown=True, object_array_first_mismatch={
                "stage": "copied_native_object_operation_success", "property": field,
                "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("array_object_operation_receipt_invalid", "Actual native allocation/default/change prefix retained; copied object operation receipt inconsistent; no follow-up SDK or retry")
    item = {"set": "Item", "add": "NewItem", "add_unique": "NewItem", "insert": "NewItem", "contains": "ItemToFind", "remove": "Item"}[operation]
    require(data.get("native_object_array_operation_parity") is True
        and data.get("canonical_operation") == operation and data.get("object_array_operation") == operation
        and data.get("object_array_item_input_name") == item,
        "operation_item", (operation, item), {key: data.get(key) for key in ("native_object_array_operation_parity", "canonical_operation", "object_array_item_input_name")})
    for key in ("native_object_array", "typed_node_shape_verified", "node_only_verified", "created", "existing_nodes_preserved", "declarations_preserved", "native_source_observed_unchanged", "requested_defaults_verified", "object_array_source_read_only", "object_array_target_requires_connection", "object_array_item_requires_connection", "object_array_operation_parity_verified", "object_array_operation_source_observed_unchanged"):
        require(data.get(key) is True, key, True, data.get(key))
    require(data.get("object_array_runtime_invocation_verified") is False and data.get("object_array_cdo_created") is False
        and data.get("object_array_literal_defaults_supported") is False
        and data.get("object_array_native_class") == values["type_object"],
        "object_source", "same requested native class, existing CDO only, runtime unexecuted", {key: data.get(key) for key in ("object_array_runtime_invocation_verified", "object_array_cdo_created", "object_array_native_class")})
    require(type(data.get("native_signature_before")) is dict and data["native_signature_before"] == data.get("native_signature_after"),
        "native_signature", "same current source signature before/after", data.get("native_signature_after"))
    source = data.get("object_array_operation_source_before")
    require(type(source) is dict and source == data.get("object_array_operation_source_after")
        and source.get("class_path") == values["type_object"] and source.get("existing_cdo_path") == data.get("object_array_existing_cdo")
        and type(source.get("existing_cdo_path")) is str and bool(source["existing_cdo_path"])
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("cdo_values_sampled") is False and source.get("cdo_creation_requested") is False
        and type(source.get("held_native_roles")) is list and bool(source["held_native_roles"]),
        "object_source_current", "same complete held native source/class/existing CDO before/after; no sampled values or CDO creation", source)
    function = {"set": "Array_Set", "add": "Array_Add", "add_unique": "Array_AddUnique", "insert": "Array_Insert", "contains": "Array_Contains", "remove": "Array_RemoveItem"}[operation]
    node = data.get("node")
    require(type(node) is dict and node.get("object_path") == data.get("object_path")
        and node.get("node_class") == "/Script/BlueprintGraph.K2Node_CallArrayFunction"
        and node.get("function_member_parent_path") == "/Script/Engine.KismetArrayLibrary"
        and node.get("function_name") == data.get("function_name") == function, "node", "same actual native array call node/source", node)
    try:
        identity = uuid.UUID(node["node_guid"])
        valid = bool(identity.int) and identity == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_guid"]) == uuid.UUID(data["node_id"])
    except (KeyError, ValueError, TypeError, AttributeError):
        valid = False
    require(valid, "node_guid", "same nonzero current native node identity", node.get("node_guid"))
    roles = {("TargetArray", "input"): ("object", True, values["type_object"]), ("self", "input"): ("object", False, "/Script/Engine.KismetArrayLibrary")}
    for name, kind in _DEFAULTS[operation].items():
        actual_name = {"index": "Index", "item": "Item", "newitem": "NewItem", "itemtofind": "ItemToFind", "bsizetofit": "bSizeToFit"}[name]
        roles[actual_name, "input"] = ("object", False, values["type_object"]) if kind == "element" else (kind, False, "")
    if operation in {"add", "add_unique", "contains", "remove"}:
        roles["ReturnValue", "output"] = ("bool" if operation in {"contains", "remove"} else "int", False, "")
    if operation != "contains":
        roles.update({("execute", "input"): ("exec", False, ""), ("then", "output"): ("exec", False, "")})
    pins = node.get("pins")
    require(type(pins) is list and len(pins) == len(roles), "node.pins", "whole actual stock operation pin inventory", pins)
    observed, ids = {}, set()
    for pin in pins:
        require(type(pin) is dict, "pin", "actual pin object", pin)
        key = pin.get("name"), pin.get("direction")
        try:
            identity = uuid.UUID(pin.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and key in roles and key not in observed, "pin.identity", "unique actual GUID/semantic port", pin)
        observed[key] = pin; ids.add(identity.hex)
        category, array, path = roles[key]
        descriptor = pin.get("pin_type")
        require(type(descriptor) is dict and descriptor.get("category") == category and descriptor.get("subcategory") == "None"
            and descriptor.get("subcategory_object") == path and descriptor.get("is_array") is array
            and type(descriptor.get("is_reference")) is bool and type(descriptor.get("is_const")) is bool,
            "pin.type." + str(key), "requested actual category/container/class with observed ref/const", descriptor)
        require(all(type(pin.get(name)) is str for name in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
            and pin.get("links") == [] and pin.get("sub_pin_ids") == [] and pin.get("parent_pin_id") == "",
            "pin.defaults_links." + str(key), "actual unlinked/unsplit stock values", pin)
    defaults, applied = json.loads(values["param_defaults_json"]), data.get("param_defaults_applied")
    require(type(applied) is dict and set(applied) == set(defaults), "param_defaults_applied", "exact supplied scalar defaults only", applied)
    for name, raw in applied.items():
        selected = [pin for (label, direction), pin in observed.items() if direction == "input" and label.casefold() == name.casefold()]
        require(len(selected) == 1 and type(raw) is str and raw == selected[0]["default_value"],
            "scalar_default." + name, "same actual native scalar pin raw", raw)
    data.update(object_array_operation_receipt_verified=True,
        object_array_operation_scope="Native component-class nine-operation node authoring with explicit TargetArray/item wires; no runtime values, object loading, array execution or complete pipeline proof")


def _extended_primitive_receipt(result, values):
    """Inspect only copied known-success native observations, never another SDK call."""
    data = result["data"]
    def require(condition, field, expected, actual):
        if not condition:
            result["stage"] = "verification"
            result["data"].update(mutation_state_unknown=True,
                extended_primitive_first_mismatch={"stage": "copied_native_success_receipt", "property": field,
                    "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                    "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("array_extended_primitive_receipt_invalid", "Actual native array-node/default prefix retained; copied type/current/default receipt inconsistent; no SDK follow-up or retry")
    element = _ELEMENTS.get(_normalize(values["element_type"]))
    category, subcategory = {"int64": ("int64", "None"), "float": ("real", "float"),
        "double": ("real", "double"), "text": ("text", "None")}[element]
    operation = _ALIASES[_normalize(values["operation"])]
    require(data.get("native_extended_primitive_array") is True and data.get("element_type") == element
        and data.get("canonical_operation") == operation, "canonical_element_operation", (element, operation),
        {key: data.get(key) for key in ("native_extended_primitive_array", "element_type", "canonical_operation")})
    require(data.get("extended_primitive_runtime_invocation_verified") is False,
        "extended_primitive_runtime_invocation_verified", False, data.get("extended_primitive_runtime_invocation_verified"))
    for key in ("typed_node_shape_verified", "node_only_verified", "created", "existing_nodes_preserved", "declarations_preserved", "native_source_observed_unchanged", "requested_defaults_verified"):
        require(data.get(key) is True, key, True, data.get(key))
    require(type(data.get("native_signature_before")) is dict
        and data["native_signature_before"] == data.get("native_signature_after"),
        "native_signature", "same copied native source signature before/after", data.get("native_signature_after"))
    def pin_type(value, cat, sub, array, field):
        require(type(value) is dict and value.get("category") == cat and value.get("subcategory") == sub
            and value.get("is_array") is array and type(value.get("is_reference")) is bool
            and value.get("subcategory_object") == "", field,
            {"category": cat, "subcategory": sub, "is_array": array, "subcategory_object": ""}, value)
    pin_type(data.get("element_pin_type"), category, subcategory, False, "element_pin_type")
    node = data.get("node")
    require(type(node) is dict and node.get("object_path") == data.get("object_path")
        and node.get("node_class") == "/Script/BlueprintGraph.K2Node_CallArrayFunction"
        and node.get("function_member_parent_path") == "/Script/Engine.KismetArrayLibrary"
        and node.get("function_name") == data.get("function_name"), "node", "same exact native array call node/source", node)
    try:
        node_guid = uuid.UUID(node["node_guid"])
        valid_guid = bool(node_guid.int) and node_guid == uuid.UUID(data["node_guid"]) == uuid.UUID(node["node_id"]) == uuid.UUID(data["node_id"])
    except (KeyError, ValueError, TypeError, AttributeError):
        valid_guid = False
    require(valid_guid, "node_guid", "same nonzero native node identity", {key: node.get(key) for key in ("node_id", "node_guid")})
    roles = {("TargetArray", "input"): (category, subcategory, True), ("self", "input"): None}
    table = {"length": [("ReturnValue", "output", "int")], "get": [("Index", "input", "int"), ("Item", "output", "element")],
        "set": [("Index", "input", "int"), ("Item", "input", "element"), ("bSizeToFit", "input", "bool")],
        "add": [("NewItem", "input", "element"), ("ReturnValue", "output", "int")],
        "add_unique": [("NewItem", "input", "element"), ("ReturnValue", "output", "int")],
        "insert": [("NewItem", "input", "element"), ("Index", "input", "int")],
        "contains": [("ItemToFind", "input", "element"), ("ReturnValue", "output", "bool")],
        "remove": [("Item", "input", "element"), ("ReturnValue", "output", "bool")], "clear": []}
    for name, direction, kind in table[operation]:
        roles[name, direction] = (category, subcategory, False) if kind == "element" else (kind, "None", False)
    if operation not in ("length", "get", "contains"):
        roles.update({("execute", "input"): ("exec", "None", False), ("then", "output"): ("exec", "None", False)})
    pins = node.get("pins")
    require(type(pins) is list and len(pins) == len(roles) and all(type(pin) is dict for pin in pins),
        "node.pins", "whole stock array-node pin inventory", pins)
    observed, ids = {}, set()
    for pin in pins:
        key = pin.get("name"), pin.get("direction")
        try:
            identity = uuid.UUID(pin.get("pin_id", "")); valid = bool(identity.int) and identity.hex not in ids
        except (ValueError, TypeError, AttributeError):
            valid = False
        require(valid and key in roles and key not in observed, "pin.identity", "unique current GUID/semantic stock port", pin)
        ids.add(identity.hex); observed[key] = pin
        require(all(type(pin.get(k)) is str for k in ("default_value", "autogenerated_default_value", "default_object", "default_text"))
            and type(pin.get("links")) is list and pin["links"] == []
            and type(pin.get("sub_pin_ids")) is list and pin["sub_pin_ids"] == [] and pin.get("parent_pin_id") == "",
            "pin.default_links", "actual unsplit/unlinked stock defaults", pin)
        if roles[key] is not None:
            pin_type(pin.get("pin_type"), *roles[key], "node.pin_type." + str(key))
        else:
            require(type(pin.get("pin_type")) is dict and pin["pin_type"].get("category") == "object"
                and pin["pin_type"].get("subcategory_object") == "/Script/Engine.KismetArrayLibrary",
                "self", "existing exact native KismetArrayLibrary self pin", pin)
    defaults = json.loads(values["param_defaults_json"])
    receipts, applied = data.get("param_default_observations"), data.get("param_defaults_applied")
    require(type(receipts) is dict and set(receipts) == set(defaults) and type(applied) is dict and set(applied) == set(defaults),
        "param_default_observations", "exact requested pin spelling/default inventory", receipts)
    for requested, observation in receipts.items():
        selected = [pin for (name, direction), pin in observed.items() if direction == "input" and name.casefold() == requested.casefold()]
        require(len(selected) == 1 and type(observation) is dict, "default.pin", "one actual input pin/default observation", observation)
        pin = selected[0]
        require(observation.get("pin_id") == pin["pin_id"]
            and all(type(observation.get(key)) is str for key in ("requested_literal", "schema_literal", "raw_default", "default_text", "default_as_string"))
            and observation.get("raw_default") == pin["default_value"] == applied[requested]
            and observation.get("default_text") == pin["default_text"]
            and observation.get("schema_match") is True and observation.get("current_default_valid") is True,
            "default.observation", "same pin/raw/FText and successful schema match/current validation", observation)
        require(type(observation.get("pin_type")) is dict and all(observation["pin_type"].get(key) == pin["pin_type"].get(key)
            for key in ("category", "subcategory", "subcategory_object", "is_array", "is_reference")),
            "default.pin_type", "same actual copied primitive pin descriptor", observation.get("pin_type"))
    result["data"].update(extended_primitive_receipt_verified=True,
        extended_primitive_default_scope="Raw/default_text/schema literal are distinct actual observations; int64 JSON numeric upstream-double precision differs from exact integer text; no runtime array evaluation")


def _run(result, values, checkpoint_callback=None):
    result["data"].update(full_legacy_array_parity=False, array_element_cohort=["int", "bool", "name", "string"],
        native_construction_verification="requires actual registered call and saved/runtime evidence",
        node_only_contract=not values["create_graph_if_missing"], implicit_data_wiring=False, array_runtime_execution_verified=False)
    if _normalize(values["element_type"]) == "struct" and values["type_object"] == "/Script/CoreUObject.Vector4":
        result["data"].update(array_element_cohort=["vector4"], vector4_operation_cohort=list(_DEFAULTS),
            vector4_target_and_item_require_connections=False, vector4_literal_defaults_supported=True,
            vector4_target_array_requires_connection=True, vector4_item_literal_scope="complete_finite_XYZW_string_only",
            vector4_runtime_values_verified=False)
    if _uds_operation_required(values):
        result["data"].update(array_element_cohort=["scalar_user_defined_struct"], scalar_uds_operation_cohort=list(_DEFAULTS),
            scalar_uds_type_object=values["type_object"], scalar_uds_runtime_values_verified=False,
            scalar_uds_literal_compile_validation_verified=False)
    if _uds_length_required(values):
        result["data"].update(array_element_cohort=["scalar_user_defined_struct"], scalar_uds_operation_cohort=["length"],
            scalar_uds_type_object=values["type_object"], scalar_uds_runtime_values_verified=False)
    if _value_struct_operation_required(values):
        result["data"].update(array_element_cohort=[_normalize(values["element_type"])], value_struct_operation_cohort=list(_DEFAULTS),
            value_struct_type_path=_VALUE_STRUCT_LENGTH_TYPES[_normalize(values["element_type"])],
            value_struct_item_literal_scope="Original bounded Any conversion with Native schema acceptance", value_struct_runtime_values_verified=False)
    if _value_struct_length_required(values):
        result["data"].update(array_element_cohort=[_normalize(values["element_type"])], value_struct_operation_cohort=["length"],
            value_struct_type_path=_VALUE_STRUCT_LENGTH_TYPES[_normalize(values["element_type"])], value_struct_runtime_values_verified=False)
    if _descriptor_operation_required(values):
        result["data"].update(array_element_cohort=[_normalize(values["element_type"])], descriptor_operation_cohort=list(_DEFAULTS),
            descriptor_type_object=values["type_object"] or "/Script/CoreUObject.Object", descriptor_runtime_values_verified=False)
    if _descriptor_length_required(values):
        result["data"].update(array_element_cohort=[_normalize(values["element_type"])], descriptor_operation_cohort=["length"],
            descriptor_type_object=values["type_object"] or "/Script/CoreUObject.Object", descriptor_runtime_values_verified=False)
    if _normalize(values["element_type"]) == "object" and not (_descriptor_length_required(values) or _descriptor_operation_required(values)):
        result["data"].update(array_element_cohort=["object"], object_operation_cohort=(sorted(_SKELETAL_COMPONENT_OPERATIONS) if values["type_object"] == _SKELETAL_COMPONENT_TYPE else sorted(_OBJECT_OPERATIONS)),
            object_type_object=values["type_object"], object_target_and_item_require_connections=True,
            object_runtime_values_verified=False, object_source_scope="Native current class/CDO identity and hierarchy; no full CDO/private/global purity or runtime array execution")
    if _normalize(values["element_type"]) in {"name", "string"}:
        result["data"].update(array_element_cohort=[_normalize(values["element_type"])],
            text_element_conversion="Original string/bool/number/null/JSON fallback; Native owns exact literals and NAME_SIZE safety bound")
    if _normalize(values["element_type"]) == "vector":
        result["data"].update(array_element_cohort=["vector"], vector_operation_cohort=list(_DEFAULTS),
            vector_item_literals_supported=True, vector_index_literals_supported=True)
    if _normalize(values["element_type"]) in {"enum", "byteenum"}:
        result["data"].update(array_element_cohort=["enum"], enum_asset_read_only=True, enum_source_validation="Native exact UserDefinedEnum entries/metadata/disk; other enum loaders remain pending")
    functions._run_action(result, "add_blueprint_array_function_node", values, checkpoint_callback, operation_api=api)
    # Shared native failure/partial/unknown gates above exit before copied success checks.
    if _descriptor_operation_required(values) or "native_descriptor_array_operation_parity" in result["data"]:
        result["success"] = False
        _descriptor_operation_receipt(result, values)
        result["success"] = True
    if _uds_operation_required(values) or "native_scalar_uds_array_operation_parity" in result["data"]:
        result["success"] = False
        _uds_operation_receipt(result, values)
        result["success"] = True
    if _uds_length_required(values) or "native_scalar_uds_array_length" in result["data"]:
        result["success"] = False
        _uds_length_receipt(result, values)
        result["success"] = True
    if _value_struct_operation_required(values) or "native_value_struct_array_operation_parity" in result["data"]:
        result["success"] = False
        _value_struct_operation_receipt(result, values)
        result["success"] = True
    if _value_struct_length_required(values) or "native_value_struct_array_length" in result["data"]:
        result["success"] = False
        _value_struct_length_receipt(result, values)
        result["success"] = True
    if _descriptor_length_required(values) or "native_descriptor_array_length" in result["data"]:
        result["success"] = False
        _descriptor_length_receipt(result, values)
        result["success"] = True
    if _object_operation_parity_required(values):
        result["success"] = False
        _object_operation_receipt(result, values)
        result["success"] = True
    if _ELEMENTS.get(_normalize(values["element_type"])) in {"int64", "float", "double", "text"}:
        result["success"] = False
        _extended_primitive_receipt(result, values)
        result["success"] = True
        result["data"].update(array_element_cohort=[_ELEMENTS[_normalize(values["element_type"])]],
            numeric_default_scope="Original number-to-double conversion precedes integer truncation/SanitizeFloat; exact int64 text avoids numeric transport precision loss")


def add_blueprint_array_function_node(blueprint_name, operation, element_type, param_defaults_json="{}", type_object="",
        enum_type="", node_position_json="[0,0]", graph_name=None, graph_id=None, graph_type=None,
        create_graph_if_missing=False, operation_id=None):
    try:
        values = _arguments(blueprint_name, operation, element_type, param_defaults_json, type_object, enum_type,
            node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("add_blueprint_array_function_node", values, operation_id, lambda result: _run(result, values))


def add_blueprint_array_function_node_action(result, blueprint_name, operation, element_type, param_defaults_json="{}",
        type_object="", enum_type="", node_position_json="[0,0]", graph_name=None, graph_id=None, graph_type=None,
        create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, _arguments(blueprint_name, operation, element_type, param_defaults_json, type_object, enum_type,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
