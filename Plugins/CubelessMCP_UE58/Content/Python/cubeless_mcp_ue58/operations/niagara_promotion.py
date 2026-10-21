"""GT promotion audit and strict request-bound promotion. Dedicated Native only; no replay or transport deadline emulation."""
from typing import Any, Dict
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_promotion as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"audit_niagara_promotion_candidate": ["system_path", "proposed_target_path", "forbidden_dependency_prefixes", "forbidden_source_roots", "allow_temp_target", "allow_temp_dependencies", "timeout_seconds"], "promote_niagara_system_candidate": ["source_system_path", "target_system_path", "dry_run", "forbidden_dependency_prefixes", "forbidden_source_roots", "allow_non_temp_source", "allow_temp_target", "allow_temp_dependencies", "overwrite_existing", "save", "request_compile", "timeout_seconds"]}
DEFAULTS={"audit_niagara_promotion_candidate": {"proposed_target_path": "", "forbidden_dependency_prefixes": None, "forbidden_source_roots": None, "allow_temp_target": False, "allow_temp_dependencies": False, "timeout_seconds": 60}, "promote_niagara_system_candidate": {"dry_run": True, "forbidden_dependency_prefixes": None, "forbidden_source_roots": None, "allow_non_temp_source": False, "allow_temp_target": False, "allow_temp_dependencies": False, "overwrite_existing": False, "save": True, "request_compile": True, "timeout_seconds": 120}}
REQUIRED={"audit_niagara_promotion_candidate": ["system_path"], "promote_niagara_system_candidate": ["source_system_path", "target_system_path"]}
FIELD_TYPES={"audit_niagara_promotion_candidate": {"system_path": "string", "proposed_target_path": "string", "forbidden_dependency_prefixes": "array", "forbidden_source_roots": "array", "allow_temp_target": "boolean", "allow_temp_dependencies": "boolean", "timeout_seconds": "number"}, "promote_niagara_system_candidate": {"source_system_path": "string", "target_system_path": "string", "dry_run": "boolean", "forbidden_dependency_prefixes": "array", "forbidden_source_roots": "array", "allow_non_temp_source": "boolean", "allow_temp_target": "boolean", "allow_temp_dependencies": "boolean", "overwrite_existing": "boolean", "save": "boolean", "request_compile": "boolean", "timeout_seconds": "number"}}
NULLABLE={"audit_niagara_promotion_candidate": ["forbidden_dependency_prefixes", "forbidden_source_roots"], "promote_niagara_system_candidate": ["forbidden_dependency_prefixes", "forbidden_source_roots"]}
ITEMS={"audit_niagara_promotion_candidate": {"forbidden_dependency_prefixes": "string", "forbidden_source_roots": "string"}, "promote_niagara_system_candidate": {"forbidden_dependency_prefixes": "string", "forbidden_source_roots": "string"}}
OBSERVERS=["audit_niagara_promotion_candidate"]
NO_JOURNAL_METHODS=["audit_niagara_promotion_candidate"]
WRITERS=["promote_niagara_system_candidate"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_promotion_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_promotion_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_promotion_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_promotion_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('niagara_promotion_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

LEGACY_MESSAGES={"audit_niagara_promotion_candidate": "Error auditing Niagara promotion candidate: ", "promote_niagara_system_candidate": "Error preparing Niagara promotion candidate request: "}
def _legacy_exception(method,error):
    message=LEGACY_MESSAGES[method]+str(error)
    return dict(success=False,message=message,error=message,error_code='niagara_tool_failed')

_GENERATED_NIAGARA_ROOT="/Game/_MCP_Temp/NiagaraGenerated"
_FORBIDDEN_MCP_REFERENCE_MARKERS=tuple(_FORBIDDEN_MCP_REFERENCE_TOKENS)

def _niagara_error(message,*,error_code='niagara_tool_failed',cause=None,**fields):
    return dict(success=False,message=message,error=message,error_code=error_code,**fields)

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

def _verified_unsaved_temporary_duplicate(result, *, expected_source, expected_target,
        expected_save, expected_dry_run, expected_allow_temp_target,
        expected_allow_temp_dependencies, expected_overwrite_existing,
        forbidden_prefixes, forbidden_roots):
    """Copied creation proof only; the actual on-disk audit stays incomplete."""
    if result.get("unsaved_temporary_duplicate_verified") is not True:
        return False
    if (expected_save or expected_dry_run or not expected_allow_temp_target
            or expected_overwrite_existing or not _is_temp_game_path(expected_target)
            or result.get("public_success_authority") != "verified_unsaved_temporary_duplicate"
            or result.get("on_disk_dependency_audit_deferred") is not True
            or result.get("promotion_contract_complete") is not False
            or result.get("saved") is not False or result.get("save_requested") is not False
            or result.get("promoted") is not True
            or result.get("post_promotion_dependency_audit_complete") is not False
            or result.get("post_promotion_audit_pass") is not False):
        raise RuntimeError("Unsaved temporary duplicate proof contradicts the exact request/authority")
    evidence=result.get("unsaved_temporary_duplicate_evidence")
    if type(evidence) is not dict or evidence.get("profile") != "exact_new_unsaved_temporary_duplicate":
        raise RuntimeError("Exact unsaved temporary duplicate evidence required")
    for key in ("observations_complete", "source_saved_clean", "source_preflight_dependency_audit_complete",
                "source_disk_hash_unchanged", "target_file_absent", "target_identity_current",
                "target_package_ownership_verified"):
        if evidence.get(key) is not True:
            raise RuntimeError("Unsaved temporary duplicate evidence missing: "+key)
    if (_normalize_game_package_path(evidence.get("source_system_path", "")) != expected_source
            or _normalize_game_package_path(evidence.get("target_system_path", "")) != expected_target
            or evidence.get("target_package_name") != expected_target):
        raise RuntimeError("Unsaved temporary duplicate source/target binding differs")
    disk=result.get("post_promotion_dependency_audit")
    if (type(disk) is not dict or any(disk.get(key) is not False
            for key in ("success", "audit_complete", "dependency_query_succeeded", "saved_file_scanned"))):
        raise RuntimeError("Unsaved creation must preserve the actual incomplete on-disk audit")
    hard=evidence.get("hard_reference_evidence")
    if type(hard) is not dict or hard.get("observations_complete") is not True:
        raise RuntimeError("Actual direct hard-reference observation required")
    packages=hard.get("direct_hard_reference_packages")
    if type(packages) is not list or not all(type(v) is str and v.strip() for v in packages):
        raise RuntimeError("Direct hard-reference packages must be strings")
    for key in ("forbidden_dependency_hits", "temp_dependency_hits"):
        if hard.get(key) != []:
            raise RuntimeError("Unsaved temporary duplicate has blocked direct hard references")
    for key in ("has_forbidden_dependency", "has_temp_dependency", "soft_reference_audit_performed",
                "transitive_audit_performed", "full_RAM_dependency_audit_performed", "on_disk_audit_performed"):
        if hard.get(key) is not False:
            raise RuntimeError("Unsaved duplicate must not advertise unsupported dependency evidence: "+key)
    roots=[_normalize_dependency_root(v) for v in forbidden_prefixes+forbidden_roots]
    for package in packages:
        if (_has_forbidden_mcp_reference_marker(package)
                or any(_is_under_dependency_root(package, root) for root in roots if root)
                or not expected_allow_temp_dependencies and _is_temp_game_path(package)):
            raise RuntimeError("Unsaved temporary duplicate direct hard-reference policy failed")
    return True


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

    # Native FResult was decoded and adopted separately; this is only the original semantic legacy leaf.
    if type(response) is not dict or response.get("success") is not True:
        raise RuntimeError("Exact successful Native promotion legacy leaf required")
    result = response
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
        unsaved_verified = _verified_unsaved_temporary_duplicate(result,
            expected_source=expected_source, expected_target=expected_target,
            expected_save=expected_save, expected_dry_run=expected_dry_run,
            expected_allow_temp_target=expected_allow_temp_target,
            expected_allow_temp_dependencies=expected_allow_temp_dependencies,
            expected_overwrite_existing=expected_overwrite_existing,
            forbidden_prefixes=forbidden_prefixes, forbidden_roots=forbidden_roots)
        post_promotion_audit = result.get("post_promotion_dependency_audit")
        _validate_niagara_promotion_dependency_audit(
            post_promotion_audit,
            expected_asset_path=expected_target,
            allow_temp_dependencies=expected_allow_temp_dependencies,
            forbidden_dependency_prefixes=forbidden_prefixes,
            forbidden_source_roots=forbidden_roots,
            audit_label="post-promotion dependency audit",
            require_clean=True,
            require_complete=not unsaved_verified,
        )
        if not unsaved_verified and result.get("post_promotion_dependency_audit_complete") is not True:
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
            and (result.get("post_promotion_audit_pass") is True or unsaved_verified)
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

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_promotion_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_promotion_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method] if field!='timeout_seconds'}
    if method=='audit_niagara_promotion_candidate':
        for field in ('forbidden_dependency_prefixes','forbidden_source_roots'):
            if params[field] is None:params.pop(field)
    else:
        try:
            params['source_system_path']=_normalize_game_package_path(values['source_system_path'])
            params['target_system_path']=_normalize_game_package_path(values['target_system_path'])
            for field in ('forbidden_dependency_prefixes','forbidden_source_roots'):
                value=_coerce_string_list(values[field],field)
                if value:params[field]=value
                else:params.pop(field)
        except Exception as cause:
            error=OperationError('niagara_promotion_invalid_request','Error preparing Niagara promotion candidate request: '+str(cause))
            error.legacy_result=_niagara_error(str(error),error_code=error.code if hasattr(error,'code') else 'niagara_promotion_invalid_request',dispatch_attempted=False)
            raise error from cause
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None,public_values=copy.deepcopy(values)),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_omission_and_value_passthrough_preserved=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=0,public_transport_only_fields=['timeout_seconds'],transport_deadline_emulated=False,Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if method in NO_JOURNAL_METHODS else 'write',original_inferred_risk='low' if method in NO_JOURNAL_METHODS else 'medium',original_native_observer=method in OBSERVERS,promotion_preflight_duplicate_compile_request_save_post_audit_Native_owned=True,public_compile_wait_parameter=False,registry_dependency_scope="direct_on_disk_package_dependencies_not_transitive_or_RAM_closure",actual_save_history_preserved_before_post_guard=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_promotion_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_promotion_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_promotion_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_promotion_audit_projection_and_request_binding_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_promotion_load_getter_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_promotion_operation"

def _promotion_unknown(result,args,error,response=None):
    targets,untrusted=_collect_niagara_promotion_response_paths({'result':response},args['params']['target_system_path'])
    leaf=_niagara_promotion_state_unknown_error('Niagara promotion response was not strictly validated; mutation and save state are unknown. Do not retry before exact-package recovery.',error_code='niagara_promotion_response_state_unknown',recovery_targets=targets,untrusted_response_paths=untrusted,dispatch_attempted=True,dry_run_requested=args['public_values']['dry_run'] is True,target_system_path=str(args['public_values']['target_system_path'] or ''),failure_detail={'type':type(error).__name__,'message':str(error)})
    result['data'].update(legacy_Named_result=leaf,original_promotion_validation_state_unknown=True,original_promotion_recovery=copy.deepcopy(leaf),change_state_known=False,mutation_state_unknown=True,mutation_performed=None,save_state_unknown=True,do_not_retry=True,command_may_still_be_in_flight=True)
    result['changes']=[dict(kind='promote_niagara_system_candidate',attempted=result['data'].get('execution_mutation_attempted'),changed=None,change_state_known=False)]
    result['retry'].update(automatic_retry=False,duplicate_risk='original_strict_promotion_response_unknown_no_replay')
    return OperationError('niagara_promotion_response_state_unknown',leaf['message'])

def _validate_public_promotion(result,args,native):
    values=args['public_values'];params=args['params'];leaf=native['data'].get('legacy_result')
    try:
        _unwrap_niagara_promotion_response(leaf,expected_source_system_path=params['source_system_path'],expected_target_system_path=params['target_system_path'],expected_dry_run=values['dry_run'] is True,expected_save=values['save'] is True,expected_request_compile=values['request_compile'] is True,expected_allow_non_temp_source=values['allow_non_temp_source'] is True,expected_allow_temp_target=values['allow_temp_target'] is True,expected_allow_temp_dependencies=values['allow_temp_dependencies'] is True,expected_overwrite_existing=values['overwrite_existing'] is True,expected_forbidden_dependency_prefixes=params.get('forbidden_dependency_prefixes',[]),expected_forbidden_source_roots=params.get('forbidden_source_roots',[]))
    except Exception as error:raise _promotion_unknown(result,args,error,leaf) from error
    result['data']['strict_public_promotion_response_validated']=True

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_promotion_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_promotion_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_promotion_operation'
        if legacy is None and method=='promote_niagara_system_candidate' and getattr(error,'native_call_dispatched',None) is not False:
            unknown=_promotion_unknown(result,args,error)
            if callback:callback(result)
            raise unknown from error
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native.get('operation_complete') is False or native['data'].get('operation_complete') is False:
        result['data'].update(operation_complete=False,in_flight=True,command_may_still_be_in_flight=True,automatic_action_retry=False,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=method,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='deferred_resource_binding_completion_in_flight_no_replay')
        if callback:callback(result)
        raise OperationError('niagara_promotion_incomplete','Native observation is incomplete; actual partial prefix retained without synthetic completion or replay')
    if legacy is None and method=='promote_niagara_system_candidate':
        try:_validate_public_promotion(result,args,native)
        except Exception:
            if callback:callback(result)
            raise
    if method in OBSERVERS and (any(native[key] is not False for key in ('mutation_attempted','compile_requested','compile_attempted','save_requested','save_attempted','saved')) or native['compiled'] is not False):
        raise OperationError('niagara_promotion_observer_contract_violated','Explicit observer mutation/compile/save must be False; actual prefix retained')
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_promotion_native_failed',native.get('error') or 'Native niagara_promotion failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_promotion_postcondition_unverified','Native niagara_promotion postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_promotion_input_validation';result['changes']=[]
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    if hasattr(error,'legacy_result'):result['data']['legacy_result']=copy.deepcopy(error.legacy_result)
    if hasattr(error,'public_validation_errors'):result['data'].update(public_validation_errors=copy.deepcopy(error.public_validation_errors),original_framework_error_text_reproduced=False,tool_body_invoked=False)
    return result

def _rejected(method,error):return _rejection(state.new_result(),method,error)

def _named(method,arguments,_public_input=False):
    try:args,original,legacy=_arguments(method,arguments,None,_public_input)
    except Exception as error:
        rejected=_rejected(method,error);rejected['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',None if hasattr(error,'public_validation_errors') else _legacy_exception(method,error)));return rejected
    if method in NO_JOURNAL_METHODS:
        result=state.new_result()
        try:return _run(result,method,args,original,legacy)
        except Exception as error:return state.fail(result,error)
    return state.mutate(method,arguments,None,lambda result:_run(result,method,args,original,legacy,state.checkpoint))

def _action(result,method,arguments,callback,legacy,public_input=False):
    result["data"]["parent_journal_only"]=True
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)

def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)

def audit_niagara_promotion_candidate(system_path,proposed_target_path="",forbidden_dependency_prefixes=None,forbidden_source_roots=None,allow_temp_target=False,allow_temp_dependencies=False,timeout_seconds=60.0):
    return _named("audit_niagara_promotion_candidate",dict(system_path=system_path,proposed_target_path=proposed_target_path,forbidden_dependency_prefixes=forbidden_dependency_prefixes,forbidden_source_roots=forbidden_source_roots,allow_temp_target=allow_temp_target,allow_temp_dependencies=allow_temp_dependencies,timeout_seconds=timeout_seconds))

def audit_niagara_promotion_candidate_action(result,system_path,proposed_target_path="",forbidden_dependency_prefixes=None,forbidden_source_roots=None,allow_temp_target=False,allow_temp_dependencies=False,timeout_seconds=60.0,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"audit_niagara_promotion_candidate",dict(system_path=system_path,proposed_target_path=proposed_target_path,forbidden_dependency_prefixes=forbidden_dependency_prefixes,forbidden_source_roots=forbidden_source_roots,allow_temp_target=allow_temp_target,allow_temp_dependencies=allow_temp_dependencies,timeout_seconds=timeout_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def promote_niagara_system_candidate(source_system_path,target_system_path,dry_run=True,forbidden_dependency_prefixes=None,forbidden_source_roots=None,allow_non_temp_source=False,allow_temp_target=False,allow_temp_dependencies=False,overwrite_existing=False,save=True,request_compile=True,timeout_seconds=120.0):
    return _named("promote_niagara_system_candidate",dict(source_system_path=source_system_path,target_system_path=target_system_path,dry_run=dry_run,forbidden_dependency_prefixes=forbidden_dependency_prefixes,forbidden_source_roots=forbidden_source_roots,allow_non_temp_source=allow_non_temp_source,allow_temp_target=allow_temp_target,allow_temp_dependencies=allow_temp_dependencies,overwrite_existing=overwrite_existing,save=save,request_compile=request_compile,timeout_seconds=timeout_seconds))

def promote_niagara_system_candidate_action(result,source_system_path,target_system_path,dry_run=True,forbidden_dependency_prefixes=None,forbidden_source_roots=None,allow_non_temp_source=False,allow_temp_target=False,allow_temp_dependencies=False,overwrite_existing=False,save=True,request_compile=True,timeout_seconds=120.0,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"promote_niagara_system_candidate",dict(source_system_path=source_system_path,target_system_path=target_system_path,dry_run=dry_run,forbidden_dependency_prefixes=forbidden_dependency_prefixes,forbidden_source_roots=forbidden_source_roots,allow_non_temp_source=allow_non_temp_source,allow_temp_target=allow_temp_target,allow_temp_dependencies=allow_temp_dependencies,overwrite_existing=overwrite_existing,save=save,request_compile=request_compile,timeout_seconds=timeout_seconds),checkpoint_callback,_legacy_command_params,_public_input)
