"""HM original1/9 logical inputs, exact JSON FString carriers and std11 output."""
import json
import math
from cubeless_mcp_ue58.operations import blueprint_resolution_connection as api
TOOLSET="cubeless_mcp_ue58.registration.blueprint_resolution_connection_toolset.CubelessBlueprintResolutionConnectionTools"
BINDINGS=["resolve_blueprint", "connect_blueprint_nodes"]
PUBLIC_FIELDS_BY_METHOD={"resolve_blueprint": ["blueprint_name"], "connect_blueprint_nodes": ["blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin", "graph_name", "graph_id", "graph_type", "allow_pin_link_replacement"]}
LOGICAL_INPUT_SCHEMAS={"resolve_blueprint": {"properties": {"blueprint_name": {"title": "Blueprint Name", "type": "string"}}, "required": ["blueprint_name"], "title": "resolve_blueprintArguments", "type": "object"}, "connect_blueprint_nodes": {"properties": {"blueprint_name": {"title": "Blueprint Name", "type": "string"}, "source_node_id": {"title": "Source Node Id", "type": "string"}, "source_pin": {"title": "Source Pin", "type": "string"}, "target_node_id": {"title": "Target Node Id", "type": "string"}, "target_pin": {"title": "Target Pin", "type": "string"}, "graph_name": {"default": "", "title": "Graph Name", "type": "string"}, "graph_id": {"default": "", "title": "Graph Id", "type": "string"}, "graph_type": {"default": "", "title": "Graph Type", "type": "string"}, "allow_pin_link_replacement": {"default": False, "title": "Allow Pin Link Replacement", "type": "boolean"}}, "required": ["blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin"], "title": "connect_blueprint_nodesArguments", "type": "object"}}
PHYSICAL_DEFAULTS_BY_METHOD={"resolve_blueprint": {}, "connect_blueprint_nodes": {"graph_name": "\"\"", "graph_id": "\"\"", "graph_type": "\"\"", "allow_pin_link_replacement": "false"}}
REQUIRED_BY_METHOD={"resolve_blueprint": ["blueprint_name"], "connect_blueprint_nodes": ["blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin"]}
ATTACHMENT_LIBRARY = 'CubelessBlueprintResolutionConnectionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_blueprint_resolution_connection_toolset', 'verify_blueprint_resolution_connection_toolset', 'remove_blueprint_resolution_connection_toolset')
ATTACHMENT_READY_FIELDS=["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "blueprint_resolution_connection_public1_9_schema_verified", "required1_5_defaults0_4_verified", "json_value_carrier10_schema_verified", "original_named_defaults_and_direct_values_preserved", "candidate_resolution_and_K2_general_connection_contract_preserved", "partial_mutation_compile_save_no_replay_preserved"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library="CubelessBlueprintResolutionConnectionLibrary",methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=2,
        public_input_counts=dict(zip(BINDINGS,(1,9))),physical_input_counts=dict(zip(BINDINGS,(1,9))),native_input_counts=dict(zip(BINDINGS,(1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,1))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=2:raise RuntimeError('Exact BlueprintResolutionConnection Toolset2 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Blueprint resolver/K2 connection contract method required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(1,9))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered BlueprintResolutionConnection wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered BlueprintResolutionConnection wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
