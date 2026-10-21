"""Original annotation public15/8/5 schema and separate native readiness; no reflected default preparation."""
import copy

TOOLSET = 'CubelessMCPUE58Editor.CubelessBlueprintAnnotationTools'
BINDINGS = ('add_blueprint_comment_box', 'set_blueprint_node_comment', 'set_blueprint_function_description')
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_comment_box': ('blueprint_name', 'comment_text', 'node_position', 'size', 'comment_color', 'font_size', 'move_mode', 'comment_bubble_visible', 'color_comment_bubble', 'node_details', 'node_ids', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'set_blueprint_node_comment': ('blueprint_name', 'node_id', 'node_comment', 'comment_bubble_visible', 'comment_bubble_pinned', 'graph_name', 'graph_id', 'graph_type'), 'set_blueprint_function_description': ('blueprint_name', 'description', 'graph_name', 'graph_id', 'graph_type')}
DEFAULTS_BY_METHOD = {'add_blueprint_comment_box': {'node_position': None, 'size': None, 'comment_color': None, 'font_size': 18, 'move_mode': 'group', 'comment_bubble_visible': False, 'color_comment_bubble': False, 'node_details': '', 'node_ids': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'set_blueprint_node_comment': {'comment_bubble_visible': True, 'comment_bubble_pinned': False, 'graph_name': '', 'graph_id': '', 'graph_type': ''}, 'set_blueprint_function_description': {'graph_name': '', 'graph_id': '', 'graph_type': 'function'}}
REQUIRED_BY_METHOD = {'add_blueprint_comment_box': ('blueprint_name', 'comment_text'), 'set_blueprint_node_comment': ('blueprint_name', 'node_id', 'node_comment'), 'set_blueprint_function_description': ('blueprint_name', 'description')}
NUMERIC_ARRAY_FIELDS = ('node_position', 'size', 'comment_color')
BOOLEAN_FIELDS = ('comment_bubble_visible', 'color_comment_bubble', 'comment_bubble_pinned', 'create_graph_if_missing')
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessBlueprintAnnotationTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessBlueprintAnnotationTools.add_blueprint_comment_box', 'description': 'Original comment-box geometry/style/live grouping authoring. Native reapplies text/style after lifecycle, preserves tolerant grouping and actual structural-mark/partial receipt. Grouping is transient live state, not saved/cold persistence; explicit compile/save are absent while structural mark may regenerate skeleton synchronously.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'comment_text': {'type': 'string'}, 'node_position': {'type': ['array', 'null'], 'items': {'type': 'number'}, 'default': None}, 'size': {'type': ['array', 'null'], 'items': {'type': 'number'}, 'default': None}, 'comment_color': {'type': ['array', 'null'], 'items': {'type': 'number'}, 'default': None}, 'font_size': {'type': 'integer', 'default': 18}, 'move_mode': {'type': 'string', 'default': 'group'}, 'comment_bubble_visible': {'type': 'boolean', 'default': False}, 'color_comment_bubble': {'type': 'boolean', 'default': False}, 'node_details': {'type': 'string', 'default': ''}, 'node_ids': {'type': ['array', 'null'], 'items': {'type': 'string'}, 'default': None}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}, 'create_graph_if_missing': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name', 'comment_text'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessBlueprintAnnotationTools.set_blueprint_node_comment', 'description': 'Original existing-node comment and visible/pinned bubble setter, including matching writes and current source/graph/pin receipt. Comment-box Details visibility/style/grouping are not synchronized by this setter. No explicit compile/save, replay or runtime/cold/pipeline equivalence claim.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'node_id': {'type': 'string'}, 'node_comment': {'type': 'string'}, 'comment_bubble_visible': {'type': 'boolean', 'default': True}, 'comment_bubble_pinned': {'type': 'boolean', 'default': False}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}}, 'required': ['blueprint_name', 'node_id', 'node_comment'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessBlueprintAnnotationTools.set_blueprint_function_description', 'description': 'Original function-entry description setter, storing both ToolTip FText and tooltip metadata with current graph/pin/source receipt. Generated UFunction/UI tooltip may remain stale until compile. No implicit graph creation, explicit compile/save, replay or full function/runtime/pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'description': {'type': 'string'}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}}, 'required': ['blueprint_name', 'description'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_annotation as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 3,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "actual_annotation_behavior_verified": False, "live_grouping_saved_cold_persistence_verified": False,
        "full_original_annotation_function_or_external_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 3):
        raise RuntimeError("Exact annotation raw3 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("Annotation method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original annotation input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
                raise RuntimeError("Original annotation type/nullable-array/default changed: " + field)
            if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
                raise RuntimeError("Original annotation default type changed: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact annotation operation outer envelope required")
        inner = outer["returnValue"]
        fields = inner.get("properties") if type(inner) is dict else None
        if (type(inner) is not dict or inner.get("type") != "object" or inner.get("additionalProperties") is not False
                or inner.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
                or any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact standard annotation operation output11 required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "original_nullable_numeric_and_string_array_schema_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_annotation_behavior_or_pipeline_verified": False}
