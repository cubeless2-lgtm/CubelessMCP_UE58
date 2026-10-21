"""Original animation metadata observer17 on the existing synchronous standard11 result."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import animation_asset_metadata


@unreal.uclass()
class CubelessAnimationAssetMetadataTools(unreal.ToolsetDefinition):
    """Authored animation metadata observations. No world tick, notify-handler invocation, direct Modify, compiler or save. Asset loads/PostLoad and notify-name caches may have effects. The const preview getter deliberately avoids the original incompatible-preview clearing mutation; compatibility is reported without changing the authored preview. Static results do not prove animation playback, cold persistence, cook or complete original workflows."""
    @toolset_registry.tool_call
    @staticmethod
    def inspect_animation_asset_metadata_guarded(asset_path: str,
            include_notifies: bool = True,
            include_notify_details: bool = True,
            include_curves: bool = True,
            include_curve_keys: bool = False,
            include_sync_markers: bool = True,
            include_montage_sections: bool = True,
            include_montage_slots: bool = True,
            include_metadata: bool = False,
            max_notifies: int = 256,
            max_curves: int = 256,
            max_curve_keys_per_curve: int = 0,
            max_markers: int = 256,
            max_sections: int = 128,
            max_slots: int = 64,
            max_segments_per_slot: int = 256,
            require_supported_class: bool = False) -> CubelessPythonOperationResult:
        """Original notify/details, float and transform curves, optional bounded keys, sync markers, Montage sections/slots/segments and metadata summaries. Native owns path loading, zero-based clamps and source-admission bounds. Output flags do not erase independent sequence counts, branching indices or Montage totals. Unsupported assets with require_supported_class=False remain completed diagnostics even with legacy errors[]; True refuses them. Full nested arrays/counts/limits/truncation/warnings/errors/unavailable fields and Native failure prefix are retained. No additional getter, compiler, save or retry is issued by this adapter."""
        return reflected_result(animation_asset_metadata.inspect_animation_asset_metadata_guarded(
            asset_path,
            include_notifies,
            include_notify_details,
            include_curves,
            include_curve_keys,
            include_sync_markers,
            include_montage_sections,
            include_montage_slots,
            include_metadata,
            max_notifies,
            max_curves,
            max_curve_keys_per_curve,
            max_markers,
            max_sections,
            max_slots,
            max_segments_per_slot,
            require_supported_class))
