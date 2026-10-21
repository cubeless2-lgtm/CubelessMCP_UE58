"""One fixed original AnimBlueprintSample10 Native API; no wrapper asset calls.

Selected Native-author Library/ordinal pact; actual final header/core and
Engine registration remain separate evidence, never inferred from this file.
"""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
LIBRARY = "CubelessAnimBlueprintSampleLibrary"
BINDINGS = ("create_anim_blueprint_sample",)

def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {method: callable(getattr(library, method, None)) for method in BINDINGS}

def _function(method):
    if method not in BINDINGS:
        raise OperationError("anim_blueprint_sample_native_api_unavailable", "Only the exact original sample creator admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("anim_blueprint_sample_native_api_unavailable", "Exact Native AnimBlueprint sample API unavailable; no fallback dispatched")
    return function

def _returned(method, raw):
    try:
        native = _decode_result(raw)
        if native.get("operation") != method or "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_anim_blueprint_sample_native_result", "Returned operation/current changed receipt differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def create_anim_blueprint_sample(target_anim_bp_path, skeleton_path, skeletal_mesh_path, preview_skeletal_mesh_path, parent_class_path, overwrite_existing, compile, save_asset, dry_run, validate_only):
    method = 'create_anim_blueprint_sample'
    raw = _function(method)(target_anim_bp_path, skeleton_path, skeletal_mesh_path, preview_skeletal_mesh_path, parent_class_path, overwrite_existing, compile, save_asset, dry_run, validate_only)
    return _returned(method, raw)
