# Electric Dreams PCG Cubeless Original Settings Pass Result

Date: 2026-06-09

## Scope

- Source project: `D:\Git\SampleProject\ElectricDreamsEnv`
- Source PCG graph: `/Game/PCG/Assets/PCGCustomNodes/SG_CopyPointsWithHierarchy`
- Source Blueprint element: `/Game/PCG/Assets/PCGCustomNodes/PostCopyPoints-OffsetIndices.PostCopyPoints-OffsetIndices_C`
- Cubeless temp graph: `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Cubeless test level: `/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

## Source Findings

The commandlet export succeeded and wrote:

- `sg_copy_points_with_hierarchy_details.json`
- `export_sg_copy_points_with_hierarchy_details.py`

`SG_CopyPointsWithHierarchy` contains this confirmed node chain:

1. `CopyPoints_0` (`PCGCopyPointsSettings`)
2. `ExecuteBlueprint_1` (`PCGBlueprintSettings`)
3. `CreateAttribute_0` (`PCGAddAttributeSettings`)
4. `CreateAttribute_4` (`PCGAddAttributeSettings`)

Confirmed source `CopyPoints_0` settings:

- `rotation_inheritance = RELATIVE`
- `scale_inheritance = RELATIVE`
- `color_inheritance = RELATIVE`
- `seed_inheritance = RELATIVE`
- `attribute_inheritance = SOURCE_ONLY`
- `tag_inheritance = BOTH`
- `copy_each_source_on_every_target = True`

Confirmed post-Blueprint source attributes:

- `CreateAttribute_0`: `IgnoreParentRotation = false`
- `CreateAttribute_4`: `IgnoreParentScale = false`

`ExecuteBlueprint_1` uses `PostCopyPoints-OffsetIndices_C` with pins:

- Input: `CopyPointsOut`
- Input: `CopyPointsTarget`
- Output: `Out`
- Overrides visible in settings: `Seed`, `AddRandomOffset`

The Blueprint graph names were found through `BlueprintEditorLibrary.find_graph`, but commandlet mode exposed zero graph nodes for `EventGraph`, `ExecuteWithContext`, and `IterationLoopBody`.

Raw asset strings from `PostCopyPoints-OffsetIndices.uasset` show the Blueprint works with:

- `ActorIndex`
- `ParentIndex`
- `AttributesToOffset`
- `CopiesCount`
- `CopyPointsOut`
- `CopyPointsTarget`
- `TotalPointCount`
- `RandomOffset`
- `AddRandomOffset`
- `GetInteger64Attribute`
- `SetInteger64Attribute`
- int64 add, multiply, modulo/percent, compare, and loop nodes

Inference: the source Blueprint offsets integer hierarchy keys after `CopyPoints` so copied assemblies keep unique `ActorIndex`/`ParentIndex` ranges per target copy. The exact node graph/formula was not recoverable through commandlet API in this pass.

## Cubeless Changes

Applied only to `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`.

- Set `CopyPoints_0.attribute_inheritance` from `SOURCE_FIRST` to `SOURCE_ONLY`.
- Reaffirmed original `CopyPoints_0` relative inheritance settings and `copy_each_source_on_every_target = true`.
- Added `15A IgnoreParentRotation False`:
  - `output_target = IgnoreParentRotation`
  - `Type = Boolean`
  - `BoolValue = false`
- Added `15B IgnoreParentScale False`:
  - `output_target = IgnoreParentScale`
  - `Type = Boolean`
  - `BoolValue = false`
- Rewired local downstream chain:
  - `14 Merge Two-Level Hierarchy Branches`
  - `15A IgnoreParentRotation False`
  - `15B IgnoreParentScale False`
  - `15 Apply Two-Level Hierarchy`
- Set `ApplyHierarchy_3.apply_parent_rotation = OPT_OUT_BY_ATTRIBUTE`.
- Set `ApplyHierarchy_3.apply_parent_rotation_attribute = IgnoreParentRotation`.
- Set `ApplyHierarchy_3.apply_parent_scale = OPT_OUT_BY_ATTRIBUTE`.
- Set `ApplyHierarchy_3.apply_parent_scale_attribute = IgnoreParentScale`.

This mirrors the confirmed source behavior where parent rotation/scale are applied unless the point explicitly opts out.

## Verification

- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success
- Regeneration marker: `MCP_PCG_ORIGINAL_SETTINGS_PASS_REGEN_BEGIN`
- Initial count immediately after generation: 0 generated ISM components, because PCG generation completed asynchronously.
- Count after 3 seconds:
  - `generated_ism_component_count = 1`
  - component: `ISM_SM_Grass_Medium01_0`
  - mesh: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`
  - instances: `20`
- Latest `Saved/Logs/StylizedCubeless.log` scan after marker:
  - `ERROR_MATCH_COUNT = 0`

Warnings observed:

- Unreal Python deprecation warnings for `EditorLevelLibrary.get_all_level_actors`.
- The warnings did not block compile, save, generation, or log validation.

## Residual Risk

The exact `PostCopyPoints-OffsetIndices` Blueprint formula is still inferred, not reconstructed. To reproduce that part exactly, the next pass should either inspect the Blueprint in a full editor graph context or build a temporary equivalent that offsets `ActorIndex` and `ParentIndex` by copy range after `CopyPoints`.
