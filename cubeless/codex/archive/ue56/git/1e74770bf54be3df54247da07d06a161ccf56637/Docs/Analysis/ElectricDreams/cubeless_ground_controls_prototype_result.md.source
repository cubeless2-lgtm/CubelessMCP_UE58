# Cubeless Ground Controls Prototype Result

Date: 2026-06-09

## Scope

Created the first non-temp Cubeless PCG prototype from the Electric Dreams PCG
learning pass.

Production graph:

```text
/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_GroundControlsPrototype
```

Validation actor:

```text
/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP
MCP_Cubeless_GroundControlsPrototype_Validation
```

The validation actor is disposable. The graph asset is the production candidate.

## Implemented Controls

The graph keeps the first pass intentionally narrow:

```text
CreatePoints
-> BranchDensity / SideMask / DitchDensityThreshold metadata
-> AttributeFilter on $Density >= 0.3
-> DitchStyleCandidatePass marker
-> DensityRemap 0.0..1.0 -> 0.5..1.0
-> DensityFilter 0.5..1.0
-> BoundsModifier SET
-> SelfPruning ALL_EQUAL, radius 0.25, randomized false
-> CubelessGroundControlsPass marker
-> Merge profile outputs
-> StaticMeshSpawner using SM_Grass_Medium01
```

Electric Dreams-derived candidate values used:

```text
Ditch density threshold: 0.3
Ground density remap: 0.0..1.0 -> 0.5..1.0
Ground density filter: 0.5..1.0
Bounds small_set: (-25,-25,-25) -> (25,25,25)
Bounds medium_set: (-50,-50,-50) -> (50,50,50)
Self pruning: ALL_EQUAL, radius 0.25
```

## Validation Result

Final verifier:

```text
build script:  build_cubeless_ground_controls_prototype.py
verify script: verify_cubeless_ground_controls_prototype.py
```

Result:

```text
point_count=8
profile_counts={'ed_small_set': 5, 'ed_medium_set': 3}
profile_seeds={'ed_small_set': [31, 32, 33, 34, 35], 'ed_medium_set': [32, 33, 35]}
total_ism_instances=8
grass_mesh=/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01
unknown_profile_count=0
unexpected_seed_count=0
density_mismatch_count=0
branch_density_mismatch_count=0
threshold_mismatch_count=0
side_mask_mismatch_count=0
bounds_settings_mismatch_count=0
log_error_count=0
cubeless_ground_controls_prototype_validation_pass=True
```

The generated output row bounds are post-`StaticMeshSpawner` mesh bounds, not the
raw `BoundsModifier` values. The verifier checks authored `BoundsModifier`
settings directly from the graph asset and uses the survivor count difference
between the small and medium profiles as the runtime pruning proof.

## Fixes During Execution

- `PCGAttributeFilteringSettings` output had to use `InsideFilter`; connecting
  an `Out` pin silently left the downstream graph disconnected.
- The production filter target was corrected to `$Density`, matching extracted
  Electric Dreams attribute filter selectors.
- `BranchDensity` is now explicit profile metadata instead of being copied from
  point density. Runtime culling is driven by `$Density`, and the Ditch-style
  profile signal remains available for the next assembly pass.
- The verifier no longer compares generated output bounds after
  `StaticMeshSpawner` against authored `BoundsModifier` values, because the
  spawner replaces point bounds with the spawned mesh bounds.

## Next Approval Boundary

The next meaningful step is a Ditch-style hierarchy/copy production graph:

```text
source hierarchy attributes
-> copy points onto spline/target points
-> post-copy ActorIndex/ParentIndex offset
-> ApplyHierarchy
-> same Ditch/Ground filters
```

That likely needs either a production Blueprint helper equivalent to the current
temp post-copy offset helper or a plugin-side/custom-node implementation. It is
the next approval boundary before touching broader production PCG assets.
