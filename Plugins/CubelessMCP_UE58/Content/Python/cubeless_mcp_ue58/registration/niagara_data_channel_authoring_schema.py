"""Niagara input parameters exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_data_channel_authoring as api
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_data_channel_authoring_toolset.CubelessNiagaraDataChannelAuthoringTools'
BINDINGS = ('configure_niagara_data_channel_asset', 'create_niagara_ndc_reader_scratch_pad_module', 'add_niagara_scratch_pad_di_variadic_input')
PUBLIC_FIELDS_BY_METHOD = {'configure_niagara_data_channel_asset': ('asset_path', 'variables', 'create_if_missing', 'keep_previous_frame_data', 'save'), 'create_niagara_ndc_reader_scratch_pad_module': ('system_path', 'scratch_pad_name', 'data_channel_asset_path', 'stage', 'position_variable', 'velocity_variable', 'spawn_count_variable', 'event_type_variable', 'position_target', 'velocity_target', 'spawn_count_target', 'event_type_target', 'read_success_target', 'read_current_frame', 'dry_run', 'allow_source_edit', 'save', 'request_compile'), 'add_niagara_scratch_pad_di_variadic_input': ('system_path', 'scratch_pad_name', 'function_node_guid', 'expected_function_name', 'expected_di_class', 'expected_existing_inputs', 'pin_name', 'module_input_name', 'expected_referencing_module_node_guid', 'default_value', 'input_type', 'scratch_pad_script_path', 'expected_input_map_node_guid', 'expected_output_node_guid', 'allow_source_edit', 'save', 'request_compile', 'wait_for_completion', 'timeout_seconds', 'poll_interval_seconds')}
LOGICAL_INPUT_SCHEMAS = {'configure_niagara_data_channel_asset': {'properties': {'asset_path': {'title': 'Asset Path', 'type': 'string'}, 'variables': {'anyOf': [{'items': {'additionalProperties': {'type': 'string'}, 'type': 'object'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Variables'}, 'create_if_missing': {'default': True, 'title': 'Create If Missing', 'type': 'boolean'}, 'keep_previous_frame_data': {'default': True, 'title': 'Keep Previous Frame Data', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}}, 'required': ['asset_path'], 'title': 'configure_niagara_data_channel_assetArguments', 'type': 'object'}, 'create_niagara_ndc_reader_scratch_pad_module': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'scratch_pad_name': {'title': 'Scratch Pad Name', 'type': 'string'}, 'data_channel_asset_path': {'title': 'Data Channel Asset Path', 'type': 'string'}, 'stage': {'default': 'particle_spawn_read', 'title': 'Stage', 'type': 'string'}, 'position_variable': {'default': 'Position', 'title': 'Position Variable', 'type': 'string'}, 'velocity_variable': {'default': 'Velocity', 'title': 'Velocity Variable', 'type': 'string'}, 'spawn_count_variable': {'default': 'SpawnCount', 'title': 'Spawn Count Variable', 'type': 'string'}, 'event_type_variable': {'default': 'EventType', 'title': 'Event Type Variable', 'type': 'string'}, 'position_target': {'default': 'Particles.Position', 'title': 'Position Target', 'type': 'string'}, 'velocity_target': {'default': 'Particles.Velocity', 'title': 'Velocity Target', 'type': 'string'}, 'spawn_count_target': {'default': 'Particles.NDCSpawnCount', 'title': 'Spawn Count Target', 'type': 'string'}, 'event_type_target': {'default': 'Particles.NDCEventType', 'title': 'Event Type Target', 'type': 'string'}, 'read_success_target': {'default': 'Particles.NDCReadSuccess', 'title': 'Read Success Target', 'type': 'string'}, 'read_current_frame': {'default': False, 'title': 'Read Current Frame', 'type': 'boolean'}, 'dry_run': {'default': False, 'title': 'Dry Run', 'type': 'boolean'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}}, 'required': ['system_path', 'scratch_pad_name', 'data_channel_asset_path'], 'title': 'create_niagara_ndc_reader_scratch_pad_moduleArguments', 'type': 'object'}, 'add_niagara_scratch_pad_di_variadic_input': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'scratch_pad_name': {'title': 'Scratch Pad Name', 'type': 'string'}, 'function_node_guid': {'title': 'Function Node Guid', 'type': 'string'}, 'expected_function_name': {'title': 'Expected Function Name', 'type': 'string'}, 'expected_di_class': {'title': 'Expected Di Class', 'type': 'string'}, 'expected_existing_inputs': {'items': {'type': 'object'}, 'title': 'Expected Existing Inputs', 'type': 'array'}, 'pin_name': {'title': 'Pin Name', 'type': 'string'}, 'module_input_name': {'title': 'Module Input Name', 'type': 'string'}, 'expected_referencing_module_node_guid': {'title': 'Expected Referencing Module Node Guid', 'type': 'string'}, 'default_value': {'default': 0, 'title': 'Default Value'}, 'input_type': {'default': 'int', 'title': 'Input Type', 'type': 'string'}, 'scratch_pad_script_path': {'default': '', 'title': 'Scratch Pad Script Path', 'type': 'string'}, 'expected_input_map_node_guid': {'default': '', 'title': 'Expected Input Map Node Guid', 'type': 'string'}, 'expected_output_node_guid': {'default': '', 'title': 'Expected Output Node Guid', 'type': 'string'}, 'allow_source_edit': {'default': False, 'title': 'Allow Source Edit', 'type': 'boolean'}, 'save': {'default': True, 'title': 'Save', 'type': 'boolean'}, 'request_compile': {'default': True, 'title': 'Request Compile', 'type': 'boolean'}, 'wait_for_completion': {'default': True, 'title': 'Wait For Completion', 'type': 'boolean'}, 'timeout_seconds': {'default': 60, 'title': 'Timeout Seconds', 'type': 'number'}, 'poll_interval_seconds': {'default': 0.1, 'title': 'Poll Interval Seconds', 'type': 'number'}}, 'required': ['system_path', 'scratch_pad_name', 'function_node_guid', 'expected_function_name', 'expected_di_class', 'expected_existing_inputs', 'pin_name', 'module_input_name', 'expected_referencing_module_node_guid'], 'title': 'add_niagara_scratch_pad_di_variadic_inputArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'configure_niagara_data_channel_asset': {'variables': 'null', 'create_if_missing': 'true', 'keep_previous_frame_data': 'true', 'save': 'true'}, 'create_niagara_ndc_reader_scratch_pad_module': {'stage': '"particle_spawn_read"', 'position_variable': '"Position"', 'velocity_variable': '"Velocity"', 'spawn_count_variable': '"SpawnCount"', 'event_type_variable': '"EventType"', 'position_target': '"Particles.Position"', 'velocity_target': '"Particles.Velocity"', 'spawn_count_target': '"Particles.NDCSpawnCount"', 'event_type_target': '"Particles.NDCEventType"', 'read_success_target': '"Particles.NDCReadSuccess"', 'read_current_frame': 'false', 'dry_run': 'false', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true'}, 'add_niagara_scratch_pad_di_variadic_input': {'default_value': '0', 'input_type': '"int"', 'scratch_pad_script_path': '""', 'expected_input_map_node_guid': '""', 'expected_output_node_guid': '""', 'allow_source_edit': 'false', 'save': 'true', 'request_compile': 'true', 'wait_for_completion': 'true', 'timeout_seconds': '60', 'poll_interval_seconds': '0.1'}}
REQUIRED_BY_METHOD = {'configure_niagara_data_channel_asset': ('asset_path',), 'create_niagara_ndc_reader_scratch_pad_module': ('system_path', 'scratch_pad_name', 'data_channel_asset_path'), 'add_niagara_scratch_pad_di_variadic_input': ('system_path', 'scratch_pad_name', 'function_node_guid', 'expected_function_name', 'expected_di_class', 'expected_existing_inputs', 'pin_name', 'module_input_name', 'expected_referencing_module_node_guid')}
ATTACHMENT_LIBRARY = 'CubelessNiagaraDataChannelAuthoringAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_data_channel_authoring_toolset', 'verify_niagara_data_channel_authoring_toolset', 'remove_niagara_data_channel_authoring_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'niagara_data_channel_public5_18_20_schema_verified', 'required1_3_9_default4_15_11_verified', 'json_value_carrier43_schema_verified', 'Any_nullable_array_and_variables_omission_preserved', 'Native_only_DI_call_raw_route_not_public_tool', 'partial_compile_save_prefix_and_wait_contract_owned', 'niagara_data_channel_authoring4_native_contract_owned')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(api.BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=3,
        public_input_counts=dict(zip(BINDINGS,(5,18,20))),physical_input_counts=dict(zip(BINDINGS,(5,18,20))),native_input_counts=dict(zip(BINDINGS,(1,1,1))),native_only_method_count=1,hidden_input_counts=dict(zip(BINDINGS,(0,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=3:raise RuntimeError('Exact NiagaraDataChannelAuthoring Toolset3 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Niagara input parameter methods required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(5,18,20))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraDataChannelAuthoring wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraDataChannelAuthoring wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
