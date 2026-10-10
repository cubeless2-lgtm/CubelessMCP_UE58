"""Canonical, additive result contracts for mutation-capable MCP tools.

The registry applies this module after a tool returns.  It never invents a
successful postcondition: only an explicit handler-side verification object
can produce ``verification.status == "passed"``.  Legacy responses remain
available unchanged while callers gain one stable place to inspect dry-run
mode, mutation state, normalized counts, and verification strength.
"""

from __future__ import annotations

from collections.abc import Mapping, Sequence
from typing import Any

MUTATION_RESULT_CONTRACT_VERSION = "mutation_result_v1"
_FAILURE_STATUSES = frozenset(
    {
        "error",
        "failed",
        "failure",
        "fail",
        "disabled",
        "rate_limited",
        "blocked",
    }
)
_VERIFICATION_STATUSES = frozenset(
    {
        "passed",
        "failed",
        "pending",
        "timeout",
        "unverified",
        "not_completed",
        "not_applicable",
    }
)
_MUTATION_PERFORMED_FIELDS = (
    "mutation_performed",
    "mutation_applied_in_memory",
    "mutation_applied",
    "asset_mutation_performed",
    "asset_mutation_applied",
)
_MUTATION_ATTEMPTED_FIELDS = ("mutation_attempted",)
_CHANGED_FIELDS = (
    "changed",
    "graph_changed",
    "defaults_changed",
    "asset_defaults_changed",
)
_CANONICAL_COUNT_FIELDS = (
    "affected_count",
    "created_count",
    "updated_count",
    "removed_count",
    "skipped_count",
    "failed_count",
)
_COUNT_PREFIXES = {
    "created_count": ("created_", "added_"),
    "updated_count": ("updated_", "changed_", "modified_", "applied_"),
    "removed_count": ("removed_", "deleted_", "cleared_"),
    "skipped_count": ("skipped_",),
    "failed_count": ("failed_", "failure_"),
}
_COUNT_EXACT_ALIASES = {
    "created_count": ("added_count",),
    "updated_count": ("changed_count", "modified_count", "applied_count"),
    "removed_count": ("deleted_count", "cleared_count", "instances_removed"),
    "skipped_count": (),
    "failed_count": ("failure_count",),
}
_TRANSPORT_ENVELOPE_KEYS = (
    "unreal_response",
    "native_response",
    "response",
    "details",
    "result",
)
_TRANSPORT_MARKER_KEYS = ("transport", "transport_marker", "transport_state")
_TRANSPORT_MARKER_TYPE = "unreal_bridge_transport_v1"
_MAX_TRANSPORT_ENVELOPE_DEPTH = 6
_MAX_TRANSPORT_ENVELOPE_MAPPINGS = 64
_SAFETY_MIRROR_FIELDS = (
    "partial_mutation",
    "partial_target_modified",
    "completed_mutation_count",
    "completed_mutation_stages",
    "failed_stage",
    "failed_stage_effect",
    "failed_stage_mutation_performed",
    "mutation_state_unknown",
    "command_may_still_be_in_flight",
    "do_not_retry",
    "workflow_recovery_required",
    "recovery_steps",
    "recovery_guidance",
    "restore_errors",
)
_TRUE_DOMINANT_SAFETY_FIELDS = frozenset(
    {
        "partial_mutation",
        "partial_target_modified",
        "failed_stage_mutation_performed",
        "mutation_state_unknown",
        "command_may_still_be_in_flight",
        "do_not_retry",
        "workflow_recovery_required",
    }
)


def _response_sources(response: Mapping[str, Any]) -> list[Mapping[str, Any]]:
    """Return bounded known bridge envelopes in stable precedence order."""

    sources: list[Mapping[str, Any]] = []
    pending: list[Mapping[str, Any]] = [response]
    seen: set[int] = set()
    cursor = 0
    envelope_keys = ("result",) + tuple(
        key for key in _TRANSPORT_ENVELOPE_KEYS if key != "result"
    )
    while cursor < len(pending) and len(sources) < _MAX_TRANSPORT_ENVELOPE_MAPPINGS:
        candidate = pending[cursor]
        cursor += 1
        candidate_id = id(candidate)
        if candidate_id in seen:
            continue
        seen.add(candidate_id)
        sources.append(candidate)
        for wrapper_key in envelope_keys:
            nested = candidate.get(wrapper_key)
            if isinstance(nested, Mapping):
                pending.append(nested)
    return sources


def find_unknown_transport_marker(response: Any) -> Mapping[str, Any] | None:
    """Find a bounded, tagged unknown-outcome marker in known envelopes."""

    if not isinstance(response, Mapping):
        return None

    pending: list[tuple[Mapping[str, Any], int]] = [(response, 0)]
    seen: set[int] = set()
    mappings_examined = 0
    while pending and mappings_examined < _MAX_TRANSPORT_ENVELOPE_MAPPINGS:
        candidate, depth = pending.pop()
        candidate_id = id(candidate)
        if candidate_id in seen:
            continue
        seen.add(candidate_id)
        mappings_examined += 1

        for marker_key in _TRANSPORT_MARKER_KEYS:
            marker = candidate.get(marker_key)
            if not isinstance(marker, Mapping):
                continue
            if (
                marker.get("marker_type") == _TRANSPORT_MARKER_TYPE
                and (
                    marker.get("request_write_started") is True
                    or marker.get("request_received_by_server") is True
                    or marker.get("command_dispatch_submitted") is True
                )
                and str(marker.get("command_outcome", "")).strip().lower() == "unknown"
            ):
                return marker

        if depth >= _MAX_TRANSPORT_ENVELOPE_DEPTH:
            continue
        for wrapper_key in _TRANSPORT_ENVELOPE_KEYS:
            nested = candidate.get(wrapper_key)
            if isinstance(nested, Mapping):
                pending.append((nested, depth + 1))
    return None


def composite_failure_safety_fields(
    response: Any,
    *,
    failed_stage: str,
    mutation_relevant: bool,
    completed_mutation_stages: Sequence[str] = (),
    failed_stage_mutation_performed: bool = False,
) -> dict[str, Any]:
    """Preserve one composite-stage failure with explicit effect ownership.

    Composite tools must classify the failed stage before exposing its raw
    response. Read failures intentionally use a non-canonical envelope so they
    cannot be mistaken for an unknown mutation by the outer result contract.
    """

    completed = [str(stage) for stage in completed_mutation_stages if str(stage)]
    response_key = "unreal_response" if mutation_relevant else "read_unreal_response"
    fields: dict[str, Any] = {
        "failed_stage": failed_stage,
        "failed_stage_effect": "mutation" if mutation_relevant else "read",
        response_key: response,
    }
    if not mutation_relevant and not completed:
        fields.update(
            {
                "mutation_attempted": False,
                "mutation_performed": False,
                "changed": False,
            }
        )
    if completed or failed_stage_mutation_performed:
        fields.update(
            {
                "partial_mutation": True,
                "mutation_performed": True,
                "do_not_retry": True,
            }
        )
        if completed:
            fields.update(
                {
                    "completed_mutation_count": len(completed),
                    "completed_mutation_stages": completed,
                }
            )
        if failed_stage_mutation_performed:
            fields["failed_stage_mutation_performed"] = True

    explicit_mutation_unknown = bool(
        mutation_relevant
        and isinstance(response, Mapping)
        and response.get("mutation_state_unknown") is True
    )
    if explicit_mutation_unknown:
        fields.update(
            {
                "mutation_state_unknown": True,
                "command_may_still_be_in_flight": bool(
                    response.get("command_may_still_be_in_flight", False)
                ),
                "do_not_retry": True,
            }
        )
    if isinstance(response, Mapping) and response.get("workflow_recovery_required") is True:
        fields["workflow_recovery_required"] = True
        fields["do_not_retry"] = True
        for recovery_key in ("recovery_steps", "recovery_guidance", "restore_errors"):
            if recovery_key in response:
                fields[recovery_key] = response[recovery_key]

    marker = find_unknown_transport_marker(response)
    if marker is None:
        return fields
    if mutation_relevant:
        fields.update(
            {
                "mutation_state_unknown": True,
                "command_may_still_be_in_flight": bool(
                    marker.get("command_may_still_be_in_flight", False)
                ),
                "do_not_retry": True,
            }
        )
    else:
        fields.update(
            {
                "read_command_outcome_unknown": True,
                "read_command_may_still_be_in_flight": bool(
                    marker.get("command_may_still_be_in_flight", False)
                ),
            }
        )
    return fields


def _known_transport_envelope_failed(response: Mapping[str, Any]) -> bool:
    """Find explicit failures below recognized bridge wrapper fields."""

    pending: list[Mapping[str, Any]] = []
    for key in ("unreal_response", "native_response", "response"):
        nested = response.get(key)
        if isinstance(nested, Mapping):
            pending.append(nested)

    seen: set[int] = set()
    cursor = 0
    while cursor < len(pending) and len(seen) < _MAX_TRANSPORT_ENVELOPE_MAPPINGS:
        candidate = pending[cursor]
        cursor += 1
        candidate_id = id(candidate)
        if candidate_id in seen:
            continue
        seen.add(candidate_id)
        if candidate.get("success") is False:
            return True
        status = candidate.get("status")
        if isinstance(status, str) and status.strip().lower() in _FAILURE_STATUSES:
            return True
        for key in ("result", "unreal_response", "native_response", "response"):
            nested = candidate.get(key)
            if isinstance(nested, Mapping):
                pending.append(nested)
    return False


def _legacy_success(response: Mapping[str, Any]) -> bool:
    if response.get("success") is False:
        return False
    status = response.get("status")
    if isinstance(status, str) and status.strip().lower() in _FAILURE_STATUSES:
        return False

    # The Unreal transport wraps the native command result. Respect an
    # explicit nested success=false, but do not reinterpret domain statuses
    # such as a completed audit whose result is "fail" as a transport error.
    native_result = response.get("result")
    if isinstance(native_result, Mapping) and native_result.get("success") is False:
        return False
    if _known_transport_envelope_failed(response):
        return False
    return True


def _first_value(sources: list[Mapping[str, Any]], key: str) -> Any:
    for source in sources:
        if key in source:
            return source[key]
    return None


def _first_bool(
    sources: list[Mapping[str, Any]],
    keys: tuple[str, ...],
) -> bool | None:
    for key in keys:
        value = _first_value(sources, key)
        if isinstance(value, bool):
            return value
    return None


def _bool_values(
    sources: list[Mapping[str, Any]],
    keys: tuple[str, ...],
) -> list[bool]:
    """Collect every explicit boolean from known response envelopes."""

    values: list[bool] = []
    for source in sources:
        for key in keys:
            value = source.get(key)
            if isinstance(value, bool):
                values.append(value)
    return values


def _mutation_bool_values(
    sources: list[Mapping[str, Any]],
    direct_keys: tuple[str, ...],
    nested_key: str,
) -> list[bool]:
    values = _bool_values(sources, direct_keys)
    for source in sources:
        mutation = source.get("mutation")
        if not isinstance(mutation, Mapping):
            continue
        value = mutation.get(nested_key)
        if isinstance(value, bool):
            values.append(value)
    return values


def _true_dominant_bool(values: Sequence[bool]) -> bool | None:
    if not values:
        return None
    return any(values)


def _has_bool_conflict(values: Sequence[bool]) -> bool:
    return any(values) and not all(values)


def _safety_mirror_value(
    sources: list[Mapping[str, Any]],
    key: str,
) -> Any:
    if key in _TRUE_DOMINANT_SAFETY_FIELDS:
        return _true_dominant_bool(_bool_values(sources, (key,)))
    return _first_value(sources, key)


def _nonnegative_int(value: Any) -> int | None:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        return None
    return value


def _count_details(sources: list[Mapping[str, Any]]) -> dict[str, int]:
    details: dict[str, int] = {}
    for source in reversed(sources):
        for key, value in source.items():
            count = _nonnegative_int(value)
            if count is None:
                continue
            if key in _CANONICAL_COUNT_FIELDS or key.endswith("_count") or key == "instances_removed":
                details[key] = count
    return dict(sorted(details.items()))


def _canonical_count(
    canonical_key: str,
    details: Mapping[str, int],
    sources: list[Mapping[str, Any]],
) -> int | None:
    direct = _nonnegative_int(_first_value(sources, canonical_key))
    if direct is not None:
        return direct

    aliases = _COUNT_EXACT_ALIASES.get(canonical_key, ())
    for alias in aliases:
        value = _nonnegative_int(_first_value(sources, alias))
        if value is not None:
            return value

    prefixes = _COUNT_PREFIXES.get(canonical_key, ())
    candidates = [
        value
        for key, value in details.items()
        if key != canonical_key
        and key.endswith("_count")
        and any(key.startswith(prefix) for prefix in prefixes)
    ]
    # Summing several domain counts can double-count the same mutation.  Only
    # promote an unambiguous single domain counter to the canonical aggregate.
    return candidates[0] if len(candidates) == 1 else None


def _normalized_delta(sources: list[Mapping[str, Any]]) -> dict[str, Any]:
    existing = _first_value(sources, "delta")
    details = _count_details(sources)
    delta: dict[str, Any] = dict(existing) if isinstance(existing, Mapping) else {}

    for canonical_key in _CANONICAL_COUNT_FIELDS:
        existing_value = _nonnegative_int(delta.get(canonical_key))
        delta[canonical_key] = (
            existing_value
            if existing_value is not None
            else _canonical_count(canonical_key, details, sources)
        )

    if delta["affected_count"] is None:
        component_counts = [
            delta[key]
            for key in ("created_count", "updated_count", "removed_count")
        ]
        if any(value is not None for value in component_counts):
            delta["affected_count"] = sum(
                value for value in component_counts if value is not None
            )

    if details:
        existing_details = delta.get("details")
        merged_details = dict(existing_details) if isinstance(existing_details, Mapping) else {}
        for key, value in details.items():
            merged_details.setdefault(key, value)
        delta["details"] = dict(sorted(merged_details.items()))
    return delta


def _requested_dry_run(policy: Any, arguments: Mapping[str, Any]) -> bool:
    if not bool(getattr(policy, "supports_dry_run", False)):
        return False
    value = arguments.get("dry_run", getattr(policy, "dry_run_default", None))
    return value is True


def _explicit_verification(sources: list[Mapping[str, Any]]) -> dict[str, Any]:
    for source in sources:
        verification = source.get("verification")
        if isinstance(verification, Mapping):
            return dict(verification)
    return {}


def _reported_dry_runs(sources: list[Mapping[str, Any]]) -> list[bool]:
    values: list[bool] = []
    for source in sources:
        value = source.get("dry_run")
        if isinstance(value, bool):
            values.append(value)
    return values


def _batch_mutation_summary(
    response: Mapping[str, Any],
    policy: Any,
) -> dict[str, Any] | None:
    """Aggregate only the native batch ``results[].response`` contract.

    Arbitrary response arrays remain opaque. Failed or contradictory child
    mutation state fails closed. Successful children without mutation evidence
    remain unclassified and retain legacy behavior while the whole batch succeeds.
    """

    payload = response.get("result")
    if not isinstance(payload, Mapping) or not isinstance(payload.get("results"), list):
        return None

    rows = payload["results"]
    attempts: list[bool] = []
    performed_states: list[bool | None] = []
    completed_stages: list[str] = []
    batch_unknown = partial_mutation = do_not_retry = in_flight = False
    unclassified_success = False
    known_partial_performed = False
    failed_stage_mutation_performed = False
    workflow_recovery_required = False

    for fallback_index, row in enumerate(rows):
        child = row.get("response") if isinstance(row, Mapping) else None
        if not isinstance(child, Mapping):
            attempts.append(True)
            performed_states.append(None)
            batch_unknown = True
            continue

        child_sources = _response_sources(child)
        has_mutation_fields = bool(
            _mutation_bool_values(
                child_sources, _MUTATION_ATTEMPTED_FIELDS, "attempted"
            )
            or _mutation_bool_values(
                child_sources, _MUTATION_PERFORMED_FIELDS, "performed"
            )
            or _bool_values(child_sources, _CHANGED_FIELDS)
        )
        has_safety_fields = bool(
            find_unknown_transport_marker(child)
            or any(
                _bool_values(
                    child_sources, tuple(_TRUE_DOMINANT_SAFETY_FIELDS)
                )
            )
        )
        child_success = _legacy_success(child)
        if (
            row.get("status") == "success"
            and child_success
            and not has_mutation_fields
            and not has_safety_fields
        ):
            unclassified_success = True
            continue

        normalized = apply_mutation_result_contract(
            dict(child),
            tool_name="_execute_batch_child",
            policy=policy,
        )
        contract = normalized.get("mutation_result")
        mutation = contract.get("mutation") if isinstance(contract, Mapping) else None
        if not isinstance(contract, Mapping) or not isinstance(mutation, Mapping):
            attempts.append(True)
            performed_states.append(None)
            batch_unknown = True
            continue

        attempted = mutation.get("attempted")
        performed = mutation.get("performed")
        child_known_partial = mutation.get("known_partial_performed") is True
        child_failed_stage_performed = bool(
            mutation.get("failed_stage_mutation_performed") is True
            or contract.get("failed_stage_mutation_performed") is True
        )
        reported_performed = _true_dominant_bool(
            _mutation_bool_values(
                child_sources, _MUTATION_PERFORMED_FIELDS, "performed"
            )
        )
        child_contradiction = bool(
            attempted is False
            and (
                performed is True
                or reported_performed is True
                or child_known_partial
                or child_failed_stage_performed
            )
        )
        child_known_performed = bool(
            not child_contradiction
            and (
                performed is True
                or child_known_partial
                or child_failed_stage_performed
            )
        )
        attempts.append(attempted if isinstance(attempted, bool) else True)
        performed_states.append(
            True
            if child_known_performed
            else performed
            if isinstance(performed, bool) and not child_contradiction
            else None
        )
        batch_unknown |= bool(
            contract.get("mutation_state_unknown") is True
            or (performed is None and not child_known_performed)
            or child_contradiction
        )
        if child_known_performed:
            known_partial_performed |= child_known_partial
            failed_stage_mutation_performed |= child_failed_stage_performed
        if performed is True and not child_contradiction:
            stage_index = _nonnegative_int(row.get("index"))
            command_type = row.get("type")
            stage_name = (
                f"batch[{fallback_index if stage_index is None else stage_index}]:"
                f"{command_type if isinstance(command_type, str) and command_type else 'unknown'}"
            )
            if row.get("status") == "success" and child_success:
                completed_stages.append(stage_name)
            elif not child_failed_stage_performed:
                failed_stage_mutation_performed = True
        partial_mutation |= bool(
            contract.get("partial_mutation") is True or child_contradiction
        )
        do_not_retry |= contract.get("do_not_retry") is True
        in_flight |= contract.get("command_may_still_be_in_flight") is True
        workflow_recovery_required |= (
            contract.get("workflow_recovery_required") is True
        )

    if not attempts and not batch_unknown:
        return None
    if unclassified_success:
        batch_failed = bool(
            not _legacy_success(response)
            or _nonnegative_int(payload.get("failed"))
        )
        has_child_safety_failure = bool(
            partial_mutation
            or do_not_retry
            or in_flight
            or failed_stage_mutation_performed
            or workflow_recovery_required
        )
        if not batch_failed and not batch_unknown and not has_child_safety_failure:
            return None
        batch_unknown = True
        partial_mutation = True
    known_performed = any(value is True for value in performed_states)
    partial_mutation |= known_performed and bool(
        _nonnegative_int(payload.get("failed")) or batch_unknown
    )
    summary: dict[str, Any] = {
        "mutation_attempted": bool(
            rows
            and (
                batch_unknown
                or any(attempts)
            )
        )
    }
    if known_performed:
        summary.update(mutation_performed=True, changed=True)
    elif not batch_unknown:
        summary.update(mutation_performed=False, changed=False)
    if batch_unknown:
        summary["mutation_state_unknown"] = True
    if partial_mutation:
        summary["partial_mutation"] = True
    if batch_unknown or partial_mutation or do_not_retry:
        summary["do_not_retry"] = True
    if in_flight:
        summary["command_may_still_be_in_flight"] = True
    if completed_stages:
        summary["completed_mutation_stages"] = completed_stages
        summary["completed_mutation_count"] = len(completed_stages)
    if failed_stage_mutation_performed:
        summary["failed_stage_mutation_performed"] = True
    if known_partial_performed:
        summary["known_partial_performed"] = True
    if workflow_recovery_required:
        summary["workflow_recovery_required"] = True
    return summary


def apply_mutation_result_contract(
    result: Any,
    *,
    tool_name: str,
    policy: Any,
    arguments: Mapping[str, Any] | None = None,
) -> Any:
    """Return an additive canonical contract for one mutation-capable result.

    Read-only tools and non-dictionary payloads remain byte-for-byte equivalent
    at the Python object level.  Strict tools fail closed only after a legacy
    success response lacks or contradicts their declared independent readback.
    """

    if getattr(policy, "effect", "read") == "read" or not isinstance(result, dict):
        return result

    response = dict(result)
    sources = _response_sources(response)
    batch_summary_result = (
        _batch_mutation_summary(response, policy)
        if tool_name.strip().lower() == "execute_batch"
        else None
    )
    if batch_summary_result is not None:
        sources.append(batch_summary_result)
    call_arguments = dict(arguments or {})
    dry_run = _requested_dry_run(policy, call_arguments)
    mode = "dry_run" if dry_run else "apply"
    nested_transport_failure_reported = _known_transport_envelope_failed(response)
    legacy_success = _legacy_success(response)
    strict_postcondition = bool(getattr(policy, "strict_postcondition", False))
    transport_marker = find_unknown_transport_marker(response)
    transport_outcome_unknown = transport_marker is not None
    reported_command_may_still_be_in_flight = _true_dominant_bool(
        _bool_values(sources, ("command_may_still_be_in_flight",))
    )
    command_may_still_be_in_flight = bool(
        reported_command_may_still_be_in_flight is True
        or (
            transport_outcome_unknown
            and transport_marker.get("command_may_still_be_in_flight") is True
        )
    )
    mutation_attempted_values = _mutation_bool_values(
        sources,
        _MUTATION_ATTEMPTED_FIELDS,
        "attempted",
    )
    mutation_performed_values = _mutation_bool_values(
        sources,
        _MUTATION_PERFORMED_FIELDS,
        "performed",
    )
    changed_values = _bool_values(sources, _CHANGED_FIELDS)
    reported_mutation_performed = _true_dominant_bool(mutation_performed_values)
    reported_changed = _true_dominant_bool(changed_values)
    canonical_changed_values = _bool_values(sources, ("changed",))
    reported_canonical_changed = _true_dominant_bool(canonical_changed_values)
    component_changed_values = _bool_values(sources, _CHANGED_FIELDS[1:])
    changed_key_conflict = any(
        _has_bool_conflict(_bool_values(sources, (key,)))
        for key in _CHANGED_FIELDS
    )
    mutation_boolean_conflict = bool(
        _has_bool_conflict(mutation_performed_values)
        or changed_key_conflict
        or (
            False in canonical_changed_values
            and _true_dominant_bool(component_changed_values) is True
        )
        or (
            reported_mutation_performed is not None
            and reported_canonical_changed is not None
            and reported_mutation_performed != reported_canonical_changed
        )
    )
    partial_mutation = bool(
        _true_dominant_bool(
            _bool_values(sources, ("partial_mutation", "partial_target_modified"))
        )
    )
    workflow_recovery_required = bool(
        _true_dominant_bool(
            _bool_values(sources, ("workflow_recovery_required",))
        )
    )
    do_not_retry_declared = bool(
        _true_dominant_bool(_bool_values(sources, ("do_not_retry",)))
    )
    safety_failure_declared = bool(
        partial_mutation or workflow_recovery_required or do_not_retry_declared
    )
    if partial_mutation and reported_mutation_performed is False:
        mutation_boolean_conflict = True
    completed_mutation_stages = _first_value(sources, "completed_mutation_stages")
    if not isinstance(completed_mutation_stages, Sequence) or isinstance(
        completed_mutation_stages,
        (str, bytes, bytearray),
    ):
        completed_mutation_stages = []
    else:
        completed_mutation_stages = [
            str(stage) for stage in completed_mutation_stages if str(stage)
        ]
    known_partial_performed = bool(
        partial_mutation
        and (
            _true_dominant_bool(
                _bool_values(sources, ("known_partial_performed",))
            )
            is True
            or (
                _true_dominant_bool(mutation_performed_values) is True
                and (
                    completed_mutation_stages
                    or _true_dominant_bool(
                        _bool_values(
                            sources, ("failed_stage_mutation_performed",)
                        )
                    )
                    is True
                )
            )
        )
    )
    explicit_mutation_state_unknown = (
        _true_dominant_bool(_bool_values(sources, ("mutation_state_unknown",)))
        is True
        or reported_command_may_still_be_in_flight is True
    )
    mutation_outcome_unknown = (
        transport_outcome_unknown
        or explicit_mutation_state_unknown
        or mutation_boolean_conflict
    )

    warnings: list[str] = []
    if mutation_boolean_conflict:
        warnings.append("conflicting_mutation_state_envelopes")
    if safety_failure_declared and legacy_success:
        warnings.append("success_conflicts_with_mutation_safety_fields")
    reported_dry_runs = _reported_dry_runs(sources)
    if any(value != dry_run for value in reported_dry_runs):
        warnings.append("response_dry_run_does_not_match_request")

    mutation_attempted = _true_dominant_bool(mutation_attempted_values)
    if mutation_attempted is None:
        mutation_attempted = not dry_run
    if transport_outcome_unknown:
        # Either a client socket write began or the native bridge accepted and
        # submitted the command.  In both cases the effect cannot be inferred
        # from the timeout response alone.
        mutation_attempted = bool(
            mutation_attempted is True
            or transport_marker.get("request_write_started") is True
            or transport_marker.get("request_received_by_server") is True
            or transport_marker.get("command_dispatch_submitted") is True
        )
    if dry_run and mutation_attempted:
        warnings.append("dry_run_reported_mutation_attempted")

    mutation_performed = reported_mutation_performed
    changed = reported_changed
    if changed is None and mutation_performed is not None:
        changed = mutation_performed
    if transport_outcome_unknown:
        mutation_performed = None
        changed = None
    elif explicit_mutation_state_unknown or mutation_boolean_conflict:
        mutation_performed = None
        changed = None
    if dry_run and mutation_performed is True:
        warnings.append("dry_run_reported_mutation_performed")

    before = _first_value(sources, "before")
    after = _first_value(sources, "after")
    after_expected = _first_value(sources, "after_expected")
    if after_expected is None:
        after_expected = _first_value(sources, "planned_after")

    verification = _explicit_verification(sources)
    raw_status = verification.get("status")
    status = raw_status.strip().lower() if isinstance(raw_status, str) else ""
    if status not in _VERIFICATION_STATUSES:
        status = "not_completed" if not legacy_success else "unverified"
    if transport_outcome_unknown:
        failure_kind = str(transport_marker.get("failure_kind", "")).strip().lower()
        status = (
            "timeout"
            if failure_kind in {"timeout", "server_timeout"}
            else "unverified"
        )
        warnings.append("transport_command_outcome_unknown")
    if warnings and not transport_outcome_unknown:
        status = "failed"

    before_observed = before is not None
    after_observed = after is not None
    if strict_postcondition and legacy_success and not mutation_outcome_unknown:
        if not before_observed:
            warnings.append("strict_contract_missing_before_observation")
        if not after_observed:
            warnings.append("strict_contract_missing_after_observation")
        method = verification.get("method")
        if not isinstance(method, str) or not method.strip():
            warnings.append("strict_contract_missing_verification_method")
        if status != "passed":
            warnings.append("strict_contract_postcondition_not_passed")
        if warnings:
            status = "failed"

    verification["status"] = status
    verification.setdefault("method", "not_reported")
    verification["before_observed"] = before_observed
    verification["after_observed"] = after_observed
    verification["strict"] = strict_postcondition
    if transport_outcome_unknown:
        failure_phase = str(transport_marker.get("failure_phase", "")).strip().lower()
        if failure_phase == "server_execution" and failure_kind in {
            "timeout",
            "server_timeout",
        }:
            verification["method"] = "server_execution_timeout_outcome_unknown"
        elif failure_phase == "server_execution":
            verification["method"] = "server_execution_outcome_unknown"
        else:
            verification["method"] = (
                "transport_response_contract_invalid"
                if transport_marker.get("response_complete") is True
                else "transport_response_not_received"
            )
        verification["command_may_still_be_in_flight"] = (
            command_may_still_be_in_flight
        )
        verification["do_not_retry"] = True
    if command_may_still_be_in_flight:
        verification["command_may_still_be_in_flight"] = True
        verification["do_not_retry"] = True
    if warnings:
        existing_warnings = verification.get("warnings")
        merged_warnings = list(existing_warnings) if isinstance(existing_warnings, list) else []
        for warning in warnings:
            if warning not in merged_warnings:
                merged_warnings.append(warning)
        verification["warnings"] = merged_warnings

    verified_success = bool(
        not mutation_outcome_unknown and legacy_success and status == "passed"
    )
    mutation = {
        "attempted": mutation_attempted,
        "performed": mutation_performed,
        "state": (
            "performed"
            if mutation_performed is True
            else "not_performed"
            if mutation_performed is False
            else "unknown"
        ),
        "transactional": bool(getattr(policy, "transactional", False)),
    }
    if known_partial_performed:
        mutation["known_partial_performed"] = True
        mutation["known_completed_stages"] = list(completed_mutation_stages)
        mutation["failed_stage_mutation_performed"] = bool(
            _true_dominant_bool(
                _bool_values(sources, ("failed_stage_mutation_performed",))
            )
            is True
        )
        if mutation_outcome_unknown:
            mutation["current_stage_outcome_unknown"] = True
    delta = _normalized_delta(sources)
    if mutation_outcome_unknown:
        success_semantics = "unknown_command_outcome"
    elif dry_run:
        success_semantics = (
            "verified_dry_run"
            if verified_success
            else "failed_dry_run"
            if not legacy_success or status == "failed"
            else "unverified_dry_run"
        )
    elif verified_success:
        success_semantics = "verified_postcondition"
    elif status == "failed":
        success_semantics = "postcondition_failed"
    elif not legacy_success:
        success_semantics = "failed_execution"
    else:
        success_semantics = "legacy_execution"

    contract: dict[str, Any] = {
        "contract_version": MUTATION_RESULT_CONTRACT_VERSION,
        "tool": tool_name,
        "mode": mode,
        "dry_run": dry_run,
        "changed": changed,
        "mutation": mutation,
        "delta": delta,
        "verification": verification,
        "verified_success": verified_success,
        "success_semantics": success_semantics,
    }
    if mutation_outcome_unknown:
        contract["mutation_state_unknown"] = True
        contract["do_not_retry"] = True
    if transport_outcome_unknown:
        contract["command_may_still_be_in_flight"] = (
            command_may_still_be_in_flight
        )
    for safety_field in _SAFETY_MIRROR_FIELDS:
        safety_value = _safety_mirror_value(sources, safety_field)
        if safety_value is not None:
            contract[safety_field] = safety_value
    if mutation_outcome_unknown:
        contract["mutation_state_unknown"] = True
        contract["do_not_retry"] = True
    if command_may_still_be_in_flight:
        contract["command_may_still_be_in_flight"] = True
    if partial_mutation:
        contract["partial_mutation"] = True
    if before_observed:
        contract["before"] = before
    if after_observed:
        contract["after"] = after
    if after_expected is not None:
        contract["after_expected"] = after_expected

    # Top-level mirrors make the safety state visible without forcing clients
    # to inspect the raw native bridge envelope.  Existing domain-specific
    # fields are retained unless the dry-run value would contradict the call.
    response["contract_version"] = MUTATION_RESULT_CONTRACT_VERSION
    response.setdefault("mode", mode)
    if "dry_run" in response and response.get("dry_run") != dry_run:
        response.setdefault("reported_dry_run", response.get("dry_run"))
    response["dry_run"] = dry_run
    response["DRY_RUN"] = dry_run
    response.setdefault("changed", changed)
    response.setdefault("mutation", mutation)
    response.setdefault("delta", delta)
    response.setdefault("verification", verification)
    if before_observed:
        response.setdefault("before", before)
    if after_observed:
        response.setdefault("after", after)
    if after_expected is not None:
        response.setdefault("after_expected", after_expected)
    for count_field in _CANONICAL_COUNT_FIELDS:
        response.setdefault(count_field, delta[count_field])
    response["verified_success"] = verified_success
    response["success_semantics"] = success_semantics
    response["mutation_result"] = contract
    response.setdefault("success", legacy_success)

    for safety_field in _SAFETY_MIRROR_FIELDS:
        safety_value = _safety_mirror_value(sources, safety_field)
        if safety_field in _TRUE_DOMINANT_SAFETY_FIELDS and safety_value is True:
            response[safety_field] = True
        elif safety_value is not None:
            response.setdefault(safety_field, safety_value)
    if partial_mutation:
        response["partial_mutation"] = True

    if explicit_mutation_state_unknown or mutation_boolean_conflict:
        response["mutation_state_unknown"] = True
        response["do_not_retry"] = True
        response["changed"] = None
        response["mutation"] = mutation
        response["verification"] = verification
        response["success"] = False
        response["status"] = "error"
        response.setdefault("error_code", "mutation_state_unknown")

    if transport_outcome_unknown:
        response["mutation_state_unknown"] = True
        response["command_may_still_be_in_flight"] = (
            command_may_still_be_in_flight
        )
        response["do_not_retry"] = True
        response["changed"] = None
        response["mutation"] = mutation
        response["verification"] = verification
        response["success"] = False
        response["status"] = "error"
        response.setdefault("error_code", "transport_command_outcome_unknown")

    if not legacy_success:
        if response.get("success") is True:
            response.setdefault("reported_success", True)
        response["success"] = False
        response["status"] = "error"

    if nested_transport_failure_reported:
        response.setdefault("error_code", "nested_mutation_execution_failed")

    if safety_failure_declared and not mutation_outcome_unknown:
        response["success"] = False
        response["status"] = "error"
        response.setdefault("error_code", "mutation_safety_failure_declared")
        message = (
            f"{tool_name} reported mutation safety or recovery fields that are "
            "incompatible with success"
        )
        response.setdefault("message", message)
        response.setdefault("error", message)

    native_legacy_success = _first_bool(sources, ("legacy_success",))
    strict_verification_failure = bool(
        strict_postcondition
        and status == "failed"
        and (legacy_success or native_legacy_success is True)
    )
    if strict_verification_failure:
        response["legacy_success"] = legacy_success or native_legacy_success is True
        response["success"] = False
        response["status"] = "error"
        response.setdefault("error_code", "mutation_postcondition_failed")
        message = (
            f"{tool_name} returned without a passing independent postcondition; "
            "treat mutation state as unverified"
        )
        response.setdefault("message", message)
        response.setdefault("error", message)

    return response
