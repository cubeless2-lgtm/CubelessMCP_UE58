"""Pure Niagara planning/refinement on the official Toolset Registry."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import niagara_planning as planning
from cubeless_mcp_ue58.operations.niagara_planning.json_boundary import call_json, operation_record


def _result(function, scalars, documents):
    return reflected_result(operation_record(call_json(function, scalars, documents)))


@unreal.uclass()
class CubelessNiagaraPlanningTools(unreal.ToolsetDefinition):
    """Seven deterministic planning decisions, with no engine edits or models."""

    @toolset_registry.tool_call
    @staticmethod
    def plan_niagara_effect(intent: str, working_path: str | None = None,
            source_system_path: str | None = None, draft_spec_json: str | None = None,
            material_candidates_json: str | None = None, constraints_json: str | None = None) -> CubelessPythonOperationResult:
        """Preserve the legacy grounded ParticleEffectSpec planner: profiles, emitter dependencies, modules, objectives, constraints and candidate materials. Nested objects/arrays use JSON strings, omitted/null optional documents retain legacy None. No LLM/network/Unreal calls, file writes or asset mutations. Historical UE56 diagnosis/tool references are labeled hypotheses; execution_ready means schema/temp-policy readiness, never current UE58 dispatch readiness. Full legacy output is in data_json.

        Args:
            intent: Required artistic and behavioral intent.
            working_path: Optional planned /Game/_MCP_Temp/NiagaraGenerated asset path; omission/null preserves empty. Existence is not checked.
            source_system_path: Optional source /Game asset path; omission/null preserves empty. No asset is loaded.
            draft_spec_json: Optional JSON object ParticleEffectSpec to normalize.
            material_candidates_json: Optional JSON array of material metadata objects.
            constraints_json: Optional JSON object overrides; safety invariants still validate.
        """
        return _result(planning.plan_niagara_effect,
            {"intent": intent, "working_path": working_path or "", "source_system_path": source_system_path or ""},
            {"draft_spec": draft_spec_json, "material_candidates": material_candidates_json, "constraints": constraints_json})

    @toolset_registry.tool_call
    @staticmethod
    def validate_niagara_effect_spec(effect_spec_json: str) -> CubelessPythonOperationResult:
        """Validate legacy spec schema, emitter/objective identities, module stages, paths/dependency cycles, temp policy and promotion/plugin-independence/rollback/best-selection requirements. JSON document is deep-copied; input is not modified. Current Cubeless/ModelContextProtocol asset references also fail the plugin independence guard. No engine existence/compile verification or mutation.

        Args:
            effect_spec_json: Required JSON object ParticleEffectSpec.
        """
        return _result(planning.validate_niagara_effect_spec, {}, {"effect_spec": effect_spec_json})

    @toolset_registry.tool_call
    @staticmethod
    def diagnose_niagara_effect_issue(observation: str, compile_feedback: str | None = None,
            system_summary_json: str | None = None, previous_patch_history_json: str | None = None,
            max_results: int = 5) -> CubelessPythonOperationResult:
        """Retrieve deterministic phrase/token-ranked symptom/cause/fix hypotheses from preserved UE56 diagnostic records. Original scores, confidence, evidence plan and patch template remain intact. Source refs/evidence tool names are historical, not current engine or registration proof. No embeddings, paid APIs, images, Unreal calls or writes.

        Args:
            observation: Required symptom or review note.
            compile_feedback: Optional supplied compiler text; not independently inspected.
            system_summary_json: Optional JSON object supplied inspection summary.
            previous_patch_history_json: Optional JSON array of prior patch summaries.
            max_results: Integer 1..10, default 5.
        """
        return _result(planning.diagnose_niagara_effect_issue,
            {"observation": observation, "compile_feedback": compile_feedback or "", "max_results": max_results},
            {"system_summary": system_summary_json, "previous_patch_history": previous_patch_history_json})

    @toolset_registry.tool_call
    @staticmethod
    def validate_niagara_effect_patch(patch_json: str, effect_spec_json: str | None = None,
            allow_non_temp_target: bool = False) -> CubelessPythonOperationResult:
        """Validate legacy objective patch paths/allowlisted command proposals, measurable observations, rollback policy, compile request and baseline binding. Preserves the non-dispatched legacy execute_batch manifest. Its type/params command format is not the current Cubeless named batch format and is not directly dispatchable; a valid response is no execution approval. No rollback or asset change is performed.

        Args:
            patch_json: Required JSON object versioned objective patch.
            effect_spec_json: Optional JSON object for exact working-path binding.
            allow_non_temp_target: Default False; True permits planning validation only, never mutation/promotion approval.
        """
        return _result(planning.validate_niagara_effect_patch, {"allow_non_temp_target": allow_non_temp_target},
            {"patch": patch_json, "effect_spec": effect_spec_json})

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_refinement_ledger(system_path: str, objectives_json: str, baseline_state_json: str,
            run_id: str | None = None, max_iterations: int = 4,
            effect_spec_hash: str | None = None) -> CubelessPythonOperationResult:
        """Create the legacy deterministic in-memory refinement ledger from supplied compile-healthy evidence. Target/at_least/at_most/equal metrics and improvement/target/non_regression policies retain their original normalization and validation. Input compile counts are evidence supplied by the caller, not a compiler invocation. Returns a ledger; never persists files or connects to Unreal.

        Args:
            system_path: Exact generated temp Niagara path.
            objectives_json: Required JSON array of objective metric contracts.
            baseline_state_json: Required JSON object state, metrics and compile evidence.
            run_id: Optional identifier; omission/null/empty preserves deterministic derived ID.
            max_iterations: Default 4, valid 1..16.
            effect_spec_hash: Optional supplied provenance hash, not independently verified.
        """
        return _result(planning.create_niagara_refinement_ledger,
            {"system_path": system_path, "run_id": run_id or "", "max_iterations": max_iterations,
             "effect_spec_hash": effect_spec_hash or ""},
            {"objectives": objectives_json, "baseline_state": baseline_state_json})

    @toolset_registry.tool_call
    @staticmethod
    def evaluate_niagara_refinement_iteration(ledger_json: str, patch_id: str, candidate_state_json: str,
            objective_ids_json: str | None = None, base_state_id: str | None = None) -> CubelessPythonOperationResult:
        """Preserve compile gates, selected/unselected objective regression guards, keep/selective-rollback decisions, rejected-state history and iteration limits. Returns a copied updated ledger. A successful evaluation may reject a candidate. Rollback plan is a decision only; no asset restoration/Undo or atomic rollback is executed.

        Args:
            ledger_json: Required JSON object existing refinement ledger.
            patch_id: Required unique patch identifier.
            candidate_state_json: Required JSON object candidate metrics/compile evidence.
            objective_ids_json: Optional JSON array of evaluated IDs; omitted/null retains legacy all-objectives selection.
            base_state_id: Optional known state ID; omission/null/empty uses ledger current state.
        """
        return _result(planning.evaluate_niagara_refinement_iteration,
            {"patch_id": patch_id, "base_state_id": base_state_id or ""},
            {"ledger": ledger_json, "candidate_state": candidate_state_json, "objective_ids": objective_ids_json})

    @toolset_registry.tool_call
    @staticmethod
    def select_niagara_best_refinement_state(ledger_json: str) -> CubelessPythonOperationResult:
        """Rank accepted compile-healthy evidence-complete historical states by weighted objective score; earliest state wins ties. Rejected/incomplete states are ineligible. Preserves legacy ranking/error contracts and never modifies the supplied ledger or engine assets.

        Args:
            ledger_json: Required JSON object refinement ledger.
        """
        return _result(planning.select_niagara_best_refinement_state, {}, {"ledger": ledger_json})
