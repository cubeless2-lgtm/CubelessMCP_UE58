"""HJ original public3/4/8 over public Editor Python SDK; business Native0 and exact registration guard."""
import json
import math
from cubeless_mcp_ue58.operations import texture_material_authoring as api
TOOLSET = 'cubeless_mcp_ue58.registration.texture_material_authoring_toolset.CubelessTextureMaterialTools'
BINDINGS=["get_static_mesh_uv_layout", "create_material_instance_with_texture", "create_material_with_texture_set"]
PUBLIC_FIELDS_BY_METHOD={"get_static_mesh_uv_layout": ["mesh_path", "uv_channel", "output_path"], "create_material_instance_with_texture": ["texture_asset_path", "material_name", "unreal_folder", "base_material_path"], "create_material_with_texture_set": ["base_color_texture_asset_path", "material_name", "unreal_folder", "normal_texture_asset_path", "roughness_texture_asset_path", "ambient_occlusion_texture_asset_path", "metallic_texture_asset_path", "base_material_path"]}
LOGICAL_INPUT_SCHEMAS={"get_static_mesh_uv_layout": {"properties": {"mesh_path": {"title": "Mesh Path", "type": "string"}, "uv_channel": {"default": 0, "title": "Uv Channel", "type": "integer"}, "output_path": {"default": "", "title": "Output Path", "type": "string"}}, "required": ["mesh_path"], "title": "get_static_mesh_uv_layoutArguments", "type": "object"}, "create_material_instance_with_texture": {"properties": {"texture_asset_path": {"title": "Texture Asset Path", "type": "string"}, "material_name": {"title": "Material Name", "type": "string"}, "unreal_folder": {"title": "Unreal Folder", "type": "string"}, "base_material_path": {"default": "", "title": "Base Material Path", "type": "string"}}, "required": ["texture_asset_path", "material_name", "unreal_folder"], "title": "create_material_instance_with_textureArguments", "type": "object"}, "create_material_with_texture_set": {"properties": {"base_color_texture_asset_path": {"title": "Base Color Texture Asset Path", "type": "string"}, "material_name": {"title": "Material Name", "type": "string"}, "unreal_folder": {"title": "Unreal Folder", "type": "string"}, "normal_texture_asset_path": {"default": "", "title": "Normal Texture Asset Path", "type": "string"}, "roughness_texture_asset_path": {"default": "", "title": "Roughness Texture Asset Path", "type": "string"}, "ambient_occlusion_texture_asset_path": {"default": "", "title": "Ambient Occlusion Texture Asset Path", "type": "string"}, "metallic_texture_asset_path": {"default": "", "title": "Metallic Texture Asset Path", "type": "string"}, "base_material_path": {"default": "", "title": "Base Material Path", "type": "string"}}, "required": ["base_color_texture_asset_path", "material_name", "unreal_folder"], "title": "create_material_with_texture_setArguments", "type": "object"}}
PHYSICAL_DEFAULTS_BY_METHOD={"get_static_mesh_uv_layout": {"uv_channel": "0", "output_path": "\"\""}, "create_material_instance_with_texture": {"base_material_path": "\"\""}, "create_material_with_texture_set": {"normal_texture_asset_path": "\"\"", "roughness_texture_asset_path": "\"\"", "ambient_occlusion_texture_asset_path": "\"\"", "metallic_texture_asset_path": "\"\"", "base_material_path": "\"\""}}
REQUIRED_BY_METHOD={"get_static_mesh_uv_layout": ["mesh_path"], "create_material_instance_with_texture": ["texture_asset_path", "material_name", "unreal_folder"], "create_material_with_texture_set": ["base_color_texture_asset_path", "material_name", "unreal_folder"]}

BINDINGS.append('import_create_and_apply_texture_set')
PUBLIC_FIELDS_BY_METHOD['import_create_and_apply_texture_set']=['base_color_image_path', 'output_name', 'unreal_folder', 'normal_image_path', 'roughness_image_path', 'ambient_occlusion_image_path', 'metallic_image_path', 'target', 'material_slot', 'base_material_path']
LOGICAL_INPUT_SCHEMAS['import_create_and_apply_texture_set']={'properties': {'base_color_image_path': {'title': 'Base Color Image Path', 'type': 'string'}, 'output_name': {'title': 'Output Name', 'type': 'string'}, 'unreal_folder': {'title': 'Unreal Folder', 'type': 'string', 'default': '/Game/AI_Generated'}, 'normal_image_path': {'title': 'Normal Image Path', 'type': 'string', 'default': ''}, 'roughness_image_path': {'title': 'Roughness Image Path', 'type': 'string', 'default': ''}, 'ambient_occlusion_image_path': {'title': 'Ambient Occlusion Image Path', 'type': 'string', 'default': ''}, 'metallic_image_path': {'title': 'Metallic Image Path', 'type': 'string', 'default': ''}, 'target': {'title': 'Target', 'type': 'string', 'default': ''}, 'material_slot': {'title': 'Material Slot', 'type': 'integer', 'default': 0}, 'base_material_path': {'title': 'Base Material Path', 'type': 'string', 'default': ''}}, 'required': ['base_color_image_path', 'output_name'], 'title': 'import_create_and_apply_texture_setArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['import_create_and_apply_texture_set']={'unreal_folder': '"/Game/AI_Generated"', 'normal_image_path': '""', 'roughness_image_path': '""', 'ambient_occlusion_image_path': '""', 'metallic_image_path': '""', 'target': '""', 'material_slot': '0', 'base_material_path': '""'}
REQUIRED_BY_METHOD['import_create_and_apply_texture_set']=['base_color_image_path', 'output_name']

BINDINGS.append('generate_pbr_companion_textures')
PUBLIC_FIELDS_BY_METHOD['generate_pbr_companion_textures']=['base_color_image_path', 'output_name', 'output_dir', 'normal_strength', 'roughness', 'roughness_detail', 'ambient_occlusion_strength', 'metallic']
LOGICAL_INPUT_SCHEMAS['generate_pbr_companion_textures']={'properties': {'base_color_image_path': {'title': 'Base Color Image Path', 'type': 'string'}, 'output_name': {'title': 'Output Name', 'type': 'string', 'default': ''}, 'output_dir': {'title': 'Output Dir', 'type': 'string', 'default': ''}, 'normal_strength': {'title': 'Normal Strength', 'type': 'number', 'default': 2.0}, 'roughness': {'title': 'Roughness', 'type': 'number', 'default': 0.75}, 'roughness_detail': {'title': 'Roughness Detail', 'type': 'number', 'default': 0.15}, 'ambient_occlusion_strength': {'title': 'Ambient Occlusion Strength', 'type': 'number', 'default': 0.35}, 'metallic': {'title': 'Metallic', 'type': 'number', 'default': 0.0}}, 'required': ['base_color_image_path'], 'title': 'generate_pbr_companion_texturesArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['generate_pbr_companion_textures']={'output_name': '""', 'output_dir': '""', 'normal_strength': '2.0', 'roughness': '0.75', 'roughness_detail': '0.15', 'ambient_occlusion_strength': '0.35', 'metallic': '0.0'}
REQUIRED_BY_METHOD['generate_pbr_companion_textures']=['base_color_image_path']

BINDINGS.append('import_texture_to_unreal')
PUBLIC_FIELDS_BY_METHOD['import_texture_to_unreal']=['image_path', 'unreal_folder', 'asset_name']
LOGICAL_INPUT_SCHEMAS['import_texture_to_unreal']={'properties': {'image_path': {'title': 'Image Path', 'type': 'string'}, 'unreal_folder': {'title': 'Unreal Folder', 'type': 'string'}, 'asset_name': {'title': 'Asset Name', 'type': 'string'}}, 'required': ['image_path', 'unreal_folder', 'asset_name'], 'title': 'import_texture_to_unrealArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['import_texture_to_unreal']={}
REQUIRED_BY_METHOD['import_texture_to_unreal']=['image_path', 'unreal_folder', 'asset_name']

BINDINGS.append('import_texture_set_to_unreal')
PUBLIC_FIELDS_BY_METHOD['import_texture_set_to_unreal']=['base_color_image_path', 'unreal_folder', 'asset_name', 'normal_image_path', 'roughness_image_path', 'ambient_occlusion_image_path', 'metallic_image_path']
LOGICAL_INPUT_SCHEMAS['import_texture_set_to_unreal']={'properties': {'base_color_image_path': {'title': 'Base Color Image Path', 'type': 'string'}, 'unreal_folder': {'title': 'Unreal Folder', 'type': 'string'}, 'asset_name': {'title': 'Asset Name', 'type': 'string'}, 'normal_image_path': {'title': 'Normal Image Path', 'type': 'string', 'default': ''}, 'roughness_image_path': {'title': 'Roughness Image Path', 'type': 'string', 'default': ''}, 'ambient_occlusion_image_path': {'title': 'Ambient Occlusion Image Path', 'type': 'string', 'default': ''}, 'metallic_image_path': {'title': 'Metallic Image Path', 'type': 'string', 'default': ''}}, 'required': ['base_color_image_path', 'unreal_folder', 'asset_name'], 'title': 'import_texture_set_to_unrealArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['import_texture_set_to_unreal']={'normal_image_path': '""', 'roughness_image_path': '""', 'ambient_occlusion_image_path': '""', 'metallic_image_path': '""'}
REQUIRED_BY_METHOD['import_texture_set_to_unreal']=['base_color_image_path', 'unreal_folder', 'asset_name']

BINDINGS.append('apply_material_to_mesh')
PUBLIC_FIELDS_BY_METHOD['apply_material_to_mesh']=['target', 'material_asset_path', 'material_slot']
LOGICAL_INPUT_SCHEMAS['apply_material_to_mesh']={'properties': {'target': {'title': 'Target', 'type': 'string'}, 'material_asset_path': {'title': 'Material Asset Path', 'type': 'string'}, 'material_slot': {'title': 'Material Slot', 'type': 'integer', 'default': 0}}, 'required': ['target', 'material_asset_path'], 'title': 'apply_material_to_meshArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['apply_material_to_mesh']={'material_slot': '0'}
REQUIRED_BY_METHOD['apply_material_to_mesh']=['target', 'material_asset_path']


BINDINGS.append('generate_texture_from_prompt')
PUBLIC_FIELDS_BY_METHOD['generate_texture_from_prompt']=['prompt', 'output_name', 'output_dir', 'size']
LOGICAL_INPUT_SCHEMAS['generate_texture_from_prompt']={'properties': {'prompt': {'title': 'Prompt', 'type': 'string'}, 'output_name': {'title': 'Output Name', 'type': 'string'}, 'output_dir': {'title': 'Output Dir', 'type': 'string'}, 'size': {'default': '1024x1024', 'title': 'Size', 'type': 'string'}}, 'required': ['prompt', 'output_name', 'output_dir'], 'title': 'generate_texture_from_promptArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['generate_texture_from_prompt']={'size': '"1024x1024"'}
REQUIRED_BY_METHOD['generate_texture_from_prompt']=['prompt', 'output_name', 'output_dir']


BINDINGS.append('generate_texture_for_mesh_uv')
PUBLIC_FIELDS_BY_METHOD['generate_texture_for_mesh_uv']=['mesh_path', 'prompt', 'uv_channel', 'output_name', 'output_dir', 'size']
LOGICAL_INPUT_SCHEMAS['generate_texture_for_mesh_uv']={'properties': {'mesh_path': {'title': 'Mesh Path', 'type': 'string'}, 'prompt': {'title': 'Prompt', 'type': 'string'}, 'uv_channel': {'default': 0, 'title': 'Uv Channel', 'type': 'integer'}, 'output_name': {'default': 'T_AI_Texture', 'title': 'Output Name', 'type': 'string'}, 'output_dir': {'default': '', 'title': 'Output Dir', 'type': 'string'}, 'size': {'default': '1024x1024', 'title': 'Size', 'type': 'string'}}, 'required': ['mesh_path', 'prompt'], 'title': 'generate_texture_for_mesh_uvArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['generate_texture_for_mesh_uv']={'uv_channel': '0', 'output_name': '"T_AI_Texture"', 'output_dir': '""', 'size': '"1024x1024"'}
REQUIRED_BY_METHOD['generate_texture_for_mesh_uv']=['mesh_path', 'prompt']


BINDINGS.append('generate_and_apply_ai_texture')
PUBLIC_FIELDS_BY_METHOD['generate_and_apply_ai_texture']=['target', 'prompt', 'output_name', 'unreal_folder', 'uv_channel', 'size']
LOGICAL_INPUT_SCHEMAS['generate_and_apply_ai_texture']={'properties': {'target': {'title': 'Target', 'type': 'string'}, 'prompt': {'title': 'Prompt', 'type': 'string'}, 'output_name': {'title': 'Output Name', 'type': 'string'}, 'unreal_folder': {'default': '/Game/AI_Generated', 'title': 'Unreal Folder', 'type': 'string'}, 'uv_channel': {'default': 0, 'title': 'Uv Channel', 'type': 'integer'}, 'size': {'default': '1024x1024', 'title': 'Size', 'type': 'string'}}, 'required': ['target', 'prompt', 'output_name'], 'title': 'generate_and_apply_ai_textureArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['generate_and_apply_ai_texture']={'unreal_folder': '"/Game/AI_Generated"', 'uv_channel': '0', 'size': '"1024x1024"'}
REQUIRED_BY_METHOD['generate_and_apply_ai_texture']=['target', 'prompt', 'output_name']

ATTACHMENT_LIBRARY = 'CubelessTextureMaterialAttachmentLibrary'
ATTACHMENT_METHODS = ('install_texture_material_authoring_toolset', 'verify_texture_material_authoring_toolset', 'remove_texture_material_authoring_toolset')
ATTACHMENT_READY_FIELDS=["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "texture_material_public3_4_8_schema_verified", "required1_3_3_defaults2_1_5_verified", "json_value_carrier15_schema_verified", "original_named_defaults_and_direct_values_preserved", "public_Editor_Python_SDK3_no_Native_leaf", "uv_PNG_and_material_partial_save_no_replay_preserved", "texture_material3_Python_contract_owned", "texture_material_import_apply10_contract_owned", "texture_material_pbr_companion8_contract_owned", "texture_material_import3_importset7_apply3_contract_owned", "texture_material_image_handoff4_6_6_contract_owned"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=None,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=11,
        public_input_counts=dict(zip(BINDINGS,(3,4,8,10,8,3,7,3,4,6,6))),physical_input_counts=dict(zip(BINDINGS,(3,4,8,10,8,3,7,3,4,6,6))),native_input_counts=dict(zip(BINDINGS,(0,0,0,0,0,0,0,0,0,0,0))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0,0,0,0,0,0,0,0,0))),
        schema_verified=False,default_verified=False,raw_guard_required=True,raw_guard_ready=False,nullable_raw_guard_ready=False,boolean_raw_guard_ready=False,number_raw_guard_ready=False,
        actual_official_wire_verified=False,nullable_Named_wire_verified=False,boolean_Named_wire_verified=False,number_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _required(expected,actual):return type(actual) is list and len(actual)==len(expected) and all(type(v) is str for v in actual) and set(actual)==set(expected)

def _exact(a,b):
    # Unreal JSON serializes integral finite floats as integers; JSON has one number kind.
    if type(a) in (int,float) and type(b) in (int,float):
        return (type(a) is int or math.isfinite(a)) and (type(b) is int or math.isfinite(b)) and a==b
    if type(a) is not type(b):return False
    if type(a) is dict:return set(a)==set(b) and all(_exact(a[k],b[k]) for k in a)
    if type(a) is list:return len(a)==len(b) and all(_exact(x,y) for x,y in zip(a,b))
    return a==b

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=11:raise RuntimeError('Exact TextureMaterial Toolset11 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original texture/material contract method required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original method unavailable')
        inputs=row.get('inputSchema')
        if not reflected_carriers:
            if not _exact(inputs,LOGICAL_INPUT_SCHEMAS[method]):raise RuntimeError('Exact original eager input schema required; no new length/range/extra-field policy')
        else:
            if type(inputs) is not dict or inputs.get('type')!='object':raise RuntimeError('Physical object required')
            props=inputs.get('properties',{});required=inputs.get('required',[])
            if type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(REQUIRED_BY_METHOD[method],required):raise RuntimeError('Exact physical fields/required required')
            for field in PUBLIC_FIELDS_BY_METHOD[method]:
                item=props[field]
                if type(item) is not dict or item.get('type')!='string':raise RuntimeError('Physical JSON FString carrier required')
                if field in REQUIRED_BY_METHOD[method]:
                    if 'default' in item:raise RuntimeError('Required public field has physical default')
                elif type(item.get('default')) is not str or item['default']!=PHYSICAL_DEFAULTS_BY_METHOD[method][field]:raise RuntimeError('Exact physical JSON default required')
        output=row.get('outputSchema');outer=output.get('properties') if type(output) is dict else None
        if type(output) is not dict or output.get('type')!='object' or type(outer) is not dict or set(outer)!={'returnValue'} or not _required(('returnValue',),output.get('required')):raise RuntimeError('Exact returnValue outer required')
        result=outer['returnValue'];props=result.get('properties') if type(result) is dict else None
        if type(result) is not dict or result.get('type')!='object' or type(props) is not dict or set(props)!=set(RESULT_FIELDS) or not _required(RESULT_FIELDS,result.get('required')) or any(type(props[k]) is not dict or props[k].get('type')!=('boolean' if k=='success' else 'string') for k in RESULT_FIELDS):raise RuntimeError('Exact standard11 output required')
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(3,4,8,10,8,3,7,3,4,6,6))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

def validate_schema(schema,toolset_class):return _validate_schema(schema,toolset_class)

def validate_reflected_carrier_schema(schema,toolset_class):return _validate_schema(schema,toolset_class,True)

def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict or attachment_report.get('operation') not in ATTACHMENT_METHODS[:2]
            or not all(attachment_report.get(key) is True for key in ATTACHMENT_READY_FIELDS)
            or attachment_report.get('asset_mutation') is not False or attachment_report.get('metadata_mutation') is not False
            or attachment_report.get('actual_named_wire_verified') is not False
            or attachment_report.get('logical_schema_source') != 'actual_registered_wrapper_GetJsonSchema'
            or attachment_report.get('logical_schema_registry_identity_verified') is not True):
        raise RuntimeError('Exact ready owner receipt with actual registered TextureMaterial wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered TextureMaterial wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
