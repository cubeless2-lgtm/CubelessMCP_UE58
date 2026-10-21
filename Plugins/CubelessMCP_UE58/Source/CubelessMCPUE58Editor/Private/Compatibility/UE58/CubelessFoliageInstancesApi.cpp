// UE56 LandscapeCommands foliage instance contracts, using installed public UE5.8 APIs.
// Editor-only compatibility. Toolset registration, Python normalization and journals stay outside this file.
#include "Compatibility/UE58/CubelessFoliageInstancesApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FoliageType_Actor.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliage.h"
#include "InstancedFoliageActor.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Misc/Crc.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

namespace Cubeless::UE58::FoliageInstances
{
namespace
{
using Variable::FResult;
constexpr int32 MaxJsonChars=1024*1024,MaxAdd=5000,MaxRemove=50000,MaxTypes=16384,MaxObservedInstances=262144;
bool Running=false;

bool Live(const UObject* Object)
{
    return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);
}
bool SafeText(const FString& Text,int32 Limit)
{
    if(Text.Len()>Limit) { return false; }
    for(TCHAR C:Text) { if(C==0) { return false; } }
    return true;
}
// JSON keys are case-sensitive; default FString set equality/hash is not.
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& Key) { return FCrc::StrCrc32(*Key); }
};
bool Parse(const FString& Text,TSharedPtr<FJsonValue>& Value)
{
    if(Text.IsEmpty()||!SafeText(Text,MaxJsonChars)) { return false; }
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));
    TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>524288) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        {
            const FString Key(Reader->GetIdentifier());
            if(Keys.Last().Contains(Key)) { return false; }Keys.Last().Add(Key);
        }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>16) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()) { return false; }
    Value=Values[0];return true;
}
bool Dependency(const FString& Value)
{
    FString Candidate=FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();Candidate.TrimQuotesInline();
    for(const TCHAR* Token:{TEXT("/Script/UnrealMCP"),TEXT("/UnrealMCP"),TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("/Script/CubelessControlRigTools"),TEXT("CubelessControlRigTools"),TEXT("CubelessControlRigToolsEditor"),TEXT("CubelessMCP_UE58"),TEXT("CubelessMCPUE58")})
    { if(Candidate.Contains(Token,ESearchCase::IgnoreCase)) { return true; } }
    return false;
}
// A bounded exact public editor-data projection. It does not inspect private foliage implementations.
struct FPlacement
{
    FVector Location;FRotator Rotation,PreAlignRotation;FVector3f Scale;float ZOffset;uint32 Flags;
    FFoliageInstanceBaseId BaseId;FGuid ProceduralGuid;UActorComponent* BaseComponent;
    explicit FPlacement(const FFoliageInstance& I):Location(I.Location),Rotation(I.Rotation),PreAlignRotation(I.PreAlignRotation),Scale(I.DrawScale3D),ZOffset(I.ZOffset),Flags(I.Flags),BaseId(I.BaseId),ProceduralGuid(I.ProceduralGuid),BaseComponent(I.BaseComponent) {}
    bool operator==(const FPlacement& B) const
    { return Location==B.Location&&Rotation==B.Rotation&&PreAlignRotation==B.PreAlignRotation&&Scale==B.Scale&&ZOffset==B.ZOffset&&Flags==B.Flags&&BaseId==B.BaseId&&ProceduralGuid==B.ProceduralGuid&&BaseComponent==B.BaseComponent; }
    friend uint32 GetTypeHash(const FPlacement& P)
    {
        uint32 H=GetTypeHash(P.Location.X==0.0?0.0:P.Location.X);
        for(double N:{P.Location.Y,P.Location.Z,P.Rotation.Pitch,P.Rotation.Yaw,P.Rotation.Roll,P.PreAlignRotation.Pitch,P.PreAlignRotation.Yaw,P.PreAlignRotation.Roll}) { H=HashCombine(H,GetTypeHash(N==0?0:N)); }
        for(float N:{P.Scale.X,P.Scale.Y,P.Scale.Z,P.ZOffset}) { H=HashCombine(H,GetTypeHash(N==0.0f?0.0f:N)); }
        H=HashCombine(H,GetTypeHash(P.Flags));H=HashCombine(H,GetTypeHash(P.BaseId));H=HashCombine(H,GetTypeHash(P.ProceduralGuid));
        return HashCombine(H,PointerHash(P.BaseComponent));
    }
};
bool Finite(const FFoliageInstance& I)
{
    for(double N:{I.Location.X,I.Location.Y,I.Location.Z,I.Rotation.Pitch,I.Rotation.Yaw,I.Rotation.Roll,I.PreAlignRotation.Pitch,I.PreAlignRotation.Yaw,I.PreAlignRotation.Roll,static_cast<double>(I.DrawScale3D.X),static_cast<double>(I.DrawScale3D.Y),static_cast<double>(I.DrawScale3D.Z),static_cast<double>(I.ZOffset)})
    { if(!FMath::IsFinite(N)) { return false; } }return true;
}
struct FWork
{
    FResult Result;FString Command;bool WriterOperation;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>();
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ULevel> Level;TWeakObjectPtr<AInstancedFoliageActor> Actor;
    TStrongObjectPtr<UFoliageType> Requested{nullptr},Settings{nullptr};TUniquePtr<FScopedTransaction> Transaction;
    TSharedPtr<FJsonObject> Before;TArray<FPlacement> Original,ExpectedOrder;TMap<FPlacement,int32> ExpectedRemaining;
    int32 OriginalCount=0,AddedReturns=0,VerifiedAdded=0,RemovalRequested=0;bool SnapshotReady=false,ExpectAdd=false,ExpectRemove=false,HasTarget=false,RequireActor=false;
    explicit FWork(const TCHAR* Name,bool Writer):Command(Name),WriterOperation(Writer)
    {
        Result.Compiled=false;Legacy->SetBoolField(TEXT("success"),false);
        Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);Result.Data->SetObjectField(TEXT("postconditions"),Checks);
        Result.Data->SetBoolField(TEXT("atomic_rollback_available"),false);Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
        Result.Data->SetBoolField(TEXT("disk_reload_verified"),false);Result.Data->SetBoolField(TEXT("render_completion_verified"),false);Result.Data->SetBoolField(TEXT("game_thread_wait_performed"),false);
        Result.Data->SetBoolField(TEXT("implicit_Engine_writes_measured"),false);
        Result.Data->SetStringField(TEXT("scope"),TEXT("Current initialized Editor world/current level IFA under LevelPartition only; no WorldPartition cell/global traversal."));
        Result.Data->SetStringField(TEXT("total_change_scope"),TEXT("Live target IFA/type public editor-instance projection and package dirty observations only. Asset load, actor construction, transaction delegates and render/other package side effects are not a complete global inventory."));
        Result.Data->SetStringField(TEXT("retry_risk"),TEXT("Adding can duplicate instances; removal uses shifting array indices (RemoveAtSwap). Inspect partial/unknown receipts before any manual rerun; no automatic retry or atomic rollback."));
        Result.Data->SetStringField(TEXT("synchronous_SDK_limit"),TEXT("Public AddInstance/RemoveInstances and actor construction are synchronous SDK calls. Refresh requests async tree work; this adapter never waits for render/tree completion."));
        auto Limits=MakeShared<FJsonObject>();Limits->SetNumberField(TEXT("JSON_UTF16_characters"),MaxJsonChars);Limits->SetNumberField(TEXT("JSON_depth"),16);Limits->SetNumberField(TEXT("add_per_call"),MaxAdd);Limits->SetNumberField(TEXT("remove_per_call"),MaxRemove);Limits->SetNumberField(TEXT("listed_types"),MaxTypes);Limits->SetNumberField(TEXT("writer_snapshot_instances"),MaxObservedInstances);Result.Data->SetObjectField(TEXT("limits"),Limits);
        for(const TCHAR* Key:{TEXT("load_asset_calls"),TEXT("load_asset_returns"),TEXT("get_IFA_read_calls"),TEXT("get_IFA_create_calls"),TEXT("get_IFA_create_returns"),TEXT("begin_transaction_calls"),TEXT("begin_transaction_returns"),TEXT("end_transaction_calls"),TEXT("end_transaction_returns"),TEXT("IFA_modify_calls"),TEXT("IFA_modify_returns"),TEXT("add_foliage_type_calls"),TEXT("add_foliage_type_returns"),TEXT("add_instance_calls"),TEXT("add_instance_returns"),TEXT("remove_instances_calls"),TEXT("remove_instances_returns"),TEXT("refresh_calls"),TEXT("refresh_returns"),TEXT("IFA_mark_dirty_calls"),TEXT("IFA_mark_dirty_returns"),TEXT("world_mark_dirty_calls"),TEXT("world_mark_dirty_returns"),TEXT("compile_blueprint_calls"),TEXT("asset_save_calls"),TEXT("map_save_calls"),TEXT("start_pie_calls"),TEXT("stop_pie_calls")}) { Calls->SetNumberField(Key,0); }
    }
    void Count(const TCHAR* Key) { double N=0;Calls->TryGetNumberField(Key,N);Calls->SetNumberField(Key,N+1); }
    bool Fail(const TCHAR* Code,const FString& Error)
    {
        if(Result.Error.IsEmpty()) { Result.ErrorCode=TEXT("foliage_instances_")+FString(Code);Result.Error=Error; }
        Result.bSuccess=false;Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error);return false;
    }
    bool Admit(bool NeedActor=false)
    {
        if(!IsEditorGameThread()||!GEditor||(WriterOperation&&IsPlaySessionActive())||!Live(World.Get())||!World->IsInitialized()||World->WorldType!=EWorldType::Editor||GEditor->GetEditorWorldContext().World()!=World.Get()||!Live(Level.Get())||World->GetCurrentLevel()!=Level.Get()||Level->GetWorld()!=World.Get())
        { return Fail(TEXT("editor_changed"),TEXT("Captured Editor world/current level is no longer an admitted target; no further writer was dispatched")); }
        if((Requested.Get()&&!Live(Requested.Get()))||(Settings.Get()&&!Live(Settings.Get())))
        { return Fail(TEXT("type_changed"),TEXT("Captured requested/effective foliage type is no longer live")); }
        const UActorPartitionSubsystem* Partition=World->GetSubsystem<UActorPartitionSubsystem>();
        if(!Partition||!Partition->IsLevelPartition()) { return Fail(TEXT("partition_unsupported"),TEXT("Current-level legacy foliage API requires LevelPartition; WorldPartition routing is not implemented")); }
        if(NeedActor&&(!Live(Actor.Get())||Actor->GetWorld()!=World.Get()||Actor->GetLevel()!=Level.Get()))
        { return Fail(TEXT("actor_changed"),TEXT("Captured foliage actor was destroyed, reparented or moved out of the captured level")); }
        if(NeedActor)
        {
            Count(TEXT("get_IFA_read_calls"));if(AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World.Get(),false)!=Actor.Get())
            { return Fail(TEXT("actor_changed"),TEXT("Current level foliage actor identity changed during an Engine callback")); }
        }return true;
    }
    bool Initialize()
    {
        if(!IsEditorGameThread()||!GEditor||(WriterOperation&&IsPlaySessionActive())) { return Fail(TEXT("editor_required"),WriterOperation?TEXT("Requires an idle Editor game thread"):TEXT("Requires the Editor game thread")); }
        World=GEditor->GetEditorWorldContext().World();if(Live(World.Get())) { Level=World->GetCurrentLevel(); }return Admit();
    }
    bool Writer(const TCHAR* Stage,const TCHAR* Counter,bool NeedActor=false)
    { Result.Stage=Stage;if(!Admit(NeedActor)) { return false; }Result.bMutationAttempted=true;Result.bChangeStateKnown=false;Count(Counter);return true; }
    bool Begin()
    {
        if(!Writer(TEXT("begin_transaction"),TEXT("begin_transaction_calls"))) { return false; }
        Transaction=MakeUnique<FScopedTransaction>(FText::FromString(TEXT("Cubeless ")+Command));Count(TEXT("begin_transaction_returns"));return Admit();
    }
    void Close()
    {
        if(Transaction) { Result.bChangeStateKnown=false;Count(TEXT("end_transaction_calls"));Transaction.Reset();Count(TEXT("end_transaction_returns")); }
    }
    FFoliageInfo* Info()
    {
        if(!Live(Actor.Get())||!Live(Settings.Get())||Actor->GetWorld()!=World.Get()||Actor->GetLevel()!=Level.Get()) { return nullptr; }
        FFoliageInfo* I=Actor->FindInfo(Settings.Get());return I&&I->IFA==Actor.Get()?I:nullptr;
    }
    TSharedRef<FJsonObject> Observe()
    {
        auto J=MakeShared<FJsonObject>();const bool Scope=Live(World.Get())&&Live(Level.Get())&&Level->GetWorld()==World.Get()&&World->GetCurrentLevel()==Level.Get();
        J->SetBoolField(TEXT("captured_world_level_valid"),Scope);
        if(Live(World.Get())) { J->SetStringField(TEXT("world"),World->GetPathName());J->SetBoolField(TEXT("world_package_dirty"),World->GetPackage()->IsDirty()); }
        if(Live(Level.Get())) { J->SetStringField(TEXT("level"),Level->GetPathName());J->SetBoolField(TEXT("level_package_dirty"),Level->GetPackage()->IsDirty()); }
        const bool A=Live(Actor.Get());J->SetBoolField(TEXT("foliage_actor_valid"),A);
        if(A) { J->SetStringField(TEXT("foliage_actor"),Actor->GetPathName());J->SetBoolField(TEXT("IFA_package_dirty"),Actor->GetPackage()->IsDirty());J->SetNumberField(TEXT("registered_types"),Actor->GetFoliageInfos().Num()); }
        if(Live(Requested.Get())) { J->SetStringField(TEXT("requested_foliage_type"),Requested->GetPathName()); }
        if(Live(Settings.Get())) { J->SetStringField(TEXT("effective_foliage_type"),Settings->GetPathName()); }
        FFoliageInfo* I=Info();J->SetBoolField(TEXT("foliage_info_valid"),I!=nullptr);
        if(I)
        {
            J->SetNumberField(TEXT("instance_count"),I->Instances.Num());J->SetBoolField(TEXT("implementation_valid"),I->Implementation.IsValid());J->SetBoolField(TEXT("initialized"),I->IsInitialized());
            // Null HISM is normal for actor/ISM-actor foliage. A data count is not render completion.
            UHierarchicalInstancedStaticMeshComponent* C=I->Implementation.IsValid()?I->GetComponent():nullptr;J->SetBoolField(TEXT("HISM_component_present"),Live(C));
            if(Live(C)) { J->SetBoolField(TEXT("HISM_async_building"),C->IsAsyncBuilding());J->SetBoolField(TEXT("HISM_tree_fully_built"),C->IsTreeFullyBuilt()); }
            else { J->SetField(TEXT("HISM_async_building"),MakeShared<FJsonValueNull>());J->SetField(TEXT("HISM_tree_fully_built"),MakeShared<FJsonValueNull>()); }
        }return J;
    }
    bool Snapshot()
    {
        FFoliageInfo* I=Info();if(!I) { return Fail(TEXT("info_changed"),TEXT("Foliage info is no longer owned by the captured IFA/settings")); }
        if(I->Instances.Num()>MaxObservedInstances) { return Fail(TEXT("snapshot_limit"),TEXT("Existing foliage instance count exceeds bounded writer readback; no instance writer was dispatched")); }
        OriginalCount=I->Instances.Num();Original.Reserve(OriginalCount);
        for(const FFoliageInstance& V:I->Instances) { if(!Finite(V)) { return Fail(TEXT("nonfinite_existing_instance"),TEXT("Existing instance projection is not finite; exact writer postconditions cannot be established")); }Original.Emplace(V); }
        SnapshotReady=true;return true;
    }
    bool Readback()
    {
        FFoliageInfo* I=Info();if(!SnapshotReady||!I||I->Instances.Num()>MaxObservedInstances+MaxAdd) { Checks->SetBoolField(TEXT("target_projection_known"),false);return false; }
        const int32 Count=I->Instances.Num();bool Changed=Count!=Original.Num();
        if(!Changed) { for(int32 N=0;N<Count;++N) { if(!(FPlacement(I->Instances[N])==Original[N])) { Changed=true;break; } } }
        Result.bChanged=Changed;Result.Data->SetBoolField(TEXT("observed_instance_projection_changed"),Changed);Checks->SetBoolField(TEXT("target_projection_known"),true);
        if(ExpectAdd)
        {
            bool Match=Count==ExpectedOrder.Num();if(Match) { for(int32 N=0;N<Count;++N) { if(!(FPlacement(I->Instances[N])==ExpectedOrder[N])) { Match=false;break; } } }
            Checks->SetBoolField(TEXT("ordered_original_and_added_prefix_preserved"),Match);Checks->SetBoolField(TEXT("count_matches_add_prefix"),Count==OriginalCount+VerifiedAdded);return Match&&Count==OriginalCount+VerifiedAdded;
        }
        if(ExpectRemove)
        {
            TMap<FPlacement,int32> Remaining=ExpectedRemaining;bool Match=Count==OriginalCount-RemovalRequested;
            for(const FFoliageInstance& V:I->Instances) { int32* N=Remaining.Find(FPlacement(V));if(!N||*N<=0) { Match=false;break; }if(--(*N)==0) { Remaining.Remove(FPlacement(V)); } }
            Match=Match&&Remaining.IsEmpty();Checks->SetBoolField(TEXT("retained_instance_multiset_preserved"),Match);Checks->SetBoolField(TEXT("count_matches_unique_removals"),Count==OriginalCount-RemovalRequested);return Match;
        }return true;
    }
    FResult Finish()
    {
        // EndTransaction broadcasts callbacks; final observations and postconditions run after it.
        Close();const bool TargetOkay=!HasTarget||Admit(RequireActor);bool ProjectionOkay=true;
        if(SnapshotReady) { ProjectionOkay=Readback(); }
        if(HasTarget)
        {
            const auto After=Observe();Result.Data->SetObjectField(TEXT("target_after"),After);
            if(Before.IsValid())
            {
                FString BeforeText,AfterText;FJsonSerializer::Serialize(Before.ToSharedRef(),TJsonWriterFactory<>::Create(&BeforeText));FJsonSerializer::Serialize(After,TJsonWriterFactory<>::Create(&AfterText));
                const bool Different=BeforeText!=AfterText;Result.bChanged=Result.bChanged||Different;Result.Data->SetBoolField(TEXT("observed_target_projection_changed"),Different);
            }
        }
        if(Result.bSuccess&&(!TargetOkay||!ProjectionOkay)) { Fail(TEXT("postcondition_failed"),TEXT("Final live target/instance projection differs from the expected operation result; changes may remain")); }
        if(Result.bSuccess) { Result.bPostconditionPassed=true;Result.Stage=TEXT("complete");Legacy->SetBoolField(TEXT("success"),true); }
        Result.Data->SetNumberField(TEXT("add_instance_returns"),AddedReturns);Result.Data->SetNumberField(TEXT("verified_added_prefix"),VerifiedAdded);Result.Data->SetNumberField(TEXT("requested_unique_removals"),RemovalRequested);
        Result.Data->SetBoolField(TEXT("total_change_state_unknown"),!Result.bChangeStateKnown);Result.Data->SetBoolField(TEXT("explicit_compile_attempted"),false);Result.Data->SetBoolField(TEXT("explicit_save_attempted"),false);
        return Result;
    }
};
bool Path(FWork& W,const FString& Name)
{
    if(Name.IsEmpty()) { return W.Fail(TEXT("path_missing"),TEXT("Missing 'foliage_type_path' parameter")); }
    if(!SafeText(Name,65536)) { return W.Fail(TEXT("path_invalid"),TEXT("foliage_type_path exceeds the text bound or contains a NUL")); }
    if(Dependency(Name)) { return W.Fail(TEXT("dependency_reference"),TEXT("Refusing to persist MCP-only reference in 'foliage_type_path': ")+Name); }return true;
}
bool Load(FWork& W,const FString& Name)
{
    if(!W.Writer(TEXT("load_foliage_type"),TEXT("load_asset_calls"))) { return false; }
    W.Requested.Reset(Cast<UFoliageType>(UEditorAssetLibrary::LoadAsset(Name)));W.Count(TEXT("load_asset_returns"));
    if(!W.Admit()) { return false; }
    if(!Live(W.Requested.Get())) { return W.Fail(TEXT("type_not_found"),TEXT("FoliageType asset not found: ")+Name); }return true;
}
bool SafeType(FWork& W)
{
    if(!Live(W.Settings.Get())) { return W.Fail(TEXT("settings_invalid"),TEXT("Effective foliage settings are not live")); }
    UFoliageType* T=W.Settings.Get();
    if(Dependency(T->GetPathName())||Dependency(T->GetClass()->GetPathName())) { return W.Fail(TEXT("dependency_reference"),TEXT("Effective foliage type/class refers to an MCP-only implementation")); }
    if(const auto* Mesh=Cast<UFoliageType_InstancedStaticMesh>(T))
    { if(Mesh->GetComponentClass()&&Dependency(Mesh->GetComponentClass()->GetPathName())) { return W.Fail(TEXT("dependency_reference"),TEXT("Foliage component class refers to an MCP-only implementation")); } }
    else if(const auto* A=Cast<UFoliageType_Actor>(T))
    {
        UClass* C=A->ActorClass.Get();
        if(C&&(!Live(C)||!C->IsChildOf(AActor::StaticClass())||C->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Dependency(C->GetPathName())))
        { return W.Fail(TEXT("actor_class_invalid"),TEXT("Foliage actor class is not a live spawnable non-MCP actor class")); }
        if(A->StaticMeshOnlyComponentClass&&Dependency(A->StaticMeshOnlyComponentClass->GetPathName())) { return W.Fail(TEXT("dependency_reference"),TEXT("Foliage ISM-actor component class refers to an MCP-only implementation")); }
    }
    else { return W.Fail(TEXT("implementation_unsupported"),TEXT("Installed public foliage SDK only creates implementations for StaticMesh and Actor foliage types")); }
    return true;
}
bool Transform(FWork& W,const TSharedPtr<FJsonValue>& V,FFoliageInstance& Out)
{
    if(!V.IsValid()||V->Type!=EJson::Object||!V->AsObject().IsValid()) { return W.Fail(TEXT("transform_invalid"),TEXT("Each transforms entry must be an object")); }
    const auto O=V->AsObject();const TArray<TSharedPtr<FJsonValue>>* A=nullptr;
    if(!O->TryGetArrayField(TEXT("location"),A)||!A||A->Num()!=3) { return W.Fail(TEXT("transform_invalid"),TEXT("Each transforms entry needs location:[x,y,z]")); }
    // Use the same SDK AsNumber conversion as UE56 (including bool/numeric-string/default-zero coercion).
    auto Read3=[&](const TArray<TSharedPtr<FJsonValue>>& Values,double* N)->bool
    { for(int32 C=0;C<3;++C) { if(!Values[C].IsValid()) { return false; }N[C]=Values[C]->AsNumber();if(!FMath::IsFinite(N[C])) { return false; } }return true; };
    double N[3];if(!Read3(*A,N)) { return W.Fail(TEXT("transform_nonfinite"),TEXT("Transform location must remain finite after SDK numeric conversion")); }Out.Location=FVector(N[0],N[1],N[2]);
    if(O->TryGetArrayField(TEXT("rotation"),A)&&A&&A->Num()==3)
    { if(!Read3(*A,N)) { return W.Fail(TEXT("transform_nonfinite"),TEXT("Transform rotation must remain finite after SDK numeric conversion")); }Out.Rotation=FRotator(N[1],N[2],N[0]); }
    if(O->TryGetArrayField(TEXT("scale"),A)&&A&&A->Num()==3)
    { if(!Read3(*A,N)) { return W.Fail(TEXT("transform_nonfinite"),TEXT("Transform scale must remain finite after SDK numeric conversion")); }Out.DrawScale3D=FVector3f(static_cast<float>(N[0]),static_cast<float>(N[1]),static_cast<float>(N[2])); }
    if(!Finite(Out)) { return W.Fail(TEXT("transform_nonfinite"),TEXT("Transform must remain finite after float32 scale conversion")); }return true;
}
void RemoveLegacy(FWork& W,bool HasActor,TOptional<bool> Registered,int32 Before,int32 Remaining,bool Dirty,TOptional<bool> All={})
{
    W.Legacy->SetBoolField(TEXT("had_foliage_actor"),HasActor);if(Registered.IsSet()) { W.Legacy->SetBoolField(TEXT("foliage_type_registered"),Registered.GetValue()); }
    W.Legacy->SetStringField(TEXT("foliage_type"),W.Requested->GetPathName());W.Legacy->SetNumberField(TEXT("instances_removed"),Before-Remaining);W.Legacy->SetNumberField(TEXT("total_instances_before"),Before);W.Legacy->SetNumberField(TEXT("remaining_instances"),Remaining);W.Legacy->SetBoolField(TEXT("level_dirty"),Dirty);
    if(All.IsSet()) { W.Legacy->SetBoolField(TEXT("remove_all"),All.GetValue());W.Legacy->SetStringField(TEXT("note"),TEXT("Level save is left to the user per map-safety policy")); }
}
}

Variable::FResult List()
{
    FWork W(TEXT("list_foliage_types"),false);if(!W.Initialize()) { return W.Finish(); }
    if(Running) { W.Fail(TEXT("reentrant"),TEXT("A foliage operation is already active on this game thread"));return W.Finish(); }Running=true;ON_SCOPE_EXIT { Running=false; };
    W.Result.Stage=TEXT("read_current_level");W.Count(TEXT("get_IFA_read_calls"));W.Actor=AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(W.World.Get(),false);W.HasTarget=true;W.RequireActor=Live(W.Actor.Get());
    if(!W.Admit(Live(W.Actor.Get()))) { return W.Finish(); }
    W.Before=W.Observe();W.Result.Data->SetObjectField(TEXT("target_before"),W.Before.ToSharedRef());TArray<TSharedPtr<FJsonValue>> Types;int64 Total=0;
    if(Live(W.Actor.Get()))
    {
        if(W.Actor->GetFoliageInfos().Num()>MaxTypes) { W.Fail(TEXT("type_limit"),TEXT("Foliage type count exceeds bounded listing; no partial success was returned"));return W.Finish(); }
        for(const auto& Pair:W.Actor->GetFoliageInfos())
        {
            const UFoliageType* T=Pair.Key;if(!T) { continue; }if(!Live(T)||Pair.Value->IFA!=W.Actor.Get()) { W.Fail(TEXT("info_invalid"),TEXT("Foliage type/info is not live in the captured IFA"));return W.Finish(); }
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("foliage_type"),T->GetPathName());J->SetNumberField(TEXT("instance_count"),Pair.Value->Instances.Num());Types.Add(MakeShared<FJsonValueObject>(J));Total+=Pair.Value->Instances.Num();
        }
    }
    W.Legacy->SetBoolField(TEXT("has_foliage_actor"),Live(W.Actor.Get()));W.Legacy->SetNumberField(TEXT("type_count"),Types.Num());W.Legacy->SetNumberField(TEXT("total_instances"),static_cast<double>(Total));W.Legacy->SetArrayField(TEXT("foliage_types"),Types);
    W.Checks->SetBoolField(TEXT("current_level_editor_instance_counts_observed"),true);W.Result.Data->SetBoolField(TEXT("read_only_direct_API"),true);W.Result.bSuccess=true;return W.Finish();
}

Variable::FResult Add(const FString& FoliageTypePath,const FString& TransformsJson)
{
    FWork W(TEXT("add_foliage_instances"),true);if(!Path(W,FoliageTypePath)) { return W.Finish(); }
    TSharedPtr<FJsonValue> Value;if(!Parse(TransformsJson,Value)||Value->Type!=EJson::Array||Value->AsArray().IsEmpty())
    { W.Fail(TEXT("transforms_invalid"),TEXT("Missing 'transforms' array: [{location:[x,y,z], rotation:[roll,pitch,yaw], scale:[x,y,z]}]"));return W.Finish(); }
    const auto& Transforms=Value->AsArray();if(Transforms.Num()>MaxAdd) { W.Fail(TEXT("transforms_limit"),FString::Printf(TEXT("transforms exceeds the %d-instance per-call limit (%d)"),MaxAdd,Transforms.Num()));return W.Finish(); }
    if(!W.Initialize()) { return W.Finish(); }if(Running) { W.Fail(TEXT("reentrant"),TEXT("A foliage operation is already active on this game thread"));return W.Finish(); }Running=true;ON_SCOPE_EXIT { Running=false; };
    if(!Load(W,FoliageTypePath)) { return W.Finish(); }
    W.Count(TEXT("get_IFA_read_calls"));W.Actor=AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(W.World.Get(),false);W.HasTarget=true;W.RequireActor=Live(W.Actor.Get());
    W.Settings.Reset(W.Requested.Get());if(!SafeType(W)) { return W.Finish(); }W.Before=W.Observe();W.Result.Data->SetObjectField(TEXT("target_before"),W.Before.ToSharedRef());
    if(!W.Begin()||!W.Writer(TEXT("get_or_create_IFA"),TEXT("get_IFA_create_calls"))) { return W.Finish(); }
    W.Actor=AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(W.World.Get(),true);W.Count(TEXT("get_IFA_create_returns"));
    W.RequireActor=Live(W.Actor.Get());if(!Live(W.Actor.Get())) { W.Fail(TEXT("actor_create_failed"),TEXT("Failed to get or create InstancedFoliageActor for the current level"));return W.Finish(); }
    if(!W.Writer(TEXT("modify_IFA"),TEXT("IFA_modify_calls"),true)) { return W.Finish(); }W.Actor->Modify();W.Count(TEXT("IFA_modify_returns"));
    if(!W.Writer(TEXT("register_foliage_type"),TEXT("add_foliage_type_calls"),true)) { return W.Finish(); }
    FFoliageInfo* OutputInfo=nullptr;W.Settings.Reset(W.Actor->AddFoliageType(W.Requested.Get(),&OutputInfo));W.Count(TEXT("add_foliage_type_returns"));
    // OutputInfo cannot safely cross later engine calls. Reacquire by the actual returned settings key.
    if(!W.Admit(true)||!Live(W.Settings.Get())||!OutputInfo||!W.Info()) { W.Fail(TEXT("type_registration_failed"),TEXT("Failed to register foliage type on the foliage actor"));return W.Finish(); }
    if(!SafeType(W)||!W.Snapshot()) { return W.Finish(); }W.ExpectedOrder=W.Original;W.ExpectAdd=true;
    W.Result.Data->SetNumberField(TEXT("requested_transforms"),Transforms.Num());W.Result.Data->SetBoolField(TEXT("per_item_validation_interleaved_with_writes"),true);
    W.Result.Data->SetBoolField(TEXT("numeric_components_follow_Engine_AsNumber_coercion"),true);
    for(int32 N=0;N<Transforms.Num();++N)
    {
        W.Result.Data->SetNumberField(TEXT("current_transform_index"),N);W.Result.Stage=TEXT("parse_transform");FFoliageInstance Instance;
        if(!Transform(W,Transforms[N],Instance)) { return W.Finish(); }
        FFoliageInfo* I=W.Info();if(!I||!I->Implementation.IsValid()||!I->InstanceHash.IsValid()) { W.Fail(TEXT("info_invalid"),TEXT("Foliage info lacks its public implementation/hash prerequisites"));return W.Finish(); }
        const int32 Before=I->Instances.Num();if(Before!=W.OriginalCount+W.VerifiedAdded) { W.Fail(TEXT("prefix_changed"),TEXT("Instance count changed outside the verified addition prefix"));return W.Finish(); }
        if(!W.Writer(TEXT("add_instance"),TEXT("add_instance_calls"),true)) { return W.Finish(); }
        // First AddInstance legitimately initializes a newly registered implementation.
        I=W.Info();if(!I||!I->Implementation.IsValid()) { W.Fail(TEXT("info_changed"),TEXT("Foliage info changed before AddInstance"));return W.Finish(); }
        I->AddInstance(W.Settings.Get(),Instance);++W.AddedReturns;W.Count(TEXT("add_instance_returns"));
        if(!W.Admit(true)) { return W.Finish(); }I=W.Info();
        if(!I||I->Instances.Num()!=Before+1||!Finite(I->Instances[Before])||I->Instances[Before].Location!=Instance.Location||I->Instances[Before].Rotation!=Instance.Rotation||I->Instances[Before].DrawScale3D!=Instance.DrawScale3D)
        { W.Fail(TEXT("add_postcondition_failed"),TEXT("AddInstance returned without the expected appended editor-instance transform; actual changes may remain"));return W.Finish(); }
        W.ExpectedOrder.Emplace(I->Instances[Before]);++W.VerifiedAdded;
    }
    if(!W.Writer(TEXT("refresh_foliage"),TEXT("refresh_calls"),true)) { return W.Finish(); }
    FFoliageInfo* I=W.Info();if(!I||!I->Implementation.IsValid()) { W.Fail(TEXT("info_changed"),TEXT("Foliage info changed before Refresh"));return W.Finish(); }I->Refresh(true,true);W.Count(TEXT("refresh_returns"));
    if(!W.Writer(TEXT("mark_IFA_dirty"),TEXT("IFA_mark_dirty_calls"),true)) { return W.Finish(); }W.Result.Data->SetBoolField(TEXT("explicit_IFA_mark_dirty_return"),W.Actor->MarkPackageDirty());W.Count(TEXT("IFA_mark_dirty_returns"));
    W.Close();if(!W.Admit(true)||(I=W.Info())==nullptr) { W.Fail(TEXT("info_changed"),TEXT("Foliage info changed after transaction close"));return W.Finish(); }
    W.Legacy->SetStringField(TEXT("foliage_type"),W.Requested->GetPathName());W.Legacy->SetNumberField(TEXT("instances_added"),W.AddedReturns);W.Legacy->SetNumberField(TEXT("total_instances"),I->Instances.Num());W.Legacy->SetBoolField(TEXT("level_dirty"),true);W.Legacy->SetStringField(TEXT("note"),TEXT("Level save is left to the user per map-safety policy"));W.Result.bSuccess=true;return W.Finish();
}

Variable::FResult Remove(const FString& FoliageTypePath,const FString& InstanceIndicesJson,bool bRemoveAll,int64 MaxRemoveCount,const FString& PrivateContextJson)
{
    FWork W(TEXT("remove_foliage_instances"),true);if(!Path(W,FoliageTypePath)) { return W.Finish(); }
    TSharedPtr<FJsonValue> Carrier;TSharedPtr<FJsonObject> Raw;const TArray<TSharedPtr<FJsonValue>>* Indices=nullptr;bool HasIndices=false;
    const bool LegacyRaw=!PrivateContextJson.IsEmpty();int32 Limit=static_cast<int32>(FMath::Clamp<int64>(MaxRemoveCount,1,MaxRemove));
    if(LegacyRaw)
    {
        if(!Parse(PrivateContextJson,Carrier)||Carrier->Type!=EJson::Object||!Carrier->AsObject().IsValid()) { W.Fail(TEXT("raw_context_invalid"),TEXT("Private raw remove context must be a bounded JSON object"));return W.Finish(); }
        Raw=Carrier->AsObject();for(const auto& Pair:Raw->Values) { if(Pair.Key!=TEXT("instance_indices")&&Pair.Key!=TEXT("remove_all")&&Pair.Key!=TEXT("max_remove_count")) { W.Fail(TEXT("raw_context_invalid"),TEXT("Private raw remove context accepts only original indices/remove_all/max fields"));return W.Finish(); } }
        bRemoveAll=false;Raw->TryGetBoolField(TEXT("remove_all"),bRemoveAll);
        double Number=5000;Raw->TryGetNumberField(TEXT("max_remove_count"),Number);
        if(!FMath::IsFinite(Number)||Number<=static_cast<double>(MIN_int32)-1.0||Number>=static_cast<double>(MAX_int32)+1.0) { W.Fail(TEXT("raw_max_unsafe"),TEXT("Raw max_remove_count cannot be safely converted to legacy int32"));return W.Finish(); }
        Limit=FMath::Clamp(static_cast<int32>(Number),1,MaxRemove);HasIndices=Raw->TryGetArrayField(TEXT("instance_indices"),Indices);W.Result.Data->SetObjectField(TEXT("legacy_raw_context"),Raw.ToSharedRef());
    }
    else
    {
        if(!Parse(InstanceIndicesJson,Carrier)) { W.Fail(TEXT("indices_invalid"),TEXT("instance_indices carrier must be bounded JSON"));return W.Finish(); }
        HasIndices=Carrier->Type==EJson::Array;if(HasIndices) { Indices=&Carrier->AsArray(); }
    }
    W.Result.Data->SetBoolField(TEXT("legacy_raw_path"),LegacyRaw);W.Result.Data->SetBoolField(TEXT("effective_remove_all"),bRemoveAll);W.Result.Data->SetNumberField(TEXT("effective_max_remove_count"),Limit);
    if(!bRemoveAll&&(!HasIndices||!Indices||Indices->IsEmpty())) { W.Fail(TEXT("indices_missing"),TEXT("Missing 'instance_indices' array; set remove_all=true to clear every instance for the foliage type"));return W.Finish(); }
    if(!W.Initialize()) { return W.Finish(); }if(Running) { W.Fail(TEXT("reentrant"),TEXT("A foliage operation is already active on this game thread"));return W.Finish(); }Running=true;ON_SCOPE_EXIT { Running=false; };
    if(!Load(W,FoliageTypePath)) { return W.Finish(); }W.Settings.Reset(W.Requested.Get());
    W.Count(TEXT("get_IFA_read_calls"));W.Actor=AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(W.World.Get(),false);W.HasTarget=true;W.RequireActor=Live(W.Actor.Get());
    if(!W.Admit(Live(W.Actor.Get()))) { return W.Finish(); }W.Before=W.Observe();W.Result.Data->SetObjectField(TEXT("target_before"),W.Before.ToSharedRef());
    if(!Live(W.Actor.Get())) { RemoveLegacy(W,false,{},0,0,false);W.Checks->SetBoolField(TEXT("no_IFA_created"),true);W.Result.bSuccess=true;return W.Finish(); }
    FFoliageInfo* I=W.Info();if(!I) { RemoveLegacy(W,true,false,0,0,false);W.Checks->SetBoolField(TEXT("no_type_registered"),true);W.Result.bSuccess=true;return W.Finish(); }
    const int32 Total=I->Instances.Num();TSet<int32> Unique;TArray<int32> Remove;
    if(bRemoveAll)
    {
        if(Total>Limit) { W.Fail(TEXT("remove_limit"),FString::Printf(TEXT("remove_all would remove %d instances, exceeding max_remove_count=%d"),Total,Limit));return W.Finish(); }
        Remove.Reserve(Total);for(int32 N=0;N<Total;++N) { Unique.Add(N);Remove.Add(N); }
    }
    else
    {
        for(const auto& V:*Indices)
        {
            if(!V.IsValid()||V->Type!=EJson::Number) { W.Fail(TEXT("indices_invalid"),TEXT("instance_indices must contain only integer numbers"));return W.Finish(); }
            const double Number=V->AsNumber();
            if(!FMath::IsFinite(Number)||Number<=static_cast<double>(MIN_int32)-1.0||Number>=static_cast<double>(MAX_int32)+1.0) { W.Fail(TEXT("index_unsafe"),TEXT("instance index cannot be safely converted to legacy int32"));return W.Finish(); }
            const int32 Index=static_cast<int32>(Number);if(!FMath::IsNearlyEqual(Number,static_cast<double>(Index))) { W.Fail(TEXT("indices_invalid"),TEXT("instance_indices must contain only integer numbers"));return W.Finish(); }
            if(Index<0||Index>=Total) { W.Fail(TEXT("index_out_of_range"),FString::Printf(TEXT("instance index %d is out of range for %d instances"),Index,Total));return W.Finish(); }Unique.Add(Index);
        }
        if(Unique.Num()>Limit) { W.Fail(TEXT("remove_limit"),FString::Printf(TEXT("instance_indices requests %d removals, exceeding max_remove_count=%d"),Unique.Num(),Limit));return W.Finish(); }
        for(int32 Index:Unique) { Remove.Add(Index); }
    }
    if(Remove.IsEmpty()) { RemoveLegacy(W,true,true,Total,Total,false);W.Result.bSuccess=true;return W.Finish(); }
    if(!I->Implementation.IsValid()||!I->InstanceHash.IsValid()||!I->IsInitialized()) { W.Fail(TEXT("info_uninitialized"),TEXT("Nonempty RemoveInstances requires initialized public foliage implementation/hash prerequisites"));return W.Finish(); }
    if(!W.Snapshot()) { return W.Finish(); }W.ExpectRemove=true;W.RemovalRequested=Remove.Num();
    for(int32 N=0;N<Total;++N) { if(!Unique.Contains(N)) { ++W.ExpectedRemaining.FindOrAdd(W.Original[N]); } }
    TArray<TSharedPtr<FJsonValue>> IndicesReceipt;for(int32 N:Remove) { IndicesReceipt.Add(MakeShared<FJsonValueNumber>(N)); }W.Result.Data->SetArrayField(TEXT("unique_indices_in_before_frame"),IndicesReceipt);
    W.Result.Data->SetStringField(TEXT("indices_after_remove"),TEXT("Not stable handles: SDK RemoveAtSwap may relocate retained instances; validation compares the exact retained projection multiset."));
    if(!W.Begin()||!W.Writer(TEXT("modify_IFA"),TEXT("IFA_modify_calls"),true)) { return W.Finish(); }W.Actor->Modify();W.Count(TEXT("IFA_modify_returns"));
    if(!W.Admit(true)) { return W.Finish(); }I=W.Info();
    if(!I||!I->Implementation.IsValid()||!I->IsInitialized()||I->Instances.Num()!=Total) { W.Fail(TEXT("info_changed"),TEXT("Foliage info changed before RemoveInstances; no removal call dispatched"));return W.Finish(); }
    for(int32 N=0;N<Total;++N) { if(!(FPlacement(I->Instances[N])==W.Original[N])) { W.Fail(TEXT("indices_frame_changed"),TEXT("Instance index frame changed after transaction/Modify callbacks; no removal call dispatched"));return W.Finish(); } }
    if(!W.Writer(TEXT("remove_instances"),TEXT("remove_instances_calls"),true)) { return W.Finish(); }
    I->RemoveInstances(MakeArrayView(Remove),true);W.Count(TEXT("remove_instances_returns"));
    if(!W.Writer(TEXT("mark_IFA_dirty"),TEXT("IFA_mark_dirty_calls"),true)) { return W.Finish(); }W.Actor->MarkPackageDirty();W.Count(TEXT("IFA_mark_dirty_returns"));
    if(!W.Writer(TEXT("mark_world_dirty"),TEXT("world_mark_dirty_calls"),true)) { return W.Finish(); }W.Result.Data->SetBoolField(TEXT("explicit_world_mark_dirty_return"),W.World->MarkPackageDirty());W.Count(TEXT("world_mark_dirty_returns"));
    W.Close();if(!W.Admit(true)||(I=W.Info())==nullptr) { W.Fail(TEXT("info_changed"),TEXT("Foliage info changed after transaction close"));return W.Finish(); }
    RemoveLegacy(W,true,true,Total,I->Instances.Num(),true,bRemoveAll);W.Result.bSuccess=true;return W.Finish();
}
}
