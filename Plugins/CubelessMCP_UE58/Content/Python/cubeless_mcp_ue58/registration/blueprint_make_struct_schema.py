"""Exact original MakeStruct7 plus three empty Graph selector defaults."""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_make_struct as api
BINDINGS = api.BINDINGS
DEFAULTS = {BINDINGS[0] + "." + k: "" for k in ("graph_name", "graph_id", "graph_type")}


def binding_status():
    methods = api.binding_status(); prepare = callable(getattr(getattr(unreal, api.LIBRARY, None), api.PREPARATION, None))
    return {"ready": all(methods.values()) and prepare, "library": api.LIBRARY, "methods": methods,
        "preparation_method": api.PREPARATION, "empty_selector_preparation_callable": prepare,
        "preparation_requires_toolset_class": True, "schema_verified": False, "behavior_verified": False}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list: raise RuntimeError("Graph tools array required")
    tools = [r for r in schema["tools"] if type(r) is dict and str(r.get("name", "")).rsplit(".", 1)[-1] in BINDINGS]
    if len(tools) != 1: raise RuntimeError("Exactly one original MakeStruct method required")
    inputs = tools[0].get("inputSchema", {}); fields = inputs.get("properties", {})
    expected = {"blueprint_name": "string", "struct_type": "string", "node_position": "array",
        "graph_name": "string", "graph_id": "string", "graph_type": "string", "create_graph_if_missing": "boolean"}
    if type(fields) is not dict or set(fields) != set(expected) or set(inputs.get("required", [])) != {"blueprint_name", "struct_type"}:
        raise RuntimeError("Changed original seven fields/two required inputs")
    for key, kind in expected.items():
        if type(fields[key]) is not dict or fields[key].get("type") != kind: raise RuntimeError("Changed MakeStruct type: " + key)
    for key in ("graph_name", "graph_id", "graph_type"):
        if fields[key].get("default") != "" or type(fields[key].get("default")) is not str: raise RuntimeError("Original empty selector default missing")
    position = fields["node_position"]
    if position.get("items", {}).get("type") != "number" or position.get("nullable") is not True or "default" not in position or position["default"] is not None:
        raise RuntimeError("Original null/array numeric node_position packing missing")
    if fields["create_graph_if_missing"].get("default") is not False: raise RuntimeError("Changed original createFalse default")
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS), "validated_input_counts": {BINDINGS[0]: 7}, "actual_call_packing_verified": False}
