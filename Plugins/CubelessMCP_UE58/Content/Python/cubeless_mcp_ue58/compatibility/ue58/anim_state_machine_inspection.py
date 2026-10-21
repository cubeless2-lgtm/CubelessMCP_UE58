"""Exact native state-machine observer6; no adapter asset loads or fallback."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessAnimStateMachineInspectionLibrary"
METHOD = "inspect_anim_state_machine_transitions"
BINDINGS = (METHOD,)
PREPARATION = "prepare_anim_state_machine_inspection_toolset"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {METHOD: callable(getattr(library, METHOD, None))}


def preparation_available():
    return callable(getattr(getattr(unreal, LIBRARY, None), PREPARATION, None))


def inspect_anim_state_machine_transitions(blueprint_name, state_machine_name, include_pins,
        include_rule_graph_nodes, include_state_nodes, max_rule_graph_nodes):
    function = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(function):
        raise OperationError("native_anim_state_machine_api_unavailable", "Exact native observer unavailable; no load or fallback dispatched")
    raw = function(blueprint_name, state_machine_name, include_pins, include_rule_graph_nodes,
        include_state_nodes, max_rule_graph_nodes)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD:
            raise OperationError("invalid_native_anim_state_machine_result", "Returned observer operation differs; raw prefix retained")
        if "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_native_anim_state_machine_result", "Native changed must be bool or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
