"""Named PCG migration contracts on the existing official Toolset Registry."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import pcg_spec


@unreal.uclass()
class CubelessPCGTools(unreal.ToolsetDefinition):
    """Public runtime PCG content authoring; no generated Cubeless dependency."""

    @toolset_registry.tool_call
    @staticmethod
    def create_pcg_graph_from_spec(graph_spec_json: str, overwrite_existing: bool = False,
            allow_overwrite_non_temp: bool = False, save: bool = True, dry_run: bool = False,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Preserve the legacy one-spec PCG pipeline: normalize nodes/settings/edges, reject duplicate/reserved ids, perform complete transient graph/property/direct-edge preflight, then create exactly one unused /Game target and optionally save/audit. Existing registry/loaded/file/directory targets are never overwritten, including when either compatibility overwrite flag is True. No atomic rollback or automatic mutation retry. Nested IO is in data_json. Native runtime /Script/PCG settings classes require exact loaded UClass metadata admission before any transient graph allocation: Abstract, Deprecated and NewerVersionExists classes are rejected without construction; presence is not behavior proof. Vector/Rotator/Quat, public nested structs and PCGPoint arrays are supported. Attribute/property selectors retain the separate named selector tool. Exact native StaticMeshSpawner inline mesh selectors use the finite factory route below; generated settings classes and unknown fields remain incomplete. Maximum256 authored nodes/2048 physical edges. Factory input/output nodes are reported separately. Pins must be directly compatible and connectable; no converter nodes or occupied-input replacement. Settings scalars use only documented float-storage rounding tolerance. Save bool/file SHA/dependency outcome is recorded; construction is not CPU/GPU generation, Actor binding, cold reload or Shipping proof. Dry-run on an unused target executes the complete transient graph preflight through the same durable operation journal, then stops before the persistent asset factory, explicit notify/compile and save. A dry-run on an existing target reports blocked policy without a transient build. Transient attempted/performed/unknown outcomes and persistent factory dispatch are reported separately. Per-node, requested-property, edge and final-read checkpoints describe only the last durable boundary, never a later crash stage.

        Construction capability is separate from callable binding presence. Nonempty exact finite native profiles (CreatePointsGrid, CreatePoints, Distance, DensityRemap, DensityFilter, AttributeNoise, AttributeFiltering, GetActorProperty, RandomChoice, PointExtentsModifier, BoundsModifier and StaticMeshSpawner settings) use one scoped C++ JSON composite for transient preflight, new-target factory, requested property/direct-edge checks and one requested save with recursive audit. Exact /Script/PCG.PCGPointExtentsModifierSettings initial settings admit Extents and Mode; exact /Script/PCG.PCGBoundsModifierSettings admit Mode, BoundsMin, BoundsMax, bAffectSteepness and Steepness. Vectors require exactly three finite JSON numbers. Mode uses native enum string tokens (Extents: Set/Minimum/Maximum/Add/Multiply; Bounds: Set/Intersect/Include/Translate/Scale) through the existing canonical converter, not numeric enums. bAffectSteepness requires JSON boolean; Steepness requires a finite float32-range number, without an automatic metadata clamp. Native FName field lookup is retained without new snake aliases. The factory checks actual requested settings and final typed readback within captured source/layout/ownership/topology scope; it does not certify every inherited property value. Exact PCGStaticMeshSpawnerSettings additionally uses its existing bounded nine general settings fields and stock Weighted/ByAttribute selector profile. Node inline mesh_entries, static_mesh_entries, spawner_mesh_entries and mesh_selector are preserved without Python rewriting of null, empty or unselected aliases. Native retains the admitted inline sequence: requested by_attribute/attribute configuration first, then Weighted when the first truthy mesh_entries/static_mesh_entries/spawner_mesh_entries/mesh_selector.entries exists. Empty entries alone do not invoke the standalone weighted-empty refusal. Known-current input errors are retained in phase order while Weighted continues; the earliest original error still fails scratch preflight and prevents persistent creation. Unknown SDK/source/identity outcomes stop immediately. Material-attribute names accept arrays, Unicode string characters and dictionary keys in received iteration order; explicit null is an iterable error, not an empty-array success. The nine general fields retain existing JSON Boolean, bounded numeric Seed-to-int32 truncation and FName admission rather than full legacy coercion. Numeric/container-to-FName string-representation parity remains incomplete. Inline attribute_name omission differs from explicit empty/null; weight integer and optional Boolean descriptor coercions remain Native-owned with actual errors and ordered readback. Only admitted already-loaded StaticMesh/material references are supported; the original loader, arbitrary settings/descriptor fields, custom classes and broader selector variants remain incomplete. Scratch errors prevent persistent creation, while later failure/save/unknown prefixes remain actual and do not imply rollback. Other profiles retain pcg_construction_blocked_python_gc; arbitrary loaded-graph construction and unrelated selector gates remain closed. This route does not establish GC root cause, runtime generation or full original parity. No disabled-GC workaround, overwrite or automatic fallback is used.

        Args:
            graph_spec_json: Internal reflected JSON-text carrier for the official MCP graph_spec object. The existing raw Registry binding keeps graph_spec as a required object, then serializes it only for this domain boundary. The original five input names/defaults remain; the existing optional operation_id is the current durable-journal extension. The spec contains asset_path or graph_path, nodes[{id,settings_class or class,title,position or node_position,settings,mesh_entries,static_mesh_entries,spawner_mesh_entries,mesh_selector}], edges[{from or from_node,from_pin,to or to_node,to_pin}] and optional forbidden_dependency_prefixes.
            overwrite_existing: Wire compatibility only; an existing target is rejected.
            allow_overwrite_non_temp: Wire compatibility only; never enables replacement.
            save: DefaultTrue; one explicit graph save after a successful complete build. Required for forbidden dependency audit.
            dry_run: DefaultFalse; True validates the full transient spec for an unused target, without persistent asset creation, explicit notify/compile or save. Existing targets remain untouched and report blocked policy.
            operation_id: Optional durable request identity. Replays retrieve the recorded result; incomplete identities are blocked.
        """
        return reflected_result(pcg_spec.create_pcg_graph_from_spec(graph_spec_json, overwrite_existing,
            allow_overwrite_non_temp, save, dry_run, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_pcg_attribute_selector(graph_path: str, node_id: str, selector_property_name: str,
            selector_type: str = "attribute", attribute_name: str | None = None, selected_property_name: str | None = None,
            domain_name: str | None = None, point_property: str | None = None, extra_property: str | None = None,
            extra_names_json: str | None = None, reset_extra_names: bool = True,
            save: bool = True, dry_run: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Preserve the legacy typed selector request through public UE58 copy ScriptMethods, exact hidden native-copy preview/IsValid/IsSame observation and topology readback. Never write protected selector identity/accessor fields. Native preview changes a value copy only. Public assignment is tested on a transient copied setting before asset dispatch. Explicit accessor arrays use the registered official PCGToolset native JSON converter via public ToolsetLibrary, not ServerRPC/ExecuteTool; a DSL round-trip that cannot represent the exact requested state fails before asset mutation. Main type/domain/accessor preservation/reset and bool changed are observed. False native cache/setter change is not an error. Dry-run returns exact native preview without asset assignment/save. No retry/rollback claim. Full inputs/results and actual property/struct/node identities are in data_json; Engine5.8 method presence alone is not execution proof.

        Args:
            graph_path: Exact /Game package or matching object/export-text/.uasset path; fuzzy asset-name lookup is incomplete.
            node_id: Unique exact node path/name/title in this loaded graph.
            selector_property_name: Exact selector field or its snake_case spelling, e.g.OutputTarget/output_target. Must be one editable native selector-derived scalar.
            selector_type: attribute, property, point_property (aliaspoint), or extra_property (aliasextra).
            attribute_name: Optional attribute name; omitted/null becomes the legacy empty string, which retains native None semantics.
            selected_property_name: Optional UE property name; omitted/null becomes the legacy empty string.
            domain_name: Optional nonempty metadata domain; omitted/null becomes empty and preserves the legacy omitted-field behavior.
            point_property: Optional EPCGPointProperties token, e.g.Density; omitted/null becomes the legacy empty string.
            extra_property: Optional EPCGExtraProperties token, e.g.Index; omitted/null becomes the legacy empty string.
            extra_names_json: Optional JSON array of accessor strings. Omitted/null preserves setter reset/preserve behavior; [] explicitly clears. Unrepresentable canonical DSL accessors fail before dispatch.
            reset_extra_names: DefaultTrue; False preserves prior accessors when changing main selector.
            save: DefaultTrue; one explicit graph save following successful selector readback.
            dry_run: DefaultFalse; True previews without mutation or save.
            operation_id: Optional durable request identity; no automatic mutation retry.
        """
        return reflected_result(pcg_spec.set_pcg_attribute_selector(graph_path, node_id, selector_property_name,
            selector_type, "" if attribute_name is None else attribute_name,
            "" if selected_property_name is None else selected_property_name,
            "" if domain_name is None else domain_name,
            "" if point_property is None else point_property,
            "" if extra_property is None else extra_property,
            extra_names_json, reset_extra_names, save, dry_run, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def compile_or_notify_pcg_graph(graph_path: str, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Preserve the exact legacy Structural ForceNotificationForEditor then UPCGGraph.Recompile request on one exact loaded native graph. Uses an Editor-only hidden public-API wrapper because Recompile is exported but not reflected. Returns notified and the exact cache-change bool in data_json; False is not compiler failure, True also occurs when no active graph compiler exists. One explicit notification/recompile request, no save; implicit invalidations from earlier setters are not counted as these calls. Verifies unchanged loaded topology and unrelated dirty state. No GPU/Shader/Blueprint compilation or generation success is claimed.

        Args:
            graph_path: Exact /Game PCGGraph package/object path; asset load observation is reported.
            operation_id: Optional durable request identity; interrupted identities are blocked.
        """
        return reflected_result(pcg_spec.compile_or_notify_pcg_graph(graph_path, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def save_pcg_graph(graph_path: str, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Save exactly one loaded native PCGGraph using public SaveLoadedAsset with only_if_is_dirty=False, preserving legacy explicit-save behavior. Report actual return, exact disk file SHA, graph topology and recursive runtime-tool dependency audit. False/exception never implies rollback or no disk change. No implicit compilation, reload/generation/packaging or automatic retry.

        Args:
            graph_path: Exact /Game PCGGraph package/object path; asset load is separately observed.
            operation_id: Optional durable request identity; replay returns recorded outcome.
        """
        return reflected_result(pcg_spec.save_pcg_graph(graph_path, operation_id))
