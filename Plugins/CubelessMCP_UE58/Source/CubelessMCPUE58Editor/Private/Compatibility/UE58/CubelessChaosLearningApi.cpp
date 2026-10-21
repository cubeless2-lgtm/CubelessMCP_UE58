#include "Compatibility/UE58/CubelessChaosLearningApi.h"
#include "Compatibility/UE58/CubelessChaosDestructionPerformanceApi.h"
#include "Compatibility/UE58/CubelessChaosCacheSessionApi.h"
#include "Compatibility/UE58/CubelessChaosTraceStrainApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/CacheCollection.h"
#include "Chaos/CacheManagerActor.h"
#include "Chaos/ChaosCache.h"
#include "Chaos/ChaosSolverActor.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Dataflow/DataflowSelection.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "FileHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FractureEngineUtility.h"
#include "GeometryCollection/Facades/CollectionRemoveOnBreakFacade.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionRootProxyRenderer.h"
#include "GeometryCollectionProxyData.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include <cmath>
#include <atomic>

namespace Cubeless::UE58::ChaosLearning
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false,ShuttingDown=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
UWorld* CurrentWorld(){if(!GEditor)return nullptr;if(auto* C=GEditor->GetPIEWorldContext()){if(C->World())return C->World();}return GEditor->GetEditorWorldContext().World();}
bool SafeEditor(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!ShuttingDown;}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
bool Forbidden(const FString& S){for(const TCHAR* K:{TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("cubelessmcp"),TEXT("cubeless_mcp"),TEXT("modelcontextprotocol"),TEXT("toolsetregistry"),TEXT("/Engine/PythonTypes")})if(S.Contains(K,ESearchCase::IgnoreCase))return true;return false;}
bool ForbiddenClass(const UClass* C){for(;C;C=C->GetSuperClass())if(Forbidden(C->GetPathName()))return true;return false;}
struct FExactKeys:DefaultKeyFuncs<FString>{static bool Matches(const FString&A,const FString&B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString&S){return FCrc::StrCrc32(*S);}};
bool Parse(const FString& S,TSharedPtr<FJsonObject>& O)
{
 if(S.IsEmpty()||S.Len()>1048576)return false;for(TCHAR C:S)if(C==0)return false;auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeys>> Keys;EJsonNotation Token;int32 Visits=0;
 while(Reader->ReadNext(Token)){if(Token==EJsonNotation::Error||++Visits>262144)return false;if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()){const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K))return false;Keys.Last().Add(K);}if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart){Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>64)return false;}else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd){if(Objects.IsEmpty())return false;Objects.Pop();Keys.Pop();}if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber()))return false;}
 return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O)&&O.IsValid();
}
struct FTarget
{
 TWeakObjectPtr<const UObject> Weak,Outer,Parent;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package;TStrongObjectPtr<const UObject> Pin{nullptr};FString Path,PackagePath,Role,Before;bool HasParent=false,Created=false;
};
struct FPhysicsTicket{const UActorComponent* Target;std::atomic<uint64> Revision{0};explicit FPhysicsTicket(const UActorComponent* In):Target(In){}};
struct FPhysicsIdentity
{
 TWeakObjectPtr<UGeometryCollectionComponent> Component;TSharedPtr<FPhysicsTicket,ESPMode::ThreadSafe> Ticket;FDelegateHandle Create,Destroy;const void* Proxy=nullptr;const void* Dynamic=nullptr;bool Registered=false,Created=false;
};
TSharedPtr<FJsonObject> ObjectProjection(const UObject* O)
{
 auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("live"),Live(O));if(!Live(O))return J;J->SetStringField(TEXT("path"),O->GetPathName());J->SetStringField(TEXT("class"),O->GetClass()->GetPathName());J->SetStringField(TEXT("outer"),O->GetOuter()?O->GetOuter()->GetPathName():FString());J->SetStringField(TEXT("package"),O->GetPackage()->GetName());J->SetBoolField(TEXT("dirty"),O->GetPackage()->IsDirty());
 if(auto* A=Cast<AActor>(O)){J->SetStringField(TEXT("transform"),A->GetActorTransform().ToString());J->SetStringField(TEXT("label"),A->GetActorLabel());J->SetNumberField(TEXT("component_count"),A->GetInstanceComponents().Num());}
 if(auto* C=Cast<UGeometryCollectionComponent>(O)){J->SetStringField(TEXT("rest"),C->GetRestCollection()?C->GetRestCollection()->GetPathName():FString());J->SetBoolField(TEXT("simulate"),C->BodyInstance.bSimulatePhysics);J->SetBoolField(TEXT("notify"),C->bNotifyBreaks);J->SetBoolField(TEXT("registered"),C->IsRegistered());J->SetNumberField(TEXT("one_way"),C->OneWayInteractionLevel);J->SetBoolField(TEXT("sleep"),C->bAllowRemovalOnSleep);J->SetBoolField(TEXT("break"),C->bAllowRemovalOnBreak);}
 if(auto* M=Cast<UStaticMesh>(O)){J->SetStringField(TEXT("lighting_guid"),M->GetLightingGuid().ToString());J->SetNumberField(TEXT("source_model_count"),M->GetNumSourceModels());J->SetStringField(TEXT("bounding_box"),M->GetBoundingBox().ToString());TArray<TSharedPtr<FJsonValue>>Materials;for(int32 I=0;I<M->GetStaticMaterials().Num();++I){const auto*Material=M->GetMaterial(I);Materials.Add(MakeShared<FJsonValueString>(Material?Material->GetPathName():FString()));}J->SetArrayField(TEXT("material_paths"),Materials);}
 if(auto* G=Cast<UGeometryCollection>(O)){const auto D=G->GetGeometryCollection();J->SetNumberField(TEXT("transforms"),D?D->NumElements(FGeometryCollection::TransformGroup):0);J->SetNumberField(TEXT("geometries"),D?D->NumElements(FGeometryCollection::GeometryGroup):0);J->SetBoolField(TEXT("nanite"),G->EnableNanite);J->SetNumberField(TEXT("proxy_meshes"),G->RootProxyData.ProxyMeshes.Num());J->SetBoolField(TEXT("remove_sleep"),G->bRemoveOnMaxSleep);}
 if(auto* M=Cast<AChaosCacheManager>(O)){J->SetStringField(TEXT("cache"),M->CacheCollection?M->CacheCollection->GetPathName():FString());J->SetNumberField(TEXT("mode"),static_cast<int32>(M->CacheMode));J->SetNumberField(TEXT("start_mode"),static_cast<int32>(M->StartMode));J->SetNumberField(TEXT("observed"),M->GetObservedComponents().Num());}
 if(auto* C=Cast<UChaosCacheCollection>(O)){J->SetNumberField(TEXT("caches"),C->GetCaches().Num());J->SetNumberField(TEXT("frames"),C->GetMaxNumFrames());const float Duration=C->GetMaxDuration();J->SetBoolField(TEXT("duration_is_finite"),FMath::IsFinite(Duration));if(FMath::IsFinite(Duration))J->SetNumberField(TEXT("duration"),Duration);}
 return J;
}
TWeakObjectPtr<AChaosCacheManager> LastOwnedManager;
TWeakObjectPtr<UWorld> LastOwnedWorld;
TWeakObjectPtr<UChaosCacheCollection> LastOwnedCollection;
ECacheMode LastOwnedMode=ECacheMode::None;
bool OwnsEvaluation=false;
void RetireExpiredWorldMetadata();
struct FWork
{
 TWeakObjectPtr<AChaosCacheManager> ExpectedManager;TWeakObjectPtr<UChaosCacheCollection> ExpectedCollection;ECacheMode ExpectedMode=ECacheMode::None;EStartMode ExpectedStart=EStartMode::Triggered;bool ManagerCaptured=false;
 FResult R;FString Operation,SessionId,FailedStage;bool Complete=true,Cancel=false,WorldCaptured=false,Writer=false;TWeakObjectPtr<UWorld> World;TArray<FTarget> Targets;TArray<FPhysicsIdentity> Physics;TArray<TSharedPtr<FJsonValue>> Events,Saves;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();uint64 Serial=0,Changed=0,CallSerial=0,CallChanged=0,CallSaves=0;TSharedPtr<FJsonObject> Adopted;
 explicit FWork(const TCHAR* Op):Operation(Op){R.Stage=TEXT("validate_request");R.Compiled=false;R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("physics_commands_completed_verified"),false);R.Data->SetBoolField(TEXT("explicit_wrapper_wait_performed"),false);}
 ~FWork(){if(IsInGameThread())ReleasePhysics();else if(!Physics.IsEmpty())UE_LOG(LogTemp,Error,TEXT("Learning physics handles survived off-thread destruction; unsupported unload boundary"));}
 void Count(const TCHAR* K){double N=0;Counts->TryGetNumberField(K,N);Counts->SetNumberField(K,N+1);}
 bool Fail(const TCHAR* Code,const FString& E){if(R.Error.IsEmpty()){R.ErrorCode=TEXT("chaos_learning_")+FString(Code);R.Error=E;FailedStage=R.Stage;}return false;}
 void SDK(const TCHAR* S){R.Stage=S;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_effects_unknown"),true);}
 void Event(const TCHAR* S,bool Engine=true){++Serial;R.bMutationAttempted=true;if(Engine){SDK(S);}else{++Changed;R.bChanged=true;R.Stage=S;}auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),Serial);J->SetStringField(TEXT("stage"),S);J->SetStringField(TEXT("scope"),Engine?TEXT("engine_attempt"):TEXT("session_control"));if(Events.Num()<65536)Events.Add(MakeShared<FJsonValueObject>(J));else R.Data->SetBoolField(TEXT("event_prefix_truncated"),true);}
 bool Guard(bool IgnoreCancel=false)
 {
  if(!SafeEditor()||(!IgnoreCancel&&Cancel))return Fail(TEXT("editor_lifetime_lost"),TEXT("Safe Editor lifetime/game thread unavailable or cancellation requested"));
  if(WorldCaptured&&(!Live(World.Get())||World->bIsTearingDown||CurrentWorld()!=World.Get()))return Fail(TEXT("world_lost"),TEXT("Captured Editor/PIE world was replaced or is tearing down"));
  if(Writer&&WorldCaptured&&OwnsEvaluation&&LastOwnedWorld.Get()!=World.Get())return Fail(TEXT("other_world_evaluation_lease"),TEXT("Another live or unresolved GA world retains evaluation ownership"));
  if(Writer&&WorldCaptured&&(ChaosCacheSessions::HasActiveSessionForWorld(World.Get())||ChaosTraceStrain::HasActiveSessionForWorld(World.Get())))return Fail(TEXT("foreign_session_conflict"),TEXT("A pending FW/FZ session owns resources in this world"));
  if(Writer&&WorldCaptured&&Cubeless::UE58::ChaosDestructionPerformance::HasActiveSessionForWorld(World.Get()))return Fail(TEXT("performance_session_conflict"),TEXT("A pending GB performance session owns resources in this world"));
  for(const auto&T:Targets){auto* O=T.Weak.Get();if(!Live(O)||!Live(T.Package.Get())||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)||!T.Package->GetName().Equals(T.PackagePath,ESearchCase::CaseSensitive)||(T.HasParent&&(!Live(T.Parent.Get())||!O->IsIn(T.Parent.Get()))))return Fail(TEXT("target_identity_lost"),TEXT("Captured object class/outer/package/path/owner changed"));if((O->IsA<AActor>()||O->IsA<UActorComponent>())&&WorldCaptured&&O->GetWorld()!=World.Get())return Fail(TEXT("target_world_lost"),TEXT("Captured target moved out of the owned world"));}
  if(ManagerCaptured&&(!Live(ExpectedManager.Get())||ExpectedManager->CacheCollection.Get()!=ExpectedCollection.Get()||ExpectedManager->CacheMode!=ExpectedMode||ExpectedManager->StartMode!=ExpectedStart))return Fail(TEXT("manager_control_changed"),TEXT("Captured manager collection/mode/start changed outside this operation"));
  for(const auto&P:Physics){auto*C=P.Component.Get();if(!Live(C)||!P.Ticket||P.Ticket->Revision.load(std::memory_order_acquire)!=0||C->GetPhysicsProxy()!=P.Proxy||C->GetDynamicCollection()!=P.Dynamic||C->IsRegistered()!=P.Registered||C->IsPhysicsStateCreated()!=P.Created)return Fail(TEXT("physics_identity_lost"),TEXT("Captured physics lifecycle/proxy/dynamic state changed; no replay performed"));}
  return true;
 }
 bool Track(const UObject* O,const TCHAR* Role,const UObject* Parent=nullptr,bool Created=false)
 {
  if(!SafeEditor()||!Live(O)||!Live(O->GetPackage())||ForbiddenClass(O->GetClass()))return Fail(TEXT("target_unavailable"),TEXT("Live captured UObject required"));for(auto&T:Targets)if(T.Weak.Get()==O)return Guard();if(Targets.Num()>=8192)return Fail(TEXT("object_budget"),TEXT("Captured object budget exceeded"));auto&T=Targets.AddDefaulted_GetRef();T.Weak=O;T.Pin.Reset(O);T.Outer=O->GetOuter();T.Parent=Parent;T.HasParent=Parent!=nullptr;T.Class=O->GetClass();T.Package=O->GetPackage();T.Path=O->GetPathName();T.PackagePath=O->GetPackage()->GetName();T.Role=Role;T.Created=Created;SDK(TEXT("capture_object_projection"));T.Before=Json(ObjectProjection(O));if(Created){++Changed;R.bChanged=true;}return Guard();
 }
 bool CaptureWorld(UWorld* W){if(!Live(W)||W->bIsTearingDown)return Fail(TEXT("world_missing"),TEXT("Live current world required"));World=W;WorldCaptured=true;return Track(W,TEXT("world"));}
 template<typename T>T* Load(const FString& P,const TCHAR* S){if(!Guard()||!R.Error.IsEmpty())return nullptr;SDK(S);Count(S);T* O=LoadObject<T>(nullptr,*P);if(!Guard())return nullptr;if(O&&!Track(O,S))return nullptr;return O;}
 void ExpectManager(AChaosCacheManager*M,UChaosCacheCollection*C,ECacheMode Mode,EStartMode Start){ExpectedManager=M;ExpectedCollection=C;ExpectedMode=Mode;ExpectedStart=Start;ManagerCaptured=true;}
 void ReturnedObject(const TCHAR* Stage,const FString& Requested,const UObject* Value){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetStringField(TEXT("requested_identity"),Requested);J->SetBoolField(TEXT("sdk_returned_nonnull"),Value!=nullptr);J->SetStringField(TEXT("returned_pointer"),FString::Printf(TEXT("%p"),static_cast<const void*>(Value)));J->SetBoolField(TEXT("live_identity_verified"),false);TArray<TSharedPtr<FJsonValue>>Rows;const TArray<TSharedPtr<FJsonValue>>*Existing=nullptr;if(R.Data->TryGetArrayField(TEXT("object_creation_returns"),Existing)&&Existing)Rows=*Existing;Rows.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("object_creation_returns"),Rows);if(Value){++Changed;R.bChanged=true;}}
 void ReleasePhysics()
 {
  for(auto&P:Physics){if(P.Create.IsValid())UActorComponent::GlobalCreatePhysicsDelegate.Remove(P.Create);if(P.Destroy.IsValid())UActorComponent::GlobalDestroyPhysicsDelegate.Remove(P.Destroy);}Physics.Reset();
 }
 bool CapturePhysics(UGeometryCollectionComponent*C)
 {
  if(!Guard()||!Live(C))return false;for(const auto&P:Physics)if(P.Component.Get()==C)return Guard();auto&P=Physics.AddDefaulted_GetRef();P.Component=C;P.Ticket=MakeShared<FPhysicsTicket,ESPMode::ThreadSafe>(C);auto Ticket=P.Ticket;
  P.Create=UActorComponent::GlobalCreatePhysicsDelegate.AddLambda([Ticket](UActorComponent* V){if(V==Ticket->Target)Ticket->Revision.fetch_add(1,std::memory_order_release);});P.Destroy=UActorComponent::GlobalDestroyPhysicsDelegate.AddLambda([Ticket](UActorComponent* V){if(V==Ticket->Target)Ticket->Revision.fetch_add(1,std::memory_order_release);});
  SDK(TEXT("capture_physics_identity"));P.Proxy=C->GetPhysicsProxy();P.Dynamic=C->GetDynamicCollection();P.Registered=C->IsRegistered();P.Created=C->IsPhysicsStateCreated();return Guard();
 }
 void StartCall(){CallSerial=Serial;CallChanged=Changed;CallSaves=Saves.Num();}
 void Observe()
 {
  if(!Guard())return;TArray<TSharedPtr<FJsonValue>> Rows;for(auto&T:Targets){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("role"),T.Role);J->SetStringField(TEXT("captured_path"),T.Path);J->SetStringField(TEXT("before"),T.Before);auto A=ObjectProjection(T.Weak.Get());const bool Different=!T.Before.Equals(Json(A),ESearchCase::CaseSensitive);J->SetObjectField(TEXT("after"),A);J->SetBoolField(TEXT("changed"),T.Created||Different);Rows.Add(MakeShared<FJsonValueObject>(J));if(Different){++Changed;R.bChanged=true;T.Before=Json(A);}}R.Data->SetArrayField(TEXT("actual_target_observations"),Rows);
 }
 TSharedRef<FJsonObject> Snapshot(){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("session_id"),SessionId);J->SetBoolField(TEXT("world_evaluation_lease_present"),OwnsEvaluation&&LastOwnedWorld.Get()==World.Get());J->SetBoolField(TEXT("operation_complete"),Complete);bool Post=Complete&&R.Error.IsEmpty();R.Data->TryGetBoolField(TEXT("postcondition_passed"),Post);J->SetBoolField(TEXT("postcondition_passed"),Complete&&Post&&R.Error.IsEmpty());J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("failed_stage"),FailedStage);J->SetStringField(TEXT("error_code"),R.ErrorCode);J->SetStringField(TEXT("error"),R.Error);J->SetBoolField(TEXT("execution_mutation_attempted"),R.bMutationAttempted);J->SetBoolField(TEXT("changed"),R.bChanged);J->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);J->SetBoolField(TEXT("compile_attempted"),false);J->SetBoolField(TEXT("compiled"),false);J->SetBoolField(TEXT("save_requested"),R.bSaveRequested);J->SetBoolField(TEXT("save_attempted"),R.bSaveAttempted);J->SetBoolField(TEXT("saved"),R.bSaved);J->SetArrayField(TEXT("actual_mutation_prefix"),Events);J->SetArrayField(TEXT("actual_save_prefix"),Saves);J->SetObjectField(TEXT("native_call_counts"),MakeShared<FJsonObject>(*Counts));return J;}
 FString Finish(TSharedPtr<FJsonObject> Legacy)
 {
  if(!Legacy){Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error.IsEmpty()?TEXT("No legacy response"):R.Error);}
  bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);const TSharedPtr<FJsonObject>* D=nullptr;FString A,Status;if(Legacy->TryGetObjectField(TEXT("data"),D)&&D&&D->IsValid()){(*D)->TryGetStringField(TEXT("assertion_status"),A);(*D)->TryGetStringField(TEXT("operation_status"),Status);}
  if((!Success||A==TEXT("FAIL")||Status==TEXT("FAIL"))&&R.Error.IsEmpty())Fail(TEXT("postcondition_failed"),TEXT("Legacy result reports failure or failed assertion"));
  FResult Out=R;Out.Data=MakeShared<FJsonObject>(*R.Data);Out.bSuccess=Success&&R.Error.IsEmpty();Out.bPostconditionPassed=Out.bSuccess;Out.bMutationAttempted=Serial>CallSerial;Out.bChanged=Changed>CallChanged;Out.bSaveAttempted=false;Out.bSaved=false;TArray<TSharedPtr<FJsonValue>> CurrentEvents,CurrentSaves;for(const auto&E:Events){double I=0;E->AsObject()->TryGetNumberField(TEXT("index"),I);if(I>CallSerial)CurrentEvents.Add(E);}for(int32 I=static_cast<int32>(CallSaves);I<Saves.Num();++I){CurrentSaves.Add(Saves[I]);Out.bSaveAttempted=true;bool Saved=false;Saves[I]->AsObject()->TryGetBoolField(TEXT("sdk_returned_true"),Saved);Out.bSaved|=Saved;}
  Out.Data->SetObjectField(TEXT("legacy_result"),Legacy);Out.Data->SetObjectField(TEXT("session_execution"),Adopted?Adopted:Snapshot());Out.Data->SetBoolField(TEXT("operation_complete"),Complete);Out.Data->SetStringField(TEXT("session_id"),SessionId);Out.Data->SetArrayField(TEXT("actual_mutation_prefix"),CurrentEvents);Out.Data->SetArrayField(TEXT("actual_save_prefix"),CurrentSaves);Out.Data->SetStringField(TEXT("failed_stage"),FailedStage);Out.Data->SetStringField(TEXT("receipt_scope"),TEXT("Current request flags are separate from cumulative session_execution. No tick mutates a returned Python journal; no prior save is promoted into a sample request."));Out.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("OR of explicit SaveAsset/SaveMap SDK true returns, including earlier partial success; per-save captured package identity and dirty readback are separate."));if(Out.bSuccess)Out.Stage=Complete?TEXT("complete"):TEXT("scheduled_or_in_progress");return Cubeless::Operations::EncodeVariableResult(Out,*Operation);
 }
};
struct FExecutionPins{FWork&W;explicit FExecutionPins(FWork&In):W(In){}bool Acquire(){if(!SafeEditor())return W.Fail(TEXT("editor_lifetime_lost"),TEXT("Cannot reacquire weak execution targets"));for(auto&T:W.Targets){auto*O=T.Weak.Get();if(!Live(O))return W.Fail(TEXT("target_expired"),TEXT("Captured weak target expired between requests"));T.Pin.Reset(O);}return W.Guard();}~FExecutionPins(){for(auto&T:W.Targets)T.Pin.Reset();}};
FWork* Work=nullptr;TSharedPtr<FWork> Owner;
bool Guard(){return Work&&Work->Guard();}
template<typename F>bool Step(const TCHAR*S,F&&Fn,bool Mutating=true){if(!Guard()||!Work->R.Error.IsEmpty())return false;if(Mutating)Work->Event(S);else Work->SDK(S);Work->Count(S);Fn();return Guard()&&Work->R.Error.IsEmpty();}
struct FLegacy{static TSharedPtr<FJsonObject>CreateErrorResponse(const FString&E){Work->Fail(TEXT("operation_failed"),E);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),E);return J;}static TSharedPtr<FJsonObject>CreateSuccessResponse(const TSharedPtr<FJsonObject>&D){auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),true);J->SetObjectField(TEXT("data"),D);return J;}};
bool CollectionSchema(const UGeometryCollection*A)
{
 if(!Guard()||!Live(A))return false;const auto D=A->GetGeometryCollection();if(!D)return Work->Fail(TEXT("collection_missing"),TEXT("Managed collection unavailable"));const int32 N=D->NumElements(FGeometryCollection::TransformGroup),G=D->NumElements(FGeometryCollection::GeometryGroup),V=D->NumElements(FGeometryCollection::VerticesGroup),F=D->NumElements(FGeometryCollection::FacesGroup);
 if(N>1000000||G>1000000||V>4000000||F>4000000||D->Transform.Num()!=N||D->Parent.Num()!=N||D->Children.Num()!=N||D->SimulationType.Num()!=N||D->TransformToGeometryIndex.Num()!=N||D->TransformIndex.Num()!=G||D->VertexStart.Num()!=G||D->VertexCount.Num()!=G||D->FaceStart.Num()!=G||D->FaceCount.Num()!=G||D->BoundingBox.Num()!=G||D->Vertex.Num()!=V||D->BoneMap.Num()!=V||D->Indices.Num()!=F)return Work->Fail(TEXT("collection_schema"),TEXT("Managed array lengths/budgets are invalid"));
 TArray<uint8>Color;Color.Init(0,N);for(int32 I=0;I<N;++I){const int32 P=D->Parent[I],Gi=D->TransformToGeometryIndex[I];if(D->Transform[I].ContainsNaN()||(P!=INDEX_NONE&&!D->Parent.IsValidIndex(P))||(Gi!=INDEX_NONE&&(!D->TransformIndex.IsValidIndex(Gi)||D->TransformIndex[Gi]!=I)))return Work->Fail(TEXT("collection_schema"),TEXT("Invalid transform/parent/geometry mapping"));for(int32 C:D->Children[I])if(!D->Parent.IsValidIndex(C)||D->Parent[C]!=I)return Work->Fail(TEXT("collection_schema"),TEXT("Parent-child relation is inconsistent"));if(P!=INDEX_NONE&&!D->Children[P].Contains(I))return Work->Fail(TEXT("collection_schema"),TEXT("Parent does not reciprocate child"));int32 K=I;while(K!=INDEX_NONE&&Color[K]==0){Color[K]=1;K=D->Parent[K];}if(K!=INDEX_NONE&&Color[K]==1)return Work->Fail(TEXT("collection_schema"),TEXT("Parent cycle"));K=I;while(K!=INDEX_NONE&&Color[K]==1){Color[K]=2;K=D->Parent[K];}}
 for(int32 I=0;I<G;++I){const int32 T=D->TransformIndex[I];if(!D->Transform.IsValidIndex(T)||D->TransformToGeometryIndex[T]!=I||D->VertexStart[I]<0||D->VertexCount[I]<0||static_cast<int64>(D->VertexStart[I])+D->VertexCount[I]>V||D->FaceStart[I]<0||D->FaceCount[I]<0||static_cast<int64>(D->FaceStart[I])+D->FaceCount[I]>F)return Work->Fail(TEXT("collection_schema"),TEXT("Invalid geometry range/mapping"));}
 for(int32 I=0;I<V;++I)if(!D->Transform.IsValidIndex(D->BoneMap[I])||FVector(D->Vertex[I]).ContainsNaN())return Work->Fail(TEXT("collection_schema"),TEXT("Invalid vertex/bone"));for(const auto&Tri:D->Indices)if(!D->Vertex.IsValidIndex(Tri.X)||!D->Vertex.IsValidIndex(Tri.Y)||!D->Vertex.IsValidIndex(Tri.Z))return Work->Fail(TEXT("collection_schema"),TEXT("Invalid triangle vertex"));return true;
}

int64 SafeQuantize(double Value,double Scale){const double Q=Value*Scale;if(!FMath::IsFinite(Q)||Q<=static_cast<double>(MIN_int64)+4096||Q>=static_cast<double>(MAX_int64)-4096){Work->Fail(TEXT("numeric_range"),TEXT("Unsafe state hash quantization"));return 0;}return FMath::RoundToInt64(Q);}
constexpr const TCHAR* LearningRoot = TEXT("/Game/_MCP_Temp/ChaosDestructionLearning");
constexpr const TCHAR* SourceCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");

const TCHAR* AssetNames[] = {
    TEXT("SM_ChaosTestBlock"),
    TEXT("SM_ChaosRootProxy"),
    TEXT("GC_ChaosBase_96"),
    TEXT("GC_ChaosTinyOptimized"),
    TEXT("GC_ChaosRemoval"),
    TEXT("GC_ChaosRootProxy"),
    TEXT("CC_ChaosLearning"),
};

const FName LearningActorTag(TEXT("MCP_ChaosLearning"));
struct FPieceMetric
{
    int32 TransformIndex = INDEX_NONE;
    int32 GeometryIndex = INDEX_NONE;
    double ConvexVolume = 0.0;
    FBox WorldBounds = FBox(ForceInit);
};

struct FCollectionMetrics
{
    int32 TransformCount = 0;
    int32 GeometryCount = 0;
    int32 RigidCount = 0;
    int32 RootCount = 0;
    int32 InvalidParentCount = 0;
    int32 ParentChildMismatchCount = 0;
    int32 CycleCount = 0;
    int32 InvalidTransformToGeometryCount = 0;
    int32 InvalidTriangleIndexCount = 0;
    int32 BoundaryEdgeCount = 0;
    int32 NonManifoldEdgeCount = 0;
    int32 TinyCandidateWithContactCount = 0;
    double TotalConvexVolume = 0.0;
    double RelativeVolumeThreshold = 0.0;
    FBox OverallWorldBounds = FBox(ForceInit);
    TArray<FPieceMetric> Pieces;
    TArray<int32> TinyCandidateIndices;
};

bool IsWithinLearningRoot(const FString& Path)
{
    return Path.Equals(LearningRoot,ESearchCase::IgnoreCase) || Path.StartsWith(FString(LearningRoot) + TEXT("/"),ESearchCase::IgnoreCase);
}

FString QuantizedVertexKey(const FVector& Point)
{
    return FString::Printf(TEXT("%lld,%lld,%lld"),
        SafeQuantize(Point.X,1000.0),
        SafeQuantize(Point.Y,1000.0),
        SafeQuantize(Point.Z,1000.0));
}

FString EdgeKey(const FVector& A, const FVector& B)
{
    const FString KeyA = QuantizedVertexKey(A);
    const FString KeyB = QuantizedVertexKey(B);
    return KeyA < KeyB ? KeyA + TEXT("|") + KeyB : KeyB + TEXT("|") + KeyA;
}

bool BoundsTouch(const FBox& A, const FBox& B, double Tolerance = 0.25)
{
    if (!A.IsValid || !B.IsValid)
    {
        return false;
    }
    const FVector Gap(
        FMath::Max(0.0, FMath::Max(A.Min.X - B.Max.X, B.Min.X - A.Max.X)),
        FMath::Max(0.0, FMath::Max(A.Min.Y - B.Max.Y, B.Min.Y - A.Max.Y)),
        FMath::Max(0.0, FMath::Max(A.Min.Z - B.Max.Z, B.Min.Z - A.Max.Z)));
    const int32 SeparatedAxes = (Gap.X > Tolerance ? 1 : 0) + (Gap.Y > Tolerance ? 1 : 0) + (Gap.Z > Tolerance ? 1 : 0);
    return SeparatedAxes == 0;
}

FCollectionMetrics MeasureCollection(const UGeometryCollection* Asset, double TinyRelativeThreshold)
{
    FCollectionMetrics Metrics;
    if (!Asset || !Asset->GetGeometryCollection().IsValid())
    {
        return Metrics;
    }
    const FGeometryCollection& Collection = *Asset->GetGeometryCollection();
    Metrics.TransformCount = Collection.NumElements(FGeometryCollection::TransformGroup);
    Metrics.GeometryCount = Collection.NumElements(FGeometryCollection::GeometryGroup);
    if(!CollectionSchema(Asset)) { return Metrics; }
    TArray<FTransform> GlobalTransforms;
    GeometryCollectionAlgo::GlobalMatrices(Collection.Transform, Collection.Parent, GlobalTransforms);

    for (int32 TransformIndex = 0; TransformIndex < Metrics.TransformCount; ++TransformIndex)
    {
        const int32 ParentIndex = Collection.Parent[TransformIndex];
        Metrics.RootCount += ParentIndex == INDEX_NONE ? 1 : 0;
        if (ParentIndex != INDEX_NONE)
        {
            if (!Collection.Parent.IsValidIndex(ParentIndex))
            {
                ++Metrics.InvalidParentCount;
            }
            else if (!Collection.Children[ParentIndex].Contains(TransformIndex))
            {
                ++Metrics.ParentChildMismatchCount;
            }
        }
        for (int32 ChildIndex : Collection.Children[TransformIndex])
        {
            if (!Collection.Parent.IsValidIndex(ChildIndex) || Collection.Parent[ChildIndex] != TransformIndex)
            {
                ++Metrics.ParentChildMismatchCount;
            }
        }
        TSet<int32> Visited;
        for (int32 Walk = TransformIndex; Walk != INDEX_NONE && Collection.Parent.IsValidIndex(Walk); Walk = Collection.Parent[Walk])
        {
            if (Visited.Contains(Walk))
            {
                ++Metrics.CycleCount;
                break;
            }
            Visited.Add(Walk);
        }

        if (Collection.SimulationType[TransformIndex] != FGeometryCollection::ESimulationTypes::FST_Rigid)
        {
            continue;
        }
        ++Metrics.RigidCount;
        const int32 GeometryIndex = Collection.TransformToGeometryIndex[TransformIndex];
        if (!Collection.TransformIndex.IsValidIndex(GeometryIndex) || Collection.TransformIndex[GeometryIndex] != TransformIndex)
        {
            ++Metrics.InvalidTransformToGeometryCount;
            continue;
        }
        FPieceMetric Piece;
        Piece.TransformIndex = TransformIndex;
        Piece.GeometryIndex = GeometryIndex;
        const int32 FaceStart = Collection.FaceStart[GeometryIndex];
        const int32 FaceCount = Collection.FaceCount[GeometryIndex];
        const int32 VertexStart = Collection.VertexStart[GeometryIndex];
        const int32 VertexCount = Collection.VertexCount[GeometryIndex];
        TMap<FString, int32> EdgeUseCount;
        double SignedVolume = 0.0;
        for (int32 FaceOffset = 0; FaceOffset < FaceCount; ++FaceOffset)
        {
            const int32 FaceIndex = FaceStart + FaceOffset;
            if (!Collection.Indices.IsValidIndex(FaceIndex))
            {
                ++Metrics.InvalidTriangleIndexCount;
                continue;
            }
            const FIntVector Triangle = Collection.Indices[FaceIndex];
            const int32 VertexIndices[3] = {Triangle.X, Triangle.Y, Triangle.Z};
            bool bValidTriangle = true;
            FVector Vertices[3];
            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                const int32 VertexIndex = VertexIndices[Corner];
                if (!Collection.Vertex.IsValidIndex(VertexIndex) || VertexIndex < VertexStart || VertexIndex >= VertexStart + VertexCount)
                {
                    bValidTriangle = false;
                    ++Metrics.InvalidTriangleIndexCount;
                    break;
                }
                Vertices[Corner] = GlobalTransforms.IsValidIndex(TransformIndex)
                    ? GlobalTransforms[TransformIndex].TransformPosition(FVector(Collection.Vertex[VertexIndex]))
                    : FVector(Collection.Vertex[VertexIndex]);
                Piece.WorldBounds += Vertices[Corner];
            }
            if (!bValidTriangle)
            {
                continue;
            }
            SignedVolume += FVector::DotProduct(Vertices[0], FVector::CrossProduct(Vertices[1], Vertices[2])) / 6.0;
            ++EdgeUseCount.FindOrAdd(EdgeKey(Vertices[0], Vertices[1]));
            ++EdgeUseCount.FindOrAdd(EdgeKey(Vertices[1], Vertices[2]));
            ++EdgeUseCount.FindOrAdd(EdgeKey(Vertices[2], Vertices[0]));
        }
        for (const TPair<FString, int32>& Edge : EdgeUseCount)
        {
            Metrics.BoundaryEdgeCount += Edge.Value == 1 ? 1 : 0;
            Metrics.NonManifoldEdgeCount += Edge.Value > 2 ? 1 : 0;
        }
        Piece.ConvexVolume = FMath::Abs(SignedVolume);
        Metrics.TotalConvexVolume += Piece.ConvexVolume;
        Metrics.OverallWorldBounds += Piece.WorldBounds;
        Metrics.Pieces.Add(Piece);
    }
    Metrics.RelativeVolumeThreshold = Metrics.TotalConvexVolume * FMath::Pow(TinyRelativeThreshold, 3.0);
    for (const FPieceMetric& Piece : Metrics.Pieces)
    {
        if (Piece.ConvexVolume >= Metrics.RelativeVolumeThreshold)
        {
            continue;
        }
        Metrics.TinyCandidateIndices.Add(Piece.TransformIndex);
        bool bHasContact = false;
        for (const FPieceMetric& Neighbor : Metrics.Pieces)
        {
            if (Neighbor.TransformIndex != Piece.TransformIndex && BoundsTouch(Piece.WorldBounds, Neighbor.WorldBounds))
            {
                bHasContact = true;
                break;
            }
        }
        Metrics.TinyCandidateWithContactCount += bHasContact ? 1 : 0;
    }
    return Metrics;
}

TArray<TSharedPtr<FJsonValue>> IntsToJson(const TArray<int32>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (int32 Value : Values)
    {
        Result.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return Result;
}

TArray<TSharedPtr<FJsonValue>> DoublesToJson(const TArray<double>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (double Value : Values)
    {
        Result.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return Result;
}

TArray<TSharedPtr<FJsonValue>> VectorToJson(const FVector& Value);

TSharedPtr<FJsonObject> MetricsToJson(const FCollectionMetrics& Metrics)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("transform_count"), Metrics.TransformCount);
    Data->SetNumberField(TEXT("geometry_count"), Metrics.GeometryCount);
    Data->SetNumberField(TEXT("rigid_count"), Metrics.RigidCount);
    Data->SetNumberField(TEXT("root_count"), Metrics.RootCount);
    Data->SetNumberField(TEXT("convex_volume"), Metrics.TotalConvexVolume);
    Data->SetNumberField(TEXT("relative_volume_threshold"), Metrics.RelativeVolumeThreshold);
    Data->SetArrayField(TEXT("tiny_candidate_indices"), IntsToJson(Metrics.TinyCandidateIndices));
    Data->SetNumberField(TEXT("tiny_candidate_count"), Metrics.TinyCandidateIndices.Num());
    Data->SetNumberField(TEXT("tiny_candidate_with_contact_count"), Metrics.TinyCandidateWithContactCount);
    Data->SetNumberField(TEXT("invalid_parent_count"), Metrics.InvalidParentCount);
    Data->SetNumberField(TEXT("parent_child_mismatch_count"), Metrics.ParentChildMismatchCount);
    Data->SetNumberField(TEXT("cycle_count"), Metrics.CycleCount);
    Data->SetNumberField(TEXT("invalid_transform_to_geometry_count"), Metrics.InvalidTransformToGeometryCount);
    Data->SetNumberField(TEXT("invalid_triangle_index_count"), Metrics.InvalidTriangleIndexCount);
    Data->SetNumberField(TEXT("boundary_edge_count"), Metrics.BoundaryEdgeCount);
    Data->SetNumberField(TEXT("nonmanifold_edge_count"), Metrics.NonManifoldEdgeCount);
    Data->SetArrayField(TEXT("overall_world_bounds_min"), VectorToJson(Metrics.OverallWorldBounds.IsValid ? Metrics.OverallWorldBounds.Min : FVector::ZeroVector));
    Data->SetArrayField(TEXT("overall_world_bounds_max"), VectorToJson(Metrics.OverallWorldBounds.IsValid ? Metrics.OverallWorldBounds.Max : FVector::ZeroVector));
    Data->SetNumberField(TEXT("overall_world_bounds_volume"), Metrics.OverallWorldBounds.IsValid ? Metrics.OverallWorldBounds.GetVolume() : 0.0);
    return Data;
}

double PercentDelta(double Before, double After)
{
    return FMath::Abs(Before) > UE_DOUBLE_SMALL_NUMBER ? FMath::Abs(After - Before) * 100.0 / FMath::Abs(Before) : 0.0;
}

FString GetStringParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, const TCHAR* DefaultValue)
{
    FString Value;
    return Params.IsValid() && Params->TryGetStringField(Name, Value) ? Value.TrimStartAndEnd() : FString(DefaultValue);
}

bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, bool DefaultValue)
{
    return Params.IsValid() && Params->HasField(Name) ? Params->GetBoolField(Name) : DefaultValue;
}

double GetNumberParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, double DefaultValue)
{
    return Params.IsValid() && Params->HasField(Name) ? Params->GetNumberField(Name) : DefaultValue;
}

bool TryGetVectorParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, FVector& OutValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(Name, Values) || !Values || Values->Num() != 3)
    {
        return false;
    }
    OutValue = FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
    return true;
}

TArray<TSharedPtr<FJsonValue>> VectorToJson(const FVector& Value)
{
    return {
        MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y),
        MakeShared<FJsonValueNumber>(Value.Z),
    };
}

TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Value)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("pitch"), Value.Pitch);
    Result->SetNumberField(TEXT("yaw"), Value.Yaw);
    Result->SetNumberField(TEXT("roll"), Value.Roll);
    return Result;
}

TArray<TSharedPtr<FJsonValue>> Vector2DToJson(const FVector2D& Value)
{
    return {
        MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y),
    };
}

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (const FString& Value : Values)
    {
        Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
}

bool ValidateLearningRoot(const FString& Root, FString& OutError)
{
    if (!Root.Equals(LearningRoot,ESearchCase::IgnoreCase))
    {
        OutError = FString::Printf(TEXT("asset_root is restricted to %s (received %s)"), LearningRoot, *Root);
        return false;
    }
    return true;
}

UWorld* GetActivePlayWorldForChaosCommands()
{
    if (!GEditor)
    {
        return nullptr;
    }

    FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
    return PIEWorldContext ? PIEWorldContext->World() : nullptr;
}

bool IsPlaySessionActive()
{
    return GEditor && GEditor->GetPIEWorldContext() != nullptr;
}

UWorld* ResolveWorld(bool bPreferPlay, FString& OutContext)
{
    if (bPreferPlay)
    {
        if (UWorld* PlayWorld = GetActivePlayWorldForChaosCommands())
        {
            OutContext = TEXT("play");
            return PlayWorld;
        }
    }
    if (GEditor)
    {
        if (UWorld* World = GEditor->GetEditorWorldContext().World())
        {
            OutContext = TEXT("editor");
            return World;
        }
    }
    OutContext = TEXT("none");
    return nullptr;
}

template <typename T>
T* FindActor(UWorld* World, const FString& NameOrLabel)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<T> It(World); It; ++It)
    {
        T* Actor = *It;
        if (NameOrLabel.IsEmpty() || Actor->GetName() == NameOrLabel || Actor->GetActorLabel() == NameOrLabel || Actor->GetPathName() == NameOrLabel)
        {
            return Actor;
        }
    }
    return nullptr;
}

TArray<UMaterialInterface*> GetStaticMeshMaterials(UStaticMesh* Mesh)
{
    TArray<UMaterialInterface*> Materials;
    if (!Mesh)
    {
        return Materials;
    }
    for (int32 Index = 0; Index < Mesh->GetStaticMaterials().Num(); ++Index)
    {
        Materials.Add(Mesh->GetMaterial(Index));
    }
    return Materials;
}

TArray<UMaterialInterface*> GetCollectionMaterials(const UGeometryCollection* Collection)
{
    TArray<UMaterialInterface*> Materials;
    if (!Collection)
    {
        return Materials;
    }
    for (UMaterialInterface* Material : Collection->Materials)
    {
        Materials.Add(Material);
    }
    return Materials;
}

TSharedPtr<FJsonObject> InspectCollection(UGeometryCollection* Asset, UGeometryCollectionComponent* Component, double TinyRelativeThreshold)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("asset_path"), Asset ? Asset->GetPathName() : FString());
    Data->SetBoolField(TEXT("asset_found"), Asset != nullptr);
    if (!Asset || !Asset->GetGeometryCollection().IsValid())
    {
        return Data;
    }

    const FGeometryCollection& Collection = *Asset->GetGeometryCollection();
    if(!CollectionSchema(Asset)) { return Data; }
    const FCollectionMetrics GuardedMetrics = MeasureCollection(Asset, TinyRelativeThreshold);
    const int32 TransformCount = Collection.NumElements(FGeometryCollection::TransformGroup);
    const int32 GeometryCount = Collection.NumElements(FGeometryCollection::GeometryGroup);
    int32 RigidCount = 0;
    int32 ClusterCount = 0;
    int32 RootCount = 0;
    int32 LeafCount = 0;
    int32 MaxLevel = 0;
    for (int32 Index = 0; Index < TransformCount; ++Index)
    {
        RigidCount += Collection.SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid ? 1 : 0;
        ClusterCount += Collection.SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Clustered ? 1 : 0;
        RootCount += Collection.Parent[Index] == INDEX_NONE ? 1 : 0;
        LeafCount += Collection.Children[Index].Num() == 0 ? 1 : 0;
        int32 Level = 0;
        for (int32 Parent = Collection.Parent[Index]; Parent != INDEX_NONE && Collection.Parent.IsValidIndex(Parent); Parent = Collection.Parent[Parent])
        {
            ++Level;
            if (Level > TransformCount)
            {
                break;
            }
        }
        MaxLevel = FMath::Max(MaxLevel, Level);
    }

    double TotalBoxVolume = 0.0;
    TArray<double> BoxVolumes;
    BoxVolumes.Reserve(Collection.BoundingBox.Num());
    FBox OverallBounds(ForceInit);
    for (const FBox& Bounds : Collection.BoundingBox)
    {
        const double Volume = Bounds.GetVolume();
        BoxVolumes.Add(Volume);
        TotalBoxVolume += Volume;
        OverallBounds += Bounds;
    }
    const double MinTinyVolume = FMath::Pow(FMath::Pow(FMath::Max(TotalBoxVolume, 0.0), 1.0 / 3.0) * TinyRelativeThreshold, 3.0);
    int32 TinyCandidateCount = 0;
    for (double Volume : BoxVolumes)
    {
        TinyCandidateCount += Volume < MinTinyVolume ? 1 : 0;
    }

    GeometryCollection::Facades::FCollectionRemoveOnBreakFacade RemoveFacade(Collection);
    int32 RemoveOnBreakEnabledCount = 0;
    if (RemoveFacade.IsValid())
    {
        for (int32 Index = 0; Index < TransformCount; ++Index)
        {
            RemoveOnBreakEnabledCount += RemoveFacade.GetData(Index).IsEnabled() ? 1 : 0;
        }
    }

    Data->SetNumberField(TEXT("transform_count"), TransformCount);
    Data->SetNumberField(TEXT("geometry_count"), GeometryCount);
    Data->SetNumberField(TEXT("rigid_count"), RigidCount);
    Data->SetNumberField(TEXT("cluster_count"), ClusterCount);
    Data->SetNumberField(TEXT("root_count"), RootCount);
    Data->SetNumberField(TEXT("leaf_count"), LeafCount);
    Data->SetNumberField(TEXT("max_hierarchy_level"), MaxLevel);
    Data->SetNumberField(TEXT("total_piece_bounds_volume"), TotalBoxVolume);
    Data->SetNumberField(TEXT("overall_bounds_volume"), OverallBounds.IsValid ? OverallBounds.GetVolume() : 0.0);
    Data->SetNumberField(TEXT("tiny_relative_threshold"), TinyRelativeThreshold);
    Data->SetNumberField(TEXT("tiny_candidate_count"), TinyCandidateCount);
    Data->SetBoolField(TEXT("enable_nanite"), Asset->EnableNanite);
    Data->SetBoolField(TEXT("has_nanite_data"), Asset->HasNaniteData());
    Data->SetBoolField(TEXT("enable_nanite_fallback"), Asset->bEnableNaniteFallback);
    Data->SetNumberField(TEXT("root_proxy_mesh_count"), Asset->RootProxyData.ProxyMeshes.Num());
    Data->SetNumberField(TEXT("root_proxy_transform_count"), Asset->RootProxyData.MeshTransforms.Num());
    bool bRootProxyArraysValid = Asset->RootProxyData.ProxyMeshes.Num() > 0 &&
        Asset->RootProxyData.ProxyMeshes.Num() == Asset->RootProxyData.MeshTransforms.Num();
    for (int32 ProxyIndex = 0;
        ProxyIndex < Asset->RootProxyData.ProxyMeshes.Num() && bRootProxyArraysValid;
        ++ProxyIndex)
    {
        bRootProxyArraysValid = Asset->RootProxyData.ProxyMeshes[ProxyIndex] != nullptr &&
            !Asset->RootProxyData.MeshTransforms[ProxyIndex].ContainsNaN() &&
            !Asset->RootProxyData.MeshTransforms[ProxyIndex].GetScale3D().IsNearlyZero();
    }
    Data->SetBoolField(TEXT("root_proxy_arrays_valid"), bRootProxyArraysValid);
    Data->SetStringField(TEXT("custom_renderer_class"),
        Asset->CustomRendererType ? Asset->CustomRendererType->GetPathName() : FString());
    TArray<FString> ProxyPaths;
    for (const UStaticMesh* Mesh : Asset->RootProxyData.ProxyMeshes)
    {
        ProxyPaths.Add(Mesh ? Mesh->GetPathName() : FString());
    }
    Data->SetArrayField(TEXT("root_proxy_mesh_paths"), StringsToJson(ProxyPaths));
    Data->SetBoolField(TEXT("remove_on_sleep_enabled"), Asset->bRemoveOnMaxSleep);
    Data->SetArrayField(TEXT("maximum_sleep_time"), Vector2DToJson(Asset->MaximumSleepTime));
    Data->SetArrayField(TEXT("removal_duration"), Vector2DToJson(Asset->RemovalDuration));
    Data->SetBoolField(TEXT("remove_on_break_schema_valid"), RemoveFacade.IsValid());
    Data->SetNumberField(TEXT("remove_on_break_enabled_count"), RemoveOnBreakEnabledCount);
    Data->SetObjectField(TEXT("guarded_metrics"), MetricsToJson(GuardedMetrics));
    Data->SetArrayField(TEXT("tiny_candidate_indices"), IntsToJson(GuardedMetrics.TinyCandidateIndices));
    Data->SetNumberField(TEXT("tiny_candidate_count"), GuardedMetrics.TinyCandidateIndices.Num());
    Data->SetNumberField(TEXT("convex_volume"), GuardedMetrics.TotalConvexVolume);
    Data->SetNumberField(TEXT("world_bounds_volume"), GuardedMetrics.OverallWorldBounds.IsValid ? GuardedMetrics.OverallWorldBounds.GetVolume() : 0.0);
    const bool bStructuralAssertionsPassed = GuardedMetrics.RootCount == 1 &&
        GuardedMetrics.InvalidParentCount == 0 && GuardedMetrics.ParentChildMismatchCount == 0 && GuardedMetrics.CycleCount == 0 &&
        GuardedMetrics.InvalidTransformToGeometryCount == 0 && GuardedMetrics.InvalidTriangleIndexCount == 0 &&
        GuardedMetrics.BoundaryEdgeCount == 0 && GuardedMetrics.NonManifoldEdgeCount == 0;
    Data->SetStringField(TEXT("assertion_status"), bStructuralAssertionsPassed ? TEXT("PASS") : TEXT("FAIL"));

    Data->SetBoolField(TEXT("component_found"), Component != nullptr);
    return Data;
}

void RememberEvaluation(AChaosCacheManager*M,UWorld*W,UChaosCacheCollection*C,ECacheMode Mode){LastOwnedManager=M;LastOwnedWorld=W;LastOwnedCollection=C;LastOwnedMode=Mode;OwnsEvaluation=true;}
bool EndManagerEvaluation(AChaosCacheManager*M,UWorld*W,const TCHAR*Stage)
{
 if(!Guard()||!Work->R.Error.IsEmpty()||!Live(M)||!Live(W))return false;
 RememberEvaluation(M,W,M->CacheCollection.Get(),M->CacheMode);
 const bool Safe=Step(Stage,[&]{M->EndEvaluate();Work->R.Data->SetBoolField(TEXT("end_evaluate_returned"),true);});
 if(Safe)OwnsEvaluation=false;else Work->R.Data->SetBoolField(TEXT("owned_evaluation_lease_unresolved"),true);
 return Safe;
}

FString ObjectPath(const FString& P){return P.Contains(TEXT("."))?P:P+TEXT(".")+FPackageName::GetLongPackageAssetName(P);}
TSharedPtr<FJsonObject> CaptureDirtyStates(const TArray<FString>& Paths)
{
 auto J=MakeShared<FJsonObject>();for(const auto&P:Paths){if(!Guard()||!Work->R.Error.IsEmpty())break;Work->SDK(TEXT("observe_package_dirty"));UPackage*Package=FindPackage(nullptr,*P);J->SetBoolField(P,Package&&Package->IsDirty());}return J;
}
TSharedPtr<FJsonObject> AuditAssetDependencies(const TArray<FString>& Paths)
{
 auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>>Rows;TArray<FString>Bad;auto&Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
 for(const auto&P:Paths){if(!Guard()||!Work->R.Error.IsEmpty())break;TArray<FName>Deps;if(!Step(TEXT("registry_dependencies"),[&]{Registry.GetDependencies(FName(*P),Deps,UE::AssetRegistry::EDependencyCategory::Package);},false))break;if(Deps.Num()>8192){Work->Fail(TEXT("dependency_budget"),TEXT("Package dependency projection exceeds8192"));break;}TArray<FString>Names;for(const FName&N:Deps){const FString S=N.ToString();Names.Add(S);if(Forbidden(S))Bad.AddUnique(S);}auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("asset_path"),P);Row->SetArrayField(TEXT("package_dependencies"),StringsToJson(Names));Rows.Add(MakeShared<FJsonValueObject>(Row));}
 J->SetArrayField(TEXT("assets"),Rows);J->SetArrayField(TEXT("forbidden_mcp_dependencies"),StringsToJson(Bad));J->SetNumberField(TEXT("forbidden_mcp_dependency_count"),Bad.Num());J->SetStringField(TEXT("audit_source"),TEXT("AssetRegistry on-disk package dependencies"));J->SetStringField(TEXT("scope"),TEXT("Package dependency projection only, not a complete serialized/management graph independence proof"));return J;
}
bool SaveExact(UObject*Asset,const FString&Path,bool Map=false)
{
 if(!Guard()||!Work->R.Error.IsEmpty()||!Live(Asset))return false;auto*Package=Asset->GetPackage();TStrongObjectPtr<UPackage>Pin(Package);const FString Before=Package->GetName();auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Path);J->SetStringField(TEXT("captured_package"),Before);J->SetBoolField(TEXT("sdk_returned_true"),false);Work->Saves.Add(MakeShared<FJsonValueObject>(J));Work->R.bSaveAttempted=true;bool Returned=false;
 const bool Safe=Step(Map?TEXT("SaveMap"):TEXT("SaveAsset"),[&]{Returned=Map?UEditorLoadingAndSavingUtils::SaveMap(CastChecked<UWorld>(Asset),Path):UEditorAssetLibrary::SaveAsset(Path,false);J->SetBoolField(TEXT("sdk_returned_true"),Returned);Work->R.bSaved|=Returned;});
 if(SafeEditor()&&Live(Package)){J->SetBoolField(TEXT("captured_package_live"),true);J->SetStringField(TEXT("captured_package_path_after"),Package->GetName());J->SetBoolField(TEXT("captured_package_name_unchanged"),Package->GetName().Equals(Before,ESearchCase::CaseSensitive));J->SetBoolField(TEXT("dirty_after"),Package->IsDirty());}
 J->SetBoolField(TEXT("post_guard_passed"),Safe);if(!Safe)return false;if(!Returned||Package->IsDirty())return Work->Fail(TEXT("save_failed"),FString::Printf(TEXT("Failed to save fixture asset cleanly: %s"),*Path));return true;
}
template<typename T>T* EnsureAsset(const FString& P,UObject* Source,bool&Created)
{
 Created=false;if(!Guard()||!Work->R.Error.IsEmpty())return nullptr;UObject*Existing=Work->Load<UObject>(ObjectPath(P),TEXT("load_fixture_asset"));if(!Work->R.Error.IsEmpty())return nullptr;if(Existing){T*Typed=Cast<T>(Existing);if(!Typed)Work->Fail(TEXT("asset_type_collision"),TEXT("Existing fixture path has another class"));return Typed;}
 UPackage*Package=nullptr;if(!Step(TEXT("create_fixture_package"),[&]{Package=CreatePackage(*P);})||!Package||!Work->Track(Package,TEXT("fixture_package")))return nullptr;
 const FString Name=FPackageName::GetLongPackageAssetName(P);if(FindObject<UObject>(Package,*Name))return Work->Fail(TEXT("asset_collision"),TEXT("Fixture path appeared during creation")),nullptr;T*Asset=nullptr;
 if(!Step(TEXT("create_fixture_asset"),[&]{Asset=Source?Cast<T>(StaticDuplicateObject(Source,Package,*Name,RF_Public|RF_Standalone|RF_Transactional)):NewObject<T>(Package,*Name,RF_Public|RF_Standalone|RF_Transactional);Work->ReturnedObject(TEXT("create_fixture_asset"),ObjectPath(P),Asset);})||!Asset)return nullptr;Created=true;if(!Work->Track(Asset,TEXT("fixture_asset"),nullptr,true))return nullptr;
 if(!Asset->GetPathName().Equals(ObjectPath(P),ESearchCase::CaseSensitive))return Work->Fail(TEXT("asset_identity_lost"),TEXT("Created fixture object path differs from request")),nullptr;
 if(!Step(TEXT("register_fixture_asset"),[&]{FAssetRegistryModule::AssetCreated(Asset);})||!Step(TEXT("dirty_fixture_asset"),[&]{Asset->MarkPackageDirty();}))return nullptr;return Asset;
}
UStaticMesh* EnsureDuplicatedStaticMesh(const FString&P,UStaticMesh*S,bool&C){return EnsureAsset<UStaticMesh>(P,S,C);}
UGeometryCollection* EnsureGeometryCollection(const FString&P,bool&C){return EnsureAsset<UGeometryCollection>(P,nullptr,C);}
UChaosCacheCollection* EnsureCacheCollection(const FString&P,bool&C){return EnsureAsset<UChaosCacheCollection>(P,nullptr,C);}
UGeometryCollection* LoadGeometryCollection(const FString&P){return Work->Load<UGeometryCollection>(ObjectPath(P),TEXT("load_geometry_collection"));}
UChaosCacheCollection* LoadCacheCollection(const FString&P){return Work->Load<UChaosCacheCollection>(ObjectPath(P),TEXT("load_cache_collection"));}
void FinalizeGeometryCollection(UGeometryCollection*A)
{
 if(!Live(A)||!CollectionSchema(A))return;if(!Step(TEXT("InvalidateCollection"),[&]{A->InvalidateCollection();}))return;if(!Step(TEXT("UpdateGeometryDependentProperties"),[&]{A->UpdateGeometryDependentProperties();}))return;if(!Step(TEXT("CreateSimulationData"),[&]{A->CreateSimulationData();}))return;if(!Step(TEXT("RebuildRenderData"),[&]{A->RebuildRenderData();}))return;Step(TEXT("dirty_geometry_collection"),[&]{A->MarkPackageDirty();});
}
void BuildBaseCollection(UGeometryCollection*A,UStaticMesh*M)
{
 if(!Step(TEXT("modify_base"),[&]{A->Modify();})||!Step(TEXT("reset_base"),[&]{A->Reset();}))return;const auto Materials=GetStaticMeshMaterials(M);if(!Guard())return;
 constexpr int32 Columns=8,Rows=6,Depth=2;for(int32 Z=0;Z<Depth;++Z)for(int32 Y=0;Y<Rows;++Y){TArray<double>Widths;double RowWidth=0;for(int32 X=0;X<Columns;++X){const int32 I=Z*Columns*Rows+Y*Columns+X;const double W=I%4==0?6.0:45.0;Widths.Add(W);RowWidth+=W;}double Cursor=-RowWidth*.5;for(int32 X=0;X<Columns;++X){const int32 I=Z*Columns*Rows+Y*Columns+X;const double Center=Cursor+Widths[X]*.5;Cursor+=Widths[X];const FVector Scale=I%4==0?FVector(.06,.06,.06):FVector(.45,.45,.45);const FVector Location(Center,(Z-(Depth-1)*.5)*45,(Y+.5)*45);if(!Step(TEXT("AppendStaticMesh"),[&]{FGeometryCollectionEngineConversion::AppendStaticMesh(M,Materials,FTransform(FQuat::Identity,Location,Scale),A,false,true,false,false);}))return;}}
 if(!CollectionSchema(A))return;if(!Step(TEXT("ReindexMaterials"),[&]{A->GetGeometryCollection()->ReindexMaterials();}))return;for(int32 Start=84;Start>=0;Start-=12){TArray<int32>Bones;for(int32 I=0;I<12;++I)Bones.Add(Start+I);if(!Step(TEXT("cluster_group"),[&]{FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(A->GetGeometryCollection().Get(),Start,Bones,true,true);}))return;}
 if(!Step(TEXT("cluster_root"),[&]{FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(A->GetGeometryCollection().Get(),TEXT("ChaosLearningRoot"),true);}))return;
 if(!Step(TEXT("configure_base_properties"),[&]{A->EnableClustering=true;A->MaxClusterLevel=10;A->DamageThreshold={50000.0f,3000.0f,250.0f};A->bUseSizeSpecificDamageThreshold=false;A->bRemoveOnMaxSleep=false;A->bScaleOnRemoval=true;A->SizeSpecificData.Reset();auto Collision=UGeometryCollection::GeometryCollectionSizeSpecificDataDefaults();if(Collision.CollisionShapes.IsEmpty())Collision.CollisionShapes.AddDefaulted();for(auto&S:Collision.CollisionShapes){S.CollisionType=ECollisionTypeEnum::Chaos_Volumetric;S.ImplicitType=EImplicitTypeEnum::Chaos_Implicit_Box;S.CollisionMarginFraction=0;}A->SizeSpecificData.Add(Collision);}))return;FinalizeGeometryCollection(A);
}
void CopyCollectionFrom(UGeometryCollection*T,const UGeometryCollection*S)
{
 if(!CollectionSchema(S)||!Step(TEXT("modify_collection_copy"),[&]{T->Modify();}))return;const auto Materials=GetCollectionMaterials(S);if(!Guard())return;if(!Step(TEXT("ResetFrom"),[&]{T->ResetFrom(*S->GetGeometryCollection(),Materials,false);}))return;
 Step(TEXT("copy_simulation_properties"),[&]{T->EnableClustering=S->EnableClustering;T->MaxClusterLevel=S->MaxClusterLevel;T->DamageModel=S->DamageModel;T->DamageThreshold=S->DamageThreshold;T->bUseSizeSpecificDamageThreshold=S->bUseSizeSpecificDamageThreshold;T->SizeSpecificData=S->SizeSpecificData;});
}
template<typename T>T* EnsureTaggedActor(UWorld*W,const FString&Name,const FTransform&Transform)
{
 if(!Guard()||!Work->R.Error.IsEmpty())return nullptr;AActor*Existing=nullptr;for(TActorIterator<AActor>It(W);It;++It)if(It->GetName()==Name){Existing=*It;break;}T*Actor=Cast<T>(Existing);if(Existing&&!Actor)return Work->Fail(TEXT("actor_type_collision"),TEXT("Existing named learning actor has another class")),nullptr;
 if(Actor){if(!Work->Track(Actor,TEXT("reused_learning_actor")))return nullptr;}else{FActorSpawnParameters P;P.Name=FName(*Name);P.NameMode=FActorSpawnParameters::ESpawnActorNameMode::Required_ReturnNull;P.ObjectFlags=RF_Transactional;if(!Step(TEXT("spawn_learning_actor"),[&]{Actor=W->SpawnActor<T>(T::StaticClass(),Transform,P);Work->ReturnedObject(TEXT("spawn_learning_actor"),Name,Actor);})||!Actor||!Work->Track(Actor,TEXT("created_learning_actor"),nullptr,true))return nullptr;}
 if(!Step(TEXT("modify_learning_actor"),[&]{Actor->Modify();})||!Step(TEXT("tag_learning_actor"),[&]{Actor->Tags.AddUnique(LearningActorTag);})||!Step(TEXT("position_learning_actor"),[&]{Actor->SetActorTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);})||!Step(TEXT("label_learning_actor"),[&]{Actor->SetActorLabel(Name);})||!Step(TEXT("collision_learning_actor"),[&]{Actor->SetActorEnableCollision(true);}))return nullptr;return Actor;
}
UGeometryCollectionComponent* ConfigureGeometryActor(AActor*A,const FString&Name,UGeometryCollection*Rest,bool Sleep,bool Break,int32 OneWay,const FTransform&Transform)
{
 if(!Live(A)||!Live(Rest)||!Guard())return nullptr;auto*C=A->FindComponentByClass<UGeometryCollectionComponent>();if(!C){if(FindObject<UObject>(A,*Name))return Work->Fail(TEXT("component_type_collision"),TEXT("Requested component name already exists")),nullptr;if(!Step(TEXT("new_learning_component"),[&]{C=NewObject<UGeometryCollectionComponent>(A,FName(*Name),RF_Transactional);Work->ReturnedObject(TEXT("new_learning_component"),Name,C);})||!C||!Work->Track(C,TEXT("learning_component"),A,true))return nullptr;if(!Step(TEXT("add_instance_component"),[&]{A->AddInstanceComponent(C);})||!Step(TEXT("set_root_component"),[&]{A->SetRootComponent(C);})||!Step(TEXT("set_component_transform"),[&]{C->SetWorldTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);}))return nullptr;}else if(!Work->Track(C,TEXT("learning_component"),A))return nullptr;
 if(!Step(TEXT("component_mobility"),[&]{C->SetMobility(EComponentMobility::Movable);})||!Step(TEXT("SetRestCollection"),[&]{C->SetRestCollection(Rest,true);})||!Step(TEXT("removal_flags"),[&]{C->bAllowRemovalOnSleep=Sleep;C->bAllowRemovalOnBreak=Break;})||!Step(TEXT("OneWayInteraction"),[&]{C->SetOneWayInteractionLevel(OneWay);})||!Step(TEXT("NotifyBreaks"),[&]{C->SetNotifyBreaks(true);}))return nullptr;
 if(!Step(TEXT("collision_profile"),[&]{C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);})||!Step(TEXT("collision_enabled"),[&]{C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);})||!Step(TEXT("collision_object"),[&]{C->SetCollisionObjectType(ECC_Destructible);})||!Step(TEXT("collision_responses"),[&]{C->SetCollisionResponseToAllChannels(ECR_Block);})||!Step(TEXT("collision_visibility"),[&]{C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);}))return nullptr;
 if(!Step(TEXT("CCD"),[&]{C->BodyInstance.bUseCCD=true;})||!Step(TEXT("SetUseCCD"),[&]{C->SetUseCCD(true);})||!Step(TEXT("SetSimulatePhysics"),[&]{C->SetSimulatePhysics(true);}))return nullptr;if(!C->IsRegistered()&&!Step(TEXT("RegisterComponent"),[&]{C->RegisterComponent();}))return nullptr;
 if(!Step(TEXT("SetActorTransform"),[&]{A->SetActorTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);})||!Step(TEXT("UpdateComponentToWorld"),[&]{C->UpdateComponentToWorld();})||!Step(TEXT("RecreatePhysicsState"),[&]{C->RecreatePhysicsState();})||!Step(TEXT("dirty_learning_component"),[&]{C->MarkPackageDirty();}))return nullptr;return C;
}

FString SourcePackageMD5(const UObject* Asset)
{
 if(!Guard()||!Live(Asset))return FString();Work->SDK(TEXT("source_package_read_hash"));FString Filename;TArray<uint8>Bytes;
 if(!FPackageName::TryConvertLongPackageNameToFilename(Asset->GetPackage()->GetName(),Filename,FPackageName::GetAssetPackageExtension()))return FString();
 const int64 Size=IFileManager::Get().FileSize(*Filename);if(Size<0||Size>67108864)return FString();return FFileHelper::LoadFileToArray(Bytes,*Filename)?FMD5::HashBytes(Bytes.GetData(),Bytes.Num()):FString();
}

bool GuardLearningObserved(AChaosCacheManager*M)
{
 if(!Guard()||!Live(M))return false;auto*Old=M->CacheCollection.Get();if(Old&&!Work->Track(Old,TEXT("previous_manager_collection")))return false;
 const auto Observed=M->GetObservedComponents();if(Observed.Num()>8192)return Work->Fail(TEXT("observed_budget"),TEXT("Observed component count exceeds budget"));
 for(const auto&O:Observed){auto*C=Cast<UGeometryCollectionComponent>(O.GetComponent(M));auto*A=C?C->GetOwner():nullptr;if(!Live(C)||!Live(A)||!A->Tags.Contains(LearningActorTag)||!A->GetName().StartsWith(TEXT("Chaos_Cache_"))||!Live(C->GetRestCollection())||!IsWithinLearningRoot(C->GetRestCollection()->GetPathName()))return Work->Fail(TEXT("foreign_observed_component"),TEXT("Existing observed components must be owned tagged learning cache targets"));if(!Work->Track(A,TEXT("previous_observed_owner"))||!Work->Track(C,TEXT("previous_observed_component"),A))return false;}
 return Guard();
}
TSharedPtr<FJsonObject> CacheCollectionToJson(UChaosCacheCollection* Collection)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("cache_collection_found"), Collection != nullptr);
    Data->SetStringField(TEXT("cache_collection_path"), Collection ? Collection->GetPathName() : FString());
    if (!Collection)
    {
        return Data;
    }
    if(!Guard())return Data;Work->SDK(TEXT("cache_collection_observation"));if(Collection->GetCaches().Num()>8192){Work->Fail(TEXT("cache_budget"),TEXT("Cache list exceeds8192"));return Data;}for(const auto*Cache:Collection->GetCaches())if(Cache&&(!Live(Cache)||!FMath::IsFinite(Cache->RecordedDuration)||Cache->RecordedDuration<0)){Work->Fail(TEXT("cache_lifetime_lost"),TEXT("Captured cache entry is not live"));return Data;}
    Data->SetNumberField(TEXT("cache_count"), Collection->GetCaches().Num());
    Data->SetNumberField(TEXT("max_recorded_duration"), Collection->GetMaxDuration());
    Data->SetNumberField(TEXT("max_recorded_frames"), Collection->GetMaxNumFrames());
    TArray<TSharedPtr<FJsonValue>> Caches;
    for (const UChaosCache* Cache : Collection->GetCaches())
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Cache ? Cache->GetName() : FString());
        Entry->SetNumberField(TEXT("recorded_duration"), Cache ? Cache->RecordedDuration : 0.0);
        Entry->SetNumberField(TEXT("num_recorded_frames"), Cache ? Cache->NumRecordedFrames : 0);
        Entry->SetNumberField(TEXT("particle_track_count"), Cache ? Cache->ParticleTracks.Num() : 0);
        Caches.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Data->SetArrayField(TEXT("caches"), Caches);
    return Data;
}
TSharedPtr<FJsonObject> ConfigureLearningWorld(
    UWorld* World,
    UStaticMesh* BlockMesh,
    UGeometryCollection* Base,
    UGeometryCollection* Tiny,
    UGeometryCollection* Removal,
    UGeometryCollection* RootProxy,
    UChaosCacheCollection* CacheCollection)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!World || !Guard() || !Work->R.Error.IsEmpty())
    {
        Result->SetBoolField(TEXT("configured"), false);
        return Result;
    }

    int32 TaggedActorCount = 0;
    int32 PlacementMatchCount = 0;
    int32 PlacementAuditCount = 0;
    double MaximumPlacementLocationDelta = 0.0;
    TArray<TSharedPtr<FJsonValue>> PlacementMismatches;
    auto CountActor = [&TaggedActorCount](AActor* Actor)
    {
        TaggedActorCount += Actor != nullptr ? 1 : 0;
    };
    auto AuditPlacement = [&PlacementMatchCount, &PlacementAuditCount, &MaximumPlacementLocationDelta, &PlacementMismatches](AActor* Actor, const FTransform& Expected)
    {
        ++PlacementAuditCount;
        if (!Actor)
        {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("actor_name"), TEXT("missing"));
            Entry->SetArrayField(TEXT("expected_location"), VectorToJson(Expected.GetLocation()));
            PlacementMismatches.Add(MakeShared<FJsonValueObject>(Entry));
            return;
        }
        const FVector ActualLocation = Actor->GetActorLocation();
        const double LocationDelta = FVector::Distance(Expected.GetLocation(), ActualLocation);
        MaximumPlacementLocationDelta = FMath::Max(MaximumPlacementLocationDelta, LocationDelta);
        const bool bMatches = Actor->GetActorTransform().Equals(Expected, 0.01);
        PlacementMatchCount += bMatches ? 1 : 0;
        if (!bMatches)
        {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("actor_name"), Actor->GetName());
            Entry->SetArrayField(TEXT("expected_location"), VectorToJson(Expected.GetLocation()));
            Entry->SetArrayField(TEXT("actual_location"), VectorToJson(ActualLocation));
            Entry->SetArrayField(TEXT("component_location"), VectorToJson(Actor->GetRootComponent() ? Actor->GetRootComponent()->GetComponentLocation() : FVector::ZeroVector));
            Entry->SetNumberField(TEXT("location_delta"), LocationDelta);
            PlacementMismatches.Add(MakeShared<FJsonValueObject>(Entry));
        }
    };
    const FTransform SleepTransform(FVector(-600, 0, 250));
    AActor* SleepActor = EnsureTaggedActor<AActor>(World, TEXT("Chaos_SleepOnly"), SleepTransform);
        if(!SleepActor||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
    CountActor(SleepActor);
    if(!ConfigureGeometryActor(SleepActor, TEXT("GC_SleepOnly"), Removal, true, false, INDEX_NONE, SleepTransform)) { return Result; }
    AuditPlacement(SleepActor, SleepTransform);
    const FTransform BreakTransform(FVector(-200, 0, 250));
    AActor* BreakActor = EnsureTaggedActor<AActor>(World, TEXT("Chaos_BreakOnly"), BreakTransform);
        if(!BreakActor||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
    CountActor(BreakActor);
    if(!ConfigureGeometryActor(BreakActor, TEXT("GC_BreakOnly"), Removal, false, true, INDEX_NONE, BreakTransform)) { return Result; }
    AuditPlacement(BreakActor, BreakTransform);

    const FTransform OneWayDisabledTransform(FVector(200, -250, 250));
    AActor* OneWayDisabled = EnsureTaggedActor<AActor>(World, TEXT("Chaos_OneWay_Minus1"), OneWayDisabledTransform);
        if(!OneWayDisabled||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
    CountActor(OneWayDisabled);
    if(!ConfigureGeometryActor(OneWayDisabled, TEXT("GC_OneWay_Minus1"), Base, false, false, -1, OneWayDisabledTransform)) { return Result; }
    AuditPlacement(OneWayDisabled, OneWayDisabledTransform);
    const FTransform OneWayLevel2Transform(FVector(200, 250, 250));
    AActor* OneWayLevel2 = EnsureTaggedActor<AActor>(World, TEXT("Chaos_OneWay_Level2"), OneWayLevel2Transform);
        if(!OneWayLevel2||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
    CountActor(OneWayLevel2);
    if(!ConfigureGeometryActor(OneWayLevel2, TEXT("GC_OneWay_Level2"), Base, false, false, 2, OneWayLevel2Transform)) { return Result; }
    AuditPlacement(OneWayLevel2, OneWayLevel2Transform);

    const FTransform FloorTransform(FRotator::ZeroRotator, FVector(0, 0, -50), FVector(20, 20, 1));
    AStaticMeshActor* Floor = EnsureTaggedActor<AStaticMeshActor>(World, TEXT("Chaos_StaticFloor"), FloorTransform);
        if(!Floor||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
    CountActor(Floor);
    if (Floor)
    {
        auto* FloorComponent=Floor->GetStaticMeshComponent();if(!FloorComponent||!Work->Track(FloorComponent,TEXT("learning_floor_component"),Floor)) { return Result; }
        if(!Step(TEXT("SetStaticMesh"),[&]{FloorComponent->SetStaticMesh(BlockMesh);})) { return Result; }
        if(!Step(TEXT("SetMobility"),[&]{FloorComponent->SetMobility(EComponentMobility::Static);})) { return Result; }
        if(!Step(TEXT("SetCollisionProfileName"),[&]{FloorComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);})) { return Result; }
        if(!Step(TEXT("SetCollisionResponseToChannel"),[&]{FloorComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);})) { return Result; }
        if(!Step(TEXT("SetSimulatePhysics"),[&]{FloorComponent->SetSimulatePhysics(false);})) { return Result; }
    }
    AuditPlacement(Floor, FloorTransform);
    for (int32 WitnessIndex = 0; WitnessIndex < 2; ++WitnessIndex)
    {
        const FString Name = FString::Printf(TEXT("Chaos_OneWay_Witness_%d"), WitnessIndex);
        const FTransform WitnessTransform(FVector(400, WitnessIndex == 0 ? -250 : 250, 385));
        AStaticMeshActor* Witness = EnsureTaggedActor<AStaticMeshActor>(World, Name, WitnessTransform);
        if(!Witness||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
        CountActor(Witness);
        if (Witness)
        {
            UStaticMeshComponent* MeshComponent = Witness->GetStaticMeshComponent();if(!MeshComponent||!Work->Track(MeshComponent,TEXT("learning_witness_component"),Witness)) { return Result; }
            if(!Step(TEXT("SetStaticMesh"),[&]{MeshComponent->SetStaticMesh(BlockMesh);})) { return Result; }
            if(!Step(TEXT("SetMobility"),[&]{MeshComponent->SetMobility(EComponentMobility::Movable);})) { return Result; }
            if(!Step(TEXT("SetCollisionProfileName"),[&]{MeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);})) { return Result; }
            if(!Step(TEXT("SetCollisionResponseToChannel"),[&]{MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);})) { return Result; }
            if(!Step(TEXT("SetSimulatePhysics"),[&]{MeshComponent->SetSimulatePhysics(true);})) { return Result; }
            if(!Step(TEXT("SetEnableGravity"),[&]{MeshComponent->SetEnableGravity(false);})) { return Result; }
            if(!Step(TEXT("SetMassOverrideInKg"),[&]{MeshComponent->SetMassOverrideInKg(NAME_None, 50.0f, true);})) { return Result; }
            if(!Step(TEXT("CCD"),[&]{MeshComponent->BodyInstance.bUseCCD = true;})) { return Result; }
            if(!Step(TEXT("SetUseCCD"),[&]{MeshComponent->SetUseCCD(true);})) { return Result; }
        }
        AuditPlacement(Witness, WitnessTransform);
    }

    int32 RootProxyActorCount = 0;
    for (int32 Index = 0; Index < 32; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Chaos_RootProxy_%02d"), Index);
        const FVector Location(1000 + (Index % 8) * 350, -700 + (Index / 8) * 450, 250);
        AActor* Actor = EnsureTaggedActor<AActor>(World, Name, FTransform(Location));
        if(!Actor||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
        if (ConfigureGeometryActor(Actor, TEXT("GC_RootProxy"), RootProxy, false, false, INDEX_NONE, FTransform(Location)))
        {
            ++RootProxyActorCount;
        }
        if(!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
        CountActor(Actor);
        AuditPlacement(Actor, FTransform(Location));
    }

    int32 BaseInstanceCount = 0;
    for (int32 Index = 0; Index < 32; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Chaos_Base_%02d"), Index);
        const FVector Location(1000 + (Index % 8) * 350, -3000 + (Index / 8) * 450, 250);
        AActor* Actor = EnsureTaggedActor<AActor>(World, Name, FTransform(Location));
        if(!Actor||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
        if (ConfigureGeometryActor(Actor, TEXT("GC_Base"), Base, false, false, INDEX_NONE, FTransform(Location)))
        {
            ++BaseInstanceCount;
        }
        CountActor(Actor);
        AuditPlacement(Actor, FTransform(Location));
    }

    int32 TinyOptimizedInstanceCount = 0;
    for (int32 Index = 0; Index < 32; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Chaos_TinyOptimized_%02d"), Index);
        const FVector Location(1000 + (Index % 8) * 350, -5200 + (Index / 8) * 450, 250);
        AActor* Actor = EnsureTaggedActor<AActor>(World, Name, FTransform(Location));
        if(!Actor||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
        if (ConfigureGeometryActor(Actor, TEXT("GC_TinyOptimized"), Tiny, false, false, INDEX_NONE, FTransform(Location)))
        {
            ++TinyOptimizedInstanceCount;
        }
        CountActor(Actor);
        AuditPlacement(Actor, FTransform(Location));
    }

    int32 ThrottleInstanceCount = 0;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Chaos_Throttle_%02d"), Index);
        const FVector Location(-1600 + Index * 350, -1400, 250);
        AActor* Actor = EnsureTaggedActor<AActor>(World, Name, FTransform(Location));
        if(!Actor||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
        if (ConfigureGeometryActor(Actor, TEXT("GC_Throttle"), Base, false, false, INDEX_NONE, FTransform(Location)))
        {
            ++ThrottleInstanceCount;
        }
        CountActor(Actor);
        AuditPlacement(Actor, FTransform(Location));
    }

    AChaosSolverActor* Solver = EnsureTaggedActor<AChaosSolverActor>(World, TEXT("Chaos_LearningSolver"), FTransform::Identity);
        if(!Solver||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
    CountActor(Solver);
    AuditPlacement(Solver, FTransform::Identity);
    AChaosCacheManager* CacheManager = EnsureTaggedActor<AChaosCacheManager>(World, TEXT("Chaos_LearningCacheManager"), FTransform::Identity);
        if(!CacheManager||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
    CountActor(CacheManager);
    AuditPlacement(CacheManager, FTransform::Identity);
    int32 CacheInstanceCount = 0;
    if (CacheManager)
    {
        if(!GuardLearningObserved(CacheManager)) { return Result; }
        if(!EndManagerEvaluation(CacheManager,World,TEXT("EndEvaluate_fixture_manager"))) { return Result; }
        if(!Step(TEXT("assign_fixture_manager"),[&]{CacheManager->CacheCollection = CacheCollection;CacheManager->CacheMode = ECacheMode::None;CacheManager->StartMode = EStartMode::Triggered;})) { return Result; }
        if(!Step(TEXT("ClearObservedComponents"),[&]{CacheManager->ClearObservedComponents(false);})) { return Result; }
    }
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Chaos_Cache_%02d"), Index);
        const FVector Location(-1600 + (Index % 8) * 350, 900 + (Index / 8) * 450, 250);
        AActor* Actor = EnsureTaggedActor<AActor>(World, Name, FTransform(Location));
        if(!Actor||!Guard()||!Work->R.Error.IsEmpty()) { return Result; }
        UGeometryCollectionComponent* Component = ConfigureGeometryActor(Actor, TEXT("GC_Cache"), Base, false, false, INDEX_NONE, FTransform(Location));
        if (CacheManager && Component)
        {
            if(!Step(TEXT("FindOrAddObservedComponent"),[&]{CacheManager->FindOrAddObservedComponent(Component, FName(*FString::Printf(TEXT("ChaosLearning_%02d"), Index)), true);})) { return Result; }
            ++CacheInstanceCount;
        }
        CountActor(Actor);
        AuditPlacement(Actor, FTransform(Location));
    }

    int32 SolverBoundComponentCount = 0;
    int32 WorldSolverOneWayComponentCount = 0;
    if (Solver)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->Tags.Contains(LearningActorTag))
            {
                continue;
            }
            if (UGeometryCollectionComponent* Component = It->FindComponentByClass<UGeometryCollectionComponent>())
            {
                const bool bOneWayFixture = It->GetName().StartsWith(TEXT("Chaos_OneWay_"));
                if(!Work->Track(Component,TEXT("solver_bound_component"),Component->GetOwner())||!Step(TEXT("assign_learning_solver"),[&]{Component->ChaosSolverActor = bOneWayFixture ? nullptr : Solver;})) { return Result; }
                if (bOneWayFixture)
                {
                    ++WorldSolverOneWayComponentCount;
                }
                else
                {
                    ++SolverBoundComponentCount;
                }
            }
        }
    }

    if(!Step(TEXT("dirty_learning_world"),[&]{World->MarkPackageDirty();})) { return Result; }
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetNumberField(TEXT("tagged_actor_count"), TaggedActorCount);
    Result->SetNumberField(TEXT("root_proxy_instance_count"), RootProxyActorCount);
    Result->SetNumberField(TEXT("base_instance_count"), BaseInstanceCount);
    Result->SetNumberField(TEXT("tiny_optimized_instance_count"), TinyOptimizedInstanceCount);
    Result->SetNumberField(TEXT("throttle_instance_count"), ThrottleInstanceCount);
    Result->SetNumberField(TEXT("cache_instance_count"), CacheInstanceCount);
    Result->SetNumberField(TEXT("solver_bound_component_count"), SolverBoundComponentCount);
    Result->SetNumberField(TEXT("world_solver_one_way_component_count"),
        WorldSolverOneWayComponentCount);
    Result->SetNumberField(TEXT("placement_audit_count"), PlacementAuditCount);
    Result->SetNumberField(TEXT("placement_match_count"), PlacementMatchCount);
    Result->SetNumberField(TEXT("placement_mismatch_count"), PlacementAuditCount - PlacementMatchCount);
    Result->SetNumberField(TEXT("maximum_placement_location_delta"), MaximumPlacementLocationDelta);
    Result->SetArrayField(TEXT("placement_mismatches"), PlacementMismatches);
    Result->SetBoolField(TEXT("sleep_only_flags"), SleepActor && SleepActor->FindComponentByClass<UGeometryCollectionComponent>() && SleepActor->FindComponentByClass<UGeometryCollectionComponent>()->bAllowRemovalOnSleep && !SleepActor->FindComponentByClass<UGeometryCollectionComponent>()->bAllowRemovalOnBreak);
    Result->SetBoolField(TEXT("break_only_flags"), BreakActor && BreakActor->FindComponentByClass<UGeometryCollectionComponent>() && !BreakActor->FindComponentByClass<UGeometryCollectionComponent>()->bAllowRemovalOnSleep && BreakActor->FindComponentByClass<UGeometryCollectionComponent>()->bAllowRemovalOnBreak);
    Result->SetBoolField(TEXT("static_floor_present"), Floor != nullptr);
    Result->SetBoolField(TEXT("solver_present"), Solver != nullptr);
    Result->SetBoolField(TEXT("cache_manager_present"), CacheManager != nullptr);
    return Result;
}
TSharedPtr<FJsonObject> HandleCreateLearningFixture(const TSharedPtr<FJsonObject>& Params)
{
    const FString Root = GetStringParam(Params, TEXT("asset_root"), LearningRoot);
    FString Error;
    if (!ValidateLearningRoot(Root, Error))
    {
        return FLegacy::CreateErrorResponse(Error);
    }
    if (IsPlaySessionActive())
    {
        return FLegacy::CreateErrorResponse(TEXT("Fixture authoring is blocked while PIE/SIE is active"));
    }

    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), true);
    const bool bForceRebuild = GetBoolParam(Params, TEXT("force_rebuild"), false);
    const bool bSaveAssets = GetBoolParam(Params, TEXT("save_assets"), true);
    Work->R.bSaveRequested=bSaveAssets;
    const bool bConfigureCurrentLevel = GetBoolParam(Params, TEXT("configure_current_level"), false);
    const FString LevelPath = Root / TEXT("L_ChaosDestructionLearning");
    UWorld* CurrentEditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if(CurrentEditorWorld&&!Work->CaptureWorld(CurrentEditorWorld))return FLegacy::CreateErrorResponse(Work->R.Error);
    if (bConfigureCurrentLevel &&
        (!CurrentEditorWorld || CurrentEditorWorld->GetOutermost()->GetName() != LevelPath))
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("configure_current_level requires the current native editor world package to be exactly %s; use safe_new_preview_map first"),
            *LevelPath));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("asset_root"), Root);
    Data->SetBoolField(TEXT("dry_run"), bDryRun);
    Data->SetBoolField(TEXT("force_rebuild"), bForceRebuild);
    Data->SetBoolField(TEXT("configure_current_level"), bConfigureCurrentLevel);
    Data->SetStringField(TEXT("required_current_level_path"), LevelPath);
    Data->SetStringField(TEXT("current_editor_world_path"), CurrentEditorWorld ? CurrentEditorWorld->GetOutermost()->GetName() : FString());
    Data->SetBoolField(TEXT("pie_active"), false);
    TArray<FString> PlannedPaths;
    for (const TCHAR* Name : AssetNames)
    {
        PlannedPaths.Add(Root / Name);
    }
    if (bConfigureCurrentLevel)
    {
        PlannedPaths.Add(LevelPath);
    }
    Data->SetArrayField(TEXT("planned_asset_paths"), StringsToJson(PlannedPaths));
    Data->SetObjectField(TEXT("dirty_packages_before"), CaptureDirtyStates(PlannedPaths));
    if (bDryRun)
    {
        Data->SetStringField(TEXT("operation_status"), TEXT("DRY_RUN"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("NOT_RUN"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Dry-run only; no fixture was authored and no five-run A/B measurement was performed"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (bForceRebuild && CurrentEditorWorld)
    {
        TArray<FString> ReferencingComponents;
        for (TActorIterator<AActor> It(CurrentEditorWorld); It; ++It)
        {
            TArray<UGeometryCollectionComponent*> Components;
            It->GetComponents<UGeometryCollectionComponent>(Components);
            for (UGeometryCollectionComponent* Component : Components)
            {
                const UGeometryCollection* RestCollection = Component ? Component->GetRestCollection() : nullptr;
                if (RestCollection && IsWithinLearningRoot(RestCollection->GetPathName()))
                {
                    ReferencingComponents.Add(Component->GetPathName());
                }
            }
        }
        if (!ReferencingComponents.IsEmpty())
        {
            return FLegacy::CreateErrorResponse(FString::Printf(
                TEXT("force_rebuild is unsafe while the current editor world has %d live Geometry Collection components referencing the learning assets; open an empty control map first (examples: %s)"),
                ReferencingComponents.Num(),
                *FString::Join(ReferencingComponents, TEXT(", "))));
        }
    }

    RetireExpiredWorldMetadata();Work->Writer=true;if(!Guard()) { return FLegacy::CreateErrorResponse(Work->R.Error); }
    for(int32 I=0;I<7;++I){UObject* Existing=Work->Load<UObject>(ObjectPath(PlannedPaths[I]),TEXT("preflight_fixture_type"));if(!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);const UClass* Expected=I<2?UStaticMesh::StaticClass():(I<6?UGeometryCollection::StaticClass():UChaosCacheCollection::StaticClass());if(Existing&&!Existing->IsA(Expected))return FLegacy::CreateErrorResponse(TEXT("Existing fixture asset has an incompatible class"));}
    UStaticMesh* SourceCube = Work->Load<UStaticMesh>(SourceCubePath,TEXT("load_source_cube"));
    if (!SourceCube)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(TEXT("Unable to load source cube: %s"), SourceCubePath));
    }

    const FString SourcePackageBefore=SourcePackageMD5(SourceCube);const FString SourceProjectionBefore=Json(ObjectProjection(SourceCube));Work->R.Data->SetStringField(TEXT("source_cube_package_md5_before"),SourcePackageBefore);Work->R.Data->SetStringField(TEXT("source_cube_projection_before"),SourceProjectionBefore);if(SourcePackageBefore.IsEmpty())return FLegacy::CreateErrorResponse(TEXT("Cannot verify the fixed source cube package within the64MiB read budget"));
    Work->Event(TEXT("fixture_transaction_begin"));ON_SCOPE_EXIT{Work->Count(TEXT("fixture_transaction_scope_returns"));};
    const FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP_UE58", "CreateChaosLearningFixture", "Create Chaos Destruction Learning Fixture"));
    if(!Guard())return FLegacy::CreateErrorResponse(Work->R.Error);
    TArray<FString> CreatedPaths;
    TArray<FString> UpdatedPaths;
    auto Track = [&CreatedPaths, &UpdatedPaths](const FString& Path, bool bCreated)
    {
        (bCreated ? CreatedPaths : UpdatedPaths).Add(Path);
        Work->R.Data->SetArrayField(TEXT("created_asset_paths"),StringsToJson(CreatedPaths));Work->R.Data->SetArrayField(TEXT("updated_or_reused_asset_paths"),StringsToJson(UpdatedPaths));
    };

    bool bCreated = false;
    const FString BlockPath = Root / TEXT("SM_ChaosTestBlock");
    UStaticMesh* BlockMesh = EnsureDuplicatedStaticMesh(BlockPath, SourceCube, bCreated);
    if (!BlockMesh)
    {
        return FLegacy::CreateErrorResponse(TEXT("Failed to create/load SM_ChaosTestBlock"));
    }
    Track(BlockPath, bCreated);

    const FString ProxyPath = Root / TEXT("SM_ChaosRootProxy");
    UStaticMesh* ProxyMesh = EnsureDuplicatedStaticMesh(ProxyPath, SourceCube, bCreated);
    if (!ProxyMesh)
    {
        return FLegacy::CreateErrorResponse(TEXT("Failed to create/load SM_ChaosRootProxy"));
    }
    Track(ProxyPath, bCreated);

    const FString BasePath = Root / TEXT("GC_ChaosBase_96");
    UGeometryCollection* Base = EnsureGeometryCollection(BasePath, bCreated);
    if (!Base)
    {
        return FLegacy::CreateErrorResponse(TEXT("Failed to create/load GC_ChaosBase_96"));
    }
    Track(BasePath, bCreated);
    if (bCreated || bForceRebuild || Base->IsEmpty())
    {
        BuildBaseCollection(Base, BlockMesh);
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
    }

    const FString TinyPath = Root / TEXT("GC_ChaosTinyOptimized");
    UGeometryCollection* Tiny = EnsureGeometryCollection(TinyPath, bCreated);
    if(!Tiny||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error.IsEmpty()?TEXT("Failed to create/load GC_ChaosTinyOptimized"):Work->R.Error);
    Track(TinyPath, bCreated);
    if (bCreated || bForceRebuild || Tiny->IsEmpty())
    {
        CopyCollectionFrom(Tiny, Base);
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
        FDataflowTransformSelection Selection;
        Selection.Initialize(Tiny->NumElements(FGeometryCollection::TransformGroup), false);
        if(!Step(TEXT("FixTinyGeo"),[&]{FFractureEngineUtility::FixTinyGeo(
            *Tiny->GetGeometryCollection(),
            Selection,
            EFixTinyGeoMergeType::MergeGeometry,
            false,
            EFixTinyGeoGeometrySelectionMethod::RelativeVolume,
            2.0f,
            0.04f,
            EFixTinyGeoUseBoneSelection::NoEffect,
            false,
            EFixTinyGeoNeighborSelectionMethod::LargestNeighbor,
            false,
            false,
            true);}))return FLegacy::CreateErrorResponse(Work->R.Error);
        FinalizeGeometryCollection(Tiny);
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
    }

    const FString RemovalPath = Root / TEXT("GC_ChaosRemoval");
    UGeometryCollection* Removal = EnsureGeometryCollection(RemovalPath, bCreated);
    if(!Removal||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error.IsEmpty()?TEXT("Failed to create/load GC_ChaosRemoval"):Work->R.Error);
    Track(RemovalPath, bCreated);
    if (bCreated || bForceRebuild || Removal->IsEmpty())
    {
        CopyCollectionFrom(Removal, Base);
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
        if(!Step(TEXT("configure_removal"),[&]{
        Removal->bRemoveOnMaxSleep = true;
        Removal->MaximumSleepTime = FVector2D(1.0, 1.0);
        Removal->RemovalDuration = FVector2D(0.5, 0.5);
        Removal->bSlowMovingAsSleeping = true;
        Removal->SlowMovingVelocityThreshold = 2.0f;
        GeometryCollection::Facades::FCollectionRemoveOnBreakFacade Facade(*Removal->GetGeometryCollection());
        Facade.DefineSchema();
        TArray<int32> RemovalIndices = MeasureCollection(Removal, 0.04).TinyCandidateIndices;
        RemovalIndices.SetNum(FMath::Min(24, RemovalIndices.Num()));
        Facade.SetFromIndexArray(
            RemovalIndices,
            GeometryCollection::Facades::FRemoveOnBreakData(true, FVector2f(0.5f, 0.5f), false, FVector2f(0.5f, 0.5f)));
        }))return FLegacy::CreateErrorResponse(Work->R.Error);
        FinalizeGeometryCollection(Removal);
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
    }

    const FString RootProxyPath = Root / TEXT("GC_ChaosRootProxy");
    UGeometryCollection* RootProxy = EnsureGeometryCollection(RootProxyPath, bCreated);
    if(!RootProxy||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error.IsEmpty()?TEXT("Failed to create/load GC_ChaosRootProxy"):Work->R.Error);
    Track(RootProxyPath, bCreated);
    if (bCreated || bForceRebuild || RootProxy->IsEmpty())
    {
        CopyCollectionFrom(RootProxy, Base);
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
        if(!Step(TEXT("configure_root_proxy"),[&]{
        RootProxy->RootProxyData.ProxyMeshes = {ProxyMesh};
        RootProxy->RootProxyData.MeshTransforms = {FTransform3f(FTransform(FRotator::ZeroRotator, FVector(0, 0, 306), FVector(8.2, 2.1, 6.2)))};
        RootProxy->CustomRendererType = UGeometryCollectionRootProxyRenderer::StaticClass();
        }))return FLegacy::CreateErrorResponse(Work->R.Error);
        FinalizeGeometryCollection(RootProxy);
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
    }

    const FString CachePath = Root / TEXT("CC_ChaosLearning");
    UChaosCacheCollection* CacheCollection = EnsureCacheCollection(CachePath, bCreated);
    if (!CacheCollection)
    {
        return FLegacy::CreateErrorResponse(TEXT("Failed to create/load CC_ChaosLearning"));
    }
    Track(CachePath, bCreated);
    if (bCreated || bForceRebuild)
    {
        if(!Step(TEXT("modify_learning_cache"),[&]{CacheCollection->Modify();})||!Step(TEXT("reset_learning_cache"),[&]{CacheCollection->Caches.Reset();})||!Step(TEXT("dirty_learning_cache"),[&]{CacheCollection->MarkPackageDirty();}))return FLegacy::CreateErrorResponse(Work->R.Error);
    }

    TSharedPtr<FJsonObject> LearningWorldConfiguration = MakeShared<FJsonObject>();
    LearningWorldConfiguration->SetBoolField(TEXT("configured"), false);
    LearningWorldConfiguration->SetBoolField(TEXT("configure_current_level_requested"), bConfigureCurrentLevel);
    LearningWorldConfiguration->SetStringField(TEXT("required_level_path"), LevelPath);
    if (bConfigureCurrentLevel)
    {
        LearningWorldConfiguration = ConfigureLearningWorld(
            CurrentEditorWorld, BlockMesh, Base, Tiny, Removal, RootProxy, CacheCollection);
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
        LearningWorldConfiguration->SetBoolField(TEXT("configure_current_level_requested"), true);
        LearningWorldConfiguration->SetStringField(TEXT("configured_level_path"), CurrentEditorWorld->GetOutermost()->GetName());
        Track(LevelPath, false);
    }
    else
    {
        LearningWorldConfiguration->SetStringField(
            TEXT("configuration_blocker"),
            TEXT("Asset-only fixture created; use safe_new_preview_map then call again with configure_current_level=true"));
    }

    if(!CollectionSchema(Base)||!CollectionSchema(Tiny)||!CollectionSchema(Removal)||!CollectionSchema(RootProxy))return FLegacy::CreateErrorResponse(Work->R.Error);
    const FCollectionMetrics BaseMetrics = MeasureCollection(Base, 0.04);
    const FCollectionMetrics TinyMetrics = MeasureCollection(Tiny, 0.04);
    const double ConvexVolumeDeltaPercent = PercentDelta(BaseMetrics.TotalConvexVolume, TinyMetrics.TotalConvexVolume);
    const double BoundsVolumeDeltaPercent = PercentDelta(
        BaseMetrics.OverallWorldBounds.IsValid ? BaseMetrics.OverallWorldBounds.GetVolume() : 0.0,
        TinyMetrics.OverallWorldBounds.IsValid ? TinyMetrics.OverallWorldBounds.GetVolume() : 0.0);
    const TSharedPtr<FJsonObject> RemovalInspection = InspectCollection(Removal, nullptr, 0.04);
    const int32 RemovalEnabledCount = static_cast<int32>(RemovalInspection->GetNumberField(TEXT("remove_on_break_enabled_count")));
    bool bProxyArraysValid = RootProxy->RootProxyData.ProxyMeshes.Num() > 0 &&
        RootProxy->RootProxyData.ProxyMeshes.Num() == RootProxy->RootProxyData.MeshTransforms.Num();
    for (int32 ProxyIndex = 0; ProxyIndex < RootProxy->RootProxyData.ProxyMeshes.Num() && bProxyArraysValid; ++ProxyIndex)
    {
        const FVector3f ProxyScale = RootProxy->RootProxyData.MeshTransforms[ProxyIndex].GetScale3D();
        bProxyArraysValid = RootProxy->RootProxyData.ProxyMeshes[ProxyIndex] != nullptr &&
            !RootProxy->RootProxyData.MeshTransforms[ProxyIndex].ContainsNaN() &&
            !ProxyScale.IsNearlyZero();
    }
    const bool bBaseIntegrity = BaseMetrics.RigidCount == 96 && BaseMetrics.RootCount == 1 &&
        BaseMetrics.TinyCandidateIndices.Num() >= 24 &&
        BaseMetrics.TinyCandidateWithContactCount == BaseMetrics.TinyCandidateIndices.Num() &&
        BaseMetrics.InvalidParentCount == 0 && BaseMetrics.ParentChildMismatchCount == 0 && BaseMetrics.CycleCount == 0 &&
        BaseMetrics.InvalidTransformToGeometryCount == 0 && BaseMetrics.InvalidTriangleIndexCount == 0 &&
        BaseMetrics.BoundaryEdgeCount == 0 && BaseMetrics.NonManifoldEdgeCount == 0;
    const bool bTinyIntegrity = BaseMetrics.TinyCandidateIndices.Num() >= 12 &&
        TinyMetrics.RigidCount < BaseMetrics.RigidCount && TinyMetrics.GeometryCount < BaseMetrics.GeometryCount &&
        ConvexVolumeDeltaPercent <= 1.0 && BoundsVolumeDeltaPercent <= 1.0;
    const bool bRemovalIntegrity = RemovalEnabledCount == 24 && BaseMetrics.TinyCandidateIndices.Num() >= 24;
    const bool bRootProxyIntegrity = !RootProxy->EnableNanite &&
        RootProxy->CustomRendererType == UGeometryCollectionRootProxyRenderer::StaticClass() && bProxyArraysValid;
    const bool bWorldIntegrity = bConfigureCurrentLevel && LearningWorldConfiguration->GetBoolField(TEXT("configured")) &&
        LearningWorldConfiguration->GetNumberField(TEXT("root_proxy_instance_count")) == 32.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("base_instance_count")) == 32.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("tiny_optimized_instance_count")) == 32.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("throttle_instance_count")) == 8.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("cache_instance_count")) == 16.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("solver_bound_component_count")) >= 122.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("world_solver_one_way_component_count")) == 2.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("placement_audit_count")) == 129.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("placement_match_count")) == 129.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("placement_mismatch_count")) == 0.0 &&
        LearningWorldConfiguration->GetNumberField(TEXT("maximum_placement_location_delta")) <= 0.01 &&
        LearningWorldConfiguration->GetBoolField(TEXT("sleep_only_flags")) &&
        LearningWorldConfiguration->GetBoolField(TEXT("break_only_flags")) &&
        LearningWorldConfiguration->GetBoolField(TEXT("static_floor_present")) &&
        LearningWorldConfiguration->GetBoolField(TEXT("solver_present")) &&
        LearningWorldConfiguration->GetBoolField(TEXT("cache_manager_present"));
    const bool bWorldGateSatisfied = !bConfigureCurrentLevel || bWorldIntegrity;
    bool bStaticAssertionsPassed = bBaseIntegrity && bTinyIntegrity && bRemovalIntegrity && bRootProxyIntegrity && bWorldGateSatisfied;

    Data->SetObjectField(TEXT("dirty_packages_after_mutation"), CaptureDirtyStates(PlannedPaths));

    if (bSaveAssets)
    {
        for (const TCHAR* Name : AssetNames)
        {
            const FString Path = Root / Name;
            UObject* SavedAsset=FindObject<UObject>(nullptr,*ObjectPath(Path));
            if (!SaveExact(SavedAsset,Path))
            {
                return FLegacy::CreateErrorResponse(FString::Printf(TEXT("Failed to save fixture asset: %s"), *Path));
            }
        }
        if (bConfigureCurrentLevel && !SaveExact(CurrentEditorWorld,LevelPath,true))
        {
            return FLegacy::CreateErrorResponse(FString::Printf(TEXT("Failed to save configured current learning level: %s"), *LevelPath));
        }
    }

    const TSharedPtr<FJsonObject> DependencyAudit = AuditAssetDependencies(PlannedPaths);
    if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
    const FString SourcePackageAfter=SourcePackageMD5(SourceCube);const FString SourceProjectionAfter=Json(ObjectProjection(SourceCube));const bool SourceUnchanged=!SourcePackageBefore.IsEmpty()&&SourcePackageBefore.Equals(SourcePackageAfter,ESearchCase::CaseSensitive)&&SourceProjectionBefore.Equals(SourceProjectionAfter,ESearchCase::CaseSensitive);Work->R.Data->SetStringField(TEXT("source_cube_package_md5_after"),SourcePackageAfter);Work->R.Data->SetStringField(TEXT("source_cube_projection_after"),SourceProjectionAfter);Work->R.Data->SetBoolField(TEXT("source_cube_package_and_projection_unchanged"),SourceUnchanged);Work->R.Data->SetStringField(TEXT("source_cube_invariance_scope"),TEXT("Persisted package MD5 plus class/path/package/dirty/lightingGuid/sourceModelCount/bounds/material-path projection; no source Modify/Save/geometry writer. This is not a complete in-memory mesh-byte proof."));
    const bool bDependencyAuditPassed = DependencyAudit->GetNumberField(TEXT("forbidden_mcp_dependency_count")) == 0.0;
    const bool bExactBoundaryPassed = PlannedPaths.ContainsByPredicate([](const FString& Path) { return !IsWithinLearningRoot(Path); }) == false;
    bStaticAssertionsPassed = bStaticAssertionsPassed && bDependencyAuditPassed && bExactBoundaryPassed && SourceUnchanged;

    Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
    Data->SetStringField(TEXT("assertion_status"), bStaticAssertionsPassed ? TEXT("PASS") : TEXT("FAIL"));
    Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
    Data->SetStringField(TEXT("learning_blocker"), TEXT("Fixture authoring succeeded, but a five-run A/B performance measurement is still required"));
    Data->SetArrayField(TEXT("created_asset_paths"), StringsToJson(CreatedPaths));
    Data->SetArrayField(TEXT("updated_or_reused_asset_paths"), StringsToJson(UpdatedPaths));
    Data->SetBoolField(TEXT("saved"), bSaveAssets);
    Data->SetNumberField(TEXT("base_rigid_count"), BaseMetrics.RigidCount);
    Data->SetNumberField(TEXT("tiny_optimized_rigid_count"), TinyMetrics.RigidCount);
    Data->SetBoolField(TEXT("tiny_piece_count_reduced"), TinyMetrics.RigidCount < BaseMetrics.RigidCount);
    Data->SetObjectField(TEXT("tiny_pre_metrics"), MetricsToJson(BaseMetrics));
    Data->SetObjectField(TEXT("tiny_post_metrics"), MetricsToJson(TinyMetrics));
    Data->SetNumberField(TEXT("tiny_convex_volume_delta_percent"), ConvexVolumeDeltaPercent);
    Data->SetNumberField(TEXT("tiny_world_bounds_volume_delta_percent"), BoundsVolumeDeltaPercent);
    Data->SetBoolField(TEXT("base_integrity_assertion"), bBaseIntegrity);
    Data->SetBoolField(TEXT("tiny_merge_assertion"), bTinyIntegrity);
    Data->SetBoolField(TEXT("remove_on_break_small_24_assertion"), bRemovalIntegrity);
    Data->SetBoolField(TEXT("root_proxy_assertion"), bRootProxyIntegrity);
    Data->SetBoolField(TEXT("learning_world_assertion"), bWorldIntegrity);
    Data->SetBoolField(TEXT("learning_world_gate_satisfied"), bWorldGateSatisfied);
    Data->SetBoolField(TEXT("exact_temp_boundary_assertion"), bExactBoundaryPassed);
    Data->SetBoolField(TEXT("forbidden_mcp_dependency_assertion"), bDependencyAuditPassed);
    Data->SetObjectField(TEXT("learning_world_configuration"), LearningWorldConfiguration);
    Data->SetBoolField(TEXT("root_proxy_arrays_valid"), bProxyArraysValid);
    Data->SetStringField(TEXT("root_proxy_custom_renderer"), RootProxy->CustomRendererType ? RootProxy->CustomRendererType->GetPathName() : FString());
    Data->SetObjectField(TEXT("base_inspection"), InspectCollection(Base, nullptr, 0.04));
    Data->SetObjectField(TEXT("tiny_inspection"), InspectCollection(Tiny, nullptr, 0.04));
    Data->SetObjectField(TEXT("removal_inspection"), RemovalInspection);
    Data->SetObjectField(TEXT("root_proxy_inspection"), InspectCollection(RootProxy, nullptr, 0.04));
    Data->SetObjectField(TEXT("cache_inspection"), CacheCollectionToJson(CacheCollection));
    Data->SetBoolField(TEXT("contains_unreal_mcp_asset_types"), !bDependencyAuditPassed);
    Data->SetObjectField(TEXT("dirty_packages_after_save"), CaptureDirtyStates(PlannedPaths));
    Data->SetObjectField(TEXT("asset_dependency_audit"), DependencyAudit);
    return FLegacy::CreateSuccessResponse(Data);
}

struct FCachePlaybackValidationSession
{
 FString RequestedManagerSelector,RequestedCachePath;TSharedPtr<FWork> Evidence;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AChaosCacheManager> Manager;TWeakObjectPtr<UChaosCacheCollection> Collection;uint64 BeginFrame=0;bool bHashesMatch=false,bHashesVaryAcrossTimes=false,bCacheDataGate=false,bIsolationPassed=false;uint32 Hashes[2][3]={};double StateHashCaptureMilliseconds=0;uint32 TimingHash=0;int32 ForcedKinematicObjectCount=0;
};
struct FCacheRecordMotionSession
{
 FString RequestedManagerSelector,RequestedCachePath;TSharedPtr<FWork> Evidence;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AChaosCacheManager> Manager;TArray<TWeakObjectPtr<UGeometryCollectionComponent>> Components;TArray<FVector> Velocities;uint64 BeginFrame=0;double BeginWorldTime=0,Deadline=0;int32 CapturedFrameCount=0,TotalVelocityAppliedObjectCount=0;bool bCaptureCompleted=false;FString CompletionReason;FTSTicker::FDelegateHandle TickerHandle;
};
struct FCacheConfigureSession
{
 FString RequestedManagerSelector,RequestedCachePath;TSharedPtr<FWork> Evidence;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AChaosCacheManager> Manager;TWeakObjectPtr<UChaosCacheCollection> InputCollection;TArray<TWeakObjectPtr<UGeometryCollectionComponent>> Components;TArray<FString> ComponentActorNames;TArray<FName> CacheNames;ECacheMode RequestedMode=ECacheMode::None;uint64 EndEvaluateFrame=0,ClearObservedComponentsFrame=0;double Deadline=0;bool bManagerResetCompleted=false,bPreservedObservedComponents=false,bCompleted=false,bSucceeded=false;FString Failure;FTSTicker::FDelegateHandle TickerHandle;
};
struct FCacheConfigurationToken
{
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AChaosCacheManager> Manager;TWeakObjectPtr<UChaosCacheCollection> Collection;TArray<TWeakObjectPtr<UGeometryCollectionComponent>> Components;ECacheMode Mode=ECacheMode::None;uint64 CompletionFrame=0;
};
TSharedPtr<FCacheConfigureSession> ActiveCacheConfigureSession;
TSharedPtr<FCacheRecordMotionSession> ActiveCacheRecordMotionSession;
TSharedPtr<FCachePlaybackValidationSession> ActiveCachePlaybackSession;
TSharedPtr<FCacheConfigurationToken> ActiveCacheConfigurationToken;
TSharedPtr<FWork> ActiveCacheEvidence;
TSharedPtr<FJsonObject> LastSessionExecution;

bool TrackLearningComponent(UGeometryCollectionComponent*C)
{
 auto*A=C?C->GetOwner():nullptr;const auto*R=C?C->GetRestCollection():nullptr;
 if(!Live(C)||!Live(A)||!A->Tags.Contains(LearningActorTag)||!Live(R)||!IsWithinLearningRoot(R->GetPathName()))return Work->Fail(TEXT("learning_component_invalid"),TEXT("Owned tagged learning Geometry Collection component required"));
 return Work->Track(A,TEXT("cache_component_owner"))&&Work->Track(C,TEXT("cache_component"),A)&&Work->Track(R,TEXT("cache_rest"))&&CollectionSchema(R);
}
UGeometryCollectionComponent* FindGeometryCollectionComponent(UWorld*W,const FString&N){auto*A=FindActor<AActor>(W,N);return A?A->FindComponentByClass<UGeometryCollectionComponent>():nullptr;}
TArray<UGeometryCollectionComponent*> ResolveObserved(AChaosCacheManager*M)
{
 TArray<UGeometryCollectionComponent*>Out;if(!Guard()||!Live(M))return Out;const auto&Observed=M->GetObservedComponents();if(Observed.Num()>8192){Work->Fail(TEXT("observed_budget"),TEXT("Observed component count exceeds budget"));return Out;}TSet<UGeometryCollectionComponent*>Seen;
 for(const auto&O:Observed){auto*C=Cast<UGeometryCollectionComponent>(O.GetComponent(M));if(!C||!C->GetOwner()||!C->GetOwner()->Tags.Contains(LearningActorTag)||!C->GetRestCollection()||!IsWithinLearningRoot(C->GetRestCollection()->GetPathName()))continue;if(Seen.Contains(C)){Work->Fail(TEXT("duplicate_observed_target"),TEXT("Observed list contains duplicate target"));return {};}Seen.Add(C);if(!TrackLearningComponent(C))return {};Out.Add(C);}return Out;
}
bool ValidateToken(UWorld*W,AChaosCacheManager*M,ECacheMode Mode)
{
 const auto T=ActiveCacheConfigurationToken;if(!T||T->World.Get()!=W||T->Manager.Get()!=M||T->Mode!=Mode||T->Collection.Get()!=M->CacheCollection.Get()||M->CacheMode!=Mode||M->GetObservedComponents().Num()!=16)return false;auto Components=ResolveObserved(M);if(Components.Num()!=16||T->Components.Num()!=16)return false;
 for(int32 I=0;I<16;++I)if(!T->Components[I].IsValid()||!Components.Contains(T->Components[I].Get()))return false;return Guard();
}
void StopConfigure(const TCHAR*Reason=TEXT("configure_session_retired"))
{
 auto S=ActiveCacheConfigureSession;if(!S)return;if(S->TickerHandle.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(S->TickerHandle);S->TickerHandle.Reset();}if(S->Evidence){S->Evidence->ReleasePhysics();if(!S->Evidence->Complete)S->Evidence->Event(Reason,false);S->Evidence->Complete=true;LastSessionExecution=S->Evidence->Snapshot();if(Work&&Work!=S->Evidence.Get()){Work->Event(Reason,false);Work->R.bChangeStateKnown&=S->Evidence->R.bChangeStateKnown;Work->R.Data->SetObjectField(TEXT("previous_session_execution"),LastSessionExecution);}}ActiveCacheConfigureSession.Reset();
}
void StopMotion(const TCHAR*Reason=TEXT("motion_session_retired"))
{
 auto S=ActiveCacheRecordMotionSession;if(!S)return;if(S->TickerHandle.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(S->TickerHandle);S->TickerHandle.Reset();}if(S->Evidence){S->Evidence->ReleasePhysics();if(!S->Evidence->Complete)S->Evidence->Event(Reason,false);S->Evidence->Complete=true;LastSessionExecution=S->Evidence->Snapshot();if(Work&&Work!=S->Evidence.Get()){Work->Event(Reason,false);Work->R.bChangeStateKnown&=S->Evidence->R.bChangeStateKnown;Work->R.Data->SetObjectField(TEXT("previous_session_execution"),LastSessionExecution);}}ActiveCacheRecordMotionSession.Reset();
}
bool OwnedEvaluationMatches()
{
 return SafeEditor()&&Live(LastOwnedWorld.Get())&&!LastOwnedWorld->bIsTearingDown&&CurrentWorld()==LastOwnedWorld.Get()&&Live(LastOwnedManager.Get())&&LastOwnedManager->GetWorld()==LastOwnedWorld.Get()&&LastOwnedManager->CacheCollection.Get()==LastOwnedCollection.Get()&&LastOwnedManager->CacheMode==LastOwnedMode;
}

bool ValidatePlaybackCollection(UChaosCacheCollection* Collection,AChaosCacheManager* Manager,const TArray<UGeometryCollectionComponent*>& Components,bool PreserveMappings)
{
 if(!Guard()||!Live(Collection)||Components.Num()<16)return Work->Fail(TEXT("playback_mapping_invalid"),TEXT("At least sixteen live playback targets required"));
 int64 TotalKeys=0;TArray<TSharedPtr<FJsonValue>>Rows;
 for(int32 I=0;I<Components.Num();++I)
 {
  auto*C=Components[I];if(!TrackLearningComponent(C))return false;const int32 N=C->GetRestCollection()->GetGeometryCollection()->NumElements(FGeometryCollection::TransformGroup);FName Name(*FString::Printf(TEXT("ChaosLearning_%02d"),I));
  if(PreserveMappings){bool Found=false;for(const auto&O:Manager->GetObservedComponents())if(O.GetComponent(Manager)==C){Name=O.CacheName;Found=true;break;}if(!Found)return Work->Fail(TEXT("playback_mapping_changed"),TEXT("Captured component has no exact observed cache mapping"));}
  UChaosCache*Cache=nullptr;if(!Step(TEXT("FindCache_playback_preflight"),[&]{Cache=Collection->FindCache(Name);},false))return false;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("cache_name"),Name.ToString());Row->SetStringField(TEXT("component"),C->GetPathName());Row->SetNumberField(TEXT("rest_transform_count"),N);Row->SetBoolField(TEXT("cache_found"),Cache!=nullptr);Rows.Add(MakeShared<FJsonValueObject>(Row));Work->R.Data->SetArrayField(TEXT("playback_preflight"),Rows);
  // Missing/empty caches retain the legacy later data/variation FAIL gate.
  if(!Cache)continue;if(!Live(Cache)||!Work->Track(Cache,TEXT("preflight_playback_cache")))return false;
  if(Cache->GetCacheData())return Work->Fail(TEXT("unsupported_external_cache_backing"),TEXT("IChaosCacheData-backed playback has no validated public bounded track projection and is not executed"));
  if(Cache->TrackToParticle.Num()!=Cache->ParticleTracks.Num()||Cache->TrackToParticle.Num()>1000000||!FMath::IsFinite(Cache->RecordedDuration)||Cache->RecordedDuration<0)return Work->Fail(TEXT("cache_track_shape"),TEXT("Cache track mapping shape/count/duration is invalid"));TSet<int32>Seen;
  for(int32 T=0;T<Cache->TrackToParticle.Num();++T)
  {
   const int32 Index=Cache->TrackToParticle[T];if(Index<0||Index>=N||Seen.Contains(Index))return Work->Fail(TEXT("cache_particle_index"),TEXT("Cache particle index is duplicate or outside the current rest transform range"));Seen.Add(Index);const auto&Track=Cache->ParticleTracks[T].TransformData;const int32 K=Track.KeyTimestamps.Num();TotalKeys+=K;
   if(K>1000000||TotalKeys>8000000||Track.RawTransformTrack.PosKeys.Num()<K||Track.RawTransformTrack.RotKeys.Num()<K||Track.RawTransformTrack.PosKeys.Num()>1000000||Track.RawTransformTrack.RotKeys.Num()>1000000||Track.RawTransformTrack.ScaleKeys.Num()>1000000||!FMath::IsFinite(Track.BeginOffset))return Work->Fail(TEXT("cache_track_shape"),TEXT("Cache timestamps/raw keys have unsafe lengths or budget"));
   for(int32 Key=0;Key<K;++Key)if(!FMath::IsFinite(Track.KeyTimestamps[Key])||(Key>0&&Track.KeyTimestamps[Key]<Track.KeyTimestamps[Key-1])||Track.RawTransformTrack.PosKeys[Key].ContainsNaN()||Track.RawTransformTrack.RotKeys[Key].ContainsNaN()||!Track.RawTransformTrack.RotKeys[Key].IsNormalized())return Work->Fail(TEXT("cache_track_nonfinite"),TEXT("Cache transform keys are nonfinite, unordered or contain nonunit rotations"));
   for(const auto&Scale:Track.RawTransformTrack.ScaleKeys)if(Scale.ContainsNaN())return Work->Fail(TEXT("cache_track_nonfinite"),TEXT("Cache scale keys are nonfinite"));
  }
  Row->SetBoolField(TEXT("native_transform_track_shape_valid"),true);Row->SetNumberField(TEXT("particle_track_count"),Cache->ParticleTracks.Num());
 }
 Work->R.Data->SetStringField(TEXT("playback_preflight_scope"),TEXT("Native transform tracks and exact CacheName->current rest particle index mapping; in-place compressed raw/key tracks remain valid. This is not every curve/event/backend integrity proof."));return Guard();
}

bool CompleteConfigureBody()
{
 auto Hold=ActiveCacheConfigureSession;if(!Hold||Hold->bCompleted)return false;auto&S=*Hold;if(GFrameCounter<=S.EndEvaluateFrame)return true;auto*W=S.World.Get();auto*M=S.Manager.Get();auto*Input=S.InputCollection.Get();if(!Guard()||!Live(W)||!Live(M)||!Live(Input)){S.bCompleted=true;S.Failure=TEXT("world_manager_or_collection_lost");return false;}
 if(!S.bManagerResetCompleted)
 {
  auto*Active=Input;if(S.RequestedMode==ECacheMode::Record){if(!Step(TEXT("duplicate_play_world_cache"),[&]{Active=DuplicateObject<UChaosCacheCollection>(Input,M);Work->ReturnedObject(TEXT("duplicate_play_world_cache"),TEXT("manager-owned transient cache"),Active);})||!Active){S.Failure=TEXT("transient_cache_duplication_failed");S.bCompleted=true;return false;}if(!Work->Track(Active,TEXT("transient_play_cache"),M,true))return false;if(!Step(TEXT("mark_transient_cache"),[&]{Active->ClearFlags(RF_Public|RF_Standalone);Active->SetFlags(RF_Transient);}))return false;}
  if(!Step(TEXT("assign_cache_mode"),[&]{M->CacheCollection=Active;M->CacheMode=S.RequestedMode;M->StartMode=EStartMode::Triggered;Work->ExpectManager(M,Active,S.RequestedMode,EStartMode::Triggered);}))return false;LastOwnedCollection=Active;LastOwnedMode=S.RequestedMode;
  auto Before=ResolveObserved(M);if(!Guard()||!Work->R.Error.IsEmpty())return false;S.bPreservedObservedComponents=M->GetObservedComponents().Num()==16;
  if(S.bPreservedObservedComponents){for(auto&C:S.Components)if(!Before.Contains(C.Get()))return Work->Fail(TEXT("observed_identity_changed"),TEXT("Existing sixteen observed targets do not match captured requested targets"));}
  else if(!Step(TEXT("ClearObservedComponents"),[&]{M->ClearObservedComponents(false);}))return false;
  S.ClearObservedComponentsFrame=GFrameCounter;S.bManagerResetCompleted=true;return true;
 }
 if(GFrameCounter<=S.ClearObservedComponentsFrame)return true;if(!M->CacheCollection)return Work->Fail(TEXT("cache_lost"),TEXT("Active cache disappeared after manager reset"));int32 Added=S.bPreservedObservedComponents?M->GetObservedComponents().Num():0;
 if(!S.bPreservedObservedComponents)for(int32 I=0;I<S.Components.Num();++I){auto*C=S.Components[I].Get();if(!TrackLearningComponent(C)||C->GetOwner()->GetName()!=S.ComponentActorNames[I])return false;if(!Step(TEXT("FindOrAddObservedComponent"),[&]{M->FindOrAddObservedComponent(C,S.CacheNames[I],true);}))return false;++Added;}
 if(Added!=16)return Work->Fail(TEXT("observed_count"),TEXT("Observed component rebuild did not produce sixteen targets"));
 if(S.RequestedMode==ECacheMode::Play){auto Targets=ResolveObserved(M);if(!ValidatePlaybackCollection(M->CacheCollection.Get(),M,Targets,true))return false;}
 auto*EvaluateCollection=M->CacheCollection.Get();if(!Step(TEXT("BeginEvaluate"),[&]{RememberEvaluation(M,W,EvaluateCollection,S.RequestedMode);M->BeginEvaluate();Work->R.Data->SetBoolField(TEXT("begin_evaluate_returned"),true);}))return false;auto Actual=ResolveObserved(M);S.bSucceeded=Actual.Num()==16;for(auto&C:S.Components)S.bSucceeded&=Actual.Contains(C.Get());if(S.bSucceeded){ActiveCacheConfigurationToken=MakeShared<FCacheConfigurationToken>();auto&T=*ActiveCacheConfigurationToken;T.World=W;T.Manager=M;T.Collection=M->CacheCollection.Get();T.Mode=S.RequestedMode;T.CompletionFrame=GFrameCounter;T.Components=S.Components;}S.bCompleted=true;S.Failure=S.bSucceeded?FString():TEXT("begin_evaluate_or_observed_rebuild_failed");return false;
}
bool CompleteActiveCacheConfigureSession()
{
 auto Hold=ActiveCacheConfigureSession;if(!Hold||!Hold->Evidence)return false;if(Running)return !ShuttingDown;auto E=Hold->Evidence;TGuardValue<bool>Busy(Running,true);TGuardValue<FWork*>Scope(Work,E.Get());FExecutionPins Pins(*E);bool Keep=false;
 if(Pins.Acquire()){if(FPlatformTime::Seconds()>=Hold->Deadline)E->Fail(TEXT("configure_timeout"),TEXT("Cache configure exceeded monotonic30second deadline"));else Keep=CompleteConfigureBody();}
 if(!Keep||!E->R.Error.IsEmpty()){Keep=false;Hold->bCompleted=true;if(!E->R.Error.IsEmpty()){Hold->Failure=E->R.Error;Hold->bSucceeded=false;}Hold->TickerHandle.Reset();E->Complete=true;E->R.Data->SetStringField(TEXT("phase"),Hold->bSucceeded?TEXT("configure_complete"):TEXT("configure_failed"));E->R.Data->SetBoolField(TEXT("postcondition_passed"),Hold->bSucceeded);E->Observe();LastSessionExecution=E->Snapshot();}return Keep;
}
int32 ApplyDirectVelocityToEnabledPhysicsObjects(UGeometryCollectionComponent*C,const FVector&V)
{
 if(!Guard()||!Live(C)||V.IsNearlyZero()||!Work->CapturePhysics(C))return 0;auto*Proxy=C->GetPhysicsProxy();const auto*D=C->GetDynamicCollection();if(!Proxy||!D)return 0;if(D->Active.Num()>1000000){Work->Fail(TEXT("physics_budget"),TEXT("Dynamic transform budget exceeded"));return 0;}TArray<Chaos::FPhysicsObjectHandle>Objects;TArray<int32>Indices;TSet<Chaos::FPhysicsObjectHandle>Seen;
 for(int32 I=0;I<D->Active.Num();++I)if(auto*O=Proxy->GetPhysicsObjectByIndex(I)){if(Seen.Contains(O)){Work->Fail(TEXT("duplicate_physics_object"),TEXT("Repeated physics object pointer"));return 0;}Seen.Add(O);Objects.Add(O);Indices.Add(I);}if(Objects.IsEmpty())return 0;TArray<int32>Enabled;
 Work->SDK(TEXT("physics_velocity_read"));{auto Read=FPhysicsObjectExternalInterface::LockRead(Objects);for(int32 I=0;I<Objects.Num();++I){auto*O=Objects[I];if(!Read->AreAllDisabled(MakeArrayView(&O,1)))Enabled.Add(Indices[I]);}}
 if(!Guard())return 0;int32 Applied=0;for(int32 I:Enabled){if(!Guard()||!Work->R.Error.IsEmpty())return Applied;if(auto*P=Proxy->GetParticleByIndex_External(I)){Work->Event(TEXT("controlled_direct_velocity"));Work->Count(TEXT("controlled_direct_velocity_calls"));P->SetV(V);++Applied;if(!Guard())return Applied;}}
 if(Applied&&!Step(TEXT("SetProxyDirty_External"),[&]{Proxy->SetProxyDirty_External();}))return Applied;return Applied;
}
bool CaptureActiveCacheRecordMotionFrame()
{
 auto Hold=ActiveCacheRecordMotionSession;if(!Hold||!Hold->Evidence||Hold->bCaptureCompleted)return false;if(Running)return !ShuttingDown;auto E=Hold->Evidence;TGuardValue<bool>Busy(Running,true);TGuardValue<FWork*>Scope(Work,E.Get());FExecutionPins Pins(*E);bool Keep=false;
 if(Pins.Acquire())
 {
  auto&S=*Hold;if(FPlatformTime::Seconds()>=S.Deadline)E->Fail(TEXT("record_motion_timeout"),TEXT("Record motion exceeded monotonic30second deadline"));
  else if(GFrameCounter<=S.BeginFrame+static_cast<uint64>(S.CapturedFrameCount))Keep=true;
  else{int32 Applied=0;for(int32 I=0;I<S.Components.Num();++I){Applied+=ApplyDirectVelocityToEnabledPhysicsObjects(S.Components[I].Get(),S.Velocities[I]);if(!Guard()||!E->R.Error.IsEmpty())break;}++S.CapturedFrameCount;S.TotalVelocityAppliedObjectCount+=Applied;if(Guard()&&E->R.Error.IsEmpty()){const double Elapsed=S.World->GetTimeSeconds()-S.BeginWorldTime;if(S.CapturedFrameCount>=36&&Elapsed>=1.2){S.bCaptureCompleted=true;S.CompletionReason=TEXT("minimum_record_motion_duration_reached");}else if(S.CapturedFrameCount>=120){S.bCaptureCompleted=true;S.CompletionReason=TEXT("record_motion_frame_timeout");E->Fail(TEXT("record_motion_frame_timeout"),TEXT("120 frame limit reached before minimum1.2seconds"));}else Keep=true;}}
 }
 if(!E->R.Error.IsEmpty()||!Keep){Keep=false;Hold->bCaptureCompleted=true;if(Hold->CompletionReason.IsEmpty())Hold->CompletionReason=E->R.Error.IsEmpty()?TEXT("record_motion_completed"):E->R.Error;Hold->TickerHandle.Reset();E->ReleasePhysics();E->Complete=true;E->R.Data->SetBoolField(TEXT("postcondition_passed"),E->R.Error.IsEmpty());E->R.Data->SetStringField(TEXT("phase"),E->R.Error.IsEmpty()?TEXT("record_motion_complete"):TEXT("record_motion_failed"));E->Observe();LastSessionExecution=E->Snapshot();}return Keep;
}
int32 ResolveActiveClusterItemIndex(const UGeometryCollectionComponent*C)
{
 if(!Live(C)||!Guard()||!CollectionSchema(C->GetRestCollection()))return INDEX_NONE;const auto Rest=C->GetRestCollection()->GetGeometryCollection();const auto*D=C->GetDynamicCollection();if(!D)return INDEX_NONE;
 for(int32 I=0;I<Rest->Parent.Num();++I)if(Rest->Parent[I]==INDEX_NONE&&Rest->SimulationType[I]==FGeometryCollection::ESimulationTypes::FST_Clustered&&D->Active.IsValidIndex(I)&&D->Active[I])return I;
 for(int32 I=0;I<Rest->Parent.Num();++I)if(Rest->Parent[I]==INDEX_NONE&&Rest->SimulationType[I]==FGeometryCollection::ESimulationTypes::FST_Clustered)return I;return INDEX_NONE;
}

bool RetirePlayback(AChaosCacheManager* Expected,UWorld* World)
{
 auto Hold=ActiveCachePlaybackSession;if(!Hold)return true;auto E=Hold->Evidence;bool Safe=false;
 if(E)
 {
  E->ReleasePhysics();FExecutionPins Pins(*E);
  Safe=SafeEditor()&&Hold->World.Get()==World&&Hold->Manager.Get()==Expected&&Live(World)&&!World->bIsTearingDown&&CurrentWorld()==World&&Live(Expected)&&Expected->GetWorld()==World&&Expected->CacheCollection.Get()==Hold->Collection.Get()&&E->R.Error.IsEmpty()&&Pins.Acquire();
  if(Safe)
  {
   E->Event(TEXT("retire_previous_playback_EndEvaluate"));Safe=EndManagerEvaluation(Expected,World,TEXT("retire_previous_playback_EndEvaluate"));bool Returned=false;Work->R.Data->TryGetBoolField(TEXT("end_evaluate_returned"),Returned);E->R.Data->SetBoolField(TEXT("end_evaluate_returned"),Returned);if(Safe&&!E->Guard()){Safe=false;OwnsEvaluation=true;}if(Safe)E->Observe();
  }
  else
  {
   E->R.bChangeStateKnown=false;E->R.Data->SetBoolField(TEXT("unsafe_or_foreign_owner_cleanup_skipped"),true);Work->R.bChangeStateKnown=false;Work->R.Data->SetBoolField(TEXT("unsafe_or_foreign_owner_cleanup_skipped"),true);
  }
  E->Complete=true;LastSessionExecution=E->Snapshot();Work->R.Data->SetObjectField(TEXT("retired_playback_session_execution"),LastSessionExecution);
 }
 Work->Event(TEXT("retire_previous_playback_metadata"),false);ActiveCachePlaybackSession.Reset();if(!Safe)return Work->Fail(TEXT("previous_playback_owner_lost"),TEXT("Previous playback owner could not be safely closed; physics/cache effects remain unknown and no replacement was dispatched"));return true;
}

void RetireExpiredWorldMetadata()
{
 const auto Retire=[&](const TSharedPtr<FWork>& E,const TCHAR*Kind)
 {
  if(!E)return;E->ReleasePhysics();const bool Pending=!E->Complete;E->Complete=true;E->R.bChangeStateKnown=false;E->R.Data->SetStringField(TEXT("expired_world_retirement"),Kind);if(Pending&&E->R.Error.IsEmpty())E->Fail(TEXT("world_expired"),TEXT("Captured weak world expired; no SDK recovery, reload or command replay was attempted"));auto Snapshot=E->Snapshot();LastSessionExecution=Snapshot;TArray<TSharedPtr<FJsonValue>>Rows;const TArray<TSharedPtr<FJsonValue>>*Old=nullptr;if(Work->R.Data->TryGetArrayField(TEXT("retired_expired_sessions"),Old)&&Old)Rows=*Old;Rows.Add(MakeShared<FJsonValueObject>(Snapshot));Work->R.Data->SetArrayField(TEXT("retired_expired_sessions"),Rows);Work->Event(TEXT("retire_expired_world_metadata"),false);
 };
 if(ActiveCacheConfigureSession&&!ActiveCacheConfigureSession->World.IsValid()){auto S=ActiveCacheConfigureSession;if(S->TickerHandle.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(S->TickerHandle);Retire(S->Evidence,TEXT("configure"));ActiveCacheConfigureSession.Reset();}
 if(ActiveCacheRecordMotionSession&&!ActiveCacheRecordMotionSession->World.IsValid()){auto S=ActiveCacheRecordMotionSession;if(S->TickerHandle.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(S->TickerHandle);Retire(S->Evidence,TEXT("record_motion"));ActiveCacheRecordMotionSession.Reset();}
 if(ActiveCachePlaybackSession&&!ActiveCachePlaybackSession->World.IsValid()){Retire(ActiveCachePlaybackSession->Evidence,TEXT("playback"));ActiveCachePlaybackSession.Reset();}
 if(ActiveCacheConfigurationToken&&!ActiveCacheConfigurationToken->World.IsValid())ActiveCacheConfigurationToken.Reset();
 if(ActiveCacheEvidence&&!ActiveCacheEvidence->World.IsValid()){Retire(ActiveCacheEvidence,TEXT("active_evidence"));ActiveCacheEvidence.Reset();}
 if(OwnsEvaluation&&!LastOwnedWorld.IsValid()){Work->R.Data->SetBoolField(TEXT("expired_world_evaluation_lease_retired_without_SDK"),true);OwnsEvaluation=false;LastOwnedManager.Reset();LastOwnedCollection.Reset();LastOwnedWorld.Reset();}
}
uint32 HashGeometryComponentState(const UGeometryCollectionComponent* Component)
{
    uint32 Hash = 0;if(!Guard())return Hash;Work->SDK(TEXT("hash_component_state"));
    if (!Component)
    {
        return Hash;
    }
    if (const FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection())
    {
        for (int32 TransformIndex = 0; TransformIndex < Dynamic->GetNumTransforms(); ++TransformIndex)
        {
            if(Dynamic->GetNumTransforms()>1000000){Work->Fail(TEXT("dynamic_budget"),TEXT("Dynamic transform budget exceeded"));return 0;}
            const FTransform3f& Transform = Dynamic->GetTransform(TransformIndex);
            const FTransform Transform64(Transform);
            const FVector Location = Transform64.GetLocation();
            const FRotator Rotation = Transform64.Rotator();
            const FVector Scale = Transform64.GetScale3D();
            const int64 Quantized[9] = {
                SafeQuantize(Location.X,10.0), SafeQuantize(Location.Y,10.0), SafeQuantize(Location.Z,10.0),
                SafeQuantize(Rotation.Roll,10.0), SafeQuantize(Rotation.Pitch,10.0), SafeQuantize(Rotation.Yaw,10.0),
                SafeQuantize(Scale.X,1000.0), SafeQuantize(Scale.Y,1000.0), SafeQuantize(Scale.Z,1000.0)};
            Hash = FCrc::MemCrc32(Quantized, sizeof(Quantized), Hash);
        }
        for (int32 ActiveIndex = 0; ActiveIndex < Dynamic->Active.Num(); ++ActiveIndex)
        {
            const bool bActive = Dynamic->Active[ActiveIndex];
            Hash = FCrc::MemCrc32(&bActive, sizeof(bool), Hash);
        }
    }
    else
    {
        const FTransform Transform = Component->GetComponentTransform();
        const FVector Location = Transform.GetLocation();
        const FRotator Rotation = Transform.Rotator();
        const FVector Scale = Transform.GetScale3D();
        const int64 Quantized[9] = {
            SafeQuantize(Location.X,10.0), SafeQuantize(Location.Y,10.0), SafeQuantize(Location.Z,10.0),
            SafeQuantize(Rotation.Roll,10.0), SafeQuantize(Rotation.Pitch,10.0), SafeQuantize(Rotation.Yaw,10.0),
            SafeQuantize(Scale.X,1000.0), SafeQuantize(Scale.Y,1000.0), SafeQuantize(Scale.Z,1000.0)};
        Hash = FCrc::MemCrc32(Quantized, sizeof(Quantized), Hash);
    }
    return Hash;
}
TSharedPtr<FJsonObject> HandleRunChaosCacheSmoke(const TSharedPtr<FJsonObject>& Params)
{
    const FString Operation = GetStringParam(Params, TEXT("operation"), TEXT("inspect"));
    const FString CachePath = GetStringParam(Params, TEXT("cache_collection_path"), *(FString(LearningRoot) / TEXT("CC_ChaosLearning")));
    if (!IsWithinLearningRoot(CachePath))
    {
        return FLegacy::CreateErrorResponse(TEXT("Chaos cache smoke is restricted to the learning temp root"));
    }
    UChaosCacheCollection* SourceCollection = LoadCacheCollection(CachePath);
    if (!SourceCollection)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(TEXT("Chaos Cache Collection not found: %s"), *CachePath));
    }

    FString WorldContext;
    UWorld* World = ResolveWorld(true, WorldContext);
    if(World&&!Work->CaptureWorld(World))return FLegacy::CreateErrorResponse(Work->R.Error);
    const bool bSourceAssetDirtyBefore = SourceCollection->GetOutermost()->IsDirty();
    TSharedPtr<FJsonObject> Data = CacheCollectionToJson(SourceCollection);
    Data->SetStringField(TEXT("operation"), Operation);
    Data->SetStringField(TEXT("world_context"), WorldContext);
    if (Operation == TEXT("inspect"))
    {
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Cache inspection alone does not satisfy recording/playback and five-run A/B validation"));
        Data->SetBoolField(TEXT("read_only"), true);
        return FLegacy::CreateSuccessResponse(Data);
    }

    if (!World || WorldContext != TEXT("play"))
    {
        Data->SetStringField(TEXT("operation_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("blocker"), TEXT("Cache configure/trigger/sample requires an existing PIE/SIE session; the command does not control shared editor lifecycle"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    RetireExpiredWorldMetadata();Work->Writer=true;if(!Guard())return FLegacy::CreateErrorResponse(Work->R.Error);
    const FString ManagerName = GetStringParam(Params, TEXT("cache_manager_actor_name"), TEXT(""));
    AChaosCacheManager* Manager = FindActor<AChaosCacheManager>(World, ManagerName);
    if (!Manager || !Manager->Tags.Contains(LearningActorTag))
    {
        return FLegacy::CreateErrorResponse(TEXT("Tagged Chaos learning Cache Manager actor not found"));
    }
    if(!Work->Track(Manager,TEXT("cache_manager"))||!GuardLearningObserved(Manager))return FLegacy::CreateErrorResponse(Work->R.Error);
    UChaosCacheCollection* Collection = Manager->CacheCollection ? Manager->CacheCollection.Get() : SourceCollection;
    if(!Live(Collection)||!Work->Track(Collection,TEXT("active_cache_collection")))return FLegacy::CreateErrorResponse(Work->R.Error);
    if(OwnsEvaluation&&(LastOwnedWorld.Get()!=World||LastOwnedManager.Get()!=Manager))return FLegacy::CreateErrorResponse(TEXT("Another GA world/manager retains an active or unresolved evaluation lease"));
    if(ActiveCacheConfigureSession&&!ActiveCacheConfigureSession->bCompleted&&(ActiveCacheConfigureSession->World.Get()!=World||ActiveCacheConfigureSession->Manager.Get()!=Manager))return FLegacy::CreateErrorResponse(TEXT("Another GA world/manager has a pending configure session"));
    Work->ExpectManager(Manager,Manager->CacheCollection.Get(),Manager->CacheMode,Manager->StartMode);
    auto AddCacheIsolationEvidence = [&]()
    {
        const bool bSourceAssetDirtyAfter = SourceCollection->GetOutermost()->IsDirty();
        Data->SetStringField(TEXT("source_cache_asset_path"), SourceCollection->GetPathName());
        Data->SetStringField(TEXT("active_cache_collection_path"), Collection ? Collection->GetPathName() : FString());
        Data->SetBoolField(TEXT("uses_play_world_cache_duplicate"), Collection && Collection != SourceCollection && Collection->IsIn(Manager));
        Data->SetBoolField(TEXT("source_asset_dirty_before"), bSourceAssetDirtyBefore);
        Data->SetBoolField(TEXT("source_asset_dirty_after"), bSourceAssetDirtyAfter);
        Data->SetBoolField(TEXT("source_asset_newly_dirtied"), !bSourceAssetDirtyBefore && bSourceAssetDirtyAfter);
    };
    auto ResolveObservedComponents=[Manager](){return ResolveObserved(Manager);};
    auto HasSuccessfulCacheConfiguration=[&](ECacheMode Mode){return ValidateToken(World,Manager,Mode);};

    if (Operation == TEXT("record_motion_16"))
    {
        StopMotion();
        if (!Collection || Collection == SourceCollection || !Collection->IsIn(Manager) ||
            !HasSuccessfulCacheConfiguration(ECacheMode::Record))
        {
            return FLegacy::CreateErrorResponse(
                TEXT("record_motion_16 requires configure_record with a manager-owned transient cache first"));
        }
        TArray<UGeometryCollectionComponent*> Components = ResolveObservedComponents();
        int32 MotionDispatchCount = 0;
        TArray<int32> MotionItemIndices;
        TArray<FVector> MotionVelocities;
        for (int32 ComponentIndex = 0; ComponentIndex < FMath::Min(16, Components.Num()); ++ComponentIndex)
        {
            UGeometryCollectionComponent* Component = Components[ComponentIndex];
            const FVector Velocity(150.0 + ComponentIndex * 20.0, (ComponentIndex % 2 == 0 ? 1.0 : -1.0) * 120.0, 500.0 + ComponentIndex * 10.0);
            MotionVelocities.Add(Velocity);
            const int32 ActiveClusterItem = ResolveActiveClusterItemIndex(Component);
            MotionItemIndices.Add(ActiveClusterItem);
            if (ActiveClusterItem == INDEX_NONE)
            {
                continue;
            }
            if(!Work->CapturePhysics(Component))return FLegacy::CreateErrorResponse(Work->R.Error);
            if(!Step(TEXT("ApplyExternalStrain"),[&]{Component->ApplyExternalStrain(ActiveClusterItem,Component->Bounds.Origin,1000.0f,10,1.0f,1000000.0f);})||!Step(TEXT("ApplyBreakingLinearVelocity"),[&]{Component->ApplyBreakingLinearVelocity(ActiveClusterItem,Velocity);})||!Step(TEXT("ApplyBreakingAngularVelocity"),[&]{Component->ApplyBreakingAngularVelocity(ActiveClusterItem,FVector(0,2.0+ComponentIndex*.1,1));}))return FLegacy::CreateErrorResponse(Work->R.Error);
            ++MotionDispatchCount;
        }
        if (MotionDispatchCount == 16)
        {
            Work->Event(TEXT("create_motion_session"),false);ActiveCacheRecordMotionSession = MakeShared<FCacheRecordMotionSession>();
            ActiveCacheRecordMotionSession->RequestedManagerSelector=ManagerName;ActiveCacheRecordMotionSession->RequestedCachePath=CachePath;ActiveCacheRecordMotionSession->Evidence=Owner;ActiveCacheRecordMotionSession->Deadline=FPlatformTime::Seconds()+30;Work->Complete=false;Work->R.Data->SetStringField(TEXT("phase"),TEXT("record_motion_pending"));ActiveCacheEvidence=Owner;
            ActiveCacheRecordMotionSession->World = World;
            ActiveCacheRecordMotionSession->Manager = Manager;
            ActiveCacheRecordMotionSession->BeginFrame = GFrameCounter;
            ActiveCacheRecordMotionSession->BeginWorldTime = World->GetTimeSeconds();
            for (int32 ComponentIndex = 0; ComponentIndex < 16; ++ComponentIndex)
            {
                ActiveCacheRecordMotionSession->Components.Add(Components[ComponentIndex]);
                ActiveCacheRecordMotionSession->Velocities.Add(MotionVelocities[ComponentIndex]);
            }
            ActiveCacheRecordMotionSession->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([](float)
                {
                    return CaptureActiveCacheRecordMotionFrame();
                }));
        }
        Data = CacheCollectionToJson(Collection);
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), MotionDispatchCount == 16 ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("An internal ticker is maintaining distinct motion for at least 1.2 seconds and 36 frames before finalize"));
        Data->SetNumberField(TEXT("motion_dispatch_count"), MotionDispatchCount);
        Data->SetArrayField(TEXT("motion_item_indices"), IntsToJson(MotionItemIndices));
        Data->SetNumberField(TEXT("dispatch_world_time_seconds"), World->GetTimeSeconds());
        Data->SetBoolField(TEXT("record_motion_ticker_started"),
            ActiveCacheRecordMotionSession && ActiveCacheRecordMotionSession->TickerHandle.IsValid());
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        AddCacheIsolationEvidence();
        return FLegacy::CreateSuccessResponse(Data);
    }

    if (Operation == TEXT("finalize_record"))
    {
        if (ActiveCacheRecordMotionSession &&
            ActiveCacheRecordMotionSession->World.Get() == World &&
            ActiveCacheRecordMotionSession->Manager.Get() == Manager &&
            !ActiveCacheRecordMotionSession->bCaptureCompleted)
        {
            Data = CacheCollectionToJson(Collection);
            Data->SetStringField(TEXT("operation"), Operation);
            Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
            Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_blocker"), TEXT("Internal record-motion capture has not yet reached 36 frames and 1.2 seconds"));
            Data->SetNumberField(TEXT("record_motion_captured_frame_count"), ActiveCacheRecordMotionSession->CapturedFrameCount);
            Data->SetNumberField(TEXT("record_motion_elapsed_seconds"),
                World->GetTimeSeconds() - ActiveCacheRecordMotionSession->BeginWorldTime);
            Data->SetBoolField(TEXT("record_motion_capture_complete"), false);
            Data->SetBoolField(TEXT("requires_followup_sample"), true);
            AddCacheIsolationEvidence();
            return FLegacy::CreateSuccessResponse(Data);
        }
        const int32 RecordMotionCapturedFrameCount = ActiveCacheRecordMotionSession
            ? ActiveCacheRecordMotionSession->CapturedFrameCount : 0;
        const int32 RecordMotionAppliedObjectCount = ActiveCacheRecordMotionSession
            ? ActiveCacheRecordMotionSession->TotalVelocityAppliedObjectCount : 0;
        const FString RecordMotionCompletionReason = ActiveCacheRecordMotionSession
            ? ActiveCacheRecordMotionSession->CompletionReason : FString();
        StopMotion();
        if(!EndManagerEvaluation(Manager,World,TEXT("EndEvaluate")))return FLegacy::CreateErrorResponse(Work->R.Error);
        OwnsEvaluation=false;
        if(!Step(TEXT("FlushAllCacheWrites"),[&]{Collection->FlushAllCacheWrites();}))return FLegacy::CreateErrorResponse(Work->R.Error);
        Data = CacheCollectionToJson(Collection);
        const bool bRecordGate = Collection->GetMaxNumFrames() >= 30 && Collection->GetMaxDuration() >= 1.0f;
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("assertion_status"), bRecordGate ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Recording was finalized, but two playback passes and five repeated runs remain"));
        Data->SetBoolField(TEXT("end_evaluate_called"), true);
        Data->SetBoolField(TEXT("end_record_closed_by_end_evaluate"), true);
        Data->SetBoolField(TEXT("frame_duration_gate"), bRecordGate);
        Data->SetNumberField(TEXT("record_motion_captured_frame_count"), RecordMotionCapturedFrameCount);
        Data->SetNumberField(TEXT("record_motion_total_velocity_applied_object_count"), RecordMotionAppliedObjectCount);
        Data->SetStringField(TEXT("record_motion_completion_reason"), RecordMotionCompletionReason);
        AddCacheIsolationEvidence();
        const bool bRecordIsolationPassed = Data->GetBoolField(TEXT("uses_play_world_cache_duplicate")) &&
            !Data->GetBoolField(TEXT("source_asset_newly_dirtied"));
        Data->SetStringField(TEXT("assertion_status"), bRecordGate && bRecordIsolationPassed ? TEXT("PASS") : TEXT("FAIL"));
        return FLegacy::CreateSuccessResponse(Data);
    }

    if (Operation == TEXT("playback_suppression_sample"))
    {
        if (!ActiveCachePlaybackSession || ActiveCachePlaybackSession->World.Get() != World ||
            ActiveCachePlaybackSession->Manager.Get() != Manager || ActiveCachePlaybackSession->Collection.Get() != Collection)
        {
            return FLegacy::CreateErrorResponse(
                TEXT("No active cache playback validation session exists; call playback_validate first"));
        }
        TArray<UGeometryCollectionComponent*> Components = ResolveObservedComponents();
        if (Components.Num() < 16)
        {
        if(!EndManagerEvaluation(Manager,World,TEXT("EndEvaluate")))return FLegacy::CreateErrorResponse(Work->R.Error);
        OwnsEvaluation=false;
            Work->Event(TEXT("retire_playback_session"),false);Work->Complete=true;Work->ReleasePhysics();ActiveCachePlaybackSession.Reset();
            return FLegacy::CreateErrorResponse(TEXT("Playback suppression sample requires 16 observed Geometry Collection components"));
        }
        int32 SuppressedSimulationCount = 0;
        int32 PlaybackEnabledPhysicsObjectCount = 0;
        int32 PlaybackKinematicPhysicsObjectCount = 0;
        int32 BodyInstanceSimulatePhysicsCount = 0;
        int32 ComponentReportsSimulatingPhysicsCount = 0;
        for (int32 ComponentIndex = 0; ComponentIndex < 16; ++ComponentIndex)
        {
            BodyInstanceSimulatePhysicsCount += Components[ComponentIndex]->BodyInstance.bSimulatePhysics ? 1 : 0;
            ComponentReportsSimulatingPhysicsCount += Components[ComponentIndex]->IsSimulatingPhysics() ? 1 : 0;
            if(!Work->CapturePhysics(Components[ComponentIndex]))return FLegacy::CreateErrorResponse(Work->R.Error);
            TArray<Chaos::FPhysicsObjectHandle> RawObjects,PhysicsObjects;
            if(!Step(TEXT("GetAllPhysicsObjects"),[&]{RawObjects=Components[ComponentIndex]->GetAllPhysicsObjects();},false))return FLegacy::CreateErrorResponse(Work->R.Error);
            if(RawObjects.Num()>1000000)return FLegacy::CreateErrorResponse(TEXT("Physics object budget exceeded"));
            TSet<Chaos::FPhysicsObjectHandle> Unique;for(auto*Object:RawObjects)if(Object){if(Unique.Contains(Object))return FLegacy::CreateErrorResponse(TEXT("Duplicate physics object handle"));Unique.Add(Object);PhysicsObjects.Add(Object);}
            bool bAllEnabledObjectsKinematic = !PhysicsObjects.IsEmpty();
            int32 ComponentEnabledObjectCount = 0;
            if (!PhysicsObjects.IsEmpty())
            {
                FLockedReadPhysicsObjectExternalInterface PhysicsObjectInterface =
                    FPhysicsObjectExternalInterface::LockRead(PhysicsObjects);
                for (Chaos::FPhysicsObjectHandle PhysicsObject : PhysicsObjects)
                {
                    if (PhysicsObjectInterface->AreAllDisabled({&PhysicsObject, 1}))
                    {
                        continue;
                    }
                    ++ComponentEnabledObjectCount;
                    ++PlaybackEnabledPhysicsObjectCount;
                    const bool bKinematic = PhysicsObjectInterface->AreAllKinematic({&PhysicsObject, 1});
                    PlaybackKinematicPhysicsObjectCount += bKinematic ? 1 : 0;
                    bAllEnabledObjectsKinematic &= bKinematic;
                }
            }
            bAllEnabledObjectsKinematic &= ComponentEnabledObjectCount > 0;
            SuppressedSimulationCount += bAllEnabledObjectsKinematic ? 1 : 0;
        }
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
        const uint64 FrameDelta = GFrameCounter - ActiveCachePlaybackSession->BeginFrame;
        const bool bPlaybackAssertions = FrameDelta > 0 && ActiveCachePlaybackSession->bCacheDataGate &&
            ActiveCachePlaybackSession->bHashesMatch && ActiveCachePlaybackSession->bHashesVaryAcrossTimes &&
            ActiveCachePlaybackSession->bIsolationPassed && SuppressedSimulationCount >= 16;
        Data = CacheCollectionToJson(Collection);
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("assertion_status"), bPlaybackAssertions ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Deterministic playback and suppression validation completed; five-run performance A/B remains"));
        Data->SetNumberField(TEXT("playback_begin_frame"), static_cast<double>(ActiveCachePlaybackSession->BeginFrame));
        Data->SetNumberField(TEXT("playback_sample_frame"), static_cast<double>(GFrameCounter));
        Data->SetNumberField(TEXT("playback_frame_delta"), static_cast<double>(FrameDelta));
        Data->SetBoolField(TEXT("physics_tick_observed"), FrameDelta > 0);
        Data->SetBoolField(TEXT("playback_hashes_match"), ActiveCachePlaybackSession->bHashesMatch);
        Data->SetBoolField(TEXT("playback_hashes_vary_across_times"), ActiveCachePlaybackSession->bHashesVaryAcrossTimes);
        Data->SetNumberField(TEXT("sample_time_0_25_hash_pass1"), ActiveCachePlaybackSession->Hashes[0][0]);
        Data->SetNumberField(TEXT("sample_time_0_50_hash_pass1"), ActiveCachePlaybackSession->Hashes[0][1]);
        Data->SetNumberField(TEXT("sample_time_1_00_hash_pass1"), ActiveCachePlaybackSession->Hashes[0][2]);
        Data->SetNumberField(TEXT("sample_time_0_25_hash_pass2"), ActiveCachePlaybackSession->Hashes[1][0]);
        Data->SetNumberField(TEXT("sample_time_0_50_hash_pass2"), ActiveCachePlaybackSession->Hashes[1][1]);
        Data->SetNumberField(TEXT("sample_time_1_00_hash_pass2"), ActiveCachePlaybackSession->Hashes[1][2]);
        Data->SetNumberField(TEXT("live_simulation_suppressed_count"), SuppressedSimulationCount);
        Data->SetNumberField(TEXT("playback_enabled_physics_object_count"), PlaybackEnabledPhysicsObjectCount);
        Data->SetNumberField(TEXT("playback_kinematic_physics_object_count"), PlaybackKinematicPhysicsObjectCount);
        Data->SetNumberField(TEXT("forced_kinematic_physics_object_count"), ActiveCachePlaybackSession->ForcedKinematicObjectCount);
        Data->SetNumberField(TEXT("body_instance_simulate_physics_true_count"), BodyInstanceSimulatePhysicsCount);
        Data->SetNumberField(TEXT("component_reports_simulating_physics_count"), ComponentReportsSimulatingPhysicsCount);
        Data->SetBoolField(TEXT("body_instance_cache_play_suppression_assertion"), BodyInstanceSimulatePhysicsCount == 0);
        Work->R.Data->SetBoolField(TEXT("postcondition_passed"),bPlaybackAssertions);
        Data->SetStringField(TEXT("manager_cache_mode"), TEXT("Play"));
        Data->SetBoolField(TEXT("cache_adapter_playback_initialized"), true);
        Data->SetNumberField(TEXT("state_hash_capture_ms"), ActiveCachePlaybackSession->StateHashCaptureMilliseconds);
        Data->SetNumberField(TEXT("sixteen_instance_timing_hash"), ActiveCachePlaybackSession->TimingHash);
        Data->SetBoolField(TEXT("frame_duration_gate"), ActiveCachePlaybackSession->bCacheDataGate);
        Data->SetBoolField(TEXT("performance_ab_available"), false);
        AddCacheIsolationEvidence();
        Work->ReleasePhysics();
        if(!EndManagerEvaluation(Manager,World,TEXT("EndEvaluate")))return FLegacy::CreateErrorResponse(Work->R.Error);
        OwnsEvaluation=false;
        Work->Event(TEXT("retire_playback_session"),false);Work->Complete=true;Work->ReleasePhysics();ActiveCachePlaybackSession.Reset();
        return FLegacy::CreateSuccessResponse(Data);
    }

    if (Operation == TEXT("playback_validate"))
    {
        StopMotion();
        if(!RetirePlayback(Manager,World))return FLegacy::CreateErrorResponse(Work->R.Error);
        TArray<UGeometryCollectionComponent*> Components = ResolveObservedComponents();
        if (Components.Num() < 16)
        {
            return FLegacy::CreateErrorResponse(TEXT("Playback validation requires at least 16 tagged observed Geometry Collection components"));
        }
        if(!ValidatePlaybackCollection(Collection,Manager,Components,true))return FLegacy::CreateErrorResponse(Work->R.Error);
        if(!EndManagerEvaluation(Manager,World,TEXT("EndEvaluate")))return FLegacy::CreateErrorResponse(Work->R.Error);
        OwnsEvaluation=false;
        if(!Step(TEXT("configure_playback_manager"),[&]{Manager->CacheCollection=Collection;Manager->CacheMode=ECacheMode::Play;Manager->StartMode=EStartMode::Timed;Work->ExpectManager(Manager,Collection,ECacheMode::Play,EStartMode::Timed);}))return FLegacy::CreateErrorResponse(Work->R.Error);
        const double TimingStart = FPlatformTime::Seconds();
        uint32 TimingHash = 0;
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const uint32 ComponentHash = HashGeometryComponentState(Components[Index]);
            TimingHash = FCrc::MemCrc32(&ComponentHash, sizeof(uint32), TimingHash);
        }
        const double StateHashCaptureMilliseconds = (FPlatformTime::Seconds() - TimingStart) * 1000.0;
        const float SampleTimes[] = {0.25f, 0.5f, 1.0f};
        TArray<TArray<uint32>> PlaybackHashes;
        for (int32 PassIndex = 0; PassIndex < 2; ++PassIndex)
        {
        if(!EndManagerEvaluation(Manager,World,TEXT("EndEvaluate")))return FLegacy::CreateErrorResponse(Work->R.Error);
        OwnsEvaluation=false;
        if(!Step(TEXT("ResetAllComponentTransforms"),[&]{Manager->ResetAllComponentTransforms();}))return FLegacy::CreateErrorResponse(Work->R.Error);
        if(!Step(TEXT("SetStartTime"),[&]{Manager->SetStartTime(0.0f);}))return FLegacy::CreateErrorResponse(Work->R.Error);
            TArray<uint32> PassHashes;
            for (float SampleTime : SampleTimes)
            {
        if(!Step(TEXT("SetStartTime"),[&]{Manager->SetStartTime(SampleTime);}))return FLegacy::CreateErrorResponse(Work->R.Error);
                uint32 CombinedHash = 0;
                for (int32 ComponentIndex = 0; ComponentIndex < 16; ++ComponentIndex)
                {
                    const uint32 ComponentHash = HashGeometryComponentState(Components[ComponentIndex]);
                    CombinedHash = FCrc::MemCrc32(&ComponentHash, sizeof(uint32), CombinedHash);
                }
                PassHashes.Add(CombinedHash);
            }
            PlaybackHashes.Add(PassHashes);
        }
        if(!EndManagerEvaluation(Manager,World,TEXT("EndEvaluate")))return FLegacy::CreateErrorResponse(Work->R.Error);
        OwnsEvaluation=false;
        for (int32 ComponentIndex = 0; ComponentIndex < 16; ++ComponentIndex)
        {
            if(!Step(TEXT("SetSimulatePhysics"),[&]{Components[ComponentIndex]->SetSimulatePhysics(false);}))return FLegacy::CreateErrorResponse(Work->R.Error);
            if(!Step(TEXT("SetDynamicState"),[&]{Components[ComponentIndex]->SetDynamicState(Chaos::EObjectStateType::Kinematic);}))return FLegacy::CreateErrorResponse(Work->R.Error);
            if(!Step(TEXT("RecreatePhysicsState"),[&]{Components[ComponentIndex]->RecreatePhysicsState();}))return FLegacy::CreateErrorResponse(Work->R.Error);
        }
#if WITH_EDITOR
        if(!Step(TEXT("SetObservedComponentProperties"),[&]{Manager->SetObservedComponentProperties(ECacheMode::Play);}))return FLegacy::CreateErrorResponse(Work->R.Error);
#endif
        if(!Step(TEXT("SetStartTime"),[&]{Manager->SetStartTime(0.0f);}))return FLegacy::CreateErrorResponse(Work->R.Error);
        if(!Step(TEXT("BeginEvaluate"),[&]{RememberEvaluation(Manager,World,Collection,ECacheMode::Play);Manager->BeginEvaluate();Work->R.Data->SetBoolField(TEXT("begin_evaluate_returned"),true);}))return FLegacy::CreateErrorResponse(Work->R.Error);
        for (int32 ComponentIndex = 0; ComponentIndex < 16; ++ComponentIndex)
        {
            if(!Step(TEXT("SetDynamicState"),[&]{Components[ComponentIndex]->SetDynamicState(Chaos::EObjectStateType::Kinematic);}))return FLegacy::CreateErrorResponse(Work->R.Error);
        }
#if WITH_EDITOR
        if(!Step(TEXT("SetObservedComponentProperties"),[&]{Manager->SetObservedComponentProperties(ECacheMode::Play);}))return FLegacy::CreateErrorResponse(Work->R.Error);
#endif
        TArray<Chaos::FPhysicsObjectHandle> PlaybackPhysicsObjects;
        for (int32 ComponentIndex = 0; ComponentIndex < 16; ++ComponentIndex)
        {
            if(!Work->CapturePhysics(Components[ComponentIndex]))return FLegacy::CreateErrorResponse(Work->R.Error);
            TArray<Chaos::FPhysicsObjectHandle> CurrentObjects;if(!Step(TEXT("GetAllPhysicsObjects"),[&]{CurrentObjects=Components[ComponentIndex]->GetAllPhysicsObjects();},false))return FLegacy::CreateErrorResponse(Work->R.Error);
            for(auto*Object:CurrentObjects)if(Object){if(PlaybackPhysicsObjects.Contains(Object))return FLegacy::CreateErrorResponse(TEXT("Duplicate playback physics object"));PlaybackPhysicsObjects.Add(Object);}
        }
        if (!PlaybackPhysicsObjects.IsEmpty())
        {
            if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);Work->Event(TEXT("controlled_ForceKinematic"));Work->Count(TEXT("controlled_ForceKinematic_calls"));
            FLockedWritePhysicsObjectExternalInterface PhysicsObjectInterface =
                FPhysicsObjectExternalInterface::LockWrite(PlaybackPhysicsObjects);
            PhysicsObjectInterface->ForceKinematic(PlaybackPhysicsObjects);
        }
        if(!Guard()||!Work->R.Error.IsEmpty())return FLegacy::CreateErrorResponse(Work->R.Error);
        bool bHashesMatch = PlaybackHashes.Num() == 2;
        for (int32 TimeIndex = 0; TimeIndex < 3 && bHashesMatch; ++TimeIndex)
        {
            bHashesMatch = PlaybackHashes[0][TimeIndex] == PlaybackHashes[1][TimeIndex];
        }
        const bool bHashesVaryAcrossTimes = PlaybackHashes[0][0] != PlaybackHashes[0][1] ||
            PlaybackHashes[0][1] != PlaybackHashes[0][2];
        const bool bCacheDataGate = Collection->GetMaxNumFrames() >= 30 && Collection->GetMaxDuration() >= 1.0f;
        Data = CacheCollectionToJson(Collection);
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Two deterministic random-access passes completed; call playback_suppression_sample after a physics frame to validate adapter-controlled kinematic state"));
        Data->SetNumberField(TEXT("observed_learning_component_count"), Components.Num());
        Data->SetNumberField(TEXT("playback_pass_count"), 2);
        Data->SetNumberField(TEXT("sample_time_0_25_hash_pass1"), PlaybackHashes[0][0]);
        Data->SetNumberField(TEXT("sample_time_0_50_hash_pass1"), PlaybackHashes[0][1]);
        Data->SetNumberField(TEXT("sample_time_1_00_hash_pass1"), PlaybackHashes[0][2]);
        Data->SetNumberField(TEXT("sample_time_0_25_hash_pass2"), PlaybackHashes[1][0]);
        Data->SetNumberField(TEXT("sample_time_0_50_hash_pass2"), PlaybackHashes[1][1]);
        Data->SetNumberField(TEXT("sample_time_1_00_hash_pass2"), PlaybackHashes[1][2]);
        Data->SetBoolField(TEXT("playback_hashes_match"), bHashesMatch);
        Data->SetBoolField(TEXT("playback_hashes_vary_across_times"), bHashesVaryAcrossTimes);
        Data->SetNumberField(TEXT("live_simulation_suppressed_count"), 0);
        Data->SetBoolField(TEXT("cache_adapter_playback_initialized"), true);
        Data->SetBoolField(TEXT("suppression_sample_pending"), true);
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        Data->SetNumberField(TEXT("playback_begin_frame"), static_cast<double>(GFrameCounter));
        Data->SetNumberField(TEXT("forced_kinematic_physics_object_count"), PlaybackPhysicsObjects.Num());
        Data->SetNumberField(TEXT("state_hash_capture_ms"), StateHashCaptureMilliseconds);
        Data->SetNumberField(TEXT("sixteen_instance_timing_hash"), TimingHash);
        Data->SetBoolField(TEXT("performance_ab_available"), false);
        Data->SetBoolField(TEXT("frame_duration_gate"), bCacheDataGate);
        AddCacheIsolationEvidence();
        const bool bPlaybackIsolationPassed = Data->GetBoolField(TEXT("uses_play_world_cache_duplicate")) &&
            !Data->GetBoolField(TEXT("source_asset_newly_dirtied"));
        Work->Event(TEXT("create_playback_validation_session"),false);ActiveCachePlaybackSession = MakeShared<FCachePlaybackValidationSession>();
        ActiveCachePlaybackSession->RequestedManagerSelector=ManagerName;ActiveCachePlaybackSession->RequestedCachePath=CachePath;ActiveCachePlaybackSession->Evidence=Owner;Work->Complete=false;Work->R.Data->SetStringField(TEXT("phase"),TEXT("playback_suppression_pending"));ActiveCacheEvidence=Owner;
        ActiveCachePlaybackSession->World = World;
        ActiveCachePlaybackSession->Manager = Manager;
        ActiveCachePlaybackSession->Collection = Collection;
        ActiveCachePlaybackSession->BeginFrame = GFrameCounter;
        ActiveCachePlaybackSession->bHashesMatch = bHashesMatch;
        ActiveCachePlaybackSession->bHashesVaryAcrossTimes = bHashesVaryAcrossTimes;
        ActiveCachePlaybackSession->bCacheDataGate = bCacheDataGate;
        ActiveCachePlaybackSession->bIsolationPassed = bPlaybackIsolationPassed;
        ActiveCachePlaybackSession->StateHashCaptureMilliseconds = StateHashCaptureMilliseconds;
        ActiveCachePlaybackSession->TimingHash = TimingHash;
        ActiveCachePlaybackSession->ForcedKinematicObjectCount = PlaybackPhysicsObjects.Num();
        for (int32 PassIndex = 0; PassIndex < 2; ++PassIndex)
        {
            for (int32 TimeIndex = 0; TimeIndex < 3; ++TimeIndex)
            {
                ActiveCachePlaybackSession->Hashes[PassIndex][TimeIndex] = PlaybackHashes[PassIndex][TimeIndex];
            }
        }
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (Operation == TEXT("configure_record") || Operation == TEXT("configure_play"))
    {
        StopMotion();
        if (ActiveCacheConfigureSession && ActiveCacheConfigureSession->Manager.Get() == Manager &&
            ActiveCacheConfigureSession->World.Get() == World)
        {
            if(ActiveCacheConfigureSession->Evidence&&Work!=ActiveCacheConfigureSession->Evidence.Get()){Work->Adopted=ActiveCacheConfigureSession->Evidence->Snapshot();Work->Complete=ActiveCacheConfigureSession->Evidence->Complete;Work->SessionId=ActiveCacheConfigureSession->Evidence->SessionId;}
            Collection = Manager->CacheCollection.Get();
            Data = CacheCollectionToJson(Collection);
            Data->SetStringField(TEXT("operation"), Operation);
            Data->SetBoolField(TEXT("cache_configure_completed"), ActiveCacheConfigureSession->bCompleted);
            Data->SetBoolField(TEXT("cache_configure_succeeded"), ActiveCacheConfigureSession->bSucceeded);
            Data->SetBoolField(TEXT("preserved_observed_components"), ActiveCacheConfigureSession->bPreservedObservedComponents);
            Data->SetNumberField(TEXT("end_evaluate_frame"), static_cast<double>(ActiveCacheConfigureSession->EndEvaluateFrame));
            Data->SetNumberField(TEXT("observed_component_count"), Manager->GetObservedComponents().Num());
            if (!ActiveCacheConfigureSession->bCompleted)
            {
                Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
                Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
                Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
                Data->SetStringField(TEXT("learning_blocker"), TEXT("Cache adapter quiescence tick has not completed"));
                Data->SetBoolField(TEXT("requires_followup_sample"), true);
                AddCacheIsolationEvidence();
                return FLegacy::CreateSuccessResponse(Data);
            }
            const bool bSucceeded = ActiveCacheConfigureSession->bSucceeded;
            const FString Failure = ActiveCacheConfigureSession->Failure;
            StopConfigure();
            Data->SetStringField(TEXT("operation_status"), bSucceeded ? TEXT("PASS") : TEXT("FAIL"));
            Data->SetStringField(TEXT("assertion_status"), bSucceeded ? TEXT("PASS") : TEXT("FAIL"));
            Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_blocker"), bSucceeded
                ? TEXT("Cache mode configured after adapter quiescence; trigger/record/playback validation remains")
                : TEXT("Deferred cache configuration failed"));
            Data->SetStringField(TEXT("failure"), Failure);
            Data->SetBoolField(TEXT("requires_trigger"), bSucceeded);
            AddCacheIsolationEvidence();
            return FLegacy::CreateSuccessResponse(Data);
        }
        StopConfigure();
        ActiveCacheConfigurationToken.Reset();
        if(!RetirePlayback(Manager,World))return FLegacy::CreateErrorResponse(Work->R.Error);
        const FString ComponentActorName = GetStringParam(Params, TEXT("component_actor_name"), TEXT(""));
        UGeometryCollectionComponent* Component = FindGeometryCollectionComponent(World, ComponentActorName);
        if (!Component || !Component->GetOwner()->Tags.Contains(LearningActorTag) ||
            !Component->GetRestCollection() || !IsWithinLearningRoot(Component->GetRestCollection()->GetPathName()))
        {
            return FLegacy::CreateErrorResponse(TEXT("Observed tagged learning Geometry Collection component not found"));
        }
        TArray<UGeometryCollectionComponent*> Components = ResolveObservedComponents();
        if (Components.Num() != 16)
        {
            Components.Reset();
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->Tags.Contains(LearningActorTag) && It->GetName().StartsWith(TEXT("Chaos_Cache_")))
                {
                    if (UGeometryCollectionComponent* CacheComponent = It->FindComponentByClass<UGeometryCollectionComponent>())
                    {
                        Components.Add(CacheComponent);
                    }
                }
            }
        }
        Components.Sort([](const UGeometryCollectionComponent& A, const UGeometryCollectionComponent& B)
        {
            return A.GetOwner()->GetName() < B.GetOwner()->GetName();
        });
        if (Components.Num() != 16)
        {
            return FLegacy::CreateErrorResponse(TEXT("Deferred cache configuration requires exactly 16 learning cache components"));
        }
        if(Operation==TEXT("configure_play")&&!ValidatePlaybackCollection(Collection,Manager,Components,Manager->GetObservedComponents().Num()==16))return FLegacy::CreateErrorResponse(Work->R.Error);
        if(!EndManagerEvaluation(Manager,World,TEXT("EndEvaluate")))return FLegacy::CreateErrorResponse(Work->R.Error);
        OwnsEvaluation=false;
        Work->Event(TEXT("create_configure_session"),false);ActiveCacheConfigureSession = MakeShared<FCacheConfigureSession>();
        ActiveCacheConfigureSession->RequestedManagerSelector=ManagerName;ActiveCacheConfigureSession->RequestedCachePath=CachePath;ActiveCacheConfigureSession->Evidence=Owner;ActiveCacheConfigureSession->Deadline=FPlatformTime::Seconds()+30;Work->Complete=false;Work->R.Data->SetStringField(TEXT("phase"),TEXT("configure_pending"));ActiveCacheEvidence=Owner;
        ActiveCacheConfigureSession->World = World;
        ActiveCacheConfigureSession->Manager = Manager;
        ActiveCacheConfigureSession->InputCollection =
            Operation == TEXT("configure_record") ? SourceCollection : Collection;
        ActiveCacheConfigureSession->RequestedMode =
            Operation == TEXT("configure_record") ? ECacheMode::Record : ECacheMode::Play;
        ActiveCacheConfigureSession->EndEvaluateFrame = GFrameCounter;
        for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ++ComponentIndex)
        {
            if(!TrackLearningComponent(Components[ComponentIndex]))return FLegacy::CreateErrorResponse(Work->R.Error);
            ActiveCacheConfigureSession->Components.Add(Components[ComponentIndex]);
            ActiveCacheConfigureSession->ComponentActorNames.Add(Components[ComponentIndex]->GetOwner()->GetName());
            ActiveCacheConfigureSession->CacheNames.Add(FName(*FString::Printf(TEXT("ChaosLearning_%02d"), ComponentIndex)));
        }
        ActiveCacheConfigureSession->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([](float)
            {
                return CompleteActiveCacheConfigureSession();
            }));
        Data = CacheCollectionToJson(Collection);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        AddCacheIsolationEvidence();
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("EndEvaluate completed; observed components and BeginEvaluate are deferred to the next editor tick"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("cache_manager_path"), Manager->GetPathName());
        Data->SetStringField(TEXT("observed_component_path"), Component->GetPathName());
        Data->SetNumberField(TEXT("observed_component_count"), Manager->GetObservedComponents().Num());
        Data->SetNumberField(TEXT("end_evaluate_frame"), static_cast<double>(GFrameCounter));
        Data->SetBoolField(TEXT("cache_configure_ticker_started"), ActiveCacheConfigureSession->TickerHandle.IsValid());
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (Operation == TEXT("trigger"))
    {
        if (!Collection || Collection == SourceCollection || !Collection->IsIn(Manager) ||
            !HasSuccessfulCacheConfiguration(ECacheMode::Record))
        {
            return FLegacy::CreateErrorResponse(
                TEXT("Cache trigger requires configure_record first so PIE recording cannot dirty the source cache asset"));
        }
        if (true)
        {
            if(!Step(TEXT("TriggerAll"),[&]{RememberEvaluation(Manager,World,Collection,ECacheMode::Record);Manager->TriggerAll();}))return FLegacy::CreateErrorResponse(Work->R.Error);
            RememberEvaluation(Manager,World,Collection,ECacheMode::Record);
            Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
            Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_blocker"), TEXT("Trigger dispatch requires frame/duration sampling and five-run A/B validation"));
            Data->SetBoolField(TEXT("trigger_dispatched"), true);
        }
        else
        {
            Data->SetStringField(TEXT("operation_status"), TEXT("FAIL"));
            Data->SetStringField(TEXT("assertion_status"), TEXT("FAIL"));
            Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_blocker"), TEXT("Cache trigger dispatch failed"));
            Data->SetStringField(TEXT("failure"), TEXT("TriggerAll UFUNCTION was not found"));
        }
        AddCacheIsolationEvidence();
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (Operation == TEXT("sample"))
    {
        if(!Step(TEXT("FlushAllCacheWrites"),[&]{Collection->FlushAllCacheWrites();}))return FLegacy::CreateErrorResponse(Work->R.Error);
        Data = CacheCollectionToJson(Collection);
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("world_context"), WorldContext);
        Data->SetStringField(TEXT("operation_status"), Collection->GetMaxNumFrames() > 0 && Collection->GetMaxDuration() > 0.0f ? TEXT("PASS") : TEXT("BLOCKED"));
        Data->SetStringField(TEXT("assertion_status"), Collection->GetMaxNumFrames() >= 30 && Collection->GetMaxDuration() >= 1.0f ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("A single cache sample cannot satisfy the required five-run A/B validation"));
        Data->SetBoolField(TEXT("frame_duration_gate"), Collection->GetMaxNumFrames() >= 30 && Collection->GetMaxDuration() >= 1.0f);
        AddCacheIsolationEvidence();
        if (Data->GetBoolField(TEXT("source_asset_newly_dirtied")))
        {
            Data->SetStringField(TEXT("assertion_status"), TEXT("FAIL"));
        }
        return FLegacy::CreateSuccessResponse(Data);
    }
    return FLegacy::CreateErrorResponse(FString::Printf(TEXT("Unsupported cache smoke operation: %s"), *Operation));
}

}
bool HasActiveSessionForWorld(const UWorld* W)
{
 if(!IsInGameThread())return true;if(!W)return false;
 if(Running&&Work&&Work->Writer&&Work->World.Get()==W)return true;
 if(OwnsEvaluation&&LastOwnedWorld.Get()==W)return true;
 if(ActiveCacheConfigureSession&&ActiveCacheConfigureSession->World.Get()==W&&(!ActiveCacheConfigureSession->bCompleted||!ActiveCacheConfigureSession->bSucceeded))return true;
 if(ActiveCacheRecordMotionSession&&ActiveCacheRecordMotionSession->World.Get()==W&&!ActiveCacheRecordMotionSession->bCaptureCompleted)return true;
 return ActiveCachePlaybackSession&&ActiveCachePlaybackSession->World.Get()==W;
}
FString CreateFixture(const FString&ParamsJson)
{
 auto W=MakeShared<FWork>(TEXT("create_chaos_destruction_learning_fixture"));TSharedPtr<FJsonObject>P;if(!Parse(ParamsJson,P)){W->Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return W->Finish(nullptr);}if(!SafeEditor()||Running){W->Fail(TEXT("editor_required"),TEXT("Non-reentrant safe Editor game-thread execution required"));return W->Finish(nullptr);}
 TGuardValue<bool>Busy(Running,true);TGuardValue<FWork*>Scope(Work,&W.Get());TGuardValue<TSharedPtr<FWork>>CallOwner(Owner,W);FExecutionPins Pins(*W);W->StartCall();if(!Pins.Acquire())return W->Finish(nullptr);auto Legacy=HandleCreateLearningFixture(P);if(Guard())W->Observe();W->Complete=true;W->R.Data->SetStringField(TEXT("strong_reference_scope"),TEXT("Request/ticker execution only; retained session identity is weak"));W->R.Data->SetBoolField(TEXT("atomic_cleanup_or_rollback_available"),false);return W->Finish(Legacy);
}
FString RunCacheSmoke(const FString&ParamsJson)
{
 auto Fresh=MakeShared<FWork>(TEXT("run_chaos_cache_smoke"));TSharedPtr<FJsonObject>P;if(!Parse(ParamsJson,P)){Fresh->Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return Fresh->Finish(nullptr);}if(!SafeEditor()||Running){Fresh->Fail(TEXT("editor_required"),TEXT("Non-reentrant safe Editor game-thread execution required"));return Fresh->Finish(nullptr);}
 const FString Op=GetStringParam(P,TEXT("operation"),TEXT("inspect"));Fresh->SDK(TEXT("resolve_request_world"));UWorld*RequestWorld=CurrentWorld();const FString RequestManager=GetStringParam(P,TEXT("cache_manager_actor_name"),TEXT("")),RequestCache=GetStringParam(P,TEXT("cache_collection_path"),*(FString(LearningRoot)/TEXT("CC_ChaosLearning")));const auto Matches=[&](const auto&S){return S&&S->World.Get()==RequestWorld&&S->RequestedManagerSelector.Equals(RequestManager,ESearchCase::IgnoreCase)&&S->RequestedCachePath.Equals(RequestCache,ESearchCase::IgnoreCase);};TSharedPtr<FWork>W=Fresh;
 if((Op==TEXT("configure_record")||Op==TEXT("configure_play"))&&Matches(ActiveCacheConfigureSession)&&ActiveCacheConfigureSession->Evidence)W=ActiveCacheConfigureSession->Evidence;
 else if(Op==TEXT("finalize_record")&&Matches(ActiveCacheRecordMotionSession)&&ActiveCacheRecordMotionSession->Evidence)W=ActiveCacheRecordMotionSession->Evidence;
 else if(Op==TEXT("playback_suppression_sample")&&Matches(ActiveCachePlaybackSession)&&ActiveCachePlaybackSession->Evidence)W=ActiveCachePlaybackSession->Evidence;
 TGuardValue<bool>Busy(Running,true);TGuardValue<FWork*>Scope(Work,W.Get());TGuardValue<TSharedPtr<FWork>>CallOwner(Owner,W);FExecutionPins Pins(*W);W->StartCall();
 if(ActiveCacheEvidence&&W!=ActiveCacheEvidence){W->R.Data->SetObjectField(TEXT("previous_session_execution"),ActiveCacheEvidence->Snapshot());if(Op==TEXT("sample")||Op==TEXT("inspect")){W->Adopted=ActiveCacheEvidence->Snapshot();W->Complete=ActiveCacheEvidence->Complete;W->SessionId=ActiveCacheEvidence->SessionId;}}
 if(!Pins.Acquire())return W->Finish(nullptr);if(W->SessionId.IsEmpty()&&Op!=TEXT("inspect"))W->SessionId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
 auto Legacy=HandleRunChaosCacheSmoke(P);if(Guard())W->Observe();if(!W->R.Error.IsEmpty()){W->ReleasePhysics();if(ActiveCacheConfigureSession&&ActiveCacheConfigureSession->Evidence==W){ActiveCacheConfigureSession->Failure=W->R.Error;ActiveCacheConfigureSession->bCompleted=true;StopConfigure(TEXT("failed_configure_ticker_stopped"));}if(ActiveCacheRecordMotionSession&&ActiveCacheRecordMotionSession->Evidence==W){ActiveCacheRecordMotionSession->CompletionReason=W->R.Error;ActiveCacheRecordMotionSession->bCaptureCompleted=true;StopMotion(TEXT("failed_motion_ticker_stopped"));}if(ActiveCachePlaybackSession&&ActiveCachePlaybackSession->Evidence==W){W->ReleasePhysics();W->Complete=true;}}
 if(W->Cancel||ShuttingDown){StopConfigure(TEXT("cancelled_on_shutdown"));StopMotion(TEXT("cancelled_on_shutdown"));W->ReleasePhysics();W->Complete=true;}
 W->R.Data->SetStringField(TEXT("cache_name_scope"),TEXT("Legacy cache_name input is intentionally ignored; sixteen deterministic ChaosLearning_00..15 mappings are used"));
 W->R.Data->SetStringField(TEXT("controlled_diagnostic_scope"),TEXT("record_motion_16 writes per-tick particle velocity and playback_validate explicitly forces Kinematic. These are legacy controlled diagnostics, not natural-simulation or adapter-only suppression proof."));
 W->R.Data->SetStringField(TEXT("cleanup_scope"),TEXT("Owned ticker/lifecycle handles are retired; a safe owned EndEvaluate can close SDK cache evaluation. Previously enqueued commands, component/manager settings, generated transient cache and changed physical transforms are not atomically restored or rolled back."));
 W->R.Data->SetBoolField(TEXT("physics_tick_observed_scope_is_GT_frame_delta_only"),true);W->R.Data->SetBoolField(TEXT("physics_step_completion_independently_verified"),false);W->R.Data->SetBoolField(TEXT("physical_state_rollback_verified"),false);W->R.Data->SetBoolField(TEXT("explicit_asset_save_performed"),false);W->R.Data->SetBoolField(TEXT("strong_pins_retained_between_requests"),false);
 LastSessionExecution=W->Snapshot();return W->Finish(Legacy);
}
void Shutdown()
{
 if(!IsInGameThread()){UE_LOG(LogTemp,Error,TEXT("Cubeless learning shutdown requires Editor game thread; unsafe UObject/physics cleanup skipped"));return;}
 auto E=ActiveCacheEvidence;auto Config=ActiveCacheConfigureSession;auto Motion=ActiveCacheRecordMotionSession;auto Playback=ActiveCachePlaybackSession;
 if(Config&&Config->TickerHandle.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Config->TickerHandle);Config->TickerHandle.Reset();}
 if(Motion&&Motion->TickerHandle.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Motion->TickerHandle);Motion->TickerHandle.Reset();}
 if(E)E->ReleasePhysics();if(Config&&Config->Evidence)Config->Evidence->ReleasePhysics();if(Motion&&Motion->Evidence)Motion->Evidence->ReleasePhysics();if(Playback&&Playback->Evidence)Playback->Evidence->ReleasePhysics();
 if(Running){ShuttingDown=true;if(E)E->Cancel=true;if(Config&&Config->Evidence)Config->Evidence->Cancel=true;if(Motion&&Motion->Evidence)Motion->Evidence->Cancel=true;if(Playback&&Playback->Evidence)Playback->Evidence->Cancel=true;UE_LOG(LogTemp,Warning,TEXT("Reentrant learning shutdown cancels owned callbacks; active scopes retain lifetime. Reentrant DLL unload is unsupported."));return;}
 TGuardValue<bool>Busy(Running,true);if(E)
 {
  TGuardValue<FWork*>Scope(Work,E.Get());FExecutionPins Pins(*E);const bool Pending=!E->Complete;
  if(OwnsEvaluation&&OwnedEvaluationMatches()&&E->R.Error.IsEmpty()&&Pins.Acquire())
  {
   const bool Safe=EndManagerEvaluation(LastOwnedManager.Get(),LastOwnedWorld.Get(),TEXT("shutdown_EndEvaluate"));E->R.Data->SetBoolField(TEXT("shutdown_end_evaluate_returned"),Safe);if(Safe)E->Observe();
  }
  else if(OwnsEvaluation)E->R.Data->SetBoolField(TEXT("unsafe_or_foreign_shutdown_evaluation_cleanup_skipped"),true);
  E->Complete=true;if(Pending&&E->R.Error.IsEmpty())E->Fail(TEXT("module_shutdown"),TEXT("Pending legacy controlled diagnostic cancelled; prior physics/cache effects were not rolled back"));E->R.Data->SetBoolField(TEXT("cleanup_complete"),true);E->R.Data->SetBoolField(TEXT("physical_restore_verified"),false);E->R.Data->SetBoolField(TEXT("owned_evaluation_lease_unresolved"),OwnsEvaluation);LastSessionExecution=E->Snapshot();UE_LOG(LogTemp,Display,TEXT("Cubeless learning shutdown receipt: %s"),*Json(LastSessionExecution));
 }
 ShuttingDown=true;ActiveCacheConfigureSession.Reset();ActiveCacheRecordMotionSession.Reset();ActiveCachePlaybackSession.Reset();ActiveCacheConfigurationToken.Reset();ActiveCacheEvidence.Reset();if(!OwnsEvaluation){LastOwnedManager.Reset();LastOwnedWorld.Reset();LastOwnedCollection.Reset();}
}
}
