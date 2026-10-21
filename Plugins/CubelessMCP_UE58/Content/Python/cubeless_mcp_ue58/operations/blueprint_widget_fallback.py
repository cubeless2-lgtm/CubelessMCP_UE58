"""Original fallback analyzer3 with native count diagnostics and report publication.

Native scans/loads source and target assets, compares seven structure counts and
publishes a unique checked report. A false legacy verification_pass is a valid
complete diagnostic, distinct from incomplete observations or publication failure.
No Python asset/report composition, compiler, package save or replay is added.
"""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_widget_fallback as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'analyze_blueprint_widget_fallbacks_mcp': ('source_root', 'target_root', 'suffix')}
_FIELD_KINDS = {'analyze_blueprint_widget_fallbacks_mcp': {'source_root': 'string', 'target_root': 'string', 'suffix': 'string'}}

def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("blueprint_widget_fallback_input_invalid", "Only original fallback analyzer public3 fields admitted")
    if any(type(value) is not str for value in public.values()):
        raise OperationError("blueprint_widget_fallback_input_invalid", "Original public string types required")
    # No trim, root normalization, pairing, asset lookup or report interpretation in Python.
    return {"method": method, "public": copy.deepcopy(public), "hidden": copy.deepcopy(public)}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]), original_public_contract_preserved=True,
        selected_native_backend="Blueprint_Widget_seven_count_diagnostic_and_checked_report_publication",
        native_report_publication_and_file_prefix_authoritative=True,
        legacy_verification_pass_is_diagnostic_not_operation_success_gate=True,
        legacy_report_path_provenance_separate_from_actual_unique_report_path=True,
        count_match_does_not_certify_topology_defaults_or_runtime_equivalence=True,
        report_writer_journal_required=True, native_registry_scan_asset_load_and_cache_effects_not_total_zero=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_explicit_report_write_calls=0, no_added_public_compile_or_save_options=True,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        full_fallback_recreation_runtime_cold_or_unique_pipeline_verified=False)


def _apply_native(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_raw_result=native["_raw_native_json"],
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=(native["changed"] if known else None) if native["mutation_attempted"] else False,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_not_attempted" if not native["compile_attempted"] else "native_returned_compiled" if native["compiled"] is True else "native_returned_compile_failed" if native["compiled"] is False else "native_returned_compile_unverified")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": None, "scope": "native_diagnostic_report_publication_and_observed_prefix", "source_root": arguments["public"]["source_root"], "target_root": arguments["public"]["target_root"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_blueprint_widget_fallback_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        native_compile_save_and_structural_callbacks="actual_native_prefix")
    result["stage"] = "returned_blueprint_widget_fallback_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_widget_fallback_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": None, "scope": "native_diagnostic_report_publication_and_observed_prefix", "source_root": arguments["public"]["source_root"], "target_root": arguments["public"]["target_root"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments["method"])(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_blueprint_widget_fallback_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_widget_fallback_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_widget_fallback_native_failed", native.get("error") or "Native diagnostic/load/registry/report publication and actual unknown prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_widget_fallback_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_fallback_report_native_prefix_and_journal_no_replay"





def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def analyze_blueprint_widget_fallbacks_mcp(source_root, target_root, suffix='_MCP'):
    try:
        arguments = _arguments("analyze_blueprint_widget_fallbacks_mcp", dict(source_root=source_root, target_root=target_root, suffix=suffix))
    except Exception as error:
        return _rejected("analyze_blueprint_widget_fallbacks_mcp", error)
    return _named(arguments)


def analyze_blueprint_widget_fallbacks_mcp_action(result, source_root, target_root, suffix='_MCP', checkpoint_callback=None):
    _run_action(result, _arguments("analyze_blueprint_widget_fallbacks_mcp", dict(source_root=source_root, target_root=target_root, suffix=suffix)), checkpoint_callback)
