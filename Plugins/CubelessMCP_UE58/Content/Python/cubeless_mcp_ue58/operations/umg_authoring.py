"""Original UMG create/text/button composition with native composite receipts.

Native owns widget classes/tree creation, slot projection, generated variables,
structural/full compiler and force-save calls. Original ignored style inputs
remain documented; Python never applies them or repeats accepted work.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import umg_authoring as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'create_umg_widget_blueprint': ('widget_name', 'parent_class', 'path'), 'add_text_block_to_widget': ('widget_name', 'text_block_name', 'text', 'position', 'size', 'font_size', 'color'), 'add_button_to_widget': ('widget_name', 'button_name', 'text', 'position', 'size', 'font_size', 'color', 'background_color')}
_FIELD_KINDS = {'create_umg_widget_blueprint': {'widget_name': 'string', 'parent_class': 'string', 'path': 'string'}, 'add_text_block_to_widget': {'widget_name': 'string', 'text_block_name': 'string', 'text': 'string', 'position': 'number_array', 'size': 'number_array', 'font_size': 'integer', 'color': 'number_array'}, 'add_button_to_widget': {'widget_name': 'string', 'button_name': 'string', 'text': 'string', 'position': 'number_array', 'size': 'number_array', 'font_size': 'integer', 'color': 'number_array', 'background_color': 'number_array'}}


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("umg_authoring_input_invalid", "Only original method fields admitted")
    hidden = {}
    for field, kind in _FIELD_KINDS[method].items():
        value = public[field]
        if kind == "number_array":
            if type(value) is not list or any(type(item) not in (int, float) for item in value):
                raise OperationError("umg_authoring_input_invalid", "Original numeric array required: " + field)
            try:
                encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
                size = len(encoded.encode("utf-8"))
            except (ValueError, OverflowError, UnicodeError, RecursionError) as error:
                raise OperationError("umg_authoring_input_invalid", "Finite JSON numeric array carrier required: " + field) from error
            if size > 131072:
                raise OperationError("umg_authoring_input_invalid", "Private array carrier exceeds fixed 128KiB bound: " + field)
            hidden[field + "_json"] = encoded
        else:
            expected = int if kind == "integer" else str
            if type(value) is not expected:
                raise OperationError("umg_authoring_input_invalid", "Original public type differs: " + field)
            if kind == "integer" and not -(1 << 63) <= value < (1 << 63):
                raise OperationError("umg_authoring_input_invalid", "Safe int64 native font carrier required")
            hidden[field] = value
    # No length/range/geometry/style clamp or path/class normalization here.
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]), original_numeric_arrays_whole_JSON=True,
        original_font_color_background_forwarded_but_not_applied=True,
        original_creation_default_path="/Game/UI", original_bare_name_load_default="/Game/Widgets",
        native_tree_root_child_slot_variable_compiler_save_authoritative=True,
        original_button_has_separate_named_text_child=True, no_added_public_compile_or_save_options=True,
        no_Python_geometry_style_or_font_clamp=True, no_added_Python_dependency_guard=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        native_implicit_structural_compile_and_loader_effects_not_total_zero=True,
        full_original_UMG_render_runtime_cold_or_unique_pipeline_verified=False)


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
    result["validation"].update(status="native_umg_authoring_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        native_compile_save_and_structural_callbacks="actual_native_prefix")
    result["stage"] = "returned_umg_authoring_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_umg_authoring_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["widget_name"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments["method"])(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_umg_authoring_operation"
            callback(result)
        if getattr(error, "code", "") == "umg_authoring_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "umg_authoring_native_failed", native.get("error") or "Native UMG tree/slot/variable/compiler/save/current observation prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("umg_authoring_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_widget_tree_slot_generated_variable_compile_save_and_journal_no_replay"



def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result



def create_umg_widget_blueprint(widget_name, parent_class='UserWidget', path='/Game/UI'):
    try:
        arguments = _arguments('create_umg_widget_blueprint', dict(widget_name=widget_name, parent_class=parent_class, path=path))
    except Exception as error:
        return _rejected('create_umg_widget_blueprint', error)
    return _named(arguments)


def create_umg_widget_blueprint_action(result, widget_name, parent_class='UserWidget', path='/Game/UI', checkpoint_callback=None):
    _run_action(result, _arguments('create_umg_widget_blueprint', dict(widget_name=widget_name, parent_class=parent_class, path=path)), checkpoint_callback)


def add_text_block_to_widget(widget_name, text_block_name, text='', position=[0.0, 0.0], size=[200.0, 50.0], font_size=12, color=[1.0, 1.0, 1.0, 1.0]):
    try:
        arguments = _arguments('add_text_block_to_widget', dict(widget_name=widget_name, text_block_name=text_block_name, text=text, position=position, size=size, font_size=font_size, color=color))
    except Exception as error:
        return _rejected('add_text_block_to_widget', error)
    return _named(arguments)


def add_text_block_to_widget_action(result, widget_name, text_block_name, text='', position=[0.0, 0.0], size=[200.0, 50.0], font_size=12, color=[1.0, 1.0, 1.0, 1.0], checkpoint_callback=None):
    _run_action(result, _arguments('add_text_block_to_widget', dict(widget_name=widget_name, text_block_name=text_block_name, text=text, position=position, size=size, font_size=font_size, color=color)), checkpoint_callback)


def add_button_to_widget(widget_name, button_name, text='', position=[0.0, 0.0], size=[200.0, 50.0], font_size=12, color=[1.0, 1.0, 1.0, 1.0], background_color=[0.1, 0.1, 0.1, 1.0]):
    try:
        arguments = _arguments('add_button_to_widget', dict(widget_name=widget_name, button_name=button_name, text=text, position=position, size=size, font_size=font_size, color=color, background_color=background_color))
    except Exception as error:
        return _rejected('add_button_to_widget', error)
    return _named(arguments)


def add_button_to_widget_action(result, widget_name, button_name, text='', position=[0.0, 0.0], size=[200.0, 50.0], font_size=12, color=[1.0, 1.0, 1.0, 1.0], background_color=[0.1, 0.1, 0.1, 1.0], checkpoint_callback=None):
    _run_action(result, _arguments('add_button_to_widget', dict(widget_name=widget_name, button_name=button_name, text=text, position=position, size=size, font_size=font_size, color=color, background_color=background_color)), checkpoint_callback)

