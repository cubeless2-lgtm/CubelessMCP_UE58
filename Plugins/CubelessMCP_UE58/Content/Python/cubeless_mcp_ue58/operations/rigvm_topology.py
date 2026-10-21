"""Default RigVM topology11 in one Named journal or the existing Batch parent.

Native owns path precedence, asset/class resolution, graph projection and limits,
getter ordering, dirty observations, identity guards and all Engine callbacks.
Python neither resolves paths nor loads, edits, compiles, saves or solves a rig.
"""
import copy
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import rigvm_topology as api
from . import state
from .state import OperationError

METHOD = "inspect_controlrig_rigvm_topology"
PUBLIC_FIELDS = ("control_rig_path", "control_rig_asset_path", "control_rig_class_path",
    "include_pins", "include_sub_pins", "include_links", "include_pin_links",
    "max_nodes", "max_pins_per_node", "max_sub_pins_per_pin", "max_links")
PATH_FIELDS = PUBLIC_FIELDS[:3]
BOOL_FIELDS = PUBLIC_FIELDS[3:7]
LIMIT_FIELDS = PUBLIC_FIELDS[7:]


def _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        include_pins, include_sub_pins, include_links, include_pin_links,
        max_nodes, max_pins_per_node, max_sub_pins_per_pin, max_links):
    values = dict(control_rig_path=control_rig_path, control_rig_asset_path=control_rig_asset_path,
        control_rig_class_path=control_rig_class_path, include_pins=include_pins,
        include_sub_pins=include_sub_pins, include_links=include_links, include_pin_links=include_pin_links,
        max_nodes=max_nodes, max_pins_per_node=max_pins_per_node,
        max_sub_pins_per_pin=max_sub_pins_per_pin, max_links=max_links)
    for field in PATH_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("rigvm_topology_input_invalid", field + " must be a string; None is not an empty path")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("rigvm_topology_input_invalid", field + " must be a boolean")
    for field in LIMIT_FIELDS:
        if type(values[field]) is not int or not -(1 << 63) <= values[field] < (1 << 63):
            raise OperationError("rigvm_topology_input_invalid", field + " must be an int64 integer; booleans are not limits")
    # Keep empty strings and whitespace unchanged. Native alone selects aliases
    # and applies the original clamp4; do not normalize a path or clamp in Python.
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_eleven_public_parameters_forwarded_unchanged=True,
        native_path_precedence_clamps_projection_getter_and_dirty_observation_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_runtime_execute_calls=0, adapter_runtime_solve_calls=0,
        legacy_read_only_field_is_total_SDK_purity_proof=False,
        legacy_source_modified_false_is_total_SDK_purity_proof=False,
        dirty_unchanged_proves_all_SDK_effects_absent=False,
        explicit_compile_flags_prove_all_implicit_compiles=False,
        actual_named_topology_wire_verified=False,
        automatic_action_retry=False, atomic_rollback=False,
        runtime_pose_verified=False, playback_verified=False, unique_external_pipeline_complete=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_rigvm_topology_compile_receipt",
        completion_scope="Native_explicit_receipt; load_or_virtual_getter_implicit_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_rigvm_topology_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "asset_dirty_before": native["data"].get("asset_dirty_before"),
        "asset_dirty_after": native["data"].get("asset_dirty_after"),
        "observed_explicit_change": native["data"].get("observed_explicit_change"),
        "legacy_source_modified": native["data"].get("source_modified"),
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_default_RigVM_topology_postcondition",
        native_postcondition_passed=native["postcondition_passed"],
        requested_control_rig=native["data"].get("requested_control_rig"),
        resolution_kind=native["data"].get("resolution_kind"),
        graph_node_path=native["data"].get("graph_node_path"),
        included_node_count=native["data"].get("included_node_count"),
        included_link_count=native["data"].get("included_link_count"),
        nodes_truncated=native["data"].get("nodes_truncated"),
        links_truncated=native["data"].get("links_truncated"),
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        runtime_pose="not_run", playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_loader_getter_or_dirty_callback_effects_inspect_native_prefix_and_parent_journal_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_rigvm_topology_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_rigvm_topology_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_rigvm_topology_operation"
    try:
        native = api.inspect_controlrig_rigvm_topology(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "rigvm_topology_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_rigvm_topology_operation"
        callback(result) # Preserve returned malformed JSON or uncertain dispatch before any error.
        raise
    _adopt(result, arguments, native)
    callback(result) # Full Native topology/dirty/partial/raw/unknown prefix precedes error handling.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "rigvm_topology_native_failed",
            native.get("error") or "Native RigVM topology inspection failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("rigvm_topology_result_unverified", "Native topology postcondition is unverified; no replay")
    # A successful bounded inspection can still have unknown total SDK effects.
    # Preserve that state so Batch stops before any subsequent writer.
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["stage"] = "rigvm_topology_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False, journal_created=False)
    return result


def inspect_controlrig_rigvm_topology(control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        include_pins: bool = True, include_sub_pins: bool = True, include_links: bool = True, include_pin_links: bool = False,
        max_nodes: int = 256, max_pins_per_node: int = 64, max_sub_pins_per_pin: int = 32, max_links: int = 512) -> Dict[str, Any]:
    try:
        arguments = _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
            include_pins, include_sub_pins, include_links, include_pin_links,
            max_nodes, max_pins_per_node, max_sub_pins_per_pin, max_links)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def inspect_controlrig_rigvm_topology_action(result, control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        include_pins: bool = True, include_sub_pins: bool = True, include_links: bool = True, include_pin_links: bool = False,
        max_nodes: int = 256, max_pins_per_node: int = 64, max_sub_pins_per_pin: int = 32, max_links: int = 512, checkpoint_callback=None):
    return _run_action(result, _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        include_pins, include_sub_pins, include_links, include_pin_links,
        max_nodes, max_pins_per_node, max_sub_pins_per_pin, max_links), checkpoint_callback)
