# Cubeless PCG Intent Gallery Result

Date: 2026-06-10

## Purpose

Create the first intent-based staging layer for making user-requested PCG
without editing production levels directly.

The intent gallery lives under:

`/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP`

It uses the validated runtime Blueprint:

`/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime`

No original Electric Dreams assets, learning graphs, `/Game/PCG`,
`RuntimeGrass`, `NewPCGGraph`, production field packages, or non-exception C++
were modified.

## Supported First Intents

| Intent | Meaning | Runtime Translation |
| --- | --- | --- |
| `MeadowPatch` | Dense mixed meadow with a small conifer punctuation point | `PresetType=1`, `DensityOverride=3` |
| `FlowerBand` | Warm low foliage and flowers with trees disabled | `PresetType=2`, `TreeOverride=1`, `MaterialMood=3` |
| `RockEdge` | Cool sparse rocks for an ecosystem border | `PresetType=3`, `MaterialMood=2` |
| `ConiferEdge` | Light conifer edge with grass undergrowth | `PresetType=4`, `TreeOverride=4`, `MaterialMood=3` |
| `BalancedEcosystem` | Combined meadow, flowers, rocks, and conifer edge | Multi-actor recipe combining the above |

## Validation Result

- Intent actors: `9`
- Total instances: `483`
- Missing actors: `0`
- Landscape trace misses: `0`
- Height failures: `0`
- XY failures: `0`
- Latest marker log errors: `0`
- Saved temp gallery: `field_layout_refine_saved=True`
- Dirty packages after save: `[]`
- Regression runner step: `intent_gallery_verify|PASS|0.328s`

## Instance Counts

| Actor | Intent | Instances |
| --- | --- | ---: |
| `Cubeless_PCG_Intent_MeadowPatch_DenseA` | `MeadowPatch` | 101 |
| `Cubeless_PCG_Intent_FlowerBand_WarmA` | `FlowerBand` | 58 |
| `Cubeless_PCG_Intent_RockEdge_CoolA` | `RockEdge` | 3 |
| `Cubeless_PCG_Intent_ConiferEdge_LightA` | `ConiferEdge` | 29 |
| `Cubeless_PCG_Intent_Balanced_MeadowWest` | `BalancedEcosystem` | 101 |
| `Cubeless_PCG_Intent_Balanced_FlowerSouth` | `BalancedEcosystem` | 58 |
| `Cubeless_PCG_Intent_Balanced_MeadowEast` | `BalancedEcosystem` | 101 |
| `Cubeless_PCG_Intent_Balanced_RockEast` | `BalancedEcosystem` | 3 |
| `Cubeless_PCG_Intent_Balanced_ConiferNorth` | `BalancedEcosystem` | 29 |

## User Request Mapping

Use these phrases as the first routing vocabulary:

- "꽃 많은 초원", "꽃밭", "낮은 식생": `FlowerBand`
- "넓은 초원", "잔디 군락", "풀밭": `MeadowPatch`
- "바위 가장자리", "돌 주변", "바위 띠": `RockEdge`
- "침엽수 경계", "작은 숲", "나무 라인": `ConiferEdge`
- "초원에 꽃이랑 바위랑 나무 조금": `BalancedEcosystem`

Next production edits should stage a requested intent in `_MCP_Temp` first,
validate it, then apply it to the approved target level only after the staged
shape is acceptable.
