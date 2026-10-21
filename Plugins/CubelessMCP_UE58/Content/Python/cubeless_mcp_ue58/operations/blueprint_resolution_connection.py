"""HM original resolve1 and K2 connect9 contracts; Native owns Engine work and actual partials."""
import copy
import hashlib
import json
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_resolution_connection as api
from . import state
from .state import OperationError
METHODS=["resolve_blueprint", "connect_blueprint_nodes"]
PUBLIC_FIELDS={"resolve_blueprint": ["blueprint_name"], "connect_blueprint_nodes": ["blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin", "graph_name", "graph_id", "graph_type", "allow_pin_link_replacement"]}
PARAM_FIELDS={"resolve_blueprint": ["blueprint_name"], "connect_blueprint_nodes": ["blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin", "graph_name", "graph_id", "graph_type", "allow_pin_link_replacement", "create_graph_if_missing"]}
DEFAULTS={"resolve_blueprint": {}, "connect_blueprint_nodes": {"graph_name": "", "graph_id": "", "graph_type": "", "allow_pin_link_replacement": False}}
REQUIRED={"resolve_blueprint": ["blueprint_name"], "connect_blueprint_nodes": ["blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin"]}
FIELD_TYPES={"resolve_blueprint": {"blueprint_name": "string"}, "connect_blueprint_nodes": {"blueprint_name": "string", "source_node_id": "string", "source_pin": "string", "target_node_id": "string", "target_pin": "string", "graph_name": "string", "graph_id": "string", "graph_type": "string", "allow_pin_link_replacement": "boolean"}}
NULLABLE={"resolve_blueprint": [], "connect_blueprint_nodes": []}
ITEMS={"resolve_blueprint": {}, "connect_blueprint_nodes": {}}
OBSERVERS=["resolve_blueprint"]
NO_JOURNAL_METHODS=["resolve_blueprint"]
WRITERS=["connect_blueprint_nodes"]
_ACTIVE_REQUEST=False
_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp","/unrealmcp","unrealmcp","mcpunreal","mcp_unreal","/script/cubelesscontrolrigtools","cubelesscontrolrigtools","cubelesscontrolrigtoolseditor","cubelessmcp","cubeless_mcp","modelcontextprotocol","toolsetregistry","/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    text=value.strip().strip('\"\'').replace('\\','/').lower()
    return any(token in text for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)



def _reject_legacy_command_dependency_refs(params):
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('blueprint_resolution_connection_dependency_reference','Refusing MCP-only reference in '+field)
            error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)




def _public_error(field,kind,message):
    error=OperationError('blueprint_resolution_connection_public_argument_invalid',field+': '+message)
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


def normalize_public_mcp_arguments(method,arguments):
    if method not in METHODS or type(arguments) is not dict:raise OperationError('blueprint_resolution_connection_public_argument_invalid','Known method and public argument object required')
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
        if FIELD_TYPES[method][field]=='string' and type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        if FIELD_TYPES[method][field]=='boolean':value=_public_bool(field,value)
        values[field]=value
    return values

def binding_status():return api.binding_status()

def _arguments(method,arguments,legacy=None,public_input=False):
    if method not in METHODS or type(arguments) is not dict or type(public_input) is not bool:raise OperationError('blueprint_resolution_connection_entry_invalid','Known method, object and Boolean entry selector required')
    original=copy.deepcopy(arguments)
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('blueprint_resolution_connection_entry_invalid','Raw command and public model are distinct entries')
        params={key:copy.deepcopy(legacy[key]) for key in PARAM_FIELDS[method] if key in legacy}
        api._carrier(params);return copy.deepcopy(arguments),params,original
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={key:copy.deepcopy(values[key]) for key in PUBLIC_FIELDS[method] if key in values}
    if method=='connect_blueprint_nodes':
        # Original add_graph_selector omits only falsy selectors; raw presence is separate.
        for key in ('graph_name','graph_id','graph_type'):
            if not values.get(key):params.pop(key,None)
    api._carrier(params);return values,params,original

def _context(result,method,values,original,legacy,public_input):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),public_argument_validation_applied=public_input,
        raw_command_arguments=copy.deepcopy(legacy),original_Native_command=True,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),automatic_action_retry=False,atomic_rollback=False,
        operation_complete=False,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,
        change_state_known=True,mutation_state_unknown=False,mutation_performed=False,SDK_global_purity_unverified=False,
        actual_named_wire_verified=False,full_original_pipeline_complete=False,registration_dependency='ToolsetAPI',
        operation_dependency='dedicated_public_EngineAPI',server_RPC_dependency=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='not_requested')
    result['save'].update(requested=False,attempted=False,saved=False,status='not_requested')

def _checkpoint(result,callback):
    if callback:callback(result)

def _changes(result):
    data=result['data'];known=data['change_state_known'];attempted=data['native_mutation_attempted'];changed=data.get('observed_changed',False)
    data.update(execution_mutation_attempted=attempted,mutation_state_unknown=not known,mutation_performed=changed if known else None)
    result['changes']=[] if known and not attempted and not changed else [dict(kind='blueprint_resolution_connection',attempted=attempted,changed=changed if known else None,change_state_known=known)]
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_native_outcome_no_replay' if not known else 'inspect_recorded_Blueprint_changes_before_new_operation' if attempted else 'none_observed')



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
    data=result['data'];result['stage']='before_blueprint_resolution_connection_checkpoint';_checkpoint(result,callback)
    prior=data['change_state_known'];data.update(native_dispatch_pending=True,change_state_known=False);_changes(result)
    try:_checkpoint(result,callback)
    except Exception:data.update(native_dispatch_pending=False,change_state_known=prior);_changes(result);raise
    data.update(native_dispatch_pending=False,native_dispatched=True)
    try:native=getattr(api,method)(params)
    except Exception as error:
        if getattr(error,'native_call_dispatched',None) is False:data.update(native_dispatched=False,change_state_known=prior)
        if getattr(error,'native_call_returned',False):data['undecoded_native_raw_result']=getattr(error,'native_raw_result',None)
        _changes(result);result['stage']='returned_blueprint_resolution_connection_operation';_checkpoint(result,callback);raise
    _adopt(result,native,native.pop('_raw_native_json',None))
    result['stage']='returned_blueprint_resolution_connection_operation';_checkpoint(result,callback)
    if native.get('success') is not True or native.get('postcondition_passed') is not True:
        raise OperationError(native.get('error_code') or 'blueprint_resolution_connection_native_failed',native.get('error') or 'Native Blueprint postcondition failed; actual current prefix preserved')
    if native.get('data',{}).get('operation_complete') is False:
        data.update(change_state_known=False,operation_complete=False);_changes(result)
        raise OperationError('blueprint_resolution_connection_native_incomplete','Native receipt is incomplete; no replay')
    if any(native.get(key) is not False for key in ('compile_requested','compile_attempted','save_requested','save_attempted','saved')) or native.get('compiled') is not False:
        raise OperationError('blueprint_resolution_connection_unexpected_compile_or_save','Original resolve/connect APIs do not explicitly compile/save; actual prefix remains preserved')
    if method in OBSERVERS and (native.get('mutation_attempted') is not False or native.get('changed') not in (False,None)):
        raise OperationError('blueprint_resolution_connection_observer_mutated','Resolution observation returned explicit mutation; actual prefix remains preserved')
    if method in WRITERS and native.get('change_state_known') is not True:
        raise OperationError('blueprint_resolution_connection_outcome_unknown','Graph connection outcome is unknown; actual partial prefix retained')
    return result['data'].get('legacy_result')

def _finish(result,method,leaf):
    if type(leaf) is not dict:raise OperationError('blueprint_resolution_connection_legacy_invalid','Original legacy object is required')
    if method=='resolve_blueprint':
        if type(leaf.get('resolved')) is not bool or type(leaf.get('candidates')) is not list or any(type(p) is not str for p in leaf['candidates']):
            raise OperationError('blueprint_resolution_connection_legacy_invalid','Original resolved Boolean and ordered candidate paths are required')
        if leaf['resolved'] and (type(leaf.get('asset_path')) is not str or type(leaf.get('name')) is not str):
            raise OperationError('blueprint_resolution_connection_legacy_invalid','Resolved metadata requires original asset_path/name')
    elif leaf.get('connected') is not True or any(type(leaf.get(k)) is not str for k in ('source_node_id','target_node_id','source_pin','target_pin','schema_response','schema_message')) or type(leaf.get('graph')) is not dict:
        raise OperationError('blueprint_resolution_connection_legacy_invalid','Original connected/schema/graph metadata is required')
    result['data'].update(legacy_result=leaf,operation_complete=True)
    result['validation'].update(status='current_Native_typed_target_receipt',disk_reload='not_run',atomic_mutation=False,legacy_success_key_required=False)
    result.update(success=True,stage='complete');return result

def _run(result,method,values,params,original,legacy,public_input,callback):
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:raise OperationError('blueprint_resolution_connection_reentry','Reentry rejected before any Engine operation')
    _ACTIVE_REQUEST=True
    try:
        _context(result,method,values,original,legacy,public_input)
        leaf=_native(result,method,params,callback)
        return _finish(result,method,leaf)
    except Exception as error:
        result['data'].setdefault('legacy_result',dict(success=False,error=str(error),error_code=getattr(error,'code','blueprint_tool_failed')))
        raise
    finally:_ACTIVE_REQUEST=False

def _rejected(method,error):
    result=state.new_result();state.fail(result,error)
    result['stage']='blueprint_resolution_connection_input_validation'
    result['compile'].update(requested=False,attempted=False,compiled=False)
    result['save'].update(requested=False,attempted=False,saved=False)
    result['data'].update(public_method=method,native_dispatched=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,
        mutation_performed=False,journal_created=False,legacy_result=dict(success=False,error=str(error),error_code=getattr(error,'code','blueprint_tool_failed')))
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



def resolve_blueprint(blueprint_name):
    return _named("resolve_blueprint",{"blueprint_name":blueprint_name})

def resolve_blueprint_action(result,blueprint_name,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"resolve_blueprint",{"blueprint_name":blueprint_name},checkpoint_callback,_legacy_command_params,_public_input)

def connect_blueprint_nodes(blueprint_name,source_node_id,source_pin,target_node_id,target_pin,graph_name="",graph_id="",graph_type="",allow_pin_link_replacement=False):
    return _named("connect_blueprint_nodes",{"blueprint_name":blueprint_name,"source_node_id":source_node_id,"source_pin":source_pin,"target_node_id":target_node_id,"target_pin":target_pin,"graph_name":graph_name,"graph_id":graph_id,"graph_type":graph_type,"allow_pin_link_replacement":allow_pin_link_replacement})

def connect_blueprint_nodes_action(result,blueprint_name,source_node_id,source_pin,target_node_id,target_pin,graph_name="",graph_id="",graph_type="",allow_pin_link_replacement=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"connect_blueprint_nodes",{"blueprint_name":blueprint_name,"source_node_id":source_node_id,"source_pin":source_pin,"target_node_id":target_node_id,"target_pin":target_pin,"graph_name":graph_name,"graph_id":graph_id,"graph_type":graph_type,"allow_pin_link_replacement":allow_pin_link_replacement},checkpoint_callback,_legacy_command_params,_public_input)

