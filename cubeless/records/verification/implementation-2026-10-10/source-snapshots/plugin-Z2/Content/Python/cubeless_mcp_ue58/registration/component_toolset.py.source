"""Named local-SCS inspection and mutation contracts; proof is separate."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import component


@unreal.uclass()
class CubelessComponentTools(unreal.ToolsetDefinition):
    """Cubeless bounded local Blueprint SCS inspection and guarded authoring. Inherited effective overrides and native CDO components are excluded. Loading may run PostLoad; mutations never request saving."""

    @toolset_registry.tool_call
    @staticmethod
    def list_blueprint_components(blueprint_name: str, component_name: str | None = None, max_components: int = 10000) -> CubelessPythonOperationResult:
        """List reachable local SCS components, class/template paths, parent name, relative transform and static mesh. Sorted by variable name; exact missing filter returns an empty success. No inherited/native/orphan completeness claim.

        Args:
            blueprint_name: Exact Blueprint package/object path, or simple name under /Game/Blueprints. Registry-wide short-name search, generated-class/export/LevelBlueprint aliases remain unsupported. Selectors at most 1023 UTF-16 units without controls.
            component_name: Optional exact case-sensitive local variable name. Omitted/null/empty lists all reachable local nodes. Filters do not bypass traversal validation or its budget.
            max_components: Total reachable-tree traversal cap, integer 1..10000; default 10000. Exceeding the cap fails instead of returning a partial success.
        """
        return reflected_result(component.list_blueprint_components(blueprint_name, component_name, max_components))

    @toolset_registry.tool_call
    @staticmethod
    def get_component_property(blueprint_name: str, component_name: str, property_name: str, max_components: int = 10000, max_value_chars: int = 65536) -> CubelessPythonOperationResult:
        """Read a local SCS component-template property with legacy typed JSON/scalar-array text encoding. Supported: bool, finite exact numeric values up to 2^53-1, string/name/text/enum, FVector/FRotator/FLinearColor and object paths. Scalar arrays remain Unreal text strings; arbitrary structs/maps/sets/delegates and fixed native arrays are unsupported. Stable dirty/status observations do not independently verify values or disk persistence.

        Args:
            blueprint_name: Exact Blueprint package/object path or simple name under /Game/Blueprints; other legacy aliases remain unsupported. No asset is preloaded by the adapter.
            component_name: Exact case-sensitive reachable local SCS variable name; nonempty and at most 1023 UTF-16 units, no controls.
            property_name: Reflected property name, case-insensitive as legacy FName lookup; nonempty and at most 1023 UTF-16 units, no controls. Missing/unsupported values fail explicitly.
            max_components: Whole local-tree traversal cap, integer 1..10000, default 10000; inherited/native CDO/orphan nodes excluded.
            max_value_chars: UTF-16 property-text budget, integer 64..65536, default 65536. Arrays have at most 256 scalar/name/string/enum elements and a conservative escaped-text bound; some valid long arrays are rejected.
        """
        return reflected_result(component.get_component_property(blueprint_name, component_name, property_name, max_components, max_value_chars))

    @toolset_registry.tool_call
    @staticmethod
    def add_component_to_blueprint(blueprint_name: str, component_type: str, component_name: str, location_json: str = "[0,0,0]", rotation_json: str = "[0,0,0]", scale_json: str = "[1,1,1]", parent_component: str | None = None, component_properties_json: str = "{}", compile_blueprint: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Add one local SCS component with validated initial properties and observed hierarchy. Supports a runtime-safe /Game Actor Blueprint, seven exact Engine classes, and an audited saved clean compiled non-scene ActorComponent generated class. Inherited/native templates, arbitrary plugin classes and external property propagation remain unsupported. Never saves. Unlike legacy's ignored component_properties, unsupported values fail explicitly.

        Args:
            blueprint_name: Exact /Game Blueprint package/object path or simple name under /Game/Blueprints. Selectors are at most 1023 UTF-16 units without controls; native name/template-slot guards can be stricter.
            component_type: Runtime Engine SceneComponent, StaticMeshComponent, CameraComponent, SpringArmComponent, SphereComponent, BoxComponent or CapsuleComponent; U-prefix, /Script/Engine and short base-name aliases are preserved. Alternatively an exact /Game/Package.Package_C non-scene ActorComponent class with saved clean compiled source and audited runtime dependencies. Generated components require identity transforms, no parent and {} initial properties; saved defaults are retained. Class loading/PostLoad observations and one explicit skeleton refresh are reported separately from optional full compilation.
            component_name: Requested exact new local component variable name: ASCII identifier with 1..100 characters, not None. Existing/conflicting names fail without silently generating a different identity.
            location_json: JSON string [X,Y,Z], default [0,0,0]. Exactly three finite numbers, not booleans; magnitude at most 1e12.
            rotation_json: JSON string [Pitch,Yaw,Roll], default [0,0,0], with the same numeric bounds.
            scale_json: JSON string [X,Y,Z], default [1,1,1], with the same numeric bounds.
            parent_component: Optional exact existing local scene parent for native classes. Omitted/null/empty uses the sole local scene root, typically DefaultSceneRoot. Multiple roots require an explicit parent; an empty valid SCS may create its first root. Generated non-scene components require omission/null/empty and are added as local SCS root nodes.
            component_properties_json: Explicit JSON-object adapter for legacy heterogeneous properties, default {}. At most 64 fields. Native typed scalar/vector/rotator/color/enum/name/string/text subset only; arbitrary aggregates/object/class references unsupported except checked StaticMeshComponent.StaticMesh. Shape fields: SphereRadius, BoxExtent (three numbers or broadcast number), CapsuleRadius and CapsuleHalfHeight; sizes must be finite, nonnegative and at most 1e12, with capsule radius <= half-height. Both capsule fields may be supplied together. Use the transform arguments for RelativeLocation/RelativeRotation/RelativeScale3D; conflicting property fields fail. JSON text at most 65536 UTF-16 units, depth 8 and 1024 values; duplicate keys, nonfinite/numeric magnitude beyond 2^53-1, NUL and unpaired surrogates fail before dispatch. Property names at most 1023 UTF-16 units.
            compile_blueprint: True preserves legacy's full compile boundary with diagnostics and SkipSave. False explicitly defers full compile for authoring batches; implicit structural engine work is separately reported. Finish the batch with one named compile action.
            operation_id: Optional durable ID; omitted/null generates a returned ID. Same ID and normalized arguments replay its recorded result. Interrupted or uncertain operations never automatically retry.
        """
        return reflected_result(component.add_component_to_blueprint(blueprint_name, component_type, component_name,
            location_json, rotation_json, scale_json, parent_component, component_properties_json, compile_blueprint, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_component_property(blueprint_name: str, component_name: str, property_name: str, property_value_json: str, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Set one permitted property of a reachable local SCS template, with typed prevalidation and native before/after observations. No full compile or save is requested; native structural/notification effects are separately reported. Equal values can return a verified no-op. Unsupported/inherited/native components and external propagation fail explicitly.

        Args:
            blueprint_name: Exact /Game Blueprint package/object path or simple name under /Game/Blueprints. Supported component classes are SceneComponent, StaticMeshComponent, CameraComponent, SpringArmComponent, SphereComponent, BoxComponent and CapsuleComponent.
            component_name: Exact local SCS variable name, at most 1023 UTF-16 units without controls.
            property_name: Native reflected property name, at most 1023 UTF-16 units without controls. Shape sizes must be nonnegative and at most 1e12; a CapsuleRadius/CapsuleHalfHeight edit must preserve radius <= half-height without silently changing its companion field. Read-only/internal/unsupported properties fail; no raw aggregate ImportText fallback.
            property_value_json: Explicit JSON-string adapter for legacy Any. Preserve JSON scalar types. FVector accepts [X,Y,Z] or one broadcast number; FRotator requires [Pitch,Yaw,Roll]; FLinearColor requires [R,G,B,A]; these values have magnitude at most 1e12. Enums accept a declared name or value. StaticMeshComponent.StaticMesh accepts a checked /Engine or /Game StaticMesh path encoded as a JSON string, or null to clear. Other UObject/class assignments are unsupported. Maximum 65536 UTF-16 units, depth 8 and 1024 values; duplicate/nonfinite/numeric magnitude beyond 2^53-1/NUL/surrogate inputs fail before dispatch.
            operation_id: Optional durable ID; omitted/null generates a returned ID. Replays return historical outcomes; unknown results are not automatically retried.
        """
        return reflected_result(component.set_component_property(blueprint_name, component_name, property_name, property_value_json, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_static_mesh_properties(blueprint_name: str, component_name: str, static_mesh: str = "/Engine/BasicShapes/Cube.Cube", operation_id: str | None = None, material: str | None = None) -> CubelessPythonOperationResult:
        """Set the mesh and/or material slot 0 of an exact native StaticMeshComponent in the Blueprint's reachable local SCS. Preserves the legacy named tool, Cube default, positional operation_id and data.component result while restoring the old native material branch. Both requested references are prevalidated before editing; invalid references fail explicitly instead of silent success. No full compile or save is requested. Supported classes/dependencies/propagation remain bounded, not full internal-command parity.

        Args:
            blueprint_name: Exact /Game Blueprint package/object path or simple name under /Game/Blueprints. Inherited/native CDO templates, custom component subclasses and external instance propagation are unsupported.
            component_name: Exact reachable local component variable name, at most 1023 UTF-16 units without controls. A non-StaticMeshComponent fails explicitly.
            static_mesh: /Engine or /Game StaticMesh package/object path, default /Engine/BasicShapes/Cube.Cube. For material-only edits, explicitly pass an empty string AND a valid material path. An empty mesh without a material remains invalid. Omission selects Cube even when material is supplied; null is not a leave-unchanged sentinel because the official conversion can apply the Cube default. Complete paths are at most 1023 UTF-16 units. General set_component_property with JSON null is a distinct mesh-clear operation.
            operation_id: Optional durable ID; omitted/null generates a returned ID. Its positional location is preserved. Mesh-only normalized arguments and existing journal replay remain unchanged. Replay never executes again; unknown results are not automatically retried.
            material: Optional /Engine or /Game material package/object path to assign slot 0; omitted/null leaves materials unchanged. Empty/malformed paths fail and do not clear. Complete paths are at most 1023 UTF-16 units. Native runtime class, dependency and slot readback guards apply; unsupported material subclasses or external archetypes fail. Supply static_mesh="" explicitly for material-only edits. This argument follows operation_id to preserve existing positional calls.

        Returns:
            Standard mutation outcome plus data.component, requested_static_mesh, material_requested, optional requested_material and native observations. legacy_internal_material_supported=true is scoped to the guarded slot-0 branch; full_legacy_command_parity=false retains the stricter validation/type/error differences. No atomic rollback is promised.
        """
        return reflected_result(component.set_static_mesh_properties(blueprint_name, component_name, static_mesh, operation_id, material))

    @toolset_registry.tool_call
    @staticmethod
    def set_physics_properties(blueprint_name: str, component_name: str, simulate_physics: bool = True, gravity_enabled: bool = True, mass: float = 1.0, linear_damping: float = 0.01, angular_damping: float = 0.0, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Set the legacy named physics-default group on one local unregistered component template. Supports exact Engine StaticMeshComponent, SphereComponent, BoxComponent and CapsuleComponent only. Uses public physics setters and independently observes stored defaults; no physics simulation, full compile or save is requested or verified. Inherited/native templates, external propagation and live physics state are unsupported. Corrects the old handler's ignored gravity_enabled input.

        Args:
            blueprint_name: Exact /Game Blueprint package/object path or simple name under /Game/Blueprints. Selectors are at most 1023 UTF-16 units without controls.
            component_name: Exact reachable local SCS variable name; unsupported component types fail before requested editing.
            simulate_physics: Boolean, default True. Sets the template simulation default, not a running simulation.
            gravity_enabled: Boolean, default True. Applied through the public gravity setter; legacy_gravity_ignored_fixed records this intentional behavior correction.
            mass: Kilogram override, default 1.0. Finite number from 0.001 to 1e12; booleans/strings/null are rejected. The lower bound follows the public BodyInstance editor mass constraint and is stricter than legacy. Native storage is float32 and reports its actual rounded value.
            linear_damping: Finite number from 0 to 1e12, default 0.01; bool/string/null rejected. Same float32 observation rule as mass.
            angular_damping: Finite number from 0 to 1e12, default 0.0; bool/string/null rejected. Same float32 observation rule as mass.
            operation_id: Optional durable ID; omitted/null generates a returned ID. One journal and action core are shared by standalone and named batch use. Replays never execute again; partial or unknown outcomes require inspection and are never automatically retried.

        Returns:
            Standard mutation outcome retaining data.component, physics_before/physics_after, legacy_gravity_ignored_fixed and runtime_simulation_verified=false. Actual changes and no-op results remain explicit. No atomic rollback is promised.
        """
        return reflected_result(component.set_physics_properties(blueprint_name, component_name, simulate_physics,
            gravity_enabled, mass, linear_damping, angular_damping, operation_id))
