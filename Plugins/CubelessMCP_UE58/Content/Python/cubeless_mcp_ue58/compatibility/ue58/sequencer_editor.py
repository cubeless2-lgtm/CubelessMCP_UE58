"""One exact loaded-sequence native UI call; no lookup/load or fallback."""
import unreal
from .functions import _decode_result
from .sequencer_maintenance import _object_path
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessSequencerLibrary"
BINDINGS = ("open_level_sequence_in_editor",)

def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}

def open_level_sequence_in_editor(sequence_path):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("sequencer_editor_native_api_unavailable", "Exact native Editor UI binding unavailable; no dispatch or fallback ran")
    raw = function(_object_path(sequence_path))
    try:
        value = _decode_result(raw)
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
