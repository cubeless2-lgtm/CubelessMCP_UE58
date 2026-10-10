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
    """Shared create/duplicate baseline for saved generated Actor parent chains."""
    requested = parent.get_path_name()
    if not requested.startswith("/Game/"):
        return None
    helper = getattr(getattr(unreal, "CubelessEventLibrary", None), "inspect_saved_actor_parent", None)
    if not callable(helper):
        raise OperationError("generated_parent_guard_unavailable", "Build/load the saved Actor parent inspector before authoring a child")
    value = json.loads(helper(requested))
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
