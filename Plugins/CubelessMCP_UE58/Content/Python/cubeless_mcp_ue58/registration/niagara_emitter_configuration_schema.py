"""Viewport exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_emitter_configuration as api
from cubeless_mcp_ue58.operations import niagara_emitter_configuration as domain
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_emitter_configuration_toolset.CubelessNiagaraEmitterConfigurationTools'
BINDINGS = ('inspect_niagara_emitter_properties', 'set_niagara_emitter_enabled', 'set_niagara_emitter_local_space', 'set_niagara_emitter_sim_target', 'set_niagara_emitter_fixed_bounds', 'set_niagara_emitter_persistent_ids', 'remove_niagara_emitter')
PUBLIC_FIELDS_BY_METHOD = {'inspect_niagara_emitter_properties': ('system_path', 'emitter_index', 'emitter_name'), 'set_niagara_emitter_enabled': ('system_path', 'enabled', 'emitter_index', 'emitter_name', 'allow_source_edit', 'save', 'request_compile'), 'set_niagara_emitter_local_space': ('system_path', 'value', 'emitter_index', 'emitter_name', 'allow_source_edit', 'save', 'request_compile'), 'set_niagara_emitter_sim_target': ('system_path', 'sim_target', 'auto_fixed_bounds', 'emitter_index', 'emitter_name', 'allow_source_edit', 'save', 'request_compile'), 'set_niagara_emitter_fixed_bounds': ('system_path', 'min', 'max', 'mode', 'emitter_index', 'emitter_name', 'allow_source_edit', 'save', 'request_compile'), 'set_niagara_emitter_persistent_ids': ('system_path', 'enabled', 'emitter_index', 'emitter_name', 'allow_source_edit', 'save', 'request_compile', 'wait_for_completion', 'timeout_seconds', 'poll_interval_seconds'), 'remove_niagara_emitter': ('system_path', 'emitter_index', 'emitter_name', 'allow_source_edit', 'save', 'request_compile')}
LOGICAL_INPUT_SCHEMAS = {'inspect_niagara_emitter_properties': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'emitter_index': {'anyOf': [{'type': 'integer'}, {'type': 'null'}], 'default': None, 'title': 'Emitter Index'}, 'emitter_name': {'default': '', 'title': 'Emitter Name', 'type': 'string'}}, 'required': ['system_path'], 'title': 'inspect_niagara_emitter_propertiesArguments', 'type': 'object'}, 'set_niagara_emitter_enabled': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'enabled': {'title': 'Enabled', 'type': 'boolean'}, 'emitter_index': {'anyOf': [{'type': 'integer'}, {'type': 'null'}], 'default': None, 'title': 'Emitter Index'}, 'emitter_name': {'default': '', 'title': 'Emitter Name', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}}, 'required': ['system_path', 'enabled'], 'title': 'set_niagara_emitter_enabledArguments', 'type': 'object'}, 'set_niagara_emitter_local_space': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'value': {'title': 'Value', 'type': 'boolean'}, 'emitter_index': {'anyOf': [{'type': 'integer'}, {'type': 'null'}], 'default': None, 'title': 'Emitter Index'}, 'emitter_name': {'default': '', 'title': 'Emitter Name', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}}, 'required': ['system_path', 'value'], 'title': 'set_niagara_emitter_local_spaceArguments', 'type': 'object'}, 'set_niagara_emitter_sim_target': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'sim_target': {'title': 'Sim Target', 'type': 'string'}, 'auto_fixed_bounds': {'default': True, 'title': 'Auto Fixed Bounds', 'type': 'boolean'}, 'emitter_index': {'anyOf': [{'type': 'integer'}, {'type': 'null'}], 'default': None, 'title': 'Emitter Index'}, 'emitter_name': {'default': '', 'title': 'Emitter Name', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}}, 'required': ['system_path', 'sim_target'], 'title': 'set_niagara_emitter_sim_targetArguments', 'type': 'object'}, 'set_niagara_emitter_fixed_bounds': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'min': {'anyOf': [{'items': {}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Min'}, 'max': {'anyOf': [{'items': {}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Max'}, 'mode': {'default': '', 'title': 'Mode', 'type': 'string'}, 'emitter_index': {'anyOf': [{'type': 'integer'}, {'type': 'null'}], 'default': None, 'title': 'Emitter Index'}, 'emitter_name': {'default': '', 'title': 'Emitter Name', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}}, 'required': ['system_path'], 'title': 'set_niagara_emitter_fixed_boundsArguments', 'type': 'object'}, 'set_niagara_emitter_persistent_ids': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'enabled': {'title': 'Enabled', 'type': 'boolean'}, 'emitter_index': {'anyOf': [{'type': 'integer'}, {'type': 'null'}], 'default': None, 'title': 'Emitter Index'}, 'emitter_name': {'default': '', 'title': 'Emitter Name', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}, 'wait_for_completion': {'default': True, 'title': 'Wait For Completion', 'type': 'boolean'}, 'timeout_seconds': {'default': 60, 'title': 'Timeout Seconds', 'type': 'number'}, 'poll_interval_seconds': {'default': 0.1, 'title': 'Poll Interval Seconds', 'type': 'number'}}, 'required': ['system_path', 'enabled'], 'title': 'set_niagara_emitter_persistent_idsArguments', 'type': 'object'}, 'remove_niagara_emitter': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'emitter_index': {'anyOf': [{'type': 'integer'}, {'type': 'null'}], 'default': None, 'title': 'Emitter Index'}, 'emitter_name': {'default': '', 'title': 'Emitter Name', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}}, 'required': ['system_path'], 'title': 'remove_niagara_emitterArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'inspect_niagara_emitter_properties': {'emitter_index': 'null', 'emitter_name': '""'}, 'set_niagara_emitter_enabled': {'emitter_index': 'null', 'emitter_name': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true'}, 'set_niagara_emitter_local_space': {'emitter_index': 'null', 'emitter_name': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true'}, 'set_niagara_emitter_sim_target': {'auto_fixed_bounds': 'true', 'emitter_index': 'null', 'emitter_name': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true'}, 'set_niagara_emitter_fixed_bounds': {'min': 'null', 'max': 'null', 'mode': '""', 'emitter_index': 'null', 'emitter_name': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true'}, 'set_niagara_emitter_persistent_ids': {'emitter_index': 'null', 'emitter_name': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true', 'wait_for_completion': 'true', 'timeout_seconds': '60', 'poll_interval_seconds': '0.1'}, 'remove_niagara_emitter': {'emitter_index': 'null', 'emitter_name': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true'}}
REQUIRED_BY_METHOD = {'inspect_niagara_emitter_properties': ('system_path',), 'set_niagara_emitter_enabled': ('system_path', 'enabled'), 'set_niagara_emitter_local_space': ('system_path', 'value'), 'set_niagara_emitter_sim_target': ('system_path', 'sim_target'), 'set_niagara_emitter_fixed_bounds': ('system_path',), 'set_niagara_emitter_persistent_ids': ('system_path', 'enabled'), 'remove_niagara_emitter': ('system_path',)}
ATTACHMENT_LIBRARY = 'CubelessNiagaraEmitterConfigurationAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_emitter_configuration_toolset', 'verify_niagara_emitter_configuration_toolset', 'remove_niagara_emitter_configuration_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'niagara_emitter_public3_7_7_8_9_10_6_schema_verified', 'required1_2_2_2_1_2_1_default2_5_5_6_8_8_5_verified', 'json_value_carrier50_schema_verified', 'nullable_bounds_and_selector_model_preserved', 'body_omission_and_raw_presence_preserved', 'hidden_persistent_value1_not_public_schema_verified', 'niagara_emitter7_native_contract_owned')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();async_status=domain.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()) and all(value is True for value in async_status["Native_bindings"].values()),finite_Named_continuation=async_status,library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=7,
        public_input_counts=dict(zip(BINDINGS,(3,7,7,8,9,10,6))),physical_input_counts=dict(zip(BINDINGS,(3,7,7,8,9,10,6))),native_input_counts=dict(zip(BINDINGS,(1,1,1,1,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0,0,0,1,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=7:raise RuntimeError('Exact NiagaraEmitterConfiguration Toolset7 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Niagara emitter methods required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(3,7,7,8,9,10,6))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraEmitterConfiguration wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraEmitterConfiguration wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
