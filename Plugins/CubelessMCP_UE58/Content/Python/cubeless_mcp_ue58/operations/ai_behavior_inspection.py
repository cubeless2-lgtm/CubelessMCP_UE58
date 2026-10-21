"""Original paired inspection identity over existing typed AI read accounting."""

import copy

from cubeless_mcp_ue58.compatibility.ue58 import ai_behavior_inspection as api
from . import ai_blackboard, ai_discovery, state
from .state import OperationError


INSPECT = "inspect_ai_behavior_assets"
DEFAULT_TREE = "/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase"
DEFAULT_BOARD = "/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase"
LEGACY_CHECKS = ("behavior_tree_loaded", "blackboard_loaded", "root_sequence", "root_selector",
    "two_children", "selector_chase_patrol", "selector_alert_chase_patrol",
    "chase_target_abort_lower_priority", "target_actor_key", "patrol_location_key",
    "mcp_dependency_audit", "editor_graph_present")


def _arguments(behavior_tree_path, blackboard_path, timeout_seconds):
    try:
        arguments = {"behavior_tree_path": ai_blackboard._text(behavior_tree_path, "behavior_tree_path"),
            "blackboard_path": ai_blackboard._text(blackboard_path, "blackboard_path"),
            "timeout_seconds": ai_blackboard._timeout(timeout_seconds)}
        # Reuse the concrete old literal-token guard used by the BH discovery
        # routes. It is not an asset loader or dependency audit of returned rows.
        ai_discovery._reference_guard(arguments)
        return arguments
    except OperationError as error:
        code = ("ai_behavior_inspection_timeout_invalid" if error.code == "ai_blackboard_timeout_invalid" else
            "ai_behavior_inspection_reference_unsupported" if error.code == "ai_discovery_reference_unsupported" else
            "ai_behavior_inspection_input_invalid")
        raise OperationError(code, str(error)) from error


def _context(result, arguments):
    result["data"].update(operation=INSPECT, requested_arguments=copy.deepcopy(arguments),
        caller_timeout_seconds=arguments["timeout_seconds"], native_game_thread_timeout_applied=False,
        adapter_load_calls=0, automatic_read_retry=False,
        behavior_tree_inspection_supported=True, supplied_blackboard_inspection_supported=True,
        requested_and_assigned_blackboards_need_not_match=True,
        false_legacy_checks_are_observations=True, full_legacy_paired_inspection_parity=False,
        runtime_behavior_tree_execution_verified=False, runtime_blackboard_values_verified=False,
        cold_persistence_verified=False, explicit_cache_refresh_requested=False,
        pre_entry_library_cdo_effects_independently_observed=False)


def _action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["stage"] = "native_paired_read"
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_state_unknown=True, read_outcome_unknown=True)
    try:
        native = api.inspect_ai_behavior_assets(arguments["behavior_tree_path"], arguments["blackboard_path"])
    except Exception as error:
        if getattr(error, "code", "") == "native_ai_behavior_inspection_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False, read_outcome_unknown=False)
        raise
    # Reuse the concrete existing typed AI copier; do not add another journal,
    # decoder, boolean-derived save count or pair of public calls here.
    ai_blackboard._apply_native(result, INSPECT, arguments, native)
    _context(result, arguments)
    result["data"].update(native_result_returned=True, read_outcome_unknown=not native["change_state_known"])
    # A paired unknown/effect is one aggregate observation. Neither resource
    # receives an invented changed boolean or is relabelled as the other.
    for change in result["changes"]:
        change.pop("asset_path", None)
        change.update(asset_paths=[arguments["behavior_tree_path"], arguments["blackboard_path"]],
            outcome="native_paired_inspection_observation" if native["change_state_known"] else "unknown_after_native_paired_read")
    if checkpoint_callback:
        checkpoint_callback(result, "returned_ai_behavior_inspection_read")
    if native["success"] is not True:
        raise OperationError(native.get("error_code") or "inspect_ai_behavior_assets_failed",
            native.get("error") or "Paired native inspection failed; actual load/read prefix is retained")
    if (not native["change_state_known"] or not native["postcondition_passed"] or native["mutation_attempted"]
            or native["changed"] or native["compile_requested"] or native["compile_attempted"]
            or native["compiled"] is not None or native["save_requested"] or native["save_attempted"] or native["saved"]):
        raise OperationError("ai_behavior_inspection_result_unverified", "Paired native success lacks its finite known no-writer postcondition; do not replay")
    checks = native["data"].get("checks")
    if type(checks) is not dict or any(type(checks.get(name)) is not bool for name in LEGACY_CHECKS):
        raise OperationError("ai_behavior_inspection_checks_incomplete", "Paired result lacks the original twelve typed observations; returned native data remains available")
    # False checks, audit issues and requested-BB mismatch are legitimate
    # observations. They never become inspection failure or runtime AI proof.
    result["validation"].update(status="native_paired_behavior_asset_inspection",
        legacy_checks_complete=True, runtime_behavior_tree_execution="not_run",
        runtime_blackboard_values="not_run", packaged_runtime="not_run")
    result["success"] = True
    result["stage"] = "complete"


def inspect_ai_behavior_assets(behavior_tree_path=DEFAULT_TREE, blackboard_path=DEFAULT_BOARD, timeout_seconds=60):
    result = state.new_result()
    try:
        arguments = _arguments(behavior_tree_path, blackboard_path, timeout_seconds)
    except Exception as error:
        state.fail(result, error)
        result["data"].update(operation=INSPECT, admission_rejected=True, native_dispatched=False,
            native_mutation_attempted=False, mutation_state_unknown=False, read_outcome_unknown=False)
        return result
    try:
        _action(result, arguments)
    except Exception as error:
        state.fail(result, error)
    return result


def inspect_ai_behavior_assets_action(result, behavior_tree_path=DEFAULT_TREE, blackboard_path=DEFAULT_BOARD,
        timeout_seconds=60, checkpoint_callback=None):
    _action(result, _arguments(behavior_tree_path, blackboard_path, timeout_seconds), checkpoint_callback)
