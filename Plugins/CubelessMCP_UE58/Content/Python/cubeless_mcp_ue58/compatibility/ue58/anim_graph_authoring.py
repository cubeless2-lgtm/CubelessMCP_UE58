"""Fixed original AnimGraph authoring Native10/7; no wrapper asset work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessAnimGraphAuthoringLibrary"
BINDINGS = ("set_anim_graph_rigidbody_settings", "ensure_anim_graph_input_pose_passthrough")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {method: callable(getattr(library, method, None)) for method in BINDINGS}


def _function(method):
    if method not in BINDINGS:
        raise OperationError("anim_graph_authoring_native_api_unavailable", "Only the exact two Native methods are admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("anim_graph_authoring_native_api_unavailable", "Exact Native AnimGraph authoring API unavailable; no fallback dispatched")
    return function


def _returned(method, raw):
    try:
        native = _decode_result(raw)
        if native.get("operation") != method:
            raise OperationError("invalid_anim_graph_authoring_native_result", "Returned operation differs; raw prefix retained")
        if "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_anim_graph_authoring_native_result", "Native changed must be bool or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def set_anim_graph_rigidbody_settings(blueprint_name, node_id, alpha_json, external_force_json,
        simulation_space, enable_world_geometry_json, allow_non_sample, graph_name, graph_id, graph_type):
    method = "set_anim_graph_rigidbody_settings"
    raw = _function(method)(blueprint_name, node_id, alpha_json, external_force_json,
        simulation_space, enable_world_geometry_json, allow_non_sample, graph_name, graph_id, graph_type)
    return _returned(method, raw)


def ensure_anim_graph_input_pose_passthrough(blueprint_name, graph_name, graph_id, graph_type,
        replace_existing, input_node_position_json, allow_non_sample):
    method = "ensure_anim_graph_input_pose_passthrough"
    raw = _function(method)(blueprint_name, graph_name, graph_id, graph_type,
        replace_existing, input_node_position_json, allow_non_sample)
    return _returned(method, raw)
