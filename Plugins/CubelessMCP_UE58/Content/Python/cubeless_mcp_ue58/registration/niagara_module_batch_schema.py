"""Original module batch11 on the existing Niagara module ToolsetAPI class."""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import niagara_module_batch as api

BINDINGS = api.BINDINGS
FIELDS = {"system_path": ("string",), "edits": ("array",), "operation": ("string", "set_existing"),
    "overwrite_existing": ("boolean", False), "continue_on_error": ("boolean", False),
    "allow_source_edit": ("boolean", False), "save": ("boolean", True), "request_compile": ("boolean", True),
    "wait_for_completion": ("boolean", False), "timeout_seconds": ("number", 10.0), "poll_interval_seconds": ("number", 0.1)}
DEFAULTS = {BINDINGS[0] + "." + name: spec[1] for name, spec in FIELDS.items() if len(spec) == 2}


def binding_status():
    methods = api.binding_status()
    jobs = api.async_binding_status()
    wrapper = getattr(unreal, "JsonObjectWrapper", None)
    export_available = isinstance(wrapper, type) and callable(getattr(wrapper, "export_text", None))
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(v is True for v in jobs.values()) and export_available,
        "library": api.LIBRARY, "methods": methods, "async_native_jobs": jobs, "sameJob_Named_batch_continuation": True, "json_object_wrapper_export_text_callable": export_available,
        "expected_defaults": dict(DEFAULTS), "preparation_required": False, "schema_verified": False,
        "behavior_verified": False, "actual_array_object_call_packing_verified": False,
        "verification_scope": "Exact batch11 binding and stock wrapper export presence; actual reflected schema/defaults/object-array packing remain independent gates"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual Niagara module tools array required")
    rows = schema["tools"]
    expected_names = {"add_niagara_stock_module_to_stack", BINDINGS[0], "inspect_niagara_module_rapid_iteration"}
    if (len(rows) != 3 or any(type(row) is not dict or type(row.get("name")) is not str for row in rows)
            or {row["name"].rsplit(".", 1)[-1] for row in rows} != expected_names):
        raise RuntimeError("Exactly old stock-module tool, original module batch tool and RI read extension required")
    row = next(row for row in rows if row["name"].rsplit(".", 1)[-1] == BINDINGS[0])
    inputs = row.get("inputSchema", {})
    fields = inputs.get("properties", {})
    required = inputs.get("required", [])
    if type(fields) is not dict or set(fields) != set(FIELDS) or len(required) != 2 or set(required) != {"system_path", "edits"}:
        raise RuntimeError("Original module batch11 fields/required inputs changed")
    for name, spec in FIELDS.items():
        field = fields[name]
        if type(field) is not dict or field.get("type") != spec[0]:
            raise RuntimeError("Changed module batch type: " + name)
        if len(spec) == 1:
            if "default" in field:
                raise RuntimeError("Required module batch input must not be defaulted: " + name)
        else:
            default = field.get("default")
            type_ok = type(default) in (int, float) if spec[0] == "number" else type(default) is type(spec[1])
            if "default" not in field or not type_ok or default != spec[1]:
                raise RuntimeError("Original module batch default changed: " + name)
    items = fields["edits"].get("items")
    if type(items) is not dict or items.get("type") != "object":
        raise RuntimeError("Original required edits must remain an array of JSON objects")
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS), "canonical_tool_names": sorted(expected_names),
        "validated_input_counts": {BINDINGS[0]: 11}, "expected_defaults": dict(DEFAULTS),
        "preparation_required": False, "actual_array_object_call_packing_verified": False, "behavior_verified": False}
