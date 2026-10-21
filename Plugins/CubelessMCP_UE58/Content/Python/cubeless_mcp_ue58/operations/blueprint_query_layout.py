"""Original exact Event GUID query and guarded composite node positions.

Graph resolution can create the original default EventGraph. Both named calls
use one journal; actual Native flags decide the current read/write branch.
Whole Any inputs and update rows remain opaque; no row coercion or second work.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_query_layout as api
from . import state
from .state import OperationError

FIND, POSITIONS = api.BINDINGS
_PUBLIC_FIELDS = {FIND: ("blueprint_name", "node_type", "event_type", "graph_name", "graph_id", "graph_type"),
    POSITIONS: ("blueprint_name", "graph_name", "updates", "graph_id", "graph_type", "dry_run", "compile", "save")}


def _json(value):
    try:
        text = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        text.encode("utf-8")
        return text
    except (TypeError, ValueError, UnicodeError, RecursionError) as error:
        raise OperationError("blueprint_query_layout_input_invalid", "Bounded whole JSON carrier required") from error


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("blueprint_query_layout_input_invalid", "Only this original method's exact fields are admitted")
    strings = ("blueprint_name", "graph_name", "graph_id", "graph_type")
    for field in strings:
        if type(public[field]) is not str:
            raise OperationError("blueprint_query_layout_input_invalid", "Original selector type differs: " + field)
    hidden = {key: public[key] for key in strings}
    if method == FIND:
        # Original untyped fields always go on wire, including None. Native's
        # TryGetStringField handles default null/missing-type before asset load,
        # exact Event/name lookup and other-string empty results.
        hidden["node_type_json"] = _json(public["node_type"])
        hidden["event_type_json"] = _json(public["event_type"])
    else:
        if type(public["updates"]) is not list or any(type(row) is not dict for row in public["updates"]):
            raise OperationError("blueprint_query_layout_input_invalid", "Original updates requires an array of objects")
        for field in ("dry_run", "compile", "save"):
            if type(public[field]) is not bool:
                raise OperationError("blueprint_query_layout_input_invalid", "Original option type differs: " + field)
            hidden[field] = public[field]
        # No GUID/class/title/coordinate/default/schema coercion or filtering.
        # Empty arrays, nullable/untyped row values and ignored keys reach Native.
        hidden["updates_json"] = _json(public["updates"])
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]),
        original_finder_graph_resolution_may_create_default_EventGraph=arguments["method"] == FIND,
        current_read_only_or_mutation_branch_native_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        original_find_exact_Event_and_event_type_to_event_name_native_authoritative=True,
        no_invented_broad_node_filter=True, original_untyped_query_JSON_preserved=True,
        native_all_row_prewrite_Guid_class_FullTitle_expected_int32_guards_authoritative=True,
        no_Python_dependency_guard_row_coercion_or_coordinate_write=True,
        native_requested_compile_save_even_matching_apply_not_optimized_away=True,
        native_postcompile_postsave_graph_and_GUID_relookup_authoritative=True,
        original_graph_resolution_and_mutation_are_not_atomic=True,
        full_original_runtime_cold_or_unique_pipeline_verified=False)


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
    result["validation"].update(status="native_blueprint_query_layout_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        implicit_structural_graph_callbacks="not_collapsed_into_direct_compiler_count")
    result["stage"] = "returned_blueprint_query_layout_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=arguments["public"].get("compile", False), attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=arguments["public"].get("save", False), attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_query_layout_operation"
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
            result["stage"] = "returned_blueprint_query_layout_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_query_layout_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_query_layout_native_failed", native.get("error") or "Native query/layout/compile/save/readback prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_query_layout_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_graph_GUID_positions_creation_compile_save_receipts_and_journal_no_writer_replay"


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def find_blueprint_nodes(blueprint_name, node_type=None, event_type=None, graph_name='', graph_id='', graph_type=''):
    try:
        arguments = _arguments(FIND, dict(blueprint_name=blueprint_name, node_type=node_type, event_type=event_type,
            graph_name=graph_name, graph_id=graph_id, graph_type=graph_type))
    except Exception as error:
        return _rejected(FIND, error)
    return _named(arguments)


def find_blueprint_nodes_action(result, blueprint_name, node_type=None, event_type=None, graph_name='', graph_id='', graph_type='', checkpoint_callback=None):
    _run_action(result, _arguments(FIND, dict(blueprint_name=blueprint_name, node_type=node_type, event_type=event_type,
        graph_name=graph_name, graph_id=graph_id, graph_type=graph_type)), checkpoint_callback)


def set_blueprint_node_positions(blueprint_name, graph_name, updates, graph_id='', graph_type='', dry_run=True, compile=False, save=False):
    try:
        arguments = _arguments(POSITIONS, dict(blueprint_name=blueprint_name, graph_name=graph_name, updates=updates,
            graph_id=graph_id, graph_type=graph_type, dry_run=dry_run, compile=compile, save=save))
    except Exception as error:
        return _rejected(POSITIONS, error)
    return _named(arguments)


def set_blueprint_node_positions_action(result, blueprint_name, graph_name, updates, graph_id='', graph_type='', dry_run=True, compile=False, save=False, checkpoint_callback=None):
    _run_action(result, _arguments(POSITIONS, dict(blueprint_name=blueprint_name, graph_name=graph_name, updates=updates,
        graph_id=graph_id, graph_type=graph_type, dry_run=dry_run, compile=compile, save=save)), checkpoint_callback)
