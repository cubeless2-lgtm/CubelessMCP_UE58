"""Seven original AI names, finite native assets/subtrees and journals.

Editor logic and cache/source admission belong to the native EngineAPI. This
domain does not execute Python text, load an asset, spawn actors, or wait on GT.
"""

import copy
import math

from cubeless_mcp_ue58.compatibility.ue58 import ai_blackboard as api
from . import state, variables
from .state import OperationError


CREATE = "create_ai_blackboard_asset"
ADD = "add_ai_blackboard_key"
INSPECT = "inspect_ai_asset_structure"
CREATE_BEHAVIOR_TREE = "create_ai_behavior_tree_asset"
SET_BEHAVIOR_TREE_BLACKBOARD = "set_ai_behavior_tree_blackboard"
BEHAVIOR_TREE_MUTATIONS = (CREATE_BEHAVIOR_TREE, SET_BEHAVIOR_TREE_BLACKBOARD)
CREATE_EQS = "create_ai_eqs_query"
CREATE_BEHAVIOR_ASSETS = "create_ai_patrol_chase_behavior_assets"
CONFIGURE_PERCEPTION = "configure_ai_controller_perception"
REMOVE = "remove_ai_blackboard_key"


def _text(value, field, *, optional=False, empty=False):
    try:
        return variables._text("" if optional and value is None else value, field, empty=optional or empty)
    except OperationError as error:
        raise OperationError("ai_blackboard_input_invalid", str(error)) from error


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("ai_blackboard_input_invalid", field + " requires an exact boolean")
    return value


def _timeout(value):
    if type(value) is not int or not 1 <= value <= 2**31 - 1:
        raise OperationError("ai_blackboard_timeout_invalid", "timeout_seconds requires a positive signed-int32 integer; it is caller context, not a native game-thread timeout")
    return value


def _create_arguments(asset_name, asset_root, save_asset, timeout_seconds):
    return {"asset_name": _text(asset_name, "asset_name"),
        "asset_root": _text(asset_root, "asset_root"),
        "save_asset": _bool(save_asset, "save_asset"), "timeout_seconds": _timeout(timeout_seconds)}


def _add_arguments(blackboard_path, key_name, key_type, base_class, enum_path,
        instance_synced, save_asset, timeout_seconds):
    # Preserve native fresh-versus-duplicate whitespace/reference semantics.
    # Native validates key types/references; do not coerce or trim them here.
    return {"blackboard_path": _text(blackboard_path, "blackboard_path"),
        "key_name": _text(key_name, "key_name"), "key_type": _text(key_type, "key_type", empty=True),
        "base_class": _text(base_class, "base_class", optional=True),
        "enum_path": _text(enum_path, "enum_path", optional=True),
        "instance_synced": _bool(instance_synced, "instance_synced"),
        "save_asset": _bool(save_asset, "save_asset"), "timeout_seconds": _timeout(timeout_seconds)}


def _inspect_arguments(asset_path, timeout_seconds):
    return {"asset_path": _text(asset_path, "asset_path"), "timeout_seconds": _timeout(timeout_seconds)}


def _context(result, identity, arguments):
    result["data"].update(operation=identity, caller_timeout_seconds=arguments["timeout_seconds"],
        native_game_thread_timeout_applied=False, adapter_load_calls=0,
        full_legacy_ai_parity=False, full_legacy_blackboard_parity=False,
        behavior_tree_inspection_supported=False, eqs_inspection_supported=False,
        runtime_blackboard_values_verified=False, runtime_synchronization_verified=False,
        hidden_cache_callback_effects_independently_verified=False)
    if _is_behavior_tree(result, identity):
        result["data"].update(behavior_tree_inspection_supported=True,
            full_legacy_behavior_tree_parity=False, runtime_behavior_tree_execution_verified=False,
            hidden_graph_callback_effects_independently_verified=False)
    if _is_eqs(result, identity):
        result["data"].update(eqs_inspection_supported=True,
            full_legacy_eqs_parity=False, runtime_eqs_execution_verified=False,
            eqs_option_generator_test_authoring_verified=False,
            hidden_query_callback_effects_independently_verified=False)
    if identity == CREATE_BEHAVIOR_ASSETS:
        result["data"].update(full_legacy_behavior_assets_parity=False,
            runtime_behavior_tree_execution_verified=False,
            hidden_graph_callback_effects_independently_verified=False,
            existing_target_replacement_verified=False)
    if identity == CONFIGURE_PERCEPTION:
        result["data"].update(full_legacy_perception_parity=False,
            runtime_perception_execution_verified=False,
            hidden_perception_callback_effects_independently_verified=False)
    if identity == REMOVE:
        result["data"].update(full_legacy_blackboard_remove_parity=False,
            behavior_tree_consumer_state_independently_observed=False)


def _is_behavior_tree(result, identity):
    return identity in BEHAVIOR_TREE_MUTATIONS or result["data"].get("asset_type") == "behavior_tree"


def _is_eqs(result, identity):
    return identity == CREATE_EQS or result["data"].get("asset_type") == "eqs"


def _rejected(error, operation_id=None):
    result = state.fail(state.new_result(operation_id), error)
    result["data"].update(admission_rejected=True, native_dispatched=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
        full_legacy_ai_parity=False, full_legacy_blackboard_parity=False)
    return result


def _apply_native(result, identity, arguments, native):
    # Keep all returned legacy fields, cache/load observations and save counts
    # before any success/error assertion. Native owns their observation scope.
    result["data"].update(copy.deepcopy(native["data"]))
    _context(result, identity, arguments)
    known = native["change_state_known"]
    result["data"].update(native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None)
    result["stage"] = native.get("stage", "native_ai_blackboard_operation")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="not_requested" if not native["compile_requested"] else "requested_completion_unobserved")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    target = native["data"].get("asset_path", arguments.get("blackboard_path", arguments.get("asset_path")))
    no_work = known and native.get("changed") is False and not native["mutation_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": identity, "asset_path": target,
        "attempted": native["mutation_attempted"], "changed": native.get("changed") if known else None,
        "change_state_known": known, "outcome": "native_blackboard_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_blackboard_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        runtime_blackboard_values="not_run", runtime_synchronization="not_run", packaged_runtime="not_run")
    if _is_behavior_tree(result, identity):
        # Reuse the concrete returned-outcome copier above. All native snapshots,
        # ordered save_calls and save_attempt_count remain opaque observations;
        # a boolean saved flag is not converted to an invented attempt count.
        if "stage" not in native:
            result["stage"] = "native_ai_behavior_tree_operation"
        target = native["data"].get("asset_path", arguments.get("behavior_tree_path", arguments.get("asset_path")))
        for change in result["changes"]:
            change.update(asset_path=target, outcome="native_behavior_tree_observation" if known else "unknown_after_native_dispatch")
        result["validation"].update(status="native_behavior_tree_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
            runtime_behavior_tree_execution="not_run")
    if _is_eqs(result, identity):
        if "stage" not in native:
            result["stage"] = "native_ai_env_query_operation"
        for change in result["changes"]:
            change.update(outcome="native_env_query_observation" if known else "unknown_after_native_dispatch")
        result["validation"].update(status="native_env_query_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
            runtime_eqs_execution="not_run", eqs_option_generator_test_authoring="not_run")
    if identity == CREATE_BEHAVIOR_ASSETS:
        # One native aggregate observation covers both resources. Preserve
        # actual per-asset creation/save flags and ordered save_calls in data;
        # do not derive per-asset changed booleans or save counts from saved.
        result["changes"] = [] if no_work else [{"kind": identity,
            "asset_paths": [path for path in (native["data"].get("blackboard_path"),
                native["data"].get("behavior_tree_path")) if path],
            "attempted": native["mutation_attempted"], "changed": native.get("changed") if known else None,
            "change_state_known": known,
            "outcome": "native_behavior_assets_observation" if known else "unknown_after_native_dispatch"}]
        result["validation"].update(status="native_behavior_assets_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
            runtime_behavior_tree_execution="not_run")

        # This is only this invocation's scoped Native replacement postcondition.
        # Full legacy parity, runtime and hidden-callback flags stay conservative.
        replacement = native["data"].get("replacement")
        result["data"]["existing_target_replacement_verified"] = (
            arguments.get("replace_existing") is True
            and native.get("success") is True and known is True
            and native.get("changed") is True and native.get("mutation_attempted") is True
            and native.get("postcondition_passed") is True and native.get("stage") == "complete"
            and native["data"].get("existing_target_replacement_supported") is True
            and native["data"].get("prior_asset_deleted") is True
            and isinstance(replacement, dict)
            and replacement.get("ordered_deletion_complete") is True
        )

    if identity == CONFIGURE_PERCEPTION:
        target = native["data"].get("controller_blueprint_path", arguments["controller_blueprint"])
        for change in result["changes"]:
            change.update(asset_path=target, outcome="native_perception_configuration_observation" if known else "unknown_after_native_dispatch")
        result["compile"]["status"] = ("compiled" if native["compiled"] is True else
            "failed" if native["compiled"] is False and native["compile_attempted"] else
            "attempted_completion_unobserved" if native["compile_attempted"] else
            "requested_not_attempted" if native["compile_requested"] else "not_requested")
        result["validation"].update(status="native_perception_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
            runtime_perception_execution="not_run")


def _run_action(result, identity, arguments, checkpoint_callback=None):
    if identity not in (CREATE, ADD, INSPECT, *BEHAVIOR_TREE_MUTATIONS, CREATE_EQS, CREATE_BEHAVIOR_ASSETS, CONFIGURE_PERCEPTION, REMOVE):
        raise OperationError("ai_blackboard_operation_invalid", "Only the nine named finite AI leaves are admitted")
    mutation = identity != INSPECT
    behavior_tree = identity in BEHAVIOR_TREE_MUTATIONS
    eqs = identity == CREATE_EQS
    behavior_assets = identity == CREATE_BEHAVIOR_ASSETS
    perception = identity == CONFIGURE_PERCEPTION
    _context(result, identity, arguments)
    checkpoint = checkpoint_callback or (state.checkpoint if mutation else None)
    if mutation:
        result["data"]["pending_mutation"] = {"operation": identity, **arguments,
            "dispatch_state_at_checkpoint": "not_yet_dispatched",
            "interrupted_outcome": "unknown_inspect_controller_perception_and_journal" if perception else
                "unknown_inspect_both_behavior_assets_and_journal" if behavior_assets else
                "unknown_inspect_eqs_and_journal" if eqs else
                "unknown_inspect_behavior_tree_and_journal" if behavior_tree else "unknown_inspect_blackboard_and_journal"}
        result["stage"] = "before_mutation_checkpoint"
        checkpoint(result)
    result["stage"] = "native_ai_env_query_operation" if eqs else "native_ai_behavior_tree_operation" if behavior_tree else "native_ai_blackboard_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "asset_path": arguments.get("behavior_tree_path") if behavior_tree else arguments.get("blackboard_path", arguments.get("asset_path")),
        "attempted": None, "changed": None, "change_state_known": False, "outcome": "unknown_until_native_result"}]
    if behavior_assets:
        result["stage"] = "native_ai_behavior_assets_operation"
        result["changes"] = [{"kind": identity, "asset_root": arguments["asset_root"],
            "behavior_tree_name": arguments["behavior_tree_name"], "blackboard_name": arguments["blackboard_name"],
            "attempted": None, "changed": None, "change_state_known": False, "outcome": "unknown_until_native_result"}]
    if perception:
        result["stage"] = "native_ai_perception_operation"
        for change in result["changes"]:
            change.update(asset_path=arguments["controller_blueprint"])
    native_arguments = {key: value for key, value in arguments.items() if key != "timeout_seconds"}
    try:
        native = getattr(api, identity)(**native_arguments)
    except Exception as error:
        if getattr(error, "code", "") in ("native_ai_blackboard_api_unavailable", "native_ai_behavior_tree_api_unavailable", "native_ai_env_query_api_unavailable", "native_ai_behavior_assets_api_unavailable", "native_ai_perception_api_unavailable"):
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    behavior_tree = _is_behavior_tree(result, identity)
    eqs = _is_eqs(result, identity)
    result["data"]["native_result_returned"] = True
    if checkpoint is not None:
        if behavior_assets:
            # This exact producer checkpoints a returned two-resource/save
            # prefix. The matching batch hook must retain it as observation,
            # rather than rewriting it into pre-dispatch intent.
            result["stage"] = "returned_ai_behavior_assets_operation"
        if perception:
            result["stage"] = "returned_ai_perception_operation"
        if identity == REMOVE:
            result["stage"] = "returned_ai_blackboard_remove_operation"
        checkpoint(result)
        if identity == REMOVE:
            result["stage"] = native.get("stage", "native_ai_blackboard_operation")
        if perception:
            result["stage"] = native.get("stage", "native_ai_perception_operation")
        if behavior_assets:
            result["stage"] = native.get("stage", "native_ai_behavior_assets_operation")
    if perception:
        # This original leaf has one full compile, unlike the seven asset-only
        # routes. Copy actual structural/full-compile/save prefixes first; a
        # failed compile or save must never be hidden by a generic no-compile guard.
        if not native["success"]:
            raise OperationError(native.get("error_code") or "native_ai_perception_operation_failed",
                native.get("error") or "Native perception operation failed; actual returned prefix is retained")
        if native["change_state_known"] is not True or native["postcondition_passed"] is not True:
            raise OperationError("ai_perception_result_unverified", "Native success lacks known perception postconditions; do not replay")
        if native["compile_requested"] is not True or native["compile_attempted"] is not True or native["compiled"] is not True:
            raise OperationError("ai_perception_compile_unverified", "Successful configuration lacks the original actual full compile; inspect the retained result")
        result.update(success=True, stage="complete")
        result["retry"]["duplicate_risk"] = "perception_configuration_structural_compile_or_save_would_repeat"
        return
    if not native["success"]:
        raise OperationError(native.get("error_code") or ("native_ai_behavior_assets_operation_failed" if behavior_assets else "native_ai_env_query_operation_failed" if eqs else "native_ai_behavior_tree_operation_failed" if behavior_tree else "native_ai_blackboard_operation_failed"),
            native.get("error") or ("Native behavior assets operation failed; actual returned prefix is retained" if behavior_assets else "Native EQS operation failed; actual returned prefix is retained" if eqs else "Native Behavior Tree operation failed; actual returned prefix is retained" if behavior_tree else "Native Blackboard operation failed; actual returned prefix is retained"))
    if native["change_state_known"] is not True or native["postcondition_passed"] is not True:
        raise OperationError("ai_behavior_assets_result_unverified" if behavior_assets else "ai_eqs_result_unverified" if eqs else "ai_behavior_tree_result_unverified" if behavior_tree else "ai_blackboard_result_unverified",
            "Native success lacks known behavior asset postconditions; do not replay" if behavior_assets else "Native success lacks known EQS postconditions; do not replay" if eqs else "Native success lacks known Behavior Tree postconditions; do not replay" if behavior_tree else "Native success lacks known Blackboard postconditions; do not replay")
    if native["compile_requested"] or native["compile_attempted"]:
        raise OperationError("ai_behavior_assets_unexpected_compile" if behavior_assets else "ai_eqs_unexpected_compile" if eqs else "ai_behavior_tree_unexpected_compile" if behavior_tree else "ai_blackboard_unexpected_compile",
            "Behavior assets operation unexpectedly requested explicit compilation; inspect the retained result" if behavior_assets else "EQS operation unexpectedly requested explicit compilation; inspect the retained result" if eqs else "Behavior Tree operation unexpectedly requested explicit compilation; inspect the retained result" if behavior_tree else "Blackboard operation unexpectedly requested explicit compilation; inspect the retained result")
    result.update(success=True, stage="complete")
    repeated = native["mutation_attempted"] or native["save_attempted"] or native.get("changed") is True
    result["retry"]["duplicate_risk"] = "blackboard_cache_or_action_would_repeat" if repeated else "none_observed"
    if behavior_tree and repeated:
        result["retry"]["duplicate_risk"] = "behavior_tree_graph_callback_or_action_would_repeat"
    if eqs and repeated:
        result["retry"]["duplicate_risk"] = "env_query_lifecycle_or_action_would_repeat"
    if behavior_assets and repeated:
        result["retry"]["duplicate_risk"] = "both_behavior_assets_graph_cache_or_save_would_repeat"


def create_ai_blackboard_asset(asset_name, asset_root="/Game/AI", save_asset=True,
        timeout_seconds=60, operation_id=None):
    try:
        arguments = _create_arguments(asset_name, asset_root, save_asset, timeout_seconds)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(CREATE, arguments, operation_id, lambda result: _run_action(result, CREATE, arguments))


def create_ai_blackboard_asset_action(result, asset_name, asset_root="/Game/AI", save_asset=True,
        timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, CREATE, _create_arguments(asset_name, asset_root, save_asset, timeout_seconds), checkpoint_callback)


def add_ai_blackboard_key(blackboard_path, key_name, key_type, base_class="", enum_path="",
        instance_synced=False, save_asset=True, timeout_seconds=60, operation_id=None):
    try:
        arguments = _add_arguments(blackboard_path, key_name, key_type, base_class, enum_path,
            instance_synced, save_asset, timeout_seconds)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(ADD, arguments, operation_id, lambda result: _run_action(result, ADD, arguments))


def add_ai_blackboard_key_action(result, blackboard_path, key_name, key_type, base_class="", enum_path="",
        instance_synced=False, save_asset=True, timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, ADD, _add_arguments(blackboard_path, key_name, key_type, base_class, enum_path,
        instance_synced, save_asset, timeout_seconds), checkpoint_callback)


def inspect_ai_asset_structure(asset_path, timeout_seconds=60):
    result = state.new_result()
    try:
        _run_action(result, INSPECT, _inspect_arguments(asset_path, timeout_seconds))
    except Exception as error:
        state.fail(result, error)
    return result


def inspect_ai_asset_structure_action(result, asset_path, timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, INSPECT, _inspect_arguments(asset_path, timeout_seconds), checkpoint_callback)


def _behavior_tree_input_error(error):
    code = "ai_behavior_tree_timeout_invalid" if error.code == "ai_blackboard_timeout_invalid" else "ai_behavior_tree_input_invalid"
    return OperationError(code, str(error))


def _create_behavior_tree_arguments(asset_name, asset_root, blackboard_path, save_asset, timeout_seconds):
    try:
        return {"asset_name": _text(asset_name, "asset_name"),
            "asset_root": _text(asset_root, "asset_root"),
            "blackboard_path": _text(blackboard_path, "blackboard_path", optional=True),
            "save_asset": _bool(save_asset, "save_asset"), "timeout_seconds": _timeout(timeout_seconds)}
    except OperationError as error:
        raise _behavior_tree_input_error(error) from error


def _set_behavior_tree_arguments(behavior_tree_path, blackboard_path, save_asset, timeout_seconds):
    try:
        return {"behavior_tree_path": _text(behavior_tree_path, "behavior_tree_path"),
            "blackboard_path": _text(blackboard_path, "blackboard_path"),
            "save_asset": _bool(save_asset, "save_asset"), "timeout_seconds": _timeout(timeout_seconds)}
    except OperationError as error:
        raise _behavior_tree_input_error(error) from error


def _rejected_behavior_tree(error, operation_id=None):
    result = _rejected(error, operation_id)
    result["data"].update(full_legacy_behavior_tree_parity=False,
        runtime_behavior_tree_execution_verified=False)
    return result


def create_ai_behavior_tree_asset(asset_name, asset_root="/Game/AI", blackboard_path="",
        save_asset=True, timeout_seconds=60, operation_id=None):
    try:
        arguments = _create_behavior_tree_arguments(asset_name, asset_root, blackboard_path, save_asset, timeout_seconds)
    except Exception as error:
        return _rejected_behavior_tree(error, operation_id)
    return state.mutate(CREATE_BEHAVIOR_TREE, arguments, operation_id,
        lambda result: _run_action(result, CREATE_BEHAVIOR_TREE, arguments))


def create_ai_behavior_tree_asset_action(result, asset_name, asset_root="/Game/AI", blackboard_path="",
        save_asset=True, timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, CREATE_BEHAVIOR_TREE,
        _create_behavior_tree_arguments(asset_name, asset_root, blackboard_path, save_asset, timeout_seconds), checkpoint_callback)


def set_ai_behavior_tree_blackboard(behavior_tree_path, blackboard_path, save_asset=True,
        timeout_seconds=60, operation_id=None):
    try:
        arguments = _set_behavior_tree_arguments(behavior_tree_path, blackboard_path, save_asset, timeout_seconds)
    except Exception as error:
        return _rejected_behavior_tree(error, operation_id)
    return state.mutate(SET_BEHAVIOR_TREE_BLACKBOARD, arguments, operation_id,
        lambda result: _run_action(result, SET_BEHAVIOR_TREE_BLACKBOARD, arguments))


def set_ai_behavior_tree_blackboard_action(result, behavior_tree_path, blackboard_path, save_asset=True,
        timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, SET_BEHAVIOR_TREE_BLACKBOARD,
        _set_behavior_tree_arguments(behavior_tree_path, blackboard_path, save_asset, timeout_seconds), checkpoint_callback)


def _create_eqs_arguments(asset_name, asset_root, save_asset, timeout_seconds):
    try:
        # Reuse only the concrete existing name/root/bool/timeout parser.
        # Native owns EQS path/class/lifecycle validation; no Python load.
        return _create_arguments(asset_name, asset_root, save_asset, timeout_seconds)
    except OperationError as error:
        code = "ai_eqs_timeout_invalid" if error.code == "ai_blackboard_timeout_invalid" else "ai_eqs_input_invalid"
        raise OperationError(code, str(error)) from error


def _rejected_eqs(error, operation_id=None):
    result = _rejected(error, operation_id)
    result["data"].update(full_legacy_eqs_parity=False, runtime_eqs_execution_verified=False,
        eqs_option_generator_test_authoring_verified=False)
    return result


def create_ai_eqs_query(asset_name, asset_root="/Game/AI/EQS", save_asset=True,
        timeout_seconds=60, operation_id=None):
    try:
        arguments = _create_eqs_arguments(asset_name, asset_root, save_asset, timeout_seconds)
    except Exception as error:
        return _rejected_eqs(error, operation_id)
    return state.mutate(CREATE_EQS, arguments, operation_id,
        lambda result: _run_action(result, CREATE_EQS, arguments))


def create_ai_eqs_query_action(result, asset_name, asset_root="/Game/AI/EQS", save_asset=True,
        timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, CREATE_EQS,
        _create_eqs_arguments(asset_name, asset_root, save_asset, timeout_seconds), checkpoint_callback)


def _create_behavior_assets_arguments(asset_root, behavior_tree_name, blackboard_name,
        target_actor_key, patrol_location_key, tree_shape, acceptable_radius,
        alert_wait_seconds, face_precision_degrees, replace_existing, save_assets, timeout_seconds):
    try:
        arguments = {name: _text(value, name) for name, value in (
            ("asset_root", asset_root), ("behavior_tree_name", behavior_tree_name), ("blackboard_name", blackboard_name),
            ("target_actor_key", target_actor_key), ("patrol_location_key", patrol_location_key), ("tree_shape", tree_shape))}
        for name, value in (("acceptable_radius", acceptable_radius), ("alert_wait_seconds", alert_wait_seconds),
                ("face_precision_degrees", face_precision_degrees)):
            try:
                finite = type(value) in (int, float) and math.isfinite(value)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("ai_behavior_assets_input_invalid", name + " requires a finite numeric value, excluding bool")
            # Preserve the requested number in the journal. Native owns exact
            # float representability and the original clamps; no Python clamp.
            arguments[name] = value
        arguments.update(replace_existing=_bool(replace_existing, "replace_existing"),
            save_assets=_bool(save_assets, "save_assets"), timeout_seconds=_timeout(timeout_seconds))
        return arguments
    except OperationError as error:
        code = "ai_behavior_assets_timeout_invalid" if error.code == "ai_blackboard_timeout_invalid" else "ai_behavior_assets_input_invalid"
        raise OperationError(code, str(error)) from error


def _rejected_behavior_assets(error, operation_id=None):
    result = _rejected(error, operation_id)
    result["data"].update(full_legacy_behavior_assets_parity=False,
        runtime_behavior_tree_execution_verified=False, existing_target_replacement_verified=False)
    return result


def create_ai_patrol_chase_behavior_assets(asset_root="/Game/Cubeless/AI/Smoke",
        behavior_tree_name="BT_AIDummyPatrolChase", blackboard_name="BB_AIDummyPatrolChase",
        target_actor_key="TargetActor", patrol_location_key="PatrolLocation", tree_shape="sequence_patrol_chase",
        acceptable_radius=55.0, alert_wait_seconds=0.5, face_precision_degrees=12.0,
        replace_existing=False, save_assets=False, timeout_seconds=60, operation_id=None):
    try:
        arguments = _create_behavior_assets_arguments(asset_root, behavior_tree_name, blackboard_name,
            target_actor_key, patrol_location_key, tree_shape, acceptable_radius,
            alert_wait_seconds, face_precision_degrees, replace_existing, save_assets, timeout_seconds)
    except Exception as error:
        return _rejected_behavior_assets(error, operation_id)
    return state.mutate(CREATE_BEHAVIOR_ASSETS, arguments, operation_id,
        lambda result: _run_action(result, CREATE_BEHAVIOR_ASSETS, arguments))


def create_ai_patrol_chase_behavior_assets_action(result, asset_root="/Game/Cubeless/AI/Smoke",
        behavior_tree_name="BT_AIDummyPatrolChase", blackboard_name="BB_AIDummyPatrolChase",
        target_actor_key="TargetActor", patrol_location_key="PatrolLocation", tree_shape="sequence_patrol_chase",
        acceptable_radius=55.0, alert_wait_seconds=0.5, face_precision_degrees=12.0,
        replace_existing=False, save_assets=False, timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, CREATE_BEHAVIOR_ASSETS,
        _create_behavior_assets_arguments(asset_root, behavior_tree_name, blackboard_name,
            target_actor_key, patrol_location_key, tree_shape, acceptable_radius,
            alert_wait_seconds, face_precision_degrees, replace_existing, save_assets, timeout_seconds), checkpoint_callback)


def _configure_perception_arguments(controller_blueprint, component_name, sight_radius,
        lose_sight_radius, peripheral_vision_angle_degrees, max_age, detect_enemies,
        detect_neutrals, detect_friendlies, replace_existing_config, save_blueprint, timeout_seconds):
    try:
        arguments = {"controller_blueprint": _text(controller_blueprint, "controller_blueprint"),
            "component_name": _text(component_name, "component_name")}
        for name, value in (("sight_radius", sight_radius), ("lose_sight_radius", lose_sight_radius),
                ("peripheral_vision_angle_degrees", peripheral_vision_angle_degrees), ("max_age", max_age)):
            try:
                finite = type(value) in (int, float) and math.isfinite(value)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("ai_perception_input_invalid", name + " requires a finite numeric value, excluding bool")
            # Keep the raw request/fingerprint. Native owns float conversion,
            # original clamps and stored/effective age-zero sentinel semantics.
            arguments[name] = value
        arguments.update(detect_enemies=_bool(detect_enemies, "detect_enemies"),
            detect_neutrals=_bool(detect_neutrals, "detect_neutrals"),
            detect_friendlies=_bool(detect_friendlies, "detect_friendlies"),
            replace_existing_config=_bool(replace_existing_config, "replace_existing_config"),
            save_blueprint=_bool(save_blueprint, "save_blueprint"), timeout_seconds=_timeout(timeout_seconds))
        return arguments
    except OperationError as error:
        code = "ai_perception_timeout_invalid" if error.code == "ai_blackboard_timeout_invalid" else "ai_perception_input_invalid"
        raise OperationError(code, str(error)) from error


def _rejected_perception(error, operation_id=None):
    result = _rejected(error, operation_id)
    result["data"].update(full_legacy_perception_parity=False, runtime_perception_execution_verified=False,
        hidden_perception_callback_effects_independently_verified=False)
    return result


def configure_ai_controller_perception(controller_blueprint="/Game/Cubeless/AI/BP_AIBot_Controller.BP_AIBot_Controller",
        component_name="AIPerception", sight_radius=900.0, lose_sight_radius=1150.0,
        peripheral_vision_angle_degrees=90.0, max_age=3.0, detect_enemies=True,
        detect_neutrals=True, detect_friendlies=True, replace_existing_config=True,
        save_blueprint=False, timeout_seconds=60, operation_id=None):
    try:
        arguments = _configure_perception_arguments(controller_blueprint, component_name, sight_radius,
            lose_sight_radius, peripheral_vision_angle_degrees, max_age, detect_enemies,
            detect_neutrals, detect_friendlies, replace_existing_config, save_blueprint, timeout_seconds)
    except Exception as error:
        return _rejected_perception(error, operation_id)
    return state.mutate(CONFIGURE_PERCEPTION, arguments, operation_id,
        lambda result: _run_action(result, CONFIGURE_PERCEPTION, arguments))


def configure_ai_controller_perception_action(result, controller_blueprint="/Game/Cubeless/AI/BP_AIBot_Controller.BP_AIBot_Controller",
        component_name="AIPerception", sight_radius=900.0, lose_sight_radius=1150.0,
        peripheral_vision_angle_degrees=90.0, max_age=3.0, detect_enemies=True,
        detect_neutrals=True, detect_friendlies=True, replace_existing_config=True,
        save_blueprint=False, timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, CONFIGURE_PERCEPTION,
        _configure_perception_arguments(controller_blueprint, component_name, sight_radius, lose_sight_radius,
            peripheral_vision_angle_degrees, max_age, detect_enemies, detect_neutrals, detect_friendlies,
            replace_existing_config, save_blueprint, timeout_seconds), checkpoint_callback)


def _remove_arguments(blackboard_path, key_name, save_asset, timeout_seconds):
    return {"blackboard_path": _text(blackboard_path, "blackboard_path"),
        "key_name": _text(key_name, "key_name"), "save_asset": _bool(save_asset, "save_asset"),
        "timeout_seconds": _timeout(timeout_seconds)}


def remove_ai_blackboard_key(blackboard_path, key_name, save_asset=True, timeout_seconds=60, operation_id=None):
    try:
        arguments = _remove_arguments(blackboard_path, key_name, save_asset, timeout_seconds)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(REMOVE, arguments, operation_id,
        lambda result: _run_action(result, REMOVE, arguments))


def remove_ai_blackboard_key_action(result, blackboard_path, key_name, save_asset=True, timeout_seconds=60, checkpoint_callback=None):
    _run_action(result, REMOVE, _remove_arguments(blackboard_path, key_name, save_asset, timeout_seconds), checkpoint_callback)
