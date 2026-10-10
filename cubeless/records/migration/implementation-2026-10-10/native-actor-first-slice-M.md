# Native Actor first slice M

2026-10-10. Implemented and independently source-reviewed; build, official registration/call, save/reload and no-MCP packaging evidence remain pending. This is six named legacy operations plus the Python `create_actor` alias, not completion of the 35-tool Editor domain.

## Source and scope

Legacy behavior and complete route/schema inventory are in `editor-actor-port-plan-K.md` and `editor-actor-source-inventory-K.json`. The source chain is original `Python/tools/editor_tools.py` → `UnrealMCPEditorCommands.cpp` handlers at 5969–6263 → `UnrealMCPCommonUtils.cpp` actor serializer and mutation contract. The original repository was read only. The new core uses direct public editor/engine APIs and does not copy the old transport, singleton or game classes.

Files: `Compatibility/UE58/CubelessActorApi.h/.cpp`, `Operations/CubelessActorOperations.h/.cpp`, `Registration/CubelessActorLibrary.h/.cpp`. The existing editor module adds the private `EditorFramework` dependency to check the typed-element editor subsystem before transform dispatch. There is no runtime module or gameplay dependency.

The hidden `UCubelessActorLibrary` is a BlueprintFunctionLibrary with six BlueprintCallable/AIIgnore functions. It is only an implementation adapter for the journaled Python Toolset, not another public generic executor. Public Python `create_actor` shares spawn logic while keeping its own journal identity.

## Positional native contracts

All return one JSON native envelope. `MaxActors` is a validated 1..10000 budget; the public wrapper fixes it to 10000. `ExpectedLevelPath` is empty or an exact persistent-level package such as `/Game/CubelessMCPTests/Actor_M/TestMap`, not a world object path. The Python wrapper first observes this package identity, durably records its intent, then passes the observed identity to native mutation.

- `GetActorsInLevel(ExpectedLevelPath, MaxActors)`
- `FindActorsByName(Pattern, ExpectedLevelPath, MaxActors)`
- `GetActorProperties(Name, ExpectedLevelPath, MaxActors)`
- `SpawnActor(Name, Type, LocationJson, RotationJson, ScaleJson, ExpectedLevelPath, MaxActors)`
- `DeleteActor(Name, ExpectedLevelPath, MaxActors)`
- `SetActorTransform(Name, LocationJson, RotationJson, ScaleJson, ExpectedLevelPath, MaxActors)`

The three transform strings contain exactly three finite JSON numbers in xyz or pitch/yaw/roll order. JSON null means preserve the component on Set and use identity on native Spawn; the public spawn adapter supplies explicit zero/zero/one defaults and rejects null. Native numeric magnitude is limited to 1e12; transform input text is limited to 1024 units (Python uses the tighter 512 input limit). Names must be shorter than NAME_SIZE, nonempty, not None, pass FString-based IsValidXName, and contain no NUL/control characters before any FNAME_Add. Type spelling is case-insensitive across StaticMeshActor, PointLight, SpotLight, DirectionalLight and CameraActor; the old Python uppercase/native mixed-case mismatch is corrected.

## Read semantics and mutation limits

Reads enumerate loaded editor-world actors using explicit TActorIterator flags, without outliner, label or selection filtering. Unloaded World Partition descriptors are not actors returned by this query. Each list is path-sorted and includes the original name/class/location/rotation/scale fields plus label, exact object/class/package paths, level ownership, selection and external-package observations. Labels use GetActorLabel(false), avoiding label creation during reads. Find is a case-insensitive substring of UObject name; empty pattern matches all. Inspect uses a case-sensitive exact UObject name and rejects ambiguity across loaded levels.

The initial mutation gate requires a single loaded, saved `/Game/` persistent level with current level equal to persistent level, no World Partition/external actors/streaming levels and no level lock. Mutations target only the five exact Engine native classes; derived Blueprint classes are incomplete. Delete/transform further require a persistent, editable, unlocked, rooted, non-editor-only, non-transient, non-child, unattached actor with no attached child actors and only native `/Script/Engine` components. This deliberately excludes authored/plugin component callback behavior. No property/asset reference is assigned by these operations. Existing arbitrary serialized references or global editor delegates are not claimed fully audited.

Spawning rejects actor name collisions case-insensitively and non-Actor outer/name conflicts. It reserves one slot before dispatch so a full 10000-actor snapshot cannot produce a 10001st actor that cannot be independently inspected. FActorSpawnParameters uses captured OverrideLevel, Required_ReturnNull (never fatal name mode), RF_Transactional, AlwaysSpawn collision handling, OverrideRootScale and no external actor package creation. One full transform is supplied at spawn, avoiding a second transform setter.

Delete uses UEditorActorSubsystem::DestroyActor after checking Actor/Layers/selection subsystem availability. Its selection and layer bookkeeping can change editor state; the before/after snapshots include selection. Transform uses UEditorActorSubsystem::SetActorTransform after checking EditorElementSubsystem. An exactly equal FTransform is skipped without transaction/API mutation; nonzero small changes still dispatch. The postcondition uses the legacy 0.01 tolerance and quaternion transform comparison. Engine movement notifications may invalidate lighting; this is not a requested shader compile or save.

No basic Actor tool compiles, saves, reloads, performs cleanup, retries, or claims atomic rollback. Saved-map changes remain dirty for an explicit later pipeline save.

## Result and failure evidence

Envelope fields include success, operation, stage, data, mutation_attempted, changed, change_state_known, postcondition_passed, dirty_before/after when observable, compile/save request flags, saved=false, atomic_rollback=false and auto_retry_safe=false for mutations. Preflight failures keep mutation_attempted=false. After-read failures or world/level identity changes report change_state_known=false and omit changed; they never claim nothing happened. Reused Capture error messages are context-neutral so an after-read failure does not falsely state that no dispatch occurred.

Mutations keep legacy `contract_version=mutation_result_v1`, mode/apply, dry_run/DRY_RUN, changed, mutation_performed, mutation, before/after, six canonical delta counters, verification, verified_success, postcondition_verified and success_semantics in data. Before/after preserve legacy exists/actor target fields and add world observations. Exact no-op optimization intentionally reports mutation attempted/performed=false while verification succeeds. Primary-request delta counters retain legacy 0.01 changed tolerance; `actor_changes` separately records actual basic-property/selection differences across the complete captured set.

`data.editor_world_path` identifies UWorld; `data.persistent_level_path` is the package guard. Native before/after snapshots include actors, count, selected_actor_paths, dirty, external_actor and World Partition observations. `actor_changes` reports created/removed/updated paths and counts over the captured basic fields, not arbitrary UObject or delegate side effects. A complete after snapshot can establish these observed changes even when API acceptance or the requested postcondition fails; verified success and observed state certainty are separate fields.

One native read uses one enumeration; a mutation uses a local before snapshot plus independent after enumeration. The Python durable-intent observation is separate and not silently reused as trusted native state. Additive performance fields count actual native snapshot enumerations/iterator visits and elapsed time before final serialization, excluding Python journal/transport/final serialization and engine-internal traversal. These are measurements to collect, not an asserted speedup.

## Installed public API evidence

- `Runtime/Engine/Classes/Engine/World.h`: public SpawnActor transform-pointer overload at 3775; spawn name modes and OverrideLevel at 437,496–515; GetLevels/GetCurrentLevel, IsPartitionedWorld.
- `Runtime/Engine/Public/LevelUtils.h:91`: public IsLevelLocked. `Level.h:1298`: IsUsingExternalActors.
- `Editor/UnrealEd/Private/Subsystems/EditorActorSubsystem.cpp:546–625`: deletion bookkeeping and its Layers/selection assumptions; :2686–2707 transform route.
- `Editor/EditorFramework/Private/Subsystems/EditorElementSubsystem.cpp`: SetElementTransform checks the world interface, emits movement notifications and returns acceptance. Native independently compares actual transform because acceptance alone is not value proof.
- `Editor/UnrealEd/Private/Elements/Actor/ActorElementEditorWorldInterface.cpp:66–75`: movement-ended callback calls PostEditMove, lighting invalidation, component update and dirty marking.
- `Core/Public/UObject/NameTypes.h:875–877`: string-based IsValidXName; the length check precedes all authored FNAME_Add calls.

## Required executable proof

Root owns the build and isolated official MCP execution. Verify schema/defaults and AIIgnore exclusion; all five spawn classes; exact object name distinct from label; list/find/inspect; duplicate and case-variant names; partial transform with preserved omitted parts; exact no-op; delete and selected-actor bookkeeping; invalid vectors/names/types; cap boundary through an internal small-budget fixture; unsupported maps/classes/components/attachments/locks; expected-level mismatch; structured unchanged preflight failures; actual journal replay and uncertainty rules. Save the exact level once, restart into a new editor process, and verify identities, transforms and deletion persistence. Package/run the map in an MCP-free project separately before claiming runtime independence.

The six operations are implemented within this bounded scope only. Unsupported streaming/World Partition/external ownership, Blueprint instances, typed arbitrary actor property mutation, richer reflected inspection and the 29 other Editor public tools remain explicitly incomplete.
