"""Original List7/Delete12 scalar ABI; no lookup, observer, compiler or transport."""
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = 'CubelessGraphLibrary'
BINDINGS = ('list_blueprint_nodes', 'delete_blueprint_node')
FIELDS = {
    BINDINGS[0]: ('blueprint_name', 'node_type', 'title_contains', 'include_pins', 'graph_name', 'graph_id', 'graph_type'),
    BINDINGS[1]: ('blueprint_name', 'node_id', 'graph_name', 'graph_id', 'graph_type', 'expected_node_name',
        'expected_node_class', 'expected_title_contains', 'allow_non_exec_linked_delete', 'allow_exec_linked_delete',
        'allow_any_linked_delete', 'allow_orphaned_event_delete'),
}

def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}

def _invoke(method, values):
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        error = OperationError('blueprint_node_cleanup_native_api_unavailable', 'Exact List7/Delete12 binding unavailable; no fallback or deletion')
        error.native_call_dispatched = False
        raise error
    try:
        raw = function(*(values[key] for key in FIELDS[method]))
    finally:
        function = None
    try:
        result = _decode_result(raw)
        if result.get('operation') != method:
            raise OperationError('blueprint_node_cleanup_result_invalid', 'Native operation differs; actual raw result retained')
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    result['_raw_native_json'] = raw
    return result

def list_blueprint_nodes(blueprint_name, node_type='', title_contains='', include_pins=True, graph_name='', graph_id='', graph_type=''):
    return _invoke(BINDINGS[0], dict(blueprint_name=blueprint_name, node_type=node_type, title_contains=title_contains,
        include_pins=include_pins, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type))

def delete_blueprint_node(blueprint_name, node_id, graph_name='', graph_id='', graph_type='', expected_node_name='',
        expected_node_class='', expected_title_contains='', allow_non_exec_linked_delete=False,
        allow_exec_linked_delete=False, allow_any_linked_delete=False, allow_orphaned_event_delete=False):
    return _invoke(BINDINGS[1], dict(blueprint_name=blueprint_name, node_id=node_id, graph_name=graph_name,
        graph_id=graph_id, graph_type=graph_type, expected_node_name=expected_node_name, expected_node_class=expected_node_class,
        expected_title_contains=expected_title_contains, allow_non_exec_linked_delete=allow_non_exec_linked_delete,
        allow_exec_linked_delete=allow_exec_linked_delete, allow_any_linked_delete=allow_any_linked_delete,
        allow_orphaned_event_delete=allow_orphaned_event_delete))
