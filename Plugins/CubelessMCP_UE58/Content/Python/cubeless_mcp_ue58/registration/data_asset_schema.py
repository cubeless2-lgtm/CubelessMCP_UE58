"""Strict original DataAsset3 subset on its dedicated stock ToolsetAPI class."""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import data_asset as api
BINDINGS = api.BINDINGS


def binding_status():
    methods = api.binding_status()
    wrapper = getattr(unreal, "JsonObjectWrapper", None)
    export_available = isinstance(wrapper, type) and callable(getattr(wrapper, "export_text", None))
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and export_available,
        "library": api.LIBRARY, "methods":methods,
        "json_object_wrapper_export_text_callable":export_available,
        "schema_verified":False, "behavior_verified":False, "preparation_required":False,
        "verification_scope":"Three exact native callables and stock JsonObjectWrapper export presence; actual reflected schema and object packing require official observations"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual DataAsset tools array required")
    rows = schema["tools"]
    if len(rows) != 3 or any(type(r) is not dict or type(r.get("name")) is not str for r in rows) or {r["name"].rsplit(".",1)[-1] for r in rows} != set(BINDINGS):
        raise RuntimeError("Exactly three original DataAsset tools required")
    definitions = {"get_data_asset":({"asset_path":"string"},{"asset_path"}),
        "set_data_asset_properties":({"asset_path":"string","properties":"object","save":"boolean"},{"asset_path","properties"}),
        "create_data_asset":({"asset_path":"string","class_path":"string","save":"boolean"},{"asset_path","class_path"})}
    for r in rows:
        name=r["name"].rsplit(".",1)[-1];expected,required=definitions[name]
        inputs=r.get("inputSchema",{});fields=inputs.get("properties",{})
        if type(fields) is not dict or set(fields)!=set(expected) or set(inputs.get("required",[]))!=required:
            raise RuntimeError("Changed original DataAsset input/required fields: "+name)
        for key,kind in expected.items():
            if type(fields[key]) is not dict or fields[key].get("type")!=kind: raise RuntimeError("Changed DataAsset type: "+key)
            if key=="save":
                if fields[key].get("default") is not True: raise RuntimeError("Original saveTrue default required")
            elif "default" in fields[key]: raise RuntimeError("Required DataAsset input must not be defaulted")
    return {"schema_verified":True,"validated_tool_names":list(BINDINGS),"validated_input_counts":{k:len(v[0]) for k,v in definitions.items()},
        "actual_object_call_packing_verified":False,"preparation_required":False}
