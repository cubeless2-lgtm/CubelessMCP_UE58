"""Named Actor contracts; imported only after the native Actor build is ready."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import actor


@unreal.uclass()
class CubelessActorTools(unreal.ToolsetDefinition):
    """Cubeless loaded Actor inspection and single saved /Game persistent-level editing; no streaming/WP/external actors for mutations, no automatic compile/save. Transform JSON is at most 512 UTF-16 units with component magnitudes at most 1e12."""

    @toolset_registry.tool_call
    @staticmethod
    def get_actors_in_level(expected_level_path: str | None = None) -> CubelessPythonOperationResult:
        """List up to 10000 loaded editor-world actors with object names and transforms. Includes loaded sublevels; not unloaded World Partition cells.

        Args:
            expected_level_path: Optional exact persistent-level package path, such as /Game/Maps/MyMap; mismatch fails.
        """
        return reflected_result(actor.get_actors_in_level(expected_level_path))

    @toolset_registry.tool_call
    @staticmethod
    def find_actors_by_name(pattern: str, expected_level_path: str | None = None) -> CubelessPythonOperationResult:
        """Find loaded actors by case-insensitive substring of UObject name, not display label. Empty pattern lists all; bounded at 10000 actors.

        Args:
            pattern: Object-name substring, at most 4096 UTF-16 units; empty string matches every loaded actor.
            expected_level_path: Optional exact persistent-level package path; mismatch fails.
        """
        return reflected_result(actor.find_actors_by_name(pattern, expected_level_path))

    @toolset_registry.tool_call
    @staticmethod
    def get_actor_properties(name: str, expected_level_path: str | None = None) -> CubelessPythonOperationResult:
        """Read exact object-name actor identity and legacy name/class/location/rotation/scale fields; this is not arbitrary reflected property enumeration.

        Args:
            name: Exact case-sensitive UObject actor name, not display label; duplicate names across loaded levels fail as ambiguous.
            expected_level_path: Optional exact persistent-level package path; mismatch fails.
        """
        return reflected_result(actor.get_actor_properties(name, expected_level_path))

    @toolset_registry.tool_call
    @staticmethod
    def spawn_actor(name: str, type: str, location_json: str = "[0,0,0]", rotation_json: str = "[0,0,0]", scale_json: str = "[1,1,1]", expected_level_path: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Spawn one supported native actor with an exact object name and verify its transform. Saved /Game persistent level only; World Partition/external actors excluded.

        Args:
            name: Exact new UObject actor name, not display label, at most 1023 UTF-16 units. Existing object-name collisions fail.
            type: StaticMeshActor, PointLight, SpotLight, DirectionalLight or CameraActor; spelling is case-insensitive.
            location_json: JSON string encoding three finite world XYZ numbers; component magnitude at most 1e12, text at most 512 UTF-16 units; default [0,0,0].
            rotation_json: JSON string encoding finite Pitch/Yaw/Roll degrees, magnitude at most 1e12 and text at most 512 UTF-16 units; default [0,0,0].
            scale_json: JSON string encoding finite XYZ scale, magnitude at most 1e12 and text at most 512 UTF-16 units; default [1,1,1]. Extends the legacy public wrapper's native scale support.
            expected_level_path: Optional exact saved persistent-level package path. Omission still pins the inspected level before dispatch.
            operation_id: Unique durable request ID; omitted/null generates an ID returned in the result. Identical completed requests replay evidence without mutation.
        """
        return reflected_result(actor.spawn_actor(name, type, location_json, rotation_json, scale_json, expected_level_path, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def create_actor(name: str, type: str, location_json: str = "[0,0,0]", rotation_json: str = "[0,0,0]", scale_json: str = "[1,1,1]", expected_level_path: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Preserve the legacy internal create_actor alias using the same spawn core. Alias has its own journal identity; no automatic compile/save or rollback.

        Args:
            name: Exact new UObject actor name, not display label; collisions fail.
            type: StaticMeshActor, PointLight, SpotLight, DirectionalLight or CameraActor; case-insensitive.
            location_json: JSON string with finite world XYZ; magnitudes at most 1e12, text at most 512 UTF-16 units; default [0,0,0].
            rotation_json: JSON string with finite Pitch/Yaw/Roll degrees; magnitudes at most 1e12, text at most 512 UTF-16 units; default [0,0,0].
            scale_json: JSON string with finite XYZ scale; magnitudes at most 1e12, text at most 512 UTF-16 units; default [1,1,1].
            expected_level_path: Optional exact saved /Game persistent-level package path. World Partition and external actors are unsupported.
            operation_id: Unique durable ID; omitted/null generates one. Reusing a spawn_actor ID here is an operation conflict.
        """
        return reflected_result(actor.create_actor(name, type, location_json, rotation_json, scale_json, expected_level_path, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def spawn_blueprint_actor(blueprint_name: str, actor_name: str, location_json: str = "[0,0,0]", rotation_json: str = "[0,0,0]", scale_json: str = "[1,1,1]", expected_level_path: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Spawn a compiled, clean, saved /Game Actor Blueprint with an exact UObject name. No explicit compile/save; asset loading can trigger PostLoad work. Construction scripts execute; arbitrary side effects and rollback are not guaranteed. Saved dependency inspection is bounded at 256 packages. Native parents are limited to Engine/CoreUObject; dependencies use /Game, /Engine and the audited Engine/CoreUObject/PhysicsCore/MeshDescription/StaticMeshDescription/NavigationSystem modules.

        Args:
            blueprint_name: Exact /Game Blueprint asset package/object path, or simple name under /Game/Blueprints. Generated-class paths and editor-only/tool dependencies are rejected.
            actor_name: Exact new UObject actor name, not display label; case-insensitive collisions fail before spawn.
            location_json: JSON string with finite XYZ world location; magnitudes at most 1e12, text at most 512 UTF-16 units.
            rotation_json: JSON string with finite Pitch/Yaw/Roll degrees; same bounds.
            scale_json: JSON string with finite XYZ scale; default [1,1,1], same bounds.
            expected_level_path: Optional exact saved /Game persistent-level package path; omission pins inspected level. One non-WP level without external actors only.
            operation_id: Unique durable request ID; omitted/null generates one. No automatic retry of an uncertain mutation.
        """
        return reflected_result(actor.spawn_blueprint_actor(blueprint_name, actor_name, location_json, rotation_json, scale_json, expected_level_path, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def delete_actor(name: str, expected_level_path: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Delete an exact-name actor and independently verify absence. Initial scope: unattached instances of the five native spawn classes in a saved /Game non-WP persistent level.

        Args:
            name: Exact case-sensitive UObject actor name; missing/ambiguous actors fail. BP, template, child and attached actors are unsupported in this slice.
            expected_level_path: Optional exact persistent-level package path; omission pins the inspected level. No map save occurs.
            operation_id: Unique durable request ID; omitted/null generates one. No automatic retry or atomic rollback.
        """
        return reflected_result(actor.delete_actor(name, expected_level_path, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_actor_transform(name: str, location_json: str = "null", rotation_json: str = "null", scale_json: str = "null", expected_level_path: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Patch a native actor's world transform and read it back. Each JSON null preserves that component; all null is an inspected no-op. Same bounded level/class scope as delete_actor.

        Args:
            name: Exact case-sensitive UObject actor name; ambiguity fails. Only unattached instances of the five native spawn classes are supported.
            location_json: JSON string with three finite XYZ values, or the string null to preserve location. Magnitudes at most 1e12, text at most 512 UTF-16 units.
            rotation_json: JSON string with finite Pitch/Yaw/Roll degrees, or the string null to preserve rotation. Magnitudes at most 1e12, text at most 512 UTF-16 units.
            scale_json: JSON string with three finite XYZ values, or the string null to preserve scale. Magnitudes at most 1e12, text at most 512 UTF-16 units.
            expected_level_path: Optional exact saved /Game non-WP persistent-level package path; omission pins the inspected level. No map save occurs.
            operation_id: Unique durable ID; omitted/null generates one. Inspect partial/uncertain changes before a new request.
        """
        return reflected_result(actor.set_actor_transform(name, location_json, rotation_json, scale_json, expected_level_path, operation_id))
