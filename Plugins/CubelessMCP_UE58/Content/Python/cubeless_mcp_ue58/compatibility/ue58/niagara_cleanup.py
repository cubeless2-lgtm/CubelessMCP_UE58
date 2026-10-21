"""UE5.8 dedicated NiagaraCleanup Native1 x6. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

import subprocess
import time
import logging
from pathlib import Path
logger=logging.getLogger(__name__)

LIBRARY='CubelessNiagaraCleanupLibrary'
BINDINGS=["remove_niagara_modules","remove_niagara_parameter_map_set_nodes","remove_niagara_orphan_parameter_map_get_nodes","remove_niagara_user_parameters","remove_niagara_simulation_stage","remove_niagara_scratch_pad_script"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"remove_niagara_modules":["system_path","module_names","module_name_prefixes","module_name_contains","module_name","emitter_index","emitter_name","include_system_modules","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds"],"remove_niagara_parameter_map_set_nodes":["system_path","parameter_pin_names","parameter_pin_name","node_guid","emitter_index","emitter_name","dry_run","allow_multiple","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds","parameter_name"],"remove_niagara_orphan_parameter_map_get_nodes":["system_path","parameter_pin_names","parameter_pin_name","node_guid","emitter_index","emitter_name","dry_run","allow_multiple","only_if_unlinked","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds","parameter_name"],"remove_niagara_user_parameters":["system_path","parameter_names","parameter_name","dry_run","remove_graph_references","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds","continue_on_error"],"remove_niagara_simulation_stage":["system_path","emitter_index","emitter_name","stage_index","stage_name","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds"],"remove_niagara_scratch_pad_script":["system_path","scratch_pad_name","scratch_pad_owner_kind","scratch_pad_script_index","scratch_pad_emitter_index","scratch_pad_emitter_name","force","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds","scratch_pad_script_name"]}


BINDINGS.append('cleanup_generated_niagara_assets_offline')
PARAM_FIELDS['cleanup_generated_niagara_assets_offline']=['package_paths', 'name_prefix', 'older_than_minutes', 'dry_run', 'require_editor_closed', 'allow_all']

def binding_status():return {name:(all(callable(getattr(getattr(unreal,'Paths',None),n,None)) for n in ('project_content_dir','convert_relative_path_to_full')) if name=='cleanup_generated_niagara_assets_offline' else callable(getattr(getattr(unreal,LIBRARY,None),name,None))) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_cleanup_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_cleanup_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_cleanup_native_api_unavailable','Build/load the exact NiagaraCleanup Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_cleanup_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def remove_niagara_modules(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remove_niagara_modules",text)

def remove_niagara_parameter_map_set_nodes(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remove_niagara_parameter_map_set_nodes",text)

def remove_niagara_orphan_parameter_map_get_nodes(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remove_niagara_orphan_parameter_map_get_nodes",text)

def remove_niagara_user_parameters(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remove_niagara_user_parameters",text)

def remove_niagara_simulation_stage(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remove_niagara_simulation_stage",text)

def remove_niagara_scratch_pad_script(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remove_niagara_scratch_pad_script",text)

from typing import Any,Dict


# Original filesystem-only generated Niagara cleanup. No UObject load/delete or RPC.
_GENERATED_NIAGARA_ROOT="/Game/_MCP_Temp/NiagaraGenerated"
_NIAGARA_PACKAGE_SIDECARS=(".uasset",".uexp",".ubulk",".uptnl",".m.ubulk",".upayload")


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


def _package_path_to_asset_file(package_path: str, content_root: Path) -> Path:
    normalized = _normalize_niagara_package_path(package_path)
    rel = normalized[len("/Game/") :]
    candidate = (content_root / rel).with_suffix(".uasset").resolve()
    root = (content_root / "_MCP_Temp" / "NiagaraGenerated").resolve()
    if not candidate.is_relative_to(root):
        raise ValueError(f"Resolved path escapes generated Niagara root: {package_path}")
    return candidate


def _asset_file_to_package_path(asset_file: Path, content_root: Path) -> str:
    rel = asset_file.resolve().relative_to(content_root.resolve())
    return "/Game/" + rel.with_suffix("").as_posix()


def cleanup_generated_niagara_assets_offline_impl(
    package_paths: list[str] | None = None,
    name_prefix: str = "",
    older_than_minutes: float | None = None,
    dry_run: bool = True,
    require_editor_closed: bool = True,
    allow_all: bool = False,
    *, content_root: Path, receipt: dict, checkpoint=None,
) -> Dict[str, Any]:
    """Delete generated Niagara package files when the Unreal editor is closed."""
    try:
        root = (content_root / "_MCP_Temp" / "NiagaraGenerated").resolve()
        if not root.is_relative_to(content_root.resolve()):
            return _offline_error(
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
            return _offline_error(
                "name_prefix must be an asset-name prefix, not a path or glob",
                error_code="niagara_cleanup_invalid_name_prefix",
                name_prefix=prefix,
            )
        if older_than_minutes is not None and older_than_minutes < 0:
            return _offline_error(
                "older_than_minutes must be non-negative when provided",
                error_code="niagara_cleanup_invalid_age",
            )
        if not explicit_paths and not prefix and not allow_all:
            return _offline_error(
                "Refusing broad cleanup: provide package_paths, name_prefix, or allow_all=true",
                error_code="niagara_cleanup_refused_broad_delete",
                root=str(root),
                dry_run=dry_run,
            )

        editor_running, editor_query_error = _query_unreal_editor_running()
        receipt.update(editor_running=editor_running,editor_query_error=editor_query_error,editor_state_known=editor_running is not None)
        if require_editor_closed and editor_running is None and not dry_run:
            return _offline_error(
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
            return _offline_error(
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
                asset_file = _package_path_to_asset_file(package_path, content_root)
                package_base = asset_file.with_suffix("")
                candidates[package_base] = _asset_file_to_package_path(package_base, content_root)
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
                        candidates[package_base] = _asset_file_to_package_path(package_base, content_root)
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
        receipt.update(selected=selected,skipped=skipped,preflight_errors=errors,deleted=deleted,delete_errors=delete_errors)
        if not dry_run:
            if errors:
                return _offline_error(
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
                receipt.update(pre_delete_editor_recheck=True,final_editor_running=final_editor_running,final_editor_query_error=final_editor_query_error)
                if final_editor_running is not False:
                    return _offline_error(
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
                    receipt['stage']='before_niagara_cleanup_checkpoint'
                    if checkpoint:
                        try:checkpoint(receipt)
                        except Exception:
                            if receipt['mutation_attempted']:receipt['change_state_known']=False
                            receipt.update(checkpoint_failed=True,do_not_retry=True,post_failure_file_observation_skipped=True)
                            raise
                    # Journal callbacks are new boundaries; do not use a redirected old path.
                    fresh_content=content_root.resolve();fresh_root=(content_root / '_MCP_Temp' / 'NiagaraGenerated').resolve()
                    fresh_file=Path(file_path).resolve()
                    if fresh_content!=content_root or fresh_root!=root or fresh_file!=Path(file_path) or not fresh_file.is_relative_to(root):
                        receipt.update(change_state_known=False,post_failure_file_observation_skipped=True,
                            path_current_mismatch=dict(expected_content=str(content_root),actual_content=str(fresh_content),expected_root=str(root),actual_root=str(fresh_root),expected_file=file_path,actual_file=str(fresh_file)))
                        raise RuntimeError('Offline cleanup path changed after checkpoint; no next unlink')
                    row=dict(file=file_path,entered=True,returned=False)
                    receipt['unlink_calls'].append(row);receipt['mutation_attempted']=True
                    try:
                        Path(file_path).unlink()
                        row['returned']=True;receipt['observed_changed']=True
                        item_deleted.append(file_path)
                    except Exception as exc:
                        row.update(raised=True,error=str(exc),exception_type=type(exc).__name__)
                        delete_errors.append({"file": file_path, "error": str(exc)})
                        # Known unlink syscall failure is an original per-file failure; unexpected
                        # Python exceptions retain conservative uncertainty without replay.
                        if not isinstance(exc,OSError):
                            receipt['change_state_known']=False;raise
                    receipt['stage']='returned_niagara_cleanup_operation'
                    if checkpoint:
                        try:checkpoint(receipt)
                        except Exception:
                            if receipt['mutation_attempted']:receipt['change_state_known']=False
                            receipt.update(checkpoint_failed=True,do_not_retry=True,post_failure_file_observation_skipped=True)
                            raise
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
        return _offline_error(f"Error cleaning generated Niagara assets offline: {exc}")


def _offline_error(message,*,error_code='niagara_tool_failed',**fields):
    return dict(success=False,message=message,error=message,error_code=error_code,**fields)


def execute_offline_cleanup(params,receipt,checkpoint=None):
    receipt.update(operation='cleanup_generated_niagara_assets_offline',stage='preflight',mutation_attempted=False,
        observed_changed=False,change_state_known=True,unlink_calls=[],compile_requested=False,compile_attempted=False,
        save_requested=False,save_attempted=False,saved=False,operation_complete=False,automatic_retry_permitted=False,
        native_dispatched=False,server_RPC_dependency=False,filesystem_only=True,project_content_path_query_calls=0)
    try:
        # Public project-path mapping only; the cleanup itself never queries/loads UObjects.
        raw_content=unreal.Paths.project_content_dir();receipt['project_content_path_query_calls']=1
        full_content=unreal.Paths.convert_relative_path_to_full(raw_content);receipt['content_full_path_query_calls']=1
        content_root=Path(full_content).resolve()
        receipt.update(content_root=str(content_root))
        leaf=cleanup_generated_niagara_assets_offline_impl(**params,content_root=content_root,receipt=receipt,checkpoint=checkpoint)
        receipt.update(legacy_result=leaf,operation_complete=True,stage='returned_niagara_cleanup_operation')
        return receipt
    except Exception as error:
        # A checkpoint/storage interruption is not permission to continue deleting.
        if receipt['mutation_attempted']:receipt['change_state_known']=False
        receipt.update(error=str(error),error_code=getattr(error,'code','niagara_cleanup_offline_interrupted'),
            do_not_retry=True,post_failure_file_observation_skipped=True,
            legacy_result=_offline_error('Error cleaning generated Niagara assets offline: '+str(error)))
        return receipt
