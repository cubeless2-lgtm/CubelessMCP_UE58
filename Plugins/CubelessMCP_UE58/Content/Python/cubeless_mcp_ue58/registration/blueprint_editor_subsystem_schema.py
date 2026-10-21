"""Distinct EditorSubsystem native binding and original public7 for EditorUtility owners."""
TOOLSET = "CubelessMCPUE58Editor.CubelessBlueprintExpressionTools"
BINDINGS = ('add_blueprint_get_editor_subsystem_node',)
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_get_editor_subsystem_node': ('blueprint_name', 'subsystem_class', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
DEFAULTS_BY_METHOD = {'add_blueprint_get_editor_subsystem_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}}
REQUIRED_BY_METHOD = {'add_blueprint_get_editor_subsystem_node': ('blueprint_name', 'subsystem_class')}


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_editor_subsystem as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "preparation_required": False, "schema_verified": False,
        "actual_official_raw_wire_verified": False, "actual_node_construction_verified": False,
        "full_original_EditorUtility_workflow_verified": False, "public_tool_count": 1,
        "public_input_counts": {BINDINGS[0]: 7}, "asset_scope": "EditorUtility/editor-only; no game asset equivalence"}


def validate_schema(schema):
    from . import blueprint_expression_schema
    verified = blueprint_expression_schema.validate_schema(schema)
    return {"schema_verified": verified["schema_verified"], "validated_tool_names": list(BINDINGS),
        "public_input_counts": {BINDINGS[0]: 7}, "original_public_defaults_verified": True,
        "public_position_Any_schema_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_EditorUtility_node_or_workflow_verified": False}
