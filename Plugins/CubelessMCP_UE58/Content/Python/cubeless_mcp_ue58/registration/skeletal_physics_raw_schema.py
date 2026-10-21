"""Original PhysicsAsset15 raw Registry schema; no reflected defaults/preparation."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import skeletal_physics as api
from . import raw_skeletal_physics as raw
TOOLSET_NAME='CubelessMCPUE58Editor.CubelessSkeletalPhysicsTools'
BINDINGS=(raw.METHOD,)
FIELDS=raw.PUBLIC_FIELDS
DEFAULTS=raw.DEFAULTS
NUMBER_FIELDS=('swing1_limit_degrees','swing2_limit_degrees','twist_limit_degrees','linear_damping','angular_damping','tip_mass_scale')
BOOL_FIELDS=('root_kinematic','disable_collision','overwrite_existing','save','dry_run')

def public_schema():
    props={}
    for key in FIELDS:
        field=({'type':['array','null'],'items':{'type':'string'}} if key=='expected_bones' else
            {'type':'number'} if key in NUMBER_FIELDS else {'type':'boolean'} if key in BOOL_FIELDS else {'type':'string'})
        if key in DEFAULTS:field['default']=DEFAULTS[key]
        props[key]=field
    output={'type':'object','properties':{key:{'type':'boolean' if key=='success' else 'string'} for key in raw.RESULT_FIELDS},
        'required':list(raw.RESULT_FIELDS),'additionalProperties':False}
    return {'name':TOOLSET_NAME,'tools':[{'name':TOOLSET_NAME+'.'+raw.METHOD,
        'description':'Original fifteen-input PhysicsAsset authoring/dry preflight on a finite native SkeletalMesh and fresh project target. Matching defaults, actual generation/body/constraint/source/save and original verifier receipts remain authoritative; no mesh assignment, compile, retry, arbitrary class or whole runtime/fixture parity.',
        'inputSchema':{'type':'object','properties':props,'required':list(FIELDS[:2]),'additionalProperties':False},
        'outputSchema':{'type':'object','properties':{'returnValue':output},'required':['returnValue'],'additionalProperties':False}}]}

def schema_json():return json.dumps(public_schema(),ensure_ascii=False,allow_nan=False,separators=(',',':'))

def binding_status():
    methods=api.binding_status();ready=set(methods)==set(BINDINGS) and all(methods.values())
    return {'ready':ready,'bindings_ready':ready,'methods':methods,'library':api.LIBRARY,'public_toolset_name':TOOLSET_NAME,
        'raw_toolset_registration_required':True,'preparation_required':False,'registration_verified':False,'schema_verified':False,
        'actual_call_packing_verified':False,'behavior_verified':False,'runtime_physics_verified':False,
        'registered_method_count':1,'future_fixture_or_inspector_registered':False}

def validate_schema(schema):
    expected=public_schema()
    if type(schema) is not dict or schema.get('name')!=TOOLSET_NAME or type(schema.get('tools')) is not list or len(schema['tools'])!=1:raise RuntimeError('Exact one-method PhysicsAsset raw schema required')
    row=schema['tools'][0];e=expected['tools'][0]
    if row.get('name')!=e['name'] or row.get('inputSchema')!=e['inputSchema'] or row.get('outputSchema')!=e['outputSchema']:raise RuntimeError('Original PhysicsAsset15 fields/defaults/types/typed output changed')
    return {'schema_verified':True,'validated_tool_names':list(BINDINGS),'validated_input_counts':{raw.METHOD:15},
        'preparation_required':False,'actual_call_packing_verified':False,'behavior_verified':False,'runtime_physics_verified':False}
