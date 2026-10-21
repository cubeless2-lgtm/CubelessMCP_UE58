"""Named graph resolution, parameter, Return and validated-call contracts with shared journals."""

import copy
import json
import re
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import functions as api
from . import state, variables
from .state import OperationError, fail, mutate, new_result


def _shared(validator, *args, **kwargs):
    # Reuse the already bounded text/JSON/path parser without changing S contracts.
    try:
        return validator(*args, **kwargs)
    except OperationError as error:
        raise OperationError(error.code.replace("variable_", "function_"), str(error)) from error


def _text(value, field, *, empty=False):
    return _shared(variables._text, value, field, empty=empty)


def _bool(value, field):
    return _shared(variables._boolean, value, field)


def _json(value, field):
    return _shared(variables._json_value, value, field)


def _selectors(blueprint_name, graph_name, graph_id, graph_type):
    guid = _text("" if graph_id is None else graph_id, "graph_id", empty=True)
    if guid:
        try:
            parsed = uuid.UUID(guid)
            if not parsed.int:
                raise ValueError("zero GUID")
            guid = parsed.hex
        except ValueError as error:
            raise OperationError("function_graph_id_invalid", "graph_id must be an exact nonzero graph GUID") from error
    kind = _text("" if graph_type is None else graph_type, "graph_type", empty=True).lower().replace(" ", "").replace("-", "_")
    aliases = {"eventgraph": "event", "event_graph": "event", "ubergraph": "event", "uber_graph": "event",
               "functiongraph": "function", "function_graph": "function", "macrograph": "macro", "macro_graph": "macro"}
    kind = aliases.get(kind, kind)
    if kind not in ("", "any", "event", "function", "macro", "delegate"):
        raise OperationError("function_graph_type_invalid", "graph_type must be event, function, macro, delegate or any")
    return {"blueprint_path": _shared(variables._path, blueprint_name, write=True),
        "graph_name": _text("" if graph_name is None else graph_name, "graph_name", empty=True),
        "graph_id": guid, "graph_type": kind}


def _resolve_arguments(blueprint_name, graph_name, graph_id, graph_type, create_if_missing):
    return {**_selectors(blueprint_name, graph_name, graph_id, graph_type),
        "create_if_missing": _bool(create_if_missing, "create_if_missing")}


def _referenced_array_parameter_requested(arguments):
    return (arguments.get("is_array") is True and type(arguments.get("parameter_type")) is str
        and arguments["parameter_type"].lower().replace("_", "").replace("-", "").replace(" ", "") in ("enum", "byteenum", "object", "class"))


def _parameter_arguments(blueprint_name, parameter_name, parameter_type, direction, default_value_json,
        type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing):
    name = _text(parameter_name, "parameter_name")
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,99}", name) is None or name.lower() == "none":
        raise OperationError("function_parameter_name_invalid", "Parameter names must be ASCII identifiers of 1..100 characters, excluding None")
    direction = _text(direction, "direction", empty=True).lower()
    if direction == "":
        direction = "input"
    if direction == "return":
        direction = "output"
    if direction not in ("input", "output"):
        raise OperationError("function_parameter_direction_invalid", "direction must be input or output (legacy return alias accepted)")
    native_struct_array = (is_array is True and type(parameter_type) is str
        and parameter_type.lower().replace("_", "").replace("-", "").replace(" ", "") in ("vector", "rotator", "transform"))
    native_class_array = (is_array is True and type(parameter_type) is str
        and parameter_type.lower().replace("_", "").replace("-", "").replace(" ", "") in ("object", "class"))
    default = (_shared(variables._json_value, default_value_json, "default_value_json", allow_finite_double=True)
        if native_struct_array or native_class_array or _scalar_uds_parameter_requested({"parameter_type": parameter_type})
        else _json(default_value_json, "default_value_json")) if default_value_json is not None else None
    return {**_selectors(blueprint_name, graph_name, graph_id, graph_type),
        "parameter_name": name, "parameter_type": _text(parameter_type, "parameter_type"), "direction": direction,
        "default_value_json": "" if default is None else variables._json_text(default),
        "type_object": _text("" if type_object is None else type_object, "type_object", empty=True),
        "is_array": _bool(is_array, "is_array"),
        "create_graph_if_missing": _bool(create_graph_if_missing, "create_graph_if_missing")}


def _call_arguments(blueprint_name, function_class, function_name, param_defaults_json, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing,
        allow_latent, allow_editor_only, allow_wildcard, allow_deprecated, allow_internal):
    flags = {name: _bool(value, name) for name, value in (
        ("allow_latent", allow_latent), ("allow_editor_only", allow_editor_only), ("allow_wildcard", allow_wildcard),
        ("allow_deprecated", allow_deprecated), ("allow_internal", allow_internal))}
    if flags["allow_editor_only"]:
        raise OperationError("function_editor_only_forbidden", "Editor-only functions cannot be embedded into persistent runtime Blueprint assets")
    dynamic_component = function_class == "/Script/Engine.Actor" and function_name == "AddComponentByClass"
    # ND: the original opt-in creates an untyped stock node; no Map profile is inferred.
    map_wildcard = (flags["allow_wildcard"] and function_class == "/Script/Engine.BlueprintMapLibrary"
        and function_name in ("Map_Clear", "Map_Find", "Map_Add", "Map_Contains", "Map_Remove", "Map_Keys", "Map_Values", "Map_Length"))
    set_wildcard = (flags["allow_wildcard"] and function_class == "/Script/Engine.BlueprintSetLibrary"
        and function_name in ("Set_Add", "Set_Remove", "Set_Contains", "Set_Length", "Set_ToArray", "Set_Union", "Set_Intersection", "Set_Difference"))
    if any(value for name, value in flags.items() if name not in ("allow_internal", "allow_latent") and not ((map_wildcard or set_wildcard) and name == "allow_wildcard")) or (flags["allow_internal"] and not dynamic_component):
        raise OperationError("function_option_unsupported", "Only exact Actor.AddComponentByClass has a specialized internal opt-in; other wildcard/deprecated/internal calls remain unsupported. Native owns the finite latent opt-in and actual function metadata")
    if dynamic_component and not flags["allow_internal"]:
        raise OperationError("function_option_unsupported", "Exact specialized Actor.AddComponentByClass requires allow_internal=True; no action dispatched")
    defaults = _json(param_defaults_json, "param_defaults_json")
    if defaults is None:
        defaults = {}  # Legacy Optional[dict]=None is an empty default map.
    if type(defaults) is not dict or len(defaults) > 64 or len({key.casefold() for key in defaults}) != len(defaults):
        raise OperationError("function_defaults_invalid", "param_defaults_json must be an object of at most 64 distinct case-insensitive pin names")
    if map_wildcard:
        _bool(create_graph_if_missing, "create_graph_if_missing")
        if defaults:
            raise OperationError("function_map_literal_defaults_unsupported", "Finite wildcard Map8 accepts only empty param_defaults; literal support remains incomplete")
    if set_wildcard:
        _bool(create_graph_if_missing, "create_graph_if_missing")
        if defaults:
            raise OperationError("function_set_literal_defaults_unsupported", "Finite wildcard Set5 accepts only empty param_defaults; literal support remains incomplete")
    position = _json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]  # Legacy node_position=None uses the origin.
    if type(position) is not list or len(position) != 2 or any(type(v) is not int or abs(v) > 1000000 for v in position):
        raise OperationError("function_position_invalid", "node_position_json must contain two integers from -1000000 to 1000000")
    return {**_selectors(blueprint_name, graph_name, graph_id, graph_type),
        "function_class": _text(function_class, "function_class"), "function_name": _text(function_name, "function_name"),
        "param_defaults_json": variables._json_text(defaults), "node_position_json": variables._json_text(position),
        "create_graph_if_missing": _bool(create_graph_if_missing, "create_graph_if_missing"), **flags}


def _return_arguments(blueprint_name, node_position, graph_name, graph_id, graph_type, create_graph_if_missing):
    # Preserve the legacy domain list/None shape; the reflected wrapper uses JSON
    # under the original node_position key rather than a new generic converter.
    if node_position is None:
        position = [320, 0]
    elif type(node_position) is str:
        position = _json(node_position, "node_position")
        if position is None:
            position = [320, 0]
    else:
        position = _json(variables._json_text(node_position), "node_position")
    if type(position) is not list or len(position) != 2 or any(type(v) is not int or v < -(2**31) or v >= 2**31 for v in position):
        raise OperationError("function_position_invalid", "node_position must be a two-integer list or its JSON encoding within the signed int32 range")
    return {**_selectors(blueprint_name, graph_name, graph_id, graph_type),
        "node_position_json": variables._json_text(position),
        "create_graph_if_missing": _bool(create_graph_if_missing, "create_graph_if_missing")}


def _dynamic_cast_arguments(blueprint_name, target_class, node_position, pure,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    create = _bool(create_graph_if_missing, "create_graph_if_missing")
    target = _text(target_class, "target_class")
    generated_actor = re.fullmatch(r"/Game/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\.[A-Za-z0-9_]+_C", target) is not None
    native_class = re.fullmatch(r"/Script/[A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_]*", target) is not None
    if not native_class and not generated_actor:
        raise OperationError("dynamic_cast_target_unsupported", "Use an exact /Script/Module.Class native path or the existing exact Game generated-class path. Native checks the actual current class, graph and runtime dependency role; aliases and wider source forms remain incomplete")
    if node_position is None:
        position = [0, 0]
    elif type(node_position) is str:
        position = _json(node_position, "node_position")
        if position is None:
            position = [0, 0]
    else:
        position = _json(variables._json_text(node_position), "node_position")
    if type(position) is not list or len(position) != 2 or any(type(v) is not int or v < -(2**31) or v >= 2**31 for v in position):
        raise OperationError("function_position_invalid", "node_position must be a two-integer list or its JSON encoding within the signed int32 range")
    return {**_selectors(blueprint_name, graph_name, graph_id, graph_type),
        "target_class": target, "node_position_json": variables._json_text(position),
        "pure": _bool(pure, "pure"), "create_graph_if_missing": create}


def _scalar_uds_parameter_requested(arguments):
    value = arguments.get("parameter_type")
    return (type(value) is str
        and value.lower().replace("_", "").replace("-", "").replace(" ", "") in ("struct", "scriptstruct"))


def _scalar_uds_parameter_source(data, arguments):
    """Copied finite UDS definition; never import defaults or sample field values."""
    try:
        source = data["parameter_type_source_before"]; after = data["parameter_type_source_after"]
        path = source["struct_path"]; requested = arguments["type_object"]
        good = (type(source) is dict and type(after) is dict
            and json.dumps(source, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
                == json.dumps(after, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
            and data.get("parameter_type_source_preserved") is True
            and source.get("observations_complete") is True and source.get("native_identity_verified") is True
            and type(path) is str and path.startswith("/Game/") and "." in path
            and type(requested) is str and requested.casefold() == path.casefold()
            and bool(uuid.UUID(source["struct_guid"]).int) and source.get("package_dirty") is False
            and type(source.get("status")) is int
            and all(source.get(key) is False for key in ("values_sampled", "default_buffer_sampled", "cdo_creation_requested", "runtime_invocation_verified"))
            and all(type(source.get(key)) is int and source[key] >= 0 for key in ("struct_size", "struct_alignment"))
            and all(type(source.get(key)) is str and bool(source[key]) for key in ("package_path", "editor_data_path"))
            and all(type(source.get(key)) is str and source[key].isascii() and source[key].isdigit() for key in ("struct_flags_string", "package_flags_string"))
            and all(type(source.get(key)) is dict and all(type(a) is str and type(b) is str for a, b in source[key].items())
                for key in ("root_metadata", "editor_data_metadata")))
        fields = source.get("fields")
        good = good and type(fields) is list and 1 <= len(fields) <= 32 and all(type(field) is dict for field in fields)
        scalar = {"StrProperty": ("string", "None"), "IntProperty": ("int", "None"), "Int64Property": ("int64", "None"),
            "FloatProperty": ("real", "float"), "DoubleProperty": ("real", "double"), "BoolProperty": ("bool", "None"), "NameProperty": ("name", "None")}
        names, guids = set(), set()
        if good:
            for field in fields:
                identity = uuid.UUID(field["field_guid"])
                good = (bool(identity.int) and identity.hex not in guids and type(field.get("internal_name")) is str
                    and bool(field["internal_name"]) and field["internal_name"] not in names and type(field.get("authored_name")) is str
                    and field.get("owner_path") == path and field.get("class") in scalar
                    and (field.get("category"), field.get("subcategory")) == scalar[field["class"]]
                    and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit()
                    and all(type(field.get(key)) is int and field[key] >= 0 for key in ("offset", "size"))
                    and type(field.get("array_dim")) is int and field["array_dim"] == 1
                    and type(field.get("metadata")) is dict and all(type(a) is str and type(b) is str for a, b in field["metadata"].items()))
                if not good:
                    break
                names.add(field["internal_name"]); guids.add(identity.hex)
        file = source.get("file", {})
        return (good and type(file) is dict and file.get("exists") is True
            and type(file.get("filename")) is str and bool(file["filename"])
            and type(file.get("bytes")) is int and file["bytes"] >= 0 and type(file.get("sha1")) is str and len(file["sha1"]) == 40
            and all(c in "0123456789abcdefABCDEF" for c in file["sha1"]))
    except (KeyError, TypeError, ValueError, AttributeError):
        return False


def _scalar_uds_parameter_receipt(result, identity, arguments, native):
    """Validate this finite success only after the original failure/unknown gates."""
    data = native["data"]
    def require(condition, key, expected, actual):
        if not condition:
            result["stage"] = "verification"
            result["data"].update(mutation_state_unknown=True,
                scalar_uds_parameter_receipt_first_mismatch={"property": key, "expected": copy.deepcopy(expected),
                    "actual": copy.deepcopy(actual), "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("function_scalar_uds_parameter_receipt_invalid", "Actual parameter/default/cleanup prefix retained; UDS source or signature receipt is inconsistent; no SDK follow-up/retry")
    array = arguments.get("is_array")
    require(identity == "add_blueprint_function_parameter" and _scalar_uds_parameter_requested(arguments)
        and type(array) is bool and data.get("native_scalar_uds_parameter") is True
        and data.get("scalar_uds_parameter_is_array") is array,
        "native_scalar_uds_parameter", "same requested scalar or one-dimensional Array UDS profile", data.get("native_scalar_uds_parameter"))
    require(all(data.get(key) is False for key in ("scalar_uds_parameter_runtime_initialization_verified",
        "scalar_uds_parameter_default_buffer_import_performed", "scalar_uds_literal_compile_validation_verified")),
        "unverified_value_effects", "runtime/default-buffer import and deferred compile literal validation remain unverified",
        {key: data.get(key) for key in ("scalar_uds_parameter_runtime_initialization_verified",
            "scalar_uds_parameter_default_buffer_import_performed", "scalar_uds_literal_compile_validation_verified")})
    require(_scalar_uds_parameter_source(data, arguments), "parameter_type_source",
        "same current saved-clean held UDS GUID/EditorData/scalar fields/metadata/file before and after", data.get("parameter_type_source_before"))
    path = data["parameter_type_source_before"]["struct_path"]
    if array:
        _primitive_array_parameter_receipt(result, identity, arguments, native)
    else:
        signature = data.get("scalar_uds_parameter_signature")
        require(type(signature) is dict, "scalar_uds_parameter_signature", "current function-owned StructProperty", signature)
        require(signature.get("observations_complete") is True and signature.get("current_verified") is True
            and signature.get("source_handle_resolved") is True and signature.get("property_class") == "StructProperty"
            and signature.get("struct_path") == path and type(signature.get("array_dim")) is int and signature["array_dim"] == 1
            and all(type(signature.get(key)) is str and bool(signature[key]) for key in ("function_path", "property_path", "property_owner_path"))
            and signature["property_owner_path"] == signature["function_path"]
            and type(signature.get("property_flags")) is str and signature["property_flags"].isascii() and signature["property_flags"].isdigit(),
            "scalar_uds_parameter_signature.binding", "current actual StructProperty-to-function/UDS binding", signature)
        terminal = {"category": "None", "subcategory": "None", "type_object": "", "is_const": False,
            "is_weak_pointer": False, "is_uobject_wrapper": False}
        for key, pin in (("pin_type", data.get("pin_type")), ("signature.pin_type", signature.get("pin_type"))):
            require(type(pin) is dict and pin.get("category") == "struct" and pin.get("subcategory") == "None"
                and pin.get("type_object") == pin.get("subcategory_object") == path and pin.get("is_array") is False
                and type(pin.get("container_type")) is int and pin["container_type"] == 0
                and all(type(pin.get(flag)) is bool for flag in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
                and pin.get("is_weak_pointer") is False and pin.get("is_uobject_wrapper") is False
                and type(pin.get("terminal")) is dict and pin["terminal"] == terminal
                and all(type(pin["terminal"].get(flag)) is bool for flag in ("is_const", "is_weak_pointer", "is_uobject_wrapper")),
                key, "same current scalar UDS semantic type; compiled ref/const independently observed", pin)
        require(arguments.get("direction") in ("input", "output") and data.get("direction") == arguments["direction"]
            and data.get("parameter_name") == arguments.get("parameter_name")
            and signature.get("is_output") is (arguments["direction"] == "output"), "parameter_name/direction",
            {"name": arguments.get("parameter_name"), "direction": arguments.get("direction")}, signature)
        require(type(data.get("default_requested")) is bool and data["default_requested"] is bool(arguments.get("default_value_json"))
            and type(data.get("requested_default_value")) is str and type(data.get("default_value")) is str,
            "default_text", "actual requested/default strings, not an imported UDS value", data.get("default_value"))
    result["data"].update(scalar_uds_parameter_receipt_verified=True,
        scalar_uds_parameter_validation_scope="Copied held UDS definition and current scalar/Array property binding; declaration/default text is not buffer import, compile literal validation or runtime initialization")


def _referenced_array_parameter_source(data, element):
    """Copied type source, not an Enum/Class resolver or sampled array value."""
    try:
        source = data["parameter_type_source_before"]; after = data["parameter_type_source_after"]
        kind = {"byte": "enum", "object": "object", "class": "class"}[element["category"]]
        path = element["subcategory_object"]; scalar = source["element_type"]; referenced = source["referenced_source"]
        good = (type(source) is dict and type(after) is dict
            and json.dumps(source, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
                == json.dumps(after, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
            and data.get("parameter_type_source_preserved") is True and source.get("observations_complete") is True
            and source.get("reference_kind") == kind and source.get("type_path") == path
            and type(path) is str and bool(path) and type(scalar) is dict
            and scalar.get("category") == element["category"] and scalar.get("subcategory") == "None"
            and scalar.get("type_object") == scalar.get("subcategory_object") == path
            and scalar.get("is_array") is False and scalar.get("is_reference") is element.get("is_reference")
            and type(scalar.get("container_type")) is int and scalar["container_type"] == 0
            and all(type(scalar.get(key)) is bool for key in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and type(scalar.get("terminal")) is dict and type(referenced) is dict)
        if good and kind == "enum":
            rows = referenced["entries"]
            good = type(rows) is list and bool(rows)
            if path == "/Script/InputCore.ETouchIndex":
                good = (good and referenced.get("enum_path") == path and referenced.get("observations_complete") is True
                    and referenced.get("native_identity_verified") is True and referenced.get("uasset_required") is False
                    and all(type(row) is dict and type(row.get("name")) is str and type(row.get("value_string")) is str
                        and type(row.get("hidden")) is bool for row in rows))
            else:
                good = (good and referenced.get("path") == path and type(referenced.get("class")) is str
                    and type(referenced.get("dirty")) is bool
                    and all(type(referenced.get(key)) is int for key in ("flags", "package_flags", "package_object_flags", "cpp_form", "underlying"))
                    and all(type(referenced.get(key)) is str for key in ("UniqueNameIndex", "EnumDescription", "DisplayNameMap"))
                    and type(referenced.get("metadata")) is dict
                    and all(type(key) is str and type(value) is str for key, value in referenced["metadata"].items())
                    and type(referenced.get("disk_exists")) is bool and type(referenced.get("disk_path")) is str and bool(referenced["disk_path"])
                    and (not referenced["disk_exists"] or (type(referenced.get("disk_bytes")) is int and referenced["disk_bytes"] >= 0
                        and type(referenced.get("disk_sha1")) is str and len(referenced["disk_sha1"]) == 40
                        and all(c in "0123456789abcdefABCDEF" for c in referenced["disk_sha1"])))
                    and all(type(row) is dict and all(type(row.get(key)) is str for key in ("name", "full_name", "value", "display"))
                        and all(type(row.get(key)) is bool for key in ("hidden", "spacer")) for row in rows))
        elif good:
            rows = referenced["hierarchy"]
            good = (path in ("/Script/CoreUObject.Object", "/Script/Engine.Actor",
                "/Script/Engine.ActorComponent", "/Script/Engine.ChildActorComponent", "/Script/Engine.SkeletalMeshComponent")
                and referenced.get("constraint") == path and referenced.get("observations_complete") is True
                and referenced.get("native_identity_verified") is True and type(rows) is list and bool(rows))
            if good:
                paths = []
                for row in rows:
                    cdo = row["existing_cdo"]
                    good = (type(row) is dict and all(type(row.get(key)) is str for key in ("path", "class", "outer", "flags", "class_flags", "package", "package_flags", "package_object_flags"))
                        and bool(row["path"]) and type(row.get("dirty")) is bool and type(row.get("metadata")) is dict
                        and all(type(key) is str and type(value) is str for key, value in row["metadata"].items())
                        and type(cdo) is dict and type(cdo.get("exists")) is bool and cdo.get("object_values_sampled") is False
                        and (not cdo["exists"] or (all(type(cdo.get(key)) is str for key in ("path", "class", "outer", "flags")) and bool(cdo["path"]))))
                    if not good:
                        break
                    paths.append(row["path"])
                good = good and len(paths) == len(set(paths)) and path in paths
    except (KeyError, TypeError, ValueError, AttributeError):
        good = False
    return good


def _primitive_array_parameter_receipt(result, identity, arguments, native):
    """Copied finite primitive/native-struct Array success; no SDK or element-buffer import."""
    data = native["data"]
    def require(condition, key, expected, actual):
        if not condition:
            result["stage"] = "verification"
            result["data"].update(mutation_state_unknown=True,
                primitive_array_receipt_first_mismatch={"stage": "copied_native_success_receipt", "property": key,
                    "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                    "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("function_primitive_array_receipt_invalid", "Actual parameter mutation prefix retained; primitive Array metadata/current receipt is inconsistent; no SDK follow-up/retry")
    require(identity == "add_blueprint_function_parameter" and arguments.get("is_array") is True
        and data.get("native_primitive_array_parameter") is True, "native_primitive_array_parameter", True,
        {"operation": identity, "is_array": arguments.get("is_array"), "marker": data.get("native_primitive_array_parameter")})
    for key in ("primitive_array_native_source_observed_unchanged", "primitive_array_descriptor_current_verified", "primitive_array_physical_pin_current_verified"):
        require(data.get(key) is True, key, True, data.get(key))
    for key in ("primitive_array_literal_buffer_import_performed", "primitive_array_runtime_initialization_verified"):
        require(data.get(key) is False, key, False, data.get(key))
    element = data.get("primitive_array_element_type")
    require(type(element) is dict, "primitive_array_element_type", "copied noncontainer scalar descriptor", element)
    kinds = {("bool", "None"): "BoolProperty", ("int", "None"): "IntProperty", ("int64", "None"): "Int64Property",
        ("real", "float"): "FloatProperty", ("real", "double"): "DoubleProperty", ("string", "None"): "StrProperty",
        ("name", "None"): "NameProperty", ("text", "None"): "TextProperty"}
    type_path = element.get("subcategory_object")
    native_struct_array = (element.get("category") == "struct" and element.get("subcategory") == "None"
        and type_path in ("/Script/CoreUObject.Vector", "/Script/CoreUObject.Rotator", "/Script/CoreUObject.Transform"))
    scalar_uds_array = (_scalar_uds_parameter_requested(arguments) and element.get("category") == "struct"
        and element.get("subcategory") == "None" and data.get("native_scalar_uds_parameter") is True)
    native_referenced_array = element.get("category") in ("byte", "object", "class") and element.get("subcategory") == "None"
    reference_kind = {"byte": "enum", "object": "object", "class": "class"}.get(element.get("category"))
    if _referenced_array_parameter_requested(arguments) or "native_referenced_array_parameter" in data:
        requested_kind = arguments["parameter_type"].lower().replace("_", "").replace("-", "").replace(" ", "")
        require(native_referenced_array and data.get("native_referenced_array_parameter") is True
            and _referenced_array_parameter_requested(arguments)
            and reference_kind == ("enum" if requested_kind == "byteenum" else requested_kind),
            "native_referenced_array_parameter", "same requested Enum/Object/Class Array profile", data.get("native_referenced_array_parameter"))
        require(_referenced_array_parameter_source(data, element), "parameter_type_source",
            "same actual held referenced element source before/after with metadata and optional nullable CDO identity", data.get("parameter_type_source_before"))
    kind = ({"byte": "ByteProperty", "object": "ObjectProperty", "class": "ClassProperty"}[element["category"]] if native_referenced_array
        else "StructProperty" if native_struct_array or scalar_uds_array else kinds.get((element.get("category"), element.get("subcategory"))))
    if native_struct_array:
        require(data.get("native_value_struct_array_parameter") is True, "native_value_struct_array_parameter", True,
            data.get("native_value_struct_array_parameter"))
        source = data.get("parameter_type_source_before")
        require(type(source) is dict and source == data.get("parameter_type_source_after")
            and data.get("parameter_type_source_preserved") is True
            and source.get("observations_complete") is True and source.get("native_identity_verified") is True
            and source.get("struct_path") == type_path and type(source.get("struct_size")) is int and source["struct_size"] > 0
            and source.get("package_path") == "/Script/CoreUObject" and source.get("package_dirty") is False
            and type(source.get("package_flags_string")) is str and source["package_flags_string"].isascii()
            and source["package_flags_string"].isdigit(), "parameter_type_source", "same held exact native struct source before/after", source)
    require(kind is not None and element.get("is_array") is False and element.get("is_reference") is False
        and (native_struct_array or native_referenced_array or scalar_uds_array or element.get("subcategory_object") == ""), "primitive_array_element_type", "one admitted plain primitive, native struct or current referenced element", element)
    terminal = {"category": "None", "subcategory": "None", "type_object": "", "is_const": False,
        "is_weak_pointer": False, "is_uobject_wrapper": False}
    def full_array(value, key):
        require(type(value) is dict, key, "whole copied Array PinType", value)
        require((value.get("category"), value.get("subcategory")) == (element.get("category"), element.get("subcategory"))
            and value.get("is_array") is True and type(value.get("container_type")) is int and value.get("container_type") == 1
            and value.get("subcategory_object") == value.get("type_object") == (type_path if native_struct_array or native_referenced_array or scalar_uds_array else "")
            and all(type(value.get(flag)) is bool for flag in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and value.get("is_weak_pointer") is False and (native_referenced_array or value.get("is_uobject_wrapper") is False)
            and type(value.get("terminal")) is dict and value["terminal"] == terminal
            and all(type(value["terminal"].get(flag)) is bool for flag in ("is_const", "is_weak_pointer", "is_uobject_wrapper")),
            key, "same finite element in one dimensional non-object Array; complete terminal and typed flags", value)
    pin_type = data.get("pin_type"); full_array(pin_type, "pin_type")
    preflight = data.get("primitive_array_native_preflight")
    require(type(preflight) is dict, "primitive_array_native_preflight", "copied native source preflight", preflight)
    require(preflight.get("stage") == "native_source_preflight" and preflight.get("observations_complete") is True
        and preflight.get("later_conditions_unobserved_after_failure") is False and preflight.get("first_failed_condition") == ""
        and type(preflight.get("evaluated")) is list
        and all(type(row) is dict and row.get("passed") is True for row in preflight["evaluated"]),
        "primitive_array_native_preflight", "complete current preflight without a failed or unobserved condition", preflight)
    signature = data.get("primitive_array_skeleton_signature")
    require(type(signature) is dict, "primitive_array_skeleton_signature", "current reacquired Array/Inner metadata", signature)
    require(signature.get("observations_complete") is True and signature.get("current_verified") is True,
        "primitive_array_skeleton_signature.current", True, {key: signature.get(key) for key in ("observations_complete", "current_verified")})
    for key in ("function_path", "property_path", "property_owner_path", "inner_path", "inner_owner_path"):
        require(type(signature.get(key)) is str and bool(signature[key]), key, "actual nonempty current metadata path", signature.get(key))
    require(signature["property_owner_path"] == signature["function_path"] and signature["inner_owner_path"] == signature["property_path"],
        "primitive_array_skeleton_signature.owner_chain", "Array property owned by current function; Inner owned by Array property", signature)
    require(signature.get("inner_class") == kind and type(signature.get("inner_array_dim")) is int and signature.get("inner_array_dim") == 1
        and type(signature.get("inner_size")) is int and signature.get("inner_size") > 0,
        "primitive_array_skeleton_signature.inner", {"inner_class": kind, "inner_array_dim": 1, "inner_size": ">0"}, signature)
    if native_struct_array:
        require(signature.get("inner_struct_path") == type_path, "primitive_array_skeleton_signature.inner_struct_path",
            type_path, signature.get("inner_struct_path"))
        result["data"]["native_value_struct_array_parameter_receipt_verified"] = True
    if scalar_uds_array:
        require(signature.get("inner_struct_path") == type_path and signature.get("inner_source_handle_resolved") is True
            and signature.get("inner_current_verified") is True
            and type_path == data["parameter_type_source_before"].get("struct_path"),
            "primitive_array_skeleton_signature.uds_binding", "same actual current held UDS Inner binding", signature)
    if native_referenced_array:
        require(signature.get("inner_source_handle_resolved") is True
            and signature.get("inner_enum_path") == (type_path if reference_kind == "enum" else "")
            and signature.get("inner_property_class_path") == (type_path if reference_kind == "object" else "/Script/CoreUObject.Class" if reference_kind == "class" else "")
            and signature.get("inner_meta_class_path") == (type_path if reference_kind == "class" else ""),
            "primitive_array_skeleton_signature.referenced_binding", "current actual Inner Enum/PropertyClass/MetaClass handle paths", signature)
        result["data"]["native_referenced_array_parameter_receipt_verified"] = True
    for key in ("property_flags", "inner_flags"):
        require(type(signature.get(key)) is str and signature[key].isascii() and signature[key].isdigit(), key, "actual numeric flags text", signature.get(key))
    full_array(signature.get("pin_type"), "primitive_array_skeleton_signature.pin_type")
    for key in ("category", "subcategory", "is_array", "subcategory_object", "type_object", "container_type", "terminal"):
        require(signature["pin_type"].get(key) == pin_type.get(key), "skeleton_signature.pin_type." + key, pin_type.get(key), signature["pin_type"].get(key))
    direction = arguments.get("direction")
    require(direction in ("input", "output") and data.get("direction") == direction
        and data.get("parameter_name") == arguments.get("parameter_name")
        and signature.get("is_output") is (direction == "output"), "parameter_name/direction", {"name": arguments.get("parameter_name"), "direction": direction},
        {"name": data.get("parameter_name"), "direction": data.get("direction"), "is_output": signature.get("is_output")})
    require(type(data.get("default_requested")) is bool and data["default_requested"] is bool(arguments.get("default_value_json")),
        "default_requested", bool(arguments.get("default_value_json")), data.get("default_requested"))
    for key in ("requested_default_value", "primitive_array_descriptor_default_value", "primitive_array_physical_default_value", "primitive_array_autogenerated_default_value", "primitive_array_default_text_value"):
        require(type(data.get(key)) is str, key, "actual copied default text; not proof of buffer initialization", data.get(key))
    result["data"].update(primitive_array_receipt_verified=True,
        primitive_array_validation_scope="Current native source/Array-Inner owner metadata and typed declaration; skeleton reference flags retained independently; raw descriptor/physical defaults are not runtime array values")


def _run_action(result, identity, arguments, checkpoint_callback=None, operation_api=None):
    result["data"].update(operation=identity, resolved_blueprint_path=arguments["blueprint_path"],
        adapter_preloaded_asset=False, loading_side_effects_verified=False, full_legacy_function_parity=False,
        pending_mutation={"operation": identity, **arguments, "dispatch_state_at_checkpoint": "not_yet_dispatched",
                          "interrupted_outcome": "unknown_inspect_asset_and_journal"})
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_function_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "asset_path": arguments["blueprint_path"],
        "attempted": None, "changed": None, "change_state_known": False, "outcome": "unknown_until_native_result"}]
    try:
        native = getattr(operation_api or api, identity)(**arguments)
    except Exception as error:
        if getattr(error, "code", "") in ("native_function_api_unavailable", "native_event_api_unavailable", "native_pin_api_unavailable"):
            result["data"].update(native_dispatched=False, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(operation=identity, native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not native["change_state_known"],
        mutation_performed=native.get("changed") if native["change_state_known"] else None)
    result["stage"] = native.get("stage", "native_function_operation")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"], status="not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    result["changes"] = [] if native["change_state_known"] and native.get("changed") is False and not native["mutation_attempted"] else [{
        "kind": identity, "asset_path": arguments["blueprint_path"], "attempted": native["mutation_attempted"],
        "changed": native.get("changed") if native["change_state_known"] else None, "change_state_known": native["change_state_known"],
        "dirty_before": native.get("dirty_before"), "dirty_after": native.get("dirty_after"),
        "outcome": "scoped_native_observation" if native["change_state_known"] else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_function_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], independent_value_check="not_run", disk_reload="not_run")
    if any(native[key] for key in ("compile_requested", "compile_attempted", "save_requested", "save_attempted", "saved")) or native["compiled"] is True:
        result["data"]["mutation_state_unknown"] = True
        raise OperationError("unexpected_function_compile_save", "Native outcome reports unrequested explicit compilation/saving; retained observations require inspection")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_function_operation_failed", native.get("error") or "Function operation failed; actual outcomes retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("function_result_unverified", "Native success lacks known postconditions")
    native_struct_array = (identity == "add_blueprint_function_parameter" and arguments.get("is_array") is True
        and type(arguments.get("parameter_type")) is str
        and arguments["parameter_type"].lower().replace("_", "").replace("-", "").replace(" ", "") in ("vector", "rotator", "transform"))
    native_referenced_array = identity == "add_blueprint_function_parameter" and _referenced_array_parameter_requested(arguments)
    scalar_uds = identity == "add_blueprint_function_parameter" and _scalar_uds_parameter_requested(arguments)
    if scalar_uds or "native_scalar_uds_parameter" in native["data"]:
        _scalar_uds_parameter_receipt(result, identity, arguments, native)
    elif ("native_primitive_array_parameter" in native["data"] or "native_value_struct_array_parameter" in native["data"]
            or "native_referenced_array_parameter" in native["data"] or native_struct_array or native_referenced_array):
        _primitive_array_parameter_receipt(result, identity, arguments, native)
    result["success"] = True
    result["stage"] = "complete"
    result["retry"]["duplicate_risk"] = "function_action_would_repeat" if native.get("changed") else "none_observed"


def _prepared(identity, arguments, operation_id):
    try:
        values = arguments()
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate(identity, values, operation_id, lambda result: _run_action(result, identity, values))


def resolve_blueprint_graph(blueprint_name, graph_name=None, graph_id=None, graph_type=None, create_if_missing=False, operation_id=None):
    return _prepared("resolve_blueprint_graph", lambda: _resolve_arguments(blueprint_name, graph_name, graph_id, graph_type, create_if_missing), operation_id)


def resolve_blueprint_graph_action(result, blueprint_name, graph_name=None, graph_id=None, graph_type=None, create_if_missing=False, checkpoint_callback=None):
    _run_action(result, "resolve_blueprint_graph", _resolve_arguments(blueprint_name, graph_name, graph_id, graph_type, create_if_missing), checkpoint_callback)


def add_blueprint_function_parameter(blueprint_name, parameter_name, parameter_type, direction="input", default_value_json=None,
        type_object=None, is_array=False, graph_name=None, graph_id=None, graph_type="function", create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_function_parameter", lambda: _parameter_arguments(blueprint_name, parameter_name, parameter_type,
        direction, default_value_json, type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_function_parameter_action(result, blueprint_name, parameter_name, parameter_type, direction="input", default_value_json=None,
        type_object=None, is_array=False, graph_name=None, graph_id=None, graph_type="function", create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, "add_blueprint_function_parameter", _parameter_arguments(blueprint_name, parameter_name, parameter_type,
        direction, default_value_json, type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def add_blueprint_return_node(blueprint_name, node_position=None, graph_name=None, graph_id=None,
        graph_type="function", create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_return_node", lambda: _return_arguments(blueprint_name, node_position,
        graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_return_node_action(result, blueprint_name, node_position=None, graph_name=None, graph_id=None,
        graph_type="function", create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, "add_blueprint_return_node", _return_arguments(blueprint_name, node_position,
        graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def add_blueprint_dynamic_cast_node(blueprint_name, target_class, node_position=None, pure=False,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_dynamic_cast_node", lambda: _dynamic_cast_arguments(blueprint_name,
        target_class, node_position, pure, graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_dynamic_cast_node_action(result, blueprint_name, target_class, node_position=None, pure=False,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, "add_blueprint_dynamic_cast_node", _dynamic_cast_arguments(blueprint_name,
        target_class, node_position, pure, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def add_blueprint_call_function_node(blueprint_name, function_class, function_name, param_defaults_json="{}", node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, allow_latent=False, allow_editor_only=False,
        allow_wildcard=False, allow_deprecated=False, allow_internal=False, operation_id=None):
    return _prepared("add_blueprint_call_function_node", lambda: _call_arguments(blueprint_name, function_class, function_name,
        param_defaults_json, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
        allow_latent, allow_editor_only, allow_wildcard, allow_deprecated, allow_internal), operation_id)


def add_blueprint_call_function_node_action(result, blueprint_name, function_class, function_name, param_defaults_json="{}", node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, allow_latent=False, allow_editor_only=False,
        allow_wildcard=False, allow_deprecated=False, allow_internal=False, checkpoint_callback=None):
    _run_action(result, "add_blueprint_call_function_node", _call_arguments(blueprint_name, function_class, function_name,
        param_defaults_json, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
        allow_latent, allow_editor_only, allow_wildcard, allow_deprecated, allow_internal), checkpoint_callback)
