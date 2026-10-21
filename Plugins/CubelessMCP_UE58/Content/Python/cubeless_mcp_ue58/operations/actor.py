"""Named Actor operations, durable intent, and native postcondition observations.

Only the native compatibility layer touches engine objects. Public aliases share
action cores, but keep distinct durable identities. No automatic compile/save,
rollback, retry, or actor selection is requested by this layer.
"""

import copy
import json
import math

from cubeless_mcp_ue58.compatibility.ue58 import actor as api
from . import state
from .state import OperationError, fail, mutate, new_result

_TYPES = {name.lower(): name for name in ("StaticMeshActor", "PointLight", "SpotLight", "DirectionalLight", "CameraActor")}


def _string(value, field, *, empty=False, limit=1023):
    if type(value) is not str or (not empty and not value) or len(value.encode("utf-16-le")) // 2 > limit:
        raise OperationError("invalid_actor_input", field + " must be " + ("a" if empty else "a nonempty") + " string of at most " + str(limit) + " UTF-16 code units")
    return value


def _guard(value):
    return _string("" if value is None else value, "expected_level_path", empty=True)


def _vector_json(value, field, *, allow_null=False):
    """Explicit JSON string adapter; never register reflected array parameters."""
    _string(value, field, limit=512)
    try:
        values = json.loads(value)
        if allow_null and values is None:
            return "null"
        if type(values) is not list or len(values) != 3:
            raise ValueError("expected exactly three numbers" + (" or JSON null" if allow_null else ""))
        if any(type(item) not in (int, float) or not math.isfinite(float(item)) for item in values):
            raise ValueError("components must be finite numbers, not booleans or strings")
        if any(abs(item) > 1.0e12 for item in values):
            raise ValueError("component magnitude must not exceed 1e12 (native editor/rendering safety bound)")
        return json.dumps(values, allow_nan=False, separators=(",", ":"))
    except (ValueError, OverflowError, RecursionError) as error:
        raise OperationError("invalid_actor_vector", field + ": " + str(error)) from error


def _spawn_arguments(name, actor_type, location_json, rotation_json, scale_json, expected_level_path):
    kind = _TYPES.get(_string(actor_type, "type").lower())
    if kind is None:
        raise OperationError("actor_type_unsupported", "This slice spawns StaticMeshActor, PointLight, SpotLight, DirectionalLight or CameraActor only")
    return {"name": _string(name, "name"), "type": kind,
            "location_json": _vector_json(location_json, "location_json"),
            "rotation_json": _vector_json(rotation_json, "rotation_json"),
            "scale_json": _vector_json(scale_json, "scale_json"), "expected_level_path": _guard(expected_level_path)}


def _apply_native(result, native):
    identity = result["data"]["operation"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"]["operation"] = identity
    result["data"]["native_outcome"] = {key: copy.deepcopy(value) for key, value in native.items() if key not in {"data", "before", "after"}}
    for key in ("before", "after"):
        if key in native:
            result["data"][key] = copy.deepcopy(native[key])
    result["data"]["limits"] = {"max_loaded_actors": api.MAX_ACTORS, "on_exceeded": "reject_without_mutation"}
    result["stage"] = native.get("stage", "native_result")
    result["compile"].update(status="not_requested")
    result["save"].update(status="not_requested", saved=False)
    result["validation"]["native_postcondition_passed"] = native.get("postcondition_passed") is True
    result["validation"]["native_verification"] = copy.deepcopy(native.get("verification", {}))
    if native["mutation_attempted"] is False and native.get("changed") is False and native.get("change_state_known") is True:
        result["changes"] = []
    else:
        known = native.get("change_state_known") is True
        result["changes"] = [{"kind": identity, "attempted": native["mutation_attempted"], "changed": native.get("changed") if known else None,
            "change_state_known": known,
            "dirty_before": native.get("dirty_before"), "dirty_after": native.get("dirty_after"),
            "outcome": "observed_native_result" if known else "unknown_after_native_dispatch"}]
    if native["success"] is not True:
        raise OperationError(native.get("error_code") or "native_actor_operation_failed", native.get("error") or "Native Actor operation failed; inspect its observations")


def _read_action(result, name, call):
    result["data"]["operation"] = name
    result["stage"] = "native_read"
    native = call()
    _apply_native(result, native)
    if native["mutation_attempted"] is not False:
        raise OperationError("unexpected_read_mutation", "Native Actor read reported a mutation")
    result["validation"]["status"] = "native_loaded_editor_world_inspection"
    result["success"] = True
    result["stage"] = "complete"


def _read(action):
    result = new_result()
    try:
        action(result)
    except Exception as error:
        fail(result, error)
    return result


def get_actors_in_level(expected_level_path=None):
    return _read(lambda result: get_actors_in_level_action(result, expected_level_path))


def get_actors_in_level_action(result, expected_level_path=None, checkpoint_callback=None):
    _read_action(result, "get_actors_in_level", lambda: api.get_actors_in_level(_guard(expected_level_path)))


def find_actors_by_name(pattern, expected_level_path=None):
    return _read(lambda result: find_actors_by_name_action(result, pattern, expected_level_path))


def find_actors_by_name_action(result, pattern, expected_level_path=None, checkpoint_callback=None):
    _read_action(result, "find_actors_by_name", lambda: api.find_actors_by_name(_string(pattern, "pattern", empty=True, limit=4096), _guard(expected_level_path)))


def get_actor_properties(name, expected_level_path=None):
    return _read(lambda result: get_actor_properties_action(result, name, expected_level_path))


def get_actor_properties_action(result, name, expected_level_path=None, checkpoint_callback=None):
    _read_action(result, "get_actor_properties", lambda: api.get_actor_properties(_string(name, "name"), _guard(expected_level_path)))


def _mutation_action(result, identity, arguments, call, checkpoint_callback=None, *, spawning=False):
    result["data"]["operation"] = identity
    result["stage"] = "before_snapshot"
    before = (api.get_actors_in_level(arguments["expected_level_path"]) if spawning else
              api.get_actor_properties(arguments["name"], arguments["expected_level_path"]))
    if before["success"] is not True or before["mutation_attempted"] is not False:
        _apply_native(result, before)
        raise OperationError("unexpected_read_mutation", "Before snapshot reported a mutation; no requested mutation dispatched")
    snapshot = copy.deepcopy(before["data"])
    world_before = before.get("before", {})
    for key in ("dirty", "world_partition", "external_actors", "selected_actor_paths"):
        if key in world_before:
            snapshot[key] = copy.deepcopy(world_before[key])
    # Preserve the relevant identity and context without writing every unrelated
    # loaded actor into the durable journal for a one-actor spawn request.
    if spawning:
        actors = snapshot.pop("actors", None)
        if type(actors) is not list:
            raise OperationError("missing_actor_snapshot", "Before snapshot lacks its loaded actors array; no mutation dispatched")
        snapshot["requested_name_matches"] = [item for item in actors if item.get("name") == arguments["name"]]
        snapshot["loaded_actor_count"] = len(actors)
    result["data"]["before_snapshot"] = snapshot
    effective_guard = snapshot.get("persistent_level_path")
    if type(effective_guard) is not str or not effective_guard.startswith("/Game/"):
        raise OperationError("missing_actor_level_identity", "Before snapshot must identify a saved /Game persistent level; no mutation dispatched")
    if arguments["expected_level_path"] and effective_guard != arguments["expected_level_path"]:
        raise OperationError("actor_level_guard_mismatch", "The inspected persistent level differs from expected_level_path; no mutation dispatched")
    result["data"]["effective_expected_level_path"] = effective_guard
    result["data"]["pending_mutation"] = {"operation": identity, "name": arguments["name"],
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_level_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_mutation"
    result["changes"].append({"kind": identity, "name": arguments["name"], "level_path": effective_guard,
        "attempted": True, "outcome": "unknown_until_native_result"})
    native = call(effective_guard)
    _apply_native(result, native)
    if native.get("postcondition_passed") is not True:
        raise OperationError("actor_postcondition_not_verified", "Native dispatch succeeded without a verified Actor postcondition")
    if native.get("change_state_known") is not True:
        raise OperationError("actor_change_state_unknown", "Native dispatch lacks a complete basic Actor snapshot; inspect the level before another mutation")
    result["validation"]["status"] = "native_actor_postcondition_passed"
    result["retry"]["duplicate_risk"] = "actor_mutation_would_repeat" if native.get("changed") else "none_observed"
    result["success"] = True
    result["stage"] = "complete"


def spawn_actor_action(result, name, type, location_json, rotation_json, scale_json, expected_level_path=None, checkpoint_callback=None, *, identity="spawn_actor"):
    """Shared guarded spawn core; caller owns the journal or enclosing batch."""
    arguments = _spawn_arguments(name, type, location_json, rotation_json, scale_json, expected_level_path)
    _spawn_action_from_arguments(result, identity, arguments, checkpoint_callback)


def _spawn_action_from_arguments(result, identity, arguments, checkpoint_callback=None):
    _mutation_action(result, identity, arguments, lambda guard: api.spawn_actor(arguments["name"], arguments["type"],
        arguments["location_json"], arguments["rotation_json"], arguments["scale_json"], guard), checkpoint_callback, spawning=True)


def _spawn(identity, name, type, location_json, rotation_json, scale_json, expected_level_path, operation_id):
    try:
        arguments = _spawn_arguments(name, type, location_json, rotation_json, scale_json, expected_level_path)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate(identity, arguments, operation_id, lambda result: _spawn_action_from_arguments(result, identity, arguments))


def spawn_actor(name, type, location_json="[0,0,0]", rotation_json="[0,0,0]", scale_json="[1,1,1]", expected_level_path=None, operation_id=None):
    return _spawn("spawn_actor", name, type, location_json, rotation_json, scale_json, expected_level_path, operation_id)


def create_actor(name, type, location_json="[0,0,0]", rotation_json="[0,0,0]", scale_json="[1,1,1]", expected_level_path=None, operation_id=None):
    return _spawn("create_actor", name, type, location_json, rotation_json, scale_json, expected_level_path, operation_id)


def _spawn_blueprint_arguments(blueprint_name, actor_name, location_json, rotation_json, scale_json, expected_level_path):
    path = _string(blueprint_name, "blueprint_name")
    if not path.startswith("/"):
        if "/" in path or "." in path:
            raise OperationError("blueprint_path_invalid", "Use a simple /Game/Blueprints name or exact /Game asset path")
        path = "/Game/Blueprints/" + path
    if not path.startswith("/Game/") or any(ord(c) < 32 or ord(c) == 127 for c in path):
        raise OperationError("blueprint_path_invalid", "Only persistent /Game Blueprint assets are supported")
    if "." not in path:
        path += "." + path.rsplit("/", 1)[-1]
    return {"blueprint_path": _string(path, "blueprint_path"), "name": _string(actor_name, "actor_name"),
        "location_json": _vector_json(location_json, "location_json"),
        "rotation_json": _vector_json(rotation_json, "rotation_json"),
        "scale_json": _vector_json(scale_json, "scale_json"), "expected_level_path": _guard(expected_level_path)}


def _spawn_blueprint_action_from_arguments(result, arguments, checkpoint_callback=None):
    _mutation_action(result, "spawn_blueprint_actor", arguments,
        lambda guard: api.spawn_blueprint_actor(arguments["blueprint_path"], arguments["name"],
            arguments["location_json"], arguments["rotation_json"], arguments["scale_json"], guard),
        checkpoint_callback, spawning=True)


def spawn_blueprint_actor_action(result, blueprint_name, actor_name, location_json="[0,0,0]", rotation_json="[0,0,0]", scale_json="[1,1,1]", expected_level_path=None, checkpoint_callback=None):
    arguments = _spawn_blueprint_arguments(blueprint_name, actor_name, location_json, rotation_json, scale_json, expected_level_path)
    _spawn_blueprint_action_from_arguments(result, arguments, checkpoint_callback)


def spawn_blueprint_actor(blueprint_name, actor_name, location_json="[0,0,0]", rotation_json="[0,0,0]", scale_json="[1,1,1]", expected_level_path=None, operation_id=None):
    """Legacy exact UObject-name spawn, sharing the native Actor mutation core."""
    try:
        arguments = _spawn_blueprint_arguments(blueprint_name, actor_name, location_json, rotation_json, scale_json, expected_level_path)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("spawn_blueprint_actor", arguments, operation_id,
        lambda result: _spawn_blueprint_action_from_arguments(result, arguments))


def delete_actor_action(result, name, expected_level_path=None, checkpoint_callback=None):
    arguments = {"name": _string(name, "name"), "expected_level_path": _guard(expected_level_path)}
    _delete_action_from_arguments(result, arguments, checkpoint_callback)


def _delete_action_from_arguments(result, arguments, checkpoint_callback=None):
    _mutation_action(result, "delete_actor", arguments, lambda guard: api.delete_actor(arguments["name"], guard), checkpoint_callback)


def delete_actor(name, expected_level_path=None, operation_id=None):
    try:
        arguments = {"name": _string(name, "name"), "expected_level_path": _guard(expected_level_path)}
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("delete_actor", arguments, operation_id, lambda result: _delete_action_from_arguments(result, arguments))


def _transform_arguments(name, location_json, rotation_json, scale_json, expected_level_path):
    return {"name": _string(name, "name"), "expected_level_path": _guard(expected_level_path),
        "location_json": _vector_json(location_json, "location_json", allow_null=True),
        "rotation_json": _vector_json(rotation_json, "rotation_json", allow_null=True),
        "scale_json": _vector_json(scale_json, "scale_json", allow_null=True)}


def set_actor_transform_action(result, name, location_json="null", rotation_json="null", scale_json="null", expected_level_path=None, checkpoint_callback=None):
    arguments = _transform_arguments(name, location_json, rotation_json, scale_json, expected_level_path)
    _transform_action_from_arguments(result, arguments, checkpoint_callback)


def _transform_action_from_arguments(result, arguments, checkpoint_callback=None):
    _mutation_action(result, "set_actor_transform", arguments, lambda guard: api.set_actor_transform(arguments["name"],
        arguments["location_json"], arguments["rotation_json"], arguments["scale_json"], guard), checkpoint_callback)


def set_actor_transform(name, location_json="null", rotation_json="null", scale_json="null", expected_level_path=None, operation_id=None):
    try:
        arguments = _transform_arguments(name, location_json, rotation_json, scale_json, expected_level_path)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("set_actor_transform", arguments, operation_id, lambda result: _transform_action_from_arguments(result, arguments))


def _original_transform_vector_json(value, field):
    """Preserve nullable, short and long original arrays; Native owns front-three float32/short-zero parsing."""
    if value is not None and type(value) is not list:
        raise OperationError("invalid_actor_original_vector", field + " must be an array or null")
    try:
        return json.dumps(value, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, OverflowError, RecursionError) as error:
        raise OperationError("invalid_actor_original_vector", field + ": " + str(error)) from error


def _original_transform_arguments(name, location, rotation, scale):
    return {"name": _string(name, "name"), "expected_level_path": "",
            "location_json": _original_transform_vector_json(location, "location"),
            "rotation_json": _original_transform_vector_json(rotation, "rotation"),
            "scale_json": _original_transform_vector_json(scale, "scale")}


def _original_transform_action_from_arguments(result, arguments, checkpoint_callback=None):
    # Existing guarded before-read/checkpoint/adopt-current preserves full Native failure/unknown and legacy payload.
    # A missing legacy_result is not synthesized here or promoted to an original success.
    _mutation_action(result, "set_actor_transform_original", arguments,
        lambda guard: api.set_actor_transform_original(arguments["name"], arguments["location_json"],
            arguments["rotation_json"], arguments["scale_json"], guard), checkpoint_callback)


def set_actor_transform_original_action(result, name, location=None, rotation=None, scale=None, checkpoint_callback=None):
    arguments = _original_transform_arguments(name, location, rotation, scale)
    _original_transform_action_from_arguments(result, arguments, checkpoint_callback)


def set_actor_transform_original(name, location=None, rotation=None, scale=None):
    """Exact original four-input Actor transform route; returns standard internal receipt plus actual Native legacy_result.

    The Raw Toolset publishes only Native's known legacy_result when present. Before-dispatch Python failures
    and unknown Native outcomes retain their real standard receipt. The existing six-input transform route
    stays separate. Native always dispatches its original setter once even for name-only/no-op requests;
    this layer adds no compile/save, mutation retry, array-length-three gate or float64 replacement parser.
    """
    try:
        arguments = _original_transform_arguments(name, location, rotation, scale)
    except Exception as error:
        return fail(new_result(), error)
    return mutate("set_actor_transform_original", arguments, None,
        lambda result: _original_transform_action_from_arguments(result, arguments))
