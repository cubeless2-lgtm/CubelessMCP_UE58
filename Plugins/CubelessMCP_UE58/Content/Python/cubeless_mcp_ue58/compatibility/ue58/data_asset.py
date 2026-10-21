"""Original DataAsset Get1/Set3/Create3 exact hidden bindings; no loader/fallback."""
import unreal
import json
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessDataAssetLibrary"
BINDINGS = ("get_data_asset", "set_data_asset_properties", "create_data_asset")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _finite_reference_clear_request(name, arguments):
    # Only the new empty-array selector cohort; no UObject wrapper reads or imports.
    if name != "set_data_asset_properties" or len(arguments) != 3:
        return False
    try:
        properties = json.loads(arguments[1])
    except (ValueError, TypeError):
        return False
    if type(properties) is not dict:
        return False
    action_arrays = [value for key, value in properties.items()
                     if key.casefold() in ("modifiers", "triggers")]
    action_empty = any(type(value) is list and not value for value in action_arrays)
    action_nonempty = any(type(value) is list and value for value in action_arrays)
    if len(properties) == 1 and action_arrays == [None]:
        return True  # Exact new sole-null preflight refusal; mixed legacy properties stay unchanged.
    if action_empty and not action_nonempty:
        return True
    mapping_owned = False
    for key, value in properties.items():
        if key.casefold() != "defaultkeymappings" or type(value) is not dict:
            continue
        for inner, rows in value.items():
            if inner.casefold() != "mappings" or type(rows) is not list:
                continue
            mapping_owned |= any(type(row) is dict and any(
                field.casefold() in ("modifiers", "triggers") and type(slots) is list and slots
                for field, slots in row.items()) for row in rows)
    if not mapping_owned:
        for key, value in properties.items():
            if key.casefold() == "defaultkeymappings" and type(value) is dict and len(value) == 1:
                if any(inner.casefold() == "mappings" and type(rows) is list and not rows
                       for inner, rows in value.items()):
                    return True
    return False


def _finite_owned_array_request(name, arguments):
    # MS: lifetime only; native retains class/owner/property admission and all writers.
    if name != "set_data_asset_properties" or len(arguments) != 3:
        return False
    try:
        properties = json.loads(arguments[1])
    except (ValueError, TypeError):
        return False
    if type(properties) is not dict:
        return False
    for key, value in properties.items():
        if key.casefold() in ("modifiers", "triggers") and type(value) is list and value:
            return True
        if key.casefold() == "defaultkeymappings" and type(value) is dict:
            if any(inner.casefold() == "mappings" and type(rows) is list and rows
                   for inner, rows in value.items()):
                return True
    return False


def _invoke(name, *arguments):
    if name not in BINDINGS:
        raise OperationError("data_asset_operation_invalid", "Only the three original DataAsset names are admitted")
    fn = getattr(getattr(unreal, LIBRARY, None), name, None)
    if not callable(fn):
        raise OperationError("native_data_asset_api_unavailable", "Exact native DataAsset binding unavailable; no fallback/load/writer dispatched")
    try:
        raw = fn(*arguments)
    finally:
        fn = None  # Release the common bound callable before decode/validation/raise.
    try:
        value = _decode_result(raw)
        if (value.get("operation") != name or type(value.get("outcome_unknown")) is not bool
                or value["outcome_unknown"] is not (not value["change_state_known"])
                or "changed" not in value or value["changed"] is not None and type(value["changed"]) is not bool):
            raise OperationError("invalid_native_data_asset_result", "Returned operation/unknown scope differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def get_data_asset(asset_path):
    return _invoke("get_data_asset", asset_path)


def set_data_asset_properties(asset_path, properties_json, save):
    return _invoke("set_data_asset_properties", asset_path, properties_json, save)


def create_data_asset(asset_path, class_path, save):
    return _invoke("create_data_asset", asset_path, class_path, save)
