"""Deterministic refinement ledger for evidence-driven Niagara iteration.

The service contains no Unreal connection and performs no mutations.  It turns
compile/readback evidence into objective-level keep or rollback decisions and
keeps rejected candidates out of best-state selection.
"""

from __future__ import annotations

import copy
import hashlib
import json
import re
from collections.abc import Mapping, Sequence
from typing import Any

from tools.response_schema import tool_error, tool_success

from services.niagara_effect_planning import TEMP_NIAGARA_ROOT

REFINEMENT_LEDGER_SCHEMA_VERSION = 1
MAX_REFINEMENT_ITERATIONS = 16
MAX_REFINEMENT_OBJECTIVES = 24
MAX_STATE_METRICS = 64

IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9_.:-]{0,127}$")
ALLOWED_COMPARISONS = {"target", "at_least", "at_most", "equal"}
ALLOWED_ACCEPTANCE_POLICIES = {"improvement", "target", "non_regression"}


def _error(message: str, error_code: str, **fields: Any) -> dict[str, Any]:
    return tool_error(
        message,
        error_code=error_code,
        dry_run=True,
        mutation_performed=False,
        writes_assets=False,
        **fields,
    )


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _is_json_metric(value: Any) -> bool:
    if value is None or isinstance(value, (str, bool)) or _is_number(value):
        return value is not None
    if isinstance(value, list):
        return len(value) <= 16 and all(
            item is not None and isinstance(item, (str, bool, int, float))
            for item in value
        )
    return False


def _validate_identifier(value: Any, field: str, errors: list[str]) -> str:
    text = str(value or "").strip()
    if not IDENTIFIER_PATTERN.fullmatch(text):
        errors.append(f"{field} must match {IDENTIFIER_PATTERN.pattern}")
    return text


def _metric_equal(left: Any, right: Any, tolerance: float) -> bool:
    if _is_number(left) and _is_number(right):
        return abs(float(left) - float(right)) <= tolerance
    if isinstance(left, list) and isinstance(right, list) and len(left) == len(right):
        return all(_metric_equal(a, b, tolerance) for a, b in zip(left, right))
    return left == right


def _first_number(data: Mapping[str, Any], keys: Sequence[str]) -> float | None:
    for key in keys:
        value = data.get(key)
        if _is_number(value):
            return float(value)
    return None


def _normalize_compile_status(
    value: Any,
    field: str,
    errors: list[str],
) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        errors.append(f"{field} must be an object")
        return {
            "error_count": None,
            "warning_count": None,
            "outstanding_requests": None,
            "wait_timed_out": None,
            "healthy": False,
        }

    error_count = _first_number(
        value,
        ("error_count", "total_error_count", "compile_errors"),
    )
    warning_count = _first_number(
        value,
        ("warning_count", "total_warning_count", "compile_warnings"),
    )
    outstanding = _first_number(
        value,
        ("outstanding_requests", "outstanding_compile_request_count"),
    )
    if outstanding is None and isinstance(value.get("has_outstanding_compile_requests"), bool):
        outstanding = 1.0 if value["has_outstanding_compile_requests"] else 0.0
    if outstanding is None:
        outstanding = 0.0

    wait_timed_out = value.get("wait_timed_out", False)
    if error_count is None:
        errors.append(
            f"{field} must include numeric error_count, total_error_count, or compile_errors"
        )
    if warning_count is None:
        warning_count = 0.0
    if not isinstance(wait_timed_out, bool):
        errors.append(f"{field}.wait_timed_out must be a boolean when present")
        wait_timed_out = True
    if error_count is not None and error_count < 0:
        errors.append(f"{field}.error_count must be non-negative")
    if warning_count < 0:
        errors.append(f"{field}.warning_count must be non-negative")
    if outstanding < 0:
        errors.append(f"{field}.outstanding_requests must be non-negative")

    healthy = (
        error_count == 0
        and outstanding == 0
        and wait_timed_out is False
    )
    return {
        "error_count": int(error_count) if error_count is not None else None,
        "warning_count": int(warning_count),
        "outstanding_requests": int(outstanding),
        "wait_timed_out": wait_timed_out,
        "healthy": healthy,
    }


def _normalize_state(
    value: Any,
    *,
    system_path: str,
    field: str,
    errors: list[str],
) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        errors.append(f"{field} must be an object")
        return {}

    state_id = _validate_identifier(value.get("state_id"), f"{field}.state_id", errors)
    state_system_path = str(value.get("system_path") or system_path).strip()
    if state_system_path != system_path:
        errors.append(f"{field}.system_path must exactly match ledger system_path")

    metrics_value = value.get("metrics")
    if not isinstance(metrics_value, Mapping):
        errors.append(f"{field}.metrics must be an object")
        metrics: dict[str, Any] = {}
    else:
        metrics = {}
        if len(metrics_value) > MAX_STATE_METRICS:
            errors.append(f"{field}.metrics exceeds {MAX_STATE_METRICS} entries")
        for key, metric_value in metrics_value.items():
            metric_name = str(key).strip()
            if not metric_name:
                errors.append(f"{field}.metrics contains an empty metric name")
                continue
            if not _is_json_metric(metric_value):
                errors.append(
                    f"{field}.metrics.{metric_name} must be a scalar or flat scalar list"
                )
                continue
            metrics[metric_name] = copy.deepcopy(metric_value)

    compile_status = _normalize_compile_status(
        value.get("compile"),
        f"{field}.compile",
        errors,
    )
    evidence = value.get("evidence", {})
    if not isinstance(evidence, Mapping):
        errors.append(f"{field}.evidence must be an object when present")
        evidence = {}

    return {
        "state_id": state_id,
        "system_path": state_system_path,
        "metrics": metrics,
        "compile": compile_status,
        "evidence": copy.deepcopy(dict(evidence)),
    }


def _normalize_objectives(value: Any, errors: list[str]) -> list[dict[str, Any]]:
    if not isinstance(value, list) or not value:
        errors.append("objectives must be a non-empty list")
        return []
    if len(value) > MAX_REFINEMENT_OBJECTIVES:
        errors.append(f"objectives exceeds {MAX_REFINEMENT_OBJECTIVES} entries")

    normalized: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    for index, raw in enumerate(value):
        field = f"objectives[{index}]"
        if not isinstance(raw, Mapping):
            errors.append(f"{field} must be an object")
            continue
        objective_id = _validate_identifier(raw.get("objective_id"), f"{field}.objective_id", errors)
        if objective_id in seen_ids:
            errors.append(f"{field}.objective_id is duplicated: {objective_id}")
        seen_ids.add(objective_id)

        metric = str(raw.get("metric") or "").strip()
        if not metric:
            errors.append(f"{field}.metric is required")
        comparison = str(raw.get("comparison") or "").strip()
        if comparison not in ALLOWED_COMPARISONS:
            errors.append(f"{field}.comparison must be one of {sorted(ALLOWED_COMPARISONS)}")
        acceptance = str(raw.get("acceptance") or "non_regression").strip()
        if acceptance not in ALLOWED_ACCEPTANCE_POLICIES:
            errors.append(
                f"{field}.acceptance must be one of {sorted(ALLOWED_ACCEPTANCE_POLICIES)}"
            )
        target = raw.get("target")
        if not _is_json_metric(target):
            errors.append(f"{field}.target must be a scalar or flat scalar list")
        if comparison != "equal" and (not _is_number(target)):
            errors.append(f"{field}.target must be numeric for comparison {comparison!r}")

        weight = raw.get("weight", 1.0)
        tolerance = raw.get("tolerance", 0.0)
        scale = raw.get("scale")
        minimum_improvement = raw.get("minimum_improvement", 0.0)
        regression_tolerance = raw.get("regression_tolerance", 0.0)
        numeric_fields = {
            "weight": weight,
            "tolerance": tolerance,
            "minimum_improvement": minimum_improvement,
            "regression_tolerance": regression_tolerance,
        }
        for name, number in numeric_fields.items():
            if not _is_number(number) or float(number) < 0:
                errors.append(f"{field}.{name} must be a non-negative number")
        if _is_number(weight) and float(weight) <= 0:
            errors.append(f"{field}.weight must be greater than zero")
        if scale is None and _is_number(target):
            scale = max(abs(float(target)), 1.0)
        if not _is_number(scale) or float(scale) <= 0:
            errors.append(f"{field}.scale must be a positive number")

        normalized.append(
            {
                "objective_id": objective_id,
                "description": str(raw.get("description") or "").strip(),
                "metric": metric,
                "comparison": comparison,
                "target": copy.deepcopy(target),
                "tolerance": float(tolerance) if _is_number(tolerance) else 0.0,
                "scale": float(scale) if _is_number(scale) else 1.0,
                "weight": float(weight) if _is_number(weight) else 0.0,
                "acceptance": acceptance,
                "minimum_improvement": (
                    float(minimum_improvement) if _is_number(minimum_improvement) else 0.0
                ),
                "regression_tolerance": (
                    float(regression_tolerance) if _is_number(regression_tolerance) else 0.0
                ),
                "required": bool(raw.get("required", True)),
            }
        )
    return normalized


def _objective_score(objective: Mapping[str, Any], value: Any) -> tuple[float, bool]:
    comparison = objective["comparison"]
    target = objective["target"]
    tolerance = float(objective["tolerance"])
    scale = float(objective["scale"])

    if comparison == "equal":
        passed = _metric_equal(value, target, tolerance)
        if passed:
            return 1.0, True
        if _is_number(value) and _is_number(target):
            score = max(0.0, 1.0 - abs(float(value) - float(target)) / scale)
            return score, False
        return 0.0, False

    if not _is_number(value) or not _is_number(target):
        return 0.0, False
    numeric_value = float(value)
    numeric_target = float(target)
    if comparison == "target":
        distance = abs(numeric_value - numeric_target)
        return max(0.0, 1.0 - distance / scale), distance <= tolerance
    if comparison == "at_least":
        passed = numeric_value >= numeric_target - tolerance
        return max(0.0, min(1.0, 1.0 - max(0.0, numeric_target - numeric_value) / scale)), passed
    passed = numeric_value <= numeric_target + tolerance
    return max(0.0, min(1.0, 1.0 - max(0.0, numeric_value - numeric_target) / scale)), passed


def _score_state(
    state: Mapping[str, Any],
    objectives: Sequence[Mapping[str, Any]],
) -> dict[str, Any]:
    metrics = state.get("metrics", {})
    weighted_score = 0.0
    total_weight = 0.0
    missing_metrics: list[str] = []
    objective_scores: list[dict[str, Any]] = []
    all_required_passed = True
    for objective in objectives:
        metric = objective["metric"]
        if metric not in metrics:
            missing_metrics.append(metric)
            score, passed = 0.0, False
        else:
            score, passed = _objective_score(objective, metrics[metric])
        weight = float(objective["weight"])
        weighted_score += score * weight
        total_weight += weight
        if objective["required"] and not passed:
            all_required_passed = False
        objective_scores.append(
            {
                "objective_id": objective["objective_id"],
                "metric": metric,
                "value": copy.deepcopy(metrics.get(metric)),
                "score": round(score, 6),
                "passed": passed,
            }
        )
    score = weighted_score / total_weight if total_weight else 0.0
    compile_healthy = bool(state.get("compile", {}).get("healthy"))
    return {
        "state_id": state.get("state_id"),
        "score": round(score, 6),
        "compile_healthy": compile_healthy,
        "evidence_complete": not missing_metrics,
        "missing_metrics": sorted(set(missing_metrics)),
        "all_required_passed": all_required_passed and not missing_metrics,
        "objective_scores": objective_scores,
    }


def _rank_states(ledger: Mapping[str, Any]) -> dict[str, Any]:
    objectives = ledger["objectives"]
    ranked: list[dict[str, Any]] = []
    for sequence, state in enumerate(ledger["states"]):
        scored = _score_state(state, objectives)
        eligible = (
            bool(state.get("eligible_for_best", True))
            and scored["compile_healthy"]
            and scored["evidence_complete"]
        )
        scored["eligible"] = eligible
        scored["sequence"] = sequence
        scored["disposition"] = state.get("disposition", "accepted")
        ranked.append(scored)

    ranked.sort(
        key=lambda item: (
            not item["eligible"],
            -float(item["score"]),
            not item["all_required_passed"],
            int(item["sequence"]),
            str(item["state_id"]),
        )
    )
    eligible = [item for item in ranked if item["eligible"]]
    selected = eligible[0] if eligible else None
    return {
        "selected_state_id": selected["state_id"] if selected else "",
        "selected_score": selected["score"] if selected else None,
        "selection_reason": (
            "highest weighted objective score among compile-healthy accepted states; "
            "earliest state wins deterministic ties"
        ),
        "ranked_states": ranked,
    }


def create_refinement_ledger(
    system_path: str,
    objectives: list[dict[str, Any]],
    baseline_state: dict[str, Any],
    *,
    run_id: str = "",
    max_iterations: int = 4,
    effect_spec_hash: str = "",
) -> dict[str, Any]:
    """Create a mutation-free ledger from a compile-healthy baseline."""

    errors: list[str] = []
    normalized_path = str(system_path or "").strip()
    if not normalized_path.startswith(TEMP_NIAGARA_ROOT):
        errors.append(f"system_path must be under generated Niagara temp root {TEMP_NIAGARA_ROOT}")
    normalized_objectives = _normalize_objectives(objectives, errors)
    normalized_baseline = _normalize_state(
        baseline_state,
        system_path=normalized_path,
        field="baseline_state",
        errors=errors,
    )
    if not isinstance(max_iterations, int) or isinstance(max_iterations, bool):
        errors.append("max_iterations must be an integer")
    elif not 1 <= max_iterations <= MAX_REFINEMENT_ITERATIONS:
        errors.append(f"max_iterations must be between 1 and {MAX_REFINEMENT_ITERATIONS}")

    baseline_metrics = normalized_baseline.get("metrics", {})
    for objective in normalized_objectives:
        if objective["metric"] not in baseline_metrics:
            errors.append(
                f"baseline_state.metrics is missing objective metric {objective['metric']!r}"
            )
    if normalized_baseline and not normalized_baseline.get("compile", {}).get("healthy"):
        errors.append("baseline_state compile gate is not healthy")

    normalized_run_id = str(run_id or "").strip()
    if normalized_run_id:
        _validate_identifier(normalized_run_id, "run_id", errors)
    else:
        digest_payload = {
            "system_path": normalized_path,
            "objectives": normalized_objectives,
            "baseline_state": normalized_baseline,
        }
        digest = hashlib.sha256(
            json.dumps(digest_payload, sort_keys=True, default=str).encode("utf-8")
        ).hexdigest()[:12]
        normalized_run_id = f"niagara_refinement_{digest}"

    if errors:
        return _error(
            "Niagara refinement ledger validation failed",
            "niagara_refinement_ledger_invalid",
            validation_errors=errors,
        )

    baseline_record = copy.deepcopy(normalized_baseline)
    baseline_record.update(
        {
            "disposition": "baseline",
            "eligible_for_best": True,
            "patch_id": "",
        }
    )
    ledger: dict[str, Any] = {
        "schema_version": REFINEMENT_LEDGER_SCHEMA_VERSION,
        "run_id": normalized_run_id,
        "system_path": normalized_path,
        "effect_spec_hash": str(effect_spec_hash or "").strip(),
        "max_iterations": max_iterations,
        "status": "active",
        "objectives": normalized_objectives,
        "baseline_state_id": baseline_record["state_id"],
        "current_state_id": baseline_record["state_id"],
        "selected_best_state_id": baseline_record["state_id"],
        "states": [baseline_record],
        "iterations": [],
    }
    selection = _rank_states(ledger)
    ledger["selected_best_state_id"] = selection["selected_state_id"]
    return tool_success(
        dry_run=True,
        mutation_performed=False,
        writes_assets=False,
        ledger=ledger,
        best_state_selection=selection,
    )


def _validate_ledger(ledger: Any) -> tuple[dict[str, Any] | None, list[str]]:
    errors: list[str] = []
    if not isinstance(ledger, Mapping):
        return None, ["ledger must be an object"]
    copied = copy.deepcopy(dict(ledger))
    if copied.get("schema_version") != REFINEMENT_LEDGER_SCHEMA_VERSION:
        errors.append(
            f"ledger.schema_version must be {REFINEMENT_LEDGER_SCHEMA_VERSION}"
        )
    system_path = str(copied.get("system_path") or "").strip()
    if not system_path.startswith(TEMP_NIAGARA_ROOT):
        errors.append(f"ledger.system_path must be under {TEMP_NIAGARA_ROOT}")
    objectives = _normalize_objectives(copied.get("objectives"), errors)
    copied["objectives"] = objectives
    states = copied.get("states")
    if not isinstance(states, list) or not states:
        errors.append("ledger.states must be a non-empty list")
        states = []
    state_ids: set[str] = set()
    for index, state in enumerate(states):
        if not isinstance(state, Mapping):
            errors.append(f"ledger.states[{index}] must be an object")
            continue
        state_id = str(state.get("state_id") or "")
        if state_id in state_ids:
            errors.append(f"ledger.states[{index}].state_id is duplicated: {state_id}")
        state_ids.add(state_id)
        if str(state.get("system_path") or "") != system_path:
            errors.append(f"ledger.states[{index}].system_path must match ledger.system_path")
    if copied.get("current_state_id") not in state_ids:
        errors.append("ledger.current_state_id does not identify a ledger state")
    iterations = copied.get("iterations")
    if not isinstance(iterations, list):
        errors.append("ledger.iterations must be a list")
    max_iterations = copied.get("max_iterations")
    if not isinstance(max_iterations, int) or isinstance(max_iterations, bool):
        errors.append("ledger.max_iterations must be an integer")
    elif not 1 <= max_iterations <= MAX_REFINEMENT_ITERATIONS:
        errors.append(f"ledger.max_iterations must be between 1 and {MAX_REFINEMENT_ITERATIONS}")
    return copied, errors


def evaluate_refinement_iteration(
    ledger: dict[str, Any],
    patch_id: str,
    candidate_state: dict[str, Any],
    *,
    objective_ids: list[str] | None = None,
    base_state_id: str = "",
) -> dict[str, Any]:
    """Append one evidence-backed candidate and decide keep versus selective rollback."""

    normalized_ledger, errors = _validate_ledger(ledger)
    if normalized_ledger is None:
        return _error(
            "Niagara refinement ledger validation failed",
            "niagara_refinement_ledger_invalid",
            validation_errors=errors,
        )
    iterations = normalized_ledger.get("iterations", [])
    if len(iterations) >= int(normalized_ledger.get("max_iterations", 0)):
        errors.append("ledger has reached max_iterations")

    normalized_patch_id = _validate_identifier(patch_id, "patch_id", errors)
    state_by_id = {state["state_id"]: state for state in normalized_ledger.get("states", [])}
    normalized_base_state_id = str(
        base_state_id or normalized_ledger.get("current_state_id") or ""
    ).strip()
    base_state = state_by_id.get(normalized_base_state_id)
    if base_state is None:
        errors.append("base_state_id does not identify a ledger state")

    candidate = _normalize_state(
        candidate_state,
        system_path=normalized_ledger.get("system_path", ""),
        field="candidate_state",
        errors=errors,
    )
    if candidate.get("state_id") in state_by_id:
        errors.append(f"candidate_state.state_id already exists: {candidate.get('state_id')}")

    objective_by_id = {
        objective["objective_id"]: objective
        for objective in normalized_ledger.get("objectives", [])
    }
    selected_objective_ids = objective_ids or list(objective_by_id)
    if not isinstance(selected_objective_ids, list) or not selected_objective_ids:
        errors.append("objective_ids must be a non-empty list when provided")
        selected_objective_ids = []
    if len(set(selected_objective_ids)) != len(selected_objective_ids):
        errors.append("objective_ids contains duplicates")
    for objective_id in selected_objective_ids:
        if objective_id not in objective_by_id:
            errors.append(f"objective_ids contains unknown objective: {objective_id}")
    if base_state is not None:
        for objective_id, objective in objective_by_id.items():
            metric = objective["metric"]
            if metric not in base_state.get("metrics", {}):
                errors.append(f"base state is missing objective metric {metric!r}")
            if metric not in candidate.get("metrics", {}):
                errors.append(f"candidate state is missing objective metric {metric!r}")

    if errors:
        return _error(
            "Niagara refinement iteration validation failed",
            "niagara_refinement_iteration_invalid",
            validation_errors=errors,
        )

    compile_gate_passed = bool(candidate["compile"]["healthy"])
    objective_results: list[dict[str, Any]] = []
    keep_ids: list[str] = []
    rollback_ids: list[str] = []
    selected_objective_id_set = set(selected_objective_ids)
    guarded_objective_ids = [
        objective_id
        for objective_id in objective_by_id
        if objective_id not in selected_objective_id_set
    ]
    for objective_id, objective in objective_by_id.items():
        is_selected = objective_id in selected_objective_id_set
        metric = objective["metric"]
        baseline_value = base_state["metrics"][metric]
        candidate_value = candidate["metrics"][metric]
        baseline_score, baseline_passed = _objective_score(objective, baseline_value)
        candidate_score, candidate_passed = _objective_score(objective, candidate_value)
        delta = candidate_score - baseline_score
        improved = delta > float(objective["minimum_improvement"])
        regressed = delta < -float(objective["regression_tolerance"])
        acceptance = objective["acceptance"]
        if not compile_gate_passed and is_selected:
            accepted = False
            reason = "candidate failed the compile gate"
        elif not is_selected:
            accepted = not regressed
            reason = (
                "unselected objective did not regress"
                if accepted
                else "unselected objective regressed beyond tolerance"
            )
        elif acceptance == "target":
            accepted = candidate_passed
            reason = "candidate reached the objective target" if accepted else "candidate did not reach the objective target"
        elif acceptance == "improvement":
            accepted = improved or (candidate_passed and not baseline_passed)
            reason = "candidate measurably improved the objective" if accepted else "candidate did not meet minimum improvement"
        else:
            accepted = not regressed
            reason = "candidate did not regress the objective" if accepted else "candidate regressed beyond tolerance"
        decision = "keep" if accepted else "rollback"
        if is_selected and accepted:
            keep_ids.append(objective_id)
        elif not accepted:
            rollback_ids.append(objective_id)
        objective_results.append(
            {
                "objective_id": objective_id,
                "evaluation_role": "selected" if is_selected else "global_non_regression_guard",
                "metric": metric,
                "baseline_value": copy.deepcopy(baseline_value),
                "candidate_value": copy.deepcopy(candidate_value),
                "baseline_score": round(baseline_score, 6),
                "candidate_score": round(candidate_score, 6),
                "score_delta": round(delta, 6),
                "baseline_passed": baseline_passed,
                "candidate_passed": candidate_passed,
                "improved": improved,
                "regressed": regressed,
                "decision": decision,
                "reason": reason,
            }
        )

    next_action = "keep_candidate" if not rollback_ids else "selectively_rollback"
    candidate_record = copy.deepcopy(candidate)
    candidate_record.update(
        {
            "disposition": "accepted" if not rollback_ids else "rejected_pending_rollback",
            "eligible_for_best": not rollback_ids,
            "patch_id": normalized_patch_id,
        }
    )
    normalized_ledger["states"].append(candidate_record)
    iteration_index = len(iterations) + 1
    iteration = {
        "iteration_index": iteration_index,
        "patch_id": normalized_patch_id,
        "base_state_id": normalized_base_state_id,
        "candidate_state_id": candidate["state_id"],
        "compile_gate": copy.deepcopy(candidate["compile"]),
        "objective_results": objective_results,
        "keep_objective_ids": keep_ids,
        "rollback_objective_ids": rollback_ids,
        "guarded_objective_ids": guarded_objective_ids,
        "next_action": next_action,
        "rollback_plan": {
            "dispatch_performed": False,
            "restore_state_id": normalized_base_state_id,
            "objective_ids": rollback_ids,
            "strategy": "restore_only_rejected_objective_operations",
        },
    }
    normalized_ledger["iterations"].append(iteration)
    if not rollback_ids:
        normalized_ledger["current_state_id"] = candidate["state_id"]
    if len(normalized_ledger["iterations"]) >= normalized_ledger["max_iterations"]:
        normalized_ledger["status"] = "iteration_limit_reached"
    selection = _rank_states(normalized_ledger)
    normalized_ledger["selected_best_state_id"] = selection["selected_state_id"]

    return tool_success(
        dry_run=True,
        mutation_performed=False,
        writes_assets=False,
        decision=next_action,
        compile_gate_passed=compile_gate_passed,
        keep_objective_ids=keep_ids,
        rollback_objective_ids=rollback_ids,
        guarded_objective_ids=guarded_objective_ids,
        objective_results=objective_results,
        rollback_plan=iteration["rollback_plan"],
        best_state_selection=selection,
        ledger=normalized_ledger,
    )


def select_best_refinement_state(ledger: dict[str, Any]) -> dict[str, Any]:
    """Rank accepted compile-healthy historical states without mutating the ledger."""

    normalized_ledger, errors = _validate_ledger(ledger)
    if normalized_ledger is None or errors:
        return _error(
            "Niagara refinement ledger validation failed",
            "niagara_refinement_ledger_invalid",
            validation_errors=errors,
        )
    selection = _rank_states(normalized_ledger)
    if not selection["selected_state_id"]:
        return _error(
            "No compile-healthy accepted refinement state is eligible",
            "niagara_refinement_best_state_unavailable",
            best_state_selection=selection,
        )
    return tool_success(
        dry_run=True,
        mutation_performed=False,
        writes_assets=False,
        best_state_selection=selection,
    )
