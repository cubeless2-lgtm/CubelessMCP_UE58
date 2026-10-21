"""HG public profile/planning composition; original Python planning helpers are preserved.
Profile metadata is an Engine leaf; preset generation is original pure C++ JSON work.
Recipe compile/save settings describe future work and never current Engine effects.
"""
import copy
from copy import deepcopy
import hashlib
import json
import re
from typing import Any, Dict, List, Optional, Sequence
from cubeless_mcp_ue58.compatibility.ue58 import controlrig_recipe_planning as api
from . import state
from .state import OperationError

METHODS=['profile_controlrig_skeleton','compile_controlrig_profile_recipe','make_controlrig_preset_recipe']
PUBLIC_FIELDS={METHODS[0]:['request'],METHODS[1]:['profile','request'],METHODS[2]:['preset']}
PARAM_FIELDS=copy.deepcopy(PUBLIC_FIELDS)
DEFAULTS={name:{} for name in METHODS}
REQUIRED=copy.deepcopy(PUBLIC_FIELDS)
FIELD_TYPES={name:{field:'object_or_string' for field in fields} for name,fields in PUBLIC_FIELDS.items()}
NULLABLE={name:[] for name in METHODS}
ITEMS={name:{} for name in METHODS}
NO_JOURNAL_METHODS=list(METHODS)
OBSERVERS=list(METHODS)
WRITERS=[]
_ACTIVE_REQUEST=False
_ORIGINAL_FORBIDDEN_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')
_BATCH_FORBIDDEN_TOKENS=_ORIGINAL_FORBIDDEN_TOKENS+('cubelessmcp','cubeless_mcp','modelcontextprotocol','toolsetregistry','/engine/pythontypes')


def _reject_references(field_name,value,tokens):
    if isinstance(value,str):
        candidate=value.strip().strip('"\'').replace('\\','/').lower()
        if any(token in candidate for token in tokens):
            error=ValueError("Refusing to persist MCP-only reference in '%s': %s. UnrealMCP is an editor authoring bridge; finished assets must not depend on it."%(field_name,value))
            error.field_name=field_name;error.reason='mcp_only_reference';raise error
    elif isinstance(value,dict):
        for key,item in value.items():_reject_references(field_name+'.'+str(key),item,tokens)
    elif isinstance(value,(list,tuple,set)):
        for index,item in enumerate(value):_reject_references(field_name+'['+str(index)+']',item,tokens)


def reject_mcp_dependency_references(field_name,value):
    _reject_references(field_name,value,_ORIGINAL_FORBIDDEN_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    # Ordinary Batch's current policy is distinct from the original public body guard.
    _reject_references('params',params,_BATCH_FORBIDDEN_TOKENS)


def _public_error(field,kind,message):
    error=OperationError('controlrig_recipe_planning_public_argument_invalid',field+': '+message)
    error.public_validation_errors=[dict(field=[field],type=kind,message=message)];return error


def normalize_public_mcp_arguments(method,arguments):
    if method not in METHODS or type(arguments) is not dict:
        raise OperationError('controlrig_recipe_planning_public_argument_invalid','Known method and public argument object required')
    values={}
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:raise _public_error(field,'missing','Field required')
        value=copy.deepcopy(arguments[field])
        # Original FastMCP preparse precedes the Dict[str,Any]|str model.
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if not isinstance(value,(dict,str)) or isinstance(value,dict) and any(not isinstance(key,str) for key in value):
            raise _public_error(field,'dict_type_or_string_type','Input should be a valid dictionary or string')
        values[field]=value
    return values


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

def _is_sample_or_temp_path(path: str) -> bool:
    return path.startswith("/Game/_MCP_Sample/") or path.startswith("/Game/_MCP_Temp/")

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



def binding_status():
    status=api.binding_status()
    return {METHODS[0]:status.get('profile_controlrig_skeleton_metadata') is True,
            METHODS[1]:callable(_compile_profile_preset_request),
            METHODS[2]:status.get('make_controlrig_preset_recipe_data') is True}


def _arguments(method,arguments,legacy=None,public_input=False):
    if method not in METHODS or type(arguments) is not dict or type(public_input) is not bool:
        raise OperationError('controlrig_recipe_planning_entry_invalid','Known method, argument object and exact model selector required')
    original=copy.deepcopy(arguments)
    if legacy is not None:
        if type(legacy) is not dict or public_input:
            raise OperationError('controlrig_recipe_planning_entry_invalid','Lower-name Batch alias is a Python public composition, not a Native raw contract')
        arguments=legacy
    values=normalize_public_mcp_arguments(method,arguments) if public_input else {key:copy.deepcopy(arguments[key]) for key in PUBLIC_FIELDS[method] if key in arguments}
    for field in REQUIRED[method]:
        if field not in values:raise _public_error(field,'missing','Field required')
    return values,original


def _context(result,method,original,legacy,public_input):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),public_argument_validation_applied=public_input,
        lower_name_Batch_arguments=copy.deepcopy(legacy),original_Native_command=False,hidden_fields=0,
        journal_created=False,automatic_action_retry=False,atomic_rollback=False,operation_complete=False,
        native_dispatched=False,native_result_returned=False,native_call_count=0,native_health_passed=True,
        native_mutation_attempted=False,change_state_known=True,observed_changed=False,
        execution_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False,
        SDK_global_purity_unverified=False,actual_named_wire_verified=False,
        actual_backend_route='Python_original_planning_composition',
        future_recipe_compile_save_are_current_effects=False,server_RPC_dependency=False,
        registration_dependency='ToolsetAPI',operation_dependency='public_Engine_metadata_or_original_pure_JSON_CPP')
    result['compile'].update(requested=False,attempted=False,compiled=False,status='not_requested')
    result['save'].update(requested=False,attempted=False,saved=False,status='not_attempted')
    result['changes']=[]


def _checkpoint(result,callback):
    if callback:callback(result)


def _changes(result):
    data=result['data'];known=data['change_state_known'];attempted=data['native_mutation_attempted'];changed=data['observed_changed']
    data.update(execution_mutation_attempted=attempted,mutation_state_unknown=not known,mutation_performed=changed if known else None)
    result['changes']=[] if known and not attempted and not changed else [dict(kind='controlrig_recipe_planning',attempted=attempted,changed=changed if known else None,change_state_known=known)]
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_native_outcome_no_replay' if not known else 'inspect_recorded_unexpected_changes_before_new_operation' if attempted or changed else 'none_observed')


def _adopt(result,native):
    data=result['data'];raw=native.pop('_raw_native_json',None)
    # Actual scalar outcomes precede semantic/observer health checks and callbacks.
    data.update(native_result_returned=True,native_mutation_attempted=native.get('mutation_attempted') is True,
        change_state_known=native.get('change_state_known') is True and type(native.get('changed')) is bool,
        observed_changed=native.get('changed') is True,
        native_health_passed=native.get('success') is True and native.get('postcondition_passed') is True)
    result['compile'].update(requested=native.get('compile_requested') is True,attempted=native.get('compile_attempted') is True,compiled=native.get('compiled'))
    result['save']['requested']|=native.get('save_requested') is True
    result['save']['attempted']|=native.get('save_attempted') is True
    result['save']['saved']|=native.get('saved') is True
    data['native_scalar_result']={key:copy.deepcopy(value) for key,value in native.items() if key!='data'}
    body=native.get('data',{})
    data['native_operation_data']={key:copy.deepcopy(value) for key,value in body.items() if key!='legacy_result'}
    data['SDK_global_purity_unverified']=body.get('SDK_global_purity_unverified') is True
    if type(body.get('legacy_result')) is dict:data['legacy_result']=copy.deepcopy(body['legacy_result'])
    for key in ('actual_mutation_prefix','actual_compile_prefix','actual_save_prefix','actual_partial_state','actual_observation_prefix','native_call_counts','actual_call_prefix'):
        if key in body:data[key]=copy.deepcopy(body[key])
    if type(raw) is str:
        encoded=raw.encode('utf-8')
        data['native_raw_receipt']=dict(original_raw_preserved=False,omitted=True,reason='decoded_authoritative_receipt_retained_once',
            sha256=hashlib.sha256(encoded).hexdigest(),utf8_bytes=len(encoded),utf16_units=len(raw.encode('utf-16-le'))//2)
    _changes(result)


def _native(result,method,payload,callback):
    data=result['data'];prior=data['change_state_known']
    data.update(native_dispatch_pending=True,change_state_known=False);_changes(result)
    try:_checkpoint(result,callback)
    except Exception:
        data.update(native_dispatch_pending=False,change_state_known=prior);_changes(result);raise
    data.update(native_dispatch_pending=False,native_dispatched=True,native_method=method)
    data['native_call_count']+=1
    try:native=getattr(api,method)(payload)
    except Exception as error:
        if getattr(error,'native_call_dispatched',None) is False:
            data.update(native_dispatched=False,change_state_known=prior)
            data['native_call_count']-=1
        if getattr(error,'native_call_returned',False):
            # Only genuine undecoded original bytes are labelled raw.
            data['undecoded_native_raw_result']=getattr(error,'native_raw_result',None)
        data['native_health_passed']=False;_changes(result)
        result['stage']='returned_controlrig_recipe_planning_operation';_checkpoint(result,callback);raise
    _adopt(result,native)
    result['stage']='returned_controlrig_recipe_planning_operation';_checkpoint(result,callback)
    if any(native.get(key) is not False for key in ('mutation_attempted','compile_requested','compile_attempted','save_requested','save_attempted','saved')) or native.get('compiled') is not False or native.get('changed') is True:
        raise OperationError('controlrig_recipe_planning_unexpected_effect','Planning leaf returned explicit mutation/compile/save effects; actual prefix preserved')
    if native.get('data',{}).get('operation_complete') is False:
        data['change_state_known']=False;_changes(result)
        raise OperationError('controlrig_recipe_planning_native_incomplete','Current leaf receipt is incomplete; no replay')
    leaf=data.get('legacy_result')
    if type(leaf) is not dict or type(leaf.get('success')) is not bool:
        raise OperationError('controlrig_recipe_planning_legacy_invalid','Original semantic legacy object and success bool required')
    # A typed semantic failure remains available to the original wrapper/compiler.
    leaf.update(authoring_plugin_used=not bool(leaf.get('plugin_unavailable')),
        authoring_subsystem=api.LIBRARY,legacy_authoring_subsystem_fallback=False,adapter='python_reflection')
    data['actual_backend_route']='UE58_public_Engine_metadata_CPP' if method=='profile_controlrig_skeleton_metadata' else 'original_pure_JSON_CPP_Operations'
    return leaf


def _legacy_exception(error,read_only=False):
    leaf=dict(success=False,message=str(error),error=str(error),error_code='controlrig_authoring_tool_failed',details=dict(exception_type=type(error).__name__))
    if read_only:leaf['read_only']=True
    if getattr(error,'code','')=='controlrig_recipe_planning_native_api_unavailable':
        leaf.update(plugin_unavailable=True,authoring_plugin_used=False,authoring_subsystem=api.LIBRARY,legacy_authoring_subsystem_fallback=False,adapter='python_reflection')
    return leaf


def _finish(result,leaf,callback):
    if type(leaf) is not dict or type(leaf.get('success')) is not bool:
        raise OperationError('controlrig_recipe_planning_legacy_invalid','Public semantic success bool is required')
    data=result['data'];data.update(legacy_result=leaf,operation_complete=True)
    result['stage']='returned_controlrig_recipe_planning_operation';_checkpoint(result,callback)
    result['validation'].update(status='planning_data_and_current_leaf_receipt',asset_build='not_run',solver_units='not_added',disk_reload='not_run',complete_dependency_scan='not_run',packaged_runtime='not_run')
    if leaf['success'] is not True or not data['native_health_passed']:
        scalar=data.get('native_scalar_result',{})
        error=OperationError(scalar.get('error_code') or leaf.get('error_code') or 'controlrig_recipe_planning_failed',
            scalar.get('error') or leaf.get('error') or leaf.get('message') or leaf.get('readback_summary') or 'ControlRig planning failed; original semantic report retained')
        return state.fail(result,error)
    if not data['change_state_known']:
        return state.fail(result,OperationError('controlrig_recipe_planning_outcome_unknown','Current leaf target outcome remains unknown; no replay'))
    result.update(success=True,stage='complete');return result


def _run(result,method,values,original,legacy,public_input,callback):
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:raise OperationError('controlrig_recipe_planning_reentry','Reentry rejected before any work')
    _ACTIVE_REQUEST=True
    try:
        _context(result,method,original,legacy,public_input)
        result['stage']='before_controlrig_recipe_planning_checkpoint';_checkpoint(result,callback)
        try:
            if method==METHODS[0]:
                leaf=_native(result,'profile_controlrig_skeleton_metadata',_json_payload(values['request'],'request'),callback)
                leaf.update(command=method,adapter='python_reflection',read_only=True)
                if leaf.get('success'):leaf['planning_evidence']=build_skeleton_profile_planning_evidence(leaf)
            elif method==METHODS[1]:
                profile_obj=_json_object_payload(values['profile'],'profile')
                request_obj=_json_object_payload(values['request'],'request')
                leaf=_compile_profile_preset_request(profile_obj,request_obj)
                leaf.update(command=method,adapter='python_reflection')
                generate_recipe=bool(request_obj.get('generate_recipe',True))
                leaf['recipe_generation_requested']=generate_recipe
                if leaf.get('success') and generate_recipe:
                    # Preserve original compiled request if the optional generation dispatch fails.
                    result['data']['compiled_request_before_generation']=copy.deepcopy(leaf)
                    recipe_report=_native(result,'make_controlrig_preset_recipe_data',leaf['preset_request_json'],callback)
                    leaf['recipe_generation_report']=recipe_report
                    leaf['recipe_generated']=bool(recipe_report.get('success') and recipe_report.get('recipe_json'))
                    if not leaf['recipe_generated']:
                        leaf['success']=False
                        leaf.update(failed_stage='make_preset',failed_stage_effect='read',read_unreal_response=recipe_report,
                            mutation_attempted=False,mutation_performed=False,changed=False)
                        leaf['issues'].append(_issue('recipe_generation_failed','Preset request compiled, but make_preset recipe generation failed.','recipe_generation_report'))
                    else:
                        leaf['recipe']=recipe_report.get('recipe');leaf['recipe_json']=recipe_report.get('recipe_json','')
                    result['data'].pop('compiled_request_before_generation',None)
                else:leaf['recipe_generated']=False
            else:
                leaf=_native(result,'make_controlrig_preset_recipe_data',_json_payload(values['preset'],'preset'),callback)
        except Exception as error:
            # Keep a returned typed child separately if a public wrapper fails afterwards.
            if 'legacy_result' in result['data']:
                result['data']['failed_leaf_semantic_report']=result['data'].pop('legacy_result')
            result['data']['legacy_result']=_legacy_exception(error,method!=METHODS[2])
            raise
        return _finish(result,leaf,callback)
    finally:_ACTIVE_REQUEST=False


def _rejected(method,error):
    result=state.new_result();state.fail(result,error)
    result['stage']='controlrig_recipe_planning_input_validation'
    result['compile'].update(requested=False,attempted=False,compiled=False)
    result['save'].update(requested=False,attempted=False,saved=False)
    result['data'].update(public_method=method,native_dispatched=False,native_call_count=0,execution_mutation_attempted=False,
        change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False,
        legacy_result=_legacy_exception(error,method!=METHODS[2]))
    if hasattr(error,'public_validation_errors'):result['data']['public_validation_errors']=copy.deepcopy(error.public_validation_errors)
    return result


def _named(method,arguments,public_input=False):
    try:values,original=_arguments(method,arguments,None,public_input)
    except Exception as error:return _rejected(method,error)
    result=state.new_result();result['data']['journal_created']=False
    try:return _run(result,method,values,original,None,public_input,None)
    except Exception as error:return state.fail(result,error)


def _named_mcp(method,arguments):return _named(method,arguments,True)


def _action(result,method,arguments,callback,legacy,public_input):
    result['data']['parent_journal_only']=True
    try:values,original=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        rejected=_rejected(method,error);rejected['operation_id']=result['operation_id']
        result.update(rejected);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,values,original,legacy,public_input,callback)


def profile_controlrig_skeleton(request):
    return _named("profile_controlrig_skeleton",dict(request=request))


def profile_controlrig_skeleton_action(result,request,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"profile_controlrig_skeleton",dict(request=request),checkpoint_callback,_legacy_command_params,_public_input)


def compile_controlrig_profile_recipe(profile,request):
    return _named("compile_controlrig_profile_recipe",dict(profile=profile,request=request))


def compile_controlrig_profile_recipe_action(result,profile,request,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"compile_controlrig_profile_recipe",dict(profile=profile,request=request),checkpoint_callback,_legacy_command_params,_public_input)


def make_controlrig_preset_recipe(preset):
    return _named("make_controlrig_preset_recipe",dict(preset=preset))


def make_controlrig_preset_recipe_action(result,preset,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"make_controlrig_preset_recipe",dict(preset=preset),checkpoint_callback,_legacy_command_params,_public_input)


