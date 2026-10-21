"""CN15 unchanged plus original CO fixture13/inspector3 raw schema."""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import skeletal_constraint as api
from . import raw_skeletal_constraint as raw
from . import skeletal_physics_raw_schema as physics
TOOLSET_NAME=physics.TOOLSET_NAME
BINDINGS=raw.METHODS
FIELDS_BY_METHOD=raw.PUBLIC_FIELDS_BY_METHOD
DEFAULTS_BY_METHOD=raw.DEFAULTS_BY_METHOD
NUMBER_FIELDS=('swing1_limit_degrees','swing2_limit_degrees','twist_limit_degrees')
BOOL_FIELDS=('disable_collision','simulate_all_bodies','dry_run')

def _row(name):
    fields=FIELDS_BY_METHOD[name];defaults=DEFAULTS_BY_METHOD[name];props={}
    for key in fields:
        prop=({'type':['array','null'],'items':{'type':'number'}} if key=='location' else
            {'type':'number'} if key in NUMBER_FIELDS else {'type':'boolean'} if key in BOOL_FIELDS else {'type':'string'})
        if key in defaults:prop['default']=defaults[key]
        props[key]=prop
    result={'type':'object','properties':{key:{'type':'boolean' if key=='success' else 'string'} for key in raw.RESULT_FIELDS},
        'required':list(raw.RESULT_FIELDS),'additionalProperties':False}
    return {'name':TOOLSET_NAME+'.'+name,
        'description':'Original native transient skeletal constraint fixture and point inspector contracts. World selectors remain literal; named calls can target original active play contexts while shared Batch retains Editor-idle admission. Actual component/bone/force/angle/simulation and partial cleanup receipts remain authoritative, without asset save/compile, play lifecycle control, stable simulation/cold persistence or full original variant claims.',
        'inputSchema':{'type':'object','properties':props,'required':[k for k in fields if k not in defaults],'additionalProperties':False},
        'outputSchema':{'type':'object','properties':{'returnValue':result},'required':['returnValue'],'additionalProperties':False}}

def public_schema():
    schema=copy.deepcopy(physics.public_schema());schema['tools'].extend(_row(name) for name in BINDINGS);return schema

def schema_json():return json.dumps(public_schema(),ensure_ascii=False,allow_nan=False,separators=(',',':'))

def binding_status():
    methods={**physics.api.binding_status(),**api.binding_status()};expected=tuple(physics.BINDINGS)+tuple(BINDINGS)
    ready=set(methods)==set(expected) and all(methods.values())
    return {'ready':ready,'bindings_ready':ready,'methods':methods,'library':api.LIBRARY,'public_toolset_name':TOOLSET_NAME,
        'raw_toolset_registration_required':True,'preparation_required':False,'registration_verified':False,'schema_verified':False,
        'actual_call_packing_verified':False,'behavior_verified':False,'runtime_physics_verified':False,'registered_method_count':3,
        'lifecycle_controls_added':False,'inspector_force_or_pose_stability_verified':False}

def validate_schema(schema):
    expected=public_schema()
    if type(schema) is not dict or schema.get('name')!=TOOLSET_NAME or type(schema.get('tools')) is not list or len(schema['tools'])!=3:raise RuntimeError('Exact three-method PhysicsAsset/fixture/inspector raw schema required')
    for row,e in zip(schema['tools'],expected['tools']):
        if row.get('name')!=e['name'] or row.get('inputSchema')!=e['inputSchema'] or row.get('outputSchema')!=e['outputSchema']:raise RuntimeError('Original15/13/3 fields/defaults/types/typed output changed')
    return {'schema_verified':True,'validated_tool_names':list(physics.BINDINGS)+list(BINDINGS),
        'validated_input_counts':{physics.BINDINGS[0]:15,BINDINGS[0]:13,BINDINGS[1]:3},'preparation_required':False,
        'actual_call_packing_verified':False,'behavior_verified':False,'runtime_physics_verified':False}
