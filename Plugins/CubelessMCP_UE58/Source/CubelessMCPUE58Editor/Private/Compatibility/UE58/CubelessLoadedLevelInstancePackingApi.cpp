#include "Compatibility/UE58/CubelessLoadedLevelInstancePackingApi.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "LevelUtils.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInterface.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Logging/MessageLog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/GarbageCollection.h"

namespace Cubeless::UE58
{
namespace
{
using FJson=TSharedRef<FJsonObject>;
bool PackingRunning=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects);}
bool Idle(){return IsInGameThread()&&Live(GEditor)&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext()&&!GEditor->bIsSimulatingInEditor&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage()&&!IsRunningCommandlet();}
FString Json(const FJson& R){FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;}
FJson TransformJson(const FTransform& T){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("translation"),T.GetTranslation().ToString());J->SetStringField(TEXT("rotation_quaternion"),T.GetRotation().ToString());J->SetStringField(TEXT("scale"),T.GetScale3D().ToString());return J;}
bool FiniteTransform(const FTransform& T){const FVector S=T.GetScale3D();return !T.ContainsNaN()&&T.GetRotation().IsNormalized()&&!FMath::IsNearlyZero(S.X)&&!FMath::IsNearlyZero(S.Y)&&!FMath::IsNearlyZero(S.Z);}
bool BoundedPath(const FString& S){if(S.IsEmpty()||S.Len()>4096||!S.StartsWith(TEXT("/")))return false;for(TCHAR C:S)if(C==0)return false;return true;}
bool Unfiltered(const ILevelInstanceInterface* I){return I->GetFilter().DataLayerFilters.IsEmpty()&&I->GetFilter().GetChildFilters().IsEmpty()&&I->GetFilteredActorsPerContainer().IsEmpty();}
bool NoEditorLevelInstanceEdits()
{
 for(TObjectIterator<AActor> It;It;++It)
 {
  if(!Live(*It)||It->IsTemplate())continue;UWorld* W=It->GetWorld();if(!Live(W)||W->WorldType!=EWorldType::Editor)continue;
  if(auto* LI=Cast<ILevelInstanceInterface>(*It))if(LI->IsInAnyEditMode())return false;
 }
 return true;
}
FString MeshKey(UStaticMeshComponent* C)
{
 FString S=GetPathNameSafe(C->GetStaticMesh());for(int32 I=0;I<C->GetNumMaterials();++I)S+=TEXT("|")+GetPathNameSafe(C->GetMaterial(I));return S;
}
struct FMeshInstance{FString Key;FTransform World;};
struct FSourceComponent
{
 TStrongObjectPtr<UActorComponent> Object;FString Path;UClass* Class=nullptr;AActor* Owner=nullptr;
 FTransform Transform=FTransform::Identity;FString Key;TArray<FTransform> Instances;TArray<float> CustomData;int32 CustomFloats=0;
 explicit FSourceComponent(UActorComponent* C):Object(C),Path(C->GetPathName()),Class(C->GetClass()),Owner(C->GetOwner())
 {
  if(auto* Scene=Cast<USceneComponent>(C))Transform=Scene->GetComponentTransform();
  if(auto* SM=Cast<UStaticMeshComponent>(C)){Key=MeshKey(SM);if(auto* ISM=Cast<UInstancedStaticMeshComponent>(SM)){CustomData=ISM->PerInstanceSMCustomData;CustomFloats=ISM->NumCustomDataFloats;for(int32 I=0;I<ISM->GetInstanceCount();++I){FTransform T;if(ISM->GetInstanceTransform(I,T,true))Instances.Add(T);}}}
 }
 bool Current()const
 {
  auto* C=Object.Get();if(!Live(C)||C->GetClass()!=Class||C->GetOwner()!=Owner||C->GetPathName()!=Path)return false;
  if(auto* Scene=Cast<USceneComponent>(C))if(!Scene->GetComponentTransform().Equals(Transform,0.0001))return false;
  if(auto* SM=Cast<UStaticMeshComponent>(C)){if(MeshKey(SM)!=Key)return false;if(auto* ISM=Cast<UInstancedStaticMeshComponent>(SM)){if(ISM->GetInstanceCount()!=Instances.Num()||ISM->NumCustomDataFloats!=CustomFloats||ISM->PerInstanceSMCustomData!=CustomData)return false;for(int32 I=0;I<Instances.Num();++I){FTransform T;if(!ISM->GetInstanceTransform(I,T,true)||!T.Equals(Instances[I],0.0001))return false;}}}
  return true;
 }
};
struct FSourceActor
{
 TStrongObjectPtr<AActor> Actor;TStrongObjectPtr<UPackage> Package;FString Path;FTransform Transform;ULevel* Level=nullptr;bool Dirty=false;TArray<FSourceComponent> Components;
 explicit FSourceActor(AActor* A):Actor(A),Package(A->GetPackage()),Path(A->GetPathName()),Transform(A->GetActorTransform()),Level(A->GetLevel()),Dirty(Package->IsDirty())
 {for(UActorComponent* C:A->GetComponents())if(C)Components.Emplace(C);}
 bool Current()const
 {if(!Live(Actor.Get())||Actor->GetPathName()!=Path||Actor->GetLevel()!=Level||Actor->GetPackage()!=Package.Get()||Package->IsDirty()!=Dirty||!Actor->GetActorTransform().Equals(Transform,0.0001)||Actor->GetComponents().Num()!=Components.Num())return false;for(const auto& C:Components)if(!Actor->GetComponents().Contains(C.Object.Get())||!C.Current())return false;return true;}
};
bool ExclusiveTargetPackage(APackedLevelActor* Target,UPackage* Package)
{
 if(!Live(Target)||!Live(Package)||!Target->IsPackageExternal()||Target->GetExternalPackage()!=Package||Package==Target->GetOutermost()||!Package->GetName().StartsWith(TEXT("/Game/"))||UWorld::FindWorldInPackage(Package))return false;
 for(TObjectIterator<UObject> It;It;++It)if(Live(*It)&&It->GetPackage()==Package&&*It!=Package&&*It!=Target&&!It->IsIn(Target))return false;
 return true;
}
bool ReadPackedOutput(APackedLevelActor* Target,USceneComponent* Root,const TArray<FMeshInstance>& Expected,TArray<TSharedPtr<FJsonValue>>& Rows,int32& InstanceCount,int32& ComponentCount)
{
 Rows.Reset();InstanceCount=0;TArray<UActorComponent*> Components;Target->GetPackedComponents(Components);ComponentCount=Components.Num();TArray<FMeshInstance> Actual;
 for(UActorComponent* C:Components)
 {
  auto* ISM=Cast<UInstancedStaticMeshComponent>(C);if(!Live(ISM)||ISM->GetOwner()!=Target||!ISM->IsRegistered()||ISM->GetAttachParent()!=Root)return false;
  const FString Key=MeshKey(ISM);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),ISM->GetPathName());Row->SetStringField(TEXT("class"),ISM->GetClass()->GetPathName());Row->SetNumberField(TEXT("instance_count"),ISM->GetInstanceCount());Row->SetStringField(TEXT("mesh_and_materials"),Key);Rows.Add(MakeShared<FJsonValueObject>(Row));
  for(int32 I=0;I<ISM->GetInstanceCount();++I){FTransform T;if(Actual.Num()>=2048||!ISM->GetInstanceTransform(I,T,true)||!FiniteTransform(T))return false;Actual.Add({Key,T});}
 }
 InstanceCount=Actual.Num();if(Actual.Num()!=Expected.Num())return false;
 TArray<bool> Matched;Matched.Init(false,Actual.Num());for(const auto& E:Expected){bool Found=false;for(int32 I=0;I<Actual.Num();++I)if(!Matched[I]&&E.Key==Actual[I].Key&&E.World.Equals(Actual[I].World,0.001)){Matched[I]=true;Found=true;break;}if(!Found)return false;}
 return true;
}
TMap<FString,bool> DirtyPackages(){TMap<FString,bool> R;for(TObjectIterator<UPackage> It;It;++It)if(Live(*It))R.Add(It->GetName(),It->IsDirty());return R;}
TArray<TSharedPtr<FJsonValue>> DirtyDelta(const TMap<FString,bool>& Before)
{TArray<TSharedPtr<FJsonValue>> R;for(TObjectIterator<UPackage> It;It;++It)if(Live(*It)&&It->IsDirty()&&!Before.FindRef(It->GetName()))R.Add(MakeShared<FJsonValueString>(It->GetName()));return R;}
}
FString PackLoadedLevelInstance(const FString& RequestJson)
{
 auto R=MakeShared<FJsonObject>();auto Data=MakeShared<FJsonObject>();R->SetObjectField(TEXT("data"),Data);
 R->SetBoolField(TEXT("success"),false);R->SetStringField(TEXT("stage"),TEXT("preflight"));R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("changed"),false);R->SetBoolField(TEXT("mutation_attempted"),false);R->SetBoolField(TEXT("engine_pack_attempted"),false);R->SetBoolField(TEXT("engine_pack_returned"),false);R->SetField(TEXT("engine_pack_result"),MakeShared<FJsonValueNull>());
 R->SetBoolField(TEXT("compile_requested"),false);R->SetBoolField(TEXT("compile_attempted"),false);R->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());R->SetBoolField(TEXT("save_requested"),false);R->SetBoolField(TEXT("save_attempted"),false);R->SetBoolField(TEXT("saved"),false);R->SetBoolField(TEXT("atomic_rollback_available"),false);R->SetBoolField(TEXT("auto_retry_safe"),false);R->SetBoolField(TEXT("postcondition_verified"),false);
 TUniquePtr<FScopedTransaction> Transaction;TSharedPtr<FPackedLevelActorBuilder> Builder;bool OwnTransactionOpen=false;TFunction<bool()> FinalizationGuard;
 auto EndTransaction=[&]{if(Transaction){const bool Outstanding=Transaction->IsOutstanding();R->SetBoolField(TEXT("transaction_end_attempted"),Outstanding);Transaction.Reset();OwnTransactionOpen=false;R->SetBoolField(TEXT("transaction_end_returned"),Outstanding);}};
 auto Fail=[&](const TCHAR* Code,const FString& Message,bool Unknown=false)
 {
  // Release callback-producing scopes before serializing, including early failure paths.
  const bool HadBoundary=Builder.IsValid()||Transaction.IsValid();Builder.Reset();EndTransaction();
  if(HadBoundary&&(!FinalizationGuard||!FinalizationGuard()))Unknown=true;
  R->SetBoolField(TEXT("success"),false);R->SetStringField(TEXT("error_code"),Code);R->SetStringField(TEXT("error"),Message);R->SetBoolField(TEXT("postcondition_verified"),false);Data->SetBoolField(TEXT("packing_complete_for_all_components"),false);
  if(Unknown)
  {
   R->SetBoolField(TEXT("change_state_known"),false);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());R->SetBoolField(TEXT("do_not_retry"),true);
   for(const TCHAR* Key:{TEXT("placement_verified"),TEXT("source_observed_state_preserved")})
   {const auto Previous=Data->TryGetField(Key);if(Previous.IsValid())Data->SetField(FString(Key)+TEXT("_last_observed"),Previous);Data->SetField(Key,MakeShared<FJsonValueNull>());}
   Data->SetStringField(TEXT("readback_observation_scope"),TEXT("Any retained after_components/counts/hash/dirty rows describe the last completed observation before the failure/finalization callback, not verified final state"));
  }
  return Json(R);
 };
 if(!Idle()||PackingRunning)return Fail(TEXT("packing_context"),TEXT("An idle non-reentrant Editor game thread outside PIE/SIE/GC/save is required"));TGuardValue<bool> Running(PackingRunning,true);
 if(GEditor->IsTransactionActive())return Fail(TEXT("packing_active_transaction"),TEXT("An existing editor transaction is active; no packing or nested transaction is started"));
 if(!NoEditorLevelInstanceEdits())return Fail(TEXT("packing_global_editing"),TEXT("All Editor LevelInstance normal/property-override edit sessions must be closed before packing"));
 TSharedPtr<FJsonObject> P;if(RequestJson.Len()>16384||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RequestJson),P)||!P)return Fail(TEXT("packing_json"),TEXT("A bounded JSON object is required"));
 const TSet<FString> Keys{TEXT("source_actor_path"),TEXT("target_actor_path"),TEXT("dry_run"),TEXT("allow_replace"),TEXT("save")};if(P->Values.Num()!=Keys.Num())return Fail(TEXT("packing_fields"),TEXT("Exactly source_actor_path,target_actor_path,dry_run,allow_replace,save are required"));for(const auto& Pair:P->Values)if(!Keys.Contains(FString(Pair.Key.ToView())))return Fail(TEXT("packing_fields"),TEXT("Unknown request field"));
 FString SourcePath,TargetPath;bool DryRun,AllowReplace,Save;
 if(!P->TryGetStringField(TEXT("source_actor_path"),SourcePath)||!P->TryGetStringField(TEXT("target_actor_path"),TargetPath)||!BoundedPath(SourcePath)||!BoundedPath(TargetPath)||!P->TryGetBoolField(TEXT("dry_run"),DryRun)||!P->TryGetBoolField(TEXT("allow_replace"),AllowReplace)||!P->TryGetBoolField(TEXT("save"),Save))return Fail(TEXT("packing_input"),TEXT("Exact bounded actor paths and explicit boolean flags are required"));
 R->SetBoolField(TEXT("save_requested"),Save);Data->SetBoolField(TEXT("dry_run"),DryRun);Data->SetBoolField(TEXT("allow_replace"),AllowReplace);
 TStrongObjectPtr<UEditorEngine> Editor(GEditor);TStrongObjectPtr<UWorld> World(GEditor->GetEditorWorldContext().World());if(!Live(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown)return Fail(TEXT("packing_world"),TEXT("A live Editor world is required"));
 AActor* SourceActor=nullptr;APackedLevelActor* TargetActor=nullptr;for(TActorIterator<AActor> It(World.Get());It;++It){if(It->GetPathName()==SourcePath)SourceActor=*It;if(It->GetPathName()==TargetPath)TargetActor=Cast<APackedLevelActor>(*It);}
 auto* Source=Cast<ILevelInstanceInterface>(SourceActor);if(!Live(SourceActor)||!Source||!Live(TargetActor)||SourceActor==TargetActor)return Fail(TEXT("packing_actors"),TEXT("Distinct exact existing LevelInstance source and PackedLevelActor target in this Editor world are required"));
 if(TargetActor->GetClass()!=APackedLevelActor::StaticClass())return Fail(TEXT("packing_target_class"),TEXT("Only native APackedLevelActor targets are supported: direct packing does not update Blueprint/SCS components and later construction can restore old data"));
 TStrongObjectPtr<AActor> SourceHold(SourceActor);TStrongObjectPtr<APackedLevelActor> Target(TargetActor);TStrongObjectPtr<UPackage> TargetPackage(Target->GetPackage());TStrongObjectPtr<ULevelInstanceSubsystem> Sub(World->GetSubsystem<ULevelInstanceSubsystem>());
 if(!Live(Sub.Get())||Source->GetLevelInstanceSubsystem()!=Sub.Get()||Target->GetLevelInstanceSubsystem()!=Sub.Get())return Fail(TEXT("packing_subsystem"),TEXT("Source and target must use this same world's LevelInstanceSubsystem"));
 if(Source->IsInAnyEditMode()||Source->HasChildEdit()||Source->HasParentEdit()||Target->IsInAnyEditMode()||Target->HasChildEdit()||Target->HasParentEdit())return Fail(TEXT("packing_editing"),TEXT("Active LevelInstance editing or property-override sessions are not supported"));
 if(Sub->IsLoading(Source)||Sub->IsLoading(Target.Get())||Sub->GetLevelInstanceLevel(Target.Get()))return Fail(TEXT("packing_loading"),TEXT("Source must be fully loaded and target must not have a loaded/loading source level"));
 TStrongObjectPtr<ULevel> Level(Sub->GetLevelInstanceLevel(Source));if(!Live(Level.Get()))return Fail(TEXT("packing_not_loaded"),TEXT("Source level must already be loaded; this API does not load or create LevelInstances"));
 TStrongObjectPtr<ULevelStreaming> Streaming(ULevelStreaming::FindStreamingLevel(Level.Get()));TStrongObjectPtr<AWorldSettings> Settings(Level->GetWorldSettings());
 if(!Live(Streaming.Get())||Streaming->GetLoadedLevel()!=Level.Get()||Streaming->IsStreamingStatePending()||!Live(Settings.Get()))return Fail(TEXT("packing_stream"),TEXT("A stable loaded level streaming object and WorldSettings are required"));
 if(Level->GetWorldPartition())return Fail(TEXT("packing_partial_source"),TEXT("Partitioned source completeness is not proven by a loaded level; use a complete non-partitioned source for this initial supported path"));
 if(Level->GetPackage()->IsDirty()||Source->IsDirty()||Source->HasDirtyChildren())return Fail(TEXT("packing_dirty_source"),TEXT("Source LevelInstance and loaded source packages must be clean; this API never saves or discards source changes"));
 if(!Unfiltered(Source)||!Unfiltered(Target.Get()))return Fail(TEXT("packing_filter"),TEXT("Nonempty LevelInstance filters are not supported: direct loaded-level packing does not reload actors with Packing filter semantics"));
 if(Sub->GetOwningLevelInstance(SourceActor->GetLevel())||Sub->GetOwningLevelInstance(Target->GetLevel())||Target->GetLevel()==Level.Get())return Fail(TEXT("packing_containment"),TEXT("Nested/contained source or target actors are not supported by this guarded flat-level path"));
 if(FLevelUtils::IsLevelLocked(Target->GetLevel()))return Fail(TEXT("packing_locked"),TEXT("Target level is locked"));
 const FString SourceAsset=Source->GetWorldAsset().ToSoftObjectPath().ToString(),TargetAsset=Target->GetWorldAsset().ToSoftObjectPath().ToString();
 if(SourceAsset.IsEmpty()||SourceAsset!=TargetAsset)return Fail(TEXT("packing_world_asset_mismatch"),TEXT("Wrapper policy requires matching nonempty source/target WorldAsset; direct PackActor does not set it. Configure the target explicitly first; metadata is never silently changed"));
 const FTransform SourceTransform=SourceActor->GetActorTransform(),TargetTransform=Target->GetActorTransform(),StreamingTransform=Streaming->LevelTransform;const FVector Pivot=Settings->LevelInstancePivotOffset;
 if(!FiniteTransform(SourceTransform)||!FiniteTransform(TargetTransform)||!FiniteTransform(StreamingTransform)||Pivot.ContainsNaN()||!Live(Target->GetRootComponent()))return Fail(TEXT("packing_transform"),TEXT("Finite invertible transforms and a target root are required"));
 const FTransform CurrentPivot(StreamingTransform.GetRelativeTransform(TargetTransform).GetTranslation());const FTransform RelativePivot=FTransform(Pivot).GetRelativeTransform(CurrentPivot);
 Data->SetStringField(TEXT("source_actor_path"),SourcePath);Data->SetStringField(TEXT("target_actor_path"),TargetPath);Data->SetStringField(TEXT("source_world_asset"),SourceAsset);Data->SetStringField(TEXT("target_world_asset"),TargetAsset);Data->SetStringField(TEXT("world_asset_requirement"),TEXT("Wrapper safety policy; the native direct overload itself does not enforce matching metadata"));Data->SetObjectField(TEXT("source_transform"),TransformJson(SourceTransform));Data->SetObjectField(TEXT("target_transform"),TransformJson(TargetTransform));Data->SetObjectField(TEXT("streaming_transform"),TransformJson(StreamingTransform));Data->SetObjectField(TEXT("relative_pivot_transform"),TransformJson(RelativePivot));Data->SetStringField(TEXT("pivot_offset"),Pivot.ToString());Data->SetBoolField(TEXT("source_loaded"),true);Data->SetBoolField(TEXT("filters_empty"),true);
 TArray<TSharedPtr<FJsonValue>> BeforeRows,Warnings,Skipped,ExpectedRows;TArray<UActorComponent*> OldPacked;Target->GetPackedComponents(OldPacked);TArray<TStrongObjectPtr<UActorComponent>> RetainedTarget;TArray<FSourceComponent> RetainedState,OldPackedState;TSet<UActorComponent*> BeforeTargetComponents;
 if(Target->GetRootComponent()->ComponentHasTag(APackedLevelActor::GetPackedComponentTag()))return Fail(TEXT("packing_packed_root"),TEXT("Target root carries the packed-component tag and would be destroyed by native packing; no writer dispatched"));
 if(Target->GetComponents().Num()>4096)return Fail(TEXT("packing_target_budget"),TEXT("Target component inventory exceeds 4096"));
 int32 TargetInstanceBudget=0;
 for(UActorComponent* C:Target->GetComponents())
 {
  if(!Live(C))return Fail(TEXT("packing_target_component"),TEXT("Target contains a retired component"));
  if(auto* ISM=Cast<UInstancedStaticMeshComponent>(C)){const int32 Count=ISM->GetInstanceCount();if(Count<0||Count>2048-TargetInstanceBudget)return Fail(TEXT("packing_target_budget"),TEXT("Target aggregate mesh inventory exceeds 2048 instances"));TargetInstanceBudget+=Count;}
  else if(C->IsA<UStaticMeshComponent>()){if(++TargetInstanceBudget>2048)return Fail(TEXT("packing_target_budget"),TEXT("Target aggregate mesh inventory exceeds 2048 instances"));}
 }
 for(UActorComponent* C:Target->GetComponents()){BeforeTargetComponents.Add(C);if(OldPacked.Contains(C))OldPackedState.Emplace(C);}
 bool TargetHasData=!OldPacked.IsEmpty();for(UActorComponent* C:Target->GetComponents())if(C){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),C->GetPathName());Row->SetStringField(TEXT("class"),C->GetClass()->GetPathName());Row->SetBoolField(TEXT("packed"),OldPacked.Contains(C));BeforeRows.Add(MakeShared<FJsonValueObject>(Row));if(!OldPacked.Contains(C)){RetainedTarget.Emplace(C);RetainedState.Emplace(C);}if(C->IsA<UStaticMeshComponent>())TargetHasData=true;}
 Data->SetArrayField(TEXT("before_components"),BeforeRows);Data->SetBoolField(TEXT("target_has_data"),TargetHasData);Data->SetBoolField(TEXT("would_replace_packed_components"),!OldPacked.IsEmpty());Data->SetNumberField(TEXT("before_packed_component_count"),OldPacked.Num());
 TArray<FSourceActor> SourceActors;TArray<FMeshInstance> Expected;TSet<AActor*> CapturedActors;int32 SourceComponentCount=0;
 if(Level->Actors.Num()>512)return Fail(TEXT("packing_source_budget"),TEXT("Flat source exceeds 512 actors"));
 for(AActor* A:Level->Actors)
 {
  if(!A)continue;if(!Live(A)||A->GetWorld()!=World.Get()||A->GetLevel()!=Level.Get()||A==Target.Get())return Fail(TEXT("packing_source_identity"),TEXT("Source contains invalid, foreign or target actor"));
  if(Cast<ILevelInstanceInterface>(A))return Fail(TEXT("packing_nested_source"),TEXT("Nested LevelInstances/PackedLevelActors require separate recursive load/filter/ownership validation; flat source only"));
  if(A->GetPackage()->IsDirty())return Fail(TEXT("packing_dirty_source"),TEXT("A source actor package is dirty"));
  SourceComponentCount+=A->GetComponents().Num();if(SourceComponentCount>4096)return Fail(TEXT("packing_source_budget"),TEXT("Source component inventory exceeds 4096"));
  for(UActorComponent* C:A->GetComponents()){if(!Live(C))return Fail(TEXT("packing_source_component"),TEXT("Source contains a retired component"));if(auto* ISM=Cast<UInstancedStaticMeshComponent>(C))if(ISM->GetInstanceCount()>2048)return Fail(TEXT("packing_source_budget"),TEXT("Source ISM inventory exceeds 2048 instances"));}
  CapturedActors.Add(A);SourceActors.Emplace(A);
  if(A==Level->GetDefaultBrush()||A==Settings.Get()||A->IsMainWorldOnly()){Skipped.Add(MakeShared<FJsonValueString>(A->GetPathName()+TEXT(" : engine recursive discard")));continue;}
  for(UActorComponent* C:A->GetComponents())
  {
   if(!Live(C))return Fail(TEXT("packing_component_identity"),TEXT("Source contains a retired component"));
   if(C->IsVisualizationComponent()){Skipped.Add(MakeShared<FJsonValueString>(C->GetPathName()+TEXT(" : visualization discard")));continue;}
   auto* SM=Cast<UStaticMeshComponent>(C);
   if(!SM){if(C->GetClass()!=USceneComponent::StaticClass()&&C->GetClass()->GetPathName()!=TEXT("/Script/ChaosSolverEngine.ChaosDebugDrawComponent"))Warnings.Add(MakeShared<FJsonValueString>(C->GetPathName()+TEXT(" : unsupported component is not packed")));continue;}
   if(!Live(SM->GetStaticMesh())||!SM->IsRegistered())return Fail(TEXT("packing_mesh"),TEXT("Every packed source mesh must have a live StaticMesh and be registered"));
   const FString Key=MeshKey(SM);TArray<FTransform> Transforms;
   if(auto* ISM=Cast<UInstancedStaticMeshComponent>(SM)){if(ISM->GetInstanceCount()>2048||ISM->NumCustomDataFloats<0||static_cast<int64>(ISM->NumCustomDataFloats)*ISM->GetInstanceCount()>ISM->PerInstanceSMCustomData.Num())return Fail(TEXT("packing_instance_data"),TEXT("Source ISM exceeds budget or has incomplete custom data"));for(int32 I=0;I<ISM->GetInstanceCount();++I){FTransform T;if(!ISM->GetInstanceTransform(I,T,true))return Fail(TEXT("packing_instance_transform"),TEXT("Unable to read a source instance"));Transforms.Add(T);}}
   else Transforms.Add(SM->GetComponentTransform());
   for(const FTransform& T:Transforms){if(!FiniteTransform(T)||Expected.Num()>=2048)return Fail(TEXT("packing_instance_budget"),TEXT("Finite source instances within the 2048 instance budget required"));const FTransform Wanted=T.GetRelativeTransform(TargetTransform)*RelativePivot*TargetTransform;Expected.Add({Key,Wanted});auto Row=TransformJson(Wanted);Row->SetStringField(TEXT("source_component"),C->GetPathName());Row->SetStringField(TEXT("mesh_and_materials"),Key);ExpectedRows.Add(MakeShared<FJsonValueObject>(Row));}
  }
 }
 if(Expected.IsEmpty())return Fail(TEXT("packing_empty_source"),TEXT("Source has no supported mesh instances; destructive empty packing is refused"));
 Data->SetArrayField(TEXT("unsupported_components"),Warnings);Data->SetArrayField(TEXT("engine_discarded_objects"),Skipped);Data->SetArrayField(TEXT("expected_engine_placement"),ExpectedRows);Data->SetNumberField(TEXT("expected_instance_count"),Expected.Num());Data->SetBoolField(TEXT("packing_complete_for_all_components"),Warnings.IsEmpty());Data->SetBoolField(TEXT("native_message_log_may_open"),!Warnings.IsEmpty());Data->SetStringField(TEXT("placement_formula"),TEXT("sourceWS.GetRelativeTransform(targetActor) * translationOnlyRelativePivot * targetActor (stock ISMBuilder)"));
 const bool Exclusive=ExclusiveTargetPackage(Target.Get(),TargetPackage.Get());
 Data->SetStringField(TEXT("target_package"),TargetPackage->GetName());Data->SetBoolField(TEXT("target_only_save_available"),Exclusive);Data->SetStringField(TEXT("save_policy"),TEXT("Only an exclusive external actor package is saved here. Ordinary map actors require an explicit separate target-level save; no automatic map save or source save"));
 if(!DryRun&&TargetHasData&&!AllowReplace)return Fail(TEXT("packing_replace_required"),TEXT("Target already contains data; allow_replace=true is required before destructive packing"));
 if(!DryRun&&Save&&!Exclusive)return Fail(TEXT("packing_save_scope"),TEXT("Target has no exclusive external actor package; save=true is refused before any writer. Use save=false and separately save the target level explicitly"));
 if(!DryRun&&Save&&IFileManager::Get().IsReadOnly(*FPackageName::LongPackageNameToFilename(TargetPackage->GetName(),FPackageName::GetAssetPackageExtension())))return Fail(TEXT("packing_save_readonly"),TEXT("Target package file is read-only; no target writer dispatched"));
 const auto DirtyBefore=DirtyPackages();const bool TargetDirtyBefore=TargetPackage->IsDirty();Data->SetBoolField(TEXT("target_package_dirty_before"),TargetDirtyBefore);
 const FString LevelPath=Level->GetPathName();TStrongObjectPtr<UPackage> SourceLevelPackage(Level->GetPackage());const bool LevelDirty=SourceLevelPackage->IsDirty();TStrongObjectPtr<USceneComponent> TargetRoot(Target->GetRootComponent());
 auto Current=[&]
 {
  if(!Idle()||(GEditor->IsTransactionActive()!=OwnTransactionOpen)||!NoEditorLevelInstanceEdits()||GEditor!=Editor.Get()||GEditor->GetEditorWorldContext().World()!=World.Get()||!Live(World.Get())||World->bIsTearingDown||!Live(SourceHold.Get())||!Live(Target.Get())||SourceHold->GetPathName()!=SourcePath||Target->GetPathName()!=TargetPath||SourceHold->GetWorld()!=World.Get()||Target->GetWorld()!=World.Get()||Target->GetClass()!=APackedLevelActor::StaticClass()||Target->GetPackage()!=TargetPackage.Get()||Target->GetRootComponent()!=TargetRoot.Get()||!Live(TargetRoot.Get())||!SourceHold->GetActorTransform().Equals(SourceTransform,0.0001)||!Target->GetActorTransform().Equals(TargetTransform,0.0001))return false;
  if(Source->GetLevelInstanceSubsystem()!=Sub.Get()||Target->GetLevelInstanceSubsystem()!=Sub.Get()||Sub->GetLevelInstanceLevel(Source)!=Level.Get()||!Live(Level.Get())||Level->GetPathName()!=LevelPath||Level->GetPackage()!=SourceLevelPackage.Get()||SourceLevelPackage->IsDirty()!=LevelDirty||ULevelStreaming::FindStreamingLevel(Level.Get())!=Streaming.Get()||!Live(Streaming.Get())||Streaming->GetLoadedLevel()!=Level.Get()||Streaming->IsStreamingStatePending()||!Streaming->LevelTransform.Equals(StreamingTransform,0.0001)||Level->GetWorldSettings()!=Settings.Get()||!Live(Settings.Get())||!Settings->LevelInstancePivotOffset.Equals(Pivot,0.0001)||Source->GetWorldAsset().ToSoftObjectPath().ToString()!=SourceAsset||Target->GetWorldAsset().ToSoftObjectPath().ToString()!=TargetAsset||!Unfiltered(Source)||!Unfiltered(Target.Get())||Source->IsInAnyEditMode()||Target->IsInAnyEditMode()||Sub->IsLoading(Source)||Sub->IsLoading(Target.Get())||Sub->GetLevelInstanceLevel(Target.Get()))return false;
  TSet<AActor*> Actual;for(AActor* A:Level->Actors)if(A)Actual.Add(A);if(Actual.Num()!=CapturedActors.Num())return false;for(AActor* A:CapturedActors)if(!Actual.Contains(A))return false;
  for(const auto& A:SourceActors)if(!A.Current())return false;for(const auto& C:RetainedState)if(!Target->GetComponents().Contains(C.Object.Get())||!C.Current())return false;
  // Replacement admission applies to the exact preflight target until the native
  // destructive call begins. A listener cannot insert or edit data under the old consent.
  if(!R->GetBoolField(TEXT("engine_pack_attempted")))
  {
   if(Target->GetComponents().Num()!=BeforeTargetComponents.Num())return false;
   for(UActorComponent* C:BeforeTargetComponents)if(!Target->GetComponents().Contains(C))return false;
   TArray<UActorComponent*> CurrentPacked;Target->GetPackedComponents(CurrentPacked);if(CurrentPacked.Num()!=OldPacked.Num())return false;
   for(UActorComponent* C:OldPacked)if(!CurrentPacked.Contains(C))return false;
   for(const auto& C:OldPackedState)if(!C.Current())return false;
  }
  return true;
 };
 FinalizationGuard=[&]{return Current();};
 if(!Current())return Fail(TEXT("packing_preflight_changed"),TEXT("Source/target changed while capturing preflight; no writer dispatched"));
 Data->SetStringField(TEXT("source_preservation_scope"),TEXT("Exact source actor/component membership and ownership, transforms, mesh/material bindings, ISM transforms/custom data and source package dirty states. Other authored properties, listener globals and persisted file bytes are not exhaustively fingerprinted"));
 if(DryRun){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("dry_run_complete"));return Json(R);}
 R->SetStringField(TEXT("stage"),TEXT("create_default_builder"));Builder=FPackedLevelActorBuilder::CreateDefaultBuilder();if(!Current()||!Builder.IsValid())return Fail(TEXT("packing_builder"),TEXT("Context changed constructing the native builder; no target writer dispatched"));
 FMessageLog Log(TEXT("PackedLevelActor"));const int32 WarningsBefore=Log.NumMessages(EMessageSeverity::Warning);if(!Current())return Fail(TEXT("packing_log_context"),TEXT("Context changed reading message-log state before target writer"));
 const double Started=FPlatformTime::Seconds();R->SetStringField(TEXT("stage"),TEXT("begin_transaction"));R->SetBoolField(TEXT("transaction_begin_attempted"),true);Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP","PackLoadedLevelInstance","Pack Loaded LevelInstance Into Existing PackedLevelActor"));OwnTransactionOpen=Transaction->IsOutstanding();R->SetBoolField(TEXT("transaction_begin_returned"),true);
 if(!OwnTransactionOpen||!Current())return Fail(TEXT("packing_transaction_start_changed"),TEXT("TransactionStarted callback changed source/target/context or the transaction did not start; Target.Modify and PackActor were not dispatched"),true);
 R->SetBoolField(TEXT("mutation_attempted"),true);R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("do_not_retry"),true);R->SetStringField(TEXT("stage"),TEXT("target_modify"));Target->Modify();if(!Current())return Fail(TEXT("packing_modify_changed"),TEXT("Source/target changed during Modify; stop without packing, saving or rollback"),true);
 R->SetStringField(TEXT("stage"),TEXT("engine_pack"));R->SetBoolField(TEXT("engine_pack_attempted"),true);const bool Packed=Builder->PackActor(Target.Get(),Source);R->SetBoolField(TEXT("engine_pack_returned"),true);R->SetBoolField(TEXT("engine_pack_result"),Packed);Data->SetNumberField(TEXT("pack_seconds"),FPlatformTime::Seconds()-Started);
 if(!Current())return Fail(TEXT("packing_callback_changed"),TEXT("Source/target ownership or preserved state changed inside native PackActor; target may already be replaced. No follow-up save, rollback or retry"),true);
 R->SetStringField(TEXT("stage"),TEXT("release_native_builder"));Builder.Reset();if(!Current())return Fail(TEXT("packing_builder_cleanup_changed"),TEXT("Source/target changed during native preview-scene cleanup; no save or retry"),true);
 R->SetStringField(TEXT("stage"),TEXT("readback"));TArray<TSharedPtr<FJsonValue>> AfterRows;int32 ActualCount=0,ActualComponents=0;
 const bool Placement=ReadPackedOutput(Target.Get(),TargetRoot.Get(),Expected,AfterRows,ActualCount,ActualComponents);
 Data->SetArrayField(TEXT("after_components"),AfterRows);Data->SetNumberField(TEXT("after_packed_component_count"),ActualComponents);Data->SetNumberField(TEXT("actual_instance_count"),ActualCount);Data->SetNumberField(TEXT("packed_hash"),Target->GetPackedHash());
 Data->SetBoolField(TEXT("placement_verified"),Placement);Data->SetBoolField(TEXT("source_observed_state_preserved"),true);Data->SetBoolField(TEXT("target_package_dirty_after_pack"),TargetPackage->IsDirty());
 const int32 WarningsAfter=Log.NumMessages(EMessageSeverity::Warning);if(!Current())return Fail(TEXT("packing_log_callback_changed"),TEXT("Context changed reading native warning count after packing"),true);Data->SetNumberField(TEXT("message_log_warning_or_error_delta"),WarningsAfter-WarningsBefore);Data->SetStringField(TEXT("message_log_count_scope"),TEXT("Observed shared PackedLevelActor listing delta only. Deduplication, page changes and unrelated listeners prevent complete warning attribution; the explicit source inventory is the primary omission evidence"));
 if(!Packed||!Placement)return Fail(TEXT("packing_engine_or_postcondition_failed"),TEXT("Native result or supported mesh/material/placement readback failed; actual replacement prefix retained and no save/retry/rollback performed"),true);
 R->SetStringField(TEXT("stage"),TEXT("mark_target_dirty"));Target->MarkPackageDirty();if(!Current()||!ReadPackedOutput(Target.Get(),TargetRoot.Get(),Expected,AfterRows,ActualCount,ActualComponents))return Fail(TEXT("packing_dirty_callback_changed"),TEXT("Source/target or packed geometry changed during dirty notification; no save or retry"),true);
 R->SetStringField(TEXT("stage"),TEXT("end_transaction"));EndTransaction();
 if(!Current()||!ReadPackedOutput(Target.Get(),TargetRoot.Get(),Expected,AfterRows,ActualCount,ActualComponents))return Fail(TEXT("packing_transaction_end_changed"),TEXT("Transaction finalization changed source/target or packed geometry; no save, rollback or retry"),true);
 Data->SetArrayField(TEXT("after_components"),AfterRows);Data->SetNumberField(TEXT("after_packed_component_count"),ActualComponents);Data->SetNumberField(TEXT("actual_instance_count"),ActualCount);Data->SetNumberField(TEXT("packed_hash"),Target->GetPackedHash());Data->SetBoolField(TEXT("placement_verified"),true);Data->SetBoolField(TEXT("source_observed_state_preserved"),true);
 R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("changed"),true);R->SetBoolField(TEXT("postcondition_verified"),true);
 if(!Warnings.IsEmpty()||WarningsAfter>WarningsBefore)
 {
  Data->SetBoolField(TEXT("packing_complete_for_all_components"),false);Data->SetBoolField(TEXT("target_package_dirty_after"),TargetPackage->IsDirty());Data->SetArrayField(TEXT("new_dirty_packages"),DirtyDelta(DirtyBefore));
  R->SetBoolField(TEXT("postcondition_verified"),false);R->SetStringField(TEXT("stage"),TEXT("packing_incomplete"));R->SetStringField(TEXT("error_code"),TEXT("packing_incomplete"));R->SetStringField(TEXT("error"),TEXT("Packing changed the target and supported geometry placement passed, but unsupported components or observed native warnings prevent complete packing. No save attempted; inspect omissions and actual target before any separate action"));return Json(R);
 }
 if(Save)
 {
  if(!ExclusiveTargetPackage(Target.Get(),TargetPackage.Get())||!Current())return Fail(TEXT("packing_save_owner_changed"),TEXT("Exclusive external target package identity changed before save"),true);
  FString Filename=FPackageName::LongPackageNameToFilename(TargetPackage->GetName(),FPackageName::GetAssetPackageExtension());if(IFileManager::Get().IsReadOnly(*Filename))return Fail(TEXT("packing_save_readonly"),TEXT("Target package file is read-only; packed memory changes remain unsaved"),true);
  R->SetStringField(TEXT("stage"),TEXT("save_target_external_package"));R->SetBoolField(TEXT("save_attempted"),true);R->SetBoolField(TEXT("change_state_known"),false);FSavePackageArgs Args;Args.TopLevelFlags=RF_Standalone;Args.SaveFlags=SAVE_NoError;const bool Saved=UPackage::SavePackage(TargetPackage.Get(),nullptr,*Filename,Args);R->SetBoolField(TEXT("saved"),Saved);Data->SetStringField(TEXT("saved_package_filename"),Filename);
  if(!Saved||!Current()||!ExclusiveTargetPackage(Target.Get(),TargetPackage.Get())||TargetPackage->IsDirty()||!ReadPackedOutput(Target.Get(),TargetRoot.Get(),Expected,AfterRows,ActualCount,ActualComponents))return Fail(TEXT("packing_save_failed_or_changed"),TEXT("Target-only save failed or state changed across save callbacks; inspect actual disk/memory state before any separate action"),true);R->SetBoolField(TEXT("change_state_known"),true);
 }
 Data->SetArrayField(TEXT("after_components"),AfterRows);Data->SetNumberField(TEXT("after_packed_component_count"),ActualComponents);Data->SetNumberField(TEXT("actual_instance_count"),ActualCount);Data->SetBoolField(TEXT("target_package_dirty_after"),TargetPackage->IsDirty());Data->SetArrayField(TEXT("new_dirty_packages"),DirtyDelta(DirtyBefore));R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Json(R);
}
}
