"""Original eight-input local-SCS replacement with one existing journal.

Native owns prewrite admission, preview/actual phases, retirement, compile and
forced save observations. Python does not load, rewrite graphs or retry work.
"""
import copy
import math

from cubeless_mcp_ue58.compatibility.ue58 import component_replacement as api
from . import component, state
from .state import OperationError


IDENTITY = "replace_blueprint_component_class"
_FORBIDDEN = ("unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools",
    "/cubelessmcp_ue58/", "/script/cubelessmcpue58editor")


def _text(value, field):
    value = component._selector(value, field)
    if any(token in value.replace("\\", "/").lower() for token in _FORBIDDEN):
        raise OperationError("component_replace_reference_unsupported", "MCP-only reference in " + field + "; no replacement dispatch ran")
    return value


def _arguments(blueprint_name, component_name, new_component_class, sphere_radius,
        dry_run, compile, save, refresh_nodes):
    _text(blueprint_name, "blueprint_name")
    if type(sphere_radius) not in (int, float):
        raise OperationError("component_replace_radius_invalid", "sphere_radius requires a finite number, not bool or coercible text")
    try:
        radius = float(sphere_radius)
    except (ValueError, OverflowError) as error:
        raise OperationError("component_replace_radius_invalid", "sphere_radius cannot be represented as a finite double") from error
    if not math.isfinite(radius):
        raise OperationError("component_replace_radius_invalid", "sphere_radius requires a finite number")
    flags = {"dry_run": dry_run, "compile": compile, "save": save, "refresh_nodes": refresh_nodes}
    if any(type(value) is not bool for value in flags.values()):
        raise OperationError("component_replace_inputs_invalid", "Original lifecycle flags require exact booleans")
    return {"blueprint_path": component._mutation_path(blueprint_name),
        "component_name": _text(component_name, "component_name"),
        "new_component_class": _text(new_component_class, "new_component_class"),
        "sphere_radius": radius, **flags}


def _context(result, arguments):
    result["data"].update(operation=IDENTITY, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, full_legacy_component_replacement_parity=False,
        cold_persistence_verified=False, runtime_component_behavior_verified=False,
        library_cdo_before_native_entry_independently_observed=False)


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None,
        actual_work_counts_authority="returned_native_counters_and_ordered_save_calls")
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_result" if native["compile_requested"] else "not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": IDENTITY, "asset_path": arguments["blueprint_path"],
        "component_name": arguments["component_name"], "attempted": native["mutation_attempted"],
        "change_state_known": known, "changed": native.get("changed") if known else None,
        "outcome": "native_replacement_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_replacement_postcondition" if native["postcondition_passed"] else "native_replacement_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run", packaged_runtime="not_run")
    result["stage"] = "returned_component_replacement_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    callback = checkpoint_callback or state.checkpoint
    result["data"]["pending_mutation"] = {"operation": IDENTITY, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_scs_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_component_replacement_operation"
    result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": IDENTITY, "asset_path": arguments["blueprint_path"], "component_name": arguments["component_name"],
        "attempted": None, "change_state_known": False, "changed": None, "outcome": "unknown_until_native_response"}]
    try:
        native = (api.replace_blueprint_component_class_raw if "require_scene_compatibility" in arguments else api.replace_blueprint_component_class)(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_component_replacement_operation"
            callback(result)
        if getattr(error, "code", "") in ("native_component_replacement_api_unavailable", "native_component_replacement_raw_api_unavailable"):
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_component_replacement_failed", native.get("error") or "Replacement failed; returned prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("component_replace_result_unverified", "Native success lacks known replacement/compile/save postconditions; do not replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "replacement_allocates_again_inspect_journal" if native["mutation_attempted"] else "none_observed"


def replace_blueprint_component_class(blueprint_name, component_name, new_component_class,
        sphere_radius=-1.0, dry_run=True, compile=True, save=True, refresh_nodes=True):
    try:
        arguments = _arguments(blueprint_name, component_name, new_component_class, sphere_radius,
            dry_run, compile, save, refresh_nodes)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        _context(result, {})
        result["data"].update(admission_rejected=True, native_dispatched=False, mutation_state_unknown=False)
        return result
    return state.mutate(IDENTITY, arguments, None, lambda result: _run_action(result, arguments))


def replace_blueprint_component_class_action(result, blueprint_name, component_name, new_component_class,
        sphere_radius=-1.0, dry_run=True, compile=True, save=True, refresh_nodes=True, checkpoint_callback=None):
    _run_action(result, _arguments(blueprint_name, component_name, new_component_class, sphere_radius,
        dry_run, compile, save, refresh_nodes), checkpoint_callback)


# Original C++ command-only flag: fixed Raw9 producer, not a new Named schema.
def replace_blueprint_component_class_raw_action(result, blueprint_name, component_name, new_component_class,
        sphere_radius=-1.0, dry_run=True, compile=True, save=True, refresh_nodes=True,
        require_scene_compatibility=True, checkpoint_callback=None):
    arguments = _arguments(blueprint_name, component_name, new_component_class, sphere_radius,
        dry_run, compile, save, refresh_nodes)
    if type(require_scene_compatibility) is not bool:
        raise OperationError("component_replace_scene_flag_invalid", "Raw require_scene_compatibility must be a converted exact bool")
    arguments["require_scene_compatibility"] = require_scene_compatibility
    _run_action(result, arguments, checkpoint_callback)
