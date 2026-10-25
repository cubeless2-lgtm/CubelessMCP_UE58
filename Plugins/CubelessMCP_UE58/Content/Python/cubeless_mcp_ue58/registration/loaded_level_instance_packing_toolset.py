"""Official Registry definitions for explicit existing actor packing."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from ..operations import loaded_level_instance_packing as packing


@unreal.uclass()
class CubelessLoadedLevelInstancePackingTools(unreal.ToolsetDefinition):
    """Pack a loaded LevelInstance into an explicitly identified existing native PLA."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_loaded_level_instance_packing(source_actor_path: str,
            target_actor_path: str) -> CubelessPythonOperationResult:
        """Preflight exact actor object paths in the current Editor world. Does not load instances, alter filters, create actors, pack or save. Reports replacement scope, native placement formula, unsupported components and save availability. Native PLA target, flat clean fully loaded nonpartitioned source, matching nonempty WorldAsset and empty filters only; no Blueprint target, nested instances or active LevelInstance editing/PIE/SIE. Initial bounds:512 source actors,4096 components,2048 mesh instances. Not a source-deletion safety assessment."""
        return reflected_result(packing.inspect_loaded_level_instance_packing(source_actor_path, target_actor_path))

    @toolset_registry.tool_call
    @staticmethod
    def pack_loaded_level_instance(source_actor_path: str, target_actor_path: str,
            operation_id: str, dry_run: bool = True, allow_replace: bool = False,
            save: bool = False) -> CubelessPythonOperationResult:
        """Pack an ALREADY LOADED LevelInstance into one EXISTING native PackedLevelActor by exact actor paths. Default dry_run=true only previews. Apply requires dry_run=false; existing target mesh/packed data also requires allow_replace=true. Native PackActor removes old packed components before work; there is NO atomic rollback. Does not create/load/delete source or target, change WorldAsset/filter/actor transform, or publish a Blueprint. Geometry placement follows UE58 streaming transform/target transform/pivot formula, NOT a simple source-world copy; inspect placement receipts. Unsupported components/native packing warnings produce an incomplete result even if engine bool=true; changes may already exist. Native-only target, flat clean loaded nonpartitioned source, matching nonempty WorldAsset, empty filters, no editing/PIE/SIE; bounds512 actors/4096 components/2048 instances. save=false by default. save=true is allowed ONLY for an exclusive external target actor package; ordinary map actors are refused before mutation and require a separate explicit map save. No source or user-world autosave. Inspect native_outcome, before/after, placement, unsupported components, actual changes and save receipts on any failure. Fresh operation_id for new work; a reused ID returns its completed journal, never retries uncertain work. Success never authorizes source deletion or proves gameplay equivalence."""
        return reflected_result(packing.pack_loaded_level_instance(source_actor_path, target_actor_path,
            operation_id, dry_run, allow_replace, save))
