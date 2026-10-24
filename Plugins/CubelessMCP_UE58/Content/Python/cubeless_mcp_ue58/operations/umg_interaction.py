"""Original UMG slider descriptors and event binding with native composite receipts.

Native owns every row/default/geometry/widget/delegate and compiler/save phase.
Python preserves the original falsey function-name fallback before forwarding it;
the original Native handler ignores that name and uses its actual bound event.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import umg_interaction as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'add_sliders_to_widget': ('widget_name', 'sliders'), 'bind_widget_event': ('widget_name', 'widget_component_name', 'event_name', 'function_name')}
_FIELD_KINDS = {'add_sliders_to_widget': {'widget_name': 'string', 'sliders': 'descriptor_array'}, 'bind_widget_event': {'widget_name': 'string', 'widget_component_name': 'string', 'event_name': 'string', 'function_name': 'string'}}

def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("umg_interaction_input_invalid", "Only original method fields admitted")
    hidden = {}
    for field, kind in _FIELD_KINDS[method].items():
        value = public[field]
        if kind == "descriptor_array":
            if type(value) is not list:
                raise OperationError("umg_interaction_input_invalid", "Original descriptor array required: " + field)
            try:
                encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
                size = len(encoded.encode("utf-8"))
            except (TypeError, ValueError, OverflowError, UnicodeError, RecursionError) as error:
                raise OperationError("umg_interaction_input_invalid", "Finite whole JSON descriptor carrier required: " + field) from error
            if size > 131072:
                raise OperationError("umg_interaction_input_invalid", "Private descriptor carrier exceeds fixed 128KiB bound")
            hidden[field + "_json"] = encoded
        else:
            if type(value) is not str:
                raise OperationError("umg_interaction_input_invalid", "Original public string type differs: " + field)
            hidden[field] = value
    if method == "bind_widget_event" and not public["function_name"]:
        hidden["function_name"] = f"{public['widget_component_name']}_{public['event_name']}"
    # No row inspection, defaults, trim, geometry projection or native event lookup here.
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]), original_slider_descriptors_whole_JSON=True,
        no_Python_slider_row_validation_trim_defaults_or_geometry=True,
        original_falsey_function_name_fallback_before_dispatch=True,
        original_native_function_name_ignored=True, native_slider_bound_event_compiler_save_authoritative=True,
        original_bare_name_load_default="/Game/Widgets", no_added_public_compile_or_save_options=True,
        no_added_Python_dependency_guard=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        native_implicit_structural_compile_and_loader_effects_not_total_zero=True,
        full_original_UMG_interaction_runtime_cold_or_unique_pipeline_verified=False)
    if arguments["method"] == "bind_widget_event":
        result["data"].update(adapter_original_function_name=arguments["public"]["function_name"],
            adapter_effective_function_name=arguments["hidden"]["function_name"],
            adapter_function_name_default_derived=not arguments["public"]["function_name"])


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
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"]["widget_name"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_umg_interaction_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        native_compile_save_and_structural_callbacks="actual_native_prefix")
    result["stage"] = "returned_umg_interaction_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_umg_interaction_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["widget_name"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments.get("native_method", arguments["method"]))(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_umg_interaction_operation"
            callback(result)
        if getattr(error, "code", "") == "umg_interaction_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "umg_interaction_native_failed", native.get("error") or "Native UMG slider/delegate/event/compiler/save/current observation prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("umg_interaction_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_slider_or_bound_event_native_phase_receipts_and_journal_no_replay"



def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result



def add_sliders_to_widget(widget_name, sliders):
    try:
        arguments = _arguments("add_sliders_to_widget", dict(widget_name=widget_name, sliders=sliders))
    except Exception as error:
        return _rejected("add_sliders_to_widget", error)
    return _named(arguments)


def add_sliders_to_widget_action(result, widget_name, sliders, checkpoint_callback=None):
    _run_action(result, _arguments("add_sliders_to_widget", dict(widget_name=widget_name, sliders=sliders)), checkpoint_callback)


def bind_widget_event(widget_name, widget_component_name, event_name, function_name=''):
    try:
        arguments = _arguments("bind_widget_event", dict(widget_name=widget_name, widget_component_name=widget_component_name, event_name=event_name, function_name=function_name))
    except Exception as error:
        return _rejected("bind_widget_event", error)
    return _named(arguments)


def bind_widget_event_action(result, widget_name, widget_component_name, event_name, function_name='', checkpoint_callback=None):
    _run_action(result, _arguments("bind_widget_event", dict(widget_name=widget_name, widget_component_name=widget_component_name, event_name=event_name, function_name=function_name)), checkpoint_callback)


def bind_widget_event_raw_action(result, widget_name, widget_component_name, event_name,
        raw_node_position, function_name='', checkpoint_callback=None):
    """Private original-command carrier; Named4 and its schema are unchanged."""
    arguments = _arguments("bind_widget_event", dict(widget_name=widget_name,
        widget_component_name=widget_component_name, event_name=event_name, function_name=function_name))
    if type(raw_node_position) is not dict or set(raw_node_position) - {"node_pos_x", "node_pos_y"}:
        raise OperationError("umg_binding_raw_position_invalid", "Only original independent node_pos_x/y fields admitted")
    try:
        encoded = json.dumps(raw_node_position, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        if len(encoded.encode("utf-8")) > 131072:
            raise ValueError("raw position exceeds fixed 128KiB bound")
    except (TypeError, ValueError, OverflowError, UnicodeError, RecursionError) as error:
        raise OperationError("umg_binding_raw_position_invalid", "Finite bounded original position JSON required") from error
    # Do not convert/filter bools or numeric strings: UE TryGetNumberField owns
    # that original coercion after initial compile and actual event selection.
    arguments["native_method"] = "bind_widget_event_raw"
    arguments["hidden"]["node_position_json"] = encoded
    _run_action(result, arguments, checkpoint_callback)
