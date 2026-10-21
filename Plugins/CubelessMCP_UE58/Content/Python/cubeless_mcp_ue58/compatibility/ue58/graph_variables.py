"""UE58 own-member node spawners and the public C++ preflight/readback bridge."""

import unreal

from . import graph
from cubeless_mcp_ue58.operations.state import OperationError


NATIVE_PAWN = "/Script/Engine.Pawn"
NATIVE_EYE_HEIGHT = "BaseEyeHeight"
NATIVE_EYE_HEIGHT_FLAGS = "6755469234274821"
NATIVE_CHILD_ACTOR_COMPONENT = "/Script/Engine.ChildActorComponent"
NATIVE_CHILD_ACTOR = "ChildActor"
NATIVE_CHILD_ACTOR_FLAGS = "93520134028657204"


NATIVE_TEXTURE_MEMBERS = {
    ("/Script/Engine.TextureRenderTarget2D", "AddressX"): "/Script/Engine.TextureAddress",
    ("/Script/Engine.TextureRenderTarget2D", "AddressY"): "/Script/Engine.TextureAddress",
    ("/Script/Engine.Texture", "Filter"): "/Script/Engine.TextureFilter",
    ("/Script/Engine.Texture", "LODGroup"): "/Script/Engine.TextureGroup",
}


def native_texture_setter_requested(class_path, variable_name, setter):
    if class_path not in ("/Script/Engine.Texture", "/Script/Engine.TextureRenderTarget2D"):
        return False
    if setter is not True or (class_path, variable_name) not in NATIVE_TEXTURE_MEMBERS:
        raise OperationError("graph_native_member_selector_unsupported", "Only exact declaring-owner RT2D.AddressX/AddressY and Texture.Filter/LODGroup setters are supported; no getter, other field or inherited selector alias")
    return True


def _native_texture_plan(value):
    pair = (value.get("target_class_path"), value.get("variable_name"))
    enum = NATIVE_TEXTURE_MEMBERS.get(pair)
    before, after = value.get("native_source_before"), value.get("native_source_after")
    expected = {"member_scope": "external_native_texture_enum_setter", "setter": True,
        "variable_guid": "0" * 32, "declaring_class_path": pair[0], "declaring_blueprint_path": "",
        "property_path": str(pair[0]) + ":" + str(pair[1]), "compiled_property_path": str(pair[0]) + ":" + str(pair[1]),
        "enum_path": enum, "pin_category": "byte", "pin_subcategory": "None",
        "native_source_observed_unchanged": True, "native_property_identity_verified": True,
        "native_texture_runtime_value_written": False, "native_cdo_value_sampled": False}
    if (enum is None or any(value.get(k) != v or type(value.get(k)) is not type(v) for k, v in expected.items())
            or type(before) is not dict or before != after or before.get("observations_complete") is not True
            or before.get("native_identity_verified") is not True or before.get("class_path") != pair[0]
            or before.get("property_path") != expected["property_path"] or before.get("property_class") != "ByteProperty"
            or before.get("property_size") != 1 or before.get("array_dim") != 1
            or type(before.get("property_offset")) is not int or before["property_offset"] < 0
            or type(before.get("class_size")) is not int or before["property_offset"] + 1 > before["class_size"]
            or before.get("package_path") != "/Script/Engine" or before.get("package_dirty") is not False
            or before.get("enum_path") != enum or before.get("enum_flags") is not False
            or before.get("enum_newer_version") is not False or before.get("enum_cpp_type") != enum.rsplit(".", 1)[-1]
            or before.get("cdo_creation_requested") is not False or before.get("cdo_values_sampled") is not False
            or type(before.get("existing_cdo_available")) is not bool
            or not value.get("expected_pin_type_text") or not value.get("expected_self_pin_type_text")):
        raise OperationError("graph_native_texture_preflight_invalid", "Exact current public native TEnumAsByte property, source/enum/CDO metadata and declaring-owner input types are required")
    rows = value.get("native_texture_member_nodes")
    if type(rows) is not list or len(rows) > 2048:
        raise OperationError("graph_native_texture_preflight_invalid", "Native current setter inventory is missing or unbounded")
    for row in rows:
        if (type(row) is not dict or row.get("node_class_path") != "/Script/BlueprintGraph.K2Node_VariableSet"
                or row.get("member_name") != pair[1] or row.get("member_guid") != "0" * 32
                or row.get("member_parent_class") != pair[0] or row.get("member_self_context") is not False
                or row.get("property_path") != expected["property_path"] or row.get("whole_pins_current_verified") is not True
                or row.get("graph_path") != value.get("graph_path") or row.get("outer_path") != value.get("graph_path")
                or type(row.get("pins")) is not list or not row["pins"]):
            raise OperationError("graph_native_texture_node_invalid", "Current returned setter/member/pin roles are incomplete")
        value_pins = [p for p in row["pins"] if p.get("name") == pair[1] and p.get("direction") == "input"]
        self_pins = [p for p in row["pins"] if p.get("name") == "self" and p.get("direction") == "input"]
        if (len(value_pins) != 1 or len(self_pins) != 1
                or value_pins[0].get("type_text") != value["expected_pin_type_text"]
                or self_pins[0].get("type_text") != value["expected_self_pin_type_text"]
                or any(type(p.get("flags")) is not int or p["flags"] & (1 | 2 | 32 | 256) for p in value_pins + self_pins)):
            raise OperationError("graph_native_texture_node_invalid", "Native setter needs exact whole visible current enum-value and explicit texture self inputs")


def native_pawn_getter_requested(class_path, variable_name, setter):
    """Reject every other native selector before loading or spawning anything."""
    if not class_path.startswith("/Script/"):
        return False
    if class_path != NATIVE_PAWN or variable_name != NATIVE_EYE_HEIGHT or setter:
        raise OperationError("graph_native_member_selector_unsupported", "Only the exact loaded Pawn.BaseEyeHeight getter is supported; other native members/classes and all native setters remain unsupported")
    return True



def native_member_getter_requested(class_path, variable_name, setter):
    if native_texture_setter_requested(class_path, variable_name, setter):
        return True
    if class_path == NATIVE_CHILD_ACTOR_COMPONENT:
        if variable_name != NATIVE_CHILD_ACTOR or setter:
            raise OperationError("graph_native_member_selector_unsupported", "Only exact native CAC.ChildActor readonly getter is admitted; setters/other private fields remain unsupported")
        return True
    return native_pawn_getter_requested(class_path, variable_name, setter)


def _native_child_actor_plan(value):
    expected = {"member_scope": "external_native_child_actor_readonly", "variable_name": NATIVE_CHILD_ACTOR,
        "setter": False, "variable_guid": "0" * 32, "target_class_path": NATIVE_CHILD_ACTOR_COMPONENT,
        "declaring_class_path": NATIVE_CHILD_ACTOR_COMPONENT, "declaring_blueprint_path": "",
        "property_path": NATIVE_CHILD_ACTOR_COMPONENT + ":" + NATIVE_CHILD_ACTOR,
        "compiled_property_path": NATIVE_CHILD_ACTOR_COMPONENT + ":" + NATIVE_CHILD_ACTOR,
        "pin_category": "object", "pin_subcategory": "None",
        "native_property_expected_flags_string": NATIVE_CHILD_ACTOR_FLAGS,
        "native_property_flags_string": NATIVE_CHILD_ACTOR_FLAGS,
        "native_property_identity_verified": True, "native_source_observed_unchanged": True,
        "native_child_actor_private_readonly_verified": True,
        "native_child_actor_runtime_evaluation_verified": False, "native_cdo_value_sampled": False}
    before, after = value.get("native_source_before"), value.get("native_source_after")
    if (any(value.get(key) != item or type(value.get(key)) is not type(item) for key, item in expected.items())
            or type(before) is not dict or before != after
            or before.get("class_path") != NATIVE_CHILD_ACTOR_COMPONENT
            or before.get("class_flags_string") != "820510884" or before.get("package_name") != "/Script/Engine"
            or before.get("package_dirty") is not False or before.get("property_class") != "ObjectProperty"
            or before.get("property_path") != expected["property_path"]
            or before.get("property_flags_string") != NATIVE_CHILD_ACTOR_FLAGS
            or before.get("cpp_type") != "TObjectPtr<AActor>" or before.get("object_class_path") != "/Script/Engine.Actor"
            or before.get("allow_private_access") != "true" or before.get("blueprint_read_only") is not True
            or before.get("native_private") is not True or before.get("pin_is_uobject_wrapper") is not True
            or type(before.get("array_dim")) is not int or before["array_dim"] != 1
            or not value.get("expected_pin_type_text") or not value.get("expected_self_pin_type_text")
            or before.get("pin_type_text") != value["expected_pin_type_text"]):
        raise OperationError("graph_native_member_preflight_invalid", "Native CAC getter observations do not prove the exact readonly private/AllowPrivateAccess, zero-GUID and wrapper-aware Actor/CAC pin contract")


def _native_pawn_plan(value):
    expected = {"member_scope": "external_native_pawn_eye_height", "variable_name": NATIVE_EYE_HEIGHT,
        "setter": False, "variable_guid": "0" * 32, "target_class_path": NATIVE_PAWN,
        "declaring_class_path": NATIVE_PAWN, "declaring_blueprint_path": "",
        "property_path": NATIVE_PAWN + ":" + NATIVE_EYE_HEIGHT,
        "compiled_property_path": NATIVE_PAWN + ":" + NATIVE_EYE_HEIGHT,
        "pin_category": "real", "pin_subcategory": "float",
        "native_property_expected_flags_string": NATIVE_EYE_HEIGHT_FLAGS,
        "native_property_flags_string": NATIVE_EYE_HEIGHT_FLAGS,
        "native_property_identity_verified": True, "native_source_observed_unchanged": True}
    before, after = value.get("native_source_before"), value.get("native_source_after")
    if (any(value.get(key) != item or type(value.get(key)) is not type(item) for key, item in expected.items())
            or type(before) is not dict or before != after
            or before.get("class_path") != NATIVE_PAWN or before.get("package_name") != "/Script/Engine"
            or before.get("package_dirty") is not False or before.get("property_class") != "FloatProperty"
            or before.get("property_path") != expected["property_path"]
            or before.get("property_flags_string") != NATIVE_EYE_HEIGHT_FLAGS
            or type(before.get("array_dim")) is not int or before["array_dim"] != 1
            or not value.get("expected_pin_type_text") or not value.get("expected_self_pin_type_text")
            or before.get("pin_type_text") != value["expected_pin_type_text"]):
        raise OperationError("graph_native_member_preflight_invalid", "Native Pawn getter observations differ from the exact public scalar float/flags/owner/current loaded-state contract")


def member_plan(editor, variable_name, setter, target_class=""):
    native = native_member_getter_requested(target_class, variable_name, setter)
    arguments = (editor.get_graph().get_path_name(), variable_name, setter)
    value = (graph._native_read("read_external_member_variable_plan", *arguments, target_class)
             if target_class else graph._native_read("read_member_variable_plan", *arguments))
    if (not value.get("variable_name") or not value.get("property_path") or value.get("setter") is not setter
            or value.get("dirty_before") != value.get("dirty_after")):
        raise OperationError("graph_member_preflight_invalid", "Native member preflight did not preserve the observed read-only state")
    if native:
        (_native_texture_plan if native_texture_setter_requested(target_class, variable_name, setter) else _native_child_actor_plan if target_class == NATIVE_CHILD_ACTOR_COMPONENT else _native_pawn_plan)(value)
    return value


def add_member(editor, variable_name, setter, x, y, target_class="", *, receipt=None):
    spawn = editor.add_set_member_variable_node if setter else editor.add_get_member_variable_node
    if receipt is not None:
        receipt.update(spawn_attempted=True, position_attempted=False)
    node = spawn(unreal.Name(variable_name), target_class)
    if receipt is not None:
        receipt["spawn_returned"] = node is not None
    if node is None:
        raise OperationError("graph_member_spawn_failed", "Official BlueprintGraphEditor returned no member node; inspect the graph before issuing another operation")
    if receipt is not None:
        receipt.update(created_node_path=node.get_path_name(), position_attempted=True)
    node.set_node_pos(unreal.IntPoint(x, y))
    if receipt is not None:
        receipt["position_returned"] = True
    return node


def add_external_actor_member(editor, variable_name, setter, x, y, target_class, *, receipt, current):
    """Existing official member writer with the Actor source's current closure."""
    receipt.update(spawn_attempted=False, position_attempted=False)
    current("before_spawn")
    spawn = editor.add_set_member_variable_node if setter else editor.add_get_member_variable_node
    receipt["spawn_attempted"] = True
    try:
        node = spawn(unreal.Name(variable_name), target_class)
    finally:
        spawn = None
    receipt["spawn_returned"] = node is not None
    if node is None:
        raise OperationError("graph_external_actor_spawn_failed", "Member spawn returned no node; no later observation or retry")
    # current retains this actual wrapper before any identity getter or position call.
    current("after_spawn_return", node)
    receipt["position_attempted"] = True
    position = node.set_node_pos
    try:
        position(unreal.IntPoint(x, y))
    finally:
        position = None
    receipt["position_returned"] = True
    current("after_position", node)
    return node


def add_material_value_member(editor, variable_name, setter, x, y, *, receipt, current):
    """Spawn the finite own value only while the freshly observed role remains current."""
    receipt.update(spawn_attempted=False, position_attempted=False)
    current("before_spawn")
    spawn = editor.add_set_member_variable_node if setter else editor.add_get_member_variable_node
    receipt["spawn_attempted"] = True
    try:
        node = spawn(unreal.Name(variable_name), "")
    finally:
        # A diagnostic may retain this frame. Drop our bound Unreal callable by RC first.
        spawn = None
    receipt["spawn_returned"] = node is not None
    if node is None:
        raise OperationError("graph_member_spawn_failed", "Official member spawn returned no node; retain the attempt without retry or another SDK read")
    # Preserve the actual returned identity before a broader source-role check can fail.
    receipt["created_node_path"] = node.get_path_name()
    current("after_return_identity", node)
    receipt["position_attempted"] = True
    node.set_node_pos(unreal.IntPoint(x, y))
    receipt["position_returned"] = True
    current("after_position", node)
    return node


def add_vector_array_member(editor, variable_name, setter, x, y, target_class="", *, receipt, current):
    """Spawn only the finite current own Map or own/external Vector array role, retaining the returned prefix."""
    receipt.update(spawn_attempted=False, position_attempted=False)
    current("before_spawn")
    spawn = editor.add_set_member_variable_node if setter else editor.add_get_member_variable_node
    receipt["spawn_attempted"] = True
    try:
        node = spawn(unreal.Name(variable_name), target_class)
    finally:
        # A diagnostic may retain this frame. Drop our bound Unreal callable by RC first.
        spawn = None
    receipt["spawn_returned"] = node is not None
    if node is None:
        raise OperationError("graph_member_spawn_failed", "Official member spawn returned no node; retain the attempt without retry or another SDK read")
    # Preserve the actual returned identity before a broader source-role check can fail.
    receipt["created_node_path"] = node.get_path_name()
    current("after_return_identity", node)
    receipt["position_attempted"] = True
    node.set_node_pos(unreal.IntPoint(x, y))
    receipt["position_returned"] = True
    current("after_position", node)
    return node


def add_native_texture_member(editor, variable_name, x, y, target_class, *, receipt, current):
    # Reuse the existing held-return/current/finally-RC spawner, not a runtime setter.
    return add_vector_array_member(editor, variable_name, True, x, y, target_class, receipt=receipt, current=current)


def member_output_type(node, variable_name):
    """Read native FEdGraphPinType text; display/schema strings lose class identity."""
    return graph.find_pin(node, variable_name, "output").get_pin_type().export_text()


def member_pin_type(node, variable_name, direction):
    return graph.find_pin(node, variable_name, direction).get_pin_type().export_text()


def external_source_state(class_path):
    if class_path.startswith("/Script/"):
        if class_path not in (NATIVE_PAWN, NATIVE_CHILD_ACTOR_COMPONENT):
            raise OperationError("graph_native_member_selector_unsupported", "Native observation supports only exact already-loaded Pawn/CAC classes")
        # CAC reads the fixed Python type metadata; do not introduce a new string lookup before native Context validation.
        value = (unreal.ChildActorComponent.static_class() if class_path == NATIVE_CHILD_ACTOR_COMPONENT
                 else unreal.find_object(None, class_path))
        if (value is None or value.get_path_name() != class_path
                or value.get_class().get_path_name() != "/Script/CoreUObject.Class"
                or value.get_outer().get_path_name() != "/Script/Engine"):
            raise OperationError("graph_native_member_class_not_loaded", "The exact native selected UClass must already be loaded; no class load or fallback was dispatched")
        return {"scope": "native_loaded_class", "class_path": value.get_path_name(), "loaded": True,
                "class_class_path": value.get_class().get_path_name(), "package_name": value.get_outer().get_path_name()}
    # Same exact Game generated-path/file/loaded-state sampler as the parent
    # guard; this helper does not load or infer that a class is an Actor parent.
    from .blueprint import parent_reference_state
    return parent_reference_state(class_path)


def load_external_class(class_path):
    value = unreal.load_class(None, class_path)
    if value is None or value.get_path_name() != class_path:
        raise OperationError("graph_external_class_not_found", "External member class did not resolve to its exact requested generated class")
    return value
