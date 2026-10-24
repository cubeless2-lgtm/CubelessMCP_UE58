"""Exact finite JSON scalar conversion through the installed UE SDK.

UKismetStringLibrary::Conv_DoubleToString uses SanitizeFloat(double,1).
FJsonValueNumber::TryGetString uses the same formatter with min digits0.
Only the final min1 padding suffix .0 is removed; no Python number formatting.
The numeric formatter uses the public BlueprintThreadSafe utility. String bool
conversion uses the internal Editor JSON-value seam; neither needs an asset or
journal, and SDK conversion calls are distinct from original-tool dispatch.
"""
import json
import math
import unreal
from cubeless_mcp_ue58.operations.state import OperationError


def number_to_legacy_string(value):
    if type(value) not in (int, float):
        raise OperationError("json_number_string_type_invalid", "Only an exact JSON number is accepted; Boolean handling stays with the original caller")
    try:
        number = float(value)  # Same binary64 representation as the original JSON DOM.
    except (OverflowError, ValueError) as error:
        raise OperationError("json_number_string_range_pending", "Number cannot be represented as a finite double; original conversion remains incomplete") from error
    if not math.isfinite(number):
        raise OperationError("json_number_string_range_pending", "Nonfinite number conversion remains incomplete")
    convert = getattr(getattr(unreal, "StringLibrary", None), "conv_double_to_string", None)
    if not callable(convert):
        raise OperationError("json_number_string_binding_unavailable", "Installed public StringLibrary.conv_double_to_string is unavailable; no asset operation was dispatched")
    try:
        text = convert(number)
    except Exception as error:
        raise OperationError("json_number_string_sdk_failed", "Read-only SDK numeric conversion failed; no asset operation was dispatched: " + str(error)) from error
    if type(text) is not str or not text:
        raise OperationError("json_number_string_result_invalid", "Read-only SDK numeric conversion did not return a nonempty string; no asset operation was dispatched")
    return text[:-2] if text.endswith(".0") else text


def try_get_legacy_bool(value):
    """Return (read_success, value); callers preserve missing/default separately.

    Mirrors the installed JSON DOM's exact bool/finite-double/null-container
    semantics. Strings go through FJsonValueString::TryGetBool so Engine
    FString::ToBool, including localized forms, remains authoritative.
    This is a pure SDK conversion, not an asset or original-tool dispatch.
    """
    if type(value) is bool:
        return True, value
    if type(value) in (int, float):
        try:
            number = float(value)
        except (OverflowError, ValueError) as error:
            raise OperationError("json_bool_number_range_pending", "Boolean conversion requires the original finite double JSON number") from error
        if not math.isfinite(number):
            raise OperationError("json_bool_number_range_pending", "Nonfinite JSON number conversion remains unsupported")
        return True, number != 0.0
    if value is None or type(value) in (list, dict):
        return False, False  # Base FJsonValue::TryGetBool does not change initialized false.
    if type(value) is not str:
        raise OperationError("json_bool_value_type_invalid", "Only an exact JSON value is accepted; no Python truthiness or custom conversion")
    convert = getattr(getattr(unreal, "CubelessJsonValueLibrary", None), "try_json_string_bool", None)
    if not callable(convert):
        raise OperationError("json_bool_string_binding_unavailable", "Internal Editor JSON-string conversion binding is unavailable; no asset operation was dispatched")
    try:
        raw = convert(value)
    except Exception as error:
        raise OperationError("json_bool_string_sdk_failed", "Read-only SDK JSON-string conversion failed; no asset operation was dispatched: " + str(error)) from error
    try:
        if type(raw) is not str or len(raw) > 256:
            raise ValueError("bounded JSON result required")
        result = json.loads(raw)
        if type(result) is not dict or set(result) != {"read_success", "value"} or any(type(result[key]) is not bool for key in result):
            raise ValueError("exact read_success/value booleans required")
    except Exception as error:
        raise OperationError("json_bool_string_result_invalid", "Read-only SDK JSON-string conversion result is invalid; no asset operation was dispatched") from error
    return result["read_success"], result["value"]
