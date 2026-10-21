"""Original public spline10, logical points array and private FString carrier1."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import spline_component_points as api
TOOLSET = 'cubeless_mcp_ue58.registration.spline_component_points_toolset.CubelessSplineComponentPointsTools'
METHOD = 'set_spline_component_points'
BINDINGS = ('set_spline_component_points',)
PUBLIC_FIELDS_BY_METHOD = {'set_spline_component_points': ('points', 'actor_label', 'actor_name', 'actor_tag', 'actor_label_prefix', 'component_name', 'component_tag', 'selected_only', 'closed_loop', 'native_first')}
DEFAULTS_BY_METHOD = {'set_spline_component_points': {'actor_label': '', 'actor_name': '', 'actor_tag': '', 'actor_label_prefix': '', 'component_name': 'Road_SourceSpline', 'component_tag': '', 'selected_only': False, 'closed_loop': False, 'native_first': True}}
REQUIRED_BY_METHOD = {'set_spline_component_points': ('points',)}
STRING_FIELDS = ('actor_label', 'actor_name', 'actor_tag', 'actor_label_prefix', 'component_name', 'component_tag')
ARRAY_FIELDS = ('points',)
BOOLEAN_FIELDS = ('selected_only', 'closed_loop', 'native_first')
FIELD_KINDS_BY_METHOD = {'set_spline_component_points': {'points': 'array', 'actor_label': 'string', 'actor_name': 'string', 'actor_tag': 'string', 'actor_label_prefix': 'string', 'component_name': 'string', 'component_tag': 'string', 'selected_only': 'boolean', 'closed_loop': 'boolean', 'native_first': 'boolean'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
ATTACHMENT_LIBRARY = 'CubelessSplineComponentPointsAttachmentLibrary'
ATTACHMENT_METHODS = ('install_spline_component_points_toolset', 'verify_spline_component_points_toolset', 'remove_spline_component_points_toolset')
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string6_schema_verified', 'points_array_schema_verified', 'boolean3_schema_verified', 'spline_points10_schema_verified', 'required1_default9_schema_verified')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,
        public_toolset=TOOLSET,public_tool_count=1,public_input_counts={METHOD:10},physical_input_counts={METHOD:10},hidden_input_counts={METHOD:10},Native_forwarded_public_input_count=9,array_json_carrier_count=1,native_signature_json_carrier_count=2,
        schema_verified=False,default_verified=False,raw_guard_required=True,raw_guard_ready=False,boolean_raw_guard_ready=False,actual_official_wire_verified=False,boolean_Named_wire_verified=False,
        points_Named_wire_verified=False,empty_string_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,adapter_noPIE_idle_gate=True,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _required(expected,actual):
    return type(actual) is list and len(actual)==len(expected) and all(type(v) is str for v in actual) and set(actual)==set(expected)

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=1 or not callable(getattr(toolset_class,METHOD,None)):raise RuntimeError('Exact canonical spline Toolset1 required')
    row=schema['tools'][0]
    if type(row) is not dict or row.get('name')!=TOOLSET+'.'+METHOD:raise RuntimeError('Exact spline method required')
    inputs=row.get('inputSchema');fields=inputs.get('properties') if type(inputs) is dict else None
    if type(inputs) is not dict or inputs.get('type')!='object' or type(fields) is not dict or set(fields)!=set(PUBLIC_FIELDS_BY_METHOD[METHOD]) or not _required(('points','actor_label','actor_name','actor_tag','actor_label_prefix','component_tag') if reflected_carriers else REQUIRED_BY_METHOD[METHOD],inputs.get('required')) or not reflected_carriers and inputs.get('additionalProperties') is not False:raise RuntimeError('Exact public10/required points1 schema differs')
    for field,kind in FIELD_KINDS_BY_METHOD[METHOD].items():
        item=fields[field]
        if type(item) is not dict or item.get('type')!=('string' if reflected_carriers and field=='points' else kind):raise RuntimeError('Original type/private carrier differs: '+field)
        if field=='points':
            if 'default' in item:raise RuntimeError('Required points must not have a default')
            if not reflected_carriers and (item.get('items')!={} or type(item.get('minItems')) is not int or item.get('minItems')!=2):raise RuntimeError('Original points array/min2 differs')
        else:
            expected=DEFAULTS_BY_METHOD[METHOD][field]
            if reflected_carriers and expected=='':
                if 'default' in item:raise RuntimeError('Physical empty-string default must be absent: '+field)
                continue
            if 'default' not in item or type(item['default']) is not type(expected) or item['default']!=expected:raise RuntimeError('Original default differs: '+field)
    output = row.get('outputSchema'); outer = output.get('properties') if type(output) is dict else None
    if (type(output) is not dict or output.get('type') != 'object' or type(outer) is not dict or set(outer) != {'returnValue'}
            or not _required(('returnValue',), output.get('required'))):
        raise RuntimeError('Exact returnValue outer output envelope is required')
    result = outer['returnValue']; result_fields = result.get('properties') if type(result) is dict else None
    if (type(result) is not dict or result.get('type') != 'object' or type(result_fields) is not dict or set(result_fields) != set(RESULT_FIELDS)
            or not _required(RESULT_FIELDS, result.get('required'))
            or any(type(result_fields[key]) is not dict or result_fields[key].get('type') != ('boolean' if key == 'success' else 'string') for key in RESULT_FIELDS)):
        raise RuntimeError('Exact synchronous standard11 output fields/types are required')

    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,
        public_input_counts={METHOD:10},array_json_carrier_count=1,native_signature_json_carrier_count=2,required_source_count=1,default_source_count=9,actual_official_wire_verified=False,boolean_Named_wire_verified=False,
        points_Named_wire_verified=False,empty_string_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,unique_external_pipeline_complete=False)

def validate_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class)


def validate_reflected_carrier_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class, reflected_carriers=True)


def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict or attachment_report.get('operation') not in ATTACHMENT_METHODS[:2]
            or not all(attachment_report.get(key) is True for key in ATTACHMENT_READY_FIELDS)
            or attachment_report.get('asset_mutation') is not False or attachment_report.get('metadata_mutation') is not False
            or attachment_report.get('actual_named_wire_verified') is not False
            or attachment_report.get('logical_schema_source') != 'actual_registered_wrapper_GetJsonSchema'
            or attachment_report.get('logical_schema_registry_identity_verified') is not True):
        raise RuntimeError('Exact ready owner receipt with actual registered Spline component points wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered Spline component points wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
