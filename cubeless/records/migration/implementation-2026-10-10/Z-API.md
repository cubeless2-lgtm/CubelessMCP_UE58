# Z: Actor event override and explicit parent call

Source candidate; build and actual execution evidence are pending root verification. Engine/original sources are read-only. No runtime Cubeless type is written to game assets.

## Contract and origin

UE56 `Python/tools/node_tools.py:335` registers `add_blueprint_event_node`; active `UnrealMCPBlueprintNodeCommands_Part1.cpp:1497` dispatches to `UnrealMCPCommonUtils.cpp:1377`. That helper finds the parent/interface/generated event, reuses an existing override, otherwise creates an event and marks the Blueprint modified; it does not compile/save. The current source has no separate public/active command for creating a parent-function call. `add_blueprint_parent_function_call` is therefore a **new explicit extension**, not an additional migrated UE56 tool.

Hidden Editor-only `CubelessEventLibrary` signatures:

- `add_blueprint_event_node(graph_path, event_name, x, y)`
- `add_blueprint_parent_function_call(graph_path, event_node_path, x, y)`
- `inspect_saved_actor_parent(class_path)` (read-only, already loaded exact class)

Python retains the legacy Blueprint/graph selectors and resolves them before these helpers. No implicit graph creation or native asset load is performed. Parent-call event selectors are resolved to exact object paths. Both mutation helpers return `success`, `stage`, `error_code/error` on failure, `mutation_attempted`, `changed` (nullable if unknown), `change_state_known`, `postcondition_passed`; compile/save requested/attempted/outcome are false. `data` includes node GUID/object path/class, graph identity, complete selected graph node/pin/link snapshots, `created/reused`, ghost/enabled state, preservation observations. Parent calls separately record compiled function identity, editor-resolved target and serialized declaration. Newly created nodes serialize the authoritative generated parent; existing factory nodes may retain the exact native original declaration as described below.

Existing exact events/parent calls are no-op reuse: no reposition, automatic linking, ghost promotion, compile, save or repair. Ambiguous/cross-graph overrides and mismatched existing parent calls fail before mutation. A failed postcondition retains the created node and actual observations; no rollback or retry is claimed.

## Installed UE58 public API basis

- `BlueprintGraph/Classes/K2Node_Event.h`: public EventReference/bOverrideFunction; exact public `FGraphNodeCreator<UK2Node_Event>` creation. `K2Node_Event.cpp:347–369` creates `OutputDelegate`, `then`, and typed event parameters. The delegate pin must be counted even for ReceiveTick.
- `EdGraphSchema_K2.h:916,938`: exported `FunctionCanBePlacedAsEvent` and `CanKismetOverrideFunction`. Actor.h:2178–2180 declares ReceiveTick as BlueprintImplementableEvent(float), without the ordinary BlueprintCallable flag.
- `K2Node_CallParentFunction.h/.cpp:57–94`: public virtual SetFromFunction retains an explicit parent context, and parent self pin is hidden. Dispatch through the exported base virtual avoids a direct link to a non-exported MinimalAPI override. Root build must confirm linkage.
- `BlueprintEditor.cpp:5616–5646` uses the same exact parent creator/SetFromFunction/Finalize sequence in the editor. We call the engine APIs directly, without editor selection/UI state.
- `MemberReference.cpp:421–422` plus `BlueprintEditorUtils.cpp:2121–2132` resolve generated function targets through current skeletons in editor scope. Compiled identity, actual resolution, and serialized class are verified separately.
- `BlueprintNodeStatics.cpp:26–60` builds self-pin type from the first declaration and converts generated classes to their authoritative class. For the inherited Actor ReceiveTick override this is Actor, not necessarily the direct generated parent.
- `KismetEditorUtilities.h:542`, `Kismet2.cpp:671–747` AddDefaultEventNode additionally creates/wires a parent call and ghost nodes. It is not substituted for the legacy single-event operation. Factory-created nodes are observed/reused instead.
- `EdGraphSchema_K2.cpp:883–895` GetCallableParentFunction walks upward from the supplied function's **declaring** class. Before a new child override exists in the skeleton, inherited lookup could select the grandparent. Our parent operation explicitly resolves `BP.ParentClass.ReceiveTick`, then verifies its own compiled/current skeleton identity without an extra skeleton compile.

## Current bounded support

Event: normal idle Actor Blueprint, directly owned standard K2 event graph; validated Game generated or Engine native ancestry; runtime overridable event with at most 128 scalar primitive input parameters. Object/struct/enum/container/reference/network/latent signatures, interface fallback and implicit event-graph creation remain pending. At most 256 graphs, 2048 nodes per scanned graph and 16384 pins in the selected snapshot. Existing factory events may name the original native declaration while manually authored overrides name a generated parent; both must refer to validated ancestry with matching typed pins.

Parent call: only ReceiveTick owned by the direct saved/clean/compiled Actor Blueprint parent whose native parent is Actor. Deeper inherited implementations/native-only Tick remain pending. Existing graph/NodeGuid/PinId, typed DeltaSeconds, hidden self type, member name/GUID (availability explicitly reported), explicit declaration and corresponding editor-resolved target are checked. The two accepted reference modes are the direct generated parent/current skeleton, and an existing node's exact original native Actor.ReceiveTick declaration/zero native GUID. No event links are added implicitly.

The existing saved generated-component inspector now has a small internal common core and separate named Actor-parent wrapper. Component results and scope remain intact. The Actor wrapper verifies every generated ancestor is saved, clean, authoritative, normal, compiled and idle; native ancestry is Engine/CoreUObject. It reuses the existing 256-package recursive dependency audit and runtime script allowlist. `generated_parents` rows contain class_path, blueprint_path, package_name, generated_class, dirty, status, saved_asset; `parent_chain` contains class paths. It does not certify arbitrary unsaved/transitive soft references or packaged execution.

The representative Z pipeline should author the parent before creating a child, preserve factory event/parent links, introduce the Sequence using X's guarded three-endpoint replacement, and use Y's generated component getter/call. Direct ReceiveTick invocation in a disposable no-MCP editor verifies that function behavior, not scheduling by an actual running game's Tick system. Full original FluidNinja host parity still requires external component variable writes, engine instance calls, hardware input, Niagara operations and the original runtime assets.

## Z2 factory parent reference and Z3 correction

The retained `blueprint-inherited-tick-Z2.json` failed before parent-call mutation because the initial identity guard assumed that every parent call serialized the generated parent. The observed factory node instead retained `/Script/Engine.Actor:ReceiveTick`, its native owner and zero function GUID, with its original event execution/data links. No retargeting or repair is needed for that representation:

- `BlueprintGraph/Private/K2Node_CallFunction.cpp:2440–2443` installs `FKCHandler_CallFunction` for this node family.
- `BlueprintGraph/Private/CallFunctionHandler.cpp:663–684` has a dedicated `UK2Node_CallParentFunction` path. It looks up the member name starting at `Context.NewClass->GetSuperClass()` and uses the first implementation's declaring class. The serialized owner and editor `GetTargetFunction()` do not choose this compiler calling context.
- The same file at `800–821` resolves the function in that context. A saved parent in UpToDate or UpToDateWithWarnings state remains on its compiled class (`Blueprint.h:786–789` defines both as up to date). At `584–588` the resulting function is assigned to the compiled statement and `bIsParentContext` is true.
- `KismetCompiler/Private/KismetCompilerVMBackend.cpp:1256,1331,1377–1393` takes that statement function and emits a final function call with its actual UFunction pointer for parent context. This prevents a virtual redispatch back into the child's override.

The Z3 change therefore adds only no-op reuse of the exact first native declaration reached from the verified parent's compiled UFunction super chain, requiring Actor as that declaration owner, the exact native function object, native zero GUID, matching typed pins and hidden/unlinked self, plus the existing graph preservation checks. Generated-parent references and new-node creation retain their previous paths. It does not accept arbitrary same-name owners or modify an existing reference.

`data.serialized_reference_mode` distinguishes `original_native_declaration` from `direct_generated_parent`. `effective_compile_parent_class` and `effective_compile_function_path` report the source-derived nearest-parent resolution separately from `function_path`/`serialized_member_parent_class`. `compile_resolution_basis` records the compiler rule; `runtime_execution_verified=false` prevents this source inspection from claiming executed behavior. Root's subsequent parent-marker runtime cases are the separate execution proof. Build and runtime evidence for this correction remain pending at source freeze.
