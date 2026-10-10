"""UE 5.8.3 public native Actor bindings; no registry or transport here."""

import json
import unreal

from cubeless_mcp_ue58.operations.state import OperationError

MAX_ACTORS = 10000


def _invoke(method, *arguments):
    library = getattr(unreal, "CubelessActorLibrary", None)
    function = getattr(library, method, None) if library is not None else None
    if function is None:
        raise OperationError("native_actor_api_unavailable", "Build/load CubelessActorLibrary for UE58 before using Actor tools; no fallback executed")
    raw = function(*arguments)
    try:
        result = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_actor_result", "Native Actor result was not valid JSON; inspect the level before retrying a mutation") from error
    if (type(result) is not dict or type(result.get("success")) is not bool
            or type(result.get("data")) is not dict or type(result.get("mutation_attempted")) is not bool):
        raise OperationError("invalid_native_actor_result", "Native Actor result lacks its success/data/mutation_attempted envelope")
    return result


def get_actors_in_level(expected_level_path):
    return _invoke("get_actors_in_level", expected_level_path, MAX_ACTORS)


def find_actors_by_name(pattern, expected_level_path):
    return _invoke("find_actors_by_name", pattern, expected_level_path, MAX_ACTORS)


def get_actor_properties(name, expected_level_path):
    return _invoke("get_actor_properties", name, expected_level_path, MAX_ACTORS)


def spawn_actor(name, actor_type, location_json, rotation_json, scale_json, expected_level_path):
    return _invoke("spawn_actor", name, actor_type, location_json, rotation_json, scale_json, expected_level_path, MAX_ACTORS)


def delete_actor(name, expected_level_path):
    return _invoke("delete_actor", name, expected_level_path, MAX_ACTORS)


def spawn_blueprint_actor(blueprint_path, name, location_json, rotation_json, scale_json, expected_level_path):
    return _invoke("spawn_blueprint_actor", blueprint_path, name, location_json, rotation_json, scale_json, expected_level_path, MAX_ACTORS)


def set_actor_transform(name, location_json, rotation_json, scale_json, expected_level_path):
    return _invoke("set_actor_transform", name, location_json, rotation_json, scale_json, expected_level_path, MAX_ACTORS)
