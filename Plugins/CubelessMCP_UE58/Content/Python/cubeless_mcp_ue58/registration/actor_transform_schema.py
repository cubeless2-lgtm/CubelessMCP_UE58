"""Exact original transform4 logical boundary over the retained Actor Toolset."""
import json
import unreal

TOOLSET = 'cubeless_mcp_ue58.registration.actor_toolset.CubelessActorTools'
METHOD = 'set_actor_transform_original'
OLD_METHODS = ('get_actors_in_level','find_actors_by_name','get_actor_properties','spawn_actor','create_actor','spawn_blueprint_actor','delete_actor','set_actor_transform')
PHYSICAL_FIELDS = ('name','location_json','rotation_json','scale_json')
LOGICAL_INPUT_SCHEMA = {'type': 'object', 'title': 'set_actor_transform_originalArguments', 'properties': {'name': {'title': 'Name', 'type': 'string'}, 'location': {'title': 'Location', 'anyOf': [{'type': 'array', 'items': {'type': 'number'}}, {'type': 'null'}], 'default': None}, 'rotation': {'title': 'Rotation', 'anyOf': [{'type': 'array', 'items': {'type': 'number'}}, {'type': 'null'}], 'default': None}, 'scale': {'title': 'Scale', 'anyOf': [{'type': 'array', 'items': {'type': 'number'}}, {'type': 'null'}], 'default': None}}, 'required': ['name']}
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'existing8_schema_preserved', 'exact4_nullable_arrays_verified', 'original_object_output_verified')

def _invalid_constant(value):
    raise ValueError('Non-finite JSON is not supported: '+value)

def _failure(message):
    # Pre-dispatch receipt only; never manufacture an original mutation result.
    return dict(success=False,stage='actor_transform_registration_admission',error_code='actor_transform_raw_guard_admission_missing',error=str(message),data=dict(native_dispatched=False,mutation_attempted=False,change_state_known=True,changed=False))

def _select_payload(result):
    data=result.get('data')
    if type(data) is not dict or type(data.get('legacy_result')) is not dict:
        return result
    legacy=data['legacy_result'];native=data.get('native_outcome')
    observations=[result,data]+([native] if type(native) is dict else [])
    if type(result.get('changes')) is list:
        observations.extend(row for row in result['changes'] if type(row) is dict)
    unknown=any(row.get('change_state_known') is False or row.get('mutation_state_unknown') is True for row in observations)
    if unknown or (result.get('success') is not True and legacy.get('success') is not False):
        return result
    return legacy

def invoke_carriers(toolset_class, operation, name, location_json, rotation_json, scale_json):
    helper = None
    try:
        helper = getattr(getattr(unreal,'CubelessActorTransformAttachmentLibrary',None),'verify_actor_transform_toolset',None)
        if not callable(helper):
            raise RuntimeError('Exact original transform raw admission helper unavailable')
        report = json.loads(helper(toolset_class))
        if (type(report) is not dict or report.get('operation')!='verify_actor_transform_toolset'
                or report.get('delegated_call_admitted') is not True or report.get('delegated_admission_only') is not True
                or report.get('delegated_method')!=METHOD or report.get('asset_mutation') is not False or report.get('metadata_mutation') is not False):
            raise RuntimeError('Only the current original-transform raw Named delegation is admitted')
        values=[]
        for carrier in (location_json,rotation_json,scale_json):
            if type(carrier) is not str or len(carrier.encode('utf-16-le'))//2>1048576 or '\x00' in carrier:
                raise ValueError('Bounded private JSON carrier required')
            value=json.loads(carrier,parse_constant=_invalid_constant)
            if value is not None and (type(value) is not list or any(type(item) not in (int,float) for item in value)):
                raise ValueError('Nullable number array carrier required')
            values.append(value)
    except Exception as error:
        return json.dumps(_failure(error),ensure_ascii=False,allow_nan=False,separators=(',',':'))
    finally:
        helper=None
    # The operation owns journaling, Native dispatch and failure/unknown state.
    # Do not run it twice or wrap post-dispatch errors as a known admission refusal.
    result=operation(name,*values)
    if type(result) is not dict:
        raise RuntimeError('Original transform operation receipt unavailable; do not replay')
    payload=_select_payload(result)
    if type(payload) is not dict:
        raise RuntimeError('Actual original transform result must be an object; do not replay')
    return json.dumps(payload,ensure_ascii=False,allow_nan=False,separators=(',',':'))

def binding_status(toolset_class):
    library=getattr(unreal,'CubelessActorLibrary',None)
    helper=getattr(library,'set_actor_transform_original',None)
    return dict(ready=False,bindings_ready=callable(helper) and callable(getattr(toolset_class,METHOD,None)),public_toolset=TOOLSET,public_tool_count=1,actual_official_wire_verified=False)

def _rows(schema):
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=9:
        raise RuntimeError('Exact existing Actor8 plus original transform1 schema required')
    rows={}
    for row in schema['tools']:
        if type(row) is not dict or type(row.get('name')) is not str or not row['name'].startswith(TOOLSET+'.'):
            raise RuntimeError('Exact owned tool schema row required')
        method=row['name'][len(TOOLSET)+1:]
        if method not in OLD_METHODS+(METHOD,) or method in rows:
            raise RuntimeError('Unexpected or duplicate Actor method')
        rows[method]=row
    return rows

def validate_reflected_carrier_schema(schema,toolset_class):
    rows=_rows(schema);row=rows[METHOD];inputs=row.get('inputSchema',{});props=inputs.get('properties',{})
    if inputs.get('type')!='object' or set(props)!=set(PHYSICAL_FIELDS) or inputs.get('required')!=['name']:
        raise RuntimeError('Exact physical transform4/required1 required')
    for name in PHYSICAL_FIELDS:
        field=props[name]
        if type(field) is not dict or field.get('type')!='string' or (('default' in field) if name=='name' else field.get('default')!='null'):
            raise RuntimeError('Exact physical transform FString/defaults required')
    output=row.get('outputSchema',{})
    if output.get('type')!='object' or output.get('required')!=['returnValue'] or set(output.get('properties',{}))!={'returnValue'} or output['properties']['returnValue'].get('type')!='string':
        raise RuntimeError('Exact physical FString output required')
    if not callable(getattr(toolset_class,METHOD,None)):
        raise RuntimeError('Original transform method unavailable')
    return dict(reflected_carrier_schema_verified=True)

def validate_schema(schema,toolset_class):
    row=_rows(schema)[METHOD]
    if row.get('inputSchema')!=LOGICAL_INPUT_SCHEMA or row.get('outputSchema',{}).get('type')!='object':
        raise RuntimeError('Original4 nullable-array/default-null/object schema required')
    return dict(schema_verified=True,default_verified=True,logical_adapter_schema_verified=True)

def validate_registered_metadata(class_schema,report,toolset_class):
    validate_reflected_carrier_schema(class_schema,toolset_class)
    if type(report) is not dict or not all(report.get(key) is True for key in ATTACHMENT_READY_FIELDS) or report.get('logical_schema_registry_identity_verified') is not True or report.get('logical_schema_source')!='actual_registered_wrapper_GetJsonSchema':
        raise RuntimeError('Exact registered original transform wrapper receipt required')
    logical=json.loads(report['logical_schema_json'])
    validation=validate_schema(logical,toolset_class)
    physical_rows=_rows(class_schema);logical_rows=_rows(logical)
    if any(physical_rows[name]!=logical_rows[name] for name in OLD_METHODS):
        raise RuntimeError('Existing Actor8 schema was changed by projection')
    return dict(validation,reflected_carrier_schema_verified=True,existing8_schema_preserved=True)
