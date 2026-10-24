"""Editor Slate status-window seam; no bridge, port, server or asset API."""
import json
import unreal
from cubeless_mcp_ue58.operations.state import OperationError


def available():
    return callable(getattr(getattr(unreal, "CubelessStatusWindowLibrary", None), "show_status_window", None))


def show(text, command):
    method = getattr(getattr(unreal, "CubelessStatusWindowLibrary", None), "show_status_window", None)
    if not callable(method):
        raise OperationError("status_window_binding_unavailable", "Editor status-window binding is unavailable; no UI call was dispatched")
    raw = method(text, command)
    try:
        result = json.loads(raw)
        fields = ("success", "ui_attempted", "ui_changed", "ui_state_known", "window_created", "window_present", "asset_mutation_attempted", "progress_applied")
        if type(result) is not dict or any(type(result.get(key)) is not bool for key in fields):
            raise ValueError("exact native status-window receipt required")
        if result["success"] and not all(result[key] for key in ("ui_attempted", "ui_changed", "ui_state_known", "window_present")):
            raise ValueError("successful UI receipt requires actual completed owned-window work")
        if not result["ui_attempted"] and result["ui_changed"]:
            raise ValueError("no-attempt result cannot claim UI change")
        if result["asset_mutation_attempted"] or result["progress_applied"]:
            raise ValueError("unexpected asset/progress effect")
        if any(type(result.get(key)) is not str for key in ("stage", "error_code", "error")):
            raise ValueError("native failure stage/code/text required")
    except Exception as error:
        failure = OperationError("status_window_result_invalid", "Native status-window receipt is invalid; actual UI outcome is uncertain and must not be retried automatically")
        failure.native_call_returned = True
        failure.native_raw_result = raw
        raise failure from error
    result["_raw_native_json"] = raw
    return result


def close():
    method = getattr(getattr(unreal, "CubelessStatusWindowLibrary", None), "close_status_window", None)
    if callable(method):
        method()
