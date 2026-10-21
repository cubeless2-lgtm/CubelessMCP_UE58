"""Original graph text import and malformed link repair through one native action.

Inline text/file precedence, safety/sample admission, tolerant raw-link scans,
actual composite callbacks, compiler and direct forced save remain Native-owned.
Legacy GeneratedClass compile predicate is separate from current compiler health.
Returned raw/count/compile/save prefixes precede errors; no replay or second save.
"""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import graph_transfer as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'import_blueprint_graph_nodes_from_text': ('blueprint_name', 'text', 'text_file_path', 'graph_name', 'graph_id', 'graph_type', 'clear_graph', 'keep_root_node', 'allow_non_sample', 'validate_only', 'dry_run', 'compile', 'save_asset'), 'repair_blueprint_pin_links': ('blueprint_name', 'allow_non_sample', 'validate_only', 'dry_run', 'compile', 'save_asset')}
_BOOLEAN_FIELDS = {'import_blueprint_graph_nodes_from_text': ('clear_graph', 'keep_root_node', 'allow_non_sample', 'validate_only', 'dry_run', 'compile', 'save_asset'), 'repair_blueprint_pin_links': ('allow_non_sample', 'validate_only', 'dry_run', 'compile', 'save_asset')}


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("graph_transfer_input_invalid", "Only exact original import/repair fields admitted")
    for field, value in public.items():
        if type(value) is not (bool if field in _BOOLEAN_FIELDS[method] else str):
            raise OperationError("graph_transfer_input_invalid", "Original field type differs: " + field)
    # No original Python guard, trim, file read, text parse or selector coercion.
    return {"method": method, "public": copy.deepcopy(public), "hidden": copy.deepcopy(public)}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], raw_public_arguments=copy.deepcopy(arguments["public"]),
        requested_arguments=copy.deepcopy(arguments["hidden"]), adapter_explicit_asset_load_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0, adapter_file_read_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        native_text_file_precedence_sample_guard_paste_and_tolerant_link_repair_authoritative=True,
        public_compile_and_save_intents_retained_even_when_native_validate_or_dry_suppresses_attempts=True,
        legacy_GeneratedClass_compiled_and_success_separate_from_current_compiler_health=True,
        original_direct_forced_save_independent_of_compiler_predicate=True,
        compile_save_or_matching_repair_callbacks_not_assumed_zero=True,
        cleanup_or_clear_import_is_not_atomic_rollback=True,
        native_composite_prefix="Opaque text/file/preflight/clear/paste/repair/link/source/compiler/forced-save attempts and returns; accepted prefix retained before errors",
        no_new_Python_dependency_guard_or_malformed_pin_repair=True,
        full_original_touch_runtime_cold_or_external_pipeline_verified=False)


def _apply_native(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_raw_result=native["_raw_native_json"],
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_not_attempted" if not native["compile_attempted"] else "native_returned_compiled" if native["compiled"] is True else "native_returned_compile_failed" if native["compiled"] is False else "native_returned_compile_unverified")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_graph_transfer_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", legacy_GeneratedClass_predicate="retained_in_native_data_not_current_compile_validation",
        implicit_structural_or_paste_callbacks="not_collapsed_into_direct_compiler_count")
    result["stage"] = "returned_graph_transfer_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=arguments["public"]["compile"], attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=arguments["public"]["save_asset"], attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_mutation=copy.deepcopy(arguments))
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_graph_transfer_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments["method"])(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_graph_transfer_operation"
            callback(result)
        if getattr(error, "code", "") == "graph_transfer_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "graph_transfer_native_failed", native.get("error") or "Native actual import/repair/compiler/save prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("graph_transfer_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_text_import_removed_nodes_raw_links_source_compile_save_receipts_and_journal_no_accepted_action_replay"



def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))



def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def import_blueprint_graph_nodes_from_text(blueprint_name, text='', text_file_path='', graph_name='', graph_id='', graph_type='', clear_graph=False, keep_root_node=True, allow_non_sample=False, validate_only=False, dry_run=False, compile=True, save_asset=False):
    try:
        arguments = _arguments('import_blueprint_graph_nodes_from_text', {'blueprint_name': blueprint_name, 'text': text, 'text_file_path': text_file_path, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'clear_graph': clear_graph, 'keep_root_node': keep_root_node, 'allow_non_sample': allow_non_sample, 'validate_only': validate_only, 'dry_run': dry_run, 'compile': compile, 'save_asset': save_asset})
    except Exception as error:
        return _rejected('import_blueprint_graph_nodes_from_text', error)
    return _named(arguments)


def import_blueprint_graph_nodes_from_text_action(result, blueprint_name, text='', text_file_path='', graph_name='', graph_id='', graph_type='', clear_graph=False, keep_root_node=True, allow_non_sample=False, validate_only=False, dry_run=False, compile=True, save_asset=False, checkpoint_callback=None):
    _run_action(result, _arguments('import_blueprint_graph_nodes_from_text', {'blueprint_name': blueprint_name, 'text': text, 'text_file_path': text_file_path, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'clear_graph': clear_graph, 'keep_root_node': keep_root_node, 'allow_non_sample': allow_non_sample, 'validate_only': validate_only, 'dry_run': dry_run, 'compile': compile, 'save_asset': save_asset}), checkpoint_callback)


def repair_blueprint_pin_links(blueprint_name, allow_non_sample=False, validate_only=False, dry_run=False, compile=True, save_asset=False):
    try:
        arguments = _arguments('repair_blueprint_pin_links', {'blueprint_name': blueprint_name, 'allow_non_sample': allow_non_sample, 'validate_only': validate_only, 'dry_run': dry_run, 'compile': compile, 'save_asset': save_asset})
    except Exception as error:
        return _rejected('repair_blueprint_pin_links', error)
    return _named(arguments)


def repair_blueprint_pin_links_action(result, blueprint_name, allow_non_sample=False, validate_only=False, dry_run=False, compile=True, save_asset=False, checkpoint_callback=None):
    _run_action(result, _arguments('repair_blueprint_pin_links', {'blueprint_name': blueprint_name, 'allow_non_sample': allow_non_sample, 'validate_only': validate_only, 'dry_run': dry_run, 'compile': compile, 'save_asset': save_asset}), checkpoint_callback)
