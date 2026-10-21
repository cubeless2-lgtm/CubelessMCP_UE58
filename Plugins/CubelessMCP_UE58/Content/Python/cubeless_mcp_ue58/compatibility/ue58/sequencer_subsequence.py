"""One explicit loaded-parent/child subsequence native call, no fallback."""
import unreal

from .functions import _decode_result
from .sequencer_maintenance import _object_path
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessSequencerLibrary"
BINDINGS = ("add_level_sequence_subsequence",)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def add_level_sequence_subsequence(sequence_path, subsequence_path, start_frame, duration_frames, save):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("sequencer_subsequence_native_api_unavailable", "Exact finite native subsequence binding unavailable; no native call/fallback ran")
    # Both arguments are already admitted canonical packages from the domain.
    # Adapt strings only; neither child nor parent is loaded here.
    try:
        raw = function(_object_path(sequence_path), _object_path(subsequence_path), start_frame, duration_frames, save)
    finally:
        function = None
    try:
        value = _decode_result(raw)
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
