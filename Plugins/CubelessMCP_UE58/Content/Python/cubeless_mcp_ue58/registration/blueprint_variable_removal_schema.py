"""Strict additive original selected RemoveVariable2 subset; no preparation."""
from cubeless_mcp_ue58.compatibility.ue58 import variable_removal as api
BINDINGS = api.BINDINGS
FIELDS = {"blueprint_name":"string","variable_name":"string"}


def binding_status():
    methods = api.binding_status()
    return {"ready":set(methods)==set(BINDINGS) and all(methods.values()),"library":api.LIBRARY,"methods":methods,
        "preparation_required":False,"schema_verified":False,"behavior_verified":False,
        "generated_runtime_layout_absence_verified":False,
        "verification_scope":"Exact hidden remove2 presence only; actual canonical required-string schema and finite own member/Get-Set retirement behavior remain observed gates"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Blueprint tools array required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".",1)[-1] in BINDINGS]
    if len(rows)!=1: raise RuntimeError("Exactly one original removal name is required")
    inputs = rows[0].get("inputSchema",{});props = inputs.get("properties",{})
    if (set(props)!=set(FIELDS) or len(inputs.get("required",[]))!=2 or set(inputs.get("required",[]))!=set(FIELDS)):
        raise RuntimeError("Original required RemoveVariable2 inputs changed")
    for name,kind in FIELDS.items():
        row=props[name]
        if type(row) is not dict or row.get("type")!=kind or "default" in row:
            raise RuntimeError("Original removal string/default changed: "+name)
    return {"schema_verified":True,"validated_tool_names":list(BINDINGS),"validated_input_counts":{BINDINGS[0]:2},
        "preparation_required":False,"behavior_verified":False,"generated_runtime_layout_absence_verified":False}
