"""Editor-only SpeedTree commands using the project's existing MCP connection."""
import json
from services.unreal_bridge_connection import UnrealBridgeConnection

RESULT_PREFIX = "RX_SPEEDTREE_RESULT:"


def execute_speedtree_import(request: dict, connection=None) -> dict:
    if not isinstance(request, dict):
        raise ValueError("SpeedTree request must be an object")
    payload = json.dumps(request, ensure_ascii=False, allow_nan=False)
    code = (
        "import unreal\n"
        f"_rx_speedtree_response = unreal.RXSpeedTreeImportLibrary.execute({payload!r})\n"
        f"print({RESULT_PREFIX!r} + _rx_speedtree_response)\n"
        "del _rx_speedtree_response\n"
    )
    bridge = connection or UnrealBridgeConnection(host="127.0.0.1", port=55561, response_timeout_seconds=120)
    policy = "legacy" if request.get("action") in {"import", "reimport", "start_batch", "retry_failed"} else "none"
    response = bridge.send_command("execute_python", {
        "code": code, "mode": "ExecuteFile", "interchange_policy": policy,
        "defer_to_ticker": True
    })
    result = (response or {}).get("result", {})
    if (response or {}).get("status") != "success" or result.get("success") is False:
        raise RuntimeError(f"SpeedTree bridge execution failed: {response}")
    for log in result.get("logs", []):
        output = log.get("output", "")
        if output.startswith(RESULT_PREFIX):
            parsed = json.loads(output[len(RESULT_PREFIX):])
            if not isinstance(parsed, dict) or not isinstance(parsed.get("success"), bool):
                raise RuntimeError("Invalid SpeedTree response")
            return parsed
    raise RuntimeError("No SpeedTree response; check that RXSpeedTreeImporter is enabled and built")
