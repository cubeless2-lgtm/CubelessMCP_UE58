"""Separate native3 lifecycle readiness for the same fixed Dispatcher public raw6."""
from . import blueprint_dispatcher_schema as dispatcher_schema
TOOLSET = dispatcher_schema.TOOLSET
BINDINGS = ('add_blueprint_event_dispatcher_unbind_node', 'add_blueprint_event_dispatcher_clear_node', 'add_blueprint_event_dispatcher_assign_node')
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_event_dispatcher_unbind_node': ('blueprint_name', 'dispatcher_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_event_dispatcher_clear_node': ('blueprint_name', 'dispatcher_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_event_dispatcher_assign_node': ('blueprint_name', 'dispatcher_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
DEFAULTS_BY_METHOD = {'add_blueprint_event_dispatcher_unbind_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_event_dispatcher_clear_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_event_dispatcher_assign_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}}
REQUIRED_BY_METHOD = {'add_blueprint_event_dispatcher_unbind_node': ('blueprint_name', 'dispatcher_name'), 'add_blueprint_event_dispatcher_clear_node': ('blueprint_name', 'dispatcher_name'), 'add_blueprint_event_dispatcher_assign_node': ('blueprint_name', 'dispatcher_name')}


def public_schema():
    return dispatcher_schema.public_schema()


def validate_schema(schema):
    result = dispatcher_schema.validate_schema(schema)
    result.update(new_native_methods=list(BINDINGS), lifecycle_native_gate_separate=True)
    return result


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_dispatcher_lifecycle as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 6, "native_input_counts": {m: 7 for m in BINDINGS},
        "preparation_required": False, "registered": False, "schema_verified": False,
        "actual_official_raw_wire_verified": False, "actual_compound_lifecycle_or_runtime_verified": False}
