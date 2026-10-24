"""UE58 public duplicate API and bounded, independently observed clone content.

EditorAssetSubsystem supplies an unattended duplicate guard. Blueprint PostDuplicate
still performs implicit compilation; AssetTools can also save through source control.
The policy checks below prevent those *known* implicit save paths without changing
editor preferences. They are not a proof about arbitrary engine callbacks.
"""

import hashlib
import copy
import json
from pathlib import Path
import re
import uuid
import unreal

from . import blueprint, graph, variables
from cubeless_mcp_ue58.operations.state import OperationError
from cubeless_mcp_ue58.validators.dependencies import reject_tool_reference


def package_path(value, field, *, simple_name=False):
    if type(value) is not str or not value.strip():
        raise OperationError("duplicate_path_invalid", field + " must be a nonempty package/object path")
    path = value.strip().replace("\\", "/")
    # Match the legacy convenient object/export-text/.uasset spellings while
    # validating the completed object path before any FName or load call.
    if "'" in path:
        first = path.find("'")
        if not path.endswith("'") or "'" in path[first + 1:-1]:
            raise OperationError("duplicate_path_invalid", field + " has malformed export-text quoting")
        path = path[first + 1:-1]
    path = path.strip('"').rstrip("/")
    if path.endswith(".uasset"):
        path = path[:-7]
    if not path.startswith("/") and simple_name:
        path = "/Game/Blueprints/" + path
    if "." in path:
        package, name = path.split(".", 1)
        if name != package.rsplit("/", 1)[-1]:
            raise OperationError("duplicate_path_invalid", field + " object name must match its package asset name")
        path = package
    directory, _, name = path.rpartition("/")
    try:
        canonical = blueprint.new_asset_path(name, directory)
    except (OperationError, UnicodeError) as error:
        raise OperationError("duplicate_path_invalid", field + ": " + str(error)) from error
    if (canonical != path or not path.startswith("/Game/")
            or any(ord(c) < 32 or ord(c) == 127 or 0xD800 <= ord(c) <= 0xDFFF for c in path)):
        raise OperationError("duplicate_path_invalid", field + " must be a bounded /Game asset path without controls")
    return path


def file_state(package):
    content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
    path = content.joinpath(*package[len("/Game/"):].split("/")).with_suffix(".uasset").resolve()
    if not path.is_relative_to(content):
        raise OperationError("duplicate_path_invalid", "Resolved asset file escapes project Content")
    digest = None
    if path.is_file():
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"path": str(path), "exists": path.is_file(), "sha256": digest}


def target_state(package):
    subsystem = blueprint.asset_subsystem()
    object_path = package + "." + package.rsplit("/", 1)[-1]
    disk = file_state(package)
    return {"asset_registered": bool(subsystem.does_asset_exist(package)),
            "directory_exists": bool(subsystem.does_directory_exist(package)),
            "loaded_package_or_object": unreal.find_object(None, package) is not None or unreal.find_object(None, object_path) is not None,
            "file": disk}


def require_target_absent(package, replacement=None):
    if replacement is not None and package != replacement["target_package"]:
        raise OperationError("duplicate_replacement_path_changed", "Private replacement context belongs to another target")
    if replacement is not None and replacement["retired"]:
        return replacement_absent(replacement)
    value = target_state(package)
    if value["asset_registered"] or value["directory_exists"] or value["loaded_package_or_object"] or value["file"]["exists"]:
        raise OperationError("duplicate_target_exists", "Target exists in the registry, loaded objects or disk; nothing was deleted")
    return value


def policy():
    current = blueprint.creation_policy()
    # USourceControlHelpers declares ScriptName="SourceControl" in UE58.
    # The C++ class spelling is deliberately not a Python fallback.
    is_enabled = getattr(getattr(unreal, "SourceControl", None), "is_enabled", None)
    if not callable(is_enabled):
        raise OperationError("duplicate_source_control_api_unavailable", "Required public unreal.SourceControl.is_enabled is unavailable; duplicate was not called")
    enabled = is_enabled()
    if type(enabled) is not bool:
        raise OperationError("duplicate_source_control_api_unavailable", "SourceControl.is_enabled did not return a boolean; duplicate was not called")
    if enabled:
        raise OperationError("duplicate_source_control_unsupported", "AssetTools duplication may save via enabled source control; disable it explicitly before this operation. No setting was changed")
    return {"save_on_compile": current["save_on_compile"], "source_control_enabled": False,
            "postduplicate_implicit_compile": True, "public_duplicate_skip_save_option": False,
            "settings_changed": False, "scope": "sampled known engine auto-save paths; arbitrary callbacks are not audited"}


def dependency_audit(package, source_work=None):
    native = getattr(unreal, "CubelessNativeToolset", None)
    if native is None:
        raise OperationError("duplicate_dependency_guard_unavailable", "Native saved-package dependency audit is unavailable")
    value = json.loads(native.audit_package_dependencies([package], True, 4096))
    if source_work is not None:
        return _dirty_dependency_audit(package, source_work, value)
    if value.get("success") is not True or value.get("passed") is not True:
        raise OperationError("duplicate_dependency_audit_failed", "Saved package dependency audit is incomplete or contains tool dependencies: " + json.dumps(value, ensure_ascii=False))
    for edge in value.get("edges", []):
        # /Game/CubelessMCPTests is a user package, not the Cubeless plugin.
        # The native audit handles plugin mount/module prefixes precisely.
        if edge["to"].startswith(("/Script/", "/Engine/PythonTypes")):
            reject_tool_reference(edge["to"])
    return value



def _dirty_dependency_audit(package, work, saved_audit):
    # The saved scanner's refusal is retained verbatim, never relabeled success.
    # Only its precise dirty-root coverage gap may be filled by the existing
    # Native package-export/loaded authored+persisted dependency scanner.
    receipt = work["receipt"]
    receipt["latest_dependency_audit"] = {"saved_registry_audit": saved_audit}
    dirty_reason = "Loaded dirty packages have unsaved state that is not represented by the on-disk dependency graph."
    if (package != work["expected_memory"]["blueprint"]["path"]
            or saved_audit.get("root_packages") != [package]
            or saved_audit.get("scope") != "saved_asset_registry_hard_and_soft_package_dependencies"
            or saved_audit.get("recursive") is not True
            or saved_audit.get("incomplete_reasons") != [dirty_reason]
            or saved_audit.get("dirty_scanned_packages") != [package]
            or saved_audit.get("violations") != []
            or saved_audit.get("packages_processed") != saved_audit.get("packages_queued")):
        raise OperationError("duplicate_dependency_audit_failed", "Dirty source requires an otherwise complete saved graph with only its own dirty-root gap: " + json.dumps(saved_audit, ensure_ascii=False))
    for edge in saved_audit.get("edges", []):
        if edge.get("forbidden") is not False:
            raise OperationError("duplicate_dependency_audit_failed", "Saved graph has a forbidden or unclassified edge")
        reject_tool_reference(edge["to"])
    library = getattr(unreal, "CubelessBlueprintNetworkingLibrary", None)
    observer = getattr(library, "audit_loaded_blueprint_duplicate_source", None)
    if not callable(observer):
        raise OperationError("duplicate_dirty_dependency_observer_unavailable", "Build/load the private readonly RAM+persisted dependency observer; dirty incomplete was not waived")
    dirty_source_current(work)
    observed = json.loads(observer(work["source_role"]["object"]))
    receipt["latest_dependency_audit"]["loaded_source_audit"] = observed
    receipt["dependency_checks"] = receipt.get("dependency_checks", 0) + 1
    dirty_source_current(work)
    data = observed.get("data", {})
    if (observed.get("success") is not True or observed.get("postcondition_passed") is not True
            or observed.get("change_state_known") is not True or observed.get("changed") is not False
            or observed.get("mutation_attempted") is not False or observed.get("compile_attempted") is not False
            or observed.get("save_attempted") is not False or observed.get("saved") is not False
            or data.get("passed") is not True or data.get("issues") != []
            or data.get("source_package") != package
            or data.get("source_object") != work["source_role"]["path"]
            or package not in data.get("loaded_packages_audited", [])
            or package not in data.get("saved_packages_audited", [])):
        raise OperationError("duplicate_dirty_dependency_audit_failed", "Readonly source RAM+persisted dependency observation rejected or remained incomplete: " + json.dumps(observed, ensure_ascii=False))
    work["dependency_guard_ready"] = True
    combined = {"success": True, "passed": True, "scope": "saved_registry_plus_current_loaded_source_package_dependencies",
        "saved_registry_audit": saved_audit, "loaded_source_audit": observed,
        "packaging_verified": False, "whole_RAM_authored_equality_proven": False}
    receipt["latest_dependency_audit"] = combined
    return combined


def capture(asset):
    basic = blueprint.snapshot(asset)
    if len(basic["graph_names"]) > 64:
        raise OperationError("duplicate_observation_unsupported", "Clone observation supports at most 64 graphs")
    graphs = []
    for name in basic["graph_names"]:
        editor = graph.graph_editor(basic["asset_path"], name)
        info = graph.graph_info(editor)
        parent_calls = [node for node in info["nodes"] if node.get("class") == "K2Node_CallParentFunction"
                        and node.get("function_name") in ("ReceiveBeginPlay", "ReceiveTick")]
        if parent_calls and basic["parent_class"].startswith("/Game/"):
            # The ordinary graph DTO intentionally omits this duplicate-only
            # proof. Reuse its existing read API; never infer current SKEL roles.
            identities = graph._identities(editor.get_graph().get_path_name())
            if set(identities) != {node["node_id"] for node in info["nodes"]}:
                raise OperationError("duplicate_parent_call_observation_failed", "Parent-call proof and graph node membership differ")
            for node in parent_calls:
                identity = identities[node["node_id"]]
                if (identity.get("node_guid") != node["node_guid"]
                        or any(identity.get(key) != node.get(key) for key in ("function_name", "function_path", "function_owner_path"))
                        or identity.get("node_class_path") != "/Script/BlueprintGraph.K2Node_CallParentFunction"
                        or type(identity.get("parent_call_proof")) is not dict
                        or identity["parent_call_proof"].get("valid") is not True):
                    raise OperationError("duplicate_parent_call_observation_failed", "Exact current parent-call class/reference/generated/skeleton proof is unavailable or changed between reads")
                node["node_class_path"] = identity["node_class_path"]
                node["parent_call_proof"] = identity["parent_call_proof"]
        # Public BlueprintGraphPin.GetPinType + StructBase.export_text retains
        # native category/subcategory/reference/container fields, unlike the
        # UI display label or lossy JSON-schema "number" category.
        types = {}
        for node in editor.list_all_nodes():
            for pin in node.list_all_pins():
                direction = "output" if pin.get_pin_direction() == unreal.EdGraphPinDirection.EGPD_OUTPUT else "input"
                key = (node.get_path_name(), str(pin.get_pin_name()), direction)
                if key in types:
                    raise OperationError("duplicate_observation_unsupported", "Duplicate pin names/directions cannot be independently matched")
                native_type = pin.get_pin_type()
                exported = native_type.export_text()
                if not exported or "PinCategory=" not in exported:
                    raise OperationError("duplicate_observation_failed", "Native pin type export lacks its category")
                types[key] = exported
        for node in info["nodes"]:
            signature = None
            if node["class"] == "K2Node_CustomEvent":
                # Python GetEditorProperty blocks these plain protected fields.
                # Consume the existing native read-only graph observation, which
                # inspects one exact OutputDelegate and already-loaded own
                # UFunction. Keep all observed identity data on that pin.
                signature = node.pop("custom_event_signature", None)
                fields = ("node_class_path", "member_name", "member_parent_path", "member_parent_class_path",
                          "function_path", "function_owner_path", "function_class_path")
                if (type(node.get("custom_function_name")) is not str or type(signature) is not dict
                        or signature.get("valid") is not True
                        or any(type(signature.get(key)) is not str for key in fields)):
                    raise OperationError("duplicate_custom_event_observation_failed", "Saved custom-event delegate requires a complete valid native loaded own-function observation; protected Python property access is not attempted")
                if sum(pin["name"] == "OutputDelegate" and pin["direction"] == "output" for pin in node["pins"]) != 1:
                    raise OperationError("duplicate_custom_event_observation_failed", "Custom-event native signature does not identify exactly one OutputDelegate output")
            for pin in node["pins"]:
                pin["native_type_export"] = types.pop((node["node_id"], pin["name"], pin["direction"]))
                if signature is not None and (pin["name"], pin["direction"]) == ("OutputDelegate", "output"):
                    pin["custom_event_signature"] = dict(signature)
        if types:
            raise OperationError("duplicate_observation_failed", "Pin identities changed during native type observation")
        graphs.append(info)
    if sum(row["total_node_count"] for row in graphs) > 4096:
        raise OperationError("duplicate_observation_unsupported", "Clone observation supports at most 4096 total graph nodes")
    read = variables.inspect_blueprint_variables(basic["asset_path"])
    if not read["success"] or read["changed"] is not False or read["mutation_attempted"]:
        raise OperationError("duplicate_observation_failed", "Variable observation failed or reported state changes")
    variable_state = read["data"]["before"]
    seen = set()
    for row in variable_state["new_variables"]:
        try:
            guid = uuid.UUID(row["guid"])
            if not guid.int or guid.hex in seen:
                raise ValueError("invalid or repeated variable GUID")
            seen.add(guid.hex)
        except (KeyError, ValueError, TypeError) as error:
            raise OperationError("duplicate_observation_failed", "Declared variable identity is invalid") from error
        if not row["default_value_available"] or not row["default_value_current_compile"]:
            raise OperationError("duplicate_observation_unsupported", "Every declared variable needs a readable current generated CDO value in this clone slice")
    observation = {"blueprint": basic, "asset_class": asset.get_class().get_path_name(),
            "compiled": asset.get_editor_property("status") in (unreal.BlueprintStatus.BS_UP_TO_DATE, unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS),
            "graphs": graphs, "variables": variable_state, "file": file_state(basic["path"])}
    if basic["parent_class"].startswith("/Game/"):
        observation["generated_parent"] = blueprint.generated_parent_observation(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset))
    return observation


def require_source(asset, observation, allow_dirty_source):
    basic = observation["blueprint"]
    if (observation["asset_class"] != "/Script/Engine.Blueprint"
            or not basic["parent_class"].startswith("/Script/") and not observation.get("generated_parent")):
        raise OperationError("duplicate_source_type_unsupported", "This slice supports ordinary UBlueprint assets with a native runtime parent or an independently inspected saved clean Actor Blueprint parent chain")
    if basic["dirty"] and not allow_dirty_source:
        raise OperationError("duplicate_source_dirty", "Source has unsaved changes and allow_dirty_source is false; source was not saved")
    if not observation["file"]["exists"] or not basic["generated_class"]:
        raise OperationError("duplicate_source_not_saved", "Source requires an existing .uasset and generated class")
    if not observation["compiled"]:
        raise OperationError("duplicate_source_not_compiled", "Source must already be compiled and saved; no source compile is performed")


def _relative_reference(value, basic):
    """Only call on actual identity/reference fields, never arbitrary user text."""
    name = basic["name"]
    package = basic["path"]
    for object_name, marker in ((name, "$ASSET"), (name + "_C", "$CLASS"),
                                ("SKEL_" + name + "_C", "$SKELETON"), ("Default__" + name + "_C", "$CDO")):
        prefix = package + "." + object_name
        if value == prefix or value.startswith(prefix + ":"):
            return marker + value[len(prefix):]
    return "$PACKAGE" if value == package else value


def _relative_type_text(value, basic):
    # ExportText object references are quoted. Normalize only complete quoted
    # references, not occurrences inside unrelated names or user string values.
    return re.sub(r'([\'\"])(/[^\'\"]+)\1',
                  lambda match: match[1] + _relative_reference(match[2], basic) + match[1], value)


def _member_identity(node, basic, declarations):
    """Match regenerated member GUIDs to their actual local declaration.

    External/inherited/SCS references keep their observed GUID. A local declared
    member must agree by resolved owner/name, serialized owner and declaration
    GUID before its identity can be compared across independently named clones.
    """
    try:
        guid = uuid.UUID(node["variable_member_guid"])
    except (TypeError, ValueError, AttributeError) as error:
        raise OperationError("duplicate_member_identity_invalid", "Observed member reference GUID is malformed") from error
    name = node.get("variable_name")
    property_path = _relative_reference(node.get("variable_property_path", ""), basic)
    if name in declarations and property_path in ("$CLASS:" + name, "$SKELETON:" + name):
        declaration = declarations[name]
        parent = _relative_reference(node.get("variable_member_parent_path", ""), basic)
        if not guid.int or guid.hex != declaration or parent not in ("", "$CLASS", "$SKELETON"):
            raise OperationError("duplicate_member_identity_invalid", "Local member reference does not match its declared GUID and owning class")
        return {"local_declared_variable": name}
    return guid.hex


def _custom_event_type_text(node, pin, basic):
    """Normalize only a proven own CustomEvent delegate's regenerated GUID.

    UpdateDelegatePin fills this reflected member reference from a function.
    PostDuplicate regenerates the event node GUID. Its equality to that member
    GUID, an independently read CustomFunctionName and a loaded own UFunction
    are all required. Other delegates/references remain literal.
    """
    value = pin["native_type_export"]
    normalized = _relative_type_text(value, basic)
    signature = pin.get("custom_event_signature")
    if (node.get("class") != "K2Node_CustomEvent" or pin.get("name") != "OutputDelegate"
            or pin.get("direction") != "output" or type(signature) is not dict
            or signature.get("valid") is not True
            or signature.get("node_class_path") != "/Script/BlueprintGraph.K2Node_CustomEvent"
            or signature.get("function_class_path") != "/Script/CoreUObject.Function"):
        return normalized
    name = node.get("custom_function_name")
    owner = _relative_reference(signature.get("function_owner_path", ""), basic)
    parent = _relative_reference(signature.get("member_parent_path", ""), basic)
    if owner not in ("$CLASS", "$SKELETON") or parent != owner:
        return normalized
    if (type(name) is not str or re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) is None
            or signature.get("member_name") != name
            or _relative_reference(signature.get("function_path", ""), basic) != owner + ":" + name
            or not value.startswith('(PinCategory="delegate",')):
        raise OperationError("duplicate_custom_event_identity_invalid", "Own custom event delegate does not match its declared function name, type or resolved owner")
    # Retain every other export field and every other GUID. The serialized
    # parent must exactly agree with the independently observed native object.
    references = list(re.finditer(r'PinSubCategoryMemberReference=\(MemberParent=(None|"[^\"]*"),MemberName="([A-Za-z_][A-Za-z0-9_]*)",MemberGuid=([0-9A-Fa-f]{32})\)', value))
    if len(references) != 1:
        raise OperationError("duplicate_custom_event_identity_invalid", "Own custom event delegate requires one exact native member reference")
    reference = references[0]
    expected_parent = '"' + signature["member_parent_class_path"] + "'" + signature["member_parent_path"] + "'\""
    try:
        node_guid, member_guid = uuid.UUID(node["node_guid"]), uuid.UUID(reference[3])
        if (reference[1] != expected_parent or reference[2] != name
                or not node_guid.int or member_guid != node_guid):
            raise ValueError("custom event member/node mismatch")
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        raise OperationError("duplicate_custom_event_identity_invalid", "Own custom event delegate GUID or serialized owner disagrees with its observed node/function") from error
    result = value[:reference.start(3)] + "$LOCAL_CUSTOM_EVENT_GUID" + value[reference.end(3):]
    return _relative_type_text(result, basic)


def _ai_parent_call_provenance(observation, node):
    """Bound a super-call display-owner alias to the existing saved parent proof.

    This is not a general function-owner normalization. The compiler resolves
    exact CallParentFunction member names from the direct parent independently
    of GetTargetFunction's generated/skeleton display result.
    """
    generated = observation.get("generated_parent")
    inspection = generated.get("inspection") if type(generated) is dict else None
    native = inspection.get("native_ai_controller_parent") if type(inspection) is dict else None
    if type(native) is not dict:
        return None  # Keep every pre-existing non-AI parent comparison literal.
    basic = observation["blueprint"]
    parent, chain = basic["parent_class"], basic["parent_chain"]
    rows, files = inspection.get("generated_parents"), generated.get("parents")
    if (native.get("class_path") != "/Script/AIModule.AIController"
            or native.get("cdo_path") != "/Script/AIModule.Default__AIController"
            or native.get("root_and_cdo_identity_unchanged") is not True
            or native.get("saved_aimodule_dependency_observed") is not True
            or inspection.get("success") is not True or inspection.get("observed_state_unchanged") is not True
            or inspection.get("class_path") != parent or inspection.get("parent_chain") != chain
            or type(chain) is not list or not chain or chain[0] != parent
            or "/Script/AIModule.AIController" not in chain
            or type(rows) is not list or not rows or type(files) is not list or len(rows) != len(files)
            or rows[0].get("class_path") != parent
            or inspection.get("dependency_audit", {}).get("passed") is not True):
        raise OperationError("duplicate_parent_call_provenance_invalid", "Super-call alias requires the exact unchanged saved AIController generated parent chain")
    for row, source in zip(rows, files):
        if (row.get("class_path") != row.get("generated_class") or row.get("class_path") not in chain
                or row.get("dirty") is not False or row.get("saved_asset") is not True
                or source.get("class_path") != row["class_path"] or source.get("loaded") is not True
                or source.get("dirty") is not False or source.get("file", {}).get("exists") is not True
                or re.fullmatch(r"[0-9a-f]{64}", source.get("file", {}).get("sha256", "")) is None):
            raise OperationError("duplicate_parent_call_provenance_invalid", "Super-call alias requires every observed saved/loaded clean generated parent source")
    direct = rows[0]
    asset = direct["blueprint_path"]
    package, separator, name = asset.partition(".")
    if (not separator or direct.get("package_name") != package
            or direct["generated_class"] != asset + "_C" or parent != asset + "_C"):
        raise OperationError("duplicate_parent_call_provenance_invalid", "Direct parent asset and authoritative generated-class identity disagree")
    preserved = json.loads(json.dumps(generated))
    # Same single timing exception as operations.blueprint.parent_comparison_state.
    preserved["inspection"]["dependency_audit"].pop("elapsed_ms", None)
    proof = node.get("parent_call_proof")
    if (node.get("node_class_path") != "/Script/BlueprintGraph.K2Node_CallParentFunction"
            or type(proof) is not dict or proof.get("valid") is not True
            or proof.get("parent_package_dirty") is not False):
        raise OperationError("duplicate_parent_call_provenance_missing", "A fresh exact native CallParentFunction reference/current-parent-role proof is required; historical snapshots are not supplemented")
    parent_bp, current_gen, current_skel = (proof.get(key) for key in
            ("direct_parent_blueprint", "current_generated_class", "current_skeleton_class"))
    for identity, expected_class in ((parent_bp, "/Script/Engine.Blueprint"),
            (current_gen, "/Script/Engine.BlueprintGeneratedClass"), (current_skel, "/Script/Engine.BlueprintGeneratedClass")):
        if (type(identity) is not dict or identity.get("class_path") != expected_class
                or identity.get("outer_path") != package or identity.get("package_path") != package
                or type(identity.get("path")) is not str or not identity["path"].startswith(package + ".")
                or re.fullmatch(r"[0-9]+", identity.get("flags_string", "")) is None):
            raise OperationError("duplicate_parent_call_provenance_invalid", "Observed current parent classes must have the exact saved Blueprint owner/package roles")
    if (parent_bp["path"] != asset or current_gen["path"] != parent or current_skel["path"] == parent):
        raise OperationError("duplicate_parent_call_provenance_invalid", "Actual current generated/skeleton roles disagree with the authoritative direct parent")
    reference = proof.get("function_reference")
    reference_keys = {"member_name", "member_scope", "member_guid", "member_parent_class", "is_self_context", "is_local_scope", "is_deprecated"}
    if (type(reference) is not dict or set(reference) != reference_keys
            or reference.get("member_name") != node.get("function_name")
            or reference.get("member_scope") != "" or reference.get("is_local_scope") is not False
            or type(reference.get("is_self_context")) is not bool or type(reference.get("is_deprecated")) is not bool
            or type(reference.get("member_parent_class")) is not str
            or reference["member_parent_class"] not in (chain + [current_skel["path"], ""])
            or (not reference["member_parent_class"] and reference["is_self_context"] is not True)
            or re.fullmatch(r"[0-9A-Fa-f]{32}", reference.get("member_guid", "")) is None):
        raise OperationError("duplicate_parent_call_reference_invalid", "The serialized member name/parent/GUID/self/local/deprecation reference must be observed and remain literal")
    return {"direct_parent": parent, "skeleton_owner": current_skel["path"],
            "native_ancestors": [p for p in chain if p.startswith("/Script/")],
            "current_parent_roles": {key: proof[key] for key in
                ("direct_parent_blueprint", "current_generated_class", "current_skeleton_class", "parent_package_dirty")},
            "saved_parent_observation": preserved}


def _ai_parent_call_reference_identity(node, parent_proof):
    """Compare only SDK-proven Actor-event -> direct-parent declaration remaps.

    ConformCallsToParentFunctions chooses the actual current parent skeleton
    implementation; SetFromFunction records its authoritative owner and GUID.
    This is not a general GUID omission, name match, or function equivalence.
    """
    proof = node["parent_call_proof"]
    reference = proof["function_reference"]
    declaration = proof.get("parent_function_declaration")
    if type(declaration) is not dict or declaration.get("valid") is not True:
        # Old observations cannot prove a remap. Keep the full reference literal;
        # their zero/native vs declared/generated difference still fails.
        return reference
    name = node["function_name"]
    parent, skeleton = parent_proof["direct_parent"], parent_proof["skeleton_owner"]
    bp = proof["direct_parent_blueprint"]
    package, asset = bp["package_path"], bp["path"]
    guid = declaration.get("declaration_guid")
    function_kind = declaration.get("generated_function_kind")
    if function_kind not in (None, "current_generated_override", "inherited_native_actor_for_disabled_event"):
        raise OperationError("duplicate_parent_call_declaration_invalid", "Unrecognized generated function provenance cannot prove a parent remap")
    inherited = function_kind == "inherited_native_actor_for_disabled_event"
    kind = ("direct_parent_disabled_native_actor_event_declaration" if inherited
            else "direct_parent_native_actor_event_override")
    if (declaration.get("declaration_kind") != kind
            or declaration.get("member_name") != name
            or re.fullmatch(r"[0-9A-Fa-f]{32}", guid or "") is None or not uuid.UUID(guid).int
            or declaration.get("generated_lookup_guid") != guid or declaration.get("skeleton_lookup_guid") != guid
            or declaration.get("native_signature_compatible") is not True
            or declaration.get("native_owner_class") != "/Script/Engine.Actor"
            or declaration.get("generated_owner_class") != ("/Script/Engine.Actor" if inherited else parent)
            or declaration.get("skeleton_owner_class") != skeleton
            or declaration.get("generated_super_function") != ("" if inherited else "/Script/Engine.Actor:" + name)
            or declaration.get("skeleton_super_function") != "/Script/Engine.Actor:" + name
            or (inherited and (declaration.get("event_disabled") is not True
                or declaration.get("event_automatically_placed_ghost") is not True
                or declaration.get("event_is_override") is not True
                or declaration.get("declaration_graph_nontransient") is not True
                or declaration.get("generated_own_function_returned_nonnull") is not False
                or declaration.get("disabled_inherited_native_role") is not True))):
        raise OperationError("duplicate_parent_call_declaration_invalid", "Parent reference remap requires the same actual declaration GUID, current skeleton override and exact generated override or disabled inherited-native role")
    def owned(identity, cls, outer, path=None):
        return (type(identity) is dict and identity.get("class_path") == cls
                and identity.get("outer_path") == outer and identity.get("package_path") == package
                and type(identity.get("path")) is str
                and (path is None or identity["path"] == path)
                and re.fullmatch(r"[0-9]+", identity.get("flags_string", "")) is not None)
    graph_id, event_id = declaration.get("declaration_graph"), declaration.get("declaration_node")
    if (not owned(graph_id, "/Script/Engine.EdGraph", asset)
            or not graph_id["path"].startswith(asset + ":")
            or not owned(event_id, "/Script/BlueprintGraph.K2Node_Event", graph_id["path"])
            or not event_id["path"].startswith(graph_id["path"] + ".")
            or re.fullmatch(r"[0-9A-Fa-f]{32}", declaration.get("declaration_graph_guid", "")) is None
            or not uuid.UUID(declaration["declaration_graph_guid"]).int
            or (not inherited and not owned(declaration.get("generated_function"), "/Script/CoreUObject.Function", parent, parent + ":" + name))
            or not owned(declaration.get("skeleton_function"), "/Script/CoreUObject.Function", skeleton, skeleton + ":" + name)):
        raise OperationError("duplicate_parent_call_declaration_invalid", "Own parent event graph/node and both current function identities must have exact observed ownership")
    native = declaration.get("native_function")
    if (type(native) is not dict or native.get("path") != "/Script/Engine.Actor:" + name
            or native.get("class_path") != "/Script/CoreUObject.Function"
            or native.get("outer_path") != "/Script/Engine.Actor" or native.get("package_path") != "/Script/Engine"
            or re.fullmatch(r"[0-9]+", native.get("flags_string", "")) is None):
        raise OperationError("duplicate_parent_call_declaration_invalid", "The declaration must override the exact loaded native Actor function")
    if inherited and declaration.get("generated_function") != native:
        raise OperationError("duplicate_parent_call_declaration_invalid", "Disabled own event requires the exact observed native Actor function identity as the inherited generated lookup")
    native_reference = dict(member_name=name, member_scope="", member_guid="0" * 32,
                            member_parent_class="/Script/Engine.Actor", is_self_context=False,
                            is_local_scope=False, is_deprecated=False)
    if declaration.get("event_reference") != native_reference:
        raise OperationError("duplicate_parent_call_declaration_invalid", "The actual parent Event declaration must retain its exact native Actor reference")
    declared_reference = dict(native_reference, member_guid=guid, member_parent_class=parent)
    if reference not in (native_reference, declared_reference):
        raise OperationError("duplicate_parent_call_reference_remap_invalid", "Only native Actor/zero or the same directly observed parent declaration GUID/authoritative owner can correspond")
    # Copy only the comparison projection. Raw captured reference and declaration
    # proof remain unmodified in source_before/target_after and failure receipts.
    return declared_reference


def semantic_content(observation):
    """GUIDs are regenerated by PostDuplicate; compare contents/topology instead."""
    basic = observation["blueprint"]
    declarations = {}
    for variable in observation["variables"]["new_variables"]:
        try:
            name, guid = variable["name"], uuid.UUID(variable["guid"])
            if not guid.int or name in declarations or guid.hex in declarations.values():
                raise ValueError("invalid or duplicate declaration identity")
            declarations[name] = guid.hex
        except (KeyError, TypeError, ValueError, AttributeError) as error:
            raise OperationError("duplicate_member_identity_invalid", "Local variable declarations require unique names and nonzero GUIDs") from error
    graphs = []
    parent_call_proof = None
    for value in observation["graphs"]:
        nodes = []
        for node in value["nodes"]:
            row = {key: item for key, item in node.items() if key not in ("node_guid", "title")}
            if (node.get("class") == "K2Node_CallParentFunction"
                    and node.get("function_name") in ("ReceiveBeginPlay", "ReceiveTick")):
                parent_call_proof = _ai_parent_call_provenance(observation, node)
                if parent_call_proof is not None:
                    name, owner = node.get("function_name"), node.get("function_owner_path")
                    owners = ["/Script/Engine.Actor", parent_call_proof["direct_parent"], parent_call_proof["skeleton_owner"]]
                    if ("/Script/Engine.Actor" not in parent_call_proof["native_ancestors"] or owner not in owners
                            or node.get("function_path") != owner + ":" + name):
                        raise OperationError("duplicate_parent_call_reference_invalid", "Observed Actor event super-call name/resolved path must match Actor or the exact direct parent generated/skeleton owner")
                    # Current parent/declaration proof, all pins/types/defaults/
                    # links and every other reference field remain literal. A
                    # proven Conform parent/GUID remap has its own narrow check.
                    row["function_owner_path"] = parent_call_proof["direct_parent"]
                    row["function_path"] = parent_call_proof["direct_parent"] + ":" + name
                    row["parent_call_proof"] = dict(node["parent_call_proof"],
                        function_reference=_ai_parent_call_reference_identity(node, parent_call_proof))
            if "variable_member_guid" in row:
                row["variable_member_guid"] = _member_identity(node, basic, declarations)
            for key in ("node_id", "variable_property_path", "variable_member_parent_path", "event_member_parent_path", "function_path", "function_owner_path"):
                if key in row:
                    row[key] = _relative_reference(row[key], basic)
            pins = []
            for pin in node["pins"]:
                item = {key: value for key, value in pin.items() if key != "type_display"}
                item["native_type_export"] = _custom_event_type_text(node, pin, basic)
                if "custom_event_signature" in item:
                    signature = dict(item["custom_event_signature"])
                    for key in ("member_parent_path", "function_path", "function_owner_path"):
                        signature[key] = _relative_reference(signature[key], basic)
                    item["custom_event_signature"] = signature
                item["links"] = sorted((dict(link, node_id=_relative_reference(link["node_id"], basic))
                                        for link in pin["links"]), key=lambda x: (x["node_id"], x["pin_name"]))
                pins.append(item)
            row["pins"] = sorted(pins, key=lambda x: (x["name"], x["direction"]))
            nodes.append(row)
        graphs.append({"graph_name": value["graph_name"], "node_count": value["node_count"],
                       "total_node_count": value["total_node_count"], "nodes": sorted(nodes, key=lambda x: x["node_id"])})
    variables_by_scope = {}
    for scope in ("new_variables", "generated_variables"):
        rows = []
        for row in observation["variables"][scope]:
            item = {key: value for key, value in row.items() if key != "guid"}
            pin_type = dict(item["pin_type"])
            if type(pin_type.get("type_object")) is str:
                pin_type["type_object"] = _relative_reference(pin_type["type_object"], basic)
            item["pin_type"] = pin_type
            # String/name/text defaults are compared literally. Complex nested
            # self references are not guessed and can fail this bounded check.
            if pin_type.get("category") in ("object", "class") and type(item.get("default_value")) is str:
                item["default_value"] = _relative_reference(item["default_value"], basic)
            rows.append(item)
        variables_by_scope[scope] = rows
    content = {"asset_class": observation["asset_class"], "parent_class": basic["parent_class"],
        "parent_chain": basic["parent_chain"], "graphs": sorted(graphs, key=lambda x: x["graph_name"]),
        "variables": variables_by_scope, "category_sorting": observation["variables"]["category_sorting"]}
    if parent_call_proof is not None:
        content["parent_call_provenance"] = parent_call_proof
    return content


def duplicate(source, destination, before_dispatch, after_return, replacement=None, source_work=None, dispatch_started=None):
    blueprint.ensure_editor_idle()
    policy()
    require_target_absent(destination, replacement) if replacement is not None else require_target_absent(destination)
    before_dispatch()
    if replacement is not None:
        replacement_current(replacement, target=False)
        if source != replacement["source_role"]["object"]:
            raise OperationError("duplicate_replacement_source_changed", "Private replacement source object differs; no Duplicate dispatch")
    # Check after the journal callback and immediately before the real SDK call;
    # it must duplicate the admitted RAM object, never silently reload/save it.
    if source_work is not None:
        dirty_source_current(source_work, full=True)
        if source != source_work["source_role"]["object"]:
            raise OperationError("duplicate_dirty_source_identity_changed", "Dirty-source role belongs to another object; no duplicate dispatched")
    if dispatch_started is not None:
        dispatch_started()  # Pure receipt update, after guards and immediately before SDK.
    value = (replacement["assets"] if replacement is not None else source_work["assets"] if source_work is not None else blueprint.asset_subsystem()).duplicate_loaded_asset(source, destination)
    after_return(value)  # Publish the real returned mutation prefix before any new observer can fail.
    if source_work is not None:
        dirty_source_current(source_work, full=True)
    if replacement is not None and value is not None:
        replacement_clone_current(replacement, value)
    return value


def save(asset):
    return bool(blueprint.asset_subsystem().save_loaded_asset(asset, False))


# UI: one operation-specific destructive replacement seam. Old handles are never
# dereferenced after retirement, including when the public force-delete returns False.
def _replacement_live(value):
    return value is not None and unreal.SystemLibrary.is_valid(value)


def _replacement_role(value, path):
    if not _replacement_live(value) or value.get_class() != unreal.Blueprint.static_class():
        raise OperationError("duplicate_replacement_role_pending", "Replacement requires an actual loaded ordinary Blueprint; wider original asset classes remain incomplete")
    outer = value.get_outer()
    package = value.get_outermost()
    if not _replacement_live(outer) or not _replacement_live(package) or outer != package or value.get_path_name() != path:
        raise OperationError("duplicate_replacement_role_changed", "Blueprint root/owner identity differs before replacement")
    generated = unreal.BlueprintEditorLibrary.generated_class(value)
    cdo_path = path.partition(".")[0] + ".Default__" + path.rsplit(".", 1)[-1] + "_C"
    cdo = unreal.find_object(None, cdo_path)
    if (not _replacement_live(generated) or not _replacement_live(cdo)
            or cdo.get_class() != generated or cdo.get_outermost() != package):
        raise OperationError("duplicate_replacement_role_pending", "Existing generated class/CDO roles required; no default object is created")
    return {"object": value, "class": value.get_class(), "outer": outer, "package": package,
            "path": path, "generated": generated, "cdo": cdo, "cdo_path": cdo_path}


def replacement_current(work, target=True):
    blueprint.ensure_editor_idle()
    if (not _replacement_live(work["editor"]) or not _replacement_live(work["assets"]) or not _replacement_live(work["world"])
            or unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem) != work["editor"]
            or unreal.get_editor_subsystem(unreal.EditorAssetSubsystem) != work["assets"]
            or work["editor"].get_editor_world() != work["world"]):
        raise OperationError("duplicate_replacement_context_changed", "Original Editor world changed; replacement prefix retained without another writer")
    roles = [work["source_role"]]
    if target and work["target_role"] is not None:
        if work["retired"]:
            raise OperationError("duplicate_replacement_retired_observation", "Old target is retired; no old-target getter is permitted")
        roles.append(work["target_role"])
    for role in roles:
        value = role["object"]
        if (not _replacement_live(value) or not _replacement_live(role["class"])
                or not _replacement_live(role["outer"]) or not _replacement_live(role["package"])
                or value.get_class() != role["class"] or value.get_outer() != role["outer"]
                or value.get_outermost() != role["package"] or value.get_path_name() != role["path"]
                or unreal.find_object(None, role["path"]) != value
                or not _replacement_live(role["generated"]) or not _replacement_live(role["cdo"])
                or unreal.BlueprintEditorLibrary.generated_class(value) != role["generated"]
                or unreal.find_object(None, role["cdo_path"]) != role["cdo"]
                or role["cdo"].get_class() != role["generated"] or role["cdo"].get_outermost() != role["package"]):
            raise OperationError("duplicate_replacement_identity_changed", "Captured source/target identity changed; no fresh baseline or automatic retry")
    return True


def _dirty_source_memory(observation):
    # Same concrete observations already used by clone/source verification; keep
    # source identities literal, without clone-relative or compiler normalizing.
    return {key: copy.deepcopy(observation[key]) for key in
            ("blueprint", "asset_class", "compiled", "graphs", "variables", "file")}


def prepare_dirty_source(source, observation):
    if observation["blueprint"]["dirty"] is not True:
        return None  # Preserve the existing clean-source route and its costs.
    if (observation["compiled"] is not True or observation["file"]["exists"] is not True
            or not observation["file"].get("sha256")):
        raise OperationError("duplicate_dirty_source_scope_pending", "Dirty support requires an existing saved file and current compiled/CDO observation; source compilation or first save was not performed")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    work = {"editor": editor, "world": editor.get_editor_world(), "assets": blueprint.asset_subsystem(),
            "source_role": _replacement_role(source, observation["blueprint"]["asset_path"]),
            "target_role": None, "graphs": [], "expected_memory": _dirty_source_memory(observation),
            "receipt": {"admitted": True, "source_was_dirty": True, "source_compile_requested": False,
                "source_save_requested": False, "duplicate_input": "held_current_RAM_SourceObject",
                "existing_saved_file_required": True, "current_compiled_CDO_required": True,
                "role_checks": 0, "memory_checks": 0, "latest_check_passed": False,
                "full_RAM_authored_state_proof": False,
                "scope": "Held source/generated/CDO and current K2 graph/node identities; literal existing K2/variable/CDO DTO, dirty/status and source file bytes. Authored Namespace is read literally on source and clone; DuplicateTransient DisplayName/Description are preserved on source only. Component template properties, comments, other unobserved metadata, arbitrary callback side effects, uncompiled and first-save source variants remain incomplete."}}
    replacement_current(work, target=False)
    # Blueprint.h declares Namespace without DuplicateTransient; DisplayName and
    # Description have that flag and must not be required equal on the clone.
    work["authored"] = {name: str(source.get_editor_property(name)) for name in
                        ("blueprint_namespace", "blueprint_display_name", "blueprint_description")}
    work["namespace"] = work["authored"]["blueprint_namespace"]
    replacement_current(work, target=False)
    work["receipt"]["source_namespace"] = work["namespace"]
    for row in observation["graphs"]:
        editor_graph = graph.graph_editor(observation["blueprint"]["asset_path"], row["graph_name"])
        replacement_current(work, target=False)
        actual = editor_graph.get_graph()
        if (not _replacement_live(actual) or actual.get_path_name() != row["graph_path"]
                or actual.get_outermost() != work["source_role"]["package"]
                or unreal.find_object(None, row["graph_path"]) != actual):
            raise OperationError("duplicate_dirty_graph_role_pending", "Actual source graph does not match its earlier observed object role")
        item = {"editor": editor_graph, "object": actual, "class": actual.get_class(),
                "outer": actual.get_outer(), "path": row["graph_path"], "nodes": []}
        for node in row["nodes"]:
            value = unreal.find_object(None, node["node_id"])
            if (not _replacement_live(value) or value.get_outer() != actual
                    or value.get_outermost() != work["source_role"]["package"]):
                raise OperationError("duplicate_dirty_node_role_pending", "Earlier observed K2 node requires the same current graph/owner role")
            item["nodes"].append({"object": value, "class": value.get_class(), "path": node["node_id"]})
        work["graphs"].append(item)
    dirty_source_current(work, full=True)
    return work


def dirty_source_current(work, *, full=False):
    if work is None:
        return
    receipt = work["receipt"]
    receipt["latest_check_passed"] = False
    replacement_current(work, target=False)
    package = work["source_role"]["package"]
    for row in work["graphs"]:
        value = row["object"]
        if (not _replacement_live(value) or not _replacement_live(row["class"])
                or not _replacement_live(row["outer"]) or value.get_class() != row["class"]
                or value.get_outer() != row["outer"] or value.get_outermost() != package
                or value.get_path_name() != row["path"] or unreal.find_object(None, row["path"]) != value
                or row["editor"].get_graph() != value):
            raise OperationError("duplicate_dirty_graph_identity_changed", "Captured source graph/class/outer role changed; no next writer or new baseline")
        for node in row["nodes"]:
            value = node["object"]
            if (not _replacement_live(value) or not _replacement_live(node["class"])
                    or value.get_class() != node["class"] or value.get_outer() != row["object"]
                    or value.get_outermost() != package or value.get_path_name() != node["path"]
                    or unreal.find_object(None, node["path"]) != value):
                raise OperationError("duplicate_dirty_node_identity_changed", "Captured source K2 node/class/graph role changed; no next writer or new baseline")
    replacement_current(work, target=False)
    for name, expected in work["authored"].items():
        actual_value = str(work["source_role"]["object"].get_editor_property(name))
        replacement_current(work, target=False)
        if actual_value != expected:
            receipt["first_memory_mismatch"] = {"property_group": name,
                "expected": expected, "actual": actual_value}
            raise OperationError("duplicate_dirty_source_changed", "Original dirty-source authored metadata changed; no next writer or fresh baseline")
    receipt["role_checks"] += 1
    actual = None
    if full:
        actual = capture(work["source_role"]["object"])
        # SDK-backed DTO collection can invoke engine code; close the held
        # graph/node/source roles again before accepting that collection.
        dirty_source_current(work, full=False)
        observed = _dirty_source_memory(actual)
        receipt["memory_checks"] += 1
        if observed != work["expected_memory"]:
            first = next(key for key in work["expected_memory"] if observed[key] != work["expected_memory"][key])
            receipt["first_memory_mismatch"] = {"property_group": first,
                "expected": copy.deepcopy(work["expected_memory"][first]), "actual": copy.deepcopy(observed[first])}
            raise OperationError("duplicate_dirty_source_changed", "Original dirty-source RAM/dirty/status or disk state changed; actual prefix retained without another writer or automatic retry")
        if work.get("dependency_guard_ready"):
            dependency_audit(work["expected_memory"]["blueprint"]["path"], work)
            # Close source roles again after Native collector/registry callbacks.
            dirty_source_current(work, full=False)
    receipt["latest_check_passed"] = True
    return actual


def replacement_dirty(work):
    replacement_current(work, target=not work["retired"])
    values = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
              + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    replacement_current(work, target=not work["retired"])
    if len(values) > 4096:
        raise OperationError("duplicate_replacement_dirty_scope_pending", "Dirty inventory exceeds the existing finite package-observation bound")
    paths = []
    for value in values:
        if not _replacement_live(value):
            raise OperationError("duplicate_replacement_dirty_changed", "Dirty inventory returned an unavailable package")
        paths.append(value.get_path_name())
    return sorted(set(paths))


def replacement_references(work):
    replacement_current(work)
    rows = work["assets"].find_package_referencers_for_asset(work["target_package"], False)
    replacement_current(work)
    if len(rows) > 4096 or any(type(row) is not str for row in rows):
        raise OperationError("duplicate_replacement_references_pending", "Bounded package-reference result is unavailable")
    # Internal package references are not external dependants. This public query
    # is explicitly NOT a complete memory/editor/Undo reference audit.
    return sorted({row for row in rows if row.casefold() != work["target_package"].casefold()})


def replacement_instances(work):
    replacement_current(work)
    generated = work["target_role"]["generated"]
    replacement_current(work)
    if not _replacement_live(generated):
        raise OperationError("duplicate_replacement_instances_pending", "Existing compiled target class is required before destructive replacement")
    cdo_path = work["target_package"] + ".Default__" + work["target_package"].rsplit("/", 1)[-1] + "_C"
    cdo = work["target_role"]["cdo"]
    if not _replacement_live(cdo) or unreal.find_object(None, cdo_path) != cdo or cdo.get_class() != generated or cdo.get_outermost() != work["target_role"]["package"]:
        raise OperationError("duplicate_replacement_instances_pending", "Existing generated CDO role is unavailable; no CDO is created")
    result = []
    for value in unreal.ObjectIterator(generated):
        replacement_current(work)
        if not _replacement_live(value):
            raise OperationError("duplicate_replacement_instances_changed", "Generated-instance iterator returned an unavailable object")
        if value != cdo:
            result.append(value.get_path_name())
            break  # Any live instance/derived CDO is a valid but unsupported deletion closure.
    replacement_current(work)
    return result


def prepare_replacement(source, initial_source, target_package, inspect_target):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    assets = blueprint.asset_subsystem()
    work = {"editor": editor, "world": world, "assets": assets, "initial_source": initial_source,
            "source_role": _replacement_role(source, initial_source["blueprint"]["asset_path"]),
            "target_role": None, "target_package": target_package, "retired": False}
    replacement_current(work, target=False)
    work["target_before"] = target_state(target_package)
    replacement_current(work, target=False)
    work["exists"] = work["target_before"]["asset_registered"]
    if not work["exists"]:
        require_target_absent(target_package)
        replacement_current(work, target=False)
        return work
    if not inspect_target:
        return work  # Original dry run does not execute destructive admission/SDK deletion.
    target_object = blueprint.resolve_blueprint(target_package)
    replacement_current(work, target=False)  # A target load must not retire/change the earlier source role.
    path = target_package + "." + target_package.rsplit("/", 1)[-1]
    work["target_role"] = _replacement_role(target_object, path)
    work["target_observation"] = capture(target_object)
    replacement_current(work)
    require_source(target_object, work["target_observation"], False)
    work["references"] = replacement_references(work)
    if work["references"]:
        raise OperationError("duplicate_replacement_referenced_pending", "Known external package dependants require a wider original force-replacement scope; no asset was deleted")
    work["instances"] = replacement_instances(work)
    if work["instances"]:
        raise OperationError("duplicate_replacement_live_instances_pending", "Live target instances or derived CDOs require a wider original deletion closure; no asset was deleted")
    work["dirty_before"] = replacement_dirty(work)
    return work


def delete_replacement_target(work, before_dispatch, dispatch_started, after_return):
    blueprint.ensure_editor_idle()
    policy()
    replacement_current(work)
    if replacement_references(work) != work["references"] or replacement_instances(work) != work["instances"]:
        raise OperationError("duplicate_replacement_references_changed", "Target references/instances changed before DeleteAsset; no delete dispatched")
    if replacement_dirty(work) != work["dirty_before"]:
        raise OperationError("duplicate_replacement_dirty_changed", "Dirty package inventory changed before DeleteAsset; no delete dispatched")
    before_dispatch()
    replacement_current(work)
    # Capture/checkpoint callbacks must not create a new deletion closure after
    # the first admission. Recheck the ORIGINAL rows, never adopt a new baseline.
    if replacement_references(work) != work["references"] or replacement_instances(work) != work["instances"]:
        raise OperationError("duplicate_replacement_references_changed", "Target references/instances changed during pre-dispatch observation; no delete dispatched")
    if replacement_dirty(work) != work["dirty_before"]:
        raise OperationError("duplicate_replacement_dirty_changed", "Dirty packages changed during pre-dispatch observation; no delete dispatched")
    replacement_current(work)
    # Copied JSON survives; all selected root/package handles are retired before
    # ForceDelete, whose callbacks/GC may invalidate even a returned-False prefix.
    work["retired"] = True
    work["target_role"] = None
    dirty_source_current(work.get("dirty_source_work"), full=True)
    dispatch_started()
    deleted = work["assets"].delete_asset(work["target_package"])
    after_return(deleted)
    if deleted is not True:
        return deleted  # Unknown terminal: do not even observe source after a False/invalid return.
    replacement_current(work, target=False)
    return deleted


def replacement_absent(work):
    replacement_current(work, target=False)
    value = target_state(work["target_package"])
    replacement_current(work, target=False)
    object_path = work["target_package"] + "." + work["target_package"].rsplit("/", 1)[-1]
    fresh = unreal.find_object(None, object_path)
    replacement_current(work, target=False)
    if value["asset_registered"] or value["directory_exists"] or value["file"]["exists"] or _replacement_live(fresh):
        raise OperationError("duplicate_replacement_delete_unconfirmed", "DeleteAsset returned True but fresh target path/registry/file is not absent; no duplicate dispatched")
    # ForceDelete can leave loaded package residue. Original DuplicateAsset may
    # reuse it; internal exports/memory refs are not proved absent. No retired getter.
    value["retired_loaded_package_residue_allowed"] = value["loaded_package_or_object"]
    return value


def replacement_clone_current(work, clone):
    replacement_current(work, target=False)
    path = work["target_package"] + "." + work["target_package"].rsplit("/", 1)[-1]
    if (not _replacement_live(clone) or clone.get_class() != unreal.Blueprint.static_class()
            or clone.get_path_name() != path or unreal.find_object(None, path) != clone
            or not _replacement_live(clone.get_outermost()) or clone.get_outer() != clone.get_outermost()
            or clone.get_outermost().get_path_name() != work["target_package"]):
        raise OperationError("duplicate_replacement_clone_changed", "Returned/current clone root is unavailable or differs; actual prefix retained")
    # Root identity is stable; legitimate implicit/explicit compilation may
    # rebuild its Generated/Skeleton/CDO roles. Existing fresh readback owns them.
    return True
