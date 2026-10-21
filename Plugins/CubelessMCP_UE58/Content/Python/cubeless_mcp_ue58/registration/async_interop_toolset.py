"""CU reflected async interop probe; no operation journal or asset work."""

import uuid
import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result


_ACTIVE = False
_PENDING = {}
_SHUTDOWN_HANDLE = None
_MAX_PENDING = 4
_LAST_TERMINAL = {}


@unreal.uclass()
class CubelessPythonAsyncOperationResult(unreal.CubelessAsyncOperationResult):
    """Actual async derivative carrying the existing reflected standard11 value."""

    value = unreal.uproperty(CubelessPythonOperationResult)


def binding_status():
    """Source binding/lifecycle observations only; no registry or schema claim."""
    return {
        "native_class_available": issubclass(CubelessPythonAsyncOperationResult,
                                             unreal.CubelessAsyncOperationResult),
        "complete_value_callable": callable(getattr(unreal.CubelessAsyncOperationResult,
                                                     "complete_value", None)),
        "ticker_register_callable": callable(getattr(unreal, "register_ticker_callback", None)),
        "ticker_unregister_callable": callable(getattr(unreal, "unregister_ticker_callback", None)),
        "frame_count_callable": callable(getattr(unreal.SystemLibrary, "get_frame_count", None)),
        "shutdown_register_callable": callable(getattr(unreal, "register_python_shutdown_callback", None)),
        "shutdown_unregister_callable": callable(getattr(unreal, "unregister_python_shutdown_callback", None)),
        "active": _ACTIVE, "pending_count": len(_PENDING), "pending_limit": _MAX_PENDING,
        "last_terminal": dict(_LAST_TERMINAL),
        "registered": False, "schema_verified": False, "actual_probe_verified": False,
        "asset_mutation": False,
    }


def _remove_ticker(entry):
    # Public unregister returns None, not a native-handle membership proof.
    handle = entry["handle"]
    entry["handle"] = None
    if handle is not None:
        entry["ticker_unregister_attempt_count"] += 1
        unreal.unregister_ticker_callback(handle)
        entry["ticker_unregister_return_count"] += 1


def _error_result(entry, reason):
    result = entry["result"]  # Strong Python wrapper retained through SetError.
    if not result.get_editor_property("is_complete"):
        result.set_error(reason)
        return True
    return False


def _tick(token, delta_seconds):
    global _LAST_TERMINAL
    entry = _PENDING.get(token)
    if entry is None:
        return False
    try:
        entry["tick_callback_count"] += 1
        current_frame = unreal.SystemLibrary.get_frame_count()
        if type(current_frame) is not int:
            raise TypeError("CU frame counter did not return int")
        if current_frame <= entry["creation_frame"]:
            return True
        _remove_ticker(entry)
        record = {
            "success": True, "operation_id": token, "stage": "later_editor_frame_value_ready",
            "error_code": "", "error": "", "changes": [],
            "compile": {"requested": False, "attempted": False, "compiled": None},
            "save": {"requested": False, "attempted": False, "saved": False},
            "validation": {"performed": True, "scope": "async binding/future/schema probe only",
                           "legacy_migration_verified": False, "asset_behavior_verified": False,
                           "shader_completion_verified": False, "batch_completion_verified": False},
            "retry": {"automatic_retry": False, "journaled": False},
            "data": {"marker": entry["marker"], "creation_frame": entry["creation_frame"],
                     "completion_frame": current_frame, "later_frame_observed": True,
                     "tick_callback_count": entry["tick_callback_count"],
                     "ticker_unregister_attempt_count": entry["ticker_unregister_attempt_count"],
                     "ticker_unregister_return_count": entry["ticker_unregister_return_count"],
                     "native_handle_membership_verified": False, "asset_mutation": False},
        }
        result = entry["result"]
        result.set_editor_property("value", reflected_result(record))
        entry["completion_attempt_count"] += 1
        completed = result.complete_value()
        entry["completion_return_count"] += 1
        if completed is not True:
            _error_result(entry, "CU native completion refused: " +
                          str(result.get_editor_property("completion_diagnostic")))
        _LAST_TERMINAL = {"token": token, "completed_returned": completed is True,
                          "completion_attempt_count": entry["completion_attempt_count"],
                          "completion_return_count": entry["completion_return_count"],
                          "ticker_unregister_return_count": entry["ticker_unregister_return_count"]}
    except Exception as exc:
        reason = "CU async callback failed: " + str(exc)[:1024]
        errors = []
        try:
            _remove_ticker(entry)
        except Exception as cleanup_exc:
            errors.append(str(cleanup_exc)[:1024])
        try:
            _error_result(entry, reason)
        except Exception as completion_exc:
            errors.append(str(completion_exc)[:1024])
        _LAST_TERMINAL = {"token": token, "error": reason, "cleanup_errors": errors,
                          "completion_attempt_count": entry["completion_attempt_count"],
                          "completion_return_count": entry["completion_return_count"],
                          "ticker_unregister_attempt_count": entry["ticker_unregister_attempt_count"],
                          "ticker_unregister_return_count": entry["ticker_unregister_return_count"]}
    _PENDING.pop(token, None)
    return False


def _shutdown():
    deactivate("CU Python shutdown canceled pending probe")


def activate():
    """Root calls before registering this exact new Toolset class."""
    global _ACTIVE, _SHUTDOWN_HANDLE
    status = binding_status()
    required = ("native_class_available", "complete_value_callable", "ticker_register_callable",
                "ticker_unregister_callable", "frame_count_callable", "shutdown_register_callable",
                "shutdown_unregister_callable")
    if not all(status[name] is True for name in required):
        raise RuntimeError("CU async public bindings unavailable")
    if not _ACTIVE:
        if _PENDING:
            raise RuntimeError("CU inactive module retains pending probes")
        _SHUTDOWN_HANDLE = unreal.register_python_shutdown_callback(_shutdown)
        _ACTIVE = True
    return binding_status()


def deactivate(reason="CU probe registration deactivated"):
    """Root calls before class unregister; shutdown uses the same bounded cleanup."""
    global _ACTIVE, _SHUTDOWN_HANDLE, _LAST_TERMINAL
    _ACTIVE = False
    errors = []
    canceled = 0
    for token, entry in tuple(_PENDING.items()):
        try:
            _remove_ticker(entry)
        except Exception as exc:
            errors.append({"token": token, "ticker_error": str(exc)[:1024]})
        try:
            if _error_result(entry, reason):
                canceled += 1
        except Exception as exc:
            errors.append({"token": token, "completion_error": str(exc)[:1024]})
        _PENDING.pop(token, None)
    handle = _SHUTDOWN_HANDLE
    _SHUTDOWN_HANDLE = None
    shutdown_unregister_returned = False
    if handle is not None:
        try:
            unreal.unregister_python_shutdown_callback(handle)
            shutdown_unregister_returned = True
        except Exception as exc:
            errors.append({"shutdown_error": str(exc)[:1024]})
    _LAST_TERMINAL = {"canceled_pending_count": canceled, "cleanup_errors": errors,
                      "shutdown_unregister_returned": shutdown_unregister_returned}
    return binding_status()


@unreal.uclass()
class CubelessAsyncInteropTools(unreal.ToolsetDefinition):
    """One no-asset probe of reflected async Value, schema and official future completion."""

    @toolset_registry.tool_call
    @staticmethod
    def probe_async_operation_result(marker: str) -> CubelessPythonAsyncOperationResult:
        """Return the existing standard11 result on an observed later Editor frame.

        Args:
            marker: Required UTF-8 text up to 1024 bytes, without NUL; returned unchanged.
        """
        if type(marker) is not str or "\x00" in marker or len(marker.encode("utf-8")) > 1024:
            raise ValueError("CU marker must be UTF-8 text up to 1024 bytes without NUL")
        if not _ACTIVE:
            raise RuntimeError("CU async probe is not activated")
        if len(_PENDING) >= _MAX_PENDING:
            raise RuntimeError("CU async pending bound reached")
        creation_frame = unreal.SystemLibrary.get_frame_count()
        if type(creation_frame) is not int:
            raise TypeError("CU frame counter did not return int")
        result = CubelessPythonAsyncOperationResult()
        token = "CU_" + uuid.uuid4().hex
        entry = {"result": result, "marker": marker, "creation_frame": creation_frame,
                 "tick_callback_count": 0, "handle": None,
                 "ticker_unregister_attempt_count": 0, "ticker_unregister_return_count": 0,
                 "completion_attempt_count": 0, "completion_return_count": 0}
        _PENDING[token] = entry
        try:
            entry["handle"] = unreal.register_ticker_callback(
                lambda delta_seconds: _tick(token, delta_seconds), delay=0.0)
        except Exception as exc:
            _PENDING.pop(token, None)
            result.set_error("CU ticker registration failed: " + str(exc)[:1024])
        return result
