"""Strict additive subset for original Blueprint metadata6/order5.

Exact field converter publishes original nullable object/bool unions with defaultNone.
No public text surrogate, new class or inferred mutation_result_v1.
"""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import variable_lifecycle as api
BINDINGS = api.BINDINGS
PREPARATION_METHOD = "prepare_blueprint_variable_lifecycle_toolset"
DEFAULTS = {"set_blueprint_variable_metadata."+key:None for key in
    ("metadata","is_editable","instance_editable","expose_on_spawn")}
FIELDS = {
    BINDINGS[0]:{"blueprint_name":("string",),"variable_name":("string",),"metadata":(["object","null"],None),
        "is_editable":(["boolean","null"],None),"instance_editable":(["boolean","null"],None),"expose_on_spawn":(["boolean","null"],None)},
    BINDINGS[1]:{"blueprint_name":("string",),"variable_order":("array",),"strict":("boolean",False),
        "compile":("boolean",False),"save":("boolean",False)}}


def binding_status():
    methods = api.binding_status()
    wrapper = getattr(unreal,"JsonObjectWrapper",None)
    export = isinstance(wrapper,type) and callable(getattr(wrapper,"export_text",None))
    library = getattr(unreal,api.LIBRARY,None)
    preparation = callable(getattr(library,PREPARATION_METHOD,None))
    return {"ready":set(methods)==set(BINDINGS) and all(methods.values()) and export and preparation,
        "library":api.LIBRARY,"methods":methods,"json_object_wrapper_export_text_callable":export,
        "preparation_required":True,"preparation_method":PREPARATION_METHOD,"preparation_callable":preparation,
        "expected_defaults":DEFAULTS,"nullable_schema_verified":False,
        "schema_verified":False,"behavior_verified":False,"actual_nullable_object_call_packing_verified":False,
        "verification_scope":"Exact native6/5, verification prep1 and stock optional object export presence; full official union/default-null schema and actual optional call packing remain independent observed gates"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Blueprint tools array required")
    rows = {}
    for tool in schema["tools"]:
        if type(tool) is not dict or type(tool.get("name")) is not str: raise RuntimeError("Invalid Blueprint tool record")
        name = tool["name"].rsplit(".",1)[-1]
        if name in BINDINGS:
            if name in rows: raise RuntimeError("Duplicate variable lifecycle method")
            rows[name] = tool
    if set(rows) != set(BINDINGS): raise RuntimeError("Both original variable lifecycle names required")
    for name,expected in FIELDS.items():
        inputs = rows[name].get("inputSchema",{});properties = inputs.get("properties",{})
        required = {k for k,v in expected.items() if len(v)==1}
        if set(properties)!=set(expected) or set(inputs.get("required",[]))!=required: raise RuntimeError("Changed variable lifecycle input/required names: "+name)
        for key,spec in expected.items():
            row=properties[key]
            if type(row) is not dict or row.get("type") != spec[0]: raise RuntimeError("Changed variable lifecycle type: "+key)
            if len(spec)==1:
                if "default" in row: raise RuntimeError("Required input must not be defaulted")
            elif "default" not in row or type(row["default"]) is not type(spec[1]) or row["default"] != spec[1]:
                raise RuntimeError("Changed original variable lifecycle default: "+key)
            if key == "variable_order" and row.get("items",{}).get("type") != "string": raise RuntimeError("Original string-array order required")
    return {"schema_verified":True,"nullable_schema_verified":True,"validated_tool_names":list(BINDINGS),"validated_input_counts":{k:len(v) for k,v in FIELDS.items()},
        "actual_nullable_object_call_packing_verified":False,"behavior_verified":False}
