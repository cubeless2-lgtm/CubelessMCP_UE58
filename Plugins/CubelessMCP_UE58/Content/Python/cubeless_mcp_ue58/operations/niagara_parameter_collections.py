"""GG default-store and script/graph reference6. Original public/direct/raw input, ordered partial save receipts; Native owns Engine work. No transport or replay."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_parameter_collections as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"inspect_niagara_parameter_collection_values": ["collection_path", "parameter_names", "namespace", "max_parameters"], "set_niagara_parameter_collection_values": ["collection_path", "values", "namespace", "allow_source_edit", "save"], "repair_niagara_parameter_collection": ["collection_path", "allow_source_edit", "save"], "remap_niagara_parameter_collection_references": ["system_path", "new_collection_path", "old_collection_path", "old_namespace", "allow_source_edit", "save", "request_compile", "replace_object_references"], "remap_niagara_script_parameter_collection_references": ["new_collection_path", "old_collection_path", "old_namespace", "script_path", "script_paths", "root_path", "allow_source_edit", "save", "request_compile", "wait_for_completion"], "recompile_niagara_scripts_in_root": ["root_path", "script_paths", "allow_source_compile", "force", "save"]}
DEFAULTS={"inspect_niagara_parameter_collection_values": {"parameter_names": None, "namespace": "", "max_parameters": 256}, "set_niagara_parameter_collection_values": {"namespace": "", "allow_source_edit": False, "save": True}, "repair_niagara_parameter_collection": {"allow_source_edit": False, "save": True}, "remap_niagara_parameter_collection_references": {"old_collection_path": "", "old_namespace": "", "allow_source_edit": False, "save": True, "request_compile": True, "replace_object_references": False}, "remap_niagara_script_parameter_collection_references": {"old_collection_path": "", "old_namespace": "", "script_path": "", "script_paths": None, "root_path": "", "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": True}, "recompile_niagara_scripts_in_root": {"root_path": "", "script_paths": None, "allow_source_compile": False, "force": True, "save": True}}
REQUIRED={"inspect_niagara_parameter_collection_values": ["collection_path"], "set_niagara_parameter_collection_values": ["collection_path", "values"], "repair_niagara_parameter_collection": ["collection_path"], "remap_niagara_parameter_collection_references": ["system_path", "new_collection_path"], "remap_niagara_script_parameter_collection_references": ["new_collection_path"], "recompile_niagara_scripts_in_root": []}
FIELD_TYPES={"inspect_niagara_parameter_collection_values": {"collection_path": "string", "parameter_names": "array", "namespace": "string", "max_parameters": "integer"}, "set_niagara_parameter_collection_values": {"collection_path": "string", "values": "object", "namespace": "string", "allow_source_edit": "boolean", "save": "boolean"}, "repair_niagara_parameter_collection": {"collection_path": "string", "allow_source_edit": "boolean", "save": "boolean"}, "remap_niagara_parameter_collection_references": {"system_path": "string", "new_collection_path": "string", "old_collection_path": "string", "old_namespace": "string", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "replace_object_references": "boolean"}, "remap_niagara_script_parameter_collection_references": {"new_collection_path": "string", "old_collection_path": "string", "old_namespace": "string", "script_path": "string", "script_paths": "array", "root_path": "string", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean"}, "recompile_niagara_scripts_in_root": {"root_path": "string", "script_paths": "array", "allow_source_compile": "boolean", "force": "boolean", "save": "boolean"}}
NULLABLE={"inspect_niagara_parameter_collection_values": ["parameter_names"], "set_niagara_parameter_collection_values": [], "repair_niagara_parameter_collection": [], "remap_niagara_parameter_collection_references": [], "remap_niagara_script_parameter_collection_references": ["script_paths"], "recompile_niagara_scripts_in_root": ["script_paths"]}
ITEMS={"inspect_niagara_parameter_collection_values": {"parameter_names": "string"}, "set_niagara_parameter_collection_values": {}, "repair_niagara_parameter_collection": {}, "remap_niagara_parameter_collection_references": {}, "remap_niagara_script_parameter_collection_references": {"script_paths": "any"}, "recompile_niagara_scripts_in_root": {"script_paths": "string"}}
OBSERVERS=["inspect_niagara_parameter_collection_values"]
WRITERS=["set_niagara_parameter_collection_values", "repair_niagara_parameter_collection", "remap_niagara_parameter_collection_references", "remap_niagara_script_parameter_collection_references", "recompile_niagara_scripts_in_root"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_parameter_collections_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_parameter_collections_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_parameter_collections_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_parameter_collections_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('niagara_parameter_collections_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

LEGACY_MESSAGES={"inspect_niagara_parameter_collection_values": "Error inspecting Niagara parameter collection values: ", "set_niagara_parameter_collection_values": "Error setting Niagara parameter collection values: ", "repair_niagara_parameter_collection": "Error repairing Niagara parameter collection: ", "remap_niagara_parameter_collection_references": "Error remapping Niagara parameter collection references: ", "remap_niagara_script_parameter_collection_references": "Error remapping Niagara script parameter collection references: ", "recompile_niagara_scripts_in_root": "Error recompiling Niagara scripts: "}
def _legacy_exception(method,error):
    message=LEGACY_MESSAGES[method]+str(error)
    return dict(success=False,message=message,error=message,error_code="niagara_tool_failed")

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_parameter_collections_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_parameter_collections_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method] if field not in NULLABLE[method] or values[field] is not None}
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_omission_preserved=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if method in OBSERVERS else 'write',original_inferred_risk='low' if method in OBSERVERS else 'high' if method in ('remap_niagara_parameter_collection_references','remap_niagara_script_parameter_collection_references','recompile_niagara_scripts_in_root') else 'medium',nonblocking_script_compile_wait_save_implementation_pending=not args.get('concrete_native_job',False),registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_parameter_collections_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_parameter_collections_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_parameter_collections_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_default_store_graph_VM_compile_save_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_parameter_collections_compile_save_remap_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_parameter_collections_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_parameter_collections_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_parameter_collections_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_parameter_collections_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_parameter_collections_native_failed',native.get('error') or 'Native niagara_parameter_collections failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_parameter_collections_postcondition_unverified','Native niagara_parameter_collections postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is True or native['saved']):
        raise OperationError('niagara_parameter_collections_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_parameter_collections_input_validation';result['changes']=[]
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

def completed_result(record):
    from . import niagara_scratch_user_recipes as shared
    return shared.completed_result(record)

def _named_mcp(method,arguments):
    if method=='remap_niagara_script_parameter_collection_references':
        from . import niagara_scratch_user_recipes as shared
        return shared._named_module_input_binding(method,arguments)
    return _named(method,arguments,_public_input=True)

def inspect_niagara_parameter_collection_values(collection_path,parameter_names=None,namespace="",max_parameters=256):
    return _named("inspect_niagara_parameter_collection_values",dict(collection_path=collection_path,parameter_names=parameter_names,namespace=namespace,max_parameters=max_parameters))

def inspect_niagara_parameter_collection_values_action(result,collection_path,parameter_names=None,namespace="",max_parameters=256,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_niagara_parameter_collection_values",dict(collection_path=collection_path,parameter_names=parameter_names,namespace=namespace,max_parameters=max_parameters),checkpoint_callback,_legacy_command_params,_public_input)

def set_niagara_parameter_collection_values(collection_path,values,namespace="",allow_source_edit=False,save=True):
    return _named("set_niagara_parameter_collection_values",dict(collection_path=collection_path,values=values,namespace=namespace,allow_source_edit=allow_source_edit,save=save))

def set_niagara_parameter_collection_values_action(result,collection_path,values,namespace="",allow_source_edit=False,save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"set_niagara_parameter_collection_values",dict(collection_path=collection_path,values=values,namespace=namespace,allow_source_edit=allow_source_edit,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def repair_niagara_parameter_collection(collection_path,allow_source_edit=False,save=True):
    return _named("repair_niagara_parameter_collection",dict(collection_path=collection_path,allow_source_edit=allow_source_edit,save=save))

def repair_niagara_parameter_collection_action(result,collection_path,allow_source_edit=False,save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"repair_niagara_parameter_collection",dict(collection_path=collection_path,allow_source_edit=allow_source_edit,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def remap_niagara_parameter_collection_references(system_path,new_collection_path,old_collection_path="",old_namespace="",allow_source_edit=False,save=True,request_compile=True,replace_object_references=False):
    return _named("remap_niagara_parameter_collection_references",dict(system_path=system_path,new_collection_path=new_collection_path,old_collection_path=old_collection_path,old_namespace=old_namespace,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,replace_object_references=replace_object_references))

def remap_niagara_parameter_collection_references_action(result,system_path,new_collection_path,old_collection_path="",old_namespace="",allow_source_edit=False,save=True,request_compile=True,replace_object_references=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"remap_niagara_parameter_collection_references",dict(system_path=system_path,new_collection_path=new_collection_path,old_collection_path=old_collection_path,old_namespace=old_namespace,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,replace_object_references=replace_object_references),checkpoint_callback,_legacy_command_params,_public_input)

def remap_niagara_script_parameter_collection_references(new_collection_path,old_collection_path="",old_namespace="",script_path="",script_paths=None,root_path="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True):
    return _named("remap_niagara_script_parameter_collection_references",dict(new_collection_path=new_collection_path,old_collection_path=old_collection_path,old_namespace=old_namespace,script_path=script_path,script_paths=script_paths,root_path=root_path,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion))

def remap_niagara_script_parameter_collection_references_action(result,new_collection_path,old_collection_path="",old_namespace="",script_path="",script_paths=None,root_path="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"remap_niagara_script_parameter_collection_references",dict(new_collection_path=new_collection_path,old_collection_path=old_collection_path,old_namespace=old_namespace,script_path=script_path,script_paths=script_paths,root_path=root_path,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion),checkpoint_callback,_legacy_command_params,_public_input)

def recompile_niagara_scripts_in_root(root_path="",script_paths=None,allow_source_compile=False,force=True,save=True):
    return _named("recompile_niagara_scripts_in_root",dict(root_path=root_path,script_paths=script_paths,allow_source_compile=allow_source_compile,force=force,save=save))

def recompile_niagara_scripts_in_root_action(result,root_path="",script_paths=None,allow_source_compile=False,force=True,save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"recompile_niagara_scripts_in_root",dict(root_path=root_path,script_paths=script_paths,allow_source_compile=allow_source_compile,force=force,save=save),checkpoint_callback,_legacy_command_params,_public_input)
