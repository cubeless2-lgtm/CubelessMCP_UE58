#include "Compatibility/UE58/CubelessInstanceVertexColorsApi.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ComponentReregisterContext.h"
#include "GameFramework/Actor.h"
#include "LevelUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "MeshPaintHelpers.h"
#include "StaticMeshResources.h"
#include "StaticMeshComponentLODInfo.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Crc.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::InstanceVertexColors
{
namespace
{
using J=FJsonObject;
constexpr int32 MaxVertices=1048576;
bool Writing=false;
FString Encode(const TSharedRef<J>& R){FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;}
FString Digest(const TArray<FColor>& Colors){return FString::Printf(TEXT("%08x"),FCrc::MemCrc32(Colors.GetData(),Colors.Num()*sizeof(FColor)));}
bool CopyBuffer(const FColorVertexBuffer* B,TArray<FColor>& Out)
{
 Out.Reset();if(!B)return true;
 if(B->GetNumVertices()>MaxVertices||(B->GetNumVertices()&&!B->GetVertexData()))return false;
 B->GetVertexColors(Out);return true;
}
struct FContext
{
 TSharedRef<J> R=MakeShared<J>();
 TStrongObjectPtr<UStaticMeshComponent> C;
 TStrongObjectPtr<AActor> Actor;
 TStrongObjectPtr<UStaticMesh> Mesh;
 TStrongObjectPtr<UPackage> Package;
 TWeakObjectPtr<UWorld> World;
 UEditorEngine* Editor=nullptr;
 UObject* Outer=nullptr;
 ULevel* Level=nullptr;
 FStaticMeshRenderData* Render=nullptr;
 FString Path,MeshPath,DerivedKey,ExpectedComponentKey,SaveFilename,PackageName;
 FTransform Relative;
 TArray<TObjectPtr<UMaterialInterface>> Materials;
 TArray<TArray<FColor>> Before;
 TArray<TArray<FPaintedVertex>> BeforeCache;
 TArray<TArray<FPaintedVertex>> RebuiltCache;
 TArray<FGuid> BuildIds;
 TArray<FGuid> OriginalBuildIds;
 TArray<TArray<FColor>> MeshColors;
 bool MeshDirty=false,PackageDirty=false,CustomLOD=false,ExpectedCustomLOD=false,RequirePaintPolicy=false,Ready=false;
 bool HasStoredColors=false,StaleKey=false,ClearOperation=false,SaveCachePrepared=false;
 int32 LOD=0,NumVertices=0;
 explicit FContext(const TCHAR* Operation,UObject* Object,int32 InLOD):LOD(InLOD)
 {
  R->SetStringField(TEXT("operation"),Operation);R->SetStringField(TEXT("stage"),TEXT("preflight"));
  for(const TCHAR* K:{TEXT("success"),TEXT("mutation_attempted"),TEXT("writer_entered"),TEXT("changed"),TEXT("compile_attempted"),TEXT("save_attempted"),TEXT("saved"),TEXT("atomic_rollback"),TEXT("automatic_retry"),TEXT("render_update_requested"),TEXT("gpu_completion_verified")})R->SetBoolField(K,false);
  R->SetBoolField(TEXT("change_state_known"),true);R->SetStringField(TEXT("retry_risk"),TEXT("none_observed"));
  if(!IsInGameThread()||!GEditor||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor||IsGarbageCollecting()||UE::IsSavingPackage()||IsEngineExitRequested()||Writing){Error(TEXT("editor_busy"),TEXT("Idle Editor game thread required; no PIE/SIE, GC, save, exit or recursive color operation."));return;}
  auto* Component=Cast<UStaticMeshComponent>(Object);
  if(!IsValid(Component)||Component->IsTemplate()||Cast<UInstancedStaticMeshComponent>(Component)){Error(TEXT("static_mesh_component_required"),TEXT("Explicit placed StaticMeshComponent required. Not Blueprint/CDO templates, SkeletalMesh or per-ISM/HISM instance coloring."));return;}
  auto* A=Component->GetOwner();UStaticMesh* M=Component->GetStaticMesh();
  if(!IsValid(A)||A->IsTemplate()||A->IsActorBeingDestroyed()||!IsValid(M)||M->IsCompiling()||!Component->IsRegistered()){Error(TEXT("live_registered_component_required"),TEXT("Live registered placed component with a ready StaticMesh is required; no compilation wait."));return;}
  auto* W=GEditor->GetEditorWorldContext().World();
  if(!IsValid(W)||Component->GetWorld()!=W||A->GetWorld()!=W||W->WorldType!=EWorldType::Editor||!A->GetLevel()||FLevelUtils::IsLevelLocked(A->GetLevel())||!A->IsEditable()){Error(TEXT("editable_editor_world_required"),TEXT("Component must belong to the current editable Editor world and an unlocked level."));return;}
  auto* P=Component->GetPackage();
  if(!P||!P->GetName().StartsWith(TEXT("/Game/"))){Error(TEXT("game_package_required"),TEXT("A placed component owned by a /Game level or external-actor package is required."));return;}
  auto* Data=M->GetRenderData();
  if(!Data||Data->LODResources.Num()>64||!Data->LODResources.IsValidIndex(LOD)){Error(TEXT("invalid_lod"),TEXT("LOD must identify an existing render LOD (maximum64)."));return;}
  const auto& L=Data->LODResources[LOD];
  if(!L.GetNumVertices()||L.GetNumVertices()>MaxVertices||L.VertexBuffers.PositionVertexBuffer.GetNumVertices()!=L.GetNumVertices()||!L.VertexBuffers.PositionVertexBuffer.GetVertexData()||int64(L.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices())<int64(L.GetNumVertices())||!L.VertexBuffers.StaticMeshVertexBuffer.GetTangentData()){Error(TEXT("cpu_render_vertices_unavailable"),TEXT("Ready CPU render positions/tangents, with1..1048576 vertices, are required. Indices are render vertices including seam duplicates."));return;}
  if(Component->LODData.Num()>Data->LODResources.Num()){Error(TEXT("stale_component_lods"),TEXT("Component has stale LOD entries; inspect/reconcile mesh changes before painting."));return;}
  int64 Total=0,TotalCached=0;
  for(int32 I=0;I<Component->LODData.Num();++I)
  {
   const auto& D=Component->LODData[I];TArray<FColor> Colors;
   if(!CopyBuffer(D.OverrideVertexColors,Colors)||(D.OverrideVertexColors&&Colors.Num()!=Data->LODResources[I].GetNumVertices())||(Total+=Colors.Num())>MaxVertices){Error(TEXT("stale_or_unavailable_override"),TEXT("Existing override buffers must have current render vertex counts and CPU data; total snapshot bound1048576."));return;}
   if((TotalCached+=D.PaintedVertices.Num())>MaxVertices){Error(TEXT("painted_cache_bound"),TEXT("Painted vertex cache snapshot exceeds1048576 vertices"));return;}
   if(D.OverrideVertexColors&&D.PaintedVertices.Num()<=1)
   {
    const auto& Other=Data->LODResources[I];
    if(Other.VertexBuffers.PositionVertexBuffer.GetNumVertices()!=Other.GetNumVertices()||!Other.VertexBuffers.PositionVertexBuffer.GetVertexData()||int64(Other.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices())<int64(Other.GetNumVertices())||!Other.VertexBuffers.StaticMeshVertexBuffer.GetTangentData()){Error(TEXT("other_lod_cpu_cache_unavailable"),TEXT("Engine cache rebuild can access every overridden LOD with an empty/single painted cache. CPU positions/tangents are required for those LODs too."));return;}
   }
   TArray<FPaintedVertex> Cached=D.PaintedVertices;
   HasStoredColors|=D.OverrideVertexColors!=nullptr||Cached.Num()>0;
   BeforeCache.Add(MoveTemp(Cached));Before.Add(MoveTemp(Colors));BuildIds.Add(D.MapBuildDataId);OriginalBuildIds.Add(D.OriginalMapBuildDataId);
  }
  Total=0;
  for(const auto& D:Data->LODResources)
  {
   TArray<FColor> Colors;if(!CopyBuffer(&D.VertexBuffers.ColorVertexBuffer,Colors)||(Total+=Colors.Num())>MaxVertices){Error(TEXT("mesh_colors_unavailable"),TEXT("Source mesh color CPU data unavailable or source-color preservation snapshot exceeds1048576."));return;}
   MeshColors.Add(MoveTemp(Colors));
  }
  C.Reset(Component);Actor.Reset(A);Mesh.Reset(M);Package.Reset(P);World=W;Editor=GEditor;Outer=C->GetOuter();Level=A->GetLevel();Render=Data;
  Path=C->GetPathName();MeshPath=M->GetPathName();PackageName=P->GetName();DerivedKey=Data->DerivedDataKey;Relative=C->GetRelativeTransform();Materials=C->OverrideMaterials;
  MeshDirty=M->GetPackage()->IsDirty();PackageDirty=P->IsDirty();CustomLOD=C->bCustomOverrideVertexColorPerLOD;ExpectedCustomLOD=CustomLOD;NumVertices=L.GetNumVertices();
  ExpectedComponentKey=C->StaticMeshDerivedDataKey;StaleKey=HasStoredColors&&ExpectedComponentKey!=DerivedKey;
  R->SetStringField(TEXT("component"),Path);R->SetStringField(TEXT("static_mesh"),MeshPath);R->SetNumberField(TEXT("lod_index"),LOD);R->SetNumberField(TEXT("vertex_count"),NumVertices);
  R->SetStringField(TEXT("color_encoding"),TEXT("RGBA uint8 bytes; no implicit linear/sRGB conversion"));R->SetStringField(TEXT("index_space"),TEXT("render_LOD_vertex_index"));
  R->SetStringField(TEXT("save_package"),P->GetName());R->SetBoolField(TEXT("package_dirty_before"),PackageDirty);R->SetBoolField(TEXT("source_mesh_dirty_before"),MeshDirty);
  R->SetNumberField(TEXT("override_vertex_count_before"),Before.IsValidIndex(LOD)?Before[LOD].Num():0);Ready=true;
  R->SetBoolField(TEXT("stale_mesh_color_key"),StaleKey);
 }
 void Error(const TCHAR* Code,const FString& Message){R->SetStringField(TEXT("error_code"),Code);R->SetStringField(TEXT("error"),Message);}
 bool Mismatch(const TCHAR* Boundary,const FString& Property,const FString& Expected,const FString& Actual)
 {
  auto M=MakeShared<J>();M->SetStringField(TEXT("object"),Path);M->SetStringField(TEXT("property"),Property);M->SetStringField(TEXT("expected"),Expected);M->SetStringField(TEXT("actual"),Actual);M->SetStringField(TEXT("boundary"),Boundary);R->SetObjectField(TEXT("first_mismatch"),M);
  Error(TEXT("state_changed"),TEXT("Context or preserved state changed at an engine callback boundary. Inspect actual target; no rollback/retry."));return false;
 }
 bool Current(const TCHAR* Boundary,bool Registered)
 {
  if(GEditor!=Editor||!World.IsValid()||!IsValid(C.Get())||!IsValid(Actor.Get())||!IsValid(Mesh.Get())||!IsValid(Package.Get())||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor||IsGarbageCollecting()||UE::IsSavingPackage()||IsEngineExitRequested())return Mismatch(Boundary,TEXT("editor_lifetime"),TEXT("same idle editor and live owners"),TEXT("changed or busy"));
  if(GEditor->GetEditorWorldContext().World()!=World.Get()||C->GetWorld()!=World.Get()||C->GetOwner()!=Actor.Get()||Actor->GetLevel()!=Level||C->GetOuter()!=Outer||C->GetPathName()!=Path||C->GetPackage()!=Package.Get()||C->IsTemplate()||Actor->IsActorBeingDestroyed()||FLevelUtils::IsLevelLocked(Level))return Mismatch(Boundary,TEXT("owner_world"),TEXT("same component, owner, world, level and package"),TEXT("changed"));
  if(Package->GetName()!=PackageName)return Mismatch(Boundary,TEXT("package_name"),PackageName,Package->GetName());
  TInlineComponentArray<UActorComponent*> Owned;Actor->GetComponents(Owned);if(!Owned.Contains(C.Get()))return Mismatch(Boundary,TEXT("component_membership"),TEXT("owned by the same actor"),TEXT("not owned"));
  if(C->IsRegistered()!=Registered)return Mismatch(Boundary,TEXT("registered"),Registered?TEXT("true"):TEXT("false"),C->IsRegistered()?TEXT("true"):TEXT("false"));
  if(C->GetStaticMesh()!=Mesh.Get()||Mesh->IsCompiling()||Mesh->GetRenderData()!=Render||Render->DerivedDataKey!=DerivedKey)return Mismatch(Boundary,TEXT("static_mesh_render_data"),MeshPath,TEXT("mesh/render data changed or compiling"));
  if(RequirePaintPolicy&&!C->CanMeshPaintVertexColors())return Mismatch(Boundary,TEXT("vertex_paint_policy"),TEXT("paintable non-Nanite-proxy component"),TEXT("disabled or Nanite proxy"));
  if(bool(C->bCustomOverrideVertexColorPerLOD)!=ExpectedCustomLOD)return Mismatch(Boundary,TEXT("custom_override_per_lod"),ExpectedCustomLOD?TEXT("true"):TEXT("false"),C->bCustomOverrideVertexColorPerLOD?TEXT("true"):TEXT("false"));
  if(C->StaticMeshDerivedDataKey!=ExpectedComponentKey)return Mismatch(Boundary,TEXT("component_mesh_color_key"),ExpectedComponentKey,C->StaticMeshDerivedDataKey);
  if(!C->GetRelativeTransform().Equals(Relative,0)||C->OverrideMaterials!=Materials)return Mismatch(Boundary,TEXT("authored_transform_materials"),TEXT("unchanged"),TEXT("changed"));
  if(Mesh->GetPackage()!=Package.Get()&&Mesh->GetPackage()->IsDirty()!=MeshDirty)return Mismatch(Boundary,TEXT("source_mesh_dirty"),MeshDirty?TEXT("true"):TEXT("false"),Mesh->GetPackage()->IsDirty()?TEXT("true"):TEXT("false"));
  for(int32 I=0;I<MeshColors.Num();++I){TArray<FColor> Colors;if(!CopyBuffer(&Render->LODResources[I].VertexBuffers.ColorVertexBuffer,Colors)||Colors!=MeshColors[I])return Mismatch(Boundary,FString::Printf(TEXT("source_mesh_LOD%d_colors"),I),TEXT("unchanged"),TEXT("changed or unavailable"));}
  return true;
 }
 bool ColorsEqual(const TCHAR* Boundary,const TArray<FColor>& Selected,bool After)
 {
  if(C->LODData.Num()<(After?FMath::Max(Before.Num(),LOD+1):Before.Num()))return Mismatch(Boundary,TEXT("lod_inventory"),TEXT("no removed LOD entries"),TEXT("shorter inventory"));
  for(int32 I=0;I<C->LODData.Num();++I)
  {
   TArray<FColor> Actual;if(!CopyBuffer(C->LODData[I].OverrideVertexColors,Actual))return Mismatch(Boundary,FString::Printf(TEXT("LOD%d_colors"),I),TEXT("CPU-readable colors"),TEXT("unavailable"));
   const TArray<FColor> Empty;const TArray<FColor>& Expected=(After&&I==LOD)?Selected:Before.IsValidIndex(I)?Before[I]:Empty;
   if(Actual!=Expected)return Mismatch(Boundary,FString::Printf(TEXT("LOD%d_colors"),I),Digest(Expected),Digest(Actual));
   {
    const TArray<FPaintedVertex> EmptyCache;
    const auto& Cache=C->LODData[I].PaintedVertices;
    const auto& OldCache=BeforeCache.IsValidIndex(I)?BeforeCache[I]:EmptyCache;
    const bool EngineRebuild=After&&(!ClearOperation||SaveCachePrepared)&&OldCache.Num()<=1;
    const auto& Generated=RebuiltCache.IsValidIndex(I)?RebuiltCache[I]:EmptyCache;
    const auto& ExpectedCache=(After&&I==LOD)?Generated:EngineRebuild?Generated:OldCache;
    if(Cache.Num()!=ExpectedCache.Num())return Mismatch(Boundary,FString::Printf(TEXT("LOD%d_painted_cache_count"),I),FString::FromInt(ExpectedCache.Num()),FString::FromInt(Cache.Num()));
    for(int32 V=0;V<Cache.Num();++V)
    {
     const auto& ActualVertex=Cache[V];const auto& WantedVertex=ExpectedCache[V];const FString Prefix=FString::Printf(TEXT("LOD%d_painted_cache[%d]."),I,V);
     if(ActualVertex.Color!=WantedVertex.Color)return Mismatch(Boundary,Prefix+TEXT("Color"),WantedVertex.Color.ToString(),ActualVertex.Color.ToString());
     if(ActualVertex.Position!=WantedVertex.Position)return Mismatch(Boundary,Prefix+TEXT("Position"),FString::Printf(TEXT("%.17g,%.17g,%.17g"),WantedVertex.Position.X,WantedVertex.Position.Y,WantedVertex.Position.Z),FString::Printf(TEXT("%.17g,%.17g,%.17g"),ActualVertex.Position.X,ActualVertex.Position.Y,ActualVertex.Position.Z));
     if(ActualVertex.Normal!=WantedVertex.Normal)return Mismatch(Boundary,Prefix+TEXT("Normal"),FString::Printf(TEXT("%.17g,%.17g,%.17g,%.17g"),WantedVertex.Normal.X,WantedVertex.Normal.Y,WantedVertex.Normal.Z,WantedVertex.Normal.W),FString::Printf(TEXT("%.17g,%.17g,%.17g,%.17g"),ActualVertex.Normal.X,ActualVertex.Normal.Y,ActualVertex.Normal.Z,ActualVertex.Normal.W));
    }
   }
   if(After&&I==LOD)
   {
    const auto& Painted=C->LODData[I].PaintedVertices;
    if(Painted.Num()!=Selected.Num())return Mismatch(Boundary,TEXT("painted_cache_count"),FString::FromInt(Selected.Num()),FString::FromInt(Painted.Num()));
    for(int32 V=0;V<Selected.Num();++V)if(Painted[V].Color!=Selected[V])return Mismatch(Boundary,FString::Printf(TEXT("painted_cache[%d].color"),V),Selected[V].ToString(),Painted[V].Color.ToString());
   }
   if(BuildIds.IsValidIndex(I)&&BuildIds[I]!=C->LODData[I].MapBuildDataId)return Mismatch(Boundary,FString::Printf(TEXT("LOD%d_map_build_data_id"),I),BuildIds[I].ToString(),C->LODData[I].MapBuildDataId.ToString());
   if(OriginalBuildIds.IsValidIndex(I)&&OriginalBuildIds[I]!=C->LODData[I].OriginalMapBuildDataId)return Mismatch(Boundary,FString::Printf(TEXT("LOD%d_original_map_build_data_id"),I),OriginalBuildIds[I].ToString(),C->LODData[I].OriginalMapBuildDataId.ToString());
  }
  return true;
 }
 bool SelectedCacheMatches(const TArray<FColor>& Colors) const
 {
  if(!BeforeCache.IsValidIndex(LOD)||BeforeCache[LOD].Num()!=Colors.Num())return false;
  const auto& Buffers=Render->LODResources[LOD].VertexBuffers;
  for(int32 V=0;V<Colors.Num();++V)
  {
   const auto& Point=BeforeCache[LOD][V];
   if(Point.Color!=Colors[V]||Point.Position!=FVector(Buffers.PositionVertexBuffer.VertexPosition(V))||Point.Normal!=FVector4(Buffers.StaticMeshVertexBuffer.VertexTangentZ(V)))return false;
  }
  return true;
 }
 bool PrepareCaches(const TArray<FColor>& Wanted,bool Clear,bool Save)
 {
  TArray<int32> Overrides,Caches;for(const auto& Values:Before)Overrides.Add(Values.Num());for(const auto& Values:BeforeCache)Caches.Add(Values.Num());
  int64 ProjectedOverrides=0,ProjectedCaches=0;
  const bool Allowed=ProjectCounts(Overrides,Caches,LOD,Wanted.Num(),Clear,Save,ProjectedOverrides,ProjectedCaches);
  R->SetNumberField(TEXT("projected_override_vertices"),double(ProjectedOverrides));R->SetNumberField(TEXT("projected_painted_vertices"),double(ProjectedCaches));
  if(!Allowed){Error(TEXT("projected_color_snapshot_bound"),TEXT("Resulting override/cache totals exceed1048576; includes other LOD cache regeneration. No component Modify or writer entered."));return false;}
  RebuiltCache.SetNum(FMath::Max(Before.Num(),LOD+1));
  for(int32 I=0;I<RebuiltCache.Num();++I)
  {
   const bool Selected=I==LOD;
   if(Selected&&Clear)continue;
   if(!Selected&&(!Before.IsValidIndex(I)||(Clear&&!Save)||BeforeCache[I].Num()>1||Before[I].IsEmpty()))continue;
   const auto& Colors=Selected?Wanted:Before[I];const auto& Buffers=Render->LODResources[I].VertexBuffers;
   auto& Cache=RebuiltCache[I];Cache.Reserve(Colors.Num());
   for(int32 V=0;V<Colors.Num();++V){FPaintedVertex Point;Point.Position=FVector(Buffers.PositionVertexBuffer.VertexPosition(V));Point.Normal=FVector4(Buffers.StaticMeshVertexBuffer.VertexTangentZ(V));Point.Color=Colors[V];Cache.Add(Point);}
  }
  return true;
 }
 FString Fail(){return Encode(R);}
};
bool Decode(const FString& Text,TArray<FColor>& Out,FString& Error)
{
 if(Text.Len()>32*1024*1024){Error=TEXT("RGBA JSON exceeds32MiB");return false;}
 TArray<TSharedPtr<FJsonValue>> Rows;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Rows)||Rows.Num()>MaxVertices){Error=TEXT("Expected bounded JSON array of RGBA byte arrays");return false;}
 Out.Reserve(Rows.Num());
 for(int32 I=0;I<Rows.Num();++I)
 {
  const TArray<TSharedPtr<FJsonValue>>* Channels=nullptr;if(!Rows[I]->TryGetArray(Channels)||Channels->Num()!=4){Error=FString::Printf(TEXT("Color%d must contain exactly4 RGBA bytes"),I);return false;}
  uint8 V[4];for(int32 K=0;K<4;++K){double N=0;if((*Channels)[K]->Type!=EJson::Number||!(*Channels)[K]->TryGetNumber(N)||!FMath::IsFinite(N)||N<0||N>255||FMath::TruncToDouble(N)!=N){Error=FString::Printf(TEXT("Color%d channel%d must be an integer0..255"),I,K);return false;}V[K]=static_cast<uint8>(N);}
  Out.Emplace(V[0],V[1],V[2],V[3]);
 }
 return true;
}
}

bool ProjectCounts(const TArray<int32>& Overrides,const TArray<int32>& Caches,int32 Selected,int32 Wanted,bool Clear,bool Save,int64& TotalOverrides,int64& TotalCaches)
{
 TotalOverrides=0;TotalCaches=0;
 if(Selected<0||Selected>=64||Wanted<0||Overrides.Num()!=Caches.Num())return false;
 for(int32 I=0;I<FMath::Max(Overrides.Num(),Selected+1);++I)
 {
  const int32 Existing=Overrides.IsValidIndex(I)?Overrides[I]:0;const int32 Cached=Caches.IsValidIndex(I)?Caches[I]:0;
  if(Existing<0||Cached<0)return false;
  const int32 Result=I==Selected?(Clear?0:Wanted):Existing;
  const int32 ResultCache=I==Selected?Result:((!Clear||Save)&&Cached<=1?Result:Cached);
  TotalOverrides+=Result;TotalCaches+=ResultCache;
 }
 return TotalOverrides<=MaxVertices&&TotalCaches<=MaxVertices;
}

FString Inspect(UObject* Object,int32 LOD,int32 Start,int32 Count,bool Positions)
{
 FContext C(TEXT("inspect_instance_vertex_colors"),Object,LOD);if(!C.Ready)return C.Fail();
 if(Start<0||Start>C.NumVertices||Count<0||Count>4096){C.Error(TEXT("invalid_page"),TEXT("start_index must be0..vertex_count; count0..4096. count0 reads metadata only."));return C.Fail();}
 TArray<FColor> Effective;const bool Override=C.Before.IsValidIndex(LOD)&&C.Before[LOD].Num()>0;
 if(Override)Effective=C.Before[LOD];else if(C.MeshColors[LOD].Num()==C.NumVertices)Effective=C.MeshColors[LOD];else if(C.MeshColors[LOD].Num()==0)Effective.Init(FColor::White,C.NumVertices);else{C.Error(TEXT("source_color_count_mismatch"),TEXT("Source render color count differs from render vertices"));return C.Fail();}
 TArray<TSharedPtr<FJsonValue>> Rows;const int32 End=FMath::Min(C.NumVertices,Start+Count);
 for(int32 I=Start;I<End;++I)
 {
  auto Row=MakeShared<J>();Row->SetNumberField(TEXT("vertex_index"),I);const auto Color=Effective[I];
  TArray<TSharedPtr<FJsonValue>> RGBA;for(uint8 Byte:{Color.R,Color.G,Color.B,Color.A})RGBA.Add(MakeShared<FJsonValueNumber>(Byte));Row->SetArrayField(TEXT("rgba"),RGBA);
  if(Positions){const auto P=C.Render->LODResources[LOD].VertexBuffers.PositionVertexBuffer.VertexPosition(I);TArray<TSharedPtr<FJsonValue>> XYZ;for(float V:{P.X,P.Y,P.Z})XYZ.Add(MakeShared<FJsonValueNumber>(V));Row->SetArrayField(TEXT("mesh_local_position"),XYZ);}
  Rows.Add(MakeShared<FJsonValueObject>(Row));
 }
 C.R->SetBoolField(TEXT("has_override"),Override);C.R->SetStringField(TEXT("effective_color_source"),Override?TEXT("component_override"):C.MeshColors[LOD].Num()?TEXT("mesh_asset"):TEXT("default_white"));
 C.R->SetNumberField(TEXT("start_index"),Start);C.R->SetNumberField(TEXT("returned_count"),Rows.Num());C.R->SetBoolField(TEXT("truncated"),End<C.NumVertices);C.R->SetArrayField(TEXT("vertices"),Rows);C.R->SetStringField(TEXT("colors_crc32"),Digest(Effective));
 C.R->SetBoolField(TEXT("can_mesh_paint_vertex_colors"),C.C->CanMeshPaintVertexColors());C.R->SetBoolField(TEXT("success"),true);C.R->SetStringField(TEXT("stage"),TEXT("inspected"));return Encode(C.R);
}

FString Apply(UObject* Object,int32 LOD,const FString& ColorsJson,bool Clear,bool Execute,bool AllowReplace,bool Save,bool AllowSaveDirtyPackage)
{
 FContext C(Clear?TEXT("clear_instance_vertex_colors"):TEXT("set_instance_vertex_colors"),Object,LOD);if(!C.Ready)return C.Fail();
 C.ClearOperation=Clear;C.RequirePaintPolicy=!Clear;
 TArray<FColor> Wanted;FString ParseError;
 if(!Clear&&(!Decode(ColorsJson,Wanted,ParseError)||Wanted.Num()!=C.NumVertices)){C.Error(TEXT("invalid_colors"),ParseError.IsEmpty()?FString::Printf(TEXT("Expected exactly%d render vertex RGBA entries"),C.NumVertices):ParseError);return C.Fail();}
 if(!Clear&&!C.C->CanMeshPaintVertexColors()){C.Error(TEXT("vertex_painting_disabled"),TEXT("Component vertex painting is disabled or uses a Nanite proxy. This tool does not change Nanite/painting policy; use supported fallback configuration or texture color painting."));return C.Fail();}
 auto* Subsystem=GEngine?GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>():nullptr;
 if(!IsValid(Subsystem)){C.Error(TEXT("mesh_painting_required"),TEXT("Enabled and loaded MeshPainting plugin/Subsystem required."));return C.Fail();}
 TStrongObjectPtr<UMeshPaintingSubsystem> HoldSubsystem(Subsystem);
 const bool HasOverride=C.Before.IsValidIndex(LOD)&&C.Before[LOD].Num()>0;
 const bool HasCache=C.BeforeCache.IsValidIndex(LOD)&&C.BeforeCache[LOD].Num()>0;
 const bool Has=HasOverride||HasCache;
 if(C.StaleKey){C.Error(TEXT("stale_mesh_color_key"),TEXT("Existing override/cache was authored for a different mesh derived-data key. Reconcile through an explicit engine color fixup first; this operation will not hide a pending remap."));return C.Fail();}
 const bool Same=Clear?!Has:HasOverride&&C.Before[LOD]==Wanted&&C.SelectedCacheMatches(Wanted)&&(LOD==0||C.CustomLOD);
 C.R->SetBoolField(TEXT("already_matches"),Same);C.R->SetBoolField(TEXT("save_requested"),Save);
 C.R->SetStringField(TEXT("save_scope"),TEXT("Explicit owning package only, including other objects in that same package; an already dirty package requires allow_save_dirty_package=true. No implicit other-level/source-mesh save."));
 if(Same){C.R->SetBoolField(TEXT("success"),true);C.R->SetStringField(TEXT("stage"),TEXT("already_matches_no_save"));return Encode(C.R);}
 if(Has&&!AllowReplace){C.Error(TEXT("existing_override_requires_permission"),Clear?TEXT("Existing instance colors require allow_remove=true"):TEXT("Existing instance colors require allow_replace=true"));return C.Fail();}
 if(Save&&C.PackageDirty&&!AllowSaveDirtyPackage){C.Error(TEXT("dirty_package_save_requires_permission"),TEXT("Owning package already contains unsaved edits. Use save=false or explicitly allow_save_dirty_package=true; no mutation performed."));return C.Fail();}
 if(!C.PrepareCaches(Wanted,Clear,Save))return C.Fail();
 if(Save)
 {
  if(!FPackageName::DoesPackageExist(C.Package->GetName(),&C.SaveFilename)&&!FPackageName::TryConvertLongPackageNameToFilename(C.Package->GetName(),C.SaveFilename,UWorld::FindWorldInPackage(C.Package.Get())?FPackageName::GetMapPackageExtension():FPackageName::GetAssetPackageExtension())){C.Error(TEXT("save_package_filename_invalid"),TEXT("Owning package filename cannot be resolved; no mutation performed."));return C.Fail();}
  C.SaveFilename=FPaths::ConvertRelativePathToFull(C.SaveFilename);C.R->SetStringField(TEXT("save_filename"),C.SaveFilename);C.R->SetStringField(TEXT("save_api"),TEXT("UPackage::SavePackage; no recursive external/BuiltData save"));
 }
 if(!Execute){C.R->SetBoolField(TEXT("success"),true);C.R->SetStringField(TEXT("stage"),TEXT("preflight_passed"));return Encode(C.R);}
 TGuardValue<bool> Guard(Writing,true);
 C.R->SetBoolField(TEXT("mutation_attempted"),true);C.R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());C.R->SetBoolField(TEXT("change_state_known"),false);C.R->SetStringField(TEXT("retry_risk"),TEXT("inspect_actual_target"));
 C.R->SetStringField(TEXT("stage"),TEXT("modify_component"));C.C->SetFlags(RF_Transactional);C.C->Modify();
 if(!C.Current(TEXT("after_modify"),true)||!C.ColorsEqual(TEXT("after_modify"),Wanted,false))return C.Fail();
 bool BoundaryOK=true;
 {
  C.R->SetStringField(TEXT("stage"),TEXT("unregister_render_component"));FComponentReregisterContext Reregister(C.C.Get());
  BoundaryOK=C.Current(TEXT("after_unregister"),false)&&C.ColorsEqual(TEXT("after_unregister"),Wanted,false);
  if(BoundaryOK&&(!IsValid(HoldSubsystem.Get())||!GEngine||GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()!=HoldSubsystem.Get()))BoundaryOK=C.Mismatch(TEXT("after_unregister"),TEXT("mesh_painting_subsystem"),TEXT("same live Engine subsystem"),TEXT("changed"));
  if(BoundaryOK)
  {
   C.R->SetStringField(TEXT("stage"),TEXT("write_instance_colors"));C.R->SetBoolField(TEXT("writer_entered"),true);
   C.ExpectedCustomLOD=C.CustomLOD||(!Clear&&LOD>0);
   C.C->SetLODDataCount(LOD+1,FMath::Max(LOD+1,C.C->LODData.Num()));
   if(Clear)C.C->RemoveInstanceVertexColorsFromLOD(LOD);else Subsystem->SetInstanceColorDataForLOD(C.C.Get(),LOD,Wanted);
   C.ExpectedComponentKey=C.DerivedKey;C.C->StaticMeshDerivedDataKey=C.DerivedKey;
  }
 }
 C.R->SetBoolField(TEXT("render_update_requested"),true);
 if(!BoundaryOK)return C.Fail();
 if(!C.Current(TEXT("after_reregister"),true)||!C.ColorsEqual(TEXT("after_reregister"),Wanted,true))return C.Fail();
 C.R->SetStringField(TEXT("last_confirmed_stage"),TEXT("colors_and_cache_verified_after_reregister"));C.R->SetNumberField(TEXT("observed_override_count_after_writer"),Clear?0:Wanted.Num());
 const bool ExpectedCustom=C.CustomLOD||(!Clear&&LOD>0);
 if(bool(C.C->bCustomOverrideVertexColorPerLOD)!=ExpectedCustom){C.Mismatch(TEXT("after_reregister"),TEXT("custom_override_per_lod"),ExpectedCustom?TEXT("true"):TEXT("false"),C.C->bCustomOverrideVertexColorPerLOD?TEXT("true"):TEXT("false"));return C.Fail();}
 C.R->SetStringField(TEXT("stage"),TEXT("mark_owner_package_dirty"));C.C->MarkPackageDirty();
 if(!C.Current(TEXT("after_dirty"),true)||!C.ColorsEqual(TEXT("after_dirty"),Wanted,true))return C.Fail();
 C.R->SetBoolField(TEXT("package_dirty_after"),C.Package->IsDirty());
 if(!C.Package->IsDirty()){C.Error(TEXT("dirty_flag_not_set"),TEXT("Colors changed but owning package dirty flag was not retained; inspect actual state before save."));return C.Fail();}
 if(Save)
 {
  // StaticMeshComponent::PreSave creates missing lighting IDs even without a light build.
  // Run that exact public native preparation now, retaining every existing valid ID;
  // snapshot its result so callbacks during Save cannot hide behind a generic exception.
  C.R->SetStringField(TEXT("stage"),TEXT("prepare_save_lighting_ids"));
  // PreSave also repairs empty/single painted caches on every overridden LOD,
  // including during clear(save=true). Do not permit that transition before this boundary.
  C.C->CachePaintedDataIfNecessary();C.SaveCachePrepared=true;
  if(!C.Current(TEXT("after_save_cache_preparation"),true)||!C.ColorsEqual(TEXT("after_save_cache_preparation"),Wanted,true))return C.Fail();
  for(int32 I=0;I<C.C->LODData.Num();++I)C.C->LODData[I].CreateMapBuildDataId(I);
  for(int32 I=0;I<C.C->LODData.Num();++I)
  {
   const auto& D=C.C->LODData[I];
   if(C.BuildIds.IsValidIndex(I)&&C.BuildIds[I].IsValid()&&C.BuildIds[I]!=D.MapBuildDataId){C.Mismatch(TEXT("prepare_save"),FString::Printf(TEXT("LOD%d_existing_map_build_data_id"),I),C.BuildIds[I].ToString(),D.MapBuildDataId.ToString());return C.Fail();}
   if(C.OriginalBuildIds.IsValidIndex(I)&&C.OriginalBuildIds[I].IsValid()&&C.OriginalBuildIds[I]!=D.OriginalMapBuildDataId){C.Mismatch(TEXT("prepare_save"),FString::Printf(TEXT("LOD%d_existing_original_map_build_data_id"),I),C.OriginalBuildIds[I].ToString(),D.OriginalMapBuildDataId.ToString());return C.Fail();}
  }
  C.BuildIds.Reset();C.OriginalBuildIds.Reset();for(const auto& D:C.C->LODData){C.BuildIds.Add(D.MapBuildDataId);C.OriginalBuildIds.Add(D.OriginalMapBuildDataId);}
  C.R->SetBoolField(TEXT("save_lighting_ids_prepared"),true);
  if(!C.Current(TEXT("after_save_preparation"),true)||!C.ColorsEqual(TEXT("after_save_preparation"),Wanted,true))return C.Fail();
  C.R->SetStringField(TEXT("stage"),TEXT("save_owner_package"));C.R->SetBoolField(TEXT("save_attempted"),true);
  UObject* SaveRoot=UWorld::FindWorldInPackage(C.Package.Get());if(!SaveRoot)SaveRoot=C.Actor->GetPackage()==C.Package.Get()?static_cast<UObject*>(C.Actor.Get()):C.C.Get();
  TStrongObjectPtr<UObject> KeepSaveRoot(SaveRoot);
  FSavePackageArgs SaveArgs;SaveArgs.TopLevelFlags=RF_Public|RF_Standalone;SaveArgs.SaveFlags=SAVE_NoError;SaveArgs.bSlowTask=false;
  const bool Saved=UPackage::SavePackage(C.Package.Get(),SaveRoot,*C.SaveFilename,SaveArgs);C.R->SetBoolField(TEXT("saved"),Saved);
  C.R->SetBoolField(TEXT("save_return"),Saved);C.R->SetBoolField(TEXT("disk_contents_verified"),false);C.R->SetBoolField(TEXT("failed_save_may_have_disk_effects"),!Saved);
  if(IsValid(C.Package.Get()))C.R->SetBoolField(TEXT("package_dirty_after"),C.Package->IsDirty());else C.R->SetField(TEXT("package_dirty_after"),MakeShared<FJsonValueNull>());
  if(!C.Current(TEXT("after_save"),true)||!C.ColorsEqual(TEXT("after_save"),Wanted,true))return C.Fail();
  if(!Saved){C.R->SetBoolField(TEXT("changed"),true);C.R->SetBoolField(TEXT("change_state_known"),true);C.Error(TEXT("save_failed"),TEXT("Instance colors are changed and verified, but the explicit package save failed. Do not replay the color writer."));return C.Fail();}
  if(C.Package->IsDirty()){C.Error(TEXT("dirty_after_save"),TEXT("Save returned true but the owning package is dirty afterward; preserve saved=true and inspect callback changes."));return C.Fail();}
 }
 C.R->SetNumberField(TEXT("override_vertex_count_after"),Clear?0:Wanted.Num());C.R->SetNumberField(TEXT("painted_vertex_count_after"),C.C->LODData.IsValidIndex(LOD)?C.C->LODData[LOD].PaintedVertices.Num():0);
 C.R->SetBoolField(TEXT("source_mesh_colors_preserved"),true);C.R->SetBoolField(TEXT("other_lod_colors_preserved"),true);C.R->SetBoolField(TEXT("changed"),true);C.R->SetBoolField(TEXT("change_state_known"),true);
 C.R->SetBoolField(TEXT("success"),true);C.R->SetStringField(TEXT("stage"),TEXT("instance_colors_verified"));return Encode(C.R);
}
}
