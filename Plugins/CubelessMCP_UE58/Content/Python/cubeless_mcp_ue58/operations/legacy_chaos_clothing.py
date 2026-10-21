"""FU original legacy section-clothing public/body/raw contracts and Native-authoritative receipts."""
import copy
import json
import math
import re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import legacy_chaos_clothing as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={method:tuple(fields) for method,fields in PARAM_FIELDS.items()}
OBSERVERS=()
WRITERS=tuple(METHODS)
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _walk_dependency(field,value):
    if isinstance(value,str) and _dependency(value):
        error=OperationError('legacy_chaos_clothing_dependency_reference','MCP-only dependency reference rejected')
        error.field_name=field;error.reason='mcp_only_reference';raise error
    if isinstance(value,dict):
        for key,row in value.items():_walk_dependency(field+'.'+str(key),row)
    elif isinstance(value,(list,tuple,set)):
        for index,row in enumerate(value):_walk_dependency(field+'['+str(index)+']',row)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursive values guard, before raw known-field filtering."""
    _walk_dependency('params',params)

DEFAULTS={"create_legacy_chaos_clothing_from_skeletal_sections":dict(dry_run=True,save_assets=True,use_source_physics_asset=True,create_vroid_lower_body_cloth_physics_asset=False)}
REQUIRED={"create_legacy_chaos_clothing_from_skeletal_sections":('source_skeletal_mesh_path','output_skeletal_mesh_path','section_configs')}

def _public_error(field,kind,message):
    error=OperationError('legacy_chaos_clothing_public_argument_invalid',field+': '+message)
    error.public_validation_errors=[dict(field=[field],type=kind,message=message)];return error


def _public_bool(field,value):
    if type(value) is bool:return value
    if type(value) in (int,float) and value in (0,1):return bool(value)
    if type(value) is str:
        lower=value.lower()
        if lower in ('true','1','t','yes','y','on'):return True
        if lower in ('false','0','f','no','n','off'):return False
        raise _public_error(field,'bool_parsing','Input should be a valid boolean, unable to interpret input')
    if type(value) in (int,float):raise _public_error(field,'bool_parsing','Input should be a valid boolean, unable to interpret input')
    raise _public_error(field,'bool_type','Input should be a valid boolean')


def _legacy_exception(method,error):
    message='Legacy Chaos Cloth skeletal-section creation failed'
    return dict(success=False,message=message,error=message,error_code='chaos_cloth_asset_creation_failed',details={'exception_type':type(error).__name__})


def normalize_public_mcp_arguments(method,arguments):
    """Observed original preparse/model: nested Dict[str,Any] values stay unchanged."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('legacy_chaos_clothing_public_argument_invalid','Known method and public argument object required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:
            if field in REQUIRED[method]:raise _public_error(field,'missing','Field required')
            continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if field=='section_configs':
            if not isinstance(value,(list,tuple)):raise _public_error(field,'list_type','Input should be a valid list')
            value=list(value)
            for index,row in enumerate(value):
                if type(row) is not dict:
                    error=_public_error(field,'dict_type','Input should be a valid dictionary');error.public_validation_errors[0]['field'].append(index);raise error
                for key in row:
                    if type(key) is not str:
                        error=_public_error(field,'string_type','Input should be a valid string');error.public_validation_errors[0]['field'].extend([index,key,'[key]']);raise error
            # No section_count/section_index/material/value normalization here.
            # Native owns strict per-section validation and its original error order.
        elif field in ('dry_run','save_assets','use_source_physics_asset','create_vroid_lower_body_cloth_physics_asset'):
            value=_public_bool(field,value)
        elif type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        values[field]=value
    return values


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('legacy_chaos_clothing_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('legacy_chaos_clothing_entry_invalid','Raw dict and public normalization are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params)
        return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    try:
        for field in ('source_skeletal_mesh_path','output_skeletal_mesh_path'):
            if _dependency(values.get(field)):_walk_dependency(field,values[field])
        _walk_dependency('section_configs',values.get('section_configs'))
    except OperationError as error:
        field=error.field_name
        if not isinstance(field,str) or not field:field='<unknown>'
        if len(field)>256:field=field[:253]+'...'
        error.code='mcp_dependency_reference_rejected'
        error.legacy_result=dict(success=False,message='MCP-only dependency reference rejected',error='MCP-only dependency reference rejected',error_code=error.code,details={'field':field,'reason':'mcp_only_reference'})
        raise
    api._carrier(values)
    return dict(params=values,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=0,Named_dependency_guard_fields=['source_skeletal_mesh_path','output_skeletal_mesh_path','section_configs'],Batch_recursive_string_value_guard_owned_by_Batch=True,
        original_pre_parse_json_preserved=True,nested_section_values_are_Any_not_coerced=True,section_strict_shape_range_order_owned_by_Native=True,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_notify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        compile_receipt_scope='Explicit compiler distinct from skeletal post-edit/build/physics cook/bind',
        actual_save_receipt_authority='Native actual SavePackages attempts/returns; legacy vacuous savedTrue is retained separately and never promoted',
        cleanup_is_not_atomic_rollback=True,cleanup_is_not_proven_UI_free=True,actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _legacy_named_result(method,legacy,args):
    """Narrow original managed projection for this dedicated Native leaf, not a transport parser."""
    if method in OBSERVERS or type(legacy) is not dict:return copy.deepcopy(legacy)
    value=copy.deepcopy(legacy);dry_run=args['params'].get('dry_run',True) is True;mode='dry_run' if dry_run else 'apply'
    legacy_success=value.get('success') is not False and str(value.get('status','')).strip().lower() not in ('error','failed','failure','fail','disabled','rate_limited','blocked')
    prior=value.get('mutation') if type(value.get('mutation')) is dict else {}
    attempted_values=[x for x in (value.get('mutation_attempted'),prior.get('attempted')) if type(x) is bool]
    performed_values=[x for x in (value.get('mutation_performed'),prior.get('performed')) if type(x) is bool]
    attempted=True if True in attempted_values else False if False in attempted_values else not dry_run
    performed=True if True in performed_values else False if False in performed_values else None
    changed=value.get('changed') if type(value.get('changed')) is bool else performed
    conflict=(True in performed_values and False in performed_values) or (type(changed) is bool and type(performed) is bool and changed!=performed) or (value.get('partial_target_modified') is True and performed is False)
    unknown=value.get('mutation_state_unknown') is True or value.get('command_may_still_be_in_flight') is True or conflict
    if unknown:performed=None;changed=None
    warnings=[]
    if conflict:warnings.append('conflicting_mutation_state_envelopes')
    safety=value.get('partial_target_modified') is True or value.get('partial_mutation') is True or value.get('do_not_retry') is True or value.get('workflow_recovery_required') is True
    if safety and legacy_success:warnings.append('success_conflicts_with_mutation_safety_fields')
    if type(value.get('dry_run')) is bool and value['dry_run']!=dry_run:warnings.append('response_dry_run_does_not_match_request')
    if dry_run and attempted:warnings.append('dry_run_reported_mutation_attempted')
    if dry_run and performed is True:warnings.append('dry_run_reported_mutation_performed')
    before=value.get('before');after=value.get('after');verification=copy.deepcopy(value.get('verification')) if type(value.get('verification')) is dict else {}
    status=str(verification.get('status','')).strip().lower()
    if status not in ('passed','failed','pending','timeout','unverified','not_completed','not_applicable'):status='unverified' if legacy_success else 'not_completed'
    if warnings:status='failed'
    if legacy_success and not unknown:
        if before is None:warnings.append('strict_contract_missing_before_observation')
        if after is None:warnings.append('strict_contract_missing_after_observation')
        if type(verification.get('method')) is not str or not verification.get('method','').strip():warnings.append('strict_contract_missing_verification_method')
        if status!='passed':warnings.append('strict_contract_postcondition_not_passed')
        if warnings:status='failed'
    verification.update(status=status,before_observed=before is not None,after_observed=after is not None,strict=True);verification.setdefault('method','not_reported')
    if warnings:verification['warnings']=list(dict.fromkeys(list(verification.get('warnings',[]))+warnings))
    counts=('affected_count','created_count','updated_count','removed_count','skipped_count','failed_count')
    delta=copy.deepcopy(value.get('delta')) if type(value.get('delta')) is dict else {}
    for field in counts:
        item=delta.get(field,value.get(field));delta[field]=item if type(item) is int and item>=0 else None
    if delta['affected_count'] is None and any(delta[k] is not None for k in counts[1:4]):delta['affected_count']=sum(delta[k] or 0 for k in counts[1:4])
    details={k:v for k,v in value.items() if (k in counts or k.endswith('_count') or k=='instances_removed') and type(v) is int and v>=0}
    if details:delta['details']=dict(sorted({**details,**(delta.get('details') if type(delta.get('details')) is dict else {})}.items()))
    verified=not unknown and legacy_success and status=='passed'
    semantics='unknown_command_outcome' if unknown else ('verified_dry_run' if verified else 'failed_dry_run' if not legacy_success or status=='failed' else 'unverified_dry_run') if dry_run else 'verified_postcondition' if verified else 'postcondition_failed' if status=='failed' else 'failed_execution' if not legacy_success else 'legacy_execution'
    mutation=dict(attempted=attempted,performed=performed,state='performed' if performed is True else 'not_performed' if performed is False else 'unknown',transactional=True)
    contract=dict(contract_version='mutation_result_v1',tool=method,mode=mode,dry_run=dry_run,changed=changed,mutation=mutation,delta=delta,verification=verification,verified_success=verified,success_semantics=semantics)
    for field in ('partial_mutation','partial_target_modified','completed_mutation_count','completed_mutation_stages','failed_stage','failed_stage_effect','failed_stage_mutation_performed','mutation_state_unknown','command_may_still_be_in_flight','do_not_retry','workflow_recovery_required','recovery_steps','recovery_guidance','restore_errors'):
        if field in value and value[field] is not None:contract[field]=copy.deepcopy(value[field])
    if before is not None:contract['before']=copy.deepcopy(before)
    if after is not None:contract['after']=copy.deepcopy(after)
    if unknown:contract.update(mutation_state_unknown=True,do_not_retry=True)
    value.update(contract_version='mutation_result_v1',dry_run=dry_run,DRY_RUN=dry_run,verified_success=verified,success_semantics=semantics,mutation_result=contract);value.setdefault('mode',mode)
    for field,item in (('changed',changed),('mutation',mutation),('delta',delta),('verification',verification)):value.setdefault(field,item)
    for field in counts:value.setdefault(field,delta[field])
    if not legacy_success:value.update(success=False,status='error')
    if unknown:value.update(success=False,status='error',mutation_state_unknown=True,do_not_retry=True,changed=None,mutation=mutation,verification=verification);value.setdefault('error_code','mutation_state_unknown')
    elif safety:
        value.update(success=False,status='error');value.setdefault('error_code','mutation_safety_failure_declared');message=method+' reported mutation safety or recovery fields that are incompatible with success';value.setdefault('message',message);value.setdefault('error',message)
    if status=='failed' and legacy_success:
        value.update(legacy_success=True,success=False,status='error');value.setdefault('error_code','mutation_postcondition_failed');message=method+' returned without a passing independent postcondition; treat mutation state as unverified';value.setdefault('message',message);value.setdefault('error',message)
    return value


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=_legacy_named_result(method,native['data'].get('legacy_result'),args)
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_legacy_chaos_clothing_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_legacy_chaos_clothing_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_legacy_chaos_clothing_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_section_factory_material_physics_paint_bind_build_actual_save_cleanup_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_legacy_cloth_create_bind_save_cleanup_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_legacy_chaos_clothing_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_legacy_chaos_clothing_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_legacy_chaos_clothing_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_named_result(method,_legacy_exception(method,error),args)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_legacy_chaos_clothing_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'legacy_chaos_clothing_native_failed',native.get('error') or 'Native legacy_chaos_clothing failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('legacy_chaos_clothing_postcondition_unverified','Native legacy_chaos_clothing postcondition did not pass; no replay')
    # Native owns complete section preflight/create/paint/bind/post-edit/readback, sequential actual saves and owned cleanup; SDK effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted']):
        raise OperationError('legacy_chaos_clothing_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='legacy_chaos_clothing_input_validation';result['changes']=[]
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    if hasattr(error,'legacy_result'):result['data']['legacy_result']=copy.deepcopy(error.legacy_result)
    if hasattr(error,'legacy_preflight_exception'):result['data']['legacy_Python_preflight_exception']=copy.deepcopy(error.legacy_preflight_exception)
    if hasattr(error,'public_validation_errors'):result['data'].update(public_validation_errors=copy.deepcopy(error.public_validation_errors),original_framework_error_text_reproduced=False,tool_body_invoked=False)
    return result


def _rejected(method,error):return _rejection(state.new_result(),method,error)


def _named(method,arguments,_public_input=False):
    try:args,original,legacy=_arguments(method,arguments,None,_public_input)
    except Exception as error:
        rejected=_rejected(method,error)
        old_value=getattr(error,'legacy_result',None if hasattr(error,'public_validation_errors') or hasattr(error,'legacy_preflight_exception') else _legacy_exception(method,error))
        rejected['data']['legacy_Named_result']=_legacy_named_result(method,old_value,dict(params=getattr(error,'normalized_arguments',arguments)))
        return rejected
    if method in OBSERVERS:
        result=state.new_result()
        try:return _run(result,method,args,original,legacy)
        except Exception as error:return state.fail(result,error)
    return state.mutate(method,arguments,None,lambda result:_run(result,method,args,original,legacy,state.checkpoint))


def _action(result,method,arguments,callback,legacy,public_input=False):
    result['data']['parent_journal_only']=True
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)


def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)


def create_legacy_chaos_clothing_from_skeletal_sections(source_skeletal_mesh_path,output_skeletal_mesh_path,section_configs,dry_run=True,save_assets=True,use_source_physics_asset=True,create_vroid_lower_body_cloth_physics_asset=False)->Dict[str,Any]:
    return _named('create_legacy_chaos_clothing_from_skeletal_sections',dict(source_skeletal_mesh_path=source_skeletal_mesh_path,output_skeletal_mesh_path=output_skeletal_mesh_path,section_configs=section_configs,dry_run=dry_run,save_assets=save_assets,use_source_physics_asset=use_source_physics_asset,create_vroid_lower_body_cloth_physics_asset=create_vroid_lower_body_cloth_physics_asset))


def create_legacy_chaos_clothing_from_skeletal_sections_action(result,source_skeletal_mesh_path=None,output_skeletal_mesh_path=None,section_configs=None,dry_run=True,save_assets=True,use_source_physics_asset=True,create_vroid_lower_body_cloth_physics_asset=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'create_legacy_chaos_clothing_from_skeletal_sections',dict(source_skeletal_mesh_path=source_skeletal_mesh_path,output_skeletal_mesh_path=output_skeletal_mesh_path,section_configs=section_configs,dry_run=dry_run,save_assets=save_assets,use_source_physics_asset=use_source_physics_asset,create_vroid_lower_body_cloth_physics_asset=create_vroid_lower_body_cloth_physics_asset),checkpoint_callback,_legacy_command_params,_public_input)
