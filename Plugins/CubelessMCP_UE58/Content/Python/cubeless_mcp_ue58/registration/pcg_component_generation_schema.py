"""HD public13/15 versus raw22/16 with semantic hidden10/1; Named Refresh final is async while raw wait remains single-frame."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import pcg_component_generation as api
TOOLSET = 'cubeless_mcp_ue58.registration.pcg_component_generation_toolset.CubelessPCGComponentGenerationTools'
BINDINGS=['refresh_pcg_components', 'bind_pcg_generation_settings_to_blueprint', 'configure_blueprint_pcg_template']
PUBLIC_FIELDS_BY_METHOD={'refresh_pcg_components': ['actor_name', 'selected_only', 'cleanup', 'generate', 'refresh', 'force', 'wait_until_complete', 'timeout_seconds', 'poll_interval_seconds', 'min_generated_output_data_count', 'min_managed_resource_count', 'max_components', 'native_first'], 'bind_pcg_generation_settings_to_blueprint': ['blueprint_name', 'component_variable', 'generation_trigger_variable', 'regenerate_in_editor_variable', 'generate_on_drop_variable', 'generate_on_drop_default', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'sync_component_template_defaults', 'function_class', 'function_name', 'compile', 'save'], 'configure_blueprint_pcg_template': ['blueprint_path', 'component_name', 'graph_path', 'regenerate_in_editor', 'generate_on_drop_when_on_demand', 'compile', 'save', 'operation_id']}
LOGICAL_INPUT_SCHEMAS={'refresh_pcg_components': {'properties': {'actor_name': {'default': '', 'title': 'Actor Name', 'type': 'string'}, 'selected_only': {'default': False, 'title': 'Selected Only', 'type': 'boolean'}, 'cleanup': {'default': True, 'title': 'Cleanup', 'type': 'boolean'}, 'generate': {'default': True, 'title': 'Generate', 'type': 'boolean'}, 'refresh': {'default': False, 'title': 'Refresh', 'type': 'boolean'}, 'force': {'default': True, 'title': 'Force', 'type': 'boolean'}, 'wait_until_complete': {'default': False, 'title': 'Wait Until Complete', 'type': 'boolean'}, 'timeout_seconds': {'default': 10, 'title': 'Timeout Seconds', 'type': 'number'}, 'poll_interval_seconds': {'default': 0.05, 'title': 'Poll Interval Seconds', 'type': 'number'}, 'min_generated_output_data_count': {'default': -1, 'title': 'Min Generated Output Data Count', 'type': 'integer'}, 'min_managed_resource_count': {'default': -1, 'title': 'Min Managed Resource Count', 'type': 'integer'}, 'max_components': {'default': 1000, 'title': 'Max Components', 'type': 'integer'}, 'native_first': {'default': True, 'title': 'Native First', 'type': 'boolean'}}, 'title': 'refresh_pcg_componentsArguments', 'type': 'object'}, 'bind_pcg_generation_settings_to_blueprint': {'properties': {'blueprint_name': {'title': 'Blueprint Name', 'type': 'string'}, 'component_variable': {'default': 'ForestPCG', 'title': 'Component Variable', 'type': 'string'}, 'generation_trigger_variable': {'default': 'PCGGenerationTrigger', 'title': 'Generation Trigger Variable', 'type': 'string'}, 'regenerate_in_editor_variable': {'default': 'PCGRegenerateInEditor', 'title': 'Regenerate In Editor Variable', 'type': 'string'}, 'generate_on_drop_variable': {'default': '', 'title': 'Generate On Drop Variable', 'type': 'string'}, 'generate_on_drop_default': {'default': False, 'title': 'Generate On Drop Default', 'type': 'boolean'}, 'graph_name': {'default': 'UserConstructionScript', 'title': 'Graph Name', 'type': 'string'}, 'graph_id': {'default': '', 'title': 'Graph Id', 'type': 'string'}, 'graph_type': {'default': 'function', 'title': 'Graph Type', 'type': 'string'}, 'replace_existing': {'default': True, 'title': 'Replace Existing', 'type': 'boolean'}, 'sync_component_template_defaults': {'default': True, 'title': 'Sync Component Template Defaults', 'type': 'boolean'}, 'function_class': {'default': '', 'title': 'Function Class', 'type': 'string'}, 'function_name': {'default': 'ApplyPCGGenerationSettings', 'title': 'Function Name', 'type': 'string'}, 'compile': {'default': False, 'title': 'Compile', 'type': 'boolean'}, 'save': {'default': False, 'title': 'Save', 'type': 'boolean'}}, 'required': ['blueprint_name'], 'title': 'bind_pcg_generation_settings_to_blueprintArguments', 'type': 'object'}, 'configure_blueprint_pcg_template': {'properties': {'blueprint_path': {'type': 'string', 'title': 'Blueprint Path'}, 'component_name': {'type': 'string', 'title': 'Component Name'}, 'graph_path': {'type': 'string', 'title': 'Graph Path'}, 'regenerate_in_editor': {'type': 'boolean', 'title': 'Regenerate In Editor', 'default': False}, 'generate_on_drop_when_on_demand': {'type': 'boolean', 'title': 'Generate On Drop When On Demand', 'default': False}, 'compile': {'type': 'boolean', 'title': 'Compile', 'default': True}, 'save': {'type': 'boolean', 'title': 'Save', 'default': True}, 'operation_id': {'type': 'string', 'title': 'Operation Id', 'default': ''}}, 'required': ['blueprint_path', 'component_name', 'graph_path'], 'title': 'configure_blueprint_pcg_templateArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD={'refresh_pcg_components': {'actor_name': '""', 'selected_only': 'false', 'cleanup': 'true', 'generate': 'true', 'refresh': 'false', 'force': 'true', 'wait_until_complete': 'false', 'timeout_seconds': '10.0', 'poll_interval_seconds': '0.05', 'min_generated_output_data_count': '-1', 'min_managed_resource_count': '-1', 'max_components': '1000', 'native_first': 'true'}, 'bind_pcg_generation_settings_to_blueprint': {'component_variable': '"ForestPCG"', 'generation_trigger_variable': '"PCGGenerationTrigger"', 'regenerate_in_editor_variable': '"PCGRegenerateInEditor"', 'generate_on_drop_variable': '""', 'generate_on_drop_default': 'false', 'graph_name': '"UserConstructionScript"', 'graph_id': '""', 'graph_type': '"function"', 'replace_existing': 'true', 'sync_component_template_defaults': 'true', 'function_class': '""', 'function_name': '"ApplyPCGGenerationSettings"', 'compile': 'false', 'save': 'false'}, 'configure_blueprint_pcg_template': {'regenerate_in_editor': 'false', 'generate_on_drop_when_on_demand': 'false', 'compile': 'true', 'save': 'true', 'operation_id': '""'}}
REQUIRED_BY_METHOD={'refresh_pcg_components': [], 'bind_pcg_generation_settings_to_blueprint': ['blueprint_name'], 'configure_blueprint_pcg_template': ['blueprint_path', 'component_name', 'graph_path']}
ATTACHMENT_LIBRARY = 'CubelessPCGComponentGenerationAttachmentLibrary'
ATTACHMENT_METHODS = ('install_pcg_component_generation_toolset', 'verify_pcg_component_generation_toolset', 'remove_pcg_component_generation_toolset')
ATTACHMENT_READY_FIELDS=['ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'pcg_component_generation_public13_15_schema_verified', 'required0_1_default13_14_verified', 'json_value_carrier28_schema_verified', 'original_Named_raw22_16_hidden10_1_preserved', 'refresh_Named_async_and_raw_single_frame_separated', 'partial_compile_save_and_no_replay_preserved', 'pcg_component_generation2_native_contract_owned', 'pcg_template_configuration8_schema_verified']
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=3,
        public_input_counts=dict(zip(BINDINGS,(13,15,8))),physical_input_counts=dict(zip(BINDINGS,(13,15,8))),native_input_counts=dict(zip(BINDINGS,(1,1,1))),raw_input_counts=dict(zip(BINDINGS,(22,16,6))),hidden_input_counts=dict(zip(BINDINGS,(10,1,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=3:raise RuntimeError('Exact PCGComponentGeneration Toolset3 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original promotion manifest method required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(13,15,8))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered PCGComponentGeneration wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered PCGComponentGeneration wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
