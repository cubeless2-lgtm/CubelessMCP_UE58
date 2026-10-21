"""Original observer6 and native nested prefix; no named mutation journal."""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import anim_state_machine_inspection as api
from . import state
from .state import OperationError

METHOD = "inspect_anim_state_machine_transitions"


def _arguments(blueprint_name, state_machine_name, include_pins, include_rule_graph_nodes,
        include_state_nodes, max_rule_graph_nodes):
    if type(blueprint_name) is not str or type(state_machine_name) is not str:
        raise OperationError("anim_state_machine_input_invalid", "Blueprint and filter must be literal strings")
    if any(type(value) is not bool for value in (include_pins, include_rule_graph_nodes, include_state_nodes)):
        raise OperationError("anim_state_machine_input_invalid", "Original include flags must be booleans")
    if type(max_rule_graph_nodes) is not int or not -(2**63) <= max_rule_graph_nodes < 2**63:
        raise OperationError("anim_state_machine_input_invalid", "Limit must fit the reflected int64 carrier; Native owns original clamp")
    return dict(blueprint_name=blueprint_name, state_machine_name=state_machine_name,
        include_pins=include_pins, include_rule_graph_nodes=include_rule_graph_nodes,
        include_state_nodes=include_state_nodes, max_rule_graph_nodes=max_rule_graph_nodes)


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_requested_observer=True, named_mutation_journal_created=False,
        native_load_PostLoad_title_cache_effects_not_global_purity_proof=True,
        native_original_filter_clamp_and_nested_payload_authoritative=True,
        automatic_action_retry=False, full_original_contract_complete=False,
        cold_persistence_verified=False, runtime_state_machine_evaluation_verified=False,
        external_original_pipeline_complete=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items()
            if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_observer_returned")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_observer_returned")
    result["changes"] = [] if known and native["changed"] is False and not native["mutation_attempted"] else [
        {"kind": "anim_state_machine_observation_scope", "attempted": native["mutation_attempted"],
            "change_state_known": known, "changed": native["changed"] if known else None,
            "outcome": "current_scoped_prefix" if known else "unknown_after_dispatch"}]
    result["stage"] = "returned_anim_state_machine_observation"
    result["validation"].update(status="native_anim_state_machine_postcondition",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        packaged_runtime="not_run", initial_load_effects="not_fully_instrumented")


def inspect_anim_state_machine_transitions_action(result, blueprint_name, state_machine_name="",
        include_pins=True, include_rule_graph_nodes=True, include_state_nodes=True,
        max_rule_graph_nodes=256, checkpoint_callback=None):
    arguments = _arguments(blueprint_name, state_machine_name, include_pins, include_rule_graph_nodes,
        include_state_nodes, max_rule_graph_nodes)
    _context(result, arguments)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        mutation_state_unknown=True, mutation_performed=None)
    result["changes"] = [{"kind": "anim_state_machine_observation_scope", "attempted": None,
        "change_state_known": False, "changed": None, "outcome": "unknown_until_native_returns"}]
    result["compile"].update(requested=False, attempted=None, compiled=None, status="unknown_until_native_returns")
    result["save"].update(requested=False, attempted=None, saved=None, status="unknown_until_native_returns")
    result["stage"] = "native_anim_state_machine_observation"
    try:
        native = api.inspect_anim_state_machine_transitions(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_anim_state_machine_observation"
            if checkpoint_callback:
                checkpoint_callback(result)
        if getattr(error, "code", "") == "native_anim_state_machine_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)  # All actual nested data/raw/flags precede criteria.
    if checkpoint_callback:
        checkpoint_callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_anim_state_machine_failed",
            native.get("error") or "Native state-machine prefix retained")
    if native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("anim_state_machine_unexpected_work", "Unexpected direct work retained; no second action")
    if not native["change_state_known"] or native["changed"] is not False or not native["postcondition_passed"]:
        raise OperationError("anim_state_machine_observation_unverified", "Current selected read proof incomplete; no replay")
    result.update(success=True, stage="complete")
    return result


def inspect_anim_state_machine_transitions(blueprint_name, state_machine_name="", include_pins=True,
        include_rule_graph_nodes=True, include_state_nodes=True, max_rule_graph_nodes=256):
    result = state.new_result()
    result["data"].update(operation=METHOD, native_dispatched=False, native_result_returned=False,
        mutation_state_unknown=False, mutation_performed=False)
    try:
        inspect_anim_state_machine_transitions_action(result, blueprint_name, state_machine_name,
            include_pins, include_rule_graph_nodes, include_state_nodes, max_rule_graph_nodes)
        result.update(success=True, stage="complete")
    except Exception as error:
        state.fail(result, error)
    return result
