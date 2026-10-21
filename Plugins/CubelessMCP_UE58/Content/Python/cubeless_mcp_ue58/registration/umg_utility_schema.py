"""Named native checkbox params adapter1 and original viewport readiness public2."""
import copy
import json

TOOLSET = 'CubelessMCPUE58Editor.CubelessUMGUtilityTools'
BINDINGS = ('add_checkbox_to_widget', 'add_widget_to_viewport')
PUBLIC_FIELDS_BY_METHOD = {'add_checkbox_to_widget': ('params',), 'add_widget_to_viewport': ('widget_name', 'z_order')}
DEFAULTS_BY_METHOD = {'add_checkbox_to_widget': {}, 'add_widget_to_viewport': {'z_order': 0}}
REQUIRED_BY_METHOD = {'add_checkbox_to_widget': ('params',), 'add_widget_to_viewport': ('widget_name',)}
FIELD_KINDS_BY_METHOD = {'add_checkbox_to_widget': {'params': 'native_params_object'}, 'add_widget_to_viewport': {'widget_name': 'string', 'z_order': 'integer'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessUMGUtilityTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessUMGUtilityTools.add_checkbox_to_widget', 'description': 'Named native-command checkbox object adapter1; no original Python wrapper or public arity is claimed. Whole params object preserves target aliases blueprint_name/widget_blueprint/widget_path/widget_name/name, component aliases checkbox_name/component_name/widget_component_name, checked/text/position/size and ignored unknown fields. Native owns alias order, checked fallback, first-two geometry when array length>=2, missing geometry SDK slot defaults, optional sibling <Name>_Label (not checkbox content), original non-Canvas root replacement and actual allocation/current root loss/compiler/forced-save prefix. No Python geometry/default/coercion, automatic rollback/replay or runtime/cold/full pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'params': {'type': 'object', 'additionalProperties': True}}, 'required': ['params'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessUMGUtilityTools.add_widget_to_viewport', 'description': 'Original add_widget_to_viewport2 preserves widget_name and z_order default0. Native returns GeneratedClass readiness and the checked int32 z_order/note without creating a widget instance or adding it to any viewport. Original dispatcher transaction and asset/class loads remain actual observations; no Modify, compiler, save or UI/runtime effect is inferred. Unknown prefix retained; no regeneration, runtime invocation or full pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'widget_name': {'type': 'string'}, 'z_order': {'type': 'integer', 'default': 0}}, 'required': ['widget_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import umg_utility as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 2,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "checkbox_native_command_adapter_public_count": 1, "original_viewport_public_count": 2,
        "native_checkbox_compile_save_and_viewport_class_readiness_authoritative": True,
        "actual_umg_utility_behavior_verified": False, "full_original_UMG_interaction_runtime_or_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 2):
        raise RuntimeError("Exact UMGUtility raw2 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("UMGUtility method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("UMGUtility input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if (type(item) is not dict or json.dumps(item, sort_keys=True, allow_nan=False)
                    != json.dumps(expected["inputSchema"]["properties"][field], sort_keys=True, allow_nan=False)):
                raise RuntimeError("UMGUtility native-object/scalar/default type changed: " + field)
        output = rows[0].get("outputSchema")
        if (type(output) is not dict or json.dumps(output, sort_keys=True, allow_nan=False)
                != json.dumps(expected["outputSchema"], sort_keys=True, allow_nan=False)):
            raise RuntimeError("Exact standard UMGUtility operation output11 envelope required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "native_checkbox_object_and_original_viewport2_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_umg_utility_behavior_or_pipeline_verified": False}
