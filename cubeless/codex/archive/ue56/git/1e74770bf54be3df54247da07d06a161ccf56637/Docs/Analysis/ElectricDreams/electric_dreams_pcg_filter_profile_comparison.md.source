# Electric Dreams PCG Filter Profile Comparison

Date: 2026-06-09

## Scope

Source data:

- `electric_dreams_pcg_graph_summaries.json`
- `electric_dreams_pcg_ditch_ground_hierarchy_usage_analysis.md`
- Cubeless `_MCP_Temp` study graph passes through:
  - `electric_dreams_pcg_cubeless_side_mask_profile_batch_validation.md`
  - `electric_dreams_pcg_cubeless_density_subtree_pruning_pass.md`

This pass is analysis only. No Unreal assets, C++, or `_MCP_Temp` graph assets were changed.

## Electric Dreams Filter Counts

### PCGDemo_Ditch

Relevant settings counts from the extracted graph summary:

| Settings Class | Count | Meaning For Cubeless Learning |
| --- | ---: | --- |
| `PCGAttributeFilteringSettings` | 57 | Main branch/point selection mechanism |
| `PCGAttributeNoiseSettings` | 23 | Density and variation noise before branch decisions |
| `PCGDensityFilterSettings` | 2 | Present but not dominant in Ditch |
| `PCGFilterByTagSettings` | 4 | Actor/tag routing for graph sections |
| `PCGSelfPruningSettings` | 1 | Minor compared with Ground |
| `PCGFilterByAttributeSettings` | 1 | Attribute-level routing edge case |

Key-node signals include repeated `PointFilter_*` and `DensityNoise_*` nodes around river embankment and vegetation subgraph calls.

### PCGDemo_Ground

Relevant settings counts from the extracted graph summary:

| Settings Class | Count | Meaning For Cubeless Learning |
| --- | ---: | --- |
| `PCGDensityFilterSettings` | 29 | Dominant density acceptance/rejection mechanism |
| `PCGAttributeNoiseSettings` | 20 | Heavy density variation/noise usage |
| `PCGBoundsModifierSettings` | 16 | Bounds shaping before spatial operations |
| `PCGAttributeFilteringSettings` | 9 | Attribute filters still present, but not dominant |
| `PCGSelfPruningSettings` | 7 | Important overlap/spacing control |
| `PCGDensityRemapSettings` | 4 | Density curve/remap behavior not yet represented in Cubeless study graph |
| `PCGFilterByTagSettings` | 2 | Tag routing |
| `PCGNormalToDensitySettings` | 1 | Terrain-normal-to-density behavior |

Ground is therefore a density-field and pruning study target more than a hierarchy-copy study target.

## Cubeless Study Graph Mapping

| Cubeless Control | Current Implementation | Electric Dreams Match | Gap |
| --- | --- | --- | --- |
| `SideMask` | `PCGAttributeFilteringSettings` on copied points | Matches Ditch-style point/attribute filters | Needs real side/terrain/actor-derived attribute instead of hand-authored side labels |
| Side profiles | all, left+center, center, right+center, center+right | Matches Ditch-style branch routing | Needs user-facing naming and possibly asymmetric Ditch-specific presets |
| `BranchDensity` noise | `PCGAttributeNoiseSettings` multiply range `0.98-1.02` | Matches Ditch/Ground density noise pattern | Needs extracted real noise ranges/settings from Electric Dreams assets |
| `leaf_mud_only` pruning | Attribute noise + filter removes a single leaf | Matches safe local point rejection | Simplified; Electric Dreams likely uses multiple filters and spatial masks |
| `right_upper_subtree` pruning | Effective density override removes parent+child subtree | Demonstrates safe hierarchy-aware pruning | Needs a real subtree/group tag rather than name-based override |
| Latest verifier | Counts, density distributions, parent gaps, ISM total, log scan | Stronger than source summary for Cubeless temp graph | Does not yet inspect visual spacing/overlap |

## Interpretation

Ditch and Ground use different filter styles:

- Ditch is closer to the current Cubeless study path:
  - many attribute filters
  - many attribute noise nodes
  - many `SG_CopyPointsWithHierarchy` calls
  - branch assembly routing before/around hierarchy copy
- Ground is the next stage after Ditch:
  - density filters dominate
  - self-pruning appears repeatedly
  - density remap and normal-to-density behavior become important
  - hierarchy copy is less central

The current Cubeless graph is now good enough for Ditch-style learning because it has:

- copied source assemblies
- original post-copy hierarchy key offset Blueprint
- graph-side side filtering
- density noise
- destructive density filtering
- safe leaf pruning
- safe subtree pruning
- reusable verification for parent gaps and final point counts

It is not yet a faithful Ground-style graph because it lacks:

- `PCGDensityFilterSettings`
- `PCGDensityRemapSettings`
- `PCGSelfPruningSettings`
- bounds expansion/shrink stages
- normal-to-density terrain influence
- overlap/spacing validation

## Recommended Next Implementation

The next non-approval implementation step should add a Ground-style density/self-pruning branch to `_MCP_Temp` while keeping the Ditch hierarchy graph intact.

Recommended order:

1. Add a dedicated `PCGDensityFilterSettings` pass after `BranchDensityNoised`.
2. Add a small, deterministic `PCGSelfPruningSettings` experiment after density filtering.
3. Extend the verifier to check expected count ranges and minimum spacing rather than exact source-count multiplication for the self-pruned branch.
4. Keep hierarchy validation active so self-pruning does not silently create parent gaps.

## Approval Boundary

No approval is needed for continued `_MCP_Temp` experiments and sibling analysis docs.

Approval should be requested before:

- moving controls into real Cubeless project-facing PCG assets outside `_MCP_Temp`
- changing production Electric Dreams-derived assets
- adding non-plugin project C++
- exposing these settings as designer-facing UI or saved project parameters
