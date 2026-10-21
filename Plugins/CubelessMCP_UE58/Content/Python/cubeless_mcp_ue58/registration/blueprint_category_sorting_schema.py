"""Strict additive original categorySorting6 schema and source-only readiness.

Internal direct string-array/default-empty projects public nullable array/defaultNone;
empty-front/default-empty also needs exact converter/preparation. Optional-array
packing is not claimed: stock splits it before a custom callback.
"""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import category_sorting as api
BINDINGS = api.BINDINGS
PREPARATION_METHOD = "prepare_blueprint_category_sorting_toolset"
DEFAULTS = {BINDINGS[0]+".category_order":None,BINDINGS[0]+".category_to_front":""}
FIELDS = {"blueprint_name":("string",),"category_order":(["array","null"],None),
    "category_to_front":("string",""),"append_existing":("boolean",True),
    "save":("boolean",True),"compile":("boolean",False)}


def binding_status():
    methods = api.binding_status()
    library = getattr(unreal,api.LIBRARY,None)
    preparation = callable(getattr(library,PREPARATION_METHOD,None))
    return {"ready":set(methods)==set(BINDINGS) and all(methods.values()) and preparation,
        "library":api.LIBRARY,"methods":methods,"preparation_required":True,
        "preparation_method":PREPARATION_METHOD,"preparation_callable":preparation,
        "expected_defaults":DEFAULTS,"schema_verified":False,"nullable_schema_verified":False,
        "behavior_verified":False,"actual_optional_array_call_packing_verified":False,
        "internal_category_order_reflection":"Array(FString)","internal_category_order_default":[],
        "public_category_order_default":None,"null_and_empty_order_both_preserve_original_omission":True,
        "verification_scope":"Exact hidden6/preparation1 presence only; direct internal Array(FString)/empty default must project original public array-null/defaultNone. Exact empty-front and official schema/preparation/call packing are separate observed gates"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Blueprint tools array required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".",1)[-1] in BINDINGS]
    if len(rows)!=1: raise RuntimeError("Exactly one original category sorting name required")
    inputs=rows[0].get("inputSchema",{});props=inputs.get("properties",{})
    if set(props)!=set(FIELDS) or inputs.get("required",[])!=["blueprint_name"]:
        raise RuntimeError("Original category sorting6 input/required names changed")
    for name,spec in FIELDS.items():
        row=props[name]
        if type(row) is not dict or row.get("type")!=spec[0]: raise RuntimeError("Original category sorting type changed: "+name)
        if len(spec)==1:
            if "default" in row: raise RuntimeError("Required Blueprint name must not be defaulted")
        elif "default" not in row or type(row["default"]) is not type(spec[1]) or row["default"]!=spec[1]:
            raise RuntimeError("Original category sorting default changed: "+name)
    if props["category_order"].get("items",{}).get("type")!="string":
        raise RuntimeError("Original nullable category order requires string items")
    return {"schema_verified":True,"nullable_schema_verified":True,"validated_tool_names":list(BINDINGS),
        "validated_input_counts":{BINDINGS[0]:6},"preparation_required":True,"expected_defaults":DEFAULTS,
        "behavior_verified":False,"actual_optional_array_call_packing_verified":False}
