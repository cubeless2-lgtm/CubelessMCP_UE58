"""GY original PCG models public5/5/4 versus14 physical JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import pcg_graph_authoring as api
TOOLSET = 'cubeless_mcp_ue58.registration.pcg_graph_authoring_toolset.CubelessPCGGraphAuthoringTools'
BINDINGS = ["add_pcg_node", "connect_pcg_nodes", "set_pcg_node_setting"]
PUBLIC_FIELDS_BY_METHOD = {"add_pcg_node": ["graph_path", "settings_class", "node_position", "node_title", "settings"], "connect_pcg_nodes": ["graph_path", "from_node_id", "from_pin", "to_node_id", "to_pin"], "set_pcg_node_setting": ["graph_path", "node_id", "property_name", "value"]}
LOGICAL_INPUT_SCHEMAS = {"add_pcg_node": {"properties": {"graph_path": {"title": "Graph Path", "type": "string"}, "settings_class": {"title": "Settings Class", "type": "string"}, "node_position": {"default": None, "title": "node_position", "type": "string"}, "node_title": {"default": "", "title": "Node Title", "type": "string"}, "settings": {"anyOf": [{"type": "object"}, {"type": "null"}], "default": None, "title": "Settings"}}, "required": ["graph_path", "settings_class"], "title": "add_pcg_nodeArguments", "type": "object"}, "connect_pcg_nodes": {"properties": {"graph_path": {"title": "Graph Path", "type": "string"}, "from_node_id": {"title": "From Node Id", "type": "string"}, "from_pin": {"title": "From Pin", "type": "string"}, "to_node_id": {"title": "To Node Id", "type": "string"}, "to_pin": {"title": "To Pin", "type": "string"}}, "required": ["graph_path", "from_node_id", "from_pin", "to_node_id", "to_pin"], "title": "connect_pcg_nodesArguments", "type": "object"}, "set_pcg_node_setting": {"properties": {"graph_path": {"title": "Graph Path", "type": "string"}, "node_id": {"title": "Node Id", "type": "string"}, "property_name": {"title": "Property Name", "type": "string"}, "value": {"title": "Value"}}, "required": ["graph_path", "node_id", "property_name", "value"], "title": "set_pcg_node_settingArguments", "type": "object"}}
PHYSICAL_DEFAULTS_BY_METHOD = {"add_pcg_node": {"node_position": "null", "node_title": "\"\"", "settings": "null"}, "connect_pcg_nodes": {}, "set_pcg_node_setting": {}}
REQUIRED_BY_METHOD = {"add_pcg_node": ["graph_path", "settings_class"], "connect_pcg_nodes": ["graph_path", "from_node_id", "from_pin", "to_node_id", "to_pin"], "set_pcg_node_setting": ["graph_path", "node_id", "property_name", "value"]}
ATTACHMENT_LIBRARY = 'CubelessPCGGraphAuthoringAttachmentLibrary'
ATTACHMENT_METHODS = ('install_pcg_graph_authoring_toolset', 'verify_pcg_graph_authoring_toolset', 'remove_pcg_graph_authoring_toolset')
ATTACHMENT_READY_FIELDS = ["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "pcg_graph_authoring_public5_5_4_schema_verified", "required2_5_4_default3_0_0_verified", "json_value_carrier14_schema_verified", "original_Any_nullable_body_and_raw_presence_preserved", "PCG_construction_gate_and_partial_transitions_Native_owned", "explicit_compile_save0_not_implicit_cache_completion", "pcg_graph_authoring3_native_contract_owned"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=3,
        public_input_counts=dict(zip(BINDINGS,(5,5,4))),physical_input_counts=dict(zip(BINDINGS,(5,5,4))),native_input_counts=dict(zip(BINDINGS,(1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=3:raise RuntimeError('Exact PCGGraphAuthoring Toolset3 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original PCG editing method required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(5,5,4))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered PCGGraphAuthoring wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered PCGGraphAuthoring wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
