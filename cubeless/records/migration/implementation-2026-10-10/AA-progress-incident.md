# AA state-transfer integration: progress and first incident

This is an incident record, not completion of AA or the legacy state-transfer pipeline. Evidence paths below are relative to `cubeless/records/verification/implementation-2026-10-10/`. The original failed reports and source snapshots remain immutable. This note records the AA1 clone comparison failure and the subsequent AA2 pin-default failure; actual AA runtime validation remains pending.

## Build and registration observed before the failure

- `build-state-transfer-AA1.log`: succeeded, 15 build actions, 38.53 seconds total. This is the editor build, not a new full BuildPlugin or Shipping proof.
- `python-contract-tests-AA1.log`: 121 tests passed in 5.006 seconds. Those tests did not cover this newly observed clone comparison regression.
- `registered-schemas-and-diagnostics-AA1.json`: official registration passed with 8 Cubeless Toolsets and 57 tools; the batch capability count is 31. Editor PID 52176, UE `5.8.3-58210709+++UE5+Release-5.8`, instance `053F78DC-43F0-1F38-C4BE-3EBD5F18CFA8`.
- DLL SHA256: `3b81a7d7a56df9bb1d2b19c3b9210fa4ed63d10298d0cf8cf3f2db33ec4500ee`.
- `source-snapshot-AA1.json` records 140 root/isolated files matching the registration snapshot. Source bytes are preserved under `source-snapshots/plugin-AA1/`; binaries are represented by hashes, not copied into that source archive. This is the AA1 snapshot boundary, not a claim that later working sources remain identical.

## AA1: a clone was created, but its content comparison failed

`blueprint-state-transfer-AA1.json` failed at pipeline stage `clone_component`, tool result stage `readback`, error `duplicate_content_failed`: `Observed clone graph/variable/class content differs after path/GUID normalization`.

The source was the saved V ActorComponent timer `/Game/CubelessMCPTests/FunctionTimer_ac30351e04c74596887ffe9b9146f4dd/BP_FunctionTimer`. The attempted destination was `/Game/CubelessMCPTests/StateTransfer_06f9799bb70b46b487a5aef29adff753/BP_StateComponent`.

- The tool recorded `created=true` and a created-asset change. The clone existed in memory, was dirty, and had no saved asset file. `BP_StateHost` was not created. This was not a mutation-free failure.
- Explicit compile was neither requested nor attempted; save was neither requested nor attempted. The engine's implicit PostDuplicate compiler work was attempted and returned with `BS_UP_TO_DATE`; diagnostics were not captured. This separate implicit work must not be described as zero engine compilation or as a verified explicit compile.
- `validation.source_preserved=passed`. The source's before/after full snapshots matched, and its disk fingerprint stayed SHA256 `bf97de438589de1155166eba36d99c05a98ea7398d918bd5fb018a71ddb5c3b1`, 31,395 bytes, mtime `1791641505603962300` ns. Source and target observations were retained in the failed tool result.
- The original report retains `outcome_unknown=true`, `do_not_retry=true`, `automatic_retries=0`, and `atomic_rollback=false`. Later comparison analysis does not rewrite those original failure-state fields.
- No AA host graph, state-transfer runtime, cold reload, or packaging result was obtained from this attempt.

Failed report SHA256: `9d206cb147c9c2df76c6cff71c5fca6c9be9cf21b11d497dff6d7def2c11b3b9`.

## Confirmed comparison regression and bounded correction

The new graph observation field `variable_member_guid` exposed the legitimate regenerated GUID of the local `TimeSinceLastClick` declaration. The source declaration, getter and setter all refer to `FAB35832459C60DA3E7240B8A0E61E22`; the cloned declaration, getter and setter all refer to `604024AD474224BCFA88168ECDA19C30`. Source before/after identities remain unchanged. The two remaining normalized differences were the getter/setter member GUIDs, not evidence of broken variable references.

The Python correction is in `compatibility/ue58/blueprint_duplicates.py::semantic_content` and its focused tests. It first checks unique nonzero local declaration GUIDs, the member's resolved own property name/owner, its serialized owner, and equality with its own declaration GUID. Only a valid local declaration reference is then compared by declaration name across the clone boundary. External, inherited and SCS member GUIDs remain literal identities; malformed or mismatched local references fail. Own generated/skeleton owner paths are normalized only at the existing exact asset boundary. No blanket GUID removal or arbitrary string-default rewriting was added.

The Python owner reported 9 focused tests passing in 0.296 seconds and an offline semantic comparison passing against the entire actual AA1 `source_before`/`target_after` payload. The coordinator read the correction. These are correction and offline evidence, not a successful AA2 engine invocation. The failed AA1 report hash above remained unchanged.

## Reconciliation and AA2 launch boundary

`editor-prelaunch-AA2.json` records that the only dirty package in PID 52176 was the owned unsaved AA1 `BP_StateComponent` above. Before stopping that owned verification editor, the coordinator confirmed source SHA/snapshots unchanged and both clone/host asset files absent. The unsaved clone was explicitly discarded at the restart. This was a deliberate disposal of an isolated fixture, not atomic rollback, an automatic mutation retry, or disposal of user work.

`editor-launch-AA2.json` records stopped PID 52176 and new PID 33360. Only the corrected plugin Python content was redeployed; the DLL hash remains the AA1 hash. The next section records the separate fresh-UUID author attempt rather than inferring success from this launch.

## AA2: clone correction passed; null object-pin comparison failed later

`blueprint-state-transfer-AA2.json` is a separate failed run, UUID `6752d7a42cb94328a19b679eba59221e`, SHA256 `af3b3873bd092d5a0017baa86fbeb0c05e7dd24b918f81526efc81b13cfa9940`. The corrected clone comparison passed. Five prototype members (Boolean, Vector and Engine object-reference fields) were added and the prototype was explicitly compiled/saved once. Its saved fingerprint is SHA256 `b40fc760dc9e0163761f42d9f4342b3143d4f68ed8b1bda16109ce2f91802c7f`, 36,160 bytes, mtime `1791648214494287800` ns. The V source's SHA/size/mtime remained unchanged.

Host creation, its two SCS components, variables, function/input and node construction then succeeded. The host was still unsaved, with 19 function-graph nodes and no links. A wrong Boolean literal was rejected before mutation and independent readback was unchanged. The subsequent `state_pin_defaults` batch stopped at index 3 with `batch_child_failed` / `pin_default_postcondition_failed`:

- Indices 0 and 1 requested Boolean false values already present: no-op, mutation not attempted.
- Index 2 set a Boolean true: observed changed=true.
- Index 3 requested JSON `null` for the `Controller` object pin. Native mutation was attempted, but the complete observations reported changed=false and change_state_known=true. The observed engine representation was empty `DefaultValue` plus null `DefaultObject`, while the requested text was `None`. Other node/pin identities and values were preserved. The native owner is checking the engine canonical-default comparison; this record does not assume the engine setter corrupted the graph.

The batch truthfully reported `partial_mutation=true`, `mutation_performed=true`, `mutation_state_unknown=false`, `command_may_still_be_in_flight=false`; the parent report retained `outcome_unknown=false` and zero automatic retries. A known unchanged failed child does not erase the earlier true-value write or prior host construction. No host compile/save or state-transfer execution was attempted after this failure. At this checkpoint the saved prototype is preserved, the host remains an owned unsaved fixture, and cleanup has not yet been recorded. The AA2 source snapshot records 140 deployed/root files before the next correction.

## Remaining AA proof

AA must still author and save the component/host graph, check its actual official-MCP readback, and execute the frozen no-MCP verifier `verify_state_transfer_runtime.py` against the saved assets. That verifier covers four direct function calls, pre-gate reset of two Boolean fields, retained owner/position fields, component identity, and file/CDO/world cleanup observations. It does not prove real player/controller input, scheduled Tick, hardware input, Shipping, or full legacy pipeline parity. Global README, status and migration-ledger completion evidence are deferred until actual AA results are available.
