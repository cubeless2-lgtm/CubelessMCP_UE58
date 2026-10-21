"""Original socket3 subset: public nullable numeric arrays, exact None/empty defaults."""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import skeletal_socket as api
BINDINGS=api.BINDINGS
FIELDS={
 'add_skeletal_socket':{'skeleton_path':('string',),'socket_name':('string',),'bone_name':('string',),'location':('array',None),'rotation':('array',None),'scale':('array',None),'save':('boolean',True)},
 'set_skeletal_socket':{'skeleton_path':('string',),'socket_name':('string',),'bone_name':('string',''),'new_socket_name':('string',''),'location':('array',None),'rotation':('array',None),'scale':('array',None),'save':('boolean',True)},
 'remove_skeletal_socket':{'skeleton_path':('string',),'socket_name':('string',),'save':('boolean',True)}}
DEFAULTS={name+'.'+key:value[1] for name,fields in FIELDS.items() for key,value in fields.items() if len(value)>1 and key!='save'}
def binding_status():
    methods=api.binding_status();prep=callable(getattr(getattr(unreal,api.LIBRARY,None),api.PREPARATION,None))
    return {'ready':set(methods)==set(BINDINGS) and all(methods.values()) and prep,'methods':methods,'library':api.LIBRARY,
        'preparation_required':True,'preparation_callable':prep,'schema_verified':False,'nullable_schema_verified':False,
        'actual_call_packing_verified':False,'behavior_verified':False,'public_transform_carrier':'numeric array/null; internal zero-or-one serialized-array string',
        'explicit_empty_array_is_omission':False}
def validate_schema(schema):
    if type(schema) is not dict or type(schema.get('tools')) is not list:raise RuntimeError('Actual Skeletal tools schema required')
    rows=[r for r in schema['tools'] if r['name'].rsplit('.',1)[-1] in BINDINGS]
    tools={r['name'].rsplit('.',1)[-1]:r for r in rows}
    if len(rows)!=3 or set(tools)!=set(BINDINGS):raise RuntimeError('Original socket3 subset missing/duplicated')
    for name,expected in FIELDS.items():
        actual=tools[name]['inputSchema'];fields=actual.get('properties',{});required=[k for k,v in expected.items() if len(v)==1]
        if set(fields)!=set(expected) or actual.get('required')!=required:raise RuntimeError('Socket field/required contract changed')
        for key,value in expected.items():
            f=fields[key]
            if value[0]=='array':
                if type(f.get('type')) is not list or len(f['type'])!=2 or set(f['type'])!={'array','null'} or f.get('items')!={'type':'number'}:
                    raise RuntimeError('Public socket transform requires nullable numeric array')
            elif f.get('type')!=value[0]:raise RuntimeError('Socket public scalar type changed')
            if len(value)==1:
                if 'default' in f:raise RuntimeError('Required socket field defaulted')
            elif 'default' not in f or type(f['default']) is not type(value[1]) or f['default']!=value[1]:raise RuntimeError('Original socket default changed')
    return {'schema_verified':True,'nullable_schema_verified':True,'validated_tool_names':list(BINDINGS),
        'validated_input_counts':{k:len(v) for k,v in FIELDS.items()},'actual_call_packing_verified':False,'behavior_verified':False}
