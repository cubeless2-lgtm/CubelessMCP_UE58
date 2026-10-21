"""Original dispatcher declaration/call/bind authoring through fixed native methods.

Signature descriptor JSON is opaque. Native owns variables/signature graphs,
metadata, property/self context, typed pins and best-effort cleanup. Actual
returned prefixes are persisted before validation; no atomic restoration,
compiler/save dispatch, runtime delegate invocation or automatic replay follows.
"""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_dispatcher as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'add_blueprint_event_dispatcher': ('blueprint_name', 'dispatcher_name', 'inputs', 'category', 'tooltip', 'friendly_name'), 'add_blueprint_event_dispatcher_call_node': ('blueprint_name', 'dispatcher_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_event_dispatcher_bind_node': ('blueprint_name', 'dispatcher_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}

def _json_value(value, label):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("blueprint_dispatcher_input_invalid", label + " exceeds bounded JSON depth/value scope")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("blueprint_dispatcher_input_invalid", "JSON object keys must be strings")
                visit(key, depth + 1)
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif type(item) is str:
            if any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise OperationError("blueprint_dispatcher_input_invalid", "Unpaired surrogate strings are outside the adapter")
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("blueprint_dispatcher_input_invalid", "JSON numbers must be finite in the native double representation")
        elif item is not None and type(item) is not bool:
            raise OperationError("blueprint_dispatcher_input_invalid", "Only JSON shapes are admitted for public " + label)
    visit(value, 0)
    text = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    if len(text) > 65536:
        raise OperationError("blueprint_dispatcher_input_invalid", label + " exceeds bounded private JSON carrier")
    return text


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("blueprint_dispatcher_input_invalid", "Only original public fields for this dispatcher method admitted")
    for field, value in public.items():
        if field == "node_position":
            continue  # Whole JSON Any; native owns bounded position interpretation.
        if field == "inputs":
            if value is not None and (type(value) is not list or any(type(row) is not dict for row in value)):
                raise OperationError("blueprint_dispatcher_input_invalid", "Original inputs require a nullable array of descriptor objects")
        elif field == "create_graph_if_missing":
            if type(value) is not bool:
                raise OperationError("blueprint_dispatcher_input_invalid", "Original create flag requires an exact boolean")
        elif type(value) is not str:
            raise OperationError("blueprint_dispatcher_input_invalid", "Original Blueprint/dispatcher/metadata/graph fields require literal strings")
    hidden = {field: copy.deepcopy(value) for field, value in public.items() if field not in ("node_position", "inputs")}
    normalizations = []
    if method == "add_blueprint_event_dispatcher":
        inputs = public["inputs"]
        hidden["inputs_json"] = "" if inputs is None or inputs == [] else _json_value(inputs, "inputs")
        if inputs is None or inputs == []:
            normalizations.append("inputs_None_or_empty_array_to_original_omission_sentinel")
        normalizations.extend(field + "_empty_string_original_metadata_write_omission" for field in ("category", "tooltip", "friendly_name") if public[field] == "")
    else:
        position = public["node_position"]
        hidden["position_json"] = _json_value([0, 0] if position is None else position, "position")
        if position is None:
            normalizations.append("node_position_None_to_origin_pair")
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden, "normalizations": normalizations}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], raw_public_arguments=copy.deepcopy(arguments["public"]),
        requested_arguments=copy.deepcopy(arguments["hidden"]), original_default_normalizations=list(arguments["normalizations"]),
        domain_omission_vs_explicit_None_not_distinguished=True, public_position_schema="JSON Any/defaultNull",
        public_inputs_schema="nullable descriptor object array with unconstrained descriptor JSON values",
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        actual_dispatcher_node_family="Engine multicast-delegate declaration signature graph, UK2Node_CallDelegate or UK2Node_AddDelegate",
        native_variable_signature_descriptor_metadata_property_owner_self_context_typed_pins_and_cleanup_authoritative=True,
        original_wrappers_have_no_dependency_guard=True,
        declaration_metadata_empty_strings_do_not_request_empty_metadata_writes=True,
        descriptor_alias_type_object_default_conversion_not_duplicated_by_adapter=True,
        native_graph_creation_allocation_reference_and_best_effort_cleanup_prefix_preserved=True,
        atomic_restoration_or_automatic_node_declaration_cleanup_by_adapter=False,
        native_core_and_implicit_structural_work="Opaque variable/graph/signature/property/metadata/lifecycle/cleanup/source/counter prefix; skeleton regeneration may be implicit",
        actual_dispatcher_declaration_call_bind_or_delegate_runtime_verified=False,
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
    result["validation"].update(status="native_blueprint_dispatcher_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", implicit_skeleton_or_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_blueprint_dispatcher_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_mutation=copy.deepcopy(arguments))
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_dispatcher_operation"
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
            result["stage"] = "returned_blueprint_dispatcher_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_dispatcher_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("blueprint_dispatcher_unexpected_compile_save", "Actual unrequested compiler/save prefix retained; no second dispatch")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_dispatcher_native_failed", native.get("error") or "Native variable/signature/property/node/cleanup/source prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_dispatcher_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_member_signature_graph_nodes_typed_pins_cleanup_state_source_and_journal_no_declaration_or_node_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result




def add_blueprint_event_dispatcher(blueprint_name, dispatcher_name, inputs=None, category='', tooltip='', friendly_name=''):
    try:
        arguments = _arguments('add_blueprint_event_dispatcher', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'inputs': inputs, 'category': category, 'tooltip': tooltip, 'friendly_name': friendly_name})
    except Exception as error:
        return _rejected('add_blueprint_event_dispatcher', error)
    return _named(arguments)


def add_blueprint_event_dispatcher_action(result, blueprint_name, dispatcher_name, inputs=None, category='', tooltip='', friendly_name='', checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_event_dispatcher', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'inputs': inputs, 'category': category, 'tooltip': tooltip, 'friendly_name': friendly_name}), checkpoint_callback)


def add_blueprint_event_dispatcher_call_node(blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_event_dispatcher_call_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_event_dispatcher_call_node', error)
    return _named(arguments)


def add_blueprint_event_dispatcher_call_node_action(result, blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_event_dispatcher_call_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)


def add_blueprint_event_dispatcher_bind_node(blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_event_dispatcher_bind_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_event_dispatcher_bind_node', error)
    return _named(arguments)


def add_blueprint_event_dispatcher_bind_node_action(result, blueprint_name, dispatcher_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_event_dispatcher_bind_node', {'blueprint_name': blueprint_name, 'dispatcher_name': dispatcher_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)
