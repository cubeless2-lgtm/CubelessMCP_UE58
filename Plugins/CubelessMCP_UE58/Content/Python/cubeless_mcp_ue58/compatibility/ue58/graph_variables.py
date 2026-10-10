"""UE58 own-member node spawners and the public C++ preflight/readback bridge."""

import unreal

from . import graph
from cubeless_mcp_ue58.operations.state import OperationError


def member_plan(editor, variable_name, setter, target_class=""):
    arguments = (editor.get_graph().get_path_name(), variable_name, setter)
    value = (graph._native_read("read_external_member_variable_plan", *arguments, target_class)
             if target_class else graph._native_read("read_member_variable_plan", *arguments))
    if (not value.get("variable_name") or not value.get("property_path") or value.get("setter") is not setter
            or value.get("dirty_before") != value.get("dirty_after")):
        raise OperationError("graph_member_preflight_invalid", "Native member preflight did not preserve the observed read-only state")
    return value


def add_member(editor, variable_name, setter, x, y, target_class=""):
    spawn = editor.add_set_member_variable_node if setter else editor.add_get_member_variable_node
    node = spawn(unreal.Name(variable_name), target_class)
    if node is None:
        raise OperationError("graph_member_spawn_failed", "Official BlueprintGraphEditor returned no member node; inspect the graph before issuing another operation")
    node.set_node_pos(unreal.IntPoint(x, y))
    return node


def member_output_type(node, variable_name):
    """Read native FEdGraphPinType text; display/schema strings lose class identity."""
    return graph.find_pin(node, variable_name, "output").get_pin_type().export_text()


def member_pin_type(node, variable_name, direction):
    return graph.find_pin(node, variable_name, direction).get_pin_type().export_text()


def external_source_state(class_path):
    # Same exact Game generated-path/file/loaded-state sampler as the parent
    # guard; this helper does not load or infer that a class is an Actor parent.
    from .blueprint import parent_reference_state
    return parent_reference_state(class_path)


def load_external_class(class_path):
    value = unreal.load_class(None, class_path)
    if value is None or value.get_path_name() != class_path:
        raise OperationError("graph_external_class_not_found", "External member class did not resolve to its exact requested generated class")
    return value
