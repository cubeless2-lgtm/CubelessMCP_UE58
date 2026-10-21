"""FS original Cloth component public/body/raw contracts and Native-authoritative receipts."""
import copy
import json
import math
import re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import chaos_cloth_component as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={method:tuple(fields) for method,fields in PARAM_FIELDS.items()}
OBSERVERS=('inspect_chaos_cloth_component',)
WRITERS=('configure_chaos_cloth_component',)
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _walk_dependency(field,value):
    if isinstance(value,str) and _dependency(value):
        error=OperationError('chaos_cloth_component_dependency_reference','MCP-only dependency reference rejected')
        error.field_name=field;error.reason='mcp_only_reference';raise error
    if isinstance(value,dict):
        for key,row in value.items():_walk_dependency(field+'.'+str(key),row)
    elif isinstance(value,(list,tuple,set)):
        for index,row in enumerate(value):_walk_dependency(field+'['+str(index)+']',row)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursive values guard, before raw known-field filtering."""
    _walk_dependency('params',params)

DEFAULTS=json.loads("{\"inspect_chaos_cloth_component\":{\"actor_name\":\"\",\"actor_label\":\"\",\"component_name\":\"\",\"model_index\":0,\"lod_index\":-1,\"world_context\":\"auto\",\"include_runtime_properties\":true,\"max_properties\":512,\"max_results\":16},\"configure_chaos_cloth_component\":{\"actor_name\":\"\",\"property_updates\":null,\"component_settings\":null,\"component_name\":\"\",\"actor_label\":\"\",\"model_index\":0,\"lod_index\":-1,\"world_context\":\"auto\",\"expected_current\":null,\"allow_intrinsic_recreate\":false,\"dry_run\":true,\"recreate_simulation\":\"auto\",\"save_level\":false}}")

def _public_error(field,kind,message):
    error=OperationError('chaos_cloth_component_public_argument_invalid',field+': '+message)
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


def _public_int(field,value):
    if type(value) in (int,bool):return int(value)
    if type(value) is float:
        if not math.isfinite(value):raise _public_error(field,'finite_number','Input should be a finite number')
        if not value.is_integer():raise _public_error(field,'int_from_float','Input should be a valid integer, got a number with a fractional part')
        if not -(1<<63)<value<(1<<63):raise _public_error(field,'int_parsing_size','Unable to parse input string as an integer, exceeded maximum size')
        return int(value)
    if type(value) is str:
        stripped=value.strip()
        if stripped.isascii() and re.fullmatch(r'[+-]?[0-9](?:_?[0-9])*(?:\.0+)?',stripped):return int(stripped.replace('_','').split('.')[0])
        raise _public_error(field,'int_parsing','Input should be a valid integer, unable to parse string as an integer')
    raise _public_error(field,'int_type','Input should be a valid integer')



def _legacy_exception(method,error):
    inspect=method=='inspect_chaos_cloth_component'
    message='Chaos Cloth component inspection failed' if inspect else 'Chaos Cloth component configuration failed'
    return dict(success=False,message=message,error=message,error_code='chaos_cloth_component_inspection_failed' if inspect else 'chaos_cloth_component_configuration_failed',details={'exception_type':type(error).__name__})

def _public_mapping(field,value,location=()):
    if type(value) is not dict:
        error=_public_error(field,'dict_type','Input should be a valid dictionary');error.public_validation_errors[0]['field'].extend(location);raise error
    for key in value:
        if type(key) is not str:
            error=_public_error(field,'string_type','Input should be a valid string');error.public_validation_errors[0]['field'].extend([*location,key,'[key]']);raise error
    return value

def normalize_public_mcp_arguments(method,arguments):
    """Actual original eager preparse/model27; no Native range or writer preflight here."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('chaos_cloth_component_public_argument_invalid','Known method and public argument object required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if field in ('actor_name','actor_label','component_name','world_context','recreate_simulation'):
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif field=='property_updates':
            if value is not None:
                if type(value) is not list:raise _public_error(field,'list_type','Input should be a valid list')
                for index,row in enumerate(value):_public_mapping(field,row,(index,))
        elif field in ('component_settings','expected_current'):
            if value is not None:_public_mapping(field,value)
        elif field in ('model_index','lod_index','max_properties','max_results'):value=_public_int(field,value)
        else:value=_public_bool(field,value)
        values[field]=value
    return values

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('chaos_cloth_component_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('chaos_cloth_component_entry_invalid','Raw dict and public normalization are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    if method=='configure_chaos_cloth_component':
        try:selector=values['actor_name'].strip() or values['actor_label'].strip()
        except Exception as error:
            wrapped=OperationError('chaos_cloth_component_python_preflight_exception',str(error));wrapped.legacy_preflight_exception=dict(exception_type=type(error).__name__,returned_legacy_result=False,reason='Original selector preflight is outside original try');raise wrapped from error
        if not selector:
            message='Either actor_name or actor_label must be non-empty'
            error=OperationError('chaos_actor_selector_required',message);error.legacy_result=dict(success=False,message=message,error=message,error_code='chaos_actor_selector_required',details={'fields':['actor_name','actor_label'],'reason':'at_least_one_non_empty'});error.normalized_arguments=copy.deepcopy(values);raise error
        if values.get('property_updates') is None:values['property_updates']=[]
        if values.get('component_settings') is None:values['component_settings']={}
        if values.get('expected_current') is None:values.pop('expected_current',None)
        for field in ('property_updates','component_settings'):
            try:_walk_dependency(field,values[field])
            except OperationError as error:
                message='MCP-only dependency reference rejected';path=error.field_name
                if not isinstance(path,str) or not path:path='<unknown>'
                if len(path)>256:path=path[:253]+'...'
                error.code='mcp_dependency_reference_rejected';error.legacy_result=dict(success=False,message=message,error=message,error_code='mcp_dependency_reference_rejected',details={'field':path,'reason':'mcp_only_reference'});raise
    api._carrier(values)
    return dict(params=values,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=0,Named_dependency_guard_fields=['property_updates','component_settings'] if method in WRITERS else [],Batch_recursive_string_value_guard_owned_by_Batch=True,
        original_pre_parse_json_preserved=True,Named_selector_strip_check_does_not_trim_transmission=True,configure_None_updates_settings_normalized_only_on_Named_or_direct=True,expected_current_None_omitted_only_on_Named_or_direct=True,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_notify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        runtime_exact_write_guard_owned_by_Native=True,Python_runtime_writer_bypass=False,semantic_restore_is_not_atomic_rollback=True,actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


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
    mutation=dict(attempted=attempted,performed=performed,state='performed' if performed is True else 'not_performed' if performed is False else 'unknown',transactional=False)
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
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_chaos_cloth_component_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_chaos_cloth_component_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_chaos_cloth_component_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_legacy_component_runtime_proxy_teleport_dirty_save_restore_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_cloth_runtime_proxy_teleport_save_restore_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_chaos_cloth_component_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_chaos_cloth_component_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_chaos_cloth_component_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_named_result(method,_legacy_exception(method,error),args)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_chaos_cloth_component_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'chaos_cloth_component_native_failed',native.get('error') or 'Native chaos_cloth_component failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('chaos_cloth_component_postcondition_unverified','Native chaos_cloth_component postcondition did not pass; no replay')
    # Native owns configured/runtime readback and unsupported-before-writer admission; SDK/live simulation effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted']):
        raise OperationError('chaos_cloth_component_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='chaos_cloth_component_input_validation';result['changes']=[]
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
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)


def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)


def inspect_chaos_cloth_component(actor_name="",actor_label="",component_name="",model_index=0,lod_index=-1,world_context="auto",include_runtime_properties=True,max_properties=512,max_results=16)->Dict[str,Any]:
    return _named("inspect_chaos_cloth_component",dict(actor_name=actor_name,actor_label=actor_label,component_name=component_name,model_index=model_index,lod_index=lod_index,world_context=world_context,include_runtime_properties=include_runtime_properties,max_properties=max_properties,max_results=max_results))

def inspect_chaos_cloth_component_action(result,actor_name="",actor_label="",component_name="",model_index=0,lod_index=-1,world_context="auto",include_runtime_properties=True,max_properties=512,max_results=16,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_chaos_cloth_component",dict(actor_name=actor_name,actor_label=actor_label,component_name=component_name,model_index=model_index,lod_index=lod_index,world_context=world_context,include_runtime_properties=include_runtime_properties,max_properties=max_properties,max_results=max_results),checkpoint_callback,_legacy_command_params,_public_input)

def configure_chaos_cloth_component(actor_name="",property_updates=None,component_settings=None,component_name="",actor_label="",model_index=0,lod_index=-1,world_context="auto",expected_current=None,allow_intrinsic_recreate=False,dry_run=True,recreate_simulation="auto",save_level=False)->Dict[str,Any]:
    return _named("configure_chaos_cloth_component",dict(actor_name=actor_name,property_updates=property_updates,component_settings=component_settings,component_name=component_name,actor_label=actor_label,model_index=model_index,lod_index=lod_index,world_context=world_context,expected_current=expected_current,allow_intrinsic_recreate=allow_intrinsic_recreate,dry_run=dry_run,recreate_simulation=recreate_simulation,save_level=save_level))

def configure_chaos_cloth_component_action(result,actor_name="",property_updates=None,component_settings=None,component_name="",actor_label="",model_index=0,lod_index=-1,world_context="auto",expected_current=None,allow_intrinsic_recreate=False,dry_run=True,recreate_simulation="auto",save_level=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"configure_chaos_cloth_component",dict(actor_name=actor_name,property_updates=property_updates,component_settings=component_settings,component_name=component_name,actor_label=actor_label,model_index=model_index,lod_index=lod_index,world_context=world_context,expected_current=expected_current,allow_intrinsic_recreate=allow_intrinsic_recreate,dry_run=dry_run,recreate_simulation=recreate_simulation,save_level=save_level),checkpoint_callback,_legacy_command_params,_public_input)
