# Planner-driven Smoke Policy

This policy defines the Section 11 gate between a planned Blueprint authoring
request and live UnrealMCP asset-authoring commands.

## Required Flow

1. Classify every request with `bp_authoring_planner.py`.
2. Convert the planner result into a Section 12 job manifest with
   `bp_authoring_job_contract.py`.
3. Build a Section 40 executor policy with
   `bp_authoring_manifest_executor.py`.
4. Put only executor-approved temporary-smoke `safe_to_author` manifests into
   the live authoring queue.
5. Record `requires_review` and `blocked_until_reinforced` manifests as prevented
   dry-run entries with `authoring_attempted=false`.
6. Preserve any durable preflight dry-run contract on prevented durable requests
   without executing durable authoring commands.
7. In live mode, run only the read-only
   `unreal.EditorAssetLibrary.does_asset_exist` durable preflight check for
   prevented durable requests that declare a target asset path.
8. Run live authoring only under `/Game/_MCP_Temp/PlannerDrivenSmoke` unless a
   caller explicitly supplies another temporary package path.
9. Compile without saving, re-read, inspect, and delete every generated
   temporary Blueprint.

## Refusal Boundary

The smoke must not send `create_blueprint` or graph-authoring commands for
review or blocked requests. A broad C++ conversion request, async proxy request,
GAS/replication request, CommonUI structure request, non-allowlisted parent
class request, durable save request, or unknown request must remain a dry-run
job manifest until dedicated reinforcement exists.

## Validation

- Offline tests must prove that only executable safe manifests are executed by
  the live runner.
- Live mode is opt-in with `--run-live`.
- If the UnrealMCP bridge is unavailable and `--require-live` is not set, the
  report records `live_gate.status=skipped`.
- A live run fails if compilation reports errors, new editor log errors appear,
  or generated `MCP_PlannerSmoke_*` assets remain after cleanup.
- Temporary smoke compile validation must use `save=false`; durable save
  semantics belong to a later non-temporary authoring executor.
- A planner-safe request that asks for a saved or durable Blueprint asset must
  be prevented unless a separate durable executor contract enables it. Its
  Section 36 durable preflight target, read-only asset-exists result, and
  overwrite/rename decision may be reported, but it must not be saved or
  authored by the temporary smoke runner.
- Durable preflight live results must report `read_only=true`,
  `authoring_attempted=false`, and `save_or_delete_attempted=false`.
- Live reports must also expose aggregate guard flags:
  `non_safe_authoring_attempted=false`,
  `durable_authoring_attempted=false`, and
  `durable_live_save_or_delete_attempted=false`.
- A Section 36 overwrite/rename decision may be classified in the manifest, but
  the smoke runner must not apply overwrite, rename, save, or delete behavior.
- Section 37 save gate and rollback policy contracts may explain why durable
  save is blocked, but the smoke runner must still use `save=false` and must
  not delete or overwrite durable assets.
- Section 38 durable executor readiness may be reported, but
  `durable_executor_ready=false` means the smoke runner must not enable durable
  save execution.
- Section 39 disabled durable executor skeleton may be reported, but
  `executor_enabled=false`, `can_execute=false`, and `command_plan=[]` mean the
  smoke runner must not run durable executor commands.
- Section 51 durable authoring enable contract may be reported, but
  `durable_executor_may_open=false` and the save/delete/rename forbidden-command
  flags mean the smoke runner must not run durable executor commands.
- Section 52 durable ownership marker contract may be reported, but
  `delete_without_marker_allowed=false`, `delete_preexisting_asset_allowed=false`,
  and `delete_allowed_now=false` mean the smoke runner must not run durable
  rollback/delete commands.
- Section 53 durable dry-run plan may be reported, but
  `execution_command_plan=[]`, `live_command_count=0`, and
  `durable_executor_may_execute=false` mean the smoke runner must not run
  durable executor commands.
- Section 54 durable save simulator may be reported, but
  `save_true_allowed=false`, `save_asset_allowed=false`, and
  `live_command_count=0` mean the smoke runner must not run durable save
  commands.
- Section 55 durable canary prep may be reported, but
  `canary_live_execution_allowed=false`,
  `general_blueprints_package_allowed=false`, `save_true_allowed=false`,
  `save_asset_allowed=false`, and `delete_asset_allowed=false` mean the smoke
  runner must not run durable canary, save, or cleanup commands.
- Section 56 durable canary approval gate may be reported, but
  `canary_executor_may_open=false`, `canary_live_execution_allowed=false`,
  `save_true_allowed=false`, `save_asset_allowed=false`,
  `delete_asset_allowed=false`, and `live_command_count=0` mean the smoke runner
  must not run durable canary, save, or cleanup commands.
- Section 57 durable canary live preflight may be reported and, in live mode,
  may run only `unreal.EditorAssetLibrary.does_asset_exist` against the canary
  asset path. `canary_execution_allowed_after_preflight=false`,
  `authoring_command_allowed=false`, `save_or_delete_allowed=false`, and
  `cleanup_command_allowed=false` mean the smoke runner must not run durable
  canary execution, save, or cleanup commands.
- Section 58 durable canary recovery matrix may be reported, but
  `cleanup_command_allowed=false`, `delete_command_allowed=false`,
  `save_command_allowed=false`, and `authoring_command_allowed=false` mean the
  smoke runner must not run durable cleanup, delete, save, or authoring
  commands.
- Section 59 release boundary v2 may be reported, but it is reporting-only and
  must not add any live smoke command.
- Section 60 MVP decision may be reported, but it is reporting-only and must
  not add any live smoke command.
- Section 11 does not require C++ changes.
