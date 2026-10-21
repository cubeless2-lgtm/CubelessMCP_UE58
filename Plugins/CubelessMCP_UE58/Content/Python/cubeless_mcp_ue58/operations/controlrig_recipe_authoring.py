"""HH public ControlRig recipe wrappers; actual work and result truth stay Native.
Recipe verification policy is distinct from current compile/save dispatch.
"""
import copy
import hashlib
import json
from . import state
from .state import OperationError
from cubeless_mcp_ue58.compatibility.ue58 import controlrig_recipe_authoring as api

METHODS=['validate_controlrig_recipe','dry_run_controlrig_recipe','build_controlrig_recipe','build_controlrig_recipe_with_animbp_binding']
PUBLIC_FIELDS={METHODS[0]:['recipe'],METHODS[1]:['recipe'],METHODS[2]:['recipe','options'],METHODS[3]:['recipe','options']}
PARAM_FIELDS=copy.deepcopy(PUBLIC_FIELDS)
DEFAULTS={METHODS[0]:{},METHODS[1]:{},METHODS[2]:{'options':None},METHODS[3]:{'options':None}}
REQUIRED={method:['recipe'] for method in METHODS}
FIELD_TYPES={method:{field:'object_or_string' for field in fields} for method,fields in PUBLIC_FIELDS.items()}
NULLABLE={METHODS[0]:[],METHODS[1]:[],METHODS[2]:['options'],METHODS[3]:['options']}
ITEMS={method:{} for method in METHODS}
NO_JOURNAL_METHODS=METHODS[:2]
OBSERVERS=METHODS[:2]
WRITERS=METHODS[2:]
_ACTIVE_REQUEST=False
_ORIGINAL_FORBIDDEN_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')
_BATCH_FORBIDDEN_TOKENS=_ORIGINAL_FORBIDDEN_TOKENS+('cubelessmcp','cubeless_mcp','modelcontextprotocol','toolsetregistry','/engine/pythontypes')


def _reject_references(field_name,value,tokens):
    if isinstance(value,str):
        candidate=value.strip().strip('"\'').replace('\\','/').lower()
        if any(token in candidate for token in tokens):
            error=ValueError("Refusing to persist MCP-only reference in '%s': %s. UnrealMCP is an editor authoring bridge; finished assets must not depend on it."%(field_name,value))
            error.field_name=field_name;error.reason='mcp_only_reference';raise error
    elif isinstance(value,dict):
        for key,item in value.items():_reject_references(field_name+'.'+str(key),item,tokens)
    elif isinstance(value,(list,tuple,set)):
        for index,item in enumerate(value):_reject_references(field_name+'['+str(index)+']',item,tokens)


def reject_mcp_dependency_references(field_name,value):
    _reject_references(field_name,value,_ORIGINAL_FORBIDDEN_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    _reject_references('params',params,_BATCH_FORBIDDEN_TOKENS)


def _public_error(field,kind,message):
    error=OperationError('controlrig_recipe_authoring_public_argument_invalid',field+': '+message)
    error.public_validation_errors=[dict(field=[field],type=kind,message=message)];return error


def normalize_public_mcp_arguments(method,arguments):
    if method not in METHODS or type(arguments) is not dict:
        raise OperationError('controlrig_recipe_authoring_public_argument_invalid','Known method and public argument object required')
    values={}
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:
            if field not in DEFAULTS[method]:raise _public_error(field,'missing','Field required')
            value=copy.deepcopy(DEFAULTS[method][field])
        else:value=copy.deepcopy(arguments[field])
        # Original FastMCP JSON preparse precedes the union model.
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if value is None and field in NULLABLE[method]:
            values[field]=None;continue
        if not isinstance(value,(dict,str)) or isinstance(value,dict) and any(not isinstance(key,str) for key in value):
            raise _public_error(field,'dict_type_or_string_type','Input should be a valid dictionary or string')
        values[field]=value
    return values


def _json_payload(value,field_name):
    if isinstance(value,str):
        parsed=json.loads(value)
        reject_mcp_dependency_references(field_name,parsed)
        return json.dumps(parsed,ensure_ascii=False)
    reject_mcp_dependency_references(field_name,value)
    return json.dumps(value,ensure_ascii=False)


def binding_status():
    from . import controlrig_recipe_binding
    methods=api.binding_status()
    methods[METHODS[3]]=controlrig_recipe_binding.binding_status()
    return methods


def _arguments(method,arguments,legacy=None,public_input=False):
    if method not in METHODS or type(arguments) is not dict or type(public_input) is not bool:
        raise OperationError('controlrig_recipe_authoring_entry_invalid','Known method, argument object and exact model selector required')
    original=copy.deepcopy(arguments)
    if legacy is not None:
        if type(legacy) is not dict or public_input:
            raise OperationError('controlrig_recipe_authoring_entry_invalid','Lower-name alias uses the Python public contract, not a separate raw Native contract')
        arguments=legacy
    if public_input:values=normalize_public_mcp_arguments(method,arguments)
    else:
        values=copy.deepcopy(DEFAULTS[method]);values.update({key:copy.deepcopy(arguments[key]) for key in PUBLIC_FIELDS[method] if key in arguments})
    for field in REQUIRED[method]:
        if field not in values:raise _public_error(field,'missing','Field required')
    return values,original


def _checkpoint(result,callback):
    if callback:callback(result)


def _context(result,method,original,legacy,public_input):
    parent=result['data'].get('parent_journal_only') is True
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),lower_name_Batch_arguments=copy.deepcopy(legacy),
        public_argument_validation_applied=public_input,original_Native_command=False,hidden_fields=0,
        journal_created=method in WRITERS and not parent,parent_journal_only=parent,
        native_dispatched=False,native_result_returned=False,native_call_count=0,native_health_passed=True,
        native_mutation_attempted=False,change_state_known=True,observed_changed=False,mutation_state_unknown=False,
        execution_mutation_attempted=False,mutation_performed=False,operation_complete=False,
        SDK_global_purity_unverified=False,automatic_action_retry=False,atomic_rollback=False,
        actual_named_wire_verified=False,actual_backend_route='UE58_ControlRig_recipe_authoring_CPP',
        recipe_compile_save_policy_is_actual_dispatch=False,options_dry_run_does_not_reclassify_build=True,
        registration_dependency='ToolsetAPI',operation_dependency='direct_public_Engine_API',server_RPC_dependency=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='not_requested')
    result['save'].update(requested=False,attempted=False,saved=False,status='not_attempted')
    result['changes']=[]


def _changes(result):
    data=result['data'];known=data['change_state_known'];attempted=data['native_mutation_attempted'];changed=data['observed_changed']
    data.update(execution_mutation_attempted=attempted,mutation_state_unknown=not known,mutation_performed=changed if known else None)
    result['changes']=[] if known and not attempted and not changed else [dict(kind='controlrig_recipe_authoring',attempted=attempted,changed=changed if known else None,change_state_known=known)]
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_native_outcome_no_replay' if not known else 'inspect_recorded_changes_before_new_operation' if attempted or changed else 'none_observed')


def _adopt(result,native):
    data=result['data'];raw=native.pop('_raw_native_json',None)
    data.update(native_result_returned=True,native_mutation_attempted=data['native_mutation_attempted'] or native.get('mutation_attempted') is True,
        change_state_known=native.get('change_state_known') is True and type(native.get('changed')) is bool,
        observed_changed=data['observed_changed'] or native.get('changed') is True,
        native_health_passed=native.get('success') is True and native.get('postcondition_passed') is True)
    result['compile']['requested']|=native.get('compile_requested') is True
    result['compile']['attempted']|=native.get('compile_attempted') is True
    result['compile']['compiled']=native.get('compiled')
    result['compile']['status']='passed' if native.get('compiled') is True else 'unknown' if native.get('compiled') is None else 'failed' if result['compile']['attempted'] else 'not_attempted_recipe_policy_only' if result['compile']['requested'] else 'not_requested'
    result['save']['requested']|=native.get('save_requested') is True
    result['save']['attempted']|=native.get('save_attempted') is True
    result['save']['saved']|=native.get('saved') is True
    result['save']['status']='saved' if result['save']['saved'] else 'failed' if result['save']['attempted'] else 'not_attempted'
    data['native_scalar_result']={key:copy.deepcopy(value) for key,value in native.items() if key!='data'}
    body=native.get('data',{})
    data['native_operation_data']={key:copy.deepcopy(value) for key,value in body.items() if key!='legacy_result'}
    data['SDK_global_purity_unverified']=body.get('SDK_global_purity_unverified') is True
    if type(body.get('legacy_result')) is dict:data['legacy_result']=copy.deepcopy(body['legacy_result'])
    for key in ('actual_mutation_prefix','actual_compile_prefix','actual_save_prefix','actual_partial_state','actual_observation_prefix','native_call_counts','actual_call_prefix','cleanup_prefix','actual_cleanup_prefix'):
        if key in body:data[key]=copy.deepcopy(body[key])
    if type(raw) is str:
        encoded=raw.encode('utf-8');data['native_raw_receipt']=dict(original_raw_preserved=False,omitted=True,reason='decoded_authoritative_receipt_retained_once',sha256=hashlib.sha256(encoded).hexdigest(),utf8_bytes=len(encoded),utf16_units=len(raw.encode('utf-16-le'))//2)
    _changes(result)


def _native(result,method,payload,callback):
    data=result['data'];prior=data['change_state_known']
    data.update(native_dispatch_pending=True,change_state_known=False);_changes(result)
    try:_checkpoint(result,callback)
    except Exception:
        data.update(native_dispatch_pending=False,change_state_known=prior);_changes(result);raise
    data.update(native_dispatch_pending=False,native_dispatched=True,native_method=method);data['native_call_count']+=1
    try:native=getattr(api,method)(payload)
    except Exception as error:
        partial=getattr(error,'native_partial_result',None)
        if type(partial) is dict:_adopt(result,partial)
        if getattr(error,'native_call_dispatched',None) is False:
            data.update(native_dispatched=False,change_state_known=prior);data['native_call_count']-=1
        else:data['change_state_known']=False
        if getattr(error,'native_call_returned',False):
            raw=getattr(error,'native_raw_result',None)
            if partial is None:data['undecoded_native_raw_result']=raw
            else:data['invalid_receipt_decoded_prefix_retained']=True
        data['native_health_passed']=False;_changes(result)
        result['stage']='returned_controlrig_recipe_authoring_operation';_checkpoint(result,callback);raise
    _adopt(result,native)
    result['stage']='returned_controlrig_recipe_authoring_operation';_checkpoint(result,callback)
    if method in OBSERVERS and (any(native.get(key) is not False for key in ('mutation_attempted','compile_attempted','save_attempted','saved')) or native.get('compiled') is not False or native.get('changed') is True):
        raise OperationError('controlrig_recipe_authoring_unexpected_observer_effect','Validate/dry-run returned actual mutation/compile/save effects; actual prefix retained')
    if native.get('data',{}).get('operation_complete') is False:
        data['change_state_known']=False;_changes(result)
        raise OperationError('controlrig_recipe_authoring_native_incomplete','Current Native request is incomplete; no replay')
    leaf=data.get('legacy_result')
    if type(leaf) is not dict or type(leaf.get('success')) is not bool:
        raise OperationError('controlrig_recipe_authoring_legacy_invalid','Original authoring report and Boolean semantic success required')
    leaf.update(authoring_plugin_used=not bool(leaf.get('plugin_unavailable')),authoring_subsystem=api.LIBRARY,legacy_authoring_subsystem_fallback=False,adapter='python_reflection')
    return leaf


def _legacy_exception(error):
    leaf=dict(success=False,message=str(error),error=str(error),error_code='controlrig_authoring_tool_failed',details=dict(exception_type=type(error).__name__))
    if getattr(error,'code','')=='controlrig_recipe_authoring_native_api_unavailable':
        leaf.update(plugin_unavailable=True,authoring_plugin_used=False,authoring_subsystem=api.LIBRARY,legacy_authoring_subsystem_fallback=False,adapter='python_reflection')
    return leaf


def _finish(result,leaf,callback):
    data=result['data'];data.update(legacy_result=leaf,operation_complete=True)
    result['stage']='returned_controlrig_recipe_authoring_operation';_checkpoint(result,callback)
    result['validation'].update(status='original_recipe_report_and_current_Native_receipt',complete_dependency_scan='not_inferred',packaged_runtime='not_run')
    if leaf.get('success') is not True or not data['native_health_passed']:
        scalar=data.get('native_scalar_result',{})
        return state.fail(result,OperationError(scalar.get('error_code') or leaf.get('error_code') or 'controlrig_recipe_authoring_failed',scalar.get('error') or leaf.get('error') or leaf.get('message') or leaf.get('readback_summary') or 'ControlRig recipe operation failed; original report retained'))
    if not data['change_state_known']:return state.fail(result,OperationError('controlrig_recipe_authoring_outcome_unknown','Current target outcome remains unknown; no replay'))
    result.update(success=True,stage='complete');return result


def _run(result,method,values,original,legacy,public_input,callback):
    if method==METHODS[3]:
        from . import controlrig_recipe_binding
        return controlrig_recipe_binding._run(result,values,original,legacy,public_input,callback)
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:raise OperationError('controlrig_recipe_authoring_reentry','Reentry rejected before work')
    _ACTIVE_REQUEST=True
    try:
        _context(result,method,original,legacy,public_input)
        result['stage']='before_controlrig_recipe_authoring_checkpoint';_checkpoint(result,callback)
        try:
            envelope={'recipe':_json_payload(values['recipe'],'recipe')}
            if method==METHODS[2] and values.get('options') is not None:envelope['options']=_json_payload(values['options'],'options')
            result['data']['options_omitted']='options' not in envelope
            result['data']['recipe_body_json']=envelope['recipe']
            if 'options' in envelope:result['data']['options_body_json']=envelope['options']
            leaf=_native(result,method,json.dumps(envelope,ensure_ascii=False),callback)
            return _finish(result,leaf,callback)
        except Exception as error:
            if 'legacy_result' not in result['data']:result['data']['legacy_result']=_legacy_exception(error)
            else:result['data']['adapter_failure']=_legacy_exception(error)
            raise
    finally:_ACTIVE_REQUEST=False


def _rejected(method,error):
    result=state.new_result();state.fail(result,error);result['stage']='controlrig_recipe_authoring_input_validation'
    result['compile'].update(requested=False,attempted=False,compiled=False)
    result['save'].update(requested=False,attempted=False,saved=False)
    result['data'].update(public_method=method,native_dispatched=False,native_call_count=0,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False,legacy_result=_legacy_exception(error))
    if hasattr(error,'public_validation_errors'):result['data']['public_validation_errors']=copy.deepcopy(error.public_validation_errors)
    return result


def _named(method,arguments,public_input=False):
    try:values,original=_arguments(method,arguments,None,public_input)
    except Exception as error:return _rejected(method,error)
    if method in NO_JOURNAL_METHODS:
        result=state.new_result()
        try:return _run(result,method,values,original,None,public_input,None)
        except Exception as error:return state.fail(result,error)
    return state.mutate(method,arguments,None,lambda result:_run(result,method,values,original,None,public_input,state.checkpoint))


def _named_mcp(method,arguments):return _named(method,arguments,True)


def _action(result,method,arguments,callback,legacy,public_input):
    result['data']['parent_journal_only']=True
    try:values,original=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        rejected=_rejected(method,error);rejected['operation_id']=result['operation_id'];result.update(rejected);result['data']['parent_journal_only']=True
        _checkpoint(result,callback);raise
    return _run(result,method,values,original,legacy,public_input,callback)


def validate_controlrig_recipe(recipe):return _named(METHODS[0],dict(recipe=recipe))


def validate_controlrig_recipe_action(result,recipe,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,METHODS[0],dict(recipe=recipe),checkpoint_callback,_legacy_command_params,_public_input)


def dry_run_controlrig_recipe(recipe):return _named(METHODS[1],dict(recipe=recipe))


def dry_run_controlrig_recipe_action(result,recipe,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,METHODS[1],dict(recipe=recipe),checkpoint_callback,_legacy_command_params,_public_input)


def build_controlrig_recipe(recipe,options=None):return _named(METHODS[2],dict(recipe=recipe,options=options))


def build_controlrig_recipe_action(result,recipe,options=None,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,METHODS[2],dict(recipe=recipe,options=options),checkpoint_callback,_legacy_command_params,_public_input)


def build_controlrig_recipe_with_animbp_binding(recipe,options=None):
    return _named(METHODS[3],dict(recipe=recipe,options=options))


def build_controlrig_recipe_with_animbp_binding_action(result,recipe,options=None,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,METHODS[3],dict(recipe=recipe,options=options),checkpoint_callback,_legacy_command_params,_public_input)
