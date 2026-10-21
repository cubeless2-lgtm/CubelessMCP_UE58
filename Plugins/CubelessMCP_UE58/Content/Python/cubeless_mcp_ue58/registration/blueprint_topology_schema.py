"""Original static topology15 whole public IO; no public operation_id."""
import copy

TOOLSET = 'CubelessMCPUE58Editor.CubelessBlueprintTopologyTools'
BINDINGS = ('inspect_blueprint_graph_call_topology',)
PUBLIC_FIELDS_BY_METHOD = {'inspect_blueprint_graph_call_topology': ('blueprint_name', 'graph_name', 'graph_id', 'graph_type', 'graph_name_contains', 'node_type', 'title_contains', 'reference_contains', 'include_pins', 'include_links', 'include_references', 'max_graphs', 'max_nodes_per_graph', 'max_links_per_graph', 'max_references_per_node')}
DEFAULTS_BY_METHOD = {'inspect_blueprint_graph_call_topology': {'graph_name': '', 'graph_id': '', 'graph_type': '', 'graph_name_contains': '', 'node_type': '', 'title_contains': '', 'reference_contains': '', 'include_pins': False, 'include_links': True, 'include_references': True, 'max_graphs': 64, 'max_nodes_per_graph': 512, 'max_links_per_graph': 1024, 'max_references_per_node': 64}}
REQUIRED_BY_METHOD = {'inspect_blueprint_graph_call_topology': ("blueprint_name",)}
FIELD_KINDS_BY_METHOD = {'inspect_blueprint_graph_call_topology': {'blueprint_name': 'string', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'graph_name_contains': 'string', 'node_type': 'string', 'title_contains': 'string', 'reference_contains': 'string', 'include_pins': 'boolean', 'include_links': 'boolean', 'include_references': 'boolean', 'max_graphs': 'integer', 'max_nodes_per_graph': 'integer', 'max_links_per_graph': 'integer', 'max_references_per_node': 'integer'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessBlueprintTopologyTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessBlueprintTopologyTools.inspect_blueprint_graph_call_topology', 'description': 'Original static graph call topology15 preserves selectors, filters, include options and -1/0/default limits. Native records classified nodes, pins, normalized links, member references and truncation counts. No Python filtering/clamping, graph creation, compiler or save. Member/title getters may resolve references, load or update caches; selected current observation and raw prefix are authoritative, not all SDK effects zero. No runtime/cold/full pipeline claim or automatic replay.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}, 'graph_name_contains': {'type': 'string', 'default': ''}, 'node_type': {'type': 'string', 'default': ''}, 'title_contains': {'type': 'string', 'default': ''}, 'reference_contains': {'type': 'string', 'default': ''}, 'include_pins': {'type': 'boolean', 'default': False}, 'include_links': {'type': 'boolean', 'default': True}, 'include_references': {'type': 'boolean', 'default': True}, 'max_graphs': {'type': 'integer', 'default': 64}, 'max_nodes_per_graph': {'type': 'integer', 'default': 512}, 'max_links_per_graph': {'type': 'integer', 'default': 1024}, 'max_references_per_node': {'type': 'integer', 'default': 64}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)



def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_topology as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "named_observer_no_mutation_journal": True,
        "SDK_getter_load_cache_effects_qualified": True,
        "actual_blueprint_topology_behavior_verified": False, "full_original_blueprint_topology_runtime_or_pipeline_verified": False}



def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 1):
        raise RuntimeError("Exact blueprint_topology raw1 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("BlueprintTopology method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original blueprint_topology input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
                raise RuntimeError("Original blueprint_topology scalar type/default changed: " + field)
            if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
                raise RuntimeError("Original blueprint_topology default type changed: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact blueprint_topology operation outer envelope required")
        inner = outer["returnValue"]
        fields = inner.get("properties") if type(inner) is dict else None
        if (type(inner) is not dict or inner.get("type") != "object" or inner.get("additionalProperties") is not False
                or inner.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
                or any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact standard blueprint_topology operation output11 required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "original15_filter_option_limit_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_blueprint_topology_behavior_or_pipeline_verified": False}
