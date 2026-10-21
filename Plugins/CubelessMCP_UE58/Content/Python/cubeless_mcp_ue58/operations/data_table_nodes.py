"""MH dedicated row authoring: native wildcard construction, never runtime row reads."""

from cubeless_mcp_ue58.compatibility.ue58 import data_table_nodes as api
from . import functions
from .state import OperationError, fail, mutate, new_result


def _arguments(blueprint_name, row_struct_path, node_position_json, graph_name, graph_id,
        graph_type, create_graph_if_missing):
    path = functions._text(row_struct_path, "row_struct_path")
    # This is an exact source asset selector, not a Blueprint simple-name alias or
    # an object value literal. Native independently checks saved/current String1.
    package, separator, name = path.rpartition(".")
    if (not path.startswith("/Game/") or not separator or not name
            or package.rsplit("/", 1)[-1] != name
            or any(c.isspace() or c in "\\:'\"" for c in path)):
        raise OperationError("datatable_node_struct_path_invalid", "row_struct_path must be an exact Game package.asset UDS object path")
    create = functions._bool(create_graph_if_missing, "create_graph_if_missing")
    if create:
        raise OperationError("datatable_node_graph_creation_unsupported", "Resolve the exact existing graph separately; row authoring does not create graphs")
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if (type(position) is not list or len(position) != 2
            or any(type(v) is not int or abs(v) > 1000000 for v in position)):
        raise OperationError("datatable_node_position_invalid", "node_position_json must be two integers in -1000000..1000000")
    values = functions._selectors(blueprint_name, graph_name, graph_id, graph_type)
    if values["graph_type"] not in ("", "any", "event", "function"):
        raise OperationError("datatable_node_graph_type_unsupported", "Use an exact existing event/function graph; macro/delegate authoring is incomplete")
    return {**values, "row_struct_path": path,
        "node_position_json": functions.variables._json_text(position), "create_graph_if_missing": False}


def _run(result, values, checkpoint_callback=None):
    result["data"].update(node_only_contract=True, data_table_input_requires_connection=True,
        row_name_input_requires_connection=True, row_result_initial_contract="wildcard_pending_typed_connection",
        row_runtime_execution_verified=False, full_legacy_typed_row_parity=False)
    functions._run_action(result, "add_blueprint_get_data_table_row_node", values,
        checkpoint_callback, operation_api=api)


def add_blueprint_get_data_table_row_node(blueprint_name, row_struct_path, node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    try:
        values = _arguments(blueprint_name, row_struct_path, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("add_blueprint_get_data_table_row_node", values, operation_id, lambda result: _run(result, values))


def add_blueprint_get_data_table_row_node_action(result, blueprint_name, row_struct_path, node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, _arguments(blueprint_name, row_struct_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
