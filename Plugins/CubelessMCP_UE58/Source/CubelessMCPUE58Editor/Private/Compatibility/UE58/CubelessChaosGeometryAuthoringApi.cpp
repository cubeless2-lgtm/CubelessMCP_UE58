#include "Compatibility/UE58/CubelessChaosGeometryAuthoringApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Algo/Count.h"
#include "Algo/Reverse.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/Particle/ObjectState.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Dataflow/DataflowSelection.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Field/FieldSystemActor.h"
#include "Field/FieldSystemAsset.h"
#include "Field/FieldSystemComponent.h"
#include "Field/FieldSystemObjects.h"
#include "FractureEngineFracturing.h"
#include "FractureEngineUtility.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionRootProxyRenderer.h"
#include "GeometryCollection/Facades/CollectionRemoveOnBreakFacade.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <cmath>

namespace Cubeless::UE58::ChaosGeometryAuthoring
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false;
bool Live(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
FString Json(const TSharedPtr<FJsonObject>& O) { FString S;if(O) { FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S)); }return S; }
bool Dependency(const FString& S)
{
    for(const TCHAR* K:{TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("cubelessmcp"),TEXT("cubeless_mcp"),TEXT("modelcontextprotocol"),TEXT("toolsetregistry"),TEXT("/Engine/PythonTypes")}) { if(S.Contains(K,ESearchCase::IgnoreCase)) { return true; } }return false;
}
struct FExactKeys:DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& S) { return FCrc::StrCrc32(*S); }
};
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||S.Len()>1048576) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }
    auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeys>> Keys;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token)) { if(Token==EJsonNotation::Error||++Slots>262144) { return false; }if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>64) { return false; } }else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber())) { return false; } }
    return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)&&Out.IsValid();
}
bool Schema(const FGeometryCollection& C);
FString HashCollectionManagedGeometry(const UGeometryCollection* Asset,bool bIncludeCollisionMappings);
struct FTarget
{
    TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Weak,Outer,Parent;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package;FString Path,PackagePath,Role;TSharedPtr<FJsonObject> Before;bool HasParent=false,HasCollectionIdentity=false;TSharedPtr<FGeometryCollection,ESPMode::ThreadSafe> CollectionIdentity;TStrongObjectPtr<const UGeometryCollection> ComponentRest{nullptr};bool HasComponentRest=false;
};
struct FWork
{
    FResult R;FString Operation,FailedStage;TWeakObjectPtr<UWorld> World;bool HasWorld=false,Observer=false;TArray<FTarget> Targets;
    TArray<TSharedPtr<FJsonValue>> Events,Saves,Retired;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();
    explicit FWork(const TCHAR* Name):Operation(Name) { R.Stage=TEXT("validate_request");R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("render_or_physics_completion_verified"),false);R.Data->SetBoolField(TEXT("explicit_wrapper_wait_performed"),false);R.Data->SetStringField(TEXT("save_flag_scope"),TEXT("Any explicit EditorAssetLibrary SaveAsset true history, including a later failed gate/reload/cleanup. SDK implicit saves and global disk effects remain unknown; legacy saved/level_saved retain original semantics."));R.Data->SetStringField(TEXT("SDK_synchronous_scope"),TEXT("Fracture/collision/render build, package reload, construction scripts and save are synchronous public SDK calls; no worker UObject execution, added wait or forced timeout.")); }
    void Count(const TCHAR* S) { double N=0;Counts->TryGetNumberField(S,N);Counts->SetNumberField(S,N+1); }
    void SDK(const TCHAR* S) { R.Stage=S;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_effects_unknown"),true); }
    void Write(const TCHAR* S) { SDK(S);R.bMutationAttempted=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),S);Events.Add(MakeShared<FJsonValueObject>(J)); }
    bool Fail(const TCHAR* Code,const FString& E) { if(R.Error.IsEmpty()) { R.ErrorCode=TEXT("chaos_geometry_authoring_")+FString(Code);R.Error=E;FailedStage=R.Stage; }return false; }
    bool Guard()
    {
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()) { return Fail(TEXT("editor_lifetime"),TEXT("Safe Editor game-thread lifetime is unavailable")); }
        if(HasWorld&&(!Live(World.Get())||World->bIsTearingDown||(GEditor->GetPIEWorldContext()?GEditor->GetPIEWorldContext()->World():GEditor->GetEditorWorldContext().World())!=World.Get())) { return Fail(TEXT("world_lost"),TEXT("Captured world is no longer live")); }
        for(const auto& T:Targets) { const auto* O=T.Weak.Get();if(!Live(O)||!Live(T.Package.Get())||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)||!T.Package->GetName().Equals(T.PackagePath,ESearchCase::CaseSensitive)||(T.HasParent&&(!Live(T.Parent.Get())||!O->IsIn(T.Parent.Get())))) { return Fail(TEXT("target_identity_lost"),TEXT("Captured class/outer/package/path/parent identity changed")); }if(T.HasCollectionIdentity) { const auto* C=Cast<UGeometryCollection>(O);if(!C||C->GetGeometryCollection().Get()!=T.CollectionIdentity.Get()) { return Fail(TEXT("managed_identity_changed"),TEXT("Captured managed collection storage was replaced during SDK execution")); } }if(T.HasComponentRest) { const auto* C=Cast<UGeometryCollectionComponent>(O);if(!C||!Live(T.ComponentRest.Get())||C->GetRestCollection()!=T.ComponentRest.Get()) { return Fail(TEXT("rest_asset_changed"),TEXT("Captured component rest asset changed")); } }if((O->IsA<AActor>()||O->IsA<UActorComponent>())&&HasWorld&&O->GetWorld()!=World.Get()) { return Fail(TEXT("target_world_lost"),TEXT("Captured actor/component changed world")); } }
        return true;
    }
    TSharedPtr<FJsonObject> Snapshot(UObject* O,bool Detailed=true)
    {
        auto J=MakeShared<FJsonObject>();if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()) { J->SetBoolField(TEXT("unsafe_observation_skipped"),true);return J; }J->SetBoolField(TEXT("live"),Live(O));if(!Live(O)) { return J; }SDK(TEXT("observe_captured_object"));J->SetStringField(TEXT("path"),O->GetPathName());J->SetStringField(TEXT("package"),O->GetPackage()->GetName());J->SetBoolField(TEXT("dirty"),O->GetPackage()->IsDirty());if(!Detailed) { return J; }
        if(auto* A=Cast<AActor>(O)) { J->SetStringField(TEXT("transform"),A->GetActorTransform().ToString()); }
        if(auto* C=Cast<UGeometryCollection>(O)) { const auto Collection=C->GetGeometryCollection();if(Collection.IsValid()&&Schema(*Collection)) { J->SetStringField(TEXT("managed_geometry"),HashCollectionManagedGeometry(C,true)); }J->SetBoolField(TEXT("nanite"),C->EnableNanite);J->SetNumberField(TEXT("proxy_count"),C->RootProxyData.ProxyMeshes.Num());J->SetNumberField(TEXT("damage_count"),C->DamageThreshold.Num());TArray<TSharedPtr<FJsonValue>> Damage,Proxy,Fit;for(float V:C->DamageThreshold) { Damage.Add(MakeShared<FJsonValueNumber>(V)); }for(const auto& M:C->RootProxyData.ProxyMeshes) { Proxy.Add(MakeShared<FJsonValueString>(Live(M.Get())?M->GetPathName():FString())); }for(const auto& T:C->RootProxyData.MeshTransforms) { Fit.Add(MakeShared<FJsonValueString>(T.ToString())); }J->SetArrayField(TEXT("damage_thresholds"),Damage);J->SetArrayField(TEXT("proxy_paths"),Proxy);J->SetArrayField(TEXT("proxy_fit_transforms"),Fit);J->SetStringField(TEXT("renderer"),C->CustomRendererType?C->CustomRendererType->GetPathName():FString()); }
        if(auto* C=Cast<UGeometryCollectionComponent>(O)) { J->SetStringField(TEXT("rest_collection"),C->GetRestCollection()?C->GetRestCollection()->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> Fields;for(const auto& F:C->InitializationFields) { Fields.Add(MakeShared<FJsonValueString>(Live(F.Get())?F->GetPathName():FString())); }J->SetArrayField(TEXT("initialization_fields"),Fields); }
        return J;
    }
    bool Track(UObject* O,const TCHAR* Role,UObject* Parent=nullptr,bool Created=false)
    {
        if(!Live(O)||!Live(O->GetPackage())||Dependency(O->GetClass()->GetPathName())||(Parent&&!O->IsIn(Parent))) { return Fail(TEXT("target_unavailable"),TEXT("Live runtime-safe object under expected parent required")); }
        for(const auto& T:Targets) { if(T.Weak.Get()==O) { return Guard(); } }if(Targets.Num()>=16384) { return Fail(TEXT("object_budget"),TEXT("Captured object budget exceeded")); }
        auto& T=Targets.AddDefaulted_GetRef();T.Hold.Reset(O);T.Weak=O;T.Outer=O->GetOuter();T.Parent=Parent;T.HasParent=Parent!=nullptr;T.Class=O->GetClass();T.Package=O->GetPackage();T.Path=O->GetPathName();T.PackagePath=O->GetPackage()->GetName();T.Role=Role;if(auto* C=Cast<UGeometryCollection>(O)) { if(FCString::Strcmp(Role,TEXT("transient_same_seed_oracle"))!=0) { T.CollectionIdentity=C->GetGeometryCollection();T.HasCollectionIdentity=true; } }if(auto* C=Cast<UGeometryCollectionComponent>(O)) { T.ComponentRest.Reset(C->GetRestCollection());T.HasComponentRest=C->GetRestCollection()!=nullptr; }T.Before=Snapshot(O);R.bChanged|=Created;return Guard();
    }
    bool Modify(UObject* O,const TCHAR* Stage) { if(!Guard()||!R.Error.IsEmpty()||!Track(O,Stage)) { return false; }Write(Stage);Count(TEXT("modify_calls"));O->Modify();return Guard(); }
    template<typename T>T* Load(const FString& Path,const TCHAR* Stage) { if(!Guard()||!R.Error.IsEmpty()) { return nullptr; }if(Dependency(Path)) { Fail(TEXT("dependency_rejected"),TEXT("MCP-only dependency is forbidden"));return nullptr; }SDK(Stage);Count(TEXT("load_calls"));T* O=LoadObject<T>(nullptr,*Path);if(O&&!Track(O,Stage)) { return nullptr; }return Guard()?O:nullptr; }
    void Retire(UObject* Parent,const TCHAR* Reason)
    {
        for(int32 I=Targets.Num()-1;I>=0;--I) { auto* O=Targets[I].Weak.Get();if(O&&(O==Parent||O->IsIn(Parent))) { auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("reason"),Reason);J->SetStringField(TEXT("captured_path"),Targets[I].Path);J->SetObjectField(TEXT("before"),Targets[I].Before);J->SetObjectField(TEXT("at_retirement"),Snapshot(O));Retired.Add(MakeShared<FJsonValueObject>(J));Targets.RemoveAt(I); } }
    }
    FString Finish(TSharedPtr<FJsonObject> Legacy)
    {
        if(!Legacy) { Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error); }bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);const TSharedPtr<FJsonObject>* D=nullptr;FString Assertion;if(Legacy->TryGetObjectField(TEXT("data"),D)&&D&&D->IsValid()) { (*D)->TryGetStringField(TEXT("assertion_status"),Assertion); }
        TArray<TSharedPtr<FJsonValue>> Rows;for(auto& T:Targets) { auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("role"),T.Role);J->SetObjectField(TEXT("before"),T.Before);const bool Safe=Guard();auto A=Snapshot(T.Weak.Get(),Safe);J->SetObjectField(TEXT("after"),A);J->SetBoolField(TEXT("full_projection_available"),Safe&&Guard());if(Safe&&Guard()) { R.bChanged|=!Json(T.Before).Equals(Json(A),ESearchCase::CaseSensitive); }Rows.Add(MakeShared<FJsonValueObject>(J)); }
        if(!Success&&R.Error.IsEmpty()) { FString E;Legacy->TryGetStringField(TEXT("error"),E);Fail(TEXT("operation_failed"),E); }if(Assertion.Equals(TEXT("FAIL"),ESearchCase::CaseSensitive)&&R.Error.IsEmpty()) { Fail(TEXT("assertion_failed"),TEXT("Legacy authoring assertion_status is FAIL")); }R.bSuccess=Success&&R.Error.IsEmpty();R.bPostconditionPassed=R.bSuccess;R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetArrayField(TEXT("actual_mutation_prefix"),Events);R.Data->SetArrayField(TEXT("actual_save_prefix"),Saves);R.Data->SetArrayField(TEXT("captured_object_observations"),Rows);R.Data->SetArrayField(TEXT("retired_object_observations"),Retired);R.Data->SetBoolField(TEXT("operation_complete"),true);R.Data->SetBoolField(TEXT("observer_requested"),Observer);R.Data->SetBoolField(TEXT("global_change_state_unknown"),!R.bChangeStateKnown);if(R.bSuccess) { R.Stage=TEXT("complete"); }else if(!FailedStage.IsEmpty()) { R.Stage=FailedStage; }return Cubeless::Operations::EncodeVariableResult(R,*Operation);
    }
};
FWork* Work=nullptr;
bool Guard() { return Work&&Work->Guard(); }
template<typename F>bool Step(const TCHAR* Stage,F&& Fn,bool Mutating=true) { if(!Guard()||!Work->R.Error.IsEmpty()) { return false; }if(Mutating) { Work->Write(Stage); }else { Work->SDK(Stage); }Work->Count(Stage);Fn();return Guard()&&Work->R.Error.IsEmpty(); }
int32 CastI32(double V,const TCHAR* K) { if(!FMath::IsFinite(V)||V<=static_cast<double>(MIN_int32)-1||V>=static_cast<double>(MAX_int32)+1) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely convert to int32"),K));return 0; }return static_cast<int32>(V); }
float CastFloat(double V,const TCHAR* K) { if(!FMath::IsFinite(V)||FMath::Abs(V)>TNumericLimits<float>::Max()) { Work->Fail(TEXT("numeric_range"),FString::Printf(TEXT("%s cannot safely convert to float32"),K));return 0; }return static_cast<float>(V); }
int64 Quantize(double V,double Scale) { const double Q=V*Scale;if(!FMath::IsFinite(Q)||Q<=static_cast<double>(MIN_int64)+4096||Q>=static_cast<double>(MAX_int64)-4096) { Work->Fail(TEXT("quantization_range"),TEXT("Managed geometry hash quantization exceeds int64"));return 0; }return FMath::RoundToInt64(Q); }
bool Schema(const FGeometryCollection& C)
{
    const int32 T=C.NumElements(FGeometryCollection::TransformGroup),V=C.NumElements(FGeometryCollection::VerticesGroup),F=C.NumElements(FGeometryCollection::FacesGroup),G=C.NumElements(FGeometryCollection::GeometryGroup);
    // Keep the existing admission bounds; distinguish measured size from malformed SDK output.
    const int64 EstimatedLayoutBytes=static_cast<int64>(T)*256+static_cast<int64>(V)*192+static_cast<int64>(F)*96+static_cast<int64>(G)*192;
    constexpr int64 LayoutBudgetBytes=64LL*1024*1024;
    const bool bBudgetExceeded=EstimatedLayoutBytes>LayoutBudgetBytes;
    const bool bCountExceeded=T<0||V<0||F<0||G<0||T>2000000||V>8000000||F>8000000||G>2000000;
    const bool bAttributeMismatch=C.Transform.Num()!=T||C.Parent.Num()!=T||C.Children.Num()!=T||C.SimulationType.Num()!=T||C.TransformToGeometryIndex.Num()!=T||C.StatusFlags.Num()!=T||C.InitialDynamicState.Num()!=T||C.ExemplarIndex.Num()!=T||C.Vertex.Num()!=V||C.BoneMap.Num()!=V||C.Normal.Num()!=V||C.TangentU.Num()!=V||C.TangentV.Num()!=V||C.Color.Num()!=V||C.Indices.Num()!=F||C.Visible.Num()!=F||C.MaterialIndex.Num()!=F||C.MaterialID.Num()!=F||C.Internal.Num()!=F||C.TransformIndex.Num()!=G||C.BoundingBox.Num()!=G||C.InnerRadius.Num()!=G||C.OuterRadius.Num()!=G||C.VertexStart.Num()!=G||C.VertexCount.Num()!=G||C.FaceStart.Num()!=G||C.FaceCount.Num()!=G;
    if(bBudgetExceeded||bCountExceeded||bAttributeMismatch)
    {
        auto D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("transform_count"),T);D->SetNumberField(TEXT("vertex_count"),V);D->SetNumberField(TEXT("face_count"),F);D->SetNumberField(TEXT("geometry_count"),G);
        D->SetNumberField(TEXT("estimated_layout_bytes"),EstimatedLayoutBytes);D->SetNumberField(TEXT("layout_budget_bytes"),LayoutBudgetBytes);D->SetBoolField(TEXT("budget_exceeded"),bBudgetExceeded);D->SetBoolField(TEXT("count_limit_exceeded"),bCountExceeded);D->SetBoolField(TEXT("attribute_length_mismatch"),bAttributeMismatch);D->SetStringField(TEXT("SDK_stage"),Work->R.Stage);
        auto L=MakeShared<FJsonObject>();
        L->SetNumberField(TEXT("Transform"),C.Transform.Num());
        L->SetNumberField(TEXT("Parent"),C.Parent.Num());
        L->SetNumberField(TEXT("Children"),C.Children.Num());
        L->SetNumberField(TEXT("SimulationType"),C.SimulationType.Num());
        L->SetNumberField(TEXT("TransformToGeometryIndex"),C.TransformToGeometryIndex.Num());
        L->SetNumberField(TEXT("StatusFlags"),C.StatusFlags.Num());
        L->SetNumberField(TEXT("InitialDynamicState"),C.InitialDynamicState.Num());
        L->SetNumberField(TEXT("ExemplarIndex"),C.ExemplarIndex.Num());
        L->SetNumberField(TEXT("Vertex"),C.Vertex.Num());
        L->SetNumberField(TEXT("BoneMap"),C.BoneMap.Num());
        L->SetNumberField(TEXT("Normal"),C.Normal.Num());
        L->SetNumberField(TEXT("TangentU"),C.TangentU.Num());
        L->SetNumberField(TEXT("TangentV"),C.TangentV.Num());
        L->SetNumberField(TEXT("Color"),C.Color.Num());
        L->SetNumberField(TEXT("Indices"),C.Indices.Num());
        L->SetNumberField(TEXT("Visible"),C.Visible.Num());
        L->SetNumberField(TEXT("MaterialIndex"),C.MaterialIndex.Num());
        L->SetNumberField(TEXT("MaterialID"),C.MaterialID.Num());
        L->SetNumberField(TEXT("Internal"),C.Internal.Num());
        L->SetNumberField(TEXT("TransformIndex"),C.TransformIndex.Num());
        L->SetNumberField(TEXT("BoundingBox"),C.BoundingBox.Num());
        L->SetNumberField(TEXT("InnerRadius"),C.InnerRadius.Num());
        L->SetNumberField(TEXT("OuterRadius"),C.OuterRadius.Num());
        L->SetNumberField(TEXT("VertexStart"),C.VertexStart.Num());
        L->SetNumberField(TEXT("VertexCount"),C.VertexCount.Num());
        L->SetNumberField(TEXT("FaceStart"),C.FaceStart.Num());
        L->SetNumberField(TEXT("FaceCount"),C.FaceCount.Num());
        D->SetObjectField(TEXT("attribute_lengths"),L);Work->R.Data->SetObjectField(TEXT("collection_schema_failure"),D);
        return Work->Fail(TEXT("collection_schema"),TEXT("Managed collection group lengths or bounded size are invalid; see collection_schema_failure for actual counts and separate bounds"));
    }
    TArray<uint8> Colors;Colors.Init(0,T);for(int32 I=0;I<T;++I) { if(C.Transform[I].ContainsNaN()) { return Work->Fail(TEXT("collection_transform"),TEXT("Managed collection contains a non-finite transform")); }int32 K=I;while(K!=INDEX_NONE&&C.Parent.IsValidIndex(K)&&Colors[K]==0) { Colors[K]=1;K=C.Parent[K]; }if(K!=INDEX_NONE&&(!C.Parent.IsValidIndex(K)||Colors[K]==1)) { return Work->Fail(TEXT("collection_parent"),TEXT("Managed collection has invalid parent or cycle")); }K=I;while(K!=INDEX_NONE&&C.Parent.IsValidIndex(K)&&Colors[K]==1) { Colors[K]=2;K=C.Parent[K]; }for(int32 Child:C.Children[I]) { if(!C.Parent.IsValidIndex(Child)||C.Parent[Child]!=I) { return Work->Fail(TEXT("collection_child"),TEXT("Managed collection child does not reciprocate its parent")); } }const int32 P=C.Parent[I];if(P!=INDEX_NONE&&(!C.Children.IsValidIndex(P)||!C.Children[P].Contains(I))) { return Work->Fail(TEXT("collection_parent_child"),TEXT("Managed collection parent does not contain its child")); } }
    for(int32 I=0;I<T;++I) { const int32 GI=C.TransformToGeometryIndex[I];if(GI!=INDEX_NONE&&(!C.TransformIndex.IsValidIndex(GI)||C.TransformIndex[GI]!=I)) { return Work->Fail(TEXT("collection_transform_geometry_mapping"),TEXT("Transform-to-geometry mapping is out of range or not reciprocal")); } }
    for(int32 I=0;I<G;++I) { const int32 TI=C.TransformIndex[I];if(!C.TransformToGeometryIndex.IsValidIndex(TI)||C.TransformToGeometryIndex[TI]!=I) { return Work->Fail(TEXT("collection_geometry_transform_mapping"),TEXT("Geometry-to-transform mapping is out of range or not reciprocal")); }if(C.VertexStart[I]<0||C.VertexCount[I]<0||static_cast<int64>(C.VertexStart[I])+C.VertexCount[I]>V||C.FaceStart[I]<0||C.FaceCount[I]<0||static_cast<int64>(C.FaceStart[I])+C.FaceCount[I]>F) { return Work->Fail(TEXT("collection_range"),TEXT("Managed geometry range is invalid")); } }
    for(int32 I=0;I<V;++I) { if(!C.Transform.IsValidIndex(C.BoneMap[I])) { return Work->Fail(TEXT("collection_vertex_bone"),TEXT("Vertex bone mapping is outside the transform group")); } }
    for(int32 I=0;I<F;++I) { const auto& Triangle=C.Indices[I];if(!C.Vertex.IsValidIndex(Triangle.X)||!C.Vertex.IsValidIndex(Triangle.Y)||!C.Vertex.IsValidIndex(Triangle.Z)) { return Work->Fail(TEXT("collection_face_vertex"),TEXT("Face indices are outside the vertex group")); } }return true;
}
struct FLegacy
{
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& D) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),true);J->SetObjectField(TEXT("data"),D);return J; }
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& E) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),E);Work->Fail(TEXT("operation_failed"),E);return J; }
};
constexpr TCHAR AuthoringRoot[] = TEXT("/Game/_MCP_Temp/ChaosDestructionLearning/Cinematic");
constexpr TCHAR EngineAnchorBlueprintClassPath[] =
    TEXT("/Engine/EditorResources/FieldNodes/FS_AnchorField_Generic.FS_AnchorField_Generic_C");

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
    return Path.StartsWith(AuthoringRoot + FString(TEXT("/"))) && !Path.Contains(TEXT("..")) && !Dependency(Path);
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

bool TryOptionalStrictVector(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Key,
    const FVector& Default,
    FVector& OutValue,
    FString& OutError)
{
    OutValue = Default;
    if (!Object.IsValid() || !Object->HasField(Key))
    {
        return Object.IsValid();
    }
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object->TryGetArrayField(Key, Values) || !Values || Values->Num() != 3)
    {
        OutError = FString::Printf(TEXT("%s must be an array of exactly three finite numbers"), Key);
        return false;
    }
    double Components[3] = {};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetNumber(Components[Index]) ||
            !FMath::IsFinite(Components[Index]))
        {
            OutError = FString::Printf(TEXT("%s[%d] must be a finite number"), Key, Index);
            return false;
        }
    }
    OutValue = FVector(Components[0], Components[1], Components[2]);
    return true;
}

bool TryTransformFromJson(const TSharedPtr<FJsonObject>& Object, FTransform& OutTransform, FString& OutError)
{
    if (!Object.IsValid())
    {
        OutError = TEXT("transform entry must be a JSON object");
        return false;
    }
    FVector Location;
    FVector Rotation;
    FVector Scale;
    if (!TryOptionalStrictVector(Object, TEXT("location"), FVector::ZeroVector, Location, OutError) ||
        !TryOptionalStrictVector(Object, TEXT("rotation"), FVector::ZeroVector, Rotation, OutError) ||
        !TryOptionalStrictVector(Object, TEXT("scale"), FVector::OneVector, Scale, OutError))
    {
        return false;
    }
    OutTransform = FTransform(FRotator(Rotation.Y, Rotation.Z, Rotation.X), Location, Scale);
    return !OutTransform.ContainsNaN();
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

FString HashCollectionTransforms(const UGeometryCollection* Asset)
{
    FString Canonical;
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection =
        Asset ? Asset->GetGeometryCollection() : nullptr;
    if (Collection.IsValid() && Schema(*Collection))
    {
        for (int32 Index = 0; Index < Collection->Transform.Num(); ++Index)
        {
            const FTransform3f& Transform32 = Collection->Transform[Index];
            const FTransform Transform(Transform32);
            const FVector Location = Transform.GetLocation();
            const FQuat Rotation = Transform.GetRotation();
            const FVector Scale = Transform.GetScale3D();
            Canonical += FString::Printf(
                TEXT("%d|%lld,%lld,%lld|%lld,%lld,%lld,%lld|%lld,%lld,%lld;"), Index,
                Quantize(Location.X,1000000.0), Quantize(Location.Y,1000000.0),
                Quantize(Location.Z,1000000.0), Quantize(Rotation.X,1000000.0),
                Quantize(Rotation.Y,1000000.0), Quantize(Rotation.Z,1000000.0),
                Quantize(Rotation.W,1000000.0), Quantize(Scale.X,1000000.0),
                Quantize(Scale.Y,1000000.0), Quantize(Scale.Z,1000000.0));
        }
    }
    FTCHARToUTF8 Utf8(*Canonical);
    return Sha256Bytes(Utf8.Get(), Utf8.Length());
}

FString HashObjectPackageBytes(const UObject* Object)
{
    if (!Object) return FString();
    FString PackageFilename;
    TArray<uint8> PackageBytes;
    const bool bResolved = FPackageName::TryConvertLongPackageNameToFilename(
        Object->GetOutermost()->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension());
    if (bResolved && FFileHelper::LoadFileToArray(PackageBytes, *PackageFilename))
    {
        return Sha256Bytes(PackageBytes.GetData(), PackageBytes.Num());
    }
    // Levels are package objects too, but are persisted as .umap rather than
    // .uasset. The asset-first fallback keeps mesh/cache hashing unchanged.
    PackageBytes.Reset();
    const bool bMapResolved = FPackageName::TryConvertLongPackageNameToFilename(
        Object->GetOutermost()->GetName(), PackageFilename, FPackageName::GetMapPackageExtension());
    return bMapResolved && FFileHelper::LoadFileToArray(PackageBytes, *PackageFilename)
        ? Sha256Bytes(PackageBytes.GetData(), PackageBytes.Num()) : FString();
}

FString HashStaticMeshSources(const TArray<FString>& Paths)
{
    FString Canonical;
    for (const FString& Path : Paths)
    {
        if (const UStaticMesh* Mesh = Work->Load<UStaticMesh>(Path,TEXT("hash_source_static_mesh")))
        {
            const FString PackageHash = HashObjectPackageBytes(Mesh);
            Canonical += FString::Printf(TEXT("%s|%s;"), *Mesh->GetOutermost()->GetName(), *PackageHash);
        }
    }
    FTCHARToUTF8 Utf8(*Canonical);
    return Sha256Bytes(Utf8.Get(), Utf8.Length());
}

TSharedPtr<FJsonObject> CommandError(const FString& E) { return FLegacy::CreateErrorResponse(E); }
bool SaveAsset(UObject* Asset)
{
    Work->R.bSaveRequested=true;if(!Live(Asset)||!Guard()||!Work->R.Error.IsEmpty()||!Work->Track(Asset,TEXT("explicit_saved_target"))) { return false; }TStrongObjectPtr<UPackage> Package(Asset->GetPackage());const FString Path=Asset->IsA<UWorld>()?Package->GetName():Asset->GetPathName(),PackagePath=Package->GetName();
    if(!Step(TEXT("mark_explicit_save_target_dirty"),[&]{Asset->MarkPackageDirty();})) { return false; }Work->Write(TEXT("explicit_SaveAsset"));Work->R.bSaveAttempted=true;Work->Count(TEXT("explicit_save_calls"));const bool Saved=UEditorAssetLibrary::SaveAsset(Path,false);Work->R.bSaved|=Saved;
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Path);J->SetStringField(TEXT("captured_package_before"),PackagePath);J->SetBoolField(TEXT("SDK_save_returned"),Saved);J->SetBoolField(TEXT("captured_package_live"),Live(Package.Get()));if(Live(Package.Get())) { J->SetStringField(TEXT("captured_package_after"),Package->GetName());J->SetBoolField(TEXT("captured_package_dirty"),Package->IsDirty());J->SetBoolField(TEXT("captured_package_name_unchanged"),Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)); }Work->Saves.Add(MakeShared<FJsonValueObject>(J));J->SetBoolField(TEXT("target_identity_after_save"),Guard());return Saved;
}
void FinalizeCollection(UGeometryCollection* Asset)
{
    if(!Guard()||!Asset||!Asset->GetGeometryCollection().IsValid()||!Schema(*Asset->GetGeometryCollection())) { return; }
    if(!Step(TEXT("InvalidateCollection"),[&]{Asset->InvalidateCollection();})) { return; }
    if(!Step(TEXT("UpdateGeometryDependentProperties"),[&]{Asset->UpdateGeometryDependentProperties();})) { return; }
    if(!Step(TEXT("CreateSimulationData"),[&]{Asset->CreateSimulationData();})) { return; }
    if(!Step(TEXT("RebuildRenderData"),[&]{Asset->RebuildRenderData();})) { return; }
    Step(TEXT("mark_finalized_collection_dirty"),[&]{Asset->MarkPackageDirty();});
}

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

FCollectionIntegrityMetrics AnalyzeCollectionIntegrity(const FGeometryCollection& Collection)
{
    FCollectionIntegrityMetrics Metrics;
    if(!Schema(Collection)) { Metrics.InvalidGeometryRangeCount=1;return Metrics; }
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
    if (Collection.IsValid() && Schema(*Collection))
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

int32 CountTinyGeometry(const UGeometryCollection* Asset, double RelativeVolumeThreshold)
{
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection =
        Asset ? Asset->GetGeometryCollection() : nullptr;
    if (!Collection.IsValid() || !Schema(*Collection) || Collection->BoundingBox.Num() == 0)
    {
        return 0;
    }
    double TotalVolume = 0.0;
    TArray<double> Volumes;
    for (const FBox& Box : Collection->BoundingBox)
    {
        const double Volume = FMath::Max(0.0, Box.GetVolume());
        Volumes.Add(Volume);
        TotalVolume += Volume;
    }
    // Match FractureEngineUtility's RelativeVolume semantics: it compares
    // volumes against (cuberoot(total) * relative)^3, not total * relative.
    const double MinimumVolume = TotalVolume * FMath::Pow(RelativeVolumeThreshold, 3.0);
    int32 Count = 0;
    for (double Volume : Volumes)
    {
        Count += TotalVolume > UE_DOUBLE_SMALL_NUMBER && Volume <= MinimumVolume ? 1 : 0;
    }
    return Count;
}

FString HashCollectionTopology(const UGeometryCollection* Asset)
{
    FString Canonical;
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection =
        Asset ? Asset->GetGeometryCollection() : nullptr;
    if (Collection.IsValid() && Schema(*Collection))
    {
        for (int32 Index = 0; Index < Collection->NumElements(FGeometryCollection::TransformGroup); ++Index)
        {
            TArray<int32> Children = Collection->Children[Index].Array();
            Children.Sort();
            Canonical += FString::Printf(TEXT("%d|%d|%d|"), Index, Collection->Parent[Index], Collection->SimulationType[Index]);
            for (int32 Child : Children) Canonical += FString::Printf(TEXT("%d,"), Child);
            Canonical += TEXT(";");
        }
    }
    FTCHARToUTF8 Utf8(*Canonical);
    return Sha256Bytes(Utf8.Get(), Utf8.Length());
}

FString HashCollectionManagedGeometry(
    const UGeometryCollection* Asset,
    const bool bIncludeCollisionMappings = true)
{
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection =
        Asset ? Asset->GetGeometryCollection() : nullptr;
    if (!Collection.IsValid() || !Schema(*Collection))
    {
        return FString();
    }

    // Hash the canonical, public UE 5.6 managed geometry state rather than
    // relying on transforms/hierarchy alone. Two fractures can share particle
    // transforms while producing different fragment vertices or face topology.
    FString Canonical(TEXT("GeometryCollectionManagedGeometryV1|"));
    Canonical.Reserve(
        Collection->Transform.Num() * 256 + Collection->Vertex.Num() * 192 +
        Collection->Indices.Num() * 96 + Collection->TransformIndex.Num() * 192);
    const auto AppendInteger = [&Canonical](const int64 Value)
    {
        Canonical += FString::Printf(TEXT("%lld,"), Value);
    };
    const auto AppendScalar = [&AppendInteger](const double Value)
    {
        AppendInteger(Quantize(Value,1000000.0));
    };
    const auto AppendVector2 = [&AppendScalar](const FVector2f& Value)
    {
        AppendScalar(Value.X);
        AppendScalar(Value.Y);
    };
    const auto AppendVector3 = [&AppendScalar](const auto& Value)
    {
        AppendScalar(Value.X);
        AppendScalar(Value.Y);
        AppendScalar(Value.Z);
    };

    AppendInteger(Collection->NumElements(FGeometryCollection::TransformGroup));
    AppendInteger(Collection->NumElements(FGeometryCollection::VerticesGroup));
    AppendInteger(Collection->NumElements(FGeometryCollection::FacesGroup));
    AppendInteger(Collection->NumElements(FGeometryCollection::GeometryGroup));
    AppendInteger(Collection->NumElements(FGeometryCollection::MaterialGroup));

    Canonical += TEXT("T|");
    for (int32 Index = 0; Index < Collection->Transform.Num(); ++Index)
    {
        const FTransform3f& Transform = Collection->Transform[Index];
        AppendVector3(Transform.GetTranslation());
        const FQuat4f Rotation = Transform.GetRotation();
        AppendScalar(Rotation.X);
        AppendScalar(Rotation.Y);
        AppendScalar(Rotation.Z);
        AppendScalar(Rotation.W);
        AppendVector3(Transform.GetScale3D());
        AppendInteger(Collection->Parent[Index]);
        TArray<int32> Children = Collection->Children[Index].Array();
        Children.Sort();
        AppendInteger(Children.Num());
        for (const int32 Child : Children)
        {
            AppendInteger(Child);
        }
        AppendInteger(Collection->TransformToGeometryIndex[Index]);
        AppendInteger(Collection->SimulationType[Index]);
        AppendInteger(Collection->StatusFlags[Index]);
        AppendInteger(Collection->InitialDynamicState[Index]);
        AppendInteger(Collection->ExemplarIndex[Index]);
        Canonical += TEXT(";");
    }

    Canonical += TEXT("V|");
    for (int32 Index = 0; Index < Collection->Vertex.Num(); ++Index)
    {
        AppendVector3(Collection->Vertex[Index]);
        AppendInteger(Collection->BoneMap[Index]);
        AppendVector3(Collection->Normal[Index]);
        AppendVector3(Collection->TangentU[Index]);
        AppendVector3(Collection->TangentV[Index]);
        const FLinearColor& Color = Collection->Color[Index];
        AppendScalar(Color.R);
        AppendScalar(Color.G);
        AppendScalar(Color.B);
        AppendScalar(Color.A);
        Canonical += TEXT(";");
    }
    const int32 UVLayerCount = Collection->NumUVLayers();
    AppendInteger(UVLayerCount);
    for (int32 LayerIndex = 0; LayerIndex < UVLayerCount; ++LayerIndex)
    {
        const TManagedArray<FVector2f>* UVLayer = Collection->FindUVLayer(LayerIndex);
        AppendInteger(UVLayer ? UVLayer->Num() : INDEX_NONE);
        if (UVLayer)
        {
            for (const FVector2f& UV : *UVLayer)
            {
                AppendVector2(UV);
            }
        }
        Canonical += TEXT(";");
    }

    Canonical += TEXT("F|");
    for (int32 Index = 0; Index < Collection->Indices.Num(); ++Index)
    {
        const FIntVector& Face = Collection->Indices[Index];
        AppendInteger(Face.X);
        AppendInteger(Face.Y);
        AppendInteger(Face.Z);
        AppendInteger(Collection->Visible[Index] ? 1 : 0);
        AppendInteger(Collection->MaterialIndex[Index]);
        AppendInteger(Collection->MaterialID[Index]);
        AppendInteger(Collection->Internal[Index] ? 1 : 0);
        Canonical += TEXT(";");
    }

    Canonical += TEXT("G|");
    for (int32 Index = 0; Index < Collection->TransformIndex.Num(); ++Index)
    {
        AppendInteger(Collection->TransformIndex[Index]);
        const FBox& Bounds = Collection->BoundingBox[Index];
        AppendInteger(Bounds.IsValid ? 1 : 0);
        if (Bounds.IsValid)
        {
            AppendVector3(Bounds.Min);
            AppendVector3(Bounds.Max);
        }
        AppendScalar(Collection->InnerRadius[Index]);
        AppendScalar(Collection->OuterRadius[Index]);
        AppendInteger(Collection->VertexStart[Index]);
        AppendInteger(Collection->VertexCount[Index]);
        AppendInteger(Collection->FaceStart[Index]);
        AppendInteger(Collection->FaceCount[Index]);
        Canonical += TEXT(";");
    }

    Canonical += TEXT("M|");
    for (const FGeometryCollectionSection& Section : Collection->Sections)
    {
        AppendInteger(Section.MaterialID);
        AppendInteger(Section.FirstIndex);
        AppendInteger(Section.NumTriangles);
        AppendInteger(Section.MinVertexIndex);
        AppendInteger(Section.MaxVertexIndex);
        Canonical += TEXT(";");
    }

    // These optional UE 5.6 attributes are stable integer/bool mappings used
    // by collision setup. Hash them when present without serializing pointer-
    // owned implicit objects or other derived runtime data.
    if (bIncludeCollisionMappings)
    {
        const TManagedArray<bool>* SimulatableParticles = Collection->FindAttribute<bool>(
            FGeometryCollection::SimulatableParticlesAttribute,
            FGeometryCollection::TransformGroup);
        AppendInteger(SimulatableParticles ? SimulatableParticles->Num() : INDEX_NONE);
        if (SimulatableParticles)
        {
            for (int32 Index = 0; Index < SimulatableParticles->Num(); ++Index)
            {
                AppendInteger((*SimulatableParticles)[Index] ? 1 : 0);
            }
        }
        const TManagedArray<TSet<int32>>* TransformToConvexIndices =
            Collection->FindAttribute<TSet<int32>>(
                TEXT("TransformToConvexIndices"), FGeometryCollection::TransformGroup);
        AppendInteger(TransformToConvexIndices ? TransformToConvexIndices->Num() : INDEX_NONE);
        if (TransformToConvexIndices)
        {
            for (const TSet<int32>& ConvexIndices : *TransformToConvexIndices)
            {
                TArray<int32> SortedIndices = ConvexIndices.Array();
                SortedIndices.Sort();
                AppendInteger(SortedIndices.Num());
                for (const int32 ConvexIndex : SortedIndices)
                {
                    AppendInteger(ConvexIndex);
                }
            }
        }
    }
    else
    {
        Canonical += TEXT("CollisionMappingsExcluded|");
    }

    FTCHARToUTF8 Utf8(*Canonical);
    return Sha256Bytes(Utf8.Get(), Utf8.Length());
}

FString HashCollectionFragmentSpatialSignature(const UGeometryCollection* Asset)
{
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection =
        Asset ? Asset->GetGeometryCollection() : nullptr;
    if (!Collection.IsValid() || !Schema(*Collection))
    {
        return FString();
    }

    TArray<FTransform> GlobalTransforms;
    GeometryCollectionAlgo::GlobalMatrices(
        Collection->Transform, Collection->Parent, GlobalTransforms);
    TArray<FString> Fragments;
    Fragments.Reserve(Collection->TransformIndex.Num());
    const auto QuantizeSpatial = [](const double Value)
    {
        return Quantize(Value,10000.0);
    };
    for (int32 GeometryIndex = 0;
        GeometryIndex < Collection->TransformIndex.Num(); ++GeometryIndex)
    {
        const int32 TransformIndex = Collection->TransformIndex[GeometryIndex];
        if (!Collection->BoundingBox.IsValidIndex(GeometryIndex) ||
            !GlobalTransforms.IsValidIndex(TransformIndex))
        {
            return FString();
        }
        const FBox Bounds = Collection->BoundingBox[GeometryIndex].TransformBy(
            GlobalTransforms[TransformIndex]);
        if (!Bounds.IsValid || !Collection->VertexCount.IsValidIndex(GeometryIndex) ||
            !Collection->FaceCount.IsValidIndex(GeometryIndex))
        {
            return FString();
        }
        Fragments.Add(FString::Printf(
            TEXT("%d|%d|%lld,%lld,%lld|%lld,%lld,%lld"),
            Collection->VertexCount[GeometryIndex],
            Collection->FaceCount[GeometryIndex],
            QuantizeSpatial(Bounds.Min.X), QuantizeSpatial(Bounds.Min.Y), QuantizeSpatial(Bounds.Min.Z),
            QuantizeSpatial(Bounds.Max.X), QuantizeSpatial(Bounds.Max.Y), QuantizeSpatial(Bounds.Max.Z)));
    }
    Fragments.Sort();
    FString Canonical(TEXT("GeometryCollectionFragmentSpatialSignatureV1|"));
    for (const FString& Fragment : Fragments)
    {
        Canonical += Fragment;
        Canonical += TEXT(";");
    }
    FTCHARToUTF8 Utf8(*Canonical);
    return Sha256Bytes(Utf8.Get(), Utf8.Length());
}

void BuildClusterHierarchy(
    FGeometryCollection& Collection,
    int32 ClusterLevels,
    int32 ClusterSize,
    const bool bClusterAllRoots)
{
    if(!Schema(Collection)||!Guard()) { return; }
    TArray<int32> CurrentLevel;
    for (int32 Index = 0; Index < Collection.NumElements(FGeometryCollection::TransformGroup); ++Index)
    {
        if (Collection.SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid &&
            Collection.Children[Index].IsEmpty())
        {
            CurrentLevel.Add(Index);
        }
    }
    // Assemble building clusters in vertical bands. Source-transform order
    // groups fractured pieces by the input mesh, which can make every
    // top-level wall cluster contain at least one anchored foundation piece.
    // Sorting by rest-space height keeps foundation clusters local while
    // leaving upper clusters dynamic after an Anchor Field is evaluated.
    TArray<FTransform> GlobalTransforms;
    GeometryCollectionAlgo::GlobalMatrices(
        Collection.Transform, Collection.Parent, GlobalTransforms);
    CurrentLevel.Sort([&GlobalTransforms](const int32 A, const int32 B)
    {
        const FVector AL = GlobalTransforms.IsValidIndex(A)
            ? GlobalTransforms[A].GetLocation() : FVector::ZeroVector;
        const FVector BL = GlobalTransforms.IsValidIndex(B)
            ? GlobalTransforms[B].GetLocation() : FVector::ZeroVector;
        if (!FMath::IsNearlyEqual(AL.Z, BL.Z)) return AL.Z < BL.Z;
        if (!FMath::IsNearlyEqual(AL.X, BL.X)) return AL.X < BL.X;
        if (!FMath::IsNearlyEqual(AL.Y, BL.Y)) return AL.Y < BL.Y;
        return A < B;
    });
    for (int32 Level = 0; Level < ClusterLevels && CurrentLevel.Num() > 1; ++Level)
    {
        TArray<int32> NextLevel;
        for (int32 Start = CurrentLevel.Num(); Start > 0; Start -= ClusterSize)
        {
            const int32 GroupStart = FMath::Max(0, Start - ClusterSize);
            TArray<int32> Group;
            for (int32 Index = GroupStart; Index < Start; ++Index) Group.Add(CurrentLevel[Index]);
            NextLevel.Add(Group.Num() > 1
                ? FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(
                    &Collection, Group[0], Group, true, true)
                : Group[0]);
        }
        CurrentLevel = MoveTemp(NextLevel);
    }
    if (bClusterAllRoots &&
        FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(&Collection))
    {
        FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(
            &Collection, TEXT("BuildingFractureRoot"), true);
    }
}

int32 RecenterRigidGeometryTransforms(FGeometryCollection& Collection)
{
    if(!Schema(Collection)||!Guard()) { return 0; }
    int32 RecenteredCount = 0;
    const int32 TransformCount =
        Collection.NumElements(FGeometryCollection::TransformGroup);
    for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
    {
        if (Collection.SimulationType[TransformIndex] !=
                FGeometryCollection::ESimulationTypes::FST_Rigid ||
            !Collection.Children[TransformIndex].IsEmpty() ||
            !Collection.TransformToGeometryIndex.IsValidIndex(TransformIndex))
        {
            continue;
        }
        const int32 GeometryIndex =
            Collection.TransformToGeometryIndex[TransformIndex];
        if (!Collection.BoundingBox.IsValidIndex(GeometryIndex) ||
            !Collection.VertexStart.IsValidIndex(GeometryIndex) ||
            !Collection.VertexCount.IsValidIndex(GeometryIndex))
        {
            continue;
        }
        const FVector Center = Collection.BoundingBox[GeometryIndex].GetCenter();
        if (Center.IsNearlyZero())
        {
            continue;
        }
        const int32 VertexStart = Collection.VertexStart[GeometryIndex];
        const int32 VertexCount = Collection.VertexCount[GeometryIndex];
        if (VertexStart < 0 || VertexCount <= 0 ||
            static_cast<int64>(VertexStart) + VertexCount > Collection.Vertex.Num())
        {
            continue;
        }
        const FVector3f Center3f(Center);
        for (int32 VertexIndex = VertexStart;
             VertexIndex < VertexStart + VertexCount;
             ++VertexIndex)
        {
            Collection.Vertex[VertexIndex] -= Center3f;
        }
        Collection.BoundingBox[GeometryIndex] =
            Collection.BoundingBox[GeometryIndex].ShiftBy(-Center);
        const FTransform3f CenterShift(FQuat4f::Identity, Center3f);
        Collection.Transform[TransformIndex] =
            CenterShift * Collection.Transform[TransformIndex];
        ++RecenteredCount;
    }
    return RecenteredCount;
}

struct FClusterRecenterSnapshot
{
    TArray<FTransform3f> Transform;
    TArray<FVector3f> Vertex;
    TArray<FBox> BoundingBox;

    void Capture(const FGeometryCollection& Collection)
    {
        Transform.SetNum(Collection.Transform.Num());
        Vertex.SetNum(Collection.Vertex.Num());
        BoundingBox.SetNum(Collection.BoundingBox.Num());
        for (int32 Index = 0; Index < Transform.Num(); ++Index)
        {
            Transform[Index] = Collection.Transform[Index];
        }
        for (int32 Index = 0; Index < Vertex.Num(); ++Index)
        {
            Vertex[Index] = Collection.Vertex[Index];
        }
        for (int32 Index = 0; Index < BoundingBox.Num(); ++Index)
        {
            BoundingBox[Index] = Collection.BoundingBox[Index];
        }
    }

    bool CanRestore(const FGeometryCollection& Collection) const
    {
        return Transform.Num() == Collection.Transform.Num() &&
            Vertex.Num() == Collection.Vertex.Num() &&
            BoundingBox.Num() == Collection.BoundingBox.Num();
    }

    bool Restore(FGeometryCollection& Collection) const
    {
        if (!CanRestore(Collection))
        {
            return false;
        }
        for (int32 Index = 0; Index < Transform.Num(); ++Index)
        {
            Collection.Transform[Index] = Transform[Index];
        }
        for (int32 Index = 0; Index < Vertex.Num(); ++Index)
        {
            Collection.Vertex[Index] = Vertex[Index];
        }
        for (int32 Index = 0; Index < BoundingBox.Num(); ++Index)
        {
            Collection.BoundingBox[Index] = BoundingBox[Index];
        }
        return true;
    }
};

int32 RecenterClusterTransforms(FGeometryCollection& Collection)
{
    if(!Schema(Collection)||!Guard()) { return 0; }
    TArray<int32> ClusterIndices;
    for (int32 Index = 0;
         Index < Collection.NumElements(FGeometryCollection::TransformGroup);
         ++Index)
    {
        if (Collection.SimulationType[Index] ==
            FGeometryCollection::ESimulationTypes::FST_Clustered)
        {
            ClusterIndices.Add(Index);
        }
    }
    ClusterIndices.Sort([&Collection](const int32 A, const int32 B)
    {
        const auto Depth = [&Collection](int32 Index)
        {
            int32 Value = 0;
            while (Collection.Parent.IsValidIndex(Index) &&
                Collection.Parent[Index] != INDEX_NONE)
            {
                Index = Collection.Parent[Index];
                ++Value;
            }
            return Value;
        };
        return Depth(A) > Depth(B);
    });

    int32 RecenteredCount = 0;
    for (const int32 ClusterIndex : ClusterIndices)
    {
        TArray<FTransform> Globals;
        GeometryCollectionAlgo::GlobalMatrices(
            Collection.Transform, Collection.Parent, Globals);
        if (!Globals.IsValidIndex(ClusterIndex)) continue;
        FBox DescendantBounds(EForceInit::ForceInit);
        for (int32 Candidate = 0; Candidate < Globals.Num(); ++Candidate)
        {
            if (Collection.SimulationType[Candidate] !=
                    FGeometryCollection::ESimulationTypes::FST_Rigid ||
                !Collection.Children[Candidate].IsEmpty() ||
                !Collection.TransformToGeometryIndex.IsValidIndex(Candidate))
            {
                continue;
            }
            int32 Cursor = Candidate;
            while (Cursor != INDEX_NONE && Cursor != ClusterIndex &&
                Collection.Parent.IsValidIndex(Cursor))
            {
                Cursor = Collection.Parent[Cursor];
            }
            if (Cursor != ClusterIndex) continue;
            const int32 GeometryIndex =
                Collection.TransformToGeometryIndex[Candidate];
            if (Collection.BoundingBox.IsValidIndex(GeometryIndex))
            {
                DescendantBounds += Collection.BoundingBox[GeometryIndex]
                    .TransformBy(Globals[Candidate]);
            }
        }
        if (!DescendantBounds.IsValid) continue;

        const FTransform OldGlobal = Globals[ClusterIndex];
        FTransform NewGlobal = OldGlobal;
        NewGlobal.SetLocation(DescendantBounds.GetCenter());
        if (NewGlobal.GetLocation().Equals(OldGlobal.GetLocation(), 0.001))
        {
            continue;
        }
        const int32 ParentIndex = Collection.Parent[ClusterIndex];
        const FTransform ParentGlobal = Globals.IsValidIndex(ParentIndex)
            ? Globals[ParentIndex] : FTransform::Identity;
        const FTransform NewGlobalInverse = NewGlobal.Inverse();
        const int32 OwnedGeometryIndex =
            Collection.TransformToGeometryIndex.IsValidIndex(ClusterIndex)
                ? Collection.TransformToGeometryIndex[ClusterIndex]
                : INDEX_NONE;
        if (Collection.VertexStart.IsValidIndex(OwnedGeometryIndex) &&
            Collection.VertexCount.IsValidIndex(OwnedGeometryIndex) &&
            Collection.BoundingBox.IsValidIndex(OwnedGeometryIndex))
        {
            const int32 VertexStart = Collection.VertexStart[OwnedGeometryIndex];
            const int32 VertexCount = Collection.VertexCount[OwnedGeometryIndex];
            if (VertexStart >= 0 && VertexCount > 0 &&
                static_cast<int64>(VertexStart) + VertexCount <= Collection.Vertex.Num())
            {
                FBox RecenteredBounds(EForceInit::ForceInit);
                for (int32 VertexIndex = VertexStart;
                     VertexIndex < VertexStart + VertexCount;
                     ++VertexIndex)
                {
                    const FVector OldWorldPosition = OldGlobal.TransformPosition(
                        FVector(Collection.Vertex[VertexIndex]));
                    const FVector NewLocalPosition =
                        NewGlobalInverse.TransformPosition(OldWorldPosition);
                    Collection.Vertex[VertexIndex] = FVector3f(NewLocalPosition);
                    RecenteredBounds += NewLocalPosition;
                }
                Collection.BoundingBox[OwnedGeometryIndex] = RecenteredBounds;
            }
        }
        for (const int32 ChildIndex : Collection.Children[ClusterIndex])
        {
            if (Globals.IsValidIndex(ChildIndex))
            {
                Collection.Transform[ChildIndex] =
                    FTransform3f(Globals[ChildIndex] * NewGlobalInverse);
            }
        }
        Collection.Transform[ClusterIndex] =
            FTransform3f(NewGlobal * ParentGlobal.Inverse());
        ++RecenteredCount;
    }
    return RecenteredCount;
}

void MergeTinyGeometry(FGeometryCollection& Collection, double TinyRelativeThreshold)
{
    if(!Schema(Collection)||!Guard()) { return; }
    FDataflowTransformSelection TinySelection;
    TinySelection.Initialize(Collection.NumElements(FGeometryCollection::TransformGroup), false);
    FFractureEngineUtility::FixTinyGeo(
        Collection,
        TinySelection,
        EFixTinyGeoMergeType::MergeGeometry,
        false,
        EFixTinyGeoGeometrySelectionMethod::RelativeVolume,
        2.0f,
        static_cast<float>(TinyRelativeThreshold),
        EFixTinyGeoUseBoneSelection::NoEffect,
        false,
        EFixTinyGeoNeighborSelectionMethod::LargestNeighbor,
        false,
        false,
        true);
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

void AppendAnchorRestSelectionEvidence(
    const TSharedPtr<FJsonObject>& Data,
    const UGeometryCollectionComponent* Component,
    const FBox& AnchorBounds)
{
    const UGeometryCollection* RestAsset = Component ? Component->GetRestCollection() : nullptr;
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Rest =
        RestAsset ? RestAsset->GetGeometryCollection() : nullptr;
    int32 RigidLeafPopulation = 0;
    int32 SelectedRigidLeaves = 0;
    int32 ClusterPopulation = 0;
    int32 SelectedClusters = 0;
    int32 SelectedRoots = 0;
    TArray<TSharedPtr<FJsonValue>> ClusterSamples;
    TArray<TSharedPtr<FJsonValue>> RootBranchSummaries;
    if (Rest.IsValid() && Schema(*Rest) && AnchorBounds.IsValid)
    {
        TArray<FTransform> Globals;
        GeometryCollectionAlgo::GlobalMatrices(Rest->Transform, Rest->Parent, Globals);
        for (int32 Index = 0; Index < Globals.Num(); ++Index)
        {
            const FVector WorldLocation = RestTransformGeometryWorldCenter(
                *Rest, Globals, Index, Component->GetComponentTransform());
            const bool bSelected = AnchorBounds.IsInsideOrOn(WorldLocation);
            const bool bRigidLeaf = Rest->SimulationType[Index] ==
                    FGeometryCollection::ESimulationTypes::FST_Rigid &&
                Rest->Children[Index].IsEmpty();
            if (bRigidLeaf)
            {
                ++RigidLeafPopulation;
                SelectedRigidLeaves += bSelected ? 1 : 0;
            }
            if (Rest->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Clustered)
            {
                ++ClusterPopulation;
                SelectedClusters += bSelected ? 1 : 0;
                SelectedRoots += bSelected && Rest->Parent[Index] == INDEX_NONE ? 1 : 0;
                TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
                Sample->SetNumberField(TEXT("transform_index"), Index);
                Sample->SetNumberField(TEXT("parent_index"), Rest->Parent[Index]);
                Sample->SetNumberField(TEXT("child_count"), Rest->Children[Index].Num());
                Sample->SetArrayField(TEXT("world_location"), VectorJson(WorldLocation));
                Sample->SetBoolField(TEXT("inside_anchor_bounds"), bSelected);
                ClusterSamples.Add(MakeShared<FJsonValueObject>(Sample));
            }
        }
        for (int32 RootIndex = 0; RootIndex < Globals.Num(); ++RootIndex)
        {
            if (Rest->Parent[RootIndex] != INDEX_NONE) continue;
            int32 DescendantRigidLeafCount = 0;
            int32 SelectedDescendantRigidLeafCount = 0;
            FBox DescendantBounds(EForceInit::ForceInit);
            for (int32 Candidate = 0; Candidate < Globals.Num(); ++Candidate)
            {
                if (Rest->SimulationType[Candidate] !=
                        FGeometryCollection::ESimulationTypes::FST_Rigid ||
                    !Rest->Children[Candidate].IsEmpty()) continue;
                int32 Cursor = Candidate;
                while (Cursor != INDEX_NONE && Cursor != RootIndex &&
                    Rest->Parent.IsValidIndex(Cursor))
                {
                    Cursor = Rest->Parent[Cursor];
                }
                if (Cursor != RootIndex) continue;
                ++DescendantRigidLeafCount;
                const FVector CandidateCenter = RestTransformGeometryWorldCenter(
                    *Rest, Globals, Candidate, Component->GetComponentTransform());
                SelectedDescendantRigidLeafCount +=
                    AnchorBounds.IsInsideOrOn(CandidateCenter) ? 1 : 0;
                const int32 GeometryIndex = Rest->TransformToGeometryIndex[Candidate];
                if (Rest->BoundingBox.IsValidIndex(GeometryIndex))
                {
                    DescendantBounds += Rest->BoundingBox[GeometryIndex]
                        .TransformBy(Globals[Candidate])
                        .TransformBy(Component->GetComponentTransform());
                }
            }
            TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
            Summary->SetNumberField(TEXT("root_transform_index"), RootIndex);
            Summary->SetNumberField(TEXT("descendant_rigid_leaf_count"),
                DescendantRigidLeafCount);
            Summary->SetNumberField(TEXT("selected_descendant_rigid_leaf_count"),
                SelectedDescendantRigidLeafCount);
            Summary->SetArrayField(TEXT("descendant_bounds_min"),
                VectorJson(DescendantBounds.IsValid ? DescendantBounds.Min : FVector::ZeroVector));
            Summary->SetArrayField(TEXT("descendant_bounds_max"),
                VectorJson(DescendantBounds.IsValid ? DescendantBounds.Max : FVector::ZeroVector));
            RootBranchSummaries.Add(MakeShared<FJsonValueObject>(Summary));
        }
    }
    Data->SetNumberField(TEXT("rest_rigid_leaf_population_count"), RigidLeafPopulation);
    Data->SetNumberField(TEXT("rest_anchor_selected_rigid_leaf_count"), SelectedRigidLeaves);
    Data->SetNumberField(TEXT("rest_anchor_selected_rigid_leaf_percent"),
        RigidLeafPopulation > 0 ? 100.0 * SelectedRigidLeaves / RigidLeafPopulation : 0.0);
    Data->SetNumberField(TEXT("rest_cluster_population_count"), ClusterPopulation);
    Data->SetNumberField(TEXT("rest_anchor_selected_cluster_count"), SelectedClusters);
    Data->SetNumberField(TEXT("rest_anchor_selected_root_count"), SelectedRoots);
    Data->SetArrayField(TEXT("rest_cluster_anchor_samples"), ClusterSamples);
    Data->SetArrayField(TEXT("rest_root_branch_summaries"), RootBranchSummaries);
}


bool OutputAbsent(const FString& Path)
{
    if(!Guard()||!Work->R.Error.IsEmpty()) { return false; }Work->SDK(TEXT("admit_exact_new_output"));const FString PackageName=FPackageName::ObjectPathToPackageName(Path),Exact=PackageName+TEXT(".")+FPackageName::GetShortName(PackageName);
    const bool Free=FPackageName::IsValidLongPackageName(PackageName)&&!FindObject<UObject>(nullptr,*Exact)&&!FindPackage(nullptr,*PackageName)&&!FPackageName::DoesPackageExist(PackageName)&&!FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Exact)).IsValid();
    return Guard()&&(Free||Work->Fail(TEXT("output_collision"),TEXT("Exact output object/package/disk/registry collision; overwrite is rejected")));
}
bool DuplicateOwned(const FString& SourcePath,const FString& OutputPath)
{
    if(!OutputAbsent(OutputPath)) { return false; }Work->R.Data->SetBoolField(TEXT("SDK_duplicate_implicit_save_possible"),true);Work->R.Data->SetBoolField(TEXT("disk_state_fully_known"),false);UObject* D=nullptr;
    if(!Step(TEXT("DuplicateAsset"),[&]{D=UEditorAssetLibrary::DuplicateAsset(SourcePath,OutputPath);if(D) { Work->R.bChanged=true; }})) { return false; }
    return D&&Work->Track(D,TEXT("created_fracture_output"),nullptr,true);
}
bool ReloadOwned(UPackage* Package,FText& Error)
{
    if(!Guard()||!Live(Package)) { return false; }const FString Path=Package->GetName();TWeakObjectPtr<UPackage> OldPackage=Package;
    Work->Retire(Package,TEXT("intentional_public_package_reload"));Work->Write(TEXT("ReloadPackages"));Work->Count(TEXT("package_reload_calls"));const bool Reloaded=UPackageTools::ReloadPackages({Package},Error,EReloadPackagesInteractionMode::AssumePositive);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("package_before"),Path);J->SetBoolField(TEXT("SDK_reload_returned"),Reloaded);J->SetBoolField(TEXT("old_package_live_after"),OldPackage.IsValid());J->SetStringField(TEXT("error"),Error.ToString());Work->R.Data->SetObjectField(TEXT("actual_reload"),J);return Guard()&&Reloaded;
}
bool CleanupOwned(UGeometryCollection* Asset,const FString& OutputPath)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_path"),OutputPath);Work->R.Data->SetObjectField(TEXT("failure_cleanup"),J);
    if(!Guard()||GEditor->IsTransactionActive()||!Live(Asset)||!Asset->GetPathName().Equals(OutputPath.Contains(TEXT("."))?OutputPath:OutputPath+TEXT(".")+FPackageName::GetShortName(OutputPath),ESearchCase::CaseSensitive)) { J->SetStringField(TEXT("skipped_reason"),TEXT("Unsafe context/active Undo transaction or exact output identity is unavailable"));return false; }
    TWeakObjectPtr<UObject> Target=Asset;TWeakObjectPtr<UPackage> Package=Asset->GetPackage();const FString CapturedPath=Asset->GetPathName(),PackagePath=Package->GetName();const TWeakObjectPtr<UClass> Class=Asset->GetClass();Work->Retire(Asset,TEXT("reference_checked_failure_cleanup"));
    bool Referenced=false,Undo=false;Work->SDK(TEXT("cleanup_reference_admission"));ObjectTools::GatherObjectReferencersForDeletion(Asset,Referenced,Undo);J->SetBoolField(TEXT("referenced"),Referenced);J->SetBoolField(TEXT("Undo_referenced"),Undo);
    if(!Guard()||GEditor->IsTransactionActive()||Referenced||Undo||!Live(Target.Get())||Target->GetClass()!=Class.Get()||Target->GetPackage()!=Package.Get()||!Target->GetPathName().Equals(CapturedPath,ESearchCase::CaseSensitive)) { J->SetStringField(TEXT("skipped_reason"),TEXT("Residual output is preserved because reference/Undo/exact identity is unsafe"));return false; }
    Work->Write(TEXT("DeleteSingleObject_failure_output"));Work->Count(TEXT("cleanup_delete_calls"));const bool Deleted=ObjectTools::DeleteSingleObject(Target.Get(),true);J->SetBoolField(TEXT("DeleteSingleObject_returned"),Deleted);Work->R.bChanged|=Deleted;if(!Guard()||GEditor->IsTransactionActive()) { return false; }
    if(Deleted&&Live(Package.Get())&&Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)) { bool PRef=false,PUndo=false;Work->SDK(TEXT("cleanup_package_reference_admission"));ObjectTools::GatherObjectReferencersForDeletion(Package.Get(),PRef,PUndo);J->SetBoolField(TEXT("package_referenced"),PRef);J->SetBoolField(TEXT("package_Undo_referenced"),PUndo);if(Guard()&&!GEditor->IsTransactionActive()&&!PRef&&!PUndo&&Live(Package.Get())&&Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)) { Work->Write(TEXT("cleanup_exact_output_package"));ObjectTools::CleanupAfterSuccessfulDelete({Package.Get()},true);if(!Guard()) { return false; } } }
    Work->SDK(TEXT("observe_cleanup_residual"));const bool Disk=FPackageName::DoesPackageExist(PackagePath),Registry=FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(CapturedPath)).IsValid(),Public=Live(Target.Get())&&Target->IsAsset();J->SetBoolField(TEXT("package_file_exists_after"),Disk);J->SetBoolField(TEXT("asset_registered_after"),Registry);J->SetBoolField(TEXT("public_asset_live_after"),Public);const bool Clean=Deleted&&!Disk&&!Registry&&!Public;J->SetBoolField(TEXT("cleanup_verified"),Clean);return Guard()&&Clean;
}
bool ConstructionBoundsFrom(UFieldSystemComponent* Component,FBox& Bounds)
{
    if(!Work->Track(Component,TEXT("anchor_field_component"),Component?Component->GetOwner():nullptr)) { return false; }const auto Commands=Component->ConstructionCommands;if(Commands.TargetNames.IsEmpty()) { return true; }
    if(Commands.TargetNames.Num()!=Commands.RootNodes.Num()||Commands.TargetNames.Num()!=Commands.MetaDatas.Num()) { return Work->Fail(TEXT("field_command_schema"),TEXT("Construction command arrays are misaligned")); }
    for(const auto& N:Commands.RootNodes) { if(N&&!Work->Track(N.Get(),TEXT("construction_root_node"))) { return false; } }for(const auto& M:Commands.MetaDatas) { if(M&&!Work->Track(M.Get(),TEXT("construction_metadata"))) { return false; } }
    return Step(TEXT("BuildFieldCommand"),[&]{Bounds=Commands.BuildFieldCommand(0).BoundingBox;},false);
}

TSharedPtr<FJsonObject> HandleCreateGeometryCollection(const TSharedPtr<FJsonObject>& Params)
{
    if (GetBool(Params, TEXT("overwrite_existing"), false))
    {
        return CommandError(TEXT("overwrite_existing=true is intentionally unsupported; create a distinct temp output asset"));
    }
    const TArray<FString> SourcePaths = GetStringArray(Params, TEXT("source_static_mesh_paths"));
    const FString OutputPath = GetString(Params, TEXT("output_geometry_collection_path"));
    if (SourcePaths.IsEmpty() || !IsAllowedOutputPath(OutputPath))
    {
        return CommandError(TEXT("source_static_mesh_paths is required and output_geometry_collection_path must be under /Game/_MCP_Temp/ChaosDestructionLearning/Cinematic"));
    }
    if (!OutputAbsent(OutputPath))
    {
        return CommandError(TEXT("Output Geometry Collection already exists; original/previous assets are never overwritten"));
    }
    const TArray<TSharedPtr<FJsonValue>>* TransformValues = nullptr;
    const bool bTransformsProvided = Params->HasField(TEXT("source_transforms"));
    if (bTransformsProvided && (!Params->TryGetArrayField(TEXT("source_transforms"), TransformValues) || !TransformValues))
    {
        return CommandError(TEXT("source_transforms must be a JSON array when provided"));
    }
    const bool bHasTransforms = TransformValues && !TransformValues->IsEmpty();
    if (bHasTransforms && TransformValues->Num() != SourcePaths.Num())
    {
        return CommandError(TEXT("source_transforms must be empty or have exactly one transform per source mesh"));
    }
    TArray<FTransform> ParsedTransforms;
    if (bHasTransforms)
    {
        ParsedTransforms.Reserve(TransformValues->Num());
        for (int32 Index = 0; Index < TransformValues->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> TransformObject = (*TransformValues)[Index].IsValid()
                ? (*TransformValues)[Index]->AsObject() : nullptr;
            FTransform ParsedTransform;
            FString TransformError;
            if (!TryTransformFromJson(TransformObject, ParsedTransform, TransformError))
            {
                return CommandError(FString::Printf(
                    TEXT("source_transforms[%d] is invalid: %s"), Index, *TransformError));
            }
            const FVector Scale = ParsedTransform.GetScale3D();
            if (FMath::Abs(Scale.X) <= KINDA_SMALL_NUMBER ||
                FMath::Abs(Scale.Y) <= KINDA_SMALL_NUMBER || FMath::Abs(Scale.Z) <= KINDA_SMALL_NUMBER)
            {
                return CommandError(FString::Printf(
                    TEXT("source_transforms[%d] must have non-zero scale"), Index));
            }
            ParsedTransforms.Add(ParsedTransform);
        }
    }
    const FString SourceHashBefore = HashStaticMeshSources(SourcePaths);if(!Guard()||!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }
    const FString PackageName = FPackageName::ObjectPathToPackageName(OutputPath);
    const FString ObjectName = FPackageName::GetShortName(PackageName);
    if(!OutputAbsent(OutputPath)) { return CommandError(Work->R.Error); }Work->Write(TEXT("CreatePackage"));UPackage* Package = CreatePackage(*PackageName);if(!Work->Track(Package,TEXT("new_geometry_package"),nullptr,true)) { return CommandError(Work->R.Error); }
    Work->Write(TEXT("NewGeometryCollection"));UGeometryCollection* Asset = NewObject<UGeometryCollection>(Package, *ObjectName, RF_Public | RF_Standalone | RF_Transactional);
    if(Asset&&!Work->Track(Asset,TEXT("created_geometry_collection"),Package,true)) { return CommandError(Work->R.Error); }
    if (!Asset)
    {
        return CommandError(TEXT("Failed to allocate Geometry Collection"));
    }
    TArray<bool> SourceDirtyBefore;
    TArray<FString> SourcePackageNames;
    TArray<FString> SourcePackageHashesBefore;
    int32 AppendedCount = 0;
    FBox ExpectedInstanceBounds(EForceInit::ForceInit);
    for (int32 Index = 0; Index < SourcePaths.Num(); ++Index)
    {
        UStaticMesh* Mesh = Work->Load<UStaticMesh>(SourcePaths[Index],TEXT("load_source_mesh"));
        if (!Mesh)
        {
            return CommandError(FString::Printf(TEXT("Static Mesh not found: %s"), *SourcePaths[Index]));
        }
        SourceDirtyBefore.Add(Mesh->GetOutermost()->IsDirty());
        SourcePackageNames.Add(Mesh->GetOutermost()->GetName());
        SourcePackageHashesBefore.Add(HashObjectPackageBytes(Mesh));
        TArray<UMaterialInterface*> Materials;
        for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetStaticMaterials().Num(); ++MaterialIndex)
        {
            UMaterialInterface* Material=Mesh->GetMaterial(MaterialIndex);if(Material&&!Work->Track(Material,TEXT("source_mesh_material"))) { return CommandError(Work->R.Error); }Materials.Add(Material);
        }
        const FTransform Transform = bHasTransforms ? ParsedTransforms[Index] : FTransform::Identity;
        const FVector Scale = Transform.GetScale3D();
        if (Transform.ContainsNaN() || FMath::Abs(Scale.X) <= KINDA_SMALL_NUMBER ||
            FMath::Abs(Scale.Y) <= KINDA_SMALL_NUMBER || FMath::Abs(Scale.Z) <= KINDA_SMALL_NUMBER)
        {
            return CommandError(FString::Printf(TEXT("source_transforms[%d] must be finite with non-zero scale"), Index));
        }
        ExpectedInstanceBounds += Mesh->GetBounds().GetBox().TransformBy(Transform);
        bool bAppended=false;if(!Step(TEXT("AppendStaticMesh"),[&]{bAppended=FGeometryCollectionEngineConversion::AppendStaticMesh(
            Mesh, Materials, Transform, Asset, GetBool(Params, TEXT("material_reindex"), true), true, false, false);})) { return CommandError(Work->R.Error); }
        AppendedCount += bAppended ? 1 : 0;
    }
    if (AppendedCount != SourcePaths.Num())
    {
        return CommandError(TEXT("One or more source Static Meshes failed Geometry Collection conversion"));
    }
    const FString RequestedProxyPath = GetString(Params, TEXT("root_mesh_proxy_static_mesh_path"));
    UStaticMesh* RequestedProxyMesh = RequestedProxyPath.IsEmpty()
        ? nullptr : Work->Load<UStaticMesh>(RequestedProxyPath,TEXT("load_requested_proxy"));
    if (!RequestedProxyPath.IsEmpty() && !RequestedProxyMesh)
    {
        return CommandError(TEXT("root_mesh_proxy_static_mesh_path did not resolve to a Static Mesh"));
    }
    if (Asset->GetGeometryCollection().IsValid() &&
        FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(Asset->GetGeometryCollection().Get()))
    {
        FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(
            Asset->GetGeometryCollection().Get(), TEXT("BuildingRoot"), true);
    }
    Asset->EnableNanite = false;
    if (RequestedProxyMesh && Asset->GetGeometryCollection().IsValid())
    {
        const FBoxSphereBounds CollectionBounds = Asset->GetGeometryCollection()->GetBoundingBox();
        const FBoxSphereBounds ProxyBounds = RequestedProxyMesh->GetBounds();
        const FVector SafeProxyExtent(
            FMath::Max(ProxyBounds.BoxExtent.X, 0.001), FMath::Max(ProxyBounds.BoxExtent.Y, 0.001),
            FMath::Max(ProxyBounds.BoxExtent.Z, 0.001));
        const FVector Scale = CollectionBounds.BoxExtent / SafeProxyExtent;
        Asset->RootProxyData.ProxyMeshes = {RequestedProxyMesh};
        Asset->RootProxyData.MeshTransforms = {
            FTransform3f(FTransform(FQuat::Identity, CollectionBounds.Origin - ProxyBounds.Origin * Scale, Scale))};
        Asset->CustomRendererType = UGeometryCollectionRootProxyRenderer::StaticClass();
    }
    FinalizeCollection(Asset);if(!Guard()||!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }
    if(!Step(TEXT("AssetCreated"),[&]{FAssetRegistryModule::AssetCreated(Asset);})) { return CommandError(Work->R.Error); }
    const bool bSaved = SaveAsset(Asset);if(!Guard()) { return CommandError(Work->R.Error); }
    const FString SourceHashAfter = HashStaticMeshSources(SourcePaths);
    bool bSourcesUnchanged = true;
    TArray<TSharedPtr<FJsonValue>> SourcePackageAudits;
    for (int32 Index = 0; Index < SourcePaths.Num(); ++Index)
    {
        UStaticMesh* Mesh = Work->Load<UStaticMesh>(SourcePaths[Index],TEXT("load_source_mesh"));
        const bool bDirtyAfter = Mesh && Mesh->GetOutermost()->IsDirty();
        const FString HashAfter = HashObjectPackageBytes(Mesh);
        const bool bPackageUnchanged = Mesh && SourceDirtyBefore[Index] == bDirtyAfter &&
            !SourcePackageHashesBefore[Index].IsEmpty() && SourcePackageHashesBefore[Index] == HashAfter;
        bSourcesUnchanged &= bPackageUnchanged;
        TSharedPtr<FJsonObject> Audit = MakeShared<FJsonObject>();
        Audit->SetStringField(TEXT("package_name"), SourcePackageNames[Index]);
        Audit->SetBoolField(TEXT("dirty_before"), SourceDirtyBefore[Index]);
        Audit->SetBoolField(TEXT("dirty_after"), bDirtyAfter);
        Audit->SetStringField(TEXT("sha256_before"), SourcePackageHashesBefore[Index]);
        Audit->SetStringField(TEXT("sha256_after"), HashAfter);
        Audit->SetBoolField(TEXT("unchanged_gate"), bPackageUnchanged);
        SourcePackageAudits.Add(MakeShared<FJsonValueObject>(Audit));
    }
    const FBox OutputBounds = Asset->GetGeometryCollection().IsValid()
        ? Asset->GetGeometryCollection()->GetBoundingBox().GetBox() : FBox(EForceInit::ForceInit);
    const double BoundsScale = FMath::Max(ExpectedInstanceBounds.GetExtent().GetMax(), 1.0);
    const double BoundsRelativeError = ExpectedInstanceBounds.IsValid && OutputBounds.IsValid
        ? FMath::Max(
            (ExpectedInstanceBounds.GetCenter() - OutputBounds.GetCenter()).GetAbsMax(),
            (ExpectedInstanceBounds.GetExtent() - OutputBounds.GetExtent()).GetAbsMax()) / BoundsScale
        : TNumericLimits<double>::Max();
    TSharedPtr<FJsonObject> Data = Success(TEXT("create_geometry_collection_from_mesh"));
    Data->SetStringField(TEXT("geometry_collection_path"), Asset->GetPathName());
    Data->SetNumberField(TEXT("source_mesh_count"), SourcePaths.Num());
    Data->SetNumberField(TEXT("appended_source_mesh_count"), AppendedCount);
    Data->SetArrayField(TEXT("source_package_names"), [&SourcePackageNames]()
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const FString& Value : SourcePackageNames) Values.Add(MakeShared<FJsonValueString>(Value));
        return Values;
    }());
    Data->SetBoolField(TEXT("source_packages_unchanged"), bSourcesUnchanged);
    Data->SetBoolField(TEXT("source_packages_clean_before_after"), !SourceDirtyBefore.Contains(true));
    Data->SetArrayField(TEXT("source_package_audits"), SourcePackageAudits);
    Data->SetStringField(TEXT("source_sha256_before"), SourceHashBefore);
    Data->SetStringField(TEXT("source_sha256_after"), SourceHashAfter);
    Data->SetBoolField(TEXT("source_sha256_unchanged"), !SourceHashBefore.IsEmpty() && SourceHashBefore == SourceHashAfter);
    Data->SetStringField(TEXT("output_transform_sha256"), HashCollectionTransforms(Asset));
    Data->SetBoolField(TEXT("source_transforms_applied"), bHasTransforms);
    Data->SetNumberField(TEXT("assembled_bounds_relative_error"), BoundsRelativeError);
    Data->SetBoolField(TEXT("assembled_bounds_one_percent_gate"), BoundsRelativeError <= 0.01);
    Data->SetBoolField(TEXT("nanite_used"), false);
    Data->SetBoolField(TEXT("root_mesh_proxy_configured"), RequestedProxyMesh != nullptr);
    AddCollectionMetrics(Data, Asset);
    Data->SetStringField(TEXT("assertion_status"), bSaved && bSourcesUnchanged &&
        !SourceHashBefore.IsEmpty() && SourceHashBefore == SourceHashAfter && BoundsRelativeError <= 0.01
            ? TEXT("PASS") : TEXT("FAIL"));
    return FLegacy::CreateSuccessResponse(Data);
}

TSharedPtr<FJsonObject> HandleConfigureFracture(const TSharedPtr<FJsonObject>& Params)
{
    const FString Operation = GetString(Params, TEXT("operation"), TEXT("configure"));
    if (Operation == TEXT("inspect_saved"))
    {
        const FString AssetPath = GetString(
            Params, TEXT("fractured_geometry_collection_path"));
        UGeometryCollection* Asset = Work->Load<UGeometryCollection>(AssetPath,TEXT("load_existing_collection"));
        const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection =
            Asset ? Asset->GetGeometryCollection() : nullptr;
        if(Collection.IsValid()&&!Schema(*Collection)) { return CommandError(Work->R.Error); }
        if (!Asset || !Collection.IsValid())
        {
            return CommandError(TEXT("Saved fractured Geometry Collection not found"));
        }
        int32 CollisionShapeCount = 0;
        for (const FGeometryCollectionSizeSpecificData& SizeData : Asset->SizeSpecificData)
        {
            CollisionShapeCount += SizeData.CollisionShapes.Num();
        }
        const int32 ConvexElementCount = Collection->NumElements(FTransformCollection::ConvexGroup);
        int32 ExternalCollisionCount = 0;
        if (const TManagedArray<Chaos::FImplicitObjectPtr>* ExternalCollisions =
            Collection->FindAttribute<Chaos::FImplicitObjectPtr>(
                FGeometryCollection::ExternalCollisionsAttribute,
                FGeometryCollection::TransformGroup))
        {
            for (const Chaos::FImplicitObjectPtr& Implicit : *ExternalCollisions)
            {
                ExternalCollisionCount += Implicit.IsValid() ? 1 : 0;
            }
        }
        TSharedPtr<FJsonObject> Data = Success(TEXT("configure_building_fracture"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("fractured_geometry_collection_path"), Asset->GetPathName());
        Data->SetBoolField(TEXT("simulation_data_dirty"), Asset->IsSimulationDataDirty());
        Data->SetBoolField(TEXT("simulation_data_cooked_readback_gate"),
            !Asset->IsSimulationDataDirty() && CollisionShapeCount > 0 && ConvexElementCount > 0);
        Data->SetNumberField(TEXT("collision_shape_count"), CollisionShapeCount);
        Data->SetNumberField(TEXT("convex_element_count"), ConvexElementCount);
        Data->SetNumberField(TEXT("external_collision_implicit_count"), ExternalCollisionCount);
        AddCollectionMetrics(Data, Asset);
        Data->SetStringField(TEXT("assertion_status"),
            !Asset->IsSimulationDataDirty() && CollisionShapeCount > 0 && ConvexElementCount > 0
                ? TEXT("PASS") : TEXT("FAIL"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (Operation == TEXT("configure_root_mesh_proxy"))
    {
        const FString AssetPath = GetString(Params, TEXT("fractured_geometry_collection_path"));
        const FString ProxyPath = GetString(Params, TEXT("root_mesh_proxy_static_mesh_path"));
        if (!IsAllowedOutputPath(AssetPath))
        {
            return CommandError(TEXT("fractured_geometry_collection_path must use the ChaosCinematic temp root"));
        }
        UGeometryCollection* Asset = Work->Load<UGeometryCollection>(AssetPath,TEXT("load_existing_collection"));
        UStaticMesh* ProxyMesh = ProxyPath.IsEmpty() ? nullptr : Work->Load<UStaticMesh>(ProxyPath,TEXT("load_root_proxy"));
        if (!Asset || !Asset->GetGeometryCollection().IsValid() || !ProxyMesh)
        {
            return CommandError(TEXT("Saved Geometry Collection and root proxy Static Mesh are required"));
        }
        if(!Work->Modify(Asset,TEXT("modify_geometry_collection"))) { return CommandError(Work->R.Error); }
        const FBoxSphereBounds CollectionBounds = Asset->GetGeometryCollection()->GetBoundingBox();
        const FBoxSphereBounds ProxyBounds = ProxyMesh->GetBounds();
        const FVector SafeProxyExtent(
            FMath::Max(ProxyBounds.BoxExtent.X, 0.001),
            FMath::Max(ProxyBounds.BoxExtent.Y, 0.001),
            FMath::Max(ProxyBounds.BoxExtent.Z, 0.001));
        const FVector Scale = CollectionBounds.BoxExtent / SafeProxyExtent;
        const FTransform FitTransform(
            FQuat::Identity, CollectionBounds.Origin - ProxyBounds.Origin * Scale, Scale);
        Asset->EnableNanite = false;
        Asset->RootProxyData.ProxyMeshes = {ProxyMesh};
        Asset->RootProxyData.MeshTransforms = {FTransform3f(FitTransform)};
        Asset->CustomRendererType = UGeometryCollectionRootProxyRenderer::StaticClass();
        Asset->MarkPackageDirty();
        const bool bSaved = SaveAsset(Asset);if(!Guard()) { return CommandError(Work->R.Error); }
        UGeometryCollection* ReloadedAsset = Work->Load<UGeometryCollection>(AssetPath,TEXT("load_existing_collection"));
        const bool bProxyReadback = ReloadedAsset && !ReloadedAsset->EnableNanite &&
            ReloadedAsset->RootProxyData.ProxyMeshes.Num() == 1 &&
            ReloadedAsset->RootProxyData.MeshTransforms.Num() == 1 &&
            ReloadedAsset->RootProxyData.ProxyMeshes[0] == ProxyMesh &&
            ReloadedAsset->CustomRendererType == UGeometryCollectionRootProxyRenderer::StaticClass();
        TSharedPtr<FJsonObject> Data = Success(TEXT("configure_building_fracture"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("fractured_geometry_collection_path"),
            ReloadedAsset ? ReloadedAsset->GetPathName() : FString());
        Data->SetStringField(TEXT("root_mesh_proxy_static_mesh_path"), ProxyMesh->GetPathName());
        Data->SetBoolField(TEXT("nanite_used"), ReloadedAsset && ReloadedAsset->EnableNanite);
        Data->SetBoolField(TEXT("root_mesh_proxy_configured"), bProxyReadback);
        Data->SetBoolField(TEXT("saved"), bSaved);
        Data->SetStringField(TEXT("assertion_status"), bSaved && bProxyReadback ? TEXT("PASS") : TEXT("FAIL"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (Operation != TEXT("configure"))
    {
        return CommandError(TEXT("operation must be configure, inspect_saved, or configure_root_mesh_proxy"));
    }
    if (GetBool(Params, TEXT("overwrite_existing"), false))
    {
        return CommandError(TEXT("overwrite_existing=true is intentionally unsupported; fracture into a distinct temp output asset"));
    }
    const FString SourcePath = GetString(Params, TEXT("source_geometry_collection_path"));
    const FString OutputPath = GetString(Params, TEXT("output_geometry_collection_path"));
    if (SourcePath.IsEmpty() || SourcePath == OutputPath || !IsAllowedOutputPath(OutputPath))
    {
        return CommandError(TEXT("Distinct source/output Geometry Collection paths are required; output must use the ChaosCinematic temp root"));
    }
    if (GetString(Params, TEXT("fracture_method"), TEXT("uniform_voronoi")) != TEXT("uniform_voronoi"))
    {
        return CommandError(TEXT("fracture_method currently supports only uniform_voronoi"));
    }
    if (GetString(Params, TEXT("collision_implicit_type"), TEXT("convex")) != TEXT("convex"))
    {
        return CommandError(TEXT("collision_implicit_type currently supports only convex"));
    }
    if (!OutputAbsent(OutputPath))
    {
        return CommandError(TEXT("Output Geometry Collection already exists; overwrite is intentionally rejected"));
    }
    UGeometryCollection* Source = Work->Load<UGeometryCollection>(SourcePath,TEXT("load_fracture_source"));
    if (!Source)
    {
        return CommandError(TEXT("Source Geometry Collection not found"));
    }
    const bool bSourceDirtyBefore = Source->GetOutermost()->IsDirty();
    if(!Source->GetGeometryCollection().IsValid()||!Schema(*Source->GetGeometryCollection())) { return CommandError(TEXT("Source Geometry Collection has no valid managed data")); }
    const FString SourceTransformHashBefore = HashCollectionTransforms(Source);
    if (!DuplicateOwned(SourcePath, OutputPath))
    {
        return CommandError(TEXT("Failed to duplicate source Geometry Collection"));
    }
    UGeometryCollection* Asset = Work->Load<UGeometryCollection>(OutputPath,TEXT("load_fracture_output"));
    if (!Asset || !Asset->GetGeometryCollection().IsValid())
    {
        return CommandError(TEXT("Duplicated Geometry Collection could not be loaded"));
    }
    if(!Work->Modify(Asset,TEXT("modify_geometry_collection"))) { return CommandError(Work->R.Error); }
    FGeometryCollection& Collection = *Asset->GetGeometryCollection();if(!Schema(Collection)) { return CommandError(Work->R.Error); }
    const int32 PreRigidCount = Algo::CountIf(Collection.SimulationType, [](int32 Type)
    {
        return Type == FGeometryCollection::ESimulationTypes::FST_Rigid;
    });
    FDataflowTransformSelection Selection;
    Selection.Initialize(Collection.NumElements(FGeometryCollection::TransformGroup), false);
    for (int32 Index = 0; Index < Collection.NumElements(FGeometryCollection::TransformGroup); ++Index)
    {
        if (Collection.SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid)
        {
            Selection.SetSelected(Index);
        }
    }
    FUniformFractureSettings Settings;
    Settings.Transform = FTransform::Identity;
    Settings.MinVoronoiSites = FMath::Clamp(CastI32(GetNumber(Params, TEXT("site_count"), 64),TEXT("site_count")), 2, 1000);
    Settings.MaxVoronoiSites = Settings.MinVoronoiSites;
    Settings.InternalMaterialID = -1;
    Settings.RandomSeed = CastI32(GetNumber(Params, TEXT("random_seed"), 1337),TEXT("random_seed"));
    Settings.ChanceToFracture = 1.0f;
    Settings.GroupFracture = true;
    Settings.SplitIslands = true;
    Settings.Grout = 0.0f;
    Settings.NoiseSettings = FNoiseSettings();
    Settings.AddSamplesForCollision = true;
    Settings.CollisionSampleSpacing = 50.0f;
    if(!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }int32 FracturedGeometryCount=0;if(!Step(TEXT("UniformFracture_output"),[&]{FracturedGeometryCount=FFractureEngineFracturing::UniformFracture(Collection,Selection,Settings);})) { return CommandError(Work->R.Error); }if(!Schema(Collection)) { return CommandError(Work->R.Error); }
    const FString FractureStageTransformHash = HashCollectionTransforms(Asset);
    const FString FractureStageManagedGeometryHash =
        HashCollectionManagedGeometry(Asset);
    const int32 ClusterLevels = FMath::Clamp(CastI32(GetNumber(Params, TEXT("cluster_levels"), 2),TEXT("cluster_levels")), 1, 4);
    const int32 ClusterSize = FMath::Clamp(CastI32(GetNumber(Params, TEXT("cluster_size"), 8),TEXT("cluster_size")), 2, 64);
    const bool bClusterAllRoots = GetBool(Params, TEXT("cluster_all_roots"), true);
    const double TinyRelativeThreshold = FMath::Clamp(
        GetNumber(Params, TEXT("tiny_relative_threshold"), 0.04), 0.0, 1.0);
    const bool bMergeTinyGeometry = GetBool(Params, TEXT("merge_tiny_geometry"), true);
    if(!Guard()||!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }Work->Write(TEXT("NewDeterminismGeometryCollection"));UGeometryCollection* DeterminismAsset = NewObject<UGeometryCollection>(GetTransientPackage());if(!Work->Track(DeterminismAsset,TEXT("transient_same_seed_oracle"),GetTransientPackage(),true)) { return CommandError(Work->R.Error); }
    TArray<UMaterialInterface*> DeterminismMaterials;
    for (UMaterialInterface* Material : Source->Materials) { if(Material&&!Work->Track(Material,TEXT("same_seed_source_material"))) { return CommandError(Work->R.Error); }DeterminismMaterials.Add(Material); }
    if(!Step(TEXT("ResetFrom_same_seed_source"),[&]{DeterminismAsset->ResetFrom(*Source->GetGeometryCollection(),DeterminismMaterials,false);})) { return CommandError(Work->R.Error); }for(auto& T:Work->Targets) { if(T.Weak.Get()==DeterminismAsset) { T.CollectionIdentity=DeterminismAsset->GetGeometryCollection();T.HasCollectionIdentity=true; } }if(!Schema(*DeterminismAsset->GetGeometryCollection())) { return CommandError(Work->R.Error); }
    FDataflowTransformSelection DeterminismSelection;
    DeterminismSelection.Initialize(
        DeterminismAsset->GetGeometryCollection()->NumElements(FGeometryCollection::TransformGroup), false);
    for (int32 Index = 0; Index < DeterminismAsset->GetGeometryCollection()->NumElements(FGeometryCollection::TransformGroup); ++Index)
    {
        if (DeterminismAsset->GetGeometryCollection()->SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid)
            DeterminismSelection.SetSelected(Index);
    }
    if(!Step(TEXT("UniformFracture_same_seed_oracle"),[&]{FFractureEngineFracturing::UniformFracture(
        *DeterminismAsset->GetGeometryCollection(), DeterminismSelection, Settings);})) { return CommandError(Work->R.Error); }for(auto& T:Work->Targets) { if(T.Weak.Get()==DeterminismAsset) { T.CollectionIdentity=DeterminismAsset->GetGeometryCollection();T.HasCollectionIdentity=true; } }if(!Schema(*DeterminismAsset->GetGeometryCollection())) { return CommandError(Work->R.Error); }
    const FString DeterminismStageHash = HashCollectionTransforms(DeterminismAsset);
    const FString DeterminismStageManagedGeometryHash =
        HashCollectionManagedGeometry(DeterminismAsset);
    // Uniform fracture retains each assembled source mesh as a level-one
    // cluster. Collapse that authoring-only layer before building the spatial
    // hierarchy; otherwise one foundation anchor can make an entire wall
    // segment kinematic. This mutates only the distinct output duplicate.
    if(!Step(TEXT("collapse_output_hierarchy"),[&]{FGeometryCollectionClusteringUtility::CollapseLevelHierarchy(1,&Collection);})) { return CommandError(Work->R.Error); }
    if(!Step(TEXT("collapse_oracle_hierarchy"),[&]{FGeometryCollectionClusteringUtility::CollapseLevelHierarchy(
        1,DeterminismAsset->GetGeometryCollection().Get());})) { return CommandError(Work->R.Error); }
    const int32 RecenteredRigidGeometryCount =
        RecenterRigidGeometryTransforms(Collection);
    const int32 DeterminismRecenteredRigidGeometryCount =
        RecenterRigidGeometryTransforms(*DeterminismAsset->GetGeometryCollection());
    if(!Step(TEXT("BuildClusterHierarchy_output"),[&]{BuildClusterHierarchy(Collection,ClusterLevels,ClusterSize,bClusterAllRoots);})) { return CommandError(Work->R.Error); }
    if(!Step(TEXT("BuildClusterHierarchy_oracle"),[&]{BuildClusterHierarchy(
        *DeterminismAsset->GetGeometryCollection(),ClusterLevels,ClusterSize,bClusterAllRoots);})) { return CommandError(Work->R.Error); }
    if (!bClusterAllRoots)
    {
        // Build the spatial layers while the assembled source root still
        // preserves source-space transforms. Only then detach the generated
        // top-level structural clusters and remove the empty synthetic root.
        const auto DetachSyntheticRoot = [](FGeometryCollection& Target)
        {
            TArray<int32> Roots;
            FGeometryCollectionClusteringUtility::GetRootBones(&Target, Roots);
            if (Roots.Num() != 1 || !Target.Children.IsValidIndex(Roots[0])) return;
            TArray<FTransform> Globals;
            GeometryCollectionAlgo::GlobalMatrices(Target.Transform, Target.Parent, Globals);
            const TArray<int32> RootChildren = Target.Children[Roots[0]].Array();
            for (const int32 Child : RootChildren)
            {
                if (!Globals.IsValidIndex(Child)) continue;
                Target.Transform[Child] = FTransform3f(Globals[Child]);
                Target.Parent[Child] = INDEX_NONE;
            }
            Target.Children[Roots[0]].Reset();
            FGeometryCollectionClusteringUtility::RemoveDanglingClusters(&Target);
        };
        DetachSyntheticRoot(Collection);
        DetachSyntheticRoot(*DeterminismAsset->GetGeometryCollection());
    }
    const int32 PreTinyCount = CountTinyGeometry(Asset, TinyRelativeThreshold);
    const int32 PreTinyRigidCount = Algo::CountIf(Collection.SimulationType, [](int32 Type)
    {
        return Type == FGeometryCollection::ESimulationTypes::FST_Rigid;
    });
    const FBox PreTinyBounds = Collection.GetBoundingBox().GetBox();
    if (bMergeTinyGeometry)
    {
        if(!Step(TEXT("FixTinyGeo_output"),[&]{MergeTinyGeometry(Collection,TinyRelativeThreshold);})) { return CommandError(Work->R.Error); }
        if(!Step(TEXT("FixTinyGeo_oracle"),[&]{MergeTinyGeometry(*DeterminismAsset->GetGeometryCollection(),TinyRelativeThreshold);})) { return CommandError(Work->R.Error); }
    }
    TArray<FTransform> LeafGlobalsBeforeClusterRecenter;
    GeometryCollectionAlgo::GlobalMatrices(
        Collection.Transform, Collection.Parent, LeafGlobalsBeforeClusterRecenter);
    FClusterRecenterSnapshot ClusterRecenterSnapshot;
    FClusterRecenterSnapshot DeterminismClusterRecenterSnapshot;
    if (!bMergeTinyGeometry)
    {
        ClusterRecenterSnapshot.Capture(Collection);
        DeterminismClusterRecenterSnapshot.Capture(
            *DeterminismAsset->GetGeometryCollection());
    }
    int32 RecenteredClusterCount =
        RecenterClusterTransforms(Collection);
    int32 DeterminismRecenteredClusterCount =
        RecenterClusterTransforms(*DeterminismAsset->GetGeometryCollection());
    TArray<FTransform> LeafGlobalsAfterClusterRecenter;
    GeometryCollectionAlgo::GlobalMatrices(
        Collection.Transform, Collection.Parent, LeafGlobalsAfterClusterRecenter);
    double MaximumLeafTranslationDriftCm = 0.0;
    double MaximumLeafRotationDriftDegrees = 0.0;
    double MaximumLeafScaleDrift = 0.0;
    const auto MeasureLeafGlobalDrift = [&Collection](
        const TArray<FTransform>& Before,
        const TArray<FTransform>& After,
        double& OutTranslation,
        double& OutRotationDegrees,
        double& OutScale)
    {
        if (Before.Num() != After.Num() ||
            Before.Num() != Collection.NumElements(FGeometryCollection::TransformGroup))
        {
            OutTranslation = TNumericLimits<double>::Max();
            OutRotationDegrees = TNumericLimits<double>::Max();
            OutScale = TNumericLimits<double>::Max();
            return;
        }
        for (int32 Index = 0; Index < Before.Num(); ++Index)
        {
            if (Collection.SimulationType[Index] !=
                    FGeometryCollection::ESimulationTypes::FST_Rigid ||
                !Collection.Children[Index].IsEmpty())
            {
                continue;
            }
            OutTranslation = FMath::Max(
                OutTranslation,
                static_cast<double>((Before[Index].GetLocation() -
                    After[Index].GetLocation()).Length()));
            OutRotationDegrees = FMath::Max(
                OutRotationDegrees,
                FMath::RadiansToDegrees(static_cast<double>(
                    Before[Index].GetRotation().AngularDistance(
                        After[Index].GetRotation()))));
            OutScale = FMath::Max(
                OutScale,
                static_cast<double>((Before[Index].GetScale3D() -
                    After[Index].GetScale3D()).GetAbsMax()));
        }
    };
    MeasureLeafGlobalDrift(
        LeafGlobalsBeforeClusterRecenter,
        LeafGlobalsAfterClusterRecenter,
        MaximumLeafTranslationDriftCm,
        MaximumLeafRotationDriftDegrees,
        MaximumLeafScaleDrift);
    const bool bSameSeedFractureStage = !FractureStageTransformHash.IsEmpty() &&
        FractureStageTransformHash == DeterminismStageHash &&
        !FractureStageManagedGeometryHash.IsEmpty() &&
        FractureStageManagedGeometryHash == DeterminismStageManagedGeometryHash;
    int32 PostTinyCount = CountTinyGeometry(Asset, TinyRelativeThreshold);
    int32 PostTinyRigidCount = Algo::CountIf(Collection.SimulationType, [](int32 Type)
    {
        return Type == FGeometryCollection::ESimulationTypes::FST_Rigid;
    });
    FBox PostTinyBounds = Collection.GetBoundingBox().GetBox();
    const double PreTinyVolume = PreTinyBounds.IsValid ? PreTinyBounds.GetVolume() : 0.0;
    double TinyBoundsRelativeError = PreTinyBounds.IsValid && PostTinyBounds.IsValid
        ? FMath::Max(
            (PreTinyBounds.GetCenter() - PostTinyBounds.GetCenter()).GetAbsMax(),
            (PreTinyBounds.GetExtent() - PostTinyBounds.GetExtent()).GetAbsMax()) /
            FMath::Max(PreTinyBounds.GetExtent().GetMax(), 1.0)
        : TNumericLimits<double>::Max();
    double TinyVolumeRelativeError = PreTinyVolume > UE_DOUBLE_SMALL_NUMBER
        ? FMath::Abs(PostTinyBounds.GetVolume() - PreTinyVolume) / PreTinyVolume : 0.0;
    bool bClusterRecenterFallbackUsed = false;
    if (!bMergeTinyGeometry &&
        (TinyBoundsRelativeError > 0.01 || TinyVolumeRelativeError > 0.01 ||
            MaximumLeafTranslationDriftCm > 0.01 ||
            MaximumLeafRotationDriftDegrees > 0.01 ||
            MaximumLeafScaleDrift > 0.0001) &&
        ClusterRecenterSnapshot.CanRestore(Collection) &&
        DeterminismClusterRecenterSnapshot.CanRestore(
            *DeterminismAsset->GetGeometryCollection()))
    {
        const bool bPrimaryStateRestored =
            ClusterRecenterSnapshot.Restore(Collection);
        const bool bDeterminismStateRestored =
            DeterminismClusterRecenterSnapshot.Restore(
                *DeterminismAsset->GetGeometryCollection());
        check(bPrimaryStateRestored && bDeterminismStateRestored);
        RecenteredClusterCount = 0;
        DeterminismRecenteredClusterCount = 0;
        PostTinyCount = CountTinyGeometry(Asset, TinyRelativeThreshold);
        PostTinyRigidCount = Algo::CountIf(
            Collection.SimulationType,
            [](int32 Type)
            {
                return Type == FGeometryCollection::ESimulationTypes::FST_Rigid;
            });
        PostTinyBounds = Collection.GetBoundingBox().GetBox();
        TinyBoundsRelativeError = PreTinyBounds.IsValid && PostTinyBounds.IsValid
            ? FMath::Max(
                (PreTinyBounds.GetCenter() - PostTinyBounds.GetCenter()).GetAbsMax(),
                (PreTinyBounds.GetExtent() - PostTinyBounds.GetExtent()).GetAbsMax()) /
                FMath::Max(PreTinyBounds.GetExtent().GetMax(), 1.0)
            : TNumericLimits<double>::Max();
        TinyVolumeRelativeError = PreTinyVolume > UE_DOUBLE_SMALL_NUMBER
            ? FMath::Abs(PostTinyBounds.GetVolume() - PreTinyVolume) / PreTinyVolume
            : 0.0;
        GeometryCollectionAlgo::GlobalMatrices(
            Collection.Transform, Collection.Parent, LeafGlobalsAfterClusterRecenter);
        MaximumLeafTranslationDriftCm = 0.0;
        MaximumLeafRotationDriftDegrees = 0.0;
        MaximumLeafScaleDrift = 0.0;
        MeasureLeafGlobalDrift(
            LeafGlobalsBeforeClusterRecenter,
            LeafGlobalsAfterClusterRecenter,
            MaximumLeafTranslationDriftCm,
            MaximumLeafRotationDriftDegrees,
            MaximumLeafScaleDrift);
        bClusterRecenterFallbackUsed = true;
    }
    const bool bTinyReductionPass = bMergeTinyGeometry
        ? (PreTinyCount == 0 ? PostTinyCount == 0 : PostTinyCount < PreTinyCount)
        : (PreTinyCount == PostTinyCount && PreTinyRigidCount == PostTinyRigidCount);
    Asset->EnableClustering = true;
    Asset->EnableNanite = false;
    Asset->MaxClusterLevel = 10;
    DeterminismAsset->EnableClustering = true;
    DeterminismAsset->EnableNanite = false;
    DeterminismAsset->MaxClusterLevel = 10;
    Work->Write(TEXT("configure_collection_collision_removal_proxy"));const TArray<double> DamageThresholds = GetNumberArray(Params, TEXT("damage_thresholds"), {500000.0, 250000.0, 100000.0});
    Asset->DamageThreshold.Reset();
    DeterminismAsset->DamageThreshold.Reset();
    for (double Threshold : DamageThresholds)
    {
        const float Value = CastFloat(Threshold,TEXT("damage_thresholds"));
        if(!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }Asset->DamageThreshold.Add(Value);
        DeterminismAsset->DamageThreshold.Add(Value);
    }
    if(!Guard()||!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }
    const bool bRemoveOnSleep = GetBool(Params, TEXT("remove_on_sleep"), true);
    const bool bRemoveOnBreak = GetBool(Params, TEXT("remove_on_break"), true);
    Asset->bRemoveOnMaxSleep = bRemoveOnSleep;
    DeterminismAsset->bRemoveOnMaxSleep = bRemoveOnSleep;
    const float SleepTime = FMath::Max(0.0f, CastFloat(GetNumber(Params, TEXT("sleep_time_seconds"), 5.0),TEXT("sleep_time_seconds")));
    const float RemovalDuration = FMath::Max(0.0f, CastFloat(GetNumber(Params, TEXT("removal_duration_seconds"), 1.0),TEXT("removal_duration_seconds")));
    const float BreakTimer = FMath::Max(0.0f, CastFloat(GetNumber(Params, TEXT("break_timer_seconds"), 1.0),TEXT("break_timer_seconds")));
    if(!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }Asset->MaximumSleepTime = FVector2D(SleepTime, SleepTime);
    Asset->RemovalDuration = FVector2D(RemovalDuration, RemovalDuration);
    DeterminismAsset->MaximumSleepTime = FVector2D(SleepTime, SleepTime);
    DeterminismAsset->RemovalDuration = FVector2D(RemovalDuration, RemovalDuration);
    GeometryCollection::Facades::FCollectionRemoveOnBreakFacade RemoveOnBreakFacade(Collection);
    RemoveOnBreakFacade.DefineSchema();
    if (bRemoveOnBreak)
    {
        TArray<int32> RemovalIndices;
        for (int32 Index = 0; Index < Collection.NumElements(FGeometryCollection::TransformGroup); ++Index)
        {
            if (Collection.SimulationType[Index] == FGeometryCollection::ESimulationTypes::FST_Rigid)
            {
                RemovalIndices.Add(Index);
            }
        }
        RemoveOnBreakFacade.SetFromIndexArray(
            RemovalIndices,
            GeometryCollection::Facades::FRemoveOnBreakData(
                true, FVector2f(BreakTimer, BreakTimer), false, FVector2f(RemovalDuration, RemovalDuration)));
    }
    if (Asset->SizeSpecificData.IsEmpty())
    {
        Asset->SizeSpecificData.Add(UGeometryCollection::GeometryCollectionSizeSpecificDataDefaults());
    }
    const float Reduction = FMath::Clamp(CastFloat(GetNumber(Params, TEXT("collision_object_reduction_percentage"), 20.0),TEXT("collision_object_reduction_percentage")), 0.0f, 100.0f);
    const int32 MaximumParticles = FMath::Max(0, CastI32(GetNumber(Params, TEXT("maximum_collision_particles"), 60),TEXT("maximum_collision_particles")));
    if(!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }int32 CollisionShapeCount = 0;
    for (FGeometryCollectionSizeSpecificData& SizeData : Asset->SizeSpecificData)
    {
        for (FGeometryCollectionCollisionTypeData& Collision : SizeData.CollisionShapes)
        {
            ++CollisionShapeCount;
            Collision.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Convex;
            Collision.CollisionObjectReductionPercentage = Reduction;
            Collision.CollisionParticles.MaximumCollisionParticles = MaximumParticles;
        }
    }
    // FinalizeCollection creates collision-derived managed attributes.  The
    // same-seed oracle must use the exact same collision settings as the
    // persisted candidate or their final hashes compare different inputs.
    DeterminismAsset->SizeSpecificData = Asset->SizeSpecificData;

    const FString ProxyPath = GetString(Params, TEXT("root_mesh_proxy_static_mesh_path"));
    UStaticMesh* ProxyMesh = ProxyPath.IsEmpty() ? nullptr : Work->Load<UStaticMesh>(ProxyPath,TEXT("load_root_proxy"));
    double ProxyCoverageError = 0.0;
    if (ProxyMesh)
    {
        const FBoxSphereBounds CollectionBounds = Collection.GetBoundingBox();
        const FBoxSphereBounds ProxyBounds = ProxyMesh->GetBounds();
        const FVector SafeProxyExtent(
            FMath::Max(ProxyBounds.BoxExtent.X, 0.001),
            FMath::Max(ProxyBounds.BoxExtent.Y, 0.001),
            FMath::Max(ProxyBounds.BoxExtent.Z, 0.001));
        const FVector Scale = CollectionBounds.BoxExtent / SafeProxyExtent;
        const FTransform FitTransform(FQuat::Identity, CollectionBounds.Origin - ProxyBounds.Origin * Scale, Scale);
        Asset->RootProxyData.ProxyMeshes = {ProxyMesh};
        Asset->RootProxyData.MeshTransforms = {FTransform3f(FitTransform)};
        Asset->CustomRendererType = UGeometryCollectionRootProxyRenderer::StaticClass();
        const FVector FittedExtent = ProxyBounds.BoxExtent * Scale;
        ProxyCoverageError = (FittedExtent - CollectionBounds.BoxExtent).GetAbsMax();
    }
    FinalizeCollection(Asset);if(!Guard()||!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }
    FinalizeCollection(DeterminismAsset);if(!Guard()||!Work->R.Error.IsEmpty()) { return CommandError(Work->R.Error); }
    const FString DeterminismFinalTransformHash = HashCollectionTransforms(DeterminismAsset);
    const FString DeterminismFinalTopologyHash = HashCollectionTopology(DeterminismAsset);
    const FString DeterminismFinalManagedGeometryHash =
        HashCollectionManagedGeometry(DeterminismAsset);
    const FString DeterminismFinalStableGeometryHash =
        HashCollectionManagedGeometry(DeterminismAsset, false);
    const FString DeterminismFinalFragmentSpatialSignature =
        HashCollectionFragmentSpatialSignature(DeterminismAsset);
    const FString PreSaveManagedGeometryHash = HashCollectionManagedGeometry(Asset);
    const bool bSourceUnchanged = Source->GetOutermost()->IsDirty() == bSourceDirtyBefore;
    const FString SourceTransformHashAfter = HashCollectionTransforms(Source);
    const FString SavedAssetObjectPath = Asset->GetPathName();
    UPackage* SavedPackage = Asset->GetOutermost();
    const bool bSaved = SaveAsset(Asset);if(!Guard()) { return CommandError(Work->R.Error); }
    FText PackageReloadError;
    const bool bPackageReloaded = bSaved && SavedPackage && Guard() && ReloadOwned(SavedPackage,PackageReloadError);
    if(!Guard()) { return CommandError(Work->R.Error); }
    UGeometryCollection* ReloadedAsset = bPackageReloaded
        ? Work->Load<UGeometryCollection>(OutputPath,TEXT("load_fracture_output")) : nullptr;
    const FString FinalTransformHash = HashCollectionTransforms(ReloadedAsset);
    const FString FinalTopologyHash = HashCollectionTopology(ReloadedAsset);
    const FString FinalManagedGeometryHash =
        HashCollectionManagedGeometry(ReloadedAsset);
    const FString FinalStableGeometryHash =
        HashCollectionManagedGeometry(ReloadedAsset, false);
    const FString FinalFragmentSpatialSignature =
        HashCollectionFragmentSpatialSignature(ReloadedAsset);
    const FString PersistedPackageSha256 = HashObjectPackageBytes(ReloadedAsset);
    const bool bPersistedReloadReadback = bSaved && bPackageReloaded && ReloadedAsset &&
        ReloadedAsset->GetGeometryCollection().IsValid() &&
        !PersistedPackageSha256.IsEmpty();
    const bool bPersistedManagedGeometryReadback = bPersistedReloadReadback &&
        !PreSaveManagedGeometryHash.IsEmpty() &&
        PreSaveManagedGeometryHash == FinalManagedGeometryHash;
    const bool bStrictSameSeedFinalDeterministic = bSameSeedFractureStage &&
        bPersistedManagedGeometryReadback &&
        !FinalTransformHash.IsEmpty() && FinalTransformHash == DeterminismFinalTransformHash &&
        !FinalTopologyHash.IsEmpty() && FinalTopologyHash == DeterminismFinalTopologyHash &&
        !FinalStableGeometryHash.IsEmpty() &&
        FinalStableGeometryHash == DeterminismFinalStableGeometryHash;
    // UE 5.6's UniformFracture can permute internal faces for some imported
    // FBX sections despite retaining the same seed, transforms, hierarchy and
    // fragment volumes. Keep the strict path as the default; a caller must
    // explicitly opt in to this narrower spatial-equivalence fallback.
    const bool bAllowNonDeterministicGeometryOrder = GetBool(
        Params, TEXT("allow_non_deterministic_geometry_order"), false);
    const bool bSpatiallyEquivalentSameSeed =
        bPersistedManagedGeometryReadback &&
        !FinalTransformHash.IsEmpty() && FinalTransformHash == DeterminismFinalTransformHash &&
        !FinalTopologyHash.IsEmpty() && FinalTopologyHash == DeterminismFinalTopologyHash &&
        !FinalFragmentSpatialSignature.IsEmpty() &&
        FinalFragmentSpatialSignature == DeterminismFinalFragmentSpatialSignature;
    const bool bSameSeedFinalDeterministic = bStrictSameSeedFinalDeterministic ||
        (bAllowNonDeterministicGeometryOrder && bSpatiallyEquivalentSameSeed);
    TSharedPtr<FJsonObject> Data = Success(TEXT("configure_building_fracture"));
    Data->SetStringField(TEXT("fractured_geometry_collection_path"), SavedAssetObjectPath);
    Data->SetNumberField(TEXT("pre_rigid_transform_count"), PreRigidCount);
    Data->SetNumberField(TEXT("fractured_geometry_count"), FracturedGeometryCount);
    Data->SetStringField(TEXT("fracture_stage_transform_sha256"), FractureStageTransformHash);
    Data->SetStringField(TEXT("determinism_stage_transform_sha256"), DeterminismStageHash);
    Data->SetStringField(TEXT("fracture_stage_managed_geometry_sha256"),
        FractureStageManagedGeometryHash);
    Data->SetStringField(TEXT("determinism_stage_managed_geometry_sha256"),
        DeterminismStageManagedGeometryHash);
    Data->SetBoolField(TEXT("same_seed_fracture_stage_pass"), bSameSeedFractureStage);
    Data->SetBoolField(TEXT("source_mesh_cluster_layer_collapsed"), true);
    Data->SetNumberField(TEXT("recentered_rigid_geometry_count"),
        RecenteredRigidGeometryCount);
    Data->SetBoolField(TEXT("rigid_particle_spatial_recenter_gate"),
        RecenteredRigidGeometryCount > 0 &&
        RecenteredRigidGeometryCount == DeterminismRecenteredRigidGeometryCount);
    Data->SetNumberField(TEXT("recentered_cluster_count"), RecenteredClusterCount);
    Data->SetBoolField(TEXT("cluster_recenter_bounds_fallback_used"),
        bClusterRecenterFallbackUsed);
    Data->SetNumberField(TEXT("cluster_recenter_max_leaf_translation_drift_cm"),
        MaximumLeafTranslationDriftCm);
    Data->SetNumberField(TEXT("cluster_recenter_max_leaf_rotation_drift_degrees"),
        MaximumLeafRotationDriftDegrees);
    Data->SetNumberField(TEXT("cluster_recenter_max_leaf_scale_drift"),
        MaximumLeafScaleDrift);
    Data->SetBoolField(TEXT("cluster_particle_spatial_recenter_gate"),
        RecenteredClusterCount > 0 &&
        RecenteredClusterCount == DeterminismRecenteredClusterCount);
    Data->SetBoolField(TEXT("cluster_all_roots"), bClusterAllRoots);
    Data->SetNumberField(TEXT("final_root_count"), ReloadedAsset && ReloadedAsset->GetGeometryCollection().IsValid()
        ? Algo::CountIf(ReloadedAsset->GetGeometryCollection()->Parent,
            [](const int32 Parent) { return Parent == INDEX_NONE; }) : 0);
    Data->SetStringField(TEXT("hierarchy_spatial_sort"), TEXT("rest_space_z_x_y"));
    Data->SetStringField(TEXT("final_transform_sha256"), FinalTransformHash);
    Data->SetStringField(TEXT("determinism_final_transform_sha256"), DeterminismFinalTransformHash);
    Data->SetStringField(TEXT("hierarchy_topology_sha256"), FinalTopologyHash);
    Data->SetStringField(TEXT("determinism_hierarchy_topology_sha256"), DeterminismFinalTopologyHash);
    Data->SetStringField(TEXT("final_managed_geometry_sha256"),
        FinalManagedGeometryHash);
    Data->SetStringField(TEXT("determinism_final_managed_geometry_sha256"),
        DeterminismFinalManagedGeometryHash);
    Data->SetStringField(TEXT("final_stable_geometry_sha256"),
        FinalStableGeometryHash);
    Data->SetStringField(TEXT("determinism_final_stable_geometry_sha256"),
        DeterminismFinalStableGeometryHash);
    Data->SetStringField(TEXT("final_fragment_spatial_signature_sha256"),
        FinalFragmentSpatialSignature);
    Data->SetStringField(TEXT("determinism_final_fragment_spatial_signature_sha256"),
        DeterminismFinalFragmentSpatialSignature);
    Data->SetBoolField(TEXT("strict_same_seed_determinism_pass"),
        bStrictSameSeedFinalDeterministic);
    Data->SetBoolField(TEXT("spatially_equivalent_same_seed_pass"),
        bSpatiallyEquivalentSameSeed);
    Data->SetBoolField(TEXT("allow_non_deterministic_geometry_order"),
        bAllowNonDeterministicGeometryOrder);
    Data->SetBoolField(TEXT("same_seed_determinism_pass"), bSameSeedFinalDeterministic);
    Data->SetNumberField(TEXT("tiny_relative_threshold"), TinyRelativeThreshold);
    Data->SetNumberField(TEXT("pre_tiny_candidate_count"), PreTinyCount);
    Data->SetNumberField(TEXT("post_tiny_candidate_count"), PostTinyCount);
    Data->SetNumberField(TEXT("pre_tiny_rigid_count"), PreTinyRigidCount);
    Data->SetNumberField(TEXT("post_tiny_rigid_count"), PostTinyRigidCount);
    Data->SetBoolField(TEXT("tiny_merge_requested"), bMergeTinyGeometry);
    Data->SetBoolField(TEXT("tiny_candidate_reduction_gate"), bTinyReductionPass);
    Data->SetNumberField(TEXT("tiny_bounds_relative_error"), TinyBoundsRelativeError);
    Data->SetNumberField(TEXT("tiny_volume_relative_error"), TinyVolumeRelativeError);
    Data->SetStringField(TEXT("tiny_residual_reason"), !bMergeTinyGeometry
        ? TEXT("tiny geometry merge disabled by request")
        : (PostTinyCount == 0
            ? TEXT("none")
            : TEXT("remaining candidates have no valid merge neighbor under FixTinyGeo constraints")));
    Data->SetNumberField(TEXT("collision_object_reduction_percentage"), Reduction);
    Data->SetNumberField(TEXT("maximum_collision_particles"), MaximumParticles);
    Data->SetNumberField(TEXT("collision_shape_count"), CollisionShapeCount);
    Data->SetBoolField(TEXT("collision_simplification_gate"),
        CollisionShapeCount > 0 && Reduction >= 20.0f && MaximumParticles <= 60);
    Data->SetStringField(TEXT("collision_implicit_type"), TEXT("convex"));
    Data->SetBoolField(TEXT("remove_on_sleep"), bRemoveOnSleep);
    Data->SetBoolField(TEXT("remove_on_break"), bRemoveOnBreak);
    Data->SetNumberField(TEXT("sleep_time_seconds"), SleepTime);
    Data->SetNumberField(TEXT("removal_duration_seconds"), RemovalDuration);
    Data->SetNumberField(TEXT("break_timer_seconds"), BreakTimer);
    Data->SetBoolField(TEXT("root_mesh_proxy_configured"), ProxyMesh != nullptr);
    Data->SetNumberField(TEXT("root_mesh_proxy_bounds_coverage_error_cm"), ProxyCoverageError);
    Data->SetBoolField(TEXT("root_mesh_proxy_bounds_coverage_pass"), !ProxyMesh || ProxyCoverageError <= 0.01);
    Data->SetBoolField(TEXT("nanite_used"),
        ReloadedAsset ? ReloadedAsset->EnableNanite : false);
    Data->SetBoolField(TEXT("source_package_unchanged"), bSourceUnchanged);
    Data->SetStringField(TEXT("source_transform_sha256_before"), SourceTransformHashBefore);
    Data->SetStringField(TEXT("source_transform_sha256_after"), SourceTransformHashAfter);
    Data->SetBoolField(TEXT("source_transform_sha256_unchanged"),
        !SourceTransformHashBefore.IsEmpty() && SourceTransformHashBefore == SourceTransformHashAfter);
    Data->SetStringField(TEXT("output_transform_sha256"), FinalTransformHash);
    Data->SetBoolField(TEXT("saved"), bSaved);
    Data->SetBoolField(TEXT("persisted_reload_readback_gate"), bPersistedReloadReadback);
    Data->SetBoolField(TEXT("persisted_package_reload_gate"), bPackageReloaded);
    Data->SetStringField(TEXT("persisted_package_reload_error"),
        PackageReloadError.ToString());
    Data->SetStringField(TEXT("pre_save_managed_geometry_sha256"),
        PreSaveManagedGeometryHash);
    Data->SetBoolField(TEXT("persisted_managed_geometry_readback_gate"),
        bPersistedManagedGeometryReadback);
    Data->SetStringField(TEXT("persisted_package_sha256"), PersistedPackageSha256);
    AddCollectionMetrics(Data, ReloadedAsset);
    const FCollectionIntegrityMetrics Integrity = ReloadedAsset && ReloadedAsset->GetGeometryCollection().IsValid()
        ? AnalyzeCollectionIntegrity(*ReloadedAsset->GetGeometryCollection()) : FCollectionIntegrityMetrics();
    const bool bCollectionIntegrity = Integrity.InvalidParentCount == 0 && Integrity.CycleCount == 0 &&
        Integrity.ParentChildReciprocityErrors == 0 && Integrity.InvalidTriangleCount == 0 &&
        Integrity.NonManifoldEdgeCount == 0 && Integrity.InvalidTransformToGeometryCount == 0 &&
        Integrity.InvalidGeometryRangeCount == 0;
    const bool bPass = bSaved && bPersistedReloadReadback && bSourceUnchanged && bSameSeedFinalDeterministic &&
        !bClusterRecenterFallbackUsed &&
        MaximumLeafTranslationDriftCm <= 0.01 &&
        MaximumLeafRotationDriftDegrees <= 0.01 && MaximumLeafScaleDrift <= 0.0001 &&
        FracturedGeometryCount > 0 && bTinyReductionPass && TinyBoundsRelativeError <= 0.01 &&
        TinyVolumeRelativeError <= 0.01 && CollisionShapeCount > 0 && Reduction >= 20.0f &&
        MaximumParticles <= 60 && ReloadedAsset && !ReloadedAsset->EnableNanite &&
        bCollectionIntegrity;
    // DuplicateAsset may persist a package before the full authoring gate is
    // known. Never leave a failed candidate behind as a promotable asset.
    const bool bFailureOutputDeleted = !bPass && bPackageReloaded && ReloadedAsset && CleanupOwned(ReloadedAsset,OutputPath);
    if(!bPass&&!bPackageReloaded) { Work->R.Data->SetStringField(TEXT("cleanup_skipped_reason"),TEXT("Reload did not establish the owned replacement identity; residual output is preserved without another load or delete")); }
    Data->SetBoolField(TEXT("saved"), bPass && bSaved);
    Data->SetBoolField(TEXT("failure_output_deleted"), bFailureOutputDeleted);
    Data->SetStringField(TEXT("assertion_status"), bPass ? TEXT("PASS") : TEXT("FAIL"));
    return FLegacy::CreateSuccessResponse(Data);
}
TSharedPtr<FJsonObject> HandleConfigureAnchor(const TSharedPtr<FJsonObject>& Params)
{
    const FString Operation = GetString(Params, TEXT("operation"), TEXT("configure"));
    // Legacy Python default 1 was Sleeping in both UE5.6 and UE5.8, while this
    // handler has always required Kinematic. Do not alias explicit 1 to 2.
    // Validate configure before Engine load/replacement/spawn; inspect_saved does
    // not consume this configure-only field and retains its existing behavior.
    const int32 FieldMagnitude = Operation == TEXT("configure") ? CastI32(GetNumber(
        Params, TEXT("field_magnitude"), static_cast<int32>(Chaos::EObjectStateType::Kinematic)),TEXT("field_magnitude")) : static_cast<int32>(Chaos::EObjectStateType::Kinematic);
    if (Operation == TEXT("configure") && FieldMagnitude != static_cast<int32>(Chaos::EObjectStateType::Kinematic))
    {
        return CommandError(TEXT("Anchor field_magnitude must equal the native Kinematic object-state value"));
    }
    UWorld* World = ResolveEditorWorld();
    const FString GeometryActorName = GetString(Params, TEXT("geometry_collection_actor_name"));
    AActor* GeometryActor = FindActor<AActor>(World, GeometryActorName);
    UGeometryCollectionComponent* Component = ResolveGeometryComponent(GeometryActor);
    if (!World || !Component)
    {
        return CommandError(TEXT("Geometry Collection actor not found in the current editor/play world"));
    }
    Work->World=World;Work->HasWorld=true;if(!Work->Track(World,TEXT("anchor_world"))||!Work->Track(GeometryActor,TEXT("anchor_geometry_actor"))||!Work->Track(Component,TEXT("anchor_geometry_component"),GeometryActor)) { return CommandError(Work->R.Error); }
    const FString FieldName = GetString(Params, TEXT("field_actor_name"), TEXT("Chaos_AnchorField"));
    AFieldSystemActor* FieldActor = FindActor<AFieldSystemActor>(World, FieldName);if(FieldActor&&!Work->Track(FieldActor,TEXT("anchor_field_actor"))) { return CommandError(Work->R.Error); }
    if (Operation == TEXT("inspect_saved"))
    {
        UFieldSystemComponent* SavedComponent = FieldActor ? FieldActor->GetFieldSystemComponent() : nullptr;
        const UFieldSystem* SavedFieldSystem = SavedComponent ? SavedComponent->GetFieldSystem() : nullptr;
        if(SavedComponent&&!Work->Track(SavedComponent,TEXT("inspected_anchor_component"),FieldActor)) { return CommandError(Work->R.Error); }
        if(SavedComponent&&SavedComponent->FieldSystem&&!Work->Track(SavedComponent->FieldSystem.Get(),TEXT("inspected_serialized_field_system"))) { return CommandError(Work->R.Error); }
        const bool bMapOwnedFieldSystem = SavedFieldSystem && SavedFieldSystem->IsIn(FieldActor) &&
            SavedFieldSystem->GetOutermost() == World->GetOutermost();
        const bool bInitializationFieldIdentity = FieldActor && Component->InitializationFields.Contains(FieldActor) &&
            FieldActor->GetLevel() == GeometryActor->GetLevel();
        const int32 ConstructionCommandCount = SavedComponent
            ? SavedComponent->ConstructionCommands.GetNumCommands() : 0;
        FBox ConstructionBounds(EForceInit::ForceInit);
        if (SavedComponent && ConstructionCommandCount > 0)
        {
            if(!ConstructionBoundsFrom(SavedComponent,ConstructionBounds)||SavedComponent->GetFieldSystem()!=SavedFieldSystem) { return CommandError(Work->R.Error.IsEmpty()?TEXT("Serialized anchor FieldSystem changed during evaluation"):Work->R.Error); }
        }
        const int32 SerializedFieldCommandCount = SavedFieldSystem ? SavedFieldSystem->Commands.Num() : 0;
        const FString SavedMapSha256 = HashObjectPackageBytes(World);
        const bool bEngineAnchorBlueprint = FieldActor &&
            FieldActor->GetClass()->GetPathName() == EngineAnchorBlueprintClassPath;
        const bool bReadbackGate = bEngineAnchorBlueprint && bInitializationFieldIdentity &&
            ConstructionCommandCount > 0 && !SavedMapSha256.IsEmpty();
        TSharedPtr<FJsonObject> Data = Success(TEXT("configure_anchor_field"));
        Data->SetStringField(TEXT("operation"), Operation);
        Data->SetStringField(TEXT("field_actor_path"), FieldActor ? FieldActor->GetPathName() : FString());
        Data->SetStringField(TEXT("geometry_collection_component_path"), Component->GetPathName());
        Data->SetStringField(TEXT("field_system_path"), SavedFieldSystem ? SavedFieldSystem->GetPathName() : FString());
        Data->SetNumberField(TEXT("initialization_field_count"), Component->InitializationFields.Num());
        Data->SetNumberField(TEXT("construction_command_count"), ConstructionCommandCount);
        Data->SetArrayField(TEXT("construction_bounds_min"), VectorJson(ConstructionBounds.Min));
        Data->SetArrayField(TEXT("construction_bounds_max"), VectorJson(ConstructionBounds.Max));
        Data->SetArrayField(TEXT("construction_bounds_extent"), VectorJson(ConstructionBounds.GetExtent()));
        Data->SetNumberField(TEXT("serialized_field_command_count"), SerializedFieldCommandCount);
        Data->SetBoolField(TEXT("map_owned_field_system"), bMapOwnedFieldSystem);
        Data->SetStringField(TEXT("anchor_actor_class_path"),
            FieldActor ? FieldActor->GetClass()->GetPathName() : FString());
        Data->SetBoolField(TEXT("engine_anchor_blueprint"), bEngineAnchorBlueprint);
        Data->SetBoolField(TEXT("initialization_field_identity_readback_gate"), bInitializationFieldIdentity);
        Data->SetStringField(TEXT("saved_map_sha256"), SavedMapSha256);
        AppendAnchorRestSelectionEvidence(Data, Component, ConstructionBounds);
        Data->SetBoolField(TEXT("saved_anchor_readback_gate"), bReadbackGate);
        Data->SetStringField(TEXT("assertion_status"), bReadbackGate ? TEXT("PASS") : TEXT("FAIL"));
        return FLegacy::CreateSuccessResponse(Data);
    }
    if (Operation != TEXT("configure"))
    {
        return CommandError(TEXT("operation must be configure or inspect_saved"));
    }
    UClass* EngineAnchorClass=nullptr;if(!Step(TEXT("load_engine_anchor_class"),[&]{EngineAnchorClass=LoadClass<AFieldSystemActor>(nullptr,EngineAnchorBlueprintClassPath);},false)) { return CommandError(Work->R.Error); }if(EngineAnchorClass&&!Work->Track(EngineAnchorClass,TEXT("engine_anchor_class"))) { return CommandError(Work->R.Error); }
    if (!EngineAnchorClass)
    {
        return CommandError(TEXT("Engine FS_AnchorField_Generic Blueprint class could not be loaded"));
    }
    if (FieldActor && FieldActor->GetClass() != EngineAnchorClass)
    {
        if(!Work->Modify(Component,TEXT("modify_geometry_initialization_fields"))) { return CommandError(Work->R.Error); }
        Work->Write(TEXT("remove_previous_anchor_initialization_field"));Component->InitializationFields.Remove(FieldActor);
        Work->Retire(FieldActor,TEXT("replace_non_engine_anchor_actor"));bool Destroyed=false;if(!Step(TEXT("destroy_previous_anchor_actor"),[&]{Destroyed=World->DestroyActor(FieldActor);})) { return CommandError(Work->R.Error); }if(!Destroyed) { return CommandError(TEXT("Previous non-engine anchor actor could not be destroyed")); }Work->R.bChanged=true;
        FieldActor = nullptr;
    }
    if (!FieldActor)
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Name = MakeUniqueObjectName(World, EngineAnchorClass, *FieldName);
        if(!Step(TEXT("spawn_engine_anchor_actor"),[&]{FieldActor=World->SpawnActor<AFieldSystemActor>(EngineAnchorClass,FTransform::Identity,SpawnParameters);if(FieldActor) { Work->R.bChanged=true; }})) { return CommandError(Work->R.Error); }if(FieldActor&&!Work->Track(FieldActor,TEXT("created_anchor_actor"),nullptr,true)) { return CommandError(Work->R.Error); }
        if (FieldActor)
        {
            if(!Step(TEXT("set_anchor_actor_label"),[&]{FieldActor->SetActorLabel(FieldName);})) { return CommandError(Work->R.Error); }
        }
    }
    if (!FieldActor || !FieldActor->GetFieldSystemComponent())
    {
        return CommandError(TEXT("Failed to create Field System actor"));
    }
    FVector Center = Component->Bounds.Origin;
    FVector Extent(100.0);
    TryVector(Params, TEXT("box_center"), Center);
    TryVector(Params, TEXT("box_extent"), Extent);
    Extent.X = FMath::Max(FMath::Abs(Extent.X), 1.0);
    Extent.Y = FMath::Max(FMath::Abs(Extent.Y), 1.0);
    Extent.Z = FMath::Max(FMath::Abs(Extent.Z), 1.0);

    UFieldSystemComponent* FieldComponent = FieldActor->GetFieldSystemComponent();
    if(!Work->Modify(FieldActor,TEXT("modify_anchor_actor"))||!Work->Modify(FieldComponent,TEXT("modify_anchor_component"))) { return CommandError(Work->R.Error); }
    if(!Step(TEXT("set_anchor_location"),[&]{FieldActor->SetActorLocation(Center);})) { return CommandError(Work->R.Error); }
    // FS_AnchorField_Generic uses a 100 cm full-size (50 cm half-extent)
    // construction box and its actor
    // transform. Keeping the Blueprint construction script as the source of
    // the command makes the field reconstruct on every level load without an
    // UnrealMCP class or manually persisted runtime command.
    if(!Step(TEXT("set_anchor_scale"),[&]{FieldActor->SetActorScale3D(Extent/50.0);})) { return CommandError(Work->R.Error); }
    TWeakObjectPtr<UFieldSystemComponent> PreConstructionComponent=FieldComponent;Work->Retire(FieldComponent,TEXT("construction_script_may_replace_component"));if(!Step(TEXT("RerunConstructionScripts"),[&]{FieldActor->RerunConstructionScripts();})) { return CommandError(Work->R.Error); }
    FieldComponent=FieldActor->GetFieldSystemComponent();Work->R.bChanged|=PreConstructionComponent.Get()!=FieldComponent;if(!Work->Track(FieldComponent,TEXT("reacquired_anchor_component"),FieldActor)) { return CommandError(Work->R.Error); }
    const UFieldSystem* SerializedFieldSystem = FieldComponent ? FieldComponent->GetFieldSystem() : nullptr;if(FieldComponent&&FieldComponent->FieldSystem&&!Work->Track(FieldComponent->FieldSystem.Get(),TEXT("serialized_anchor_field_system"))) { return CommandError(Work->R.Error); }
    if (!Component->InitializationFields.Contains(FieldActor))
    {
        if(!Work->Modify(Component,TEXT("modify_geometry_initialization_fields"))) { return CommandError(Work->R.Error); }
        Work->Write(TEXT("bind_anchor_initialization_field"));Component->InitializationFields.Add(FieldActor);
    }
    const bool bSaveLevel = GetBool(Params, TEXT("save_level"), true);
    const bool bLevelSaved = !bSaveLevel || SaveAsset(World);if(!Guard()) { return CommandError(Work->R.Error); }
    const int32 ConstructionCommandCount = FieldComponent->ConstructionCommands.GetNumCommands();
    FBox ConstructionBounds(EForceInit::ForceInit);
    if (ConstructionCommandCount > 0)
    {
        if(!ConstructionBoundsFrom(FieldComponent,ConstructionBounds)||FieldComponent->GetFieldSystem()!=SerializedFieldSystem) { return CommandError(Work->R.Error.IsEmpty()?TEXT("Serialized anchor FieldSystem changed during evaluation"):Work->R.Error); }
    }
    const int32 SerializedFieldCommandCount = SerializedFieldSystem ? SerializedFieldSystem->Commands.Num() : 0;
    const bool bMapOwnedFieldSystem = SerializedFieldSystem && SerializedFieldSystem->IsIn(FieldActor) &&
        SerializedFieldSystem->GetOutermost() == World->GetOutermost();
    const bool bEngineAnchorBlueprint = FieldActor->GetClass()->GetPathName() == EngineAnchorBlueprintClassPath;
    const bool bInitializationFieldIdentity = Component->InitializationFields.Contains(FieldActor) &&
        FieldActor->GetLevel() == GeometryActor->GetLevel();
    const double ConstructionCenterError = ConstructionBounds.IsValid
        ? (ConstructionBounds.GetCenter() - Center).GetAbsMax() : TNumericLimits<double>::Max();
    const double ConstructionExtentError = ConstructionBounds.IsValid
        ? (ConstructionBounds.GetExtent() - Extent).GetAbsMax() : TNumericLimits<double>::Max();
    const FString SavedMapSha256 = bLevelSaved ? HashObjectPackageBytes(World) : FString();
    const bool bReadbackGate = bLevelSaved && bEngineAnchorBlueprint && bInitializationFieldIdentity &&
        ConstructionCommandCount > 0 && ConstructionCenterError <= 0.1 &&
        ConstructionExtentError <= 0.1 && !SavedMapSha256.IsEmpty();
    TSharedPtr<FJsonObject> Data = Success(TEXT("configure_anchor_field"));
    Data->SetStringField(TEXT("operation"), Operation);
    Data->SetStringField(TEXT("field_actor_path"), FieldActor->GetPathName());
    Data->SetStringField(TEXT("geometry_collection_component_path"), Component->GetPathName());
    Data->SetStringField(TEXT("field_system_path"), SerializedFieldSystem ? SerializedFieldSystem->GetPathName() : FString());
    Data->SetNumberField(TEXT("initialization_field_count"), Component->InitializationFields.Num());
    Data->SetNumberField(TEXT("serialized_construction_command_count"), FieldComponent->GetConstructionFields().Num());
    Data->SetNumberField(TEXT("construction_command_count"), ConstructionCommandCount);
    Data->SetArrayField(TEXT("construction_bounds_min"), VectorJson(ConstructionBounds.Min));
    Data->SetArrayField(TEXT("construction_bounds_max"), VectorJson(ConstructionBounds.Max));
    Data->SetArrayField(TEXT("construction_bounds_extent"), VectorJson(ConstructionBounds.GetExtent()));
    Data->SetNumberField(TEXT("construction_bounds_center_error_cm"), ConstructionCenterError);
    Data->SetNumberField(TEXT("construction_bounds_extent_error_cm"), ConstructionExtentError);
    Data->SetNumberField(TEXT("serialized_field_command_count"), SerializedFieldCommandCount);
    Data->SetBoolField(TEXT("map_owned_field_system"), bMapOwnedFieldSystem);
    Data->SetStringField(TEXT("anchor_actor_class_path"), FieldActor->GetClass()->GetPathName());
    Data->SetBoolField(TEXT("engine_anchor_blueprint"), bEngineAnchorBlueprint);
    Data->SetBoolField(TEXT("initialization_field_identity_readback_gate"), bInitializationFieldIdentity);
    Data->SetStringField(TEXT("saved_map_sha256"), SavedMapSha256);
    AppendAnchorRestSelectionEvidence(Data, Component, ConstructionBounds);
    Data->SetBoolField(TEXT("saved_anchor_readback_gate"), bReadbackGate);
    Data->SetNumberField(TEXT("field_magnitude"), FieldMagnitude);
    Data->SetArrayField(TEXT("box_center"), VectorJson(Center));
    Data->SetArrayField(TEXT("box_extent"), VectorJson(Extent));
    Data->SetBoolField(TEXT("anchor_configured"), bReadbackGate);
    Data->SetBoolField(TEXT("level_saved"), bLevelSaved);
    Data->SetStringField(TEXT("assertion_status"), bReadbackGate ? TEXT("PASS") : TEXT("FAIL"));
    return FLegacy::CreateSuccessResponse(Data);
}

}
FString Create(const FString& ParamsJson)
{
    FWork W(TEXT("create_geometry_collection_from_mesh"));TSharedPtr<FJsonObject> P;if(!Parse(ParamsJson,P)) { W.Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return W.Finish(nullptr); }if(Running) { W.Fail(TEXT("reentrant_request"),TEXT("Authoring API is already executing"));return W.Finish(nullptr); }TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,&W);if(!W.Guard()) { return W.Finish(nullptr); }return W.Finish(HandleCreateGeometryCollection(P));
}
FString ConfigureFracture(const FString& ParamsJson)
{
    FWork W(TEXT("configure_building_fracture"));TSharedPtr<FJsonObject> P;if(!Parse(ParamsJson,P)) { W.Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return W.Finish(nullptr); }if(Running) { W.Fail(TEXT("reentrant_request"),TEXT("Authoring API is already executing"));return W.Finish(nullptr); }TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,&W);if(!W.Guard()) { return W.Finish(nullptr); }W.Observer=GetString(P,TEXT("operation"),TEXT("configure"))==TEXT("inspect_saved");return W.Finish(HandleConfigureFracture(P));
}
FString ConfigureAnchor(const FString& ParamsJson)
{
    FWork W(TEXT("configure_anchor_field"));TSharedPtr<FJsonObject> P;if(!Parse(ParamsJson,P)) { W.Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return W.Finish(nullptr); }if(Running) { W.Fail(TEXT("reentrant_request"),TEXT("Authoring API is already executing"));return W.Finish(nullptr); }TGuardValue<bool> Busy(Running,true);TGuardValue<FWork*> Scope(Work,&W);if(!W.Guard()) { return W.Finish(nullptr); }W.Observer=GetString(P,TEXT("operation"),TEXT("configure"))==TEXT("inspect_saved");return W.Finish(HandleConfigureAnchor(P));
}
}
