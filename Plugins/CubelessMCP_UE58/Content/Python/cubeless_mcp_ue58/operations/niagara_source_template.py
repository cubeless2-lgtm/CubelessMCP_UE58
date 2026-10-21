"""PS original source-system duplicate/filter workflow; existing concrete HN wait.

One parent journal and one owned compile-status Job. No Engine writer, transport,
general task scheduler, shared compiler cancellation, automatic cleanup or retry.
"""
import copy
import hashlib
import json
import os
import time
import uuid
import unreal
from . import state, niagara_system_analysis as analysis, niagara_promotion as promotion
from . import niagara_emitter_configuration as emitters
from ..compatibility.ue58 import niagara_authoring_inspection as jobs
from ..compatibility.ue58 import niagara_emitter_configuration as emitter_api
from ..compatibility.ue58 import pcg_utilities as context_api, blueprint_duplicates
from ..registration.async_interop_toolset import CubelessPythonAsyncOperationResult
from ..registration.results import reflected_result

OperationError=state.OperationError
METHOD='create_niagara_system_from_source_template'
FIELDS=('target_system_path','source_system_path','source_emitter_indices','copy_source_user_parameters',
        'save','request_compile','wait_for_completion','timeout_seconds')
DEFAULTS=dict(source_emitter_indices=None,copy_source_user_parameters=True,save=True,request_compile=True,
              wait_for_completion=True,timeout_seconds=180)
_ACTIVE=False
_TICKER=None
_SHUTDOWN=None
_ENTRY=None
_RETAINED=None


def normalize_public_mcp_arguments(method,arguments):
    if method!=METHOD or type(arguments) is not dict:raise OperationError('niagara_template_arguments_invalid','Exact original source-template arguments required')
    out=copy.deepcopy(DEFAULTS)
    out.update({key:copy.deepcopy(arguments[key]) for key in FIELDS if key in arguments})
    for name in FIELDS[:2]:
        if type(out.get(name)) is not str:raise OperationError('niagara_template_arguments_invalid',name+' requires a string')
    indices=out['source_emitter_indices']
    if indices is not None:
        if not isinstance(indices,(list,tuple)):raise OperationError('niagara_template_arguments_invalid','source_emitter_indices requires a list or null')
        out['source_emitter_indices']=[analysis._public_int('source_emitter_indices',v) for v in indices]
    for name in ('copy_source_user_parameters','save','request_compile','wait_for_completion'):
        out[name]=analysis._public_bool(name,out[name])
    out['timeout_seconds']=analysis._public_int('timeout_seconds',out['timeout_seconds'])
    return out


def _indices(source,requested):
    # Original helper1432: source analysis precedes semantic index validation;
    # keep caller ordering/duplicates for the report and use a set only to prune.
    summary=source.get('summary') if type(source.get('summary')) is dict else {}
    count=source.get('emitter_count',summary.get('emitter_count'))
    if isinstance(count,bool):count=0
    elif isinstance(count,(int,float)):count=int(count)
    else:count=len(source['emitters']) if type(source.get('emitters')) is list else 0
    if count<=0:raise ValueError('Source Niagara system has no emitters')
    selected=list(range(count)) if requested is None else []
    if requested is not None:
        for value in requested:
            if isinstance(value,bool):raise ValueError('source_emitter_indices values must be integers, not bool: '+str(requested))
            selected.append(int(value))
    invalid=[i for i in selected if i<0 or i>=count]
    if invalid:raise ValueError(f'source_emitter_indices out of range 0..{count-1}: {invalid}')
    return count,selected


def _same(a,b):return json.dumps(a,sort_keys=True,allow_nan=False)==json.dumps(b,sort_keys=True,allow_nan=False)


def _leaf(result):
    data=result['data']
    return copy.deepcopy(data.get('legacy_Named_result',data.get('legacy_result',{})))


class _Flow:
    """Only this eight-input workflow's captured state and continuation."""
    def __init__(self,result,values,checkpoint):
        self.r=result;self.v=values;self.checkpoint=checkpoint
        self.ctx=None;self.source=None;self.source_state=None;self.source_file=None
        self.target=None;self.job=None;self.job_id=None;self.revision=None;self.cancel_entered=False
        self.count=0;self.selected=[];self.removed=[];self.create={};self.removals=[];self.compile={}
        self.last_stage='source_analysis';self.last_mutation='';self.in_flight=False
        self.path=values['target_system_path'];self.expected_handles=None
        self.lost=False;self.remove_plan=None
        self.cleanup_paths=set();self.untrusted_paths=set()
        try:self.cleanup_paths.add(promotion._normalize_game_package_path(values['target_system_path']))
        except Exception:
            if str(values['target_system_path'] or '').strip():self.untrusted_paths.add(str(values['target_system_path']).strip())
        result['data'].update(public_method=METHOD,requested_arguments=copy.deepcopy(values),parent_journal_only=True,
            child_journals_created=0,steps=[],change_state_known=True,mutation_state_unknown=False,
            execution_mutation_attempted=False,mutation_performed=False,operation_complete=False,
            source_observation_scope='HF held identity/current, existing emitter property inventory and source file; not exhaustive authored data or atomic callback purity')
        result['compile'].update(requested=False,attempted=False,compiled=False)
        result['save'].update(requested=False,attempted=False,saved=False)

    def unsafe(self,error):
        self.lost=True
        if self.ctx:self.ctx.lost=True
        self.r['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None,
            post_failure_live_observation_skipped=True,do_not_retry=True)
        return error

    def source_observation(self):
        try:
            self.ctx.admit()
            native=emitter_api.inspect_niagara_emitter_properties(dict(system_path=self.source.obj.get_path_name(),emitter_index=0,emitter_name=''))
            if native['success'] is not True or native['postcondition_passed'] is not True or native['change_state_known'] is not True:
                error=OperationError(native.get('error_code') or 'niagara_template_source_unknown',native.get('error') or 'Source property observation failed')
                error.observation=native;raise error
            value=native['data'].get('after',native['data'].get('before'))
            if type(value) is not dict or type(value.get('emitters')) is not list:raise OperationError('niagara_template_source_unknown','Existing emitter inventory missing')
            package=self.source.obj.get_package().get_path_name()
            disk=blueprint_duplicates.file_state(package) if package.startswith('/Game/') else None
            self.ctx.admit();return dict(properties=value,file=disk)
        except Exception as error:
            self.r['data'].setdefault('first_source_observation_failure',dict(error=str(error),observation=copy.deepcopy(getattr(error,'observation',None))))
            raise self.unsafe(error)

    def inventory(self,target):
        native=emitter_api.inspect_niagara_emitter_properties(dict(system_path=target.obj.get_path_name()))
        if any(native.get(k) is not True for k in ('success','postcondition_passed','change_state_known')):
            error=OperationError(native.get('error_code') or 'niagara_template_inventory_unknown',native.get('error') or 'Actual emitter inventory is unknown')
            error.observation=native;raise error
        value=native['data'].get('after',native['data'].get('before'))
        if type(value) is not dict or type(value.get('emitters')) is not list:raise OperationError('niagara_template_inventory_unknown','Actual emitter inventory missing')
        return {key:copy.deepcopy(value[key]) for key in ('system_path','package','emitters')}

    def current(self):
        try:
            if self.ctx:
                self.ctx.admit()
                if self.source_state is not None:
                    actual=self.source_observation()
                    if not _same(actual,self.source_state):raise OperationError('niagara_template_source_changed','Captured original source changed; no next stage')
                    self.r['data']['source_preserved']=True
                if self.target is not None and self.expected_handles is not None:
                    if not _same(self.inventory(self.target),self.expected_handles):raise OperationError('niagara_template_target_changed','Captured target emitter GUID/order/values changed outside the planned removal')
        except Exception as error:raise self.unsafe(error)

    def bind_target(self,leaf):
        if self.target is not None:return
        try:
            self.path=leaf.get('promoted_system_path') or leaf.get('target_system_path') or self.path
            path=self.path if '.' in self.path.rsplit('/',1)[-1] else self.path+'.'+self.path.rsplit('/',1)[-1]
            self.target=self.ctx.pin(unreal.find_object(None,path))
            self.expected_handles=self.inventory(self.target)
            if len(self.expected_handles['emitters'])!=self.count:raise OperationError('niagara_template_duplicate_count','Returned duplicate emitter count differs')
            self.r['data']['duplicated_handle_baseline']=copy.deepcopy(self.expected_handles)
        except Exception as error:raise self.unsafe(error)

    def publish(self):
        if self.checkpoint:
            try:self.checkpoint(self.r)
            except Exception as error:raise self.unsafe(error)
        self.current()  # Checkpoint callbacks cannot establish a new baseline.

    def compile_status(self):
        # RequestCompile return/idle terminal are not successful compilation.
        # Keep the authoritative tri-state value copied from child/Job receipts.
        value=self.r['compile'];compiled=value.get('compiled')
        if compiled is True:value['status']='native_compiled_true_observed'
        elif self.r['data'].get('compile_wait_terminal_observed') is True:
            value['status']='terminal_status_observed_compilation_unverified'
        elif value.get('attempted') is True:value['status']='compile_attempted_outcome_unverified'
        elif value.get('requested') is True:value['status']='compile_requested_not_attempted'
        else:value['status']='not_requested'

    def compile_wait_policy(self,native):
        legacy=native['data'].get('legacy_result')
        effective=legacy.get('timeout_seconds') if type(legacy) is dict else None
        self.r['data']['compile_wait_policy']=dict(requested_timeout_seconds=self.v['timeout_seconds'],
            effective_timeout_seconds=effective,deadline_owner='existing_HN_NativeJob',
            effective_timeout_source='actual_native_legacy_result',
            timeout_is_not_shared_compiler_cancellation=True)

    def merge(self,child,row):
        row['result']=copy.deepcopy(child)
        data=child['data'];ours=self.r['data']
        if row['stage']=='promote_niagara_system_candidate':
            paths,untrusted=promotion._collect_niagara_promotion_response_paths({'result':_leaf(child)},self.v['target_system_path'])
            self.cleanup_paths.update(paths);self.untrusted_paths.update(untrusted)
        attempted=data.get('execution_mutation_attempted',data.get('native_mutation_attempted'))
        ours['execution_mutation_attempted']|=attempted is True
        known=data.get('change_state_known') is True and not self.lost
        if data.get('mutation_state_unknown') is True or data.get('outcome_unknown') is True:known=False
        ours['change_state_known']=known
        ours['mutation_state_unknown']=not known
        if data.get('mutation_performed') is True:ours['observed_changed']=True
        ours['mutation_performed']=ours.get('observed_changed',False) if known else None
        for name in ('requested','attempted','saved'):
            self.r['save'][name]=self.r['save'].get(name) is True or child['save'].get(name) is True
        for name in ('requested','attempted'):
            self.r['compile'][name]=self.r['compile'].get(name) is True or child['compile'].get(name) is True
        if child['compile'].get('attempted'):self.r['compile']['compiled']=child['compile'].get('compiled')
        self.compile_status()
        self.r['changes']=[] if not ours['execution_mutation_attempted'] and known else [dict(kind=METHOD,
            attempted=ours['execution_mutation_attempted'],changed=ours['mutation_performed'],change_state_known=known)]

    def child(self,stage,action,arguments,mutation=False):
        self.last_stage=stage;self.r['stage']='before_niagara_source_template_checkpoint'
        row=dict(stage=stage,entered=False,returned=False,mutation_possible=mutation)
        self.r['data']['steps'].append(row)
        self.publish()
        child=state.new_result(self.r['operation_id']);child['data']['parent_journal_only']=True
        def checkpoint(observed):
            self.merge(observed,row);self.r['stage']='before_niagara_source_template_checkpoint' if observed['stage'].startswith('before_') else 'returned_niagara_source_template_operation'
            data=observed['data'];native=data.get('native_outcome')
            if observed['stage'].startswith('returned_') and (data.get('change_state_known') is not True
                    or type(native) is dict and (native.get('success') is not True or native.get('postcondition_passed') is not True)):
                error=OperationError((native or {}).get('error_code') or 'niagara_template_child_failed',
                    (native or {}).get('error') or 'Returned child failed/unknown; copied prefix retained before further SDK')
                if data.get('change_state_known') is not True:self.unsafe(error)
                raise error
            if observed['data'].get('native_result_returned') is True and observed['data'].get('change_state_known') is True:
                if stage=='promote_niagara_system_candidate' and _leaf(observed).get('promoted') is True:self.bind_target(_leaf(observed))
                elif self.remove_plan is not None and stage.startswith('remove_emitter_'):
                    body=observed['data'].get('native_result_data',{})
                    actual=body.get('after')
                    if type(actual) is not dict or not _same(actual.get('emitters'),self.remove_plan['emitters']):
                        raise self.unsafe(OperationError('niagara_template_remove_projection','Actual removed GUID/remaining indices differ from the copied plan'))
                    self.expected_handles=copy.deepcopy(self.remove_plan)
            self.publish()
        row['entered']=True
        if mutation:self.last_mutation=stage;self.in_flight=True
        failure=None
        try:action(child,**arguments,checkpoint_callback=checkpoint)
        except Exception as error:failure=error
        if failure is not None:state.fail(child,failure)
        self.merge(child,row);row['returned']=True
        if mutation and (child['data'].get('native_result_returned') is True or child['data'].get('native_dispatched') is False):self.in_flight=False
        # Preserve failed/unknown whole response before any live followup.
        if failure is not None or child.get('success') is not True:
            if child['data'].get('change_state_known') is not True:self.unsafe(failure or OperationError('niagara_template_child_unknown','Unknown child outcome'))
            raise failure or OperationError(child.get('error_code') or 'niagara_template_child_failed',child.get('error') or 'Existing child API failed')
        if self.r['data']['change_state_known'] is not True:raise self.unsafe(OperationError('niagara_template_child_unknown','Known child receipt required before next stage'))
        self.current();self.r['stage']='returned_niagara_source_template_operation';self.publish()
        return child,_leaf(child)

    def start(self,async_allowed):
        _,source=self.child('analyze_source',analysis.analyze_niagara_system_action,dict(system_path=self.v['source_system_path'],
            include_renderers=True,include_user_parameters=True,include_stack=False,include_graph=False,
            include_module_inputs=False,include_compile_status=False,include_scratch_pads=False))
        try:self.count,self.selected=_indices(source,self.v['source_emitter_indices'])
        except ValueError as error:
            self.r['data']['legacy_Named_result']=dict(success=False,error_code='niagara_invalid_source_emitter_indices',error=str(error)+': '+self.v['source_system_path'])
            raise OperationError('niagara_invalid_source_emitter_indices',str(error)) from error
        self.removed=[i for i in range(self.count) if i not in set(self.selected)]
        if self.v['wait_for_completion'] and not async_allowed:
            raise OperationError('niagara_template_sync_wait_unsupported','Original wait=True requires the official Named HN continuation; synchronous Direct/Batch has no pending wait. No duplicate was dispatched')
        self.ctx=context_api._Context(dict(actual_call_prefix=[],mutation_attempted=False,save_attempted=False),None)
        source_path=source.get('system_path',self.v['source_system_path'])
        if '.' not in source_path.rsplit('/',1)[-1]:source_path+='.'+source_path.rsplit('/',1)[-1]
        try:self.source=self.ctx.pin(unreal.find_object(None,source_path))
        except Exception as error:raise self.unsafe(error)
        self.source_state=self.source_observation();self.r['data']['source_before']=copy.deepcopy(self.source_state)
        _,self.create=self.child('promote_niagara_system_candidate',promotion.promote_niagara_system_candidate_action,
            dict(source_system_path=self.v['source_system_path'],target_system_path=self.path,dry_run=False,
                allow_non_temp_source=True,allow_temp_target=True,allow_temp_dependencies=False,
                save=bool(self.v['save'] and not self.removed),request_compile=bool(self.v['request_compile'] and not self.removed),
                timeout_seconds=self.v['timeout_seconds']),True)
        if not self.create.get('promoted'):
            self.r['data']['legacy_Named_result']=dict(success=False,error_code='niagara_promotion_blocked',error='Source system promotion did not create the target asset',promotion_result=self.create)
            raise OperationError('niagara_promotion_blocked','Source system promotion did not create the target asset')
        self.path=self.create.get('promoted_system_path') or self.create.get('target_system_path') or self.path
        self.create.setdefault('system_path',self.path)
        self.create.setdefault('dependency_audit',self.create.get('post_promotion_dependency_audit') or self.create.get('preflight_dependency_audit'))
        self.bind_target(self.create);self.current()
        for ordinal,index in enumerate(reversed(self.removed)):
            self.current()
            last=ordinal==len(self.removed)-1
            self.remove_plan=copy.deepcopy(self.expected_handles)
            removed=self.remove_plan['emitters'].pop(index)
            for physical,row in enumerate(self.remove_plan['emitters']):row['index']=physical
            self.r['data'].setdefault('removal_plans',[]).append(dict(source_index=index,handle_guid=removed['handle_guid'],remaining=copy.deepcopy(self.remove_plan['emitters'])))
            child,leaf=self.child('remove_emitter_'+str(index),emitters.remove_niagara_emitter_action,
                dict(system_path=self.path,emitter_index=index,allow_source_edit=False,
                     save=bool(self.v['save'] and last),request_compile=bool(self.v['request_compile'] and last)),True)
            # The existing Native writer captures physical handle/index before
            # removal and verifies remaining order. Retain its actual GUID plan.
            body=child['data'].get('native_result_data',{})
            self.r['data'].setdefault('removed_handle_receipts',[]).append(dict(source_index=index,
                before=copy.deepcopy(body.get('before')),after=copy.deepcopy(body.get('after'))))
            self.removals.append(leaf)
            self.remove_plan=None
        self.audit_final_save()
        if self.v['wait_for_completion']:
            self.last_stage='inspect_niagara_compile_status';self.last_mutation=self.last_stage if self.v['request_compile'] else self.last_mutation
            self.r['stage']='before_niagara_source_template_checkpoint';self.publish()
            params=dict(system_path=self.path,request_compile=bool(self.v['request_compile']),force=False,
                wait_for_completion=True,timeout_seconds=self.v['timeout_seconds'],poll_interval_seconds=0.1)
            self.r['data']['compile_job_start_entered']=True;self.in_flight=True
            try:self.job=jobs.start_job('inspect_niagara_compile_status',params)
            except Exception as error:
                if getattr(error,'native_start_returned',False):self.job=getattr(error,'native_returned_job',None)
                if getattr(error,'native_start_dispatched',None) is False:self.in_flight=False
                else:self.unsafe(error)
                raise
            self.r['data']['compile_job_start_returned']=True
            return False
        return self.finish()

    def audit_final_save(self):
        if self.create.get('on_disk_dependency_audit_deferred') is not True:return
        self.r['data']['initial_promotion_on_disk_audit_deferred']=True
        if not self.v['save']:
            self.r['data']['final_on_disk_dependency_audit_deferred']=True
            return
        if self.r['save'].get('saved') is not True:
            raise OperationError('niagara_template_final_save_unverified','Actual final save required before the deferred disk audit')
        _,leaf=self.child('audit_saved_generated',promotion.audit_niagara_promotion_candidate_action,
            dict(system_path=self.path,allow_temp_target=True,allow_temp_dependencies=False,
                 timeout_seconds=self.v['timeout_seconds']))
        # Keep the initial unsaved creation receipt byte-for-byte semantic data;
        # only this separate post-save read proves actual on-disk dependencies.
        self.r['data']['final_saved_dependency_audit']=copy.deepcopy(leaf)
        promotion._validate_niagara_promotion_dependency_audit(leaf.get('dependency_audit'),
            expected_asset_path=self.path,allow_temp_dependencies=False,
            forbidden_dependency_prefixes=[],forbidden_source_roots=[],
            audit_label='source-template final saved dependency audit',require_clean=True,require_complete=True)
        if (leaf.get('dependency_audit_complete') is not True
                or leaf.get('persistence_evidence_complete') is not True
                or leaf['dependency_audit'].get('saved_file_scanned') is not True):
            raise OperationError('niagara_template_final_disk_audit_incomplete','Actual saved-file dependency evidence required after final save')
        self.r['data'].update(final_saved_dependency_audit_verified=True,final_on_disk_dependency_audit_deferred=False)

    def poll(self):
        self.current()
        try:native=jobs.snapshot(self.job,'inspect_niagara_compile_status')
        except Exception as error:
            self.r['data'].update(undecoded_native_snapshot=getattr(error,'native_raw_result',None),
                decoded_native_snapshot=copy.deepcopy(getattr(error,'native_decoded_result',None)),
                unsafe_job_publication_quarantined=getattr(error,'native_unsafe_publication_quarantined',False))
            raise self.unsafe(error)
        data=native['data'];identity=data['native_job_id'];revision=data['native_checkpoint_revision']
        if self.job_id is not None and identity!=self.job_id:raise self.unsafe(OperationError('niagara_template_job_changed','Owned compile-status job identity changed'))
        if self.revision is not None and revision<self.revision:raise self.unsafe(OperationError('niagara_template_job_revision','Owned compile-status revision regressed'))
        self.job_id=identity
        self.r['data']['compile_status_receipt']=copy.deepcopy(native)
        for n,k in (('requested','compile_requested'),('attempted','compile_attempted')):self.r['compile'][n]|=native[k]
        self.r['compile']['compiled']=native['compiled']
        self.compile_wait_policy(native)
        self.compile_status()
        if data['native_job_terminal'] and (not native['success'] or not native['postcondition_passed'] or not native['change_state_known']):
            self.in_flight=False
            error=OperationError(native.get('error_code') or 'niagara_template_wait_failed',native.get('error') or 'Compile-status inspection failed')
            if not native['change_state_known']:self.unsafe(error)
            raise error
        if revision!=self.revision:
            self.revision=revision;self.r['stage']='returned_niagara_source_template_operation';self.publish()
        if not data['native_job_terminal']:return False
        self.in_flight=False
        self.compile=copy.deepcopy(data.get('legacy_result',{}))
        self.r['data']['compile_wait_terminal_observed']=True
        self.compile_status()
        return self.finish()

    def finish(self):
        _,generated=self.child('analyze_generated',analysis.analyze_niagara_system_action,dict(system_path=self.path,
            include_renderers=True,include_user_parameters=True,include_stack=True,include_graph=False,
            include_module_inputs=False,include_compile_status=True,include_scratch_pads=True))
        self.r['data']['legacy_Named_result']=dict(success=True,target_system_path=self.path,source_system_path=self.v['source_system_path'],
            source_emitter_count=self.count,copied_emitter_indices=self.selected,removed_source_emitter_indices=self.removed,
            copy_source_user_parameters=self.v['copy_source_user_parameters'],
            copy_source_user_parameters_note='Source User parameters are preserved by duplicating the whole source system; the flag is retained for API compatibility.',
            source_template_mode='source_system_duplicate',create_result=self.create,attach_results=[],remove_results=self.removals,
            compile_result=self.compile,generated_summary=generated.get('summary',{}),generated_user_parameters=generated.get('user_parameters',{}),
            write_scope='generated_temp_niagara_source_system_duplicate')
        if self.create.get('on_disk_dependency_audit_deferred') is True:
            self.r['data']['legacy_Named_result'].update(
                final_saved_dependency_audit=copy.deepcopy(self.r['data'].get('final_saved_dependency_audit')),
                final_saved_dependency_audit_verified=self.r['data'].get('final_saved_dependency_audit_verified') is True,
                on_disk_dependency_audit_deferred=self.r['data'].get('final_on_disk_dependency_audit_deferred') is True)
        self.r['data']['operation_complete']=True;self.r.update(success=True,stage='complete');return True

    def fail(self,error):
        state.fail(self.r,error)
        if self.r['data']['execution_mutation_attempted'] or self.last_mutation:
            # Original helper preserves conservative recovery even if a child
            # returned a narrower known prefix. Do not erase actual save history.
            self.r['data']['actual_child_change_state_known']=self.r['data']['change_state_known']
            self.r['data']['mutation_state_unknown']=True  # Original workflow failure also forces the existing Batch stop.
            policy='close_editor_then_verify_exact_disk_before_recovery' if self.in_flight else 'reload_then_offline_exact_cleanup_required'
            after='rehash_then_choose_exact_recovery' if self.in_flight else ''
            action=(f"Do not continue, retry, save, or reload '{self.v['target_system_path']}'. Close the editor and confirm the process has exited, then rehash the exact target. Reload only unchanged bytes; exactly clean/recreate the owned disposable target if its saved state changed or remains unknown." if self.in_flight else
                f"Do not continue or retry creation for '{self.v['target_system_path']}'. Reload that exact package if it exists, close the editor, and run exact-path offline cleanup before starting another mutation.")
            self.r['data']['legacy_Named_result']=dict(success=False,error_code='niagara_system_template_creation_state_unknown',
                error='Niagara source-template creation failed after mutation dispatch; inspect the actual stage receipts before recovery: '+str(error),
                mutation_state_unknown=True,save_state_unknown=True,do_not_retry=True,requires_package_reload=True,
                transport_state_unknown=self.in_flight,mutation_response_received=not self.in_flight,
                command_may_still_be_in_flight=self.in_flight,requires_editor_close_before_disk_verification=self.in_flight,
                do_not_reload_or_trust_disk_until_editor_closed=self.in_flight,cleanup_required_unknown=True,
                offline_exact_cleanup_required=bool(self.cleanup_paths),recreate_required=bool(self.cleanup_paths),exact_cleanup_only=bool(self.cleanup_paths),
                cleanup_candidate_paths=sorted(self.cleanup_paths),observed_untrusted_response_paths=sorted(self.untrusted_paths),
                target_system_path=str(self.v['target_system_path'] or ''),source_system_path=str(self.v['source_system_path'] or ''),
                failed_command=self.last_stage,last_mutating_command=self.last_mutation,recovery_policy=policy,
                post_editor_close_recovery_policy=after,recovery_action=action,exception_type=type(error).__name__,failure_detail=str(error),
                recovery_steps=['close_editor_and_confirm_process_exit','verify_exact_target_disk_hashes','choose_exact_recovery_from_post_close_hashes'] if self.in_flight else ['reload_exact_target','close_editor_and_confirm_process_exit','offline_exact_cleanup_before_retry'])
        else:self.r['data'].setdefault('legacy_Named_result',dict(success=False,error_code=getattr(error,'code','niagara_template_failed'),error=str(error)))
        self.r['data'].update(command_may_still_be_in_flight=self.in_flight,
            workflow_recovery_required=bool(self.last_mutation or self.r['data']['execution_mutation_attempted'] or self.lost),
            legacy_conservative_unknown_is_not_actual_saved_flag=True,local_HN_inflight_is_not_legacy_bridge_timeout=True)
        self.r['retry'].update(automatic_retry=False,duplicate_risk='inspect_partial_source_template_workflow_no_replay')
        return self.r


class _Journal:
    def __init__(self,values):
        self.result=state.new_result();self.path=state._journal_path(self.result['operation_id']);self.owned=False;self.final_entered=False
        if state._active_operation is not None:raise OperationError('mutation_busy','Another operation owns the project journal slot')
        record=dict(schema_version=1,operation_id=self.result['operation_id'],operation=METHOD,arguments=values,
            fingerprint=hashlib.sha256(json.dumps({'name':METHOD,'arguments':values},sort_keys=True).encode()).hexdigest(),
            state='started',result=self.result,engine_version=unreal.SystemLibrary.get_engine_version(),process_id=os.getpid())
        self.path.parent.mkdir(parents=True,exist_ok=True)
        with self.path.open('x',encoding='utf-8') as stream:
            stream.write(json.dumps(record,ensure_ascii=False,sort_keys=True));stream.flush();os.fsync(stream.fileno())
        state._active_operation=self.result['operation_id'];self.owned=True
    def checkpoint(self,result):state.checkpoint(result)
    def finish(self,recovery):
        if self.final_entered:raise OperationError('niagara_template_finalize_repeated','Finalization is never replayed')
        self.final_entered=True
        with self.path.open(encoding='utf-8') as stream:record=json.load(stream)
        if not self.owned or state._active_operation!=self.result['operation_id'] or record.get('state')!='started' or record.get('process_id')!=os.getpid():raise OperationError('journal_finalize_not_owned','Source-template journal ownership changed')
        record.update(result=copy.deepcopy(self.result),state='started' if recovery else 'complete');state._write_record(self.path,record)
        if not recovery:state._active_operation=None;self.owned=False


def binding_status():return dict(active=_ACTIVE,pending_count=int(_ENTRY is not None),quarantined_count=int(_RETAINED is not None),
    concrete_HN_job_reused=True,general_async_framework=False)


def activate():
    global _ACTIVE,_TICKER,_SHUTDOWN
    if not _ACTIVE:
        if _ENTRY is not None or _RETAINED is not None or _TICKER is not None or _SHUTDOWN is not None:raise OperationError('niagara_template_ownership_retained','Reconcile retained workflow before activation')
        _SHUTDOWN=unreal.register_python_shutdown_callback(_shutdown)
        try:_TICKER=unreal.register_ticker_callback(_tick,delay=0.0)
        except Exception:unreal.unregister_python_shutdown_callback(_SHUTDOWN);_SHUTDOWN=None;raise
        _ACTIVE=True
    return binding_status()


def _complete(entry,recovery):
    global _ENTRY,_RETAINED
    if entry.get('delivery_entered'):return
    entry['delivery_entered']=True
    if entry['flow'].lost:
        # Unsafe receiver/publication: keep only Python metadata and the durable
        # started journal. No reflected-result construction, CU/Job call or late
        # getter is safe to infer here; the caller's future may time out.
        entry['metadata_only_quarantine']=True
        try:entry['journal'].finish(True)
        except Exception as error:entry['delivery_error']=str(error)
        _RETAINED=entry;_ENTRY=None
        return
    try:
        entry['journal'].finish(recovery)
        entry['output'].set_editor_property('value',reflected_result(copy.deepcopy(entry['flow'].r)))
        entry['completion_entered']=True
        if entry['output'].complete_value() is not True:raise OperationError('niagara_template_delivery_failed','Official final delivery refused; no second completion')
    except Exception as error:
        recovery=True;entry['delivery_error']=str(error)
        if not entry.get('completion_entered'):
            entry['error_entered']=True
            try:entry['output'].set_error('Source-template durable/delivery outcome uncertain; operation_id='+entry['flow'].r['operation_id']+'; '+str(error))
            except Exception:pass
    if recovery:_RETAINED=entry
    _ENTRY=None


def _tick(delta_seconds):
    entry=_ENTRY
    if not _ACTIVE:return False
    if entry is not None:
        try:
            if entry['flow'].poll():_complete(entry,False)
        except Exception as error:
            entry['flow'].fail(error);_complete(entry,entry['flow'].in_flight or entry['flow'].r['data']['change_state_known'] is not True)
    return True


def _shutdown():
    global _ACTIVE,_RETAINED,_ENTRY
    _ACTIVE=False
    if _ENTRY is not None:
        _RETAINED=_ENTRY;_ENTRY=None
        _RETAINED['flow'].r['data'].update(change_state_known=False,mutation_state_unknown=True,lifecycle_interrupted=True,do_not_retry=True)
    # Metadata only during Engine exit; no late Job/asset/save/cleanup method.


def deactivate(reason='PS source-template unregister'):
    global _ACTIVE,_TICKER,_SHUTDOWN,_ENTRY,_RETAINED
    _ACTIVE=False
    if _ENTRY is not None:
        entry=_ENTRY;flow=entry['flow']
        if flow.job is not None and not flow.cancel_entered:
            flow.cancel_entered=True
            try:flow.job.cancel_job(reason)
            except Exception:pass
        flow.fail(flow.unsafe(OperationError('niagara_template_lifecycle_interrupted',reason)))
        _complete(entry,True)
    for name,handle,remove in (('ticker',_TICKER,unreal.unregister_ticker_callback),('shutdown',_SHUTDOWN,unreal.unregister_python_shutdown_callback)):
        if handle is not None:
            remove(handle)
            if name=='ticker':_TICKER=None
            else:_SHUTDOWN=None
    return binding_status()


def _named_mcp(method,arguments):
    global _ENTRY
    output=CubelessPythonAsyncOperationResult();journal=None;flow=None
    try:
        values=normalize_public_mcp_arguments(method,arguments)
        if not _ACTIVE or _ENTRY is not None or _RETAINED is not None:raise OperationError('niagara_template_inactive_or_retained','Official workflow lifecycle must be active and unowned')
        journal=_Journal(values);flow=_Flow(journal.result,values,journal.checkpoint)
        entry=dict(output=output,journal=journal,flow=flow);_ENTRY=entry
        if flow.start(True):_complete(entry,False)
    except Exception as error:
        if flow is not None:
            flow.fail(error);_complete(entry,flow.in_flight or flow.r['data']['change_state_known'] is not True)
        else:
            result=state.fail(state.new_result(),error);result['data'].update(mutation_performed=False,change_state_known=True,admission_rejected=True)
            output.set_editor_property('value',reflected_result(result));output.complete_value()
    return output


def completed_result(result):
    output=CubelessPythonAsyncOperationResult()
    output.set_editor_property('value',reflected_result(result));output.complete_value()
    return output


def create_niagara_system_from_source_template(target_system_path,source_system_path,source_emitter_indices=None,
        copy_source_user_parameters=True,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180):
    values=dict(target_system_path=target_system_path,source_system_path=source_system_path,source_emitter_indices=source_emitter_indices,
        copy_source_user_parameters=copy_source_user_parameters,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds)
    return state.mutate(METHOD,values,None,lambda result:create_niagara_system_from_source_template_action(result,**values,checkpoint_callback=state.checkpoint))


def create_niagara_system_from_source_template_action(result,target_system_path,source_system_path,source_emitter_indices=None,
        copy_source_user_parameters=True,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180,checkpoint_callback=None):
    values=dict(target_system_path=target_system_path,source_system_path=source_system_path,source_emitter_indices=source_emitter_indices,
        copy_source_user_parameters=copy_source_user_parameters,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds)
    flow=_Flow(result,values,checkpoint_callback)
    try:flow.start(False)
    except Exception as error:flow.fail(error);raise
    return result
