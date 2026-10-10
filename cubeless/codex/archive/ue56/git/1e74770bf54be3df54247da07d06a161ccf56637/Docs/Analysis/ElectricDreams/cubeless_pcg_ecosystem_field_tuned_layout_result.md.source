# Cubeless PCG Ecosystem Field Tuned Layout Result

Date: 2026-06-10

## Scope

Updated the saved field level:

`/Game/Cubeless/Map/LVL_Cubeless_PCG_Ecosystem_Field`

The field still uses only the validated runtime Blueprint:

`/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime`

No original Electric Dreams assets, learning graphs, `/Game/PCG`,
`RuntimeGrass`, `NewPCGGraph`, or non-exception C++ were modified.

## Tuned Layout

| Actor | Preset | Overrides | Instances |
| --- | --- | --- | ---: |
| `Cubeless_PCG_EcosystemRuntime_DenseMeadowWest` | `MixedMeadowDefault` | `DensityOverride=3` | 101 |
| `Cubeless_PCG_EcosystemRuntime_DenseMeadowCenter` | `MixedMeadowDefault` | `DensityOverride=3` | 101 |
| `Cubeless_PCG_EcosystemRuntime_DenseMeadowEast` | `MixedMeadowDefault` | `DensityOverride=3` | 101 |
| `Cubeless_PCG_EcosystemRuntime_GroundFoliageSouthWestWarm` | `DenseGroundFoliage` | `TreeOverride=1`, `MaterialMood=3` | 58 |
| `Cubeless_PCG_EcosystemRuntime_GroundFoliageSouthWarm` | `DenseGroundFoliage` | `TreeOverride=1`, `MaterialMood=3` | 58 |
| `Cubeless_PCG_EcosystemRuntime_GroundFoliageSouthEastWarm` | `DenseGroundFoliage` | `TreeOverride=1`, `MaterialMood=3` | 58 |
| `Cubeless_PCG_EcosystemRuntime_RockyCoolEdgeEastNorth` | `RockySparse` | `MaterialMood=2` | 3 |
| `Cubeless_PCG_EcosystemRuntime_RockyCoolEdgeEastSouth` | `RockySparse` | `MaterialMood=2` | 3 |
| `Cubeless_PCG_EcosystemRuntime_ConiferGroveNorthWest` | `LightConiferEdge` | `TreeOverride=4`, `MaterialMood=3` | 29 |
| `Cubeless_PCG_EcosystemRuntime_ConiferGroveNorthEast` | `LightConiferEdge` | `TreeOverride=4`, `MaterialMood=3` | 29 |

## Validation

- Total field instances: `541`
- Top-down QA bounds: `44.2m x 23.0m`
- Top-down QA category counts: `300` meadow grass, `174` warm foliage/flowers,
  `61` conifer, and `6` rock instances
- Missing actors: `0`
- Landscape trace misses: `0`
- Height failures: `0`
- XY failures: `0`
- Latest marker log errors: `0`
- Save result: `field_layout_refine_saved=True`
- Dirty packages after save: `[]`
- Read-only regression: `ecosystem_field_level_verify|PASS|0.328s`
- Top-down QA regression: `ecosystem_field_topdown_qa|PASS|0.114s`
- QA artifacts:
  `D:\Git\CubelessStylized\Saved\MCP_Screenshots\pcg_field_broad_patch_topdown.json`,
  `D:\Git\CubelessStylized\Saved\MCP_Screenshots\pcg_field_broad_patch_topdown.svg`,
  and `D:\Git\CubelessStylized\Saved\MCP_Screenshots\pcg_field_broad_patch_topdown.png`

## Interpretation

This replaces the earlier 88-instance field with a broader, more readable
ecosystem patch while keeping the validated runtime Blueprint and learning
graph references intact. The layout is still conservative: three dense meadow
rows, three warm low-foliage rows, two rocky east accents, and two conifer
edge actors.
