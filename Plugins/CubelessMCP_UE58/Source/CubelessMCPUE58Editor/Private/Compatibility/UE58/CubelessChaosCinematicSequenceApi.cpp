#include "Compatibility/UE58/CubelessChaosCinematicSequenceApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/CacheCollection.h"
#include "Chaos/CacheManagerActor.h"
#include "Chaos/ChaosCache.h"
#include "Chaos/Sequencer/MovieSceneChaosCacheSection.h"
#include "Chaos/Sequencer/MovieSceneChaosCacheTrack.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Field/FieldSystemActor.h"
#include "Field/FieldSystemComponent.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionRootProxyRenderer.h"
#include "HAL/FileManager.h"
#include "LevelSequence.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <cmath>

namespace Cubeless::UE58::ChaosCinematicSequence
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr int32 MaxJsonChars=1048576,MaxJsonDepth=64,MaxSlots=262144,MaxObjects=16384;
bool Running=false;
bool Live(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
FString Json(const TSharedPtr<FJsonObject>& O) { FString S;if(O) { FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S)); }return S; }
bool Dependency(const FString& S)
{
    for(const TCHAR* K:{TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal")}) { if(S.Contains(K,ESearchCase::IgnoreCase)) { return true; } }
    FString Candidate=S.Replace(TEXT("\\"),TEXT("/")).ToLower();TArray<FString> Segments;Candidate.ParseIntoArray(Segments,TEXT("/"),false);const FString Mount=Segments.IsValidIndex(1)?Segments[1]:FString();FString Module;
    if(Mount.Equals(TEXT("script"),ESearchCase::CaseSensitive)&&Segments.IsValidIndex(2)) { Module=Segments[2];int32 I;if(Module.FindChar(TEXT('.'),I)) { Module=Module.Left(I); }if(Module.FindChar(TEXT(':'),I)) { Module=Module.Left(I); } }
    for(const TCHAR* K:{TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("cubelesscontrolrigtools"),TEXT("cubelessmcp"),TEXT("cubeless_mcp"),TEXT("modelcontextprotocol"),TEXT("toolsetregistry"),TEXT("pythonscriptplugin"),TEXT("editorscriptingutilities")}) { if(Mount.Equals(K,ESearchCase::CaseSensitive)||(!Module.IsEmpty()&&Module.StartsWith(K,ESearchCase::CaseSensitive))) { return true; } }
    for(const TCHAR* K:{TEXT("blutility"),TEXT("unrealed"),TEXT("blueprinteditorlibrary")}) { if(Module.Equals(K,ESearchCase::CaseSensitive)) { return true; } }
    return Mount.Equals(TEXT("cubelessmcp_ue58"),ESearchCase::CaseSensitive)||Candidate.Equals(TEXT("/engine/pythontypes"),ESearchCase::CaseSensitive)||Candidate.StartsWith(TEXT("/engine/pythontypes/"),ESearchCase::CaseSensitive)||Candidate.StartsWith(TEXT("/engine/pythontypes."),ESearchCase::CaseSensitive)||Candidate.StartsWith(TEXT("/engine/pythontypes:"),ESearchCase::CaseSensitive);
}
struct FExactKeyFuncs:DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& S) { return FCrc::StrCrc32(*S); }
};
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||S.Len()>MaxJsonChars) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }
    auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeyFuncs>> Keys;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>MaxSlots) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>MaxJsonDepth) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
        if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber())) { return false; }
    }
    return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)&&Out.IsValid();
}
UWorld* CurrentWorld() { if(!GEditor) { return nullptr; }if(auto* C=GEditor->GetPIEWorldContext()) { if(C->World()) { return C->World(); } }return GEditor->GetEditorWorldContext().World(); }
struct FTracked
{
    TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Weak,Outer,ExpectedParent;TWeakObjectPtr<UPackage> Package;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UWorld> ObjectWorld;FString Path,PackagePath,Role;TSharedPtr<FJsonObject> Before;bool ExpectedParentCaptured=false,ObjectWorldCaptured=false;
};
struct FWork
{
    FResult R;TSharedPtr<FJsonObject> Params;FString Operation,FailedStage;bool Writer=false,WorldCaptured=false;
    TWeakObjectPtr<UWorld> World;TArray<FTracked> Tracked;TArray<TSharedPtr<FJsonValue>> Saves,Events;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>();int64 FileBytes=0;int32 FileReads=0;
    explicit FWork(const TCHAR* Name,bool bWriter):Operation(Name),Writer(bWriter)
    {
        R.Compiled=false;R.bSaveRequested=bWriter;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("postconditions"),Checks);
        R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_wrapper_wait_performed"),false);
        R.Data->SetStringField(TEXT("SDK_synchronous_scope"),TEXT("EndEvaluate may wait for solver tasks, finalize adapters and flush recording. ClearObservedComponents(false) restores previous component simulation/notify flags. Wrapper adds no wait or worker UObject execution; these SDK calls are not nonblocking."));
        R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("saved retains any explicit EditorAssetLibrary SaveAsset true result even if a later stage or identity guard fails. Save callbacks/checkout/full loading and global disk effects are not exhaustively known. Clean readback and all requested saves are separate."));
        R.Data->SetStringField(TEXT("world_scope"),TEXT("Captured active PIE context world when present, otherwise current Editor world, including loaded actors/streaming levels only. EditorAssetSubsystem PIE save permission remains unchanged; default is refusal."));
        R.Data->SetStringField(TEXT("observation_scope"),TEXT("Captured object identity/outer/package/dirty plus selected declarative sequence/cache/component fields. Not all UObject properties, rendered frames, effective solver state, byte-exact persistence or global SDK purity."));
        R.Data->SetBoolField(TEXT("cold_reload_verified"),false);R.Data->SetBoolField(TEXT("render_or_physics_completion_verified"),false);R.Data->SetBoolField(TEXT("whole_external_pipeline_complete"),false);
        for(const TCHAR* K:{TEXT("load_calls"),TEXT("modify_calls"),TEXT("new_sequence_calls"),TEXT("initialize_calls"),TEXT("EndEvaluate_calls"),TEXT("ClearObservedComponents_false_calls"),TEXT("FindOrAddObservedComponent_calls"),TEXT("SetSimulatePhysics_calls"),TEXT("add_possessable_calls"),TEXT("bind_possessable_calls"),TEXT("add_track_calls"),TEXT("add_section_calls"),TEXT("explicit_save_calls"),TEXT("explicit_save_returns"),TEXT("file_read_calls"),TEXT("explicit_Blueprint_compile_calls"),TEXT("explicit_wrapper_wait_calls")}) { Counts->SetNumberField(K,0); }
    }
    void Count(const TCHAR* K) { double N=0;Counts->TryGetNumberField(K,N);Counts->SetNumberField(K,N+1); }
    void SDK(const TCHAR* S) { R.Stage=S;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true); }
    void Write(const TCHAR* S) { SDK(S);R.bMutationAttempted=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),S);J->SetNumberField(TEXT("index"),Events.Num());if(Events.Num()<65536) { Events.Add(MakeShared<FJsonValueObject>(J)); }else { R.Data->SetBoolField(TEXT("mutation_event_budget_exceeded"),true); } }
    bool Fail(const TCHAR* C,const FString& E) { if(R.Error.IsEmpty()) { R.ErrorCode=TEXT("chaos_cinematic_")+FString(C);R.Error=E;FailedStage=R.Stage;R.Data->SetStringField(TEXT("failed_stage"),FailedStage); }return false; }
    bool Guard()
    {
        if(!IsInGameThread()||!GEditor) { return Fail(TEXT("editor_lost"),TEXT("Editor game-thread context is unavailable")); }
        if(WorldCaptured&&CurrentWorld()!=World.Get()) { return Fail(TEXT("world_changed"),TEXT("Captured Editor/PIE world changed")); }
        for(const auto& T:Tracked)
        {
            const UObject* O=T.Weak.Get();if(!Live(O)||!Live(T.Package.Get())||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)||!T.Package->GetName().Equals(T.PackagePath,ESearchCase::CaseSensitive)||(T.ExpectedParentCaptured&&(!Live(T.ExpectedParent.Get())||!O->IsIn(T.ExpectedParent.Get())))) { return Fail(TEXT("target_identity_lost"),TEXT("Captured object class/outer/package/path or expected parent changed")); }
            if(T.ObjectWorldCaptured) { const UWorld* Actual=O->GetWorld();if(!Live(O)||!Live(T.ObjectWorld.Get())||Actual!=T.ObjectWorld.Get()||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)) { return Fail(TEXT("target_world_lost"),TEXT("Captured actor/component world or identity changed")); } }
        }
        return true;
    }
    TSharedPtr<FJsonObject> Snapshot(UObject* Object,bool Detailed=true)
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("live"),Live(Object));if(!Live(Object)) { return J; }
        SDK(TEXT("observe_captured_object"));J->SetStringField(TEXT("path"),Object->GetPathName());J->SetStringField(TEXT("class"),Object->GetClass()->GetPathName());J->SetStringField(TEXT("outer"),Object->GetOuter()?Object->GetOuter()->GetPathName():FString());J->SetStringField(TEXT("package"),Object->GetPackage()->GetName());J->SetBoolField(TEXT("package_dirty"),Object->GetPackage()->IsDirty());
        if(!Detailed) { J->SetBoolField(TEXT("detailed_SDK_readback_skipped_after_guard_failure"),true);return J; }
        if(auto* S=Cast<UMovieScene>(Object))
        {
            J->SetNumberField(TEXT("display_numerator"),S->GetDisplayRate().Numerator);J->SetNumberField(TEXT("display_denominator"),S->GetDisplayRate().Denominator);J->SetNumberField(TEXT("tick_numerator"),S->GetTickResolution().Numerator);J->SetNumberField(TEXT("tick_denominator"),S->GetTickResolution().Denominator);const auto Range=S->GetPlaybackRange();J->SetBoolField(TEXT("playback_closed"),Range.HasLowerBound()&&Range.HasUpperBound());if(Range.HasLowerBound()&&Range.HasUpperBound()) { J->SetNumberField(TEXT("playback_lower"),Range.GetLowerBoundValue().Value);J->SetNumberField(TEXT("playback_upper"),Range.GetUpperBoundValue().Value); }
            TArray<TSharedPtr<FJsonValue>> B;for(const auto& Binding:S->GetBindings()) { auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("guid"),Binding.GetObjectGuid().ToString());V->SetNumberField(TEXT("track_count"),Binding.GetTracks().Num());B.Add(MakeShared<FJsonValueObject>(V)); }J->SetArrayField(TEXT("bindings"),B);
        }
        if(auto* S=Cast<UMovieSceneSection>(Object))
        {
            const auto Range=S->GetRange();J->SetBoolField(TEXT("range_closed"),Range.HasLowerBound()&&Range.HasUpperBound());if(Range.HasLowerBound()&&Range.HasUpperBound()) { J->SetNumberField(TEXT("start_tick"),S->GetInclusiveStartFrame().Value);J->SetNumberField(TEXT("end_tick"),S->GetExclusiveEndFrame().Value); }
            if(auto* C=Cast<UMovieSceneChaosCacheSection>(S)) { J->SetNumberField(TEXT("play_rate"),C->Params.PlayRate);J->SetNumberField(TEXT("offset_tick"),C->Params.StartFrameOffset.Value);J->SetBoolField(TEXT("reverse"),C->Params.bReverse);J->SetStringField(TEXT("collection"),C->Params.CacheCollection?C->Params.CacheCollection->GetPathName():FString()); }
        }
        if(auto* C=Cast<UGeometryCollectionComponent>(Object)) { J->SetNumberField(TEXT("object_type"),static_cast<int32>(C->ObjectType));const bool Sim=C->IsSimulatingPhysics();if(!Guard()) { return J; }J->SetBoolField(TEXT("simulate_physics"),Sim);J->SetBoolField(TEXT("notify_breaks"),C->bNotifyBreaks); }
        if(auto* C=Cast<UChaosCacheCollection>(Object)) { J->SetNumberField(TEXT("max_frames"),C->GetMaxNumFrames());J->SetNumberField(TEXT("max_duration"),C->GetMaxDuration());TArray<TSharedPtr<FJsonValue>> Caches;for(const auto* Cache:C->GetCaches()) { Caches.Add(MakeShared<FJsonValueString>(Live(Cache)?Cache->GetPathName():FString())); }J->SetArrayField(TEXT("cache_paths"),Caches); }
        if(auto* M=Cast<AChaosCacheManager>(Object))
        {
            J->SetStringField(TEXT("collection"),M->CacheCollection?M->CacheCollection->GetPathName():FString());J->SetNumberField(TEXT("cache_mode"),static_cast<int32>(M->CacheMode));J->SetNumberField(TEXT("start_mode"),static_cast<int32>(M->StartMode));const TArray<FObservedComponent> Copy=M->GetObservedComponents();TArray<TSharedPtr<FJsonValue>> Rows;
            for(const auto& O:Copy) { auto V=MakeShared<FJsonObject>();auto* C=O.GetComponent(M);if(!Guard()) { return J; }V->SetStringField(TEXT("component"),Live(C)?C->GetPathName():FString());V->SetStringField(TEXT("cache_name"),O.CacheName.ToString());V->SetBoolField(TEXT("playback"),O.bPlaybackEnabled);V->SetBoolField(TEXT("captured_simulating"),O.bIsSimulating);V->SetBoolField(TEXT("captured_notify"),O.bHasNotifyBreaks);Rows.Add(MakeShared<FJsonValueObject>(V)); }J->SetArrayField(TEXT("observed"),Rows);
        }
        return J;
    }
    bool Track(UObject* O,const TCHAR* Role,UObject* Parent=nullptr,bool Created=false)
    {
        if(!Live(O)||!Live(O->GetPackage())||Dependency(O->GetClass()->GetPathName())||(Parent&&!O->IsIn(Parent))) { return Fail(TEXT("target_unavailable"),TEXT("Live runtime-safe object under its expected parent required")); }
        for(const auto& T:Tracked) { if(T.Weak.Get()==O) { return Guard(); } }
        if(Tracked.Num()>=MaxObjects) { return Fail(TEXT("object_budget"),TEXT("Captured object budget exceeded")); }
        auto& T=Tracked.AddDefaulted_GetRef();T.Hold.Reset(O);T.Weak=O;T.Outer=O->GetOuter();T.ExpectedParent=Parent;T.ExpectedParentCaptured=Parent!=nullptr;T.Package=O->GetPackage();T.Class=O->GetClass();T.Path=O->GetPathName();T.PackagePath=O->GetPackage()->GetName();T.Role=Role;T.ObjectWorldCaptured=O->IsA<AActor>()||O->IsA<UActorComponent>();if(T.ObjectWorldCaptured) { T.ObjectWorld=O->GetWorld(); }T.Before=Snapshot(O);R.bChanged|=Created;return Guard();
    }
    bool Modify(UObject* O,const TCHAR* Stage) { if(!Guard()||!Track(O,Stage)) { return false; }Write(Stage);Count(TEXT("modify_calls"));O->Modify();return Guard(); }
    template<typename T>T* Load(const FString& Path,const TCHAR* Stage)
    {
        if(Dependency(Path)) { Fail(TEXT("dependency_rejected"),TEXT("MCP-only asset dependency rejected"));return nullptr; }SDK(Stage);Count(TEXT("load_calls"));T* O=LoadObject<T>(nullptr,*Path);if(O&&!Track(O,Stage)) { return nullptr; }if(!Guard()) { return nullptr; }return O;
    }
    bool Save(UObject* Asset,const FString& RequestedPath,const TCHAR* Stage,bool MarkDirty)
    {
        if(!Guard()||!Track(Asset,Stage)) { return false; }TStrongObjectPtr<UPackage> Package(Asset->GetPackage());const FString Before=Package->GetName();
        if(MarkDirty) { Write(TEXT("mark_target_package_dirty"));Asset->MarkPackageDirty();if(!Guard()) { return false; } }
        Write(Stage);R.bSaveAttempted=true;Count(TEXT("explicit_save_calls"));const bool Returned=UEditorAssetLibrary::SaveAsset(RequestedPath,false);Count(TEXT("explicit_save_returns"));R.bSaved|=Returned;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetStringField(TEXT("requested_path"),RequestedPath);Row->SetStringField(TEXT("captured_package_before"),Before);Row->SetBoolField(TEXT("SDK_save_returned"),Returned);Row->SetBoolField(TEXT("captured_package_live"),Live(Package.Get()));if(Live(Package.Get())) { Row->SetStringField(TEXT("captured_package_after"),Package->GetName());Row->SetBoolField(TEXT("captured_package_dirty_after"),Package->IsDirty());Row->SetBoolField(TEXT("captured_package_name_unchanged"),Package->GetName().Equals(Before,ESearchCase::CaseSensitive)); }Saves.Add(MakeShared<FJsonValueObject>(Row));Row->SetBoolField(TEXT("captured_target_identity_after_save"),Guard());return Returned;
    }
    FString Finish(TSharedPtr<FJsonObject> Legacy)
    {
        if(!Legacy) { Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error.IsEmpty()?TEXT("No legacy result returned"):R.Error); }
        bool LegacySuccess=false;Legacy->TryGetBoolField(TEXT("success"),LegacySuccess);const TSharedPtr<FJsonObject>* Data=nullptr;FString Assertion;if(Legacy->TryGetObjectField(TEXT("data"),Data)&&Data&&Data->IsValid()) { (*Data)->TryGetStringField(TEXT("assertion_status"),Assertion); }
        TArray<TSharedPtr<FJsonValue>> Observations;
        for(auto& T:Tracked)
        {
            auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("role"),T.Role);V->SetStringField(TEXT("captured_path"),T.Path);V->SetObjectField(TEXT("before"),T.Before);
            const bool DetailsAdmitted=Guard();auto After=Snapshot(T.Weak.Get(),DetailsAdmitted);const bool Complete=DetailsAdmitted&&Guard();V->SetObjectField(TEXT("after"),After);V->SetBoolField(TEXT("captured_projection_comparison_complete"),Complete);
            bool RawChanged=false;for(const TCHAR* K:{TEXT("path"),TEXT("class"),TEXT("outer"),TEXT("package")}) { FString A,B;const bool HasA=T.Before->TryGetStringField(K,A),HasB=After->TryGetStringField(K,B);RawChanged|=HasA!=HasB||(HasA&&!A.Equals(B,ESearchCase::CaseSensitive)); }
            for(const TCHAR* K:{TEXT("live"),TEXT("package_dirty")}) { bool A=false,B=false;const bool HasA=T.Before->TryGetBoolField(K,A),HasB=After->TryGetBoolField(K,B);RawChanged|=HasA!=HasB||(HasA&&A!=B); }
            V->SetBoolField(TEXT("raw_identity_or_dirty_projection_changed"),RawChanged);R.bChanged|=RawChanged;
            if(Complete) { const bool Same=Json(T.Before).Equals(Json(After),ESearchCase::CaseSensitive);V->SetBoolField(TEXT("captured_projection_unchanged"),Same);R.bChanged|=!Same; }
            Observations.Add(MakeShared<FJsonValueObject>(V));
        }
        if(WorldCaptured) { Guard(); }if(!LegacySuccess&&R.Error.IsEmpty()) { FString E;Legacy->TryGetStringField(TEXT("error"),E);Fail(TEXT("operation_failed"),E.IsEmpty()?TEXT("Legacy operation failed"):E); }
        if(Assertion==TEXT("FAIL")&&R.Error.IsEmpty()) { Fail(TEXT("assertion_failed"),TEXT("Legacy operation returned data but its assertion_status is FAIL")); }
        R.bSuccess=LegacySuccess&&R.Error.IsEmpty();R.bPostconditionPassed=R.bSuccess;R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetArrayField(TEXT("actual_save_prefix"),Saves);R.Data->SetArrayField(TEXT("captured_object_observations"),Observations);R.Data->SetArrayField(TEXT("actual_mutation_prefix"),Events);R.Data->SetBoolField(TEXT("global_change_state_unknown"),!R.bChangeStateKnown);R.Data->SetNumberField(TEXT("file_bytes_read"),static_cast<double>(FileBytes));R.Data->SetNumberField(TEXT("file_read_calls"),FileReads);R.Data->SetBoolField(TEXT("SDK_readback_is_cold_reload"),false);
        if(R.bSuccess) { R.Stage=TEXT("complete"); }else if(!FailedStage.IsEmpty()) { R.Stage=FailedStage; }return Cubeless::Operations::EncodeVariableResult(R,*Operation);
    }
};
FWork* Work=nullptr;
bool Guard() { return !Work||Work->Guard(); }
void SDK(const TCHAR* S) { if(Work) { Work->SDK(S); } }
void Count(const TCHAR* S) { if(Work) { Work->Count(S); } }
void Write(const TCHAR* S) { if(Work) { Work->Write(S); } }
struct FLegacy
{
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& E) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),E);if(Work) { Work->Fail(TEXT("operation_failed"),E); }return J; }
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),true);if(Data) { J->SetObjectField(TEXT("data"),Data); }return J; }
};
bool Begin(FWork& W,const FString& ParamsJson)
{
    if(!Parse(ParamsJson,W.Params)) { return W.Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required")); }
    if(!IsInGameThread()||!GEditor||Running) { return W.Fail(TEXT("editor_required"),TEXT("Non-reentrant Editor game-thread execution required")); }W.World=CurrentWorld();W.WorldCaptured=true;return true;
}

constexpr TCHAR AuthoringRoot[]=TEXT("/Game/_MCP_Temp/ChaosDestructionLearning/Cinematic");

int32 CastI32(double V,const TCHAR* Field)
{
    if(!FMath::IsFinite(V)||V<=static_cast<double>(MIN_int32)-1.0||V>=static_cast<double>(MAX_int32)+1.0) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely convert to int32"),Field));return 0; }return static_cast<int32>(V);
}
float CastFloat(double V,const TCHAR* Field) { if(!FMath::IsFinite(V)||FMath::Abs(V)>static_cast<double>(TNumericLimits<float>::Max())) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely convert to finite float32"),Field));return 0.f; }return static_cast<float>(V); }
int32 EndAfter(int32 Start,int32 Requested,const TCHAR* Field)
{
    if(Start==MAX_int32) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s requires start+1 inside int32"),Field));return Start; }return FMath::Max(Start+1,Requested);
}
int32 Duration(int32 Start,int32 End)
{
    const int64 N=static_cast<int64>(End)-Start;if(N<1||N>MAX_int32) { Work->Fail(TEXT("numeric_range"),TEXT("Playback tick duration must be positive and fit int32"));return 0; }return static_cast<int32>(N);
}
bool NewOutputAdmission(const FString& ObjectOrPackagePath)
{
    const FString Package=FPackageName::ObjectPathToPackageName(ObjectOrPackagePath),Name=FPackageName::GetShortName(Package),Object=Package+TEXT(".")+Name;SDK(TEXT("new_sequence_collision_admission"));
    if(!FPackageName::IsValidLongPackageName(Package)||Name.IsEmpty()||Dependency(Package)) { return Work->Fail(TEXT("output_path_invalid"),TEXT("Valid runtime-safe new output package required")); }
    const bool Exists=FindObject<UObject>(nullptr,*Object)||FindPackage(nullptr,*Package)||FPackageName::DoesPackageExist(Package)||FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Object)).IsValid();
    if(!Guard()) { return false; }if(Exists) { return Work->Fail(TEXT("output_collision"),TEXT("New output already exists in memory/on disk/Registry; no allocation or overwrite is performed")); }return true;
}
bool ReadEvidenceFile(const FString& Path,TArray<uint8>& Bytes,int64 Limit=67108864)
{
    SDK(TEXT("read_evidence_file"));TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));const int64 Size=Reader?Reader->TotalSize():-1;
    if(Size<0||Size>Limit||Work->FileBytes>268435456-Size||Work->FileReads>=128) { Work->R.Data->SetBoolField(TEXT("evidence_file_budget_or_missing"),true);return false; }
    ++Work->FileReads;Count(TEXT("file_read_calls"));Bytes.SetNumUninitialized(static_cast<int32>(Size));Reader->Serialize(Bytes.GetData(),Size);const bool Complete=!Reader->IsError()&&Reader->Tell()==Size&&Reader->TotalSize()==Size;const bool Closed=Reader->Close();if(Complete&&Closed) { Work->FileBytes+=Size; }return Guard()&&Complete&&Closed;
}
bool CaptureCacheState(UChaosCacheCollection* Collection,const TCHAR* Role)
{
    if(!Collection) { return true; }if(!Work->Track(Collection,Role)) { return false; }const TArray<UChaosCache*> Copy=Collection->GetCaches();for(auto* Cache:Copy) { if(Cache&&!Work->Track(Cache,Role,Collection)) { return false; } }return Guard();
}

FString GetString(const TSharedPtr<FJsonObject>& Params, const TCHAR* Key, const FString& Default = FString())
{
    FString Value;
    return Params.IsValid() && Params->TryGetStringField(Key, Value) ? Value : Default;
}

double GetNumber(const TSharedPtr<FJsonObject>& Params, const TCHAR* Key, double Default)
{
    double Value = Default;
    return Params.IsValid() && Params->TryGetNumberField(Key, Value) ? Value : Default;
}

bool GetBool(const TSharedPtr<FJsonObject>& Params, const TCHAR* Key, bool Default)
{
    bool Value = Default;
    return Params.IsValid() && Params->TryGetBoolField(Key, Value) ? Value : Default;
}

bool IsAllowedOutputPath(const FString& Path)
{
    return Path.StartsWith(AuthoringRoot + FString(TEXT("/"))) && !Path.Contains(TEXT(".."));
}

TArray<FString> GetStringArray(const TSharedPtr<FJsonObject>& Params, const TCHAR* Key)
{
    TArray<FString> Result;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Params.IsValid() && Params->TryGetArrayField(Key, Values) && Values)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Item;
            if (Value.IsValid() && Value->TryGetString(Item))
            {
                Result.Add(Item);
            }
        }
    }
    return Result;
}

FFrameNumber DisplayFrameToTickFrame(int32 DisplayFrame, const FFrameRate& DisplayRate, const FFrameRate& TickResolution)
{
    const double X=static_cast<double>(DisplayFrame)*TickResolution.AsDecimal()/DisplayRate.AsDecimal(),Rounded=std::floor(X+0.5);if(!FMath::IsFinite(X)||Rounded<MIN_int32||Rounded>MAX_int32) { Work->Fail(TEXT("numeric_range"),TEXT("Display to tick frame conversion exceeds int32"));return FFrameNumber(0); }return FFrameRate::TransformTime(FFrameTime(DisplayFrame),DisplayRate,TickResolution).RoundToFrame();
}

TSharedPtr<FJsonObject> Success(const FString& Command)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("command"), Command);
    Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
    Data->SetStringField(TEXT("assertion_status"), TEXT("PASS"));
    Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
    return Data;
}

FString Sha256Bytes(const void* Data, uint32 ByteCount)
{
    // FPlatformMisc::GetSHA256Signature asserts on stock UE 5.6 Windows because
    // that platform hook has no implementation. Keep evidence hashing fully
    // deterministic and platform-independent inside the authoring plugin.
    static constexpr uint32 K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

    uint32 State[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    const auto RotateRight = [](uint32 Value, uint32 Count) -> uint32
    {
        return (Value >> Count) | (Value << (32u - Count));
    };
    const auto ProcessBlock = [&State, &RotateRight](const uint8* Block)
    {
        uint32 W[64];
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const int32 Offset = Index * 4;
            W[Index] = (static_cast<uint32>(Block[Offset]) << 24) |
                (static_cast<uint32>(Block[Offset + 1]) << 16) |
                (static_cast<uint32>(Block[Offset + 2]) << 8) |
                static_cast<uint32>(Block[Offset + 3]);
        }
        for (int32 Index = 16; Index < 64; ++Index)
        {
            const uint32 S0 = RotateRight(W[Index - 15], 7) ^ RotateRight(W[Index - 15], 18) ^ (W[Index - 15] >> 3);
            const uint32 S1 = RotateRight(W[Index - 2], 17) ^ RotateRight(W[Index - 2], 19) ^ (W[Index - 2] >> 10);
            W[Index] = W[Index - 16] + S0 + W[Index - 7] + S1;
        }

        uint32 A = State[0], B = State[1], C = State[2], D = State[3];
        uint32 E = State[4], F = State[5], G = State[6], H = State[7];
        for (int32 Index = 0; Index < 64; ++Index)
        {
            const uint32 S1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
            const uint32 Choice = (E & F) ^ ((~E) & G);
            const uint32 Temp1 = H + S1 + Choice + K[Index] + W[Index];
            const uint32 S0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
            const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
            const uint32 Temp2 = S0 + Majority;
            H = G; G = F; F = E; E = D + Temp1;
            D = C; C = B; B = A; A = Temp1 + Temp2;
        }
        State[0] += A; State[1] += B; State[2] += C; State[3] += D;
        State[4] += E; State[5] += F; State[6] += G; State[7] += H;
    };

    const uint8* Bytes = static_cast<const uint8*>(Data);
    uint32 Offset = 0;
    while (Offset + 64u <= ByteCount)
    {
        ProcessBlock(Bytes + Offset);
        Offset += 64u;
    }

    uint8 Tail[128] = {};
    const uint32 Remaining = ByteCount - Offset;
    if (Remaining > 0u && Bytes)
    {
        FMemory::Memcpy(Tail, Bytes + Offset, Remaining);
    }
    Tail[Remaining] = 0x80u;
    const uint32 TailBytes = Remaining < 56u ? 64u : 128u;
    const uint64 BitCount = static_cast<uint64>(ByteCount) * 8ull;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        Tail[TailBytes - 1u - Index] = static_cast<uint8>(BitCount >> (Index * 8));
    }
    ProcessBlock(Tail);
    if (TailBytes == 128u)
    {
        ProcessBlock(Tail + 64);
    }

    FString Result;
    Result.Reserve(64);
    for (const uint32 Word : State)
    {
        Result += FString::Printf(TEXT("%08x"), Word);
    }
    return Result;
}

TSharedPtr<FJsonObject> CommandError(const FString& Message)
{
    return FLegacy::CreateErrorResponse(Message);
}

bool SaveAsset(UObject* Asset) { return Asset&&Work->Save(Asset,Asset->GetPathName(),TEXT("save_sequence"),true); }

struct FCollectionIntegrityMetrics
{
    int32 InvalidParentCount = 0;
    int32 CycleCount = 0;
    int32 ParentChildReciprocityErrors = 0;
    int32 InvalidTriangleCount = 0;
    int32 NonManifoldEdgeCount = 0;
    int32 InvalidTransformToGeometryCount = 0;
    int32 InvalidGeometryRangeCount = 0;
};

bool CollectionReadable(const FGeometryCollection& C)
{
    const int32 T=C.NumElements(FGeometryCollection::TransformGroup),G=C.NumElements(FGeometryCollection::GeometryGroup),F=C.NumElements(FGeometryCollection::FacesGroup);
    return T>=0&&G>=0&&F>=0&&C.Parent.Num()==T&&C.Children.Num()==T&&C.SimulationType.Num()==T&&C.TransformToGeometryIndex.Num()==T&&C.TransformIndex.Num()==G&&C.VertexStart.Num()==G&&C.VertexCount.Num()==G&&C.FaceStart.Num()==G&&C.FaceCount.Num()==G&&C.Indices.Num()==F;
}

FCollectionIntegrityMetrics AnalyzeCollectionIntegrity(const FGeometryCollection& Collection)
{
    FCollectionIntegrityMetrics Metrics;
    if(!CollectionReadable(Collection)) { Metrics.InvalidParentCount=1;Work->Fail(TEXT("collection_schema_unreadable"),TEXT("Collection group arrays have inconsistent lengths; unsafe topology indexing was skipped"));return Metrics; }
    const int32 TransformCount = Collection.NumElements(FGeometryCollection::TransformGroup);
    for (int32 Index = 0; Index < TransformCount; ++Index)
    {
        const int32 Parent = Collection.Parent[Index];
        if (Parent != INDEX_NONE && !Collection.Parent.IsValidIndex(Parent))
        {
            ++Metrics.InvalidParentCount;
        }
        else if (Parent != INDEX_NONE && !Collection.Children[Parent].Contains(Index))
        {
            ++Metrics.ParentChildReciprocityErrors;
        }
        TSet<int32> Visited;
        for (int32 Cursor = Index; Cursor != INDEX_NONE && Collection.Parent.IsValidIndex(Cursor); Cursor = Collection.Parent[Cursor])
        {
            if (Visited.Contains(Cursor))
            {
                ++Metrics.CycleCount;
                break;
            }
            Visited.Add(Cursor);
        }
        for (int32 Child : Collection.Children[Index])
        {
            if (!Collection.Parent.IsValidIndex(Child) || Collection.Parent[Child] != Index)
            {
                ++Metrics.ParentChildReciprocityErrors;
            }
        }
    }
    const int32 VertexCount = Collection.NumElements(FGeometryCollection::VerticesGroup);
    const int32 GeometryCount = Collection.NumElements(FGeometryCollection::GeometryGroup);
    const int32 FaceCount = Collection.NumElements(FGeometryCollection::FacesGroup);
    for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
    {
        const int32 GeometryIndex = Collection.TransformToGeometryIndex[TransformIndex];
        if (GeometryIndex != INDEX_NONE && (!Collection.TransformIndex.IsValidIndex(GeometryIndex) ||
            Collection.TransformIndex[GeometryIndex] != TransformIndex))
        {
            ++Metrics.InvalidTransformToGeometryCount;
        }
    }
    for (int32 GeometryIndex = 0; GeometryIndex < GeometryCount; ++GeometryIndex)
    {
        const bool bVertexRangeValid = Collection.VertexStart[GeometryIndex] >= 0 &&
            Collection.VertexCount[GeometryIndex] >= 0 &&
            static_cast<int64>(Collection.VertexStart[GeometryIndex]) + Collection.VertexCount[GeometryIndex] <= VertexCount;
        const bool bFaceRangeValid = Collection.FaceStart[GeometryIndex] >= 0 &&
            Collection.FaceCount[GeometryIndex] >= 0 &&
            static_cast<int64>(Collection.FaceStart[GeometryIndex]) + Collection.FaceCount[GeometryIndex] <= FaceCount;
        const int32 TransformIndex = Collection.TransformIndex[GeometryIndex];
        const bool bReverseMappingValid = Collection.TransformToGeometryIndex.IsValidIndex(TransformIndex) &&
            Collection.TransformToGeometryIndex[TransformIndex] == GeometryIndex;
        Metrics.InvalidGeometryRangeCount += bVertexRangeValid && bFaceRangeValid && bReverseMappingValid ? 0 : 1;
    }
    TMap<uint64, int32> EdgeUseCounts;
    auto AddEdge = [&EdgeUseCounts](int32 A, int32 B)
    {
        const uint32 Low = static_cast<uint32>(FMath::Min(A, B));
        const uint32 High = static_cast<uint32>(FMath::Max(A, B));
        ++EdgeUseCounts.FindOrAdd((static_cast<uint64>(Low) << 32) | High);
    };
    for (const FIntVector& Triangle : Collection.Indices)
    {
        if (Triangle.X < 0 || Triangle.Y < 0 || Triangle.Z < 0 ||
            Triangle.X >= VertexCount || Triangle.Y >= VertexCount || Triangle.Z >= VertexCount ||
            Triangle.X == Triangle.Y || Triangle.Y == Triangle.Z || Triangle.Z == Triangle.X)
        {
            ++Metrics.InvalidTriangleCount;
            continue;
        }
        AddEdge(Triangle.X, Triangle.Y);
        AddEdge(Triangle.Y, Triangle.Z);
        AddEdge(Triangle.Z, Triangle.X);
    }
    for (const TPair<uint64, int32>& Pair : EdgeUseCounts)
    {
        Metrics.NonManifoldEdgeCount += Pair.Value > 2 ? 1 : 0;
    }
    return Metrics;
}

void AddCollectionMetrics(const TSharedPtr<FJsonObject>& Data, const UGeometryCollection* Asset)
{
    int32 RigidCount = 0;
    int32 ClusterCount = 0;
    int32 RootCount = 0;
    int32 MaxDepth = 0;
    int32 MinimumLeafDepth = TNumericLimits<int32>::Max();
    int32 MaximumLeafDepth = 0;
    TMap<int32, int32> LevelCounts;
    FCollectionIntegrityMetrics Integrity;
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection =
        Asset ? Asset->GetGeometryCollection() : nullptr;
    const bool Readable=Collection.IsValid()&&CollectionReadable(*Collection);Data->SetBoolField(TEXT("collection_schema_readable"),Readable);Work->Checks->SetBoolField(TEXT("collection_schema_readable"),Readable);
    if(Collection.IsValid()&&!Readable) { Integrity.InvalidParentCount=1;Work->Fail(TEXT("collection_schema_unreadable"),TEXT("Collection arrays are not aligned with group counts; unsafe metrics indexing was skipped")); }
    if (Readable)
    {
        Integrity = AnalyzeCollectionIntegrity(*Collection);
        for (int32 Index = 0; Index < Collection->NumElements(FGeometryCollection::TransformGroup); ++Index)
        {
            RigidCount += Collection->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid ? 1 : 0;
            ClusterCount += Collection->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Clustered ? 1 : 0;
            RootCount += Collection->Parent[Index] == INDEX_NONE ? 1 : 0;
            int32 Depth = 0;
            for (int32 Parent = Collection->Parent[Index]; Parent != INDEX_NONE && Collection->Parent.IsValidIndex(Parent); Parent = Collection->Parent[Parent])
            {
                if (++Depth > Collection->NumElements(FGeometryCollection::TransformGroup))
                {
                    break;
                }
            }
            MaxDepth = FMath::Max(MaxDepth, Depth);
            ++LevelCounts.FindOrAdd(Depth);
            if (Collection->Children[Index].IsEmpty())
            {
                MinimumLeafDepth = FMath::Min(MinimumLeafDepth, Depth);
                MaximumLeafDepth = FMath::Max(MaximumLeafDepth, Depth);
            }
        }
        Data->SetNumberField(TEXT("transform_count"), Collection->NumElements(FGeometryCollection::TransformGroup));
        Data->SetNumberField(TEXT("geometry_count"), Collection->NumElements(FGeometryCollection::GeometryGroup));
    }
    Data->SetNumberField(TEXT("rigid_transform_count"), RigidCount);
    Data->SetNumberField(TEXT("cluster_count"), ClusterCount);
    Data->SetNumberField(TEXT("root_count"), RootCount);
    Data->SetNumberField(TEXT("hierarchy_depth"), MaxDepth);
    Data->SetNumberField(TEXT("minimum_leaf_depth"), MinimumLeafDepth == TNumericLimits<int32>::Max() ? 0 : MinimumLeafDepth);
    Data->SetNumberField(TEXT("maximum_leaf_depth"), MaximumLeafDepth);
    Data->SetBoolField(TEXT("leaf_depth_consistent"), MinimumLeafDepth == MaximumLeafDepth);
    Data->SetNumberField(TEXT("invalid_parent_count"), Integrity.InvalidParentCount);
    Data->SetNumberField(TEXT("cycle_count"), Integrity.CycleCount);
    Data->SetNumberField(TEXT("parent_child_reciprocity_errors"), Integrity.ParentChildReciprocityErrors);
    Data->SetNumberField(TEXT("invalid_triangle_count"), Integrity.InvalidTriangleCount);
    Data->SetNumberField(TEXT("non_manifold_edge_count"), Integrity.NonManifoldEdgeCount);
    Data->SetNumberField(TEXT("invalid_transform_to_geometry_count"), Integrity.InvalidTransformToGeometryCount);
    Data->SetNumberField(TEXT("invalid_geometry_range_count"), Integrity.InvalidGeometryRangeCount);
    Data->SetBoolField(TEXT("collection_integrity_gate"), Integrity.InvalidParentCount == 0 &&
        Integrity.CycleCount == 0 && Integrity.ParentChildReciprocityErrors == 0 &&
        Integrity.InvalidTriangleCount == 0 && Integrity.NonManifoldEdgeCount == 0 &&
        Integrity.InvalidTransformToGeometryCount == 0 && Integrity.InvalidGeometryRangeCount == 0);
    TSharedPtr<FJsonObject> LevelCountsJson = MakeShared<FJsonObject>();
    for (const TPair<int32, int32>& Pair : LevelCounts)
        LevelCountsJson->SetNumberField(FString::FromInt(Pair.Key), Pair.Value);
    Data->SetObjectField(TEXT("level_counts"), LevelCountsJson);
}

UWorld* GetActivePlayWorldForChaosCinematics()
{
    if (!GEditor)
    {
        return nullptr;
    }

    FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
    return PIEWorldContext ? PIEWorldContext->World() : nullptr;
}

UWorld* ResolveEditorWorld()
{
    if (!GEditor)
    {
        return nullptr;
    }
    if (UWorld* PlayWorld = GetActivePlayWorldForChaosCinematics())
    {
        return PlayWorld;
    }
    return GEditor->GetEditorWorldContext().World();
}

template <typename T>
T* FindActor(UWorld* World, const FString& Name)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<T> It(World); It; ++It)
    {
        if (It->GetName() == Name || It->GetActorLabel() == Name || It->GetPathName() == Name)
        {
            SDK(TEXT("resolve_actor"));return Work->Track(*It,TEXT("selected_actor"))?*It:nullptr;
        }
    }
    return nullptr;
}

UGeometryCollectionComponent* ResolveGeometryComponent(AActor* Actor)
{
    if (AGeometryCollectionActor* GeometryActor = Cast<AGeometryCollectionActor>(Actor))
    {
        if (UGeometryCollectionComponent* Component =
            GeometryActor->GetGeometryCollectionComponent())
        {
            return Component;
        }
    }
    return Actor ? Actor->FindComponentByClass<UGeometryCollectionComponent>() : nullptr;
}

TArray<UGeometryCollectionComponent*> ResolveGeometryComponents(
    UWorld* World,
    const TArray<FString>& ActorNames)
{
    TArray<UGeometryCollectionComponent*> Components;
    for (const FString& ActorName : ActorNames)
    {
        if (AActor* Actor = FindActor<AActor>(World, ActorName))
        {
            if (UGeometryCollectionComponent* Component = ResolveGeometryComponent(Actor))
            {
                if(!Work->Track(Component,TEXT("requested_geometry_component"),Actor)) { return {}; }Components.Add(Component);
            }
        }
    }
    return Components;
}

TSharedPtr<FJsonObject> HandleCreateSequence(const TSharedPtr<FJsonObject>& Params)
{
    const FString OutputPath=GetString(Params,TEXT("output_sequence_path"));
    if(!IsAllowedOutputPath(OutputPath)) { return CommandError(TEXT("output_sequence_path must be under /Game/_MCP_Temp/ChaosDestructionLearning/Cinematic")); }
    if(!NewOutputAdmission(OutputPath)) { return CommandError(Work->R.Error); }
    const int32 DisplayRate=FMath::Max(1,CastI32(GetNumber(Params,TEXT("display_rate"),30),TEXT("display_rate")));
    const int32 TickResolution=FMath::Max(DisplayRate,CastI32(GetNumber(Params,TEXT("tick_resolution"),24000),TEXT("tick_resolution")));
    const int32 StartFrame=CastI32(GetNumber(Params,TEXT("start_frame"),0),TEXT("start_frame")),EndFrame=EndAfter(StartFrame,CastI32(GetNumber(Params,TEXT("end_frame"),60),TEXT("end_frame")),TEXT("end_frame"));
    const FFrameNumber TickStart=DisplayFrameToTickFrame(StartFrame,FFrameRate(DisplayRate,1),FFrameRate(TickResolution,1)),TickEnd=DisplayFrameToTickFrame(EndFrame,FFrameRate(DisplayRate,1),FFrameRate(TickResolution,1));const int32 TickDuration=Duration(TickStart.Value,TickEnd.Value);
    if(!Work->R.Error.IsEmpty()||!Guard()||!NewOutputAdmission(OutputPath)) { return CommandError(Work->R.Error); }
    const FString SequencePackageName=FPackageName::ObjectPathToPackageName(OutputPath);Write(TEXT("create_sequence_package"));UPackage* Package=CreatePackage(*SequencePackageName);
    if(!Work->Track(Package,TEXT("new_sequence_package"),nullptr,true)) { return CommandError(Work->R.Error); }
    Write(TEXT("new_sequence"));Count(TEXT("new_sequence_calls"));ULevelSequence* Sequence=NewObject<ULevelSequence>(Package,*FPackageName::GetShortName(SequencePackageName),RF_Public|RF_Standalone|RF_Transactional);
    if(!Sequence) { return CommandError(TEXT("Failed to allocate Level Sequence")); }
    if(!Work->Track(Sequence,TEXT("new_sequence"),Package,true)) { return CommandError(Work->R.Error); }
    Write(TEXT("initialize_sequence"));Count(TEXT("initialize_calls"));Sequence->Initialize();if(!Guard()) { return CommandError(Work->R.Error); }
    SDK(TEXT("resolve_movie_scene"));UMovieScene* MovieScene=Sequence->GetMovieScene();if(!Work->Track(MovieScene,TEXT("new_movie_scene"),Sequence)) { return CommandError(Work->R.Error); }
    Write(TEXT("set_display_rate"));MovieScene->SetDisplayRate(FFrameRate(DisplayRate,1));if(!Guard()) { return CommandError(Work->R.Error); }
    Write(TEXT("set_tick_resolution"));MovieScene->SetTickResolutionDirectly(FFrameRate(TickResolution,1));if(!Guard()) { return CommandError(Work->R.Error); }
    Write(TEXT("set_playback_range"));MovieScene->SetPlaybackRange(TickStart.Value,TickDuration);if(!Guard()) { return CommandError(Work->R.Error); }
    int32 CameraCutCount=0;FString CameraBindingGuid;const FString CameraActorName=GetString(Params,TEXT("camera_actor_name"));
    if(!CameraActorName.IsEmpty())
    {
        UWorld* World=ResolveEditorWorld();AActor* CameraActor=FindActor<AActor>(World,CameraActorName);
        if(!CameraActor) { return CommandError(TEXT("camera_actor_name did not resolve in the current world")); }
        if(!Guard()) { return CommandError(Work->R.Error); }Write(TEXT("add_camera_possessable"));Count(TEXT("add_possessable_calls"));const FGuid CameraGuid=MovieScene->AddPossessable(CameraActor->GetActorLabel(),CameraActor->GetClass());
        if(!Guard()||!CameraGuid.IsValid()) { return CommandError(TEXT("Camera possessable allocation failed or context changed")); }
        Write(TEXT("bind_camera_possessable"));Count(TEXT("bind_possessable_calls"));Sequence->BindPossessableObject(CameraGuid,*CameraActor,World);if(!Guard()) { return CommandError(Work->R.Error); }
        SDK(TEXT("resolve_camera_track"));auto* CameraTrack=Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack());
        if(!CameraTrack) { Write(TEXT("add_camera_track"));Count(TEXT("add_track_calls"));CameraTrack=Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass())); }
        if(!CameraTrack) { return CommandError(TEXT("Failed to create Camera Cut track")); }
        if(!Work->Track(CameraTrack,TEXT("camera_track"),MovieScene)) { return CommandError(Work->R.Error); }
        Write(TEXT("add_camera_cut"));Count(TEXT("add_section_calls"));auto* CameraSection=CameraTrack->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(CameraGuid),TickStart);
        if(CameraSection)
        {
            if(!Work->Track(CameraSection,TEXT("camera_section"),CameraTrack)) { return CommandError(Work->R.Error); }
            Write(TEXT("set_camera_cut_range"));CameraSection->SetRange(TRange<FFrameNumber>(TickStart,TickEnd));if(!Guard()) { return CommandError(Work->R.Error); }
            SDK(TEXT("read_camera_cut_count"));CameraCutCount=CameraTrack->GetAllSections().Num();if(!Guard()) { return CommandError(Work->R.Error); }CameraBindingGuid=CameraGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
        }
    }
    const bool TimingMatches=MovieScene->GetDisplayRate()==FFrameRate(DisplayRate,1)&&MovieScene->GetTickResolution()==FFrameRate(TickResolution,1)&&MovieScene->GetPlaybackRange()==TRange<FFrameNumber>(TickStart,TickEnd);Work->Checks->SetBoolField(TEXT("created_sequence_timing_matches"),TimingMatches);
    if(!TimingMatches||!Guard()) { return CommandError(TEXT("Created sequence timing differs from the admitted request")); }
    Write(TEXT("register_new_sequence"));FAssetRegistryModule::AssetCreated(Sequence);if(!Guard()) { return CommandError(Work->R.Error); }
    const bool bSaved=SaveAsset(Sequence);if(!Guard()) { return CommandError(Work->R.Error); }
    ULevelSequence* ReloadedSequence=Work->Load<ULevelSequence>(OutputPath,TEXT("same_session_sequence_requery"));if(!Guard()) { return CommandError(Work->R.Error); }
    SDK(TEXT("requery_movie_scene"));auto* ReloadedScene=ReloadedSequence?ReloadedSequence->GetMovieScene():nullptr;if(!Guard()||(ReloadedScene&&!Work->Track(ReloadedScene,TEXT("same_session_movie_scene"),ReloadedSequence))) { return CommandError(Work->R.Error); }
    auto* ReloadedCameraTrack=ReloadedScene?Cast<UMovieSceneCameraCutTrack>(ReloadedScene->GetCameraCutTrack()):nullptr;if(ReloadedCameraTrack&&!Work->Track(ReloadedCameraTrack,TEXT("same_session_camera_track"),ReloadedScene)) { return CommandError(Work->R.Error); }
    const int32 ReloadedCameraCutCount=ReloadedCameraTrack?ReloadedCameraTrack->GetAllSections().Num():0;if(!Guard()) { return CommandError(Work->R.Error); }
    TSharedPtr<FJsonObject> Data = Success(TEXT("create_chaos_level_sequence"));
    Data->SetStringField(TEXT("sequence_path"), Sequence->GetPathName());
    Data->SetNumberField(TEXT("display_rate"), DisplayRate);
    Data->SetNumberField(TEXT("tick_resolution"), TickResolution);
    Data->SetNumberField(TEXT("playback_start_frame"), StartFrame);
    Data->SetNumberField(TEXT("playback_end_frame"), EndFrame);
    Data->SetStringField(TEXT("camera_binding_guid"), CameraBindingGuid);
    Data->SetNumberField(TEXT("camera_cut_section_count"), CameraCutCount);
    Data->SetBoolField(TEXT("camera_cut_gate"), CameraActorName.IsEmpty() || CameraCutCount == 1);
    Data->SetNumberField(TEXT("reloaded_camera_cut_section_count"), ReloadedCameraCutCount);
    Data->SetBoolField(TEXT("camera_cut_reload_gate"), CameraActorName.IsEmpty() || ReloadedCameraCutCount == CameraCutCount);
    Data->SetBoolField(TEXT("saved"), bSaved);
    Data->SetStringField(TEXT("assertion_status"), bSaved &&
        (CameraActorName.IsEmpty() || (CameraCutCount == 1 && ReloadedCameraCutCount == 1))
            ? TEXT("PASS") : TEXT("FAIL"));
    return FLegacy::CreateSuccessResponse(Data);
}

TArray<UMovieSceneChaosCacheSection*> FindChaosSections(UMovieScene* MovieScene)
{
    TArray<UMovieSceneChaosCacheSection*> Result;if(!MovieScene) { return Result; }
    TArray<UMovieSceneChaosCacheTrack*> Tracks;for(const FMovieSceneBinding& Binding:MovieScene->GetBindings()) { for(UMovieSceneTrack* Base:Binding.GetTracks()) { if(auto* Track=Cast<UMovieSceneChaosCacheTrack>(Base)) { Tracks.Add(Track); } } }
    for(auto* Track:Tracks)
    {
        if(!Work->Track(Track,TEXT("enumerated_chaos_track"),MovieScene)) { return {}; }SDK(TEXT("enumerate_chaos_sections"));const TArray<UMovieSceneSection*> Sections=Track->GetAllSections();if(!Guard()) { return {}; }
        for(auto* Base:Sections) { if(auto* Section=Cast<UMovieSceneChaosCacheSection>(Base)) { if(!Work->Track(Section,TEXT("enumerated_chaos_section"),Track)) { return {}; }Result.Add(Section); } }
    }return Result;
}
TSharedPtr<FJsonObject> HandleBindCache(const TSharedPtr<FJsonObject>& Params)
{
    ULevelSequence* Sequence=Work->Load<ULevelSequence>(GetString(Params,TEXT("sequence_path")),TEXT("load_sequence"));
    const FString CacheCollectionPath=GetString(Params,TEXT("cache_collection_path"));UChaosCacheCollection* CacheCollection=Work->Load<UChaosCacheCollection>(CacheCollectionPath,TEXT("load_requested_cache"));
    UWorld* World=ResolveEditorWorld();AChaosCacheManager* Manager=FindActor<AChaosCacheManager>(World,GetString(Params,TEXT("cache_manager_actor_name")));
    if(!Sequence||!Manager||!CacheCollection||!IsAllowedOutputPath(CacheCollectionPath)) { return CommandError(TEXT("Level Sequence, Cache Manager actor, and an existing Cinematic-root cache_collection_path are required")); }
    SDK(TEXT("resolve_movie_scene"));UMovieScene* MovieScene=Sequence->GetMovieScene();if(!Work->Track(MovieScene,TEXT("bound_movie_scene"),Sequence)||!Work->Track(World,TEXT("current_saved_world"))) { return CommandError(Work->R.Error); }
    if(!CaptureCacheState(CacheCollection,TEXT("requested_cache_collection"))||!CaptureCacheState(Manager->CacheCollection,TEXT("previous_manager_cache_collection"))) { return CommandError(Work->R.Error); }
    TArray<UGeometryCollectionComponent*> PlaybackComponents=ResolveGeometryComponents(World,GetStringArray(Params,TEXT("geometry_collection_actor_names")));if(!Guard()) { return CommandError(Work->R.Error); }
    if(PlaybackComponents.IsEmpty()) { for(TActorIterator<AActor> It(World);It;++It) { if(auto* C=It->FindComponentByClass<UGeometryCollectionComponent>()) { if(!Work->Track(C,TEXT("derived_geometry_component"),*It)) { return CommandError(Work->R.Error); }PlaybackComponents.Add(C); } } }
    PlaybackComponents.Sort([](const UGeometryCollectionComponent& A,const UGeometryCollectionComponent& B){return A.GetPathName()<B.GetPathName();});
    TArray<FString> PlaybackCacheNames=GetStringArray(Params,TEXT("cache_names"));if(PlaybackCacheNames.IsEmpty()) { for(const UChaosCache* Cache:CacheCollection->GetCaches()) { if(Cache) { PlaybackCacheNames.Add(Cache->GetName()); } } }
    if(PlaybackComponents.IsEmpty()||PlaybackComponents.Num()!=PlaybackCacheNames.Num()) { return CommandError(TEXT("bind_chaos_cache_to_sequence requires matching Geometry Collection components and cache_names; automatic derivation requires an unambiguous one-to-one level/cache layout")); }
    for(const FString& Name:PlaybackCacheNames) { SDK(TEXT("validate_requested_cache_name"));if(!CacheCollection->FindCache(FName(*Name))) { return CommandError(FString::Printf(TEXT("Cache Collection does not contain requested cache_name: %s"),*Name)); }if(!Guard()) { return CommandError(Work->R.Error); } }
    const TArray<FObservedComponent> PreviousObserved=Manager->GetObservedComponents();for(const auto& O:PreviousObserved) { SDK(TEXT("capture_previous_observed_component"));auto* C=O.GetComponent(Manager);if(!Guard()||(C&&!Work->Track(C,TEXT("previous_observed_component"),C->GetOwner()))) { return CommandError(Work->R.Error); } }
    const auto PreviousSections=FindChaosSections(MovieScene);if(!Work->R.Error.IsEmpty()||!Guard()) { return CommandError(Work->R.Error); }
    const int32 StartFrame=CastI32(GetNumber(Params,TEXT("section_start_frame"),0),TEXT("section_start_frame")),EndFrame=EndAfter(StartFrame,CastI32(GetNumber(Params,TEXT("section_end_frame"),60),TEXT("section_end_frame")),TEXT("section_end_frame"));
    const FFrameNumber TickStart=DisplayFrameToTickFrame(StartFrame,MovieScene->GetDisplayRate(),MovieScene->GetTickResolution()),TickEnd=DisplayFrameToTickFrame(EndFrame,MovieScene->GetDisplayRate(),MovieScene->GetTickResolution());
    const float DesiredPlayRate=CastFloat(GetNumber(Params,TEXT("cache_play_rate"),1.0),TEXT("cache_play_rate"));const FFrameNumber DesiredStartOffset(FMath::Max(0,CastI32(GetNumber(Params,TEXT("cache_start_offset_seconds"),0.0)*MovieScene->GetTickResolution().AsDecimal(),TEXT("cache_start_offset_seconds_ticks"))));
    if(!Work->R.Error.IsEmpty()||!Guard()) { return CommandError(Work->R.Error); }
    if(!Work->Modify(Sequence,TEXT("modify_sequence"))||!Work->Modify(MovieScene,TEXT("modify_movie_scene"))||!Work->Modify(Manager,TEXT("modify_manager"))) { return CommandError(Work->R.Error); }
    Write(TEXT("EndEvaluate_previous_manager"));Count(TEXT("EndEvaluate_calls"));Manager->EndEvaluate();if(!Guard()) { return CommandError(Work->R.Error); }
    Work->R.Data->SetBoolField(TEXT("EndEvaluate_returned"),true);Work->R.Data->SetBoolField(TEXT("previous_cache_record_flush_effect_unknown"),true);Write(TEXT("set_manager_collection"));Manager->CacheCollection=CacheCollection;
    for(auto* C:PlaybackComponents) { if(!Work->Modify(C,TEXT("modify_playback_component"))) { return CommandError(Work->R.Error); }Write(TEXT("suppress_component_simulation"));C->ObjectType=EObjectStateTypeEnum::Chaos_Object_Kinematic;Count(TEXT("SetSimulatePhysics_calls"));C->SetSimulatePhysics(false);if(!Guard()) { return CommandError(Work->R.Error); } }
    bool bReusedObservedBindings=Manager->GetObservedComponents().Num()==PlaybackComponents.Num();
    if(bReusedObservedBindings) { const TArray<FObservedComponent> Copy=Manager->GetObservedComponents();for(int32 I=0;I<Copy.Num();++I) { SDK(TEXT("compare_observed_bindings"));const auto& O=Copy[I];auto* C=O.GetComponent(Manager);if(!Guard()) { return CommandError(Work->R.Error); }bReusedObservedBindings&=C==PlaybackComponents[I]&&O.CacheName==FName(*PlaybackCacheNames[I])&&O.bPlaybackEnabled; } }
    if(!bReusedObservedBindings)
    {
        Write(TEXT("clear_observed_without_destroy"));Count(TEXT("ClearObservedComponents_false_calls"));Manager->ClearObservedComponents(false);if(!Guard()) { return CommandError(Work->R.Error); }
        for(int32 I=0;I<PlaybackComponents.Num();++I) { Write(TEXT("add_observed_binding"));Count(TEXT("FindOrAddObservedComponent_calls"));Manager->FindOrAddObservedComponent(PlaybackComponents[I],FName(*PlaybackCacheNames[I]),false);if(!Guard()) { return CommandError(Work->R.Error); } }
        for(auto* C:PlaybackComponents) { Write(TEXT("reapply_sim_suppression_after_notifiers"));C->ObjectType=EObjectStateTypeEnum::Chaos_Object_Kinematic;Count(TEXT("SetSimulatePhysics_calls"));C->SetSimulatePhysics(false);if(!Guard()) { return CommandError(Work->R.Error); } }
    }
    Write(TEXT("enable_observed_playback"));for(auto& O:Manager->GetObservedComponents()) { O.bPlaybackEnabled=true; }Manager->CacheMode=ECacheMode::None;Manager->StartMode=EStartMode::Timed;
    SDK(TEXT("find_manager_binding"));FGuid BindingGuid=Sequence->FindBindingFromObject(Manager,World);if(!Guard()) { return CommandError(Work->R.Error); }const bool bReusedBinding=BindingGuid.IsValid();
    if(!BindingGuid.IsValid()) { Write(TEXT("add_manager_possessable"));Count(TEXT("add_possessable_calls"));BindingGuid=MovieScene->AddPossessable(Manager->GetActorLabel(),Manager->GetClass());if(!Guard()||!BindingGuid.IsValid()) { return CommandError(TEXT("Manager possessable allocation failed or context changed")); }Write(TEXT("bind_manager_possessable"));Count(TEXT("bind_possessable_calls"));Sequence->BindPossessableObject(BindingGuid,*Manager,World);if(!Guard()) { return CommandError(Work->R.Error); } }
    UMovieSceneChaosCacheTrack* Track=nullptr;if(const auto* B=MovieScene->FindBinding(BindingGuid)) { for(auto* Base:B->GetTracks()) { if(auto* C=Cast<UMovieSceneChaosCacheTrack>(Base)) { Track=C;break; } } }const bool bReusedTrack=Track!=nullptr;
    if(!Track) { Write(TEXT("add_chaos_cache_track"));Count(TEXT("add_track_calls"));Track=MovieScene->AddTrack<UMovieSceneChaosCacheTrack>(BindingGuid); }
    if(!Track) { return CommandError(TEXT("Failed to add Chaos Cache track")); }if(!Work->Track(Track,TEXT("bound_chaos_track"),MovieScene,!bReusedTrack)) { return CommandError(Work->R.Error); }
    UMovieSceneChaosCacheSection* Section=nullptr;SDK(TEXT("find_matching_chaos_section"));const TArray<UMovieSceneSection*> ExistingSections=Track->GetAllSections();if(!Guard()) { return CommandError(Work->R.Error); }
    for(auto* Base:ExistingSections) { if(auto* Existing=Cast<UMovieSceneChaosCacheSection>(Base)) { if(!Work->Track(Existing,TEXT("candidate_chaos_section"),Track)) { return CommandError(Work->R.Error); }if(Existing->GetRange()==TRange<FFrameNumber>(TickStart,TickEnd)&&FMath::IsNearlyEqual(Existing->Params.PlayRate,DesiredPlayRate)&&Existing->Params.StartFrameOffset==DesiredStartOffset&&Existing->Params.CacheCollection==CacheCollection) { Section=Existing;break; } } }
    const bool bReusedSection=Section!=nullptr;if(!Section) { Write(TEXT("add_chaos_cache_section"));Count(TEXT("add_section_calls"));Section=Cast<UMovieSceneChaosCacheSection>(Track->AddNewAnimation(TickStart,Manager)); }
    if(!Section) { return CommandError(TEXT("Failed to add Chaos Cache section")); }if(!Work->Track(Section,TEXT("bound_chaos_section"),Track,!bReusedSection)) { return CommandError(Work->R.Error); }
    Write(TEXT("set_chaos_section_range"));Section->SetRange(TRange<FFrameNumber>(TickStart,TickEnd));if(!Guard()) { return CommandError(Work->R.Error); }Write(TEXT("set_chaos_section_params"));Section->Params.PlayRate=DesiredPlayRate;Section->Params.StartFrameOffset=DesiredStartOffset;Section->Params.CacheCollection=CacheCollection;
    const bool bSaved=SaveAsset(Sequence);if(!Guard()) { return CommandError(Work->R.Error); }const bool bLevelSaved=Work->Save(World,World->GetOutermost()->GetName(),TEXT("save_current_world"),false);if(!Guard()) { return CommandError(Work->R.Error); }
    SDK(TEXT("read_bound_sequence"));
    int32 ExactSectionMatchCount=0,ChaosTrackCount=0;TArray<UMovieSceneTrack*> ReadbackTracks;if(const auto* B=MovieScene->FindBinding(BindingGuid)) { ReadbackTracks=B->GetTracks(); }
    for(auto* Base:ReadbackTracks)
    {
        if(auto* ReadbackTrack=Cast<UMovieSceneChaosCacheTrack>(Base))
        {
            if(!Work->Track(ReadbackTrack,TEXT("readback_chaos_track"),MovieScene)) { return CommandError(Work->R.Error); }++ChaosTrackCount;SDK(TEXT("readback_chaos_sections"));const TArray<UMovieSceneSection*> Copy=ReadbackTrack->GetAllSections();if(!Guard()) { return CommandError(Work->R.Error); }
            for(auto* B:Copy) { if(auto* S=Cast<UMovieSceneChaosCacheSection>(B)) { if(!Work->Track(S,TEXT("readback_chaos_section"),ReadbackTrack)) { return CommandError(Work->R.Error); }ExactSectionMatchCount+=S->GetRange()==TRange<FFrameNumber>(TickStart,TickEnd)&&FMath::IsNearlyEqual(S->Params.PlayRate,DesiredPlayRate)&&S->Params.StartFrameOffset==DesiredStartOffset&&S->Params.CacheCollection==CacheCollection?1:0; } }
        }
    }
    const bool bExactCacheReadback = Manager->CacheCollection == CacheCollection && ExactSectionMatchCount == 1;
    bool bPersistedPlaybackSimulationSuppressed = true;
    for (const UGeometryCollectionComponent* PlaybackComponent : PlaybackComponents)
    {
        SDK(TEXT("read_sim_suppression"));const bool Simulating=PlaybackComponent&&PlaybackComponent->IsSimulatingPhysics();if(!Guard()) { return CommandError(Work->R.Error); }bPersistedPlaybackSimulationSuppressed&=PlaybackComponent&&!Simulating&&PlaybackComponent->ObjectType==EObjectStateTypeEnum::Chaos_Object_Kinematic;
    }
    bool bPersistedObservedBindingReadback =
        Manager->GetObservedComponents().Num() == PlaybackComponents.Num();
    TArray<TSharedPtr<FJsonValue>> ObservedBindingEntries;
    const TArray<FObservedComponent> ReadbackObserved=Manager->GetObservedComponents();
    for(int32 Index=0;Index<ReadbackObserved.Num();++Index)
    {
        const FObservedComponent& Observed=ReadbackObserved[Index];
        SDK(TEXT("read_observed_binding"));UPrimitiveComponent* ObservedComponent=Observed.GetComponent(Manager);if(!Guard()) { return CommandError(Work->R.Error); }const bool bCacheResolved=CacheCollection->FindCache(Observed.CacheName)!=nullptr;if(!Guard()) { return CommandError(Work->R.Error); }
        const bool bExpected = PlaybackComponents.IsValidIndex(Index) &&
            PlaybackCacheNames.IsValidIndex(Index) && ObservedComponent == PlaybackComponents[Index] &&
            Observed.CacheName == FName(*PlaybackCacheNames[Index]) && Observed.bPlaybackEnabled && bCacheResolved;
        bPersistedObservedBindingReadback &= bExpected;
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("component_path"),
            ObservedComponent ? ObservedComponent->GetPathName() : FString());
        Entry->SetStringField(TEXT("cache_name"), Observed.CacheName.ToString());
        Entry->SetBoolField(TEXT("playback_enabled"), Observed.bPlaybackEnabled);
        Entry->SetBoolField(TEXT("cache_resolved_in_collection"), bCacheResolved);
        Entry->SetBoolField(TEXT("exact_binding_match"), bExpected);
        ObservedBindingEntries.Add(MakeShared<FJsonValueObject>(Entry));
    }
    TSharedPtr<FJsonObject> Data = Success(TEXT("bind_chaos_cache_to_sequence"));
    Data->SetStringField(TEXT("manager_binding_guid"), BindingGuid.ToString());
    Data->SetBoolField(TEXT("reused_manager_binding"), bReusedBinding);
    Data->SetBoolField(TEXT("reused_chaos_cache_track"), bReusedTrack);
    Data->SetBoolField(TEXT("reused_chaos_cache_section"), bReusedSection);
    Data->SetBoolField(TEXT("idempotent_binding_gate"), ChaosTrackCount == 1 && ExactSectionMatchCount == 1);
    Data->SetNumberField(TEXT("chaos_cache_track_count"), ChaosTrackCount);
    SDK(TEXT("read_final_chaos_sections"));const TArray<UMovieSceneSection*> FinalTrackSections=Track->GetAllSections();if(!Guard()) { return CommandError(Work->R.Error); }Data->SetNumberField(TEXT("chaos_cache_section_count"),FinalTrackSections.Num());
    Data->SetNumberField(TEXT("section_start_frame"), StartFrame);
    Data->SetNumberField(TEXT("section_end_frame"), EndFrame);
    Data->SetStringField(TEXT("manager_cache_collection_path"), Manager->CacheCollection ? Manager->CacheCollection->GetPathName() : FString());
    Data->SetStringField(TEXT("requested_cache_collection_path"), CacheCollectionPath);
    Data->SetNumberField(TEXT("exact_cache_section_match_count"), ExactSectionMatchCount);
    Data->SetBoolField(TEXT("exact_cache_collection_readback_gate"), bExactCacheReadback);
    Data->SetBoolField(TEXT("reused_observed_bindings"), bReusedObservedBindings);
    Data->SetArrayField(TEXT("observed_bindings"), ObservedBindingEntries);
    Data->SetBoolField(TEXT("persisted_observed_binding_readback_gate"),
        bPersistedObservedBindingReadback);
    Data->SetBoolField(TEXT("persisted_live_simulation_suppression_gate"),
        bPersistedPlaybackSimulationSuppressed);
    Data->SetStringField(TEXT("persisted_geometry_collection_object_type"),
        TEXT("Kinematic"));
    Data->SetStringField(TEXT("persisted_cache_mode"), TEXT("None"));
    Data->SetStringField(TEXT("persisted_start_mode"), TEXT("Timed"));
    TArray<TSharedPtr<FJsonValue>> SectionEntries;
    for(UMovieSceneSection* ExistingSectionBase:FinalTrackSections)
    {
        if (UMovieSceneChaosCacheSection* ExistingSection = Cast<UMovieSceneChaosCacheSection>(ExistingSectionBase))
        {
            if(!Work->Track(ExistingSection,TEXT("reported_chaos_section"),Track)) { return CommandError(Work->R.Error); }const auto CurrentRange=ExistingSection->GetRange();if(!CurrentRange.HasLowerBound()||!CurrentRange.HasUpperBound()) { return CommandError(TEXT("Chaos Cache section range became unbounded during readback")); }
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("start_tick"), ExistingSection->GetInclusiveStartFrame().Value);
            Entry->SetNumberField(TEXT("end_tick"), ExistingSection->GetExclusiveEndFrame().Value);
            Entry->SetNumberField(TEXT("play_rate"), ExistingSection->Params.PlayRate);
            Entry->SetNumberField(TEXT("start_offset_tick"), ExistingSection->Params.StartFrameOffset.Value);
            Entry->SetStringField(TEXT("cache_collection_path"), ExistingSection->Params.CacheCollection
                ? ExistingSection->Params.CacheCollection->GetPathName() : FString());
            SectionEntries.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }
    Data->SetArrayField(TEXT("sections"), SectionEntries);
    Data->SetBoolField(TEXT("saved"), bSaved);
    Data->SetBoolField(TEXT("level_saved"), bLevelSaved);
    Data->SetStringField(TEXT("assertion_status"),
        bSaved && bLevelSaved && bExactCacheReadback && bPersistedObservedBindingReadback &&
        bPersistedPlaybackSimulationSuppressed &&
        Manager->CacheMode == ECacheMode::None && Manager->StartMode == EStartMode::Timed && ChaosTrackCount == 1
            ? TEXT("PASS") : TEXT("FAIL"));
    return FLegacy::CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> HandleTiming(const TSharedPtr<FJsonObject>& Params)
{
    ULevelSequence* Sequence=Work->Load<ULevelSequence>(GetString(Params,TEXT("sequence_path")),TEXT("load_sequence_for_timing"));SDK(TEXT("resolve_movie_scene"));UMovieScene* MovieScene=Sequence?Sequence->GetMovieScene():nullptr;
    if(!MovieScene) { return CommandError(TEXT("Level Sequence not found")); }if(!Work->Track(MovieScene,TEXT("timing_movie_scene"),Sequence)) { return CommandError(Work->R.Error); }
    const int32 DisplayRate=FMath::Max(1,CastI32(GetNumber(Params,TEXT("display_rate"),30),TEXT("display_rate"))),TickResolution=FMath::Max(DisplayRate,CastI32(GetNumber(Params,TEXT("tick_resolution"),24000),TEXT("tick_resolution")));
    const int32 StartFrame=CastI32(GetNumber(Params,TEXT("start_frame"),0),TEXT("start_frame")),EndFrame=EndAfter(StartFrame,CastI32(GetNumber(Params,TEXT("end_frame"),60),TEXT("end_frame")),TEXT("end_frame"));
    const int32 SectionStart=CastI32(GetNumber(Params,TEXT("section_start_frame"),StartFrame),TEXT("section_start_frame")),SectionEnd=EndAfter(SectionStart,CastI32(GetNumber(Params,TEXT("section_end_frame"),EndFrame),TEXT("section_end_frame")),TEXT("section_end_frame"));
    const FFrameNumber TickStart=DisplayFrameToTickFrame(StartFrame,FFrameRate(DisplayRate,1),FFrameRate(TickResolution,1)),TickEnd=DisplayFrameToTickFrame(EndFrame,FFrameRate(DisplayRate,1),FFrameRate(TickResolution,1));const int32 TickDuration=Duration(TickStart.Value,TickEnd.Value);
    const FFrameNumber ExpectedSectionTickStart=DisplayFrameToTickFrame(SectionStart,FFrameRate(DisplayRate,1),FFrameRate(TickResolution,1)),ExpectedSectionTickEnd=DisplayFrameToTickFrame(SectionEnd,FFrameRate(DisplayRate,1),FFrameRate(TickResolution,1));
    const float PlayRate=CastFloat(GetNumber(Params,TEXT("cache_play_rate"),1.0),TEXT("cache_play_rate"));const FFrameNumber Offset(FMath::Max(0,CastI32(GetNumber(Params,TEXT("cache_start_offset_seconds"),0.0)*TickResolution,TEXT("cache_start_offset_seconds_ticks"))));const bool Reverse=GetBool(Params,TEXT("reverse"),false);
    if(!Work->R.Error.IsEmpty()||!Guard()) { return CommandError(Work->R.Error); }
    const TArray<UMovieSceneChaosCacheSection*> Sections=FindChaosSections(MovieScene);if(!Work->R.Error.IsEmpty()||!Guard()) { return CommandError(Work->R.Error); }
    Work->R.Data->SetNumberField(TEXT("timing_pre_mutation_section_count"),Sections.Num());
    if(!Work->Modify(Sequence,TEXT("modify_timing_sequence"))||!Work->Modify(MovieScene,TEXT("modify_timing_movie_scene"))) { return CommandError(Work->R.Error); }
    Write(TEXT("set_timing_display_rate"));MovieScene->SetDisplayRate(FFrameRate(DisplayRate,1));if(!Guard()) { return CommandError(Work->R.Error); }
    Write(TEXT("set_timing_tick_resolution"));MovieScene->SetTickResolutionDirectly(FFrameRate(TickResolution,1));if(!Guard()) { return CommandError(Work->R.Error); }
    Write(TEXT("set_timing_playback_range"));MovieScene->SetPlaybackRange(TickStart.Value,TickDuration);if(!Guard()) { return CommandError(Work->R.Error); }
    const auto SectionsAfterRateChange=FindChaosSections(MovieScene);if(!Work->R.Error.IsEmpty()||!Guard()) { return CommandError(Work->R.Error); }
    if(SectionsAfterRateChange!=Sections) { return CommandError(TEXT("Existing Chaos Cache section identities changed during Modify/rate/playback callbacks; changes are retained")); }
    for(auto* Section:Sections) { if(!Work->Modify(Section,TEXT("modify_timing_section"))) { return CommandError(Work->R.Error); }Write(TEXT("set_timing_section_range"));Section->SetRange(TRange<FFrameNumber>(ExpectedSectionTickStart,ExpectedSectionTickEnd));if(!Guard()) { return CommandError(Work->R.Error); }Write(TEXT("set_timing_section_params"));Section->Params.PlayRate=PlayRate;Section->Params.StartFrameOffset=Offset;Section->Params.bReverse=Reverse; }
    const bool bSaved=SaveAsset(Sequence);if(!Guard()) { return CommandError(Work->R.Error); }SDK(TEXT("timing_independent_readback"));const auto CurrentSections=FindChaosSections(MovieScene);if(!Work->R.Error.IsEmpty()||!Guard()) { return CommandError(Work->R.Error); }
    const bool SameSections=CurrentSections==Sections;Work->Checks->SetBoolField(TEXT("timing_section_identities_retained"),SameSections);
    const bool bTwoSecondsSixtyFrames=DisplayRate==30&&StartFrame==0&&EndFrame==60&&SectionStart==0&&SectionEnd==60&&!Sections.IsEmpty();
    const auto PlaybackRangeReadback=MovieScene->GetPlaybackRange();if(!PlaybackRangeReadback.HasLowerBound()||!PlaybackRangeReadback.HasUpperBound()) { return CommandError(TEXT("Playback range became unbounded during timing readback")); }
    const int32 PlaybackStartTickReadback=PlaybackRangeReadback.GetLowerBoundValue().Value,PlaybackEndTickReadback=PlaybackRangeReadback.GetUpperBoundValue().Value;
    int64 MaxTickReadbackError=FMath::Max(FMath::Abs(static_cast<int64>(PlaybackStartTickReadback)-TickStart.Value),FMath::Abs(static_cast<int64>(PlaybackEndTickReadback)-TickEnd.Value));TArray<TSharedPtr<FJsonValue>> SectionReadbacks;
    bool AllParams=true;for(const auto* Section:CurrentSections)
    {
        const auto Range=Section->GetRange();if(!Range.HasLowerBound()||!Range.HasUpperBound()) { return CommandError(TEXT("Chaos Cache range became unbounded during timing readback")); }const int32 ActualStartTick=Section->GetInclusiveStartFrame().Value,ActualEndTick=Section->GetExclusiveEndFrame().Value;
        const int64 StartTickError=FMath::Abs(static_cast<int64>(ActualStartTick)-ExpectedSectionTickStart.Value),EndTickError=FMath::Abs(static_cast<int64>(ActualEndTick)-ExpectedSectionTickEnd.Value);MaxTickReadbackError=FMath::Max(MaxTickReadbackError,FMath::Max(StartTickError,EndTickError));
        AllParams&=FMath::IsNearlyEqual(Section->Params.PlayRate,PlayRate)&&Section->Params.StartFrameOffset==Offset&&bool(Section->Params.bReverse)==Reverse;
        auto Entry=MakeShared<FJsonObject>();Entry->SetNumberField(TEXT("start_tick_readback"),ActualStartTick);Entry->SetNumberField(TEXT("end_tick_readback"),ActualEndTick);Entry->SetNumberField(TEXT("start_tick_error"),static_cast<double>(StartTickError));Entry->SetNumberField(TEXT("end_tick_error"),static_cast<double>(EndTickError));SectionReadbacks.Add(MakeShared<FJsonValueObject>(Entry));
    }
    const bool RatesMatch=MovieScene->GetDisplayRate()==FFrameRate(DisplayRate,1)&&MovieScene->GetTickResolution()==FFrameRate(TickResolution,1);Work->Checks->SetBoolField(TEXT("timing_actual_rates_match"),RatesMatch);Work->Checks->SetBoolField(TEXT("timing_actual_section_params_match"),AllParams);
    if(!SameSections||!RatesMatch||!AllParams) { Work->Fail(TEXT("timing_postcondition"),TEXT("Current section identities/rates/params differ after retained mutation/save")); }const bool bTimingReadbackWithinOneTick=MaxTickReadbackError<=1;
    TSharedPtr<FJsonObject> Data = Success(TEXT("configure_chaos_sequence_timing"));
    Data->SetNumberField(TEXT("display_rate"), DisplayRate);
    Data->SetNumberField(TEXT("tick_resolution"), TickResolution);
    Data->SetNumberField(TEXT("playback_start_frame"), StartFrame);
    Data->SetNumberField(TEXT("playback_end_frame"), EndFrame);
    Data->SetNumberField(TEXT("cache_section_count"), Sections.Num());
    Data->SetNumberField(TEXT("cache_section_start_frame"), SectionStart);
    Data->SetNumberField(TEXT("cache_section_end_frame"), SectionEnd);
    Data->SetNumberField(TEXT("playback_start_tick_readback"), PlaybackStartTickReadback);
    Data->SetNumberField(TEXT("playback_end_tick_readback"), PlaybackEndTickReadback);
    Data->SetArrayField(TEXT("cache_section_tick_readbacks"), SectionReadbacks);
    Data->SetNumberField(TEXT("maximum_tick_readback_error"), MaxTickReadbackError);
    Data->SetBoolField(TEXT("timing_readback_within_one_tick"), bTimingReadbackWithinOneTick);
    Data->SetBoolField(TEXT("two_second_sixty_frame_gate"), bTwoSecondsSixtyFrames);
    Data->SetBoolField(TEXT("saved"), bSaved);
    Data->SetStringField(TEXT("assertion_status"), bSaved && bTwoSecondsSixtyFrames && bTimingReadbackWithinOneTick
        ? TEXT("PASS") : TEXT("FAIL"));
    return FLegacy::CreateSuccessResponse(Data);
}

struct FMRQManifestAudit
{
    bool bParsed = false;
    bool bIdentity = false;
    bool bRangeResolution = false;
    bool bFrames = false;
    bool bAcceptance = false;
    bool bPayloadShaPresent = false;
    bool bPayloadShaValid = false;
    int32 FullFrameCount = 0;
    int32 RepeatFrameCount = 0;
    int32 InvalidFrameCount = 0;
    FString OutputRoot;
    FString ManifestFileSha256;
    FString ChaosStateSha256;
    TArray<FString> Failures;
};

bool ReadIntPair(const TSharedPtr<FJsonObject>& Object,const TCHAR* Field,int32 ExpectedA,int32 ExpectedB)
{
    const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(!Object||!Object->TryGetArrayField(Field,Values)||!Values||Values->Num()!=2||!(*Values)[0]||!(*Values)[1]) { return false; }
    const double A=(*Values)[0]->AsNumber(),B=(*Values)[1]->AsNumber();return FMath::IsFinite(A)&&FMath::IsFinite(B)&&A>static_cast<double>(MIN_int32)-1.0&&A<static_cast<double>(MAX_int32)+1.0&&B>static_cast<double>(MIN_int32)-1.0&&B<static_cast<double>(MAX_int32)+1.0&&static_cast<int32>(A)==ExpectedA&&static_cast<int32>(B)==ExpectedB;
}
int32 AuditMRQFrames(
    const TSharedPtr<FJsonObject>& Manifest,
    const TCHAR* Field,
    int32 ExpectedCount,
    const FString& NormalizedOutputRoot,
    int32& OutInvalidCount)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Manifest->TryGetArrayField(Field, Values) || !Values)
    {
        OutInvalidCount += ExpectedCount;
        return 0;
    }
    TSet<FString> UniquePaths;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
        FString Path;
        FString ExpectedSha;
        double ExpectedBytes = 0.0;
        double Width = 0.0;
        double Height = 0.0;
        bool bBlackFrame = true;
        const bool bSchema = Entry.IsValid() && Entry->TryGetStringField(TEXT("path"), Path) &&
            Entry->TryGetStringField(TEXT("sha256"), ExpectedSha) &&
            Entry->TryGetNumberField(TEXT("bytes"), ExpectedBytes) &&
            Entry->TryGetNumberField(TEXT("width"), Width) &&
            Entry->TryGetNumberField(TEXT("height"), Height) &&
            Entry->TryGetBoolField(TEXT("black_frame"), bBlackFrame);
        FString NormalizedPath = FPaths::ConvertRelativePathToFull(Path);
        FPaths::MakeStandardFilename(NormalizedPath);
        const bool bUniquePath = !UniquePaths.Contains(NormalizedPath);
        if (bUniquePath) UniquePaths.Add(NormalizedPath);
        const bool bPathIdentity = bSchema && bUniquePath &&
            NormalizedPath.StartsWith(NormalizedOutputRoot + TEXT("/"));
        TArray<uint8> Bytes;
        const bool bRead=bPathIdentity&&ExpectedBytes>0.0&&ExpectedBytes<=67108864.0&&ReadEvidenceFile(NormalizedPath,Bytes);
        const FString ActualSha = bRead ? Sha256Bytes(Bytes.GetData(), Bytes.Num()) : FString();
        const bool bValid = bRead && Bytes.Num() > 0 && Bytes.Num() == static_cast<int64>(ExpectedBytes) &&
            Width == 640.0 && Height == 360.0 && !bBlackFrame && ExpectedSha.Len() == 64 &&
            ActualSha.Equals(ExpectedSha, ESearchCase::IgnoreCase);
        OutInvalidCount += bValid ? 0 : 1;
    }
    return Values->Num();
}

FMRQManifestAudit AuditMRQManifest(
    const FString& ManifestPath,
    const FString& ExpectedSequencePath,
    const FString& ExpectedMapPath,
    const FString& ExpectedOutputRoot,
    const FString& ExpectedChaosStateSha256)
{
    FMRQManifestAudit Audit;
    FString ManifestText;
    TArray<uint8> ManifestBytes;
    if(ManifestPath.IsEmpty()||!ReadEvidenceFile(ManifestPath,ManifestBytes,4194304))
    {
        Audit.Failures.Add(TEXT("mrq_manifest_missing_or_unreadable"));
        return Audit;
    }
    FFileHelper::BufferToString(ManifestText,ManifestBytes.GetData(),ManifestBytes.Num());Audit.ManifestFileSha256=Sha256Bytes(ManifestBytes.GetData(),ManifestBytes.Num());
    TSharedPtr<FJsonObject> Manifest;
    Audit.bParsed=Parse(ManifestText,Manifest);
    if (!Audit.bParsed)
    {
        Audit.Failures.Add(TEXT("mrq_manifest_json_invalid"));
        return Audit;
    }
    FString Operation;
    FString OperationStatus;
    FString AssertionStatus;
    FString SequencePath;
    FString MapPath;
    FString PayloadSha;
    Manifest->TryGetStringField(TEXT("operation"), Operation);
    Manifest->TryGetStringField(TEXT("operation_status"), OperationStatus);
    Manifest->TryGetStringField(TEXT("assertion_status"), AssertionStatus);
    Manifest->TryGetStringField(TEXT("sequence_path"), SequencePath);
    Manifest->TryGetStringField(TEXT("map_path"), MapPath);
    Manifest->TryGetStringField(TEXT("output_root"), Audit.OutputRoot);
    Manifest->TryGetStringField(TEXT("chaos_state_sha256"), Audit.ChaosStateSha256);
    Manifest->TryGetStringField(TEXT("manifest_payload_sha256"), PayloadSha);
    Audit.bPayloadShaPresent = PayloadSha.Len() == 64;
    FString CanonicalPayloadText = ManifestText.Replace(TEXT("\r\n"), TEXT("\n"));
    const FString PayloadFieldMarker = TEXT("\n  \"manifest_payload_sha256\"");
    const int32 PayloadFieldIndex = CanonicalPayloadText.Find(PayloadFieldMarker, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (PayloadFieldIndex != INDEX_NONE)
    {
        FString PayloadWithoutDigest = CanonicalPayloadText.Left(PayloadFieldIndex);
        if (PayloadWithoutDigest.EndsWith(TEXT(","))) PayloadWithoutDigest.LeftChopInline(1);
        PayloadWithoutDigest += TEXT("\n}");
        FTCHARToUTF8 PayloadUtf8(*PayloadWithoutDigest);
        const FString ComputedPayloadSha = Sha256Bytes(PayloadUtf8.Get(), PayloadUtf8.Length());
        Audit.bPayloadShaValid = Audit.bPayloadShaPresent &&
            ComputedPayloadSha.Equals(PayloadSha, ESearchCase::IgnoreCase);
    }
    FString NormalizedOutputRoot = FPaths::ConvertRelativePathToFull(Audit.OutputRoot);
    FPaths::MakeStandardFilename(NormalizedOutputRoot);
    FString NormalizedExpectedOutputRoot = FPaths::ConvertRelativePathToFull(ExpectedOutputRoot);
    FPaths::MakeStandardFilename(NormalizedExpectedOutputRoot);
    const bool bExpectedOutputMatches = ExpectedOutputRoot.IsEmpty() ||
        NormalizedOutputRoot.Equals(NormalizedExpectedOutputRoot, ESearchCase::IgnoreCase);
    const auto NormalizeAssetIdentity = [](const FString& Path)
    {
        return FPackageName::ObjectPathToPackageName(Path);
    };
    const bool bExpectedMapMatches = !ExpectedMapPath.IsEmpty() &&
        NormalizeAssetIdentity(MapPath) == NormalizeAssetIdentity(ExpectedMapPath);
    const bool bExpectedChaosStateMatches = Audit.ChaosStateSha256.Len() == 64 &&
        (ExpectedChaosStateSha256.IsEmpty() || Audit.ChaosStateSha256.Equals(ExpectedChaosStateSha256, ESearchCase::IgnoreCase));
    Audit.bIdentity = Operation == TEXT("chaos_cinematic_mrq") && OperationStatus == TEXT("PASS") &&
        AssertionStatus == TEXT("PASS") &&
        NormalizeAssetIdentity(SequencePath) == NormalizeAssetIdentity(ExpectedSequencePath) && bExpectedMapMatches &&
        bExpectedOutputMatches && bExpectedChaosStateMatches;
    double RepeatKeyFrame = -1.0;
    double PerceptualDistance = TNumericLimits<double>::Max();
    Manifest->TryGetNumberField(TEXT("repeat_key_frame"), RepeatKeyFrame);
    Manifest->TryGetNumberField(TEXT("repeat_key_perceptual_hash_distance"), PerceptualDistance);
    Audit.bRangeResolution = ReadIntPair(Manifest, TEXT("resolution"), 640, 360) &&
        ReadIntPair(Manifest, TEXT("frame_range"), 0, 60) && RepeatKeyFrame == 30.0 && PerceptualDistance <= 4.0;
    Audit.FullFrameCount = AuditMRQFrames(
        Manifest, TEXT("full_frames"), 60, NormalizedOutputRoot, Audit.InvalidFrameCount);
    Audit.RepeatFrameCount = AuditMRQFrames(
        Manifest, TEXT("repeat_key_frames"), 1, NormalizedOutputRoot, Audit.InvalidFrameCount);
    Audit.bFrames = Audit.FullFrameCount == 60 && Audit.RepeatFrameCount == 1 && Audit.InvalidFrameCount == 0;
    const TSharedPtr<FJsonObject>* Acceptance = nullptr;
    Audit.bAcceptance = Manifest->TryGetObjectField(TEXT("acceptance"), Acceptance) && Acceptance && Acceptance->IsValid();
    const TCHAR* AcceptanceKeys[] = {
        TEXT("full_frame_count_60"), TEXT("repeat_key_frame_count_1"), TEXT("resolution_640x360"),
        TEXT("decode_zero_byte_black_failures_zero"), TEXT("repeat_key_perceptual_distance_lte_4"),
        TEXT("chaos_state_sha256_supplied")};
    for (const TCHAR* Key : AcceptanceKeys)
    {
        bool bValue = false;
        Audit.bAcceptance &= Acceptance && Acceptance->IsValid() && (*Acceptance)->TryGetBoolField(Key, bValue) && bValue;
    }
    if (!Audit.bIdentity) Audit.Failures.Add(TEXT("mrq_manifest_identity_mismatch"));
    if (!Audit.bRangeResolution) Audit.Failures.Add(TEXT("mrq_manifest_range_or_resolution_mismatch"));
    if (!Audit.bFrames) Audit.Failures.Add(TEXT("mrq_manifest_frame_file_sha_or_decode_evidence_invalid"));
    if (!Audit.bAcceptance) Audit.Failures.Add(TEXT("mrq_manifest_acceptance_flags_failed"));
    if (!Audit.bPayloadShaValid) Audit.Failures.Add(TEXT("mrq_manifest_payload_sha256_mismatch"));
    return Audit;
}

TSharedPtr<FJsonObject> HandleInspect(const TSharedPtr<FJsonObject>& Params)
{
    UGeometryCollection* CollectionAsset=Work->Load<UGeometryCollection>(GetString(Params,TEXT("geometry_collection_path")),TEXT("load_inspected_geometry_collection"));
    ULevelSequence* Sequence=Work->Load<ULevelSequence>(GetString(Params,TEXT("sequence_path")),TEXT("load_inspected_sequence"));
    SDK(TEXT("resolve_inspected_movie_scene"));UMovieScene* MovieScene=Sequence?Sequence->GetMovieScene():nullptr;if(!Guard()||(MovieScene&&!Work->Track(MovieScene,TEXT("inspected_movie_scene"),Sequence))) { return CommandError(Work->R.Error); }
    UWorld* World = ResolveEditorWorld();
    AChaosCacheManager* Manager = FindActor<AChaosCacheManager>(World, GetString(Params, TEXT("cache_manager_actor_name")));
    AFieldSystemActor* FieldActor = FindActor<AFieldSystemActor>(World, GetString(Params, TEXT("field_actor_name"), TEXT("Chaos_AnchorField")));
    UFieldSystemComponent* FieldComponent=FieldActor?FieldActor->GetFieldSystemComponent():nullptr;if(!Guard()||(FieldComponent&&!Work->Track(FieldComponent,TEXT("inspected_anchor_field"),FieldActor))) { return CommandError(Work->R.Error); }
    if(Manager&&!CaptureCacheState(Manager->CacheCollection,TEXT("inspected_manager_cache"))) { return CommandError(Work->R.Error); }
    TArray<FString> Failures;
    if (!CollectionAsset) Failures.Add(TEXT("geometry_collection_missing"));
    if (!MovieScene) Failures.Add(TEXT("level_sequence_missing"));
    if (!Manager) Failures.Add(TEXT("cache_manager_missing"));
    if (!FieldComponent || FieldComponent->ConstructionCommands.GetNumCommands() <= 0)
        Failures.Add(TEXT("anchor_field_missing"));
    const TArray<UMovieSceneChaosCacheSection*> Sections=FindChaosSections(MovieScene);if(!Work->R.Error.IsEmpty()||!Guard()) { return CommandError(Work->R.Error); }
    if (Sections.IsEmpty()) Failures.Add(TEXT("chaos_cache_track_or_section_missing"));
    const bool bProxy = CollectionAsset && CollectionAsset->RootProxyData.ProxyMeshes.Num() > 0 &&
        CollectionAsset->RootProxyData.ProxyMeshes.Num() == CollectionAsset->RootProxyData.MeshTransforms.Num() &&
        CollectionAsset->CustomRendererType == UGeometryCollectionRootProxyRenderer::StaticClass();
    if (!bProxy) Failures.Add(TEXT("root_mesh_proxy_missing"));
    if (CollectionAsset && CollectionAsset->EnableNanite) Failures.Add(TEXT("nanite_must_remain_disabled"));
    const int32 ExpectedDisplayRate = FMath::Max(1, CastI32(GetNumber(Params,TEXT("expected_display_rate"),30),TEXT("expected_display_rate")));
    const int32 ExpectedTickResolution = FMath::Max(ExpectedDisplayRate, CastI32(GetNumber(Params,TEXT("expected_tick_resolution"),24000),TEXT("expected_tick_resolution")));
    const int32 ExpectedStartFrame = CastI32(GetNumber(Params,TEXT("expected_playback_start_frame"),0),TEXT("expected_playback_start_frame"));
    const int32 ExpectedEndFrame = EndAfter(ExpectedStartFrame,CastI32(GetNumber(Params,TEXT("expected_playback_end_frame"),60),TEXT("expected_playback_end_frame")),TEXT("expected_playback_end_frame"));
    const FFrameRate ExpectedDisplayFrameRate(ExpectedDisplayRate, 1);
    const FFrameNumber ExpectedTickStart = MovieScene
        ? DisplayFrameToTickFrame(ExpectedStartFrame, ExpectedDisplayFrameRate, MovieScene->GetTickResolution())
        : FFrameNumber(0);
    const FFrameNumber ExpectedTickEnd = MovieScene
        ? DisplayFrameToTickFrame(ExpectedEndFrame, ExpectedDisplayFrameRate, MovieScene->GetTickResolution())
        : FFrameNumber(0);
    if(!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }
    const bool bExpectedSequenceTiming = MovieScene&&MovieScene->GetPlaybackRange().HasLowerBound()&&MovieScene->GetPlaybackRange().HasUpperBound()&&MovieScene->GetDisplayRate()==ExpectedDisplayFrameRate&&
        MovieScene->GetTickResolution() == FFrameRate(ExpectedTickResolution, 1) &&
        MovieScene->GetPlaybackRange().GetLowerBoundValue() == ExpectedTickStart &&
        MovieScene->GetPlaybackRange().GetUpperBoundValue() == ExpectedTickEnd;
    if (!bExpectedSequenceTiming) Failures.Add(TEXT("expected_sequence_timing_missing"));
    const bool bMRQPluginLoaded = FModuleManager::Get().ModuleExists(TEXT("MovieRenderPipelineCore"));
    UClass* QueueSubsystemClass = FindObject<UClass>(nullptr, TEXT("/Script/MovieRenderPipelineCore.MoviePipelineQueueEngineSubsystem"));
    UClass* ExecutorJobClass = FindObject<UClass>(nullptr, TEXT("/Script/MovieRenderPipelineCore.MoviePipelineExecutorJob"));
    if (GetBool(Params, TEXT("require_movie_render_pipeline"), true) && (!bMRQPluginLoaded || !QueueSubsystemClass || !ExecutorJobClass))
        Failures.Add(TEXT("movie_render_pipeline_unavailable"));
    const FString MRQOutputDirectory = GetString(Params, TEXT("mrq_output_directory"));
    int32 MRQFrameFileCount=0,DirectoryEntries=0;bool DirectoryScanComplete=true;
    if (!MRQOutputDirectory.IsEmpty())
    {
        SDK(TEXT("scan_MRQ_output_files"));IFileManager::Get().IterateDirectoryRecursively(*MRQOutputDirectory,[&](const TCHAR* Path,bool bDirectory)
        {
            if(++DirectoryEntries>100000) { DirectoryScanComplete=false;return false; }MRQFrameFileCount += !bDirectory&&FPaths::GetExtension(Path).Equals(TEXT("png"),ESearchCase::IgnoreCase)?1:0;
            return true;
        });
    }
    if(!DirectoryScanComplete) { Failures.Add(TEXT("mrq_output_directory_budget_exceeded")); }if(!Guard()) { return CommandError(Work->R.Error); }
    const FString MRQManifestPath = GetString(Params, TEXT("mrq_manifest_path"));
    const FMRQManifestAudit MRQManifest = AuditMRQManifest(
        MRQManifestPath,
        Sequence ? Sequence->GetPathName() : FString(),
        GetString(Params, TEXT("map_path")),
        MRQOutputDirectory,
        GetString(Params, TEXT("chaos_state_sha256")));
    for (const FString& ManifestFailure : MRQManifest.Failures)
        Failures.AddUnique(ManifestFailure);
    TArray<FString> AuditedPackageNames;
    if (CollectionAsset) AuditedPackageNames.Add(CollectionAsset->GetOutermost()->GetName());
    if (Sequence) AuditedPackageNames.Add(Sequence->GetOutermost()->GetName());
    if (Manager && Manager->CacheCollection) AuditedPackageNames.Add(Manager->CacheCollection->GetOutermost()->GetName());
    if (World) AuditedPackageNames.AddUnique(World->GetOutermost()->GetName());
    const FString RequestedMapPath = GetString(Params, TEXT("map_path"));
    if (!RequestedMapPath.IsEmpty())
        AuditedPackageNames.AddUnique(FPackageName::ObjectPathToPackageName(RequestedMapPath));
    TArray<FString> ForbiddenDependencies;
    TArray<TSharedPtr<FJsonValue>> DependencyAuditEntries;
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FString> PackagesToAudit = AuditedPackageNames;
    TSet<FString> VisitedPackages;
    bool bAllDependencyQueriesSucceeded = true;
    for (int32 PackageCursor = 0; PackageCursor < PackagesToAudit.Num() && PackagesToAudit.Num() <= 8192; ++PackageCursor)
    {
        const FString PackageName = PackagesToAudit[PackageCursor];
        if (VisitedPackages.Contains(PackageName)) continue;
        VisitedPackages.Add(PackageName);
        TArray<FName> Dependencies;
        const bool bQuerySucceeded = AssetRegistry.GetDependencies(
            FName(*PackageName), Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
        bAllDependencyQueriesSucceeded &= bQuerySucceeded;
        TArray<TSharedPtr<FJsonValue>> DependencyValues;
        for (const FName& DependencyName : Dependencies)
        {
            const FString DependencyString = DependencyName.ToString();
            if (AuditedPackageNames.Contains(PackageName))
                DependencyValues.Add(MakeShared<FJsonValueString>(DependencyString));
            if(Dependency(DependencyString))
                ForbiddenDependencies.AddUnique(DependencyString);
            if (DependencyString.StartsWith(TEXT("/")) &&
                !DependencyString.StartsWith(TEXT("/Script/")) &&
                !DependencyString.StartsWith(TEXT("/Engine/")) &&
                !VisitedPackages.Contains(DependencyString))
                PackagesToAudit.AddUnique(DependencyString);
        }
        if (AuditedPackageNames.Contains(PackageName))
        {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("package_name"), PackageName);
            Entry->SetBoolField(TEXT("query_succeeded"), bQuerySucceeded);
            Entry->SetArrayField(TEXT("dependencies"), DependencyValues);
            DependencyAuditEntries.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }
    const bool bDependencyTraversalComplete = PackagesToAudit.Num() <= 8192;
    const bool bNoMCPAssetDependency = !AuditedPackageNames.IsEmpty() && bAllDependencyQueriesSucceeded &&
        bDependencyTraversalComplete && ForbiddenDependencies.IsEmpty();
    if (!bNoMCPAssetDependency) Failures.Add(TEXT("mcp_asset_dependency_detected_or_audit_empty"));
    TSharedPtr<FJsonObject> Data = Success(TEXT("inspect_chaos_cinematic_pipeline"));
    Data->SetBoolField(TEXT("geometry_collection_found"), CollectionAsset != nullptr);
    SDK(TEXT("observe_collection_metrics"));AddCollectionMetrics(Data,CollectionAsset);if(!Guard()) { return CommandError(Work->R.Error); }
    Data->SetStringField(TEXT("topology_assertion_scope"),TEXT("Original legacy assertion_failures gates are preserved. collection_integrity_gate is a separate reported metric and was not a legacy assertion predicate; no full topology validation is implied by assertion_status PASS."));
    Data->SetStringField(TEXT("MRQ_image_validation_scope"),TEXT("Native verifies actual file bytes/SHA and manifest metadata claims. PNG decode/black-frame/perceptual-hash computation belongs to the external Python producer and is not rerun by this observer."));
    Data->SetStringField(TEXT("dependency_graph_scope"),TEXT("AssetRegistry on-disk Package-category traversal only, bounded8192; not all serialized/management/runtime references or noMCP packaging proof."));
    Data->SetBoolField(TEXT("root_mesh_proxy_configured"), bProxy);
    Data->SetBoolField(TEXT("nanite_used"), CollectionAsset && CollectionAsset->EnableNanite);
    Data->SetBoolField(TEXT("anchor_configured"),
        FieldComponent && FieldComponent->ConstructionCommands.GetNumCommands() > 0);
    Data->SetBoolField(TEXT("cache_manager_found"), Manager != nullptr);
    Data->SetBoolField(TEXT("cache_collection_found"), Manager && Manager->CacheCollection != nullptr);
    Data->SetNumberField(TEXT("chaos_cache_section_count"), Sections.Num());
    Data->SetNumberField(TEXT("expected_display_rate"), ExpectedDisplayRate);
    Data->SetNumberField(TEXT("expected_tick_resolution"), ExpectedTickResolution);
    Data->SetNumberField(TEXT("expected_playback_start_frame"), ExpectedStartFrame);
    Data->SetNumberField(TEXT("expected_playback_end_frame"), ExpectedEndFrame);
    Data->SetBoolField(TEXT("expected_sequence_timing_gate"), bExpectedSequenceTiming);
    Data->SetBoolField(TEXT("two_second_sixty_frame_gate"),
        bExpectedSequenceTiming && ExpectedStartFrame == 0 && ExpectedEndFrame == 60 && ExpectedDisplayRate == 30);
    Data->SetBoolField(TEXT("mrq_plugin_loaded"), bMRQPluginLoaded);
    Data->SetBoolField(TEXT("mrq_queue_subsystem_available"), QueueSubsystemClass != nullptr);
    Data->SetBoolField(TEXT("mrq_classes_available"), QueueSubsystemClass && ExecutorJobClass);
    Data->SetStringField(TEXT("mrq_output_directory"), MRQOutputDirectory);
    Data->SetNumberField(TEXT("mrq_frame_file_count"),MRQFrameFileCount);Data->SetBoolField(TEXT("mrq_output_directory_scan_complete"),DirectoryScanComplete);
    Data->SetStringField(TEXT("mrq_manifest_path"), MRQManifestPath);
    Data->SetBoolField(TEXT("mrq_manifest_parsed"), MRQManifest.bParsed);
    Data->SetBoolField(TEXT("mrq_manifest_identity_gate"), MRQManifest.bIdentity);
    Data->SetBoolField(TEXT("mrq_manifest_range_resolution_gate"), MRQManifest.bRangeResolution);
    Data->SetBoolField(TEXT("mrq_manifest_frame_sha_gate"), MRQManifest.bFrames);
    Data->SetBoolField(TEXT("mrq_manifest_acceptance_gate"), MRQManifest.bAcceptance);
    Data->SetBoolField(TEXT("mrq_manifest_payload_sha256_present"), MRQManifest.bPayloadShaPresent);
    Data->SetBoolField(TEXT("mrq_manifest_payload_sha256_valid"), MRQManifest.bPayloadShaValid);
    Data->SetStringField(TEXT("mrq_manifest_file_sha256"), MRQManifest.ManifestFileSha256);
    Data->SetStringField(TEXT("mrq_chaos_state_sha256"), MRQManifest.ChaosStateSha256);
    Data->SetNumberField(TEXT("mrq_full_frame_count"), MRQManifest.FullFrameCount);
    Data->SetNumberField(TEXT("mrq_repeat_frame_count"), MRQManifest.RepeatFrameCount);
    Data->SetNumberField(TEXT("mrq_invalid_frame_evidence_count"), MRQManifest.InvalidFrameCount);
    Data->SetBoolField(TEXT("mrq_manifest_gate"), MRQManifest.bParsed && MRQManifest.bIdentity &&
        MRQManifest.bRangeResolution && MRQManifest.bFrames && MRQManifest.bAcceptance &&
        MRQManifest.bPayloadShaValid);
    Data->SetArrayField(TEXT("asset_registry_dependency_audit"), DependencyAuditEntries);
    TArray<TSharedPtr<FJsonValue>> ForbiddenDependencyValues;
    for (const FString& Dependency : ForbiddenDependencies)
        ForbiddenDependencyValues.Add(MakeShared<FJsonValueString>(Dependency));
    Data->SetArrayField(TEXT("forbidden_mcp_dependencies"), ForbiddenDependencyValues);
    Data->SetNumberField(TEXT("forbidden_mcp_dependency_count"), ForbiddenDependencies.Num());
    Data->SetNumberField(TEXT("dependency_package_query_count"), VisitedPackages.Num());
    Data->SetBoolField(TEXT("dependency_queries_succeeded"), bAllDependencyQueriesSucceeded);
    Data->SetBoolField(TEXT("dependency_traversal_complete"), bDependencyTraversalComplete);
    Data->SetStringField(TEXT("dependency_audit_source"), TEXT("AssetRegistry on-disk package dependencies"));
    Data->SetBoolField(TEXT("no_mcp_asset_dependency"), bNoMCPAssetDependency);
    Data->SetBoolField(TEXT("mcp_dependency_audit_pass"), bNoMCPAssetDependency);
    Data->SetBoolField(TEXT("mcp_dependency_audit_performed"), !AuditedPackageNames.IsEmpty());
    TArray<TSharedPtr<FJsonValue>> FailureValues;
    for (const FString& Failure : Failures) FailureValues.Add(MakeShared<FJsonValueString>(Failure));
    Data->SetArrayField(TEXT("assertion_failures"), FailureValues);
    Data->SetStringField(TEXT("assertion_status"), Failures.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"));
    return FLegacy::CreateSuccessResponse(Data);
}
}
FString CreateSequence(const FString& ParamsJson)
{
    FWork W(TEXT("create_chaos_level_sequence"),true);if(!Begin(W,ParamsJson)) { return W.Finish(nullptr); }
    TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,&W);return W.Finish(HandleCreateSequence(W.Params));
}
FString BindCache(const FString& ParamsJson)
{
    FWork W(TEXT("bind_chaos_cache_to_sequence"),true);if(!Begin(W,ParamsJson)) { return W.Finish(nullptr); }
    TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,&W);return W.Finish(HandleBindCache(W.Params));
}
FString ConfigureTiming(const FString& ParamsJson)
{
    FWork W(TEXT("configure_chaos_sequence_timing"),true);if(!Begin(W,ParamsJson)) { return W.Finish(nullptr); }
    TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,&W);return W.Finish(HandleTiming(W.Params));
}
FString InspectPipeline(const FString& ParamsJson)
{
    FWork W(TEXT("inspect_chaos_cinematic_pipeline"),false);if(!Begin(W,ParamsJson)) { return W.Finish(nullptr); }
    TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,&W);return W.Finish(HandleInspect(W.Params));
}
}
