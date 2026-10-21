"""Landscape logical public0/1/11/2 versus fixed physical carriers/empty3 projection."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import landscape as api
TOOLSET = 'cubeless_mcp_ue58.registration.landscape_toolset.CubelessLandscapeTools'
BINDINGS = ('list_landscapes', 'get_landscape_info', 'create_landscape', 'set_landscape_material')
PUBLIC_FIELDS_BY_METHOD = {'list_landscapes': (), 'get_landscape_info': ('actor_label',), 'create_landscape': ('actor_label', 'material_path', 'location', 'scale', 'component_count_x', 'component_count_y', 'sections_per_component', 'quads_per_section', 'height_value', 'allow_non_temp_level', 'required_root'), 'set_landscape_material': ('material_path', 'actor_label')}
DEFAULTS_BY_METHOD = {'list_landscapes': {}, 'get_landscape_info': {'actor_label': ''}, 'create_landscape': {'actor_label': 'MCP_Landscape', 'material_path': '', 'location': None, 'scale': None, 'component_count_x': 1, 'component_count_y': 1, 'sections_per_component': 1, 'quads_per_section': 7, 'height_value': 32768, 'allow_non_temp_level': False, 'required_root': '/Game/_MCP_Temp'}, 'set_landscape_material': {'actor_label': ''}}
REQUIRED_BY_METHOD = {'list_landscapes': (), 'get_landscape_info': (), 'create_landscape': (), 'set_landscape_material': ('material_path',)}
PHYSICAL_REQUIRED_BY_METHOD = {'list_landscapes': (), 'get_landscape_info': ('actor_label',), 'create_landscape': ('material_path',), 'set_landscape_material': ('material_path', 'actor_label')}
EMPTY_DEFAULTS_BY_METHOD = {'list_landscapes': (), 'get_landscape_info': ('actor_label',), 'create_landscape': ('material_path',), 'set_landscape_material': ('actor_label',)}
FIELD_KINDS_BY_METHOD = {'list_landscapes': {}, 'get_landscape_info': {'actor_label': 'string'}, 'create_landscape': {'actor_label': 'string', 'material_path': 'string', 'location': ['array', 'null'], 'scale': ['array', 'null'], 'component_count_x': 'integer', 'component_count_y': 'integer', 'sections_per_component': 'integer', 'quads_per_section': 'integer', 'height_value': 'integer', 'allow_non_temp_level': 'boolean', 'required_root': 'string'}, 'set_landscape_material': {'material_path': 'string', 'actor_label': 'string'}}
ATTACHMENT_LIBRARY = 'CubelessLandscapeAttachmentLibrary'
ATTACHMENT_METHODS = ('install_landscape_toolset', 'verify_landscape_toolset', 'remove_landscape_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string_schema_verified', 'nullable_vector2_schema_verified', 'boolean1_schema_verified', 'integer5_schema_verified', 'landscape_public0_1_11_2_schema_verified', 'required0_0_0_1_default0_1_11_1_verified', 'empty_string_default3_projection_verified')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=4,
        public_input_counts=dict(zip(BINDINGS,(0,1,11,2))),physical_input_counts=dict(zip(BINDINGS,(0,1,11,2))),native_input_counts=dict(zip(BINDINGS,(0,2,12,3))),hidden_input_counts=dict.fromkeys(BINDINGS,0),internal_compatibility_input_counts=dict(zip(BINDINGS,(0,1,1,1))),
        schema_verified=False,default_verified=False,raw_guard_required=True,raw_guard_ready=False,nullable_raw_guard_ready=False,boolean_raw_guard_ready=False,integer_raw_guard_ready=False,
        actual_official_wire_verified=False,nullable_Named_wire_verified=False,boolean_Named_wire_verified=False,integer_Named_wire_verified=False,empty_string_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _required(expected,actual):return type(actual) is list and len(actual)==len(expected) and all(type(v) is str for v in actual) and set(actual)==set(expected)

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=4:raise RuntimeError('Exact Landscape Toolset4 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('Exactly four distinct Landscape methods required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original class method unavailable')
        inputs=row.get('inputSchema');fields=inputs.get('properties') if type(inputs) is dict else None
        if reflected_carriers and method==BINDINGS[0] and type(inputs) is dict:
            if 'properties' not in inputs:fields={}
            required=inputs.get('required',[])
        else:required=inputs.get('required') if type(inputs) is dict else None
        expected_required=PHYSICAL_REQUIRED_BY_METHOD[method] if reflected_carriers else REQUIRED_BY_METHOD[method]
        if type(inputs) is not dict or inputs.get('type')!='object' or type(fields) is not dict or set(fields)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(expected_required,required) or not reflected_carriers and inputs.get('additionalProperties') is not False:raise RuntimeError('Landscape exact fields/required/default projection differ')
        for field,kind in FIELD_KINDS_BY_METHOD[method].items():
            item=fields[field];carrier=kind=='integer' or kind==['array','null']
            if type(item) is not dict or item.get('type')!=('string' if reflected_carriers and carrier else kind):raise RuntimeError('Per-field logical/physical Landscape type differs')
            if field in REQUIRED_BY_METHOD[method] or reflected_carriers and field in EMPTY_DEFAULTS_BY_METHOD[method]:
                if 'default' in item:raise RuntimeError('Physical required/empty-default field has default')
            else:
                expected=DEFAULTS_BY_METHOD[method][field]
                if reflected_carriers:expected='null' if kind==['array','null'] else str(expected) if kind=='integer' else expected
                if 'default' not in item or type(item['default']) is not type(expected) or item['default']!=expected:raise RuntimeError('Original Landscape default differs')
            if not reflected_carriers and kind==['array','null'] and item.get('items')!={'type':'number'}:raise RuntimeError('Nullable vector number item schema differs')
        output=row.get('outputSchema');outer=output.get('properties') if type(output) is dict else None
        if type(output) is not dict or output.get('type')!='object' or type(outer) is not dict or set(outer)!={'returnValue'} or not _required(('returnValue',),output.get('required')):raise RuntimeError('Exact returnValue outer required')
        result=outer['returnValue'];props=result.get('properties') if type(result) is dict else None
        if type(result) is not dict or result.get('type')!='object' or type(props) is not dict or set(props)!=set(RESULT_FIELDS) or not _required(RESULT_FIELDS,result.get('required')) or any(type(props[k]) is not dict or props[k].get('type')!=('boolean' if k=='success' else 'string') for k in RESULT_FIELDS):raise RuntimeError('Exact standard11 output required')
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,empty_string_default3_projection_verified=not reflected_carriers,public_input_counts=dict(zip(BINDINGS,(0,1,11,2))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered Landscape wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered Landscape wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
