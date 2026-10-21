"""HF public Python utilities4; Native/H2/transport0. Preserve original legacy results.
Current SDK prefix precedes health/partial failure; observations are not atomic.
"""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import pcg_utilities as api
from . import state
from .state import OperationError
METHODS=["list_pcg_assets","list_pcg_components","set_pcg_debug_enabled","resave_pcg_assets"]
PUBLIC_FIELDS={"list_pcg_assets":["root_path"],"list_pcg_components":[],"set_pcg_debug_enabled":["enabled","actor_name","selected_only"],"resave_pcg_assets":["root_path"]}
PARAM_FIELDS=copy.deepcopy(PUBLIC_FIELDS)
DEFAULTS={"list_pcg_assets":{"root_path":"/Game"},"list_pcg_components":{},"set_pcg_debug_enabled":{"enabled":False,"actor_name":"","selected_only":False},"resave_pcg_assets":{"root_path":"/Game"}}
REQUIRED={name:[] for name in METHODS}
FIELD_TYPES={"list_pcg_assets":{"root_path":"string"},"list_pcg_components":{},"set_pcg_debug_enabled":{"enabled":"boolean","actor_name":"string","selected_only":"boolean"},"resave_pcg_assets":{"root_path":"string"}}
NULLABLE={name:[] for name in METHODS}
ITEMS={name:{} for name in METHODS}
OBSERVERS=["list_pcg_assets","list_pcg_components"]
NO_JOURNAL_METHODS=list(OBSERVERS)
WRITERS=["set_pcg_debug_enabled","resave_pcg_assets"]
_ACTIVE_REQUEST=False
_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp","/unrealmcp","unrealmcp","mcpunreal","mcp_unreal","/script/cubelesscontrolrigtools","cubelesscontrolrigtools","cubelesscontrolrigtoolseditor","cubelessmcp","cubeless_mcp","modelcontextprotocol","toolsetregistry","/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    text=value.strip().strip('\"\'').replace('\\','/').lower()
    return any(token in text for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('pcg_utilities_dependency_reference','Refusing MCP-only reference in '+field)
            error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('pcg_utilities_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('pcg_utilities_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('pcg_utilities_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('pcg_utilities_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values



def binding_status():return api.binding_status()


def _arguments(method,arguments,legacy,public_input):
    if method not in METHODS or type(arguments) is not dict or type(public_input) is not bool:raise OperationError('pcg_utilities_entry_invalid','Known method/argument object/model selector required')
    original=copy.deepcopy(arguments)
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('pcg_utilities_entry_invalid','Named model and Direct lower-name Batch alias are separate')
        arguments=legacy
    values=normalize_public_mcp_arguments(method,arguments) if public_input else dict(copy.deepcopy(DEFAULTS[method]),**{key:copy.deepcopy(arguments[key]) for key in PUBLIC_FIELDS[method] if key in arguments})
    try:json.dumps(values,ensure_ascii=False,allow_nan=False).encode('utf-16-le')
    except (ValueError,TypeError,OverflowError,UnicodeError,RecursionError) as error:raise OperationError('pcg_utilities_arguments_invalid','Finite valid Unicode public JSON data required') from error
    return values,original


def _adopt(result,receipt):
    # This is a current public Python SDK receipt, not a Native/RPC/raw result.
    data=result['data'];attempted=receipt.get('mutation_attempted') is True;known=receipt.get('change_state_known') is True;changed=receipt.get('observed_changed') is True
    data['current_Python_receipt']={key:copy.deepcopy(value) for key,value in receipt.items() if key!='legacy_result'}
    if type(receipt.get('legacy_result')) is dict:data['legacy_result']=copy.deepcopy(receipt['legacy_result'])
    data.update(execution_mutation_attempted=attempted,change_state_known=known,mutation_state_unknown=not known,
        mutation_performed=changed if known else None,SDK_global_purity_unverified=receipt.get('SDK_global_purity_unverified') is True)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='not_requested')
    result['save'].update(requested=receipt.get('save_requested') is True,attempted=receipt.get('save_attempted') is True,saved=result['save'].get('saved') is True or receipt.get('saved') is True,
        status='API_true_physical_file_write_not_proved' if receipt.get('saved') else 'attempted_without_API_true' if receipt.get('save_attempted') else 'not_attempted')
    result['changes']=[] if known and not attempted and not changed and not receipt.get('save_attempted') else [dict(kind='pcg_utility',attempted=attempted,changed=changed if known else None,change_state_known=known,save_attempted=receipt.get('save_attempted') is True,saved_API_true=receipt.get('saved') is True)]
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_scope_no_replay' if not known else 'inspect_partial_SDK_receipt_before_new_call' if attempted or receipt.get('save_attempted') else 'none_observed')
    result['stage']=receipt.get('stage','preflight')


def _run(result,method,values,original,legacy,public_input,callback):
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:raise OperationError('pcg_utilities_reentry','Same-domain reentry is refused before Editor work')
    _ACTIVE_REQUEST=True
    try:
        result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),lower_name_Batch_arguments=copy.deepcopy(legacy),public_argument_validation_applied=public_input,
            original_Native_command=False,hidden_fields=0,Native_leaf_required=False,registration_dependency='ToolsetAPI',operation_dependency='public_Editor_Python_SDK',server_RPC_dependency=False,
            automatic_action_retry=False,atomic_rollback=False,full_original_pipeline_complete=False,actual_named_wire_verified=False,
            change_state_scope='captured identity, requested readable debug flags, observed label changes and per-asset SDK save returns; not whole UObject/disk/global equality')
        receipt={}
        def checkpoint(current):
            _adopt(result,current)
            if callback:callback(result)
        try:api.execute(method,values,receipt,checkpoint)
        finally:_adopt(result,receipt)
        # Actual savedTrue/attempts/partial state have already been adopted.
        result['stage']='returned_pcg_utilities_operation'
        if callback:callback(result)
        if receipt.get('error_code'):raise OperationError(receipt['error_code'],receipt.get('error','Public Engine Python utility failed'))
        leaf=result['data'].get('legacy_result')
        if type(leaf) is not dict or leaf.get('success') is not True:raise OperationError('pcg_utilities_legacy_invalid','Original public utility success bool/result required')
        if not receipt.get('change_state_known'):raise OperationError('pcg_utilities_outcome_unknown','Actual partial target scope is unknown; no automatic replay')
        result['validation'].update(status='public_Python_current_request_scope',physical_file_write='not_proved',global_GC_exit_save_atomicity='not_proved',all_property_or_semantic_rollback='not_available',disk_reload='not_run')
        result.update(success=True,stage='complete');return result
    finally:_ACTIVE_REQUEST=False


def _rejected(method,error):
    result=state.new_result();state.fail(result,error)
    result['stage']='pcg_utilities_input_validation'
    result['compile'].update(requested=False,attempted=False,compiled=False)
    result['save'].update(requested=False,attempted=False,saved=False)
    result['data'].update(public_method=method,Native_leaf_required=False,SDK_dispatched=False,execution_mutation_attempted=False,
        change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False,
        legacy_result=dict(success=False,error=str(error),error_code=getattr(error,'code','pcg_utilities_failed')))
    if hasattr(error,'public_validation_errors'):result['data']['public_validation_errors']=copy.deepcopy(error.public_validation_errors)
    return result


def _named(method,arguments,public_input=False):
    try:values,original=_arguments(method,arguments,None,public_input)
    except Exception as error:return _rejected(method,error)
    if method in WRITERS:return state.mutate(method,arguments,None,lambda result:_run(result,method,values,original,None,public_input,state.checkpoint))
    result=state.new_result();result['data']['journal_created']=False
    try:return _run(result,method,values,original,None,public_input,None)
    except Exception as error:return state.fail(result,error)


def _named_mcp(method,arguments):return _named(method,arguments,True)


def _action(result,method,arguments,callback,legacy,public_input):
    result['data']['parent_journal_only']=True
    try:values,original=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        rejected=_rejected(method,error);rejected['operation_id']=result['operation_id'];result.update(rejected);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,values,original,legacy,public_input,callback)


def list_pcg_assets(root_path="/Game"):
    return _named('list_pcg_assets',dict(root_path=root_path))


def list_pcg_assets_action(result,root_path="/Game",checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'list_pcg_assets',dict(root_path=root_path),checkpoint_callback,_legacy_command_params,_public_input)


def list_pcg_components():
    return _named('list_pcg_components',dict())


def list_pcg_components_action(result,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'list_pcg_components',dict(),checkpoint_callback,_legacy_command_params,_public_input)


def set_pcg_debug_enabled(enabled=False,actor_name="",selected_only=False):
    return _named('set_pcg_debug_enabled',dict(enabled=enabled,actor_name=actor_name,selected_only=selected_only))


def set_pcg_debug_enabled_action(result,enabled=False,actor_name="",selected_only=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'set_pcg_debug_enabled',dict(enabled=enabled,actor_name=actor_name,selected_only=selected_only),checkpoint_callback,_legacy_command_params,_public_input)


def resave_pcg_assets(root_path="/Game"):
    return _named('resave_pcg_assets',dict(root_path=root_path))


def resave_pcg_assets_action(result,root_path="/Game",checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'resave_pcg_assets',dict(root_path=root_path),checkpoint_callback,_legacy_command_params,_public_input)

