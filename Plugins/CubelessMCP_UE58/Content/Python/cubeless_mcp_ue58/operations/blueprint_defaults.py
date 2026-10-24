"""Original Actor tick and skeletal template defaults using one shared journal.

Existing pure component selector/path machinery is reused. The older component
mutation core deliberately rejects saving, so it is not reused for these two
compile/save-aware original contracts. SCS class replacement and bulk rigid
constraint authoring remain separate pending original operations.
"""

import copy
import json
import math

from cubeless_mcp_ue58.compatibility.ue58 import blueprint_defaults as api
from . import component, state
from .state import OperationError


TICK = "set_blueprint_actor_tick_settings"
ANIM = "set_skeletal_mesh_component_anim_defaults"
_FORBIDDEN = ("unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools",
    "/cubelessmcp_ue58/", "/script/cubelessmcpue58editor")


def _text(value, field, *, empty=False):
    text = component._selector(value, field, empty=empty)
    if any(token in text.replace("\\", "/").lower() for token in _FORBIDDEN):
        raise OperationError("blueprint_defaults_reference_unsupported", "MCP-only reference in " + field + "; no defaults helper dispatch ran")
    return text


def _path(value):
    _text(value, "blueprint_name")
    return component._mutation_path(value)


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("blueprint_defaults_input_invalid", field + " requires an exact boolean")
    return value


def _number(value, field):
    if type(value) not in (int, float):
        raise OperationError("blueprint_defaults_number_invalid", field + " requires a finite number, not a boolean or coercible value")
    try:
        number = float(value)
    except (ValueError, OverflowError) as error:
        raise OperationError("blueprint_defaults_number_invalid", field + " cannot be represented as a finite double") from error
    if not math.isfinite(number):
        raise OperationError("blueprint_defaults_number_invalid", field + " requires a finite number")
    return number


def _optional_json(value, field, expected):
    if value is not None:
        if expected is str:
            value = _text(value, field, empty=True)
        elif type(value) is not expected:
            raise OperationError("blueprint_defaults_input_invalid", field + " requires " + expected.__name__ + " or None")
    # None means omitted original field, whereas a quoted empty PhysicsAsset
    # string means clear. The hidden ABI uses JSON only for this presence bit.
    return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))


def _tick_arguments(blueprint_name, tick_hz, tick_interval, can_ever_tick,
        start_with_tick_enabled, allow_tick_on_dedicated_server, tick_even_when_paused,
        dry_run, compile, save):
    return {"blueprint_path": _path(blueprint_name), "tick_hz": _number(tick_hz, "tick_hz"),
        "tick_interval": _number(tick_interval, "tick_interval"),
        "can_ever_tick": _bool(can_ever_tick, "can_ever_tick"),
        "start_with_tick_enabled": _bool(start_with_tick_enabled, "start_with_tick_enabled"),
        "allow_tick_on_dedicated_server": _bool(allow_tick_on_dedicated_server, "allow_tick_on_dedicated_server"),
        "tick_even_when_paused": _bool(tick_even_when_paused, "tick_even_when_paused"),
        "dry_run": _bool(dry_run, "dry_run"), "compile": _bool(compile, "compile"), "save": _bool(save, "save")}


def _anim_arguments(blueprint_name, component_name, skeletal_mesh, anim_class,
        physics_asset, simulate_physics, collision_enabled, collision_response,
        enable_gravity, visibility_based_anim_tick_option, compile, save):
    return {"blueprint_path": _path(blueprint_name), "component_name": _text(component_name, "component_name"),
        "skeletal_mesh": _text(skeletal_mesh, "skeletal_mesh", empty=True),
        "anim_class": _text(anim_class, "anim_class", empty=True),
        "physics_asset_json": _optional_json(physics_asset, "physics_asset", str),
        "simulate_physics_json": _optional_json(simulate_physics, "simulate_physics", bool),
        "collision_enabled_json": _optional_json(collision_enabled, "collision_enabled", str),
        "collision_response_json": _optional_json(collision_response, "collision_response", str),
        "enable_gravity_json": _optional_json(enable_gravity, "enable_gravity", bool),
        "visibility_based_anim_tick_option_json": _optional_json(visibility_based_anim_tick_option, "visibility_based_anim_tick_option", str),
        "compile": _bool(compile, "compile"), "save": _bool(save, "save")}


def _context(result, identity, arguments):
    result["data"].update(operation=identity, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, full_legacy_blueprint_defaults_parity=False,
        cold_persistence_verified=False, runtime_tick_or_animation_verified=False,
        library_cdo_before_native_entry_independently_observed=False)


def _apply_native(result, identity, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None,
        actual_work_counts_authority="returned_native_counters_and_ordered_save_calls")
    _context(result, identity, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_result" if native["compile_requested"] else "not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": identity, "asset_path": arguments["blueprint_path"],
        "component_name": arguments.get("component_name"), "attempted": native["mutation_attempted"],
        "change_state_known": known, "changed": native.get("changed") if known else None,
        "outcome": "native_defaults_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_defaults_postcondition" if native["postcondition_passed"] else "native_defaults_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run", packaged_runtime="not_run")
    result["stage"] = "returned_blueprint_defaults_operation"


def _run_action(result, identity, arguments, checkpoint_callback=None, native_method=None):
    if identity not in (TICK, ANIM):
        raise OperationError("blueprint_defaults_operation_invalid", "Only the two agreed named defaults helpers are admitted")
    _context(result, identity, arguments)
    callback = checkpoint_callback or state.checkpoint
    result["data"]["pending_mutation"] = {"operation": identity, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_blueprint_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_defaults_operation"
    result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "asset_path": arguments["blueprint_path"],
        "attempted": None, "change_state_known": False, "changed": None, "outcome": "unknown_until_native_response"}]
    try:
        native = getattr(api, native_method or identity)(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_blueprint_defaults_operation"
            callback(result)
        if getattr(error, "code", "") == "native_blueprint_defaults_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_blueprint_defaults_failed", native.get("error") or "Defaults helper failed; full returned prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_defaults_result_unverified", "Native success lacks known actual defaults/compile/save postconditions; do not replay")
    # Original lifecycle differs: Tick never saves without compile, whereas
    # Anim permits an explicitly reported unvalidated save without compilation.
    # Counters/receipts and dry-run desired-vs-observed state stay native-owned.
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_defaults_and_journal_before_new_request" if native["mutation_attempted"] else "none_observed"


def _mutate(identity, arguments):
    return state.mutate(identity, arguments, None, lambda result: _run_action(result, identity, arguments))


def _rejected(identity, error):
    result = state.fail(state.new_result(), error)
    _context(result, identity, {})
    result["data"].update(admission_rejected=True, native_dispatched=False, mutation_state_unknown=False)
    return result


def set_blueprint_actor_tick_settings(blueprint_name, tick_hz=60.0, tick_interval=-1.0,
        can_ever_tick=True, start_with_tick_enabled=True, allow_tick_on_dedicated_server=True,
        tick_even_when_paused=False, dry_run=True, compile=True, save=True):
    try:
        arguments = _tick_arguments(blueprint_name, tick_hz, tick_interval, can_ever_tick,
            start_with_tick_enabled, allow_tick_on_dedicated_server, tick_even_when_paused, dry_run, compile, save)
    except Exception as error:
        return _rejected(TICK, error)
    return _mutate(TICK, arguments)


def set_blueprint_actor_tick_settings_action(result, blueprint_name, tick_hz=60.0, tick_interval=-1.0,
        can_ever_tick=True, start_with_tick_enabled=True, allow_tick_on_dedicated_server=True,
        tick_even_when_paused=False, dry_run=True, compile=True, save=True, checkpoint_callback=None):
    _run_action(result, TICK, _tick_arguments(blueprint_name, tick_hz, tick_interval, can_ever_tick,
        start_with_tick_enabled, allow_tick_on_dedicated_server, tick_even_when_paused, dry_run, compile, save), checkpoint_callback)


def set_skeletal_mesh_component_anim_defaults(blueprint_name, component_name,
        skeletal_mesh="", anim_class="", physics_asset=None, simulate_physics=None,
        collision_enabled=None, collision_response=None, enable_gravity=None,
        visibility_based_anim_tick_option=None, compile=True, save=True):
    try:
        arguments = _anim_arguments(blueprint_name, component_name, skeletal_mesh, anim_class,
            physics_asset, simulate_physics, collision_enabled, collision_response,
            enable_gravity, visibility_based_anim_tick_option, compile, save)
    except Exception as error:
        return _rejected(ANIM, error)
    return _mutate(ANIM, arguments)


def set_skeletal_mesh_component_anim_defaults_action(result, blueprint_name, component_name,
        skeletal_mesh="", anim_class="", physics_asset=None, simulate_physics=None,
        collision_enabled=None, collision_response=None, enable_gravity=None,
        visibility_based_anim_tick_option=None, compile=True, save=True, checkpoint_callback=None):
    _run_action(result, ANIM, _anim_arguments(blueprint_name, component_name, skeletal_mesh, anim_class,
        physics_asset, simulate_physics, collision_enabled, collision_response,
        enable_gravity, visibility_based_anim_tick_option, compile, save), checkpoint_callback)


def set_blueprint_actor_tick_settings_raw_action(result, blueprint_name, raw_settings, checkpoint_callback=None):
    """Private original-command presence carrier; Named10 remains unchanged."""
    fields = {"tick_hz", "tick_interval", "can_ever_tick", "start_with_tick_enabled",
        "allow_tick_on_dedicated_server", "tick_even_when_paused", "dry_run", "compile", "save", "refresh_nodes"}
    if type(raw_settings) is not dict or set(raw_settings) - fields:
        raise OperationError("blueprint_defaults_raw_tick_invalid", "Only original Tick settings fields admitted")
    try:
        encoded = json.dumps(raw_settings, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        if len(encoded.encode("utf-8")) > 131072:
            raise ValueError("private Tick carrier exceeds fixed 128KiB bound")
    except (TypeError, ValueError, OverflowError, UnicodeError, RecursionError) as error:
        raise OperationError("blueprint_defaults_raw_tick_invalid", "Finite bounded original Tick JSON required") from error
    # CDO-dependent defaults and SDK bool/number coercions stay Native-owned.
    arguments = {"blueprint_path": _path(blueprint_name), "settings_json": encoded}
    _run_action(result, TICK, arguments, checkpoint_callback, native_method="set_blueprint_actor_tick_settings_raw")
