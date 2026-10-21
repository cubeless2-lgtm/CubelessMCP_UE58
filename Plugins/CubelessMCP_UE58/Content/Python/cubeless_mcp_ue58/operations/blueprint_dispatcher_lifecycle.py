"""Original Unbind/Clear/Assign node authoring; all construction remains Native-owned.

Actual partial allocation, void compound Assign event/link and structural prefixes
are checkpointed before errors. These APIs author future execution nodes; the
adapter does not execute delegate changes, compile/save, cleanup or retry.
"""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_dispatcher_lifecycle as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'add_blueprint_event_dispatcher_unbind_node': ('blueprint_name', 'dispatcher_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_event_dispatcher_clear_node': ('blueprint_name', 'dispatcher_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_event_dispatcher_assign_node': ('blueprint_name', 'dispatcher_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}

def _json_value(value, label):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("blueprint_dispatcher_lifecycle_input_invalid", label + " exceeds bounded JSON depth/value scope")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("blueprint_dispatcher_lifecycle_input_invalid", "JSON object keys must be strings")
                visit(key, depth + 1)
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif type(item) is str:
            if any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise OperationError("blueprint_dispatcher_lifecycle_input_invalid", "Unpaired surrogate strings are outside the adapter")
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("blueprint_dispatcher_lifecycle_input_invalid", "JSON numbers must be finite in the native double representation")
        elif item is not None and type(item) is not bool:
            raise OperationError("blueprint_dispatcher_lifecycle_input_invalid", "Only JSON shapes are admitted for public " + label)
    visit(value, 0)
    text = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    if len(text) > 65536:
        raise OperationError("blueprint_dispatcher_lifecycle_input_invalid", label + " exceeds bounded private JSON carrier")
    return text


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("blueprint_dispatcher_lifecycle_input_invalid", "Only original public fields for this lifecycle method admitted")
    for field, value in public.items():
        if field == "node_position":
            continue  # Whole JSON Any; native owns bounded original projection and grid effects.
        expected = bool if field == "create_graph_if_missing" else str
        if type(value) is not expected:
            raise OperationError("blueprint_dispatcher_lifecycle_input_invalid", "Original dispatcher/graph names and create flag type differ")
    hidden = {field: copy.deepcopy(value) for field, value in public.items() if field != "node_position"}
    position = public["node_position"]
    hidden["position_json"] = _json_value([0, 0] if position is None else position, "position")
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden,
        "normalizations": ["node_position_None_to_origin_pair"] if position is None else []}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], raw_public_arguments=copy.deepcopy(arguments["public"]),
        requested_arguments=copy.deepcopy(arguments["hidden"]), original_default_normalizations=list(arguments["normalizations"]),
        domain_omission_vs_explicit_None_not_distinguished=True, public_position_schema="JSON Any/defaultNull",
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        actual_dispatcher_node_family="Engine UK2Node_RemoveDelegate/ClearDelegate/AssignDelegate with actual Assign compound CustomEvent",
        native_property_self_signature_lifecycle_secondary_event_edge_grid_and_source_proof_authoritative=True,
        original_wrappers_have_no_dependency_guard=True,
        native_graph_creation_allocation_compatibility_and_void_compound_prefix_preserved=True,
        SDK_Assign_nonnull_return_not_assumed_complete_event_or_link=True,
        compound_implicit_skeleton_or_transitive_callbacks_not_counted_as_explicit_compiler=True,
        atomic_restoration_or_automatic_node_declaration_cleanup_by_adapter=False,
        native_core_and_implicit_structural_work="Opaque property/graph/allocation/lifecycle/Assign secondary event/link/grid/structural/source/count prefix",
        actual_runtime_delegate_unbind_clear_or_assignment_invoked_by_adapter=False,
        full_original_dispatcher_inherited_cold_signature_or_linked_lifecycle_pipeline_verified=False)


def _apply_native(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_raw_result=native["_raw_native_json"],
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_not_attempted" if not native["compile_attempted"] else "unexpected_native_compile_attempt")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_blueprint_dispatcher_lifecycle_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", implicit_skeleton_or_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_blueprint_dispatcher_lifecycle_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_mutation=copy.deepcopy(arguments))
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_dispatcher_lifecycle_operation"
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
            result["stage"] = "returned_blueprint_dispatcher_lifecycle_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_dispatcher_lifecycle_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("blueprint_dispatcher_lifecycle_unexpected_compile_save", "Actual unrequested compiler/save prefix retained; no second dispatch")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_dispatcher_lifecycle_native_failed", native.get("error") or "Native property/signature/compound event/link/lifecycle/source prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_dispatcher_lifecycle_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_main_secondary_event_typed_pins_edges_grid_graph_source_and_journal_no_node_or_compound_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result





def add_blueprint_event_dispatcher_unbind_node(blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_event_dispatcher_unbind_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_event_dispatcher_unbind_node', error)
    return _named(arguments)


def add_blueprint_event_dispatcher_unbind_node_action(result, blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_event_dispatcher_unbind_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)


def add_blueprint_event_dispatcher_clear_node(blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_event_dispatcher_clear_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_event_dispatcher_clear_node', error)
    return _named(arguments)


def add_blueprint_event_dispatcher_clear_node_action(result, blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_event_dispatcher_clear_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)


def add_blueprint_event_dispatcher_assign_node(blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_event_dispatcher_assign_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_event_dispatcher_assign_node', error)
    return _named(arguments)


def add_blueprint_event_dispatcher_assign_node_action(result, blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_event_dispatcher_assign_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)
