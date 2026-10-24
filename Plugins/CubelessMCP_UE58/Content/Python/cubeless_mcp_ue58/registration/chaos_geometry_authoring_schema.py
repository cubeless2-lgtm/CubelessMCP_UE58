"""Viewport exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import chaos_geometry_authoring as api
TOOLSET = 'cubeless_mcp_ue58.registration.chaos_geometry_authoring_toolset.CubelessChaosGeometryAuthoringTools'
BINDINGS = ('create_geometry_collection_from_mesh', 'configure_building_fracture', 'configure_anchor_field')
PUBLIC_FIELDS_BY_METHOD = {'create_geometry_collection_from_mesh': ('source_static_mesh_paths', 'output_geometry_collection_path', 'source_transforms', 'root_mesh_proxy_static_mesh_path', 'material_reindex', 'overwrite_existing'), 'configure_building_fracture': ('source_geometry_collection_path', 'output_geometry_collection_path', 'fracture_method', 'site_count', 'random_seed', 'cluster_levels', 'cluster_size', 'damage_thresholds', 'merge_tiny_geometry', 'tiny_relative_threshold', 'collision_object_reduction_percentage', 'maximum_collision_particles', 'collision_implicit_type', 'remove_on_sleep', 'remove_on_break', 'sleep_time_seconds', 'removal_duration_seconds', 'break_timer_seconds', 'root_mesh_proxy_static_mesh_path', 'overwrite_existing'), 'configure_anchor_field': ('geometry_collection_actor_name', 'field_actor_name', 'box_center', 'box_extent', 'field_magnitude', 'save_level')}
LOGICAL_INPUT_SCHEMAS = {'create_geometry_collection_from_mesh': {'properties': {'source_static_mesh_paths': {'items': {'type': 'string'}, 'title': 'Source Static Mesh Paths', 'type': 'array'}, 'output_geometry_collection_path': {'title': 'Output Geometry Collection Path', 'type': 'string'}, 'source_transforms': {'anyOf': [{'items': {'type': 'object'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Source Transforms'}, 'root_mesh_proxy_static_mesh_path': {'default': '', 'title': 'Root Mesh Proxy Static Mesh Path', 'type': 'string'}, 'material_reindex': {'default': True, 'title': 'Material Reindex', 'type': 'boolean'}, 'overwrite_existing': {'default': False, 'title': 'Overwrite Existing', 'type': 'boolean'}}, 'required': ['source_static_mesh_paths', 'output_geometry_collection_path'], 'title': 'create_geometry_collection_from_meshArguments', 'type': 'object'}, 'configure_building_fracture': {'properties': {'source_geometry_collection_path': {'title': 'Source Geometry Collection Path', 'type': 'string'}, 'output_geometry_collection_path': {'title': 'Output Geometry Collection Path', 'type': 'string'}, 'fracture_method': {'default': 'uniform_voronoi', 'title': 'Fracture Method', 'type': 'string'}, 'site_count': {'default': 64, 'title': 'Site Count', 'type': 'integer'}, 'random_seed': {'default': 1337, 'title': 'Random Seed', 'type': 'integer'}, 'cluster_levels': {'default': 2, 'title': 'Cluster Levels', 'type': 'integer'}, 'cluster_size': {'default': 8, 'title': 'Cluster Size', 'type': 'integer'}, 'damage_thresholds': {'anyOf': [{'items': {'type': 'number'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Damage Thresholds'}, 'merge_tiny_geometry': {'default': True, 'title': 'Merge Tiny Geometry', 'type': 'boolean'}, 'tiny_relative_threshold': {'default': 0.01, 'title': 'Tiny Relative Threshold', 'type': 'number'}, 'collision_object_reduction_percentage': {'default': 20, 'title': 'Collision Object Reduction Percentage', 'type': 'number'}, 'maximum_collision_particles': {'default': 60, 'title': 'Maximum Collision Particles', 'type': 'integer'}, 'collision_implicit_type': {'default': 'convex', 'title': 'Collision Implicit Type', 'type': 'string'}, 'remove_on_sleep': {'default': True, 'title': 'Remove On Sleep', 'type': 'boolean'}, 'remove_on_break': {'default': True, 'title': 'Remove On Break', 'type': 'boolean'}, 'sleep_time_seconds': {'default': 5, 'title': 'Sleep Time Seconds', 'type': 'number'}, 'removal_duration_seconds': {'default': 1, 'title': 'Removal Duration Seconds', 'type': 'number'}, 'break_timer_seconds': {'default': 1, 'title': 'Break Timer Seconds', 'type': 'number'}, 'root_mesh_proxy_static_mesh_path': {'default': '', 'title': 'Root Mesh Proxy Static Mesh Path', 'type': 'string'}, 'overwrite_existing': {'default': False, 'title': 'Overwrite Existing', 'type': 'boolean'}}, 'required': ['source_geometry_collection_path', 'output_geometry_collection_path'], 'title': 'configure_building_fractureArguments', 'type': 'object'}, 'configure_anchor_field': {'properties': {'geometry_collection_actor_name': {'title': 'Geometry Collection Actor Name', 'type': 'string'}, 'field_actor_name': {'default': 'Chaos_AnchorField', 'title': 'Field Actor Name', 'type': 'string'}, 'box_center': {'anyOf': [{'items': {'type': 'number'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Box Center'}, 'box_extent': {'anyOf': [{'items': {'type': 'number'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Box Extent'}, 'field_magnitude': {'default': 2, 'title': 'Field Magnitude', 'type': 'integer'}, 'save_level': {'default': True, 'title': 'Save Level', 'type': 'boolean'}}, 'required': ['geometry_collection_actor_name'], 'title': 'configure_anchor_fieldArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'create_geometry_collection_from_mesh': {'source_transforms': 'null', 'root_mesh_proxy_static_mesh_path': '""', 'material_reindex': 'true', 'overwrite_existing': 'false'}, 'configure_building_fracture': {'fracture_method': '"uniform_voronoi"', 'site_count': '64', 'random_seed': '1337', 'cluster_levels': '2', 'cluster_size': '8', 'damage_thresholds': 'null', 'merge_tiny_geometry': 'true', 'tiny_relative_threshold': '0.01', 'collision_object_reduction_percentage': '20', 'maximum_collision_particles': '60', 'collision_implicit_type': '"convex"', 'remove_on_sleep': 'true', 'remove_on_break': 'true', 'sleep_time_seconds': '5', 'removal_duration_seconds': '1', 'break_timer_seconds': '1', 'root_mesh_proxy_static_mesh_path': '""', 'overwrite_existing': 'false'}, 'configure_anchor_field': {'field_actor_name': '"Chaos_AnchorField"', 'box_center': 'null', 'box_extent': 'null', 'field_magnitude': '2', 'save_level': 'true'}}
REQUIRED_BY_METHOD = {'create_geometry_collection_from_mesh': ('source_static_mesh_paths', 'output_geometry_collection_path'), 'configure_building_fracture': ('source_geometry_collection_path', 'output_geometry_collection_path'), 'configure_anchor_field': ('geometry_collection_actor_name',)}
ATTACHMENT_LIBRARY = 'CubelessChaosGeometryAuthoringAttachmentLibrary'
ATTACHMENT_METHODS = ('install_chaos_geometry_authoring_toolset', 'verify_chaos_geometry_authoring_toolset', 'remove_chaos_geometry_authoring_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string9_schema_verified', 'integer6_number5_schema_verified', 'array5_nestedAny_schema_verified', 'nullable_array4_schema_verified', 'chaos_geometry_authoring_public6_20_6_schema_verified', 'required2_2_1_default4_18_5_verified', 'json_value_carrier32_schema_verified')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=3,
        public_input_counts=dict(zip(BINDINGS,(6,20,6))),physical_input_counts=dict(zip(BINDINGS,(6,20,6))),native_input_counts=dict(zip(BINDINGS,(1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,4,1))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=3:raise RuntimeError('Exact ChaosGeometryAuthoring Toolset3 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('Exactly three distinct original Chaos geometry authoring methods required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original method unavailable')
        inputs=row.get('inputSchema')
        if not reflected_carriers:
            if not _exact(inputs,LOGICAL_INPUT_SCHEMAS[method]):raise RuntimeError('Exact original eager input schema required; no new length/range/extra-field policy')
        else:
            if type(inputs) is not dict or inputs.get('type')!='object':raise RuntimeError('Physical object required')
            props=inputs.get('properties',{});required=inputs.get('required',[])
            if type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(REQUIRED_BY_METHOD[method],required):raise RuntimeError('Exact physical6/20/6 fields/required required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(6,20,6))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered ChaosGeometryAuthoring wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered ChaosGeometryAuthoring wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
