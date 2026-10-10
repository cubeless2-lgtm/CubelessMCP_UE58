# Blueprint component add/set: source contract and proposed next slice

2026-10-10. This is a source/API investigation and implementation proposal. No add/set component mutation was implemented or registered by this record. Component read N and its fixture authoring are separate work. Paths below distinguish the read-only legacy repository from the installed UE 5.8 engine; historical code is evidence, not authorization to run historical commands.

## Legacy public tools → command → handler

| Public Python tool | Command and native handler | Actual behavior requiring preservation or explicit correction |
|---|---|---|
| `add_component_to_blueprint` (`Python/tools/blueprint_tools.py:93–172`) | `add_component_to_blueprint` → `UnrealMCPBlueprintCommands.cpp:1280–1431` | Finds Blueprint and class, creates local SCS node/template, attaches to local parent or root, sets optional relative transform, and always calls full CompileBlueprint. Returns requested name/type/parent without validating compiler diagnostics or actual generated names. No explicit save. |
| `set_component_property` (`Python/tools/blueprint_tools.py:266–302`) | `set_component_property` → same C++ file `:1860–2447` → selected `FUnrealMCPCommonUtils::SetObjectProperty` branches (`UnrealMCPCommonUtils.cpp:1787+`) | Finds local SCS template; SpringArm special handling; typed vector/struct/enum/numeric/name paths and generic fallbacks; property notifications differ by branch; marks Blueprint modified but does not explicitly full-compile/save. AttachSocketName also changes the SCS node's AttachToName. |

Legacy roots: `C:/Git/UnrealMCP5_6_1/Python` and `C:/Git/UnrealMCP5_6_1/MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private`. Source SHA-256: Python blueprint tools `a3905dea7c882660f6d40d75150f1abd72fe4e0bf68184e38890fb7515590f43`; native Blueprint commands `fa72cb0d37cfbc2377f6cbf5926f82352b5e07ea570aced4125eec732b32446f`.

### Existing contract defects must remain visible

1. Python forwards nonempty `component_properties`, but the entire native add handler never reads that field. A new implementation must not silently return success for unapplied properties. Implement a validated subset before creation, or explicitly reject nonempty properties until that subset exists. This is a correction to the old public promise, not verified legacy behavior.
2. Add's dynamic class lookup tries requested name and several suffix/prefix variants without a reliable unique-candidate proof. New fixed runtime Engine class mapping is safer but incomplete for arbitrary old classes; register the same named capability while tracking the unsupported class branch.
3. Add compiles with default options, ignores diagnostics, and can report requested names even if CreateNode renamed them. Preserve the compile boundary with N2 SkipSave and expose actual names plus diagnostics. Do not preserve false success or unrequested SaveOnCompile writes.
4. SpringArm's scoped Modify/PostEditChange can run even when conversion fails. Generic setter branches vary in notification behavior. Prevalidate all new typed input before any notification, and mark this intentional correction.
5. The old enum/numeric direct setter's `ApplyDirectPropertyValue` uses `ContainerPtrToValuePtr` and reacquires storage after PreEditChange. It contains the historical fix for writing through the wrong address. Do not substitute UObject header addresses or retain potentially invalidated property/template pointers across callbacks or compilation.

## Installed public engine API and hidden side effects

All following engine paths are relative to `C:/Program Files/Epic Games/UE_5.8/Engine/Source`.

### Creation and exact identity

`Runtime/Engine/Classes/Engine/SimpleConstructionScript.h` exposes CreateNode, AddNode, RemoveNode, GenerateNewComponentName and root access. `Private/SimpleConstructionScript.cpp:1465–1487` checks Blueprint/class validity, requires GeneratedClass, computes a possibly different component variable name, creates a component template, then renames that template under the generated class. Its CreateNodeImpl (`:1437–1443`) owns the node under the SCS.

`RenameComponentTemplateIntoClass` (`:1453–1462`) moves any colliding old `<variable>_GEN_VARIABLE` template out of the generated class into the transient package before moving the new template in. Preflight must reject a conflicting generated-class template slot rather than silently relocating an unrelated orphan template. Check inherited/native/component/variable/function name conflicts as well as the visible local node list.

GenerateNewComponentName is exported, but `GenerateListOfExistingNames` (`:1315+`) traverses generated-class objects, native CDO and skeleton names and calls GetAllNodes. That call can initialize the legacy node cache. Likewise FKismetNameValidator's constructor may enumerate SCS variable names. These are not proven side-effect-free preflight helpers. First bound external names before FName conversion and either use a specifically audited identity check or report possible cache initialization separately. New node names also need room for the engine template suffix.

`AddNode` updates root/all-node collections and calls ValidateSceneRootNodes. Validation (`:1193+`) can remove the old default root and reparent children when another scene root is added. Thus a root add is not necessarily a one-row delta. Initial support should require an explicit existing local scene parent when a root already exists; root replacement stays incomplete until a separate hierarchy test proves its actual delta. An empty valid actor SCS can be a separate tested case.

`RemoveNode` (`:914+`) updates hierarchy/cache; it does not delete every created UObject/template or undo template relocation. Cleanup after a failed creation is an explicit best-effort action with a new observation. Never call it atomic rollback.

### Property writes, propagation and stale pointers

`Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h:2110` exports `UKismetSystemLibrary::Generic_SetEditorProperty`; implementation `Private/KismetSystemLibrary.cpp:3445+` routes through PropertyAccessUtil. `Runtime/CoreUObject/Private/UObject/PropertyAccessUtil.cpp:585+` obtains archetype instances inheriting a template value and propagates a property assignment into those objects. `CanSetPropertyValue` (`:723–750`) checks property flags/template restrictions; it does not by itself prove arbitrary UObject::CanEditChange or callback safety.

Changing a component template can therefore change already loaded derived templates or actor instances outside the requested asset. Initial bounded support should query the public `GetArchetypeInstancesInheritingPropertyValue` API and reject a nonempty external affected-object set unless every affected object/package is deliberately included in the operation contract and durable before snapshot. A single local Blueprint dirty flag is not a complete effect report.

After notifications or full compile, reacquire Blueprint, SCS node, template and FProperty by validated identity. Use typed initialized scratch storage and the actual FProperty for copying; never reuse raw offsets captured before PreEditChange. Inspect template values independently afterward through the N local read core and official property getter where available.

`USceneComponent::SetAttachSocketName` is a public C++ API (`SceneComponent.h:1399`, `.cpp:4429`). The corresponding SCS node public AttachToName is used during runtime SetupAttachment (`SCS_Node.cpp:168`). Preserve the old dual-state behavior deliberately: writing only the template FName is insufficient. This field is not exposed to generic editor-property assignment, so it needs a named special path with two before/after observations and bounded FName input.

### Compilation and save semantics

`Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:1890–1917` implements MarkBlueprintAsStructurallyModified by synchronous skeleton regeneration. Calling it is an implicit compiler action even if a later full compilation is deferred. MarkBlueprintAsModified (`:1922+`) updates caches/custom property lists, status/dirty state and calls PostEditChangeProperty; derived-class and callback effects still need observation.

The old add already performs one direct full CompileBlueprint, and the manager's normal job regenerates skeleton state. A new core may be able to attach the node then perform one full compile with SkipSave, avoiding a redundant explicit structural compile. That is a candidate to test, not an assumption. CompileSynchronously can garbage collect/reinstance/broadcast; no pointer acquired before that boundary is considered stable.

The standalone add must retain full validation with actual compiler messages. An existing batch can later defer repeated full compilation only through an explicit composite contract that proves final state and keeps implicit skeleton activity visible. Set retains its no-full-compile default and reports `compile: not_requested`. Neither mutation saves automatically. The separate explicit save workflow records its actual call and result.

## Proposed first implementation contract (not registered)

Keep public names `add_component_to_blueprint` and `set_component_property`. Use public JSON-string adapters for old heterogeneous dictionaries/Any and nullable arrays, as done for Material tools; do not pretend the wire schema is unchanged.

Candidate public add arguments: `blueprint_name`, `component_type`, `component_name`, `location_json='[0,0,0]'`, `rotation_json='[0,0,0]'`, `scale_json='[1,1,1]'`, nullable `parent_component`, `component_properties_json='{}'`, existing operation ID convention. Candidate set: `blueprint_name`, `component_name`, `property_name`, `value_json`, operation ID. Parse duplicate keys/nonfinite/depth/value-count/size/name bounds before dispatch. JSON string adapters explicitly preserve zero/default vectors and property-value scalar types.

Implementation separation remains Python registration → journal/operation orchestration → hidden named C++ helper → UE58 compatibility. No bridge or public generic native executor. One internal typed property preparation core can serve add initialization and standalone set; independent before/after exporters should be shared with the read core rather than copied.

Suggested first support:

- Saved `/Game` normal Actor Blueprint, current valid GeneratedClass, runtime-safe parent ancestry, local reachable SCS only, no PIE/SIE, no inherited/native CDO template mutation.
- Exact native runtime Engine SceneComponent, StaticMeshComponent, CameraComponent and SpringArmComponent class mapping. Arbitrary Blueprint/plugin component classes remain explicit unsupported until their callback/dependency behavior is verified.
- Boolean, bounded integer/float, bounded FName/FString, declared enum names/values, finite FVector/FRotator/FLinearColor typed values where editor permissions and metadata are valid. No raw aggregate ImportText, unknown struct, delegate or arbitrary UObject/class reference fallback. Each delayed legacy branch remains in the feature matrix.
- Component-property object for add is either prevalidated using exactly this same core or rejected if nonempty. No application of a first property before all later candidates have passed validation. Object/class references may be supported later with exact runtime dependency checks and package proof.
- Native effect envelope records exact node/template identity, actual local hierarchy, dirty/status before and after, propagation set, compile state/errors, save not-requested, mutation_attempted, change_state_known, cleanup action/result and postcondition. Python writes durable intent before actual mutation and before full compile; unknown outcome stops the sequence and forbids automatic retry.

## Verification required before claiming equivalence

1. Named add for each supported class with exact local parent, transform and component properties; independent N/official value readback. Name conflict and orphan template slot collision must fail before relocation. Missing parent, unsupported class, bad/nonfinite/oversized property, invalid enum and NAME_SIZE boundaries leave node/template counts unchanged.
2. Existing root vs new root behavior, default root hierarchy, actual generated name, GUID stability, template ownership and engine-generated variable identity are explicit cases. Unimplemented root replacement is rejected rather than approximated.
3. Typed set round trips include SpringArm float/bool/Rotator, scalar/vector conversion compatibility, numeric range, name/socket dual state, wrong type, readonly/internal property, malformed enum metadata and unsupported aggregate. No unrelated field/asset/instance changes.
4. A loaded child Blueprint and a live actor instance expose propagation: first-slice rejection must precede notifications; broader support later requires independent values for every affected object and package.
5. Full add compile under SaveOnCompile Never/Always/SuccessOnly must use SkipSave and preserve editor setting/config bytes. Failed compile reports persisted mutation and diagnostics without implicit save. Injected readback/checkpoint failure preserves actual partial state without retry.
6. One explicit save, new editor load and repeat component/property read. Finally copy only the resulting game assets into the MCP-free project, compile/cook/package and execute the resulting game without plugin dependence, separately from editor-only verification.
7. Batch optimization compares full/skeleton compiler actions and property/SCS traversal counts as well as elapsed time. Fewer HTTP calls alone do not prove fewer engine operations. Rooted-tree read scope and orphan-array limitations remain visible.

The two named functions are still incomplete. This plan neither marks generic reflection as complete migration nor activates old rules/commands from the original source.
