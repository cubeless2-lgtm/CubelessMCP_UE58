"""Original PhysicsAsset observer12 on the existing synchronous standard11 result."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import physics_asset_inspection


@unreal.uclass()
class CubelessPhysicsAssetInspectionTools(unreal.ToolsetDefinition):
    """Authored PhysicsAsset bodies, shapes, constraints, profiles and solver settings. No direct asset mutation, compiler, save, collision cook, profile application or physics simulation. Asset/preview loads and SDK caches may have effects. Current static evidence does not prove live physics, cold persistence, cook or complete original workflows."""
    @toolset_registry.tool_call
    @staticmethod
    def inspect_physics_asset_constraints_guarded(physics_asset_path: str = '',
            skeletal_mesh_path: str = '',
            include_bodies: bool = True,
            include_constraints: bool = True,
            include_profiles: bool = True,
            include_solver_settings: bool = True,
            include_collision_summary: bool = True,
            max_bodies: int = 128,
            max_constraints: int = 128,
            max_profiles_per_item: int = 16,
            max_shapes_per_body: int = 32,
            require_supported_class: bool = False) -> CubelessPythonOperationResult:
        """Original two path defaults and all include flags/limits. Native validates at least one path and gives explicit PhysicsAsset precedence over assigned skeletal-mesh resolution. Native owns zero-based clamps, source admission, optional bodies/shape kinds, constraints/default profiles/profile handles, asset-level profile names and solver settings. Null body/constraint rows, original indexes, counts/limits/truncation/warnings/errors/unavailable fields remain intact. Explicit wrong PhysicsAsset type with require_supported_class=False can complete as a diagnostic with errors[]; True refuses it. Wrong skeletal mesh and missing assigned asset remain Native failures. No extra load/getter/compiler/save/retry is issued by this adapter."""
        return reflected_result(physics_asset_inspection.inspect_physics_asset_constraints_guarded(
            physics_asset_path,
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
            require_supported_class))
