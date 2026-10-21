"""Exact additive original CustomEventMetadata10 on canonical Graph tools."""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import custom_event_metadata as api
BINDINGS = api.BINDINGS
PREPARATION_METHOD = api.PREPARATION
DEFAULTS = {BINDINGS[0]+"."+name:"" for name in ("custom_event_name","description","node_id","event_name","tooltip")}
DEFAULTS[BINDINGS[0]+".call_in_editor"] = None
FIELDS = {"blueprint_name":("string",),"custom_event_name":("string",""),"description":("string",""),
    "node_id":("string",""),"event_name":("string",""),"tooltip":("string",""),
    "call_in_editor":(["boolean","null"],None),"set_node_comment":("boolean",True),
    "compile":("boolean",True),"save":("boolean",False)}


def binding_status():
    methods = api.binding_status()
    preparation = callable(getattr(getattr(unreal,api.LIBRARY,None),PREPARATION_METHOD,None))
    return {"ready":set(methods)==set(BINDINGS) and all(methods.values()) and preparation,
        "library":api.LIBRARY,"methods":methods,"preparation_required":True,
        "preparation_method":PREPARATION_METHOD,"preparation_callable":preparation,
        "preparation_requires_toolset_class":True,"expected_defaults":dict(DEFAULTS),
        "schema_verified":False,"nullable_schema_verified":False,"behavior_verified":False,
        "actual_nullable_boolean_call_packing_verified":False,
        "verification_scope":"Exact hidden10/preparation1 presence only. Canonical Graph defaults6/type-union/preparation and actual null/False/alias/compile-save behavior are separate gates"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Graph tools array required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".",1)[-1] in BINDINGS]
    if len(rows)!=1: raise RuntimeError("Exactly one original CustomEventMetadata name required")
    inputs = rows[0].get("inputSchema",{}); fields = inputs.get("properties",{})
    if (type(fields) is not dict or set(fields)!=set(FIELDS) or len(inputs.get("required",[]))!=1
            or set(inputs.get("required",[]))!={"blueprint_name"}):
        raise RuntimeError("Original ten fields/one required Blueprint name changed")
    for name,spec in FIELDS.items():
        row = fields[name]
        if type(row) is not dict or row.get("type")!=spec[0]:
            raise RuntimeError("Original custom-event metadata type changed: "+name)
        if len(spec)==1:
            if "default" in row: raise RuntimeError("Required Blueprint name must not be defaulted")
        elif "default" not in row or type(row["default"]) is not type(spec[1]) or row["default"]!=spec[1]:
            raise RuntimeError("Original custom-event metadata default changed: "+name)
    return {"schema_verified":True,"nullable_schema_verified":True,"validated_tool_names":list(BINDINGS),
        "validated_input_counts":{BINDINGS[0]:10},"preparation_required":True,"expected_defaults":dict(DEFAULTS),
        "behavior_verified":False,"actual_nullable_boolean_call_packing_verified":False}
