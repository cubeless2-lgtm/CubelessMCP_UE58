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

FString World()
{
 FContext C(TEXT("inspect_editor_world_configuration"));if(!C.Ready)return Encode(C.R);auto* W=C.EditorWorld;
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
 C.R->SetArrayField(TEXT("streaming_levels"),Streams);C.R->SetArrayField(TEXT("loaded_levels"),Levels);C.R->SetArrayField(TEXT("bookmarks"),Bookmarks);C.R->SetStringField(TEXT("scope"),TEXT("Current Editor world only. Does not load partition cells or calculate unloaded cell geometry/memory. Uses public AWorldSettings bookmark accessor without RX class dependency."));return C.Done();
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
