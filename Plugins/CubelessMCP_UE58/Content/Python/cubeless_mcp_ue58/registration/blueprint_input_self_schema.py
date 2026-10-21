"""Distinct legacy InputAction/current-owner Self bindings and original public7/6 definitions."""
TOOLSET = "CubelessMCPUE58Editor.CubelessBlueprintExpressionTools"
BINDINGS = ('add_blueprint_input_action_node', 'add_blueprint_self_reference')
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_input_action_node': ('blueprint_name', 'action_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_self_reference': ('blueprint_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
DEFAULTS_BY_METHOD = {'add_blueprint_input_action_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_self_reference': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}}
REQUIRED_BY_METHOD = {'add_blueprint_input_action_node': ('blueprint_name', 'action_name'), 'add_blueprint_self_reference': ('blueprint_name',)}


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_input_self as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "preparation_required": False, "schema_verified": False,
        "actual_official_raw_wire_verified": False, "actual_node_construction_verified": False,
        "full_original_runtime_or_mixed_pipeline_verified": False, "public_tool_count": 2,
        "public_input_counts": {BINDINGS[0]: 7, BINDINGS[1]: 6},
        "unknown_action_compile_warning_is_not_creation_rejection": True,
        "EditorSubsystem_EditorUtility_followup_unimplemented": True}


def validate_schema(schema):
    from . import blueprint_expression_schema
    verified = blueprint_expression_schema.validate_schema(schema)
    return {"schema_verified": verified["schema_verified"], "validated_tool_names": list(BINDINGS),
        "public_input_counts": {BINDINGS[0]: 7, BINDINGS[1]: 6}, "original_public_defaults_verified": True,
        "public_position_Any_schema_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_input_self_node_or_runtime_verified": False}
