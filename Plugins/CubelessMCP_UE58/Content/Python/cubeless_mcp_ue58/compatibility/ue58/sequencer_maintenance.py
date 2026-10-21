"""Two explicit Sequencer maintenance native calls and their typed results.

Native owns original requested exact save. No loader, scripting fallback,
second save, UI/player or generic command transport is provided here.
"""
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessSequencerLibrary"
BINDINGS = ("set_level_sequence_playback_range", "remove_level_sequence_binding")
PREPARATION = "prepare_sequencer_maintenance_toolset"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(name, *arguments):
    function = getattr(getattr(unreal, LIBRARY, None), name, None)
    if not callable(function):
        raise OperationError("sequencer_maintenance_native_api_unavailable", "Exact finite Sequencer maintenance binding unavailable; no native call/fallback ran")
    try:
        raw = function(*arguments)
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


def _object_path(package):
    # The domain has already admitted/normalized an exact canonical package.
    # Native Resolve requires its object identity; no lookup or load is needed.
    return package + "." + package.rsplit("/", 1)[-1]


def set_level_sequence_playback_range(sequence_path, playback_start_frame, playback_end_frame, save):
    return _invoke(BINDINGS[0], _object_path(sequence_path), playback_start_frame, playback_end_frame, save)


def remove_level_sequence_binding(sequence_path, binding_id, actor_label, save):
    return _invoke(BINDINGS[1], _object_path(sequence_path), binding_id, actor_label, save)
