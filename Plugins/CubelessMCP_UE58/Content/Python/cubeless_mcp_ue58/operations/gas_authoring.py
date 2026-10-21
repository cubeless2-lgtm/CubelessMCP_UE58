"""GAS6 public contract, Editor-only Native work, explicit compile/save/partial prefixes. No transport/fallback."""
import copy
import json
import math
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import gas_authoring as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"inspect_gas_blueprint": ["blueprint_path"], "create_gameplay_ability_blueprint": ["asset_name", "package_path", "save"], "create_gameplay_effect_blueprint": ["asset_name", "package_path", "save"], "create_attribute_set_blueprint": ["asset_name", "package_path", "save"], "add_ability_system_component_to_blueprint": ["blueprint_path", "component_name", "save"], "add_gameplay_attribute_to_attribute_set": ["attribute_set_path", "attribute_name", "category", "save"]}
DEFAULTS={"inspect_gas_blueprint": {}, "create_gameplay_ability_blueprint": {"package_path": "/Game/GAS/Abilities", "save": True}, "create_gameplay_effect_blueprint": {"package_path": "/Game/GAS/Effects", "save": True}, "create_attribute_set_blueprint": {"package_path": "/Game/GAS/Attributes", "save": True}, "add_ability_system_component_to_blueprint": {"component_name": "AbilitySystem", "save": True}, "add_gameplay_attribute_to_attribute_set": {"category": "GAS|Attributes", "save": True}}
REQUIRED={"inspect_gas_blueprint": ["blueprint_path"], "create_gameplay_ability_blueprint": ["asset_name"], "create_gameplay_effect_blueprint": ["asset_name"], "create_attribute_set_blueprint": ["asset_name"], "add_ability_system_component_to_blueprint": ["blueprint_path"], "add_gameplay_attribute_to_attribute_set": ["attribute_set_path", "attribute_name"]}
FIELD_TYPES={"inspect_gas_blueprint": {"blueprint_path": "string"}, "create_gameplay_ability_blueprint": {"asset_name": "string", "package_path": "string", "save": "boolean"}, "create_gameplay_effect_blueprint": {"asset_name": "string", "package_path": "string", "save": "boolean"}, "create_attribute_set_blueprint": {"asset_name": "string", "package_path": "string", "save": "boolean"}, "add_ability_system_component_to_blueprint": {"blueprint_path": "string", "component_name": "string", "save": "boolean"}, "add_gameplay_attribute_to_attribute_set": {"attribute_set_path": "string", "attribute_name": "string", "category": "string", "save": "boolean"}}
OBSERVERS=["inspect_gas_blueprint"]
WRITERS=["create_gameplay_ability_blueprint", "create_gameplay_effect_blueprint", "create_attribute_set_blueprint", "add_ability_system_component_to_blueprint", "add_gameplay_attribute_to_attribute_set"]
NULLABLE={"inspect_gas_blueprint": [], "create_gameplay_ability_blueprint": [], "create_gameplay_effect_blueprint": [], "create_attribute_set_blueprint": [], "add_ability_system_component_to_blueprint": [], "add_gameplay_attribute_to_attribute_set": []}
ITEMS={"inspect_gas_blueprint": {}, "create_gameplay_ability_blueprint": {}, "create_gameplay_effect_blueprint": {}, "create_attribute_set_blueprint": {}, "add_ability_system_component_to_blueprint": {}, "add_gameplay_attribute_to_attribute_set": {}}

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('gas_authoring_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


DEPENDENCY_FIELDS={'run_gas_authoring_smoke':()}
LEGACY_ERRORS={'run_gas_authoring_smoke':("Chaos destruction smoke operation failed","chaos_destruction_smoke_failed")}

def _public_error(field,kind,message):
    error=OperationError('gas_authoring_public_argument_invalid',field+': '+message)
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


def reject_mcp_dependency_reference(field,value):
    if _dependency(value):
        error=ValueError('Refusing MCP-only dependency');error.field_name=field;error.reason='mcp_only_reference';raise error

def tool_error(message,*,error_code='error',**fields):
    return dict(success=False,message=message,error=message,error_code=error_code,**fields)

_MAX_UNREAL_NAME_UNITS = 1023


def _utf16_units(value: str) -> int:
    return len(value.encode("utf-16-le", errors="surrogatepass")) // 2


def _validated_identifier(field_name: str, value: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{field_name} must be a string")
    normalized = value.strip()
    if not normalized:
        raise ValueError(f"{field_name} must not be empty")
    if _utf16_units(normalized) > _MAX_UNREAL_NAME_UNITS:
        raise ValueError(
            f"{field_name} exceeds the Unreal name limit of {_MAX_UNREAL_NAME_UNITS} UTF-16 code units"
        )
    if normalized.casefold() == "none":
        raise ValueError(f"{field_name} must not resolve to NAME_None")
    if not (normalized[0] == "_" or normalized[0].isalpha()):
        raise ValueError(f"{field_name} must begin with a letter or underscore")
    if not all(character == "_" or character.isalnum() for character in normalized):
        raise ValueError(f"{field_name} may contain only letters, numbers, and underscores")
    reject_mcp_dependency_reference(field_name, normalized)
    return normalized


def _validated_package_path(value: str) -> str:
    if not isinstance(value, str):
        raise ValueError("package_path must be a string")
    normalized = value.strip().replace("\\", "/").rstrip("/")
    if normalized != "/Game" and not normalized.startswith("/Game/"):
        raise ValueError("package_path must be /Game or a child of /Game")
    if "." in normalized or "//" in normalized:
        raise ValueError("package_path must be a package directory, not an object path")
    if _utf16_units(normalized) > _MAX_UNREAL_NAME_UNITS:
        raise ValueError("package_path exceeds the Unreal name limit")
    reject_mcp_dependency_reference("package_path", normalized)
    return normalized


def _validated_blueprint_path(field_name: str, value: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{field_name} must be a string")
    normalized = value.strip().strip("\"'").replace("\\", "/")
    if not normalized.startswith("/Game/") or "//" in normalized:
        raise ValueError(f"{field_name} must identify a Blueprint under /Game")
    if _utf16_units(normalized) > _MAX_UNREAL_NAME_UNITS:
        raise ValueError(f"{field_name} exceeds the Unreal name limit")
    reject_mcp_dependency_reference(field_name, normalized)
    return normalized


def _validated_bool(field_name: str, value: bool) -> bool:
    if not isinstance(value, bool):
        raise ValueError(f"{field_name} must be a boolean")
    return value


def _validation_error(exc: ValueError) -> Dict[str, Any]:
    if getattr(exc, "reason", "") == "mcp_only_reference":
        field_name = str(getattr(exc, "field_name", "value"))
        message = f"Rejected MCP-only reference in {field_name}"
        error_code = "mcp_dependency_reference_rejected"
    else:
        message = str(exc)
        error_code = "gas_invalid_request"
    return tool_error(
        message,
        error_code=error_code,
        mutation_attempted=False,
        mutation_performed=False,
        mutation_state_unknown=False,
        changed=False,
    )


def normalize_public_mcp_arguments(method,arguments):
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('gas_authoring_public_argument_invalid','Known method and public object required')
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
        if FIELD_TYPES[method][field]=='boolean':value=_public_bool(field,value)
        elif type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        values[field]=value
    return values

def _body_arguments(method,values):
    if method=='inspect_gas_blueprint':return dict(blueprint_path=_validated_blueprint_path('blueprint_path',values['blueprint_path']))
    if method in ('create_gameplay_ability_blueprint','create_gameplay_effect_blueprint','create_attribute_set_blueprint'):
        return dict(asset_name=_validated_identifier('asset_name',values['asset_name']),package_path=_validated_package_path(values['package_path']),save=_validated_bool('save',values['save']))
    if method=='add_ability_system_component_to_blueprint':
        return dict(blueprint_path=_validated_blueprint_path('blueprint_path',values['blueprint_path']),component_name=_validated_identifier('component_name',values['component_name']),save=_validated_bool('save',values['save']))
    category=values['category'].strip() if isinstance(values['category'],str) else ''
    if not category:raise ValueError('category must be a non-empty string')
    reject_mcp_dependency_reference('category',category)
    return dict(attribute_set_path=_validated_blueprint_path('attribute_set_path',values['attribute_set_path']),attribute_name=_validated_identifier('attribute_name',values['attribute_name']),category=category,save=_validated_bool('save',values['save']))

LEGACY_MESSAGES=dict(zip(METHODS,('Failed to inspect GAS Blueprint','Failed to create GameplayAbility Blueprint','Failed to create GameplayEffect Blueprint','Failed to create AttributeSet Blueprint','Failed to add AbilitySystemComponent','Failed to add gameplay attribute')))
def _legacy_exception(method,error):
    return dict(success=False,message=LEGACY_MESSAGES[method],error=LEGACY_MESSAGES[method],error_code='gas_tool_failed',details=dict(exception_type=type(error).__name__),mutation_attempted=False,mutation_performed=False,mutation_state_unknown=False,changed=False)

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('gas_authoring_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('gas_authoring_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    try:params=_body_arguments(method,values)
    except ValueError as error:
        legacy_error=_validation_error(error);wrapped=OperationError(legacy_error['error_code'],legacy_error['message']);wrapped.legacy_result=legacy_error;raise wrapped from error
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_validators_preserved=True,raw_strict_boolean_and_presence_Native_owned=True,hidden_fields=0,Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if method in OBSERVERS else 'write',original_inferred_risk='low' if method in OBSERVERS else 'medium',registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_GameplayAbilities_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_gas_authoring_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_gas_authoring_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_gas_authoring_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_compile_save_dependency_audit_cleanup_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_GAS_compile_save_audit_cleanup_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_gas_authoring_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_gas_authoring_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_gas_authoring_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_gas_authoring_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'gas_authoring_native_failed',native.get('error') or 'Native gas_authoring failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('gas_authoring_postcondition_unverified','Native gas_authoring postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is True or native['saved']):
        raise OperationError('gas_authoring_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='gas_authoring_input_validation';result['changes']=[]
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    if hasattr(error,'legacy_result'):result['data']['legacy_result']=copy.deepcopy(error.legacy_result)
    if hasattr(error,'public_validation_errors'):result['data'].update(public_validation_errors=copy.deepcopy(error.public_validation_errors),original_framework_error_text_reproduced=False,tool_body_invoked=False)
    return result

def _rejected(method,error):return _rejection(state.new_result(),method,error)

def _named(method,arguments,_public_input=False):
    try:args,original,legacy=_arguments(method,arguments,None,_public_input)
    except Exception as error:
        rejected=_rejected(method,error);rejected['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',None if hasattr(error,'public_validation_errors') else _legacy_exception(method,error)));return rejected
    if method in OBSERVERS:
        result=state.new_result()
        try:return _run(result,method,args,original,legacy)
        except Exception as error:return state.fail(result,error)
    return state.mutate(method,arguments,None,lambda result:_run(result,method,args,original,legacy,state.checkpoint))

def _action(result,method,arguments,callback,legacy,public_input=False):
    result["data"]["parent_journal_only"]=True
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)

def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)

def inspect_gas_blueprint(blueprint_path):
    return _named("inspect_gas_blueprint",dict(blueprint_path=blueprint_path))

def inspect_gas_blueprint_action(result,blueprint_path,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_gas_blueprint",dict(blueprint_path=blueprint_path),checkpoint_callback,_legacy_command_params,_public_input)

def create_gameplay_ability_blueprint(asset_name,package_path="/Game/GAS/Abilities",save=True):
    return _named("create_gameplay_ability_blueprint",dict(asset_name=asset_name,package_path=package_path,save=save))

def create_gameplay_ability_blueprint_action(result,asset_name,package_path="/Game/GAS/Abilities",save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_gameplay_ability_blueprint",dict(asset_name=asset_name,package_path=package_path,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def create_gameplay_effect_blueprint(asset_name,package_path="/Game/GAS/Effects",save=True):
    return _named("create_gameplay_effect_blueprint",dict(asset_name=asset_name,package_path=package_path,save=save))

def create_gameplay_effect_blueprint_action(result,asset_name,package_path="/Game/GAS/Effects",save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_gameplay_effect_blueprint",dict(asset_name=asset_name,package_path=package_path,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def create_attribute_set_blueprint(asset_name,package_path="/Game/GAS/Attributes",save=True):
    return _named("create_attribute_set_blueprint",dict(asset_name=asset_name,package_path=package_path,save=save))

def create_attribute_set_blueprint_action(result,asset_name,package_path="/Game/GAS/Attributes",save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_attribute_set_blueprint",dict(asset_name=asset_name,package_path=package_path,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def add_ability_system_component_to_blueprint(blueprint_path,component_name="AbilitySystem",save=True):
    return _named("add_ability_system_component_to_blueprint",dict(blueprint_path=blueprint_path,component_name=component_name,save=save))

def add_ability_system_component_to_blueprint_action(result,blueprint_path,component_name="AbilitySystem",save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"add_ability_system_component_to_blueprint",dict(blueprint_path=blueprint_path,component_name=component_name,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def add_gameplay_attribute_to_attribute_set(attribute_set_path,attribute_name,category="GAS|Attributes",save=True):
    return _named("add_gameplay_attribute_to_attribute_set",dict(attribute_set_path=attribute_set_path,attribute_name=attribute_name,category=category,save=save))

def add_gameplay_attribute_to_attribute_set_action(result,attribute_set_path,attribute_name,category="GAS|Attributes",save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"add_gameplay_attribute_to_attribute_set",dict(attribute_set_path=attribute_set_path,attribute_name=attribute_name,category=category,save=save),checkpoint_callback,_legacy_command_params,_public_input)

