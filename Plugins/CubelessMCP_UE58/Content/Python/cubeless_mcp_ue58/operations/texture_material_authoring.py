"""HJ public texture material composites3. Current effects precede health/final delivery. No transport."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import texture_material_authoring as api
from . import state
from .state import OperationError
METHODS=["get_static_mesh_uv_layout","create_material_instance_with_texture","create_material_with_texture_set"]
PUBLIC_FIELDS={"get_static_mesh_uv_layout":["mesh_path","uv_channel","output_path"],"create_material_instance_with_texture":["texture_asset_path","material_name","unreal_folder","base_material_path"],"create_material_with_texture_set":["base_color_texture_asset_path","material_name","unreal_folder","normal_texture_asset_path","roughness_texture_asset_path","ambient_occlusion_texture_asset_path","metallic_texture_asset_path","base_material_path"]}
PARAM_FIELDS=copy.deepcopy(PUBLIC_FIELDS)
DEFAULTS={"get_static_mesh_uv_layout":{"uv_channel":0,"output_path":""},"create_material_instance_with_texture":{"base_material_path":""},"create_material_with_texture_set":{"normal_texture_asset_path":"","roughness_texture_asset_path":"","ambient_occlusion_texture_asset_path":"","metallic_texture_asset_path":"","base_material_path":""}}
REQUIRED={"get_static_mesh_uv_layout":["mesh_path"],"create_material_instance_with_texture":["texture_asset_path","material_name","unreal_folder"],"create_material_with_texture_set":["base_color_texture_asset_path","material_name","unreal_folder"]}
FIELD_TYPES={"get_static_mesh_uv_layout":{"mesh_path":"string","uv_channel":"integer","output_path":"string"},"create_material_instance_with_texture":{"texture_asset_path":"string","material_name":"string","unreal_folder":"string","base_material_path":"string"},"create_material_with_texture_set":{"base_color_texture_asset_path":"string","material_name":"string","unreal_folder":"string","normal_texture_asset_path":"string","roughness_texture_asset_path":"string","ambient_occlusion_texture_asset_path":"string","metallic_texture_asset_path":"string","base_material_path":"string"}}

# RP original local-file import/material/apply composite; original three tables remain intact.
METHODS.append('import_create_and_apply_texture_set')
PUBLIC_FIELDS['import_create_and_apply_texture_set']=['base_color_image_path', 'output_name', 'unreal_folder', 'normal_image_path', 'roughness_image_path', 'ambient_occlusion_image_path', 'metallic_image_path', 'target', 'material_slot', 'base_material_path']
PARAM_FIELDS['import_create_and_apply_texture_set']=list(PUBLIC_FIELDS['import_create_and_apply_texture_set'])
DEFAULTS['import_create_and_apply_texture_set']={'unreal_folder': '/Game/AI_Generated', 'normal_image_path': '', 'roughness_image_path': '', 'ambient_occlusion_image_path': '', 'metallic_image_path': '', 'target': '', 'material_slot': 0, 'base_material_path': ''}
REQUIRED['import_create_and_apply_texture_set']=['base_color_image_path', 'output_name']
FIELD_TYPES['import_create_and_apply_texture_set']={field:("integer" if field=="material_slot" else "string") for field in PUBLIC_FIELDS['import_create_and_apply_texture_set']}
# RQ original8 local PBR file generator; no Editor or import/apply stage.
METHODS.append('generate_pbr_companion_textures')
PUBLIC_FIELDS['generate_pbr_companion_textures']=['base_color_image_path', 'output_name', 'output_dir', 'normal_strength', 'roughness', 'roughness_detail', 'ambient_occlusion_strength', 'metallic']
PARAM_FIELDS['generate_pbr_companion_textures']=list(PUBLIC_FIELDS['generate_pbr_companion_textures'])
DEFAULTS['generate_pbr_companion_textures']={'output_name': '', 'output_dir': '', 'normal_strength': 2.0, 'roughness': 0.75, 'roughness_detail': 0.15, 'ambient_occlusion_strength': 0.35, 'metallic': 0.0}
REQUIRED['generate_pbr_companion_textures']=['base_color_image_path']
FIELD_TYPES['generate_pbr_companion_textures']={field:('string' if field in ('base_color_image_path','output_name','output_dir') else 'number') for field in PUBLIC_FIELDS['generate_pbr_companion_textures']}

# RS original public import3/importset7/apply3 over the existing fixed Native leaves.
METHODS.append('import_texture_to_unreal')
PUBLIC_FIELDS['import_texture_to_unreal']=['image_path', 'unreal_folder', 'asset_name']
PARAM_FIELDS['import_texture_to_unreal']=list(PUBLIC_FIELDS['import_texture_to_unreal'])
DEFAULTS['import_texture_to_unreal']={}
REQUIRED['import_texture_to_unreal']=['image_path', 'unreal_folder', 'asset_name']
FIELD_TYPES['import_texture_to_unreal']={field:("integer" if field=="material_slot" else "string") for field in PUBLIC_FIELDS['import_texture_to_unreal']}
METHODS.append('import_texture_set_to_unreal')
PUBLIC_FIELDS['import_texture_set_to_unreal']=['base_color_image_path', 'unreal_folder', 'asset_name', 'normal_image_path', 'roughness_image_path', 'ambient_occlusion_image_path', 'metallic_image_path']
PARAM_FIELDS['import_texture_set_to_unreal']=list(PUBLIC_FIELDS['import_texture_set_to_unreal'])
DEFAULTS['import_texture_set_to_unreal']={'normal_image_path': '', 'roughness_image_path': '', 'ambient_occlusion_image_path': '', 'metallic_image_path': ''}
REQUIRED['import_texture_set_to_unreal']=['base_color_image_path', 'unreal_folder', 'asset_name']
FIELD_TYPES['import_texture_set_to_unreal']={field:("integer" if field=="material_slot" else "string") for field in PUBLIC_FIELDS['import_texture_set_to_unreal']}
METHODS.append('apply_material_to_mesh')
PUBLIC_FIELDS['apply_material_to_mesh']=['target', 'material_asset_path', 'material_slot']
PARAM_FIELDS['apply_material_to_mesh']=list(PUBLIC_FIELDS['apply_material_to_mesh'])
DEFAULTS['apply_material_to_mesh']={'material_slot': 0}
REQUIRED['apply_material_to_mesh']=['target', 'material_asset_path']
FIELD_TYPES['apply_material_to_mesh']={field:("integer" if field=="material_slot" else "string") for field in PUBLIC_FIELDS['apply_material_to_mesh']}

# SY original disabled image handoff public4/6/6; no backend request.
METHODS.append('generate_texture_from_prompt')
PUBLIC_FIELDS['generate_texture_from_prompt']=['prompt', 'output_name', 'output_dir', 'size']
PARAM_FIELDS['generate_texture_from_prompt']=list(PUBLIC_FIELDS['generate_texture_from_prompt'])
DEFAULTS['generate_texture_from_prompt']={'size': '1024x1024'}
REQUIRED['generate_texture_from_prompt']=['prompt', 'output_name', 'output_dir']
FIELD_TYPES['generate_texture_from_prompt']={field:("integer" if field=="uv_channel" else "string") for field in PUBLIC_FIELDS['generate_texture_from_prompt']}
METHODS.append('generate_texture_for_mesh_uv')
PUBLIC_FIELDS['generate_texture_for_mesh_uv']=['mesh_path', 'prompt', 'uv_channel', 'output_name', 'output_dir', 'size']
PARAM_FIELDS['generate_texture_for_mesh_uv']=list(PUBLIC_FIELDS['generate_texture_for_mesh_uv'])
DEFAULTS['generate_texture_for_mesh_uv']={'uv_channel': 0, 'output_name': 'T_AI_Texture', 'output_dir': '', 'size': '1024x1024'}
REQUIRED['generate_texture_for_mesh_uv']=['mesh_path', 'prompt']
FIELD_TYPES['generate_texture_for_mesh_uv']={field:("integer" if field=="uv_channel" else "string") for field in PUBLIC_FIELDS['generate_texture_for_mesh_uv']}
METHODS.append('generate_and_apply_ai_texture')
PUBLIC_FIELDS['generate_and_apply_ai_texture']=['target', 'prompt', 'output_name', 'unreal_folder', 'uv_channel', 'size']
PARAM_FIELDS['generate_and_apply_ai_texture']=list(PUBLIC_FIELDS['generate_and_apply_ai_texture'])
DEFAULTS['generate_and_apply_ai_texture']={'unreal_folder': '/Game/AI_Generated', 'uv_channel': 0, 'size': '1024x1024'}
REQUIRED['generate_and_apply_ai_texture']=['target', 'prompt', 'output_name']
FIELD_TYPES['generate_and_apply_ai_texture']={field:("integer" if field=="uv_channel" else "string") for field in PUBLIC_FIELDS['generate_and_apply_ai_texture']}
NULLABLE={name:[] for name in METHODS}
ITEMS={name:{} for name in METHODS}
OBSERVERS=[]
NO_JOURNAL_METHODS=[]
WRITERS=list(METHODS)  # UV writes a local PNG, so it also has a real current-request journal.
_ACTIVE_REQUEST=False
_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp","/unrealmcp","unrealmcp","mcpunreal","mcp_unreal","/script/cubelesscontrolrigtools","cubelesscontrolrigtools","cubelesscontrolrigtoolseditor","cubelessmcp","cubeless_mcp","modelcontextprotocol","toolsetregistry","/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    text=value.strip().strip('\"\'').replace('\\','/').lower()
    return any(token in text for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('texture_material_authoring_dependency_reference','Refusing MCP-only reference in '+field)
            error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    # output_path is a local file destination, never an Engine runtime reference.
    visit('params',{key:value for key,value in params.items() if key not in ('output_path','output_dir','image_path','prompt') and not key.endswith('_image_path')})


def _public_error(field,kind,message):
    error=OperationError('texture_material_authoring_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('texture_material_authoring_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('texture_material_authoring_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('texture_material_authoring_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values



def binding_status():return api.binding_status()


def _arguments(method,arguments,legacy,public_input):
    if method not in METHODS or type(arguments) is not dict or type(public_input) is not bool:raise OperationError('texture_material_authoring_entry_invalid','Known method/argument object/model selector required')
    original=copy.deepcopy(arguments)
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('texture_material_authoring_entry_invalid','Named model and Direct lower-name Batch alias are separate')
        arguments=legacy
    values=normalize_public_mcp_arguments(method,arguments) if public_input else dict(copy.deepcopy(DEFAULTS[method]),**{key:copy.deepcopy(arguments[key]) for key in PUBLIC_FIELDS[method] if key in arguments})
    try:json.dumps(values,ensure_ascii=False,allow_nan=False).encode('utf-16-le')
    except (ValueError,TypeError,OverflowError,UnicodeError,RecursionError) as error:raise OperationError('texture_material_authoring_arguments_invalid','Finite valid Unicode public JSON data required') from error
    return values,original


def _adopt(result,receipt):
    # One canonical current Python receipt, no Native/raw/RPC aliases.
    data=result['data'];known=receipt.get('change_state_known') is True
    data['current_Python_receipt']={key:copy.deepcopy(value) for key,value in receipt.items() if key!='legacy_result'}
    if type(receipt.get('legacy_result')) is dict:data['legacy_result']=copy.deepcopy(receipt['legacy_result'])
    attempted=receipt.get('mutation_attempted') is True;changed=receipt.get('observed_changed') is True
    data.update(execution_mutation_attempted=attempted,change_state_known=known,mutation_state_unknown=not known,
        mutation_performed=changed if known else None,SDK_global_purity_unverified=receipt.get('SDK_global_purity_unverified') is True)
    result['compile'].update(requested=receipt.get('compile_requested') is True,attempted=receipt.get('compile_attempted') is True,
        compiled=None if receipt.get('compile_attempted') else False,status='SDK_return_not_shader_or_GPU_completion' if receipt.get('compile_attempted') else 'not_requested')
    result['save'].update(requested=receipt.get('save_requested') is True,attempted=receipt.get('save_attempted') is True,
        saved=result['save'].get('saved') is True or receipt.get('saved') is True,
        status='API_true_physical_file_write_not_proved' if receipt.get('saved') else 'attempted_without_API_true' if receipt.get('save_attempted') else 'not_attempted')
    result['changes']=[] if known and not attempted and not changed else [dict(kind='texture_material_operation',attempted=attempted,
        changed=changed if known else None,change_state_known=known,save_attempted=receipt.get('save_attempted') is True,saved_API_true=receipt.get('saved') is True)]
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_scope_no_replay' if not known else 'inspect_created_asset_or_PNG_before_new_call' if attempted else 'none_observed')
    result['stage']=receipt.get('stage','preflight')


def _run(result,method,values,original,legacy,public_input,callback):
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:raise OperationError('texture_material_authoring_reentry','Same-domain reentry is refused before Editor work')
    _ACTIVE_REQUEST=True
    try:
        result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),lower_name_Batch_arguments=copy.deepcopy(legacy),public_argument_validation_applied=public_input,
            original_Native_command=False,hidden_fields=0,Native_leaf_required=method in ('import_create_and_apply_texture_set','import_texture_to_unreal','import_texture_set_to_unreal','apply_material_to_mesh'),registration_dependency='ToolsetAPI',operation_dependency='local_Pillow_NumPy_float32_files' if method=='generate_pbr_companion_textures' else 'public_Editor_Python_SDK',server_RPC_dependency=False,
            private_import_apply_leaf_required=method in ('import_create_and_apply_texture_set','import_texture_to_unreal','import_texture_set_to_unreal','apply_material_to_mesh'),
            automatic_action_retry=False,atomic_rollback=False,full_original_pipeline_complete=False,actual_named_wire_verified=False,
            change_state_scope='current captured object identity, authored material/parameter readback or bounded UV/PNG bytes and actual SDK save returns; not whole UObject/disk/global equality')
        receipt={}
        def checkpoint(current):
            _adopt(result,current)
            if callback:callback(result)
        try:api.execute(method,values,receipt,checkpoint)
        finally:_adopt(result,receipt)
        # Actual savedTrue/attempts/partial state have already been adopted.
        result['stage']='returned_texture_material_authoring_operation'
        if callback:callback(result)
        if receipt.get('error_code'):raise OperationError(receipt['error_code'],receipt.get('error','Public Engine Python utility failed'))
        leaf=result['data'].get('legacy_result')
        if type(leaf) is dict and leaf.get('success') is False:
            raise OperationError(leaf.get('error_code') or 'texture_material_authoring_operation_failed',leaf.get('message') or leaf.get('error') or 'Original texture material utility reported failure')
        if type(leaf) is not dict or leaf.get('success') is not True:raise OperationError('texture_material_authoring_legacy_invalid','Original public utility success bool/result required')
        if not receipt.get('change_state_known'):raise OperationError('texture_material_authoring_outcome_unknown','Actual partial target scope is unknown; no automatic replay')
        result['validation'].update(status='public_Python_current_request_scope',physical_file_write='asset_API_true_only; PNG_current_file_bytes_checked',global_GC_exit_save_atomicity='not_proved',all_property_or_semantic_rollback='not_available',disk_reload='not_run')
        if method=='generate_pbr_companion_textures':
            result['validation'].update(status='local_PBR_file_generation_scope',physical_file_write='returned PNG saves and current per-file hash/dimension/mode checks',Engine_asset_compile_save='not_requested',shader_or_GPU_completion='not_applicable')
        result.update(success=True,stage='complete');return result
    finally:_ACTIVE_REQUEST=False


def _rejected(method,error):
    result=state.new_result();state.fail(result,error)
    result['stage']='texture_material_authoring_input_validation'
    result['compile'].update(requested=False,attempted=False,compiled=False)
    result['save'].update(requested=False,attempted=False,saved=False)
    result['data'].update(public_method=method,Native_leaf_required=False,SDK_dispatched=False,execution_mutation_attempted=False,
        change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False,
        legacy_result=dict(success=False,error=str(error),error_code=getattr(error,'code','texture_material_authoring_failed')))
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
    if method in ('import_create_and_apply_texture_set','import_texture_to_unreal','import_texture_set_to_unreal'):
        error=OperationError('python_import_requires_standalone_ticker','The original declared legacy import requires a standalone next-tick Named call; Batch/reentrant import is refused before any Engine work')
        rejected=_rejected(method,error);rejected['operation_id']=result['operation_id']
        result.update(rejected);result['data'].update(parent_journal_only=True,Native_leaf_required=True,import_attempted=False)
        if callback:callback(result)
        raise error
    return _run(result,method,values,original,legacy,public_input,callback)


def get_static_mesh_uv_layout(mesh_path,uv_channel=0,output_path=""):
    return _named("get_static_mesh_uv_layout",dict(mesh_path=mesh_path,uv_channel=uv_channel,output_path=output_path))


def get_static_mesh_uv_layout_action(result,mesh_path,uv_channel=0,output_path="",checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"get_static_mesh_uv_layout",dict(mesh_path=mesh_path,uv_channel=uv_channel,output_path=output_path),checkpoint_callback,_legacy_command_params,_public_input)


def create_material_instance_with_texture(texture_asset_path,material_name,unreal_folder,base_material_path=""):
    return _named("create_material_instance_with_texture",dict(texture_asset_path=texture_asset_path,material_name=material_name,unreal_folder=unreal_folder,base_material_path=base_material_path))


def create_material_instance_with_texture_action(result,texture_asset_path,material_name,unreal_folder,base_material_path="",checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_material_instance_with_texture",dict(texture_asset_path=texture_asset_path,material_name=material_name,unreal_folder=unreal_folder,base_material_path=base_material_path),checkpoint_callback,_legacy_command_params,_public_input)


def create_material_with_texture_set(base_color_texture_asset_path,material_name,unreal_folder,normal_texture_asset_path="",roughness_texture_asset_path="",ambient_occlusion_texture_asset_path="",metallic_texture_asset_path="",base_material_path=""):
    return _named("create_material_with_texture_set",dict(base_color_texture_asset_path=base_color_texture_asset_path,material_name=material_name,unreal_folder=unreal_folder,normal_texture_asset_path=normal_texture_asset_path,roughness_texture_asset_path=roughness_texture_asset_path,ambient_occlusion_texture_asset_path=ambient_occlusion_texture_asset_path,metallic_texture_asset_path=metallic_texture_asset_path,base_material_path=base_material_path))


def create_material_with_texture_set_action(result,base_color_texture_asset_path,material_name,unreal_folder,normal_texture_asset_path="",roughness_texture_asset_path="",ambient_occlusion_texture_asset_path="",metallic_texture_asset_path="",base_material_path="",checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_material_with_texture_set",dict(base_color_texture_asset_path=base_color_texture_asset_path,material_name=material_name,unreal_folder=unreal_folder,normal_texture_asset_path=normal_texture_asset_path,roughness_texture_asset_path=roughness_texture_asset_path,ambient_occlusion_texture_asset_path=ambient_occlusion_texture_asset_path,metallic_texture_asset_path=metallic_texture_asset_path,base_material_path=base_material_path),checkpoint_callback,_legacy_command_params,_public_input)


def import_create_and_apply_texture_set(base_color_image_path,output_name,unreal_folder='/Game/AI_Generated',normal_image_path='',roughness_image_path='',ambient_occlusion_image_path='',metallic_image_path='',target='',material_slot=0,base_material_path=''):
    return _named('import_create_and_apply_texture_set',dict(base_color_image_path=base_color_image_path,output_name=output_name,unreal_folder=unreal_folder,normal_image_path=normal_image_path,roughness_image_path=roughness_image_path,ambient_occlusion_image_path=ambient_occlusion_image_path,metallic_image_path=metallic_image_path,target=target,material_slot=material_slot,base_material_path=base_material_path))


def import_create_and_apply_texture_set_action(result,base_color_image_path,output_name,unreal_folder='/Game/AI_Generated',normal_image_path='',roughness_image_path='',ambient_occlusion_image_path='',metallic_image_path='',target='',material_slot=0,base_material_path='',checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'import_create_and_apply_texture_set',dict(base_color_image_path=base_color_image_path,output_name=output_name,unreal_folder=unreal_folder,normal_image_path=normal_image_path,roughness_image_path=roughness_image_path,ambient_occlusion_image_path=ambient_occlusion_image_path,metallic_image_path=metallic_image_path,target=target,material_slot=material_slot,base_material_path=base_material_path),checkpoint_callback,_legacy_command_params,_public_input)


def generate_pbr_companion_textures(base_color_image_path,output_name='',output_dir='',normal_strength=2.0,roughness=0.75,roughness_detail=0.15,ambient_occlusion_strength=0.35,metallic=0.0):
    return _named('generate_pbr_companion_textures',dict(base_color_image_path=base_color_image_path,output_name=output_name,output_dir=output_dir,normal_strength=normal_strength,roughness=roughness,roughness_detail=roughness_detail,ambient_occlusion_strength=ambient_occlusion_strength,metallic=metallic))


def generate_pbr_companion_textures_action(result,base_color_image_path,output_name='',output_dir='',normal_strength=2.0,roughness=0.75,roughness_detail=0.15,ambient_occlusion_strength=0.35,metallic=0.0,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'generate_pbr_companion_textures',dict(base_color_image_path=base_color_image_path,output_name=output_name,output_dir=output_dir,normal_strength=normal_strength,roughness=roughness,roughness_detail=roughness_detail,ambient_occlusion_strength=ambient_occlusion_strength,metallic=metallic),checkpoint_callback,_legacy_command_params,_public_input)


def import_texture_to_unreal(image_path,unreal_folder,asset_name):
    return _named('import_texture_to_unreal',dict(image_path=image_path,unreal_folder=unreal_folder,asset_name=asset_name))


def import_texture_to_unreal_action(result,image_path,unreal_folder,asset_name,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'import_texture_to_unreal',dict(image_path=image_path,unreal_folder=unreal_folder,asset_name=asset_name),checkpoint_callback,_legacy_command_params,_public_input)


def import_texture_set_to_unreal(base_color_image_path,unreal_folder,asset_name,normal_image_path='',roughness_image_path='',ambient_occlusion_image_path='',metallic_image_path=''):
    return _named('import_texture_set_to_unreal',dict(base_color_image_path=base_color_image_path,unreal_folder=unreal_folder,asset_name=asset_name,normal_image_path=normal_image_path,roughness_image_path=roughness_image_path,ambient_occlusion_image_path=ambient_occlusion_image_path,metallic_image_path=metallic_image_path))


def import_texture_set_to_unreal_action(result,base_color_image_path,unreal_folder,asset_name,normal_image_path='',roughness_image_path='',ambient_occlusion_image_path='',metallic_image_path='',checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'import_texture_set_to_unreal',dict(base_color_image_path=base_color_image_path,unreal_folder=unreal_folder,asset_name=asset_name,normal_image_path=normal_image_path,roughness_image_path=roughness_image_path,ambient_occlusion_image_path=ambient_occlusion_image_path,metallic_image_path=metallic_image_path),checkpoint_callback,_legacy_command_params,_public_input)


def apply_material_to_mesh(target,material_asset_path,material_slot=0):
    return _named('apply_material_to_mesh',dict(target=target,material_asset_path=material_asset_path,material_slot=material_slot))


def apply_material_to_mesh_action(result,target,material_asset_path,material_slot=0,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'apply_material_to_mesh',dict(target=target,material_asset_path=material_asset_path,material_slot=material_slot),checkpoint_callback,_legacy_command_params,_public_input)


def generate_texture_from_prompt(prompt,output_name,output_dir,size='1024x1024'):
    return _named('generate_texture_from_prompt',dict(prompt=prompt,output_name=output_name,output_dir=output_dir,size=size))


def generate_texture_from_prompt_action(result,prompt,output_name,output_dir,size='1024x1024',checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'generate_texture_from_prompt',dict(prompt=prompt,output_name=output_name,output_dir=output_dir,size=size),checkpoint_callback,_legacy_command_params,_public_input)


def generate_texture_for_mesh_uv(mesh_path,prompt,uv_channel=0,output_name='T_AI_Texture',output_dir='',size='1024x1024'):
    return _named('generate_texture_for_mesh_uv',dict(mesh_path=mesh_path,prompt=prompt,uv_channel=uv_channel,output_name=output_name,output_dir=output_dir,size=size))


def generate_texture_for_mesh_uv_action(result,mesh_path,prompt,uv_channel=0,output_name='T_AI_Texture',output_dir='',size='1024x1024',checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'generate_texture_for_mesh_uv',dict(mesh_path=mesh_path,prompt=prompt,uv_channel=uv_channel,output_name=output_name,output_dir=output_dir,size=size),checkpoint_callback,_legacy_command_params,_public_input)


def generate_and_apply_ai_texture(target,prompt,output_name,unreal_folder='/Game/AI_Generated',uv_channel=0,size='1024x1024'):
    return _named('generate_and_apply_ai_texture',dict(target=target,prompt=prompt,output_name=output_name,unreal_folder=unreal_folder,uv_channel=uv_channel,size=size))


def generate_and_apply_ai_texture_action(result,target,prompt,output_name,unreal_folder='/Game/AI_Generated',uv_channel=0,size='1024x1024',checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'generate_and_apply_ai_texture',dict(target=target,prompt=prompt,output_name=output_name,unreal_folder=unreal_folder,uv_channel=uv_channel,size=size),checkpoint_callback,_legacy_command_params,_public_input)
