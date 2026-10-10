"""Mutation-free Niagara planning, diagnosis, and patch-contract tools."""

from __future__ import annotations

import logging
from typing import Any

from mcp.server.fastmcp import FastMCP
from services.niagara_effect_planning import (
    diagnose_effect_issue,
    plan_effect,
    validate_effect_patch,
    validate_effect_spec,
)
from services.niagara_refinement_ledger import (
    create_refinement_ledger,
    evaluate_refinement_iteration,
    select_best_refinement_state,
)
from services.niagara_visual_critic import critique_preview_evidence
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")


def register_niagara_planning_tools(mcp: FastMCP) -> None:
    """Register grounded planning tools that never mutate Unreal state."""

    @mcp.tool()
    def plan_niagara_effect(
        intent: str,
        working_path: str = "",
        source_system_path: str = "",
        draft_spec: dict[str, Any] | None = None,
        material_candidates: list[dict[str, Any]] | None = None,
        constraints: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        """Create a grounded, dry-run ParticleEffectSpec for Niagara authoring.

        The planner decomposes an artistic intent into emitter responsibilities,
        module responsibilities, measurable objectives, knowledge topics, and a
        safe follow-up workflow. It does not call an LLM, connect to Unreal, save
        files, or execute mutations. Pass a draft_spec to normalize and validate
        an agent-authored plan against the same contract.

        Args:
            intent: Natural-language artistic and behavioral intent.
            working_path: Optional unused generated temp Niagara path under
                /Game/_MCP_Temp/NiagaraGenerated.
            source_system_path: Optional source Niagara System to inspect or duplicate.
            draft_spec: Optional agent-authored ParticleEffectSpec to normalize.
            material_candidates: Optional project/engine material metadata candidates.
            constraints: Optional constraint overrides; safety invariants remain validated.
        """
        try:
            return plan_effect(
                intent,
                working_path=working_path,
                source_system_path=source_system_path,
                draft_spec=draft_spec,
                material_candidates=material_candidates,
                constraints=constraints,
            )
        except Exception as exc:
            logger.exception("Niagara effect planning failed")
            return tool_error(
                f"Niagara effect planning failed: {exc}",
                error_code="niagara_effect_planning_failed",
                dry_run=True,
                mutation_performed=False,
                exception_type=type(exc).__name__,
            )

    @mcp.tool()
    def validate_niagara_effect_spec(
        effect_spec: dict[str, Any],
    ) -> dict[str, Any]:
        """Validate a ParticleEffectSpec and its emitter dependency graph.

        Validation covers schema version, emitter/objective identity, module
        stages, material and system asset paths, dependency cycles, temp working
        scope, promotion requirements, plugin independence, selective rollback,
        and best-state selection. This tool is mutation-free.
        """
        try:
            return validate_effect_spec(effect_spec)
        except Exception as exc:
            logger.exception("Niagara effect spec validation failed")
            return tool_error(
                f"Niagara effect spec validation failed: {exc}",
                error_code="niagara_effect_spec_validation_failed",
                dry_run=True,
                mutation_performed=False,
                exception_type=type(exc).__name__,
            )

    @mcp.tool()
    def diagnose_niagara_effect_issue(
        observation: str,
        compile_feedback: str = "",
        system_summary: dict[str, Any] | None = None,
        previous_patch_history: list[dict[str, Any]] | None = None,
        max_results: int = 5,
    ) -> dict[str, Any]:
        """Retrieve likely Niagara causes and fixes from repository-grounded DRAG data.

        P0 uses deterministic phrase/token retrieval over versioned symptom-cause-
        fix records derived from current MCPWeather/Niagara lessons and ParticleGen.
        The result includes evidence tools and an objective-patch template. It does
        not use embeddings, paid APIs, Unreal connections, or asset mutations.

        Args:
            observation: Visible symptom or review note.
            compile_feedback: Optional compile errors or warnings.
            system_summary: Optional output subset from Niagara inspection tools.
            previous_patch_history: Optional earlier objective-patch summaries.
            max_results: Maximum diagnoses to return, 1..10.
        """
        try:
            return diagnose_effect_issue(
                observation,
                compile_feedback=compile_feedback,
                system_summary=system_summary,
                previous_patch_history=previous_patch_history,
                max_results=max_results,
            )
        except Exception as exc:
            logger.exception("Niagara effect diagnosis failed")
            return tool_error(
                f"Niagara effect diagnosis failed: {exc}",
                error_code="niagara_effect_diagnosis_failed",
                dry_run=True,
                mutation_performed=False,
                exception_type=type(exc).__name__,
            )

    @mcp.tool()
    def validate_niagara_effect_patch(
        patch: dict[str, Any],
        effect_spec: dict[str, Any] | None = None,
        allow_non_temp_target: bool = False,
    ) -> dict[str, Any]:
        """Validate an objective-based Niagara patch without executing it.

        Every objective must define a measurable expected observation and a
        rollback policy. Operations are restricted to the existing reviewed
        Niagara command allowlist, share one exact system path, and default to
        generated temp content. A valid response includes a non-dispatched
        execute_batch manifest; execution always requires a separate explicit
        tool call.

        Args:
            patch: Versioned objective-patch object.
            effect_spec: Optional ParticleEffectSpec for exact working-path binding.
            allow_non_temp_target: Validate a non-temp path for planning only;
                this never approves or performs mutation and does not bypass promotion.
        """
        try:
            return validate_effect_patch(
                patch,
                effect_spec=effect_spec,
                allow_non_temp_target=allow_non_temp_target,
            )
        except Exception as exc:
            logger.exception("Niagara effect patch validation failed")
            return tool_error(
                f"Niagara effect patch validation failed: {exc}",
                error_code="niagara_effect_patch_validation_failed",
                dry_run=True,
                mutation_performed=False,
                exception_type=type(exc).__name__,
            )

    @mcp.tool()
    def create_niagara_refinement_ledger(
        system_path: str,
        objectives: list[dict[str, Any]],
        baseline_state: dict[str, Any],
        run_id: str = "",
        max_iterations: int = 4,
        effect_spec_hash: str = "",
    ) -> dict[str, Any]:
        """Create an evidence ledger from a compile-healthy generated baseline.

        Objective metrics use one of target, at_least, at_most, or equal and an
        acceptance policy of improvement, target, or non_regression. State
        evidence must include exact metric values and normalized compile counts.
        The tool is deterministic and never connects to Unreal or writes assets.
        """
        try:
            return create_refinement_ledger(
                system_path,
                objectives,
                baseline_state,
                run_id=run_id,
                max_iterations=max_iterations,
                effect_spec_hash=effect_spec_hash,
            )
        except Exception as exc:
            logger.exception("Niagara refinement ledger creation failed")
            return tool_error(
                f"Niagara refinement ledger creation failed: {exc}",
                error_code="niagara_refinement_ledger_creation_failed",
                dry_run=True,
                mutation_performed=False,
                exception_type=type(exc).__name__,
            )

    @mcp.tool()
    def evaluate_niagara_refinement_iteration(
        ledger: dict[str, Any],
        patch_id: str,
        candidate_state: dict[str, Any],
        objective_ids: list[str] | None = None,
        base_state_id: str = "",
    ) -> dict[str, Any]:
        """Evaluate a candidate and return objective-level keep/rollback decisions.

        Compile failures reject every evaluated objective. A regressing objective
        is marked for selective rollback while accepted historical state remains
        current. Rejected candidates are recorded for audit but excluded from
        best-state selection. No rollback command is dispatched by this tool.
        """
        try:
            return evaluate_refinement_iteration(
                ledger,
                patch_id,
                candidate_state,
                objective_ids=objective_ids,
                base_state_id=base_state_id,
            )
        except Exception as exc:
            logger.exception("Niagara refinement iteration evaluation failed")
            return tool_error(
                f"Niagara refinement iteration evaluation failed: {exc}",
                error_code="niagara_refinement_iteration_evaluation_failed",
                dry_run=True,
                mutation_performed=False,
                exception_type=type(exc).__name__,
            )

    @mcp.tool()
    def select_niagara_best_refinement_state(
        ledger: dict[str, Any],
    ) -> dict[str, Any]:
        """Select the best accepted compile-healthy historical Niagara state.

        States are ranked by weighted objective score. Rejected or incomplete
        states are ineligible, and the earliest state wins deterministic ties.
        This is a mutation-free decision tool.
        """
        try:
            return select_best_refinement_state(ledger)
        except Exception as exc:
            logger.exception("Niagara best refinement state selection failed")
            return tool_error(
                f"Niagara best refinement state selection failed: {exc}",
                error_code="niagara_best_refinement_state_selection_failed",
                dry_run=True,
                mutation_performed=False,
                exception_type=type(exc).__name__,
            )

    @mcp.tool()
    def critique_niagara_preview_evidence(
        candidate_frame_paths: list[str],
        criteria: list[dict[str, Any]],
        baseline_frame_paths: list[str] | None = None,
        roi: list[float] | None = None,
    ) -> dict[str, Any]:
        """Bind deterministic Niagara preview findings to refinement objectives.

        Frames must exist under MCPGameProject/Saved/MCP/NiagaraReviews. The
        Critic measures bright-particle coverage, foreground, edges, luma,
        centroid, visibility, and temporal change; optional baseline frames add
        paired difference and delta criteria. Results include baseline/candidate
        metric maps ready to merge into P1 ledger states. No model, external API,
        Unreal connection, or asset mutation is used.
        """
        try:
            return critique_preview_evidence(
                candidate_frame_paths,
                criteria,
                baseline_frame_paths=baseline_frame_paths,
                roi=roi,
            )
        except Exception as exc:
            logger.exception("Niagara preview evidence critique failed")
            return tool_error(
                f"Niagara preview evidence critique failed: {exc}",
                error_code="niagara_preview_evidence_critique_failed",
                dry_run=True,
                mutation_performed=False,
                exception_type=type(exc).__name__,
            )

    logger.info("Niagara planning tools registered successfully")
