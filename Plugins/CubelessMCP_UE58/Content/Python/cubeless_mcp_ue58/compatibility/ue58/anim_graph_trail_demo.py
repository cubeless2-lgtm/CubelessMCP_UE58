"""Fixed original Trail demo Native19; no wrapper asset work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessAnimGraphTrailDemoLibrary"
BINDINGS = ('ensure_anim_graph_trail_demo',)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {method: callable(getattr(library, method, None)) for method in BINDINGS}


def _function(method):
    if method not in BINDINGS:
        raise OperationError("anim_graph_trail_demo_native_api_unavailable", "Only the exact TrailDemo Native method is admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("anim_graph_trail_demo_native_api_unavailable", "Exact Native AnimGraph authoring API unavailable; no fallback dispatched")
    return function


def _returned(method, raw):
    try:
        native = _decode_result(raw)
        if native.get("operation") != method:
            raise OperationError("invalid_anim_graph_trail_demo_native_result", "Returned operation differs; raw prefix retained")
        if "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_anim_graph_trail_demo_native_result", "Native changed must be bool or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def ensure_anim_graph_trail_demo(blueprint_name, trail_bone, base_joint, chain_length, chain_bone_axis, alpha, fake_velocity_json, graph_name, graph_id, graph_type, replace_existing, allow_non_sample, invert_chain_bone_axis, reorient_parent_to_child, actor_space_fake_velocity, relaxation_speed_scale, limit_stretch, stretch_limit, max_delta_time):
    method = 'ensure_anim_graph_trail_demo'
    raw = _function(method)(blueprint_name, trail_bone, base_joint, chain_length, chain_bone_axis, alpha, fake_velocity_json, graph_name, graph_id, graph_type, replace_existing, allow_non_sample, invert_chain_bone_axis, reorient_parent_to_child, actor_space_fake_velocity, relaxation_speed_scale, limit_stretch, stretch_limit, max_delta_time)
    return _returned(method, raw)
