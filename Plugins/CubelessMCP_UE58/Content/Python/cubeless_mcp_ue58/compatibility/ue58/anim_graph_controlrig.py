"""Fixed original ControlRig input12/forced15 Native APIs; no wrapper asset work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessAnimGraphControlRigLibrary"
BINDINGS = ('set_anim_graph_controlrig_input_defaults', 'ensure_controlrig_forced_driver_animbp')


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {method: callable(getattr(library, method, None)) for method in BINDINGS}


def _function(method):
    if method not in BINDINGS:
        raise OperationError("anim_graph_controlrig_native_api_unavailable", "Only the exact two ControlRig Native methods are admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("anim_graph_controlrig_native_api_unavailable", "Exact Native AnimGraph authoring API unavailable; no fallback dispatched")
    return function


def _returned(method, raw):
    try:
        native = _decode_result(raw)
        if native.get("operation") != method:
            raise OperationError("invalid_anim_graph_controlrig_native_result", "Returned operation differs; raw prefix retained")
        if "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_anim_graph_controlrig_native_result", "Native changed must be bool or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def set_anim_graph_controlrig_input_defaults(blueprint_name, input_defaults_json, expose_input_pins_json, graph_name, graph_id, graph_type, node_id, node_name, title_contains, control_rig_class, disconnect_existing_links, allow_non_sample):
    method = 'set_anim_graph_controlrig_input_defaults'
    raw = _function(method)(blueprint_name, input_defaults_json, expose_input_pins_json, graph_name, graph_id, graph_type, node_id, node_name, title_contains, control_rig_class, disconnect_existing_links, allow_non_sample)
    return _returned(method, raw)


def ensure_controlrig_forced_driver_animbp(blueprint_name, curve_values_json, input_defaults_json, alpha, apply_mode, graph_name, graph_id, graph_type, node_id, node_name, title_contains, control_rig_class, replace_existing, disconnect_existing_links, allow_non_sample):
    method = 'ensure_controlrig_forced_driver_animbp'
    raw = _function(method)(blueprint_name, curve_values_json, input_defaults_json, alpha, apply_mode, graph_name, graph_id, graph_type, node_id, node_name, title_contains, control_rig_class, replace_existing, disconnect_existing_links, allow_non_sample)
    return _returned(method, raw)


