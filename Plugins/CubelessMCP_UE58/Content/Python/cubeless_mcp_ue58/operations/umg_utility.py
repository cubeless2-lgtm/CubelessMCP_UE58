"""Named checkbox command adapter and original viewport class readiness.

Checkbox has no original Python public wrapper. Its whole native-command object
preserves aliases, omissions and arbitrary JSON fields for Native interpretation.
Viewport preserves the original two fields but performs no runtime instantiation.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import umg_utility as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'add_checkbox_to_widget': ('params',), 'add_widget_to_viewport': ('widget_name', 'z_order')}
_FIELD_KINDS = {'add_checkbox_to_widget': {'params': 'native_params_object'}, 'add_widget_to_viewport': {'widget_name': 'string', 'z_order': 'integer'}}

def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("umg_utility_input_invalid", "Only this named method's fields admitted")
    hidden = {}
    for field, kind in _FIELD_KINDS[method].items():
        value = public[field]
        if kind == "native_params_object":
            if type(value) is not dict:
                raise OperationError("umg_utility_input_invalid", "Named checkbox native-command params object required")
            try:
                encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
                size = len(encoded.encode("utf-8"))
            except (TypeError, ValueError, OverflowError, UnicodeError, RecursionError) as error:
                raise OperationError("umg_utility_input_invalid", "Finite whole JSON params object required") from error
            if size > 131072:
                raise OperationError("umg_utility_input_invalid", "Private params carrier exceeds fixed 128KiB bound")
            hidden["params_json"] = encoded
        elif kind == "integer":
            if type(value) is not int or not -(2 ** 63) <= value < 2 ** 63:
                raise OperationError("umg_utility_input_invalid", "Exact safe int64 carrier required; Native owns checked int32 admission")
            hidden[field] = value
        else:
            if type(value) is not str:
                raise OperationError("umg_utility_input_invalid", "Original public string type differs: " + field)
            hidden[field] = value
    # No params row/key coercion, aliases, trim, geometry, checked default or source lookup here.
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    checkbox = arguments["method"] == "add_checkbox_to_widget"
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]),
        checkbox_native_command_object_adapter=checkbox, checkbox_original_Python_wrapper_exists=False,
        original_checkbox_native_command_contract_preserved=True,
        original_viewport_public_arity=2, viewport_scope="class_readiness_only_no_runtime_widget_or_viewport_effect",
        no_Python_checkbox_key_alias_default_trim_or_geometry_projection=True,
        no_added_public_compile_or_save_options=True, no_added_Python_dependency_guard=True,
        direct_named_mutation_journal_performed=checkbox, native_outer_transaction_and_loader_effects_not_total_zero=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        full_original_UMG_utility_runtime_cold_or_unique_pipeline_verified=False)


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
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"].get("widget_name"),
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_umg_utility_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        native_compile_save_and_structural_callbacks="actual_native_prefix")
    result["stage"] = "returned_umg_utility_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or (state.checkpoint if arguments["method"] == "add_checkbox_to_widget" else lambda observed: None)
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint" if arguments["method"] == "add_checkbox_to_widget" else "before_read_checkpoint"
    callback(result)
    result["stage"] = "native_umg_utility_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"].get("widget_name"),
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments["method"])(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_umg_utility_operation"
            callback(result)
        if getattr(error, "code", "") == "umg_utility_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "umg_utility_native_failed", native.get("error") or "Native checkbox/root/label/compiler/save or viewport class-readiness prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("umg_utility_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_checkbox_or_class_readiness_prefix_no_replay"




def _named(arguments):
    if arguments["method"] == "add_checkbox_to_widget":
        return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))
    result = state.new_result()
    try:
        _run_action(result, arguments)
    except Exception as error:
        state.fail(result, error)
    return result


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def add_checkbox_to_widget(params):
    try:
        arguments = _arguments("add_checkbox_to_widget", dict(params=params))
    except Exception as error:
        return _rejected("add_checkbox_to_widget", error)
    return _named(arguments)


def add_checkbox_to_widget_action(result, params, checkpoint_callback=None):
    _run_action(result, _arguments("add_checkbox_to_widget", dict(params=params)), checkpoint_callback)


def add_widget_to_viewport(widget_name, z_order=0):
    try:
        arguments = _arguments("add_widget_to_viewport", dict(widget_name=widget_name, z_order=z_order))
    except Exception as error:
        return _rejected("add_widget_to_viewport", error)
    return _named(arguments)


def add_widget_to_viewport_action(result, widget_name, z_order=0, checkpoint_callback=None):
    _run_action(result, _arguments("add_widget_to_viewport", dict(widget_name=widget_name, z_order=z_order)), checkpoint_callback)
