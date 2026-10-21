"""Editor level session exact original eager logical schemas versus public JSON FString carriers."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import editor_level_session as api
TOOLSET = 'cubeless_mcp_ue58.registration.editor_level_session_toolset.CubelessEditorLevelSessionTools'
BINDINGS = ('open_editor_level', 'get_editor_level_open_status', 'clear_dirty_package_flags', 'safe_new_preview_map')
PUBLIC_FIELDS_BY_METHOD = {'open_editor_level': ('level_path', 'dry_run', 'allow_dirty_packages', 'load_as_template', 'show_progress', 'deferred_load_delay_seconds'), 'get_editor_level_open_status': ('request_id',), 'clear_dirty_package_flags': ('package_paths', 'package_path', 'clear_all_dirty', 'dry_run', 'allow_non_temp_path', 'required_root'), 'safe_new_preview_map': ('target_path', 'map_path', 'dry_run', 'allow_dirty_packages', 'overwrite_existing', 'allow_non_temp_path', 'required_root', 'is_partitioned_world')}
LOGICAL_INPUT_SCHEMAS = {'open_editor_level': {'properties': {'level_path': {'title': 'Level Path', 'type': 'string'}, 'dry_run': {'default': True, 'title': 'Dry Run', 'type': 'boolean'}, 'allow_dirty_packages': {'default': False, 'title': 'Allow Dirty Packages', 'type': 'boolean'}, 'load_as_template': {'default': False, 'title': 'Load As Template', 'type': 'boolean'}, 'show_progress': {'default': True, 'title': 'Show Progress', 'type': 'boolean'}, 'deferred_load_delay_seconds': {'default': 1.0, 'title': 'Deferred Load Delay Seconds', 'type': 'number'}}, 'required': ['level_path'], 'title': 'open_editor_levelArguments', 'type': 'object'}, 'get_editor_level_open_status': {'properties': {'request_id': {'default': '', 'title': 'Request Id', 'type': 'string'}}, 'title': 'get_editor_level_open_statusArguments', 'type': 'object'}, 'clear_dirty_package_flags': {'properties': {'package_paths': {'anyOf': [{'items': {'type': 'string'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Package Paths'}, 'package_path': {'default': '', 'title': 'Package Path', 'type': 'string'}, 'clear_all_dirty': {'default': False, 'title': 'Clear All Dirty', 'type': 'boolean'}, 'dry_run': {'default': True, 'title': 'Dry Run', 'type': 'boolean'}, 'allow_non_temp_path': {'default': False, 'title': 'Allow Non Temp Path', 'type': 'boolean'}, 'required_root': {'default': '/Game/_MCP_Temp', 'title': 'Required Root', 'type': 'string'}}, 'title': 'clear_dirty_package_flagsArguments', 'type': 'object'}, 'safe_new_preview_map': {'properties': {'target_path': {'default': '', 'title': 'Target Path', 'type': 'string'}, 'map_path': {'default': '', 'title': 'Map Path', 'type': 'string'}, 'dry_run': {'default': True, 'title': 'Dry Run', 'type': 'boolean'}, 'allow_dirty_packages': {'default': False, 'title': 'Allow Dirty Packages', 'type': 'boolean'}, 'overwrite_existing': {'default': False, 'title': 'Overwrite Existing', 'type': 'boolean'}, 'allow_non_temp_path': {'default': False, 'title': 'Allow Non Temp Path', 'type': 'boolean'}, 'required_root': {'default': '/Game/_MCP_Temp', 'title': 'Required Root', 'type': 'string'}, 'is_partitioned_world': {'default': False, 'title': 'Is Partitioned World', 'type': 'boolean'}}, 'title': 'safe_new_preview_mapArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'open_editor_level': {'dry_run': 'true', 'allow_dirty_packages': 'false', 'load_as_template': 'false', 'show_progress': 'true', 'deferred_load_delay_seconds': '1.0'}, 'get_editor_level_open_status': {'request_id': '""'}, 'clear_dirty_package_flags': {'package_paths': 'null', 'package_path': '""', 'clear_all_dirty': 'false', 'dry_run': 'true', 'allow_non_temp_path': 'false', 'required_root': '"/Game/_MCP_Temp"'}, 'safe_new_preview_map': {'target_path': '""', 'map_path': '""', 'dry_run': 'true', 'allow_dirty_packages': 'false', 'overwrite_existing': 'false', 'allow_non_temp_path': 'false', 'required_root': '"/Game/_MCP_Temp"', 'is_partitioned_world': 'false'}}
REQUIRED_BY_METHOD = {'open_editor_level': ('level_path',), 'get_editor_level_open_status': (), 'clear_dirty_package_flags': (), 'safe_new_preview_map': ()}
ATTACHMENT_LIBRARY = 'CubelessEditorLevelSessionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_editor_level_session_toolset', 'verify_editor_level_session_toolset', 'remove_editor_level_session_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string7_schema_verified', 'nullable_string_array1_schema_verified', 'boolean12_schema_verified', 'number1_schema_verified', 'editor_level_session_public6_1_6_8_schema_verified', 'required1_0_0_0_default5_1_6_8_verified', 'json_value_carrier21_schema_verified')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=4,
        public_input_counts=dict(zip(BINDINGS,(6,1,6,8))),physical_input_counts=dict(zip(BINDINGS,(6,1,6,8))),native_input_counts=dict(zip(BINDINGS,(1,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(1,0,0,1))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=4:raise RuntimeError('Exact EditorLevelSession Toolset4 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('Exactly four distinct original editor level session methods required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original method unavailable')
        inputs=row.get('inputSchema')
        if not reflected_carriers:
            if not _exact(inputs,LOGICAL_INPUT_SCHEMAS[method]):raise RuntimeError('Exact original eager input schema required; no new length/range/extra-field policy')
        else:
            if type(inputs) is not dict or inputs.get('type')!='object':raise RuntimeError('Physical object required')
            props=inputs.get('properties',{});required=inputs.get('required',[])
            if type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(REQUIRED_BY_METHOD[method],required):raise RuntimeError('Exact physical6/1/6/8 fields/required required')
            for field in PUBLIC_FIELDS_BY_METHOD[method]:
                item=props[field]
                if type(item) is not dict or item.get('type')!='string':raise RuntimeError('Physical JSON FString carrier required')
                if field in REQUIRED_BY_METHOD[method]:
                    if 'default' in item:raise RuntimeError('Required level_path has physical default')
                elif type(item.get('default')) is not str or item['default']!=PHYSICAL_DEFAULTS_BY_METHOD[method][field]:raise RuntimeError('Exact physical JSON default required')
        output=row.get('outputSchema');outer=output.get('properties') if type(output) is dict else None
        if type(output) is not dict or output.get('type')!='object' or type(outer) is not dict or set(outer)!={'returnValue'} or not _required(('returnValue',),output.get('required')):raise RuntimeError('Exact returnValue outer required')
        result=outer['returnValue'];props=result.get('properties') if type(result) is dict else None
        if type(result) is not dict or result.get('type')!='object' or type(props) is not dict or set(props)!=set(RESULT_FIELDS) or not _required(RESULT_FIELDS,result.get('required')) or any(type(props[k]) is not dict or props[k].get('type')!=('boolean' if k=='success' else 'string') for k in RESULT_FIELDS):raise RuntimeError('Exact standard11 output required')
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(6,1,6,8))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered EditorLevelSession wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered EditorLevelSession wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
