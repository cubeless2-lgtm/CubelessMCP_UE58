# W: generated self call and gated timer

Status: installed UE 5.8 source review and native source candidate. No W build, editor call or asset change was performed by the native implementation agent. Runtime results belong in the root executor's verification records. Original UE 5.6 and Epic sources were read only.

## Selected next unit

Extend the existing `add_blueprint_call_function_node` implementation to call an already compiled function declared by the **same** Blueprint. Preserve its public signature and existing Engine-static behavior. Reuse the existing function graph/input, declared bool getter, Branch, typed connection, final compile/save and operation-journal cores. Author a new `CFNL_GatedAdvance(DeltaSeconds)` caller on a fresh copy of the V ActorComponent timer, with three bool gates before a self call to `CFNL_AdvanceMouseTime`.

Root separately assigned exact Sequence creation to the Python/graph agent. Its files and public registration are not owned by this change. Sequence plus a fresh caller does not migrate the original host's occupied-link replacement contract.

This is an executable step toward the old caller policy. It is **not** full migration of `build_cube_fluid_ninja_mouse_timer_host.py`, its FluidNinja host, component attachment, or mouse input/event behavior.

## Original chain and remaining boundaries

Paths in this section are under `C:/Git/UnrealMCP5_6_1/`.

| Source | Actual behavior | W disposition |
|---|---|---|
| `Python/scripts/analysis/build_cube_fluid_ninja_mouse_timer_host.py:9–33` | Requires complete topology, exactly one ReceiveTick event and parent call, and exactly one outgoing parent link; inserts two-output Sequence, reconnects original flow on then_0, gates then_1 with FieldResourcesReady/PawnInside/VisibilityInputAllowed, calls the generated MouseButtons component timer with Tick DeltaSeconds. | Extract caller-gate behavior for a self call; full host remains pending. |
| Same file `:21–23`; `build_cube_fluid_ninja_mouse_event_host.py:38–41` | Occupied exec replacement requires expected source count, exact previous destination, schema response `break_others_a`, and `replacement_guard_verified`. | Current direct-link tool rejects occupied links. Do not bypass it or silently omit the preserved branch. |
| `build_cube_fluid_ninja_mouse_event_host.py:14–15,25–34` | Adds generated ActorComponent class to local SCS, compiles, gets component object, invokes external generated functions and accesses external properties through self pins. | Generated SCS class add, SCS object getter, external target member access and generated instance calls are distinct pending contracts. |
| `build_cube_fluid_ninja_mouse_buttons_endplay.py:17–25` | Generated self call from ReceiveEndPlay to a custom event. | W restricts targets to own function graphs with compiled bytecode; custom-event/override support remains pending. |
| Same file `:31–58` | Requires source proof/SHA and target absence, duplicates into a new target, compiles/saves once, checks protected file hashes; records failure with `do_not_retry=True`. | Reuse V exact duplicate and current durable journal. Never duplicate/retry after an uncertain mutation. New operation IDs do not deduplicate equivalent nodes. |
| `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands_Part1.cpp:1966–2092` | Validated call supports generated classes; creates a node before applying defaults and destroys it on invalid defaults. | W prevalidates scalar defaults before creation. A later failure retains actual partial state; no atomic rollback claim. |
| `.../UnrealMCPBlueprintNodeCommands_Part2.cpp:779–856` | Sequence defaults to two outputs, rounds/clamps supplied count to at least two, appends execution pins and marks modified. | Separate named Sequence port; changed bounds must be explicit. |

Current `CubelessGraphApi.cpp:129–150` only accepts `NewVariables` declared by the owning Blueprint and rejects component/inherited/external or object-reference members. A local SCS component is not a `NewVariables` row; relaxing the primitive pin check alone cannot migrate `MouseButtons`.

## Installed public API evidence

Paths below are relative to `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`. Private **implementation source** was inspected as evidence; no private engine header is included by the plugin.

- `Editor/BlueprintGraph/Classes/K2Node_CallFunction.h:150,209` exports `GetTargetFunction` and `SetFromFunction`. `Private/K2Node_CallFunction.cpp:1772–1780` sets the member reference from the UFunction and the node Blueprint class. The existing exact `FGraphNodeCreator<UK2Node_CallFunction>` avoids function-spawner promotion/operator cache substitutions.
- `Editor/BlueprintGraph/Private/K2Node.cpp:2222–2228` selects the owning Blueprint's skeleton before its generated class. `Runtime/Engine/Classes/Engine/MemberReference.h:146–156` computes the field GUID and self scope; `Runtime/Engine/Private/MemberReference.cpp:78–93` recognizes classes generated by the same Blueprint and clears MemberParent for self context.
- `K2Node_CallFunction.cpp:1377–1401` resolves current function identity through that scope. `MemberReference.cpp:418–422` requests the current class, and `Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:2121–2132` selects the skeleton. Therefore a raw generated UFunction pointer is not the correct sole postcondition for an editor self call.
- `Runtime/Engine/Classes/Engine/Blueprint.h:1081,1251–1254` exposes function GUID lookup. `BlueprintEditorUtils.cpp:9759–9775` resolves an own function graph name to its GraphGuid. Both generated/skeleton resolutions must match the selected local graph.
- `Editor/BlueprintGraph/Private/BlueprintNodeStatics.cpp:26–60` maps a function owner to its authoritative generated class, and when it is the node Blueprint's class creates a `PC_Object/PSC_Self` pin with no class object reference. W verifies this shape, empty self default object/links and self member identity after creation.
- `Editor/BlueprintGraph/Classes/EdGraphSchema_K2.h:1228` exposes `CanFunctionBeUsedInGraph`; implementation `Private/EdGraphSchema_K2.cpp:709–810` validates callable/pure/imperative/latent and generated access conditions. W still explicitly refuses private/protected/editor/delegate/network/latent/wildcard/custom-thunk/internal/deprecated targets.
- Sequence has a public `MinimalAPI` class. `K2Node_ExecutionSequence.h:49` declares virtual `AddInputPin` without a standalone export; direct linkage must not be assumed. Exported `InsertPinIntoExecutionNode` is declared at `:62`, implemented at `Private/K2Node_ExecutionSequence.cpp:208–235` with Modify/CreatePin/name maintenance. Reflected `BlueprintGraphEditor.AddNodePin` is another supported path; no localized action-name lookup is needed.
- Later generated component attachment can use exported `USimpleConstructionScript::CreateNode` (`Runtime/Engine/Classes/Engine/SimpleConstructionScript.h:170`), but implementation `Private/SimpleConstructionScript.cpp:1451–1487` can rename colliding orphan templates into the transient package. Existing collision guards must remain. Creating the component template is a mutation, not read-only resolution.
- Later component getters have a reflected public entry point: `Editor/BlueprintEditorLibrary/Private/BlueprintEditorLibrary/BlueprintGraphEditor.h:291–296`, implementation `:872–911`, obtains the skeleton's exact member FProperty and a variable spawner. Local SCS identity/type/dependency checks are still needed before using it.

## Native W contract

Only `Compatibility/UE58/CubelessFunctionApi.cpp` is changed by this agent; registration, hidden signature and operation envelope remain unchanged.

- `function_class`: existing exact `/Script/Engine.<Class>` static path, or the target Blueprint's **exact current GeneratedClass path** under `/Game/`. External generated classes, inherited functions, overrides, SKEL/REINST selectors and newly uncompiled targets fail before graph mutation.
- The generated route loads only the target Blueprint; it never loads an unrelated caller-selected class asset. Existing load reporting remains: the dirty/status baseline starts after load and implicit PostLoad work is unmeasured.
- Generated and skeleton classes must both be generated by this Blueprint and resolve to its current authoritative generated class. The target's generated UFunction must have nonempty Script, be declared on that exact class, and have an exact own function graph. This proves a compiled target exists, not that its old bytecode reflects every current unsaved body edit.
- Compare generated/current parameter count/order/name/flags and exact pin types, plus relevant callable/purity/access flags. A changed caller is allowed to have Dirty status after skeleton regeneration. A changed callee signature must be explicitly compiled first. Only the existing scalar primitive signature/default grammar is supported; no new object/container/reference grammar is introduced.
- The current skeleton UFunction drives node creation and postcondition identity. Direct recursive authoring (caller graph equals callee graph) is explicitly unsupported. Indirect recursion is not analyzed.
- Reuse the normal editor-thread, no-PIE, normal/idle Blueprint, runtime-parent and loaded-external-instance guards. The node introduces a self GUID/name and self pin; no Cubeless class, function, component or asset is embedded. This is not a complete audit of all pre-existing Blueprint dependencies.
- Preserve all prior caller nodes/pins/defaults/links, and independently observe the callee before/after and require its nodes to be unchanged. Creation failure returns actual partial/unknown state and forbids automatic retry. No implicit deduplication by function name: multiple explicit call sites are valid.
- Explicit compile/save remain false. Additive result data: `requested_function_class`, `compiled_function_path`, `skeleton_function_path`, `function_graph_guid`, `compiled_signature_matches_skeleton`, `self_context`, `callee_before`, `callee_after`, and explicit scope strings. `function_class` remains the requested generated class; `function_path` is the actual current skeleton UFunction observed in the editor.

## Focused verification handed to root

1. On a fresh V duplicate, add three bool members and `CFNL_GatedAdvance(float DeltaSeconds)`, connect three Branches to the own compiled timer self call, then compile/save once. Verify exact callee GUID, generated/current signature, self pin/reference, old graph preservation and no unexpected dirty packages.
2. A different Blueprint class path and a nonexistent/uncompiled own function must fail before mutation; use representative boundary cases, not a new test framework. Existing native Engine AddDouble behavior remains a regression check through the timer fixture.
3. In the existing plugin-free project, run the gated caller on a transient ActorComponent: any false gate leaves the timer unchanged; all true permits the known deltas to accumulate. Keep CDO zero, asset SHA and dirty state unchanged. This proves the authored caller policy; it does not prove actual Tick/input/press/release/EndPlay or FluidNinja integration.
4. Registration, build, execution and reload status must be supplied by root's actual results. No new packaging pass is justified solely by this self-reference addition; prior packaging evidence must not be relabeled as W execution.

## Root executor evidence: W2 plugin / W3 fixture

Actual results now exist separately from the source review above. `registered-schemas-and-diagnostics-W2.json` records8Toolsets/53tools, PID35420 and116pluginfiles. W1 native build passed in19.01s; Python93tests/3.689s and the GUID correction's focused4tests/0.209s passed. The manifest has27 named batch capabilities. `source-snapshot-W2.json` preserves the matching deployed sources before later X edits; this is not a claim that ongoing root sources remain W2.

The first Sequence failure occurred after node creation: equivalent GUIDs differed in string case. `sequence-failure-observation-W.json` resolves the inspected graph as one valid, unlinked3-output Sequence plus entry, with no explicit compile/save and no saved target. The original unknown/failure report and source snapshots under `source-snapshots/W` remain intact; only the owned unsaved fixture was discarded at the controlled restart. Python now compares normalized UUID identities.

W2 author passed but `gated-timer-runtime-W.json` failed at the first transient gate setter because CPF_DisableEditOnInstance forbids instance edits: calls0, preservationPASS, outcome_unknown=true. The W3 author gives only the three test gates is_exposed=True; the plugin and runtime verifier are unchanged. The prior writer is preserved at `source-snapshots/blueprint_gated_timer-W2.py.source`.

`blueprint-gated-timer-W3.json` passed21named calls/30HTTP, stage elapsed sum2.1638213s and whole3.0985859s. Batches have6/9/12children; final explicit compile/save occurs once each. The saved caller has10nodes/11edges. Unlike the original two-output host Sequence, the fixture requests3outputs, leaving then_2 disconnected to exercise public pin extension. Direct self recursion is rejected before changes; generated/skeleton function identity/signatures and source timer/EventGraph preservation are observed. These are single warm NullRHI/loopback measurements, not a speedup claim.

`gated-timer-runtime-W3.json` loads a SHA-matching copy in a fresh no-MCP PID46076. Six calls independently observe each false gate, two all-true accumulation calls, then false again: timer0/0/0/.25/.75/.75; LastDelta always equals the supplied delta. CDOFalse/0, asset SHA/mtime, project descriptor/Config, dirty packages and disabled MCP/Toolsets remain unchanged. Saved editor settings files are observed separately; no global settings getter or settings change is claimed. Explicit compile/save0; body0.1494675s excludes editor startup. This is fresh editor load/direct execution, not official-connector cold, world Tick, input, reset or Shipping execution.

`gated-timer-verification-W3.json` joins15checks including the runtime copy/source fingerprints,116root/deployed W2 hashes,1,059original protected files (only authorized uproject activation differs), actual Codex saved graph equality, output_count=1 preflight rejection with changes=[], saved dependency audit and clean PID35420. Existing V2 clone cold is independently recorded by `actual-codex-cold-duplicate-V2.json`11checks at the W deployment restart. Full original host/external component/inherited Tick/occupied-link replacement and W packaging remain incomplete. All historical raw evidence stays immutable.
