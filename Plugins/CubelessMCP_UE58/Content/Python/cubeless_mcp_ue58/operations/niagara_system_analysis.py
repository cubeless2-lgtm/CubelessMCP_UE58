"""HB original Python aggregate: one guarded metadata normalization then six existing compatibility leaves. No engine calls, child journals, transport or invented completion."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_system_analysis as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"analyze_niagara_system": ["system_path", "include_renderers", "include_user_parameters", "include_stack", "include_graph", "include_module_inputs", "include_compile_status", "include_pins", "include_links", "include_scratch_pads", "include_resolved_stack_inputs", "max_function_calls", "max_nodes_per_graph", "max_links_per_graph", "max_modules", "max_candidates_per_module", "max_resolved_inputs_per_module", "max_top_candidates"]}
DEFAULTS={"analyze_niagara_system": {"include_renderers": True, "include_user_parameters": True, "include_stack": True, "include_graph": True, "include_module_inputs": True, "include_compile_status": True, "include_pins": False, "include_links": False, "include_scratch_pads": True, "include_resolved_stack_inputs": False, "max_function_calls": 200, "max_nodes_per_graph": 300, "max_links_per_graph": 0, "max_modules": 200, "max_candidates_per_module": 24, "max_resolved_inputs_per_module": 8, "max_top_candidates": 80}}
REQUIRED={"analyze_niagara_system": ["system_path"]}
FIELD_TYPES={"analyze_niagara_system": {"system_path": "string", "include_renderers": "boolean", "include_user_parameters": "boolean", "include_stack": "boolean", "include_graph": "boolean", "include_module_inputs": "boolean", "include_compile_status": "boolean", "include_pins": "boolean", "include_links": "boolean", "include_scratch_pads": "boolean", "include_resolved_stack_inputs": "boolean", "max_function_calls": "integer", "max_nodes_per_graph": "integer", "max_links_per_graph": "integer", "max_modules": "integer", "max_candidates_per_module": "integer", "max_resolved_inputs_per_module": "integer", "max_top_candidates": "integer"}}
NULLABLE={"analyze_niagara_system": []}
ITEMS={"analyze_niagara_system": {}}
OBSERVERS=["analyze_niagara_system"]
NO_JOURNAL_METHODS=["analyze_niagara_system"]
WRITERS=[]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_system_analysis_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_system_analysis_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_system_analysis_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_system_analysis_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='object':
            if type(value) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
            if any(type(key) is not str for key in value):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('niagara_system_analysis_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values


SECTIONS=('renderers','user_parameters','stack','graph','module_inputs','compile_status')
SUMMARY_FIELDS={
 'renderers':{'renderer_count':'renderer_count'},
 'user_parameters':{'user_parameter_count':'parameter_count','settable_user_parameter_count':'settable_count'},
 'stack':{'stack_function_call_count':'total_emitter_function_call_count','stack_scratch_pad_count':'total_scratch_pad_count'},
 'graph':{'graph_count':'total_graph_count','graph_node_count':'total_node_count','graph_link_count':'total_link_count','graph_scratch_pad_count':'total_scratch_pad_count'},
 'module_inputs':{'module_count':'module_count','module_input_candidate_count':'candidate_count'},
 'compile_status':{'compile_script_count':'script_count','compile_error_count':'error_count','compile_warning_count':'warning_count','compile_dirty_count':'dirty_count','compile_outstanding':'outstanding_compilation_requests_after'}}

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_system_analysis_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_system_analysis_entry_invalid','Raw and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params)
        return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    # Original Python body forwards all eighteen values without int/bool coercion or truthy omission.
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,hidden_fields=0,
        operational_effect='read_world',original_python_body18_forwarding_preserved=True,metadata_native_sdk_normalizes_raw_options=True,
        source_policy='read_only_analysis_no_save_no_compile_request',child_call_order=list(SECTIONS),child_calls_are_compatibility_EngineAPI=True,child_public_Toolset_calls=0,child_journals_created=0,
        full_legacy_niagara_user_parameter_parity=False,user_leaf_projection_scope='finite_existing_Function_FResult_parameters_counts; typed_original_retained',
        legacy_success_is_initial_metadata_success=True,legacy_complete_is_limitations_empty=True,legacy_section_status_is_separate=True,
        sequential_observation_is_atomic_snapshot=False,SDK_global_purity_verified=False,GPU_completion_verified=False,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated metadata EngineAPI plus existing compatibility6',server_RPC_dependency=False,
        automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _typed(native):
    flags=('success','mutation_attempted','change_state_known','postcondition_passed','compile_requested','compile_attempted','save_requested','save_attempted','saved')
    if (type(native) is not dict or type(native.get('data')) is not dict or any(type(native.get(k)) is not bool for k in flags)
            or 'compiled' not in native or native['compiled'] is not None and type(native['compiled']) is not bool
            or 'changed' not in native or native['changed'] is not None and type(native['changed']) is not bool
            or native['change_state_known'] and type(native['changed']) is not bool):
        raise OperationError('niagara_system_analysis_child_result_invalid','Typed native receipt required; dispatched observation remains unknown')
    return native

def _adopt(result,section,native):
    # Keep the whole returned receipt before checking success, postcondition or observer intent.
    receipt=copy.deepcopy(native)
    result['data']['native_receipts'].append(dict(section=section,result=receipt))
    result['data']['native_result_returned']=True
    data=result['data'];data['native_mutation_attempted']=data['native_mutation_attempted'] is True or native['mutation_attempted']
    data['execution_mutation_attempted']=data['native_mutation_attempted']
    data['change_state_known']=data['change_state_known'] is True and native['change_state_known']
    data['mutation_state_unknown']=not data['change_state_known']
    if native['changed'] is True:data['observed_target_changed']=True
    data['mutation_performed']=data['observed_target_changed'] if data['change_state_known'] else None
    data['SDK_global_purity_unverified']=data.get('SDK_global_purity_unverified',False) or native['data'].get('SDK_global_purity_unverified') is True
    for source,target in (('compile_requested','requested'),('compile_attempted','attempted')):result['compile'][target]=result['compile'][target] or native[source]
    if native['compiled'] is None or result['compile']['compiled'] is None:result['compile']['compiled']=None
    elif native['compiled'] is True:result['compile']['compiled']=True
    for source,target in (('save_requested','requested'),('save_attempted','attempted'),('saved','saved')):result['save'][target]=result['save'][target] or native[source]
    result['changes']=[] if data['change_state_known'] and not data['native_mutation_attempted'] and not data['observed_target_changed'] else [dict(kind='analyze_niagara_system',attempted=data['native_mutation_attempted'],changed=data['mutation_performed'],change_state_known=data['change_state_known'])]
    result['compile']['status']='actual_metadata_and_child_receipts';result['save']['status']='actual_metadata_and_child_receipts'
    result['stage']='returned_niagara_system_analysis_operation'

def _observer_health(native,section='metadata'):
    return (native['success'] is True and native['postcondition_passed'] is True and native['changed'] is not True
        and not any(native[k] for k in ('mutation_attempted','compile_requested','compile_attempted','save_requested','save_attempted','saved')) and (native['compiled'] is False or section=='user_parameters' and native['compiled'] is None)
        and native['data'].get('operation_complete') is not False and native.get('operation_complete') is not False)

def _must_stop(native,section):
    if native.get('operation_complete') is False or native['data'].get('operation_complete') is False:return 'incomplete_native_observation'
    if native['changed'] is True:return 'unexpected_target_change_during_observation'
    if any(native[k] for k in ('mutation_attempted','compile_requested','compile_attempted','save_requested','save_attempted','saved')) or not (native['compiled'] is False or section=='user_parameters' and native['compiled'] is None):return 'unexpected_explicit_work_or_compile_state'
    # Existing native leaves guard their own SDK lifetime. Explicit unsafe receipts stop all later leaves.
    code=str(native.get('error_code','')).lower()
    if code.endswith('_changed') or code.endswith('_not_preserved'):return 'unsafe_native_context_or_identity'
    data=native['data']
    if data.get('projection_skipped_unsafe') is True or any(type(data.get(key)) is dict and data[key].get('projection_skipped_unsafe') is True for key in ('before','after','projection','source_projection','target_projection')):return 'unsafe_native_projection'
    if any(token in code for token in ('context_unavailable','world_unavailable','world_changed','identity_changed','identity_unexpected','object_unavailable','object_expired','routing_changed','version_changed','graph_changed','node_changed','pin_changed','node_content_changed','unsafe','garbage_collect','engine_exit')):return 'unsafe_native_context_or_identity'
    return None

def _params(section,p):
    out={'system_path':p['system_path']}
    fields={'stack':('include_pins','max_function_calls'),'graph':('include_pins','include_links','include_scratch_pads','max_nodes_per_graph','max_links_per_graph'),
        'module_inputs':('include_resolved_stack_inputs','max_modules','max_candidates_per_module','max_resolved_inputs_per_module','max_top_candidates')}.get(section,())
    out.update({key:copy.deepcopy(p[key]) for key in fields})
    if section=='module_inputs':out['include_linked_sources']=True
    if section=='compile_status':out.update(request_compile=False,force=False,allow_source_compile=False,wait_for_completion=False)
    return out

def _normalized(native):
    p=native['data'].get('normalized_parameters')
    if type(p) is not dict or set(p)!=set(PUBLIC_FIELDS['analyze_niagara_system']) or type(p.get('system_path')) is not str or not p['system_path']:
        raise OperationError('niagara_system_analysis_metadata_invalid','Exact SDK-normalized parameters18 required')
    for key in PUBLIC_FIELDS['analyze_niagara_system'][1:]:
        if key.startswith('include_'):
            if type(p[key]) is not bool:raise OperationError('niagara_system_analysis_metadata_invalid','SDK-normalized Boolean required: '+key)
        elif type(p[key]) is not int or not -(1<<31)<=p[key]<(1<<31):raise OperationError('niagara_system_analysis_metadata_invalid','SDK-normalized int32 required: '+key)
    return p

def _section_payload(section,native,path):
    if section!='user_parameters':
        value=native['data'].get('legacy_result')
        if value is not None and type(value) is not dict:raise OperationError('niagara_system_analysis_section_invalid','Legacy section must be an object or null')
        return copy.deepcopy(value)
    # Existing User leaf has Function::FResult, no Variable legacy_result. Do not invent unsupported original fields.
    data=native['data'];value=dict(success=native['success'],system_path=path,read_only=True)
    for key in ('parameters','parameter_count','settable_count'):
        if key in data:value[key]=copy.deepcopy(data[key])
    if native.get('error'):value['error']=native['error']
    return value

def _add_section(result,legacy,section,payload):
    succeeded=type(payload) is dict and payload.get('success') is True
    legacy['section_status'][section]=succeeded
    if payload is None:legacy['limitations'].append(section+': no result')
    else:
        legacy[section]=copy.deepcopy(payload)
        if not succeeded:
            error=payload.get('error')
            if type(error) is str and error:legacy['limitations'].append(section+': '+error)
    if succeeded:
        for target,source in SUMMARY_FIELDS[section].items():
            value=payload.get(source)
            valid=type(value) is bool if target=='compile_outstanding' else type(value) in (int,float) and (type(value) is int or math.isfinite(value))
            if valid:legacy['summary'][target]=copy.deepcopy(value)
            else:result['data']['summary_unavailable'].append(dict(section=section,field=target,source=source,reason='existing_leaf_did_not_supply_typed_value'))
    legacy['complete']=len(legacy['limitations'])==0

def _dispatch(result,section,params,callback):
    prior_known=result['data']['change_state_known'];prior_performed=result['data']['mutation_performed']
    result['stage']='before_niagara_system_analysis_checkpoint'
    result['data'].update(current_section=section,current_native_params=copy.deepcopy(params),current_native_dispatched=False,current_observation_unknown=True,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind='analyze_niagara_system',attempted=None,changed=None,change_state_known=False)]
    if callback:callback(result)
    result['data'].update(native_dispatched=True,current_native_dispatched=True,current_observation_unknown=True)
    result['stage']='native_niagara_system_analysis_operation'
    try:native=api.inspect_metadata(params) if section=='metadata' else api.inspect_section(section,params)
    except Exception as error:
        result['data'].setdefault('dispatch_failures',[]).append(dict(section=section,error=str(error),native_call_dispatched=getattr(error,'native_call_dispatched',None),native_call_returned=getattr(error,'native_call_returned',False),native_raw_result=getattr(error,'native_raw_result',None)))
        if getattr(error,'native_call_dispatched',None) is not False:
            result['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None,current_observation_unknown=True)
        else:result['data'].update(current_observation_unknown=False,change_state_known=prior_known,mutation_state_unknown=not prior_known,mutation_performed=prior_performed)
        result['stage']='returned_niagara_system_analysis_operation'
        if callback:callback(result)
        raise
    try:native=_typed(native)
    except Exception:
        result['data'].setdefault('invalid_native_results',[]).append(dict(section=section,result=copy.deepcopy(native)))
        result['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        if callback:callback(result)
        raise
    result['data']['change_state_known']=prior_known
    _adopt(result,section,native);result['data']['current_observation_unknown']=False
    if callback:callback(result)
    return native

def _run(result,method,args,original,raw,checkpoint_callback=None):
    callback=checkpoint_callback;_context(result,method,args,original,raw)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_receipts=[],native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,observed_target_changed=False,summary_unavailable=[],skipped_sections=[],operation_complete=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    metadata=_dispatch(result,'metadata',args['params'],callback)
    common=metadata['data'].get('legacy_result')
    if type(common) is dict:result['data']['legacy_result']=copy.deepcopy(common)
    if not _observer_health(metadata):raise OperationError(metadata.get('error_code') or 'niagara_system_analysis_metadata_failed',metadata.get('error') or 'Initial metadata observation failed; no child replay')
    p=_normalized(metadata)
    if type(common) is not dict or common.get('success') is not True or common.get('system_path')!=p['system_path'] or type(common.get('emitter_count')) is not int or common['emitter_count']<0:
        raise OperationError('niagara_system_analysis_metadata_invalid','Successful canonical metadata/count required')
    result['data']['normalized_parameters']=copy.deepcopy(p)
    legacy=copy.deepcopy(common);legacy.update(section_status={},summary={},limitations=[],complete=True)
    result['data']['legacy_result']=legacy
    failures=[]
    for index,section in enumerate(SECTIONS):
        if not p['include_'+section]:continue
        try:
            native=_dispatch(result,section,_params(section,p),callback)
            payload=_section_payload(section,native,p['system_path']);_add_section(result,legacy,section,payload)
            if not _observer_health(native,section) or payload is None or payload.get('success') is not True:failures.append(section)
            reason=_must_stop(native,section)
            if reason:
                result['data'].update(continuation_stopped=True,continuation_stop_reason=reason)
                result['data']['skipped_sections']=[dict(section=s,reason=reason) for s in SECTIONS[index+1:] if p['include_'+s]]
                raise OperationError(native.get('error_code') or 'niagara_system_analysis_continuation_stopped',native.get('error') or 'Unsafe or incomplete child observation; remaining leaves were not dispatched')
        except Exception as error:
            result['data'].setdefault('failed_section',section)
            result['data']['skipped_sections']=[dict(section=s,reason='preceding_section_failed_without_safe_continuation') for s in SECTIONS[index+1:] if p['include_'+s]]
            result['data'].update(operation_complete=False,partial_observation=True)
            if callback:callback(result)
            raise
        result['data']['legacy_result']=legacy
        if callback:callback(result)
    result['data'].update(operation_complete=True,partial_observation=bool(failures),section_health_failures=failures)
    result['validation'].update(status='metadata_and_existing_six_leaf_receipts',native_postcondition_passed=not failures,legacy_complete=legacy['complete'],legacy_section_status=copy.deepcopy(legacy['section_status']),total_change_state_known=result['data']['change_state_known'],disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='SDK_global_effects_unverified_no_replay' if not result['data']['change_state_known'] else 'none_observed')
    if failures:raise OperationError('niagara_system_analysis_section_health_failed','Existing section health failed; original legacy success/complete and actual receipts preserved: '+', '.join(failures))
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_system_analysis_input_validation';result['changes']=[]
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    if hasattr(error,'public_validation_errors'):result['data'].update(public_validation_errors=copy.deepcopy(error.public_validation_errors),tool_body_invoked=False)
    return result

def _rejected(method,error):return _rejection(state.new_result(),method,error)

def _named(method,arguments,_public_input=False):
    try:args,original,raw=_arguments(method,arguments,None,_public_input)
    except Exception as error:return _rejected(method,error)
    result=state.new_result()
    try:return _run(result,method,args,original,raw)
    except Exception as error:return state.fail(result,error)

def _action(result,method,arguments,callback,raw,public_input=False):
    result['data']['parent_journal_only']=True
    try:args,original,raw=_arguments(method,arguments,raw,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,raw,callback)

def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)

def analyze_niagara_system(system_path,include_renderers=True,include_user_parameters=True,include_stack=True,include_graph=True,include_module_inputs=True,include_compile_status=True,include_pins=False,include_links=False,include_scratch_pads=True,include_resolved_stack_inputs=False,max_function_calls=200,max_nodes_per_graph=300,max_links_per_graph=0,max_modules=200,max_candidates_per_module=24,max_resolved_inputs_per_module=8,max_top_candidates=80):
    return _named("analyze_niagara_system",dict(system_path=system_path,include_renderers=include_renderers,include_user_parameters=include_user_parameters,include_stack=include_stack,include_graph=include_graph,include_module_inputs=include_module_inputs,include_compile_status=include_compile_status,include_pins=include_pins,include_links=include_links,include_scratch_pads=include_scratch_pads,include_resolved_stack_inputs=include_resolved_stack_inputs,max_function_calls=max_function_calls,max_nodes_per_graph=max_nodes_per_graph,max_links_per_graph=max_links_per_graph,max_modules=max_modules,max_candidates_per_module=max_candidates_per_module,max_resolved_inputs_per_module=max_resolved_inputs_per_module,max_top_candidates=max_top_candidates))

def analyze_niagara_system_action(result,system_path,include_renderers=True,include_user_parameters=True,include_stack=True,include_graph=True,include_module_inputs=True,include_compile_status=True,include_pins=False,include_links=False,include_scratch_pads=True,include_resolved_stack_inputs=False,max_function_calls=200,max_nodes_per_graph=300,max_links_per_graph=0,max_modules=200,max_candidates_per_module=24,max_resolved_inputs_per_module=8,max_top_candidates=80,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"analyze_niagara_system",dict(system_path=system_path,include_renderers=include_renderers,include_user_parameters=include_user_parameters,include_stack=include_stack,include_graph=include_graph,include_module_inputs=include_module_inputs,include_compile_status=include_compile_status,include_pins=include_pins,include_links=include_links,include_scratch_pads=include_scratch_pads,include_resolved_stack_inputs=include_resolved_stack_inputs,max_function_calls=max_function_calls,max_nodes_per_graph=max_nodes_per_graph,max_links_per_graph=max_links_per_graph,max_modules=max_modules,max_candidates_per_module=max_candidates_per_module,max_resolved_inputs_per_module=max_resolved_inputs_per_module,max_top_candidates=max_top_candidates),checkpoint_callback,_legacy_command_params,_public_input)
