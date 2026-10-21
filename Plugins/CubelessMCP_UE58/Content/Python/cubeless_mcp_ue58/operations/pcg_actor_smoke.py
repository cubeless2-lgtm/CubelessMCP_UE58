"""Original PCG actor smoke6 composite, one journal; no runtime completion claim."""
import copy
from . import pcg_contract_authoring as contracts, state
from .state import OperationError
from ..compatibility.ue58 import pcg_actor_smoke as api

FIELDS = ('graph_path', 'actor_label', 'selected_only', 'cleanup', 'generate', 'dry_run')
DEFAULTS = dict(actor_label='', selected_only=False, cleanup=True, generate=True, dry_run=True)
_ACTIVE = False


def _arguments(arguments):
    if type(arguments) is not dict or set(arguments) - set(FIELDS):
        raise OperationError('pcg_actor_smoke_arguments_invalid', 'Exact original six fields required')
    values = {**DEFAULTS, **arguments}
    if 'graph_path' not in values or any(type(values[k]) is not str for k in ('graph_path', 'actor_label')) or any(type(values[k]) is not bool for k in FIELDS[2:]):
        raise OperationError('pcg_actor_smoke_arguments_invalid', 'Original string2 and Boolean4 inputs required')
    return values


def _adopt(result, receipt):
    known = receipt.get('change_state_known') is True
    attempted = receipt.get('mutation_attempted') is True
    result['data'].update(current_Python_receipt=copy.deepcopy(receipt), change_state_known=known,
        execution_mutation_attempted=attempted, mutation_state_unknown=not known,
        mutation_performed=True if receipt.get('observed_changed') is True else None if attempted else False,
        dispatch_boundary_known=known, runtime_change_observed=None, completion_proven=False,
        task_acceptance_proven=False, full_effect_state_known=False,
        change_state_scope='current dispatch/identity/graph projection only; opaque runtime/world/resource changes are not proved')
    if 'legacy_result' in receipt:
        result['data']['legacy_result'] = copy.deepcopy(receipt['legacy_result'])
    result['changes'] = [] if not attempted else [dict(kind='pcg_actor_smoke_test', attempted=True,
        changed=True if receipt.get('observed_changed') is True else None, change_state_known=False,
        scope='SDK entry and observed label delta; actual PCG effect unknown')]
    result['retry'].update(automatic_retry=False, duplicate_risk='inspect_actual_dispatch_prefix_no_replay' if attempted or not known else 'none_observed')
    result['stage'] = receipt.get('stage', result['stage'])


def _run(result, values, callback):
    global _ACTIVE
    if _ACTIVE:
        raise OperationError('pcg_actor_smoke_reentry', 'Same workflow reentry refused before SDK work')
    _ACTIVE = True
    try:
        result['compile'].update(requested=False, attempted=False, compiled=None, status='not_requested')
        result['save'].update(requested=False, attempted=False, saved=False)
        result['data'].update(public_method='pcg_actor_smoke_test', requested_arguments=copy.deepcopy(values),
            parent_journal_only=True, original_native_command=False, automatic_retry=False,
            change_state_known=True, mutation_state_unknown=False, execution_mutation_attempted=False, mutation_performed=False,
            task_acceptance_proven=False, completion_proven=False, full_effect_state_known=False)
        audit_result = state.new_result(result['operation_id']); failure = None
        def audit_checkpoint(current):
            result['data']['audit_receipt'] = copy.deepcopy(current)
            result['stage'] = 'audit_pcg_graph_contract'
            if callback:
                callback(result)
        try:
            contracts.audit_pcg_graph_contract_action(audit_result, values['graph_path'], include_dependencies=True,
                forbidden_dependency_prefixes=[], fail_on_forbidden_dependencies=False, checkpoint_callback=audit_checkpoint)
        except Exception as error:
            failure = error
        result['data']['audit_receipt'] = copy.deepcopy(audit_result)
        audit = copy.deepcopy(audit_result['data'].get('legacy_result', {}))
        if failure is not None or audit.get('success') is not True:
            child_data = audit_result['data']
            unknown = (child_data.get('mutation_state_unknown') is True or child_data.get('outcome_unknown') is True
                or child_data.get('change_state_known') is False or failure is not None)
            result['data'].update(legacy_result=dict(success=False, error='PCG graph audit failed before actor smoke mutation',
                error_code='pcg_actor_smoke_preflight_failed', audit=audit, failed_stage='audit_pcg_graph_contract'),
                mutation_state_unknown=unknown, change_state_known=not unknown, dispatch_boundary_known=not unknown,
                post_failure_live_observation_skipped=unknown, do_not_retry=unknown)
            result['retry'].update(automatic_retry=False, duplicate_risk='audit_state_unknown_no_replay' if unknown else 'none_observed')
            raise OperationError('pcg_actor_smoke_preflight_failed', 'PCG graph audit failed before actor smoke mutation') from failure
        receipt = {}
        def checkpoint(current):
            _adopt(result, current)
            if callback:
                callback(result)
        try:
            api.execute(values, audit, receipt, checkpoint)
        finally:
            _adopt(result, receipt)
        if receipt.get('error_code'):
            raise OperationError(receipt['error_code'], receipt.get('error', 'PCG actor smoke failed'))
        if receipt.get('change_state_known') is not True or receipt.get('legacy_result', {}).get('success') is not True:
            raise OperationError('pcg_actor_smoke_result_unverified', 'Actual partial dispatch state retained; no replay')
        result['validation'].update(status='current_scoped_dispatch_and_selection', runtime_completion='not_proved', full_effect_state='not_proved')
        result.update(success=True, stage='complete')
        if callback:
            callback(result)
        return result
    finally:
        _ACTIVE = False


def pcg_actor_smoke_test(graph_path, actor_label='', selected_only=False, cleanup=True, generate=True, dry_run=True):
    try:
        values = _arguments(dict(graph_path=graph_path, actor_label=actor_label, selected_only=selected_only, cleanup=cleanup, generate=generate, dry_run=dry_run))
    except Exception as error:
        return state.fail(state.new_result(), error)
    return state.mutate('pcg_actor_smoke_test', values, None, lambda result: _run(result, values, state.checkpoint))


def pcg_actor_smoke_test_action(result, graph_path, actor_label='', selected_only=False, cleanup=True, generate=True, dry_run=True, checkpoint_callback=None):
    values = _arguments(dict(graph_path=graph_path, actor_label=actor_label, selected_only=selected_only, cleanup=cleanup, generate=generate, dry_run=dry_run))
    return _run(result, values, checkpoint_callback)
