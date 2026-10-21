"""Specific UE58 native function extraction; no transport or fallback."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessBlueprintFunctionExtractionLibrary"
METHOD = "collapse_blueprint_nodes_to_function"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576
LEGACY_COMMAND_FIELDS = ("graph_id",)


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _private_context(legacy):
    if legacy is None:
        return {}
    if type(legacy) is not dict or any(key not in LEGACY_COMMAND_FIELDS for key in legacy):
        raise OperationError("blueprint_function_extraction_input_invalid",
            "Private context permits only the confirmed graph_id raw value/presence")
    return {key: legacy[key] for key in LEGACY_COMMAND_FIELDS if key in legacy}


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("blueprint_function_extraction_carrier_invalid",
            "Node selection and limited graph context require finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("blueprint_function_extraction_carrier_bound",
            "Each carrier must fit 1 Mi UTF16 units and contain no raw NUL")
    return encoded


def collapse_blueprint_nodes_to_function(blueprint_name, graph_name, function_name, node_ids, graph_type='event', expected_node_count=-1, allow_non_temp=False, dry_run=True, compile=True, save_asset=False, *, _legacy_command_params=None):
    try:
        nodes_json = _carrier(node_ids)
        context_json = _carrier(_private_context(_legacy_command_params))
    except Exception as error:
        error.native_call_dispatched = False
        raise
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        error = OperationError("blueprint_function_extraction_native_api_unavailable",
            "UE58 function extraction binding is unavailable; no fallback executed")
        error.native_call_dispatched = False
        raise error
    try:
        raw = fn(blueprint_name, graph_name, function_name, nodes_json, graph_type,
            expected_node_count, allow_non_temp, dry_run, compile, save_asset, context_json)
    finally:
        fn = None
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("blueprint_function_extraction_native_result_invalid", "Native operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("blueprint_function_extraction_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
