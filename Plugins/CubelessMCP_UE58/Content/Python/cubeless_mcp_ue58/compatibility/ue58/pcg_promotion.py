"""PR public Editor duplicate/save leaf; reuse HF context and native JSON topology.

Snapshots protect captured identity/topology/disk, not every authored setting or
PostDuplicate/PreSave cache. No Python reflected pin/edge struct traversal.
"""
import copy
import json
import unreal
from . import pcg, pcg_utilities as utilities
from ...operations.state import OperationError


def _same(left, right):
    return json.dumps(left, sort_keys=True, allow_nan=False) == json.dumps(right, sort_keys=True, allow_nan=False)


def _package(value):
    text = str(value).strip()
    if text.startswith('/Game/') and '.' in text:
        text = text.split('.', 1)[0]
    return text.rstrip('/')


def _name(value, field):
    text = utilities._name(str(value), field)
    if not text.startswith('/Game/') or '..' in text or '\\' in text or not text.rpartition('/')[2]:
        raise OperationError('pcg_promotion_path_invalid', field + ' requires a bounded /Game package')
    return text


def execute(values, receipt, checkpoint=None):
    """Source validation has already completed; never run a second journal."""
    r = receipt
    r.update(stage='promotion_preflight', mutation_attempted=False, change_state_known=True, observed_changed=False,
             save_requested=not values['dry_run'], save_attempted=False, saved=False,
             SDK_global_purity_unverified=False, actual_call_prefix=[], context_lost=False,
             created_new_target=False, candidate_created_in_memory=False, operation_complete=False,
             source_observation_scope='held graph identity; native JSON owned identity/topology/labels/types and source file/dirty; general setting values/editor caches are not exhaustive')
    ctx = None
    held = {'source': None, 'target': None}
    source_expected = target_expected = None

    def stop(error):
        if ctx is not None:
            ctx.lost = True
        r.update(change_state_known=False, context_lost=True, post_failure_live_observation_skipped=True, do_not_retry=True)
        return error

    def observation(obj, package):
        try:
            ctx.admit()
            snapshot = pcg.snapshot(obj)
            ctx.admit()
            file = pcg.file_state(package)
            dirty = package in pcg.dirty_packages()
            ctx.admit()
            return dict(snapshot=snapshot, file=file, dirty=dirty)
        except Exception as error:
            r.setdefault('first_observation_failure', dict(package=package, error=str(error),
                code=getattr(error, 'code', ''), observation=copy.deepcopy(getattr(error, 'observation', None))))
            raise stop(error)

    def current(check_target=True):
        try:
            ctx.admit()
            if source_expected is not None:
                actual = observation(held['source'], source_package)
                r['source_after'] = copy.deepcopy(actual)
                if not _same(actual, source_expected):
                    raise OperationError('pcg_promotion_source_changed', 'Captured source identity/topology/file/dirty changed; no next SDK')
                r['source_preserved'] = True
            if check_target and target_expected is not None:
                actual = observation(held['target'], target_package)
                if not _same(actual['snapshot'], target_expected['snapshot']):
                    raise OperationError('pcg_promotion_target_changed', 'Captured new target identity/topology changed; no next SDK')
                r['target_after'] = copy.deepcopy(actual)
            ctx.admit()
        except Exception as error:
            raise stop(error)

    def absent():
        state = pcg.target_state(target_package)
        r['target_current_before_duplicate'] = copy.deepcopy(state)
        if pcg.exists(state):
            raise OperationError('pcg_promotion_target_exists', 'Fresh target became present; no overwrite/delete/replay')

    def sdk(stage, function, *, writer=False, save=False, fresh=False, returned=None):
        ctx.admit()
        if writer or save:
            ctx.writer_admit()
        row = dict(stage=stage, entered=False, returned=False, explicit_mutation=writer, explicit_save=save)
        r['actual_call_prefix'].append(row)
        if writer or save:
            try:
                ctx.publish('before_pcg_promotion_checkpoint')
            except Exception as error:
                raise stop(error)
        # HF call intentionally is not used here: checkpoint callbacks must be
        # followed by current/absence fences before the next writer enters.
        ctx.admit()
        current()
        if fresh:
            absent()
            current()
        row['entered'] = True
        r['SDK_global_purity_unverified'] = True
        r['mutation_attempted'] |= writer
        r['save_attempted'] |= save
        try:
            value = function()
            row['returned'] = True
            if returned:
                returned(value)  # Holds clone/records save result before getters.
        except Exception as error:
            row.update(raised=True, error=str(error))
            raise stop(error)  # No post-error SDK/current/cleanup observation.
        current()
        if writer or save:
            try:
                ctx.publish('returned_pcg_promotion_operation')
            except Exception as error:
                raise stop(error)
            current()
        return value

    try:
        source_package = _name(_package(values['source_graph_path']), 'source_graph_path')
        target_package = _name(_package(values['target_graph_path']), 'target_graph_path')
        if not values['allow_non_temp_source'] and not source_package.startswith('/Game/_MCP_Temp/'):
            raise OperationError('pcg_promotion_non_temp_source', 'Refusing non-temp source without allow_non_temp_source=True')
        if target_package.startswith('/Game/_MCP_Temp/'):
            raise OperationError('pcg_promotion_temp_target', 'Promotion target must not be under /Game/_MCP_Temp/')
        r.update(source_graph_path=source_package, target_graph_path=target_package)
        ctx = utilities._Context(r, checkpoint)
        # Existing native audit is loaded-only. Never reload/rebind a source
        # which vanished after that validation or turn this into a loader route.
        path = source_package + '.' + source_package.rsplit('/', 1)[-1]
        held['source'] = unreal.find_object(None, path)
        ctx.pin(held['source'])
        if held['source'].get_class().get_path_name() != '/Script/PCG.PCGGraph' or held['source'].get_path_name().casefold() != path.casefold():
            raise OperationError('pcg_promotion_source_class', 'Exact already-loaded native PCGGraph required')
        source_package = held['source'].get_package().get_path_name()
        r['source_graph_path'] = source_package
        source_expected = observation(held['source'], source_package)
        r['source_before'] = copy.deepcopy(source_expected)
        current()
        target_before = pcg.target_state(target_package)
        current()
        target_exists = pcg.exists(target_before)
        blocked = ('Live overwrite is disabled; remove or rename target manually: ' if values['overwrite_existing'] else 'Target graph already exists: ') + target_package if target_exists else ''
        leaf = dict(success=True, dry_run=values['dry_run'], source_graph_path=source_package, target_graph_path=target_package,
                    target_exists=target_exists, overwrite_existing_requested=values['overwrite_existing'], can_promote=not target_exists,
                    blocked_reason=blocked, would_create_new_target=not target_exists, would_overwrite=False,
                    mutation_attempted=False, mutation_performed=False, changed=False, validation=copy.deepcopy(values['validation']))
        r['legacy_result'] = leaf
        if values['dry_run']:
            current()
            r['operation_complete'] = True
            return r
        if target_exists:
            leaf.update(success=False, error_code='pcg_promotion_target_exists', message=blocked)
            raise OperationError(leaf['error_code'], blocked)
        # Public duplicate can invoke automatic checkout/save paths. Reuse the
        # existing explicit source-control/autosave policy; never change settings.
        r['creation_policy'] = pcg.blueprint_duplicates.policy()
        current()
        parent = target_package.rpartition('/')[0]
        sdk('MakeDirectory', lambda: unreal.EditorAssetLibrary.make_directory(parent), writer=True, fresh=True)

        def duplicate_returned(value):
            nonlocal target_expected
            held['target'] = value
            r.update(duplicate_returned=True, duplicate_returned_nonnull=value is not None)
            if value is None:
                raise OperationError('pcg_promotion_duplicate_failed', 'Duplicate returned null; actual SDK prefix retained')
            r.update(created_new_target=True, candidate_created_in_memory=True, observed_changed=True)
            r['legacy_result'].update(created_new_target=True, candidate_created_in_memory=True,
                                      mutation_attempted=True, mutation_performed=True, changed=True, saved=False)
            ctx.pin(value)
            actual_path = value.get_path_name()
            r['object_path'] = actual_path
            expected_path = target_package + '.' + target_package.rsplit('/', 1)[-1]
            if actual_path.casefold() != expected_path.casefold() or value.get_class().get_path_name() != '/Script/PCG.PCGGraph' or value == held['source']:
                raise OperationError('pcg_promotion_duplicate_identity', 'Returned clone differs from exact fresh target')
            # Freeze the returned clone before source checks or checkpoint
            # publication; a callback must not become a new accepted baseline.
            target_expected = observation(value, target_package)
            r['target_before_save'] = copy.deepcopy(target_expected)

        sdk('DuplicateAsset', lambda: unreal.EditorAssetLibrary.duplicate_asset(source_package, target_package), writer=True, fresh=True, returned=duplicate_returned)
        current()

        def save_returned(value):
            r.update(save_returned=True, save_return_value=value if type(value) is bool else None, saved=value is True)
            if value is not True:
                # Original false/exception contract is unknown and created in
                # memory. Do not run even source/target getters after this.
                r['legacy_result'] = dict(success=False, error_code='pcg_promotion_save_failed',
                    message='PCG candidate was duplicated in memory but could not be saved', dry_run=False,
                    source_graph_path=source_package, target_graph_path=target_package, object_path=r.get('object_path'),
                    created_new_target=True, candidate_created_in_memory=True, persisted=False, saved=False,
                    save_error='save_loaded_asset returned false' if value is False else 'save_loaded_asset returned a non-Boolean result', mutation_attempted=True,
                    mutation_performed=True, partial_target_modified=True, partial_mutation=True,
                    failed_stage='save_promoted_pcg_graph', failed_stage_effect='mutation', failed_stage_mutation_performed=True,
                    mutation_state_unknown=True, do_not_retry=True, workflow_recovery_required=True,
                    recovery_guidance='Resolve package write access, then save or remove the unsaved candidate manually before retrying.', validation=copy.deepcopy(values['validation']))
                raise OperationError('pcg_promotion_save_failed', r['legacy_result']['save_error'])

        sdk('SaveLoadedAsset', lambda: unreal.EditorAssetLibrary.save_loaded_asset(held['target']), save=True, returned=save_returned)
        r['legacy_result'] = dict(success=True, dry_run=False, source_graph_path=source_package, target_graph_path=target_package,
            object_path=r['object_path'], created_new_target=True, saved=True, mutation_attempted=True, mutation_performed=True,
            verification=dict(status='passed', method='duplicate_and_save_result'), validation=copy.deepcopy(values['validation']))
        r.update(operation_complete=True, change_state_known=True)
    except Exception as error:
        if r.get('save_attempted') and not r.get('save_returned') and r.get('created_new_target'):
            # Save exception follows the same legacy partial-created contract.
            r['legacy_result'] = dict(success=False, error_code='pcg_promotion_save_failed',
                message='PCG candidate was duplicated in memory but could not be saved', dry_run=False, save_error=str(error),
                source_graph_path=r.get('source_graph_path'), target_graph_path=r.get('target_graph_path'), object_path=r.get('object_path'),
                created_new_target=True, candidate_created_in_memory=True, persisted=False, saved=False,
                mutation_attempted=True, mutation_performed=True, partial_target_modified=True, partial_mutation=True,
                failed_stage='save_promoted_pcg_graph', failed_stage_effect='mutation', failed_stage_mutation_performed=True,
                mutation_state_unknown=True, do_not_retry=True, workflow_recovery_required=True,
                recovery_guidance='Resolve package write access, then save or remove the unsaved candidate manually before retrying.', validation=copy.deepcopy(values['validation']))
        code = 'pcg_promotion_save_failed' if r.get('save_attempted') and not r.get('save_returned') else getattr(error, 'code', 'pcg_promotion_sdk_failed')
        r.update(error_code=code, error=str(error),
                 do_not_retry=r['mutation_attempted'] or r['save_attempted'] or not r['change_state_known'])
        r.setdefault('legacy_result', dict(success=False, error=str(error), error_code=r['error_code']))
        if r['legacy_result'].get('success') is True:
            r['legacy_result'].update(success=False, error_code=code, message=str(error),
                                      saved=r['saved'], mutation_attempted=r['mutation_attempted'],
                                      mutation_state_unknown=not r['change_state_known'])
    return r
