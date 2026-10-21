"""Viewport exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import chaos_cinematic_sequence as api
TOOLSET = 'cubeless_mcp_ue58.registration.chaos_cinematic_sequence_toolset.CubelessChaosCinematicSequenceTools'
BINDINGS = ('create_chaos_level_sequence', 'bind_chaos_cache_to_sequence', 'configure_chaos_sequence_timing', 'inspect_chaos_cinematic_pipeline')
PUBLIC_FIELDS_BY_METHOD = {'create_chaos_level_sequence': ('output_sequence_path', 'camera_actor_name', 'display_rate', 'tick_resolution', 'start_frame', 'end_frame', 'overwrite_existing'), 'bind_chaos_cache_to_sequence': ('sequence_path', 'cache_manager_actor_name', 'cache_collection_path', 'section_start_frame', 'section_end_frame', 'cache_play_rate', 'cache_start_offset_seconds'), 'configure_chaos_sequence_timing': ('sequence_path', 'display_rate', 'tick_resolution', 'start_frame', 'end_frame', 'section_start_frame', 'section_end_frame', 'cache_play_rate', 'cache_start_offset_seconds', 'reverse'), 'inspect_chaos_cinematic_pipeline': ('geometry_collection_path', 'sequence_path', 'cache_manager_actor_name', 'map_path', 'chaos_state_sha256', 'field_actor_name', 'require_movie_render_pipeline', 'mrq_output_directory', 'mrq_manifest_path', 'expected_display_rate', 'expected_tick_resolution', 'expected_playback_start_frame', 'expected_playback_end_frame')}
LOGICAL_INPUT_SCHEMAS = {'create_chaos_level_sequence': {'properties': {'output_sequence_path': {'title': 'Output Sequence Path', 'type': 'string'}, 'camera_actor_name': {'default': '', 'title': 'Camera Actor Name', 'type': 'string'}, 'display_rate': {'default': 30.0, 'title': 'Display Rate', 'type': 'number'}, 'tick_resolution': {'default': 24000, 'title': 'Tick Resolution', 'type': 'integer'}, 'start_frame': {'default': 0, 'title': 'Start Frame', 'type': 'integer'}, 'end_frame': {'default': 60, 'title': 'End Frame', 'type': 'integer'}, 'overwrite_existing': {'default': False, 'title': 'Overwrite Existing', 'type': 'boolean'}}, 'required': ['output_sequence_path'], 'title': 'create_chaos_level_sequenceArguments', 'type': 'object'}, 'bind_chaos_cache_to_sequence': {'properties': {'sequence_path': {'title': 'Sequence Path', 'type': 'string'}, 'cache_manager_actor_name': {'title': 'Cache Manager Actor Name', 'type': 'string'}, 'cache_collection_path': {'title': 'Cache Collection Path', 'type': 'string'}, 'section_start_frame': {'default': 0, 'title': 'Section Start Frame', 'type': 'integer'}, 'section_end_frame': {'default': 60, 'title': 'Section End Frame', 'type': 'integer'}, 'cache_play_rate': {'default': 1.0, 'title': 'Cache Play Rate', 'type': 'number'}, 'cache_start_offset_seconds': {'default': 0.0, 'title': 'Cache Start Offset Seconds', 'type': 'number'}}, 'required': ['sequence_path', 'cache_manager_actor_name', 'cache_collection_path'], 'title': 'bind_chaos_cache_to_sequenceArguments', 'type': 'object'}, 'configure_chaos_sequence_timing': {'properties': {'sequence_path': {'title': 'Sequence Path', 'type': 'string'}, 'display_rate': {'default': 30.0, 'title': 'Display Rate', 'type': 'number'}, 'tick_resolution': {'default': 24000, 'title': 'Tick Resolution', 'type': 'integer'}, 'start_frame': {'default': 0, 'title': 'Start Frame', 'type': 'integer'}, 'end_frame': {'default': 60, 'title': 'End Frame', 'type': 'integer'}, 'section_start_frame': {'default': 0, 'title': 'Section Start Frame', 'type': 'integer'}, 'section_end_frame': {'default': 60, 'title': 'Section End Frame', 'type': 'integer'}, 'cache_play_rate': {'default': 1.0, 'title': 'Cache Play Rate', 'type': 'number'}, 'cache_start_offset_seconds': {'default': 0.0, 'title': 'Cache Start Offset Seconds', 'type': 'number'}, 'reverse': {'default': False, 'title': 'Reverse', 'type': 'boolean'}}, 'required': ['sequence_path'], 'title': 'configure_chaos_sequence_timingArguments', 'type': 'object'}, 'inspect_chaos_cinematic_pipeline': {'properties': {'geometry_collection_path': {'title': 'Geometry Collection Path', 'type': 'string'}, 'sequence_path': {'title': 'Sequence Path', 'type': 'string'}, 'cache_manager_actor_name': {'title': 'Cache Manager Actor Name', 'type': 'string'}, 'map_path': {'title': 'Map Path', 'type': 'string'}, 'chaos_state_sha256': {'title': 'Chaos State Sha256', 'type': 'string'}, 'field_actor_name': {'default': 'Chaos_AnchorField', 'title': 'Field Actor Name', 'type': 'string'}, 'require_movie_render_pipeline': {'default': True, 'title': 'Require Movie Render Pipeline', 'type': 'boolean'}, 'mrq_output_directory': {'default': '', 'title': 'Mrq Output Directory', 'type': 'string'}, 'mrq_manifest_path': {'default': '', 'title': 'Mrq Manifest Path', 'type': 'string'}, 'expected_display_rate': {'default': 30, 'title': 'Expected Display Rate', 'type': 'integer'}, 'expected_tick_resolution': {'default': 24000, 'title': 'Expected Tick Resolution', 'type': 'integer'}, 'expected_playback_start_frame': {'default': 0, 'title': 'Expected Playback Start Frame', 'type': 'integer'}, 'expected_playback_end_frame': {'default': 60, 'title': 'Expected Playback End Frame', 'type': 'integer'}}, 'required': ['geometry_collection_path', 'sequence_path', 'cache_manager_actor_name', 'map_path', 'chaos_state_sha256'], 'title': 'inspect_chaos_cinematic_pipelineArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'create_chaos_level_sequence': {'camera_actor_name': '""', 'display_rate': '30.0', 'tick_resolution': '24000', 'start_frame': '0', 'end_frame': '60', 'overwrite_existing': 'false'}, 'bind_chaos_cache_to_sequence': {'section_start_frame': '0', 'section_end_frame': '60', 'cache_play_rate': '1.0', 'cache_start_offset_seconds': '0.0'}, 'configure_chaos_sequence_timing': {'display_rate': '30.0', 'tick_resolution': '24000', 'start_frame': '0', 'end_frame': '60', 'section_start_frame': '0', 'section_end_frame': '60', 'cache_play_rate': '1.0', 'cache_start_offset_seconds': '0.0', 'reverse': 'false'}, 'inspect_chaos_cinematic_pipeline': {'field_actor_name': '"Chaos_AnchorField"', 'require_movie_render_pipeline': 'true', 'mrq_output_directory': '""', 'mrq_manifest_path': '""', 'expected_display_rate': '30', 'expected_tick_resolution': '24000', 'expected_playback_start_frame': '0', 'expected_playback_end_frame': '60'}}
REQUIRED_BY_METHOD = {'create_chaos_level_sequence': ('output_sequence_path',), 'bind_chaos_cache_to_sequence': ('sequence_path', 'cache_manager_actor_name', 'cache_collection_path'), 'configure_chaos_sequence_timing': ('sequence_path',), 'inspect_chaos_cinematic_pipeline': ('geometry_collection_path', 'sequence_path', 'cache_manager_actor_name', 'map_path', 'chaos_state_sha256')}
ATTACHMENT_LIBRARY = 'CubelessChaosCinematicSequenceAttachmentLibrary'
ATTACHMENT_METHODS = ('install_chaos_cinematic_sequence_toolset', 'verify_chaos_cinematic_sequence_toolset', 'remove_chaos_cinematic_sequence_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string14_schema_verified', 'integer14_schema_verified', 'boolean3_schema_verified', 'number6_schema_verified', 'chaos_cinematic_sequence_public7_7_10_13_schema_verified', 'required1_3_1_5_default6_4_9_8_verified', 'json_value_carrier37_schema_verified')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=4,
        public_input_counts=dict(zip(BINDINGS,(7,7,10,13))),physical_input_counts=dict(zip(BINDINGS,(7,7,10,13))),native_input_counts=dict(zip(BINDINGS,(1,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,2,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=4:raise RuntimeError('Exact ChaosCinematicSequence Toolset4 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('Exactly four distinct original Chaos cinematic methods required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original method unavailable')
        inputs=row.get('inputSchema')
        if not reflected_carriers:
            if not _exact(inputs,LOGICAL_INPUT_SCHEMAS[method]):raise RuntimeError('Exact original eager input schema required; no new length/range/extra-field policy')
        else:
            if type(inputs) is not dict or inputs.get('type')!='object':raise RuntimeError('Physical object required')
            props=inputs.get('properties',{});required=inputs.get('required',[])
            if type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(REQUIRED_BY_METHOD[method],required):raise RuntimeError('Exact physical7/7/10/13 fields/required required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(7,7,10,13))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered ChaosCinematicSequence wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered ChaosCinematicSequence wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
