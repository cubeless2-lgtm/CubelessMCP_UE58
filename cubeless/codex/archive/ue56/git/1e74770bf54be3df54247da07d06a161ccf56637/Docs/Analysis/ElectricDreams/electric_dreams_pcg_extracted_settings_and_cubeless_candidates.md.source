# Electric Dreams PCG Extracted Settings And Cubeless Candidates

Date: 2026-06-09

## Scope

- Extraction script:
  - `extract_electric_dreams_pcg_settings.py`
- Output:
  - `electric_dreams_pcg_extracted_settings.json`
- Reference project:
  - `D:\Git\SampleProject\ElectricDreamsEnv\ElectricDreamsEnv.uproject`

The extraction was first attempted from the Cubeless editor context. That correctly produced `loaded=False` because the Electric Dreams `/Game/PCG/...` assets are not mounted in Cubeless. The same script was then run headless against `ElectricDreamsEnv` through `UnrealEditor-Cmd.exe -run=pythonscript`, and the graph assets loaded successfully.

## Commandlet Result

Commandlet target:

```text
C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
D:\Git\SampleProject\ElectricDreamsEnv\ElectricDreamsEnv.uproject
```

Extraction result:

```text
PCGDemo_Ditch loaded=True nodes=411 extracted=86
PCGDemo_Ground loaded=True nodes=235 extracted=85
DiscardPointsInBumpyAreas loaded=True nodes=50 extracted=5
commandlet result=success, 0 errors, 0 warnings
```

## Ditch Extracted Values

`PCGDemo_Ditch` remains mostly attribute-filter and attribute-noise driven.

Key extracted settings:

```text
PCGAttributeFilteringSettings: 57
PCGAttributeNoiseSettings: 23
PCGDensityFilterSettings: 2
PCGBoundsModifierSettings: 3
PCGSelfPruningSettings: 1
```

Density filters:

```text
0.25..0.40
0.00..0.20
```

Attribute noise:

```text
23x SET density noise, 0.0..1.0, input=$Density, output=@Source
```

Bounds modifiers:

```text
SET   (-25, -25, -25) -> (25, 25, 25)
SCALE (0.75, 1.0, 1.0)
SCALE (2.0, 1.0, 1.0)
```

Self-pruning:

```text
pruning_type=ALL_EQUAL
comparison_source=$Extents
radius_similarity_factor=0.25
randomized_pruning=True
use_collision_attribute=False
```

Common attribute-filter signals include:

```text
$Density GREATER thresholds around 0.3, 0.365, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9
Clutter / RotZ / ParentRot / Cull / CullLeft / Flower / Collider / Hanging style boolean or integer selectors
```

## Ground Extracted Values

`PCGDemo_Ground` is density, bounds, and pruning heavy.

Key extracted settings:

```text
PCGDensityFilterSettings: 29
PCGAttributeNoiseSettings: 20
PCGBoundsModifierSettings: 16
PCGAttributeFilteringSettings: 9
PCGSelfPruningSettings: 7
PCGDensityRemapSettings: 4
```

Most frequent density filters:

```text
0.50..1.00  count=3
0.25..0.75  count=2
```

Other important density bands:

```text
0.00..0.015
0.00..0.20
0.00..0.25
0.00..0.33
0.00..0.50
0.00..0.55
0.00..1.00
0.05..0.20
0.10..0.30
0.10..0.50
0.10..0.67
0.10..0.70
0.10..0.75
0.20..0.80
0.25..0.75
0.40..0.45
0.40..0.60
0.40..1.00
0.50..1.00
0.60..0.80
0.60..0.99
0.66..1.00
0.70..1.00
0.80..1.00
0.95..1.00
```

One extracted density filter has `lower_bound=1.0` and `upper_bound=0.975`. Treat that as an Electric Dreams authored edge case, not a Cubeless default candidate.

Density remaps:

```text
0.10..0.50 -> 1.0..0.0
0.20..0.75 -> 1.0..0.0
0.00..1.00 -> 0.5..1.0
0.50..1.00 -> 0.0..1.0
```

Bounds modifiers:

```text
SET   (-25, -25, -25) -> (25, 25, 25)          count=2
SET   (-30, -30, -35) -> (30, 30, 35)
SET   (-50, -50, -50) -> (50, 50, 50)
SET   (-175, -5, -10) -> (175, 5, 15)
SET   (-300, -110, -200) -> (270, 110, 200)
SET   (-500, -600, -100) -> (500, 600, 100)
SCALE (0.1, 0.1, 1.0)
SCALE (0.3, 0.2, 0.5) / (0.2, 0.4, 0.5)
SCALE (0.9, 0.9, 0.95) / (0.9, 0.9, 0.9)
SCALE (1.5, 1.5, 1.0)                         count=2
SCALE (2.0, 2.0, 2.0)
SCALE (4.0, 2.0, 1.0)
SCALE (4.0, 4.0, 4.0) / (4.0, 4.0, 1.0)
```

Self-pruning:

```text
4x ALL_EQUAL, radius_similarity_factor=0.25, randomized_pruning=True
2x ALL_EQUAL, radius_similarity_factor=0.25, randomized_pruning=False
1x ALL_EQUAL, radius_similarity_factor=0.0, randomized_pruning=True
comparison_source=$Extents
use_collision_attribute=False
```

Attribute noise:

```text
14x SET      0.0..1.0
3x  MULTIPLY 0.5..1.0
2x  MULTIPLY 0.0..1.0
1x  MULTIPLY 0.25..1.0
```

Attribute filters include:

```text
$Density GREATER 0.3
$Density GREATER 0.5
DistanceLength EQUAL 0
DistanceLength GREATER 600
$Position.z GREATER_OR_EQUAL 1655
Clutter / Rocks / SelectedRock selectors
```

## Forest Helper Extracted Values

`DiscardPointsInBumpyAreas` contributes useful bounds references:

```text
PCGBoundsModifierSettings: 4
PCGDensityRemapSettings: 1
```

Bounds:

```text
SET (-200, -200, -1) -> (200, 200, 1)  count=2
SET (-10, -10, -10) -> (10, 10, 10)
SET (-50, -50, -50) -> (50, 50, 50)
```

Density remap:

```text
0.0..1.0 -> 1.0..1.0
```

## Cubeless Candidate Defaults

These are not production changes yet. They are candidate values for the next approved Cubeless PCG asset pass.

### Ditch-Style Candidates

```text
BranchDensityNoiseMode: SET or MULTIPLY
Exploration noise ranges:
  SET 0.0..1.0
  MULTIPLY 0.5..1.0
  MULTIPLY 0.25..1.0
Attribute density thresholds:
  0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9
Side/branch selectors:
  SideMask, ClutterMask, RockMask, CullMask, RotationGroup
```

The current Cubeless `SideMask` and `BranchDensity` study graph maps well to this category.

### Ground-Style Candidates

```text
Primary DensityFilter presets:
  all: 0.0..1.0
  low: 0.0..0.25 or 0.0..0.33
  mid: 0.25..0.75
  high: 0.5..1.0
  peak: 0.8..1.0 or 0.95..1.0

DensityRemap presets:
  inverse_soft: 0.10..0.50 -> 1.0..0.0
  inverse_wide: 0.20..0.75 -> 1.0..0.0
  boost_high: 0.00..1.00 -> 0.5..1.0
  high_only_ramp: 0.50..1.00 -> 0.0..1.0

Bounds presets:
  small_set: +/-25
  medium_set: +/-50
  terrain_strip: (-175,-5,-10) -> (175,5,15)
  broad_ground: (-300,-110,-200) -> (270,110,200)
  huge_ground: (-500,-600,-100) -> (500,600,100)
  shrink_xy: scale 0.1,0.1,1.0
  widen_xy: scale 1.5,1.5,1.0
  large_xy: scale 4.0,2.0,1.0 or 4.0,4.0,1.0

SelfPruning presets:
  deterministic_equal: ALL_EQUAL, radius 0.25, randomized=False
  organic_equal: ALL_EQUAL, radius 0.25, randomized=True
  strict_equal: ALL_EQUAL, radius 0.0, randomized=True
```

The current Cubeless Ground combo fixture already proves:

```text
DensityRemap -> BoundsModifier -> SelfPruning
tight bounds: 6 survivors
expanded bounds: 3 survivors
```

## Approval Boundary

The next meaningful implementation is moving a selected subset of these candidate controls into real Cubeless PCG assets or designer-facing parameters.

That requires user approval because it leaves disposable `_MCP_Temp` learning assets and starts affecting production project content.
