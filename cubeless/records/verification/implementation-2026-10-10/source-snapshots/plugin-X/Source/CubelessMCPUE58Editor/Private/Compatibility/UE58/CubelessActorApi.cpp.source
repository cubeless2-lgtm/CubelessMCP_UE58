// UE58 adaptation of the six named UE56 editor Actor commands.
// Original route/behavior evidence: editor-actor-port-plan-K.md. No bridge code.
#include "Compatibility/UE58/CubelessActorApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"

#include "Camera/CameraActor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Level.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelUtils.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Subsystems/EditorElementSubsystem.h"
#include "Layers/LayersSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::Actor
{
namespace
{
TArray<TSharedPtr<FJsonValue>> Triple(double X, double Y, double Z)
{
    return { MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y), MakeShared<FJsonValueNumber>(Z) };
}

TSharedPtr<FJsonObject> SnapshotActor(AActor* Value)
{
    auto Result = MakeShared<FJsonObject>();
    const FVector Location = Value->GetActorLocation();
    const FRotator Rotation = Value->GetActorRotation();
    const FVector Scale = Value->GetActorScale3D();
    Result->SetStringField(TEXT("name"), Value->GetName());
    Result->SetStringField(TEXT("class"), Value->GetClass()->GetName());
    Result->SetArrayField(TEXT("location"), Triple(Location.X, Location.Y, Location.Z));
    Result->SetArrayField(TEXT("rotation"), Triple(Rotation.Pitch, Rotation.Yaw, Rotation.Roll));
    Result->SetArrayField(TEXT("scale"), Triple(Scale.X, Scale.Y, Scale.Z));
    Result->SetStringField(TEXT("actor_path"), Value->GetPathName());
    Result->SetStringField(TEXT("class_path"), Value->GetClass()->GetPathName());
    // false avoids manufacturing a missing editor label during a read.
    Result->SetStringField(TEXT("label"), Value->GetActorLabel(false));
    Result->SetStringField(TEXT("level_path"), Value->GetLevel()->GetOutermost()->GetName());
    Result->SetStringField(TEXT("package_path"), Value->GetPackage()->GetName());
    Result->SetBoolField(TEXT("external_package"), Value->IsPackageExternal());
    Result->SetBoolField(TEXT("selected"), Value->IsSelectedInEditor());
    Result->SetBoolField(TEXT("editor_only"), Value->IsEditorOnly());
    Result->SetBoolField(TEXT("has_root_component"), Value->GetRootComponent() != nullptr);
    Result->SetBoolField(TEXT("transform_finite"), !Value->GetActorTransform().ContainsNaN());
    return Result;
}

bool ParseTriple(const FString& Json, FVector& Out, bool& bPresent, FString& Error)
{
    bPresent = false;
    if (Json.Len() > 1024) { Error = TEXT("Transform JSON exceeds 1024 UTF-16 code units."); return false; }
    TArray<TSharedPtr<FJsonValue>> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Root) || Root.Num() != 1 || !Root[0].IsValid())
    {
        Error = TEXT("Transform input must be one JSON null or an array of exactly three finite numbers."); return false;
    }
    if (Root[0]->Type == EJson::Null) { return true; }
    if (Root[0]->Type != EJson::Array || Root[0]->AsArray().Num() != 3) { Error = TEXT("Transform input must have exactly three numbers."); return false; }
    double Values[3];
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const auto& Item = Root[0]->AsArray()[Index];
        if (!Item.IsValid() || Item->Type != EJson::Number || !FMath::IsFinite(Item->AsNumber())) { Error = TEXT("Transform components must be finite JSON numbers, not booleans."); return false; }
        Values[Index] = Item->AsNumber();
        // Actor component/rendering APIs include float-backed state. Extremely
        // large finite doubles are not safe merely because FTransform is LWC.
        if (FMath::Abs(Values[Index]) > 1.0e12) { Error = TEXT("Transform component magnitude exceeds the supported 1e12 bound."); return false; }
    }
    Out = FVector(Values[0], Values[1], Values[2]);
    bPresent = true;
    return true;
}

UClass* SupportedClass(const FString& Type)
{
    if (Type.Equals(TEXT("StaticMeshActor"), ESearchCase::IgnoreCase)) { return AStaticMeshActor::StaticClass(); }
    if (Type.Equals(TEXT("PointLight"), ESearchCase::IgnoreCase)) { return APointLight::StaticClass(); }
    if (Type.Equals(TEXT("SpotLight"), ESearchCase::IgnoreCase)) { return ASpotLight::StaticClass(); }
    if (Type.Equals(TEXT("DirectionalLight"), ESearchCase::IgnoreCase)) { return ADirectionalLight::StaticClass(); }
    if (Type.Equals(TEXT("CameraActor"), ESearchCase::IgnoreCase)) { return ACameraActor::StaticClass(); }
    return nullptr;
}

bool IsSupportedClass(UClass* Type)
{
    return Type == AStaticMeshActor::StaticClass() || Type == APointLight::StaticClass() || Type == ASpotLight::StaticClass()
        || Type == ADirectionalLight::StaticClass() || Type == ACameraActor::StaticClass();
}

bool SetError(FString& Code, FString& Error, const TCHAR* InCode, const FString& Message)
{
    Code = InCode; Error = Message; return false;
}

UClass* ResolveSavedActorBlueprint(const FString& Path, bool& bLoadAttempted, FString& Code, FString& Error)
{
    auto Reject = [&](const TCHAR* InCode, const FString& Message) -> UClass*
    { SetError(Code, Error, InCode, Message); return nullptr; };
    if (Path.Len() >= NAME_SIZE || !Path.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidObjectPath(Path))
    { return Reject(TEXT("blueprint_path_invalid"), TEXT("Provide an exact /Game Blueprint asset object path shorter than NAME_SIZE.")); }
    const FString PackageName = FPackageName::ObjectPathToPackageName(Path);
    if (!FPackageName::DoesPackageExist(PackageName))
    { return Reject(TEXT("blueprint_not_saved"), TEXT("Compile and explicitly save the Blueprint before spawning it.")); }
    bLoadAttempted = true;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal || !Blueprint->GeneratedClass
        || !Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass())
        || Blueprint->GeneratedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    { return Reject(TEXT("blueprint_actor_class_unsupported"), TEXT("Asset must be a normal Actor Blueprint with an instantiable generated class.")); }
    if (Blueprint->GetOutermost()->IsDirty() || (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings))
    { return Reject(TEXT("blueprint_compile_save_required"), TEXT("The Blueprint must be compiled and clean on disk. No explicit compile/save is dispatched; loading may perform engine PostLoad work.")); }
    const auto Parent = Cubeless::UE58::InspectBlueprintParent(Blueprint->GeneratedClass->GetPathName());
    if (!Parent->GetBoolField(TEXT("success")))
    { return Reject(TEXT("blueprint_runtime_parent_rejected"), Parent->GetStringField(TEXT("error"))); }
    for (const UClass* Class = Blueprint->GeneratedClass; Class; Class = Class->GetSuperClass())
    {
        const FString Package = Class->GetOutermost()->GetName();
        if (Class->HasAnyClassFlags(CLASS_Native) && Package != TEXT("/Script/Engine") && Package != TEXT("/Script/CoreUObject"))
        { return Reject(TEXT("blueprint_native_parent_unsupported"), TEXT("Native parents outside audited Engine/CoreUObject classes remain unsupported in this slice.")); }
    }
    const auto Dependencies = Cubeless::UE58::ReadPackageDependencies({PackageName}, true, 256);
    bool bPassed = false;
    if (!Dependencies->TryGetBoolField(TEXT("passed"), bPassed) || !bPassed)
    { return Reject(TEXT("blueprint_runtime_dependencies_rejected"), TEXT("Saved recursive dependency inspection failed, is dirty/incomplete, or references editor tooling. Inspect ReadPackageDependencies; limit 256 packages.")); }
    // Engine Cube contains a runtime UNavCollision (Runtime/NavigationSystem).
    const TSet<FString> NativePackages = {TEXT("/Script/Engine"), TEXT("/Script/CoreUObject"), TEXT("/Script/PhysicsCore"), TEXT("/Script/MeshDescription"), TEXT("/Script/StaticMeshDescription"), TEXT("/Script/NavigationSystem")};
    for (const auto& Edge : Dependencies->GetArrayField(TEXT("edges")))
    {
        const FString Package = Edge->AsObject()->GetStringField(TEXT("to"));
        if (!Package.StartsWith(TEXT("/Game/")) && !Package.StartsWith(TEXT("/Engine/")) && !NativePackages.Contains(Package))
        { return Reject(TEXT("blueprint_dependency_module_unsupported"), TEXT("Unaudited module or plugin mount in saved dependencies: ") + Package); }
    }
    return Blueprint->GeneratedClass;
}

void AddWorldIdentity(const FState& State, const TSharedPtr<FJsonObject>& Result)
{
    Result->SetStringField(TEXT("editor_world_path"), State.World.IsValid() ? State.World->GetPathName() : FString());
    Result->SetStringField(TEXT("persistent_level_path"), State.LevelPath);
    Result->SetStringField(TEXT("actor_scope"), TEXT("loaded editor-world actors; unloaded World Partition descriptors are not enumerated"));
}
}

FString OperationName(EOperation Operation)
{
    switch (Operation)
    {
    case EOperation::List: return TEXT("get_actors_in_level");
    case EOperation::Find: return TEXT("find_actors_by_name");
    case EOperation::Inspect: return TEXT("get_actor_properties");
    case EOperation::Spawn: return TEXT("spawn_actor");
    case EOperation::Delete: return TEXT("delete_actor");
    case EOperation::SetTransform: return TEXT("set_actor_transform");
    }
    return TEXT("unknown");
}

bool IsMutation(EOperation Operation)
{
    return Operation == EOperation::Spawn || Operation == EOperation::Delete || Operation == EOperation::SetTransform;
}

FString ToJson(const TSharedPtr<FJsonObject>& Value)
{
    FString Result;
    FJsonSerializer::Serialize(Value.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
    return Result;
}

bool Capture(const FRequest& Request, FState& Out, FString& Code, FString& Error)
{
    if (!IsInGameThread() || !GEditor) { return SetError(Code, Error, TEXT("editor_game_thread_required"), TEXT("Actor APIs require the editor game thread.")); }
    if (GEditor->PlayWorld || GEditor->GetPIEWorldContext()) { return SetError(Code, Error, TEXT("play_session_active"), TEXT("Actor tools operate only on the editor world with PIE/SIE stopped.")); }
    if (Request.MaxActors < 1 || Request.MaxActors > 10000) { return SetError(Code, Error, TEXT("invalid_budget"), TEXT("max_actors must be in 1..10000.")); }
    if (Request.Name.Len() >= NAME_SIZE || Request.Pattern.Len() > 4096) { return SetError(Code, Error, TEXT("actor_selector_invalid"), TEXT("Object name must be shorter than NAME_SIZE and pattern must be at most 4096 UTF-16 units.")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    ULevel* Level = World ? World->PersistentLevel.Get() : nullptr;
    if (!World || World->WorldType != EWorldType::Editor || !Level) { return SetError(Code, Error, TEXT("editor_world_unavailable"), TEXT("No persistent editor world/level is available.")); }
    Out.World = World; Out.Level = Level; Out.LevelPath = Level->GetOutermost()->GetName();
    Out.bDirty = Level->GetOutermost()->IsDirty();
    if (!Request.ExpectedLevelPath.IsEmpty() && Request.ExpectedLevelPath != Out.LevelPath)
    {
        return SetError(Code, Error, TEXT("editor_level_mismatch"), TEXT("Persistent level differs from expected_level_path; this observation cannot identify the expected level."));
    }
    TArray<AActor*> Actors;
    // Explicitly include loaded inactive levels; no outliner/label filtering.
    for (TActorIterator<AActor> It(World, AActor::StaticClass(), EActorIteratorFlags::SkipPendingKill); It; ++It)
    {
        ++Out.ActorVisits;
        if (Actors.Num() >= Request.MaxActors) { return SetError(Code, Error, TEXT("actor_inspection_budget_exceeded"), TEXT("Loaded actor count exceeds the inspection budget; this enumeration is incomplete.")); }
        Actors.Add(*It);
    }
    Actors.Sort([](const AActor& A, const AActor& B) { return A.GetPathName() < B.GetPathName(); });
    TArray<TSharedPtr<FJsonValue>> Rows, Selected;
    int32 Matches = 0;
    for (AActor* Value : Actors)
    {
        if (Value->GetActorTransform().ContainsNaN())
        {
            return SetError(Code, Error, TEXT("actor_observation_nonfinite"), TEXT("Loaded actor transform is non-finite; refusing an invalid JSON snapshot: ") + Value->GetPathName());
        }
        auto Row = SnapshotActor(Value);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        Out.ActorsByPath.Add(Value->GetPathName(), Row);
        if (Value->IsSelectedInEditor()) { Selected.Add(MakeShared<FJsonValueString>(Value->GetPathName())); }
        if (!Request.Name.IsEmpty() && Value->GetName().Equals(Request.Name, ESearchCase::IgnoreCase)) { Out.CaseInsensitiveNameMatches.Add(Value->GetPathName()); }
        if (!Request.Name.IsEmpty() && Value->GetName().Equals(Request.Name, ESearchCase::CaseSensitive))
        {
            ++Matches; Out.Target = Value; Out.TargetSnapshot = Row; Out.TargetTransform = Value->GetActorTransform();
        }
    }
    Out.Snapshot = MakeShared<FJsonObject>();
    AddWorldIdentity(Out, Out.Snapshot);
    Out.Snapshot->SetArrayField(TEXT("actors"), Rows);
    Out.Snapshot->SetArrayField(TEXT("selected_actor_paths"), Selected);
    Out.Snapshot->SetNumberField(TEXT("actor_count"), Rows.Num());
    Out.Snapshot->SetBoolField(TEXT("enumeration_complete"), true);
    Out.Snapshot->SetBoolField(TEXT("dirty"), Out.bDirty);
    Out.Snapshot->SetBoolField(TEXT("world_partition"), World->IsPartitionedWorld());
    Out.Snapshot->SetBoolField(TEXT("external_actors"), Level->IsUsingExternalActors());
    // Keep the legacy target-existence contract, with world observations added.
    if (!Request.Name.IsEmpty())
    {
        Out.Snapshot->SetBoolField(TEXT("exists"), Out.TargetSnapshot.IsValid());
        if (Out.TargetSnapshot.IsValid()) { Out.Snapshot->SetObjectField(TEXT("actor"), Out.TargetSnapshot); }
    }
    if (Matches > 1) { return SetError(Code, Error, TEXT("actor_name_ambiguous"), TEXT("Multiple loaded actors have the exact object name; target identity is ambiguous.")); }
    return true;
}

bool PrepareMutation(const FRequest& Request, const FState& Before, FPreparedMutation& Out, FString& Code, FString& Error)
{
    UWorld* World = Before.World.Get(); ULevel* Level = Before.Level.Get();
    if (!World || !Level || World->GetCurrentLevel() != Level || World->GetLevels().Num() != 1
        || World->IsPartitionedWorld() || Level->IsUsingExternalActors() || FLevelUtils::IsLevelLocked(Level)
        || !Before.LevelPath.StartsWith(TEXT("/Game/")) || Before.LevelPath.Len() >= NAME_SIZE
        || Level->GetOutermost()->HasAnyPackageFlags(PKG_NewlyCreated | PKG_EditorOnly)
        || !FPackageName::DoesPackageExist(Before.LevelPath))
    {
        return SetError(Code, Error, TEXT("actor_level_unsupported"), TEXT("Mutations require one saved /Game persistent level, current= persistent, unlocked, non-World-Partition and without external actors/streaming levels."));
    }
    FText NameReason;
    for (TCHAR Character : Request.Name)
    {
        if (Character < 32 || Character == 127)
        {
            return SetError(Code, Error, TEXT("actor_name_invalid"), TEXT("Object names cannot contain embedded NUL or control characters."));
        }
    }
    if (Request.Name.IsEmpty() || Request.Name.Equals(TEXT("None"), ESearchCase::IgnoreCase) || Request.Name.Len() >= NAME_SIZE
        || !FName::IsValidXName(Request.Name, INVALID_OBJECTNAME_CHARACTERS, &NameReason))
    {
        return SetError(Code, Error, TEXT("actor_name_invalid"), TEXT("Provide a nonempty valid exact UObject name shorter than NAME_SIZE; 'None' is reserved."));
    }
    AActor* Target = Before.Target.Get();
    if (Request.Operation == EOperation::Spawn)
    {
        if (Before.ActorsByPath.Num() >= Request.MaxActors) { return SetError(Code, Error, TEXT("actor_inspection_budget_exceeded"), TEXT("Spawning one actor would exceed the independent readback budget; no mutation was dispatched.")); }
        if (!Before.CaseInsensitiveNameMatches.IsEmpty()) { return SetError(Code, Error, TEXT("actor_name_conflict"), TEXT("Actor object name already exists (UObject names are case-insensitive for creation).")); }
        const FName ExistingName(*Request.Name, FNAME_Find);
        if (!ExistingName.IsNone() && StaticFindObjectFast(UObject::StaticClass(), Level, ExistingName)) { return SetError(Code, Error, TEXT("actor_name_conflict"), TEXT("A non-Actor object already owns the requested level object name.")); }
        if (!Request.BlueprintPath.IsEmpty())
        {
            Out.SpawnClass = ResolveSavedActorBlueprint(Request.BlueprintPath, Out.bBlueprintLoadAttempted, Code, Error);
            if (!Out.SpawnClass) { return false; }
        }
        else
        {
            Out.SpawnClass = SupportedClass(Request.Type);
            if (!Out.SpawnClass) { return SetError(Code, Error, TEXT("actor_class_unsupported"), TEXT("Supported exact native types: StaticMeshActor, PointLight, SpotLight, DirectionalLight, CameraActor (case-insensitive spelling).")); }
        }
        Out.DesiredTransform = FTransform::Identity;
    }
    else
    {
        if (!Target) { return SetError(Code, Error, TEXT("actor_not_found"), TEXT("No actor has the exact object name; actor labels are not selectors.")); }
        if (!IsSupportedClass(Target->GetClass()) || Target->GetLevel() != Level || Target->IsTemplate() || Target->IsEditorOnly()
            || Target->IsPackageExternal() || Target->HasAnyFlags(RF_Transient) || !Target->IsEditable()
            || Target->IsLockLocation() || !Target->GetRootComponent() || Target->IsChildActor() || Target->GetAttachParentActor())
        {
            return SetError(Code, Error, TEXT("actor_target_unsupported"), TEXT("Target must be one of the exact five native classes in the supported level, editable, unlocked, rooted, non-transient and unattached; Blueprint actors remain a separate incomplete slice."));
        }
        TArray<AActor*> Attached;
        Target->GetAttachedActors(Attached);
        if (!Attached.IsEmpty()) { return SetError(Code, Error, TEXT("actor_target_unsupported"), TEXT("Actors with attached child actors are not supported by this mutation slice.")); }
        TInlineComponentArray<UActorComponent*> Components(Target);
        for (const UActorComponent* Component : Components)
        {
            if (Component && Component->GetClass()->GetOutermost()->GetName() != TEXT("/Script/Engine"))
            {
                return SetError(Code, Error, TEXT("actor_component_unsupported"), TEXT("Mutation requires Engine native components; authored or plugin component callbacks are outside this slice."));
            }
        }
        Out.DesiredTransform = Before.TargetTransform;
        if (!GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) { return SetError(Code, Error, TEXT("actor_subsystem_unavailable"), TEXT("EditorActorSubsystem is unavailable.")); }
        if (Request.Operation == EOperation::Delete && (!GEditor->GetSelectedActors() || !GEditor->GetEditorSubsystem<ULayersSubsystem>()))
        { return SetError(Code, Error, TEXT("actor_subsystem_unavailable"), TEXT("Deletion requires editor selection and Layers subsystems.")); }
        if (Request.Operation == EOperation::SetTransform && !GEditor->GetEditorSubsystem<UEditorElementSubsystem>())
        { return SetError(Code, Error, TEXT("actor_subsystem_unavailable"), TEXT("Transform requires the editor typed-element subsystem.")); }
    }
    if (Request.Operation != EOperation::Delete)
    {
        FVector Value; bool bPresent = false;
        if (!ParseTriple(Request.LocationJson, Value, bPresent, Error)) { Code = TEXT("actor_transform_invalid"); return false; }
        if (bPresent) { Out.DesiredTransform.SetLocation(Value); }
        if (!ParseTriple(Request.RotationJson, Value, bPresent, Error)) { Code = TEXT("actor_transform_invalid"); return false; }
        if (bPresent) { Out.DesiredTransform.SetRotation(FRotator(Value.X, Value.Y, Value.Z).Quaternion()); }
        if (!ParseTriple(Request.ScaleJson, Value, bPresent, Error)) { Code = TEXT("actor_transform_invalid"); return false; }
        if (bPresent) { Out.DesiredTransform.SetScale3D(Value); }
        if (Out.DesiredTransform.ContainsNaN()) { return SetError(Code, Error, TEXT("actor_transform_invalid"), TEXT("Requested transform is non-finite after conversion.")); }
        Out.bNoOp = Request.Operation == EOperation::SetTransform && Before.TargetTransform.Equals(Out.DesiredTransform, 0.0);
    }
    return true;
}

bool ExecuteMutation(const FRequest& Request, const FState& Before, const FPreparedMutation& Prepared)
{
    UWorld* World = Before.World.Get(); ULevel* Level = Before.Level.Get(); AActor* Target = Before.Target.Get();
    if (!World || !Level) { return false; }
    if (Prepared.bNoOp) { return true; }
    if (Request.Operation == EOperation::Delete)
    {
        const bool bAccepted = GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DestroyActor(Target);
        if (bAccepted && Before.Level.IsValid()) { Before.Level->MarkPackageDirty(); }
        return bAccepted;
    }
    const FScopedTransaction Transaction(FText::FromString(TEXT("Cubeless ") + OperationName(Request.Operation)));
    World->Modify(false); Level->Modify(false);
    if (Request.Operation == EOperation::Spawn)
    {
        FActorSpawnParameters Parameters;
        Parameters.Name = FName(*Request.Name);
        Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ReturnNull;
        Parameters.OverrideLevel = Level;
        Parameters.ObjectFlags |= RF_Transactional;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Parameters.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
        Parameters.bCreateActorPackage = false;
        AActor* Created = World->SpawnActor(Prepared.SpawnClass, &Prepared.DesiredTransform, Parameters);
        if (Created && Before.Level.IsValid()) { Before.Level->MarkPackageDirty(); }
        return Created != nullptr;
    }
    if (!Target) { return false; }
    Target->Modify(false);
    Target->GetRootComponent()->Modify(false);
    const bool bAccepted = GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->SetActorTransform(Target, Prepared.DesiredTransform);
    if (Before.Target.IsValid() && Before.Level.IsValid() && !Before.Target->GetActorTransform().Equals(Before.TargetTransform, 0.0)) { Before.Level->MarkPackageDirty(); }
    return bAccepted;
}

bool VerifyMutation(const FRequest& Request, const FState& Before, const FState& After, const FPreparedMutation& Prepared, bool bApiAccepted)
{
    if (!bApiAccepted || Before.World != After.World || Before.Level != After.Level) { return false; }
    if (Request.Operation == EOperation::Delete) { return Before.TargetSnapshot.IsValid() && !After.Target.IsValid(); }
    AActor* Target = After.Target.Get();
    if (!Target || Target->GetLevel() != After.Level.Get() || !After.TargetTransform.Equals(Prepared.DesiredTransform, 0.01)) { return false; }
    return Request.Operation == EOperation::Spawn ? Target->GetClass() == Prepared.SpawnClass : Target == Before.Target.Get();
}

TSharedPtr<FJsonObject> ReadPayload(const FRequest& Request, const FState& State)
{
    auto Result = MakeShared<FJsonObject>();
    if (Request.Operation == EOperation::List || Request.Operation == EOperation::Find)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto& Value : State.Snapshot->GetArrayField(TEXT("actors")))
        {
            if (Request.Operation == EOperation::List || Value->AsObject()->GetStringField(TEXT("name")).Contains(Request.Pattern, ESearchCase::IgnoreCase)) { Rows.Add(Value); }
        }
        Result->SetArrayField(TEXT("actors"), Rows);
        Result->SetNumberField(TEXT("actor_count"), Rows.Num());
        Result->SetBoolField(TEXT("enumeration_complete"), true);
        Result->SetStringField(TEXT("name_matching"), TEXT("case-insensitive substring of UObject name; empty pattern matches all"));
    }
    else if (State.TargetSnapshot.IsValid()) { Result->Values = State.TargetSnapshot->Values; }
    AddWorldIdentity(State, Result);
    return Result;
}

TSharedPtr<FJsonObject> DescribeChanges(const FState& Before, const FState& After)
{
    auto Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Created, Removed, Updated;
    TArray<FString> Paths;
    After.ActorsByPath.GetKeys(Paths); Paths.Sort();
    for (const FString& Path : Paths)
    {
        const auto* Old = Before.ActorsByPath.Find(Path);
        if (!Old) { Created.Add(MakeShared<FJsonValueString>(Path)); }
        else if (ToJson(*Old) != ToJson(After.ActorsByPath[Path])) { Updated.Add(MakeShared<FJsonValueString>(Path)); }
    }
    Before.ActorsByPath.GetKeys(Paths); Paths.Sort();
    for (const FString& Path : Paths) { if (!After.ActorsByPath.Contains(Path)) { Removed.Add(MakeShared<FJsonValueString>(Path)); } }
    Result->SetArrayField(TEXT("created_actor_paths"), Created);
    Result->SetArrayField(TEXT("removed_actor_paths"), Removed);
    Result->SetArrayField(TEXT("updated_actor_paths"), Updated);
    Result->SetNumberField(TEXT("created_count"), Created.Num());
    Result->SetNumberField(TEXT("removed_count"), Removed.Num());
    Result->SetNumberField(TEXT("updated_count"), Updated.Num());
    Result->SetNumberField(TEXT("affected_count"), Created.Num() + Removed.Num() + Updated.Num());
    Result->SetBoolField(TEXT("package_dirty_changed"), Before.bDirty != After.bDirty);
    Result->SetStringField(TEXT("scope"), TEXT("loaded actor identity/basic-property/selection observations; not arbitrary UObject or delegate side-effect audit"));
    return Result;
}
}
