# Component read verification pipeline N

Source-only implementation, 2026-10-10. This document does not claim that the fixture, official MCP calls or cold reload have run. Root owns all engine/process/asset operations.

The independent migration items are `list_blueprint_components` and `get_component_property`; their legacy/public/native connections and deliberate limitations are in `component-python-first-slice-N.md` and `native-component-read-N.md`. The fixture author is a verification script, not another migrated generic execution tool.

## Execution contract

Root creates three fresh assets with `verification/create_component_read_fixture.py` in the exact `cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject`. The fixture author uses public SubobjectData APIs only for a newly created local Blueprint; it does not gather inherited components or derive expected values from the N native reader. Native compile diagnostics are used for explicit fixture compilation, independently of N component reads. Factory implicit compilation is recorded separately, SaveOnCompile must already be Never, and a single explicit save batch produces the three files. Earlier asset hashes and final dirty state are preserved in the fixture report.

The external pipeline runs against the existing official MCP endpoint and starts no server/editor:

```text
python -B cubeless/tools/pipelines/component_read_smoke.py --endpoint http://127.0.0.1:8000/mcp --expected-project <exact UE58Verification .uproject> --phase read --fixture-report <fresh successful fixture report> --output <new first-read report>
python -B cubeless/tools/verification/component_pipeline_report.py <first-read report> --fixture-report <fixture report> --output <new independent report>
```

After root has cleanly restarted the editor, run the same pipeline with `--phase reload --parent-report <first-read report>`. The independent verifier also receives `--parent-report` for that phase. Endpoint must match root's actual official connection; the example is not a required port or a second server. Evidence paths must be new; the pipeline does not overwrite a prior report or retry a failed attempt.

Each phase contains 51 named calls:

- Project/version/thread/PIE/clean-state guard, full local list, immediate repeat, exact filter, missing filter, explicit empty/null filter, no-SCS and inherited-local scope lists.
- Ten named native property reads, compared to declared values and independent public getter results. Nine also have `ObjectTools.get_properties` baselines/readbacks. AttachSocketName is a public getter exception because the generic official property accessor cannot read its metadata.
- Nine explicit negative cases, each followed by an actual full local list: missing component, missing property, unsupported BodyInstance struct, traversal cap, text cap, 1024-unit selector, empty property, no-SCS lookup and inherited-but-not-local lookup.
- Final local list, three official dirty queries, final clean diagnostics and exact before/after/final disk SHA256 observations.

No component editing, explicit compile, save, cleanup, mutation replay or engine process control occurs in the external read pipeline. Transport/decoding loss around potentially loading reads is retained as uncertain; the run stops and does not retry. An unexpectedly accepted known negative is followed by one readback and then fails. Session closure runs in `finally`, and failed reports still retain a final disk observation when readable.

## Fixture and value scope

`BP_Local` has DefaultSceneRoot → MeshChild → LightChild. Mesh uses the engine cube, relative XYZ [12,34,56], Pitch/Yaw/Roll [10,20,30], scale [1.25,0.75,2], visible false, two tags and Movable mobility. Light has intensity 1234.5, Lumens units and default None socket name. `BP_Empty` derives from SaveGame with no SCS. `BP_Inherited` derives from BP_Local but is never gathered for editable overrides; its expected local list is empty. This proves the stated local scope, not effective inherited components.

Property families covered by the fixture: FVector, FRotator, object reference, bool, scalar numeric, scalar FName, byte enum, typed enum and bounded FName array text. FString/FText, arbitrary unsupported structures, malformed native enum descriptors, orphan/cyclic trees and every budget boundary remain outside this actual fixture. Existing synthetic/native source evidence must not be promoted to real-engine execution proof.

The native ComponentTags return remains a **string** in UE ExportText syntax. The verifier parses only this fixture's two simple alphabetic names to compare with independent JSON-array getters; it is not a generic Unreal text parser. Raw text must also be identical after a cold restart. Official byte enum strings and object references are checked in their own encoding; they are not treated as the legacy native JSON shape.

GUIDs have no independent public Python accessor in this fixture. The verifier checks syntax, uniqueness and same-run/cold stability only, with `guid_independently_verified=false`. Component names/classes/templates/hierarchy/transforms and ten property values use independent fixture baselines. The verifier re-decodes raw MCP data and binds every stage to its actual named call and arguments; cached success flags are insufficient.

## Verification limits and performance

Stable dirty/status and file hashes do not prove that asset loading performed no implicit PostLoad work or that arbitrary UObject state stayed identical. Those limits remain explicit in each public operation and final verification scope. Disk persistence here refers only to unchanged already-saved fixture files. Packaging, runtime independence, inherited CDO completeness and full legacy migration are not covered by this pipeline.

Each call retains HTTP elapsed time and the native local-node count/timing. This is an observation protocol, not a latency optimization benchmark; no speedup is claimed. List and property implementation share the same bounded native traversal rather than invoking duplicate component enumerations.

`verification/test_component_pipeline.py` exercises synthetic first/cold success, raw/provenance tampering, hierarchy/traversal/value/encoding changes, false scope claims, dirty/unknown/save effects, wrong rejection categories, GUID changes, exact asset ownership, implicit compile/save policy, same-process replay, final bytes, and the actual external pipeline with a fake client. No test invokes the editor or network.

Actor M external code remains frozen. Its missing `client.close()` in `finally` is recorded as a separate maintenance suggestion; adding the N pipeline's cleanup does not rewrite the already-recorded M source/evidence.
