"""Original Retarget5 subset; no default converter or preparation."""
from cubeless_mcp_ue58.compatibility.ue58 import skeletal_retarget as api
BINDINGS=api.BINDINGS
FIELDS={BINDINGS[0]:{'skeleton_path':('string',),'bone_name':('string',),'mode':('string',),'recursive':('boolean',False),'save':('boolean',True)}}

def binding_status():
    methods=api.binding_status()
    return {'ready':set(methods)==set(BINDINGS) and all(methods.values()),'methods':methods,'library':api.LIBRARY,
        'preparation_required':False,'schema_verified':False,'behavior_verified':False,'actual_call_packing_verified':False}

def validate_schema(schema):
    if type(schema) is not dict or type(schema.get('tools')) is not list:raise RuntimeError('Actual Skeletal schema required')
    rows=[row for row in schema['tools'] if row['name'].rsplit('.',1)[-1] in BINDINGS]
    if len(rows)!=1:raise RuntimeError('Exact original Retarget5 method required')
    fields=FIELDS[BINDINGS[0]];actual=rows[0]['inputSchema'];properties=actual.get('properties',{})
    if set(properties)!=set(fields) or actual.get('required')!=['skeleton_path','bone_name','mode']:raise RuntimeError('Retarget5 required field set changed')
    for key,value in fields.items():
        prop=properties[key]
        if prop.get('type')!=value[0]:raise RuntimeError('Retarget5 public scalar type changed')
        if len(value)==1:
            if 'default' in prop:raise RuntimeError('Required retarget field defaulted')
        elif 'default' not in prop or type(prop['default']) is not type(value[1]) or prop['default']!=value[1]:raise RuntimeError('Original recursive/save default changed')
    return {'schema_verified':True,'validated_tool_names':list(BINDINGS),'validated_input_counts':{BINDINGS[0]:5},
        'preparation_required':False,'behavior_verified':False,'actual_call_packing_verified':False}
