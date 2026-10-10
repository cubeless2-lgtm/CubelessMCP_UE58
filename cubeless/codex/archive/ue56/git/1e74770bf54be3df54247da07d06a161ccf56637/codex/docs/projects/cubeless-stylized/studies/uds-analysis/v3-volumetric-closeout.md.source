# UDS Volumetric V3 Closeout

## Scope

- Reference level: `/Game/Cubeless/SKY/Test_UDS`.
- Target level: `/Game/Cubeless/SKY/Test_UnusedUDS_V3`.
- Target content root: `/Game/Cubeless/SKY/V3`.
- Level transitions for verification used UnrealMCP native `open_editor_level`; Python map switching was not used.

## Final Assets

- Sky dome mesh uses `/Game/UltraDynamicSky/Meshes/Ultra_Dynamic_Sky_Sphere`.
- Sky dome material instance uses `/Game/Cubeless/SKY/V3/Materials/MI_Cubeless_UDSVolumetric_SkyDome_V3_Rebuilt`.
- Volumetric cloud material uses `/Game/Cubeless/SKY/V3/Materials/M_Cubeless_UDSVolumetricCloud_V3_Reassembled`.
- Volumetric cloud material instance uses `/Game/Cubeless/SKY/V3/Materials/MI_Cubeless_UDSVolumetricCloud_V3_Reassembled`.
- Volumetric cloud functions use `/Game/Cubeless/SKY/V3/Functions/MF_Cubeless_UDSVolumetric_Clouds_Extinction_V3` and `/Game/Cubeless/SKY/V3/Functions/MF_Cubeless_UDSVolumetric_Clouds_Conservative_Density_V3`.
- Volumetric cloud shadow light function instance uses `/Game/Cubeless/SKY/V3/Materials/MI_Cubeless_UDSVolCloud_Shadows_V3`.
- Volumetric cloud shadow light function base material uses `/Game/Cubeless/SKY/V3/Materials/M_Cubeless_UDSVolCloud_Shadows_LightFunction_V3`.
- Post process wind/fog composite uses `/Game/Cubeless/SKY/V3/Materials/M_Cubeless_UDS_Post_Process_Wind_Fog_Composite_V3`.

## Notes

- The visible wispy clouds in the reference viewport are skybox/skydome texture clouds, not volumetric clouds. They are not valid evidence for volumetric cloud comparison.
- The natural saved reference state can redraw UDS volumetric `Cloud Density` to `0.0`; that is a skybox-only state and must not be treated as volumetric success.
- For volumetric comparison, enable realtime viewport, activate the Unreal Editor window, and compare only captures where volumetric clouds are actually visible.
- Controlled reference captures use runtime MPC sync from `/Game/Cubeless/SKY/V3/MPC/MPC_Cubeless_UDSVolumetricCloud_V3` to `/Game/UltraDynamicSky/Materials/Material_Functions/UDS_VolumetricClouds_MPC`.
- V3 VolumetricCloud component reflection sample scale values were matched to the reference component: `reflection_view_sample_count_scale_value=2.0` and `shadow_reflection_view_sample_count_scale_value=0.3`.
- The reference sky dome still contains wispy image clouds. V3 keeps `Wispy Cloud Alpha` at the reference value `0.5`.
- V3 material/function assets have no forbidden dependencies on `/Game/UltraDynamicSky/Materials`, `/Game/UltraDynamicSky/Blueprints`, or `/Game/_MCP_Temp`; UDS texture usage remains allowed for this pass.
- The light function material chain is Cubeless-owned. `MI_Cubeless_UDSVolCloud_Shadows_V3` parents to `M_Cubeless_UDSVolCloud_Shadows_LightFunction_V3`, and the copied light-function helper functions call only `/Game/Cubeless/SKY/V3/Functions` functions.
- The main cause of the previous weak V3 volumetric result was not the reassembled material graph. Applying the UDS source volumetric material inside the V3 world produced the same weak contribution. The missing reference environment values were Sun/Moon `cloud_scattered_luminance_scale`, directional-light light function settings, selected SkyAtmosphere absorption values, and selected HeightFog values.

## Verification

- Final volumetric material compiles: `M_Cubeless_UDSVolumetricCloud_V3_Reassembled`, `MF_Cubeless_UDSVolumetric_Clouds_Extinction_V3`, and `MF_Cubeless_UDSVolumetric_Clouds_Conservative_Density_V3` all compiled with `compile_error_count=0`.
- Final MPC sync status was disabled after verification.
- Collection parameter audit: `M_Cubeless_UDSVolumetricCloud_V3_Reassembled` has 21 V3 MPC collection nodes, extinction function has 14, conservative-density function has 20; all report `mismatched_id_count=0` and `missing_collection_parameter_count=0`.
- Light function compile audit: `M_Cubeless_UDSVolCloud_Shadows_LightFunction_V3` and its copied helper functions compiled and saved with `compile_error_count=0`.
- Light function package dependency audit: recursive hard/soft package dependencies from `MI_Cubeless_UDSVolCloud_Shadows_V3` and `M_Cubeless_UDSVolCloud_Shadows_LightFunction_V3` contain `0` `/Game/UltraDynamicSky/Materials` packages. Remaining UDS dependencies are texture packages only.
- Current dependency audit: `Saved/MCP/UDS_V3_Volumetric/dependency_audit_light_function_v3.json`.
- Dependency audit summary: light-function chain, V3 content root, and V3 level all report `forbidden_count=0`, `uds_material_count=0`, `uds_blueprint_count=0`, and `mcp_temp_count=0`. The V3 level still depends on `/Game/UltraDynamicSky/Meshes/Ultra_Dynamic_Sky_Sphere`, which is intentional for the UDS sky dome mesh.
- Final content root audit: `Saved/MCP/V3_content_root_audit_report.json`.
- Final map audit: `Saved/UDS_Analysis/V3_Comparisons/V3_final_map_audit.json`.
- Final clean comparison metrics: `Saved/UDS_Analysis/V3_Comparisons/comparison_metrics_clean.json`.
- Current controlled realtime skybox-free volumetric comparison:
  - comparison sheet: `Saved/MCP/UDS_V3_Volumetric/ActiveRealtime/COMPARE_REF_syncV3_vs_V3_envLightFix_settled_active_rt_cvar_skyhidden_panel_review_opaque.png`
  - metrics: `Saved/MCP/UDS_V3_Volumetric/ActiveRealtime/comparison_metrics_REF_syncV3_vs_V3_envLightFix_settled_active_rt_cvar_skyhidden.json`
  - settled REF mean contribution: `7.4817`
  - settled V3 mean contribution: `7.5127`
  - settled V3/REF mean ratio: `1.0041`
  - settled mean abs contribution difference: `1.5839/255`

## Residual Editor-Only Difference

The closest volumetric comparison is the settled realtime capture with sky dome hidden and `r.VolumetricCloud` on/off contribution isolation. Small residual edge and temporal sampling differences remain visible in the contribution diff panel. Do not judge this pass from screenshots where the reference volumetric cloud contribution is absent.
