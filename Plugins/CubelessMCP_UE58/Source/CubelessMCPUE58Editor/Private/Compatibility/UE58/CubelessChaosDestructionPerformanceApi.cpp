#include "Compatibility/UE58/CubelessChaosDestructionPerformanceApi.h"
#include "Compatibility/UE58/CubelessChaosPerformanceBreakListener.h"
#include "Compatibility/UE58/CubelessChaosCacheSessionApi.h"
#include "Compatibility/UE58/CubelessChaosTraceStrainApi.h"
#include "Compatibility/UE58/CubelessChaosLearningApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Chaos/ChaosGameplayEventDispatcher.h"
#include "Chaos/ChaosSolverActor.h"
#include "Chaos/Collision/ParticleCollisions.h"
#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/Facades/CollectionRemoveOnBreakFacade.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionEngineRemoval.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionRootProxyRenderer.h"
#include "GeometryCollectionProxyData.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/Crc.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "Physics/Experimental/ChaosEventType.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "DynamicRHI.h"
#include "RHIStats.h"
#include "RenderTimer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include <atomic>
#include <cmath>

namespace Cubeless::UE58::ChaosDestructionPerformance
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* LearningRoot=TEXT("/Game/_MCP_Temp/ChaosDestructionLearning");
const FName LearningActorTag(TEXT("MCP_ChaosLearning"));
bool Running=false,ShuttingDown=false,Restoring=false;
struct FWork;
TSharedPtr<FWork> CurrentWork,LastWork;
TArray<TSharedPtr<FJsonValue>> RetiredSessions;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!ShuttingDown;}
UWorld* ActiveWorld(){if(!GEditor)return nullptr;if(auto*C=GEditor->GetPIEWorldContext()){if(C->World())return C->World();}return GEditor->GetEditorWorldContext().World();}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
struct FExactKeys:DefaultKeyFuncs<FString>{static bool Matches(const FString&A,const FString&B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString&S){return FCrc::StrCrc32(*S);}};
bool Parse(const FString&S,TSharedPtr<FJsonObject>&O)
{
 if(S.IsEmpty()||S.Len()>1048576)return false;for(TCHAR C:S)if(C==0)return false;
 auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeys>> Keys;EJsonNotation Token;int32 Visits=0;
 while(Reader->ReadNext(Token)){if(Token==EJsonNotation::Error||++Visits>262144)return false;if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()){FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K))return false;Keys.Last().Add(K);}if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart){Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>64)return false;}else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd){if(Objects.IsEmpty())return false;Objects.Pop();Keys.Pop();}if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber()))return false;}
 return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O)&&O.IsValid();
}
bool CollectionShape(const FGeometryCollection& C)
{
 const int32 T=C.NumElements(FGeometryCollection::TransformGroup),G=C.NumElements(FGeometryCollection::GeometryGroup),V=C.NumElements(FGeometryCollection::VerticesGroup),F=C.NumElements(FGeometryCollection::FacesGroup);
 if(T<0||T>1000000||G<0||G>1000000||V<0||V>8000000||F<0||F>8000000||C.Transform.Num()!=T||C.Parent.Num()!=T||C.Children.Num()!=T||C.SimulationType.Num()!=T||C.TransformToGeometryIndex.Num()!=T||C.TransformIndex.Num()!=G||C.VertexStart.Num()!=G||C.VertexCount.Num()!=G||C.FaceStart.Num()!=G||C.FaceCount.Num()!=G||C.BoundingBox.Num()!=G||C.Vertex.Num()!=V||C.Indices.Num()!=F)return false;
 for(int32 I=0;I<T;++I){if(C.Transform[I].ContainsNaN())return false;int32 P=C.Parent[I],N=0;if(P!=INDEX_NONE&&(!C.Parent.IsValidIndex(P)||!C.Children[P].Contains(I)))return false;for(int32 Child:C.Children[I])if(!C.Parent.IsValidIndex(Child)||C.Parent[Child]!=I)return false;while(P!=INDEX_NONE){if(!C.Parent.IsValidIndex(P)||++N>T)return false;P=C.Parent[P];}int32 GI=C.TransformToGeometryIndex[I];if(GI!=INDEX_NONE&&(!C.TransformIndex.IsValidIndex(GI)||C.TransformIndex[GI]!=I))return false;}
 for(int32 I=0;I<G;++I){int32 TI=C.TransformIndex[I];if(!C.TransformToGeometryIndex.IsValidIndex(TI)||C.TransformToGeometryIndex[TI]!=I||C.VertexStart[I]<0||C.VertexCount[I]<0||static_cast<int64>(C.VertexStart[I])+C.VertexCount[I]>V||C.FaceStart[I]<0||C.FaceCount[I]<0||static_cast<int64>(C.FaceStart[I])+C.FaceCount[I]>F)return false;}
 for(int32 I=0;I<V;++I)if(C.Vertex[I].ContainsNaN())return false;
 for(int32 I=0;I<F;++I)for(int32 K=0;K<3;++K)if(!C.Vertex.IsValidIndex(C.Indices[I][K]))return false;return true;
}
struct FTarget{TWeakObjectPtr<const UObject> Weak,Outer,Owner,Rest;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package;TStrongObjectPtr<const UObject> Hold{nullptr};FString Path,Before;};
struct FEpoch{const UActorComponent*Target=nullptr;std::atomic<uint64> Revision{0};explicit FEpoch(const UActorComponent* C):Target(C){}};
struct FPhysics{TWeakObjectPtr<UPrimitiveComponent> Component;TSharedPtr<FEpoch,ESPMode::ThreadSafe> Epoch;FDelegateHandle Create,Destroy;const void*Proxy=nullptr;const void*Dynamic=nullptr;bool Registered=false,Created=false;};
struct FQueueTicket{std::atomic<int32> State{0};TArray<TPair<TSharedPtr<FEpoch,ESPMode::ThreadSafe>,uint64>> Epochs;bool Fresh()const{for(const auto&E:Epochs)if(E.Key->Revision.load(std::memory_order_acquire)!=E.Value)return false;return true;}};
struct FSubscription{TWeakObjectPtr<UGeometryCollectionComponent> Component;TStrongObjectPtr<UCubelessChaosPerformanceBreakListener> Listener{nullptr};bool OriginalNotify=false;bool AppliedNotify=true;};
TSharedPtr<FJsonObject> Projection(const UObject*O)
{
 auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("live"),Live(O));if(!Live(O))return J;J->SetStringField(TEXT("path"),O->GetPathName());J->SetStringField(TEXT("class"),O->GetClass()->GetPathName());J->SetStringField(TEXT("package"),O->GetPackage()->GetName());J->SetBoolField(TEXT("dirty"),O->GetPackage()->IsDirty());
 if(auto*A=Cast<AActor>(O))J->SetStringField(TEXT("transform"),A->GetActorTransform().ToString());
 if(auto*C=Cast<UPrimitiveComponent>(O)){J->SetStringField(TEXT("transform"),C->GetComponentTransform().ToString());J->SetBoolField(TEXT("simulate"),C->BodyInstance.bSimulatePhysics);J->SetBoolField(TEXT("gravity"),C->IsGravityEnabled());J->SetNumberField(TEXT("collision"),static_cast<int32>(C->GetCollisionEnabled()));J->SetStringField(TEXT("profile"),C->GetCollisionProfileName().ToString());J->SetStringField(TEXT("velocity"),C->GetPhysicsLinearVelocity().ToString());}
 if(auto*C=Cast<UGeometryCollectionComponent>(O)){J->SetStringField(TEXT("rest"),C->GetRestCollection()?C->GetRestCollection()->GetPathName():FString());J->SetBoolField(TEXT("notify"),C->bNotifyBreaks);J->SetNumberField(TEXT("one_way"),C->OneWayInteractionLevel);J->SetStringField(TEXT("solver"),C->ChaosSolverActor?C->ChaosSolverActor->GetPathName():FString());}
 if(auto*A=Cast<AChaosSolverActor>(O)){const auto&D=A->Properties.DestructionSettings;J->SetNumberField(TEXT("break_allowed"),D.PerAdvanceBreaksAllowed);J->SetNumberField(TEXT("reschedule"),D.PerAdvanceBreaksRescheduleLimit);J->SetNumberField(TEXT("release_min"),D.ClusteringParticleReleaseThrottlingMinCount);J->SetNumberField(TEXT("release_max"),D.ClusteringParticleReleaseThrottlingMaxCount);J->SetBoolField(TEXT("optimize"),D.bOptimizeForRuntimeMemory);}
 return J;
}
struct FWork:TSharedFromThis<FWork>
{
 FResult R;FString Operation,Group,SessionId=FGuid::NewGuid().ToString(EGuidFormats::Digits),FailedStage;TWeakObjectPtr<UWorld> World;TArray<FTarget> Targets;TArray<FPhysics> Physics;TArray<TSharedPtr<FQueueTicket,ESPMode::ThreadSafe>> Queue;TArray<FSubscription> Subscriptions;TMap<TWeakObjectPtr<const UGeometryCollectionComponent>,bool> NotifyBefore;TSet<TWeakObjectPtr<UGeometryCollectionComponent>> NotifyTouched;
 TArray<TSharedPtr<FJsonValue>> Prefix;uint64 Serial=0,Changed=0,CallSerial=0,CallChanged=0;int32 CallPrefix=0;bool Pending=false,Cancel=false,Writer=true,HasWorld=false,AllowErrorObservation=false;uint32 PinDepth=0;double Deadline=FPlatformTime::Seconds()+180.0;
 FWork(){R.Compiled=false;R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_compile_performed"),false);R.Data->SetBoolField(TEXT("explicit_save_performed"),false);R.Data->SetBoolField(TEXT("physics_completion_independently_verified"),false);}
 ~FWork(){if(IsInGameThread())ReleasePhysics();}
 bool Fail(const TCHAR*Code,const FString&Error){if(R.Error.IsEmpty()){R.ErrorCode=TEXT("chaos_destruction_performance_")+FString(Code);R.Error=Error;FailedStage=R.Stage;}return false;}
 void SDK(const TCHAR*Stage,bool Write){R.Stage=Stage;R.bChangeStateKnown=false;if(Write){R.bMutationAttempted=true;++Serial;auto E=MakeShared<FJsonObject>();E->SetNumberField(TEXT("index"),Serial);E->SetStringField(TEXT("stage"),Stage);E->SetBoolField(TEXT("sdk_returned"),false);if(Prefix.Num()<65536)Prefix.Add(MakeShared<FJsonValueObject>(E));else R.Data->SetBoolField(TEXT("actual_prefix_truncated"),true);}}
 bool Guard()
 {
  if(!Safe()||Cancel)return Fail(TEXT("unsafe_context"),TEXT("Editor lifetime/game thread unavailable, shutdown, GC or cancellation"));
  if(HasWorld&&(!Live(World.Get())||World->bIsTearingDown||ActiveWorld()!=World.Get()))return Fail(TEXT("world_identity_lost"),TEXT("Captured world changed or is tearing down; no replay/reload was attempted"));
  if(Writer&&HasWorld&&(ChaosCacheSessions::HasActiveSessionForWorld(World.Get())||ChaosTraceStrain::HasActiveSessionForWorld(World.Get())||ChaosLearning::HasActiveSessionForWorld(World.Get())))return Fail(TEXT("foreign_session_conflict"),TEXT("FW/FZ/GA owns a pending writer/evaluation lease in this world"));
  for(const auto&T:Targets){auto*O=T.Weak.Get();if(!Live(O)||!Live(T.Package.Get())||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive))return Fail(TEXT("target_identity_lost"),TEXT("Captured object class/outer/package/path changed"));if((O->IsA<AActor>()||O->IsA<UActorComponent>())&&O->GetWorld()!=World.Get())return Fail(TEXT("target_world_lost"),TEXT("Captured target left the expected world"));if(auto*C=Cast<UActorComponent>(O)){if(!Live(C->GetOwner())||C->GetOwner()!=T.Owner.Get())return Fail(TEXT("component_owner_lost"),TEXT("Captured component owner was destroyed"));if(auto*G=Cast<UGeometryCollectionComponent>(C)){if(G->GetRestCollection()!=T.Rest.Get()||C->GetOwner()->FindComponentByClass<UGeometryCollectionComponent>()!=C)return Fail(TEXT("component_binding_changed"),TEXT("Original GC/rest binding changed"));}if(auto*A=Cast<AStaticMeshActor>(C->GetOwner()))if(C->IsA<UStaticMeshComponent>()&&A->GetStaticMeshComponent()!=C)return Fail(TEXT("static_mesh_binding_changed"),TEXT("Original witness/floor component binding changed"));}}
  for(const auto&P:Physics){auto*C=P.Component.Get();if(!Live(C)||P.Epoch->Revision.load(std::memory_order_acquire)!=0||C->IsRegistered()!=P.Registered||C->IsPhysicsStateCreated()!=P.Created)return Fail(TEXT("physics_epoch_lost"),TEXT("Captured physics lifecycle changed; queued commands are not replayed"));const void*Proxy=nullptr;const void*Dynamic=nullptr;if(auto*G=Cast<UGeometryCollectionComponent>(C)){Proxy=G->GetPhysicsProxy();Dynamic=G->GetDynamicCollection();}else Proxy=C->BodyInstance.GetPhysicsActor();if(Proxy!=P.Proxy||Dynamic!=P.Dynamic)return Fail(TEXT("physics_proxy_lost"),TEXT("Physics proxy/dynamic identity changed"));}
  return true;
 }
 bool Check(){return (Restoring||AllowErrorObservation||R.Error.IsEmpty())&&Guard();}
 bool Track(const UObject*O)
 {
  if(!Safe()||!Live(O)||!Live(O->GetPackage()))return Fail(TEXT("target_unavailable"),TEXT("Live target required"));for(auto&T:Targets)if(T.Weak.Get()==O)return Guard();if(Targets.Num()>=8192)return Fail(TEXT("target_budget"),TEXT("8192 captured target budget exceeded"));auto&T=Targets.AddDefaulted_GetRef();T.Weak=O;T.Outer=O->GetOuter();if(auto*C=Cast<UActorComponent>(O))T.Owner=C->GetOwner();if(auto*G=Cast<UGeometryCollectionComponent>(O))T.Rest=G->GetRestCollection();T.Class=O->GetClass();T.Package=O->GetPackage();T.Path=O->GetPathName();T.Hold.Reset(O);SDK(TEXT("capture_target"),false);T.Before=Json(Projection(O));
  if(auto*G=Cast<UGeometryCollectionComponent>(O)){NotifyBefore.FindOrAdd(TWeakObjectPtr<const UGeometryCollectionComponent>(G),G->bNotifyBreaks);const auto*Rest=G->GetRestCollection();if(Rest){if(!Track(Rest))return false;auto C=Rest->GetGeometryCollection();if(C&&!CollectionShape(*C))return Fail(TEXT("collection_shape"),TEXT("Managed collection shape/index/hierarchy admission failed before SDK indexing"));if(const auto*D=G->GetDynamicCollection()){const int32 N=D->Active.Num();if(N<0||N>1000000||D->DynamicState.Num()!=N||D->GetInternalClusterParentTypeAttribute().Num()!=N||D->GetHasParent().Num()!=N||D->GetNumTransforms()!=N||(C&&N<C->Transform.Num()))return Fail(TEXT("dynamic_collection_shape"),TEXT("Dynamic facade arrays must cover the captured rest transforms"));}}}
  return Guard();
 }
 bool CaptureWorld(UWorld*W){if(!Safe()||!Live(W)||W->bIsTearingDown)return Fail(TEXT("world_unavailable"),TEXT("Live Editor/PIE world required"));World=W;HasWorld=true;return Track(W);}
 void ReleasePhysics(){for(auto&P:Physics){if(P.Create.IsValid())UActorComponent::GlobalCreatePhysicsDelegate.Remove(P.Create);if(P.Destroy.IsValid())UActorComponent::GlobalDestroyPhysicsDelegate.Remove(P.Destroy);}Physics.Reset();}
 bool CapturePhysics(UPrimitiveComponent*C,bool Refresh=false)
 {
  if(!Check()||!Live(C)||!Track(C))return false;for(int32 I=0;I<Physics.Num();++I)if(Physics[I].Component.Get()==C){if(!Refresh)return Guard();auto&P=Physics[I];P.Epoch->Revision.fetch_add(1,std::memory_order_release);UActorComponent::GlobalCreatePhysicsDelegate.Remove(P.Create);UActorComponent::GlobalDestroyPhysicsDelegate.Remove(P.Destroy);Physics.RemoveAt(I);break;}
  auto&P=Physics.AddDefaulted_GetRef();P.Component=C;P.Epoch=MakeShared<FEpoch,ESPMode::ThreadSafe>(C);auto E=P.Epoch;P.Create=UActorComponent::GlobalCreatePhysicsDelegate.AddLambda([E](UActorComponent*V){if(V==E->Target)E->Revision.fetch_add(1,std::memory_order_release);});P.Destroy=UActorComponent::GlobalDestroyPhysicsDelegate.AddLambda([E](UActorComponent*V){if(V==E->Target)E->Revision.fetch_add(1,std::memory_order_release);});P.Registered=C->IsRegistered();P.Created=C->IsPhysicsStateCreated();if(auto*G=Cast<UGeometryCollectionComponent>(C)){P.Proxy=G->GetPhysicsProxy();P.Dynamic=G->GetDynamicCollection();}else P.Proxy=C->BodyInstance.GetPhysicsActor();return Guard();
 }
 void Observe(){if(!Guard())return;TArray<TSharedPtr<FJsonValue>> Rows;for(auto&T:Targets){auto A=Projection(T.Weak.Get());const FString After=Json(A);const bool Diff=!T.Before.Equals(After,ESearchCase::CaseSensitive);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("captured_path"),T.Path);J->SetStringField(TEXT("before"),T.Before);J->SetObjectField(TEXT("after"),A);J->SetBoolField(TEXT("changed"),Diff);Rows.Add(MakeShared<FJsonValueObject>(J));if(Diff){++Changed;R.bChanged=true;}T.Before=After;}R.Data->SetArrayField(TEXT("actual_target_observations"),Rows);}
 TSharedRef<FJsonObject> Snapshot(){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("session_id"),SessionId);J->SetStringField(TEXT("operation"),Operation);J->SetStringField(TEXT("behavior_group"),Group);J->SetBoolField(TEXT("operation_complete"),!Pending);J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("failed_stage"),FailedStage);J->SetStringField(TEXT("error"),R.Error);J->SetStringField(TEXT("error_code"),R.ErrorCode);J->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);J->SetBoolField(TEXT("changed"),R.bChanged);J->SetBoolField(TEXT("execution_mutation_attempted"),R.bMutationAttempted);J->SetBoolField(TEXT("compile_attempted"),false);J->SetBoolField(TEXT("compiled"),false);J->SetBoolField(TEXT("save_requested"),false);J->SetBoolField(TEXT("save_attempted"),false);J->SetBoolField(TEXT("saved"),false);J->SetArrayField(TEXT("actual_mutation_prefix"),Prefix);const TSharedPtr<FJsonObject>*Legacy=nullptr;if(R.Data->TryGetObjectField(TEXT("legacy_result"),Legacy)&&Legacy){J->SetObjectField(TEXT("legacy_result"),*Legacy);const TSharedPtr<FJsonObject>*Leaf=nullptr;FString Assertion;if((*Legacy)->TryGetObjectField(TEXT("data"),Leaf)&&Leaf&&(*Leaf)->TryGetStringField(TEXT("assertion_status"),Assertion))J->SetBoolField(TEXT("postcondition_passed"),!Pending&&R.Error.IsEmpty()&&Assertion.Equals(TEXT("PASS"),ESearchCase::CaseSensitive));}J->SetStringField(TEXT("verification_scope"),TEXT("Original selected operation assertion only; external five-run aggregate and wider pipeline remain unverified"));const TArray<TSharedPtr<FJsonValue>>*Rows=nullptr;if(R.Data->TryGetArrayField(TEXT("actual_target_observations"),Rows)&&Rows)J->SetArrayField(TEXT("actual_target_observations"),*Rows);int32 Queued=0,Started=0,Completed=0,Cancelled=0;for(const auto&T:Queue){const int32 S=T->State.load(std::memory_order_acquire);Queued+=S==0;Started+=S==1;Completed+=S==2;Cancelled+=S<0;}J->SetNumberField(TEXT("physics_queue_pending_count"),Queued);J->SetNumberField(TEXT("physics_queue_running_count"),Started);J->SetNumberField(TEXT("physics_queue_callback_returned_count"),Completed);J->SetNumberField(TEXT("physics_queue_cancelled_before_start_count"),Cancelled);J->SetBoolField(TEXT("physics_effect_completion_verified"),false);return J;}
 FString Finish(const TSharedPtr<FJsonObject>&Legacy)
 {
  Observe();bool SuccessValue=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),SuccessValue);if(!SuccessValue&&Legacy&&R.Error.IsEmpty()){FString E;if(Legacy->TryGetStringField(TEXT("error"),E))Fail(TEXT("legacy_failed"),E);}R.bSuccess=SuccessValue&&R.Error.IsEmpty();R.bPostconditionPassed=R.bSuccess;R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());R.Data->SetBoolField(TEXT("operation_complete"),!Pending);R.Data->SetStringField(TEXT("session_id"),SessionId);R.Data->SetObjectField(TEXT("session_execution"),Snapshot());R.Data->SetBoolField(TEXT("postcondition_passed"),R.bPostconditionPassed);R.Data->SetBoolField(TEXT("actual_solver_step_independently_observed"),false);R.Data->SetBoolField(TEXT("legacy_physics_tick_observed_is_game_frame_delta"),true);R.Data->SetStringField(TEXT("semantic_restore_scope"),TEXT("Owned tickers/listeners, original NotifyBreaks, gravity, sibling simulation collision and solver settings where exact identity/current-value admission succeeds; no undo of fracture/decay/velocity/queued physics"));R.Data->SetBoolField(TEXT("full_physics_rollback_available"),false);R.Data->SetArrayField(TEXT("retired_session_receipts"),RetiredSessions);TArray<TSharedPtr<FJsonValue>>CurrentPrefix;for(int32 I=CallPrefix;I<Prefix.Num();++I)CurrentPrefix.Add(Prefix[I]);R.Data->SetArrayField(TEXT("actual_mutation_prefix"),CurrentPrefix);R.Data->SetBoolField(TEXT("current_request_saved"),false);
  const bool HistoryAttempted=R.bMutationAttempted,HistoryChanged=R.bChanged;R.bMutationAttempted=Serial!=CallSerial;R.bChanged=Changed!=CallChanged;R.Stage=R.Error.IsEmpty()?(Pending?TEXT("pending"):TEXT("complete")):FailedStage;FString Out= Cubeless::Operations::EncodeVariableResult(R,TEXT("run_chaos_destruction_performance_smoke"));R.bMutationAttempted=HistoryAttempted;R.bChanged=HistoryChanged;return Out;
 }
};
bool Check(){return CurrentWork&&CurrentWork->Check();}
bool BeforeSDK(const TCHAR*Stage,bool Write=true){if(!Check())return false;CurrentWork->SDK(Stage,Write);return true;}
bool AfterSDK(){if(CurrentWork)for(int32 I=CurrentWork->Prefix.Num()-1;I>=0;--I){auto P=CurrentWork->Prefix[I]->AsObject();bool Returned=false;if(P&&P->TryGetBoolField(TEXT("sdk_returned"),Returned)&&!Returned){P->SetBoolField(TEXT("sdk_returned"),true);break;}}return Check();}
void DropPhysics(UPrimitiveComponent*C)
{
 if(!CurrentWork)return;for(int32 I=CurrentWork->Physics.Num()-1;I>=0;--I){auto&P=CurrentWork->Physics[I];if(P.Component.Get()!=C)continue;P.Epoch->Revision.fetch_add(1,std::memory_order_release);UActorComponent::GlobalCreatePhysicsDelegate.Remove(P.Create);UActorComponent::GlobalDestroyPhysicsDelegate.Remove(P.Destroy);CurrentWork->Physics.RemoveAt(I);}
}
bool BeforeObjectSDK(UObject*O,const TCHAR*Stage)
{
 if(!Check()||!CurrentWork->Track(O))return false;if(auto*C=Cast<UActorComponent>(O))if(!CurrentWork->Track(C->GetOwner()))return false;if(auto*P=Cast<UPrimitiveComponent>(O)){if(!CurrentWork->CapturePhysics(P))return false;}if(FString(Stage).Equals(TEXT("SetNotifyBreaks"),ESearchCase::CaseSensitive))if(auto*G=Cast<UGeometryCollectionComponent>(O))CurrentWork->NotifyTouched.Add(G);return BeforeSDK(Stage);
}
void RefreshPhysics(UPrimitiveComponent*C){if(CurrentWork)CurrentWork->CapturePhysics(C,true);}
TSharedPtr<FJsonObject> Success(const TSharedPtr<FJsonObject>&Data){auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),true);if(Data)J->SetObjectField(TEXT("data"),Data);return J;}
TSharedPtr<FJsonObject> LegacyError(const FString&E){auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),E);return J;}
int32 CastI32(double V){if(!FMath::IsFinite(V)||V<MIN_int32||V>MAX_int32){if(CurrentWork)CurrentWork->Fail(TEXT("numeric_range"),TEXT("Original int32 SDK conversion is outside its defined safe range"));return 0;}return static_cast<int32>(V);}
float CastFloat(double V){if(!FMath::IsFinite(V)||FMath::Abs(V)>MAX_flt){if(CurrentWork)CurrentWork->Fail(TEXT("numeric_range"),TEXT("Original float SDK conversion is outside its finite range"));return 0;}return static_cast<float>(V);}
struct FExecutionScope
{
 TSharedPtr<FWork> Work,Previous;bool Valid=false,Pinned=false;explicit FExecutionScope(TSharedPtr<FWork>W):Work(MoveTemp(W)),Previous(CurrentWork){CurrentWork=Work;if(!Work||!Safe())return;Pinned=true;if(Work->PinDepth++==0)for(auto&T:Work->Targets){auto*O=T.Weak.Get();if(!Live(O))return;T.Hold.Reset(O);}Valid=Work->Guard();}
 ~FExecutionScope(){if(Work&&Pinned&&--Work->PinDepth==0)for(auto&T:Work->Targets)T.Hold.Reset();CurrentWork=Previous;}
};
bool HasPendingWork(const FWork* Work);
void TerminateWork(const TSharedPtr<FWork>&,const TCHAR*,bool);
template<typename F>bool TickOwned(TSharedPtr<FWork>Work,F&&Body)
{
 if(!Work||Running||ShuttingDown)return false;TGuardValue<bool> Busy(Running,true);TGuardValue<bool> WriterAdmission(Work->Writer,true);TGuardValue<bool> ErrorObservation(Work->AllowErrorObservation,false);FExecutionScope Scope(Work);if(!Scope.Valid||FPlatformTime::Seconds()>Work->Deadline){Work->Fail(TEXT("session_timeout_or_lifetime"),TEXT("Session exceeded monotonic deadline or target lifetime"));TerminateWork(Work,TEXT("ticker_lifetime_or_monotonic_timeout"),true);return false;}const bool More=Body();Work->Pending=HasPendingWork(Work.Get());if(!Work->R.Error.IsEmpty())TerminateWork(Work,TEXT("ticker_failure"),true);Work->Observe();if(!More&&Work->R.Error.IsEmpty())Work->R.Stage=TEXT("session_complete");return More;
}
template<typename F>void EnqueueSafe(Chaos::FPBDRigidsSolver*Solver,FGeometryCollectionPhysicsProxy* /*Proxy*/,F&&Body)
{
 if(!Solver||!BeforeSDK(TEXT("enqueue_physics_command")))return;auto Ticket=MakeShared<FQueueTicket,ESPMode::ThreadSafe>();for(const auto&P:CurrentWork->Physics)Ticket->Epochs.Add({P.Epoch,P.Epoch->Revision.load(std::memory_order_acquire)});CurrentWork->Queue.Add(Ticket);
 Solver->EnqueueCommandImmediate([Ticket,Command=Forward<F>(Body)]()mutable{int32 Expected=0;if(!Ticket->Fresh()){Ticket->State.compare_exchange_strong(Expected,-1,std::memory_order_acq_rel);return;}if(!Ticket->State.compare_exchange_strong(Expected,1,std::memory_order_acq_rel))return;ON_SCOPE_EXIT{Ticket->State.store(2,std::memory_order_release);};if(Ticket->Fresh())Command();});AfterSDK();
}
bool RegisterOwnedBreak(UGeometryCollectionComponent*C,TFunction<void(const FChaosBreakEvent&)> Callback)
{
 if(!Check()||!CurrentWork->Track(C))return false;auto&Sub=CurrentWork->Subscriptions.AddDefaulted_GetRef();Sub.Component=C;Sub.OriginalNotify=CurrentWork->NotifyBefore.FindRef(TWeakObjectPtr<const UGeometryCollectionComponent>(C));Sub.Listener.Reset(NewObject<UCubelessChaosPerformanceBreakListener>(GetTransientPackage(),NAME_None,RF_Transient|RF_TextExportTransient|RF_DuplicateTransient));auto*L=Sub.Listener.Get();if(!L)return CurrentWork->Fail(TEXT("listener_creation"),TEXT("Transient owned listener creation failed"));TWeakPtr<FWork> Weak=CurrentWork;L->Callback=[Weak,Callback=MoveTemp(Callback)](const FChaosBreakEvent&E){auto W=Weak.Pin();if(!W||!Safe())return;FExecutionScope Pins(W);if(Pins.Valid&&W->Guard())Callback(E);};CurrentWork->SDK(TEXT("attach_owned_break_listener"),true);C->OnChaosBreakEvent.AddUniqueDynamic(L,&UCubelessChaosPerformanceBreakListener::HandleBreak);return AfterSDK();
}
void UnregisterOwnedBreak(UGeometryCollectionComponent*C)
{
 if(!CurrentWork)return;
 for(int32 I=CurrentWork->Subscriptions.Num()-1;I>=0;--I)
 {
  auto&Sub=CurrentWork->Subscriptions[I];if(C&&Sub.Component.Get()!=C)continue;
  auto*L=Sub.Listener.Get();if(L){L->bAcceptEvents=false;L->Callback=nullptr;}
  if(Safe()&&Live(Sub.Component.Get())&&L&&CurrentWork->Guard())Sub.Component->OnChaosBreakEvent.RemoveDynamic(L,&UCubelessChaosPerformanceBreakListener::HandleBreak);
  else CurrentWork->R.Data->SetBoolField(TEXT("listener_detach_unsafe_skipped"),true);
  CurrentWork->Subscriptions.RemoveAt(I);
 }
 for(auto It=CurrentWork->NotifyTouched.CreateIterator();It;++It)
 {
  auto*Target=It->Get();if(C&&Target!=C)continue;
  if(!Safe()||!Live(Target)||!CurrentWork->Guard()){CurrentWork->R.Data->SetBoolField(TEXT("notify_restore_unresolved"),true);continue;}
  const bool Original=CurrentWork->NotifyBefore.FindRef(TWeakObjectPtr<const UGeometryCollectionComponent>(Target));
  if(Target->bNotifyBreaks==Original){It.RemoveCurrent();continue;}
  if(!BeforeSDK(TEXT("restore_owned_notify_breaks")))continue;
  Target->SetNotifyBreaks(Original);
  if(!AfterSDK())continue;
  if(Target->bNotifyBreaks==Original)It.RemoveCurrent();else CurrentWork->R.Data->SetBoolField(TEXT("notify_restore_readback_failed"),true);
 }
}
void RuntimeValueReturned(const TCHAR*Stage,int32 Index,float Before,float After)
{
 if(!CurrentWork)return;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetNumberField(TEXT("transform_index"),Index);J->SetBoolField(TEXT("before_finite"),FMath::IsFinite(Before));J->SetBoolField(TEXT("after_finite"),FMath::IsFinite(After));if(FMath::IsFinite(Before))J->SetNumberField(TEXT("before"),Before);if(FMath::IsFinite(After))J->SetNumberField(TEXT("after"),After);const bool Changed=Before!=After;J->SetBoolField(TEXT("changed"),Changed);if(CurrentWork->Prefix.Num()<65536)CurrentWork->Prefix.Add(MakeShared<FJsonValueObject>(J));if(Changed){++CurrentWork->Changed;CurrentWork->R.bChanged=true;}
}
TArray<Chaos::FPhysicsObjectHandle> CheckedPhysicsObjects(UGeometryCollectionComponent*C)
{
 if(!Check()||!CurrentWork->Track(C))return {};auto Objects=C->GetAllPhysicsObjects();if(!AfterSDK())return {};if(Objects.Num()>1000001){CurrentWork->Fail(TEXT("physics_object_budget"),TEXT("Physics object observation budget exceeded"));return {};}TSet<Chaos::FPhysicsObjectHandle>Seen;for(auto O:Objects)if(O){if(Seen.Contains(O)){CurrentWork->Fail(TEXT("duplicate_physics_object"),TEXT("Physics object handles are not unique"));return {};}Seen.Add(O);}return Objects;
}
bool ValidateNumericInputs(const TSharedPtr<FJsonObject>&,const FString&);
// Original operation-specific algorithms below. No command dispatcher or transport is embedded.
TArray<TSharedPtr<FJsonValue>> VectorToJson(const FVector&);
TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>&);
TArray<int32> ResolveTraceItemTransformIndices(UGeometryCollectionComponent*,int32);
void CaptureTraceItemDispatchSnapshot(UWorld*,UGeometryCollectionComponent*,int32,const FVector&);
int32 ApplyDirectVelocityToEnabledTransforms(UGeometryCollectionComponent*,const TArray<int32>&,const FVector&);
void StartDeferredTraceVelocityDispatch(UGeometryCollectionComponent*,int32);
void StartOneWayCollisionCapture(UWorld*,UGeometryCollectionComponent*,UStaticMeshComponent*,UStaticMeshComponent*,int32,const FVector&,double);

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
    return Path == LearningRoot || Path.StartsWith(FString(LearningRoot) + TEXT("/"));
}

FString QuantizedVertexKey(const FVector& Point)
{
    return FString::Printf(TEXT("%lld,%lld,%lld"),
        FMath::RoundToInt64(Point.X * 1000.0),
        FMath::RoundToInt64(Point.Y * 1000.0),
        FMath::RoundToInt64(Point.Z * 1000.0));
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
    const double Value=Params.IsValid()&&Params->HasField(Name)?Params->GetNumberField(Name):DefaultValue;
    if(!FMath::IsFinite(Value)&&CurrentWork)CurrentWork->Fail(TEXT("nonfinite_number"),FString::Printf(TEXT("%s is not finite"),Name));
    return FMath::IsFinite(Value)?Value:0.0;
}

bool TryGetVectorParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* Name, FVector& OutValue)
{
    const TArray<TSharedPtr<FJsonValue>>*Values=nullptr;
    if(!Params||!Params->TryGetArrayField(Name,Values)||!Values||Values->Num()!=3)return false;
    double V[3];for(int32 I=0;I<3;++I){V[I]=(*Values)[I]->AsNumber();if(!FMath::IsFinite(V[I])){if(CurrentWork)CurrentWork->Fail(TEXT("nonfinite_vector"),FString::Printf(TEXT("%s vector is not finite"),Name));return false;}}
    OutValue=FVector(V[0],V[1],V[2]);return true;
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
    if(!Check()) return {};

    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<T> It(World); It; ++It)
    {
        T* Actor = *It;
        if (NameOrLabel.IsEmpty() || Actor->GetName() == NameOrLabel || Actor->GetActorLabel() == NameOrLabel || Actor->GetPathName() == NameOrLabel)
        {
            if(CurrentWork&&!CurrentWork->Track(Actor))return nullptr;
            return Actor;
        }
    }
    return nullptr;
}

TSharedPtr<FJsonObject> InspectCollection(const UGeometryCollection* Asset, UGeometryCollectionComponent* Component, double TinyRelativeThreshold)
{
    if(!Check()) return {};

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("asset_path"), Asset ? Asset->GetPathName() : FString());
    Data->SetBoolField(TEXT("asset_found"), Asset != nullptr);
    if (!Asset || !Asset->GetGeometryCollection().IsValid())
    {
        return Data;
    }

    const FGeometryCollection& Collection = *Asset->GetGeometryCollection();
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
    if (Component)
    {
        Data->SetStringField(TEXT("component_path"), Component->GetPathName());
        Data->SetNumberField(TEXT("one_way_interaction_level"), Component->OneWayInteractionLevel);
        Data->SetBoolField(TEXT("allow_removal_on_sleep"), Component->bAllowRemovalOnSleep);
        Data->SetBoolField(TEXT("allow_removal_on_break"), Component->bAllowRemovalOnBreak);
        Data->SetNumberField(TEXT("max_simulated_level"), Component->MaxSimulatedLevel);
        if (FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection())
        {
            int32 ActiveCount = 0;
            int32 SleepingCount = 0;
            int32 DynamicCount = 0;
            int32 BrokenCount = 0;
            FGeometryCollectionDynamicStateFacade StateFacade(*Dynamic);
            for (int32 Index = 0; Index < Dynamic->Active.Num(); ++Index)
            {
                ActiveCount += StateFacade.IsActive(Index) ? 1 : 0;
                SleepingCount += StateFacade.IsSleeping(Index) ? 1 : 0;
                DynamicCount += StateFacade.IsDynamicOrSleeping(Index) ? 1 : 0;
                BrokenCount += StateFacade.HasBrokenOff(Index) ? 1 : 0;
            }
            const TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects = CheckedPhysicsObjects(Component);
            FLockedReadPhysicsObjectExternalInterface PhysicsObjectInterface =
                FPhysicsObjectExternalInterface::LockRead(PhysicsObjects);
            int32 DisabledPhysicsObjectCount = 0;
            for (Chaos::FPhysicsObjectHandle PhysicsObject : PhysicsObjects)
            {
                DisabledPhysicsObjectCount += PhysicsObjectInterface->AreAllDisabled({ &PhysicsObject, 1 }) ? 1 : 0;
            }
            TArray<bool> HiddenTransforms;
            Component->GetHiddenTransforms(HiddenTransforms);
            int32 HiddenCount = 0;
            for (bool bHidden : HiddenTransforms)
            {
                HiddenCount += bHidden ? 1 : 0;
            }
            Data->SetBoolField(TEXT("dynamic_collection_available"), true);
            Data->SetNumberField(TEXT("dynamic_transform_count"), Dynamic->Active.Num());
            Data->SetNumberField(TEXT("active_transform_count"), ActiveCount);
            Data->SetNumberField(TEXT("sleeping_transform_count"), SleepingCount);
            Data->SetNumberField(TEXT("dynamic_or_sleeping_transform_count"), DynamicCount);
            Data->SetNumberField(TEXT("broken_transform_count"), BrokenCount);
            Data->SetBoolField(TEXT("physics_object_evidence_available"), !PhysicsObjects.IsEmpty());
            Data->SetNumberField(TEXT("physics_object_count"), PhysicsObjects.Num());
            Data->SetNumberField(TEXT("disabled_physics_object_count"), DisabledPhysicsObjectCount);
            Data->SetNumberField(TEXT("hidden_transform_count"), HiddenCount);
            Data->SetNumberField(TEXT("remaining_visible_rigid_count"), FMath::Max(0, RigidCount - HiddenCount));
        }
        else
        {
            Data->SetBoolField(TEXT("dynamic_collection_available"), false);
        }
        Data->SetBoolField(TEXT("render_state_created"), Component->IsRenderStateCreated());
        Data->SetBoolField(TEXT("component_visible"), Component->IsVisible());
        Data->SetBoolField(TEXT("owner_has_learning_tag"), Component->GetOwner() && Component->GetOwner()->Tags.Contains(LearningActorTag));
    }
    return Data;
}

UGeometryCollectionComponent* FindGeometryCollectionComponent(UWorld* World, const FString& ActorName)
{
    if(!Check()) return {};

    AActor* Actor = FindActor<AActor>(World, ActorName);
    auto*C=Actor?Actor->FindComponentByClass<UGeometryCollectionComponent>():nullptr;
    if(C&&CurrentWork&&!CurrentWork->Track(C))return nullptr;return C;
}

struct FTargetTraceDiagnostics
{
    int32 BlockingHitCount = 0;
    int32 TargetComponentHitCount = 0;
    int32 ValidItemHitCount = 0;
    bool bUsedDirectComponentTrace = false;
    TArray<FString> Candidates;
};

void AddComponentQueryDiagnostics(const TSharedPtr<FJsonObject>& Data, UGeometryCollectionComponent* Component)
{
    if(!Check()) return;

    Data->SetBoolField(TEXT("query_component_found"), Component != nullptr);
    if (!Component)
    {
        return;
    }
    Data->SetArrayField(TEXT("query_bounds_origin"), VectorToJson(Component->Bounds.Origin));
    Data->SetArrayField(TEXT("query_bounds_extent"), VectorToJson(Component->Bounds.BoxExtent));
    Data->SetBoolField(TEXT("query_owner_collision_enabled"), Component->GetOwner() && Component->GetOwner()->GetActorEnableCollision());
    Data->SetBoolField(TEXT("query_component_registered"), Component->IsRegistered());
    Data->SetBoolField(TEXT("query_physics_state_created"), Component->IsPhysicsStateCreated());
    Data->SetStringField(TEXT("query_collision_profile"), Component->GetCollisionProfileName().ToString());
    Data->SetNumberField(TEXT("query_collision_enabled"), static_cast<int32>(Component->GetCollisionEnabled()));
    Data->SetNumberField(TEXT("query_object_type"), static_cast<int32>(Component->GetCollisionObjectType()));
    Data->SetNumberField(TEXT("query_visibility_response"), static_cast<int32>(Component->GetCollisionResponseToChannel(ECC_Visibility)));
    Data->SetBoolField(TEXT("query_simulates_physics"), Component->IsSimulatingPhysics());
    Data->SetBoolField(TEXT("query_dynamic_collection_available"), Component->GetDynamicCollection() != nullptr);
    Data->SetStringField(TEXT("query_rest_collection_path"), Component->GetRestCollection() ? Component->GetRestCollection()->GetPathName() : FString());

    const TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects = CheckedPhysicsObjects(Component);
    Data->SetNumberField(TEXT("query_physics_object_count"), PhysicsObjects.Num());
    if (!PhysicsObjects.IsEmpty())
    {
        FLockedReadPhysicsObjectExternalInterface PhysicsObjectInterface =
            FPhysicsObjectExternalInterface::LockRead(PhysicsObjects);
        Data->SetBoolField(TEXT("query_physics_objects_valid"), PhysicsObjectInterface->AreAllValid(PhysicsObjects));
        Data->SetBoolField(TEXT("query_shapes_enabled"), PhysicsObjectInterface->AreAllShapesQueryEnabled(PhysicsObjects));
        Data->SetNumberField(TEXT("query_shape_count"), PhysicsObjectInterface->GetAllThreadShapes(PhysicsObjects).Num());
        const FBox PhysicsBounds = PhysicsObjectInterface->GetWorldBounds(PhysicsObjects);
        Data->SetArrayField(TEXT("query_physics_bounds_min"), VectorToJson(PhysicsBounds.IsValid ? PhysicsBounds.Min : FVector::ZeroVector));
        Data->SetArrayField(TEXT("query_physics_bounds_max"), VectorToJson(PhysicsBounds.IsValid ? PhysicsBounds.Max : FVector::ZeroVector));
    }

    if (const UGeometryCollection* RestCollection = Component->GetRestCollection())
    {
        Data->SetNumberField(TEXT("query_size_specific_count"), RestCollection->SizeSpecificData.Num());
        const FGeometryCollectionSizeSpecificData& DefaultCollision = RestCollection->GetDefaultSizeSpecificData();
        Data->SetNumberField(TEXT("query_collision_shape_config_count"), DefaultCollision.CollisionShapes.Num());
        if (!DefaultCollision.CollisionShapes.IsEmpty())
        {
            Data->SetNumberField(TEXT("query_collision_type"), static_cast<int32>(DefaultCollision.CollisionShapes[0].CollisionType));
            Data->SetNumberField(TEXT("query_implicit_type"), static_cast<int32>(DefaultCollision.CollisionShapes[0].ImplicitType));
        }
    }
}

int32 ResolveActiveClusterItemIndex(UGeometryCollectionComponent* Component)
{
    if(!Check()) return {};

    if (!Component || !Component->GetRestCollection() || !Component->GetDynamicCollection())
    {
        return INDEX_NONE;
    }
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest = Component->GetRestCollection()->GetGeometryCollection();
    FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection();
    if (!Rest.IsValid())
    {
        return INDEX_NONE;
    }
    FGeometryCollectionDynamicStateFacade State(*Dynamic);
    for (int32 Index = 0; Index < Rest->Parent.Num(); ++Index)
    {
        if (Rest->Parent[Index] == INDEX_NONE &&
            Rest->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Clustered &&
            Dynamic->Active.IsValidIndex(Index) && State.IsActive(Index))
        {
            return Index;
        }
    }
    for (int32 Index = 0; Index < Rest->Parent.Num(); ++Index)
    {
        if (Rest->Parent[Index] == INDEX_NONE && Rest->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Clustered)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

int32 ResolveLargestInternalClusterParentItem(
    UGeometryCollectionComponent* Component,
    int32& OutChildCount,
    TArray<FString>& OutDiagnostics)
{
    if(!Check()) return {};

    OutChildCount = 0;
    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    const UGeometryCollection* RestCollection = Component ? Component->GetRestCollection() : nullptr;
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest =
        RestCollection ? RestCollection->GetGeometryCollection() : nullptr;
    if (!Proxy || !Rest.IsValid())
    {
        return INDEX_NONE;
    }
    TSet<int32> CandidateItems;
    for (int32 TransformIndex = 0; TransformIndex < Rest->SimulationType.Num(); ++TransformIndex)
    {
        if (Rest->SimulationType[TransformIndex] != FGeometryCollection::ESimulationTypes::FST_Rigid)
        {
            continue;
        }
        const FGeometryCollectionItemIndex ParentItem =
            Proxy->GetInternalClusterParentItemIndex_External(TransformIndex);
        if (ParentItem.IsValid() && ParentItem.IsInternalCluster())
        {
            CandidateItems.Add(ParentItem.GetItemIndex());
        }
    }
    int32 BestItem = INDEX_NONE;
    for (int32 CandidateItem : CandidateItems)
    {
        const FGeometryCollectionItemIndex Decoded =
            FGeometryCollectionItemIndex::CreateFromExistingItemIndex(CandidateItem);
        const TArray<int32>* Children = Proxy->FindInternalClusterChildrenTransformIndices_External(Decoded);
        const int32 ChildCount = Children ? Children->Num() : 0;
        OutDiagnostics.Add(FString::Printf(
            TEXT("actor=%s proxy_parent_item=%d child_count=%d"),
            Component->GetOwner() ? *Component->GetOwner()->GetName() : TEXT("none"),
            CandidateItem,
            ChildCount));
        if (ChildCount > OutChildCount)
        {
            BestItem = CandidateItem;
            OutChildCount = ChildCount;
        }
    }
    if (BestItem == INDEX_NONE)
    {
        const int32 ActiveRootTransform = ResolveActiveClusterItemIndex(Component);
        if (Rest->SimulationType.IsValidIndex(ActiveRootTransform) &&
            Rest->SimulationType[ActiveRootTransform] == FGeometryCollection::ESimulationTypes::FST_Clustered &&
            Rest->Children.IsValidIndex(ActiveRootTransform))
        {
            BestItem = ActiveRootTransform;
            OutChildCount = Rest->Children[ActiveRootTransform].Num();
            OutDiagnostics.Add(FString::Printf(
                TEXT("actor=%s proxy_parent_unavailable_fallback_active_root_transform=%d rest_child_count=%d"),
                Component->GetOwner() ? *Component->GetOwner()->GetName() : TEXT("none"),
                BestItem,
                OutChildCount));
        }
    }
    return BestItem;
}

bool TraceTargetGeometryCollection(
    UWorld* World,
    UGeometryCollectionComponent* Target,
    const FVector& Start,
    const FVector& End,
    bool bTraceComplex,
    const TArray<AActor*>& IgnoredActors,
    FHitResult& OutHit,
    FTargetTraceDiagnostics& OutDiagnostics)
{
    if(!Check()) return {};

    if (!World || !Target)
    {
        return false;
    }
    FCollisionQueryParams QueryParams(TEXT("UnrealMCPChaosTargetTrace"), bTraceComplex);
    QueryParams.bReturnFaceIndex = true;
    for (AActor* IgnoredActor : IgnoredActors)
    {
        QueryParams.AddIgnoredActor(IgnoredActor);
    }
    TArray<FHitResult> Hits;
    if(!BeforeSDK(TEXT("trace_world"),false))return false;
    World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, QueryParams);
    if(!AfterSDK())return false;
    for (const FHitResult& Hit : Hits)
    {
        OutDiagnostics.BlockingHitCount += Hit.bBlockingHit ? 1 : 0;
        const bool bTarget = Hit.GetComponent() == Target;
        OutDiagnostics.TargetComponentHitCount += bTarget ? 1 : 0;
        OutDiagnostics.ValidItemHitCount += bTarget && Hit.Item != INDEX_NONE ? 1 : 0;
        OutDiagnostics.Candidates.Add(FString::Printf(
            TEXT("component=%s actor=%s item=%d blocking=%s distance=%.3f"),
            Hit.GetComponent() ? *Hit.GetComponent()->GetPathName() : TEXT("none"),
            Hit.GetActor() ? *Hit.GetActor()->GetPathName() : TEXT("none"),
            Hit.Item,
            Hit.bBlockingHit ? TEXT("true") : TEXT("false"),
            Hit.Distance));
        if (bTarget && Hit.Item != INDEX_NONE)
        {
            OutHit = Hit;
            return true;
        }
    }

    FHitResult DirectHit;
    if(!BeforeSDK(TEXT("trace_target_component"),false))return false;
    const bool DirectHitResult=Target->LineTraceComponent(DirectHit, Start, End, QueryParams);
    if(!AfterSDK())return false;
    if (DirectHitResult && DirectHit.Item != INDEX_NONE)
    {
        OutDiagnostics.bUsedDirectComponentTrace = true;
        OutDiagnostics.TargetComponentHitCount += 1;
        OutDiagnostics.ValidItemHitCount += 1;
        OutDiagnostics.Candidates.Add(FString::Printf(
            TEXT("direct_component_trace item=%d blocking=%s distance=%.3f"),
            DirectHit.Item,
            DirectHit.bBlockingHit ? TEXT("true") : TEXT("false"),
            DirectHit.Distance));
        OutHit = DirectHit;
        return true;
    }
    return false;
}

void AddTraceDiagnostics(const TSharedPtr<FJsonObject>& Data, const FTargetTraceDiagnostics& Diagnostics)
{
    Data->SetNumberField(TEXT("candidate_blocking_hit_count"), Diagnostics.BlockingHitCount);
    Data->SetNumberField(TEXT("candidate_target_component_hit_count"), Diagnostics.TargetComponentHitCount);
    Data->SetNumberField(TEXT("candidate_valid_item_hit_count"), Diagnostics.ValidItemHitCount);
    Data->SetBoolField(TEXT("used_direct_component_trace"), Diagnostics.bUsedDirectComponentTrace);
    Data->SetArrayField(TEXT("trace_candidates"), StringsToJson(Diagnostics.Candidates));
}

void ApplySolverPropertyChange(AChaosSolverActor* SolverActor)
{
    if(!Check()) return;

#if WITH_EDITOR
    if (FProperty* Property = FindFProperty<FProperty>(AChaosSolverActor::StaticClass(), GET_MEMBER_NAME_CHECKED(AChaosSolverActor, Properties)))
    {
        FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
        Event.MemberProperty = Property;
        if(!BeforeObjectSDK(SolverActor,TEXT("PostEditChangeProperty"))) return;
        SolverActor->PostEditChangeProperty(Event);
        if(!AfterSDK()) return;
    }
#endif
}

bool SameDestructionSettings(const FChaosSolverDestructionSettings& A, const FChaosSolverDestructionSettings& B);

struct FThrottleLearningSession
{
    TSharedPtr<FWork> Evidence;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AChaosSolverActor> SolverActor;
    FChaosSolverConfiguration OriginalConfiguration;
    FChaosSolverDestructionSettings AppliedSettings;
    FString Scenario;
    bool bExpectBreakThrottling = false;
    bool bExpectParticleReleaseThrottling = false;
    TMap<FString, TSet<int32>> PreviousBrokenSets;
    TMap<FString, int32> InitialRootTransformIndices;
    TSet<FString> PreviousTransitionedRoots;
    TArray<TWeakObjectPtr<UGeometryCollectionComponent>> RegisteredBreakComponents;
    int32 RegisteredBreakComponentCount = 0;
    TWeakObjectPtr<UChaosGameplayEventDispatcher> BreakEventDispatcher;
    TSet<FString> ComponentsWithBreakEvent;
    TMap<uint64, int32> FirstComponentBreakEventsPerFrame;
    TMap<uint64, int32> AllBreakEventsPerFrame;
    int32 TotalGameplayBreakEvents = 0;
    FTSTicker::FDelegateHandle TickerHandle;
    uint64 LastFrame = 0;
    int32 DispatchCount = 0;
    TArray<TWeakObjectPtr<UGeometryCollectionComponent>> DispatchComponents;
    TArray<int32> DispatchItemIndices;
    float DispatchStrain = 1000000000000.0f;
    int32 ReapplyDispatchCount = 0;
    int32 CaptureLimitFrames = 32;
    int32 CapturedFrameCount = 0;
    int32 ClusterReleaseTransitionCount = 0;
    int32 ParticleReleaseTransitionCount = 0;
    int32 TotalNewClusterBreaks = 0;
    int32 TotalNewReleasedParticles = 0;
    int32 MaximumNewClusterBreaksPerFrame = 0;
    int32 MaximumNewReleasedParticlesPerFrame = 0;
    int32 PreviousActiveCount = 0;
    int32 PreviousDisabledPhysicsObjectCount = 0;
    int32 InitialActiveCount = 0;
    int32 InitialDisabledPhysicsObjectCount = 0;
    bool bPerFrameLimitViolated = false;
    bool bAllCapturedFrameDeltasExact = true;
    bool bCaptureCompleted = false;
    bool bSettingsRestored = false;
    bool bOwnsTemporarySettings=false;
    FString CompletionReason;
    TArray<TSharedPtr<FJsonValue>> Samples;
};

TUniquePtr<FThrottleLearningSession> ActiveThrottleSession;

struct FTraceItemDispatchSnapshot
{
    TSharedPtr<FWork> Evidence;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UGeometryCollectionComponent> Component;
    int32 ItemIndex = INDEX_NONE;
    int32 BreakParentItemIndex = INDEX_NONE;
    int32 BreakParentChildCount = 0;
    uint64 DispatchFrame = 0;
    TArray<int32> TransformIndices;
    FVector RequestedVelocity = FVector::ZeroVector;
    int32 DirectVelocityAppliedCount = 0;
    int32 DeferredVelocityDispatchAttemptCount = 0;
    int32 DeferredVelocityTickCount = 0;
    uint64 DeferredVelocityDispatchFrame = 0;
    FVector DeferredObservedVelocity = FVector::ZeroVector;
    bool bDeferredVelocityObserved = false;
    bool bDeferredVelocityDispatchComplete = false;
    FTSTicker::FDelegateHandle DeferredVelocityTickerHandle;
};

TMap<FString, FTraceItemDispatchSnapshot> TraceItemDispatchSnapshots;

struct FOneWayPTContactDiagnostics
{
    TAtomic<int32> SampleCount{0};
    TAtomic<int32> SameSolver{-1};
    TAtomic<int32> TargetObjectState{-1};
    TAtomic<int32> WitnessObjectState{-1};
    TAtomic<int32> TargetCollisionGroup{MAX_int32};
    TAtomic<int32> WitnessCollisionGroup{MAX_int32};
    TAtomic<int32> TargetShapeCount{0};
    TAtomic<int32> TargetSimShapeCount{0};
    TAtomic<int32> WitnessShapeCount{0};
    TAtomic<int32> WitnessSimShapeCount{0};
    TAtomic<int32> PairConstraintCount{0};
    TAtomic<int32> CurrentPairConstraintCount{0};
    TAtomic<int32> EnabledPairConstraintCount{0};
    TAtomic<int32> OneWayPairConstraintCount{0};
    TAtomic<int32> PairManifoldPointCount{0};
    TAtomic<int64> TargetMassMicroKg{-1};
    TAtomic<int64> WitnessMassMicroKg{-1};
    TAtomic<int64> WitnessPoseErrorMicrometers{-1};
    TAtomic<int64> MinimumPairPhiMicrometers{MAX_int64};
    TAtomic<int64> MaximumPairImpulseMicroNs{0};
};

struct FOneWayCollisionCaptureSession
{
    TSharedPtr<FWork> Evidence;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UGeometryCollectionComponent> Component;
    TWeakObjectPtr<UStaticMeshComponent> Witness;
    TWeakObjectPtr<UStaticMeshComponent> Floor;
    int32 TargetTransformIndex = INDEX_NONE;
    TArray<int32> DebrisTransformIndices;
    FVector Direction = FVector::ZeroVector;
    FVector InitialWitnessLocation = FVector::ZeroVector;
    FVector InitialWitnessVelocity = FVector::ZeroVector;
    FVector InitialDebrisLocation = FVector::ZeroVector;
    FVector LatestDebrisLocation = FVector::ZeroVector;
    FVector LatestDebrisVelocity = FVector::ZeroVector;
    double MaximumProjectedWitnessDisplacement = 0.0;
    double MaximumProjectedWitnessVelocity = 0.0;
    double MaximumProjectedDebrisTravel = 0.0;
    double InitialDebrisWitnessSeparation = 0.0;
    double FirstSampleDebrisWitnessSeparation = -1.0;
    double PreviousDebrisWitnessSeparation = 0.0;
    double InitialDebrisVelocityDirectionDot = -2.0;
    double MinimumDebrisWitnessSeparation = TNumericLimits<double>::Max();
    int32 ClosingFrameCount = 0;
    int32 ApproachFrameCount = 0;
    int32 ContactProximityFrameCount = 0;
    int32 DebrisFloorProximityFrameCount = 0;
    int32 CapturedFrameCount = 0;
    uint64 LastFrame = 0;
    bool bCaptureCompleted = false;
    TSharedPtr<FOneWayPTContactDiagnostics, ESPMode::ThreadSafe> PTDiagnostics;
    FTSTicker::FDelegateHandle TickerHandle;
};

TUniquePtr<FOneWayCollisionCaptureSession> ActiveOneWayCollisionCapture;

struct FOneWayStagedLaunchSession
{
    TSharedPtr<FWork> Evidence;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UGeometryCollectionComponent> Component;
    TWeakObjectPtr<UStaticMeshComponent> Witness;
    TWeakObjectPtr<UStaticMeshComponent> Floor;
    TArray<int32> CandidateRigidIndices;
    FVector RequestedVelocity = FVector::ZeroVector;
    int32 InitialBreakItem = INDEX_NONE;
    int32 TargetRigidIndex = INDEX_NONE;
    int32 TargetParentItem = INDEX_NONE;
    int32 TickCount = 0;
    int32 ParentReleaseDispatchCount = 0;
    uint64 BeginFrame = 0;
    uint64 LaunchFrame = 0;
    FVector AlignedWitnessLocation = FVector::ZeroVector;
    FVector AlignedWitnessVelocity = FVector::ZeroVector;
    FVector InitialTargetLocation = FVector::ZeroVector;
    double InitialWitnessSeparation = 0.0;
    double CalibrationWitnessGap = 0.0;
    double StagedHorizontalFlightGap = 0.0;
    double PredictedFlightTimeSeconds = 0.0;
    double StagedGravityZ = 0.0;
    double LaunchDirectionDot = 0.0;
    FVector LaunchTargetToWitnessDirection = FVector::ZeroVector;
    TArray<int32> CollisionIsolatedSiblingIndices;
    int32 TargetCollisionConfiguredCount = 0;
    uint64 TargetCollisionConfiguredFrame = 0;
    uint64 WitnessAlignmentFrame = 0;
    bool bWitnessAlignmentCompleted = false;
    FVector VelocityDispatchTargetLocation = FVector::ZeroVector;
    uint64 VelocityDispatchFrame = 0;
    double ObservedTargetMotionProjection = 0.0;
    int32 PTVelocityDispatchCount = 0;
    TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> PTVelocityAppliedCount;
    bool bVelocityDispatchCompleted = false;
    bool bTargetMotionObserved = false;
    uint64 ContactWitnessAlignmentFrame = 0;
    bool bContactWitnessAlignmentCompleted = false;
    uint64 ContactVelocityDispatchFrame = 0;
    int32 ContactPTVelocityDispatchCount = 0;
    TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> ContactPTVelocityAppliedCount;
    TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> ContactPTOneWayFlag;
    TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> ContactPTObjectState;
    TSharedPtr<FOneWayPTContactDiagnostics, ESPMode::ThreadSafe> PTContactDiagnostics;
    bool bSiblingCollisionRestored = false;
    bool bLaunchCompleted = false;
    FString Failure;
    FTSTicker::FDelegateHandle TickerHandle;
};

TUniquePtr<FOneWayStagedLaunchSession> ActiveOneWayStagedLaunch;

int32 SetOneWaySiblingSimulationCollision(FOneWayStagedLaunchSession& Session, bool bSimCollision)
{
    if(!Check()) return {};

    UGeometryCollectionComponent* Component = Session.Component.Get();
    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    if (!Component || !Proxy)
    {
        return 0;
    }
    if (!bSimCollision && Session.CollisionIsolatedSiblingIndices.IsEmpty())
    {
        const UGeometryCollection* RestCollection = Component->GetRestCollection();
        const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest =
            RestCollection ? RestCollection->GetGeometryCollection() : nullptr;
        if (Rest.IsValid())
        {
            for (int32 TransformIndex = 0; TransformIndex < Rest->Transform.Num(); ++TransformIndex)
            {
                if (TransformIndex != Session.TargetRigidIndex && Proxy->GetPhysicsObjectByIndex(TransformIndex))
                {
                    Session.CollisionIsolatedSiblingIndices.Add(TransformIndex);
                }
            }
        }
    }
    TArray<Chaos::FPhysicsObjectHandle> SiblingPhysicsObjects;
    for (int32 TransformIndex : Session.CollisionIsolatedSiblingIndices)
    {
        if (Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(TransformIndex))
        {
            SiblingPhysicsObjects.Add(PhysicsObject);
        }
    }
    if (!SiblingPhysicsObjects.IsEmpty())
    {
        FLockedWritePhysicsObjectExternalInterface WriteInterface =
            FPhysicsObjectExternalInterface::LockWrite(SiblingPhysicsObjects);
        if(!BeforeSDK(TEXT("UpdateShapeCollisionFlags"))) return {};
        WriteInterface->UpdateShapeCollisionFlags(SiblingPhysicsObjects, bSimCollision, true);
        if(!AfterSDK()) return {};
    }
    if (bSimCollision)
    {
        Session.bSiblingCollisionRestored = true;
    }
    return SiblingPhysicsObjects.Num();
}

void EnqueueOneWayPTContactDiagnostics(FOneWayStagedLaunchSession& Session)
{
    if(!Check()) return;

    UGeometryCollectionComponent* Component = Session.Component.Get();
    UStaticMeshComponent* Witness = Session.Witness.Get();
    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    FPhysicsActorHandle WitnessActor = Witness ? Witness->BodyInstance.GetPhysicsActor() : nullptr;
    Chaos::FPBDRigidsSolver* Solver = Proxy ? Proxy->GetSolver<Chaos::FPBDRigidsSolver>() : nullptr;
    if (!Proxy || !WitnessActor || !Solver || Session.TargetRigidIndex == INDEX_NONE)
    {
        return;
    }
    if (!Session.PTContactDiagnostics.IsValid())
    {
        Session.PTContactDiagnostics = MakeShared<FOneWayPTContactDiagnostics, ESPMode::ThreadSafe>();
    }
    const int32 TargetRigidIndex = Session.TargetRigidIndex;
    const FVector IntendedWitnessLocation = Session.AlignedWitnessLocation;
    const TSharedPtr<FOneWayPTContactDiagnostics, ESPMode::ThreadSafe> Diagnostics =
        Session.PTContactDiagnostics;
    EnqueueSafe(Solver, Proxy,
        [Proxy, WitnessActor, Solver, TargetRigidIndex, IntendedWitnessLocation, Diagnostics]()
        {
            FGeometryCollectionPhysicsProxy::FClusterHandle* TargetParticle =
                Proxy->GetParticle_Internal(TargetRigidIndex);
            Chaos::FGeometryParticleHandle* WitnessGeometryParticle =
                WitnessActor->GetHandle_LowLevel();
            Chaos::FPBDRigidParticleHandle* WitnessParticle = WitnessGeometryParticle
                ? WitnessGeometryParticle->CastToRigidParticle()
                : nullptr;
            if (!TargetParticle || !WitnessParticle)
            {
                return;
            }

            Diagnostics->SampleCount.IncrementExchange();
            Diagnostics->SameSolver.Store(
                WitnessActor->GetSolver<Chaos::FPBDRigidsSolver>() == Solver ? 1 : 0);
            Diagnostics->TargetObjectState.Store(static_cast<int32>(TargetParticle->ObjectState()));
            Diagnostics->WitnessObjectState.Store(static_cast<int32>(WitnessParticle->ObjectState()));
            Diagnostics->TargetCollisionGroup.Store(TargetParticle->CollisionGroup());
            Diagnostics->WitnessCollisionGroup.Store(WitnessParticle->CollisionGroup());
            Diagnostics->TargetMassMicroKg.Store(
                FMath::RoundToInt64(static_cast<double>(TargetParticle->M()) * 1000000.0));
            Diagnostics->WitnessMassMicroKg.Store(
                FMath::RoundToInt64(static_cast<double>(WitnessParticle->M()) * 1000000.0));
            const double WitnessPoseError = FVector::Distance(
                FVector(WitnessParticle->GetX()),
                IntendedWitnessLocation);
            const int64 WitnessPoseErrorMicrometers =
                FMath::RoundToInt64(WitnessPoseError * 10000.0);
            Diagnostics->WitnessPoseErrorMicrometers.Store(FMath::Max(
                Diagnostics->WitnessPoseErrorMicrometers.Load(),
                WitnessPoseErrorMicrometers));

            int32 TargetSimShapeCount = 0;
            for (const Chaos::FShapeInstancePtr& Shape : TargetParticle->ShapeInstances())
            {
                TargetSimShapeCount += Shape && Shape->GetSimEnabled() ? 1 : 0;
            }
            int32 WitnessSimShapeCount = 0;
            for (const Chaos::FShapeInstancePtr& Shape : WitnessParticle->ShapeInstances())
            {
                WitnessSimShapeCount += Shape && Shape->GetSimEnabled() ? 1 : 0;
            }
            Diagnostics->TargetShapeCount.Store(TargetParticle->ShapeInstances().Num());
            Diagnostics->TargetSimShapeCount.Store(TargetSimShapeCount);
            Diagnostics->WitnessShapeCount.Store(WitnessParticle->ShapeInstances().Num());
            Diagnostics->WitnessSimShapeCount.Store(WitnessSimShapeCount);

            int32 PairConstraintCount = 0;
            int32 CurrentPairConstraintCount = 0;
            int32 EnabledPairConstraintCount = 0;
            int32 OneWayPairConstraintCount = 0;
            int32 PairManifoldPointCount = 0;
            int64 MinimumPairPhiMicrometers = MAX_int64;
            int64 MaximumPairImpulseMicroNs = 0;
            TargetParticle->ParticleCollisions().VisitConstCollisions(
                [WitnessParticle, &PairConstraintCount, &CurrentPairConstraintCount,
                    &EnabledPairConstraintCount, &OneWayPairConstraintCount,
                    &PairManifoldPointCount, &MinimumPairPhiMicrometers,
                    &MaximumPairImpulseMicroNs](const Chaos::FPBDCollisionConstraint& Constraint)
                {
                    if (Constraint.GetParticle0() != WitnessParticle &&
                        Constraint.GetParticle1() != WitnessParticle)
                    {
                        return Chaos::ECollisionVisitorResult::Continue;
                    }
                    ++PairConstraintCount;
                    CurrentPairConstraintCount += Constraint.IsCurrent() ? 1 : 0;
                    EnabledPairConstraintCount += !Constraint.GetDisabled() ? 1 : 0;
                    OneWayPairConstraintCount += Constraint.GetIsOneWayInteraction() ? 1 : 0;
                    PairManifoldPointCount += Constraint.NumManifoldPoints();
                    if (FMath::IsFinite(static_cast<double>(Constraint.GetPhi())))
                    {
                        MinimumPairPhiMicrometers = FMath::Min(
                            MinimumPairPhiMicrometers,
                            FMath::RoundToInt64(static_cast<double>(Constraint.GetPhi()) * 10000.0));
                    }
                    for (int32 PointIndex = 0; PointIndex < Constraint.NumManifoldPoints(); ++PointIndex)
                    {
                        const Chaos::FManifoldPointResult& Result =
                            Constraint.GetManifoldPointResult(PointIndex);
                        if (Result.bIsValid)
                        {
                            MaximumPairImpulseMicroNs = FMath::Max(
                                MaximumPairImpulseMicroNs,
                                FMath::RoundToInt64(
                                    static_cast<double>(Result.NetImpulse.Size()) * 1000000.0));
                        }
                    }
                    return Chaos::ECollisionVisitorResult::Continue;
                },
                Chaos::ECollisionVisitorFlags::VisitAllCurrent);

            Diagnostics->PairConstraintCount.Store(FMath::Max(
                Diagnostics->PairConstraintCount.Load(), PairConstraintCount));
            Diagnostics->CurrentPairConstraintCount.Store(FMath::Max(
                Diagnostics->CurrentPairConstraintCount.Load(), CurrentPairConstraintCount));
            Diagnostics->EnabledPairConstraintCount.Store(FMath::Max(
                Diagnostics->EnabledPairConstraintCount.Load(), EnabledPairConstraintCount));
            Diagnostics->OneWayPairConstraintCount.Store(FMath::Max(
                Diagnostics->OneWayPairConstraintCount.Load(), OneWayPairConstraintCount));
            Diagnostics->PairManifoldPointCount.Store(FMath::Max(
                Diagnostics->PairManifoldPointCount.Load(), PairManifoldPointCount));
            if (MinimumPairPhiMicrometers != MAX_int64)
            {
                Diagnostics->MinimumPairPhiMicrometers.Store(FMath::Min(
                    Diagnostics->MinimumPairPhiMicrometers.Load(), MinimumPairPhiMicrometers));
            }
            Diagnostics->MaximumPairImpulseMicroNs.Store(FMath::Max(
                Diagnostics->MaximumPairImpulseMicroNs.Load(), MaximumPairImpulseMicroNs));
        });
}

void DispatchOneWayTargetVelocityOnPhysicsThread(
    FOneWayStagedLaunchSession& Session,
    FGeometryCollectionPhysicsProxy* Proxy,
    const FVector& Velocity,
    bool bContactLaunch)
{
    if(!Check()) return;

    if (!Proxy || Session.TargetRigidIndex == INDEX_NONE)
    {
        return;
    }
    Chaos::FPBDRigidsSolver* Solver = Proxy->GetSolver<Chaos::FPBDRigidsSolver>();
    if (!Solver)
    {
        return;
    }
    TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe>& SessionAppliedCounter = bContactLaunch
        ? Session.ContactPTVelocityAppliedCount
        : Session.PTVelocityAppliedCount;
    if (!SessionAppliedCounter.IsValid())
    {
        SessionAppliedCounter = MakeShared<TAtomic<int32>, ESPMode::ThreadSafe>(0);
    }
    const int32 TargetRigidIndex = Session.TargetRigidIndex;
    const TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> AppliedCounter =
        SessionAppliedCounter;
    if (bContactLaunch && !Session.ContactPTOneWayFlag.IsValid())
    {
        Session.ContactPTOneWayFlag = MakeShared<TAtomic<int32>, ESPMode::ThreadSafe>(-1);
        Session.ContactPTObjectState = MakeShared<TAtomic<int32>, ESPMode::ThreadSafe>(-1);
    }
    const TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> OneWayCounter =
        bContactLaunch ? Session.ContactPTOneWayFlag : nullptr;
    const TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> ObjectStateCounter =
        bContactLaunch ? Session.ContactPTObjectState : nullptr;
    const FPhysicsActorHandle WitnessActor = bContactLaunch && Session.Witness.IsValid()
        ? Session.Witness->BodyInstance.GetPhysicsActor()
        : nullptr;
    if (bContactLaunch)
    {
        ++Session.ContactPTVelocityDispatchCount;
    }
    else
    {
        ++Session.PTVelocityDispatchCount;
    }
    EnqueueSafe(Solver, Proxy,
        [Proxy, Solver, TargetRigidIndex, Velocity, AppliedCounter, OneWayCounter,
            ObjectStateCounter, WitnessActor]()
        {
            if (WitnessActor && WitnessActor->GetSolver<Chaos::FPBDRigidsSolver>() == Solver)
            {
                Chaos::FGeometryParticleHandle* WitnessGeometryParticle =
                    WitnessActor->GetHandle_LowLevel();
                if (Chaos::FPBDRigidParticleHandle* WitnessParticle = WitnessGeometryParticle
                    ? WitnessGeometryParticle->CastToRigidParticle()
                    : nullptr)
                {
                    if (!WitnessParticle->Disabled())
                    {
                        Solver->GetEvolution()->SetParticleObjectState(
                            WitnessParticle,
                            Chaos::EObjectStateType::Dynamic);
                        WitnessParticle->SetV(FVector::ZeroVector);
                        WitnessParticle->SetW(FVector::ZeroVector);
                        if (WitnessParticle->CollisionGroup() == INDEX_NONE)
                        {
                            WitnessParticle->SetCollisionGroup(0);
                        }
                    }
                }
            }
            if (FGeometryCollectionPhysicsProxy::FClusterHandle* Particle =
                Proxy->GetParticle_Internal(TargetRigidIndex))
            {
                if (!Particle->Disabled())
                {
                    Solver->GetEvolution()->SetParticleObjectState(
                        Particle,
                        Chaos::EObjectStateType::Dynamic);
                    Particle->SetV(Velocity);
                    Particle->SetW(FVector::ZeroVector);
                    if (Particle->CollisionGroup() == INDEX_NONE)
                    {
                        Particle->SetCollisionGroup(0);
                    }
                    if (OneWayCounter.IsValid())
                    {
                        OneWayCounter->Store(Particle->OneWayInteraction() ? 1 : 0);
                    }
                    if (ObjectStateCounter.IsValid())
                    {
                        ObjectStateCounter->Store(static_cast<int32>(Particle->ObjectState()));
                    }
                    AppliedCounter->IncrementExchange();
                }
            }
        });
}

void ClearOneWayStagedLaunch()
{
    TGuardValue<bool> RestoreAdmission(Restoring,true);

    if(!Check()) return;

    if (ActiveOneWayStagedLaunch)
    {
        SetOneWaySiblingSimulationCollision(*ActiveOneWayStagedLaunch, true);
    }
    if (ActiveOneWayStagedLaunch && ActiveOneWayStagedLaunch->TickerHandle.IsValid())
    {
        FTSTicker::RemoveTicker(ActiveOneWayStagedLaunch->TickerHandle);
    }
    if(!ActiveOneWayStagedLaunch||ActiveOneWayStagedLaunch->CollisionIsolatedSiblingIndices.IsEmpty()||ActiveOneWayStagedLaunch->bSiblingCollisionRestored)ActiveOneWayStagedLaunch.Reset();
    else{ActiveOneWayStagedLaunch->bLaunchCompleted=true;CurrentWork->R.Data->SetBoolField(TEXT("unresolved_one_way_restore_lease"),true);}
}

bool AdvanceOneWayStagedLaunch()
{
    if(!Check()) return {};

    if (!ActiveOneWayStagedLaunch || ActiveOneWayStagedLaunch->bLaunchCompleted)
    {
        return false;
    }
    FOneWayStagedLaunchSession& Session = *ActiveOneWayStagedLaunch;
    UWorld* World = Session.World.Get();
    UGeometryCollectionComponent* Component = Session.Component.Get();
    UStaticMeshComponent* Witness = Session.Witness.Get();
    UStaticMeshComponent* Floor = Session.Floor.Get();
    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    if (!World || !Component || !Witness || !Floor || !Proxy)
    {
        Session.Failure = TEXT("world_or_component_lost");
        Session.TickerHandle.Reset();
        return false;
    }
    ++Session.TickCount;
    if (Session.TargetRigidIndex == INDEX_NONE)
    {
        double BestScore = TNumericLimits<double>::Max();
        for (int32 CandidateIndex : Session.CandidateRigidIndices)
        {
            if (const FGeometryCollectionPhysicsProxy::FParticle* Particle =
                Proxy->GetParticleByIndex_External(CandidateIndex))
            {
                const FVector Delta = Particle->X() - Witness->GetComponentLocation();
                const double Score = FMath::Abs(Delta.Y) * 10.0 + FMath::Abs(Delta.Z) +
                    FMath::Abs(Delta.X) * 0.01;
                if (Score < BestScore)
                {
                    BestScore = Score;
                    Session.TargetRigidIndex = CandidateIndex;
                }
            }
        }
        if (Session.TargetRigidIndex == INDEX_NONE && !Session.CandidateRigidIndices.IsEmpty())
        {
            Session.TargetRigidIndex = Session.CandidateRigidIndices[0];
        }
    }
    if (Session.TargetRigidIndex == INDEX_NONE)
    {
        Session.Failure = TEXT("no_candidate_rigid_leaf");
        Session.TickerHandle.Reset();
        return false;
    }
    Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(Session.TargetRigidIndex);
    bool bEnabled = false;
    if (PhysicsObject)
    {
        FLockedReadPhysicsObjectExternalInterface Interface =
            FPhysicsObjectExternalInterface::LockRead({&PhysicsObject, 1});
        bEnabled = !Interface->AreAllDisabled({&PhysicsObject, 1});
    }
    if (!bEnabled)
    {
        const FGeometryCollectionItemIndex ParentItem =
            Proxy->GetInternalClusterParentItemIndex_External(Session.TargetRigidIndex);
        if (ParentItem.IsValid() && ParentItem.IsInternalCluster())
        {
            Session.TargetParentItem = ParentItem.GetItemIndex();
            if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return {};
            Component->ApplyExternalStrain(
                Session.TargetParentItem,
                Component->Bounds.Origin,
                1000.0f,
                10,
                1.0f,
                1000000000000.0f);
            if(!AfterSDK()) return {};
            ++Session.ParentReleaseDispatchCount;
        }
        else
        {
            if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return {};
            Component->ApplyExternalStrain(
                Session.TargetRigidIndex,
                Component->Bounds.Origin,
                1000.0f,
                10,
                1.0f,
                1000000000000.0f);
            if(!AfterSDK()) return {};
            ++Session.ParentReleaseDispatchCount;
        }
        if (Session.TickCount < 300)
        {
            return true;
        }
        Session.Failure = TEXT("target_rigid_leaf_did_not_enable");
        Session.TickerHandle.Reset();
        return false;
    }
    const FGeometryCollectionItemIndex ActiveTargetParent =
        Proxy->GetInternalClusterParentItemIndex_External(Session.TargetRigidIndex);
    if (ActiveTargetParent.IsValid() && ActiveTargetParent.IsInternalCluster())
    {
        Session.TargetParentItem = ActiveTargetParent.GetItemIndex();
        if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return {};
        Component->ApplyExternalStrain(
            Session.TargetParentItem,
            Component->Bounds.Origin,
            1000.0f,
            10,
            1.0f,
            1000000000000.0f);
        if(!AfterSDK()) return {};
        ++Session.ParentReleaseDispatchCount;
        if (Session.TickCount < 300)
        {
            return true;
        }
        Session.Failure = TEXT("target_rigid_leaf_retained_internal_cluster_parent");
        Session.TickerHandle.Reset();
        return false;
    }
    FVector HorizontalVelocity(Session.RequestedVelocity.X, Session.RequestedVelocity.Y, 0.0);
    if (HorizontalVelocity.IsNearlyZero())
    {
        HorizontalVelocity = (Witness->GetComponentLocation() - Component->Bounds.Origin);
        HorizontalVelocity.Z = 0.0;
        HorizontalVelocity = HorizontalVelocity.GetSafeNormal() * 600.0;
    }
    if (Session.TargetCollisionConfiguredCount == 0)
    {
        SetOneWaySiblingSimulationCollision(Session, false);
        if(!BeforeObjectSDK(Component,TEXT("SetCollisionEnabled"))) return {};
        Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Component,TEXT("SetCollisionResponseToAllChannels"))) return {};
        Component->SetCollisionResponseToAllChannels(ECR_Block);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Component,TEXT("SetCollisionObjectType"))) return {};
        Component->SetCollisionObjectType(ECC_Destructible);
        if(!AfterSDK()) return {};
        if (Chaos::FPhysicsObjectHandle TargetPhysicsObject =
            Proxy->GetPhysicsObjectByIndex(Session.TargetRigidIndex))
        {
            const TArray<Chaos::FPhysicsObjectHandle> TargetPhysicsObjects = {TargetPhysicsObject};
            FBodyCollisionFilterData TargetFilterData;
            if(!BeforeSDK(TEXT("build_body_filter_data"),false))return false;
            Component->BodyInstance.BuildBodyFilterData(TargetFilterData);
            if(!AfterSDK())return false;
            FLockedWritePhysicsObjectExternalInterface WriteInterface =
                FPhysicsObjectExternalInterface::LockWrite(TargetPhysicsObjects);
            if(!BeforeSDK(TEXT("UpdateShapeCollisionFlags"))) return {};
            WriteInterface->UpdateShapeCollisionFlags(TargetPhysicsObjects, true, true);
            if(!AfterSDK()) return {};
            WriteInterface->UpdateShapeFilterData(
                TargetPhysicsObjects,
                TargetFilterData.QuerySimpleFilter,
                TargetFilterData.SimFilter);
            Session.TargetCollisionConfiguredCount = TargetPhysicsObjects.Num();
            Session.TargetCollisionConfiguredFrame = GFrameCounter;
        }
        if (Session.TargetCollisionConfiguredCount == 0)
        {
            Session.Failure = TEXT("target_collision_shape_configuration_failed");
            SetOneWaySiblingSimulationCollision(Session, true);
            Session.TickerHandle.Reset();
            return false;
        }
        return true;
    }
    if (GFrameCounter <= Session.TargetCollisionConfiguredFrame)
    {
        return true;
    }
    if (!Session.bWitnessAlignmentCompleted)
    {
        const FGeometryCollectionPhysicsProxy::FParticle* TargetParticle =
            Proxy->GetParticleByIndex_External(Session.TargetRigidIndex);
        if (!TargetParticle)
        {
            Session.Failure = TEXT("target_particle_lost_before_witness_alignment");
            SetOneWaySiblingSimulationCollision(Session, true);
            Session.TickerHandle.Reset();
            return false;
        }
        const FVector HorizontalDirection = HorizontalVelocity.GetSafeNormal();
        constexpr double HorizontalFlightGap = 500.0;
        const double HorizontalSpeed = FMath::Max(HorizontalVelocity.Length(), 1.0);
        const double PredictedFlightTime = HorizontalFlightGap / HorizontalSpeed;
        Session.InitialTargetLocation = TargetParticle->X();
        Session.CalibrationWitnessGap = HorizontalFlightGap;
        Session.StagedHorizontalFlightGap = HorizontalFlightGap;
        Session.PredictedFlightTimeSeconds = PredictedFlightTime;
        Session.StagedGravityZ = World->GetGravityZ();
        Session.AlignedWitnessLocation = Session.InitialTargetLocation +
            HorizontalDirection * HorizontalFlightGap;
        Session.AlignedWitnessLocation.Z = Session.InitialTargetLocation.Z;
        Session.InitialWitnessSeparation = FVector::Distance(
            Session.InitialTargetLocation,
            Session.AlignedWitnessLocation);
        Session.LaunchDirectionDot = FVector::DotProduct(
            HorizontalDirection,
            (Session.AlignedWitnessLocation - Session.InitialTargetLocation).GetSafeNormal());
        if(!BeforeObjectSDK(Witness,TEXT("SetWorldLocation"))) return {};
        Witness->SetWorldLocation(
            Session.AlignedWitnessLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetPhysicsLinearVelocity"))) return {};
        Witness->SetPhysicsLinearVelocity(FVector::ZeroVector);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetPhysicsAngularVelocityInDegrees"))) return {};
        Witness->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        if(!AfterSDK()) return {};
        Session.AlignedWitnessVelocity = Witness->GetPhysicsLinearVelocity();
        Session.WitnessAlignmentFrame = GFrameCounter;
        Session.bWitnessAlignmentCompleted = true;
        return true;
    }
    if (GFrameCounter <= Session.WitnessAlignmentFrame)
    {
        return true;
    }
    const FGeometryCollectionPhysicsProxy::FParticle* LaunchTargetParticle =
        Proxy->GetParticleByIndex_External(Session.TargetRigidIndex);
    if (!LaunchTargetParticle)
    {
        Session.Failure = TEXT("target_particle_lost_before_launch");
        SetOneWaySiblingSimulationCollision(Session, true);
        Session.TickerHandle.Reset();
        return false;
    }
    FVector LaunchTargetToWitness = Witness->GetComponentLocation() - LaunchTargetParticle->X();
    LaunchTargetToWitness.Z = 0.0;
    const FVector HorizontalDirection = LaunchTargetToWitness.GetSafeNormal();
    if (HorizontalDirection.IsNearlyZero())
    {
        Session.Failure = TEXT("target_to_witness_launch_direction_is_zero");
        SetOneWaySiblingSimulationCollision(Session, true);
        Session.TickerHandle.Reset();
        return false;
    }
    const double RequestedHorizontalSpeed = FMath::Max(
        FVector(Session.RequestedVelocity.X, Session.RequestedVelocity.Y, 0.0).Length(),
        HorizontalVelocity.Length());
    const double RequestedLiftVelocity = Session.RequestedVelocity.Z;
    if (!Session.bVelocityDispatchCompleted)
    {
        Session.VelocityDispatchTargetLocation = LaunchTargetParticle->X();
        Session.InitialTargetLocation = Session.VelocityDispatchTargetLocation;
        Session.InitialWitnessSeparation = FVector::Distance(
            Session.InitialTargetLocation,
            Witness->GetComponentLocation());
        Session.LaunchTargetToWitnessDirection = HorizontalDirection;
        Session.RequestedVelocity = HorizontalDirection * RequestedHorizontalSpeed;
        Session.RequestedVelocity.Z = RequestedLiftVelocity;
        Session.LaunchDirectionDot = FVector::DotProduct(
            FVector(Session.RequestedVelocity.X, Session.RequestedVelocity.Y, 0.0).GetSafeNormal(),
            Session.LaunchTargetToWitnessDirection);
        CaptureTraceItemDispatchSnapshot(World, Component, Session.TargetRigidIndex, Session.RequestedVelocity);
        const TArray<int32> TargetRigidIndices = {Session.TargetRigidIndex};
        ApplyDirectVelocityToEnabledTransforms(Component, TargetRigidIndices, Session.RequestedVelocity);
        DispatchOneWayTargetVelocityOnPhysicsThread(Session, Proxy, Session.RequestedVelocity, false);
        StartDeferredTraceVelocityDispatch(Component, Session.TargetRigidIndex);
        Session.VelocityDispatchFrame = GFrameCounter;
        Session.bVelocityDispatchCompleted = true;
        return true;
    }
    Session.ObservedTargetMotionProjection = FVector::DotProduct(
        LaunchTargetParticle->X() - Session.VelocityDispatchTargetLocation,
        Session.LaunchTargetToWitnessDirection);
    if (Session.ObservedTargetMotionProjection <= 1.0)
    {
        const TArray<int32> TargetRigidIndices = {Session.TargetRigidIndex};
        ApplyDirectVelocityToEnabledTransforms(Component, TargetRigidIndices, Session.RequestedVelocity);
        DispatchOneWayTargetVelocityOnPhysicsThread(Session, Proxy, Session.RequestedVelocity, false);
        if (Session.TickCount < 300)
        {
            return true;
        }
        Session.Failure = TEXT("physics_thread_target_velocity_did_not_produce_forward_motion");
        SetOneWaySiblingSimulationCollision(Session, true);
        Session.TickerHandle.Reset();
        return false;
    }
    Session.bTargetMotionObserved = true;
    if (!Session.bContactWitnessAlignmentCompleted)
    {
        FVector ObservedMotionDirection =
            LaunchTargetParticle->X() - Session.VelocityDispatchTargetLocation;
        ObservedMotionDirection.Z = 0.0;
        ObservedMotionDirection = ObservedMotionDirection.GetSafeNormal();
        if (ObservedMotionDirection.IsNearlyZero())
        {
            Session.Failure = TEXT("calibration_motion_direction_is_zero");
            SetOneWaySiblingSimulationCollision(Session, true);
            Session.TickerHandle.Reset();
            return false;
        }
        // Keep the witness outside the largest leaf's contact envelope before
        // the measured launch. The previous 60 cm centre gap could begin with
        // 11-25 cm penetration after the witness physics state synchronized,
        // producing a solver depenetration impulse instead of an approach.
        constexpr double ContactHorizontalGap = 120.0;
        const double ContactFlightTime = ContactHorizontalGap /
            FMath::Max(FVector(Session.RequestedVelocity.X, Session.RequestedVelocity.Y, 0.0).Length(), 1.0);
        Session.StagedHorizontalFlightGap = ContactHorizontalGap;
        Session.PredictedFlightTimeSeconds = ContactFlightTime;
        Session.LaunchTargetToWitnessDirection = ObservedMotionDirection;
        Session.AlignedWitnessLocation = LaunchTargetParticle->X() +
            ObservedMotionDirection * ContactHorizontalGap;
        Session.AlignedWitnessLocation.Z = LaunchTargetParticle->X().Z +
            Session.RequestedVelocity.Z * ContactFlightTime +
            0.5 * Session.StagedGravityZ * FMath::Square(ContactFlightTime);
        if(!BeforeObjectSDK(Component,TEXT("SetCollisionEnabled"))) return {};
        Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Component,TEXT("SetCollisionResponseToAllChannels"))) return {};
        Component->SetCollisionResponseToAllChannels(ECR_Block);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetCollisionProfileName"))) return {};
        Witness->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetCollisionEnabled"))) return {};
        Witness->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetCollisionObjectType"))) return {};
        Witness->SetCollisionObjectType(ECC_PhysicsBody);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetCollisionResponseToAllChannels"))) return {};
        Witness->SetCollisionResponseToAllChannels(ECR_Block);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("update_witness_filter")))return false;
        Witness->BodyInstance.UpdatePhysicsFilterData();
        if(!AfterSDK())return false;
        // Recreate the witness body at the staged pre-contact pose. Moving the
        // component while its previous dynamic body is still live can leave
        // the physics-thread pose hundreds of centimetres behind the game
        // thread, which contaminates the measured displacement.
        if(!BeforeObjectSDK(Witness,TEXT("SetSimulatePhysics"))) return {};
        DropPhysics(Witness);
        Witness->SetSimulatePhysics(false);
        RefreshPhysics(Witness);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetWorldLocation"))) return {};
        Witness->SetWorldLocation(
            Session.AlignedWitnessLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetSimulatePhysics"))) return {};
        DropPhysics(Witness);
        Witness->SetSimulatePhysics(true);
        RefreshPhysics(Witness);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetEnableGravity"))) return {};
        Witness->SetEnableGravity(false);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetMassOverrideInKg"))) return {};
        Witness->SetMassOverrideInKg(NAME_None, 50.0f, true);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetPhysicsLinearVelocity"))) return {};
        Witness->SetPhysicsLinearVelocity(FVector::ZeroVector);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("SetPhysicsAngularVelocityInDegrees"))) return {};
        Witness->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        if(!AfterSDK()) return {};
        if(!BeforeObjectSDK(Witness,TEXT("WakeAllRigidBodies"))) return {};
        Witness->WakeAllRigidBodies();
        if(!AfterSDK()) return {};
        Session.AlignedWitnessVelocity = Witness->GetPhysicsLinearVelocity();
        const TArray<int32> CalibrationStopTargetIndices = {Session.TargetRigidIndex};
        ApplyDirectVelocityToEnabledTransforms(
            Component,
            CalibrationStopTargetIndices,
            FVector::ZeroVector);
        DispatchOneWayTargetVelocityOnPhysicsThread(
            Session,
            Proxy,
            FVector::ZeroVector,
            false);
        Session.ContactWitnessAlignmentFrame = GFrameCounter;
        Session.bContactWitnessAlignmentCompleted = true;
        return true;
    }
    if (GFrameCounter < Session.ContactWitnessAlignmentFrame + 3)
    {
        return true;
    }
    FVector ContactLaunchDirection = Witness->GetComponentLocation() - LaunchTargetParticle->X();
    ContactLaunchDirection.Z = 0.0;
    ContactLaunchDirection = ContactLaunchDirection.GetSafeNormal();
    if (ContactLaunchDirection.IsNearlyZero())
    {
        Session.Failure = TEXT("contact_launch_direction_is_zero");
        SetOneWaySiblingSimulationCollision(Session, true);
        Session.TickerHandle.Reset();
        return false;
    }
    const double ContactHorizontalSpeed = FMath::Max(
        FVector(Session.RequestedVelocity.X, Session.RequestedVelocity.Y, 0.0).Length(), 1.0);
    Session.LaunchTargetToWitnessDirection = ContactLaunchDirection;
    Session.RequestedVelocity = ContactLaunchDirection * ContactHorizontalSpeed;
    Session.RequestedVelocity.Z = RequestedLiftVelocity;
    Session.InitialTargetLocation = LaunchTargetParticle->X();
    Session.InitialWitnessSeparation = FVector::Distance(
        Session.InitialTargetLocation,
        Witness->GetComponentLocation());
    if(!BeforeObjectSDK(Witness,TEXT("WakeAllRigidBodies"))) return {};
    Witness->WakeAllRigidBodies();
    if(!AfterSDK()) return {};
    StartOneWayCollisionCapture(
        World,
        Component,
        Witness,
        Floor,
        Session.TargetRigidIndex,
        Session.LaunchTargetToWitnessDirection,
        Session.InitialWitnessSeparation);
    CaptureTraceItemDispatchSnapshot(World, Component, Session.TargetRigidIndex, Session.RequestedVelocity);
    const TArray<int32> ContactTargetRigidIndices = {Session.TargetRigidIndex};
    ApplyDirectVelocityToEnabledTransforms(
        Component,
        ContactTargetRigidIndices,
        Session.RequestedVelocity);
    EnqueueOneWayPTContactDiagnostics(Session);
    DispatchOneWayTargetVelocityOnPhysicsThread(Session, Proxy, Session.RequestedVelocity, true);
    StartDeferredTraceVelocityDispatch(Component, Session.TargetRigidIndex);
    Session.ContactVelocityDispatchFrame = GFrameCounter;
    Session.LaunchFrame = GFrameCounter;
    Session.bLaunchCompleted = true;
    Session.TickerHandle.Reset();
    return false;
}

void ClearOneWayCollisionCapture(bool bRestoreSiblingCollision = true)
{
    if(!Check()) return;

    if (bRestoreSiblingCollision && ActiveOneWayStagedLaunch)
    {
        SetOneWaySiblingSimulationCollision(*ActiveOneWayStagedLaunch, true);
    }
    if (ActiveOneWayCollisionCapture && ActiveOneWayCollisionCapture->TickerHandle.IsValid())
    {
        FTSTicker::RemoveTicker(ActiveOneWayCollisionCapture->TickerHandle);
    }
    ActiveOneWayCollisionCapture.Reset();
}

bool CaptureOneWayCollisionFrame()
{
    if(!Check()) return {};

    if (!ActiveOneWayCollisionCapture || ActiveOneWayCollisionCapture->bCaptureCompleted)
    {
        return false;
    }
    FOneWayCollisionCaptureSession& Session = *ActiveOneWayCollisionCapture;
    UGeometryCollectionComponent* Component = Session.Component.Get();
    UStaticMeshComponent* Witness = Session.Witness.Get();
    UStaticMeshComponent* Floor = Session.Floor.Get();
    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    if (!Session.World.IsValid() || !Component || !Witness || !Floor || !Proxy)
    {
        if (ActiveOneWayStagedLaunch)
        {
            SetOneWaySiblingSimulationCollision(*ActiveOneWayStagedLaunch, true);
        }
        Session.bCaptureCompleted = true;
        Session.TickerHandle.Reset();
        return false;
    }
    if (GFrameCounter <= Session.LastFrame)
    {
        return true;
    }
    if (ActiveOneWayStagedLaunch &&
        ActiveOneWayStagedLaunch->Component.Get() == Component &&
        ActiveOneWayStagedLaunch->Witness.Get() == Witness)
    {
        EnqueueOneWayPTContactDiagnostics(*ActiveOneWayStagedLaunch);
        Session.PTDiagnostics = ActiveOneWayStagedLaunch->PTContactDiagnostics;
    }
    const FVector Direction = Session.Direction.GetSafeNormal();
    Session.MaximumProjectedWitnessDisplacement = FMath::Max(
        Session.MaximumProjectedWitnessDisplacement,
        FVector::DotProduct(Witness->GetComponentLocation() - Session.InitialWitnessLocation, Direction));
    Session.MaximumProjectedWitnessVelocity = FMath::Max(
        Session.MaximumProjectedWitnessVelocity,
        FVector::DotProduct(Witness->GetPhysicsLinearVelocity() - Session.InitialWitnessVelocity, Direction));
    FVector DebrisLocation = FVector::ZeroVector;
    FVector DebrisVelocity = FVector::ZeroVector;
    int32 DebrisSampleCount = 0;
    bool bNearFloor = false;
    const double FloorTop = Floor->Bounds.GetBox().Max.Z;
    for (int32 TransformIndex : Session.DebrisTransformIndices)
    {
        if (const FGeometryCollectionPhysicsProxy::FParticle* Particle = Proxy->GetParticleByIndex_External(TransformIndex))
        {
            if (!Particle->Disabled())
            {
                DebrisLocation += Particle->X();
                DebrisVelocity += Particle->V();
                bNearFloor |= Particle->X().Z <= FloorTop + 50.0;
                ++DebrisSampleCount;
            }
        }
    }
    if (DebrisSampleCount > 0)
    {
        DebrisLocation /= static_cast<double>(DebrisSampleCount);
        DebrisVelocity /= static_cast<double>(DebrisSampleCount);
        if (Session.CapturedFrameCount == 0)
        {
            Session.InitialDebrisLocation = DebrisLocation;
            Session.InitialDebrisVelocityDirectionDot = DebrisVelocity.IsNearlyZero()
                ? 0.0
                : FVector::DotProduct(DebrisVelocity.GetSafeNormal(), Direction);
        }
        Session.LatestDebrisLocation = DebrisLocation;
        Session.LatestDebrisVelocity = DebrisVelocity;
        const double CurrentSeparation = FVector::Distance(DebrisLocation, Witness->GetComponentLocation());
        if (Session.FirstSampleDebrisWitnessSeparation < 0.0)
        {
            Session.FirstSampleDebrisWitnessSeparation = CurrentSeparation;
        }
        Session.MinimumDebrisWitnessSeparation = FMath::Min(
            Session.MinimumDebrisWitnessSeparation,
            CurrentSeparation);
        Session.ClosingFrameCount += CurrentSeparation <
            Session.PreviousDebrisWitnessSeparation - 0.1 ? 1 : 0;
        Session.ApproachFrameCount += CurrentSeparation <
            Session.InitialDebrisWitnessSeparation - 1.0 ? 1 : 0;
        Session.ContactProximityFrameCount += CurrentSeparation <= 55.0 ? 1 : 0;
        Session.PreviousDebrisWitnessSeparation = CurrentSeparation;
        Session.MaximumProjectedDebrisTravel = FMath::Max(
            Session.MaximumProjectedDebrisTravel,
            FVector::DotProduct(DebrisLocation - Session.InitialDebrisLocation, Direction));
    }
    Session.DebrisFloorProximityFrameCount += bNearFloor ? 1 : 0;
    Session.LastFrame = GFrameCounter;
    ++Session.CapturedFrameCount;
    if ((Session.CapturedFrameCount >= 30 && Session.DebrisFloorProximityFrameCount > 0) ||
        Session.CapturedFrameCount >= 120)
    {
        if (ActiveOneWayStagedLaunch)
        {
            SetOneWaySiblingSimulationCollision(*ActiveOneWayStagedLaunch, true);
        }
        Session.bCaptureCompleted = true;
        Session.TickerHandle.Reset();
        return false;
    }
    return true;
}

void StartOneWayCollisionCapture(
    UWorld* World,
    UGeometryCollectionComponent* Component,
    UStaticMeshComponent* Witness,
    UStaticMeshComponent* Floor,
    int32 TargetTransformIndex,
    const FVector& Direction,
    double InitialSeparation)
{
    if(!Check()) return;

    ClearOneWayCollisionCapture(false);
    CurrentWork->SDK(TEXT("create_FOneWayCollisionCaptureSession"),true);
        ActiveOneWayCollisionCapture = MakeUnique<FOneWayCollisionCaptureSession>();
        ActiveOneWayCollisionCapture->Evidence=CurrentWork;
    ActiveOneWayCollisionCapture->World = World;
    ActiveOneWayCollisionCapture->Component = Component;
    ActiveOneWayCollisionCapture->Witness = Witness;
    ActiveOneWayCollisionCapture->Floor = Floor;
    ActiveOneWayCollisionCapture->TargetTransformIndex = TargetTransformIndex;
    if (FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr)
    {
        if (TargetTransformIndex != INDEX_NONE && Proxy->GetPhysicsObjectByIndex(TargetTransformIndex))
        {
            ActiveOneWayCollisionCapture->DebrisTransformIndices = {TargetTransformIndex};
        }
    }
    ActiveOneWayCollisionCapture->Direction = Direction;
    ActiveOneWayCollisionCapture->InitialDebrisWitnessSeparation = InitialSeparation;
    ActiveOneWayCollisionCapture->PreviousDebrisWitnessSeparation = InitialSeparation;
    ActiveOneWayCollisionCapture->InitialWitnessLocation = Witness->GetComponentLocation();
    ActiveOneWayCollisionCapture->InitialWitnessVelocity = Witness->GetPhysicsLinearVelocity();
    ActiveOneWayCollisionCapture->LastFrame = GFrameCounter;
    ActiveOneWayCollisionCapture->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([](float)
        {
            return ActiveOneWayCollisionCapture ? TickOwned(ActiveOneWayCollisionCapture->Evidence,[]{return CaptureOneWayCollisionFrame();}):false;
        }));
}

struct FChaosPerformanceSession
{
    TSharedPtr<FWork> Evidence;
    TWeakObjectPtr<UWorld> World;
    FString Scenario;
    FString ActorPrefix;
    FString ActorName;
    int32 ExpectedComponentCount = INDEX_NONE;
    int32 CaptureLimitFrames = 90;
    int32 CapturedFrameCount = 0;
    uint64 BeginFrame = 0;
    uint64 LastFrame = 0;
    int32 ComponentCount = 0;
    int32 PeakActivePhysicsObjectCount = 0;
    int32 PeakDisabledPhysicsObjectCount = 0;
    int32 PeakDynamicPhysicsObjectCount = 0;
    int32 PeakSleepingPhysicsObjectCount = 0;
    int32 PeakKinematicPhysicsObjectCount = 0;
    int32 PeakStaticPhysicsObjectCount = 0;
    int32 ComponentSimulatingPhysicsCount = 0;
    int32 ComponentKinematicObjectTypeCount = 0;
    int32 LiveComponentPreparedCount = 0;
    int32 LiveExternalStrainDispatchCount = 0;
    int32 InitialCustomRendererCount = 0;
    int32 LatestCustomRendererCount = 0;
    bool bDoubleRenderDetected = false;
    bool bCaptureCompleted = false;
    double ClusterDispatchMilliseconds = 0.0;
    double LastCaptureWallSeconds = 0.0;
    double LastAppCurrentSeconds = 0.0;
    int32 FrameAvailableSampleCount = 0;
    int32 ControlledTimeSampleCount = 0;
    int32 UnadvancedAppClockSampleCount = 0;
    bool LastUnitDataAvailable = false;
    TArray<TSharedPtr<FJsonValue>> RawMeasurementSnapshots;
    TArray<double> WallTickDeltaMilliseconds;
    TArray<double> FrameMilliseconds;
    TArray<double> GameThreadMilliseconds;
    TArray<double> RenderThreadMilliseconds;
    TArray<double> GPUMilliseconds;
    TArray<double> DrawCalls;
    TArray<int32> ActivePhysicsObjectCounts;
    TArray<int32> DisabledPhysicsObjectCounts;
    TArray<int32> DynamicPhysicsObjectCounts;
    TArray<int32> SleepingPhysicsObjectCounts;
    TArray<int32> KinematicPhysicsObjectCounts;
    TArray<int32> StaticPhysicsObjectCounts;
    FTSTicker::FDelegateHandle TickerHandle;
};

TUniquePtr<FChaosPerformanceSession> ActiveChaosPerformanceSession;

TSharedPtr<FJsonObject> ChaosPerformanceMeasurementAvailability(const FChaosPerformanceSession& Session)
{
    auto Availability = MakeShared<FJsonObject>();
    const bool bAllFramesAvailable = Session.CapturedFrameCount > 0 &&
        Session.FrameAvailableSampleCount == Session.CapturedFrameCount;
    Availability->SetStringField(TEXT("contract_version"), TEXT("GB_measurement_freshness_v2"));
    Availability->SetBoolField(TEXT("unit_data_available"), Session.LastUnitDataAvailable);
    Availability->SetBoolField(TEXT("unit_data_freshness_verified"), false);
    Availability->SetBoolField(TEXT("frame_available"), bAllFramesAvailable);
    Availability->SetBoolField(TEXT("frame_nonzero"), bAllFramesAvailable);
    Availability->SetBoolField(TEXT("frame_measurement_complete"), Session.bCaptureCompleted && bAllFramesAvailable &&
        Session.CapturedFrameCount >= Session.CaptureLimitFrames && Session.Evidence && Session.Evidence->R.Error.IsEmpty());
    Availability->SetNumberField(TEXT("frame_available_sample_count"), Session.FrameAvailableSampleCount);
    Availability->SetNumberField(TEXT("frame_unavailable_sample_count"), Session.CapturedFrameCount - Session.FrameAvailableSampleCount);
    Availability->SetNumberField(TEXT("controlled_time_sample_count"), Session.ControlledTimeSampleCount);
    Availability->SetNumberField(TEXT("unadvanced_app_clock_sample_count"), Session.UnadvancedAppClockSampleCount);
    Availability->SetStringField(TEXT("frame_source"), TEXT("FApp::GetCurrentTime()-FApp::GetLastTime(), admitted only for advancing finite positive normal realtime engine intervals"));
    Availability->SetStringField(TEXT("frame_scope"), TEXT("Previous whole-engine frame interval including pacing/end-of-frame idle, matching the non-fixed DrawStat expression; not per-world, CPU busy, solver, GPU or wall-ticker duration. Controlled fixed/benchmark/custom/override-FPS time is unavailable."));
    Availability->SetBoolField(TEXT("game_thread_available"), false);
    Availability->SetBoolField(TEXT("render_thread_available"), false);
    Availability->SetBoolField(TEXT("gpu0_available"), false);
    Availability->SetBoolField(TEXT("draw_calls_available"), false);
    Availability->SetBoolField(TEXT("chaos_simulation_proxy_available"), false);
    Availability->SetStringField(TEXT("chaos_simulation_proxy_scope"), TEXT("Legacy proxy aggregates alias the unavailable game-thread sample array; they are not a direct Chaos simulation/solver timer and numeric 0 is not measured simulation cost."));
    Availability->SetBoolField(TEXT("game_nonzero"), false);
    Availability->SetBoolField(TEXT("render_nonzero"), false);
    Availability->SetBoolField(TEXT("gpu0_nonzero"), false);
    Availability->SetBoolField(TEXT("all_legacy_metrics_measured"), false);
    Availability->SetStringField(TEXT("unavailable_reason"), TEXT("Unit data is refreshed by DrawStat. The sampled thread/RHI counters expose no verified frame-generation stamp here; they are raw diagnostic snapshots only."));
    Availability->SetStringField(TEXT("unavailable_numeric_policy"), TEXT("Unavailable legacy scalar/array metrics contain numeric 0 to preserve their types and cardinality; 0 is a placeholder, not measured zero cost. Frame aggregates are unavailable if any admitted sample is missing."));
    Availability->SetStringField(TEXT("scope"), TEXT("Capture completion and the original component-count assertion do not certify timing availability, physics step completion or a five-run performance comparison. Wall-ticker cadence remains a separate metric; no UI stats were enabled."));
    return Availability;
}


bool MatchesChaosPerformanceActor(const FChaosPerformanceSession& Session, const AActor* Actor)
{
    if (!Actor)
    {
        return false;
    }
    if (!Session.ActorName.IsEmpty())
    {
        return Actor->GetName() == Session.ActorName || Actor->GetActorLabel() == Session.ActorName ||
            Actor->GetPathName() == Session.ActorName;
    }
    return Actor->GetName().StartsWith(Session.ActorPrefix);
}

double Percentile(TArray<double> Values, double Fraction)
{
    if (Values.IsEmpty())
    {
        return 0.0;
    }
    Values.Sort();
    const int32 Index = FMath::Clamp(
        FMath::CeilToInt(Fraction * static_cast<double>(Values.Num())) - 1,
        0,
        Values.Num() - 1);
    return Values[Index];
}

void ClearChaosPerformanceSession()
{
    if(!Check()) return;

    if (ActiveChaosPerformanceSession)
    {
        if (ActiveChaosPerformanceSession->TickerHandle.IsValid())
        {
            FTSTicker::RemoveTicker(ActiveChaosPerformanceSession->TickerHandle);
        }
    }
    ActiveChaosPerformanceSession.Reset();
}

bool CaptureChaosPerformanceFrame()
{
    if(!Check()) return {};

    if (!ActiveChaosPerformanceSession || ActiveChaosPerformanceSession->bCaptureCompleted)
    {
        return false;
    }
    FChaosPerformanceSession& Session = *ActiveChaosPerformanceSession;
    UWorld* World = Session.World.Get();
    if (!World)
    {
        Session.bCaptureCompleted = true;
        Session.TickerHandle.Reset();
        return false;
    }
    if (GFrameCounter <= Session.LastFrame)
    {
        return true;
    }
    const double CaptureWallSeconds = FPlatformTime::Seconds();
    const double WallTickDeltaMs = Session.LastCaptureWallSeconds > 0.0
        ? (CaptureWallSeconds - Session.LastCaptureWallSeconds) * 1000.0
        : 0.0;
    Session.LastCaptureWallSeconds = CaptureWallSeconds;
    int32 ComponentCount = 0;
    int32 ActiveCount = 0;
    int32 DisabledCount = 0;
    int32 DynamicCount = 0;
    int32 SleepingCount = 0;
    int32 KinematicCount = 0;
    int32 StaticCount = 0;
    int32 SimulatingComponentCount = 0;
    int32 KinematicObjectTypeComponentCount = 0;
    int32 CustomRendererCount = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (!MatchesChaosPerformanceActor(Session, *It))
        {
            continue;
        }
        UGeometryCollectionComponent* Component = It->FindComponentByClass<UGeometryCollectionComponent>();
        if (!Component)
        {
            continue;
        }
        ++ComponentCount;
        SimulatingComponentCount += Component->IsSimulatingPhysics() ? 1 : 0;
        KinematicObjectTypeComponentCount +=
            Component->ObjectType == EObjectStateTypeEnum::Chaos_Object_Kinematic ? 1 : 0;
        CustomRendererCount += Component->GetCustomRenderer() != nullptr ? 1 : 0;
        const TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects = CheckedPhysicsObjects(Component);
        if (!PhysicsObjects.IsEmpty())
        {
            FLockedReadPhysicsObjectExternalInterface Interface =
                FPhysicsObjectExternalInterface::LockRead(PhysicsObjects);
            for (Chaos::FPhysicsObjectHandle PhysicsObject : PhysicsObjects)
            {
                const bool bDisabled = Interface->AreAllDisabled({&PhysicsObject, 1});
                DisabledCount += bDisabled ? 1 : 0;
                ActiveCount += bDisabled ? 0 : 1;
                if (!bDisabled)
                {
                    if (const Chaos::FPBDRigidParticle* Particle = Interface->GetRigidParticle(PhysicsObject))
                    {
                        switch (Particle->ObjectState())
                        {
                        case Chaos::EObjectStateType::Dynamic: ++DynamicCount; break;
                        case Chaos::EObjectStateType::Sleeping: ++SleepingCount; break;
                        case Chaos::EObjectStateType::Kinematic: ++KinematicCount; break;
                        case Chaos::EObjectStateType::Static: ++StaticCount; break;
                        default: break;
                        }
                    }
                }
            }
        }
    }
    Session.ComponentCount = ComponentCount;
    Session.PeakActivePhysicsObjectCount = FMath::Max(Session.PeakActivePhysicsObjectCount, ActiveCount);
    Session.PeakDisabledPhysicsObjectCount = FMath::Max(Session.PeakDisabledPhysicsObjectCount, DisabledCount);
    Session.PeakDynamicPhysicsObjectCount = FMath::Max(Session.PeakDynamicPhysicsObjectCount, DynamicCount);
    Session.PeakSleepingPhysicsObjectCount = FMath::Max(Session.PeakSleepingPhysicsObjectCount, SleepingCount);
    Session.PeakKinematicPhysicsObjectCount = FMath::Max(Session.PeakKinematicPhysicsObjectCount, KinematicCount);
    Session.PeakStaticPhysicsObjectCount = FMath::Max(Session.PeakStaticPhysicsObjectCount, StaticCount);
    Session.ComponentSimulatingPhysicsCount = SimulatingComponentCount;
    Session.ComponentKinematicObjectTypeCount = KinematicObjectTypeComponentCount;
    Session.LatestCustomRendererCount = CustomRendererCount;
    if (Session.CapturedFrameCount == 0)
    {
        Session.InitialCustomRendererCount = CustomRendererCount;
    }
    // FStatUnitData is updated by DrawStat, not by this ticker. A nonzero
    // snapshot or a nonnull unit-data pointer cannot prove per-frame freshness.
    const double AppCurrentSeconds = FApp::GetCurrentTime();
    const double AppLastSeconds = FApp::GetLastTime();
    const double AppFrameMs = (AppCurrentSeconds - AppLastSeconds) * 1000.0;
    const bool bBenchmarking = FApp::IsBenchmarking();
    const bool bFixedTimeStep = FApp::UseFixedTimeStep();
    const bool bFixedFrameRate = GEngine && GEngine->bUseFixedFrameRate;
    const bool bCustomTimeStep = GEngine && GEngine->GetCustomTimeStep() != nullptr;
    const IConsoleVariable* OverrideFPSVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("t.OverrideFPS"));
    const double OverrideFPS = OverrideFPSVariable ? static_cast<double>(OverrideFPSVariable->GetFloat()) : 0.0;
    const bool bOverrideFPSControlUnknown = !OverrideFPSVariable || !FMath::IsFinite(OverrideFPS);
    const bool bOverrideFPS = !bOverrideFPSControlUnknown && OverrideFPS >= 0.001;
    const bool bClockAdvanced = FMath::IsFinite(AppCurrentSeconds) &&
        FMath::IsFinite(Session.LastAppCurrentSeconds) && AppCurrentSeconds > Session.LastAppCurrentSeconds;
    const bool bFrameAvailable = !bBenchmarking && !bFixedTimeStep && !bFixedFrameRate &&
        !bCustomTimeStep && !bOverrideFPS && !bOverrideFPSControlUnknown && bClockAdvanced && FMath::IsFinite(AppLastSeconds) &&
        FMath::IsFinite(AppFrameMs) && AppFrameMs > 0.0;
    const double FrameMs = bFrameAvailable ? AppFrameMs : 0.0;
    Session.LastAppCurrentSeconds = AppCurrentSeconds;
    Session.FrameAvailableSampleCount += bFrameAvailable ? 1 : 0;
    Session.ControlledTimeSampleCount += bBenchmarking || bFixedTimeStep || bFixedFrameRate || bCustomTimeStep || bOverrideFPS || bOverrideFPSControlUnknown ? 1 : 0;
    Session.UnadvancedAppClockSampleCount += bClockAdvanced ? 0 : 1;

    auto Snapshot = MakeShared<FJsonObject>();
    const auto SnapshotNumber = [&Snapshot](const TCHAR* Key, double Value)
    {
        if (FMath::IsFinite(Value)) Snapshot->SetNumberField(Key, Value);
        else Snapshot->SetField(Key, MakeShared<FJsonValueNull>());
    };
    SnapshotNumber(TEXT("game_frame_counter"), static_cast<double>(GFrameCounter));
    SnapshotNumber(TEXT("capture_wall_seconds"), CaptureWallSeconds);
    SnapshotNumber(TEXT("wall_tick_delta_ms"), WallTickDeltaMs);
    SnapshotNumber(TEXT("app_current_seconds"), AppCurrentSeconds);
    SnapshotNumber(TEXT("app_last_seconds"), AppLastSeconds);
    SnapshotNumber(TEXT("app_current_minus_last_ms"), AppFrameMs);
    SnapshotNumber(TEXT("app_delta_seconds"), FApp::GetDeltaTime());
    SnapshotNumber(TEXT("app_idle_seconds"), FApp::GetIdleTime());
    Snapshot->SetBoolField(TEXT("app_clock_advanced"), bClockAdvanced);
    Snapshot->SetBoolField(TEXT("benchmarking"), bBenchmarking);
    Snapshot->SetBoolField(TEXT("fixed_time_step"), bFixedTimeStep);
    Snapshot->SetBoolField(TEXT("fixed_frame_rate"), bFixedFrameRate);
    Snapshot->SetBoolField(TEXT("custom_time_step"), bCustomTimeStep);
    Snapshot->SetBoolField(TEXT("override_fps_variable_available"), OverrideFPSVariable != nullptr);
    Snapshot->SetBoolField(TEXT("override_fps_active"), bOverrideFPS);
    Snapshot->SetBoolField(TEXT("override_fps_control_state_known"), !bOverrideFPSControlUnknown);
    SnapshotNumber(TEXT("override_fps"), OverrideFPS);
    Snapshot->SetBoolField(TEXT("frame_available"), bFrameAvailable);
    Snapshot->SetStringField(TEXT("frame_unavailable_reason"), bFrameAvailable ? TEXT("") :
        (bBenchmarking || bFixedTimeStep || bFixedFrameRate || bCustomTimeStep || bOverrideFPS || bOverrideFPSControlUnknown ?
            TEXT("Engine time is controlled or override-FPS state is unavailable; FApp interval is not admitted as real elapsed frame time") :
            TEXT("FApp current time did not advance or its current-last interval is nonfinite/nonpositive")));
    const FStatUnitData* UnitData = GEngine && GEngine->GameViewport ?
        GEngine->GameViewport->GetStatUnitData() : nullptr;
    Snapshot->SetBoolField(TEXT("unit_data_available"), UnitData != nullptr);
    if (UnitData)
    {
        SnapshotNumber(TEXT("unit_raw_frame_ms"), UnitData->RawFrameTime);
        SnapshotNumber(TEXT("unit_raw_game_thread_ms"), UnitData->RawGameThreadTime);
        SnapshotNumber(TEXT("unit_raw_render_thread_ms"), UnitData->RawRenderThreadTime);
        SnapshotNumber(TEXT("unit_raw_gpu0_ms"), UnitData->RawGPUFrameTime[0]);
    }
    SnapshotNumber(TEXT("raw_game_thread_cycles"), static_cast<double>(GGameThreadTime));
    SnapshotNumber(TEXT("raw_render_thread_cycles"), static_cast<double>(GRenderThreadTime));
    SnapshotNumber(TEXT("raw_gpu0_frame_cycles"), static_cast<double>(RHIGetGPUFrameCycles(0)));
    SnapshotNumber(TEXT("raw_gpu0_draw_calls"), static_cast<double>(GNumDrawCallsRHI[0]));
    Snapshot->SetBoolField(TEXT("unit_thread_rhi_freshness_verified"), false);
    Session.LastUnitDataAvailable = UnitData != nullptr;
    Session.RawMeasurementSnapshots.Add(MakeShared<FJsonValueObject>(Snapshot));

    // Keep the original numeric keys and sample-array shape. These unavailable
    // zeros are placeholders, never measured zero cost and never a frame fallback.
    Session.WallTickDeltaMilliseconds.Add(WallTickDeltaMs);
    Session.FrameMilliseconds.Add(FrameMs);
    Session.GameThreadMilliseconds.Add(0.0);
    Session.RenderThreadMilliseconds.Add(0.0);
    Session.GPUMilliseconds.Add(0.0);
    Session.DrawCalls.Add(0.0);
    Session.ActivePhysicsObjectCounts.Add(ActiveCount);
    Session.DisabledPhysicsObjectCounts.Add(DisabledCount);
    Session.DynamicPhysicsObjectCounts.Add(DynamicCount);
    Session.SleepingPhysicsObjectCounts.Add(SleepingCount);
    Session.KinematicPhysicsObjectCounts.Add(KinematicCount);
    Session.StaticPhysicsObjectCounts.Add(StaticCount);
    Session.LastFrame = GFrameCounter;
    ++Session.CapturedFrameCount;
    Session.bCaptureCompleted = Session.CapturedFrameCount >= Session.CaptureLimitFrames;
    CurrentWork->R.Data->SetObjectField(TEXT("measurement_availability"), ChaosPerformanceMeasurementAvailability(Session));
    CurrentWork->R.Data->SetArrayField(TEXT("raw_measurement_snapshots"), Session.RawMeasurementSnapshots);
    if (Session.bCaptureCompleted)
    {
        Session.bCaptureCompleted = true;
        Session.TickerHandle.Reset();
        return false;
    }
    return true;
}

struct FRemovalPieceSnapshot
{
    int32 TargetCount = 0;
    int32 TargetHiddenCount = 0;
    int32 TargetDisabledCount = 0;
    int32 TargetBrokenCount = 0;
    int32 TargetSleepingCount = 0;
    int32 TargetGoneCount = 0;
    int32 TargetRemovalActiveCount = 0;
    int32 TargetDecayingCount = 0;
    int32 TargetFullyDecayedCount = 0;
    int32 LargeCount = 0;
    int32 LargeRemainingCount = 0;
};

struct FRemovalPieceTimeline
{
    int32 TransformIndex = INDEX_NONE;
    bool bTarget = false;
    bool bEverReleased = false;
    double FirstBrokenWorldTime = -1.0;
    double FirstSleepingWorldTime = -1.0;
    double FirstGoneWorldTime = -1.0;
    double FirstDecayStartWorldTime = -1.0;
    uint64 FirstBrokenFrame = 0;
    uint64 FirstSleepingFrame = 0;
    uint64 FirstGoneFrame = 0;
    uint64 FirstDecayStartFrame = 0;
    float LatestDecay = 0.0f;
    FVector2D BreakTimerRange = FVector2D::ZeroVector;
    FVector2D ExpectedCompletionDelayRange = FVector2D::ZeroVector;
    double SleepTimerSeconds = 0.0;
    double SleepRemovalDurationSeconds = 0.0;
    FString BreakTimeSource;
    FString SleepTimeSource;
};

struct FRemovalLearningSession
{
    TSharedPtr<FWork> Evidence;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UGeometryCollectionComponent> Component;
    TWeakObjectPtr<UChaosGameplayEventDispatcher> BreakEventDispatcher;
    FString Mode;
    TArray<int32> TargetIndices;
    TArray<int32> LargeIndices;
    uint64 BeginFrame = 0;
    uint64 LastSampleFrame = 0;
    double BeginWorldTime = 0.0;
    double AllGoneWorldTime = -1.0;
    int32 CaptureLimitFrames = 300;
    int32 CapturedFrameCount = 0;
    bool bCaptureCompleted = false;
    int32 ForcedSleepAttemptCount = 0;
    int32 ForcedSleepObjectCount = 0;
    int32 GameplayBreakEventCount = 0;
    int32 SleepReleaseRootItem = INDEX_NONE;
    int32 SleepReleaseRootDispatchCount = 0;
    uint64 SleepReleaseRootDispatchFrame = 0;
    TArray<int32> SleepReleaseParentItems;
    int32 SleepReleaseParentDispatchCount = 0;
    uint64 SleepReleaseParentDispatchFrame = 0;
    int32 SleepReleaseTargetDispatchCount = 0;
    uint64 SleepReleaseTargetDispatchFrame = 0;
    int32 SleepReleaseTargetReapplyCount = 0;
    TSet<int32> CollisionIsolatedTargetIndices;
    bool bTargetCollisionRestored = false;
    int32 PTSleepCommandDispatchCount = 0;
    int32 PTSleepRequestedParticleCount = 0;
    TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> PTSleepAppliedParticleCount;
    bool bOriginalGravityEnabled = true;
    bool bGravityOverridden = false;
    bool bGravityRestored = false;
    uint64 LastForcedSleepFrame = 0;
    FString CompletionReason;
    FTSTicker::FDelegateHandle TickerHandle;
    TMap<int32, FRemovalPieceTimeline> PieceTimelines;
    FRemovalPieceSnapshot Previous;
    TArray<TSharedPtr<FJsonValue>> Samples;
};

TUniquePtr<FRemovalLearningSession> ActiveRemovalSession;

int32 SetRemovalTargetSimulationCollision(FRemovalLearningSession& Session, bool bSimCollision)
{
    if(!Check()) return {};

    UGeometryCollectionComponent* Component = Session.Component.Get();
    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    if (!Proxy)
    {
        return 0;
    }
    TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects;
    if (bSimCollision)
    {
        for (int32 TransformIndex : Session.CollisionIsolatedTargetIndices)
        {
            if (Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(TransformIndex))
            {
                PhysicsObjects.Add(PhysicsObject);
            }
        }
    }
    else
    {
        for (int32 TransformIndex : Session.TargetIndices)
        {
            const FRemovalPieceTimeline* Timeline = Session.PieceTimelines.Find(TransformIndex);
            if (Timeline && Timeline->FirstSleepingWorldTime >= 0.0)
            {
                continue;
            }
            if (Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(TransformIndex))
            {
                PhysicsObjects.Add(PhysicsObject);
                Session.CollisionIsolatedTargetIndices.Add(TransformIndex);
            }
        }
    }
    if (!PhysicsObjects.IsEmpty())
    {
        FLockedWritePhysicsObjectExternalInterface WriteInterface =
            FPhysicsObjectExternalInterface::LockWrite(PhysicsObjects);
        if(!BeforeSDK(TEXT("UpdateShapeCollisionFlags"))) return {};
        WriteInterface->UpdateShapeCollisionFlags(PhysicsObjects, bSimCollision, true);
        if(!AfterSDK()) return {};
    }
    if (bSimCollision)
    {
        Session.bTargetCollisionRestored = true;
    }
    return PhysicsObjects.Num();
}

void RestoreRemovalGravity(FRemovalLearningSession& Session)
{
    TGuardValue<bool> RestoreAdmission(Restoring,true);

    if(!Check()) return;

    if (Session.bGravityOverridden && !Session.bGravityRestored)
    {
        if (UGeometryCollectionComponent* Component = Session.Component.Get())
        {
            if(!BeforeObjectSDK(Component,TEXT("SetEnableGravity"))) return;
            Component->SetEnableGravity(Session.bOriginalGravityEnabled);
            if(!AfterSDK()) return;
            Session.bGravityRestored = Component->IsGravityEnabled() == Session.bOriginalGravityEnabled;
        }
    }
}

FRemovalPieceSnapshot CaptureRemovalPieceSnapshot(
    UGeometryCollectionComponent* Component,
    FRemovalLearningSession& Session,
    double WorldTime,
    uint64 Frame)
{
    if(!Check()) return {};

    FRemovalPieceSnapshot Result;
    Result.TargetCount = Session.TargetIndices.Num();
    Result.LargeCount = Session.LargeIndices.Num();
    if (!Component)
    {
        return Result;
    }
    TArray<bool> HiddenTransforms;
    Component->GetHiddenTransforms(HiddenTransforms);
    FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection();
    TUniquePtr<FGeometryCollectionDynamicStateFacade> State;
    TUniquePtr<FGeometryCollectionDecayDynamicFacade> DecayFacade;
    TUniquePtr<FGeometryCollectionRemoveOnBreakDynamicFacade> BreakRemovalFacade;
    TUniquePtr<FGeometryCollectionRemoveOnSleepDynamicFacade> SleepRemovalFacade;
    if (Dynamic)
    {
        State = MakeUnique<FGeometryCollectionDynamicStateFacade>(*Dynamic);
        DecayFacade = MakeUnique<FGeometryCollectionDecayDynamicFacade>(*Dynamic);
        BreakRemovalFacade = MakeUnique<FGeometryCollectionRemoveOnBreakDynamicFacade>(*Dynamic);
        SleepRemovalFacade = MakeUnique<FGeometryCollectionRemoveOnSleepDynamicFacade>(*Dynamic);
    }
    FGeometryCollectionPhysicsProxy* Proxy = Component->GetPhysicsProxy();
    auto IsDisabled = [Proxy](int32 TransformIndex)
    {
        if (!Proxy)
        {
            return false;
        }
        Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(TransformIndex);
        if (!PhysicsObject)
        {
            return true;
        }
        FLockedReadPhysicsObjectExternalInterface PhysicsObjectInterface =
            FPhysicsObjectExternalInterface::LockRead({&PhysicsObject, 1});
        return PhysicsObjectInterface->AreAllDisabled({&PhysicsObject, 1});
    };
    auto UpdateTimeline = [&](int32 Index, bool bTarget)
    {
        FRemovalPieceTimeline& Timeline = Session.PieceTimelines.FindChecked(Index);
        const bool bHidden = HiddenTransforms.IsValidIndex(Index) && HiddenTransforms[Index];
        const bool bDisabled = IsDisabled(Index);
        const bool bBroken = State && State->HasBrokenOff(Index);
        const bool bSleeping = State && State->IsSleeping(Index);
        const float Decay = DecayFacade && DecayFacade->IsValid() ? DecayFacade->GetDecay(Index) : 0.0f;
        const bool bRemovalActive = Session.Mode == TEXT("break_only")
            ? BreakRemovalFacade && BreakRemovalFacade->IsValid() && BreakRemovalFacade->IsRemovalActive(Index)
            : SleepRemovalFacade && SleepRemovalFacade->IsValid() && SleepRemovalFacade->IsRemovalActive(Index);
        if (bTarget && Session.Mode == TEXT("break_only") && !Timeline.bEverReleased &&
            Decay > KINDA_SMALL_NUMBER)
        {
            const double BreakTimer = 0.5 * (Timeline.BreakTimerRange.X + Timeline.BreakTimerRange.Y);
            Timeline.bEverReleased = true;
            Timeline.FirstBrokenWorldTime = WorldTime - BreakTimer;
            Timeline.FirstBrokenFrame = Frame;
            Timeline.BreakTimeSource = TEXT("inferred_from_decay_start_minus_asset_timer");
        }
        if (bTarget && Session.Mode == TEXT("sleep_only") && !Timeline.bEverReleased &&
            (bBroken || !bDisabled))
        {
            Timeline.bEverReleased = true;
            Timeline.FirstBrokenWorldTime = WorldTime;
            Timeline.FirstBrokenFrame = Frame;
            Timeline.BreakTimeSource = bBroken
                ? TEXT("dynamic_state_has_broken_off")
                : TEXT("enabled_physics_object_after_parent_release");
        }
        if (Timeline.bEverReleased && Timeline.FirstSleepingWorldTime < 0.0 && bSleeping)
        {
            Timeline.FirstSleepingWorldTime = WorldTime;
            Timeline.FirstSleepingFrame = Frame;
            Timeline.SleepTimeSource = TEXT("dynamic_state_is_sleeping");
        }
        if ((Timeline.bEverReleased || !Timeline.bTarget) && Timeline.FirstDecayStartWorldTime < 0.0 && Decay > KINDA_SMALL_NUMBER)
        {
            Timeline.FirstDecayStartWorldTime = WorldTime;
            Timeline.FirstDecayStartFrame = Frame;
            if (bTarget && Session.Mode == TEXT("sleep_only") && Timeline.SleepTimerSeconds > 0.0)
            {
                Timeline.FirstSleepingWorldTime = WorldTime - Timeline.SleepTimerSeconds;
                Timeline.FirstSleepingFrame = Frame;
                Timeline.SleepTimeSource = TEXT("inferred_from_decay_start_minus_asset_max_sleep_time");
            }
        }
        Timeline.LatestDecay = Decay;
        const bool bVisualRemovalComplete = Timeline.bTarget && Timeline.bEverReleased &&
            Decay >= 1.0f - KINDA_SMALL_NUMBER;
        if (Timeline.FirstGoneWorldTime < 0.0 && bVisualRemovalComplete)
        {
            Timeline.FirstGoneWorldTime = WorldTime;
            Timeline.FirstGoneFrame = Frame;
        }
        if (bTarget)
        {
            Result.TargetHiddenCount += bHidden ? 1 : 0;
            Result.TargetDisabledCount += bDisabled ? 1 : 0;
            Result.TargetBrokenCount += Timeline.bEverReleased ? 1 : 0;
            Result.TargetSleepingCount += bSleeping ? 1 : 0;
            Result.TargetGoneCount += Timeline.FirstGoneWorldTime >= 0.0 ? 1 : 0;
            Result.TargetRemovalActiveCount += bRemovalActive ? 1 : 0;
            Result.TargetDecayingCount += Decay > KINDA_SMALL_NUMBER && Decay < 1.0f - KINDA_SMALL_NUMBER ? 1 : 0;
            Result.TargetFullyDecayedCount += Decay >= 1.0f - KINDA_SMALL_NUMBER ? 1 : 0;
        }
        else
        {
            Result.LargeRemainingCount += !bHidden && Decay < 1.0f - KINDA_SMALL_NUMBER ? 1 : 0;
        }
    };
    for (int32 Index : Session.TargetIndices)
    {
        UpdateTimeline(Index, true);
    }
    for (int32 Index : Session.LargeIndices)
    {
        UpdateTimeline(Index, false);
    }
    return Result;
}

TSharedPtr<FJsonObject> RemovalPieceSnapshotToJson(const FRemovalPieceSnapshot& Snapshot)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("target_count"), Snapshot.TargetCount);
    Data->SetNumberField(TEXT("target_hidden_count"), Snapshot.TargetHiddenCount);
    Data->SetNumberField(TEXT("target_visible_count"), Snapshot.TargetCount - Snapshot.TargetHiddenCount);
    Data->SetNumberField(TEXT("target_disabled_count"), Snapshot.TargetDisabledCount);
    Data->SetNumberField(TEXT("target_broken_count"), Snapshot.TargetBrokenCount);
    Data->SetNumberField(TEXT("target_sleeping_count"), Snapshot.TargetSleepingCount);
    Data->SetNumberField(TEXT("target_gone_count"), Snapshot.TargetGoneCount);
    Data->SetNumberField(TEXT("target_remaining_count"), Snapshot.TargetCount - Snapshot.TargetGoneCount);
    Data->SetNumberField(TEXT("target_removal_active_count"), Snapshot.TargetRemovalActiveCount);
    Data->SetNumberField(TEXT("target_decaying_count"), Snapshot.TargetDecayingCount);
    Data->SetNumberField(TEXT("target_fully_decayed_count"), Snapshot.TargetFullyDecayedCount);
    Data->SetNumberField(TEXT("designated_large_count"), Snapshot.LargeCount);
    Data->SetNumberField(TEXT("designated_large_remaining_count"), Snapshot.LargeRemainingCount);
    return Data;
}

void ClearActiveRemovalSession()
{
    TGuardValue<bool> RestoreAdmission(Restoring,true);

    if(!Check()) return;

    if (ActiveRemovalSession)
    {
        SetRemovalTargetSimulationCollision(*ActiveRemovalSession, true);
        RestoreRemovalGravity(*ActiveRemovalSession);
        if (UChaosGameplayEventDispatcher* Dispatcher = ActiveRemovalSession->BreakEventDispatcher.Get())
        {
            if (UGeometryCollectionComponent* Component = ActiveRemovalSession->Component.Get())
            {
                UnregisterOwnedBreak(Component);
            }
        }
    }
    if (ActiveRemovalSession && ActiveRemovalSession->TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ActiveRemovalSession->TickerHandle);
        ActiveRemovalSession->TickerHandle.Reset();
    }
    if(!ActiveRemovalSession||((!ActiveRemovalSession->bGravityOverridden||ActiveRemovalSession->bGravityRestored)&&(ActiveRemovalSession->CollisionIsolatedTargetIndices.IsEmpty()||ActiveRemovalSession->bTargetCollisionRestored)))ActiveRemovalSession.Reset();
    else{ActiveRemovalSession->bCaptureCompleted=true;CurrentWork->R.Data->SetBoolField(TEXT("unresolved_removal_restore_lease"),true);}
}

int32 ForceSleepEnabledRemovalTargets(FRemovalLearningSession& Session)
{
    if(!Check()) return {};

    UGeometryCollectionComponent* Component = Session.Component.Get();
    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    if (!Proxy)
    {
        return 0;
    }
    SetRemovalTargetSimulationCollision(Session, false);
    TArray<Chaos::FPhysicsObjectHandle> TargetPhysicsObjects;
    for (int32 TargetIndex : Session.TargetIndices)
    {
        const FRemovalPieceTimeline* Timeline = Session.PieceTimelines.Find(TargetIndex);
        if (Timeline && Timeline->FirstSleepingWorldTime >= 0.0)
        {
            continue;
        }
        if (Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(TargetIndex))
        {
            TargetPhysicsObjects.Add(PhysicsObject);
        }
    }
    TArray<Chaos::FPhysicsObjectHandle> EnabledTargetPhysicsObjects;
    if (!TargetPhysicsObjects.IsEmpty())
    {
        FLockedReadPhysicsObjectExternalInterface ReadInterface =
            FPhysicsObjectExternalInterface::LockRead(TargetPhysicsObjects);
        for (Chaos::FPhysicsObjectHandle PhysicsObject : TargetPhysicsObjects)
        {
            if (!ReadInterface->AreAllDisabled({&PhysicsObject, 1}))
            {
                EnabledTargetPhysicsObjects.Add(PhysicsObject);
            }
        }
    }
    if (!EnabledTargetPhysicsObjects.IsEmpty())
    {
        FLockedWritePhysicsObjectExternalInterface WriteInterface =
            FPhysicsObjectExternalInterface::LockWrite(EnabledTargetPhysicsObjects);
        if(!BeforeSDK(TEXT("UpdateShapeCollisionFlags"))) return {};
        WriteInterface->UpdateShapeCollisionFlags(EnabledTargetPhysicsObjects, false, true);
        if(!AfterSDK()) return {};
        if(!BeforeSDK(TEXT("SetLinearImpulseVelocity"))) return {};
        WriteInterface->SetLinearImpulseVelocity(EnabledTargetPhysicsObjects, FVector::ZeroVector, true);
        if(!AfterSDK()) return {};
        if(!BeforeSDK(TEXT("PutToSleep"))) return {};
        WriteInterface->PutToSleep(EnabledTargetPhysicsObjects);
        if(!AfterSDK()) return {};
    }
    int32 SleepingParticleCount = 0;
    for (int32 TargetIndex : Session.TargetIndices)
    {
        const FRemovalPieceTimeline* Timeline = Session.PieceTimelines.Find(TargetIndex);
        if (Timeline && Timeline->FirstSleepingWorldTime >= 0.0)
        {
            continue;
        }
        if (FGeometryCollectionPhysicsProxy::FParticle* Particle = Proxy->GetParticleByIndex_External(TargetIndex))
        {
            if (!Particle->Disabled())
            {
                if(!BeforeSDK(TEXT("SetV"))) return {};
                Particle->SetV(FVector::ZeroVector);
                if(!AfterSDK()) return {};
                if(!BeforeSDK(TEXT("SetW"))) return {};
                Particle->SetW(FVector::ZeroVector);
                if(!AfterSDK()) return {};
                if(!BeforeSDK(TEXT("SetObjectState"))) return {};
                Particle->SetObjectState(Chaos::EObjectStateType::Sleeping);
                if(!AfterSDK()) return {};
                ++SleepingParticleCount;
            }
        }
    }
    if (SleepingParticleCount > 0)
    {
        if(!BeforeSDK(TEXT("SetProxyDirty_External"))) return {};
        Proxy->SetProxyDirty_External();
        if(!AfterSDK()) return {};
    }
    TArray<int32> PendingTargetIndices;
    for (int32 TargetIndex : Session.TargetIndices)
    {
        const FRemovalPieceTimeline* Timeline = Session.PieceTimelines.Find(TargetIndex);
        if (!Timeline || Timeline->FirstSleepingWorldTime < 0.0)
        {
            PendingTargetIndices.Add(TargetIndex);
        }
    }
    if (!PendingTargetIndices.IsEmpty())
    {
        if (Chaos::FPBDRigidsSolver* Solver = Proxy->GetSolver<Chaos::FPBDRigidsSolver>())
        {
            if (!Session.PTSleepAppliedParticleCount.IsValid())
            {
                Session.PTSleepAppliedParticleCount =
                    MakeShared<TAtomic<int32>, ESPMode::ThreadSafe>(0);
            }
            const TSharedPtr<TAtomic<int32>, ESPMode::ThreadSafe> AppliedCounter =
                Session.PTSleepAppliedParticleCount;
            ++Session.PTSleepCommandDispatchCount;
            Session.PTSleepRequestedParticleCount += PendingTargetIndices.Num();
            EnqueueSafe(Solver, Proxy,
                [Proxy, Solver, PendingTargetIndices = MoveTemp(PendingTargetIndices), AppliedCounter]()
                {
                    for (int32 TargetIndex : PendingTargetIndices)
                    {
                        if (FGeometryCollectionPhysicsProxy::FClusterHandle* Particle =
                            Proxy->GetParticle_Internal(TargetIndex))
                        {
                            if (!Particle->Disabled())
                            {
                                Particle->SetV(FVector::ZeroVector);
                                Particle->SetW(FVector::ZeroVector);
                                Solver->GetEvolution()->SetParticleObjectState(
                                    Particle,
                                    Chaos::EObjectStateType::Sleeping);
                                AppliedCounter->IncrementExchange();
                            }
                        }
                    }
                });
        }
    }
    return FMath::Max(SleepingParticleCount, EnabledTargetPhysicsObjects.Num());
}

bool CaptureActiveRemovalFrame()
{
    if(!Check()) return {};

    if (!ActiveRemovalSession || ActiveRemovalSession->bCaptureCompleted)
    {
        return false;
    }
    FRemovalLearningSession& Session = *ActiveRemovalSession;
    UWorld* World = Session.World.Get();
    UGeometryCollectionComponent* Component = Session.Component.Get();
    if (!World || !Component)
    {
        Session.bCaptureCompleted = true;
        Session.CompletionReason = TEXT("world_or_component_lost");
        Session.TickerHandle.Reset();
        return false;
    }
    const uint64 CurrentFrame = GFrameCounter;
    if (CurrentFrame <= Session.LastSampleFrame)
    {
        return true;
    }
    const double CurrentWorldTime = World->GetTimeSeconds();
    const FRemovalPieceSnapshot Current = CaptureRemovalPieceSnapshot(Component, Session, CurrentWorldTime, CurrentFrame);
    if (Session.Mode == TEXT("sleep_only"))
    {
        if (FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection())
        {
            FGeometryCollectionRemoveOnSleepDynamicFacade SleepRemovalFacade(*Dynamic);
            FGeometryCollectionDecayDynamicFacade DecayFacade(*Dynamic);
            if (SleepRemovalFacade.IsValid())
            {
                TSet<int32> TargetSet;
                for (int32 TargetIndex : Session.TargetIndices)
                {
                    TargetSet.Add(TargetIndex);
                }
                const UGeometryCollection* RestCollection = Component->GetRestCollection();
                const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest =
                    RestCollection ? RestCollection->GetGeometryCollection() : nullptr;
                if (Rest.IsValid())
                {
                    for (int32 TransformIndex = 0; TransformIndex < Rest->SimulationType.Num(); ++TransformIndex)
                    {
                        if (Rest->SimulationType[TransformIndex] == FGeometryCollection::ESimulationTypes::FST_Rigid &&
                            !TargetSet.Contains(TransformIndex))
                        {
                            {
                             const float BeforeRuntimeValue=SleepRemovalFacade.GetMaxSleepTime(TransformIndex);
                             if(!BeforeSDK(TEXT("SetMaxSleepTime"))) return {};
                             SleepRemovalFacade.SetMaxSleepTime(TransformIndex, 1000.0f);
                             RuntimeValueReturned(TEXT("SetMaxSleepTime"),TransformIndex,BeforeRuntimeValue,SleepRemovalFacade.GetMaxSleepTime(TransformIndex));
                             if(!AfterSDK()) return {};
                            }
                            {
                             const float BeforeRuntimeValue=SleepRemovalFacade.GetSleepRemovalDuration(TransformIndex);
                             if(!BeforeSDK(TEXT("SetSleepRemovalDuration"))) return {};
                             SleepRemovalFacade.SetSleepRemovalDuration(TransformIndex, 0.5f);
                             RuntimeValueReturned(TEXT("SetSleepRemovalDuration"),TransformIndex,BeforeRuntimeValue,SleepRemovalFacade.GetSleepRemovalDuration(TransformIndex));
                             if(!AfterSDK()) return {};
                            }
                            if (DecayFacade.IsValid())
                            {
                                {
                                 const float BeforeRuntimeValue=DecayFacade.GetDecay(TransformIndex);
                                 if(!BeforeSDK(TEXT("SetDecay"))) return {};
                                 DecayFacade.SetDecay(TransformIndex, 0.0f);
                                 RuntimeValueReturned(TEXT("SetDecay"),TransformIndex,BeforeRuntimeValue,DecayFacade.GetDecay(TransformIndex));
                                 if(!AfterSDK()) return {};
                                }
                            }
                        }
                    }
                }
            }
        }
        if (Session.SleepReleaseTargetDispatchCount == 0 &&
            Session.SleepReleaseRootDispatchCount > 0 &&
            CurrentFrame >= Session.SleepReleaseRootDispatchFrame + 2)
        {
            if (Session.SleepReleaseParentDispatchCount == 0)
            {
                if (FGeometryCollectionPhysicsProxy* Proxy = Component->GetPhysicsProxy())
                {
                    TSet<int32> UniqueParentItems;
                    for (int32 TargetIndex : Session.TargetIndices)
                    {
                        const FGeometryCollectionItemIndex ParentItem =
                            Proxy->GetInternalClusterParentItemIndex_External(TargetIndex);
                        if (ParentItem.IsValid() && ParentItem.IsInternalCluster() &&
                            ParentItem.GetItemIndex() != Session.SleepReleaseRootItem)
                        {
                            UniqueParentItems.Add(ParentItem.GetItemIndex());
                        }
                    }
                    Session.SleepReleaseParentItems = UniqueParentItems.Array();
                    Session.SleepReleaseParentItems.Sort();
                    for (int32 ParentItem : Session.SleepReleaseParentItems)
                    {
                        if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return {};
                        Component->ApplyExternalStrain(
                            ParentItem,
                            Component->Bounds.Origin,
                            1000.0f,
                            0,
                            0.0f,
                            1000000000000.0f);
                        if(!AfterSDK()) return {};
                        ++Session.SleepReleaseParentDispatchCount;
                    }
                    if (Session.SleepReleaseParentDispatchCount > 0)
                    {
                        Session.SleepReleaseParentDispatchFrame = CurrentFrame;
                    }
                }
            }
            const bool bParentStageSettled = Session.SleepReleaseParentDispatchCount == 0 ||
                CurrentFrame > Session.SleepReleaseParentDispatchFrame;
            if (bParentStageSettled)
            {
                for (int32 TargetIndex : Session.TargetIndices)
                {
                    if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return {};
                    Component->ApplyExternalStrain(
                        TargetIndex,
                        Component->Bounds.Origin,
                        50.0f,
                        0,
                        0.0f,
                        1000000000000.0f);
                    if(!AfterSDK()) return {};
                    ++Session.SleepReleaseTargetDispatchCount;
                }
                Session.SleepReleaseTargetDispatchFrame = CurrentFrame;
            }
        }
        const bool bAllTargetsWereReleased = Current.TargetCount > 0 &&
            Current.TargetBrokenCount == Current.TargetCount;
        if (bAllTargetsWereReleased)
        {
            const int32 ForcedSleepCount = ForceSleepEnabledRemovalTargets(Session);
            if (ForcedSleepCount > 0)
            {
                ++Session.ForcedSleepAttemptCount;
                Session.ForcedSleepObjectCount = FMath::Max(Session.ForcedSleepObjectCount, ForcedSleepCount);
                Session.LastForcedSleepFrame = CurrentFrame;
            }
        }
        else if (Session.SleepReleaseTargetDispatchCount > 0 &&
            Current.TargetBrokenCount < Current.TargetCount && Session.CapturedFrameCount % 15 == 0)
        {
            for (int32 TargetIndex : Session.TargetIndices)
            {
                if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return {};
                Component->ApplyExternalStrain(
                    TargetIndex,
                    Component->Bounds.Origin,
                    50.0f,
                    0,
                    0.0f,
                    1000000000000.0f);
                if(!AfterSDK()) return {};
                ++Session.SleepReleaseTargetReapplyCount;
            }
        }
    }
    const uint64 FrameDelta = CurrentFrame - Session.LastSampleFrame;
    TSharedPtr<FJsonObject> Sample = RemovalPieceSnapshotToJson(Current);
    Sample->SetNumberField(TEXT("frame"), static_cast<double>(CurrentFrame));
    Sample->SetNumberField(TEXT("frame_delta"), static_cast<double>(FrameDelta));
    Sample->SetNumberField(TEXT("world_time"), CurrentWorldTime);
    Sample->SetNumberField(TEXT("elapsed_seconds"), CurrentWorldTime - Session.BeginWorldTime);
    Session.Samples.Add(MakeShared<FJsonValueObject>(Sample));
    Session.Previous = Current;
    Session.LastSampleFrame = CurrentFrame;
    ++Session.CapturedFrameCount;
    if (Current.TargetGoneCount == Current.TargetCount && Current.TargetCount > 0)
    {
        Session.AllGoneWorldTime = CurrentWorldTime;
        Session.bCaptureCompleted = true;
        Session.CompletionReason = TEXT("all_target_pieces_removed");
        Session.TickerHandle.Reset();
        return false;
    }
    if (Session.CapturedFrameCount >= Session.CaptureLimitFrames)
    {
        Session.bCaptureCompleted = true;
        Session.CompletionReason = TEXT("bounded_frame_capture_timeout");
        Session.TickerHandle.Reset();
        return false;
    }
    return true;
}

TArray<TSharedPtr<FJsonValue>> RemovalPieceTimelinesToJson(
    const FRemovalLearningSession& Session,
    double FixedDeltaSeconds,
    bool& bOutAllTargetTimingPass)
{
    if(!Check()) return {};

    TArray<TSharedPtr<FJsonValue>> Result;
    bOutAllTargetTimingPass = true;
    for (int32 TargetIndex : Session.TargetIndices)
    {
        const FRemovalPieceTimeline* Timeline = Session.PieceTimelines.Find(TargetIndex);
        if (!Timeline)
        {
            bOutAllTargetTimingPass = false;
            continue;
        }
        const double ExpectedBaseTime = Timeline->FirstBrokenWorldTime;
        const double ObservedDelay = ExpectedBaseTime >= 0.0 && Timeline->FirstGoneWorldTime >= 0.0
            ? Timeline->FirstGoneWorldTime - ExpectedBaseTime
            : -1.0;
        const double MinimumDelay = FMath::Min(
            Timeline->ExpectedCompletionDelayRange.X, Timeline->ExpectedCompletionDelayRange.Y);
        const double MaximumDelay = FMath::Max(
            Timeline->ExpectedCompletionDelayRange.X, Timeline->ExpectedCompletionDelayRange.Y);
        const bool bTimingPass = ObservedDelay >= MinimumDelay - FixedDeltaSeconds - 1.e-4 &&
            ObservedDelay <= MaximumDelay + FixedDeltaSeconds + 1.e-4;
        bOutAllTargetTimingPass &= bTimingPass;
        TSharedPtr<FJsonObject> Piece = MakeShared<FJsonObject>();
        Piece->SetNumberField(TEXT("transform_index"), TargetIndex);
        Piece->SetBoolField(TEXT("ever_released"), Timeline->bEverReleased);
        Piece->SetNumberField(TEXT("first_broken_world_time"), Timeline->FirstBrokenWorldTime);
        Piece->SetNumberField(TEXT("first_sleeping_world_time"), Timeline->FirstSleepingWorldTime);
        Piece->SetNumberField(TEXT("first_gone_world_time"), Timeline->FirstGoneWorldTime);
        Piece->SetNumberField(TEXT("first_decay_start_world_time"), Timeline->FirstDecayStartWorldTime);
        Piece->SetStringField(TEXT("break_time_source"), Timeline->BreakTimeSource);
        Piece->SetStringField(TEXT("sleep_time_source"), Timeline->SleepTimeSource);
        Piece->SetStringField(TEXT("timing_base_source"), TEXT("first_broken_or_released_world_time"));
        Piece->SetNumberField(TEXT("timing_base_world_time"), ExpectedBaseTime);
        Piece->SetNumberField(TEXT("runtime_max_sleep_time_seconds"), Timeline->SleepTimerSeconds);
        Piece->SetNumberField(TEXT("runtime_sleep_removal_duration_seconds"),
            Timeline->SleepRemovalDurationSeconds);
        Piece->SetNumberField(TEXT("first_broken_frame"), static_cast<double>(Timeline->FirstBrokenFrame));
        Piece->SetNumberField(TEXT("first_sleeping_frame"), static_cast<double>(Timeline->FirstSleepingFrame));
        Piece->SetNumberField(TEXT("first_gone_frame"), static_cast<double>(Timeline->FirstGoneFrame));
        Piece->SetNumberField(TEXT("first_decay_start_frame"), static_cast<double>(Timeline->FirstDecayStartFrame));
        Piece->SetNumberField(TEXT("latest_decay"), Timeline->LatestDecay);
        Piece->SetNumberField(TEXT("expected_delay_min_seconds"), MinimumDelay);
        Piece->SetNumberField(TEXT("expected_delay_max_seconds"), MaximumDelay);
        Piece->SetNumberField(TEXT("observed_delay_seconds"), ObservedDelay);
        Piece->SetBoolField(TEXT("timing_within_one_fixed_tick"), bTimingPass);
        Result.Add(MakeShared<FJsonValueObject>(Piece));
    }
    return Result;
}

FString MakeTraceItemSnapshotKey(const UGeometryCollectionComponent* Component, int32 ItemIndex)
{
    return FString::Printf(TEXT("%s|%d"), Component ? *Component->GetPathName() : TEXT("none"), ItemIndex);
}

TArray<int32> ResolveTraceItemTransformIndices(UGeometryCollectionComponent* Component, int32 ItemIndex)
{
    if(!Check()) return {};

    TArray<int32> Result;
    if (!Component)
    {
        return Result;
    }
    const FGeometryCollectionItemIndex DecodedItem = FGeometryCollectionItemIndex::CreateFromExistingItemIndex(ItemIndex);
    if (!DecodedItem.IsValid())
    {
        return Result;
    }
    TArray<int32> CandidateIndices;
    if (!DecodedItem.IsInternalCluster())
    {
        CandidateIndices.Add(DecodedItem.GetTransformIndex());
    }
    else if (FGeometryCollectionPhysicsProxy* Proxy = Component->GetPhysicsProxy())
    {
        if (const TArray<int32>* Children = Proxy->FindInternalClusterChildrenTransformIndices_External(DecodedItem))
        {
            CandidateIndices = *Children;
        }
    }
    const UGeometryCollection* RestCollection = Component->GetRestCollection();
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest =
        RestCollection ? RestCollection->GetGeometryCollection() : nullptr;
    if (!Rest.IsValid())
    {
        return CandidateIndices;
    }
    TArray<int32> Pending = CandidateIndices;
    TSet<int32> Visited;
    while (!Pending.IsEmpty())
    {
        const int32 Index = Pending.Pop(EAllowShrinking::No);
        if (!Rest->SimulationType.IsValidIndex(Index) || Visited.Contains(Index))
        {
            continue;
        }
        Visited.Add(Index);
        if (Rest->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid)
        {
            Result.Add(Index);
            continue;
        }
        if (Rest->Children.IsValidIndex(Index))
        {
            for (int32 ChildIndex : Rest->Children[Index])
            {
                Pending.Add(ChildIndex);
            }
        }
    }
    Result.Sort();
    return Result;
}

void CaptureTraceItemDispatchSnapshot(
    UWorld* World,
    UGeometryCollectionComponent* Component,
    int32 ItemIndex,
    const FVector& RequestedVelocity)
{
    if(!Check()) return;

    for (auto It = TraceItemDispatchSnapshots.CreateIterator(); It; ++It)
    {
        if (!It.Value().World.IsValid() || It.Value().World.Get() != World || !It.Value().Component.IsValid())
        {
            if (It.Value().DeferredVelocityTickerHandle.IsValid())
            {
                FTSTicker::RemoveTicker(It.Value().DeferredVelocityTickerHandle);
            }
            It.RemoveCurrent();
        }
    }
    const FString SnapshotKey = MakeTraceItemSnapshotKey(Component, ItemIndex);
    if (FTraceItemDispatchSnapshot* PreviousSnapshot = TraceItemDispatchSnapshots.Find(SnapshotKey))
    {
        if (PreviousSnapshot->DeferredVelocityTickerHandle.IsValid())
        {
            FTSTicker::RemoveTicker(PreviousSnapshot->DeferredVelocityTickerHandle);
        }
    }
    FTraceItemDispatchSnapshot Snapshot;
    Snapshot.Evidence=CurrentWork;
    Snapshot.World = World;
    Snapshot.Component = Component;
    Snapshot.ItemIndex = ItemIndex;
    Snapshot.DispatchFrame = GFrameCounter;
    Snapshot.TransformIndices = ResolveTraceItemTransformIndices(Component, ItemIndex);
    Snapshot.RequestedVelocity = RequestedVelocity;
    TraceItemDispatchSnapshots.Add(SnapshotKey, MoveTemp(Snapshot));
}

int32 ApplyDirectVelocityToEnabledTransforms(
    UGeometryCollectionComponent* Component,
    const TArray<int32>& TransformIndices,
    const FVector& Velocity)
{
    if(!Check()) return {};

    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    if (!Proxy || Velocity.IsNearlyZero())
    {
        return 0;
    }
    TArray<Chaos::FPhysicsObjectHandle> CandidateHandles;
    TArray<int32> CandidateTransformIndices;
    for (int32 TransformIndex : TransformIndices)
    {
        if (Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(TransformIndex))
        {
            CandidateHandles.Add(PhysicsObject);
            CandidateTransformIndices.Add(TransformIndex);
        }
    }
    if (CandidateHandles.IsEmpty())
    {
        return 0;
    }
    TArray<int32> EnabledTransformIndices;
    {
        FLockedReadPhysicsObjectExternalInterface PhysicsObjectInterface =
            FPhysicsObjectExternalInterface::LockRead(CandidateHandles);
        for (int32 CandidateIndex = 0; CandidateIndex < CandidateHandles.Num(); ++CandidateIndex)
        {
            Chaos::FPhysicsObjectHandle PhysicsObject = CandidateHandles[CandidateIndex];
            if (!PhysicsObjectInterface->AreAllDisabled({&PhysicsObject, 1}))
            {
                EnabledTransformIndices.Add(CandidateTransformIndices[CandidateIndex]);
            }
        }
    }
    if (EnabledTransformIndices.IsEmpty())
    {
        return 0;
    }
    int32 AppliedCount = 0;
    for (int32 TransformIndex : EnabledTransformIndices)
    {
        if (FGeometryCollectionPhysicsProxy::FParticle* Particle = Proxy->GetParticleByIndex_External(TransformIndex))
        {
            if(!BeforeSDK(TEXT("SetV"))) return {};
            Particle->SetV(Velocity);
            if(!AfterSDK()) return {};
            ++AppliedCount;
        }
    }
    if (AppliedCount > 0)
    {
        if(!BeforeSDK(TEXT("SetProxyDirty_External"))) return {};
        Proxy->SetProxyDirty_External();
        if(!AfterSDK()) return {};
    }
    return AppliedCount;
}

FVector SampleEnabledTransformVelocity(
    UGeometryCollectionComponent* Component,
    const TArray<int32>& TransformIndices,
    int32& OutSampleCount)
{
    if(!Check()) return {};

    OutSampleCount = 0;
    FVector AverageVelocity = FVector::ZeroVector;
    FGeometryCollectionPhysicsProxy* Proxy = Component ? Component->GetPhysicsProxy() : nullptr;
    if (!Proxy)
    {
        return AverageVelocity;
    }
    TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects;
    for (int32 TransformIndex : TransformIndices)
    {
        if (Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(TransformIndex))
        {
            PhysicsObjects.Add(PhysicsObject);
        }
    }
    if (PhysicsObjects.IsEmpty())
    {
        return AverageVelocity;
    }
    FLockedReadPhysicsObjectExternalInterface PhysicsObjectInterface =
        FPhysicsObjectExternalInterface::LockRead(PhysicsObjects);
    for (Chaos::FPhysicsObjectHandle PhysicsObject : PhysicsObjects)
    {
        if (!PhysicsObjectInterface->AreAllDisabled({&PhysicsObject, 1}))
        {
            AverageVelocity += PhysicsObjectInterface->GetV(PhysicsObject);
            ++OutSampleCount;
        }
    }
    return OutSampleCount > 0 ? AverageVelocity / static_cast<double>(OutSampleCount) : FVector::ZeroVector;
}

void StartDeferredTraceVelocityDispatch(UGeometryCollectionComponent* Component, int32 ItemIndex)
{
    if(!Check()) return;

    const FString SnapshotKey = MakeTraceItemSnapshotKey(Component, ItemIndex);
    FTraceItemDispatchSnapshot* Snapshot = TraceItemDispatchSnapshots.Find(SnapshotKey);
    if (!Snapshot)
    {
        return;
    }
    if (Snapshot->DeferredVelocityTickerHandle.IsValid())
    {
        FTSTicker::RemoveTicker(Snapshot->DeferredVelocityTickerHandle);
    }
    Snapshot->DeferredVelocityTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([SnapshotKey](float)
        {
            FTraceItemDispatchSnapshot* ActiveSnapshot = TraceItemDispatchSnapshots.Find(SnapshotKey);
            if(!ActiveSnapshot||Running||ShuttingDown)return false;
            TGuardValue<bool>Busy(Running,true);TGuardValue<bool> WriterAdmission(ActiveSnapshot->Evidence->Writer,true);TGuardValue<bool> ErrorObservation(ActiveSnapshot->Evidence->AllowErrorObservation,false);FExecutionScope Scope(ActiveSnapshot->Evidence);
            if(!Scope.Valid||FPlatformTime::Seconds()>ActiveSnapshot->Evidence->Deadline){ActiveSnapshot->Evidence->Fail(TEXT("trace_deadline_or_lifetime"),TEXT("Deferred trace deadline or lifecycle lost"));ActiveSnapshot->bDeferredVelocityDispatchComplete=true;TerminateWork(ActiveSnapshot->Evidence,TEXT("deferred_trace_lifetime_or_timeout"),true);return false;}
            if (!ActiveSnapshot || !ActiveSnapshot->World.IsValid() || !ActiveSnapshot->Component.IsValid())
            {
                return false;
            }
            if (GFrameCounter <= ActiveSnapshot->DispatchFrame)
            {
                return true;
            }
            ++ActiveSnapshot->DeferredVelocityTickCount;
            int32 VelocitySampleCount = 0;
            const FVector ObservedVelocity = SampleEnabledTransformVelocity(
                ActiveSnapshot->Component.Get(), ActiveSnapshot->TransformIndices, VelocitySampleCount);
            const double DirectionDot = VelocitySampleCount > 0 && !ObservedVelocity.IsNearlyZero() &&
                !ActiveSnapshot->RequestedVelocity.IsNearlyZero()
                ? FVector::DotProduct(ObservedVelocity.GetSafeNormal(), ActiveSnapshot->RequestedVelocity.GetSafeNormal())
                : 0.0;
            if (DirectionDot > 0.5)
            {
                ActiveSnapshot->DeferredObservedVelocity = ObservedVelocity;
                ActiveSnapshot->bDeferredVelocityObserved = true;
                ActiveSnapshot->bDeferredVelocityDispatchComplete = true;
                ActiveSnapshot->DeferredVelocityTickerHandle.Reset();
                return false;
            }
            if (ActiveSnapshot->DeferredVelocityDispatchAttemptCount >= 4 ||
                ActiveSnapshot->DeferredVelocityTickCount >= 5)
            {
                ActiveSnapshot->bDeferredVelocityDispatchComplete = true;
                ActiveSnapshot->DeferredVelocityTickerHandle.Reset();
                return false;
            }
            const int32 AppliedCount = ApplyDirectVelocityToEnabledTransforms(
                ActiveSnapshot->Component.Get(),
                ActiveSnapshot->TransformIndices,
                ActiveSnapshot->RequestedVelocity);
            if (AppliedCount > 0)
            {
                ActiveSnapshot->DirectVelocityAppliedCount = AppliedCount;
                ActiveSnapshot->DeferredVelocityDispatchFrame = GFrameCounter;
                ++ActiveSnapshot->DeferredVelocityDispatchAttemptCount;
            }
            return true;
        }));
}

bool RestoreThrottleSettings(FThrottleLearningSession& Session)
{
    TGuardValue<bool> RestoreAdmission(Restoring,true);
    if(!Check())return false;
    for(const auto&Component:Session.RegisteredBreakComponents)UnregisterOwnedBreak(Component.Get());
    Session.RegisteredBreakComponents.Reset();Session.BreakEventDispatcher.Reset();
    if(Session.bSettingsRestored)return true;if(!Session.bOwnsTemporarySettings){Session.bSettingsRestored=true;return true;}auto*SolverActor=Session.SolverActor.Get();if(!Live(SolverActor))return false;
    FChaosSolverConfiguration Expected=Session.OriginalConfiguration;Expected.DestructionSettings=Session.AppliedSettings;
    if(!FChaosSolverConfiguration::StaticStruct()->CompareScriptStruct(&Expected,&SolverActor->Properties,0))
    {
        CurrentWork->R.Data->SetBoolField(TEXT("solver_restore_external_change_conflict"),true);
        CurrentWork->Fail(TEXT("solver_restore_conflict"),TEXT("Solver settings changed outside the owned session; original configuration was not written over them"));
        return false;
    }
    if(!BeforeObjectSDK(SolverActor,TEXT("restore_solver_configuration")))return false;
    SolverActor->Properties=Session.OriginalConfiguration;
    ApplySolverPropertyChange(SolverActor);
    if(!AfterSDK())return false;
    Session.bSettingsRestored=FChaosSolverConfiguration::StaticStruct()->CompareScriptStruct(&Session.OriginalConfiguration,&SolverActor->Properties,0);
    CurrentWork->R.Data->SetBoolField(TEXT("full_solver_configuration_restore_readback"),Session.bSettingsRestored);
    return Session.bSettingsRestored;
}

bool RestoreActiveThrottleSession()
{
    TGuardValue<bool> RestoreAdmission(Restoring,true);

    if(!Check()) return {};

    if (!ActiveThrottleSession)
    {
        return true;
    }
    if (ActiveThrottleSession->TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ActiveThrottleSession->TickerHandle);
        ActiveThrottleSession->TickerHandle.Reset();
    }
    const bool bRestored = RestoreThrottleSettings(*ActiveThrottleSession);
    if(bRestored)ActiveThrottleSession.Reset();
    else {ActiveThrottleSession->bCaptureCompleted=true;CurrentWork->Pending=true;CurrentWork->R.Data->SetBoolField(TEXT("unresolved_solver_restore_lease"),true);}
    return bRestored;
}

void CaptureThrottleState(
    UWorld* World,
    const TMap<FString, int32>& /*InitialRootTransformIndices*/,
    TMap<FString, TSet<int32>>& OutBrokenSets,
    TSet<FString>& OutTransitionedRoots,
    int32& OutActiveCount,
    int32& OutDisabledPhysicsObjectCount,
    int32& OutComponentCount)
{
    if(!Check()) return;

    OutBrokenSets.Reset();
    OutTransitionedRoots.Reset();
    OutActiveCount = 0;
    OutDisabledPhysicsObjectCount = 0;
    OutComponentCount = 0;
    if (!World)
    {
        return;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(LearningActorTag) || !It->GetName().StartsWith(TEXT("Chaos_Throttle_")))
        {
            continue;
        }
        UGeometryCollectionComponent* Component = It->FindComponentByClass<UGeometryCollectionComponent>();
        if (!Component)
        {
            continue;
        }
        ++OutComponentCount;
        const FString ActorName = It->GetName();
        TSet<int32>& BrokenSet = OutBrokenSets.Add(ActorName);
        if (FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection())
        {
            FGeometryCollectionDynamicStateFacade State(*Dynamic);
            const UGeometryCollection* RestCollection = Component->GetRestCollection();
            const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest =
                RestCollection ? RestCollection->GetGeometryCollection() : nullptr;
            for (int32 Index = 0; Index < Dynamic->Active.Num(); ++Index)
            {
                OutActiveCount += State.IsActive(Index) ? 1 : 0;
                if (State.HasBrokenOff(Index) && Rest.IsValid() && Rest->SimulationType.IsValidIndex(Index) &&
                    Rest->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid)
                {
                    BrokenSet.Add(Index);
                }
            }
        }
        const TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects = CheckedPhysicsObjects(Component);
        if (!PhysicsObjects.IsEmpty())
        {
            FLockedReadPhysicsObjectExternalInterface PhysicsObjectInterface =
                FPhysicsObjectExternalInterface::LockRead(PhysicsObjects);
            int32 EnabledPhysicsObjectCount = 0;
            for (Chaos::FPhysicsObjectHandle PhysicsObject : PhysicsObjects)
            {
                const bool bDisabled = PhysicsObjectInterface->AreAllDisabled({&PhysicsObject, 1});
                OutDisabledPhysicsObjectCount += bDisabled ? 1 : 0;
                EnabledPhysicsObjectCount += bDisabled ? 0 : 1;
            }
            if (EnabledPhysicsObjectCount > 1)
            {
                OutTransitionedRoots.Add(ActorName);
            }
        }
    }
}

bool CaptureActiveThrottleFrame()
{
    if(!Check()) return {};

    if (!ActiveThrottleSession || ActiveThrottleSession->bCaptureCompleted)
    {
        return false;
    }
    FThrottleLearningSession& Session = *ActiveThrottleSession;
    UWorld* World = Session.World.Get();
    if (!World || !Session.SolverActor.IsValid())
    {
        Session.CompletionReason = TEXT("world_or_solver_lost");
        Session.bCaptureCompleted = true;
        RestoreThrottleSettings(Session);
        Session.TickerHandle.Reset();
        return false;
    }
    const uint64 CurrentFrame = GFrameCounter;
    if (CurrentFrame <= Session.LastFrame)
    {
        return true;
    }
    TMap<FString, TSet<int32>> CurrentBrokenSets;
    TSet<FString> CurrentTransitionedRoots;
    int32 ActiveCount = 0;
    int32 DisabledPhysicsObjectCount = 0;
    int32 ComponentCount = 0;
    CaptureThrottleState(
        World,
        Session.InitialRootTransformIndices,
        CurrentBrokenSets,
        CurrentTransitionedRoots,
        ActiveCount,
        DisabledPhysicsObjectCount,
        ComponentCount);
    int32 DynamicNewReleasedParticleCount = 0;
    for (const TPair<FString, TSet<int32>>& Pair : CurrentBrokenSets)
    {
        const TSet<int32>* Previous = Session.PreviousBrokenSets.Find(Pair.Key);
        for (int32 BrokenIndex : Pair.Value)
        {
            DynamicNewReleasedParticleCount += !Previous || !Previous->Contains(BrokenIndex) ? 1 : 0;
        }
    }
    int32 NewClusterBreakCount = 0;
    for (const FString& ActorName : CurrentTransitionedRoots)
    {
        NewClusterBreakCount += Session.PreviousTransitionedRoots.Contains(ActorName) ? 0 : 1;
    }
    const int32 NewReleasedParticleCount = DynamicNewReleasedParticleCount;
    const uint64 FrameDelta = CurrentFrame - Session.LastFrame;
    Session.bAllCapturedFrameDeltasExact &= FrameDelta == 1;
    ++Session.CapturedFrameCount;
    Session.ClusterReleaseTransitionCount += NewClusterBreakCount > 0 ? 1 : 0;
    Session.ParticleReleaseTransitionCount += NewReleasedParticleCount > 0 ? 1 : 0;
    Session.TotalNewClusterBreaks += NewClusterBreakCount;
    Session.TotalNewReleasedParticles += NewReleasedParticleCount;
    Session.MaximumNewClusterBreaksPerFrame = FMath::Max(Session.MaximumNewClusterBreaksPerFrame, NewClusterBreakCount);
    Session.MaximumNewReleasedParticlesPerFrame = FMath::Max(Session.MaximumNewReleasedParticlesPerFrame, NewReleasedParticleCount);
    Session.bPerFrameLimitViolated |= NewClusterBreakCount > Session.AppliedSettings.PerAdvanceBreaksAllowed;
    TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
    Sample->SetNumberField(TEXT("frame"), static_cast<double>(CurrentFrame));
    Sample->SetNumberField(TEXT("frame_delta"), static_cast<double>(FrameDelta));
    Sample->SetNumberField(TEXT("new_cluster_break_count"), NewClusterBreakCount);
    Sample->SetNumberField(TEXT("new_released_particle_count"), NewReleasedParticleCount);
    Sample->SetNumberField(TEXT("dynamic_state_new_released_particle_count"), DynamicNewReleasedParticleCount);
    Sample->SetNumberField(TEXT("active_count"), ActiveCount);
    Sample->SetNumberField(TEXT("active_count_delta"), ActiveCount - Session.PreviousActiveCount);
    Sample->SetNumberField(TEXT("disabled_physics_object_count"), DisabledPhysicsObjectCount);
    Sample->SetNumberField(TEXT("disabled_physics_object_delta"), DisabledPhysicsObjectCount - Session.PreviousDisabledPhysicsObjectCount);
    Sample->SetNumberField(TEXT("transitioned_root_cluster_count"), CurrentTransitionedRoots.Num());
    Sample->SetBoolField(TEXT("exact_one_frame_delta"), FrameDelta == 1);
    Sample->SetBoolField(TEXT("per_frame_cluster_limit_pass"),
        NewClusterBreakCount <= Session.AppliedSettings.PerAdvanceBreaksAllowed);
    Session.Samples.Add(MakeShared<FJsonValueObject>(Sample));
    Session.PreviousBrokenSets = MoveTemp(CurrentBrokenSets);
    Session.PreviousTransitionedRoots = MoveTemp(CurrentTransitionedRoots);
    Session.PreviousActiveCount = ActiveCount;
    Session.PreviousDisabledPhysicsObjectCount = DisabledPhysicsObjectCount;
    Session.LastFrame = CurrentFrame;
    if (Session.ClusterReleaseTransitionCount == 0 && Session.CapturedFrameCount % 15 == 0)
    {
        for (int32 DispatchIndex = 0; DispatchIndex < Session.DispatchComponents.Num(); ++DispatchIndex)
        {
            UGeometryCollectionComponent* Component = Session.DispatchComponents[DispatchIndex].Get();
            if (Component && Session.DispatchItemIndices.IsValidIndex(DispatchIndex))
            {
                if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return {};
                Component->ApplyExternalStrain(
                    Session.DispatchItemIndices[DispatchIndex],
                    Component->Bounds.Origin,
                    1000.0f,
                    10,
                    1.0f,
                    Session.DispatchStrain);
                if(!AfterSDK()) return {};
                ++Session.ReapplyDispatchCount;
            }
        }
    }
    if (Session.CapturedFrameCount >= Session.CaptureLimitFrames)
    {
        Session.CompletionReason = TEXT("bounded_frame_capture_complete");
        Session.bCaptureCompleted = true;
        RestoreThrottleSettings(Session);
        Session.TickerHandle.Reset();
        return false;
    }
    return true;
}

TSharedPtr<FJsonObject> DestructionSettingsToJson(const FChaosSolverDestructionSettings& Settings)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("per_advance_breaks_allowed"), Settings.PerAdvanceBreaksAllowed);
    Data->SetNumberField(TEXT("per_advance_breaks_reschedule_limit"), Settings.PerAdvanceBreaksRescheduleLimit);
    Data->SetNumberField(TEXT("particle_release_throttling_min_count"), Settings.ClusteringParticleReleaseThrottlingMinCount);
    Data->SetNumberField(TEXT("particle_release_throttling_max_count"), Settings.ClusteringParticleReleaseThrottlingMaxCount);
    Data->SetBoolField(TEXT("optimize_for_runtime_memory"), Settings.bOptimizeForRuntimeMemory);
    return Data;
}

bool SameDestructionSettings(const FChaosSolverDestructionSettings& A, const FChaosSolverDestructionSettings& B)
{
    return A.PerAdvanceBreaksAllowed == B.PerAdvanceBreaksAllowed &&
        A.PerAdvanceBreaksRescheduleLimit == B.PerAdvanceBreaksRescheduleLimit &&
        A.ClusteringParticleReleaseThrottlingMinCount == B.ClusteringParticleReleaseThrottlingMinCount &&
        A.ClusteringParticleReleaseThrottlingMaxCount == B.ClusteringParticleReleaseThrottlingMaxCount &&
        A.bOptimizeForRuntimeMemory == B.bOptimizeForRuntimeMemory;
}

TSharedPtr<FJsonObject> RuntimeFrameEvidence(UGeometryCollectionComponent* Component)
{
    if(!Check()) return {};

    TSharedPtr<FJsonObject> Evidence = MakeShared<FJsonObject>();
    Evidence->SetNumberField(TEXT("frame_counter"), static_cast<double>(GFrameCounter));
    Evidence->SetBoolField(TEXT("component_found"), Component != nullptr);
    if (!Component || !Component->GetDynamicCollection())
    {
        Evidence->SetBoolField(TEXT("dynamic_collection_available"), false);
        return Evidence;
    }
    FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection();
    FGeometryCollectionDynamicStateFacade State(*Dynamic);
    int32 ActiveCount = 0;
    int32 ReleasedCount = 0;
    for (int32 Index = 0; Index < Dynamic->Active.Num(); ++Index)
    {
        ActiveCount += State.IsActive(Index) ? 1 : 0;
        ReleasedCount += State.HasBrokenOff(Index) ? 1 : 0;
    }
    Evidence->SetBoolField(TEXT("dynamic_collection_available"), true);
    Evidence->SetNumberField(TEXT("broken_off_cumulative_count"), ReleasedCount);
    Evidence->SetBoolField(TEXT("per_frame_release_delta_available"), false);
    Evidence->SetNumberField(TEXT("active_count"), ActiveCount);
    return Evidence;
}

TSharedPtr<FJsonObject> RuntimeFrameEvidenceForPrefix(UWorld* World, const FString& ActorPrefix)
{
    if(!Check()) return {};

    TSharedPtr<FJsonObject> Aggregate = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> PerActor;
    int32 ComponentCount = 0;
    int32 DynamicCollectionCount = 0;
    int32 ActiveCount = 0;
    int32 BrokenOffCumulativeCount = 0;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->Tags.Contains(LearningActorTag) || !It->GetName().StartsWith(ActorPrefix))
            {
                continue;
            }
            UGeometryCollectionComponent* Component = It->FindComponentByClass<UGeometryCollectionComponent>();
            TSharedPtr<FJsonObject> Entry = RuntimeFrameEvidence(Component);
            Entry->SetStringField(TEXT("actor_name"), It->GetName());
            ++ComponentCount;
            if (Entry->GetBoolField(TEXT("dynamic_collection_available")))
            {
                ++DynamicCollectionCount;
                ActiveCount += static_cast<int32>(Entry->GetNumberField(TEXT("active_count")));
                BrokenOffCumulativeCount += static_cast<int32>(Entry->GetNumberField(TEXT("broken_off_cumulative_count")));
            }
            PerActor.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }
    Aggregate->SetNumberField(TEXT("frame_counter"), static_cast<double>(GFrameCounter));
    Aggregate->SetNumberField(TEXT("component_count"), ComponentCount);
    Aggregate->SetNumberField(TEXT("dynamic_collection_count"), DynamicCollectionCount);
    Aggregate->SetBoolField(TEXT("dynamic_collection_available"), ComponentCount > 0 && DynamicCollectionCount == ComponentCount);
    Aggregate->SetNumberField(TEXT("active_count"), ActiveCount);
    Aggregate->SetNumberField(TEXT("broken_off_cumulative_count"), BrokenOffCumulativeCount);
    Aggregate->SetBoolField(TEXT("per_frame_release_delta_available"), false);
    Aggregate->SetArrayField(TEXT("per_actor"), PerActor);
    return Aggregate;
}

TSharedPtr<FJsonObject> Handle(const TSharedPtr<FJsonObject>& Params)
{
    if(!Check()) return LegacyError(CurrentWork->R.Error);

    const FString Operation = GetStringParam(Params, TEXT("operation"), TEXT("sample"));
    const bool bRequirePlay = GetBoolParam(Params, TEXT("require_play_session"), true);
    FString WorldContext;
    UWorld* World = ResolveWorld(true, WorldContext);
    if (!World || (bRequirePlay && WorldContext != TEXT("play")))
    {
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("blocker"), TEXT("An existing PIE/SIE session is required; this command never starts or stops shared editor play sessions"));
        Data->SetStringField(TEXT("world_context"), WorldContext);
        return Success(Data);
    }

    if(!ValidateNumericInputs(Params,Operation))return LegacyError(CurrentWork->R.Error);

    if (Operation == TEXT("performance_begin"))
    {
        ClearChaosPerformanceSession();
        const FString Scenario = GetStringParam(Params, TEXT("performance_scenario"), TEXT(""));
        FString ActorPrefix;
        if (Scenario == TEXT("tiny_base")) ActorPrefix = TEXT("Chaos_Base_");
        else if (Scenario == TEXT("tiny_optimized")) ActorPrefix = TEXT("Chaos_TinyOptimized_");
        else if (Scenario == TEXT("root_proxy")) ActorPrefix = TEXT("Chaos_RootProxy_");
        else if (Scenario == TEXT("cache_live") || Scenario == TEXT("cache_playback")) ActorPrefix = TEXT("Chaos_Cache_");
        else
        {
            return LegacyError(
                TEXT("performance_scenario must be tiny_base, tiny_optimized, root_proxy, cache_live, or cache_playback"));
        }
        CurrentWork->SDK(TEXT("create_FChaosPerformanceSession"),true);
        ActiveChaosPerformanceSession = MakeUnique<FChaosPerformanceSession>();
        ActiveChaosPerformanceSession->Evidence=CurrentWork;
        FChaosPerformanceSession& Session = *ActiveChaosPerformanceSession;
        Session.World = World;
        Session.Scenario = Scenario;
        Session.ActorPrefix = ActorPrefix;
        Session.ActorName = GetStringParam(Params, TEXT("actor_name"), TEXT(""));
        Session.ExpectedComponentCount = CastI32(GetNumberParam(
            Params,
            TEXT("expected_component_count"),
            Scenario.StartsWith(TEXT("cache_")) ? 16 : 32));
        Session.CaptureLimitFrames = FMath::Clamp(
            CastI32(GetNumberParam(Params, TEXT("capture_limit_frames"), 90)), 30, 600);
        const bool bPrepareLiveComponent = Scenario == TEXT("cache_live") &&
            GetBoolParam(Params, TEXT("prepare_live_component"), false);
        Session.BeginFrame = GFrameCounter;
        Session.LastFrame = GFrameCounter;
        Session.LastCaptureWallSeconds = FPlatformTime::Seconds();
        Session.LastAppCurrentSeconds = FApp::GetCurrentTime();
        const double DispatchStart = FPlatformTime::Seconds();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!MatchesChaosPerformanceActor(Session, *It))
            {
                continue;
            }
            if (UGeometryCollectionComponent* Component = It->FindComponentByClass<UGeometryCollectionComponent>())
            {
                if (bPrepareLiveComponent)
                {
                    if(!BeforeObjectSDK(Component,TEXT("assign_ObjectType"))) return LegacyError(CurrentWork->R.Error);
                    Component->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
                    if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                    if(!BeforeObjectSDK(Component,TEXT("SetSimulatePhysics"))) return LegacyError(CurrentWork->R.Error);
                    DropPhysics(Component);
                    Component->SetSimulatePhysics(true);
                    RefreshPhysics(Component);
                    if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                    if(!BeforeObjectSDK(Component,TEXT("RecreatePhysicsState"))) return LegacyError(CurrentWork->R.Error);
                    DropPhysics(Component);
                    Component->RecreatePhysicsState();
                    RefreshPhysics(Component);
                    if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                    ++Session.LiveComponentPreparedCount;
                    const int32 RootItem = ResolveActiveClusterItemIndex(Component);
                    if (RootItem != INDEX_NONE)
                    {
                        if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return LegacyError(CurrentWork->R.Error);
                        Component->ApplyExternalStrain(
                            RootItem,
                            Component->Bounds.Origin,
                            2000.0f,
                            10,
                            1.0f,
                            CastFloat(GetNumberParam(
                                Params, TEXT("live_external_strain"), 1000000000.0)));
                        if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                        ++Session.LiveExternalStrainDispatchCount;
                    }
                }
                if (Scenario == TEXT("tiny_base") || Scenario == TEXT("tiny_optimized"))
                {
                    const int32 RootItem = ResolveActiveClusterItemIndex(Component);
                    if (RootItem != INDEX_NONE)
                    {
                        if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return LegacyError(CurrentWork->R.Error);
                        Component->ApplyExternalStrain(
                            RootItem, Component->Bounds.Origin, 1000.0f, 10, 1.0f, 1000000.0f);
                        if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                    }
                }
                else if (Scenario == TEXT("root_proxy"))
                {
                    if(!BeforeObjectSDK(Component,TEXT("RefreshCustomRenderer"))) return LegacyError(CurrentWork->R.Error);
                    Component->RefreshCustomRenderer();
                    if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                }
            }
        }
        Session.ClusterDispatchMilliseconds =
            (FPlatformTime::Seconds() - DispatchStart) * 1000.0;
        Session.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([](float)
            {
                return ActiveChaosPerformanceSession ? TickOwned(ActiveChaosPerformanceSession->Evidence,[]{return CaptureChaosPerformanceFrame();}):false;
            }));
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("performance_scenario"), Scenario);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Native per-frame performance capture is active; call performance_finish after completion"));
        Data->SetNumberField(TEXT("capture_limit_frames"), Session.CaptureLimitFrames);
        Data->SetStringField(TEXT("actor_name"), Session.ActorName);
        Data->SetNumberField(TEXT("expected_component_count"), Session.ExpectedComponentCount);
        Data->SetNumberField(TEXT("live_component_prepared_count"), Session.LiveComponentPreparedCount);
        Data->SetNumberField(TEXT("live_external_strain_dispatch_count"), Session.LiveExternalStrainDispatchCount);
        Data->SetNumberField(TEXT("cluster_dispatch_ms"), Session.ClusterDispatchMilliseconds);
        Data->SetBoolField(TEXT("advance_clustering_stat_available"), false);
        Data->SetStringField(TEXT("advance_clustering_stat_unavailable_reason"),
            TEXT("The host does not expose independently verified per-cycle STAT_AdvanceClustering values; cluster_dispatch_ms is synchronous command-enqueue wall time. Frame availability and unavailable thread/GPU/draw metrics are reported separately in measurement_availability"));
        Data->SetObjectField(TEXT("measurement_availability"), ChaosPerformanceMeasurementAvailability(Session));
        Data->SetArrayField(TEXT("raw_measurement_snapshots"), Session.RawMeasurementSnapshots);
        CurrentWork->R.Data->SetObjectField(TEXT("measurement_availability"), ChaosPerformanceMeasurementAvailability(Session));
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        return Success(Data);
    }

    if (Operation == TEXT("performance_sample") || Operation == TEXT("performance_finish"))
    {
        if (!ActiveChaosPerformanceSession || ActiveChaosPerformanceSession->World.Get() != World)
        {
            return LegacyError(TEXT("No active Chaos performance session exists"));
        }
        FChaosPerformanceSession& Session = *ActiveChaosPerformanceSession;
        const bool bFinish = Operation == TEXT("performance_finish");
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("performance_scenario"), Session.Scenario);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetObjectField(TEXT("measurement_availability"), ChaosPerformanceMeasurementAvailability(Session));
        Data->SetArrayField(TEXT("raw_measurement_snapshots"), Session.RawMeasurementSnapshots);
        CurrentWork->R.Data->SetObjectField(TEXT("measurement_availability"), ChaosPerformanceMeasurementAvailability(Session));
        CurrentWork->R.Data->SetArrayField(TEXT("raw_measurement_snapshots"), Session.RawMeasurementSnapshots);
        Data->SetNumberField(TEXT("captured_frame_count"), Session.CapturedFrameCount);
        Data->SetNumberField(TEXT("capture_limit_frames"), Session.CaptureLimitFrames);
        Data->SetStringField(TEXT("actor_name"), Session.ActorName);
        Data->SetNumberField(TEXT("expected_component_count"), Session.ExpectedComponentCount);
        Data->SetBoolField(TEXT("capture_completed"), Session.bCaptureCompleted);
        Data->SetNumberField(TEXT("component_count"), Session.ComponentCount);
        Data->SetNumberField(TEXT("peak_active_body_count"), Session.PeakActivePhysicsObjectCount);
        Data->SetNumberField(TEXT("peak_disabled_body_count"), Session.PeakDisabledPhysicsObjectCount);
        Data->SetNumberField(TEXT("peak_dynamic_body_count"), Session.PeakDynamicPhysicsObjectCount);
        Data->SetNumberField(TEXT("peak_sleeping_body_count"), Session.PeakSleepingPhysicsObjectCount);
        Data->SetNumberField(TEXT("peak_kinematic_body_count"), Session.PeakKinematicPhysicsObjectCount);
        Data->SetNumberField(TEXT("peak_static_body_count"), Session.PeakStaticPhysicsObjectCount);
        Data->SetNumberField(TEXT("component_simulating_physics_count"), Session.ComponentSimulatingPhysicsCount);
        Data->SetNumberField(TEXT("component_kinematic_object_type_count"), Session.ComponentKinematicObjectTypeCount);
        Data->SetNumberField(TEXT("live_component_prepared_count"), Session.LiveComponentPreparedCount);
        Data->SetNumberField(TEXT("live_external_strain_dispatch_count"), Session.LiveExternalStrainDispatchCount);
        Data->SetNumberField(TEXT("cluster_dispatch_ms"), Session.ClusterDispatchMilliseconds);
        Data->SetBoolField(TEXT("advance_clustering_stat_available"), false);
        Data->SetStringField(TEXT("advance_clustering_stat_unavailable_reason"),
            TEXT("The host does not expose independently verified per-cycle STAT_AdvanceClustering values; cluster_dispatch_ms is synchronous command-enqueue wall time. Frame availability and unavailable thread/GPU/draw metrics are reported separately in measurement_availability"));
        Data->SetNumberField(TEXT("frame_median_ms"), Percentile(Session.FrameMilliseconds, 0.5));
        Data->SetNumberField(TEXT("frame_p95_ms"), Percentile(Session.FrameMilliseconds, 0.95));
        Data->SetNumberField(TEXT("wall_tick_delta_median_ms"), Percentile(Session.WallTickDeltaMilliseconds, 0.5));
        Data->SetNumberField(TEXT("wall_tick_delta_p95_ms"), Percentile(Session.WallTickDeltaMilliseconds, 0.95));
        Data->SetNumberField(TEXT("game_thread_median_ms"), Percentile(Session.GameThreadMilliseconds, 0.5));
        Data->SetNumberField(TEXT("game_thread_p95_ms"), Percentile(Session.GameThreadMilliseconds, 0.95));
        Data->SetNumberField(TEXT("render_thread_median_ms"), Percentile(Session.RenderThreadMilliseconds, 0.5));
        Data->SetNumberField(TEXT("render_thread_p95_ms"), Percentile(Session.RenderThreadMilliseconds, 0.95));
        Data->SetNumberField(TEXT("gpu_median_ms"), Percentile(Session.GPUMilliseconds, 0.5));
        Data->SetNumberField(TEXT("gpu_p95_ms"), Percentile(Session.GPUMilliseconds, 0.95));
        Data->SetNumberField(TEXT("draw_calls_median"), Percentile(Session.DrawCalls, 0.5));
        Data->SetNumberField(TEXT("draw_calls_p95"), Percentile(Session.DrawCalls, 0.95));
        Data->SetArrayField(TEXT("frame_samples_ms"), DoublesToJson(Session.FrameMilliseconds));
        Data->SetArrayField(TEXT("wall_tick_delta_samples_ms"), DoublesToJson(Session.WallTickDeltaMilliseconds));
        Data->SetArrayField(TEXT("game_thread_samples_ms"), DoublesToJson(Session.GameThreadMilliseconds));
        Data->SetArrayField(TEXT("render_thread_samples_ms"), DoublesToJson(Session.RenderThreadMilliseconds));
        Data->SetArrayField(TEXT("gpu_samples_ms"), DoublesToJson(Session.GPUMilliseconds));
        Data->SetArrayField(TEXT("draw_call_samples"), DoublesToJson(Session.DrawCalls));
        Data->SetArrayField(TEXT("active_body_samples"), IntsToJson(Session.ActivePhysicsObjectCounts));
        Data->SetArrayField(TEXT("disabled_body_samples"), IntsToJson(Session.DisabledPhysicsObjectCounts));
        Data->SetArrayField(TEXT("dynamic_body_samples"), IntsToJson(Session.DynamicPhysicsObjectCounts));
        Data->SetArrayField(TEXT("sleeping_body_samples"), IntsToJson(Session.SleepingPhysicsObjectCounts));
        Data->SetArrayField(TEXT("kinematic_body_samples"), IntsToJson(Session.KinematicPhysicsObjectCounts));
        Data->SetArrayField(TEXT("static_body_samples"), IntsToJson(Session.StaticPhysicsObjectCounts));
        Data->SetNumberField(TEXT("chaos_simulation_proxy_median_ms"), Percentile(Session.GameThreadMilliseconds, 0.5));
        Data->SetNumberField(TEXT("chaos_simulation_proxy_p95_ms"), Percentile(Session.GameThreadMilliseconds, 0.95));
        Data->SetNumberField(TEXT("initial_custom_renderer_interface_count"), Session.InitialCustomRendererCount);
        Data->SetNumberField(TEXT("latest_custom_renderer_interface_count"), Session.LatestCustomRendererCount);
        Data->SetBoolField(TEXT("double_render_detected"), Session.bDoubleRenderDetected);
        if (!bFinish || !Session.bCaptureCompleted)
        {
            Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_blocker"), TEXT("Performance capture has not completed"));
            Data->SetBoolField(TEXT("requires_followup_sample"), true);
            return Success(Data);
        }
        const bool bScenarioComplete = Session.ComponentCount == Session.ExpectedComponentCount;
        Data->SetStringField(TEXT("assertion_status"), bScenarioComplete ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("One performance run completed; five fresh-PIE aggregate comparison remains"));
        ActiveChaosPerformanceSession.Reset();
        return Success(Data);
    }

    if (Operation == TEXT("one_way_configure") || Operation == TEXT("one_way_run") || Operation == TEXT("one_way_sample"))
    {
        const FString ActorName = GetStringParam(Params, TEXT("actor_name"), TEXT(""));
        UGeometryCollectionComponent* OneWayComponent = FindGeometryCollectionComponent(World, ActorName);
        AStaticMeshActor* WitnessActor = FindActor<AStaticMeshActor>(World, GetStringParam(Params, TEXT("witness_actor_name"), TEXT("")));
        AStaticMeshActor* FloorActor = FindActor<AStaticMeshActor>(World, GetStringParam(Params, TEXT("floor_actor_name"), TEXT("Chaos_StaticFloor")));
        if (!OneWayComponent || !OneWayComponent->GetOwner()->Tags.Contains(LearningActorTag) ||
            !WitnessActor || !WitnessActor->Tags.Contains(LearningActorTag) ||
            !FloorActor || !FloorActor->Tags.Contains(LearningActorTag))
        {
            return LegacyError(TEXT("One-Way operations require tagged learning component, witness, and static floor actors"));
        }
        UStaticMeshComponent* Witness = WitnessActor->GetStaticMeshComponent();
        UStaticMeshComponent* Floor = FloorActor->GetStaticMeshComponent();
        if(!CurrentWork->Track(Witness)||!CurrentWork->Track(Floor)||!CurrentWork->CapturePhysics(Witness)||!CurrentWork->CapturePhysics(OneWayComponent))return LegacyError(CurrentWork->R.Error);
        const int32 RequestedLevel = CastI32(GetNumberParam(Params, TEXT("one_way_level"), OneWayComponent->OneWayInteractionLevel));
        if (RequestedLevel != -1 && RequestedLevel != 2)
        {
            return LegacyError(TEXT("one_way_level must be -1 or 2 for the learning A/B fixture"));
        }
        if (Operation == TEXT("one_way_configure"))
        {
            ClearOneWayCollisionCapture();
            ClearOneWayStagedLaunch();
            if (OneWayComponent->ChaosSolverActor)
            {
                // The dynamic witness body is registered with the world's default solver. One-Way
                // interaction can only be measured when the Geometry Collection is in that solver too.
                const UGeometryCollection* RestCollection = OneWayComponent->GetRestCollection();
                if(!BeforeObjectSDK(OneWayComponent,TEXT("assign_ChaosSolverActor"))) return LegacyError(CurrentWork->R.Error);
                OneWayComponent->ChaosSolverActor = nullptr;
                if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                if(!BeforeObjectSDK(OneWayComponent,TEXT("SetRestCollection"))) return LegacyError(CurrentWork->R.Error);
                DropPhysics(OneWayComponent);
                OneWayComponent->SetRestCollection(RestCollection, false);
                RefreshPhysics(OneWayComponent);
                if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            }
            if(!BeforeObjectSDK(OneWayComponent,TEXT("SetOneWayInteractionLevel"))) return LegacyError(CurrentWork->R.Error);
            DropPhysics(OneWayComponent);
            OneWayComponent->SetOneWayInteractionLevel(RequestedLevel);
            RefreshPhysics(OneWayComponent);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            FVector ResetWitnessLocation(
                OneWayComponent->Bounds.Origin.X + OneWayComponent->Bounds.BoxExtent.X + 55.0,
                WitnessActor->GetName().EndsWith(TEXT("_0")) ? -250.0 : 250.0,
                OneWayComponent->Bounds.Origin.Z);
            TryGetVectorParam(Params, TEXT("reset_witness_location"), ResetWitnessLocation);
            if(!BeforeObjectSDK(Witness,TEXT("SetWorldLocation"))) return LegacyError(CurrentWork->R.Error);
            Witness->SetWorldLocation(ResetWitnessLocation, false, nullptr, ETeleportType::TeleportPhysics);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            if(!BeforeObjectSDK(Witness,TEXT("SetEnableGravity"))) return LegacyError(CurrentWork->R.Error);
            Witness->SetEnableGravity(false);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            if(!BeforeObjectSDK(Witness,TEXT("SetMassOverrideInKg"))) return LegacyError(CurrentWork->R.Error);
            Witness->SetMassOverrideInKg(NAME_None, 50.0f, true);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            if(!BeforeObjectSDK(OneWayComponent,TEXT("SetUseCCD"))) return LegacyError(CurrentWork->R.Error);
            OneWayComponent->SetUseCCD(true);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            if(!BeforeObjectSDK(Witness,TEXT("SetUseCCD"))) return LegacyError(CurrentWork->R.Error);
            Witness->SetUseCCD(true);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            if(!BeforeObjectSDK(Witness,TEXT("SetPhysicsLinearVelocity"))) return LegacyError(CurrentWork->R.Error);
            Witness->SetPhysicsLinearVelocity(FVector::ZeroVector);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            if(!BeforeObjectSDK(Witness,TEXT("SetPhysicsAngularVelocityInDegrees"))) return LegacyError(CurrentWork->R.Error);
            Witness->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
        }
        const FVector InitialWitnessLocation = Witness->GetComponentLocation();
        const FVector InitialWitnessVelocity = Witness->GetPhysicsLinearVelocity();
        bool bDebrisDispatched = false;
        int32 DebrisItemIndex = INDEX_NONE;
        int32 DebrisBreakParentItemIndex = INDEX_NONE;
        int32 DebrisBreakParentChildCount = 0;
        FVector DebrisVelocity = FVector::ZeroVector;
        FTargetTraceDiagnostics OneWayTraceDiagnostics;
        if (Operation == TEXT("one_way_run"))
        {
            FVector TraceStart = OneWayComponent->Bounds.Origin - FVector(OneWayComponent->Bounds.BoxExtent.X + 200.0, 0, 0);
            FVector TraceEnd = OneWayComponent->Bounds.Origin;
            TryGetVectorParam(Params, TEXT("start"), TraceStart);
            TryGetVectorParam(Params, TEXT("end"), TraceEnd);
            FHitResult Hit;
            const bool bHit = TraceTargetGeometryCollection(
                World,
                OneWayComponent,
                TraceStart,
                TraceEnd,
                true,
                {WitnessActor, FloorActor},
                Hit,
                OneWayTraceDiagnostics);
            if (!bHit)
            {
                return LegacyError(FString::Printf(
                    TEXT("One-Way debris trace did not resolve a valid target item (blocking=%d target=%d valid_item=%d candidates=%s)"),
                    OneWayTraceDiagnostics.BlockingHitCount,
                    OneWayTraceDiagnostics.TargetComponentHitCount,
                    OneWayTraceDiagnostics.ValidItemHitCount,
                    *FString::Join(OneWayTraceDiagnostics.Candidates, TEXT("; "))));
            }
            DebrisItemIndex = Hit.Item;
            if (!TryGetVectorParam(Params, TEXT("breaking_velocity_vector"), DebrisVelocity))
            {
                const FVector HorizontalDirection = FVector(
                    Witness->GetComponentLocation().X - Hit.ImpactPoint.X,
                    Witness->GetComponentLocation().Y - Hit.ImpactPoint.Y,
                    0.0).GetSafeNormal();
                DebrisVelocity = HorizontalDirection * GetNumberParam(Params, TEXT("breaking_velocity"), 600.0);
                DebrisVelocity.Z = GetNumberParam(Params, TEXT("ballistic_lift_velocity"), 300.0);
            }
            const FGeometryCollectionItemIndex HitItem =
                FGeometryCollectionItemIndex::CreateFromExistingItemIndex(Hit.Item);
            FGeometryCollectionItemIndex BreakParentItem = HitItem;
            if (HitItem.IsValid() && !HitItem.IsInternalCluster())
            {
                if (FGeometryCollectionPhysicsProxy* Proxy = OneWayComponent->GetPhysicsProxy())
                {
                    const FGeometryCollectionItemIndex ActiveParent =
                        Proxy->GetInternalClusterParentItemIndex_External(HitItem.GetTransformIndex());
                    if (ActiveParent.IsValid() && ActiveParent.IsInternalCluster())
                    {
                        BreakParentItem = ActiveParent;
                        if (const TArray<int32>* Children =
                            Proxy->FindInternalClusterChildrenTransformIndices_External(ActiveParent))
                        {
                            DebrisBreakParentChildCount = Children->Num();
                        }
                    }
                }
            }
            DebrisBreakParentItemIndex = BreakParentItem.GetItemIndex();
            if(!BeforeObjectSDK(OneWayComponent,TEXT("ApplyExternalStrain"))) return LegacyError(CurrentWork->R.Error);
            OneWayComponent->ApplyExternalStrain(
                DebrisBreakParentItemIndex,
                Hit.ImpactPoint,
                1000.0f,
                10,
                1.0f,
                CastFloat(GetNumberParam(Params, TEXT("strain"), 1000000000000.0)));
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            ClearOneWayStagedLaunch();
            CurrentWork->SDK(TEXT("create_FOneWayStagedLaunchSession"),true);
        ActiveOneWayStagedLaunch = MakeUnique<FOneWayStagedLaunchSession>();
        ActiveOneWayStagedLaunch->Evidence=CurrentWork;
            ActiveOneWayStagedLaunch->World = World;
            ActiveOneWayStagedLaunch->Component = OneWayComponent;
            ActiveOneWayStagedLaunch->Witness = Witness;
            ActiveOneWayStagedLaunch->Floor = Floor;
            ActiveOneWayStagedLaunch->CandidateRigidIndices =
                ResolveTraceItemTransformIndices(OneWayComponent, Hit.Item);
            ActiveOneWayStagedLaunch->RequestedVelocity = DebrisVelocity;
            ActiveOneWayStagedLaunch->InitialBreakItem = DebrisBreakParentItemIndex;
            ActiveOneWayStagedLaunch->BeginFrame = GFrameCounter;
            ActiveOneWayStagedLaunch->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([](float)
                {
                    return ActiveOneWayStagedLaunch ? TickOwned(ActiveOneWayStagedLaunch->Evidence,[]{return AdvanceOneWayStagedLaunch();}):false;
                }));
            bDebrisDispatched = true;
        }
        FVector ExpectedDirection = FVector::ZeroVector;
        if (!TryGetVectorParam(Params, TEXT("expected_direction"), ExpectedDirection))
        {
            ExpectedDirection = (Witness->GetComponentLocation() - OneWayComponent->Bounds.Origin).GetSafeNormal();
        }
        const FVector ActualVelocity = Witness->GetPhysicsLinearVelocity();
        FVector BaselineLocation = InitialWitnessLocation;
        FVector BaselineVelocity = InitialWitnessVelocity;
        TryGetVectorParam(Params, TEXT("initial_witness_location"), BaselineLocation);
        TryGetVectorParam(Params, TEXT("initial_witness_velocity"), BaselineVelocity);
        if (Operation == TEXT("one_way_sample") && ActiveOneWayStagedLaunch &&
            ActiveOneWayStagedLaunch->World.Get() == World &&
            ActiveOneWayStagedLaunch->Component.Get() == OneWayComponent &&
            ActiveOneWayStagedLaunch->bLaunchCompleted)
        {
            BaselineLocation = ActiveOneWayStagedLaunch->AlignedWitnessLocation;
            BaselineVelocity = ActiveOneWayStagedLaunch->AlignedWitnessVelocity;
        }
        const FVector WitnessLocationDelta = Witness->GetComponentLocation() - BaselineLocation;
        const FVector WitnessVelocityDelta = ActualVelocity - BaselineVelocity;
        const FVector WitnessImpulseEstimate = WitnessVelocityDelta * Witness->GetMass();
        const FVector ExpectedDirectionNormalized = ExpectedDirection.GetSafeNormal();
        const double ProjectedLocationDelta = FVector::DotProduct(WitnessLocationDelta, ExpectedDirectionNormalized);
        const double ProjectedVelocityDelta = FVector::DotProduct(WitnessVelocityDelta, ExpectedDirectionNormalized);
        const double ProjectedImpulseEstimate = FVector::DotProduct(WitnessImpulseEstimate, ExpectedDirectionNormalized);
        const double OrthogonalVelocityDelta = (WitnessVelocityDelta - ExpectedDirectionNormalized * ProjectedVelocityDelta).Length();
        int32 FollowupDirectVelocityAppliedCount = 0;
        int32 DeferredVelocityDispatchAttemptCount = 0;
        uint64 DeferredVelocityDispatchFrame = 0;
        bool bDeferredVelocityObserved = false;
        bool bDeferredVelocityDispatchComplete = false;
        int32 StagedLaunchTickCount = 0;
        int32 StagedTargetRigidIndex = INDEX_NONE;
        int32 StagedTargetParentItem = INDEX_NONE;
        int32 StagedParentReleaseDispatchCount = 0;
        uint64 StagedLaunchFrame = 0;
        FVector StagedAlignedWitnessLocation = FVector::ZeroVector;
        FVector StagedAlignedWitnessVelocity = FVector::ZeroVector;
        FVector StagedInitialTargetLocation = FVector::ZeroVector;
        double StagedInitialWitnessSeparation = 0.0;
        double StagedCalibrationWitnessGap = 0.0;
        double StagedHorizontalFlightGap = 0.0;
        double StagedPredictedFlightTimeSeconds = 0.0;
        double StagedGravityZ = 0.0;
        double StagedLaunchDirectionDot = 0.0;
        FVector StagedLaunchTargetToWitnessDirection = FVector::ZeroVector;
        int32 StagedCollisionIsolatedSiblingCount = 0;
        int32 StagedTargetCollisionConfiguredCount = 0;
        uint64 StagedTargetCollisionConfiguredFrame = 0;
        uint64 StagedWitnessAlignmentFrame = 0;
        bool bStagedWitnessAlignmentCompleted = false;
        uint64 StagedVelocityDispatchFrame = 0;
        double StagedObservedTargetMotionProjection = 0.0;
        int32 StagedPTVelocityDispatchCount = 0;
        int32 StagedPTVelocityAppliedCount = 0;
        bool bStagedVelocityDispatchCompleted = false;
        bool bStagedTargetMotionObserved = false;
        uint64 StagedContactWitnessAlignmentFrame = 0;
        bool bStagedContactWitnessAlignmentCompleted = false;
        uint64 StagedContactVelocityDispatchFrame = 0;
        int32 StagedContactPTVelocityDispatchCount = 0;
        int32 StagedContactPTVelocityAppliedCount = 0;
        int32 StagedContactPTOneWayFlag = -1;
        int32 StagedContactPTObjectState = -1;
        int32 StagedPTContactDiagnosticSampleCount = 0;
        int32 StagedPTContactSameSolver = -1;
        int32 StagedPTTargetObjectState = -1;
        int32 StagedPTWitnessObjectState = -1;
        int32 StagedPTTargetCollisionGroup = MAX_int32;
        int32 StagedPTWitnessCollisionGroup = MAX_int32;
        int32 StagedPTTargetShapeCount = 0;
        int32 StagedPTTargetSimShapeCount = 0;
        int32 StagedPTWitnessShapeCount = 0;
        int32 StagedPTWitnessSimShapeCount = 0;
        int32 StagedPTPairConstraintCount = 0;
        int32 StagedPTCurrentPairConstraintCount = 0;
        int32 StagedPTEnabledPairConstraintCount = 0;
        int32 StagedPTOneWayPairConstraintCount = 0;
        int32 StagedPTPairManifoldPointCount = 0;
        double StagedPTTargetMassKg = -1.0;
        double StagedPTWitnessMassKg = -1.0;
        double StagedPTWitnessPoseErrorCm = -1.0;
        bool bStagedPTPairPhiObserved = false;
        double StagedPTMinimumPairPhiCm = -1.0;
        double StagedPTMaximumPairImpulseNs = 0.0;
        bool bStagedSiblingCollisionRestored = false;
        bool bStagedLaunchCompleted = false;
        FString StagedLaunchFailure;
        const bool bMatchingStagedLaunch = ActiveOneWayStagedLaunch &&
            ActiveOneWayStagedLaunch->World.Get() == World &&
            ActiveOneWayStagedLaunch->Component.Get() == OneWayComponent;
        if (bMatchingStagedLaunch)
        {
            StagedLaunchTickCount = ActiveOneWayStagedLaunch->TickCount;
            StagedTargetRigidIndex = ActiveOneWayStagedLaunch->TargetRigidIndex;
            StagedTargetParentItem = ActiveOneWayStagedLaunch->TargetParentItem;
            StagedParentReleaseDispatchCount = ActiveOneWayStagedLaunch->ParentReleaseDispatchCount;
            StagedLaunchFrame = ActiveOneWayStagedLaunch->LaunchFrame;
            StagedAlignedWitnessLocation = ActiveOneWayStagedLaunch->AlignedWitnessLocation;
            StagedAlignedWitnessVelocity = ActiveOneWayStagedLaunch->AlignedWitnessVelocity;
            StagedInitialTargetLocation = ActiveOneWayStagedLaunch->InitialTargetLocation;
            StagedInitialWitnessSeparation = ActiveOneWayStagedLaunch->InitialWitnessSeparation;
            StagedCalibrationWitnessGap = ActiveOneWayStagedLaunch->CalibrationWitnessGap;
            StagedHorizontalFlightGap = ActiveOneWayStagedLaunch->StagedHorizontalFlightGap;
            StagedPredictedFlightTimeSeconds =
                ActiveOneWayStagedLaunch->PredictedFlightTimeSeconds;
            StagedGravityZ = ActiveOneWayStagedLaunch->StagedGravityZ;
            StagedLaunchDirectionDot = ActiveOneWayStagedLaunch->LaunchDirectionDot;
            StagedLaunchTargetToWitnessDirection =
                ActiveOneWayStagedLaunch->LaunchTargetToWitnessDirection;
            StagedCollisionIsolatedSiblingCount =
                ActiveOneWayStagedLaunch->CollisionIsolatedSiblingIndices.Num();
            StagedTargetCollisionConfiguredCount =
                ActiveOneWayStagedLaunch->TargetCollisionConfiguredCount;
            StagedTargetCollisionConfiguredFrame =
                ActiveOneWayStagedLaunch->TargetCollisionConfiguredFrame;
            StagedWitnessAlignmentFrame = ActiveOneWayStagedLaunch->WitnessAlignmentFrame;
            bStagedWitnessAlignmentCompleted = ActiveOneWayStagedLaunch->bWitnessAlignmentCompleted;
            StagedVelocityDispatchFrame = ActiveOneWayStagedLaunch->VelocityDispatchFrame;
            StagedObservedTargetMotionProjection =
                ActiveOneWayStagedLaunch->ObservedTargetMotionProjection;
            StagedPTVelocityDispatchCount = ActiveOneWayStagedLaunch->PTVelocityDispatchCount;
            StagedPTVelocityAppliedCount = ActiveOneWayStagedLaunch->PTVelocityAppliedCount.IsValid()
                ? ActiveOneWayStagedLaunch->PTVelocityAppliedCount->Load()
                : 0;
            bStagedVelocityDispatchCompleted =
                ActiveOneWayStagedLaunch->bVelocityDispatchCompleted;
            bStagedTargetMotionObserved = ActiveOneWayStagedLaunch->bTargetMotionObserved;
            StagedContactWitnessAlignmentFrame =
                ActiveOneWayStagedLaunch->ContactWitnessAlignmentFrame;
            bStagedContactWitnessAlignmentCompleted =
                ActiveOneWayStagedLaunch->bContactWitnessAlignmentCompleted;
            StagedContactVelocityDispatchFrame =
                ActiveOneWayStagedLaunch->ContactVelocityDispatchFrame;
            StagedContactPTVelocityDispatchCount =
                ActiveOneWayStagedLaunch->ContactPTVelocityDispatchCount;
            StagedContactPTVelocityAppliedCount =
                ActiveOneWayStagedLaunch->ContactPTVelocityAppliedCount.IsValid()
                    ? ActiveOneWayStagedLaunch->ContactPTVelocityAppliedCount->Load()
                    : 0;
            StagedContactPTOneWayFlag = ActiveOneWayStagedLaunch->ContactPTOneWayFlag.IsValid()
                ? ActiveOneWayStagedLaunch->ContactPTOneWayFlag->Load()
                : -1;
            StagedContactPTObjectState = ActiveOneWayStagedLaunch->ContactPTObjectState.IsValid()
                ? ActiveOneWayStagedLaunch->ContactPTObjectState->Load()
                : -1;
            if (ActiveOneWayStagedLaunch->PTContactDiagnostics.IsValid())
            {
                const FOneWayPTContactDiagnostics& Diagnostics =
                    *ActiveOneWayStagedLaunch->PTContactDiagnostics;
                StagedPTContactDiagnosticSampleCount = Diagnostics.SampleCount.Load();
                StagedPTContactSameSolver = Diagnostics.SameSolver.Load();
                StagedPTTargetObjectState = Diagnostics.TargetObjectState.Load();
                StagedPTWitnessObjectState = Diagnostics.WitnessObjectState.Load();
                StagedPTTargetCollisionGroup = Diagnostics.TargetCollisionGroup.Load();
                StagedPTWitnessCollisionGroup = Diagnostics.WitnessCollisionGroup.Load();
                StagedPTTargetShapeCount = Diagnostics.TargetShapeCount.Load();
                StagedPTTargetSimShapeCount = Diagnostics.TargetSimShapeCount.Load();
                StagedPTWitnessShapeCount = Diagnostics.WitnessShapeCount.Load();
                StagedPTWitnessSimShapeCount = Diagnostics.WitnessSimShapeCount.Load();
                StagedPTPairConstraintCount = Diagnostics.PairConstraintCount.Load();
                StagedPTCurrentPairConstraintCount = Diagnostics.CurrentPairConstraintCount.Load();
                StagedPTEnabledPairConstraintCount = Diagnostics.EnabledPairConstraintCount.Load();
                StagedPTOneWayPairConstraintCount = Diagnostics.OneWayPairConstraintCount.Load();
                StagedPTPairManifoldPointCount = Diagnostics.PairManifoldPointCount.Load();
                StagedPTTargetMassKg = Diagnostics.TargetMassMicroKg.Load() / 1000000.0;
                StagedPTWitnessMassKg = Diagnostics.WitnessMassMicroKg.Load() / 1000000.0;
                StagedPTWitnessPoseErrorCm = Diagnostics.WitnessPoseErrorMicrometers.Load() / 10000.0;
                const int64 MinimumPairPhiMicrometers =
                    Diagnostics.MinimumPairPhiMicrometers.Load();
                bStagedPTPairPhiObserved = MinimumPairPhiMicrometers != MAX_int64;
                StagedPTMinimumPairPhiCm = !bStagedPTPairPhiObserved
                    ? -1.0
                    : MinimumPairPhiMicrometers / 10000.0;
                StagedPTMaximumPairImpulseNs = Diagnostics.MaximumPairImpulseMicroNs.Load() / 1000000.0;
            }
            bStagedSiblingCollisionRestored = ActiveOneWayStagedLaunch->bSiblingCollisionRestored;
            bStagedLaunchCompleted = ActiveOneWayStagedLaunch->bLaunchCompleted;
            StagedLaunchFailure = ActiveOneWayStagedLaunch->Failure;
            if (bStagedLaunchCompleted)
            {
                DebrisItemIndex = StagedTargetRigidIndex;
                DebrisVelocity = ActiveOneWayStagedLaunch->RequestedVelocity;
            }
        }
        if (Operation == TEXT("one_way_sample"))
        {
            for (TPair<FString, FTraceItemDispatchSnapshot>& Pair : TraceItemDispatchSnapshots)
            {
                FTraceItemDispatchSnapshot& Snapshot = Pair.Value;
                if (Snapshot.World.Get() == World && Snapshot.Component.Get() == OneWayComponent &&
                    (!bStagedLaunchCompleted || Snapshot.ItemIndex == StagedTargetRigidIndex))
                {
                    FollowupDirectVelocityAppliedCount = Snapshot.DirectVelocityAppliedCount;
                    DeferredVelocityDispatchAttemptCount = Snapshot.DeferredVelocityDispatchAttemptCount;
                    DeferredVelocityDispatchFrame = Snapshot.DeferredVelocityDispatchFrame;
                    bDeferredVelocityObserved = Snapshot.bDeferredVelocityObserved;
                    bDeferredVelocityDispatchComplete = Snapshot.bDeferredVelocityDispatchComplete;
                    DebrisItemIndex = Snapshot.ItemIndex;
                    DebrisBreakParentItemIndex = Snapshot.BreakParentItemIndex;
                    DebrisBreakParentChildCount = Snapshot.BreakParentChildCount;
                    DebrisVelocity = Snapshot.RequestedVelocity;
                    break;
                }
            }
        }
        const double DirectionDot = ExpectedDirection.IsNearlyZero() || ActualVelocity.IsNearlyZero()
            ? 0.0 : FVector::DotProduct(ExpectedDirection.GetSafeNormal(), ActualVelocity.GetSafeNormal());
        const double DispatchTime = GetNumberParam(Params, TEXT("dispatch_world_time_seconds"), -1.0);
        const bool bPhysicsTickObserved = DispatchTime >= 0.0 && World->GetTimeSeconds() > DispatchTime;
        const bool bStaticFloor = Floor->Mobility == EComponentMobility::Static && !Floor->IsSimulatingPhysics();
        const bool bWitnessMassValid = FMath::IsNearlyEqual(Witness->GetMass(), 50.0f, 0.5f);
        const bool bWitnessGravityDisabled = !Witness->IsGravityEnabled();
        const bool bWitnessCollisionValid =
            Witness->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics &&
            Witness->GetCollisionObjectType() == ECC_PhysicsBody &&
            Witness->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block &&
            Witness->IsSimulatingPhysics();
        const bool bGeometryCollectionCollisionValid =
            OneWayComponent->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics &&
            OneWayComponent->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block;
        const bool bOneWayFlagsValid = OneWayComponent->OneWayInteractionLevel == RequestedLevel;
        const bool bPTOneWayFlagMatchesRequestedLevel = RequestedLevel == -1
            ? StagedContactPTOneWayFlag == 0
            : StagedContactPTOneWayFlag == 1;
        const bool bPTNativePairConstraintObserved =
            StagedPTContactDiagnosticSampleCount > 0 &&
            StagedPTContactSameSolver == 1 &&
            StagedPTPairConstraintCount > 0 &&
            StagedPTCurrentPairConstraintCount > 0 &&
            StagedPTEnabledPairConstraintCount > 0 &&
            StagedPTPairManifoldPointCount > 0 &&
            bStagedPTPairPhiObserved && FMath::IsFinite(StagedPTMinimumPairPhiCm) &&
            FMath::IsFinite(StagedPTMaximumPairImpulseNs) &&
            StagedPTMaximumPairImpulseNs > 0.0;
        const bool bCollisionCaptureComplete = ActiveOneWayCollisionCapture &&
            ActiveOneWayCollisionCapture->World.Get() == World &&
            ActiveOneWayCollisionCapture->Component.Get() == OneWayComponent &&
            ActiveOneWayCollisionCapture->bCaptureCompleted;
        const bool bDebrisReachedFloor = bCollisionCaptureComplete &&
            ActiveOneWayCollisionCapture->DebrisFloorProximityFrameCount > 0;
        const bool bExactStagedTargetCaptured = bCollisionCaptureComplete &&
            ActiveOneWayCollisionCapture->TargetTransformIndex == StagedTargetRigidIndex &&
            ActiveOneWayCollisionCapture->DebrisTransformIndices.Num() == 1 &&
            ActiveOneWayCollisionCapture->DebrisTransformIndices[0] == StagedTargetRigidIndex;
        const bool bNativeContactApproachEvidence = bCollisionCaptureComplete &&
            bExactStagedTargetCaptured &&
            bPTNativePairConstraintObserved &&
            ActiveOneWayCollisionCapture->MaximumProjectedDebrisTravel > 1.0;
        const bool bStagedLaunchFailed = !StagedLaunchFailure.IsEmpty();
        const bool bOneWaySampleAssertion = Operation == TEXT("one_way_sample") && bPhysicsTickObserved &&
            bStaticFloor && bWitnessMassValid && bWitnessGravityDisabled &&
            bWitnessCollisionValid && bGeometryCollectionCollisionValid && bOneWayFlagsValid &&
            bPTOneWayFlagMatchesRequestedLevel && bPTNativePairConstraintObserved &&
            bStagedLaunchCompleted && bStagedTargetMotionObserved &&
            StagedPTVelocityAppliedCount > 0 && StagedContactPTVelocityAppliedCount > 0 &&
            bStagedContactWitnessAlignmentCompleted && !bStagedLaunchFailed && bCollisionCaptureComplete &&
            bNativeContactApproachEvidence && bDebrisReachedFloor;
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("assertion_status"), Operation == TEXT("one_way_sample")
            ? (bStagedLaunchFailed ? TEXT("FAIL")
                : (!bStagedLaunchCompleted || !bDeferredVelocityDispatchComplete || !bCollisionCaptureComplete
                ? TEXT("BLOCKED") : (bOneWaySampleAssertion && bDeferredVelocityObserved ? TEXT("PASS") : TEXT("FAIL"))))
            : TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Both -1 and 2 variants still require five repeated witness measurements"));
        Data->SetNumberField(TEXT("one_way_level"), OneWayComponent->OneWayInteractionLevel);
        Data->SetBoolField(TEXT("one_way_disabled_flag"), OneWayComponent->OneWayInteractionLevel == -1);
        Data->SetBoolField(TEXT("one_way_level2_flag"), OneWayComponent->OneWayInteractionLevel == 2);
        Data->SetBoolField(TEXT("pt_one_way_flag_matches_requested_level"),
            bPTOneWayFlagMatchesRequestedLevel);
        Data->SetBoolField(TEXT("geometry_collection_uses_world_solver"),
            OneWayComponent->ChaosSolverActor == nullptr);
        Data->SetBoolField(TEXT("pt_native_pair_constraint_observed"),
            bPTNativePairConstraintObserved);
        Data->SetBoolField(TEXT("native_pair_constraint_evidence"),
            bPTNativePairConstraintObserved);
        Data->SetNumberField(TEXT("witness_mass_kg"), Witness->GetMass());
        Data->SetStringField(TEXT("witness_collision_profile"),
            Witness->GetCollisionProfileName().ToString());
        Data->SetNumberField(TEXT("witness_collision_enabled"),
            static_cast<int32>(Witness->GetCollisionEnabled()));
        Data->SetNumberField(TEXT("witness_collision_object_type"),
            static_cast<int32>(Witness->GetCollisionObjectType()));
        Data->SetNumberField(TEXT("witness_physics_body_response"),
            static_cast<int32>(Witness->GetCollisionResponseToChannel(ECC_PhysicsBody)));
        Data->SetBoolField(TEXT("witness_simulating_physics"), Witness->IsSimulatingPhysics());
        Data->SetBoolField(TEXT("witness_any_rigid_body_awake"), Witness->IsAnyRigidBodyAwake());
        Data->SetBoolField(TEXT("witness_collision_assertion"), bWitnessCollisionValid);
        Data->SetStringField(TEXT("geometry_collection_collision_profile"),
            OneWayComponent->GetCollisionProfileName().ToString());
        Data->SetNumberField(TEXT("geometry_collection_collision_enabled"),
            static_cast<int32>(OneWayComponent->GetCollisionEnabled()));
        Data->SetNumberField(TEXT("geometry_collection_physics_body_response"),
            static_cast<int32>(OneWayComponent->GetCollisionResponseToChannel(ECC_PhysicsBody)));
        Data->SetBoolField(TEXT("geometry_collection_collision_assertion"),
            bGeometryCollectionCollisionValid);
        TSharedPtr<FJsonObject> WitnessTransform = MakeShared<FJsonObject>();
        WitnessTransform->SetArrayField(TEXT("location"), VectorToJson(Witness->GetComponentLocation()));
        Data->SetObjectField(TEXT("witness_transform"), WitnessTransform);
        Data->SetArrayField(TEXT("initial_witness_location"), VectorToJson(BaselineLocation));
        Data->SetArrayField(TEXT("initial_witness_velocity"), VectorToJson(BaselineVelocity));
        Data->SetArrayField(TEXT("witness_velocity"), VectorToJson(ActualVelocity));
        Data->SetArrayField(TEXT("witness_location_delta"), VectorToJson(WitnessLocationDelta));
        Data->SetArrayField(TEXT("witness_velocity_delta"), VectorToJson(WitnessVelocityDelta));
        Data->SetArrayField(TEXT("witness_impulse_estimate"), VectorToJson(WitnessImpulseEstimate));
        Data->SetNumberField(TEXT("projected_witness_location_delta"), ProjectedLocationDelta);
        Data->SetNumberField(TEXT("projected_witness_velocity_delta"), ProjectedVelocityDelta);
        Data->SetNumberField(TEXT("projected_witness_impulse_estimate"), ProjectedImpulseEstimate);
        Data->SetNumberField(TEXT("orthogonal_witness_velocity_delta"), OrthogonalVelocityDelta);
        Data->SetBoolField(TEXT("gravity_orthogonal_to_expected_direction"), FMath::Abs(ExpectedDirectionNormalized.Z) <= 0.1);
        Data->SetBoolField(TEXT("witness_direct_impulse_applied"), false);
        Data->SetBoolField(TEXT("debris_dispatched"), bDebrisDispatched);
        Data->SetNumberField(TEXT("debris_item_index"), DebrisItemIndex);
        Data->SetNumberField(TEXT("debris_break_parent_item_index"), DebrisBreakParentItemIndex);
        Data->SetNumberField(TEXT("debris_break_parent_child_count"), DebrisBreakParentChildCount);
        int32 SelectedMinimumHierarchyLevel = MAX_int32;
        int32 SelectedMaximumHierarchyLevel = INDEX_NONE;
        TMap<int32, int32> HierarchyLevelCounts;
        if (const UGeometryCollection* RestCollection = OneWayComponent->GetRestCollection())
        {
            if (const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest = RestCollection->GetGeometryCollection())
            {
                auto ResolveHierarchyLevel = [&Rest](int32 TransformIndex)
                {
                    int32 Level = 0;
                    for (int32 Parent = Rest->Parent.IsValidIndex(TransformIndex) ? Rest->Parent[TransformIndex] : INDEX_NONE;
                        Parent != INDEX_NONE && Rest->Parent.IsValidIndex(Parent);
                        Parent = Rest->Parent[Parent])
                    {
                        ++Level;
                        if (Level > Rest->Parent.Num())
                        {
                            break;
                        }
                    }
                    return Level;
                };
                for (int32 TransformIndex = 0; TransformIndex < Rest->Parent.Num(); ++TransformIndex)
                {
                    ++HierarchyLevelCounts.FindOrAdd(ResolveHierarchyLevel(TransformIndex));
                }
                if (const FTraceItemDispatchSnapshot* Snapshot =
                    TraceItemDispatchSnapshots.Find(MakeTraceItemSnapshotKey(OneWayComponent, DebrisItemIndex)))
                {
                    for (int32 TransformIndex : Snapshot->TransformIndices)
                    {
                        if (Rest->Parent.IsValidIndex(TransformIndex))
                        {
                            const int32 HierarchyLevel = ResolveHierarchyLevel(TransformIndex);
                            SelectedMinimumHierarchyLevel = FMath::Min(
                                SelectedMinimumHierarchyLevel, HierarchyLevel);
                            SelectedMaximumHierarchyLevel = FMath::Max(
                                SelectedMaximumHierarchyLevel, HierarchyLevel);
                        }
                    }
                }
            }
        }
        TArray<FString> HierarchyLevelHistogram;
        for (const TPair<int32, int32>& Pair : HierarchyLevelCounts)
        {
            HierarchyLevelHistogram.Add(FString::Printf(TEXT("level_%d=%d"), Pair.Key, Pair.Value));
        }
        HierarchyLevelHistogram.Sort();
        Data->SetArrayField(TEXT("hierarchy_level_histogram"), StringsToJson(HierarchyLevelHistogram));
        Data->SetNumberField(TEXT("selected_item_min_hierarchy_level"),
            SelectedMinimumHierarchyLevel == MAX_int32 ? INDEX_NONE : SelectedMinimumHierarchyLevel);
        Data->SetNumberField(TEXT("selected_item_max_hierarchy_level"), SelectedMaximumHierarchyLevel);
        Data->SetArrayField(TEXT("debris_velocity"), VectorToJson(DebrisVelocity));
        Data->SetNumberField(TEXT("followup_direct_velocity_applied_count"), FollowupDirectVelocityAppliedCount);
        Data->SetNumberField(TEXT("deferred_velocity_dispatch_attempt_count"), DeferredVelocityDispatchAttemptCount);
        Data->SetNumberField(TEXT("deferred_velocity_dispatch_frame"), static_cast<double>(DeferredVelocityDispatchFrame));
        Data->SetBoolField(TEXT("deferred_velocity_observed"), bDeferredVelocityObserved);
        Data->SetBoolField(TEXT("deferred_velocity_dispatch_complete"), bDeferredVelocityDispatchComplete);
        Data->SetNumberField(TEXT("staged_launch_tick_count"), StagedLaunchTickCount);
        Data->SetNumberField(TEXT("staged_target_rigid_index"), StagedTargetRigidIndex);
        Data->SetNumberField(TEXT("staged_target_parent_item"), StagedTargetParentItem);
        Data->SetNumberField(TEXT("staged_parent_release_dispatch_count"), StagedParentReleaseDispatchCount);
        Data->SetNumberField(TEXT("staged_launch_frame"), static_cast<double>(StagedLaunchFrame));
        Data->SetArrayField(TEXT("staged_aligned_witness_location"), VectorToJson(StagedAlignedWitnessLocation));
        Data->SetArrayField(TEXT("staged_aligned_witness_velocity"), VectorToJson(StagedAlignedWitnessVelocity));
        Data->SetArrayField(TEXT("staged_initial_target_location"), VectorToJson(StagedInitialTargetLocation));
        Data->SetNumberField(TEXT("staged_initial_witness_separation"), StagedInitialWitnessSeparation);
        Data->SetNumberField(TEXT("staged_calibration_witness_gap"), StagedCalibrationWitnessGap);
        Data->SetNumberField(TEXT("staged_horizontal_flight_gap"), StagedHorizontalFlightGap);
        Data->SetNumberField(TEXT("staged_predicted_flight_time_seconds"),
            StagedPredictedFlightTimeSeconds);
        Data->SetNumberField(TEXT("staged_gravity_z"), StagedGravityZ);
        Data->SetNumberField(TEXT("staged_launch_direction_dot"), StagedLaunchDirectionDot);
        Data->SetArrayField(TEXT("staged_launch_target_to_witness_direction"),
            VectorToJson(StagedLaunchTargetToWitnessDirection));
        Data->SetNumberField(TEXT("staged_collision_isolated_sibling_count"),
            StagedCollisionIsolatedSiblingCount);
        Data->SetNumberField(TEXT("staged_target_collision_configured_count"),
            StagedTargetCollisionConfiguredCount);
        Data->SetNumberField(TEXT("staged_target_collision_configured_frame"),
            static_cast<double>(StagedTargetCollisionConfiguredFrame));
        Data->SetNumberField(TEXT("staged_witness_alignment_frame"),
            static_cast<double>(StagedWitnessAlignmentFrame));
        Data->SetBoolField(TEXT("staged_witness_alignment_completed"),
            bStagedWitnessAlignmentCompleted);
        Data->SetNumberField(TEXT("staged_velocity_dispatch_frame"),
            static_cast<double>(StagedVelocityDispatchFrame));
        Data->SetNumberField(TEXT("staged_observed_target_motion_projection"),
            StagedObservedTargetMotionProjection);
        Data->SetNumberField(TEXT("staged_pt_velocity_dispatch_count"),
            StagedPTVelocityDispatchCount);
        Data->SetNumberField(TEXT("staged_pt_velocity_applied_count"),
            StagedPTVelocityAppliedCount);
        Data->SetBoolField(TEXT("staged_velocity_dispatch_completed"),
            bStagedVelocityDispatchCompleted);
        Data->SetBoolField(TEXT("staged_target_motion_observed"),
            bStagedTargetMotionObserved);
        Data->SetNumberField(TEXT("staged_contact_witness_alignment_frame"),
            static_cast<double>(StagedContactWitnessAlignmentFrame));
        Data->SetBoolField(TEXT("staged_contact_witness_alignment_completed"),
            bStagedContactWitnessAlignmentCompleted);
        Data->SetNumberField(TEXT("staged_contact_velocity_dispatch_frame"),
            static_cast<double>(StagedContactVelocityDispatchFrame));
        Data->SetNumberField(TEXT("staged_contact_pt_velocity_dispatch_count"),
            StagedContactPTVelocityDispatchCount);
        Data->SetNumberField(TEXT("staged_contact_pt_velocity_applied_count"),
            StagedContactPTVelocityAppliedCount);
        Data->SetNumberField(TEXT("staged_contact_pt_one_way_flag"),
            StagedContactPTOneWayFlag);
        Data->SetNumberField(TEXT("staged_contact_pt_object_state"),
            StagedContactPTObjectState);
        Data->SetNumberField(TEXT("staged_pt_contact_diagnostic_sample_count"),
            StagedPTContactDiagnosticSampleCount);
        Data->SetNumberField(TEXT("staged_pt_contact_same_solver"), StagedPTContactSameSolver);
        Data->SetNumberField(TEXT("staged_pt_target_object_state"), StagedPTTargetObjectState);
        Data->SetNumberField(TEXT("staged_pt_witness_object_state"), StagedPTWitnessObjectState);
        Data->SetNumberField(TEXT("staged_pt_target_collision_group"), StagedPTTargetCollisionGroup);
        Data->SetNumberField(TEXT("staged_pt_witness_collision_group"), StagedPTWitnessCollisionGroup);
        Data->SetNumberField(TEXT("staged_pt_target_shape_count"), StagedPTTargetShapeCount);
        Data->SetNumberField(TEXT("staged_pt_target_sim_shape_count"), StagedPTTargetSimShapeCount);
        Data->SetNumberField(TEXT("staged_pt_witness_shape_count"), StagedPTWitnessShapeCount);
        Data->SetNumberField(TEXT("staged_pt_witness_sim_shape_count"), StagedPTWitnessSimShapeCount);
        Data->SetNumberField(TEXT("staged_pt_pair_constraint_count"), StagedPTPairConstraintCount);
        Data->SetNumberField(TEXT("staged_pt_current_pair_constraint_count"),
            StagedPTCurrentPairConstraintCount);
        Data->SetNumberField(TEXT("staged_pt_enabled_pair_constraint_count"),
            StagedPTEnabledPairConstraintCount);
        Data->SetNumberField(TEXT("staged_pt_one_way_pair_constraint_count"),
            StagedPTOneWayPairConstraintCount);
        Data->SetNumberField(TEXT("staged_pt_pair_manifold_point_count"),
            StagedPTPairManifoldPointCount);
        Data->SetNumberField(TEXT("staged_pt_target_mass_kg"), StagedPTTargetMassKg);
        Data->SetNumberField(TEXT("staged_pt_witness_mass_kg"), StagedPTWitnessMassKg);
        Data->SetNumberField(TEXT("staged_pt_witness_pose_error_cm"), StagedPTWitnessPoseErrorCm);
        Data->SetBoolField(TEXT("staged_pt_pair_phi_observed"), bStagedPTPairPhiObserved);
        Data->SetNumberField(TEXT("staged_pt_minimum_pair_phi_cm"), StagedPTMinimumPairPhiCm);
        Data->SetNumberField(TEXT("staged_pt_maximum_pair_impulse_ns"),
            StagedPTMaximumPairImpulseNs);
        Data->SetBoolField(TEXT("staged_sibling_collision_restored"),
            bStagedSiblingCollisionRestored);
        Data->SetBoolField(TEXT("staged_launch_completed"), bStagedLaunchCompleted);
        Data->SetStringField(TEXT("staged_launch_failure"), StagedLaunchFailure);
        Data->SetNumberField(TEXT("velocity_direction_dot"), DirectionDot);
        Data->SetBoolField(TEXT("physics_tick_observed"), bPhysicsTickObserved);
        Data->SetNumberField(TEXT("world_time_seconds"), World->GetTimeSeconds());
        Data->SetBoolField(TEXT("static_floor"), bStaticFloor);
        Data->SetBoolField(TEXT("witness_mass_assertion"), bWitnessMassValid);
        Data->SetBoolField(TEXT("witness_gravity_enabled"), Witness->IsGravityEnabled());
        Data->SetBoolField(TEXT("witness_gravity_disabled_assertion"), bWitnessGravityDisabled);
        Data->SetBoolField(TEXT("one_way_flag_assertion"), bOneWayFlagsValid);
        Data->SetBoolField(TEXT("native_contact_approach_evidence"),
            bNativeContactApproachEvidence);
        Data->SetBoolField(TEXT("exact_staged_target_capture_assertion"),
            bExactStagedTargetCaptured);
        if (ActiveOneWayCollisionCapture && ActiveOneWayCollisionCapture->World.Get() == World &&
            ActiveOneWayCollisionCapture->Component.Get() == OneWayComponent)
        {
            Data->SetNumberField(TEXT("collision_capture_target_transform_index"),
                ActiveOneWayCollisionCapture->TargetTransformIndex);
            Data->SetArrayField(TEXT("collision_capture_transform_indices"),
                IntsToJson(ActiveOneWayCollisionCapture->DebrisTransformIndices));
            Data->SetNumberField(TEXT("collision_capture_transform_count"),
                ActiveOneWayCollisionCapture->DebrisTransformIndices.Num());
            Data->SetNumberField(TEXT("collision_capture_frame_count"), ActiveOneWayCollisionCapture->CapturedFrameCount);
            Data->SetBoolField(TEXT("collision_capture_completed"), ActiveOneWayCollisionCapture->bCaptureCompleted);
            Data->SetNumberField(TEXT("max_projected_witness_displacement"),
                ActiveOneWayCollisionCapture->MaximumProjectedWitnessDisplacement);
            Data->SetNumberField(TEXT("max_projected_witness_velocity"),
                ActiveOneWayCollisionCapture->MaximumProjectedWitnessVelocity);
            Data->SetNumberField(TEXT("max_projected_debris_travel"),
                ActiveOneWayCollisionCapture->MaximumProjectedDebrisTravel);
            Data->SetNumberField(TEXT("initial_debris_witness_separation"),
                ActiveOneWayCollisionCapture->InitialDebrisWitnessSeparation);
            Data->SetNumberField(TEXT("first_sample_debris_witness_separation"),
                ActiveOneWayCollisionCapture->FirstSampleDebrisWitnessSeparation);
            Data->SetNumberField(TEXT("initial_debris_velocity_target_direction_dot"),
                ActiveOneWayCollisionCapture->InitialDebrisVelocityDirectionDot);
            Data->SetNumberField(TEXT("minimum_debris_witness_separation"),
                ActiveOneWayCollisionCapture->MinimumDebrisWitnessSeparation == TNumericLimits<double>::Max()
                    ? -1.0
                    : ActiveOneWayCollisionCapture->MinimumDebrisWitnessSeparation);
            Data->SetNumberField(TEXT("debris_witness_closing_frame_count"),
                ActiveOneWayCollisionCapture->ClosingFrameCount);
            Data->SetNumberField(TEXT("debris_witness_approach_frame_count"),
                ActiveOneWayCollisionCapture->ApproachFrameCount);
            Data->SetNumberField(TEXT("contact_proximity_frame_count"),
                ActiveOneWayCollisionCapture->ContactProximityFrameCount);
            Data->SetArrayField(TEXT("first_sample_debris_location"),
                VectorToJson(ActiveOneWayCollisionCapture->InitialDebrisLocation));
            Data->SetArrayField(TEXT("latest_debris_location"),
                VectorToJson(ActiveOneWayCollisionCapture->LatestDebrisLocation));
            Data->SetArrayField(TEXT("latest_debris_velocity"),
                VectorToJson(ActiveOneWayCollisionCapture->LatestDebrisVelocity));
            Data->SetNumberField(TEXT("debris_floor_proximity_frame_count"),
                ActiveOneWayCollisionCapture->DebrisFloorProximityFrameCount);
            Data->SetBoolField(TEXT("debris_reached_floor"), bDebrisReachedFloor);
        }
        Data->SetBoolField(TEXT("requires_followup_sample"),
            Operation == TEXT("one_way_run") ||
            (Operation == TEXT("one_way_sample") && !bStagedLaunchFailed &&
                (!bStagedLaunchCompleted || !bDeferredVelocityDispatchComplete ||
                    !bCollisionCaptureComplete)));
        AddTraceDiagnostics(Data, OneWayTraceDiagnostics);
        return Success(Data);
    }

    if (Operation == TEXT("removal_abort"))
    {
        const bool bHadSession = ActiveRemovalSession != nullptr;
        ClearActiveRemovalSession();
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Removal session was cleared without claiming timeline validation"));
        Data->SetBoolField(TEXT("had_active_session"), bHadSession);
        return Success(Data);
    }

    if (Operation == TEXT("removal_begin"))
    {
        ClearActiveRemovalSession();
        const FString RemovalActorName = GetStringParam(Params, TEXT("actor_name"), TEXT(""));
        UGeometryCollectionComponent* RemovalComponent = FindGeometryCollectionComponent(World, RemovalActorName);
        if (!RemovalComponent || !RemovalComponent->GetOwner()->Tags.Contains(LearningActorTag) ||
            !RemovalComponent->GetRestCollection() || !IsWithinLearningRoot(RemovalComponent->GetRestCollection()->GetPathName()))
        {
            return LegacyError(TEXT("Removal session requires a tagged learning Geometry Collection actor"));
        }
        const bool bSleepOnly = RemovalComponent->bAllowRemovalOnSleep && !RemovalComponent->bAllowRemovalOnBreak;
        const bool bBreakOnly = !RemovalComponent->bAllowRemovalOnSleep && RemovalComponent->bAllowRemovalOnBreak;
        if (!bSleepOnly && !bBreakOnly)
        {
            return LegacyError(TEXT("Removal actor must be configured as exactly Sleep-only or Break-only"));
        }
        CurrentWork->SDK(TEXT("create_FRemovalLearningSession"),true);
        ActiveRemovalSession = MakeUnique<FRemovalLearningSession>();
        ActiveRemovalSession->Evidence=CurrentWork;
        ActiveRemovalSession->World = World;
        ActiveRemovalSession->Component = RemovalComponent;
        ActiveRemovalSession->Mode = bSleepOnly ? TEXT("sleep_only") : TEXT("break_only");
        ActiveRemovalSession->CaptureLimitFrames = FMath::Clamp(
            CastI32(GetNumberParam(Params, TEXT("capture_limit_frames"), 300)), 60, 1200);
        const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest =
            RemovalComponent->GetRestCollection()->GetGeometryCollection();
        GeometryCollection::Facades::FCollectionRemoveOnBreakFacade RemoveFacade(*Rest);
        TSet<int32> TargetSet;
        if (RemoveFacade.IsValid())
        {
            for (int32 Index = 0; Index < Rest->Transform.Num(); ++Index)
            {
                if (RemoveFacade.GetData(Index).IsEnabled())
                {
                    ActiveRemovalSession->TargetIndices.Add(Index);
                    TargetSet.Add(Index);
                    const GeometryCollection::Facades::FRemoveOnBreakData RemoveData = RemoveFacade.GetData(Index);
                    const FVector2f BreakTimer = RemoveData.GetBreakTimer();
                    const FVector2f RemovalTimer = RemoveData.GetRemovalTimer();
                    FRemovalPieceTimeline& Timeline = ActiveRemovalSession->PieceTimelines.Add(Index);
                    Timeline.TransformIndex = Index;
                    Timeline.bTarget = true;
                    Timeline.BreakTimerRange = FVector2D(BreakTimer.X, BreakTimer.Y);
                    Timeline.ExpectedCompletionDelayRange = bBreakOnly
                        ? FVector2D(BreakTimer.X + RemovalTimer.X, BreakTimer.Y + RemovalTimer.Y)
                        : FVector2D(
                            RemovalComponent->GetRestCollection()->MaximumSleepTime.X +
                                RemovalComponent->GetRestCollection()->RemovalDuration.X,
                            RemovalComponent->GetRestCollection()->MaximumSleepTime.Y +
                                RemovalComponent->GetRestCollection()->RemovalDuration.Y);
                }
            }
        }
        if (bSleepOnly)
        {
            ActiveRemovalSession->bOriginalGravityEnabled = RemovalComponent->IsGravityEnabled();
            if(!BeforeObjectSDK(RemovalComponent,TEXT("SetEnableGravity"))) return LegacyError(CurrentWork->R.Error);
            ActiveRemovalSession->bGravityOverridden=true;
            RemovalComponent->SetEnableGravity(false);
            CurrentWork->R.Data->SetBoolField(TEXT("removal_gravity_disable_sdk_returned"),true);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            ActiveRemovalSession->bGravityOverridden = true;
            if (FGeometryDynamicCollection* Dynamic = RemovalComponent->GetDynamicCollection())
            {
                FGeometryCollectionRemoveOnSleepDynamicFacade SleepRemovalFacade(*Dynamic);
                if (SleepRemovalFacade.IsValid())
                {
                    for (int32 TargetIndex : ActiveRemovalSession->TargetIndices)
                    {
                        constexpr float RuntimeMaxSleepTimeSeconds = 1.0f;
                        constexpr float RuntimeRemovalDurationSeconds = 0.5f;
                        {
                         const float BeforeRuntimeValue=SleepRemovalFacade.GetMaxSleepTime(TargetIndex);
                         if(!BeforeSDK(TEXT("SetMaxSleepTime"))) return LegacyError(CurrentWork->R.Error);
                         SleepRemovalFacade.SetMaxSleepTime(TargetIndex, RuntimeMaxSleepTimeSeconds);
                         RuntimeValueReturned(TEXT("SetMaxSleepTime"),TargetIndex,BeforeRuntimeValue,SleepRemovalFacade.GetMaxSleepTime(TargetIndex));
                         if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                        }
                        {
                         const float BeforeRuntimeValue=SleepRemovalFacade.GetSleepRemovalDuration(TargetIndex);
                         if(!BeforeSDK(TEXT("SetSleepRemovalDuration"))) return LegacyError(CurrentWork->R.Error);
                         SleepRemovalFacade.SetSleepRemovalDuration(TargetIndex, RuntimeRemovalDurationSeconds);
                         RuntimeValueReturned(TEXT("SetSleepRemovalDuration"),TargetIndex,BeforeRuntimeValue,SleepRemovalFacade.GetSleepRemovalDuration(TargetIndex));
                         if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                        }
                        FRemovalPieceTimeline& Timeline = ActiveRemovalSession->PieceTimelines.FindChecked(TargetIndex);
                        const double ExactDelay = RuntimeMaxSleepTimeSeconds + RuntimeRemovalDurationSeconds;
                        Timeline.SleepTimerSeconds = RuntimeMaxSleepTimeSeconds;
                        Timeline.SleepRemovalDurationSeconds = RuntimeRemovalDurationSeconds;
                        Timeline.ExpectedCompletionDelayRange = FVector2D(ExactDelay, ExactDelay);
                    }
                }
            }
        }
        for (int32 Index = 0; Index < Rest->SimulationType.Num() && ActiveRemovalSession->LargeIndices.Num() < 8; ++Index)
        {
            if (Rest->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid && !TargetSet.Contains(Index))
            {
                ActiveRemovalSession->LargeIndices.Add(Index);
                FRemovalPieceTimeline& Timeline = ActiveRemovalSession->PieceTimelines.Add(Index);
                Timeline.TransformIndex = Index;
                Timeline.bTarget = false;
            }
        }
        if (bSleepOnly)
        {
            if (FGeometryDynamicCollection* Dynamic = RemovalComponent->GetDynamicCollection())
            {
                FGeometryCollectionRemoveOnSleepDynamicFacade SleepRemovalFacade(*Dynamic);
                if (SleepRemovalFacade.IsValid())
                {
                    for (int32 TransformIndex = 0; TransformIndex < Rest->SimulationType.Num(); ++TransformIndex)
                    {
                        if (Rest->SimulationType[TransformIndex] == FGeometryCollection::ESimulationTypes::FST_Rigid &&
                            !TargetSet.Contains(TransformIndex))
                        {
                            {
                             const float BeforeRuntimeValue=SleepRemovalFacade.GetMaxSleepTime(TransformIndex);
                             if(!BeforeSDK(TEXT("SetMaxSleepTime"))) return LegacyError(CurrentWork->R.Error);
                             SleepRemovalFacade.SetMaxSleepTime(TransformIndex, 1000.0f);
                             RuntimeValueReturned(TEXT("SetMaxSleepTime"),TransformIndex,BeforeRuntimeValue,SleepRemovalFacade.GetMaxSleepTime(TransformIndex));
                             if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                            }
                            {
                             const float BeforeRuntimeValue=SleepRemovalFacade.GetSleepRemovalDuration(TransformIndex);
                             if(!BeforeSDK(TEXT("SetSleepRemovalDuration"))) return LegacyError(CurrentWork->R.Error);
                             SleepRemovalFacade.SetSleepRemovalDuration(TransformIndex, 0.5f);
                             RuntimeValueReturned(TEXT("SetSleepRemovalDuration"),TransformIndex,BeforeRuntimeValue,SleepRemovalFacade.GetSleepRemovalDuration(TransformIndex));
                             if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                            }
                        }
                    }
                }
            }
        }
        ActiveRemovalSession->BeginFrame = GFrameCounter;
        ActiveRemovalSession->LastSampleFrame = GFrameCounter;
        ActiveRemovalSession->BeginWorldTime = World->GetTimeSeconds();
        ActiveRemovalSession->Previous = CaptureRemovalPieceSnapshot(
            RemovalComponent, *ActiveRemovalSession, ActiveRemovalSession->BeginWorldTime, ActiveRemovalSession->BeginFrame);
        AChaosSolverActor* RemovalSolverActor = RemovalComponent->ChaosSolverActor;
        UChaosGameplayEventDispatcher* RemovalEventDispatcher =
            RemovalSolverActor ? RemovalSolverActor->GetGameplayEventDispatcher() : nullptr;
        if (!RemovalEventDispatcher)
        {
            ClearActiveRemovalSession();
            return LegacyError(
                TEXT("Removal session requires a Chaos gameplay event dispatcher for exact break timing"));
        }
        ActiveRemovalSession->BreakEventDispatcher = RemovalEventDispatcher;
        if(!BeforeObjectSDK(RemovalComponent,TEXT("SetNotifyBreaks"))) return LegacyError(CurrentWork->R.Error);
        RemovalComponent->SetNotifyBreaks(true);
        if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
        RegisterOwnedBreak(
            RemovalComponent,
            [](const FChaosBreakEvent& Event)
            {
                if (!ActiveRemovalSession || Event.Component != ActiveRemovalSession->Component.Get())
                {
                    return;
                }
                FRemovalPieceTimeline* Timeline = ActiveRemovalSession->PieceTimelines.Find(Event.Index);
                if (!Timeline || !Timeline->bTarget)
                {
                    return;
                }
                ++ActiveRemovalSession->GameplayBreakEventCount;
                if (!Timeline->bEverReleased)
                {
                    Timeline->bEverReleased = true;
                    Timeline->FirstBrokenFrame = GFrameCounter;
                    Timeline->BreakTimeSource = TEXT("chaos_gameplay_break_callback");
                    if (UWorld* SessionWorld = ActiveRemovalSession->World.Get())
                    {
                        Timeline->FirstBrokenWorldTime = SessionWorld->GetTimeSeconds();
                    }
                }
            });
        const int32 RootItem = ResolveActiveClusterItemIndex(RemovalComponent);
        int32 RemovalDispatchCount = 0;
        if (bSleepOnly)
        {
            ActiveRemovalSession->SleepReleaseRootItem = RootItem;
            if (RootItem != INDEX_NONE)
            {
                if(!BeforeObjectSDK(RemovalComponent,TEXT("ApplyExternalStrain"))) return LegacyError(CurrentWork->R.Error);
                RemovalComponent->ApplyExternalStrain(
                    RootItem,
                    RemovalComponent->Bounds.Origin,
                    1000.0f,
                    0,
                    0.0f,
                    1000000000000.0f);
                if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                ++ActiveRemovalSession->SleepReleaseRootDispatchCount;
                ActiveRemovalSession->SleepReleaseRootDispatchFrame = GFrameCounter;
            }
        }
        else for (int32 TargetIndex : ActiveRemovalSession->TargetIndices)
        {
            if(!BeforeObjectSDK(RemovalComponent,TEXT("ApplyExternalStrain"))) return LegacyError(CurrentWork->R.Error);
            RemovalComponent->ApplyExternalStrain(
                TargetIndex, RemovalComponent->Bounds.Origin, 50.0f, 0, 0.0f, 1000000.0f);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            ++RemovalDispatchCount;
        }
        ActiveRemovalSession->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([](float)
            {
                return ActiveRemovalSession ? TickOwned(ActiveRemovalSession->Evidence,[]{return CaptureActiveRemovalFrame();}):false;
            }));
        const bool bBaselineValid = ActiveRemovalSession->TargetIndices.Num() == 24 &&
            ActiveRemovalSession->Previous.TargetGoneCount == 0 && ActiveRemovalSession->LargeIndices.Num() == 8;
        const bool bReleaseStageStarted = bSleepOnly
            ? ActiveRemovalSession->SleepReleaseRootDispatchCount == 1
            : RemovalDispatchCount == 24;
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), bReleaseStageStarted && bBaselineValid ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("An internal editor-tick sampler is tracking every target piece until removal or bounded timeout"));
        Data->SetStringField(TEXT("removal_mode"), ActiveRemovalSession->Mode);
        Data->SetArrayField(TEXT("target_indices"), IntsToJson(ActiveRemovalSession->TargetIndices));
        Data->SetArrayField(TEXT("designated_large_indices"), IntsToJson(ActiveRemovalSession->LargeIndices));
        Data->SetNumberField(TEXT("root_item_index"), RootItem);
        Data->SetNumberField(TEXT("target_item_destruction_dispatch_count"), RemovalDispatchCount);
        Data->SetNumberField(TEXT("sleep_release_root_dispatch_count"), ActiveRemovalSession->SleepReleaseRootDispatchCount);
        Data->SetNumberField(TEXT("sleep_release_root_dispatch_frame"),
            static_cast<double>(ActiveRemovalSession->SleepReleaseRootDispatchFrame));
        Data->SetArrayField(TEXT("sleep_release_parent_items"), IntsToJson(ActiveRemovalSession->SleepReleaseParentItems));
        Data->SetNumberField(TEXT("sleep_release_parent_dispatch_count"), ActiveRemovalSession->SleepReleaseParentDispatchCount);
        Data->SetNumberField(TEXT("sleep_release_target_dispatch_count"), ActiveRemovalSession->SleepReleaseTargetDispatchCount);
        Data->SetBoolField(TEXT("gameplay_break_callback_registered"), true);
        Data->SetBoolField(TEXT("gravity_overridden_for_sleep_session"), ActiveRemovalSession->bGravityOverridden);
        Data->SetNumberField(TEXT("begin_frame"), static_cast<double>(ActiveRemovalSession->BeginFrame));
        Data->SetNumberField(TEXT("begin_world_time"), ActiveRemovalSession->BeginWorldTime);
        Data->SetObjectField(TEXT("baseline"), RemovalPieceSnapshotToJson(ActiveRemovalSession->Previous));
        Data->SetNumberField(TEXT("capture_limit_frames"), ActiveRemovalSession->CaptureLimitFrames);
        Data->SetBoolField(TEXT("internal_tick_sampler_active"), ActiveRemovalSession->TickerHandle.IsValid());
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        return Success(Data);
    }

    if (Operation == TEXT("removal_sample") || Operation == TEXT("removal_finish"))
    {
        if (!ActiveRemovalSession || ActiveRemovalSession->World.Get() != World || !ActiveRemovalSession->Component.IsValid())
        {
            return LegacyError(TEXT("No active removal session exists; call removal_begin first"));
        }
        const uint64 CurrentFrame = GFrameCounter;
        const uint64 FrameDelta = CurrentFrame - ActiveRemovalSession->LastSampleFrame;
        const FRemovalPieceSnapshot Current = ActiveRemovalSession->Previous;
        const bool bFinish = Operation == TEXT("removal_finish");
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("removal_mode"), ActiveRemovalSession->Mode);
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetObjectField(TEXT("current"), RemovalPieceSnapshotToJson(Current));
        Data->SetNumberField(TEXT("begin_world_time"), ActiveRemovalSession->BeginWorldTime);
        Data->SetNumberField(TEXT("all_gone_world_time"), ActiveRemovalSession->AllGoneWorldTime);
        Data->SetNumberField(TEXT("captured_frame_count"), ActiveRemovalSession->CapturedFrameCount);
        Data->SetNumberField(TEXT("capture_limit_frames"), ActiveRemovalSession->CaptureLimitFrames);
        Data->SetBoolField(TEXT("capture_completed"), ActiveRemovalSession->bCaptureCompleted);
        Data->SetNumberField(TEXT("forced_sleep_attempt_count"), ActiveRemovalSession->ForcedSleepAttemptCount);
        Data->SetNumberField(TEXT("forced_sleep_object_count"), ActiveRemovalSession->ForcedSleepObjectCount);
        Data->SetNumberField(TEXT("collision_isolated_target_count"), ActiveRemovalSession->CollisionIsolatedTargetIndices.Num());
        Data->SetNumberField(TEXT("pt_sleep_command_dispatch_count"), ActiveRemovalSession->PTSleepCommandDispatchCount);
        Data->SetNumberField(TEXT("pt_sleep_requested_particle_count"), ActiveRemovalSession->PTSleepRequestedParticleCount);
        Data->SetNumberField(TEXT("pt_sleep_applied_particle_count"),
            ActiveRemovalSession->PTSleepAppliedParticleCount.IsValid()
                ? ActiveRemovalSession->PTSleepAppliedParticleCount->Load()
                : 0);
        Data->SetNumberField(TEXT("gameplay_break_event_count"), ActiveRemovalSession->GameplayBreakEventCount);
        Data->SetNumberField(TEXT("sleep_release_root_item"), ActiveRemovalSession->SleepReleaseRootItem);
        Data->SetNumberField(TEXT("sleep_release_root_dispatch_count"), ActiveRemovalSession->SleepReleaseRootDispatchCount);
        Data->SetNumberField(TEXT("sleep_release_root_dispatch_frame"),
            static_cast<double>(ActiveRemovalSession->SleepReleaseRootDispatchFrame));
        Data->SetNumberField(TEXT("sleep_release_parent_dispatch_count"), ActiveRemovalSession->SleepReleaseParentDispatchCount);
        Data->SetNumberField(TEXT("sleep_release_parent_dispatch_frame"),
            static_cast<double>(ActiveRemovalSession->SleepReleaseParentDispatchFrame));
        Data->SetNumberField(TEXT("sleep_release_target_dispatch_count"), ActiveRemovalSession->SleepReleaseTargetDispatchCount);
        Data->SetNumberField(TEXT("sleep_release_target_dispatch_frame"),
            static_cast<double>(ActiveRemovalSession->SleepReleaseTargetDispatchFrame));
        Data->SetNumberField(TEXT("sleep_release_target_reapply_count"), ActiveRemovalSession->SleepReleaseTargetReapplyCount);
        Data->SetBoolField(TEXT("gravity_overridden_for_sleep_session"), ActiveRemovalSession->bGravityOverridden);
        Data->SetNumberField(TEXT("last_forced_sleep_frame"), static_cast<double>(ActiveRemovalSession->LastForcedSleepFrame));
        Data->SetStringField(TEXT("completion_reason"), ActiveRemovalSession->CompletionReason);
        Data->SetArrayField(TEXT("samples"), ActiveRemovalSession->Samples);
        if (!bFinish || !ActiveRemovalSession->bCaptureCompleted)
        {
            Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_blocker"), TEXT("Internal per-frame removal capture is still active; removal_finish only evaluates a completed timeline"));
            Data->SetBoolField(TEXT("requires_followup_sample"), true);
            return Success(Data);
        }

        const double FixedDeltaSeconds = GetNumberParam(Params, TEXT("fixed_delta_seconds"), 1.0 / 30.0);
        bool bTimingPass = false;
        const TArray<TSharedPtr<FJsonValue>> PieceTimelines = RemovalPieceTimelinesToJson(
            *ActiveRemovalSession, FixedDeltaSeconds, bTimingPass);
        const bool bLargeRemainPass = Current.LargeRemainingCount == Current.LargeCount;
        const bool bEnoughEvidence = ActiveRemovalSession->AllGoneWorldTime >= 0.0 &&
            Current.TargetGoneCount == Current.TargetCount && Current.TargetCount == 24;
        const bool bRemovalPass = bEnoughEvidence && bTimingPass && bLargeRemainPass && Current.TargetGoneCount == 24;
        SetRemovalTargetSimulationCollision(*ActiveRemovalSession, true);
        RestoreRemovalGravity(*ActiveRemovalSession);
        Data->SetNumberField(TEXT("fixed_delta_seconds"), FixedDeltaSeconds);
        Data->SetBoolField(TEXT("timing_within_one_fixed_tick"), bTimingPass);
        Data->SetBoolField(TEXT("designated_large_remain_assertion"), bLargeRemainPass);
        Data->SetArrayField(TEXT("piece_timing_summary"), PieceTimelines);
        Data->SetBoolField(TEXT("gravity_restored"), ActiveRemovalSession->bGravityRestored || !ActiveRemovalSession->bGravityOverridden);
        Data->SetBoolField(TEXT("target_simulation_collision_restored"), ActiveRemovalSession->bTargetCollisionRestored);
        Data->SetStringField(TEXT("assertion_status"), bRemovalPass ? TEXT("PASS") : (bEnoughEvidence ? TEXT("FAIL") : TEXT("BLOCKED")));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), bRemovalPass
            ? TEXT("Removal timeline passed; five repeated Sleep-only/Break-only runs remain")
            : TEXT("Exact fixed-frame disappearance timing and remaining-large evidence are required"));
        ClearActiveRemovalSession();
        return Success(Data);
    }

    if (Operation == TEXT("throttle_abort"))
    {
        const bool bHadSession = ActiveThrottleSession != nullptr;
        const bool bRestored = RestoreActiveThrottleSession();
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), bRestored ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("assertion_status"), bRestored ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Throttle session was aborted/restored; no completed fixed-frame validation was produced"));
        Data->SetBoolField(TEXT("had_active_session"), bHadSession);
        Data->SetBoolField(TEXT("settings_restored"), bRestored);
        return Success(Data);
    }

    if (Operation == TEXT("throttle_begin"))
    {
        if(!RestoreActiveThrottleSession())return LegacyError(TEXT("Prior throttle settings could not be safely restored"));
        const FString SolverName = GetStringParam(Params, TEXT("solver_actor_name"), TEXT(""));
        AChaosSolverActor* SolverActor = FindActor<AChaosSolverActor>(World, SolverName);
        if (!SolverActor || !SolverActor->Tags.Contains(LearningActorTag))
        {
            return LegacyError(TEXT("Tagged Chaos learning solver actor not found"));
        }
        FString Scenario = GetStringParam(Params, TEXT("scenario"), TEXT("break_throttled"));
        Scenario.ToLowerInline();
        if (Scenario != TEXT("unthrottled_baseline") && Scenario != TEXT("break_throttled") &&
            Scenario != TEXT("particle_release_throttled"))
        {
            return LegacyError(
                TEXT("scenario must be unthrottled_baseline, break_throttled, or particle_release_throttled"));
        }
        const int32 Unlimited = TNumericLimits<int32>::Max();
        const int32 DefaultBreaksAllowed = Scenario == TEXT("break_throttled") ? 2 : Unlimited;
        const int32 DefaultRescheduleLimit = Scenario == TEXT("break_throttled") ? 64 : Unlimited;
        const int32 DefaultMinimumReleaseCount = Scenario == TEXT("particle_release_throttled") ? 1 : INDEX_NONE;
        const int32 DefaultMaximumReleaseCount = Scenario == TEXT("particle_release_throttled") ? 2 : INDEX_NONE;
        const int32 BreaksAllowed = FMath::Max(0, CastI32(
            GetNumberParam(Params, TEXT("per_advance_breaks_allowed"), DefaultBreaksAllowed)));
        const int32 RescheduleLimit = FMath::Max(0, CastI32(
            GetNumberParam(Params, TEXT("per_advance_breaks_reschedule_limit"), DefaultRescheduleLimit)));
        const int32 MinimumReleaseCount = CastI32(GetNumberParam(
            Params, TEXT("particle_release_throttling_min_count"), DefaultMinimumReleaseCount));
        const int32 MaximumReleaseCount = CastI32(GetNumberParam(
            Params, TEXT("particle_release_throttling_max_count"), DefaultMaximumReleaseCount));
        const bool bParticleReleaseDisabled = MinimumReleaseCount == INDEX_NONE && MaximumReleaseCount == INDEX_NONE;
        if ((!bParticleReleaseDisabled && (MinimumReleaseCount < 0 || MaximumReleaseCount < 0)) ||
            MinimumReleaseCount > MaximumReleaseCount)
        {
            return LegacyError(
                TEXT("particle release throttling requires min=max=-1 to disable, or 0 <= min <= max"));
        }
        CurrentWork->SDK(TEXT("create_FThrottleLearningSession"),true);
        ActiveThrottleSession = MakeUnique<FThrottleLearningSession>();
        ActiveThrottleSession->Evidence=CurrentWork;
        ActiveThrottleSession->World = World;
        ActiveThrottleSession->SolverActor = SolverActor;
        ActiveThrottleSession->OriginalConfiguration = SolverActor->Properties;
        ActiveThrottleSession->Scenario = Scenario;
        ActiveThrottleSession->bExpectBreakThrottling = Scenario == TEXT("break_throttled");
        ActiveThrottleSession->bExpectParticleReleaseThrottling = Scenario == TEXT("particle_release_throttled");
        if(!BeforeObjectSDK(SolverActor,TEXT("assign_solver_destruction_settings")))return LegacyError(CurrentWork->R.Error);
        if(ActiveThrottleSession&&ActiveThrottleSession->Evidence==CurrentWork)ActiveThrottleSession->bOwnsTemporarySettings=true;
        FChaosSolverDestructionSettings& Settings = SolverActor->Properties.DestructionSettings;
        Settings.PerAdvanceBreaksAllowed = BreaksAllowed;
        Settings.PerAdvanceBreaksRescheduleLimit = RescheduleLimit;
        Settings.ClusteringParticleReleaseThrottlingMinCount = MinimumReleaseCount;
        Settings.ClusteringParticleReleaseThrottlingMaxCount = MaximumReleaseCount;
        Settings.bOptimizeForRuntimeMemory = GetBoolParam(Params, TEXT("optimize_for_runtime_memory"), true);
        ActiveThrottleSession->AppliedSettings = Settings;
        ActiveThrottleSession->CaptureLimitFrames = FMath::Clamp(
            CastI32(GetNumberParam(Params, TEXT("capture_limit_frames"), 300)), 8, 300);
        ActiveThrottleSession->DispatchStrain = CastFloat(
            GetNumberParam(Params, TEXT("strain"), 1000000000000.0));
        ApplySolverPropertyChange(SolverActor);

        TArray<UGeometryCollectionComponent*> ThrottleComponents;
        int32 SolverBoundThrottleComponentCount = 0;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->Tags.Contains(LearningActorTag) || !It->GetName().StartsWith(TEXT("Chaos_Throttle_")))
            {
                continue;
            }
            if (UGeometryCollectionComponent* ThrottleComponent = It->FindComponentByClass<UGeometryCollectionComponent>())
            {
                ThrottleComponents.Add(ThrottleComponent);
                SolverBoundThrottleComponentCount += ThrottleComponent->ChaosSolverActor == SolverActor ? 1 : 0;
            }
        }
        ThrottleComponents.Sort([](const UGeometryCollectionComponent& A, const UGeometryCollectionComponent& B)
        {
            return A.GetOwner()->GetName() < B.GetOwner()->GetName();
        });
        UChaosGameplayEventDispatcher* ThrottleEventDispatcher = SolverActor->GetGameplayEventDispatcher();
        if (!ThrottleEventDispatcher)
        {
            RestoreActiveThrottleSession();
            return LegacyError(
                TEXT("Throttle session requires a Chaos gameplay event dispatcher"));
        }
        ActiveThrottleSession->BreakEventDispatcher = ThrottleEventDispatcher;
        TArray<int32> DispatchItemIndices;
        TArray<int32> ValidDispatchItemIndices;
        TArray<UGeometryCollectionComponent*> DispatchComponents;
        TArray<FString> DispatchTraceDiagnostics;
        for (UGeometryCollectionComponent* ThrottleComponent : ThrottleComponents)
        {
            const FString ComponentActorName = ThrottleComponent->GetOwner()->GetName();
            if(!BeforeObjectSDK(ThrottleComponent,TEXT("SetNotifyBreaks"))) return LegacyError(CurrentWork->R.Error);
            ThrottleComponent->SetNotifyBreaks(true);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            RegisterOwnedBreak(
                ThrottleComponent,
                [ComponentActorName](const FChaosBreakEvent& Event)
                {
                    if (!ActiveThrottleSession || !Event.Component ||
                        !Event.Component->GetOwner() || Event.Component->GetOwner()->GetName() != ComponentActorName)
                    {
                        return;
                    }
                    const uint64 EventFrame = GFrameCounter;
                    ++ActiveThrottleSession->AllBreakEventsPerFrame.FindOrAdd(EventFrame);
                    ++ActiveThrottleSession->TotalGameplayBreakEvents;
                    if (!ActiveThrottleSession->ComponentsWithBreakEvent.Contains(ComponentActorName))
                    {
                        ActiveThrottleSession->ComponentsWithBreakEvent.Add(ComponentActorName);
                        ++ActiveThrottleSession->FirstComponentBreakEventsPerFrame.FindOrAdd(EventFrame);
                    }
                });
            ActiveThrottleSession->RegisteredBreakComponents.Add(ThrottleComponent);
            ActiveThrottleSession->RegisteredBreakComponentCount = ActiveThrottleSession->RegisteredBreakComponents.Num();

            int32 InternalChildCount = 0;
            const int32 ActiveInternalItem = ResolveLargestInternalClusterParentItem(
                ThrottleComponent,
                InternalChildCount,
                DispatchTraceDiagnostics);
            DispatchItemIndices.Add(ActiveInternalItem);
            DispatchTraceDiagnostics.Add(FString::Printf(
                TEXT("actor=%s selected_proxy_parent_item=%d selected_child_count=%d"),
                *ComponentActorName,
                ActiveInternalItem,
                InternalChildCount));
            if (ActiveInternalItem != INDEX_NONE && InternalChildCount > 0)
            {
                DispatchComponents.Add(ThrottleComponent);
                ValidDispatchItemIndices.Add(ActiveInternalItem);
                ActiveThrottleSession->DispatchComponents.Add(ThrottleComponent);
                ActiveThrottleSession->DispatchItemIndices.Add(ActiveInternalItem);
            }
        }
        int32 BaselineActiveCount = 0;
        int32 BaselineDisabledCount = 0;
        int32 BaselineComponentCount = 0;
        CaptureThrottleState(
            World,
            ActiveThrottleSession->InitialRootTransformIndices,
            ActiveThrottleSession->PreviousBrokenSets,
            ActiveThrottleSession->PreviousTransitionedRoots,
            BaselineActiveCount,
            BaselineDisabledCount,
            BaselineComponentCount);
        ActiveThrottleSession->PreviousActiveCount = BaselineActiveCount;
        ActiveThrottleSession->PreviousDisabledPhysicsObjectCount = BaselineDisabledCount;
        ActiveThrottleSession->InitialActiveCount = BaselineActiveCount;
        ActiveThrottleSession->InitialDisabledPhysicsObjectCount = BaselineDisabledCount;
        for (int32 DispatchIndex = 0; DispatchIndex < DispatchComponents.Num(); ++DispatchIndex)
        {
            UGeometryCollectionComponent* ThrottleComponent = DispatchComponents[DispatchIndex];
            if(!BeforeObjectSDK(ThrottleComponent,TEXT("ApplyExternalStrain"))) return LegacyError(CurrentWork->R.Error);
            ThrottleComponent->ApplyExternalStrain(
                ValidDispatchItemIndices[DispatchIndex],
                ThrottleComponent->Bounds.Origin,
                1000.0f,
                10,
                1.0f,
                ActiveThrottleSession->DispatchStrain);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            ++ActiveThrottleSession->DispatchCount;
        }
        ActiveThrottleSession->LastFrame = GFrameCounter;
        ActiveThrottleSession->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([](float)
            {
                return ActiveThrottleSession ? TickOwned(ActiveThrottleSession->Evidence,[]{return CaptureActiveThrottleFrame();}):false;
            }));
        const bool bSettingsApplied = SameDestructionSettings(
            ActiveThrottleSession->AppliedSettings,
            SolverActor->Properties.DestructionSettings);
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("scenario"), ActiveThrottleSession->Scenario);
        Data->SetBoolField(TEXT("expects_break_throttling"), ActiveThrottleSession->bExpectBreakThrottling);
        Data->SetBoolField(TEXT("expects_particle_release_throttling"), ActiveThrottleSession->bExpectParticleReleaseThrottling);
        Data->SetStringField(TEXT("operation_status"), bSettingsApplied && ActiveThrottleSession->DispatchCount == 8 &&
            SolverBoundThrottleComponentCount == 8 ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), FString::Printf(
            TEXT("An internal editor-tick sampler is capturing up to %d consecutive frames; call throttle_finish after capture completion"),
            ActiveThrottleSession->CaptureLimitFrames));
        Data->SetObjectField(TEXT("original"), DestructionSettingsToJson(ActiveThrottleSession->OriginalConfiguration.DestructionSettings));
        Data->SetObjectField(TEXT("applied"), DestructionSettingsToJson(ActiveThrottleSession->AppliedSettings));
        Data->SetBoolField(TEXT("applied_effective_same"), bSettingsApplied);
        Data->SetNumberField(TEXT("destruction_dispatch_count"), ActiveThrottleSession->DispatchCount);
        Data->SetNumberField(TEXT("dispatch_strain"), ActiveThrottleSession->DispatchStrain);
        Data->SetNumberField(TEXT("solver_bound_throttle_component_count"), SolverBoundThrottleComponentCount);
        Data->SetBoolField(TEXT("solver_binding_assertion"), SolverBoundThrottleComponentCount == 8);
        Data->SetArrayField(TEXT("dispatch_item_indices"), IntsToJson(DispatchItemIndices));
        Data->SetArrayField(TEXT("dispatch_trace_diagnostics"), StringsToJson(DispatchTraceDiagnostics));
        Data->SetNumberField(TEXT("baseline_frame"), static_cast<double>(ActiveThrottleSession->LastFrame));
        Data->SetNumberField(TEXT("baseline_active_count"), BaselineActiveCount);
        Data->SetNumberField(TEXT("baseline_disabled_physics_object_count"), BaselineDisabledCount);
        Data->SetNumberField(TEXT("component_count"), BaselineComponentCount);
        Data->SetNumberField(TEXT("capture_limit_frames"), ActiveThrottleSession->CaptureLimitFrames);
        Data->SetBoolField(TEXT("internal_tick_sampler_active"), ActiveThrottleSession->TickerHandle.IsValid());
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        return Success(Data);
    }

    if (Operation == TEXT("throttle_sample") || Operation == TEXT("throttle_finish"))
    {
        if (!ActiveThrottleSession || ActiveThrottleSession->World.Get() != World || !ActiveThrottleSession->SolverActor.IsValid())
        {
            return LegacyError(TEXT("No active throttle session exists in this PIE world; call throttle_begin first"));
        }
        const uint64 CurrentFrame = GFrameCounter;
        const uint64 FrameDelta = CurrentFrame - ActiveThrottleSession->LastFrame;
        const bool bFinish = Operation == TEXT("throttle_finish");
        const bool bAllDispatchesObserved =
            ActiveThrottleSession->TotalNewClusterBreaks == ActiveThrottleSession->DispatchCount;
        const bool bRescheduledNotLost = ActiveThrottleSession->ClusterReleaseTransitionCount >= 4 &&
            ActiveThrottleSession->TotalNewClusterBreaks == ActiveThrottleSession->DispatchCount &&
            ActiveThrottleSession->TotalNewReleasedParticles > ActiveThrottleSession->AppliedSettings.PerAdvanceBreaksAllowed;
        const bool bEnoughExactFrameSamples = ActiveThrottleSession->CapturedFrameCount >= 4 &&
            ActiveThrottleSession->bAllCapturedFrameDeltasExact;
        const bool bScenarioEvidencePass = ActiveThrottleSession->bExpectBreakThrottling
            ? bRescheduledNotLost
            : bAllDispatchesObserved && ActiveThrottleSession->ParticleReleaseTransitionCount > 0;
        const bool bDecisivePass = ActiveThrottleSession->bCaptureCompleted && bEnoughExactFrameSamples && bScenarioEvidencePass &&
            !ActiveThrottleSession->bPerFrameLimitViolated && ActiveThrottleSession->DispatchCount == 8;
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("scenario"), ActiveThrottleSession->Scenario);
        Data->SetBoolField(TEXT("expects_break_throttling"), ActiveThrottleSession->bExpectBreakThrottling);
        Data->SetBoolField(TEXT("expects_particle_release_throttling"), ActiveThrottleSession->bExpectParticleReleaseThrottling);
        Data->SetNumberField(TEXT("current_frame"), static_cast<double>(CurrentFrame));
        Data->SetNumberField(TEXT("frame_delta"), static_cast<double>(FrameDelta));
        Data->SetNumberField(TEXT("active_count"), ActiveThrottleSession->PreviousActiveCount);
        Data->SetNumberField(TEXT("disabled_physics_object_count"), ActiveThrottleSession->PreviousDisabledPhysicsObjectCount);
        Data->SetNumberField(TEXT("component_count"), ActiveThrottleSession->RegisteredBreakComponentCount);
        Data->SetNumberField(TEXT("captured_frame_count"), ActiveThrottleSession->CapturedFrameCount);
        Data->SetNumberField(TEXT("capture_limit_frames"), ActiveThrottleSession->CaptureLimitFrames);
        Data->SetNumberField(TEXT("cluster_release_transition_count"), ActiveThrottleSession->ClusterReleaseTransitionCount);
        Data->SetNumberField(TEXT("particle_release_transition_count"), ActiveThrottleSession->ParticleReleaseTransitionCount);
        Data->SetNumberField(TEXT("total_new_cluster_breaks"), ActiveThrottleSession->TotalNewClusterBreaks);
        Data->SetNumberField(TEXT("total_new_released_particles"), ActiveThrottleSession->TotalNewReleasedParticles);
        Data->SetNumberField(TEXT("total_gameplay_break_events"), ActiveThrottleSession->TotalGameplayBreakEvents);
        Data->SetNumberField(TEXT("reapply_dispatch_count"), ActiveThrottleSession->ReapplyDispatchCount);
        Data->SetNumberField(TEXT("components_with_gameplay_break_event"), ActiveThrottleSession->ComponentsWithBreakEvent.Num());
        Data->SetNumberField(TEXT("maximum_new_cluster_breaks_per_frame"), ActiveThrottleSession->MaximumNewClusterBreaksPerFrame);
        Data->SetNumberField(TEXT("maximum_new_released_particles_per_frame"), ActiveThrottleSession->MaximumNewReleasedParticlesPerFrame);
        Data->SetNumberField(TEXT("per_advance_breaks_allowed"), ActiveThrottleSession->AppliedSettings.PerAdvanceBreaksAllowed);
        Data->SetNumberField(TEXT("per_advance_breaks_reschedule_limit"), ActiveThrottleSession->AppliedSettings.PerAdvanceBreaksRescheduleLimit);
        Data->SetNumberField(TEXT("particle_release_throttling_min_count"), ActiveThrottleSession->AppliedSettings.ClusteringParticleReleaseThrottlingMinCount);
        Data->SetNumberField(TEXT("particle_release_throttling_max_count"), ActiveThrottleSession->AppliedSettings.ClusteringParticleReleaseThrottlingMaxCount);
        Data->SetNumberField(TEXT("initial_active_count"), ActiveThrottleSession->InitialActiveCount);
        Data->SetNumberField(TEXT("initial_disabled_physics_object_count"), ActiveThrottleSession->InitialDisabledPhysicsObjectCount);
        Data->SetNumberField(TEXT("active_count_delta_from_initial"),
            ActiveThrottleSession->PreviousActiveCount - ActiveThrottleSession->InitialActiveCount);
        Data->SetNumberField(TEXT("disabled_physics_object_delta_from_initial"),
            ActiveThrottleSession->PreviousDisabledPhysicsObjectCount - ActiveThrottleSession->InitialDisabledPhysicsObjectCount);
        Data->SetBoolField(TEXT("per_frame_release_delta_available"), ActiveThrottleSession->CapturedFrameCount > 0);
        Data->SetBoolField(TEXT("all_captured_frame_deltas_exact"), ActiveThrottleSession->bAllCapturedFrameDeltasExact);
        Data->SetStringField(TEXT("cluster_transition_evidence_source"),
            TEXT("per_actor_enabled_physics_object_count_first_transition_from_one_to_more_than_one"));
        Data->SetStringField(TEXT("released_particle_evidence_source"),
            TEXT("per_transform_dynamic_has_broken_off_set_delta"));
        Data->SetBoolField(TEXT("per_frame_limit_violated"), ActiveThrottleSession->bPerFrameLimitViolated);
        Data->SetBoolField(TEXT("capture_completed"), ActiveThrottleSession->bCaptureCompleted);
        Data->SetStringField(TEXT("completion_reason"), ActiveThrottleSession->CompletionReason);
        Data->SetBoolField(TEXT("all_destruction_dispatches_observed"), bAllDispatchesObserved);
        Data->SetBoolField(TEXT("rescheduled_not_lost"), bRescheduledNotLost);
        Data->SetBoolField(TEXT("scenario_evidence_pass"), bScenarioEvidencePass);
        Data->SetArrayField(TEXT("samples"), ActiveThrottleSession->Samples);
        if (!bFinish || !ActiveThrottleSession->bCaptureCompleted)
        {
            Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
            Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
            Data->SetStringField(TEXT("learning_blocker"), TEXT("Internal tick capture is still active; throttle_finish only retrieves a completed bounded timeline"));
            Data->SetBoolField(TEXT("requires_followup_sample"), true);
            return Success(Data);
        }

        const bool bRestoredSame = ActiveThrottleSession->bSettingsRestored;
        Data->SetStringField(TEXT("operation_status"), bRestoredSame ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("assertion_status"), bDecisivePass && bRestoredSame
            ? TEXT("PASS") : (bRestoredSame ? TEXT("FAIL") : TEXT("FAIL")));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), bDecisivePass
            ? TEXT("Scenario capture passed; compare unthrottled_baseline, break_throttled, and particle_release_throttled over five fresh-PIE runs")
            : TEXT("Exact one-frame samples and all eight fixture transitions are required for the selected throttling scenario"));
        Data->SetObjectField(TEXT("original"),
            DestructionSettingsToJson(ActiveThrottleSession->OriginalConfiguration.DestructionSettings));
        Data->SetObjectField(TEXT("applied"),
            DestructionSettingsToJson(ActiveThrottleSession->AppliedSettings));
        Data->SetObjectField(TEXT("restored"),
            DestructionSettingsToJson(ActiveThrottleSession->SolverActor->Properties.DestructionSettings));
        Data->SetBoolField(TEXT("settings_restored"), bRestoredSame);
        ActiveThrottleSession.Reset();
        return Success(Data);
    }

    if (Operation == TEXT("measure_throttling"))
    {
        const FString SolverName = GetStringParam(Params, TEXT("solver_actor_name"), TEXT(""));
        AChaosSolverActor* SolverActor = FindActor<AChaosSolverActor>(World, SolverName);
        if (!SolverActor || !SolverActor->Tags.Contains(LearningActorTag))
        {
            return LegacyError(TEXT("Tagged Chaos learning solver actor not found"));
        }
        const int32 MinimumReleaseCount = CastI32(GetNumberParam(Params, TEXT("particle_release_throttling_min_count"), 64));
        const int32 MaximumReleaseCount = CastI32(GetNumberParam(Params, TEXT("particle_release_throttling_max_count"), 128));
        if (MinimumReleaseCount < 0 || MaximumReleaseCount < 0 || MinimumReleaseCount > MaximumReleaseCount)
        {
            return LegacyError(TEXT("particle release throttling requires 0 <= min <= max"));
        }
        const FChaosSolverConfiguration BeforeConfiguration = SolverActor->Properties;
        FChaosSolverConfiguration AppliedFullConfiguration=BeforeConfiguration;
        const FChaosSolverDestructionSettings BeforeSettings = BeforeConfiguration.DestructionSettings;
        FChaosSolverDestructionSettings AppliedSettings = BeforeSettings;
        FChaosSolverDestructionSettings EffectiveSettings = BeforeSettings;
        TSharedPtr<FJsonObject> FrameEvidence;
        int32 DestructionDispatchCount = 0;
        {
            ON_SCOPE_EXIT
            {
                TGuardValue<bool> RestoreAdmission(Restoring,true);
                if(Check()&&FChaosSolverConfiguration::StaticStruct()->CompareScriptStruct(&AppliedFullConfiguration,&SolverActor->Properties,0))
                {
                    if(BeforeObjectSDK(SolverActor,TEXT("restore_synchronous_solver_configuration")))
                    {
                        SolverActor->Properties=BeforeConfiguration;ApplySolverPropertyChange(SolverActor);AfterSDK();
                    }
                }
                else if(CurrentWork)CurrentWork->R.Data->SetBoolField(TEXT("measure_restore_external_change_conflict"),true);
            };
            if(!BeforeObjectSDK(SolverActor,TEXT("Modify"))) return LegacyError(CurrentWork->R.Error);
            SolverActor->Modify();
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            if(!BeforeObjectSDK(SolverActor,TEXT("assign_solver_destruction_settings")))return LegacyError(CurrentWork->R.Error);
        FChaosSolverDestructionSettings& Settings = SolverActor->Properties.DestructionSettings;
            Settings.PerAdvanceBreaksAllowed = FMath::Max(0, CastI32(GetNumberParam(Params, TEXT("per_advance_breaks_allowed"), 2)));
            Settings.PerAdvanceBreaksRescheduleLimit = FMath::Max(0, CastI32(GetNumberParam(Params, TEXT("per_advance_breaks_reschedule_limit"), 64)));
            Settings.ClusteringParticleReleaseThrottlingMinCount = MinimumReleaseCount;
            Settings.ClusteringParticleReleaseThrottlingMaxCount = MaximumReleaseCount;
            Settings.bOptimizeForRuntimeMemory = GetBoolParam(Params, TEXT("optimize_for_runtime_memory"), true);
            AppliedSettings = Settings;
            AppliedFullConfiguration=SolverActor->Properties;
            ApplySolverPropertyChange(SolverActor);
            EffectiveSettings = SolverActor->Properties.DestructionSettings;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (!It->Tags.Contains(LearningActorTag) || !It->GetName().StartsWith(TEXT("Chaos_Throttle_")))
                {
                    continue;
                }
                if (UGeometryCollectionComponent* ThrottleComponent = It->FindComponentByClass<UGeometryCollectionComponent>())
                {
                    if(!BeforeObjectSDK(ThrottleComponent,TEXT("ApplyExternalStrain"))) return LegacyError(CurrentWork->R.Error);
                    ThrottleComponent->ApplyExternalStrain(0, ThrottleComponent->Bounds.Origin, 1000.0f, 10, 1.0f, 1000000.0f);
                    if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
                    ++DestructionDispatchCount;
                }
            }
            FrameEvidence = RuntimeFrameEvidenceForPrefix(World, TEXT("Chaos_Throttle_"));
        }
        const bool bRestoredSame = SameDestructionSettings(BeforeSettings, SolverActor->Properties.DestructionSettings);
        const bool bAppliedEffectiveSame = SameDestructionSettings(AppliedSettings, EffectiveSettings);
        const bool bFrameEvidenceAvailable = FrameEvidence.IsValid() && FrameEvidence->GetBoolField(TEXT("dynamic_collection_available"));
        const bool bPerFrameDeltaAvailable = FrameEvidence.IsValid() && FrameEvidence->GetBoolField(TEXT("per_frame_release_delta_available"));
        const bool bThrottlingAssertion = bRestoredSame && bAppliedEffectiveSame && bFrameEvidenceAvailable &&
            DestructionDispatchCount == 8 && bPerFrameDeltaAvailable;
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation_status"), bRestoredSame && bAppliedEffectiveSame ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("assertion_status"), bThrottlingAssertion
            ? TEXT("PASS")
            : (bRestoredSame && bAppliedEffectiveSame ? TEXT("BLOCKED") : TEXT("FAIL")));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Eight destruction requests were dispatched under the RAII settings, but fixed-tick release-set deltas and a five-run A/B aggregate are not available in this synchronous call"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("solver_actor_path"), SolverActor->GetPathName());
        Data->SetObjectField(TEXT("before"), DestructionSettingsToJson(BeforeSettings));
        Data->SetObjectField(TEXT("applied"), DestructionSettingsToJson(AppliedSettings));
        Data->SetObjectField(TEXT("effective"), DestructionSettingsToJson(EffectiveSettings));
        Data->SetObjectField(TEXT("restored"), DestructionSettingsToJson(SolverActor->Properties.DestructionSettings));
        Data->SetBoolField(TEXT("restored_same"), bRestoredSame);
        Data->SetBoolField(TEXT("applied_effective_same"), bAppliedEffectiveSame);
        Data->SetBoolField(TEXT("frame_evidence_available"), bFrameEvidenceAvailable);
        Data->SetBoolField(TEXT("per_frame_release_delta_available"), bPerFrameDeltaAvailable);
        Data->SetNumberField(TEXT("destruction_dispatch_count"), DestructionDispatchCount);
        Data->SetObjectField(TEXT("frame_evidence"), FrameEvidence.IsValid() ? FrameEvidence : MakeShared<FJsonObject>());
        Data->SetBoolField(TEXT("cvars_touched"), false);
        return Success(Data);
    }

    const FString ActorName = GetStringParam(Params, TEXT("actor_name"), TEXT(""));
    UGeometryCollectionComponent* Component = FindGeometryCollectionComponent(World, ActorName);
    if (!Component || !Component->GetRestCollection())
    {
        return LegacyError(TEXT("Geometry Collection component not found on actor"));
    }
    if (!IsWithinLearningRoot(Component->GetRestCollection()->GetPathName()) || !Component->GetOwner()->Tags.Contains(LearningActorTag))
    {
        return LegacyError(TEXT("Destruction smoke requires a tagged learning actor using a temp-root Rest Collection"));
    }

    if (Operation == TEXT("trace_field_benchmark"))
    {
        FVector Start;
        FVector End;
        if (!TryGetVectorParam(Params, TEXT("start"), Start) || !TryGetVectorParam(Params, TEXT("end"), End))
        {
            return LegacyError(TEXT("start and end must be [X,Y,Z] arrays"));
        }
        const bool bExpectedHit = GetBoolParam(Params, TEXT("expected_hit"), true);
        const int32 TraceCount = FMath::Clamp(CastI32(GetNumberParam(Params, TEXT("trace_count"), 100)), 20, 1000);
        FString BenchmarkPath = GetStringParam(Params, TEXT("benchmark_path"), TEXT("both"));
        BenchmarkPath.ToLowerInline();
        const bool bRunTrace = BenchmarkPath == TEXT("both") || BenchmarkPath == TEXT("trace_only");
        const bool bRunField = BenchmarkPath == TEXT("both") || BenchmarkPath == TEXT("field_only");
        if (!bRunTrace && !bRunField)
        {
            return LegacyError(
                TEXT("benchmark_path must be both, trace_only, or field_only"));
        }
        int32 BlockingHitCount = 0;
        int32 RequestedComponentHitCount = 0;
        int32 ValidItemHitCount = 0;
        FHitResult LastHit;
        TArray<AActor*> IgnoredActors;
        if (AActor* FloorActor = FindActor<AActor>(World, TEXT("Chaos_StaticFloor"))) IgnoredActors.Add(FloorActor);
        if (AActor* Witness0 = FindActor<AActor>(World, TEXT("Chaos_OneWay_Witness_0"))) IgnoredActors.Add(Witness0);
        if (AActor* Witness1 = FindActor<AActor>(World, TEXT("Chaos_OneWay_Witness_1"))) IgnoredActors.Add(Witness1);
        bool bUsedDirectComponentTrace = false;
        const double TraceStartSeconds = FPlatformTime::Seconds();
        for (int32 Index = 0; bRunTrace && Index < TraceCount; ++Index)
        {
            FHitResult Hit;
            FTargetTraceDiagnostics Diagnostics;
            const bool bHit = TraceTargetGeometryCollection(
                World, Component, Start, End, GetBoolParam(Params, TEXT("trace_complex"), true),
                IgnoredActors, Hit, Diagnostics);
            BlockingHitCount += Diagnostics.BlockingHitCount;
            RequestedComponentHitCount += Diagnostics.TargetComponentHitCount;
            ValidItemHitCount += bHit && Hit.Item != INDEX_NONE ? 1 : 0;
            bUsedDirectComponentTrace |= Diagnostics.bUsedDirectComponentTrace;
            if (bHit)
            {
                LastHit = Hit;
            }
        }
        const double TraceElapsedMilliseconds = bRunTrace
            ? (FPlatformTime::Seconds() - TraceStartSeconds) * 1000.0 : 0.0;
        const FVector FieldPosition = LastHit.bBlockingHit ? LastHit.ImpactPoint : (Start + End) * 0.5;
        const float Radius = CastFloat(GetNumberParam(Params, TEXT("radius"), 100.0));
        const float Strain = CastFloat(GetNumberParam(Params, TEXT("strain"), 100000.0));
        int32 FieldDispatchCount = 0;
        const double FieldStartSeconds = FPlatformTime::Seconds();
        for (int32 Index = 0; bRunField && Index < TraceCount; ++Index)
        {
            URadialFalloff* Field = NewObject<URadialFalloff>(GetTransientPackage());
            if(!BeforeObjectSDK(Field,TEXT("SetRadialFalloff"))) return LegacyError(CurrentWork->R.Error);
            Field->SetRadialFalloff(Strain, 0.0f, 1.0f, 0.0f, Radius, FieldPosition, EFieldFalloffType::Field_FallOff_None);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            if(!BeforeObjectSDK(Component,TEXT("ApplyPhysicsField"))) return LegacyError(CurrentWork->R.Error);
            Component->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, Field);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            ++FieldDispatchCount;
        }
        const double FieldElapsedMilliseconds = bRunField
            ? (FPlatformTime::Seconds() - FieldStartSeconds) * 1000.0 : 0.0;
        const bool bExpectedResult = !bRunTrace || (bExpectedHit ? ValidItemHitCount >= 20 : BlockingHitCount == 0);
        const bool bBenchmarkAssertions = bExpectedResult && (!bRunField || FieldDispatchCount == TraceCount);
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("operation_status"), bBenchmarkAssertions ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("assertion_status"), bBenchmarkAssertions ? TEXT("PASS") : TEXT("FAIL"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("One 100-operation benchmark is not the required five-run A/B aggregate"));
        Data->SetStringField(TEXT("benchmark_path"), BenchmarkPath);
        Data->SetBoolField(TEXT("trace_path_executed"), bRunTrace);
        Data->SetBoolField(TEXT("field_path_executed"), bRunField);
        Data->SetBoolField(TEXT("expected_hit"), bExpectedHit);
        Data->SetNumberField(TEXT("trace_count"), TraceCount);
        Data->SetNumberField(TEXT("blocking_hit_count"), BlockingHitCount);
        Data->SetNumberField(TEXT("requested_component_hit_count"), RequestedComponentHitCount);
        Data->SetNumberField(TEXT("valid_item_hit_count"), ValidItemHitCount);
        Data->SetBoolField(TEXT("used_direct_component_trace"), bUsedDirectComponentTrace);
        Data->SetBoolField(TEXT("twenty_valid_hit_assertion"), ValidItemHitCount >= 20);
        Data->SetBoolField(TEXT("expected_result_assertion"), bExpectedResult);
        Data->SetStringField(TEXT("trace_timing_operation"), TEXT("target_geometry_collection_line_trace"));
        Data->SetStringField(TEXT("field_timing_operation"), TEXT("Chaos_ExternalClusterStrain_radial_field_dispatch"));
        Data->SetStringField(TEXT("timing_scope"), TEXT("game_thread_command_submission_only"));
        Data->SetNumberField(TEXT("line_trace_elapsed_ms"), TraceElapsedMilliseconds);
        Data->SetNumberField(TEXT("line_trace_mean_us"), bRunTrace ? TraceElapsedMilliseconds * 1000.0 / TraceCount : 0.0);
        Data->SetNumberField(TEXT("trace_elapsed_ms"), TraceElapsedMilliseconds);
        Data->SetNumberField(TEXT("field_dispatch_count"), FieldDispatchCount);
        Data->SetNumberField(TEXT("field_chaos_dispatch_elapsed_ms"), FieldElapsedMilliseconds);
        Data->SetNumberField(TEXT("field_chaos_dispatch_mean_us"), bRunField && FieldDispatchCount > 0
            ? FieldElapsedMilliseconds * 1000.0 / FieldDispatchCount : 0.0);
        Data->SetNumberField(TEXT("field_elapsed_ms"), FieldElapsedMilliseconds);
        Data->SetBoolField(TEXT("trace_vs_field_timing_available"), true);
        Data->SetBoolField(TEXT("requires_followup_sample"), true);
        Data->SetNumberField(TEXT("dispatch_world_time_seconds"), World->GetTimeSeconds());
        AddComponentQueryDiagnostics(Data, Component);
        return Success(Data);
    }

    if (Operation == TEXT("sample"))
    {
        TSharedPtr<FJsonObject> Data = InspectCollection(Component->GetRestCollection(), Component, 0.04);
        Data->SetStringField(TEXT("operation_status"), Component->GetDynamicCollection() ? TEXT("PASS") : TEXT("BLOCKED"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("A single sample cannot satisfy the required five-run A/B numeric gate"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("world_context"), WorldContext);
        Data->SetNumberField(TEXT("world_time_seconds"), World->GetTimeSeconds());
        Data->SetNumberField(TEXT("frame_counter"), static_cast<double>(GFrameCounter));
        const int32 ItemIndex = CastI32(GetNumberParam(Params, TEXT("item_index"), INDEX_NONE));
        FVector ActualVelocity = FVector::ZeroVector;
        bool bVelocityAvailable = false;
        int32 VelocitySampleCount = 0;
        TArray<int32> ResolvedTransformIndices = ResolveTraceItemTransformIndices(Component, ItemIndex);
        bool bUsedDispatchSnapshot = false;
        FTraceItemDispatchSnapshot* DispatchSnapshot =
            TraceItemDispatchSnapshots.Find(MakeTraceItemSnapshotKey(Component, ItemIndex));
        const FGeometryCollectionItemIndex DecodedItem = FGeometryCollectionItemIndex::CreateFromExistingItemIndex(ItemIndex);
        if (ResolvedTransformIndices.IsEmpty())
        {
            if (DispatchSnapshot && DispatchSnapshot->World.Get() == World && DispatchSnapshot->Component.Get() == Component)
            {
                ResolvedTransformIndices = DispatchSnapshot->TransformIndices;
                bUsedDispatchSnapshot = !ResolvedTransformIndices.IsEmpty();
            }
        }
        if (FGeometryCollectionPhysicsProxy* Proxy = Component->GetPhysicsProxy())
        {
            TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects;
            for (int32 TransformIndex : ResolvedTransformIndices)
            {
                if (Chaos::FPhysicsObjectHandle PhysicsObject = Proxy->GetPhysicsObjectByIndex(TransformIndex))
                {
                    PhysicsObjects.Add(PhysicsObject);
                }
            }
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
                    ActualVelocity += PhysicsObjectInterface->GetV(PhysicsObject);
                    ++VelocitySampleCount;
                }
                if (VelocitySampleCount > 0)
                {
                    ActualVelocity /= static_cast<double>(VelocitySampleCount);
                    bVelocityAvailable = true;
                }
            }
        }
        if (!bVelocityAvailable && !DecodedItem.IsInternalCluster())
        {
            if (FGeometryDynamicCollection* Dynamic = Component->GetDynamicCollection())
            {
                if (const TManagedArray<FVector3f>* Velocities = Dynamic->GetLinearVelocitiesAttribute();
                    Velocities && Velocities->IsValidIndex(ItemIndex))
                {
                    ActualVelocity = FVector((*Velocities)[ItemIndex]);
                    VelocitySampleCount = 1;
                    bVelocityAvailable = true;
                }
            }
        }
        FVector ExpectedDirection = FVector::ZeroVector;
        TryGetVectorParam(Params, TEXT("expected_direction"), ExpectedDirection);
        const double DirectionDot = bVelocityAvailable && !ActualVelocity.IsNearlyZero() && !ExpectedDirection.IsNearlyZero()
            ? FVector::DotProduct(ActualVelocity.GetSafeNormal(), ExpectedDirection.GetSafeNormal()) : 0.0;
        int32 FollowupVelocityDispatchCount = 0;
        int32 DeferredVelocityDispatchAttemptCount = 0;
        uint64 DeferredVelocityDispatchFrame = 0;
        bool bDeferredVelocityObserved = false;
        bool bDeferredVelocityDispatchComplete = false;
        if (DispatchSnapshot && DispatchSnapshot->World.Get() == World && DispatchSnapshot->Component.Get() == Component)
        {
            FollowupVelocityDispatchCount = DispatchSnapshot->DirectVelocityAppliedCount;
            DeferredVelocityDispatchAttemptCount = DispatchSnapshot->DeferredVelocityDispatchAttemptCount;
            DeferredVelocityDispatchFrame = DispatchSnapshot->DeferredVelocityDispatchFrame;
            bDeferredVelocityObserved = DispatchSnapshot->bDeferredVelocityObserved;
            bDeferredVelocityDispatchComplete = DispatchSnapshot->bDeferredVelocityDispatchComplete;
        }
        const double DispatchTime = GetNumberParam(Params, TEXT("dispatch_world_time_seconds"), -1.0);
        const bool bPhysicsTickObserved = DispatchTime >= 0.0 && World->GetTimeSeconds() > DispatchTime;
        const bool bTraceSampleRequested = ItemIndex != INDEX_NONE || DispatchTime >= 0.0 || !ExpectedDirection.IsNearlyZero();
        const bool bVelocityDirectionAssertion = bPhysicsTickObserved && bVelocityAvailable && DirectionDot > 0.5;
        if (bTraceSampleRequested)
        {
            Data->SetStringField(TEXT("assertion_status"), !bDeferredVelocityDispatchComplete
                ? TEXT("BLOCKED") : (bVelocityDirectionAssertion ? TEXT("PASS") : TEXT("FAIL")));
        }
        Data->SetNumberField(TEXT("sampled_item_index"), ItemIndex);
        Data->SetStringField(TEXT("sampled_item_kind"), !DecodedItem.IsValid()
            ? TEXT("invalid") : (DecodedItem.IsInternalCluster() ? TEXT("internal_cluster") : TEXT("transform")));
        Data->SetNumberField(TEXT("sampled_internal_cluster_id"), DecodedItem.IsInternalCluster() ? DecodedItem.GetInternalClusterIndex() : INDEX_NONE);
        Data->SetArrayField(TEXT("sampled_child_transform_indices"), IntsToJson(ResolvedTransformIndices));
        Data->SetBoolField(TEXT("used_dispatch_item_snapshot"), bUsedDispatchSnapshot);
        Data->SetNumberField(TEXT("velocity_sample_count"), VelocitySampleCount);
        Data->SetNumberField(TEXT("followup_velocity_dispatch_count"), FollowupVelocityDispatchCount);
        Data->SetNumberField(TEXT("deferred_velocity_dispatch_attempt_count"), DeferredVelocityDispatchAttemptCount);
        Data->SetNumberField(TEXT("deferred_velocity_dispatch_frame"), static_cast<double>(DeferredVelocityDispatchFrame));
        Data->SetBoolField(TEXT("deferred_velocity_observed"), bDeferredVelocityObserved);
        Data->SetBoolField(TEXT("deferred_velocity_dispatch_complete"), bDeferredVelocityDispatchComplete);
        Data->SetBoolField(TEXT("requires_followup_sample"), !bDeferredVelocityDispatchComplete);
        Data->SetBoolField(TEXT("actual_velocity_available"), bVelocityAvailable);
        Data->SetArrayField(TEXT("actual_velocity"), VectorToJson(ActualVelocity));
        Data->SetNumberField(TEXT("actual_velocity_direction_dot"), DirectionDot);
        Data->SetBoolField(TEXT("physics_tick_observed"), bPhysicsTickObserved);
        Data->SetBoolField(TEXT("trace_sample_assertion_evaluated"), bTraceSampleRequested);
        Data->SetBoolField(TEXT("velocity_direction_assertion"), bVelocityDirectionAssertion);
        TSharedPtr<FJsonObject> RemovalTimeline = MakeShared<FJsonObject>();
        RemovalTimeline->SetNumberField(TEXT("timer_seconds"), World->GetTimeSeconds());
        RemovalTimeline->SetNumberField(TEXT("frame_counter"), static_cast<double>(GFrameCounter));
        RemovalTimeline->SetNumberField(TEXT("break_event_proxy_count"), Data->HasField(TEXT("broken_transform_count")) ? Data->GetNumberField(TEXT("broken_transform_count")) : 0.0);
        RemovalTimeline->SetNumberField(TEXT("disabled_count"), Data->HasField(TEXT("disabled_physics_object_count")) ? Data->GetNumberField(TEXT("disabled_physics_object_count")) : 0.0);
        RemovalTimeline->SetNumberField(TEXT("hidden_count"), Data->HasField(TEXT("hidden_transform_count")) ? Data->GetNumberField(TEXT("hidden_transform_count")) : 0.0);
        RemovalTimeline->SetNumberField(TEXT("remaining_count"), Data->HasField(TEXT("remaining_visible_rigid_count")) ? Data->GetNumberField(TEXT("remaining_visible_rigid_count")) : 0.0);
        RemovalTimeline->SetBoolField(TEXT("sleep_only"), Component->bAllowRemovalOnSleep && !Component->bAllowRemovalOnBreak);
        RemovalTimeline->SetBoolField(TEXT("break_only"), !Component->bAllowRemovalOnSleep && Component->bAllowRemovalOnBreak);
        Data->SetObjectField(TEXT("removal_timeline_sample"), RemovalTimeline);
        return Success(Data);
    }

    if (Operation != TEXT("trace_external_strain"))
    {
        return LegacyError(FString::Printf(TEXT("Unsupported destruction smoke operation: %s"), *Operation));
    }

    FVector Start;
    FVector End;
    if (!TryGetVectorParam(Params, TEXT("start"), Start) || !TryGetVectorParam(Params, TEXT("end"), End))
    {
        return LegacyError(TEXT("start and end must be [X,Y,Z] arrays"));
    }
    FHitResult Hit;
    FTargetTraceDiagnostics TraceDiagnostics;
    TArray<AActor*> IgnoredActors;
    if (AActor* FloorActor = FindActor<AActor>(World, TEXT("Chaos_StaticFloor")))
    {
        IgnoredActors.Add(FloorActor);
    }
    if (AActor* Witness0 = FindActor<AActor>(World, TEXT("Chaos_OneWay_Witness_0")))
    {
        IgnoredActors.Add(Witness0);
    }
    if (AActor* Witness1 = FindActor<AActor>(World, TEXT("Chaos_OneWay_Witness_1")))
    {
        IgnoredActors.Add(Witness1);
    }
    const bool bHit = TraceTargetGeometryCollection(
        World,
        Component,
        Start,
        End,
        GetBoolParam(Params, TEXT("trace_complex"), true),
        IgnoredActors,
        Hit,
        TraceDiagnostics);
    const bool bExpectedHit = GetBoolParam(Params, TEXT("expected_hit"), true);
    UGeometryCollectionComponent* HitComponent = Cast<UGeometryCollectionComponent>(Hit.GetComponent());
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("operation"), Operation);
    Data->SetStringField(TEXT("world_context"), WorldContext);
    Data->SetBoolField(TEXT("blocking_hit"), bHit);
    Data->SetBoolField(TEXT("expected_hit"), bExpectedHit);
    Data->SetNumberField(TEXT("item_index"), Hit.Item);
    Data->SetNumberField(TEXT("face_index"), Hit.FaceIndex);
    Data->SetArrayField(TEXT("impact_point"), VectorToJson(Hit.ImpactPoint));
    Data->SetStringField(TEXT("hit_actor_path"), Hit.GetActor() ? Hit.GetActor()->GetPathName() : FString());
    Data->SetStringField(TEXT("hit_component_path"), HitComponent ? HitComponent->GetPathName() : FString());
    Data->SetBoolField(TEXT("hit_requested_component"), HitComponent == Component);
    AddTraceDiagnostics(Data, TraceDiagnostics);
    AddComponentQueryDiagnostics(Data, Component);
    if (!bExpectedHit && !bHit)
    {
        Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("PASS"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("Expected miss passed, but the full hit/field five-run suite remains"));
        Data->SetBoolField(TEXT("expected_result_assertion"), true);
        Data->SetBoolField(TEXT("external_strain_dispatched"), false);
        return Success(Data);
    }
    if (!bExpectedHit || !bHit || HitComponent != Component || Hit.Item == INDEX_NONE)
    {
        Data->SetStringField(TEXT("operation_status"), TEXT("FAIL"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("FAIL"));
        Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("learning_blocker"), TEXT("External strain dispatch failed before an A/B measurement could be collected"));
        Data->SetStringField(TEXT("failure"), TEXT("Trace did not return the requested Geometry Collection component with a valid FHitResult.Item"));
        return Success(Data);
    }

    const float Radius = CastFloat(GetNumberParam(Params, TEXT("radius"), 100.0));
    const float Strain = CastFloat(GetNumberParam(Params, TEXT("strain"), 100000.0));
    const int32 PropagationDepth = CastI32(GetNumberParam(Params, TEXT("propagation_depth"), 1));
    const float PropagationFactor = CastFloat(GetNumberParam(Params, TEXT("propagation_factor"), 0.5));
    FVector Velocity = (End - Start).GetSafeNormal() * GetNumberParam(Params, TEXT("breaking_velocity"), 800.0);
    TryGetVectorParam(Params, TEXT("breaking_velocity_vector"), Velocity);
    CaptureTraceItemDispatchSnapshot(World, Component, Hit.Item, Velocity);
    if(!BeforeObjectSDK(Component,TEXT("ApplyExternalStrain"))) return LegacyError(CurrentWork->R.Error);
    Component->ApplyExternalStrain(Hit.Item, Hit.ImpactPoint, Radius, PropagationDepth, PropagationFactor, Strain);
    if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
    if(!BeforeObjectSDK(Component,TEXT("ApplyBreakingLinearVelocity"))) return LegacyError(CurrentWork->R.Error);
    Component->ApplyBreakingLinearVelocity(Hit.Item, Velocity);
    if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
    int32 LeafLinearVelocityDispatchCount = 0;
    if (const FTraceItemDispatchSnapshot* Snapshot = TraceItemDispatchSnapshots.Find(MakeTraceItemSnapshotKey(Component, Hit.Item)))
    {
        for (int32 TransformIndex : Snapshot->TransformIndices)
        {
            if(!BeforeObjectSDK(Component,TEXT("ApplyLinearVelocity"))) return LegacyError(CurrentWork->R.Error);
            Component->ApplyLinearVelocity(TransformIndex, Velocity);
            if(!AfterSDK()) return LegacyError(CurrentWork->R.Error);
            ++LeafLinearVelocityDispatchCount;
        }
    }
    StartDeferredTraceVelocityDispatch(Component, Hit.Item);
    Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
    Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
    Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
    Data->SetStringField(TEXT("learning_blocker"), TEXT("External strain was dispatched, but follow-up sampling and five-run A/B validation remain"));
    Data->SetBoolField(TEXT("external_strain_dispatched"), true);
    Data->SetNumberField(TEXT("strain"), Strain);
    Data->SetNumberField(TEXT("radius"), Radius);
    Data->SetArrayField(TEXT("breaking_velocity"), VectorToJson(Velocity));
    Data->SetNumberField(TEXT("leaf_linear_velocity_dispatch_count"), LeafLinearVelocityDispatchCount);
    Data->SetNumberField(TEXT("direct_velocity_applied_count"), 0);
    Data->SetBoolField(TEXT("deferred_velocity_ticker_started"), true);
    Data->SetBoolField(TEXT("requires_followup_sample"), true);
    Data->SetNumberField(TEXT("dispatch_world_time_seconds"), World->GetTimeSeconds());
    Data->SetArrayField(TEXT("expected_direction"), VectorToJson(Velocity.GetSafeNormal()));
    Data->SetBoolField(TEXT("cvars_touched"), false);
    return Success(Data);
}
bool ValidateNumericInputs(const TSharedPtr<FJsonObject>&P,const FString&Op)
{
 TArray<const TCHAR*> Ints,Floats,Vectors;
 if(Op==TEXT("performance_begin")){Ints={TEXT("expected_component_count"),TEXT("capture_limit_frames")};Floats={TEXT("live_external_strain")};}
 else if(Op==TEXT("one_way_configure")||Op==TEXT("one_way_run")||Op==TEXT("one_way_sample")){Ints={TEXT("one_way_level")};if(Op==TEXT("one_way_run")){Floats={TEXT("strain"),TEXT("breaking_velocity"),TEXT("ballistic_lift_velocity")};Vectors={TEXT("start"),TEXT("end"),TEXT("breaking_velocity_vector")};}else if(Op==TEXT("one_way_configure"))Vectors={TEXT("reset_witness_location")};}
 else if(Op==TEXT("removal_begin"))Ints={TEXT("capture_limit_frames")};
 else if(Op==TEXT("throttle_begin")||Op==TEXT("measure_throttling")){Ints={TEXT("per_advance_breaks_allowed"),TEXT("per_advance_breaks_reschedule_limit"),TEXT("particle_release_throttling_min_count"),TEXT("particle_release_throttling_max_count")};if(Op==TEXT("throttle_begin")){Ints.Add(TEXT("capture_limit_frames"));Floats={TEXT("strain")};}}
 else if(Op==TEXT("trace_field_benchmark")){Ints={TEXT("trace_count")};Floats={TEXT("strain"),TEXT("radius")};Vectors={TEXT("start"),TEXT("end")};}
 else if(Op==TEXT("sample")){Ints={TEXT("item_index")};Vectors={TEXT("expected_direction"),TEXT("initial_witness_location"),TEXT("initial_witness_velocity")};}
 else if(Op==TEXT("trace_external_strain")){Ints={TEXT("propagation_depth")};Floats={TEXT("radius"),TEXT("strain"),TEXT("propagation_factor"),TEXT("breaking_velocity")};Vectors={TEXT("start"),TEXT("end"),TEXT("breaking_velocity_vector")};}
 for(const TCHAR*K:Ints)if(P->HasField(K))CastI32(GetNumberParam(P,K,0));
 for(const TCHAR*K:Floats)if(P->HasField(K))CastFloat(GetNumberParam(P,K,0));
 for(const TCHAR*K:Vectors){FVector V;if(TryGetVectorParam(P,K,V)&&!FMath::IsFinite(V.SizeSquared()))CurrentWork->Fail(TEXT("vector_range"),FString::Printf(TEXT("%s overflows vector arithmetic"),K));}
 return CurrentWork->R.Error.IsEmpty();
}


bool HasPendingWork(const FWork*Work)
{
 if(!Work)return false;
 if(!Work->NotifyTouched.IsEmpty())return true;
 if(ActiveThrottleSession&&ActiveThrottleSession->Evidence.Get()==Work&&ActiveThrottleSession->bOwnsTemporarySettings&&!ActiveThrottleSession->bSettingsRestored)return true;
 if(ActiveRemovalSession&&ActiveRemovalSession->Evidence.Get()==Work&&((ActiveRemovalSession->bGravityOverridden&&!ActiveRemovalSession->bGravityRestored)||(!ActiveRemovalSession->CollisionIsolatedTargetIndices.IsEmpty()&&!ActiveRemovalSession->bTargetCollisionRestored)))return true;
 if(ActiveOneWayStagedLaunch&&ActiveOneWayStagedLaunch->Evidence.Get()==Work&&!ActiveOneWayStagedLaunch->CollisionIsolatedSiblingIndices.IsEmpty()&&!ActiveOneWayStagedLaunch->bSiblingCollisionRestored)return true;
 if(ActiveChaosPerformanceSession&&ActiveChaosPerformanceSession->Evidence.Get()==Work&&!ActiveChaosPerformanceSession->bCaptureCompleted)return true;
 if(ActiveOneWayStagedLaunch&&ActiveOneWayStagedLaunch->Evidence.Get()==Work&&!ActiveOneWayStagedLaunch->bLaunchCompleted)return true;
 if(ActiveOneWayCollisionCapture&&ActiveOneWayCollisionCapture->Evidence.Get()==Work&&!ActiveOneWayCollisionCapture->bCaptureCompleted)return true;
 if(ActiveRemovalSession&&ActiveRemovalSession->Evidence.Get()==Work&&!ActiveRemovalSession->bCaptureCompleted)return true;
 if(ActiveThrottleSession&&ActiveThrottleSession->Evidence.Get()==Work&&!ActiveThrottleSession->bCaptureCompleted)return true;
 for(const auto&Pair:TraceItemDispatchSnapshots)if(Pair.Value.Evidence.Get()==Work&&!Pair.Value.bDeferredVelocityDispatchComplete&&Pair.Value.DeferredVelocityTickerHandle.IsValid())return true;
 for(const auto&Ticket:Work->Queue){const int32 State=Ticket->State.load(std::memory_order_acquire);if(State==0||State==1)return true;}
 return false;
}
TMap<FString,TSharedPtr<FWork>> GroupWorks;
FString OperationGroup(const FString&Op)
{
 if(Op.StartsWith(TEXT("performance_")))return TEXT("performance");
 if(Op.StartsWith(TEXT("one_way_")))return TEXT("one_way");
 if(Op.StartsWith(TEXT("removal_")))return TEXT("removal");
 if(Op.StartsWith(TEXT("throttle_")))return TEXT("throttle");
 if(Op==TEXT("measure_throttling"))return TEXT("synchronous_throttle");
 if(Op==TEXT("trace_field_benchmark"))return TEXT("trace_field_benchmark");
 return TEXT("legacy_trace");
}
bool ObserverRequest(const FString&Op){return Op==TEXT("performance_sample")||Op==TEXT("performance_finish")||Op==TEXT("one_way_sample")||Op==TEXT("removal_sample")||Op==TEXT("throttle_sample")||Op==TEXT("sample");}
void CancelQueue(FWork&Work){for(auto&T:Work.Queue){int32 Expected=0;T->State.compare_exchange_strong(Expected,-1,std::memory_order_acq_rel);}}
void StopMetadataForWork(const TSharedPtr<FWork>&Work)
{
 if(!Work)return;CancelQueue(*Work);
 if(ActiveChaosPerformanceSession&&ActiveChaosPerformanceSession->Evidence==Work){if(ActiveChaosPerformanceSession->TickerHandle.IsValid())FTSTicker::RemoveTicker(ActiveChaosPerformanceSession->TickerHandle);ActiveChaosPerformanceSession->bCaptureCompleted=true;ActiveChaosPerformanceSession->TickerHandle.Reset();}
 if(ActiveOneWayStagedLaunch&&ActiveOneWayStagedLaunch->Evidence==Work){if(ActiveOneWayStagedLaunch->TickerHandle.IsValid())FTSTicker::RemoveTicker(ActiveOneWayStagedLaunch->TickerHandle);ActiveOneWayStagedLaunch->bLaunchCompleted=true;ActiveOneWayStagedLaunch->TickerHandle.Reset();}
 if(ActiveOneWayCollisionCapture&&ActiveOneWayCollisionCapture->Evidence==Work){if(ActiveOneWayCollisionCapture->TickerHandle.IsValid())FTSTicker::RemoveTicker(ActiveOneWayCollisionCapture->TickerHandle);ActiveOneWayCollisionCapture->bCaptureCompleted=true;ActiveOneWayCollisionCapture->TickerHandle.Reset();}
 if(ActiveRemovalSession&&ActiveRemovalSession->Evidence==Work){if(ActiveRemovalSession->TickerHandle.IsValid())FTSTicker::RemoveTicker(ActiveRemovalSession->TickerHandle);ActiveRemovalSession->bCaptureCompleted=true;ActiveRemovalSession->TickerHandle.Reset();}
 if(ActiveThrottleSession&&ActiveThrottleSession->Evidence==Work){if(ActiveThrottleSession->TickerHandle.IsValid())FTSTicker::RemoveTicker(ActiveThrottleSession->TickerHandle);ActiveThrottleSession->bCaptureCompleted=true;ActiveThrottleSession->TickerHandle.Reset();}
 for(auto&Pair:TraceItemDispatchSnapshots)if(Pair.Value.Evidence==Work){if(Pair.Value.DeferredVelocityTickerHandle.IsValid())FTSTicker::RemoveTicker(Pair.Value.DeferredVelocityTickerHandle);Pair.Value.bDeferredVelocityDispatchComplete=true;Pair.Value.DeferredVelocityTickerHandle.Reset();}
 Work->Pending=HasPendingWork(Work.Get());Work->ReleasePhysics();
}
void DetachOwnedOnlyUnsafe(const TSharedPtr<FWork>&Work)
{
 if(!Work)return;for(auto&Sub:Work->Subscriptions){if(auto*L=Sub.Listener.Get()){L->bAcceptEvents=false;L->Callback=nullptr;}}Work->Subscriptions.Reset();Work->R.Data->SetBoolField(TEXT("unsafe_context_owned_listener_pins_released"),true);Work->R.Data->SetBoolField(TEXT("notify_restore_skipped_unsafe_context"),true);
}
void TerminateWork(const TSharedPtr<FWork>&Work,const TCHAR*Reason,bool AttemptRestore=true)
{
 if(!Work)return;
 TGuardValue<bool> RestoreAdmission(Restoring,true);
 FExecutionScope Scope(Work);
 bool SafeRestore=AttemptRestore&&Scope.Valid&&Safe()&&Work->Guard();
 if(SafeRestore)
 {
  if(ActiveOneWayStagedLaunch&&ActiveOneWayStagedLaunch->Evidence==Work)SetOneWaySiblingSimulationCollision(*ActiveOneWayStagedLaunch,true);
  if(ActiveRemovalSession&&ActiveRemovalSession->Evidence==Work){SetRemovalTargetSimulationCollision(*ActiveRemovalSession,true);RestoreRemovalGravity(*ActiveRemovalSession);}
  if(ActiveThrottleSession&&ActiveThrottleSession->Evidence==Work)RestoreThrottleSettings(*ActiveThrottleSession);
  UnregisterOwnedBreak(nullptr);
 }
 else DetachOwnedOnlyUnsafe(Work);
 StopMetadataForWork(Work);
 Work->R.Data->SetStringField(TEXT("termination_reason"),Reason);
 Work->R.Data->SetBoolField(TEXT("termination_sdk_restoration_admitted"),SafeRestore);
 Work->R.Data->SetBoolField(TEXT("unresolved_restore_or_queue_lease"),HasPendingWork(Work.Get()));
 Work->R.bChangeStateKnown=false;
}
void ForgetResolvedWork(const TSharedPtr<FWork>&W)
{
 if(!W||HasPendingWork(W.Get()))return;
 if(ActiveChaosPerformanceSession&&ActiveChaosPerformanceSession->Evidence==W)ActiveChaosPerformanceSession.Reset();
 if(ActiveOneWayStagedLaunch&&ActiveOneWayStagedLaunch->Evidence==W)ActiveOneWayStagedLaunch.Reset();
 if(ActiveOneWayCollisionCapture&&ActiveOneWayCollisionCapture->Evidence==W)ActiveOneWayCollisionCapture.Reset();
 if(ActiveRemovalSession&&ActiveRemovalSession->Evidence==W)ActiveRemovalSession.Reset();
 if(ActiveThrottleSession&&ActiveThrottleSession->Evidence==W)ActiveThrottleSession.Reset();
 for(auto It=TraceItemDispatchSnapshots.CreateIterator();It;++It)if(It.Value().Evidence==W)It.RemoveCurrent();
 W->ReleasePhysics();
}

void RetireExpiredMetadata()
{
 for(auto It=GroupWorks.CreateIterator();It;++It){auto W=It.Value();if(!W||!W->HasWorld||W->World.IsValid())continue;StopMetadataForWork(W);DetachOwnedOnlyUnsafe(W);W->Fail(TEXT("world_expired"),TEXT("Captured weak world expired; no SDK restoration, reload or replay was attempted"));W->R.bChangeStateKnown=false;W->R.Stage=TEXT("expired_world_retired");if(RetiredSessions.Num()<32)RetiredSessions.Add(MakeShared<FJsonValueObject>(W->Snapshot()));It.RemoveCurrent();}
}
}
bool HasActiveSessionForWorld(const UWorld*World)
{
 if(!IsInGameThread())return true;
 if(Running&&CurrentWork&&CurrentWork->Writer&&CurrentWork->World.Get()==World)return true;
 for(const auto&Pair:GroupWorks)if(Pair.Value&&Pair.Value->World.Get()==World&&HasPendingWork(Pair.Value.Get()))return true;
 return false;
}
FString Run(const FString&ParamsJson)
{
 auto Fresh=MakeShared<FWork>();if(!Safe()){Fresh->Fail(TEXT("unsafe_context"),TEXT("Safe Editor game thread required"));return Fresh->Finish(LegacyError(Fresh->R.Error));}
 if(Running){Fresh->Fail(TEXT("reentrant_request"),TEXT("A native performance request/ticker is already executing"));return Fresh->Finish(LegacyError(Fresh->R.Error));}
 TSharedPtr<FJsonObject>Params;if(!Parse(ParamsJson,Params)){Fresh->Fail(TEXT("invalid_json"),TEXT("Expected a bounded strict JSON object with unique case-sensitive keys and finite numbers"));return Fresh->Finish(LegacyError(Fresh->R.Error));}
 const FString Op=GetStringParam(Params,TEXT("operation"),TEXT("sample"));const FString Group=OperationGroup(Op);const bool Observer=ObserverRequest(Op);
 RetireExpiredMetadata();UWorld*World=ActiveWorld();TSharedPtr<FWork>Work=Fresh;
 if(const auto*Existing=GroupWorks.Find(Group))if(*Existing&&(*Existing)->World.Get()==World)Work=*Existing;
 for(const auto&Pair:GroupWorks){const auto&Other=Pair.Value;if(!Observer&&Other&&Other!=Work&&HasPendingWork(Other.Get())){Fresh->Fail(TEXT("busy_session"),TEXT("A different GB behavior group retains pending physical/ticker work; finish or abort it first"));Fresh->R.Data->SetObjectField(TEXT("conflicting_session_execution"),Other->Snapshot());return Fresh->Finish(LegacyError(Fresh->R.Error));}}
 if(!Observer&&Work!=Fresh&&(Op==TEXT("performance_begin")||Op==TEXT("one_way_configure")||Op==TEXT("one_way_run")||Op==TEXT("removal_begin")||Op==TEXT("throttle_begin")||Op==TEXT("measure_throttling")||Op==TEXT("trace_field_benchmark")||Op==TEXT("trace_external_strain")))
 {
  TGuardValue<bool> RetirementBusy(Running,true);
  TerminateWork(Work,TEXT("explicit_new_request_retirement"),true);
  if(HasPendingWork(Work.Get())){Fresh->Fail(TEXT("unresolved_prior_session"),TEXT("Prior session retains unresolved runtime settings or queued work; no new dispatch occurred"));Fresh->R.Data->SetObjectField(TEXT("prior_session_execution"),Work->Snapshot());return Fresh->Finish(LegacyError(Fresh->R.Error));}
  if(RetiredSessions.Num()<32)RetiredSessions.Add(MakeShared<FJsonValueObject>(Work->Snapshot()));
  ForgetResolvedWork(Work);
  Work=Fresh;
 }
 Work->Operation=Op;Work->Group=Group;Work->CallSerial=Work->Serial;Work->CallChanged=Work->Changed;Work->CallPrefix=Work->Prefix.Num();
 TGuardValue<bool>Busy(Running,true);TGuardValue<bool> RequestWriter(Work->Writer,!Observer);TGuardValue<bool> RequestObserver(Work->AllowErrorObservation,Observer);FExecutionScope Scope(Work);if(!Scope.Valid)return Work->Finish(LegacyError(Work->R.Error));
 if(!Work->HasWorld&&World&&!Work->CaptureWorld(World))return Work->Finish(LegacyError(Work->R.Error));
 if(!Work->R.Error.IsEmpty()&&!Observer&&Op!=TEXT("removal_abort")&&Op!=TEXT("throttle_abort"))return Work->Finish(LegacyError(Work->R.Error));
 if(Work->Writer&&(!Work->Guard()))return Work->Finish(LegacyError(Work->R.Error));
 LastWork=Work;GroupWorks.Add(Group,Work);
 TGuardValue<bool> CleanupAdmission(Restoring,Op==TEXT("removal_abort")||Op==TEXT("throttle_abort"));
 auto Legacy=Handle(Params);
 bool LegacySucceeded=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),LegacySucceeded);if(!LegacySucceeded&&Legacy&&Work->R.Error.IsEmpty()){FString E;if(Legacy->TryGetStringField(TEXT("error"),E))Work->Fail(TEXT("legacy_failed"),E);}
 Work->Pending=HasPendingWork(Work.Get());
 if(!Work->R.Error.IsEmpty())
 {
  TerminateWork(Work,TEXT("request_failure"),true);
 }
 return Work->Finish(Legacy);
}
void Shutdown()
{
 if(!IsInGameThread())return;
 if(Running){ShuttingDown=true;if(CurrentWork)CurrentWork->Cancel=true;for(auto&Pair:GroupWorks)CancelQueue(*Pair.Value);return;}
 for(auto&Pair:GroupWorks)
 {
  auto Work=Pair.Value;if(!Work)continue;const bool WasPending=HasPendingWork(Work.Get());FExecutionScope Scope(Work);TGuardValue<bool> RestoreAdmission(Restoring,true);
  if(Scope.Valid&&Safe())
  {
   if(ActiveOneWayStagedLaunch&&ActiveOneWayStagedLaunch->Evidence==Work)SetOneWaySiblingSimulationCollision(*ActiveOneWayStagedLaunch,true);
   if(ActiveRemovalSession&&ActiveRemovalSession->Evidence==Work){SetRemovalTargetSimulationCollision(*ActiveRemovalSession,true);RestoreRemovalGravity(*ActiveRemovalSession);}
   if(ActiveThrottleSession&&ActiveThrottleSession->Evidence==Work)RestoreThrottleSettings(*ActiveThrottleSession);
   UnregisterOwnedBreak(nullptr);
  }
  else DetachOwnedOnlyUnsafe(Work);
  StopMetadataForWork(Work);
  if(WasPending){Work->Fail(TEXT("module_shutdown"),TEXT("Pending capture was stopped on module shutdown; queued commands and physical effects were not rolled back"));Work->R.Stage=TEXT("cancelled_on_shutdown");}
  Work->R.Data->SetBoolField(TEXT("queued_callback_DLL_unload_safety_verified"),false);
 }
 ShuttingDown=true;
}
}
