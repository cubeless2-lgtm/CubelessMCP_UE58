"""Original Ieta UI status operation through official Cubeless tools, without TCP."""
import math
from cubeless_mcp_ue58.compatibility.ue58 import status_window as api
from .state import OperationError, fail, new_result


def _show(result, text, command, progress, checkpoint_callback=None):
    result["data"].update(operation="show_ieta_connection_status", original_command="ieta_status",
        connection_status="not_checked", legacy_tcp_connection_checked=False,
        editor_api_response_confirmed=False, slate_call="not_dispatched", asset_mutation_attempted=False,
        progress_applied=False, native_dispatched=False, ui_state_known=True, ui_changed=False)
    try:
        if type(text) is not str or type(command) is not str or type(progress) not in (int, float):
            raise OperationError("status_window_input_invalid", "Original text/command strings and finite numeric progress are required")
        if not math.isfinite(progress):
            raise OperationError("status_window_input_invalid", "Progress must be finite; it is input compatibility only, not an Engine UI effect")
        result["data"].update(progress_requested=progress, progress_clamped=max(0.0, min(1.0, progress)), requested_text=text, requested_command=command)
        if not api.available():
            raise OperationError("status_window_binding_unavailable", "Editor status-window binding is unavailable; no UI call was dispatched")
        result["stage"] = "before_ui_status_checkpoint"
        result["data"]["pending_ui_status"] = {"text": text, "command": command}
        if checkpoint_callback:
            checkpoint_callback(result)
        result["stage"] = "native_status_window"
        result["data"].update(native_dispatched=True, ui_state_known=False)
        result["retry"]["duplicate_risk"] = "ui_outcome_unknown_no_automatic_retry"
        result["changes"] = [{"kind": "editor_status_window", "attempted": True, "changed": None, "asset_change": False}]
        native = api.show(text, command)
        result["data"].update(native_result=native, native_raw_result=native["_raw_native_json"], editor_api_response_confirmed=True,
            ui_state_known=native["ui_state_known"], ui_changed=native["ui_changed"],
            slate_call="success" if native["success"] else "refused" if not native["ui_attempted"] else "failed_after_ui_prefix")
        result["changes"] = []
        if native["ui_attempted"]:
            result["changes"] = [{"kind": "editor_status_window", "attempted": True,
                "changed": native["ui_changed"] if native["ui_state_known"] else None, "asset_change": False}]
        result["validation"].update(status="native_slate_request_receipt", screen_pixels_verified=False, legacy_tcp_verified=False)
        result["retry"].update(duplicate_risk="status_window_update_no_automatic_retry", atomic_rollback=False)
        result["stage"] = "returned_status_window_operation"
        if checkpoint_callback:
            checkpoint_callback(result)
        result["stage"] = native["stage"]
        if not native["success"]:
            raise OperationError(native["error_code"] or "status_window_failed", native["error"] or "Native status UI was not completed")
        result.update(success=True, stage="complete")
        return result
    except Exception as error:
        if getattr(error, "code", "") == "status_window_binding_unavailable":
            result["data"].update(native_dispatched=False, ui_state_known=True)
            result["changes"] = []
            result["retry"]["duplicate_risk"] = "none_observed"
        if getattr(error, "native_call_returned", False):
            result["data"]["native_raw_result"] = getattr(error, "native_raw_result", None)
        return fail(result, error)


def close():
    api.close()


def show_ieta_connection_status(text="흥, Unreal MCP 연결은 확인했어.", command="이에타", progress=1.0):
    return _show(new_result(), text, command, progress)


def show_ieta_connection_status_action(result, text="흥, Unreal MCP 연결은 확인했어.", command="이에타", progress=1.0, checkpoint_callback=None):
    actual = _show(result, text, command, progress, checkpoint_callback)
    if not actual["success"]:
        raise OperationError(actual["error_code"], actual["error"])
    return actual
