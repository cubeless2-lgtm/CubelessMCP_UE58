"""Viewport exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_parameter_collections as api
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_parameter_collections_toolset.CubelessNiagaraParameterCollectionTools'
BINDINGS = ('inspect_niagara_parameter_collection_values', 'set_niagara_parameter_collection_values', 'repair_niagara_parameter_collection', 'remap_niagara_parameter_collection_references', 'remap_niagara_script_parameter_collection_references', 'recompile_niagara_scripts_in_root')
PUBLIC_FIELDS_BY_METHOD = {'inspect_niagara_parameter_collection_values': ('collection_path', 'parameter_names', 'namespace', 'max_parameters'), 'set_niagara_parameter_collection_values': ('collection_path', 'values', 'namespace', 'allow_source_edit', 'save'), 'repair_niagara_parameter_collection': ('collection_path', 'allow_source_edit', 'save'), 'remap_niagara_parameter_collection_references': ('system_path', 'new_collection_path', 'old_collection_path', 'old_namespace', 'allow_source_edit', 'save', 'request_compile', 'replace_object_references'), 'remap_niagara_script_parameter_collection_references': ('new_collection_path', 'old_collection_path', 'old_namespace', 'script_path', 'script_paths', 'root_path', 'allow_source_edit', 'save', 'request_compile', 'wait_for_completion'), 'recompile_niagara_scripts_in_root': ('root_path', 'script_paths', 'allow_source_compile', 'force', 'save')}
LOGICAL_INPUT_SCHEMAS = {'inspect_niagara_parameter_collection_values': {'properties': {'collection_path': {'title': 'Collection Path', 'type': 'string'}, 'parameter_names': {'anyOf': [{'items': {'type': 'string'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Parameter Names'}, 'namespace': {'default': '', 'title': 'Namespace', 'type': 'string'}, 'max_parameters': {'default': 256, 'title': 'Max Parameters', 'type': 'integer'}}, 'required': ['collection_path'], 'title': 'inspect_niagara_parameter_collection_valuesArguments', 'type': 'object'}, 'set_niagara_parameter_collection_values': {'properties': {'collection_path': {'title': 'Collection Path', 'type': 'string'}, 'values': {'title': 'Values', 'type': 'object'}, 'namespace': {'default': '', 'title': 'Namespace', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}}, 'required': ['collection_path', 'values'], 'title': 'set_niagara_parameter_collection_valuesArguments', 'type': 'object'}, 'repair_niagara_parameter_collection': {'properties': {'collection_path': {'title': 'Collection Path', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}}, 'required': ['collection_path'], 'title': 'repair_niagara_parameter_collectionArguments', 'type': 'object'}, 'remap_niagara_parameter_collection_references': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'new_collection_path': {'title': 'New Collection Path', 'type': 'string'}, 'old_collection_path': {'default': '', 'title': 'Old Collection Path', 'type': 'string'}, 'old_namespace': {'default': '', 'title': 'Old Namespace', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}, 'replace_object_references': {'default': False, 'title': 'Replace Object References', 'type': 'boolean'}}, 'required': ['system_path', 'new_collection_path'], 'title': 'remap_niagara_parameter_collection_referencesArguments', 'type': 'object'}, 'remap_niagara_script_parameter_collection_references': {'properties': {'new_collection_path': {'title': 'New Collection Path', 'type': 'string'}, 'old_collection_path': {'default': '', 'title': 'Old Collection Path', 'type': 'string'}, 'old_namespace': {'default': '', 'title': 'Old Namespace', 'type': 'string'}, 'script_path': {'default': '', 'title': 'Script Path', 'type': 'string'}, 'script_paths': {'anyOf': [{'items': {}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Script Paths'}, 'root_path': {'default': '', 'title': 'Root Path', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}, 'wait_for_completion': {'default': True, 'title': 'Wait For Completion', 'type': 'boolean'}}, 'required': ['new_collection_path'], 'title': 'remap_niagara_script_parameter_collection_referencesArguments', 'type': 'object'}, 'recompile_niagara_scripts_in_root': {'properties': {'root_path': {'default': '', 'title': 'Root Path', 'type': 'string'}, 'script_paths': {'anyOf': [{'items': {'type': 'string'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Script Paths'}, 'allow_source_compile': {'default': False, 'title': 'Allow Source Compile', 'type': 'boolean'}, 'force': {'default': True, 'title': 'Force', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}}, 'title': 'recompile_niagara_scripts_in_rootArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'inspect_niagara_parameter_collection_values': {'parameter_names': 'null', 'namespace': '""', 'max_parameters': '256'}, 'set_niagara_parameter_collection_values': {'namespace': '""', 'allow_source_edit': 'false', 'save': 'true'}, 'repair_niagara_parameter_collection': {'allow_source_edit': 'false', 'save': 'true'}, 'remap_niagara_parameter_collection_references': {'old_collection_path': '""', 'old_namespace': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true', 'replace_object_references': 'false'}, 'remap_niagara_script_parameter_collection_references': {'old_collection_path': '""', 'old_namespace': '""', 'script_path': '""', 'script_paths': 'null', 'root_path': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true', 'wait_for_completion': 'true'}, 'recompile_niagara_scripts_in_root': {'root_path': '""', 'script_paths': 'null', 'allow_source_compile': 'false', 'force': 'true', 'save': 'true'}}
REQUIRED_BY_METHOD = {'inspect_niagara_parameter_collection_values': ('collection_path',), 'set_niagara_parameter_collection_values': ('collection_path', 'values'), 'repair_niagara_parameter_collection': ('collection_path',), 'remap_niagara_parameter_collection_references': ('system_path', 'new_collection_path'), 'remap_niagara_script_parameter_collection_references': ('new_collection_path',), 'recompile_niagara_scripts_in_root': ()}
ATTACHMENT_LIBRARY = 'CubelessNiagaraParameterCollectionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_parameter_collections_toolset', 'verify_niagara_parameter_collections_toolset', 'remove_niagara_parameter_collections_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'niagara_collection_public4_5_3_8_10_5_schema_verified', 'required1_2_1_2_1_0_default3_3_2_6_9_5_verified', 'json_value_carrier35_schema_verified', 'typed_and_Any_lists_nested_values_preserved', 'body_omission_and_raw_presence_preserved', 'hidden0_and_original_model_extra_drop_verified', 'niagara_collection6_native_contract_owned')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=6,
        public_input_counts=dict(zip(BINDINGS,(4,5,3,8,10,5))),physical_input_counts=dict(zip(BINDINGS,(4,5,3,8,10,5))),native_input_counts=dict(zip(BINDINGS,(1,1,1,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0,0,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=6:raise RuntimeError('Exact NiagaraParameterCollection Toolset6 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Niagara parameter collection methods required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(4,5,3,8,10,5))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraParameterCollection wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraParameterCollection wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
