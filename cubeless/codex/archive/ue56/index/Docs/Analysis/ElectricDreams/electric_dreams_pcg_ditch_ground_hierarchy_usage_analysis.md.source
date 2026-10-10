# Electric Dreams PCG Ditch/Ground Hierarchy Usage Analysis

Date: 2026-06-09

## Scope

Source data:

- `electric_dreams_pcg_graph_summaries.json`
- `electric_dreams_pcg_asset_index.json`
- `sg_copy_points_with_hierarchy_details.json`

This pass is analysis only. No Unreal assets or C++ were changed.

## Confirmed Subgraph Pattern

`SG_CopyPointsWithHierarchy` is a small reusable hierarchy-copy graph:

1. `CopyPoints`
2. `PostCopyPoints-OffsetIndices` Blueprint
3. Add `IgnoreParentRotation`
4. Add `IgnoreParentScale`
5. Output

Its input contract is:

- `CopyPoints Source`
- `CopyPoints Target`

Its purpose is to copy a local assembly onto target points while preserving per-copy parent/child relationships through offset `ActorIndex` and `ParentIndex` keys.

## Usage Count

| Graph | SG_CopyPointsWithHierarchy calls | Total nodes | Edge refs | Notes |
| --- | ---: | ---: | ---: | --- |
| `PCGDemo_Ditch` | 21 | 413 | 954 | Best next learning target |
| `PCGDemo_Ground` | 1 | 237 | 544 | Later target; mostly other scatter/copy logic |
| `SplineExampleGraph` | 1 | 22 | 50 | Already represented by the Cubeless spline study graph |

## Ditch Blueprint Assembly Mix

`PCGDemo_Ditch` uses these PCG assembly Blueprint categories:

| Category | Count | Representative assets |
| --- | ---: | --- |
| River embankment | 7 | `ASM_RiverEmbankment_00_PCG`, `01`, `02`, `02A`, `02B`, `03`, `04` |
| Vegetation | 6 | `ASM_HornBeamJungle_01_PCG`, `02`, `04`, `05`, `ASM_FlowerBush_00_PCG` |
| Rock/cliff | 3 | `ASM_SmallCliff_02_PCG`, `ASM_Perimeter_Cliff_01_PCG`, `ASM_LargeBoulder_01A_PCG` |
| River surrounding floor | 1 | `ASM_RiverSurroundingFloor_00_PCG` |
| Inline Blueprint helpers | 16 | graph-local helper Blueprints inside `PCGDemo_Ditch` |

## Ditch Call Groups

The extracted summary does not include a full edge map for the parent Ditch graph, so these groupings are based on key-node order and nearby Blueprint/filter/transform nodes. Treat them as learning-map guidance, not exact edge reconstruction.

| SG nodes | Nearby assembly class | Local flow signal |
| --- | --- | --- |
| `Subgraph_9`, `11`, `12`, `4`, `10`, `45` | River embankment | spline sampling, density noise, point filters, transform branches |
| `Subgraph_71`, `84` | River surrounding floor / inline helper | transform, secondary spline sampler, difference masks |
| `Subgraph_135`, `137`, `140`, `147` | Vegetation | point filters, transform variation, density noise before HornBeam assemblies |
| `Subgraph_164`, `167` | Rock/cliff | multi-transform branch feeding cliff/boulder assemblies |
| `Subgraph_13` | River embankment variant | actor/tag filtering, density noise, transform/copy attribute follow-up |
| `Subgraph_39`, `3`, `23`, `37`, `38`, `40` | Unknown from summary | listed as subgraph calls but omitted from the key-node context excerpt |

## Ground Comparison

`PCGDemo_Ground` has only one `SG_CopyPointsWithHierarchy` call. Its Blueprint assembly mix is:

- Ground assemblies:
  - `PL_MediumRiverbedEmbankment_01_PCG`
  - `Asmbl_RiverbedRocks_00_PCG`
  - `PL_DriedRapid_01_PCG`
  - `PCG_GroundBump_01_PCG`
- Other assemblies:
  - `RockLayer2_PCG`
  - `SmallAssembly_PCG`
- Inline helpers:
  - two graph-local Blueprint settings

Ground is useful after Ditch, but it is less useful for learning the hierarchy-copy pattern because most of its complexity is density filters, bounds modifiers, self-pruning, ordinary copy points, and spawners.

## Recommendation

The next Cubeless implementation pass should target a Ditch-style river embankment study preset:

1. Keep using `_MCP_Temp` assets only.
2. Keep the original imported `PostCopyPoints-OffsetIndices` Blueprint until a Cubeless-owned replacement is built.
3. Add a Ditch/RiverBank source assembly preset to the Cubeless study builder.
4. Add script-level controls first:
   - `SOURCE_ASSEMBLY_PRESET`
   - target spacing or spline sampler count
   - optional branch transform offsets
5. Validate with the same checks:
   - generated point count
   - root count
   - depth counts
   - unique `ActorIndex`
   - missing parent count
   - parent-depth mismatch count
   - ISM instance count
   - latest log `Error:` scan

Do not start with Ground. Ditch has enough repeated hierarchy-copy usage to teach the pattern; Ground should come after the river embankment/vegetation/cliff variants are understood.

## Follow-Up

Implemented in `electric_dreams_pcg_cubeless_ditch_riverbank_preset_pass.md`.

- Added `SOURCE_ASSEMBLY_PRESET = "ditch_riverbank"` to the Cubeless study builder.
- Preserved the previous `learning_tree` preset.
- Added script-level target sampling controls.
- Verified 12 source points copied onto 6 target points:
  - output point count: `72`
  - depth counts: `{0: 6, 1: 18, 2: 36, 3: 12}`
  - root groups: `[12, 12, 12, 12, 12, 12]`
  - generated ISM instances: `72`
  - latest log `ERROR_MATCH_COUNT=0`
