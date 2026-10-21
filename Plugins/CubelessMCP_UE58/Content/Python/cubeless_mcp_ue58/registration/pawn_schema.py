"""Exact additive original Pawn6 nullable/default schema on canonical Blueprint."""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import pawn as api
BINDINGS=api.BINDINGS
PREPARATION_METHOD=api.PREPARATION
FIELDS={"blueprint_name":("string",),"auto_possess_player":("string",""),
    "use_controller_rotation_yaw":(["boolean","null"],None),"use_controller_rotation_pitch":(["boolean","null"],None),
    "use_controller_rotation_roll":(["boolean","null"],None),"can_be_damaged":(["boolean","null"],None)}
DEFAULTS={BINDINGS[0]+"."+name:spec[1] for name,spec in FIELDS.items() if len(spec)==2}


def binding_status():
    methods=api.binding_status();preparation=callable(getattr(getattr(unreal,api.LIBRARY,None),PREPARATION_METHOD,None))
    return {"ready":set(methods)==set(BINDINGS) and all(methods.values()) and preparation,"library":api.LIBRARY,"methods":methods,
        "preparation_required":True,"preparation_method":PREPARATION_METHOD,"preparation_callable":preparation,
        "preparation_requires_toolset_class":True,"expected_defaults":dict(DEFAULTS),"schema_verified":False,"nullable_schema_verified":False,
        "behavior_verified":False,"actual_nullable_boolean_call_packing_verified":False,
        "verification_scope":"Exact native6/preparation1 presence only; canonical defaults5/schema/None/False/partial behavior are separate gates"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Blueprint tools array required")
    rows=[r for r in schema["tools"] if type(r) is dict and type(r.get("name")) is str and r["name"].rsplit(".",1)[-1] in BINDINGS]
    if len(rows)!=1:raise RuntimeError("Exactly one original Pawn method required")
    inputs=rows[0].get("inputSchema",{});properties=inputs.get("properties",{})
    if type(properties) is not dict or set(properties)!=set(FIELDS) or len(inputs.get("required",[]))!=1 or set(inputs["required"])!={"blueprint_name"}:
        raise RuntimeError("Original Pawn six inputs/required Blueprint name changed")
    for name,spec in FIELDS.items():
        row=properties[name]
        if type(row) is not dict or row.get("type")!=spec[0]:raise RuntimeError("Original Pawn type changed: "+name)
        if len(spec)==1:
            if "default" in row:raise RuntimeError("Required Blueprint name must not be defaulted")
        elif "default" not in row or type(row["default"]) is not type(spec[1]) or row["default"]!=spec[1]:
            raise RuntimeError("Original Pawn default changed: "+name)
    return {"schema_verified":True,"nullable_schema_verified":True,"validated_tool_names":list(BINDINGS),
        "validated_input_counts":{BINDINGS[0]:6},"expected_defaults":dict(DEFAULTS),"preparation_required":True,
        "behavior_verified":False,"actual_nullable_boolean_call_packing_verified":False}
