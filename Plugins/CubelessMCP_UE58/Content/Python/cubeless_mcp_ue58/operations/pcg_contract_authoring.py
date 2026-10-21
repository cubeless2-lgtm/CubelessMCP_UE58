"""HE public audit4 / audit-derived read3 / spawner7. No script executor or RPC.
Native owns Engine work; current actual receipt precedes every health failure.
"""
import copy
import hashlib
import json
import math
import re
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import pcg_contract_authoring as api
from . import state
from .state import OperationError
METHODS=["audit_pcg_graph_contract","read_pcg_node_contract","set_pcg_static_mesh_spawner_entries"]
PUBLIC_FIELDS={"audit_pcg_graph_contract":["graph_path","include_dependencies","forbidden_dependency_prefixes","fail_on_forbidden_dependencies"],"read_pcg_node_contract":["graph_path","node_id","include_dependencies"],"set_pcg_static_mesh_spawner_entries":["graph_path","node_id","mesh_entries","selector_type","attribute_name","material_override_attributes","save"]}
PARAM_FIELDS=copy.deepcopy(PUBLIC_FIELDS)
DEFAULTS={"audit_pcg_graph_contract":{"include_dependencies":True,"forbidden_dependency_prefixes":None,"fail_on_forbidden_dependencies":True},"read_pcg_node_contract":{"node_id":"","include_dependencies":False},"set_pcg_static_mesh_spawner_entries":{"mesh_entries":None,"selector_type":"weighted","attribute_name":"Meshes","material_override_attributes":None,"save":True}}
REQUIRED={"audit_pcg_graph_contract":["graph_path"],"read_pcg_node_contract":["graph_path"],"set_pcg_static_mesh_spawner_entries":["graph_path","node_id"]}
FIELD_TYPES={"audit_pcg_graph_contract":{"graph_path":"string","include_dependencies":"boolean","forbidden_dependency_prefixes":"array","fail_on_forbidden_dependencies":"boolean"},"read_pcg_node_contract":{"graph_path":"string","node_id":"string","include_dependencies":"boolean"},"set_pcg_static_mesh_spawner_entries":{"graph_path":"string","node_id":"string","mesh_entries":"array","selector_type":"string","attribute_name":"string","material_override_attributes":"array","save":"boolean"}}
NULLABLE={"audit_pcg_graph_contract":["forbidden_dependency_prefixes"],"read_pcg_node_contract":[],"set_pcg_static_mesh_spawner_entries":["mesh_entries","material_override_attributes"]}
ITEMS={"audit_pcg_graph_contract":{"forbidden_dependency_prefixes":"string"},"read_pcg_node_contract":{},"set_pcg_static_mesh_spawner_entries":{"mesh_entries":"object","material_override_attributes":"string"}}
OBSERVERS=["audit_pcg_graph_contract","read_pcg_node_contract"]
NO_JOURNAL_METHODS=list(OBSERVERS)
WRITERS=["set_pcg_static_mesh_spawner_entries"]
_ACTIVE_REQUEST=False
_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp","/unrealmcp","unrealmcp","mcpunreal","mcp_unreal","/script/cubelesscontrolrigtools","cubelesscontrolrigtools","cubelesscontrolrigtoolseditor","cubelessmcp","cubeless_mcp","modelcontextprotocol","toolsetregistry","/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    text=value.strip().strip('\"\'').replace('\\','/').lower()
    return any(token in text for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('pcg_contract_authoring_dependency_reference','Refusing MCP-only reference in '+field)
            error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)



def _public_error(field,kind,message):
    error=OperationError('pcg_contract_authoring_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('pcg_contract_authoring_public_argument_invalid','Known method and public argument object required')
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
                        if any(type(key) is not str for key in item):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
                    elif ITEMS[method][field]=='number':item=_public_float(field,item)
                    elif ITEMS[method][field]!='any':raise OperationError('pcg_contract_authoring_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('pcg_contract_authoring_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values



def binding_status():
    status=api.binding_status()
    return {METHODS[0]:status.get(METHODS[0]) is True,METHODS[1]:status.get(METHODS[0]) is True,METHODS[2]:status.get(METHODS[2]) is True}


def _arguments(method,arguments,legacy=None,public_input=False):
    if method not in METHODS or type(arguments) is not dict or type(public_input) is not bool:
        raise OperationError('pcg_contract_authoring_entry_invalid','Known method, argument object and exact model selector required')
    original=copy.deepcopy(arguments)
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('pcg_contract_authoring_entry_invalid','Direct lower-name Batch alias and Named model are distinct')
        arguments=legacy
    values=normalize_public_mcp_arguments(method,arguments) if public_input else dict(copy.deepcopy(DEFAULTS[method]),**{key:copy.deepcopy(arguments[key]) for key in PUBLIC_FIELDS[method] if key in arguments})
    for field in REQUIRED[method]:
        if field not in values:raise _public_error(field,'missing','Field required')
    # Original wrappers apply Python bool/None transforms before injected JSON.
    # ParamsJson now carries the public names, not old private payload aliases.
    if method==METHODS[0]:
        params=dict(graph_path=values['graph_path'],include_dependencies=bool(values['include_dependencies']),
            forbidden_dependency_prefixes=[] if values['forbidden_dependency_prefixes'] is None else values['forbidden_dependency_prefixes'],
            fail_on_forbidden_dependencies=bool(values['fail_on_forbidden_dependencies']))
    elif method==METHODS[1]:
        params=dict(graph_path=values['graph_path'],include_dependencies=bool(values['include_dependencies']),forbidden_dependency_prefixes=[],fail_on_forbidden_dependencies=False)
    else:
        params=dict(graph_path=values['graph_path'],node_id=values['node_id'],mesh_entries=values['mesh_entries'] or [],
            selector_type=values['selector_type'],attribute_name=values['attribute_name'],
            material_override_attributes=[] if values['material_override_attributes'] is None else values['material_override_attributes'],save=bool(values['save']))
    api._carrier(params)
    return values,params,original


def _context(result,method,values,original,legacy,public_input):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),public_argument_validation_applied=public_input,
        lower_name_Batch_arguments=copy.deepcopy(legacy),original_Native_command=False,hidden_fields=0,
        Native_method=METHODS[0] if method==METHODS[1] else method,read_is_Python_audit_composition=method==METHODS[1],
        automatic_action_retry=False,atomic_rollback=False,operation_complete=False,
        native_dispatched=False,native_result_returned=False,execution_mutation_attempted=False,
        native_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,
        SDK_global_purity_unverified=False,actual_named_wire_verified=False,full_original_pipeline_complete=False,
        registration_dependency='ToolsetAPI',operation_dependency='dedicated_public_EngineAPI',server_RPC_dependency=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='not_requested')
    result['save'].update(requested=bool(values.get('save',False)) if method==METHODS[2] else False,attempted=False,saved=False,status='not_attempted')


def _checkpoint(result,callback):
    if callback:callback(result)


def _changes(result):
    data=result['data'];known=data['change_state_known'];attempted=data['native_mutation_attempted'];changed=data.get('observed_changed',False)
    # Save history is independent of the overall mutation/legacy success flag.
    save=result['save'];body=data.get('native_operation_data',{});prefix=data.get('actual_save_prefix',{})
    terminal=data.get('native_result_returned') is True and (body.get('operation_complete') is True or prefix.get('SDK_save_returned') is True)
    scalar=data.get('native_scalar_result',{})
    save_flags_observed=all(type(scalar.get(key)) is bool for key in ('save_attempted','saved'))
    if save.get('requested') is True and data.get('native_result_returned') is True and not save_flags_observed:save['status']='unknown'
    elif save.get('saved') is True:save['status']='saved'
    elif save.get('attempted') is True:save['status']='failed' if terminal else 'unknown'
    elif save.get('requested') is True and data.get('native_dispatch_pending') is True:save['status']='dispatch_pending'
    elif save.get('requested') is True and data.get('native_dispatched') is True and data.get('native_result_returned') is not True:save['status']='unknown'
    else:save['status']='not_attempted'
    data.update(execution_mutation_attempted=attempted,mutation_state_unknown=not known,mutation_performed=changed if known else None)
    result['changes']=[] if known and not attempted and not changed else [dict(kind='pcg_contract_authoring',attempted=attempted,changed=changed if known else None,change_state_known=known)]
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_native_outcome_no_replay' if not known else 'inspect_recorded_PCG_changes_before_new_operation' if attempted else 'none_observed')


def _adopt(result,native,raw):
    data=result['data'];data['native_result_returned']=True
    data['native_scalar_result']={key:copy.deepcopy(value) for key,value in native.items() if key not in ('data','_raw_native_json')}
    data['native_mutation_attempted']=native.get('mutation_attempted') is True
    data['change_state_known']=native.get('change_state_known') is True and type(native.get('changed')) is bool
    data['observed_changed']=native.get('changed') is True
    result['compile'].update(requested=native.get('compile_requested') is True,attempted=native.get('compile_attempted') is True,compiled=native.get('compiled'))
    result['save']['requested']|=native.get('save_requested') is True
    result['save']['attempted']|=native.get('save_attempted') is True
    result['save']['saved']|=native.get('saved') is True
    body=native.get('data',{})
    if type(body) is dict:
        data['native_operation_data']={key:copy.deepcopy(value) for key,value in body.items() if key!='legacy_result'}
        if type(body.get('legacy_result')) is dict:data['legacy_result']=copy.deepcopy(body['legacy_result'])
        data['SDK_global_purity_unverified']=body.get('SDK_global_purity_unverified') is True
        for key in ('actual_mutation_prefix','actual_compile_prefix','actual_save_prefix','actual_partial_state'):
            if key in body:data[key]=copy.deepcopy(body[key])
    # Canonical parsed Native data occurs once. This receipt describes genuine
    # omitted diagnostic JSON; it is not transformed content labelled as raw.
    if type(raw) is str:
        encoded=raw.encode('utf-8');data['native_raw_receipt']=dict(original_raw_preserved=False,omitted=True,
            reason='decoded_authoritative_receipt_retained_once',sha256=hashlib.sha256(encoded).hexdigest(),utf8_bytes=len(encoded),utf16_units=len(raw.encode('utf-16-le'))//2,
            legacy_result_reference='data.legacy_result',native_other_data_reference='data.native_operation_data')
    _changes(result)


def _native(result,method,params,callback):
    data=result['data'];result['stage']='before_pcg_contract_authoring_checkpoint';_checkpoint(result,callback)
    prior=data['change_state_known'];data.update(native_dispatch_pending=True,change_state_known=False);_changes(result)
    try:_checkpoint(result,callback)
    except Exception:data.update(native_dispatch_pending=False,change_state_known=prior);_changes(result);raise
    data.update(native_dispatch_pending=False,native_dispatched=True)
    try:native=getattr(api,method)(params)
    except Exception as error:
        if getattr(error,'native_call_dispatched',None) is False:data.update(native_dispatched=False,change_state_known=prior)
        if getattr(error,'native_call_returned',False):data['undecoded_native_raw_result']=getattr(error,'native_raw_result',None)
        _changes(result);result['stage']='returned_pcg_contract_authoring_operation';_checkpoint(result,callback);raise
    _adopt(result,native,native.pop('_raw_native_json',None))
    result['stage']='returned_pcg_contract_authoring_operation';_checkpoint(result,callback)
    if native.get('success') is not True or native.get('postcondition_passed') is not True:
        raise OperationError(native.get('error_code') or 'pcg_contract_authoring_native_failed',native.get('error') or 'Native PCG postcondition failed; current actual prefix preserved')
    if native.get('data',{}).get('operation_complete') is False:
        data.update(change_state_known=False,operation_complete=False);_changes(result)
        raise OperationError('pcg_contract_authoring_native_incomplete','Actual PCG receipt is incomplete; no replay')
    if any(native.get(key) is not False for key in ('compile_requested','compile_attempted')) or native.get('compiled') is not False:
        raise OperationError('pcg_contract_authoring_unexpected_compile','These original APIs never compile; actual returned prefix preserved')
    if method==METHODS[2] and (native.get('save_requested') is not bool(params['save']) or not params['save'] and any(native.get(key) is not False for key in ('save_attempted','saved'))):
        raise OperationError('pcg_contract_authoring_save_contract_invalid','Spawner optional save intent/actual state differs; returned savedTrue remains preserved')
    if method==METHODS[0] and (native.get('mutation_attempted') is not False or native.get('changed') is not False or any(native.get(key) is not False for key in ('save_requested','save_attempted','saved'))):
        raise OperationError('pcg_contract_authoring_observer_mutated','Read-only audit returned explicit writer/save state; actual prefix preserved')
    return result['data'].get('legacy_result')


def _read_contract(audit,values):
    # Original read helper returns a failed audit unchanged. No second load,
    # registry scan, Native request or query of every node occurs here.
    if not audit.get('success'):return audit
    nodes=audit.get('nodes',[])
    if values['node_id']:
        needle=str(values['node_id']).lower()
        nodes=[node for node in nodes if needle in {str(node.get('node_name','')).lower(),str(node.get('title','')).lower(),str(node.get('settings_class','')).lower()}]
        if not nodes:
            message='No PCG node matched node_id: '+str(values['node_id'])
            return dict(success=False,error=message,message=message,error_code='pcg_node_not_found',graph_path=audit.get('graph_path',values['graph_path']),
                available_nodes=[{key:node.get(key) for key in ('node_name','title','settings_class')} for node in audit.get('nodes',[])])
    return dict(success=True,graph_path=audit.get('graph_path',values['graph_path']),node_id=values['node_id'],node_count=len(nodes),nodes=nodes,
        dependencies=audit.get('dependencies',[]) if values['include_dependencies'] else [])


def _finish(result,leaf):
    if type(leaf) is not dict or type(leaf.get('success')) is not bool:
        raise OperationError('pcg_contract_authoring_legacy_invalid','Original semantic legacy success bool is required')
    result['data'].update(legacy_result=leaf,operation_complete=True)
    result['validation'].update(status='current_Native_typed_target_receipt',atomic_mutation=False,disk_reload='not_run',whole_serialized_transitive_or_packaged_independence='not_proved')
    if leaf['success'] is False:
        raise OperationError(leaf.get('error_code') or 'pcg_graph_contract_audit_failed',leaf.get('error') or leaf.get('message') or 'PCG contract audit returned successFalse')
    if not result['data']['change_state_known']:
        raise OperationError('pcg_contract_authoring_outcome_unknown','Native target outcome remains unknown; actual prefix preserved')
    result.update(success=True,stage='complete');return result


def _run(result,method,values,params,original,legacy,public_input,callback):
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:raise OperationError('pcg_contract_authoring_reentry','Reentry rejected before any Engine operation')
    _ACTIVE_REQUEST=True
    try:
        _context(result,method,values,original,legacy,public_input)
        if method==METHODS[2]:
            if type(values['selector_type']) is not str or values['selector_type'] not in {'weighted','by_attribute','attribute'}:
                error=OperationError('pcg_invalid_selector_type','selector_type must be weighted or by_attribute')
                result['data']['legacy_result']=dict(success=False,error=str(error),message=str(error),error_code=error.code);raise error
            if values['selector_type']=='weighted' and not params['mesh_entries']:
                error=OperationError('pcg_empty_mesh_entries','mesh_entries must not be empty for weighted selector')
                result['data']['legacy_result']=dict(success=False,error=str(error),message=str(error),error_code=error.code);raise error
        native_method=METHODS[0] if method==METHODS[1] else method
        leaf=_native(result,native_method,params,callback)
        if type(leaf) is not dict:raise OperationError('pcg_contract_authoring_legacy_invalid','Native legacy object is required')
        if method==METHODS[1]:
            # Original read drops the unfiltered audit graph/edges. Preserve
            # its small observation status, not another full diagnostic copy.
            result['data']['audit_observation_summary']={key:copy.deepcopy(leaf.get(key)) for key in ('success','graph_path','node_count','edge_count','dependency_scan_skipped','dependency_scan_warning')}
            result['data']['unfiltered_audit_diagnostic_omitted']=True
            leaf=_read_contract(leaf,values)
        return _finish(result,leaf)
    except Exception as error:
        result['data'].setdefault('legacy_result',dict(success=False,error=str(error),error_code=getattr(error,'code','pcg_tool_failed')))
        raise
    finally:_ACTIVE_REQUEST=False


def _rejected(method,error):
    result=state.new_result();state.fail(result,error)
    result['stage']='pcg_contract_authoring_input_validation'
    result['compile'].update(requested=False,attempted=False,compiled=False)
    result['save'].update(requested=False,attempted=False,saved=False)
    result['data'].update(public_method=method,native_dispatched=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,
        mutation_performed=False,journal_created=False,legacy_result=dict(success=False,error=str(error),error_code=getattr(error,'code','pcg_tool_failed')))
    if hasattr(error,'public_validation_errors'):result['data']['public_validation_errors']=copy.deepcopy(error.public_validation_errors)
    return result


def _named(method,arguments,public_input=False):
    try:values,params,original=_arguments(method,arguments,None,public_input)
    except Exception as error:return _rejected(method,error)
    if method in WRITERS:return state.mutate(method,arguments,None,lambda result:_run(result,method,values,params,original,None,public_input,state.checkpoint))
    result=state.new_result();result['data']['journal_created']=False
    try:return _run(result,method,values,params,original,None,public_input,None)
    except Exception as error:return state.fail(result,error)


def _named_mcp(method,arguments):return _named(method,arguments,True)


def _action(result,method,arguments,callback,legacy,public_input):
    result['data']['parent_journal_only']=True
    try:values,params,original=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        # Do not replace the parent's operation ID on public validation failure.
        rejected=_rejected(method,error);rejected['operation_id']=result['operation_id']
        result.update(rejected);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,values,params,original,legacy,public_input,callback)


def audit_pcg_graph_contract(graph_path,include_dependencies=True,forbidden_dependency_prefixes=None,fail_on_forbidden_dependencies=True):
    return _named('audit_pcg_graph_contract',dict(graph_path=graph_path,include_dependencies=include_dependencies,forbidden_dependency_prefixes=forbidden_dependency_prefixes,fail_on_forbidden_dependencies=fail_on_forbidden_dependencies))


def audit_pcg_graph_contract_action(result,graph_path,include_dependencies=True,forbidden_dependency_prefixes=None,fail_on_forbidden_dependencies=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'audit_pcg_graph_contract',dict(graph_path=graph_path,include_dependencies=include_dependencies,forbidden_dependency_prefixes=forbidden_dependency_prefixes,fail_on_forbidden_dependencies=fail_on_forbidden_dependencies),checkpoint_callback,_legacy_command_params,_public_input)


def read_pcg_node_contract(graph_path,node_id="",include_dependencies=False):
    return _named('read_pcg_node_contract',dict(graph_path=graph_path,node_id=node_id,include_dependencies=include_dependencies))


def read_pcg_node_contract_action(result,graph_path,node_id="",include_dependencies=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'read_pcg_node_contract',dict(graph_path=graph_path,node_id=node_id,include_dependencies=include_dependencies),checkpoint_callback,_legacy_command_params,_public_input)


def set_pcg_static_mesh_spawner_entries(graph_path,node_id,mesh_entries=None,selector_type="weighted",attribute_name="Meshes",material_override_attributes=None,save=True):
    return _named('set_pcg_static_mesh_spawner_entries',dict(graph_path=graph_path,node_id=node_id,mesh_entries=mesh_entries,selector_type=selector_type,attribute_name=attribute_name,material_override_attributes=material_override_attributes,save=save))


def set_pcg_static_mesh_spawner_entries_action(result,graph_path,node_id,mesh_entries=None,selector_type="weighted",attribute_name="Meshes",material_override_attributes=None,save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'set_pcg_static_mesh_spawner_entries',dict(graph_path=graph_path,node_id=node_id,mesh_entries=mesh_entries,selector_type=selector_type,attribute_name=attribute_name,material_override_attributes=material_override_attributes,save=save),checkpoint_callback,_legacy_command_params,_public_input)

