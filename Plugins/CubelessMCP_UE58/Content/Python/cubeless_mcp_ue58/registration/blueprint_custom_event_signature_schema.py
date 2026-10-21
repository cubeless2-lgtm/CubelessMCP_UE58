"""Original CustomEvent12 raw alias; distinct from existing Graph13 adapted IO."""
import copy
TOOLSET = 'CubelessMCPUE58Editor.CubelessBlueprintEventTools'
BINDINGS = ('add_blueprint_custom_event_node',)
PUBLIC_FIELDS = ('blueprint_name', 'custom_event_name', 'node_position', 'signature_source_dispatcher_name', 'signature_source_class', 'signature_source_delegate_name', 'native_delegate_name', 'call_in_editor', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')
DEFAULTS = {'node_position': None, 'signature_source_dispatcher_name': '', 'signature_source_class': '', 'signature_source_delegate_name': '', 'native_delegate_name': '', 'call_in_editor': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessBlueprintEventTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessBlueprintEventTools.add_blueprint_custom_event_node', 'description': 'Exact original CustomEvent12 with Any node_position/defaultNull, empty signature and graph defaults, native alias precedence and nullable Call In Editor. Shares Native typed signature/current pins/factory/grid/reuse/partial-prefix logic with distinct existing Graph13 adapted signature route; Graph13 parameterless API stays unchanged. No input-13 equivalence, new original-feature count, explicit compiler/save/automatic replay or full runtime/cold/pipeline claim. Also admits an explicit CustomEvent in the bounded already-loaded direct-UserWidget WidgetBlueprint profile with empty Canvas root and existing EventGraph, including dirty hosts. Raw12 supports no signature or admitted primitive inputs from the selected own dispatcher, preserving source precedence, reuse and nullable Call In Editor presence. Native owns actual typed pins, event flags, grid/structural effects and current class reacquisition. Wider native-delegate sources, missing Widget graphs and runtime callbacks remain incomplete.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'custom_event_name': {'type': 'string'}, 'node_position': {'default': None}, 'signature_source_dispatcher_name': {'type': 'string', 'default': ''}, 'signature_source_class': {'type': 'string', 'default': ''}, 'signature_source_delegate_name': {'type': 'string', 'default': ''}, 'native_delegate_name': {'type': 'string', 'default': ''}, 'call_in_editor': {'type': ['boolean', 'null'], 'default': None}, 'graph_name': {'type': 'string', 'default': ''}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': ''}, 'create_graph_if_missing': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name', 'custom_event_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}


def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import custom_event_signatures as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "hidden_method": api.HIDDEN_METHOD, "hidden_input_count": 13, "public_input_count": 12,
        "public_toolset": TOOLSET, "public_tool_count": 1, "original_new_feature_count": 0,
        "preparation_required": False, "registered": False, "schema_verified": False,
        "actual_official_raw_wire_verified": False, "actual_typed_event_or_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or type(schema["tools"][0]) is not dict
            or schema["tools"][0].get("name") != TOOLSET + "." + BINDINGS[0]):
        raise RuntimeError("Exact original CustomEvent12 fixed named schema required")
    row = schema["tools"][0]
    inputs = row.get("inputSchema")
    output = row.get("outputSchema")
    returned = output.get("properties", {}).get("returnValue") if type(output) is dict and type(output.get("properties")) is dict else None
    if (type(inputs) is not dict or inputs.get("additionalProperties") is not False
            or type(output) is not dict or output.get("additionalProperties") is not False
            or type(returned) is not dict or returned.get("additionalProperties") is not False):
        raise RuntimeError("Exact input and standard operation outer/inner closed envelopes required")
    if row.get("inputSchema") != _PUBLIC_SCHEMA["tools"][0]["inputSchema"]:
        raise RuntimeError("Original CustomEvent12 input/default/Any/nullable flag schema differs")
    properties = row["inputSchema"]["properties"]
    if tuple(properties) != PUBLIC_FIELDS or any(type(properties[k].get("default")) is not type(v) for k, v in DEFAULTS.items()):
        raise RuntimeError("Original CustomEvent12 ordered fields or exact default types differ")
    if row.get("outputSchema") != _PUBLIC_SCHEMA["tools"][0]["outputSchema"]:
        raise RuntimeError("Exact standard11 CustomEvent operation envelope required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_count": 12, "hidden_input_count": 13, "existing_adapted_Graph_input_count": 13,
        "interfaces_not_claimed_IO_equivalent": True, "original_new_feature_count": 0,
        "actual_official_raw_wire_verified": False, "actual_typed_event_or_pipeline_verified": False}
