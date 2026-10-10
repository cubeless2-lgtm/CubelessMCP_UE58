"""Named Material operations with durable intent and explicit native observations.

The native layer resolves graph/node selectors and uses public editor APIs. This
layer owns request journaling and result semantics. No explicit compile/save is
requested; Unreal PostEditChange may independently schedule shader work.
"""

import copy
import json
import math

from cubeless_mcp_ue58.compatibility.ue58 import material as api
from . import state
from .state import OperationError, fail, mutate, new_result


def _apply_native(result, native):
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"]["native_outcome"] = {key: copy.deepcopy(value) for key, value in native.items() if key not in {"data", "before", "after"}}
    for key in ("before", "after"):
        if key in native:
            result["data"][key] = copy.deepcopy(native[key])
    result["data"]["limits"] = {"max_graph_nodes": api.MAX_GRAPH_NODES, "on_exceeded": "reject_without_mutation"}
    result["stage"] = native.get("stage", "native_result")
    result["compile"].update(status=native.get("compile_status", "not_requested"),
                             implicit_compile_possible=native.get("implicit_compile_possible", False),
                             completion_verified=False)
    result["save"].update(status=native.get("save_status", "not_requested"), saved=False)
    result["validation"]["native_postcondition_passed"] = native.get("postcondition_passed") is True
    result["validation"]["native_verification"] = copy.deepcopy(native.get("verification", {}))
    if native.get("mutation_attempted") is False:
        result["changes"] = []
    elif native.get("mutation_attempted") is True:
        result["changes"] = [{"kind": result["data"].get("operation", "material_mutation"), "attempted": True,
                              "changed": native.get("changed"), "dirty_before": native.get("dirty_before"),
                              "dirty_after": native.get("dirty_after"), "outcome": "observed_native_result"}]
    if native["success"] is not True:
        raise OperationError(native.get("error_code") or "native_material_operation_failed", native.get("error") or "Native Material operation failed; inspect recorded observations")


def _read(name, call):
    result = new_result()
    result["data"]["operation"] = name
    try:
        result["stage"] = "native_read"
        native = call()
        _apply_native(result, native)
        if native.get("mutation_attempted") is True:
            raise OperationError("unexpected_read_mutation", "Native read helper unexpectedly reported a mutation")
        result["validation"]["status"] = "native_loaded_asset_inspection"
        result["success"] = True
        result["stage"] = "complete"
    except Exception as error:
        fail(result, error)
    return result


def resolve_material_graph(material_path, graph_type="auto"):
    return _read("resolve_material_graph", lambda: api.resolve_material_graph(material_path, graph_type))


def list_material_nodes(material_path, graph_type="auto", node_type="", desc_contains="", include_pins=True):
    return _read("list_material_nodes", lambda: api.list_material_nodes(material_path, graph_type, node_type, desc_contains, include_pins))


def analyze_material_graph(material_path, graph_type="auto", include_nodes=False, include_usage=True, max_referencers=25):
    return _read("analyze_material_graph", lambda: api.analyze_material_graph(material_path, graph_type, include_nodes, include_usage, max_referencers))


def list_material_collection_parameter_nodes(material_path, graph_type="auto"):
    return _read("list_material_collection_parameter_nodes", lambda: api.list_material_collection_parameter_nodes(material_path, graph_type))


def get_material_parameter_collection_values(collection_path, parameter_names=None, include_asset_defaults=True, include_runtime=True):
    return _read("get_material_parameter_collection_values", lambda: api.get_material_parameter_collection_values(collection_path, parameter_names, include_asset_defaults, include_runtime))


def _mutation(name, arguments, operation_id, graph_type, call):
    def action(result):
        result["data"]["operation"] = name
        result["stage"] = "before_snapshot"
        before = api.list_material_nodes(arguments["material_path"], graph_type, "", "", True)
        if before["success"] is not True:
            _apply_native(result, before)  # Preserve native candidate paths and preflight diagnostics.
        snapshot = copy.deepcopy(before["data"])
        canonical_path = snapshot.get("material") or snapshot.get("asset_path")
        if not isinstance(canonical_path, str) or not canonical_path:
            raise OperationError("missing_material_identity", "Before snapshot lacks its resolved material path; no mutation dispatched")
        if not canonical_path.startswith("/Game/"):
            raise OperationError("material_mutation_path_restricted", "This migration slice mutates only /Game Material assets; no plugin or engine content was edited")
        result["data"]["before"] = snapshot
        result["data"]["before_snapshot"] = copy.deepcopy(snapshot)
        result["data"]["resolved_material_path"] = canonical_path
        result["data"]["pending_mutation"] = {"operation": name, "dispatch_state_at_checkpoint": "not_yet_dispatched",
                                                "interrupted_outcome": "unknown_inspect_asset_and_journal"}
        result["stage"] = "before_mutation_checkpoint"
        state.checkpoint(result)
        result["stage"] = "native_mutation"
        result["changes"].append({"kind": name, "asset_path": canonical_path, "attempted": True, "outcome": "unknown_until_native_result"})
        native = call(canonical_path)
        _apply_native(result, native)
        if native.get("postcondition_passed") is not True:
            raise OperationError("material_postcondition_not_verified", "Native dispatch succeeded but no verified Material postcondition was returned")
        result["validation"]["status"] = "native_material_postcondition_passed"
        result["retry"]["duplicate_risk"] = "material_mutation_would_repeat" if native.get("changed") else "none_observed"
        result["success"] = True
        result["stage"] = "complete"
    return mutate(name, arguments, operation_id, action)


def connect_material_nodes(material_path, from_node_id, to_node_id, to_input, from_output="", graph_type="auto", operation_id=None):
    arguments = {"material_path": material_path, "from_node_id": from_node_id, "to_node_id": to_node_id, "to_input": to_input,
                 "from_output": from_output, "graph_type": graph_type}
    return _mutation("connect_material_nodes", arguments, operation_id, graph_type,
                     lambda resolved: api.connect_material_nodes(resolved, from_node_id, to_node_id, to_input, from_output, graph_type))


def connect_material_property(material_path, from_node_id, property, from_output="", operation_id=None):
    arguments = {"material_path": material_path, "from_node_id": from_node_id, "property": property, "from_output": from_output}
    return _mutation("connect_material_property", arguments, operation_id, "material",
                     lambda resolved: api.connect_material_property(resolved, from_node_id, property, from_output))


def delete_material_node(material_path, node_id, graph_type="auto", operation_id=None):
    arguments = {"material_path": material_path, "node_id": node_id, "graph_type": graph_type}
    return _mutation("delete_material_node", arguments, operation_id, graph_type,
                     lambda resolved: api.delete_material_node(resolved, node_id, graph_type))


def layout_material_nodes(material_path, graph_type="auto", operation_id=None):
    arguments = {"material_path": material_path, "graph_type": graph_type}
    return _mutation("layout_material_nodes", arguments, operation_id, graph_type, lambda resolved: api.layout_material_nodes(resolved, graph_type))


def _json_input(text, *, object_required=False):
    """Validate syntax/size without loading or modifying an engine object."""
    if type(text) is not str or not text or len(text.encode("utf-16-le")) // 2 > 65536:
        raise OperationError("invalid_material_json", "JSON must contain one value and at most 65536 UTF-16 code units")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise OperationError("invalid_material_json", "Duplicate JSON property: " + key)
            result[key] = value
        return result
    try:
        value = json.loads(text, object_pairs_hook=unique)
        count = 0
        def bounded(item, depth=0):
            nonlocal count
            count += 1
            if depth > 16 or count > 8192:
                raise OperationError("invalid_material_json", "Property JSON exceeds depth 16 or 8192 values")
            if isinstance(item, float) and not math.isfinite(item):
                raise OperationError("invalid_material_json", "JSON numbers must be finite")
            if isinstance(item, dict):
                for member in item.values():
                    bounded(member, depth + 1)
            elif isinstance(item, list):
                for member in item:
                    bounded(member, depth + 1)
        bounded(value)
        if object_required and (type(value) is not dict or len(value) > 64):
            raise OperationError("invalid_material_properties", "properties_json must encode an object with at most 64 properties")
        normalized = json.dumps(value, ensure_ascii=False, sort_keys=True, allow_nan=False, separators=(",", ":"))
        if len(normalized.encode("utf-16-le")) // 2 > 65536:
            raise OperationError("invalid_material_json", "Normalized JSON exceeds the native 65536-character limit")
        return normalized
    except (ValueError, RecursionError) as error:
        raise OperationError("invalid_material_json", str(error)) from error


def add_material_node(material_path, expression_class, graph_type="auto", node_position=None,
                      selected_asset=None, properties_json=None, operation_id=None):
    try:
        position = [0, 0] if node_position is None else node_position
        if type(position) is not list or len(position) != 2 or any(type(value) is not int or not -(2 ** 31) <= value < 2 ** 31 for value in position):
            raise OperationError("invalid_material_node_position", "node_position must be two int32 coordinates; omitted/null means [0,0]")
        selected = "" if selected_asset is None else selected_asset
        if type(selected) is not str:
            raise OperationError("invalid_material_selected_asset", "selected_asset must be a path string or null")
        properties = _json_input("{}" if properties_json is None else properties_json, object_required=True)
    except Exception as error:
        return fail(new_result(operation_id), error)
    arguments = {"material_path": material_path, "expression_class": expression_class, "graph_type": graph_type,
                 "node_position": list(position), "selected_asset": selected, "properties_json": properties}
    return _mutation("add_material_node", arguments, operation_id, graph_type,
        lambda resolved: api.add_material_node(resolved, expression_class, graph_type, position, selected, properties))


def set_material_node_property(material_path, node_id, property_name, value_json, graph_type="auto", operation_id=None):
    try:
        value = _json_input(value_json)
    except Exception as error:
        return fail(new_result(operation_id), error)
    arguments = {"material_path": material_path, "node_id": node_id, "property_name": property_name,
                 "value_json": value, "graph_type": graph_type}
    return _mutation("set_material_node_property", arguments, operation_id, graph_type,
        lambda resolved: api.set_material_node_property(resolved, node_id, property_name, value, graph_type))


def add_custom_material_node(material_path, code, output_type="CMOT_Float3", inputs_json="[]", description=None,
                             graph_type="auto", node_position_json="[0,0]", additional_defines_json="[]",
                             include_file_paths_json="[]", operation_id=None):
    """Specialized Custom expression creation; native validators run before AddNode.

    Only JSON syntax/shape and int32 coordinates belong at this reflected
    boundary. Native code owns HLSL identifier/alias/define/include validation
    so it remains identical for every internal caller of the shared Add core.
    """
    try:
        if type(code) is not str or not code.strip() or len(code.encode("utf-16-le")) // 2 > 65536:
            raise OperationError("invalid_custom_material_code", "code must be nonempty HLSL with at most 65536 UTF-16 code units")
        arrays = {}
        for name, text in (("inputs_json", inputs_json), ("additional_defines_json", additional_defines_json),
                           ("include_file_paths_json", include_file_paths_json), ("node_position_json", node_position_json)):
            normalized = _json_input(text)
            value = json.loads(normalized)
            if type(value) is not list:
                raise OperationError("invalid_custom_material_array", name + " must encode a JSON array")
            limit = {"inputs_json": 64, "additional_defines_json": 64, "include_file_paths_json": 32,
                     "node_position_json": 2}[name]
            if len(value) > limit:
                raise OperationError("invalid_custom_material_array", name + " exceeds " + str(limit) + " entries")
            arrays[name] = (normalized, value)
        position = arrays["node_position_json"][1]
        if len(position) != 2 or any(type(value) is not int or not -(2 ** 31) <= value < 2 ** 31 for value in position):
            raise OperationError("invalid_material_node_position", "node_position_json must encode exactly two int32 coordinates")
        caption = "" if description is None else description
        if type(caption) is not str or len(caption.encode("utf-16-le")) // 2 > 4096:
            raise OperationError("invalid_custom_material_description", "description must be a string of at most 4096 UTF-16 code units or null")
    except Exception as error:
        return fail(new_result(operation_id), error)
    arguments = {"material_path": material_path, "code": code, "output_type": output_type,
                 "inputs_json": arrays["inputs_json"][0], "description": caption, "graph_type": graph_type,
                 "node_position": list(position), "additional_defines_json": arrays["additional_defines_json"][0],
                 "include_file_paths_json": arrays["include_file_paths_json"][0]}
    return _mutation("add_custom_material_node", arguments, operation_id, graph_type,
        lambda resolved: api.add_custom_material_node(resolved, code, output_type, arguments["inputs_json"], caption,
            graph_type, position, arguments["additional_defines_json"], arguments["include_file_paths_json"]))
