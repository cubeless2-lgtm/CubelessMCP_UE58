"""Original Find6/Positions8 schemas; whole Any/row inputs remain Native-authoritative."""
import copy

TOOLSET = 'CubelessMCPUE58Editor.CubelessBlueprintQueryLayoutTools'
BINDINGS = ('find_blueprint_nodes', 'set_blueprint_node_positions')
PUBLIC_FIELDS_BY_METHOD = {'find_blueprint_nodes': ('blueprint_name', 'node_type', 'event_type', 'graph_name', 'graph_id', 'graph_type'), 'set_blueprint_node_positions': ('blueprint_name', 'graph_name', 'updates', 'graph_id', 'graph_type', 'dry_run', 'compile', 'save')}
DEFAULTS_BY_METHOD = {'find_blueprint_nodes': {'node_type': None, 'event_type': None, 'graph_name': '', 'graph_id': '', 'graph_type': ''}, 'set_blueprint_node_positions': {'graph_id': '', 'graph_type': '', 'dry_run': True, 'compile': False, 'save': False}}
REQUIRED_BY_METHOD = {'find_blueprint_nodes': ('blueprint_name',), 'set_blueprint_node_positions': ('blueprint_name', 'graph_name', 'updates')}
FIELD_KINDS_BY_METHOD = {'find_blueprint_nodes': {'blueprint_name': 'string', 'node_type': 'any', 'event_type': 'any', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string'}, 'set_blueprint_node_positions': {'blueprint_name': 'string', 'graph_name': 'string', 'updates': 'object_array', 'graph_id': 'string', 'graph_type': 'string', 'dry_run': 'boolean', 'compile': 'boolean', 'save': 'boolean'}}
BOOLEAN_FIELDS_BY_METHOD = {'find_blueprint_nodes': (), 'set_blueprint_node_positions': ('dry_run', 'compile', 'save')}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessBlueprintQueryLayoutTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessBlueprintQueryLayoutTools.find_blueprint_nodes', 'description': 'Original Find6 preserves untyped node_type/event_type defaultNull and all graph selectors. Native exact case-sensitive Event search matches EventReference member name and returns node GUIDs; default/nonstring node_type retains the original missing-type error, other string types yield an empty list. This is not a broad invented class/title filter. Original graph resolution may create a missing default EventGraph; one named journal retains its actual allocation/dirty/partial prefix and Native decides the current read/write branch; no extra compiler/save, writer retry or full runtime/cold pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'node_type': {'default': None}, 'event_type': {'default': None}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessBlueprintQueryLayoutTools.set_blueprint_node_positions', 'description': 'Original Positions8 preserves required graph_name and whole updates object array, graph_id/type empty, dry_run=True and compile/save=False. Native validates every GUID/class/FullTitle/exact integral int32 expected/target row before any coordinate write, then owns direct changed-only layout, requested compile/save even matching, re-resolution/current readback and completion failures. Full actual flags/raw/partial counts precede errors; no Python row coercion, second compiler/save, automatic retry or atomic restoration. Unique practical pipelines remain unexecuted.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'graph_name': {'type': 'string'}, 'updates': {'type': 'array', 'items': {'type': 'object', 'additionalProperties': {}}}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}, 'dry_run': {'type': 'boolean', 'default': True}, 'compile': {'type': 'boolean', 'default': False}, 'save': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name', 'graph_name', 'updates'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)



def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_query_layout as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 2,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "finder_original_default_graph_creation_native_owned": True,
        "finder_read_only_not_hardcoded": True,
        "actual_blueprint_query_layout_behavior_verified": False, "full_original_blueprint_query_layout_runtime_or_pipeline_verified": False}



def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 2):
        raise RuntimeError("Exact blueprint_query_layout raw2 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("BlueprintQueryLayout method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original blueprint_query_layout input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
                raise RuntimeError("Original blueprint_query_layout scalar type/default changed: " + field)
            if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
                raise RuntimeError("Original blueprint_query_layout default type changed: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact blueprint_query_layout operation outer envelope required")
        inner = outer["returnValue"]
        fields = inner.get("properties") if type(inner) is dict else None
        if (type(inner) is not dict or inner.get("type") != "object" or inner.get("additionalProperties") is not False
                or inner.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
                or any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact standard blueprint_query_layout operation output11 required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "public_Any_and_guarded_updates_array_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_blueprint_query_layout_behavior_or_pipeline_verified": False}
