"""PCG exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import pcg_inspection as api
TOOLSET = 'cubeless_mcp_ue58.registration.pcg_inspection_toolset.CubelessPCGInspectionTools'
BINDINGS = ('resolve_pcg_graph', 'list_pcg_graph_nodes', 'inspect_pcg_custom_hlsl_nodes', 'inspect_pcg_component_outputs')
PUBLIC_FIELDS_BY_METHOD = {'resolve_pcg_graph': ('graph_path',), 'list_pcg_graph_nodes': ('graph_path', 'node_type', 'title_contains', 'include_pins'), 'inspect_pcg_custom_hlsl_nodes': ('graph_path', 'node_id', 'title_contains', 'include_pins', 'include_declarations', 'max_source_chars'), 'inspect_pcg_component_outputs': ('actor_label', 'actor_name', 'actor_path', 'actor_tag', 'actor_label_prefix', 'component_name', 'component_tag', 'selected_only', 'max_components', 'max_tagged_data', 'max_points', 'include_point_samples')}
LOGICAL_INPUT_SCHEMAS = {'resolve_pcg_graph': {'properties': {'graph_path': {'title': 'Graph Path', 'type': 'string'}}, 'required': ['graph_path'], 'title': 'resolve_pcg_graphArguments', 'type': 'object'}, 'list_pcg_graph_nodes': {'properties': {'graph_path': {'title': 'Graph Path', 'type': 'string'}, 'node_type': {'default': '', 'title': 'Node Type', 'type': 'string'}, 'title_contains': {'default': '', 'title': 'Title Contains', 'type': 'string'}, 'include_pins': {'default': True, 'title': 'Include Pins', 'type': 'boolean'}}, 'required': ['graph_path'], 'title': 'list_pcg_graph_nodesArguments', 'type': 'object'}, 'inspect_pcg_custom_hlsl_nodes': {'properties': {'graph_path': {'title': 'Graph Path', 'type': 'string'}, 'node_id': {'default': '', 'title': 'Node Id', 'type': 'string'}, 'title_contains': {'default': '', 'title': 'Title Contains', 'type': 'string'}, 'include_pins': {'default': True, 'title': 'Include Pins', 'type': 'boolean'}, 'include_declarations': {'default': True, 'title': 'Include Declarations', 'type': 'boolean'}, 'max_source_chars': {'default': 4000, 'title': 'Max Source Chars', 'type': 'integer'}}, 'required': ['graph_path'], 'title': 'inspect_pcg_custom_hlsl_nodesArguments', 'type': 'object'}, 'inspect_pcg_component_outputs': {'properties': {'actor_label': {'default': '', 'title': 'Actor Label', 'type': 'string'}, 'actor_name': {'default': '', 'title': 'Actor Name', 'type': 'string'}, 'actor_path': {'default': '', 'title': 'Actor Path', 'type': 'string'}, 'actor_tag': {'default': '', 'title': 'Actor Tag', 'type': 'string'}, 'actor_label_prefix': {'default': '', 'title': 'Actor Label Prefix', 'type': 'string'}, 'component_name': {'default': '', 'title': 'Component Name', 'type': 'string'}, 'component_tag': {'default': '', 'title': 'Component Tag', 'type': 'string'}, 'selected_only': {'default': False, 'title': 'Selected Only', 'type': 'boolean'}, 'max_components': {'default': 16, 'title': 'Max Components', 'type': 'integer'}, 'max_tagged_data': {'default': 64, 'title': 'Max Tagged Data', 'type': 'integer'}, 'max_points': {'default': 8, 'title': 'Max Points', 'type': 'integer'}, 'include_point_samples': {'default': True, 'title': 'Include Point Samples', 'type': 'boolean'}}, 'title': 'inspect_pcg_component_outputsArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'resolve_pcg_graph': {}, 'list_pcg_graph_nodes': {'node_type': '""', 'title_contains': '""', 'include_pins': 'true'}, 'inspect_pcg_custom_hlsl_nodes': {'node_id': '""', 'title_contains': '""', 'include_pins': 'true', 'include_declarations': 'true', 'max_source_chars': '4000'}, 'inspect_pcg_component_outputs': {'actor_label': '""', 'actor_name': '""', 'actor_path': '""', 'actor_tag': '""', 'actor_label_prefix': '""', 'component_name': '""', 'component_tag': '""', 'selected_only': 'false', 'max_components': '16', 'max_tagged_data': '64', 'max_points': '8', 'include_point_samples': 'true'}}
REQUIRED_BY_METHOD = {'resolve_pcg_graph': ('graph_path',), 'list_pcg_graph_nodes': ('graph_path',), 'inspect_pcg_custom_hlsl_nodes': ('graph_path',), 'inspect_pcg_component_outputs': ()}
ATTACHMENT_LIBRARY = 'CubelessPCGInspectionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_pcg_inspection_toolset', 'verify_pcg_inspection_toolset', 'remove_pcg_inspection_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'pcg_inspection_public1_4_6_12_schema_verified', 'required1_1_1_0_default0_3_5_12_verified', 'json_value_carrier23_schema_verified', 'scalar_original_model_and_no_body_coercion_preserved', 'body_all_fields_and_raw_presence_preserved', 'hidden0_and_original_model_extra_drop_verified', 'pcg_inspection4_native_contract_owned')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=4,
        public_input_counts=dict(zip(BINDINGS,(1,4,6,12))),physical_input_counts=dict(zip(BINDINGS,(1,4,6,12))),native_input_counts=dict(zip(BINDINGS,(1,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=4:raise RuntimeError('Exact PCGInspection Toolset4 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original PCG inspection methods required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(1,4,6,12))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered PCGInspection wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered PCGInspection wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
