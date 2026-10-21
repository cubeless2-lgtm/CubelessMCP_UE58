"""Original CustomEventMetadata10 on the exact existing native Variable library."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessVariableLibrary"
BINDINGS = ("set_blueprint_custom_event_metadata",)
PREPARATION = "prepare_blueprint_custom_event_metadata_toolset"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def set_blueprint_custom_event_metadata(blueprint_path, custom_event_name, description, node_id, event_name, tooltip,
        call_in_editor_json, set_node_comment, compile, save):
    function = getattr(getattr(unreal,LIBRARY,None),BINDINGS[0],None)
    if not callable(function):
        raise OperationError("native_custom_event_metadata_api_unavailable","Exact native CustomEventMetadata binding unavailable; no load/fallback/writer dispatched")
    raw = function(blueprint_path,custom_event_name,description,node_id,event_name,tooltip,
        call_in_editor_json,set_node_comment,compile,save)
    try:
        native = _decode_result(raw)
        if (native.get("operation") != BINDINGS[0] or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_native_custom_event_metadata_result","Wrong returned operation/changed type; full raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
