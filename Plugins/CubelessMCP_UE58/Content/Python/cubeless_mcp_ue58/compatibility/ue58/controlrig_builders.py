"""Fixed original sample builders13/20; Native owns every asset/compiler/save action.

Library pact selected by the Native author. Actual frozen-header ABI validation
is pending; this candidate does not claim registration or Engine readiness.
"""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
LIBRARY = "CubelessControlRigSampleLibrary"
BINDINGS = ("ensure_anim_graph_controlrig_node_sample", "ensure_controlrig_sample_asset")

def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {m: callable(getattr(library, m, None)) for m in BINDINGS}

def _function(method):
    if method not in BINDINGS:
        raise OperationError("controlrig_builders_native_api_unavailable", "Only two exact original sample builder methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("controlrig_builders_native_api_unavailable", "Exact Native sample builder unavailable; no fallback dispatched")
    return function

def _returned(method, raw):
    try:
        native = _decode_result(raw)
        if native.get("operation") != method or "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_controlrig_builders_native_result", "Returned original operation/known-current changed receipt differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def ensure_anim_graph_controlrig_node_sample(source_anim_bp_path, control_rig_asset_path, control_rig_class_path, target_anim_bp_path, target_root, variant_name, allow_existing_target_reuse, overwrite_existing, replace_existing_links, compile, save_asset, dry_run, validate_only):
    method = 'ensure_anim_graph_controlrig_node_sample'
    raw = _function(method)(source_anim_bp_path, control_rig_asset_path, control_rig_class_path, target_anim_bp_path, target_root, variant_name, allow_existing_target_reuse, overwrite_existing, replace_existing_links, compile, save_asset, dry_run, validate_only)
    return _returned(method, raw)


def ensure_controlrig_sample_asset(target_control_rig_path, skeletal_mesh_path, skeleton_path, preview_skeletal_mesh_path, setup_mode, curves_json, nulls_json, controls_json, unit_nodes_json, max_direct_curves, max_direct_nulls, max_direct_controls, max_unit_nodes, allow_existing_target_reuse, overwrite_existing, modular_rig, compile, save_asset, dry_run, validate_only):
    method = 'ensure_controlrig_sample_asset'
    raw = _function(method)(target_control_rig_path, skeletal_mesh_path, skeleton_path, preview_skeletal_mesh_path, setup_mode, curves_json, nulls_json, controls_json, unit_nodes_json, max_direct_curves, max_direct_nulls, max_direct_controls, max_unit_nodes, allow_existing_target_reuse, overwrite_existing, modular_rig, compile, save_asset, dry_run, validate_only)
    return _returned(method, raw)
