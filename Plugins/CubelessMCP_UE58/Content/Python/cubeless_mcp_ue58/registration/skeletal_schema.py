"""Exact original read2 schema and one empty-string default preparation gate."""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import skeletal as api
BINDINGS=api.BINDINGS
FIELDS={'list_skeleton_bones':{'skeleton_path':('string',),'bone_name_contains':('string',''),'max_bones':('integer',500)},'list_skeletal_sockets':{'skeleton_path':('string',)}}
def binding_status():
    methods=api.binding_status();prep=callable(getattr(getattr(unreal,api.LIBRARY,None),api.PREPARATION,None))
    return {'ready':set(methods)==set(BINDINGS) and all(methods.values()) and prep,'methods':methods,'library':api.LIBRARY,
        'preparation_required':True,'preparation_callable':prep,'schema_verified':False,'behavior_verified':False,'actual_call_packing_verified':False}
def validate_schema(schema):
    if type(schema) is not dict or type(schema.get('tools')) is not list or len(schema['tools'])!=2:raise RuntimeError('Actual dedicated Skeletal tools2 required')
    tools={r['name'].rsplit('.',1)[-1]:r for r in schema['tools']}
    if set(tools)!=set(BINDINGS):raise RuntimeError('Original two skeletal reads required')
    for name,expected in FIELDS.items():
        actual=tools[name]['inputSchema'];fields=actual.get('properties',{});required=[k for k,v in expected.items() if len(v)==1]
        if set(fields)!=set(expected) or actual.get('required')!=required:raise RuntimeError('Skeletal required/default field set changed')
        for key,v in expected.items():
            f=fields[key]
            if f.get('type')!=v[0]:raise RuntimeError('Skeletal public type changed')
            if len(v)==1:
                if 'default' in f:raise RuntimeError('Required skeletal path defaulted')
            elif 'default' not in f or type(f['default']) is not type(v[1]) or f['default']!=v[1]:raise RuntimeError('Original skeletal default changed')
    return {'schema_verified':True,'validated_tool_names':list(BINDINGS),'validated_input_counts':{k:len(v) for k,v in FIELDS.items()},
        'bone_filter_empty_default_verified':True,'behavior_verified':False,'actual_call_packing_verified':False}
