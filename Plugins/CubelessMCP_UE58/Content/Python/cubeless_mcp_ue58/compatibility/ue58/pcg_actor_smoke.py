"""QU source smoke selection and exact C++ noarg compatibility; no transport."""
import copy
import json
import unreal
from . import pcg, pcg_utilities as utilities
from ...operations.state import OperationError


def _package(value):
    text = str(value).strip()
    if text.startswith('/Game/') and '.' in text:
        text = text.split('.', 1)[0]
    return text.rstrip('/')


def execute(values, audit, receipt, checkpoint=None):
    r = receipt
    r.update(stage='smoke_preflight', mutation_attempted=False, change_state_known=True,
        observed_changed=False, runtime_change_observed=None, actual_call_prefix=[], lazy_actor_labels_created=0,
        save_requested=False, save_attempted=False, saved=False, SDK_global_purity_unverified=False,
        completion_proven=False, task_acceptance_proven=False, full_effect_state_known=False,
        noarg_dispatch_prefix=[], matches=[], processed=[], errors=[], context_lost=False)
    ctx = None; graph = None; baseline = None; bindings = []; held = []; retained = []

    def stop(error):
        if ctx is not None:
            ctx.lost = True
        r.update(change_state_known=False, context_lost=True, post_failure_live_observation_skipped=True, do_not_retry=True)
        return error

    def observation():
        try:
            ctx.admit()
            result = dict(snapshot=pcg.snapshot(graph), file=pcg.file_state(package), dirty=package in pcg.dirty_packages())
            ctx.admit(); return result
        except Exception as error:
            r.setdefault('first_observation_failure', dict(error=str(error), code=getattr(error, 'code', ''), observation=copy.deepcopy(getattr(error, 'observation', None))))
            raise stop(error)

    def get_graph(component):
        function = getattr(component, 'get_graph', None)
        if not callable(function):
            return None
        try:
            return function()
        finally:
            function = None

    def current(check_source=False):
        try:
            ctx.admit()
            for component, expected in bindings:
                actual = get_graph(component)
                ctx.admit()
                if actual != expected:
                    raise OperationError('pcg_actor_smoke_graph_binding_changed', 'Selected component effective top Graph changed; no next operation')
            if check_source and baseline is not None:
                after = observation(); r['source_after'] = copy.deepcopy(after)
                if after != baseline:
                    raise OperationError('pcg_actor_smoke_source_changed', 'Captured graph identity/topology/file/dirty changed; no next operation')
                r['captured_source_preserved'] = True
            ctx.admit()
        except Exception as error:
            raise stop(error)

    def publish(stage):
        try:
            ctx.publish(stage)
        except Exception as error:
            raise stop(error)

    def pin(obj, owner=None, require_world=False):
        try:
            return ctx.pin(obj, owner, require_world)
        except Exception as error:
            raise stop(error)

    def call(stage, function, writer=False, returned=None):
        current()
        if writer:
            ctx.writer_admit(); current(True); publish('before_pcg_actor_smoke_checkpoint'); current(True)
        if len(r['actual_call_prefix']) >= 262144:
            raise OperationError('pcg_actor_smoke_budget', 'Existing bounded utility call budget exhausted')
        row = dict(stage=stage, entered=True, returned=False, explicit_mutation=writer)
        r['actual_call_prefix'].append(row); r['SDK_global_purity_unverified'] = True
        r['mutation_attempted'] |= writer
        try:
            value = function(); row['returned'] = True
            if returned:
                returned(value)
        except Exception as error:
            row.update(raised=True, error=str(error)); raise stop(error)
        finally:
            function = None
        current(writer)
        if writer:
            publish('returned_pcg_actor_smoke_operation'); current(True)
        return value

    def legacy():
        if values['dry_run']:
            return dict(success=True, dry_run=True, graph_path=package,
                matching_actor_count=len(r['matches']), matching_component_count=sum(len(x['components']) for x in r['matches']),
                matches=copy.deepcopy(r['matches']), audit=copy.deepcopy(audit),
                note='dry_run=True; no PCG component cleanup/generate was executed',
                mutation_attempted=False, mutation_performed=False, changed=False)
        return dict(success=not r['errors'], dry_run=False, graph_path=package,
            processed_count=len(r['processed']), processed=copy.deepcopy(r['processed']), errors=copy.deepcopy(r['errors']), audit=copy.deepcopy(audit))

    try:
        package = utilities._name(_package(values['graph_path']), 'graph package path')
        utilities._name(values['graph_path'], 'graph_path')
        ctx = utilities._Context(r, checkpoint)
        r['graph_selection_adapter'] = 'UE58 public component.get_graph() effective top graph; no raw GraphInstance equivalence or protected property access'
        graph = call('LoadGraph', lambda: unreal.EditorAssetLibrary.load_asset(values['graph_path']), returned=lambda obj: held.append(obj))
        if graph is None and values['graph_path'] != package:
            graph = call('LoadGraphPackage', lambda: unreal.EditorAssetLibrary.load_asset(package), returned=lambda obj: held.append(obj))
        if graph is None:
            raise OperationError('pcg_actor_smoke_graph_unavailable', 'Could not load PCG graph: ' + values['graph_path'])
        retained.append(pin(graph)); baseline = observation(); r['source_before'] = copy.deepcopy(baseline)
        subsystem = call('GetEditorActorSubsystem', lambda: unreal.get_editor_subsystem(unreal.EditorActorSubsystem), returned=lambda obj: held.append(obj))
        retained.append(pin(subsystem))
        actors = list(call('GetSelectedLevelActors' if values['selected_only'] else 'GetAllLevelActors', lambda: subsystem.get_selected_level_actors() if values['selected_only'] else subsystem.get_all_level_actors()))
        if len(actors) > 262144:
            raise OperationError('pcg_actor_smoke_budget', 'Actor result exceeds the existing bounded utility observation budget')
        selected = []
        for actor in actors:
            ctx.targets = list(retained)
            anchor = pin(actor, require_world=True)
            previous = call('GetActorLabel_NoCreate', lambda: actor.get_actor_label(False))
            def label_returned(label):
                anchor.label = label
                if not previous and label:
                    r['observed_changed'] = True; r['lazy_actor_labels_created'] += 1
            label = call('GetActorLabel', lambda: actor.get_actor_label(), writer=not bool(previous), returned=label_returned)
            path = anchor.shape[0]
            if values['actor_label'] and values['actor_label'] not in label and values['actor_label'] not in path:
                continue
            rows = []
            components = list(call('GetActorComponents', lambda: actor.get_components_by_class(unreal.ActorComponent)))
            for component in components:
                ctx.targets = list(retained) + [anchor]
                component_anchor = pin(component, actor, True)
                class_name = call('GetComponentClassName', lambda: component_anchor.shape[1].get_name())
                if 'PCG' not in class_name:
                    continue
                source = call('GetComponentTopGraph', lambda: get_graph(component), returned=lambda obj: held.append(obj))
                if source is None:
                    continue
                source_anchor = pin(source)
                source_path = call('GetComponentTopGraphPath', lambda: source.get_path_name())
                if _package(source_path) != package:
                    continue
                for target in (anchor, component_anchor, source_anchor):
                    if target not in retained:
                        retained.append(target)
                ctx.targets = list(retained)
                bindings.append((component, source)); current()
                row = dict(component=call('GetComponentName', lambda: component.get_name()), component_class=class_name, component_path=component_anchor.shape[0], graph_matches=True)
                rows.append(row); selected.append((actor, component, label, row))
            if rows:
                r['matches'].append(dict(actor=label, actor_path=path, components=rows))
        ctx.targets = list(retained)
        r['legacy_result'] = legacy()
        current(True); publish('returned_pcg_actor_smoke_selection'); current(True)
        if values['dry_run']:
            r['operation_complete'] = True
            return r
        if not selected:
            raise OperationError('pcg_actor_smoke_no_matches', 'No existing PCG component in the current level references graph: ' + package)
        for actor, component, label, component_row in selected:
            processed = dict(actor=label, component=component_row['component'], called_methods=[])
            r['processed'].append(processed)
            for name in (['cleanup'] if values['cleanup'] else []) + (['generate'] if values['generate'] else []):
                current(True); ctx.writer_admit(); publish('before_pcg_actor_smoke_checkpoint'); current(True)
                function = getattr(getattr(unreal, 'CubelessPCGComponentGenerationLibrary', None), 'invoke_pcg_component_noarg', None)
                if not callable(function):
                    raise OperationError('pcg_actor_smoke_native_noarg_unavailable', 'Exact public C++ noarg compatibility binding is unavailable; no fallback writer')
                row = dict(actor=label, component_path=component_row['component_path'], method=name, entered=True, returned=False)
                r['noarg_dispatch_prefix'].append(row); r['mutation_attempted'] = True
                try:
                    try:
                        raw = function(component, name)
                    finally:
                        function = None
                    row['raw'] = raw
                    value = json.loads(raw); row['receipt'] = copy.deepcopy(value)
                    if type(value) is not dict:
                        raise OperationError('pcg_actor_smoke_native_receipt_invalid', 'Exact noarg receipt object required')
                    row['returned'] = value.get('dispatch_returned') is True
                    if row['returned']:
                        processed['called_methods'].append(name)
                    if (value.get('success') is not True or value.get('continuation_safe') is not True
                            or value.get('dispatch_entered') is not True or not row['returned']
                            or value.get('target_alive_after_return') is not True or value.get('target_alive_observed_after_return') is not True
                            or value.get('completion_proven') is not False or value.get('task_acceptance_proven') is not False or value.get('full_effect_state_known') is not False):
                        raise OperationError(value.get('error_code') or 'pcg_actor_smoke_noarg_unknown', value.get('error') or 'Noarg return lacks a safe exact continuation boundary')
                except Exception as error:
                    r['errors'].append(dict(actor=label, component=component_row['component'], method=name, error=str(error)))
                    r['legacy_result'] = legacy(); raise stop(error)
                current(True); r['legacy_result'] = legacy()
                publish('returned_pcg_actor_smoke_operation'); current(True)
        r['legacy_result'] = legacy(); r['operation_complete'] = True
        return r
    except Exception as error:
        r.update(error_code=getattr(error, 'code', 'pcg_actor_smoke_failed'), error=str(error))
        if 'package' in locals():
            r['legacy_result'] = legacy()
            r['legacy_result'].update(success=False, error=str(error), error_code=r['error_code'])
        r['do_not_retry'] = r.get('do_not_retry', False) or r['mutation_attempted'] or not r['change_state_known']
        return r
