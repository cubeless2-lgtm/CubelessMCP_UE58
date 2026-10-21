"""Exact UE58 rigid-chain EngineAPI; array normalization stays in domain."""
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessRigidConstraintChainLibrary"
BINDINGS = ("ensure_blueprint_rigid_constraint_chain",)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def ensure_blueprint_rigid_constraint_chain(blueprint_path, body_components_json,
        root_component_name, constraint_prefix, swing1_limit_degrees, swing2_limit_degrees,
        twist_limit_degrees, linear_damping, angular_damping, projection_linear_alpha,
        projection_angular_alpha, projection_linear_tolerance, projection_angular_tolerance,
        required_root, save, dry_run):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_rigid_constraint_chain_api_unavailable", "Build/load the exact rigid-chain helper; no loader, mutation or fallback ran")
    try:
        raw = function(blueprint_path, body_components_json, root_component_name, constraint_prefix,
            swing1_limit_degrees, swing2_limit_degrees, twist_limit_degrees, linear_damping,
            angular_damping, projection_linear_alpha, projection_angular_alpha,
            projection_linear_tolerance, projection_angular_tolerance, required_root, save, dry_run)
    finally:
        function = None
    try:
        return _decode_result(raw)
    except Exception as error:
        error.native_raw_result = raw
        error.native_call_returned = True
        raise
