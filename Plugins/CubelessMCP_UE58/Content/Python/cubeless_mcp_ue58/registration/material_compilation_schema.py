"""Strict original Compile/Save3 subset on the existing Material Toolset."""
from cubeless_mcp_ue58.compatibility.ue58 import material_compilation as api

BINDINGS = api.BINDINGS


def binding_status():
    methods = api.binding_status()
    jobs = api.async_binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(v is True for v in jobs.values()), "library": api.LIBRARY,
        "methods": methods, "async_native_jobs": jobs, "Named_sameJob_continuation": True, "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Exact original3 native binding presence; immediate completion and actual save are separate observations"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Material schema requires tools array")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".", 1)[-1] in BINDINGS]
    if len(rows) != 1:
        raise RuntimeError("Exactly one original compile_and_save_material method required")
    inputs = rows[0].get("inputSchema")
    if type(inputs) is not dict or inputs.get("required") != ["material_path"] or set(inputs.get("properties", {})) != {"material_path", "graph_type", "save"}:
        raise RuntimeError("Changed original Material Compile/Save3 input contract")
    for key, kind, default in (("material_path", "string", None), ("graph_type", "string", "auto"), ("save", "boolean", True)):
        row = inputs["properties"][key]
        if type(row) is not dict or row.get("type") != kind:
            raise RuntimeError("Changed original Material compilation type: " + key)
        if key == "material_path":
            if "default" in row:
                raise RuntimeError("Required material_path unexpectedly defaulted")
        elif type(row.get("default")) is not type(default) or row["default"] != default:
            raise RuntimeError("Changed original Material compilation default: " + key)
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 3}, "observed_tool_count": len(schema["tools"]),
        "immediate_compile_or_saved_outcome_verified": False, "whole_original_wait_streaming_contract_verified": False}
