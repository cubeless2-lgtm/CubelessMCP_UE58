"""Original fallback analyzer3 and checked native diagnostic report publication."""
import copy
import json

TOOLSET = 'CubelessMCPUE58Editor.CubelessBlueprintWidgetFallbackTools'
BINDINGS = ('analyze_blueprint_widget_fallbacks_mcp',)
PUBLIC_FIELDS_BY_METHOD = {'analyze_blueprint_widget_fallbacks_mcp': ('source_root', 'target_root', 'suffix')}
DEFAULTS_BY_METHOD = {'analyze_blueprint_widget_fallbacks_mcp': {'suffix': '_MCP'}}
REQUIRED_BY_METHOD = {'analyze_blueprint_widget_fallbacks_mcp': ('source_root', 'target_root')}
FIELD_KINDS_BY_METHOD = {'analyze_blueprint_widget_fallbacks_mcp': {'source_root': 'string', 'target_root': 'string', 'suffix': 'string'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessBlueprintWidgetFallbackTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessBlueprintWidgetFallbackTools.analyze_blueprint_widget_fallbacks_mcp', 'description': 'Original analyze_blueprint_widget_fallbacks_mcp3 preserves required source_root/target_root strings and suffix default _MCP. Native normalizes /Game roots, forced-scans Registry, pairs selected Blueprint/Widget classes and compares seven original graph/node/variable/SCS/widget counts with legacy risk summaries. verification_pass false can be a successful complete diagnostic; count matches do not certify topology, defaults, runtime or safe fresh recreation. Native owns a unique no-overwrite ProjectDir/cubeless/records/verification/blueprint-widget-fallbacks/<GUID>_report.json publication and checked UTF8 readback; original Saved/MCP suggested path remains separate provenance. This is a potential report writer with one named journal or shared Batch parent checkpoint; full raw loads/registry/counts/report/known-null prefix precedes errors. Incomplete observation or report publication is a typed failure. No Python report writes, extra compiler/package save, asset recreation, rollback/retry, cold/render/cook/full pipeline claims.', 'inputSchema': {'type': 'object', 'properties': {'source_root': {'type': 'string'}, 'target_root': {'type': 'string'}, 'suffix': {'type': 'string', 'default': '_MCP'}}, 'required': ['source_root', 'target_root'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_widget_fallback as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_public_input_count": 3, "original_suffix_default": "_MCP",
        "native_diagnostic_and_checked_report_publication_authoritative": True,
        "legacy_verification_pass_false_is_complete_diagnostic_eligible": True,
        "potential_report_writer_journal": True,
        "actual_widget_fallback_diagnostic_behavior_verified": False, "full_recreation_runtime_or_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 1):
        raise RuntimeError("Exact BlueprintWidgetFallback raw1 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("BlueprintWidgetFallback method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("BlueprintWidgetFallback input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if (type(item) is not dict or json.dumps(item, sort_keys=True, allow_nan=False)
                    != json.dumps(expected["inputSchema"]["properties"][field], sort_keys=True, allow_nan=False)):
                raise RuntimeError("BlueprintWidgetFallback native-object/scalar/default type changed: " + field)
        output = rows[0].get("outputSchema")
        if (type(output) is not dict or json.dumps(output, sort_keys=True, allow_nan=False)
                != json.dumps(expected["outputSchema"], sort_keys=True, allow_nan=False)):
            raise RuntimeError("Exact standard BlueprintWidgetFallback operation output11 envelope required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "original_fallback_analyzer3_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_widget_fallback_diagnostic_behavior_or_pipeline_verified": False}
