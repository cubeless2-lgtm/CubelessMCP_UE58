"""Original comment-box, node-comment and function-description authoring.

Native owns geometry/style normalization, lifecycle reapplication, tolerant live
grouping, text/metadata mutation and fresh current graph/source/pin proof.
Structural mark may regenerate the skeleton synchronously despite zero explicit
compiler requests. No saved/cold live-group or generated-tooltip equivalence is
inferred, and the adapter does not add writes, cleanup, compile, save or replay.
"""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_annotation as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'add_blueprint_comment_box': ('blueprint_name', 'comment_text', 'node_position', 'size', 'comment_color', 'font_size', 'move_mode', 'comment_bubble_visible', 'color_comment_bubble', 'node_details', 'node_ids', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'set_blueprint_node_comment': ('blueprint_name', 'node_id', 'node_comment', 'comment_bubble_visible', 'comment_bubble_pinned', 'graph_name', 'graph_id', 'graph_type'), 'set_blueprint_function_description': ('blueprint_name', 'description', 'graph_name', 'graph_id', 'graph_type')}
_DEPENDENCY_TOKENS = ("/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")


def _json_value(value, label):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("blueprint_annotation_input_invalid", label + " exceeds bounded JSON depth/value scope")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("blueprint_annotation_input_invalid", "JSON object keys must be strings")
                visit(key, depth + 1)
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif type(item) is str:
            if any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise OperationError("blueprint_annotation_input_invalid", "Unpaired surrogate strings are outside the adapter")
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("blueprint_annotation_input_invalid", "JSON numbers must be finite in the native double representation")
        elif item is not None and type(item) is not bool:
            raise OperationError("blueprint_annotation_input_invalid", "Only JSON shapes are admitted for public " + label)
    visit(value, 0)
    text = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    if len(text) > 65536:
        raise OperationError("blueprint_annotation_input_invalid", label + " exceeds bounded private JSON carrier")
    return text


def _guard_reference(value, label):
    # Exact original recursive guard: literal scalar strings/list child paths.
    if type(value) is str:
        candidate = value.strip().strip("\"'").replace("\\", "/").lower()
        if any(token in candidate for token in _DEPENDENCY_TOKENS):
            raise OperationError("blueprint_annotation_dependency_reference", "Original " + label + " guard refuses an MCP-only authored reference")
    elif type(value) is list:
        for index, child in enumerate(value):
            _guard_reference(child, label + "[" + str(index) + "]")


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("blueprint_annotation_input_invalid", "Only original public fields for this method admitted")
    for field, value in public.items():
        if field in ("node_position", "size", "comment_color"):
            if value is not None and (type(value) is not list or any(type(v) not in (int, float) for v in value)):
                raise OperationError("blueprint_annotation_input_invalid", "Original nullable numeric array required: " + field)
            continue  # No length/clamp/style projection; native preserves explicit empty arrays.
        if field == "node_ids":
            if value is not None and (type(value) is not list or any(type(v) is not str for v in value)):
                raise OperationError("blueprint_annotation_input_invalid", "Original nullable string array required: node_ids")
        elif field in ("comment_bubble_visible", "color_comment_bubble", "comment_bubble_pinned", "create_graph_if_missing"):
            if type(value) is not bool:
                raise OperationError("blueprint_annotation_input_invalid", "Original flags require exact booleans")
        elif field == "font_size":
            if type(value) is not int or not -2147483648 <= value <= 2147483647:
                raise OperationError("blueprint_annotation_input_invalid", "Original integer font_size exceeds safe reflected int32 carrier; native owns clamp1..1000")
        elif type(value) is not str:
            raise OperationError("blueprint_annotation_input_invalid", "Original names/text/graph selectors require literal strings")
    if method == "add_blueprint_comment_box":
        _guard_reference(public["comment_text"], "comment_text")
        _guard_reference(public["node_details"], "node_details")
        _guard_reference(public["node_ids"] or [], "node_ids")
    elif method == "set_blueprint_node_comment":
        _guard_reference(public["node_comment"], "node_comment")
    else:
        _guard_reference(public["description"], "description")
    hidden = {}; normalizations = []
    for field, value in public.items():
        if field in ("node_position", "size", "comment_color", "node_ids"):
            name = {"node_position": "position_json", "size": "size_json", "comment_color": "color_json", "node_ids": "node_ids_json"}[field]
            omitted = value is None or (field == "node_ids" and value == [])
            hidden[name] = "" if omitted else _json_value(value, field)
            if omitted:
                normalizations.append(field + "_original_omission_to_empty_private_carrier")
        else:
            hidden[field] = copy.deepcopy(value)
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden, "normalizations": normalizations}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], raw_public_arguments=copy.deepcopy(arguments["public"]),
        requested_arguments=copy.deepcopy(arguments["hidden"]), original_default_normalizations=list(arguments["normalizations"]),
        domain_omission_vs_explicit_None_not_distinguished=True,
        public_nullable_array_schema="Original numeric arrays for position/size/color; string array for node_ids",
        original_geometry_None_omission_distinct_from_explicit_empty_array=True,
        original_node_ids_None_or_empty_list_both_omitted=True,
        original_node_details_empty_string_omission_semantically_retained_by_literal_empty_hidden_field=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        actual_annotation_family="Engine UEdGraphNode_Comment or existing node/UK2Node_FunctionEntry",
        native_geometry_style_grouping_text_metadata_and_current_proof_authoritative=True,
        tolerant_blank_unresolved_self_group_skips_and_duplicate_calls_not_rewritten=True,
        structural_mark_synchronous_skeleton_regeneration_not_an_explicit_compiler_call=True,
        transient_live_grouping_saved_or_cold_persistence_verified=False,
        generated_UFunction_tooltip_UI_equivalence_not_inferred=True,
        original_added_node_cleanup_or_atomic_rollback_available=False,
        native_core_and_implicit_structural_work="Opaque actual graph/allocation/reapplication/grouping/modify/notify/structural skeleton/source/pin/counter prefix",
        actual_annotation_behavior_or_saved_cold_or_pipeline_verified=False,
        full_original_owner_graph_numeric_text_grouping_runtime_or_pipeline_verified=False)


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
    result["validation"].update(status="native_blueprint_annotation_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", implicit_skeleton_or_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_blueprint_annotation_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_mutation=copy.deepcopy(arguments))
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_annotation_operation"
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
            result["stage"] = "returned_blueprint_annotation_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_annotation_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("blueprint_annotation_unexpected_compile_save", "Actual unrequested compiler/save prefix retained; no second dispatch")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_annotation_native_failed", native.get("error") or "Native annotation graph/node/text/style/group/source prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_annotation_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_annotation_node_text_style_group_function_metadata_graph_source_and_journal_no_writer_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result




def add_blueprint_comment_box(blueprint_name, comment_text, node_position=None, size=None, comment_color=None, font_size=18, move_mode='group', comment_bubble_visible=False, color_comment_bubble=False, node_details='', node_ids=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_comment_box', {'blueprint_name': blueprint_name, 'comment_text': comment_text, 'node_position': node_position, 'size': size, 'comment_color': comment_color, 'font_size': font_size, 'move_mode': move_mode, 'comment_bubble_visible': comment_bubble_visible, 'color_comment_bubble': color_comment_bubble, 'node_details': node_details, 'node_ids': node_ids, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_comment_box', error)
    return _named(arguments)


def add_blueprint_comment_box_action(result, blueprint_name, comment_text, node_position=None, size=None, comment_color=None, font_size=18, move_mode='group', comment_bubble_visible=False, color_comment_bubble=False, node_details='', node_ids=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_comment_box', {'blueprint_name': blueprint_name, 'comment_text': comment_text, 'node_position': node_position, 'size': size, 'comment_color': comment_color, 'font_size': font_size, 'move_mode': move_mode, 'comment_bubble_visible': comment_bubble_visible, 'color_comment_bubble': color_comment_bubble, 'node_details': node_details, 'node_ids': node_ids, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)


def set_blueprint_node_comment(blueprint_name, node_id, node_comment, comment_bubble_visible=True, comment_bubble_pinned=False, graph_name='', graph_id='', graph_type=''):
    try:
        arguments = _arguments('set_blueprint_node_comment', {'blueprint_name': blueprint_name, 'node_id': node_id, 'node_comment': node_comment, 'comment_bubble_visible': comment_bubble_visible, 'comment_bubble_pinned': comment_bubble_pinned, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type})
    except Exception as error:
        return _rejected('set_blueprint_node_comment', error)
    return _named(arguments)


def set_blueprint_node_comment_action(result, blueprint_name, node_id, node_comment, comment_bubble_visible=True, comment_bubble_pinned=False, graph_name='', graph_id='', graph_type='', checkpoint_callback=None):
    _run_action(result, _arguments('set_blueprint_node_comment', {'blueprint_name': blueprint_name, 'node_id': node_id, 'node_comment': node_comment, 'comment_bubble_visible': comment_bubble_visible, 'comment_bubble_pinned': comment_bubble_pinned, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type}), checkpoint_callback)


def set_blueprint_function_description(blueprint_name, description, graph_name='', graph_id='', graph_type='function'):
    try:
        arguments = _arguments('set_blueprint_function_description', {'blueprint_name': blueprint_name, 'description': description, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type})
    except Exception as error:
        return _rejected('set_blueprint_function_description', error)
    return _named(arguments)


def set_blueprint_function_description_action(result, blueprint_name, description, graph_name='', graph_id='', graph_type='function', checkpoint_callback=None):
    _run_action(result, _arguments('set_blueprint_function_description', {'blueprint_name': blueprint_name, 'description': description, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type}), checkpoint_callback)
