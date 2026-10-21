"""Distinct new Compare/Literal bindings and original public8/8 definitions."""
TOOLSET = "CubelessMCPUE58Editor.CubelessBlueprintExpressionTools"
BINDINGS = ("add_blueprint_compare_node", "add_blueprint_literal_node")
PUBLIC_FIELDS_BY_METHOD = {
    BINDINGS[0]: ("blueprint_name", "operation", "value_type", "node_position", "graph_name", "graph_id", "graph_type", "create_graph_if_missing"),
    BINDINGS[1]: ("blueprint_name", "literal_type", "value", "node_position", "graph_name", "graph_id", "graph_type", "create_graph_if_missing")}
DEFAULTS_BY_METHOD = {
    BINDINGS[0]: {"value_type": "double", "node_position": None, "graph_name": "", "graph_id": "", "graph_type": "", "create_graph_if_missing": False},
    BINDINGS[1]: {"value": None, "node_position": None, "graph_name": "", "graph_id": "", "graph_type": "", "create_graph_if_missing": False}}
REQUIRED_BY_METHOD = {BINDINGS[0]: ("blueprint_name", "operation"), BINDINGS[1]: ("blueprint_name", "literal_type")}


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_value as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": "CubelessMCPUE58Editor.CubelessBlueprintExpressionTools", "preparation_required": False,
        "schema_verified": False, "actual_official_raw_wire_verified": False, "actual_node_construction_verified": False,
        "full_original_runtime_or_mixed_pipeline_verified": False, "public_tool_count": 2,
        "public_input_counts": {BINDINGS[0]: 8, BINDINGS[1]: 8},
        "literal_float_single_reflected_UFunction_equivalence_verified": False}


def validate_schema(schema):
    from . import blueprint_expression_schema
    verified = blueprint_expression_schema.validate_schema(schema)
    return {"schema_verified": verified["schema_verified"], "validated_tool_names": list(BINDINGS),
        "public_input_counts": {BINDINGS[0]: 8, BINDINGS[1]: 8}, "original_public_defaults_verified": True,
        "public_value_and_position_Any_schema_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_node_default_retirement_or_runtime_verified": False}
