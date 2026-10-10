"""Call the editor-only shared Pivot Painter API through the existing MCP bridge."""
import json

from services.unreal_bridge_connection import UnrealBridgeConnection

RESULT_PREFIX = "RX_PIVOT_RESULT:"


def execute_pivot_paint(request: dict, connection=None) -> dict:
    if not isinstance(request, dict):
        raise ValueError("Pivot Painter request must be an object")
    payload = json.dumps(request, ensure_ascii=False, allow_nan=False)
    code = (
        "import unreal\n"
        f"_rx_pivot_response = unreal.RXPivotPaintLibrary.execute({payload!r})\n"
        f"print({RESULT_PREFIX!r} + _rx_pivot_response)\n"
        "del _rx_pivot_response\n"
    )
    bridge = connection or UnrealBridgeConnection(host="127.0.0.1", port=55561, response_timeout_seconds=120)
    response = bridge.send_command("execute_python", {
        "code": code, "mode": "ExecuteFile", "interchange_policy": "none",
        "transaction_policy": "script",
    })
    result = (response or {}).get("result", {})
    if (response or {}).get("status") != "success" or result.get("success") is False:
        raise RuntimeError(f"Pivot Painter bridge execution failed: {response}")
    for log in result.get("logs", []):
        output = log.get("output", "")
        if output.startswith(RESULT_PREFIX):
            parsed = json.loads(output[len(RESULT_PREFIX):])
            if not isinstance(parsed, dict) or not isinstance(parsed.get("success"), bool):
                raise RuntimeError("Invalid Pivot Painter response")
            return parsed
    raise RuntimeError("No Pivot Painter response returned; check that RXSpeedTreeImporter is enabled and built")
