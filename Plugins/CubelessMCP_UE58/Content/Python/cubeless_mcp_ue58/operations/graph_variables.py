"""Named own/inherited/component member nodes sharing one observed spawn core."""

import copy
import json
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import graph as graph_api
from cubeless_mcp_ue58.compatibility.ue58 import graph_variables as api
from cubeless_mcp_ue58.compatibility.ue58.blueprint import ensure_editor_idle
from . import state
from .blueprint import parent_comparison_state
from .graph import _contract
from .state import OperationError, mutate


_NATIVE_SCS_IDENTITY_FIELDS = (
    "member_scope", "setter", "variable_name", "property_path", "expected_pin_type_text",
    "scs_node_path", "scs_variable_guid", "scs_outer_path", "scs_guid_equals_member_reference_guid_verified", "component_class_path",
    "component_template_path", "component_template_class_path", "component_template_outer_path",
    "skeleton_class_path", "property_owner_class_path", "property_flags_string", "property_array_dim", "pin_type",
    "component_class_validation",
)

_NATIVE_LOCAL_SCS_SIGNATURES = {
    "local_scs_native_spline_component": ("spline", "SplineComponent", "PrimitiveComponent", "runtime_evaluation_verified"),
    "local_scs_native_ism_component": ("ism", "InstancedStaticMeshComponent", "StaticMeshComponent", "runtime_instance_operations_verified"),
}


def _native_scs_fields(family, runtime_field):
    return _NATIVE_SCS_IDENTITY_FIELDS + tuple("native_" + family + "_" + name for name in (
        "local_template_identity_verified", "skeleton_property_verified", "source_before", "source_after",
        "source_observed_unchanged", runtime_field))


def _native_local_scs_plan(plan, variable_name, setter, target_class):
    """Share the exact Spline/ISM local-SCS checks; generated class guards stay separate."""
    scope = plan.get("member_scope")
    family, class_name, super_name, runtime_field = _NATIVE_LOCAL_SCS_SIGNATURES[scope]
    native_class = "/Script/Engine." + class_name
    prefix = "native_" + family + "_"
    source = plan.get(prefix + "source_before")
    pin = plan.get("pin_type")
    expected_pin = {"category": "object", "subcategory": "None", "type_object": native_class,
        "container_type": 0, "is_reference": False, "is_const": False}
    try:
        guid_valid = bool(uuid.UUID(plan.get("scs_variable_guid", "")).int)
    except (TypeError, ValueError, AttributeError):
        guid_valid = False
    required_paths = ("scs_node_path", "scs_outer_path", "component_template_path",
        "component_template_outer_path", "skeleton_class_path", "property_owner_class_path", "property_path")
    passed = (setter is False and not target_class
        and plan.get("member_scope") == scope
        and plan.get("setter") is False and plan.get("variable_name") == variable_name
        and plan.get("component_class_path") == native_class and plan.get("component_template_class_path") == native_class
        and plan.get(prefix + "local_template_identity_verified") is True
        and plan.get(prefix + "skeleton_property_verified") is True
        and plan.get("scs_guid_equals_member_reference_guid_verified") is False
        and plan.get("pin_category") == "object" and plan.get("pin_subcategory") == "None"
        and type(plan.get("property_array_dim")) is int and plan["property_array_dim"] == 1
        and plan.get("property_owner_class_path") == plan.get("skeleton_class_path")
        and type(plan.get("property_flags_string")) is str and plan["property_flags_string"].isdecimal()
        and guid_valid and all(type(plan.get(key)) is str and plan[key].startswith("/Game/") for key in required_paths)
        and type(plan.get("expected_pin_type_text")) is str and bool(plan["expected_pin_type_text"])
        and type(pin) is dict and all(pin.get(key) == value and type(pin.get(key)) is type(value) for key, value in expected_pin.items())
        and type(source) is dict and source == plan.get(prefix + "source_after")
        and source == plan.get("component_class_validation")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("success") is True and source.get("class_path") == native_class
        and source.get("super_class_path") == "/Script/Engine." + super_name
        and source.get("class_generated_by_path") == "" and source.get("package_path") == "/Script/Engine"
        and source.get("package_dirty") is False and source.get("existing_cdo_available") is True
        and source.get("existing_cdo_path") == "/Script/Engine.Default__" + class_name
        and source.get("cdo_creation_requested") is False
        and all(type(source.get(key)) is str and source[key].isdecimal()
            for key in ("class_flags_string", "package_flags_string", "existing_cdo_flags_string"))
        and plan.get(prefix + "source_observed_unchanged") is True
        and plan.get(prefix + runtime_field) is False)
    if not passed:
        label = "Spline" if family == "spline" else "ISM"
        raise OperationError("graph_native_" + family + "_preflight_invalid", "Native local " + label + " observations do not prove the exact clean Engine class, existing CDO, reachable own SCS/template/outer/GUID and current scalar object property/type")



_NATIVE_CLASS_IDENTITY_FIELDS = (
    "member_scope", "setter", "variable_name", "variable_guid", "property_path", "expected_pin_type_text",
    "pin_category", "pin_subcategory", "class_constraint_path", "skeleton_class_path", "property_owner_class_path",
    "property_flags_string", "property_array_dim", "pin_type", "class_property_uobject_wrapper_matches",
    "native_class_constraint_before", "native_class_constraint_after", "native_class_constraint_observed_unchanged",
    "native_class_constraint_identity_verified", "stored_default_before", "stored_default_after",
    "stored_default_observed_unchanged", "class_default_write_attempted", "native_class_runtime_evaluation_verified",
)


def _native_class_plan(plan, variable_name, setter, target_class):
    """Validate the separate getter-only Class plan without treating it as an Object pin."""
    constraint = plan.get("class_constraint_path")
    source = plan.get("native_class_constraint_before")
    pin = plan.get("pin_type")
    try:
        guid_valid = bool(uuid.UUID(plan.get("variable_guid", "")).int)
    except (TypeError, ValueError, AttributeError):
        guid_valid = False
    passed = (setter is False and not target_class and plan.get("setter") is False
        and plan.get("member_scope") == "own_native_class" and plan.get("variable_name") == variable_name
        and constraint in ("/Script/Engine.Actor", "/Script/Engine.ActorComponent")
        and plan.get("pin_category") == "class" and plan.get("pin_subcategory") == "None"
        and type(plan.get("property_array_dim")) is int and plan["property_array_dim"] == 1
        and plan.get("property_owner_class_path") == plan.get("skeleton_class_path")
        and all(type(plan.get(key)) is str and plan[key].startswith("/Game/")
            for key in ("property_path", "skeleton_class_path", "property_owner_class_path"))
        and type(plan.get("property_flags_string")) is str and plan["property_flags_string"].isdecimal()
        and guid_valid and type(plan.get("expected_pin_type_text")) is str and bool(plan["expected_pin_type_text"])
        and type(pin) is dict and pin.get("category") == "class" and pin.get("subcategory") == "None"
        and pin.get("type_object") == constraint and type(pin.get("container_type")) is int and pin["container_type"] == 0
        and pin.get("is_reference") is False and pin.get("is_const") is False
        and plan.get("class_property_uobject_wrapper_matches") is True
        and plan.get("native_class_constraint_identity_verified") is True
        and plan.get("native_class_constraint_observed_unchanged") is True
        and type(source) is dict and source == plan.get("native_class_constraint_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("class_path") == constraint and source.get("class_generated_by_path") == ""
        and source.get("package_path") == "/Script/Engine" and source.get("package_dirty") is False
        and source.get("existing_cdo_available") is True and source.get("cdo_creation_requested") is False
        and plan.get("stored_default_before") in ("", "None")
        and plan.get("stored_default_before") == plan.get("stored_default_after")
        and plan.get("stored_default_observed_unchanged") is True
        and plan.get("class_default_write_attempted") is False
        and plan.get("native_class_runtime_evaluation_verified") is False)
    if not passed:
        raise OperationError("graph_native_class_preflight_invalid", "Class getter observations do not prove an own current scalar Class property with an exact loaded native Actor/ActorComponent constraint and preserved null stored default")


_MATERIAL_VALUE_IDENTITY_FIELDS = (
    "graph_path", "blueprint_path", "member_scope", "setter", "variable_name", "variable_guid", "property_path", "property_owner_class_path",
    "skeleton_class_path", "property_class", "property_array_dim", "property_flags_string", "native_type_path",
    "value_kind", "pin_type", "expected_pin_type_text", "pin_category", "pin_subcategory", "pin_is_uobject_wrapper",
    "pin_serialize_single_precision", "pin_terminal_is_uobject_wrapper", "stored_default_before", "stored_default_after",
    "stored_default_observed_unchanged", "native_value_source_before", "native_value_source_after",
    "native_value_source_observed_unchanged", "native_value_type_identity_verified", "native_value_property_identity_verified",
    "runtime_value_read_attempted", "default_write_attempted", "runtime_evaluation_verified", "native_resource_is_niagara_system", "native_resource_is_texture_render_target_2d",
)

_MATERIAL_VALUE_NODE_IDENTITY_FIELDS = (
    "object_path", "node_guid", "node_class_path", "graph_path", "outer_path", "member_name", "member_guid",
    "graph_member", "member_self_context", "member_local_scope", "member_deprecated", "property_path",
    "value_pin_name", "value_pin_direction", "value_pin_type_text", "value_pin_id", "value_pin_flags",
    "value_pin_whole_current_verified", "value_pin_trashed", "value_pin_hidden", "value_pin_orphaned",
    "value_pin_split", "value_pin_owner_path",
)

_MATERIAL_VALUE_TYPES = {
    "/Script/Engine.MaterialInterface": ("object", "object", "ObjectProperty"),
    "/Script/Engine.MaterialInstanceDynamic": ("object", "object", "ObjectProperty"),
    "/Script/Engine.Texture": ("object", "object", "ObjectProperty"),
    "/Script/Engine.TextureRenderTarget2D": ("object", "object", "ObjectProperty"),
    "/Script/Niagara.NiagaraSystem": ("object", "object", "ObjectProperty"),
    "/Script/CoreUObject.LinearColor": ("linear_color", "struct", "StructProperty"),
}


def _own_material_value_plan(plan, variable_name, setter, target_class):
    """Validate the current finite own-value plan without reading a runtime value."""
    native_type = plan.get("native_type_path")
    signature = _MATERIAL_VALUE_TYPES.get(native_type)
    source, pin = plan.get("native_value_source_before"), plan.get("pin_type")
    try:
        guid_valid = bool(uuid.UUID(plan.get("variable_guid", "")).int)
    except (TypeError, ValueError, AttributeError):
        guid_valid = False
    passed = (not target_class and type(setter) is bool and plan.get("setter") is setter
        and plan.get("member_scope") == "own_native_material_value" and plan.get("variable_name") == variable_name
        and signature is not None and guid_valid
        and (native_type != "/Script/Niagara.NiagaraSystem" or plan.get("native_resource_is_niagara_system") is True)
        and (native_type != "/Script/Engine.TextureRenderTarget2D" or plan.get("native_resource_is_texture_render_target_2d") is True)
        and type(plan.get("property_array_dim")) is int and plan["property_array_dim"] == 1
        and plan.get("property_owner_class_path") == plan.get("skeleton_class_path")
        and all(type(plan.get(key)) is str and plan[key].startswith("/Game/")
                for key in ("graph_path", "blueprint_path", "property_path", "property_owner_class_path", "skeleton_class_path"))
        and type(plan.get("property_flags_string")) is str and plan["property_flags_string"].isdecimal()
        and type(plan.get("expected_pin_type_text")) is str and bool(plan["expected_pin_type_text"])
        and all(type(plan.get(key)) is bool for key in ("pin_is_uobject_wrapper", "pin_serialize_single_precision", "pin_terminal_is_uobject_wrapper"))
        and plan.get("native_value_type_identity_verified") is True
        and plan.get("native_value_property_identity_verified") is True
        and plan.get("native_value_source_observed_unchanged") is True
        and type(source) is dict and source == plan.get("native_value_source_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("type_path") == native_type and source.get("package_dirty") is False
        and all(type(source.get(key)) is str and source[key].isdecimal()
                for key in ("package_flags_string", "type_flags_string"))
        and type(source.get("type_structure_size")) is int and source["type_structure_size"] > 0
        and type(plan.get("stored_default_before")) is str
        and plan["stored_default_before"] == plan.get("stored_default_after")
        and plan.get("stored_default_observed_unchanged") is True
        and all(plan.get(key) is False for key in ("runtime_value_read_attempted", "default_write_attempted", "runtime_evaluation_verified")))
    if passed:
        kind, category, property_class = signature
        expected_pin = {"category": category, "subcategory": "None", "type_object": native_type,
            "container_type": 0, "is_reference": False, "is_const": False}
        source_matches = (source.get("type_kind") == kind and source.get("package_path") == native_type.split(".", 1)[0])
        if kind == "object":
            available = source.get("existing_cdo_available")
            cdo_path = native_type.split(".", 1)[0] + ".Default__" + native_type.rsplit(".", 1)[-1] if available else ""
            source_matches = (source_matches and source.get("class_path") == native_type
                and type(available) is bool and source.get("existing_cdo_path") == cdo_path
                and type(source.get("existing_cdo_flags_string")) is str and source["existing_cdo_flags_string"].isdecimal()
                and (available or source["existing_cdo_flags_string"] == "0")
                and source.get("cdo_creation_requested") is False and source.get("cdo_values_sampled") is False)
        else:
            fields = source.get("native_fields")
            source_matches = (source_matches and source.get("struct_path") == native_type
                and type(fields) is list and len(fields) == 4 and all(type(field) is dict and bool(field) for field in fields))
        passed = (source_matches and plan.get("value_kind") == kind and plan.get("property_class") == property_class
            and plan.get("pin_category") == category and plan.get("pin_subcategory") == "None"
            and type(pin) is dict and all(pin.get(key) == value and type(pin.get(key)) is type(value)
                for key, value in expected_pin.items()))
    rows = plan.get("native_value_member_nodes")
    if passed:
        passed = type(rows) is list and len(rows) <= 2048
    paths, guids = set(), set()
    if passed:
        for row in rows:
            if type(row) is not dict:
                passed = False
                break
            path, node_guid = row.get("object_path"), row.get("node_guid")
            cls = row.get("node_class_path")
            node_setter = cls == "/Script/BlueprintGraph.K2Node_VariableSet"
            try:
                guid = uuid.UUID(node_guid)
                member_guid = uuid.UUID(row.get("member_guid", ""))
                pin_guid = uuid.UUID(row.get("value_pin_id", ""))
                same_guid = bool(pin_guid.int) and member_guid == uuid.UUID(plan["variable_guid"])
            except (TypeError, ValueError, AttributeError):
                guid, same_guid = None, False
            if (type(path) is not str or not path.startswith(plan["graph_path"] + ".") or path in paths
                    or guid is None or not guid.int or guid.hex in guids or not same_guid
                    or cls not in ("/Script/BlueprintGraph.K2Node_VariableGet", "/Script/BlueprintGraph.K2Node_VariableSet")
                    or row.get("graph_path") != plan["graph_path"] or row.get("outer_path") != plan["graph_path"]
                    or row.get("graph_member") is not True or row.get("member_name") != variable_name
                    or row.get("member_self_context") is not True or row.get("member_local_scope") is not False
                    or row.get("member_deprecated") is not False or row.get("property_path") != plan["property_path"]
                    or row.get("value_pin_name") != variable_name
                    or row.get("value_pin_direction") != ("input" if node_setter else "output")
                    or row.get("value_pin_type_text") != plan["expected_pin_type_text"]
                    or row.get("value_pin_whole_current_verified") is not True
                    or any(row.get(key) is not False for key in ("value_pin_trashed", "value_pin_hidden", "value_pin_orphaned", "value_pin_split"))
                    or row.get("value_pin_owner_path") != path
                    or type(row.get("value_pin_flags")) is not int or not 0 <= row["value_pin_flags"] <= 0xFFFFFFFF):
                passed = False
                break
            paths.add(path)
            guids.add(guid.hex)
    if not passed:
        raise OperationError("graph_native_material_value_preflight_invalid", "Own MaterialInterface/MID/Texture/TextureRenderTarget2D/NiagaraSystem/LinearColor observations do not prove the exact scalar source, current property/GUID/default and physical pin contract")


_VECTOR_ARRAY_IDENTITY_FIELDS = _MATERIAL_VALUE_IDENTITY_FIELDS + (
    "inner_property_path", "inner_property_flags_string", "target_class_path", "declaring_class_path",
    "declaring_blueprint_path", "compiled_property_path", "expected_self_pin_type_text",
    "external_source_before", "external_source_after", "external_source_observed_unchanged",
)
_VECTOR_ARRAY_NODE_IDENTITY_FIELDS = _MATERIAL_VALUE_NODE_IDENTITY_FIELDS + (
    "property_declaring_role", "self_pin_name", "self_pin_direction", "self_pin_id", "self_pin_type_text",
    "self_pin_owner_path", "self_pin_whole_current_verified", "self_pin_flags",
)


def _native_vector_array_plan(plan, variable_name, setter, target_class):
    """Validate exact own Vector4[] or saved generated external Vector[]/Vector4[] observations."""
    native_type = plan.get("native_type_path")
    double_map = plan.get("member_scope") == "own_native_string_double_map"
    mapping = double_map or plan.get("member_scope") == "own_native_string_vector_map"
    external = plan.get("member_scope") == "external_native_vector_array"
    kind = "string_double_map" if double_map else "string_vector_map" if mapping else "vector4_array" if native_type == "/Script/CoreUObject.Vector4" else "vector_array"
    signature = ((kind, "string", "MapProperty") if (native_type == "" if double_map else native_type == "/Script/CoreUObject.Vector") else None) if mapping else (kind, "struct", "ArrayProperty") if (native_type == "/Script/CoreUObject.Vector4" or (external and native_type == "/Script/CoreUObject.Vector")) else None
    source, pin = plan.get("native_value_source_before"), plan.get("pin_type")
    try:
        guid_valid = bool(uuid.UUID(plan.get("variable_guid", "")).int)
    except (TypeError, ValueError, AttributeError):
        guid_valid = False
    passed = (bool(target_class) is external and (not external or setter is False) and type(setter) is bool and plan.get("setter") is setter
        and plan.get("member_scope") == ("own_native_string_double_map" if double_map else "own_native_string_vector_map" if mapping else "external_native_vector_array" if external else "own_native_vector4_array") and plan.get("variable_name") == variable_name
        and signature is not None and guid_valid
        and type(plan.get("property_array_dim")) is int and plan["property_array_dim"] == 1
        and plan.get("property_owner_class_path") == plan.get("skeleton_class_path")
        and all(type(plan.get(key)) is str and plan[key].startswith("/Game/")
                for key in ("graph_path", "blueprint_path", "property_path", "property_owner_class_path", "skeleton_class_path"))
        and type(plan.get("property_flags_string")) is str and plan["property_flags_string"].isdecimal()
        and type(plan.get("expected_pin_type_text")) is str and bool(plan["expected_pin_type_text"])
        and all(type(plan.get(key)) is bool for key in ("pin_is_uobject_wrapper", "pin_serialize_single_precision", "pin_terminal_is_uobject_wrapper"))
        and plan.get("native_value_type_identity_verified") is True
        and plan.get("native_value_property_identity_verified") is True
        and plan.get("native_value_source_observed_unchanged") is True
        and type(source) is dict and source == plan.get("native_value_source_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("type_path") == native_type and source.get("package_dirty") is False
        and all(type(source.get(key)) is str and source[key].isdecimal()
                for key in ("package_flags_string", "type_flags_string"))
        and type(source.get("type_structure_size")) is int and source["type_structure_size"] > 0
        and type(plan.get("stored_default_before")) is str
        and plan["stored_default_before"] == plan.get("stored_default_after")
        and plan.get("stored_default_observed_unchanged") is True
        and all(plan.get(key) is False for key in ("runtime_value_read_attempted", "default_write_attempted", "runtime_evaluation_verified")))
    if passed:
        kind, category, property_class = signature
        expected_pin = {"category": category, "subcategory": "None", "type_object": "" if mapping else native_type,
            "container_type": 3 if mapping else 1, "is_reference": False, "is_const": False}
        fields = source.get("native_fields")
        source_matches = ((source.get("type_kind") == kind and source.get("package_path") == "/Script/CoreUObject"
            and source.get("struct_path") == "" and source.get("scalar_property_class") == "DoubleProperty"
            and source.get("scalar_category") == "real" and source.get("scalar_subcategory") == "double"
            and source.get("type_structure_size") == 8 and source.get("type_alignment") == 8
            and fields == [] and plan.get("inner_property_class") == "DoubleProperty") if double_map else
            (source.get("type_kind") == kind and source.get("package_path") == "/Script/CoreUObject"
            and source.get("struct_path") == native_type and type(fields) is list
            and len(fields) == (4 if native_type.endswith(".Vector4") else 3)
            and all(type(field) is dict and field.get("name") == "XYZW"[index]
                    and field.get("owner") == native_type and field.get("class") == "DoubleProperty"
                    for index, field in enumerate(fields))))
        passed = (source_matches and plan.get("value_kind") == kind and plan.get("property_class") == property_class
            and plan.get("pin_category") == category and plan.get("pin_subcategory") == "None"
            and type(pin) is dict and all(pin.get(key) == value and type(pin.get(key)) is type(value)
                for key, value in expected_pin.items()))
    passed = (passed and type(plan.get("inner_property_path")) is str and plan["inner_property_path"].startswith("/Game/")
        and type(plan.get("inner_property_flags_string")) is str and plan["inner_property_flags_string"].isdecimal()
        and all(plan.get(key) is False for key in ("pin_is_uobject_wrapper", "pin_serialize_single_precision", "pin_terminal_is_uobject_wrapper")))
    if passed and mapping:
        terminal = pin.get("terminal")
        expected_terminal = {"category": "real" if double_map else "struct", "subcategory": "double" if double_map else "None", "type_object": "" if double_map else "/Script/CoreUObject.Vector",
            "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False}
        passed = (plan.get("native_string_double_map_member" if double_map else "native_string_vector_map_member") is True and plan.get("map_property_identity_verified") is True
            and plan.get("map_setter_input_requires_connection") is setter
            and type(terminal) is dict and all(terminal.get(k) == value and type(terminal.get(k)) is type(value) for k, value in expected_terminal.items())
            and all(pin.get(k) is False for k in ("is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and type(plan.get("key_property_path")) is str and plan["key_property_path"].startswith("/Game/")
            and type(plan.get("key_property_flags_string")) is str and plan["key_property_flags_string"].isdecimal()
            and plan["stored_default_before"] == "")
    if passed and external:
        passed = (target_class == plan.get("target_class_path") and target_class.endswith("_C")
            and target_class.startswith("/Game/") and all(type(plan.get(key)) is str and plan[key].startswith("/Game/")
                for key in ("declaring_class_path", "declaring_blueprint_path", "compiled_property_path"))
            and type(plan.get("expected_self_pin_type_text")) is str and bool(plan["expected_self_pin_type_text"])
            and type(plan.get("external_source_before")) is dict
            and plan["external_source_before"] == plan.get("external_source_after")
            and plan.get("external_source_observed_unchanged") is True
            and type(plan.get("external_source_validation")) is dict and bool(plan["external_source_validation"]))
    rows = plan.get("native_value_member_nodes")
    if passed:
        passed = type(rows) is list and len(rows) <= 2048
    paths, guids = set(), set()
    if passed:
        for row in rows:
            if type(row) is not dict:
                passed = False
                break
            path, node_guid = row.get("object_path"), row.get("node_guid")
            cls = row.get("node_class_path")
            node_setter = cls == "/Script/BlueprintGraph.K2Node_VariableSet"
            try:
                guid = uuid.UUID(node_guid)
                member_guid = uuid.UUID(row.get("member_guid", ""))
                pin_guid = uuid.UUID(row.get("value_pin_id", ""))
                same_guid = bool(pin_guid.int) and member_guid == uuid.UUID(plan["variable_guid"])
            except (TypeError, ValueError, AttributeError):
                guid, same_guid = None, False
            if (type(path) is not str or not path.startswith(plan["graph_path"] + ".") or path in paths
                    or guid is None or not guid.int or guid.hex in guids or not same_guid
                    or cls not in (("/Script/BlueprintGraph.K2Node_VariableGet",) if external else ("/Script/BlueprintGraph.K2Node_VariableGet", "/Script/BlueprintGraph.K2Node_VariableSet"))
                    or row.get("graph_path") != plan["graph_path"] or row.get("outer_path") != plan["graph_path"]
                    or row.get("graph_member") is not True or row.get("member_name") != variable_name
                    or row.get("member_self_context") is not (not external) or row.get("member_local_scope") is not False
                    or row.get("member_deprecated") is not False
                    or (row.get("property_path") != (plan.get("compiled_property_path") if row.get("property_declaring_role") == "generated" else plan["property_path"]) if external else row.get("property_path") != plan["property_path"])
                    or (external and row.get("property_declaring_role") not in ("skeleton", "generated"))
                    or row.get("value_pin_name") != variable_name
                    or row.get("value_pin_direction") != ("input" if node_setter else "output")
                    or row.get("value_pin_type_text") != plan["expected_pin_type_text"]
                    or row.get("value_pin_whole_current_verified") is not True
                    or any(row.get(key) is not False for key in ("value_pin_trashed", "value_pin_hidden", "value_pin_orphaned", "value_pin_split"))
                    or row.get("value_pin_owner_path") != path
                    or type(row.get("value_pin_flags")) is not int or not 0 <= row["value_pin_flags"] <= 0xFFFFFFFF):
                passed = False
                break
            if external:
                try:
                    self_guid_valid = bool(uuid.UUID(row.get("self_pin_id", "")).int)
                except (TypeError, ValueError, AttributeError):
                    self_guid_valid = False
                if (not self_guid_valid or row.get("self_pin_name") != "self" or row.get("self_pin_direction") != "input"
                        or row.get("self_pin_type_text") != plan["expected_self_pin_type_text"]
                        or row.get("self_pin_owner_path") != path or row.get("self_pin_whole_current_verified") is not True
                        or type(row.get("self_pin_flags")) is not int or not 0 <= row["self_pin_flags"] <= 0xFFFFFFFF):
                    passed = False
                    break
            paths.add(path)
            guids.add(guid.hex)
    if not passed:
        raise OperationError("graph_native_string_vector_map_preflight_invalid" if mapping else "graph_native_vector_array_preflight_invalid", "Exact own String/Vector Map or Vector4[] or saved generated external Vector[]/Vector4[] source/property/GUID/default/current physical-pin observations are required")


_OBJECT_ARRAY_IDENTITY_FIELDS = (
    "member_scope", "variable_name", "variable_guid", "setter", "property_path", "property_owner_class_path",
    "skeleton_class_path", "object_constraint_path", "property_flags_string", "inner_property_flags_string",
    "property_array_dim", "inner_property_path", "object_array_property_identity_verified", "object_array_wrapper_matches",
    "pin_type", "expected_pin_type_text", "pin_category", "pin_subcategory", "native_object_array_source_before",
    "native_object_array_source_after", "native_object_array_source_observed_unchanged", "stored_default_before",
    "stored_default_after", "stored_default_observed_unchanged", "array_value_read_attempted", "array_default_write_attempted",
    "object_array_runtime_evaluation_verified",
)


def _own_object_array_plan(plan, variable_name, setter, target_class):
    """Keep finite own object-array metadata separate from scalar/external member admission."""
    constraint = plan.get("object_constraint_path")
    source, pin = plan.get("native_object_array_source_before"), plan.get("pin_type")
    try:
        guid_valid = bool(uuid.UUID(plan.get("variable_guid", "")).int)
    except (TypeError, ValueError, AttributeError):
        guid_valid = False
    expected_pin = {"category": "object", "subcategory": "None", "type_object": constraint,
        "container_type": 1, "is_reference": False, "is_const": False}
    passed = (not target_class and type(setter) is bool and plan.get("setter") is setter
        and plan.get("member_scope") == "own_native_object_array" and plan.get("variable_name") == variable_name
        and constraint in ("/Script/Engine.ActorComponent", "/Script/Engine.ChildActorComponent", "/Script/Engine.SkeletalMeshComponent") and guid_valid
        and (constraint != "/Script/Engine.SkeletalMeshComponent" or (setter is False and plan.get("native_skeletal_object_array_getter") is True))
        and plan.get("pin_category") == "object" and plan.get("pin_subcategory") == "None"
        and type(plan.get("property_array_dim")) is int and plan["property_array_dim"] == 1
        and plan.get("property_owner_class_path") == plan.get("skeleton_class_path")
        and all(type(plan.get(key)) is str and plan[key].startswith("/Game/")
                for key in ("property_path", "property_owner_class_path", "skeleton_class_path", "inner_property_path"))
        and all(type(plan.get(key)) is str and plan[key].isdecimal()
                for key in ("property_flags_string", "inner_property_flags_string"))
        and plan.get("object_array_property_identity_verified") is True and plan.get("object_array_wrapper_matches") is True
        and type(pin) is dict and all(pin.get(key) == value and type(pin.get(key)) is type(value) for key, value in expected_pin.items())
        and type(plan.get("expected_pin_type_text")) is str and bool(plan["expected_pin_type_text"])
        and type(source) is dict and source == plan.get("native_object_array_source_after")
        and source.get("observations_complete") is True and source.get("native_identity_verified") is True
        and source.get("class_path") == constraint and source.get("class_generated_by_path") == ""
        and source.get("package_path") == "/Script/Engine" and source.get("package_flags_string") == "16"
        and source.get("package_dirty") is False and source.get("existing_cdo_available") is True
        and source.get("existing_cdo_path") == "/Script/Engine.Default__" + constraint.rsplit(".", 1)[-1]
        and all(type(source.get(key)) is str and source[key].isdecimal() for key in ("class_flags_string", "existing_cdo_flags_string"))
        and source.get("cdo_creation_requested") is False and source.get("cdo_values_sampled") is False
        and plan.get("native_object_array_source_observed_unchanged") is True
        and type(plan.get("stored_default_before")) is str and plan["stored_default_before"] == plan.get("stored_default_after")
        and plan.get("stored_default_observed_unchanged") is True and plan.get("array_value_read_attempted") is False
        and plan.get("array_default_write_attempted") is False and plan.get("object_array_runtime_evaluation_verified") is False)
    if not passed:
        raise OperationError("graph_object_array_preflight_invalid", "Own object-array plan must prove an exact current ActorComponent/CAC or getter-only SkeletalMeshComponent strong array descriptor, skeleton property, GUID, native source metadata and preserved stored declaration default")


def _options(target_class, graph_id, graph_type, create_graph_if_missing, node_position_json, x, y):
    from . import functions
    if functions._bool(create_graph_if_missing, "create_graph_if_missing"):
        raise OperationError("graph_member_creation_unsupported", "Resolve the graph explicitly before adding a member node")
    options = {}
    for name, value in (("target_class", target_class), ("graph_id", graph_id), ("graph_type", graph_type)):
        if value is not None:
            value = functions._text(value, name, empty=True)
            if value:
                options[name] = value
    if node_position_json is not None:
        position = functions._json(node_position_json, "node_position_json")
        if position is None:
            position = [0, 0]
        if type(position) is not list or len(position) != 2 or any(type(v) is not int or abs(v) > 1000000 for v in position):
            raise OperationError("node_position_invalid", "node_position_json requires two integers from -1000000 to 1000000")
        if (x, y) != (0, 0) and [x, y] != position:
            raise OperationError("node_position_conflict", "Explicit node_position_json conflicts with existing x/y arguments")
        from .variables import _json_text
        options["node_position_json"] = _json_text(position)
    return options


_TEXTURE_IDENTITY_FIELDS = ("graph_path", "blueprint_path", "member_scope", "variable_name", "variable_guid", "setter",
    "target_class_path", "declaring_class_path", "declaring_blueprint_path", "property_path", "compiled_property_path",
    "pin_category", "pin_subcategory", "pin_type", "expected_pin_type_text", "expected_self_pin_type_text", "enum_path",
    "native_source_before", "native_source_after", "native_source_observed_unchanged", "native_property_identity_verified",
    "native_texture_runtime_value_written", "native_cdo_value_sampled")
_TEXTURE_NODE_FIELDS = ("object_path", "node_guid", "node_class_path", "graph_path", "outer_path", "member_name", "member_guid",
    "member_parent_class", "member_self_context", "property_path", "pins", "whole_pins_current_verified")


def _source_preserved(before, after, *, allow_load=False):
    if before and after and (before.get("scope") == "native_loaded_class" or after.get("scope") == "native_loaded_class"):
        return before == after and after.get("loaded") is True and after.get("class_path") in (api.NATIVE_PAWN, api.NATIVE_CHILD_ACTOR_COMPONENT)
    if not before or not after or not after.get("loaded") or after.get("dirty"):
        return False
    if before.get("file") != after.get("file") or before.get("class_path") != after.get("class_path"):
        return False
    if before.get("loaded"):
        return before == after
    return allow_load and not before.get("dirty")


_LOCAL_MAP_IDENTITY = (
    "member_scope", "variable_name", "variable_guid", "setter", "graph_path", "graph_id", "local_scope_name",
    "target_context", "function_library_profile", "target_parent_class_path", "function_entry_flags_string",
    "function_entry_path", "function_entry_guid", "property_path", "property_owner_function_path", "property_declaring_role",
    "native_type_path", "value_kind", "pin_type", "expected_pin_type_text", "stored_default_before", "stored_default_after",
    "property_flags_string", "key_property_path", "key_property_flags_string", "value_property_path", "value_property_flags_string",
    "local_descriptor_current_verified", "local_property_current_verified", "native_value_source_before", "native_value_source_after",
)


def _local_map_plan(plan, name, setter, target_class):
    terminal = {"category": "real", "subcategory": "double", "type_object": "", "is_const": False,
        "is_weak_pointer": False, "is_uobject_wrapper": False}
    expected = {"category": "string", "subcategory": "None", "type_object": "", "container_type": 3,
        "is_reference": False, "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False,
        "serialize_single_precision": False, "terminal": terminal}
    try:
        good = (type(setter) is bool and not target_class and plan.get("success") is True
            and plan.get("member_scope") == "local_native_string_double_map" and plan.get("setter") is setter
            and plan.get("variable_name", "").casefold() == name.casefold() and plan.get("pin_type") == expected
            and ((plan.get("target_context") == "normal_actor" and plan.get("function_library_profile") == ""
                and plan.get("target_parent_class_path") == "/Script/Engine.Actor")
                or (plan.get("target_context") == "preset_loader_library" and plan.get("function_library_profile") == "preset_loader"
                    and plan.get("target_parent_class_path") == "/Script/Engine.BlueprintFunctionLibrary"
                    and str(plan.get("local_scope_name", "")).casefold() == "presetloader"))
            and type(plan.get("function_entry_flags_string")) is str and plan["function_entry_flags_string"].isdigit()
            and plan.get("value_kind") == "string_double_map" and plan.get("native_type_path") == ""
            and all(type(plan.get(key)) is str and bool(plan[key]) for key in (
                "graph_path", "local_scope_name", "function_entry_path", "property_path", "property_owner_function_path", "expected_pin_type_text"))
            and all(bool(uuid.UUID(plan[key]).int) for key in ("variable_guid", "graph_id", "function_entry_guid"))
            and plan.get("property_declaring_role") == "skeleton"
            and all(plan.get(key) is True for key in ("local_descriptor_current_verified", "local_property_current_verified", "stored_default_observed_unchanged"))
            and all(plan.get(key) is False for key in ("runtime_value_read_attempted", "runtime_evaluation_verified", "default_write_attempted"))
            and plan.get("stored_default_before") == plan.get("stored_default_after") == ""
            and plan.get("native_value_source_observed_unchanged") is True
            and plan.get("native_value_source_before") == plan.get("native_value_source_after")
            and plan["native_value_source_before"].get("native_identity_verified") is True
            and plan["native_value_source_before"].get("scalar_property_class") == "DoubleProperty"
            and plan["native_value_source_before"].get("size") == plan["native_value_source_before"].get("alignment") == 8)
    except (ValueError, TypeError, KeyError, AttributeError):
        good = False
    if not good:
        raise OperationError("graph_local_map_plan_invalid", "Exact current own Actor function-local GUID/String-double Map property is required")


def _add_local_map(result, editor, before, plan, variable_name, x, y, checkpoint_callback):
    from .variables import _apply_native
    from .graph import _finish_node_add
    _local_map_plan(plan, variable_name, False, "")
    if plan["graph_path"] != before["graph_path"]:
        raise OperationError("graph_local_map_graph_changed", "Selected local graph changed before native dispatch")
    result["stage"] = "before_local_getter_spawn"
    result["data"].update(local_map_plan=copy.deepcopy(plan), mutation_state_unknown=False)
    result["changes"].append({"kind": "add_blueprint_variable_get_node", "graph_path": before["graph_path"],
        "attempted": False, "outcome": "not_yet_dispatched_at_checkpoint"})
    (checkpoint_callback or state.checkpoint)(result)
    result["data"]["mutation_state_unknown"] = True
    result["changes"][-1].update(attempted=True, outcome="unknown_until_native_result")
    native = graph_api.add_string_double_local_getter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
    _apply_native(result, "add_blueprint_variable_get_node", {"blueprint_path": before.get("blueprint_path", before["graph_path"].split(":", 1)[0]),
        "variable_name": variable_name}, native)
    # Failure/unknown above exits without another engine query. Success is still independently read back.
    result["data"]["mutation_state_unknown"] = True
    after_plan = api.member_plan(editor, variable_name, False)
    result["data"]["member_after"] = copy.deepcopy(after_plan)
    _local_map_plan(after_plan, variable_name, False, "")
    after = graph_api.graph_info(editor)
    # graph_info also calls public wrappers; close that boundary with a fresh native getter role.
    final_plan = api.member_plan(editor, variable_name, False)
    result["data"]["member_after_graph_readback"] = copy.deepcopy(final_plan)
    _local_map_plan(final_plan, variable_name, False, "")
    node = native["data"].get("node", {})
    rows = final_plan.get("native_value_member_nodes", [])
    selected = [row for row in rows if row.get("object_path") == node.get("node_id")]
    role = selected[0] if len(selected) == 1 else {}
    current = {row["node_id"]: row for row in after["nodes"]}
    observed = current.get(node.get("node_id"), {})
    verified = (all(plan.get(k) == after_plan.get(k) == final_plan.get(k) for k in _LOCAL_MAP_IDENTITY)
        and role.get("member_name") == plan["variable_name"] and role.get("member_guid") == plan["variable_guid"]
        and role.get("member_local_scope") == plan["local_scope_name"] and role.get("graph_path") == before["graph_path"]
        and role.get("node_class_path") == "/Script/BlueprintGraph.K2Node_VariableGet"
        and role.get("value_pin_owner_path") == node.get("node_id") and role.get("value_pin_id") == node.get("value_pin_id")
        and role.get("value_pin_name") == plan["variable_name"] and role.get("value_pin_direction") == "output"
        and role.get("value_pin_whole_current_verified") is True and role.get("pin_type") == plan["pin_type"]
        and role.get("property_path") == plan["property_path"] and role.get("node_guid", "").lower() == node.get("node_guid", "").lower()
        and node.get("local_reference_current_verified") is True and node.get("physical_pin_current_verified") is True
        and node.get("variable_guid") == plan["variable_guid"] and node.get("local_scope_name") == plan["local_scope_name"]
        and node.get("property_path") == plan["property_path"] and node.get("pin_type") == plan["pin_type"]
        and observed.get("node_guid", "").replace("-", "").lower() == node.get("node_guid", "").lower()
        and len(observed.get("pins", [])) == 1 and observed["pins"][0].get("name") == plan["variable_name"]
        and observed["pins"][0].get("direction") == "output" and observed["pins"][0].get("links") == [])
    result["data"].update(native_string_double_map_local_getter=True, runtime_map_assignment_verified=False)
    _finish_node_add(result, before, after, node.get("node_id"), "add_blueprint_variable_get_node", "K2Node_VariableGet", x, y, verified)


def _local_map_setter_observation(plan, node, observed=None):
    """Validate copied NS roles; stock flags/defaults are actual observations."""
    try:
        def guid(value):
            if type(value) is not str or not uuid.UUID(value).int:
                raise ValueError("nonzero copied GUID required")
            return uuid.UUID(value).hex
        if (type(node) is not dict or node.get("setter") is not True
                or node.get("node_id") != node.get("object_path") or not node.get("node_id")
                or node.get("node_class_path") != "/Script/BlueprintGraph.K2Node_VariableSet"
                or node.get("graph_path") != plan["graph_path"]
                or node.get("variable_name") != plan["variable_name"]
                or guid(node["variable_guid"]) != guid(plan["variable_guid"])
                or node.get("local_scope_name") != plan["local_scope_name"]
                or node.get("property_path") != plan["property_path"] or node.get("pin_type") != plan["pin_type"]
                or node.get("member_name") != plan["variable_name"]
                or guid(node["member_guid"]) != guid(plan["variable_guid"])
                or node.get("member_local_scope") != plan["local_scope_name"]
                or type(node.get("member_self_context")) is not bool
                or node.get("local_reference_current_verified") is not True
                or node.get("physical_pin_current_verified") is not True):
            raise ValueError("setter declaration/property/current node role differs")
        guid(node["node_guid"])
        pins = node["pins"]
        if type(pins) is not list or len(pins) != 4:
            raise ValueError("stock local setter whole pin inventory required")
        expected_roles = {("execute", "input"), ("then", "output"),
            (plan["variable_name"], "input"), ("Output_Get", "output")}
        roles, ids = {}, set()
        exec_type = {"category": "exec", "subcategory": "None", "type_object": "",
            "container_type": 0, "is_reference": False, "is_const": False}
        for pin in pins:
            key = pin["name"], pin["direction"]
            identity = guid(pin["pin_id"])
            if (key not in expected_roles or key in roles or identity in ids
                    or type(pin.get("flags")) is not int or not 0 <= pin["flags"] < 1024
                    or any(type(pin.get(k)) is not str for k in ("raw_default_value", "autogenerated_default_value", "default_object_path"))
                    or type(pin.get("default_text_empty")) is not bool
                    or type(pin.get("links")) is not list or pin["links"] != []):
                raise ValueError("physical setter pin/default/link observation differs")
            expected_type = exec_type if key[0] in ("execute", "then") else plan["pin_type"]
            if pin.get("pin_type") != expected_type:
                raise ValueError("whole stock execution/Map PinType differs")
            roles[key] = pin; ids.add(identity)
        if (set(roles) != expected_roles
                or guid(roles[(plan["variable_name"], "input")]["pin_id"]) != guid(node["value_pin_id"])
                or guid(roles[("Output_Get", "output")]["pin_id"]) != guid(node["output_pin_id"])):
            raise ValueError("Map input/Output_Get identity differs")
        if observed is not None:
            public = {(p["name"], p["direction"]): p for p in observed["pins"]}
            if (observed.get("node_id") != node["node_id"] or observed.get("class") != "K2Node_VariableSet"
                    or guid(observed["node_guid"]) != guid(node["node_guid"])
                    or len(observed["pins"]) != 4 or set(public) != expected_roles
                    or any(public[key].get("links") != [] for key in expected_roles)):
                raise ValueError("independent public stock-node/GUID/whole-port projection differs")
        return True
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        raise OperationError("graph_local_map_setter_observation_invalid", "Actual setter prefix retained; current local GUID/property/full pin inventory is unverified") from error


def _add_local_map_setter(result, editor, before, plan, variable_name, x, y, checkpoint_callback):
    from .variables import _apply_native
    from .graph import _finish_node_add
    _local_map_plan(plan, variable_name, True, "")
    if plan["graph_path"] != before["graph_path"]:
        raise OperationError("graph_local_map_graph_changed", "Selected graph changed before native setter dispatch")
    result["stage"] = "before_local_setter_spawn"
    result["data"].update(local_map_plan=copy.deepcopy(plan), mutation_state_unknown=False)
    result["changes"].append({"kind": "add_blueprint_variable_set_node", "graph_path": before["graph_path"],
        "attempted": False, "outcome": "not_yet_dispatched_at_checkpoint"})
    (checkpoint_callback or state.checkpoint)(result)
    result["data"]["mutation_state_unknown"] = True
    result["changes"][-1].update(attempted=True, outcome="unknown_until_native_result")
    try:
        native = graph_api.add_string_double_local_setter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
    except OperationError as error:
        if error.code == "native_local_map_api_unavailable":
            result["data"].update(mutation_state_unknown=False, native_dispatched=False,
                native_mutation_attempted=False, mutation_performed=False)
            result["changes"][-1].update(attempted=False, changed=False, change_state_known=True,
                outcome="exact_native_api_unavailable_before_engine_dispatch")
        raise
    _apply_native(result, "add_blueprint_variable_set_node", {"blueprint_path": before.get("blueprint_path", before["graph_path"].split(":", 1)[0]),
        "variable_name": variable_name}, native)
    # Native failure/unknown above exits before another SDK call.
    result["data"]["mutation_state_unknown"] = True
    data = native["data"]; node = data.get("node", {})
    if data.get("native_string_double_map_local_setter") is not True:
        raise OperationError("graph_local_map_setter_result_invalid", "Missing exact setter receipt; no follow-up SDK dispatched")
    preflight = data.get("local_preflight", {})
    _local_map_plan(preflight, variable_name, True, "")
    if not all(plan.get(k) == preflight.get(k) for k in _LOCAL_MAP_IDENTITY):
        raise OperationError("graph_local_map_setter_preflight_changed", "Native identity/source differs; actual prefix retained without follow-up SDK")
    _local_map_setter_observation(plan, node)
    try:
        after_plan = api.member_plan(editor, variable_name, True)
        result["data"]["member_after"] = copy.deepcopy(after_plan)
        _local_map_plan(after_plan, variable_name, True, "")
        first_rows = after_plan.get("native_local_setter_nodes")
        old_rows = plan.get("native_local_setter_nodes")
        first_selected = [row for row in first_rows if type(row) is dict and row.get("node_id") == node.get("node_id")] if type(first_rows) is list else []
        first_old = [row for row in first_rows if type(row) is dict and row.get("node_id") != node.get("node_id")] if type(first_rows) is list else []
        if (not all(plan.get(k) == after_plan.get(k) for k in _LOCAL_MAP_IDENTITY)
                or type(first_rows) is not list or any(type(row) is not dict for row in first_rows)
                or len(first_selected) != 1 or first_selected[0] != node
                or type(old_rows) is not list or first_old != old_rows):
            raise OperationError("graph_local_map_setter_current_changed", "First current fence differs from original identity/returned full node/prior roles; no follow-up SDK")
        after = graph_api.graph_info(editor)
        final_plan = api.member_plan(editor, variable_name, True)
        result["data"]["member_after_graph_readback"] = copy.deepcopy(final_plan)
        _local_map_plan(final_plan, variable_name, True, "")
    except Exception as error:
        failed = getattr(error, "native_graph_result", None)
        if type(failed) is dict:
            result["data"]["local_setter_readback_failure"] = copy.deepcopy(failed)
        raise
    rows = final_plan.get("native_local_setter_nodes", [])
    if type(rows) is not list:
        raise OperationError("graph_local_map_setter_roles_invalid", "Fresh whole local setter roles missing")
    selected = [row for row in rows if row.get("node_id") == node.get("node_id")]
    role = selected[0] if len(selected) == 1 else {}
    current = {row["node_id"]: row for row in after["nodes"]}
    _local_map_setter_observation(final_plan, role, current.get(node.get("node_id"), {}))
    prior_rows = plan.get("native_local_setter_nodes")
    observed_old = [row for row in rows if row.get("node_id") != node.get("node_id")]
    verified = (all(plan.get(k) == after_plan.get(k) == final_plan.get(k) for k in _LOCAL_MAP_IDENTITY)
        and role == node and type(prior_rows) is list and observed_old == prior_rows
        and after_plan.get("native_local_setter_nodes") == rows)
    result["data"].update(native_string_double_map_local_setter=True, runtime_map_assignment_verified=False,
        local_setter_whole_pin_roles_verified=verified,
        local_setter_default_scope="Actual stock flags/raw/Auto/object/text observations compared; not equated with stored declaration default")
    _finish_node_add(result, before, after, node.get("node_id"), "add_blueprint_variable_set_node", "K2Node_VariableSet", x, y, verified)


_LOCAL_PRIMITIVE_IDENTITY = (
    "member_scope", "variable_name", "variable_guid", "setter", "graph_path", "graph_id", "local_scope_name",
    "target_context", "function_library_profile", "target_parent_class_path", "function_entry_flags_string",
    "function_entry_path", "function_entry_guid", "property_path", "property_owner_function_path", "property_declaring_role",
    "native_type_path", "value_kind", "pin_type", "expected_pin_type_text", "stored_default_before", "stored_default_after",
    "property_flags_string", "local_descriptor_current_verified", "local_property_current_verified",
    "native_value_source_before", "native_value_source_after",
)


def _local_primitive_plan(plan, name, setter, target_class):
    """Finite copied scalar-local plan; declaration default is not a pin default."""
    kinds = {"bool": ("bool", "None", "BoolProperty"), "int": ("int", "None", "IntProperty"),
        "int64": ("int64", "None", "Int64Property"), "float": ("real", "float", "FloatProperty"),
        "double": ("real", "double", "DoubleProperty"), "string": ("string", "None", "StrProperty"),
        "name": ("name", "None", "NameProperty"), "text": ("text", "None", "TextProperty")}
    terminal = {"category": "None", "subcategory": "None", "type_object": "", "is_const": False,
        "is_weak_pointer": False, "is_uobject_wrapper": False}
    try:
        category, subcategory, property_class = kinds[plan["value_kind"]]
        pin_type = plan["pin_type"]
        expected = {"category": category, "subcategory": subcategory, "type_object": "", "container_type": 0,
            "is_reference": False, "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False,
            "serialize_single_precision": False, "terminal": terminal}
        source = plan["native_value_source_before"]
        good = (type(setter) is bool and not target_class and plan.get("success") is True
            and plan.get("member_scope") == "local_primitive" and plan.get("setter") is setter
            and plan.get("variable_name", "").casefold() == name.casefold() and pin_type == expected
            and type(pin_type.get("container_type")) is int
            and all(type(pin_type.get(key)) is bool for key in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and all(type(pin_type["terminal"].get(key)) is bool for key in ("is_const", "is_weak_pointer", "is_uobject_wrapper"))
            and ((plan.get("target_context") == "normal_actor" and plan.get("function_library_profile") == ""
                and plan.get("target_parent_class_path") == "/Script/Engine.Actor")
                or (plan.get("target_context") == "preset_loader_library" and plan.get("function_library_profile") == "preset_loader"
                    and plan.get("target_parent_class_path") == "/Script/Engine.BlueprintFunctionLibrary"
                    and plan.get("local_scope_name", "").casefold() == "presetloader"))
            and all(type(plan.get(key)) is str and bool(plan[key]) for key in (
                "graph_path", "local_scope_name", "function_entry_path", "property_path", "property_owner_function_path", "expected_pin_type_text"))
            and all(type(plan.get(key)) is str and plan[key].isascii() and plan[key].isdigit()
                for key in ("function_entry_flags_string", "property_flags_string"))
            and all(bool(uuid.UUID(plan[key]).int) for key in ("variable_guid", "graph_id", "function_entry_guid"))
            and plan.get("property_declaring_role") == "skeleton" and plan.get("native_type_path") == ""
            and all(plan.get(key) is True for key in ("local_descriptor_current_verified", "local_property_current_verified", "stored_default_observed_unchanged", "native_value_source_observed_unchanged"))
            and all(plan.get(key) is False for key in ("runtime_value_read_attempted", "runtime_evaluation_verified", "default_write_attempted"))
            and type(plan.get("stored_default_before")) is str and plan["stored_default_before"] == plan.get("stored_default_after")
            and type(source) is dict and source == plan.get("native_value_source_after")
            and source.get("observations_complete") is True and source.get("native_identity_verified") is True
            and source.get("scalar_property_class") == property_class
            and source.get("scalar_category") == category and source.get("scalar_subcategory") == subcategory
            and type(source.get("size")) is int and source["size"] > 0
            and type(source.get("alignment")) is int and source["alignment"] > 0)
    except (KeyError, TypeError, ValueError, AttributeError):
        good = False
    if not good:
        raise OperationError("graph_local_primitive_plan_invalid", "Exact current own function-local GUID/primitive property/source plan is unverified; no fallback or value access")


def _local_primitive_node(plan, node, setter, observed=None):
    """Validate actual whole stock pins without imposing declaration/Auto defaults."""
    try:
        def guid(value):
            if type(value) is not str or not uuid.UUID(value).int:
                raise ValueError("nonzero current GUID required")
            return uuid.UUID(value).hex
        expected_class = "/Script/BlueprintGraph.K2Node_VariableSet" if setter else "/Script/BlueprintGraph.K2Node_VariableGet"
        if (type(node) is not dict or node.get("setter") is not setter
                or not node.get("node_id") or node.get("node_id") != node.get("object_path")
                or node.get("node_class_path") != expected_class or node.get("graph_path") != plan["graph_path"]
                or node.get("variable_name") != plan["variable_name"] or node.get("member_name") != plan["variable_name"]
                or guid(node["variable_guid"]) != guid(plan["variable_guid"]) or guid(node["member_guid"]) != guid(plan["variable_guid"])
                or node.get("local_scope_name") != plan["local_scope_name"] or node.get("member_local_scope") != plan["local_scope_name"]
                or type(node.get("member_self_context")) is not bool or node.get("property_path") != plan["property_path"]
                or node.get("pin_type") != plan["pin_type"] or node.get("local_reference_current_verified") is not True
                or node.get("physical_pin_current_verified") is not True):
            raise ValueError("current local node/reference/property role differs")
        guid(node["node_guid"])
        pins = node["pins"]
        roles = {(plan["variable_name"], "output")} if not setter else {
            ("execute", "input"), ("then", "output"), (plan["variable_name"], "input"), ("Output_Get", "output")}
        if type(pins) is not list or len(pins) != len(roles):
            raise ValueError("whole local stock pin inventory differs")
        exec_type = dict(plan["pin_type"], category="exec", subcategory="None")
        current, ids = {}, set()
        for pin in pins:
            key = pin["name"], pin["direction"]
            identity = guid(pin["pin_id"])
            expected_type = exec_type if setter and key[0] in ("execute", "then") else plan["pin_type"]
            if (key not in roles or key in current or identity in ids or pin.get("pin_type") != expected_type
                    or type(pin.get("flags")) is not int or not 0 <= pin["flags"] < 1024
                    or any(type(pin.get(key)) is not str for key in ("raw_default_value", "autogenerated_default_value", "default_object_path"))
                    or type(pin.get("default_text_empty")) is not bool or type(pin.get("links")) is not list or pin["links"] != []):
                raise ValueError("actual full scalar pin/default/link descriptor differs")
            current[key] = pin; ids.add(identity)
        value_role = plan["variable_name"], "input" if setter else "output"
        if set(current) != roles or guid(node["value_pin_id"]) != guid(current[value_role]["pin_id"]):
            raise ValueError("current local value pin identity differs")
        if setter and guid(node["output_pin_id"]) != guid(current[("Output_Get", "output")]["pin_id"]):
            raise ValueError("current Output_Get pin identity differs")
        if observed is not None:
            public = {(p["name"], p["direction"]): p for p in observed["pins"]}
            if (observed.get("node_id") != node["node_id"] or observed.get("class") != expected_class.rsplit(".", 1)[1]
                    or guid(observed["node_guid"]) != guid(node["node_guid"])
                    or len(observed["pins"]) != len(roles) or set(public) != roles
                    or any(public[key].get("links") != [] for key in roles)):
                raise ValueError("independent public node/GUID/whole-port projection differs")
        return True
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        raise OperationError("graph_local_primitive_node_unverified", "Actual primitive local node prefix retained; whole current role/pins are unverified") from error


def _add_primitive_local(result, editor, before, plan, variable_name, x, y, setter, checkpoint_callback):
    from .variables import _apply_native
    from .graph import _finish_node_add
    _local_primitive_plan(plan, variable_name, setter, "")
    identity = "add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node"
    marker = "native_primitive_local_setter" if setter else "native_primitive_local_getter"
    if plan["graph_path"] != before["graph_path"]:
        raise OperationError("graph_local_primitive_graph_changed", "Original local graph changed before dispatch")
    result["stage"] = "before_primitive_local_spawn"
    result["data"].update(local_primitive_plan=copy.deepcopy(plan), mutation_state_unknown=False)
    result["changes"].append({"kind": identity, "graph_path": before["graph_path"], "attempted": False,
        "outcome": "not_yet_dispatched_at_checkpoint"})
    (checkpoint_callback or state.checkpoint)(result)
    result["data"]["mutation_state_unknown"] = True
    result["changes"][-1].update(attempted=True, outcome="unknown_until_native_result")
    try:
        if setter:
            native = graph_api.add_primitive_local_setter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
        else:
            native = graph_api.add_primitive_local_getter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
    except OperationError as error:
        if error.code == "native_local_primitive_api_unavailable":
            result["data"].update(mutation_state_unknown=False, native_dispatched=False, native_mutation_attempted=False, mutation_performed=False)
            result["changes"][-1].update(attempted=False, changed=False, change_state_known=True,
                outcome="exact_native_api_unavailable_before_engine_dispatch")
        raise
    _apply_native(result, identity, {"blueprint_path": before.get("blueprint_path", before["graph_path"].split(":", 1)[0]), "variable_name": variable_name}, native)
    # A failed or unknown native result above exits before any later SDK call.
    result["data"]["mutation_state_unknown"] = True
    data = native["data"]; node = data.get("node", {}); preflight = data.get("local_preflight", {})
    if data.get(marker) is not True:
        raise OperationError("graph_local_primitive_result_invalid", "Exact scalar local result marker missing; no SDK follow-up")
    _local_primitive_plan(preflight, variable_name, setter, "")
    if not all(plan.get(key) == preflight.get(key) for key in _LOCAL_PRIMITIVE_IDENTITY):
        raise OperationError("graph_local_primitive_preflight_changed", "Original current identity/source differs; actual prefix retained without SDK follow-up")
    _local_primitive_node(plan, node, setter)
    try:
        first = api.member_plan(editor, variable_name, setter)
        result["data"]["member_after"] = copy.deepcopy(first)
        _local_primitive_plan(first, variable_name, setter, "")
        rows, old = first.get("native_local_primitive_nodes"), plan.get("native_local_primitive_nodes")
        selected = [row for row in rows if type(row) is dict and row.get("node_id") == node.get("node_id")] if type(rows) is list else []
        retained = [row for row in rows if type(row) is dict and row.get("node_id") != node.get("node_id")] if type(rows) is list else []
        if (not all(plan.get(key) == first.get(key) for key in _LOCAL_PRIMITIVE_IDENTITY)
                or type(rows) is not list or any(type(row) is not dict for row in rows)
                or len(selected) != 1 or selected[0] != node or type(old) is not list or retained != old):
            raise OperationError("graph_local_primitive_current_changed", "First current role differs from original identity/returned whole node/prior nodes; no SDK follow-up")
        after = graph_api.graph_info(editor)
        final = api.member_plan(editor, variable_name, setter)
        result["data"]["member_after_graph_readback"] = copy.deepcopy(final)
        _local_primitive_plan(final, variable_name, setter, "")
        if (not all(plan.get(key) == final.get(key) for key in _LOCAL_PRIMITIVE_IDENTITY)
                or final.get("native_local_primitive_nodes") != rows):
            raise OperationError("graph_local_primitive_current_changed", "Current identity/full physical node roles changed during public readback; no SDK follow-up")
    except Exception as error:
        failed = getattr(error, "native_graph_result", None)
        if type(failed) is dict:
            result["data"]["local_primitive_readback_failure"] = copy.deepcopy(failed)
        raise
    observed = {row["node_id"]: row for row in after["nodes"]}.get(node["node_id"], {})
    _local_primitive_node(final, selected[0], setter, observed)
    result["data"].update(primitive_local_whole_node_verified=True, runtime_local_value_verified=False,
        primitive_local_default_scope="Stored declaration default and stock raw/Auto/object/text pin defaults are distinct preserved observations; no default/value assignment")
    _finish_node_add(result, before, after, node["node_id"], identity,
        "K2Node_VariableSet" if setter else "K2Node_VariableGet", x, y, True)


# The current graph/reference/default identity keys are identical to NU;
# native struct source/layout admission below remains a separate finite contract.
_LOCAL_VALUE_STRUCT_IDENTITY = _LOCAL_PRIMITIVE_IDENTITY


def _local_scalar_uds_plan(plan, name, setter, target_class):
    """Copied saved-clean scalar UDS/own-function binding, without values or buffers."""
    try:
        path = plan["native_type_path"]
        terminal = {"category": "None", "subcategory": "None", "type_object": "", "is_const": False,
            "is_weak_pointer": False, "is_uobject_wrapper": False}
        expected = {"category": "struct", "subcategory": "None", "type_object": path, "container_type": 0,
            "is_reference": False, "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False,
            "serialize_single_precision": False, "terminal": terminal}
        pin = plan["pin_type"]; source = plan["native_value_source_before"]; after = plan["native_value_source_after"]
        good = (type(setter) is bool and not target_class and plan.get("success") is True
            and plan.get("member_scope") == "local_native_value_struct" and plan.get("setter") is setter
            and plan.get("native_scalar_uds_local_plan") is True and plan.get("value_kind") == "scalar_uds"
            and plan.get("variable_name", "").casefold() == name.casefold()
            and type(path) is str and path.startswith("/Game/") and "." in path
            and pin == expected and type(pin.get("container_type")) is int
            and all(type(pin.get(k)) is bool for k in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and all(type(pin["terminal"].get(k)) is bool for k in ("is_const", "is_weak_pointer", "is_uobject_wrapper"))
            and ((plan.get("target_context") == "normal_actor" and plan.get("function_library_profile") == ""
                and plan.get("target_parent_class_path") == "/Script/Engine.Actor")
                or (plan.get("target_context") == "preset_loader_library" and plan.get("function_library_profile") == "preset_loader"
                    and plan.get("target_parent_class_path") == "/Script/Engine.BlueprintFunctionLibrary"
                    and plan.get("local_scope_name", "").casefold() == "presetloader"))
            and all(type(plan.get(k)) is str and bool(plan[k]) for k in (
                "graph_path", "local_scope_name", "function_entry_path", "property_path", "property_owner_function_path", "expected_pin_type_text"))
            and all(type(plan.get(k)) is str and plan[k].isascii() and plan[k].isdigit() for k in ("function_entry_flags_string", "property_flags_string"))
            and all(bool(uuid.UUID(plan[k]).int) for k in ("variable_guid", "graph_id", "function_entry_guid"))
            and plan.get("property_declaring_role") == "skeleton"
            and all(plan.get(k) is True for k in ("local_descriptor_current_verified", "local_property_current_verified", "stored_default_observed_unchanged", "native_value_source_observed_unchanged"))
            and all(plan.get(k) is False for k in ("runtime_value_read_attempted", "runtime_evaluation_verified", "default_write_attempted"))
            and type(plan.get("stored_default_before")) is str and plan["stored_default_before"] == plan.get("stored_default_after")
            and type(source) is dict and type(after) is dict
            and json.dumps(source, sort_keys=True, ensure_ascii=False, separators=(",", ":")) == json.dumps(after, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
            and source.get("observations_complete") is True and source.get("native_identity_verified") is True
            and source.get("struct_path") == path and bool(uuid.UUID(source["struct_guid"]).int)
            and source.get("package_dirty") is False and type(source.get("status")) is int
            and all(source.get(k) is False for k in ("values_sampled", "default_buffer_sampled", "cdo_creation_requested", "runtime_invocation_verified"))
            and all(type(source.get(k)) is int and source[k] >= 0 for k in ("struct_size", "struct_alignment"))
            and all(type(source.get(k)) is str and bool(source[k]) for k in ("package_path", "editor_data_path"))
            and all(type(source.get(k)) is str and source[k].isascii() and source[k].isdigit() for k in ("struct_flags_string", "package_flags_string"))
            and all(type(source.get(k)) is dict and all(type(a) is str and type(b) is str for a, b in source[k].items()) for k in ("root_metadata", "editor_data_metadata")))
        binding = source.get("property_binding", {})
        good = (good and type(binding) is dict and binding.get("property_path") == plan["property_path"]
            and binding.get("property_class") == "StructProperty" and binding.get("owner_function_path") == plan["property_owner_function_path"]
            and binding.get("struct_path") == path and binding.get("source_handle_resolved") is True and binding.get("current_verified") is True)
        fields = source.get("fields")
        good = good and type(fields) is list and 1 <= len(fields) <= 32 and all(type(f) is dict for f in fields)
        scalar = {"StrProperty": ("string", "None"), "IntProperty": ("int", "None"), "Int64Property": ("int64", "None"),
            "FloatProperty": ("real", "float"), "DoubleProperty": ("real", "double"), "BoolProperty": ("bool", "None"), "NameProperty": ("name", "None")}
        names, guids = set(), set()
        if good:
            for field in fields:
                identity = uuid.UUID(field["field_guid"])
                good = (bool(identity.int) and identity.hex not in guids and type(field.get("internal_name")) is str
                    and bool(field["internal_name"]) and field["internal_name"] not in names and type(field.get("authored_name")) is str
                    and field.get("owner_path") == path and field.get("class") in scalar
                    and (field.get("category"), field.get("subcategory")) == scalar[field["class"]]
                    and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit()
                    and all(type(field.get(k)) is int and field[k] >= 0 for k in ("offset", "size"))
                    and type(field.get("array_dim")) is int and field["array_dim"] == 1
                    and type(field.get("metadata")) is dict and all(type(a) is str and type(b) is str for a, b in field["metadata"].items()))
                if not good:
                    break
                names.add(field["internal_name"]); guids.add(identity.hex)
        file = source.get("file", {})
        good = (good and type(file) is dict and file.get("exists") is True and type(file.get("filename")) is str and bool(file["filename"])
            and type(file.get("bytes")) is int and file["bytes"] >= 0 and type(file.get("sha1")) is str and len(file["sha1"]) == 40
            and all(c in "0123456789abcdefABCDEF" for c in file["sha1"]))
    except (KeyError, TypeError, ValueError, AttributeError):
        good = False
    if not good:
        raise OperationError("graph_local_scalar_uds_plan_invalid", "Current scalar UDS local GUID/function binding/source metadata/file is unverified; no generic struct fallback or SDK follow-up")


def _local_value_struct_plan(plan, name, setter, target_class):
    """Copied finite scalar struct source/layout; no runtime/split access."""
    if plan.get("value_kind") == "scalar_uds" or "native_scalar_uds_local_plan" in plan:
        return _local_scalar_uds_plan(plan, name, setter, target_class)
    kinds = {"vector": ("/Script/CoreUObject.Vector", {"X", "Y", "Z"}),
        "rotator": ("/Script/CoreUObject.Rotator", {"Pitch", "Yaw", "Roll"}),
        "transform": ("/Script/CoreUObject.Transform", {"Rotation", "Translation", "Scale3D"})}
    terminal = {"category": "None", "subcategory": "None", "type_object": "", "is_const": False,
        "is_weak_pointer": False, "is_uobject_wrapper": False}
    try:
        type_path, names = kinds[plan["value_kind"]]
        pin_type = plan["pin_type"]
        expected = {"category": "struct", "subcategory": "None", "type_object": type_path, "container_type": 0,
            "is_reference": False, "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False,
            "serialize_single_precision": False, "terminal": terminal}
        source = plan["native_value_source_before"]
        fields = source["properties"]
        good = (type(setter) is bool and not target_class and plan.get("success") is True
            and plan.get("member_scope") == "local_native_value_struct" and plan.get("setter") is setter
            and plan.get("variable_name", "").casefold() == name.casefold() and plan.get("native_type_path") == type_path
            and pin_type == expected and type(pin_type.get("container_type")) is int
            and all(type(pin_type.get(key)) is bool for key in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and all(type(pin_type["terminal"].get(key)) is bool for key in ("is_const", "is_weak_pointer", "is_uobject_wrapper"))
            and ((plan.get("target_context") == "normal_actor" and plan.get("function_library_profile") == ""
                and plan.get("target_parent_class_path") == "/Script/Engine.Actor")
                or (plan.get("target_context") == "preset_loader_library" and plan.get("function_library_profile") == "preset_loader"
                    and plan.get("target_parent_class_path") == "/Script/Engine.BlueprintFunctionLibrary"
                    and plan.get("local_scope_name", "").casefold() == "presetloader"))
            and all(type(plan.get(key)) is str and bool(plan[key]) for key in (
                "graph_path", "local_scope_name", "function_entry_path", "property_path", "property_owner_function_path", "expected_pin_type_text"))
            and all(type(plan.get(key)) is str and plan[key].isascii() and plan[key].isdigit()
                for key in ("function_entry_flags_string", "property_flags_string"))
            and all(bool(uuid.UUID(plan[key]).int) for key in ("variable_guid", "graph_id", "function_entry_guid"))
            and plan.get("property_declaring_role") == "skeleton"
            and all(plan.get(key) is True for key in ("local_descriptor_current_verified", "local_property_current_verified", "stored_default_observed_unchanged", "native_value_source_observed_unchanged"))
            and all(plan.get(key) is False for key in ("runtime_value_read_attempted", "runtime_evaluation_verified", "default_write_attempted"))
            and type(plan.get("stored_default_before")) is str and plan["stored_default_before"] == plan.get("stored_default_after")
            and type(source) is dict and source == plan.get("native_value_source_after")
            and source.get("observations_complete") is True and source.get("native_identity_verified") is True
            and type(source.get("type_kind")) is str and bool(source["type_kind"])
            and source.get("struct_path") == source.get("type_path") == type_path
            and source.get("package_path") == "/Script/CoreUObject" and source.get("package_dirty") is False
            and type(source.get("metadata")) is dict
            and all(type(source.get(key)) is str and source[key].isascii() and source[key].isdigit()
                for key in ("package_flags_string", "type_flags_string"))
            and source.get("scalar_property_class") == "StructProperty" and source.get("scalar_category") == "struct"
            and source.get("scalar_subcategory") == "None"
            and all(type(source.get(key)) is int and source[key] > 0 for key in ("size", "alignment", "type_structure_size", "type_alignment"))
            and type(fields) is list and len(fields) == 3
            and all(type(field) is dict for field in fields)
            and {field["name"] for field in fields} == names
            and len({field["path"] for field in fields}) == 3)
        if good and plan["value_kind"] == "transform":
            good = _local_transform_source(plan, source, array=False)
        elif good:
            for field in fields:
                good = (field.get("class") == "DoubleProperty" and field.get("owner") == type_path
                    and type(field.get("path")) is str and bool(field["path"])
                    and type(field.get("offset")) is int and field["offset"] >= 0
                    and all(type(field.get(key)) is int and field[key] > 0 for key in ("size", "alignment"))
                    and type(field.get("array_dim")) is int and field["array_dim"] == 1
                    and field["offset"] + field["size"] <= source["type_structure_size"]
                    and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit())
                if not good:
                    break
    except (KeyError, TypeError, ValueError, AttributeError):
        good = False
    if not good:
        raise OperationError("graph_local_value_struct_plan_invalid", "Exact current local GUID/native Vector, Rotator or Transform source/layout is unverified; no generic struct fallback")


def _local_value_struct_node(plan, node, setter, observed=None):
    """Validate actual whole stock pins without imposing declaration/Auto defaults."""
    try:
        def guid(value):
            if type(value) is not str or not uuid.UUID(value).int:
                raise ValueError("nonzero current GUID required")
            return uuid.UUID(value).hex
        expected_class = "/Script/BlueprintGraph.K2Node_VariableSet" if setter else "/Script/BlueprintGraph.K2Node_VariableGet"
        if (type(node) is not dict or node.get("setter") is not setter
                or not node.get("node_id") or node.get("node_id") != node.get("object_path")
                or node.get("node_class_path") != expected_class or node.get("graph_path") != plan["graph_path"]
                or node.get("variable_name") != plan["variable_name"] or node.get("member_name") != plan["variable_name"]
                or guid(node["variable_guid"]) != guid(plan["variable_guid"]) or guid(node["member_guid"]) != guid(plan["variable_guid"])
                or node.get("local_scope_name") != plan["local_scope_name"] or node.get("member_local_scope") != plan["local_scope_name"]
                or type(node.get("member_self_context")) is not bool or node.get("property_path") != plan["property_path"]
                or node.get("pin_type") != plan["pin_type"] or node.get("local_reference_current_verified") is not True
                or node.get("physical_pin_current_verified") is not True):
            raise ValueError("current local node/reference/property role differs")
        guid(node["node_guid"])
        pins = node["pins"]
        roles = {(plan["variable_name"], "output")} if not setter else {
            ("execute", "input"), ("then", "output"), (plan["variable_name"], "input"), ("Output_Get", "output")}
        if type(pins) is not list or len(pins) != len(roles):
            raise ValueError("whole local stock pin inventory differs")
        exec_type = dict(plan["pin_type"], category="exec", subcategory="None", type_object="")
        current, ids = {}, set()
        for pin in pins:
            key = pin["name"], pin["direction"]
            identity = guid(pin["pin_id"])
            expected_type = exec_type if setter and key[0] in ("execute", "then") else plan["pin_type"]
            if (key not in roles or key in current or identity in ids or pin.get("pin_type") != expected_type
                    or type(pin.get("flags")) is not int or not 0 <= pin["flags"] < 1024
                    or any(type(pin.get(key)) is not str for key in ("raw_default_value", "autogenerated_default_value", "default_object_path"))
                    or type(pin.get("default_text_empty")) is not bool or type(pin.get("links")) is not list or pin["links"] != []):
                raise ValueError("actual full scalar pin/default/link descriptor differs")
            current[key] = pin; ids.add(identity)
        value_role = plan["variable_name"], "input" if setter else "output"
        if set(current) != roles or guid(node["value_pin_id"]) != guid(current[value_role]["pin_id"]):
            raise ValueError("current local value pin identity differs")
        if setter and guid(node["output_pin_id"]) != guid(current[("Output_Get", "output")]["pin_id"]):
            raise ValueError("current Output_Get pin identity differs")
        if observed is not None:
            public = {(p["name"], p["direction"]): p for p in observed["pins"]}
            if (observed.get("node_id") != node["node_id"] or observed.get("class") != expected_class.rsplit(".", 1)[1]
                    or guid(observed["node_guid"]) != guid(node["node_guid"])
                    or len(observed["pins"]) != len(roles) or set(public) != roles
                    or any(public[key].get("links") != [] for key in roles)):
                raise ValueError("independent public node/GUID/whole-port projection differs")
        return True
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        raise OperationError("graph_local_value_struct_node_unverified", "Actual native value local node prefix retained; whole current role/pins are unverified") from error


def _add_native_value_local(result, editor, before, plan, variable_name, x, y, setter, checkpoint_callback):
    from .variables import _apply_native
    from .graph import _finish_node_add
    _local_value_struct_plan(plan, variable_name, setter, "")
    identity = "add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node"
    marker = "native_value_struct_local_setter" if setter else "native_value_struct_local_getter"
    if plan["graph_path"] != before["graph_path"]:
        raise OperationError("graph_local_value_struct_graph_changed", "Original local graph changed before dispatch")
    result["stage"] = "before_native_value_local_spawn"
    result["data"].update(local_value_struct_plan=copy.deepcopy(plan), mutation_state_unknown=False)
    result["changes"].append({"kind": identity, "graph_path": before["graph_path"], "attempted": False,
        "outcome": "not_yet_dispatched_at_checkpoint"})
    (checkpoint_callback or state.checkpoint)(result)
    result["data"]["mutation_state_unknown"] = True
    result["changes"][-1].update(attempted=True, outcome="unknown_until_native_result")
    try:
        if setter:
            native = graph_api.add_native_value_local_setter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
        else:
            native = graph_api.add_native_value_local_getter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
    except OperationError as error:
        if error.code == "native_local_value_struct_api_unavailable":
            result["data"].update(mutation_state_unknown=False, native_dispatched=False, native_mutation_attempted=False, mutation_performed=False)
            result["changes"][-1].update(attempted=False, changed=False, change_state_known=True,
                outcome="exact_native_api_unavailable_before_engine_dispatch")
        raise
    _apply_native(result, identity, {"blueprint_path": before.get("blueprint_path", before["graph_path"].split(":", 1)[0]), "variable_name": variable_name}, native)
    # A failed or unknown native result above exits before any later SDK call.
    result["data"]["mutation_state_unknown"] = True
    data = native["data"]; node = data.get("node", {}); preflight = data.get("local_preflight", {})
    if (data.get(marker) is not True or plan.get("value_kind") == "scalar_uds"
            and data.get("native_scalar_uds_local_setter" if setter else "native_scalar_uds_local_getter") is not True):
        raise OperationError("graph_local_value_struct_result_invalid", "Finite scalar struct local result marker missing; no SDK follow-up")
    _local_value_struct_plan(preflight, variable_name, setter, "")
    if not all(plan.get(key) == preflight.get(key) for key in _LOCAL_VALUE_STRUCT_IDENTITY):
        raise OperationError("graph_local_value_struct_preflight_changed", "Original current identity/source differs; actual prefix retained without SDK follow-up")
    _local_value_struct_node(plan, node, setter)
    try:
        first = api.member_plan(editor, variable_name, setter)
        result["data"]["member_after"] = copy.deepcopy(first)
        _local_value_struct_plan(first, variable_name, setter, "")
        rows, old = first.get("native_local_value_struct_nodes"), plan.get("native_local_value_struct_nodes")
        selected = [row for row in rows if type(row) is dict and row.get("node_id") == node.get("node_id")] if type(rows) is list else []
        retained = [row for row in rows if type(row) is dict and row.get("node_id") != node.get("node_id")] if type(rows) is list else []
        if (not all(plan.get(key) == first.get(key) for key in _LOCAL_VALUE_STRUCT_IDENTITY)
                or type(rows) is not list or any(type(row) is not dict for row in rows)
                or len(selected) != 1 or selected[0] != node or type(old) is not list or retained != old):
            raise OperationError("graph_local_value_struct_current_changed", "First current role differs from original identity/returned whole node/prior nodes; no SDK follow-up")
        after = graph_api.graph_info(editor)
        final = api.member_plan(editor, variable_name, setter)
        result["data"]["member_after_graph_readback"] = copy.deepcopy(final)
        _local_value_struct_plan(final, variable_name, setter, "")
        if (not all(plan.get(key) == final.get(key) for key in _LOCAL_VALUE_STRUCT_IDENTITY)
                or final.get("native_local_value_struct_nodes") != rows):
            raise OperationError("graph_local_value_struct_current_changed", "Current identity/full physical node roles changed during public readback; no SDK follow-up")
    except Exception as error:
        failed = getattr(error, "native_graph_result", None)
        if type(failed) is dict:
            result["data"]["local_value_struct_readback_failure"] = copy.deepcopy(failed)
        raise
    observed = {row["node_id"]: row for row in after["nodes"]}.get(node["node_id"], {})
    _local_value_struct_node(final, selected[0], setter, observed)
    result["data"].update(native_value_local_whole_node_verified=True, runtime_local_value_verified=False,
        native_value_local_default_scope="Stored declaration default and stock raw/Auto/object/text pin defaults are distinct preserved observations; no default/value assignment")
    _finish_node_add(result, before, after, node["node_id"], identity,
        "K2Node_VariableSet" if setter else "K2Node_VariableGet", x, y, True)


# Referenced source admission is finite; shared current identity keys remain unchanged.
_LOCAL_REFERENCED_IDENTITY = _LOCAL_PRIMITIVE_IDENTITY


def _local_referenced_plan(plan, name, setter, target_class):
    """Copied UDE/native class identity and property binding, never a live value."""
    kinds = {"enum": ("byte", "ByteProperty"), "object": ("object", "ObjectProperty"),
        "class": ("class", "ClassProperty")}
    try:
        kind = plan["value_kind"]; category, field_class = kinds[kind]
        type_path = plan["native_type_path"]; pin = plan["pin_type"]
        source = plan["native_value_source_before"]; binding = source["property_binding"]
        good = (type(setter) is bool and not target_class and plan.get("success") is True
            and plan.get("member_scope") == "local_referenced_value" and plan.get("setter") is setter
            and plan.get("variable_name", "").casefold() == name.casefold()
            and type(type_path) is str and bool(type_path)
            and (kind == "enum" or type_path in ("/Script/CoreUObject.Object", "/Script/Engine.Actor"))
            and type(pin) is dict and pin.get("category") == category and pin.get("subcategory") == "None"
            and pin.get("type_object") == type_path and type(pin.get("container_type")) is int and pin["container_type"] == 0
            and all(type(pin.get(key)) is bool for key in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and type(pin.get("terminal")) is dict
            and all(type(pin["terminal"].get(key)) is str for key in ("category", "subcategory", "type_object"))
            and all(type(pin["terminal"].get(key)) is bool for key in ("is_const", "is_weak_pointer", "is_uobject_wrapper"))
            and ((plan.get("target_context") == "normal_actor" and plan.get("function_library_profile") == ""
                and plan.get("target_parent_class_path") == "/Script/Engine.Actor")
                or (plan.get("target_context") == "preset_loader_library" and plan.get("function_library_profile") == "preset_loader"
                    and plan.get("target_parent_class_path") == "/Script/Engine.BlueprintFunctionLibrary"
                    and plan.get("local_scope_name", "").casefold() == "presetloader"))
            and all(type(plan.get(key)) is str and bool(plan[key]) for key in (
                "graph_path", "local_scope_name", "function_entry_path", "property_path", "property_owner_function_path", "expected_pin_type_text"))
            and all(type(plan.get(key)) is str and plan[key].isascii() and plan[key].isdigit()
                for key in ("function_entry_flags_string", "property_flags_string"))
            and all(bool(uuid.UUID(plan[key]).int) for key in ("variable_guid", "graph_id", "function_entry_guid"))
            and plan.get("property_declaring_role") == "skeleton"
            and all(plan.get(key) is True for key in ("local_descriptor_current_verified", "local_property_current_verified", "stored_default_observed_unchanged", "native_value_source_observed_unchanged"))
            and all(plan.get(key) is False for key in ("runtime_value_read_attempted", "runtime_evaluation_verified", "default_write_attempted"))
            and type(plan.get("stored_default_before")) is str and plan["stored_default_before"] == plan.get("stored_default_after")
            and type(source) is dict and source == plan.get("native_value_source_after")
            and source.get("observations_complete") is True and source.get("native_identity_verified") is True
            and source.get("source_path") == type_path and source.get("scalar_property_class") == field_class
            and source.get("scalar_category") == category and source.get("scalar_subcategory") == "None"
            and all(type(source.get(key)) is int and source[key] > 0 for key in ("size", "alignment"))
            and type(binding) is dict and binding.get("property_path") == plan["property_path"]
            and binding.get("property_class") == field_class and binding.get("owner_function_path") == plan["property_owner_function_path"]
            and binding.get("source_handle_resolved") is True and binding.get("current_verified") is True
            and binding.get("enum_path") == (type_path if kind == "enum" else "")
            and binding.get("property_class_path") == (type_path if kind == "object" else "/Script/CoreUObject.Class" if kind == "class" else "")
            and binding.get("meta_class_path") == (type_path if kind == "class" else ""))
        if good and kind == "enum":
            enum = source["enum"]; entries = enum["entries"]; file = enum["file"]
            good = (source.get("source_kind") == "user_defined_enum" and type(enum) is dict
                and type(entries) is list and bool(entries)
                and type(enum.get("cpp_form")) is int and type(enum.get("underlying")) is int
                and type(enum.get("metadata")) is dict and type(enum.get("stored_fields")) is dict
                and all(type(enum["stored_fields"].get(key)) is str for key in ("UniqueNameIndex", "EnumDescription", "DisplayNameMap"))
                and type(file) is dict and type(file.get("path")) is str and bool(file["path"])
                and type(file.get("exists")) is bool and type(file.get("bytes")) is int
                and type(file.get("sha1")) is str
                and ((file["exists"] and file["bytes"] >= 0 and len(file["sha1"]) == 40
                        and all(c in "0123456789abcdefABCDEF" for c in file["sha1"]))
                    or (not file["exists"] and file["bytes"] == -1 and file["sha1"] == "")))
            if good:
                for index, row in enumerate(entries):
                    good = (type(row) is dict and type(row.get("index")) is int and row["index"] == index
                        and all(type(row.get(key)) is str for key in ("name", "full_name", "display", "value"))
                        and bool(row["name"]) and bool(row["full_name"])
                        and row["value"].isascii() and row["value"].removeprefix("-").isdigit()
                        and all(type(row.get(key)) is bool for key in ("hidden", "spacer")))
                    if not good:
                        break
        elif good:
            rows = source["native_classes"]
            good = (source.get("source_kind") == "native_class" and type(rows) is list and bool(rows)
                and all(type(row) is dict for row in rows)
                and len({row["path"] for row in rows}) == len(rows)
                and type_path in {row["path"] for row in rows})
            if good:
                for row in rows:
                    good = (all(type(row.get(key)) is str for key in ("path", "class", "outer", "super", "within", "package", "existing_cdo", "existing_cdo_class", "existing_cdo_outer"))
                        and bool(row["path"]) and type(row.get("metadata")) is dict and type(row.get("package_dirty")) is bool
                        and all(type(row.get(key)) is str and row[key].isascii() and row[key].isdigit()
                            for key in ("flags", "class_flags", "package_flags", "existing_cdo_flags")))
                    if not good:
                        break
    except (KeyError, TypeError, ValueError, AttributeError):
        good = False
    if not good:
        raise OperationError("graph_local_referenced_plan_invalid", "Exact current local UDE byte/native Object or Class source and property binding are unverified; no generic source fallback")


def _local_referenced_node(plan, node, setter, observed=None):
    """Validate actual whole stock pins without imposing declaration/Auto defaults."""
    try:
        def guid(value):
            if type(value) is not str or not uuid.UUID(value).int:
                raise ValueError("nonzero current GUID required")
            return uuid.UUID(value).hex
        expected_class = "/Script/BlueprintGraph.K2Node_VariableSet" if setter else "/Script/BlueprintGraph.K2Node_VariableGet"
        if (type(node) is not dict or node.get("setter") is not setter
                or not node.get("node_id") or node.get("node_id") != node.get("object_path")
                or node.get("node_class_path") != expected_class or node.get("graph_path") != plan["graph_path"]
                or node.get("variable_name") != plan["variable_name"] or node.get("member_name") != plan["variable_name"]
                or guid(node["variable_guid"]) != guid(plan["variable_guid"]) or guid(node["member_guid"]) != guid(plan["variable_guid"])
                or node.get("local_scope_name") != plan["local_scope_name"] or node.get("member_local_scope") != plan["local_scope_name"]
                or type(node.get("member_self_context")) is not bool or node.get("property_path") != plan["property_path"]
                or node.get("pin_type") != plan["pin_type"] or node.get("local_reference_current_verified") is not True
                or node.get("physical_pin_current_verified") is not True):
            raise ValueError("current local node/reference/property role differs")
        guid(node["node_guid"])
        pins = node["pins"]
        roles = {(plan["variable_name"], "output")} if not setter else {
            ("execute", "input"), ("then", "output"), (plan["variable_name"], "input"), ("Output_Get", "output")}
        if type(pins) is not list or len(pins) != len(roles):
            raise ValueError("whole local stock pin inventory differs")
        exec_type = {"category": "exec", "subcategory": "None", "type_object": "", "container_type": 0,
            "is_reference": False, "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False,
            "serialize_single_precision": False, "terminal": {"category": "None", "subcategory": "None",
                "type_object": "", "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False}}
        current, ids = {}, set()
        for pin in pins:
            key = pin["name"], pin["direction"]
            identity = guid(pin["pin_id"])
            expected_type = exec_type if setter and key[0] in ("execute", "then") else plan["pin_type"]
            if (key not in roles or key in current or identity in ids or pin.get("pin_type") != expected_type
                    or type(pin.get("flags")) is not int or not 0 <= pin["flags"] < 1024
                    or any(type(pin.get(key)) is not str for key in ("raw_default_value", "autogenerated_default_value", "default_object_path"))
                    or type(pin.get("default_text_empty")) is not bool or type(pin.get("links")) is not list or pin["links"] != []):
                raise ValueError("actual full scalar pin/default/link descriptor differs")
            current[key] = pin; ids.add(identity)
        value_role = plan["variable_name"], "input" if setter else "output"
        if set(current) != roles or guid(node["value_pin_id"]) != guid(current[value_role]["pin_id"]):
            raise ValueError("current local value pin identity differs")
        if setter and guid(node["output_pin_id"]) != guid(current[("Output_Get", "output")]["pin_id"]):
            raise ValueError("current Output_Get pin identity differs")
        if observed is not None:
            public = {(p["name"], p["direction"]): p for p in observed["pins"]}
            if (observed.get("node_id") != node["node_id"] or observed.get("class") != expected_class.rsplit(".", 1)[1]
                    or guid(observed["node_guid"]) != guid(node["node_guid"])
                    or len(observed["pins"]) != len(roles) or set(public) != roles
                    or any(public[key].get("links") != [] for key in roles)):
                raise ValueError("independent public node/GUID/whole-port projection differs")
        return True
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        raise OperationError("graph_local_referenced_node_unverified", "Actual referenced local node prefix retained; whole current role/pins are unverified") from error


def _add_referenced_local(result, editor, before, plan, variable_name, x, y, setter, checkpoint_callback):
    from .variables import _apply_native
    from .graph import _finish_node_add
    _local_referenced_plan(plan, variable_name, setter, "")
    identity = "add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node"
    marker = "native_referenced_local_setter" if setter else "native_referenced_local_getter"
    if plan["graph_path"] != before["graph_path"]:
        raise OperationError("graph_local_referenced_graph_changed", "Original local graph changed before dispatch")
    result["stage"] = "before_referenced_local_spawn"
    result["data"].update(local_referenced_plan=copy.deepcopy(plan), mutation_state_unknown=False)
    result["changes"].append({"kind": identity, "graph_path": before["graph_path"], "attempted": False,
        "outcome": "not_yet_dispatched_at_checkpoint"})
    (checkpoint_callback or state.checkpoint)(result)
    result["data"]["mutation_state_unknown"] = True
    result["changes"][-1].update(attempted=True, outcome="unknown_until_native_result")
    try:
        if setter:
            native = graph_api.add_referenced_local_setter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
        else:
            native = graph_api.add_referenced_local_getter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
    except OperationError as error:
        if error.code == "native_local_referenced_api_unavailable":
            result["data"].update(mutation_state_unknown=False, native_dispatched=False, native_mutation_attempted=False, mutation_performed=False)
            result["changes"][-1].update(attempted=False, changed=False, change_state_known=True,
                outcome="exact_native_api_unavailable_before_engine_dispatch")
        raise
    _apply_native(result, identity, {"blueprint_path": before.get("blueprint_path", before["graph_path"].split(":", 1)[0]), "variable_name": variable_name}, native)
    # A failed or unknown native result above exits before any later SDK call.
    result["data"]["mutation_state_unknown"] = True
    data = native["data"]; node = data.get("node", {}); preflight = data.get("local_preflight", {})
    if data.get(marker) is not True:
        raise OperationError("graph_local_referenced_result_invalid", "Exact referenced UDE/Object/Class local result marker missing; no SDK follow-up")
    _local_referenced_plan(preflight, variable_name, setter, "")
    if not all(plan.get(key) == preflight.get(key) for key in _LOCAL_REFERENCED_IDENTITY):
        raise OperationError("graph_local_referenced_preflight_changed", "Original current identity/source differs; actual prefix retained without SDK follow-up")
    _local_referenced_node(plan, node, setter)
    try:
        first = api.member_plan(editor, variable_name, setter)
        result["data"]["member_after"] = copy.deepcopy(first)
        _local_referenced_plan(first, variable_name, setter, "")
        rows, old = first.get("native_local_referenced_nodes"), plan.get("native_local_referenced_nodes")
        selected = [row for row in rows if type(row) is dict and row.get("node_id") == node.get("node_id")] if type(rows) is list else []
        retained = [row for row in rows if type(row) is dict and row.get("node_id") != node.get("node_id")] if type(rows) is list else []
        if (not all(plan.get(key) == first.get(key) for key in _LOCAL_REFERENCED_IDENTITY)
                or type(rows) is not list or any(type(row) is not dict for row in rows)
                or len(selected) != 1 or selected[0] != node or type(old) is not list or retained != old):
            raise OperationError("graph_local_referenced_current_changed", "First current role differs from original identity/returned whole node/prior nodes; no SDK follow-up")
        after = graph_api.graph_info(editor)
        final = api.member_plan(editor, variable_name, setter)
        result["data"]["member_after_graph_readback"] = copy.deepcopy(final)
        _local_referenced_plan(final, variable_name, setter, "")
        if (not all(plan.get(key) == final.get(key) for key in _LOCAL_REFERENCED_IDENTITY)
                or final.get("native_local_referenced_nodes") != rows):
            raise OperationError("graph_local_referenced_current_changed", "Current identity/full physical node roles changed during public readback; no SDK follow-up")
    except Exception as error:
        failed = getattr(error, "native_graph_result", None)
        if type(failed) is dict:
            result["data"]["local_referenced_readback_failure"] = copy.deepcopy(failed)
        raise
    observed = {row["node_id"]: row for row in after["nodes"]}.get(node["node_id"], {})
    _local_referenced_node(final, selected[0], setter, observed)
    result["data"].update(referenced_local_whole_node_verified=True, runtime_local_value_verified=False,
        referenced_local_default_scope="Stored declaration default and stock raw/Auto/object/text pin defaults are distinct preserved observations; no default/value assignment")
    _finish_node_add(result, before, after, node["node_id"], identity,
        "K2Node_VariableSet" if setter else "K2Node_VariableGet", x, y, True)


_LOCAL_PRIMITIVE_ARRAY_IDENTITY = _LOCAL_PRIMITIVE_IDENTITY


def _local_transform_source(plan, source, *, array):
    """Copied exact Transform -> Quat/Vector source; scalar and Array roles stay distinct."""
    root = "/Script/CoreUObject.Transform"
    nested = source.get("nested_structs")
    mode = (plan.get("native_value_struct_array_local_plan") is True and source.get("inner", {}).get("struct_path") == root) if array else (
        plan.get("member_scope") == "local_native_value_struct" and source.get("scalar_property_class") == "StructProperty")
    if not (mode and plan.get("native_type_path") == root
            and source.get("struct_path") == source.get("type_path") == root and source.get("type_kind") == "transform"
            and source.get("package_path") == "/Script/CoreUObject" and source.get("package_dirty") is False
            and type(source.get("package_flags_string")) is str and source["package_flags_string"].isascii() and source["package_flags_string"].isdigit()
            and type(nested) is list and len(nested) == 2
            and all(type(row) is dict for row in nested)
            and {row.get("struct_path") for row in nested} == {"/Script/CoreUObject.Quat", "/Script/CoreUObject.Vector"}
            and type(plan.get("native_value_source_after")) is dict
            and json.dumps(source, sort_keys=True, ensure_ascii=False, separators=(",", ":")) == json.dumps(plan["native_value_source_after"], sort_keys=True, ensure_ascii=False, separators=(",", ":"))):
        return False
    bindings = {"Rotation": "/Script/CoreUObject.Quat", "Translation": "/Script/CoreUObject.Vector", "Scale3D": "/Script/CoreUObject.Vector"}
    for part in [source] + nested:
        path = part.get("struct_path")
        names = set(bindings) if path == root else {"X", "Y", "Z", "W"} if path == "/Script/CoreUObject.Quat" else {"X", "Y", "Z"}
        fields = part.get("properties")
        if not (part.get("type_path") == path and type(part.get("type_flags_string")) is str
                and part["type_flags_string"].isascii() and part["type_flags_string"].isdigit()
                and all(type(part.get(key)) is int and part[key] > 0 for key in ("type_structure_size", "type_alignment"))
                and type(part.get("metadata")) is dict and all(type(k) is str and type(v) is str for k, v in part["metadata"].items())
                and type(fields) is list and len(fields) == len(names) and all(type(field) is dict for field in fields)
                and {field.get("name") for field in fields} == names and len({field.get("path") for field in fields}) == len(names)):
            return False
        for field in fields:
            if not (field.get("class") == ("StructProperty" if path == root else "DoubleProperty") and field.get("owner") == path
                    and field.get("struct_path") == (bindings[field["name"]] if path == root else "")
                    and type(field.get("path")) is str and bool(field["path"])
                    and type(field.get("offset")) is int and field["offset"] >= 0
                    and all(type(field.get(key)) is int and field[key] > 0 for key in ("size", "alignment"))
                    and type(field.get("array_dim")) is int and field["array_dim"] == 1
                    and field["offset"] + field["size"] <= part["type_structure_size"]
                    and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit()):
                return False
    return True


def _local_transform_array_source(plan, source):
    """Retain the existing Array marker and current Inner binding requirements."""
    return _local_transform_source(plan, source, array=True)


def _local_value_struct_array_source(plan, source):
    """Copied exact native struct binding/layout; never inspect a runtime array buffer."""
    if plan.get("value_kind") == "transform":
        return _local_transform_array_source(plan, source)
    type_path, names = {"vector": ("/Script/CoreUObject.Vector", {"X", "Y", "Z"}),
        "rotator": ("/Script/CoreUObject.Rotator", {"Pitch", "Yaw", "Roll"})}[plan["value_kind"]]
    fields = source["properties"]
    good = (plan.get("native_value_struct_array_local_plan") is True
        and plan.get("native_type_path") == type_path
        and source.get("struct_path") == source.get("type_path") == type_path
        and source.get("type_kind") == plan["value_kind"]
        and source.get("package_path") == "/Script/CoreUObject" and source.get("package_dirty") is False
        and type(source.get("metadata")) is dict
        and all(type(source.get(key)) is str and source[key].isascii() and source[key].isdigit()
            for key in ("package_flags_string", "type_flags_string"))
        and all(type(source.get(key)) is int and source[key] > 0 for key in ("type_structure_size", "type_alignment"))
        and source["inner"].get("struct_path") == type_path
        and type(fields) is list and len(fields) == 3 and all(type(field) is dict for field in fields)
        and {field["name"] for field in fields} == names and len({field["path"] for field in fields}) == 3)
    if good:
        for field in fields:
            good = (field.get("class") == "DoubleProperty" and field.get("owner") == type_path
                and type(field.get("path")) is str and bool(field["path"])
                and type(field.get("offset")) is int and field["offset"] >= 0
                and all(type(field.get(key)) is int and field[key] > 0 for key in ("size", "alignment"))
                and type(field.get("array_dim")) is int and field["array_dim"] == 1
                and field["offset"] + field["size"] <= source["type_structure_size"]
                and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit())
            if not good:
                break
    return good


def _local_referenced_array_source(plan, source):
    """Copied actual Array Inner referenced binding/source; no live values or resolution."""
    kinds = {"enum": ("byte", "ByteProperty"), "object": ("object", "ObjectProperty"),
        "class": ("class", "ClassProperty")}
    try:
        kind = plan["value_kind"]; category, field_class = kinds[kind]
        type_path = plan["native_type_path"]; inner = source["inner"]
        array_source = source
        source = array_source["referenced_source"]
        binding = source["property_binding"]
        good = (plan.get("native_referenced_array_local_plan") is True
            and type(type_path) is str and bool(type_path)
            and (kind == "enum" or type_path in ("/Script/CoreUObject.Object", "/Script/Engine.Actor"))
            and json.dumps(array_source, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
                == json.dumps(plan.get("native_value_source_after"), sort_keys=True, ensure_ascii=False, separators=(",", ":"))
            and type(source) is dict and source.get("observations_complete") is True and source.get("native_identity_verified") is True
            and source.get("source_path") == type_path and source.get("scalar_property_class") == field_class
            and source.get("scalar_category") == category and source.get("scalar_subcategory") == "None"
            and all(type(source.get(key)) is int and source[key] > 0 for key in ("size", "alignment"))
            and type(binding) is dict and binding.get("property_path") == inner["property_path"]
            and binding.get("property_class") == field_class and binding.get("owner_function_path") == plan["property_owner_function_path"]
            and binding.get("owner_property_path") == plan["property_path"]
            and binding.get("source_handle_resolved") is True and binding.get("current_verified") is True
            and inner.get("source_handle_resolved") is True
            and inner.get("enum_path") == binding.get("enum_path") == (type_path if kind == "enum" else "")
            and inner.get("property_class_path") == binding.get("property_class_path") == (type_path if kind == "object" else "/Script/CoreUObject.Class" if kind == "class" else "")
            and inner.get("meta_class_path") == binding.get("meta_class_path") == (type_path if kind == "class" else ""))
        if good and kind == "enum":
            enum = source["enum"]; entries = enum["entries"]; file = enum["file"]
            good = (source.get("source_kind") == "user_defined_enum" and type(enum) is dict
                and type(entries) is list and bool(entries)
                and type(enum.get("cpp_form")) is int and type(enum.get("underlying")) is int
                and type(enum.get("metadata")) is dict and type(enum.get("stored_fields")) is dict
                and all(type(enum["stored_fields"].get(key)) is str for key in ("UniqueNameIndex", "EnumDescription", "DisplayNameMap"))
                and type(file) is dict and type(file.get("path")) is str and bool(file["path"])
                and type(file.get("exists")) is bool and type(file.get("bytes")) is int
                and type(file.get("sha1")) is str
                and ((file["exists"] and file["bytes"] >= 0 and len(file["sha1"]) == 40
                        and all(c in "0123456789abcdefABCDEF" for c in file["sha1"]))
                    or (not file["exists"] and file["bytes"] == -1 and file["sha1"] == "")))
            if good:
                for index, row in enumerate(entries):
                    good = (type(row) is dict and type(row.get("index")) is int and row["index"] == index
                        and all(type(row.get(key)) is str for key in ("name", "full_name", "display", "value"))
                        and bool(row["name"]) and bool(row["full_name"])
                        and row["value"].isascii() and row["value"].removeprefix("-").isdigit()
                        and all(type(row.get(key)) is bool for key in ("hidden", "spacer")))
                    if not good:
                        break
        elif good:
            rows = source["native_classes"]
            good = (source.get("source_kind") == "native_class" and type(rows) is list and bool(rows)
                and all(type(row) is dict for row in rows)
                and len({row["path"] for row in rows}) == len(rows)
                and type_path in {row["path"] for row in rows})
            if good:
                for row in rows:
                    good = (all(type(row.get(key)) is str for key in ("path", "class", "outer", "super", "within", "package", "existing_cdo", "existing_cdo_class", "existing_cdo_outer"))
                        and bool(row["path"]) and type(row.get("metadata")) is dict and type(row.get("package_dirty")) is bool
                        and all(type(row.get(key)) is str and row[key].isascii() and row[key].isdigit()
                            for key in ("flags", "class_flags", "package_flags", "existing_cdo_flags")))
                    if not good:
                        break
    except (KeyError, TypeError, ValueError, AttributeError):
        good = False
    return good


def _local_scalar_uds_array_source(plan, source):
    """Copied finite UDS definition with actual Array.Inner binding, without values."""
    try:
        path=plan["native_type_path"]; uds=source["scalar_uds_source"]; inner=source["inner"]
        good=(plan.get("native_scalar_uds_array_local_plan") is True and plan.get("value_kind")=="scalar_uds" and source.get("type_kind")=="scalar_uds"
            and type(path) is str and path.startswith("/Game/") and "." in path
            and json.dumps(source,sort_keys=True,ensure_ascii=False,separators=(",",":"))
                ==json.dumps(plan["native_value_source_after"],sort_keys=True,ensure_ascii=False,separators=(",",":"))
            and inner.get("struct_path")==path and inner.get("source_handle_resolved") is True and inner.get("current_verified") is True
            and type(uds) is dict and uds.get("observations_complete") is True and uds.get("native_identity_verified") is True
            and uds.get("struct_path")==path and bool(uuid.UUID(uds["struct_guid"]).int)
            and uds.get("package_dirty") is False and type(uds.get("status")) is int
            and all(uds.get(key) is False for key in ("values_sampled","default_buffer_sampled","cdo_creation_requested","runtime_invocation_verified"))
            and all(type(uds.get(key)) is int and uds[key]>=0 for key in ("struct_size","struct_alignment"))
            and all(type(uds.get(key)) is str and bool(uds[key]) for key in ("package_path","editor_data_path"))
            and all(type(uds.get(key)) is str and uds[key].isascii() and uds[key].isdigit() for key in ("struct_flags_string","package_flags_string"))
            and all(type(uds.get(key)) is dict and all(type(a) is str and type(b) is str for a,b in uds[key].items())
                for key in ("root_metadata","editor_data_metadata")))
        fields=uds.get("fields")
        good=good and type(fields) is list and 1<=len(fields)<=32 and all(type(field) is dict for field in fields)
        scalar={"StrProperty":("string","None"),"IntProperty":("int","None"),"Int64Property":("int64","None"),
            "FloatProperty":("real","float"),"DoubleProperty":("real","double"),"BoolProperty":("bool","None"),"NameProperty":("name","None")}
        names,guids=set(),set()
        if good:
            for field in fields:
                identity=uuid.UUID(field["field_guid"])
                good=(bool(identity.int) and identity.hex not in guids and type(field.get("internal_name")) is str
                    and bool(field["internal_name"]) and field["internal_name"] not in names and type(field.get("authored_name")) is str
                    and field.get("owner_path")==path and field.get("class") in scalar
                    and (field.get("category"),field.get("subcategory"))==scalar[field["class"]]
                    and type(field.get("flags")) is str and field["flags"].isascii() and field["flags"].isdigit()
                    and all(type(field.get(key)) is int and field[key]>=0 for key in ("offset","size"))
                    and type(field.get("array_dim")) is int and field["array_dim"]==1
                    and type(field.get("metadata")) is dict and all(type(a) is str and type(b) is str for a,b in field["metadata"].items()))
                if not good:break
                names.add(field["internal_name"]);guids.add(identity.hex)
        file=uds.get("file",{})
        return (good and type(file) is dict and file.get("exists") is True
            and type(file.get("filename")) is str and bool(file["filename"])
            and type(file.get("bytes")) is int and file["bytes"]>=0 and type(file.get("sha1")) is str and len(file["sha1"])==40
            and all(c in "0123456789abcdefABCDEF" for c in file["sha1"]))
    except (KeyError,TypeError,ValueError,AttributeError):
        return False


def _local_scalar_uds_array_same_source(first, second):
    if first.get("value_kind")!="scalar_uds":
        return True
    return all(json.dumps(first.get(key),sort_keys=True,ensure_ascii=False,separators=(",",":"))
        ==json.dumps(second.get(key),sort_keys=True,ensure_ascii=False,separators=(",",":"))
        for key in ("native_value_source_before","native_value_source_after"))


def _local_primitive_array_plan(plan, name, setter, target_class):
    """Finite copied primitive or exact Vector/Rotator Array local plan; declaration default is not a pin default."""
    kinds = {"bool": ("bool", "None", "BoolProperty"), "int": ("int", "None", "IntProperty"),
        "int64": ("int64", "None", "Int64Property"), "float": ("real", "float", "FloatProperty"),
        "double": ("real", "double", "DoubleProperty"), "string": ("string", "None", "StrProperty"),
        "name": ("name", "None", "NameProperty"), "text": ("text", "None", "TextProperty"),
        "vector": ("struct", "None", "StructProperty"), "rotator": ("struct", "None", "StructProperty"),
        "transform": ("struct", "None", "StructProperty"), "scalar_uds": ("struct", "None", "StructProperty"),
        "enum": ("byte", "None", "ByteProperty"),
        "object": ("object", "None", "ObjectProperty"), "class": ("class", "None", "ClassProperty")}
    terminal = {"category": "None", "subcategory": "None", "type_object": "", "is_const": False,
        "is_weak_pointer": False, "is_uobject_wrapper": False}
    try:
        category, subcategory, property_class = kinds[plan["value_kind"]]
        native_struct_array = plan["value_kind"] in ("vector", "rotator", "transform")
        native_referenced_array = plan["value_kind"] in ("enum", "object", "class")
        type_path = plan.get("native_type_path") if native_referenced_array or plan["value_kind"] == "scalar_uds" else {"vector": "/Script/CoreUObject.Vector", "rotator": "/Script/CoreUObject.Rotator", "transform": "/Script/CoreUObject.Transform"}[plan["value_kind"]] if native_struct_array else ""
        pin_type = plan["pin_type"]
        expected = {"category": category, "subcategory": subcategory, "type_object": type_path, "container_type": 1,
            "is_reference": False, "is_const": False, "is_weak_pointer": False, "is_uobject_wrapper": False,
            "serialize_single_precision": False, "terminal": terminal}
        if native_referenced_array:
            expected.update({key: pin_type.get(key) for key in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision")})
        source = plan["native_value_source_before"]
        inner = source["inner"]
        element = dict(expected, container_type=0)
        good = (type(setter) is bool and not target_class and plan.get("success") is True
            and plan.get("member_scope") == "local_primitive_array" and plan.get("setter") is setter
            and plan.get("variable_name", "").casefold() == name.casefold() and pin_type == expected
            and type(pin_type.get("container_type")) is int
            and all(type(pin_type.get(key)) is bool for key in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and all(type(pin_type["terminal"].get(key)) is bool for key in ("is_const", "is_weak_pointer", "is_uobject_wrapper"))
            and ((plan.get("target_context") == "normal_actor" and plan.get("function_library_profile") == ""
                and plan.get("target_parent_class_path") == "/Script/Engine.Actor")
                or (plan.get("target_context") == "preset_loader_library" and plan.get("function_library_profile") == "preset_loader"
                    and plan.get("target_parent_class_path") == "/Script/Engine.BlueprintFunctionLibrary"
                    and plan.get("local_scope_name", "").casefold() == "presetloader"))
            and all(type(plan.get(key)) is str and bool(plan[key]) for key in (
                "graph_path", "local_scope_name", "function_entry_path", "property_path", "property_owner_function_path", "expected_pin_type_text"))
            and all(type(plan.get(key)) is str and plan[key].isascii() and plan[key].isdigit()
                for key in ("function_entry_flags_string", "property_flags_string"))
            and all(bool(uuid.UUID(plan[key]).int) for key in ("variable_guid", "graph_id", "function_entry_guid"))
            and plan.get("property_declaring_role") == "skeleton" and plan.get("native_type_path") == type_path
            and all(plan.get(key) is True for key in ("local_descriptor_current_verified", "local_property_current_verified", "stored_default_observed_unchanged", "native_value_source_observed_unchanged"))
            and all(plan.get(key) is False for key in ("runtime_value_read_attempted", "runtime_evaluation_verified", "default_write_attempted"))
            and type(plan.get("stored_default_before")) is str and plan["stored_default_before"] == plan.get("stored_default_after")
            and type(source) is dict and source == plan.get("native_value_source_after")
            and source.get("observations_complete") is True and source.get("native_identity_verified") is True
            and source.get("scalar_property_class") == property_class
            and source.get("scalar_category") == category and source.get("scalar_subcategory") == subcategory
            and type(source.get("size")) is int and source["size"] > 0
            and type(source.get("alignment")) is int and source["alignment"] > 0
            and source.get("array_property_class") == "ArrayProperty"
            and source.get("array_property_path") == plan["property_path"]
            and source.get("array_owner_function_path") == plan["property_owner_function_path"]
            and type(source.get("array_flags_string")) is str and source["array_flags_string"].isascii() and source["array_flags_string"].isdigit()
            and all(type(source.get(key)) is int and source[key] > 0 for key in ("array_size", "array_alignment"))
            and source.get("element_type") == element and type(source["element_type"].get("container_type")) is int
            and all(type(source["element_type"].get(key)) is bool for key in ("is_reference", "is_const", "is_weak_pointer", "is_uobject_wrapper", "serialize_single_precision"))
            and type(inner) is dict and type(inner.get("property_path")) is str and bool(inner["property_path"])
            and inner.get("property_class") == property_class and inner.get("owner_property_path") == plan["property_path"]
            and inner.get("owner_function_path") == plan["property_owner_function_path"]
            and type(inner.get("flags_string")) is str and inner["flags_string"].isascii() and inner["flags_string"].isdigit()
            and type(inner.get("array_dim")) is int and inner["array_dim"] == 1
            and type(inner.get("offset")) is int and inner["offset"] >= 0
            and type(inner.get("size")) is int and inner["size"] == source["size"]
            and type(inner.get("alignment")) is int and inner["alignment"] == source["alignment"]
            and inner.get("current_verified") is True)
        if good and native_struct_array:
            good = _local_value_struct_array_source(plan, source)
        if good and native_referenced_array:
            good = _local_referenced_array_source(plan, source)
        if good and plan["value_kind"] == "scalar_uds":
            good = _local_scalar_uds_array_source(plan, source)
    except (KeyError, TypeError, ValueError, AttributeError):
        good = False
    if not good:
        raise OperationError("graph_local_primitive_array_plan_invalid", "Exact current own function-local GUID/primitive Array property/Inner/source plan is unverified; no fallback or value access")


def _local_primitive_array_node(plan, node, setter, observed=None):
    """Validate actual whole stock pins without imposing declaration/Auto defaults."""
    try:
        def guid(value):
            if type(value) is not str or not uuid.UUID(value).int:
                raise ValueError("nonzero current GUID required")
            return uuid.UUID(value).hex
        expected_class = "/Script/BlueprintGraph.K2Node_VariableSet" if setter else "/Script/BlueprintGraph.K2Node_VariableGet"
        if (type(node) is not dict or node.get("setter") is not setter
                or not node.get("node_id") or node.get("node_id") != node.get("object_path")
                or node.get("node_class_path") != expected_class or node.get("graph_path") != plan["graph_path"]
                or node.get("variable_name") != plan["variable_name"] or node.get("member_name") != plan["variable_name"]
                or guid(node["variable_guid"]) != guid(plan["variable_guid"]) or guid(node["member_guid"]) != guid(plan["variable_guid"])
                or node.get("local_scope_name") != plan["local_scope_name"] or node.get("member_local_scope") != plan["local_scope_name"]
                or type(node.get("member_self_context")) is not bool or node.get("property_path") != plan["property_path"]
                or node.get("pin_type") != plan["pin_type"] or node.get("local_reference_current_verified") is not True
                or node.get("physical_pin_current_verified") is not True):
            raise ValueError("current local node/reference/property role differs")
        guid(node["node_guid"])
        pins = node["pins"]
        roles = {(plan["variable_name"], "output")} if not setter else {
            ("execute", "input"), ("then", "output"), (plan["variable_name"], "input"), ("Output_Get", "output")}
        if type(pins) is not list or len(pins) != len(roles):
            raise ValueError("whole local stock pin inventory differs")
        exec_type = dict(plan["pin_type"], category="exec", subcategory="None", type_object="", container_type=0)
        if plan.get("value_kind") in ("enum", "object", "class"):
            exec_type.update(is_reference=False, is_const=False, is_weak_pointer=False,
                is_uobject_wrapper=False, serialize_single_precision=False)
        current, ids = {}, set()
        for pin in pins:
            key = pin["name"], pin["direction"]
            identity = guid(pin["pin_id"])
            expected_type = exec_type if setter and key[0] in ("execute", "then") else plan["pin_type"]
            if (key not in roles or key in current or identity in ids or pin.get("pin_type") != expected_type
                    or type(pin.get("flags")) is not int or not 0 <= pin["flags"] < 1024
                    or any(type(pin.get(key)) is not str for key in ("raw_default_value", "autogenerated_default_value", "default_object_path"))
                    or type(pin.get("default_text_empty")) is not bool or type(pin.get("links")) is not list or pin["links"] != []):
                raise ValueError("actual full scalar pin/default/link descriptor differs")
            current[key] = pin; ids.add(identity)
        value_role = plan["variable_name"], "input" if setter else "output"
        if set(current) != roles or guid(node["value_pin_id"]) != guid(current[value_role]["pin_id"]):
            raise ValueError("current local value pin identity differs")
        if setter and guid(node["output_pin_id"]) != guid(current[("Output_Get", "output")]["pin_id"]):
            raise ValueError("current Output_Get pin identity differs")
        if observed is not None:
            public = {(p["name"], p["direction"]): p for p in observed["pins"]}
            if (observed.get("node_id") != node["node_id"] or observed.get("class") != expected_class.rsplit(".", 1)[1]
                    or guid(observed["node_guid"]) != guid(node["node_guid"])
                    or len(observed["pins"]) != len(roles) or set(public) != roles
                    or any(public[key].get("links") != [] for key in roles)):
                raise ValueError("independent public node/GUID/whole-port projection differs")
        return True
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        raise OperationError("graph_local_primitive_array_node_unverified", "Actual primitive Array local node prefix retained; whole current role/pins are unverified") from error


def _add_primitive_array_local(result, editor, before, plan, variable_name, x, y, setter, checkpoint_callback):
    from .variables import _apply_native
    from .graph import _finish_node_add
    _local_primitive_array_plan(plan, variable_name, setter, "")
    identity = "add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node"
    marker = "native_primitive_array_local_setter" if setter else "native_primitive_array_local_getter"
    if plan["graph_path"] != before["graph_path"]:
        raise OperationError("graph_local_primitive_array_graph_changed", "Original local graph changed before dispatch")
    result["stage"] = "before_primitive_array_local_spawn"
    result["data"].update(local_primitive_array_plan=copy.deepcopy(plan), mutation_state_unknown=False)
    result["changes"].append({"kind": identity, "graph_path": before["graph_path"], "attempted": False,
        "outcome": "not_yet_dispatched_at_checkpoint"})
    (checkpoint_callback or state.checkpoint)(result)
    result["data"]["mutation_state_unknown"] = True
    result["changes"][-1].update(attempted=True, outcome="unknown_until_native_result")
    try:
        if setter:
            native = graph_api.add_primitive_array_local_setter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
        else:
            native = graph_api.add_primitive_array_local_getter(before["graph_path"], variable_name, plan["variable_guid"], x, y)
    except OperationError as error:
        if error.code == "native_local_primitive_array_api_unavailable":
            result["data"].update(mutation_state_unknown=False, native_dispatched=False, native_mutation_attempted=False, mutation_performed=False)
            result["changes"][-1].update(attempted=False, changed=False, change_state_known=True,
                outcome="exact_native_api_unavailable_before_engine_dispatch")
        raise
    _apply_native(result, identity, {"blueprint_path": before.get("blueprint_path", before["graph_path"].split(":", 1)[0]), "variable_name": variable_name}, native)
    # A failed or unknown native result above exits before any later SDK call.
    result["data"]["mutation_state_unknown"] = True
    data = native["data"]; node = data.get("node", {}); preflight = data.get("local_preflight", {})
    if data.get(marker) is not True:
        raise OperationError("graph_local_primitive_array_result_invalid", "Exact primitive Array local result marker missing; no SDK follow-up")
    if plan.get("value_kind") in ("vector", "rotator", "transform") and data.get("native_value_struct_array_local_setter" if setter else "native_value_struct_array_local_getter") is not True:
        raise OperationError("graph_local_value_struct_array_result_invalid", "Exact native Vector/Rotator/Transform Array marker missing; actual prefix retained without SDK follow-up")
    if plan.get("value_kind") in ("enum", "object", "class") and data.get("native_referenced_array_local_setter" if setter else "native_referenced_array_local_getter") is not True:
        raise OperationError("graph_local_referenced_array_result_invalid", "Exact current Enum/native Object/Class Array marker missing; actual prefix retained without SDK follow-up")
    if plan.get("value_kind") == "scalar_uds" and data.get("native_scalar_uds_array_local_setter" if setter else "native_scalar_uds_array_local_getter") is not True:
        raise OperationError("graph_local_scalar_uds_array_result_invalid", "Current UDS Array result marker missing; actual prefix retained without SDK follow-up")
    _local_primitive_array_plan(preflight, variable_name, setter, "")
    if (not all(plan.get(key) == preflight.get(key) for key in _LOCAL_PRIMITIVE_ARRAY_IDENTITY)
            or not _local_scalar_uds_array_same_source(plan, preflight)):
        raise OperationError("graph_local_primitive_array_preflight_changed", "Original current identity/source differs; actual prefix retained without SDK follow-up")
    _local_primitive_array_node(plan, node, setter)
    try:
        first = api.member_plan(editor, variable_name, setter)
        result["data"]["member_after"] = copy.deepcopy(first)
        _local_primitive_array_plan(first, variable_name, setter, "")
        rows, old = first.get("native_local_primitive_array_nodes"), plan.get("native_local_primitive_array_nodes")
        selected = [row for row in rows if type(row) is dict and row.get("node_id") == node.get("node_id")] if type(rows) is list else []
        retained = [row for row in rows if type(row) is dict and row.get("node_id") != node.get("node_id")] if type(rows) is list else []
        if (not all(plan.get(key) == first.get(key) for key in _LOCAL_PRIMITIVE_ARRAY_IDENTITY)
                or not _local_scalar_uds_array_same_source(plan, first)
                or type(rows) is not list or any(type(row) is not dict for row in rows)
                or len(selected) != 1 or selected[0] != node or type(old) is not list or retained != old):
            raise OperationError("graph_local_primitive_array_current_changed", "First current role differs from original identity/returned whole node/prior nodes; no SDK follow-up")
        after = graph_api.graph_info(editor)
        final = api.member_plan(editor, variable_name, setter)
        result["data"]["member_after_graph_readback"] = copy.deepcopy(final)
        _local_primitive_array_plan(final, variable_name, setter, "")
        if (not all(plan.get(key) == final.get(key) for key in _LOCAL_PRIMITIVE_ARRAY_IDENTITY)
                or not _local_scalar_uds_array_same_source(plan, final)
                or final.get("native_local_primitive_array_nodes") != rows):
            raise OperationError("graph_local_primitive_array_current_changed", "Current identity/full physical node roles changed during public readback; no SDK follow-up")
    except Exception as error:
        failed = getattr(error, "native_graph_result", None)
        if type(failed) is dict:
            result["data"]["local_primitive_array_readback_failure"] = copy.deepcopy(failed)
        raise
    observed = {row["node_id"]: row for row in after["nodes"]}.get(node["node_id"], {})
    _local_primitive_array_node(final, selected[0], setter, observed)
    result["data"].update(primitive_array_local_whole_node_verified=True, runtime_local_value_verified=False,
        primitive_array_local_default_scope="Stored declaration default and stock raw/Auto/object/text pin defaults are distinct preserved observations; no default/value assignment")
    _finish_node_add(result, before, after, node["node_id"], identity,
        "K2Node_VariableSet" if setter else "K2Node_VariableGet", x, y, True)


_EXTERNAL_ACTOR_IDENTITY = ("member_scope", "external_actor_member", "variable_name", "setter",
    "graph_path", "target_class_path", "declaring_class_path", "declaring_blueprint_path", "variable_guid",
    "property_path", "compiled_property_path", "pin_category", "pin_subcategory", "expected_pin_type_text",
    "expected_self_pin_type_text", "external_actor_source_snapshot", "inherited_actor_member")


def _external_actor_plan(plan, variable_name, setter, target_class):
    snapshot = plan.get("external_actor_source_snapshot")
    try:
        guid_valid = bool(uuid.UUID(plan.get("variable_guid", "")).int)
    except (TypeError, ValueError, AttributeError):
        guid_valid = False
    inherited = plan.get("member_scope") == "inherited_generated_actor"
    scope_valid = ((not inherited and plan.get("member_scope") == "external_generated_actor"
                    and plan.get("external_actor_member") is True
                    and plan.get("target_class_path") == target_class
                    and plan.get("declaring_class_path") == target_class)
                   or (inherited and not target_class and plan.get("external_actor_member") is False
                       and plan.get("inherited_actor_member") is True
                       and plan.get("inherited_declaring_property_verified") is True
                       and type(plan.get("target_class_path")) is str and bool(plan["target_class_path"])
                       and type(plan.get("declaring_class_path")) is str and bool(plan["declaring_class_path"])
                       and plan["target_class_path"] != plan["declaring_class_path"]))
    if (not scope_valid
            or plan.get("variable_name") != variable_name or plan.get("setter") is not setter
            or not guid_valid or type(snapshot) is not dict or not snapshot
            or any(type(plan.get(key)) is not str or not plan[key] for key in
                ("graph_path", "declaring_blueprint_path", "property_path", "compiled_property_path",
                 "expected_pin_type_text", "expected_self_pin_type_text"))
            or plan.get("dirty_before") != plan.get("dirty_after")):
        raise OperationError("graph_external_actor_plan_invalid", "Exact current Actor source/property/GUID/value-self type and copied source snapshot required")


def _add_action(result, blueprint_name, variable_name, graph_name, x, y, setter, checkpoint_callback=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    if type(variable_name) is not str or not variable_name or any(ord(c) < 32 or ord(c) == 127 for c in variable_name):
        raise OperationError("variable_name_invalid", "An exact nonempty member name is required")
    if any(type(v) is not int or abs(v) > 1000000 for v in (x, y)):
        raise OperationError("node_position_invalid", "Node coordinates must be integers between -1000000 and 1000000")
    options = _options(target_class, graph_id, graph_type, create_graph_if_missing, node_position_json, x, y)
    target_class = options.get("target_class", "")
    native_texture = api.native_texture_setter_requested(target_class, variable_name, setter)
    native_member = api.native_member_getter_requested(target_class, variable_name, setter)
    native_child_actor = native_member and target_class == api.NATIVE_CHILD_ACTOR_COMPONENT
    if "node_position_json" in options:
        from .functions import _json
        x, y = _json(options["node_position_json"], "node_position_json")
    ensure_editor_idle()
    if options.get("graph_id") or options.get("graph_type"):
        from .functions import _selectors
        selectors = _selectors(blueprint_name, graph_name, graph_id, graph_type)
        editor, selection = graph_api.sequence_editor(**selectors)
        result["data"]["graph_selection"] = selection
    else:
        editor = graph_api.graph_editor(blueprint_name, graph_name)
    before = graph_api.graph_info(editor)
    result["data"].update(before=copy.deepcopy(before),
        full_legacy_member_scope_parity=False, mutation_state_unknown=False)
    if before["total_node_count"] >= 2048:
        raise OperationError("graph_node_limit_reached", "Member node would exceed the bounded graph observation limit")
    source_baseline = None
    external_class = None  # Retain the loaded class through spawn and readback.
    if native_texture:
        result["data"].update(external_class_load_attempted=False, native_texture_runtime_value_written=False)
    if target_class and not native_texture:
        result["stage"] = "external_class_preflight"
        source_before = api.external_source_state(target_class)
        result["data"]["external_source_before_load"] = source_before
        if native_member:
            # Native identity uses fixed CAC type metadata or the existing Pawn lookup.
            # Keep saved Game Blueprint loading/file sampling on its existing path.
            source_baseline = source_before
            result["data"].update(external_class_load_attempted=False,
                native_class_observation_scope=("Exact already-loaded native CAC UClass/readonly property; no private actor value, Blueprint file/CDO sampling or explicit class load"
                    if native_child_actor else "Exact already-loaded native Pawn UClass; no Blueprint file/CDO sampling or explicit class load"))
        elif (not source_before["file"]["exists"] or not source_before["file"]["sha256"] or source_before["dirty"]):
            raise OperationError("graph_external_source_not_saved_clean", "External class requires a clean existing saved Game Blueprint")
        if not native_member:
            result["data"]["pending_external_load"] = {"class_path": target_class,
                "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_source_and_target"}
            if not source_before["loaded"]:
                # A cold class load can run asset callbacks. Preserve this earlier
                # boundary even though the actual member spawn has its own intent.
                (checkpoint_callback or state.checkpoint)(result)
            result["data"]["external_class_load_attempted"] = True
            result["data"]["mutation_state_unknown"] = True
            try:
                external_class = api.load_external_class(target_class)
            finally:
                source_baseline = api.external_source_state(target_class)
                result["data"]["external_source_after_load"] = source_baseline
                after_load = graph_api.graph_info(editor)
                result["data"]["after_external_load"] = after_load
                if not _source_preserved(source_before, source_baseline, allow_load=True) or after_load != before:
                    result["data"]["mutation_state_unknown"] = True
                    result["changes"].append({"kind": "external_class_load", "attempted": True,
                        "outcome": "source_or_target_changed_during_load", "graph_changed": after_load != before})
                    raise OperationError("graph_external_load_changed_state", "Loading changed source bytes/state or target graph; no node spawn dispatched")
                result["data"]["mutation_state_unknown"] = False
    try:
        plan = api.member_plan(editor, variable_name, setter, target_class) if target_class else api.member_plan(editor, variable_name, setter)
    except OperationError as error:
        native_failure = getattr(error, "native_graph_result", None)
        if type(native_failure) is dict and ((not target_class and native_failure.get("member_scope") in ("own_native_object_array", "own_native_material_value")) or native_failure.get("member_scope") in ("own_native_vector4_array", "external_native_vector_array", "own_native_string_vector_map", "own_native_string_double_map", "local_native_string_double_map", "local_primitive", "local_native_value_struct", "local_referenced_value", "local_primitive_array", "external_native_texture_enum_setter")):
            result["data"]["member_preflight_failure"] = copy.deepcopy(native_failure)
            if native_failure.get("member_scope") in ("own_native_material_value", "own_native_vector4_array", "external_native_vector_array", "own_native_string_vector_map", "own_native_string_double_map", "local_native_string_double_map", "local_primitive", "local_native_value_struct", "local_referenced_value", "local_primitive_array", "external_native_texture_enum_setter"):
                result["data"]["mutation_state_unknown"] = True
        if type(native_failure) is dict and native_failure.get("member_scope") in ("external_generated_actor", "inherited_generated_actor"):
            result["data"].update(member_preflight_failure=copy.deepcopy(native_failure),
                mutation_state_unknown=native_failure.get("change_state_known") is not True or native_failure.get("observation_incomplete") is True)
        raise
    result["data"]["member_preflight"] = plan
    if plan.get("member_scope") == "local_primitive_array":
        _local_primitive_array_plan(plan, variable_name, setter, target_class)
        return _add_primitive_array_local(result, editor, before, plan, variable_name, x, y, setter, checkpoint_callback)
    if plan.get("member_scope") == "local_referenced_value":
        _local_referenced_plan(plan, variable_name, setter, target_class)
        return _add_referenced_local(result, editor, before, plan, variable_name, x, y, setter, checkpoint_callback)
    if plan.get("member_scope") == "local_native_value_struct":
        _local_value_struct_plan(plan, variable_name, setter, target_class)
        return _add_native_value_local(result, editor, before, plan, variable_name, x, y, setter, checkpoint_callback)
    if plan.get("member_scope") == "local_primitive":
        _local_primitive_plan(plan, variable_name, setter, target_class)
        return _add_primitive_local(result, editor, before, plan, variable_name, x, y, setter, checkpoint_callback)
    if plan.get("member_scope") == "local_native_string_double_map":
        _local_map_plan(plan, variable_name, setter, target_class)
        if setter:
            return _add_local_map_setter(result, editor, before, plan, variable_name, x, y, checkpoint_callback)
        return _add_local_map(result, editor, before, plan, variable_name, x, y, checkpoint_callback)
    inherited_actor = plan.get("member_scope") == "inherited_generated_actor"
    external_actor = inherited_actor or plan.get("member_scope") == "external_generated_actor"
    if external_actor:
        try:
            _external_actor_plan(plan, variable_name, setter, target_class)
            if plan["graph_path"] != before["graph_path"]:
                raise OperationError("graph_external_actor_graph_changed", "Actor plan selected another graph; no spawn")
        except Exception:
            result["data"]["mutation_state_unknown"] = True
            raise
    own_object_array = plan.get("member_scope") == "own_native_object_array"
    own_material_value = plan.get("member_scope") == "own_native_material_value"
    native_vector_array = plan.get("member_scope") in ("own_native_vector4_array", "external_native_vector_array", "own_native_string_vector_map", "own_native_string_double_map")
    native_string_vector_map = plan.get("member_scope") in ("own_native_string_vector_map", "own_native_string_double_map")
    if native_string_vector_map:
        result["data"].update({"native_string_double_map_member" if plan.get("member_scope") == "own_native_string_double_map" else "native_string_vector_map_member": True})
        result["data"].update(runtime_map_assignment_verified=False)
    if native_vector_array:
        try:
            _native_vector_array_plan(plan, variable_name, setter, target_class)
            if plan.get("graph_path") != before["graph_path"]:
                raise OperationError("graph_native_vector_array_graph_changed", "Current array plan no longer selects the original graph; no spawn was dispatched")
        except OperationError:
            result["data"]["mutation_state_unknown"] = True
            raise
    if own_material_value:
        try:
            _own_material_value_plan(plan, variable_name, setter, target_class)
            if plan.get("graph_path") != before["graph_path"]:
                raise OperationError("graph_native_material_value_graph_changed", "The native own-value plan no longer selects the original graph; no spawn was dispatched")
        except OperationError:
            result["data"]["mutation_state_unknown"] = True
            raise
    if own_object_array:
        _own_object_array_plan(plan, variable_name, setter, target_class)
    if plan.get("member_scope") in _NATIVE_LOCAL_SCS_SIGNATURES:
        _native_local_scs_plan(plan, variable_name, setter, target_class)
    elif plan.get("member_scope") == "own_native_class":
        _native_class_plan(plan, variable_name, setter, target_class)
    if target_class and not native_texture:
        source_preflight = api.external_source_state(target_class)
        result["data"]["external_source_before_spawn"] = source_preflight
        if not _source_preserved(source_baseline, source_preflight):
            result["data"]["mutation_state_unknown"] = True
            raise OperationError("graph_external_source_changed", "External source changed during member preflight; no node spawn dispatched")
    kind = "add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node"
    result["stage"] = "before_member_node_spawn"
    result["changes"].append({"kind": kind, "graph_path": before["graph_path"], "variable_name": plan["variable_name"],
        "attempted": False, "outcome": "not_yet_dispatched_at_checkpoint"})
    result["data"]["interrupted_outcome"] = "unknown_inspect_graph_and_journal"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "spawn_member_node"
    result["changes"][-1].update(attempted=True, outcome="unknown_until_graph_readback")
    result["data"]["mutation_state_unknown"] = True
    node, dispatch_error = None, None
    try:
        if external_actor:
            receipt = result["data"].setdefault("external_actor_spawn_receipt", {})
            actor_node_hold, actor_lost = [None], [False]

            def current_external_actor(boundary, returned_node=None):
                if returned_node is not None:
                    actor_node_hold[0] = returned_node
                if actor_lost[0]:
                    raise OperationError("graph_external_actor_current_lost", "Sticky current failure; no next SDK or retry")
                receipt["checkpoint_attempted"] = boundary
                try:
                    def fresh():
                        now = api.member_plan(editor, variable_name, setter, target_class)
                        _external_actor_plan(now, variable_name, setter, target_class)
                        if any(now.get(key) != plan.get(key) for key in _EXTERNAL_ACTOR_IDENTITY):
                            raise OperationError("graph_external_actor_source_changed", "Captured Actor source/default/property role changed; no next SDK")
                        result["data"]["external_actor_last_plan"] = copy.deepcopy(now)
                        return now
                    fresh()
                    held_node = actor_node_hold[0]
                    if held_node is not None:
                        members = editor.list_all_nodes()
                        fresh()
                        if sum(item is held_node for item in members) != 1:
                            raise OperationError("graph_external_actor_return_detached", "Returned actual member wrapper is not uniquely owned by the current graph")
                        observed_graph = graph_api.graph_info(editor)
                        fresh()
                        path = held_node.get_path_name()
                        prior = {row["node_id"]: row for row in before["nodes"]}
                        rows = {row["node_id"]: row for row in observed_graph["nodes"]}
                        row = rows.get(path, {})
                        if (set(rows)-set(prior) != {path} or any(rows.get(key) != value for key, value in prior.items())
                                or row.get("class") != ("K2Node_VariableSet" if setter else "K2Node_VariableGet")
                                or row.get("variable_name") != variable_name or row.get("self_context") is not inherited_actor
                                or (receipt.get("position_returned") is True and (row.get("x") != x or row.get("y") != y))
                                or row.get("variable_member_parent_path") != ("" if inherited_actor else target_class)
                                or row.get("variable_property_path") not in (plan["property_path"], plan["compiled_property_path"])
                                or uuid.UUID(row.get("variable_member_guid", "")) != uuid.UUID(plan["variable_guid"])):
                            raise OperationError("graph_external_actor_node_changed", "Actual new member/reference/property or prior graph changed")
                        if inherited_actor:
                            native_rows = result["data"]["external_actor_last_plan"].get("inherited_member_nodes")
                            if type(native_rows) is not list:
                                raise OperationError("graph_inherited_member_physical_pins_unavailable", "Current Native physical pin roles are required; visible wrappers omit hidden self")
                            matches = [entry for entry in native_rows if type(entry) is dict and entry.get("object_path") == path]
                            if len(matches) != 1:
                                raise OperationError("graph_inherited_member_physical_pins_changed", "Exactly one held current inherited physical node role is required")
                            physical_role = matches[0]
                            if (physical_role.get("graph_path") != before["graph_path"]
                                    or physical_role.get("node_class_path") != ("/Script/BlueprintGraph.K2Node_VariableSet" if setter else "/Script/BlueprintGraph.K2Node_VariableGet")
                                    or uuid.UUID(physical_role.get("node_guid", "")) != uuid.UUID(row.get("node_guid", ""))
                                    or uuid.UUID(physical_role.get("member_guid", "")) != uuid.UUID(plan["variable_guid"])
                                    or physical_role.get("member_name") != variable_name
                                    or physical_role.get("member_self_context") is not True
                                    or physical_role.get("member_parent_path") != ""
                                    or physical_role.get("physical_roles_verified") is not True
                                    or physical_role.get("all_pins_unlinked") is not True
                                    or physical_role.get("self_pin_hidden") is not True
                                    or physical_role.get("value_pin_type_text") != plan["expected_pin_type_text"]
                                    or physical_role.get("self_pin_type_text") != plan["expected_self_pin_type_text"]):
                                raise OperationError("graph_inherited_member_physical_pins_changed", "Actual hidden self/value/exec/reference roles differ from the held source plan")
                            previous = receipt.get("created_native_pin_role")
                            if previous is not None and previous != physical_role:
                                raise OperationError("graph_inherited_member_physical_pins_changed", "Current inherited physical pin identity/type/defaults changed across a callback")
                            receipt["created_native_pin_role"] = copy.deepcopy(physical_role)
                            result["data"]["inherited_member_current_physical_role"] = copy.deepcopy(physical_role)
                        role = {key: copy.deepcopy(value) for key, value in row.items() if key not in ("x", "y")}
                        if receipt.get("created_node_role") is not None and receipt["created_node_role"] != role:
                            raise OperationError("graph_external_actor_node_changed", "Returned member semantic role changed across a callback")
                        receipt.update(created_node_path=path, created_node_role=role)
                        result["data"]["external_actor_current_graph"] = copy.deepcopy(observed_graph)
                    receipt["checkpoint_passed"] = boundary
                except Exception as error:
                    actor_lost[0] = True
                    receipt["checkpoint_failed"] = boundary
                    result["data"].update(mutation_state_unknown=True, external_actor_current_lost=True)
                    failure = getattr(error, "native_graph_result", None)
                    if type(failure) is dict:
                        result["data"]["external_actor_checkpoint_failure"] = copy.deepcopy(failure)
                    raise

            node = api.add_external_actor_member(editor, plan["variable_name"], setter, x, y, target_class,
                receipt=receipt, current=current_external_actor)
        elif native_texture:
            receipt = result["data"].setdefault("native_texture_spawn_receipt", {})
            texture_node_hold = [None]

            def current_texture(boundary, returned_node=None):
                if returned_node is not None:
                    texture_node_hold[0] = returned_node
                receipt["checkpoint_attempted"] = boundary

                def observe():
                    try:
                        now = api.member_plan(editor, variable_name, True, target_class)
                        api._native_texture_plan(now)
                    except OperationError as error:
                        failure = getattr(error, "native_graph_result", None)
                        if type(failure) is dict:
                            result["data"]["native_texture_checkpoint_failure"] = copy.deepcopy(failure)
                        receipt["checkpoint_failed"] = boundary
                        raise
                    if any(now.get(key) != plan.get(key) for key in _TEXTURE_IDENTITY_FIELDS):
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_texture_changed", "Native source/property/enum/target identity changed; no later SDK or retry")
                    result["data"]["native_texture_last_plan"] = copy.deepcopy(now)
                    return now

                now = observe()
                held = texture_node_hold[0]
                if held is not None:
                    path = receipt.get("created_node_path")
                    if type(path) is not str or not path.startswith(before["graph_path"] + ".") or any(row.get("node_id") == path for row in before["nodes"]):
                        raise OperationError("graph_native_texture_return_invalid", "Returned setter is not a fresh identity in the selected graph")
                    nodes = editor.list_all_nodes()
                    if sum(item is held for item in nodes) != 1:
                        raise OperationError("graph_native_texture_node_detached", "Returned native setter is no longer the same current graph object")
                    now = observe()
                    rows = [row for row in now["native_texture_member_nodes"] if row["object_path"] == path]
                    if len(rows) != 1:
                        raise OperationError("graph_native_texture_return_invalid", "Returned setter member/GUID/pins do not have one current native role")
                    role = {key: copy.deepcopy(rows[0].get(key)) for key in _TEXTURE_NODE_FIELDS}
                    if receipt.get("created_node_role") is not None and role != receipt["created_node_role"]:
                        raise OperationError("graph_native_texture_node_changed", "Returned setter reference/GUID/physical pins changed before a later SDK")
                    if boundary == "after_position" and (rows[0].get("x"), rows[0].get("y")) != (x, y):
                        raise OperationError("graph_native_texture_position_changed", "Returned setter did not retain its requested position")
                    receipt["created_node_role"] = role
                    result["data"]["native_texture_current_node_role"] = copy.deepcopy(role)
                receipt["checkpoint_passed"] = boundary

            node = api.add_native_texture_member(editor, variable_name, x, y, target_class, receipt=receipt, current=current_texture)
        elif native_vector_array:
            receipt = result["data"].setdefault("native_vector_array_spawn_receipt", {})
            if native_string_vector_map:
                result["data"]["native_string_vector_map_spawn_receipt"] = receipt

            array_node_hold = [None]

            def current_vector_array(boundary, returned_node=None):
                if returned_node is not None:
                    array_node_hold[0] = returned_node
                receipt["checkpoint_attempted"] = boundary

                def read_current_plan():
                    try:
                        current = api.member_plan(editor, variable_name, setter, target_class)
                    except OperationError as error:
                        native_failure = getattr(error, "native_graph_result", None)
                        if type(native_failure) is dict:
                            result["data"]["native_vector_array_checkpoint_failure"] = copy.deepcopy(native_failure)
                        receipt["checkpoint_failed"] = boundary
                        raise
                    result["data"]["native_vector_array_last_plan"] = copy.deepcopy(current)
                    try:
                        _native_vector_array_plan(current, variable_name, setter, target_class)
                    except OperationError:
                        receipt["checkpoint_failed"] = boundary
                        raise
                    if any(current.get(key) != plan.get(key) for key in (_VECTOR_ARRAY_IDENTITY_FIELDS + (("key_property_path", "key_property_flags_string", "map_property_identity_verified", "map_setter_input_requires_connection", "native_string_vector_map_member") if native_string_vector_map else ()))):
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_vector_array_changed", "The own value/source/property/default identity changed; no later spawn/position/read was dispatched")
                    if target_class and (parent_comparison_state(current.get("external_source_validation", {}))
                            != parent_comparison_state(plan.get("external_source_validation", {}))):
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_vector_array_source_changed", "Saved external array source roles changed; no later SDK read or writer follows")
                    return current

                current = read_current_plan()
                held_node = array_node_hold[0]
                if held_node is not None:
                    created_path = receipt.get("created_node_path")
                    if (type(created_path) is not str or not created_path.startswith(before["graph_path"] + ".")
                            or any(row.get("node_id") == created_path for row in before["nodes"])):
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_vector_array_return_role_invalid", "Returned member identity is not a fresh node of the selected graph")
                    # The public wrapper factory reuses the same wrapper for a held UObject*.
                    # Compare that actual return against current graph members before using it again.
                    current_nodes = editor.list_all_nodes()
                    if sum(item is held_node for item in current_nodes) != 1:
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_vector_array_node_detached", "Returned member is no longer the same current graph object; no position/readback follows")
                    current = read_current_plan()  # Close the list_all_nodes SDK boundary.
                    matches = [row for row in current["native_value_member_nodes"] if row["object_path"] == created_path]
                    expected_class = "/Script/BlueprintGraph.K2Node_VariableSet" if setter else "/Script/BlueprintGraph.K2Node_VariableGet"
                    if len(matches) != 1 or matches[0]["node_class_path"] != expected_class:
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_vector_array_node_role_changed", "Current returned node class/member/property/pin role differs from the requested spawn")
                    role = {key: copy.deepcopy(matches[0].get(key)) for key in _VECTOR_ARRAY_NODE_IDENTITY_FIELDS}
                    previous = receipt.get("created_node_role")
                    if previous is not None and role != previous:
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_vector_array_node_identity_changed", "Returned node GUID/reference or physical pin changed at a current checkpoint")
                    receipt["created_node_role"] = role
                    result["data"]["native_vector_array_current_node_role"] = copy.deepcopy(role)
                receipt["checkpoint_passed"] = boundary

            node = api.add_vector_array_member(editor, plan["variable_name"], setter, x, y, target_class,
                receipt=receipt, current=current_vector_array)
        elif own_material_value:
            receipt = result["data"].setdefault("native_material_value_spawn_receipt", {})

            material_node_hold = [None]

            def current_material_value(boundary, returned_node=None):
                if returned_node is not None:
                    material_node_hold[0] = returned_node
                receipt["checkpoint_attempted"] = boundary

                def read_current_plan():
                    try:
                        current = api.member_plan(editor, variable_name, setter)
                    except OperationError as error:
                        native_failure = getattr(error, "native_graph_result", None)
                        if type(native_failure) is dict:
                            result["data"]["native_material_value_checkpoint_failure"] = copy.deepcopy(native_failure)
                        receipt["checkpoint_failed"] = boundary
                        raise
                    result["data"]["native_material_value_last_plan"] = copy.deepcopy(current)
                    try:
                        _own_material_value_plan(current, variable_name, setter, "")
                    except OperationError:
                        receipt["checkpoint_failed"] = boundary
                        raise
                    if any(current.get(key) != plan.get(key) for key in _MATERIAL_VALUE_IDENTITY_FIELDS):
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_material_value_changed", "The own value/source/property/default identity changed; no later spawn/position/read was dispatched")
                    return current

                current = read_current_plan()
                held_node = material_node_hold[0]
                if held_node is not None:
                    created_path = receipt.get("created_node_path")
                    if (type(created_path) is not str or not created_path.startswith(before["graph_path"] + ".")
                            or any(row.get("node_id") == created_path for row in before["nodes"])):
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_material_value_return_role_invalid", "Returned member identity is not a fresh node of the selected graph")
                    # The public wrapper factory reuses the same wrapper for a held UObject*.
                    # Compare that actual return against current graph members before using it again.
                    current_nodes = editor.list_all_nodes()
                    if sum(item is held_node for item in current_nodes) != 1:
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_material_value_node_detached", "Returned member is no longer the same current graph object; no position/readback follows")
                    current = read_current_plan()  # Close the list_all_nodes SDK boundary.
                    matches = [row for row in current["native_value_member_nodes"] if row["object_path"] == created_path]
                    expected_class = "/Script/BlueprintGraph.K2Node_VariableSet" if setter else "/Script/BlueprintGraph.K2Node_VariableGet"
                    if len(matches) != 1 or matches[0]["node_class_path"] != expected_class:
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_material_value_node_role_changed", "Current returned node class/member/property/pin role differs from the requested spawn")
                    role = {key: copy.deepcopy(matches[0].get(key)) for key in _MATERIAL_VALUE_NODE_IDENTITY_FIELDS}
                    previous = receipt.get("created_node_role")
                    if previous is not None and role != previous:
                        receipt["checkpoint_failed"] = boundary
                        raise OperationError("graph_native_material_value_node_identity_changed", "Returned node GUID/reference or physical pin changed at a current checkpoint")
                    receipt["created_node_role"] = role
                    result["data"]["native_material_value_current_node_role"] = copy.deepcopy(role)
                receipt["checkpoint_passed"] = boundary

            node = api.add_material_value_member(editor, plan["variable_name"], setter, x, y,
                receipt=receipt, current=current_material_value)
        elif native_child_actor or own_object_array:
            receipt = result["data"].setdefault("native_child_actor_spawn_receipt" if native_child_actor else "object_array_spawn_receipt", {})
            node = api.add_member(editor, plan["variable_name"], setter, x, y, target_class, receipt=receipt)
        else:
            node = (api.add_member(editor, plan["variable_name"], setter, x, y, target_class)
                    if target_class else api.add_member(editor, plan["variable_name"], setter, x, y))
    except Exception as error:
        dispatch_error = error
        result["data"]["member_dispatch_error"] = str(error)
    if external_actor and dispatch_error is not None:
        result["stage"] = "external_actor_dispatch_failed"
        result["data"]["mutation_state_unknown"] = True
        result["changes"][-1].update(outcome="dispatch_or_current_failed_inspect_returned_prefix")
        raise dispatch_error
    if native_texture and dispatch_error is not None:
        result["stage"] = "native_texture_dispatch_failed"
        result["data"]["mutation_state_unknown"] = True
        result["changes"][-1].update(outcome="dispatch_or_current_failed_inspect_returned_prefix")
        raise dispatch_error
    if native_vector_array and dispatch_error is not None:
        result["stage"] = "native_vector_array_dispatch_failed"
        result["data"]["mutation_state_unknown"] = True
        result["changes"][-1].update(outcome="dispatch_or_current_failed_inspect_returned_prefix")
        raise dispatch_error
    if own_material_value and dispatch_error is not None:
        # An uncertain spawn or failed native current check cannot authorize another SDK read.
        result["stage"] = "native_material_value_dispatch_failed"
        result["data"]["mutation_state_unknown"] = True
        result["changes"][-1].update(outcome="dispatch_or_current_failed_inspect_returned_prefix")
        raise dispatch_error
    result["stage"] = "readback"
    if external_actor:
        current_external_actor("before_graph_readback", node)
    if native_texture:
        current_texture("before_graph_readback")
    if native_vector_array:
        current_vector_array("before_graph_readback")
    if own_material_value:
        current_material_value("before_graph_readback")
    try:
        after = graph_api.graph_info(editor)
        if native_member or own_object_array or own_material_value or native_vector_array:
            # Retain the observed partial graph even if later native-class
            # observation fails; never repeat the already dispatched spawn.
            result["data"]["after"] = copy.deepcopy(after)
    finally:
        if target_class and not native_vector_array and not native_texture and not external_actor:
            source_after = api.external_source_state(target_class)
            result["data"].update(external_source_after=source_after,
                external_source_preserved=_source_preserved(source_baseline, source_after))
    result["data"]["after"] = copy.deepcopy(after)
    if external_actor:
        current_external_actor("after_graph_readback", node)
    if native_texture:
        current_texture("after_graph_readback")
    if native_vector_array:
        current_vector_array("after_graph_readback")
        if target_class:
            source_after = api.external_source_state(target_class)
            current_vector_array("after_external_source_state_read")
            source_preserved = _source_preserved(source_baseline, source_after)
            result["data"].update(external_source_after=source_after, external_source_preserved=source_preserved)
            if not source_preserved:
                result["data"]["mutation_state_unknown"] = True
                raise OperationError("graph_native_vector_array_source_changed", "External source state changed after spawn; retain the observed graph without another read or retry")
    if own_material_value:
        current_material_value("after_graph_readback")
    if dispatch_error is not None:
        result["changes"][-1].update(outcome="dispatch_failed_after_graph_observation", graph_changed=before != after)
        # A failed/absent reply stays uncertain even when current graph changes
        # are observable; retain evidence and never dispatch another spawn.
        raise dispatch_error
    created_id = (receipt["created_node_path"] if external_actor or own_material_value or native_vector_array or native_texture else graph_api.node_id(node))
    old = {row["node_id"]: row for row in before["nodes"]}
    current = {row["node_id"]: row for row in after["nodes"]}
    observed = current.get(created_id, {})
    expected_class = "K2Node_VariableSet" if setter else "K2Node_VariableGet"
    property_path_passed = external_actor and observed.get("variable_property_path") in (plan["property_path"], plan["compiled_property_path"])
    if native_texture:
        # Generic graph observation does not resolve these four external native enum properties.
        # Keep its raw empty cell, and require the latest exact native property/member role instead.
        current_texture("before_enum_pin_readback")
        role = result["data"]["native_texture_current_node_role"]
        generic_path = observed.get("variable_property_path")
        native_path = role.get("property_path")
        try:
            same_node_guid = uuid.UUID(observed.get("node_guid", "")) == uuid.UUID(role.get("node_guid", ""))
        except (ValueError, TypeError, AttributeError):
            same_node_guid = False
        native_property_passed = (native_path == plan["property_path"] and type(native_path) is str and bool(native_path)
            and role.get("object_path") == created_id and role.get("graph_path") == after["graph_path"]
            and role.get("outer_path") == after["graph_path"] and same_node_guid
            and role.get("node_class_path") == "/Script/BlueprintGraph.K2Node_VariableSet"
            and result["data"]["native_texture_last_plan"].get("native_property_identity_verified") is True)
        property_path_passed = native_property_passed and type(generic_path) is str and generic_path in ("", native_path)
        result["data"]["native_texture_property_identity_readback"] = {
            "generic_property_path": generic_path, "generic_property_path_available": bool(generic_path),
            "native_current_property_path": native_path, "expected_property_path": plan["property_path"],
            "native_current_property_verified": native_property_passed, "same_node_guid_verified": same_node_guid,
            "basis": "latest exact native property/source and held member-node current role; generic raw graph is unchanged",
            "verified": property_path_passed}
    passed = (set(current) - set(old) == {created_id} and all(current.get(key) == value for key, value in old.items())
        and observed.get("class") == expected_class and observed.get("x") == x and observed.get("y") == y
        and observed.get("variable_name") == plan["variable_name"] and observed.get("self_context") is (not bool(target_class))
        and (property_path_passed if native_texture or external_actor else observed.get("variable_property_path") == (result["data"]["native_vector_array_current_node_role"]["property_path"] if native_vector_array else plan["property_path"])))
    if native_texture:
        role = result["data"]["native_texture_current_node_role"]
        value_pin = [p for p in role["pins"] if p["name"] == variable_name and p["direction"] == "input"]
        self_pin = [p for p in role["pins"] if p["name"] == "self" and p["direction"] == "input"]
        typed_passed = (len(value_pin) == len(self_pin) == 1 and value_pin[0]["type_text"] == plan["expected_pin_type_text"] and self_pin[0]["type_text"] == plan["expected_self_pin_type_text"])
        try:
            guid_passed = uuid.UUID(observed.get("variable_member_guid", "")).int == 0
        except (ValueError, TypeError, AttributeError):
            guid_passed = False
        passed = passed and typed_passed and guid_passed and observed.get("variable_member_parent_path") == target_class
        result["data"].update(member_after=copy.deepcopy(result["data"]["native_texture_last_plan"]), native_texture_member_verified=passed,
            member_output_type_verified=typed_passed, native_member_guid_zero_verified=guid_passed,
            native_texture_runtime_value_written=False, external_class_load_attempted=False, external_source_preserved=True,
            member_spawn_scope="Official spawner resolves the already-loaded exact owner class path internally; no claim that it never calls a load API")
    elif native_vector_array:
        current_vector_array("before_array_pin_readback")
        role = result["data"]["native_vector_array_current_node_role"]
        observed_type = role["value_pin_type_text"]
        typed_passed = observed_type == plan["expected_pin_type_text"]
        if target_class:
            typed_passed = typed_passed and role.get("self_pin_type_text") == plan["expected_self_pin_type_text"]
        final_graph = graph_api.graph_info(editor)
        result["data"]["native_vector_array_final_graph"] = copy.deepcopy(final_graph)
        current_vector_array("after_final_graph_readback")
        graph_preserved = final_graph == after
        after = final_graph
        result["data"]["after"] = copy.deepcopy(after)
        current = {row["node_id"]: row for row in after["nodes"]}
        observed = current.get(created_id, {})
        try:
            guid_passed = uuid.UUID(observed.get("variable_member_guid", "")) == uuid.UUID(plan["variable_guid"])
        except (TypeError, ValueError, AttributeError):
            guid_passed = False
        source_preserved = not target_class or result["data"].get("external_source_preserved") is True
        parent_passed = not target_class or observed.get("variable_member_parent_path") == target_class
        result["data"].update(member_after=copy.deepcopy(result["data"]["native_vector_array_last_plan"]),
            member_pin_type_text=observed_type, member_output_type_verified=typed_passed,
            native_vector_array_member_verified=typed_passed and guid_passed and graph_preserved and source_preserved and parent_passed,
            native_vector_array_guid_verified=guid_passed, native_vector_array_graph_preserved=graph_preserved,
            runtime_value_read_attempted=False, default_write_attempted=False, runtime_evaluation_verified=False)
        if native_string_vector_map:
            result["data"].update({"native_string_double_map_member_verified" if plan.get("member_scope") == "own_native_string_double_map" else "native_string_vector_map_member_verified": typed_passed and guid_passed and graph_preserved})
            result["data"].update(map_setter_input_requires_connection=setter, runtime_map_assignment_verified=False)
        passed = passed and typed_passed and guid_passed and graph_preserved and source_preserved and parent_passed
    elif external_actor:
        current_external_actor("before_typed_readback", node)
        if inherited_actor:
            # The exact Native observation includes the real bHidden self pin.
            # Public FBlueprintGraphPin wrappers cannot represent that pin.
            physical_role = result["data"]["inherited_member_current_physical_role"]
            value_type = physical_role["value_pin_type_text"]
            self_type = physical_role["self_pin_type_text"]
            result["data"].update(inherited_hidden_self_verified=True,
                inherited_self_observation="held_native_physical_pin_not_visible_wrapper")
        else:
            value_type = api.member_pin_type(node, plan["variable_name"], "input" if setter else "output")
            current_external_actor("after_value_pin_readback", node)
            self_type = api.member_pin_type(node, "self", "input")
            current_external_actor("after_self_pin_readback", node)
        typed_passed = value_type == plan["expected_pin_type_text"] and self_type == plan["expected_self_pin_type_text"]
        result["data"].update(external_actor_member=not inherited_actor, external_actor_source_preserved=True,
            inherited_actor_member=inherited_actor, inherited_actor_member_verified=inherited_actor and passed and typed_passed,
            external_source_preserved=True, external_actor_member_verified=(not inherited_actor) and passed and typed_passed,
            member_pin_types={"variable": value_type, "self": self_type}, member_output_type_verified=typed_passed,
            external_actor_last_plan=copy.deepcopy(result["data"]["external_actor_last_plan"]),
            external_actor_runtime_value_verified=False)
        passed = passed and typed_passed
    elif target_class:
        after_plan = api.member_plan(editor, variable_name, setter, target_class)
        result["data"]["member_after"] = after_plan
        source_preserved = result["data"]["external_source_preserved"]
        plan_preserved = (parent_comparison_state(after_plan.get("external_source_validation", {}))
            == parent_comparison_state(plan.get("external_source_validation", {})) and
            all(after_plan.get(key) == plan.get(key) for key in (
                "variable_guid", "target_class_path", "declaring_class_path", "declaring_blueprint_path",
                "property_path", "compiled_property_path", "expected_pin_type_text", "expected_self_pin_type_text")))
        if native_member:
            plan_preserved = plan_preserved and all(after_plan.get(key) == plan.get(key) for key in (
                "member_scope", "setter", "variable_name", "pin_category", "pin_subcategory", "native_source_before", "native_source_after",
                "native_property_expected_flags_string", "native_property_flags_string", "native_property_identity_verified", "native_source_observed_unchanged"))
        pin_types = {"variable": api.member_pin_type(node, plan["variable_name"], "input" if setter else "output"),
                     "self": api.member_pin_type(node, "self", "input")}
        typed_passed = (bool(plan.get("expected_pin_type_text")) and bool(plan.get("expected_self_pin_type_text"))
            and pin_types["variable"] == plan["expected_pin_type_text"] and pin_types["self"] == plan["expected_self_pin_type_text"])
        try:
            observed_guid = uuid.UUID(observed.get("variable_member_guid", ""))
            guid_passed = observed_guid == uuid.UUID(plan["variable_guid"]) and (not observed_guid.int if native_member else bool(observed_guid.int))
        except (TypeError, ValueError, AttributeError, KeyError):
            guid_passed = False
        result["data"].update(external_source_preserved=source_preserved and plan_preserved,
            member_pin_types=pin_types, member_output_type_verified=typed_passed, external_member_guid_verified=guid_passed)
        if native_member:
            result["data"].update(native_member_guid_zero_verified=guid_passed,
                member_spawn_scope="Installed official spawner internally resolves the supplied class path; it was independently already loaded. No claim that the spawner avoids load APIs.")
            if native_child_actor:
                result["data"].update(native_child_actor_getter_verified=source_preserved and plan_preserved and typed_passed and guid_passed,
                    native_child_actor_private_readonly_verified=after_plan.get("native_child_actor_private_readonly_verified") is True,
                    native_child_actor_runtime_evaluation_verified=False)
            else:
                result["data"]["native_pawn_eye_height_getter_verified"] = source_preserved and plan_preserved and typed_passed and guid_passed
        passed = (passed and source_preserved and plan_preserved and typed_passed and guid_passed
            and observed.get("variable_member_parent_path") == target_class)
    elif own_material_value:
        # The plan is a fresh native observation, never a retained permission token.
        current_material_value("before_value_pin_readback")
        observed_type = result["data"]["native_material_value_current_node_role"]["value_pin_type_text"]
        final_graph = graph_api.graph_info(editor)
        result["data"]["native_material_value_final_graph"] = copy.deepcopy(final_graph)
        current_material_value("after_final_graph_readback")
        graph_preserved = final_graph == after
        after = final_graph
        result["data"]["after"] = copy.deepcopy(after)
        current = {row["node_id"]: row for row in after["nodes"]}
        observed = current.get(created_id, {})
        result["data"]["member_after"] = copy.deepcopy(result["data"]["native_material_value_last_plan"])
        typed_passed = observed_type == plan["expected_pin_type_text"]
        try:
            guid_passed = uuid.UUID(observed.get("variable_member_guid", "")) == uuid.UUID(plan["variable_guid"])
        except (TypeError, ValueError, AttributeError):
            guid_passed = False
        result["data"].update(member_pin_type_text=observed_type, member_output_type_verified=typed_passed,
            native_material_value_member_verified=typed_passed and guid_passed and graph_preserved,
            native_material_value_guid_verified=guid_passed, native_material_value_graph_preserved=graph_preserved,
            runtime_value_read_attempted=False,
            default_write_attempted=False, runtime_evaluation_verified=False)
        passed = passed and typed_passed and guid_passed and graph_preserved
    elif own_object_array:
        try:
            after_plan = api.member_plan(editor, variable_name, setter)
        except OperationError as error:
            native_failure = getattr(error, "native_graph_result", None)
            if type(native_failure) is dict and native_failure.get("member_scope") == "own_native_object_array":
                result["data"]["member_after_failure"] = copy.deepcopy(native_failure)
            raise
        result["data"]["member_after"] = after_plan
        _own_object_array_plan(after_plan, variable_name, setter, "")
        plan_preserved = all(after_plan.get(key) == plan.get(key) for key in _OBJECT_ARRAY_IDENTITY_FIELDS)
        observed_type = api.member_pin_type(node, plan["variable_name"], "input" if setter else "output")
        typed_passed = observed_type == plan["expected_pin_type_text"]
        try:
            guid_passed = uuid.UUID(observed.get("variable_member_guid", "")) == uuid.UUID(plan["variable_guid"])
        except (TypeError, ValueError, AttributeError):
            guid_passed = False
        result["data"].update(member_pin_type_text=observed_type, member_output_type_verified=typed_passed,
            object_array_member_plan_preserved=plan_preserved, object_array_member_guid_verified=guid_passed,
            own_object_array_member_verified=typed_passed and plan_preserved and guid_passed,
            object_array_runtime_evaluation_verified=False, array_value_read_attempted=False, array_default_write_attempted=False)
        passed = passed and typed_passed and plan_preserved and guid_passed
    elif plan.get("member_scope") == "own_runtime_object":
        observed_type = api.member_pin_type(node, plan["variable_name"], "input" if setter else "output")
        typed_passed = bool(plan.get("expected_pin_type_text")) and observed_type == plan["expected_pin_type_text"]
        result["data"].update(member_pin_type_text=observed_type, member_output_type_verified=typed_passed)
        passed = passed and typed_passed
    elif plan.get("member_scope") == "own_native_class":
        after_plan = api.member_plan(editor, variable_name, False)
        result["data"]["member_after"] = after_plan
        _native_class_plan(after_plan, variable_name, False, "")
        plan_preserved = all(after_plan.get(key) == plan.get(key) for key in _NATIVE_CLASS_IDENTITY_FIELDS)
        observed_type = api.member_pin_type(node, plan["variable_name"], "output")
        typed_passed = observed_type == plan["expected_pin_type_text"]
        result["data"].update(member_pin_type_text=observed_type, member_output_type_verified=typed_passed,
            native_class_member_plan_preserved=plan_preserved,
            native_class_getter_verified=typed_passed and plan_preserved,
            native_class_runtime_evaluation_verified=False)
        passed = passed and typed_passed and plan_preserved
    elif plan.get("member_scope") in _NATIVE_LOCAL_SCS_SIGNATURES:
        family, _, _, runtime_field = _NATIVE_LOCAL_SCS_SIGNATURES[plan["member_scope"]]
        after_plan = api.member_plan(editor, variable_name, False)
        result["data"]["member_after"] = after_plan
        _native_local_scs_plan(after_plan, variable_name, False, "")
        plan_preserved = all(after_plan.get(key) == plan.get(key) for key in _native_scs_fields(family, runtime_field))
        observed_type = api.member_output_type(node, plan["variable_name"])
        typed_passed = observed_type == plan["expected_pin_type_text"]
        result["data"].update(output_pin_type_text=observed_type, member_output_type_verified=typed_passed,
            component_output_type_verified=typed_passed)
        result["data"].update({"native_" + family + "_member_plan_preserved": plan_preserved,
            "native_" + family + "_local_getter_verified": typed_passed and plan_preserved,
            "native_" + family + "_" + runtime_field: False})
        passed = passed and typed_passed and plan_preserved
    if plan.get("member_scope") in ("local_scs_generated_component", "inherited_actor_boolean"):
        observed_type = api.member_output_type(node, plan["variable_name"])
        typed_passed = bool(plan.get("expected_pin_type_text")) and observed_type == plan["expected_pin_type_text"]
        result["data"].update(output_pin_type_text=observed_type, member_output_type_verified=typed_passed)
        if plan["member_scope"] == "local_scs_generated_component":
            result["data"]["component_output_type_verified"] = typed_passed
        else:
            after_plan = api.member_plan(editor, variable_name, False)
            result["data"]["member_after"] = after_plan
            parent_preserved = (parent_comparison_state(after_plan.get("declaring_parent_validation", {}))
                == parent_comparison_state(plan.get("declaring_parent_validation", {})) and
                all(after_plan.get(key) == plan.get(key) for key in (
                    "variable_guid", "declaring_class_path", "declaring_blueprint_path", "property_path", "expected_pin_type_text")))
            result["data"]["declaring_parent_preserved"] = parent_preserved
            passed = passed and parent_preserved
        passed = passed and not setter and typed_passed
    result["data"].update(observed, member_preflight=plan, mutation_state_unknown=not passed)
    result["changes"][-1].update(outcome="observed_new_member_node" if passed else "postcondition_mismatch_inspect_graph",
        created_node_id=created_id)
    actual_created = (created_id in current and created_id not in old) if external_actor or own_material_value or native_vector_array else passed
    _contract(result, before, after, kind, created=1 if actual_created else 0, passed=passed)


def _add(blueprint_name, variable_name, graph_name, x, y, setter, operation_id, **options):
    name = "add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node"
    args = {"blueprint_name": blueprint_name, "variable_name": variable_name, "graph_name": graph_name, "x": x, "y": y}
    try:
        extras = _options(**options, x=x, y=y)
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    args.update(extras)
    return mutate(name, args, operation_id, lambda result: _add_action(result, blueprint_name, variable_name, graph_name, x, y, setter, **extras))


def add_blueprint_variable_get_node(blueprint_name, variable_name, graph_name="EventGraph", x=0, y=0, operation_id=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    return _add(blueprint_name, variable_name, graph_name, x, y, False, operation_id, target_class=target_class,
        graph_id=graph_id, graph_type=graph_type, create_graph_if_missing=create_graph_if_missing, node_position_json=node_position_json)


def add_blueprint_variable_set_node(blueprint_name, variable_name, graph_name="EventGraph", x=0, y=0, operation_id=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    return _add(blueprint_name, variable_name, graph_name, x, y, True, operation_id, target_class=target_class,
        graph_id=graph_id, graph_type=graph_type, create_graph_if_missing=create_graph_if_missing, node_position_json=node_position_json)


def add_blueprint_variable_get_node_action(result, blueprint_name, variable_name, graph_name="EventGraph", x=0, y=0, checkpoint_callback=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    _add_action(result, blueprint_name, variable_name, graph_name, x, y, False, checkpoint_callback,
        target_class, graph_id, graph_type, create_graph_if_missing, node_position_json)


def add_blueprint_variable_set_node_action(result, blueprint_name, variable_name, graph_name="EventGraph", x=0, y=0, checkpoint_callback=None,
        target_class=None, graph_id=None, graph_type=None, create_graph_if_missing=False, node_position_json=None):
    _add_action(result, blueprint_name, variable_name, graph_name, x, y, True, checkpoint_callback,
        target_class, graph_id, graph_type, create_graph_if_missing, node_position_json)
