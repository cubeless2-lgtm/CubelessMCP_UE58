"""Viewport exact original eager logical schemas versus public JSON FString carriers."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import viewport_capture as api
TOOLSET = 'cubeless_mcp_ue58.registration.viewport_capture_toolset.CubelessViewportCaptureTools'
BINDINGS = ('list_viewport_bookmarks', 'set_active_viewport_realtime', 'capture_viewport_bookmark_screenshot', 'take_screenshot')
PUBLIC_FIELDS_BY_METHOD = {'list_viewport_bookmarks': (), 'set_active_viewport_realtime': ('realtime', 'realtime_frame_count'), 'capture_viewport_bookmark_screenshot': ('filepath', 'bookmark_index', 'redraw_count', 'clean_game_view', 'view_location', 'view_rotation', 'view_fov', 'restore_view_after_capture'), 'take_screenshot': ('filepath',)}
LOGICAL_INPUT_SCHEMAS = {'list_viewport_bookmarks': {'properties': {}, 'title': 'list_viewport_bookmarksArguments', 'type': 'object'}, 'set_active_viewport_realtime': {'properties': {'realtime': {'default': True, 'title': 'Realtime', 'type': 'boolean'}, 'realtime_frame_count': {'default': 300, 'title': 'Realtime Frame Count', 'type': 'integer'}}, 'title': 'set_active_viewport_realtimeArguments', 'type': 'object'}, 'capture_viewport_bookmark_screenshot': {'properties': {'filepath': {'title': 'Filepath', 'type': 'string'}, 'bookmark_index': {'default': -1, 'title': 'Bookmark Index', 'type': 'integer'}, 'redraw_count': {'default': 2, 'title': 'Redraw Count', 'type': 'integer'}, 'clean_game_view': {'default': False, 'title': 'Clean Game View', 'type': 'boolean'}, 'view_location': {'anyOf': [{'items': {'type': 'number'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'View Location'}, 'view_rotation': {'anyOf': [{'items': {'type': 'number'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'View Rotation'}, 'view_fov': {'anyOf': [{'type': 'number'}, {'type': 'null'}], 'default': None, 'title': 'View Fov'}, 'restore_view_after_capture': {'default': True, 'title': 'Restore View After Capture', 'type': 'boolean'}}, 'required': ['filepath'], 'title': 'capture_viewport_bookmark_screenshotArguments', 'type': 'object'}, 'take_screenshot': {'properties': {'filepath': {'title': 'Filepath', 'type': 'string'}}, 'required': ['filepath'], 'title': 'take_screenshotArguments', 'type': 'object'}}
PHYSICAL_DEFAULTS_BY_METHOD = {'list_viewport_bookmarks': {}, 'set_active_viewport_realtime': {'realtime': 'true', 'realtime_frame_count': '300'}, 'capture_viewport_bookmark_screenshot': {'bookmark_index': '-1', 'redraw_count': '2', 'clean_game_view': 'false', 'view_location': 'null', 'view_rotation': 'null', 'view_fov': 'null', 'restore_view_after_capture': 'true'}, 'take_screenshot': {}}
REQUIRED_BY_METHOD = {'list_viewport_bookmarks': (), 'set_active_viewport_realtime': (), 'capture_viewport_bookmark_screenshot': ('filepath',), 'take_screenshot': ('filepath',)}
ATTACHMENT_LIBRARY = 'CubelessViewportCaptureAttachmentLibrary'
ATTACHMENT_METHODS = ('install_viewport_capture_toolset', 'verify_viewport_capture_toolset', 'remove_viewport_capture_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string2_schema_verified', 'nullable_vector2_schema_verified', 'nullable_number1_schema_verified', 'boolean3_schema_verified', 'integer3_schema_verified', 'viewport_capture_public0_2_8_1_schema_verified', 'required0_0_1_1_default0_2_7_0_verified')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=4,
        public_input_counts=dict(zip(BINDINGS,(0,2,8,1))),physical_input_counts=dict(zip(BINDINGS,(0,2,8,1))),native_input_counts=dict(zip(BINDINGS,(0,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,1,0))),
        schema_verified=False,default_verified=False,raw_guard_required=True,raw_guard_ready=False,nullable_raw_guard_ready=False,boolean_raw_guard_ready=False,integer_raw_guard_ready=False,
        actual_official_wire_verified=False,nullable_Named_wire_verified=False,boolean_Named_wire_verified=False,integer_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _required(expected,actual):return type(actual) is list and len(actual)==len(expected) and all(type(v) is str for v in actual) and set(actual)==set(expected)

def _exact(a,b):
    if type(a) is not type(b):return False
    if type(a) is dict:return set(a)==set(b) and all(_exact(a[k],b[k]) for k in a)
    if type(a) is list:return len(a)==len(b) and all(_exact(x,y) for x,y in zip(a,b))
    return a==b

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=4:raise RuntimeError('Exact ViewportCapture Toolset4 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('Exactly four distinct original viewport methods required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original method unavailable')
        inputs=row.get('inputSchema')
        if not reflected_carriers:
            if not _exact(inputs,LOGICAL_INPUT_SCHEMAS[method]):raise RuntimeError('Exact original eager input schema required; no new length/range/extra-field policy')
        else:
            if type(inputs) is not dict or inputs.get('type')!='object':raise RuntimeError('Physical object required')
            props=inputs.get('properties',{});required=inputs.get('required',[])
            if type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(REQUIRED_BY_METHOD[method],required):raise RuntimeError('Exact physical0/2/8/1 fields/required required')
            for field in PUBLIC_FIELDS_BY_METHOD[method]:
                item=props[field]
                if type(item) is not dict or item.get('type')!='string':raise RuntimeError('Physical JSON FString carrier required')
                if field in REQUIRED_BY_METHOD[method]:
                    if 'default' in item:raise RuntimeError('Required filepath has physical default')
                elif type(item.get('default')) is not str or item['default']!=PHYSICAL_DEFAULTS_BY_METHOD[method][field]:raise RuntimeError('Exact physical JSON default required')
        output=row.get('outputSchema');outer=output.get('properties') if type(output) is dict else None
        if type(output) is not dict or output.get('type')!='object' or type(outer) is not dict or set(outer)!={'returnValue'} or not _required(('returnValue',),output.get('required')):raise RuntimeError('Exact returnValue outer required')
        result=outer['returnValue'];props=result.get('properties') if type(result) is dict else None
        if type(result) is not dict or result.get('type')!='object' or type(props) is not dict or set(props)!=set(RESULT_FIELDS) or not _required(RESULT_FIELDS,result.get('required')) or any(type(props[k]) is not dict or props[k].get('type')!=('boolean' if k=='success' else 'string') for k in RESULT_FIELDS):raise RuntimeError('Exact standard11 output required')
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(0,2,8,1))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered ViewportCapture wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered ViewportCapture wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
