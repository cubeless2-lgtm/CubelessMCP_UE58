# Historical investigation log

Superseded settings, failures and hypotheses are retained as evidence. Use README.md for current state.

# Volumetric Clouds & Sky Effects study

2026-11-14. Status: incomplete.

Requested destination: /Game/Study/VolumetricCloudsSkyEffects.
Engine used: installed UE 5.8.4. Current project only.

## Current verified state (latest correction, 2026-11-14)

All seven lesson maps have been authored and saved, but the study is **not complete**. Use the native lesson materials and their instances; old M_01_CloudBase/M_02_CloudErosion and probe materials are historical drafts, not accepted outputs.

- Maps: L_01_CloudBase, L_02_CloudErosion, L_03_StormFlow, L_04_EyeBase, L_05_EyeFlowParallax, L_06_StormEye, L_07_StormSky.
- Source-critical correction: Part01 at 568 seconds clearly shows **Multiply**, not Power, for Height Mask Bias. Correct formula is `density = (noiseG - (1-(1-exp(h))*(1-exp(1-h)))*HeightMaskBias) * CloudDensity/100`. Prior log entries claiming Power, a compiler defect, or requiring bias0.1/0.2 are superseded. M_01_CloudBaseNative, M_02_CloudErosionNative, M_03_StormFlowNative and M_07_StormLighting were corrected and saved. MI01/MI02 bias restored1.52, density10.
- Part03 frames1200/1269 show center mask `flowmap.B * bounds`, without the draft's invented Power. Height bias nesting is `lerp(normal, lerp(storm, center, centerMask), stormAlpha)`. Both M03 and M07 were corrected and saved. MI03 uses normal1.52/storm0.87/center2.2, density20. Center2.2 is visual tuning to open the storm eye; 1.43 was an initial approximate source reading, not a verified final value.
- Part03 time jitter from frames1048/1076 was added to M03/M07: `Time + 0.25*VolumeNoiseShape64.R(frac(World/(TimeNoiseScaleKm*100000)+Time))`, where Time is the shared flow time. TimeNoiseScaleKm40 is an approximate reference setting requiring final comparison.
- Earlier captures proved custom world-space volume density works. Prior scalar-calibration captures are intermediate evidence only. New corrected graphs still require stage-by-stage visual and runtime verification.
- Corrected MI03 is now saved in maps3–6. Maps4–6 eye scale is (22.5,22.5,13.5), positionZ150000cm; map7 eye positionZ650000cm matches its raised5–15km cloud layer. These transforms are fitted to the scene, not exact source transforms. MI07 has independent final-stage settings.
- Shared thunder is implemented in MF_ThunderPulse and connected to M06 eye, M07 eye and M07 volume. GPU sampling verified the pulse varies over time (maximum analytical error 0.00001134); rate constants remain approximate. L07 actual PIE was observed with a DefaultPawn and animated clouds, then stopped normally.
- Latest source corrections: M07 negative-exponent lighting now takes density-scaled extinction (Part07 frame280), translucent opacity takes the continuous Saturate mask instead of DitherTemporalAA. M06/M07 stars and tip noise use G; tip selection now uses bounded flowmap B, matching Part06 frame2340 rather than the draft vertex-G mask. Compile returned no errors and owned materials were saved.
- Remaining: reconcile remaining graph/value differences (including Part07 center contrast and remaining parameter values), refine Part07 color/sky, finish per-stage motion/fidelity comparisons after all seven PIE smoke checks, clean owned diagnostics and accidental empty L_06_EyeStarsAndCore. No packaged-runtime claim.

The sections below retain chronological investigation history. Old process IDs, transient sessions, formulas and incomplete-state statements are not current instructions or current verification.

## Sources in lesson order

1. https://www.youtube.com/watch?v=paO-Its5P2Q
2. https://www.youtube.com/watch?v=ND1A-9n6e0Y
3. https://www.youtube.com/watch?v=Zy4GGO7-9ig
4. https://www.youtube.com/watch?v=x36BCa_Rotw
5. https://www.youtube.com/watch?v=zg2tYGTznho
6. https://www.youtube.com/watch?v=ypfoC--ABmM
7. https://www.youtube.com/watch?v=GwOln8Ckdio

Each title was searched through YouTube. All seven 360p videos and English automatic captions were acquired in Saved/CloudTutorialReference.
Author resources, linked in Part03 description:
https://drive.google.com/file/d/1XNi98Ierc0EkV0dtUwjteLoWXsUvOSCs/view

Archive contains storm_flowmap.png, Custom_volume_flowmap.txt, HalfSphere.fbx, stars.jpg.
Original files retained in Saved/CloudTutorialReference/author-resources.

## Historical initial assets

- Source/storm_flowmap: imported, sRGB disabled, vector displacement compression.
- Source/stars: imported.
- Source/HalfSphere: imported with vertex colors, no material or texture import.
- Materials/M_01_CloudBase: draft custom-expression implementation of the first lesson.
- Materials/M_02_CloudErosion: draft extension with opposite-moving erosion and altitude weighting.
- L_01_CloudBase: cloud, atmosphere, directional light, skylight.

The draft uses HLSL Custom expressions instead of reproducing every individual tutorial graph node. Numerical and visual equivalence are NOT verified. It must not be described as an exact reproduction.
The height expression was corrected after examining the lesson frame; further full graph comparison remains required.
Part03-07 materials and stage levels have NOT been authored.

## Historical initial observations

Official MCP at http://127.0.0.1:18765/mcp verified project and PID 36180.
Changing maps in a Python tool returned an unknown transaction-owner status. A subsequent read established the requested map, four expected actors, saved assets and zero dirty packages. The map creation was not replayed.
Material compile and save were invoked. Early incomplete graph compilation produced an error before wiring was finished. Final shader validation and visual comparison remain pending.
Both viewport screenshots and SceneCapture output were black. Cause unresolved. Computer Use did not list a targetable Unreal Editor window for the hidden launched process.
Preview capture actor removed and temporary exposure override restored to zero.
Helper scripts and raw evidence reside in Saved/CloudTutorialReference. No commit/push performed.

Next work: resolve visible rendering in this same editor/project, verify Part01 graph against frames, then Part02. Import the author's function graph for Part03, author and compare Parts03-07 in order, and retain individual lesson assets/levels. Complete visual, save/reload and runtime checks before claiming completion.

## Active continuation 2026-11-14 12:19 KST

Goal active, user explicitly requested completion and permits Computer Use.
Visible same-project Editor launched at PID 24376, endpoint http://127.0.0.1:18765/mcp.
Saved/CloudTutorialReference/run_editor.py now guards PID 24376.
Computer Use sky UE window id 2558566. Node REPL variables sky, ue are available. sky.get_window_state automatically emits screenshots but returned undefined in this runtime. Read SKILL guidance on resumption.

Observed missing UsedWithVolumetricCloud flag in UI, enabled on both materials. Black viewport fixed, atmosphere visible.
Default engine cloud material displays clouds in the same scene. Draft still did not render clouds, even native constant extinction.
Corrected extinction connection from MP_OPACITY to MP_SUBSURFACE_COLOR based on installed engine source.
Height mask reference frame formula440.png shows (1-exp(x))*(1-exp(1-x)); formula533.png shows a final Power. Changed to pow(1-product,Bias).
Original production custom code retained in Saved/CloudTutorialReference/base_code_backup.txt. M_01 bias restored 1.5.
Current diagnostic: wire_volume_bsdf.py explicitly connects SubstrateVolumetricFogCloudBSDF to MP_FRONT_MATERIAL for both drafts, with albedo and custom extinction. It restores production custom code for M_01. Editor is compiling/saving. Running exec session 58312 must be polled before further mutations. At last log Part01 save completed and Part02 save was underway. Do NOT replay this script without reconciliation.
An unused native Constant3Vector density diagnostic remains in M_01 and should be removed after successful output.
Scene cloud component restored to M_01 via set_material. Level may be dirty from material swaps, owned only.
ShowFlag.Cloud forced to 1 for investigation (original 2), must restore to 2 later. Wrong ShowFlag.VolumetricCloud name was only queried, no actual useful setting.
Computer Use Ctrl+R toggled twice, real-time restored.
No Part03-07 authored yet. Need finish all lessons, visual checks, orderly final editor handling.

## Continuation 2026-11-14 12:30 KST

Previous turn made progress (diagnostic evidence and authoritative material edits), not blocked.
Part04 source transcript read fully. Created Materials/M_04_EyeBase and MI_04_EyeBase with native graph: vertex-red intensity/power, inverse Fresnel using VertexNormalWS, multiplier, Saturate, DitherTemporalAA to opacity mask; Masked/Unlit. Compile returned [] and both saved. Visual comparison pending. PreviewColor is a provisional inspection color.
New diagnostic assets under Materials: M_CloudOutputProbe, MI_CloudOutputProbe, M_CloudIntactProbe, M_CloudUnchangedProbe, MI_CloudInheritedProbe. Owned temporary assets; remove once diagnosis complete. They are NOT lesson outputs.
Key observation: even untouched duplicate of engine cloud MASTER and original engine MASTER showed no clouds; original engine m_SimpleVolumetricCloud_Inst previously displayed clouds. Engine instance parent same master, scalar only RefractionDepthBias 0, vectors empty. Further inspect static switches/base-property override fields/compile resource.
Minimal constant extinction tests (including copied default material) did not display. Shader errors returned [] except one corrected duplicate advanced output diagnostic.
Installed MaterialEditingLibrary.DeleteAllMaterialExpressions iterates mutable collection while deleting, leaving roughly half the nodes. Use list(get_material_expressions(m)) and delete one by one. Confirmed remaining 0 with reset_probe.py. This diagnosed and corrected the probe itself, not the missing-cloud root cause.
M_CloudOutputProbe now truly contains white albedo, 0.01 extinction, AdvancedOutput with ConservativeDensity=(1,1,1). Still no visible clouds.
M_01 and M_02 have explicit Substrate volume BSDF wired to FrontMaterial, saved from prior turn. Production custom code restored. M_01 still has unused constant diagnostic.
Level was saved with M_01 and reloaded successfully; map-changing Python returned expected unknown-owner guard and later read-only call succeeded. Current M_01/02 may remain dirty from compile_errors.py; save only owned assets.
No outstanding exec session. Editor PID 24376 remains open for ongoing goal, MCP functional.

### 2026-11-14 continuation: native graph and Substrate diagnostics
- Minimal probe with explicit SubstrateVolumetricFogCloudBSDF plus enable_new_hlsl_generator=True rendered a visible homogeneous cloud layer; previous legacy root-only probe did not. This is a diagnostic, not lesson completion.
- Created Materials/M_01_CloudBaseNative using discrete standard expression nodes and explicit Substrate output, replacing the custom-HLSL approach for further work. Density=0 test visibly removes the cloud layer, confirming the graph influences rendering.
- Current Native base is saved with Density=3, HeightMaskBias=1.5, CloudTiling=65 and UV=World/(65*100000)+wind. A mistaken multiplication variant was tested and reverted after rereading original transcript (237-242 seconds explicitly multiply tiling by100000 for denominator).
- Native graph at Density3 has only been observed as a broad continuous layer, not yet matched to reference cloud shapes. Do not call Part01 verified. Need inspect layer height mask, camera/scale, advanced output and exact source channels.
- Original M_01_CloudBase in memory is currently a diagnostic constant custom return0.01, with AdvancedOutput removed; it is NOT the new final asset. Do not save that diagnostic as the lesson result. M_02 remains earlier draft. Temporary probe materials must be removed after final versions exist.
- Latest local scripts: native_base.py, restore_native_uv.py, explicit_probe.py, refresh_native.py in Saved/CloudTutorialReference. Screenshot CloudNativeFresh.png records density0 test; CloudCurrent.png records earlier uniform cloud view.
- 3/5/6/7 lessons still need creation and all seven need visual/runtime checks. Goal remains active.

### 2026-11-14 continuation: original flow function and Parts 02/03/05 graphs
- Imported author clipboard graph to `/Game/Study/VolumetricCloudsSkyEffects/Functions/MF_AuthorVolumeFlowmap`: 48 nodes including comments, 56 connected input links. `flow_graph.json` is parsed source; `finish_flow_v3.py` created nodes, `fix_flow_links.py` repaired four typed function input labels. Vector4f requires construction then individual property assignment. Source comment dimensions not restored. Must still compare preview defaults and visual function behavior; no fidelity claim yet.
- Saved `M_02_CloudErosionNative` from discrete native base: reverse wind, erosion texture green, contrast/intensity, altitude weighting, scattering occlusion .2.
- Saved `M_03_StormFlowNative`: single bounded source flowmap, centered world-space coordinates, remapRG, original volume flow function for both noise scales, storm/center height-bias blending. Function compiles within material without reported errors. Flow time jitter from source noise is NOT added yet; parameter values need visual source reconciliation. This is an unfinished draft, not completed lesson.
- Duplicated exact tutorial `/Engine/Tutorial/SubEditors/TutorialAssets/T_soft_smoke` to `Source/T_SoftSmoke`. Saved `M_05_EyeFlowParallax`: centered flowmap scale/rotation, engine `/Engine/Functions/Engine_MaterialFunctions02/Texturing/Flowmaps`, two flow samples around BumpOffset, blue tint, preserved Part04 dither. Compiles without reported errors; visual/layout verification pending. `ExampleContent/FlowMaps` is a Material, not function; correct path is Texturing/Flowmaps. Helpers `eye5_finish.py`, `native_erosion.py`, `native_storm.py` retain authoring steps.
- Cloud issue now reproduced more narrowly: native base WITHOUT advanced output renders a uniform cloud layer and density0 removes it. ADDING AdvancedOutput makes layer disappear, even explicit constant Extinction=.01 and ConservativeDensity=(1,1,1). Octave1/2, HLSL generator true/false, and raster vs compute did not resolve it. None of these tests proves a renderer bug; root cause remains unresolved. r.VolumetricCloud.DisableCompute restored0. ShowFlag.Cloud remains forced1 from earlier diagnostics (restore2 eventually).
- Native base currently saved with original graph extinction restored (`MaterialExpressionMultiply_4`), explicit ConservativeDensity1, octave2, enable_new_hlsl_generator=False; actual display remains blank sky with advanced output. Camera restored pitch25 at(0,0,200). Tests at pitch80 also blank. Native2/3 inherited generatorTrue and octave2 before last base diagnostics.
- Dirty content now only old `M_01_CloudBase` and `M_CloudOutputProbe` diagnostic changes; do not bulk-save them. Main map still L_01, Native base assigned in memory. No other stages' levels yet. Parts06/07 not created. All stage visual/PIE verification remains outstanding.
- Extracted frames `part05-smoke.png`, `part06-{355,1050,1450,2037,2405,2600}.png`. Review these and full subtitles for remaining work. Goal remains active; this turn created authoritative assets and narrowed diagnostics, so it is progress, not blocked.

### 2026-11-14 continuation: Part06 asset, crash and pending editor restart
- Saved `M_06_EyeStarsAndCore.uasset` (76347 bytes) and `Source/T_TipNoise`, copied from exact tutorial `/Engine/MaterialTemplates/Textures/T_Noise01`. Added cloud color layers, vertex-G center mask, flowing stars, flowing tip noise. Draft has no shared thunder pulse and no atmosphere-sunlight rim yet; those are required for fidelity. Values still need source/frame matching. Compile returned [] but no scene visual/PIE check.
- Computer Use inspected native base material editor. UI displayed Unlit while Python shading_model read DefaultLit and BSDF.emissive_only=False. Reloading saved material did not fix missing clouds. Conservative density debug was pink/red, so do not assume conservative-density skipping is the cause. Debug override restored2 before crash.
- Crash at 03:59:15 UTC during LoadLevel to L06: `EditorServer.cpp:2544 World Memory Leaks: 2 leaks objects and packages`. Reference-chain evidence specifically names standalone UNSAVED duplicated world `/Game/Study/VolumetricCloudsSkyEffects/L_06_EyeStarsAndCore`, not L01. Initial commentary attributing it to previous-world references was too broad. L01 was saved; L06 duplicate was NOT saved to disk and was lost. All earlier saved materials remain on disk. PID24376 exited. The only content dirty before crash was owned old M01/probe diagnostic state.
- Reexecution of the combined same-project editor launch/helper-update command was rejected by automatic approval review with only `blocked by policy`; did not retry or use UI to bypass. Asked user asynchronously to open the same .uproject directly. No UnrealEditor process as of last check. Goal active; this is first turn with restart blocked, not eligible for blocked status yet.
- Independently patched run_editor.py to execute each script with an isolated Python globals dictionary and gc.collect(); defensive reference-lifetime improvement, NOT a proven fix for standalone UWorld keep flags. Runner still expected_pid=24376 and must be updated to observed new editor PID after user restart; endpoint must be rediscovered/verified.
- Prepared `configure_stage6_safe.py` using save_map(currentWorld,newPath) instead of an unsaved duplicate. Syntax checked only; NOT run. Native HalfSphere bounds: originZ27349.999756cm, radiusXY49530.945312cm, extentZ22650.000244cm; scale10 is a plausible initial 5km-radius eye, to be visually tuned. Prior assumption of scale1000 was inappropriate for this imported asset.
- Part07 source confirms emission = saturate(pow(volumeMask,-.25)*.001) * intensity * pow(stormAlpha,contrast) * color, with shared thunder modulation and tip-fill multiplier. Need exact shared time function from video graph, atmosphere-aware sky dome and final translucent eye. These are NOT implemented. Existing Parts01-06 still need fixes/fidelity reconciliation/levels/PIE. No completion claim.

### Restart wait audit, second consecutive turn
- Rechecked OS process state: no UnrealEditor process. User restart reply has not arrived; did not retry rejected launch or bypass it through UI.
- Prepared `part7_emission_prepared.py` from Part07 39-320s: storm albedo blending and density/region-masked orange emission. Syntax check passed. It has NOT run and no M07 asset exists. Shared thunder, tip-fill, sky dome and scene verification remain outstanding.
- Editor restart remains necessary for authoritative asset creation and rendering verification. Goal active; second consecutive turn with same external restart blocker. Latest work is offline preparation only, not runtime completion.

### 2026-11-14 resumed editor: saved Part06 level and Part07 emission draft
- User explicitly asked agent to reopen. Launch succeeded; first Hidden startup PID24468 never exposed a window. Confirmed own identity, stopped only that startup, relaunched with Normal. Current verified editor PID12752, same project/map, MCP18765. User subsequently set goal active. Runner expected_pid updated12752. Do not use old PID24376/24468.
- Fresh factory-created diagnostic `M_FreshCloudCheck` Volume/Additive/DefaultLit, legacy extinction .01 and AdvancedOutput still produced blank sky. Existing diagnostic materials remain pending reviewed native cleanup.
- `save_map(currentWorld,newPath)` saves a COPY; it does NOT switch active world. First configure_stage6_safe saved empty L06_EyeStarsAndCore then added eye to active L01 and second save displayed duplicate-name informational modal. CUA acknowledged modal; mutation outcome inspected. Do not repeat this pattern.
- Python `.delete_asset` cleanup explicitly blocked by plugin with instruction to use reviewed native cleanup. No deletion happened. Used distinct `/Game/Study/VolumetricCloudsSkyEffects/L_06_StormEye` to save configured scene, removed only new Part06 actor from L01 via EditorActorSubsystem, restored native base cloud material and saved L01. L06_EyeStarsAndCore remains empty accidental copy requiring native cleanup later.
- Loaded SAVED L06_StormEye successfully using isolated execution; MCP reported owner-change unknown but next read confirmed current world, 5 actors, no crash. Eye actor at(0,0,150000), scale10, original HalfSphere with M_06. Current map L06_StormEye. Scene saved after temporary capture actor cleanup.
- Executed prepared Part07 emission script: `M_07_StormLighting` now saved with storm albedo + negative-exponent emission mask; compile[]. Still missing shared thunder, tip-fill modulation, sky dome and final translucent-eye treatment. Never claim Part07 finished.
- Important new verification API: `unreal.AutomationLibrary.finish_loading_before_screenshot()` exists and was executed successfully after shader workers. Use it after material changes before capture to avoid interpreting async fallback. Prior recompile[] alone does not establish shader completion.
- User working in another foreground window; CUA captures of main target sometimes showed unrelated foreground/RDP window, so avoid repetitive focus changes. Use SceneCapture2D for intermediate inspection. Automation high-res screenshot may remain queued while editor obscured/minimized.
- `scene_capture6.py` and `capture6_manual.py` create temporary capture actor, RTF_RGBA8, SCS_FINAL_COLOR_LDR, export PNG then destroy actor. Manual exposure settings succeeded. Captures `Part06SceneCapture.png`, `Part06ManualExposure.png` show mostly dark blue sky and a faint blue dotted ring, NOT intended source result. Need diagnose opacity/mesh vertex colors/flow samples/camera and light. SceneCapture is not substitute for final viewport/PIE validation.
- `audit_roots.py`: M06 FrontMaterial=None, Emissive root=MaterialExpressionLinearInterpolate_2, no Substrate nodes. Native base FrontMaterial points to explicit BSDF_1, legacy extinction=None. Fresh diagnostic legacy extinction Constant3Vector_1, FrontMaterial=None. Thus assumed stale surface Substrate root was NOT supported.
- Native base, cloud3 and eye6 still visually wrong; no stage passes full fidelity/runtime gate. Levels02/03/04/05/07 and all PIE checks still missing. Goal remains active; this turn made saved asset/map progress and successful map-loading evidence.

### 2026-11-14 continuation: stage maps and occlusion diagnosis
- Current editor PID12752 and MCP18765 verified. Current map is now L_01_CloudBase (loaded saved map; owner-change unknown was followed by authoritative world read).
- Saved L_02_CloudErosion, L_03_StormFlow (4-actor scenes, respective native materials), L_04_EyeBase, L_05_EyeFlowParallax (cloud3 + source HalfSphere and stage-specific eye material). L06 restored to cloud3/M06 and saved before leaving. These are draft snapshots, NOT fidelity/runtime passes. L07 still missing.
- Created/saved MI_01_CloudBase parent M_01_CloudBaseNative; assigned/saved L01. All temporary parameter overrides restored: CloudDensity3, CloudTiling65, WindSpeed.02, HeightMaskBias1.5.
- Native1 updated to tutorial-exact Custom exp(x) nodes instead of native Exponential, and a nonnegative Max(extinction,0) before Substrate BSDF. Saved. Neither change fixed the visual issue; do not claim otherwise. Native2/3/7 have not inherited these changes.
- Key new evidence: stage6 eye renders visibly with engine cloud instance or density0. With native1 density3 it is occluded almost entirely by the cloud layer; density.01 produces a translucent gray veil. Thus previous statement of simply missing clouds was incomplete: native layer is strongly occluding the eye/sun, with no visible noise-shaped cloud boundaries. Eye mesh itself does render; no need to flip normals based on earlier blank captures.
- Assign/capture in the same Python execution can sample stale scene state. Use separate calls after assignment, and a separate finish_pending.py compilation barrier after mutation transaction. finish_loading_before_screenshot source calls FinishAllCompilation, but recompile/edit transaction may schedule additional work afterward. Immediate captures after recompile were transiently transparent and later opaque; do not treat them as stable fixes.
- Stable tests: native1 instance tiling1 instead of65 did not reveal shapes; HeightMaskBias0 removes layer as expected. Values restored. Full exported graph native_debug.t3d confirms NormAltitudeInLayer output index2, texture green output2, expected arithmetic and explicit BSDF extinction connection. Native scalar GUIDs distinct, parameter names and default values all present. Advanced defaults match engine (.5 multiple-scattering constants, phase0). Root cause still unresolved.
- MaterialEditingLibrary.set_material_instance_scalar_parameter_value returns False even after successful change: installed MaterialEditingLibrary.cpp1485-1493 initializes bResult=false and never sets true. Readback confirmed all actual overrides. Earlier commentary interpreted false as failed update then corrected it; do not repeat that diagnosis.
- CUA helper list_windows repeatedly returned UE window2558566 while get_window reported only3148220. Recovery refresh/retry failed; stopped CUA inputs for this turn. MCP and SceneCapture remain functional. Need fresh helper recovery for final viewport/PIE proof; no user action currently required.
- Capture files Part06ManualExposure.png and CloudFrozen.png were repeatedly overwritten by diagnostics; they are not final proof or consistently named stage evidence. barrier_compare.py first same-call comparison is invalid for judging graph state. All temporary scene capture actors destroyed. No outstanding shell job.
- Still required: resolve volume shading/noise, exact source thunder/rim/tip-fill/skydome/translucent-eye graph, finish stage7, tune scene/camera, stage-by-stage visual/PIE verification, reviewed native cleanup of old diagnostics and accidental empty L06_EyeStarsAndCore. Goal active.

### 2026-11-14 continuation: Part07 scene and additional source corrections
- Current verified editor PID12752; current SAVED map L_07_StormSky. All seven stage maps now exist, but none has final fidelity/PIE signoff.
- Added atmosphere-sunlight rim to M_06_EyeStarsAndCore: vertexR power/multiplier/saturate blends final eye output with SkyAtmosphereLightIlluminance(index0)*AtmosphereSunLightIntensity(.1). Compile[] and saved; original-source graph at Part06 ~600-700s. Applied after stars/tip in our graph; inspect exact ordering during fidelity audit.
- Created M_07_SkyDome from exact /Engine/EngineSky/M_SimpleSkyDome, preserving SkyAtmosphereViewLuminance + LightDiskLuminance. Added panning T_SoftSmoke G channel, independent XY tiling(1,2), speedY-.001, contrast2, UV-Y zenith fade*2/power2, sunlight brightness.1. Values/gradient need final source and visual tuning.
- Created M_07_EyeTranslucent from updated M06: changed blend to Translucent and connected the existing opacity-mask source to Opacity, following Part07 1700-1717s. Retains dither function; source output wiring should be reviewed at higher clarity. Not visually verified.
- Added M07 TipFillIntensity2.75: final emission multiplied by (1+bounded storm-center mask*TipFillIntensity), following 832-987s. Shared thunder still NOT implemented; constants need source reading.
- Created L_07_StormSky from L06 as saved copy, then restored/saved L06. Stage7 cloud=M07, layerbottom5km/height10km (read directly from Part07 screenshot940), eye=M07Translucent scale(22.5,22.5,13.5) provisional from reference visual, position unchanged. Added /Engine/EngineSky/SM_SkySphere (bounds radius4096cm), scaled to200km radius, M07SkyDome, no collision/shadows. Reloaded saved L07 and confirmed correct world. Saved after capture actor destroyed.
- Stage07Draft.png manual-exposure scene capture is severely overbright yellow/white, NOT intended visual. Need resolve volume shading/emission before acceptance. Current negative-exponent emission clamps signed mask to epsilon; this may inflate emission in empty/negative regions. Check source exact mask, Power semantics and exposure rather than treating this as a pass.
- Diagnostic M_01_CloudTutorial was created from CLEAN MaterialFactoryNew using tutorial legacy volume root (whiteAlbedo, noise-height density on SubsurfaceColor), defaultlit/additive/volume, advancedoctave2, mip0/wrap. It also failed to render intended cloud shapes; saved as diagnostic candidate, not final stage1. New asset requires future cleanup if unused.
- Native1 sampler changed to explicit const_mip_value0 and SharedWrap, saved. Neither resolved issue. Native1 custom exp debug offset restored to exact exp(x), advanced raymarchshadow restoredTrue, MI01 overrides restored3/65/.02/1.5 and saved. L01 saved with MI01 before leaving.
- M_FreshCloudCheck has UNSAVED diagnostic state: now simple sphere-lattice density in world coordinates using Custom, legacy volume root, AdvancedOutput2. This also showed blank sky in capture. Before that, repurposed it temporarily as surface RT noise slice: NoiseSliceG.png clearly shows real grayscale noise, excluding an all-white texture asset. Do not save this as lesson output.
- Enabling raymarchshadowFalse on native1 gave bright uniform fog; True gave dark/blue uniform appearance. Texture wrap and constmip0, coverage .5/tiling10 did not reveal shapes. Tests reverted except explicit sampler settings.
- get_statistics and list_shaders are available on MaterialEditingLibrary (earlier guess get_material_stats was wrong). CleanTutorial stats pixel341/samplers9, native1 pixel86/samplers10; cloud PS/CS permutations exist (99 shader entries on CleanTutorial). No missing cloud usage compilation diagnosed. Avoid printing entire shader list.
- r.DumpShaderDebugInfo temporarily1 during unique custom epsilon edit; produced no useful material shader dump (only existing WorldGrid DDC file). Restored0. No engine/project settings files changed. Other queried console values: r.Substrate1, r.VolumetricCloud1, DisableCompute0, VolumetricRenderTarget1.
- CUA recovered main editor using actual previously returned window3148220 and confirmed visible L01 viewport. Opening M01CloudTutorial material editor via MCP succeeded, but targeting its returned window461722 failed twice with 'foreground window did not report a process id'; stopped that CUA attempt and closed owned material editor via AssetEditorSubsystem. Main editor retained for goal. Do not say Computer Use completely unavailable.
- Reference captures: part01-exp496.png confirms source custom exp(x), part01-pin547.png confirms green sample; pulse_graph522.png shows full shared pulse topology (Time, periodicTime~1.86, multiply/cosine/sine/one-minus/multiply/saturate); constants still not read clearly. part06_early_contact.png, part06_build_contact.png, part06_tail_contact.png cover full Part06; pulse block is not visibly taught in sampled frames. Part07 copies it at522-524s. part07_translucent.png, tipgraph860.png, tipfill940.png support later changes.
- Official Epic documentation checked: https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-cloud-material-in-unreal-engine says default cloud material changed in5.5, original noise textures retained. This is compatibility context, NOT proof of our bug. Forum https://forums.unrealengine.com/t/ue5-8-1-older-materials-ignore-opacity-mask-and-ao-when-using-substrate/2769761 describes hidden bCanMaskedBeAssumedOpaque issue for MASKED surface materials; not evidence for volume problem and no patch applied.
- Still required: solve custom cloud rendering, finish source-shared thunder and flow noise time jitter, reconcile exact graph/settings, verify each stage viewport/PIE/animation, cleanup reviewed native diagnostic assets and accidental empty L06. Goal active.

### 2026-11-14 cloud coverage diagnosis and visible stage 1–2 results
- Previous turn only revalidated the editor; this continuation produced new render evidence and saved calibrated lesson instances. Same live editor PID12752, no restart.
- M_01_CloudTutorial new HLSL generator=True did not reveal intended clouds. Restored False in memory; existing saved asset was already False. This test does not establish an engine/compiler defect.
- Rebuilt UNSAVED M_FreshCloudCheck diagnostic as explicit Substrate volume with world-space stripe density. Constant emission illuminates even empty volume, so it is not a valid density visualization by itself. Density-masked emission and a lit stripe variant showed spatial boundaries. This proves custom world-position density participates in rendering here. Diagnostic remains unsaved and is NOT lesson content.
- Important correction to earlier blanket 'custom cloud rendering failure': native lesson 1 at HeightMaskBias0.5 shows a nearly continuous cloud underside; at 0.1, individual illuminated clouds and sky gaps are clearly visible. Existing 1.5 setting creates dense overcast in the tested view. The precise reason reference values produce different coverage remains unproven; do not claim an engine bug or exact numeric fidelity.
- Saved MI_01_CloudBase overrides HeightMaskBias0.1, CloudDensity10, WindSpeed0.02; parent graph retains source-derived structure/defaults. Saved L_01_CloudBase with this instance. Diagnostic capture Stage01_Coverage01_WindZero.png uses temporarily frozen wind0, camera(0,0,200), pitch25/yaw0, manual exposure and no temporal accumulation; not final viewport/PIE evidence.
- Created and saved MI_02_CloudErosion parent M_02_CloudErosionNative. At HeightMaskBias0.1 erosion removes most clouds; 0.2 gives clearly separated larger clouds. Saved overrides HeightMaskBias0.2, CloudDensity10, WindSpeed0.02. Applied and saved L_02_CloudErosion. Stage02_Coverage02_WindZero.png captures wind0 before restoring/saving0.02. Existing erosion parameters remain unchanged.
- Both captures have temporal noise/blur and no ground terrain; they verify visible volume shapes only. Final appearance matching, animation/PIE, stage3–7 calibration, shared thunder, flow jitter and diagnostic cleanup remain required. Current world L_02_CloudErosion. These height overrides are explicit calibration differences, not a claim of exact reference settings.

### 2026-11-14 source graph correction supersedes scalar workaround
- Direct enlarged source frame `part01-subtract568.png` clearly reads Multiply at the Height Mask Bias node. Earlier interpretation as Power was wrong. Fixed four native volume materials (M01/M02/M03/M07), saved, compile returned[]. Restored MI01/MI02 HeightMaskBias1.52, density10. Old M01CloudTutorial/old custom-HLSL drafts remain obsolete diagnostic content pending cleanup; do not reuse their formula.
- Source Part03 `part03-center1200.png` and `part03-center1269.png` show no exponent on center blue and a different lerp nesting. Removed invented StormCenterContrast Power and parameter from M03/M07. Correct center=`B*bounds`; bias=`lerp(normal,lerp(storm,center,centerMask),stormAlpha)`. MI03 saved normal1.52,storm0.87,center1.43,density20. These approximate source readings still require final visual tuning.
- Added source time-noise jitter to M03/M07 using T_VolumeNoiseShape64, frac(World/(40*100000)+flowTime), R*.25 added to flowTime. Both source frames1048/1076 and transcript1052–1071 support topology. Scalar40 remains approximate. Saved both materials.
- M07 removed extra Max(volumeMask,1e-6) before negative-exponent Power. Source does not insert it. Installed HLSLMaterialTranslator Power emits PositiveClampedPow; Common.ush returns0 for Base<=2.980233e-8. The added Max wrongly converted empty/negative regions into emissive positive inputs. Direct source Power compiled/saved. Actual final M07 visual still unverified after this fix.
- `Stage01_SourceMultiply152.png`: visible separate cloud masses at restored1.52, normal animated wind0.02. Computer Use main UE window3148220 confirmed L01 editor viewport also displays white cloud masses against blue sky, four actors. This is actual editor visual verification, not PIE or exact reference-frame matching. Saved L01 after temporary capture cleanup and camera placement(0,0,200),pitch25/yaw0.
- `Stage03_SourceGraphCorrection.png`: flow-like streaks now visible, but current view remains heavily clouded and storm opening is not accepted. Camera at(0,-800000,200),pitch35/yaw90. Later maps4–6 still reference M03 master defaults and must receive the corrected study instance; M07 instance/calibration also pending.
- Current world L01, PID12752. No live subprocess wait remains. Unsaved diagnostic M_FreshCloudCheck and restored generator state in M01CloudTutorial may remain; do not bulk-save. Goal incomplete: later-stage tuning, shared thunder, viewport/PIE motion checks and reviewed diagnostic cleanup remain.


### 2026-11-14 storm opening, later stage instances and rim polarity
- Verified current PID12752 and saved L03 with MI03. StormCenterHeightBias2.2 gives a visible central opening; 1.43 remained clouded. Stage03_StormOpening22.png, camera(0,-1800000,200),pitch20/yaw90, shows the complete opening and surrounding cloud ring. This is explicit visual tuning, not exact source parameter transcription.
- Applied saved MI03 to L04/L05/L06, replacing direct M03 master defaults. Saved their camera above and fitted HalfSphere scale(22.5,22.5,13.5), unchanged position(0,0,150000). L04 base mesh initially visibly too small at scale10. Stage05_EyeFit.png shows blue flow/parallax content inside the opening; no PIE validation yet.
- Part06 transcript529–653 explicitly says red vertex0 is border, red1 interior. Prior rim lerp A/B were reversed and overwrote the eye with sunlight. Swapped inputs in M06 and M07EyeTranslucent, saved/compile[]. Stage06_RimPolarityFixed.png now shows blue cloud flow, stars and orange core through the opening. Rim is still applied after the combined eye layers in our graph; source places it earlier, requiring final graph reconciliation.
- Created MI07 parentM07, applied and saved L07. Initial intensity3 made the entire storm ring strongly emissive. Source Part07 frame350 shows much lower light-intensity and cloud-density settings; saved approximate/tuned values CloudDensity3, HeightMaskBias1.445, StormHeightBias1.175, StormCenterHeightBias1.96, Light01Intensity0.05, WindSpeed0.02. These are not exact digit-level readings. The stored sample7 screenshot before these changes is obsolete.
- L07 camera saved(0,-2500000,200),pitch20/yaw90. Raised only Part07_StormEye_Translucent toZ650000cm because its oldZ150000 made the eye appear below the cloud opening after cloud-layer bottom increased to5km. Existing scale22.5/22.5/13.5 retained. Computer Use main UE window3148220 confirmed center alignment after raising; final look remains pale and needs contrast/light matching.
- Tested IsSky=True on translucent M07 eye following discussion in source. UE5.8 rejects it: 'Sky materials must be opaque or masked, and unlit. They are expected to completely replace the background.' Immediately restoredFalse, recompiled[] and saved; no invalid final material retained. Source presents IsSky+masked and translucent as alternatives; do not combine in this engine.
- Current map L07, saved, six actors; retained user editor. No shell sessions waiting. Shared thunder still absent; next work should implement/reconcile that source topology, improve final look, verify motion/PIE and clean owned diagnostic artifacts. Do not equate SceneCapture success with final fidelity or PIE.


## 2026-11-14 shared thunder and source channel corrections

MF_ThunderPulse implements a shared periodic Time signal (period1.86s), cosine/sine/cosine product and saturation. Slow/burst/counter rates0.1/2/3 are adjustable approximations because source frame522 does not resolve those numbers. ThunderLightIntensity1.5 scales volume and both eye materials. A transient GPU render sampled64 points across10seconds; output range0..0.877402 and max analytical error0.00001134. The transient material was released and no probe asset was created. This validates pulse math, not exact reference flash timing. Evidence: Saved/CloudTutorialReference/thunder_gpu_samples.json.

L07 now has a standard GameModeBase override and Study_Viewpoint_PlayerStart at (0,-2500000,200), rotation(20,90,0). Actual PIE showed DefaultPawn, PlayerController and the game world UEDPIE_0_L_07_StormSky; the Pawn remained at the viewpoint after93.78seconds. Computer Use showed moving clouds and eye textures. StopPIE followed by IsPIERunning=false confirmed normal termination. Stage07_PIE_A.png was produced by CaptureViewport while PIE ran, but its render path is not proven to be the game viewport; use the actual UI observations as PIE evidence.

Part07 frame280 shows the density Multiply feeding the negative Power. The draft used the preceding Subtract. M07 is now connected to the same density-scaled signal used by Extinction. The continuous Saturate opacity mask now feeds M07 translucent opacity, avoiding a temporal dither intended for masked blending.

Part06 narration1588/2046 identifies G for stars and tip noise. Narration2209 and frame2340 identify bounded flowmap B for the inner mask. Both M06 and M07 eye graphs were corrected accordingly; TipMaskIntensity was added. These fixes do not yet establish final visual equivalence: the current eye is still considerably bluer and the surrounding storm paler than Part07 final1814. Remaining source differences must be reconciled before acceptance. No commit or push.


## 2026-11-14 all seven PIE checks and eye layer order

All seven saved maps now have a standard GameModeBase and a Study_Viewpoint_PlayerStart. Maps1–2 use (0,0,200), pitch25/yaw0; maps3–6 use (0,-1800000,200), pitch20/yaw90; map7 uses (0,-2500000,200), pitch20/yaw90. Each map was separately loaded, its world name asserted, saved, started in PIE, verified as its matching UEDPIE world with a DefaultPawn, and visually inspected through the actual Editor UI. Each session was stopped before the next map; final IsPIERunning=false. Local reports stage01_runtime_check.json through stage07_runtime_check.json record the initial running state; the subsequent UI review and StopPIE calls are separate conversation evidence.

Observed: stages1–2 show base/eroded clouds; stage3 shows the opening; stage4 has a subtle flat eye (low contrast against sky); stage5 shows flowing texture; stage6 shows stars and core; stage7 shows orange/pink eye and illuminated volume. These are runtime smoke checks, not proof of exact source fidelity or all motion characteristics. Fine cloud stippling was visible in some UI frames. Current global AA=TSR(4), volumetric render target mode0, effects quality3 were read, not modified.

M06/M07 now apply atmosphere rim to the cloud layer before stars/core; stars use Lerp(cloud,stars,vertex-green mask) rather than additive overlay. Shared flash is in the cloud branch. Source Part06 frame2170 also established the tip order: G * intensity -> Power(contrast) -> Saturate -> Add(offset) -> Multiply(color). Both graphs now follow this order and expose TipNoiseOffset. Both compiled with no returned errors and were saved.

New MI_07_StormEye is assigned and saved in L07. It provides an approximate final-shot warm palette without changing the blue lesson6 settings. Values are visual reconstruction, not exact source readings: CloudDarkColor(.12,.025,.008), CloudLightColor(3,.35,.02), BackgroundColor(.025,.005,.003), StarsColor(.35,.3,.6), TipNoiseColor(8,1.5,.03), ThunderColor(2,.6,.08); TipNoiseOffset.15, TipNoiseFlowIntensity.075, TipNoiseIntensity1.5, TipMaskIntensity1.5, TipMaskContrast1.25, StarsTiling2.75, CloudTiling-3.1. Actual L07 PIE showed this warm core. It remains smoother and paler than the reference; source-noise detail, contrast and final center-mask settings remain acceptance work.
