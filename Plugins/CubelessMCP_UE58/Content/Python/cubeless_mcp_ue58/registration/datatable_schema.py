"""Strict original DataTable6 schema and exact named binding readiness."""
import unreal

from cubeless_mcp_ue58.compatibility.ue58 import datatable as api


BINDINGS = api.BINDINGS
FIELDS = {
    "get_datatable_schema": {"datatable_path": ("string",)},
    "list_datatable_rows": {"datatable_path": ("string",), "row_name_contains": ("string", ""),
        "max_rows": ("integer", 200), "include_values": ("boolean", True)},
    "get_datatable_row": {"datatable_path": ("string",), "row_name": ("string",)},
    "add_or_update_datatable_row": {"datatable_path": ("string",), "row_name": ("string",),
        "row_values": ("object",), "save": ("boolean", True)},
    "remove_datatable_row": {"datatable_path": ("string",), "row_name": ("string",), "save": ("boolean", True)},
    "create_datatable": {"asset_path": ("string",), "row_struct_path": ("string",), "save": ("boolean", True)}}


def binding_status():
    methods = api.binding_status()
    wrapper = getattr(unreal, "JsonObjectWrapper", None)
    export_available = isinstance(wrapper, type) and callable(getattr(wrapper, "export_text", None))
    prepare_available = callable(getattr(getattr(unreal, api.LIBRARY, None), api.PREPARATION, None))
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and export_available and prepare_available,
        "methods": methods, "library": api.LIBRARY,
        "json_object_wrapper_export_text_callable": export_available,
        "empty_string_default_preparation_callable": prepare_available,
        "preparation_method": api.PREPARATION, "preparation_requires_toolset_class": True,
        "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Six exact native callables and stock JsonObjectWrapper export presence only; root must prepare this exact toolset class then verify reflected schema and actual call packing"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("DataTable toolset must provide tools array")
    tools = schema["tools"]
    by_name = {}
    for tool in tools:
        if type(tool) is not dict or type(tool.get("name")) is not str:
            raise RuntimeError("DataTable toolset contains an invalid method record")
        name = tool["name"].rsplit(".", 1)[-1]
        if name not in FIELDS or name in by_name:
            raise RuntimeError("DataTable toolset changed the original six names or duplicated a method")
        by_name[name] = tool
    if set(by_name) != set(BINDINGS) or len(tools) != 6:
        raise RuntimeError("DataTable toolset must expose exactly the six original public methods")
    for name, fields in FIELDS.items():
        inputs = by_name[name].get("inputSchema")
        if type(inputs) is not dict:
            raise RuntimeError("DataTable method lacks input schema: " + name)
        properties = inputs.get("properties", {})
        required = inputs.get("required", [])
        expected_required = {key for key, definition in fields.items() if len(definition) == 1}
        if (type(properties) is not dict or set(properties) != set(fields) or type(required) is not list
                or len(required) != len(expected_required) or set(required) != expected_required):
            raise RuntimeError("DataTable method changed original input/required fields: " + name)
        for key, definition in fields.items():
            row = properties[key]
            if type(row) is not dict or row.get("type") != definition[0]:
                raise RuntimeError("DataTable changed reflected type: " + name + "." + key)
            if len(definition) == 1:
                if "default" in row:
                    raise RuntimeError("Required DataTable input acquired a default: " + name + "." + key)
            else:
                expected_type = {"string": str, "integer": int, "boolean": bool}[definition[0]]
                if "default" not in row or type(row["default"]) is not expected_type or row["default"] != definition[1]:
                    raise RuntimeError("DataTable changed original typed default: " + name + "." + key)
        if name == "add_or_update_datatable_row":
            row = properties["row_values"]
            if row.get("properties") or row.get("required") or row.get("additionalProperties") is False:
                raise RuntimeError("Stock JsonObjectWrapper must retain the original generic row object, including unknown keys")
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {name: len(fields) for name, fields in FIELDS.items()},
        "observed_tool_count": len(tools), "row_values_type": "object",
        "empty_string_default_verified": True, "actual_call_packing_verified": False}
