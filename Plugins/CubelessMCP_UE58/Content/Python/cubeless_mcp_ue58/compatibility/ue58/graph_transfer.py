"""Original Import13/Repair6 through exact Editor native graph-transfer methods.

Producer uses immutable Variable encoder so actual compiler/save flags survive.
The shared scalar bool/null decoder does not hardcode node-operation zero flags.
"""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintGraphTransferLibrary"
BINDINGS = ('import_blueprint_graph_nodes_from_text', 'repair_blueprint_pin_links')
HIDDEN_ARGUMENTS_BY_METHOD = {'import_blueprint_graph_nodes_from_text': ('blueprint_name', 'text', 'text_file_path', 'graph_name', 'graph_id', 'graph_type', 'clear_graph', 'keep_root_node', 'allow_non_sample', 'validate_only', 'dry_run', 'compile', 'save_asset'), 'repair_blueprint_pin_links': ('blueprint_name', 'allow_non_sample', 'validate_only', 'dry_run', 'compile', 'save_asset')}



def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}



def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("graph_transfer_method_invalid", "Only exact original graph import/repair methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("graph_transfer_native_api_unavailable", "Exact native graph-transfer binding unavailable; no import/repair/compiler/save dispatched")
    if method == "import_blueprint_graph_nodes_from_text":
        try:
            raw = function(*arguments)
        finally:
            function = None  # Release the import callable before decode/traceback; no text parsing or new admission.
    else:
        raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_graph_transfer_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def import_blueprint_graph_nodes_from_text(blueprint_name, text, text_file_path, graph_name, graph_id, graph_type, clear_graph, keep_root_node, allow_non_sample, validate_only, dry_run, compile, save_asset):
    return _invoke('import_blueprint_graph_nodes_from_text', (blueprint_name, text, text_file_path, graph_name, graph_id, graph_type, clear_graph, keep_root_node, allow_non_sample, validate_only, dry_run, compile, save_asset))


def repair_blueprint_pin_links(blueprint_name, allow_non_sample, validate_only, dry_run, compile, save_asset):
    return _invoke('repair_blueprint_pin_links', (blueprint_name, allow_non_sample, validate_only, dry_run, compile, save_asset))
