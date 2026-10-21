#include "Compatibility/UE58/CubelessChaosCacheSessionApi.h"
#include "Compatibility/UE58/CubelessChaosDestructionPerformanceApi.h"
#include "Compatibility/UE58/CubelessChaosLearningApi.h"
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
#include "Algo/Count.h"
#include "Algo/Reverse.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "HAL/PlatformTime.h"
#include "Chaos/ChaosSolverActor.h"
#include "Chaos/Particle/ObjectState.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollectionProxyData.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include <cmath>

namespace Cubeless::UE58::ChaosCacheSessions
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr int32 MaxJsonChars=1048576,MaxJsonDepth=64,MaxSlots=262144,MaxObjects=16384;
bool Running=false,ShuttingDown=false,ShutdownRestore=false;
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
    TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Weak,Outer,ExpectedParent;TWeakObjectPtr<UPackage> Package;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UWorld> ObjectWorld;FString Path,PackagePath,Role;TSharedPtr<FJsonObject> Before;bool ExpectedParentCaptured=false,ObjectWorldCaptured=false,ComponentRestCaptured=false;TWeakObjectPtr<const UGeometryCollection> ComponentRest;
};
struct FWork
{
    FResult R;TSharedPtr<FJsonObject> Params;FString Operation,FailedStage,SessionId;bool RequiresForeignWriterAdmission=true;bool Writer=false,WorldCaptured=false,OperationComplete=true,CancelRequested=false;uint64 WriteSerial=0,CallWriteSerial=0;int32 CallSaveStart=0,StructuralChangeSerial=0,CallStructuralChangeSerial=0;TArray<TSharedPtr<FJsonObject>> CallBefore;
    TWeakObjectPtr<UWorld> World;TArray<FTracked> Tracked;TArray<TSharedPtr<FJsonValue>> Saves,Events;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>();int64 FileBytes=0;int32 FileReads=0;
    explicit FWork(const TCHAR* Name,bool bWriter):Operation(Name),Writer(bWriter)
    {
        R.Compiled=false;R.bSaveRequested=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("postconditions"),Checks);
        R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_wrapper_wait_performed"),false);
        R.Data->SetStringField(TEXT("SDK_synchronous_scope"),TEXT("EndEvaluate may wait for solver tasks, finalize adapters and flush recording. ClearObservedComponents(false) restores previous component simulation/notify flags. Wrapper adds no wait or worker UObject execution; these SDK calls are not nonblocking."));
        R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("saved retains any explicit UPackage::SavePackage true result even if a later stage or identity guard fails. Save callbacks/checkout/full loading and global disk effects are not exhaustively known. Clean readback and all requested saves are separate."));
        R.Data->SetStringField(TEXT("world_scope"),TEXT("Captured active PIE context world when present, otherwise current Editor world, including loaded actors/streaming levels only. EditorAssetSubsystem PIE save permission remains unchanged; default is refusal."));
        R.Data->SetStringField(TEXT("observation_scope"),TEXT("Captured object identity/outer/package/dirty plus selected declarative sequence/cache/component fields. Not all UObject properties, rendered frames, effective solver state, byte-exact persistence or global SDK purity."));
        R.Data->SetBoolField(TEXT("cold_reload_verified"),false);R.Data->SetBoolField(TEXT("render_or_physics_completion_verified"),false);R.Data->SetBoolField(TEXT("whole_external_pipeline_complete"),false);
        for(const TCHAR* K:{TEXT("load_calls"),TEXT("modify_calls"),TEXT("new_sequence_calls"),TEXT("initialize_calls"),TEXT("EndEvaluate_calls"),TEXT("ClearObservedComponents_false_calls"),TEXT("FindOrAddObservedComponent_calls"),TEXT("SetSimulatePhysics_calls"),TEXT("add_possessable_calls"),TEXT("bind_possessable_calls"),TEXT("add_track_calls"),TEXT("add_section_calls"),TEXT("explicit_save_calls"),TEXT("explicit_save_returns"),TEXT("file_read_calls"),TEXT("explicit_Blueprint_compile_calls"),TEXT("explicit_wrapper_wait_calls")}) { Counts->SetNumberField(K,0); }
    }
    void Count(const TCHAR* K) { double N=0;Counts->TryGetNumberField(K,N);Counts->SetNumberField(K,N+1); }
    void SDK(const TCHAR* S) { R.Stage=S;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true); }
    void Write(const TCHAR* S) { ++WriteSerial;SDK(S);R.bMutationAttempted=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),S);J->SetNumberField(TEXT("index"),Events.Num());if(Events.Num()<65536) { Events.Add(MakeShared<FJsonValueObject>(J)); }else { R.Data->SetBoolField(TEXT("mutation_event_budget_exceeded"),true); } }
    bool Fail(const TCHAR* C,const FString& E) { if(R.Error.IsEmpty()) { R.ErrorCode=TEXT("chaos_cache_session_")+FString(C);R.Error=E;FailedStage=R.Stage;R.Data->SetStringField(TEXT("failed_stage"),FailedStage); }return false; }
    bool Guard()
    {
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||(ShuttingDown&&!ShutdownRestore)||CancelRequested) { return Fail(TEXT("editor_lost"),TEXT("Safe Editor game-thread/Engine/GC lifetime is unavailable or cancellation was requested")); }
        if(WorldCaptured&&(!Live(World.Get())||World->bIsTearingDown||CurrentWorld()!=World.Get())) { return Fail(TEXT("world_changed"),TEXT("Captured Editor/PIE world changed")); }
        if(RequiresForeignWriterAdmission&&WorldCaptured&&Cubeless::UE58::ChaosLearning::HasActiveSessionForWorld(World.Get())) { return Fail(TEXT("learning_session_conflict"),TEXT("An active or unresolved GA evaluation owns this world")); }
        if(RequiresForeignWriterAdmission&&WorldCaptured&&Cubeless::UE58::ChaosDestructionPerformance::HasActiveSessionForWorld(World.Get())) { return Fail(TEXT("performance_session_conflict"),TEXT("A pending GB performance session owns resources in this world")); }
        for(const auto& T:Tracked)
        {
            const UObject* O=T.Weak.Get();if(!Live(O)||!Live(T.Package.Get())||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)||!T.Package->GetName().Equals(T.PackagePath,ESearchCase::CaseSensitive)||(T.ExpectedParentCaptured&&(!Live(T.ExpectedParent.Get())||!O->IsIn(T.ExpectedParent.Get())))) { return Fail(TEXT("target_identity_lost"),TEXT("Captured object class/outer/package/path or expected parent changed")); }
            if(T.ComponentRestCaptured) { const auto* C=Cast<UGeometryCollectionComponent>(O);if(!C||C->GetRestCollection()!=T.ComponentRest.Get()) { return Fail(TEXT("component_rest_changed"),TEXT("Captured Geometry Collection rest asset reference changed")); } }
            if(T.ObjectWorldCaptured) { const UWorld* Actual=O->GetWorld();if(!Live(O)||!Live(T.ObjectWorld.Get())||Actual!=T.ObjectWorld.Get()||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)) { return Fail(TEXT("target_world_lost"),TEXT("Captured actor/component world or identity changed")); } }
        }
        return true;
    }
    TSharedPtr<FJsonObject> Snapshot(UObject* Object,bool Detailed=true)
    {
        auto J=MakeShared<FJsonObject>();if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()) { J->SetBoolField(TEXT("observation_skipped_unsafe_engine_state"),true);return J; }J->SetBoolField(TEXT("live"),Live(Object));if(!Live(Object)) { return J; }
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
        if(auto* C=Cast<UGeometryCollectionComponent>(Object)) { J->SetNumberField(TEXT("object_type"),static_cast<int32>(C->ObjectType));const bool Sim=C->IsSimulatingPhysics();if(!Guard()) { return J; }J->SetBoolField(TEXT("simulate_physics"),Sim);J->SetBoolField(TEXT("body_simulate_flag"),C->BodyInstance.bSimulatePhysics);J->SetBoolField(TEXT("notify_breaks"),C->bNotifyBreaks);J->SetBoolField(TEXT("registered"),C->IsRegistered());J->SetStringField(TEXT("rest_collection"),C->GetRestCollection()?C->GetRestCollection()->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> Fields;for(const auto& F:C->InitializationFields) { Fields.Add(MakeShared<FJsonValueString>(Live(F.Get())?F->GetPathName():FString())); }J->SetArrayField(TEXT("initialization_fields"),Fields); }
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
        auto& T=Tracked.AddDefaulted_GetRef();T.Hold.Reset(O);T.Weak=O;T.Outer=O->GetOuter();T.ExpectedParent=Parent;T.ExpectedParentCaptured=Parent!=nullptr;T.Package=O->GetPackage();T.Class=O->GetClass();T.Path=O->GetPathName();T.PackagePath=O->GetPackage()->GetName();T.Role=Role;T.ObjectWorldCaptured=O->IsA<AActor>()||O->IsA<UActorComponent>();if(T.ObjectWorldCaptured) { T.ObjectWorld=O->GetWorld(); }if(auto* C=Cast<UGeometryCollectionComponent>(O)) { T.ComponentRest=C->GetRestCollection();T.ComponentRestCaptured=true; }T.Before=Snapshot(O);R.bChanged|=Created;StructuralChangeSerial+=Created?1:0;return Guard();
    }
    bool Modify(UObject* O,const TCHAR* Stage) { if(!Guard()||!Track(O,Stage)) { return false; }Write(Stage);Count(TEXT("modify_calls"));O->Modify();return Guard(); }
    template<typename T>T* Load(const FString& Path,const TCHAR* Stage)
    {
        if(Dependency(Path)) { Fail(TEXT("dependency_rejected"),TEXT("MCP-only asset dependency rejected"));return nullptr; }SDK(Stage);Count(TEXT("load_calls"));T* O=LoadObject<T>(nullptr,*Path);if(O&&!Track(O,Stage)) { return nullptr; }if(!Guard()) { return nullptr; }return O;
    }
    void StartCall()
    {
        CallWriteSerial=WriteSerial;CallSaveStart=Saves.Num();CallStructuralChangeSerial=StructuralChangeSerial;CallBefore.Reset();const bool Detailed=Guard();for(auto& T:Tracked) { CallBefore.Add(Snapshot(T.Weak.Get(),Detailed)); }
    }
    TSharedRef<FJsonObject> SessionSnapshot()
    {
        auto J=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("cleanup_complete"),TEXT("cleanup_skipped_unsafe_lifetime"),TEXT("semantic_restore_not_atomic_rollback"),TEXT("sequence_actor_destroy_returned")}) { if(R.Data->HasField(K)) { J->SetField(K,R.Data->TryGetField(K)); } }J->SetStringField(TEXT("session_id"),SessionId);J->SetBoolField(TEXT("operation_complete"),OperationComplete);J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("error_code"),R.ErrorCode);J->SetStringField(TEXT("error"),R.Error);J->SetBoolField(TEXT("execution_mutation_attempted"),R.bMutationAttempted);J->SetBoolField(TEXT("changed"),R.bChanged);J->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);J->SetBoolField(TEXT("compile_attempted"),false);J->SetBoolField(TEXT("compiled"),false);J->SetBoolField(TEXT("save_attempted"),R.bSaveAttempted);J->SetBoolField(TEXT("saved"),R.bSaved);J->SetArrayField(TEXT("actual_mutation_prefix"),Events);J->SetArrayField(TEXT("actual_save_prefix"),Saves);J->SetObjectField(TEXT("native_call_counts"),MakeShared<FJsonObject>(*Counts));for(const TCHAR* K:{TEXT("phase"),TEXT("last_callback_frame"),TEXT("timed_out"),TEXT("wall_timeout"),TEXT("cleanup_attempted")}) { if(R.Data->HasField(K)) { J->SetField(K,R.Data->TryGetField(K)); } }return J;
    }
    FString Finish(TSharedPtr<FJsonObject> Legacy)
    {
        if(!Legacy) { Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error.IsEmpty()?TEXT("No legacy result returned"):R.Error); }
        bool LegacySuccess=false;Legacy->TryGetBoolField(TEXT("success"),LegacySuccess);const TSharedPtr<FJsonObject>* Data=nullptr;FString Assertion;if(Legacy->TryGetObjectField(TEXT("data"),Data)&&Data&&Data->IsValid()) { (*Data)->TryGetStringField(TEXT("assertion_status"),Assertion); }
        bool CallChanged=StructuralChangeSerial>CallStructuralChangeSerial;TArray<TSharedPtr<FJsonValue>> Observations;
        for(int32 I=0;I<Tracked.Num();++I)
        {
            auto& T=Tracked[I];auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("role"),T.Role);V->SetStringField(TEXT("captured_path"),T.Path);const auto Before=CallBefore.IsValidIndex(I)?CallBefore[I]:T.Before;V->SetObjectField(TEXT("before"),Before);
            const bool Detailed=Guard();auto After=Snapshot(T.Weak.Get(),Detailed);const bool Complete=Detailed&&Guard();V->SetObjectField(TEXT("after"),After);V->SetBoolField(TEXT("captured_projection_comparison_complete"),Complete);
            bool RawChanged=false;const bool RawAvailable=After->HasField(TEXT("live"));V->SetBoolField(TEXT("raw_identity_or_dirty_observation_available"),RawAvailable);if(RawAvailable) { for(const TCHAR* K:{TEXT("path"),TEXT("class"),TEXT("outer"),TEXT("package")}) { FString A,B;const bool HasA=Before->TryGetStringField(K,A),HasB=After->TryGetStringField(K,B);RawChanged|=HasA!=HasB||(HasA&&!A.Equals(B,ESearchCase::CaseSensitive)); }for(const TCHAR* K:{TEXT("live"),TEXT("package_dirty")}) { bool A=false,B=false;const bool HasA=Before->TryGetBoolField(K,A),HasB=After->TryGetBoolField(K,B);RawChanged|=HasA!=HasB||(HasA&&A!=B); }
            }V->SetBoolField(TEXT("raw_identity_or_dirty_projection_changed"),RawChanged);CallChanged|=RawChanged;if(Complete) { const bool Same=Json(Before).Equals(Json(After),ESearchCase::CaseSensitive);V->SetBoolField(TEXT("captured_projection_unchanged"),Same);CallChanged|=!Same; }Observations.Add(MakeShared<FJsonValueObject>(V));
        }
        if(!LegacySuccess&&R.Error.IsEmpty()) { FString E;Legacy->TryGetStringField(TEXT("error"),E);Fail(TEXT("operation_failed"),E.IsEmpty()?TEXT("Legacy operation failed"):E); }if(Assertion.Equals(TEXT("FAIL"),ESearchCase::CaseSensitive)&&R.Error.IsEmpty()) { Fail(TEXT("assertion_failed"),TEXT("Legacy session assertion_status is FAIL")); }
        R.bChanged|=CallChanged;FResult Out=R;Out.Data=MakeShared<FJsonObject>(*R.Data);Out.bMutationAttempted=WriteSerial>CallWriteSerial;Out.bChanged=CallChanged;Out.bSaveAttempted=Saves.Num()>CallSaveStart;Out.bSaved=false;
        TArray<TSharedPtr<FJsonValue>> NewSaves;for(int32 I=CallSaveStart;I<Saves.Num();++I) { NewSaves.Add(Saves[I]);bool Saved=false;if(Saves[I].IsValid()&&Saves[I]->AsObject().IsValid()) { Saves[I]->AsObject()->TryGetBoolField(TEXT("SDK_save_returned"),Saved); }Out.bSaved|=Saved; }
        Out.bSuccess=LegacySuccess&&R.Error.IsEmpty();Out.bPostconditionPassed=Out.bSuccess;Out.Data->SetBoolField(TEXT("operation_complete"),OperationComplete);Out.Data->SetStringField(TEXT("session_id"),SessionId);TArray<TSharedPtr<FJsonValue>> NewEvents;for(const auto& E:Events) { double I=-1;if(E.IsValid()&&E->AsObject().IsValid()) { E->AsObject()->TryGetNumberField(TEXT("index"),I); }if(I>=static_cast<double>(CallWriteSerial)) { NewEvents.Add(E); } }Out.Data->SetArrayField(TEXT("actual_mutation_prefix"),NewEvents);Out.Data->SetObjectField(TEXT("session_execution"),SessionSnapshot());Out.Data->SetObjectField(TEXT("legacy_result"),Legacy);Out.Data->SetArrayField(TEXT("actual_save_prefix"),NewSaves);Out.Data->SetArrayField(TEXT("captured_object_observations"),Observations);Out.Data->SetBoolField(TEXT("global_change_state_unknown"),!Out.bChangeStateKnown);
        Out.Data->SetStringField(TEXT("receipt_scope"),TEXT("Top-level mutation/save flags describe this API request; session_execution retains earlier ticker execution and actual save true history. Returned snapshots do not asynchronously update Python journals."));if(Out.bSuccess) { Out.Stage=OperationComplete?TEXT("complete"):TEXT("scheduled_or_in_progress"); }else if(!FailedStage.IsEmpty()) { Out.Stage=FailedStage; }return Cubeless::Operations::EncodeVariableResult(Out,*Operation);
    }
};
FWork* Work=nullptr;
TSharedPtr<FWork> CallOwner;
bool Guard() { return !Work||Work->Guard(); }
void SDK(const TCHAR* S) { if(Work) { Work->SDK(S); } }
void Count(const TCHAR* S) { if(Work) { Work->Count(S); } }
void Write(const TCHAR* S) { if(Work) { Work->Write(S); } }
bool Restoring=false;
template<typename F> bool Step(const TCHAR* Stage,F&& Body,bool Mutating=true)
{
    if(!Guard()||(!Restoring&&!Work->R.Error.IsEmpty())) { return false; }if(Mutating) { Write(Stage); }else { SDK(Stage); }Count(Stage);Body();return Guard();
}
struct FLegacy
{
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& E) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),E);if(Work) { Work->Fail(TEXT("operation_failed"),E); }return J; }
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),true);if(Data) { J->SetObjectField(TEXT("data"),Data); }return J; }
};

constexpr TCHAR AuthoringRoot[]=TEXT("/Game/_MCP_Temp/ChaosDestructionLearning/Cinematic");

int32 CastI32(double V,const TCHAR* Field)
{
    if(!FMath::IsFinite(V)||V<=static_cast<double>(MIN_int32)-1.0||V>=static_cast<double>(MAX_int32)+1.0) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely convert to int32"),Field));return 0; }return static_cast<int32>(V);
}
float CastFloat(double V,const TCHAR* Field) { if(!FMath::IsFinite(V)||FMath::Abs(V)>static_cast<double>(TNumericLimits<float>::Max())) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely convert to finite float32"),Field));return 0.f; }return static_cast<float>(V); }
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


TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Vector)
{
    return {
        MakeShared<FJsonValueNumber>(Vector.X),
        MakeShared<FJsonValueNumber>(Vector.Y),
        MakeShared<FJsonValueNumber>(Vector.Z)};
}

TArray<TSharedPtr<FJsonValue>> IntArrayJson(const TArray<int32>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (const int32 Value : Values)
    {
        Result.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return Result;
}

TArray<TSharedPtr<FJsonValue>> BoolArrayJson(const TArray<bool>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (const bool Value : Values)
    {
        Result.Add(MakeShared<FJsonValueBoolean>(Value));
    }
    return Result;
}

bool TryVector(const TSharedPtr<FJsonObject>& Params, const TCHAR* Key, FVector& OutValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(Key, Values) || !Values || Values->Num() != 3)
    {
        return false;
    }
    OutValue = FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
    return !OutValue.ContainsNaN();
}

TArray<double> GetNumberArray(const TSharedPtr<FJsonObject>& Params, const TCHAR* Key, const TArray<double>& Default)
{
    TArray<double> Result;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Params.IsValid() && Params->TryGetArrayField(Key, Values) && Values)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            double Number = 0.0;
            if (Value.IsValid() && Value->TryGetNumber(Number))
            {
                Result.Add(Number);
            }
        }
    }
    return Result.IsEmpty() ? Default : Result;
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

FVector RestTransformGeometryWorldCenter(
    const FGeometryCollection& Rest,
    const TArray<FTransform>& Globals,
    const int32 TransformIndex,
    const FTransform& ComponentTransform)
{
    FBox DescendantBounds(EForceInit::ForceInit);
    for (int32 Candidate = 0;
        Candidate < Rest.NumElements(FGeometryCollection::TransformGroup);
        ++Candidate)
    {
        int32 Cursor = Candidate;
        bool bDescendant = Cursor == TransformIndex;
        while (!bDescendant && Rest.Parent.IsValidIndex(Cursor) && Rest.Parent[Cursor] != INDEX_NONE)
        {
            Cursor = Rest.Parent[Cursor];
            bDescendant = Cursor == TransformIndex;
        }
        if (!bDescendant || !Globals.IsValidIndex(Candidate) ||
            !Rest.TransformToGeometryIndex.IsValidIndex(Candidate)) continue;
        const int32 GeometryIndex = Rest.TransformToGeometryIndex[Candidate];
        if (!Rest.BoundingBox.IsValidIndex(GeometryIndex)) continue;
        const FBox ComponentSpaceBounds = Rest.BoundingBox[GeometryIndex].TransformBy(Globals[Candidate]);
        DescendantBounds += ComponentSpaceBounds.TransformBy(ComponentTransform);
    }
    return DescendantBounds.IsValid
        ? DescendantBounds.GetCenter()
        : ComponentTransform.TransformPosition(
            Globals.IsValidIndex(TransformIndex)
                ? Globals[TransformIndex].GetLocation() : FVector::ZeroVector);
}
TArray<TWeakObjectPtr<UGeometryCollectionComponent>> ActiveCinematicRecordComponents;
bool TickNativeCacheRecord(float);
bool TickNativeCacheValidation(float);
bool SaveAssetPackageDirect(UObject* Asset);
int32 RoundI32(double V,const TCHAR* Field) { const double Rounded=std::floor(V+0.5);if(!FMath::IsFinite(V)||Rounded<MIN_int32||Rounded>MAX_int32) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely round to int32"),Field));return 0; }return static_cast<int32>(Rounded); }
int64 Quantize(double V) { if(!FMath::IsFinite(V)||V<=-9223372036854775808.0||V>=9223372036854775808.0) { Work->Fail(TEXT("sample_numeric_range"),TEXT("Observed transform cannot safely quantize to int64"));return 0; }return FMath::RoundToInt64(V); }
bool RestReadable(const UGeometryCollection* Asset)
{
    if(!Asset||!Work->Track(const_cast<UGeometryCollection*>(Asset),TEXT("captured_rest_asset"))) { return false; }const auto C=Asset->GetGeometryCollection();if(!C.IsValid()) { return Work->Fail(TEXT("rest_unavailable"),TEXT("Geometry Collection rest data is unavailable")); }
    const int32 N=C->NumElements(FGeometryCollection::TransformGroup);if(N<0||C->Transform.Num()!=N||C->Parent.Num()!=N||C->Children.Num()!=N||C->SimulationType.Num()!=N||C->TransformToGeometryIndex.Num()!=N) { return Work->Fail(TEXT("rest_schema_invalid"),TEXT("Rest collection transform group arrays are inconsistent")); }
    TArray<uint8> Colors;Colors.Init(0,N);for(int32 I=0;I<N;++I) { int32 K=I;while(K!=INDEX_NONE&&C->Parent.IsValidIndex(K)&&Colors[K]==0) { Colors[K]=1;K=C->Parent[K]; }if(K!=INDEX_NONE&&(!C->Parent.IsValidIndex(K)||Colors[K]==1)) { return Work->Fail(TEXT("rest_parent_invalid"),TEXT("Rest collection parent index or cycle is invalid")); }K=I;while(K!=INDEX_NONE&&C->Parent.IsValidIndex(K)&&Colors[K]==1) { Colors[K]=2;K=C->Parent[K]; } }return Guard();
}
bool CaptureCollection(UChaosCacheCollection* C,const TCHAR* Role)
{
    if(!C) { return true; }if(!Work->Track(C,Role)) { return false; }const TArray<UChaosCache*> Copy=C->GetCaches();for(auto* Cache:Copy) { if(Cache&&!Work->Track(Cache,Role,C)) { return false; } }return Guard();
}
bool CaptureTargets(AChaosCacheManager* M,UChaosCacheCollection* C,const TArray<UGeometryCollectionComponent*>& Components)
{
    if(!Work->Track(Work->World.Get(),TEXT("captured_session_world"))||!Work->Track(M,TEXT("session_manager"))||!CaptureCollection(C,TEXT("session_cache_collection"))||!CaptureCollection(M->CacheCollection,TEXT("previous_manager_collection"))) { return false; }
    const TArray<FObservedComponent> Previous=M->GetObservedComponents();for(const auto& O:Previous) { UPrimitiveComponent* P=nullptr;if(!Step(TEXT("capture_original_observed"),[&]{P=O.GetComponent(M);},false)||(P&&!Work->Track(P,TEXT("original_observed_component"),P->GetOwner()))) { return false; } }
    TSet<UGeometryCollectionComponent*> Seen;for(auto* P:Components)
    {
        if(Seen.Contains(P)) { return Work->Fail(TEXT("duplicate_component"),TEXT("The same Geometry Collection component cannot be recorded/validated twice")); }Seen.Add(P);if(!Work->Track(P,TEXT("session_geometry_component"),P?P->GetOwner():nullptr)||P->GetWorld()!=Work->World.Get()||!RestReadable(P->GetRestCollection())) { return false; }
        const auto Fields=P->InitializationFields;for(const auto& F:Fields) { auto* A=const_cast<AFieldSystemActor*>(F.Get());if(!Live(A)||!Work->Track(A,TEXT("initialization_field_actor"))) { return false; }auto* FC=A->GetFieldSystemComponent();if(!Work->Track(FC,TEXT("initialization_field_component"),A)) { return false; }const auto Commands=FC->ConstructionCommands;if(Commands.TargetNames.Num()!=Commands.RootNodes.Num()||Commands.TargetNames.Num()!=Commands.MetaDatas.Num()) { return Work->Fail(TEXT("field_command_schema_invalid"),TEXT("Field command name/node/metadata arrays are not aligned")); }for(const auto& Entry:Commands.RootNodes) { auto* Node=Entry.Get();if(Node&&!Work->Track(Node,TEXT("initialization_field_node"))) { return false; } }for(const auto& Entry:Commands.MetaDatas) { auto* Meta=Entry.Get();if(Meta&&!Work->Track(Meta,TEXT("initialization_field_metadata"))) { return false; } } }
    }return Guard();
}
bool FieldBounds(const UFieldSystemComponent* C,FBox& Out)
{
    if(!Live(C)||!Guard()) { return false; }const auto Commands=C->ConstructionCommands;if(Commands.TargetNames.IsEmpty()) { return true; }if(Commands.TargetNames.Num()!=Commands.RootNodes.Num()||Commands.TargetNames.Num()!=Commands.MetaDatas.Num()||!Commands.RootNodes.IsValidIndex(0)||!Commands.MetaDatas.IsValidIndex(0)) { return Work->Fail(TEXT("field_command_schema_invalid"),TEXT("Field command arrays cannot safely build index0")); }
    for(const auto& N:Commands.RootNodes) { if(N&&!Work->Track(N.Get(),TEXT("field_bounds_node"))) { return false; } }for(const auto& M:Commands.MetaDatas) { if(M&&!Work->Track(M.Get(),TEXT("field_bounds_metadata"))) { return false; } }return Step(TEXT("build_copied_field_command_bounds"),[&]{Out+=Commands.BuildFieldCommand(0).BoundingBox;},false);
}
bool PreflightRecord(const TSharedPtr<FJsonObject>& P)
{
    const double Rate=GetNumber(P,TEXT("frame_rate"),30),DurationSeconds=GetNumber(P,TEXT("duration_seconds"),2),Timeout=GetNumber(P,TEXT("timeout_seconds"),10);CastI32(Rate*DurationSeconds,TEXT("frame_rate*duration_seconds"));
    for(const TCHAR* K:{TEXT("initial_trigger_propagation_depth"),TEXT("initial_trigger_delay_frames"),TEXT("per_advance_breaks_allowed"),TEXT("per_advance_breaks_reschedule_limit"),TEXT("particle_release_throttling_min_count"),TEXT("particle_release_throttling_max_count"),TEXT("initial_trigger_root_transform_index")}) { CastI32(GetNumber(P,K,0),K); }
    CastFloat(GetNumber(P,TEXT("initial_trigger_radius"),100),TEXT("initial_trigger_radius"));FVector V(300,0,600);TryVector(P,TEXT("motion_velocity"),V);CastFloat(V.X*10000,TEXT("motion_velocity.x impulse"));CastFloat(V.Y*10000,TEXT("motion_velocity.y impulse"));CastFloat(V.Z*10000,TEXT("motion_velocity.z impulse"));
    if(!FMath::IsFinite(Rate)||!FMath::IsFinite(DurationSeconds)||!FMath::IsFinite(Timeout)||!FMath::IsFinite(FPlatformTime::Seconds()+FMath::Max(DurationSeconds+2,Timeout))) { return Work->Fail(TEXT("numeric_range"),TEXT("Finite recording duration/frame rate/deadline required")); }return Work->R.Error.IsEmpty()&&Guard();
}
bool PreflightValidation(const TSharedPtr<FJsonObject>& P)
{
    const int32 Rate=FMath::Max(1,CastI32(GetNumber(P,TEXT("sample_frame_rate"),30),TEXT("sample_frame_rate")));CastI32(GetNumber(P,TEXT("repeat_count"),2),TEXT("repeat_count"));for(double V:GetNumberArray(P,TEXT("scrub_frames"),{0,15,30,45,60})) { CastI32(V,TEXT("scrub_frames")); }for(double V:GetNumberArray(P,TEXT("sample_times"),{.25,.5,1})) { CastFloat(V,TEXT("sample_times"));RoundI32(V*Rate,TEXT("sample_time_frame")); }
    const double Timeout=GetNumber(P,TEXT("timeout_seconds"),10);if(!FMath::IsFinite(Timeout)||!FMath::IsFinite(FPlatformTime::Seconds()+Timeout)) { return Work->Fail(TEXT("numeric_range"),TEXT("Finite validation deadline required")); }return Work->R.Error.IsEmpty()&&Guard();
}
bool SaveAssetPackageDirect(UObject* Asset)
{
    Work->R.bSaveRequested=true;if(!Guard()||!Work->Track(Asset,TEXT("direct_saved_cache"))) { return false; }TStrongObjectPtr<UPackage> Package(Asset->GetPackage());const FString Before=Package->GetName(),Filename=FPackageName::LongPackageNameToFilename(Before,FPackageName::GetAssetPackageExtension());
    if(!Step(TEXT("mark_cache_package_dirty"),[&]{Package->MarkPackageDirty();})) { return false; }FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.Error=GError;bool Saved=false;Write(TEXT("direct_save_cache_package"));Work->R.bSaveAttempted=true;Count(TEXT("explicit_save_calls"));Saved=UPackage::SavePackage(Package.Get(),Asset,*Filename,Args);Work->R.bSaved|=Saved;Count(TEXT("explicit_save_returns"));
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("captured_package_before"),Before);J->SetStringField(TEXT("filename"),Filename);J->SetBoolField(TEXT("SDK_save_returned"),Saved);J->SetBoolField(TEXT("captured_package_live"),Live(Package.Get()));if(Live(Package.Get())) { J->SetStringField(TEXT("captured_package_after"),Package->GetName());J->SetBoolField(TEXT("captured_package_dirty_after"),Package->IsDirty());J->SetBoolField(TEXT("captured_package_name_unchanged"),Package->GetName().Equals(Before,ESearchCase::CaseSensitive)); }Work->Saves.Add(MakeShared<FJsonValueObject>(J));J->SetBoolField(TEXT("captured_target_identity_after_save"),Guard());return Saved;
}
bool AdoptSessionTargets(const TSharedPtr<FWork>& Evidence)
{
    if(!Evidence||!Evidence->Guard()||!Guard()) { return false; }if(Work==Evidence.Get()) { return true; }for(const auto& T:Evidence->Tracked) { if(!Work->Track(T.Weak.Get(),TEXT("adopted_previous_session_target"),T.ExpectedParentCaptured?T.ExpectedParent.Get():nullptr)) { return false; } }return Guard();
}

struct FOriginalComponentFlags
{
    TWeakObjectPtr<UPrimitiveComponent> Component;bool Simulating=false,BodySimulate=false,NotifyBreaks=false,Registered=false;EObjectStateTypeEnum ObjectType=EObjectStateTypeEnum::Chaos_Object_Dynamic;
};
bool CaptureOriginalFlags(AChaosCacheManager* M,const TArray<UGeometryCollectionComponent*>& Requested,TArray<FOriginalComponentFlags>& Out)
{
    TArray<UPrimitiveComponent*> All;for(auto* C:Requested) { All.AddUnique(C); }const TArray<FObservedComponent> Copy=M->GetObservedComponents();for(const auto& O:Copy) { UPrimitiveComponent* C=nullptr;if(!Step(TEXT("capture_entry_observed_component"),[&]{C=O.GetComponent(M);},false)) { return false; }if(C) { All.AddUnique(C); } }
    Out.Reset();for(auto* C:All) { if(!Work->Track(C,TEXT("captured_entry_component_flags"),C->GetOwner())) { return false; }FOriginalComponentFlags F;F.Component=C;if(!Step(TEXT("capture_entry_simulation_flag"),[&]{F.Simulating=C->IsSimulatingPhysics();},false)) { return false; }F.BodySimulate=C->BodyInstance.bSimulatePhysics;F.Registered=C->IsRegistered();if(auto* G=Cast<UGeometryCollectionComponent>(C)) { F.NotifyBreaks=G->bNotifyBreaks;F.ObjectType=G->ObjectType; }Out.Add(F); }return Guard();
}
bool RestoreOriginalFlags(const TArray<FOriginalComponentFlags>& Flags,const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Skip)
{
    bool Complete=true;for(const auto& F:Flags) { auto* C=F.Component.Get();bool Skipped=false;for(const auto& K:Skip) { Skipped|=K.Get()==C; }if(Skipped) { continue; }if(!Live(C)||!Guard()) { return false; }
        bool Actual=false;if(!Step(TEXT("read_original_observed_simulation_before_restore"),[&]{Actual=C->IsSimulatingPhysics();},false)) { return false; }if(Actual!=F.Simulating||C->BodyInstance.bSimulatePhysics!=F.BodySimulate) { if(!Step(TEXT("restore_entry_observed_simulation"),[&]{C->SetSimulatePhysics(F.Simulating);})) { return false; }if(!Step(TEXT("restore_entry_observed_body_simulation_flag"),[&]{C->BodyInstance.bSimulatePhysics=F.BodySimulate;})) { return false; } }
        if(auto* G=Cast<UGeometryCollectionComponent>(C)) { if(G->bNotifyBreaks!=F.NotifyBreaks&&!Step(TEXT("restore_entry_observed_notify"),[&]{G->SetNotifyBreaks(F.NotifyBreaks);})) { return false; }if(G->ObjectType!=F.ObjectType&&!Step(TEXT("restore_entry_observed_object_type"),[&]{G->ObjectType=F.ObjectType;})) { return false; }Complete&=G->bNotifyBreaks==F.NotifyBreaks&&G->ObjectType==F.ObjectType; }
        if(!Step(TEXT("read_entry_observed_restoration"),[&]{Actual=C->IsSimulatingPhysics();},false)) { return false; }Complete&=Actual==F.Simulating&&C->BodyInstance.bSimulatePhysics==F.BodySimulate&&C->IsRegistered()==F.Registered;
    }return Complete;
}
struct FNativeCacheRecordSession
{
    FString SessionId;
    TSharedPtr<FWork> Evidence;
    TArray<FOriginalComponentFlags> OriginalComponentFlags;
    double MonotonicStart=0.0;
    bool bCleanupAttempted=false,bManagerTouched=false;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AChaosCacheManager> Manager;
    TWeakObjectPtr<UChaosCacheCollection> Collection;
    TWeakObjectPtr<UChaosCacheCollection> OriginalManagerCacheCollection;
    ECacheMode OriginalManagerCacheMode = ECacheMode::None;
    EStartMode OriginalManagerStartMode = EStartMode::Timed;
    TArray<FObservedComponent> OriginalObservedComponents;
    bool bHasOriginalManagerState = false;
    TWeakObjectPtr<AChaosSolverActor> Solver;
    FChaosSolverConfiguration OriginalSolverConfiguration;
    bool bHasSolverConfiguration = false;
    TArray<TWeakObjectPtr<UGeometryCollectionComponent>> Components;
    TArray<FString> ComponentActorNames;
    TArray<FString> AppliedCacheNames;
    FTSTicker::FDelegateHandle TickerHandle;
    FString ConfigurationPhase = TEXT("end_evaluate_quiescence");
    FString ConfigurationFailure;
    uint64 RequestFrame = 0;
    uint64 EndEvaluateFrame = 0;
    uint64 ManagerConfigureFrame = 0;
    uint64 PhysicsStateRecreateFrame = 0;
    uint64 PhysicsStateReadyFrame = 0;
    uint64 BeginEvaluateFrame = 0;
    uint64 InitialStrainDispatchFrame = 0;
    double RequestWorldTime = 0.0;
    uint64 BeginFrame = 0;
    double BeginWorldTime = 0.0;
    int32 TargetFrameCount = 60;
    double TargetDurationSeconds = 2.0;
    double TimeoutSeconds = 10.0;
    FVector MotionVelocity = FVector(300.0, 0.0, 600.0);
    FString InitialTriggerMode = TEXT("strain_velocity");
    int32 CapturedFrameCount = 0;
    int32 InitialExternalStrainDispatchCount = 0;
    bool bInitialStrainDispatchAttempted = false;
    int32 InitialMotionDispatchCount = 0;
    TArray<int32> DeferredBreakingItems;
    TArray<int32> DeferredBreakingRootParentIndices;
    TArray<int32> DeferredBreakingRootChildCounts;
    TArray<FVector> InitialTriggerImpactPoints;
    TArray<float> InitialTriggerRadii;
    int32 InitialTriggerPropagationDepth = 0;
    int32 InitialTriggerDelayFrames = 15;
    TArray<int32> InitialPhysicsObjectCounts;
    TArray<int32> InitialDisabledPhysicsObjectCounts;
    TArray<int32> CurrentDisabledPhysicsObjectCounts;
    TArray<TArray<int32>> InitialPhysicsObjectIds;
    TArray<TArray<int32>> InitialPhysicsObjectStates;
    TArray<TArray<bool>> InitialPhysicsObjectDisabled;
    TArray<TArray<FVector>> InitialPhysicsObjectPositions;
    TArray<TArray<FQuat>> InitialPhysicsObjectRotations;
    TArray<TArray<int32>> AnchoredTransformIndices;
    TArray<TArray<FTransform>> InitialAnchoredGlobalTransforms;
    int32 InitialAnchorSelectionPopulationCount = 0;
    int32 InitialAnchorSelectedTransformCount = 0;
    int32 InitialAnchorSelectedPhysicsBodyCount = 0;
    int32 InitialAnchorKinematicPhysicsBodyCount = 0;
    int32 InitialAnchorDynamicPhysicsBodyCount = 0;
    int32 InitialAnchorOtherPhysicsBodyCount = 0;
    double AnchorSelectionPercent = 0.0;
    double MaximumAnchoredDisplacementCm = 0.0;
    double MaximumAnchoredRotationDegrees = 0.0;
    int32 MatchedAnchorSelectedTransformCount = 0;
    int32 MatchedAnchorSelectedPhysicsBodyCount = 0;
    int32 NativeAnchorByBoxDispatchCount = 0;
    int32 NativeAnchorByIndexDispatchCount = 0;
    int32 NativeAnchorKinematicFieldDispatchCount = 0;
    int32 NativeAnchorForceKinematicObjectCount = 0;
    int32 NativeAnchorRemovalCount = 0;
    int32 ReleasedDynamicPhysicsObjectCount = 0;
    double MaximumReleasedDynamicDisplacementCm = 0.0;
    TArray<bool> ComponentReleaseObserved;
    uint64 DeferredBreakingDispatchFrame = 0;
    uint64 ReleaseObservedFrame = 0;
    uint64 NativeAnchorDispatchFrame = 0;
    int32 DeferredBreakingDispatchFrameDelta = INDEX_NONE;
    bool bDeferredBreakingVelocityDispatched = false;
    bool bBreakingVelocityArmedBeforeRelease = false;
    bool bPostReleaseImpulseDispatched = false;
    bool bReleaseObservedBeforeMotion = false;
    int32 RadialImpulseDispatchCount = 0;
    int32 UniformVelocityFieldDispatchCount = 0;
    int32 LocalizedImpulseDispatchCount = 0;
    int32 PhysicsStateRecreateCount = 0;
    int32 DynamicCollectionResetCount = 0;
    int32 ComponentRegisterCount = 0;
    int32 ComponentUnregisterOnRestoreCount = 0;
    int32 SimulatePhysicsEnableCount = 0;
    int32 SimulatePhysicsRestoreCount = 0;
    int32 PhysicsStateDestroyOnRestoreCount = 0;
    int32 PhysicsProxyReadyCount = 0;
    TArray<bool> ComponentTouched;
    TArray<bool> OriginalComponentRegisteredStates;
    TArray<bool> OriginalSimulatePhysicsStates,OriginalBodySimulateStates,OriginalNotifyBreaksStates;
    TArray<EObjectStateTypeEnum> OriginalObjectTypes;
    TArray<TArray<TObjectPtr<const AFieldSystemActor>>> OriginalInitializationFields;
    TArray<FBox> NativeAnchorBounds;
    int32 InitializationFieldSuppressCount = 0;
    int32 InitializationFieldRestoreCount = 0;
    int32 AuthoredInitializationFieldRedispatchCount = 0;
    int32 ObjectTypeDynamicConfigureCount = 0;
    int32 ObjectTypeRestoreCount = 0;
    TArray<int32> PhysicsObjectCountsBeforeRecreate;
    TArray<int32> PhysicsObjectCountsAfterRecreate;
    bool bManagerConfigured = false;
    bool bPhysicsStateReady = false;
    bool bComponentSimulationSettingsRestored = false;
    bool bComponentRegistrationSettingsRestored = false;
    bool bRecordingActive = false;
    bool bRequireSavedCacheTransformVariation = false;
    bool bSavedCacheTransformVariation = false;
    bool bPreserveInitializationFieldsDuringRecord = false;
    bool bUseNativeAfterReleaseAnchorFallback = true;
    TSet<FString> ObservedTransformHashes;
    bool bCachePackageDirtyBefore = false;
    bool bReadyToFinish = false;
    bool bTimedOut = false;
    bool bSettingsRestored = false;
    bool bUniqueCacheNames = false;
    bool bCacheSaved = false;
    bool bCleanupComplete = false;
};

struct FNativeCacheValidationSession
{
    FString SessionId;
    TSharedPtr<FWork> Evidence;
    TArray<FOriginalComponentFlags> OriginalComponentFlags;
    double MonotonicStart=0.0;
    bool bCleanupAttempted=false,bManagerTouched=false;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AChaosCacheManager> Manager;
    TWeakObjectPtr<UChaosCacheCollection> Collection;
    TWeakObjectPtr<UChaosCacheCollection> OriginalManagerCacheCollection;
    ECacheMode OriginalManagerCacheMode = ECacheMode::None;
    EStartMode OriginalManagerStartMode = EStartMode::Timed;
    TArray<FObservedComponent> OriginalObservedComponents;
    bool bHasOriginalManagerState = false;
    TArray<TWeakObjectPtr<UGeometryCollectionComponent>> Components;
    TWeakObjectPtr<ULevelSequencePlayer> SequencePlayer;
    TWeakObjectPtr<ALevelSequenceActor> SequenceActor;
    FTSTicker::FDelegateHandle TickerHandle;
    TArray<int32> ScheduledFrames;
    TArray<FString> ScheduledPasses;
    TArray<double> RequestedSampleTimes;
    TArray<int32> SampleTimeFrames;
    TArray<FString> CapturedHashes;
    TArray<FString> CapturedSolverDiagnosticHashes;
    TArray<double> CapturedManagerStartTimes;
    TArray<int32> CapturedPlayerFrames;
    int32 ScheduleIndex = 0;
    uint64 DispatchFrame = 0;
    int32 RequiredSettleFrameDelta = 2;
    int32 MinimumSuppressedCount = TNumericLimits<int32>::Max();
    bool bEndpointPrimePending = false;
    int32 EndpointFrame = 60;
    int32 EndpointPrimeFrame = 59;
    bool bUsedSequencePlayer = false;
    bool bSequenceBindingValidated = false;
    bool bSequenceMode = false;
    bool bForwardReverseRandomScheduled = false;
    bool bReadyToFinish = false;
    bool bTimedOut = false;
    bool bCleanupComplete = false;
    uint64 BeginFrame = 0;
    double BeginWorldTime = 0.0;
    double TimeoutSeconds = 10.0;
};

TSharedPtr<FNativeCacheRecordSession> ActiveNativeRecordSession;
TSharedPtr<FNativeCacheValidationSession> ActiveNativeValidationSession;

template<typename T> bool TargetOverlap(const TSharedPtr<T>& Other,AChaosCacheManager* M,const TArray<UGeometryCollectionComponent*>& Components,const FString& CachePath)
{
    if(!Other||Other->bCleanupComplete) { return false; }if(Other->World.Get()==Work->World.Get()) { return true; }if(Other->Manager.Get()==M) { return true; }if(auto* C=Other->Collection.Get()) { if(C->GetPackage()->GetName().Equals(FPackageName::ObjectPathToPackageName(CachePath),ESearchCase::IgnoreCase)) { return true; } }for(auto* C:Components) { for(const auto& O:Other->Components) { if(O.Get()==C) { return true; } } }return false;
}
void DispatchNativeValidationFrame(FNativeCacheValidationSession& Session)
{
    if (!Session.ScheduledFrames.IsValidIndex(Session.ScheduleIndex))
    {
        return;
    }
    const int32 Frame = Session.ScheduledFrames[Session.ScheduleIndex];
    if (ULevelSequencePlayer* Player = Session.SequencePlayer.Get())
    {
        const int32 EvaluationFrame = Frame == Session.EndpointFrame
            ? Session.EndpointPrimeFrame : Frame;
        FMovieSceneSequencePlaybackParams PlaybackParams(FFrameTime(EvaluationFrame), EUpdatePositionMethod::Jump);
        PlaybackParams.bHasJumped = true;
        if(!Step(TEXT("SetPlaybackPosition"),[&]{Player->SetPlaybackPosition(PlaybackParams);})) { return; }
        Session.bEndpointPrimePending = Frame == Session.EndpointFrame;
    }
    Session.DispatchFrame = GFrameCounter;
}

struct FRestoreEvidenceScope
{
    FWork* Caller;TSharedPtr<FWork> Evidence;int32 EventStart,SaveStart;TSharedRef<FJsonObject> CountsBefore;
    explicit FRestoreEvidenceScope(const TSharedPtr<FWork>& E):Caller(Work),Evidence(E),EventStart(Work->Events.Num()),SaveStart(Work->Saves.Num()),CountsBefore(MakeShared<FJsonObject>(*Work->Counts)) {}
    ~FRestoreEvidenceScope()
    {
        if(!Evidence||Evidence.Get()==Caller) { return; }for(int32 I=EventStart;I<Caller->Events.Num();++I) { auto J=MakeShared<FJsonObject>(*Caller->Events[I]->AsObject());J->SetNumberField(TEXT("source_request_event_index"),I);J->SetNumberField(TEXT("index"),Evidence->Events.Num());J->SetStringField(TEXT("origin"),TEXT("explicit_request_cleanup"));Evidence->Events.Add(MakeShared<FJsonValueObject>(J));++Evidence->WriteSerial;Evidence->R.bMutationAttempted=true; }
        for(int32 I=SaveStart;I<Caller->Saves.Num();++I) { Evidence->Saves.Add(Caller->Saves[I]);bool Saved=false;Caller->Saves[I]->AsObject()->TryGetBoolField(TEXT("SDK_save_returned"),Saved);Evidence->R.bSaved|=Saved;Evidence->R.bSaveAttempted=true; }
        for(const auto& Pair:Caller->Counts->Values) { const FString Key(Pair.Key);double Before=0,Previous=0;CountsBefore->TryGetNumberField(Key,Before);Evidence->Counts->TryGetNumberField(Key,Previous);Evidence->Counts->SetNumberField(Key,Previous+FMath::Max(0.0,Pair.Value->AsNumber()-Before)); }
        Evidence->R.bChangeStateKnown&=Caller->R.bChangeStateKnown;for(const TCHAR* K:{TEXT("cleanup_complete"),TEXT("cleanup_skipped_unsafe_lifetime"),TEXT("cleanup_attempted"),TEXT("semantic_restore_not_atomic_rollback")}) { if(Caller->R.Data->HasField(K)) { Evidence->R.Data->SetField(K,Caller->R.Data->TryGetField(K)); } }if(!Caller->R.Error.IsEmpty()) { Evidence->Fail(TEXT("cleanup_incomplete"),Caller->R.Error); }
    }
};
void CleanupNativeValidationSession(FNativeCacheValidationSession& Session)
{
    if(Session.TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Session.TickerHandle);Session.TickerHandle.Reset(); }
    if(Session.bCleanupAttempted) { return; }FRestoreEvidenceScope Receipt(Session.Evidence);Session.bCleanupAttempted=true;Work->OperationComplete=true;Work->R.Data->SetBoolField(TEXT("cleanup_attempted"),true);if(Session.Evidence) { Session.Evidence->OperationComplete=true; }
    if(!AdoptSessionTargets(Session.Evidence)) { Work->R.Data->SetBoolField(TEXT("cleanup_skipped_unsafe_lifetime"),true);Work->Fail(TEXT("cleanup_incomplete"),TEXT("Validation cleanup skipped unsafe Engine/world/target lifetime"));return; }
    if(!Session.bManagerTouched) { Session.bCleanupComplete=true;Work->R.Data->SetBoolField(TEXT("cleanup_complete"),true);return; }
    TGuardValue<bool> RestoreScope(Restoring,true);if(auto* Player=Session.SequencePlayer.Get()) { if(!Step(TEXT("stop_validation_player"),[&]{Player->Stop();})) { return; } }
    auto* Manager=Session.Manager.Get();if(Manager&&!Step(TEXT("EndEvaluate_validation_cleanup"),[&]{Manager->EndEvaluate();})) { return; }
    if(auto* Actor=Session.SequenceActor.Get())
    {
        TStrongObjectPtr<ALevelSequenceActor> Hold(Actor);const FString Path=Actor->GetPathName();for(int32 I=Work->Tracked.Num()-1;I>=0;--I) { auto* O=Work->Tracked[I].Weak.Get();if(O&&(O==Actor||O->IsIn(Actor))) { Work->Tracked.RemoveAt(I);if(Work->CallBefore.IsValidIndex(I)) { Work->CallBefore.RemoveAt(I); } } }bool Destroyed=false;if(!Step(TEXT("destroy_owned_validation_actor"),[&]{Destroyed=Actor->Destroy();})) { return; }Work->R.Data->SetStringField(TEXT("destroyed_sequence_actor_path"),Path);Work->R.Data->SetBoolField(TEXT("sequence_actor_destroy_returned"),Destroyed);if(!Destroyed) { Work->Fail(TEXT("cleanup_incomplete"),TEXT("Owned validation actor Destroy returned false"));return; }Work->R.bChanged=true;++Work->StructuralChangeSerial;
    }
    if(Manager&&Session.bHasOriginalManagerState)
    {
        if(!Step(TEXT("restore_validation_manager_observed_clear"),[&]{Manager->ClearObservedComponents(false);})) { return; }
        if(!Step(TEXT("restore_validation_manager_state"),[&]{Manager->CacheCollection=Session.OriginalManagerCacheCollection.Get();Manager->CacheMode=Session.OriginalManagerCacheMode;Manager->StartMode=Session.OriginalManagerStartMode;Manager->GetObservedComponents()=Session.OriginalObservedComponents;})) { return; }
        Session.bCleanupComplete=Manager->CacheCollection==Session.OriginalManagerCacheCollection.Get()&&Manager->CacheMode==Session.OriginalManagerCacheMode&&Manager->StartMode==Session.OriginalManagerStartMode&&Manager->GetObservedComponents().Num()==Session.OriginalObservedComponents.Num();
        const TArray<FObservedComponent> Copy=Manager->GetObservedComponents();for(int32 I=0;I<Copy.Num();++I) { UPrimitiveComponent* A=nullptr;UPrimitiveComponent* B=nullptr;if(!Step(TEXT("readback_validation_manager_restore"),[&]{A=Copy[I].GetComponent(Manager);B=Session.OriginalObservedComponents[I].GetComponent(Manager);},false)) { Session.bCleanupComplete=false;return; }Session.bCleanupComplete&=A==B&&Copy[I].CacheName==Session.OriginalObservedComponents[I].CacheName&&Copy[I].bPlaybackEnabled==Session.OriginalObservedComponents[I].bPlaybackEnabled; }
    }
    Session.bCleanupComplete&=RestoreOriginalFlags(Session.OriginalComponentFlags,{});Work->R.Data->SetStringField(TEXT("validation_simulation_scope"),TEXT("No simulation suppression is forced to pass samples; cleanup only restores captured entry flags after the measurement schedule."));Work->R.Data->SetBoolField(TEXT("cleanup_complete"),Session.bCleanupComplete);if(!Session.bCleanupComplete) { Work->Fail(TEXT("cleanup_incomplete"),TEXT("Validation manager restore readback differs")); }
}
bool ValidateManagerSequenceBinding(ULevelSequence* Sequence,AChaosCacheManager* Manager,UChaosCacheCollection* Collection,UWorld* World,int32& OutChaosTrackCount,int32& OutMatchingSectionCount)
{
    OutChaosTrackCount=0;OutMatchingSectionCount=0;UMovieScene* Scene=nullptr;if(!Sequence||!Manager||!Collection||!World||!Step(TEXT("resolve_validation_scene"),[&]{Scene=Sequence->GetMovieScene();},false)||!Work->Track(Scene,TEXT("validation_movie_scene"),Sequence)) { return false; }
    FGuid Id;if(!Step(TEXT("find_validation_manager_binding"),[&]{Id=Sequence->FindBindingFromObject(Manager,World);},false)) { return false; }const auto* Binding=Id.IsValid()?Scene->FindBinding(Id):nullptr;if(!Binding) { return false; }const TArray<UMovieSceneTrack*> Tracks=Binding->GetTracks();
    for(auto* Base:Tracks) { if(auto* Track=Cast<UMovieSceneChaosCacheTrack>(Base)) { if(!Work->Track(Track,TEXT("validation_chaos_track"),Scene)) { return false; }++OutChaosTrackCount;TArray<UMovieSceneSection*> Sections;if(!Step(TEXT("read_validation_chaos_sections"),[&]{Sections=Track->GetAllSections();},false)) { return false; }for(auto* BaseSection:Sections) { if(auto* Section=Cast<UMovieSceneChaosCacheSection>(BaseSection)) { if(!Work->Track(Section,TEXT("validation_chaos_section"),Track)) { return false; }OutMatchingSectionCount+=Section->Params.CacheCollection==Collection?1:0; } } } }return OutChaosTrackCount==1&&OutMatchingSectionCount>0;
}

void ApplySolverConfiguration(AChaosSolverActor* Solver, const FChaosSolverConfiguration& Configuration)
{
    if (!Solver)
    {
        return;
    }
    if(!Work->Modify(Solver,TEXT("modify_record_solver"))) { return; }if(!Step(TEXT("assign_record_solver_configuration"),[&]{Solver->Properties=Configuration;})) { return; }
#if WITH_EDITOR
    if (FProperty* Property = FindFProperty<FProperty>(
        AChaosSolverActor::StaticClass(), GET_MEMBER_NAME_CHECKED(AChaosSolverActor, Properties)))
    {
        FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
        Event.MemberProperty = Property;
        if(!Step(TEXT("notify_record_solver_configuration"),[&]{Solver->PostEditChangeProperty(Event);})) { return; }
    }
#endif
}

void RestoreRecordSolverSettings(FNativeCacheRecordSession& Session)
{
    if(Session.bSettingsRestored||!Session.bHasSolverConfiguration) { Session.bSettingsRestored=true;return; }auto* Solver=Session.Solver.Get();if(!Solver||!Guard()) { return; }ApplySolverConfiguration(Solver,Session.OriginalSolverConfiguration);if(!Guard()) { return; }
    FString Expected,Actual;FChaosSolverConfiguration::StaticStruct()->ExportText(Expected,&Session.OriginalSolverConfiguration,nullptr,nullptr,PPF_None,nullptr);FChaosSolverConfiguration::StaticStruct()->ExportText(Actual,&Solver->Properties,nullptr,nullptr,PPF_None,nullptr);Session.bSettingsRestored=Expected.Equals(Actual,ESearchCase::CaseSensitive);
}
void CleanupNativeRecordSession(FNativeCacheRecordSession& Session,bool bSaveCompletedCache)
{
    if(Session.TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Session.TickerHandle);Session.TickerHandle.Reset(); }if(Session.bCleanupAttempted) { return; }FRestoreEvidenceScope Receipt(Session.Evidence);Session.bCleanupAttempted=true;Work->OperationComplete=true;Work->R.Data->SetBoolField(TEXT("cleanup_attempted"),true);if(Session.Evidence) { Session.Evidence->OperationComplete=true; }
    if(!AdoptSessionTargets(Session.Evidence)) { Work->R.Data->SetBoolField(TEXT("cleanup_skipped_unsafe_lifetime"),true);Work->Fail(TEXT("cleanup_incomplete"),TEXT("Record cleanup skipped unsafe Engine/world/target lifetime"));return; }if(!Session.bManagerTouched) { Session.bCleanupComplete=true;Session.bSettingsRestored=true;Session.bComponentSimulationSettingsRestored=true;Session.bComponentRegistrationSettingsRestored=true;Work->R.Data->SetBoolField(TEXT("cleanup_complete"),true);return; }TGuardValue<bool> RestoreScope(Restoring,true);
    auto* Manager=Session.Manager.Get();if(Manager&&!Step(TEXT("EndEvaluate_record_cleanup"),[&]{Manager->EndEvaluate();})) { return; }
    if(bSaveCompletedCache) { auto* C=Session.Collection.Get();if(!C||!Step(TEXT("flush_completed_cache_writes"),[&]{C->FlushAllCacheWrites();})) { return; }Session.bCacheSaved=SaveAssetPackageDirect(C);if(!Guard()) { return; } }
    if(Manager&&Session.bHasOriginalManagerState)
    {
        if(!Step(TEXT("clear_record_observed_before_component_restore"),[&]{Manager->ClearObservedComponents(false);})) { return; }
        if(!Step(TEXT("restore_record_manager_state"),[&]{Manager->CacheCollection=Session.OriginalManagerCacheCollection.Get();Manager->CacheMode=Session.OriginalManagerCacheMode;Manager->StartMode=Session.OriginalManagerStartMode;Manager->GetObservedComponents()=Session.OriginalObservedComponents;})) { return; }
    }
    bool SimRestored=true,RegistrationRestored=true;
    for(int32 I=0;I<Session.Components.Num();++I)
    {
        if(!Session.ComponentTouched.IsValidIndex(I)||!Session.ComponentTouched[I]) { continue; }auto* C=Session.Components[I].Get();if(!C||!Session.OriginalSimulatePhysicsStates.IsValidIndex(I)||!Session.OriginalComponentRegisteredStates.IsValidIndex(I)||!Session.OriginalObjectTypes.IsValidIndex(I)||!Session.OriginalInitializationFields.IsValidIndex(I)) { SimRestored=false;RegistrationRestored=false;continue; }
        if(Session.NativeAnchorByBoxDispatchCount>0) { if(!Step(TEXT("remove_record_native_anchors"),[&]{C->RemoveAllAnchors();})) { return; }++Session.NativeAnchorRemovalCount; }
        if(!Session.bPreserveInitializationFieldsDuringRecord) { if(!Step(TEXT("restore_record_initialization_fields"),[&]{C->InitializationFields=Session.OriginalInitializationFields[I];})) { return; }++Session.InitializationFieldRestoreCount; }
        if(!Step(TEXT("restore_record_object_type"),[&]{C->ObjectType=Session.OriginalObjectTypes[I];})) { return; }++Session.ObjectTypeRestoreCount;
        if(!Step(TEXT("restore_record_simulation"),[&]{C->SetSimulatePhysics(Session.OriginalSimulatePhysicsStates[I]);})) { return; }++Session.SimulatePhysicsRestoreCount;if(Session.OriginalBodySimulateStates.IsValidIndex(I)&&!Step(TEXT("restore_record_body_simulate_flag"),[&]{C->BodyInstance.bSimulatePhysics=Session.OriginalBodySimulateStates[I];})) { return; }if(Session.OriginalNotifyBreaksStates.IsValidIndex(I)&&C->bNotifyBreaks!=Session.OriginalNotifyBreaksStates[I]&&!Step(TEXT("restore_record_notify_breaks"),[&]{C->SetNotifyBreaks(Session.OriginalNotifyBreaksStates[I]);})) { return; }
        if(!Session.OriginalSimulatePhysicsStates[I]) { if(!Step(TEXT("destroy_record_temporary_physics"),[&]{C->DestroyPhysicsState();})) { return; }++Session.PhysicsStateDestroyOnRestoreCount; }
        if(!Session.OriginalComponentRegisteredStates[I]&&C->IsRegistered()) { if(!Step(TEXT("unregister_record_temporary_component"),[&]{C->UnregisterComponent();})) { return; }++Session.ComponentUnregisterOnRestoreCount; }
        bool Sim=false;if(!Step(TEXT("readback_record_component_restore"),[&]{Sim=C->IsSimulatingPhysics();},false)) { return; }SimRestored&=Sim==Session.OriginalSimulatePhysicsStates[I]&&C->BodyInstance.bSimulatePhysics==Session.OriginalBodySimulateStates[I]&&C->bNotifyBreaks==Session.OriginalNotifyBreaksStates[I]&&C->ObjectType==Session.OriginalObjectTypes[I]&&C->InitializationFields==Session.OriginalInitializationFields[I];RegistrationRestored&=C->IsRegistered()==Session.OriginalComponentRegisteredStates[I];
    }
    Session.bComponentSimulationSettingsRestored=SimRestored;Session.bComponentRegistrationSettingsRestored=RegistrationRestored;RestoreRecordSolverSettings(Session);if(!Guard()) { return; }
    bool ManagerRestored=!Session.bHasOriginalManagerState||(Manager&&Manager->CacheCollection==Session.OriginalManagerCacheCollection.Get()&&Manager->CacheMode==Session.OriginalManagerCacheMode&&Manager->StartMode==Session.OriginalManagerStartMode&&Manager->GetObservedComponents().Num()==Session.OriginalObservedComponents.Num());
    if(ManagerRestored&&Session.bHasOriginalManagerState) { const TArray<FObservedComponent> Copy=Manager->GetObservedComponents();for(int32 I=0;I<Copy.Num();++I) { UPrimitiveComponent* A=nullptr;UPrimitiveComponent* B=nullptr;if(!Step(TEXT("readback_record_manager_restore"),[&]{A=Copy[I].GetComponent(Manager);B=Session.OriginalObservedComponents[I].GetComponent(Manager);},false)) { return; }ManagerRestored&=A==B&&Copy[I].CacheName==Session.OriginalObservedComponents[I].CacheName&&Copy[I].bPlaybackEnabled==Session.OriginalObservedComponents[I].bPlaybackEnabled; } }
    TArray<TWeakObjectPtr<UGeometryCollectionComponent>> FullyRestored;for(int32 I=0;I<Session.Components.Num();++I) { if(Session.ComponentTouched.IsValidIndex(I)&&Session.ComponentTouched[I]) { FullyRestored.Add(Session.Components[I]); } }const bool OtherObservedRestored=RestoreOriginalFlags(Session.OriginalComponentFlags,FullyRestored);Session.bCleanupComplete=SimRestored&&RegistrationRestored&&Session.bSettingsRestored&&ManagerRestored&&OtherObservedRestored;ActiveCinematicRecordComponents.Reset();Work->R.Data->SetBoolField(TEXT("cleanup_complete"),Session.bCleanupComplete);Work->R.Data->SetBoolField(TEXT("semantic_restore_not_atomic_rollback"),true);if(!Session.bCleanupComplete) { Work->Fail(TEXT("cleanup_incomplete"),TEXT("Record restore readback differs; indirect physics/cache effects are not rolled back")); }
}
int32 ResolveRootItem(const UGeometryCollectionComponent* Component)
{
    const UGeometryCollection* Rest = Component ? Component->GetRestCollection() : nullptr;
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection = Rest ? Rest->GetGeometryCollection() : nullptr;
    if (Collection.IsValid())
    {
        if(!RestReadable(Rest)) { return INDEX_NONE; }
        for (int32 Index = 0; Index < Collection->NumElements(FGeometryCollection::TransformGroup); ++Index)
        {
            if (Collection->Parent[Index] == INDEX_NONE)
            {
                return Index;
            }
        }
    }
    return INDEX_NONE;
}

void CapturePhysicsObjectCounts(
    const UGeometryCollectionComponent* Component,
    int32& OutObjectCount,
    int32& OutDisabledCount)
{
    OutObjectCount = 0;
    OutDisabledCount = 0;
    if (!Component)
    {
        return;
    }
    SDK(TEXT("acquire_current_physics_objects"));const TArray<Chaos::FPhysicsObject*> Objects = Component->GetAllPhysicsObjects();
    TArray<Chaos::FConstPhysicsObjectHandle> ConstObjects;
    ConstObjects.Reserve(Objects.Num());
    for (Chaos::FPhysicsObject* Object : Objects)
    {
        if (Object)
        {
            ConstObjects.Add(Object);
        }
    }
    OutObjectCount = ConstObjects.Num();
    if (ConstObjects.IsEmpty())
    {
        return;
    }
    if(!Guard()) { return; }SDK(TEXT("locked_physics_snapshot"));
    FLockedReadPhysicsObjectExternalInterface Read =
        FPhysicsObjectExternalInterface::LockRead(ConstObjects);
    for (Chaos::FConstPhysicsObjectHandle Object : ConstObjects)
    {
        OutDisabledCount += Read->AreAllDisabled(MakeArrayView(&Object, 1)) ? 1 : 0;
    }
}

void CapturePhysicsObjectSnapshot(
    const UGeometryCollectionComponent* Component,
    TArray<int32>& OutIds,
    TArray<int32>& OutStates,
    TArray<bool>& OutDisabled,
    TArray<FVector>& OutPositions,
    TArray<FQuat>& OutRotations)
{
    OutIds.Reset();
    OutStates.Reset();
    OutDisabled.Reset();
    OutPositions.Reset();
    OutRotations.Reset();
    if (!Component) return;
    SDK(TEXT("acquire_current_physics_objects"));const TArray<Chaos::FPhysicsObject*> Objects = Component->GetAllPhysicsObjects();
    TArray<Chaos::FConstPhysicsObjectHandle> ConstObjects;
    for (Chaos::FPhysicsObject* Object : Objects)
    {
        if (Object) ConstObjects.Add(Object);
    }
    if (ConstObjects.IsEmpty()) return;
    if(!Guard()) { return; }SDK(TEXT("locked_physics_snapshot"));
    FLockedReadPhysicsObjectExternalInterface Read =
        FPhysicsObjectExternalInterface::LockRead(ConstObjects);
    for (Chaos::FConstPhysicsObjectHandle Object : ConstObjects)
    {
        const Chaos::FPBDRigidParticle* RigidParticle = Read->GetRigidParticle(Object);
        OutIds.Add(Chaos::FPhysicsObjectInterface::GetId(Object));
        OutStates.Add(RigidParticle
            ? static_cast<int32>(RigidParticle->ObjectState()) : INDEX_NONE);
        OutDisabled.Add(Read->AreAllDisabled(MakeArrayView(&Object, 1)));
        const FTransform PhysicsTransform = Read->GetTransform(Object);
        OutPositions.Add(PhysicsTransform.GetLocation());
        OutRotations.Add(PhysicsTransform.GetRotation());
    }
}

void UpdateNativeReleasedDynamicDisplacement(FNativeCacheRecordSession& Session)
{
    int32 CurrentReleasedDynamicCount = 0;
    double CurrentMaximumDisplacementCm = 0.0;
    for (int32 ComponentIndex = 0; ComponentIndex < Session.Components.Num(); ++ComponentIndex)
    {
        const UGeometryCollectionComponent* Component = Session.Components[ComponentIndex].Get();
        if (!Component || !Session.InitialPhysicsObjectIds.IsValidIndex(ComponentIndex) ||
            !Session.InitialPhysicsObjectDisabled.IsValidIndex(ComponentIndex) ||
            !Session.InitialPhysicsObjectPositions.IsValidIndex(ComponentIndex) ||
            !Session.InitialPhysicsObjectRotations.IsValidIndex(ComponentIndex))
        {
            continue;
        }
        TArray<int32> CurrentIds;
        TArray<int32> CurrentStates;
        TArray<bool> CurrentDisabled;
        TArray<FVector> CurrentPositions;
        TArray<FQuat> CurrentRotations;
        CapturePhysicsObjectSnapshot(
            Component, CurrentIds, CurrentStates, CurrentDisabled, CurrentPositions,
            CurrentRotations);
        const TArray<int32>& InitialIds = Session.InitialPhysicsObjectIds[ComponentIndex];
        const TArray<bool>& InitialDisabled = Session.InitialPhysicsObjectDisabled[ComponentIndex];
        const TArray<FVector>& InitialPositions = Session.InitialPhysicsObjectPositions[ComponentIndex];
        for (int32 CurrentIndex = 0; CurrentIndex < CurrentIds.Num(); ++CurrentIndex)
        {
            const int32 InitialIndex = InitialIds.Find(CurrentIds[CurrentIndex]);
            if (InitialIndex == INDEX_NONE || !InitialDisabled.IsValidIndex(InitialIndex) ||
                !InitialPositions.IsValidIndex(InitialIndex) ||
                !CurrentDisabled.IsValidIndex(CurrentIndex) ||
                !CurrentStates.IsValidIndex(CurrentIndex) ||
                !CurrentPositions.IsValidIndex(CurrentIndex))
            {
                continue;
            }
            if (InitialDisabled[InitialIndex] && !CurrentDisabled[CurrentIndex] &&
                CurrentStates[CurrentIndex] == static_cast<int32>(Chaos::EObjectStateType::Dynamic))
            {
                ++CurrentReleasedDynamicCount;
                CurrentMaximumDisplacementCm = FMath::Max(
                    CurrentMaximumDisplacementCm,
                    FVector::Distance(InitialPositions[InitialIndex], CurrentPositions[CurrentIndex]));
            }
        }
    }
    Session.ReleasedDynamicPhysicsObjectCount = FMath::Max(
        Session.ReleasedDynamicPhysicsObjectCount, CurrentReleasedDynamicCount);
    Session.MaximumReleasedDynamicDisplacementCm = FMath::Max(
        Session.MaximumReleasedDynamicDisplacementCm, CurrentMaximumDisplacementCm);
}

FTransform ComputeDynamicGlobalTransform(
    const FGeometryDynamicCollection& Dynamic,
    const int32 TransformIndex)
{
    if (TransformIndex < 0 || TransformIndex >= Dynamic.GetNumTransforms())
    {
        return FTransform::Identity;
    }
    FTransform Global(Dynamic.GetTransform(TransformIndex));
    int32 Current = TransformIndex;
    int32 Remaining = Dynamic.GetNumTransforms();
    while (Remaining-- > 0 && Dynamic.GetHasParent(Current))
    {
        Current = Dynamic.GetParent(Current);
        if (Current < 0 || Current >= Dynamic.GetNumTransforms()) break;
        Global *= FTransform(Dynamic.GetTransform(Current));
    }
    return Global;
}

void CaptureNativeAnchorSelection(FNativeCacheRecordSession& Session)
{
    Session.AnchoredTransformIndices.SetNum(Session.Components.Num());
    Session.InitialAnchoredGlobalTransforms.SetNum(Session.Components.Num());
    for (TArray<int32>& Indices : Session.AnchoredTransformIndices) Indices.Reset();
    for (TArray<FTransform>& Transforms : Session.InitialAnchoredGlobalTransforms) Transforms.Reset();
    Session.InitialAnchorSelectionPopulationCount = 0;
    Session.InitialAnchorSelectedTransformCount = 0;
    Session.InitialAnchorSelectedPhysicsBodyCount = 0;
    Session.InitialAnchorKinematicPhysicsBodyCount = 0;
    Session.InitialAnchorDynamicPhysicsBodyCount = 0;
    Session.InitialAnchorOtherPhysicsBodyCount = 0;
    for (int32 ComponentIndex = 0; ComponentIndex < Session.Components.Num(); ++ComponentIndex)
    {
        const UGeometryCollectionComponent* Component = Session.Components[ComponentIndex].Get();
        const FGeometryDynamicCollection* Dynamic = Component ? Component->GetDynamicCollection() : nullptr;
        const UGeometryCollection* RestAsset = Component ? Component->GetRestCollection() : nullptr;
        const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest =
            RestAsset ? RestAsset->GetGeometryCollection() : nullptr;
        if (!Dynamic || !Rest.IsValid()) continue;if(!RestReadable(RestAsset)) { return; }
        FBox AnchorBounds = Session.NativeAnchorBounds.IsValidIndex(ComponentIndex)
            ? Session.NativeAnchorBounds[ComponentIndex] : FBox(EForceInit::ForceInit);
        const auto InitializationFieldsCopy=Component->InitializationFields;
        for (const TObjectPtr<const AFieldSystemActor>& InitializationField : InitializationFieldsCopy)
        {
            const AFieldSystemActor* FieldActor = InitializationField.Get();
            const UFieldSystemComponent* FieldComponent =
                FieldActor ? FieldActor->GetFieldSystemComponent() : nullptr;
            if (FieldComponent && FieldComponent->ConstructionCommands.GetNumCommands() > 0)
            {
                if(!FieldBounds(FieldComponent,AnchorBounds)) { return; }
            }
        }
        if (!AnchorBounds.IsValid) continue;
        TArray<FTransform> RestGlobals;
        GeometryCollectionAlgo::GlobalMatrices(Rest->Transform, Rest->Parent, RestGlobals);
        const int32 Count = FMath::Min3(
            RestGlobals.Num(), Dynamic->GetNumTransforms(),
            Rest->NumElements(FGeometryCollection::TransformGroup));
        for (int32 TransformIndex = 0; TransformIndex < Count; ++TransformIndex)
        {
            const bool bRigidLeaf = Rest->SimulationType[TransformIndex] ==
                    FGeometryCollection::ESimulationTypes::FST_Rigid &&
                Rest->Children[TransformIndex].IsEmpty();
            Session.InitialAnchorSelectionPopulationCount += bRigidLeaf ? 1 : 0;
        }
        TSet<int32> EffectiveDescendantLeaves;
        for (int32 TransformIndex = 0; TransformIndex < Count; ++TransformIndex)
        {
            if (Rest->SimulationType[TransformIndex] !=
                    FGeometryCollection::ESimulationTypes::FST_Clustered ||
                Rest->Parent[TransformIndex] != INDEX_NONE)
            {
                continue;
            }
            const FVector ClusterWorldLocation = RestTransformGeometryWorldCenter(
                *Rest, RestGlobals, TransformIndex, Component->GetComponentTransform());
            if (!AnchorBounds.IsInsideOrOn(ClusterWorldLocation)) continue;
            Session.AnchoredTransformIndices[ComponentIndex].Add(TransformIndex);
            FTransform InitialAnchorTransform =
                ComputeDynamicGlobalTransform(*Dynamic, TransformIndex) *
                Component->GetComponentTransform();
            ++Session.InitialAnchorSelectedPhysicsBodyCount;
            if (Chaos::FPhysicsObject* PhysicsObject =
                Component->GetPhysicsObjectById(TransformIndex))
            {
                FLockedReadPhysicsObjectExternalInterface Read =
                    FPhysicsObjectExternalInterface::LockRead(PhysicsObject);
                // Compare like-for-like solver transforms. Cluster proxy
                // pivots can differ from the dynamic collection's local
                // hierarchy representation without any actual motion. A
                // kinematic anchor may become solver-disabled while remaining
                // fixed, but its solver transform is still the right sample.
                InitialAnchorTransform = Read->GetTransform(PhysicsObject);
                const Chaos::FPBDRigidParticle* RigidParticle =
                    Read->GetRigidParticle(PhysicsObject);
                if (RigidParticle &&
                    RigidParticle->ObjectState() == Chaos::EObjectStateType::Kinematic)
                {
                    ++Session.InitialAnchorKinematicPhysicsBodyCount;
                }
                else if (RigidParticle &&
                    RigidParticle->ObjectState() == Chaos::EObjectStateType::Dynamic)
                {
                    ++Session.InitialAnchorDynamicPhysicsBodyCount;
                }
                else
                {
                    ++Session.InitialAnchorOtherPhysicsBodyCount;
                }
            }
            Session.InitialAnchoredGlobalTransforms[ComponentIndex].Add(
                InitialAnchorTransform);
            for (int32 Candidate = 0; Candidate < Count; ++Candidate)
            {
                if (Rest->SimulationType[Candidate] !=
                        FGeometryCollection::ESimulationTypes::FST_Rigid ||
                    !Rest->Children[Candidate].IsEmpty()) continue;
                int32 Cursor = Candidate;
                while (Cursor != INDEX_NONE && Cursor != TransformIndex &&
                    Rest->Parent.IsValidIndex(Cursor))
                {
                    Cursor = Rest->Parent[Cursor];
                }
                if (Cursor == TransformIndex)
                {
                    EffectiveDescendantLeaves.Add(Candidate);
                }
            }
        }
        Session.InitialAnchorSelectedTransformCount +=
            EffectiveDescendantLeaves.Num();
    }
    Session.AnchorSelectionPercent =
        Session.InitialAnchorSelectionPopulationCount > 0
        ? 100.0 * Session.InitialAnchorSelectedTransformCount /
            Session.InitialAnchorSelectionPopulationCount
        : 0.0;
}

void UpdateNativeAnchorStability(FNativeCacheRecordSession& Session)
{
    int32 CurrentMatchedCount = 0;
    for (int32 ComponentIndex = 0; ComponentIndex < Session.Components.Num(); ++ComponentIndex)
    {
        const UGeometryCollectionComponent* Component = Session.Components[ComponentIndex].Get();
        const FGeometryDynamicCollection* Dynamic = Component ? Component->GetDynamicCollection() : nullptr;
        if (!Dynamic || !Session.AnchoredTransformIndices.IsValidIndex(ComponentIndex) ||
            !Session.InitialAnchoredGlobalTransforms.IsValidIndex(ComponentIndex)) continue;
        const TArray<int32>& Indices = Session.AnchoredTransformIndices[ComponentIndex];
        const TArray<FTransform>& Initial = Session.InitialAnchoredGlobalTransforms[ComponentIndex];
        for (int32 AnchorIndex = 0; AnchorIndex < Indices.Num(); ++AnchorIndex)
        {
            const int32 TransformIndex = Indices[AnchorIndex];
            if (TransformIndex < 0 || TransformIndex >= Dynamic->GetNumTransforms() ||
                !Initial.IsValidIndex(AnchorIndex)) continue;
            FTransform Current = ComputeDynamicGlobalTransform(*Dynamic, TransformIndex) *
                Component->GetComponentTransform();
            if (Chaos::FPhysicsObject* PhysicsObject =
                Component->GetPhysicsObjectById(TransformIndex))
            {
                FLockedReadPhysicsObjectExternalInterface Read =
                    FPhysicsObjectExternalInterface::LockRead(PhysicsObject);
                Current = Read->GetTransform(PhysicsObject);
            }
            ++CurrentMatchedCount;
            Session.MaximumAnchoredDisplacementCm = FMath::Max(
                Session.MaximumAnchoredDisplacementCm,
                FVector::Distance(
                    Initial[AnchorIndex].GetLocation(), Current.GetLocation()));
            Session.MaximumAnchoredRotationDegrees = FMath::Max(
                Session.MaximumAnchoredRotationDegrees,
                FMath::RadiansToDegrees(Initial[AnchorIndex].GetRotation().AngularDistance(
                    Current.GetRotation())));
        }
    }
    Session.MatchedAnchorSelectedPhysicsBodyCount = FMath::Max(
        Session.MatchedAnchorSelectedPhysicsBodyCount, CurrentMatchedCount);
    if (CurrentMatchedCount == Session.InitialAnchorSelectedPhysicsBodyCount)
    {
        Session.MatchedAnchorSelectedTransformCount =
            Session.InitialAnchorSelectedTransformCount;
    }
}

void AppendNativeAnchorStabilityEvidence(
    const TSharedPtr<FJsonObject>& Data,
    const FNativeCacheRecordSession& Session)
{
    const bool bAuthoredFieldOnly =
        Session.bPreserveInitializationFieldsDuringRecord &&
        !Session.bUseNativeAfterReleaseAnchorFallback &&
        Session.InitializationFieldSuppressCount == 0 &&
        Session.NativeAnchorByBoxDispatchCount == 0 &&
        Session.NativeAnchorByIndexDispatchCount == 0 &&
        Session.NativeAnchorKinematicFieldDispatchCount == 0 &&
        Session.NativeAnchorForceKinematicObjectCount == 0 &&
        Session.AuthoredInitializationFieldRedispatchCount ==
            Session.Components.Num();
    const bool bSelectionGate = Session.InitialAnchorSelectionPopulationCount > 0 &&
        Session.AnchorSelectionPercent >= 10.0 && Session.AnchorSelectionPercent <= 30.0;
    const bool bStabilityGate = Session.InitialAnchorSelectedTransformCount > 0 &&
        Session.InitialAnchorSelectedPhysicsBodyCount > 0 &&
        Session.MatchedAnchorSelectedPhysicsBodyCount ==
            Session.InitialAnchorSelectedPhysicsBodyCount &&
        Session.MatchedAnchorSelectedTransformCount == Session.InitialAnchorSelectedTransformCount &&
        Session.MaximumAnchoredDisplacementCm <= 0.5 &&
        Session.MaximumAnchoredRotationDegrees <= 0.5;
    const bool bUpperReleaseGate = Session.ReleasedDynamicPhysicsObjectCount > 0 &&
        Session.MaximumReleasedDynamicDisplacementCm >= 10.0;
    Data->SetNumberField(TEXT("anchor_selection_population_count"),
        Session.InitialAnchorSelectionPopulationCount);
    Data->SetNumberField(TEXT("anchor_selected_transform_count"),
        Session.InitialAnchorSelectedTransformCount);
    Data->SetNumberField(TEXT("anchored_cluster_descendant_count"),
        Session.InitialAnchorSelectedTransformCount);
    Data->SetNumberField(TEXT("anchor_selected_cluster_physics_body_count"),
        Session.InitialAnchorSelectedPhysicsBodyCount);
    Data->SetNumberField(TEXT("initial_anchor_kinematic_physics_body_count"),
        Session.InitialAnchorKinematicPhysicsBodyCount);
    Data->SetNumberField(TEXT("initial_anchor_dynamic_physics_body_count"),
        Session.InitialAnchorDynamicPhysicsBodyCount);
    Data->SetNumberField(TEXT("initial_anchor_other_physics_body_count"),
        Session.InitialAnchorOtherPhysicsBodyCount);
    Data->SetNumberField(TEXT("anchor_selection_percent"), Session.AnchorSelectionPercent);
    Data->SetStringField(TEXT("anchor_selection_semantics"),
        TEXT("descendants_of_initialization_field_selected_active_root_clusters"));
    Data->SetNumberField(TEXT("matched_anchor_selected_transform_count"),
        Session.MatchedAnchorSelectedTransformCount);
    Data->SetNumberField(TEXT("matched_anchor_selected_cluster_physics_body_count"),
        Session.MatchedAnchorSelectedPhysicsBodyCount);
    Data->SetNumberField(TEXT("maximum_anchored_displacement_cm"),
        Session.MaximumAnchoredDisplacementCm);
    Data->SetNumberField(TEXT("maximum_anchored_rotation_degrees"),
        Session.MaximumAnchoredRotationDegrees);
    Data->SetBoolField(TEXT("anchor_selection_ten_to_thirty_percent_gate"), bSelectionGate);
    Data->SetBoolField(TEXT("anchor_stability_half_cm_half_degree_gate"), bStabilityGate);
    Data->SetBoolField(TEXT("upper_structure_ten_cm_or_break_gate"), bUpperReleaseGate);
    Data->SetBoolField(TEXT("anchor_functional_gate"),
        bSelectionGate && bStabilityGate && bUpperReleaseGate);
    Data->SetNumberField(TEXT("native_set_anchored_by_box_dispatch_count"),
        Session.NativeAnchorByBoxDispatchCount);
    Data->SetNumberField(TEXT("native_remove_all_anchors_restore_count"),
        Session.NativeAnchorRemovalCount);
    Data->SetNumberField(TEXT("native_set_anchored_by_index_dispatch_count"),
        Session.NativeAnchorByIndexDispatchCount);
    Data->SetNumberField(TEXT("native_anchor_kinematic_field_dispatch_count"),
        Session.NativeAnchorKinematicFieldDispatchCount);
    Data->SetNumberField(TEXT("native_anchor_force_kinematic_object_count"),
        Session.NativeAnchorForceKinematicObjectCount);
    Data->SetBoolField(TEXT("native_anchor_api_gate"),
        bAuthoredFieldOnly ||
        (Session.NativeAnchorByBoxDispatchCount == Session.Components.Num() &&
         Session.NativeAnchorByIndexDispatchCount == Session.InitialAnchorSelectedTransformCount &&
         Session.NativeAnchorKinematicFieldDispatchCount == Session.Components.Num() &&
         Session.NativeAnchorForceKinematicObjectCount >=
            Session.InitialAnchorSelectedTransformCount &&
         (Session.bCleanupComplete
            ? Session.NativeAnchorRemovalCount == Session.NativeAnchorByBoxDispatchCount
            : true)));
    Data->SetBoolField(TEXT("authored_initialization_anchor_field_only_gate"),
        bAuthoredFieldOnly);
    Data->SetBoolField(TEXT("preserve_initialization_fields_during_record"),
        Session.bPreserveInitializationFieldsDuringRecord);
    Data->SetBoolField(TEXT("use_native_after_release_anchor_fallback"),
        Session.bUseNativeAfterReleaseAnchorFallback);
    Data->SetNumberField(TEXT("authored_initialization_field_redispatch_count"),
        Session.AuthoredInitializationFieldRedispatchCount);
    Data->SetNumberField(TEXT("initialization_field_suppress_count"),
        Session.InitializationFieldSuppressCount);
    Data->SetNumberField(TEXT("initialization_field_restore_count"),
        Session.InitializationFieldRestoreCount);
    Data->SetBoolField(TEXT("initialization_field_record_lifecycle_gate"),
        Session.bPreserveInitializationFieldsDuringRecord
            ? Session.InitializationFieldSuppressCount == 0 &&
                Session.InitializationFieldRestoreCount == 0
            : Session.InitializationFieldSuppressCount == Session.Components.Num() &&
                (Session.bCleanupComplete
                    ? Session.InitializationFieldRestoreCount ==
                        Session.InitializationFieldSuppressCount
                    : true));
}

FString HashVisiblePlaybackTransforms(
    const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Components,
    AChaosCacheManager* Manager = nullptr);

FString HashLiveSolverDiagnostic(
    const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Components);

bool TickNativeCacheRecordBody(float)
{
    if (!ActiveNativeRecordSession)
    {
        return false;
    }
    FNativeCacheRecordSession& Session = *ActiveNativeRecordSession;
    UWorld* World = Session.World.Get();
    AChaosCacheManager* Manager = Session.Manager.Get();
    if (!World || !Manager)
    {
        CleanupNativeRecordSession(Session, false);
        Session.bTimedOut = true;
        return false;
    }
    const double RequestElapsed = World->GetTimeSeconds() - Session.RequestWorldTime;
    if (RequestElapsed >= Session.TimeoutSeconds||FPlatformTime::Seconds()-Session.MonotonicStart>=Session.TimeoutSeconds)
    {
        Session.ConfigurationFailure = TEXT("record_configuration_or_capture_timeout");
        Session.ConfigurationPhase = TEXT("failed");
        CleanupNativeRecordSession(Session, false);
        Session.bTimedOut = true;
        return false;
    }
    if (!Session.bManagerConfigured)
    {
        if (GFrameCounter <= Session.EndEvaluateFrame)
        {
            return true;
        }
        UChaosCacheCollection* Collection = Session.Collection.Get();
        if (!Collection || Session.AppliedCacheNames.Num() != Session.Components.Num())
        {
            Session.ConfigurationFailure = TEXT("collection_or_cache_name_mapping_lost");
            Session.ConfigurationPhase = TEXT("failed");
            CleanupNativeRecordSession(Session, false);
            Session.bTimedOut = true;
            return false;
        }
        TArray<UGeometryCollectionComponent*> CurrentComponents;for(const auto& C:Session.Components) { if(C.IsValid()) { CurrentComponents.Add(C.Get()); } }
        if(CurrentComponents.Num()!=Session.Components.Num()||!CaptureTargets(Manager,Collection,CurrentComponents)||!Work->Modify(Manager,TEXT("configure_record_manager"))) { return false; }
        Manager->CacheCollection = Collection;
        Manager->CacheMode = ECacheMode::Record;
        Manager->StartMode = EStartMode::Timed;
        // UE 5.6 ClearObservedComponents() destroys every referenced
        // component. Evaluation is already quiescent here, so replace only
        // the binding array and preserve the authored Geometry Collection.
        if(!Step(TEXT("clear_previous_observed_without_destroy"),[&]{Manager->ClearObservedComponents(false);})) { return false; }
        int32 AddedComponentCount = 0;
        // Entry settings were captured before EndEvaluate/Clear; notifier effects must not replace that baseline.
        Session.NativeAnchorBounds.Init(FBox(EForceInit::ForceInit), Session.Components.Num());
        Session.PhysicsObjectCountsBeforeRecreate.Init(0, Session.Components.Num());
        Session.PhysicsObjectCountsAfterRecreate.Init(0, Session.Components.Num());
        for (int32 Index = 0; Index < Session.Components.Num(); ++Index)
        {
            AActor* ComponentActor=Session.Components[Index].IsValid()?Session.Components[Index]->GetOwner():nullptr;
            if (UGeometryCollectionComponent* Component=Session.Components[Index].Get())
            {
                Session.Components[Index] = Component;

                const auto InitializationFieldsCopy=Component->InitializationFields;
        for (const TObjectPtr<const AFieldSystemActor>& InitializationField : InitializationFieldsCopy)
                {
                    const UFieldSystemComponent* FieldComponent = InitializationField
                        ? InitializationField->GetFieldSystemComponent() : nullptr;
                    if (FieldComponent && FieldComponent->ConstructionCommands.GetNumCommands() > 0)
                    {
                        if(!FieldBounds(FieldComponent,Session.NativeAnchorBounds[Index])) { return false; }
                    }
                }
                Session.ComponentTouched[Index]=true;
                if (!Session.bPreserveInitializationFieldsDuringRecord)
                {
                    Write(TEXT("suppress_record_initialization_fields"));Component->InitializationFields.Empty();
                    ++Session.InitializationFieldSuppressCount;
                }
                Session.PhysicsObjectCountsBeforeRecreate[Index] =
                    Component->GetAllPhysicsObjects().Num();
                // Playback assets are authored Kinematic. Recording is the one
                // lifecycle that temporarily needs Dynamic initialization;
                // restore the exact authored ObjectType on every cleanup path.
                Write(TEXT("set_record_dynamic_object_type"));Component->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
                ++Session.ObjectTypeDynamicConfigureCount;
                // EndEvaluate can leave the re-resolved PIE component without
                // a dynamic collection or live physics proxy. Reapplying the
                // identical Rest Collection through the public engine API
                // resets the dynamic collection and recreates physics state
                // exactly once without applying asset defaults.
                if (const UGeometryCollection* RestCollection = Component->GetRestCollection())
                {
                    if(!Step(TEXT("SetRestCollection"),[&]{Component->SetRestCollection(RestCollection, false);})) { return false; }
                    ++Session.PhysicsStateRecreateCount;
                    ++Session.DynamicCollectionResetCount;
                }
                if (!Component->IsRegistered())
                {
                    if(!Step(TEXT("RegisterComponentWithWorld"),[&]{Component->RegisterComponentWithWorld(World);})) { return false; }
                    if (Component->IsRegistered())
                    {
                        ++Session.ComponentRegisterCount;
                    }
                }
                // Geometry Collection only registers its physics proxy when
                // simulation is enabled. Cache Record requires live Chaos, so
                // enable it once for this session and restore the exact
                // original flag during every cleanup path.
                if(!Step(TEXT("SetSimulatePhysics"),[&]{Component->SetSimulatePhysics(true);})) { return false; }
                ++Session.SimulatePhysicsEnableCount;
                Session.PhysicsObjectCountsAfterRecreate[Index] =
                    Component->GetAllPhysicsObjects().Num();
                if(!Guard()) { return false; }UE_LOG(LogTemp,Display,TEXT("Chaos cache record physics init component=%s actor=%s frame=%llu"),*Component->GetPathName(),ComponentActor?*ComponentActor->GetName():TEXT("<null>"),GFrameCounter);
                if(!Step(TEXT("FindOrAddObservedComponent"),[&]{Manager->FindOrAddObservedComponent(
                    Component, FName(*Session.AppliedCacheNames[Index]), true);})) { return false; }
                ++AddedComponentCount;
            }
        }
        if (AddedComponentCount != Session.Components.Num())
        {
            Session.ConfigurationFailure = FString::Printf(
                TEXT("observed_component_rebuild_count_%d"), AddedComponentCount);
            Session.ConfigurationPhase = TEXT("failed");
            CleanupNativeRecordSession(Session, false);
            Session.bTimedOut = true;
            return false;
        }
        Session.ManagerConfigureFrame = GFrameCounter;
        Session.PhysicsStateRecreateFrame = GFrameCounter;
        Session.ConfigurationPhase = TEXT("physics_state_quiescence");
        Session.bManagerConfigured = true;
        ActiveCinematicRecordComponents = Session.Components;
        return true;
    }
    if (!Session.bPhysicsStateReady)
    {
        if (GFrameCounter <= Session.PhysicsStateRecreateFrame)
        {
            return true;
        }
        bool bAllPhysicsStatesReady = Session.Components.Num() > 0;
        int32 ProxyReadyCount = 0;
        for (int32 Index = 0; Index < Session.Components.Num(); ++Index)
        {
            UGeometryCollectionComponent* Component = Session.Components[Index].Get();
            const int32 PhysicsObjectCount = Component
                ? Component->GetAllPhysicsObjects().Num() : 0;
            if (Session.PhysicsObjectCountsAfterRecreate.IsValidIndex(Index))
            {
                Session.PhysicsObjectCountsAfterRecreate[Index] = PhysicsObjectCount;
            }
            const bool bComponentReady = Component && Component->GetPhysicsProxy() &&
                PhysicsObjectCount > 0;
            ProxyReadyCount += bComponentReady ? 1 : 0;
            bAllPhysicsStatesReady &= bComponentReady;
        }
        Session.PhysicsProxyReadyCount = ProxyReadyCount;
        if (!bAllPhysicsStatesReady)
        {
            if (GFrameCounter - Session.PhysicsStateRecreateFrame >= 60)
            {
                for (const TWeakObjectPtr<UGeometryCollectionComponent>& WeakComponent : Session.Components)
                {
                    if (UGeometryCollectionComponent* Component = WeakComponent.Get())
                    {
                        if(!Guard()) { return false; }UE_LOG(LogTemp,Warning,TEXT("Chaos cache record physics readiness timeout component=%s frame=%llu"),*Component->GetPathName(),GFrameCounter);
                    }
                }
                Session.ConfigurationFailure = TEXT("geometry_collection_physics_state_not_ready_after_recreate");
                Session.ConfigurationPhase = TEXT("failed");
                CleanupNativeRecordSession(Session, false);
                Session.bTimedOut = true;
                return false;
            }
            return true;
        }
        if (Session.bPreserveInitializationFieldsDuringRecord)
        {
            // The cache workflow recreated these proxies after the level's
            // construction pass. Re-dispatch the exact authored engine Anchor
            // Field graph only after every target proxy is confirmed ready.
            for (int32 Index = 0; Index < Session.Components.Num(); ++Index)
            {
                UGeometryCollectionComponent* Component =
                    Session.Components[Index].Get();
                if (!Component ||
                    !Session.OriginalInitializationFields.IsValidIndex(Index))
                {
                    continue;
                }
                for (const TObjectPtr<const AFieldSystemActor>& InitializationField :
                    Session.OriginalInitializationFields[Index])
                {
                    UFieldSystemComponent* AuthoredFieldComponent =
                        InitializationField
                            ? InitializationField->GetFieldSystemComponent()
                            : nullptr;
                    if (!AuthoredFieldComponent ||
                        !AuthoredFieldComponent->ConstructionCommands.RootNodes.IsValidIndex(0) ||
                        !AuthoredFieldComponent->ConstructionCommands.RootNodes[0])
                    {
                        continue;
                    }
                    const auto AuthoredCommands=AuthoredFieldComponent->ConstructionCommands;if(AuthoredCommands.TargetNames.Num()!=AuthoredCommands.RootNodes.Num()||AuthoredCommands.TargetNames.Num()!=AuthoredCommands.MetaDatas.Num()||!AuthoredCommands.RootNodes.IsValidIndex(0)||!AuthoredCommands.RootNodes[0]) { Work->Fail(TEXT("field_command_schema_invalid"),TEXT("Authored initialization command arrays changed inconsistently"));return false; }auto* AuthoredRoot=AuthoredCommands.RootNodes[0].Get();if(!Work->Track(AuthoredRoot,TEXT("redispatched_authored_field_root"))) { return false; }
                    if(!Step(TEXT("ApplyPhysicsField"),[&]{Component->ApplyPhysicsField(
                        true,
                        EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState,
                        nullptr,
                        AuthoredRoot);})) { return false; }
                    ++Session.AuthoredInitializationFieldRedispatchCount;
                }
            }
        }
        Session.PhysicsStateReadyFrame = GFrameCounter;
        Session.ConfigurationPhase = TEXT("physics_state_ready");
        Session.bPhysicsStateReady = true;
        return true;
    }
    if (!Session.bRecordingActive)
    {
        if (GFrameCounter <= Session.PhysicsStateReadyFrame)
        {
            return true;
        }
        if(!Step(TEXT("BeginEvaluate"),[&]{Manager->BeginEvaluate();})) { return false; }
        Session.BeginEvaluateFrame = GFrameCounter;
        Session.BeginFrame = GFrameCounter;
        Session.BeginWorldTime = World->GetTimeSeconds();
        Session.CapturedFrameCount = 0;
        Session.ConfigurationPhase = TEXT("recording_active");
        Session.bRecordingActive = true;
        Session.ObservedTransformHashes.Add(HashLiveSolverDiagnostic(Session.Components));
        return true;
    }
    Session.CapturedFrameCount = CastI32(static_cast<double>(GFrameCounter-Session.BeginFrame),TEXT("captured_frame_count"));
    const double Elapsed = World->GetTimeSeconds() - Session.BeginWorldTime;
    if (Session.InitialTriggerMode == TEXT("strain_velocity") &&
        !Session.bInitialStrainDispatchAttempted &&
        Session.CapturedFrameCount >= Session.InitialTriggerDelayFrames)
    {
        Session.bInitialStrainDispatchAttempted = true;
        Session.DeferredBreakingDispatchFrame = GFrameCounter;
        Session.InitialStrainDispatchFrame = GFrameCounter;
        Session.InitialPhysicsObjectIds.SetNum(Session.Components.Num());
        Session.InitialPhysicsObjectStates.SetNum(Session.Components.Num());
        Session.InitialPhysicsObjectDisabled.SetNum(Session.Components.Num());
        Session.InitialPhysicsObjectPositions.SetNum(Session.Components.Num());
        Session.InitialPhysicsObjectRotations.SetNum(Session.Components.Num());
        CaptureNativeAnchorSelection(Session);
        for (int32 Index = 0; Index < Session.Components.Num(); ++Index)
        {
            UGeometryCollectionComponent* Component = Session.Components[Index].Get();
            const int32 RootItem = Session.DeferredBreakingItems.IsValidIndex(Index)
                ? Session.DeferredBreakingItems[Index] : INDEX_NONE;
            if (Component && RootItem != INDEX_NONE)
            {
                const FVector Velocity = Session.MotionVelocity +
                    FVector(0.0, Index * 20.0, Index * 5.0);
                CapturePhysicsObjectCounts(
                    Component,
                    Session.InitialPhysicsObjectCounts[Index],
                    Session.InitialDisabledPhysicsObjectCounts[Index]);
                CapturePhysicsObjectSnapshot(
                    Component,
                    Session.InitialPhysicsObjectIds[Index],
                    Session.InitialPhysicsObjectStates[Index],
                    Session.InitialPhysicsObjectDisabled[Index],
                    Session.InitialPhysicsObjectPositions[Index],
                    Session.InitialPhysicsObjectRotations[Index]);
                Session.CurrentDisabledPhysicsObjectCounts[Index] =
                    Session.InitialDisabledPhysicsObjectCounts[Index];
                if (Session.bUseNativeAfterReleaseAnchorFallback &&
                    Session.AnchoredTransformIndices.IsValidIndex(Index))
                {
                    TArray<Chaos::FPhysicsObjectHandle> PreStrainAnchorObjects;
                    for (const int32 AnchorTransformIndex :
                        Session.AnchoredTransformIndices[Index])
                    {
                        if (Chaos::FPhysicsObject* PhysicsObject =
                            Component->GetPhysicsObjectById(AnchorTransformIndex))
                        {
                            PreStrainAnchorObjects.Add(PhysicsObject);
                        }
                    }
                    if (!PreStrainAnchorObjects.IsEmpty())
                    {
                        if(!Guard()||!Work->R.Error.IsEmpty()) { return false; }Work->Write(TEXT("force_prestrain_anchor_kinematic"));
                        FLockedWritePhysicsObjectExternalInterface Write =
                            FPhysicsObjectExternalInterface::LockWrite(PreStrainAnchorObjects);
                        Write->ForceKinematic(PreStrainAnchorObjects);
                        Session.NativeAnchorForceKinematicObjectCount +=
                            PreStrainAnchorObjects.Num();
                    }
                    if(!Guard()) { return false; }
                }
                const FVector ImpactPoint = Session.InitialTriggerImpactPoints.IsValidIndex(Index)
                    ? Session.InitialTriggerImpactPoints[Index] : Component->Bounds.Origin;
                const float ImpactRadius = Session.InitialTriggerRadii.IsValidIndex(Index)
                    ? Session.InitialTriggerRadii[Index] : 100.0f;
                if(!Step(TEXT("ApplyExternalStrain"),[&]{Component->ApplyExternalStrain(
                    RootItem, ImpactPoint, ImpactRadius,
                    Session.InitialTriggerPropagationDepth, 1.0f, 1000000000000.0f);})) { return false; }
                ++Session.InitialExternalStrainDispatchCount;
                // UE 5.6 evaluates this against the still-active root and
                // arms children whose ExternalStrain is sufficient to break.
                // Calling it after release would target a disabled root and
                // intentionally do nothing.
                if(!Step(TEXT("ApplyBreakingLinearVelocity"),[&]{Component->ApplyBreakingLinearVelocity(RootItem, Velocity);})) { return false; }
                ++Session.InitialMotionDispatchCount;
            }
        }
        if (Session.InitialExternalStrainDispatchCount == Session.Components.Num() &&
            Session.InitialMotionDispatchCount == Session.Components.Num())
        {
            Session.bDeferredBreakingVelocityDispatched = true;
            Session.bBreakingVelocityArmedBeforeRelease = true;
            Session.DeferredBreakingDispatchFrameDelta = 0;
        }
    }
    if (Session.InitialTriggerMode == TEXT("strain_velocity") &&
        Session.InitialExternalStrainDispatchCount == Session.Components.Num() &&
        Session.bDeferredBreakingVelocityDispatched &&
        !Session.bPostReleaseImpulseDispatched)
    {
        bool bAllComponentsReleased = Session.Components.Num() > 0 &&
            Session.ComponentReleaseObserved.Num() == Session.Components.Num();
        for (int32 Index = 0; Index < Session.Components.Num(); ++Index)
        {
            if (UGeometryCollectionComponent* Component = Session.Components[Index].Get())
            {
                int32 ObjectCount = 0;
                int32 DisabledCount = 0;
                CapturePhysicsObjectCounts(Component, ObjectCount, DisabledCount);
                if (Session.CurrentDisabledPhysicsObjectCounts.IsValidIndex(Index))
                {
                    Session.CurrentDisabledPhysicsObjectCounts[Index] = DisabledCount;
                }
                if (Session.ComponentReleaseObserved.IsValidIndex(Index) &&
                    Session.InitialDisabledPhysicsObjectCounts.IsValidIndex(Index) &&
                    DisabledCount < Session.InitialDisabledPhysicsObjectCounts[Index])
                {
                    Session.ComponentReleaseObserved[Index] = true;
                }
                bAllComponentsReleased &= Session.ComponentReleaseObserved.IsValidIndex(Index) &&
                    Session.ComponentReleaseObserved[Index];
            }
            else
            {
                bAllComponentsReleased = false;
            }
        }
        if (bAllComponentsReleased && !Session.bReleaseObservedBeforeMotion)
        {
            Session.bReleaseObservedBeforeMotion = true;
            Session.ReleaseObservedFrame = GFrameCounter;
        }
        if (Session.bUseNativeAfterReleaseAnchorFallback &&
            bAllComponentsReleased && Session.bReleaseObservedBeforeMotion &&
            Session.NativeAnchorByBoxDispatchCount == 0 &&
            GFrameCounter >= Session.ReleaseObservedFrame)
        {
            for (int32 ComponentIndex = 0; ComponentIndex < Session.Components.Num(); ++ComponentIndex)
            {
                UGeometryCollectionComponent* Component = Session.Components[ComponentIndex].Get();
                if (!Component) continue;
                const FBox AnchorBounds = Session.NativeAnchorBounds.IsValidIndex(ComponentIndex)
                    ? Session.NativeAnchorBounds[ComponentIndex] : FBox(EForceInit::ForceInit);
                if (AnchorBounds.IsValid)
                {
                    // Apply the authored field selection when the first
                    // External-Strain release exposes child particles, before
                    // the deliberate upper-structure impulse on the next tick.
                    if(!Step(TEXT("SetAnchoredByBox"),[&]{Component->SetAnchoredByBox(AnchorBounds, true, -1);})) { return false; }
                    ++Session.NativeAnchorByBoxDispatchCount;

                    // Anchoring metadata cannot re-freeze leaf particles that
                    // External Strain has already released. Apply the authored
                    // box as a real Chaos Dynamic State field so its particles
                    // become kinematic before the upper-structure impulse.
                    const FVector FieldScale = AnchorBounds.GetSize() / 100.0;
                    const FTransform FieldTransform(
                        FQuat::Identity,
                        AnchorBounds.GetCenter(),
                        FieldScale);
                    Write(TEXT("create_transient_anchor_fields"));UBoxFalloff* BoxField = NewObject<UBoxFalloff>(Component);
                    UUniformInteger* KinematicField = NewObject<UUniformInteger>(Component);
                    UCullingField* SelectedKinematicField = NewObject<UCullingField>(Component);
                    if(!Work->Track(BoxField,TEXT("temporary_anchor_box"),Component,true)||!Work->Track(KinematicField,TEXT("temporary_anchor_state"),Component,true)||!Work->Track(SelectedKinematicField,TEXT("temporary_anchor_culling"),Component,true)) { return false; }
                    if(!Step(TEXT("SetBoxFalloff"),[&]{BoxField->SetBoxFalloff(
                        1.0f,
                        0.0f,
                        1.0f,
                        0.0f,
                        FieldTransform,
                        EFieldFalloffType::Field_FallOff_None);})) { return false; }
                    if(!Step(TEXT("SetUniformInteger"),[&]{KinematicField->SetUniformInteger(
                        static_cast<int32>(Chaos::EObjectStateType::Kinematic));})) { return false; }
                    if(!Step(TEXT("SetCullingField"),[&]{SelectedKinematicField->SetCullingField(
                        BoxField,
                        KinematicField,
                        EFieldCullingOperationType::Field_Culling_Outside);})) { return false; }
                    if(!Step(TEXT("ApplyPhysicsField"),[&]{Component->ApplyPhysicsField(
                        true,
                        EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState,
                        nullptr,
                        SelectedKinematicField);})) { return false; }
                    ++Session.NativeAnchorKinematicFieldDispatchCount;
                }
            }
            for (int32 ComponentIndex = 0; ComponentIndex < Session.Components.Num(); ++ComponentIndex)
            {
                UGeometryCollectionComponent* Component = Session.Components[ComponentIndex].Get();
                if (!Component || !Session.AnchoredTransformIndices.IsValidIndex(ComponentIndex)) continue;
                TArray<Chaos::FPhysicsObjectHandle> SelectedPhysicsObjects;
                for (const int32 TransformIndex : Session.AnchoredTransformIndices[ComponentIndex])
                {
                    if(!Step(TEXT("SetAnchoredByIndex"),[&]{Component->SetAnchoredByIndex(TransformIndex, true);})) { return false; }
                    ++Session.NativeAnchorByIndexDispatchCount;
                    if (Chaos::FPhysicsObject* PhysicsObject =
                        Component->GetPhysicsObjectById(TransformIndex))
                    {
                        SelectedPhysicsObjects.Add(PhysicsObject);
                    }
                }
                if (!SelectedPhysicsObjects.IsEmpty())
                {
                    if(!Guard()||!Work->R.Error.IsEmpty()) { return false; }Work->Write(TEXT("force_released_anchor_kinematic"));
                    FLockedWritePhysicsObjectExternalInterface Write =
                        FPhysicsObjectExternalInterface::LockWrite(SelectedPhysicsObjects);
                    Write->ForceKinematic(SelectedPhysicsObjects);
                    Session.NativeAnchorForceKinematicObjectCount +=
                        SelectedPhysicsObjects.Num();
                }
                if(!Guard()) { return false; }
            }
            Session.NativeAnchorDispatchFrame = GFrameCounter;
        }
        const bool bAnchorActuationReady =
            !Session.bUseNativeAfterReleaseAnchorFallback ||
            (Session.NativeAnchorByBoxDispatchCount == Session.Components.Num() &&
             Session.NativeAnchorKinematicFieldDispatchCount == Session.Components.Num());
        const uint64 AnchorActuationFrame = Session.bUseNativeAfterReleaseAnchorFallback
            ? Session.NativeAnchorDispatchFrame : Session.ReleaseObservedFrame;
        if (bAllComponentsReleased && Session.bReleaseObservedBeforeMotion &&
            bAnchorActuationReady &&
            !Session.bPostReleaseImpulseDispatched &&
            GFrameCounter > AnchorActuationFrame)
        {
            for (int32 Index = 0; Index < Session.Components.Num(); ++Index)
            {
                if (UGeometryCollectionComponent* Component = Session.Components[Index].Get())
                {
                    const int32 RootItem = Session.DeferredBreakingItems[Index];
                    if (RootItem != INDEX_NONE)
                    {
                        const FVector Velocity = Session.MotionVelocity + FVector(0.0, Index * 20.0, Index * 5.0);
                        const float Radius = FMath::Max(500.0f, Component->Bounds.SphereRadius * 2.0f);
                        const FVector ImpulseOrigin = Component->Bounds.Origin -
                            Velocity.GetSafeNormal() * FMath::Max(50.0f, Radius * 0.25f);
                        if(!Step(TEXT("AddRadialImpulse"),[&]{Component->AddRadialImpulse(
                            ImpulseOrigin, Radius, Velocity.Size(),
                            ERadialImpulseFalloff::RIF_Constant, true);})) { return false; }
                        ++Session.RadialImpulseDispatchCount;
                        if(!Step(TEXT("AddImpulse"),[&]{Component->AddImpulse(Velocity, NAME_None, true);})) { return false; }
                        ++Session.UniformVelocityFieldDispatchCount;
                        if(!Step(TEXT("AddImpulseAtLocation"),[&]{Component->AddImpulseAtLocation(
                            Velocity.GetSafeNormal() * Velocity.Size() * 10000.0f,
                            Component->Bounds.Origin);})) { return false; }
                        ++Session.LocalizedImpulseDispatchCount;
                    }
                }
            }
            Session.bPostReleaseImpulseDispatched = true;
        }
    }
    if (Session.InitialTriggerMode == TEXT("strain_velocity") &&
        Session.bInitialStrainDispatchAttempted)
    {
        UpdateNativeReleasedDynamicDisplacement(Session);
        UpdateNativeAnchorStability(Session);
    }
    Session.ObservedTransformHashes.Add(HashLiveSolverDiagnostic(Session.Components));if(!Work->R.Error.IsEmpty()) { Session.ConfigurationFailure=Work->R.Error;CleanupNativeRecordSession(Session,false);return false; }
    if (Session.CapturedFrameCount >= Session.TargetFrameCount && Elapsed >= Session.TargetDurationSeconds)
    {
        Session.bReadyToFinish = true;
        CleanupNativeRecordSession(Session, true);
        return false;
    }
    return true;
}
FString HashVisiblePlaybackTransforms(
    const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Components,
    AChaosCacheManager* Manager)
{
    TArray<UGeometryCollectionComponent*> SortedComponents;
    for (const TWeakObjectPtr<UGeometryCollectionComponent>& WeakComponent : Components)
        if (UGeometryCollectionComponent* Component = WeakComponent.Get()) SortedComponents.Add(Component);
    SortedComponents.Sort([](const UGeometryCollectionComponent& A, const UGeometryCollectionComponent& B)
    {
        return A.GetPathName() < B.GetPathName();
    });
    FString Canonical;
    if (Manager)
    {
        Canonical += FString::Printf(TEXT("CACHE_COLLECTION|%s;"),
            Manager->CacheCollection ? *Manager->CacheCollection->GetPathName() : TEXT("<null>"));
        TArray<FString> ObservedIdentities;
        const TArray<FObservedComponent> ObservedCopy=Manager->GetObservedComponents();for (const FObservedComponent& Observed : ObservedCopy)
        {
            SDK(TEXT("read_visible_observed_identity"));const UPrimitiveComponent* ObservedComponent = Observed.GetComponent(Manager);if(!Guard()) { return FString(); }
            ObservedIdentities.Add(FString::Printf(TEXT("%s|%s"),
                ObservedComponent ? *ObservedComponent->GetPathName() : TEXT("<null>"),
                *Observed.CacheName.ToString()));
        }
        ObservedIdentities.Sort();
        for (const FString& Identity : ObservedIdentities)
            Canonical += FString::Printf(TEXT("OBSERVED_CACHE|%s;"), *Identity);
    }
    for (const UGeometryCollectionComponent* Component : SortedComponents)
    {
        const FTransform Transform = Component->GetComponentTransform();
        const FVector Location = Transform.GetLocation();
        const FRotator Rotation = Transform.Rotator();
        const FVector Scale = Transform.GetScale3D();
        Canonical += FString::Printf(
            TEXT("%s|C|%lld,%lld,%lld|%lld,%lld,%lld|%lld,%lld,%lld;"),
            *Component->GetPathName(),
            Quantize(Location.X * 10.0), Quantize(Location.Y * 10.0), Quantize(Location.Z * 10.0),
            Quantize(Rotation.Roll * 10.0), Quantize(Rotation.Pitch * 10.0), Quantize(Rotation.Yaw * 10.0),
            Quantize(Scale.X * 1000.0), Quantize(Scale.Y * 1000.0), Quantize(Scale.Z * 1000.0));
        // Geometry Collection cache playback is applied by
        // FGeometryCollectionCacheAdapter::SetRestState through SetRestState,
        // so the render-facing cache pose lives in the component's current
        // local rest transforms rather than its simulation DynamicCollection.
        // GetLocalRestTransforms returns a read-only copy and performs no pose
        // or PhysicsObject mutation.
        SDK(TEXT("read_visible_rest_transforms"));const TArray<FTransform> PlaybackRestTransforms = Component->GetLocalRestTransforms(false);if(!Guard()) { return FString(); }
        Canonical += FString::Printf(TEXT("PLAYBACK_REST_COUNT|%d;"), PlaybackRestTransforms.Num());
        for (int32 TransformIndex = 0; TransformIndex < PlaybackRestTransforms.Num(); ++TransformIndex)
        {
            const FTransform& RestTransform = PlaybackRestTransforms[TransformIndex];
            const FVector RestLocation = RestTransform.GetLocation();
            const FQuat RestRotation = RestTransform.GetRotation();
            Canonical += FString::Printf(
                TEXT("R|%d|%lld,%lld,%lld|%lld,%lld,%lld,%lld;"),
                TransformIndex,
                Quantize(RestLocation.X * 10.0),
                Quantize(RestLocation.Y * 10.0),
                Quantize(RestLocation.Z * 10.0),
                Quantize(RestRotation.X * 10000.0),
                Quantize(RestRotation.Y * 10000.0),
                Quantize(RestRotation.Z * 10000.0),
                Quantize(RestRotation.W * 10000.0));
        }
    }
    FTCHARToUTF8 Utf8(*Canonical);
    return Sha256Bytes(Utf8.Get(), Utf8.Length());
}

FString HashLiveSolverDiagnostic(
    const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Components)
{
    TArray<UGeometryCollectionComponent*> SortedComponents;
    for (const TWeakObjectPtr<UGeometryCollectionComponent>& WeakComponent : Components)
        if (UGeometryCollectionComponent* Component = WeakComponent.Get()) SortedComponents.Add(Component);
    SortedComponents.Sort([](const UGeometryCollectionComponent& A, const UGeometryCollectionComponent& B)
    {
        return A.GetPathName() < B.GetPathName();
    });
    FString Canonical;
    for (const UGeometryCollectionComponent* Component : SortedComponents)
    {
        bool Simulating=false;if(!Step(TEXT("observe_diagnostic_simulation"),[&]{Simulating=Component->IsSimulatingPhysics();},false)) { return FString(); }
        Canonical += FString::Printf(TEXT("%s|OBJECT_TYPE|%d|SIMULATING|%d;"),
            *Component->GetPathName(), static_cast<int32>(Component->ObjectType),
            Simulating ? 1 : 0);
        if (const FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection())
        {
            Canonical += FString::Printf(TEXT("DYNAMIC_COUNT|%d;"), Dynamic->GetNumTransforms());
            for (int32 TransformIndex = 0; TransformIndex < Dynamic->GetNumTransforms(); ++TransformIndex)
            {
                const FTransform LocalTransform(Dynamic->GetTransform(TransformIndex));
                const FVector Local = LocalTransform.GetLocation();
                const FRotator LocalRotation = LocalTransform.Rotator();
                Canonical += FString::Printf(
                    TEXT("D|%d|%lld,%lld,%lld|%lld,%lld,%lld;"), TransformIndex,
                    Quantize(Local.X * 10.0),
                    Quantize(Local.Y * 10.0),
                    Quantize(Local.Z * 10.0),
                    Quantize(LocalRotation.Roll * 10.0),
                    Quantize(LocalRotation.Pitch * 10.0),
                    Quantize(LocalRotation.Yaw * 10.0));
            }
        }
        SDK(TEXT("acquire_hash_physics_objects"));const TArray<Chaos::FPhysicsObject*> PhysicsObjects = Component->GetAllPhysicsObjects();if(!Guard()) { return FString(); }
        TArray<Chaos::FConstPhysicsObjectHandle> ConstPhysicsObjects;
        for (Chaos::FPhysicsObject* PhysicsObject : PhysicsObjects)
            if (PhysicsObject) ConstPhysicsObjects.Add(PhysicsObject);
        if (!ConstPhysicsObjects.IsEmpty())
        {
            FLockedReadPhysicsObjectExternalInterface Read =
                FPhysicsObjectExternalInterface::LockRead(ConstPhysicsObjects);
            Canonical += FString::Printf(TEXT("PHYSICS_OBJECT_COUNT|%d;"), ConstPhysicsObjects.Num());
            for (int32 ObjectIndex = 0; ObjectIndex < ConstPhysicsObjects.Num(); ++ObjectIndex)
            {
                const Chaos::FConstPhysicsObjectHandle PhysicsObject = ConstPhysicsObjects[ObjectIndex];
                const FTransform PhysicsTransform = Read->GetTransform(PhysicsObject);
                const FVector PhysicsLocation = PhysicsTransform.GetLocation();
                const FRotator PhysicsRotation = PhysicsTransform.Rotator();
                const TArrayView<const Chaos::FConstPhysicsObjectHandle> OneObject =
                    MakeArrayView(&PhysicsObject, 1);
                const uint8 StateBits =
                    (Read->AreAllKinematic(OneObject) ? 1u : 0u) |
                    (Read->AreAllSleeping(OneObject) ? 2u : 0u) |
                    (Read->AreAllRigidBody(OneObject) ? 4u : 0u) |
                    (Read->AreAllDynamic(OneObject) ? 8u : 0u) |
                    (Read->AreAllDisabled(OneObject) ? 16u : 0u);
                Canonical += FString::Printf(
                    TEXT("P|%d|%lld,%lld,%lld|%lld,%lld,%lld|%u;"), ObjectIndex,
                    Quantize(PhysicsLocation.X * 10.0),
                    Quantize(PhysicsLocation.Y * 10.0),
                    Quantize(PhysicsLocation.Z * 10.0),
                    Quantize(PhysicsRotation.Roll * 10.0),
                    Quantize(PhysicsRotation.Pitch * 10.0),
                    Quantize(PhysicsRotation.Yaw * 10.0), StateBits);
            }
        }
    }
    FTCHARToUTF8 Utf8(*Canonical);
    return Sha256Bytes(Utf8.Get(), Utf8.Length());
}

TSharedPtr<FJsonObject> InspectChaosCacheTracks(
    const UChaosCache* Cache,
    const TArray<double>& RequestedSampleTimes)
{
    TSharedPtr<FJsonObject> Inspection = MakeShared<FJsonObject>();
    if (!Cache)
    {
        Inspection->SetStringField(TEXT("cache_name"), TEXT("<null>"));
        Inspection->SetBoolField(TEXT("sample_time_hash_variation"), false);
        return Inspection;
    }

    for(const auto& Particle:Cache->ParticleTracks) { const auto& Track=Particle.TransformData;const int32 Count=FMath::Min3(Track.KeyTimestamps.Num(),Track.RawTransformTrack.PosKeys.Num(),Track.RawTransformTrack.RotKeys.Num());for(int32 I=0;I<Count;++I) { if(!FMath::IsFinite(Track.KeyTimestamps[I])||(I>0&&Track.KeyTimestamps[I]<Track.KeyTimestamps[I-1])||Track.RawTransformTrack.PosKeys[I].ContainsNaN()||Track.RawTransformTrack.RotKeys[I].ContainsNaN()) { Work->Fail(TEXT("cache_track_invalid"),TEXT("Cache transform track has nonfinite values or descending timestamps"));Inspection->SetBoolField(TEXT("sample_time_hash_variation"),false);return Inspection; } } }
    int32 TotalTransformKeyCount = 0;
    int32 TracksWithTransformKeys = 0;
    int32 TracksWithMultipleTransformKeys = 0;
    int32 TracksWithTransformKeyVariation = 0;
    const auto GetReadOnlyKeyCount = [](const FParticleTransformTrack& Track)
    {
        return FMath::Min3(
            Track.KeyTimestamps.Num(),
            Track.RawTransformTrack.PosKeys.Num(),
            Track.RawTransformTrack.RotKeys.Num());
    };
    const auto ReadOnlyTransformAt = [](const FParticleTransformTrack& Track, int32 KeyIndex)
    {
        return FTransform(
            FQuat(Track.RawTransformTrack.RotKeys[KeyIndex]),
            FVector(Track.RawTransformTrack.PosKeys[KeyIndex]));
    };
    const auto EvaluateReadOnly = [&GetReadOnlyKeyCount, &ReadOnlyTransformAt](
        const FParticleTransformTrack& Track, float Time)
    {
        const int32 KeyCount = GetReadOnlyKeyCount(Track);
        if (KeyCount == 0)
        {
            return FTransform::Identity;
        }
        const float ClampedTime = FMath::Clamp(
            Time, Track.KeyTimestamps[0], Track.KeyTimestamps[KeyCount - 1]);
        int32 UpperKeyIndex = 0;
        while (UpperKeyIndex < KeyCount && Track.KeyTimestamps[UpperKeyIndex] <= ClampedTime)
        {
            ++UpperKeyIndex;
        }
        UpperKeyIndex = FMath::Clamp(UpperKeyIndex, 0, KeyCount - 1);
        const int32 LowerKeyIndex = FMath::Max(0, UpperKeyIndex - 1);
        const FTransform LowerTransform = ReadOnlyTransformAt(Track, LowerKeyIndex);
        const FTransform UpperTransform = ReadOnlyTransformAt(Track, UpperKeyIndex);
        const float Interval = Track.KeyTimestamps[UpperKeyIndex] - Track.KeyTimestamps[LowerKeyIndex];
        const float Alpha = Interval > 0.0f
            ? (ClampedTime - Track.KeyTimestamps[LowerKeyIndex]) / Interval : 0.0f;
        FTransform Result;
        Result.Blend(LowerTransform, UpperTransform, Alpha);
        return Result;
    };
    for (const FPerParticleCacheData& ParticleTrack : Cache->ParticleTracks)
    {
        const FParticleTransformTrack& TransformTrack = ParticleTrack.TransformData;
        const int32 KeyCount = GetReadOnlyKeyCount(TransformTrack);
        TotalTransformKeyCount += KeyCount;
        TracksWithTransformKeys += KeyCount > 0 ? 1 : 0;
        TracksWithMultipleTransformKeys += KeyCount > 1 ? 1 : 0;
        if (KeyCount > 1)
        {
            const FTransform FirstTransform = ReadOnlyTransformAt(TransformTrack, 0);
            bool bTrackVaries = false;
            for (int32 KeyIndex = 1; KeyIndex < KeyCount; ++KeyIndex)
            {
                if (!FirstTransform.Equals(ReadOnlyTransformAt(TransformTrack, KeyIndex), 0.001f))
                {
                    bTrackVaries = true;
                    break;
                }
            }
            TracksWithTransformKeyVariation += bTrackVaries ? 1 : 0;
        }
    }

    TArray<TSharedPtr<FJsonValue>> SampleHashes;
    TSet<FString> UniqueSampleHashes;
    for (double RequestedTime : RequestedSampleTimes)
    {
        FString Canonical;
        for (int32 TrackIndex = 0; TrackIndex < Cache->ParticleTracks.Num(); ++TrackIndex)
        {
            const FTransform Transform = EvaluateReadOnly(
                Cache->ParticleTracks[TrackIndex].TransformData,
                static_cast<float>(RequestedTime));
            const FVector Location = Transform.GetLocation();
            const FQuat Rotation = Transform.GetRotation();
            Canonical += FString::Printf(
                TEXT("%d|%lld,%lld,%lld|%lld,%lld,%lld,%lld;"),
                TrackIndex,
                Quantize(Location.X * 1000.0),
                Quantize(Location.Y * 1000.0),
                Quantize(Location.Z * 1000.0),
                Quantize(Rotation.X * 100000.0),
                Quantize(Rotation.Y * 100000.0),
                Quantize(Rotation.Z * 100000.0),
                Quantize(Rotation.W * 100000.0));
        }
        FTCHARToUTF8 Utf8(*Canonical);
        const FString Hash = Sha256Bytes(Utf8.Get(), Utf8.Length());
        UniqueSampleHashes.Add(Hash);
        TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
        Sample->SetNumberField(TEXT("time_seconds"), RequestedTime);
        Sample->SetStringField(TEXT("evaluated_transform_sha256"), Hash);
        SampleHashes.Add(MakeShared<FJsonValueObject>(Sample));
    }

    Inspection->SetStringField(TEXT("cache_name"), Cache->GetName());
    Inspection->SetNumberField(TEXT("recorded_frame_count"), Cache->NumRecordedFrames);
    Inspection->SetNumberField(TEXT("recorded_duration_seconds"), Cache->GetDuration());
    Inspection->SetNumberField(TEXT("particle_track_count"), Cache->ParticleTracks.Num());
    Inspection->SetNumberField(TEXT("total_transform_key_count"), TotalTransformKeyCount);
    Inspection->SetNumberField(TEXT("tracks_with_transform_keys"), TracksWithTransformKeys);
    Inspection->SetNumberField(TEXT("tracks_with_multiple_transform_keys"), TracksWithMultipleTransformKeys);
    Inspection->SetNumberField(TEXT("tracks_with_transform_key_variation"), TracksWithTransformKeyVariation);
    Inspection->SetArrayField(TEXT("requested_time_evaluated_hashes"), SampleHashes);
    Inspection->SetNumberField(TEXT("unique_requested_time_hash_count"), UniqueSampleHashes.Num());
    Inspection->SetBoolField(TEXT("sample_time_hash_variation"), UniqueSampleHashes.Num() > 1);
    return Inspection;
}

bool IsPhysicsObjectPlaybackSuppressed(const UGeometryCollectionComponent* Component)
{
    if(!Component||!Guard()) { return false; }bool Sim=false;if(!Step(TEXT("observe_playback_simulation"),[&]{Sim=Component->IsSimulatingPhysics();},false)) { return false; }
    if (Sim || Component->ObjectType != EObjectStateTypeEnum::Chaos_Object_Kinematic)
    {
        return false;
    }
    SDK(TEXT("acquire_current_physics_objects"));const TArray<Chaos::FPhysicsObject*> Objects = Component->GetAllPhysicsObjects();
    if (Objects.IsEmpty())
    {
        return true;
    }
    TArray<Chaos::FConstPhysicsObjectHandle> ConstObjects;
    for (Chaos::FPhysicsObject* Object : Objects)
        if (Object) ConstObjects.Add(Object);
    if (ConstObjects.IsEmpty())
    {
        return true;
    }
    if(!Guard()) { return false; }SDK(TEXT("read_playback_physics_state"));FLockedReadPhysicsObjectExternalInterface Read = FPhysicsObjectExternalInterface::LockRead(ConstObjects);
    for (Chaos::FConstPhysicsObjectHandle Object : ConstObjects)
    {
        if (!Read->AreAllKinematic(MakeArrayView(&Object, 1)) &&
            !Read->AreAllDisabled(MakeArrayView(&Object, 1)))
        {
            return false;
        }
    }
    return true;
}

bool TickNativeCacheValidationBody(float)
{
    if (!ActiveNativeValidationSession)
    {
        return false;
    }
    FNativeCacheValidationSession& Session = *ActiveNativeValidationSession;
    UWorld* World = Session.World.Get();
    AChaosCacheManager* Manager = Session.Manager.Get();
    UChaosCacheCollection* Collection = Session.Collection.Get();
    if (!World || !Manager || !Collection)
    {
        Session.bTimedOut = true;
        CleanupNativeValidationSession(Session);
        return false;
    }
    const double Elapsed = World->GetTimeSeconds() - Session.BeginWorldTime;
    if (Elapsed >= Session.TimeoutSeconds||FPlatformTime::Seconds()-Session.MonotonicStart>=Session.TimeoutSeconds)
    {
        Session.bTimedOut = true;
        CleanupNativeValidationSession(Session);
        return false;
    }
    if (Session.ScheduledFrames.IsValidIndex(Session.ScheduleIndex) &&
        GFrameCounter >= Session.DispatchFrame + Session.RequiredSettleFrameDelta)
    {
        if (Session.bEndpointPrimePending)
        {
            if (ULevelSequencePlayer* Player = Session.SequencePlayer.Get())
            {
                FMovieSceneSequencePlaybackParams EndpointParams(
                    FFrameTime(Session.EndpointFrame), EUpdatePositionMethod::Jump);
                EndpointParams.bHasJumped = true;
                if(!Step(TEXT("SetPlaybackPosition"),[&]{Player->SetPlaybackPosition(EndpointParams);})) { return false; }
            }
            Session.bEndpointPrimePending = false;
            Session.DispatchFrame = GFrameCounter;
            return true;
        }
        Session.CapturedHashes.Add(HashVisiblePlaybackTransforms(Session.Components, Manager));
        Session.CapturedSolverDiagnosticHashes.Add(
            HashLiveSolverDiagnostic(Session.Components));
        Session.CapturedManagerStartTimes.Add(Manager->StartTime);
        const ULevelSequencePlayer* Player = Session.SequencePlayer.Get();
        Session.CapturedPlayerFrames.Add(Player ? Player->GetCurrentTime().Time.FrameNumber.Value : INDEX_NONE);
        int32 SuppressedCount = 0;
        for (const TWeakObjectPtr<UGeometryCollectionComponent>& WeakComponent : Session.Components)
        {
            SuppressedCount += IsPhysicsObjectPlaybackSuppressed(WeakComponent.Get()) ? 1 : 0;
        }
        Session.MinimumSuppressedCount = FMath::Min(Session.MinimumSuppressedCount, SuppressedCount);
        ++Session.ScheduleIndex;
        if (Session.ScheduledFrames.IsValidIndex(Session.ScheduleIndex))
        {
            DispatchNativeValidationFrame(Session);
        }
        else
        {
            Session.bReadyToFinish = true;
            CleanupNativeValidationSession(Session);
            return false;
        }
    }
    return true;
}

TSharedPtr<FJsonObject> RecordChaosCacheNative(const TSharedPtr<FJsonObject>& Params)
{
    const FString Operation = GetString(Params, TEXT("operation"), TEXT("begin"));
    if (Operation == TEXT("abort"))
    {
        if (ActiveNativeRecordSession)
        {
            auto Previous=ActiveNativeRecordSession;CleanupNativeRecordSession(*Previous,false);Work->R.Data->SetObjectField(TEXT("previous_session_execution"),Previous->Evidence?Previous->Evidence->SessionSnapshot():MakeShared<FJsonObject>());ActiveNativeRecordSession.Reset();if(!Previous->bCleanupComplete) { return CommandError(TEXT("Previous recording cleanup is incomplete; new mutation was not started")); }
        }
        TSharedPtr<FJsonObject> Data = Success(TEXT("record_chaos_cache"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetBoolField(TEXT("autonomous_cleanup_complete"), true);
        Data->SetStringField(TEXT("assertion_status"), TEXT("PASS"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    UWorld* World = GetActivePlayWorldForChaosCinematics();
    if (!World || !World->IsGameWorld())
    {
        return CommandError(TEXT("record_chaos_cache requires the active fresh PIE/SIE game world"));
    }
    if (Operation == TEXT("begin"))
    {
        if (ActiveNativeRecordSession)
        {
            auto Previous=ActiveNativeRecordSession;CleanupNativeRecordSession(*Previous,false);Work->R.Data->SetObjectField(TEXT("previous_session_execution"),Previous->Evidence?Previous->Evidence->SessionSnapshot():MakeShared<FJsonObject>());ActiveNativeRecordSession.Reset();if(!Previous->bCleanupComplete) { return CommandError(TEXT("Previous recording cleanup is incomplete; new mutation was not started")); }
        }
        if(!PreflightRecord(Params)) { return CommandError(Work->R.Error); }
        AChaosCacheManager* Manager = FindActor<AChaosCacheManager>(World, GetString(Params, TEXT("cache_manager_actor_name")));
        const TArray<FString> ActorNames = GetStringArray(Params, TEXT("geometry_collection_actor_names"));
        const TArray<UGeometryCollectionComponent*> Components = ResolveGeometryComponents(World, ActorNames);
        const FString CachePath = GetString(Params, TEXT("cache_collection_path"));
        if (!Manager || Components.IsEmpty() || Components.Num()!=ActorNames.Num() || !IsAllowedOutputPath(CachePath))
        {
            return CommandError(TEXT("record begin requires cache manager, geometry_collection_actor_names, and a Cinematic-root cache path"));
        }
        if(TargetOverlap(ActiveNativeValidationSession,Manager,Components,CachePath)) { return CommandError(TEXT("Recording conflicts with an unfinished validation manager/component/cache target")); }
        UChaosCacheCollection* Collection=Work->Load<UChaosCacheCollection>(CachePath,TEXT("load_record_cache_collection"));if(!Guard()||!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }
        if (!Collection)
        {
            const FString CachePackageName = FPackageName::ObjectPathToPackageName(CachePath);
            const FString ExactObject=CachePackageName+TEXT(".")+FPackageName::GetShortName(CachePackageName);SDK(TEXT("admit_new_cache_collection"));if(!FPackageName::IsValidLongPackageName(CachePackageName)||FindObject<UObject>(nullptr,*ExactObject)||FindPackage(nullptr,*CachePackageName)||FPackageName::DoesPackageExist(CachePackageName)||FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(ExactObject)).IsValid()||!Guard()) { return CommandError(TEXT("Existing wrong-class/unloadable cache output or invalid package cannot be overwritten")); }Write(TEXT("create_cache_package"));UPackage* Package = CreatePackage(*CachePackageName);if(!Work->Track(Package,TEXT("new_cache_package"),nullptr,true)) { return CommandError(Work->R.Error); }
            Write(TEXT("new_cache_collection"));Collection = NewObject<UChaosCacheCollection>(
                Package, *FPackageName::GetShortName(CachePackageName), RF_Public | RF_Standalone | RF_Transactional);
            if(!Work->Track(Collection,TEXT("new_cache_collection"),Package,true)||!Step(TEXT("register_new_cache_collection"),[&]{FAssetRegistryModule::AssetCreated(Collection);})) { return CommandError(Work->R.Error); }
        }
        const TArray<FString> RequestedCacheNames = GetStringArray(Params, TEXT("cache_names"));
        if (!RequestedCacheNames.IsEmpty() && RequestedCacheNames.Num() != Components.Num())
        {
            return CommandError(TEXT("cache_names must be empty or exactly match geometry_collection_actor_names"));
        }
        if (!RequestedCacheNames.IsEmpty() && TSet<FString>(RequestedCacheNames).Num() != RequestedCacheNames.Num())
        {
            return CommandError(TEXT("cache_names must be unique"));
        }
        UChaosCacheCollection* OriginalManagerCacheCollection = Manager->CacheCollection;
        const ECacheMode OriginalManagerCacheMode = Manager->CacheMode;
        const EStartMode OriginalManagerStartMode = Manager->StartMode;
        const TArray<FObservedComponent> OriginalObservedComponents = Manager->GetObservedComponents();
        ActiveNativeRecordSession = MakeShared<FNativeCacheRecordSession>();
        FNativeCacheRecordSession& Session = *ActiveNativeRecordSession;
        Session.Evidence=CallOwner;Session.MonotonicStart=FPlatformTime::Seconds();Work->OperationComplete=false;
        Session.SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);Work->SessionId=Session.SessionId;
        Session.World = World;
        Session.Manager = Manager;
        Session.Collection = Collection;
        Session.OriginalManagerCacheCollection = OriginalManagerCacheCollection;
        Session.OriginalManagerCacheMode = OriginalManagerCacheMode;
        Session.OriginalManagerStartMode = OriginalManagerStartMode;
        Session.OriginalObservedComponents = OriginalObservedComponents;
        Session.bHasOriginalManagerState = true;
        for(auto* C:Components) { Session.Components.Add(C); }Session.ComponentActorNames=ActorNames;
        if(!CaptureOriginalFlags(Manager,Components,Session.OriginalComponentFlags)) { CleanupNativeRecordSession(Session,false);return CommandError(Work->R.Error); }
        Session.ComponentTouched.Init(false,Components.Num());
        for(auto* C:Components) { const FOriginalComponentFlags* F=Session.OriginalComponentFlags.FindByPredicate([&](const FOriginalComponentFlags& V){return V.Component.Get()==C;});if(!F) { return CommandError(TEXT("Captured component entry settings are unavailable")); }Session.OriginalComponentRegisteredStates.Add(F->Registered);Session.OriginalSimulatePhysicsStates.Add(F->Simulating);Session.OriginalBodySimulateStates.Add(F->BodySimulate);Session.OriginalNotifyBreaksStates.Add(F->NotifyBreaks);Session.OriginalObjectTypes.Add(F->ObjectType);Session.OriginalInitializationFields.Add(C->InitializationFields); }
        if(!CaptureTargets(Manager,Collection,Components)) { CleanupNativeRecordSession(Session,false);return CommandError(Work->R.Error); }Session.bManagerTouched=true;if(!Step(TEXT("EndEvaluate_before_new_session"),[&]{Manager->EndEvaluate();})) { CleanupNativeRecordSession(Session,false);return CommandError(Work->R.Error); }
        const FString CacheNamePrefix = GetString(Params, TEXT("cache_name_prefix"), TEXT("ChaosCinematic"));
        TArray<FString> AppliedCacheNames;
        for (int32 Index = 0; Index < Components.Num(); ++Index)
        {
            const FString CacheName = RequestedCacheNames.IsEmpty()
                ? FString::Printf(TEXT("%s_%02d"), *CacheNamePrefix, Index) : RequestedCacheNames[Index];
            AppliedCacheNames.Add(CacheName);
        }

        Session.AppliedCacheNames = AppliedCacheNames;
        Session.bUniqueCacheNames = TSet<FString>(AppliedCacheNames).Num() == AppliedCacheNames.Num();
        Session.bCachePackageDirtyBefore = Collection->GetOutermost()->IsDirty();
        Session.RequestFrame = GFrameCounter;
        Session.EndEvaluateFrame = GFrameCounter;
        Session.RequestWorldTime = World->GetTimeSeconds();
        Session.TargetFrameCount = FMath::Max(60, CastI32(GetNumber(Params, TEXT("frame_rate"), 30) *
            GetNumber(Params, TEXT("duration_seconds"), 2.0),TEXT("frame_rate")));
        Session.TargetDurationSeconds = FMath::Max(2.0, GetNumber(Params, TEXT("duration_seconds"), 2.0));
        Session.TimeoutSeconds = FMath::Max(Session.TargetDurationSeconds + 2.0, GetNumber(Params, TEXT("timeout_seconds"), 10.0));
        TryVector(Params, TEXT("motion_velocity"), Session.MotionVelocity);
        Session.InitialTriggerMode = GetString(Params, TEXT("initial_trigger_mode"), TEXT("strain_velocity"));
        Session.InitialTriggerPropagationDepth = FMath::Clamp(
            CastI32(GetNumber(Params, TEXT("initial_trigger_propagation_depth"), 0.0),TEXT("initial_trigger_propagation_depth")),
            0, 32);
        Session.InitialTriggerDelayFrames = FMath::Clamp(
            CastI32(GetNumber(Params, TEXT("initial_trigger_delay_frames"), 15.0),TEXT("initial_trigger_delay_frames")),
            1, 240);
        Session.bRequireSavedCacheTransformVariation = GetBool(
            Params, TEXT("require_saved_cache_transform_variation"), false);
        Session.bPreserveInitializationFieldsDuringRecord = GetBool(
            Params, TEXT("preserve_initialization_fields_during_record"), false);
        Session.bUseNativeAfterReleaseAnchorFallback = GetBool(
            Params, TEXT("use_native_after_release_anchor_fallback"), true);
        if (Session.InitialTriggerMode != TEXT("strain_velocity") && Session.InitialTriggerMode != TEXT("none"))
        {
            CleanupNativeRecordSession(Session, false);
            ActiveNativeRecordSession.Reset();
            return CommandError(TEXT("initial_trigger_mode must be strain_velocity or none"));
        }

        if (AChaosSolverActor* Solver = FindActor<AChaosSolverActor>(World, GetString(Params, TEXT("solver_actor_name"))))
        {
            Session.Solver = Solver;
            Session.OriginalSolverConfiguration = Solver->Properties;
            Session.bHasSolverConfiguration = true;
            FChaosSolverConfiguration Applied = Solver->Properties;
            Applied.DestructionSettings.PerAdvanceBreaksAllowed = FMath::Max(0, CastI32(
                GetNumber(Params, TEXT("per_advance_breaks_allowed"), TNumericLimits<int32>::Max()),TEXT("per_advance_breaks_allowed")));
            Applied.DestructionSettings.PerAdvanceBreaksRescheduleLimit = FMath::Max(0, CastI32(
                GetNumber(Params, TEXT("per_advance_breaks_reschedule_limit"), TNumericLimits<int32>::Max()),TEXT("per_advance_breaks_reschedule_limit")));
            Applied.DestructionSettings.ClusteringParticleReleaseThrottlingMinCount = CastI32(
                GetNumber(Params, TEXT("particle_release_throttling_min_count"), INDEX_NONE),TEXT("particle_release_throttling_min_count"));
            Applied.DestructionSettings.ClusteringParticleReleaseThrottlingMaxCount = CastI32(
                GetNumber(Params, TEXT("particle_release_throttling_max_count"), INDEX_NONE),TEXT("particle_release_throttling_max_count"));
            ApplySolverConfiguration(Solver, Applied);
        }
        if (Session.InitialTriggerMode == TEXT("strain_velocity"))
        {
            Session.DeferredBreakingItems.Init(INDEX_NONE, Session.Components.Num());
            Session.DeferredBreakingRootParentIndices.Init(INDEX_NONE, Session.Components.Num());
            Session.DeferredBreakingRootChildCounts.Init(0, Session.Components.Num());
            Session.InitialTriggerImpactPoints.Init(FVector::ZeroVector, Session.Components.Num());
            Session.InitialTriggerRadii.Init(100.0f, Session.Components.Num());
            Session.InitialPhysicsObjectCounts.Init(0, Session.Components.Num());
            Session.InitialDisabledPhysicsObjectCounts.Init(0, Session.Components.Num());
            Session.CurrentDisabledPhysicsObjectCounts.Init(0, Session.Components.Num());
            Session.ComponentReleaseObserved.Init(false, Session.Components.Num());
            for (int32 Index = 0; Index < Session.Components.Num(); ++Index)
            {
                if (UGeometryCollectionComponent* Component = Session.Components[Index].Get())
                {
                    Session.InitialTriggerImpactPoints[Index] = Component->Bounds.Origin +
                        FVector(0.0, 0.0, Component->Bounds.BoxExtent.Z * 0.65);
                    Session.InitialTriggerRadii[Index] = FMath::Clamp(
                        CastFloat(GetNumber(
                            Params, TEXT("initial_trigger_radius"),
                            Component->Bounds.BoxExtent.Z * 0.5),TEXT("initial_trigger_radius")),
                        1.0f, 1000.0f);
                    int32 RootItem = ResolveRootItem(Component);
                    const int32 RequestedRootItem = CastI32(GetNumber(
                        Params, TEXT("initial_trigger_root_transform_index"), INDEX_NONE),TEXT("initial_trigger_root_transform_index"));
                    if (RequestedRootItem != INDEX_NONE)
                    {
                        const UGeometryCollection* RequestedRest =
                            Component->GetRestCollection();
                        const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe>
                            RequestedCollection = RequestedRest
                                ? RequestedRest->GetGeometryCollection() : nullptr;
                        if (!RequestedCollection.IsValid() ||
                            !RequestedCollection->Parent.IsValidIndex(RequestedRootItem) ||
                            RequestedCollection->Parent[RequestedRootItem] != INDEX_NONE)
                        {
                            CleanupNativeRecordSession(Session, false);
                            ActiveNativeRecordSession.Reset();
                            return CommandError(
                                TEXT("initial_trigger_root_transform_index must identify a parentless root"));
                        }
                        RootItem = RequestedRootItem;
                    }
                    if (RootItem != INDEX_NONE)
                    {
                        Session.DeferredBreakingItems[Index] = RootItem;
                        if (const UGeometryCollection* Rest = Component->GetRestCollection())
                        {
                            const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> RestCollection =
                                Rest->GetGeometryCollection();
                            if (RestCollection.IsValid())
                            {
                                Session.DeferredBreakingRootParentIndices[Index] =
                                    RestCollection->Parent.IsValidIndex(RootItem)
                                        ? RestCollection->Parent[RootItem] : INDEX_NONE;
                                Session.DeferredBreakingRootChildCounts[Index] =
                                    RestCollection->Children.IsValidIndex(RootItem)
                                        ? RestCollection->Children[RootItem].Num() : 0;
                            }
                        }
                    }
                }
            }
        }
        Session.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            return TickNativeCacheRecord(0.0f);
        }));
        TSharedPtr<FJsonObject> Data = Success(TEXT("record_chaos_cache"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("session_id"), Session.SessionId);
        Data->SetNumberField(TEXT("observed_component_count"), Components.Num());
        TArray<TSharedPtr<FJsonValue>> AppliedCacheNameValues;
        for (const FString& CacheName : AppliedCacheNames)
            AppliedCacheNameValues.Add(MakeShared<FJsonValueString>(CacheName));
        Data->SetArrayField(TEXT("applied_cache_names"), AppliedCacheNameValues);
        Data->SetBoolField(TEXT("unique_cache_name_gate"), Session.bUniqueCacheNames);
        Data->SetNumberField(TEXT("target_frame_count"), Session.TargetFrameCount);
        Data->SetNumberField(TEXT("target_duration_seconds"), Session.TargetDurationSeconds);
        Data->SetStringField(TEXT("configuration_phase"), Session.ConfigurationPhase);
        Data->SetBoolField(TEXT("recording_active"), Session.bRecordingActive);
        Data->SetNumberField(TEXT("request_frame"), static_cast<double>(Session.RequestFrame));
        Data->SetNumberField(TEXT("end_evaluate_frame"), static_cast<double>(Session.EndEvaluateFrame));
        Data->SetNumberField(TEXT("manager_configure_frame"), 0.0);
        Data->SetNumberField(TEXT("physics_state_recreate_frame"), 0.0);
        Data->SetNumberField(TEXT("physics_state_ready_frame"), 0.0);
        Data->SetNumberField(TEXT("begin_evaluate_frame"), 0.0);
        Data->SetNumberField(TEXT("physics_state_recreate_count"), 0);
        Data->SetNumberField(TEXT("dynamic_collection_reset_count"), 0);
        Data->SetNumberField(TEXT("component_register_count"), 0);
        Data->SetNumberField(TEXT("component_unregister_on_restore_count"), 0);
        Data->SetNumberField(TEXT("simulate_physics_enable_count"), 0);
        Data->SetNumberField(TEXT("simulate_physics_restore_count"), 0);
        Data->SetNumberField(TEXT("physics_state_destroy_on_restore_count"), 0);
        Data->SetNumberField(TEXT("physics_proxy_ready_count"), 0);
        Data->SetArrayField(TEXT("original_component_registered_states"), {});
        Data->SetArrayField(TEXT("original_simulate_physics_states"), {});
        Data->SetArrayField(TEXT("physics_object_counts_before_recreate"), {});
        Data->SetArrayField(TEXT("physics_object_counts_after_recreate"), {});
        Data->SetBoolField(TEXT("physics_state_ready_before_begin_evaluate"), false);
        Data->SetBoolField(TEXT("component_simulation_settings_restored"), false);
        Data->SetBoolField(TEXT("component_registration_settings_restored"), false);
        Data->SetNumberField(TEXT("initial_external_strain_dispatch_count"), Session.InitialExternalStrainDispatchCount);
        Data->SetNumberField(TEXT("initial_strain_dispatch_attempt_count"),
            Session.bInitialStrainDispatchAttempted ? 1 : 0);
        Data->SetNumberField(TEXT("initial_external_strain_and_velocity_dispatch_count"), Session.InitialMotionDispatchCount);
        Data->SetNumberField(TEXT("initial_breaking_velocity_dispatch_count"), Session.InitialMotionDispatchCount);
        Data->SetBoolField(TEXT("breaking_velocity_deferred_to_later_tick"),
            false);
        Data->SetBoolField(TEXT("breaking_velocity_armed_before_release"), false);
        TArray<TSharedPtr<FJsonValue>> RootEvidence;
        for (int32 Index = 0; Index < Session.DeferredBreakingItems.Num(); ++Index)
        {
            TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
            Root->SetNumberField(TEXT("component_index"), Index);
            Root->SetNumberField(TEXT("root_item"), Session.DeferredBreakingItems[Index]);
            Root->SetNumberField(TEXT("parent_index"), Session.DeferredBreakingRootParentIndices[Index]);
            Root->SetNumberField(TEXT("direct_child_count"), Session.DeferredBreakingRootChildCounts[Index]);
            Root->SetArrayField(TEXT("impact_point"),
                VectorJson(Session.InitialTriggerImpactPoints.IsValidIndex(Index)
                    ? Session.InitialTriggerImpactPoints[Index] : FVector::ZeroVector));
            Root->SetNumberField(TEXT("impact_radius_cm"),
                Session.InitialTriggerRadii.IsValidIndex(Index)
                    ? Session.InitialTriggerRadii[Index] : 0.0f);
            Root->SetNumberField(TEXT("propagation_depth"),
                Session.InitialTriggerPropagationDepth);
            Root->SetBoolField(TEXT("top_level_cluster"),
                Session.DeferredBreakingItems[Index] != INDEX_NONE &&
                Session.DeferredBreakingRootParentIndices[Index] == INDEX_NONE &&
                Session.DeferredBreakingRootChildCounts[Index] > 0);
            RootEvidence.Add(MakeShared<FJsonValueObject>(Root));
        }
        Data->SetArrayField(TEXT("initial_trigger_root_hierarchy"), RootEvidence);
        Data->SetNumberField(TEXT("breaking_velocity_requested_deferred_frame_delta"), 0);
        Data->SetNumberField(TEXT("radial_impulse_dispatch_count"), 0);
        Data->SetNumberField(TEXT("uniform_velocity_field_dispatch_count"), 0);
        Data->SetNumberField(TEXT("localized_impulse_dispatch_count"), 0);
        Data->SetNumberField(TEXT("released_dynamic_physics_object_count"), 0);
        Data->SetNumberField(TEXT("maximum_released_dynamic_displacement_cm"), 0.0);
        Data->SetBoolField(TEXT("released_dynamic_displacement_observed"), false);
        Data->SetNumberField(TEXT("direct_velocity_apply_count"), 0);
        Data->SetNumberField(TEXT("retry_velocity_dispatch_count"), 0);
        Data->SetStringField(TEXT("initial_trigger_mode"), Session.InitialTriggerMode);
        Data->SetBoolField(TEXT("record_observation_only"), Session.InitialTriggerMode == TEXT("none"));
        Data->SetBoolField(TEXT("per_frame_motion_manipulation"), false);
        Data->SetBoolField(TEXT("require_saved_cache_transform_variation"),
            Session.bRequireSavedCacheTransformVariation);
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (!ActiveNativeRecordSession || ActiveNativeRecordSession->World.Get() != World)
    {
        return CommandError(TEXT("No active native cache recording session"));
    }
    FNativeCacheRecordSession& Session = *ActiveNativeRecordSession;
    const double Elapsed = Session.bRecordingActive
        ? World->GetTimeSeconds() - Session.BeginWorldTime : 0.0;
    if (Operation == TEXT("sample"))
    {
        TSharedPtr<FJsonObject> Data = Success(TEXT("record_chaos_cache"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("session_id"), Session.SessionId);
        Data->SetNumberField(TEXT("captured_frame_count"), Session.CapturedFrameCount);
        Data->SetNumberField(TEXT("recording_elapsed_seconds"), Elapsed);
        Data->SetStringField(TEXT("configuration_phase"), Session.ConfigurationPhase);
        Data->SetStringField(TEXT("configuration_failure"), Session.ConfigurationFailure);
        Data->SetBoolField(TEXT("manager_configured"), Session.bManagerConfigured);
        Data->SetBoolField(TEXT("recording_active"), Session.bRecordingActive);
        Data->SetNumberField(TEXT("request_frame"), static_cast<double>(Session.RequestFrame));
        Data->SetNumberField(TEXT("end_evaluate_frame"), static_cast<double>(Session.EndEvaluateFrame));
        Data->SetNumberField(TEXT("manager_configure_frame"),
            static_cast<double>(Session.ManagerConfigureFrame));
        Data->SetNumberField(TEXT("physics_state_recreate_frame"),
            static_cast<double>(Session.PhysicsStateRecreateFrame));
        Data->SetNumberField(TEXT("physics_state_ready_frame"),
            static_cast<double>(Session.PhysicsStateReadyFrame));
        Data->SetNumberField(TEXT("begin_evaluate_frame"),
            static_cast<double>(Session.BeginEvaluateFrame));
        Data->SetNumberField(TEXT("physics_state_recreate_count"),
            Session.PhysicsStateRecreateCount);
        Data->SetNumberField(TEXT("dynamic_collection_reset_count"),
            Session.DynamicCollectionResetCount);
        Data->SetNumberField(TEXT("component_register_count"),
            Session.ComponentRegisterCount);
        Data->SetNumberField(TEXT("component_unregister_on_restore_count"),
            Session.ComponentUnregisterOnRestoreCount);
        Data->SetNumberField(TEXT("simulate_physics_enable_count"),
            Session.SimulatePhysicsEnableCount);
        Data->SetNumberField(TEXT("simulate_physics_restore_count"),
            Session.SimulatePhysicsRestoreCount);
        Data->SetNumberField(TEXT("object_type_dynamic_configure_count"),
            Session.ObjectTypeDynamicConfigureCount);
        Data->SetNumberField(TEXT("object_type_restore_count"),
            Session.ObjectTypeRestoreCount);
        Data->SetNumberField(TEXT("physics_state_destroy_on_restore_count"),
            Session.PhysicsStateDestroyOnRestoreCount);
        Data->SetNumberField(TEXT("physics_proxy_ready_count"),
            Session.PhysicsProxyReadyCount);
        Data->SetArrayField(TEXT("original_component_registered_states"),
            BoolArrayJson(Session.OriginalComponentRegisteredStates));
        Data->SetArrayField(TEXT("original_simulate_physics_states"),
            BoolArrayJson(Session.OriginalSimulatePhysicsStates));
        Data->SetArrayField(TEXT("physics_object_counts_before_recreate"),
            IntArrayJson(Session.PhysicsObjectCountsBeforeRecreate));
        Data->SetArrayField(TEXT("physics_object_counts_after_recreate"),
            IntArrayJson(Session.PhysicsObjectCountsAfterRecreate));
        Data->SetBoolField(TEXT("physics_state_ready_before_begin_evaluate"),
            Session.bPhysicsStateReady && Session.PhysicsStateReadyFrame > Session.PhysicsStateRecreateFrame &&
            (!Session.bRecordingActive || Session.BeginEvaluateFrame > Session.PhysicsStateReadyFrame));
        Data->SetBoolField(TEXT("component_simulation_settings_restored"),
            Session.bComponentSimulationSettingsRestored);
        Data->SetBoolField(TEXT("component_registration_settings_restored"),
            Session.bComponentRegistrationSettingsRestored);
        Data->SetNumberField(TEXT("initial_strain_dispatch_frame"),
            static_cast<double>(Session.InitialStrainDispatchFrame));
        Data->SetBoolField(TEXT("recording_complete"), Session.bReadyToFinish);
        Data->SetBoolField(TEXT("timed_out"), Session.bTimedOut);
        Data->SetBoolField(TEXT("autonomous_cleanup_complete"), Session.bCleanupComplete);
        Data->SetNumberField(TEXT("initial_external_strain_dispatch_count"), Session.InitialExternalStrainDispatchCount);
        Data->SetNumberField(TEXT("initial_breaking_velocity_dispatch_count"), Session.InitialMotionDispatchCount);
        Data->SetNumberField(TEXT("initial_strain_dispatch_attempt_count"),
            Session.bInitialStrainDispatchAttempted ? 1 : 0);
        Data->SetBoolField(TEXT("breaking_velocity_deferred_dispatch_complete"),
            Session.InitialTriggerMode == TEXT("none") || Session.bDeferredBreakingVelocityDispatched);
        Data->SetBoolField(TEXT("breaking_velocity_armed_before_release"),
            Session.bBreakingVelocityArmedBeforeRelease);
        Data->SetBoolField(TEXT("post_release_impulse_dispatch_complete"),
            Session.InitialTriggerMode == TEXT("none") || Session.bPostReleaseImpulseDispatched);
        Data->SetNumberField(TEXT("breaking_velocity_actual_deferred_frame_delta"),
            Session.DeferredBreakingDispatchFrameDelta);
        Data->SetBoolField(TEXT("solver_release_observed_before_motion"),
            false);
        Data->SetBoolField(TEXT("solver_release_observed_after_velocity_arm"),
            Session.bReleaseObservedBeforeMotion);
        Data->SetNumberField(TEXT("release_observed_frame"),
            static_cast<double>(Session.ReleaseObservedFrame));
        Data->SetStringField(TEXT("solver_release_observation_source"),
            TEXT("read_locked PhysicsObject disabled-state transition"));
        Data->SetNumberField(TEXT("radial_impulse_dispatch_count"),
            Session.RadialImpulseDispatchCount);
        Data->SetNumberField(TEXT("uniform_velocity_field_dispatch_count"),
            Session.UniformVelocityFieldDispatchCount);
        Data->SetNumberField(TEXT("localized_impulse_dispatch_count"),
            Session.LocalizedImpulseDispatchCount);
        Data->SetNumberField(TEXT("released_dynamic_physics_object_count"),
            Session.ReleasedDynamicPhysicsObjectCount);
        Data->SetNumberField(TEXT("maximum_released_dynamic_displacement_cm"),
            Session.MaximumReleasedDynamicDisplacementCm);
        Data->SetBoolField(TEXT("released_dynamic_displacement_observed"),
            Session.ReleasedDynamicPhysicsObjectCount > 0 &&
            Session.MaximumReleasedDynamicDisplacementCm > 1.0);
        AppendNativeAnchorStabilityEvidence(Data, Session);
        Data->SetBoolField(TEXT("requires_followup_sample"), !Session.bReadyToFinish && !Session.bTimedOut);
        Data->SetStringField(TEXT("assertion_status"), Session.bReadyToFinish ? TEXT("PASS") : TEXT("BLOCKED"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (Operation != TEXT("finish"))
    {
        return CommandError(TEXT("record_chaos_cache operation must be begin, sample, or finish"));
    }
    AChaosCacheManager* Manager = Session.Manager.Get();
    UChaosCacheCollection* Collection = Session.Collection.Get();
    if (!Session.bReadyToFinish || Session.bTimedOut || !Manager || !Collection)
    {
        CleanupNativeRecordSession(Session, false);
        ActiveNativeRecordSession.Reset();
        return CommandError(TEXT("Recording did not reach the required 60 frames and 2 seconds before finish/timeout"));
    }
    if (!Session.bCleanupComplete)
    {
        CleanupNativeRecordSession(Session, true);
    }
    const bool bSaved = Session.bCacheSaved;
    const bool bCacheGate = Collection->GetMaxNumFrames() >= 60 && Collection->GetMaxDuration() >= 2.0f;
    const bool bObservedTransformVariation = Session.ObservedTransformHashes.Num() > 1;
    const bool bPhysicsStateReadinessGate =
        Session.PhysicsStateRecreateCount == Session.Components.Num() &&
        Session.DynamicCollectionResetCount == Session.Components.Num() &&
        Session.ComponentRegisterCount ==
            Algo::Count(Session.OriginalComponentRegisteredStates, false) &&
        Session.SimulatePhysicsEnableCount == Session.Components.Num() &&
        Session.PhysicsProxyReadyCount == Session.Components.Num() &&
        Session.bPhysicsStateReady &&
        Session.PhysicsStateRecreateFrame == Session.ManagerConfigureFrame &&
        Session.ManagerConfigureFrame < Session.PhysicsStateReadyFrame &&
        Session.PhysicsStateReadyFrame < Session.BeginEvaluateFrame;
    const bool bConfigurationFrameOrder = Session.bRecordingActive &&
        Session.EndEvaluateFrame < Session.ManagerConfigureFrame &&
        bPhysicsStateReadinessGate &&
        (Session.InitialTriggerMode == TEXT("none") ||
            Session.InitialStrainDispatchFrame > Session.BeginEvaluateFrame);
    TArray<TSharedPtr<FJsonValue>> SavedCacheTrackInspections;
    bool bSavedCacheTransformVariation = Collection->GetCaches().Num() > 0;
    const TArray<double> SavedCacheSampleTimes = {0.25, 0.5, 1.0};
    for (const UChaosCache* Cache : Collection->GetCaches())
    {
        const TSharedPtr<FJsonObject> Inspection =
            InspectChaosCacheTracks(Cache, SavedCacheSampleTimes);
        bool bCacheVaries = false;
        Inspection->TryGetBoolField(TEXT("sample_time_hash_variation"), bCacheVaries);
        bSavedCacheTransformVariation &= bCacheVaries;
        SavedCacheTrackInspections.Add(MakeShared<FJsonValueObject>(Inspection));
    }
    Session.bSavedCacheTransformVariation = bSavedCacheTransformVariation;
    const bool bCacheNewlyDirtied = !Session.bCachePackageDirtyBefore && Collection->GetOutermost()->IsDirty();
    TSharedPtr<FJsonObject> Data = Success(TEXT("record_chaos_cache"));
    Data->SetStringField(TEXT("operation"), Operation);
    Data->SetStringField(TEXT("session_id"), Session.SessionId);
    Data->SetNumberField(TEXT("captured_frame_count"), Session.CapturedFrameCount);
    Data->SetNumberField(TEXT("recording_elapsed_seconds"), Elapsed);
    Data->SetStringField(TEXT("configuration_phase"), Session.ConfigurationPhase);
    Data->SetStringField(TEXT("configuration_failure"), Session.ConfigurationFailure);
    Data->SetBoolField(TEXT("manager_configured"), Session.bManagerConfigured);
    Data->SetBoolField(TEXT("recording_active"), Session.bRecordingActive);
    Data->SetNumberField(TEXT("request_frame"), static_cast<double>(Session.RequestFrame));
    Data->SetNumberField(TEXT("end_evaluate_frame"), static_cast<double>(Session.EndEvaluateFrame));
    Data->SetNumberField(TEXT("manager_configure_frame"),
        static_cast<double>(Session.ManagerConfigureFrame));
    Data->SetNumberField(TEXT("physics_state_recreate_frame"),
        static_cast<double>(Session.PhysicsStateRecreateFrame));
    Data->SetNumberField(TEXT("physics_state_ready_frame"),
        static_cast<double>(Session.PhysicsStateReadyFrame));
    Data->SetNumberField(TEXT("begin_evaluate_frame"),
        static_cast<double>(Session.BeginEvaluateFrame));
    Data->SetNumberField(TEXT("physics_state_recreate_count"),
        Session.PhysicsStateRecreateCount);
    Data->SetNumberField(TEXT("dynamic_collection_reset_count"),
        Session.DynamicCollectionResetCount);
    Data->SetNumberField(TEXT("component_register_count"),
        Session.ComponentRegisterCount);
    Data->SetNumberField(TEXT("component_unregister_on_restore_count"),
        Session.ComponentUnregisterOnRestoreCount);
    Data->SetNumberField(TEXT("simulate_physics_enable_count"),
        Session.SimulatePhysicsEnableCount);
    Data->SetNumberField(TEXT("simulate_physics_restore_count"),
        Session.SimulatePhysicsRestoreCount);
    Data->SetNumberField(TEXT("object_type_dynamic_configure_count"),
        Session.ObjectTypeDynamicConfigureCount);
    Data->SetNumberField(TEXT("object_type_restore_count"),
        Session.ObjectTypeRestoreCount);
    Data->SetNumberField(TEXT("physics_state_destroy_on_restore_count"),
        Session.PhysicsStateDestroyOnRestoreCount);
    Data->SetNumberField(TEXT("physics_proxy_ready_count"),
        Session.PhysicsProxyReadyCount);
    Data->SetArrayField(TEXT("original_component_registered_states"),
        BoolArrayJson(Session.OriginalComponentRegisteredStates));
    Data->SetArrayField(TEXT("original_simulate_physics_states"),
        BoolArrayJson(Session.OriginalSimulatePhysicsStates));
    Data->SetArrayField(TEXT("physics_object_counts_before_recreate"),
        IntArrayJson(Session.PhysicsObjectCountsBeforeRecreate));
    Data->SetArrayField(TEXT("physics_object_counts_after_recreate"),
        IntArrayJson(Session.PhysicsObjectCountsAfterRecreate));
    Data->SetBoolField(TEXT("physics_state_ready_before_begin_evaluate"),
        bPhysicsStateReadinessGate);
    Data->SetBoolField(TEXT("component_simulation_settings_restored"),
        Session.bComponentSimulationSettingsRestored);
    Data->SetBoolField(TEXT("component_registration_settings_restored"),
        Session.bComponentRegistrationSettingsRestored);
    Data->SetNumberField(TEXT("initial_strain_dispatch_frame"),
        static_cast<double>(Session.InitialStrainDispatchFrame));
    Data->SetBoolField(TEXT("configuration_frame_order_gate"), bConfigurationFrameOrder);
    Data->SetNumberField(TEXT("cache_frame_count"), Collection->GetMaxNumFrames());
    Data->SetNumberField(TEXT("cache_duration_seconds"), Collection->GetMaxDuration());
    Data->SetBoolField(TEXT("sixty_frame_two_second_gate"), bCacheGate);
    Data->SetNumberField(TEXT("unique_observed_transform_hash_count"), Session.ObservedTransformHashes.Num());
    Data->SetBoolField(TEXT("observed_transform_variation"), bObservedTransformVariation);
    Data->SetArrayField(TEXT("saved_cache_track_inspections"), SavedCacheTrackInspections);
    Data->SetBoolField(TEXT("saved_cache_transform_variation"),
        Session.bSavedCacheTransformVariation);
    Data->SetBoolField(TEXT("require_saved_cache_transform_variation"),
        Session.bRequireSavedCacheTransformVariation);
    Data->SetNumberField(TEXT("initial_external_strain_dispatch_count"), Session.InitialExternalStrainDispatchCount);
    Data->SetNumberField(TEXT("initial_strain_dispatch_attempt_count"),
        Session.bInitialStrainDispatchAttempted ? 1 : 0);
    Data->SetNumberField(TEXT("initial_external_strain_and_velocity_dispatch_count"), Session.InitialMotionDispatchCount);
    Data->SetNumberField(TEXT("initial_breaking_velocity_dispatch_count"), Session.InitialMotionDispatchCount);
    Data->SetBoolField(TEXT("breaking_velocity_deferred_dispatch_complete"),
        Session.InitialTriggerMode == TEXT("none") || Session.bDeferredBreakingVelocityDispatched);
    Data->SetBoolField(TEXT("breaking_velocity_armed_before_release"),
        Session.bBreakingVelocityArmedBeforeRelease);
    Data->SetBoolField(TEXT("post_release_impulse_dispatch_complete"),
        Session.InitialTriggerMode == TEXT("none") || Session.bPostReleaseImpulseDispatched);
    Data->SetNumberField(TEXT("breaking_velocity_requested_deferred_frame_delta"), 0);
    Data->SetNumberField(TEXT("breaking_velocity_actual_deferred_frame_delta"),
        Session.DeferredBreakingDispatchFrameDelta);
    Data->SetBoolField(TEXT("solver_release_observed_before_motion"),
        false);
    Data->SetBoolField(TEXT("solver_release_observed_after_velocity_arm"),
        Session.bReleaseObservedBeforeMotion);
    Data->SetNumberField(TEXT("release_observed_frame"),
        static_cast<double>(Session.ReleaseObservedFrame));
    Data->SetStringField(TEXT("solver_release_observation_source"),
        TEXT("read_locked PhysicsObject disabled-state transition"));
    Data->SetNumberField(TEXT("radial_impulse_dispatch_count"),
        Session.RadialImpulseDispatchCount);
    Data->SetNumberField(TEXT("uniform_velocity_field_dispatch_count"),
        Session.UniformVelocityFieldDispatchCount);
    Data->SetNumberField(TEXT("localized_impulse_dispatch_count"),
        Session.LocalizedImpulseDispatchCount);
    Data->SetNumberField(TEXT("released_dynamic_physics_object_count"),
        Session.ReleasedDynamicPhysicsObjectCount);
    Data->SetNumberField(TEXT("maximum_released_dynamic_displacement_cm"),
        Session.MaximumReleasedDynamicDisplacementCm);
    Data->SetBoolField(TEXT("released_dynamic_displacement_observed"),
        Session.ReleasedDynamicPhysicsObjectCount > 0 &&
        Session.MaximumReleasedDynamicDisplacementCm > 1.0);
    AppendNativeAnchorStabilityEvidence(Data, Session);
    Data->SetNumberField(TEXT("direct_velocity_apply_count"), 0);
    Data->SetNumberField(TEXT("retry_velocity_dispatch_count"), 0);
    Data->SetStringField(TEXT("initial_trigger_mode"), Session.InitialTriggerMode);
    Data->SetBoolField(TEXT("record_observation_only"), Session.InitialTriggerMode == TEXT("none"));
    Data->SetBoolField(TEXT("per_frame_motion_manipulation"), false);
    Data->SetBoolField(TEXT("unique_cache_name_gate"), Session.bUniqueCacheNames);
    Data->SetBoolField(TEXT("cache_package_dirty_before"), Session.bCachePackageDirtyBefore);
    Data->SetBoolField(TEXT("cache_package_dirty_after_save"), Collection->GetOutermost()->IsDirty());
    Data->SetBoolField(TEXT("cache_source_newly_dirtied"), bCacheNewlyDirtied);
    Data->SetBoolField(TEXT("settings_restored"), Session.bSettingsRestored);
    Data->SetBoolField(TEXT("autonomous_cleanup_complete"), Session.bCleanupComplete);
    Data->SetBoolField(TEXT("saved"), bSaved);
    Data->SetStringField(TEXT("assertion_status"), bSaved && bCacheGate && bObservedTransformVariation &&
        bConfigurationFrameOrder &&
        (!Session.bRequireSavedCacheTransformVariation || Session.bSavedCacheTransformVariation) &&
        Session.bComponentSimulationSettingsRestored &&
        Session.bComponentRegistrationSettingsRestored &&
        Session.SimulatePhysicsRestoreCount == Session.SimulatePhysicsEnableCount &&
        Session.ObjectTypeRestoreCount == Session.ObjectTypeDynamicConfigureCount &&
        Session.ComponentUnregisterOnRestoreCount == Session.ComponentRegisterCount &&
        Session.PhysicsStateDestroyOnRestoreCount ==
            Algo::Count(Session.OriginalSimulatePhysicsStates, false) &&
        (Session.InitialTriggerMode == TEXT("none") ||
            (Session.InitialExternalStrainDispatchCount == Session.Components.Num() &&
                Session.InitialMotionDispatchCount == Session.Components.Num() &&
                Session.RadialImpulseDispatchCount == Session.Components.Num() &&
                Session.UniformVelocityFieldDispatchCount == Session.Components.Num() &&
                Session.LocalizedImpulseDispatchCount == Session.Components.Num() &&
                Session.bBreakingVelocityArmedBeforeRelease &&
                Session.bReleaseObservedBeforeMotion &&
                Session.bDeferredBreakingVelocityDispatched &&
                Session.bPostReleaseImpulseDispatched &&
                Session.ReleasedDynamicPhysicsObjectCount > 0 &&
                Session.MaximumReleasedDynamicDisplacementCm >= 10.0 &&
                Session.InitialAnchorSelectionPopulationCount > 0 &&
                Session.AnchorSelectionPercent >= 10.0 &&
                Session.AnchorSelectionPercent <= 30.0 &&
                Session.InitialAnchorSelectedTransformCount > 0 &&
                Session.InitialAnchorSelectedPhysicsBodyCount > 0 &&
                Session.MatchedAnchorSelectedPhysicsBodyCount ==
                    Session.InitialAnchorSelectedPhysicsBodyCount &&
                Session.MatchedAnchorSelectedTransformCount ==
                    Session.InitialAnchorSelectedTransformCount &&
                Session.MaximumAnchoredDisplacementCm <= 0.5 &&
                Session.MaximumAnchoredRotationDegrees <= 0.5 &&
                (Session.bPreserveInitializationFieldsDuringRecord &&
                    !Session.bUseNativeAfterReleaseAnchorFallback
                    ? Session.NativeAnchorByBoxDispatchCount == 0 &&
                        Session.NativeAnchorByIndexDispatchCount == 0 &&
                        Session.NativeAnchorKinematicFieldDispatchCount == 0 &&
                        Session.NativeAnchorForceKinematicObjectCount == 0 &&
                        Session.AuthoredInitializationFieldRedispatchCount ==
                            Session.Components.Num() &&
                        Session.InitializationFieldSuppressCount == 0 &&
                        Session.InitializationFieldRestoreCount == 0
                    : Session.NativeAnchorByBoxDispatchCount == Session.Components.Num() &&
                        Session.NativeAnchorByIndexDispatchCount ==
                            Session.InitialAnchorSelectedTransformCount &&
                        Session.NativeAnchorKinematicFieldDispatchCount ==
                            Session.Components.Num() &&
                        Session.NativeAnchorForceKinematicObjectCount >=
                            Session.InitialAnchorSelectedTransformCount &&
                        Session.NativeAnchorRemovalCount ==
                            Session.NativeAnchorByBoxDispatchCount &&
                        Session.InitializationFieldSuppressCount ==
                            Session.Components.Num() &&
                        Session.InitializationFieldRestoreCount ==
                            Session.InitializationFieldSuppressCount) &&
                Session.DeferredBreakingDispatchFrameDelta == 0)) &&
        Session.bSettingsRestored && Session.bUniqueCacheNames && !bCacheNewlyDirtied ? TEXT("PASS") : TEXT("FAIL"));
    ActiveNativeRecordSession.Reset();
    return FLegacy::CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> ValidateChaosCacheNative(const TSharedPtr<FJsonObject>& Params)
{
    const FString Operation = GetString(Params, TEXT("operation"), TEXT("begin"));
    if (Operation == TEXT("abort"))
    {
        if (ActiveNativeValidationSession)
        {
            auto Previous=ActiveNativeValidationSession;CleanupNativeValidationSession(*Previous);Work->R.Data->SetObjectField(TEXT("previous_session_execution"),Previous->Evidence?Previous->Evidence->SessionSnapshot():MakeShared<FJsonObject>());ActiveNativeValidationSession.Reset();if(!Previous->bCleanupComplete) { return CommandError(TEXT("Previous validation cleanup is incomplete; new mutation was not started")); }
        }
        TSharedPtr<FJsonObject> Data = Success(TEXT("validate_chaos_cache"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetBoolField(TEXT("autonomous_cleanup_complete"), true);
        Data->SetStringField(TEXT("assertion_status"), TEXT("PASS"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    UWorld* World = ResolveEditorWorld();
    if (!World)
    {
        return CommandError(TEXT("validate_chaos_cache requires an editor or PIE world"));
    }
    if (Operation == TEXT("begin"))
    {
        if (ActiveNativeValidationSession)
        {
            auto Previous=ActiveNativeValidationSession;CleanupNativeValidationSession(*Previous);Work->R.Data->SetObjectField(TEXT("previous_session_execution"),Previous->Evidence?Previous->Evidence->SessionSnapshot():MakeShared<FJsonObject>());ActiveNativeValidationSession.Reset();if(!Previous->bCleanupComplete) { return CommandError(TEXT("Previous validation cleanup is incomplete; new mutation was not started")); }
        }
        if(!PreflightValidation(Params)) { return CommandError(Work->R.Error); }
        AChaosCacheManager* Manager = FindActor<AChaosCacheManager>(World, GetString(Params, TEXT("cache_manager_actor_name")));
        UChaosCacheCollection* Collection=Work->Load<UChaosCacheCollection>(GetString(Params,TEXT("cache_collection_path")),TEXT("load_validation_cache_collection"));
        if (!Manager || !Collection)
        {
            return CommandError(TEXT("validate begin requires an existing Cache Manager and Cache Collection"));
        }
        TArray<UGeometryCollectionComponent*> Components = ResolveGeometryComponents(
            World, GetStringArray(Params, TEXT("geometry_collection_actor_names")));
        if (Components.IsEmpty())
        {
            const TArray<FObservedComponent> ObservedCopy=Manager->GetObservedComponents();for (const FObservedComponent& Observed : ObservedCopy)
            {
                if (UGeometryCollectionComponent* Component = Cast<UGeometryCollectionComponent>(Observed.GetComponent(Manager)))
                    Components.Add(Component);
            }
        }
        if (Components.IsEmpty()||!Guard())
        {
            return CommandError(TEXT("No observed Geometry Collection components were resolved"));
        }
        if(TargetOverlap(ActiveNativeRecordSession,Manager,Components,GetString(Params,TEXT("cache_collection_path")))) { return CommandError(TEXT("Validation conflicts with an unfinished recording manager/component/cache target")); }
        const FString SequencePath = GetString(Params, TEXT("sequence_path"));
        ULevelSequence* Sequence=SequencePath.IsEmpty()?nullptr:Work->Load<ULevelSequence>(SequencePath,TEXT("load_validation_sequence"));
        if (!Sequence)
        {
            return CommandError(TEXT("validate_chaos_cache is sequence-only and requires a valid sequence_path"));
        }
        int32 SequenceChaosTrackCount = 0;
        int32 MatchingSequenceSectionCount = 0;
        const bool bSequenceBindingValidated = Sequence && ValidateManagerSequenceBinding(
            Sequence, Manager, Collection, World, SequenceChaosTrackCount, MatchingSequenceSectionCount);
        if (!bSequenceBindingValidated)
        {
            return CommandError(TEXT("sequence_path must bind the requested Cache Manager through exactly one Chaos Cache track with a section using the same Cache Collection"));
        }
        UChaosCacheCollection* OriginalManagerCacheCollection = Manager->CacheCollection;
        const ECacheMode OriginalManagerCacheMode = Manager->CacheMode;
        const EStartMode OriginalManagerStartMode = Manager->StartMode;
        const TArray<FObservedComponent> OriginalObservedComponents = Manager->GetObservedComponents();
        ActiveNativeValidationSession = MakeShared<FNativeCacheValidationSession>();
        FNativeCacheValidationSession& Session = *ActiveNativeValidationSession;
        Session.Evidence=CallOwner;Session.MonotonicStart=FPlatformTime::Seconds();Work->OperationComplete=false;
        Session.SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);Work->SessionId=Session.SessionId;
        Session.World = World;
        Session.Manager = Manager;
        Session.Collection = Collection;
        Session.OriginalManagerCacheCollection = OriginalManagerCacheCollection;
        Session.OriginalManagerCacheMode = OriginalManagerCacheMode;
        Session.OriginalManagerStartMode = OriginalManagerStartMode;
        Session.OriginalObservedComponents = OriginalObservedComponents;
        Session.bHasOriginalManagerState = true;
        if(!CaptureOriginalFlags(Manager,Components,Session.OriginalComponentFlags)) { CleanupNativeValidationSession(Session);return CommandError(Work->R.Error); }
        if(!CaptureTargets(Manager,Collection,Components)) { CleanupNativeValidationSession(Session);return CommandError(Work->R.Error); }Session.bManagerTouched=true;if(!Step(TEXT("EndEvaluate_before_new_session"),[&]{Manager->EndEvaluate();})) { CleanupNativeValidationSession(Session);return CommandError(Work->R.Error); }
        Write(TEXT("configure_validation_manager"));Manager->CacheCollection = Collection;
        // Sequencer's Chaos Cache execution token drives SetStartTime while
        // CacheMode is None. BeginEvaluate in that same manual-evaluation mode
        // initializes adapters without setting bIsSimulating, whose guard would
        // otherwise reject every sequencer-driven time update.
        Manager->CacheMode = ECacheMode::None;
        Manager->StartMode = EStartMode::Timed;
        if(!Step(TEXT("BeginEvaluate_validation"),[&]{Manager->BeginEvaluate();})) { CleanupNativeValidationSession(Session);return CommandError(Work->R.Error); }

        Session.bSequenceMode = true;
        Session.bSequenceBindingValidated = bSequenceBindingValidated;
        Session.BeginFrame = GFrameCounter;
        Session.BeginWorldTime = World->GetTimeSeconds();
        Session.TimeoutSeconds = FMath::Max(2.0, GetNumber(Params, TEXT("timeout_seconds"), 10.0));
        for (UGeometryCollectionComponent* Component : Components)
        {
            Session.Components.Add(Component);
        }
        const TArray<double> ScrubNumbers = GetNumberArray(
            Params, TEXT("scrub_frames"), {0.0, 15.0, 30.0, 45.0, 60.0});
        TArray<int32> ForwardFrames;
        for (double Number : ScrubNumbers) ForwardFrames.Add(FMath::Max(0, CastI32(Number,TEXT("scrub_frame"))));
        Session.RequestedSampleTimes = GetNumberArray(Params, TEXT("sample_times"), {0.25, 0.5, 1.0});
        const int32 SampleFrameRate = FMath::Max(1, CastI32(GetNumber(Params, TEXT("sample_frame_rate"), 30),TEXT("sample_frame_rate")));
        for (double SampleTime : Session.RequestedSampleTimes)
        {
            const int32 SampleFrame = FMath::Max(0, RoundI32(SampleTime*SampleFrameRate,TEXT("sample_time_frame")));
            Session.SampleTimeFrames.Add(SampleFrame);
            ForwardFrames.AddUnique(SampleFrame);
        }
        ForwardFrames.Sort();
        TArray<int32> ReverseFrames = ForwardFrames;
        Algo::Reverse(ReverseFrames);
        TArray<int32> RandomFrames = ForwardFrames;
        FRandomStream Random(1337);
        for (int32 Index = RandomFrames.Num() - 1; Index > 0; --Index)
            RandomFrames.Swap(Index, Random.RandRange(0, Index));
        const int32 RepeatCount = FMath::Clamp(CastI32(GetNumber(Params, TEXT("repeat_count"), 2),TEXT("repeat_count")), 2, 5);
        if (!GetBool(Params, TEXT("validate_forward_reverse"), true))
        {
            CleanupNativeValidationSession(Session);
            ActiveNativeValidationSession.Reset();
            return CommandError(TEXT("validate_forward_reverse must remain true for deterministic cinematic acceptance"));
        }
        auto AppendSchedule = [&Session](const TArray<int32>& Frames, const FString& PassName)
        {
            for (int32 Frame : Frames)
            {
                Session.ScheduledFrames.Add(Frame);
                Session.ScheduledPasses.Add(PassName);
            }
        };
        for (int32 Repeat = 0; Repeat < RepeatCount; ++Repeat)
        {
            AppendSchedule(ForwardFrames, FString::Printf(TEXT("forward_%d"), Repeat));
            AppendSchedule(ReverseFrames, FString::Printf(TEXT("reverse_%d"), Repeat));
            AppendSchedule(RandomFrames, FString::Printf(TEXT("random_%d"), Repeat));
        }
        if(Session.ScheduledFrames.Num()>8192) { CleanupNativeValidationSession(Session);ActiveNativeValidationSession.Reset();return CommandError(TEXT("Validation schedule budget8192 exceeded")); }
        Session.bForwardReverseRandomScheduled = !ForwardFrames.IsEmpty() && !ReverseFrames.IsEmpty() &&
            !RandomFrames.IsEmpty() && RepeatCount >= 2;
        if (Sequence)
        {
            FMovieSceneSequencePlaybackSettings PlaybackSettings;
            ALevelSequenceActor* SequenceActor = nullptr;
            ULevelSequencePlayer* Player=nullptr;if(!Step(TEXT("create_validation_sequence_player"),[&]{Player=ULevelSequencePlayer::CreateLevelSequencePlayer(World,Sequence,PlaybackSettings,SequenceActor);Session.SequencePlayer=Player;Session.SequenceActor=SequenceActor;if(Player||SequenceActor) { Work->R.bChanged=true;++Work->StructuralChangeSerial; }})) { CleanupNativeValidationSession(Session);return CommandError(Work->R.Error); }
            if(SequenceActor&&!Work->Track(SequenceActor,TEXT("owned_validation_sequence_actor"),SequenceActor->GetOuter(),true)) { CleanupNativeValidationSession(Session);return CommandError(Work->R.Error); }
            if (Player&&Work->Track(Player,TEXT("owned_validation_sequence_player"),SequenceActor,true))
            {
                Session.SequencePlayer = Player;
                Session.SequenceActor = SequenceActor;
                Session.bUsedSequencePlayer = true;
            }
        }
        if (Session.bSequenceMode && !Session.bUsedSequencePlayer)
        {
            CleanupNativeValidationSession(Session);
            ActiveNativeValidationSession.Reset();
            return CommandError(TEXT("Validated sequence mode could not create a LevelSequencePlayer; manager SetStartTime fallback is forbidden"));
        }
        DispatchNativeValidationFrame(Session);if(!Guard()||!Work->R.Error.IsEmpty()) { CleanupNativeValidationSession(Session);return CommandError(Work->R.Error); }
        Session.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            return TickNativeCacheValidation(0.0f);
        }));
        TSharedPtr<FJsonObject> Data = Success(TEXT("validate_chaos_cache"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("session_id"), Session.SessionId);
        Data->SetNumberField(TEXT("observed_component_count"), Components.Num());
        Data->SetNumberField(TEXT("playback_begin_frame"), static_cast<double>(Session.BeginFrame));
        Data->SetNumberField(TEXT("scheduled_async_sample_count"), Session.ScheduledFrames.Num());
        Data->SetNumberField(TEXT("required_settle_frame_delta"), Session.RequiredSettleFrameDelta);
        Data->SetNumberField(TEXT("endpoint_frame"), Session.EndpointFrame);
        Data->SetNumberField(TEXT("endpoint_prime_frame"), Session.EndpointPrimeFrame);
        Data->SetBoolField(TEXT("endpoint_actual_evaluation_scheduled"),
            ForwardFrames.Contains(Session.EndpointFrame));
        TArray<TSharedPtr<FJsonValue>> RequestedSampleTimeValues;
        TArray<TSharedPtr<FJsonValue>> SampleTimeFrameValues;
        for (double SampleTime : Session.RequestedSampleTimes)
            RequestedSampleTimeValues.Add(MakeShared<FJsonValueNumber>(SampleTime));
        for (int32 SampleFrame : Session.SampleTimeFrames)
            SampleTimeFrameValues.Add(MakeShared<FJsonValueNumber>(SampleFrame));
        Data->SetArrayField(TEXT("requested_sample_times_seconds"), RequestedSampleTimeValues);
        Data->SetArrayField(TEXT("sample_time_frames"), SampleTimeFrameValues);
        Data->SetBoolField(TEXT("sequence_player_created"), Session.bUsedSequencePlayer);
        Data->SetBoolField(TEXT("sequence_binding_validated"), Session.bSequenceBindingValidated);
        Data->SetNumberField(TEXT("sequence_chaos_track_count"), SequenceChaosTrackCount);
        Data->SetNumberField(TEXT("matching_cache_section_count"), MatchingSequenceSectionCount);
        TArray<TSharedPtr<FJsonValue>> CacheTrackInspections;
        for (const UChaosCache* Cache : Collection->GetCaches())
        {
            CacheTrackInspections.Add(MakeShared<FJsonValueObject>(
                InspectChaosCacheTracks(Cache, Session.RequestedSampleTimes)));
        }
        Data->SetArrayField(TEXT("cache_track_inspections"), CacheTrackInspections);
        Data->SetBoolField(TEXT("manager_set_start_time_fallback_used"), !Session.bSequenceMode);
        Data->SetBoolField(TEXT("forward_reverse_random_scheduled"), Session.bForwardReverseRandomScheduled);
        Data->SetBoolField(TEXT("tool_forced_simulation_state"), false);
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (!ActiveNativeValidationSession || ActiveNativeValidationSession->World.Get() != World)
    {
        return CommandError(TEXT("No active native cache validation session"));
    }
    FNativeCacheValidationSession& Session = *ActiveNativeValidationSession;
    AChaosCacheManager* Manager = Session.Manager.Get();
    UChaosCacheCollection* Collection = Session.Collection.Get();
    const bool bPhysicsTickObserved = GFrameCounter > Session.BeginFrame;
    const double Elapsed = World->GetTimeSeconds() - Session.BeginWorldTime;
    if (Operation == TEXT("sample"))
    {
        const bool bEvaluationTickObserved = Session.CapturedHashes.Num() > 0;
        const bool bAllSamplesCaptured = Session.bReadyToFinish;
        TSharedPtr<FJsonObject> Data = Success(TEXT("validate_chaos_cache"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("session_id"), Session.SessionId);
        Data->SetBoolField(TEXT("physics_tick_observed"), bPhysicsTickObserved);
        Data->SetBoolField(TEXT("evaluation_tick_observed"), bEvaluationTickObserved);
        Data->SetNumberField(TEXT("captured_async_sample_count"), Session.CapturedHashes.Num());
        Data->SetNumberField(TEXT("scheduled_async_sample_count"), Session.ScheduledFrames.Num());
        Data->SetBoolField(TEXT("all_async_samples_captured"), bAllSamplesCaptured);
        Data->SetBoolField(TEXT("timed_out"), Session.bTimedOut);
        Data->SetBoolField(TEXT("autonomous_cleanup_complete"), Session.bCleanupComplete);
        Data->SetBoolField(TEXT("tool_forced_simulation_state"), false);
        Data->SetBoolField(TEXT("requires_followup_sample"), !bAllSamplesCaptured);
        Data->SetStringField(TEXT("assertion_status"), bAllSamplesCaptured ? TEXT("PASS") : TEXT("BLOCKED"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (Operation != TEXT("finish") || !Manager || !Collection)
    {
        return CommandError(TEXT("validate_chaos_cache operation must be begin, sample, or finish"));
    }
    if (!Session.bReadyToFinish || Session.bTimedOut || Session.ScheduleIndex < Session.ScheduledFrames.Num() ||
        Session.CapturedHashes.Num() != Session.ScheduledFrames.Num() ||
        Session.CapturedSolverDiagnosticHashes.Num() != Session.ScheduledFrames.Num())
    {
        CleanupNativeValidationSession(Session);
        ActiveNativeValidationSession.Reset();
        return CommandError(TEXT("Cache validation finish was requested before autonomous sampling completed or after timeout; session was aborted and cleaned up"));
    }
    TMap<int32, FString> ExpectedByFrame;
    TMap<int32, FString> ExpectedSolverByFrame;
    bool bHashesMatch = true;
    bool bSolverHashesMatchByFrame = true;
    bool bPlayerFramesMatchRequested = true;
    int32 EndpointActualEvaluationCount = 0;
    TSet<FString> UniqueSolverDiagnosticHashes;
    TArray<TSharedPtr<FJsonValue>> ScrubSamples;
    for (int32 Index = 0; Index < Session.ScheduledFrames.Num(); ++Index)
    {
        const int32 Frame = Session.ScheduledFrames[Index];
        const FString& Hash = Session.CapturedHashes[Index];
        const FString& SolverHash = Session.CapturedSolverDiagnosticHashes[Index];
        const int32 CapturedPlayerFrame = Session.CapturedPlayerFrames.IsValidIndex(Index)
            ? Session.CapturedPlayerFrames[Index] : INDEX_NONE;
        bPlayerFramesMatchRequested &= CapturedPlayerFrame == Frame;
        EndpointActualEvaluationCount +=
            Frame == Session.EndpointFrame && CapturedPlayerFrame == Session.EndpointFrame ? 1 : 0;
        if (const FString* Existing = ExpectedByFrame.Find(Frame)) bHashesMatch &= *Existing == Hash;
        else ExpectedByFrame.Add(Frame, Hash);
        if (const FString* Existing = ExpectedSolverByFrame.Find(Frame))
            bSolverHashesMatchByFrame &= *Existing == SolverHash;
        else
            ExpectedSolverByFrame.Add(Frame, SolverHash);
        UniqueSolverDiagnosticHashes.Add(SolverHash);
        TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
        Sample->SetStringField(TEXT("pass"), Session.ScheduledPasses[Index]);
        Sample->SetNumberField(TEXT("frame"), Frame);
        Sample->SetStringField(TEXT("transform_sha256"), Hash);
        Sample->SetStringField(TEXT("visible_playback_sha256"), Hash);
        Sample->SetStringField(TEXT("live_solver_diagnostic_sha256"), SolverHash);
        Sample->SetNumberField(TEXT("manager_start_time_seconds"),
            Session.CapturedManagerStartTimes.IsValidIndex(Index)
                ? Session.CapturedManagerStartTimes[Index] : -1.0);
        Sample->SetNumberField(TEXT("sequence_player_frame"),
            Session.CapturedPlayerFrames.IsValidIndex(Index)
                ? Session.CapturedPlayerFrames[Index] : INDEX_NONE);
        ScrubSamples.Add(MakeShared<FJsonValueObject>(Sample));
    }
    TSet<FString> UniqueHashes;
    for (const TPair<int32, FString>& Pair : ExpectedByFrame) UniqueHashes.Add(Pair.Value);
    const bool bHashesVaryAcrossTimes = UniqueHashes.Num() > 1;
    const bool bSolverDiagnosticStable = UniqueSolverDiagnosticHashes.Num() == 1;
    const bool bCacheGate = Collection->GetMaxNumFrames() >= 60 && Collection->GetMaxDuration() >= 2.0f;
    const bool bSequenceSectionEvaluation = Session.bUsedSequencePlayer;
    if (!Session.bCleanupComplete) CleanupNativeValidationSession(Session);
    TSharedPtr<FJsonObject> Data = Success(TEXT("validate_chaos_cache"));
    Data->SetStringField(TEXT("operation"), Operation);
    Data->SetStringField(TEXT("session_id"), Session.SessionId);
    Data->SetBoolField(TEXT("physics_tick_observed"), bPhysicsTickObserved);
    Data->SetNumberField(TEXT("cache_frame_count"), Collection->GetMaxNumFrames());
    Data->SetNumberField(TEXT("cache_duration_seconds"), Collection->GetMaxDuration());
    Data->SetBoolField(TEXT("sixty_frame_two_second_gate"), bCacheGate);
    Data->SetArrayField(TEXT("scrub_samples"), ScrubSamples);
    TArray<TSharedPtr<FJsonValue>> RequestedSampleTimeValues;
    TArray<TSharedPtr<FJsonValue>> SampleTimeFrameValues;
    for (double SampleTime : Session.RequestedSampleTimes)
        RequestedSampleTimeValues.Add(MakeShared<FJsonValueNumber>(SampleTime));
    for (int32 SampleFrame : Session.SampleTimeFrames)
        SampleTimeFrameValues.Add(MakeShared<FJsonValueNumber>(SampleFrame));
    Data->SetArrayField(TEXT("requested_sample_times_seconds"), RequestedSampleTimeValues);
    Data->SetArrayField(TEXT("sample_time_frames"), SampleTimeFrameValues);
    Data->SetBoolField(TEXT("playback_hashes_match"), bHashesMatch);
    Data->SetBoolField(TEXT("playback_hashes_vary_across_times"), bHashesVaryAcrossTimes);
    Data->SetStringField(TEXT("playback_hash_scope"),
        TEXT("component transform + manager/cache binding + render-facing local rest transforms"));
    Data->SetBoolField(TEXT("live_solver_diagnostic_hashes_match_by_frame"),
        bSolverHashesMatchByFrame);
    Data->SetBoolField(TEXT("live_solver_diagnostic_repeatable_by_frame"),
        bSolverHashesMatchByFrame);
    Data->SetNumberField(TEXT("unique_live_solver_diagnostic_hash_count"),
        UniqueSolverDiagnosticHashes.Num());
    Data->SetBoolField(TEXT("live_solver_diagnostic_constant_across_scrub_times"),
        bSolverDiagnosticStable);
    Data->SetBoolField(TEXT("forward_reverse_random_validated"), Session.bForwardReverseRandomScheduled);
    Data->SetBoolField(TEXT("sequence_section_evaluation_used"), bSequenceSectionEvaluation);
    Data->SetBoolField(TEXT("sequence_player_frames_match_requested"), bPlayerFramesMatchRequested);
    Data->SetNumberField(TEXT("endpoint_actual_evaluation_count"), EndpointActualEvaluationCount);
    Data->SetNumberField(TEXT("endpoint_frame"), Session.EndpointFrame);
    Data->SetBoolField(TEXT("sequence_binding_validated"), Session.bSequenceBindingValidated);
    Data->SetNumberField(TEXT("live_simulation_suppressed_count"), Session.MinimumSuppressedCount);
    Data->SetBoolField(TEXT("tool_forced_simulation_state"), false);
    const bool bPass = bPhysicsTickObserved && bCacheGate && bHashesMatch && bHashesVaryAcrossTimes &&
        bSolverHashesMatchByFrame && bPlayerFramesMatchRequested && EndpointActualEvaluationCount > 0 &&
        bSequenceSectionEvaluation && Session.bSequenceBindingValidated && Session.bForwardReverseRandomScheduled &&
        Session.MinimumSuppressedCount == Session.Components.Num();
    Data->SetStringField(TEXT("suppression_observation_source"),
        TEXT("authored ObjectType Kinematic + SimulatePhysics false + PhysicsObject state (all kinematic or disabled)"));
    Data->SetBoolField(TEXT("autonomous_cleanup_complete"), Session.bCleanupComplete);
    Data->SetStringField(TEXT("assertion_status"), bPass ? TEXT("PASS") : TEXT("FAIL"));
    Data->SetStringField(TEXT("learning_verdict"), bPass ? TEXT("PASS") : TEXT("BLOCKED"));
    ActiveNativeValidationSession.Reset();
    return FLegacy::CreateSuccessResponse(Data);
}
TSharedPtr<FJsonObject> LastRecordExecution,LastValidationExecution;
void ObserveCumulative(FWork& W)
{
    if(!W.Guard()) { return; }for(auto& T:W.Tracked) { auto After=W.Snapshot(T.Weak.Get());if(!W.Guard()) { return; }W.R.bChanged|=!Json(T.Before).Equals(Json(After),ESearchCase::CaseSensitive); }
}
bool TickNativeCacheRecord(float Delta)
{
    if(!IsInGameThread()) { return false; }auto Hold=ActiveNativeRecordSession;if(!Hold) { return false; }if(Running) { return !ShuttingDown; }TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,Hold->Evidence.Get());if(!Work) { return false; }
    bool Keep=false;if(Work->Guard()) { Keep=TickNativeCacheRecordBody(Delta); }if(!Work->R.Error.IsEmpty()||!Work->Guard()) { Hold->ConfigurationFailure=Work->R.Error;Hold->ConfigurationPhase=TEXT("failed");CleanupNativeRecordSession(*Hold,false);Keep=false; }
    if(Hold->bTimedOut) { Work->Fail(TEXT("record_timeout"),Hold->ConfigurationFailure.IsEmpty()?TEXT("Record session timed out or lost its world"):Hold->ConfigurationFailure);Keep=false; }if(!Keep&&!Hold->bCleanupAttempted) { CleanupNativeRecordSession(*Hold,false); }Work->OperationComplete=!Keep;Work->R.Data->SetNumberField(TEXT("last_callback_frame"),static_cast<double>(GFrameCounter));Work->R.Data->SetStringField(TEXT("phase"),Keep?Hold->ConfigurationPhase:(Work->R.Error.IsEmpty()?TEXT("terminal_complete"):TEXT("terminal_failed")));Work->R.Data->SetBoolField(TEXT("timed_out"),Hold->bTimedOut);Work->R.Data->SetBoolField(TEXT("wall_timeout"),FPlatformTime::Seconds()-Hold->MonotonicStart>=Hold->TimeoutSeconds);if(!Keep) { Hold->TickerHandle.Reset(); }ObserveCumulative(*Work);LastRecordExecution=Work->SessionSnapshot();return Keep;
}
bool TickNativeCacheValidation(float Delta)
{
    if(!IsInGameThread()) { return false; }auto Hold=ActiveNativeValidationSession;if(!Hold) { return false; }if(Running) { return !ShuttingDown; }TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,Hold->Evidence.Get());if(!Work) { return false; }
    bool Keep=false;if(Work->Guard()) { Keep=TickNativeCacheValidationBody(Delta); }if(!Work->R.Error.IsEmpty()||!Work->Guard()) { CleanupNativeValidationSession(*Hold);Keep=false; }if(Hold->bTimedOut) { Work->Fail(TEXT("validation_timeout"),TEXT("Validation session timed out or lost its world"));Keep=false; }if(!Keep&&!Hold->bCleanupAttempted) { CleanupNativeValidationSession(*Hold); }Work->OperationComplete=!Keep;Work->R.Data->SetNumberField(TEXT("last_callback_frame"),static_cast<double>(GFrameCounter));Work->R.Data->SetStringField(TEXT("phase"),Keep?TEXT("scrubbing"):(Work->R.Error.IsEmpty()?TEXT("terminal_complete"):TEXT("terminal_failed")));Work->R.Data->SetBoolField(TEXT("timed_out"),Hold->bTimedOut);Work->R.Data->SetBoolField(TEXT("wall_timeout"),FPlatformTime::Seconds()-Hold->MonotonicStart>=Hold->TimeoutSeconds);if(!Keep) { Hold->TickerHandle.Reset(); }ObserveCumulative(*Work);LastValidationExecution=Work->SessionSnapshot();return Keep;
}
}
FString Record(const FString& ParamsJson)
{
    auto Fresh=MakeShared<FWork>(TEXT("record_chaos_cache"),true);TSharedPtr<FJsonObject> P;if(!Parse(ParamsJson,P)) { Fresh->Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return Fresh->Finish(nullptr); }
    if(!IsInGameThread()||!GEditor||Running||ShuttingDown||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()) { Fresh->Fail(TEXT("editor_required"),TEXT("Non-reentrant safe Editor game-thread required"));return Fresh->Finish(nullptr); }const FString Op=GetString(P,TEXT("operation"),TEXT("begin"));const bool ExistingOperation=Op==TEXT("sample")||Op==TEXT("finish")||Op==TEXT("abort");
    auto Hold=ActiveNativeRecordSession;auto W=ExistingOperation&&Hold&&Hold->Evidence?Hold->Evidence:Fresh;if(W==Fresh) { W->World=CurrentWorld();W->WorldCaptured=W->World.IsValid(); }W->Params=P;if(LastRecordExecution) { W->R.Data->SetObjectField(TEXT("last_session_execution_before_request"),LastRecordExecution); }
    TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,W.Get());TGuardValue<TSharedPtr<FWork>> Owner(CallOwner,W);TGuardValue<bool> WriterAdmission(W->RequiresForeignWriterAdmission,Op!=TEXT("sample"));W->StartCall();if(!W->Guard()&&Op!=TEXT("abort")) { return W->Finish(nullptr); }auto Legacy=RecordChaosCacheNative(P);if(!W->R.Error.IsEmpty()&&ActiveNativeRecordSession&&ActiveNativeRecordSession->Evidence==W&&!ActiveNativeRecordSession->bCleanupAttempted) { CleanupNativeRecordSession(*ActiveNativeRecordSession,false);W->OperationComplete=true; }const FString Reply=W->Finish(Legacy);if(!W->SessionId.IsEmpty()) { LastRecordExecution=W->SessionSnapshot(); }return Reply;
}
FString Validate(const FString& ParamsJson)
{
    auto Fresh=MakeShared<FWork>(TEXT("validate_chaos_cache"),true);TSharedPtr<FJsonObject> P;if(!Parse(ParamsJson,P)) { Fresh->Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return Fresh->Finish(nullptr); }
    if(!IsInGameThread()||!GEditor||Running||ShuttingDown||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()) { Fresh->Fail(TEXT("editor_required"),TEXT("Non-reentrant safe Editor game-thread required"));return Fresh->Finish(nullptr); }const FString Op=GetString(P,TEXT("operation"),TEXT("begin"));const bool ExistingOperation=Op==TEXT("sample")||Op==TEXT("finish")||Op==TEXT("abort");
    auto Hold=ActiveNativeValidationSession;auto W=ExistingOperation&&Hold&&Hold->Evidence?Hold->Evidence:Fresh;if(W==Fresh) { W->World=CurrentWorld();W->WorldCaptured=W->World.IsValid(); }W->Params=P;if(LastValidationExecution) { W->R.Data->SetObjectField(TEXT("last_session_execution_before_request"),LastValidationExecution); }
    TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,W.Get());TGuardValue<TSharedPtr<FWork>> Owner(CallOwner,W);TGuardValue<bool> WriterAdmission(W->RequiresForeignWriterAdmission,Op!=TEXT("sample"));W->StartCall();if(!W->Guard()&&Op!=TEXT("abort")) { return W->Finish(nullptr); }auto Legacy=ValidateChaosCacheNative(P);if(!W->R.Error.IsEmpty()&&ActiveNativeValidationSession&&ActiveNativeValidationSession->Evidence==W&&!ActiveNativeValidationSession->bCleanupAttempted) { CleanupNativeValidationSession(*ActiveNativeValidationSession);W->OperationComplete=true; }const FString Reply=W->Finish(Legacy);if(!W->SessionId.IsEmpty()) { LastValidationExecution=W->SessionSnapshot(); }return Reply;
}
TWeakObjectPtr<UGeometryCollectionComponent> FindActiveRecordedComponent(const AActor* Owner,const UWorld* ExpectedWorld)
{
    if(!IsInGameThread()||!GEditor||Running||Restoring||ShuttingDown||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||!Live(Owner)||!Live(ExpectedWorld)||ExpectedWorld->bIsTearingDown||CurrentWorld()!=ExpectedWorld||Owner->GetWorld()!=ExpectedWorld) { return {}; }
    const auto Hold=ActiveNativeRecordSession;
    if(!Hold||!Hold->Evidence||Hold->bCleanupAttempted||Hold->Evidence->OperationComplete||Hold->World.Get()!=ExpectedWorld) { return {}; }
    TWeakObjectPtr<UGeometryCollectionComponent> Match;
    for(const auto& Weak:ActiveCinematicRecordComponents)
    {
        auto* Candidate=Weak.Get();if(!Live(Candidate)||Candidate->GetOwner()!=Owner||Candidate->GetWorld()!=ExpectedWorld) { continue; }
        bool Captured=false;for(const auto& T:Hold->Evidence->Tracked) { if(T.Weak.Get()!=Candidate) { continue; }Captured=Live(T.Package.Get())&&Candidate->GetClass()==T.Class.Get()&&Candidate->GetOuter()==T.Outer.Get()&&Candidate->GetPackage()==T.Package.Get()&&Candidate->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)&&T.Package->GetName().Equals(T.PackagePath,ESearchCase::CaseSensitive)&&(!T.ComponentRestCaptured||(Live(T.ComponentRest.Get())&&Candidate->GetRestCollection()==T.ComponentRest.Get()))&&(!T.ExpectedParentCaptured||(T.ExpectedParent.Get()==Owner&&Candidate->IsIn(Owner)));break; }
        if(!Captured) { continue; }if(Match.IsValid()&&Match.Get()!=Candidate) { return {}; }Match=Candidate;
    }
    return Match;
}

bool HasActiveSessionForWorld(const UWorld* World)
{
    if(!IsInGameThread()) { return true; }if(!World) { return false; }
    if(Running&&Work&&Work->World.Get()==World) { return true; }
    const auto Record=ActiveNativeRecordSession;const auto Validation=ActiveNativeValidationSession;
    return (Record&&Record->World.Get()==World&&!Record->bCleanupComplete)||(Validation&&Validation->World.Get()==World&&!Validation->bCleanupComplete);
}

void Shutdown()
{
    if(!IsInGameThread()) { UE_LOG(LogTemp,Error,TEXT("Cubeless Chaos cache session shutdown requires the Editor game thread; no UObject restoration was attempted"));return; }ShuttingDown=true;auto RecordHold=ActiveNativeRecordSession;auto ValidationHold=ActiveNativeValidationSession;
    if(RecordHold&&RecordHold->TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(RecordHold->TickerHandle);RecordHold->TickerHandle.Reset(); }if(ValidationHold&&ValidationHold->TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(ValidationHold->TickerHandle);ValidationHold->TickerHandle.Reset(); }
    if(Running) { if(RecordHold&&RecordHold->Evidence) { RecordHold->Evidence->CancelRequested=true; }if(ValidationHold&&ValidationHold->Evidence) { ValidationHold->Evidence->CancelRequested=true; }UE_LOG(LogTemp,Warning,TEXT("Cubeless cache shutdown was reentrant; owned tickers were cancelled and active scopes retain session lifetime. Reentrant DLL unload is unsupported."));return; }
    TGuardValue<bool> Busy(Running,true);TGuardValue<bool> AllowRestore(ShutdownRestore,true);
    if(RecordHold&&RecordHold->Evidence) { TGuardValue<FWork*> Scope(Work,RecordHold->Evidence.Get());const bool Pending=!Work->OperationComplete;if(Pending) { CleanupNativeRecordSession(*RecordHold,false);Work->OperationComplete=true;if(Work->R.Error.IsEmpty()) { Work->Fail(TEXT("module_shutdown"),TEXT("Pending recording was cancelled during module shutdown; semantic cleanup is separate from recording completion"));Work->R.Stage=TEXT("cancelled_on_shutdown");Work->R.Data->SetStringField(TEXT("phase"),TEXT("cancelled_on_shutdown")); } }LastRecordExecution=Work->SessionSnapshot();UE_LOG(LogTemp,Display,TEXT("Cubeless record shutdown receipt: %s"),*Json(LastRecordExecution)); }
    if(ValidationHold&&ValidationHold->Evidence) { TGuardValue<FWork*> Scope(Work,ValidationHold->Evidence.Get());const bool Pending=!Work->OperationComplete;if(Pending) { CleanupNativeValidationSession(*ValidationHold);Work->OperationComplete=true;if(Work->R.Error.IsEmpty()) { Work->Fail(TEXT("module_shutdown"),TEXT("Pending validation was cancelled during module shutdown; semantic cleanup is separate from validation completion"));Work->R.Stage=TEXT("cancelled_on_shutdown");Work->R.Data->SetStringField(TEXT("phase"),TEXT("cancelled_on_shutdown")); } }LastValidationExecution=Work->SessionSnapshot();UE_LOG(LogTemp,Display,TEXT("Cubeless validation shutdown receipt: %s"),*Json(LastValidationExecution)); }
    ActiveNativeRecordSession.Reset();ActiveNativeValidationSession.Reset();ActiveCinematicRecordComponents.Reset();
}
}
