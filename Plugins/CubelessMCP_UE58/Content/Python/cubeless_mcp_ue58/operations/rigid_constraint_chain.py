"""Original ordered rigid-chain input normalization and one existing journal.

Native owns source loads/simple collision, topology/admission, all SCS setters,
compile/save and actual prefix. No Python loader, second compiler, retry or rollback.
"""
import copy
import json
import math

from cubeless_mcp_ue58.compatibility.ue58 import rigid_constraint_chain as api
from . import component, state
from .state import OperationError


IDENTITY = "ensure_blueprint_rigid_constraint_chain"
_FORBIDDEN = ("unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools",
    "/cubelessmcp_ue58/", "/script/cubelessmcpue58editor")
_MAX_JSON_UNITS = 1048576


def _references(value, field):
    if isinstance(value, str):
        if any(token in value.replace("\\", "/").lower() for token in _FORBIDDEN):
            raise OperationError("blueprint_rigid_chain_reference_unsupported", "MCP-only reference in " + field + "; no native dispatch ran")
    elif type(value) is list:
        for index, child in enumerate(value):
            _references(child, field + "[" + str(index) + "]")
    elif type(value) is dict:
        for key, child in value.items():
            _references(child, field + "." + str(key))


def _text(value, field):
    value = component._selector(value, field)
    _references(value, field)
    return value


def _number(value, field):
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError) as error:
        raise OperationError("blueprint_rigid_chain_number_invalid", field + " must be convertible to a finite number") from error
    if not math.isfinite(number):
        raise OperationError("blueprint_rigid_chain_number_invalid", field + " must be finite")
    return number


def _vector(value, field):
    if type(value) is not list or len(value) != 3:
        raise OperationError("blueprint_rigid_chain_invalid_body_transform", field + " must be three numbers")
    return [_number(item, field + "[" + str(index) + "]") for index, item in enumerate(value)]


def _bodies(body_components):
    if type(body_components) is not list or not 2 <= len(body_components) <= 16:
        raise OperationError("blueprint_rigid_chain_invalid_bodies", "body_components must contain between 2 and 16 objects")
    _references(body_components, "body_components")
    rows = []
    for index, body in enumerate(body_components):
        prefix = "body_components[" + str(index) + "]"
        if type(body) is not dict:
            raise OperationError("blueprint_rigid_chain_invalid_body", prefix + " must be an object")
        if not body.get("component_name") or not body.get("static_mesh"):
            raise OperationError("blueprint_rigid_chain_invalid_body", prefix + " requires component_name and static_mesh")
        normalized = copy.deepcopy(body)
        normalized["relative_location"] = _vector(body.get("relative_location"), prefix + ".relative_location")
        if "relative_rotation" in normalized:
            normalized["relative_rotation"] = _vector(normalized["relative_rotation"], prefix + ".relative_rotation")
        if "mass_kg" in normalized:
            normalized["mass_kg"] = _number(normalized["mass_kg"], prefix + ".mass_kg")
        rows.append(normalized)
    try:
        text = json.dumps(rows, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("blueprint_rigid_chain_invalid_bodies", "Body rows must retain JSON-compatible finite data") from error
    if len(text.encode("utf-16-le", errors="surrogatepass")) // 2 > _MAX_JSON_UNITS:
        raise OperationError("blueprint_rigid_chain_invalid_bodies", "Body JSON exceeds the native 1048576 UTF-16 unit bound")
    return text


def _arguments(blueprint_name, body_components, root_component_name, constraint_prefix,
        swing1_limit_degrees, swing2_limit_degrees, twist_limit_degrees,
        linear_damping, angular_damping, projection_linear_alpha, projection_angular_alpha,
        projection_linear_tolerance, projection_angular_tolerance, required_root, save, dry_run):
    if type(save) is not bool or type(dry_run) is not bool:
        raise OperationError("blueprint_rigid_chain_inputs_invalid", "Original save/dry_run flags require booleans")
    _text(blueprint_name, "blueprint_name")
    return {"blueprint_path": component._mutation_path(blueprint_name), "body_components_json": _bodies(body_components),
        "root_component_name": _text(root_component_name, "root_component_name"), "constraint_prefix": _text(constraint_prefix, "constraint_prefix"),
        "swing1_limit_degrees": _number(swing1_limit_degrees, "swing1_limit_degrees"),
        "swing2_limit_degrees": _number(swing2_limit_degrees, "swing2_limit_degrees"),
        "twist_limit_degrees": _number(twist_limit_degrees, "twist_limit_degrees"),
        "linear_damping": _number(linear_damping, "linear_damping"), "angular_damping": _number(angular_damping, "angular_damping"),
        "projection_linear_alpha": _number(projection_linear_alpha, "projection_linear_alpha"),
        "projection_angular_alpha": _number(projection_angular_alpha, "projection_angular_alpha"),
        "projection_linear_tolerance": _number(projection_linear_tolerance, "projection_linear_tolerance"),
        "projection_angular_tolerance": _number(projection_angular_tolerance, "projection_angular_tolerance"),
        "required_root": _text(required_root, "required_root"), "save": save, "dry_run": dry_run}


def _context(result, arguments):
    result["data"].update(operation=IDENTITY, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, full_legacy_rigid_chain_parity=False,
        cold_persistence_verified=False, runtime_physics_verified=False,
        library_cdo_before_native_entry_independently_observed=False,
        actual_work_counts_authority="returned_native_counters_and_ordered_load_save_receipts")


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_result" if native["compile_requested"] else "not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": IDENTITY, "asset_path": arguments["blueprint_path"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native.get("changed") if known else None,
        "outcome": "native_chain_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_chain_postcondition" if native["postcondition_passed"] else "native_chain_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run", packaged_runtime="not_run")
    result["stage"] = "returned_rigid_constraint_chain_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    callback = checkpoint_callback or state.checkpoint
    result["data"]["pending_mutation"] = {"operation": IDENTITY, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_scs_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_rigid_constraint_chain_operation"
    result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": IDENTITY, "asset_path": arguments["blueprint_path"],
        "attempted": None, "change_state_known": False, "changed": None, "outcome": "unknown_until_native_response"}]
    try:
        native = api.ensure_blueprint_rigid_constraint_chain(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_rigid_constraint_chain_operation"
            callback(result)
        if getattr(error, "code", "") == "native_rigid_constraint_chain_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_rigid_constraint_chain_failed", native.get("error") or "Rigid chain failed; returned prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("rigid_chain_result_unverified", "Native success lacks known complete chain postconditions; do not replay")
    if not arguments["dry_run"] and (native["compile_attempted"] is not True or native["compiled"] is not True):
        raise OperationError("rigid_chain_compile_unverified", "Apply success lacks the original one full compile outcome; returned prefix retained")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "new_chain_names_inspect_journal_no_replay" if native["mutation_attempted"] else "none_observed"


def ensure_blueprint_rigid_constraint_chain(blueprint_name, body_components, root_component_name="SceneRoot", constraint_prefix="PC",
        swing1_limit_degrees=50.0, swing2_limit_degrees=50.0, twist_limit_degrees=10.0, linear_damping=0.05,
        angular_damping=0.15, projection_linear_alpha=0.25, projection_angular_alpha=0.08,
        projection_linear_tolerance=5.0, projection_angular_tolerance=15.0,
        required_root="/Game/Study/ChaosSkeletalMesh", save=True, dry_run=True):
    try:
        arguments = _arguments(blueprint_name, body_components, root_component_name, constraint_prefix,
            swing1_limit_degrees, swing2_limit_degrees, twist_limit_degrees, linear_damping, angular_damping,
            projection_linear_alpha, projection_angular_alpha, projection_linear_tolerance,
            projection_angular_tolerance, required_root, save, dry_run)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        _context(result, {})
        result["data"].update(admission_rejected=True, native_dispatched=False, mutation_state_unknown=False)
        return result
    return state.mutate(IDENTITY, arguments, None, lambda result: _run_action(result, arguments))


def ensure_blueprint_rigid_constraint_chain_action(result, blueprint_name, body_components, root_component_name="SceneRoot", constraint_prefix="PC",
        swing1_limit_degrees=50.0, swing2_limit_degrees=50.0, twist_limit_degrees=10.0, linear_damping=0.05,
        angular_damping=0.15, projection_linear_alpha=0.25, projection_angular_alpha=0.08,
        projection_linear_tolerance=5.0, projection_angular_tolerance=15.0,
        required_root="/Game/Study/ChaosSkeletalMesh", save=True, dry_run=True, checkpoint_callback=None):
    _run_action(result, _arguments(blueprint_name, body_components, root_component_name, constraint_prefix,
        swing1_limit_degrees, swing2_limit_degrees, twist_limit_degrees, linear_damping, angular_damping,
        projection_linear_alpha, projection_angular_alpha, projection_linear_tolerance,
        projection_angular_tolerance, required_root, save, dry_run), checkpoint_callback)
