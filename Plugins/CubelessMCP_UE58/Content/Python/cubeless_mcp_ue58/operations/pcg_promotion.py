"""PR original Python validator3/promotion7; one parent journal, no RPC."""
import copy
from . import pcg_contract_authoring as contracts, state
from .state import OperationError
from ..compatibility.ue58 import pcg_promotion as api

METHODS = ('validate_pcg_source_independence', 'promote_pcg_temp_graph')
PUBLIC_FIELDS = {
    METHODS[0]: ('graph_path', 'forbidden_dependency_prefixes', 'allowed_dependency_prefixes'),
    METHODS[1]: ('source_graph_path', 'target_graph_path', 'forbidden_dependency_prefixes', 'allowed_dependency_prefixes',
                 'overwrite_existing', 'allow_non_temp_source', 'dry_run')}
DEFAULTS = {
    METHODS[0]: dict(forbidden_dependency_prefixes=None, allowed_dependency_prefixes=None),
    METHODS[1]: dict(forbidden_dependency_prefixes=None, allowed_dependency_prefixes=None,
                     overwrite_existing=False, allow_non_temp_source=False, dry_run=True)}
PARAM_FIELDS = PUBLIC_FIELDS
OBSERVERS = (METHODS[0],)
WRITERS = (METHODS[1],)
NO_JOURNAL_METHODS = OBSERVERS
_ACTIVE = False


def normalize_public_mcp_arguments(method, arguments):
    if method not in METHODS or type(arguments) is not dict:
        raise OperationError('pcg_promotion_arguments_invalid', 'Known original method and argument object required')
    values = copy.deepcopy(DEFAULTS[method])
    for name in PUBLIC_FIELDS[method]:
        if name not in arguments:
            if name not in values:
                raise OperationError('pcg_promotion_arguments_invalid', 'Required argument missing: '+name)
            continue
        value = copy.deepcopy(arguments[name])
        if name.endswith('_prefixes'):
            if value is not None and (not isinstance(value, (list, tuple)) or any(type(item) is not str for item in value)):
                raise OperationError('pcg_promotion_arguments_invalid', name+' requires a string list or null')
            value = list(value) if value is not None else None
        elif name in ('overwrite_existing', 'allow_non_temp_source', 'dry_run'):
            value = contracts._public_bool(name, value)
        elif type(value) is not str:
            raise OperationError('pcg_promotion_arguments_invalid', name+' requires a string')
        values[name] = value
    return values


def _prefixes(values):
    # None selects original defaults; [] explicitly disables that prefix list.
    values = copy.deepcopy(values)
    if values['forbidden_dependency_prefixes'] is None:
        values['forbidden_dependency_prefixes'] = ['/Game/Cubeless/PCG/PCGStudy']
    if values['allowed_dependency_prefixes'] is None:
        values['allowed_dependency_prefixes'] = ['/Script/PCG', '/Game/DreamscapeSeries']
    return values


def _publish(result, callback, stage):
    result['stage'] = stage
    if callback:
        callback(result)


def _audit(result, values, callback):
    observed = state.new_result(result['operation_id'])
    failure = None

    def adopt(current, publish=True):
        data = current['data']
        result['data']['audit_receipt'] = {key: copy.deepcopy(value) for key, value in current.items() if key != 'data'}
        result['data']['audit_receipt']['data'] = {key: copy.deepcopy(value) for key, value in data.items() if key != 'legacy_result'}
        result['data'].update(change_state_known=data.get('change_state_known') is True,
                              mutation_state_unknown=data.get('mutation_state_unknown') is True)
        if publish:
            _publish(result, callback, 'returned_pcg_promotion_operation' if current['stage'].startswith('returned_') else 'before_pcg_promotion_checkpoint')

    try:
        contracts.audit_pcg_graph_contract_action(observed, values.get('graph_path', values.get('source_graph_path')),
            include_dependencies=True, forbidden_dependency_prefixes=values['forbidden_dependency_prefixes'],
            fail_on_forbidden_dependencies=True, checkpoint_callback=adopt)
    except Exception as error:
        failure = error
    adopt(observed, publish=False)
    leaf = copy.deepcopy(observed['data'].get('legacy_result', {}))
    if type(leaf.get('success')) is not bool:
        leaf = dict(success=False, error=str(failure or 'Original audit result missing'),
                    error_code=getattr(failure, 'code', 'pcg_promotion_audit_invalid'))
    return observed, leaf, failure


def _validate(audit, values):
    if not audit.get('success'):
        return audit
    dependencies = audit.get('dependencies', [])
    hits = audit.get('forbidden_dependency_hits', [])
    return dict(success=not hits, graph_path=audit.get('graph_path', values.get('graph_path', values.get('source_graph_path'))),
                node_count=audit.get('node_count', 0), edge_count=audit.get('edge_count', 0), dependency_count=len(dependencies),
                dependencies=copy.deepcopy(dependencies), forbidden_dependency_prefixes=copy.deepcopy(values['forbidden_dependency_prefixes']),
                forbidden_dependency_hits=copy.deepcopy(hits), allowed_dependency_prefixes=copy.deepcopy(values['allowed_dependency_prefixes']),
                dependencies_outside_allowed_prefixes=[item for item in dependencies if not any(str(item).startswith(prefix) for prefix in values['allowed_dependency_prefixes'])],
                audit=copy.deepcopy(audit))


def _adopt(result, receipt):
    result['data']['current_Python_receipt'] = {key: copy.deepcopy(value) for key, value in receipt.items() if key != 'legacy_result'}
    if 'legacy_result' in receipt:
        result['data']['legacy_result'] = copy.deepcopy(receipt['legacy_result'])
    known = receipt.get('change_state_known') is True
    attempted = receipt.get('mutation_attempted') is True
    changed = receipt.get('observed_changed') is True
    result['data'].update(execution_mutation_attempted=attempted, change_state_known=known,
                          mutation_state_unknown=not known, mutation_performed=changed if known else None)
    result['save'].update(requested=receipt.get('save_requested') is True, attempted=receipt.get('save_attempted') is True,
                          saved=result['save'].get('saved') is True or receipt.get('saved') is True,
                          status='API_true' if receipt.get('saved') else 'unknown_or_failed' if receipt.get('save_attempted') else 'not_attempted')
    result['changes'] = [] if known and not attempted and not changed else [dict(kind=METHODS[1], attempted=attempted,
                          changed=changed if known else None, change_state_known=known)]
    result['retry'].update(automatic_retry=False, duplicate_risk='unknown_scope_no_replay' if not known else 'inspect_created_target_before_new_request' if attempted else 'none_observed')


def _run(result, method, values, callback):
    global _ACTIVE
    if _ACTIVE:
        raise OperationError('pcg_promotion_reentry', 'Same-domain reentry refused before SDK work')
    _ACTIVE = True
    try:
        result['compile'].update(requested=False, attempted=False, compiled=False, status='not_requested')
        result['save'].update(requested=False, attempted=False, saved=False, status='not_attempted')
        result['data'].update(public_method=method, requested_arguments=copy.deepcopy(values), original_Native_command=False,
                              parent_journal_only=True, execution_mutation_attempted=False, mutation_performed=False,
                              change_state_known=True, mutation_state_unknown=False, operation_complete=False,
                              allowed_prefix_misses_are_diagnostic=True, full_original_pipeline_complete=False)
        values = _prefixes(values)
        # Original validator runs before promotion temp/target admission.
        observed, audit, failure = _audit(result, values, callback)
        leaf = _validate(audit, values)
        result['data']['legacy_result'] = copy.deepcopy(leaf)
        if failure is not None or observed.get('success') is not True or not result['data']['change_state_known']:
            if method == METHODS[1]:
                result['data']['legacy_result'] = dict(success=False, error_code='pcg_source_independence_validation_failed',
                    message='Source graph failed source-independence validation', validation=leaf,
                    mutation_attempted=False, mutation_performed=False,
                    mutation_state_unknown=result['data']['mutation_state_unknown'])
                raise OperationError('pcg_source_independence_validation_failed', str(failure or 'Source validation is incomplete'))
            raise failure or OperationError('pcg_graph_contract_audit_failed', 'Source audit failed or is unknown')
        if not leaf['success']:
            raise OperationError('pcg_graph_contract_audit_failed', 'Forbidden source dependencies found')
        if method == METHODS[1]:
            receipt = {}
            values = dict(values, validation=leaf)

            def returned(current):
                _adopt(result, current)
                _publish(result, callback, current.get('stage', 'returned_pcg_promotion_operation'))

            try:
                api.execute(values, receipt, returned)
            finally:
                _adopt(result, receipt)
            if receipt.get('error_code'):
                raise OperationError(receipt['error_code'], receipt.get('error', 'Promotion failed; inspect actual prefix'))
            leaf = result['data'].get('legacy_result', {})
            if not result['data']['change_state_known']:
                raise OperationError('pcg_promotion_outcome_unknown', 'Actual promotion scope unknown; no replay')
        if leaf.get('success') is not True:
            raise OperationError(leaf.get('error_code', 'pcg_promotion_failed'), leaf.get('message', 'Original result failed'))
        result['data']['operation_complete'] = True
        result['validation'].update(status='captured_current_source_and_original_policy', dependency_scope='direct on-disk hard+soft package query',
                                     RAM_transitive_cook_independence='not_proved', runtime='not_run', disk_reload='not_run')
        result.update(success=True, stage='complete')
        return result
    finally:
        _ACTIVE = False


def _named(method, arguments):
    try:
        values = normalize_public_mcp_arguments(method, arguments)
    except Exception as error:
        return state.fail(state.new_result(), error)
    if method == METHODS[1] and not values['dry_run']:
        return state.mutate(method, values, None, lambda result: _run(result, method, values, state.checkpoint))
    result = state.new_result()
    result['data']['journal_created'] = False
    try:
        return _run(result, method, values, None)
    except Exception as error:
        return state.fail(result, error)


def _action(result, method, arguments, callback, legacy=None, public_input=False):
    values = normalize_public_mcp_arguments(method, legacy if legacy is not None else arguments)
    return _run(result, method, values, callback)


def validate_pcg_source_independence(graph_path, forbidden_dependency_prefixes=None, allowed_dependency_prefixes=None):
    return _named(METHODS[0], dict(graph_path=graph_path, forbidden_dependency_prefixes=forbidden_dependency_prefixes, allowed_dependency_prefixes=allowed_dependency_prefixes))


def validate_pcg_source_independence_action(result, graph_path, forbidden_dependency_prefixes=None, allowed_dependency_prefixes=None, checkpoint_callback=None, *, _legacy_command_params=None, _public_input=False):
    return _action(result, METHODS[0], dict(graph_path=graph_path, forbidden_dependency_prefixes=forbidden_dependency_prefixes, allowed_dependency_prefixes=allowed_dependency_prefixes), checkpoint_callback, _legacy_command_params, _public_input)


def promote_pcg_temp_graph(source_graph_path, target_graph_path, forbidden_dependency_prefixes=None, allowed_dependency_prefixes=None, overwrite_existing=False, allow_non_temp_source=False, dry_run=True):
    return _named(METHODS[1], dict(source_graph_path=source_graph_path, target_graph_path=target_graph_path, forbidden_dependency_prefixes=forbidden_dependency_prefixes, allowed_dependency_prefixes=allowed_dependency_prefixes, overwrite_existing=overwrite_existing, allow_non_temp_source=allow_non_temp_source, dry_run=dry_run))


def promote_pcg_temp_graph_action(result, source_graph_path, target_graph_path, forbidden_dependency_prefixes=None, allowed_dependency_prefixes=None, overwrite_existing=False, allow_non_temp_source=False, dry_run=True, checkpoint_callback=None, *, _legacy_command_params=None, _public_input=False):
    return _action(result, METHODS[1], dict(source_graph_path=source_graph_path, target_graph_path=target_graph_path, forbidden_dependency_prefixes=forbidden_dependency_prefixes, allowed_dependency_prefixes=allowed_dependency_prefixes, overwrite_existing=overwrite_existing, allow_non_temp_source=allow_non_temp_source, dry_run=dry_run), checkpoint_callback, _legacy_command_params, _public_input)
