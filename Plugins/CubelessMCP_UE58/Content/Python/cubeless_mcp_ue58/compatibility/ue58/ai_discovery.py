"""Finite UE58 AI discovery EngineAPI; no loader, evaluator or ServerRPC."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


SEARCH_LIBRARY = "CubelessAIDiscoveryLibrary"
SEARCH_BINDINGS = ("search_ai_behavior_tree_node_classes",)
EXPOSURE_NAMES = ("AIController", "AIPerceptionComponent", "AISenseConfig_Sight",
    "AIPerceptionStimuliSourceComponent", "BehaviorTree", "BehaviorTreeFactory",
    "BlackboardData", "BlackboardDataFactory", "Character", "PawnSensingComponent", "TargetPoint")
CONTROLLER_MEMBERS = ("move_to_actor", "move_to_location", "line_of_sight_to",
    "possess", "run_behavior_tree", "use_blackboard")
ASSET_CLASS_MAP = {"BehaviorTree": "behavior_tree", "BlackboardData": "blackboard",
    "EnvQuery": "eqs", "EnvironmentQuery": "eqs"}


def binding_status():
    helper = getattr(unreal, "AssetRegistryHelpers", None)
    library = getattr(unreal, SEARCH_LIBRARY, None)
    return {"asset_registry_getter_callable": callable(getattr(helper, "get_asset_registry", None)),
        "native_search_callable": callable(getattr(library, SEARCH_BINDINGS[0], None))}


def inspect_ai_authoring_support():
    # Actual current Python exposure, not an assumed all-True capability table.
    exposed = {name: bool(getattr(unreal, name, None)) for name in EXPOSURE_NAMES}
    controller = getattr(unreal, "AIController", None)
    members = [name for name in dir(controller) if name in CONTROLLER_MEMBERS] if controller else []
    return {"success": True, "stage": "inspect_ai_authoring_support", "exposed": exposed,
        "ai_controller_members": members,
        "behavior_tree_python_note": "This reports current Unreal Python class/member exposure only; factory or method availability does not prove BT/BB authoring, movement, LOS, perception or runtime behavior.",
        "mutation_attempted": False, "mutation_performed": False, "changed": False,
        "explicit_asset_load_calls": 0, "explicit_compile_calls": 0, "explicit_save_calls": 0,
        "python_type_wrapping_effects_independently_observed": False,
        "engine_target": "Unreal Engine 5.8.3"}


def list_ai_assets(asset_root, asset_type, name_filter, max_results):
    getter = getattr(getattr(unreal, "AssetRegistryHelpers", None), "get_asset_registry", None)
    if not callable(getter):
        raise OperationError("ai_asset_registry_unavailable", "The public AssetRegistry getter is unavailable; no fallback or scan/load ran")
    registry = getter()
    if not callable(getattr(registry, "get_assets_by_path", None)):
        raise OperationError("ai_asset_registry_unavailable", "The public AssetRegistry path query is unavailable; no fallback or scan/load ran")
    items = []
    try:
        # Preserve the original recursive query and default include-only-on-disk=False.
        # Do not substitute a synchronous scan, AssetData.get_asset or class load.
        for asset in registry.get_assets_by_path(asset_root, recursive=True):
            class_name = str(asset.asset_class_path.asset_name)
            kind = ASSET_CLASS_MAP.get(class_name)
            if not kind or asset_type != "all" and kind != asset_type:
                continue
            asset_name = str(asset.asset_name)
            package_name = str(asset.package_name)
            object_path = package_name + "." + asset_name
            if name_filter and name_filter not in asset_name.lower() and name_filter not in object_path.lower():
                continue
            items.append({"asset_type": kind, "asset_name": asset_name,
                "package_name": package_name, "object_path": object_path, "class_name": class_name})
        items.sort(key=lambda item: (item["asset_type"], item["object_path"].lower()))
    except Exception as error:
        # Retain actual projected rows even if a later getter/projection fails.
        failure = OperationError("list_ai_assets_failed", str(error))
        failure.observation_prefix = {"assets_observed_prefix": items,
            "inventory_complete": False, "exception_type": type(error).__name__}
        raise failure from error
    total = len(items)
    return {"success": True, "asset_root": asset_root, "asset_type": asset_type,
        "total_matched": total, "returned_count": min(total, max_results),
        "truncated": total > max_results, "assets": items[:max_results],
        "inventory_complete": True, "registry_recursive": True,
        "registry_include_only_on_disk_assets": False, "registry_synchronous_scan_requested": False,
        "mutation_attempted": False, "mutation_performed": False, "changed": False,
        "explicit_asset_load_calls": 0, "explicit_compile_calls": 0, "explicit_save_calls": 0,
        "registry_implicit_metadata_effects_independently_observed": False,
        "engine_target": "Unreal Engine 5.8.3"}


def search_ai_behavior_tree_node_classes(category, query, include_abstract, max_results):
    function = getattr(getattr(unreal, SEARCH_LIBRARY, None), SEARCH_BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_ai_discovery_api_unavailable", "Build/load the exact finite AI class discovery helper; no Python class-flag guess, load or fallback ran")
    return _decode_result(function(category, query, include_abstract, max_results))
