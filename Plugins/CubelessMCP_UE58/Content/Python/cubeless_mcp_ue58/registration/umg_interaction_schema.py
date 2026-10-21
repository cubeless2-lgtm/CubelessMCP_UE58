"""Exact original Slider2/Bind4 public contracts and native composite receipts."""
import copy
import json

TOOLSET = 'CubelessMCPUE58Editor.CubelessUMGInteractionTools'
BINDINGS = ('add_sliders_to_widget', 'bind_widget_event')
PUBLIC_FIELDS_BY_METHOD = {'add_sliders_to_widget': ('widget_name', 'sliders'), 'bind_widget_event': ('widget_name', 'widget_component_name', 'event_name', 'function_name')}
DEFAULTS_BY_METHOD = {'add_sliders_to_widget': {}, 'bind_widget_event': {'function_name': ''}}
REQUIRED_BY_METHOD = {'add_sliders_to_widget': ('widget_name', 'sliders'), 'bind_widget_event': ('widget_name', 'widget_component_name', 'event_name')}
FIELD_KINDS_BY_METHOD = {'add_sliders_to_widget': {'widget_name': 'string', 'sliders': 'descriptor_array'}, 'bind_widget_event': {'widget_name': 'string', 'widget_component_name': 'string', 'event_name': 'string', 'function_name': 'string'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessUMGInteractionTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessUMGInteractionTools.add_sliders_to_widget', 'description': 'Original Slider2 forwards the full List[Dict[str,Any]] descriptor array without Python per-row trim/default/geometry/range or coalescing. Native owns original all-row validation, CanvasPanel root, Slider properties/slot/generated variables and one structural/full compiler then unconditional force-save. All actual row/allocation/compiler/save/known-null prefixes precede errors; legacy response separate. No wrapper retry, style fallback, render/runtime/cold/full pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'widget_name': {'type': 'string'}, 'sliders': {'type': 'array', 'items': {'type': 'object', 'additionalProperties': True}}}, 'required': ['widget_name', 'sliders'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessUMGInteractionTools.bind_widget_event', 'description': 'Original Bind4 preserves function_name default empty string in reflection. The Python wrapper derives component_event only when falsey before forwarding; original Native ignores function_name. Native owns actual widget/generated property/delegate lookup after the first compile, Engine ComponentBoundEvent reuse/creation, structural refresh and second compile then force-save, even reuse. Actual phase counts/typed flags/raw partial prefixes precede errors; no Python lookup, extra compile/save, replay, runtime invocation or full pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'widget_name': {'type': 'string'}, 'widget_component_name': {'type': 'string'}, 'event_name': {'type': 'string'}, 'function_name': {'type': 'string', 'default': ''}}, 'required': ['widget_name', 'widget_component_name', 'event_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import umg_interaction as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 2,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_native_function_name_ignored": True, "native_composite_compile_save_authoritative": True,
        "actual_umg_interaction_behavior_verified": False, "full_original_UMG_interaction_runtime_or_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 2):
        raise RuntimeError("Exact UMGInteraction raw2 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("UMGInteraction method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original UMGInteraction input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if (type(item) is not dict or json.dumps(item, sort_keys=True, allow_nan=False)
                    != json.dumps(expected["inputSchema"]["properties"][field], sort_keys=True, allow_nan=False)):
                raise RuntimeError("Original UMGInteraction scalar/descriptor-array/default type changed: " + field)
        output = rows[0].get("outputSchema")
        if (type(output) is not dict or json.dumps(output, sort_keys=True, allow_nan=False)
                != json.dumps(expected["outputSchema"], sort_keys=True, allow_nan=False)):
            raise RuntimeError("Exact standard UMGInteraction operation output11 envelope required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "original2_4_descriptor_function_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_umg_interaction_behavior_or_pipeline_verified": False}
