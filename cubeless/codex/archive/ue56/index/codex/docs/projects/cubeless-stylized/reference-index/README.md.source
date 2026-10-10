# Reference Index

External Unreal learning projects are indexed as reference material for CubelessStylized. Keep source projects outside this repository and store only reviewed notes, JSON summaries, and MCP applicability decisions here.

## Rules

- Do not copy external `Content`, `Plugins`, `Saved`, `Intermediate`, `DerivedDataCache`, or build outputs into this repository.
- Use clean source projects for preservation and separate copied projects for MCP/editor experiments.
- Treat documentation and JSON in this folder as project memory; treat external projects as re-verification sources.
- Grade patterns before MCP generation uses them:
  - `A`: approved pattern for MCP-assisted recreation or adaptation.
  - `B`: useful reference, but requires Ieta review before use.
  - `C`: context-heavy or messy; documentation only.
  - `Blocked`: current MCP tools should not generate or modify this pattern.

## Recommended JSON Shape

```json
{
  "source_project": "LyraStarterGame",
  "source_root_key": "LYRA_ROOT",
  "source_root_example": "D:/Git/LyraStarterGame",
  "unreal_version": "unknown",
  "indexes": [
    {
      "asset_path": "/Game/Example/Asset",
      "filesystem_path": "D:/Git/LyraStarterGame/Content/Example/Asset.uasset",
      "asset_type": "AnimationBlueprint",
      "category": "anim_bp",
      "pattern_summary": "",
      "referenced_assets": [],
      "mcp_applicability": "A|B|C|Blocked",
      "risks": [],
      "requires_source_project": true
    }
  ]
}
```

## Current Focus

- AnimBP: start with CubelessStylized, Lyra, and Game Animation Sample.
- PCG: start with CubelessStylized and Electric Dreams.
- BP gameplay: start with small project-local gameplay Blueprints, then lightweight external samples.
