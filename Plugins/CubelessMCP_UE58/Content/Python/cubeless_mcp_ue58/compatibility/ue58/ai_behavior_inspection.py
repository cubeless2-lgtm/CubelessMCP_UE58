"""UE58 paired BT/BB EngineAPI read; no Python loading or type fallback."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessAIBehaviorAssetsInspectionLibrary"
BINDINGS = ("inspect_ai_behavior_assets",)


def binding_status():
    return {"paired_inspector_callable": callable(getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None))}


def inspect_ai_behavior_assets(behavior_tree_path, blackboard_path):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_ai_behavior_inspection_api_unavailable", "Build/load the exact paired BT/Blackboard reader; no Python load, generic executor or fallback ran")
    return _decode_result(function(behavior_tree_path, blackboard_path))
