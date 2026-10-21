"""Original permissive component-reference7; standard operation11 envelope."""
import copy

TOOLSET = 'CubelessMCPUE58Editor.CubelessBlueprintComponentReferenceTools'
BINDINGS = ('add_blueprint_get_self_component_reference',)
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_get_self_component_reference': ('blueprint_name', 'component_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
DEFAULTS_BY_METHOD = {'add_blueprint_get_self_component_reference': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}}
REQUIRED_BY_METHOD = {'add_blueprint_get_self_component_reference': ("blueprint_name", "component_name")}
FIELD_KINDS_BY_METHOD = {'add_blueprint_get_self_component_reference': {'blueprint_name': 'string', 'component_name': 'string', 'node_position': 'any', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'create_graph_if_missing': 'boolean'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessBlueprintComponentReferenceTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessBlueprintComponentReferenceTools.add_blueprint_get_self_component_reference', 'description': 'Original permissive SelfComponentReference7 directly creates Engine VariableGet and SetSelfMember with the literal component name, loose Any position and original graph selectors. None position only normalizes to origin. Empty or unresolved names remain authorable; no adapter component/property-existence or nonempty-output-pin guard. Native owns default-graph creation, AddNode(userActionFalse), GUID/Post/Pins/Reconstruct/Mark and current graph/node/source proofs. Actual full raw/accepted/unknown prefixes precede errors; no extra compiler/save, cleanup, atomic rollback or automatic replay. Property resolution and compiler/runtime evaluation are separate unverified outcomes.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'component_name': {'type': 'string'}, 'node_position': {'default': None}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}, 'create_graph_if_missing': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name', 'component_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)



def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_component_reference as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "named_writer_one_journal": True,
        "permissive_missing_component_authoring_native_owned": True,
        "actual_blueprint_component_reference_behavior_verified": False, "full_original_blueprint_component_reference_runtime_or_pipeline_verified": False}



def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 1):
        raise RuntimeError("Exact blueprint_component_reference raw1 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("BlueprintComponentReference method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original blueprint_component_reference input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
                raise RuntimeError("Original blueprint_component_reference scalar type/default changed: " + field)
            if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
                raise RuntimeError("Original blueprint_component_reference default type changed: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact blueprint_component_reference operation outer envelope required")
        inner = outer["returnValue"]
        fields = inner.get("properties") if type(inner) is dict else None
        if (type(inner) is not dict or inner.get("type") != "object" or inner.get("additionalProperties") is not False
                or inner.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
                or any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact standard blueprint_component_reference operation output11 required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "original7_Any_position_graph_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_blueprint_component_reference_behavior_or_pipeline_verified": False}
