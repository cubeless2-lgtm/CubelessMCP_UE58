"""Fixed native PhysicsAsset observer12; no adapter asset loads or fallback."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessPhysicsAssetInspectionLibrary"
METHOD = "inspect_physics_asset_constraints_guarded"
BINDINGS = (METHOD,)
PREPARATION = "prepare_physics_asset_inspection_toolset"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {METHOD: callable(getattr(library, METHOD, None))}


def preparation_available():
    return callable(getattr(getattr(unreal, LIBRARY, None), PREPARATION, None))


def inspect_physics_asset_constraints_guarded(physics_asset_path,
        skeletal_mesh_path,
        include_bodies,
        include_constraints,
        include_profiles,
        include_solver_settings,
        include_collision_summary,
        max_bodies,
        max_constraints,
        max_profiles_per_item,
        max_shapes_per_body,
        require_supported_class):
    function = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(function):
        raise OperationError("native_physics_asset_inspection_api_unavailable", "Exact native PhysicsAsset observer unavailable; no asset load or fallback dispatched")
    raw = function(physics_asset_path,
            skeletal_mesh_path,
            include_bodies,
            include_constraints,
            include_profiles,
            include_solver_settings,
            include_collision_summary,
            max_bodies,
            max_constraints,
            max_profiles_per_item,
            max_shapes_per_body,
            require_supported_class)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD:
            raise OperationError("invalid_native_physics_asset_inspection_result", "Returned observer operation differs; raw prefix retained")
        if "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_native_physics_asset_inspection_result", "Native changed must be bool or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
