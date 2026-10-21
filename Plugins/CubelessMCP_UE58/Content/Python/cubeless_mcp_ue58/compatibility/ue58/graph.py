"""Public reflected BlueprintGraphEditor/BlueprintGraphPin API calls in UE58."""

import json
import uuid
import unreal

from .blueprint import resolve_blueprint
from cubeless_mcp_ue58.operations.state import OperationError


def graph_editor(blueprint_name, graph_name):
    _selector(graph_name, "graph_name")
    blueprint = resolve_blueprint(blueprint_name)
    editor = unreal.BlueprintGraphEditor.get_graph_editor_by_name(blueprint, unreal.Name(graph_name))
    if not editor:
        raise OperationError("graph_not_found", f"Graph does not exist: {graph_name}")
    return editor


def _selector(value, field):
    if (type(value) is not str or not value
            or any(ord(c) < 32 or ord(c) == 127 or 0xD800 <= ord(c) <= 0xDFFF for c in value)
            or len(value.encode("utf-16-le")) // 2 >= 1024):
        raise OperationError("graph_selector_invalid", field + " must be nonempty and shorter than 1024 UTF-16 units without controls")
    return value


def node_id(node):
    # Object path is serialized and explicit. Do not pretend it is the legacy GUID.
    return node.get_path_name()


def pin_category(pin):
    schema_text = pin.get_pin_type_as_json_schema()
    schema = json.loads(schema_text) if schema_text else {}
    # UE58 exports exec exactly. Other values are JSON-schema type labels,
    # not a claim that every legacy FEdGraphPinType category has been ported.
    return schema.get("type", "unknown")


def _native_read(method, *arguments):
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), method, None)
    if function is None:
        raise OperationError("native_graph_api_unavailable", "Build/load the Cubeless graph observation helpers; no mutation fallback executed")
    try:
        try:
            raw = function(*arguments)
        finally:
            # A raised diagnostic retains this frame through its traceback.
            # Release only our bound Unreal callable by normal refcounting before
            # JSON decoding/validation can allocate or leave an exception cycle.
            function = None
        value = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_graph_result", "Graph observation did not return valid JSON") from error
    if type(value) is not dict or type(value.get("success")) is not bool:
        raise OperationError("invalid_native_graph_result", "Graph observation lacks a typed success flag")
    if not value["success"]:
        error = OperationError(value.get("error_code") or "native_graph_read_failed", value.get("error") or "Native graph observation failed")
        if value.get("member_scope") in ("own_native_object_array", "own_native_material_value", "own_native_vector4_array", "external_native_vector_array", "own_native_string_vector_map", "own_native_string_double_map", "local_native_string_double_map", "local_primitive", "local_native_value_struct", "local_referenced_value", "local_primitive_array", "external_native_texture_enum_setter", "external_generated_actor") or value.get("connection_scope") in ("native_string_vector_map", "native_string_double_map", "string1_datatable_row", "scalar_uds_datatable_row", "native_asset_data_break", "native_datatable_bool_select", "native_name_foreach_connection", "native_is_valid_exec_macro") or "bfl_name_array_context_failure" in value or "bfl_name_array_context" in value or "bfl_target_context" in value or type(value.get("dynamic_cast_source_observation")) is dict and "bfl_target_context" in value["dynamic_cast_source_observation"]:
            # Preserve this finite read-only plan failure without another engine call.
            error.native_graph_result = value
        raise error
    for key in ("mutation_attempted", "saved", "compiled", "loaded_during_read"):
        if value.get(key) is not False:
            error = OperationError("invalid_native_graph_result", "Graph observation reported an unexpected mutation/compile/save/load")
            if value.get("member_scope") in ("local_primitive", "local_native_value_struct", "local_referenced_value", "local_primitive_array", "external_generated_actor") or value.get("connection_scope") in ("scalar_uds_datatable_row", "native_asset_data_break", "native_datatable_bool_select", "native_name_foreach_connection", "native_is_valid_exec_macro"):
                value["read_contract_first_mismatch"] = {
                    "object": value.get("graph_path", ""), "property": key,
                    "expected": False, "actual_present": key in value,
                    "actual": value.get(key), "stage": "native_read_contract",
                    "observation_kind": "copied_response_field_not_failed_object_reread"}
                error.native_graph_result = value
            raise error
    return value


def _identities(graph_path, *, include_total=False, include_observation=False):
    native = _native_read("read_node_identities", graph_path)
    rows = native.get("nodes")
    if type(rows) is not list or len(rows) > 2048:
        raise OperationError("invalid_native_graph_result", "Graph identity rows are missing or exceed the observation cap")
    total = native.get("total_node_count")
    if type(total) is not int or not len(rows) <= total <= 2048:
        raise OperationError("invalid_native_graph_result", "Graph observation lacks a valid total node count including comments")
    result = {}
    seen = set()
    for row in rows:
        try:
            path = row["object_path"]
            guid = uuid.UUID(row["node_guid"])
            if type(path) is not str or not path or path in result or not guid.int or guid.hex in seen:
                raise ValueError("invalid or duplicated node identity")
        except (TypeError, KeyError, ValueError, AttributeError) as error:
            raise OperationError("invalid_native_graph_result", "Graph has an invalid or duplicated node identity row") from error
        result[path] = dict(row, node_guid=guid.hex)
        seen.add(guid.hex)
    if include_observation:
        return result, total, native
    return (result, total) if include_total else result


def node_info(node, identities=None, *, exact_pin_types=False):
    identities = identities if identities is not None else _identities(node.get_outer().get_path_name())
    identity = identities.get(node_id(node))
    if identity is None:
        raise OperationError("graph_identity_mismatch", "Reflected node is absent from the independently observed native identity rows")
    position = node.get_node_pos()
    pins = []
    for pin in node.list_all_pins():
        direction = "output" if pin.get_pin_direction() == unreal.EdGraphPinDirection.EGPD_OUTPUT else "input"
        pins.append({"name": str(pin.get_pin_name()), "direction": direction,
                     "category": pin_category(pin), "category_encoding": "UE58_pin_json_schema_type",
                     "type_display": str(pin.get_pin_type_display_string()), "value": pin.get_pin_value(),
                     "links": [{"node_id": node_id(other.get_owning_node()), "pin_name": str(other.get_pin_name())}
                               for other in pin.list_connected_pins()]})
        if exact_pin_types:
            # Public GetPinType/StructBase.export_text observes exact native
            # float/double, references and containers without localized labels.
            pins[-1]["native_pin_type_text"] = pin.get_pin_type().export_text()
    result = {"node_id": node_id(node), "node_identifier_kind": "object_path", "node_guid": identity["node_guid"], "name": node.get_name(),
            "class": node.get_class().get_name(), "title": str(node.get_node_title()),
            "x": int(position.x), "y": int(position.y), "pins": pins}
    for key in ("variable_name", "self_context", "variable_property_path", "variable_member_parent_path", "variable_member_guid", "event_name", "event_member_parent_path",
                "function_name", "function_path", "function_owner_path", "custom_function_name", "custom_event_function_flags_string", "call_in_editor", "custom_event_signature"):
        if key in identity:
            result[key] = identity[key]
    return result


def graph_info(editor, *, exact_pin_types=False):
    nodes = list(editor.list_all_nodes())
    if len(nodes) > 2048:
        raise OperationError("graph_too_large", "This bounded synchronous graph tool supports at most 2048 K2 nodes")
    if exact_pin_types:
        identities, total, observation = _identities(editor.get_graph().get_path_name(), include_observation=True)
    else:
        identities, total = _identities(editor.get_graph().get_path_name(), include_total=True)
    if {node_id(node) for node in nodes} != set(identities):
        raise OperationError("graph_identity_mismatch", "Reflected/native K2 node sets differ; no partial graph success returned")
    result = {"graph_name": editor.get_graph().get_name(), "graph_path": editor.get_graph().get_path_name(),
            "nodes": [node_info(node, identities, exact_pin_types=exact_pin_types) for node in nodes], "node_count": len(nodes), "total_node_count": total,
            "node_scope": "K2 nodes returned by official BlueprintGraphEditor.list_all_nodes; comments are excluded"}
    if exact_pin_types:
        if type(observation.get("dirty_after")) is not bool:
            raise OperationError("invalid_native_graph_result", "Connection snapshot lacks an observed package dirty state")
        result["package_dirty"] = observation["dirty_after"]
    return result


def find_node(editor, requested_id):
    _selector(requested_id, "node_id")
    if requested_id.startswith("/"):
        resolved_path = requested_id
    else:
        try:
            guid = uuid.UUID(requested_id)
            if not guid.int:
                raise ValueError("zero GUID")
        except ValueError as error:
            raise OperationError("node_identifier_invalid", "Node selector is neither an object path nor a nonzero GUID") from error
        identities = _identities(editor.get_graph().get_path_name())
        paths = [path for path, row in identities.items() if row["node_guid"] == guid.hex]
        resolved_path = paths[0] if len(paths) == 1 else None
    matches = [node for node in editor.list_all_nodes() if node_id(node) == resolved_path]
    if len(matches) != 1:
        raise OperationError("node_not_found", f"Expected one node matching the exact object path or GUID, found {len(matches)}")
    return matches[0]


def find_pin(node, name, direction):
    matches = [pin for pin in node.list_all_pins()
               if str(pin.get_pin_name()) == name and
               ((pin.get_pin_direction() == unreal.EdGraphPinDirection.EGPD_OUTPUT) == (direction == "output"))]
    if len(matches) != 1:
        raise OperationError("pin_not_found", f"Expected one {direction} pin named {name}, found {len(matches)}")
    return matches[0]


def add_branch(editor, x, y):
    node = editor.add_branch_node()
    if not node:
        raise OperationError("node_creation_failed", "Official BlueprintGraphEditor did not create a Branch node")
    node.set_node_pos(unreal.IntPoint(x, y))
    return node


def sequence_editor(blueprint_path, graph_name, graph_id, graph_type):
    # Reuse the exact selector resolver with create=False; no implicit graph
    # creation, menu-label lookup or UI context is used.
    from . import functions
    value = functions.resolve_blueprint_graph(blueprint_path, graph_name, graph_id, graph_type, False)
    if not value["success"]:
        raise OperationError(value.get("error_code") or "sequence_graph_resolution_failed", value.get("error") or "Graph resolution failed")
    if value["mutation_attempted"] or value.get("changed") is not False or any(value[key] for key in ("compile_attempted", "save_attempted", "saved")):
        raise OperationError("sequence_graph_resolution_changed_state", "Existing graph resolution reported unexpected changes; inspect the Blueprint")
    selected = value["data"]["graph"]
    editor = graph_editor(blueprint_path, selected["graph_name"])
    if editor.get_graph().get_path_name() != selected["object_path"]:
        raise OperationError("sequence_graph_resolution_mismatch", "Reflected graph identity does not match exact native selectors; no mutation dispatched")
    return editor, selected


def _control_flow(method, arguments):
    if method not in ("add_blueprint_branch_node", "add_blueprint_sequence_node"):
        raise OperationError("native_function_api_unavailable", "Only the two original control-flow methods are admitted")
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), method, None)
    if not callable(function):
        raise OperationError("native_function_api_unavailable", "Build/load the exact control-flow Native binding; no fallback")
    try:
        raw = function(*arguments)
    finally:
        function = None
    from .functions import _decode_result
    try:
        value = _decode_result(raw)
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def add_blueprint_branch_node(blueprint_path, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _control_flow("add_blueprint_branch_node", (blueprint_path, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_blueprint_sequence_node(blueprint_path, node_position_json, output_count, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _control_flow("add_blueprint_sequence_node", (blueprint_path, node_position_json, output_count, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_sequence(editor, x, y, output_count):
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_sequence_node", None)
    if function is None:
        raise OperationError("native_sequence_api_unavailable", "Build/load the exact Sequence helper; no localized-menu fallback executed")
    try:
        raw = function(editor.get_graph().get_path_name(), x, y, output_count)
    finally:
        function = None
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_sequence_result", "Sequence response is invalid JSON; dispatched outcome remains unknown") from error
    flags = ("success", "mutation_attempted", "change_state_known", "postcondition_passed", "compiled", "saved", "loaded_during_operation")
    if (type(value) is not dict or type(value.get("data")) is not dict
            or any(type(value.get(key)) is not bool for key in flags)
            or value["change_state_known"] and type(value.get("changed")) is not bool):
        raise OperationError("invalid_native_sequence_result", "Sequence response lacks typed outcome observations")
    return value


def move_node(node, x, y):
    node.set_node_pos(unreal.IntPoint(x, y))


def remove_node(editor, node):
    editor.remove_nodes([node])


def connect(output_pin, input_pin):
    return bool(output_pin.try_create_connection(input_pin))


def _connect_finite_native_pins(method, editor, source_node_path, source_pin_name, target_node_path, target_pin_name):
    """Only the two finite schema+aggregate-notification leaves share this parser."""
    if method not in ("connect_data_table_bool_select_pins", "connect_name_foreach_pins"):
        raise OperationError("native_connection_api_unavailable", "Only the two finite native connection methods are permitted")
    select = method == "connect_data_table_bool_select_pins"
    label = "Select" if select else "Name ForEach"
    invalid_code = "invalid_native_select_connection_result" if select else "invalid_native_name_foreach_connection_result"
    function = getattr(getattr(unreal, "CubelessFunctionLibrary", None), method, None)
    if function is None:
        raise OperationError("native_connection_api_unavailable", "Build/load the finite " + label + " connection helper; no generic fallback dispatched")
    try:
        raw = function(editor.get_graph().get_path_name(), source_node_path, source_pin_name, target_node_path, target_pin_name)
    finally:
        function = None
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as cause:
        error = OperationError(invalid_code, label + " connection returned invalid JSON; dispatch outcome remains unknown")
        error.native_connection_raw = raw
        raise error from cause
    flags = ("success", "mutation_attempted", "change_state_known", "postcondition_passed", "compiled", "saved")
    if (type(value) is not dict or type(value.get("data")) is not dict
            or any(type(value.get(key)) is not bool for key in flags)
            or value["change_state_known"] and type(value.get("changed")) is not bool
            or "engine_return" not in value or value["engine_return"] is not None and type(value["engine_return"]) is not bool):
        error = OperationError(invalid_code, label + " connection lacks typed actual outcomes; no SDK read or retry is safe")
        error.native_connection_raw = raw
        error.native_connection_result = value
        raise error
    return value



def connect_data_table_bool_select_pins(editor, source_node_path, source_pin_name, target_node_path, target_pin_name):
    """One finite native write completes schema connection and required public node notification."""
    return _connect_finite_native_pins("connect_data_table_bool_select_pins", editor,
        source_node_path, source_pin_name, target_node_path, target_pin_name)


def connect_name_foreach_pins(editor, source_node_path, source_pin_name, target_node_path, target_pin_name):
    """Exact RowNames or Name MakeArray->stock ForEach schema+notification; no generic fallback."""
    return _connect_finite_native_pins("connect_name_foreach_pins", editor,
        source_node_path, source_pin_name, target_node_path, target_pin_name)

def connection_plan(editor, source_node_id, source_pin_name, target_node_id, target_pin_name):
    native = _native_read("read_connection_plan", editor.get_graph().get_path_name(), source_node_id,
        source_pin_name, target_node_id, target_pin_name)
    if any(type(native.get(key)) is not bool for key in ("direct_connection_allowed", "already_linked", "type_exactly_equal")):
        raise OperationError("invalid_native_graph_result", "Graph connection plan lacks its exact schema decision")
    connection_target_default_state(native)
    return native


def connection_target_default_state(plan):
    """Read additive public native pin defaults without inferring missing data."""
    value = plan.get("target_pin_default_state")
    if value is None:
        return None
    if (type(value) is not dict
            or any(type(value.get(key)) is not str for key in
                   ("raw_default_value", "autogenerated_default_value", "default_object_path"))
            or any(type(value.get(key)) is not bool for key in
                   ("default_text_empty", "is_orphaned", "default_value_is_ignored", "default_value_is_read_only"))):
        raise OperationError("invalid_native_graph_result", "Connection target defaults lack exact public native observations")
    return dict(value)


def connection_editor(blueprint_path, graph_name, graph_id, graph_type):
    try:
        return sequence_editor(blueprint_path, graph_name, graph_id, graph_type)
    except OperationError as error:
        raise OperationError(error.code.replace("sequence_", "connection_"), str(error)) from error


def replace_exec_link(editor, source_node_path, source_pin_name, target_node_path, target_pin_name,
        expected_old_target_node_path, expected_old_target_pin_name):
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "replace_exec_link", None)
    if function is None:
        raise OperationError("native_connection_api_unavailable", "Build/load the guarded connection helper; no fallback dispatched")
    raw = function(editor.get_graph().get_path_name(), source_node_path, source_pin_name,
        target_node_path, target_pin_name, expected_old_target_node_path, expected_old_target_pin_name, 1, "break_others_a")
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_connection_result", "Connection response is invalid JSON; dispatch outcome remains unknown") from error
    flags = ("success", "mutation_attempted", "change_state_known", "postcondition_passed", "compiled", "saved")
    if (type(value) is not dict or type(value.get("data")) is not dict
            or any(type(value.get(key)) is not bool for key in flags)
            or value["change_state_known"] and type(value.get("changed")) is not bool
            or "engine_return" not in value or value["engine_return"] is not None and type(value["engine_return"]) is not bool):
        raise OperationError("invalid_native_connection_result", "Connection response lacks typed outcomes; inspect the graph before another action")
    return value


def add_string_double_local_getter(graph_path, variable_name, expected_local_guid, x, y):
    """One hidden editor leaf; native rechecks current local scope, never a class-member fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_string_double_local_getter", None)
    if function is None:
        raise OperationError("native_local_map_api_unavailable", "Build/load the exact local getter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_string_double_local_setter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact hidden stock local setter leaf; no class-member/literal fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_string_double_local_setter", None)
    if function is None:
        raise OperationError("native_local_map_api_unavailable", "Build/load the exact local setter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_primitive_local_getter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact finite scalar local getter; no class-member or runtime value fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_primitive_local_getter", None)
    if function is None:
        raise OperationError("native_local_primitive_api_unavailable", "Build/load the exact primitive local getter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_primitive_local_setter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact finite scalar local setter; no class-member or runtime value fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_primitive_local_setter", None)
    if function is None:
        raise OperationError("native_local_primitive_api_unavailable", "Build/load the exact primitive local setter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_native_value_local_getter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact finite native Vector/Rotator local getter; no generic struct fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_native_value_local_getter", None)
    if function is None:
        raise OperationError("native_local_value_struct_api_unavailable", "Build/load the exact native value local getter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_native_value_local_setter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact finite native Vector/Rotator local setter; no generic struct fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_native_value_local_setter", None)
    if function is None:
        raise OperationError("native_local_value_struct_api_unavailable", "Build/load the exact native value local setter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_referenced_local_getter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact finite UDE byte/native Object or Class local getter; no arbitrary source fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_referenced_local_getter", None)
    if function is None:
        raise OperationError("native_local_referenced_api_unavailable", "Build/load the exact referenced local getter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_referenced_local_setter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact finite UDE byte/native Object or Class local setter; no arbitrary source fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_referenced_local_setter", None)
    if function is None:
        raise OperationError("native_local_referenced_api_unavailable", "Build/load the exact referenced local setter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_primitive_array_local_getter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact finite primitive Array local getter; no class-member or runtime value fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_primitive_array_local_getter", None)
    if function is None:
        raise OperationError("native_local_primitive_array_api_unavailable", "Build/load the exact primitive Array local getter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)


def add_primitive_array_local_setter(graph_path, variable_name, expected_local_guid, x, y):
    """Exact finite primitive Array local setter; no class-member or runtime value fallback."""
    from .functions import _decode_result
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "add_primitive_array_local_setter", None)
    if function is None:
        raise OperationError("native_local_primitive_array_api_unavailable", "Build/load the exact primitive Array local setter leaf; no fallback dispatched")
    try:
        raw = function(graph_path, variable_name, expected_local_guid, x, y)
    finally:
        function = None
    return _decode_result(raw)
