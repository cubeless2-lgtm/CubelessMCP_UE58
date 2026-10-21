"""HH required recipe object|string and nullable options: public1/1/2/2; Native recipe leaf3 plus direct Python composite, JSON carriers6."""
import json
import math
from cubeless_mcp_ue58.operations import controlrig_recipe_authoring as api
TOOLSET = 'cubeless_mcp_ue58.registration.controlrig_recipe_authoring_toolset.CubelessControlRigRecipeAuthoringTools'
BINDINGS=['validate_controlrig_recipe', 'dry_run_controlrig_recipe', 'build_controlrig_recipe', 'build_controlrig_recipe_with_animbp_binding']
PUBLIC_FIELDS_BY_METHOD={'validate_controlrig_recipe': ['recipe'], 'dry_run_controlrig_recipe': ['recipe'], 'build_controlrig_recipe': ['recipe', 'options'], 'build_controlrig_recipe_with_animbp_binding': ['recipe', 'options']}
LOGICAL_INPUT_SCHEMAS={'validate_controlrig_recipe': {'properties': {'recipe': {'anyOf': [{'type': 'object'}, {'type': 'string'}], 'title': 'Recipe'}}, 'required': ['recipe'], 'title': 'validate_controlrig_recipeArguments', 'type': 'object'}, 'dry_run_controlrig_recipe': {'properties': {'recipe': {'anyOf': [{'type': 'object'}, {'type': 'string'}], 'title': 'Recipe'}}, 'required': ['recipe'], 'title': 'dry_run_controlrig_recipeArguments', 'type': 'object'}, 'build_controlrig_recipe': {'properties': {'recipe': {'anyOf': [{'type': 'object'}, {'type': 'string'}], 'title': 'Recipe'}, 'options': {'anyOf': [{'type': 'object'}, {'type': 'string'}, {'type': 'null'}], 'default': None, 'title': 'Options'}}, 'required': ['recipe'], 'title': 'build_controlrig_recipeArguments', 'type': 'object'}, 'build_controlrig_recipe_with_animbp_binding': {'properties': {'recipe': {'anyOf': [{'type': 'object'}, {'type': 'string'}], 'title': 'Recipe'}, 'options': {'anyOf': [{'type': 'object'}, {'type': 'string'}, {'type': 'null'}], 'default': None, 'title': 'Options'}}, 'required': ['recipe'], 'title': 'build_controlrig_recipe_with_animbp_bindingArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD={'validate_controlrig_recipe': {}, 'dry_run_controlrig_recipe': {}, 'build_controlrig_recipe': {'options': 'null'}, 'build_controlrig_recipe_with_animbp_binding': {'options': 'null'}}
REQUIRED_BY_METHOD={'validate_controlrig_recipe': ['recipe'], 'dry_run_controlrig_recipe': ['recipe'], 'build_controlrig_recipe': ['recipe'], 'build_controlrig_recipe_with_animbp_binding': ['recipe']}
ATTACHMENT_LIBRARY = 'CubelessControlRigRecipeAuthoringAttachmentLibrary'
ATTACHMENT_METHODS = ('install_controlrig_recipe_authoring_toolset', 'verify_controlrig_recipe_authoring_toolset', 'remove_controlrig_recipe_authoring_toolset')
ATTACHMENT_READY_FIELDS=['ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'controlrig_recipe_authoring_public1_1_2_schema_verified', 'required1_1_1_default0_0_1_verified', 'json_union_value_carrier4_schema_verified', 'original_DictAny_or_string_nullable_options_preserved', 'pure_validation_and_guarded_Engine_build_preserved', 'partial_compile_save_and_no_replay_preserved', 'controlrig_recipe_authoring3_contract_owned', 'controlrig_recipe_animbp_composite2_schema_verified', 'direct_leaf_composite_not_Native_whole_command', 'controlrig_recipe_authoring4_contract_owned']
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library='CubelessControlRigRecipeAuthoringLibrary',methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=4,
        composite_direct_existing_leaf_actions=True,composite_Native_whole_command=False,public_input_counts=dict(zip(BINDINGS,(1,1,2,2))),physical_input_counts=dict(zip(BINDINGS,(1,1,2,2))),native_input_counts=dict(zip(BINDINGS,(1,1,1,0))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=4:raise RuntimeError('Exact ControlRigRecipeAuthoring Toolset4 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original ControlRig authoring method required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(1,1,2,2))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered ControlRigRecipeAuthoring wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered ControlRigRecipeAuthoring wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
