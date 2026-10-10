"""
Niagara authoring and inspection tools for Unreal MCP.

These tools route to UnrealMCP C++ commands for Niagara APIs that are not
safe or practical through Unreal Python alone, such as renderer material edits
and stack/module input inspection.
"""

import json
import logging
import math
import subprocess
import time
import hashlib
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Dict, Literal

from mcp.server.fastmcp import Context, FastMCP

from tools.mutation_result_contract import composite_failure_safety_fields
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")

_GENERATED_NIAGARA_ROOT = "/Game/_MCP_Temp/NiagaraGenerated"
_NIAGARA_PACKAGE_SIDECARS = (
    ".uasset",
    ".uexp",
    ".ubulk",
    ".uptnl",
    ".m.ubulk",
    ".upayload",
)
_NIAGARA_GRAPH_RECIPE_COMPILE_PHASE_COUNT = 4
_NIAGARA_GRAPH_RECIPE_TRANSPORT_MARGIN_SECONDS = 30.0
_FORBIDDEN_MCP_REFERENCE_MARKERS = (
    "/Script/UnrealMCP",
    "/UnrealMCP",
    "UnrealMCP",
    "MCPUnreal",
    "mcp_unreal",
)


class NiagaraBridgeResponseError(RuntimeError):
    """Carry a rejected bridge response through composite Niagara recipes."""

    def __init__(
        self,
        command_name: str,
        response: Dict[str, Any],
        *,
        command_effect: Literal["read", "mutation", "unknown"],
    ) -> None:
        self.command_name = command_name
        self.response = response
        self.command_effect = command_effect
        super().__init__(f"Rejected Unreal bridge response for {command_name}")


def _niagara_bridge_failure_fields(exc: BaseException) -> Dict[str, Any]:
    if not isinstance(exc, NiagaraBridgeResponseError):
        return {}
    response_field = (
        "read_unreal_response"
        if exc.command_effect == "read"
        else "unreal_response"
    )
    return {
        response_field: exc.response,
        "failed_bridge_command": exc.command_name,
        "failed_bridge_command_effect": exc.command_effect,
    }


def _niagara_partial_mutation_fields(
    completed_mutation_commands: list[str],
) -> Dict[str, Any]:
    if not completed_mutation_commands:
        return {}
    return {
        "partial_mutation": True,
        "mutation_performed": True,
        "completed_mutation_count": len(completed_mutation_commands),
        "completed_mutation_commands": list(completed_mutation_commands),
        "do_not_retry": True,
        "recovery_policy": "inspect_current_niagara_graph_and_stack_before_retry",
        "recovery_steps": [
            "inspect_current_niagara_graph_and_stack",
            "reconcile_completed_recipe_stages",
            "retry_only_the_verified_missing_stage",
        ],
        "verification": {
            "status": "unverified",
            "method": "composite_recipe_postcondition_not_completed",
        },
    }


def _niagara_error(
    message: str,
    *,
    error_code: str = "niagara_tool_failed",
    cause: BaseException | None = None,
    **fields: Any,
) -> Dict[str, Any]:
    if cause is not None:
        for key, value in _niagara_bridge_failure_fields(cause).items():
            fields.setdefault(key, value)
    return tool_error(message, error_code=error_code, **fields)


def _normalize_niagara_graph_recipe_phase_timeout(timeout_seconds: float) -> float:
    """Mirror the bridge's finite 1..60 second per-phase timeout contract."""

    if isinstance(timeout_seconds, bool):
        raise TypeError("timeout_seconds must be a finite number, not bool")
    value = float(timeout_seconds)
    if not math.isfinite(value):
        raise ValueError("timeout_seconds must be finite")
    return min(60.0, max(1.0, value))


def _niagara_graph_recipe_transport_timeout(timeout_seconds: float) -> float:
    """Cover baseline, pre-removal, final, and rollback compile waits plus I/O."""

    per_phase_timeout = _normalize_niagara_graph_recipe_phase_timeout(timeout_seconds)
    return max(
        10.0,
        per_phase_timeout * _NIAGARA_GRAPH_RECIPE_COMPILE_PHASE_COUNT
        + _NIAGARA_GRAPH_RECIPE_TRANSPORT_MARGIN_SECONDS,
    )


def _is_exact_niagara_graph_recipe_save_failure(
    result: dict[str, Any],
    *,
    expected_system_path: str,
    expected_dry_run: bool,
    expected_save: bool,
) -> bool:
    """Validate the fail-closed recovery contract for an attempted ghost save."""

    if expected_dry_run or not expected_save:
        return False

    exact_fields: dict[str, Any] = {
        "success": False,
        "mutation_performed": True,
        "mutation_applied_in_memory": True,
        "saved": False,
        "rollback_attempted": True,
        "save_state_unknown": True,
        "save_attempted": True,
        "cleanup_required_unknown": True,
        "offline_exact_cleanup_required": True,
        "recreate_required": True,
        "exact_cleanup_only": True,
        "rollback_does_not_resolve_save_state": True,
        "persisted_state_recovery_proven": False,
        "requires_package_reload": True,
        "undo_recovery_unproven": True,
        "caller_script_undo_recovery_supported": False,
        "do_not_retry": True,
        "requires_reload_or_undo": True,
        "recovery_policy": "reload_required",
        "rollback_policy": "reload_required",
        "rollback_scope": "in_memory_only",
        "save_recovery_policy": (
            "reload_then_offline_exact_cleanup_and_recreate_required"
        ),
    }
    if any(
        type(result.get(field)) is not type(expected)
        or result.get(field) != expected
        for field, expected in exact_fields.items()
    ):
        return False
    if result.get("error_code") != "niagara_graph_recipe_save_failed":
        return False
    if type(result.get("rollback_verified")) is not bool:
        return False
    if not isinstance(result.get("rollback"), dict):
        return False

    try:
        requested_package = _normalize_game_package_path(expected_system_path)
        target_package = _normalize_game_package_path(
            str(result.get("target_package_path") or "")
        )
        recovery_package = _normalize_game_package_path(
            str(result.get("recovery_target_package") or "")
        )
        target_system_package = _normalize_game_package_path(
            str(result.get("target_system_path") or "")
        )
    except ValueError:
        return False
    if not requested_package.startswith(_GENERATED_NIAGARA_ROOT + "/"):
        return False
    if not (
        requested_package
        == target_package
        == recovery_package
        == target_system_package
    ):
        return False

    expected_object_path = (
        requested_package + "." + requested_package.rsplit("/", 1)[-1]
    )
    if str(result.get("target_system_path") or "") != expected_object_path:
        return False
    for field in ("reload_required_packages", "cleanup_candidate_paths"):
        values = result.get(field)
        if (
            not isinstance(values, list)
            or len(values) != 1
            or type(values[0]) is not str
        ):
            return False
        try:
            if _normalize_game_package_path(values[0]) != requested_package:
                return False
        except ValueError:
            return False

    filename = str(result.get("target_package_filename") or "").replace("\\", "/")
    expected_suffix = (
        "/Content/" + requested_package.removeprefix("/Game/") + ".uasset"
    )
    if not filename.casefold().endswith(expected_suffix.casefold()):
        return False
    recovery_action = result.get("recovery_action")
    if (
        type(recovery_action) is not str
        or requested_package not in recovery_action
        or "Do not retry" not in recovery_action
    ):
        return False
    return True


def _is_niagara_graph_recipe_transport_failure(
    response: Any,
    *,
    expected_dry_run: bool,
    expected_save: bool,
    expected_system_path: str = "",
) -> bool:
    """Identify failures where the bridge did not return a structured recipe result."""

    if not isinstance(response, dict):
        return True

    error_code = str(response.get("error_code") or "")
    if error_code in {"unreal_connection_failed", "unreal_no_response"}:
        return True

    # Recipe validation and rollback failures are returned by C++ under result.
    # A socket exception, bridge timeout, missing response, or malformed envelope
    # cannot prove mutation state. Require both envelope layers to agree exactly.
    status = response.get("status")
    result = response.get("result")
    if type(status) is not str or status not in ("success", "error") or not isinstance(result, dict):
        return True
    result_success = result.get("success")
    if type(result_success) is not bool:
        return True
    if (status == "success") != result_success:
        return True
    for field in ("mutation_performed", "mutation_applied_in_memory", "saved"):
        if type(result.get(field)) is not bool:
            return True
    mutation_performed = result["mutation_performed"]
    mutation_applied = result["mutation_applied_in_memory"]
    saved = result["saved"]
    if mutation_applied and not mutation_performed:
        return True
    if saved and not mutation_performed:
        return True
    if status == "error" and saved:
        return True
    save_recovery_fields = {
        "save_state_unknown",
        "save_attempted",
        "cleanup_required_unknown",
        "offline_exact_cleanup_required",
        "recreate_required",
        "exact_cleanup_only",
        "rollback_does_not_resolve_save_state",
        "persisted_state_recovery_proven",
        "rollback_scope",
        "save_recovery_policy",
        "target_package_path",
        "target_package_filename",
        "recovery_target_package",
        "reload_required_packages",
        "cleanup_candidate_paths",
    }
    declares_save_failure = (
        result.get("error_code") == "niagara_graph_recipe_save_failed"
        or any(field in result for field in save_recovery_fields)
    )
    if declares_save_failure and not _is_exact_niagara_graph_recipe_save_failure(
        result,
        expected_system_path=expected_system_path,
        expected_dry_run=expected_dry_run,
        expected_save=expected_save,
    ):
        return True
    if status == "error" and (mutation_performed or mutation_applied):
        reload_contract = {
            "requires_package_reload": True,
            "undo_recovery_unproven": True,
            "caller_script_undo_recovery_supported": False,
            "do_not_retry": True,
            "recovery_policy": "reload_required",
            "rollback_policy": "reload_required",
        }
        if any(
            type(result.get(field)) is not type(expected)
            or result.get(field) != expected
            for field, expected in reload_contract.items()
        ):
            return True
    if status == "success":
        response_dry_run = result.get("dry_run")
        if type(response_dry_run) is not bool or response_dry_run != expected_dry_run:
            return True
        if expected_dry_run:
            if mutation_performed or mutation_applied or saved:
                return True
        else:
            if not mutation_performed or not mutation_applied or saved != expected_save:
                return True
    elif expected_dry_run and (mutation_performed or mutation_applied or saved):
        return True
    return False


def _niagara_graph_recipe_unknown_state_error(
    system_path: str,
    *,
    transport_error: Any,
    transport_timeout_seconds: float,
) -> Dict[str, Any]:
    target_system_path = str(system_path or "")
    recovery_packages: list[str] = []
    try:
        normalized_target = _normalize_niagara_package_path(target_system_path)
        recovery_packages.append(normalized_target)
    except ValueError:
        normalized_target = ""
    recovery_action = (
        f"Do not retry, save, or reload package '{target_system_path}' while the editor "
        "may still be executing the command. Close the editor and confirm the process has "
        "exited, verify the exact on-disk package, then perform exact-package cleanup and "
        "ghost recreation before reopening if persistence cannot be proven."
    )
    if isinstance(transport_error, dict):
        transport_message = str(
            transport_error.get("error")
            or transport_error.get("message")
            or "No structured response from Unreal Engine"
        )
        transport_error_code = str(transport_error.get("error_code") or "")
    else:
        transport_message = str(transport_error or "No response from Unreal Engine")
        transport_error_code = ""

    return _niagara_error(
        "Niagara Scratch Pad graph recipe transport failed; mutation and save state are unknown. "
        + recovery_action,
        error_code="niagara_graph_recipe_transport_state_unknown",
        mutation_state_unknown=True,
        save_state_unknown=True,
        do_not_retry=True,
        requires_package_reload=True,
        command_may_still_be_in_flight=True,
        requires_editor_close_before_disk_verification=True,
        do_not_reload_or_trust_disk_until_editor_closed=True,
        cleanup_required_unknown=True,
        offline_exact_cleanup_required=bool(recovery_packages),
        recreate_required=bool(recovery_packages),
        exact_cleanup_only=bool(recovery_packages),
        target_system_path=target_system_path,
        recovery_target_package=normalized_target,
        reload_required_packages=recovery_packages,
        cleanup_candidate_paths=recovery_packages,
        recovery_policy=(
            "close_editor_then_verify_disk_then_exact_cleanup_and_recreate_if_unproven"
        ),
        post_editor_close_recovery_policy="rehash_then_choose_exact_recovery",
        recovery_steps=[
            "close_editor_and_confirm_process_exit",
            "verify_exact_target_disk_hashes",
            "choose_exact_recovery_from_post_close_hashes",
        ],
        recovery_action=recovery_action,
        transport_error=transport_message,
        transport_error_code=transport_error_code,
        transport_timeout_seconds=transport_timeout_seconds,
    )


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _content_root() -> Path:
    return _repo_root() / "MCPGameProject" / "Content"


def _saved_mcp_root() -> Path:
    return _repo_root() / "MCPGameProject" / "Saved" / "MCP"


def _generated_niagara_content_root() -> Path:
    return _content_root() / "_MCP_Temp" / "NiagaraGenerated"


def _query_unreal_editor_running() -> tuple[bool | None, str]:
    """Return tri-state editor status; process-query failure is never 'closed'."""

    for image_name in ("UnrealEditor.exe", "UnrealEditor-Cmd.exe"):
        try:
            result = subprocess.run(
                ["tasklist", "/FI", f"IMAGENAME eq {image_name}", "/NH"],
                capture_output=True,
                text=True,
                check=False,
                timeout=5,
            )
        except Exception as exc:
            return None, (
                f"tasklist query for {image_name} raised {type(exc).__name__}: {exc}"
            )
        if result.returncode != 0:
            detail = str(result.stderr or result.stdout or "").strip()
            return None, (
                f"tasklist query for {image_name} failed with exit code "
                f"{result.returncode}: {detail}"
            )
        if image_name.casefold() in str(result.stdout or "").casefold():
            return True, ""
    return False, ""


def _package_base_from_sidecar(path: Path) -> Path | None:
    """Return the extension-free package base for one recognized sidecar."""

    path_text = str(path)
    for suffix in sorted(_NIAGARA_PACKAGE_SIDECARS, key=len, reverse=True):
        if path_text.casefold().endswith(suffix.casefold()):
            return Path(path_text[: -len(suffix)])
    return None


def _normalize_niagara_package_path(package_path: str) -> str:
    value = str(package_path or "").strip().strip("\"'")
    if not value:
        raise ValueError("Package path is empty")
    value = value.replace("\\", "/")
    if value.endswith(".uasset"):
        value = value[:-7]
    last_slash = value.rfind("/")
    dot_after_slash = value.find(".", last_slash + 1)
    if dot_after_slash != -1:
        value = value[:dot_after_slash]
    if not value.startswith("/Game/"):
        raise ValueError(f"Package path must start with /Game/: {package_path}")
    if not value.startswith(_GENERATED_NIAGARA_ROOT + "/"):
        raise ValueError(
            f"Package path must be under {_GENERATED_NIAGARA_ROOT}/: {package_path}"
        )
    return value.rstrip("/")


def _normalize_game_package_path(package_path: str) -> str:
    value = str(package_path or "").strip().strip("\"'")
    if not value:
        raise ValueError("Package path is empty")
    value = value.replace("\\", "/")
    if value.endswith(".uasset"):
        value = value[:-7]
    last_slash = value.rfind("/")
    dot_after_slash = value.find(".", last_slash + 1)
    if dot_after_slash != -1:
        value = value[:dot_after_slash]
    value = value.rstrip("/")
    if not value.startswith("/Game/"):
        raise ValueError(f"Package path must start with /Game/: {package_path}")
    return value


def _normalize_game_root_path(root_path: str) -> str:
    value = str(root_path or "").strip().strip("\"'").replace("\\", "/").rstrip("/")
    if not value:
        raise ValueError("target_root is empty")
    if value == "/Game":
        return value
    if not value.startswith("/Game/"):
        raise ValueError(f"target_root must start with /Game/: {root_path}")
    if "." in value.rsplit("/", 1)[-1]:
        raise ValueError(f"target_root must be a folder path, not an asset/object path: {root_path}")
    return value


def _is_temp_game_path(package_path: str) -> bool:
    return package_path == "/Game/_MCP_Temp" or package_path.startswith("/Game/_MCP_Temp/")


def _has_forbidden_mcp_reference_marker(value: str) -> bool:
    folded_value = str(value or "").casefold()
    return any(
        marker.casefold() in folded_value
        for marker in _FORBIDDEN_MCP_REFERENCE_MARKERS
    )


def _normalize_dependency_root(root_path: str) -> str:
    value = str(root_path or "").strip().strip("\"'").replace("\\", "/")
    while len(value) > 1 and value.endswith("/"):
        value = value[:-1]
    return value


def _is_under_dependency_root(package_path: str, root_path: str) -> bool:
    normalized_path = str(package_path or "").strip().replace("\\", "/")
    normalized_root = _normalize_dependency_root(root_path)
    if not normalized_root:
        return True
    path_folded = normalized_path.casefold()
    root_folded = normalized_root.casefold()
    return path_folded == root_folded or path_folded.startswith(root_folded + "/")


def _validate_niagara_promotion_dependency_audit(
    audit: Any,
    *,
    expected_asset_path: str,
    allow_temp_dependencies: bool,
    forbidden_dependency_prefixes: list[str],
    forbidden_source_roots: list[str],
    audit_label: str,
    require_clean: bool,
    require_complete: bool,
) -> tuple[bool, bool]:
    """Bind one dependency audit to the request and recompute every advertised hit."""

    expected_asset = _normalize_game_package_path(expected_asset_path)
    combined_roots = [
        _normalize_dependency_root(value)
        for value in forbidden_dependency_prefixes + forbidden_source_roots
    ]
    combined_roots = [value for value in combined_roots if value]
    if not isinstance(audit, dict):
        raise RuntimeError(f"Malformed {audit_label}: dependency audit must be an object")

    try:
        audit_package = _normalize_game_package_path(
            str(audit.get("package_name") or "")
        )
    except ValueError as exc:
        raise RuntimeError(
            f"Malformed {audit_label}: package/asset binding is invalid; "
            + json.dumps(audit, ensure_ascii=False, default=str)
        ) from exc
    expected_object_path = expected_asset + "." + expected_asset.rsplit("/", 1)[-1]
    audit_object_path = (
        str(audit.get("asset_path") or "")
        .strip()
        .strip("\"'")
        .replace("\\", "/")
    )
    if (
        audit_package != expected_asset
        or audit_object_path != expected_object_path
    ):
        raise RuntimeError(
            f"Malformed {audit_label}: package/asset binding differs from the request; "
            + json.dumps(audit, ensure_ascii=False, default=str)
        )

    dependencies = audit.get("dependencies")
    forbidden_hits = audit.get("forbidden_dependency_hits")
    temp_hits = audit.get("temp_dependency_hits")
    returned_roots = audit.get("additional_forbidden_dependency_prefixes")
    if not all(
        isinstance(value, list)
        for value in (dependencies, forbidden_hits, temp_hits, returned_roots)
    ):
        raise RuntimeError(
            f"Malformed {audit_label}: dependency and policy arrays are required; "
            + json.dumps(audit, ensure_ascii=False, default=str)
        )
    if not all(type(value) is str and bool(value.strip()) for value in dependencies):
        raise RuntimeError(f"Malformed {audit_label}: dependencies must be non-empty strings")
    if not all(type(value) is str and bool(value.strip()) for value in forbidden_hits):
        raise RuntimeError(f"Malformed {audit_label}: forbidden hits must be non-empty strings")
    if not all(type(value) is str and bool(value.strip()) for value in temp_hits):
        raise RuntimeError(f"Malformed {audit_label}: temp hits must be non-empty strings")
    if not all(type(value) is str for value in returned_roots):
        raise RuntimeError(f"Malformed {audit_label}: returned policy roots must be strings")

    normalized_returned_roots = [
        _normalize_dependency_root(value) for value in returned_roots
    ]
    if sorted(value.casefold() for value in normalized_returned_roots) != sorted(
        value.casefold() for value in combined_roots
    ):
        raise RuntimeError(
            f"Malformed {audit_label}: forbidden dependency policy differs from the request; "
            + json.dumps(audit, ensure_ascii=False, default=str)
        )

    expected_forbidden_hits: list[str] = []
    expected_temp_hits: list[str] = []
    for dependency in dependencies:
        if _has_forbidden_mcp_reference_marker(dependency):
            expected_forbidden_hits.append(dependency)
        if any(
            _is_under_dependency_root(dependency, root)
            for root in combined_roots
        ):
            expected_forbidden_hits.append(dependency)
        if not allow_temp_dependencies and _is_temp_game_path(
            _normalize_game_package_path(dependency)
            if str(dependency).startswith("/Game/")
            else str(dependency)
        ):
            expected_temp_hits.append(dependency)

    for field in (
        "dependency_count",
        "forbidden_dependency_hit_count",
        "temp_dependency_hit_count",
    ):
        if type(audit.get(field)) is not int:
            raise RuntimeError(f"Malformed {audit_label}: {field} must be int")
    for field in (
        "success",
        "audit_complete",
        "asset_resolved",
        "package_name_present",
        "dependency_query_attempted",
        "dependency_query_succeeded",
        "saved_file_scanned",
        "has_forbidden_dependency",
        "has_temp_dependency",
    ):
        if type(audit.get(field)) is not bool:
            raise RuntimeError(f"Malformed {audit_label}: {field} must be bool")

    audit_complete = bool(
        audit["success"]
        and audit["audit_complete"]
        and audit["asset_resolved"]
        and audit["package_name_present"]
        and audit["dependency_query_attempted"]
        and audit["dependency_query_succeeded"]
    )
    advertised_complete = bool(audit["audit_complete"])
    audit_consistent = bool(
        advertised_complete == audit_complete
        and audit["success"] == advertised_complete
        and audit["dependency_count"] == len(dependencies)
        and audit["forbidden_dependency_hit_count"] == len(forbidden_hits)
        and audit["temp_dependency_hit_count"] == len(temp_hits)
        and audit["has_forbidden_dependency"] == bool(forbidden_hits)
        and audit["has_temp_dependency"] == bool(temp_hits)
        and forbidden_hits == expected_forbidden_hits
        and temp_hits == expected_temp_hits
    )
    if not audit_consistent:
        raise RuntimeError(
            f"Contradictory {audit_label}: "
            + json.dumps(audit, ensure_ascii=False, default=str)
        )
    if require_complete and not audit_complete:
        raise RuntimeError(
            f"Rejected {audit_label}: dependency audit completeness is required; "
            + json.dumps(audit, ensure_ascii=False, default=str)
        )
    audit_clean = not forbidden_hits and (
        allow_temp_dependencies or not temp_hits
    )
    if require_clean and not audit_clean:
        raise RuntimeError(
            f"Rejected {audit_label}: a clean dependency audit is required; "
            + json.dumps(audit, ensure_ascii=False, default=str)
        )
    return audit_complete, audit_clean


def _short_asset_name(package_path: str) -> str:
    normalized = _normalize_game_package_path(package_path)
    return normalized.rsplit("/", 1)[-1]


def _sanitize_unreal_asset_name(value: str, fallback: str = "NS_NiagaraCandidate") -> str:
    raw = str(value or "").strip()
    sanitized = "".join(
        ch if ch.isascii() and (ch.isalnum() or ch == "_") else "_"
        for ch in raw
    ).strip("_")
    if not sanitized:
        sanitized = fallback
    if sanitized[0].isdigit():
        sanitized = f"NS_{sanitized}"
    return sanitized


def _coerce_string_list(value: Any, field_name: str) -> list[str]:
    if value is None:
        return []
    if isinstance(value, str):
        return [value] if value.strip() else []
    try:
        items = list(value)
    except TypeError as exc:
        raise ValueError(f"{field_name} must be a string or list of strings") from exc

    result = []
    for item in items:
        if isinstance(item, bool):
            raise ValueError(f"{field_name} values must be strings, not bool")
        item_value = str(item or "").strip()
        if item_value:
            result.append(item_value)
    return result


def _promotion_manifest_dir() -> Path:
    return _saved_mcp_root() / "NiagaraPromotionManifests"


def _resolve_promotion_manifest_path(manifest_path: str) -> Path:
    raw = str(manifest_path or "").strip().strip("\"'")
    if not raw:
        raise ValueError("manifest_path is empty")

    path = Path(raw)
    if not path.is_absolute():
        path = _promotion_manifest_dir() / raw
    resolved = path.resolve()
    root = _promotion_manifest_dir().resolve()
    if not resolved.is_relative_to(root):
        raise ValueError(f"manifest_path must resolve under {root}")
    if resolved.suffix.lower() != ".json":
        raise ValueError("manifest_path must point to a .json file")
    return resolved


def _load_promotion_manifest(manifest: Dict[str, Any] | str | None, manifest_path: str) -> Dict[str, Any]:
    if manifest is not None:
        if isinstance(manifest, str):
            return json.loads(manifest)
        if isinstance(manifest, dict):
            return dict(manifest)
        raise ValueError("manifest must be a JSON object or JSON string")
    resolved = _resolve_promotion_manifest_path(manifest_path)
    return json.loads(resolved.read_text(encoding="utf-8"))


def _promotion_manifest_confirmation_payload(manifest: Dict[str, Any]) -> Dict[str, Any]:
    manifest_version = manifest.get("manifest_version")
    if type(manifest_version) is not int or manifest_version != 1:
        raise ValueError("manifest_version must be the integer 1")
    policy_fields: dict[str, bool] = {}
    for field in (
        "all_can_promote",
        "allow_non_temp_source",
        "allow_temp_target",
        "allow_temp_dependencies",
    ):
        value = manifest.get(field)
        if type(value) is not bool:
            raise ValueError(f"{field} must be bool in a promotion manifest")
        policy_fields[field] = value

    entries = manifest.get("entries")
    if not isinstance(entries, list):
        entries = []

    stable_entries = []
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        stable_entries.append(
            {
                "source_system_path": _normalize_game_package_path(str(entry.get("source_system_path") or "")),
                "target_system_path": _normalize_game_package_path(str(entry.get("target_system_path") or "")),
            }
        )

    return {
        "manifest_version": manifest_version,
        "target_root": _normalize_game_root_path(str(manifest.get("target_root") or "/Game")),
        "all_can_promote": policy_fields["all_can_promote"],
        "allow_non_temp_source": policy_fields["allow_non_temp_source"],
        "allow_temp_target": policy_fields["allow_temp_target"],
        "allow_temp_dependencies": policy_fields["allow_temp_dependencies"],
        "forbidden_dependency_prefixes": sorted(
            _coerce_string_list(manifest.get("forbidden_dependency_prefixes"), "forbidden_dependency_prefixes")
        ),
        "forbidden_source_roots": sorted(
            _coerce_string_list(manifest.get("forbidden_source_roots"), "forbidden_source_roots")
        ),
        "entries": stable_entries,
    }


def _promotion_manifest_confirmation_token(manifest: Dict[str, Any]) -> str:
    payload = _promotion_manifest_confirmation_payload(manifest)
    encoded = json.dumps(payload, ensure_ascii=True, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return "niagara-promote-" + hashlib.sha256(encoded).hexdigest()[:16]


def _target_path_for_promotion(
    source_system_path: str,
    target_root: str,
    asset_name_prefix: str,
    asset_name_suffix: str,
) -> str:
    source_name = _short_asset_name(source_system_path)
    asset_name = _sanitize_unreal_asset_name(f"{asset_name_prefix}{source_name}{asset_name_suffix}")
    return f"{target_root}/{asset_name}"


def _package_path_to_asset_file(package_path: str) -> Path:
    normalized = _normalize_niagara_package_path(package_path)
    rel = normalized[len("/Game/") :]
    candidate = (_content_root() / rel).with_suffix(".uasset").resolve()
    root = _generated_niagara_content_root().resolve()
    if not candidate.is_relative_to(root):
        raise ValueError(f"Resolved path escapes generated Niagara root: {package_path}")
    return candidate


def _asset_file_to_package_path(asset_file: Path) -> str:
    rel = asset_file.resolve().relative_to(_content_root().resolve())
    return "/Game/" + rel.with_suffix("").as_posix()


def cleanup_generated_niagara_assets_offline_impl(
    package_paths: list[str] | None = None,
    name_prefix: str = "",
    older_than_minutes: float | None = None,
    dry_run: bool = True,
    require_editor_closed: bool = True,
    allow_all: bool = False,
) -> Dict[str, Any]:
    """Delete generated Niagara package files when the Unreal editor is closed."""
    try:
        root = _generated_niagara_content_root().resolve()
        if not root.is_relative_to(_content_root().resolve()):
            return _niagara_error(
                "Generated Niagara root does not resolve under project Content",
                error_code="niagara_cleanup_invalid_root",
                root=str(root),
            )
        if isinstance(package_paths, str):
            explicit_paths = [package_paths]
        else:
            explicit_paths = list(package_paths or [])
        prefix = str(name_prefix or "").strip()
        if any(char in prefix for char in ('/', "\\", "*", "?", "[", "]", ":")):
            return _niagara_error(
                "name_prefix must be an asset-name prefix, not a path or glob",
                error_code="niagara_cleanup_invalid_name_prefix",
                name_prefix=prefix,
            )
        if older_than_minutes is not None and older_than_minutes < 0:
            return _niagara_error(
                "older_than_minutes must be non-negative when provided",
                error_code="niagara_cleanup_invalid_age",
            )
        if not explicit_paths and not prefix and not allow_all:
            return _niagara_error(
                "Refusing broad cleanup: provide package_paths, name_prefix, or allow_all=true",
                error_code="niagara_cleanup_refused_broad_delete",
                root=str(root),
                dry_run=dry_run,
            )

        editor_running, editor_query_error = _query_unreal_editor_running()
        if require_editor_closed and editor_running is None and not dry_run:
            return _niagara_error(
                "Cannot prove that UnrealEditor.exe is closed; refusing offline cleanup",
                error_code="niagara_cleanup_editor_state_unknown",
                editor_running=None,
                editor_state_known=False,
                editor_query_error=editor_query_error,
                deleted=[],
                dry_run=dry_run,
                root=str(root),
            )
        if require_editor_closed and editor_running is True and not dry_run:
            return _niagara_error(
                "UnrealEditor.exe is running; close the editor or use dry_run=true",
                error_code="niagara_cleanup_editor_running",
                editor_running=editor_running,
                editor_state_known=True,
                editor_query_error="",
                deleted=[],
                dry_run=dry_run,
                root=str(root),
            )

        candidates: dict[Path, str] = {}
        errors: list[Dict[str, str]] = []

        for package_path in explicit_paths:
            try:
                asset_file = _package_path_to_asset_file(package_path)
                package_base = asset_file.with_suffix("")
                candidates[package_base] = _asset_file_to_package_path(package_base)
            except Exception as exc:
                errors.append({"input": str(package_path), "error": str(exc)})

        if prefix or allow_all:
            if root.exists():
                for sidecar_file in root.rglob("*"):
                    try:
                        if not sidecar_file.is_file():
                            continue
                        resolved = sidecar_file.resolve()
                        package_base = _package_base_from_sidecar(resolved)
                        if package_base is None or not package_base.is_relative_to(root):
                            continue
                        if prefix and not package_base.name.startswith(prefix):
                            continue
                        candidates[package_base] = _asset_file_to_package_path(package_base)
                    except Exception as exc:
                        errors.append({"input": str(sidecar_file), "error": str(exc)})

        cutoff = None
        if older_than_minutes is not None:
            cutoff = time.time() - (older_than_minutes * 60.0)

        selected: list[Dict[str, Any]] = []
        skipped: list[Dict[str, str]] = []
        for package_base, package_path in sorted(candidates.items(), key=lambda item: str(item[0])):
            sidecars = []
            for suffix in _NIAGARA_PACKAGE_SIDECARS:
                sidecar = package_base.with_suffix(suffix).resolve()
                if sidecar.exists():
                    if not sidecar.is_relative_to(root):
                        errors.append(
                            {"input": str(sidecar), "error": "sidecar escapes generated Niagara root"}
                        )
                        continue
                    sidecars.append(sidecar)
            if not sidecars:
                skipped.append({"package_path": package_path, "reason": "missing"})
                continue
            if cutoff is not None and any(path.stat().st_mtime > cutoff for path in sidecars):
                skipped.append({"package_path": package_path, "reason": "newer_than_cutoff"})
                continue

            selected.append(
                {
                    "package_path": package_path,
                    "files": [str(path) for path in sidecars],
                }
            )

        deleted: list[Dict[str, Any]] = []
        delete_errors: list[Dict[str, str]] = []
        if not dry_run:
            if errors:
                return _niagara_error(
                    "Offline Niagara cleanup preflight found invalid or unsafe candidates; "
                    "no files were deleted",
                    error_code="niagara_cleanup_preflight_failed",
                    dry_run=False,
                    editor_running=editor_running,
                    editor_state_known=editor_running is not None,
                    editor_query_error=editor_query_error,
                    require_editor_closed=require_editor_closed,
                    root=str(root),
                    package_root=_GENERATED_NIAGARA_ROOT,
                    selected_count=len(selected),
                    selected=selected,
                    skipped=skipped,
                    errors=errors,
                    deleted=[],
                    deletion_started=False,
                )
            if require_editor_closed:
                final_editor_running, final_editor_query_error = (
                    _query_unreal_editor_running()
                )
                if final_editor_running is not False:
                    return _niagara_error(
                        "Unreal Editor state changed or became unknown after cleanup "
                        "preflight; no files were deleted",
                        error_code=(
                            "niagara_cleanup_editor_reopened"
                            if final_editor_running is True
                            else "niagara_cleanup_editor_state_unknown"
                        ),
                        dry_run=False,
                        editor_running=final_editor_running,
                        editor_state_known=final_editor_running is not None,
                        editor_query_error=final_editor_query_error,
                        require_editor_closed=True,
                        preflight_editor_running=editor_running,
                        pre_delete_editor_recheck=True,
                        root=str(root),
                        package_root=_GENERATED_NIAGARA_ROOT,
                        selected_count=len(selected),
                        selected=selected,
                        skipped=skipped,
                        errors=[],
                        deleted=[],
                        deletion_started=False,
                    )
            for item in selected:
                item_deleted = []
                for file_path in item["files"]:
                    try:
                        Path(file_path).unlink()
                        item_deleted.append(file_path)
                    except Exception as exc:
                        delete_errors.append({"file": file_path, "error": str(exc)})
                deleted.append({"package_path": item["package_path"], "files": item_deleted})

        success = not errors and not delete_errors
        action = "would_delete" if dry_run else "deleted"
        return {
            "success": success,
            "message": f"{action} {len(selected)} generated Niagara package(s)",
            "dry_run": dry_run,
            "editor_running": editor_running,
            "editor_state_known": editor_running is not None,
            "editor_query_error": editor_query_error,
            "require_editor_closed": require_editor_closed,
            "root": str(root),
            "package_root": _GENERATED_NIAGARA_ROOT,
            "selected_count": len(selected),
            "would_delete": selected if dry_run else [],
            "deleted": deleted,
            "skipped": skipped,
            "errors": errors + delete_errors,
        }
    except Exception as exc:
        logger.error("Error cleaning generated Niagara assets offline: %s", exc)
        return _niagara_error(f"Error cleaning generated Niagara assets offline: {exc}")


def send_niagara_command(
    command_name: str,
    params: Dict[str, Any],
    timeout_seconds: float | None = None,
) -> Dict[str, Any]:
    from unreal_mcp_server import get_unreal_connection

    unreal = get_unreal_connection()
    if not unreal:
        logger.error("Failed to connect to Unreal Engine")
        return _niagara_error("Failed to connect to Unreal Engine", error_code="unreal_connection_failed")

    response = unreal.send_command(command_name, params, timeout_seconds=timeout_seconds)
    if not response:
        logger.error("No response from Unreal Engine")
        return _niagara_error("No response from Unreal Engine", error_code="unreal_no_response")

    return response


def _unwrap_niagara_response(
    command_name: str,
    response: Dict[str, Any],
    *,
    command_effect: Literal["read", "mutation", "unknown"] = "unknown",
) -> Dict[str, Any]:
    if not response:
        raise RuntimeError(f"No response from Unreal Engine for {command_name}")
    if response.get("status") == "error" or response.get("success") is False:
        raise NiagaraBridgeResponseError(
            command_name,
            response,
            command_effect=command_effect,
        )
    result = response.get("result")
    if isinstance(result, dict):
        if result.get("success") is False:
            raise NiagaraBridgeResponseError(
                command_name,
                response,
                command_effect=command_effect,
            )
        return result
    return response


def _unwrap_niagara_mutation_response(
    command_name: str,
    response: Any,
    *,
    command_effect: Literal["read", "mutation", "unknown"] = "mutation",
) -> Dict[str, Any]:
    """Require an exact success envelope after a mutation-capable dispatch."""

    if not isinstance(response, dict):
        raise RuntimeError(json.dumps(response, ensure_ascii=False, default=str))
    if response.get("status") != "success":
        raise NiagaraBridgeResponseError(
            command_name,
            response,
            command_effect=command_effect,
        )
    result = response.get("result")
    if not isinstance(result, dict) or result.get("success") is not True:
        raise NiagaraBridgeResponseError(
            command_name,
            response,
            command_effect=command_effect,
        )
    return result


def _collect_niagara_promotion_response_paths(
    response: Any,
    requested_target_system_path: str,
) -> tuple[set[str], set[str]]:
    """Keep canonical reload candidates separate from untrusted raw response paths."""

    candidates: set[str] = set()
    untrusted: set[str] = set()
    raw_paths: list[str] = [str(requested_target_system_path or "").strip()]
    response_result = response.get("result") if isinstance(response, dict) else None
    if isinstance(response_result, dict):
        raw_paths.extend(
            str(response_result.get(field) or "").strip()
            for field in ("target_system_path", "promoted_system_path")
        )
    for raw_path in raw_paths:
        if not raw_path:
            continue
        try:
            candidates.add(_normalize_game_package_path(raw_path))
        except ValueError:
            untrusted.add(raw_path)
    return candidates, untrusted


def _niagara_promotion_state_unknown_error(
    message: str,
    *,
    error_code: str,
    recovery_targets: set[str] | list[str],
    untrusted_response_paths: set[str] | list[str],
    **fields: Any,
) -> Dict[str, Any]:
    targets = sorted(set(recovery_targets))
    return _niagara_error(
        message,
        error_code=error_code,
        mutation_state_unknown=True,
        save_state_unknown=True,
        do_not_retry=True,
        requires_package_reload=bool(targets),
        command_may_still_be_in_flight=True,
        requires_editor_close_before_disk_verification=True,
        do_not_reload_or_trust_disk_until_editor_closed=True,
        cleanup_required_unknown=True,
        recovery_policy=(
            "close_editor_then_verify_exact_disk_targets_before_cleanup_or_retry"
        ),
        post_editor_close_recovery_policy="rehash_then_choose_exact_recovery",
        recovery_steps=[
            "close_editor_and_confirm_process_exit",
            "verify_exact_promotion_target_disk_state",
            "choose_exact_recovery_from_post_close_hashes",
        ],
        reload_required_packages=targets,
        cleanup_candidate_paths=[path for path in targets if _is_temp_game_path(path)],
        manual_review_candidate_paths=[
            path for path in targets if not _is_temp_game_path(path)
        ],
        observed_untrusted_response_paths=sorted(set(untrusted_response_paths)),
        **fields,
    )


def _unwrap_niagara_promotion_response(
    response: Any,
    *,
    expected_source_system_path: str,
    expected_target_system_path: str,
    expected_dry_run: bool,
    expected_save: bool,
    expected_request_compile: bool,
    expected_allow_non_temp_source: bool,
    expected_allow_temp_target: bool,
    expected_allow_temp_dependencies: bool,
    expected_overwrite_existing: bool,
    expected_forbidden_dependency_prefixes: list[str] | None = None,
    expected_forbidden_source_roots: list[str] | None = None,
) -> Dict[str, Any]:
    """Validate the promotion mutation/no-mutation boundary without inference."""

    result = _unwrap_niagara_mutation_response(
        "promote_niagara_system_candidate",
        response,
        command_effect="mutation",
    )
    for field in (
        "dry_run",
        "can_promote",
        "promoted",
        "save_requested",
        "saved",
        "compile_requested",
        "mutation_performed",
        "mutation_applied_in_memory",
        "allow_non_temp_source",
        "allow_temp_target",
        "allow_temp_dependencies",
        "overwrite_existing",
    ):
        if type(result.get(field)) is not bool:
            raise RuntimeError(
                "Malformed promote_niagara_system_candidate result: "
                f"{field} must be bool; {json.dumps(result, ensure_ascii=False, default=str)}"
            )
    if type(result.get("source_package_dirty")) is not bool:
        raise RuntimeError(
            "Malformed promote_niagara_system_candidate result: "
            "source_package_dirty must be bool; "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    promotion_blockers = result.get("promotion_blockers")
    if not isinstance(promotion_blockers, list) or not all(
        type(item) is str and bool(item.strip()) for item in promotion_blockers
    ):
        raise RuntimeError(
            "Malformed promote_niagara_system_candidate result: "
            "promotion_blockers must be a list of non-empty strings; "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    if result["can_promote"] != (len(promotion_blockers) == 0):
        raise RuntimeError(
            "Contradictory promote_niagara_system_candidate result: can_promote "
            "does not match promotion_blockers; "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    if result["source_package_dirty"] and result["can_promote"]:
        raise RuntimeError(
            "Contradictory promote_niagara_system_candidate result: dirty source "
            "cannot be promotable; "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    if (
        result["dry_run"] != expected_dry_run
        or result["save_requested"] != expected_save
        or result["compile_requested"] != expected_request_compile
        or result["allow_non_temp_source"] != expected_allow_non_temp_source
        or result["allow_temp_target"] != expected_allow_temp_target
        or result["allow_temp_dependencies"] != expected_allow_temp_dependencies
        or result["overwrite_existing"] != expected_overwrite_existing
    ):
        raise RuntimeError(
            "Promotion response does not match the dispatched request: "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    expected_source = _normalize_game_package_path(expected_source_system_path)
    expected_target = _normalize_game_package_path(expected_target_system_path)
    if (
        not expected_allow_non_temp_source
        and not expected_source.startswith(_GENERATED_NIAGARA_ROOT + "/")
    ):
        raise RuntimeError(
            "Promotion success contradicts allow_non_temp_source=false: "
            + expected_source
        )
    if _is_temp_game_path(expected_target) and not expected_allow_temp_target:
        raise RuntimeError(
            "Promotion success contradicts allow_temp_target=false: "
            + expected_target
        )
    if (
        not _is_temp_game_path(expected_target)
        and not expected_save
        and result["can_promote"]
    ):
        raise RuntimeError(
            "Promotion response incorrectly allows an unsaved non-temporary target: "
            + expected_target
        )
    if expected_overwrite_existing:
        raise RuntimeError(
            "Promotion success contradicts unsupported overwrite_existing=true"
        )
    try:
        response_source = _normalize_game_package_path(
            str(result.get("source_system_path") or "")
        )
        response_target = _normalize_game_package_path(
            str(result.get("target_system_path") or "")
        )
    except ValueError as exc:
        raise RuntimeError(
            "Promotion response omitted a valid source/target package binding: "
            + json.dumps(result, ensure_ascii=False, default=str)
        ) from exc
    if response_source != expected_source or response_target != expected_target:
        raise RuntimeError(
            "Promotion response source/target binding differs from the request: "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    forbidden_prefixes = list(expected_forbidden_dependency_prefixes or [])
    forbidden_roots = list(expected_forbidden_source_roots or [])
    preflight_audit_complete, preflight_audit_clean = (
        _validate_niagara_promotion_dependency_audit(
            result.get("preflight_dependency_audit"),
            expected_asset_path=expected_source,
            allow_temp_dependencies=expected_allow_temp_dependencies,
            forbidden_dependency_prefixes=forbidden_prefixes,
            forbidden_source_roots=forbidden_roots,
            audit_label="promotion preflight dependency audit",
            require_clean=False,
            require_complete=False,
        )
    )
    if type(result.get("preflight_dependency_audit_complete")) is not bool:
        raise RuntimeError(
            "Malformed promote_niagara_system_candidate result: "
            "preflight_dependency_audit_complete must be bool"
        )
    if result["preflight_dependency_audit_complete"] != preflight_audit_complete:
        raise RuntimeError(
            "Promotion response preflight completeness summary is contradictory: "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    preflight_audit = result["preflight_dependency_audit"]
    if type(result.get("preflight_persistence_evidence_complete")) is not bool:
        raise RuntimeError(
            "Malformed promote_niagara_system_candidate result: "
            "preflight_persistence_evidence_complete must be bool"
        )
    if result["preflight_persistence_evidence_complete"] != bool(
        preflight_audit.get("saved_file_scanned")
    ):
        raise RuntimeError(
            "Promotion response preflight persistence evidence is contradictory: "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    if result["can_promote"] and not (
        preflight_audit_complete
        and result["preflight_persistence_evidence_complete"]
        and preflight_audit_clean
    ):
        raise RuntimeError(
            "Contradictory promote_niagara_system_candidate result: can_promote=true "
            "with an incomplete or blocked preflight dependency audit; "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    if result["promoted"]:
        try:
            promoted_system_path = _normalize_game_package_path(
                str(result.get("promoted_system_path") or "")
            )
        except ValueError as exc:
            raise RuntimeError(
                "Promoted response omitted promoted_system_path: "
                + json.dumps(result, ensure_ascii=False, default=str)
            ) from exc
        post_promotion_audit = result.get("post_promotion_dependency_audit")
        _validate_niagara_promotion_dependency_audit(
            post_promotion_audit,
            expected_asset_path=expected_target,
            allow_temp_dependencies=expected_allow_temp_dependencies,
            forbidden_dependency_prefixes=forbidden_prefixes,
            forbidden_source_roots=forbidden_roots,
            audit_label="post-promotion dependency audit",
            require_clean=True,
            require_complete=True,
        )
        if result.get("post_promotion_dependency_audit_complete") is not True:
            raise RuntimeError(
                "Promoted response omitted a complete post-promotion dependency audit"
            )
        expected_post_persistence_evidence = bool(
            _is_temp_game_path(expected_target)
            or (
                expected_save
                and isinstance(post_promotion_audit, dict)
                and post_promotion_audit.get("saved_file_scanned") is True
            )
        )
        if (
            type(result.get("post_promotion_persistence_evidence_complete"))
            is not bool
            or result["post_promotion_persistence_evidence_complete"]
            != expected_post_persistence_evidence
            or not expected_post_persistence_evidence
        ):
            raise RuntimeError(
                "Promoted response lacks durable saved-file audit evidence: "
                + json.dumps(result, ensure_ascii=False, default=str)
            )
        valid_state = bool(
            result["can_promote"]
            and not result["dry_run"]
            and result["mutation_performed"]
            and result["mutation_applied_in_memory"]
            and result["saved"] == expected_save
            and promoted_system_path == expected_target
            and result.get("post_promotion_audit_pass") is True
        )
    else:
        valid_state = bool(
            not result["mutation_performed"]
            and not result["mutation_applied_in_memory"]
            and not result["saved"]
            and (result["dry_run"] or not result["can_promote"])
        )
    if not valid_state:
        raise RuntimeError(
            "Contradictory promote_niagara_system_candidate result: "
            + json.dumps(result, ensure_ascii=False, default=str)
        )
    return result


def _resolve_source_emitter_indices(source_analysis: Dict[str, Any], source_emitter_indices: list[int] | None) -> tuple[int, list[int]]:
    summary = source_analysis.get("summary") if isinstance(source_analysis.get("summary"), dict) else {}
    emitter_count = source_analysis.get("emitter_count", summary.get("emitter_count"))
    if isinstance(emitter_count, bool):
        emitter_count = 0
    elif isinstance(emitter_count, (int, float)):
        emitter_count = int(emitter_count)
    else:
        emitters = source_analysis.get("emitters")
        emitter_count = len(emitters) if isinstance(emitters, list) else 0
    if emitter_count <= 0:
        raise ValueError("Source Niagara system has no emitters")

    if source_emitter_indices is None:
        resolved_indices = list(range(emitter_count))
    else:
        resolved_indices = []
        for index in source_emitter_indices:
            if isinstance(index, bool):
                raise ValueError(f"source_emitter_indices values must be integers, not bool: {source_emitter_indices}")
            resolved_indices.append(int(index))

    invalid_indices = [index for index in resolved_indices if index < 0 or index >= emitter_count]
    if invalid_indices:
        raise ValueError(f"source_emitter_indices out of range 0..{emitter_count - 1}: {invalid_indices}")
    return emitter_count, resolved_indices


def create_niagara_system_from_source_template_impl(
    target_system_path: str,
    source_system_path: str,
    source_emitter_indices: list[int] | None = None,
    copy_source_user_parameters: bool = True,
    save: bool = True,
    request_compile: bool = True,
    wait_for_completion: bool = True,
    timeout_seconds: int = 180,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
) -> Dict[str, Any]:
    """Create a generated temp Niagara System by duplicating a source system."""

    mutation_dispatch_started = False
    last_mutation_response_received = False
    last_mutation_command_may_still_be_in_flight = False
    last_command_name = ""
    last_mutating_command_name = ""
    uncertain_cleanup_candidate_paths: set[str] = set()
    observed_untrusted_response_paths: set[str] = set()
    try:
        uncertain_cleanup_candidate_paths.add(
            _normalize_niagara_package_path(target_system_path)
        )
    except ValueError:
        if str(target_system_path or "").strip():
            observed_untrusted_response_paths.add(
                str(target_system_path).strip()
            )

    def command(
        command_name: str,
        params: Dict[str, Any],
        *,
        mutation_possible: bool = False,
    ) -> Dict[str, Any]:
        nonlocal mutation_dispatch_started, last_command_name, last_mutating_command_name
        nonlocal last_mutation_response_received
        nonlocal last_mutation_command_may_still_be_in_flight
        last_command_name = command_name
        if mutation_possible:
            mutation_dispatch_started = True
            last_mutating_command_name = command_name
            last_mutation_response_received = False
            last_mutation_command_may_still_be_in_flight = True
        if command_sender is not None:
            response = command_sender(command_name, params, timeout_seconds)
        else:
            response = send_niagara_command(command_name, params, timeout_seconds=timeout_seconds)
        if mutation_possible and response is not None:
            last_mutation_response_received = True
        if command_name == "promote_niagara_system_candidate":
            response_result = (
                response.get("result") if isinstance(response, dict) else None
            )
            if isinstance(response_result, dict):
                for path_field in ("target_system_path", "promoted_system_path"):
                    raw_candidate = str(
                        response_result.get(path_field) or ""
                    ).strip()
                    if not raw_candidate:
                        continue
                    try:
                        uncertain_cleanup_candidate_paths.add(
                            _normalize_niagara_package_path(raw_candidate)
                        )
                    except ValueError:
                        observed_untrusted_response_paths.add(raw_candidate)
            validated_promotion = _unwrap_niagara_promotion_response(
                response,
                expected_source_system_path=str(
                    params.get("source_system_path") or ""
                ),
                expected_target_system_path=str(
                    params.get("target_system_path") or ""
                ),
                expected_dry_run=params.get("dry_run") is True,
                expected_save=params.get("save") is True,
                expected_request_compile=params.get("request_compile") is True,
                expected_allow_non_temp_source=params.get("allow_non_temp_source") is True,
                expected_allow_temp_target=params.get("allow_temp_target") is True,
                expected_allow_temp_dependencies=params.get("allow_temp_dependencies") is True,
                expected_overwrite_existing=params.get("overwrite_existing") is True,
                expected_forbidden_dependency_prefixes=_coerce_string_list(
                    params.get("forbidden_dependency_prefixes"),
                    "forbidden_dependency_prefixes",
                ),
                expected_forbidden_source_roots=_coerce_string_list(
                    params.get("forbidden_source_roots"),
                    "forbidden_source_roots",
                ),
            )
            if mutation_possible:
                last_mutation_command_may_still_be_in_flight = False
            return validated_promotion
        if mutation_possible:
            validated_mutation = _unwrap_niagara_mutation_response(
                command_name,
                response,
            )
            last_mutation_command_may_still_be_in_flight = False
            return validated_mutation
        return _unwrap_niagara_response(
            command_name,
            response,
            command_effect="mutation" if mutation_possible else "read",
        )

    try:
        source_analysis = command(
            "analyze_niagara_system",
            {
                "system_path": source_system_path,
                "include_renderers": True,
                "include_user_parameters": True,
                "include_stack": False,
                "include_graph": False,
                "include_module_inputs": False,
                "include_compile_status": False,
                "include_scratch_pads": False,
            },
        )

        try:
            emitter_count, resolved_indices = _resolve_source_emitter_indices(source_analysis, source_emitter_indices)
        except ValueError as exc:
            return _niagara_error(
                f"{exc}: {source_system_path}",
                error_code="niagara_invalid_source_emitter_indices",
            )

        indices_to_keep = set(resolved_indices)
        indices_to_remove = [index for index in range(emitter_count) if index not in indices_to_keep]
        needs_emitter_removal = bool(indices_to_remove)

        create_result = command(
            "promote_niagara_system_candidate",
            {
                "source_system_path": source_system_path,
                "target_system_path": target_system_path,
                "dry_run": False,
                "allow_non_temp_source": True,
                "allow_temp_target": True,
                "allow_temp_dependencies": False,
                "save": bool(save and not needs_emitter_removal),
                "request_compile": bool(request_compile and not needs_emitter_removal),
            },
            mutation_possible=True,
        )

        # promote_niagara_system_candidate reports success=true even when promotion is
        # blocked (can_promote=false, dry-run semantics); in that case no target asset
        # exists and every follow-up command would fail with a misleading
        # "Failed to load Niagara system: <target>" error. Fail fast instead.
        if not create_result.get("promoted"):
            return _niagara_error(
                "Source system promotion did not create the target asset; "
                f"blockers: {create_result.get('promotion_blockers') or create_result.get('blocked_reasons') or 'unknown'}",
                error_code="niagara_promotion_blocked",
                promotion_result=create_result,
            )

        duplicated_system_path = (
            create_result.get("promoted_system_path")
            or create_result.get("target_system_path")
            or target_system_path
        )
        create_result.setdefault("system_path", duplicated_system_path)
        create_result.setdefault(
            "dependency_audit",
            create_result.get("post_promotion_dependency_audit")
            or create_result.get("preflight_dependency_audit"),
        )

        remove_results = []
        for remove_index, source_emitter_index in enumerate(reversed(indices_to_remove)):
            is_last = remove_index == len(indices_to_remove) - 1
            remove_result = command(
                "remove_niagara_emitter",
                {
                    "system_path": duplicated_system_path,
                    "emitter_index": source_emitter_index,
                    "allow_source_edit": False,
                    "save": bool(save and is_last),
                    "request_compile": bool(request_compile and is_last),
                },
                mutation_possible=True,
            )
            remove_results.append(remove_result)

        compile_result = {}
        if wait_for_completion:
            compile_result = command(
                "inspect_niagara_compile_status",
                {
                    "system_path": duplicated_system_path,
                    "request_compile": bool(request_compile),
                    "force": False,
                    "wait_for_completion": True,
                    "timeout_seconds": timeout_seconds,
                    "poll_interval_seconds": 0.1,
                },
                mutation_possible=bool(request_compile),
            )

        generated_analysis = command(
            "analyze_niagara_system",
            {
                "system_path": duplicated_system_path,
                "include_renderers": True,
                "include_user_parameters": True,
                "include_stack": True,
                "include_graph": False,
                "include_module_inputs": False,
                "include_compile_status": True,
                "include_scratch_pads": True,
            },
        )

        return {
            "success": True,
            "target_system_path": duplicated_system_path,
            "source_system_path": source_system_path,
            "source_emitter_count": emitter_count,
            "copied_emitter_indices": resolved_indices,
            "removed_source_emitter_indices": indices_to_remove,
            "copy_source_user_parameters": copy_source_user_parameters,
            "copy_source_user_parameters_note": (
                "Source User parameters are preserved by duplicating the whole source system; "
                "the flag is retained for API compatibility."
            ),
            "source_template_mode": "source_system_duplicate",
            "create_result": create_result,
            "attach_results": [],
            "remove_results": remove_results,
            "compile_result": compile_result,
            "generated_summary": generated_analysis.get("summary", {}),
            "generated_user_parameters": generated_analysis.get("user_parameters", {}),
            "write_scope": "generated_temp_niagara_source_system_duplicate",
        }
    except Exception as exc:
        logger.error("Error creating Niagara system from source template: %s", exc)
        if mutation_dispatch_started:
            if last_mutation_command_may_still_be_in_flight:
                recovery_policy = "close_editor_then_verify_exact_disk_before_recovery"
                post_close_policy = "rehash_then_choose_exact_recovery"
                recovery_action = (
                    f"Do not continue, retry, save, or reload '{target_system_path}'. "
                    "Close the editor and confirm the process has exited, then rehash the "
                    "exact target. Reload only unchanged bytes; exactly clean/recreate the "
                    "owned disposable target if its saved state changed or remains unknown."
                )
            else:
                recovery_policy = "reload_then_offline_exact_cleanup_required"
                post_close_policy = ""
                recovery_action = (
                    f"Do not continue or retry creation for '{target_system_path}'. "
                    "Reload that exact package if it exists, close the editor, and run exact-path "
                    "offline cleanup before starting another mutation."
                )
            return _niagara_error(
                "Niagara source-template creation failed after mutation dispatch; "
                "mutation and save state are unknown. " + recovery_action,
                error_code="niagara_system_template_creation_state_unknown",
                mutation_state_unknown=True,
                save_state_unknown=True,
                do_not_retry=True,
                requires_package_reload=True,
                transport_state_unknown=last_mutation_command_may_still_be_in_flight,
                mutation_response_received=last_mutation_response_received,
                command_may_still_be_in_flight=(
                    last_mutation_command_may_still_be_in_flight
                ),
                requires_editor_close_before_disk_verification=(
                    last_mutation_command_may_still_be_in_flight
                ),
                do_not_reload_or_trust_disk_until_editor_closed=(
                    last_mutation_command_may_still_be_in_flight
                ),
                cleanup_required_unknown=True,
                offline_exact_cleanup_required=bool(
                    uncertain_cleanup_candidate_paths
                ),
                recreate_required=bool(uncertain_cleanup_candidate_paths),
                exact_cleanup_only=bool(uncertain_cleanup_candidate_paths),
                cleanup_candidate_paths=sorted(uncertain_cleanup_candidate_paths),
                observed_untrusted_response_paths=sorted(
                    observed_untrusted_response_paths
                ),
                target_system_path=str(target_system_path or ""),
                source_system_path=str(source_system_path or ""),
                failed_command=last_command_name,
                last_mutating_command=last_mutating_command_name,
                recovery_policy=recovery_policy,
                post_editor_close_recovery_policy=post_close_policy,
                recovery_steps=(
                    [
                        "close_editor_and_confirm_process_exit",
                        "verify_exact_target_disk_hashes",
                        "choose_exact_recovery_from_post_close_hashes",
                    ]
                    if last_mutation_command_may_still_be_in_flight
                    else [
                        "reload_exact_target",
                        "close_editor_and_confirm_process_exit",
                        "offline_exact_cleanup_before_retry",
                    ]
                ),
                recovery_action=recovery_action,
                exception_type=type(exc).__name__,
                failure_detail=str(exc),
                cause=exc,
            )
        return _niagara_error(
            f"Error creating Niagara system from source template: {exc}",
            cause=exc,
        )


def create_niagara_scratch_pad_vec2_user_parameter_module_impl(
    target_system_path: str,
    scratch_pad_name: str = "MCP_BlankUpdateScratch",
    parameter_name: str = "Particles.SpriteSize",
    parameter_value: Any | None = None,
    user_parameter_name: str = "User.MCP_BlankScratchPad_SpriteSize",
    user_parameter_default_value: Any | None = None,
    target_usage: str = "ParticleUpdateScript",
    target_owner_kind: str = "system",
    target_emitter_index: int | None = None,
    target_emitter_name: str = "",
    target_stack_emitter_index: int | None = 0,
    target_stack_emitter_name: str = "",
    stack_suggested_name: str = "",
    allow_source_edit: bool = False,
    save: bool = True,
    request_compile: bool = True,
    wait_for_completion: bool = True,
    timeout_seconds: int = 180,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
) -> Dict[str, Any]:
    """Create a blank Scratch Pad module that writes a Vector2D User parameter."""

    completed_mutation_commands: list[str] = []

    def command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_sender is not None:
            response = command_sender(command_name, params, timeout_seconds)
        else:
            response = send_niagara_command(command_name, params, timeout_seconds=timeout_seconds)
        command_effect = (
            "read"
            if command_name == "inspect_niagara_compile_status"
            else "mutation"
        )
        result = _unwrap_niagara_response(
            command_name,
            response,
            command_effect=command_effect,
        )
        if command_effect == "mutation":
            completed_mutation_commands.append(command_name)
        return result

    if not target_system_path:
        return _niagara_error("target_system_path is required", error_code="niagara_missing_required_parameter")
    if parameter_value is None:
        parameter_value = [24.0, 24.0]
    if user_parameter_default_value is None:
        user_parameter_default_value = [42.0, 42.0]
    if not user_parameter_name:
        return _niagara_error("user_parameter_name is required", error_code="niagara_missing_required_parameter")
    if not parameter_name or "." not in parameter_name:
        return _niagara_error(
            "parameter_name must include a Niagara namespace, for example Particles.SpriteSize",
            error_code="niagara_invalid_parameter_name",
        )

    try:
        stages: list[Dict[str, Any]] = []

        create_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "new_script_name": scratch_pad_name,
            "target_supported_usage": target_usage,
            "target_owner_kind": target_owner_kind,
            "target_emitter_name": target_emitter_name,
            "allow_source_edit": allow_source_edit,
            "save": False,
            "request_compile": False,
        }
        if target_emitter_index is not None:
            create_params["target_emitter_index"] = target_emitter_index
        created_scratch = command("create_or_duplicate_scratch_pad_module", create_params)
        scratch_pad_path = str(created_scratch.get("new_script_path") or "")
        scratch_pad_script_name = str(created_scratch.get("new_script_name") or "")
        stages.append({"stage": "create_or_duplicate_scratch_pad_module", "result": created_scratch})

        parameter_write = command(
            "add_niagara_scratch_pad_parameter_write",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "parameter_name": parameter_name,
                "parameter_type": "vec2",
                "value": parameter_value,
                "skip_if_exists": True,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "add_niagara_scratch_pad_parameter_write", "result": parameter_write})

        user_link = command(
            "link_niagara_scratch_pad_pin_to_user_parameter",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "target_pin_name": parameter_name,
                "user_parameter_name": user_parameter_name,
                "default_value": user_parameter_default_value,
                "overwrite_existing": False,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "link_niagara_scratch_pad_pin_to_user_parameter", "result": user_link})

        stack_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "scratch_pad_script_path": scratch_pad_path,
            "target_usage": target_usage,
            "target_emitter_name": target_stack_emitter_name,
            "suggested_name": stack_suggested_name or scratch_pad_name,
            "skip_if_duplicate": True,
            "allow_source_edit": allow_source_edit,
            "save": save,
            "request_compile": request_compile,
        }
        if target_stack_emitter_index is not None:
            stack_params["target_emitter_index"] = target_stack_emitter_index
        stack_insert = command("add_scratch_pad_module_to_stack", stack_params)
        stages.append({"stage": "add_scratch_pad_module_to_stack", "result": stack_insert})

        compile_status: Dict[str, Any] | None = None
        if request_compile and wait_for_completion:
            compile_status = command(
                "inspect_niagara_compile_status",
                {
                    "system_path": target_system_path,
                    "request_compile": False,
                    "wait_for_completion": True,
                    "timeout_seconds": timeout_seconds,
                },
            )
            stages.append({"stage": "inspect_niagara_compile_status", "result": compile_status})

        return {
            "success": True,
            "system_path": stack_insert.get("system_path", target_system_path),
            "scratch_pad_script_path": scratch_pad_path,
            "scratch_pad_script_name": scratch_pad_script_name or user_link.get("scratch_pad_script_name"),
            "target_usage": target_usage,
            "parameter_name": parameter_name,
            "parameter_type": "Vector 2D",
            "parameter_value": parameter_value,
            "user_parameter_name": user_link.get("user_parameter_name", user_parameter_name),
            "linked_parameter_name": user_link.get("linked_parameter_name", user_parameter_name),
            "user_parameter_default_value": user_parameter_default_value,
            "user_parameter_created": user_link.get("user_parameter_created"),
            "parameter_map_set_node_name": user_link.get("parameter_map_set_node_name"),
            "parameter_map_set_node_guid": user_link.get("parameter_map_set_node_guid"),
            "new_link_count": user_link.get("new_link_count"),
            "new_module_node_name": stack_insert.get("new_module_node_name"),
            "new_module_function_name": stack_insert.get("new_module_function_name"),
            "compile_status": compile_status,
            "stages": stages,
            "write_scope": "scratch_pad_vec2_user_parameter_module_recipe",
            "saved": stack_insert.get("saved", False),
            "compile_requested": request_compile,
        }
    except Exception as exc:
        error_msg = f"Error creating Scratch Pad Vector2D User-parameter module: {exc}"
        logger.error(error_msg)
        return _niagara_error(
            error_msg,
            cause=exc,
            **_niagara_partial_mutation_fields(completed_mutation_commands),
        )


def create_niagara_scratch_pad_color_user_parameter_module_impl(
    target_system_path: str,
    scratch_pad_name: str = "MCP_ColorScratch",
    parameter_name: str = "Particles.Color",
    parameter_value: Any | None = None,
    user_parameter_name: str = "User.MCP_ScratchPad_Color",
    user_parameter_default_value: Any | None = None,
    target_usage: str = "ParticleUpdateScript",
    target_owner_kind: str = "system",
    target_emitter_index: int | None = None,
    target_emitter_name: str = "",
    target_stack_emitter_index: int | None = 0,
    target_stack_emitter_name: str = "",
    stack_suggested_name: str = "",
    allow_source_edit: bool = False,
    save: bool = True,
    request_compile: bool = True,
    wait_for_completion: bool = True,
    timeout_seconds: int = 180,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
) -> Dict[str, Any]:
    """Create a blank Scratch Pad module that writes a Linear Color User parameter."""

    completed_mutation_commands: list[str] = []

    def command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_sender is not None:
            response = command_sender(command_name, params, timeout_seconds)
        else:
            response = send_niagara_command(command_name, params, timeout_seconds=timeout_seconds)
        command_effect = (
            "read"
            if command_name == "inspect_niagara_compile_status"
            else "mutation"
        )
        result = _unwrap_niagara_response(
            command_name,
            response,
            command_effect=command_effect,
        )
        if command_effect == "mutation":
            completed_mutation_commands.append(command_name)
        return result

    if not target_system_path:
        return _niagara_error("target_system_path is required", error_code="niagara_missing_required_parameter")
    if parameter_value is None:
        parameter_value = [0.35, 0.65, 1.0, 1.0]
    if user_parameter_default_value is None:
        user_parameter_default_value = [1.0, 0.45, 0.18, 1.0]
    if not user_parameter_name:
        return _niagara_error("user_parameter_name is required", error_code="niagara_missing_required_parameter")
    if not parameter_name or "." not in parameter_name:
        return _niagara_error(
            "parameter_name must include a Niagara namespace, for example Particles.Color",
            error_code="niagara_invalid_parameter_name",
        )

    try:
        stages: list[Dict[str, Any]] = []

        create_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "new_script_name": scratch_pad_name,
            "target_supported_usage": target_usage,
            "target_owner_kind": target_owner_kind,
            "target_emitter_name": target_emitter_name,
            "allow_source_edit": allow_source_edit,
            "save": False,
            "request_compile": False,
        }
        if target_emitter_index is not None:
            create_params["target_emitter_index"] = target_emitter_index
        created_scratch = command("create_or_duplicate_scratch_pad_module", create_params)
        scratch_pad_path = str(created_scratch.get("new_script_path") or "")
        scratch_pad_script_name = str(created_scratch.get("new_script_name") or "")
        stages.append({"stage": "create_or_duplicate_scratch_pad_module", "result": created_scratch})

        parameter_write = command(
            "add_niagara_scratch_pad_parameter_write",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "parameter_name": parameter_name,
                "parameter_type": "color",
                "value": parameter_value,
                "skip_if_exists": True,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "add_niagara_scratch_pad_parameter_write", "result": parameter_write})

        user_link = command(
            "link_niagara_scratch_pad_pin_to_user_parameter",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "target_pin_name": parameter_name,
                "user_parameter_name": user_parameter_name,
                "default_value": user_parameter_default_value,
                "overwrite_existing": False,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "link_niagara_scratch_pad_pin_to_user_parameter", "result": user_link})

        stack_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "scratch_pad_script_path": scratch_pad_path,
            "target_usage": target_usage,
            "target_emitter_name": target_stack_emitter_name,
            "suggested_name": stack_suggested_name or scratch_pad_name,
            "skip_if_duplicate": True,
            "allow_source_edit": allow_source_edit,
            "save": save,
            "request_compile": request_compile,
        }
        if target_stack_emitter_index is not None:
            stack_params["target_emitter_index"] = target_stack_emitter_index
        stack_insert = command("add_scratch_pad_module_to_stack", stack_params)
        stages.append({"stage": "add_scratch_pad_module_to_stack", "result": stack_insert})

        compile_status: Dict[str, Any] | None = None
        if request_compile and wait_for_completion:
            compile_status = command(
                "inspect_niagara_compile_status",
                {
                    "system_path": target_system_path,
                    "request_compile": False,
                    "wait_for_completion": True,
                    "timeout_seconds": timeout_seconds,
                },
            )
            stages.append({"stage": "inspect_niagara_compile_status", "result": compile_status})

        return {
            "success": True,
            "system_path": stack_insert.get("system_path", target_system_path),
            "scratch_pad_script_path": scratch_pad_path,
            "scratch_pad_script_name": scratch_pad_script_name or user_link.get("scratch_pad_script_name"),
            "target_usage": target_usage,
            "parameter_name": parameter_name,
            "parameter_type": "Linear Color",
            "parameter_value": parameter_value,
            "user_parameter_name": user_link.get("user_parameter_name", user_parameter_name),
            "linked_parameter_name": user_link.get("linked_parameter_name", user_parameter_name),
            "user_parameter_default_value": user_parameter_default_value,
            "user_parameter_created": user_link.get("user_parameter_created"),
            "parameter_map_set_node_name": user_link.get("parameter_map_set_node_name"),
            "parameter_map_set_node_guid": user_link.get("parameter_map_set_node_guid"),
            "new_link_count": user_link.get("new_link_count"),
            "new_module_node_name": stack_insert.get("new_module_node_name"),
            "new_module_function_name": stack_insert.get("new_module_function_name"),
            "compile_status": compile_status,
            "stages": stages,
            "write_scope": "scratch_pad_color_user_parameter_module_recipe",
            "saved": stack_insert.get("saved", False),
            "compile_requested": request_compile,
        }
    except Exception as exc:
        error_msg = f"Error creating Scratch Pad Linear Color User-parameter module: {exc}"
        logger.error(error_msg)
        return _niagara_error(
            error_msg,
            cause=exc,
            **_niagara_partial_mutation_fields(completed_mutation_commands),
        )


def create_niagara_scratch_pad_bool_user_parameter_module_impl(
    target_system_path: str,
    scratch_pad_name: str = "MCP_BoolUserScratch",
    parameter_name: str = "Particles.MCPBoolProbe",
    parameter_value: Any | None = None,
    module_input_name: str = "MCPBoolInput",
    module_input_default_value: Any | None = None,
    user_parameter_name: str = "User.MCP_ScratchPad_Bool",
    user_parameter_default_value: Any | None = None,
    target_usage: str = "ParticleUpdateScript",
    target_owner_kind: str = "system",
    target_emitter_index: int | None = None,
    target_emitter_name: str = "",
    target_stack_emitter_index: int | None = 0,
    target_stack_emitter_name: str = "",
    stack_suggested_name: str = "",
    overwrite_existing: bool = False,
    allow_source_edit: bool = False,
    save: bool = True,
    request_compile: bool = True,
    wait_for_completion: bool = True,
    timeout_seconds: int = 180,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
) -> Dict[str, Any]:
    """Create a blank Scratch Pad bool module input bound to a User parameter."""

    completed_mutation_commands: list[str] = []

    def command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_sender is not None:
            response = command_sender(command_name, params, timeout_seconds)
        else:
            response = send_niagara_command(command_name, params, timeout_seconds=timeout_seconds)
        command_effect = (
            "read"
            if command_name == "inspect_niagara_compile_status"
            else "mutation"
        )
        result = _unwrap_niagara_response(
            command_name,
            response,
            command_effect=command_effect,
        )
        if command_effect == "mutation":
            completed_mutation_commands.append(command_name)
        return result

    if not target_system_path:
        return _niagara_error("target_system_path is required", error_code="niagara_missing_required_parameter")
    if parameter_value is None:
        parameter_value = True
    if module_input_default_value is None:
        module_input_default_value = True
    if user_parameter_default_value is None:
        user_parameter_default_value = False
    if not user_parameter_name:
        return _niagara_error("user_parameter_name is required", error_code="niagara_missing_required_parameter")
    if not module_input_name:
        return _niagara_error("module_input_name is required", error_code="niagara_missing_required_parameter")
    if not parameter_name or "." not in parameter_name:
        return _niagara_error(
            "parameter_name must include a Niagara namespace, for example Particles.MCPBoolProbe",
            error_code="niagara_invalid_parameter_name",
        )

    try:
        stages: list[Dict[str, Any]] = []

        create_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "new_script_name": scratch_pad_name,
            "target_supported_usage": target_usage,
            "target_owner_kind": target_owner_kind,
            "target_emitter_name": target_emitter_name,
            "allow_source_edit": allow_source_edit,
            "save": False,
            "request_compile": False,
        }
        if target_emitter_index is not None:
            create_params["target_emitter_index"] = target_emitter_index
        created_scratch = command("create_or_duplicate_scratch_pad_module", create_params)
        scratch_pad_path = str(created_scratch.get("new_script_path") or "")
        scratch_pad_script_name = str(created_scratch.get("new_script_name") or "")
        stages.append({"stage": "create_or_duplicate_scratch_pad_module", "result": created_scratch})

        parameter_write = command(
            "add_niagara_scratch_pad_parameter_write",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "parameter_name": parameter_name,
                "parameter_type": "bool",
                "value": parameter_value,
                "skip_if_exists": True,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "add_niagara_scratch_pad_parameter_write", "result": parameter_write})

        module_input = command(
            "add_niagara_scratch_pad_module_input",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "input_name": module_input_name,
                "input_type": "bool",
                "default_value": module_input_default_value,
                "consumer_pin_name": parameter_name,
                "require_consumer_link": True,
                "overwrite_existing_link": False,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "add_niagara_scratch_pad_module_input", "result": module_input})

        stack_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "scratch_pad_script_path": scratch_pad_path,
            "target_usage": target_usage,
            "target_emitter_name": target_stack_emitter_name,
            "suggested_name": stack_suggested_name or scratch_pad_name,
            "skip_if_duplicate": True,
            "allow_source_edit": allow_source_edit,
            "save": False,
            "request_compile": False,
        }
        if target_stack_emitter_index is not None:
            stack_params["target_emitter_index"] = target_stack_emitter_index
        stack_insert = command("add_scratch_pad_module_to_stack", stack_params)
        module_node_guid = str(
            stack_insert.get("new_module_node_guid") or stack_insert.get("existing_module_node_guid") or ""
        )
        if not module_node_guid:
            raise RuntimeError(f"Scratch Pad stack insertion did not return a module node GUID: {stack_insert}")
        stages.append({"stage": "add_scratch_pad_module_to_stack", "result": stack_insert})

        user_link_params: Dict[str, Any] = {
            "system_path": target_system_path,
            "input_name": module_input_name,
            "user_parameter_name": user_parameter_name,
            "default_value": user_parameter_default_value,
            "emitter_name": target_stack_emitter_name,
            "module_node_guid": module_node_guid,
            "overwrite_existing": overwrite_existing,
            "allow_source_edit": allow_source_edit,
            "save": save,
            "request_compile": request_compile,
            "wait_for_completion": wait_for_completion,
            "timeout_seconds": timeout_seconds,
            "poll_interval_seconds": 0.1,
        }
        if target_stack_emitter_index is not None:
            user_link_params["emitter_index"] = target_stack_emitter_index
        user_link = command("set_niagara_module_input_user_parameter", user_link_params)
        stages.append({"stage": "set_niagara_module_input_user_parameter", "result": user_link})

        compile_status: Dict[str, Any] | None = None
        if request_compile and wait_for_completion:
            compile_status = command(
                "inspect_niagara_compile_status",
                {
                    "system_path": target_system_path,
                    "request_compile": False,
                    "wait_for_completion": True,
                    "timeout_seconds": timeout_seconds,
                },
            )
            stages.append({"stage": "inspect_niagara_compile_status", "result": compile_status})

        return {
            "success": True,
            "system_path": user_link.get("system_path", stack_insert.get("system_path", target_system_path)),
            "scratch_pad_script_path": scratch_pad_path,
            "scratch_pad_script_name": scratch_pad_script_name or module_input.get("scratch_pad_script_name"),
            "target_usage": target_usage,
            "parameter_name": parameter_name,
            "parameter_type": "bool",
            "parameter_value": parameter_value,
            "module_input_name": user_link.get("input_name", module_input.get("input_name", module_input_name)),
            "module_input_default_value": module_input_default_value,
            "module_input_type": user_link.get("input_type", module_input.get("input_type", "bool")),
            "consumer_pin_name": module_input.get("consumer_pin_name", parameter_name),
            "new_consumer_link_count": module_input.get("new_consumer_link_count"),
            "user_parameter_name": user_link.get("user_parameter_name", user_parameter_name),
            "linked_parameter_name": user_link.get("linked_parameter_name", user_parameter_name),
            "user_parameter_default_value": user_parameter_default_value,
            "user_parameter_created": user_link.get("user_parameter_created"),
            "default_value_set": user_link.get("default_value_set"),
            "default_value": user_link.get("default_value"),
            "previous_override_link_count": user_link.get("previous_override_link_count"),
            "overwrote_existing": user_link.get("overwrote_existing"),
            "new_module_node_guid": module_node_guid,
            "new_module_node_name": stack_insert.get("new_module_node_name")
            or stack_insert.get("existing_module_node_name"),
            "new_module_function_name": stack_insert.get("new_module_function_name")
            or stack_insert.get("existing_module_function_name"),
            "compile_status": compile_status,
            "stages": stages,
            "write_scope": "scratch_pad_bool_user_parameter_module_recipe",
            "saved": user_link.get("saved", False),
            "compile_requested": request_compile,
        }
    except Exception as exc:
        error_msg = f"Error creating Scratch Pad bool User-parameter module: {exc}"
        logger.error(error_msg)
        return _niagara_error(
            error_msg,
            cause=exc,
            **_niagara_partial_mutation_fields(completed_mutation_commands),
        )


def create_niagara_scratch_pad_bool_visible_toggle_module_impl(
    target_system_path: str,
    scratch_pad_name: str = "MCP_BoolVisibleScratch",
    parameter_name: str = "Particles.SpriteSize",
    parameter_value: Any | None = None,
    base_module_input_name: str = "MCPBaseSpriteSize",
    base_module_input_default_value: Any | None = None,
    user_parameter_name: str = "User.MCP_VisibleToggle",
    user_parameter_default_value: Any | None = None,
    custom_hlsl_input_name: str = "MCPVisibleToggle",
    custom_hlsl_expression: str = "BaseValue * (MCPVisibleToggle ? 1.0 : 0.0)",
    custom_hlsl_output_pin_name: str = "MCPVisibleSize",
    custom_hlsl_signature_name: str = "MCP_ScratchPadBoolVisibleSize",
    target_usage: str = "ParticleUpdateScript",
    target_owner_kind: str = "system",
    target_emitter_index: int | None = None,
    target_emitter_name: str = "",
    target_stack_emitter_index: int | None = 0,
    target_stack_emitter_name: str = "",
    stack_suggested_name: str = "",
    overwrite_existing: bool = False,
    allow_source_edit: bool = False,
    save: bool = True,
    request_compile: bool = True,
    wait_for_completion: bool = True,
    timeout_seconds: int = 180,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
) -> Dict[str, Any]:
    """Create a temp Scratch Pad module where a bool User parameter toggles sprite visibility."""

    completed_mutation_commands: list[str] = []

    def command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_sender is not None:
            response = command_sender(command_name, params, timeout_seconds)
        else:
            response = send_niagara_command(command_name, params, timeout_seconds=timeout_seconds)
        command_effect = (
            "read"
            if command_name == "inspect_niagara_compile_status"
            else "mutation"
        )
        result = _unwrap_niagara_response(
            command_name,
            response,
            command_effect=command_effect,
        )
        if command_effect == "mutation":
            completed_mutation_commands.append(command_name)
        return result

    if not target_system_path:
        return _niagara_error("target_system_path is required", error_code="niagara_missing_required_parameter")
    if parameter_value is None:
        parameter_value = [64.0, 64.0]
    if base_module_input_default_value is None:
        base_module_input_default_value = parameter_value
    if user_parameter_default_value is None:
        user_parameter_default_value = True
    if not user_parameter_name:
        return _niagara_error("user_parameter_name is required", error_code="niagara_missing_required_parameter")
    if not base_module_input_name:
        return _niagara_error("base_module_input_name is required", error_code="niagara_missing_required_parameter")
    if not custom_hlsl_input_name:
        return _niagara_error("custom_hlsl_input_name is required", error_code="niagara_missing_required_parameter")
    if not parameter_name or "." not in parameter_name:
        return _niagara_error(
            "parameter_name must include a Niagara namespace, for example Particles.SpriteSize",
            error_code="niagara_invalid_parameter_name",
        )
    if overwrite_existing:
        return _niagara_error(
            "overwrite_existing is not admitted for bool visible Scratch Pad toggle creation",
            error_code="niagara_overwrite_not_allowed",
        )

    try:
        stages: list[Dict[str, Any]] = []

        create_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "new_script_name": scratch_pad_name,
            "target_supported_usage": target_usage,
            "target_owner_kind": target_owner_kind,
            "target_emitter_name": target_emitter_name,
            "allow_source_edit": allow_source_edit,
            "save": False,
            "request_compile": False,
        }
        if target_emitter_index is not None:
            create_params["target_emitter_index"] = target_emitter_index
        created_scratch = command("create_or_duplicate_scratch_pad_module", create_params)
        scratch_pad_path = str(created_scratch.get("new_script_path") or "")
        scratch_pad_script_name = str(created_scratch.get("new_script_name") or "")
        stages.append({"stage": "create_or_duplicate_scratch_pad_module", "result": created_scratch})

        parameter_write = command(
            "add_niagara_scratch_pad_parameter_write",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "parameter_name": parameter_name,
                "parameter_type": "vec2",
                "value": parameter_value,
                "skip_if_exists": True,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "add_niagara_scratch_pad_parameter_write", "result": parameter_write})

        base_module_input = command(
            "add_niagara_scratch_pad_module_input",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "input_name": base_module_input_name,
                "input_type": "vec2",
                "default_value": base_module_input_default_value,
                "consumer_pin_name": parameter_name,
                "require_consumer_link": True,
                "overwrite_existing_link": False,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "add_niagara_scratch_pad_module_input", "result": base_module_input})

        user_upsert = command(
            "upsert_niagara_user_parameters",
            {
                "system_path": target_system_path,
                "parameters": [
                    {
                        "name": user_parameter_name,
                        "type": "bool",
                        "default_value": user_parameter_default_value,
                    }
                ],
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
                "continue_on_error": False,
            },
        )
        stages.append({"stage": "upsert_niagara_user_parameters", "result": user_upsert})

        custom_hlsl = command(
            "insert_niagara_scratch_pad_custom_hlsl_for_pin",
            {
                "system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_path,
                "scratch_pad_owner_kind": target_owner_kind,
                "scratch_pad_name": scratch_pad_script_name or scratch_pad_name,
                "target_pin_name": parameter_name,
                "expression": custom_hlsl_expression,
                "preserve_existing_link_as": "BaseValue",
                "output_pin_name": custom_hlsl_output_pin_name,
                "signature_name": custom_hlsl_signature_name,
                "user_parameter_inputs": [
                    {
                        "input_name": custom_hlsl_input_name,
                        "user_parameter_name": user_parameter_name,
                    }
                ],
                "skip_if_already_inserted": True,
                "replace_existing_custom": False,
                "rebuild_existing_custom": False,
                "requires_context": True,
                "delete_unlinked_custom_nodes": False,
                "delete_unlinked_custom_input_source_nodes": False,
                "allow_source_edit": allow_source_edit,
                "save": False,
                "request_compile": False,
            },
        )
        stages.append({"stage": "insert_niagara_scratch_pad_custom_hlsl_for_pin", "result": custom_hlsl})

        stack_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "scratch_pad_script_path": scratch_pad_path,
            "target_usage": target_usage,
            "target_emitter_name": target_stack_emitter_name,
            "suggested_name": stack_suggested_name or scratch_pad_name,
            "skip_if_duplicate": True,
            "allow_source_edit": allow_source_edit,
            "save": save,
            "request_compile": request_compile,
        }
        if target_stack_emitter_index is not None:
            stack_params["target_emitter_index"] = target_stack_emitter_index
        stack_insert = command("add_scratch_pad_module_to_stack", stack_params)
        module_node_guid = str(
            stack_insert.get("new_module_node_guid") or stack_insert.get("existing_module_node_guid") or ""
        )
        if not module_node_guid:
            raise RuntimeError(f"Scratch Pad stack insertion did not return a module node GUID: {stack_insert}")
        stages.append({"stage": "add_scratch_pad_module_to_stack", "result": stack_insert})

        compile_status: Dict[str, Any] | None = None
        if request_compile and wait_for_completion:
            compile_status = command(
                "inspect_niagara_compile_status",
                {
                    "system_path": target_system_path,
                    "request_compile": False,
                    "wait_for_completion": True,
                    "timeout_seconds": timeout_seconds,
                },
            )
            stages.append({"stage": "inspect_niagara_compile_status", "result": compile_status})

        return {
            "success": True,
            "system_path": stack_insert.get("system_path", target_system_path),
            "scratch_pad_script_path": scratch_pad_path,
            "scratch_pad_script_name": scratch_pad_script_name
            or base_module_input.get("scratch_pad_script_name")
            or custom_hlsl.get("scratch_pad_script_name"),
            "target_usage": target_usage,
            "parameter_name": parameter_name,
            "parameter_type": "vec2",
            "parameter_value": parameter_value,
            "base_module_input_name": base_module_input.get("input_name", base_module_input_name),
            "base_module_input_type": base_module_input.get("input_type", "vec2"),
            "base_module_input_default_value": base_module_input_default_value,
            "base_consumer_pin_name": base_module_input.get("consumer_pin_name", parameter_name),
            "base_new_consumer_link_count": base_module_input.get("new_consumer_link_count"),
            "user_parameter_name": user_parameter_name if user_parameter_name.startswith("User.") else f"User.{user_parameter_name}",
            "user_parameter_default_value": user_parameter_default_value,
            "user_parameter_created": bool(user_upsert.get("added_count")),
            "custom_hlsl_input_name": custom_hlsl_input_name,
            "custom_hlsl_expression": custom_hlsl.get("expression", custom_hlsl_expression),
            "custom_hlsl_output_pin_name": custom_hlsl.get("output_pin_name", custom_hlsl_output_pin_name),
            "custom_hlsl_signature_name": custom_hlsl_signature_name,
            "custom_hlsl_node_guid": custom_hlsl.get("custom_hlsl_node_guid"),
            "custom_hlsl_previous_link_count": custom_hlsl.get("previous_link_count"),
            "custom_hlsl_user_parameter_input_count": custom_hlsl.get("user_parameter_input_count"),
            "custom_hlsl_created_user_parameter_input_node_count": custom_hlsl.get(
                "created_user_parameter_input_node_count"
            ),
            "custom_hlsl_auto_added_context_map_input": custom_hlsl.get("auto_added_context_map_input"),
            "custom_hlsl_custom_inputs": custom_hlsl.get("custom_inputs"),
            "new_module_node_guid": module_node_guid,
            "new_module_node_name": stack_insert.get("new_module_node_name")
            or stack_insert.get("existing_module_node_name"),
            "new_module_function_name": stack_insert.get("new_module_function_name")
            or stack_insert.get("existing_module_function_name"),
            "compile_status": compile_status,
            "stages": stages,
            "write_scope": "scratch_pad_bool_visible_toggle_module_recipe",
            "saved": stack_insert.get("saved", False),
            "compile_requested": request_compile,
        }
    except Exception as exc:
        error_msg = f"Error creating Scratch Pad bool visible toggle module: {exc}"
        logger.error(error_msg)
        return _niagara_error(
            error_msg,
            cause=exc,
            **_niagara_partial_mutation_fields(completed_mutation_commands),
        )


def create_niagara_visible_scratch_pad_snow_seed_impl(
    target_system_path: str,
    preset: str = "snow",
    emitter_name: str = "Emitter",
    material_path: str = "",
    use_project_weather_materials: bool = True,
    scratch_pad_name: str = "MCP_VisibleSizeScratch",
    parameter_name: str = "Particles.SpriteSize",
    parameter_value: Any | None = None,
    user_parameter_name: str = "User.MCP_VisibleScratchPad_SpriteSize",
    user_parameter_default_value: Any | None = None,
    target_usage: str = "ParticleUpdateScript",
    spawn_rate: float | None = 180.0,
    velocity: list | None = None,
    sprite_size: list | None = None,
    lifetime: float | None = 10.0,
    add_spawn_box_location: bool | None = True,
    spawn_box_size: list | None = None,
    spawn_box_offset: list | None = None,
    add_weather_detail_modules: bool | None = True,
    allow_source_edit: bool = False,
    save: bool = True,
    request_compile: bool = True,
    wait_for_completion: bool = True,
    timeout_seconds: int = 180,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
) -> Dict[str, Any]:
    """Create a visible snow-like temp seed and add a Scratch Pad User sprite-size control."""

    completed_mutation_commands: list[str] = []

    def command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_sender is not None:
            response = command_sender(command_name, params, timeout_seconds)
        else:
            response = send_niagara_command(command_name, params, timeout_seconds=timeout_seconds)
        result = _unwrap_niagara_response(
            command_name,
            response,
            command_effect="mutation",
        )
        completed_mutation_commands.append(command_name)
        return result

    if not target_system_path:
        return _niagara_error("target_system_path is required", error_code="niagara_missing_required_parameter")
    if parameter_value is None:
        parameter_value = [36.0, 36.0]
    if user_parameter_default_value is None:
        user_parameter_default_value = [56.0, 56.0]
    if velocity is None:
        velocity = [0.0, 0.0, -90.0]
    if sprite_size is None:
        sprite_size = [32.0, 32.0]
    if spawn_box_size is None:
        spawn_box_size = [1200.0, 1200.0, 700.0]
    if spawn_box_offset is None:
        spawn_box_offset = [0.0, 0.0, 300.0]

    try:
        preset_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "preset": preset,
            "emitter_name": emitter_name,
            "material_path": material_path,
            "use_project_weather_materials": use_project_weather_materials,
            "add_spawn_box_location": add_spawn_box_location,
            "spawn_box_size": spawn_box_size,
            "spawn_box_offset": spawn_box_offset,
            "add_weather_detail_modules": add_weather_detail_modules,
            "allow_non_temp_path": allow_source_edit,
            "overwrite_existing": False,
            "save": True,
            "wait_for_completion": True,
            "timeout_seconds": timeout_seconds,
        }
        if spawn_rate is not None:
            preset_params["spawn_rate"] = spawn_rate
        if velocity is not None:
            preset_params["velocity"] = velocity
        if sprite_size is not None:
            preset_params["sprite_size"] = sprite_size
        if lifetime is not None:
            preset_params["lifetime"] = lifetime

        created_system = command("create_sprite_niagara_preset_system", preset_params)
        created_system_path = str(
            created_system.get("system_path")
            or created_system.get("target_system_path")
            or target_system_path
        )

        scratch_pad_recipe = create_niagara_scratch_pad_vec2_user_parameter_module_impl(
            target_system_path=created_system_path,
            scratch_pad_name=scratch_pad_name,
            parameter_name=parameter_name,
            parameter_value=parameter_value,
            user_parameter_name=user_parameter_name,
            user_parameter_default_value=user_parameter_default_value,
            target_usage=target_usage,
            target_stack_emitter_index=0,
            stack_suggested_name=scratch_pad_name,
            allow_source_edit=allow_source_edit,
            save=save,
            request_compile=request_compile,
            wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,
            command_sender=command_sender,
        )
        if scratch_pad_recipe.get("success") is False:
            inner_completed = scratch_pad_recipe.get("completed_mutation_commands")
            combined_completed = list(completed_mutation_commands)
            if isinstance(inner_completed, list):
                combined_completed.extend(
                    str(command_name)
                    for command_name in inner_completed
                    if isinstance(command_name, str) and command_name
                )
            bridge_failure_fields = {
                key: scratch_pad_recipe[key]
                for key in (
                    "unreal_response",
                    "read_unreal_response",
                    "failed_bridge_command",
                    "failed_bridge_command_effect",
                )
                if key in scratch_pad_recipe
            }
            return _niagara_error(
                "Scratch Pad recipe failed during visible snow seed creation",
                error_code="niagara_visible_seed_scratch_pad_failed",
                created_system=created_system,
                scratch_pad_recipe=scratch_pad_recipe,
                **bridge_failure_fields,
                **_niagara_partial_mutation_fields(combined_completed),
            )

        return {
            "success": True,
            "system_path": created_system_path,
            "preset": preset,
            "material_path": material_path,
            "use_project_weather_materials": use_project_weather_materials,
            "scratch_pad_script_path": scratch_pad_recipe.get("scratch_pad_script_path"),
            "scratch_pad_script_name": scratch_pad_recipe.get("scratch_pad_script_name"),
            "parameter_name": parameter_name,
            "parameter_type": "Vector 2D",
            "parameter_value": parameter_value,
            "user_parameter_name": scratch_pad_recipe.get("user_parameter_name", user_parameter_name),
            "user_parameter_default_value": user_parameter_default_value,
            "created_system": created_system,
            "scratch_pad_recipe": scratch_pad_recipe,
            "compile_status": scratch_pad_recipe.get("compile_status"),
            "write_scope": "visible_scratch_pad_snow_seed_recipe",
            "saved": bool(save),
            "compile_requested": request_compile,
        }
    except Exception as exc:
        error_msg = f"Error creating visible Scratch Pad snow seed: {exc}"
        logger.error(error_msg)
        return _niagara_error(
            error_msg,
            cause=exc,
            **_niagara_partial_mutation_fields(completed_mutation_commands),
        )


def promote_niagara_system_candidate_impl(
    source_system_path: str,
    target_system_path: str,
    dry_run: bool = True,
    forbidden_dependency_prefixes: list[str] | None = None,
    forbidden_source_roots: list[str] | None = None,
    allow_non_temp_source: bool = False,
    allow_temp_target: bool = False,
    allow_temp_dependencies: bool = False,
    overwrite_existing: bool = False,
    save: bool = True,
    request_compile: bool = True,
    timeout_seconds: float = 120.0,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
    preserve_success_envelope: bool = False,
) -> Dict[str, Any]:
    """Dispatch one promotion with strict request/response and unknown-state recovery."""

    dispatch_attempted = False
    response_candidates: set[str] = set()
    untrusted_response_paths: set[str] = set()
    try:
        normalized_source = _normalize_game_package_path(source_system_path)
        normalized_target = _normalize_game_package_path(target_system_path)
        forbidden_prefixes = _coerce_string_list(
            forbidden_dependency_prefixes,
            "forbidden_dependency_prefixes",
        )
        forbidden_roots = _coerce_string_list(
            forbidden_source_roots,
            "forbidden_source_roots",
        )
        params: Dict[str, Any] = {
            "source_system_path": normalized_source,
            "target_system_path": normalized_target,
            "dry_run": dry_run,
            "allow_non_temp_source": allow_non_temp_source,
            "allow_temp_target": allow_temp_target,
            "allow_temp_dependencies": allow_temp_dependencies,
            "overwrite_existing": overwrite_existing,
            "save": save,
            "request_compile": request_compile,
        }
        if forbidden_prefixes:
            params["forbidden_dependency_prefixes"] = forbidden_prefixes
        if forbidden_roots:
            params["forbidden_source_roots"] = forbidden_roots

        response_candidates, untrusted_response_paths = (
            _collect_niagara_promotion_response_paths(None, normalized_target)
        )
        dispatch_attempted = True
        if command_sender is not None:
            response = command_sender(
                "promote_niagara_system_candidate",
                params,
                timeout_seconds,
            )
        else:
            response = send_niagara_command(
                "promote_niagara_system_candidate",
                params,
                timeout_seconds=timeout_seconds,
            )
        returned_candidates, returned_untrusted = (
            _collect_niagara_promotion_response_paths(response, normalized_target)
        )
        response_candidates.update(returned_candidates)
        untrusted_response_paths.update(returned_untrusted)
        validated_result = _unwrap_niagara_promotion_response(
            response,
            expected_source_system_path=normalized_source,
            expected_target_system_path=normalized_target,
            expected_dry_run=dry_run is True,
            expected_save=save is True,
            expected_request_compile=request_compile is True,
            expected_allow_non_temp_source=allow_non_temp_source is True,
            expected_allow_temp_target=allow_temp_target is True,
            expected_allow_temp_dependencies=allow_temp_dependencies is True,
            expected_overwrite_existing=overwrite_existing is True,
            expected_forbidden_dependency_prefixes=forbidden_prefixes,
            expected_forbidden_source_roots=forbidden_roots,
        )
        if preserve_success_envelope:
            # The strict unwrap above proved both envelope layers and the exact
            # request binding, so preserve the bridge response verbatim for the
            # legacy public tool contract instead of synthesizing a second shape.
            return response
        return validated_result
    except Exception as exc:
        if dispatch_attempted:
            return _niagara_promotion_state_unknown_error(
                "Niagara promotion response was not strictly validated; mutation and save "
                "state are unknown. Do not retry before exact-package recovery.",
                error_code="niagara_promotion_response_state_unknown",
                recovery_targets=response_candidates,
                untrusted_response_paths=untrusted_response_paths,
                dispatch_attempted=True,
                dry_run_requested=dry_run is True,
                target_system_path=str(target_system_path or ""),
                failure_detail={
                    "type": type(exc).__name__,
                    "message": str(exc),
                },
                cause=exc,
            )
        logger.error("Error preparing Niagara promotion candidate request: %s", exc)
        return _niagara_error(
            f"Error preparing Niagara promotion candidate request: {exc}",
            error_code="niagara_promotion_invalid_request",
            dispatch_attempted=False,
        )


def plan_niagara_promotion_manifest_impl(
    source_system_path: str = "",
    source_system_paths: list[str] | None = None,
    target_root: str = "/Game/MCPWeather/Generated",
    target_system_paths: list[str] | None = None,
    asset_name_prefix: str = "",
    asset_name_suffix: str = "",
    forbidden_dependency_prefixes: list[str] | None = None,
    forbidden_source_roots: list[str] | None = None,
    allow_non_temp_source: bool = False,
    allow_temp_target: bool = False,
    allow_temp_dependencies: bool = False,
    write_manifest: bool = False,
    manifest_name: str = "",
    timeout_seconds: float = 120.0,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
) -> Dict[str, Any]:
    """Plan Niagara promotion candidates by running dry-run promotion preflights."""

    last_promotion_response_candidate_paths: set[str] = set()
    last_promotion_untrusted_response_paths: set[str] = set()
    last_promotion_response_validated = False

    def command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        nonlocal last_promotion_response_candidate_paths
        nonlocal last_promotion_untrusted_response_paths
        nonlocal last_promotion_response_validated
        if command_name == "promote_niagara_system_candidate":
            last_promotion_response_candidate_paths, last_promotion_untrusted_response_paths = (
                _collect_niagara_promotion_response_paths(
                    None,
                    str(params.get("target_system_path") or ""),
                )
            )
            last_promotion_response_validated = False
        if command_sender is not None:
            response = command_sender(command_name, params, timeout_seconds)
        else:
            response = send_niagara_command(command_name, params, timeout_seconds=timeout_seconds)
        if command_name == "promote_niagara_system_candidate":
            response_candidates, response_untrusted = (
                _collect_niagara_promotion_response_paths(
                    response,
                    str(params.get("target_system_path") or ""),
                )
            )
            last_promotion_response_candidate_paths.update(response_candidates)
            last_promotion_untrusted_response_paths.update(response_untrusted)
            validated_result = _unwrap_niagara_promotion_response(
                response,
                expected_source_system_path=str(
                    params.get("source_system_path") or ""
                ),
                expected_target_system_path=str(
                    params.get("target_system_path") or ""
                ),
                expected_dry_run=params.get("dry_run") is True,
                expected_save=params.get("save") is True,
                expected_request_compile=params.get("request_compile") is True,
                expected_allow_non_temp_source=params.get("allow_non_temp_source") is True,
                expected_allow_temp_target=params.get("allow_temp_target") is True,
                expected_allow_temp_dependencies=params.get("allow_temp_dependencies") is True,
                expected_overwrite_existing=params.get("overwrite_existing") is True,
                expected_forbidden_dependency_prefixes=_coerce_string_list(
                    params.get("forbidden_dependency_prefixes"),
                    "forbidden_dependency_prefixes",
                ),
                expected_forbidden_source_roots=_coerce_string_list(
                    params.get("forbidden_source_roots"),
                    "forbidden_source_roots",
                ),
            )
            last_promotion_response_validated = True
            return validated_result
        return _unwrap_niagara_response(command_name, response)

    try:
        created_at_utc = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
        try:
            sources = _coerce_string_list(source_system_path, "source_system_path")
            sources.extend(_coerce_string_list(source_system_paths, "source_system_paths"))
            explicit_targets = _coerce_string_list(target_system_paths, "target_system_paths")
            normalized_target_root = _normalize_game_root_path(target_root)
        except Exception as exc:
            return _niagara_error(
                str(exc),
                error_code="niagara_promotion_manifest_invalid_request",
                all_can_promote=False,
                entries=[],
            )

        if not sources:
            return _niagara_error(
                "At least one source_system_path is required",
                error_code="niagara_promotion_manifest_missing_source",
                all_can_promote=False,
                entries=[],
            )
        if explicit_targets and len(explicit_targets) != len(sources):
            return _niagara_error(
                "target_system_paths length must match source_system_paths length",
                error_code="niagara_promotion_manifest_target_count_mismatch",
                source_count=len(sources),
                target_count=len(explicit_targets),
                all_can_promote=False,
                entries=[],
            )
        if _is_temp_game_path(normalized_target_root) and not allow_temp_target:
            return _niagara_error(
                "target_root under /Game/_MCP_Temp requires allow_temp_target=true",
                error_code="niagara_promotion_manifest_temp_target_requires_opt_in",
                target_root=normalized_target_root,
                all_can_promote=False,
                entries=[],
            )

        forbidden_prefixes = (
            _coerce_string_list(forbidden_dependency_prefixes, "forbidden_dependency_prefixes")
            if forbidden_dependency_prefixes is not None
            else []
        )
        forbidden_roots = (
            _coerce_string_list(forbidden_source_roots, "forbidden_source_roots")
            if forbidden_source_roots is not None
            else []
        )

        entries: list[Dict[str, Any]] = []
        seen_targets: set[str] = set()
        dry_run_recovery_targets: set[str] = set()
        dry_run_untrusted_response_paths: set[str] = set()
        dry_run_bridge_failure_fields: Dict[str, Any] = {}
        for index, source in enumerate(sources):
            entry: Dict[str, Any] = {
                "index": index,
                "source_system_path": source,
                "dry_run": True,
                "can_promote": False,
                "promotion_blockers": [],
            }
            try:
                normalized_source = _normalize_game_package_path(source)
                target_path = (
                    _normalize_game_package_path(explicit_targets[index])
                    if explicit_targets
                    else _target_path_for_promotion(
                        normalized_source,
                        normalized_target_root,
                        asset_name_prefix,
                        asset_name_suffix,
                    )
                )
                entry["source_system_path"] = normalized_source
                entry["target_system_path"] = target_path

                local_blockers: list[str] = []
                if _is_temp_game_path(target_path) and not allow_temp_target:
                    local_blockers.append("target_system_path under /Game/_MCP_Temp requires allow_temp_target=true")
                if normalized_source == target_path:
                    local_blockers.append("source_system_path and target_system_path must differ")
                if target_path in seen_targets:
                    local_blockers.append("Duplicate target_system_path in promotion manifest")
                if _has_forbidden_mcp_reference_marker(normalized_source):
                    local_blockers.append("source_system_path contains an MCP-only reference marker")
                if _has_forbidden_mcp_reference_marker(target_path):
                    local_blockers.append("target_system_path contains an MCP-only reference marker")
                seen_targets.add(target_path)

                if local_blockers:
                    entry["promotion_blockers"] = local_blockers
                    entries.append(entry)
                    continue

                params: Dict[str, Any] = {
                    "source_system_path": normalized_source,
                    "target_system_path": target_path,
                    "dry_run": True,
                    "allow_non_temp_source": allow_non_temp_source,
                    "allow_temp_target": allow_temp_target,
                    "allow_temp_dependencies": allow_temp_dependencies,
                    "overwrite_existing": False,
                    # Dry-run validates the policy that the default execution
                    # will use; it still performs no save or compile mutation.
                    "save": True,
                    "request_compile": True,
                }
                if forbidden_prefixes:
                    params["forbidden_dependency_prefixes"] = forbidden_prefixes
                if forbidden_roots:
                    params["forbidden_source_roots"] = forbidden_roots

                dry_run_result = command("promote_niagara_system_candidate", params)
                dependency_audit = dry_run_result.get("preflight_dependency_audit")
                if not isinstance(dependency_audit, dict):
                    dependency_audit = {}

                entry.update(
                    {
                        "can_promote": bool(dry_run_result.get("can_promote")),
                        "promotion_blockers": dry_run_result.get("promotion_blockers", []),
                        "target_system_path": dry_run_result.get("target_system_path", target_path),
                        "target_package_name": dry_run_result.get("target_package_name", ""),
                        "target_filename": dry_run_result.get("target_filename", ""),
                        "source_package_dirty": dry_run_result.get("source_package_dirty"),
                        "preflight_dependency_audit": {
                            "dependency_count": dependency_audit.get("dependency_count"),
                            "has_forbidden_dependency": dependency_audit.get("has_forbidden_dependency"),
                            "forbidden_dependency_hit_count": dependency_audit.get("forbidden_dependency_hit_count"),
                            "has_temp_dependency": dependency_audit.get("has_temp_dependency"),
                            "temp_dependency_hit_count": dependency_audit.get("temp_dependency_hit_count"),
                        },
                        "dry_run_result": dry_run_result,
                    }
                )
            except Exception as exc:
                entry["promotion_blockers"] = [str(exc)]
                entry["dry_run_error"] = str(exc)
                dry_run_bridge_failure_fields.update(
                    _niagara_bridge_failure_fields(exc)
                )
                if (
                    last_promotion_response_candidate_paths
                    and not last_promotion_response_validated
                ):
                    entry["response_state_unknown"] = True
                    dry_run_recovery_targets.update(
                        last_promotion_response_candidate_paths
                    )
                    dry_run_untrusted_response_paths.update(
                        last_promotion_untrusted_response_paths
                    )
            entries.append(entry)
            if dry_run_recovery_targets:
                break

        if dry_run_recovery_targets:
            return _niagara_promotion_state_unknown_error(
                "Niagara promotion manifest dry-run response was not strictly validated; "
                "read-only and write state are unknown. Do not retry before exact-package recovery.",
                error_code="niagara_promotion_manifest_dry_run_state_unknown",
                recovery_targets=dry_run_recovery_targets,
                untrusted_response_paths=dry_run_untrusted_response_paths,
                all_can_promote=False,
                entries=entries,
                dry_run_requested=True,
                dry_run_response_validated=False,
                read_only_state_unknown=True,
                writes_assets_state_unknown=True,
                manifest_written=False,
                **dry_run_bridge_failure_fields,
            )

        all_can_promote = bool(entries) and all(bool(entry.get("can_promote")) for entry in entries)
        manifest: Dict[str, Any] = {
            "success": True,
            "message": f"Planned {len(entries)} Niagara promotion candidate(s); all_can_promote={all_can_promote}",
            "manifest_version": 1,
            "created_at_utc": created_at_utc,
            "read_only": True,
            "dry_run_only": True,
            "writes_assets": False,
            "target_root": normalized_target_root,
            "allow_non_temp_source": allow_non_temp_source,
            "allow_temp_target": allow_temp_target,
            "allow_temp_dependencies": allow_temp_dependencies,
            "forbidden_dependency_prefixes": forbidden_prefixes,
            "forbidden_source_roots": forbidden_roots,
            "source_count": len(sources),
            "all_can_promote": all_can_promote,
            "entries": entries,
        }
        manifest["confirmation_token"] = _promotion_manifest_confirmation_token(manifest)

        if write_manifest:
            manifest_dir = _promotion_manifest_dir()
            manifest_dir.mkdir(parents=True, exist_ok=True)
            fallback_name = "niagara_promotion_" + datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
            safe_name = _sanitize_unreal_asset_name(manifest_name, fallback=fallback_name)
            manifest_path = manifest_dir / f"{safe_name}.json"
            manifest["manifest_path"] = str(manifest_path)
            manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2, default=str), encoding="utf-8")

        return manifest
    except Exception as exc:
        logger.error("Error planning Niagara promotion manifest: %s", exc)
        return _niagara_error(
            f"Error planning Niagara promotion manifest: {exc}",
            all_can_promote=False,
            cause=exc,
        )


def execute_niagara_promotion_manifest_impl(
    manifest: Dict[str, Any] | str | None = None,
    manifest_path: str = "",
    confirmation_token: str = "",
    require_confirmation: bool = True,
    save: bool = True,
    request_compile: bool = True,
    write_execution_report: bool = False,
    execution_report_name: str = "",
    timeout_seconds: float = 180.0,
    command_sender: Callable[[str, Dict[str, Any], float], Dict[str, Any]] | None = None,
) -> Dict[str, Any]:
    """Revalidate and execute a Niagara promotion manifest after explicit confirmation."""

    last_promotion_response_candidate_paths: set[str] = set()
    last_promotion_untrusted_response_paths: set[str] = set()
    last_promotion_response_validated = False

    def command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        nonlocal last_promotion_response_candidate_paths
        nonlocal last_promotion_untrusted_response_paths
        nonlocal last_promotion_response_validated
        if command_name == "promote_niagara_system_candidate":
            last_promotion_response_candidate_paths, last_promotion_untrusted_response_paths = (
                _collect_niagara_promotion_response_paths(
                    None,
                    str(params.get("target_system_path") or ""),
                )
            )
            last_promotion_response_validated = False
        if command_sender is not None:
            response = command_sender(command_name, params, timeout_seconds)
        else:
            response = send_niagara_command(command_name, params, timeout_seconds=timeout_seconds)
        if command_name == "promote_niagara_system_candidate":
            response_candidates, response_untrusted = (
                _collect_niagara_promotion_response_paths(
                    response,
                    str(params.get("target_system_path") or ""),
                )
            )
            last_promotion_response_candidate_paths.update(response_candidates)
            last_promotion_untrusted_response_paths.update(response_untrusted)
            validated_result = _unwrap_niagara_promotion_response(
                response,
                expected_source_system_path=str(
                    params.get("source_system_path") or ""
                ),
                expected_target_system_path=str(
                    params.get("target_system_path") or ""
                ),
                expected_dry_run=params.get("dry_run") is True,
                expected_save=params.get("save") is True,
                expected_request_compile=params.get("request_compile") is True,
                expected_allow_non_temp_source=params.get("allow_non_temp_source") is True,
                expected_allow_temp_target=params.get("allow_temp_target") is True,
                expected_allow_temp_dependencies=params.get("allow_temp_dependencies") is True,
                expected_overwrite_existing=params.get("overwrite_existing") is True,
                expected_forbidden_dependency_prefixes=_coerce_string_list(
                    params.get("forbidden_dependency_prefixes"),
                    "forbidden_dependency_prefixes",
                ),
                expected_forbidden_source_roots=_coerce_string_list(
                    params.get("forbidden_source_roots"),
                    "forbidden_source_roots",
                ),
            )
            last_promotion_response_validated = True
            return validated_result
        return _unwrap_niagara_response(command_name, response)

    try:
        if require_confirmation is not True:
            return _niagara_error(
                "Niagara promotion confirmation cannot be disabled",
                error_code="niagara_promotion_confirmation_cannot_be_disabled",
                executed=False,
                dispatch_attempted=False,
            )
        if type(save) is not bool or type(request_compile) is not bool:
            return _niagara_error(
                "save and request_compile must be bool",
                error_code="niagara_promotion_manifest_invalid_execution_options",
                executed=False,
                dispatch_attempted=False,
            )
        loaded_manifest = _load_promotion_manifest(manifest, manifest_path)
        try:
            expected_token = _promotion_manifest_confirmation_token(loaded_manifest)
        except Exception as exc:
            return _niagara_error(
                f"Invalid Niagara promotion manifest: {exc}",
                error_code="niagara_promotion_manifest_invalid_policy",
                executed=False,
                dispatch_attempted=False,
            )
        if require_confirmation and str(confirmation_token or "").strip() != expected_token:
            return _niagara_error(
                "confirmation_token mismatch; no asset mutation performed",
                error_code="niagara_promotion_confirmation_token_mismatch",
                executed=False,
                expected_confirmation_token=expected_token,
                provided_confirmation_token=confirmation_token,
            )

        entries = loaded_manifest.get("entries")
        if not isinstance(entries, list) or not entries:
            return _niagara_error(
                "Manifest has no promotion entries",
                error_code="niagara_promotion_manifest_empty",
                executed=False,
                confirmation_token=expected_token,
            )
        if loaded_manifest.get("all_can_promote") is not True:
            return _niagara_error(
                "Manifest all_can_promote is not true; rerun plan_niagara_promotion_manifest",
                error_code="niagara_promotion_manifest_not_promotable",
                executed=False,
                confirmation_token=expected_token,
            )

        if not save:
            durable_targets = sorted(
                {
                    _normalize_game_package_path(
                        str(entry.get("target_system_path") or "")
                    )
                    for entry in entries
                    if isinstance(entry, dict)
                    and not _is_temp_game_path(
                        _normalize_game_package_path(
                            str(entry.get("target_system_path") or "")
                        )
                    )
                }
            )
            if durable_targets:
                return _niagara_error(
                    "Non-temporary promotion targets require save=true",
                    error_code="niagara_promotion_manifest_unsaved_durable_target",
                    executed=False,
                    dispatch_attempted=False,
                    durable_target_paths=durable_targets,
                    confirmation_token=expected_token,
                )

        allow_non_temp_source = loaded_manifest["allow_non_temp_source"]
        allow_temp_target = loaded_manifest["allow_temp_target"]
        allow_temp_dependencies = loaded_manifest["allow_temp_dependencies"]
        forbidden_prefixes = _coerce_string_list(
            loaded_manifest.get("forbidden_dependency_prefixes"),
            "forbidden_dependency_prefixes",
        )
        forbidden_roots = _coerce_string_list(
            loaded_manifest.get("forbidden_source_roots"),
            "forbidden_source_roots",
        )

        revalidated_entries: list[Dict[str, Any]] = []
        validation_errors: list[Dict[str, Any]] = []
        revalidation_recovery_targets: set[str] = set()
        revalidation_untrusted_response_paths: set[str] = set()
        revalidation_bridge_failure_fields: Dict[str, Any] = {}
        seen_targets: set[str] = set()
        for index, raw_entry in enumerate(entries):
            if not isinstance(raw_entry, dict):
                validation_errors.append({"index": index, "errors": ["Manifest entry is not an object"]})
                continue
            entry_errors: list[str] = []
            try:
                source_path = _normalize_game_package_path(str(raw_entry.get("source_system_path") or ""))
                target_path = _normalize_game_package_path(str(raw_entry.get("target_system_path") or ""))
                if source_path == target_path:
                    entry_errors.append("source_system_path and target_system_path must differ")
                if target_path in seen_targets:
                    entry_errors.append("Duplicate target_system_path in promotion manifest")
                if _is_temp_game_path(target_path) and not allow_temp_target:
                    entry_errors.append("target_system_path under /Game/_MCP_Temp requires allow_temp_target=true")
                if _has_forbidden_mcp_reference_marker(source_path):
                    entry_errors.append("source_system_path contains an MCP-only reference marker")
                if _has_forbidden_mcp_reference_marker(target_path):
                    entry_errors.append("target_system_path contains an MCP-only reference marker")
                seen_targets.add(target_path)

                params: Dict[str, Any] = {
                    "source_system_path": source_path,
                    "target_system_path": target_path,
                    "dry_run": True,
                    "allow_non_temp_source": allow_non_temp_source,
                    "allow_temp_target": allow_temp_target,
                    "allow_temp_dependencies": allow_temp_dependencies,
                    "overwrite_existing": False,
                    # Revalidate the exact save/compile policy that execution
                    # will dispatch immediately below.
                    "save": save,
                    "request_compile": request_compile,
                }
                if forbidden_prefixes:
                    params["forbidden_dependency_prefixes"] = forbidden_prefixes
                if forbidden_roots:
                    params["forbidden_source_roots"] = forbidden_roots

                dry_run_result = command("promote_niagara_system_candidate", params)
                dry_run_target = _normalize_game_package_path(str(dry_run_result.get("target_system_path") or target_path))
                if dry_run_target != target_path:
                    entry_errors.append(f"Revalidated target changed from {target_path} to {dry_run_target}")
                if not dry_run_result.get("can_promote"):
                    blockers = dry_run_result.get("promotion_blockers") or []
                    entry_errors.append(f"Revalidation cannot promote: {blockers}")

                revalidated_entries.append(
                    {
                        "index": index,
                        "source_system_path": source_path,
                        "target_system_path": target_path,
                        "can_promote": bool(dry_run_result.get("can_promote")),
                        "promotion_blockers": dry_run_result.get("promotion_blockers", []),
                        "dry_run_result": dry_run_result,
                    }
                )
            except Exception as exc:
                entry_errors.append(str(exc))
                revalidation_bridge_failure_fields.update(
                    _niagara_bridge_failure_fields(exc)
                )
                if (
                    last_promotion_response_candidate_paths
                    and not last_promotion_response_validated
                ):
                    revalidation_recovery_targets.update(
                        last_promotion_response_candidate_paths
                    )
                    revalidation_untrusted_response_paths.update(
                        last_promotion_untrusted_response_paths
                    )
            if entry_errors:
                validation_errors.append({"index": index, "errors": entry_errors})
            if revalidation_recovery_targets:
                break

        if revalidation_recovery_targets:
            return _niagara_promotion_state_unknown_error(
                "Niagara promotion manifest revalidation dry-run was not strictly validated; "
                "the no-mutation claim cannot be made. Do not retry before exact-package recovery.",
                error_code="niagara_promotion_manifest_revalidation_state_unknown",
                recovery_targets=revalidation_recovery_targets,
                untrusted_response_paths=revalidation_untrusted_response_paths,
                executed=False,
                confirmation_token=expected_token,
                validation_errors=validation_errors,
                revalidated_entries=revalidated_entries,
                dry_run_requested=True,
                dry_run_response_validated=False,
                read_only_state_unknown=True,
                writes_assets_state_unknown=True,
                **revalidation_bridge_failure_fields,
            )

        if validation_errors:
            return _niagara_error(
                "Manifest revalidation failed; no asset mutation performed",
                error_code="niagara_promotion_manifest_revalidation_failed",
                executed=False,
                confirmation_token=expected_token,
                validation_errors=validation_errors,
                revalidated_entries=revalidated_entries,
            )

        promoted_entries: list[Dict[str, Any]] = []
        attempted_entries: list[Dict[str, Any]] = []
        execution_errors: list[Dict[str, Any]] = []
        mutation_recovery_targets: list[str] = []
        mutation_untrusted_response_paths: list[str] = []
        mutation_bridge_failure_fields: Dict[str, Any] = {}
        for entry in revalidated_entries:
            params = {
                "source_system_path": entry["source_system_path"],
                "target_system_path": entry["target_system_path"],
                "dry_run": False,
                "allow_non_temp_source": allow_non_temp_source,
                "allow_temp_target": allow_temp_target,
                "allow_temp_dependencies": allow_temp_dependencies,
                "overwrite_existing": False,
                "save": save,
                "request_compile": request_compile,
            }
            if forbidden_prefixes:
                params["forbidden_dependency_prefixes"] = forbidden_prefixes
            if forbidden_roots:
                params["forbidden_source_roots"] = forbidden_roots
            try:
                promotion_result = command("promote_niagara_system_candidate", params)
                attempt = {
                    "index": entry["index"],
                    "source_system_path": entry["source_system_path"],
                    "target_system_path": entry["target_system_path"],
                    "dispatch_attempted": True,
                    "response_state_unknown": False,
                    "promoted": bool(promotion_result.get("promoted")),
                    "saved": promotion_result.get("saved"),
                    "post_promotion_audit_pass": promotion_result.get(
                        "post_promotion_audit_pass"
                    ),
                    "promotion_result": promotion_result,
                }
                attempted_entries.append(attempt)
                if not promotion_result.get("promoted"):
                    execution_errors.append(
                        {
                            "index": entry["index"],
                            "errors": [f"Promotion command did not promote: {promotion_result.get('promotion_blockers')}"],
                        }
                    )
                    break
                promoted_entries.append(attempt)
            except Exception as exc:
                mutation_bridge_failure_fields.update(
                    _niagara_bridge_failure_fields(exc)
                )
                execution_errors.append({"index": entry["index"], "errors": [str(exc)]})
                attempted_entries.append(
                    {
                        "index": entry["index"],
                        "source_system_path": entry["source_system_path"],
                        "target_system_path": entry["target_system_path"],
                        "dispatch_attempted": True,
                        "response_state_unknown": True,
                        "promoted": None,
                    }
                )
                mutation_recovery_targets.extend(
                    sorted(last_promotion_response_candidate_paths)
                    or [entry["target_system_path"]]
                )
                mutation_untrusted_response_paths.extend(
                    sorted(last_promotion_untrusted_response_paths)
                )
                break

        success = not execution_errors and len(promoted_entries) == len(revalidated_entries)
        report: Dict[str, Any] = {
            "success": success,
            "message": (
                f"Executed {len(promoted_entries)} Niagara promotion entries"
                if success
                else "Niagara promotion execution failed or was partial"
            ),
            "executed": bool(promoted_entries),
            "confirmation_token": expected_token,
            "source_manifest_path": manifest_path,
            "entry_count": len(entries),
            "attempted_count": len(attempted_entries),
            "promoted_count": len(promoted_entries),
            "save": save,
            "request_compile": request_compile,
            "allow_non_temp_source": allow_non_temp_source,
            "allow_temp_target": allow_temp_target,
            "allow_temp_dependencies": allow_temp_dependencies,
            "revalidated_entries": revalidated_entries,
            "attempted_entries": attempted_entries,
            "promoted_entries": promoted_entries,
            "execution_errors": execution_errors,
        }
        if promoted_entries and not success:
            report.update(
                {
                    "do_not_retry": True,
                    "partial_manifest_execution": True,
                    "confirmed_promoted_paths": [
                        str(item.get("target_system_path") or "")
                        for item in promoted_entries
                    ],
                    "recovery_policy": (
                        "manual_reconcile_partial_manifest_before_retry"
                    ),
                }
            )
        if mutation_recovery_targets:
            recovery_targets = sorted(set(mutation_recovery_targets))
            temp_cleanup_candidates = [
                path for path in recovery_targets if _is_temp_game_path(path)
            ]
            manual_review_candidates = [
                path for path in recovery_targets if not _is_temp_game_path(path)
            ]
            pre_close_observed_policy = (
                "manual_reconcile_partial_manifest_and_verify_unknown_targets_before_retry"
                if promoted_entries
                else "reload_and_inspect_exact_targets_before_retry"
            )
            post_close_policy = (
                "rehash_then_reconcile_partial_manifest_and_unknown_targets"
                if promoted_entries
                else "rehash_then_choose_exact_recovery"
            )
            report.update(
                {
                    "mutation_state_unknown": True,
                    "save_state_unknown": True,
                    "requires_package_reload": True,
                    "cleanup_required_unknown": True,
                    "do_not_retry": True,
                    "command_may_still_be_in_flight": True,
                    "requires_editor_close_before_disk_verification": True,
                    "do_not_reload_or_trust_disk_until_editor_closed": True,
                    "recovery_policy": (
                        "close_editor_then_verify_exact_disk_before_recovery"
                    ),
                    "post_editor_close_recovery_policy": post_close_policy,
                    "pre_close_observed_recovery_policy": (
                        pre_close_observed_policy
                    ),
                    "recovery_steps": [
                        "close_editor_and_confirm_process_exit",
                        "verify_exact_promotion_target_disk_state",
                        "choose_exact_recovery_from_post_close_hashes",
                    ],
                    "recovery_action": (
                        "Close the editor and confirm the process has exited before "
                        "trusting, reloading, inspecting, or cleaning any target. Then "
                        "verify every exact target and reconcile confirmed promotions "
                        "before any retry."
                    ),
                    "reload_required_packages": recovery_targets,
                    "cleanup_candidate_paths": temp_cleanup_candidates,
                    "manual_review_candidate_paths": manual_review_candidates,
                    "observed_untrusted_response_paths": sorted(
                        set(mutation_untrusted_response_paths)
                    ),
                }
            )
            report.update(mutation_bridge_failure_fields)

        if write_execution_report:
            try:
                execution_dir = _promotion_manifest_dir() / "Executions"
                execution_dir.mkdir(parents=True, exist_ok=True)
                fallback_name = "niagara_promotion_execution_" + datetime.now(
                    timezone.utc
                ).strftime("%Y%m%d_%H%M%S")
                safe_name = _sanitize_unreal_asset_name(
                    execution_report_name,
                    fallback=fallback_name,
                )
                report_path = execution_dir / f"{safe_name}.json"
                report["execution_report_path"] = str(report_path)
                report_path.write_text(
                    json.dumps(report, ensure_ascii=False, indent=2, default=str),
                    encoding="utf-8",
                )
            except Exception as exc:
                report.update(
                    {
                        "success": False,
                        "message": (
                            "Niagara promotion execution completed, but its execution "
                            "report could not be written"
                        ),
                        "do_not_retry": bool(
                            promoted_entries or mutation_recovery_targets
                        ),
                        "execution_succeeded_before_report_write_failure": success,
                        "execution_report_error": {
                            "type": type(exc).__name__,
                            "message": str(exc),
                        },
                        "confirmed_promoted_paths": [
                            str(item.get("target_system_path") or "")
                            for item in promoted_entries
                        ],
                    }
                )

        return report
    except Exception as exc:
        logger.error("Error executing Niagara promotion manifest: %s", exc)
        return _niagara_error(
            f"Error executing Niagara promotion manifest: {exc}",
            executed=False,
            cause=exc,
        )


def register_niagara_tools(mcp: FastMCP):
    """Register Niagara inspection and limited authoring tools."""

    @mcp.tool()
    def analyze_niagara_system(
        ctx: Context,
        system_path: str,
        include_renderers: bool = True,
        include_user_parameters: bool = True,
        include_stack: bool = True,
        include_graph: bool = True,
        include_module_inputs: bool = True,
        include_compile_status: bool = True,
        include_pins: bool = False,
        include_links: bool = False,
        include_scratch_pads: bool = True,
        include_resolved_stack_inputs: bool = False,
        max_function_calls: int = 200,
        max_nodes_per_graph: int = 300,
        max_links_per_graph: int = 0,
        max_modules: int = 200,
        max_candidates_per_module: int = 24,
        max_resolved_inputs_per_module: int = 8,
        max_top_candidates: int = 80,
    ) -> Dict[str, Any]:
        """
        Aggregate read-only Niagara renderer, user parameter, stack, graph,
        module-input, and compile-health inspection into one response.

        Args:
            system_path: Niagara System package path or object path
            include_renderers: Include renderer/material inspection
            include_user_parameters: Include exposed User.* parameter inspection
            include_stack: Include stack function-call inspection
            include_graph: Include graph topology inspection
            include_module_inputs: Include module input candidate inspection
            include_compile_status: Include read-only compile status inspection
            include_pins: Include pin metadata where supported
            include_links: Include explicit graph links where supported
            include_scratch_pads: Include Scratch Pad graph/script data
            include_resolved_stack_inputs: Include resolved stack input values in module inspection
            max_function_calls: Maximum function calls per stack graph
            max_nodes_per_graph: Maximum graph nodes per graph
            max_links_per_graph: Maximum graph links per graph
            max_modules: Maximum modules to inspect
            max_candidates_per_module: Maximum candidates per module
            max_resolved_inputs_per_module: Maximum resolved inputs per module
            max_top_candidates: Maximum prioritized module input candidates
        """
        try:
            return send_niagara_command(
                "analyze_niagara_system",
                {
                    "system_path": system_path,
                    "include_renderers": include_renderers,
                    "include_user_parameters": include_user_parameters,
                    "include_stack": include_stack,
                    "include_graph": include_graph,
                    "include_module_inputs": include_module_inputs,
                    "include_compile_status": include_compile_status,
                    "include_pins": include_pins,
                    "include_links": include_links,
                    "include_scratch_pads": include_scratch_pads,
                    "include_resolved_stack_inputs": include_resolved_stack_inputs,
                    "max_function_calls": max_function_calls,
                    "max_nodes_per_graph": max_nodes_per_graph,
                    "max_links_per_graph": max_links_per_graph,
                    "max_modules": max_modules,
                    "max_candidates_per_module": max_candidates_per_module,
                    "max_resolved_inputs_per_module": max_resolved_inputs_per_module,
                    "max_top_candidates": max_top_candidates,
                },
            )
        except Exception as e:
            error_msg = f"Error analyzing Niagara system: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_factory_bootstrap_defaults(
        ctx: Context,
        kind: str = "all",
        create_default_system_nodes: bool = True,
        add_default_modules_and_renderers: bool = False,
        include_pins: bool = False,
        max_function_calls: int = 80,
    ) -> Dict[str, Any]:
        """
        Inspect UE Niagara factory bootstrap defaults using transient objects only.

        Args:
            kind: One of all, system, emitter, empty_emitter, or basic_sprite_emitter
            create_default_system_nodes: Whether system factory initialization creates default system graph nodes
            add_default_modules_and_renderers: Used only with kind="emitter"; creates the factory default sprite stack
            include_pins: Include graph pin metadata in returned module/function-call summaries
            max_function_calls: Maximum function-call nodes to include per graph summary
        """
        try:
            return send_niagara_command(
                "inspect_niagara_factory_bootstrap_defaults",
                {
                    "kind": kind,
                    "create_default_system_nodes": create_default_system_nodes,
                    "add_default_modules_and_renderers": add_default_modules_and_renderers,
                    "include_pins": include_pins,
                    "max_function_calls": max_function_calls,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara factory bootstrap defaults: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def audit_niagara_promotion_candidate(
        ctx: Context,
        system_path: str,
        proposed_target_path: str = "",
        forbidden_dependency_prefixes: list[str] | None = None,
        forbidden_source_roots: list[str] | None = None,
        allow_temp_target: bool = False,
        allow_temp_dependencies: bool = False,
        timeout_seconds: float = 60.0,
    ) -> Dict[str, Any]:
        """
        Read-only preflight for promoting a generated Niagara System outside temp.

        Args:
            system_path: Niagara System package/object path to audit
            proposed_target_path: Optional future destination package path; must be outside /Game/_MCP_Temp by default
            forbidden_dependency_prefixes: Additional dependency roots to reject
            forbidden_source_roots: Source/template roots to reject when the promoted asset must be independent
            allow_temp_target: Allow proposed_target_path under /Game/_MCP_Temp
            allow_temp_dependencies: Allow dependencies under /Game/_MCP_Temp
            timeout_seconds: Unreal bridge timeout
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "proposed_target_path": proposed_target_path,
                "allow_temp_target": allow_temp_target,
                "allow_temp_dependencies": allow_temp_dependencies,
            }
            if forbidden_dependency_prefixes is not None:
                params["forbidden_dependency_prefixes"] = forbidden_dependency_prefixes
            if forbidden_source_roots is not None:
                params["forbidden_source_roots"] = forbidden_source_roots
            return send_niagara_command(
                "audit_niagara_promotion_candidate",
                params,
                timeout_seconds=timeout_seconds,
            )
        except Exception as e:
            error_msg = f"Error auditing Niagara promotion candidate: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def promote_niagara_system_candidate(
        ctx: Context,
        source_system_path: str,
        target_system_path: str,
        dry_run: bool = True,
        forbidden_dependency_prefixes: list[str] | None = None,
        forbidden_source_roots: list[str] | None = None,
        allow_non_temp_source: bool = False,
        allow_temp_target: bool = False,
        allow_temp_dependencies: bool = False,
        overwrite_existing: bool = False,
        save: bool = True,
        request_compile: bool = True,
        timeout_seconds: float = 120.0,
    ) -> Dict[str, Any]:
        """
        Duplicate a generated Niagara System to a promotion target after dependency preflight.

        Args:
            source_system_path: Source Niagara System package/object path, generated temp only by default
            target_system_path: New destination package/object path, non-temp /Game by default
            dry_run: Validate only by default; pass false to duplicate and save
            forbidden_dependency_prefixes: Additional dependency roots to reject before/after promotion
            forbidden_source_roots: Source/template roots to reject when the promoted asset must be independent
            allow_non_temp_source: Allow source outside generated temp
            allow_temp_target: Allow target under /Game/_MCP_Temp, mainly for smoke tests
            allow_temp_dependencies: Allow dependencies under /Game/_MCP_Temp
            overwrite_existing: Reserved; currently refused
            save: Save promoted package when dry_run is false
            request_compile: Request Niagara compile on the promoted copy
            timeout_seconds: Unreal bridge timeout
        """
        return promote_niagara_system_candidate_impl(
            source_system_path=source_system_path,
            target_system_path=target_system_path,
            dry_run=dry_run,
            forbidden_dependency_prefixes=forbidden_dependency_prefixes,
            forbidden_source_roots=forbidden_source_roots,
            allow_non_temp_source=allow_non_temp_source,
            allow_temp_target=allow_temp_target,
            allow_temp_dependencies=allow_temp_dependencies,
            overwrite_existing=overwrite_existing,
            save=save,
            request_compile=request_compile,
            timeout_seconds=timeout_seconds,
            preserve_success_envelope=True,
        )

    @mcp.tool()
    def plan_niagara_promotion_manifest(
        ctx: Context,
        source_system_path: str = "",
        source_system_paths: list[str] | None = None,
        target_root: str = "/Game/MCPWeather/Generated",
        target_system_paths: list[str] | None = None,
        asset_name_prefix: str = "",
        asset_name_suffix: str = "",
        forbidden_dependency_prefixes: list[str] | None = None,
        forbidden_source_roots: list[str] | None = None,
        allow_non_temp_source: bool = False,
        allow_temp_target: bool = False,
        allow_temp_dependencies: bool = False,
        write_manifest: bool = False,
        manifest_name: str = "",
        timeout_seconds: float = 120.0,
    ) -> Dict[str, Any]:
        """
        Build a read-only dry-run manifest for Niagara System promotion candidates.

        Args:
            source_system_path: Single source Niagara System package/object path
            source_system_paths: Additional source Niagara System paths
            target_root: Non-temp /Game folder used when target_system_paths is omitted
            target_system_paths: Optional explicit destination package paths, one per source
            asset_name_prefix: Prefix for generated target asset names
            asset_name_suffix: Suffix for generated target asset names
            forbidden_dependency_prefixes: Additional dependency roots to reject
            forbidden_source_roots: Source/template roots to reject when promoted assets must be independent
            allow_non_temp_source: Allow source systems outside generated temp
            allow_temp_target: Allow promotion targets under /Game/_MCP_Temp
            allow_temp_dependencies: Allow dependencies under /Game/_MCP_Temp
            write_manifest: Also write the JSON manifest under Saved/MCP/NiagaraPromotionManifests
            manifest_name: Optional manifest file name when write_manifest is true
            timeout_seconds: Unreal bridge timeout for each dry-run promotion call
        """
        return plan_niagara_promotion_manifest_impl(
            source_system_path=source_system_path,
            source_system_paths=source_system_paths,
            target_root=target_root,
            target_system_paths=target_system_paths,
            asset_name_prefix=asset_name_prefix,
            asset_name_suffix=asset_name_suffix,
            forbidden_dependency_prefixes=forbidden_dependency_prefixes,
            forbidden_source_roots=forbidden_source_roots,
            allow_non_temp_source=allow_non_temp_source,
            allow_temp_target=allow_temp_target,
            allow_temp_dependencies=allow_temp_dependencies,
            write_manifest=write_manifest,
            manifest_name=manifest_name,
            timeout_seconds=timeout_seconds,
        )

    @mcp.tool()
    def execute_niagara_promotion_manifest(
        ctx: Context,
        manifest: Any | None = None,
        manifest_path: str = "",
        confirmation_token: str = "",
        require_confirmation: bool = True,
        save: bool = True,
        request_compile: bool = True,
        write_execution_report: bool = False,
        execution_report_name: str = "",
        timeout_seconds: float = 180.0,
    ) -> Dict[str, Any]:
        """
        Revalidate and execute a Niagara promotion manifest after explicit confirmation.

        Args:
            manifest: Manifest JSON object or JSON string returned by plan_niagara_promotion_manifest
            manifest_path: Optional Saved/MCP/NiagaraPromotionManifests JSON path when manifest is omitted
            confirmation_token: Exact token returned by the planner; required by default
            require_confirmation: Require confirmation_token match before any asset mutation
            save: Save promoted packages
            request_compile: Request Niagara compile on promoted copies
            write_execution_report: Write a JSON execution report under Saved/MCP/NiagaraPromotionManifests/Executions
            execution_report_name: Optional execution report file name
            timeout_seconds: Unreal bridge timeout for each dry-run and promotion command
        """
        return execute_niagara_promotion_manifest_impl(
            manifest=manifest,
            manifest_path=manifest_path,
            confirmation_token=confirmation_token,
            require_confirmation=require_confirmation,
            save=save,
            request_compile=request_compile,
            write_execution_report=write_execution_report,
            execution_report_name=execution_report_name,
            timeout_seconds=timeout_seconds,
        )

    @mcp.tool()
    def create_empty_niagara_system(
        ctx: Context,
        target_system_path: str,
        allow_non_temp_path: bool = False,
        overwrite_existing: bool = False,
        save: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Create a saved empty Niagara System using UE's Niagara System factory.

        Args:
            target_system_path: Target Niagara System package/object path, default-safe under /Game/_MCP_Temp/NiagaraGenerated
            allow_non_temp_path: Explicitly allow creation outside the generated temp Niagara root
            overwrite_existing: Reserved; currently refused by the C++ command
            save: Save the created package
            wait_for_completion: Wait for factory-requested Niagara compilation
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            return send_niagara_command(
                "create_empty_niagara_system",
                {
                    "target_system_path": target_system_path,
                    "allow_non_temp_path": allow_non_temp_path,
                    "overwrite_existing": overwrite_existing,
                    "save": save,
                    "wait_for_completion": wait_for_completion,
                    "timeout_seconds": timeout_seconds,
                    "poll_interval_seconds": poll_interval_seconds,
                },
            )
        except Exception as e:
            error_msg = f"Error creating empty Niagara system: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def create_empty_niagara_emitter(
        ctx: Context,
        target_emitter_path: str,
        add_default_modules_and_renderers: bool = False,
        allow_non_temp_path: bool = False,
        overwrite_existing: bool = False,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Create a saved Niagara Emitter asset using UE's Niagara Emitter factory.

        Args:
            target_emitter_path: Target Niagara Emitter package/object path, default-safe under /Game/_MCP_Temp/NiagaraGenerated
            add_default_modules_and_renderers: Add the editor default sprite renderer/modules instead of a truly empty emitter
            allow_non_temp_path: Explicitly allow creation outside the generated temp Niagara root
            overwrite_existing: Reserved; currently refused by the C++ command
            save: Save the created package
        """
        try:
            return send_niagara_command(
                "create_empty_niagara_emitter",
                {
                    "target_emitter_path": target_emitter_path,
                    "add_default_modules_and_renderers": add_default_modules_and_renderers,
                    "allow_non_temp_path": allow_non_temp_path,
                    "overwrite_existing": overwrite_existing,
                    "save": save,
                },
            )
        except Exception as e:
            error_msg = f"Error creating empty Niagara emitter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def create_basic_sprite_niagara_system(
        ctx: Context,
        target_system_path: str,
        emitter_name: str = "Emitter",
        material_path: str = "",
        allow_non_temp_path: bool = False,
        overwrite_existing: bool = False,
        save: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Create a saved Niagara System with one default sprite emitter.

        Args:
            target_system_path: Target Niagara System package/object path, default-safe under /Game/_MCP_Temp/NiagaraGenerated
            emitter_name: Name for the generated emitter handle/source copy
            material_path: Optional material to apply to the generated sprite renderer
            allow_non_temp_path: Explicitly allow creation outside the generated temp Niagara root
            overwrite_existing: Reserved; currently refused by the C++ command
            save: Save the created package
            wait_for_completion: Wait for factory-requested Niagara compilation
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            return send_niagara_command(
                "create_basic_sprite_niagara_system",
                {
                    "target_system_path": target_system_path,
                    "emitter_name": emitter_name,
                    "material_path": material_path,
                    "allow_non_temp_path": allow_non_temp_path,
                    "overwrite_existing": overwrite_existing,
                    "save": save,
                    "wait_for_completion": wait_for_completion,
                    "timeout_seconds": timeout_seconds,
                    "poll_interval_seconds": poll_interval_seconds,
                },
            )
        except Exception as e:
            error_msg = f"Error creating basic sprite Niagara system: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def create_sprite_niagara_preset_system(
        ctx: Context,
        target_system_path: str,
        preset: str = "basic",
        emitter_name: str = "Emitter",
        material_path: str = "",
        spawn_rate: float | None = None,
        velocity: list | None = None,
        alignment: str = "",
        facing_mode: str = "",
        sub_image_size: list | None = None,
        fixed_bounds_min: list | None = None,
        fixed_bounds_max: list | None = None,
        local_space: bool | None = None,
        sprite_size: list | None = None,
        lifetime: float | None = None,
        use_project_weather_materials: bool = False,
        add_spawn_box_location: bool | None = None,
        spawn_box_size: list | None = None,
        spawn_box_offset: list | None = None,
        add_weather_detail_modules: bool | None = None,
        allow_non_temp_path: bool = False,
        overwrite_existing: bool = False,
        save: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Create a saved Niagara System from a conservative factory sprite preset.

        Args:
            target_system_path: Target Niagara System package/object path, default-safe under /Game/_MCP_Temp/NiagaraGenerated
            preset: One of basic, rain, snow, dust, or sparks
            emitter_name: Name for the generated emitter handle/source copy
            material_path: Optional material to apply to the generated sprite renderer
            spawn_rate: Optional override for the preset SpawnRate module input
            velocity: Optional [x, y, z] override for the preset AddVelocity module input
            alignment: Optional sprite alignment override
            facing_mode: Optional sprite facing mode override
            sub_image_size: Optional [x, y] SubUV tile count override
            fixed_bounds_min: Optional fixed bounds minimum [x, y, z]
            fixed_bounds_max: Optional fixed bounds maximum [x, y, z]
            local_space: Optional local-space setting for the generated emitter
            sprite_size: Optional InitializeParticle sprite size [x, y]
            lifetime: Optional InitializeParticle particle lifetime in seconds
            use_project_weather_materials: Use project-owned /Game/MCPWeather rain/snow particle materials when material_path is empty
            add_spawn_box_location: Enable/disable BoxLocation insertion; rain/snow enable it by default
            spawn_box_size: Optional BoxLocation size [x, y, z]
            spawn_box_offset: Optional BoxLocation offset [x, y, z]
            add_weather_detail_modules: Enable/disable rain/snow force/noise/color/size detail modules
            allow_non_temp_path: Explicitly allow creation outside the generated temp Niagara root
            overwrite_existing: Reserved; currently refused by the C++ command
            save: Save the created package
            wait_for_completion: Wait for Niagara compilation after preset edits
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "target_system_path": target_system_path,
                "preset": preset,
                "emitter_name": emitter_name,
                "material_path": material_path,
                "allow_non_temp_path": allow_non_temp_path,
                "overwrite_existing": overwrite_existing,
                "save": save,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
                "use_project_weather_materials": use_project_weather_materials,
            }
            if spawn_rate is not None:
                params["spawn_rate"] = spawn_rate
            if velocity is not None:
                params["velocity"] = velocity
            if alignment:
                params["alignment"] = alignment
            if facing_mode:
                params["facing_mode"] = facing_mode
            if sub_image_size is not None:
                params["sub_image_size"] = sub_image_size
            if fixed_bounds_min is not None:
                params["fixed_bounds_min"] = fixed_bounds_min
            if fixed_bounds_max is not None:
                params["fixed_bounds_max"] = fixed_bounds_max
            if local_space is not None:
                params["local_space"] = local_space
            if sprite_size is not None:
                params["sprite_size"] = sprite_size
            if lifetime is not None:
                params["lifetime"] = lifetime
            if add_spawn_box_location is not None:
                params["add_spawn_box_location"] = add_spawn_box_location
            if spawn_box_size is not None:
                params["spawn_box_size"] = spawn_box_size
            if spawn_box_offset is not None:
                params["spawn_box_offset"] = spawn_box_offset
            if add_weather_detail_modules is not None:
                params["add_weather_detail_modules"] = add_weather_detail_modules
            return send_niagara_command("create_sprite_niagara_preset_system", params)
        except Exception as e:
            error_msg = f"Error creating sprite Niagara preset system: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def create_niagara_visible_scratch_pad_snow_seed(
        ctx: Context,
        target_system_path: str,
        preset: str = "snow",
        emitter_name: str = "Emitter",
        material_path: str = "",
        use_project_weather_materials: bool = True,
        scratch_pad_name: str = "MCP_VisibleSizeScratch",
        parameter_name: str = "Particles.SpriteSize",
        parameter_value: Any | None = None,
        user_parameter_name: str = "User.MCP_VisibleScratchPad_SpriteSize",
        user_parameter_default_value: Any | None = None,
        target_usage: str = "ParticleUpdateScript",
        spawn_rate: float | None = 180.0,
        velocity: list | None = None,
        sprite_size: list | None = None,
        lifetime: float | None = 10.0,
        add_spawn_box_location: bool | None = True,
        spawn_box_size: list | None = None,
        spawn_box_offset: list | None = None,
        add_weather_detail_modules: bool | None = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: int = 180,
    ) -> Dict[str, Any]:
        """
        Create a visible snow-like temp Niagara seed and add a Scratch Pad User sprite-size control.

        Args:
            target_system_path: Target Niagara System package path, generated temp by default
            preset: Sprite preset to create first; defaults to snow for stock weather detail modules
            emitter_name: Name for the generated emitter
            material_path: Optional explicit particle material
            use_project_weather_materials: Use project MCPWeather snow material when material_path is empty
            scratch_pad_name: Scratch Pad module name for the sprite-size control
            parameter_name: Namespaced Niagara parameter written by the Scratch Pad
            parameter_value: Fallback Vector2D pin default before the User link
            user_parameter_name: Vector2D User parameter used to drive the Scratch Pad write
            user_parameter_default_value: Vector2D default value for the User parameter
            target_usage: Stack usage for Scratch Pad insertion
            spawn_rate: Optional preset SpawnRate override
            velocity: Optional preset AddVelocity override
            sprite_size: Optional InitializeParticle SpriteSize override
            lifetime: Optional InitializeParticle Lifetime override
            add_spawn_box_location: Add preset BoxLocation module
            spawn_box_size: Optional BoxLocation size
            spawn_box_offset: Optional BoxLocation offset
            add_weather_detail_modules: Add preset force/noise/color/size detail modules
            allow_source_edit: Allow non-temp target creation and source edits
            save: Save after Scratch Pad insertion
            request_compile: Request compile after Scratch Pad insertion
            wait_for_completion: Wait for compile completion
            timeout_seconds: Per-command Unreal bridge timeout
        """
        return create_niagara_visible_scratch_pad_snow_seed_impl(
            target_system_path=target_system_path,
            preset=preset,
            emitter_name=emitter_name,
            material_path=material_path,
            use_project_weather_materials=use_project_weather_materials,
            scratch_pad_name=scratch_pad_name,
            parameter_name=parameter_name,
            parameter_value=parameter_value,
            user_parameter_name=user_parameter_name,
            user_parameter_default_value=user_parameter_default_value,
            target_usage=target_usage,
            spawn_rate=spawn_rate,
            velocity=velocity,
            sprite_size=sprite_size,
            lifetime=lifetime,
            add_spawn_box_location=add_spawn_box_location,
            spawn_box_size=spawn_box_size,
            spawn_box_offset=spawn_box_offset,
            add_weather_detail_modules=add_weather_detail_modules,
            allow_source_edit=allow_source_edit,
            save=save,
            request_compile=request_compile,
            wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,
        )

    @mcp.tool()
    def inspect_niagara_renderers(ctx: Context, system_path: str) -> Dict[str, Any]:
        """
        Inspect enabled renderer properties and materials on a Niagara System.

        Args:
            system_path: Niagara System package path or object path
        """
        try:
            return send_niagara_command("inspect_niagara_renderers", {"system_path": system_path})
        except Exception as e:
            error_msg = f"Error inspecting Niagara renderers: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_renderer_material(
        ctx: Context,
        system_path: str,
        material_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        renderer_index: int | None = None,
        material_slot_index: int = 0,
        allow_source_edit: bool = False,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Set a Niagara renderer material on a temp/generated system by default.

        Args:
            system_path: Niagara System package path or object path
            material_path: Material or Material Instance package path or object path
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            renderer_index: Optional target renderer index
            material_slot_index: Mesh renderer override slot index
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "material_path": material_path,
                "emitter_name": emitter_name,
                "material_slot_index": material_slot_index,
                "allow_source_edit": allow_source_edit,
                "save": save,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if renderer_index is not None:
                params["renderer_index"] = renderer_index
            return send_niagara_command("set_niagara_renderer_material", params)
        except Exception as e:
            error_msg = f"Error setting Niagara renderer material: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_sprite_renderer(
        ctx: Context,
        system_path: str,
        material_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        alignment: str = "Automatic",
        facing_mode: str = "FaceCamera",
        sub_image_size: list | None = None,
        skip_if_existing: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Add a Sprite renderer to a Niagara emitter.

        Args:
            system_path: Niagara System package path or object path
            material_path: Material or Material Instance package path or object path
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            alignment: Sprite alignment token
            facing_mode: Sprite facing mode token
            sub_image_size: Optional [x, y] SubUV tile count
            skip_if_existing: Return the existing matching material renderer instead of adding another
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "material_path": material_path,
                "emitter_name": emitter_name,
                "alignment": alignment,
                "facing_mode": facing_mode,
                "skip_if_existing": skip_if_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if sub_image_size is not None:
                params["sub_image_size"] = sub_image_size
            return send_niagara_command("add_niagara_sprite_renderer", params)
        except Exception as e:
            error_msg = f"Error adding Niagara sprite renderer: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_mesh_renderer(
        ctx: Context,
        system_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        meshes_binding_parameter: str = "User.MeshList",
        mesh_index_binding: str = "Particles.MeshIndex",
        mesh_paths: list[Any] | None = None,
        skip_if_existing: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Add a Mesh renderer to a Niagara emitter, optionally binding meshes to a User Mesh Array.

        Args:
            system_path: Niagara System package path or object path
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            meshes_binding_parameter: Mesh renderer Meshes Binding parameter, defaults to User.MeshList
            mesh_index_binding: Particle attribute used to select the mesh slot, defaults to Particles.MeshIndex
            mesh_paths: Optional StaticMesh path strings or mesh-entry objects used as renderer fallback meshes
            skip_if_existing: Return an existing matching MeshesBinding renderer instead of adding another
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "emitter_name": emitter_name,
                "meshes_binding_parameter": meshes_binding_parameter,
                "mesh_index_binding": mesh_index_binding,
                "skip_if_existing": skip_if_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if mesh_paths is not None:
                params["mesh_paths"] = mesh_paths
            return send_niagara_command("add_niagara_mesh_renderer", params)
        except Exception as e:
            error_msg = f"Error adding Niagara mesh renderer: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_renderer_enabled(
        ctx: Context,
        system_path: str,
        enabled: bool,
        emitter_index: int | None = None,
        emitter_name: str = "",
        renderer_index: int | None = None,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Enable or disable one Niagara renderer on an emitter.

        Args:
            system_path: Niagara System package path or object path
            enabled: Desired renderer enabled state
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            renderer_index: Optional renderer index
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "enabled": enabled,
                "emitter_name": emitter_name,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if renderer_index is not None:
                params["renderer_index"] = renderer_index
            return send_niagara_command("set_niagara_renderer_enabled", params)
        except Exception as e:
            error_msg = f"Error setting Niagara renderer enabled state: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def cleanup_niagara_renderers(
        ctx: Context,
        system_path: str,
        remove_renderer_indices: list[int] | None = None,
        clear_meshes_binding_renderer_indices: list[int] | None = None,
        expected_renderers: list[Dict[str, Any]] | None = None,
        expected_renderer_count: int | None = None,
        emitter_index: int | None = None,
        emitter_name: str = "",
        dry_run: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """Safely remove selected renderers and clear inert MeshesBinding values.

        Every target renderer index must have a matching entry in
        ``expected_renderers`` with ``renderer_index``, ``renderer_class``, and
        ``enabled``. A MeshesBinding clear target must also provide its current
        ``meshes_binding_parameter``. The command is a dry run by default and
        refuses stale indices, binding values, or unexpected renderer topology.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "remove_renderer_indices": remove_renderer_indices or [],
                "clear_meshes_binding_renderer_indices": clear_meshes_binding_renderer_indices or [],
                "expected_renderers": expected_renderers or [],
                "emitter_name": emitter_name,
                "dry_run": dry_run,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if expected_renderer_count is not None:
                params["expected_renderer_count"] = expected_renderer_count
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            return send_niagara_command("cleanup_niagara_renderers", params)
        except Exception as e:
            error_msg = f"Error cleaning Niagara renderers: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_emitter_enabled(
        ctx: Context,
        system_path: str,
        enabled: bool,
        emitter_index: int | None = None,
        emitter_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Enable or disable one Niagara emitter handle.

        Args:
            system_path: Niagara System package path or object path
            enabled: Desired emitter handle enabled state
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "enabled": enabled,
                "emitter_name": emitter_name,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            return send_niagara_command("set_niagara_emitter_enabled", params)
        except Exception as e:
            error_msg = f"Error setting Niagara emitter enabled state: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remove_niagara_emitter(
        ctx: Context,
        system_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Remove one Niagara emitter handle from a system.

        Args:
            system_path: Niagara System package path or object path
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "emitter_name": emitter_name,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            return send_niagara_command("remove_niagara_emitter", params)
        except Exception as e:
            error_msg = f"Error removing Niagara emitter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remove_niagara_modules(
        ctx: Context,
        system_path: str,
        module_names: list[str] | None = None,
        module_name_prefixes: list[str] | None = None,
        module_name_contains: list[str] | None = None,
        module_name: str = "",
        emitter_index: int | None = None,
        emitter_name: str = "",
        include_system_modules: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Remove Niagara stack modules by name through the native editor bridge.

        Args:
            system_path: Niagara System path.
            module_names: Exact module/function names to remove.
            module_name_prefixes: Remove modules whose function/node name starts with any prefix.
            module_name_contains: Remove modules whose function/node name contains any value.
            module_name: Single exact module/function name convenience parameter.
            emitter_index: Optional emitter index filter.
            emitter_name: Optional emitter display name filter.
            include_system_modules: Also scan SystemSpawn/SystemUpdate stack modules (no emitter filter).
            allow_source_edit: Permit edits outside /Game/_MCP_Temp.
            save: Save the system after removal.
            request_compile: Request Niagara compile after removal.
            wait_for_completion: Wait for compile completion.
            timeout_seconds: Compile wait timeout.
            poll_interval_seconds: Compile wait poll interval.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "include_system_modules": include_system_modules,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if module_names:
                params["module_names"] = module_names
            if module_name_prefixes:
                params["module_name_prefixes"] = module_name_prefixes
            if module_name_contains:
                params["module_name_contains"] = module_name_contains
            if module_name:
                params["module_name"] = module_name
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if emitter_name:
                params["emitter_name"] = emitter_name
            return send_niagara_command("remove_niagara_modules", params)
        except Exception as e:
            error_msg = f"Error removing Niagara modules: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remove_niagara_parameter_map_set_nodes(
        ctx: Context,
        system_path: str,
        parameter_pin_names: list[str] | None = None,
        parameter_pin_name: str = "",
        node_guid: str = "",
        emitter_index: int | None = None,
        emitter_name: str = "",
        dry_run: bool = True,
        allow_multiple: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Remove emitter-graph Niagara ParameterMapSet nodes that only write selected pins.

        Args:
            system_path: Niagara System path.
            parameter_pin_names: Exact ParameterMapSet input pin names to match.
            parameter_pin_name: Single exact ParameterMapSet input pin name convenience parameter.
            node_guid: Optional exact node GUID selector.
            emitter_index: Optional emitter index filter.
            emitter_name: Optional emitter display name filter.
            dry_run: Preflight only by default.
            allow_multiple: Permit removing more than one matched node.
            allow_source_edit: Permit edits outside /Game/_MCP_Temp.
            save: Save the system after removal.
            request_compile: Request Niagara compile after removal.
            wait_for_completion: Wait for compile completion.
            timeout_seconds: Compile wait timeout.
            poll_interval_seconds: Compile wait poll interval.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "node_guid": node_guid,
                "emitter_name": emitter_name,
                "dry_run": dry_run,
                "allow_multiple": allow_multiple,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if parameter_pin_names:
                params["parameter_pin_names"] = parameter_pin_names
            if parameter_pin_name:
                params["parameter_pin_name"] = parameter_pin_name
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            return send_niagara_command("remove_niagara_parameter_map_set_nodes", params)
        except Exception as e:
            error_msg = f"Error removing Niagara ParameterMapSet nodes: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remove_niagara_orphan_parameter_map_get_nodes(
        ctx: Context,
        system_path: str,
        parameter_pin_names: list[str] | None = None,
        parameter_pin_name: str = "",
        node_guid: str = "",
        emitter_index: int | None = None,
        emitter_name: str = "",
        dry_run: bool = True,
        allow_multiple: bool = False,
        only_if_unlinked: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Remove emitter-graph Niagara ParameterMapGet nodes for unused selected output pins.

        Args:
            system_path: Niagara System path.
            parameter_pin_names: Exact ParameterMapGet output pin names to match.
            parameter_pin_name: Single exact ParameterMapGet output pin name convenience parameter.
            node_guid: Optional exact node GUID selector.
            emitter_index: Optional emitter index filter.
            emitter_name: Optional emitter display name filter.
            dry_run: Preflight only by default.
            allow_multiple: Permit removing more than one matched node.
            only_if_unlinked: Must remain true; false is rejected to keep this command orphan-only.
            allow_source_edit: Permit edits outside /Game/_MCP_Temp.
            save: Save the system after removal.
            request_compile: Request Niagara compile after removal.
            wait_for_completion: Wait for compile completion.
            timeout_seconds: Compile wait timeout.
            poll_interval_seconds: Compile wait poll interval.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "node_guid": node_guid,
                "emitter_name": emitter_name,
                "dry_run": dry_run,
                "allow_multiple": allow_multiple,
                "only_if_unlinked": only_if_unlinked,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if parameter_pin_names:
                params["parameter_pin_names"] = parameter_pin_names
            if parameter_pin_name:
                params["parameter_pin_name"] = parameter_pin_name
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            return send_niagara_command("remove_niagara_orphan_parameter_map_get_nodes", params)
        except Exception as e:
            error_msg = f"Error removing Niagara orphan ParameterMapGet nodes: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remap_niagara_parameter_collection_references(
        ctx: Context,
        system_path: str,
        new_collection_path: str,
        old_collection_path: str = "",
        old_namespace: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        replace_object_references: bool = False,
    ) -> Dict[str, Any]:
        """
        Remap Niagara graph and script references from one NPC namespace to another.

        Args:
            system_path: Niagara System package path or object path
            new_collection_path: Replacement Niagara Parameter Collection path
            old_collection_path: Optional source Niagara Parameter Collection path
            old_namespace: Optional source NPC namespace when old_collection_path is not available
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
            replace_object_references: Also run a broad UObject reference archive pass
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "new_collection_path": new_collection_path,
                "old_collection_path": old_collection_path,
                "old_namespace": old_namespace,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "replace_object_references": replace_object_references,
            }
            return send_niagara_command("remap_niagara_parameter_collection_references", params)
        except Exception as e:
            error_msg = f"Error remapping Niagara parameter collection references: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remap_niagara_script_parameter_collection_references(
        ctx: Context,
        new_collection_path: str,
        old_collection_path: str = "",
        old_namespace: str = "",
        script_path: str = "",
        script_paths: list | None = None,
        root_path: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
    ) -> Dict[str, Any]:
        """
        Remap standalone Niagara Script assets from one NPC namespace to another.

        Args:
            new_collection_path: Replacement Niagara Parameter Collection path
            old_collection_path: Optional source Niagara Parameter Collection path
            old_namespace: Optional source NPC namespace when old_collection_path is not available
            script_path: Optional single Niagara Script path
            script_paths: Optional list of Niagara Script paths
            root_path: Optional package root to scan recursively for Niagara Scripts
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save modified scripts after editing
            request_compile: Request script compile after editing
            wait_for_completion: Wait for script compilation before saving
        """
        try:
            params: Dict[str, Any] = {
                "new_collection_path": new_collection_path,
                "old_collection_path": old_collection_path,
                "old_namespace": old_namespace,
                "script_path": script_path,
                "root_path": root_path,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
            }
            if script_paths is not None:
                params["script_paths"] = script_paths
            return send_niagara_command("remap_niagara_script_parameter_collection_references", params)
        except Exception as e:
            error_msg = f"Error remapping Niagara script parameter collection references: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def recompile_niagara_scripts_in_root(
        ctx: Context,
        root_path: str = "",
        script_paths: list[str] | None = None,
        allow_source_compile: bool = False,
        force: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Request compile for Niagara Script assets under a root or explicit path list.

        Args:
            root_path: Optional /Game root to scan recursively for NiagaraScript assets
            script_paths: Optional explicit NiagaraScript package/object paths
            allow_source_compile: Allow compile requests outside /Game/_MCP_Temp/NiagaraGenerated
            force: Force script compile
            save: Save scripts after requesting compile
        """
        try:
            params: Dict[str, Any] = {
                "root_path": root_path,
                "allow_source_compile": allow_source_compile,
                "force": force,
                "save": save,
            }
            if script_paths is not None:
                params["script_paths"] = script_paths
            return send_niagara_command("recompile_niagara_scripts_in_root", params)
        except Exception as e:
            error_msg = f"Error recompiling Niagara scripts: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def repair_niagara_parameter_collection(
        ctx: Context,
        collection_path: str,
        allow_source_edit: bool = False,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Repair a Niagara Parameter Collection default instance store.

        Args:
            collection_path: Niagara Parameter Collection package/object path
            allow_source_edit: Allow edits outside /Game/_MCP_Temp/NiagaraGenerated
            save: Save the collection after repair
        """
        try:
            return send_niagara_command(
                "repair_niagara_parameter_collection",
                {
                    "collection_path": collection_path,
                    "allow_source_edit": allow_source_edit,
                    "save": save,
                },
            )
        except Exception as e:
            error_msg = f"Error repairing Niagara parameter collection: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_parameter_collection_values(
        ctx: Context,
        collection_path: str,
        parameter_names: list[str] | None = None,
        namespace: str = "",
        max_parameters: int = 256,
    ) -> Dict[str, Any]:
        """
        Read default values from a Niagara Parameter Collection default instance.

        Args:
            collection_path: Niagara Parameter Collection package/object path
            parameter_names: Optional parameter name filters
            namespace: Optional namespace used to match short parameter names
            max_parameters: Maximum returned parameters
        """
        try:
            params: Dict[str, Any] = {
                "collection_path": collection_path,
                "namespace": namespace,
                "max_parameters": max_parameters,
            }
            if parameter_names is not None:
                params["parameter_names"] = parameter_names
            return send_niagara_command("inspect_niagara_parameter_collection_values", params)
        except Exception as e:
            error_msg = f"Error inspecting Niagara parameter collection values: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_parameter_collection_values(
        ctx: Context,
        collection_path: str,
        values: Dict[str, Any],
        namespace: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Set default values on a Niagara Parameter Collection default instance.

        Args:
            collection_path: Niagara Parameter Collection package/object path
            values: Parameter name to JSON value mapping
            namespace: Optional namespace used to match short parameter names
            allow_source_edit: Allow edits outside /Game/_MCP_Temp/NiagaraGenerated
            save: Save the collection after editing
        """
        try:
            return send_niagara_command(
                "set_niagara_parameter_collection_values",
                {
                    "collection_path": collection_path,
                    "values": values,
                    "namespace": namespace,
                    "allow_source_edit": allow_source_edit,
                    "save": save,
                },
            )
        except Exception as e:
            error_msg = f"Error setting Niagara parameter collection values: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_user_parameters(ctx: Context, system_path: str) -> Dict[str, Any]:
        """
        Inspect exposed User parameters on a Niagara System.

        Args:
            system_path: Niagara System package path or object path
        """
        try:
            return send_niagara_command("inspect_niagara_user_parameters", {"system_path": system_path})
        except Exception as e:
            error_msg = f"Error inspecting Niagara user parameters: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_user_parameter(
        ctx: Context,
        system_path: str,
        parameter_name: str,
        value: Any,
        allow_source_edit: bool = False,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Set a supported exposed Niagara User parameter value.

        Args:
            system_path: Niagara System package path or object path
            parameter_name: Parameter name, with or without the User. prefix
            value: New JSON value. Vector/color values use arrays or objects.
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
        """
        try:
            return send_niagara_command(
                "set_niagara_user_parameter",
                {
                    "system_path": system_path,
                    "parameter_name": parameter_name,
                    "value": value,
                    "allow_source_edit": allow_source_edit,
                    "save": save,
                },
            )
        except Exception as e:
            error_msg = f"Error setting Niagara user parameter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def upsert_niagara_user_parameters(
        ctx: Context,
        system_path: str,
        parameters: list[dict[str, Any]],
        continue_on_error: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Create or update exposed Niagara User parameters, including supported Array Data Interfaces.

        Args:
            system_path: Niagara System package path or object path
            parameters: Entries with name/parameter_name plus type/type_name, and optional value/default_value
            continue_on_error: Continue processing later entries after a failed entry
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
        """
        try:
            return send_niagara_command(
                "upsert_niagara_user_parameters",
                {
                    "system_path": system_path,
                    "parameters": parameters,
                    "continue_on_error": continue_on_error,
                    "allow_source_edit": allow_source_edit,
                    "save": save,
                    "request_compile": request_compile,
                },
            )
        except Exception as e:
            error_msg = f"Error upserting Niagara user parameters: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remove_niagara_user_parameters(
        ctx: Context,
        system_path: str,
        parameter_names: list[str] | None = None,
        parameter_name: str = "",
        dry_run: bool = True,
        remove_graph_references: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
        continue_on_error: bool = False,
    ) -> Dict[str, Any]:
        """
        Remove exposed Niagara User parameters by name.

        Args:
            system_path: Niagara System package path or object path
            parameter_names: Parameter names, with or without the User. prefix
            parameter_name: Optional single parameter name
            dry_run: Preview matched parameters without mutating; defaults true
            remove_graph_references: Also ask Niagara to reset downstream references for each parameter
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing; required for apply
            wait_for_completion: Wait for compile completion and post-compile absence readback; required for apply
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait polling interval
            continue_on_error: Continue after a missing or failed parameter
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "parameter_names": parameter_names or [],
                "parameter_name": parameter_name,
                "dry_run": dry_run,
                "remove_graph_references": remove_graph_references,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
                "continue_on_error": continue_on_error,
            }
            return send_niagara_command("remove_niagara_user_parameters", params)
        except Exception as e:
            error_msg = f"Error removing Niagara user parameters: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remove_niagara_unused_module_input_path(
        ctx: Context,
        system_path: str,
        scratch_pad_name: str,
        module_name: str,
        module_node_guid: str,
        input_name: str,
        linked_user_parameter_name: str,
        custom_hlsl_signature_name: str,
        expected_hlsl_identifier_absent: str,
        custom_hlsl_input_name: str = "",
        scratch_pad_script_path: str = "",
        expected_hlsl_body_hash: str = "",
        expected_custom_hlsl_node_guid: str = "",
        expected_override_node_guid: str = "",
        expected_input_type: str = "",
        expected_user_source_node_guid: str = "",
        expected_scratch_map_get_node_guids: list[str] | None = None,
        dry_run: bool = True,
        allow_source_edit: bool = False,
        save: bool = False,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Remove one proven, unused User -> Module -> Custom HLSL input path.

        Dry-run is mandatory as the first step. It returns the HLSL hash and
        exact node GUID contract that apply mode requires back unchanged.

        Args:
            system_path: Niagara System package path or object path
            scratch_pad_name: Exact system-owned Scratch Pad script name
            module_name: Exact emitter-stack module function name
            module_node_guid: Exact emitter-stack function-call node GUID
            input_name: Module input name, with or without the Module. prefix
            linked_user_parameter_name: Exact linked User parameter name
            custom_hlsl_signature_name: Exact Custom HLSL signature name
            expected_hlsl_identifier_absent: Exact Custom HLSL input name; that identifier must be absent from the HLSL body
            custom_hlsl_input_name: Exact Custom HLSL input pin; defaults to the short module input name
            scratch_pad_script_path: Optional exact system-owned Scratch Pad object path
            expected_hlsl_body_hash: Exact hash returned by dry-run; required for apply
            expected_custom_hlsl_node_guid: Exact GUID returned by dry-run; required for apply
            expected_override_node_guid: Exact override ParameterMapSet GUID returned by dry-run; required for apply
            expected_input_type: Exact Niagara input type returned by dry-run; required for apply
            expected_user_source_node_guid: Exact direct User ParameterMapGet GUID returned by dry-run; required for apply
            expected_scratch_map_get_node_guids: Exact sorted GUID set returned by dry-run; required for apply
            dry_run: Inspect and return the apply contract without mutation; defaults true
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save only after readback and compile gates pass; defaults false
            request_compile: Request Niagara compile after an applied mutation; required for apply
            wait_for_completion: Wait for compile completion; required for apply
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait polling interval
        """
        try:
            return send_niagara_command(
                "remove_niagara_unused_module_input_path",
                {
                    "system_path": system_path,
                    "scratch_pad_name": scratch_pad_name,
                    "scratch_pad_script_path": scratch_pad_script_path,
                    "module_name": module_name,
                    "module_node_guid": module_node_guid,
                    "input_name": input_name,
                    "linked_user_parameter_name": linked_user_parameter_name,
                    "custom_hlsl_signature_name": custom_hlsl_signature_name,
                    "custom_hlsl_input_name": custom_hlsl_input_name,
                    "expected_hlsl_identifier_absent": expected_hlsl_identifier_absent,
                    "expected_hlsl_body_hash": expected_hlsl_body_hash,
                    "expected_custom_hlsl_node_guid": expected_custom_hlsl_node_guid,
                    "expected_override_node_guid": expected_override_node_guid,
                    "expected_input_type": expected_input_type,
                    "expected_user_source_node_guid": expected_user_source_node_guid,
                    "expected_scratch_map_get_node_guids": expected_scratch_map_get_node_guids or [],
                    "dry_run": dry_run,
                    "allow_source_edit": allow_source_edit,
                    "save": save,
                    "request_compile": request_compile,
                    "wait_for_completion": wait_for_completion,
                    "timeout_seconds": timeout_seconds,
                    "poll_interval_seconds": poll_interval_seconds,
                },
            )
        except Exception as e:
            error_msg = f"Error removing Niagara unused module input path: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_stack(
        ctx: Context,
        system_path: str,
        include_pins: bool = False,
        max_function_calls: int = 200,
    ) -> Dict[str, Any]:
        """
        Inspect Niagara System, emitter, and scratch-pad stack function calls.

        Args:
            system_path: Niagara System package path or object path
            include_pins: Include function call pin metadata
            max_function_calls: Maximum function calls per script/graph
        """
        try:
            return send_niagara_command(
                "inspect_niagara_stack",
                {
                    "system_path": system_path,
                    "include_pins": include_pins,
                    "max_function_calls": max_function_calls,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara stack: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_graph(
        ctx: Context,
        system_path: str,
        include_pins: bool = True,
        include_links: bool = True,
        include_scratch_pads: bool = True,
        max_nodes_per_graph: int = 600,
        max_links_per_graph: int = 2000,
    ) -> Dict[str, Any]:
        """
        Inspect full Niagara graph nodes, pins, links, and scratch-pad graphs.

        Args:
            system_path: Niagara System package path or object path
            include_pins: Include pin metadata on each graph node
            include_links: Include explicit graph links and linked pin refs
            include_scratch_pads: Include system, emitter, and parent scratch pads
            max_nodes_per_graph: Maximum nodes returned per graph
            max_links_per_graph: Maximum links returned per graph
        """
        try:
            return send_niagara_command(
                "inspect_niagara_graph",
                {
                    "system_path": system_path,
                    "include_pins": include_pins,
                    "include_links": include_links,
                    "include_scratch_pads": include_scratch_pads,
                    "max_nodes_per_graph": max_nodes_per_graph,
                    "max_links_per_graph": max_links_per_graph,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara graph: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_graph_actions(
        ctx: Context,
        system_path: str,
        scratch_pad_name: str = "",
        scratch_pad_script_path: str = "",
        scratch_pad_script_index: int | None = None,
        safe_only: bool = True,
        search_text: str = "",
        max_actions: int = 600,
        include_canonical: bool = False,
        include_template_pins: bool = False,
    ) -> Dict[str, Any]:
        """
        Inspect UE 5.6 Niagara graph-menu actions for one system-owned Scratch Pad.

        Args:
            system_path: Niagara System package path or object path
            scratch_pad_name: Optional exact Scratch Pad script name selector
            scratch_pad_script_path: Optional exact private Scratch Pad object path selector
            scratch_pad_script_index: Optional system-owned Scratch Pad index selector
            safe_only: Return only native node classes admitted by the guarded recipe writer
            search_text: Optional case-insensitive action search filter
            max_actions: Maximum action records returned
            include_canonical: Include the canonical string used to derive each stable action id
            include_template_pins: Materialize transient action templates and include native pin metadata (first 200 results)
        """
        try:
            params: dict[str, Any] = {
                "system_path": system_path,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_script_path": scratch_pad_script_path,
                "safe_only": safe_only,
                "search_text": search_text,
                "max_actions": max_actions,
                "include_canonical": include_canonical,
                "include_template_pins": include_template_pins,
            }
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            return send_niagara_command("inspect_niagara_graph_actions", params)
        except Exception as e:
            error_msg = f"Error inspecting Niagara graph actions: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def apply_niagara_scratch_pad_graph_recipe(
        ctx: Context,
        system_path: str,
        custom_hlsl_node_guid: str,
        nodes: list[dict[str, Any]],
        links: list[dict[str, Any]],
        output_mappings: list[dict[str, Any]],
        scratch_pad_name: str = "",
        scratch_pad_script_path: str = "",
        scratch_pad_script_index: int | None = None,
        expected_hlsl_body_hash: str = "",
        dry_run: bool = True,
        remove_custom_hlsl: bool = True,
        save: bool = True,
        request_compile: bool = True,
        timeout_seconds: float = 30.0,
        poll_interval_seconds: float = 0.05,
    ) -> Dict[str, Any]:
        """
        Apply an allowlisted native-node recipe to one Custom HLSL node in a ghost system.

        Args:
            system_path: Target under /Game/_MCP_Temp/NiagaraGenerated only
            custom_hlsl_node_guid: Exact Custom HLSL node GUID from inspection
            nodes: Native graph-action nodes with node_id, action_id, optional x/y/defaults
            links: Source/target endpoint objects connecting created nodes or Custom HLSL inputs
            output_mappings: Mapping from every converted Custom HLSL output to its native source
            scratch_pad_name: Optional exact Scratch Pad script name selector
            scratch_pad_script_path: Optional exact private Scratch Pad object path selector
            scratch_pad_script_index: Optional system-owned Scratch Pad index selector
            expected_hlsl_body_hash: Required dry-run/readback hash when dry_run is false
            dry_run: Validate selectors and action ids without mutating; defaults true
            remove_custom_hlsl: Remove the converted node after pre-removal compile validation
            save: Save only after final compile, pending-ID binding, and dependency checks pass
            request_compile: Apply mode requires owning-system compile validation
            timeout_seconds: Maximum wait for each compile phase
            poll_interval_seconds: Niagara compilation polling interval

        Apply uses non-force owning-system compiles and proves that each caller's
        pending ComputedId becomes the post-compile CachedId/ComputedId. After any
        post-mutation failure it restores visible graph structure, keeps the ghost
        package dirty, and requires an exact package reload; Undo alone is unproven
        and it never reports a clean full rollback.
        """
        try:
            phase_timeout_seconds = _normalize_niagara_graph_recipe_phase_timeout(
                timeout_seconds
            )
            transport_timeout_seconds = _niagara_graph_recipe_transport_timeout(
                phase_timeout_seconds
            )
            params: dict[str, Any] = {
                "system_path": system_path,
                "custom_hlsl_node_guid": custom_hlsl_node_guid,
                "nodes": nodes,
                "links": links,
                "output_mappings": output_mappings,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_script_path": scratch_pad_script_path,
                "expected_hlsl_body_hash": expected_hlsl_body_hash,
                "dry_run": dry_run,
                "remove_custom_hlsl": remove_custom_hlsl,
                "save": save,
                "request_compile": request_compile,
                "timeout_seconds": phase_timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
        except (TypeError, ValueError) as e:
            error_msg = f"Invalid Niagara Scratch Pad graph recipe timeout: {e}"
            logger.error(error_msg)
            return _niagara_error(
                error_msg,
                error_code="niagara_graph_recipe_invalid_timeout",
                target_system_path=str(system_path or ""),
            )

        try:
            response = send_niagara_command(
                "apply_niagara_scratch_pad_graph_recipe",
                params,
                timeout_seconds=transport_timeout_seconds,
            )
        except Exception as e:
            logger.error("Niagara graph recipe transport raised an exception: %s", e)
            return _niagara_graph_recipe_unknown_state_error(
                system_path,
                transport_error=e,
                transport_timeout_seconds=transport_timeout_seconds,
            )

        if _is_niagara_graph_recipe_transport_failure(
            response,
            expected_dry_run=dry_run is True,
            expected_save=save is True,
            expected_system_path=system_path,
        ):
            logger.error("Niagara graph recipe transport returned no structured result: %s", response)
            return _niagara_graph_recipe_unknown_state_error(
                system_path,
                transport_error=response,
                transport_timeout_seconds=transport_timeout_seconds,
            )

        return response

    @mcp.tool()
    def inspect_niagara_custom_hlsl_nodes(
        ctx: Context,
        system_path: str,
        node_guid: str = "",
        signature_name: str = "",
        contains_text: str = "",
        max_hlsl_chars: int = 4000,
    ) -> Dict[str, Any]:
        """
        Inspect Niagara Custom HLSL nodes, including their current HLSL strings.

        Args:
            system_path: Niagara System package path or object path
            node_guid: Optional node GUID filter
            signature_name: Optional custom HLSL signature filter
            contains_text: Optional case-sensitive HLSL substring filter
            max_hlsl_chars: Maximum HLSL characters returned per node; -1 returns full strings
        """
        try:
            return send_niagara_command(
                "inspect_niagara_custom_hlsl_nodes",
                {
                    "system_path": system_path,
                    "node_guid": node_guid,
                    "signature_name": signature_name,
                    "contains_text": contains_text,
                    "max_hlsl_chars": max_hlsl_chars,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara Custom HLSL nodes: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def replace_niagara_custom_hlsl(
        ctx: Context,
        system_path: str,
        replacement_hlsl: str,
        node_guid: str = "",
        signature_name: str = "",
        contains_text: str = "",
        scratch_pad_name: str = "",
        source_path_contains: str = "",
        expected_contains: str = "",
        dry_run: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        timeout_seconds: float = 30.0,
        poll_interval_seconds: float = 0.05,
        max_hlsl_chars: int = 8000,
    ) -> Dict[str, Any]:
        """
        Replace exactly one Niagara Custom HLSL node after selector and expected-text checks.

        Args:
            system_path: Niagara System package path or object path
            replacement_hlsl: Full replacement HLSL string
            node_guid: Optional node GUID filter
            signature_name: Optional signature filter
            contains_text: Optional case-sensitive substring selector
            scratch_pad_name: Optional Scratch Pad script name filter
            source_path_contains: Optional case-sensitive source path substring filter
            expected_contains: Optional required substring in the current HLSL before replacing
            dry_run: Preview replacement without mutation by default
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save only after postcondition verification passes
            request_compile: Launch, wait for, and verify Niagara compiled-data replacement
            timeout_seconds: Maximum wait for pre-existing and requested compilation
            poll_interval_seconds: Niagara compilation polling interval
            max_hlsl_chars: Maximum before/after HLSL characters returned
        """
        try:
            return send_niagara_command(
                "replace_niagara_custom_hlsl",
                {
                    "system_path": system_path,
                    "replacement_hlsl": replacement_hlsl,
                    "node_guid": node_guid,
                    "signature_name": signature_name,
                    "contains_text": contains_text,
                    "scratch_pad_name": scratch_pad_name,
                    "source_path_contains": source_path_contains,
                    "expected_contains": expected_contains,
                    "dry_run": dry_run,
                    "allow_source_edit": allow_source_edit,
                    "save": save,
                    "request_compile": request_compile,
                    "timeout_seconds": timeout_seconds,
                    "poll_interval_seconds": poll_interval_seconds,
                    "max_hlsl_chars": max_hlsl_chars,
                },
                timeout_seconds=max(10.0, float(timeout_seconds) + 30.0),
            )
        except Exception as e:
            error_msg = f"Error replacing Niagara Custom HLSL: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_scratch_pad_interface(
        ctx: Context,
        system_path: str,
        include_graph_summary: bool = True,
        include_parent_scratch_pads: bool = True,
        max_scripts: int = 200,
        max_function_calls: int = 80,
    ) -> Dict[str, Any]:
        """
        Inspect Scratch Pad scripts as read-only authoring interfaces.

        Args:
            system_path: Niagara System package path or object path
            include_graph_summary: Include compact function-call/input/output graph summary per Scratch Pad
            include_parent_scratch_pads: Include inherited parent-emitter Scratch Pads
            max_scripts: Maximum Scratch Pad scripts to return
            max_function_calls: Maximum function calls per Scratch Pad graph summary
        """
        try:
            return send_niagara_command(
                "inspect_niagara_scratch_pad_interface",
                {
                    "system_path": system_path,
                    "include_graph_summary": include_graph_summary,
                    "include_parent_scratch_pads": include_parent_scratch_pads,
                    "max_scripts": max_scripts,
                    "max_function_calls": max_function_calls,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara scratch pad interface: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def duplicate_or_attach_emitter_from_source(
        ctx: Context,
        target_system_path: str,
        source_emitter_path: str = "",
        source_system_path: str = "",
        source_emitter_index: int | None = None,
        source_emitter_name: str = "",
        new_emitter_name: str = "",
        enabled: bool | None = None,
        copy_source_user_parameters: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Duplicate or attach a source Niagara emitter into a target temp Niagara System.

        Args:
            target_system_path: Target Niagara System package path or object path
            source_emitter_path: Optional source Niagara Emitter asset path
            source_system_path: Optional source Niagara System path when copying one of its emitter handles
            source_emitter_index: Optional source emitter index for source_system_path
            source_emitter_name: Optional source emitter display name for source_system_path
            new_emitter_name: Optional desired display name for the new target emitter
            enabled: Optional enabled-state override for the new target emitter
            copy_source_user_parameters: Copy serializable User.* defaults from source_system_path into the target system
            allow_source_edit: Allow editing target systems outside /Game/_MCP_Temp/NiagaraGenerated/
            save: Save the target system after attaching the emitter
            request_compile: Request Niagara compile after attaching the emitter
        """
        try:
            params: Dict[str, Any] = {
                "target_system_path": target_system_path,
                "source_emitter_path": source_emitter_path,
                "source_system_path": source_system_path,
                "source_emitter_name": source_emitter_name,
                "new_emitter_name": new_emitter_name,
                "copy_source_user_parameters": copy_source_user_parameters,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if source_emitter_index is not None:
                params["source_emitter_index"] = source_emitter_index
            if enabled is not None:
                params["enabled"] = enabled
            return send_niagara_command("duplicate_or_attach_emitter_from_source", params)
        except Exception as e:
            error_msg = f"Error duplicating or attaching Niagara emitter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def create_niagara_system_from_source_template(
        ctx: Context,
        target_system_path: str,
        source_system_path: str,
        source_emitter_indices: list[int] | None = None,
        copy_source_user_parameters: bool = True,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: int = 180,
    ) -> Dict[str, Any]:
        """
        Create a generated temp Niagara System by duplicating a source system.

        Args:
            target_system_path: New target Niagara System under /Game/_MCP_Temp/NiagaraGenerated/
            source_system_path: Source Niagara System to duplicate
            source_emitter_indices: Optional explicit source emitter indices to keep after duplicate; omit to keep all emitters
            copy_source_user_parameters: Retained for API compatibility; User.* defaults are preserved by the system duplicate
            save: Save the target system after duplicate/removal
            request_compile: Request compile after duplicate/removal
            wait_for_completion: Wait for target compile status after creation
            timeout_seconds: Per-command timeout for Unreal bridge calls
        """
        return create_niagara_system_from_source_template_impl(
            target_system_path=target_system_path,
            source_system_path=source_system_path,
            source_emitter_indices=source_emitter_indices,
            copy_source_user_parameters=copy_source_user_parameters,
            save=save,
            request_compile=request_compile,
            wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,
        )

    @mcp.tool()
    def create_niagara_scratch_pad_vec2_user_parameter_module(
        ctx: Context,
        target_system_path: str,
        scratch_pad_name: str = "MCP_BlankUpdateScratch",
        parameter_name: str = "Particles.SpriteSize",
        parameter_value: Any | None = None,
        user_parameter_name: str = "User.MCP_BlankScratchPad_SpriteSize",
        user_parameter_default_value: Any | None = None,
        target_usage: str = "ParticleUpdateScript",
        target_owner_kind: str = "system",
        target_emitter_index: int | None = None,
        target_emitter_name: str = "",
        target_stack_emitter_index: int | None = 0,
        target_stack_emitter_name: str = "",
        stack_suggested_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: int = 180,
    ) -> Dict[str, Any]:
        """
        Create, wire, and stack a blank Scratch Pad module driven by a Vector2D User parameter.

        Args:
            target_system_path: Target Niagara System package path or object path
            scratch_pad_name: Desired Scratch Pad script name
            parameter_name: Namespaced Niagara parameter written inside the Scratch Pad, such as Particles.SpriteSize
            parameter_value: Fallback Vector2D pin default before the User parameter link; defaults to [24, 24]
            user_parameter_name: Vector2D User parameter name, with or without User. prefix
            user_parameter_default_value: Vector2D default value for the User parameter; defaults to [42, 42]
            target_usage: Target stack usage and supported usage for the new Scratch Pad
            target_owner_kind: Scratch Pad owner kind: system or emitter
            target_emitter_index: Optional emitter index when creating an emitter-owned Scratch Pad
            target_emitter_name: Optional emitter name when creating an emitter-owned Scratch Pad
            target_stack_emitter_index: Optional emitter index for stack insertion; defaults to 0 for particle stacks
            target_stack_emitter_name: Optional emitter name for stack insertion
            stack_suggested_name: Optional suggested module node name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after stack insertion
            request_compile: Request Niagara compile after stack insertion
            wait_for_completion: Wait for compile completion after the recipe runs
            timeout_seconds: Per-command timeout for Unreal bridge calls
        """
        return create_niagara_scratch_pad_vec2_user_parameter_module_impl(
            target_system_path=target_system_path,
            scratch_pad_name=scratch_pad_name,
            parameter_name=parameter_name,
            parameter_value=parameter_value,
            user_parameter_name=user_parameter_name,
            user_parameter_default_value=user_parameter_default_value,
            target_usage=target_usage,
            target_owner_kind=target_owner_kind,
            target_emitter_index=target_emitter_index,
            target_emitter_name=target_emitter_name,
            target_stack_emitter_index=target_stack_emitter_index,
            target_stack_emitter_name=target_stack_emitter_name,
            stack_suggested_name=stack_suggested_name,
            allow_source_edit=allow_source_edit,
            save=save,
            request_compile=request_compile,
            wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,
        )

    @mcp.tool()
    def create_niagara_scratch_pad_color_user_parameter_module(
        ctx: Context,
        target_system_path: str,
        scratch_pad_name: str = "MCP_ColorScratch",
        parameter_name: str = "Particles.Color",
        parameter_value: Any | None = None,
        user_parameter_name: str = "User.MCP_ScratchPad_Color",
        user_parameter_default_value: Any | None = None,
        target_usage: str = "ParticleUpdateScript",
        target_owner_kind: str = "system",
        target_emitter_index: int | None = None,
        target_emitter_name: str = "",
        target_stack_emitter_index: int | None = 0,
        target_stack_emitter_name: str = "",
        stack_suggested_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: int = 180,
    ) -> Dict[str, Any]:
        """
        Create, wire, and stack a blank Scratch Pad module driven by a Linear Color User parameter.

        Args:
            target_system_path: Target Niagara System package path or object path
            scratch_pad_name: Desired Scratch Pad script name
            parameter_name: Namespaced Niagara color parameter written inside the Scratch Pad, such as Particles.Color
            parameter_value: Fallback Linear Color pin default before the User parameter link; defaults to [0.35, 0.65, 1.0, 1.0]
            user_parameter_name: Linear Color User parameter name, with or without User. prefix
            user_parameter_default_value: Linear Color User parameter default; defaults to [1.0, 0.45, 0.18, 1.0]
            target_usage: Target stack usage and supported usage for the new Scratch Pad
            target_owner_kind: Scratch Pad owner kind: system or emitter
            target_emitter_index: Optional emitter index when creating an emitter-owned Scratch Pad
            target_emitter_name: Optional emitter name when creating an emitter-owned Scratch Pad
            target_stack_emitter_index: Optional emitter index for stack insertion; defaults to 0 for particle stacks
            target_stack_emitter_name: Optional emitter name for stack insertion
            stack_suggested_name: Optional suggested module node name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after stack insertion
            request_compile: Request Niagara compile after stack insertion
            wait_for_completion: Wait for compile completion after the recipe runs
            timeout_seconds: Per-command timeout for Unreal bridge calls
        """
        return create_niagara_scratch_pad_color_user_parameter_module_impl(
            target_system_path=target_system_path,
            scratch_pad_name=scratch_pad_name,
            parameter_name=parameter_name,
            parameter_value=parameter_value,
            user_parameter_name=user_parameter_name,
            user_parameter_default_value=user_parameter_default_value,
            target_usage=target_usage,
            target_owner_kind=target_owner_kind,
            target_emitter_index=target_emitter_index,
            target_emitter_name=target_emitter_name,
            target_stack_emitter_index=target_stack_emitter_index,
            target_stack_emitter_name=target_stack_emitter_name,
            stack_suggested_name=stack_suggested_name,
            allow_source_edit=allow_source_edit,
            save=save,
            request_compile=request_compile,
            wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,
        )

    @mcp.tool()
    def create_niagara_scratch_pad_bool_user_parameter_module(
        ctx: Context,
        target_system_path: str,
        scratch_pad_name: str = "MCP_BoolUserScratch",
        parameter_name: str = "Particles.MCPBoolProbe",
        parameter_value: Any | None = None,
        module_input_name: str = "MCPBoolInput",
        module_input_default_value: Any | None = None,
        user_parameter_name: str = "User.MCP_ScratchPad_Bool",
        user_parameter_default_value: Any | None = None,
        target_usage: str = "ParticleUpdateScript",
        target_owner_kind: str = "system",
        target_emitter_index: int | None = None,
        target_emitter_name: str = "",
        target_stack_emitter_index: int | None = 0,
        target_stack_emitter_name: str = "",
        stack_suggested_name: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: int = 180,
    ) -> Dict[str, Any]:
        """
        Create, wire, and stack a blank Scratch Pad bool input driven by a User parameter.

        Args:
            target_system_path: Target Niagara System package path or object path
            scratch_pad_name: Desired Scratch Pad script name
            parameter_name: Namespaced bool Niagara parameter written inside the Scratch Pad, such as Particles.MCPBoolProbe
            parameter_value: Fallback bool pin default for the internal ParameterMapSet write; defaults to true
            module_input_name: Module input name exposed by the Scratch Pad, with or without Module. prefix
            module_input_default_value: Fallback bool default for the Scratch Pad Module input read; defaults to true
            user_parameter_name: Bool User parameter name, with or without User. prefix
            user_parameter_default_value: Bool default value for the User parameter; defaults to false
            target_usage: Target stack usage and supported usage for the new Scratch Pad
            target_owner_kind: Scratch Pad owner kind: system or emitter
            target_emitter_index: Optional emitter index when creating an emitter-owned Scratch Pad
            target_emitter_name: Optional emitter name when creating an emitter-owned Scratch Pad
            target_stack_emitter_index: Optional emitter index for stack insertion; defaults to 0 for particle stacks
            target_stack_emitter_name: Optional emitter name for stack insertion
            stack_suggested_name: Optional suggested module node name
            overwrite_existing: Replace an existing override graph on the inserted module input
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after User parameter binding
            request_compile: Request Niagara compile after User parameter binding
            wait_for_completion: Wait for compile completion after the recipe runs
            timeout_seconds: Per-command timeout for Unreal bridge calls
        """
        return create_niagara_scratch_pad_bool_user_parameter_module_impl(
            target_system_path=target_system_path,
            scratch_pad_name=scratch_pad_name,
            parameter_name=parameter_name,
            parameter_value=parameter_value,
            module_input_name=module_input_name,
            module_input_default_value=module_input_default_value,
            user_parameter_name=user_parameter_name,
            user_parameter_default_value=user_parameter_default_value,
            target_usage=target_usage,
            target_owner_kind=target_owner_kind,
            target_emitter_index=target_emitter_index,
            target_emitter_name=target_emitter_name,
            target_stack_emitter_index=target_stack_emitter_index,
            target_stack_emitter_name=target_stack_emitter_name,
            stack_suggested_name=stack_suggested_name,
            overwrite_existing=overwrite_existing,
            allow_source_edit=allow_source_edit,
            save=save,
            request_compile=request_compile,
            wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,
        )

    @mcp.tool()
    def create_niagara_scratch_pad_bool_visible_toggle_module(
        ctx: Context,
        target_system_path: str,
        scratch_pad_name: str = "MCP_BoolVisibleScratch",
        parameter_name: str = "Particles.SpriteSize",
        parameter_value: Any | None = None,
        base_module_input_name: str = "MCPBaseSpriteSize",
        base_module_input_default_value: Any | None = None,
        user_parameter_name: str = "User.MCP_VisibleToggle",
        user_parameter_default_value: Any | None = None,
        custom_hlsl_input_name: str = "MCPVisibleToggle",
        custom_hlsl_expression: str = "BaseValue * (MCPVisibleToggle ? 1.0 : 0.0)",
        custom_hlsl_output_pin_name: str = "MCPVisibleSize",
        custom_hlsl_signature_name: str = "MCP_ScratchPadBoolVisibleSize",
        target_usage: str = "ParticleUpdateScript",
        target_owner_kind: str = "system",
        target_emitter_index: int | None = None,
        target_emitter_name: str = "",
        target_stack_emitter_index: int | None = 0,
        target_stack_emitter_name: str = "",
        stack_suggested_name: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: int = 180,
    ) -> Dict[str, Any]:
        """
        Create, wire, and stack a blank Scratch Pad module whose User bool toggles sprite visibility.

        Args:
            target_system_path: Target Niagara System package path or object path
            scratch_pad_name: Desired Scratch Pad script name
            parameter_name: Namespaced Vector2D parameter to control, defaults to Particles.SpriteSize
            parameter_value: Fallback visible sprite size; defaults to [64.0, 64.0]
            base_module_input_name: Vector2D Module input preserved as Custom HLSL BaseValue
            base_module_input_default_value: Default BaseValue, defaults to parameter_value
            user_parameter_name: Bool User parameter name, with or without User. prefix
            user_parameter_default_value: Bool default value; true means visible
            custom_hlsl_input_name: Bool Custom HLSL input name
            custom_hlsl_expression: Exact expression applied to BaseValue and the bool input
            custom_hlsl_output_pin_name: Custom HLSL output pin name
            custom_hlsl_signature_name: Custom HLSL dynamic input signature name
            target_usage: Target stack usage and supported usage for the new Scratch Pad
            target_owner_kind: Scratch Pad owner kind: system or emitter
            target_emitter_index: Optional emitter index when creating an emitter-owned Scratch Pad
            target_emitter_name: Optional emitter name when creating an emitter-owned Scratch Pad
            target_stack_emitter_index: Optional emitter index for stack insertion; defaults to 0 for particle stacks
            target_stack_emitter_name: Optional emitter name for stack insertion
            stack_suggested_name: Optional suggested module node name
            overwrite_existing: Reserved guard; false is required for this reviewed shape
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after stack insertion
            request_compile: Request Niagara compile after stack insertion
            wait_for_completion: Wait for compile completion after the recipe runs
            timeout_seconds: Per-command timeout for Unreal bridge calls
        """
        return create_niagara_scratch_pad_bool_visible_toggle_module_impl(
            target_system_path=target_system_path,
            scratch_pad_name=scratch_pad_name,
            parameter_name=parameter_name,
            parameter_value=parameter_value,
            base_module_input_name=base_module_input_name,
            base_module_input_default_value=base_module_input_default_value,
            user_parameter_name=user_parameter_name,
            user_parameter_default_value=user_parameter_default_value,
            custom_hlsl_input_name=custom_hlsl_input_name,
            custom_hlsl_expression=custom_hlsl_expression,
            custom_hlsl_output_pin_name=custom_hlsl_output_pin_name,
            custom_hlsl_signature_name=custom_hlsl_signature_name,
            target_usage=target_usage,
            target_owner_kind=target_owner_kind,
            target_emitter_index=target_emitter_index,
            target_emitter_name=target_emitter_name,
            target_stack_emitter_index=target_stack_emitter_index,
            target_stack_emitter_name=target_stack_emitter_name,
            stack_suggested_name=stack_suggested_name,
            overwrite_existing=overwrite_existing,
            allow_source_edit=allow_source_edit,
            save=save,
            request_compile=request_compile,
            wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,
        )

    @mcp.tool()
    def create_or_duplicate_scratch_pad_module(
        ctx: Context,
        target_system_path: str,
        source_script_path: str = "",
        source_system_path: str = "",
        source_owner_kind: str = "system",
        source_script_index: int | None = None,
        source_scratch_pad_name: str = "",
        source_emitter_index: int | None = None,
        source_emitter_name: str = "",
        target_owner_kind: str = "system",
        target_emitter_index: int | None = None,
        target_emitter_name: str = "",
        new_script_name: str = "",
        target_supported_usage: str = "ParticleUpdateScript",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Create a blank Scratch Pad module or duplicate an existing Scratch Pad script into a target temp Niagara System or emitter.

        Args:
            target_system_path: Target Niagara System package path or object path
            source_script_path: Optional direct Scratch Pad script object path; omit with source_system_path to create from the engine default module template
            source_system_path: Optional source Niagara System path when selecting by owner/index/name; omit with source_script_path to create from the engine default module template
            source_owner_kind: Source owner kind: system, emitter, parent_emitter
            source_script_index: Optional source Scratch Pad script index
            source_scratch_pad_name: Optional source Scratch Pad script name
            source_emitter_index: Optional source emitter index for emitter-owned Scratch Pads
            source_emitter_name: Optional source emitter name for emitter-owned Scratch Pads
            target_owner_kind: Target owner kind: system or emitter
            target_emitter_index: Optional target emitter index when target_owner_kind is emitter
            target_emitter_name: Optional target emitter name when target_owner_kind is emitter
            new_script_name: Optional desired name for the created/duplicated Scratch Pad script
            target_supported_usage: Supported stack usage for newly-created blank modules, such as ParticleUpdateScript
            allow_source_edit: Allow editing target systems outside /Game/_MCP_Temp/NiagaraGenerated/
            save: Save the target system after creating/duplicating the Scratch Pad
            request_compile: Request Niagara compile after creating/duplicating the Scratch Pad
        """
        try:
            params: Dict[str, Any] = {
                "target_system_path": target_system_path,
                "source_script_path": source_script_path,
                "source_system_path": source_system_path,
                "source_owner_kind": source_owner_kind,
                "source_scratch_pad_name": source_scratch_pad_name,
                "source_emitter_name": source_emitter_name,
                "target_owner_kind": target_owner_kind,
                "target_emitter_name": target_emitter_name,
                "new_script_name": new_script_name,
                "target_supported_usage": target_supported_usage,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if source_script_index is not None:
                params["source_script_index"] = source_script_index
            if source_emitter_index is not None:
                params["source_emitter_index"] = source_emitter_index
            if target_emitter_index is not None:
                params["target_emitter_index"] = target_emitter_index
            return send_niagara_command("create_or_duplicate_scratch_pad_module", params)
        except Exception as e:
            error_msg = f"Error duplicating Niagara Scratch Pad module: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_scratch_pad_module_to_stack(
        ctx: Context,
        target_system_path: str,
        scratch_pad_script_path: str = "",
        scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None,
        scratch_pad_name: str = "",
        scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "",
        target_usage: str = "ParticleUpdateScript",
        target_usage_id: str = "",
        target_emitter_index: int | None = None,
        target_emitter_name: str = "",
        target_index: int | None = None,
        suggested_name: str = "",
        skip_if_duplicate: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Insert a target-local Scratch Pad module into a Niagara stack.

        Args:
            target_system_path: Target Niagara System package path or object path
            scratch_pad_script_path: Optional direct Scratch Pad script object path already inside the target system
            scratch_pad_owner_kind: Scratch Pad owner kind: system or emitter
            scratch_pad_script_index: Optional Scratch Pad script index
            scratch_pad_name: Optional Scratch Pad script name
            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads
            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads
            target_usage: Target stack usage, such as ParticleUpdateScript or EmitterSpawnScript
            target_usage_id: Optional target output usage GUID
            target_emitter_index: Optional target emitter index for emitter/particle stacks
            target_emitter_name: Optional target emitter name for emitter/particle stacks
            target_index: Optional stack insertion index; omit or -1 to append
            suggested_name: Optional suggested module node name
            skip_if_duplicate: Skip without mutation if the same Scratch Pad already exists in the requested output stack
            allow_source_edit: Allow editing target systems outside /Game/_MCP_Temp/NiagaraGenerated/
            save: Save the target system after stack insertion
            request_compile: Request Niagara compile after stack insertion
        """
        try:
            params: Dict[str, Any] = {
                "target_system_path": target_system_path,
                "scratch_pad_script_path": scratch_pad_script_path,
                "scratch_pad_owner_kind": scratch_pad_owner_kind,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_emitter_name": scratch_pad_emitter_name,
                "target_usage": target_usage,
                "target_usage_id": target_usage_id,
                "target_emitter_name": target_emitter_name,
                "suggested_name": suggested_name,
                "skip_if_duplicate": skip_if_duplicate,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            if scratch_pad_emitter_index is not None:
                params["scratch_pad_emitter_index"] = scratch_pad_emitter_index
            if target_emitter_index is not None:
                params["target_emitter_index"] = target_emitter_index
            if target_index is not None:
                params["target_index"] = target_index
            return send_niagara_command("add_scratch_pad_module_to_stack", params)
        except Exception as e:
            error_msg = f"Error adding Niagara Scratch Pad module to stack: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_scratch_pad_assignment_module_to_stack(
        ctx: Context,
        target_system_path: str,
        parameter_name: str = "Particles.SpriteSize",
        parameter_type: str = "vec2",
        default_value: str = "50.000000,50.000000",
        value: Any | None = None,
        target_usage: str = "ParticleUpdateScript",
        target_usage_id: str = "",
        target_emitter_index: int | None = None,
        target_emitter_name: str = "",
        target_index: int | None = None,
        skip_if_duplicate: bool = True,
        allow_assignment_module_route: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Add a guarded temp-only Niagara assignment module to the target stack.

        Args:
            target_system_path: Target Niagara System package path or object path
            parameter_name: Admitted target parameter; currently Particles.SpriteSize, Particles.Color, Particles.Lifetime, or Particles.Velocity
            parameter_type: Admitted type; vec2 / Vector2D for SpriteSize, color / Linear Color for Color, float for Lifetime, vec3 / Vector for Velocity
            default_value: Raw Unreal pin default string; takes precedence over value
            value: JSON value converted to a Niagara pin default string when default_value is empty
            target_usage: Target stack usage, such as ParticleUpdateScript
            target_usage_id: Optional target output usage GUID
            target_emitter_index: Optional target emitter index for emitter/particle stacks
            target_emitter_name: Optional target emitter name for emitter/particle stacks
            target_index: Optional stack insertion index; omit or -1 to append
            skip_if_duplicate: Skip without mutation if the same assignment target already exists
            allow_assignment_module_route: Required proof token for this not-yet-release-ready route
            allow_source_edit: Allow editing target systems outside /Game/_MCP_Temp/NiagaraGenerated/
            save: Save the target system after stack insertion
            request_compile: Request Niagara compile after stack insertion
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "target_system_path": target_system_path,
                "parameter_name": parameter_name,
                "parameter_type": parameter_type,
                "target_usage": target_usage,
                "target_usage_id": target_usage_id,
                "target_emitter_name": target_emitter_name,
                "skip_if_duplicate": skip_if_duplicate,
                "allow_assignment_module_route": allow_assignment_module_route,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if default_value:
                params["default_value"] = default_value
            else:
                params["value"] = value
            if target_emitter_index is not None:
                params["target_emitter_index"] = target_emitter_index
            if target_index is not None:
                params["target_index"] = target_index
            result = send_niagara_command("add_niagara_scratch_pad_assignment_module_to_stack", params)
            result_payload = result.get("result")
            if not isinstance(result_payload, dict):
                result_payload = result
            result_succeeded = bool(
                result.get("status") == "success"
                or result.get("success") is True
                or result_payload.get("success") is True
            ) and result.get("status") != "error" and result_payload.get("success") is not False
            skipped_duplicate = result_payload.get("skipped_duplicate") is True
            compile_was_requested = bool(
                result_payload.get("compile_requested", request_compile and not skipped_duplicate)
            )
            if (
                request_compile
                and wait_for_completion
                and result_succeeded
                and compile_was_requested
            ):
                compile_status = send_niagara_command(
                    "inspect_niagara_compile_status",
                    {
                        "system_path": target_system_path,
                        "request_compile": False,
                        "force": False,
                        "allow_source_compile": allow_source_edit,
                        "wait_for_completion": True,
                        "timeout_seconds": timeout_seconds,
                        "poll_interval_seconds": poll_interval_seconds,
                    },
                )
                result["compile_status"] = compile_status
                compile_payload = compile_status.get("result")
                if not isinstance(compile_payload, dict):
                    compile_payload = compile_status
                compile_payload_invalid = False

                wait_timed_out_value = compile_payload.get("wait_timed_out", False)
                if not isinstance(wait_timed_out_value, bool):
                    compile_payload_invalid = True
                    wait_timed_out_value = False

                def nonnegative_compile_count(field_name: str) -> int:
                    nonlocal compile_payload_invalid
                    value = compile_payload.get(field_name, 0)
                    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
                        compile_payload_invalid = True
                        return 0
                    return value

                outstanding_after = nonnegative_compile_count(
                    "outstanding_compilation_requests_after"
                )
                active_after = nonnegative_compile_count("active_compilations_after")
                error_count = nonnegative_compile_count("error_count")
                has_errors_value = compile_payload.get("has_errors", False)
                if not isinstance(has_errors_value, bool):
                    compile_payload_invalid = True
                    has_errors_value = False

                compile_may_still_be_in_flight = bool(
                    wait_timed_out_value or outstanding_after > 0 or active_after > 0
                )
                compile_has_errors = bool(has_errors_value or error_count > 0)
                compile_read_failed = bool(
                    compile_status.get("status") == "error"
                    or compile_status.get("success") is False
                    or compile_payload_invalid
                    or (
                        isinstance(compile_status.get("result"), dict)
                        and compile_status["result"].get("success") is False
                    )
                )
                if compile_read_failed or compile_may_still_be_in_flight or compile_has_errors:
                    if compile_may_still_be_in_flight:
                        error_code = "niagara_compile_completion_timeout"
                        message = "Niagara assignment completed, but compilation may still be in flight"
                    elif compile_has_errors:
                        error_code = "niagara_compile_postcondition_failed"
                        message = "Niagara assignment completed, but compile readback reported errors"
                    elif compile_payload_invalid:
                        error_code = "niagara_compile_status_invalid"
                        message = "Niagara assignment completed, but compile readback fields were malformed"
                    else:
                        error_code = "niagara_compile_status_read_failed"
                        message = "Niagara assignment was sent, but compile-status readback failed"
                    return _niagara_error(
                        message,
                        error_code=error_code,
                        mutation_response=result,
                        compile_status=compile_status,
                        niagara_compile_may_still_be_in_flight=compile_may_still_be_in_flight,
                        niagara_compile_has_errors=compile_has_errors,
                        niagara_compile_status_invalid=compile_payload_invalid,
                        **composite_failure_safety_fields(
                            compile_status,
                            failed_stage="inspect_niagara_compile_status",
                            mutation_relevant=False,
                            completed_mutation_stages=(
                                ["add_niagara_scratch_pad_assignment_module_to_stack"]
                                if not skipped_duplicate
                                else []
                            ),
                        ),
                    )
            return result
        except Exception as e:
            error_msg = f"Error adding Niagara Scratch Pad assignment module to stack: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_scratch_pad_parameter_write(
        ctx: Context,
        system_path: str,
        parameter_name: str,
        value: Any | None = None,
        parameter_type: str = "vec2",
        default_value: str = "",
        scratch_pad_script_path: str = "",
        scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None,
        scratch_pad_name: str = "",
        scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "",
        skip_if_exists: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Add a ParameterMapSet write inside a target-local Scratch Pad module graph.

        Args:
            system_path: Niagara System package path or object path
            parameter_name: Namespaced Niagara parameter to write, such as Particles.SpriteSize
            value: JSON value converted to a Niagara pin default string
            parameter_type: Niagara type alias, such as float, bool, vec2, vec3, vec4, color, or position
            default_value: Raw Unreal pin default string; takes precedence over value
            scratch_pad_script_path: Direct target Scratch Pad script path; required unless name or index is provided
            scratch_pad_owner_kind: system or emitter
            scratch_pad_script_index: Scratch Pad script index; required unless path or name is provided
            scratch_pad_name: Scratch Pad script name; required unless path or index is provided
            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads
            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads
            skip_if_exists: Return success without mutation when the Scratch Pad already writes parameter_name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "parameter_name": parameter_name,
                "parameter_type": parameter_type,
                "scratch_pad_script_path": scratch_pad_script_path,
                "scratch_pad_owner_kind": scratch_pad_owner_kind,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_emitter_name": scratch_pad_emitter_name,
                "skip_if_exists": skip_if_exists,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if default_value:
                params["default_value"] = default_value
            else:
                params["value"] = value
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            if scratch_pad_emitter_index is not None:
                params["scratch_pad_emitter_index"] = scratch_pad_emitter_index
            return send_niagara_command("add_niagara_scratch_pad_parameter_write", params)
        except Exception as e:
            error_msg = f"Error adding Niagara Scratch Pad parameter write: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_scratch_pad_module_input(
        ctx: Context,
        system_path: str,
        input_name: str,
        default_value: Any,
        input_type: str = "bool",
        consumer_pin_name: str = "",
        scratch_pad_script_path: str = "",
        scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None,
        scratch_pad_name: str = "",
        scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "",
        require_consumer_link: bool = True,
        overwrite_existing_link: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Add a reachable Module.* input read inside a target-local Scratch Pad module graph.

        Args:
            system_path: Niagara System package path or object path
            input_name: Module input name, with or without Module. prefix
            default_value: JSON default value for the input
            input_type: Niagara type alias, such as bool, float, int, vec2, vec3, vec4, color, or position
            consumer_pin_name: Existing Scratch Pad ParameterMapSet value pin to consume the Module.* read
            scratch_pad_script_path: Direct target Scratch Pad script path; required unless name or index is provided
            scratch_pad_owner_kind: system or emitter
            scratch_pad_script_index: Scratch Pad script index; required unless path or name is provided
            scratch_pad_name: Scratch Pad script name; required unless path or index is provided
            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads
            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads
            require_consumer_link: Require consumer_pin_name so the read is reachable by stack traversal
            overwrite_existing_link: Replace existing links on the consumer pin
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "input_type": input_type,
                "default_value": default_value,
                "consumer_pin_name": consumer_pin_name,
                "scratch_pad_script_path": scratch_pad_script_path,
                "scratch_pad_owner_kind": scratch_pad_owner_kind,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_emitter_name": scratch_pad_emitter_name,
                "require_consumer_link": require_consumer_link,
                "overwrite_existing_link": overwrite_existing_link,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            if scratch_pad_emitter_index is not None:
                params["scratch_pad_emitter_index"] = scratch_pad_emitter_index
            return send_niagara_command("add_niagara_scratch_pad_module_input", params)
        except Exception as e:
            error_msg = f"Error adding Niagara Scratch Pad module input: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_stock_module_to_stack(
        ctx: Context,
        target_system_path: str,
        module_script_path: str,
        target_usage: str = "ParticleSpawnScriptInterpolated",
        target_usage_id: str = "",
        target_emitter_index: int | None = None,
        target_emitter_name: str = "",
        target_index: int | None = None,
        suggested_name: str = "",
        skip_if_duplicate: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Insert an existing Niagara Module script asset into a target stack.

        Args:
            target_system_path: Target Niagara System package path or object path
            module_script_path: Niagara Module script path, for example /Niagara/Modules/Spawn/Location/BoxLocation.BoxLocation
            target_usage: Target stack usage, such as ParticleSpawnScriptInterpolated or ParticleUpdateScript
            target_usage_id: Optional target output usage GUID
            target_emitter_index: Optional target emitter index for emitter/particle stacks
            target_emitter_name: Optional target emitter name for emitter/particle stacks
            target_index: Optional stack insertion index; omit or -1 to append
            suggested_name: Optional suggested module node name
            skip_if_duplicate: Skip without mutation if the same module already exists in the requested output stack
            allow_source_edit: Allow editing target systems outside /Game/_MCP_Temp/NiagaraGenerated/
            save: Save the target system after stack insertion
            request_compile: Request Niagara compile after stack insertion
        """
        try:
            params: Dict[str, Any] = {
                "target_system_path": target_system_path,
                "module_script_path": module_script_path,
                "target_usage": target_usage,
                "target_usage_id": target_usage_id,
                "target_emitter_name": target_emitter_name,
                "suggested_name": suggested_name,
                "skip_if_duplicate": skip_if_duplicate,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if target_emitter_index is not None:
                params["target_emitter_index"] = target_emitter_index
            if target_index is not None:
                params["target_index"] = target_index
            return send_niagara_command("add_niagara_stock_module_to_stack", params)
        except Exception as e:
            error_msg = f"Error adding Niagara stock module to stack: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_emitter_persistent_ids(
        ctx: Context,
        system_path: str,
        enabled: bool,
        emitter_index: int | None = None,
        emitter_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 60.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """Enable or disable stable Particles.ID generation on one emitter.

        Collision Event generation requires this on its CPU source emitter.
        Mutation is restricted to generated temp systems by default.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "enabled": enabled,
                "emitter_name": emitter_name,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            return send_niagara_command("set_niagara_emitter_persistent_ids", params)
        except Exception as e:
            error_msg = f"Error setting Niagara emitter Persistent IDs: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_event_handlers(
        ctx: Context,
        system_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        max_handlers: int = 256,
    ) -> Dict[str, Any]:
        """Read-only inspect of Niagara Event Handler topology and settings.

        Returns each matching emitter's handler usage ID, source emitter/event,
        execution mode, spawn limits, script graph, and output-node readback.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "max_handlers": max_handlers,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if emitter_name:
                params["emitter_name"] = emitter_name
            return send_niagara_command("inspect_niagara_event_handlers", params)
        except Exception as e:
            error_msg = f"Error inspecting Niagara Event Handlers: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_event_handler(
        ctx: Context,
        system_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        source_emitter_index: int | None = None,
        source_emitter_name: str = "",
        source_emitter_id: str = "",
        source_event_name: str = "CollisionEvent",
        execution_mode: str = "SpawnedParticles",
        spawn_number: int = 1,
        max_events_per_frame: int = 64,
        random_spawn_number: bool = False,
        min_spawn_number: int = 0,
        update_attribute_initial_values: bool = True,
        skip_if_duplicate: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 60.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """Create one CPU Niagara Event Handler output in a target emitter.

        The returned target_usage_id can be passed to
        add_niagara_stock_module_to_stack(target_usage="ParticleEventScript")
        to populate the handler. Mutation is temp-only by default, and
        duplicate source-event consumers fail closed.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "emitter_name": emitter_name,
                "source_emitter_name": source_emitter_name,
                "source_emitter_id": source_emitter_id,
                "source_event_name": source_event_name,
                "execution_mode": execution_mode,
                "spawn_number": spawn_number,
                "max_events_per_frame": max_events_per_frame,
                "random_spawn_number": random_spawn_number,
                "min_spawn_number": min_spawn_number,
                "update_attribute_initial_values": update_attribute_initial_values,
                "skip_if_duplicate": skip_if_duplicate,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if source_emitter_index is not None:
                params["source_emitter_index"] = source_emitter_index
            return send_niagara_command("add_niagara_event_handler", params)
        except Exception as e:
            error_msg = f"Error adding Niagara Event Handler: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_scratch_pad_di_variadic_input(
        ctx: Context,
        system_path: str,
        scratch_pad_name: str,
        function_node_guid: str,
        expected_function_name: str,
        expected_di_class: str,
        expected_existing_inputs: list[dict[str, Any]],
        pin_name: str,
        module_input_name: str,
        expected_referencing_module_node_guid: str,
        default_value: Any = 0,
        input_type: str = "int",
        scratch_pad_script_path: str = "",
        expected_input_map_node_guid: str = "",
        expected_output_node_guid: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 60.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """Atomically add one int variadic input to an existing signature-only Scratch Pad DI call.

        The command requires exact pre-mutation fingerprints for the function node,
        DI class, every existing signature input/link, and the single referencing stack
        module. It rejects duplicates, creates a Module.* ParameterMapGet read, refreshes
        the stack reference, and saves only after graph readback and a synchronized,
        warning-free Niagara compile gate. A clean package and idle compiler are required
        by default; post-mutation failures roll the graph back without saving.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_script_path": scratch_pad_script_path,
                "expected_input_map_node_guid": expected_input_map_node_guid,
                "expected_output_node_guid": expected_output_node_guid,
                "function_node_guid": function_node_guid,
                "expected_function_name": expected_function_name,
                "expected_di_class": expected_di_class,
                "expected_existing_inputs": expected_existing_inputs,
                "pin_name": pin_name,
                "module_input_name": module_input_name,
                "expected_referencing_module_node_guid": expected_referencing_module_node_guid,
                "default_value": default_value,
                "input_type": input_type,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            return send_niagara_command(
                "add_niagara_scratch_pad_di_variadic_input",
                params,
                timeout_seconds=max(10.0, float(timeout_seconds) + 30.0),
            )
        except Exception as e:
            error_msg = f"Error adding Niagara Scratch Pad DI variadic input: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_scratch_pad_function_input_default(
        ctx: Context,
        system_path: str,
        function_name: str = "",
        input_pin_name: str = "",
        value: Any | None = None,
        default_value: str = "",
        function_node_guid: str = "",
        function_node_index: int | None = None,
        function_call_index: int | None = None,
        scratch_pad_script_path: str = "",
        scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None,
        scratch_pad_name: str = "",
        scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "",
        break_links: bool = True,
        allow_multi_link_break: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Set a Scratch Pad internal function-call input pin default value.

        Args:
            system_path: Niagara System package path or object path
            function_name: Target internal function name, for example SetRenderTargetValue
            input_pin_name: Target function input pin name, for example Value
            value: JSON value converted to a Niagara pin default string
            default_value: Raw Unreal pin default string; takes precedence over value
            function_node_guid: Optional target function node GUID
            function_node_index: Optional graph node index from inspect_niagara_graph
            function_call_index: Optional function-call-only index inside the Scratch Pad graph
            scratch_pad_script_path: Direct target Scratch Pad script path; required unless name or index is provided
            scratch_pad_owner_kind: system or emitter
            scratch_pad_script_index: Scratch Pad script index; required unless path or name is provided
            scratch_pad_name: Scratch Pad script name; required unless path or index is provided
            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads
            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads
            break_links: Break existing links from the target input pin after setting its default
            allow_multi_link_break: Allow breaking more than one existing link
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "function_name": function_name,
                "function_node_guid": function_node_guid,
                "input_pin_name": input_pin_name,
                "scratch_pad_script_path": scratch_pad_script_path,
                "scratch_pad_owner_kind": scratch_pad_owner_kind,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_emitter_name": scratch_pad_emitter_name,
                "break_links": break_links,
                "allow_multi_link_break": allow_multi_link_break,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if default_value:
                params["default_value"] = default_value
            else:
                params["value"] = value
            if function_node_index is not None:
                params["function_node_index"] = function_node_index
            if function_call_index is not None:
                params["function_call_index"] = function_call_index
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            if scratch_pad_emitter_index is not None:
                params["scratch_pad_emitter_index"] = scratch_pad_emitter_index
            return send_niagara_command("set_niagara_scratch_pad_function_input_default", params)
        except Exception as e:
            error_msg = f"Error setting Niagara Scratch Pad function input default: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def link_niagara_scratch_pad_pin_to_user_parameter(
        ctx: Context,
        system_path: str,
        target_pin_name: str,
        user_parameter_name: str,
        scratch_pad_script_path: str = "",
        scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None,
        scratch_pad_name: str = "",
        scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "",
        target_node_guid: str = "",
        target_node_index: int | None = None,
        parameter_map_set_index: int | None = None,
        default_value: Any = None,
        overwrite_existing: bool = False,
        allow_multi_link_break: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Link a Scratch Pad ParameterMapSet input pin to a matching User parameter.

        Args:
            system_path: Niagara System package path or object path
            target_pin_name: Target ParameterMapSet input pin, e.g. Local.Module.AdvectionAmount
            user_parameter_name: User parameter name, with or without User. prefix
            scratch_pad_script_path: Direct target Scratch Pad script path; required unless name or index is provided
            scratch_pad_owner_kind: system or emitter
            scratch_pad_script_index: Scratch Pad script index; required unless path or name is provided
            scratch_pad_name: Scratch Pad script name; required unless path or index is provided
            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads
            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads
            target_node_guid: Optional target ParameterMapSet node GUID
            target_node_index: Optional graph node index from inspect_niagara_graph
            parameter_map_set_index: Optional ParameterMapSet-only index in the Scratch Pad graph
            default_value: Optional User parameter default value such as [0, 0] or [1, 1, 1, 1]
            overwrite_existing: Replace existing links on the target pin
            allow_multi_link_break: Allow breaking more than one existing link
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "target_pin_name": target_pin_name,
                "user_parameter_name": user_parameter_name,
                "scratch_pad_script_path": scratch_pad_script_path,
                "scratch_pad_owner_kind": scratch_pad_owner_kind,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_emitter_name": scratch_pad_emitter_name,
                "target_node_guid": target_node_guid,
                "overwrite_existing": overwrite_existing,
                "allow_multi_link_break": allow_multi_link_break,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            if scratch_pad_emitter_index is not None:
                params["scratch_pad_emitter_index"] = scratch_pad_emitter_index
            if target_node_index is not None:
                params["target_node_index"] = target_node_index
            if parameter_map_set_index is not None:
                params["parameter_map_set_index"] = parameter_map_set_index
            if default_value is not None:
                params["default_value"] = default_value
            return send_niagara_command("link_niagara_scratch_pad_pin_to_user_parameter", params)
        except Exception as e:
            error_msg = f"Error linking Niagara Scratch Pad pin to User parameter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def wrap_niagara_scratch_pad_output_with_stack_context(
        ctx: Context,
        system_path: str,
        target_pin_name: str = "StackContext.RGBA",
        scratch_pad_script_path: str = "",
        scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None,
        scratch_pad_name: str = "RenderCircleToGrid",
        scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "",
        target_node_guid: str = "",
        target_node_index: int | None = None,
        parameter_map_set_index: int | None = None,
        previous_stack_value_pin_name: str = "PreviousStackValue",
        local_value_pin_name: str = "LocalStampValue",
        expression: str = "max(PreviousStackValue, LocalStampValue)",
        skip_if_already_wrapped: bool = True,
        replace_existing_custom: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Wrap a Scratch Pad ParameterMapSet value input with a Custom HLSL accumulator.

        The existing linked value becomes local_value_pin_name, while the expression can
        read the incoming StackContext through the dynamic input's ParameterMap.

        Args:
            system_path: Niagara System package path or object path
            target_pin_name: ParameterMapSet input pin to wrap, defaults to StackContext.RGBA
            scratch_pad_script_path: Direct target Scratch Pad script path
            scratch_pad_owner_kind: system or emitter
            scratch_pad_script_index: Scratch Pad script index
            scratch_pad_name: Scratch Pad script name, defaults to RenderCircleToGrid
            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads
            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads
            target_node_guid: Optional target ParameterMapSet node GUID
            target_node_index: Optional graph node index from inspect_niagara_graph
            parameter_map_set_index: Optional ParameterMapSet-only index in the Scratch Pad graph
            previous_stack_value_pin_name: Custom HLSL input name for the incoming StackContext value
            local_value_pin_name: Custom HLSL input name for the previous linked value
            expression: Dynamic Custom HLSL expression, defaults to max(PreviousStackValue, LocalStampValue)
            skip_if_already_wrapped: Return success when the same Custom HLSL wrapper already exists
            replace_existing_custom: Replace an existing Custom HLSL wrapper while preserving local_value_pin_name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "target_pin_name": target_pin_name,
                "scratch_pad_script_path": scratch_pad_script_path,
                "scratch_pad_owner_kind": scratch_pad_owner_kind,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_emitter_name": scratch_pad_emitter_name,
                "target_node_guid": target_node_guid,
                "previous_stack_value_pin_name": previous_stack_value_pin_name,
                "local_value_pin_name": local_value_pin_name,
                "expression": expression,
                "skip_if_already_wrapped": skip_if_already_wrapped,
                "replace_existing_custom": replace_existing_custom,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            if scratch_pad_emitter_index is not None:
                params["scratch_pad_emitter_index"] = scratch_pad_emitter_index
            if target_node_index is not None:
                params["target_node_index"] = target_node_index
            if parameter_map_set_index is not None:
                params["parameter_map_set_index"] = parameter_map_set_index
            return send_niagara_command("wrap_niagara_scratch_pad_output_with_stack_context", params)
        except Exception as e:
            error_msg = f"Error wrapping Niagara Scratch Pad output with StackContext accumulator: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def insert_niagara_scratch_pad_custom_hlsl_for_pin(
        ctx: Context,
        system_path: str,
        target_pin_name: str,
        expression: str,
        scratch_pad_script_path: str = "",
        scratch_pad_owner_kind: str = "system",
        scratch_pad_script_index: int | None = None,
        scratch_pad_name: str = "RenderGrid",
        scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "",
        target_node_guid: str = "",
        target_node_name: str = "",
        target_node_index: int | None = None,
        preserve_existing_link_as: str = "BaseValue",
        output_pin_name: str = "CustomHLSLOutput",
        signature_name: str = "IF_CustomPinValue",
        custom_hlsl_script_usage: str = "DynamicInput",
        inputs: list[dict[str, Any]] | None = None,
        user_parameter_inputs: list[dict[str, Any]] | None = None,
        skip_if_already_inserted: bool = True,
        replace_existing_custom: bool = False,
        rebuild_existing_custom: bool = False,
        requires_context: bool = False,
        delete_unlinked_custom_nodes: bool = False,
        delete_unlinked_custom_input_source_nodes: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Insert or replace a Custom HLSL dynamic input for a Scratch Pad input pin.

        Args:
            system_path: Niagara System package path or object path
            target_pin_name: Target input pin to wrap with Custom HLSL
            expression: Custom HLSL expression body
            scratch_pad_script_path: Direct target Scratch Pad script path
            scratch_pad_owner_kind: system or emitter
            scratch_pad_script_index: Scratch Pad script index
            scratch_pad_name: Scratch Pad script name, defaults to RenderGrid
            scratch_pad_emitter_index: Optional emitter index for emitter-owned Scratch Pads
            scratch_pad_emitter_name: Optional emitter name for emitter-owned Scratch Pads
            target_node_guid: Optional target node GUID
            target_node_name: Optional target node name
            target_node_index: Optional target graph node index
            preserve_existing_link_as: Custom input name for the existing target-pin value link
            output_pin_name: Custom HLSL output pin name
            signature_name: Custom HLSL function signature name
            custom_hlsl_script_usage: Custom HLSL node script usage, DynamicInput by default; Function admits statement-style output assignments for generated/temp diagnostics
            inputs: Extra inputs linked from existing Scratch Pad graph pins
            user_parameter_inputs: Extra inputs created from User.* ParameterMapGet nodes
            skip_if_already_inserted: Return success when the same expression is already present
            replace_existing_custom: Replace an existing Custom HLSL wrapper
            rebuild_existing_custom: Preserve the existing base-value input when replacing a Custom node
            requires_context: Mark the Custom HLSL signature as context-requiring and auto-wire Map from the target ParameterMap source when no explicit Map input is passed
            delete_unlinked_custom_nodes: Delete unlinked Custom HLSL nodes after insertion
            delete_unlinked_custom_input_source_nodes: Delete now-unlinked input source nodes that belonged to the replaced Custom node
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "target_pin_name": target_pin_name,
                "expression": expression,
                "scratch_pad_script_path": scratch_pad_script_path,
                "scratch_pad_owner_kind": scratch_pad_owner_kind,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_emitter_name": scratch_pad_emitter_name,
                "target_node_guid": target_node_guid,
                "target_node_name": target_node_name,
                "preserve_existing_link_as": preserve_existing_link_as,
                "output_pin_name": output_pin_name,
                "signature_name": signature_name,
                "custom_hlsl_script_usage": custom_hlsl_script_usage,
                "skip_if_already_inserted": skip_if_already_inserted,
                "replace_existing_custom": replace_existing_custom,
                "rebuild_existing_custom": rebuild_existing_custom,
                "requires_context": requires_context,
                "delete_unlinked_custom_nodes": delete_unlinked_custom_nodes,
                "delete_unlinked_custom_input_source_nodes": delete_unlinked_custom_input_source_nodes,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            if scratch_pad_emitter_index is not None:
                params["scratch_pad_emitter_index"] = scratch_pad_emitter_index
            if target_node_index is not None:
                params["target_node_index"] = target_node_index
            if inputs is not None:
                params["inputs"] = inputs
            if user_parameter_inputs is not None:
                params["user_parameter_inputs"] = user_parameter_inputs
            return send_niagara_command("insert_niagara_scratch_pad_custom_hlsl_for_pin", params)
        except Exception as e:
            error_msg = f"Error inserting Niagara Scratch Pad Custom HLSL for pin: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def rewire_niagara_custom_hlsl_input_to_module_parameter(
        ctx: Context,
        system_path: str,
        input_name: str,
        scratch_pad_name: str = "",
        custom_node_signature_name: str = "",
        module_parameter_name: str = "",
        delete_orphaned_source_node: bool = True,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        timeout_seconds: float = 60.0,
    ) -> Dict[str, Any]:
        """
        Rewire a Scratch Pad Custom HLSL input to a Module.* ParameterMapGet pin.

        This is the middle step of the runtime DI chain proven for Array DI
        visibility: User parameter -> Module input -> HLSL input.

        Args:
            system_path: Niagara System package path or object path
            input_name: Custom HLSL input pin to rewire, e.g. Positions
            scratch_pad_name: Optional Scratch Pad script name filter
            custom_node_signature_name: Optional Custom HLSL signature filter
            module_parameter_name: Module parameter name to create/reuse; defaults to Module.<input_name>
            delete_orphaned_source_node: Delete the previous now-unlinked source node when safe
            allow_source_edit: Allow edits outside /Game/_MCP_Temp/NiagaraGenerated/
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            timeout_seconds: Per-command timeout for the Unreal bridge call
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "scratch_pad_name": scratch_pad_name,
                "custom_node_signature_name": custom_node_signature_name,
                "delete_orphaned_source_node": delete_orphaned_source_node,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if module_parameter_name:
                params["module_parameter_name"] = module_parameter_name
            return send_niagara_command(
                "rewire_niagara_custom_hlsl_input_to_module_parameter",
                params,
                timeout_seconds=timeout_seconds,
            )
        except Exception as e:
            error_msg = f"Error rewiring Niagara Custom HLSL input to Module parameter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_compile_status(
        ctx: Context,
        system_path: str,
        request_compile: bool = False,
        force: bool = False,
        allow_source_compile: bool = False,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Inspect Niagara script compile status and outstanding compile requests.

        Args:
            system_path: Niagara System package path or object path
            request_compile: Request a compile before returning status
            force: Force compile when request_compile is true
            allow_source_compile: Allow compile requests outside /Game/_MCP_Temp/NiagaraGenerated/
            wait_for_completion: Poll outstanding compile requests before returning
            timeout_seconds: Maximum wait time when wait_for_completion is true
            poll_interval_seconds: Poll interval when wait_for_completion is true
        """
        try:
            return send_niagara_command(
                "inspect_niagara_compile_status",
                {
                    "system_path": system_path,
                    "request_compile": request_compile,
                    "force": force,
                    "allow_source_compile": allow_source_compile,
                    "wait_for_completion": wait_for_completion,
                    "timeout_seconds": timeout_seconds,
                    "poll_interval_seconds": poll_interval_seconds,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara compile status: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_simulation_stages(
        ctx: Context,
        system_path: str,
        include_compile_data: bool = True,
        include_script_compile_status: bool = True,
        max_stages: int = 128,
    ) -> Dict[str, Any]:
        """
        Inspect Niagara SimulationStage settings and compiled stage data.

        Args:
            system_path: Niagara System package path or object path
            include_compile_data: Include FillCompilationData output for generic stages
            include_script_compile_status: Include per-stage script compile status
            max_stages: Maximum SimulationStages to include across all emitters
        """
        try:
            return send_niagara_command(
                "inspect_niagara_simulation_stages",
                {
                    "system_path": system_path,
                    "include_compile_data": include_compile_data,
                    "include_script_compile_status": include_script_compile_status,
                    "max_stages": max_stages,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara simulation stages: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_simulation_stage_settings(
        ctx: Context,
        system_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        stage_index: int | None = None,
        stage_name: str = "",
        enabled: bool | None = None,
        iteration_source: str = "",
        data_interface_name: str = "",
        direct_dispatch_type: str = "",
        direct_dispatch_element_type: str = "",
        execute_behavior: str = "",
        element_count: list[int] | None = None,
        element_count_x: int | None = None,
        element_count_y: int | None = None,
        element_count_z: int | None = None,
        num_iterations: int | None = None,
        num_iterations_binding: str = "",
        gpu_dispatch_force_linear: bool | None = None,
        override_gpu_dispatch_num_threads: bool | None = None,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Edit a target-local Niagara generic SimulationStage setting.

        Args:
            system_path: Niagara System package path or object path
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            stage_index: Optional SimulationStage index
            stage_name: Optional SimulationStage name
            enabled: Optional stage enabled state
            iteration_source: Optional Particles, DataInterface, or DirectSet
            data_interface_name: Optional namespaced Grid2D DI variable for DataInterface iteration stages
            direct_dispatch_type: Optional OneD, TwoD, or ThreeD
            direct_dispatch_element_type: Optional NumThreads, NumThreadsNoClipping, or NumGroups
            execute_behavior: Optional Always, OnSimulationReset, or NotOnSimulationReset
            element_count: Optional 1-3 integer array for direct dispatch
            element_count_x: Optional direct dispatch element count X
            element_count_y: Optional direct dispatch element count Y
            element_count_z: Optional direct dispatch element count Z
            num_iterations: Optional number of stage iterations
            num_iterations_binding: Optional existing User.* int parameter that dynamically controls stage iterations
            gpu_dispatch_force_linear: Optional force-linear GPU dispatch flag
            override_gpu_dispatch_num_threads: Optional custom thread-group override flag
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "emitter_name": emitter_name,
                "stage_name": stage_name,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if stage_index is not None:
                params["stage_index"] = stage_index
            if enabled is not None:
                params["enabled"] = enabled
            if iteration_source:
                params["iteration_source"] = iteration_source
            if data_interface_name:
                params["data_interface_name"] = data_interface_name
            if direct_dispatch_type:
                params["direct_dispatch_type"] = direct_dispatch_type
            if direct_dispatch_element_type:
                params["direct_dispatch_element_type"] = direct_dispatch_element_type
            if execute_behavior:
                params["execute_behavior"] = execute_behavior
            if element_count is not None:
                params["element_count"] = element_count
            if element_count_x is not None:
                params["element_count_x"] = element_count_x
            if element_count_y is not None:
                params["element_count_y"] = element_count_y
            if element_count_z is not None:
                params["element_count_z"] = element_count_z
            if num_iterations is not None:
                params["num_iterations"] = num_iterations
            if num_iterations_binding:
                params["num_iterations_binding"] = num_iterations_binding
            if gpu_dispatch_force_linear is not None:
                params["gpu_dispatch_force_linear"] = gpu_dispatch_force_linear
            if override_gpu_dispatch_num_threads is not None:
                params["override_gpu_dispatch_num_threads"] = override_gpu_dispatch_num_threads
            return send_niagara_command("set_niagara_simulation_stage_settings", params)
        except Exception as e:
            error_msg = f"Error setting Niagara simulation stage settings: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remove_niagara_simulation_stage(
        ctx: Context,
        system_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        stage_index: int | None = None,
        stage_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Remove one Niagara SimulationStage and its associated stage graph nodes.

        Args:
            system_path: Niagara System package path or object path
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            stage_index: Optional SimulationStage index
            stage_name: Optional SimulationStage name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after removal
            request_compile: Request Niagara compile after removal
            wait_for_completion: Poll compile completion before returning
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "emitter_name": emitter_name,
                "stage_name": stage_name,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if stage_index is not None:
                params["stage_index"] = stage_index
            return send_niagara_command(
                "remove_niagara_simulation_stage",
                params,
                timeout_seconds=timeout_seconds,
            )
        except Exception as e:
            error_msg = f"Error removing Niagara simulation stage: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def remove_niagara_scratch_pad_script(
        ctx: Context,
        system_path: str,
        scratch_pad_name: str = "",
        scratch_pad_owner_kind: str = "any",
        scratch_pad_script_index: int | None = None,
        scratch_pad_emitter_index: int | None = None,
        scratch_pad_emitter_name: str = "",
        force: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Remove a target-local Niagara Scratch Pad script by name/index.

        This is primarily for temp-system cleanup after graph-authoring probes.
        Production use requires allow_source_edit=True and should only happen
        after stack references have been removed.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "scratch_pad_name": scratch_pad_name,
                "scratch_pad_owner_kind": scratch_pad_owner_kind,
                "scratch_pad_emitter_name": scratch_pad_emitter_name,
                "force": force,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if scratch_pad_script_index is not None:
                params["scratch_pad_script_index"] = scratch_pad_script_index
            if scratch_pad_emitter_index is not None:
                params["scratch_pad_emitter_index"] = scratch_pad_emitter_index
            return send_niagara_command(
                "remove_niagara_scratch_pad_script",
                params,
                timeout_seconds=timeout_seconds,
            )
        except Exception as e:
            error_msg = f"Error removing Niagara Scratch Pad script: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def add_niagara_grid_simulation_stage(
        ctx: Context,
        system_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        stage_name: str = "MCP_GridSimulationStage",
        data_interface_name: str = "Emitter.Grid2D Collection",
        insert_index: int | None = None,
        skip_if_duplicate: bool = True,
        enabled: bool = True,
        iteration_source: str = "DataInterface",
        direct_dispatch_type: str = "",
        direct_dispatch_element_type: str = "",
        execute_behavior: str = "Always",
        element_count: list[int] | None = None,
        element_count_x: int | None = None,
        element_count_y: int | None = None,
        element_count_z: int | None = None,
        num_iterations: int = 1,
        gpu_dispatch_force_linear: bool | None = None,
        override_gpu_dispatch_num_threads: bool | None = None,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Add a generic Grid2D DataInterface SimulationStage and reset its stage graph.

        Args:
            system_path: Niagara System package path or object path
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            stage_name: New SimulationStage name
            data_interface_name: Grid2D Collection iteration source variable, e.g. Emitter.Grid2D Collection
            insert_index: Optional target stage index after creation
            skip_if_duplicate: Return the existing generic stage instead of failing
            enabled: Initial stage enabled state
            iteration_source: Must be DataInterface for this grid-stage command
            direct_dispatch_type: Optional OneD, TwoD, ThreeD, or Custom
            direct_dispatch_element_type: Optional NumThreads, NumThreadsNoClipping, or NumGroups
            execute_behavior: Always, OnSimulationReset, or NotOnSimulationReset
            element_count: Optional 1-3 integer array for direct dispatch metadata
            element_count_x/y/z: Optional explicit element counts
            num_iterations: Stage iteration count
            gpu_dispatch_force_linear: Optional force-linear GPU dispatch flag
            override_gpu_dispatch_num_threads: Optional custom thread-group override flag
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after creation
            request_compile: Request Niagara compile after creation
            wait_for_completion: Poll compile completion before returning
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "emitter_name": emitter_name,
                "stage_name": stage_name,
                "data_interface_name": data_interface_name,
                "skip_if_duplicate": skip_if_duplicate,
                "enabled": enabled,
                "iteration_source": iteration_source,
                "execute_behavior": execute_behavior,
                "num_iterations": num_iterations,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if insert_index is not None:
                params["insert_index"] = insert_index
            if direct_dispatch_type:
                params["direct_dispatch_type"] = direct_dispatch_type
            if direct_dispatch_element_type:
                params["direct_dispatch_element_type"] = direct_dispatch_element_type
            if element_count is not None:
                params["element_count"] = element_count
            if element_count_x is not None:
                params["element_count_x"] = element_count_x
            if element_count_y is not None:
                params["element_count_y"] = element_count_y
            if element_count_z is not None:
                params["element_count_z"] = element_count_z
            if gpu_dispatch_force_linear is not None:
                params["gpu_dispatch_force_linear"] = gpu_dispatch_force_linear
            if override_gpu_dispatch_num_threads is not None:
                params["override_gpu_dispatch_num_threads"] = override_gpu_dispatch_num_threads
            return send_niagara_command(
                "add_niagara_grid_simulation_stage",
                params,
                timeout_seconds=timeout_seconds,
            )
        except Exception as e:
            error_msg = f"Error adding Niagara grid simulation stage: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_emitter_local_space(
        ctx: Context,
        system_path: str,
        value: bool,
        emitter_index: int | None = None,
        emitter_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """Set a Niagara emitter's Local Space flag (FVersionedNiagaraEmitterData.bLocalSpace).

        Local space makes the particle field move rigidly with the component, so a
        camera-following weather system tracks smoothly (no world-space fill-in lag).
        Editor-only. Targets one emitter by emitter_index and/or emitter_name.

        Args:
            system_path: Niagara System package/object path
            value: True for local space, False for world space
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter display name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
        """
        try:
            params = {
                "system_path": system_path,
                "value": value,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if emitter_name:
                params["emitter_name"] = emitter_name
            return send_niagara_command("set_niagara_emitter_local_space", params)
        except Exception as e:
            error_msg = f"Error setting Niagara emitter local space: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_emitter_sim_target(
        ctx: Context,
        system_path: str,
        sim_target: str,
        auto_fixed_bounds: bool = True,
        emitter_index: int | None = None,
        emitter_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """Set a Niagara emitter's SimTarget ("CPU" or "GPU").

        GPU compute sim is far cheaper for high particle counts (weather, rain/snow).
        GPU cannot use Dynamic bounds; with auto_fixed_bounds=True the bounds mode is
        switched to Fixed (default bounds if none set) automatically. Editor-only.

        Args:
            system_path: Niagara System package/object path
            sim_target: "CPU" (CPUSim) or "GPU" (GPUComputeSim)
            auto_fixed_bounds: If GPU + Dynamic bounds, auto-switch to Fixed instead of erroring
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter display name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
        """
        try:
            params = {
                "system_path": system_path,
                "sim_target": sim_target,
                "auto_fixed_bounds": auto_fixed_bounds,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if emitter_name:
                params["emitter_name"] = emitter_name
            return send_niagara_command("set_niagara_emitter_sim_target", params)
        except Exception as e:
            error_msg = f"Error setting Niagara emitter sim target: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_emitter_fixed_bounds(
        ctx: Context,
        system_path: str,
        min: list | None = None,
        max: list | None = None,
        mode: str = "",
        emitter_index: int | None = None,
        emitter_name: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """Set a Niagara emitter's fixed bounds and/or CalculateBoundsMode.

        min/max are [x, y, z] world-space corners and must be provided together.
        mode is "Dynamic", "Fixed", or "Programmable"; providing min/max defaults the
        mode to Fixed. Fixed bounds are required for GPU emitters. Editor-only.

        Args:
            system_path: Niagara System package/object path
            min: [x, y, z] minimum corner (with max)
            max: [x, y, z] maximum corner (with min)
            mode: Optional CalculateBoundsMode (Dynamic|Fixed|Programmable)
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter display name
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request a compile after editing
        """
        try:
            params = {
                "system_path": system_path,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if min is not None:
                params["min"] = min
            if max is not None:
                params["max"] = max
            if mode:
                params["mode"] = mode
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if emitter_name:
                params["emitter_name"] = emitter_name
            return send_niagara_command("set_niagara_emitter_fixed_bounds", params)
        except Exception as e:
            error_msg = f"Error setting Niagara emitter fixed bounds: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_emitter_properties(
        ctx: Context,
        system_path: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
    ) -> Dict[str, Any]:
        """Read-only inspect of Niagara emitter properties.

        Returns local_space, sim_target, calculate_bounds_mode, fixed_bounds,
        determinism, and random_seed for each emitter (or one filtered by
        emitter_index/emitter_name). Use this instead of calling a setter with
        save=False to read emitter state. Editor-only.

        Args:
            system_path: Niagara System package/object path
            emitter_index: Optional filter to a single emitter index
            emitter_name: Optional filter to a single emitter display name
        """
        try:
            params = {"system_path": system_path}
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if emitter_name:
                params["emitter_name"] = emitter_name
            return send_niagara_command("inspect_niagara_emitter_properties", params)
        except Exception as e:
            error_msg = f"Error inspecting Niagara emitter properties: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_rapid_iteration_parameters(
        ctx: Context,
        system_path: str,
        contains: list[str] | None = None,
        max_parameters_per_script: int = 256,
    ) -> Dict[str, Any]:
        """Read-only dump of Niagara RapidIteration parameter values.

        Args:
            system_path: Niagara System package/object path
            contains: Optional case-insensitive name filters such as ["Sprite", "Lifetime"]
            max_parameters_per_script: Maximum matching values to report per script
        """
        try:
            return send_niagara_command(
                "inspect_niagara_rapid_iteration_parameters",
                {
                    "system_path": system_path,
                    "contains": contains or [],
                    "max_parameters_per_script": max_parameters_per_script,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara rapid iteration parameters: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_rapid_iteration_parameter(
        ctx: Context,
        system_path: str,
        parameter_name: str,
        value: Any,
        owner_kind: str = "",
        owner_name: str = "",
        script_usage: str = "",
        emitter_index: int | None = None,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Set one existing Niagara RapidIteration parameter by exact name.

        Args:
            system_path: Niagara System package/object path
            parameter_name: Exact RapidIteration parameter name from inspection
            value: New JSON value
            owner_kind: Optional owner filter, system or emitter
            owner_name: Optional owner display name filter
            script_usage: Optional Niagara script usage filter
            emitter_index: Optional emitter index filter
            allow_source_edit: Allow edits outside /Game/_MCP_Temp/NiagaraGenerated
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "parameter_name": parameter_name,
                "value": value,
                "owner_kind": owner_kind,
                "owner_name": owner_name,
                "script_usage": script_usage,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            return send_niagara_command("set_niagara_rapid_iteration_parameter", params)
        except Exception as e:
            error_msg = f"Error setting Niagara rapid iteration parameter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_module_static_switch(
        ctx: Context,
        system_path: str,
        input_name: str = "",
        value: Any | None = None,
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        expected_current_value: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = True,
        timeout_seconds: float = 60.0,
        poll_interval_seconds: float = 0.1,
        require_compile_gate_for_save: bool = True,
    ) -> Dict[str, Any]:
        """Set (or list) a Niagara module static-switch value on a particle/emitter module.

        Static switches (e.g. InitializeParticle "Sprite Size Mode") gate which inputs
        compile and are NOT RapidIteration-writable, so this is the only way to flip
        them via MCP. Enum values accept a display name, internal name, or exact integer
        value and are canonicalized against the pin's UEnum before mutation. Editor-only.

        Discovery: call with no input_name/value to list the module's static switches,
        each with current_value and (for enums) the available options.

        Args:
            system_path: Niagara System package/object path
            input_name: Static switch input name (omit to list)
            value: New enum display/internal name, enum integer, or boolean; omit to list
            emitter_index / emitter_name: Target emitter
            module_index / module_name / module_node_guid: Target module
            expected_current_value: Optional exact raw current_value stale-write guard
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save after editing
            request_compile: Request compile after editing
            wait_for_completion: Wait for Niagara compile completion before saving
            timeout_seconds: Compile wait timeout, clamped by the editor bridge
            poll_interval_seconds: Compile wait poll interval
            require_compile_gate_for_save: Require request+wait when saving a mutation
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
                "require_compile_gate_for_save": require_compile_gate_for_save,
            }
            if input_name:
                params["input_name"] = input_name
            if value is not None and value != "":
                params["value"] = value
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if emitter_name:
                params["emitter_name"] = emitter_name
            if module_index is not None:
                params["module_index"] = module_index
            if module_name:
                params["module_name"] = module_name
            if module_node_guid:
                params["module_node_guid"] = module_node_guid
            if expected_current_value:
                params["expected_current_value"] = expected_current_value
            return send_niagara_command("set_niagara_module_static_switch", params)
        except Exception as e:
            error_msg = f"Error setting Niagara module static switch: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_sprite_renderer_settings(
        ctx: Context,
        system_path: str,
        alignment: str = "",
        facing_mode: str = "",
        sub_image_size: list | None = None,
        emitter_index: int | None = None,
        emitter_name: str = "",
        renderer_index: int | None = None,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """Set sprite renderer settings on a Niagara emitter (alignment / facing / SubUV).

        Use alignment="VelocityAligned" so sprites stretch along velocity (rain streaks).
        sub_image_size is [x, y] SubUV grid. Only the provided fields change. Editor-only.

        Args:
            system_path: Niagara System package/object path
            alignment: Unaligned | VelocityAligned | CustomAlignment | Automatic
            facing_mode: FaceCamera | FaceCameraPlane | CustomFacingVector | FaceCameraPosition | FaceCameraDistanceBlend | Automatic
            sub_image_size: [x, y] SubUV grid size
            emitter_index / emitter_name: Target emitter
            renderer_index: Target renderer index
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save after editing
            request_compile: Request compile after editing
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            if alignment:
                params["alignment"] = alignment
            if facing_mode:
                params["facing_mode"] = facing_mode
            if sub_image_size is not None:
                params["sub_image_size"] = sub_image_size
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if emitter_name:
                params["emitter_name"] = emitter_name
            if renderer_index is not None:
                params["renderer_index"] = renderer_index
            return send_niagara_command("set_niagara_sprite_renderer_settings", params)
        except Exception as e:
            error_msg = f"Error setting Niagara sprite renderer settings: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_module_inputs(
        ctx: Context,
        system_path: str,
        include_linked_sources: bool = True,
        include_resolved_stack_inputs: bool = False,
        max_modules: int = 200,
        max_candidates_per_module: int = 24,
        max_resolved_inputs_per_module: int = 8,
        max_top_candidates: int = 80,
    ) -> Dict[str, Any]:
        """
        Inspect Niagara module input candidates for generation planning.

        Args:
            system_path: Niagara System package path or object path
            include_linked_sources: Include linked script/source metadata
            include_resolved_stack_inputs: Include resolved stack input values
            max_modules: Maximum modules to inspect per emitter
            max_candidates_per_module: Maximum editable-looking candidates per module
            max_resolved_inputs_per_module: Maximum resolved inputs per module
            max_top_candidates: Maximum prioritized candidates in the top list
        """
        try:
            return send_niagara_command(
                "inspect_niagara_module_inputs",
                {
                    "system_path": system_path,
                    "include_linked_sources": include_linked_sources,
                    "include_resolved_stack_inputs": include_resolved_stack_inputs,
                    "max_modules": max_modules,
                    "max_candidates_per_module": max_candidates_per_module,
                    "max_resolved_inputs_per_module": max_resolved_inputs_per_module,
                    "max_top_candidates": max_top_candidates,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Niagara module inputs: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def inspect_niagara_data_interface_overrides(
        ctx: Context,
        system_path: str,
        input_name: str = "",
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        max_modules: int = 200,
        max_inputs_per_module: int = 64,
        include_data_interface_properties: bool = False,
        max_data_interface_properties: int = 120,
        max_data_interface_property_value_length: int = 512,
    ) -> Dict[str, Any]:
        """
        Inspect Niagara Data Interface module input overrides and User object bindings.

        Args:
            system_path: Niagara System package path or object path
            input_name: Optional module input name filter, with or without the Module. prefix
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            max_modules: Maximum matching modules to include per emitter
            max_inputs_per_module: Maximum Data Interface inputs to include per module
            include_data_interface_properties: Include reflected editable Data Interface properties
            max_data_interface_properties: Maximum reflected Data Interface properties to include
            max_data_interface_property_value_length: Maximum reflected property value string length
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "max_modules": max_modules,
                "max_inputs_per_module": max_inputs_per_module,
                "include_data_interface_properties": include_data_interface_properties,
                "max_data_interface_properties": max_data_interface_properties,
                "max_data_interface_property_value_length": max_data_interface_property_value_length,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("inspect_niagara_data_interface_overrides", params)
        except Exception as e:
            error_msg = f"Error inspecting Niagara data interface overrides: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_module_input_value(
        ctx: Context,
        system_path: str,
        input_name: str,
        value: Any,
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Set an existing Niagara RapidIteration module input override.

        Args:
            system_path: Niagara System package path or object path
            input_name: Module input name, with or without the Module. prefix
            value: New JSON value. Only existing RapidIteration values are edited.
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "value": value,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("set_niagara_module_input_value", params)
        except Exception as e:
            error_msg = f"Error setting Niagara module input value: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def create_niagara_module_input_override(
        ctx: Context,
        system_path: str,
        input_name: str,
        value: Any,
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Create a missing Niagara RapidIteration module input override.

        Args:
            system_path: Niagara System package path or object path
            input_name: Module input name, with or without the Module. prefix
            value: Initial JSON value for the new override
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            overwrite_existing: Allow replacing an existing override
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "value": value,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "overwrite_existing": overwrite_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("create_niagara_module_input_override", params)
        except Exception as e:
            error_msg = f"Error creating Niagara module input override: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_module_input_literal_override(
        ctx: Context,
        system_path: str,
        input_name: str,
        value: Any,
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Set a Niagara module input literal value through an override-pin default string.

        Args:
            system_path: Niagara System package path or object path
            input_name: Module input name, with or without the Module. prefix
            value: New JSON literal value for scalar/vector/color/bool inputs
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            overwrite_existing: Replace an existing override-pin default string
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "value": value,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "overwrite_existing": overwrite_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("set_niagara_module_input_literal_override", params)
        except Exception as e:
            error_msg = f"Error setting Niagara module input literal override: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_render_target2d_module_input(
        ctx: Context,
        system_path: str,
        input_name: str,
        user_parameter_name: str = "User.RT_IF_Deform",
        render_target_asset_path: str = "",
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        inherit_user_parameter_settings: bool = True,
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Bind a Niagara RenderTarget2D data-interface module input to a User render target parameter.

        Args:
            system_path: Niagara System package path or object path
            input_name: RenderTarget2D module input name, with or without the Module. prefix
            user_parameter_name: User render target parameter, default User.RT_IF_Deform
            render_target_asset_path: Optional TextureRenderTarget asset to store as the User parameter default
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            inherit_user_parameter_settings: Use size/format/settings from the User render target asset
            overwrite_existing: Replace an existing linked override for the module input
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "user_parameter_name": user_parameter_name,
                "render_target_asset_path": render_target_asset_path,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "inherit_user_parameter_settings": inherit_user_parameter_settings,
                "overwrite_existing": overwrite_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("set_niagara_render_target2d_module_input", params)
        except Exception as e:
            error_msg = f"Error binding Niagara RenderTarget2D module input: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_texture2d_module_input(
        ctx: Context,
        system_path: str,
        input_name: str,
        user_parameter_name: str = "User.T_Mask",
        texture_asset_path: str = "",
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Bind a Niagara Texture2D data-interface module input to a User texture parameter.

        Args:
            system_path: Niagara System package path or object path
            input_name: Texture2D module input name, with or without the Module. prefix
            user_parameter_name: User texture parameter, default User.T_Mask
            texture_asset_path: Optional Texture2D asset to store as the User parameter default
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            overwrite_existing: Replace an existing linked override for the module input
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "user_parameter_name": user_parameter_name,
                "texture_asset_path": texture_asset_path,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "overwrite_existing": overwrite_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("set_niagara_texture2d_module_input", params)
        except Exception as e:
            error_msg = f"Error binding Niagara Texture2D module input: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_data_interface_module_input(
        ctx: Context,
        system_path: str,
        input_name: str,
        properties: Dict[str, Any] | None = None,
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Create a local Data Interface override for a DI-typed module input and set its properties.

        The DI instance class is taken from the module input's own type (for example
        NiagaraDataInterfaceParticleRead for Particle Attribute Reader inputs), and each
        entry in `properties` is imported into the instance through UProperty reflection
        (string / bool / number JSON values; e.g. {"EmitterName": "Droplets"}).

        Args:
            system_path: Niagara System package path or object path
            input_name: DI module input name, with or without the Module. prefix
            properties: Optional {property_name: value} map applied to the created DI instance
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            overwrite_existing: Replace an existing linked override for the module input
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "overwrite_existing": overwrite_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if properties:
                params["properties"] = properties
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("set_niagara_data_interface_module_input", params)
        except Exception as e:
            error_msg = f"Error setting Niagara Data Interface module input: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def configure_niagara_data_channel_asset(
        ctx: Context,
        asset_path: str,
        variables: list[Dict[str, str]] | None = None,
        create_if_missing: bool = True,
        keep_previous_frame_data: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Create or update a Niagara Data Channel asset (Global handler).

        Sets the channel's variable layout and bKeepPreviousFrameData. Variables replace
        the existing layout wholesale when provided.

        Args:
            asset_path: NDC asset package path (e.g. /Game/_MCP_Temp/.../NDC_Foo)
            variables: List of {"name": ..., "type": ...}; types like "Position",
                "Vector", "float", "int", "bool", "LinearColor"
            create_if_missing: Create the asset if it does not exist
            keep_previous_frame_data: Keep previous frame data (lets readers accept one
                frame of latency instead of tick-order dependency)
            save: Save the asset after editing
        """
        try:
            params: Dict[str, Any] = {
                "asset_path": asset_path,
                "create_if_missing": create_if_missing,
                "keep_previous_frame_data": keep_previous_frame_data,
                "save": save,
            }
            if variables:
                params["variables"] = variables
            return send_niagara_command("configure_niagara_data_channel_asset", params)
        except Exception as e:
            error_msg = f"Error configuring Niagara data channel asset: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def create_niagara_ndc_reader_scratch_pad_module(
        ctx: Context,
        system_path: str,
        scratch_pad_name: str,
        data_channel_asset_path: str,
        stage: str = "particle_spawn_read",
        position_variable: str = "Position",
        velocity_variable: str = "Velocity",
        spawn_count_variable: str = "SpawnCount",
        event_type_variable: str = "EventType",
        position_target: str = "Particles.Position",
        velocity_target: str = "Particles.Velocity",
        spawn_count_target: str = "Particles.NDCSpawnCount",
        event_type_target: str = "Particles.NDCEventType",
        read_success_target: str = "Particles.NDCReadSuccess",
        read_current_frame: bool = False,
        dry_run: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
    ) -> Dict[str, Any]:
        """Create one fresh, system-owned Scratch Pad module for a guarded NDC reader pipeline.

        This mirrors the three module graphs generated by UE 5.6's Niagara Data Channel
        Wizard. Call it once per stage, in canonical order:

        * ``emitter_spawn_init`` exposes ``Module.DataChannel`` and stores it in
          ``Emitter.SpawnDataChannel``.
        * ``emitter_update_spawn`` calls ``SpawnDirect`` using the selected int
          channel variable (for example ``DebrisCount`` or ``DustCount``).
        * ``particle_spawn_read`` calls ``GetNDCSpawnData`` then variadic ``Read`` and
          maps Position, Velocity, SpawnCount, and EventType to particle parameters.

        The command validates a Global NDC and its exact required field types before
        mutation, creates from UE's pristine four-node DefaultModuleScript in transient
        memory, refuses every existing Scratch Pad name, and edits generated/temp systems
        only unless explicitly allowed. It intentionally does not insert the module into
        an emitter stack or bind the first module's DI override; use the structured
        ``required_follow_up`` response with ``add_scratch_pad_module_to_stack`` and
        ``set_niagara_data_interface_module_input``.

        Args:
            system_path: Existing target Niagara System package or object path.
            scratch_pad_name: New ASCII-only Scratch Pad name; overwrite is never allowed.
            data_channel_asset_path: Existing Global Niagara Data Channel asset.
            stage: emitter_spawn_init, emitter_update_spawn, or particle_spawn_read.
            position_variable: Position-typed NDC field name.
            velocity_variable: Vector-typed NDC field name.
            spawn_count_variable: Int-typed NDC field used by SpawnDirect.
            event_type_variable: Int-typed NDC event discriminator.
            position_target: Particle target for Position reads.
            velocity_target: Particle target for Velocity reads.
            spawn_count_target: Particle target retaining the source SpawnCount.
            event_type_target: Particle target retaining EventType.
            read_success_target: Particle bool target for the Read success output.
            read_current_frame: DI override value to use during the documented follow-up.
                False requires the channel to keep previous-frame data.
            dry_run: Validate system, name, template, signatures, and NDC schema only.
            allow_source_edit: Allow editing systems outside /Game/_MCP_Temp/NiagaraGenerated/.
            save: Save the target system after adding the authored Scratch Pad.
            request_compile: Request Niagara compilation after adding the Scratch Pad.
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "scratch_pad_name": scratch_pad_name,
                "data_channel_asset_path": data_channel_asset_path,
                "stage": stage,
                "position_variable": position_variable,
                "velocity_variable": velocity_variable,
                "spawn_count_variable": spawn_count_variable,
                "event_type_variable": event_type_variable,
                "position_target": position_target,
                "velocity_target": velocity_target,
                "spawn_count_target": spawn_count_target,
                "event_type_target": event_type_target,
                "read_success_target": read_success_target,
                "read_current_frame": read_current_frame,
                "dry_run": dry_run,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
            }
            return send_niagara_command(
                "create_niagara_ndc_reader_scratch_pad_module",
                params,
            )
        except Exception as e:
            error_msg = f"Error creating Niagara NDC reader Scratch Pad module: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_module_input_user_parameter(
        ctx: Context,
        system_path: str,
        input_name: str,
        user_parameter_name: str,
        default_value: Any | None = None,
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Bind a Niagara scalar/vector/color/bool module input to a User parameter.

        Args:
            system_path: Niagara System package path or object path
            input_name: Module input name, with or without the Module. prefix
            user_parameter_name: User parameter name, with or without the User. prefix
            default_value: Optional default value for the User parameter
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            overwrite_existing: Replace an existing linked override for the module input
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "user_parameter_name": user_parameter_name,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "overwrite_existing": overwrite_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if default_value is not None:
                params["default_value"] = default_value
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("set_niagara_module_input_user_parameter", params)
        except Exception as e:
            error_msg = f"Error binding Niagara module input to User parameter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_array_data_interface_module_input_user_parameter(
        ctx: Context,
        system_path: str,
        input_name: str,
        user_parameter_name: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Bind a supported Niagara Array Data Interface module input to a User parameter.

        Args:
            system_path: Niagara System package path or object path
            input_name: Module input name, with or without the Module. prefix
            user_parameter_name: User parameter name, with or without the User. prefix
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            overwrite_existing: Replace an existing linked override for the module input
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "user_parameter_name": user_parameter_name,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "overwrite_existing": overwrite_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("set_niagara_array_data_interface_module_input_user_parameter", params)
        except Exception as e:
            error_msg = f"Error binding Niagara Array Data Interface module input to User parameter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_module_input_linked_parameter(
        ctx: Context,
        system_path: str,
        input_name: str,
        linked_parameter_name: str,
        emitter_index: int | None = None,
        emitter_name: str = "",
        module_index: int | None = None,
        module_name: str = "",
        module_node_guid: str = "",
        overwrite_existing: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Link a Niagara module input to an existing namespaced Niagara parameter.

        Args:
            system_path: Niagara System package path or object path
            input_name: Module input name, with or without the Module. prefix
            linked_parameter_name: Existing parameter name, e.g. Emitter.Grid2D Collection
            emitter_index: Optional target emitter index
            emitter_name: Optional target emitter name
            module_index: Optional target module index from inspect_niagara_module_inputs
            module_name: Optional target module name
            module_node_guid: Optional target module node GUID
            overwrite_existing: Replace an existing linked override for the module input
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System after editing
            request_compile: Request Niagara compile after editing
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            params: Dict[str, Any] = {
                "system_path": system_path,
                "input_name": input_name,
                "linked_parameter_name": linked_parameter_name,
                "emitter_name": emitter_name,
                "module_name": module_name,
                "module_node_guid": module_node_guid,
                "overwrite_existing": overwrite_existing,
                "allow_source_edit": allow_source_edit,
                "save": save,
                "request_compile": request_compile,
                "wait_for_completion": wait_for_completion,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
            }
            if emitter_index is not None:
                params["emitter_index"] = emitter_index
            if module_index is not None:
                params["module_index"] = module_index
            return send_niagara_command("set_niagara_module_input_linked_parameter", params)
        except Exception as e:
            error_msg = f"Error linking Niagara module input to parameter: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def set_niagara_module_inputs_batch(
        ctx: Context,
        system_path: str,
        edits: list[Dict[str, Any]],
        operation: str = "set_existing",
        overwrite_existing: bool = False,
        continue_on_error: bool = False,
        allow_source_edit: bool = False,
        save: bool = True,
        request_compile: bool = True,
        wait_for_completion: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.1,
    ) -> Dict[str, Any]:
        """
        Apply multiple Niagara RapidIteration module input edits in one command.

        Args:
            system_path: Niagara System package path or object path
            edits: Edit objects with selectors, input_name, value, and optional operation
            operation: Default operation: set_existing, create_override, or upsert
            overwrite_existing: Default overwrite flag for create/upsert operations
            continue_on_error: Continue processing after an edit failure
            allow_source_edit: Allow edits outside /Game/_MCP_Temp
            save: Save the Niagara System once after successful edits
            request_compile: Request Niagara compile after successful edits
            wait_for_completion: Wait for compile completion
            timeout_seconds: Compile wait timeout
            poll_interval_seconds: Compile wait poll interval
        """
        try:
            return send_niagara_command(
                "set_niagara_module_inputs_batch",
                {
                    "system_path": system_path,
                    "edits": edits,
                    "operation": operation,
                    "overwrite_existing": overwrite_existing,
                    "continue_on_error": continue_on_error,
                    "allow_source_edit": allow_source_edit,
                    "save": save,
                    "request_compile": request_compile,
                    "wait_for_completion": wait_for_completion,
                    "timeout_seconds": timeout_seconds,
                    "poll_interval_seconds": poll_interval_seconds,
                },
            )
        except Exception as e:
            error_msg = f"Error batch setting Niagara module input values: {e}"
            logger.error(error_msg)
            return _niagara_error(error_msg)

    @mcp.tool()
    def cleanup_generated_niagara_assets_offline(
        ctx: Context,
        package_paths: list[str] | None = None,
        name_prefix: str = "",
        older_than_minutes: float | None = None,
        dry_run: bool = True,
        require_editor_closed: bool = True,
        allow_all: bool = False,
    ) -> Dict[str, Any]:
        """
        Clean generated Niagara temp asset files after the editor is closed.

        Args:
            package_paths: Explicit generated Niagara package/object paths under /Game/_MCP_Temp/NiagaraGenerated
            name_prefix: Optional asset-name prefix to match under the generated Niagara temp root
            older_than_minutes: Optional minimum file age before selection
            dry_run: Report selected files without deleting them by default
            require_editor_closed: Refuse real deletion while UnrealEditor.exe is running
            allow_all: Permit selecting the whole generated Niagara temp root when no other selector is provided
        """
        return cleanup_generated_niagara_assets_offline_impl(
            package_paths=package_paths,
            name_prefix=name_prefix,
            older_than_minutes=older_than_minutes,
            dry_run=dry_run,
            require_editor_closed=require_editor_closed,
            allow_all=allow_all,
        )

    logger.info("Niagara tools registered successfully")
