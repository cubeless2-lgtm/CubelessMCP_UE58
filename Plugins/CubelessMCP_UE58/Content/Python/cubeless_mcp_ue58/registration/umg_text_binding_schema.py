"""Original Text binding public4 and selected actual property-path repair surface."""
import copy
import json

TOOLSET = 'CubelessMCPUE58Editor.CubelessUMGTextBindingTools'
BINDINGS = ('set_text_block_binding',)
PUBLIC_FIELDS_BY_METHOD = {'set_text_block_binding': ('widget_name', 'text_block_name', 'binding_property', 'binding_type')}
DEFAULTS_BY_METHOD = {'set_text_block_binding': {'binding_type': 'Text'}}
REQUIRED_BY_METHOD = {'set_text_block_binding': ('widget_name', 'text_block_name', 'binding_property')}
FIELD_KINDS_BY_METHOD = {'set_text_block_binding': {'widget_name': 'string', 'text_block_name': 'string', 'binding_property': 'string', 'binding_type': 'string'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessUMGTextBindingTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessUMGTextBindingTools.set_text_block_binding', 'description': 'Original set_text_block_binding4 preserves widget_name/text_block_name/binding_property and binding_type string defaultText. Explicit functional repair: the original wrapper sends incompatible native keys and the original native path never writes WidgetBlueprint.Bindings. Native now owns selected existing/new FText source, public FEditorPropertyPath KindProperty Text binding/editor row/generated runtime binding proof and actual full compiler plus original force-save receipts. Literal binding_type provenance is retained; no Visibility or arbitrary binding semantics are inferred. No fabricated Get<property> graph artifact, Python member/binding writes, extra compiler/Save, rollback/replay or runtime visible Text/cold/cook/full pipeline claim. Original sources/failure history and binding_name response identifier remain distinct from repaired finite current proof.', 'inputSchema': {'type': 'object', 'properties': {'widget_name': {'type': 'string'}, 'text_block_name': {'type': 'string'}, 'binding_property': {'type': 'string'}, 'binding_type': {'type': 'string', 'default': 'Text'}}, 'required': ['widget_name', 'text_block_name', 'binding_property'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import umg_text_binding as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_public_input_count": 4, "original_public_binding_type_default": "Text",
        "native_property_path_Text_binding_compile_save_authoritative": True,
        "original_key_mismatch_and_missing_Bindings_functional_repair": True,
        "actual_umg_text_binding_behavior_verified": False, "full_Text_binding_runtime_or_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 1):
        raise RuntimeError("Exact UMGTextBinding raw1 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("UMGTextBinding method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("UMGTextBinding input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if (type(item) is not dict or json.dumps(item, sort_keys=True, allow_nan=False)
                    != json.dumps(expected["inputSchema"]["properties"][field], sort_keys=True, allow_nan=False)):
                raise RuntimeError("UMGTextBinding native-object/scalar/default type changed: " + field)
        output = rows[0].get("outputSchema")
        if (type(output) is not dict or json.dumps(output, sort_keys=True, allow_nan=False)
                != json.dumps(expected["outputSchema"], sort_keys=True, allow_nan=False)):
            raise RuntimeError("Exact standard UMGTextBinding operation output11 envelope required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "original_Text_binding4_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_umg_text_binding_behavior_or_pipeline_verified": False}
