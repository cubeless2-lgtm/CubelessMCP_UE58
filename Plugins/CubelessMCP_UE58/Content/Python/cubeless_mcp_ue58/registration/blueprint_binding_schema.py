"""Exact original NativeBind8/CreateDelegate11/InterfaceMessage10 schema and native readiness."""
import copy

TOOLSET = 'CubelessMCPUE58Editor.CubelessBlueprintBindingTools'
BINDINGS = ('add_blueprint_native_delegate_bind_node', 'add_blueprint_create_delegate_node', 'add_blueprint_interface_message_node')
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_native_delegate_bind_node': ('blueprint_name', 'delegate_owner_class', 'delegate_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_create_delegate_node': ('blueprint_name', 'function_name', 'object_node_id', 'delegate_target_node_id', 'object_pin_name', 'delegate_target_pin_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_interface_message_node': ('blueprint_name', 'interface_class', 'function_name', 'target_node_id', 'target_pin_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
DEFAULTS_BY_METHOD = {'add_blueprint_native_delegate_bind_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_create_delegate_node': {'object_pin_name': 'ReturnValue', 'delegate_target_pin_name': 'Delegate', 'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_interface_message_node': {'target_pin_name': 'ReturnValue', 'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}}
REQUIRED_BY_METHOD = {'add_blueprint_native_delegate_bind_node': ('blueprint_name', 'delegate_owner_class', 'delegate_name'), 'add_blueprint_create_delegate_node': ('blueprint_name', 'function_name', 'object_node_id', 'delegate_target_node_id'), 'add_blueprint_interface_message_node': ('blueprint_name', 'interface_class', 'function_name', 'target_node_id')}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessBlueprintBindingTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessBlueprintBindingTools.add_blueprint_native_delegate_bind_node', 'description': 'Original native BlueprintAssignable delegate bind node with requested external owner class, Any position and graph selectors. Native retains requested owner even inherited property, selfFalse context, typed Delegate and structural partial prefix. No runtime event binding, atomic rollback, explicit compiler/save or automatic replay claim.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'delegate_owner_class': {'type': 'string'}, 'delegate_name': {'type': 'string'}, 'node_position': {'default': None}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}, 'create_graph_if_missing': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name', 'delegate_owner_class', 'delegate_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessBlueprintBindingTools.add_blueprint_create_delegate_node', 'description': 'Original Create Event delegate node with object-output and delegate-input wiring, literal selected function name, Any position and graph selectors. Native preserves ordered connection/default/HandleAnyChange and only-new-node cleanup prefixes; selected-name authoring is not early validated callable or runtime invocation proof. No explicit compile/save, automatic replay or atomic endpoint restoration.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'function_name': {'type': 'string'}, 'object_node_id': {'type': 'string'}, 'delegate_target_node_id': {'type': 'string'}, 'object_pin_name': {'type': 'string', 'default': 'ReturnValue'}, 'delegate_target_pin_name': {'type': 'string', 'default': 'Delegate'}, 'node_position': {'default': None}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}, 'create_graph_if_missing': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name', 'function_name', 'object_node_id', 'delegate_target_node_id'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessBlueprintBindingTools.add_blueprint_interface_message_node', 'description': 'Original interface Message node with interface class/function and supplied target output, Any position and graph selectors. Native owns actual Message family, class/function identity, late target connection and only-new-node cleanup. No runtime interface execution, explicit compiler/save, replay or atomic link restoration claim.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'interface_class': {'type': 'string'}, 'function_name': {'type': 'string'}, 'target_node_id': {'type': 'string'}, 'target_pin_name': {'type': 'string', 'default': 'ReturnValue'}, 'node_position': {'default': None}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}, 'create_graph_if_missing': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name', 'interface_class', 'function_name', 'target_node_id'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}



def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)



def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_binding as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 3,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "actual_binding_behavior_verified": False, "full_original_binding_runtime_or_pipeline_verified": False}



def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 3):
        raise RuntimeError("Exact binding raw3 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("Binding method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original binding input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
                raise RuntimeError("Original binding type/position/default changed: " + field)
            if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
                raise RuntimeError("Original binding default type changed: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact binding operation outer envelope required")
        inner = outer["returnValue"]
        fields = inner.get("properties") if type(inner) is dict else None
        if (type(inner) is not dict or inner.get("type") != "object" or inner.get("additionalProperties") is not False
                or inner.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
                or any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact standard binding operation output11 required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "public_position_Any_and_exact_string_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_binding_behavior_or_pipeline_verified": False}
