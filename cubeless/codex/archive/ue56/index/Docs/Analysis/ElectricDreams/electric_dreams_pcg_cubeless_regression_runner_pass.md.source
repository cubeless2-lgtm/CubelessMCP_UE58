# Electric Dreams PCG Cubeless Regression Runner Pass

Date: 2026-06-09

## Scope

- Runner:
  - `run_pcg_study_regression.py`
- Covered study scripts:
  - `build_spline_assembly_with_post_copy_offset.py`
  - `verify_spline_assembly_output.py`
  - `build_ground_self_pruning_fixture.py`
  - `verify_ground_self_pruning_fixture.py`
  - `build_ground_density_bounds_fixture.py`
  - `verify_ground_density_bounds_fixture.py`
  - `build_ground_combo_fixture.py`
  - `verify_ground_combo_fixture.py`
  - `verify_cubeless_ed_dynamic_material_axis_actor_property_selector_compat.py`
  - `verify_cubeless_ed_dynamic_material_axis_menu_apply.py`
  - `verify_cubeless_ed_true_material_applied_presets.py`
  - `prepare_cubeless_ed_material_override_selector_validation.py`
  - `verify_cubeless_ed_material_override_selector_blueprint.py`
  - `prepare_cubeless_ed_ecosystem_selector_validation.py`
  - `verify_cubeless_ed_ecosystem_selector_blueprint.py`

Only sibling analysis scripts and `_MCP_Temp` Unreal assets are involved. No production Cubeless assets or C++ were changed.

## Runner Behavior

The runner supports:

```python
PCG_STUDY_REGRESSION_PHASE = "all" | "build" | "verify"
PCG_STUDY_REGRESSION_STEP = "<step_name>"
PCG_STUDY_REGRESSION_SUPPRESS_FINAL_RAISE = True | False
```

Important Unreal behavior:

- Running build and verify in the same Unreal Python call can fail because PCG generated output may not be materialized until the next editor tick.
- Reliable execution is one build step per MCP call, then its matching verify step in the next MCP call.
- Selector `prepare_*` scripts are listed under the runner's `build` phase because they create disposable validation actors before the matching verify step.
- The runner prints a note when `phase=all` is used so this failure mode is visible.

## Verified Step Pair Results

### Main Spline Assembly

```text
main_spline_assembly_build: PASS
main_spline_assembly_verify: PASS
expected_point_count=42
point_count=42
missing_parent_count=0
parent_depth_mismatch_count=0
ground_style_smoke_pass_count=42
ism_total=42
log_error_count=0
spline_assembly_validation_pass=True
```

### Self-Pruning Fixture

```text
self_pruning_fixture_build: PASS
self_pruning_fixture_verify: PASS
source_point_count=8
point_count=4
pruned_count=4
survived_seeds=[100, 200, 300, 301]
self_pruning_fixture_pass_count=4
log_error_count=0
self_pruning_fixture_validation_pass=True
```

### Density/Bounds Fixture

```text
density_bounds_fixture_build: PASS
density_bounds_fixture_verify: PASS
source_point_count=3
point_count=3
expected_density_by_seed={10: 0.26, 11: 0.5, 12: 0.74}
density_bounds_fixture_pass_count=3
density_mismatch_count=0
bounds_mismatch_count=0
log_error_count=0
density_bounds_fixture_validation_pass=True
```

### Ground Combo Fixture

```text
ground_combo_fixture_build: PASS
ground_combo_fixture_verify: PASS
source_point_count=6
point_count=9
profile_counts={'tight': 6, 'expanded': 3}
profile_seeds={'tight': [20, 21, 22, 23, 24, 25], 'expanded': [20, 23, 25]}
ground_combo_fixture_pass_count=9
density_mismatch_count=0
bounds_mismatch_count=0
log_error_count=0
ground_combo_fixture_validation_pass=True
```

### Dynamic Material And Selector Coverage

The runner now includes the latest Cubeless ED selector/material verification scripts:

```text
dynamic_material_axis_actor_property_compat_verify
dynamic_material_axis_menu_apply_verify
true_material_applied_presets_verify
material_override_selector_prepare
material_override_selector_verify
ecosystem_selector_prepare
ecosystem_selector_verify
```

These cover:

- the production-compatible shared dynamic material axis graph
- menu application for non-default material override selectors
- true material-applied preset graph verification
- material override selector validation actors
- ecosystem selector validation actors, including the dynamic material preview component

Runner filter validation:

```text
PCG_STUDY_REGRESSION_PHASE=build
PCG_STUDY_REGRESSION_STEP=ecosystem_selector_prepare
regression_step_result=ecosystem_selector_prepare|PASS
pcg_study_regression_pass=True

PCG_STUDY_REGRESSION_PHASE=verify
PCG_STUDY_REGRESSION_STEP=ecosystem_selector_verify
regression_step_result=ecosystem_selector_verify|PASS
ecosystem_selector_validation_pass=True
log_error_count=0
pcg_study_regression_pass=True
```

## Result

The disposable Cubeless PCG study suite now has a reusable regression runner and all current study targets pass when executed as build/verify pairs.

This gives a pre-production safety gate before moving any PCG controls outside `_MCP_Temp`.

## 2026-06-12 StaticMeshSpawner Actor Property Audit Gate

The runner now includes:

```text
static_mesh_spawner_actor_property_audit_verify
```

This wrapper executes the project audit script:

```text
D:\Git\CubelessStylized\Tools\Unreal\audit_pcg_static_mesh_spawner_actor_property_overrides.py
```

Gate conditions:

- audit policy loads successfully
- `actionable_graphs_needing_actor_property_review=0`
- `actionable_review_spawner_count=0`
- `production_graphs_needing_actor_property_review=0`
- `production_review_spawner_count=0`
- `cleanup_candidate_graph_count=0` after the 2026-06-12 archive cleanup
- `cleanup_candidate_spawner_count=0` after the 2026-06-12 archive cleanup

Latest targeted runner evidence:

```text
PCG_STUDY_REGRESSION_PHASE=verify
PCG_STUDY_REGRESSION_STEP=static_mesh_spawner_actor_property_audit_verify
regression_step_result=static_mesh_spawner_actor_property_audit_verify|PASS|0.186s
pcg_study_regression_pass=True
```

The 2026-06-12 cleanup pass archived the previously reported material override candidates under `/Game/Cubeless/_Archive/PCG_StaticMeshSpawnerActorPropertyAudit_20260612`. The empty `RuntimeGrass/NewPCGGraph` candidate was deleted after its archive copy produced a missing soft reference AssetCheck error. This runner step still does not delete, move, save, or archive Unreal assets; it only verifies the current audit state.

## Approval Boundary

No approval is needed to keep adding `_MCP_Temp` regression cases.

Approval is needed before:

- turning these fixtures into production Cubeless PCG assets
- saving designer-facing PCG parameters outside `_MCP_Temp`
- changing non-plugin project C++
