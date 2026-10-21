// CO: original transient skeletal bone fixture13 and live inspector3, Editor host only.
#include "Compatibility/UE58/CubelessSkeletalSimulationApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/Skeleton.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::SkeletalSimulation
{
namespace
{
using Variable::FResult;
struct FContext
{
    TStrongObjectPtr<USkeletalMesh> Mesh { nullptr };
    TStrongObjectPtr<USkeleton> Skeleton { nullptr };
    TStrongObjectPtr<UPhysicsAsset> Physics { nullptr };
    TStrongObjectPtr<UWorld> World { nullptr };
    TStrongObjectPtr<AActor> Actor { nullptr };
    TStrongObjectPtr<UPhysicsConstraintComponent> Constraint { nullptr };
    TSharedPtr<FJsonObject> SourceBefore;
};
void Count(FResult& R,const TCHAR* K,int32 N=1) { R.Data->SetNumberField(K,R.Data->GetNumberField(K)+N); }
bool Text(const FString& V,int32 Max) { if(V.Len()>Max) { return false; }for(int32 I=0;I<V.Len();++I) { if(V[I]==TCHAR('\0')) { return false; } }return true; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool Dependency(const FString& V) { FString S=V.ToLower();for(const TCHAR* T:{TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("cubelesscontrolrigtools")}) { if(S.Contains(T)) { return true; } }return false; }
void Init(FResult& R,const TCHAR* Operation,bool Read)
{
    R.Data->SetStringField(TEXT("operation"),Operation);R.Data->SetBoolField(TEXT("read_only"),Read);
    R.Data->SetBoolField(TEXT("source_mesh_physics_asset_assignment_attempted"),false);
    R.Data->SetBoolField(TEXT("runtime_trajectory_force_stability_or_cold_permanence_verified"),false);
    R.Data->SetBoolField(TEXT("implicit_Engine_physics_animation_preview_cache_callbacks_exhaustively_measured"),false);
    R.Data->SetStringField(TEXT("change_state_scope"),TEXT("Actual owned transient fixture and bounded selected source pointer/package/disk observations; live physics evolves independently, no scene/global zero-effect claim."));
    R.Data->SetStringField(TEXT("counter_scope"),TEXT("Adapter callsites only; engine setter internals, animation/physics construction and simulation updates are unmeasured. Void setter counts are dispatch calls, not inferred successful writes."));
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("asset_load_calls"),TEXT("fallback_load_object_calls"),TEXT("skeleton_getter_calls"),TEXT("physics_asset_getter_calls"),
        TEXT("source_capture_calls"),TEXT("actor_spawn_attempts"),TEXT("actor_spawn_returns"),TEXT("actor_destroy_attempts"),TEXT("actor_destroy_returns"),TEXT("modify_calls"),
        TEXT("actor_label_setter_calls"),TEXT("tag_add_calls"),TEXT("component_setter_calls"),TEXT("body_instance_getter_calls"),TEXT("constraint_allocation_attempts"),TEXT("constraint_allocation_returns"),
        TEXT("instance_component_add_calls"),TEXT("attachment_calls"),TEXT("component_register_calls"),TEXT("constraint_setter_calls"),TEXT("constraint_init_calls"),
        TEXT("simulate_physics_setter_calls"),TEXT("simulate_all_bodies_setter_calls"),TEXT("constrained_components_getter_calls"),TEXT("simulation_state_getter_calls"),
        TEXT("force_getter_calls"),TEXT("angle_getter_calls"),TEXT("broken_getter_calls"),TEXT("projection_getter_calls"),TEXT("actor_template_getter_calls"),
        TEXT("world_context_query_calls"),TEXT("actor_scan_calls"),TEXT("actor_candidates_visited"),TEXT("skeletal_mesh_asset_getter_calls"),TEXT("mesh_assignment_raw_pointer_observations"),TEXT("compile_calls"),TEXT("save_attempt_count")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetArrayField(TEXT("save_calls"),{});
}
TSharedPtr<FJsonObject> Error(FResult& R,const FString& Message)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Message);
    R.bSuccess=false;R.ErrorCode=TEXT("skeletal_constraint_error");R.Error=Message;
    // A later adapter observation error must not erase an already returned legacy operation receipt.
    if(!R.Data->HasField(TEXT("original_result"))) { R.Data->SetObjectField(TEXT("original_result"),J); }
    else { R.Data->SetObjectField(TEXT("adapter_error_result"),J); }
    return J;
}
void Project(FResult& R,const TSharedPtr<FJsonObject>& J)
{
    if(!J.IsValid()) { return; }R.Data->SetObjectField(TEXT("original_result"),J);
    for(const auto& P:J->Values) { if(!FString(P.Key.ToView()).Equals(TEXT("success"),ESearchCase::CaseSensitive)) { R.Data->SetField(P.Key,P.Value); } }
}
bool SameValue(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();
    case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!SameValue(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& P:X->Values) { const auto* V=Y->Values.Find(P.Key);if(!V||!SameValue(P.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}
bool Disk(const UObject* O,const TSharedRef<FJsonObject>& J)
{
    if(!Loaded(O)) { return false; }J->SetStringField(TEXT("object_path"),O->GetPathName());J->SetStringField(TEXT("class"),O->GetClass()->GetPathName());
    J->SetBoolField(TEXT("dirty"),O->GetOutermost()->IsDirty());FString Path;
    if(!FPackageName::TryConvertLongPackageNameToFilename(O->GetOutermost()->GetName(),Path,FPackageName::GetAssetPackageExtension())) { return false; }
    const int64 Bytes=IFileManager::Get().FileSize(*Path);J->SetStringField(TEXT("disk_path"),Path);J->SetBoolField(TEXT("disk_exists"),Bytes>=0);J->SetNumberField(TEXT("disk_bytes"),FMath::Max<int64>(0,Bytes));
    if(Bytes<0) { J->SetStringField(TEXT("disk_sha1"),TEXT(""));return true; }if(Bytes>134217728) { return false; }
    TArray<uint8> Data;if(!FFileHelper::LoadFileToArray(Data,*Path)||Data.Num()!=Bytes) { return false; }FSHAHash Hash;FSHA1::HashBuffer(Data.GetData(),Data.Num(),Hash.Hash);J->SetStringField(TEXT("disk_sha1"),Hash.ToString());return true;
}
bool CaptureSource(FResult& R,FContext& C,const TCHAR* Phase,TSharedPtr<FJsonObject>& Out)
{
    Count(R,TEXT("source_capture_calls"));Out=MakeShared<FJsonObject>();R.Data->SetObjectField(Phase,Out);Out->SetBoolField(TEXT("complete"),false);
    if(!Loaded(C.Mesh.Get())||!Loaded(C.Skeleton.Get())||!Loaded(C.Physics.Get())||C.Mesh->IsCompiling()) { return false; }
    for(const auto& Pair:{TPair<const TCHAR*,UObject*>(TEXT("mesh"),C.Mesh.Get()),TPair<const TCHAR*,UObject*>(TEXT("skeleton"),C.Skeleton.Get()),TPair<const TCHAR*,UObject*>(TEXT("physics_asset"),C.Physics.Get())})
    {auto Row=MakeShared<FJsonObject>();Out->SetObjectField(Pair.Key,Row);if(!Disk(Pair.Value,Row)) { return false; }}
    Count(R,TEXT("skeleton_getter_calls"));const auto* Skeleton=C.Mesh->GetSkeleton();Count(R,TEXT("physics_asset_getter_calls"));const auto* Physics=C.Mesh->GetPhysicsAsset();
    Out->SetStringField(TEXT("assigned_skeleton"),Skeleton?Skeleton->GetPathName():FString());Out->SetStringField(TEXT("assigned_physics_asset"),Physics?Physics->GetPathName():FString());
    if(Skeleton!=C.Skeleton.Get()||Physics!=C.Physics.Get()) { return false; }Out->SetBoolField(TEXT("complete"),true);return true;
}
void Cleanup(FResult& R,AActor* Actor)
{
    R.Stage=TEXT("cleanup_failed_fixture");Count(R,TEXT("actor_destroy_attempts"));const bool Destroyed=Actor->Destroy();Count(R,TEXT("actor_destroy_returns"));
    R.Data->SetBoolField(TEXT("cleanup_destroy_returned"),true);R.Data->SetBoolField(TEXT("cleanup_destroy_result"),Destroyed);
    R.Data->SetBoolField(TEXT("whole_world_physics_callback_state_restored"),false);
}
FVector VectorFromJsonArray(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FVector& Default, bool& bOutValid)
{
    bOutValid = true;
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return Default; // absent: keep default
    }

    const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
    if (!Params->TryGetArrayField(FieldName, Components) || !Components)
    {
        bOutValid = false;
        return Default;
    }
    if (Components->Num() != 3)
    {
        bOutValid = false;
        return Default;
    }

    double ParsedComponents[3] = {};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ParsedComponents); ++Index)
    {
        if (!(*Components)[Index].IsValid() ||
            !(*Components)[Index]->TryGetNumber(ParsedComponents[Index]) ||
            !FMath::IsFinite(ParsedComponents[Index]))
        {
            bOutValid = false;
            return Default;
        }
    }

    return FVector(ParsedComponents[0], ParsedComponents[1], ParsedComponents[2]);
}

UWorld* GetConstraintEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

FString WorldTypeToString(const UWorld* World)
{
    if (!World)
    {
        return TEXT("none");
    }
    switch (World->WorldType)
    {
    case EWorldType::Editor: return TEXT("editor");
    case EWorldType::PIE: return TEXT("pie");
    case EWorldType::Game: return TEXT("game");
    case EWorldType::GamePreview: return TEXT("game_preview");
    default: return TEXT("other");
    }
}

AActor* FindConstraintActor(FResult& R, UWorld* World, const FString& NameOrLabel)
{
    if (!World || NameOrLabel.IsEmpty())
    {
        return nullptr;
    }

    Count(R,TEXT("actor_scan_calls"));int32 Visited=0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        Count(R,TEXT("actor_candidates_visited"));
        if(++Visited>16384) { R.ErrorCode=TEXT("skeletal_constraint_actor_scope_incomplete");R.Error=TEXT("Bounded original first-match actor scan exceeded16384.");return nullptr; }
        AActor* Actor = *It;
        if (!Actor)
        {
            continue;
        }
        if (Actor->GetName().Equals(NameOrLabel, ESearchCase::CaseSensitive) ||
            Actor->GetActorLabel().Equals(NameOrLabel, ESearchCase::CaseSensitive) ||
            Actor->GetPathName().Equals(NameOrLabel, ESearchCase::CaseSensitive))
        {
            return Actor;
        }
    }
    return nullptr;
}

bool TryParseFixtureObjectName(
    const FString& RequestedName,
    const TCHAR* FieldName,
    FName& OutObjectName,
    FString& OutError)
{
    if (RequestedName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), FieldName);
        return false;
    }
    if (RequestedName.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(
            TEXT("'%s' is too long for an Unreal object name (maximum %d characters)"),
            FieldName,
            NAME_SIZE - 1);
        return false;
    }

    FText InvalidReason;
    if (!FName::IsValidXName(
            RequestedName,
            INVALID_OBJECTNAME_CHARACTERS,
            &InvalidReason))
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' object name '%s': %s"),
            FieldName,
            *RequestedName,
            *InvalidReason.ToString());
        return false;
    }

    OutObjectName = FName(*RequestedName);
    if (OutObjectName.IsNone())
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' object name '%s': name resolves to None"),
            FieldName,
            *RequestedName);
        return false;
    }
    return true;
}

bool TryParseSkeletalName(
    const FString& RequestedName,
    const TCHAR* FieldName,
    FName& OutSelectorName,
    FString& OutError)
{
    if (RequestedName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), FieldName);
        return false;
    }
    if (RequestedName.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(
            TEXT("'%s' is too long for an Unreal name (maximum %d characters)"),
            FieldName,
            NAME_SIZE - 1);
        return false;
    }

    OutSelectorName = FName(*RequestedName);
    if (OutSelectorName.IsNone())
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' name '%s': name resolves to None"),
            FieldName,
            *RequestedName);
        return false;
    }
    return true;
}

TArray<UWorld*> GetConstraintCandidateWorlds(const FString& WorldContext)
{
    TArray<UWorld*> Worlds;
    auto AddUniqueWorld = [&Worlds](UWorld* World)
    {
        if (World && !Worlds.Contains(World))
        {
            Worlds.Add(World);
        }
    };

    if (WorldContext.Equals(TEXT("pie"), ESearchCase::IgnoreCase) ||
        WorldContext.Equals(TEXT("game"), ESearchCase::IgnoreCase) ||
        WorldContext.Equals(TEXT("game_preview"), ESearchCase::IgnoreCase))
    {
        if (GEngine)
        {
            for (const FWorldContext& WorldContextEntry : GEngine->GetWorldContexts())
            {
                const bool bMatchesRequestedWorldType =
                    (WorldContext.Equals(TEXT("pie"), ESearchCase::IgnoreCase) &&
                        WorldContextEntry.WorldType == EWorldType::PIE) ||
                    (WorldContext.Equals(TEXT("game"), ESearchCase::IgnoreCase) &&
                        WorldContextEntry.WorldType == EWorldType::Game) ||
                    (WorldContext.Equals(TEXT("game_preview"), ESearchCase::IgnoreCase) &&
                        WorldContextEntry.WorldType == EWorldType::GamePreview);
                if (bMatchesRequestedWorldType)
                {
                    AddUniqueWorld(WorldContextEntry.World());
                }
            }
        }
        return Worlds;
    }

    if (WorldContext.Equals(TEXT("editor"), ESearchCase::IgnoreCase))
    {
        AddUniqueWorld(GetConstraintEditorWorld());
        return Worlds;
    }

    // Auto deliberately prefers an active play world so that the same actor
    // selector can independently verify the duplicated SIE/PIE fixture.
    if (GEngine)
    {
        for (const FWorldContext& WorldContextEntry : GEngine->GetWorldContexts())
        {
            const bool bIsPlayWorld =
                WorldContextEntry.WorldType == EWorldType::PIE ||
                WorldContextEntry.WorldType == EWorldType::Game ||
                WorldContextEntry.WorldType == EWorldType::GamePreview;
            if (bIsPlayWorld)
            {
                AddUniqueWorld(WorldContextEntry.World());
            }
        }
    }
    AddUniqueWorld(GetConstraintEditorWorld());
    return Worlds;
}

void AddVectorField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const FVector& Value)
{
    if (!Object.IsValid())
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Value.X));
    Values.Add(MakeShared<FJsonValueNumber>(Value.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Value.Z));
    Object->SetArrayField(FieldName, Values);
}

TSharedPtr<FJsonObject> ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return ActorObject;
}

void AddMutationResultContract(
    const TSharedPtr<FJsonObject>& Result,
    bool bDryRun,
    bool bMutationPerformed,
    bool bChanged,
    const TSharedPtr<FJsonObject>& Before,
    const TSharedPtr<FJsonObject>& After,
    const TSharedPtr<FJsonObject>& Delta,
    bool bVerificationPassed,
    const FString& VerificationMethod,
    const TArray<FString>& VerificationChecks,
    bool bStrictSuccess = true)
{
    if (!Result.IsValid())
    {
        return;
    }

    bool bLegacySuccess = true;
    if (Result->HasTypedField<EJson::Boolean>(TEXT("success")))
    {
        bLegacySuccess = Result->GetBoolField(TEXT("success"));
    }

    Result->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1"));
    Result->SetStringField(TEXT("mode"), bDryRun ? TEXT("dry_run") : TEXT("apply"));
    Result->SetBoolField(TEXT("dry_run"), bDryRun);
    Result->SetBoolField(TEXT("DRY_RUN"), bDryRun);
    Result->SetBoolField(TEXT("changed"), bChanged);
    Result->SetBoolField(TEXT("mutation_performed"), bMutationPerformed);

    TSharedPtr<FJsonObject> Mutation = MakeShared<FJsonObject>();
    Mutation->SetBoolField(TEXT("attempted"), !bDryRun);
    Mutation->SetBoolField(TEXT("performed"), bMutationPerformed);
    Mutation->SetStringField(
        TEXT("state"),
        bMutationPerformed ? TEXT("performed") : TEXT("not_performed"));
    Result->SetObjectField(TEXT("mutation"), Mutation);

    if (Before.IsValid())
    {
        Result->SetObjectField(TEXT("before"), Before);
    }
    if (After.IsValid())
    {
        Result->SetObjectField(TEXT("after"), After);
    }

    TSharedPtr<FJsonObject> NormalizedDelta = Delta.IsValid()
        ? Delta
        : MakeShared<FJsonObject>();
    static const TCHAR* CanonicalCountFields[] =
    {
        TEXT("affected_count"),
        TEXT("created_count"),
        TEXT("updated_count"),
        TEXT("removed_count"),
        TEXT("skipped_count"),
        TEXT("failed_count")
    };
    for (const TCHAR* FieldName : CanonicalCountFields)
    {
        if (!NormalizedDelta->HasField(FieldName))
        {
            NormalizedDelta->SetField(FieldName, MakeShared<FJsonValueNull>());
        }
    }
    Result->SetObjectField(TEXT("delta"), NormalizedDelta);

    TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(
        TEXT("status"),
        bVerificationPassed ? TEXT("passed") : TEXT("failed"));
    Verification->SetStringField(TEXT("method"), VerificationMethod);
    Verification->SetBoolField(TEXT("before_observed"), Before.IsValid());
    Verification->SetBoolField(TEXT("after_observed"), After.IsValid());
    Verification->SetBoolField(TEXT("strict"), bStrictSuccess);
    TArray<TSharedPtr<FJsonValue>> CheckValues;
    CheckValues.Reserve(VerificationChecks.Num());
    for (const FString& Check : VerificationChecks)
    {
        CheckValues.Add(MakeShared<FJsonValueString>(Check));
    }
    Verification->SetArrayField(TEXT("checks"), CheckValues);
    Result->SetObjectField(TEXT("verification"), Verification);

    const bool bVerifiedSuccess = bLegacySuccess && bVerificationPassed;
    Result->SetBoolField(TEXT("verified_success"), bVerifiedSuccess);
    FString SuccessSemantics = TEXT("legacy_execution");
    if (bDryRun)
    {
        SuccessSemantics = bVerifiedSuccess ? TEXT("verified_dry_run") : TEXT("failed_dry_run");
    }
    else if (bVerifiedSuccess)
    {
        SuccessSemantics = TEXT("verified_postcondition");
    }
    else if (bStrictSuccess && bLegacySuccess)
    {
        SuccessSemantics = TEXT("postcondition_failed");
    }
    else if (!bLegacySuccess)
    {
        SuccessSemantics = TEXT("failed_execution");
    }
    Result->SetStringField(TEXT("success_semantics"), SuccessSemantics);

    if (!Result->HasField(TEXT("success")))
    {
        Result->SetBoolField(TEXT("success"), bLegacySuccess);
    }
    if (bStrictSuccess && bLegacySuccess && !bVerificationPassed)
    {
        Result->SetBoolField(TEXT("legacy_success"), true);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error_code"), TEXT("mutation_postcondition_failed"));
        if (!Result->HasField(TEXT("error")))
        {
            Result->SetStringField(
                TEXT("error"),
                TEXT("Mutation postcondition failed independent readback"));
        }
    }
}

TSharedPtr<FJsonObject> LegacyCreate(const TSharedPtr<FJsonObject>& Params, FResult& R, FContext& Keep)
{
    if (!Params.IsValid())
    {
        return Error(R,TEXT("Missing parameters"));
    }

    FString SkeletalMeshPath;
    if (!Params->TryGetStringField(TEXT("skeletal_mesh_path"), SkeletalMeshPath) || SkeletalMeshPath.IsEmpty())
    {
        return Error(R,TEXT("Missing 'skeletal_mesh_path' parameter"));
    }
    if (Dependency(SkeletalMeshPath))
    {
        return Error(R,
            FString(TEXT("MCP-only dependency reference is not allowed for skeletal_mesh_path")));
    }

    R.Stage=TEXT("load_mesh");Count(R,TEXT("asset_load_calls"));
    USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(UEditorAssetLibrary::LoadAsset(SkeletalMeshPath));
    if (!SkeletalMesh)
    {
        FString SkeletalMeshObjectPath = FPackageName::ExportTextPathToObjectPath(SkeletalMeshPath);
        if (!SkeletalMeshObjectPath.Contains(TEXT(".")))
        {
            const FString AssetName = FPackageName::GetShortName(SkeletalMeshObjectPath);
            SkeletalMeshObjectPath = FString::Printf(TEXT("%s.%s"), *SkeletalMeshObjectPath, *AssetName);
        }
        Count(R,TEXT("fallback_load_object_calls"));
        SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshObjectPath);
    }
    if (!SkeletalMesh)
    {
        return Error(R,FString::Printf(
            TEXT("SkeletalMesh not found: %s"), *SkeletalMeshPath));
    }

    Keep.Mesh.Reset(SkeletalMesh);
    if(!Loaded(SkeletalMesh)||SkeletalMesh->GetClass()!=USkeletalMesh::StaticClass()||SkeletalMesh->IsCompiling()) { return Error(R,TEXT("Finite native idle SkeletalMesh required; async/custom mesh variants remain incomplete.")); }
    Count(R,TEXT("skeleton_getter_calls"));USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    Count(R,TEXT("physics_asset_getter_calls"));UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
    Keep.Skeleton.Reset(Skeleton);Keep.Physics.Reset(PhysicsAsset);
    if (!Skeleton)
    {
        return Error(R,FString::Printf(
            TEXT("SkeletalMesh has no Skeleton: %s"), *SkeletalMesh->GetPathName()));
    }
    if (!PhysicsAsset)
    {
        return Error(R,FString::Printf(
            TEXT("SkeletalMesh has no PhysicsAsset: %s"), *SkeletalMesh->GetPathName()));
    }

    const bool bDryRun = Params->HasTypedField<EJson::Boolean>(TEXT("dry_run"))
        ? Params->GetBoolField(TEXT("dry_run"))
        : true;
    FString WorldContext = TEXT("editor");
    Params->TryGetStringField(TEXT("world_context"), WorldContext);
    if (!WorldContext.Equals(TEXT("editor"), ESearchCase::IgnoreCase) &&
        !WorldContext.Equals(TEXT("pie"), ESearchCase::IgnoreCase) &&
        !WorldContext.Equals(TEXT("game"), ESearchCase::IgnoreCase) &&
        !WorldContext.Equals(TEXT("game_preview"), ESearchCase::IgnoreCase))
    {
        return Error(R,TEXT("world_context must be editor, pie, game, or game_preview"));
    }
    const FString ActorName = [&Params]()
    {
        FString Value;
        Params->TryGetStringField(TEXT("actor_name"), Value);
        return Value.IsEmpty() ? FString(TEXT("MCP_SkeletalBoneConstraint_Ghost")) : Value;
    }();
    FName ActorObjectName;
    FString ObjectNameError;
    if (!TryParseFixtureObjectName(ActorName, TEXT("actor_name"), ActorObjectName, ObjectNameError))
    {
        return Error(R,ObjectNameError);
    }
    const FString BoneName1 = [&Params]()
    {
        FString Value;
        Params->TryGetStringField(TEXT("bone_name1"), Value);
        return Value.IsEmpty() ? FString(TEXT("hand_l")) : Value;
    }();
    const FString BoneName2 = [&Params]()
    {
        FString Value;
        Params->TryGetStringField(TEXT("bone_name2"), Value);
        return Value.IsEmpty() ? FString(TEXT("hand_r")) : Value;
    }();
    if (Dependency(ActorName) ||
        Dependency(BoneName1) ||
        Dependency(BoneName2))
    {
        return Error(R,TEXT("MCP-only dependency references are not allowed in fixture selectors"));
    }
    if (BoneName1.Equals(BoneName2, ESearchCase::IgnoreCase))
    {
        return Error(R,TEXT("bone_name1 and bone_name2 must be different"));
    }

    if(!Loaded(Skeleton)||!Loaded(PhysicsAsset)||Skeleton->GetClass()!=USkeleton::StaticClass()||PhysicsAsset->GetClass()!=UPhysicsAsset::StaticClass()
        ||PhysicsAsset->SkeletalBodySetups.Num()>256||Skeleton->GetReferenceSkeleton().GetNum()>2048) { return Error(R,TEXT("Finite native Skeleton/PhysicsAsset body and reference scope required.")); }
    const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
    FName Bone1;
    FName Bone2;
    if (!TryParseSkeletalName(BoneName1, TEXT("bone_name1"), Bone1, ObjectNameError) ||
        !TryParseSkeletalName(BoneName2, TEXT("bone_name2"), Bone2, ObjectNameError))
    {
        return Error(R,ObjectNameError);
    }
    if (ReferenceSkeleton.FindBoneIndex(Bone1) == INDEX_NONE || ReferenceSkeleton.FindBoneIndex(Bone2) == INDEX_NONE)
    {
        return Error(R,FString::Printf(
            TEXT("Both bones must exist in the Skeleton: '%s', '%s'"), *BoneName1, *BoneName2));
    }
    if (PhysicsAsset->FindBodyIndex(Bone1) == INDEX_NONE || PhysicsAsset->FindBodyIndex(Bone2) == INDEX_NONE)
    {
        return Error(R,FString::Printf(
            TEXT("Both bones must have PhysicsAsset bodies: '%s', '%s'"), *BoneName1, *BoneName2));
    }

    R.Stage=TEXT("resolve_world");Count(R,TEXT("world_context_query_calls"));
    UWorld* TargetWorld = nullptr;
    if (WorldContext.Equals(TEXT("editor"), ESearchCase::IgnoreCase))
    {
        TargetWorld = GetConstraintEditorWorld();
        if (!TargetWorld)
        {
            return Error(R,TEXT("Editor world is unavailable"));
        }
        if (GEditor && GEditor->GetPIEWorldContext())
        {
            return Error(R,TEXT("End PIE/SIE before creating an editor-world skeletal constraint fixture"));
        }
    }
    else
    {
        const TArray<UWorld*> PlayWorlds = GetConstraintCandidateWorlds(WorldContext);
        if (PlayWorlds.Num() == 0)
        {
            return Error(R,FString::Printf(
                TEXT("No active play world matches world_context '%s'"), *WorldContext));
        }
        TargetWorld = PlayWorlds[0];
    }
    Keep.World.Reset(TargetWorld);
    ULevel* TargetLevel = TargetWorld->GetCurrentLevel();
    if (!TargetLevel)
    {
        return Error(R,TEXT("Target world has no current level"));
    }
    AActor* ExistingActor=FindConstraintActor(R,TargetWorld,ActorName);
    if(!R.ErrorCode.IsEmpty()) { return Error(R,R.Error); }
    if (ExistingActor || StaticFindObjectFast(nullptr, TargetLevel, ActorObjectName))
    {
        return Error(R,FString::Printf(
            TEXT("Fixture actor name is unavailable: %s"), *ActorName));
    }

    bool bLocationValid = true;
    const FVector Location = VectorFromJsonArray(
        Params,
        TEXT("location"),
        FVector(0.0f, 0.0f, 250.0f),
        bLocationValid);
    if (!bLocationValid)
    {
        return Error(R,TEXT("'location' must be an array of three numbers"));
    }

    const float Swing1Limit = FMath::Clamp(
        Params->HasTypedField<EJson::Number>(TEXT("swing1_limit_degrees"))
            ? static_cast<float>(Params->GetNumberField(TEXT("swing1_limit_degrees")))
            : 45.0f,
        0.0f,
        180.0f);
    const float Swing2Limit = FMath::Clamp(
        Params->HasTypedField<EJson::Number>(TEXT("swing2_limit_degrees"))
            ? static_cast<float>(Params->GetNumberField(TEXT("swing2_limit_degrees")))
            : 45.0f,
        0.0f,
        180.0f);
    const float TwistLimit = FMath::Clamp(
        Params->HasTypedField<EJson::Number>(TEXT("twist_limit_degrees"))
            ? static_cast<float>(Params->GetNumberField(TEXT("twist_limit_degrees")))
            : 30.0f,
        0.0f,
        180.0f);
    const bool bDisableCollision = Params->HasTypedField<EJson::Boolean>(TEXT("disable_collision"))
        ? Params->GetBoolField(TEXT("disable_collision"))
        : true;
    const bool bSimulateAllBodies = Params->HasTypedField<EJson::Boolean>(TEXT("simulate_all_bodies"))
        ? Params->GetBoolField(TEXT("simulate_all_bodies"))
        : true;
    const FString ConstraintName = [&Params]()
    {
        FString Value;
        Params->TryGetStringField(TEXT("constraint_component_name"), Value);
        return Value.IsEmpty() ? FString(TEXT("MCP_SkeletalBoneConstraint")) : Value;
    }();
    FName ConstraintObjectName;
    if (!TryParseFixtureObjectName(
            ConstraintName,
            TEXT("constraint_component_name"),
            ConstraintObjectName,
            ObjectNameError))
    {
        return Error(R,ObjectNameError);
    }

    Count(R,TEXT("actor_template_getter_calls"));
    UObject* ActorTemplate = ASkeletalMeshActor::StaticClass()->GetDefaultObject();
    if (!ActorTemplate)
    {
        return Error(R,TEXT("ASkeletalMeshActor default object is unavailable"));
    }
    if (StaticFindObjectFast(nullptr, ActorTemplate, ConstraintObjectName))
    {
        return Error(R,FString::Printf(
            TEXT("constraint_component_name conflicts with an ASkeletalMeshActor default subobject: %s"),
            *ConstraintName));
    }

    const FString CanonicalActorName = ActorObjectName.ToString();
    const FString CanonicalConstraintName = ConstraintObjectName.ToString();
    const FString CanonicalBoneName1 = Bone1.ToString();
    const FString CanonicalBoneName2 = Bone2.ToString();

    TSharedPtr<FJsonObject> Before = MakeShared<FJsonObject>();
    Before->SetBoolField(TEXT("actor_exists"), false);
    Before->SetStringField(TEXT("actor_name"), CanonicalActorName);
    Before->SetStringField(TEXT("world_context"), WorldContext);
    Before->SetStringField(TEXT("world_type"), WorldTypeToString(TargetWorld));
    Before->SetStringField(TEXT("skeletal_mesh_path"), SkeletalMesh->GetPathName());
    Before->SetStringField(TEXT("physics_asset_path"), PhysicsAsset->GetPathName());

    R.Stage=TEXT("before_observation");
    if(!CaptureSource(R,Keep,TEXT("before_source"),Keep.SourceBefore)) { return Error(R,TEXT("Bounded selected source observation incomplete before fixture dispatch.")); }
    R.Data->SetObjectField(TEXT("before_observation"),Before);
    R.Data->SetStringField(TEXT("target_world"),TargetWorld->GetPathName());R.Data->SetStringField(TEXT("target_level"),TargetLevel->GetPathName());
    R.Data->SetBoolField(TEXT("world_package_dirty_before"),TargetWorld->GetOutermost()->IsDirty());
    if (bDryRun)
    {
        TSharedPtr<FJsonObject> Planned = MakeShared<FJsonObject>();
        Planned->SetBoolField(TEXT("can_create"), true);
        Planned->SetBoolField(TEXT("actor_created"), false);
        Planned->SetStringField(TEXT("actor_name"), CanonicalActorName);
        Planned->SetStringField(TEXT("world_context"), WorldContext);
        Planned->SetStringField(TEXT("world_type"), WorldTypeToString(TargetWorld));
        Planned->SetStringField(TEXT("skeletal_mesh_path"), SkeletalMesh->GetPathName());
        Planned->SetStringField(TEXT("physics_asset_path"), PhysicsAsset->GetPathName());
        Planned->SetStringField(TEXT("bone_name1"), CanonicalBoneName1);
        Planned->SetStringField(TEXT("bone_name2"), CanonicalBoneName2);
        Planned->SetStringField(TEXT("constraint_component_name"), CanonicalConstraintName);
        AddVectorField(Planned, TEXT("location"), Location);
        Planned->SetBoolField(TEXT("disable_collision"), bDisableCollision);
        Planned->SetBoolField(TEXT("simulate_all_bodies"), bSimulateAllBodies);
        Planned->SetNumberField(TEXT("swing1_limit_degrees"), Swing1Limit);
        Planned->SetNumberField(TEXT("swing2_limit_degrees"), Swing2Limit);
        Planned->SetNumberField(TEXT("twist_limit_degrees"), TwistLimit);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("fixture_kind"), TEXT("skeletal_bone_physics_constraint_component"));
        Result->SetBoolField(TEXT("can_create"), true);
        Result->SetStringField(TEXT("actor_name"), CanonicalActorName);
        Result->SetStringField(TEXT("world_context"), WorldContext);
        Result->SetStringField(TEXT("world_type"), WorldTypeToString(TargetWorld));
        Result->SetStringField(TEXT("skeletal_mesh_path"), SkeletalMesh->GetPathName());
        Result->SetStringField(TEXT("physics_asset_path"), PhysicsAsset->GetPathName());
        Result->SetObjectField(TEXT("planned"), Planned);
        TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
        Delta->SetNumberField(TEXT("affected_count"), 1);
        Delta->SetNumberField(TEXT("created_count"), 0);
        Delta->SetNumberField(TEXT("updated_count"), 0);
        Delta->SetNumberField(TEXT("removed_count"), 0);
        Delta->SetNumberField(TEXT("skipped_count"), 1);
        Delta->SetNumberField(TEXT("failed_count"), 0);
        AddMutationResultContract(
            Result,
            true,
            false,
            false,
            Before,
            Planned,
            Delta,
            true,
            TEXT("skeletal_mesh_skeleton_physics_asset_preflight"),
            {TEXT("mesh_resolved"), TEXT("physics_asset_resolved"), TEXT("bones_exist"), TEXT("bodies_exist"), TEXT("object_names_valid"), TEXT("actor_name_available"), TEXT("actor_object_name_available"), TEXT("constraint_component_name_available")});
        return Result;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = ActorObjectName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
    SpawnParams.OverrideLevel = TargetLevel;
    SpawnParams.ObjectFlags = RF_Transient | RF_Transactional;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    R.Stage=TEXT("spawn_fixture");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("actor_spawn_attempts"));
    ASkeletalMeshActor* Actor = TargetWorld->SpawnActor<ASkeletalMeshActor>(
        ASkeletalMeshActor::StaticClass(),
        FTransform(FRotator::ZeroRotator, Location),
        SpawnParams);
    Count(R,TEXT("actor_spawn_returns"));Keep.Actor.Reset(Actor);R.Data->SetBoolField(TEXT("actor_spawn_returned"),true);R.Data->SetBoolField(TEXT("actor_spawn_success"),Actor!=nullptr);
    if(Actor) { R.bChanged=true;R.Data->SetStringField(TEXT("returned_actor_path"),Actor->GetPathName()); }
    if (!Actor)
    {
        return Error(R,FString::Printf(
            TEXT("Failed to spawn skeletal fixture actor '%s'; its object name may be unavailable"),
            *ActorName));
    }

    if (StaticFindObjectFast(nullptr, Actor, ConstraintObjectName))
    {
        Cleanup(R,Actor);
        return Error(R,FString::Printf(
            TEXT("constraint_component_name is already used by the spawned actor: %s"),
            *ConstraintName));
    }

    Count(R,TEXT("modify_calls"));Actor->Modify();
    Count(R,TEXT("actor_label_setter_calls"));Actor->SetActorLabel(ActorName);
    Count(R,TEXT("tag_add_calls"));Actor->Tags.AddUnique(FName(TEXT("MCP_SkeletalBoneConstraintFixture")));
    USkeletalMeshComponent* SkeletalMeshComponent = Actor->GetSkeletalMeshComponent();
    if (!SkeletalMeshComponent)
    {
        Cleanup(R,Actor);
        return Error(R,TEXT("Spawned actor has no SkeletalMeshComponent"));
    }
    Count(R,TEXT("modify_calls"));SkeletalMeshComponent->Modify();
    Count(R,TEXT("component_setter_calls"));SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh, true);
    Count(R,TEXT("component_setter_calls"));SkeletalMeshComponent->SetPhysicsAsset(PhysicsAsset, true);
    Count(R,TEXT("component_setter_calls"));SkeletalMeshComponent->SetMobility(EComponentMobility::Movable);
    Count(R,TEXT("component_setter_calls"));SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    R.Stage=TEXT("configure_mesh");
    Count(R,TEXT("body_instance_getter_calls"));auto* Body1=SkeletalMeshComponent->GetBodyInstance(Bone1);
    FBodyInstance* Body2=nullptr;if(Body1) { Count(R,TEXT("body_instance_getter_calls"));Body2=SkeletalMeshComponent->GetBodyInstance(Bone2); }
    R.Data->SetBoolField(TEXT("bone1_body_exists"),Body1!=nullptr);R.Data->SetBoolField(TEXT("bone2_body_exists"),Body2!=nullptr);
    if (!Body1 || !Body2)
    {
        Cleanup(R,Actor);
        return Error(R,TEXT("SkeletalMeshComponent could not create both requested PhysicsAsset bodies"));
    }

    R.Stage=TEXT("allocate_constraint");Count(R,TEXT("constraint_allocation_attempts"));
    UPhysicsConstraintComponent* Constraint = NewObject<UPhysicsConstraintComponent>(
        Actor,
        ConstraintObjectName,
        RF_Transient | RF_Transactional);
    Count(R,TEXT("constraint_allocation_returns"));Keep.Constraint.Reset(Constraint);R.Data->SetBoolField(TEXT("constraint_allocation_returned"),true);R.Data->SetBoolField(TEXT("constraint_allocation_success"),Constraint!=nullptr);
    if(Constraint) { R.Data->SetStringField(TEXT("returned_constraint_path"),Constraint->GetPathName()); }
    if (!Constraint)
    {
        Cleanup(R,Actor);
        return Error(R,TEXT("Failed to create PhysicsConstraintComponent"));
    }
    Count(R,TEXT("modify_calls"));Constraint->Modify();
    Count(R,TEXT("instance_component_add_calls"));Actor->AddInstanceComponent(Constraint);
    Count(R,TEXT("attachment_calls"));Constraint->SetupAttachment(SkeletalMeshComponent);
    Count(R,TEXT("component_register_calls"));Constraint->RegisterComponent();
    Count(R,TEXT("constraint_setter_calls"));Constraint->SetConstrainedComponents(SkeletalMeshComponent, Bone1, SkeletalMeshComponent, Bone2);
    Count(R,TEXT("constraint_setter_calls"));Constraint->SetDisableCollision(bDisableCollision);
    Count(R,TEXT("constraint_setter_calls"));Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Count(R,TEXT("constraint_setter_calls"));Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Count(R,TEXT("constraint_setter_calls"));Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
    Count(R,TEXT("constraint_setter_calls"));Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, Swing1Limit);
    Count(R,TEXT("constraint_setter_calls"));Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, Swing2Limit);
    Count(R,TEXT("constraint_setter_calls"));Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, TwistLimit);
    Count(R,TEXT("constraint_init_calls"));Constraint->InitComponentConstraint();

    Count(R,TEXT("simulate_physics_setter_calls"));SkeletalMeshComponent->SetSimulatePhysics(false);
    Count(R,TEXT("simulate_all_bodies_setter_calls"));SkeletalMeshComponent->SetAllBodiesSimulatePhysics(bSimulateAllBodies);
    if (bSimulateAllBodies)
    {
        Count(R,TEXT("simulate_physics_setter_calls"));SkeletalMeshComponent->SetSimulatePhysics(true);
    }

    UPrimitiveComponent* ConnectedComponent1 = nullptr;
    UPrimitiveComponent* ConnectedComponent2 = nullptr;
    FName ConnectedBone1;
    FName ConnectedBone2;
    R.Stage=TEXT("verify_fixture");Count(R,TEXT("constrained_components_getter_calls"));
    Constraint->GetConstrainedComponents(
        ConnectedComponent1,
        ConnectedBone1,
        ConnectedComponent2,
        ConnectedBone2);
    const bool bTransientFixture =
        Actor->HasAnyFlags(RF_Transient) &&
        Constraint->HasAnyFlags(RF_Transient);
    const bool bPostconditionVerified =
        ConnectedComponent1 == SkeletalMeshComponent &&
        ConnectedComponent2 == SkeletalMeshComponent &&
        ConnectedBone1 == Bone1 &&
        ConnectedBone2 == Bone2 &&
        bTransientFixture;
    if (!bPostconditionVerified)
    {
        Cleanup(R,Actor);
        return Error(R,TEXT("Constraint postcondition failed: connected components, bone names, or transient flags did not read back exactly"));
    }

    TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("after_observation"),After);
    After->SetBoolField(TEXT("actor_exists"), true);
    After->SetStringField(TEXT("actor_name"), Actor->GetName());
    After->SetStringField(TEXT("actor_label"), Actor->GetActorLabel());
    After->SetStringField(TEXT("actor_path"), Actor->GetPathName());
    After->SetStringField(TEXT("world_context"), WorldContext);
    After->SetStringField(TEXT("world_type"), WorldTypeToString(Actor->GetWorld()));
    After->SetStringField(TEXT("constraint_component_name"), Constraint->GetName());
    After->SetStringField(TEXT("bone_name1"), ConnectedBone1.ToString());
    After->SetStringField(TEXT("bone_name2"), ConnectedBone2.ToString());
    After->SetBoolField(TEXT("actor_transient"), Actor->HasAnyFlags(RF_Transient));
    After->SetBoolField(TEXT("constraint_transient"), Constraint->HasAnyFlags(RF_Transient));
    After->SetBoolField(TEXT("simulate_all_bodies_requested"), bSimulateAllBodies);
    Count(R,TEXT("simulation_state_getter_calls"));After->SetBoolField(TEXT("bone1_simulating"), SkeletalMeshComponent->IsSimulatingPhysics(Bone1));
    Count(R,TEXT("simulation_state_getter_calls"));After->SetBoolField(TEXT("bone2_simulating"), SkeletalMeshComponent->IsSimulatingPhysics(Bone2));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("fixture_kind"), TEXT("skeletal_bone_physics_constraint_component"));
    Result->SetBoolField(TEXT("actor_created"), true);
    Result->SetStringField(TEXT("actor_name"), Actor->GetName());
    Result->SetStringField(TEXT("actor_label"), Actor->GetActorLabel());
    Result->SetStringField(TEXT("actor_path"), Actor->GetPathName());
    Result->SetStringField(TEXT("world_context"), WorldContext);
    Result->SetStringField(TEXT("world_type"), WorldTypeToString(Actor->GetWorld()));
    Result->SetStringField(TEXT("skeletal_mesh_path"), SkeletalMesh->GetPathName());
    Result->SetStringField(TEXT("physics_asset_path"), PhysicsAsset->GetPathName());
    Result->SetStringField(TEXT("constraint_component_name"), Constraint->GetName());
    Result->SetStringField(TEXT("bone_name1"), ConnectedBone1.ToString());
    Result->SetStringField(TEXT("bone_name2"), ConnectedBone2.ToString());
    Result->SetBoolField(TEXT("actor_transient"), Actor->HasAnyFlags(RF_Transient));
    Result->SetBoolField(TEXT("constraint_transient"), Constraint->HasAnyFlags(RF_Transient));
    AddVectorField(Result, TEXT("location"), Location);
    Result->SetBoolField(TEXT("disable_collision"), bDisableCollision);
    Result->SetBoolField(TEXT("simulate_all_bodies"), bSimulateAllBodies);
    Result->SetNumberField(TEXT("swing1_limit_degrees"), Swing1Limit);
    Result->SetNumberField(TEXT("swing2_limit_degrees"), Swing2Limit);
    Result->SetNumberField(TEXT("twist_limit_degrees"), TwistLimit);
    Result->SetObjectField(TEXT("actor"), ActorToJsonObject(Actor, true));

    TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
    Delta->SetNumberField(TEXT("affected_count"), 1);
    Delta->SetNumberField(TEXT("created_count"), 1);
    Delta->SetNumberField(TEXT("updated_count"), 0);
    Delta->SetNumberField(TEXT("removed_count"), 0);
    Delta->SetNumberField(TEXT("skipped_count"), 0);
    Delta->SetNumberField(TEXT("failed_count"), 0);
    AddMutationResultContract(
        Result,
        false,
        true,
        true,
        Before,
        After,
        Delta,
        true,
        TEXT("constraint_component_get_constrained_components_readback"),
        {TEXT("actor_created"), TEXT("skeletal_mesh_assigned"), TEXT("physics_asset_assigned"), TEXT("bone1_body_exists"), TEXT("bone2_body_exists"), TEXT("constraint_components_match"), TEXT("constraint_bones_match"), TEXT("actor_transient"), TEXT("constraint_transient")});
    return Result;
}

// JU: exact native AActor component composition, Editor point-read only.
// The original SkeletalMeshActor/live-world reader below is unchanged.
TSharedPtr<FJsonObject> InspectNativeActorComponents(AActor* Actor,UWorld* World,const FString& WorldContext,
    const FString& RequestedConstraintName,FResult& R,const TSharedPtr<FJsonObject>& Selected)
{
    if(!Loaded(Actor)||!Loaded(World)||Actor->GetClass()!=AActor::StaticClass()||World->WorldType!=EWorldType::Editor
        ||!GEditor||GEditor->GetEditorWorldContext().World()!=World||IsPlaySessionActive())
    {return Error(R,TEXT("Exact native Actor composition inspector requires the current idle Editor world; other Actor/live variants remain incomplete."));}
    TStrongObjectPtr<ULevel> Level(Actor->GetLevel());TStrongObjectPtr<UPackage> ActorPackage(Actor->GetPackage()),WorldPackage(World->GetPackage());
    if(!Loaded(Level.Get())||!Loaded(ActorPackage.Get())||!Loaded(WorldPackage.Get())){return Error(R,TEXT("Selected native Actor level/package role unavailable."));}
    const TWeakObjectPtr<UEditorEngine> Editor(GEditor);const FString ActorPath=Actor->GetPathName(),WorldPath=World->GetPathName(),LevelPath=Level->GetPathName(),ActorLabel=Actor->GetActorLabel();
    const EObjectFlags ActorFlags=Actor->GetFlags(),WorldFlags=World->GetFlags(),LevelFlags=Level->GetFlags();const bool ActorDirty=ActorPackage->IsDirty(),WorldDirty=WorldPackage->IsDirty();
    const TArray<FName> Tags=Actor->Tags;auto Result=MakeShared<FJsonObject>();FString Failure;
    struct FComponentReadRole{TStrongObjectPtr<UActorComponent> Hold;UClass* Class=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;};
    TArray<FComponentReadRole> Roles;TArray<UPhysicsConstraintComponent*> Constraints;TStrongObjectPtr<UPhysicsConstraintComponent> Constraint;
    TStrongObjectPtr<UPrimitiveComponent> First,Second;TStrongObjectPtr<USkeletalMesh> Mesh;
    bool ConstraintsCaptured=false,LinkageCaptured=false,MeshCaptured=false;FObjectProperty* LegacyMeshField=nullptr;FObjectProperty* SkinnedAssetField=nullptr;USkeletalMeshComponent* Skeletal=nullptr;UObject* LegacyMeshValue=nullptr;UObject* SkinnedAssetValue=nullptr;
    TWeakObjectPtr<UPrimitiveComponent> Override1,Override2;AActor* ConstraintActor1=nullptr;AActor* ConstraintActor2=nullptr;FName ComponentName1,ComponentName2,StoredBone1,StoredBone2;
    FString MeshPath;UObject* MeshOuter=nullptr;UClass* MeshClass=nullptr;EObjectFlags MeshFlags=RF_NoFlags;bool MeshDirty=false;
    const auto Stop=[&](const TCHAR* Phase)
    {R.Stage=Phase;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("automatic_retry_safe"),false);R.Data->SetStringField(TEXT("inspector_first_failed_boundary"),Phase);R.Data->SetStringField(TEXT("inspector_role_failure"),Failure);R.Data->SetObjectField(TEXT("partial_inspector_result"),Result);return Error(R,TEXT("Native Actor inspector role changed or became incomplete: ")+Failure+TEXT("; actual read prefix retained, no next getter or retry."));};
    const auto CaptureComponent=[&](UActorComponent* Component)
    {
        if(!Loaded(Component)||Component->IsBeingDestroyed()||Component->GetOuter()!=Actor||Component->GetOwner()!=Actor||Component->GetWorld()!=World||Component->GetPackage()!=ActorPackage.Get()||!Actor->GetComponents().Contains(Component))
        {Failure=TEXT("component current owner/world/membership unavailable");return false;}
        for(const auto& Role:Roles){if(Role.Hold.Get()==Component){return true;}}
        FComponentReadRole Role;Role.Hold.Reset(Component);Role.Class=Component->GetClass();Role.Path=Component->GetPathName();Role.Flags=Component->GetFlags();Roles.Add(MoveTemp(Role));return true;
    };
    const auto Current=[&]()
    {
        if(!IsEditorGameThread()||IsEngineExitRequested()||IsGarbageCollecting()||!Editor.IsValid()||GEditor!=Editor.Get()||IsPlaySessionActive()
            ||!Loaded(World)||World->bIsTearingDown||World->WorldType!=EWorldType::Editor||GEditor->GetEditorWorldContext().World()!=World||World->GetPathName()!=WorldPath||World->GetFlags()!=WorldFlags||World->GetPackage()!=WorldPackage.Get())
        {Failure=TEXT("captured Editor/world context");return false;}
        if(!Loaded(Actor)||Actor->IsActorBeingDestroyed()||Actor->GetClass()!=AActor::StaticClass()||Actor->GetOuter()!=Level.Get()||Actor->GetLevel()!=Level.Get()||Actor->GetWorld()!=World
            ||Actor->GetPathName()!=ActorPath||Actor->GetActorLabel()!=ActorLabel||Actor->GetFlags()!=ActorFlags||Actor->GetPackage()!=ActorPackage.Get()||Actor->Tags!=Tags)
        {Failure=TEXT("captured exact Actor identity/label/tags");return false;}
        if(!Loaded(Level.Get())||Level->GetWorld()!=World||Level->GetPathName()!=LevelPath||Level->GetFlags()!=LevelFlags||!World->GetLevels().Contains(Level.Get())||Level->Actors.Num()>16384
            ||!Loaded(ActorPackage.Get())||!Loaded(WorldPackage.Get())||ActorPackage->IsDirty()!=ActorDirty||WorldPackage->IsDirty()!=WorldDirty)
        {Failure=TEXT("captured level/package membership or dirty observation");return false;}
        int32 Membership=0;for(const auto& Member:Level->Actors){if(Member.Get()==Actor){++Membership;}}if(Membership!=1){Failure=TEXT("Actor level membership");return false;}
        for(const auto& Role:Roles)
        {
            UActorComponent* Component=Role.Hold.Get();
            if(!Loaded(Component)||Component->IsBeingDestroyed()||Component->GetClass()!=Role.Class||Component->GetOuter()!=Actor||Component->GetOwner()!=Actor||Component->GetWorld()!=World||Component->GetPackage()!=ActorPackage.Get()
                ||Component->GetPathName()!=Role.Path||Component->GetFlags()!=Role.Flags||!Actor->GetComponents().Contains(Component))
            {Failure=TEXT("captured component identity/owner/world/membership");return false;}
        }
        if(ConstraintsCaptured)
        {TArray<UPhysicsConstraintComponent*> Now;Actor->GetComponents<UPhysicsConstraintComponent>(Now);if(Now.Num()>64||Now!=Constraints){Failure=TEXT("ordered constraint selection changed");return false;}}
        if(LinkageCaptured&&(Constraint->OverrideComponent1!=Override1||Constraint->OverrideComponent2!=Override2||Constraint->ConstraintActor1.Get()!=ConstraintActor1||Constraint->ConstraintActor2.Get()!=ConstraintActor2
            ||Constraint->ComponentName1.ComponentName!=ComponentName1||Constraint->ComponentName2.ComponentName!=ComponentName2||Constraint->ConstraintInstance.ConstraintBone1!=StoredBone1||Constraint->ConstraintInstance.ConstraintBone2!=StoredBone2))
        {Failure=TEXT("captured constrained component/bone selectors changed");return false;}
        if(MeshCaptured)
        {
            // Raw property addresses deliberately avoid reflected Getter dispatch.
            // The public pointer-value API copies the TObjectPtr without resolving it.
            Count(R,TEXT("mesh_assignment_raw_pointer_observations"));
            const auto LegacyValue=LegacyMeshField->GetObjectPtrPropertyValue(LegacyMeshField->ContainerPtrToValuePtr<void>(Skeletal));
            const auto SkinnedValue=SkinnedAssetField->GetObjectPtrPropertyValue(SkinnedAssetField->ContainerPtrToValuePtr<void>(Skeletal));
            if(!LegacyValue.IsResolved()||!SkinnedValue.IsResolved()){Failure=TEXT("current mesh backing pointer became unresolved; no load or resolve fallback");return false;}
            if(LegacyValue.Get()!=LegacyMeshValue||SkinnedValue.Get()!=SkinnedAssetValue||(LegacyValue.Get()?LegacyValue.Get():SkinnedValue.Get())!=Mesh.Get())
            {Failure=TEXT("selected component actual mesh backing assignment changed");return false;}
            if(Mesh.IsValid()&&(!Loaded(Mesh.Get())||Mesh->GetClass()!=MeshClass||Mesh->GetOuter()!=MeshOuter||Mesh->GetPathName()!=MeshPath||Mesh->GetFlags()!=MeshFlags||Mesh->GetOutermost()->IsDirty()!=MeshDirty))
            {Failure=TEXT("captured loaded mesh identity/dirty changed");return false;}
        }
        return true;
    };
    if(!Current()){return Stop(TEXT("native_actor_admission"));}
    Actor->GetComponents<UPhysicsConstraintComponent>(Constraints);if(Constraints.Num()>64){return Error(R,TEXT("Bounded constraint component array exceeds64; broader actor variants remain incomplete."));}
    for(UPhysicsConstraintComponent* Candidate:Constraints)
    {if(!CaptureComponent(Candidate)){return Stop(TEXT("native_actor_constraint_capture"));}if(!Constraint.IsValid()&&(RequestedConstraintName.IsEmpty()||Candidate->GetName().Equals(RequestedConstraintName))){Constraint.Reset(Candidate);}}
    ConstraintsCaptured=true;if(!Current()){return Stop(TEXT("after_constraint_selection"));}
    if(!Constraint.IsValid()){return Error(R,FString::Printf(TEXT("No PhysicsConstraintComponent matched '%s' on actor '%s'"),*RequestedConstraintName,*Actor->GetName()));}
    Selected->SetStringField(TEXT("constraint_path"),Constraint->GetPathName());Selected->SetStringField(TEXT("constraint_class"),Constraint->GetClass()->GetPathName());Selected->SetStringField(TEXT("constraint_owner"),ActorPath);
    if(Constraint->GetClass()!=UPhysicsConstraintComponent::StaticClass()){return Error(R,TEXT("Native Actor composition inspector admits only an exact native PhysicsConstraintComponent; custom component variants remain incomplete."));}
    Override1=Constraint->OverrideComponent1;Override2=Constraint->OverrideComponent2;ConstraintActor1=Constraint->ConstraintActor1.Get();ConstraintActor2=Constraint->ConstraintActor2.Get();
    ComponentName1=Constraint->ComponentName1.ComponentName;ComponentName2=Constraint->ComponentName2.ComponentName;StoredBone1=Constraint->ConstraintInstance.ConstraintBone1;StoredBone2=Constraint->ConstraintInstance.ConstraintBone2;LinkageCaptured=true;
    if((ConstraintActor1&&ConstraintActor1!=Actor)||(ConstraintActor2&&ConstraintActor2!=Actor)){return Error(R,TEXT("Native Actor composition inspector does not admit an external constraint Actor reference."));}
    if(!Current()){return Stop(TEXT("before_constrained_components"));}
    UPrimitiveComponent* Component1=nullptr;UPrimitiveComponent* Component2=nullptr;FName Bone1,Bone2;
    R.Stage=TEXT("inspect_live_pair");Count(R,TEXT("constrained_components_getter_calls"));Constraint->GetConstrainedComponents(Component1,Bone1,Component2,Bone2);
    Selected->SetStringField(TEXT("bone1"),Bone1.ToString());Selected->SetStringField(TEXT("bone2"),Bone2.ToString());
    if(!Current()){return Stop(TEXT("after_constrained_components"));}
    for(UPrimitiveComponent* Component:{Component1,Component2})
    {
        if(!Component){continue;}if(!CaptureComponent(Component)){return Stop(TEXT("returned_pair_capture"));}
        if(Component->GetClass()!=USkeletalMeshComponent::StaticClass()){return Error(R,TEXT("Native Actor composition inspector admits only exact native SkeletalMeshComponent pair members; other primitive variants remain incomplete."));}
    }
    First.Reset(Component1);Second.Reset(Component2);Skeletal=Component1?Cast<USkeletalMeshComponent>(Component1):Cast<USkeletalMeshComponent>(Component2);
    Selected->SetStringField(TEXT("component1"),Component1?Component1->GetPathName():FString());Selected->SetStringField(TEXT("component2"),Component2?Component2->GetPathName():FString());
    if(!Current()){return Stop(TEXT("before_force"));}FVector LinearForce=FVector::ZeroVector,AngularForce=FVector::ZeroVector;Count(R,TEXT("force_getter_calls"));Constraint->GetConstraintForce(LinearForce,AngularForce);
    if(LinearForce.ContainsNaN()||AngularForce.ContainsNaN()){R.Data->SetStringField(TEXT("nonfinite_linear_force_text"),LinearForce.ToString());R.Data->SetStringField(TEXT("nonfinite_angular_force_text"),AngularForce.ToString());Failure=TEXT("actual force is nonfinite");return Stop(TEXT("after_force"));}
    AddVectorField(Selected,TEXT("linear_force"),LinearForce);AddVectorField(Selected,TEXT("angular_force"),AngularForce);AddVectorField(Result,TEXT("linear_force"),LinearForce);AddVectorField(Result,TEXT("angular_force"),AngularForce);
    if(!Current()){return Stop(TEXT("after_force"));}
    const bool ComponentsResolved=Component1&&Component2,BonesResolved=!Bone1.IsNone()&&!Bone2.IsNone(),FixtureTag=Tags.Contains(FName(TEXT("MCP_SkeletalBoneConstraintFixture"))),SkeletalPair=Skeletal&&Component1==Skeletal&&Component2==Skeletal;
    const bool Postcondition=ComponentsResolved&&BonesResolved&&FixtureTag&&SkeletalPair;
    Result->SetBoolField(TEXT("success"),Postcondition);Result->SetBoolField(TEXT("postcondition_verified"),Postcondition);Result->SetStringField(TEXT("fixture_kind"),TEXT("skeletal_bone_physics_constraint_component"));
    Result->SetStringField(TEXT("world_context"),WorldContext);Result->SetStringField(TEXT("world_type"),WorldTypeToString(World));Result->SetStringField(TEXT("actor_name"),Actor->GetName());Result->SetStringField(TEXT("actor_label"),ActorLabel);Result->SetStringField(TEXT("actor_path"),ActorPath);
    Result->SetStringField(TEXT("constraint_component_name"),Constraint->GetName());Result->SetStringField(TEXT("component1_name"),Component1?Component1->GetName():FString());Result->SetStringField(TEXT("component2_name"),Component2?Component2->GetName():FString());Result->SetStringField(TEXT("bone_name1"),Bone1.ToString());Result->SetStringField(TEXT("bone_name2"),Bone2.ToString());
    Result->SetBoolField(TEXT("fixture_tag_verified"),FixtureTag);Result->SetBoolField(TEXT("skeletal_pair_verified"),SkeletalPair);Result->SetBoolField(TEXT("actor_transient"),Actor->HasAnyFlags(RF_Transient));Result->SetBoolField(TEXT("constraint_transient"),Constraint->HasAnyFlags(RF_Transient));
    if(!Current()){return Stop(TEXT("before_broken"));}Count(R,TEXT("broken_getter_calls"));Result->SetBoolField(TEXT("is_broken"),Constraint->IsBroken());if(!Current()){return Stop(TEXT("after_broken"));}
    Count(R,TEXT("projection_getter_calls"));Result->SetBoolField(TEXT("projection_enabled"),Constraint->ConstraintInstance.IsProjectionEnabled());if(!Current()){return Stop(TEXT("after_projection"));}
    for(const FName Angle:{FName(TEXT("current_swing1_degrees")),FName(TEXT("current_swing2_degrees")),FName(TEXT("current_twist_degrees"))})
    {
        if(!Current()){return Stop(TEXT("before_angle"));}Count(R,TEXT("angle_getter_calls"));const float Value=Angle==TEXT("current_swing1_degrees")?Constraint->GetCurrentSwing1():Angle==TEXT("current_swing2_degrees")?Constraint->GetCurrentSwing2():Constraint->GetCurrentTwist();
        if(!FMath::IsFinite(Value)){R.Data->SetStringField(TEXT("nonfinite_angle_text"),FString::SanitizeFloat(Value));Failure=TEXT("actual ")+Angle.ToString()+TEXT(" is nonfinite");return Stop(TEXT("after_angle"));}Result->SetNumberField(Angle.ToString(),Value);if(!Current()){return Stop(TEXT("after_angle"));}
    }
    if(Skeletal)
    {
        LegacyMeshField=FindFProperty<FObjectProperty>(USkinnedMeshComponent::StaticClass(),TEXT("SkeletalMesh"));
        SkinnedAssetField=FindFProperty<FObjectProperty>(USkinnedMeshComponent::StaticClass(),TEXT("SkinnedAsset"));
        if(!LegacyMeshField||!SkinnedAssetField||LegacyMeshField->GetClass()!=FObjectProperty::StaticClass()||SkinnedAssetField->GetClass()!=FObjectProperty::StaticClass()
            ||LegacyMeshField->ArrayDim!=1||SkinnedAssetField->ArrayDim!=1||LegacyMeshField->PropertyClass!=USkeletalMesh::StaticClass()||SkinnedAssetField->PropertyClass!=USkinnedAsset::StaticClass()
            ||LegacyMeshField->GetOwnerStruct()!=USkinnedMeshComponent::StaticClass()||SkinnedAssetField->GetOwnerStruct()!=USkinnedMeshComponent::StaticClass())
        {Failure=TEXT("exact native skinned component backing pointer fields unavailable");return Stop(TEXT("before_mesh"));}
        if(!Current()){return Stop(TEXT("before_mesh"));}Count(R,TEXT("skeletal_mesh_asset_getter_calls"));USkeletalMesh* ActualMesh=Skeletal->GetSkeletalMeshAsset();if(!Current()){return Stop(TEXT("after_mesh"));}
        if(ActualMesh)
        {if(!Loaded(ActualMesh)||ActualMesh->GetClass()!=USkeletalMesh::StaticClass()){Failure=TEXT("returned current mesh unavailable or not exact native");return Stop(TEXT("after_mesh_capture"));}Mesh.Reset(ActualMesh);MeshPath=Mesh->GetPathName();MeshOuter=Mesh->GetOuter();MeshClass=Mesh->GetClass();MeshFlags=Mesh->GetFlags();MeshDirty=Mesh->GetOutermost()->IsDirty();}
        Count(R,TEXT("mesh_assignment_raw_pointer_observations"));
        const auto LegacyValue=LegacyMeshField->GetObjectPtrPropertyValue(LegacyMeshField->ContainerPtrToValuePtr<void>(Skeletal));
        const auto SkinnedValue=SkinnedAssetField->GetObjectPtrPropertyValue(SkinnedAssetField->ContainerPtrToValuePtr<void>(Skeletal));
        if(!LegacyValue.IsResolved()||!SkinnedValue.IsResolved()){Failure=TEXT("mesh backing pointer is unresolved; no load or resolve fallback");return Stop(TEXT("after_mesh_capture"));}
        LegacyMeshValue=LegacyValue.Get();SkinnedAssetValue=SkinnedValue.Get();
        MeshCaptured=true;Result->SetStringField(TEXT("skeletal_mesh_path"),MeshPath);if(!Current()){return Stop(TEXT("after_mesh_capture"));}
        if(!Current()){return Stop(TEXT("before_bone1_simulating"));}Count(R,TEXT("simulation_state_getter_calls"));Result->SetBoolField(TEXT("bone1_simulating"),Skeletal->IsSimulatingPhysics(Bone1));if(!Current()){return Stop(TEXT("after_bone1_simulating"));}
        Count(R,TEXT("simulation_state_getter_calls"));Result->SetBoolField(TEXT("bone2_simulating"),Skeletal->IsSimulatingPhysics(Bone2));if(!Current()){return Stop(TEXT("after_bone2_simulating"));}
    }
    if(!Current()){return Stop(TEXT("final_native_actor_observation"));}R.Data->SetStringField(TEXT("inspector_actor_route"),TEXT("exact_native_Actor_Editor_components"));R.Data->SetBoolField(TEXT("current_actor_component_roles_preserved"),true);
    Result->SetStringField(TEXT("runtime_note"),TEXT("Read-only current idle Editor native Actor component point observation; no physics progression, stable force/trajectory, actual bone topology, save or cold/runtime guarantee. Native SkeletalMeshActor keeps its existing live-world route."));return Result;
}

TSharedPtr<FJsonObject> LegacyInspect(const TSharedPtr<FJsonObject>& Params, FResult& R, FContext& Keep)
{
    if (!Params.IsValid())
    {
        return Error(R,TEXT("Missing parameters"));
    }

    FString ActorSelector;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorSelector) || ActorSelector.IsEmpty())
    {
        return Error(R,TEXT("Missing 'actor_name' parameter"));
    }
    FString WorldContext = TEXT("auto");
    Params->TryGetStringField(TEXT("world_context"), WorldContext);
    if (!WorldContext.Equals(TEXT("auto"), ESearchCase::IgnoreCase) &&
        !WorldContext.Equals(TEXT("editor"), ESearchCase::IgnoreCase) &&
        !WorldContext.Equals(TEXT("pie"), ESearchCase::IgnoreCase) &&
        !WorldContext.Equals(TEXT("game"), ESearchCase::IgnoreCase) &&
        !WorldContext.Equals(TEXT("game_preview"), ESearchCase::IgnoreCase))
    {
        return Error(R,TEXT("world_context must be auto, editor, pie, game, or game_preview"));
    }
    FString RequestedConstraintName;
    Params->TryGetStringField(TEXT("constraint_component_name"), RequestedConstraintName);

    UWorld* FoundWorld = nullptr;
    AActor* FoundActor = nullptr;
    Count(R,TEXT("world_context_query_calls"));
    for (UWorld* World : GetConstraintCandidateWorlds(WorldContext))
    {
        AActor* Candidate = FindConstraintActor(R,World,ActorSelector);
        if(!R.ErrorCode.IsEmpty()) { return Error(R,R.Error); }
        if (Candidate)
        {
            FoundWorld = World;
            FoundActor = Candidate;
            break;
        }
    }
    if (!FoundActor)
    {
        return Error(R,FString::Printf(
            TEXT("No skeletal constraint fixture actor matched '%s' in world_context '%s'"),
            *ActorSelector,
            *WorldContext));
    }

    Keep.Actor.Reset(FoundActor);Keep.World.Reset(FoundWorld);
    auto Selected=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("selected_fixture"),Selected);
    Selected->SetStringField(TEXT("actor_path"),FoundActor->GetPathName());Selected->SetStringField(TEXT("actor_class"),FoundActor->GetClass()->GetPathName());
    Selected->SetStringField(TEXT("world_path"),FoundWorld->GetPathName());Selected->SetBoolField(TEXT("world_package_dirty"),FoundWorld->GetOutermost()->IsDirty());
    if(FoundActor->GetWorld()!=FoundWorld) { return Error(R,TEXT("Selected current actor no longer belongs to the matched world.")); }
    if(FoundActor->GetClass()==AActor::StaticClass()){return InspectNativeActorComponents(FoundActor,FoundWorld,WorldContext,RequestedConstraintName,R,Selected);}
    if(FoundActor->GetClass()!=ASkeletalMeshActor::StaticClass()) { return Error(R,TEXT("First inspector slice requires exact native SkeletalMeshActor; custom/generated fixture actors remain valid incomplete variants.")); }
    TArray<UPhysicsConstraintComponent*> ConstraintComponents;
    FoundActor->GetComponents<UPhysicsConstraintComponent>(ConstraintComponents);
    if(ConstraintComponents.Num()>64) { return Error(R,TEXT("Bounded constraint component array exceeds64; broader actor variants remain incomplete.")); }
    UPhysicsConstraintComponent* Constraint = nullptr;
    for (UPhysicsConstraintComponent* Candidate : ConstraintComponents)
    {
        if (Candidate && (RequestedConstraintName.IsEmpty() || Candidate->GetName().Equals(RequestedConstraintName)))
        {
            Constraint = Candidate;
            break;
        }
    }
    Keep.Constraint.Reset(Constraint);
    if (!Constraint)
    {
        return Error(R,FString::Printf(
            TEXT("No PhysicsConstraintComponent matched '%s' on actor '%s'"),
            *RequestedConstraintName,
            *FoundActor->GetName()));
    }

    Selected->SetStringField(TEXT("constraint_path"),Constraint->GetPathName());Selected->SetStringField(TEXT("constraint_class"),Constraint->GetClass()->GetPathName());
    Selected->SetStringField(TEXT("constraint_owner"),Constraint->GetOuter()->GetPathName());
    UPrimitiveComponent* Component1 = nullptr;
    UPrimitiveComponent* Component2 = nullptr;
    FName Bone1;
    FName Bone2;
    R.Stage=TEXT("inspect_live_pair");Count(R,TEXT("constrained_components_getter_calls"));
    Constraint->GetConstrainedComponents(Component1, Bone1, Component2, Bone2);
    Selected->SetStringField(TEXT("component1"),Component1?Component1->GetPathName():FString());Selected->SetStringField(TEXT("component2"),Component2?Component2->GetPathName():FString());
    Selected->SetStringField(TEXT("bone1"),Bone1.ToString());Selected->SetStringField(TEXT("bone2"),Bone2.ToString());
    FVector LinearForce = FVector::ZeroVector;
    FVector AngularForce = FVector::ZeroVector;
    Count(R,TEXT("force_getter_calls"));Constraint->GetConstraintForce(LinearForce, AngularForce);
    if(LinearForce.ContainsNaN()||AngularForce.ContainsNaN())
    { R.bChangeStateKnown=false;R.Data->SetStringField(TEXT("nonfinite_linear_force_text"),LinearForce.ToString());R.Data->SetStringField(TEXT("nonfinite_angular_force_text"),AngularForce.ToString());return Error(R,TEXT("Actual live constraint force is nonfinite; no synthetic force or retry.")); }
    // Preserve the actual finite force prefix before any later angle observation can fail.
    AddVectorField(Selected,TEXT("linear_force"),LinearForce);AddVectorField(Selected,TEXT("angular_force"),AngularForce);
    USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component1);
    if (!SkeletalMeshComponent)
    {
        SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component2);
    }

    const bool bComponentsResolved = Component1 != nullptr && Component2 != nullptr;
    const bool bBonesResolved = Bone1 != NAME_None && Bone2 != NAME_None;
    const bool bFixtureTagVerified = FoundActor->Tags.Contains(FName(TEXT("MCP_SkeletalBoneConstraintFixture")));
    const bool bSkeletalPairVerified =
        SkeletalMeshComponent != nullptr &&
        Component1 == SkeletalMeshComponent &&
        Component2 == SkeletalMeshComponent;
    const bool bPostconditionVerified =
        bComponentsResolved &&
        bBonesResolved &&
        bFixtureTagVerified &&
        bSkeletalPairVerified;
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bPostconditionVerified);
    Result->SetBoolField(TEXT("postcondition_verified"), bPostconditionVerified);
    Result->SetStringField(TEXT("fixture_kind"), TEXT("skeletal_bone_physics_constraint_component"));
    Result->SetStringField(TEXT("world_context"), WorldContext);
    Result->SetStringField(TEXT("world_type"), WorldTypeToString(FoundWorld));
    Result->SetStringField(TEXT("actor_name"), FoundActor->GetName());
    Result->SetStringField(TEXT("actor_label"), FoundActor->GetActorLabel());
    Result->SetStringField(TEXT("actor_path"), FoundActor->GetPathName());
    Result->SetStringField(TEXT("constraint_component_name"), Constraint->GetName());
    Result->SetStringField(TEXT("component1_name"), Component1 ? Component1->GetName() : FString());
    Result->SetStringField(TEXT("component2_name"), Component2 ? Component2->GetName() : FString());
    Result->SetStringField(TEXT("bone_name1"), Bone1.ToString());
    Result->SetStringField(TEXT("bone_name2"), Bone2.ToString());
    Result->SetBoolField(TEXT("fixture_tag_verified"), bFixtureTagVerified);
    Result->SetBoolField(TEXT("skeletal_pair_verified"), bSkeletalPairVerified);
    Result->SetBoolField(TEXT("actor_transient"), FoundActor->HasAnyFlags(RF_Transient));
    Result->SetBoolField(TEXT("constraint_transient"), Constraint->HasAnyFlags(RF_Transient));
    Count(R,TEXT("broken_getter_calls"));Result->SetBoolField(TEXT("is_broken"), Constraint->IsBroken());
    // UPhysicsConstraintComponent::IsProjectionEnabled is not an exported
    // symbol in the UE 5.6 module. Read the public constraint instance's
    // inline profile flag instead so this editor module links cleanly.
    Count(R,TEXT("projection_getter_calls"));Result->SetBoolField(TEXT("projection_enabled"), Constraint->ConstraintInstance.IsProjectionEnabled());
    Count(R,TEXT("angle_getter_calls"));const float CurrentSwing1=Constraint->GetCurrentSwing1();
    if(!FMath::IsFinite(CurrentSwing1)) { R.bChangeStateKnown=false;R.Data->SetObjectField(TEXT("partial_inspector_result"),Result);R.Data->SetStringField(TEXT("nonfinite_angle_text"),FString::SanitizeFloat(CurrentSwing1));return Error(R,TEXT("Actual live constraint angle is nonfinite; point-read prefix retained without retry.")); }
    Result->SetNumberField(TEXT("current_swing1_degrees"),CurrentSwing1);
    Count(R,TEXT("angle_getter_calls"));const float CurrentSwing2=Constraint->GetCurrentSwing2();
    if(!FMath::IsFinite(CurrentSwing2)) { R.bChangeStateKnown=false;R.Data->SetObjectField(TEXT("partial_inspector_result"),Result);R.Data->SetStringField(TEXT("nonfinite_angle_text"),FString::SanitizeFloat(CurrentSwing2));return Error(R,TEXT("Actual live constraint angle is nonfinite; point-read prefix retained without retry.")); }
    Result->SetNumberField(TEXT("current_swing2_degrees"),CurrentSwing2);
    Count(R,TEXT("angle_getter_calls"));const float CurrentTwist=Constraint->GetCurrentTwist();
    if(!FMath::IsFinite(CurrentTwist)) { R.bChangeStateKnown=false;R.Data->SetObjectField(TEXT("partial_inspector_result"),Result);R.Data->SetStringField(TEXT("nonfinite_angle_text"),FString::SanitizeFloat(CurrentTwist));return Error(R,TEXT("Actual live constraint angle is nonfinite; point-read prefix retained without retry.")); }
    Result->SetNumberField(TEXT("current_twist_degrees"),CurrentTwist);
    AddVectorField(Result, TEXT("linear_force"), LinearForce);
    AddVectorField(Result, TEXT("angular_force"), AngularForce);
    if (SkeletalMeshComponent)
    {
        Result->SetStringField(TEXT("skeletal_mesh_path"), SkeletalMeshComponent->GetSkeletalMeshAsset()
            ? SkeletalMeshComponent->GetSkeletalMeshAsset()->GetPathName()
            : FString());
        Count(R,TEXT("simulation_state_getter_calls"));Result->SetBoolField(TEXT("bone1_simulating"), SkeletalMeshComponent->IsSimulatingPhysics(Bone1));
        Count(R,TEXT("simulation_state_getter_calls"));Result->SetBoolField(TEXT("bone2_simulating"), SkeletalMeshComponent->IsSimulatingPhysics(Bone2));
    }
    Result->SetStringField(TEXT("runtime_note"), TEXT("Read-only native readback; use LevelEditorSubsystem.editor_play_simulate() to start SIE and call this command with world_context=pie for runtime evidence."));
    return Result;
}

}
FResult CreateFixture(const FString& MeshPath,const FString& ActorName,const FString& Bone1,const FString& Bone2,
    const FString& World,const FString& LocationJson,const FString& ConstraintName,double Swing1,double Swing2,double Twist,
    bool DisableCollision,bool SimulateAll,bool Dry)
{
    FResult R;Init(R,TEXT("create_skeletal_bone_constraint_fixture"),false);
    if(!IsEditorGameThread()||IsEngineExitRequested()) { Error(R,TEXT("Editor game thread required."));return R; }
    for(const auto& V:{MeshPath,ActorName,Bone1,Bone2,World,ConstraintName}) { if(!Text(V,4096)) { Error(R,TEXT("Bounded literal strings without interior NUL required."));return R; } }
    if(!FMath::IsFinite(Swing1)||!FMath::IsFinite(Swing2)||!FMath::IsFinite(Twist)||FMath::Abs(Swing1)>MAX_flt||FMath::Abs(Swing2)>MAX_flt||FMath::Abs(Twist)>MAX_flt)
    { Error(R,TEXT("Finite float32 angular carrier required; original extreme conversion variants remain incomplete."));return R; }
    if(!Text(LocationJson,4096)) { Error(R,TEXT("Bounded nullable location JSON required."));return R; }
    TSharedPtr<FJsonObject> Envelope;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("{\"location\":")+LocationJson+TEXT("}")),Envelope)||!Envelope.IsValid()||Envelope->Values.Num()!=1)
    { Error(R,TEXT("Bounded null or numeric3 location required."));return R; }
    const auto Location=Envelope->TryGetField(TEXT("location"));if(!Location.IsValid()||(Location->Type!=EJson::Null&&Location->Type!=EJson::Array)) { Error(R,TEXT("Null or numeric3 location required."));return R; }
    auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("skeletal_mesh_path"),MeshPath);Params->SetStringField(TEXT("actor_name"),ActorName);
    Params->SetStringField(TEXT("bone_name1"),Bone1);Params->SetStringField(TEXT("bone_name2"),Bone2);Params->SetStringField(TEXT("world_context"),World);
    if(Location->Type!=EJson::Null) { Params->SetField(TEXT("location"),Location); }
    Params->SetStringField(TEXT("constraint_component_name"),ConstraintName);Params->SetNumberField(TEXT("swing1_limit_degrees"),Swing1);
    Params->SetNumberField(TEXT("swing2_limit_degrees"),Swing2);Params->SetNumberField(TEXT("twist_limit_degrees"),Twist);
    Params->SetBoolField(TEXT("disable_collision"),DisableCollision);Params->SetBoolField(TEXT("simulate_all_bodies"),SimulateAll);Params->SetBoolField(TEXT("dry_run"),Dry);
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP","COSkeletalFixture","Create skeletal constraint fixture"));
    FContext Keep;auto Result=LegacyCreate(Params,R,Keep);Project(R,Result);
    if(!Result.IsValid()||!Result->GetBoolField(TEXT("success"))) { return R; }
    if(!Keep.SourceBefore.IsValid()) { Error(R,TEXT("Actual selected source prefix missing."));return R; }
    TSharedPtr<FJsonObject> SourceAfter;R.Stage=TEXT("after_source_observation");R.bChangeStateKnown=false;
    if(!CaptureSource(R,Keep,TEXT("after_source"),SourceAfter)) { Error(R,TEXT("Returned fixture prefix retained; selected source observation incomplete."));return R; }
    const bool Stable=SameValue(MakeShared<FJsonValueObject>(Keep.SourceBefore),MakeShared<FJsonValueObject>(SourceAfter));R.Data->SetBoolField(TEXT("selected_source_preserved"),Stable);
    if(!Stable) { Error(R,TEXT("Original selected fixture result retained; tracked source disk/package/pointers changed, no fallback save or replay."));return R; }
    if(Keep.World.IsValid()) { R.Data->SetBoolField(TEXT("world_package_dirty_after"),Keep.World->GetOutermost()->IsDirty()); }
    if(!Dry)
    {
        if(!Loaded(Keep.Actor.Get())||!Loaded(Keep.Constraint.Get())||!Cast<ASkeletalMeshActor>(Keep.Actor.Get()))
        { Error(R,TEXT("Returned fixture objects are no longer current; retained raw prefix is not a lifetime restoration proof."));return R; }
        auto* Component=Cast<ASkeletalMeshActor>(Keep.Actor.Get())->GetSkeletalMeshComponent();
        UPhysicsAsset* AssignedPhysics=nullptr;
        if(Component) { Count(R,TEXT("physics_asset_getter_calls"));AssignedPhysics=Component->GetPhysicsAsset(); }
        const bool Assigned=Component&&Component->GetSkeletalMeshAsset()==Keep.Mesh.Get()&&AssignedPhysics==Keep.Physics.Get();
        const bool Attached=Keep.Constraint.IsValid()&&Keep.Constraint->IsRegistered()&&Keep.Constraint->GetAttachParent()==Component;
        R.Data->SetBoolField(TEXT("current_mesh_physics_assignment_verified"),Assigned);R.Data->SetBoolField(TEXT("current_attachment_registration_verified"),Attached);
        if(!Assigned||!Attached) { Error(R,TEXT("Original pair result retained; current assignment/attachment observation failed."));return R; }
    }
    R.bSuccess=R.bPostconditionPassed=R.bChangeStateKnown=true;R.bChanged=!Dry;R.Stage=TEXT("complete");return R;
}
FResult InspectFixture(const FString& ActorName,const FString& World,const FString& ConstraintName)
{
    FResult R;Init(R,TEXT("inspect_skeletal_bone_constraint_fixture"),true);
    if(!IsEditorGameThread()||IsEngineExitRequested()) { Error(R,TEXT("Editor game thread required."));return R; }
    for(const auto& V:{ActorName,World,ConstraintName}) { if(!Text(V,4096)||Dependency(V)) { Error(R,TEXT("Bounded literal selectors without MCP-only dependency references required."));return R; } }
    auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("actor_name"),ActorName);Params->SetStringField(TEXT("world_context"),World);Params->SetStringField(TEXT("constraint_component_name"),ConstraintName);
    FContext Keep;auto Result=LegacyInspect(Params,R,Keep);Project(R,Result);
    if(!Result.IsValid()||!Result->GetBoolField(TEXT("success"))) { if(R.ErrorCode.IsEmpty()) { R.ErrorCode=TEXT("skeletal_constraint_postcondition_failed");R.Error=TEXT("Current native tag/components/bones did not satisfy original inspector predicate."); }return R; }
    R.Data->SetBoolField(TEXT("live_values_are_point_observations_not_stability_proof"),true);
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
