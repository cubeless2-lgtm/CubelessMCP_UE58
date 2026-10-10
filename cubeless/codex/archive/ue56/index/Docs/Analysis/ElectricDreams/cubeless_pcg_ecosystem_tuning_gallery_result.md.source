# Cubeless PCG Ecosystem Tuning Gallery Result

Date: 2026-06-10

## Scope

Built a disposable tuning gallery under:

`/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_TuningGallery_MCP`

The gallery uses the saved runtime Blueprint:

`/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime`

No original Electric Dreams assets, learning graphs, `/Game/PCG`,
`RuntimeGrass`, `NewPCGGraph`, non-exception C++, or saved production field
packages were modified by the tuning gallery step.

## Error And Fix

The first gallery prepare attempt crashed Unreal Editor with:

`Old world ... not cleaned up by garbage collection while loading new map`

Cause:

- The script duplicated the temp level and then loaded it while a Python local
  still referenced the duplicated `World` object.

Fix:

- Clear the duplicated World reference before `load_level`.
- Reuse an existing `_MCP_Temp` tuning gallery map instead of deleting and
  re-duplicating it inside the same editor process.

After the fix, the gallery prepare and verify passes completed without editor
crash.

## Gallery Cases

| Case | Instances | Result |
| --- | ---: | --- |
| `MixedMeadowDefault` | 27 | Pass |
| `DenseGroundFoliage_TreeOff` | 58 | Pass |
| `RockySparse` | 3 | Pass |
| `LightConiferEdge` | 29 | Pass |
| `ClassicGrassFill` | 16 | Pass |
| `MeadowDenseOverride` | 101 | Pass |
| `GroundFoliageWarm` | 58 | Pass |
| `RockyCoolDark` | 3 | Pass |
| `ConiferLightGrove` | 29 | Pass |

## Validation

- Total gallery instances: `324`
- Missing actors: `0`
- Landscape trace misses: `0`
- Height failures: `0`
- XY failures: `0`
- Latest marker log errors: `0`
- Dirty packages after verify/save: `[]`
- Result: `field_layout_refine_validation_pass=True`

## Follow-Up Decision

The best next production field direction was selected as a balanced
four-actor layout:

- dense meadow center for readable ground coverage
- warm ground foliage south patch for flowers/leaves
- cool rocky east accent
- light conifer northwest edge for vertical structure

That selected layout was applied separately by
`verify_save_cubeless_pcg_ecosystem_field_tuned_layout.py`.
