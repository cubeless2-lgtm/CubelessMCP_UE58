"""Original public4 Text binding with explicit repair and Native phase receipts.

The original wrapper sends mismatched keys and the old native path never creates
Bindings. Native implements public property-path KindProperty Text binding while
retaining original sources, binding_name response and actual compile/save prefix.
It does not fabricate the original broken Get<property> graph or runtime display.
"""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import umg_text_binding as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'set_text_block_binding': ('widget_name', 'text_block_name', 'binding_property', 'binding_type')}
_FIELD_KINDS = {'set_text_block_binding': {'widget_name': 'string', 'text_block_name': 'string', 'binding_property': 'string', 'binding_type': 'string'}}

def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("umg_text_binding_input_invalid", "Only original Text binding public4 fields admitted")
    if any(type(value) is not str for value in public.values()):
        raise OperationError("umg_text_binding_input_invalid", "Original public string types required")
    # No trim, field remap, source/member/Binding lookup or type interpretation in Python.
    return {"method": method, "public": copy.deepcopy(public), "hidden": copy.deepcopy(public)}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]), original_public_contract_preserved=True,
        original_wrapper_native_key_mismatch_repaired=True, original_native_never_added_WidgetBlueprint_Bindings=True,
        selected_native_backend="public_FEditorPropertyPath_KindProperty_Text_binding",
        original_broken_Get_property_graph_artifact_not_created=True,
        binding_type_literal_forwarded_native_Text_scope_authoritative=True,
        no_Python_source_property_binding_row_graph_or_compiler_save_calls=True,
        no_added_public_compile_or_save_options=True, no_added_Python_dependency_guard=True,
        direct_named_mutation_journal_performed=True, native_loader_and_implicit_structural_effects_not_total_zero=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        full_Text_binding_runtime_cold_or_unique_pipeline_verified=False)


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
    result["validation"].update(status="native_umg_text_binding_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        native_compile_save_and_structural_callbacks="actual_native_prefix")
    result["stage"] = "returned_umg_text_binding_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_umg_text_binding_operation"
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
            result["stage"] = "returned_umg_text_binding_operation"
            callback(result)
        if getattr(error, "code", "") == "umg_text_binding_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "umg_text_binding_native_failed", native.get("error") or "Native Text source/property-path/editor/generated binding/compiler/save prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("umg_text_binding_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_Text_binding_native_prefix_and_journal_no_replay"





def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def set_text_block_binding(widget_name, text_block_name, binding_property, binding_type='Text'):
    try:
        arguments = _arguments("set_text_block_binding", dict(widget_name=widget_name, text_block_name=text_block_name, binding_property=binding_property, binding_type=binding_type))
    except Exception as error:
        return _rejected("set_text_block_binding", error)
    return _named(arguments)


def set_text_block_binding_action(result, widget_name, text_block_name, binding_property, binding_type='Text', checkpoint_callback=None):
    _run_action(result, _arguments("set_text_block_binding", dict(widget_name=widget_name, text_block_name=text_block_name, binding_property=binding_property, binding_type=binding_type)), checkpoint_callback)
