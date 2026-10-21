#include "Compatibility/UE58/CubelessChaosTraceStrainApi.h"
#include "Compatibility/UE58/CubelessChaosDestructionPerformanceApi.h"
#include "Compatibility/UE58/CubelessChaosLearningApi.h"
#include "Compatibility/UE58/CubelessChaosCacheSessionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Chaos/Particle/ObjectState.h"
#include "Containers/Ticker.h"
#include "Components/ActorComponent.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionProxyData.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <cmath>
#include <atomic>

namespace Cubeless::UE58::ChaosTraceStrain
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false,ShuttingDown=false;
bool Live(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
UWorld* CurrentWorld() { if(!GEditor) { return nullptr; }if(auto* C=GEditor->GetPIEWorldContext()) { if(C->World()) { return C->World(); } }return GEditor->GetEditorWorldContext().World(); }
FString Json(const TSharedPtr<FJsonObject>& O) { FString S;if(O) { FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S)); }return S; }
bool Dependency(const FString& S) { for(const TCHAR* K:{TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("cubelessmcp"),TEXT("cubeless_mcp"),TEXT("modelcontextprotocol"),TEXT("toolsetregistry"),TEXT("/Engine/PythonTypes")}) { if(S.Contains(K,ESearchCase::IgnoreCase)) { return true; } }return false; }
struct FExactKeys:DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& S) { return FCrc::StrCrc32(*S); }
};
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||S.Len()>1048576) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeys>> Keys;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token)) { if(Token==EJsonNotation::Error||++Slots>262144) { return false; }if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>64) { return false; } }else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber())) { return false; } }
    return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)&&Out.IsValid();
}
struct FTarget
{
    TStrongObjectPtr<const UObject> Hold{nullptr};TWeakObjectPtr<const UObject> Weak,Outer,Parent;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package;FString Path,PackagePath,Role;bool HasParent=false;
};
struct FPhysicsEpoch
{
    const UActorComponent* const Target;std::atomic<uint64> Revision{0};
    explicit FPhysicsEpoch(const UActorComponent* In):Target(In) {}
};
struct FWork
{
    FResult R;FString SessionId,FailedStage;bool RequiresForeignWriterAdmission=true;bool OperationComplete=true,CancelRequested=false,WorldCaptured=false;TWeakObjectPtr<UWorld> World;TArray<FTarget> Targets;
    TSharedPtr<FPhysicsEpoch,ESPMode::ThreadSafe> PhysicsEpoch;FDelegateHandle PhysicsCreatedHandle,PhysicsDestroyedHandle;bool PhysicsCaptured=false,PhysicsRegistered=false,PhysicsCreated=false;const void* ProxyIdentity=nullptr;const void* DynamicIdentity=nullptr;
    TWeakObjectPtr<const UGeometryCollection> Rest;const FGeometryCollection* RestStorageIdentity=nullptr;TWeakObjectPtr<const UGeometryCollectionComponent> RestComponent;
    uint64 Serial=0,ControlSerial=0,EngineSerial=0,ChangedSerial=0,CallSerial=0,CallControl=0,CallEngine=0,CallChanged=0;
    TArray<TSharedPtr<FJsonValue>> Events;TSharedPtr<FJsonObject> AdoptedExecution;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();
    FWork() { R.Stage=TEXT("validate_request");R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_wrapper_wait_performed"),false);R.Data->SetBoolField(TEXT("physics_commands_completed_verified"),false);R.Data->SetStringField(TEXT("cleanup_scope"),TEXT("Cleanup removes only the owned ticker/session. Already sent solver commands are not cancelled, replayed, flushed or rolled back; no forced transform/simulation restore is performed."));R.Data->SetStringField(TEXT("state_scope"),TEXT("Session-control state changes and actual observed engine changes are distinguished. Prepare/miss physics dispatch0 does not promise the plugin session state or SDK global state is unchanged."));R.Data->SetStringField(TEXT("strong_reference_scope"),TEXT("Captured UObject strong pins exist only during one API request/ticker callback. Retained prepared/terminal sessions contain weak identity and plain evidence; abort/Shutdown do not reacquire targets.")); }
    void Count(const TCHAR* K) { double N=0;Counts->TryGetNumberField(K,N);Counts->SetNumberField(K,N+1); }
    void SDK(const TCHAR* S) { R.Stage=S;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_effects_unknown"),true); }
    void Event(const TCHAR* S,bool Engine) { ++Serial;if(Engine) { ++EngineSerial;SDK(S); }else { ++ControlSerial;++ChangedSerial;R.Stage=S;R.bChanged=true; }R.bMutationAttempted=true;auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),Serial);J->SetStringField(TEXT("stage"),S);J->SetStringField(TEXT("scope"),Engine?TEXT("engine_command_attempt"):TEXT("owned_session_control"));if(Events.Num()<65536) { Events.Add(MakeShared<FJsonValueObject>(J)); }else { R.Data->SetBoolField(TEXT("event_prefix_truncated"),true); } }
    void ObservedChange() { ++ChangedSerial;R.bChanged=true; }
    bool Fail(const TCHAR* C,const FString& E) { if(R.Error.IsEmpty()) { R.ErrorCode=TEXT("chaos_trace_strain_")+FString(C);R.Error=E;FailedStage=R.Stage;R.Data->SetStringField(TEXT("failed_stage"),FailedStage); }return false; }
    bool Guard(bool IgnoreCancel=false)
    {
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||ShuttingDown||(!IgnoreCancel&&CancelRequested)) { return Fail(TEXT("editor_lifetime_lost"),TEXT("Safe Editor game thread/Engine/GC lifetime is unavailable or cancellation was requested")); }
        if(WorldCaptured&&(!Live(World.Get())||World->bIsTearingDown||CurrentWorld()!=World.Get())) { return Fail(TEXT("world_lost"),TEXT("Captured current Editor/PIE world changed or is tearing down")); }
        if(RequiresForeignWriterAdmission&&WorldCaptured&&Cubeless::UE58::ChaosLearning::HasActiveSessionForWorld(World.Get())) { return Fail(TEXT("learning_session_conflict"),TEXT("An active or unresolved GA evaluation owns this world")); }
        if(RequiresForeignWriterAdmission&&WorldCaptured&&Cubeless::UE58::ChaosDestructionPerformance::HasActiveSessionForWorld(World.Get())) { return Fail(TEXT("performance_session_conflict"),TEXT("A pending GB performance session owns resources in this world")); }
        for(const auto& T:Targets) { const auto* O=T.Weak.Get();if(!Live(O)||!Live(T.Package.Get())||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)||!T.Package->GetName().Equals(T.PackagePath,ESearchCase::CaseSensitive)||(T.HasParent&&(!Live(T.Parent.Get())||!O->IsIn(T.Parent.Get())))) { return Fail(TEXT("target_identity_lost"),TEXT("Captured class/outer/package/path/owner identity changed")); }if((O->IsA<AActor>()||O->IsA<UActorComponent>())&&WorldCaptured&&O->GetWorld()!=World.Get()) { return Fail(TEXT("target_world_lost"),TEXT("Captured actor/component world changed")); } }
        if(RestComponent.IsValid()&&(!Live(Rest.Get())||RestComponent->GetRestCollection()!=Rest.Get()||Rest->GetGeometryCollection().Get()!=RestStorageIdentity)) { return Fail(TEXT("rest_identity_lost"),TEXT("Captured rest asset or managed storage changed")); }
        if(PhysicsCaptured) { if(!PhysicsEpoch||PhysicsEpoch->Revision.load(std::memory_order_acquire)!=0) { return Fail(TEXT("physics_lifetime_changed"),TEXT("Captured component physics lifecycle changed; queued commands are not retried")); }const auto* C=RestComponent.Get();if(!Live(C)||static_cast<const void*>(C->GetPhysicsProxy())!=ProxyIdentity||static_cast<const void*>(C->GetDynamicCollection())!=DynamicIdentity||C->IsRegistered()!=PhysicsRegistered||C->IsPhysicsStateCreated()!=PhysicsCreated) { return Fail(TEXT("physics_storage_changed"),TEXT("Captured physics proxy/dynamic/registration/state identity changed")); } }return true;
    }
    bool Track(const UObject* O,const TCHAR* Role,const UObject* Parent=nullptr)
    {
        if(!Live(O)||!Live(O->GetPackage())||Dependency(O->GetClass()->GetPathName())||(Parent&&!O->IsIn(Parent))) { return Fail(TEXT("target_unavailable"),TEXT("Live runtime-safe captured object required")); }for(const auto& T:Targets) { if(T.Weak.Get()==O) { return Guard(); } }if(Targets.Num()>=8192) { return Fail(TEXT("object_budget"),TEXT("Captured object budget exceeded")); }auto& T=Targets.AddDefaulted_GetRef();T.Hold.Reset(O);T.Weak=O;T.Outer=O->GetOuter();T.Parent=Parent;T.HasParent=Parent!=nullptr;T.Class=O->GetClass();T.Package=O->GetPackage();T.Path=O->GetPathName();T.PackagePath=O->GetPackage()->GetName();T.Role=Role;return Guard();
    }
    void RemovePhysicsGuards()
    {
        if(PhysicsCreatedHandle.IsValid()) { UActorComponent::GlobalCreatePhysicsDelegate.Remove(PhysicsCreatedHandle);PhysicsCreatedHandle.Reset(); }
        if(PhysicsDestroyedHandle.IsValid()) { UActorComponent::GlobalDestroyPhysicsDelegate.Remove(PhysicsDestroyedHandle);PhysicsDestroyedHandle.Reset(); }
        if(PhysicsCaptured||PhysicsEpoch) { R.Data->SetBoolField(TEXT("physics_identity_guard_retired"),true); }PhysicsCaptured=false;
    }
    bool CapturePhysicsIdentity(UGeometryCollectionComponent* C)
    {
        if(!Guard()||!R.Error.IsEmpty()||!Live(C)) { return false; }if(PhysicsEpoch) { return Fail(TEXT("physics_epoch_reuse"),TEXT("A trace physics lifetime ticket cannot be reused for another dispatch")); }
        SDK(TEXT("capture_dispatch_physics_identity"));PhysicsEpoch=MakeShared<FPhysicsEpoch,ESPMode::ThreadSafe>(C);const auto Ticket=PhysicsEpoch;Event(TEXT("bind_trace_physics_lifecycle_guards"),false);
        PhysicsCreatedHandle=UActorComponent::GlobalCreatePhysicsDelegate.AddLambda([Ticket](UActorComponent* Changed){if(Changed==Ticket->Target) { Ticket->Revision.fetch_add(1,std::memory_order_release); }});
        PhysicsDestroyedHandle=UActorComponent::GlobalDestroyPhysicsDelegate.AddLambda([Ticket](UActorComponent* Changed){if(Changed==Ticket->Target) { Ticket->Revision.fetch_add(1,std::memory_order_release); }});
        ProxyIdentity=static_cast<const void*>(C->GetPhysicsProxy());DynamicIdentity=static_cast<const void*>(C->GetDynamicCollection());PhysicsRegistered=C->IsRegistered();PhysicsCreated=C->IsPhysicsStateCreated();PhysicsCaptured=true;R.Data->SetBoolField(TEXT("physics_identity_guard_captured"),true);R.Data->SetBoolField(TEXT("captured_proxy_present"),ProxyIdentity!=nullptr);R.Data->SetBoolField(TEXT("captured_dynamic_present"),DynamicIdentity!=nullptr);R.Data->SetBoolField(TEXT("captured_registered"),PhysicsRegistered);R.Data->SetBoolField(TEXT("captured_physics_created"),PhysicsCreated);return Guard();
    }
    void StartCall() { CallSerial=Serial;CallControl=ControlSerial;CallEngine=EngineSerial;CallChanged=ChangedSerial; }
    TSharedRef<FJsonObject> SessionSnapshot()
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("session_id"),SessionId);J->SetBoolField(TEXT("operation_complete"),OperationComplete);bool Postcondition=false;R.Data->TryGetBoolField(TEXT("session_postcondition_passed"),Postcondition);J->SetBoolField(TEXT("postcondition_passed"),Postcondition);J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("failed_stage"),FailedStage);J->SetStringField(TEXT("error_code"),R.ErrorCode);J->SetStringField(TEXT("error"),R.Error);J->SetBoolField(TEXT("execution_mutation_attempted"),R.bMutationAttempted);J->SetBoolField(TEXT("changed"),R.bChanged);J->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);J->SetBoolField(TEXT("compile_attempted"),false);J->SetBoolField(TEXT("compiled"),false);J->SetBoolField(TEXT("save_attempted"),false);J->SetBoolField(TEXT("saved"),false);J->SetArrayField(TEXT("actual_mutation_prefix"),Events);J->SetArrayField(TEXT("actual_save_prefix"),{});J->SetObjectField(TEXT("native_call_counts"),MakeShared<FJsonObject>(*Counts));J->SetBoolField(TEXT("engine_mutation_attempted"),EngineSerial>0);J->SetNumberField(TEXT("state_control_count"),ControlSerial);J->SetNumberField(TEXT("physics_lifetime_revision"),PhysicsEpoch?static_cast<double>(PhysicsEpoch->Revision.load(std::memory_order_acquire)):0.0);for(const TCHAR* K:{TEXT("phase"),TEXT("cleanup_complete"),TEXT("timed_out"),TEXT("wall_timeout"),TEXT("last_callback_frame"),TEXT("postcondition_identity_verified"),TEXT("physics_identity_guard_captured"),TEXT("physics_identity_guard_retired"),TEXT("captured_proxy_present"),TEXT("captured_dynamic_present"),TEXT("captured_registered"),TEXT("captured_physics_created")}) { if(R.Data->HasField(K)) { J->SetField(K,R.Data->TryGetField(K)); } }return J;
    }
    FString Finish(TSharedPtr<FJsonObject> Legacy)
    {
        if(!Legacy) { Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error.IsEmpty()?TEXT("No legacy response returned"):R.Error); }bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);const TSharedPtr<FJsonObject>* D=nullptr;FString Assertion;if(Legacy->TryGetObjectField(TEXT("data"),D)&&D&&D->IsValid()) { (*D)->TryGetStringField(TEXT("assertion_status"),Assertion); }
        if(!Success&&R.Error.IsEmpty()) { FString E;Legacy->TryGetStringField(TEXT("error"),E);Fail(TEXT("operation_failed"),E); }if(Assertion.Equals(TEXT("FAIL"),ESearchCase::CaseSensitive)&&R.Error.IsEmpty()) { Fail(TEXT("assertion_failed"),TEXT("Legacy trace assertion_status is FAIL")); }
        FResult Out=R;Out.Data=MakeShared<FJsonObject>(*R.Data);Out.bSuccess=Success&&R.Error.IsEmpty();Out.bPostconditionPassed=Out.bSuccess;Out.bMutationAttempted=Serial>CallSerial;Out.bChanged=ChangedSerial>CallChanged;Out.bSaveAttempted=false;Out.bSaved=false;Out.Compiled=false;
        TArray<TSharedPtr<FJsonValue>> CurrentEvents;for(const auto& E:Events) { double I=0;E->AsObject()->TryGetNumberField(TEXT("index"),I);if(I>static_cast<double>(CallSerial)) { CurrentEvents.Add(E); } }Out.Data->SetArrayField(TEXT("actual_mutation_prefix"),CurrentEvents);Out.Data->SetArrayField(TEXT("actual_save_prefix"),{});Out.Data->SetObjectField(TEXT("legacy_result"),Legacy);Out.Data->SetStringField(TEXT("session_id"),SessionId);Out.Data->SetBoolField(TEXT("operation_complete"),OperationComplete);Out.Data->SetObjectField(TEXT("session_execution"),AdoptedExecution?AdoptedExecution:SessionSnapshot());Out.Data->SetBoolField(TEXT("engine_mutation_attempted"),EngineSerial>CallEngine);Out.Data->SetBoolField(TEXT("state_control_changed"),ControlSerial>CallControl);Out.Data->SetBoolField(TEXT("global_change_state_unknown"),!Out.bChangeStateKnown);Out.Data->SetStringField(TEXT("receipt_scope"),TEXT("Top-level flags belong to this API request; session_execution is the cumulative ticker/session snapshot. No tick mutates a returned Python journal and no prior save is promoted into this request."));if(Out.bSuccess) { Out.Stage=OperationComplete?TEXT("complete"):TEXT("scheduled_or_in_progress"); }else if(!FailedStage.IsEmpty()) { Out.Stage=FailedStage; }return Cubeless::Operations::EncodeVariableResult(Out,TEXT("trigger_trace_external_strain"));
    }
};
// Retained sessions keep weak identity and plain evidence only. Strong pins exist
// during one API request or ticker callback and are released before returning.
struct FExecutionPins
{
    FWork& W;
    explicit FExecutionPins(FWork& In):W(In) {}
    bool Acquire()
    {
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||ShuttingDown)
        {
            return W.Fail(TEXT("editor_lifetime_lost"),TEXT("Safe execution scope cannot acquire captured objects"));
        }
        for(auto& T:W.Targets)
        {
            const UObject* O=T.Weak.Get();
            if(!Live(O)) { return W.Fail(TEXT("target_identity_lost"),TEXT("Captured weak target expired between trace requests")); }
            T.Hold.Reset(O);
        }
        return W.Guard();
    }
    ~FExecutionPins()
    {
        // Releasing a pin does not call a UObject getter, restore physics or load
        // a replacement. This also releases targets added during this scope.
        for(auto& T:W.Targets) { T.Hold.Reset(); }
    }
};
FWork* Work=nullptr;TSharedPtr<FWork> CallOwner;
bool Guard() { return Work&&Work->Guard(); }
template<typename F>bool Step(const TCHAR* Stage,F&& Fn,bool Mutating=true)
{
    if(!Guard()||!Work->R.Error.IsEmpty()) { return false; }if(Mutating) { Work->Event(Stage,true); }else { Work->SDK(Stage); }Work->Count(Stage);Fn();return Guard()&&Work->R.Error.IsEmpty();
}
struct FLegacy
{
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& E) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),E);Work->Fail(TEXT("operation_failed"),E);return J; }
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& D) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),true);J->SetObjectField(TEXT("data"),D);return J; }
};
TSharedPtr<FJsonObject> CommandError(const FString& E) { return FLegacy::CreateErrorResponse(E); }
int32 CastI32(double V,const TCHAR* K) { if(!FMath::IsFinite(V)||V<=static_cast<double>(MIN_int32)-1||V>=static_cast<double>(MAX_int32)+1) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely convert to int32"),K));return 0; }return static_cast<int32>(V); }
float CastFloat(double V,const TCHAR* K) { if(!FMath::IsFinite(V)||FMath::Abs(V)>TNumericLimits<float>::Max()) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely convert to float32"),K));return 0; }return static_cast<float>(V); }
int64 Quantize(double V) { const double Q=V*10;if(!FMath::IsFinite(Q)||Q<=static_cast<double>(MIN_int64)+4096||Q>=static_cast<double>(MAX_int64)-4096) { Work->Fail(TEXT("hash_numeric_range"),TEXT("Dynamic transform hash cannot safely quantize value"));return 0; }return FMath::RoundToInt64(Q); }
bool RestReadable(const UGeometryCollectionComponent* C)
{
    if(!Guard()||!Live(C)) { return false; }const auto* A=C->GetRestCollection();if(!Live(A)) { return Work->Fail(TEXT("rest_missing"),TEXT("A live rest collection is required")); }const auto Data=A->GetGeometryCollection();if(!Data.IsValid()) { return Work->Fail(TEXT("rest_missing"),TEXT("A live rest collection is required")); }
    const int32 N=Data->NumElements(FGeometryCollection::TransformGroup);if(N>1000000||Data->Transform.Num()!=N||Data->Parent.Num()!=N||Data->Children.Num()!=N||Data->SimulationType.Num()!=N) { return Work->Fail(TEXT("rest_schema"),TEXT("Rest transform group lengths or budget are invalid")); }
    TArray<uint8> Colors;Colors.Init(0,N);for(int32 I=0;I<N;++I) { int32 K=I;while(K!=INDEX_NONE&&Data->Parent.IsValidIndex(K)&&Colors[K]==0) { Colors[K]=1;K=Data->Parent[K]; }if(K!=INDEX_NONE&&(!Data->Parent.IsValidIndex(K)||Colors[K]==1)) { return Work->Fail(TEXT("rest_parent_cycle"),TEXT("Rest parent chain is invalid or cyclic")); }K=I;while(K!=INDEX_NONE&&Data->Parent.IsValidIndex(K)&&Colors[K]==1) { Colors[K]=2;K=Data->Parent[K]; }for(int32 Child:Data->Children[I]) { if(!Data->Parent.IsValidIndex(Child)||Data->Parent[Child]!=I) { return Work->Fail(TEXT("rest_child"),TEXT("Rest child does not reciprocate its parent")); } }const int32 P=Data->Parent[I];if(P!=INDEX_NONE&&!Data->Children[P].Contains(I)) { return Work->Fail(TEXT("rest_parent_child"),TEXT("Rest parent does not contain its child")); }if(Data->Transform[I].ContainsNaN()) { return Work->Fail(TEXT("rest_transform"),TEXT("Rest transform is non-finite")); } }
    Work->Rest=A;Work->RestStorageIdentity=Data.Get();Work->RestComponent=C;return Work->Track(A,TEXT("captured_rest_asset"))&&Guard();
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
            return *It;
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

struct FCinematicTraceObservationSession
{
    FString SessionId;
    TSharedPtr<FWork> Evidence;double MonotonicDispatchStart=0;bool PhysicsIdsComplete=true;
    FString ActorName;
    FString ActorClassPath;
    FString RestCollectionPath;
    FString PreparedWorldPath;
    FString TraceQueryScope;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AActor> Owner;
    TWeakObjectPtr<UGeometryCollectionComponent> Component;
    FTSTicker::FDelegateHandle TickerHandle;
    int32 CollisionItemIndex = INDEX_NONE;
    int32 ChosenRigidLeafIndex = INDEX_NONE;
    int32 ItemIndex = INDEX_NONE;
    double ChosenRigidLeafDistance = -1.0;
    TArray<int32> TargetTransformIndices;
    FVector RequestedVelocity = FVector::ZeroVector;
    FVector ImpactPoint = FVector::ZeroVector;
    float Radius = 0.0f;
    float Strain = 0.0f;
    int32 PropagationDepth = 1;
    float PropagationFactor = 0.5f;
    float RadialImpulseStrength = 0.0f;
    FString InitialTransformSha256;
    FString FinalTransformSha256;
    int32 InitialPhysicsObjectCount = 0;
    int32 FinalPhysicsObjectCount = 0;
    int32 InitialDisabledCount = 0;
    int32 FinalDisabledCount = 0;
    TArray<int32> InitialPhysicsObjectIds;
    TArray<int32> FinalPhysicsObjectIds;
    TArray<int32> InitialPhysicsObjectStates;
    TArray<int32> FinalPhysicsObjectStates;
    TArray<bool> InitialPhysicsObjectDisabled;
    TArray<bool> FinalPhysicsObjectDisabled;
    TArray<FVector> InitialPhysicsObjectPositions;
    TArray<FVector> FinalPhysicsObjectPositions;
    int32 ObservedTickCount = 0;
    double MaximumVelocityDirectionDot = -1.0;
    bool bReady = false;
    bool bTracePrepared = false;
    bool bExternalStrainDispatched = false;
    bool bTimedOut = false;
    bool bBreakingVelocityDispatched = false;
    bool bBreakingVelocityArmedBeforeRelease = false;
    bool bRadialImpulseDispatched = false;
    int32 UniformVelocityFieldDispatchCount = 0;
    int32 LocalizedImpulseDispatchCount = 0;
    bool bReleaseObservedBeforeMotion = false;
    int32 PostMotionObservedTickCount = 0;
    uint64 DispatchFrame = 0;
    uint64 ReleaseObservedFrame = 0;
    uint64 BreakingVelocityDispatchFrame = 0;
};

TSharedPtr<FCinematicTraceObservationSession> ActiveCinematicTraceObservation;
int32 ResolveLocalizedTraceClusterItem(
    const UGeometryCollectionComponent* Component,
    const int32 CollisionItem,
    const FVector& WorldImpactPoint,
    int32& OutClosestLeaf,
    double& OutClosestLeafDistance)
{
    OutClosestLeaf = INDEX_NONE;
    OutClosestLeafDistance = -1.0;
    const UGeometryCollection* Rest = Component ? Component->GetRestCollection() : nullptr;
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection =
        Rest ? Rest->GetGeometryCollection() : nullptr;
    if (!Component || !Collection.IsValid() ||
        !Collection->Children.IsValidIndex(CollisionItem))
    {
        return CollisionItem;
    }
    TArray<FTransform> GlobalTransforms;
    GeometryCollectionAlgo::GlobalMatrices(
        Collection->Transform, Collection->Parent, GlobalTransforms);
    const FVector LocalImpact = Component->GetComponentTransform().InverseTransformPosition(
        WorldImpactPoint);
    int32 ClosestLeaf = INDEX_NONE;
    double ClosestDistanceSquared = TNumericLimits<double>::Max();
    TArray<int32> Pending = {CollisionItem};
    while (!Pending.IsEmpty())
    {
        const int32 TransformIndex = Pending.Pop(EAllowShrinking::No);
        if (!Collection->Children.IsValidIndex(TransformIndex))
        {
            continue;
        }
        if (Collection->Children[TransformIndex].IsEmpty())
        {
            if (GlobalTransforms.IsValidIndex(TransformIndex))
            {
                const double DistanceSquared = FVector::DistSquared(
                    GlobalTransforms[TransformIndex].GetLocation(), LocalImpact);
                if (DistanceSquared < ClosestDistanceSquared)
                {
                    ClosestDistanceSquared = DistanceSquared;
                    ClosestLeaf = TransformIndex;
                }
            }
            continue;
        }
        for (const int32 Child : Collection->Children[TransformIndex])
        {
            Pending.Add(Child);
        }
    }
    if (ClosestLeaf == INDEX_NONE || !Collection->Parent.IsValidIndex(ClosestLeaf))
    {
        return CollisionItem;
    }
    OutClosestLeaf = ClosestLeaf;
    OutClosestLeafDistance = FMath::Sqrt(ClosestDistanceSquared);
    // UE 5.6 resolves ApplyExternalStrain and ApplyBreakingLinearVelocity
    // against the active collision cluster. A disabled descendant from the
    // rest hierarchy is not an equivalent solver target, so retain the
    // collision item and expose the nearest leaf only as trace evidence.
    return CollisionItem;
}

FString HashSingleDynamicCollection(const UGeometryCollectionComponent* Component)
{
    if(!Guard()||!Live(Component)) { return FString(); }Work->SDK(TEXT("hash_dynamic_collection"));
    FString Canonical = Component ? Component->GetPathName() : FString();
    if (Component)
    {
        if (const FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection())
        {
            if(Dynamic->GetNumTransforms()>1000000) { Work->Fail(TEXT("dynamic_budget"),TEXT("Dynamic collection exceeds one million transforms"));return FString(); }
            for (int32 Index = 0; Index < Dynamic->GetNumTransforms(); ++Index)
            {
                const FTransform Transform(Dynamic->GetTransform(Index));
                const FVector Location = Transform.GetLocation();
                const FRotator Rotation = Transform.Rotator();
                Canonical += FString::Printf(TEXT("|%d:%lld,%lld,%lld:%lld,%lld,%lld"), Index,
                    Quantize(Location.X), Quantize(Location.Y),
                    Quantize(Location.Z), Quantize(Rotation.Roll),
                    Quantize(Rotation.Pitch), Quantize(Rotation.Yaw));
            }
        }
    }
    FTCHARToUTF8 Utf8(*Canonical);
    return Sha256Bytes(Utf8.Get(), Utf8.Length());
}

void CaptureTracePhysicsObservation(FCinematicTraceObservationSession& Session)
{
    auto* Component=Session.Component.Get();if(!Component||!Guard()) { return; }TArray<Chaos::FPhysicsObject*> Objects;if(!Step(TEXT("GetAllPhysicsObjects"),[&]{Objects=Component->GetAllPhysicsObjects();},false)) { return; }
    if(Objects.Num()>1000000) { Work->Fail(TEXT("physics_budget"),TEXT("Physics object snapshot exceeds one million slots"));return; }
    TSet<Chaos::FPhysicsObject*> Seen;TSet<int32> Ids;int32 DisabledCount=0;Session.FinalPhysicsObjectIds.Reset();Session.FinalPhysicsObjectStates.Reset();Session.FinalPhysicsObjectDisabled.Reset();Session.FinalPhysicsObjectPositions.Reset();
    for(auto* Object:Objects)
    {
        if(!Object) { continue; }if(Seen.Contains(Object)) { Work->Fail(TEXT("physics_duplicate_pointer"),TEXT("Physics object list contains a duplicate pointer"));return; }Seen.Add(Object);if(!Guard()) { return; }
        bool Disabled=false;int32 Id=INDEX_NONE,State=INDEX_NONE;FVector Position,Velocity;
        Work->SDK(TEXT("read_locked_physics_snapshot"));Work->Count(TEXT("physics_read_locks"));
        {
            FLockedReadPhysicsObjectExternalInterface Read=FPhysicsObjectExternalInterface::LockRead(Object);Disabled=Read->AreAllDisabled(MakeArrayView(&Object,1));const auto* Particle=Read->GetRigidParticle(Object);Id=Chaos::FPhysicsObjectInterface::GetId(Object);State=Particle?static_cast<int32>(Particle->ObjectState()):INDEX_NONE;Position=Read->GetX(Object);Velocity=Read->GetV(Object);
        }
        if(!Guard()) { return; }if(Id==INDEX_NONE||Ids.Contains(Id)||Position.ContainsNaN()||Velocity.ContainsNaN()) { Work->Fail(TEXT("physics_identity_or_vector"),TEXT("Physics ids must be unique and observations finite"));return; }Ids.Add(Id);DisabledCount+=Disabled?1:0;Session.FinalPhysicsObjectIds.Add(Id);Session.FinalPhysicsObjectStates.Add(State);Session.FinalPhysicsObjectDisabled.Add(Disabled);Session.FinalPhysicsObjectPositions.Add(Position);
        if(!Velocity.IsNearlyZero()&&!Session.RequestedVelocity.IsNearlyZero()) { Session.MaximumVelocityDirectionDot=FMath::Max(Session.MaximumVelocityDirectionDot,FVector::DotProduct(Velocity.GetSafeNormal(),Session.RequestedVelocity.GetSafeNormal())); }
    }
    Session.FinalPhysicsObjectCount=Objects.Num();Session.FinalDisabledCount=DisabledCount;Session.FinalTransformSha256=HashSingleDynamicCollection(Component);if(!Guard()||!Work->R.Error.IsEmpty()) { return; }
    if(Session.bExternalStrainDispatched)
    {
        TSet<int32> InitialIds(Session.InitialPhysicsObjectIds);Session.PhysicsIdsComplete&=InitialIds.Num()==Session.InitialPhysicsObjectIds.Num()&&InitialIds.Num()==Ids.Num();for(int32 Id:InitialIds) { Session.PhysicsIdsComplete&=Ids.Contains(Id); }
        if(!Session.InitialTransformSha256.IsEmpty()&&!Session.InitialTransformSha256.Equals(Session.FinalTransformSha256,ESearchCase::CaseSensitive)) { Work->ObservedChange(); }
    }
}

TSharedPtr<FJsonObject> LastSessionExecution;
void CleanupCinematicTraceObservation(const TCHAR* Reason=TEXT("explicit_cleanup"))
{
    auto Hold=ActiveCinematicTraceObservation;if(!Hold) { if(Work) { Work->R.Data->SetBoolField(TEXT("cleanup_complete"),true); }return; }
    auto E=Hold->Evidence;const bool Pending=E&&!E->OperationComplete;const bool HadTicker=Hold->TickerHandle.IsValid();
    if(HadTicker) { FTSTicker::GetCoreTicker().RemoveTicker(Hold->TickerHandle);Hold->TickerHandle.Reset(); }
    if(E)
    {
        E->RemovePhysicsGuards();if(Pending||HadTicker) { E->Event(Reason,false); }E->OperationComplete=true;E->R.Data->SetBoolField(TEXT("cleanup_complete"),true);if(Pending) { E->R.Stage=Reason;E->R.Data->SetStringField(TEXT("phase"),Reason); }LastSessionExecution=E->SessionSnapshot();
        if(Work&&Work!=E.Get()) { Work->Event(Reason,false);Work->R.bChangeStateKnown&=E->R.bChangeStateKnown;Work->R.Data->SetObjectField(TEXT("previous_session_execution"),LastSessionExecution); }
    }
    if(Work) { Work->R.Data->SetBoolField(TEXT("cleanup_complete"),true); }
}
bool TickCinematicTraceObservation(float Delta);

bool TickCinematicTraceObservationBody(float)
{
    if (!ActiveCinematicTraceObservation) return false;
    FCinematicTraceObservationSession& Session = *ActiveCinematicTraceObservation;
    if (!Session.World.IsValid() || !Session.Component.IsValid())
    {
        Session.bTimedOut = true;
        Session.TickerHandle.Reset();
        return false;
    }
    if (GFrameCounter <= Session.DispatchFrame) return true;
    CaptureTracePhysicsObservation(Session);if(!Guard()||!Work->R.Error.IsEmpty()) { return false; }if(!Session.PhysicsIdsComplete) { return Work->Fail(TEXT("physics_id_set_changed"),TEXT("Physics object identity set changed; further impulses were not sent")); }
    ++Session.ObservedTickCount;
    if (!Session.bReleaseObservedBeforeMotion &&
        Session.FinalDisabledCount < Session.InitialDisabledCount)
    {
        Session.bReleaseObservedBeforeMotion = true;
        Session.ReleaseObservedFrame = GFrameCounter;
    }
    if (Session.bReleaseObservedBeforeMotion && !Session.bRadialImpulseDispatched &&
        GFrameCounter > Session.ReleaseObservedFrame)
    {
        if (UGeometryCollectionComponent* Component = Session.Component.Get())
        {
            const FVector Direction = Session.RequestedVelocity.GetSafeNormal();
            const FVector ImpulseOrigin = Session.ImpactPoint - Direction * FMath::Max(25.0f, Session.Radius * 0.25f);
            if(!Step(TEXT("AddRadialImpulse"),[&]{Component->AddRadialImpulse(
                ImpulseOrigin,Session.Radius,Session.RadialImpulseStrength,ERadialImpulseFalloff::RIF_Constant,true);Session.bRadialImpulseDispatched=true;})) { return false; }
            if(!Step(TEXT("AddImpulse"),[&]{Component->AddImpulse(Session.RequestedVelocity,NAME_None,true);++Session.UniformVelocityFieldDispatchCount;})) { return false; }
            if(!Step(TEXT("AddImpulseAtLocation"),[&]{Component->AddImpulseAtLocation(
                Session.RequestedVelocity.GetSafeNormal()*Session.RadialImpulseStrength*10000.0f,Session.ImpactPoint);++Session.LocalizedImpulseDispatchCount;})) { return false; }
        }
    }
    if (Session.bRadialImpulseDispatched)
    {
        ++Session.PostMotionObservedTickCount;
    }
    if (Session.bRadialImpulseDispatched && Session.PostMotionObservedTickCount >= 30)
    {
        Session.bReady = true;
        Session.TickerHandle.Reset();
        return false;
    }
    if (Session.ObservedTickCount >= 60)
    {
        Session.bTimedOut = true;
        Session.TickerHandle.Reset();
        return false;
    }
    return true;
}

struct FSameIdEvidence
{
    bool Complete=true;int32 ReleasedDynamic=0;double MaximumDisplacement=0;
};
FSameIdEvidence SameIdEvidence(const FCinematicTraceObservationSession& S)
{
    FSameIdEvidence E;TMap<int32,int32> Final;for(int32 I=0;I<S.FinalPhysicsObjectIds.Num();++I) { const int32 Id=S.FinalPhysicsObjectIds[I];if(Final.Contains(Id)) { E.Complete=false; }Final.Add(Id,I); }
    TSet<int32> Initial;for(int32 I=0;I<S.InitialPhysicsObjectIds.Num();++I) { const int32 Id=S.InitialPhysicsObjectIds[I];if(Initial.Contains(Id)) { E.Complete=false; }Initial.Add(Id);const int32* J=Final.Find(Id);if(!J||!S.InitialPhysicsObjectDisabled.IsValidIndex(I)||!S.InitialPhysicsObjectPositions.IsValidIndex(I)||!S.FinalPhysicsObjectDisabled.IsValidIndex(*J)||!S.FinalPhysicsObjectPositions.IsValidIndex(*J)||!S.FinalPhysicsObjectStates.IsValidIndex(*J)) { E.Complete=false;continue; }if(S.InitialPhysicsObjectDisabled[I]&&!S.FinalPhysicsObjectDisabled[*J]&&S.FinalPhysicsObjectStates[*J]==static_cast<int32>(Chaos::EObjectStateType::Dynamic)) { ++E.ReleasedDynamic;E.MaximumDisplacement=FMath::Max(E.MaximumDisplacement,FVector::Distance(S.InitialPhysicsObjectPositions[I],S.FinalPhysicsObjectPositions[*J])); } }E.Complete&=Initial.Num()>0&&Initial.Num()==Final.Num()&&S.PhysicsIdsComplete;return E;
}

TSharedPtr<FJsonObject> TraceObservationResponse(const FString& Operation)
{
    if (!ActiveCinematicTraceObservation)
        return CommandError(TEXT("No active cinematic trace observation session"));
    const FCinematicTraceObservationSession& Session = *ActiveCinematicTraceObservation;
    const bool bTransformChanged = !Session.InitialTransformSha256.IsEmpty() &&
        Session.InitialTransformSha256 != Session.FinalTransformSha256;
    const bool bBreakOrReleaseObserved = bTransformChanged ||
        Session.InitialPhysicsObjectCount != Session.FinalPhysicsObjectCount ||
        Session.InitialDisabledCount != Session.FinalDisabledCount;
    TSharedPtr<FJsonObject> Data = Success(TEXT("trigger_trace_external_strain"));
    Data->SetStringField(TEXT("operation"), Operation);
    Data->SetStringField(TEXT("session_id"), Session.SessionId);
    Data->SetStringField(TEXT("prepared_trace_id"), Session.SessionId);
    Data->SetStringField(TEXT("prepared_actor_name"), Session.ActorName);
    Data->SetStringField(TEXT("prepared_actor_class_path"), Session.ActorClassPath);
    Data->SetStringField(TEXT("prepared_rest_collection_path"), Session.RestCollectionPath);
    Data->SetStringField(TEXT("prepared_world_path"), Session.PreparedWorldPath);
    Data->SetBoolField(TEXT("trace_prepared"), Session.bTracePrepared);
    Data->SetBoolField(TEXT("blocking_hit"), Session.bTracePrepared);
    Data->SetNumberField(TEXT("collision_item"), Session.CollisionItemIndex);
    Data->SetNumberField(TEXT("chosen_rigid_leaf_item"), Session.ChosenRigidLeafIndex);
    Data->SetNumberField(TEXT("strain_item"), Session.ItemIndex);
    Data->SetNumberField(TEXT("chosen_rigid_leaf_distance_cm"),
        Session.ChosenRigidLeafDistance);
    Data->SetNumberField(TEXT("observed_post_dispatch_tick_count"), Session.ObservedTickCount);
    Data->SetNumberField(TEXT("dispatch_frame"), static_cast<double>(Session.DispatchFrame));
    Data->SetNumberField(TEXT("release_observed_frame"), static_cast<double>(Session.ReleaseObservedFrame));
    Data->SetNumberField(TEXT("release_observed_frame_delta"),
        Session.ReleaseObservedFrame >= Session.DispatchFrame
            ? static_cast<double>(Session.ReleaseObservedFrame - Session.DispatchFrame) : -1.0);
    Data->SetNumberField(TEXT("breaking_velocity_dispatch_frame"),
        static_cast<double>(Session.BreakingVelocityDispatchFrame));
    Data->SetNumberField(TEXT("breaking_velocity_deferred_frame_delta"),
        Session.BreakingVelocityDispatchFrame >= Session.DispatchFrame
            ? static_cast<double>(Session.BreakingVelocityDispatchFrame - Session.DispatchFrame) : -1.0);
    Data->SetNumberField(TEXT("observed_target_transform_count"), Session.TargetTransformIndices.Num());
    Data->SetBoolField(TEXT("observation_ready"), Session.bReady);
    Data->SetBoolField(TEXT("timed_out"), Session.bTimedOut);
    Data->SetStringField(TEXT("initial_transform_sha256"), Session.InitialTransformSha256);
    Data->SetStringField(TEXT("final_transform_sha256"), Session.FinalTransformSha256);
    Data->SetBoolField(TEXT("transform_variation_observed"), bTransformChanged);
    Data->SetNumberField(TEXT("initial_physics_object_count"), Session.InitialPhysicsObjectCount);
    Data->SetNumberField(TEXT("final_physics_object_count"), Session.FinalPhysicsObjectCount);
    Data->SetNumberField(TEXT("initial_disabled_physics_object_count"), Session.InitialDisabledCount);
    Data->SetNumberField(TEXT("final_disabled_physics_object_count"), Session.FinalDisabledCount);
    Data->SetArrayField(TEXT("initial_physics_object_ids"), IntArrayJson(Session.InitialPhysicsObjectIds));
    Data->SetArrayField(TEXT("final_physics_object_ids"), IntArrayJson(Session.FinalPhysicsObjectIds));
    Data->SetArrayField(TEXT("initial_physics_object_states"), IntArrayJson(Session.InitialPhysicsObjectStates));
    Data->SetArrayField(TEXT("final_physics_object_states"), IntArrayJson(Session.FinalPhysicsObjectStates));
    Data->SetArrayField(TEXT("initial_physics_object_disabled"),
        BoolArrayJson(Session.InitialPhysicsObjectDisabled));
    Data->SetArrayField(TEXT("final_physics_object_disabled"),
        BoolArrayJson(Session.FinalPhysicsObjectDisabled));
    TArray<int32> ReleasedObjectArrayIndices;
    TArray<int32> ReleasedObjectIds;
    TArray<int32> ReleasedObjectStates;
    int32 ReleasedDynamicObjectCount = 0;
    double MaximumReleasedDynamicDisplacementCm = 0.0;
    for (int32 Index = 0; Index < Session.InitialPhysicsObjectDisabled.Num(); ++Index)
    {
        if (Session.InitialPhysicsObjectDisabled[Index] &&
            Session.FinalPhysicsObjectDisabled.IsValidIndex(Index) &&
            !Session.FinalPhysicsObjectDisabled[Index])
        {
            ReleasedObjectArrayIndices.Add(Index);
            ReleasedObjectIds.Add(Session.FinalPhysicsObjectIds.IsValidIndex(Index)
                ? Session.FinalPhysicsObjectIds[Index] : INDEX_NONE);
            ReleasedObjectStates.Add(Session.FinalPhysicsObjectStates.IsValidIndex(Index)
                ? Session.FinalPhysicsObjectStates[Index] : INDEX_NONE);
            if (Session.FinalPhysicsObjectStates.IsValidIndex(Index) &&
                Session.FinalPhysicsObjectStates[Index] ==
                    static_cast<int32>(Chaos::EObjectStateType::Dynamic) &&
                Session.InitialPhysicsObjectPositions.IsValidIndex(Index) &&
                Session.FinalPhysicsObjectPositions.IsValidIndex(Index))
            {
                ++ReleasedDynamicObjectCount;
                MaximumReleasedDynamicDisplacementCm = FMath::Max(
                    MaximumReleasedDynamicDisplacementCm,
                    FVector::Distance(
                        Session.InitialPhysicsObjectPositions[Index],
                        Session.FinalPhysicsObjectPositions[Index]));
            }
        }
    }
    Data->SetArrayField(TEXT("released_physics_object_array_indices"),
        IntArrayJson(ReleasedObjectArrayIndices));
    Data->SetArrayField(TEXT("released_physics_object_ids"), IntArrayJson(ReleasedObjectIds));
    Data->SetArrayField(TEXT("released_physics_object_states"), IntArrayJson(ReleasedObjectStates));
    Data->SetNumberField(TEXT("released_dynamic_physics_object_count"),
        ReleasedDynamicObjectCount);
    Data->SetNumberField(TEXT("maximum_released_dynamic_displacement_cm"),
        MaximumReleasedDynamicDisplacementCm);
    Data->SetBoolField(TEXT("released_dynamic_displacement_observed"),
        ReleasedDynamicObjectCount > 0 && MaximumReleasedDynamicDisplacementCm > 1.0);
    Data->SetBoolField(TEXT("break_or_release_observed"), bBreakOrReleaseObserved);
    Data->SetBoolField(TEXT("solver_release_observed_before_motion"), false);
    Data->SetBoolField(TEXT("breaking_velocity_armed_before_release"),
        Session.bBreakingVelocityArmedBeforeRelease);
    Data->SetBoolField(TEXT("solver_release_observed_after_velocity_arm"),
        Session.bReleaseObservedBeforeMotion);
    Data->SetStringField(TEXT("solver_release_observation_source"),
        TEXT("read_locked PhysicsObject disabled-state transition"));
    Data->SetNumberField(TEXT("maximum_actual_velocity_direction_dot"), Session.MaximumVelocityDirectionDot);
    Data->SetBoolField(TEXT("directional_velocity_observed"), Session.MaximumVelocityDirectionDot > 0.5);
    Data->SetNumberField(TEXT("external_strain_dispatch_count"),
        Session.bExternalStrainDispatched ? 1 : 0);
    Data->SetStringField(TEXT("trace_query_scope"), Session.TraceQueryScope);
    Data->SetNumberField(TEXT("trace_dispatch_count"), 1);
    Data->SetNumberField(TEXT("breaking_velocity_dispatch_count"), Session.bBreakingVelocityDispatched ? 1 : 0);
    Data->SetNumberField(TEXT("radial_impulse_dispatch_count"), Session.bRadialImpulseDispatched ? 1 : 0);
    Data->SetNumberField(TEXT("uniform_velocity_field_dispatch_count"),
        Session.UniformVelocityFieldDispatchCount);
    Data->SetNumberField(TEXT("localized_impulse_dispatch_count"),
        Session.LocalizedImpulseDispatchCount);
    Data->SetNumberField(TEXT("direct_velocity_apply_count"), 0);
    Data->SetNumberField(TEXT("retry_velocity_dispatch_count"), 0);
    Data->SetBoolField(TEXT("per_tick_result_manipulation"), false);
    const bool bPass = Session.bReady && !Session.bTimedOut && bBreakOrReleaseObserved &&
        bTransformChanged && ReleasedDynamicObjectCount > 0 &&
        MaximumReleasedDynamicDisplacementCm > 1.0 &&
        Session.bReleaseObservedBeforeMotion && Session.bBreakingVelocityDispatched &&
        Session.bBreakingVelocityArmedBeforeRelease &&
        Session.bRadialImpulseDispatched && Session.UniformVelocityFieldDispatchCount == 1 &&
        Session.LocalizedImpulseDispatchCount == 1;
    const FSameIdEvidence Identity=SameIdEvidence(Session);Data->SetBoolField(TEXT("same_physics_id_set_complete"),Identity.Complete);Data->SetNumberField(TEXT("same_id_released_dynamic_count"),Identity.ReleasedDynamic);Data->SetNumberField(TEXT("same_id_maximum_displacement_cm"),Identity.MaximumDisplacement);Work->R.Data->SetBoolField(TEXT("postcondition_identity_verified"),Identity.Complete);if(bPass&&(!Identity.Complete||Identity.ReleasedDynamic<=0||Identity.MaximumDisplacement<=1.0)) { Work->Fail(TEXT("physics_identity_postcondition"),TEXT("Legacy ordinal result does not establish same-id release/displacement")); }
    Data->SetBoolField(TEXT("requires_followup_sample"), !Session.bReady);
    Data->SetStringField(TEXT("assertion_status"), Session.bReady ? (bPass ? TEXT("PASS") : TEXT("FAIL")) : TEXT("BLOCKED"));
    Data->SetStringField(TEXT("learning_verdict"), bPass ? TEXT("PASS") : TEXT("BLOCKED"));
    return FLegacy::CreateSuccessResponse(Data);
}

bool DispatchPreparedTraceStrain(
    FCinematicTraceObservationSession& Session,
    FString& OutError)
{
    if (!Session.bTracePrepared || Session.bExternalStrainDispatched || !Session.Evidence || Session.Evidence->OperationComplete || !Session.Evidence->R.Error.IsEmpty())
    {
        OutError = TEXT("trace session is not prepared or External Strain was already dispatched");
        return false;
    }
    if(!Guard()) { OutError=Work->R.Error;return false; }UWorld* World = ResolveEditorWorld();
    if(World!=Work->World.Get()) { OutError=TEXT("Prepared world identity changed before dispatch");return false; }AActor* Actor = FindActor<AActor>(World, Session.ActorName);
    UGeometryCollectionComponent* Component = ResolveGeometryComponent(Actor);
    if (!World || !Actor || !Component)
    {
        OutError = TEXT("prepared trace actor/component could not be re-resolved for dispatch");
        return false;
    }
    if(!Guard()||Actor!=Session.Owner.Get()||Component!=Session.Component.Get()) { OutError=TEXT("Prepared actor/component identity changed before rest lookup");return false; }
    const UGeometryCollection* Rest = Component->GetRestCollection();
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> RestCollection =
        Rest ? Rest->GetGeometryCollection() : nullptr;
    if (!RestCollection.IsValid() || Session.ItemIndex < 0 ||
        Session.ItemIndex >= RestCollection->NumElements(FGeometryCollection::TransformGroup))
    {
        OutError = TEXT("prepared collision item is no longer valid in the same Rest Collection");
        return false;
    }
    if (Actor->GetClass()->GetPathName() != Session.ActorClassPath ||
        !Rest || Rest->GetPathName() != Session.RestCollectionPath)
    {
        OutError = TEXT("prepared actor class or Rest Collection identity changed before dispatch");
        return false;
    }
    if(World!=Work->World.Get()||Actor!=Session.Owner.Get()||Component!=Session.Component.Get()||!RestReadable(Component)) { OutError=TEXT("Prepared actor/component/world/rest identity changed before dispatch");return false; }
    Session.World = World;
    Session.Component = Component;
    Session.DispatchFrame = GFrameCounter;
    if(!Work->CapturePhysicsIdentity(Component)) { OutError=Work->R.Error;return false; }Session.InitialTransformSha256 = HashSingleDynamicCollection(Component);
    CaptureTracePhysicsObservation(Session);if(!Guard()||!Work->R.Error.IsEmpty()) { OutError=Work->R.Error;return false; }
    Session.InitialPhysicsObjectCount = Session.FinalPhysicsObjectCount;
    Session.InitialDisabledCount = Session.FinalDisabledCount;
    Session.InitialPhysicsObjectIds = Session.FinalPhysicsObjectIds;
    Session.InitialPhysicsObjectStates = Session.FinalPhysicsObjectStates;
    Session.InitialPhysicsObjectDisabled = Session.FinalPhysicsObjectDisabled;
    Session.InitialPhysicsObjectPositions = Session.FinalPhysicsObjectPositions;
    Session.MaximumVelocityDirectionDot = -1.0;
    Session.MonotonicDispatchStart=FPlatformTime::Seconds();if(!Step(TEXT("ApplyExternalStrain"),[&]{Component->ApplyExternalStrain(
        Session.ItemIndex,Session.ImpactPoint,Session.Radius,Session.PropagationDepth,Session.PropagationFactor,Session.Strain);Session.bExternalStrainDispatched=true;})) { OutError=Work->R.Error;return false; }
    if(GFrameCounter!=Session.DispatchFrame) { OutError=TEXT("Game frame changed after strain; breaking velocity was not automatically retried");return Work->Fail(TEXT("dispatch_frame_changed"),OutError); }
    // These two proxy commands must be enqueued in this order on the same
    // game tick. ApplyBreakingLinearVelocity arms direct children whose
    // ExternalStrain will break; after the parent is released/disabled the
    // same call is intentionally a no-op in UE 5.6.
    if(!Step(TEXT("ApplyBreakingLinearVelocity"),[&]{Component->ApplyBreakingLinearVelocity(Session.ItemIndex,Session.RequestedVelocity);Session.bBreakingVelocityDispatched=true;Session.bBreakingVelocityArmedBeforeRelease=true;Session.BreakingVelocityDispatchFrame=GFrameCounter;})) { OutError=Work->R.Error;return false; }
    if(Session.BreakingVelocityDispatchFrame!=Session.DispatchFrame) { OutError=TEXT("Game frame changed during breaking velocity dispatch; no ticker or retry was started");return Work->Fail(TEXT("dispatch_frame_changed"),OutError); }
    Work->Event(TEXT("register_trace_ticker"),false);Session.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([](float)
        {
            return TickCinematicTraceObservation(0.0f);
        }));
    return true;
}

TSharedPtr<FJsonObject> HandleTraceStrain(const TSharedPtr<FJsonObject>& Params)
{
    const FString Operation = GetString(Params, TEXT("operation"), TEXT("begin"));
    if (Operation == TEXT("dispatch") || Operation == TEXT("dispatch_prepared"))
    {
        if (!ActiveCinematicTraceObservation)
        {
            return CommandError(TEXT("No prepared cinematic trace observation session"));
        }
        const FString PreparedTraceId = GetString(Params, TEXT("prepared_trace_id"));
        if (PreparedTraceId.IsEmpty() ||
            PreparedTraceId != ActiveCinematicTraceObservation->SessionId)
        {
            return CommandError(TEXT("dispatch_prepared requires the exact server-issued prepared_trace_id"));
        }
        FString DispatchError;
        if (!DispatchPreparedTraceStrain(*ActiveCinematicTraceObservation, DispatchError))
        {
            return CommandError(DispatchError);
        }
        return TraceObservationResponse(Operation);
    }
    if (Operation == TEXT("sample") || Operation == TEXT("finish"))
    {
        TSharedPtr<FJsonObject> Response = TraceObservationResponse(Operation);
        if (Operation == TEXT("finish") && ActiveCinematicTraceObservation && ActiveCinematicTraceObservation->bReady)
        {
            CleanupCinematicTraceObservation();
            ActiveCinematicTraceObservation.Reset();
        }
        return Response;
    }
    if (Operation == TEXT("abort"))
    {
        CleanupCinematicTraceObservation(TEXT("aborted"));if(LastSessionExecution) { Work->AdoptedExecution=LastSessionExecution; }Work->OperationComplete=true;ActiveCinematicTraceObservation.Reset();
        TSharedPtr<FJsonObject> Data = Success(TEXT("trigger_trace_external_strain"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetBoolField(TEXT("cleanup_complete"), true);
        return FLegacy::CreateSuccessResponse(Data);
    }
    CleanupCinematicTraceObservation(TEXT("replaced_by_new_request"));
    ActiveCinematicTraceObservation.Reset();
    Work->SDK(TEXT("resolve_trace_targets"));UWorld* World = ResolveEditorWorld();if(!Live(World)||World->bIsTearingDown) { return CommandError(TEXT("trigger_trace_external_strain could not resolve the editor/PIE world")); }Work->World=World;Work->WorldCaptured=true;if(!Work->Track(World,TEXT("trace_world"))) { return CommandError(Work->R.Error); }
    const FString ActorName = GetString(Params, TEXT("actor_name"));
    AActor* Actor = FindActor<AActor>(World, ActorName);if(Actor&&!Work->Track(Actor,TEXT("trace_actor"))) { return CommandError(Work->R.Error); }
    UGeometryCollectionComponent* Component = ResolveGeometryComponent(Actor);if(!Guard()) { return CommandError(Work->R.Error); }
    if (Actor && !Component)
    {
        Work->SDK(TEXT("read_active_record_component_fallback"));Component=Cubeless::UE58::ChaosCacheSessions::FindActiveRecordedComponent(Actor,World).Get();if(!Guard()) { return CommandError(Work->R.Error); }
    }
    FVector Start;
    FVector End;
    if (!World)
        return CommandError(TEXT("trigger_trace_external_strain could not resolve the editor/PIE world"));
    if (!Actor)
    {
        TArray<FString> GeometryActors;
        for (TActorIterator<AGeometryCollectionActor> It(World); It; ++It)
            GeometryActors.Add(FString::Printf(TEXT("%s|%s"), *It->GetName(), *It->GetActorLabel()));
        return CommandError(FString::Printf(
            TEXT("trigger_trace_external_strain actor '%s' was not found in world '%s'; Geometry Collection actors: [%s]"),
            *ActorName, *World->GetPathName(), *FString::Join(GeometryActors, TEXT(", "))));
    }
    if (!Component)
        return CommandError(FString::Printf(
            TEXT("trigger_trace_external_strain actor '%s' (%s) has no Geometry Collection component in world '%s'"),
            *Actor->GetActorLabel(), *Actor->GetClass()->GetPathName(), *World->GetPathName()));
    Work->World=World;Work->WorldCaptured=true;if(!Work->Track(World,TEXT("trace_world"))||!Work->Track(Actor,TEXT("trace_actor"))||!Work->Track(Component,TEXT("trace_component"),Actor)) { return CommandError(Work->R.Error); }
    if (!TryVector(Params, TEXT("start"), Start))
        return CommandError(TEXT("trigger_trace_external_strain start must be exactly three finite numbers"));
    if (!TryVector(Params, TEXT("end"), End))
        return CommandError(TEXT("trigger_trace_external_strain end must be exactly three finite numbers"));
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MCPCinematicTraceStrain),
        GetBool(Params, TEXT("trace_complex"), true));
    FHitResult Hit;
    const bool bPrepareOnly = Operation == TEXT("prepare");
    bool bHit=false;if(!Step(TEXT("trace_query"),[&]{bHit=bPrepareOnly?World->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,QueryParams):Component->LineTraceComponent(Hit,Start,End,QueryParams);},false)) { return CommandError(Work->R.Error); }
    UGeometryCollectionComponent* HitGeometryComponent =
        bHit ? Cast<UGeometryCollectionComponent>(Hit.GetComponent()) : nullptr;
    const bool bExpectedHit = GetBool(Params, TEXT("expected_hit"), true);
    if (!bExpectedHit && !bHit)
    {
        TSharedPtr<FJsonObject> Data = Success(TEXT("trigger_trace_external_strain"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetBoolField(TEXT("blocking_hit"), false);
        Data->SetNumberField(TEXT("external_strain_dispatch_count"), 0);
        Data->SetNumberField(TEXT("radial_impulse_dispatch_count"), 0);
        Data->SetNumberField(TEXT("uniform_velocity_field_dispatch_count"), 0);
        Data->SetNumberField(TEXT("localized_impulse_dispatch_count"), 0);
        Data->SetNumberField(TEXT("released_dynamic_physics_object_count"), 0);
        Data->SetNumberField(TEXT("maximum_released_dynamic_displacement_cm"), 0.0);
        Data->SetBoolField(TEXT("released_dynamic_displacement_observed"), false);
        Data->SetNumberField(TEXT("retry_velocity_dispatch_count"), 0);
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (!bExpectedHit || !bHit || HitGeometryComponent != Component ||
        Component->GetOwner() != Actor || Hit.Item == INDEX_NONE)
    {
        return CommandError(FString::Printf(
            TEXT("Trace did not hit requested actor '%s' as a Geometry Collection item; blocking_hit=%s hit_actor_label='%s' hit_actor_class='%s' hit_component_path='%s' hit_component_class='%s' requested_component_path='%s' hit_item=%d"),
            *ActorName,
            bHit ? TEXT("true") : TEXT("false"),
            Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("<null>"),
            Hit.GetActor() ? *Hit.GetActor()->GetClass()->GetPathName() : TEXT("<null>"),
            Hit.GetComponent() ? *Hit.GetComponent()->GetPathName() : TEXT("<null>"),
            Hit.GetComponent() ? *Hit.GetComponent()->GetClass()->GetPathName() : TEXT("<null>"),
            Component ? *Component->GetPathName() : TEXT("<null>"),
            Hit.Item));
    }
    if(Hit.ImpactPoint.ContainsNaN()) { return CommandError(TEXT("Trace impact point is not finite")); }
    Component = HitGeometryComponent;
    const float Radius = CastFloat(GetNumber(Params,TEXT("radius"),100.0),TEXT("radius"));
    const float Strain = CastFloat(GetNumber(Params,TEXT("strain"),100000.0),TEXT("strain"));
    const int32 PropagationDepth = CastI32(GetNumber(Params,TEXT("propagation_depth"),1),TEXT("propagation_depth"));
    const float PropagationFactor = CastFloat(GetNumber(Params,TEXT("propagation_factor"),0.5),TEXT("propagation_factor"));
    const double BreakingVelocityMagnitude = GetNumber(Params, TEXT("breaking_velocity"), 800.0);
    const FVector Velocity = (End - Start).GetSafeNormal() * BreakingVelocityMagnitude;
    CastFloat(BreakingVelocityMagnitude,TEXT("breaking_velocity"));CastFloat(Velocity.X*10000,TEXT("velocity.x impulse"));CastFloat(Velocity.Y*10000,TEXT("velocity.y impulse"));CastFloat(Velocity.Z*10000,TEXT("velocity.z impulse"));if(!Work->R.Error.IsEmpty()||!RestReadable(Component)) { return CommandError(Work->R.Error); }
    Work->Event(TEXT("create_trace_session"),false);ActiveCinematicTraceObservation = MakeShared<FCinematicTraceObservationSession>();
    FCinematicTraceObservationSession& Session = *ActiveCinematicTraceObservation;
    Session.Evidence=CallOwner;Session.Owner=Actor;Work->OperationComplete=false;Session.SessionId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);Work->SessionId=Session.SessionId;Work->R.Data->SetStringField(TEXT("phase"),TEXT("prepared"));
    Session.ActorName = ActorName;
    Session.ActorClassPath = Actor->GetClass()->GetPathName();
    Session.PreparedWorldPath = World->GetPathName();
    Session.TraceQueryScope = bPrepareOnly
        ? TEXT("world Visibility channel")
        : TEXT("requested Geometry Collection component");
    Session.World = World;
    Session.Component = Component;
    Session.CollisionItemIndex = Hit.Item;
    Session.ItemIndex = ResolveLocalizedTraceClusterItem(
        Component, Hit.Item, Hit.ImpactPoint,
        Session.ChosenRigidLeafIndex, Session.ChosenRigidLeafDistance);
    Session.RestCollectionPath = Component->GetRestCollection()
        ? Component->GetRestCollection()->GetPathName() : FString();
    if (const UGeometryCollection* Rest = Component->GetRestCollection())
    {
        const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> RestCollection = Rest->GetGeometryCollection();
        if (RestCollection.IsValid() && RestCollection->Children.IsValidIndex(Session.ItemIndex))
        {
            TArray<int32> Pending = {Session.ItemIndex};
            while (!Pending.IsEmpty())
            {
                const int32 TransformIndex = Pending.Pop(EAllowShrinking::No);
                Session.TargetTransformIndices.AddUnique(TransformIndex);
                if (RestCollection->Children.IsValidIndex(TransformIndex))
                    for (int32 Child : RestCollection->Children[TransformIndex]) Pending.Add(Child);
            }
        }
    }
    if (Session.TargetTransformIndices.IsEmpty()) Session.TargetTransformIndices.Add(Session.ItemIndex);
    Session.RequestedVelocity = Velocity;
    Session.ImpactPoint = Hit.ImpactPoint;
    Session.Radius = Radius;
    Session.Strain = Strain;
    Session.PropagationDepth = PropagationDepth;
    Session.PropagationFactor = PropagationFactor;
    Session.RadialImpulseStrength = CastFloat(BreakingVelocityMagnitude,TEXT("breaking_velocity"));
    Session.bTracePrepared = true;
    if (Operation == TEXT("prepare"))
    {
        return TraceObservationResponse(Operation);
    }
    FString DispatchError;
    if (!DispatchPreparedTraceStrain(Session, DispatchError))
    {
        CleanupCinematicTraceObservation(TEXT("failed_dispatch_cleanup"));ActiveCinematicTraceObservation.Reset();
        return CommandError(DispatchError);
    }
    return TraceObservationResponse(Operation);
}

bool TickCinematicTraceObservation(float Delta)
{
    auto Hold=ActiveCinematicTraceObservation;if(!Hold) { return false; }if(Running) { return !ShuttingDown; }auto E=Hold->Evidence;if(!E) { return false; }TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,E.Get());FExecutionPins Pins(*E);bool Keep=false;
    if(Pins.Acquire()) { if(Hold->MonotonicDispatchStart>0&&FPlatformTime::Seconds()-Hold->MonotonicDispatchStart>=30.0) { Hold->bTimedOut=true;Work->R.Data->SetBoolField(TEXT("wall_timeout"),true);Work->Fail(TEXT("wall_timeout"),TEXT("Trace session exceeded the monotonic 30 second safety deadline")); }else { Keep=TickCinematicTraceObservationBody(Delta); } }
    if(!Work->R.Error.IsEmpty()||!Work->Guard()) { Keep=false; }
    if(!Keep)
    {
        if(Hold->bTimedOut&&Work->R.Error.IsEmpty()) { Work->Fail(TEXT("trace_timeout"),TEXT("Trace observation did not complete within the original 60 ticks")); }
        if(Hold->bReady&&Work->R.Error.IsEmpty()) { auto Response=TraceObservationResponse(TEXT("sample"));const TSharedPtr<FJsonObject>* Data=nullptr;FString A;if(Response->TryGetObjectField(TEXT("data"),Data)&&Data&&Data->IsValid()&&(*Data)->TryGetStringField(TEXT("assertion_status"),A)&&A.Equals(TEXT("FAIL"),ESearchCase::CaseSensitive)) { Work->Fail(TEXT("assertion_failed"),TEXT("Trace terminal release/displacement gate failed")); } }
        Work->R.Data->SetBoolField(TEXT("session_postcondition_passed"),Hold->bReady&&!Hold->bTimedOut&&Work->R.Error.IsEmpty());CleanupCinematicTraceObservation(ShuttingDown?TEXT("cancelled_on_shutdown"):TEXT("terminal_ticker_stopped"));Work->OperationComplete=true;
    }
    Work->R.Data->SetBoolField(TEXT("timed_out"),Hold->bTimedOut);Work->R.Data->SetNumberField(TEXT("last_callback_frame"),static_cast<double>(GFrameCounter));Work->R.Data->SetStringField(TEXT("phase"),Keep?TEXT("observing"):(Work->R.Error.IsEmpty()?TEXT("terminal_complete"):TEXT("terminal_failed")));LastSessionExecution=Work->SessionSnapshot();return Keep;
}
}
FString Trigger(const FString& ParamsJson)
{
    auto Fresh=MakeShared<FWork>();TSharedPtr<FJsonObject> P;if(!Parse(ParamsJson,P)) { Fresh->Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return Fresh->Finish(nullptr); }
    if(!IsInGameThread()||Running||ShuttingDown) { Fresh->Fail(TEXT("editor_required"),TEXT("Non-reentrant Editor game-thread execution required"));return Fresh->Finish(nullptr); }
    const FString Op=GetString(P,TEXT("operation"),TEXT("begin"));auto Hold=ActiveCinematicTraceObservation;const bool Dispatch=Op==TEXT("dispatch")||Op==TEXT("dispatch_prepared");
    const bool ValidPrepared=Dispatch&&Hold&&Hold->Evidence&&!Hold->Evidence->OperationComplete&&Hold->Evidence->R.Error.IsEmpty()&&Hold->bTracePrepared&&!Hold->bExternalStrainDispatched&&GetString(P,TEXT("prepared_trace_id"))==Hold->SessionId;
    const bool Existing=Hold&&Hold->Evidence&&(Op==TEXT("sample")||Op==TEXT("finish")||ValidPrepared);auto W=Existing?Hold->Evidence:Fresh;
    TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,W.Get());TGuardValue<TSharedPtr<FWork>> Owner(CallOwner,W);TGuardValue<bool> WriterAdmission(W->RequiresForeignWriterAdmission,Op!=TEXT("sample")&&Op!=TEXT("finish")&&Op!=TEXT("abort"));FExecutionPins Pins(*W);W->StartCall();if(LastSessionExecution) { W->R.Data->SetObjectField(TEXT("last_session_execution_before_request"),LastSessionExecution); }
    if(Op!=TEXT("abort")&&!Pins.Acquire()) { CleanupCinematicTraceObservation(TEXT("unsafe_context_ticker_stopped"));return W->Finish(nullptr); }
    auto Legacy=HandleTraceStrain(P);if(!W->R.Error.IsEmpty()&&ActiveCinematicTraceObservation&&ActiveCinematicTraceObservation->Evidence==W) { CleanupCinematicTraceObservation(TEXT("failed_request_ticker_stopped")); }if(Dispatch&&!ValidPrepared&&Hold&&Hold->Evidence) { W->AdoptedExecution=Hold->Evidence->SessionSnapshot();W->SessionId=Hold->SessionId; }if(ActiveCinematicTraceObservation&&ActiveCinematicTraceObservation->Evidence) { LastSessionExecution=ActiveCinematicTraceObservation->Evidence->SessionSnapshot(); }else if(W->AdoptedExecution) { LastSessionExecution=W->AdoptedExecution; }else if(!W->SessionId.IsEmpty()) { LastSessionExecution=W->SessionSnapshot(); }return W->Finish(Legacy);
}
bool HasActiveSessionForWorld(const UWorld* World)
{
    if(!IsInGameThread()) { return true; }if(!World) { return false; }
    if(Running&&Work&&Work->World.Get()==World) { return true; }
    const auto Hold=ActiveCinematicTraceObservation;return Hold&&Hold->World.Get()==World&&Hold->Evidence&&!Hold->Evidence->OperationComplete;
}

void Shutdown()
{
    if(!IsInGameThread()) { UE_LOG(LogTemp,Error,TEXT("Cubeless trace shutdown requires the Editor game thread; no UObject/physics cleanup was attempted"));return; }
    ShuttingDown=true;auto Hold=ActiveCinematicTraceObservation;if(!Hold) { return; }
    if(Hold->TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Hold->TickerHandle);Hold->TickerHandle.Reset(); }
    if(Hold->Evidence) { Hold->Evidence->RemovePhysicsGuards(); }if(Running) { if(Hold->Evidence) { Hold->Evidence->CancelRequested=true; }UE_LOG(LogTemp,Warning,TEXT("Cubeless trace shutdown was reentrant; the active scope retains session lifetime. Reentrant DLL unload is unsupported."));return; }
    TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,Hold->Evidence.Get());if(Work) { const bool Pending=!Work->OperationComplete;CleanupCinematicTraceObservation(Pending?TEXT("cancelled_on_shutdown"):TEXT("terminal_shutdown_release"));if(Pending&&Work->R.Error.IsEmpty()) { Work->Fail(TEXT("module_shutdown"),TEXT("Pending trace session was cancelled; already sent physics commands were not rolled back")); }LastSessionExecution=Work->SessionSnapshot();UE_LOG(LogTemp,Display,TEXT("Cubeless trace shutdown receipt: %s"),*Json(LastSessionExecution)); }ActiveCinematicTraceObservation.Reset();
}
}
