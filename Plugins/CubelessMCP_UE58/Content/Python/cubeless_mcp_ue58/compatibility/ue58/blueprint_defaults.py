"""Named UE58 Blueprint defaults EngineAPI; no loading/fallback in Python."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessBlueprintDefaultsLibrary"
BINDINGS = ("set_blueprint_actor_tick_settings", "set_skeletal_mesh_component_anim_defaults")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(name, *arguments):
    function = getattr(getattr(unreal, LIBRARY, None), name, None)
    if (name not in BINDINGS and name != "set_blueprint_actor_tick_settings_raw") or not callable(function):
        raise OperationError("native_blueprint_defaults_api_unavailable", "Build/load the exact Blueprint defaults helper; no Python loader, writer or fallback ran")
    if name == "set_blueprint_actor_tick_settings_raw":
        try:
            raw = function(*arguments)
        finally:
            function = None
    else:
        raw = function(*arguments)
    try:
        return _decode_result(raw)
    except Exception as error:
        error.native_raw_result = raw
        error.native_call_returned = True
        raise


def set_blueprint_actor_tick_settings(blueprint_path, tick_hz, tick_interval,
        can_ever_tick, start_with_tick_enabled, allow_tick_on_dedicated_server,
        tick_even_when_paused, dry_run, compile, save):
    return _invoke("set_blueprint_actor_tick_settings", blueprint_path, tick_hz, tick_interval,
        can_ever_tick, start_with_tick_enabled, allow_tick_on_dedicated_server,
        tick_even_when_paused, dry_run, compile, save)


def set_skeletal_mesh_component_anim_defaults(blueprint_path, component_name,
        skeletal_mesh, anim_class, physics_asset_json, simulate_physics_json,
        collision_enabled_json, collision_response_json, enable_gravity_json,
        visibility_based_anim_tick_option_json, compile, save):
    return _invoke("set_skeletal_mesh_component_anim_defaults", blueprint_path, component_name,
        skeletal_mesh, anim_class, physics_asset_json, simulate_physics_json,
        collision_enabled_json, collision_response_json, enable_gravity_json,
        visibility_based_anim_tick_option_json, compile, save)


def set_blueprint_actor_tick_settings_raw(blueprint_path, settings_json):
    # Intentionally absent from Named BINDINGS/readiness and public schemas.
    return _invoke("set_blueprint_actor_tick_settings_raw", blueprint_path, settings_json)
