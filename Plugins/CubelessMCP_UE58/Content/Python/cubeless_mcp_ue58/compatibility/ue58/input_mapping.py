"""UE58 legacy InputSettings authoring binding; no wrapper engine work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessInputMappingLibrary"
METHOD = "create_input_mapping"
BINDINGS = (METHOD,)

def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}

def create_input_mapping(action_name, key, input_type, shift=False, ctrl=False, alt=False, cmd=False):
    function = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(function):
        raise OperationError("input_mapping_native_api_unavailable", "Exact InputSettings authoring API unavailable; no fallback or config write dispatched")
    raw = function(action_name, key, input_type, shift, ctrl, alt, cmd)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_input_mapping_native_result", "Native Input mapping receipt differs; preserve RAM/cache/disk prefix and inspect before another call")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
