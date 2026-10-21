"""Chaos cache session exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import chaos_cache_sessions as api
TOOLSET = 'cubeless_mcp_ue58.registration.chaos_cache_sessions_toolset.CubelessChaosCacheSessionTools'
BINDINGS = ('record_chaos_cache', 'validate_chaos_cache')
PUBLIC_FIELDS_BY_METHOD = {'record_chaos_cache': ('operation', 'cache_manager_actor_name', 'geometry_collection_actor_names', 'cache_collection_path', 'cache_names', 'solver_actor_name', 'duration_seconds', 'frame_rate', 'timeout_seconds', 'initial_trigger_mode', 'motion_velocity', 'per_advance_breaks_allowed', 'per_advance_breaks_reschedule_limit', 'particle_release_throttling_min_count', 'particle_release_throttling_max_count'), 'validate_chaos_cache': ('operation', 'cache_manager_actor_name', 'cache_collection_path', 'sequence_path', 'geometry_collection_actor_names', 'sample_times', 'scrub_frames', 'repeat_count', 'timeout_seconds')}
LOGICAL_INPUT_SCHEMAS = {'record_chaos_cache': {'properties': {'operation': {'title': 'Operation', 'type': 'string'}, 'cache_manager_actor_name': {'title': 'Cache Manager Actor Name', 'type': 'string'}, 'geometry_collection_actor_names': {'items': {'type': 'string'}, 'title': 'Geometry Collection Actor Names', 'type': 'array'}, 'cache_collection_path': {'title': 'Cache Collection Path', 'type': 'string'}, 'cache_names': {'anyOf': [{'items': {'type': 'string'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Cache Names'}, 'solver_actor_name': {'default': '', 'title': 'Solver Actor Name', 'type': 'string'}, 'duration_seconds': {'default': 2.0, 'title': 'Duration Seconds', 'type': 'number'}, 'frame_rate': {'default': 30.0, 'title': 'Frame Rate', 'type': 'number'}, 'timeout_seconds': {'default': 10.0, 'title': 'Timeout Seconds', 'type': 'number'}, 'initial_trigger_mode': {'default': 'strain_velocity', 'title': 'Initial Trigger Mode', 'type': 'string'}, 'motion_velocity': {'anyOf': [{'items': {'type': 'number'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Motion Velocity'}, 'per_advance_breaks_allowed': {'default': 8, 'title': 'Per Advance Breaks Allowed', 'type': 'integer'}, 'per_advance_breaks_reschedule_limit': {'default': 64, 'title': 'Per Advance Breaks Reschedule Limit', 'type': 'integer'}, 'particle_release_throttling_min_count': {'default': 64, 'title': 'Particle Release Throttling Min Count', 'type': 'integer'}, 'particle_release_throttling_max_count': {'default': 128, 'title': 'Particle Release Throttling Max Count', 'type': 'integer'}}, 'required': ['operation', 'cache_manager_actor_name', 'geometry_collection_actor_names', 'cache_collection_path'], 'title': 'record_chaos_cacheArguments', 'type': 'object'}, 'validate_chaos_cache': {'properties': {'operation': {'title': 'Operation', 'type': 'string'}, 'cache_manager_actor_name': {'title': 'Cache Manager Actor Name', 'type': 'string'}, 'cache_collection_path': {'title': 'Cache Collection Path', 'type': 'string'}, 'sequence_path': {'title': 'Sequence Path', 'type': 'string'}, 'geometry_collection_actor_names': {'anyOf': [{'items': {'type': 'string'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Geometry Collection Actor Names'}, 'sample_times': {'anyOf': [{'items': {'type': 'number'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Sample Times'}, 'scrub_frames': {'anyOf': [{'items': {'type': 'integer'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Scrub Frames'}, 'repeat_count': {'default': 2, 'title': 'Repeat Count', 'type': 'integer'}, 'timeout_seconds': {'default': 10.0, 'title': 'Timeout Seconds', 'type': 'number'}}, 'required': ['operation', 'cache_manager_actor_name', 'cache_collection_path', 'sequence_path'], 'title': 'validate_chaos_cacheArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'record_chaos_cache': {'cache_names': 'null', 'solver_actor_name': '""', 'duration_seconds': '2.0', 'frame_rate': '30.0', 'timeout_seconds': '10.0', 'initial_trigger_mode': '"strain_velocity"', 'motion_velocity': 'null', 'per_advance_breaks_allowed': '8', 'per_advance_breaks_reschedule_limit': '64', 'particle_release_throttling_min_count': '64', 'particle_release_throttling_max_count': '128'}, 'validate_chaos_cache': {'geometry_collection_actor_names': 'null', 'sample_times': 'null', 'scrub_frames': 'null', 'repeat_count': '2', 'timeout_seconds': '10.0'}}
REQUIRED_BY_METHOD = {'record_chaos_cache': ('operation', 'cache_manager_actor_name', 'geometry_collection_actor_names', 'cache_collection_path'), 'validate_chaos_cache': ('operation', 'cache_manager_actor_name', 'cache_collection_path', 'sequence_path')}
ATTACHMENT_LIBRARY = 'CubelessChaosCacheSessionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_chaos_cache_sessions_toolset', 'verify_chaos_cache_sessions_toolset', 'remove_chaos_cache_sessions_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string9_schema_verified', 'integer5_number4_schema_verified', 'string_array3_schema_verified', 'nullable_array5_schema_verified', 'chaos_cache_sessions_public15_9_schema_verified', 'required4_4_default11_5_verified', 'json_value_carrier24_schema_verified')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=2,
        public_input_counts=dict(zip(BINDINGS,(15,9))),physical_input_counts=dict(zip(BINDINGS,(15,9))),native_input_counts=dict(zip(BINDINGS,(1,1))),hidden_input_counts=dict(zip(BINDINGS,(8,2))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=2:raise RuntimeError('Exact ChaosCacheSession Toolset2 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('Exactly two distinct original Chaos cache session methods required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original method unavailable')
        inputs=row.get('inputSchema')
        if not reflected_carriers:
            if not _exact(inputs,LOGICAL_INPUT_SCHEMAS[method]):raise RuntimeError('Exact original eager input schema required; no new length/range/extra-field policy')
        else:
            if type(inputs) is not dict or inputs.get('type')!='object':raise RuntimeError('Physical object required')
            props=inputs.get('properties',{});required=inputs.get('required',[])
            if type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(REQUIRED_BY_METHOD[method],required):raise RuntimeError('Exact physical15/9 fields/required required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(15,9))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered ChaosCacheSession wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered ChaosCacheSession wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
