"""Viewport exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_authoring_inspection as api
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_authoring_inspection_toolset.CubelessNiagaraAuthoringInspectionTools'
BINDINGS = ('inspect_niagara_stack', 'inspect_niagara_graph', 'inspect_niagara_graph_actions', 'inspect_niagara_custom_hlsl_nodes', 'inspect_niagara_scratch_pad_interface', 'inspect_niagara_compile_status')
PUBLIC_FIELDS_BY_METHOD = {'inspect_niagara_stack': ('system_path', 'include_pins', 'max_function_calls'), 'inspect_niagara_graph': ('system_path', 'include_pins', 'include_links', 'include_scratch_pads', 'max_nodes_per_graph', 'max_links_per_graph'), 'inspect_niagara_graph_actions': ('system_path', 'scratch_pad_name', 'scratch_pad_script_path', 'scratch_pad_script_index', 'safe_only', 'search_text', 'max_actions', 'include_canonical', 'include_template_pins'), 'inspect_niagara_custom_hlsl_nodes': ('system_path', 'node_guid', 'signature_name', 'contains_text', 'max_hlsl_chars'), 'inspect_niagara_scratch_pad_interface': ('system_path', 'include_graph_summary', 'include_parent_scratch_pads', 'max_scripts', 'max_function_calls'), 'inspect_niagara_compile_status': ('system_path', 'request_compile', 'force', 'allow_source_compile', 'wait_for_completion', 'timeout_seconds', 'poll_interval_seconds')}
LOGICAL_INPUT_SCHEMAS = {'inspect_niagara_stack': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'include_pins': {'default': False, 'title': 'Include Pins', 'type': 'boolean'}, 'max_function_calls': {'default': 200, 'title': 'Max Function Calls', 'type': 'integer'}}, 'required': ['system_path'], 'title': 'inspect_niagara_stackArguments', 'type': 'object'}, 'inspect_niagara_graph': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'include_pins': {'default': True, 'title': 'Include Pins', 'type': 'boolean'}, 'include_links': {'default': True, 'title': 'Include Links', 'type': 'boolean'}, 'include_scratch_pads': {'default': True, 'title': 'Include Scratch Pads', 'type': 'boolean'}, 'max_nodes_per_graph': {'default': 600, 'title': 'Max Nodes Per Graph', 'type': 'integer'}, 'max_links_per_graph': {'default': 2000, 'title': 'Max Links Per Graph', 'type': 'integer'}}, 'required': ['system_path'], 'title': 'inspect_niagara_graphArguments', 'type': 'object'}, 'inspect_niagara_graph_actions': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'scratch_pad_name': {'default': '', 'title': 'Scratch Pad Name', 'type': 'string'}, 'scratch_pad_script_path': {'default': '', 'title': 'Scratch Pad Script Path', 'type': 'string'}, 'scratch_pad_script_index': {'anyOf': [{'type': 'integer'}, {'type': 'null'}], 'default': None, 'title': 'Scratch Pad Script Index'}, 'safe_only': {'default': True, 'title': 'Safe Only', 'type': 'boolean'}, 'search_text': {'default': '', 'title': 'Search Text', 'type': 'string'}, 'max_actions': {'default': 600, 'title': 'Max Actions', 'type': 'integer'}, 'include_canonical': {'default': False, 'title': 'Include Canonical', 'type': 'boolean'}, 'include_template_pins': {'default': False, 'title': 'Include Template Pins', 'type': 'boolean'}}, 'required': ['system_path'], 'title': 'inspect_niagara_graph_actionsArguments', 'type': 'object'}, 'inspect_niagara_custom_hlsl_nodes': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'node_guid': {'default': '', 'title': 'Node Guid', 'type': 'string'}, 'signature_name': {'default': '', 'title': 'Signature Name', 'type': 'string'}, 'contains_text': {'default': '', 'title': 'Contains Text', 'type': 'string'}, 'max_hlsl_chars': {'default': 4000, 'title': 'Max Hlsl Chars', 'type': 'integer'}}, 'required': ['system_path'], 'title': 'inspect_niagara_custom_hlsl_nodesArguments', 'type': 'object'}, 'inspect_niagara_scratch_pad_interface': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'include_graph_summary': {'default': True, 'title': 'Include Graph Summary', 'type': 'boolean'}, 'include_parent_scratch_pads': {'default': True, 'title': 'Include Parent Scratch Pads', 'type': 'boolean'}, 'max_scripts': {'default': 200, 'title': 'Max Scripts', 'type': 'integer'}, 'max_function_calls': {'default': 80, 'title': 'Max Function Calls', 'type': 'integer'}}, 'required': ['system_path'], 'title': 'inspect_niagara_scratch_pad_interfaceArguments', 'type': 'object'}, 'inspect_niagara_compile_status': {'properties': {'system_path': {'title': 'System Path', 'type': 'string'}, 'request_compile': {'default': False, 'title': 'Request Compile', 'type': 'boolean'}, 'force': {'default': False, 'title': 'Force', 'type': 'boolean'}, 'allow_source_compile': {'default': False, 'title': 'Allow Source Compile', 'type': 'boolean'}, 'wait_for_completion': {'default': False, 'title': 'Wait For Completion', 'type': 'boolean'}, 'timeout_seconds': {'default': 10, 'title': 'Timeout Seconds', 'type': 'number'}, 'poll_interval_seconds': {'default': 0.1, 'title': 'Poll Interval Seconds', 'type': 'number'}}, 'required': ['system_path'], 'title': 'inspect_niagara_compile_statusArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'inspect_niagara_stack': {'include_pins': 'false', 'max_function_calls': '200'}, 'inspect_niagara_graph': {'include_pins': 'true', 'include_links': 'true', 'include_scratch_pads': 'true', 'max_nodes_per_graph': '600', 'max_links_per_graph': '2000'}, 'inspect_niagara_graph_actions': {'scratch_pad_name': '""', 'scratch_pad_script_path': '""', 'scratch_pad_script_index': 'null', 'safe_only': 'true', 'search_text': '""', 'max_actions': '600', 'include_canonical': 'false', 'include_template_pins': 'false'}, 'inspect_niagara_custom_hlsl_nodes': {'node_guid': '""', 'signature_name': '""', 'contains_text': '""', 'max_hlsl_chars': '4000'}, 'inspect_niagara_scratch_pad_interface': {'include_graph_summary': 'true', 'include_parent_scratch_pads': 'true', 'max_scripts': '200', 'max_function_calls': '80'}, 'inspect_niagara_compile_status': {'request_compile': 'false', 'force': 'false', 'allow_source_compile': 'false', 'wait_for_completion': 'false', 'timeout_seconds': '10', 'poll_interval_seconds': '0.1'}}
REQUIRED_BY_METHOD = {'inspect_niagara_stack': ('system_path',), 'inspect_niagara_graph': ('system_path',), 'inspect_niagara_graph_actions': ('system_path',), 'inspect_niagara_custom_hlsl_nodes': ('system_path',), 'inspect_niagara_scratch_pad_interface': ('system_path',), 'inspect_niagara_compile_status': ('system_path',)}
ATTACHMENT_LIBRARY = 'CubelessNiagaraAuthoringInspectionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_authoring_inspection_toolset', 'verify_niagara_authoring_inspection_toolset', 'remove_niagara_authoring_inspection_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'niagara_inspection_public3_6_9_5_5_7_schema_verified', 'required1_each_default2_5_8_4_4_6_verified', 'json_value_carrier35_schema_verified', 'nullable_scratch_index_model_preserved', 'body_omission_and_raw_presence_preserved', 'hidden0_and_original_model_extra_drop_verified', 'niagara_inspection6_native_contract_owned')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    jobs=api.async_binding_status();methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(v is True for v in jobs.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,async_native_jobs=jobs,existing_HN_compile_status_continuation=True,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=6,
        public_input_counts=dict(zip(BINDINGS,(3,6,9,5,5,7))),physical_input_counts=dict(zip(BINDINGS,(3,6,9,5,5,7))),native_input_counts=dict(zip(BINDINGS,(1,1,1,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0,0,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=6:raise RuntimeError('Exact NiagaraAuthoringInspection Toolset6 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Niagara authoring inspection methods required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(3,6,9,5,5,7))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraAuthoringInspection wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraAuthoringInspection wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
