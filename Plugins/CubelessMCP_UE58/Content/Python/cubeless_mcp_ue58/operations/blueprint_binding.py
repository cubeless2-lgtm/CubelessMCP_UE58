"""Original native delegate bind, Create Event and interface Message node authoring.

Native owns property/class/function resolution, ordered endpoint connections,
selected-name binding and only-new-node failure cleanup. Actual returned prefixes
precede validation; late failure is not atomic rollback or callable/runtime proof.
"""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_binding as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'add_blueprint_native_delegate_bind_node': ('blueprint_name', 'delegate_owner_class', 'delegate_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_create_delegate_node': ('blueprint_name', 'function_name', 'object_node_id', 'delegate_target_node_id', 'object_pin_name', 'delegate_target_pin_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_interface_message_node': ('blueprint_name', 'interface_class', 'function_name', 'target_node_id', 'target_pin_name', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
_DEPENDENCY_TOKENS = ("/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")


def _json_value(value, label):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("blueprint_binding_input_invalid", label + " exceeds bounded JSON depth/value scope")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("blueprint_binding_input_invalid", "JSON object keys must be strings")
                visit(key, depth + 1)
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif type(item) is str:
            if any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise OperationError("blueprint_binding_input_invalid", "Unpaired surrogate strings are outside the adapter")
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("blueprint_binding_input_invalid", "JSON numbers must be finite in the native double representation")
        elif item is not None and type(item) is not bool:
            raise OperationError("blueprint_binding_input_invalid", "Only JSON shapes are admitted for public " + label)
    visit(value, 0)
    text = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    if len(text) > 65536:
        raise OperationError("blueprint_binding_input_invalid", label + " exceeds bounded private JSON carrier")
    return text


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("blueprint_binding_input_invalid", "Only original public fields for this method admitted")
    for field, value in public.items():
        if field == "node_position":
            continue  # Whole JSON Any; native owns bounded position interpretation.
        if field == "create_graph_if_missing":
            if type(value) is not bool:
                raise OperationError("blueprint_binding_input_invalid", "Original graph-create input requires a boolean")
        elif type(value) is not str:
            raise OperationError("blueprint_binding_input_invalid", "Original Blueprint/class/function/node/pin/graph fields require literal strings")
    class_field = {"add_blueprint_native_delegate_bind_node": "delegate_owner_class",
        "add_blueprint_interface_message_node": "interface_class"}.get(method)
    if class_field is not None:
        # Exact original scalar class-reference guard; CreateDelegate has no guard.
        candidate = public[class_field].strip().strip("\"'").replace("\\", "/").lower()
        if any(token in candidate for token in _DEPENDENCY_TOKENS):
            raise OperationError("blueprint_binding_dependency_reference", "Original " + class_field + " guard refuses an MCP-only runtime reference")
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
        actual_binding_node_families="Engine UK2Node_AddDelegate external owner, UK2Node_CreateDelegate or UK2Node_Message",
        native_property_owner_function_endpoint_connection_lifecycle_and_cleanup_authoritative=True,
        original_class_guard_fields=["delegate_owner_class", "interface_class"],
        create_delegate_selected_function_not_prevalidated_by_adapter=True,
        selected_function_name_success_is_not_runtime_callable_or_delegate_invocation_proof=True,
        only_new_node_cleanup_is_not_atomic_endpoint_or_conversion_restoration=True,
        native_core_and_implicit_structural_work="Opaque actual property/function/class/link/lifecycle/retirement/current graph/source/direct counter prefix; structural skeleton work may be implicit",
        full_original_inherited_generated_interface_runtime_cold_or_UI_vehicle_pipeline_verified=False)



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
    result["validation"].update(status="native_blueprint_binding_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", implicit_skeleton_or_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_blueprint_binding_operation"



def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_mutation=copy.deepcopy(arguments))
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_binding_operation"
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
            result["stage"] = "returned_blueprint_binding_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_binding_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("blueprint_binding_unexpected_compile_save", "Actual unrequested compiler/save prefix retained; no second dispatch")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_binding_native_failed", native.get("error") or "Native class/node/default/restoration/cleanup prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_binding_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_class_default_restoration_created_retired_graph_and_journal_no_node_replay"



def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))



def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def add_blueprint_native_delegate_bind_node(blueprint_name, delegate_owner_class, delegate_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_native_delegate_bind_node', {'blueprint_name': blueprint_name, 'delegate_owner_class': delegate_owner_class, 'delegate_name': delegate_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_native_delegate_bind_node', error)
    return _named(arguments)


def add_blueprint_native_delegate_bind_node_action(result, blueprint_name, delegate_owner_class, delegate_name, node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_native_delegate_bind_node', {'blueprint_name': blueprint_name, 'delegate_owner_class': delegate_owner_class, 'delegate_name': delegate_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)


def add_blueprint_create_delegate_node(blueprint_name, function_name, object_node_id, delegate_target_node_id, object_pin_name='ReturnValue', delegate_target_pin_name='Delegate', node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_create_delegate_node', {'blueprint_name': blueprint_name, 'function_name': function_name, 'object_node_id': object_node_id, 'delegate_target_node_id': delegate_target_node_id, 'object_pin_name': object_pin_name, 'delegate_target_pin_name': delegate_target_pin_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_create_delegate_node', error)
    return _named(arguments)


def add_blueprint_create_delegate_node_action(result, blueprint_name, function_name, object_node_id, delegate_target_node_id, object_pin_name='ReturnValue', delegate_target_pin_name='Delegate', node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_create_delegate_node', {'blueprint_name': blueprint_name, 'function_name': function_name, 'object_node_id': object_node_id, 'delegate_target_node_id': delegate_target_node_id, 'object_pin_name': object_pin_name, 'delegate_target_pin_name': delegate_target_pin_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)


def add_blueprint_interface_message_node(blueprint_name, interface_class, function_name, target_node_id, target_pin_name='ReturnValue', node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_interface_message_node', {'blueprint_name': blueprint_name, 'interface_class': interface_class, 'function_name': function_name, 'target_node_id': target_node_id, 'target_pin_name': target_pin_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_interface_message_node', error)
    return _named(arguments)


def add_blueprint_interface_message_node_action(result, blueprint_name, interface_class, function_name, target_node_id, target_pin_name='ReturnValue', node_position=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_interface_message_node', {'blueprint_name': blueprint_name, 'interface_class': interface_class, 'function_name': function_name, 'target_node_id': target_node_id, 'target_pin_name': target_pin_name, 'node_position': node_position, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)
