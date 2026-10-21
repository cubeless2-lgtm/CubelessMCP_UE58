"""Fixed original SecondaryMotion chain Native19; no wrapper asset work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessAnimGraphSecondaryMotionLibrary"
BINDINGS = ('ensure_anim_graph_secondary_motion_chain',)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {method: callable(getattr(library, method, None)) for method in BINDINGS}


def _function(method):
    if method not in BINDINGS:
        raise OperationError("anim_graph_secondary_motion_native_api_unavailable", "Only the exact SecondaryMotionChain Native method is admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("anim_graph_secondary_motion_native_api_unavailable", "Exact Native AnimGraph authoring API unavailable; no fallback dispatched")
    return function


def _returned(method, raw):
    try:
        native = _decode_result(raw)
        if native.get("operation") != method:
            raise OperationError("invalid_anim_graph_secondary_motion_native_result", "Returned operation differs; raw prefix retained")
        if "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_anim_graph_secondary_motion_native_result", "Native changed must be bool or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def ensure_anim_graph_secondary_motion_chain(blueprint_name, dynamics_type, root_bone, chain_end_bone, alpha, gravity_json, angular_limit_degrees, body_extents_json, linear_damping, angular_damping, kawaii_damping, kawaii_stiffness, graph_name, graph_id, graph_type, replace_existing, allow_non_sample, compile, save):
    method = 'ensure_anim_graph_secondary_motion_chain'
    raw = _function(method)(blueprint_name, dynamics_type, root_bone, chain_end_bone, alpha, gravity_json, angular_limit_degrees, body_extents_json, linear_damping, angular_damping, kawaii_damping, kawaii_stiffness, graph_name, graph_id, graph_type, replace_existing, allow_non_sample, compile, save)
    return _returned(method, raw)
