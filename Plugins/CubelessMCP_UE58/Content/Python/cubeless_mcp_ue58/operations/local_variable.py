"""Original local-variable12 Any defaults on held native scalar, primitive and exact native struct Array slices.

One standalone or parent journal; full returned native prefix before errors.
Public Any is intact JSON, not a reflected string or object surrogate.
"""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import local_variable as api
from . import state, variables
from .datatable import _reference
from .state import OperationError

IDENTITY = "add_blueprint_local_variable"
RETURNED_STAGE = "returned_local_variable_operation"


def _literal(value, field):
    if type(value) is not str:
        raise OperationError("local_variable_input_invalid", field + " requires a literal string")
    # Native owns its bounded field/name/selector admission. No trim or alias
    # normalization changes the requested original graph/default semantics.
    return value


def _default_json(value):
    count = 0
    def visit(item, depth, field):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("local_variable_default_invalid", "Default JSON exceeds the native depth/value budget")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("local_variable_default_invalid", "JSON object keys must be strings")
                visit(child, depth + 1, field + "." + key)
        elif type(item) is list:
            for index, child in enumerate(item):
                visit(child, depth + 1, field + "[" + str(index) + "]")
        elif type(item) is str:
            _reference(item, field)  # Original recursively guards VALUE strings.
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("local_variable_default_invalid", "JSON numbers must be finite")
        elif item is not None and type(item) is not bool:
            raise OperationError("local_variable_default_invalid", "Original Any admits JSON object/array/scalar/null values only")
    visit(value, 0, "default_value")
    try:
        text = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(text.encode("utf-16-le")) // 2
    except (ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("local_variable_default_invalid", "Bounded valid Unicode JSON required") from error
    if units > 65536:
        raise OperationError("local_variable_default_invalid", "Default JSON exceeds the native UTF16 budget")
    # None is explicit JSON null internally. Held CD translates it to original
    # omitted default; explicit []/{} retain their shape for native type/default admission.
    return text


def _arguments(blueprint_name, variable_name, variable_type, default_value, category,
        tooltip, type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing):
    type_object = _literal(type_object, "type_object")
    _reference(type_object, "type_object")
    arguments = {"blueprint_path": variables._path(blueprint_name, write=True),
        "variable_name": _literal(variable_name, "variable_name"),
        "variable_type": _literal(variable_type, "variable_type"), "default_json": _default_json(default_value),
        "category": _literal(category, "category"), "tooltip": _literal(tooltip, "tooltip"),
        "type_object": type_object, "is_array": variables._boolean(is_array, "is_array"),
        "graph_name": _literal(graph_name, "graph_name"), "graph_id": _literal(graph_id, "graph_id"),
        "graph_type": _literal(graph_type, "graph_type"),
        "create_graph_if_missing": variables._boolean(create_graph_if_missing, "create_graph_if_missing")}
    arguments["_public_arguments"] = copy.deepcopy({"blueprint_name": blueprint_name, "variable_name": variable_name,
        "variable_type": variable_type, "default_value": default_value, "category": category, "tooltip": tooltip,
        "type_object": type_object, "is_array": is_array, "graph_name": graph_name, "graph_id": graph_id,
        "graph_type": graph_type, "create_graph_if_missing": create_graph_if_missing})
    return arguments


def _native_arguments(arguments):
    return {key: arguments[key] for key in api.HIDDEN_ARGUMENTS}


def _context(result, arguments):
    result["data"].update(operation=IDENTITY, requested_arguments=copy.deepcopy(_native_arguments(arguments)),
        raw_public_arguments=copy.deepcopy(arguments["_public_arguments"]),
        original_None_default_omitted=arguments["_public_arguments"]["default_value"] is None,
        domain_omission_vs_explicit_None_not_distinguished=True,
        public_default_schema_route="official_named_raw_toolset_full_JSON_Any",
        actual_official_raw_toolset_wire_verified=False, full_original_local_variable_parity=False,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, runtime_or_cold_local_layout_verified=False,
        actual_work_counts_authority="held_CD_native_explicit_receipts_not_source_expected_implicit_counts")


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_no_explicit_full_compile" if not native["compile_attempted"] else "unexpected_native_compile_attempt")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_failed" if native["save_attempted"] else "native_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": IDENTITY, "asset_path": arguments["blueprint_path"],
        "variable_name": arguments["variable_name"], "attempted": native["mutation_attempted"],
        "change_state_known": known, "changed": native["changed"] if known else None,
        "outcome": "native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_local_variable_postcondition" if native["postcondition_passed"] else "native_local_variable_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run", runtime="not_run")
    result["stage"] = RETURNED_STAGE


def _primitive_array_declaration_receipt(result, arguments, native):
    """Validate copied known-success declaration/current Inner metadata without SDK."""
    from .graph_variables import _local_primitive_array_plan
    data = native["data"]
    def require(condition, property_name, expected, actual):
        if not condition:
            result["stage"] = "verification"
            result["data"].update(mutation_state_unknown=True,
                primitive_array_declaration_first_mismatch={"stage": "copied_native_success_receipt", "property": property_name,
                    "expected": copy.deepcopy(expected), "actual": copy.deepcopy(actual),
                    "observation_kind": "copied_response_field_not_engine_reread"})
            raise OperationError("local_primitive_array_receipt_invalid", "Actual local declaration/structural prefix retained; copied Array/Inner receipt inconsistent; no SDK follow-up or retry")
    require(arguments.get("is_array") is True and data.get("native_primitive_array_local_declaration") is True,
        "native_primitive_array_local_declaration", True, data.get("native_primitive_array_local_declaration"))
    for key in ("primitive_array_runtime_initialization_verified", "primitive_array_literal_buffer_import_performed"):
        require(data.get(key) is False, key, False, data.get(key))
    plan = data.get("primitive_array_declaration_plan")
    require(type(plan) is dict, "primitive_array_declaration_plan", "complete copied current local Array/Inner plan", plan)
    try:
        _local_primitive_array_plan(plan, arguments["variable_name"], False, "")
    except OperationError:
        require(False, "primitive_array_declaration_plan", "complete finite current Array/Inner/source plan", plan)
    require(plan.get("variable_guid", "").replace("-", "").casefold() == str(data.get("selected_variable_guid", "")).replace("-", "").casefold()
        and plan.get("variable_name") == data.get("stored_variable_name") and plan.get("stored_default_before") == data.get("default_value"),
        "selected_declaration", "same current GUID/name and preserved declaration default", {key: data.get(key) for key in ("selected_variable_guid", "stored_variable_name", "default_value")})
    native_struct_array = plan.get("value_kind") in ("vector", "rotator", "transform")
    if native_struct_array:
        require(data.get("native_value_struct_array_local_declaration") is True,
            "native_value_struct_array_local_declaration", True, data.get("native_value_struct_array_local_declaration"))
    if plan.get("value_kind") == "transform":
        require(data.get("native_transform_array_local_declaration") is True
            and type(data.get("transform_array_default_text")) is str and data["transform_array_default_text"] == data.get("default_value"),
            "transform_declaration_default_text", "one actual checked Any/group declaration string, not per-element array initialization", data.get("transform_array_default_text"))
    pin = data.get("pin_type")
    require(type(pin) is dict and pin.get("category") == plan["pin_type"]["category"]
        and pin.get("subcategory") == plan["pin_type"]["subcategory"] and pin.get("is_array") is True,
        "declaration_pin_type", "same primitive Array declaration type", pin)
    if native_struct_array:
        require(pin.get("subcategory_object") == plan["native_type_path"], "declaration_struct_binding", plan["native_type_path"], pin.get("subcategory_object"))
        result["data"]["native_value_struct_array_declaration_receipt_verified"] = True
    result["data"].update(primitive_array_declaration_receipt_verified=True,
        primitive_array_declaration_validation_scope="Current Array/Inner metadata and stored descriptor text; stock node defaults and runtime array initialization are separate unverified values")


def _transform_declaration_receipt(result, arguments, native):
    """Copied scalar declaration text only; no extra plan, value or buffer query."""
    data = native["data"]
    pin = data.get("pin_type", {})
    omitted = json.loads(arguments["default_json"]) is None
    text = data.get("transform_default_text")
    valid = (arguments.get("is_array") is False and data.get("native_transform_local_declaration") is True
        and type(text) is str and text == data.get("default_value")
        and data.get("default_omitted_original_None") is omitted and (not omitted or text == "")
        and data.get("transform_runtime_initialization_verified") is False
        and data.get("transform_default_buffer_import_performed") is False
        and type(pin) is dict and pin.get("category") == "struct" and pin.get("subcategory") == "None"
        and pin.get("subcategory_object") == "/Script/CoreUObject.Transform" and pin.get("is_array") is False)
    if not valid:
        result["stage"] = "verification"
        result["data"].update(mutation_state_unknown=True, transform_declaration_receipt_verified=False)
        raise OperationError("local_transform_receipt_invalid", "Actual declaration/structural prefix retained; scalar Transform stored text/type receipt inconsistent; no SDK follow-up or retry")
    result["data"].update(transform_declaration_receipt_verified=True,
        transform_declaration_validation_scope="One stored scalar declaration string; stock pin defaults and runtime initialization remain separate unverified values")


def _action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"]["pending_mutation"] = {"operation": IDENTITY, **arguments, "dispatch_state_at_checkpoint": "not_yet_dispatched"}
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"; callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True, mutation_performed=None)
    result["stage"] = "native_local_variable_operation"
    result["changes"] = [{"kind": IDENTITY, "asset_path": arguments["blueprint_path"], "attempted": None,
        "changed": None, "change_state_known": False, "outcome": "unknown_until_native_response"}]
    try:
        native = api.add_blueprint_local_variable(**_native_arguments(arguments))
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = RETURNED_STAGE; callback(result)
        if getattr(error, "code", "") == "native_local_variable_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native); callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("unexpected_local_variable_compile_save", "Actual unrequested compile/save prefix retained; no second action dispatched")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_local_variable_failed", native.get("error") or "Actual local declaration prefix retained; no retry")
    if not (known := native["change_state_known"]) or not native["postcondition_passed"] or native["data"].get("local_variable_declared") is not True:
        raise OperationError("local_variable_result_unverified", "Native success lacks complete known selected declaration proof; raw prefix retained")
    if arguments.get("is_array") is True:
        _primitive_array_declaration_receipt(result, arguments, native)
    elif (arguments["variable_type"].lower().replace("_", "").replace("-", "").replace(" ", "") == "transform"
            or "native_transform_local_declaration" in native["data"]):
        _transform_declaration_receipt(result, arguments, native)
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_local_declaration_and_journal_no_add_replay"


def add_blueprint_local_variable(blueprint_name, variable_name, variable_type, default_value=None,
        category="", tooltip="", type_object="", is_array=False, graph_name="", graph_id="",
        graph_type="function", create_graph_if_missing=False):
    try:
        arguments = _arguments(blueprint_name, variable_name, variable_type, default_value, category,
            tooltip, type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        result["data"].update(operation=IDENTITY, admission_rejected=True, native_dispatched=False,
            native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
        return result
    return state.mutate(IDENTITY, arguments, None, lambda result: _action(result, arguments))


def add_blueprint_local_variable_action(result, blueprint_name, variable_name, variable_type, default_value=None,
        category="", tooltip="", type_object="", is_array=False, graph_name="", graph_id="",
        graph_type="function", create_graph_if_missing=False, checkpoint_callback=None):
    _action(result, _arguments(blueprint_name, variable_name, variable_type, default_value, category,
        tooltip, type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
