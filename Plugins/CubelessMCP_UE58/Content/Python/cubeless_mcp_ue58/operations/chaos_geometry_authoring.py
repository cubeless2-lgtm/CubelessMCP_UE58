"""FX Geometry Collection authoring3. Engine work is Native-owned.
Legacy assertion/save leaves are separate from actual SDK save/cleanup/reload receipts.
"""
import copy,json,math,re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import chaos_geometry_authoring as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={'create_geometry_collection_from_mesh': ('source_static_mesh_paths', 'output_geometry_collection_path', 'source_transforms', 'root_mesh_proxy_static_mesh_path', 'material_reindex', 'overwrite_existing'), 'configure_building_fracture': ('source_geometry_collection_path', 'output_geometry_collection_path', 'fracture_method', 'site_count', 'random_seed', 'cluster_levels', 'cluster_size', 'damage_thresholds', 'merge_tiny_geometry', 'tiny_relative_threshold', 'collision_object_reduction_percentage', 'maximum_collision_particles', 'collision_implicit_type', 'remove_on_sleep', 'remove_on_break', 'sleep_time_seconds', 'removal_duration_seconds', 'break_timer_seconds', 'root_mesh_proxy_static_mesh_path', 'overwrite_existing'), 'configure_anchor_field': ('geometry_collection_actor_name', 'field_actor_name', 'box_center', 'box_extent', 'field_magnitude', 'save_level')}
DEFAULTS={'create_geometry_collection_from_mesh': {'source_transforms': None, 'root_mesh_proxy_static_mesh_path': '', 'material_reindex': True, 'overwrite_existing': False}, 'configure_building_fracture': {'fracture_method': 'uniform_voronoi', 'site_count': 64, 'random_seed': 1337, 'cluster_levels': 2, 'cluster_size': 8, 'damage_thresholds': None, 'merge_tiny_geometry': True, 'tiny_relative_threshold': 0.01, 'collision_object_reduction_percentage': 20, 'maximum_collision_particles': 60, 'collision_implicit_type': 'convex', 'remove_on_sleep': True, 'remove_on_break': True, 'sleep_time_seconds': 5, 'removal_duration_seconds': 1, 'break_timer_seconds': 1, 'root_mesh_proxy_static_mesh_path': '', 'overwrite_existing': False}, 'configure_anchor_field': {'field_actor_name': 'Chaos_AnchorField', 'box_center': None, 'box_extent': None, 'field_magnitude': 2, 'save_level': True}}
REQUIRED={'create_geometry_collection_from_mesh': ('source_static_mesh_paths', 'output_geometry_collection_path'), 'configure_building_fracture': ('source_geometry_collection_path', 'output_geometry_collection_path'), 'configure_anchor_field': ('geometry_collection_actor_name',)}
FIELD_TYPES={'create_geometry_collection_from_mesh': {'source_static_mesh_paths': 'array', 'output_geometry_collection_path': 'string', 'source_transforms': 'array', 'root_mesh_proxy_static_mesh_path': 'string', 'material_reindex': 'boolean', 'overwrite_existing': 'boolean'}, 'configure_building_fracture': {'source_geometry_collection_path': 'string', 'output_geometry_collection_path': 'string', 'fracture_method': 'string', 'site_count': 'integer', 'random_seed': 'integer', 'cluster_levels': 'integer', 'cluster_size': 'integer', 'damage_thresholds': 'array', 'merge_tiny_geometry': 'boolean', 'tiny_relative_threshold': 'number', 'collision_object_reduction_percentage': 'number', 'maximum_collision_particles': 'integer', 'collision_implicit_type': 'string', 'remove_on_sleep': 'boolean', 'remove_on_break': 'boolean', 'sleep_time_seconds': 'number', 'removal_duration_seconds': 'number', 'break_timer_seconds': 'number', 'root_mesh_proxy_static_mesh_path': 'string', 'overwrite_existing': 'boolean'}, 'configure_anchor_field': {'geometry_collection_actor_name': 'string', 'field_actor_name': 'string', 'box_center': 'array', 'box_extent': 'array', 'field_magnitude': 'integer', 'save_level': 'boolean'}}
NULLABLE={'create_geometry_collection_from_mesh': {'source_transforms'}, 'configure_building_fracture': {'damage_thresholds'}, 'configure_anchor_field': {'box_extent', 'box_center'}}
ITEMS={'create_geometry_collection_from_mesh': {'source_static_mesh_paths': 'string', 'source_transforms': 'object'}, 'configure_building_fracture': {'damage_thresholds': 'number'}, 'configure_anchor_field': {'box_center': 'number', 'box_extent': 'number'}}
OBSERVERS=()
WRITERS=tuple(METHODS)
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('chaos_geometry_authoring_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


DEPENDENCY_FIELDS={'create_geometry_collection_from_mesh':('output_geometry_collection_path','root_mesh_proxy_static_mesh_path'),'configure_building_fracture':('source_geometry_collection_path','output_geometry_collection_path','root_mesh_proxy_static_mesh_path'),'configure_anchor_field':()}
LEGACY_ERRORS={'create_geometry_collection_from_mesh':('Geometry Collection conversion failed','chaos_cinematic_conversion_failed'),'configure_building_fracture':('Building fracture configuration failed','chaos_cinematic_fracture_failed'),'configure_anchor_field':('Anchor Field configuration failed','chaos_cinematic_anchor_failed')}
def _legacy_exception(method,error):
    message,code=LEGACY_ERRORS[method]
    return dict(success=False,message=message,error=message,error_code=code,details={'exception_type':type(error).__name__})
def _early_error(code,message):
    error=OperationError(code,message);error.legacy_result=dict(success=False,error=message,message=message,error_code=code);raise error

def _public_error(field,kind,message):
    error=OperationError('chaos_geometry_authoring_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('chaos_geometry_authoring_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]=='object':
                        if type(item) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
                        # JSON object keys are strings. Values remain the original nested Any.
                        for key in item:
                            if type(key) is not str:raise _public_error(field,'string_type','Input should be a valid string')
                        item=copy.deepcopy(item)
                    elif ITEMS[method][field]=='integer':item=_public_int(field,item)
                    else:item=_public_float(field,item)
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('chaos_geometry_authoring_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('chaos_geometry_authoring_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('chaos_geometry_authoring_entry_invalid','Raw and public entries are separate')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params)
        read_only=method in ('configure_building_fracture','configure_anchor_field') and params.get('operation')=='inspect_saved'
        return dict(params=params,entry='raw_legacy',public_input=False,read_only=read_only),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    if method=='create_geometry_collection_from_mesh' and not values['source_static_mesh_paths']:
        _early_error('chaos_cinematic_invalid_source_meshes','source_static_mesh_paths must not be empty')
    try:
        for field in DEPENDENCY_FIELDS[method]:
            if field=='root_mesh_proxy_static_mesh_path':continue
            if _dependency(values.get(field)):raise ValueError('MCP-only dependency reference rejected')
        if method=='create_geometry_collection_from_mesh':
            for path in values['source_static_mesh_paths']:
                if _dependency(path):raise ValueError('MCP-only dependency reference rejected')
    except Exception as error:
        wrapped=OperationError(LEGACY_ERRORS[method][1],LEGACY_ERRORS[method][0]);wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    if method=='configure_building_fracture' and values['source_geometry_collection_path']==values['output_geometry_collection_path']:
        _early_error('chaos_cinematic_source_overwrite_blocked','source and output Geometry Collection paths must differ')
    try:
        if _dependency(values.get('root_mesh_proxy_static_mesh_path')):raise ValueError('MCP-only dependency reference rejected')
    except Exception as error:
        wrapped=OperationError(LEGACY_ERRORS[method][1],LEGACY_ERRORS[method][0]);wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
    if method=='create_geometry_collection_from_mesh' and params['source_transforms'] is None:params['source_transforms']=[]
    elif method=='configure_building_fracture' and params['damage_thresholds'] is None:params['damage_thresholds']=[500000.0,250000.0,100000.0]
    elif method=='configure_anchor_field':
        if params['box_center'] is None:params['box_center']=[0.0,0.0,0.0]
        if params['box_extent'] is None:params['box_extent']=[100.0,100.0,100.0]
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None,read_only=False),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_raw_operation=copy.deepcopy(args['params'].get('operation')),requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),read_only_direct_API=args['read_only'],
        Named_dependency_guard_count=len(DEPENDENCY_FIELDS[method]),Batch_recursive_string_value_guard_owned_by_Batch=True,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Named_anchor_default1_raw_omitted_Kinematic2_preserved=False,anchor_public_default_native_Kinematic2=True,legacy_default1_corrected_without_value_remapping=True,anchor_invalid_magnitude_precedes_engine_load_and_writes=True,legacy_vacuous_save_is_not_actual_SDK_save=True,
        Native_reload_and_cleanup_and_partial_save_prefix_authoritative=True,managed_writer_idle_differs_from_original_Native_PIE=True,
        SDK_internal_waits='Synchronous fracture/build/render/reload SDK calls are not preempted by wall timeout',
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_chaos_geometry_authoring_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_chaos_geometry_authoring_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_chaos_geometry_authoring_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_geometry_fracture_anchor_source_reload_save_cleanup_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_geometry_fracture_anchor_source_reload_save_cleanup_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_chaos_geometry_authoring_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_chaos_geometry_authoring_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_chaos_geometry_authoring_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_chaos_geometry_authoring_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'chaos_geometry_authoring_native_failed',native.get('error') or 'Native chaos_geometry_authoring failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('chaos_geometry_authoring_postcondition_unverified','Native chaos_geometry_authoring postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if args['read_only'] and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is True or native['saved']):
        raise OperationError('chaos_geometry_authoring_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='chaos_geometry_authoring_input_validation';result['changes']=[]
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


def create_geometry_collection_from_mesh(source_static_mesh_paths:list,output_geometry_collection_path:str,source_transforms:list=None,root_mesh_proxy_static_mesh_path:str='',material_reindex:bool=True,overwrite_existing:bool=False)->Dict[str,Any]:
    return _named('create_geometry_collection_from_mesh',dict(source_static_mesh_paths=source_static_mesh_paths,output_geometry_collection_path=output_geometry_collection_path,source_transforms=source_transforms,root_mesh_proxy_static_mesh_path=root_mesh_proxy_static_mesh_path,material_reindex=material_reindex,overwrite_existing=overwrite_existing))


def create_geometry_collection_from_mesh_action(result,source_static_mesh_paths=None,output_geometry_collection_path=None,source_transforms=None,root_mesh_proxy_static_mesh_path='',material_reindex=True,overwrite_existing=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'create_geometry_collection_from_mesh',dict(source_static_mesh_paths=source_static_mesh_paths,output_geometry_collection_path=output_geometry_collection_path,source_transforms=source_transforms,root_mesh_proxy_static_mesh_path=root_mesh_proxy_static_mesh_path,material_reindex=material_reindex,overwrite_existing=overwrite_existing),checkpoint_callback,_legacy_command_params,_public_input)


def configure_building_fracture(source_geometry_collection_path:str,output_geometry_collection_path:str,fracture_method:str='uniform_voronoi',site_count:int=64,random_seed:int=1337,cluster_levels:int=2,cluster_size:int=8,damage_thresholds:list=None,merge_tiny_geometry:bool=True,tiny_relative_threshold:float=0.01,collision_object_reduction_percentage:float=20,maximum_collision_particles:int=60,collision_implicit_type:str='convex',remove_on_sleep:bool=True,remove_on_break:bool=True,sleep_time_seconds:float=5,removal_duration_seconds:float=1,break_timer_seconds:float=1,root_mesh_proxy_static_mesh_path:str='',overwrite_existing:bool=False)->Dict[str,Any]:
    return _named('configure_building_fracture',dict(source_geometry_collection_path=source_geometry_collection_path,output_geometry_collection_path=output_geometry_collection_path,fracture_method=fracture_method,site_count=site_count,random_seed=random_seed,cluster_levels=cluster_levels,cluster_size=cluster_size,damage_thresholds=damage_thresholds,merge_tiny_geometry=merge_tiny_geometry,tiny_relative_threshold=tiny_relative_threshold,collision_object_reduction_percentage=collision_object_reduction_percentage,maximum_collision_particles=maximum_collision_particles,collision_implicit_type=collision_implicit_type,remove_on_sleep=remove_on_sleep,remove_on_break=remove_on_break,sleep_time_seconds=sleep_time_seconds,removal_duration_seconds=removal_duration_seconds,break_timer_seconds=break_timer_seconds,root_mesh_proxy_static_mesh_path=root_mesh_proxy_static_mesh_path,overwrite_existing=overwrite_existing))


def configure_building_fracture_action(result,source_geometry_collection_path=None,output_geometry_collection_path=None,fracture_method='uniform_voronoi',site_count=64,random_seed=1337,cluster_levels=2,cluster_size=8,damage_thresholds=None,merge_tiny_geometry=True,tiny_relative_threshold=0.01,collision_object_reduction_percentage=20,maximum_collision_particles=60,collision_implicit_type='convex',remove_on_sleep=True,remove_on_break=True,sleep_time_seconds=5,removal_duration_seconds=1,break_timer_seconds=1,root_mesh_proxy_static_mesh_path='',overwrite_existing=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'configure_building_fracture',dict(source_geometry_collection_path=source_geometry_collection_path,output_geometry_collection_path=output_geometry_collection_path,fracture_method=fracture_method,site_count=site_count,random_seed=random_seed,cluster_levels=cluster_levels,cluster_size=cluster_size,damage_thresholds=damage_thresholds,merge_tiny_geometry=merge_tiny_geometry,tiny_relative_threshold=tiny_relative_threshold,collision_object_reduction_percentage=collision_object_reduction_percentage,maximum_collision_particles=maximum_collision_particles,collision_implicit_type=collision_implicit_type,remove_on_sleep=remove_on_sleep,remove_on_break=remove_on_break,sleep_time_seconds=sleep_time_seconds,removal_duration_seconds=removal_duration_seconds,break_timer_seconds=break_timer_seconds,root_mesh_proxy_static_mesh_path=root_mesh_proxy_static_mesh_path,overwrite_existing=overwrite_existing),checkpoint_callback,_legacy_command_params,_public_input)


def configure_anchor_field(geometry_collection_actor_name:str,field_actor_name:str='Chaos_AnchorField',box_center:list=None,box_extent:list=None,field_magnitude:int=2,save_level:bool=True)->Dict[str,Any]:
    return _named('configure_anchor_field',dict(geometry_collection_actor_name=geometry_collection_actor_name,field_actor_name=field_actor_name,box_center=box_center,box_extent=box_extent,field_magnitude=field_magnitude,save_level=save_level))


def configure_anchor_field_action(result,geometry_collection_actor_name=None,field_actor_name='Chaos_AnchorField',box_center=None,box_extent=None,field_magnitude=2,save_level=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'configure_anchor_field',dict(geometry_collection_actor_name=geometry_collection_actor_name,field_actor_name=field_actor_name,box_center=box_center,box_extent=box_extent,field_magnitude=field_magnitude,save_level=save_level),checkpoint_callback,_legacy_command_params,_public_input)


