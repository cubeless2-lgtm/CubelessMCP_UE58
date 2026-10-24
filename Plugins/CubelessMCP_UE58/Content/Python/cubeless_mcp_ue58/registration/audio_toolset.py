"""Four original Audio names through finite public Engine authoring APIs."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import audio


@unreal.uclass()
class CubelessAudioTools(unreal.ToolsetDefinition):
    """Audio metadata, scoped native inspection, one Wave-player SoundCue and one finite continuous MetaSound builder. SourceControl-enabled authoring and full audio playback/cold/cooked parity remain incomplete."""

    @toolset_registry.tool_call
    @staticmethod
    def list_audio_assets(directory: str = "/Game", recursive: bool = True,
            max_results: int = 200, offset: int = 0) -> CubelessPythonOperationResult:
        """List original SoundWave, SoundCue and MetaSoundSource metadata, including descendants through the public AssetRegistry recursive class filter. Rows retain name, asset_path, class and package_path. Total is observed before original pagination; paths sort with Unreal's ASCII case-insensitive TCHAR rule. No asset load, registry rescan or dependency audit is requested. Registry may include loaded unsaved assets, and implicit metadata/wrapping effects are unmeasured.

        Args:
            directory: Original /Game default; normalize native whitespace, double-quote endpoints and slash/trailing slash. Admit original /Game or /Engine long package paths; MCP-only tokens reject.
            recursive: Original True default; exact bool controls recursive package paths. Recursive classes always remain enabled.
            max_results: Original 200 default; signed-int32 integer clamps to 1..1000. Historical Python arbitrary int coercion is not exposed at reflection.
            offset: Original 0 default; signed-int32 integer clamps to at least zero. Return next_offset only when has_more is True.
        """
        return reflected_result(audio.list_audio_assets(directory, recursive, max_results, offset))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_audio_asset(asset_path: str) -> CubelessPythonOperationResult:
        """The bounded inherited native profile follows the original MetaSoundSource class-chain, SoundCue IsA, then SoundWave IsA precedence. It retains at most 64 native runtime classes and actual source/owner roles; the original duration getter is called once and context/target/class state is checked immediately after it before further SDK observation. Nullable existing CDO and repeated payload snapshots are post-duration observations. Virtual getter side effects and global callback purity remain unknown; no whole-call zero-effect guarantee is added. New native-subclass usage validation is pending. Inspect one finite native SoundWave, SoundCue or conditional MetaSoundSource through the named EngineAPI helper. Original identity/kind/duration and applicable Wave/Cue fields are retained. Public resolution can load/PostLoad; SoundCue duration can refresh aggregate caches. Native repeated preservation observations start after initial resolution/duration, so no whole-call zero-effect claim is made. No explicit asset writer, compile, save, playback or fallback is requested. Generated sources, unadmitted native class/source profiles, arbitrary virtual getter effects and full MetaSound authoring remain incomplete.

        Args:
            asset_path: Original required path; native normalizes and resolves its admitted /Game or /Engine native audio asset and reports actual load/getter scope.
        """
        return reflected_result(audio.inspect_audio_asset(asset_path))

    @toolset_registry.tool_call
    @staticmethod
    def create_sound_cue_from_wave(name: str, sound_wave_path: str,
            package_path: str = "/Game/Audio/SoundCues") -> CubelessPythonOperationResult:
        'Persisted admitted native SoundWave subclasses additionally reuse the existing source hierarchy/current-state observation around the original factory, WavePlayer assignment, linking, PostEditChange and single save. Source virtual SDK effects are not globally certified absent. Subclass creation usage is implemented / usage validation pending. Create the original exact SoundCue with one SoundNodeWavePlayer referencing a persisted native SoundWave. Public factory, graph linking and PostEditChange perform their original Editor lifecycle before one forced exact save. Existing targets are never overwritten: only an exact clean persisted matching graph is accepted as a no-op. Original legacy data.saved=True for that no-op is separate from actual top save flags/counters, which remain zero. Full returned allocation/save/readback prefixes survive errors; no automatic action retry, cleanup, rollback or Blueprint compilation occurs. One internal generated durable journal ID adds no public input. Playback, cold reload, cooked audio and basic MetaSound builder behavior are not proved.\n\n        Args:\n            name: Original required simple asset name; native trims/validates the target before allocation.\n            sound_wave_path: Original required persisted native SoundWave source; native resolution/load and dependency scope are explicit.\n            package_path: Original /Game/Audio/SoundCues default; native normalizes and admits a project target package root. There is no public save, timeout or operation_id input.\n        '
        return reflected_result(audio.create_sound_cue_from_wave(name, sound_wave_path, package_path))

    @toolset_registry.tool_call
    @staticmethod
    def create_basic_metasound_source(name: str, package_path: str = "/Game/Audio/MetaSounds",
            oscillator: str = "sine", frequency: float = 440.0,
            output_format: str = "mono") -> CubelessPythonOperationResult:
        """Create the original continuous MetaSound using its public source builder and Editor BuildToAsset lifecycle. Native preserves the exact generator/literal/format/interfaces/channels/graph readback and one forced exact save. An existing clean persisted matching document succeeds with actual save0, while original legacy data.saved=True stays separate. SourceControl-enabled creation rejects before factory/builder dispatch in this first finite scope; disabled SourceControl still permits disclosed BuildToAsset registry/template/possible implicit work. Actual builder/allocation/save prefixes survive errors, no overwrite, cleanup or action retry is performed, and internal generated journaling adds no public argument. Custom/preset/subgraph variants, audio rendering/playback and packaged execution are not proved.

        Args:
            name: Original required simple Unreal asset name, validated by native before allocation.
            package_path: Original /Game/Audio/MetaSounds default; native normalizes and admits the project target root.
            oscillator: Original sine default; native trim/lowercase admits sine, saw, square, triangle or noise, with the original exact class name/version.
            frequency: Original 440.0 default, finite 1..20000 Hz even for noise. Noise retains requested frequency in results but applies no frequency literal; other generators use the original float literal and readback tolerance.
            output_format: Original mono default; native trim/lowercase admits mono or stereo and verifies the requested channels/interfaces. There is no public save, timeout or operation_id input.
        """
        return reflected_result(audio.create_basic_metasound_source(name, package_path,
            oscillator, frequency, output_format))
