#include "Compatibility/UE58/CubelessEditorDataApi.h"
#include "Compatibility/UE58/DomainExpansion/CubelessDomainDependencyAudit.h"
#include "Editor.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "LevelUtils.h"
#include "ScopedSuspendRerunConstructionScripts.h"
#include "Engine/LevelStreaming.h"
#include "Engine/BookMark.h"
#include "GameFramework/WorldSettings.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/ActorDescContainerInstanceCollection.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "StaticMeshComponentLODInfo.h"
#include "Rendering/ColorVertexBuffer.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/MovieScene2DTransformSection.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "Elements/PCGGetActorProperty.h"
#include "Elements/Polygon/PCGPolygon2DOperation.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::EditorData
{
namespace
{
using J=FJsonObject;
bool Writing=false;
FString Path(const UObject* O){return IsValid(O)?O->GetPathName():FString();}
TSharedPtr<FJsonValue> Row(const TSharedRef<J>& O){return MakeShared<FJsonValueObject>(O);}
FString Encode(const TSharedRef<J>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
TArray<TSharedPtr<FJsonValue>> Vector(const FVector& V){return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
bool NameOK(const FString& N)
{
 if(N.IsEmpty()||N.Len()>128||!FChar::IsAlpha(N[0]))return false;
 for(TCHAR C:N)if(C>127||(!FChar::IsAlnum(C)&&C!=TEXT('_')))return false;
 return !FName(*N).IsNone();
}
struct FContext
{
 TSharedRef<J> R=MakeShared<J>();UWorld* EditorWorld=nullptr;bool Ready=false;
 explicit FContext(const TCHAR* Op)
 {
  R->SetStringField(TEXT("operation"),Op);R->SetStringField(TEXT("stage"),TEXT("preflight"));R->SetBoolField(TEXT("success"),false);
  R->SetBoolField(TEXT("mutation_attempted"),false);R->SetBoolField(TEXT("changed"),false);R->SetBoolField(TEXT("change_state_known"),true);
  R->SetBoolField(TEXT("compile_attempted"),false);R->SetBoolField(TEXT("save_attempted"),false);R->SetBoolField(TEXT("saved"),false);
  R->SetBoolField(TEXT("atomic_rollback"),false);R->SetBoolField(TEXT("automatic_retry"),false);
  if(!Current()){R->SetStringField(TEXT("error"),TEXT("Live Editor game thread outside PIE/SIE, GC, package save and exit required"));return;}
  EditorWorld=GEditor->GetEditorWorldContext().World();Ready=true;
 }
 bool Current()const
 {
  if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor)return false;
  UWorld* W=GEditor->GetEditorWorldContext().World();return IsValid(W)&&W->WorldType==EWorldType::Editor&&!W->bIsTearingDown&&(!EditorWorld||W==EditorWorld);
 }
 FString Fail(const TCHAR* Error){R->SetStringField(TEXT("error"),Error);return Encode(R);}
 FString Done(){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Encode(R);}
 bool EditorActor(AActor* A)const{return IsValid(A)&&!A->IsTemplate()&&!A->IsActorBeingDestroyed()&&A->GetWorld()==EditorWorld&&IsValid(A->GetLevel());}
 bool After(UObject* O,UPackage* P,const FString& BeforePath)
 {
  const bool Known=Current()&&IsValid(O)&&O->GetOutermost()==P&&O->GetPathName()==BeforePath;
  R->SetBoolField(TEXT("target_valid"),IsValid(O));R->SetBoolField(TEXT("change_state_known"),Known);
  if(IsValid(P))R->SetBoolField(TEXT("dirty_after"),P->IsDirty());
  if(!Known)R->SetStringField(TEXT("retry_risk"),TEXT("inspect_actual_target_before_any_new_request"));return Known;
 }
};
}

FString Blueprint(UObject* Object)
{
 FContext C(TEXT("inspect_blueprint_definition"));if(!C.Ready)return Encode(C.R);
 UBlueprint* BP=Cast<UBlueprint>(Object);if(!IsValid(BP))return C.Fail(TEXT("Explicit loaded Blueprint asset required"));
 C.R->SetStringField(TEXT("asset"),Path(BP));C.R->SetStringField(TEXT("parent_class"),Path(BP->ParentClass));C.R->SetStringField(TEXT("generated_class"),Path(BP->GeneratedClass));
 C.R->SetNumberField(TEXT("compile_status"),BP->Status);C.R->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());
 TArray<TSharedPtr<FJsonValue>> Vars,Components;TSet<UBlueprint*> Seen;UBlueprint* Current=BP;
 while(Current&&Seen.Num()<64&&!Seen.Contains(Current))
 {
  Seen.Add(Current);
  if(Current->NewVariables.Num()>16384)return C.Fail(TEXT("Variable inventory exceeds 16384 per class"));
  for(const auto& V:Current->NewVariables)
  {
   if(Vars.Num()>=16384)return C.Fail(TEXT("Variable inventory exceeds total16384"));
   auto O=MakeShared<J>();O->SetStringField(TEXT("name"),V.VarName.ToString());O->SetStringField(TEXT("owner_blueprint"),Path(Current));O->SetBoolField(TEXT("inherited"),Current!=BP);
   O->SetStringField(TEXT("category"),V.VarType.PinCategory.ToString());O->SetStringField(TEXT("subcategory"),V.VarType.PinSubCategory.ToString());O->SetStringField(TEXT("type_object"),Path(V.VarType.PinSubCategoryObject.Get()));O->SetNumberField(TEXT("container_type"),static_cast<int32>(V.VarType.ContainerType));O->SetStringField(TEXT("default_value"),V.DefaultValue.Left(4096));O->SetBoolField(TEXT("default_truncated"),V.DefaultValue.Len()>4096);Vars.Add(Row(O));
  }
  if(Current->SimpleConstructionScript)for(USCS_Node* N:Current->SimpleConstructionScript->GetAllNodes())if(IsValid(N))
  {
   if(Components.Num()>=16384)return C.Fail(TEXT("SCS inventory exceeds 16384"));auto O=MakeShared<J>();O->SetStringField(TEXT("name"),N->GetVariableName().ToString());O->SetStringField(TEXT("class"),Path(N->ComponentClass));O->SetStringField(TEXT("source_template"),Path(N->ComponentTemplate));O->SetStringField(TEXT("guid"),N->VariableGuid.ToString());O->SetStringField(TEXT("owner_blueprint"),Path(Current));O->SetBoolField(TEXT("inherited"),Current!=BP);Components.Add(Row(O));
  }
  Current=Current->ParentClass?Cast<UBlueprint>(Current->ParentClass->ClassGeneratedBy):nullptr;
 }
 C.R->SetArrayField(TEXT("variables"),Vars);C.R->SetArrayField(TEXT("scs_components"),Components);C.R->SetBoolField(TEXT("ancestry_truncated"),Current!=nullptr);
 // Do not create a CDO just to inspect source. Native defaults are distinct from SCS declarations.
 AActor* CDO=IsValid(BP->GeneratedClass)?Cast<AActor>(BP->GeneratedClass->GetDefaultObject(false)):nullptr;
 C.R->SetBoolField(TEXT("actor_cdo_available"),IsValid(CDO));TArray<TSharedPtr<FJsonValue>> NativeComponents;
 if(IsValid(CDO))
 {
  TInlineComponentArray<UActorComponent*> All;CDO->GetComponents(All);
  if(All.Num()>16384)return C.Fail(TEXT("Cached CDO component inventory exceeds bound"));
  for(UActorComponent* A:All)if(IsValid(A))
  {
   auto O=MakeShared<J>();O->SetStringField(TEXT("name"),A->GetName());O->SetStringField(TEXT("class"),Path(A->GetClass()));O->SetStringField(TEXT("template"),Path(A));O->SetNumberField(TEXT("creation_method"),static_cast<int32>(A->CreationMethod));NativeComponents.Add(Row(O));
  }
 }
 C.R->SetArrayField(TEXT("cached_cdo_components"),NativeComponents);
 C.R->SetStringField(TEXT("component_scope"),TEXT("Local/inherited SCS declarations plus already-created Actor CDO components. Missing CDO is unavailable, not proof of no native components; no CDO is created by this query."));return C.Done();
}

namespace
{
TArray<TSharedPtr<FJsonValue>> DataLayerNames(TArrayView<const FName> Names)
{
 TArray<TSharedPtr<FJsonValue>> Rows;
 for(FName Name:Names)
 {
  Rows.Add(MakeShared<FJsonValueString>(Name.ToString()));
 }
 return Rows;
}

constexpr int32 MaxActorMemberships=256;

bool LayerObjectReady(const UObject* Object)
{
 return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_WillBeLoaded|RF_BeginDestroyed|RF_FinishDestroyed);
}

bool ReadAuthoredLayerPaths(AActor* Actor,TArray<FSoftObjectPath>& Paths)
{
 const auto* Array=FindFProperty<FArrayProperty>(AActor::StaticClass(),AActor::GetDataLayerAssetsPropertyName());
 const auto* Inner=Array?CastField<FSoftObjectProperty>(Array->Inner):nullptr;
 if(!Inner||Inner->PropertyClass!=UDataLayerAsset::StaticClass())return false;
 FScriptArrayHelper Values(Array,Array->ContainerPtrToValuePtr<void>(Actor));
 if(Values.Num()>MaxActorMemberships)return false;
 Paths.Reset(Values.Num());
 for(int32 Index=0;Index<Values.Num();++Index)
 {
  Paths.Add(Inner->GetPropertyValue(Values.GetRawPtr(Index)).ToSoftObjectPath());
 }
 // AActor::PreSave deduplicates with a set. Membership has no authored order.
 Paths.Sort([](const FSoftObjectPath& A,const FSoftObjectPath& B){return A.LexicalLess(B);});
 return true;
}

TArray<TSharedPtr<FJsonValue>> SoftPathRows(const TArray<FSoftObjectPath>& Paths)
{
 TArray<TSharedPtr<FJsonValue>> Rows;
 for(const auto& PathValue:Paths)Rows.Add(MakeShared<FJsonValueString>(PathValue.ToString()));
 return Rows;
}

// Keep descriptor fields intact and report direct authored membership separately
// for the exact loaded actor. GetActor() on the descriptor can flush an async load.
void ReadLoadedDataLayers(const UWorldPartition& Partition,const FWorldPartitionActorDescInstance& Desc,
 const FString& ActorPath,FJsonObject& RowValue)
{
 auto Membership=MakeShared<J>();
 RowValue.SetObjectField(TEXT("loaded_data_layers"),Membership);
 Membership->SetBoolField(TEXT("available"),false);
 AActor* Actor=FindObject<AActor>(nullptr,*ActorPath);
 if(!LayerObjectReady(Actor)||Actor->IsActorBeingDestroyed()||Actor->IsTemplate()||
  Actor->GetWorld()!=Partition.GetWorld()||Actor->GetActorGuid()!=Desc.GetGuid())
 {
  Membership->SetStringField(TEXT("reason"),TEXT("live_actor_identity_unavailable"));
  return;
 }
 TArray<FSoftObjectPath> Paths;
 if(!ReadAuthoredLayerPaths(Actor,Paths))
 {
  Membership->SetStringField(TEXT("reason"),TEXT("authored_membership_unavailable"));
  return;
 }
 if(const auto* External=Actor->GetExternalDataLayerAsset())
 {
  Paths.AddUnique(FSoftObjectPath(External));
  Paths.Sort([](const FSoftObjectPath& A,const FSoftObjectPath& B){return A.LexicalLess(B);});
 }
 TArray<FName> InstanceNames;
 const auto* Manager=UDataLayerManager::GetDataLayerManager(Actor);
 bool AllResolved=true;
 for(const auto& AssetPath:Paths)
 {
  const auto* Asset=Cast<UDataLayerAsset>(AssetPath.ResolveObject());
  const auto* Instance=IsValid(Manager)&&LayerObjectReady(Asset)&&FSoftObjectPath(Asset)==AssetPath?
   Manager->GetDataLayerInstance(Asset):nullptr;
  if(LayerObjectReady(Instance))InstanceNames.AddUnique(Instance->GetFName());
  else AllResolved=false;
 }
 Membership->SetBoolField(TEXT("available"),true);
 Membership->SetArrayField(TEXT("asset_paths"),SoftPathRows(Paths));
 Membership->SetArrayField(TEXT("resolved_direct_instance_names"),DataLayerNames(InstanceNames));
 Membership->SetBoolField(TEXT("all_direct_instance_names_resolved"),AllResolved);
}

bool ReadPartitionActors(const UWorldPartition& Partition,int32 Offset,int32 Limit,FJsonObject& Inventory)
{
 struct FEntry
 {
  const FWorldPartitionActorDescInstance* Descriptor;
  FString ActorPath;
 };
 TArray<FEntry> Entries;
 for(FActorDescContainerInstanceCollection::TConstIterator<> It(&Partition);It;++It)
 {
  const FWorldPartitionActorDescInstance* Desc=*It;
  if(!Desc||!Desc->IsValid())continue;
  if(Entries.Num()==100000)return false;
  Entries.Add({Desc,Desc->GetActorSoftPath().ToString()});
 }
 Entries.Sort([](const FEntry& Left,const FEntry& Right)
 {
  const int32 PathOrder=Left.ActorPath.Compare(Right.ActorPath,ESearchCase::CaseSensitive);
  if(PathOrder!=0)return PathOrder<0;
  return Left.Descriptor->GetGuid()<Right.Descriptor->GetGuid();
 });

 TArray<TSharedPtr<FJsonValue>> Actors;
 const int32 End=FMath::Min(Entries.Num(),Offset+Limit);
 for(int32 Index=Offset;Index<End;++Index)
 {
  const FEntry& Entry=Entries[Index];
  const FWorldPartitionActorDescInstance& Desc=*Entry.Descriptor;
  auto Actor=MakeShared<J>();
  Actor->SetStringField(TEXT("guid"),Desc.GetGuid().ToString(EGuidFormats::DigitsWithHyphens));
  Actor->SetStringField(TEXT("actor_path"),Entry.ActorPath);
  Actor->SetStringField(TEXT("actor_package"),Desc.GetActorPackage().ToString());
  Actor->SetStringField(TEXT("label"),Desc.GetActorLabelOrName().ToString());
  Actor->SetStringField(TEXT("native_class"),Desc.GetNativeClass().ToString());
  const bool Loaded=Desc.IsLoaded();
  Actor->SetBoolField(TEXT("loaded"),Loaded);
  Actor->SetBoolField(TEXT("async_load_pending"),Desc.GetAsyncLoadID()!=INDEX_NONE);
  if(const UActorDescContainerInstance* Container=Desc.GetContainerInstance())
  {
   const FVector Position=Container->GetTransform().TransformPosition(Desc.GetActorTransform().GetLocation());
   if(!Position.ContainsNaN())Actor->SetArrayField(TEXT("descriptor_world_position"),Vector(Position));
  }
  const TArray<FName> AssetPaths=Desc.GetDataLayers();
  Actor->SetArrayField(TEXT("data_layer_asset_paths"),DataLayerNames(AssetPaths));
  const bool NamesResolved=Desc.HasResolvedDataLayerInstanceNames();
  Actor->SetBoolField(TEXT("data_layer_instance_names_resolved"),NamesResolved);
  if(NamesResolved)
  {
   Actor->SetArrayField(TEXT("data_layer_instance_names"),DataLayerNames(Desc.GetDataLayerInstanceNames().ToArray()));
  }
  if(Loaded)ReadLoadedDataLayers(Partition,Desc,Entry.ActorPath,*Actor);
  Actors.Add(Row(Actor));
 }
 Inventory.SetNumberField(TEXT("total"),Entries.Num());
 Inventory.SetNumberField(TEXT("offset"),Offset);
 Inventory.SetNumberField(TEXT("limit"),Limit);
 if(End<Entries.Num())Inventory.SetNumberField(TEXT("next_offset"),End);
 else Inventory.SetField(TEXT("next_offset"),MakeShared<FJsonValueNull>());
 Inventory.SetArrayField(TEXT("actors"),MoveTemp(Actors));
 return true;
}
}

FString World(bool bIncludePartitionActors,int32 ActorOffset,int32 ActorLimit)
{
 FContext C(TEXT("inspect_editor_world_configuration"));if(!C.Ready)return Encode(C.R);auto* W=C.EditorWorld;
 if(bIncludePartitionActors&&(ActorOffset<0||ActorOffset>100000||ActorLimit<1||ActorLimit>200))
  return C.Fail(TEXT("Partition actor offset must be 0..100000 and limit 1..200"));
 C.R->SetStringField(TEXT("world"),Path(W));auto* P=W->GetWorldPartition();C.R->SetStringField(TEXT("world_partition"),Path(P));C.R->SetBoolField(TEXT("partitioned"),P!=nullptr);
 if(P){C.R->SetBoolField(TEXT("partition_initialized"),P->IsInitialized());C.R->SetBoolField(TEXT("streaming_enabled"),P->IsStreamingEnabled());}
 TArray<TSharedPtr<FJsonValue>> Streams,Levels,Bookmarks;
 if(W->GetStreamingLevels().Num()>16384)return C.Fail(TEXT("Streaming level inventory exceeds bound"));
 for(ULevelStreaming* S:W->GetStreamingLevels())if(IsValid(S))
 {
  auto O=MakeShared<J>();O->SetStringField(TEXT("object"),Path(S));O->SetStringField(TEXT("world_asset"),S->GetWorldAssetPackageName());O->SetStringField(TEXT("loaded_level"),Path(S->GetLoadedLevel()));O->SetBoolField(TEXT("loaded"),S->GetLoadedLevel()!=nullptr);O->SetBoolField(TEXT("visible"),S->IsLevelVisible());O->SetBoolField(TEXT("should_be_loaded"),S->ShouldBeLoaded());O->SetBoolField(TEXT("should_be_visible"),S->ShouldBeVisible());O->SetBoolField(TEXT("visible_in_editor"),S->GetShouldBeVisibleInEditor());Streams.Add(Row(O));
 }
 for(ULevel* L:W->GetLevels())if(IsValid(L)){auto O=MakeShared<J>();O->SetStringField(TEXT("level"),Path(L));O->SetBoolField(TEXT("visible"),L->bIsVisible);O->SetBoolField(TEXT("persistent"),L==W->PersistentLevel);Levels.Add(Row(O));}
 auto* Settings=W->GetWorldSettings();C.R->SetStringField(TEXT("world_settings_class"),Settings?Path(Settings->GetClass()):FString());
 if(Settings)
 {
  const auto& B=Settings->GetBookmarks();if(B.Num()>4096)return C.Fail(TEXT("Bookmark inventory exceeds bound"));
  for(int32 I=0;I<B.Num();++I){auto O=MakeShared<J>();O->SetNumberField(TEXT("index"),I);O->SetStringField(TEXT("object"),Path(B[I]));if(IsValid(B[I]))O->SetStringField(TEXT("class"),Path(B[I]->GetClass()));if(auto* Standard=Cast<UBookMark>(B[I])){O->SetArrayField(TEXT("location"),Vector(Standard->Location));O->SetStringField(TEXT("rotation"),Standard->Rotation.ToString());}Bookmarks.Add(Row(O));}
 }
 if(bIncludePartitionActors)
 {
  auto Inventory=MakeShared<J>();
  const bool Available=P&&P->IsInitialized();
  Inventory->SetBoolField(TEXT("available"),Available);
  if(!Available)
  {
   Inventory->SetStringField(TEXT("reason"),P?TEXT("partition_not_initialized"):TEXT("not_partitioned"));
  }
  else if(!ReadPartitionActors(*P,ActorOffset,ActorLimit,*Inventory))
  {
   return C.Fail(TEXT("Partition actor inventory exceeds 100000 descriptors"));
  }
  C.R->SetObjectField(TEXT("partition_actor_inventory"),Inventory);
 }
 C.R->SetArrayField(TEXT("streaming_levels"),Streams);C.R->SetArrayField(TEXT("loaded_levels"),Levels);C.R->SetArrayField(TEXT("bookmarks"),Bookmarks);C.R->SetStringField(TEXT("scope"),TEXT("Current Editor world only. Does not load partition cells or calculate unloaded cell geometry/memory. Uses public AWorldSettings bookmark accessor without RX class dependency."));return C.Done();
}

namespace
{
bool CanPreserveMembershipOnSave(AActor* Actor,const UDataLayerManager* Manager,const TArray<FSoftObjectPath>& Paths)
{
 if(!Actor->SupportsDataLayerType(UDataLayerInstance::StaticClass()))return false;
 for(int32 Index=0;Index<Paths.Num();++Index)
 {
  const auto& AssetPath=Paths[Index];
  if(Index>0&&Paths[Index-1]==AssetPath)return false;
  const auto* Asset=Cast<UDataLayerAsset>(AssetPath.ResolveObject());
  if(!LayerObjectReady(Asset)||Asset->GetClass()!=UDataLayerAsset::StaticClass()||
   FSoftObjectPath(Asset)!=AssetPath||!Asset->CanBeReferencedByActor(Actor))return false;
  const UDataLayerInstance* Instance=Manager->GetDataLayerInstance(Asset);
  if(Instance&&Instance->GetRootExternalDataLayerInstance())return false;
 }
 return true;
}

struct FLayerActor
{
 TStrongObjectPtr<AActor> Actor;
 TStrongObjectPtr<UPackage> Package;
 FString Path;
 FString Filename;
 FString PackageName;
 FGuid Guid;
 TArray<FSoftObjectPath> Before;
 TArray<FSoftObjectPath> Expected;
 TMap<FString,FString> SaveSnapshot;
 bool DirtyBefore=false;
 bool NeedsAdd=false;
 bool AddAttempted=false;
 bool SaveAttempted=false;
 bool Saved=false;

 bool Current(const FContext& Context,const ULevel* Level) const
 {
  return LayerObjectReady(Actor.Get())&&LayerObjectReady(Package.Get())&&
   Context.EditorActor(Actor.Get())&&Actor->GetLevel()==Level&&Package->GetName()==PackageName&&
   Actor->GetPathName()==Path&&Actor->GetActorGuid()==Guid&&Actor->GetExternalPackage()==Package.Get()&&
   Actor->GetPackage()==Package.Get()&&!Actor->GetExternalDataLayerAsset()&&!Actor->IsRunningUserConstructionScript();
 }
};

// For saving, admit only the external package containing this actor and its owned objects.
// Compare editable values apart from the membership array before any package write.
bool CaptureLayerSaveScope(const FLayerActor& Target,TMap<FString,FString>& Snapshot)
{
 TArray<UObject*> Objects;
 GetObjectsWithPackage(Target.Package.Get(),Objects);
 if(Objects.Num()>256||!Objects.Contains(Target.Actor.Get()))return false;
 Snapshot.Reset();
 int64 Characters=0;
 for(UObject* Object:Objects)
 {
  if(Object==Target.Package.Get())continue;
  if(!IsValid(Object)||(Object!=Target.Actor.Get()&&!Object->IsIn(Target.Actor.Get())))return false;
  const FString ObjectKey=Object->GetPathName();
  Snapshot.Add(ObjectKey,Object->GetClass()->GetPathName());
  for(TFieldIterator<FProperty> It(Object->GetClass());It;++It)
  {
   const FProperty* Property=*It;
   const FName Name=Property->GetFName();
   const bool IsTarget=Object==Target.Actor.Get();
   // Actor metadata edited by the World Outliner may be serialized without CPF_Edit.
   // Transient visibility flags updated by DataLayerEditor are not persisted.
   if((!IsTarget&&!Property->HasAnyPropertyFlags(CPF_Edit))||
    Property->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_SkipSerialization)||
    (IsTarget&&Name==AActor::GetDataLayerAssetsPropertyName()))continue;
   for(int32 Index=0;Index<Property->ArrayDim;++Index)
   {
    const FString Key=ObjectKey+TEXT(".")+Property->GetName()+FString::Printf(TEXT("[%d]"),Index);
    FString Value;
    Property->ExportTextItem_Direct(Value,Property->ContainerPtrToValuePtr<void>(Object,Index),nullptr,Object,PPF_None);
    Characters+=Key.Len()+Value.Len();
    if(Characters>2*1024*1024)return false;
    Snapshot.Add(Key,MoveTemp(Value));
   }
  }
 }
 return true;
}
}

FString AddActorsToDataLayer(const TArray<FString>& ActorPaths,const FString& InstanceName,bool Execute,bool Save)
{
 FContext Context(TEXT("add_editor_actors_to_data_layer"));
 if(!Context.Ready)return Encode(Context.R);
 auto& Result=Context.R;
 Result->SetBoolField(TEXT("execute_requested"),Execute);
 Result->SetBoolField(TEXT("save_requested"),Save);
 if(Writing||ActorPaths.IsEmpty()||ActorPaths.Num()>32||InstanceName.IsEmpty()||InstanceName.Len()>256)
  return Context.Fail(TEXT("Idle editor, 1..32 actor paths and a bounded instance name required"));
 UWorld* World=Context.EditorWorld;
 ULevel* Level=World->PersistentLevel;
 UWorldPartition* Partition=World->GetWorldPartition();
 AWorldDataLayers* WorldLayers=IsValid(Level)?Level->GetWorldDataLayers():nullptr;
 UDataLayerManager* Manager=IsValid(Partition)?Partition->GetDataLayerManager():nullptr;
 if(!IsValid(Partition)||!Partition->IsInitialized()||!IsValid(WorldLayers)||!IsValid(Manager)||FLevelUtils::IsLevelLocked(Level))
  return Context.Fail(TEXT("Initialized World Partition and unlocked persistent level required"));
 const FName LayerName(*InstanceName,FNAME_Find);
 auto* Layer=const_cast<UDataLayerInstanceWithAsset*>(Cast<UDataLayerInstanceWithAsset>(WorldLayers->GetDataLayerInstance(LayerName)));
 const UDataLayerAsset* LayerAsset=IsValid(Layer)?Layer->GetAsset():nullptr;
 auto* Subsystem=GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
 if(!LayerObjectReady(Layer)||Layer->GetClass()!=UDataLayerInstanceWithAsset::StaticClass()||
  !LayerObjectReady(LayerAsset)||LayerAsset->GetClass()!=UDataLayerAsset::StaticClass()||
  Layer->GetFName()!=LayerName||Layer->GetOuterWorldDataLayers()!=WorldLayers||
  Layer->GetRootExternalDataLayerInstance()||Layer->IsReadOnly()||!IsValid(Subsystem))
  return Context.Fail(TEXT("Existing writable standard Data Layer instance in the current world required"));

 const TStrongObjectPtr<UWorld> WorldLease(World);
 const TStrongObjectPtr<ULevel> LevelLease(Level);
 const TStrongObjectPtr<UWorldPartition> PartitionLease(Partition);
 const TStrongObjectPtr<UDataLayerManager> ManagerLease(Manager);
 const TStrongObjectPtr<UDataLayerEditorSubsystem> SubsystemLease(Subsystem);
 const TStrongObjectPtr<AWorldDataLayers> WorldLayerLease(WorldLayers);
 const TStrongObjectPtr<UDataLayerInstanceWithAsset> LayerLease(Layer);
 const TStrongObjectPtr<const UDataLayerAsset> AssetLease(LayerAsset);
 const FSoftObjectPath LayerAssetPath(LayerAsset);
 const FString LevelPackage=Level->GetOutermost()->GetName();
 if(!LevelPackage.StartsWith(TEXT("/Game/"))||!FPackageName::DoesPackageExist(LevelPackage))
  return Context.Fail(TEXT("Saved project persistent level required"));
 const FString ExternalActorPrefix=ULevel::GetExternalActorsPath(LevelPackage)+TEXT("/");
 Result->SetStringField(TEXT("data_layer_instance_name"),InstanceName);
 Result->SetStringField(TEXT("data_layer_asset_path"),LayerAssetPath.ToString());

 TGuardValue<bool> Guard(Writing,true);
 TArray<FLayerActor> Targets;
 TSet<FString> SeenPaths;
 TSet<UPackage*> SeenPackages;
 bool ProtectedStateUncertain=false;
 auto ScopeCurrent=[&]()
 {
  return Context.Current()&&World->PersistentLevel==Level&&World->GetWorldPartition()==Partition&&IsValid(Partition)&&
   Partition->IsInitialized()&&IsValid(Manager)&&Partition->GetDataLayerManager()==Manager&&IsValid(Level)&&!FLevelUtils::IsLevelLocked(Level)&&
   IsValid(WorldLayers)&&Level->GetWorldDataLayers()==WorldLayers&&LayerObjectReady(Layer)&&LayerObjectReady(LayerAsset)&&
   WorldLayers->GetDataLayerInstance(LayerName)==Layer&&Layer->GetOuterWorldDataLayers()==WorldLayers&&
   Layer->GetAsset()==LayerAsset&&FSoftObjectPath(LayerAsset)==LayerAssetPath&&!Layer->IsReadOnly()&&IsValid(Subsystem);
 };
 auto Report=[&]()
 {
  const bool WorldKnown=Context.Current()&&World->PersistentLevel==Level;
  bool Known=WorldKnown&&!ProtectedStateUncertain;
  bool Changed=false;
  int32 SavedCount=0;
  int32 RequestedSaveCount=0;
  TArray<TSharedPtr<FJsonValue>> Rows;
  for(const auto& Target:Targets)
  {
   auto Item=MakeShared<J>();
   Item->SetStringField(TEXT("actor_path"),Target.Path);
   Item->SetStringField(TEXT("actor_package"),Target.PackageName);
   Item->SetBoolField(TEXT("would_add"),Target.NeedsAdd);
   Item->SetBoolField(TEXT("add_attempted"),Target.AddAttempted);
   Item->SetBoolField(TEXT("dirty_before"),Target.DirtyBefore);
   Item->SetArrayField(TEXT("memberships_before"),SoftPathRows(Target.Before));
   TArray<FSoftObjectPath> Actual;
   const bool Observed=WorldKnown&&Target.Current(Context,Level)&&ReadAuthoredLayerPaths(Target.Actor.Get(),Actual);
   Item->SetBoolField(TEXT("change_state_known"),Observed);
   if(Observed)
   {
    Item->SetBoolField(TEXT("changed"),Actual!=Target.Before);
    Item->SetBoolField(TEXT("dirty_after"),Target.Package->IsDirty());
    Item->SetArrayField(TEXT("memberships_after"),SoftPathRows(Actual));
    Changed|=Actual!=Target.Before;
   }
   else Item->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());
   Known&=Observed;
   Item->SetBoolField(TEXT("save_attempted"),Target.SaveAttempted);
   Item->SetBoolField(TEXT("saved"),Target.Saved);
   SavedCount+=Target.Saved?1:0;
   RequestedSaveCount+=(Save&&Target.NeedsAdd)?1:0;
   Rows.Add(Row(Item));
  }
  Result->SetArrayField(TEXT("actors"),MoveTemp(Rows));
  Result->SetNumberField(TEXT("saved_actor_count"),SavedCount);
  Result->SetBoolField(TEXT("saved"),SavedCount>0);
  Result->SetBoolField(TEXT("partial_save"),SavedCount>0&&SavedCount<RequestedSaveCount);
  Result->SetBoolField(TEXT("all_requested_actor_packages_saved"),RequestedSaveCount>0&&SavedCount==RequestedSaveCount);
  Result->SetBoolField(TEXT("change_state_known"),Known);
  if(Changed||Known)Result->SetBoolField(TEXT("changed"),Changed);
  else Result->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());
  Result->SetStringField(TEXT("retry_risk"),Result->GetBoolField(TEXT("mutation_attempted"))||!Known?
   TEXT("inspect_actual_targets_and_disk"):TEXT("none_observed"));
 };
 auto Fail=[&](const TCHAR* Error)
 {
  Report();
  return Context.Fail(Error);
 };
 for(const FString& ActorPath:ActorPaths)
 {
  Result->SetStringField(TEXT("current_actor"),ActorPath);
  if(!ActorPath.StartsWith(LevelPackage+TEXT("."))||ActorPath.Len()>=NAME_SIZE||!ActorPath.Contains(TEXT(":"))||SeenPaths.Contains(ActorPath))
   return Fail(TEXT("Unique exact actor paths in the current level required"));
  SeenPaths.Add(ActorPath);
  AActor* Actor=FindObject<AActor>(nullptr,*ActorPath);
  UPackage* Package=IsValid(Actor)?Actor->GetExternalPackage():nullptr;
  if(!ScopeCurrent()||!LayerObjectReady(Actor)||!Context.EditorActor(Actor)||Actor->GetPathName()!=ActorPath||Actor->GetLevel()!=Level||
   Actor->IsRunningUserConstructionScript()||Actor->GetExternalDataLayerAsset()||
   UDataLayerManager::GetDataLayerManager(Actor)!=Manager||
   !LayerObjectReady(Package)||Package!=Actor->GetPackage()||!Package->GetName().StartsWith(ExternalActorPrefix,ESearchCase::CaseSensitive)||
   SeenPackages.Contains(Package))
   return Fail(TEXT("Exact loaded external actor in the current persistent level required"));
  SeenPackages.Add(Package);
  FLayerActor Target;
  Target.Actor.Reset(Actor);
  Target.Package.Reset(Package);
  Target.Path=ActorPath;
  Target.PackageName=Package->GetName();
  Target.Guid=Actor->GetActorGuid();
  Target.DirtyBefore=Package->IsDirty();
  if(!Target.Guid.IsValid()||!ReadAuthoredLayerPaths(Actor,Target.Before))
   return Fail(TEXT("Actor identity or authored membership could not be observed"));
  Target.Expected=Target.Before;
  Target.NeedsAdd=!Target.Before.Contains(LayerAssetPath);
  if(Target.NeedsAdd&&Target.Before.Num()>=MaxActorMemberships)
   return Fail(TEXT("Adding the layer would exceed the 256 membership limit"));
  if(Save&&Target.NeedsAdd&&
   (Target.DirtyBefore||!FPackageName::DoesPackageExist(Package->GetName(),&Target.Filename)||
    IFileManager::Get().IsReadOnly(*Target.Filename)||!CanPreserveMembershipOnSave(Actor,Manager,Target.Before)||
    !CaptureLayerSaveScope(Target,Target.SaveSnapshot)))
   return Fail(TEXT("Save requires a clean writable actor package and existing memberships that the engine can preserve"));
  Targets.Add(MoveTemp(Target));
 }
 auto ExpectedState=[&]()
 {
  if(!ScopeCurrent())return false;
  for(const auto& Target:Targets)
  {
   TArray<FSoftObjectPath> Actual;
   if(!Target.Current(Context,Level)||!ReadAuthoredLayerPaths(Target.Actor.Get(),Actual)||Actual!=Target.Expected)
    return false;
   if(Save&&Target.NeedsAdd)
   {
    TMap<FString,FString> Now;
    if((!Target.AddAttempted&&Target.Package->IsDirty())||
     !CanPreserveMembershipOnSave(Target.Actor.Get(),Manager,Actual)||
     !CaptureLayerSaveScope(Target,Now)||!Now.OrderIndependentCompareEqual(Target.SaveSnapshot))return false;
   }
  }
  return true;
 };
 for(auto& Target:Targets)
 {
  Result->SetStringField(TEXT("current_actor"),Target.Path);
  const bool CanAdd=!Target.NeedsAdd||Layer->CanAddActor(Target.Actor.Get());
  if(!ExpectedState())
  {
   ProtectedStateUncertain=true;
   return Fail(TEXT("Actor membership, package contents or Data Layer changed during preflight"));
  }
  if(!CanAdd)return Fail(TEXT("Engine refused actor for this Data Layer"));
 }
 Result->RemoveField(TEXT("current_actor"));
 if(!Execute)
 {
  Report();
  Result->SetBoolField(TEXT("success"),true);
  Result->SetStringField(TEXT("stage"),TEXT("validated"));
  return Encode(Result);
 }

 Result->SetStringField(TEXT("stage"),TEXT("assign"));
 for(auto& Target:Targets)
 {
  if(!Target.NeedsAdd)continue;
  Result->SetStringField(TEXT("current_actor"),Target.Path);
  if(!ExpectedState())return Fail(TEXT("Observed state changed before assignment"));
  Target.AddAttempted=true;
  Result->SetBoolField(TEXT("mutation_attempted"),true);
  Target.Expected.Add(LayerAssetPath);
  Target.Expected.Sort([](const FSoftObjectPath& A,const FSoftObjectPath& B){return A.LexicalLess(B);});
  const bool Added=Subsystem->AddActorToDataLayer(Target.Actor.Get(),Layer);
  if(!ExpectedState()||!Added)
  {
   ProtectedStateUncertain=true;
   return Fail(TEXT("Assignment failed or a callback changed protected state; save skipped"));
  }
 }
 if(Save)
 {
  Result->SetStringField(TEXT("stage"),TEXT("save_actor_packages"));
  for(auto& Target:Targets)
  {
   if(!Target.AddAttempted)continue;
   Result->SetStringField(TEXT("current_actor"),Target.Path);
   FString Filename;
   if(!ExpectedState()||!FPackageName::DoesPackageExist(Target.Package->GetName(),&Filename)||
    Filename!=Target.Filename||IFileManager::Get().IsReadOnly(*Filename))
    return Fail(TEXT("Actor package changed or is no longer writable before save"));
   FSavePackageArgs Args;
   Args.TopLevelFlags=RF_Standalone;
   Args.SaveFlags=SAVE_NoError;
   Target.SaveAttempted=true;
   Result->SetBoolField(TEXT("save_attempted"),true);
   Target.Saved=UPackage::SavePackage(Target.Package.Get(),nullptr,*Filename,Args);
   const bool StatePreserved=ExpectedState();
   if(!StatePreserved)ProtectedStateUncertain=true;
   if(!Target.Saved)
   {
    Result->SetBoolField(TEXT("save_state_known"),false);
    return Fail(StatePreserved?
     TEXT("Actor package save failed; inspect actual disk state"):
     TEXT("Actor package save failed and a callback changed protected state; inspect actual targets and disk"));
   }
   if(!StatePreserved)return Fail(TEXT("A callback changed protected state after actor package save"));
  }
 }
 Result->RemoveField(TEXT("current_actor"));
 Report();
 return Context.Done();
}

FString Mesh(UObject* Object)
{
 FContext C(TEXT("inspect_static_mesh_metadata"));if(!C.Ready)return Encode(C.R);auto* M=Cast<UStaticMesh>(Object);if(!IsValid(M))return C.Fail(TEXT("StaticMesh asset required"));
 if(M->IsCompiling())return C.Fail(TEXT("StaticMesh build in progress; no blocking wait or implicit compile"));
 C.R->SetStringField(TEXT("asset"),Path(M));TArray<TSharedPtr<FJsonValue>> LODs,Sockets;
 const int32 Sources=M->GetNumSourceModels(),Rendered=M->GetNumLODs();if(Sources>64||Rendered>64||M->Sockets.Num()>4096)return C.Fail(TEXT("Mesh metadata inventory exceeds bound"));
 C.R->SetNumberField(TEXT("source_lod_count"),Sources);C.R->SetNumberField(TEXT("render_lod_count"),Rendered);
 for(int32 I=0;I<FMath::Max(Sources,Rendered);++I)
 {
  auto O=MakeShared<J>();O->SetNumberField(TEXT("lod"),I);if(I<Rendered)O->SetNumberField(TEXT("uv_channels"),M->GetNumUVChannels(I));else O->SetBoolField(TEXT("render_lod_available"),false);
  if(I<Sources){const auto& S=M->GetSourceModel(I);auto Build=MakeShared<J>(),Reduction=MakeShared<J>();if(!FJsonObjectConverter::UStructToJsonObject(FMeshBuildSettings::StaticStruct(),&S.BuildSettings,Build,0,0)||!FJsonObjectConverter::UStructToJsonObject(FMeshReductionSettings::StaticStruct(),&S.ReductionSettings,Reduction,0,0))return C.Fail(TEXT("Source-model settings serialization failed"));O->SetObjectField(TEXT("build_settings"),Build);O->SetObjectField(TEXT("reduction_settings"),Reduction);}
  LODs.Add(Row(O));
 }
 for(UStaticMeshSocket* S:M->Sockets)if(IsValid(S)){auto O=MakeShared<J>();O->SetStringField(TEXT("name"),S->SocketName.ToString());O->SetStringField(TEXT("object"),Path(S));O->SetArrayField(TEXT("relative_location"),Vector(S->RelativeLocation));O->SetArrayField(TEXT("relative_scale"),Vector(S->RelativeScale));O->SetStringField(TEXT("relative_rotation"),S->RelativeRotation.ToString());Sockets.Add(Row(O));}
 C.R->SetArrayField(TEXT("lods"),LODs);C.R->SetArrayField(TEXT("sockets"),Sockets);C.R->SetBoolField(TEXT("dirty"),M->GetOutermost()->IsDirty());return C.Done();
}

FString Component(UObject* Object)
{
 FContext C(TEXT("inspect_component_authoring_data"));if(!C.Ready)return Encode(C.R);auto* A=Cast<UActorComponent>(Object);if(!IsValid(A))return C.Fail(TEXT("Loaded ActorComponent or source template required"));
 C.R->SetStringField(TEXT("component"),Path(A));C.R->SetStringField(TEXT("class"),Path(A->GetClass()));C.R->SetBoolField(TEXT("is_template"),A->IsTemplate());C.R->SetBoolField(TEXT("registered"),A->IsRegistered());
 bool Supported=false;
 if(auto* M=Cast<UStaticMeshComponent>(A))
 {
  Supported=true;TArray<TSharedPtr<FJsonValue>> LODs;if(M->LODData.Num()>64)return C.Fail(TEXT("Component LOD inventory exceeds bound"));
  for(int32 I=0;I<M->LODData.Num();++I){const auto& L=M->LODData[I];auto O=MakeShared<J>();O->SetNumberField(TEXT("lod"),I);O->SetNumberField(TEXT("painted_vertex_count"),L.PaintedVertices.Num());O->SetBoolField(TEXT("has_override_vertex_colors"),L.OverrideVertexColors!=nullptr);if(L.OverrideVertexColors)O->SetNumberField(TEXT("override_vertex_count"),L.OverrideVertexColors->GetNumVertices());O->SetStringField(TEXT("map_build_data_id"),L.MapBuildDataId.ToString());LODs.Add(Row(O));}C.R->SetArrayField(TEXT("lod_data"),LODs);C.R->SetStringField(TEXT("static_mesh"),Path(M->GetStaticMesh()));
 }
 if(auto* F=Cast<UExponentialHeightFogComponent>(A)){Supported=true;C.R->SetBoolField(TEXT("enable_volumetric_fog"),F->bEnableVolumetricFog);C.R->SetNumberField(TEXT("volumetric_fog_distance"),F->VolumetricFogDistance);C.R->SetNumberField(TEXT("volumetric_fog_extinction_scale"),F->VolumetricFogExtinctionScale);C.R->SetNumberField(TEXT("volumetric_fog_scattering_distribution"),F->VolumetricFogScatteringDistribution);}
 if(auto* S=Cast<USkeletalMeshComponent>(A)){Supported=true;const auto& D=S->AnimationData;C.R->SetBoolField(TEXT("saved_looping"),D.bSavedLooping);C.R->SetBoolField(TEXT("saved_playing"),D.bSavedPlaying);C.R->SetNumberField(TEXT("saved_position"),D.SavedPosition);C.R->SetNumberField(TEXT("saved_play_rate"),D.SavedPlayRate);C.R->SetStringField(TEXT("saved_animation"),Path(D.AnimToPlay));C.R->SetStringField(TEXT("animation_scope"),TEXT("Authored SingleAnimationPlayData defaults, not current animation playback state"));}
 if(!Supported)return C.Fail(TEXT("Detailed authoring data supported for StaticMesh, SkeletalMesh or ExponentialHeightFog components"));return C.Done();
}

FString Widget(UObject* Object)
{
 FContext C(TEXT("inspect_widget_animation_data"));if(!C.Ready)return Encode(C.R);auto* BP=Cast<UWidgetBlueprint>(Object);if(!IsValid(BP)||!IsValid(BP->WidgetTree))return C.Fail(TEXT("Source WidgetBlueprint with a valid WidgetTree required"));
 C.R->SetStringField(TEXT("asset"),Path(BP));C.R->SetStringField(TEXT("parent_class"),Path(BP->ParentClass));TArray<TSharedPtr<FJsonValue>> Widgets,Animations;TArray<UWidget*> All;BP->WidgetTree->GetAllWidgets(All);if(All.Num()>16384||BP->Animations.Num()>4096)return C.Fail(TEXT("Widget inventory exceeds bound"));
 for(auto* W:All)if(IsValid(W)){auto O=MakeShared<J>();O->SetStringField(TEXT("name"),W->GetName());O->SetStringField(TEXT("class"),Path(W->GetClass()));O->SetStringField(TEXT("parent"),Path(W->GetParent()));Widgets.Add(Row(O));}
 int32 SectionCount=0;
 for(UWidgetAnimation* A:BP->Animations)if(IsValid(A))
 {
  auto O=MakeShared<J>();O->SetStringField(TEXT("name"),A->GetName());O->SetNumberField(TEXT("start_seconds"),A->GetStartTime());O->SetNumberField(TEXT("end_seconds"),A->GetEndTime());TArray<TSharedPtr<FJsonValue>> Sections;
  if(const UMovieScene* MS=A->GetMovieScene())
  {
   O->SetNumberField(TEXT("display_rate_numerator"),MS->GetDisplayRate().Numerator);O->SetNumberField(TEXT("display_rate_denominator"),MS->GetDisplayRate().Denominator);O->SetNumberField(TEXT("tick_resolution_numerator"),MS->GetTickResolution().Numerator);O->SetNumberField(TEXT("tick_resolution_denominator"),MS->GetTickResolution().Denominator);
   for(const auto& Binding:MS->GetBindings())for(UMovieSceneTrack* T:Binding.GetTracks())if(IsValid(T))for(UMovieSceneSection* S:T->GetAllSections())if(IsValid(S))
   {
    if(++SectionCount>16384)return C.Fail(TEXT("Animation section inventory exceeds bound"));auto V=MakeShared<J>();V->SetStringField(TEXT("binding_guid"),Binding.GetObjectGuid().ToString());V->SetStringField(TEXT("track_class"),Path(T->GetClass()));V->SetStringField(TEXT("section"),Path(S));if(auto* X=Cast<UMovieScene2DTransformSection>(S))V->SetNumberField(TEXT("transform_mask"),static_cast<uint32>(X->GetMask().GetChannels()));Sections.Add(Row(V));
   }
  }
  O->SetArrayField(TEXT("sections"),Sections);Animations.Add(Row(O));
 }
 C.R->SetArrayField(TEXT("widgets"),Widgets);C.R->SetArrayField(TEXT("animations"),Animations);C.R->SetStringField(TEXT("scope"),TEXT("Source-local tree/animations; parent class identified, no generated runtime instance created"));return C.Done();
}

FString PCG(UObject* Object)
{
 FContext C(TEXT("inspect_pcg_extraction_settings"));if(!C.Ready)return Encode(C.R);if(!IsValid(Object))return C.Fail(TEXT("Exact loaded PCG node settings required"));C.R->SetStringField(TEXT("settings"),Path(Object));
 if(auto* P=Cast<UPCGGetActorPropertySettings>(Object))
 {
  C.R->SetStringField(TEXT("settings_class"),Path(P->GetClass()));C.R->SetStringField(TEXT("struct_extractor_behavior"),UEnum::GetValueAsString(P->StructExtractorBehavior));C.R->SetStringField(TEXT("object_extractor_behavior"),UEnum::GetValueAsString(P->ObjectExtractorBehavior));C.R->SetStringField(TEXT("container_extractor_behavior"),UEnum::GetValueAsString(P->ContainerExtractorBehavior));
  auto* Legacy=FindFProperty<FBoolProperty>(P->GetClass(),TEXT("bForceObjectAndStructExtraction"));if(Legacy)C.R->SetBoolField(TEXT("deprecated_force_object_and_struct_extraction"),Legacy->GetPropertyValue_InContainer(P));C.R->SetStringField(TEXT("legacy_note"),TEXT("UE5.8 deprecated compatibility value; current extractor behavior enums are authoritative"));
 }
 else if(auto* Polygon=Cast<UPCGPolygon2DOperationSettings>(Object)){C.R->SetStringField(TEXT("polygon_operation"),UEnum::GetValueAsString(Polygon->Operation));C.R->SetStringField(TEXT("metadata_mode"),UEnum::GetValueAsString(Polygon->MetadataMode));}
 else return C.Fail(TEXT("GetActorProperty or Polygon2DOperation settings required"));return C.Done();
}

FString Performance()
{
 FContext C(TEXT("inspect_editor_performance_settings"));if(!C.Ready)return Encode(C.R);auto* O=GetDefault<UEditorPerformanceSettings>();auto Values=MakeShared<J>();
 for(TFieldIterator<FProperty> It(O->GetClass());It;++It)if(It->HasAnyPropertyFlags(CPF_Config))
 {
  FString V;It->ExportText_InContainer(0,V,O,O,const_cast<UEditorPerformanceSettings*>(O),PPF_None);Values->SetStringField(It->GetName(),V.Left(4096));
 }
 C.R->SetObjectField(TEXT("config_values"),Values);C.R->SetStringField(TEXT("scope"),TEXT("Read-only effective Editor performance CDO configuration; no settings or ini changes"));return C.Done();
}

FString RefreshActor(UObject* Object,bool Execute)
{
 FContext C(TEXT("rerun_actor_construction"));if(!C.Ready)return Encode(C.R);auto* A=Cast<AActor>(Object);if(Writing||!C.EditorActor(A)||(A->IsRunningUserConstructionScript()||FLevelUtils::IsLevelLocked(A->GetLevel())))return C.Fail(TEXT("Exact live Editor actor outside construction and another mutation required"));
 if(FScopedSuspendRerunConstructionScripts::IsSuspended()||GIsReconstructingBlueprintInstances||A->bActorSeamlessTraveled)return C.Fail(TEXT("Construction is suspended/reconstructing or actor seamlessly traveled; no deferred/redirected work queued"));
 if(A->IsChildActor())return C.Fail(TEXT("ChildActor reconstruction redirects to its parent; explicitly target that parent instead"));
 TArray<UBlueprint*> Hierarchy;if(!UBlueprint::GetBlueprintHierarchyFromClass(A->GetClass(),Hierarchy))return C.Fail(TEXT("Blueprint hierarchy has compile errors; no construction call"));
 for(UBlueprint* BP:Hierarchy)if(!IsValid(BP)||BP->bBeingCompiled)return C.Fail(TEXT("Blueprint hierarchy is being compiled; no construction call"));
 TGuardValue<bool> Guard(Writing,true);TStrongObjectPtr<AActor> Keep(A);auto* P=A->GetOutermost();TStrongObjectPtr<UPackage> KeepPackage(P);const FString Before=Path(A);C.R->SetStringField(TEXT("actor"),Before);C.R->SetBoolField(TEXT("dirty_before"),P->IsDirty());C.R->SetBoolField(TEXT("construction_may_replace_components"),true);
 if(!Execute){C.R->SetBoolField(TEXT("dry_run"),true);return C.Done();}
 C.R->SetStringField(TEXT("stage"),TEXT("rerun_construction_scripts"));C.R->SetBoolField(TEXT("mutation_attempted"),true);C.R->SetBoolField(TEXT("changed"),true);C.R->SetBoolField(TEXT("change_state_known"),false);A->RerunConstructionScripts();C.R->SetBoolField(TEXT("SDK_returned"),true);
 if(!C.After(A,P,Before)||!C.EditorActor(A))return C.Fail(TEXT("Actor/context changed during construction callback; inspect actual state, never retry automatically"));
 C.R->SetBoolField(TEXT("construction_call_returned"),true);C.R->SetBoolField(TEXT("construction_side_effects_verified"),false);C.R->SetStringField(TEXT("component_reference_policy"),TEXT("Construction may replace components; resolve new component paths before further edits"));return C.Done();
}

FString RefreshComponent(UObject* Object,bool Execute)
{
 FContext C(TEXT("refresh_component_render_state"));if(!C.Ready)return Encode(C.R);auto* A=Cast<UActorComponent>(Object);if(Writing||!IsValid(A)||A->IsTemplate()||!A->IsRegistered()||!C.EditorActor(A->GetOwner())||FLevelUtils::IsLevelLocked(A->GetOwner()->GetLevel())||A->GetWorld()!=C.EditorWorld)return C.Fail(TEXT("Exact registered component on a live Editor actor required"));
 TGuardValue<bool> Guard(Writing,true);TStrongObjectPtr<UActorComponent> Keep(A);auto* P=A->GetOutermost();TStrongObjectPtr<UPackage> KeepPackage(P);const FString Before=Path(A);C.R->SetStringField(TEXT("component"),Before);C.R->SetBoolField(TEXT("dirty_before"),P->IsDirty());
 if(!Execute){C.R->SetBoolField(TEXT("dry_run"),true);return C.Done();}
 C.R->SetStringField(TEXT("stage"),TEXT("mark_render_state_dirty"));C.R->SetBoolField(TEXT("mutation_attempted"),true);C.R->SetBoolField(TEXT("changed"),true);C.R->SetBoolField(TEXT("change_state_known"),false);A->MarkRenderStateDirty();C.R->SetBoolField(TEXT("SDK_returned"),true);
 if(!C.After(A,P,Before)||!C.EditorActor(A->GetOwner())||A->GetWorld()!=C.EditorWorld)return C.Fail(TEXT("Component/owner/context changed in render-state notification; inspect before any new request"));
 C.R->SetBoolField(TEXT("render_update_queued"),A->IsRenderStateDirty());C.R->SetBoolField(TEXT("render_thread_completion_observed"),false);return C.Done();
}

FString PackageDirty(UObject* Object,bool Dirty,bool AllowClear)
{
 FContext C(TEXT("set_asset_package_dirty"));if(!C.Ready)return Encode(C.R);
 if(Writing||!IsValid(Object)||!Object->IsAsset()||Object->IsA<UWorld>()||Object->GetOutermost()->ContainsMap()||!Object->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))return C.Fail(TEXT("Explicit /Game top-level asset required; world/actor/component/config packages are not accepted"));
 if(!Dirty&&!AllowClear)return C.Fail(TEXT("Clearing dirty state requires allow_clear=true; it neither saves nor reverts in-memory content"));
 TGuardValue<bool> Guard(Writing,true);TStrongObjectPtr<UObject> Keep(Object);auto* P=Object->GetOutermost();TStrongObjectPtr<UPackage> KeepPackage(P);const FString Before=Path(Object);C.R->SetBoolField(TEXT("dirty_before"),P->IsDirty());C.R->SetStringField(TEXT("asset"),Before);
 C.R->SetStringField(TEXT("stage"),TEXT("set_dirty_flag"));C.R->SetBoolField(TEXT("mutation_attempted"),true);C.R->SetBoolField(TEXT("changed"),P->IsDirty()!=Dirty);C.R->SetBoolField(TEXT("change_state_known"),false);P->SetDirtyFlag(Dirty);C.R->SetBoolField(TEXT("SDK_returned"),true);
 if(!C.After(Object,P,Before))return C.Fail(TEXT("Asset/context changed in dirty notification; inspect actual package state"));
 C.R->SetBoolField(TEXT("content_saved_or_reverted"),false);if(P->IsDirty()!=Dirty)return C.Fail(TEXT("Observed dirty flag differs from requested value after notification"));return C.Done();
}

FString Spawn(UClass* Class,const FString& Name,const FTransform& Transform,bool Execute)
{
 FContext C(TEXT("spawn_editor_actor_deferred"));if(!C.Ready)return Encode(C.R);
 if(Writing||!IsValid(Class)||!Class->IsChildOf(AActor::StaticClass())||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_NotPlaceable)||!NameOK(Name)||!Transform.IsValid())return C.Fail(TEXT("Placeable Actor class, explicit nonreserved ASCII name and finite transform required"));
 ULevel* L=C.EditorWorld->PersistentLevel.Get();if(!IsValid(L)||FLevelUtils::IsLevelLocked(L)||FindObject<UObject>(L,*Name))return C.Fail(TEXT("Named target already exists or persistent level unavailable; no replacement"));
 TGuardValue<bool> Guard(Writing,true);TStrongObjectPtr<UClass> KeepClass(Class);TStrongObjectPtr<ULevel> KeepLevel(L);TStrongObjectPtr<UPackage> KeepPackage(L->GetOutermost());C.R->SetStringField(TEXT("world"),Path(C.EditorWorld));C.R->SetStringField(TEXT("target_level"),Path(L));C.R->SetStringField(TEXT("requested_name"),Name);C.R->SetBoolField(TEXT("dirty_before"),L->GetOutermost()->IsDirty());
 using FAudit=DomainExpansion::FDomainDependencyAudit;
 for(UClass* Parent=Class;Parent;Parent=Parent->GetSuperClass())if(FAudit::IsMCPDependencyReference(Parent->GetPathName()))return C.Fail(TEXT("Actor class hierarchy references MCP development types; no actor spawned"));
 if(auto* Source=Cast<UBlueprint>(Class->ClassGeneratedBy))
 {
  if(Source->GeneratedClass!=Class||Source->bBeingCompiled||Source->Status==BS_Error)return C.Fail(TEXT("Current compiled Blueprint generated class required; skeleton/reinstancing/compile-error classes are not spawned"));
  FString Error;if(!FAudit::ValidateMCPDependencyIndependence(TEXT("actor_blueprint"),Source,DomainExpansion::EDomainAuditMode::InMemoryOnly,Error)){C.R->SetStringField(TEXT("dependency_error"),Error);return C.Fail(TEXT("Actor Blueprint dependency audit failed before spawn"));}
  if(!C.Current()||!IsValid(Class)||!IsValid(Source)||Source->GeneratedClass!=Class||Source->bBeingCompiled||Source->Status==BS_Error||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_NotPlaceable)||!IsValid(L)||C.EditorWorld->PersistentLevel!=L||FLevelUtils::IsLevelLocked(L))return C.Fail(TEXT("Class/compile/level context changed during dependency inspection; no actor spawned"));
 }
 if(!Execute){C.R->SetBoolField(TEXT("dry_run"),true);return C.Done();}
 C.R->SetStringField(TEXT("stage"),TEXT("deferred_spawn"));C.R->SetBoolField(TEXT("mutation_attempted"),true);C.R->SetBoolField(TEXT("change_state_known"),false);
 FActorSpawnParameters Params;Params.Name=FName(*Name);Params.NameMode=FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;Params.OverrideLevel=L;Params.bDeferConstruction=true;Params.ObjectFlags=RF_Transactional;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 AActor* A=C.EditorWorld->SpawnActor<AActor>(Class,Transform,Params);C.R->SetBoolField(TEXT("spawn_SDK_returned"),true);C.R->SetBoolField(TEXT("changed"),IsValid(A));
 if(!IsValid(A)){C.R->SetBoolField(TEXT("dirty_after"),L->GetOutermost()->IsDirty());return C.Fail(TEXT("Deferred spawn did not yield a live actor; inspect level before a new request"));}
 TStrongObjectPtr<AActor> Keep(A);const FString ActorPath=Path(A);auto* Package=A->GetOutermost();TStrongObjectPtr<UPackage> KeepActorPackage(Package);C.R->SetStringField(TEXT("actor"),ActorPath);
 if(!C.After(A,Package,ActorPath)||!C.EditorActor(A)||A->GetLevel()!=L||A->GetFName()!=FName(*Name))return C.Fail(TEXT("Deferred actor/context changed before FinishSpawning; partial actor retained"));
 C.R->SetStringField(TEXT("stage"),TEXT("finish_spawning"));C.R->SetBoolField(TEXT("change_state_known"),false);A->FinishSpawning(Transform);C.R->SetBoolField(TEXT("finish_SDK_returned"),true);
 if(!C.After(A,Package,ActorPath)||!C.EditorActor(A)||A->GetLevel()!=L)return C.Fail(TEXT("Actor/context changed in construction/finish callbacks; no cleanup or retry"));
 C.R->SetStringField(TEXT("actual_transform"),A->GetActorTransform().ToString());C.R->SetBoolField(TEXT("finish_spawning_returned"),true);C.R->SetBoolField(TEXT("construction_side_effects_verified"),false);return C.Done();
}
}
