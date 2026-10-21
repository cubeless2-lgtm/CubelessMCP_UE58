"""Original CustomEvent12 and the distinct existing Graph13 signature route.

Native owns Any geometry, actual signature/parameter type copying, optional flag
presence, grid effects, existing typed-event reuse and all partial prefixes. This
core is shared behind fixed wrappers; it is independent of the MCP transport.
The old Graph13 parameterless facade remains on its immutable native API.
"""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import custom_event_signatures as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'add_blueprint_custom_event_node': ('blueprint_name', 'custom_event_name', 'node_position', 'signature_source_dispatcher_name', 'signature_source_class', 'signature_source_delegate_name', 'native_delegate_name', 'call_in_editor', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
_DEPENDENCY_TOKENS = ("/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")

def _json_value(value, label):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("custom_event_signatures_input_invalid", label + " exceeds bounded JSON depth/value scope")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("custom_event_signatures_input_invalid", "JSON object keys must be strings")
                visit(key, depth + 1)
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif type(item) is str:
            if any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise OperationError("custom_event_signatures_input_invalid", "Unpaired surrogate strings are outside the adapter")
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("custom_event_signatures_input_invalid", "JSON numbers must be finite in the native double representation")
        elif item is not None and type(item) is not bool:
            raise OperationError("custom_event_signatures_input_invalid", "Only JSON shapes are admitted for public " + label)
    visit(value, 0)
    text = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    if len(text) > 65536:
        raise OperationError("custom_event_signatures_input_invalid", label + " exceeds bounded private JSON carrier")
    return text


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("custom_event_signatures_input_invalid", "Only original CustomEvent12 public fields admitted")
    for field, value in public.items():
        if field == "node_position":
            continue  # Any JSON, only original None-to-origin is normalized by Python.
        if field == "call_in_editor":
            if value is not None and type(value) is not bool:
                raise OperationError("custom_event_signatures_input_invalid", "Original Call In Editor requires boolean/null")
        elif type(value) is not (bool if field == "create_graph_if_missing" else str):
            raise OperationError("custom_event_signatures_input_invalid", "Original name/class/delegate/graph field or create flag type differs")
    # Exact original pre-RPC guard remains even when dispatcher wins Native lookup.
    candidate = public["signature_source_class"].strip().strip("\"'").replace("\\", "/").lower()
    if public["signature_source_class"] and any(token in candidate for token in _DEPENDENCY_TOKENS):
        raise OperationError("custom_event_signatures_dependency_reference", "Original signature_source_class guard refuses an MCP-only reference")
    hidden = {field: copy.deepcopy(value) for field, value in public.items() if field not in ("node_position", "call_in_editor")}
    normalizations = []
    position = public["node_position"]
    hidden["position_json"] = _json_value([0, 0] if position is None else position, "position")
    if position is None:
        normalizations.append("node_position_None_to_origin_pair")
    if public["native_delegate_name"] and not public["signature_source_delegate_name"]:
        hidden["signature_source_delegate_name"] = public["native_delegate_name"]
        normalizations.append("native_delegate_name_to_falsey_source_delegate_before_trim")
    # Original Python did not send the alias after applying this public precedence.
    hidden["native_delegate_name"] = ""
    hidden["call_in_editor"] = False if public["call_in_editor"] is None else public["call_in_editor"]
    hidden["has_call_in_editor"] = public["call_in_editor"] is not None
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden, "normalizations": normalizations,
        "interface": "original_raw12"}


def _adapted_arguments(blueprint_name, custom_event_name, node_position_json,
        signature_source_dispatcher_name, signature_source_class, signature_source_delegate_name,
        native_delegate_name, call_in_editor, graph_name, graph_id, graph_type, create_graph_if_missing):
    from . import functions
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if type(position) is not list or len(position) != 2 or any(type(v) is not int or abs(v) > 1000000 for v in position):
        raise OperationError("event_position_invalid", "Existing Graph13 node_position_json requires two integers from -1000000 to 1000000")
    selectors = functions._selectors(blueprint_name, graph_name, graph_id, graph_type)
    public = {"blueprint_name": selectors["blueprint_path"], "custom_event_name": functions._text(custom_event_name, "custom_event_name"),
        "node_position": position, "call_in_editor": call_in_editor,
        "graph_name": selectors["graph_name"], "graph_id": selectors["graph_id"], "graph_type": selectors["graph_type"],
        "create_graph_if_missing": functions._bool(create_graph_if_missing, "create_graph_if_missing")}
    for field, value in (("signature_source_dispatcher_name", signature_source_dispatcher_name),
            ("signature_source_class", signature_source_class), ("signature_source_delegate_name", signature_source_delegate_name),
            ("native_delegate_name", native_delegate_name)):
        public[field] = functions._text("" if value is None else value, field, empty=True)
    args = _arguments("add_blueprint_custom_event_node", public)
    args.update(interface="existing_adapted_graph13", custom_event_signature_route=True,
        adapted_node_position_json=node_position_json, adapted_operation_id_remains_in_parent_journal=True)
    return args


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], raw_public_arguments=copy.deepcopy(arguments["public"]),
        requested_arguments=copy.deepcopy(arguments["hidden"]), original_default_normalizations=list(arguments["normalizations"]),
        public_interface=arguments["interface"], original_public_input_count=12, existing_adapted_Graph_input_count=13,
        interfaces_not_claimed_IO_equivalent=True, domain_omission_vs_explicit_None_not_distinguished=True,
        public_position_schema="original raw JSON Any/defaultNull; existing Graph13 keeps strict node_position_json",
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        native_any_projection_signature_source_alias_lookup_typed_factory_grid_lifecycle_and_reuse_authoritative=True,
        source_class_original_guard_applies_even_when_dispatcher_selector_wins=True,
        original_alias_precedence_applied_before_trim_private_alias_empty=True,
        original_call_in_editor_None_False_True_presence_retained=True,
        CreateFromFunction_nonnull_or_void_result_not_assumed_typed_signature_complete=True,
        existing_Graph_parameterless_API_route_unchanged=True,
        implicit_structural_skeleton_regeneration_not_explicit_compiler_dispatch=True,
        actual_runtime_delegate_evaluation_or_callback_invoked_by_adapter=False,
        full_original_typed_event_metadata_runtime_cold_or_linked_pipeline_verified=False)


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
    result["validation"].update(status="native_custom_event_signatures_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", implicit_skeleton_or_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_custom_event_signatures_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_mutation=copy.deepcopy(arguments))
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_custom_event_signatures_operation"
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
            result["stage"] = "returned_custom_event_signatures_operation"
            callback(result)
        if getattr(error, "code", "") == "custom_event_signatures_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("custom_event_signatures_unexpected_compile_save", "Actual unrequested compiler/save prefix retained; no second dispatch")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "custom_event_signatures_native_failed", native.get("error") or "Native signature/source/created event/default/typed pin/grid/reuse prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("custom_event_signatures_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_event_signature_typed_userpins_flags_grid_graph_source_and_journal_no_event_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result





def add_blueprint_custom_event_node(blueprint_name, custom_event_name, node_position=None, signature_source_dispatcher_name='', signature_source_class='', signature_source_delegate_name='', native_delegate_name='', call_in_editor=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False):
    try:
        arguments = _arguments('add_blueprint_custom_event_node', {'blueprint_name': blueprint_name, 'custom_event_name': custom_event_name, 'node_position': node_position, 'signature_source_dispatcher_name': signature_source_dispatcher_name, 'signature_source_class': signature_source_class, 'signature_source_delegate_name': signature_source_delegate_name, 'native_delegate_name': native_delegate_name, 'call_in_editor': call_in_editor, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing})
    except Exception as error:
        return _rejected('add_blueprint_custom_event_node', error)
    return _named(arguments)


def add_blueprint_custom_event_node_action(result, blueprint_name, custom_event_name, node_position=None, signature_source_dispatcher_name='', signature_source_class='', signature_source_delegate_name='', native_delegate_name='', call_in_editor=None, graph_name='', graph_id='', graph_type='', create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments('add_blueprint_custom_event_node', {'blueprint_name': blueprint_name, 'custom_event_name': custom_event_name, 'node_position': node_position, 'signature_source_dispatcher_name': signature_source_dispatcher_name, 'signature_source_class': signature_source_class, 'signature_source_delegate_name': signature_source_delegate_name, 'native_delegate_name': native_delegate_name, 'call_in_editor': call_in_editor, 'graph_name': graph_name, 'graph_id': graph_id, 'graph_type': graph_type, 'create_graph_if_missing': create_graph_if_missing}), checkpoint_callback)
