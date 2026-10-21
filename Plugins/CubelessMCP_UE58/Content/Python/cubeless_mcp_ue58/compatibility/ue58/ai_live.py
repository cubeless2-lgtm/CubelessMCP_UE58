"""UE58 typed completion adapter only; no gameplay, bridge or registry import."""
import json
import unreal
from ...operations.state import OperationError


def result_class():
    cls = getattr(unreal, "CubelessAILiveResult", None)
    if cls is None or not issubclass(cls, unreal.ToolCallAsyncResult) or not callable(getattr(cls, "set_value", None)):
        raise OperationError("ai_live_result_unavailable", "Build/load the exact Editor-only CubelessAILiveResult; no gameplay was dispatched")
    if cls.static_class().get_path_name() != "/Script/CubelessMCPUE58Editor.CubelessAILiveResult":
        raise OperationError("ai_live_result_unavailable", "The completion class must be the exact native Editor derivative")
    return cls


def create_result():
    return unreal.new_object(result_class())


def deliver(result, report):
    # Native completion is an object-valued promise, not a JSON string result.
    payload = json.dumps(report, ensure_ascii=False, allow_nan=False)
    if not result.set_value(payload):
        raise OperationError("ai_live_delivery_failed", "Fixed terminal result was already completed or rejected; do not restart its Engine workflow")


def adapter_error(result, message):
    return result.set_error(str(message)[:1024])
