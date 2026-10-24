"""UE 5.8.3 Blueprint APIs; keep version-specific calls in this module."""

import json
import hashlib
from pathlib import Path
import re
import unreal

from cubeless_mcp_ue58.operations.state import OperationError
from cubeless_mcp_ue58.validators.dependencies import parent_chain, reject_tool_reference

_INVALID_PACKAGE_CHARACTERS = set('\\:*?"<>|\' ,.&!~\n\r\t@#(){}[]=;^%$`')


def ensure_editor_idle():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem.get_game_world() is not None:
        raise OperationError("play_session_active", "End PIE/SIE before mutating Blueprint assets")


def directory_path(package_path):
    path = (package_path or "/Game/Blueprints").strip().rstrip("/")
    if path != "/Game" and not path.startswith("/Game/"):
        raise OperationError("invalid_package_path", "package_path must be a directory under /Game")
    if any(char in _INVALID_PACKAGE_CHARACTERS for char in path) or "//" in path:
        raise OperationError("invalid_package_path", "package_path must contain valid package directory segments and no object suffix")
    return path


def new_asset_path(name, package_path):
    if not name or name != name.strip() or "/" in name or any(char in _INVALID_PACKAGE_CHARACTERS for char in name):
        raise OperationError("invalid_asset_name", "name must be a nonempty single asset identifier without path/object separators")
    path = directory_path(package_path) + "/" + name
    if len((path + "." + name).encode("utf-16-le")) // 2 >= 1024:
        raise OperationError("invalid_asset_name", "The complete new Blueprint object path must be shorter than 1024 UTF-16 units")
    return path


def creation_policy():
    # Installed CreateBlueprint overloads expose no compile options. The factory
    # performs an initial compile without SkipSave; do not change global settings.
    settings = unreal.load_object(None, "/Script/BlueprintGraph.Default__BlueprintEditorSettings")
    if settings is None:
        raise OperationError("creation_policy_unavailable", "Cannot inspect Blueprint SaveOnCompile; factory was not called")
    value = json.loads(unreal.ToolsetLibrary.get_object_properties(settings, ["SaveOnCompile"]))
    mode = value.get("SaveOnCompile")
    if mode != "SoC_Never":
        raise OperationError("creation_autosave_policy_unsupported", f"Unsaved Blueprint creation requires SaveOnCompile=Never; observed {mode!r}. Factory has an implicit compile without a public SkipSave option. No setting was changed or factory called.")
    return {"save_on_compile": mode, "factory_implicit_compile": True,
            "factory_skip_save_option_available": False, "settings_changed": False,
            "scope": "sampled engine setting before factory; arbitrary callback side effects are not proven absent"}


def resolve_parent(parent_name):
    requested = parent_name.strip() or "Actor"
    reject_tool_reference(requested)
    if requested.startswith("/"):
        parent = unreal.load_class(None, requested)
    else:
        # Use actual reflected class names, not the legacy erroneous A/U-prefix path construction.
        class_type = getattr(unreal, requested, None)
        if class_type is None and len(requested) > 1 and requested[0] in "AU":
            class_type = getattr(unreal, requested[1:], None)
        parent = class_type.static_class() if isinstance(class_type, type) and hasattr(class_type, "static_class") else None
    if not parent:
        raise OperationError("parent_class_not_found", f"Parent class did not resolve: {requested}; no fallback class was substituted")
    paths = parent_chain(parent)
    return parent, paths


def parent_reference_state(class_path):
    """Sample a generated parent before loading it; never load in this function."""
    requested = class_path.strip()
    package, separator, name = requested.partition(".")
    if (not package.startswith("/Game/") or not separator
            or name != package.rsplit("/", 1)[-1] + "_C"
            or any(char in _INVALID_PACKAGE_CHARACTERS for char in package)
            or "//" in package or len(requested.encode("utf-16-le")) // 2 >= 1024):
        raise OperationError("generated_parent_path_invalid", "Use an exact /Game/Package.Package_C parent; no skeleton/reinstanced/export aliases")
    path = Path(unreal.Paths.project_content_dir()).resolve().joinpath(*package[6:].split("/")).with_suffix(".uasset")
    digest = None
    if path.is_file():
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
    asset = unreal.find_object(None, package + "." + package.rsplit("/", 1)[-1])
    dirty = {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    return {"class_path": requested, "package_name": package,
        "file": {"path": str(path), "exists": path.is_file(), "sha256": digest},
        "loaded": asset is not None, "dirty": package in dirty,
        "status": str(asset.get_editor_property("status")) if asset is not None else None}


def generated_parent_observation(parent):
    """Create/duplicate baseline; existing Actor/component or pure UObject-root profile."""
    requested = parent.get_path_name()
    if not requested.startswith("/Game/"):
        return None
    # Installed KismetMathLibrary ClassIsChildOf is a public pure class-role
    # query. Do not call the Actor inspector and reinterpret its failure.
    actor_role = unreal.MathLibrary.class_is_child_of(parent, unreal.Actor.static_class())
    component_role = unreal.MathLibrary.class_is_child_of(parent, unreal.ActorComponent.static_class())
    method = "inspect_saved_actor_parent" if actor_role or component_role else "inspect_saved_object_parent"
    helper = getattr(getattr(unreal, "CubelessEventLibrary", None), method, None)
    if not callable(helper):
        raise OperationError("generated_parent_guard_unavailable", "Build/load the selected saved parent inspector before authoring a child")
    try:
        raw = helper(requested)
    finally:
        helper = None
    value = json.loads(raw)
    if not value.get("success"):
        raise OperationError(value.get("error_code") or "generated_parent_unsupported", value.get("error") or "Saved Actor parent validation failed")
    rows = value.get("generated_parents")
    if type(rows) is not list or not rows or len(rows) > 256 or rows[0].get("class_path") != requested:
        raise OperationError("generated_parent_observation_invalid", "Inspector returned no exact bounded generated parent chain")
    files = [parent_reference_state(row["class_path"]) for row in rows]
    if any(not row["file"]["exists"] or not row["file"]["sha256"] or not row["loaded"] or row["dirty"] for row in files):
        raise OperationError("generated_parent_not_saved_clean", "Every generated parent requires a loaded clean source and existing .uasset")
    return {"inspection": value, "parents": files,
        "scope": "saved class chain, native runtime dependency guard, exact source bytes and loaded dirty/status; arbitrary callbacks are not fully observed"}


def asset_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def create(name, path, parent, before_factory=None, after_factory=None):
    creation_policy()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    blueprint_class = unreal.Blueprint.static_class()
    package_path = path.rsplit("/", 1)[0]
    if before_factory is not None:
        before_factory()
    blueprint = asset_tools.create_asset(name, package_path, blueprint_class, factory)
    if after_factory is not None:
        after_factory(blueprint)
    return blueprint


_FUNCTION_LIBRARY_PARENT = "/Script/Engine.BlueprintFunctionLibrary"


def _function_library_native_observation(factory, blueprint, path):
    native = getattr(getattr(unreal, "CubelessNativeToolset", None), "inspect_blueprint_function_library_creation", None)
    if not callable(native):
        raise OperationError("function_library_creation_observer_unavailable", "Load the exact read-only BFL creation observer; no protected property fallback is used")
    try:
        raw = native(factory, blueprint, path + "." + path.rsplit("/", 1)[-1])
    finally:
        native = None
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as error:
        failure = OperationError("function_library_creation_observation_invalid", "Native BFL observer returned invalid JSON")
        failure.native_function_library_result = {"raw_json": raw, "decode_failed": True}
        raise failure from error
    if (type(value) is not dict or value.get("success") is not True
            or value.get("observations_complete") is not True or value.get("current_identity_verified") is not True
            or value.get("read_only") is not True or value.get("mutation_performed") is not False
            or any(value.get(key) is not False for key in ("load_requested", "creation_requested", "compile_requested", "save_requested", "cdo_creation_requested", "cdo_values_sampled"))):
        code = value.get("error_code") or "function_library_creation_observation_failed" if type(value) is dict else "function_library_creation_observation_invalid"
        message = value.get("error") or "Incomplete exact native BFL creation observation" if type(value) is dict else "Native BFL observer result is not an object"
        failure = OperationError(code, message)
        failure.native_function_library_result = value
        raise failure
    return value


def create_function_library(name, path, parent, before_factory=None, after_factory=None):
    """Exact stock BFL factory; native public-field observer replaces protected Python reads."""
    creation_policy()
    factory_type = getattr(unreal, "BlueprintFunctionLibraryFactory", None)
    if factory_type is None:
        raise OperationError("function_library_factory_unavailable", "The installed public BlueprintFunctionLibrary factory is unavailable; no factory dispatch")
    native_parent = unreal.BlueprintFunctionLibrary.static_class()
    if parent != native_parent or parent.get_path_name() != _FUNCTION_LIBRARY_PARENT:
        raise OperationError("function_library_parent_changed", "Requires the exact already resolved native BlueprintFunctionLibrary parent")
    factory = factory_type()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    blueprint_class = unreal.Blueprint.static_class()
    observation = _function_library_native_observation(factory, None, path)
    if (observation.get("observation_kind") != "factory"
            or observation.get("factory_class_path") != "/Script/UnrealEd.BlueprintFunctionLibraryFactory"
            or observation.get("parent_class_path") != _FUNCTION_LIBRARY_PARENT
            or observation.get("blueprint_type") != "BPTYPE_FunctionLibrary"
            or observation.get("target_loaded_before_factory") is not False):
        failure = OperationError("function_library_factory_changed", "Native stock BFL factory fields/current observation differ; no asset creation dispatch")
        failure.native_function_library_result = observation
        raise failure
    if before_factory is not None:
        before_factory()
    create_asset = asset_tools.create_asset
    try:
        blueprint = create_asset(name, path.rsplit("/", 1)[0], blueprint_class, factory)
    finally:
        create_asset = None
    if after_factory is not None:
        after_factory(blueprint)
    return blueprint


def function_library_return_identity(blueprint, observation):
    """Actual return prefix only; broad BFL admission is performed afterwards."""
    observation.update(factory_returned=True, returned_nonnull=blueprint is not None,
                       returned_identity_observed=False)
    if blueprint is None:
        return
    if not unreal.SystemLibrary.is_valid(blueprint):
        raise OperationError("function_library_return_unavailable", "Factory returned an unavailable object; inspect the retained creation prefix")
    observation["returned_object_path"] = blueprint.get_path_name()
    observation["returned_class_path"] = blueprint.get_class().get_path_name()
    observation["returned_identity_observed"] = True


def _function_library_metadata(blueprint, path, parent, observation):
    value = _function_library_native_observation(None, blueprint, path)
    observation["native_metadata"] = value
    if (value.get("observation_kind") != "blueprint" or value.get("observed_object_path") != path + "." + path.rsplit("/", 1)[-1]
            or value.get("observed_class_path") != "/Script/Engine.Blueprint"
            or value.get("blueprint_type") != "BPTYPE_FunctionLibrary"
            or value.get("parent_class_path") != _FUNCTION_LIBRARY_PARENT
            or value.get("parent_resolved") is not True or value.get("scs_resolved") is not True
            or value.get("simple_construction_script_is_null") is not True
            or type(value.get("own_member_count")) is not int or value["own_member_count"] != 0
            or value.get("own_member_names") != [] or value.get("no_scs_verified") is not True
            or value.get("no_own_members_verified") is not True or value.get("native_parent_verified") is not True):
        failure = OperationError("function_library_creation_readback_changed", "Native BFL type/parent/no-SCS/member-free observation did not match")
        failure.native_function_library_result = value
        raise failure
    name = path.rsplit("/", 1)[-1]
    for role, expected in (("generated_class", path + "." + name + "_C"),
                           ("skeleton_class", path + ".SKEL_" + name + "_C")):
        available = value.get(role + "_available")
        if type(available) is not bool or value.get(role + "_path") != (expected if available else ""):
            failure = OperationError("function_library_class_changed", "Native optional BFL class observation differs from the exact target path")
            failure.native_function_library_result = value
            raise failure
        observation[role + "_available"] = available
        observation[role + "_path"] = value[role + "_path"]
    observation.update(blueprint_type=value["blueprint_type"], scs_observed_none=True, own_member_names=[])
    return value


def function_library_creation_snapshot(blueprint, path, parent, observation):
    """Compare fresh native JSON role observations around the existing public snapshot."""
    before = _function_library_metadata(blueprint, path, parent, observation)
    observation["before_readback"] = before
    value = snapshot(blueprint)
    after = _function_library_metadata(blueprint, path, parent, observation)
    observation["after_readback"] = after
    if (before != after or value["path"] != path or value["asset_path"] != observation["returned_object_path"]
            or value["parent_class"] != _FUNCTION_LIBRARY_PARENT or value["variable_names"]
            or value["generated_class"] != after["generated_class_path"]):
        raise OperationError("function_library_creation_readback_changed", "Native BFL roles or existing snapshot changed during readback")
    observation.update(postcondition_passed=True, native_parent_verified=True,
                       no_scs_verified=True, no_own_members_verified=True,
                       existing_class_roles_preserved=True, cdo_creation_requested=False,
                       cdo_values_sampled=False,
                       scope="fresh exact native BFL public C++ field observation only; no protected Python property bypass, function definition/call/runtime or global callback/GC purity proof")
    return value


def resolve_blueprint(name):
    requested = name.strip()
    if not requested:
        raise OperationError("invalid_blueprint_name", "blueprint_name cannot be empty")
    path = requested if requested.startswith("/") else "/Game/Blueprints/" + requested
    if not path.startswith("/Game/"):
        raise OperationError("invalid_blueprint_path", "Blueprint mutations and inspection require a /Game path")
    asset = asset_subsystem().load_asset(path)
    if not isinstance(asset, unreal.Blueprint):
        raise OperationError("blueprint_not_found", f"No Blueprint found at {path}")
    return asset


def snapshot(blueprint):
    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    generated = blueprint.generated_class()
    dirty_packages = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    package_name = blueprint.get_path_name().split(".", 1)[0]
    return {"name": blueprint.get_name(), "asset_path": blueprint.get_path_name(), "path": package_name,
            "parent_class": parent.get_path_name() if parent else "",
            "parent_chain": parent_chain(parent) if parent else [],
            "generated_class": generated.get_path_name() if generated else "",
            "status": str(blueprint.get_editor_property("status")),
            "dirty": package_name in {package.get_path_name() for package in dirty_packages},
            "graph_names": [str(name) for name in unreal.BlueprintEditorLibrary.list_graph_names(blueprint)],
            "variable_names": [str(name) for name in unreal.BlueprintEditorLibrary.list_member_variable_names(blueprint, False)]}


def compile_validation(blueprint, save, refresh_nodes):
    native = getattr(unreal, "CubelessNativeToolset", None)
    if native is None:
        raise OperationError("native_compiler_unavailable", "Build/load CubelessMCPUE58Editor before compiling; no reduced-diagnostic fallback was used")
    return json.loads(native.compile_blueprint_validation(blueprint.get_path_name(), save, refresh_nodes))
