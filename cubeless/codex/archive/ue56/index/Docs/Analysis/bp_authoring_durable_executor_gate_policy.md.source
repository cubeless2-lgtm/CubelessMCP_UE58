# BP Authoring Durable Executor Gate Policy

This policy captures the Section 46-48 durable authoring boundary.

## Section 46 - Durable Gate Matrix

Durable Blueprint requests may be classified and inspected, but they must not
be executed by the temporary manifest executor. The executor policy must report
a durable gate for every manifest and prove these counts for the default
request set:

- durable requested manifests: `1`
- read-only live preflight allowed: `1`
- durable executor enabled: `0`
- durable executor executable: `0`
- allowed durable authoring commands: `0`
- save/delete/rename commands allowed: `0`
- preflight pass: `0`

If any durable save, delete, rename, overwrite, or authoring command becomes
allowed before a separate durable executor is explicitly enabled, the temporary
executor policy must block the manifest with `durable_gate_opened`.

## Section 47 - Explicit Durable Enable Contract

The durable executor remains disabled by default. A future durable executor
must prove all of these before any saved asset can be created:

- explicit durable executor enable flag
- target package allowlist
- live read-only asset-exists result promoted into the save gate
- overwrite-or-rename decision
- rollback policy that protects preexisting assets
- compile/save validation for durable-only manifests
- executor-created asset ownership marker

Until those checks pass, durable manifests stay non-executable records.

## Section 48 - Limited Live Preflight

The only live action currently allowed for a durable request is a read-only
`unreal.EditorAssetLibrary.does_asset_exist` check against the target asset.
The live preflight result must prove:

- `read_only=true`
- `authoring_attempted=false`
- `save_or_delete_attempted=false`
- the asset-exists check was performed
- `preflight_pass=false`

The planner-driven live smoke may run this read-only preflight while still
blocking durable authoring. It must fail if the preflight result attempts
authoring, save, delete, rename, or reports an unexpected manifest id.

## Section 51 - Durable Authoring Enable Contract

Section 51 still does not enable durable Blueprint creation or saving. It
separates the preconditions a later durable executor must satisfy before it can
open:

- target package allowlist
- exactly one overwrite-or-rename decision
- rollback readiness that protects preexisting assets
- executor-created asset ownership marker policy

For the default request set, the release boundary must prove:

- durable enable contract requests: `1`
- enable contract satisfied: `0`
- durable executor may open: `0`
- durable authoring allowed: `0`
- forbidden command allowance for `save=true`, `save_asset`, `delete_asset`, and `rename_asset`: `0`
- target allowlist gate passed: `1`
- overwrite/rename decision gate passed: `0`
- rollback readiness gate passed: `0`
- ownership marker gate passed after Section 52 marker policy: `1`

The contract is intentionally stricter than the current read-only preflight.
Even if all Section 51 gates are satisfied in a future offline contract, this
section alone still reports `durable_executor_may_open=false`; a later explicit
durable release must separately enable and verify live durable authoring.

## Section 52 - Rollback Ownership Marker Contract

Section 52 defines the ownership marker a future durable executor must record
before rollback/delete can even be considered. The marker must bind the created
asset to:

- marker schema and namespace
- executor id, durable plan id, and run id
- target asset path and created asset path
- proof that the preflight asset-exists result was `false`

The release boundary must prove:

- durable ownership marker requests: `1`
- ownership marker policy ready: `1`
- delete without marker allowed: `0`
- delete preexisting asset allowed: `0`

This section only authorizes a future rollback decision at contract level. It
still reports `delete_allowed_now=false` and does not run live delete, save,
rename, or overwrite commands.

## Section 53 - Durable Executor Dry-Run Plan

Section 53 records a future durable executor plan without allowing any live
command execution. The plan may list report-only steps for:

- read-only target preflight
- conflict policy review
- ownership marker preparation
- compile validation draft
- save gate review
- rollback authorization draft

The release boundary must prove:

- durable dry-run plan requests: `1`
- dry-run plan created: `1`
- dry-run plan valid: `1`
- durable executor may execute: `0`
- live command count: `0`
- forbidden command allowance: `0`

The dry-run `execution_command_plan` must remain empty. A dry-run plan is
evidence for review only, not permission to create, save, delete, rename, or
overwrite assets.

## Section 54 - Durable Save Validation Simulator

Section 54 evaluates durable save prerequisites without running a save. The
simulator checks:

- target package allowlist
- read-only asset-exists result availability
- overwrite-or-rename decision
- ownership marker policy
- rollback readiness
- dry-run plan validity
- enable contract satisfaction
- compile/save validation enablement
- explicit durable executor enable flag

The release boundary must prove:

- durable save simulation requests: `1`
- simulation evaluated: `1`
- future save conditions satisfied: `0`
- `save=true` allowed: `0`
- `save_asset` allowed: `0`
- compile-save command allowed: `0`
- live command count: `0`

The simulator may explain why a future save is blocked, but it must not promote
that explanation into live durable execution.

## Section 55 - Durable Canary Preparation Contract

Section 55 reserves a narrow canary target and cleanup boundary for a future
durable canary. It does not approve live durable canary execution.

The canary prep boundary is:

- canary package allowlist: `/Game/_MCP_Temp/DurableCanary`
- source durable target remains `/Game/Blueprints/BP_PlannerDurable`
- canary asset path uses the canary package, not the general Blueprint package
- ownership marker policy must be ready before cleanup can be considered
- save simulation must already be evaluated
- live canary execution allowed: `false`
- general `/Game/Blueprints` package allowed for canary output: `false`
- `save=true`, `save_asset`, and `delete_asset` allowed: `false`

The release boundary must prove:

- durable canary prep requests: `1`
- canary prep ready: `1`
- live canary execution allowed: `0`
- general Blueprints package allowed: `0`
- `save=true` allowed: `0`
- `save_asset` allowed: `0`
- `delete_asset` allowed: `0`

The prep contract is useful only as a target and cleanup definition. It must
not produce a live command plan or open durable save/delete behavior.

## Section 56 - Durable Canary Approval Gate

Section 56 adds an explicit approval gate for the canary target prepared in
Section 55. The approval record must be scoped to the canary package and canary
asset path. It does not authorize live canary execution.

The approval gate boundary is:

- approval record schema: `section_56_durable_canary_approval_record_v1`
- approval gate schema: `section_56_durable_canary_approval_gate_v1`
- approved operation: `canary_preflight_only`
- approval scope: `durable_canary_prep`
- approval package: `/Game/_MCP_Temp/DurableCanary`
- canary approval gate passed: `true`
- canary executor may open: `false`
- live canary execution allowed: `false`
- general `/Game/Blueprints` package allowed: `false`
- `save=true`, `save_asset`, and `delete_asset` allowed: `false`
- live command count: `0`

The release boundary must prove:

- durable canary approval requests: `1`
- approval record present: `1`
- approval gate passed: `1`
- approval scoped to canary package: `1`
- canary executor may open: `0`
- live canary execution allowed: `0`
- general Blueprints package allowed: `0`
- `save=true`, `save_asset`, and `delete_asset` allowed: `0`
- live command count: `0`

If the approval record is missing or points outside the canary target, the gate
must fail. If it passes, it still only permits a later section to consider a
read-only canary preflight; it does not create a command plan.

## Section 57 - Durable Canary Live Preflight

Section 57 allows only a read-only live preflight for the Section 55 canary
target after the Section 56 approval gate passes. The live operation is limited
to `unreal.EditorAssetLibrary.does_asset_exist` on
`/Game/_MCP_Temp/DurableCanary/<BlueprintName>_Canary`.

The live preflight boundary is:

- canary live preflight schema:
  `section_57_durable_canary_live_preflight_contract_v1`
- result schema: `section_57_durable_canary_live_preflight_result_v1`
- read-only live preflight allowed: `true`
- canary execution allowed after preflight: `false`
- authoring command allowed: `false`
- save/delete command allowed: `false`
- cleanup command allowed: `false`
- live authoring/save/delete/cleanup command counts: `0`

The release boundary must prove:

- durable canary live preflight requests: `1`
- read-only live preflight allowed: `1`
- canary execution allowed after preflight: `0`
- authoring command allowed: `0`
- save/delete command allowed: `0`
- cleanup command allowed: `0`
- live authoring/save/delete/cleanup command counts: `0`

The planner-driven live smoke may run the read-only canary asset-exists check,
but the result must report `authoring_attempted=false`,
`save_or_delete_attempted=false`, `cleanup_attempted=false`, and
`canary_execution_attempted=false`.

## Section 58 - Durable Canary Recovery Matrix

Section 58 defines the future rollback and cleanup recovery scenarios for a
durable canary. The matrix is report-only.

The recovery boundary is:

- recovery schema: `section_58_durable_canary_recovery_matrix_v1`
- recovery matrix ready: `true`
- scenario count: `6`
- cleanup requires ownership marker: `true`
- cleanup requires the canary preflight asset to have been absent: `true`
- cleanup requires the created asset path to match the canary path: `true`
- cleanup/delete/save/authoring command allowed: `false`
- live cleanup/delete/save/authoring command counts: `0`

The matrix covers preflight asset absent, preflight asset present, creation
failure before marker, creation failure after marker, compile/save blocked, and
valid-marker cleanup review. It does not run cleanup or delete.

## Section 59 - Release Boundary V2 Consolidation

Section 59 raises the release boundary report schema to
`section_59_bp_authoring_release_boundary_v2` and adds a consolidation row for
Section 51-58. The consolidation row must prove:

- durable authoring enabled: `false`
- durable enable satisfied: `0`
- durable executor may open: `0`
- durable save allowed: `0`
- durable canary executor may open: `0`
- durable canary live execution allowed: `0`
- durable canary recovery cleanup allowed: `0`
- durable gate summary: `passed`
- Section 51-58 blocking contracts ready: `true`

This is a reporting boundary only. It does not enable durable authoring.

## Section 60 - MVP Decision Contract

Section 60 adds `section_60_bp_authoring_mvp_decision_contract_v1`.

The decision is:

- temporary Blueprint authoring MVP ready: `true`
- MVP scope: planner-safe Actor Blueprint temporary smoke
- durable Blueprint authoring MVP ready: `false`
- durable authoring enabled: `false`
- durable save/delete/rename/cleanup allowed: `false`
- durable canary live execution allowed: `false`

Durable authoring remains contracts-only until a later explicit durable MVP
request implements and verifies a real durable executor.

## Decision

Section 46-48 improves durable safety visibility, Section 51 separates the
future durable enable gates, and Section 52 defines the ownership marker needed
for future rollback. Section 53 adds a no-command dry-run plan. Section 54 adds
a no-command save validation simulator. Section 55 adds canary prep only.
Section 56 adds scoped approval only. Section 57 adds read-only canary live
preflight only. Section 58 adds recovery scenarios only. These sections do not
enable durable Blueprint creation, saving, delete, rename, cleanup, or live
canary execution. Section 59 consolidates that boundary in the v2 release
report. Section 60 marks the temporary authoring MVP ready while keeping durable
authoring outside the MVP.
