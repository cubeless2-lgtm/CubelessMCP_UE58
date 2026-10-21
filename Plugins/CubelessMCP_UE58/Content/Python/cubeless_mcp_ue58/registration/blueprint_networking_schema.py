"""Original Inspect1/Set11 strict nullable networking contracts on the official raw2 toolset."""
import copy

TOOLSET = 'CubelessMCPUE58Editor.CubelessBlueprintNetworkingTools'
BINDINGS = ('inspect_blueprint_networking_settings', 'set_blueprint_networking_settings')
PUBLIC_FIELDS_BY_METHOD = {'inspect_blueprint_networking_settings': ('blueprint_name',), 'set_blueprint_networking_settings': ('blueprint_name', 'replicates', 'replicate_movement', 'net_dormancy', 'always_relevant', 'only_relevant_to_owner', 'net_update_frequency', 'min_net_update_frequency', 'net_priority', 'net_cull_distance_squared', 'dry_run')}
DEFAULTS_BY_METHOD = {'inspect_blueprint_networking_settings': {}, 'set_blueprint_networking_settings': {'replicates': None, 'replicate_movement': None, 'net_dormancy': None, 'always_relevant': None, 'only_relevant_to_owner': None, 'net_update_frequency': None, 'min_net_update_frequency': None, 'net_priority': None, 'net_cull_distance_squared': None, 'dry_run': True}}
REQUIRED_BY_METHOD = {'inspect_blueprint_networking_settings': ('blueprint_name',), 'set_blueprint_networking_settings': ('blueprint_name',)}
FIELD_KINDS_BY_METHOD = {'inspect_blueprint_networking_settings': {'blueprint_name': 'string'}, 'set_blueprint_networking_settings': {'blueprint_name': 'string', 'replicates': 'nullable_boolean', 'replicate_movement': 'nullable_boolean', 'net_dormancy': 'nullable_string', 'always_relevant': 'nullable_boolean', 'only_relevant_to_owner': 'nullable_boolean', 'net_update_frequency': 'nullable_number', 'min_net_update_frequency': 'nullable_number', 'net_priority': 'nullable_number', 'net_cull_distance_squared': 'nullable_number', 'dry_run': 'boolean'}}
BOOLEAN_FIELDS_BY_METHOD = {'inspect_blueprint_networking_settings': (), 'set_blueprint_networking_settings': ('dry_run',)}
NULLABLE_BOOLEAN_FIELDS = ('replicates', 'replicate_movement', 'always_relevant', 'only_relevant_to_owner')
NULLABLE_NUMBER_FIELDS = ('net_update_frequency', 'min_net_update_frequency', 'net_priority', 'net_cull_distance_squared')
NULLABLE_STRING_FIELDS = ('net_dormancy',)
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessBlueprintNetworkingTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessBlueprintNetworkingTools.inspect_blueprint_networking_settings', 'description': 'Original Inspect1 reads current Actor Blueprint CDO networking fields, generated class/status and dirty state through Native public API. Load/CDO creation and callback effects are scoped observations, not total side-effect-free proof. No explicit compiler/save or mutation journal; runtime multiplayer/cold behavior remains unverified.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessBlueprintNetworkingTools.set_blueprint_networking_settings', 'description': 'Original Set11 preserves nine nullable strict JSON fields and dry_run=True; null/omission preserves unspecified values. Native owns original dormancy/numeric/relevancy validation, changed-field public setters, compile/reacquire/readback, dependency guards and forced save. Dry or matching skips compiler/save; real partial/current/compiler/saved flags and raw receipts precede failures. No public compile/save extras, second dispatch, atomic restore, automatic retry or multiplayer/cold/pipeline completion claim.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'replicates': {'type': ['boolean', 'null'], 'default': None}, 'replicate_movement': {'type': ['boolean', 'null'], 'default': None}, 'net_dormancy': {'type': ['string', 'null'], 'default': None}, 'always_relevant': {'type': ['boolean', 'null'], 'default': None}, 'only_relevant_to_owner': {'type': ['boolean', 'null'], 'default': None}, 'net_update_frequency': {'type': ['number', 'null'], 'default': None}, 'min_net_update_frequency': {'type': ['number', 'null'], 'default': None}, 'net_priority': {'type': ['number', 'null'], 'default': None}, 'net_cull_distance_squared': {'type': ['number', 'null'], 'default': None}, 'dry_run': {'type': 'boolean', 'default': True}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)



def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_networking as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 2,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "actual_blueprint_networking_behavior_verified": False, "full_original_blueprint_networking_runtime_or_pipeline_verified": False}



def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 2):
        raise RuntimeError("Exact blueprint_networking raw2 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("BlueprintNetworking method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original blueprint_networking input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
                raise RuntimeError("Original blueprint_networking scalar type/default changed: " + field)
            if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
                raise RuntimeError("Original blueprint_networking default type changed: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact blueprint_networking operation outer envelope required")
        inner = outer["returnValue"]
        fields = inner.get("properties") if type(inner) is dict else None
        if (type(inner) is not dict or inner.get("type") != "object" or inner.get("additionalProperties") is not False
                or inner.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
                or any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact standard blueprint_networking operation output11 required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "public_strict_nullable_types_and_exact_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_blueprint_networking_behavior_or_pipeline_verified": False}
