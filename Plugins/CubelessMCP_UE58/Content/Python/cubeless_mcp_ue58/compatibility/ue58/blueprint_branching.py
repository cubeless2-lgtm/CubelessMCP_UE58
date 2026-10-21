"""Original EnumLiteral8/IsValid7/SwitchInt10 through exact hidden Editor native methods."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintBranchingLibrary"
BINDINGS = ('add_blueprint_enum_literal_node', 'add_blueprint_is_valid_node', 'add_blueprint_switch_int_node')
HIDDEN_ARGUMENTS_BY_METHOD = {'add_blueprint_enum_literal_node': ('blueprint_name', 'enum_type', 'value_json', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_is_valid_node': ('blueprint_name', 'value_type', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_switch_int_node': ('blueprint_name', 'start_index', 'case_count', 'case_values_json', 'has_default_pin', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_branching_method_invalid", "Only exact original enum-literal/validity/integer-switch methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_branching_native_api_unavailable", "Exact native branching binding unavailable; no node/default/cleanup dispatched")
    if method == BINDINGS[1]:
        # Release only this exact validity method's bound callable; normal/BFL
        # selection still belongs to native, without a second target SDK lookup.
        try:
            raw = function(*arguments)
        finally:
            function = None
    else:
        raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_branching_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def add_blueprint_enum_literal_node(blueprint_name, enum_type, value_json, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke(BINDINGS[0], (blueprint_name, enum_type, value_json, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_blueprint_is_valid_node(blueprint_name, value_type, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke(BINDINGS[1], (blueprint_name, value_type, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_blueprint_switch_int_node(blueprint_name, start_index, case_count, case_values_json, has_default_pin, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke(BINDINGS[2], (blueprint_name, start_index, case_count, case_values_json, has_default_pin, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


