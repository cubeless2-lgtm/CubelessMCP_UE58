# UDS Analysis Delivery Manifest

This document records the read-only delivery classification for the current
UDS analysis branch. It is meant to prevent staging unrelated Unreal assets or
generated reports when the branch is ready for commit review.

## Command

Run:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\build_uds_analysis_delivery_manifest.py --timestamped-output
```

Default behavior:

- expected branch: `codex/uds-analysis`
- managed project: `C:\Git\CubelessStylized`
- sibling MCP workspace: `C:\Git\unreal-mcp-cubeless`
- report root: `Saved/UDS_Analysis`
- console output: compact summary JSON
- Git staging: none
- commits: none
- Unreal asset edits/saves: none

Use `--full-json` only when the full manifest payload is needed on stdout. The
written report file is always full JSON.

## Latest Manifest

Generated report:

`Saved/UDS_Analysis/uds_analysis_delivery_manifest_20260617_023159.json`

Run result:

| Field | Value |
| --- | ---: |
| On expected branch | `true` |
| Project status readable | `true` |
| Sibling MCP status readable | `true` |
| Project `git diff --check` pass | `true` |
| Unknown review paths | `0` |
| Latest closeout pass | `true` |
| Latest promotion preflight pass | `true` |
| Delivery manifest pass | `true` |

Classification summary:

| Class | Count |
| --- | ---: |
| Stage candidate | `35` |
| Manual decision | `1` |
| Excluded | `483` |
| Unknown review | `0` |

## Stage Candidates

These paths are in the intended UDS analysis delivery scope:

- `Content/Cubeless/Sky/BP_SkySystem.uasset`
- `Content/Cubeless/Sky/Data/DA_Weather_Clear.uasset`
- `Content/Cubeless/Sky/Data/DA_Weather_Cloudy.uasset`
- `Content/Cubeless/Sky/Data/DA_Weather_Overcast.uasset`
- `Content/Cubeless/Sky/Materials/M_Sky_Dome.uasset`
- `Content/Cubeless/Sky/Meshes/SM_Cubeless_SkySphere.uasset`
- `Tools/Unreal/audit_cubeless_sky_dependencies.py`
- `Tools/Unreal/build_uds_analysis_delivery_manifest.py`
- `Tools/Unreal/check_uds_analysis_staging_scope.py`
- `Tools/Unreal/capture_uds_sky_snapshot.py`
- `Tools/Unreal/check_cubeless_sky_promotion_preflight.py`
- `Tools/Unreal/run_uds_analysis_closeout.py`
- `Tools/Unreal/scan_unreal_editor_log.py`
- `docs/uds-analysis/README.md`
- `docs/uds-analysis/asset-map.md`
- `docs/uds-analysis/blueprint-flow.md`
- `docs/uds-analysis/blueprint-index.md`
- `docs/uds-analysis/cubeless-dependency-audit.md`
- `docs/uds-analysis/cubeless-implementation-roadmap.md`
- `docs/uds-analysis/cubeless-sky-backlog.md`
- `docs/uds-analysis/cubeless-transfer-notes.md`
- `docs/uds-analysis/current-volumetric-cloud-repair.md`
- `docs/uds-analysis/delivery-manifest.md`
- `docs/uds-analysis/handoff.md`
- `docs/uds-analysis/learning-curriculum.md`
- `docs/uds-analysis/material-dependency-index.md`
- `docs/uds-analysis/material-mpc-flow.md`
- `docs/uds-analysis/mcp-followups.md`
- `docs/uds-analysis/runtime-flow.md`
- `docs/uds-analysis/runtime-snapshot-checklist.md`
- `docs/uds-analysis/state-signal-matrix.md`
- `docs/uds-analysis/static-and-2d-clouds.md`
- `docs/uds-analysis/volumetric-cloud-stack.md`
- `docs/uds-analysis/weather-preset-flow.md`
- `docs/work-log.md`

## Manual Decision

Do not stage this path automatically:

- `Content/UltraDynamicSky/Maps/DemoMap.umap`

Reason:

The UDS reference map was touched by the runtime-only volumetric cloud repair.
Review this binary map diff separately before deciding whether it belongs in a
commit. The current analysis does not require saving UDS MPC asset defaults.

## Explicitly Excluded

Do not stage:

- `Content/ANGRY_MESH/` file-level entries: `483`

Reason:

This is unrelated untracked content present in the workspace. The UDS analysis
work did not inspect, modify, or depend on it. The manifest uses
`git status --porcelain=v1 -uall`, so these are counted as individual files;
the summary stores a small sample and the prefix count instead of expanding all
excluded paths in the human-facing section.

Generated reports and screenshots under `Saved/UDS_Analysis` are also excluded
from delivery source by default.

## Staging Scope Guard

Before staging, this command may be run as a read-only preview:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\check_uds_analysis_staging_scope.py --timestamped-output
```

After staging the intended delivery set, use the strict check:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\check_uds_analysis_staging_scope.py --require-staged --require-all-candidates --timestamped-output
```

Default strict behavior fails if `Content/UltraDynamicSky/Maps/DemoMap.umap`,
`Content/ANGRY_MESH/`, generated paths, or unknown paths are staged. If the
manual UDS DemoMap binary is intentionally staged after review, add
`--allow-manual-decisions` to make that decision explicit.

## Current Decision

Use this manifest after closeout and before any staging operation. If a new
path appears as `unknown_review`, classify it deliberately before commit work.
