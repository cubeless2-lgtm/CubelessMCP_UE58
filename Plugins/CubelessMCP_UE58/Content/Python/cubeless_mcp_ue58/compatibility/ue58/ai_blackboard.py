"""UE58 finite AI EngineAPI bindings; no adapter load, fallback or RPC."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessBlackboardLibrary"
BEHAVIOR_TREE_LIBRARY = "CubelessBehaviorTreeLibrary"
BEHAVIOR_TREE_BINDINGS = ("create_ai_behavior_tree_asset", "set_ai_behavior_tree_blackboard")
ENV_QUERY_LIBRARY = "CubelessEnvQueryLibrary"
ENV_QUERY_BINDINGS = ("create_ai_eqs_query",)
BEHAVIOR_ASSETS_LIBRARY = "CubelessAIBehaviorAssetsLibrary"
BEHAVIOR_ASSETS_BINDINGS = ("create_ai_patrol_chase_behavior_assets",)
PERCEPTION_LIBRARY = "CubelessAIPerceptionLibrary"
PERCEPTION_BINDINGS = ("configure_ai_controller_perception",)
BLACKBOARD_REMOVE_BINDINGS = ("remove_ai_blackboard_key",)
BINDINGS = ("create_ai_blackboard_asset", "add_ai_blackboard_key", "inspect_ai_asset_structure",
    *BEHAVIOR_TREE_BINDINGS, *ENV_QUERY_BINDINGS, *BEHAVIOR_ASSETS_BINDINGS, *PERCEPTION_BINDINGS, *BLACKBOARD_REMOVE_BINDINGS)
BINDING_LIBRARIES = {BINDINGS[0]: LIBRARY, BINDINGS[1]: LIBRARY,
    BINDINGS[2]: ENV_QUERY_LIBRARY, **{name: BEHAVIOR_TREE_LIBRARY for name in BEHAVIOR_TREE_BINDINGS},
    **{name: ENV_QUERY_LIBRARY for name in ENV_QUERY_BINDINGS},
    **{name: BEHAVIOR_ASSETS_LIBRARY for name in BEHAVIOR_ASSETS_BINDINGS},
    **{name: PERCEPTION_LIBRARY for name in PERCEPTION_BINDINGS},
    **{name: LIBRARY for name in BLACKBOARD_REMOVE_BINDINGS}}


def _invoke(method, *arguments):
    if method not in BINDINGS:
        raise OperationError("native_ai_blackboard_api_unavailable", "Only the nine named finite AI leaves are admitted")
    library = BINDING_LIBRARIES[method]
    unavailable = ("native_ai_perception_api_unavailable" if method in PERCEPTION_BINDINGS else
        "native_ai_behavior_assets_api_unavailable" if method in BEHAVIOR_ASSETS_BINDINGS else
        "native_ai_env_query_api_unavailable" if method in ENV_QUERY_BINDINGS else
        "native_ai_behavior_tree_api_unavailable" if method in BEHAVIOR_TREE_BINDINGS else "native_ai_blackboard_api_unavailable")
    function = getattr(getattr(unreal, library, None), method, None)
    if not callable(function):
        raise OperationError(unavailable, "Build/load " + library + "; no fallback, adapter asset load or command executor ran")
    return _decode_result(function(*arguments))


def create_ai_blackboard_asset(asset_name, asset_root, save_asset):
    return _invoke(BINDINGS[0], asset_name, asset_root, save_asset)


def add_ai_blackboard_key(blackboard_path, key_name, key_type, base_class,
        enum_path, instance_synced, save_asset):
    return _invoke(BINDINGS[1], blackboard_path, key_name, key_type, base_class,
        enum_path, instance_synced, save_asset)


def inspect_ai_asset_structure(asset_path):
    # Native EQS dispatch delegates the existing BT/Blackboard result unchanged.
    # Python never loads/type-sniffs an asset or falls back to another library.
    return _invoke(BINDINGS[2], asset_path)


def create_ai_behavior_tree_asset(asset_name, asset_root, blackboard_path, save_asset):
    return _invoke(BEHAVIOR_TREE_BINDINGS[0], asset_name, asset_root, blackboard_path, save_asset)


def set_ai_behavior_tree_blackboard(behavior_tree_path, blackboard_path, save_asset):
    return _invoke(BEHAVIOR_TREE_BINDINGS[1], behavior_tree_path, blackboard_path, save_asset)


def create_ai_eqs_query(asset_name, asset_root, save_asset):
    return _invoke(ENV_QUERY_BINDINGS[0], asset_name, asset_root, save_asset)


def create_ai_patrol_chase_behavior_assets(asset_root, behavior_tree_name, blackboard_name,
        target_actor_key, patrol_location_key, tree_shape, acceptable_radius,
        alert_wait_seconds, face_precision_degrees, replace_existing, save_assets):
    return _invoke(BEHAVIOR_ASSETS_BINDINGS[0], asset_root, behavior_tree_name, blackboard_name,
        target_actor_key, patrol_location_key, tree_shape, acceptable_radius,
        alert_wait_seconds, face_precision_degrees, replace_existing, save_assets)


def configure_ai_controller_perception(controller_blueprint, component_name, sight_radius,
        lose_sight_radius, peripheral_vision_angle_degrees, max_age, detect_enemies,
        detect_neutrals, detect_friendlies, replace_existing_config, save_blueprint):
    return _invoke(PERCEPTION_BINDINGS[0], controller_blueprint, component_name, sight_radius,
        lose_sight_radius, peripheral_vision_angle_degrees, max_age, detect_enemies,
        detect_neutrals, detect_friendlies, replace_existing_config, save_blueprint)


def remove_ai_blackboard_key(blackboard_path, key_name, save_asset):
    return _invoke(BLACKBOARD_REMOVE_BINDINGS[0], blackboard_path, key_name, save_asset)
