"""HC original promotion manifest composition. Dedicated GT owns Engine work.
One parent journal, token-first full revalidation and ordered no-replay promotion.
"""
import copy
import hashlib
import json
import logging
import math
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict
import unreal
from . import niagara_promotion as promotion
from . import state
from .state import OperationError

logger=logging.getLogger(__name__)
METHODS=['plan_niagara_promotion_manifest','execute_niagara_promotion_manifest']
PUBLIC_FIELDS={"plan_niagara_promotion_manifest":["source_system_path","source_system_paths","target_root","target_system_paths","asset_name_prefix","asset_name_suffix","forbidden_dependency_prefixes","forbidden_source_roots","allow_non_temp_source","allow_temp_target","allow_temp_dependencies","write_manifest","manifest_name","timeout_seconds"],"execute_niagara_promotion_manifest":["manifest","manifest_path","confirmation_token","require_confirmation","save","request_compile","write_execution_report","execution_report_name","timeout_seconds"]}
DEFAULTS={"plan_niagara_promotion_manifest":{"source_system_path":"","source_system_paths":None,"target_root":"/Game/MCPWeather/Generated","target_system_paths":None,"asset_name_prefix":"","asset_name_suffix":"","forbidden_dependency_prefixes":None,"forbidden_source_roots":None,"allow_non_temp_source":False,"allow_temp_target":False,"allow_temp_dependencies":False,"write_manifest":False,"manifest_name":"","timeout_seconds":120},"execute_niagara_promotion_manifest":{"manifest":None,"manifest_path":"","confirmation_token":"","require_confirmation":True,"save":True,"request_compile":True,"write_execution_report":False,"execution_report_name":"","timeout_seconds":180}}
REQUIRED={method:[] for method in METHODS}
PARAM_FIELDS=PUBLIC_FIELDS
FIELD_TYPES={"plan_niagara_promotion_manifest":{"source_system_path":"string","source_system_paths":"array","target_root":"string","target_system_paths":"array","asset_name_prefix":"string","asset_name_suffix":"string","forbidden_dependency_prefixes":"array","forbidden_source_roots":"array","allow_non_temp_source":"boolean","allow_temp_target":"boolean","allow_temp_dependencies":"boolean","write_manifest":"boolean","manifest_name":"string","timeout_seconds":"number"},"execute_niagara_promotion_manifest":{"manifest":"any","manifest_path":"string","confirmation_token":"string","require_confirmation":"boolean","save":"boolean","request_compile":"boolean","write_execution_report":"boolean","execution_report_name":"string","timeout_seconds":"number"}}
NULLABLE={"plan_niagara_promotion_manifest":["source_system_paths","target_system_paths","forbidden_dependency_prefixes","forbidden_source_roots"],"execute_niagara_promotion_manifest":["manifest"]}
ITEMS={METHODS[0]:{k:'string' for k in ('source_system_paths','target_system_paths','forbidden_dependency_prefixes','forbidden_source_roots')},METHODS[1]:{}}
# Plan can write a Saved manifest; its asset read-only semantics are separate.
OBSERVERS=[]
NO_JOURNAL_METHODS=['plan_niagara_promotion_manifest']
WRITERS=['execute_niagara_promotion_manifest']
_normalize_game_package_path=promotion._normalize_game_package_path
_is_temp_game_path=promotion._is_temp_game_path
_has_forbidden_mcp_reference_marker=promotion._has_forbidden_mcp_reference_marker
_coerce_string_list=promotion._coerce_string_list
_reject_legacy_command_dependency_refs=promotion._reject_legacy_command_dependency_refs
_niagara_error=promotion._niagara_error
_niagara_promotion_state_unknown_error=promotion._niagara_promotion_state_unknown_error


def binding_status():
    ready=promotion.api.binding_status().get('promote_niagara_system_candidate') is True
    return {method:ready for method in METHODS}


def _saved_mcp_root():
    return Path(unreal.Paths.project_saved_dir()).resolve()/'MCP'


def _collect_niagara_promotion_response_paths(response,target):
    # Existing validator reads its semantic leaf from result. This is not raw RPC.
    return promotion._collect_niagara_promotion_response_paths({'result':response},target)


def _niagara_bridge_failure_fields(error):
    # Only genuine returned Native text is labelled raw. No RPC is dispatched.
    return copy.deepcopy(getattr(error,'manifest_failure_fields',{}))



def _public_error(field,kind,message):
    error=OperationError('niagara_promotion_manifest_public_argument_invalid',field+': '+message)
    error.public_validation_errors=[dict(field=[field],type=kind,message=message)];return error

def _public_bool(field,value):
    if type(value) is bool:return value
    if type(value) in (int,float) and value in (0,1):return bool(value)
    if type(value) is str:
        lower=value.lower()
        if lower in ('true','1','t','yes','y','on'):return True
        if lower in ('false','0','f','no','n','off'):return False
        raise _public_error(field,'bool_parsing','Input should be a valid boolean, unable to interpret input')
    if type(value) in (int,float):raise _public_error(field,'bool_parsing','Input should be a valid boolean, unable to interpret input')
    raise _public_error(field,'bool_type','Input should be a valid boolean')

def _public_int(field,value):
    if type(value) in (int,bool):return int(value)
    if type(value) is float:
        if not math.isfinite(value):raise _public_error(field,'finite_number','Input should be a finite number')
        if not value.is_integer():raise _public_error(field,'int_from_float','Input should be a valid integer, got a number with a fractional part')
        return int(value)
    if type(value) is str:
        stripped=value.strip()
        if stripped.isascii() and re.fullmatch(r'[+-]?[0-9](?:_?[0-9])*(?:\.0+)?',stripped):return int(stripped.replace('_','').split('.')[0])
        raise _public_error(field,'int_parsing','Input should be a valid integer, unable to parse string as an integer')
    raise _public_error(field,'int_type','Input should be a valid integer')

def _public_float(field,value):
    if type(value) in (int,float,bool):
        try:return float(value)
        except (ValueError,OverflowError):raise _public_error(field,'float_type','Input should be a valid number')
    if type(value) is str:
        try:
            if not value.isascii():raise ValueError()
            return float(value)
        except (ValueError,OverflowError):raise _public_error(field,'float_parsing','Input should be a valid number, unable to parse string as a number')
    raise _public_error(field,'float_type','Input should be a valid number')

def normalize_public_mcp_arguments(method,arguments):
    """Field-specific original FastMCP preparse/Pydantic profile; no Engine/raw coercion clone."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_promotion_manifest_public_argument_invalid','Known method and public argument object required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:
            if field in REQUIRED[method]:raise _public_error(field,'missing','Field required')
            continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        kind=FIELD_TYPES[method][field]
        if value is None and field in NULLABLE[method]:pass
        elif kind=='array':
            if not isinstance(value,(list,tuple)):raise _public_error(field,'list_type','Input should be a valid list')
            rows=[]
            for index,item in enumerate(value):
                try:
                    if ITEMS[method][field]=='string':
                        if type(item) is not str:raise _public_error(field,'string_type','Input should be a valid string')
                    elif ITEMS[method][field]=='integer':item=_public_int(field,item)
                    elif ITEMS[method][field]=='object':
                        if type(item) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
                    elif ITEMS[method][field]=='number':item=_public_float(field,item)
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_promotion_manifest_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='object':
            if type(value) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
            if any(type(key) is not str for key in value):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
        elif kind=='any':pass
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('niagara_promotion_manifest_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values


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
    _promote=None,
    _before_file=None,
    _write_file=None,
) -> Dict[str, Any]:
    """Plan Niagara promotion candidates by running dry-run promotion preflights."""

    last_promotion_response_candidate_paths: set[str] = set()
    last_promotion_untrusted_response_paths: set[str] = set()
    last_promotion_response_validated = False

    def command(params):
        nonlocal last_promotion_response_candidate_paths
        nonlocal last_promotion_untrusted_response_paths
        nonlocal last_promotion_response_validated
        last_promotion_response_candidate_paths,last_promotion_untrusted_response_paths=(
            _collect_niagara_promotion_response_paths(None,str(params.get('target_system_path') or '')))
        last_promotion_response_validated=False
        try:
            response=_promote(params)
        except Exception as error:
            response=getattr(error,'promotion_response',None)
            candidates,untrusted=_collect_niagara_promotion_response_paths(response,str(params.get('target_system_path') or ''))
            last_promotion_response_candidate_paths.update(candidates)
            last_promotion_untrusted_response_paths.update(untrusted)
            raise
        candidates,untrusted=_collect_niagara_promotion_response_paths(response,str(params.get('target_system_path') or ''))
        last_promotion_response_candidate_paths.update(candidates)
        last_promotion_untrusted_response_paths.update(untrusted)
        try:
            validated_result=promotion._unwrap_niagara_promotion_response(response,
                expected_source_system_path=str(params.get('source_system_path') or ''),
                expected_target_system_path=str(params.get('target_system_path') or ''),
                expected_dry_run=params.get('dry_run') is True,
                expected_save=params.get('save') is True,
                expected_request_compile=params.get('request_compile') is True,
                expected_allow_non_temp_source=params.get('allow_non_temp_source') is True,
                expected_allow_temp_target=params.get('allow_temp_target') is True,
                expected_allow_temp_dependencies=params.get('allow_temp_dependencies') is True,
                expected_overwrite_existing=params.get('overwrite_existing') is True,
                expected_forbidden_dependency_prefixes=_coerce_string_list(params.get('forbidden_dependency_prefixes'),'forbidden_dependency_prefixes'),
                expected_forbidden_source_roots=_coerce_string_list(params.get('forbidden_source_roots'),'forbidden_source_roots'))
        except Exception as error:
            error.manifest_failure_fields={'failed_native_command':'promote_niagara_system_candidate','native_legacy_response':copy.deepcopy(response)}
            raise
        last_promotion_response_validated=True
        return validated_result


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

                dry_run_result = command(params)
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
            _before_file('manifest')
            manifest_dir = _promotion_manifest_dir()
            manifest_dir.mkdir(parents=True, exist_ok=True)
            fallback_name = "niagara_promotion_" + datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
            safe_name = _sanitize_unreal_asset_name(manifest_name, fallback=fallback_name)
            manifest_path = manifest_dir / f"{safe_name}.json"
            manifest["manifest_path"] = str(manifest_path)
            _write_file(manifest_path,json.dumps(manifest, ensure_ascii=False, indent=2, default=str),'manifest')

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
    _promote=None,
    _before_file=None,
    _write_file=None,
) -> Dict[str, Any]:
    """Revalidate and execute a Niagara promotion manifest after explicit confirmation."""

    last_promotion_response_candidate_paths: set[str] = set()
    last_promotion_untrusted_response_paths: set[str] = set()
    last_promotion_response_validated = False

    def command(params):
        nonlocal last_promotion_response_candidate_paths
        nonlocal last_promotion_untrusted_response_paths
        nonlocal last_promotion_response_validated
        last_promotion_response_candidate_paths,last_promotion_untrusted_response_paths=(
            _collect_niagara_promotion_response_paths(None,str(params.get('target_system_path') or '')))
        last_promotion_response_validated=False
        try:
            response=_promote(params)
        except Exception as error:
            response=getattr(error,'promotion_response',None)
            candidates,untrusted=_collect_niagara_promotion_response_paths(response,str(params.get('target_system_path') or ''))
            last_promotion_response_candidate_paths.update(candidates)
            last_promotion_untrusted_response_paths.update(untrusted)
            raise
        candidates,untrusted=_collect_niagara_promotion_response_paths(response,str(params.get('target_system_path') or ''))
        last_promotion_response_candidate_paths.update(candidates)
        last_promotion_untrusted_response_paths.update(untrusted)
        try:
            validated_result=promotion._unwrap_niagara_promotion_response(response,
                expected_source_system_path=str(params.get('source_system_path') or ''),
                expected_target_system_path=str(params.get('target_system_path') or ''),
                expected_dry_run=params.get('dry_run') is True,
                expected_save=params.get('save') is True,
                expected_request_compile=params.get('request_compile') is True,
                expected_allow_non_temp_source=params.get('allow_non_temp_source') is True,
                expected_allow_temp_target=params.get('allow_temp_target') is True,
                expected_allow_temp_dependencies=params.get('allow_temp_dependencies') is True,
                expected_overwrite_existing=params.get('overwrite_existing') is True,
                expected_forbidden_dependency_prefixes=_coerce_string_list(params.get('forbidden_dependency_prefixes'),'forbidden_dependency_prefixes'),
                expected_forbidden_source_roots=_coerce_string_list(params.get('forbidden_source_roots'),'forbidden_source_roots'))
        except Exception as error:
            error.manifest_failure_fields={'failed_native_command':'promote_niagara_system_candidate','native_legacy_response':copy.deepcopy(response)}
            raise
        last_promotion_response_validated=True
        return validated_result


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

                dry_run_result = command(params)
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
                promotion_result = command(params)
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
                _before_file('execution_report')
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
                _write_file(report_path,json.dumps(report, ensure_ascii=False, indent=2, default=str),'execution_report')
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



def _arguments(method,arguments,legacy=None,public_input=False):
    if method not in METHODS or type(arguments) is not dict or type(public_input) is not bool:
        raise OperationError('niagara_promotion_manifest_arguments_invalid','Known method and arguments object required')
    if legacy is not None and (type(legacy) is not dict or public_input):
        raise OperationError('niagara_promotion_manifest_arguments_invalid','Direct Batch and Named model are separate entries')
    original=copy.deepcopy(arguments if legacy is None else legacy)
    if public_input:
        values=normalize_public_mcp_arguments(method,arguments)
    else:
        values=copy.deepcopy(DEFAULTS[method])
        source=arguments if legacy is None else legacy
        values.update({k:copy.deepcopy(source[k]) for k in PUBLIC_FIELDS[method] if k in source})
    # Parameter JSON is not reinterpreted as a new Native command.
    encoded=json.dumps(values,ensure_ascii=False,allow_nan=False)
    encoded.encode('utf-8')
    if '\x00' in encoded:raise OperationError('niagara_promotion_manifest_arguments_invalid','Embedded raw NUL is not admitted')
    return values,original,('named_mcp' if public_input else 'direct_batch' if legacy is not None else 'direct_python')


def _context(result,method,values,original,entry):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),entry_route=entry,
        public_argument_validation_applied=entry=='named_mcp',hidden_fields=0,
        original_python_manifest_composition=True,child_method='promote_niagara_system_candidate',
        child_journals_created=0,child_calls_are_dedicated_compatibility_API=True,
        asset_read_only=method==METHODS[0],optional_Saved_file_effect_separate=True,
        confirmation_first=True,full_revalidation_before_first_promotion=True,
        sequential_execution_is_atomic=False,automatic_action_retry=False,
        public_transport_only_fields=['timeout_seconds'],transport_deadline_emulated=False,
        registry_dependency_scope='GT_direct_on_disk_Package_dependencies_not_transitive_or_full_serialized',
        operation_dependency='existing_GT_EngineAPI_plus_Python_composition',registration_dependency='ToolsetAPI',server_RPC_dependency=False,
        actual_named_wire_verified=False,unique_external_pipeline_complete=False)


def _refresh_changes(result):
    data=result['data'];known=data['change_state_known'] and not data['file_outcome_unknown']
    attempted=data['native_mutation_attempted'] or data['filesystem_mutation_attempted']
    changed=data['observed_asset_changed'] or data['filesystem_changed']
    data.update(change_state_known=known,mutation_state_unknown=not known,mutation_performed=changed if known else None,
        execution_mutation_attempted=attempted)
    result['changes']=[] if known and not attempted and not changed else [dict(kind='niagara_promotion_manifest',attempted=attempted,changed=changed if known else None,change_state_known=known)]
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_child_or_file_outcome_no_replay' if not known else 'inspect_partial_manifest_before_new_call' if attempted else 'none_observed')


def _checkpoint(result,context):
    if context.get('checkpoint_error') is not None:raise context['checkpoint_error']
    if context['callback']:
        try:context['callback'](result)
        except Exception as error:
            context['checkpoint_error']=error
            raise


def _adopt_native(result,native,params):
    data=result['data']
    scalar={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')}
    native_data=native.get('data')
    raw=native.get('_raw_native_json')
    diagnostic={'inlined':False,'reason':'decoded_receipt_data_retained_once_not_reconstructed_raw'}
    if type(raw) is str:
        try:
            encoded=raw.encode('utf-8');diagnostic.update(sha256_utf8=hashlib.sha256(encoded).hexdigest(),utf8_bytes=len(encoded))
        except UnicodeError:diagnostic['unavailable_reason']='original_raw_is_not_valid_Unicode'
    receipt=dict(index=len(data['native_receipts']),params=copy.deepcopy(params),actual=scalar,
        native_data_except_legacy_result={k:copy.deepcopy(v) for k,v in native_data.items() if k!='legacy_result'} if type(native_data) is dict else None,
        original_raw_fingerprint=diagnostic)
    data['native_receipts'].append(receipt);data['native_result_returned']=True
    data['native_mutation_attempted']|=native.get('mutation_attempted') is True
    data['observed_asset_changed']|=native.get('changed') is True
    known=native.get('change_state_known') is True and type(native.get('changed')) is bool
    data['change_state_known']&=known
    data['SDK_global_purity_unverified']|=type(native_data) is dict and native_data.get('SDK_global_purity_unverified') is True
    for prefix,part in (('compile',result['compile']),('save',result['save'])):
        for field in ('requested','attempted'):part[field]|=native.get(prefix+'_'+field) is True
    result['save']['saved']|=native.get('saved') is True
    compiled=native.get('compiled')
    if compiled is None or result['compile']['compiled'] is None:result['compile']['compiled']=None
    elif compiled is True:result['compile']['compiled']=True
    _refresh_changes(result)
    return receipt


def _child(result,context,params):
    if context.get('stop_error') is not None:raise context['stop_error']
    if context.get('checkpoint_error') is not None:raise context['checkpoint_error']
    data=result['data'];prior_known=data['change_state_known']
    index=len(data['native_receipts']);data.update(current_child_index=index,current_child_params=copy.deepcopy(params),native_dispatched=False)
    result['stage']='before_niagara_promotion_manifest_checkpoint'
    _checkpoint(result,context)
    # Conservatively cover an in-flight call in the one parent journal.
    data.update(native_dispatch_pending=True,current_child_outcome_unknown=True,change_state_known=False)
    _refresh_changes(result)
    try:_checkpoint(result,context)
    except Exception:
        data.update(native_dispatch_pending=False,native_dispatched=False,current_child_outcome_unknown=False,change_state_known=prior_known)
        _refresh_changes(result)
        raise
    native=None
    data.update(native_dispatch_pending=False,native_dispatched=True)
    try:
        native=promotion.api.promote_niagara_system_candidate(params)
    except Exception as error:
        if getattr(error,'native_call_dispatched',None) is False:
            data.update(native_dispatched=False,change_state_known=prior_known,current_child_outcome_unknown=False)
            _refresh_changes(result)
        if getattr(error,'native_call_returned',False):
            data['undecoded_native_raw_result']=getattr(error,'native_raw_result',None)
        context['stop_error']=error
        error.manifest_failure_fields=dict(failed_native_command='promote_niagara_system_candidate',native_call_returned=getattr(error,'native_call_returned',False),native_failure_type=type(error).__name__)
        if getattr(error,'native_call_returned',False):error.manifest_failure_fields['undecoded_native_raw_result']=getattr(error,'native_raw_result',None)
        result['stage']='returned_niagara_promotion_manifest_operation'
        _checkpoint(result,context)
        raise
    if type(native) is not dict:
        error=OperationError('niagara_promotion_manifest_child_invalid','Decoded Native object required')
        context['stop_error']=error;raise error
    data['change_state_known']=prior_known
    _adopt_native(result,native,params)
    result['stage']='returned_niagara_promotion_manifest_operation'
    _checkpoint(result,context)  # Actual savedTrue/partial is durable before health/semantic checks.
    flags=('success','mutation_attempted','change_state_known','postcondition_passed','compile_requested','compile_attempted','save_requested','save_attempted','saved')
    valid=(type(native.get('data')) is dict and all(type(native.get(k)) is bool for k in flags)
        and 'compiled' in native and (native['compiled'] is None or type(native['compiled']) is bool)
        and 'changed' in native and (native['changed'] is None or type(native['changed']) is bool))
    leaf=native.get('data',{}).get('legacy_result') if type(native.get('data')) is dict else None
    healthy=(valid and native['success'] and native['postcondition_passed'] and native['change_state_known']
        and native.get('operation_complete') is not False and native['data'].get('operation_complete') is not False)
    if params.get('dry_run') is True and (native.get('mutation_attempted') is not False or native.get('changed') is not False
        or any(native.get(k) is not False for k in ('compile_attempted','save_attempted','saved'))):healthy=False
    data['current_child_outcome_unknown']=not healthy
    if not healthy:
        data['change_state_known']=False;_refresh_changes(result)
        error=OperationError(native.get('error_code') or 'niagara_promotion_manifest_child_unverified',native.get('error') or 'Native child health/postcondition/terminal boundary is unverified; no replay')
        error.promotion_response=copy.deepcopy(leaf);error.manifest_failure_fields={'failed_native_command':'promote_niagara_system_candidate','native_error_code':native.get('error_code','')}
        context['stop_error']=error
        raise error
    return copy.deepcopy(leaf)


def _before_file(result,context,kind):
    if context.get('checkpoint_error') is not None:raise context['checkpoint_error']
    data=result['data'];context['known_before_file']=data['change_state_known']
    data['file_effects'].append(dict(kind=kind,attempted=False,write_returned=False,outcome_unknown=False))
    result['stage']='before_niagara_promotion_manifest_checkpoint'
    _checkpoint(result,context)
    data['filesystem_mutation_attempted']=True;data['file_outcome_unknown']=True
    data['file_effects'][-1].update(attempted=True,outcome_unknown=True)
    _refresh_changes(result)


def _write_file(result,context,path,text,kind):
    if context.get('checkpoint_error') is not None:raise context['checkpoint_error']
    row=result['data']['file_effects'][-1];row['path']=str(path)
    count=path.write_text(text,encoding='utf-8')
    row.update(write_returned=True,text_characters_written=count,outcome_unknown=False)
    result['data']['filesystem_changed']=True;result['data']['file_outcome_unknown']=False
    result['data']['change_state_known']=context['known_before_file']
    # File success does not claim Native asset save or clear Native unknown.
    _refresh_changes(result);result['stage']='returned_niagara_promotion_manifest_operation';_checkpoint(result,context)


def _run(result,method,values,original,entry,checkpoint_callback=None):
    _context(result,method,values,original,entry)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_receipts=[],
        native_mutation_attempted=False,observed_asset_changed=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,
        SDK_global_purity_unverified=False,filesystem_mutation_attempted=False,filesystem_changed=False,file_outcome_unknown=False,file_effects=[],operation_complete=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='actual_GT_child_receipts')
    result['save'].update(requested=False,attempted=False,saved=False,status='actual_GT_asset_save_receipts_file_writes_are_separate')
    context=dict(callback=checkpoint_callback,checkpoint_error=None,stop_error=None)
    result['stage']='before_niagara_promotion_manifest_checkpoint';_checkpoint(result,context)
    implementation=plan_niagara_promotion_manifest_impl if method==METHODS[0] else execute_niagara_promotion_manifest_impl
    leaf=implementation(**values,_promote=lambda params:_child(result,context,params),
        _before_file=lambda kind:_before_file(result,context,kind),
        _write_file=lambda path,text,kind:_write_file(result,context,path,text,kind))
    result['data']['legacy_result']=leaf
    result['data']['legacy_Named_result_reference']='data.legacy_result'
    if leaf.get('mutation_state_unknown') is True or leaf.get('read_only_state_unknown') is True:
        result['data']['change_state_known']=False
    _refresh_changes(result)
    result['stage']='returned_niagara_promotion_manifest_operation'
    result['validation'].update(status='original_manifest_policy_and_GT_child_postconditions',
        original_confirmation_and_full_revalidation_preserved=True,all_child_postconditions=context['stop_error'] is None,
        atomic_transaction=False,disk_reload='not_run',full_original_pipeline='not_run')
    result['data']['operation_complete']=True
    if context['checkpoint_error'] is not None:raise context['checkpoint_error']
    _checkpoint(result,context)
    if leaf.get('success') is not True or context['stop_error'] is not None:
        raise OperationError(leaf.get('error_code') or 'niagara_promotion_manifest_failed',leaf.get('error') or leaf.get('message') or 'Manifest failed or partial; actual prefix retained')
    result.update(success=True,stage='complete')
    return result


def _rejected(method,error):
    result=state.new_result();state.fail(result,error)
    result['stage']='niagara_promotion_manifest_input_validation'
    result['data'].update(public_method=method,native_dispatched=False,native_mutation_attempted=False,change_state_known=True,mutation_performed=False,journal_created=False)
    result['compile'].update(requested=False,attempted=False,compiled=False)
    result['save'].update(requested=False,attempted=False,saved=False)
    if hasattr(error,'public_validation_errors'):result['data']['public_validation_errors']=copy.deepcopy(error.public_validation_errors)
    result['data']['legacy_result']=_niagara_error(str(error),error_code=getattr(error,'code','niagara_tool_failed'),dispatch_attempted=False)
    return result


def _named(method,arguments,_public_input=False):
    try:values,original,entry=_arguments(method,arguments,None,_public_input)
    except Exception as error:return _rejected(method,error)
    # An optional manifest write is an explicit filesystem mutation, even for the planner.
    if method==METHODS[0] and not values['write_manifest']:
        result=state.new_result()
        try:return _run(result,method,values,original,entry)
        except Exception as error:return state.fail(result,error)
    return state.mutate(method,arguments,None,lambda result:_run(result,method,values,original,entry,state.checkpoint))


def _named_mcp(method,arguments):return _named(method,arguments,True)


def _action(result,method,arguments,callback,legacy,public_input=False):
    result['data']['parent_journal_only']=True
    try:values,original,entry=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        rejected=_rejected(method,error)
        result.update(rejected);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,values,original,entry,callback)


def plan_niagara_promotion_manifest(source_system_path="",source_system_paths=None,target_root="/Game/MCPWeather/Generated",target_system_paths=None,asset_name_prefix="",asset_name_suffix="",forbidden_dependency_prefixes=None,forbidden_source_roots=None,allow_non_temp_source=False,allow_temp_target=False,allow_temp_dependencies=False,write_manifest=False,manifest_name="",timeout_seconds=120):
    return _named('plan_niagara_promotion_manifest',dict(source_system_path=source_system_path,source_system_paths=source_system_paths,target_root=target_root,target_system_paths=target_system_paths,asset_name_prefix=asset_name_prefix,asset_name_suffix=asset_name_suffix,forbidden_dependency_prefixes=forbidden_dependency_prefixes,forbidden_source_roots=forbidden_source_roots,allow_non_temp_source=allow_non_temp_source,allow_temp_target=allow_temp_target,allow_temp_dependencies=allow_temp_dependencies,write_manifest=write_manifest,manifest_name=manifest_name,timeout_seconds=timeout_seconds))


def plan_niagara_promotion_manifest_action(result,source_system_path="",source_system_paths=None,target_root="/Game/MCPWeather/Generated",target_system_paths=None,asset_name_prefix="",asset_name_suffix="",forbidden_dependency_prefixes=None,forbidden_source_roots=None,allow_non_temp_source=False,allow_temp_target=False,allow_temp_dependencies=False,write_manifest=False,manifest_name="",timeout_seconds=120,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'plan_niagara_promotion_manifest',dict(source_system_path=source_system_path,source_system_paths=source_system_paths,target_root=target_root,target_system_paths=target_system_paths,asset_name_prefix=asset_name_prefix,asset_name_suffix=asset_name_suffix,forbidden_dependency_prefixes=forbidden_dependency_prefixes,forbidden_source_roots=forbidden_source_roots,allow_non_temp_source=allow_non_temp_source,allow_temp_target=allow_temp_target,allow_temp_dependencies=allow_temp_dependencies,write_manifest=write_manifest,manifest_name=manifest_name,timeout_seconds=timeout_seconds),checkpoint_callback,_legacy_command_params,_public_input)


def execute_niagara_promotion_manifest(manifest=None,manifest_path="",confirmation_token="",require_confirmation=True,save=True,request_compile=True,write_execution_report=False,execution_report_name="",timeout_seconds=180):
    return _named('execute_niagara_promotion_manifest',dict(manifest=manifest,manifest_path=manifest_path,confirmation_token=confirmation_token,require_confirmation=require_confirmation,save=save,request_compile=request_compile,write_execution_report=write_execution_report,execution_report_name=execution_report_name,timeout_seconds=timeout_seconds))


def execute_niagara_promotion_manifest_action(result,manifest=None,manifest_path="",confirmation_token="",require_confirmation=True,save=True,request_compile=True,write_execution_report=False,execution_report_name="",timeout_seconds=180,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'execute_niagara_promotion_manifest',dict(manifest=manifest,manifest_path=manifest_path,confirmation_token=confirmation_token,require_confirmation=require_confirmation,save=save,request_compile=request_compile,write_execution_report=write_execution_report,execution_report_name=execution_report_name,timeout_seconds=timeout_seconds),checkpoint_callback,_legacy_command_params,_public_input)
