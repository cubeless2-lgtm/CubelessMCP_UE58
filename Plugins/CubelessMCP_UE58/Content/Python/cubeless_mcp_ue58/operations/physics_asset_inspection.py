"""Original PhysicsAsset observer12; Native nested diagnostics and actual prefix remain authoritative."""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import physics_asset_inspection as api
from . import state
from .state import OperationError

METHOD = "inspect_physics_asset_constraints_guarded"


def _arguments(physics_asset_path,
        skeletal_mesh_path,
        include_bodies,
        include_constraints,
        include_profiles,
        include_solver_settings,
        include_collision_summary,
        max_bodies,
        max_constraints,
        max_profiles_per_item,
        max_shapes_per_body,
        require_supported_class):
    if type(physics_asset_path) is not str or type(skeletal_mesh_path) is not str:
        raise OperationError("physics_asset_inspection_input_invalid", "Both paths must be literal strings; Native owns original empty-path refusal, normalization and loading")
    if any(type(value) is not bool for value in (include_bodies, include_constraints, include_profiles, include_solver_settings, include_collision_summary, require_supported_class)):
        raise OperationError("physics_asset_inspection_input_invalid", "Original include and supported-class flags must be booleans")
    if any(type(value) is not int or not -(2**63) <= value < 2**63 for value in (max_bodies, max_constraints, max_profiles_per_item, max_shapes_per_body)):
        raise OperationError("physics_asset_inspection_input_invalid", "Limits must fit the private int64 carriers; Native owns original zero-based clamps")
    return dict(physics_asset_path=physics_asset_path,
        skeletal_mesh_path=skeletal_mesh_path,
        include_bodies=include_bodies,
        include_constraints=include_constraints,
        include_profiles=include_profiles,
        include_solver_settings=include_solver_settings,
        include_collision_summary=include_collision_summary,
        max_bodies=max_bodies,
        max_constraints=max_constraints,
        max_profiles_per_item=max_profiles_per_item,
        max_shapes_per_body=max_shapes_per_body,
        require_supported_class=require_supported_class)


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_requested_observer=True, named_mutation_journal_created=False,
        native_load_preview_cache_effects_not_global_purity_proof=True,
        native_physics_asset_precedence_and_assigned_asset_resolution_authoritative=True,
        native_body_shape_constraint_profile_solver_counts_and_null_rows_authoritative=True,
        unsupported_require_false_is_completed_diagnostic=True,
        warnings_errors_and_unavailable_fields_not_blanket_failure_predicates=True,
        automatic_action_retry=False, full_original_contract_complete=False,
        cold_persistence_verified=False, live_physics_simulation_or_profile_application_verified=False,
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
        {"kind": "physics_asset_inspection_observation_scope", "attempted": native["mutation_attempted"],
            "change_state_known": known, "changed": native["changed"] if known else None,
            "outcome": "current_scoped_prefix" if known else "unknown_after_dispatch"}]
    result["stage"] = "returned_physics_asset_inspection_observation"
    result["validation"].update(status="native_physics_asset_inspection_postcondition",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        packaged_runtime="not_run", initial_load_effects="not_fully_instrumented")


def inspect_physics_asset_constraints_guarded_action(result, physics_asset_path='',
        skeletal_mesh_path='',
        include_bodies=True,
        include_constraints=True,
        include_profiles=True,
        include_solver_settings=True,
        include_collision_summary=True,
        max_bodies=128,
        max_constraints=128,
        max_profiles_per_item=16,
        max_shapes_per_body=32,
        require_supported_class=False, checkpoint_callback=None):
    arguments = _arguments(physics_asset_path,
            skeletal_mesh_path,
            include_bodies,
            include_constraints,
            include_profiles,
            include_solver_settings,
            include_collision_summary,
            max_bodies,
            max_constraints,
            max_profiles_per_item,
            max_shapes_per_body,
            require_supported_class)
    _context(result, arguments)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        mutation_state_unknown=True, mutation_performed=None)
    result["changes"] = [{"kind": "physics_asset_inspection_observation_scope", "attempted": None,
        "change_state_known": False, "changed": None, "outcome": "unknown_until_native_returns"}]
    result["compile"].update(requested=False, attempted=None, compiled=None, status="unknown_until_native_returns")
    result["save"].update(requested=False, attempted=None, saved=None, status="unknown_until_native_returns")
    result["stage"] = "native_physics_asset_inspection_observation"
    try:
        native = api.inspect_physics_asset_constraints_guarded(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_physics_asset_inspection_observation"
            if checkpoint_callback:
                checkpoint_callback(result)
        if getattr(error, "code", "") == "native_physics_asset_inspection_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)  # Actual nested data/raw/flags precede every criterion.
    if checkpoint_callback:
        checkpoint_callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_physics_asset_inspection_failed",
            native.get("error") or "Native PhysicsAsset prefix retained")
    if native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("physics_asset_inspection_unexpected_work", "Unexpected direct work retained; no second observation")
    if not native["change_state_known"] or native["changed"] is not False or not native["postcondition_passed"]:
        raise OperationError("physics_asset_inspection_observation_unverified", "Current selected read proof incomplete; no replay")
    # requireFalse can carry legacy errors[] and still be a completed diagnostic.
    result.update(success=True, stage="complete")
    return result


def inspect_physics_asset_constraints_guarded(physics_asset_path='',
        skeletal_mesh_path='',
        include_bodies=True,
        include_constraints=True,
        include_profiles=True,
        include_solver_settings=True,
        include_collision_summary=True,
        max_bodies=128,
        max_constraints=128,
        max_profiles_per_item=16,
        max_shapes_per_body=32,
        require_supported_class=False):
    result = state.new_result()
    result["data"].update(operation=METHOD, native_dispatched=False, native_result_returned=False,
        mutation_state_unknown=False, mutation_performed=False)
    try:
        inspect_physics_asset_constraints_guarded_action(result, physics_asset_path,
            skeletal_mesh_path,
            include_bodies,
            include_constraints,
            include_profiles,
            include_solver_settings,
            include_collision_summary,
            max_bodies,
            max_constraints,
            max_profiles_per_item,
            max_shapes_per_body,
            require_supported_class)
        result.update(success=True, stage="complete")
    except Exception as error:
        state.fail(result, error)
    return result
