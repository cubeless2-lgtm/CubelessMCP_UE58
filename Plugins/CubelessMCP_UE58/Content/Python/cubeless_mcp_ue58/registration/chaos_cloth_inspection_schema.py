"""Chaos Cloth inspection exact original eager logical schemas versus public JSON FString carriers."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import chaos_cloth_inspection as api
TOOLSET = 'cubeless_mcp_ue58.registration.chaos_cloth_inspection_toolset.CubelessChaosClothInspectionTools'
BINDINGS = ('inspect_chaos_cloth_asset', 'inspect_chaos_cloth_simulation_mesh')
PUBLIC_FIELDS_BY_METHOD = {'inspect_chaos_cloth_asset': ('cloth_asset_path', 'lod_index', 'property_name_filter', 'include_property_values', 'include_property_flags', 'include_dataflow_nodes', 'max_properties', 'max_nodes'), 'inspect_chaos_cloth_simulation_mesh': ('cloth_asset_path', 'lod_index', 'weight_map_name', 'vertex_start', 'max_vertices', 'face_start', 'max_faces', 'include_positions', 'include_normals', 'include_indices', 'include_weight_values')}
LOGICAL_INPUT_SCHEMAS = {'inspect_chaos_cloth_asset': {'properties': {'cloth_asset_path': {'title': 'Cloth Asset Path', 'type': 'string'}, 'lod_index': {'default': -1, 'title': 'Lod Index', 'type': 'integer'}, 'property_name_filter': {'default': '', 'title': 'Property Name Filter', 'type': 'string'}, 'include_property_values': {'default': True, 'title': 'Include Property Values', 'type': 'boolean'}, 'include_property_flags': {'default': True, 'title': 'Include Property Flags', 'type': 'boolean'}, 'include_dataflow_nodes': {'default': True, 'title': 'Include Dataflow Nodes', 'type': 'boolean'}, 'max_properties': {'default': 512, 'title': 'Max Properties', 'type': 'integer'}, 'max_nodes': {'default': 256, 'title': 'Max Nodes', 'type': 'integer'}}, 'required': ['cloth_asset_path'], 'title': 'inspect_chaos_cloth_assetArguments', 'type': 'object'}, 'inspect_chaos_cloth_simulation_mesh': {'properties': {'cloth_asset_path': {'title': 'Cloth Asset Path', 'type': 'string'}, 'lod_index': {'default': 0, 'title': 'Lod Index', 'type': 'integer'}, 'weight_map_name': {'default': '', 'title': 'Weight Map Name', 'type': 'string'}, 'vertex_start': {'default': 0, 'title': 'Vertex Start', 'type': 'integer'}, 'max_vertices': {'default': 4096, 'title': 'Max Vertices', 'type': 'integer'}, 'face_start': {'default': 0, 'title': 'Face Start', 'type': 'integer'}, 'max_faces': {'default': 4096, 'title': 'Max Faces', 'type': 'integer'}, 'include_positions': {'default': True, 'title': 'Include Positions', 'type': 'boolean'}, 'include_normals': {'default': False, 'title': 'Include Normals', 'type': 'boolean'}, 'include_indices': {'default': True, 'title': 'Include Indices', 'type': 'boolean'}, 'include_weight_values': {'default': True, 'title': 'Include Weight Values', 'type': 'boolean'}}, 'required': ['cloth_asset_path'], 'title': 'inspect_chaos_cloth_simulation_meshArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'inspect_chaos_cloth_asset': {'lod_index': '-1', 'property_name_filter': '""', 'include_property_values': 'true', 'include_property_flags': 'true', 'include_dataflow_nodes': 'true', 'max_properties': '512', 'max_nodes': '256'}, 'inspect_chaos_cloth_simulation_mesh': {'lod_index': '0', 'weight_map_name': '""', 'vertex_start': '0', 'max_vertices': '4096', 'face_start': '0', 'max_faces': '4096', 'include_positions': 'true', 'include_normals': 'false', 'include_indices': 'true', 'include_weight_values': 'true'}}
REQUIRED_BY_METHOD = {'inspect_chaos_cloth_asset': ('cloth_asset_path',), 'inspect_chaos_cloth_simulation_mesh': ('cloth_asset_path',)}
ATTACHMENT_LIBRARY = 'CubelessChaosClothInspectionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_chaos_cloth_inspection_toolset', 'verify_chaos_cloth_inspection_toolset', 'remove_chaos_cloth_inspection_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string4_schema_verified', 'integer8_boolean7_schema_verified', 'chaos_cloth_inspection_public8_11_schema_verified', 'required1_1_default7_10_verified', 'json_value_carrier19_schema_verified')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=2,
        public_input_counts=dict(zip(BINDINGS,(8,11))),physical_input_counts=dict(zip(BINDINGS,(8,11))),native_input_counts=dict(zip(BINDINGS,(1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0))),
        schema_verified=False,default_verified=False,raw_guard_required=True,raw_guard_ready=False,nullable_raw_guard_ready=False,boolean_raw_guard_ready=False,number_raw_guard_ready=False,
        actual_official_wire_verified=False,nullable_Named_wire_verified=False,boolean_Named_wire_verified=False,number_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _required(expected,actual):return type(actual) is list and len(actual)==len(expected) and all(type(v) is str for v in actual) and set(actual)==set(expected)

def _exact(a,b):
    if type(a) in (int,float) and type(b) in (int,float):return a==b
    if type(a) is not type(b):return False
    if type(a) is dict:return set(a)==set(b) and all(_exact(a[k],b[k]) for k in a)
    if type(a) is list:return len(a)==len(b) and all(_exact(x,y) for x,y in zip(a,b))
    return a==b

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=2:raise RuntimeError('Exact ChaosClothInspection Toolset2 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('Exactly two distinct original Chaos Cloth inspection methods required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original method unavailable')
        inputs=row.get('inputSchema')
        if not reflected_carriers:
            if not _exact(inputs,LOGICAL_INPUT_SCHEMAS[method]):raise RuntimeError('Exact original eager input schema required; no new length/range/extra-field policy')
        else:
            if type(inputs) is not dict or inputs.get('type')!='object':raise RuntimeError('Physical object required')
            props=inputs.get('properties',{});required=inputs.get('required',[])
            if type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(REQUIRED_BY_METHOD[method],required):raise RuntimeError('Exact physical8/11 fields/required required')
            for field in PUBLIC_FIELDS_BY_METHOD[method]:
                item=props[field]
                if type(item) is not dict or item.get('type')!='string':raise RuntimeError('Physical JSON FString carrier required')
                if field in REQUIRED_BY_METHOD[method]:
                    if 'default' in item:raise RuntimeError('Required cloth asset path field has physical default')
                elif type(item.get('default')) is not str or item['default']!=PHYSICAL_DEFAULTS_BY_METHOD[method][field]:raise RuntimeError('Exact physical JSON default required')
        output=row.get('outputSchema');outer=output.get('properties') if type(output) is dict else None
        if type(output) is not dict or output.get('type')!='object' or type(outer) is not dict or set(outer)!={'returnValue'} or not _required(('returnValue',),output.get('required')):raise RuntimeError('Exact returnValue outer required')
        result=outer['returnValue'];props=result.get('properties') if type(result) is dict else None
        if type(result) is not dict or result.get('type')!='object' or type(props) is not dict or set(props)!=set(RESULT_FIELDS) or not _required(RESULT_FIELDS,result.get('required')) or any(type(props[k]) is not dict or props[k].get('type')!=('boolean' if k=='success' else 'string') for k in RESULT_FIELDS):raise RuntimeError('Exact standard11 output required')
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(8,11))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered ChaosClothInspection wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered ChaosClothInspection wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
