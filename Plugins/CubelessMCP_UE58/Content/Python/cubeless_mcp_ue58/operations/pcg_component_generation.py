"""HD PCG component generation and Blueprint binding. Dedicated Native owns its SDK work.
Named deferred observation, raw single-frame and direct Python fallback are distinct.
"""
import copy
import hashlib
import json
import math
import re
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import pcg_component_generation as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS[:2]
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"refresh_pcg_components":["actor_name","selected_only","cleanup","generate","refresh","force","wait_until_complete","timeout_seconds","poll_interval_seconds","min_generated_output_data_count","min_managed_resource_count","max_components","native_first"],"bind_pcg_generation_settings_to_blueprint":["blueprint_name","component_variable","generation_trigger_variable","regenerate_in_editor_variable","generate_on_drop_variable","generate_on_drop_default","graph_name","graph_id","graph_type","replace_existing","sync_component_template_defaults","function_class","function_name","compile","save"]}
DEFAULTS={"refresh_pcg_components":{"actor_name":"","selected_only":False,"cleanup":True,"generate":True,"refresh":False,"force":True,"wait_until_complete":False,"timeout_seconds":10,"poll_interval_seconds":0.05,"min_generated_output_data_count":-1,"min_managed_resource_count":-1,"max_components":1000,"native_first":True},"bind_pcg_generation_settings_to_blueprint":{"component_variable":"ForestPCG","generation_trigger_variable":"PCGGenerationTrigger","regenerate_in_editor_variable":"PCGRegenerateInEditor","generate_on_drop_variable":"","generate_on_drop_default":False,"graph_name":"UserConstructionScript","graph_id":"","graph_type":"function","replace_existing":True,"sync_component_template_defaults":True,"function_class":"","function_name":"ApplyPCGGenerationSettings","compile":False,"save":False}}
REQUIRED={"refresh_pcg_components":[],"bind_pcg_generation_settings_to_blueprint":["blueprint_name"]}
FIELD_TYPES={"refresh_pcg_components":{"actor_name":"string","selected_only":"boolean","cleanup":"boolean","generate":"boolean","refresh":"boolean","force":"boolean","wait_until_complete":"boolean","timeout_seconds":"number","poll_interval_seconds":"number","min_generated_output_data_count":"integer","min_managed_resource_count":"integer","max_components":"integer","native_first":"boolean"},"bind_pcg_generation_settings_to_blueprint":{"blueprint_name":"string","component_variable":"string","generation_trigger_variable":"string","regenerate_in_editor_variable":"string","generate_on_drop_variable":"string","generate_on_drop_default":"boolean","graph_name":"string","graph_id":"string","graph_type":"string","replace_existing":"boolean","sync_component_template_defaults":"boolean","function_class":"string","function_name":"string","compile":"boolean","save":"boolean"}}
NULLABLE={method:[] for method in METHODS}
ITEMS={method:{} for method in METHODS}
OBSERVERS=[]
NO_JOURNAL_METHODS=[]
WRITERS=list(METHODS)
_ACTIVE_REQUEST=False
_FORBIDDEN_MCP_REFERENCE_TOKENS=['/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor','cubelessmcp','cubeless_mcp','modelcontextprotocol','toolsetregistry','/engine/pythontypes']


def binding_status():return api.binding_status()


def _dependency(value):
    if not isinstance(value,str):return False
    text=value.strip().strip('\"\'').replace('\\','/').lower()
    return any(token in text for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('pcg_component_generation_dependency_reference','Refusing MCP-only reference in '+field)
            error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('pcg_component_generation_public_argument_invalid',field+': '+message)
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
        return int(value)
    if type(value) is str:
        stripped=value.strip()
        if stripped.isascii() and re.fullmatch(r'[+-]?[0-9](?:_?[0-9])*(?:\.0+)?',stripped):return int(stripped.replace('_','').split('.')[0])
        raise _public_error(field,'int_parsing','Input should be a valid integer, unable to parse string as an integer')
    raise _public_error(field,'int_type','Input should be a valid integer')

def _public_float(field,value):
    if type(value) in (int,float,bool):
        try:return float(value)
        except (ValueError,OverflowError):raise _public_error(field,'float_type','Input should be a valid number')
    if type(value) is str:
        try:
            if not value.isascii():raise ValueError()
            return float(value)
        except (ValueError,OverflowError):raise _public_error(field,'float_parsing','Input should be a valid number, unable to parse string as a number')
    raise _public_error(field,'float_type','Input should be a valid number')

def normalize_public_mcp_arguments(method,arguments):
    """Field-specific original FastMCP preparse/Pydantic profile; no Engine/raw coercion clone."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('pcg_component_generation_public_argument_invalid','Known method and public argument object required')
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
        kind=FIELD_TYPES[method][field]
        if value is None and field in NULLABLE[method]:pass
        elif kind=='array':
            if not isinstance(value,(list,tuple)):raise _public_error(field,'list_type','Input should be a valid list')
            rows=[]
            for index,item in enumerate(value):
                try:
                    if ITEMS[method][field]=='string':
                        if type(item) is not str:raise _public_error(field,'string_type','Input should be a valid string')
                    elif ITEMS[method][field]=='integer':item=_public_int(field,item)
                    elif ITEMS[method][field]=='object':
                        if type(item) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
                    elif ITEMS[method][field]=='number':item=_public_float(field,item)
                    elif ITEMS[method][field]!='any':raise OperationError('pcg_component_generation_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='object':
            if type(value) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
            if any(type(key) is not str for key in value):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
        elif kind=='any':pass
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('pcg_component_generation_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values


def _arguments(method,arguments,legacy=None,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('pcg_component_generation_entry_invalid','Internal model selector must be bool')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('pcg_component_generation_entry_invalid','Raw Native and public model are separate entries')
        params={key:copy.deepcopy(legacy[key]) for key in PARAM_FIELDS[method] if key in legacy}
        api._carrier(params)
        return dict(params=params,values=None,entry='raw_native',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    if method=='refresh_pcg_components':
        params={key:copy.deepcopy(values[key]) for key in PUBLIC_FIELDS[method] if key not in ('actor_name','native_first')}
        params['wait_until_complete']=False
        if values['actor_name']:params['actor_label_prefix']=values['actor_name']
    else:
        if _dependency(values['function_class']):raise OperationError('pcg_component_generation_dependency_reference','Refusing MCP-only function_class reference')
        params={key:copy.deepcopy(values[key]) for key in PUBLIC_FIELDS[method]}
        for key in ('function_class','generate_on_drop_variable'):
            if not params[key]:params.pop(key)
    api._carrier(params)
    return dict(params=params,values=values,entry='named_mcp' if public_input else 'direct_python',public_input=public_input),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),entry_route=args['entry'],
        public_argument_validation_applied=args['public_input'],legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),
        Named_actor_name_is_label_prefix=True,raw_actor_name_is_exact_native_selector=True,
        Named_initial_wait_false_external_wait_is_separate=True,raw_wait_is_single_frame_only=True,
        fallback_classname_PCG_substring_and_noarg_methods_preserved=True,
        automatic_action_retry=False,atomic_rollback=False,shared_PCG_task_cancelled=False,
        registration_dependency='ToolsetAPI',operation_dependency='dedicated_EngineAPI_and_finite_Python_Engine_fallback',server_RPC_dependency=False,
        actual_named_wire_verified=False,full_original_pipeline_complete=False)


def _initialize(result):
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,
        execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,
        observed_changed=False,SDK_global_purity_unverified=False,native_call_count=0,native_receipts=[],
        fallback_attempted=False,fallback_mutation_attempted=False,operation_complete=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched')
    result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')


def _refresh_changes(result):
    data=result['data'];known=data['change_state_known'];attempted=data['native_mutation_attempted'] or data['fallback_mutation_attempted']
    data.update(execution_mutation_attempted=attempted,mutation_state_unknown=not known,mutation_performed=data['observed_changed'] if known else None)
    result['changes']=[] if known and not attempted and not data['observed_changed'] else [dict(kind='pcg_component_generation',attempted=attempted,changed=data['mutation_performed'],change_state_known=known)]
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_PCG_outcome_no_replay' if not known else 'inspect_current_PCG_state_before_new_call' if attempted else 'none_observed')


def _checkpoint(result,callback):
    if callback:callback(result)


def _adopt(result,native,role):
    data=result['data'];data['native_result_returned']=True;data['native_call_count']+=1
    scalar={key:copy.deepcopy(value) for key,value in native.items() if key not in ('data','_raw_native_json')}
    if role in ('initial','raw','bind'):
        data['native_receipts'].append(dict(role=role,actual=scalar))
    else:data['last_observation_actual']=scalar
    data['native_mutation_attempted']|=native.get('mutation_attempted') is True
    data['observed_changed']|=native.get('changed') is True
    data['change_state_known']&=native.get('change_state_known') is True and type(native.get('changed')) is bool
    native_data=native.get('data',{})
    if type(native_data) is dict:
        if role in ('initial','raw','bind'):
            data['native_operation_data']={key:copy.deepcopy(value) for key,value in native_data.items() if key!='legacy_result'}
        else:data['last_query_data']={key:copy.deepcopy(value) for key,value in native_data.items() if key!='legacy_result'}
        data['last_native_legacy_result']=copy.deepcopy(native_data.get('legacy_result'))
        if role in ('initial','raw') and native_data.get('scoped_cleanup_generate') is True:
            # Preserve actual initial phase receipts even if generation later fails.
            for key in ('PCG_cleanup_completed','PCG_generation_completed'):
                if key in native_data:data[key]=copy.deepcopy(native_data[key])
            if native.get('change_state_known') is not True:
                data.update(Python_context_lost=True,projection_skipped_unsafe=True)
        data['SDK_global_purity_unverified']|=native_data.get('SDK_global_purity_unverified') is True
        for key in ('actual_mutation_prefix','actual_compile_prefix','actual_save_prefix','actual_partial_state','native_call_counts'):
            if key in native_data:
                if role in ('initial','raw','bind'):data[key]=copy.deepcopy(native_data[key])
                else:data.setdefault('last_query_prefix',{})[key]=copy.deepcopy(native_data[key])
    for source,target in (('compile_requested','requested'),('compile_attempted','attempted')):result['compile'][target]|=native.get(source) is True
    if native.get('compiled') is None or result['compile']['compiled'] is None:result['compile']['compiled']=None
    elif native.get('compiled') is True:result['compile']['compiled']=True
    for source,target in (('save_requested','requested'),('save_attempted','attempted'),('saved','saved')):result['save'][target]|=native.get(source) is True
    if role=='bind':
        # Actual compiler/save receipts are independent of final binding health.
        # In particular, returned compiler error0 does not prove child reacquisition.
        cp=native_data.get('actual_compile_prefix') if type(native_data) is dict else None
        sp=native_data.get('actual_save_prefix') if type(native_data) is dict else None
        cp=cp if type(cp) is dict else {};sp=sp if type(sp) is dict else {}
        result['compile']['native_receipt']=copy.deepcopy(cp)
        result['save']['native_receipt']=copy.deepcopy(sp)
        result['compile']['native_returned']=cp.get('returned') if type(cp.get('returned')) is bool else None
        result['compile']['postcompile_verification']='passed' if cp.get('postcompile_reacquire_passed') is True else 'failed' if native.get('success') is False and cp.get('returned') is True else 'not_proven'
        legacy=native_data.get('legacy_result') if type(native_data) is dict else None
        if type(legacy) is dict and 'compile_status' in legacy:
            result['compile']['native_legacy_status']=copy.deepcopy(legacy['compile_status'])
        if not result['compile']['requested']:compile_status='not_requested'
        elif not result['compile']['attempted']:compile_status='not_attempted_prior_failure' if native.get('success') is False else 'not_attempted'
        elif native.get('compiled') is True:compile_status='compiled_native_result'
        elif native.get('compiled') is False:compile_status='returned_compile_failed' if cp.get('returned') is True else 'compile_failed'
        elif cp.get('returned') is True:compile_status='returned_postcompile_verification_failed' if native.get('success') is False else 'returned_compile_outcome_unverified'
        else:compile_status='attempted_compile_outcome_unverified'
        result['compile']['status']=compile_status
        if result['save']['saved']:
            save_status='saved' if sp.get('SDK_save_success') is True and sp.get('dirty_after') is False else 'save_returned_true_postcondition_unverified'
        elif result['save']['attempted']:
            save_status='save_returned_false_or_postcondition_failed' if sp.get('SDK_save_returned') is True else 'attempted_save_outcome_unverified'
        elif result['save']['requested']:save_status='not_attempted_prior_failure' if native.get('success') is False else 'not_attempted'
        else:save_status='not_requested'
        result['save']['status']=save_status
    _refresh_changes(result)


def _native(result,method,params,role,callback=None):
    data=result['data'];prior_known=data['change_state_known']
    result['stage']='before_pcg_component_generation_checkpoint';_checkpoint(result,callback)
    data.update(native_dispatch_pending=True,change_state_known=False);_refresh_changes(result)
    try:_checkpoint(result,callback)
    except Exception:
        data.update(native_dispatch_pending=False,change_state_known=prior_known);_refresh_changes(result);raise
    data.update(native_dispatch_pending=False,native_dispatched=True)
    try:native=getattr(api,method)(params)
    except Exception as error:
        if getattr(error,'native_call_dispatched',None) is False:data.update(native_dispatched=False,change_state_known=prior_known)
        if getattr(error,'native_call_returned',False):data['undecoded_native_raw_result']=getattr(error,'native_raw_result',None)
        _refresh_changes(result);result['stage']='returned_pcg_component_generation_operation';_checkpoint(result,callback);raise
    data['change_state_known']=prior_known
    _adopt(result,native,role)
    if role in ('initial','raw','bind') and type(native.get('data',{}).get('legacy_result')) is dict:
        data['legacy_result']=copy.deepcopy(native['data']['legacy_result'])
    result['stage']='returned_pcg_component_generation_operation'
    try:_checkpoint(result,callback)
    except Exception as error:error.native_decoded_result=native;raise
    if native.get('success') is not True or native.get('postcondition_passed') is not True:
        error=OperationError(native.get('error_code') or 'pcg_component_generation_native_failed',native.get('error') or 'Native PCG postcondition failed')
        error.native_decoded_result=native;raise error
    if native.get('operation_complete') is False or native.get('data',{}).get('operation_complete') is False:
        data.update(change_state_known=False,operation_complete=False);_refresh_changes(result)
        raise OperationError('pcg_component_generation_native_incomplete','Actual partial PCG receipt is incomplete; no replay')
    if method=='refresh_pcg_components' and native.get('data',{}).get('scoped_cleanup_generate') is True and native.get('change_state_known') is not True:
        error=OperationError('pcg_component_generation_composite_outcome_unverified','Scoped cleanup-then-generate success lacks known current state; actual phase receipts preserved')
        error.native_decoded_result=native;raise error
    if method=='refresh_pcg_components' and (any(native.get(key) is not False for key in ('compile_requested','compile_attempted','save_requested','save_attempted','saved')) or native.get('compiled') is not False):
        raise OperationError('pcg_component_generation_refresh_compile_save_violation','Refresh explicit compile/save must be False; actual prefix preserved')
    return native


def _native_leaf(native):
    leaf=native.get('data',{}).get('legacy_result')
    if type(leaf) is not dict:raise OperationError('pcg_component_generation_legacy_invalid','Exact Native legacy object required')
    return copy.deepcopy(leaf)


def _with_marker(native):
    leaf=_native_leaf(native)
    leaf.setdefault('success',True);leaf.update(native_command_used=True,native_command='refresh_pcg_components',
        native_response={key:copy.deepcopy(value) for key,value in native.items() if key!='_raw_native_json'})
    return leaf


def _int_or_default(value,default):
    try:return int(value)
    except (TypeError,ValueError):return default


def _refresh_operation_receipt(data,source=None,previous=None,query_only=None):
    # Copied JSON only. This feature-local receipt correlates the current refresh,
    # independently of old bGenerated/session/output and the immutable source proof.
    receipt=data.get('scoped_refresh_operation') if type(data) is dict else None
    if type(receipt) is not dict:raise ValueError('Current refresh operation receipt is missing')
    token=receipt.get('token');identity=receipt.get('component_identity')
    if (type(token) is not str or len(token)!=32 or any(c not in '0123456789abcdefABCDEF' for c in token)
            or type(identity) is not str or not identity or len(identity)>4096
            or receipt.get('status') not in ('pending','succeeded','aborted','unknown')
            or any(type(receipt.get(k)) is not bool for k in ('query_only','started_observed','generated_event_observed','cancelled_or_aborted_event_observed','refresh_task_id_available'))
            or type(receipt.get('refresh_task_id')) is not str or receipt['refresh_task_id']!=''
            or receipt['refresh_task_id_available'] is not False
            or type(receipt.get('generation_task_id')) is not str or len(receipt['generation_task_id'])>128):
        raise ValueError('Typed current refresh token/identity/task/event observation required')
    if query_only is not None and receipt['query_only'] is not query_only:
        raise ValueError('Refresh operation query role differs from the actual dispatch role')
    if source is not None:
        roles=source.get('objects') if type(source) is dict else None
        components=[r for r in roles if type(r) is dict and r.get('role')=='component'] if type(roles) is list else []
        if len(components)!=1 or components[0].get('identity')!=identity:
            raise ValueError('Refresh operation component differs from the captured source identity')
    if previous is not None:
        if token!=previous['token'] or identity!=previous['component_identity']:
            raise ValueError('Refresh operation token or component identity changed')
        if previous['generation_task_id'] and receipt['generation_task_id']!=previous['generation_task_id']:
            raise ValueError('Refresh generation task identity changed')
        for key in ('started_observed','generated_event_observed','cancelled_or_aborted_event_observed'):
            if previous[key] and not receipt[key]:raise ValueError('Refresh event observation was lost')
        if previous['status'] in ('succeeded','aborted') and receipt['status']!=previous['status']:
            raise ValueError('Terminal refresh observation changed')
    if receipt['started_observed'] and not receipt['generation_task_id']:
        raise ValueError('Started refresh lacks the observed public generation task ID')
    if receipt['status']=='succeeded' and (receipt['started_observed'] is not True
            or receipt['generated_event_observed'] is not True or receipt['cancelled_or_aborted_event_observed'] is not False):
        raise ValueError('Refresh success lacks this operation generated event')
    if receipt['status']=='aborted' and receipt['cancelled_or_aborted_event_observed'] is not True:
        raise ValueError('Refresh terminal failure lacks its cancellation/abort event')
    if receipt['status']=='pending' and (receipt['generated_event_observed'] or receipt['cancelled_or_aborted_event_observed']):
        raise ValueError('Pending refresh has contradictory terminal events')
    if receipt['status']=='unknown':raise ValueError('Current refresh completion observation is unknown; no cancellation is inferred')
    return copy.deepcopy(receipt)


def _native_refresh_result_ready(result):
    response=result.get('native_response')
    data=response.get('data') if type(response) is dict else None
    if type(data) is dict and (data.get('scoped_generate_only') is True or data.get('scoped_immediate_cleanup_only') is True or data.get('scoped_cleanup_generate') is True or data.get('scoped_refresh_only') is True) and result.get('wait_completed') is not True:return False
    if type(data) is dict and data.get('scoped_refresh_only') is True:
        try:receipt=_refresh_operation_receipt(data,data.get('scoped_generate_source'))
        except (ValueError,KeyError,TypeError):return False
        if receipt['status']!='succeeded':return False
    scoped_generation=type(data) is dict and any(data.get(key) is True for key in ('scoped_generate_only','scoped_cleanup_generate','scoped_refresh_only'))
    components=result.get('components',[])
    if not isinstance(components,list):return bool(result.get('wait_completed',False))
    if not components:return _int_or_default(result.get('component_count'),0)==0
    min_output=_int_or_default(result.get('min_generated_output_data_count'),-1)
    min_resources=_int_or_default(result.get('min_managed_resource_count'),-1)
    for component in components:
        if not isinstance(component,dict):return False
        # A serialized output/bGenerated from a previous Editor session is not
        # completion of this request. Native also requires this session's post-generate.
        if scoped_generation and (component.get('generated') is not True or component.get('was_generated_this_session') is not True):return False
        if component.get('generating') or component.get('cleaning_up') or component.get('refresh_in_progress'):return False
        if min_output>=0 and _int_or_default(component.get('generated_output_data_count'),0)<min_output:return False
        if min_resources>=0 and _int_or_default(component.get('managed_resource_count'),0)<min_resources:return False
    return True



def _query_params():
    return dict(cleanup=False,generate=False,refresh=False,force=False,notify_properties_changed=False,
        cancel_existing_refresh=False,remove_components=False,immediate_cleanup=False,selected_only=False,
        wait_until_complete=False,max_components=1)


def _admission(result,callback=None):
    native=_native(result,'refresh_pcg_components',_query_params(),'admission',callback)
    context=native.get('data',{}).get('execution_context')
    fields=('editor_path','editor_class','world_path','world_class')
    if type(context) is not dict or any(type(context.get(key)) is not str or not context[key] for key in fields):
        raise OperationError('pcg_python_context_receipt_unavailable','Exact safe Native Editor/world admission metadata is required before Python UObject work')
    result['data']['last_execution_context']=copy.deepcopy(context)
    return context


def _valid_object(obj):
    return obj is not None and unreal.SystemLibrary.is_valid(obj) is True


class _ObjectAnchor:
    # Python's UObject wrappers retain actual TObjectPtr references. No reload,
    # copied path retargeting or ownership transfer is used between ticks.
    def __init__(self,obj,world,actor=None):
        if not _valid_object(obj):raise OperationError('pcg_python_object_unavailable','Captured object is not live')
        self.obj=obj;self.world=world;self.actor=actor
        self.path=obj.get_path_name();self.cls=obj.get_class();self.outer=obj.get_outer()
        self.package=obj.get_package();self.outermost=obj.get_outermost();self.label=None
        if obj.get_world()!=world:raise OperationError('pcg_python_world_changed','Captured object is outside the exact Editor world')
        if actor is not None and obj.get_owner()!=actor:raise OperationError('pcg_python_owner_changed','Component owner differs')

    def same(self):
        obj=self.obj
        if not _valid_object(obj) or not _valid_object(self.cls):return False
        if (obj.get_path_name()!=self.path or obj.get_class()!=self.cls or obj.get_outer()!=self.outer
            or obj.get_package()!=self.package or obj.get_outermost()!=self.outermost or obj.get_world()!=self.world):return False
        if self.actor is not None:
            if not _valid_object(self.actor) or obj.get_owner()!=self.actor:return False
            if obj not in self.actor.get_components_by_class(unreal.ActorComponent):return False
        elif self.label is not None and obj.get_actor_label(False)!=self.label:return False
        return True


class _PythonContext:
    def __init__(self,result,callback=None):
        self.result=result;self.callback=callback;self.anchors=[];self.lost=False
        self.metadata=_admission(result,callback)
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.actor_subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not _valid_object(self.editor) or not _valid_object(self.actor_subsystem):
            raise OperationError('pcg_python_context_unavailable','Exact public Editor subsystems are required')
        self.world=self.editor.get_editor_world()
        if not _valid_object(self.world):raise OperationError('pcg_python_world_unavailable','Editor world is unavailable')
        self.world_class=self.world.get_class();self.editor_class=self.editor.get_class()
        self.actor_subsystem_class=self.actor_subsystem.get_class()
        self.native_editor=unreal.find_object(None,self.metadata['editor_path'],follow_redirectors=False)
        if not _valid_object(self.native_editor):raise OperationError('pcg_python_editor_unavailable','Captured Native Editor object is not loaded')
        self.native_editor_shape=(self.native_editor.get_class(),self.native_editor.get_outer(),self.native_editor.get_package(),self.native_editor.get_outermost())
        self.admit()

    def admit(self):
        if self.lost:raise OperationError('pcg_python_context_lost','Context loss is sticky; no further UObject work or retry')
        try:
            current=_admission(self.result,self.callback)
            if current!=self.metadata:raise OperationError('pcg_python_context_changed','Native Editor/world identity receipt changed')
            if (not _valid_object(self.editor) or not _valid_object(self.actor_subsystem) or not _valid_object(self.world)
                or self.editor.get_class()!=self.editor_class or self.actor_subsystem.get_class()!=self.actor_subsystem_class or self.world.get_class()!=self.world_class
                or not _valid_object(self.native_editor)
                or unreal.find_object(None,self.metadata['editor_path'],follow_redirectors=False)!=self.native_editor
                or self.native_editor.get_path_name()!=self.metadata['editor_path']
                or self.native_editor.get_class().get_path_name()!=self.metadata['editor_class']
                or (self.native_editor.get_class(),self.native_editor.get_outer(),self.native_editor.get_package(),self.native_editor.get_outermost())!=self.native_editor_shape
                or self.editor.get_editor_world()!=self.world
                or self.world.get_path_name()!=self.metadata['world_path'] or self.world_class.get_path_name()!=self.metadata['world_class']):
                raise OperationError('pcg_python_context_changed','Captured exact Editor world differs')
            if any(not anchor.same() for anchor in self.anchors):
                raise OperationError('pcg_python_target_identity_changed','Borrowed target owner/class/path/package/world/membership changed')
        except Exception:
            self.lost=True
            self.result['data'].update(change_state_known=False,Python_context_lost=True,projection_skipped_unsafe=True)
            _refresh_changes(self.result)
            raise

    def call(self,function):
        self.admit()
        value=function()
        self.admit()
        return value

    def actor(self,actor):
        self.admit()
        anchor=_ObjectAnchor(actor,self.world);self.anchors.append(anchor)
        self.admit();return anchor

    def component(self,component,actor):
        self.admit()
        anchor=_ObjectAnchor(component,self.world,actor);self.anchors.append(anchor)
        self.admit();return anchor

    def _scoped_source_observation(self,leaf,phase,cleanup=False,composite=False):
        # Pure copied proof comparison; no target/source SDK observation here.
        scope='cleanup_generate' if composite else 'cleanup' if cleanup else 'generate'
        attribute='scoped_'+scope+'_source'
        if phase=='poll' and getattr(self,attribute,None) is None:return None
        marker='scoped_cleanup_generate' if composite else 'scoped_immediate_cleanup_only' if cleanup else 'scoped_generate_only'
        source_key='scoped_cleanup_generate_source_after' if composite else 'scoped_cleanup_source_after' if cleanup else 'scoped_generate_source'
        code='pcg_wait_scoped_source_invalid'
        try:
            response=leaf.get('native_response') if type(leaf) is dict else None
            data=response.get('data') if type(response) is dict else None
            if not cleanup and not composite and type(data) is dict and data.get('scoped_refresh_only') is True:
                marker='scoped_refresh_only'
            if type(data) is not dict or marker not in data or data[marker] is False:
                if phase=='initial':return None
                raise ValueError('Query lost the initial scoped '+scope+' receipt')
            if data[marker] is not True:raise ValueError('Scoped marker must be a JSON boolean')
            source=data.get(source_key)
            if type(source) is not dict or set(source)!={'profile','objects','source_authored','source_files'}:
                raise ValueError('Exact scoped stable source object required')
            if source['profile'] not in ('owned_editor_cpu_createpoints_v1','owned_editor_cpu_weighted_ism_v1','owned_editor_cpu_createpoints_refresh_v1','owned_editor_cpu_point_pipeline_v1','owned_editor_cpu_point_pipeline_refresh_v1'):raise ValueError('Unknown scoped source profile')
            objects=source['objects'];files=source['source_files']
            if type(objects) is not list or not objects or type(source['source_authored']) is not dict or not source['source_authored']:
                raise ValueError('Observed object roles and authored source are required')
            for row in objects:
                if type(row) is not dict or any(type(row.get(key)) is not str for key in ('role','path','class_path','outer_path','package_path','identity')):
                    raise ValueError('Typed observed object identity strings required')
                if any(not row[key] for key in ('role','path','class_path','package_path','identity')):
                    raise ValueError('Observed object identity must not be empty')
            if type(files) is not list or not files:raise ValueError('Observed saved source files required')
            for row in files:
                if (type(row) is not dict or type(row.get('path')) is not str or not row['path']
                        or row.get('exists') is not True or type(row.get('bytes')) is not int or row['bytes']<0
                        or type(row.get('sha1')) is not str or len(row['sha1'])!=40
                        or any(char not in '0123456789abcdefABCDEF' for char in row['sha1'])):
                    raise ValueError('Typed current source file receipt required')
            if len(files)==1:
                # AEE's native Volume has no source Blueprint or mesh file to fabricate.
                # Only this exact captured host/ownership proof admits one actual graph file.
                volume=source['source_authored'].get('native_volume_host')
                roles={row['role']:row for row in objects}
                expected={'actor':'/Script/PCG.PCGVolume','component':'/Script/PCG.PCGComponent',
                    'volume_root':'/Script/Engine.BrushComponent','volume_model':'/Script/Engine.Model',
                    'volume_polys':'/Script/Engine.Polys','source_graph':'/Script/PCG.PCGGraph'}
                if (source['profile']=='owned_editor_cpu_weighted_ism_v1' or type(volume) is not dict
                        or volume.get('kind')!='native_pcg_volume' or len(roles)!=len(objects)
                        or any(roles.get(role,{}).get('class_path')!=cls for role,cls in expected.items())
                        or roles['component']['outer_path']!=roles['actor']['path']
                        or roles['volume_root']['outer_path']!=roles['actor']['path']
                        or roles['volume_model']['outer_path']!=roles['actor']['path']
                        or roles['volume_polys']['outer_path']!=roles['volume_model']['path']
                        or volume.get('model')!=roles['volume_model']['identity']
                        or volume.get('polys')!=roles['volume_polys']['identity']):
                    raise ValueError('Single source file requires the exact captured native PCGVolume/Brush/Model/Polys ownership proof')
                graph_package=roles['source_graph']['package_path']
                if (not graph_package.startswith('/Game/') or not files[0]['path'].replace('\\','/').casefold().endswith('/'+graph_package[6:].casefold()+'.uasset')):
                    raise ValueError('Single Volume source file must identify the actual saved /Game graph')
            encoded=json.dumps(source,ensure_ascii=False,sort_keys=True,allow_nan=False,separators=(',',':'))
            def captured_history(state):
                # Immutable copied pre-call observation, never a reconstructed baseline.
                fresh=data.get('scoped_cleanup_started_never_generated')
                if (type(fresh) is not bool or type(state) is not dict
                        or type(state.get('generated')) is not bool or type(state.get('was_generated_this_session')) is not bool):
                    raise ValueError('Typed captured generation origin and actual session history required')
                history=state['was_generated_this_session']
                if fresh:
                    if state['generated'] is not False or history is not False:
                        raise ValueError('Never-generated origin requires generated/session false')
                    origin='never_generated_empty'
                else:
                    if state['generated'] is not True:raise ValueError('Existing generated origin requires generated true')
                    origin='generated_this_session' if history else 'existing_generated_no_session_history'
                if data.get('scoped_cleanup_origin')!=origin:
                    raise ValueError('Captured generation origin does not match actual pre-call history')
                return history
            if not cleanup and leaf.get('wait_completed') is True:
                rows=leaf.get('components')
                if (type(rows) is not list or not rows or any(type(row) is not dict
                        or row.get('generated') is not True or row.get('was_generated_this_session') is not True for row in rows)):
                    raise ValueError('Scoped generation completion requires actual current generated/session true, not cached output')
            if source['profile'] in ('owned_editor_cpu_createpoints_refresh_v1','owned_editor_cpu_point_pipeline_refresh_v1'):
                if cleanup or composite or marker!='scoped_refresh_only':raise ValueError('Refresh source requires its exact single-operation marker')
                receipt=_refresh_operation_receipt(data,source,
                    getattr(self,'scoped_refresh_operation',None) if phase=='poll' else None,phase=='poll')
                if phase=='initial' and receipt['status']!='pending':
                    raise ValueError('Initial refresh must preserve its actual pending observation')
                if leaf.get('wait_completed') is True and receipt['status']!='succeeded':
                    raise ValueError('Old generated/output cannot prove completion of this refresh')
                if phase=='initial':
                    captured_history(data.get('scoped_refresh_before'))
                    if (data.get('scoped_cleanup_started_never_generated') is not False
                            or data.get('scoped_refresh_dispatch_returned') is not True
                            or data.get('scoped_refresh_task_pending_observed') is not True
                            or leaf.get('wait_completed') is not False):
                        raise ValueError('Actual one-shot refresh dispatch and pending task observation required')
                elif (data.get('scoped_refresh_query_only') is not True
                        or getattr(self,'scoped_refresh_pending_observed',None) is not True
                        or type(getattr(self,'scoped_refresh_initial_session_history',None)) is not bool
                        or data.get('scoped_refresh_completed') is not (leaf.get('wait_completed') is True)):
                    raise ValueError('Refresh continuation requires original pending receipt and actual query-only completion')
            if composite:
                before=data.get('scoped_cleanup_generate_source_before')
                if (type(before) is not dict or json.dumps(before,ensure_ascii=False,sort_keys=True,allow_nan=False,separators=(',',':'))!=encoded
                        or data.get('scoped_cleanup_generate_source_preserved') is not True):
                    raise ValueError('Composite source before/after proof differs')
                if data.get('scoped_generate_only') is True or data.get('scoped_immediate_cleanup_only') is True:
                    raise ValueError('Composite scope must remain separate from single-operation scopes')
                if type(data.get('PCG_generation_completed')) is not bool:
                    raise ValueError('Typed generation completion observation required')
                if phase=='initial':
                    dispatch=data.get('scoped_cleanup_dispatch');generation=data.get('scoped_generate_dispatch')
                    after=data.get('scoped_cleanup_after')
                    expected_force=self.result['data'].get('effective_generation_force')
                    state_before=data.get('scoped_cleanup_before')
                    fresh=data.get('scoped_cleanup_started_never_generated')
                    expected_session=captured_history(state_before)
                    if fresh:
                        if (source['profile'] not in ('owned_editor_cpu_createpoints_v1','owned_editor_cpu_weighted_ism_v1','owned_editor_cpu_point_pipeline_v1')
                                or any(state_before.get(key) is not False for key in ('generated','was_generated_this_session','generating','cleaning_up','refresh_in_progress'))
                                or state_before.get('managed_resources_accessible') is not True
                                or type(state_before.get('managed_resource_count')) is not int or state_before['managed_resource_count']!=0
                                or type(state_before.get('generated_output_data_count')) is not int or state_before['generated_output_data_count']!=0):
                            raise ValueError('Never-generated cleanup requires exact empty idle bounded CPU pre-state')
                        if source['profile']=='owned_editor_cpu_weighted_ism_v1':
                            survivors=source['source_authored'].get('surviving_owner_components')
                            if (any(type(state_before.get(key)) is not int or state_before[key]!=0 for key in ('captured_managed_row_count','captured_generated_component_count','captured_held_managed_count'))
                                    or state_before.get('managed_resource_details')!=[]
                                    or type(survivors) is not list or not survivors
                                    or any(type(row) is not dict or type(row.get('identity')) is not str or not row['identity'] for row in survivors)
                                    or type(state_before.get('captured_survivor_component_count')) is not int
                                    or state_before['captured_survivor_component_count']!=len(survivors)):
                                raise ValueError('Fresh Weighted cleanup requires empty captured selected resources and the complete original survivor inventory')
                    if (data.get('scoped_cleanup_generate_query_only') is not False or data.get('PCG_cleanup_completed') is not True
                            or leaf.get('wait_completed') is not False or data['PCG_generation_completed'] is not False
                            or type(dispatch) is not dict or dispatch.get('remove_components') is not True
                            or any(dispatch.get(key) is not True for key in ('entered','returned','completion_proven'))
                            or type(generation) is not dict
                            or any(generation.get(key) is not True for key in ('entered','returned','task_accepted'))
                            or type(expected_force) is not bool or generation.get('requested_force') is not expected_force
                            or generation.get('completion_proven') is not False
                            or type(after) is not dict
                            or any(after.get(key) is not False for key in ('generated','generating','cleaning_up','refresh_in_progress'))
                            or after.get('managed_resources_accessible') is not True
                            or after.get('was_generated_this_session') is not expected_session
                            or type(after.get('managed_resource_count')) is not int or after['managed_resource_count']!=0
                            or type(after.get('generated_output_data_count')) is not int or after['generated_output_data_count']!=0):
                        raise ValueError('Actual cleanup completion and separate accepted generation dispatch required')
                elif (data.get('scoped_cleanup_generate_query_only') is not True or data.get('PCG_cleanup_completed') is not False
                        or data['PCG_generation_completed'] is not (leaf.get('wait_completed') is True)):
                    raise ValueError('Composite wait must remain query-only with actual generation readiness')
            if cleanup:
                before=data.get('scoped_cleanup_source_before')
                if (type(before) is not dict or json.dumps(before,ensure_ascii=False,sort_keys=True,allow_nan=False,separators=(',',':'))!=encoded
                        or data.get('scoped_cleanup_source_preserved') is not True):
                    raise ValueError('Cleanup source before/after proof differs')
                if data.get('scoped_generate_only') is True or data.get('PCG_generation_completed') is not False:
                    raise ValueError('Cleanup completion must be separate from generation completion')
                if phase=='initial':
                    dispatch=data.get('scoped_cleanup_dispatch')
                    expected_session=captured_history(data.get('scoped_cleanup_before'))
                    after=data.get('scoped_cleanup_after')
                    if (data.get('scoped_cleanup_started_never_generated') is not False
                            or data.get('PCG_cleanup_completed') is not True or type(dispatch) is not dict
                            or type(dispatch.get('remove_components')) is not bool
                            or any(dispatch.get(key) is not True for key in ('entered','returned','completion_proven'))
                            or type(after) is not dict or after.get('was_generated_this_session') is not expected_session
                            or after.get('generated') is not (not dispatch['remove_components'])
                            or any(after.get(key) is not False for key in ('generating','cleaning_up','refresh_in_progress'))):
                        raise ValueError('Actual synchronous cleanup dispatch and preserved captured history required')
                else:
                    history=getattr(self,'scoped_cleanup_session_history',None)
                    after=data.get('scoped_cleanup_after')
                    if (data.get('scoped_cleanup_query_only') is not True or data.get('PCG_cleanup_completed') is not False
                            or type(data.get('scoped_cleanup_state_ready')) is not bool or type(history) is not bool
                            or data.get('scoped_cleanup_expected_session_history') is not history
                            or type(after) is not dict or after.get('was_generated_this_session') is not history):
                        raise ValueError('Cleanup wait must remain query-only with the original captured session history')
            if phase=='poll' and encoded!=getattr(self,attribute+'_json'):
                code='pcg_wait_scoped_source_changed'
                raise ValueError('Query stable source differs from the initial Native receipt')
            return source,encoded
        except Exception as error:
            self.lost=True
            self.result['data'].update(change_state_known=False,Python_context_lost=True,projection_skipped_unsafe=True)
            self.result['data']['scoped_'+scope+'_observation_failure']=dict(error_code=code,phase=phase,
                property=source_key,source_values_queried_after_failure=False,
                initial_receipt='data.native_operation_data',query_receipt='data.last_query_data')
            _refresh_changes(self.result)
            raise OperationError(code,str(error)) from error

    def capture_native_components(self,leaf):
        proof=self._scoped_source_observation(leaf,'initial')
        self.scoped_generate_source=copy.deepcopy(proof[0]) if proof is not None else None
        self.scoped_generate_source_json=proof[1] if proof is not None else None
        initial_data=leaf.get('native_response',{}).get('data',{})
        refresh_source=proof is not None and proof[0]['profile'] in ('owned_editor_cpu_createpoints_refresh_v1','owned_editor_cpu_point_pipeline_refresh_v1')
        self.scoped_refresh_pending_observed=initial_data.get('scoped_refresh_task_pending_observed') if refresh_source else None
        self.scoped_refresh_initial_session_history=initial_data['scoped_refresh_before']['was_generated_this_session'] if refresh_source else None
        self.scoped_refresh_operation=_refresh_operation_receipt(initial_data,proof[0],query_only=False) if refresh_source else None
        cleanup=self._scoped_source_observation(leaf,'initial',cleanup=True)
        self.scoped_cleanup_source=copy.deepcopy(cleanup[0]) if cleanup is not None else None
        self.scoped_cleanup_source_json=cleanup[1] if cleanup is not None else None
        self.scoped_cleanup_remove_components=leaf['native_response']['data']['scoped_cleanup_dispatch']['remove_components'] if cleanup is not None else None
        self.scoped_cleanup_session_history=initial_data['scoped_cleanup_before']['was_generated_this_session'] if cleanup is not None else None
        composite=self._scoped_source_observation(leaf,'initial',composite=True)
        self.scoped_cleanup_generate_source=copy.deepcopy(composite[0]) if composite is not None else None
        self.scoped_cleanup_generate_source_json=composite[1] if composite is not None else None
        rows=leaf.get('components')
        if type(rows) is not list:raise OperationError('pcg_wait_identity_unavailable','Initial component list is required for deferred exact observation')
        paths=[]
        # The preceding Native return is guarded. find_object explicitly avoids
        # loads/redirectors; wrapper references are retained immediately.
        for row in rows:
            if type(row) is not dict or type(row.get('actor_path')) is not str or type(row.get('component_path')) is not str:
                raise OperationError('pcg_wait_identity_unavailable','Exact actor/component paths are required')
            actor=unreal.find_object(None,row['actor_path'],follow_redirectors=False)
            component=unreal.find_object(None,row['component_path'],follow_redirectors=False)
            a=self.actor(actor);c=self.component(component,actor)
            if a.path!=row['actor_path'] or c.path!=row['component_path']:
                raise OperationError('pcg_wait_identity_changed','Initial loaded component path differs')
            paths.append(c.path)
        if len(set(paths))!=len(paths):raise OperationError('pcg_wait_identity_invalid','Duplicate component rows are not a target identity set')
        self.component_paths=tuple(paths)

    def verify_observation(self,leaf):
        self._scoped_source_observation(leaf,'poll')
        self._scoped_source_observation(leaf,'poll',cleanup=True)
        self._scoped_source_observation(leaf,'poll',composite=True)
        self.admit()
        rows=leaf.get('components')
        if type(rows) is not list or tuple(row.get('component_path') for row in rows if type(row) is dict)!=self.component_paths:
            raise OperationError('pcg_wait_target_set_changed','Deferred observation cannot adopt another component set/order')


def _fallback(result,values,initial_native_response,callback=None,context=None):
    if values['wait_until_complete']:
        raise OperationError('pcg_python_fallback_async_wait_unsupported','Original fallback has wait_supported=False; genuine completion cannot be claimed, no fallback writer dispatched')
    context=context or _PythonContext(result,callback)
    result['data']['fallback_attempted']=True
    methods=[]
    if values['cleanup']:methods.append('cleanup')
    if values['refresh']:methods.extend(['refresh','dirty_generated'])
    if values['generate']:methods.append('generate')
    if not methods:methods=['refresh']
    actors=context.call(lambda:context.actor_subsystem.get_selected_level_actors() if values['selected_only'] else context.actor_subsystem.get_all_level_actors())
    processed=[];errors=[];limit_hit=False;limit=int(values['max_components'])
    result['data']['fallback_processed_prefix']=[];result['data']['fallback_errors']=errors
    for actor in list(actors):
        if len(processed)>=limit:limit_hit=True;break
        actor_anchor=context.actor(actor)
        # The original default label getter can lazily create a label. Keep that
        # local side effect explicit rather than asserting a pure observer.
        previous=context.call(lambda:actor.get_actor_label(False))
        label=context.call(lambda:actor.get_actor_label())
        if not previous and label:
            result['data']['fallback_mutation_attempted']=True;result['data']['observed_changed']=True
            result['data']['lazy_actor_labels_created']=result['data'].get('lazy_actor_labels_created',0)+1
            _refresh_changes(result)
        actor_anchor.label=label
        path=actor_anchor.path
        if values['actor_name'] and values['actor_name'] not in label and values['actor_name'] not in path:continue
        components=context.call(lambda:actor.get_components_by_class(unreal.ActorComponent))
        for component in list(components):
            anchor=context.component(component,actor)
            class_name=anchor.cls.get_name()
            if 'PCG' not in class_name:continue
            if len(processed)>=limit:limit_hit=True;break
            called=[]
            row={'actor':label,'component':component.get_name(),'component_class':class_name,'called_methods':called}
            result['data']['fallback_processed_prefix'].append(row)
            for name in methods:
                context.admit();method=getattr(component,name,None)
                if not method:continue
                result['stage']='before_pcg_component_generation_checkpoint';_checkpoint(result,callback)
                result['data']['fallback_mutation_attempted']=True
                result['data']['change_state_known']=False;_refresh_changes(result)
                prefix=dict(actor=label,component_path=anchor.path,method=name,SDK_returned=False)
                result['data'].setdefault('fallback_call_prefix',[]).append(prefix)
                try:
                    method()  # Deliberately original no-argument behavior.
                    prefix['SDK_returned']=True;called.append(name)
                except Exception as error:
                    prefix.update(SDK_raised=True,error=str(error))
                    errors.append({'actor':label,'component':row['component'],'method':name,'error':str(error)})
                context.admit()
                result['stage']='returned_pcg_component_generation_operation';_checkpoint(result,callback)
            processed.append(row)
    legacy=dict(success=True,native_command_used=False,native_command='refresh_pcg_components',native_response=initial_native_response,
        fallback_reason='native unavailable or no matching native components',wait_requested=bool(values['wait_until_complete']),wait_supported=False,
        wait_note='Python fallback does not block for PCG completion because that can stall editor ticking; use native refresh_pcg_components for wait_until_complete.',
        count=len(processed),component_limit_hit=limit_hit,processed=processed,errors=errors)
    context.admit()
    result['data'].update(legacy_result=legacy,fallback_noarg_methods=methods,Python_fallback_returned=True)
    # Method return/errors are exact original results; opaque Engine callback
    # mutation is not promoted to a full target-state proof.
    if result['data']['fallback_mutation_attempted']:
        result['data'].update(fallback_target_change_unproven=True,change_state_known=False)
        _refresh_changes(result)
    return legacy



def _finish(result,leaf):
    result['data']['legacy_result']=leaf
    result['data']['legacy_Named_result_reference']='data.legacy_result'
    result['data']['operation_complete']=True
    result['stage']='returned_pcg_component_generation_operation'
    result['validation'].update(status='HD_target_scoped_Native_or_finite_fallback_receipt',
        native_or_fallback_is_atomic=False,full_PCG_generation_proved=False,disk_reload='not_run')
    if leaf.get('success') is False:
        raise OperationError(leaf.get('error_code') or 'pcg_component_generation_failed',leaf.get('error') or leaf.get('message') or 'PCG operation failed; actual prefix preserved')
    if not result['data']['change_state_known']:
        raise OperationError('pcg_component_generation_outcome_unverified','Actual PCG callback changes remain unverified; legacy result and no-replay prefix preserved')
    result.update(success=True,stage='complete')
    return result


def _start(result,method,args,original,legacy,callback=None,deferred=False):
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:raise OperationError('pcg_component_generation_reentry','Same-domain reentry is rejected before SDK work')
    _ACTIVE_REQUEST=True
    try:
        _context(result,method,args,original,legacy);_initialize(result)
        if method=='refresh_pcg_components' and legacy is None:
            result['data']['effective_generation_force']=args['values']['force']
        result['stage']='before_pcg_component_generation_checkpoint';_checkpoint(result,callback)
        if method=='bind_pcg_generation_settings_to_blueprint':
            native=_native(result,method,args['params'],'bind',callback)
            return _finish(result,_native_leaf(native)),None
        if legacy is not None:
            native=_native(result,method,args['params'],'raw',callback)
            result['data']['raw_single_frame_readback_only']=True
            return _finish(result,_native_leaf(native)),None
        values=args['values'];requested_wait=bool(values['wait_until_complete'])
        if requested_wait and not deferred:
            raise OperationError('pcg_component_generation_async_wait_required','Direct Python/ordinary Batch cannot resume external completion; no writer dispatched, use the Named async entry')
        if requested_wait and not values['native_first']:
            raise OperationError('pcg_python_fallback_async_wait_unsupported','Original fallback wait_supported=False; no fallback writer dispatched')
        # Capture the safe Editor world before an initial writer when a deferred
        # observer needs to retain exact component identity between frames.
        context=_PythonContext(result,callback) if requested_wait else None
        native=None
        if values['native_first']:
            try:native=_native(result,method,args['params'],'initial',callback)
            except Exception as error:
                if getattr(error,'native_call_dispatched',None) is not False or getattr(error,'code','')!='pcg_component_generation_native_api_unavailable':
                    response=getattr(error,'native_decoded_result',None)
                    result['data']['legacy_result']=dict(success=False,error='Native PCG refresh command failed before Python fallback',
                        error_code='native_pcg_refresh_failed',native_command_used=False,native_command='refresh_pcg_components',
                        native_response={key:copy.deepcopy(value) for key,value in response.items() if key!='_raw_native_json'} if type(response) is dict else result['data'].get('undecoded_native_raw_result'))
                    raise
        if native is not None:
            leaf=_with_marker(native)
            result['data']['legacy_result']=leaf
            if not values['actor_name'] or int(leaf.get('component_count',0))>0:
                if requested_wait:
                    context.capture_native_components(leaf)
                    # No later writers: checkpoint only meaningful async observations,
                    # not every exact safety-admission query.
                    context.callback=None
                    descriptor=dict(context=context,initial=copy.deepcopy(leaf),params=copy.deepcopy(args['params']),values=copy.deepcopy(values))
                    result['data'].update(operation_complete=False,deferred_completion_pending=True,initial_mutation_was_not_replayed=True)
                    return result,descriptor
                return _finish(result,leaf),None
        leaf=_fallback(result,values,{key:copy.deepcopy(value) for key,value in native.items() if key!='_raw_native_json'} if native else None,callback,context)
        return _finish(result,leaf),None
    except Exception as error:
        result['data'].setdefault('legacy_result',dict(success=False,error=str(error),error_code=getattr(error,'code','pcg_tool_failed')))
        raise
    finally:_ACTIVE_REQUEST=False


def _poll(result,descriptor,callback=None):
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:raise OperationError('pcg_component_generation_reentry','Reentrant observation is rejected')
    _ACTIVE_REQUEST=True
    try:
        context=descriptor['context'];context.admit()
        params=copy.deepcopy(descriptor['params'])
        params.update(cleanup=False,generate=False,refresh=False,force=False,notify_properties_changed=False,wait_until_complete=True)
        scoped=getattr(context,'scoped_generate_source',None) is not None
        cleanup_scoped=getattr(context,'scoped_cleanup_source',None) is not None
        composite_scoped=getattr(context,'scoped_cleanup_generate_source',None) is not None
        if scoped:
            params['_scoped_generate_expectation']=copy.deepcopy(context.scoped_generate_source)
            params['notify_properties_changed']=False
            if getattr(context,'scoped_refresh_operation',None) is not None:
                params['_scoped_refresh_operation']=context.scoped_refresh_operation['token']
        if cleanup_scoped:
            params['_scoped_cleanup_expectation']=copy.deepcopy(context.scoped_cleanup_source)
            params['_scoped_cleanup_remove_components']=context.scoped_cleanup_remove_components
            params['_scoped_cleanup_session_history']=context.scoped_cleanup_session_history
            params['notify_properties_changed']=False
        if composite_scoped:
            params['_scoped_cleanup_generate_expectation']=copy.deepcopy(context.scoped_cleanup_generate_source)
            params['notify_properties_changed']=False
        try:
            native=_native(result,'refresh_pcg_components',params,'poll',callback)
            leaf=_with_marker(native)
            context.verify_observation(leaf)
            if getattr(context,'scoped_refresh_operation',None) is not None:
                context.scoped_refresh_operation=_refresh_operation_receipt(native['data'],context.scoped_generate_source,context.scoped_refresh_operation,True)
        except Exception as error:
            # A verified Native terminal cancellation/abort is a failed operation,
            # not a lost lease. Validate only copied receipts; do not observe Engine
            # again after this terminal error, retry the writer or cancel any task.
            native=getattr(error,'native_decoded_result',None)
            if _verified_refresh_terminal_failure(result,context,native,error):raise
            if scoped or cleanup_scoped or composite_scoped:
                context.lost=True
                result['data'].update(change_state_known=False,Python_context_lost=True,projection_skipped_unsafe=True)
                result['data']['scoped_cleanup_generate_poll_stopped' if composite_scoped else 'scoped_cleanup_poll_stopped' if cleanup_scoped else 'scoped_generate_poll_stopped']=True
                _refresh_changes(result)
            raise
        result['data']['last_wait_native_result']=leaf
        return leaf
    finally:_ACTIVE_REQUEST=False


def _verified_refresh_terminal_failure(result,context,native,error):
    if getattr(context,'scoped_refresh_operation',None) is None or type(native) is not dict:return False
    data=native.get('data')
    if (native.get('error_code')!='pcg_scoped_refresh_aborted' or native.get('success') is not False
            or native.get('change_state_known') is not True or type(native.get('changed')) is not bool
            or native.get('operation_complete') is False or type(data) is not dict or data.get('operation_complete') is not True
            or data.get('scoped_refresh_only') is not True
            or data.get('scoped_refresh_terminal_observation_verified') is not True
            or data.get('typed_target_postcondition_passed') is not True
            or any(native.get(k) is not False for k in ('compile_requested','compile_attempted','compiled','save_requested','save_attempted','saved'))):return False
    try:
        receipt=_refresh_operation_receipt(data,context.scoped_generate_source,context.scoped_refresh_operation,True)
        if receipt['status']!='aborted':return False
        # The Native error retains its original legacy error object. A copied
        # observation leaf is used solely for the existing pure source/target seal.
        leaf=_with_marker(native)
        rows=data.get('after')
        if (type(rows) is not list or not rows or any(type(row) is not dict
                or any(row.get(k) is not False for k in ('generating','cleaning_up','refresh_in_progress')) for row in rows)):
            raise ValueError('Terminal refresh requires actual idle Native after rows')
        leaf.update(components=copy.deepcopy(rows),component_count=len(rows),wait_completed=False)
        context._scoped_source_observation(leaf,'poll')
        if tuple(row.get('component_path') for row in rows if type(row) is dict)!=context.component_paths:
            raise ValueError('Terminal refresh component set differs from the initial target')
        if context.lost or result['data'].get('change_state_known') is not True:return False
        context.scoped_refresh_operation=receipt
        result['data'].update(last_wait_native_result=leaf,scoped_refresh_terminal_failure_verified=True,
            scoped_refresh_terminal_status='aborted_or_cancelled',PCG_generation_may_still_be_in_flight=False)
        error.pcg_refresh_terminal_failure=True
        return True
    except Exception as validation_error:
        result['data']['scoped_refresh_terminal_validation_error']=str(validation_error)[:700]
        return False


def _wait_failure(result,descriptor,error):
    # Original completion-poll failure is a READ stage, after a separately
    # recorded initial mutation. The response is current typed Native data,
    # never reconstructed or relabelled as an original RPC/raw envelope.
    initial=descriptor['initial']
    completed=['refresh_pcg_components_initial'] if sum(max(0,_int_or_default(initial.get(key),0)) for key in ('cleanup_count','refresh_count','generate_count'))>0 else []
    response=getattr(error,'native_decoded_result',None)
    if type(response) is dict:response={key:copy.deepcopy(value) for key,value in response.items() if key!='_raw_native_json'}
    else:response=dict(error=str(error),error_code=getattr(error,'code','pcg_tool_failed'),outcome_unknown=result['data'].get('change_state_known') is not True)
    leaf=dict(success=False,status='error',error='Native PCG completion poll failed',message='Native PCG completion poll failed',
        error_code='native_pcg_completion_poll_failed',native_command_used=True,native_command='refresh_pcg_components',
        initial_native_response=initial.get('native_response'),failed_stage='poll_pcg_completion',failed_stage_effect='read',read_unreal_response=response)
    if getattr(error,'pcg_refresh_terminal_failure',False) is True:
        leaf.update(error_code='pcg_scoped_refresh_aborted',error='This refresh was aborted or cancelled',
            message='This refresh was aborted or cancelled; the cause is not distinguished by the Engine event',
            terminal_observation_verified=True,terminal_status='aborted_or_cancelled',
            pcg_generation_may_still_be_in_flight=False,do_not_retry=True)
    if completed:leaf.update(partial_mutation=True,mutation_performed=True,do_not_retry=True,completed_mutation_count=len(completed),completed_mutation_stages=completed)
    else:leaf.update(mutation_attempted=False,mutation_performed=False,changed=False)
    if result['data'].get('change_state_known') is not True:leaf.update(read_command_outcome_unknown=True,workflow_recovery_required=True,do_not_retry=True)
    result['data'].update(legacy_result=leaf,read_response_is_current_typed_receipt_not_legacy_RPC=True,PCG_generation_completed=False)
    initial_response=initial.get('native_response')
    initial_data=initial_response.get('data') if type(initial_response) is dict else None
    if type(initial_data) is dict and initial_data.get('scoped_cleanup_generate') is True:
        completed=initial_data.get('PCG_cleanup_completed') is True
        result['data']['PCG_cleanup_completed']=completed
        leaf.update(PCG_cleanup_completed=completed,PCG_generation_completed=False)


def _wait_result(result,descriptor,leaf,elapsed,iterations):
    values=descriptor['values'];initial=descriptor['initial']
    timeout=max(0.0,min(float(values['timeout_seconds']),300.0))
    interval=max(0.005,min(float(values['poll_interval_seconds']),1.0))
    ready=_native_refresh_result_ready(leaf)
    leaf=copy.deepcopy(leaf)
    leaf.update(external_wait_used=True,wait_requested=True,wait_supported=True,wait_mode='external_mcp_poll',
        wait_completed=ready,wait_timed_out=not ready,wait_elapsed_seconds=elapsed,wait_iterations=iterations,
        timeout_seconds=timeout,poll_interval_seconds=interval,
        initial_native_response=initial.get('native_response'),initial_component_count=initial.get('component_count'),
        initial_cleanup_count=initial.get('cleanup_count'),initial_refresh_count=initial.get('refresh_count'),initial_generate_count=initial.get('generate_count'))
    mutation_count=sum(max(0,_int_or_default(initial.get(key),0)) for key in ('cleanup_count','refresh_count','generate_count'))
    if not ready:
        message='PCG generation did not reach a terminal ready state before the external wait deadline'
        leaf.update(success=False,status='error',error_code='pcg_generation_completion_timeout',message=message,error=message,
            failed_stage='wait_for_pcg_completion',failed_stage_effect='read',pcg_generation_may_still_be_in_flight=True,
            recovery_policy='poll_or_inspect_current_pcg_component_state_before_retry')
        if mutation_count>0:
            leaf.update(partial_mutation=True,mutation_performed=True,completed_mutation_count=1,
                completed_mutation_stages=['refresh_pcg_components_initial'],do_not_retry=True)
        else:leaf.update(mutation_attempted=False,mutation_performed=False,changed=False)
    result['data'].update(actual_wait_mode='deferred_GT_ticker_query_only',original_wait_mode_spelling_preserved=True,
        initial_mutation_was_not_replayed=True,shared_PCG_task_cancelled=False,PCG_generation_completed=ready)
    initial_response=initial.get('native_response')
    initial_data=initial_response.get('data') if type(initial_response) is dict else None
    if type(initial_data) is dict and initial_data.get('scoped_immediate_cleanup_only') is True:
        # Native synchronous cleanup completion and requested wait minima are separate.
        completed=initial_data.get('PCG_cleanup_completed') is True
        result['data'].update(PCG_generation_completed=False,PCG_cleanup_completed=completed)
        leaf.update(PCG_generation_completed=False,PCG_cleanup_completed=completed)
    elif type(initial_data) is dict and initial_data.get('scoped_cleanup_generate') is True:
        # Query completion never replays or replaces the initial cleanup dispatch.
        completed=initial_data.get('PCG_cleanup_completed') is True
        result['data']['PCG_cleanup_completed']=completed
        leaf.update(PCG_generation_completed=ready,PCG_cleanup_completed=completed)
    return leaf


def _rejected(method,error):
    result=state.new_result();state.fail(result,error)
    result['stage']='pcg_component_generation_input_validation'
    result['data'].update(public_method=method,native_dispatched=False,execution_mutation_attempted=False,
        change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    result['compile'].update(requested=False,attempted=False,compiled=False)
    result['save'].update(requested=False,attempted=False,saved=False)
    if hasattr(error,'public_validation_errors'):result['data']['public_validation_errors']=copy.deepcopy(error.public_validation_errors)
    result['data']['legacy_result']=dict(success=False,error=str(error),error_code=getattr(error,'code','pcg_tool_failed'))
    return result


def _named(method,arguments,_public_input=False):
    try:args,original,legacy=_arguments(method,arguments,None,_public_input)
    except Exception as error:return _rejected(method,error)
    return state.mutate(method,arguments,None,lambda result:_start(result,method,args,original,legacy,state.checkpoint)[0])


def _named_mcp(method,arguments):return _named(method,arguments,True)


def _action(result,method,arguments,callback,legacy,public_input=False):
    result['data']['parent_journal_only']=True
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        result.update(_rejected(method,error));result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _start(result,method,args,original,legacy,callback)[0]


def refresh_pcg_components(actor_name="",selected_only=False,cleanup=True,generate=True,refresh=False,force=True,wait_until_complete=False,timeout_seconds=10,poll_interval_seconds=0.05,min_generated_output_data_count=-1,min_managed_resource_count=-1,max_components=1000,native_first=True):
    return _named('refresh_pcg_components',dict(actor_name=actor_name,selected_only=selected_only,cleanup=cleanup,generate=generate,refresh=refresh,force=force,wait_until_complete=wait_until_complete,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds,min_generated_output_data_count=min_generated_output_data_count,min_managed_resource_count=min_managed_resource_count,max_components=max_components,native_first=native_first))


def refresh_pcg_components_action(result,actor_name="",selected_only=False,cleanup=True,generate=True,refresh=False,force=True,wait_until_complete=False,timeout_seconds=10,poll_interval_seconds=0.05,min_generated_output_data_count=-1,min_managed_resource_count=-1,max_components=1000,native_first=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'refresh_pcg_components',dict(actor_name=actor_name,selected_only=selected_only,cleanup=cleanup,generate=generate,refresh=refresh,force=force,wait_until_complete=wait_until_complete,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds,min_generated_output_data_count=min_generated_output_data_count,min_managed_resource_count=min_managed_resource_count,max_components=max_components,native_first=native_first),checkpoint_callback,_legacy_command_params,_public_input)


def bind_pcg_generation_settings_to_blueprint(blueprint_name,component_variable="ForestPCG",generation_trigger_variable="PCGGenerationTrigger",regenerate_in_editor_variable="PCGRegenerateInEditor",generate_on_drop_variable="",generate_on_drop_default=False,graph_name="UserConstructionScript",graph_id="",graph_type="function",replace_existing=True,sync_component_template_defaults=True,function_class="",function_name="ApplyPCGGenerationSettings",compile=False,save=False):
    return _named('bind_pcg_generation_settings_to_blueprint',dict(blueprint_name=blueprint_name,component_variable=component_variable,generation_trigger_variable=generation_trigger_variable,regenerate_in_editor_variable=regenerate_in_editor_variable,generate_on_drop_variable=generate_on_drop_variable,generate_on_drop_default=generate_on_drop_default,graph_name=graph_name,graph_id=graph_id,graph_type=graph_type,replace_existing=replace_existing,sync_component_template_defaults=sync_component_template_defaults,function_class=function_class,function_name=function_name,compile=compile,save=save))


def bind_pcg_generation_settings_to_blueprint_action(result,blueprint_name,component_variable="ForestPCG",generation_trigger_variable="PCGGenerationTrigger",regenerate_in_editor_variable="PCGRegenerateInEditor",generate_on_drop_variable="",generate_on_drop_default=False,graph_name="UserConstructionScript",graph_id="",graph_type="function",replace_existing=True,sync_component_template_defaults=True,function_class="",function_name="ApplyPCGGenerationSettings",compile=False,save=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'bind_pcg_generation_settings_to_blueprint',dict(blueprint_name=blueprint_name,component_variable=component_variable,generation_trigger_variable=generation_trigger_variable,regenerate_in_editor_variable=regenerate_in_editor_variable,generate_on_drop_variable=generate_on_drop_variable,generate_on_drop_default=generate_on_drop_default,graph_name=graph_name,graph_id=graph_id,graph_type=graph_type,replace_existing=replace_existing,sync_component_template_defaults=sync_component_template_defaults,function_class=function_class,function_name=function_name,compile=compile,save=save),checkpoint_callback,_legacy_command_params,_public_input)


def _template_arguments(blueprint_path, component_name, graph_path, regenerate_in_editor, generate_on_drop_when_on_demand, compile, save, operation_id):
    values = dict(blueprint_path=blueprint_path,component_name=component_name,graph_path=graph_path,regenerate_in_editor=regenerate_in_editor,generate_on_drop_when_on_demand=generate_on_drop_when_on_demand,compile=compile,save=save,operation_id=operation_id)
    for key in ('blueprint_path','component_name','graph_path','operation_id'):
        value = values[key]
        if type(value) is not str or (not value and key != 'operation_id') or any(ord(c)<32 or ord(c)==127 or 0xD800<=ord(c)<=0xDFFF for c in value) or len(value.encode('utf-16-le'))//2 > 1023:
            raise OperationError('pcg_template_argument_invalid', key+': bounded nonempty valid string required (operation_id may be empty)')
        if _dependency(value): raise OperationError('pcg_template_dependency_reference', 'Refusing tool-only reference in '+key)
    for key in ('blueprint_path','graph_path'):
        value = values[key]
        if not value.startswith('/Game/') or value != value.strip() or any(c in value for c in "\\:'\""):
            raise OperationError('pcg_template_argument_invalid', key+': exact /Game package/object path required')
    for key in ('regenerate_in_editor','generate_on_drop_when_on_demand','compile','save'):
        if type(values[key]) is not bool: raise OperationError('pcg_template_argument_invalid', key+': exact boolean required')
    if save and not compile: raise OperationError('pcg_template_save_requires_compile', 'save=True requires this request to perform a successful compile; no SDK call dispatched')
    if operation_id and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,95}', operation_id) is None:
        raise OperationError('invalid_operation_id', 'operation_id must be 1-96 ASCII identifier characters')
    return values


def _template_observation_projection(receipts):
    projection = {}
    for key, receipt in receipts.items():
        data = receipt['data']
        if key == 'components':
            projection[key] = copy.deepcopy(data['components'])
        else:
            projection[key] = {field:copy.deepcopy(data.get(field)) for field in ('component_name','component_class','property_name','property_type','property_encoding','property_value')}
    return projection


def _template_unknown(result, error):
    result['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None,post_failure_sdk_skipped=True)
    if hasattr(error,'native_raw_result'): result['data']['undecoded_native_raw_result'] = error.native_raw_result
    if hasattr(error,'native_decoded_result'): result['data']['failed_native_observation'] = copy.deepcopy(error.native_decoded_result)
    if hasattr(error,'partial_observation'): result['data']['partial_observation'] = copy.deepcopy(error.partial_observation)
    for change in result['changes']: change.update(change_state_known=False,changed=None)
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_template_or_save_outcome_no_replay')


def _configure_template_action(result, values, callback=None):
    method = 'configure_blueprint_pcg_template'
    params = {key:values[key] for key in ('blueprint_path','component_name','graph_path','regenerate_in_editor','generate_on_drop_when_on_demand','compile')}
    result['data'].update(operation=method,native_dispatched=False,native_result_returned=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,operation_complete=False,automatic_action_retry=False,atomic_rollback=False,registration_dependency='ToolsetAPI',operation_dependency='Editor-only EngineAPI',server_RPC_dependency=False,original_script_id='script:Python/scripts/analysis/pcg_cpu_falloff_binding_count_preflight_smoke.py',original_helper='configure_blueprint_pcg_template',original_helper_all_templates_scope_complete=False,policy_default_extension='GenerateOnDemand is fixed; regenerate_in_editor and generate_on_drop_when_on_demand default False are explicit extensions; exact component selection differs from the original all-PCG-template loop',full_original_pipeline_complete=False,cold_reload_verified=False,packaged_runtime_verified=False)
    result['compile'].update(requested=values['compile'],attempted=False,compiled=False,status='native_not_dispatched')
    result['save'].update(requested=values['save'],attempted=False,saved=False,status='not_attempted')
    result['stage']='before_pcg_template_checkpoint'
    _checkpoint(result,callback)
    result['stage']='native_pcg_template_configuration'
    result['data'].update(native_dispatched=True,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,asset_path=values['blueprint_path'],component_name=values['component_name'],attempted=True,change_state_known=False,changed=None)]
    result['compile'].update(attempted=None,compiled=None,status='unknown_until_native_result')
    _checkpoint(result,callback)
    try: native = api.configure_blueprint_pcg_template(params)
    except Exception as error:
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
            result['changes']=[]
            result['compile'].update(attempted=False,compiled=False,status='not_dispatched')
        else: _template_unknown(result,error)
        raise
    result['data'].update(native_result_returned=True,native_configuration=copy.deepcopy(native),change_state_known=native['change_state_known'],mutation_state_unknown=not native['change_state_known'],mutation_performed=native.get('changed') if native['change_state_known'] else None)
    result['stage']=native.get('stage','native_configuration_result')
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native.get('compiled'),native_receipt=copy.deepcopy(native['data'].get('compile')),status='compiled' if native.get('compiled') is True else 'failed' if native['compile_attempted'] else 'not_attempted')
    result['changes']=[dict(kind=method,asset_path=values['blueprint_path'],component_name=values['component_name'],attempted=native['mutation_attempted'],change_state_known=native['change_state_known'],changed=native.get('changed') if native['change_state_known'] else None)] if native['mutation_attempted'] or not native['change_state_known'] or native.get('changed') else []
    _checkpoint(result,callback)
    if native['save_requested'] or native['saved'] or native['compile_requested'] != values['compile'] or (not values['compile'] and native['compile_attempted']):
        error=OperationError('pcg_template_native_contract_mismatch','Actual Native compile/save receipt contradicts the request; no follow-up SDK')
        _template_unknown(result,error); raise error
    if not native['success']:
        if not native['change_state_known']: _template_unknown(result,OperationError('pcg_template_native_unknown','Native current state is unknown'))
        raise OperationError(native.get('error_code') or 'pcg_template_configuration_failed',native.get('error') or 'Native template configuration failed; actual prefix preserved')
    if not native['change_state_known'] or native['postcondition_passed'] is not True or (values['compile'] and native.get('compiled') is not True):
        error=OperationError('pcg_template_configuration_unverified','Native success lacks current postcondition/compile evidence; no follow-up SDK')
        _template_unknown(result,error); raise error
    if values['save']:
        package=values['blueprint_path'].split('.',1)[0]
        try:
            result['stage']='before_template_save_observation'
            before=api.observe_blueprint_pcg_template(values['blueprint_path'],values['component_name'])
            result['data']['before_save_observation']=copy.deepcopy(before)
            result['data']['file_before_save']=api.template_file_state(package)
            _checkpoint(result,callback)
            result['stage']='save_blueprint_template'
            result['save'].update(attempted=True,saved=None,status='save_outcome_unknown_until_return')
            result['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
            for change in result['changes']: change.update(change_state_known=False,changed=None)
            _checkpoint(result,callback)
            saved=api.save_blueprint_template(values['blueprint_path'])
            result['save'].update(saved=saved,status='saved' if saved else 'save_returned_false')
            _checkpoint(result,callback)
            if saved is not True: raise OperationError('pcg_template_save_failed','Configured Blueprint save returned false; changes remain and no retry is dispatched')
            observed_file=api.template_file_state(package)
            result['data']['saved_file']=copy.deepcopy(observed_file)
            if observed_file.get('exists') is not True or not observed_file.get('sha256'): raise OperationError('pcg_template_saved_file_unverified','Save returned true but a hashed target file was not observed')
            result['stage']='saved_template_dependency_audit'
            result['data']['saved_dependencies']=api.template_saved_dependencies(package,observed_file)
            _checkpoint(result,callback)
            result['stage']='post_save_template_observation'
            after=api.observe_blueprint_pcg_template(values['blueprint_path'],values['component_name'])
            result['data']['after_save_observation']=copy.deepcopy(after)
            result['data']['saved_file_after_observation']=api.template_file_state(package)
            if _template_observation_projection(before) != _template_observation_projection(after) or result['data']['saved_file_after_observation'] != observed_file:
                raise OperationError('pcg_template_save_postcondition_changed','Save/registry callback changed observed SCS/template/instance/policy or saved bytes; actual saved prefix is retained')
            result['data'].update(change_state_known=True,mutation_state_unknown=False,mutation_performed=native.get('changed'))
            for change in result['changes']: change.update(change_state_known=True,changed=native.get('changed'))
            result['validation'].update(complete_dependency_scan='passed',saved_file='hashed_and_stable',save_readback='SCS_template_instance_reference_and_policy_projection_unchanged',inner_graph_binding_after_save='not_independently_reobserved')
        except Exception as error:
            _template_unknown(result,error)
            raise
    else:
        result['save'].update(status='not_requested')
        result['validation'].update(complete_dependency_scan='not_run_save_false',save_readback='not_run')
    result['data']['operation_complete']=True
    result['validation'].update(status='scoped_native_configuration_and_requested_persistence_completed',disk_reload='not_run',packaged_runtime='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='template_configuration_callbacks_would_repeat',policy='Same completed operation_id returns its recorded result. An interrupted/unknown intent is never re-executed.')
    result.update(success=True,stage='complete')
    return result


def configure_blueprint_pcg_template(blueprint_path, component_name, graph_path, regenerate_in_editor=False, generate_on_drop_when_on_demand=False, compile=True, save=True, operation_id=''):
    try: values=_template_arguments(blueprint_path,component_name,graph_path,regenerate_in_editor,generate_on_drop_when_on_demand,compile,save,operation_id)
    except Exception as error: return _rejected('configure_blueprint_pcg_template',error)
    arguments={key:copy.deepcopy(value) for key,value in values.items() if key!='operation_id'}
    return state.mutate('configure_blueprint_pcg_template',arguments,values['operation_id'] or None,lambda result:_configure_template_action(result,values,state.checkpoint))
