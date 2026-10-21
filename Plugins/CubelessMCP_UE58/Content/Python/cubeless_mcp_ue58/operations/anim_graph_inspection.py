"""Original authored AnimGraph settings and protected pose topology.

Both names use one potential-writer journal: all-empty selectors may create the
original default EventGraph before AnimationGraph type refusal. Native owns
reflection, filters, limits, pins, links and actual current read/creation prefix.
"""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_inspection as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'inspect_anim_graph_node_settings': ('blueprint_name', 'node_id', 'node_type', 'title_contains', 'include_pins', 'max_depth', 'graph_name', 'graph_id', 'graph_type'), 'inspect_anim_graph_protected_topology': ('blueprint_name', 'node_id', 'node_type', 'title_contains', 'include_pins', 'include_pose_pins', 'include_links', 'include_non_pose_links', 'include_settings', 'max_depth', 'max_nodes', 'max_links', 'graph_name', 'graph_id', 'graph_type')}
_FIELD_KINDS = {'inspect_anim_graph_node_settings': {'blueprint_name': 'string', 'node_id': 'string', 'node_type': 'string', 'title_contains': 'string', 'include_pins': 'boolean', 'max_depth': 'integer', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string'}, 'inspect_anim_graph_protected_topology': {'blueprint_name': 'string', 'node_id': 'string', 'node_type': 'string', 'title_contains': 'string', 'include_pins': 'boolean', 'include_pose_pins': 'boolean', 'include_links': 'boolean', 'include_non_pose_links': 'boolean', 'include_settings': 'boolean', 'max_depth': 'integer', 'max_nodes': 'integer', 'max_links': 'integer', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string'}}


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("anim_graph_inspection_input_invalid", "Only original method fields admitted")
    for field, kind in _FIELD_KINDS[method].items():
        expected = bool if kind == "boolean" else int if kind == "integer" else str
        if type(public[field]) is not expected:
            raise OperationError("anim_graph_inspection_input_invalid", "Original public type differs: " + field)
        if kind == "integer" and not -(1 << 63) <= public[field] < (1 << 63):
            raise OperationError("anim_graph_inspection_input_invalid", "Exact safe int64 limit carrier required: " + field)
    # Original selectors/filters/limits are untouched. Native clamps/checks scope.
    return {"method": method, "public": copy.deepcopy(public), "hidden": dict(public)}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]),
        original_empty_graph_selectors_may_create_EventGraph_before_type_failure=True,
        normal_current_read_or_creation_prefix_native_authoritative=True,
        authored_FAnimNode_settings_pose_pin_and_protected_link_semantics_native_owned=True,
        no_Python_limit_clamp_filter_reflection_or_graph_work=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        Native_reflection_loader_cache_structural_effects_not_total_zero=True,
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
    result["validation"].update(status="native_anim_graph_inspection_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        implicit_graph_creation_structural_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_anim_graph_inspection_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=arguments["public"].get("compile", False), attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=arguments["public"].get("save", False), attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_anim_graph_inspection_operation"
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
            result["stage"] = "returned_anim_graph_inspection_operation"
            callback(result)
        if getattr(error, "code", "") == "anim_graph_inspection_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_graph_inspection_native_failed", native.get("error") or "Native AnimGraph/default-graph/settings/topology/current observation prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("anim_graph_inspection_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_AnimGraph_settings_pose_links_default_creation_and_journal_no_replay"



def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def inspect_anim_graph_node_settings(blueprint_name, node_id='', node_type='', title_contains='', include_pins=True, max_depth=4, graph_name='AnimGraph', graph_id='', graph_type='function'):
    try:
        arguments = _arguments('inspect_anim_graph_node_settings', dict(blueprint_name=blueprint_name, node_id=node_id, node_type=node_type, title_contains=title_contains, include_pins=include_pins, max_depth=max_depth, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type))
    except Exception as error:
        return _rejected('inspect_anim_graph_node_settings', error)
    return _named(arguments)


def inspect_anim_graph_node_settings_action(result, blueprint_name, node_id='', node_type='', title_contains='', include_pins=True, max_depth=4, graph_name='AnimGraph', graph_id='', graph_type='function', checkpoint_callback=None):
    _run_action(result, _arguments('inspect_anim_graph_node_settings', dict(blueprint_name=blueprint_name, node_id=node_id, node_type=node_type, title_contains=title_contains, include_pins=include_pins, max_depth=max_depth, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type)), checkpoint_callback)


def inspect_anim_graph_protected_topology(blueprint_name, node_id='', node_type='', title_contains='', include_pins=True, include_pose_pins=True, include_links=True, include_non_pose_links=False, include_settings=False, max_depth=3, max_nodes=512, max_links=2048, graph_name='AnimGraph', graph_id='', graph_type='function'):
    try:
        arguments = _arguments('inspect_anim_graph_protected_topology', dict(blueprint_name=blueprint_name, node_id=node_id, node_type=node_type, title_contains=title_contains, include_pins=include_pins, include_pose_pins=include_pose_pins, include_links=include_links, include_non_pose_links=include_non_pose_links, include_settings=include_settings, max_depth=max_depth, max_nodes=max_nodes, max_links=max_links, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type))
    except Exception as error:
        return _rejected('inspect_anim_graph_protected_topology', error)
    return _named(arguments)


def inspect_anim_graph_protected_topology_action(result, blueprint_name, node_id='', node_type='', title_contains='', include_pins=True, include_pose_pins=True, include_links=True, include_non_pose_links=False, include_settings=False, max_depth=3, max_nodes=512, max_links=2048, graph_name='AnimGraph', graph_id='', graph_type='function', checkpoint_callback=None):
    _run_action(result, _arguments('inspect_anim_graph_protected_topology', dict(blueprint_name=blueprint_name, node_id=node_id, node_type=node_type, title_contains=title_contains, include_pins=include_pins, include_pose_pins=include_pose_pins, include_links=include_links, include_non_pose_links=include_non_pose_links, include_settings=include_settings, max_depth=max_depth, max_nodes=max_nodes, max_links=max_links, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type)), checkpoint_callback)


