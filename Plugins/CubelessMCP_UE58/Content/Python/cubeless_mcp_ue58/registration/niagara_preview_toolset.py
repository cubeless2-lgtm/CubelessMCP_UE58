"""Named read-only preview critique on the official Toolset Registry."""

import json

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.compatibility.ue58.niagara_preview import current_project_file
from cubeless_mcp_ue58.operations.niagara_preview import critique_niagara_preview_evidence as critique
from cubeless_mcp_ue58.operations.niagara_planning.response_schema import tool_error
from cubeless_mcp_ue58.operations.niagara_planning.json_boundary import operation_record


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON object key: " + key)
        result[key] = value
    return result


def _reject_constant(value):
    raise ValueError("non-finite JSON value: " + value)


def _decode(text, name, optional=False):
    if text is None and optional:
        return None
    if not isinstance(text, str):
        raise ValueError(name + " must be a JSON string" + (" or null" if optional else ""))
    return json.loads(text, object_pairs_hook=_unique_object, parse_constant=_reject_constant)


def _operation(response):
    result = operation_record(response)
    result["validation"] = {"status": "preview_evidence_evaluated" if response.get("success") else "preview_evidence_rejected",
        "engine_state_validation": "not_run", "render_origin": "unverified",
        "disk_reload": "not_applicable_no_asset_write", "packaged_runtime": "not_applicable"}
    result["retry"]["duplicate_risk"] = "none_read_only_analysis"
    return reflected_result(result)


@unreal.uclass()
class CubelessNiagaraPreviewTools(unreal.ToolsetDefinition):
    """Analyze supplied PNG/JPEG evidence; never render or mutate an asset."""

    @toolset_registry.tool_call
    @staticmethod
    def critique_niagara_preview_evidence(candidate_frame_paths_json: str, criteria_json: str,
            baseline_frame_paths_json: str | None = None, roi_json: str | None = None) -> CubelessPythonOperationResult:
        """Preserve the legacy visual critic's luminance, bright-neutral/foreground/edge ratios, centroid, temporal/baseline comparison, weighted criteria, ranking and refinement-ledger metrics. Full legacy result is in data_json. Four legacy nested inputs use JSON strings; omitted/null baseline and ROI retain legacy None. Paths resolve strictly beneath the running project's Saved/MCP/NiagaraReviews, including relative paths; external paths/junction escapes are rejected. The internal project anchor comes only from public UE58 Paths APIs. No screenshot/render/Unreal asset mutation, compiler/save/journal, dependency installation, or model call occurs. Pillow is lazily imported only after request/path validation; missing/incompatible decoder produces the legacy critique-failed code with dependency_preflight details. Supplied images do not prove Niagara/GPU rendering origin. Processing is synchronous, capped at32 frames per list,32MiB per file and640x640 analysis resolution; latency is measured separately.

        Args:
            candidate_frame_paths_json: Required JSON array of1..32 PNG/JPEG paths beneath current Saved/MCP/NiagaraReviews.
            criteria_json: Required JSON array of legacy criterion objects including criterion_id, objective_id, metric, comparison and target.
            baseline_frame_paths_json: Optional JSON array paired by position; omitted/null preserves no baseline. If supplied, length must equal candidate count.
            roi_json: Optional JSON array [x_min,y_min,x_max,y_max] normalized0..1; omitted/null uses legacy [.15,.15,.85,.80].
        """
        try:
            documents = {"candidate_frame_paths": _decode(candidate_frame_paths_json, "candidate_frame_paths"),
                "criteria": _decode(criteria_json, "criteria"),
                "baseline_frame_paths": _decode(baseline_frame_paths_json, "baseline_frame_paths", optional=True),
                "roi": _decode(roi_json, "roi", optional=True)}
        except (TypeError, ValueError) as error:
            return _operation(tool_error(str(error), error_code="niagara_preview_json_invalid",
                dry_run=True, mutation_performed=False, writes_assets=False,
                failure_stage="json_preflight", exception_type=type(error).__name__))
        try:
            project_file = current_project_file()
        except Exception as error:
            return _operation(tool_error("Niagara preview evidence critique failed: " + str(error),
                error_code="niagara_preview_evidence_critique_failed", dry_run=True,
                mutation_performed=False, writes_assets=False, failure_stage="project_context",
                exception_type=type(error).__name__))
        return _operation(critique(**documents, project_file=project_file))
