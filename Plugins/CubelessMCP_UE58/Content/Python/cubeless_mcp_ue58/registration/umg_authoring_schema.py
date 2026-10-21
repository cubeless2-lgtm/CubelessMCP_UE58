"""Exact original UMG3/7/8 contracts; ignored styles and native composite receipts."""
import copy
import json

TOOLSET = 'CubelessMCPUE58Editor.CubelessUMGAuthoringTools'
BINDINGS = ('create_umg_widget_blueprint', 'add_text_block_to_widget', 'add_button_to_widget')
PUBLIC_FIELDS_BY_METHOD = {'create_umg_widget_blueprint': ('widget_name', 'parent_class', 'path'), 'add_text_block_to_widget': ('widget_name', 'text_block_name', 'text', 'position', 'size', 'font_size', 'color'), 'add_button_to_widget': ('widget_name', 'button_name', 'text', 'position', 'size', 'font_size', 'color', 'background_color')}
DEFAULTS_BY_METHOD = {'create_umg_widget_blueprint': {'parent_class': 'UserWidget', 'path': '/Game/UI'}, 'add_text_block_to_widget': {'text': '', 'position': [0.0, 0.0], 'size': [200.0, 50.0], 'font_size': 12, 'color': [1.0, 1.0, 1.0, 1.0]}, 'add_button_to_widget': {'text': '', 'position': [0.0, 0.0], 'size': [200.0, 50.0], 'font_size': 12, 'color': [1.0, 1.0, 1.0, 1.0], 'background_color': [0.1, 0.1, 0.1, 1.0]}}
REQUIRED_BY_METHOD = {'create_umg_widget_blueprint': ('widget_name',), 'add_text_block_to_widget': ('widget_name', 'text_block_name'), 'add_button_to_widget': ('widget_name', 'button_name')}
FIELD_KINDS_BY_METHOD = {'create_umg_widget_blueprint': {'widget_name': 'string', 'parent_class': 'string', 'path': 'string'}, 'add_text_block_to_widget': {'widget_name': 'string', 'text_block_name': 'string', 'text': 'string', 'position': 'number_array', 'size': 'number_array', 'font_size': 'integer', 'color': 'number_array'}, 'add_button_to_widget': {'widget_name': 'string', 'button_name': 'string', 'text': 'string', 'position': 'number_array', 'size': 'number_array', 'font_size': 'integer', 'color': 'number_array', 'background_color': 'number_array'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessUMGAuthoringTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessUMGAuthoringTools.create_umg_widget_blueprint', 'description': 'Original Create3 preserves parent_class UserWidget and path /Game/UI. Native resolves supported UUserWidget parent, creates WidgetBlueprint/tree/RootCanvas, registers asset and performs original structural/full compiler then force-save. Actual flags, legacy result and partial tree/package/save prefixes precede errors; compiler and save are not Python flags or coalesced Batch work. Ordinary UMG assets use stock Engine/UMG runtime types. EditorUtility/widget class variants, renderer/cold/runtime/full pipeline remain separately scoped.', 'inputSchema': {'type': 'object', 'properties': {'widget_name': {'type': 'string'}, 'parent_class': {'type': 'string', 'default': 'UserWidget'}, 'path': {'type': 'string', 'default': '/Game/UI'}}, 'required': ['widget_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessUMGAuthoringTools.add_text_block_to_widget', 'description': 'Original Text7 preserves numeric arrays and default values without length or geometry clamps. Native adds named TextBlock/text/RootCanvas/slot position-size and generated variable, then original structural/full compiler and force-save. font_size/color are forwarded but ignored by original handlers; no style-applied claim. Bare target names resolve original /Game/Widgets, while create defaults /Game/UI; use returned full asset path. Full actual partial/known-null/raw/compiler/save prefixes precede errors, no wrapper retry or extra save.', 'inputSchema': {'type': 'object', 'properties': {'widget_name': {'type': 'string'}, 'text_block_name': {'type': 'string'}, 'text': {'type': 'string', 'default': ''}, 'position': {'type': 'array', 'items': {'type': 'number'}, 'default': [0.0, 0.0]}, 'size': {'type': 'array', 'items': {'type': 'number'}, 'default': [200.0, 50.0]}, 'font_size': {'type': 'integer', 'default': 12}, 'color': {'type': 'array', 'items': {'type': 'number'}, 'default': [1.0, 1.0, 1.0, 1.0]}}, 'required': ['widget_name', 'text_block_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessUMGAuthoringTools.add_button_to_widget', 'description': 'Original Button8 creates Button plus separately named button_name_Text TextBlock child before RootCanvas/slot position-size/generated variable work and original structural/full compiler/force-save. Numeric arrays/defaults remain whole data. font_size/color/background_color are forwarded but ignored by original handlers. Full returned legacy/tree/child/slot/partial/compiler/save prefix is retained before errors; no Python style/geometry mutation, compiler/save coalescing, atomic cleanup, replay or UMG render/runtime/cold/full pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'widget_name': {'type': 'string'}, 'button_name': {'type': 'string'}, 'text': {'type': 'string', 'default': ''}, 'position': {'type': 'array', 'items': {'type': 'number'}, 'default': [0.0, 0.0]}, 'size': {'type': 'array', 'items': {'type': 'number'}, 'default': [200.0, 50.0]}, 'font_size': {'type': 'integer', 'default': 12}, 'color': {'type': 'array', 'items': {'type': 'number'}, 'default': [1.0, 1.0, 1.0, 1.0]}, 'background_color': {'type': 'array', 'items': {'type': 'number'}, 'default': [0.1, 0.1, 0.1, 1.0]}}, 'required': ['widget_name', 'button_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import umg_authoring as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 3,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_style_arguments_ignored": True, "native_composite_compile_save_authoritative": True,
        "actual_umg_authoring_behavior_verified": False, "full_original_UMG_render_runtime_or_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 3):
        raise RuntimeError("Exact UMGAuthoring raw3 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("UMGAuthoring method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original UMGAuthoring input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if (type(item) is not dict or json.dumps(item, sort_keys=True, allow_nan=False)
                    != json.dumps(expected["inputSchema"]["properties"][field], sort_keys=True, allow_nan=False)):
                raise RuntimeError("Original UMGAuthoring scalar/numeric-array/default type changed: " + field)
        output = rows[0].get("outputSchema")
        if (type(output) is not dict or json.dumps(output, sort_keys=True, allow_nan=False)
                != json.dumps(expected["outputSchema"], sort_keys=True, allow_nan=False)):
            raise RuntimeError("Exact standard UMGAuthoring operation output11 envelope required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "original3_7_8_numeric_arrays_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_umg_authoring_behavior_or_pipeline_verified": False}
