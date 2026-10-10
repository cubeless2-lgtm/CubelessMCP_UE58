# Native local SCS inspection slice N

2026-10-10. Source implementation only until a separate build and official MCP execution record exists. These two helpers do not establish complete migration of the legacy Blueprint component domain.

## Contract and ownership

The new editor-only `UCubelessComponentLibrary` is a hidden `UBlueprintFunctionLibrary`. Its functions have `BlueprintCallable, meta=(AIIgnore)` so Python can use reflected calls without registering another public MCP Toolset. No runtime module, gameplay dependency, engine source edit or Build.cs change is introduced.

| Hidden Python method | Positional arguments | Return |
| --- | --- | --- |
| `unreal.CubelessComponentLibrary.list_blueprint_components` | `blueprint_path: str, component_name: str, max_components: int` | JSON string |
| `unreal.CubelessComponentLibrary.get_component_property` | `blueprint_path: str, component_name: str, property_name: str, max_components: int, max_value_chars: int` | JSON string |

An empty component name lists all local reachable components. Property get requires nonempty exact component name and property name. The native helper accepts an exact Blueprint package path or object path. Short-name/generated-class/export-text/subobject aliases are not supported here. Public Python registration and any legacy `blueprint_name` adapter are a separate work item; this change alone adds zero discoverable MCP tools.

Registration files contain only reflected adapters; `Operations/CubelessComponentOperations.*` builds result/observation envelopes; `Compatibility/UE58/CubelessComponentApi.*` owns installed UE5.8 API access. Existing Actor M and Material L implementations remain unchanged.

## Source provenance and actual UE5.8 APIs

Read-only legacy sources:

- `C:/Git/UnrealMCP5_6_1/Python/tools/blueprint_tools.py:175,550` defines the two public tools. SHA256 `A3905DEA7C882660F6D40D75150F1ABD72FE4E0BF68184E38890FB7515590F43`.
- `C:/Git/UnrealMCP5_6_1/MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp:1434,2450` handles the commands; `PropertyValueToJsonValue` provides old property encoding. SHA256 `FA72CB0D37CFBC2377F6CBF5926F82352B5E07EA570ACED4125EEC732B32446F`.

Installed engine root: `C:/Program Files/Epic Games/UE_5.8/Engine/Source`. The implementation uses public `FindObject/LoadObject<UBlueprint>`, UBlueprint SimpleConstructionScript/Status, USimpleConstructionScript GetRootNodes, USCS_Node GetChildNodes/GetVariableName/ComponentTemplate/ComponentClass/VariableGuid, public scene/static-mesh getters, TFieldIterator and typed FProperty readers.

- `Runtime/Engine/Classes/Engine/SimpleConstructionScript.h:84` and `SCS_Node.h:123` return root/child array references without modifying the tree.
- `Runtime/Engine/Private/SimpleConstructionScript.cpp:1437–1443` creates local nodes with the SCS as their outer; N validates that ownership rather than silently reading a foreign tree.
- `SimpleConstructionScript.cpp:870–885` can lazily fill the AllNodes cache in GetAllNodes using const_cast. N does not call GetAllNodes or FindParentNode.
- `Editor/SubobjectDataInterface/Private/SubobjectDataSubsystem.cpp:397–408` may ask for inherited editable objects while gathering. `SubobjectData.cpp:256+` may create override templates. N does not call that gathering/editable-object path.
- `Runtime/CoreUObject/Public/UObject/UnrealType.h` exposes typed property readers, FScriptArrayHelper and ExportTextItem_Direct. The generic exporter is allowed only for a pre-inspected bounded plain-scalar array; arbitrary native struct/map/set/delegate exporters remain unsupported.

No claims are based on guessed Python exposure of UBlueprint internals. The Python caller sees only the verified reflected library signatures.

## Preserved payload and deliberate limits

List data preserves `blueprint_name`, `has_simple_construction_script`, `component_count`, `components`, and each old component row's name, variable name, GUID, class/path, template/path, parent name, scene relative transform and static mesh path. Rows are sorted by variable name rather than preserving AllNodes storage order. A filter that matches no node produces an empty successful list. Get data preserves component name/class, requested property name, C++ type, property value and success; additional list/diagnostic fields are additive.

Traversal visits only nodes reachable from this Blueprint's local SCS roots, with a visited/name set and total pending-node budget. It does not include inherited effective overrides, native CDO components or orphan AllNodes entries. These limits are returned explicitly. A repeated node, cycle, duplicate variable name, foreign owner or exceeded budget fails instead of returning a misleading partial-success list. Selected rows additionally require valid template/class metadata, a compatible template class and finite transforms. Filtered-out rows receive tree/name checks but their template values are not inspected. Root/child traversal matches ordinary local trees, but orphan behavior and sorting differ from the old implementation; full legacy equivalence remains incomplete.

Property names are compared without constructing a new FName from request text. Supported values are bool, finite numeric values exactly representable as JSON doubles, string/name/text, enums, FVector/FRotator, and object paths. The legacy serializer tested FNumericProperty before FByteProperty, so byte enums remain numeric. FEnumProperty uses its declared name. Arrays of at most 256 plain scalar/name/string/enum elements retain the legacy Unreal text-string encoding after conservative bounded inspection. Array return values are not silently changed into JSON arrays.

Unsupported cases are explicit errors: inexact integers beyond 2^53-1, nonfinite values, fixed-size native arrays, arbitrary structs beyond FVector/FRotator, maps/sets/delegates/custom exporters, oversized scalar text and arrays whose conservative escaped-text estimate exceeds the limit. Some valid arrays may be rejected by that estimate. Long FString content remains allowed up to the requested text budget; this is an output limit, not a bound on temporary copies or the material write ImportText NAME_SIZE limit. N2 validates missing enum/underlying metadata, including an empty enum array, before any typed read/export/GetCPPType. Object/class/soft-class metadata is also checked before object reads and C++ type reporting.

Limits: 1..10000 reachable nodes, 64..65536 UTF-16 value characters, 256 scalar array elements, and selectors/object paths shorter than NAME_SIZE with no controls. Property get uses an exact local variable name. Reads may inspect editor assets/classes; they create no asset references, components, templates or saved dependencies.

## Observation and failure semantics

Native results have success/operation/stage/data, mutation_attempted=false, compile_requested=false, save_requested=false, saved=false, dirty/status before and after where observed, loaded_during_read, postcondition_passed, changed/change_state_known, performance and limits. Actual Blueprint dirty/status changes fail the operation; the failure envelope preserves those observations. A failed load has no post-load baseline and reports unknown change state. Changed is scoped to the observed Blueprint dirty/status, not every possible UObject field.

Asset loading and reference resolution can run engine PostLoad work. The baseline is captured after loading the Blueprint; N does not claim loading is inert or prove all engine-internal work absent. No explicit compile, save, structural-modification, Modify, inherited override creation or UI API is called. auto_retry_safe is true only for a successful already-loaded read whose scoped observations remain unchanged; failed/loaded reads are not automatically retried by this implementation.

The property exporter does not promise disk persistence or independent property correctness by itself. `postcondition_passed` means the bounded read completed and its dirty/status observations remained stable. Future official readback/fixture evidence must independently establish values.

Error codes include editor_game_thread_required, component_budget_invalid, component_selector_invalid, blueprint_path_invalid, blueprint_not_found, component_budget_exceeded, component_tree_invalid, component_tree_ambiguous, component_observation_nonfinite, component_not_found, component_property_not_found, component_property_type_unsupported and component_read_state_changed. Failed reads retain any safely assembled diagnostic data, with success=false. There is no rollback claim.

Performance reports actual local_scs_nodes_visited and native elapsed time before result serialization. The list and property functions share one traversal core; get performs no second component enumeration. No speedup is claimed before measurements.

## Required proof before completion

1. Build the six new files with the installed UE5.8 toolchain and confirm hidden methods remain absent from official discovery.
2. Add two typed Python wrappers retaining legacy public identities; describe/register and call them through the official MCP connection in the isolated verification project.
3. Compare a loaded three-node local SCS to independently prepared fixture values: root/child names, transforms, static mesh, bool/numeric/name/string/byte enum/FEnum/object properties and bounded array text. Empty-SCS/nonactor Blueprint and inherited/native components must have explicit expected scope.
4. Probe exact filter/missing component/unknown property/nonfinite/budget/1024-character selectors and unsupported export types without treating a successful registration as functional verification.
5. Compare dirty/status, local node/template counts and independently read values before and after. Test freshly loaded versus already-loaded assets separately. Corrupt-tree tests must be synthetic or disposable fixtures, never user assets.
6. Save fixture assets explicitly outside these read operations, restart, repeat reads and retain the original source/API limitations in the final feature matrix.

Current status: native source implemented; build, Python registration, runtime calls, independent value proof and cold reload are pending. Existing L2 guard audit and M material-positive evidence do not verify these new component helpers.

## N2 corrections

The first build failed at result serialization: MakeShared returns TSharedRef, so calling ToSharedRef on it was invalid. N2 passes the existing shared reference directly. The failed build log `build-component-N1.log` is retained.

Independent Ops review found the moved/deleted-enum case documented by installed `Runtime/CoreUObject/Private/UObject/EnumProperty.cpp:411–425`; `GetCPPType` at :385–388 checks enum and underlying metadata. N2 rejects invalid descriptors before dereference and checks empty array inner descriptors as well. Python review proposed class/template consistency for selected rows, which N2 also checks. No intentionally invalid descriptor was executed in the engine.

Separately, N2 fixes the existing Blueprint compile helper to pass `EBlueprintCompileOptions::SkipSave`. Earlier source used None, so the editor's SaveOnCompile preference could save even when the tool requested bSave=false. Explicit validated save remains owned by the existing bSave branch. Source reasoning and required preference-mode counterexamples are recorded in `blueprint-compile-skip-save-correction-N2.md`; old frozen source/logs are not rewritten.
