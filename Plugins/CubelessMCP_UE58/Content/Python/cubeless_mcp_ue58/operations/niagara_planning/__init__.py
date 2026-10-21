"""Seven deterministic UE56 planning contracts; no Unreal, MCP or model imports."""

import logging

from . import effect_planning, refinement_ledger
from .response_schema import tool_error


_LOGGER = logging.getLogger("CubelessMCP_UE58.NiagaraPlanning")
_CONTEXT = {
    "target_engine": "UE 5.8.3",
    "source_engine_context": "UE 5.6",
    "engine_behavior_verified_in_target": False,
    "engine_api_invoked": False,
    "model_or_network_invoked": False,
    "historical_tool_names_are_current_registration_evidence": False,
    "execution_ready_semantics": "legacy schema and temp-policy readiness only; not UE58 dispatch readiness",
    "ue58_dispatch_validated": False,
    "legacy_execution_manifest_directly_dispatchable": False,
    "rollback_semantics": "decision/proposal only; no rollback executed or atomic rollback provided",
}


def _run(function, message, error_code, *args, **kwargs):
    try:
        response = function(*args, **kwargs)
    except Exception as error:
        _LOGGER.exception(message)
        response = tool_error(f"{message}: {error}", error_code=error_code,
            dry_run=True, mutation_performed=False, exception_type=type(error).__name__)
    # Add provenance without deleting legacy fields, decision codes or readiness.
    response["migration_context"] = dict(_CONTEXT)
    if "matches" in response:
        for row in response["matches"]:
            row["evidence_scope"] = "historical UE56 diagnosis hypothesis; current UE58 observation required"
    return response


def plan_niagara_effect(intent, working_path="", source_system_path="", draft_spec=None,
        material_candidates=None, constraints=None):
    return _run(effect_planning.plan_effect, "Niagara effect planning failed", "niagara_effect_planning_failed",
        intent, working_path=working_path, source_system_path=source_system_path,
        draft_spec=draft_spec, material_candidates=material_candidates, constraints=constraints)


def validate_niagara_effect_spec(effect_spec):
    return _run(effect_planning.validate_effect_spec, "Niagara effect spec validation failed",
        "niagara_effect_spec_validation_failed", effect_spec)


def diagnose_niagara_effect_issue(observation, compile_feedback="", system_summary=None,
        previous_patch_history=None, max_results=5):
    return _run(effect_planning.diagnose_effect_issue, "Niagara effect diagnosis failed",
        "niagara_effect_diagnosis_failed", observation, compile_feedback=compile_feedback,
        system_summary=system_summary, previous_patch_history=previous_patch_history, max_results=max_results)


def validate_niagara_effect_patch(patch, effect_spec=None, allow_non_temp_target=False):
    return _run(effect_planning.validate_effect_patch, "Niagara effect patch validation failed",
        "niagara_effect_patch_validation_failed", patch, effect_spec=effect_spec,
        allow_non_temp_target=allow_non_temp_target)


def create_niagara_refinement_ledger(system_path, objectives, baseline_state, run_id="",
        max_iterations=4, effect_spec_hash=""):
    return _run(refinement_ledger.create_refinement_ledger, "Niagara refinement ledger creation failed",
        "niagara_refinement_ledger_creation_failed", system_path, objectives, baseline_state,
        run_id=run_id, max_iterations=max_iterations, effect_spec_hash=effect_spec_hash)


def evaluate_niagara_refinement_iteration(ledger, patch_id, candidate_state, objective_ids=None,
        base_state_id=""):
    return _run(refinement_ledger.evaluate_refinement_iteration, "Niagara refinement iteration evaluation failed",
        "niagara_refinement_iteration_evaluation_failed", ledger, patch_id, candidate_state,
        objective_ids=objective_ids, base_state_id=base_state_id)


def select_niagara_best_refinement_state(ledger):
    return _run(refinement_ledger.select_best_refinement_state, "Niagara best refinement state selection failed",
        "niagara_best_refinement_state_selection_failed", ledger)
