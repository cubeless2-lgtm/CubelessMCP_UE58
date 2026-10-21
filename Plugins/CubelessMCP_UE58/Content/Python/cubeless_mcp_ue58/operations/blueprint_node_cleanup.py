"""Original List7/Delete12; Native owns selected graph, expected guards and retirement."""
import copy
from ..compatibility.ue58 import blueprint_node_cleanup as api
from . import state
from .state import OperationError

FIELDS = api.FIELDS
DEFAULTS = {
    'list_blueprint_nodes': dict(node_type='', title_contains='', include_pins=True, graph_name='', graph_id='', graph_type=''),
    'delete_blueprint_node': dict(graph_name='', graph_id='', graph_type='', expected_node_name='', expected_node_class='',
        expected_title_contains='', allow_non_exec_linked_delete=False, allow_exec_linked_delete=False,
        allow_any_linked_delete=False, allow_orphaned_event_delete=False),
}
BOOLS = {'list_blueprint_nodes': ('include_pins',), 'delete_blueprint_node':
    ('allow_non_exec_linked_delete', 'allow_exec_linked_delete', 'allow_any_linked_delete', 'allow_orphaned_event_delete')}

def _arguments(method, values):
    if method not in FIELDS or type(values) is not dict or set(values)-set(FIELDS[method]):
        raise OperationError('blueprint_node_cleanup_arguments_invalid', 'Only original List7/Delete12 fields admitted')
    public = {**DEFAULTS[method], **values}
    if set(public) != set(FIELDS[method]) or any(type(public[key]) is not (bool if key in BOOLS[method] else str) for key in FIELDS[method]):
        raise OperationError('blueprint_node_cleanup_arguments_invalid', 'Original string/Boolean fields and required inputs must be retained')
    return dict(method=method, public=copy.deepcopy(public))

def _context(result, arguments):
    result['data'].update(operation=arguments['method'], requested_arguments=copy.deepcopy(arguments['public']),
        native_selector_filter_expected_guards_link_permission_and_retirement_authoritative=True,
        adapter_explicit_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        original_public_compile_save_operation_id_added=False, automatic_retry=False,
        getter_cache_and_transitive_SDK_effects_not_total_zero=True)

def _adopt(result, arguments, native):
    known = native['change_state_known']
    result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_raw_result=native['_raw_native_json'],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ('data', '_raw_native_json')},
        native_result_returned=True, native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'], change_state_known=known,
        mutation_state_unknown=not known, mutation_performed=native['changed'] if known else None)
    _context(result, arguments)
    result['compile'].update(requested=native['compile_requested'], attempted=native['compile_attempted'], compiled=native['compiled'],
        status='native_returned_not_attempted' if not native['compile_attempted'] else 'native_returned_compiled' if native['compiled'] is True else 'native_returned_compile_failed' if native['compiled'] is False else 'native_returned_compile_unverified')
    result['save'].update(requested=native['save_requested'], attempted=native['save_attempted'], saved=native['saved'])
    no_work = known and native['changed'] is False and not native['mutation_attempted'] and not native['compile_attempted'] and not native['save_attempted']
    result['changes'] = [] if no_work else [dict(kind=arguments['method'], attempted=native['mutation_attempted'],
        changed=native['changed'] if known else None, change_state_known=known, postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_node_cleanup_postcondition', native_postcondition_passed=native['postcondition_passed'],
        runtime='not_run', disk_reload='not_run', full_original_node_pin_JSON='native_reported_scope_only')
    result['stage'] = 'returned_blueprint_node_cleanup_operation'
    result['retry'].update(automatic_retry=False, duplicate_risk='inspect_retained_raw_node_graph_and_deletion_prefix_no_replay' if native['mutation_attempted'] or not known else 'none_observed')

def _run(result, arguments, callback=None):
    method = arguments['method']; observer = method == 'list_blueprint_nodes'
    publish = callback or ((lambda value: None) if observer else state.checkpoint)
    _context(result, arguments)
    result['data'].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        execution_mutation_attempted=False, change_state_known=True, mutation_state_unknown=False, mutation_performed=False,
        pending_operation=copy.deepcopy(arguments))
    result['compile'].update(requested=False, attempted=False, compiled=None, status='not_requested')
    result['save'].update(requested=False, attempted=False, saved=False)
    result['stage'] = 'before_read_checkpoint' if observer else 'before_mutation_checkpoint'
    publish(result)
    result['stage'] = 'native_blueprint_node_cleanup_operation'
    result['data'].update(native_dispatched=True, native_mutation_attempted=None, execution_mutation_attempted=None,
        change_state_known=False, mutation_state_unknown=True, mutation_performed=None)
    result['changes'] = [dict(kind=method, attempted=None, changed=None, change_state_known=False)]
    try:
        native = getattr(api, method)(**arguments['public'])
    except Exception as error:
        if getattr(error, 'native_call_dispatched', None) is False:
            result['data'].update(native_dispatched=False, native_mutation_attempted=False, execution_mutation_attempted=False,
                change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result['changes'] = []
        else:
            result['retry'].update(automatic_retry=False, duplicate_risk='unknown_native_dispatch_no_replay')
        if getattr(error, 'native_call_returned', False):
            result['data'].update(native_result_returned=True, native_raw_result=getattr(error, 'native_raw_result', None))
            result['stage'] = 'returned_blueprint_node_cleanup_operation'
            publish(result)
        raise
    _adopt(result, arguments, native)
    publish(result)
    if not native['success']:
        raise OperationError(native.get('error_code') or 'blueprint_node_cleanup_native_failed', native.get('error') or 'Actual Native node/graph/partial prefix retained')
    if not native['change_state_known'] or not native['postcondition_passed']:
        raise OperationError('blueprint_node_cleanup_unverified', 'Native success lacks current postcondition; no replay or next operation')
    result.update(success=True, stage='complete')
    return result

def _named(arguments):
    if arguments['method'] == 'delete_blueprint_node':
        return state.mutate(arguments['method'], arguments, None, lambda result: _run(result, arguments))
    result = state.new_result(); result['data']['journal_created'] = False
    try: return _run(result, arguments)
    except Exception as error: return state.fail(result, error)

def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result['data'].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, execution_mutation_attempted=False, change_state_known=True,
        mutation_state_unknown=False, mutation_performed=False)
    return result

def list_blueprint_nodes(blueprint_name, node_type='', title_contains='', include_pins=True, graph_name='', graph_id='', graph_type=''):
    try: arguments = _arguments('list_blueprint_nodes', dict(blueprint_name=blueprint_name, node_type=node_type, title_contains=title_contains, include_pins=include_pins, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type))
    except Exception as error: return _rejected('list_blueprint_nodes', error)
    return _named(arguments)

def list_blueprint_nodes_action(result, blueprint_name, node_type='', title_contains='', include_pins=True, graph_name='', graph_id='', graph_type='', checkpoint_callback=None):
    return _run(result, _arguments('list_blueprint_nodes', dict(blueprint_name=blueprint_name, node_type=node_type, title_contains=title_contains, include_pins=include_pins, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type)), checkpoint_callback)

def delete_blueprint_node(blueprint_name, node_id, graph_name='', graph_id='', graph_type='', expected_node_name='', expected_node_class='', expected_title_contains='', allow_non_exec_linked_delete=False, allow_exec_linked_delete=False, allow_any_linked_delete=False, allow_orphaned_event_delete=False):
    try: arguments = _arguments('delete_blueprint_node', dict(blueprint_name=blueprint_name, node_id=node_id, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, expected_node_name=expected_node_name, expected_node_class=expected_node_class, expected_title_contains=expected_title_contains, allow_non_exec_linked_delete=allow_non_exec_linked_delete, allow_exec_linked_delete=allow_exec_linked_delete, allow_any_linked_delete=allow_any_linked_delete, allow_orphaned_event_delete=allow_orphaned_event_delete))
    except Exception as error: return _rejected('delete_blueprint_node', error)
    return _named(arguments)

def delete_blueprint_node_action(result, blueprint_name, node_id, graph_name='', graph_id='', graph_type='', expected_node_name='', expected_node_class='', expected_title_contains='', allow_non_exec_linked_delete=False, allow_exec_linked_delete=False, allow_any_linked_delete=False, allow_orphaned_event_delete=False, checkpoint_callback=None):
    return _run(result, _arguments('delete_blueprint_node', dict(blueprint_name=blueprint_name, node_id=node_id, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, expected_node_name=expected_node_name, expected_node_class=expected_node_class, expected_title_contains=expected_title_contains, allow_non_exec_linked_delete=allow_non_exec_linked_delete, allow_exec_linked_delete=allow_exec_linked_delete, allow_any_linked_delete=allow_any_linked_delete, allow_orphaned_event_delete=allow_orphaned_event_delete)), checkpoint_callback)
