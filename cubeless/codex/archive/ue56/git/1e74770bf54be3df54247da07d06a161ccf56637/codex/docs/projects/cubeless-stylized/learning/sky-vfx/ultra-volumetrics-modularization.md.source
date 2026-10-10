# Cubeless UltraVolumetrics Modularization

## Current Integration Model

Cubeless uses a thin-wrapper integration for UltraVolumetrics first.

- Production root: `/Game/Cubeless/VFX/Volumetrics`
- Reference dependency root: `/Game/UltraVolumetrics`
- Temporary research root: `/Game/_MCP_Temp/CubelessUltraVolumetricsModular`

The production pack intentionally depends on the original UltraVolumetrics package. The goal of this pass is fast, feature-oriented use inside Cubeless, not a full fork of every material function, Niagara module, and Blueprint implementation.

## Feature Entry Points

Use these Blueprint wrappers as the first authoring surface:

- Core local volume fog: `/Game/Cubeless/VFX/Volumetrics/Blueprints/BP_CL_UV_CoreVolume`
- Spline fog: `/Game/Cubeless/VFX/Volumetrics/Blueprints/BP_CL_UV_SplineFog`
- Ground fog: `/Game/Cubeless/VFX/Volumetrics/Blueprints/BP_CL_UV_GroundFog`
- Stamp/projected fog: `/Game/Cubeless/VFX/Volumetrics/Blueprints/BP_CL_UV_StampFog`
- Path traced volume variant: `/Game/Cubeless/VFX/Volumetrics/Blueprints/BP_CL_UV_PathTracedVolume`
- Interaction controller: `/Game/Cubeless/VFX/Volumetrics/Blueprints/BP_CL_UV_InteractionController`
- Trail component wrapper: `/Game/Cubeless/VFX/Volumetrics/Blueprints/BP_CL_UV_TrailComponent`

Ready-to-place module Blueprints live under:

- `/Game/Cubeless/VFX/Volumetrics/Blueprints/Modules`

Available actor modules:

- `BP_CL_UV_Module_Core_Static`
- `BP_CL_UV_Module_Core_Interactive`
- `BP_CL_UV_Module_Core_DistanceStatic`
- `BP_CL_UV_Module_Core_DistanceInteractive`
- `BP_CL_UV_Module_Ground_Static`
- `BP_CL_UV_Module_Ground_Interactive`
- `BP_CL_UV_Module_Stamp_Static`
- `BP_CL_UV_Module_Stamp_Interactive`
- `BP_CL_UV_Module_Spline_Static`
- `BP_CL_UV_Module_Spline_Interactive`
- `BP_CL_UV_Module_PathTraced`
- `BP_CL_UV_Module_InteractionController`

Use the module Blueprints for ordinary placement. Keep the base wrappers as neutral parent assets for future Cubeless variants and lower-level tuning.

## Material Variants

Primary material entry points live under:

- `/Game/Cubeless/VFX/Volumetrics/Materials`
- `/Game/Cubeless/VFX/Volumetrics/Materials/Variants`

The variants are grouped by intended feature:

- Core volume: static no-interaction, interactive, distance static, distance interactive
- Ground fog: static no-interaction, interactive, extended
- Stamp fog: static no-interaction, interactive, extended
- Interaction sprite: object, character, blocker

These are Material Instance wrappers over original UltraVolumetrics materials or material instances. Tune Cubeless-facing values in these instances first. Do not edit the original UltraVolumetrics materials unless a later deep-fork pass explicitly requires it.

## Niagara Effects

Feature-named Niagara systems live under:

- `/Game/Cubeless/VFX/Volumetrics/Niagara`

Available wrappers:

- `NS_CL_UV_Blocker`
- `NS_CL_UV_Burst`
- `NS_CL_UV_BurstLoop`
- `NS_CL_UV_Jump`
- `NS_CL_UV_Projectile2d`
- `NS_CL_UV_Ring`
- `NS_CL_UV_Swing`
- `NS_CL_UV_TrailCharacter`
- `NS_CL_UV_TrailObjects`
- `NS_CL_UV_Vortex`

Ready-to-place Niagara Actor wrappers live under:

- `/Game/Cubeless/VFX/Volumetrics/Blueprints/FX`

Available FX actors:

- `BP_CL_UV_FX_Blocker`
- `BP_CL_UV_FX_Burst`
- `BP_CL_UV_FX_BurstLoop`
- `BP_CL_UV_FX_Jump`
- `BP_CL_UV_FX_Projectile2d`
- `BP_CL_UV_FX_Ring`
- `BP_CL_UV_FX_Swing`
- `BP_CL_UV_FX_TrailCharacter`
- `BP_CL_UV_FX_TrailObjects`
- `BP_CL_UV_FX_Vortex`

These systems still reference the original UltraVolumetrics Niagara modules and sprite materials. That is acceptable for the thin-wrapper phase. If interaction behavior must be fully isolated later, duplicate and remap the Niagara modules and their parameter collection dependencies.

Important classification: treat these Niagara systems as interaction data/mask writer modules first, not as guaranteed final visible beauty VFX. Several systems participate in UltraVolumetrics' capture/render-target interaction path and may only produce meaningful output when `BP_CL_UV_InteractionController` and the original Niagara parameter collection state are driving a capture pass.

## Presets

Preset data assets live under:

- `/Game/Cubeless/VFX/Volumetrics/Presets`

The first pass preserves the original data shape and splits presets into default-style and distance-field-style starting points. Use these as Cubeless-facing preset copies for tuning. The preset parent class still comes from UltraVolumetrics.

## Shared Resources

Shared-resource candidates live under:

- `/Game/Cubeless/VFX/Volumetrics/SharedResources`

Included candidates:

- `MPC_CL_UV_Modular`
- `NMPC_CL_UV_Modular`
- `RT_CL_UV_Interaction`
- `RT_CL_UV_Ring`
- `RT_CL_UV_Paint`
- `T_CL_UV_RTBorder`
- `T_CL_UV_DistortionRGB`
- `T_CL_UV_PaintMaskPlaceholder`

These assets are prepared for future isolation, but the current wrapper Blueprints, materials, and Niagara systems are not fully remapped to them yet. Current interactive material variants still read the original UltraVolumetrics `T_RT` through the `RT-trail` texture parameter, so the original `BP_InteractionController` and the current thin-wrapper materials remain compatible.

## Usage Guidance

For simple static fog, start with `BP_CL_UV_Module_Core_Static`, `BP_CL_UV_Module_Ground_Static`, `BP_CL_UV_Module_Stamp_Static`, or `BP_CL_UV_Module_Spline_Static`.

For fog that must react to characters or objects, place the matching `*_Interactive` module and add `BP_CL_UV_Module_InteractionController` only in the needed area. The interactive modules default `Allow Interaction?` to `true`. Keep one interaction controller per overlapping feature group. When multiple controllers overlap, UltraVolumetrics' runtime auto-wiring may attach fog actors to an existing controller instead of the newly placed one. Interaction response will not be visible when the fog actor creates a `NoInteraction` material variant.

For distance-field local fog, start with `BP_CL_UV_Module_Core_DistanceStatic` or `BP_CL_UV_Module_Core_DistanceInteractive`.

For ground-hugging fog, use the ground modules. For projected local pockets, decals, or localized fog stamps, use the stamp modules.

For path-traced shots, use `BP_CL_UV_Module_PathTraced` as a separate authoring path instead of mixing it into the runtime wrapper set.

For individual interaction FX emitters, place the matching `BP_CL_UV_FX_*` actor. These wrappers are `NiagaraActor` children with their NiagaraComponent bound to the corresponding `NS_CL_UV_*` system and `auto_activate=true`.

For trails and character movement effects, use `BP_CL_UV_TrailComponent` with `NS_CL_UV_TrailCharacter` or `NS_CL_UV_TrailObjects`. A ready-to-place trail module was not kept because the trail wrapper did not spawn as a normal actor in the module placement smoke test.

## Validation Snapshot

The production root was promoted from the verified temp prototype and validated through UnrealMCP.

- Asset count: `81`
- Class counts: `20` MaterialInstanceConstant, `29` Blueprint, `14` BP_PresetDataAsset_C, `10` NiagaraSystem, `3` Texture2D, `3` TextureRenderTarget2D, `1` MaterialParameterCollection, `1` NiagaraParameterCollection
- Blueprint validation: all seven base wrappers, twelve ready-to-place fog/controller modules, and ten ready-to-place FX actors compiled and saved cleanly
- Module placement validation: all twelve ready-to-place modules spawned in the smoke map through `spawn_actor_from_object`, preserved their expected `Allow Interaction?` and `Preset` defaults, were destroyed after inspection, and left dirty package count at `0`
- Module runtime validation: `/Game/_MCP_Temp/CubelessUltraVolumetricsModuleRuntime/Map_CL_UV_ModuleRuntime_20260618_001` proves the isolated module set in PIE/SIE. Four interactive modules create non-`NoInteraction` dynamic material instances and auto-wire to the single `BP_CL_UV_Module_InteractionController`; four static comparison modules create `NoInteraction` dynamic material instances and keep `InteractionControllerRef=None`.
- Module trigger validation: in the isolated module map, `DrawRing` on `CL_UV_ModuleIso_InteractionController` with `T_CL_UV_RTBorder` started `RingTimeline`, then advanced it to the 10-second end state without new PIE/runtime errors.
- FX actor validation: all ten `BP_CL_UV_FX_*` actors spawned in the isolated validation map, exposed a NiagaraComponent bound to the expected `NS_CL_UV_*` system, accepted `activate`, `reinitialize_system`, and `advance_simulation`, then were destroyed with dirty package count `0`.
- Material validation: all Material Instance variants loaded, updated, saved, and retained expected UltraVolumetrics parents
- Temp dependency audit: `0` references to `/Game/_MCP_Temp/CubelessUltraVolumetricsModular`
- Final production audit: all `81` assets load, all `29` Blueprints compile and save, all ten FX actors retain their Niagara system binding, no `/Game/_MCP_Temp` dependencies exist, no redirectors exist under the production root, and final dirty package count is `0`.
- Dirty package audit: no dirty production packages after save

## Smoke Test Snapshot

The first placement smoke test lives in:

- `/Game/_MCP_Temp/CubelessUltraVolumetricsSmoke/Map_CL_UV_Smoke_20260618_001`

Placed actors:

- `CL_UV_Smoke_CoreVolume`
- `CL_UV_Smoke_GroundFog`
- `CL_UV_Smoke_StampFog`
- `CL_UV_Smoke_InteractionController`
- `CL_UV_Smoke_Niagara_Ring`
- `CL_UV_Smoke_Niagara_Burst`
- Basic floor, landmark meshes, directional light, and skylight

Smoke result:

- Native `safe_new_preview_map` dry-run and real creation passed under `/Game/_MCP_Temp`.
- Wrapper Blueprint placement passed for core volume, ground fog, stamp fog, and interaction controller.
- NiagaraActor placement passed for ring and burst systems; both loaded the production Niagara systems, activated, and accepted `advance_simulation`.
- The smoke map saved cleanly and the final dirty package count was `0`.
- Opaque review screenshots were written under `Saved/MCP/UltraVolumetricsSmoke`.
- Niagara visual body was not clearly visible in the active editor viewport screenshot even after simulation advance. Treat this as a remaining visual review item for PIE or a dedicated Niagara preview map, not as an asset load failure.

Follow-up Niagara visual investigation:

- String/API inspection found that original module `NMS_Activator` reads `NPC.NMPC_UltraVolumetrics.CaptureActive`; the default value is `false`.
- Unreal's `NiagaraFunctionLibrary.get_niagara_parameter_collection` and `NiagaraParameterCollectionInstance.set_bool_parameter` are available in this project, so the smoke test toggled `CaptureActive=true` and `Strength=1.0` on the editor-world collection instance only.
- After toggling those runtime values, `NS_CL_UV_Ring` and `NS_CL_UV_Burst` were reinitialized, activated, and advanced successfully again, but the active editor viewport still showed only NiagaraActor/editor icons, not a clear rendered ring or burst body.
- The runtime collection value was reset to `CaptureActive=false` after the test, all temporarily hidden smoke actors were restored, and dirty package count remained `0`.
- Current decision: keep Niagara load/spawn/activation as technically passed, but require PIE or a proper runtime interaction-controller capture for visual approval. Do not deep-fork Niagara yet; first prove the desired interaction in a real runtime test.

PIE/runtime follow-up:

- `LevelEditorSubsystem.editor_request_begin_play()` successfully entered PIE/SIE on the smoke map after the first simulate request settled.
- In the PIE world `/Game/_MCP_Temp/CubelessUltraVolumetricsSmoke/UEDPIE_0_Map_CL_UV_Smoke_20260618_001`, the runtime `NMPC_UltraVolumetrics` instance was set to `CaptureActive=true` and `Strength=1.0`.
- `NS_CL_UV_Ring` and `NS_CL_UV_Burst` were found as PIE-world `NiagaraActor` instances, reinitialized, activated, and advanced successfully.
- The PIE viewport still did not show a clear ring/burst body; the capture mostly showed the smoke floor/landmarks. The PIE run was stopped cleanly and final dirty package count stayed `0`.
- Updated interpretation: this is not currently a visual-effect approval failure. It indicates the Ring/Burst systems should be treated as interaction writer/capture participants. Their visible result should be judged through the fog volume response or render-target output driven by `BP_CL_UV_InteractionController`, not by standalone Niagara beauty capture.

Interaction-controller follow-up:

- Binary string and Python reflection inspection identified the key controller outputs and runtime functions: `InitializeCapture`, `DrawRing`, `ClearRenderTarget2D`, `DrawMaterialToRenderTarget`, `T_RT`, `T_RingRT`, `M_DrawRing`, `RingTimeline`, `Interaction2dTimeline`, and ten `Projectile*Timeline` components.
- `DrawRing` is callable through Unreal reflection as `controller.call_method("DrawRing", (), {"texture": Texture2D, "radius": Float, "location": Vector})`; a successful call starts `RingTimeline`.
- `RTF_R16F` render targets cannot be numerically validated through this project's current `RenderingLibrary.read_render_target_*` path: clearing transient or asset `RTF_R16F` targets to different values still read back as red `1.0`. Use viewport/fog response, exported RT inspection, or an alternate RGBA16F diagnostic path for future validation.
- The smoke map initially used `NoInteraction` runtime fog material instances (`MI_Default-NoInteraction`, `MI_UltraVolumetricsGroundNoInt_Inst`, and `MI_UltraVolumetricsStampNoInt_Inst`), so `DrawRing` could run without any visible fog response. For interaction smoke tests, force the fog components to the interactive variants before judging the result.
- After switching the smoke fog components to `MI_CL_UV_CoreVolume_Interactive`, `MI_CL_UV_GroundFog_Interactive`, and `MI_CL_UV_StampFog_Interactive`, editor-world `InitializeCapture` plus `DrawRing` still produced only render-noise-level viewport differences. This is not enough for visual approval because the controller is designed for runtime BeginPlay/tick context.
- PIE validation exposed a real setup requirement: `BP_InteractionController` repeatedly reads `GetPlayerPawn` in `TrailGraph`. A smoke map with no player pawn logs repeated PIE runtime errors on `CallFunc_GetPlayerPawn_ReturnValue == None`. The next interaction smoke should either provide a minimal pawn/player start or configure the controller to use a fixed capture location without following the player.
- UnrealMCP active-viewport screenshot commands are now guarded in source against PIE/SIE use, but this run still crashed before the guard existed. The crash occurred in `UnrealMCPEditorCommands.cpp` inside `CaptureActiveEditorViewportToPng()` after PIE runtime errors were already occurring. After editor restart, the guarded plugin should return a structured error instead of touching the active viewport during PIE/SIE.
- Live guard validation passed after editor restart: `capture_viewport_bookmark_screenshot` still works outside PIE and returns a structured error during PIE/SIE instead of crashing.
- The smoke map now contains `CL_UV_Smoke_TestPawn` (`DefaultPawn`, `AutoPossessPlayer=Player0`) and `CL_UV_Smoke_PlayerStart`. With that setup, PIE starts in `/Game/_MCP_Temp/CubelessUltraVolumetricsSmoke/UEDPIE_0_Map_CL_UV_Smoke_20260618_001`, `GetPlayerPawn(0)` resolves to `DefaultPawn_0`, and the previous `CallFunc_GetPlayerPawn_ReturnValue == None` errors stay at `0`.
- The durable authoring switch is the fog actor property `Allow Interaction?`. When it was `false`, PIE BeginPlay recreated fog component MIDs from the parent Blueprint defaults and reset them to `NoInteraction` materials even after an editor-world component material override. After setting `Allow Interaction?=true` on `CL_UV_Smoke_CoreVolume`, `CL_UV_Smoke_GroundFog`, and `CL_UV_Smoke_StampFog`, the runtime actors created interactive MIDs and automatically wired `InteractionControllerRef` to `CL_UV_Smoke_InteractionController`.
- With `Allow Interaction?=true`, `DrawRing` can be driven in the PIE/SIE smoke world without runtime-forcing material replacement. The verified call used `T_CL_UV_RTBorder` loaded via `unreal.load_asset`, then `controller.call_method("DrawRing", (), {"texture": texture, "radius": 1200.0, "location": unreal.Vector(0, 0, 120)})`; `RingTimeline` entered `playing=True` and advanced to its 10-second end. Directly calling `InitializeCapture` during PIE can still log `Thumb == None` on the `Destroy Component` node when called after the controller has already initialized. Treat `InitializeCapture` as a BeginPlay/setup path, not a repeatedly callable interaction trigger.

## Next Deep-Fork Gate

Move to a deeper fork only after a real scene test proves a specific collision or customization need. The likely first deep-fork targets are:

- Interaction render target ownership
- `MPC_UltraVolumetrics`
- `NMPC_UltraVolumetrics`
- Niagara modules `NMS_Activator` and `NMS_2dCurlNoiseForce`
- Sprite material references used by interaction effects
- Preset parent/data class if Cubeless needs a custom preset editor or strict package independence
