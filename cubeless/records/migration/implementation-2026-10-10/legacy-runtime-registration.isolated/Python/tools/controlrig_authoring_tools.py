"""ControlRig recipe authoring adapter tools for Unreal MCP."""

import json
import logging
import os
import re
import tempfile
import textwrap
import threading
from copy import deepcopy
from functools import wraps
from typing import Any, Callable, Dict, List, Optional, Sequence
from uuid import uuid4

from mcp.server.fastmcp import Context, FastMCP

from services.unreal_bridge_connection import unreal_python_result_unknown_error
from tools.dependency_guard import reject_mcp_dependency_references
from tools.mutation_result_contract import composite_failure_safety_fields
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")
_CONTROLRIG_COMPOSITE_LOCK = threading.RLock()


def _serialize_controlrig_composite(func: Any) -> Any:
    """Prevent one multi-stage ControlRig/AnimBP recipe from interleaving another."""

    @wraps(func)
    def wrapped(*args: Any, **kwargs: Any) -> Dict[str, Any]:
        if not _CONTROLRIG_COMPOSITE_LOCK.acquire(blocking=False):
            return tool_error(
                "Another ControlRig binding workflow is already running",
                error_code="controlrig_composite_already_running",
                mutation_attempted=False,
                mutation_performed=False,
                changed=False,
            )
        try:
            return func(*args, **kwargs)
        finally:
            _CONTROLRIG_COMPOSITE_LOCK.release()

    return wrapped


def _unreal_connection_error(**fields: Any) -> Dict[str, Any]:
    return tool_error(
        "Failed to connect to Unreal Engine",
        error_code="unreal_connection_failed",
        **fields,
    )


def _unreal_no_response_error(**fields: Any) -> Dict[str, Any]:
    return tool_error(
        "No response from Unreal Engine",
        error_code="unreal_no_response",
        **fields,
    )


def _controlrig_exception_error(
    message: str,
    exc: Exception,
    *,
    error_code: str = "controlrig_authoring_tool_failed",
    **fields: Any,
) -> Dict[str, Any]:
    return tool_error(
        message,
        error_code=error_code,
        details={"exception_type": type(exc).__name__},
        **fields,
    )


def _json_payload(value: Any, field_name: str) -> str:
    if isinstance(value, str):
        parsed = json.loads(value)
        reject_mcp_dependency_references(field_name, parsed)
        return json.dumps(parsed, ensure_ascii=False)

    reject_mcp_dependency_references(field_name, value)
    return json.dumps(value, ensure_ascii=False)


def _json_object_payload(value: Any, field_name: str) -> Dict[str, Any]:
    if isinstance(value, str):
        parsed = json.loads(value)
    else:
        parsed = value

    if not isinstance(parsed, dict):
        raise ValueError(f"{field_name} must be a JSON object")

    reject_mcp_dependency_references(field_name, parsed)
    return parsed


def _run_unreal_python_json(code_body: str, result_name: str) -> Dict[str, Any]:
    """Run Unreal Python code that writes RESULT to a temp JSON file."""
    from unreal_mcp_server import get_unreal_connection

    safe_name = "".join(ch if ch.isalnum() or ch in ("_", "-") else "_" for ch in result_name)
    result_path = os.path.join(
        tempfile.gettempdir(),
        f"unreal_mcp_{safe_name}_{uuid4().hex}_result.json",
    )

    wrapped_code = f"""
import json
import traceback
RESULT_PATH = {result_path!r}
RESULT = None
try:
{textwrap.indent(code_body, "    ")}
    if RESULT is None:
        RESULT = {{"success": True}}
except Exception as exc:
    RESULT = {{"success": False, "message": str(exc), "traceback": traceback.format_exc()}}
with open(RESULT_PATH, "w", encoding="utf-8") as result_file:
    json.dump(RESULT, result_file, ensure_ascii=False, indent=2, default=str)
"""

    try:
        unreal = get_unreal_connection()
        if not unreal:
            return _unreal_connection_error()

        response = unreal.send_command(
            "execute_python",
            {
                "code": wrapped_code,
                "mode": "ExecuteFile",
                "interchange_policy": "none",
            },
        )
        if not response:
            return _unreal_no_response_error()
        if response.get("status") == "error":
            return tool_error(
                str(response.get("error") or "Unreal Python execution failed"),
                error_code="controlrig_authoring_unreal_python_failed",
                unreal_response=response,
            )

        if not os.path.exists(result_path):
            return unreal_python_result_unknown_error(
                "Unreal Python did not write a result file",
                error_code="controlrig_authoring_missing_result_file",
                unreal_response=response,
                failure_kind="side_channel_result_missing",
            )

        try:
            with open(result_path, "r", encoding="utf-8") as result_file:
                result = json.load(result_file)
            if not isinstance(result, dict):
                raise ValueError("Unreal Python result must be a JSON object")
        except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as result_error:
            return unreal_python_result_unknown_error(
                "Unreal Python wrote an unreadable result file",
                error_code="controlrig_authoring_invalid_result_file",
                unreal_response=response,
                failure_kind="side_channel_result_invalid",
                exception=result_error,
            )
        if result.get("success") is False and result.get("traceback"):
            unknown_result = unreal_python_result_unknown_error(
                str(result.get("message") or "Unreal Python execution raised after it began"),
                error_code="controlrig_authoring_execution_outcome_unknown",
                unreal_response=response,
                failure_kind="side_channel_execution_exception",
            )
            unknown_result["unreal_python_result"] = result
            return unknown_result
        result["unreal_response"] = response
        return result
    finally:
        try:
            os.remove(result_path)
        except FileNotFoundError:
            pass
        except OSError as exc:
            logger.warning("Failed to remove ControlRig result file %s: %s", result_path, exc)


def _call_authoring_subsystem(operation: str, recipe_json: str, options_json: str = "") -> Dict[str, Any]:
    code = f"""
import json
import unreal

operation = {operation!r}
recipe_json = {recipe_json!r}
options_json = {options_json!r}

subsystem_class = getattr(unreal, "UnrealMCPControlRigAuthoringSubsystem", None)
authoring_subsystem_name = "UnrealMCPControlRigAuthoringSubsystem"
if subsystem_class is None:
    subsystem_class = getattr(unreal, "CubelessControlRigAuthoringSubsystem", None)
    authoring_subsystem_name = "CubelessControlRigAuthoringSubsystem"
if subsystem_class is None:
    RESULT = {{
        "success": False,
        "plugin_unavailable": True,
        "message": "UnrealMCPControlRigToolsEditor module is not loaded in the editor.",
    }}
else:
    subsystem = unreal.get_editor_subsystem(subsystem_class)
    if subsystem is None:
        RESULT = {{
            "success": False,
            "plugin_unavailable": True,
            "message": authoring_subsystem_name + " is unavailable.",
        }}
    elif operation == "validate":
        report_json = subsystem.validate_recipe_json(recipe_json)
        RESULT = json.loads(report_json)
    elif operation == "dry_run":
        report_json = subsystem.dry_run_recipe_json(recipe_json)
        RESULT = json.loads(report_json)
    elif operation == "build":
        if options_json:
            report_json = subsystem.build_recipe_json_with_options(recipe_json, options_json)
        else:
            report_json = subsystem.build_recipe_json(recipe_json)
        RESULT = json.loads(report_json)
    elif operation == "make_preset":
        report_json = subsystem.make_preset_recipe_json(recipe_json)
        RESULT = json.loads(report_json)
    elif operation == "profile_skeleton":
        profile_method = getattr(subsystem, "profile_skeleton_json", None)
        if profile_method is None:
            RESULT = {{
                "success": False,
                "plugin_unavailable": True,
                "message": authoring_subsystem_name + ".profile_skeleton_json is unavailable.",
            }}
        else:
            report_json = profile_method(recipe_json)
            RESULT = json.loads(report_json)
    else:
        RESULT = {{"success": False, "message": "Unsupported operation: " + operation}}

    if isinstance(RESULT, dict):
        RESULT["authoring_plugin_used"] = not bool(RESULT.get("plugin_unavailable"))
        RESULT["authoring_subsystem"] = authoring_subsystem_name
        RESULT["legacy_authoring_subsystem_fallback"] = authoring_subsystem_name == "CubelessControlRigAuthoringSubsystem"
        RESULT["adapter"] = "python_reflection"
"""
    return _run_unreal_python_json(code, f"controlrig_authoring_{operation}")


def _send_unreal_command(command: str, params: Dict[str, Any], timeout_seconds: Optional[int] = None) -> Dict[str, Any]:
    from unreal_mcp_server import get_unreal_connection

    unreal = get_unreal_connection()
    if not unreal:
        return {"status": "error", "error": "Failed to connect to Unreal Engine"}
    return unreal.send_command(command, params, timeout_seconds=timeout_seconds) or {
        "status": "error",
        "error": "No response from Unreal Engine",
    }


def _bridge_response_result(response: Dict[str, Any]) -> Dict[str, Any]:
    result = response.get("result") if isinstance(response, dict) else None
    return result if isinstance(result, dict) else {}


def _bridge_response_success(response: Dict[str, Any]) -> bool:
    if not isinstance(response, dict) or response.get("status") == "error" or response.get("success") is False:
        return False
    result = _bridge_response_result(response)
    return result.get("success") is not False


def _bridge_response_mutation_performed(response: Dict[str, Any]) -> bool:
    """Return true only when a native response reports concrete change evidence."""

    sources = [response, _bridge_response_result(response)]
    mutation_fields = (
        "mutation_performed",
        "changed",
        "target_modified",
        "partial_target_modified",
        "target_created",
        "graph_changed",
        "defaults_changed",
        "visibility_changed",
        "links_changed",
    )
    return any(source.get(field) is True for source in sources for field in mutation_fields)


def _is_sample_or_temp_path(path: str) -> bool:
    return path.startswith("/Game/_MCP_Sample/") or path.startswith("/Game/_MCP_Temp/")


def _stage(name: str, success: bool, **fields: Any) -> Dict[str, Any]:
    item = {"name": name, "success": success}
    item.update(fields)
    return item


def _compact_bone_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", name.lower())


def _normalized_bone_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


def _bone_side(name: str) -> str:
    compact = _compact_bone_name(name)
    normalized = _normalized_bone_name(name)
    tokens = [token for token in normalized.split("_") if token]

    if "left" in tokens or compact.startswith("left") or compact.endswith("left"):
        return "left"
    if "right" in tokens or compact.startswith("right") or compact.endswith("right"):
        return "right"
    if tokens and tokens[-1] in {"l", "lf"}:
        return "left"
    if tokens and tokens[-1] in {"r", "rt"}:
        return "right"
    if compact.endswith("l") and any(marker in compact for marker in ("arm", "hand", "leg", "foot", "toe", "clavicle", "shoulder", "thigh", "calf")):
        return "left"
    if compact.endswith("r") and any(marker in compact for marker in ("arm", "hand", "leg", "foot", "toe", "clavicle", "shoulder", "thigh", "calf")):
        return "right"
    return ""


def _record_for_bone(raw: Dict[str, Any]) -> Dict[str, Any]:
    name = str(raw.get("name") or "")
    return {
        "name": name,
        "index": int(raw.get("index", -1)),
        "parent_name": str(raw.get("parent_name") or ""),
        "path": str(raw.get("path") or name),
        "depth": int(raw.get("depth", 0)),
        "side": _bone_side(name),
        "_compact": _compact_bone_name(name),
    }


def _matches_any(record: Dict[str, Any], terms: Sequence[str], side: str = "") -> bool:
    compact = str(record.get("_compact") or "")
    if side and record.get("side") != side:
        return False
    return any(term in compact for term in terms)


def _candidate_payload(record: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "name": record["name"],
        "index": record["index"],
        "parent_name": record["parent_name"],
        "path": record["path"],
    }


def _single_alias(
    records: Sequence[Dict[str, Any]],
    alias_name: str,
    terms: Sequence[str],
    *,
    side: str = "",
) -> Dict[str, Any]:
    candidates = [_candidate_payload(record) for record in records if _matches_any(record, terms, side)]
    if not candidates:
        status = "missing"
    elif len(candidates) == 1:
        status = "resolved"
    else:
        status = "ambiguous"
    return {
        "alias": alias_name,
        "status": status,
        "selected": candidates[0]["name"] if candidates else "",
        "candidates": candidates[:8],
        "candidate_count": len(candidates),
        "match_terms": list(terms),
    }


def _chain_alias(
    records: Sequence[Dict[str, Any]],
    alias_name: str,
    segment_terms: Dict[str, Sequence[str]],
    *,
    side: str = "",
    multi_match_segments: Sequence[str] = (),
) -> Dict[str, Any]:
    segment_matches: Dict[str, List[Dict[str, Any]]] = {}
    chain_records: List[Dict[str, Any]] = []
    missing_segments: List[str] = []
    ambiguous_segments: List[str] = []
    multi_match_set = set(multi_match_segments)

    for segment, terms in segment_terms.items():
        matches = [record for record in records if _matches_any(record, terms, side)]
        if not matches:
            missing_segments.append(segment)
        elif len(matches) > 1 and segment not in multi_match_set:
            ambiguous_segments.append(segment)
        segment_matches[segment] = [_candidate_payload(record) for record in matches[:8]]
        chain_records.extend(matches)

    unique_by_index = {record["index"]: record for record in chain_records}
    chain = [
        _candidate_payload(record)
        for record in sorted(unique_by_index.values(), key=lambda item: (item["depth"], item["index"], item["name"]))
    ]

    if not chain:
        status = "missing"
    elif ambiguous_segments:
        status = "ambiguous"
    elif missing_segments:
        status = "partial"
    else:
        status = "resolved"

    return {
        "alias": alias_name,
        "status": status,
        "side": side,
        "chain": chain,
        "segment_matches": segment_matches,
        "missing_segments": missing_segments,
        "ambiguous_segments": ambiguous_segments,
    }


def _recommend_preset(
    preset: str,
    ready: bool,
    reason: str,
    required_aliases: Sequence[str],
    *,
    confidence: str = "medium",
) -> Dict[str, Any]:
    return {
        "preset": preset,
        "ready": ready,
        "confidence": confidence if ready else "blocked",
        "reason": reason,
        "required_aliases": list(required_aliases),
        "phase_1_only": True,
        "adds_solver_units": False,
    }


def _stable_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _issue(code: str, message: str, field_path: str, severity: str = "error") -> Dict[str, str]:
    return {
        "severity": severity,
        "code": code,
        "message": message,
        "field_path": field_path,
    }


_SOLVER_GATE_VALIDATION_ONLY_STATUS = "validation_only_no_sample"


def _default_solver_gate() -> Dict[str, Any]:
    return {
        "status": _SOLVER_GATE_VALIDATION_ONLY_STATUS,
        "allows_build": False,
        "allows_asset_mutation": False,
        "requires_sample_live_proof": True,
    }


def _controlrig_name(value: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9_]+", "_", str(value or "").strip()).strip("_")
    if not sanitized:
        return ""
    if sanitized[0].isdigit():
        sanitized = "_" + sanitized
    return sanitized


def _bone_records_by_name(profile: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    records: Dict[str, Dict[str, Any]] = {}
    for item in profile.get("bones", []):
        if not isinstance(item, dict) or not item.get("name"):
            continue
        name = str(item.get("name"))
        records[name] = item
        records[name.lower()] = item
    return records


def _bone_location(record: Dict[str, Any], fallback_z: float = 0.0) -> Dict[str, float]:
    transform = record.get("local_transform") if isinstance(record.get("local_transform"), dict) else {}
    location = transform.get("location") if isinstance(transform.get("location"), dict) else {}
    return {
        "x": float(location.get("x", 0.0)),
        "y": float(location.get("y", 0.0)),
        "z": float(location.get("z", fallback_z)),
    }


def _offset_location(base: Dict[str, float], *, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> Dict[str, float]:
    return {
        "x": round(float(base.get("x", 0.0)) + x, 4),
        "y": round(float(base.get("y", 0.0)) + y, 4),
        "z": round(float(base.get("z", 0.0)) + z, 4),
    }


def _alias_status(evidence: Dict[str, Any], alias: str) -> str:
    aliases = evidence.get("humanoid_aliases") if isinstance(evidence.get("humanoid_aliases"), dict) else {}
    value = aliases.get(alias)
    return str(value.get("status") or "") if isinstance(value, dict) else "missing"


def _alias_selected_names(evidence: Dict[str, Any], alias: str) -> List[str]:
    aliases = evidence.get("humanoid_aliases") if isinstance(evidence.get("humanoid_aliases"), dict) else {}
    value = aliases.get(alias)
    if not isinstance(value, dict):
        return []
    if isinstance(value.get("chain"), list):
        return [str(item.get("name")) for item in value["chain"] if isinstance(item, dict) and item.get("name")]
    selected = str(value.get("selected") or "")
    return [selected] if selected else []


def _override_names(overrides: Dict[str, Any], alias: str) -> List[str]:
    raw = overrides.get(alias)
    if isinstance(raw, str) and raw.strip():
        return [raw.strip()]
    if isinstance(raw, list):
        return [str(item).strip() for item in raw if str(item).strip()]
    return []


def _resolve_alias_names(
    evidence: Dict[str, Any],
    overrides: Dict[str, Any],
    alias: str,
    issues: List[Dict[str, str]],
    *,
    allow_ambiguous_aliases: bool = False,
    required: bool = True,
) -> List[str]:
    override = _override_names(overrides, alias)
    if override:
        return override

    status = _alias_status(evidence, alias)
    names = _alias_selected_names(evidence, alias)
    if status == "ambiguous" and not allow_ambiguous_aliases:
        issues.append(
            _issue(
                "ambiguous_alias_requires_override",
                f"{alias} is ambiguous; provide bone_overrides.{alias}.",
                f"bone_overrides.{alias}",
            )
        )
        return []
    if required and (status == "missing" or not names):
        issues.append(
            _issue(
                "missing_alias_requires_override",
                f"{alias} is missing; provide bone_overrides.{alias}.",
                f"bone_overrides.{alias}",
            )
        )
        return []
    return names


def _compile_fk_request(
    evidence: Dict[str, Any],
    profile: Dict[str, Any],
    request: Dict[str, Any],
    issues: List[Dict[str, str]],
) -> Dict[str, Any]:
    overrides = request.get("bone_overrides") if isinstance(request.get("bone_overrides"), dict) else {}
    allow_ambiguous = bool(request.get("allow_ambiguous_aliases", False))
    bone_records = _bone_records_by_name(profile)
    names = _resolve_alias_names(evidence, overrides, "spine", issues, allow_ambiguous_aliases=allow_ambiguous, required=False)
    if not names:
        names = _resolve_alias_names(evidence, overrides, "head", issues, allow_ambiguous_aliases=allow_ambiguous, required=True)

    chain = []
    for name in names:
        record = bone_records.get(name) or bone_records.get(name.lower(), {})
        chain.append(
            {
                "name": name,
                "location": _bone_location(record, fallback_z=float(len(chain)) * 25.0),
            }
        )
    return {"chain": chain}


def _compile_spine_request(
    evidence: Dict[str, Any],
    profile: Dict[str, Any],
    request: Dict[str, Any],
    issues: List[Dict[str, str]],
) -> Dict[str, Any]:
    overrides = request.get("bone_overrides") if isinstance(request.get("bone_overrides"), dict) else {}
    allow_ambiguous = bool(request.get("allow_ambiguous_aliases", False))
    names = _resolve_alias_names(evidence, overrides, "spine", issues, allow_ambiguous_aliases=allow_ambiguous)
    bone_records = _bone_records_by_name(profile)
    points = []
    for index, name in enumerate(names):
        record = bone_records.get(name) or bone_records.get(name.lower(), {})
        location = _bone_location(record, fallback_z=90.0 + (index * 25.0))
        points.append(
            {
                "name": name,
                "location": location,
                "tangent_location": _offset_location(location, x=30.0),
            }
        )
    return {
        "points": points,
        "include_spaces": True,
        "include_tangent_controls": True,
        "include_weight_curves": True,
        "include_stretch_curve": True,
        "chain_parent_controls": False,
    }


def _compile_target_request(
    evidence: Dict[str, Any],
    profile: Dict[str, Any],
    request: Dict[str, Any],
    issues: List[Dict[str, str]],
    aliases: Sequence[str],
) -> List[Dict[str, Any]]:
    overrides = request.get("bone_overrides") if isinstance(request.get("bone_overrides"), dict) else {}
    allow_ambiguous = bool(request.get("allow_ambiguous_aliases", False))
    bone_records = _bone_records_by_name(profile)
    targets: List[Dict[str, Any]] = []
    for alias in aliases:
        names = _resolve_alias_names(
            evidence,
            overrides,
            alias,
            issues,
            allow_ambiguous_aliases=allow_ambiguous,
            required=False,
        )
        if not names:
            continue
        name = names[-1]
        record = bone_records.get(name) or bone_records.get(name.lower(), {})
        side = "Left" if alias.startswith("left_") else "Right" if alias.startswith("right_") else ""
        clean_name = "".join(part.capitalize() for part in alias.split("_") if part and part not in {"left", "right"})
        target_name = f"{side}{clean_name or name}"
        location = _bone_location(record, fallback_z=100.0)
        lateral = -35.0 if alias.startswith("left_") else 35.0 if alias.startswith("right_") else 0.0
        targets.append(
            {
                "name": target_name,
                "target_location": _offset_location(location, y=lateral, z=10.0),
                "pole_location": _offset_location(location, y=lateral * 2.0, z=-15.0),
            }
        )
    if not targets:
        issues.append(
            _issue(
                "no_target_aliases_resolved",
                "No requested hand/foot aliases resolved; provide bone_overrides or choose another preset.",
                "bone_overrides",
            )
        )
    return targets


def _compile_two_bone_request(
    evidence: Dict[str, Any],
    profile: Dict[str, Any],
    request: Dict[str, Any],
    issues: List[Dict[str, str]],
) -> Dict[str, Any]:
    overrides = request.get("bone_overrides") if isinstance(request.get("bone_overrides"), dict) else {}
    allow_ambiguous = bool(request.get("allow_ambiguous_aliases", False))
    bone_records = _bone_records_by_name(profile)
    name_prefix = _controlrig_name(str(request.get("name_prefix") or "Stable")) or "Stable"
    include_solver_intents = bool(request.get("include_solver_intents", False))
    limbs = []
    solver_intents = []
    for alias in ("left_arm", "right_arm", "left_leg", "right_leg"):
        names = _resolve_alias_names(
            evidence,
            overrides,
            alias,
            issues,
            allow_ambiguous_aliases=allow_ambiguous,
            required=False,
        )
        if len(names) < 2:
            continue
        chain_records = [bone_records.get(name) or bone_records.get(name.lower(), {}) for name in names]
        side = "Left" if alias.startswith("left_") else "Right"
        limb_type = "Arm" if alias.endswith("_arm") else "Leg"
        limb_name = f"{side}{limb_type}"
        base_name = f"{name_prefix}_{limb_name}"
        root_location = _bone_location(chain_records[0], fallback_z=120.0)
        mid_location = _bone_location(chain_records[min(1, len(chain_records) - 1)], fallback_z=105.0)
        end_location = _bone_location(chain_records[-1], fallback_z=95.0)
        lateral = -45.0 if alias.startswith("left_") else 45.0
        limbs.append(
            {
                "name": limb_name,
                "root_location": root_location,
                "mid_location": mid_location,
                "target_location": _offset_location(end_location, y=lateral, z=10.0),
                "pole_location": _offset_location(mid_location, y=lateral * 1.6, z=10.0),
                "default_weight": 1.0,
            }
        )
        if include_solver_intents:
            solver_intents.append(
                {
                    "kind": "two_bone_ik_limb",
                    "name": limb_name,
                    "side": side.lower(),
                    "candidate_unit_struct": "FRigUnit_TwoBoneIKSimplePerItem",
                    "root_bone": names[0],
                    "mid_bone": names[min(1, len(names) - 1)],
                    "end_bone": names[-1],
                    "target_control": f"{base_name}_Target",
                    "pole_control": f"{base_name}_Pole",
                    "weight_curve": f"{base_name}_Weight",
                    "validation_only": True,
                    "adds_solver_units": False,
                }
            )
    if not limbs:
        issues.append(
            _issue(
                "no_limb_aliases_resolved",
                "No arm or leg chain aliases resolved; provide bone_overrides or choose another preset.",
                "bone_overrides",
            )
        )
    result: Dict[str, Any] = {
        "limbs": limbs,
        "include_spaces": True,
        "include_poles": True,
        "include_joint_controls": True,
        "include_weight_curves": True,
    }
    if include_solver_intents:
        result["solver_gate"] = (
            deepcopy(request["solver_gate"])
            if isinstance(request.get("solver_gate"), dict)
            else _default_solver_gate()
        )
        result["solver_intents"] = solver_intents
        result["solver_intents_validation_only"] = True
        result["adds_solver_units"] = False
    return result


def _compile_profile_preset_request(profile: Dict[str, Any], request: Dict[str, Any]) -> Dict[str, Any]:
    evidence = profile.get("planning_evidence") if isinstance(profile.get("planning_evidence"), dict) else None
    if evidence is None:
        evidence = build_skeleton_profile_planning_evidence(profile)

    preset = str(request.get("preset") or "").strip()
    target = str(request.get("target_control_rig_path") or "").strip()
    issues: List[Dict[str, str]] = []

    if not preset:
        issues.append(_issue("missing_preset", "preset is required.", "preset"))
    if not target:
        issues.append(_issue("missing_target_control_rig_path", "target_control_rig_path is required.", "target_control_rig_path"))
    elif not _is_sample_or_temp_path(target):
        issues.append(
            _issue(
                "target_outside_sample_temp_scope",
                "target_control_rig_path must be under /Game/_MCP_Temp or /Game/_MCP_Sample.",
                "target_control_rig_path",
            )
        )
    if bool(request.get("save", False)):
        issues.append(_issue("save_not_allowed_in_phase2_compiler", "Phase 2 compiler emits save=false recipes only.", "save"))

    supported = {
        "simple_fk_control_chain",
        "spine_spline_control_chain",
        "hand_foot_target_controls",
        "look_at_aim_target_controls",
        "two_bone_ik_limb_controls",
        "interaction_target_controls",
        "curve_gate_controls",
    }
    if preset and preset not in supported:
        issues.append(_issue("unsupported_preset", f"Unsupported Phase 2 preset compiler target: {preset}.", "preset"))

    compiled: Dict[str, Any] = {}
    if not issues:
        if preset == "simple_fk_control_chain":
            compiled = _compile_fk_request(evidence, profile, request, issues)
        elif preset == "spine_spline_control_chain":
            compiled = _compile_spine_request(evidence, profile, request, issues)
        elif preset == "hand_foot_target_controls":
            compiled = {
                "targets": _compile_target_request(
                    evidence,
                    profile,
                    request,
                    issues,
                    ("left_hand", "right_hand", "left_foot", "right_foot"),
                ),
                "include_spaces": True,
                "include_poles": True,
            }
        elif preset == "look_at_aim_target_controls":
            targets = _compile_target_request(evidence, profile, request, issues, ("head",))
            for item in targets:
                item["up_vector_location"] = _offset_location(item["target_location"], z=55.0)
                item["default_weight"] = 1.0
                item.pop("pole_location", None)
            compiled = {
                "targets": targets,
                "include_spaces": True,
                "include_up_vectors": True,
                "include_weight_curves": True,
            }
        elif preset == "two_bone_ik_limb_controls":
            compiled = _compile_two_bone_request(evidence, profile, request, issues)
        elif preset == "interaction_target_controls":
            targets = _compile_target_request(evidence, profile, request, issues, ("left_hand", "right_hand"))
            for item in targets:
                item["offset_location"] = {"x": 10.0, "y": -8.0 if "Left" in item["name"] else 8.0, "z": 0.0}
                item["default_reach"] = 1.0
                item["default_weight"] = 1.0
                item["default_contact"] = 0.0
                item.pop("pole_location", None)
            compiled = {
                "targets": targets,
                "include_spaces": True,
                "include_offsets": True,
                "include_reach_controls": True,
                "include_weight_curves": True,
                "include_contact_curves": True,
            }
        elif preset == "curve_gate_controls":
            compiled = {
                "gates": request.get("gates") if isinstance(request.get("gates"), list) else [
                    {"name": "ProfileReady", "default_value": 1.0}
                ],
            }

    preset_request: Dict[str, Any] = {
        "schema_version": 1,
        "preset": preset,
        "target_control_rig_path": target,
        "source_skeletal_mesh": str(request.get("source_skeletal_mesh") or profile.get("skeletal_mesh_path") or profile.get("source_asset_path") or ""),
        "preview_skeletal_mesh": str(request.get("preview_skeletal_mesh") or profile.get("preview_mesh_path") or profile.get("skeletal_mesh_path") or ""),
        "import_preview_bones": bool(request.get("import_preview_bones", False)),
        "max_import_bones": int(request.get("max_import_bones", 128)),
        "name_prefix": str(request.get("name_prefix") or "Stable"),
        "compile": bool(request.get("compile", True)),
        "save": False,
        "inspect_topology": bool(request.get("inspect_topology", True)),
        "direct_solve": bool(request.get("direct_solve", True)),
        "dependency_audit": bool(request.get("dependency_audit", True)),
    }
    preset_request.update(compiled)

    return {
        "success": not any(issue["severity"] == "error" for issue in issues),
        "dry_run": True,
        "read_only": True,
        "sample_only": True,
        "target_modified": False,
        "partial_target_modified": False,
        "evidence_step": "deterministic_profile_recipe_compiler",
        "preset": preset,
        "target_control_rig_path": target,
        "preset_request": preset_request,
        "preset_request_json": _stable_json(preset_request),
        "issues": issues,
        "warnings": [],
        "source_profile_summary": {
            "source_asset_path": profile.get("source_asset_path", ""),
            "skeleton_path": profile.get("skeleton_path", ""),
            "preview_mesh_path": profile.get("preview_mesh_path", ""),
            "bone_count": profile.get("bone_count", 0),
            "returned_bone_count": profile.get("returned_bone_count", 0),
            "truncated": bool(profile.get("truncated", False)),
        },
        "readback_summary": (
            "Deterministic preset request compiled. No assets were created, modified, compiled, or saved."
            if not issues
            else "Deterministic preset request compile failed. No assets were created, modified, compiled, or saved."
        ),
    }


def build_skeleton_profile_planning_evidence(profile: Dict[str, Any]) -> Dict[str, Any]:
    """Build deterministic read-only ControlRig recipe planning evidence from a skeleton profile."""
    raw_bones = profile.get("bones") if isinstance(profile.get("bones"), list) else []
    records = [_record_for_bone(item) for item in raw_bones if isinstance(item, dict) and item.get("name")]
    records.sort(key=lambda item: (item["depth"], item["index"], item["name"]))

    aliases: Dict[str, Any] = {
        "spine": _chain_alias(
            records,
            "spine",
            {
                "pelvis_or_hips": ("pelvis", "hips", "hip"),
                "spine": ("spine", "torso", "chest"),
            },
            multi_match_segments=("spine",),
        ),
        "neck": _single_alias(records, "neck", ("neck",)),
        "head": _single_alias(records, "head", ("head",)),
    }

    for side in ("left", "right"):
        prefix = f"{side}_"
        aliases[prefix + "clavicle"] = _single_alias(records, prefix + "clavicle", ("clavicle", "collar", "shoulder"), side=side)
        aliases[prefix + "arm"] = _chain_alias(
            records,
            prefix + "arm",
            {
                "upper": ("upperarm", "uparm"),
                "lower": ("lowerarm", "forearm", "loarm"),
                "hand": ("hand", "wrist"),
            },
            side=side,
        )
        aliases[prefix + "hand"] = _single_alias(records, prefix + "hand", ("hand", "wrist"), side=side)
        aliases[prefix + "leg"] = _chain_alias(
            records,
            prefix + "leg",
            {
                "upper": ("thigh", "upleg", "upperleg"),
                "lower": ("calf", "shin", "lowerleg", "loleg"),
                "foot": ("foot", "ankle"),
            },
            side=side,
        )
        aliases[prefix + "foot"] = _single_alias(records, prefix + "foot", ("foot", "ankle"), side=side)
        aliases[prefix + "toe"] = _single_alias(records, prefix + "toe", ("toe", "ball"), side=side)

    missing_aliases = sorted(
        alias for alias, value in aliases.items()
        if isinstance(value, dict) and value.get("status") == "missing"
    )
    ambiguous_aliases = sorted(
        alias for alias, value in aliases.items()
        if isinstance(value, dict) and value.get("status") == "ambiguous"
    )

    spine_chain = aliases["spine"].get("chain", [])
    has_head = aliases["head"].get("status") in {"resolved", "ambiguous"}
    has_hand = any(aliases[f"{side}_hand"].get("status") in {"resolved", "ambiguous"} for side in ("left", "right"))
    has_foot = any(aliases[f"{side}_foot"].get("status") in {"resolved", "ambiguous"} for side in ("left", "right"))
    has_arm_chain = any(aliases[f"{side}_arm"].get("status") in {"resolved", "partial", "ambiguous"} for side in ("left", "right"))
    has_leg_chain = any(aliases[f"{side}_leg"].get("status") in {"resolved", "partial", "ambiguous"} for side in ("left", "right"))

    recommended_presets = [
        _recommend_preset(
            "simple_fk_control_chain",
            bool(spine_chain) or has_head,
            "Use detected spine/head bones as a first controls-only FK chain.",
            ("spine", "head"),
            confidence="high" if len(spine_chain) >= 2 else "medium",
        ),
        _recommend_preset(
            "spine_spline_control_chain",
            len(spine_chain) >= 2,
            "Use two or more detected spine candidates as spline-control point evidence.",
            ("spine",),
            confidence="high" if len(spine_chain) >= 3 else "medium",
        ),
        _recommend_preset(
            "hand_foot_target_controls",
            has_hand or has_foot,
            "Use detected hand or foot aliases to place controls-only target/pole helpers.",
            ("left_hand", "right_hand", "left_foot", "right_foot"),
        ),
        _recommend_preset(
            "two_bone_ik_limb_controls",
            has_arm_chain or has_leg_chain,
            "Use detected limb chain candidates for controls-only IK prep; Phase 1 does not add solver units.",
            ("left_arm", "right_arm", "left_leg", "right_leg"),
        ),
        _recommend_preset(
            "look_at_aim_target_controls",
            has_head,
            "Use the detected head alias for controls-only look/aim target planning.",
            ("head",),
        ),
        _recommend_preset(
            "interaction_target_controls",
            has_hand,
            "Use detected hand aliases for interaction target planning without solver units.",
            ("left_hand", "right_hand"),
        ),
        _recommend_preset(
            "curve_gate_controls",
            True,
            "Curve gates do not depend on humanoid bone aliases and remain available as a generic controls-only preset.",
            (),
            confidence="low",
        ),
    ]

    return {
        "success": True,
        "read_only": True,
        "evidence_step": "skeleton_profile_recipe_planning",
        "source_asset_path": profile.get("source_asset_path", ""),
        "skeleton_path": profile.get("skeleton_path", ""),
        "preview_mesh_path": profile.get("preview_mesh_path", ""),
        "bone_count": profile.get("bone_count", len(records)),
        "returned_bone_count": profile.get("returned_bone_count", len(records)),
        "truncated": bool(profile.get("truncated", False)),
        "humanoid_aliases": aliases,
        "missing_aliases": missing_aliases,
        "ambiguous_aliases": ambiguous_aliases,
        "recommended_presets": recommended_presets,
        "notes": [
            "Phase 1 evidence only: no recipe was built, no asset was modified, and no solver unit was added.",
            "Ambiguous aliases must be resolved by explicit bone overrides before Phase 2 deterministic recipe compilation.",
        ],
    }


AuthoringCaller = Callable[[str, str, str], Dict[str, Any]]
CommandSender = Callable[[str, Dict[str, Any], Optional[int]], Dict[str, Any]]


def _build_controlrig_recipe_with_animbp_binding_impl(
    recipe_obj: Dict[str, Any],
    options_obj: Optional[Dict[str, Any]],
    *,
    authoring_caller: AuthoringCaller = _call_authoring_subsystem,
    command_sender: CommandSender = _send_unreal_command,
) -> Dict[str, Any]:
    binding = recipe_obj.get("animbp_binding")
    verification = recipe_obj.get("verification") if isinstance(recipe_obj.get("verification"), dict) else {}
    options_obj = options_obj or {}

    result: Dict[str, Any] = {
        "success": False,
        "command": "build_controlrig_recipe_with_animbp_binding",
        "adapter": "python_reflection_composite",
        "sample_only": True,
        "stages": [],
        "target_control_rig_path": recipe_obj.get("target_control_rig_path", ""),
    }
    completed_mutation_stages: List[str] = []

    if not isinstance(binding, dict) or not binding.get("enabled"):
        result["message"] = "animbp_binding.enabled=true is required for binding orchestration."
        result["stages"].append(_stage("local_contract", False, message=result["message"]))
        result.update(
            mutation_attempted=False,
            mutation_performed=False,
            changed=False,
        )
        return result

    source_anim_bp_path = str(binding.get("source_anim_bp_path") or "")
    target_anim_bp_path = str(binding.get("target_anim_bp_path") or "")
    target_control_rig_path = str(recipe_obj.get("target_control_rig_path") or "")
    result.update(
        {
            "source_anim_bp_path": source_anim_bp_path,
            "target_anim_bp_path": target_anim_bp_path,
            "target_control_rig_path": target_control_rig_path,
        }
    )

    if not _is_sample_or_temp_path(target_control_rig_path):
        result["message"] = "target_control_rig_path must be under /Game/_MCP_Sample or /Game/_MCP_Temp."
        result["stages"].append(_stage("local_contract", False, message=result["message"]))
        result.update(
            mutation_attempted=False,
            mutation_performed=False,
            changed=False,
        )
        return result
    if not _is_sample_or_temp_path(target_anim_bp_path):
        result["message"] = "animbp_binding.target_anim_bp_path must be under /Game/_MCP_Sample or /Game/_MCP_Temp."
        result["stages"].append(_stage("local_contract", False, message=result["message"]))
        result.update(
            mutation_attempted=False,
            mutation_performed=False,
            changed=False,
        )
        return result

    recipe_json = json.dumps(recipe_obj, ensure_ascii=False)
    options_json = json.dumps(options_obj, ensure_ascii=False) if options_obj else ""

    validation_report = authoring_caller("validate", recipe_json, "")
    validation_success = bool(validation_report.get("success"))
    result["validation_report"] = validation_report
    result["stages"].append(_stage("validate_recipe", validation_success))
    if not validation_success:
        result["message"] = "Recipe validation failed before orchestration."
        result.update(
            composite_failure_safety_fields(
                validation_report,
                failed_stage="validate_recipe",
                mutation_relevant=False,
            )
        )
        return result

    controlrig_recipe = deepcopy(recipe_obj)
    controlrig_recipe.setdefault("animbp_binding", {})
    controlrig_recipe["animbp_binding"]["enabled"] = False
    controlrig_recipe_json = json.dumps(controlrig_recipe, ensure_ascii=False)

    controlrig_build_report = authoring_caller("build", controlrig_recipe_json, options_json)
    controlrig_success = bool(controlrig_build_report.get("success"))
    result["controlrig_build_report"] = controlrig_build_report
    result["stages"].append(_stage("build_controlrig_without_binding", controlrig_success))
    if not controlrig_success:
        result["message"] = "ControlRig build failed; AnimBP binding was not attempted."
        result.update(
            composite_failure_safety_fields(
                controlrig_build_report,
                failed_stage="build_controlrig_without_binding",
                mutation_relevant=True,
                failed_stage_mutation_performed=bool(
                    controlrig_build_report.get("target_modified")
                    or controlrig_build_report.get("partial_target_modified")
                ),
            )
        )
        return result
    if _bridge_response_mutation_performed(controlrig_build_report):
        completed_mutation_stages.append("build_controlrig_without_binding")

    save_requested = bool(verification.get("save")) and bool(options_obj.get("allow_save"))
    node_params: Dict[str, Any] = {
        "source_anim_bp_path": source_anim_bp_path,
        "target_anim_bp_path": target_anim_bp_path,
        "control_rig_asset_path": controlrig_build_report.get("target_control_rig_path") or target_control_rig_path,
        "allow_existing_target_reuse": bool(binding.get("allow_existing_target_reuse", True)),
        "overwrite_existing": bool(binding.get("overwrite_existing", False)),
        "replace_existing_links": bool(binding.get("replace_existing_links", True)),
        "compile": bool(verification.get("compile", True)),
        "save_asset": save_requested,
    }

    node_response = command_sender("ensure_anim_graph_controlrig_node_sample", node_params, None)
    node_success = _bridge_response_success(node_response)
    result["animbp_node_response"] = node_response
    result["stages"].append(_stage("ensure_anim_graph_controlrig_node_sample", node_success))
    if not node_success:
        result["message"] = "ControlRig node sample creation failed; input defaults were not attempted."
        result.update(
            composite_failure_safety_fields(
                node_response,
                failed_stage="ensure_anim_graph_controlrig_node_sample",
                mutation_relevant=True,
                completed_mutation_stages=completed_mutation_stages,
                failed_stage_mutation_performed=_bridge_response_mutation_performed(
                    node_response
                ),
            )
        )
        return result
    if _bridge_response_mutation_performed(node_response):
        completed_mutation_stages.append("ensure_anim_graph_controlrig_node_sample")

    input_defaults = binding.get("input_defaults") if isinstance(binding.get("input_defaults"), dict) else {}
    raw_expose_input_pins = binding.get("expose_input_pins") if isinstance(binding.get("expose_input_pins"), list) else []
    expose_input_pins = [
        str(pin).strip()
        for pin in raw_expose_input_pins
        if isinstance(pin, str) and str(pin).strip()
    ]
    defaults_response: Optional[Dict[str, Any]] = None
    defaults_success = True
    if input_defaults or expose_input_pins:
        node_result = _bridge_response_result(node_response)
        defaults_params: Dict[str, Any] = {
            "blueprint_name": target_anim_bp_path,
            "graph_name": "AnimGraph",
            "graph_type": "function",
            "disconnect_existing_links": bool(binding.get("disconnect_existing_input_links", False)),
            "allow_non_sample": target_anim_bp_path.startswith("/Game/_MCP_Temp/"),
        }
        if input_defaults:
            defaults_params["input_defaults"] = input_defaults
        if expose_input_pins:
            defaults_params["expose_input_pins"] = expose_input_pins
        created_node_id = node_result.get("created_node_id")
        if created_node_id:
            defaults_params["node_id"] = created_node_id
        control_rig_class_path = node_result.get("control_rig_class_path")
        if control_rig_class_path:
            defaults_params["control_rig_class"] = control_rig_class_path

        defaults_response = command_sender("set_anim_graph_controlrig_input_defaults", defaults_params, None)
        defaults_success = _bridge_response_success(defaults_response)
        result["input_defaults_response"] = defaults_response
        result["stages"].append(_stage("set_anim_graph_controlrig_input_defaults", defaults_success))
        if not defaults_success:
            result["message"] = "AnimBP ControlRig node was created, but input pin exposure/default application failed."
            result.update(
                composite_failure_safety_fields(
                    defaults_response,
                    failed_stage="set_anim_graph_controlrig_input_defaults",
                    mutation_relevant=True,
                    completed_mutation_stages=completed_mutation_stages,
                    failed_stage_mutation_performed=_bridge_response_mutation_performed(
                        defaults_response
                    ),
                )
            )
            return result
        if _bridge_response_mutation_performed(defaults_response):
            completed_mutation_stages.append("set_anim_graph_controlrig_input_defaults")

        if verification.get("compile", True):
            compile_response = command_sender("compile_blueprint", {"blueprint_name": target_anim_bp_path}, None)
            compile_success = _bridge_response_success(compile_response)
            result["animbp_compile_response"] = compile_response
            result["stages"].append(_stage("compile_bound_anim_bp", compile_success))
            if not compile_success:
                result["message"] = "AnimBP input pins/defaults were applied, but post-change compile failed."
                result.update(
                    composite_failure_safety_fields(
                        compile_response,
                        failed_stage="compile_bound_anim_bp",
                        mutation_relevant=True,
                        completed_mutation_stages=completed_mutation_stages,
                    )
                )
                return result
    else:
        skipped_message = "No animbp_binding.input_defaults or expose_input_pins were provided; input command was skipped."
        result["stages"].append(
            _stage(
                "set_anim_graph_controlrig_input_defaults",
                True,
                skipped=True,
                message=skipped_message,
            )
        )

    result.update(
        {
            "success": defaults_success,
            "message": "ControlRig recipe and sample AnimBP binding orchestration completed.",
            "save_requested": save_requested,
            "input_defaults_applied": bool(input_defaults and defaults_response),
            "input_pins_exposed": bool((input_defaults or expose_input_pins) and defaults_response),
        }
    )
    return result


def register_controlrig_authoring_tools(mcp: FastMCP):
    """Register recipe-driven ControlRig authoring adapter tools."""

    @mcp.tool()
    def profile_controlrig_skeleton(ctx: Context, request: Dict[str, Any] | str) -> Dict[str, Any]:
        """
        Read a skeletal mesh or skeleton profile and attach recipe-planning evidence.

        This is a read-only Phase 1 helper. It calls UnrealMCPControlRigToolsEditor
        through Python/reflection, returns bounded bones and parent chains, then
        derives humanoid alias and preset-candidate evidence without mutating
        assets or adding solver units.

        Args:
            request: JSON object or string with source_asset_path,
                skeletal_mesh_path, or skeleton_path, plus optional max_bones.
        """
        try:
            profile = _call_authoring_subsystem("profile_skeleton", _json_payload(request, "request"))
            profile["command"] = "profile_controlrig_skeleton"
            profile["adapter"] = "python_reflection"
            profile["read_only"] = True
            if profile.get("success"):
                profile["planning_evidence"] = build_skeleton_profile_planning_evidence(profile)
            return profile
        except Exception as exc:
            logger.exception("Failed to profile ControlRig skeleton")
            return _controlrig_exception_error(str(exc), exc, read_only=True)

    @mcp.tool()
    def compile_controlrig_profile_recipe(
        ctx: Context,
        profile: Dict[str, Any] | str,
        request: Dict[str, Any] | str,
    ) -> Dict[str, Any]:
        """
        Compile a skeleton profile plus preset request into deterministic recipe evidence.

        This Phase 2 helper is read-only. It derives a stable preset request
        from Phase 1 profile evidence, rejects ambiguous required aliases unless
        bone_overrides are supplied, and optionally generates a recipe through
        the existing UnrealMCPControlRigToolsEditor preset route. It does not build,
        compile, save, or mutate assets.

        Args:
            profile: Result from profile_controlrig_skeleton or a compatible
                skeleton profile object.
            request: Compiler request with preset, target_control_rig_path,
                optional name_prefix, bone_overrides, and generate_recipe.
        """
        try:
            profile_obj = _json_object_payload(profile, "profile")
            request_obj = _json_object_payload(request, "request")
            result = _compile_profile_preset_request(profile_obj, request_obj)
            result["command"] = "compile_controlrig_profile_recipe"
            result["adapter"] = "python_reflection"

            generate_recipe = bool(request_obj.get("generate_recipe", True))
            result["recipe_generation_requested"] = generate_recipe
            if result.get("success") and generate_recipe:
                recipe_report = _call_authoring_subsystem("make_preset", result["preset_request_json"])
                result["recipe_generation_report"] = recipe_report
                result["recipe_generated"] = bool(recipe_report.get("success") and recipe_report.get("recipe_json"))
                if not result["recipe_generated"]:
                    result["success"] = False
                    result.update(
                        composite_failure_safety_fields(
                            recipe_report,
                            failed_stage="make_preset",
                            mutation_relevant=False,
                        )
                    )
                    result["issues"].append(
                        _issue(
                            "recipe_generation_failed",
                            "Preset request compiled, but make_preset recipe generation failed.",
                            "recipe_generation_report",
                        )
                    )
                else:
                    result["recipe"] = recipe_report.get("recipe")
                    result["recipe_json"] = recipe_report.get("recipe_json", "")
            else:
                result["recipe_generated"] = False
            return result
        except Exception as exc:
            logger.exception("Failed to compile ControlRig profile recipe")
            return _controlrig_exception_error(str(exc), exc, read_only=True)

    @mcp.tool()
    def validate_controlrig_solver_intents(ctx: Context, request: Dict[str, Any] | str) -> Dict[str, Any]:
        """
        Validate future ControlRig solver intents through the UnrealMCP native bridge.

        This is a validation-only UnrealMCP C++ route. It accepts a request or
        recipe-like object with solver_gate and solver_intents, verifies that the
        intents are limited to validation-only candidates, rejects materialized
        solver RigVM units, and reports that sample/temp live proof is still
        required before any build path may add solver units.

        Args:
            request: JSON object or string with target_control_rig_path,
                solver_gate, solver_intents, and optional recipe hierarchy.
        """
        try:
            request_obj = _json_object_payload(request, "request")
            response = _send_unreal_command("validate_controlrig_solver_intents", request_obj)
            result = _bridge_response_result(response)
            if result:
                return result
            return response
        except Exception as exc:
            logger.exception("Failed to validate ControlRig solver intents")
            return _controlrig_exception_error(
                str(exc),
                exc,
                error_code="controlrig_solver_intent_validation_failed",
                read_only=True,
                validation_only=True,
                build_allowed=False,
                asset_mutation_allowed=False,
            )

    @mcp.tool()
    def make_controlrig_preset_recipe(ctx: Context, preset: Dict[str, Any] | str) -> Dict[str, Any]:
        """
        Generate a normal ControlRig recipe from an approved preset request.

        This does not mutate assets. The returned recipe can be passed to
        validate_controlrig_recipe, dry_run_controlrig_recipe, or
        build_controlrig_recipe.

        Args:
            preset: Preset request object or JSON string. Supported presets:
                simple_fk_control_chain, curve_gate_controls,
                hand_foot_target_controls, look_at_aim_target_controls,
                two_bone_ik_limb_controls, spine_spline_control_chain, and
                interaction_target_controls. spine_spline_control_chain keeps
                point spaces independent by default; pass
                chain_parent_controls=true when a parented control chain is
                required.
        """
        try:
            return _call_authoring_subsystem("make_preset", _json_payload(preset, "preset"))
        except Exception as exc:
            logger.exception("Failed to generate ControlRig preset recipe")
            return _controlrig_exception_error(str(exc), exc)

    @mcp.tool()
    def validate_controlrig_recipe(ctx: Context, recipe: Dict[str, Any] | str) -> Dict[str, Any]:
        """
        Validate a ControlRig authoring recipe without mutating assets.

        Args:
            recipe: Recipe object or JSON string accepted by UnrealMCPControlRigToolsEditor.
        """
        try:
            return _call_authoring_subsystem("validate", _json_payload(recipe, "recipe"))
        except Exception as exc:
            logger.exception("Failed to validate ControlRig recipe")
            return _controlrig_exception_error(str(exc), exc)

    @mcp.tool()
    def dry_run_controlrig_recipe(ctx: Context, recipe: Dict[str, Any] | str) -> Dict[str, Any]:
        """
        Dry-run a ControlRig authoring recipe without mutating assets.

        Args:
            recipe: Recipe object or JSON string accepted by UnrealMCPControlRigToolsEditor.
        """
        try:
            return _call_authoring_subsystem("dry_run", _json_payload(recipe, "recipe"))
        except Exception as exc:
            logger.exception("Failed to dry-run ControlRig recipe")
            return _controlrig_exception_error(str(exc), exc)

    @mcp.tool()
    def build_controlrig_recipe(
        ctx: Context,
        recipe: Dict[str, Any] | str,
        options: Dict[str, Any] | str | None = None,
    ) -> Dict[str, Any]:
        """
        Build a sample/temp ControlRig from a recipe through the editor-only authoring module.

        Args:
            recipe: Recipe object or JSON string accepted by UnrealMCPControlRigToolsEditor.
            options: Optional build options object or JSON string. Saving requires
                {"allow_save": true} and a recipe with verification.save=true.
        """
        try:
            recipe_json = _json_payload(recipe, "recipe")
            options_json = _json_payload(options, "options") if options is not None else ""
            return _call_authoring_subsystem("build", recipe_json, options_json)
        except Exception as exc:
            logger.exception("Failed to build ControlRig recipe")
            return _controlrig_exception_error(str(exc), exc)

    @mcp.tool()
    @_serialize_controlrig_composite
    def build_controlrig_recipe_with_animbp_binding(
        ctx: Context,
        recipe: Dict[str, Any] | str,
        options: Dict[str, Any] | str | None = None,
    ) -> Dict[str, Any]:
        """
        Build a sample/temp ControlRig recipe and execute its sample AnimBP binding.

        The recipe must contain animbp_binding.enabled=true and sample/temp
        ControlRig and AnimBP targets. The adapter validates the full recipe,
        builds the ControlRig with binding disabled, then calls the existing
        sample-only AnimBP ControlRig node and input-default commands.

        Args:
            recipe: Recipe object or JSON string accepted by UnrealMCPControlRigToolsEditor.
            options: Optional build options object or JSON string. Saving still
                requires {"allow_save": true} and verification.save=true.
        """
        try:
            recipe_obj = _json_object_payload(recipe, "recipe")
            options_obj = _json_object_payload(options, "options") if options is not None else None
            return _build_controlrig_recipe_with_animbp_binding_impl(recipe_obj, options_obj)
        except Exception as exc:
            logger.exception("Failed to build ControlRig recipe with AnimBP binding")
            return _controlrig_exception_error(str(exc), exc)

    logger.info("ControlRig authoring tools registered successfully")
