"""Missing animation-asset editors on the official Toolset Registry."""
import unreal
import toolset_registry
from cubeless_mcp_ue58.operations import animation_editing as domain


@unreal.uclass()
class CubelessAnimationEditingTools(unreal.ToolsetDefinition):
    """Editor-only Sequence/Montage edits. Explicit asset paths; default preflight and no save. Exact fully loaded AnimSequence/AnimMontage without parent or loaded children; reject PIE/SIE, GC, saving and exit. Preserve skeleton, bone tracks, authored slot/segment references and unrelated checked values across callbacks. If this API acquires an unloaded Montage, pre-PostLoad link values are unobserved: save=true is refused for that live object on cold and subsequent warm calls, including after preview/save=false or adapter reload. Inspection or dirty approval does not clear this guard. Acquisition exceptions also retain pending uncertainty. Known unresolved P2: if redirector acquisition fails before actual target identity is recorded, a later save via the destination path can evade the guard; do not save that destination after this failure. This is not retrospective disk verification for assets loaded/modified outside this API. Montage save=true is also refused if any nondefault Notify start/State end slot survives the edit: stock PostLoad re-links to slot0. save=false keeps live edits with persistence warnings; manual save/reload can lose links. No atomic rollback/automatic retry or compression/runtime-ready proof. Max4096 notifies/markers/segments,256 tracks,64 slots,1024 curves/sections,65536 float keys,16MiB snapshot. Reuse existing metadata and official rig/Sequencer tools."""

    @toolset_registry.tool_call
    @staticmethod
    def add_animation_float_curve(asset_path: str, curve_name: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Add an unused float curve to an AnimSequence using its data controller. Does not edit skeleton curve metadata. execute=false validates only; save=false leaves edits dirty."""
        return domain.edit('add_float_curve', asset_path, {'curve_name': curve_name}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def remove_animation_float_curve(asset_path: str, curve_name: str, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Remove exactly one existing float curve, preserving skeleton names and other curves. Actual removal requires allow_remove=true and execute=true."""
        return domain.edit('remove_float_curve', asset_path, {'curve_name': curve_name}, execute, save, allow_save_dirty_package, allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def set_animation_float_curve_keys(asset_path: str, curve_name: str, keys_json: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Replace all keys of an existing float curve with JSON [[time_seconds,value],...]. Strictly increasing times within sequence; max16384 keys/request and65536 total. Empty list clears keys. Linear default keys; concrete AnimSequencer controller quantizes to model-rate or240000fps and generates tangents. Colliding channel frames are refused before editing. No merge or preservation of old tangents."""
        return domain.edit('set_float_curve_keys', asset_path, {'curve_name': curve_name, 'keys_json': keys_json}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def add_animation_notify_track(asset_path: str, track_name: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Add an unused white Notify track to a Sequence or Montage. Does not create notifies or sync markers."""
        return domain.edit('add_notify_track', asset_path, {'track_name': track_name}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def remove_animation_notify_track(asset_path: str, track_name: str, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Remove an empty non-final Notify track; update later Notify/marker track indices. A track containing events or markers is refused before Modify; remove its contents explicitly. No cascading deletion."""
        return domain.edit('remove_notify_track', asset_path, {'track_name': track_name}, execute, save, allow_save_dirty_package, allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def add_animation_notify(asset_path: str, track_name: str, time: float, notify_class: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Add one default-valued instance of an already-loaded concrete UAnimNotify class at seconds; returns new notify_guid. Canonical top-level /Package.Class only; complete package and class name each below1024 UTF16 units, no subobject/export-text syntax. Explicit track/class, no UI. Requires class/instance CanBePlaced admission. Freeze CDO defaults before callbacks; constructor/name/placement errors stop further writes/save. No class/default/source editing or atomic rollback."""
        return domain.edit('add_notify', asset_path, {'track_name': track_name, 'time': time, 'notify_class': notify_class}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def add_animation_notify_state(asset_path: str, track_name: str, time: float, duration: float, notify_class: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Add a default-valued already-loaded concrete UAnimNotifyState on an existing track; positive duration and end within animation. Canonical top-level /Package.Class only; complete package and class name each below1024 UTF16 units, no subobject/export-text syntax. Requires class/instance CanBePlaced; e.g. DisableRootMotion requires Montage. CDO/identity checks and callback errors guard writes/save. Returns exact notify_guid."""
        return domain.edit('add_notify_state', asset_path, {'track_name': track_name, 'time': time, 'duration': duration, 'notify_class': notify_class}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def remove_animation_notify(asset_path: str, notify_guid: str, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Remove exactly one event by GUID, preserving other events and their objects/properties. Requires explicit removal approval; no ambiguous notify-name matching."""
        return domain.edit('remove_notify', asset_path, {'notify_guid': notify_guid}, execute, save, allow_save_dirty_package, allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def set_animation_notify_timing(asset_path: str, notify_guid: str, time: float, duration: float = 0.0, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Move an exact Notify GUID and set duration: plain Notify must use0; NotifyState must use positive duration. Preserve event GUID, class, original owned object, start/end Montage slots and link methods. Relative/proportional destination must retain a segment in its existing slot. Seconds within animation; no implicit save."""
        return domain.edit('set_notify_timing', asset_path, {'notify_guid': notify_guid, 'time': time, 'duration': duration}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def add_animation_sync_marker(asset_path: str, marker_name: str, time: float, track_name: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Add one named sync marker to an AnimSequence on an existing Notify track. Repeated names at different times are supported; returns new marker_guid."""
        return domain.edit('add_sync_marker', asset_path, {'marker_name': marker_name, 'time': time, 'track_name': track_name}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def remove_animation_sync_markers(asset_path: str, marker_name: str, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Remove every sync marker with the exact FName, returning removed_count. Explicit allow_remove=true required; other names and Notify events preserved."""
        return domain.edit('remove_sync_markers', asset_path, {'marker_name': marker_name}, execute, save, allow_save_dirty_package, allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def rename_animation_sync_marker(asset_path: str, marker_name: str, new_name: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Rename all matching markers to a distinct unused FName, preserving GUIDs/times/tracks. Existing destination name is refused rather than merging groups."""
        return domain.edit('rename_sync_marker', asset_path, {'marker_name': marker_name, 'new_name': new_name}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def add_montage_section(asset_path: str, section_name: str, time: float, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Add a named section at seconds and sort sections by time. Preserve existing next-section links; new section terminates. Does not implicitly rewire the preceding section."""
        return domain.edit('add_montage_section', asset_path, {'section_name': section_name, 'time': time}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def remove_montage_section(asset_path: str, section_name: str, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Remove one section and clear inbound next-section references to it. Keep the last section and the section at time0. Animation segments/notifies remain; no source animation deletion."""
        return domain.edit('remove_montage_section', asset_path, {'section_name': section_name}, execute, save, allow_save_dirty_package, allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def set_montage_section_next(asset_path: str, section_name: str, next_section: str = '', execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Set a section's next-section link to an existing section, including self-loop, or empty string to terminate. No runtime montage instance or UI dependency."""
        return domain.edit('set_montage_section_next', asset_path, {'section_name': section_name, 'next_section': next_section}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def set_montage_section_time(asset_path: str, section_name: str, time: float, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Move a named section to seconds within montage and sort by time, preserving name-based next links. Does not move animation segments or Notify events."""
        return domain.edit('set_montage_section_time', asset_path, {'section_name': section_name, 'time': time}, execute, save, allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def add_montage_slot(asset_path: str, slot_name: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Add an empty Montage slot track using an existing Skeleton slot in the same group; max64 tracks. Does not create a Skeleton slot or populate animation segments."""
        return domain.edit('add_montage_slot', asset_path, {'slot_name': slot_name}, execute, save, allow_save_dirty_package)
