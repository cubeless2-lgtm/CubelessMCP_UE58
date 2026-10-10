"""Deterministic, objective-bound visual Critic for Niagara preview evidence.

Only rendered evidence under ``Saved/MCP/NiagaraReviews`` is admitted. The
service performs no Unreal calls and no writes; it computes reproducible image
metrics that can be attached directly to the Niagara refinement ledger.
"""

from __future__ import annotations

import hashlib
import math
import re
from collections import defaultdict
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import Any

from tools.response_schema import tool_error, tool_success

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REVIEW_ROOT = (
    REPO_ROOT / "MCPGameProject" / "Saved" / "MCP" / "NiagaraReviews"
).resolve()

MAX_FRAME_COUNT = 32
MAX_FRAME_BYTES = 32 * 1024 * 1024
MAX_ANALYSIS_WIDTH = 640
MAX_ANALYSIS_HEIGHT = 640
ALLOWED_EXTENSIONS = {".png", ".jpg", ".jpeg"}
IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9_.:-]{0,127}$")
ALLOWED_COMPARISONS = {
    "at_least",
    "at_most",
    "target",
    "delta_at_least",
    "delta_at_most",
}
AGGREGATE_METRICS = {
    "bright_neutral_ratio_mean",
    "bright_neutral_ratio_min",
    "bright_neutral_ratio_max",
    "foreground_ratio_mean",
    "edge_pixel_ratio_mean",
    "mean_luma_mean",
    "luma_stddev_mean",
    "bright_centroid_x_mean",
    "bright_centroid_y_mean",
    "visible_frame_ratio",
    "temporal_difference_ratio_mean",
    "temporal_mean_abs_difference_mean",
}


def _error(message: str, error_code: str, **fields: Any) -> dict[str, Any]:
    return tool_error(
        message,
        error_code=error_code,
        dry_run=True,
        mutation_performed=False,
        writes_assets=False,
        critic_mode="deterministic_image_metrics",
        **fields,
    )


def _is_number(value: Any) -> bool:
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(float(value))
    )


def _normalize_roi(value: Any, errors: list[str]) -> tuple[float, float, float, float]:
    if value is None:
        return (0.15, 0.15, 0.85, 0.80)
    if not isinstance(value, list) or len(value) != 4 or not all(_is_number(item) for item in value):
        errors.append("roi must be [x0, y0, x1, y1] with finite normalized numbers")
        return (0.15, 0.15, 0.85, 0.80)
    x0, y0, x1, y1 = (float(item) for item in value)
    if not (0.0 <= x0 < x1 <= 1.0 and 0.0 <= y0 < y1 <= 1.0):
        errors.append("roi must satisfy 0 <= x0 < x1 <= 1 and 0 <= y0 < y1 <= 1")
        return (0.15, 0.15, 0.85, 0.80)
    return (x0, y0, x1, y1)


def _resolve_frame_paths(
    values: Any,
    *,
    field: str,
    required: bool,
    errors: list[str],
) -> list[Path]:
    if values is None and not required:
        return []
    if not isinstance(values, list) or (required and not values):
        errors.append(f"{field} must be a non-empty list")
        return []
    if len(values) > MAX_FRAME_COUNT:
        errors.append(f"{field} exceeds {MAX_FRAME_COUNT} frames")

    resolved: list[Path] = []
    seen: set[Path] = set()
    for index, value in enumerate(values):
        text = str(value or "").strip()
        if not text:
            errors.append(f"{field}[{index}] is empty")
            continue
        path = Path(text)
        if not path.is_absolute():
            path = DEFAULT_REVIEW_ROOT / path
        try:
            path = path.resolve(strict=True)
        except OSError:
            errors.append(f"{field}[{index}] does not exist: {text}")
            continue
        if not path.is_relative_to(DEFAULT_REVIEW_ROOT):
            errors.append(
                f"{field}[{index}] must resolve under {DEFAULT_REVIEW_ROOT}"
            )
            continue
        if path.suffix.lower() not in ALLOWED_EXTENSIONS:
            errors.append(f"{field}[{index}] must be PNG or JPEG: {path}")
            continue
        if not path.is_file():
            errors.append(f"{field}[{index}] is not a file: {path}")
            continue
        if path.stat().st_size > MAX_FRAME_BYTES:
            errors.append(f"{field}[{index}] exceeds {MAX_FRAME_BYTES} bytes")
            continue
        if path in seen:
            errors.append(f"{field}[{index}] duplicates another frame: {path}")
            continue
        seen.add(path)
        resolved.append(path)
    return resolved


def _crop_and_scale(path: Path, roi: tuple[float, float, float, float]):
    from PIL import Image

    with Image.open(path) as source:
        image = source.convert("RGB")
    width, height = image.size
    x0 = max(0, min(width - 1, int(width * roi[0])))
    y0 = max(0, min(height - 1, int(height * roi[1])))
    x1 = max(x0 + 1, min(width, int(width * roi[2])))
    y1 = max(y0 + 1, min(height, int(height * roi[3])))
    cropped = image.crop((x0, y0, x1, y1))
    cropped.thumbnail((MAX_ANALYSIS_WIDTH, MAX_ANALYSIS_HEIGHT))
    return image.size, (x0, y0, x1, y1), cropped


def _analyze_frame(
    path: Path,
    roi: tuple[float, float, float, float],
) -> dict[str, Any]:
    from PIL import ImageFilter, ImageStat

    original_size, crop_pixels, image = _crop_and_scale(path, roi)
    width, height = image.size
    pixel_count = max(1, width * height)
    data = image.tobytes()
    bright_neutral_count = 0
    foreground_count = 0
    bright_x_sum = 0.0
    bright_y_sum = 0.0
    for offset in range(0, len(data), 3):
        red, green, blue = data[offset], data[offset + 1], data[offset + 2]
        luma = (red * 299 + green * 587 + blue * 114) // 1000
        pixel_index = offset // 3
        x = pixel_index % width
        y = pixel_index // width
        if luma >= 72:
            foreground_count += 1
        if luma > 185 and max(red, green, blue) - min(red, green, blue) < 90:
            bright_neutral_count += 1
            bright_x_sum += x
            bright_y_sum += y

    grayscale = image.convert("L")
    stats = ImageStat.Stat(grayscale)
    edge_image = grayscale.filter(ImageFilter.FIND_EDGES)
    edge_histogram = edge_image.histogram()
    edge_count = sum(edge_histogram[33:])
    bright_centroid_x = (
        bright_x_sum / bright_neutral_count / max(1, width - 1)
        if bright_neutral_count
        else 0.5
    )
    bright_centroid_y = (
        bright_y_sum / bright_neutral_count / max(1, height - 1)
        if bright_neutral_count
        else 0.5
    )
    return {
        "path": str(path),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "file_bytes": path.stat().st_size,
        "original_width": original_size[0],
        "original_height": original_size[1],
        "analysis_width": width,
        "analysis_height": height,
        "crop_pixels": list(crop_pixels),
        "mean_luma": float(stats.mean[0]),
        "luma_stddev": float(stats.stddev[0]),
        "foreground_ratio": foreground_count / pixel_count,
        "bright_neutral_ratio": bright_neutral_count / pixel_count,
        "edge_pixel_ratio": edge_count / pixel_count,
        "bright_centroid_x": bright_centroid_x,
        "bright_centroid_y": bright_centroid_y,
        "visible": bright_neutral_count / pixel_count >= 0.0005,
    }


def _mean(values: Sequence[float]) -> float:
    return sum(values) / len(values) if values else 0.0


def _frame_difference(
    left_path: Path,
    right_path: Path,
    roi: tuple[float, float, float, float],
) -> dict[str, float]:
    from PIL import Image, ImageChops, ImageStat

    _, _, left = _crop_and_scale(left_path, roi)
    _, _, right = _crop_and_scale(right_path, roi)
    target_size = (
        min(left.width, right.width),
        min(left.height, right.height),
    )
    left = left.resize(target_size, Image.Resampling.BILINEAR)
    right = right.resize(target_size, Image.Resampling.BILINEAR)
    difference = ImageChops.difference(left, right)
    difference_data = difference.tobytes()
    pixel_count = max(1, target_size[0] * target_size[1])
    changed = 0
    strong = 0
    for offset in range(0, len(difference_data), 3):
        mean_difference = (
            difference_data[offset]
            + difference_data[offset + 1]
            + difference_data[offset + 2]
        ) / 3.0
        if mean_difference > 14.0:
            changed += 1
        if mean_difference > 36.0:
            strong += 1
    stats = ImageStat.Stat(difference)
    mean_abs = sum(stats.mean) / 3.0
    return {
        "difference_pixel_ratio": changed / pixel_count,
        "strong_difference_pixel_ratio": strong / pixel_count,
        "mean_abs_difference": mean_abs,
    }


def _aggregate_frames(
    frame_paths: Sequence[Path],
    roi: tuple[float, float, float, float],
) -> tuple[list[dict[str, Any]], dict[str, float]]:
    frames = [_analyze_frame(path, roi) for path in frame_paths]
    bright_values = [float(frame["bright_neutral_ratio"]) for frame in frames]
    temporal = [
        _frame_difference(frame_paths[index - 1], frame_paths[index], roi)
        for index in range(1, len(frame_paths))
    ]
    aggregate = {
        "bright_neutral_ratio_mean": _mean(bright_values),
        "bright_neutral_ratio_min": min(bright_values, default=0.0),
        "bright_neutral_ratio_max": max(bright_values, default=0.0),
        "foreground_ratio_mean": _mean(
            [float(frame["foreground_ratio"]) for frame in frames]
        ),
        "edge_pixel_ratio_mean": _mean(
            [float(frame["edge_pixel_ratio"]) for frame in frames]
        ),
        "mean_luma_mean": _mean([float(frame["mean_luma"]) for frame in frames]),
        "luma_stddev_mean": _mean(
            [float(frame["luma_stddev"]) for frame in frames]
        ),
        "bright_centroid_x_mean": _mean(
            [float(frame["bright_centroid_x"]) for frame in frames]
        ),
        "bright_centroid_y_mean": _mean(
            [float(frame["bright_centroid_y"]) for frame in frames]
        ),
        "visible_frame_ratio": _mean(
            [1.0 if frame["visible"] else 0.0 for frame in frames]
        ),
        "temporal_difference_ratio_mean": _mean(
            [item["difference_pixel_ratio"] for item in temporal]
        ),
        "temporal_mean_abs_difference_mean": _mean(
            [item["mean_abs_difference"] for item in temporal]
        ),
    }
    return frames, aggregate


def _compare_frame_sets(
    baseline_paths: Sequence[Path],
    candidate_paths: Sequence[Path],
    roi: tuple[float, float, float, float],
) -> dict[str, Any]:
    if not baseline_paths:
        return {
            "paired_frame_count": 0,
            "difference_pixel_ratio_mean": 0.0,
            "strong_difference_pixel_ratio_mean": 0.0,
            "mean_abs_difference_mean": 0.0,
            "pairs": [],
        }
    pairs = [
        _frame_difference(baseline_path, candidate_path, roi)
        for baseline_path, candidate_path in zip(baseline_paths, candidate_paths)
    ]
    return {
        "paired_frame_count": len(pairs),
        "difference_pixel_ratio_mean": _mean(
            [item["difference_pixel_ratio"] for item in pairs]
        ),
        "strong_difference_pixel_ratio_mean": _mean(
            [item["strong_difference_pixel_ratio"] for item in pairs]
        ),
        "mean_abs_difference_mean": _mean(
            [item["mean_abs_difference"] for item in pairs]
        ),
        "pairs": pairs,
    }


def _normalize_criteria(value: Any, *, has_baseline: bool, errors: list[str]) -> list[dict[str, Any]]:
    if not isinstance(value, list) or not value:
        errors.append("criteria must be a non-empty list")
        return []
    normalized: list[dict[str, Any]] = []
    seen: set[tuple[str, str]] = set()
    seen_metric_keys: set[str] = set()
    for index, raw in enumerate(value):
        field = f"criteria[{index}]"
        if not isinstance(raw, Mapping):
            errors.append(f"{field} must be an object")
            continue
        objective_id = str(raw.get("objective_id") or "").strip()
        criterion_id = str(raw.get("criterion_id") or "").strip()
        for name, identifier in (("objective_id", objective_id), ("criterion_id", criterion_id)):
            if not IDENTIFIER_PATTERN.fullmatch(identifier):
                errors.append(f"{field}.{name} must match {IDENTIFIER_PATTERN.pattern}")
        identity = (objective_id, criterion_id)
        if identity in seen:
            errors.append(f"{field} duplicates objective/criterion identity {identity}")
        seen.add(identity)
        metric = str(raw.get("metric") or "").strip()
        if metric not in AGGREGATE_METRICS:
            errors.append(f"{field}.metric must be one of {sorted(AGGREGATE_METRICS)}")
        comparison = str(raw.get("comparison") or "").strip()
        if comparison not in ALLOWED_COMPARISONS:
            errors.append(f"{field}.comparison must be one of {sorted(ALLOWED_COMPARISONS)}")
        if comparison.startswith("delta_") and not has_baseline:
            errors.append(f"{field}.comparison {comparison!r} requires baseline_frame_paths")
        target = raw.get("target")
        tolerance = raw.get("tolerance", 0.0)
        weight = raw.get("weight", 1.0)
        if not _is_number(target):
            errors.append(f"{field}.target must be a finite number")
        if not _is_number(tolerance) or float(tolerance) < 0:
            errors.append(f"{field}.tolerance must be a non-negative finite number")
        if not _is_number(weight) or float(weight) <= 0:
            errors.append(f"{field}.weight must be a positive finite number")
        metric_key = str(
            raw.get("ledger_metric_key")
            or f"visual.{objective_id}.{criterion_id}"
        ).strip()
        if not metric_key:
            errors.append(f"{field}.ledger_metric_key must not be empty")
        if metric_key in seen_metric_keys:
            errors.append(f"{field}.ledger_metric_key is duplicated: {metric_key}")
        seen_metric_keys.add(metric_key)
        normalized.append(
            {
                "objective_id": objective_id,
                "criterion_id": criterion_id,
                "description": str(raw.get("description") or "").strip(),
                "metric": metric,
                "comparison": comparison,
                "target": float(target) if _is_number(target) else 0.0,
                "tolerance": float(tolerance) if _is_number(tolerance) else 0.0,
                "weight": float(weight) if _is_number(weight) else 0.0,
                "required": bool(raw.get("required", True)),
                "ledger_metric_key": metric_key,
            }
        )
    objective_requirements: dict[str, bool] = {}
    for criterion in normalized:
        objective_id = criterion["objective_id"]
        objective_requirements[objective_id] = (
            objective_requirements.get(objective_id, False)
            or bool(criterion["required"])
        )
    for objective_id, has_required_criterion in objective_requirements.items():
        if not has_required_criterion:
            errors.append(
                f"visual objective {objective_id!r} must contain at least one required criterion"
            )
    return normalized


def _criterion_score(observed: float, target: float, comparison: str, tolerance: float) -> tuple[float, bool]:
    scale = max(abs(target), 0.01)
    if comparison in {"at_least", "delta_at_least"}:
        passed = observed >= target - tolerance
        return max(0.0, min(1.0, observed / scale if scale else 1.0)), passed
    if comparison in {"at_most", "delta_at_most"}:
        passed = observed <= target + tolerance
        score = 1.0 if passed else max(0.0, 1.0 - (observed - target) / scale)
        return score, passed
    distance = abs(observed - target)
    return max(0.0, 1.0 - distance / scale), distance <= tolerance


def critique_preview_evidence(
    candidate_frame_paths: list[str],
    criteria: list[dict[str, Any]],
    *,
    baseline_frame_paths: list[str] | None = None,
    roi: list[float] | None = None,
) -> dict[str, Any]:
    """Analyze preview frames and bind deterministic findings to objective IDs."""

    errors: list[str] = []
    normalized_roi = _normalize_roi(roi, errors)
    candidate_paths = _resolve_frame_paths(
        candidate_frame_paths,
        field="candidate_frame_paths",
        required=True,
        errors=errors,
    )
    baseline_paths = _resolve_frame_paths(
        baseline_frame_paths,
        field="baseline_frame_paths",
        required=False,
        errors=errors,
    )
    if baseline_paths and len(baseline_paths) != len(candidate_paths):
        errors.append("baseline_frame_paths and candidate_frame_paths must have equal lengths")
    normalized_criteria = _normalize_criteria(
        criteria,
        has_baseline=bool(baseline_paths),
        errors=errors,
    )
    if errors:
        return _error(
            "Niagara visual Critic request validation failed",
            "niagara_visual_critic_request_invalid",
            validation_errors=errors,
            allowed_review_root=str(DEFAULT_REVIEW_ROOT),
        )

    try:
        candidate_frames, candidate_aggregate = _aggregate_frames(
            candidate_paths,
            normalized_roi,
        )
        if baseline_paths:
            baseline_frames, baseline_aggregate = _aggregate_frames(
                baseline_paths,
                normalized_roi,
            )
        else:
            baseline_frames, baseline_aggregate = [], {}
        comparison = _compare_frame_sets(
            baseline_paths,
            candidate_paths,
            normalized_roi,
        )
    except (OSError, ValueError) as exc:
        return _error(
            f"Niagara visual Critic image analysis failed: {exc}",
            "niagara_visual_critic_analysis_failed",
            exception_type=type(exc).__name__,
        )

    criterion_results: list[dict[str, Any]] = []
    baseline_ledger_metrics: dict[str, float] = {}
    candidate_ledger_metrics: dict[str, float] = {}
    baseline_objective_metrics: dict[str, float] = {}
    candidate_objective_metrics: dict[str, float] = {}
    objective_groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for criterion in normalized_criteria:
        metric = criterion["metric"]
        candidate_value = float(candidate_aggregate[metric])
        baseline_value = (
            float(baseline_aggregate[metric]) if baseline_aggregate else None
        )
        if criterion["comparison"].startswith("delta_"):
            observed = candidate_value - float(baseline_value)
        else:
            observed = candidate_value
        score, passed = _criterion_score(
            observed,
            criterion["target"],
            criterion["comparison"],
            criterion["tolerance"],
        )
        if baseline_value is None:
            baseline_score, baseline_passed = 0.0, False
        else:
            baseline_observed = (
                0.0
                if criterion["comparison"].startswith("delta_")
                else baseline_value
            )
            baseline_score, baseline_passed = _criterion_score(
                baseline_observed,
                criterion["target"],
                criterion["comparison"],
                criterion["tolerance"],
            )
        result = {
            **criterion,
            "baseline_value": baseline_value,
            "baseline_score": round(baseline_score, 6),
            "baseline_passed": baseline_passed,
            "candidate_value": candidate_value,
            "observed_value": observed,
            "score": round(score, 6),
            "passed": passed,
        }
        criterion_results.append(result)
        objective_groups[criterion["objective_id"]].append(result)
        candidate_ledger_metrics[criterion["ledger_metric_key"]] = candidate_value
        if baseline_value is not None:
            baseline_ledger_metrics[criterion["ledger_metric_key"]] = baseline_value

    objective_results: list[dict[str, Any]] = []
    for objective_id, results in objective_groups.items():
        total_weight = sum(float(result["weight"]) for result in results)
        weighted_score = sum(
            float(result["score"]) * float(result["weight"])
            for result in results
        )
        required_results = [result for result in results if result["required"]]
        baseline_weighted_score = sum(
            float(result["baseline_score"]) * float(result["weight"])
            for result in results
        )
        baseline_passed = bool(required_results) and all(
            result["baseline_passed"] for result in required_results
        )
        candidate_passed = bool(required_results) and all(
            result["passed"] for result in required_results
        )
        objective_metric_key = f"visual_objective.{objective_id}.passed"
        objective_results.append(
            {
                "objective_id": objective_id,
                "baseline_passed": baseline_passed,
                "baseline_score": (
                    round(baseline_weighted_score / total_weight, 6)
                    if total_weight
                    else 0.0
                ),
                "passed": candidate_passed,
                "score": round(weighted_score / total_weight, 6) if total_weight else 0.0,
                "ledger_metric_key": objective_metric_key,
                "criterion_ids": [result["criterion_id"] for result in results],
                "failed_criterion_ids": [
                    result["criterion_id"]
                    for result in required_results
                    if not result["passed"]
                ],
            }
        )
        baseline_objective_metrics[objective_metric_key] = 1.0 if baseline_passed else 0.0
        candidate_objective_metrics[objective_metric_key] = 1.0 if candidate_passed else 0.0
    critic_passed = all(result["passed"] for result in objective_results)
    return tool_success(
        dry_run=True,
        mutation_performed=False,
        writes_assets=False,
        critic_mode="deterministic_image_metrics",
        model_used=False,
        external_api_used=False,
        critic_passed=critic_passed,
        evidence_complete=True,
        review_root=str(DEFAULT_REVIEW_ROOT),
        roi=list(normalized_roi),
        baseline_frame_count=len(baseline_paths),
        candidate_frame_count=len(candidate_paths),
        baseline_frames=baseline_frames,
        candidate_frames=candidate_frames,
        baseline_aggregate=baseline_aggregate,
        candidate_aggregate=candidate_aggregate,
        comparison=comparison,
        criterion_results=criterion_results,
        objective_results=objective_results,
        ledger_metric_updates={
            "baseline": baseline_ledger_metrics,
            "candidate": candidate_ledger_metrics,
        },
        ledger_objective_updates={
            "baseline": baseline_objective_metrics,
            "candidate": candidate_objective_metrics,
        },
    )
