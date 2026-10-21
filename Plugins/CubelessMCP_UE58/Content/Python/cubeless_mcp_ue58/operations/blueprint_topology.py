"""Original static topology15; Native owns classifications, filters and limits.

Named observation does not enter a mutation journal. Batch uses its parent only.
Getter/load/cache effects are qualified by current Native selected-state proof.
"""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_topology as api
from . import state
from .state import OperationError

METHOD = api.BINDINGS[0]
_PUBLIC_FIELDS = ('blueprint_name', 'graph_name', 'graph_id', 'graph_type', 'graph_name_contains', 'node_type', 'title_contains', 'reference_contains', 'include_pins', 'include_links', 'include_references', 'max_graphs', 'max_nodes_per_graph', 'max_links_per_graph', 'max_references_per_node')


def _arguments(public):
    if set(public) != set(_PUBLIC_FIELDS):
        raise OperationError("blueprint_topology_input_invalid", "Only original topology15 fields admitted")
    for field in _PUBLIC_FIELDS[:8]:
        if type(public[field]) is not str:
            raise OperationError("blueprint_topology_input_invalid", "Original string type differs: " + field)
    for field in _PUBLIC_FIELDS[8:11]:
        if type(public[field]) is not bool:
            raise OperationError("blueprint_topology_input_invalid", "Original boolean type differs: " + field)
    for field in _PUBLIC_FIELDS[11:]:
        if type(public[field]) is not int or not -(1 << 63) <= public[field] < (1 << 63):
            raise OperationError("blueprint_topology_input_invalid", "Exact safe int64 limit carrier required: " + field)
    # Preserve -1/0 and every supplied int64: Native owns original clamping.
    return {"method": METHOD, "public": copy.deepcopy(public), "hidden": dict(public)}


def _context(result, arguments):
    result["data"].update(operation=METHOD, adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]), direct_named_mutation_journal_performed=False,
        native_static_classification_filter_links_reference_limits_authoritative=True,
        getter_cache_load_effects_not_total_zero=True, automatic_action_retry=False,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        no_Python_limit_clamp_filter_topology_rewrite=True, actual_official_raw_wire_verified=False,
        full_runtime_cold_or_unique_pipeline_verified=False)


def _apply_native(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_raw_result=native["_raw_native_json"],
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known and native["mutation_attempted"] else False,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_not_attempted" if not native["compile_attempted"] else "native_returned_compiled" if native["compiled"] is True else "native_returned_compile_failed" if native["compiled"] is False else "native_returned_compile_unverified")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_blueprint_topology_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        SDK_getter_cache_load_effects="not_total_sideeffect_zero")
    result["stage"] = "returned_blueprint_topology_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or (lambda observed: None)
    _context(result, arguments)
    result["compile"].update(requested=arguments["public"].get("compile", False), attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=arguments["public"].get("save", False), attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_read_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_topology_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=False, mutation_state_unknown=True, observed_state_changed=None)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments["method"])(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_blueprint_topology_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_topology_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_topology_native_failed", native.get("error") or "Native full topology/current observation prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_topology_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_returned_observation_prefix_no_automatic_replay"



def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def inspect_blueprint_graph_call_topology(blueprint_name, graph_name='', graph_id='', graph_type='', graph_name_contains='', node_type='', title_contains='', reference_contains='', include_pins=False, include_links=True, include_references=True, max_graphs=64, max_nodes_per_graph=512, max_links_per_graph=1024, max_references_per_node=64):
    try:
        arguments = _arguments(dict(blueprint_name=blueprint_name, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, graph_name_contains=graph_name_contains, node_type=node_type, title_contains=title_contains, reference_contains=reference_contains, include_pins=include_pins, include_links=include_links, include_references=include_references, max_graphs=max_graphs, max_nodes_per_graph=max_nodes_per_graph, max_links_per_graph=max_links_per_graph, max_references_per_node=max_references_per_node))
    except Exception as error:
        return _rejected(error)
    result = state.new_result()
    try:
        _run_action(result, arguments)
    except Exception as error:
        state.fail(result, error)
    return result


def inspect_blueprint_graph_call_topology_action(result, blueprint_name, graph_name='', graph_id='', graph_type='', graph_name_contains='', node_type='', title_contains='', reference_contains='', include_pins=False, include_links=True, include_references=True, max_graphs=64, max_nodes_per_graph=512, max_links_per_graph=1024, max_references_per_node=64, checkpoint_callback=None):
    _run_action(result, _arguments(dict(blueprint_name=blueprint_name, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, graph_name_contains=graph_name_contains, node_type=node_type, title_contains=title_contains, reference_contains=reference_contains, include_pins=include_pins, include_links=include_links, include_references=include_references, max_graphs=max_graphs, max_nodes_per_graph=max_nodes_per_graph, max_links_per_graph=max_links_per_graph, max_references_per_node=max_references_per_node)), checkpoint_callback)
