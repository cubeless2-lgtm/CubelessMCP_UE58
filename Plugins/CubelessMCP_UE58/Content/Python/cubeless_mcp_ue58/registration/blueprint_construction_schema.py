"""Distinct subsystem-from-controller/construct-object bindings and original public7/7 definitions."""
TOOLSET = "CubelessMCPUE58Editor.CubelessBlueprintExpressionTools"
BINDINGS = ('add_blueprint_get_subsystem_from_pc_node', 'add_blueprint_construct_object_node')
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_get_subsystem_from_pc_node': ('blueprint_name', 'subsystem_class', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_construct_object_node': ('blueprint_name', 'object_class', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
DEFAULTS_BY_METHOD = {'add_blueprint_get_subsystem_from_pc_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_construct_object_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}}
REQUIRED_BY_METHOD = {'add_blueprint_get_subsystem_from_pc_node': ('blueprint_name', 'subsystem_class'), 'add_blueprint_construct_object_node': ('blueprint_name', 'object_class')}


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_construction as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "preparation_required": False, "schema_verified": False,
        "actual_official_raw_wire_verified": False, "actual_node_construction_verified": False,
        "full_original_runtime_or_mixed_pipeline_verified": False, "public_tool_count": 2,
        "public_input_counts": {method: 7 for method in BINDINGS},
        "EditorSubsystem_EditorUtility_followup_unimplemented": True}


def validate_schema(schema):
    from . import blueprint_expression_schema
    verified = blueprint_expression_schema.validate_schema(schema)
    return {"schema_verified": verified["schema_verified"], "validated_tool_names": list(BINDINGS),
        "public_input_counts": {method: 7 for method in BINDINGS}, "original_public_defaults_verified": True,
        "public_position_Any_schema_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_class_default_restoration_retirement_or_runtime_verified": False}
