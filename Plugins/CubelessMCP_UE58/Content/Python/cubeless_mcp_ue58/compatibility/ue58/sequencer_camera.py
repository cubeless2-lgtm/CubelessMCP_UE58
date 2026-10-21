"""One explicit loaded-sequence CameraCut native call, no fallback or save."""
import unreal

from .functions import _decode_result
from .sequencer_maintenance import _object_path
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessSequencerLibrary"
BINDINGS = ("add_level_sequence_camera_cut",)
PREPARATION = "prepare_sequencer_camera_toolset"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def add_level_sequence_camera_cut(sequence_path, start_frame, end_frame, binding_id, actor_label, save):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("sequencer_camera_native_api_unavailable", "Exact finite native CameraCut binding unavailable; no native call/fallback ran")
    # The first argument is the admitted canonical package. Adapt only that
    # string; literal selectors and all numeric values reach native unchanged.
    try:
        raw = function(_object_path(sequence_path), start_frame, end_frame, binding_id, actor_label, save)
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
