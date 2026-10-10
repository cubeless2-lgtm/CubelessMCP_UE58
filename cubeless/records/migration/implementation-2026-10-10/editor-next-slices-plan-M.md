# Editor/Blueprint next slices after Actor M

2026-10-10. Source/API plan, not runtime verification. Actor M compiled successfully per root (`build-actor-M.log`, UBT 20.38s, compile actions 13.13s); its official execution remains separate. No existing Actor/Material core is modified by this plan.

## Priorities and feature identities

1. **Blueprint component read pair:** preserve `list_blueprint_components(blueprint_name, component_name='')` and `get_component_property(blueprint_name, component_name, property_name)`. These belong to the original Blueprint domain, not the Editor35 count. They establish independent component/default readback needed before writes. Root authorized new hidden named native helpers first; Python registration will follow a separate assignment.
2. **Actor transform batch:** expand the existing migrated `execute_batch` allowlist to the six implemented Actor actions plus `create_actor`, after Actor M author/reload proof. This preserves a legacy composite capability and is not a new eighth Actor tool. Keep old batch command `params.location/rotation/scale` array adapters as well as explicitly documented new JSON-string adapters; reject conflicting representations.
3. **Actor typed property setter:** preserve Editor35 `set_actor_property(name, property_name, property_value)` with an explicit `value_json` public adapter, expected-level guard and operation ID. First allow safe runtime Engine instance scalar fields on M's exact five classes. Preserve old output actor/property/success/actor_details, and add independent typed before/after evidence. Reference-bearing types, noneditable/internal fields, authored components and arbitrary BP instances stay incomplete until separately audited.
4. **Blueprint local-SCS component add + property set:** preserve `add_component_to_blueprint` and `set_component_property`; use the read pair to prove actual template values. Name/class/parent/property validation must precede node/template creation. Keep implicit skeleton compile and explicit full compile distinct. Do not silently drop the old standalone add's compile step merely to improve batch timing.
5. **Blueprint Actor spawn and inherited components:** Editor35 `spawn_blueprint_actor` needs runtime-safe generated-class resolution and construction-script side-effect accounting. Inherited effective component default edits need override ownership and instance propagation evidence. These are not safe aliases to M native actor creation or local-SCS reads.

Of Editor35, only set_actor_property/spawn_blueprint_actor among the next items add new Editor identities. Component APIs and batch are cross-domain dependencies; do not count them as missing/new Editor public tools. The other 27 Editor identities remain the explicit groups in `editor-actor-port-plan-K.md`.

## Actual installed UE58 API findings

Paths below are relative to `C:/Program Files/Epic Games/UE_5.8/Engine/Source`.

### Local component reads

`Runtime/Engine/Classes/Engine/SimpleConstructionScript.h:78,84,108` exposes GetAllNodes/GetRootNodes/FindSCSNode; SCS_Node.h exposes ComponentTemplate, ComponentClass, GetVariableName/GetChildNodes and public GetActualComponentTemplate. The legacy list/get uses the current Blueprint's local SCS, not native CDO components or inherited effective templates. Keep that scope explicit.

**Read-side effects matter:** `SimpleConstructionScript.cpp:870–885` GetAllNodes can fill an older object's empty AllNodes cache from roots using const_cast. Prefer direct GetRootNodes/GetChildNodes traversal for a strictly non-authoring read, or explicitly report/cache-check GetAllNodes's legacy initialization if exact orphan-array semantics are required. Normal local trees can be traversed with a bounded visited set, cycle/duplicate-name detection and parent map in one pass. Orphan nodes outside the rooted tree are not proven by a root traversal; this difference must be labeled and must not count as full legacy equivalence.

**Do not use broad subobject gathering as a supposedly harmless query:** `Editor/SubobjectDataInterface/Private/SubobjectDataSubsystem.cpp:397–408` calls GetObjectForBlueprint on inherited nodes while gathering. `SubobjectData.cpp:256+` can create editable inherited override templates. Public API availability alone does not prove no authored-state changes. Inherited effective reads therefore require a separate exact no-create API path and dirty/object/template-count proof.

Initial hidden contract proposal: `CubelessComponentLibrary.ListBlueprintComponents(BlueprintPath, ComponentName, MaxComponents)` and `GetComponentProperty(BlueprintPath, ComponentName, PropertyName, MaxComponents, MaxValueChars)`, returning JSON read-only envelopes. Use an exact package/object path resolved in the Python compatibility layer; unsupported short-name ambiguity is explicit. No Blueprint compilation or save as a side effect of list/get. Type conversion must preserve legacy bool/numeric/string/name/text/object-path, FVector/FRotator arrays, enum behavior and bounded fallback export semantics, while recording deliberate precision/nonfinite/type restrictions.

### Property mutation

Legacy Editor handler :6707 calls `FUnrealMCPCommonUtils::SetObjectProperty` (:1787). That utility directly writes bool/int32/float/string/LinearColor/class/object/enum/byte branches and returns without general Modify/PreEditChange/PostEditChange notification. Copying it verbatim would preserve unsafe conversion and weak persistence behavior.

Public `UKismetSystemLibrary::Generic_SetEditorProperty` is declared in `Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h:2110` and implemented at `Private/KismetSystemLibrary.cpp:3445+`. It uses editor permission checks and change notification, and may propagate template values through archetype instances. Candidate native call: validated FProperty name + typed scratch storage + Generic_SetEditorProperty(Object, Property->GetFName(), Scratch, Property, OnChanged), then fresh property lookup/readback and dirty evidence. Do not call it on CDO/templates in the initial Actor slice. Preflight needs full scalar conversion and FName safety before engine notification; failed validation must never touch the actor.

Candidate first scalar fields include AActor InitialLifeSpan (EditAnywhere float at Actor.h:792–794) and bHidden (EditAnywhere bool at :364–365, with visibility notification behavior separately tested). Tags is an array and was not supported by the old generic setter's scalar branches; do not use it to claim old type equivalence without explicit scope. Do not expose unsafe instance flags, RootComponent, owner/world pointers, delegates, soft paths or custom struct imports through a generic fallback. Full legacy class/object branches remain incomplete until asset dependency/class/meta-class validation and no-MCP saved-level proof exist.

### Structure changes and compilation

`USimpleConstructionScript::CreateNode` is exported but has check(Blueprint), class-child check and a generated-class ensure (`SimpleConstructionScript.cpp:1465–1487`), generates a possibly different name, and creates/renames a component template. Preflight valid generated class, exact safe names and all property candidates before calling it. Re-read actual variable/template names; never echo the requested name as proof. Public AddNode/RemoveNode/AddChildNode are available, but cleanup after a failure is a separate observed action, not atomic rollback.

`FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified` (`BlueprintEditorUtils.cpp:1890–1917`) calls `FBlueprintCompilationManager::CompileSynchronously(...RegenerateSkeletonOnly...)`. Therefore even a deferred full-compile batch cannot promise zero implicit compilation. Record skeleton refresh separately and compile once at the explicit full-validation boundary when compatible with the requested workflow. Existing standalone add called FKismetEditorUtilities::CompileBlueprint unconditionally, so its compile contract must be retained or the unsupported/different behavior explicitly surfaced.

## Transform batch contract and optimization boundary

Old Python batch accepts 1..100 commands and forwards to bridge `batch`; original bridge :1523 creates one FScopedTransaction and dispatches sequentially on game thread. The migrated batch already owns a journal, named allowlist, per-item before-engine durable checkpoints, stop_on_error and forced stop on unknown state. Reuse Actor action cores directly, preserving alias identity, then canonicalize old array inputs. Do not nest another journal per item or declare a transaction rollback guarantee.

Each item must carry/derive the expected level identity, retain actual before/after and saved/compiled=false, and stop after uncertain dispatch/readback/checkpoint failure even when stop_on_error=false. Known no-effect errors may continue only according to the existing explicit batch policy. A successful earlier item must remain visible when a later item fails.

First implementation may retain M's native before+after enumerations and Python durable-before observation per item. It reduces HTTP round trips without claiming fewer actor traversals. Measure counts/latency for 1/10/100 transforms before designing an internal request-scoped lookup map. Any later optimization must preserve each requested postcondition and durable boundary; caller-supplied or cross-tick actor snapshots are not trusted cache inputs. A giant native loop with no per-item durable checkpoints is not an equivalent implementation.

## Test gates for each proposed slice

- Read pair: empty and three-node local SCS; exact filter/missing name; bool/numeric/name/string/enum/vector/rotator/object values; inherited/native components explicitly excluded; cycle/budget/unknown property failures; dirty flag, Blueprint status, local node/template counts unchanged. A fresh loaded asset and a previously loaded asset are separate cases.
- Property: supported scalar round trip via official independent get_properties; wrong type/nonfinite/overflow/long names/private/readonly/object-reference rejection with unchanged state; callbacks observed; exact one save and new-process level reload; unsupported branches remain visible.
- Batch: repeated target partial transforms, expected-level mismatch, middle known failure, unknown readback/journal failure forced stop, no-op, replay/no automatic retry, aliases, one transaction ownership and no implicit compile/save. Compare results with equivalent single calls, then record API traversal counts and elapsed distributions.
- Component add: exact class/name/parent ownership, attach/root hierarchy, invalid property before creation, partial creation cleanup evidence, skeleton/full compile diagnostics, instance/template propagation, save/reload and no-MCP runtime package. Inherited override and arbitrary class branches have separate tests.

No source in the frozen Actor M or Material L slices is changed by these plans. Proposed API declarations are source-verified, not execution evidence; new implementations will get their own freeze/build/runtime records.
