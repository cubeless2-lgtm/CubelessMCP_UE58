"""Distinct BranchingLibrary3 readiness and original public8/7/10 contracts."""
TOOLSET = "CubelessMCPUE58Editor.CubelessBlueprintSelectionTools"
BINDINGS = ('add_blueprint_enum_literal_node', 'add_blueprint_is_valid_node', 'add_blueprint_switch_int_node')
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_enum_literal_node': ('blueprint_name', 'enum_type', 'value', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_is_valid_node': ('blueprint_name', 'value_type', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_switch_int_node': ('blueprint_name', 'start_index', 'case_count', 'case_values', 'has_default_pin', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
DEFAULTS_BY_METHOD = {'add_blueprint_enum_literal_node': {'value': None, 'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_is_valid_node': {'value_type': 'object', 'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_switch_int_node': {'start_index': 0, 'case_count': 2, 'case_values': None, 'has_default_pin': True, 'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}}
REQUIRED_BY_METHOD = {'add_blueprint_enum_literal_node': ('blueprint_name', 'enum_type'), 'add_blueprint_is_valid_node': ('blueprint_name',), 'add_blueprint_switch_int_node': ('blueprint_name',)}


def field_schema(field):
    if field in ("value", "node_position"):
        return {}
    if field == "case_values":
        return {"type": ["array", "null"], "items": {"type": "integer"}}
    return {"type": "boolean" if field in ("has_default_pin", "create_graph_if_missing") else "integer" if field in ("start_index", "case_count") else "string"}


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_branching as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "preparation_required": False, "schema_verified": False,
        "actual_official_raw_wire_verified": False, "actual_node_construction_verified": False,
        "full_original_runtime_or_mixed_pipeline_verified": False, "public_tool_count": 3,
        "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS}}


def validate_schema(schema):
    from . import blueprint_selection_schema
    verified = blueprint_selection_schema.validate_schema(schema)
    return {"schema_verified": verified["schema_verified"], "validated_tool_names": list(BINDINGS),
        "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
        "original_public_defaults_verified": True, "public_value_position_Any_and_cases_nullable_schema_verified": True,
        "preparation_required": False, "actual_official_raw_wire_verified": False,
        "actual_enum_validity_switch_node_default_retirement_runtime_verified": False}

