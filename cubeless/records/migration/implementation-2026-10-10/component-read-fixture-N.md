# Component read fixture N: source-only authoring contract

2026-10-10. `cubeless/tools/verification/create_component_read_fixture.py` prepares three **new** Blueprints for the component read pair. Its authoring has not been executed by the native implementation agent. The root executor owns actual editor execution, official MCP calls, and new-process readback. Offline guard tests are separate from engine evidence.

## Invocation and fixed scope

Execute `author(report_path, run_id, expected_pid)` inside the editor with `runpy.run_path` and a concrete current editor PID. `run_id` must be a fresh 32-character lowercase hexadecimal UUID. `report_path` must be a new `.json` file directly under the project's `cubeless/records/verification/implementation-2026-10-10`. The script refuses existing reports and existing fixture asset directories; a failed run must be inspected, not automatically rerun.

The only permitted project is `cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject`. The actual user project and sibling verification projects are rejected before any asset subsystem is obtained. UE 5.8.3, matching PID, game thread, inactive PIE/SIE, empty dirty-package list and `BlueprintEditorSettings.SaveOnCompile == SoC_Never` are required. No settings are changed. Creation through BlueprintFactory itself compiles without SkipSave; the explicit compile count is therefore not a claim that only three compiler jobs run.

Each engine authoring intent is durably written (fsync and replace) before dispatch, with returned/partial-failure state afterward. No automatic retry or atomic rollback is offered. All preexisting isolated project `.uasset`/`.umap` hashes are compared at the end. No existing asset is explicitly saved.

## Fixture and independent observations

The new directory is `/Game/CubelessMCPTests/Components_<run_id>`:

- `BP_Local`, native Actor parent: existing `DefaultSceneRoot` → `MeshChild` (StaticMeshComponent) → `LightChild` (PointLightComponent). Root renaming is intentionally avoided because DefaultSceneRoot cannot be renamed through the public subsystem. Mesh uses `/Engine/BasicShapes/Cube.Cube`, relative location `[12,34,56]`, rotation `[10,20,30]` in pitch/yaw/roll order, scale `[1.25,.75,2]`, hidden visibility, two fixed tags, and Movable mobility. Light intensity is 1234.5 Lumens.
- `BP_Empty`, native SaveGame parent: expected no SCS.
- `BP_Inherited`, generated parent BP_Local: expected empty **local reachable** SCS. No subobject gathering is performed on this child, because gathering inherited components can create override templates.

Three explicit hidden `compile_blueprint_validation(..., False, False)` calls use the N2 native SkipSave fix. The script refuses any fixture file already on disk before its one `EditorAssetSubsystem.save_loaded_assets([local,empty,inherited], False)` call. It then records three asset hashes, Blueprint statuses, final empty dirty-package list, and source hashes.

Expected values are not produced by `CubelessComponentLibrary`. Authoring uses the public SubobjectDataSubsystem on only BP_Local, then reacquires templates after compilation. The public, deprecated `SubobjectDataBlueprintFunctionLibrary.get_object` returns the raw WeakObjectPtr rather than `GetAssociatedObject`'s blueprint-context path. Independent editor-property getters prove nine values; `GetAttachSocketName` proves the tenth. The official `ToolsetLibrary.get_object_properties` JSON baseline is retained for nine fields. `AttachSocketName` lacks the generic editor-property access flags, so its baseline is explicitly absent with the public getter recorded instead.

The ten fields are RelativeLocation, RelativeRotation, RelativeScale3D, StaticMesh, bVisible, ComponentTags, Mobility, Intensity, IntensityUnits and AttachSocketName. Tags are declared/read back as a JSON array while the native read deliberately returns bounded legacy UE text. Mobility has native integer value 2 and official enum-string encoding. Node GUID is not independently observable through the chosen public Python API: only format, uniqueness, repeated read and cold reload stability may be claimed.

## Installed engine evidence

Paths below are relative to `C:/Program Files/Epic Games/UE_5.8/Engine`.

- `Source/Editor/SubobjectDataInterface/Public/SubobjectDataSubsystem.h`: AddNewSubobjectParams reflected fields, K2_GatherSubobjectDataForBlueprint, AddNewSubobject and RenameSubobject are public UFUNCTIONs. `SubobjectDataBlueprintFunctionLibrary.h:35–50,92–102` exposes handle/data/parent and direct/deprecated object access. The private library `.cpp:19–21` returns `Data.GetObject` directly.
- `Source/Runtime/Engine/Classes/Components/SceneComponent.h:112–113,303,703–705`: AttachSocketName lacks Edit/BlueprintVisible; Mobility is `TEnumAsByte<EComponentMobility::Type>`; GetAttachSocketName is a public UFUNCTION. `Source/Runtime/CoreUObject/Private/UObject/PropertyByte.cpp:152–189` formats this non-enum-class property as `TEnumAsByte<CppType>`.
- `Source/Runtime/Engine/Classes/Components/LocalLightComponent.h:28,57`: IntensityUnits and SetIntensityUnits. The fixture uses public getters/editor getters rather than unsupported unreflected C++ fields.
- `Source/Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h:110,124`: both public CreateBlueprint overloads have no compile-options argument. `Private/Kismet2/Kismet2.cpp:526–533` supplies its own initial compile flags without SkipSave. The fixture therefore requires the current Never preference instead of modifying a global editor preference.
- `Source/Editor/UnrealEd/Public/Subsystems/EditorAssetSubsystem.h:285–286`: public SaveLoadedAssets allows one explicit call for all three new assets.

## Offline verification and remaining gates

`test_component_read_fixture.py` contains seven guard tests: exact isolated path accepted, actual user project rejected, sibling project rejected, report escape/bad UUID rejected, existing evidence retained, wrong-project `author()` proves zero asset-subsystem calls, and numeric/bool readback discrimination. All seven passed with installed engine Python and `-B` on 2026-10-10. This does not verify the Unreal Python tuple bindings, actual subobject hierarchy, enum serialization, inherited local node state, save, or cold reload; root execution and the separate component MCP pipeline must establish those.

Known test limits: no FString/FText component fixture, no malformed/moved enum asset execution, no inherited effective-template read, no orphan AllNodes completeness and no independent GUID source. Unexpected binding/value failures retain a partial report and must not be represented as completed migration.
