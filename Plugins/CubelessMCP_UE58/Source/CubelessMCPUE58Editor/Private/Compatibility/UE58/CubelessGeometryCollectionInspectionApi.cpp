#include "Compatibility/UE58/CubelessGeometryCollectionInspectionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionEngineRemoval.h"
#include "GeometryCollection/Facades/CollectionRemoveOnBreakFacade.h"
#include "GeometryCollectionProxyData.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::GeometryCollectionInspection
{
namespace
{
using FResult = Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* LearningRoot = TEXT("/Game/_MCP_Temp/ChaosDestructionLearning");
const FName LearningActorTag(TEXT("MCP_ChaosLearning"));
constexpr int32 MaxJsonChars=1048576, MaxJsonDepth=64, MaxJsonSlots=262144;
struct FExactKeyFuncs : BaseKeyFuncs<FString,FString,false>
{
    static KeyInitType GetSetKey(ElementInitType K) { return K; }
    static bool Matches(KeyInitType A,KeyInitType B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(KeyInitType K) { return FCrc::StrCrc32(*K); }
};
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>MaxJsonChars) { return false; }
    for(TCHAR C:Text) { if(C==0) { return false; } }
    auto Reader=TJsonReaderFactory<>::Create(Text); EJsonNotation Token; int32 Slots=0;
    TArray<bool> Objects; TArray<TSet<FString,FExactKeyFuncs>> Keys;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>MaxJsonSlots) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        { const FString Key(Reader->GetIdentifier()); if(Keys.Last().Contains(Key)) { return false; } Keys.Last().Add(Key); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>MaxJsonDepth) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
        if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber())) { return false; }
    }
    return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out)&&Out.IsValid();
}
FString JsonText(const TSharedPtr<FJsonObject>& Object)
{ FString Text;FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return Text; }
bool FiniteJson(const TSharedPtr<FJsonValue>& Value)
{
    if(!Value.IsValid()) { return false; }
    if(Value->Type==EJson::Number) { return FMath::IsFinite(Value->AsNumber()); }
    if(Value->Type==EJson::Array) { for(const auto& Child:Value->AsArray()) { if(!FiniteJson(Child)) { return false; } } }
    if(Value->Type==EJson::Object) { for(const auto& Pair:Value->AsObject()->Values) { if(!FiniteJson(Pair.Value)) { return false; } } }
    return true;
}
struct FTracked
{
    TStrongObjectPtr<const UObject> Hold{nullptr}; TWeakObjectPtr<UClass> Class; TWeakObjectPtr<UObject> Outer;
    TStrongObjectPtr<UPackage> Package{nullptr}; TWeakObjectPtr<UWorld> ObjectWorld;
    FString Path,PackagePath,Role; bool HadWorld=false,BeforeDirty=false;
};
struct FWork
{
    FResult R; FString Operation,FailedStage; TSharedPtr<FJsonObject> Params,Legacy;
    TArray<FTracked> Tracked; TWeakObjectPtr<UWorld> World; bool WorldCaptured=false,PIEWorld=false;
    int32 PIEInstance=INDEX_NONE; TWeakObjectPtr<UGeometryCollectionComponent> Component;
    TWeakObjectPtr<const UGeometryCollection> Rest; FGeometryDynamicCollection* Dynamic=nullptr;
    bool ComponentCaptured=false; FGeometryCollectionPhysicsProxy* CapturedProxy=nullptr; TSharedPtr<FGeometryCollection,ESPMode::ThreadSafe> Collection;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();
    explicit FWork(const TCHAR* Name):Operation(Name)
    {
        R.Compiled=false; R.Data->SetObjectField(TEXT("native_call_counts"),Counts);
        R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
        R.Data->SetBoolField(TEXT("explicit_read_only_API"),true);
        R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);
        R.Data->SetBoolField(TEXT("cold_reload_verified"),false);
        R.Data->SetBoolField(TEXT("five_run_performance_measurement_verified"),false);
        R.Data->SetBoolField(TEXT("physics_effective_state_completion_verified"),false);
        R.Data->SetStringField(TEXT("observation_scope"),TEXT("Identity, package dirty flags and selected collection/physics projections on the game thread. Scoped physics ReadLock can synchronously wait for the scene read lock and does not make all game-thread, render and solver snapshots atomic. No wrapper worker task or extra wait is added."));
        R.Data->SetStringField(TEXT("explicit_write_scope"),TEXT("No wrapper Modify, SetDirty, schema definition, attribute addition, transaction, Blueprint compile or asset/map save. Load/getters/transform-cache and physics-lock SDK effects are globally unverified. Public mutable facade constructors use TManagedArrayAccessor -> FindAttributeTyped -> ModifyTypedPtr, which may MakeUniqueForWrite/MarkDirty internal arrays; no wrapper value assignment or package SetDirty is requested."));
        for(const TCHAR* K:{TEXT("load_calls"),TEXT("transform_cache_getter_calls"),TEXT("physics_read_lock_calls"),TEXT("dynamic_facade_constructor_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("schema_definition_calls"),TEXT("attribute_add_calls")}) { Counts->SetNumberField(K,0); }
    }
    void Count(const TCHAR* Key) { double N=0;Counts->TryGetNumberField(Key,N);Counts->SetNumberField(Key,N+1); }
    void SDK(const TCHAR* Stage) { R.Stage=Stage;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true); }
    bool Fail(const TCHAR* Code,const FString& Error)
    { if(R.Error.IsEmpty()) { R.ErrorCode=Code;R.Error=Error;FailedStage=R.Stage; } R.bSuccess=false;R.bPostconditionPassed=false;return false; }
    bool Guard()
    {
        if(!R.Error.IsEmpty()) { return false; }
        if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()) { return Fail(TEXT("unsafe_engine_state"),TEXT("Query requires a live game thread outside GC/engine shutdown")); }
        if(WorldCaptured)
        {
            UWorld* W=World.Get();
            if(!IsValid(W)||W->bIsTearingDown) { return Fail(TEXT("world_identity_changed"),TEXT("Captured query world is unavailable or tearing down")); }
            if(PIEWorld)
            { bool Found=false;if(GEngine) { for(const FWorldContext& Context:GEngine->GetWorldContexts()) { Found|=Context.WorldType==EWorldType::PIE&&Context.PIEInstance==PIEInstance&&Context.World()==W; } }if(!Found||!W->HasBegunPlay()) { return Fail(TEXT("PIE_world_changed"),TEXT("Selected begun-play PIE context changed during query")); } }
        }
        for(const FTracked& T:Tracked)
        {
            const UObject* O=T.Hold.Get();
            if(!IsValid(O)||O->GetClass()!=T.Class.Get()||O->GetOuter()!=T.Outer.Get()||O->GetPackage()!=T.Package.Get()||!O->GetPathName().Equals(T.Path,ESearchCase::CaseSensitive)||!IsValid(T.Package.Get())||!T.Package->GetName().Equals(T.PackagePath,ESearchCase::CaseSensitive)||(T.HadWorld&&O->GetWorld()!=T.ObjectWorld.Get()))
            { return Fail(TEXT("query_target_identity_changed"),TEXT("Captured object class/outer/path/package/world changed during SDK observation")); }
        }
        if(ComponentCaptured)
        {
            auto* C=Component.Get();
            if(!IsValid(C)||C->GetRestCollection()!=Rest.Get()||C->GetDynamicCollection()!=Dynamic||C->GetPhysicsProxy()!=CapturedProxy||(Rest.IsValid()&&Rest->GetGeometryCollection()!=Collection))
            { return Fail(TEXT("collection_identity_changed"),TEXT("Component/rest/dynamic collection identity changed during SDK observation")); }
        }
        return true;
    }
    bool Track(const UObject* Object,const TCHAR* Role)
    {
        if(!Guard()) { return false; } if(!IsValid(Object)||!IsValid(Object->GetPackage())) { return Fail(TEXT("query_target_invalid"),TEXT("Invalid query target or package")); }
        for(const auto& Existing:Tracked) { if(Existing.Hold.Get()==Object) { return true; } }
        FTracked T;T.Hold=TStrongObjectPtr<const UObject>(Object);T.Class=Object->GetClass();T.Outer=Object->GetOuter();T.Package=TStrongObjectPtr<UPackage>(Object->GetPackage());T.Path=Object->GetPathName();T.PackagePath=T.Package->GetName();T.Role=Role;T.BeforeDirty=T.Package->IsDirty();T.ObjectWorld=Object->GetWorld();T.HadWorld=T.ObjectWorld.IsValid();Tracked.Add(MoveTemp(T));return true;
    }
    bool Bind(UGeometryCollectionComponent* C)
    {
        if(!Track(C,TEXT("component"))) { return false; } if(C->GetOwner()&&!Track(C->GetOwner(),TEXT("actor"))) { return false; }
        Component=C;CapturedProxy=C->GetPhysicsProxy();Rest=C->GetRestCollection();Dynamic=C->GetDynamicCollection();Collection=Rest.IsValid()?Rest->GetGeometryCollection():nullptr;
        if(Rest.IsValid()&&!Track(Rest.Get(),TEXT("rest_asset"))) { return false; }
        ComponentCaptured=true;return Guard();
    }
    bool Start(const FString& Input)
    { R.Stage=TEXT("parse_params");return Guard()&&(Parse(Input,Params)||Fail(TEXT("invalid_params_json"),TEXT("Expected bounded finite JSON object without duplicate exact keys"))); }
    TSharedPtr<FJsonObject> Success(const TSharedPtr<FJsonObject>& Data)
    { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),true);J->SetObjectField(TEXT("data"),Data);return J; }
    TSharedPtr<FJsonObject> Error(const FString& Message)
    { Fail(TEXT("legacy_query_failed"),Message);auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Message);return J; }
    FString Finish()
    {
        if(R.Error.IsEmpty()) { Guard(); }
        if(Legacy.IsValid()&&!FiniteJson(MakeShared<FJsonValueObject>(Legacy)))
        { Fail(TEXT("nonfinite_observation"),TEXT("Collection/physics observation contains a nonfinite value"));R.Data->SetBoolField(TEXT("nonfinite_legacy_projection_omitted"),true);Legacy.Reset(); }
        if(Legacy.IsValid()) { R.Data->SetObjectField(TEXT("legacy_result"),Legacy); }
        TArray<TSharedPtr<FJsonValue>> Rows;bool ObservedDirtyChange=false,DirtyObservationsKnown=true;
        const bool bSafeObservation=IsInGameThread()&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting();
        for(const FTracked& T:Tracked)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("role"),T.Role);J->SetStringField(TEXT("captured_path"),T.Path);J->SetStringField(TEXT("captured_package"),T.PackagePath);J->SetBoolField(TEXT("dirty_before"),T.BeforeDirty);
            if(!bSafeObservation)
            {
                DirtyObservationsKnown=false;J->SetBoolField(TEXT("after_observation_skipped"),true);
                J->SetStringField(TEXT("after_observation_reason"),TEXT("Unsafe engine lifetime; only captured plain data is reported"));
                J->SetField(TEXT("package_alive"),MakeShared<FJsonValueNull>());J->SetField(TEXT("dirty_after"),MakeShared<FJsonValueNull>());
                Rows.Add(MakeShared<FJsonValueObject>(J));continue;
            }
            const bool Valid=IsValid(T.Package.Get());J->SetBoolField(TEXT("package_alive"),Valid);J->SetBoolField(TEXT("after_observation_skipped"),!Valid);
            if(Valid) { const bool Dirty=T.Package->IsDirty();J->SetBoolField(TEXT("dirty_after"),Dirty);ObservedDirtyChange|=Dirty!=T.BeforeDirty; }
            else { DirtyObservationsKnown=false;J->SetField(TEXT("dirty_after"),MakeShared<FJsonValueNull>()); }
            Rows.Add(MakeShared<FJsonValueObject>(J));
        }
        R.Data->SetArrayField(TEXT("target_package_observations"),Rows);R.Data->SetBoolField(TEXT("observed_package_dirty_change_known"),DirtyObservationsKnown);
        if(DirtyObservationsKnown) { R.Data->SetBoolField(TEXT("observed_package_dirty_changed"),ObservedDirtyChange); }
        else { R.Data->SetField(TEXT("observed_package_dirty_changed"),MakeShared<FJsonValueNull>()); }
        R.Data->SetBoolField(TEXT("explicit_compile_requested"),false);R.Data->SetBoolField(TEXT("explicit_save_requested"),false);
        if(R.Error.IsEmpty()&&Legacy.IsValid())
        { bool LegacySuccess=false;Legacy->TryGetBoolField(TEXT("success"),LegacySuccess);R.bSuccess=LegacySuccess;R.bPostconditionPassed=LegacySuccess;R.Stage=TEXT("complete"); }
        else { R.bSuccess=false;R.bPostconditionPassed=false;if(!FailedStage.IsEmpty()) { R.Stage=FailedStage; } }
        return Cubeless::Operations::EncodeVariableResult(R,*Operation);
    }
};

bool FiniteVector(const FVector& V) { return FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::IsFinite(V.Z); }
bool CollectionShape(FWork& W,const FGeometryCollection& C)
{
    const int32 N=C.NumElements(FGeometryCollection::TransformGroup),G=C.NumElements(FGeometryCollection::GeometryGroup);
    if(N<0||G<0||C.Parent.Num()!=N||C.Children.Num()!=N||C.Transform.Num()!=N||C.SimulationType.Num()!=N||C.TransformToGeometryIndex.Num()!=N||C.TransformIndex.Num()!=G||C.FaceStart.Num()!=G||C.FaceCount.Num()!=G||C.VertexStart.Num()!=G||C.VertexCount.Num()!=G)
    { return W.Fail(TEXT("collection_array_shape_invalid"),TEXT("Collection transform/geometry attribute counts do not match their groups")); }
    for(const auto& Transform:C.Transform) { if(Transform.ContainsNaN()) { return W.Fail(TEXT("nonfinite_rest_transform"),TEXT("Nonfinite rest transform")); } }
    return true;
}
bool DynamicShape(FWork& W,const FGeometryDynamicCollection& C)
{
    const int32 N=C.GetNumTransforms();
    if(N<0||C.Active.Num()!=N||C.DynamicState.Num()!=N||C.GetHasParent().Num()!=N||C.GetInternalClusterParentTypeAttribute().Num()!=N)
    { return W.Fail(TEXT("dynamic_array_shape_invalid"),TEXT("Dynamic facade attribute lengths do not match the transform group")); }
    return true;
}
template<typename T> bool AttributeHasRows(const FManagedArrayCollection& C,const TCHAR* Name,int32 N)
{ const auto* A=C.FindAttributeTyped<T>(FName(Name),FGeometryCollection::TransformGroup);return A&&A->Num()>=N; }
bool RemovalShapes(FWork& W,const FGeometryDynamicCollection& C,bool bSleepValid,bool bBreakValid)
{
    const int32 N=C.GetNumTransforms();
    if(bSleepValid&&!(AttributeHasRows<float>(C,TEXT("SleepTimer"),N)&&AttributeHasRows<float>(C,TEXT("MaxSleepTime"),N)&&AttributeHasRows<float>(C,TEXT("SleepRemovalDuration"),N)&&AttributeHasRows<FVector>(C,TEXT("LastPosition"),N)))
    { return W.Fail(TEXT("sleep_removal_shape_invalid"),TEXT("Sleep-removal view attributes are shorter than the dynamic transform group")); }
    if(bBreakValid&&!(AttributeHasRows<float>(C,TEXT("BreakTimer"),N)&&AttributeHasRows<float>(C,TEXT("PostBreakDuration"),N)&&AttributeHasRows<float>(C,TEXT("BreakRemovalDuration"),N)))
    { return W.Fail(TEXT("break_removal_shape_invalid"),TEXT("Break-removal view attributes are shorter than the dynamic transform group")); }
    return true;
}

TArray<TSharedPtr<FJsonValue>> VectorToJson(const FVector& Value);
TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values);
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

FCollectionMetrics MeasureCollection(FWork& W, const UGeometryCollection* Asset, double TinyRelativeThreshold)
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
    // Validate parent graph before calling SDK code that assumes valid acyclic parent indices.
    bool bSafeHierarchy=true;TArray<uint8> Colors;Colors.Init(0,Metrics.TransformCount);
    for(int32 Start=0;Start<Metrics.TransformCount;++Start)
    {
        if(Colors[Start]!=0) { continue; } TArray<int32> Chain;int32 Walk=Start;
        while(Walk!=INDEX_NONE)
        {
            if(!Collection.Parent.IsValidIndex(Walk)) { bSafeHierarchy=false;break; }
            if(Colors[Walk]==1) { bSafeHierarchy=false;break; }
            if(Colors[Walk]==2) { break; } Colors[Walk]=1;Chain.Add(Walk);Walk=Collection.Parent[Walk];
        }
        for(int32 Index:Chain) { Colors[Index]=2; }
    }
    W.R.Data->SetBoolField(TEXT("hierarchy_world_projection_available"),bSafeHierarchy);
    if(bSafeHierarchy) { GeometryCollectionAlgo::GlobalMatrices(Collection.Transform,Collection.Parent,GlobalTransforms); }
    // Invalid hierarchies retain original failure counters; local vertices are diagnostic fallback only.

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
        if(FaceStart<0||FaceCount<0||VertexStart<0||VertexCount<0||int64(FaceStart)+FaceCount>Collection.Indices.Num()||int64(VertexStart)+VertexCount>Collection.Vertex.Num())
        { ++Metrics.InvalidTriangleIndexCount;continue; }
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
                if (!Collection.Vertex.IsValidIndex(VertexIndex) || VertexIndex < VertexStart || int64(VertexIndex) >= int64(VertexStart) + VertexCount)
                {
                    bValidTriangle = false;
                    ++Metrics.InvalidTriangleIndexCount;
                    break;
                }
                Vertices[Corner] = GlobalTransforms.IsValidIndex(TransformIndex)
                    ? GlobalTransforms[TransformIndex].TransformPosition(FVector(Collection.Vertex[VertexIndex]))
                    : FVector(Collection.Vertex[VertexIndex]);
                if(!FiniteVector(Vertices[Corner])||FMath::Abs(Vertices[Corner].X)>9.0e15||FMath::Abs(Vertices[Corner].Y)>9.0e15||FMath::Abs(Vertices[Corner].Z)>9.0e15)
                { W.Fail(TEXT("metric_coordinate_unrepresentable"),TEXT("Nonfinite coordinate or coordinate exceeds the millimetre int64 edge-key range"));return Metrics; }
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
    if(!FMath::IsFinite(Metrics.TotalConvexVolume)||!FMath::IsFinite(Metrics.RelativeVolumeThreshold))
    { W.Fail(TEXT("metric_volume_nonfinite"),TEXT("Signed triangle volume sum or relative threshold is nonfinite"));return Metrics; }
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

UGeometryCollection* LoadGeometryCollection(const FString& AssetPath)
{
    const FString ObjectPath = AssetPath.Contains(TEXT("."))
        ? AssetPath
        : AssetPath + TEXT(".") + FPackageName::GetLongPackageAssetName(AssetPath);
    return LoadObject<UGeometryCollection>(nullptr, *ObjectPath);
}
TSharedPtr<FJsonObject> InspectCollection(FWork& W, UGeometryCollection* Asset, UGeometryCollectionComponent* Component, double TinyRelativeThreshold)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("asset_path"), Asset ? Asset->GetPathName() : FString());
    Data->SetBoolField(TEXT("asset_found"), Asset != nullptr);
    if (!Asset || !Asset->GetGeometryCollection().IsValid())
    {
        return Data;
    }

    const FGeometryCollection& Collection = *Asset->GetGeometryCollection();
    if(!CollectionShape(W,Collection)) { return Data; }
    const FCollectionMetrics GuardedMetrics = MeasureCollection(W,Asset,TinyRelativeThreshold);
    if(!W.Guard()) { return Data; }
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
    if (RemoveFacade.IsValid()&&!AttributeHasRows<FVector4f>(Collection,TEXT("RemoveOnBreak"),TransformCount))
    { W.Fail(TEXT("remove_on_break_shape_invalid"),TEXT("RemoveOnBreak attribute is shorter than transform count"));return Data; }
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
            if(!DynamicShape(W,*Dynamic)||!W.Guard()) { return Data; }
            int32 ActiveCount = 0;
            int32 SleepingCount = 0;
            int32 DynamicCount = 0;
            int32 BrokenCount = 0;
            W.SDK(TEXT("optional_dynamic_state_facade"));W.Count(TEXT("dynamic_facade_constructor_calls"));W.R.Data->SetBoolField(TEXT("SDK_internal_attribute_COW_or_dirty_possible"),true);
            FGeometryCollectionDynamicStateFacade StateFacade(*Dynamic);
            if(!StateFacade.IsValid()) { W.Fail(TEXT("dynamic_state_unavailable"),TEXT("Optional component dynamic-state facade is invalid"));return Data; }
            for (int32 Index = 0; Index < Dynamic->Active.Num(); ++Index)
            {
                ActiveCount += StateFacade.IsActive(Index) ? 1 : 0;
                SleepingCount += StateFacade.IsSleeping(Index) ? 1 : 0;
                DynamicCount += StateFacade.IsDynamicOrSleeping(Index) ? 1 : 0;
                BrokenCount += StateFacade.HasBrokenOff(Index) ? 1 : 0;
            }
            W.SDK(TEXT("optional_component_physics_query"));
            if(!W.Guard()) { return Data; }
            const TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects=Component->GetAllPhysicsObjects();
            TArray<Chaos::FPhysicsObjectHandle> UniqueObjects;TSet<Chaos::FPhysicsObjectHandle> Seen;int32 NullCount=0,DuplicateCount=0;
            for(auto Handle:PhysicsObjects) { if(!Handle) { ++NullCount;continue; }if(Seen.Contains(Handle)) { ++DuplicateCount;continue; }Seen.Add(Handle);UniqueObjects.Add(Handle); }
            int32 DisabledPhysicsObjectCount=0;
            if(!UniqueObjects.IsEmpty())
            {
                W.Count(TEXT("physics_read_lock_calls"));
                FLockedReadPhysicsObjectExternalInterface Read=FPhysicsObjectExternalInterface::LockRead(UniqueObjects);
                // Retain legacy per-returned-entry count, but never submit null handles to SDK read functions.
                for(auto Handle:PhysicsObjects) { if(Handle) { DisabledPhysicsObjectCount+=Read->AreAllDisabled({&Handle,1})?1:0; } }
            }
            if(!W.Guard()) { return Data; }
            Data->SetNumberField(TEXT("physics_object_null_entry_count"),NullCount);
            Data->SetNumberField(TEXT("physics_object_duplicate_entry_count"),DuplicateCount);
            Data->SetNumberField(TEXT("unique_nonnull_physics_object_count"),UniqueObjects.Num());
            TArray<bool> HiddenTransforms;
            Component->GetHiddenTransforms(HiddenTransforms);
            if(!W.Guard()) { return Data; }
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
    AActor* Actor = FindActor<AActor>(World, ActorName);
    return Actor ? Actor->FindComponentByClass<UGeometryCollectionComponent>() : nullptr;
}
TSharedPtr<FJsonObject> Performance(FWork& W, const TSharedPtr<FJsonObject>& Params)
{
    const FString AssetPath = GetStringParam(Params, TEXT("geometry_collection_path"), TEXT(""));
    if (AssetPath.IsEmpty())
    {
        return W.Error(TEXT("Missing geometry_collection_path"));
    }
    if (!IsWithinLearningRoot(AssetPath))
    {
        return W.Error(TEXT("Geometry Collection inspection is restricted to the learning temp root"));
    }
    W.SDK(TEXT("load_geometry_collection"));W.Count(TEXT("load_calls"));
    UGeometryCollection* Asset = LoadGeometryCollection(AssetPath);
    if(!W.Guard()) { return nullptr; }
    if (!Asset)
    {
        return W.Error(FString::Printf(TEXT("Geometry Collection not found: %s"), *AssetPath));
    }

    if(!W.Track(Asset,TEXT("inspected_asset"))) { return nullptr; }
    UGeometryCollectionComponent* Component = nullptr;
    const FString ActorName = GetStringParam(Params, TEXT("actor_name"), TEXT(""));
    FString WorldContext;
    if (!ActorName.IsEmpty())
    {
        UWorld* SelectedWorld=ResolveWorld(true,WorldContext);
        if(SelectedWorld) { W.World=SelectedWorld;W.WorldCaptured=true;if(!W.Track(SelectedWorld,TEXT("optional_component_world"))) { return nullptr; } }
        Component=FindGeometryCollectionComponent(SelectedWorld,ActorName);
        if(Component&&!W.Bind(Component)) { return nullptr; }
    }
    const double Threshold=GetNumberParam(Params,TEXT("tiny_relative_threshold"),0.04);
    if(!FMath::IsFinite(Threshold)||!FMath::IsFinite(FMath::Pow(Threshold,3.0))) { return W.Error(TEXT("tiny_relative_threshold must have a finite representable cube")); }
    W.SDK(TEXT("inspect_collection"));if(!W.Guard()) { return nullptr; }
    TSharedPtr<FJsonObject> Data = InspectCollection(W,Asset,Component, Threshold);
    Data->SetStringField(TEXT("world_context"), WorldContext);
    Data->SetBoolField(TEXT("read_only"), true);
    Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
    Data->SetStringField(TEXT("learning_verdict"), TEXT("BLOCKED"));
    Data->SetStringField(TEXT("learning_blocker"), TEXT("Static inspection does not satisfy the required five-run A/B performance measurement"));
    return W.Success(Data);
}
TSharedPtr<FJsonObject> Runtime(FWork& W, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("operation"), TEXT("inspect_geometry_collection_runtime_state"));
    Data->SetBoolField(TEXT("read_only"), true);

    auto BlockedResponse = [&Data,&W](const FString& Reason)
    {
        Data->SetStringField(TEXT("operation_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("assertion_status"), TEXT("BLOCKED"));
        Data->SetStringField(TEXT("blocker"), Reason);
        return W.Success(Data);
    };

    int32 RequestedPIEInstance = INDEX_NONE;
    const bool bHasRequestedPIEInstance = Params->HasField(TEXT("pie_instance"));
    Data->SetBoolField(TEXT("pie_instance_selection_explicit"), bHasRequestedPIEInstance);
    if (bHasRequestedPIEInstance)
    {
        double RequestedPIEInstanceNumber = 0.0;
        if (!Params->TryGetNumberField(TEXT("pie_instance"), RequestedPIEInstanceNumber) ||
            !FMath::IsFinite(RequestedPIEInstanceNumber) ||
            RequestedPIEInstanceNumber < 0.0 ||
            RequestedPIEInstanceNumber > static_cast<double>(TNumericLimits<int32>::Max()) ||
            RequestedPIEInstanceNumber != FMath::TruncToDouble(RequestedPIEInstanceNumber))
        {
            return BlockedResponse(TEXT("Optional parameter 'pie_instance' must be a non-negative integer"));
        }
        RequestedPIEInstance = static_cast<int32>(RequestedPIEInstanceNumber);
        Data->SetNumberField(TEXT("requested_pie_instance"), RequestedPIEInstance);
    }

    W.SDK(TEXT("select_begun_play_PIE_world"));if(!W.Guard()) { return nullptr; }
    TArray<TPair<int32, UWorld*>> BegunPlayPIEWorlds;
    int32 MatchingPIEContextCount = 0;
    if (GEngine)
    {
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            if (WorldContext.WorldType != EWorldType::PIE ||
                (bHasRequestedPIEInstance && WorldContext.PIEInstance != RequestedPIEInstance))
            {
                continue;
            }

            ++MatchingPIEContextCount;
            UWorld* CandidateWorld = WorldContext.World();
            if (IsValid(CandidateWorld) && CandidateWorld->HasBegunPlay())
            {
                BegunPlayPIEWorlds.Emplace(WorldContext.PIEInstance, CandidateWorld);
            }
        }
    }
    Data->SetNumberField(TEXT("matching_pie_context_count"), MatchingPIEContextCount);
    Data->SetNumberField(TEXT("begun_play_pie_world_count"), BegunPlayPIEWorlds.Num());
    if (MatchingPIEContextCount == 0)
    {
        return BlockedResponse(bHasRequestedPIEInstance
            ? FString::Printf(TEXT("No PIE world context exists for pie_instance %d"), RequestedPIEInstance)
            : TEXT("No PIE world context is active"));
    }
    if (BegunPlayPIEWorlds.IsEmpty())
    {
        return BlockedResponse(bHasRequestedPIEInstance
            ? FString::Printf(TEXT("PIE world for pie_instance %d has not begun play"), RequestedPIEInstance)
            : TEXT("No active PIE world has begun play"));
    }
    if (BegunPlayPIEWorlds.Num() != 1)
    {
        return BlockedResponse(bHasRequestedPIEInstance
            ? FString::Printf(
                TEXT("pie_instance %d resolved %d begun-play PIE worlds; exactly one is required"),
                RequestedPIEInstance,
                BegunPlayPIEWorlds.Num())
            : FString::Printf(
                TEXT("PIE world selection is ambiguous (%d begun-play worlds); specify pie_instance"),
                BegunPlayPIEWorlds.Num()));
    }

    const int32 SelectedPIEInstance = BegunPlayPIEWorlds[0].Key;
    UWorld* World=BegunPlayPIEWorlds[0].Value;
    W.World=World;W.WorldCaptured=true;W.PIEWorld=true;W.PIEInstance=SelectedPIEInstance;
    if(!W.Track(World,TEXT("PIE_world"))) { return nullptr; }

    Data->SetStringField(TEXT("world_context"), TEXT("play"));
    Data->SetNumberField(TEXT("pie_instance"), SelectedPIEInstance);
    Data->SetStringField(TEXT("pie_world_name"), World->GetName());
    Data->SetStringField(TEXT("pie_world_path"), World->GetPathName());
    Data->SetStringField(TEXT("pie_package"), World->GetPackage() ? World->GetPackage()->GetName() : FString());
    Data->SetStringField(TEXT("pie_map_name"), World->GetMapName());
    Data->SetNumberField(TEXT("world_time_seconds"), World->GetTimeSeconds());
    Data->SetNumberField(TEXT("frame_counter"), static_cast<double>(GFrameCounter));

    const FString RequestedActorName = GetStringParam(Params, TEXT("actor_name"), TEXT(""));
    const FString RequestedActorLabel = GetStringParam(Params, TEXT("actor_label"), TEXT(""));
    const FString RequestedComponentName = GetStringParam(Params, TEXT("component_name"), TEXT(""));
    Data->SetStringField(TEXT("requested_actor_name"), RequestedActorName);
    Data->SetStringField(TEXT("requested_actor_label"), RequestedActorLabel);
    Data->SetStringField(TEXT("requested_component_name"), RequestedComponentName);

    if (RequestedActorName.IsEmpty() && RequestedActorLabel.IsEmpty())
    {
        return BlockedResponse(TEXT("actor_name or actor_label is required"));
    }
    if (RequestedComponentName.IsEmpty())
    {
        return BlockedResponse(TEXT("component_name is required"));
    }

    TArray<AActor*> MatchingActors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        const bool bNameMatches = RequestedActorName.IsEmpty() || Candidate->GetName() == RequestedActorName;
        const bool bLabelMatches = RequestedActorLabel.IsEmpty() || Candidate->GetActorLabel() == RequestedActorLabel;
        if (bNameMatches && bLabelMatches)
        {
            MatchingActors.Add(Candidate);
        }
    }
    Data->SetNumberField(TEXT("matching_actor_count"), MatchingActors.Num());
    if (MatchingActors.Num() != 1)
    {
        return BlockedResponse(FString::Printf(
            TEXT("Actor selector must resolve exactly one PIE actor (resolved %d)"),
            MatchingActors.Num()));
    }
    AActor* Actor = MatchingActors[0];

    TArray<UGeometryCollectionComponent*> GeometryComponents;
    Actor->GetComponents<UGeometryCollectionComponent>(GeometryComponents);
    TArray<UGeometryCollectionComponent*> MatchingComponents;
    TArray<FString> AvailableComponentNames;
    for (UGeometryCollectionComponent* Candidate : GeometryComponents)
    {
        if (!Candidate)
        {
            continue;
        }
        AvailableComponentNames.Add(Candidate->GetName());
        if (Candidate->GetName() == RequestedComponentName)
        {
            MatchingComponents.Add(Candidate);
        }
    }
    Data->SetArrayField(TEXT("available_geometry_component_names"), StringsToJson(AvailableComponentNames));
    Data->SetNumberField(TEXT("matching_component_count"), MatchingComponents.Num());
    if (MatchingComponents.Num() != 1)
    {
        return BlockedResponse(FString::Printf(
            TEXT("component_name must resolve exactly one Geometry Collection component (resolved %d)"),
            MatchingComponents.Num()));
    }
    UGeometryCollectionComponent* Component=MatchingComponents[0];
    if(!W.Bind(Component)) { return nullptr; }

    Data->SetStringField(TEXT("actor_name"), Actor->GetName());
    Data->SetStringField(TEXT("actor_label"), Actor->GetActorLabel());
    Data->SetStringField(TEXT("actor_path"), Actor->GetPathName());
    Data->SetStringField(TEXT("actor_package"), Actor->GetPackage() ? Actor->GetPackage()->GetName() : FString());
    Data->SetStringField(TEXT("component_name"), Component->GetName());
    Data->SetStringField(TEXT("component_path"), Component->GetPathName());

    const UGeometryCollection* RestAsset = Component->GetRestCollection();
    const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> RestCollection =
        RestAsset ? RestAsset->GetGeometryCollection() : nullptr;
    Data->SetStringField(TEXT("rest_collection"), RestAsset ? RestAsset->GetPathName() : FString());
    Data->SetStringField(TEXT("rest_collection_package"),
        RestAsset && RestAsset->GetPackage() ? RestAsset->GetPackage()->GetName() : FString());
    if (!RestAsset || !RestCollection.IsValid())
    {
        return BlockedResponse(TEXT("The selected component has no valid Rest Collection"));
    }

    FGeometryDynamicCollection* DynamicCollection = Component->GetDynamicCollection();
    Data->SetBoolField(TEXT("dynamic_collection_available"), DynamicCollection != nullptr);
    if (!DynamicCollection)
    {
        return BlockedResponse(TEXT("The selected PIE component has no runtime Dynamic Collection"));
    }

    W.SDK(TEXT("dynamic_view_facades"));W.R.Data->SetBoolField(TEXT("SDK_internal_attribute_COW_or_dirty_possible"),true);
    for(int32 Index=0;Index<4;++Index) { W.Count(TEXT("dynamic_facade_constructor_calls")); }
    FGeometryCollectionDynamicStateFacade DynamicState(*DynamicCollection);
    FGeometryCollectionDecayDynamicFacade DecayFacade(*DynamicCollection);
    FGeometryCollectionRemoveOnSleepDynamicFacade SleepRemovalFacade(*DynamicCollection);
    FGeometryCollectionRemoveOnBreakDynamicFacade BreakRemovalFacade(*DynamicCollection);
    Data->SetBoolField(TEXT("dynamic_state_schema_available"), DynamicState.IsValid());
    Data->SetBoolField(TEXT("decay_schema_available"), DecayFacade.IsValid());
    Data->SetBoolField(TEXT("remove_on_sleep_schema_available"), SleepRemovalFacade.IsValid());
    Data->SetBoolField(TEXT("remove_on_break_schema_available"), BreakRemovalFacade.IsValid());
    if (!DynamicState.IsValid())
    {
        return BlockedResponse(TEXT("The runtime Dynamic Collection state schema is unavailable"));
    }

    if(!CollectionShape(W,*RestCollection)||!DynamicShape(W,*DynamicCollection)||!RemovalShapes(W,*DynamicCollection,SleepRemovalFacade.IsValid(),BreakRemovalFacade.IsValid())||!W.Guard()) { return W.Success(Data); }
    const bool bIncludePieces = GetBoolParam(Params, TEXT("include_pieces"), true);
    const double RawMaxPieces=GetNumberParam(Params,TEXT("max_pieces"),512.0);
    if(!FMath::IsFinite(RawMaxPieces)) { return BlockedResponse(TEXT("max_pieces must be finite")); }
    // Clamp before integer conversion; inside 1..512 this preserves legacy RoundToInt then clamp.
    const int32 MaxPieces=FMath::RoundToInt(FMath::Clamp(RawMaxPieces,1.0,512.0));
    Data->SetBoolField(TEXT("include_pieces"), bIncludePieces);
    Data->SetNumberField(TEXT("max_pieces"), MaxPieces);

    struct FRuntimeRigidPieceSnapshot
    {
        int32 TransformIndex = INDEX_NONE;
        int32 ParentIndex = INDEX_NONE;
        bool bActive = false;
        bool bDynamic = false;
        bool bSleeping = false;
        bool bKinematic = false;
        bool bStatic = false;
        bool bBrokenOff = false;
        bool bRemoved = false;
        bool bRemovalActive = false;
        bool bSleepRemovalActive = false;
        bool bBreakRemovalActive = false;
        bool bSleepRemovalMetadataAvailable = false;
        bool bRenderWorldTransformAvailable = false;
        bool bPhysicsHandlePresent = false;
        bool bDisabled = false;
        bool bPhysicsSleeping = false;
        float Decay = 0.0f;
        float MaxSleepTimeSeconds = 0.0f;
        float SleepRemovalDurationSeconds = 0.0f;
        FTransform RenderWorldTransform = FTransform::Identity;
        FTransform PhysicsWorldTransform = FTransform::Identity;
        FVector LinearVelocity = FVector::ZeroVector;
        FVector AngularVelocity = FVector::ZeroVector;
    };

    const int32 TransformCount = RestCollection->NumElements(FGeometryCollection::TransformGroup);
    const int32 DynamicTransformCount = DynamicCollection->GetNumTransforms();
    W.SDK(TEXT("get_component_space_transforms"));W.Count(TEXT("transform_cache_getter_calls"));
    const TArray<FTransform3f>& ComponentSpaceTransforms=Component->GetComponentSpaceTransforms3f();
    if(!W.Guard()) { return W.Success(Data); }
    const FTransform ComponentToWorld = Component->GetComponentTransform();
    TArray<FRuntimeRigidPieceSnapshot> RigidPieces;
    RigidPieces.Reserve(bIncludePieces?FMath::Min(TransformCount,MaxPieces):0);

    int32 RigidCount = 0;
    int32 ClusterCount = 0;
    int32 ActiveTransformCount = 0;
    int32 ActiveRigidCount = 0;
    int32 DynamicRigidCount = 0;
    int32 ActiveDynamicRigidCount = 0;
    int32 SleepingRigidCount = 0;
    int32 ActiveSleepingRigidCount = 0;
    int32 KinematicRigidCount = 0;
    int32 StaticRigidCount = 0;
    int32 BrokenOffRigidCount = 0;
    int32 RemovalActiveRigidCount = 0;
    int32 SleepRemovalMetadataRigidCount = 0;
    int32 DecayingRigidCount = 0;
    int32 RemovedRigidCount = 0;
    float MaximumDecay = 0.0f;
    float MinimumMaxSleepTimeSeconds = TNumericLimits<float>::Max();
    float MaximumMaxSleepTimeSeconds = 0.0f;
    float MinimumSleepRemovalDurationSeconds = TNumericLimits<float>::Max();
    float MaximumSleepRemovalDurationSeconds = 0.0f;

    for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
    {
        const bool bDynamicIndexValid = TransformIndex < DynamicTransformCount;
        const bool bActive = bDynamicIndexValid && DynamicState.IsActive(TransformIndex);
        ActiveTransformCount += bActive ? 1 : 0;

        const int32 SimulationType = RestCollection->SimulationType.IsValidIndex(TransformIndex)
            ? RestCollection->SimulationType[TransformIndex]
            : INDEX_NONE;
        ClusterCount += SimulationType == FGeometryCollection::ESimulationTypes::FST_Clustered ? 1 : 0;
        if (SimulationType != FGeometryCollection::ESimulationTypes::FST_Rigid)
        {
            continue;
        }

        ++RigidCount;
        FRuntimeRigidPieceSnapshot Piece;
        Piece.TransformIndex = TransformIndex;
        Piece.ParentIndex = RestCollection->Parent.IsValidIndex(TransformIndex)
            ? RestCollection->Parent[TransformIndex]
            : INDEX_NONE;
        Piece.bActive = bActive;
        Piece.bDynamic = bDynamicIndexValid && DynamicCollection->DynamicState.IsValidIndex(TransformIndex) &&
            DynamicCollection->DynamicState[TransformIndex] ==
                static_cast<uint8>(EObjectStateTypeEnum::Chaos_Object_Dynamic);
        Piece.bSleeping = bDynamicIndexValid && DynamicState.IsSleeping(TransformIndex);
        Piece.bKinematic = bDynamicIndexValid && DynamicCollection->DynamicState.IsValidIndex(TransformIndex) &&
            DynamicCollection->DynamicState[TransformIndex] ==
                static_cast<uint8>(EObjectStateTypeEnum::Chaos_Object_Kinematic);
        Piece.bStatic = bDynamicIndexValid && DynamicCollection->DynamicState.IsValidIndex(TransformIndex) &&
            DynamicCollection->DynamicState[TransformIndex] ==
                static_cast<uint8>(EObjectStateTypeEnum::Chaos_Object_Static);
        Piece.bBrokenOff = bDynamicIndexValid && Piece.ParentIndex != INDEX_NONE &&
            DynamicState.HasBrokenOff(TransformIndex);
        Piece.Decay = DecayFacade.IsValid() && TransformIndex < DecayFacade.GetDecayAttributeSize()
            ? DecayFacade.GetDecay(TransformIndex)
            : 0.0f;
        Piece.bRemoved = Piece.Decay >= 1.0f - KINDA_SMALL_NUMBER;
        Piece.bSleepRemovalActive = bDynamicIndexValid && SleepRemovalFacade.IsValid() &&
            SleepRemovalFacade.IsRemovalActive(TransformIndex);
        Piece.bBreakRemovalActive = bDynamicIndexValid && BreakRemovalFacade.IsValid() &&
            BreakRemovalFacade.IsRemovalActive(TransformIndex);
        Piece.bRemovalActive = Piece.bSleepRemovalActive || Piece.bBreakRemovalActive;
        Piece.bSleepRemovalMetadataAvailable = bDynamicIndexValid && SleepRemovalFacade.IsValid();
        if (Piece.bSleepRemovalMetadataAvailable)
        {
            Piece.MaxSleepTimeSeconds = SleepRemovalFacade.GetMaxSleepTime(TransformIndex);
            Piece.SleepRemovalDurationSeconds =
                SleepRemovalFacade.GetSleepRemovalDuration(TransformIndex);
        }
        if (ComponentSpaceTransforms.IsValidIndex(TransformIndex))
        {
            Piece.RenderWorldTransform = FTransform(ComponentSpaceTransforms[TransformIndex]) * ComponentToWorld;
            Piece.bRenderWorldTransformAvailable = true;
        }

        if((Piece.bRenderWorldTransformAvailable&&Piece.RenderWorldTransform.ContainsNaN())||!FMath::IsFinite(Piece.Decay)||!FMath::IsFinite(Piece.MaxSleepTimeSeconds)||!FMath::IsFinite(Piece.SleepRemovalDurationSeconds))
        { W.Fail(TEXT("runtime_piece_nonfinite"),TEXT("Runtime transform/removal metadata contains a nonfinite value"));return W.Success(Data); }
        ActiveRigidCount += Piece.bActive ? 1 : 0;
        DynamicRigidCount += Piece.bDynamic ? 1 : 0;
        ActiveDynamicRigidCount += Piece.bActive && Piece.bDynamic ? 1 : 0;
        SleepingRigidCount += Piece.bSleeping ? 1 : 0;
        ActiveSleepingRigidCount += Piece.bActive && Piece.bSleeping ? 1 : 0;
        KinematicRigidCount += Piece.bKinematic ? 1 : 0;
        StaticRigidCount += Piece.bStatic ? 1 : 0;
        BrokenOffRigidCount += Piece.bBrokenOff ? 1 : 0;
        RemovalActiveRigidCount += Piece.bRemovalActive ? 1 : 0;
        SleepRemovalMetadataRigidCount += Piece.bSleepRemovalMetadataAvailable ? 1 : 0;
        if (Piece.bSleepRemovalMetadataAvailable)
        {
            MinimumMaxSleepTimeSeconds =
                FMath::Min(MinimumMaxSleepTimeSeconds, Piece.MaxSleepTimeSeconds);
            MaximumMaxSleepTimeSeconds =
                FMath::Max(MaximumMaxSleepTimeSeconds, Piece.MaxSleepTimeSeconds);
            MinimumSleepRemovalDurationSeconds = FMath::Min(
                MinimumSleepRemovalDurationSeconds,
                Piece.SleepRemovalDurationSeconds);
            MaximumSleepRemovalDurationSeconds = FMath::Max(
                MaximumSleepRemovalDurationSeconds,
                Piece.SleepRemovalDurationSeconds);
        }
        DecayingRigidCount += Piece.Decay > KINDA_SMALL_NUMBER && !Piece.bRemoved ? 1 : 0;
        RemovedRigidCount += Piece.bRemoved ? 1 : 0;
        MaximumDecay = FMath::Max(MaximumDecay, Piece.Decay);
        if(bIncludePieces&&RigidPieces.Num()<MaxPieces) { RigidPieces.Add(MoveTemp(Piece)); }
    }

    FGeometryCollectionPhysicsProxy* PhysicsProxy = Component->GetPhysicsProxy();
    TArray<Chaos::FPhysicsObjectHandle> IndexedPhysicsObjects = PhysicsProxy
        ? PhysicsProxy->GetAllPhysicsObjectIncludingNulls()
        : TArray<Chaos::FPhysicsObjectHandle>();
    TArray<Chaos::FPhysicsObjectHandle> UniquePhysicsObjects;
    TSet<Chaos::FPhysicsObjectHandle> SeenPhysicsObjects;
    UniquePhysicsObjects.Reserve(IndexedPhysicsObjects.Num());
    SeenPhysicsObjects.Reserve(IndexedPhysicsObjects.Num());
    for (Chaos::FPhysicsObjectHandle PhysicsObject : IndexedPhysicsObjects)
    {
        if (PhysicsObject && !SeenPhysicsObjects.Contains(PhysicsObject))
        {
            SeenPhysicsObjects.Add(PhysicsObject);
            UniquePhysicsObjects.Add(PhysicsObject);
        }
    }

    int32 PhysicsHandlePresentRigidCount = 0;
    int32 DisabledRigidCount = 0;
    int32 DisabledPhysicsObjectCount = 0;
    int32 DynamicPhysicsObjectCount = 0;
    int32 SleepingPhysicsObjectCount = 0;
    int32 KinematicPhysicsObjectCount = 0;
    int32 StaticOrOtherPhysicsObjectCount = 0;
    FVector MeanEnabledLinearVelocity = FVector::ZeroVector;
    int32 EnabledPhysicsObjectCount = 0;
    double MaximumEnabledLinearSpeed = 0.0;
    FBox EnabledPhysicsWorldBounds(ForceInit);
    bool bRootPhysicsHandlePresent = false;
    bool bRootPhysicsObjectDisabled = false;
    FTransform RootPhysicsWorldTransform = FTransform::Identity;
    FVector RootLinearVelocity = FVector::ZeroVector;
    FVector RootAngularVelocity = FVector::ZeroVector;

    if(!W.Guard()) { return W.Success(Data); }
    const int32 CapturedRootIndex=Component->GetRootIndex();
    if (!UniquePhysicsObjects.IsEmpty())
    {
        W.SDK(TEXT("scoped_physics_read"));W.Count(TEXT("physics_read_lock_calls"));
        FLockedReadPhysicsObjectExternalInterface Read =
            FPhysicsObjectExternalInterface::LockRead(UniquePhysicsObjects);

        TArray<Chaos::FPhysicsObjectHandle> EnabledPhysicsObjects;
        for (Chaos::FPhysicsObjectHandle PhysicsObject : UniquePhysicsObjects)
        {
            const bool bDisabled = Read->AreAllDisabled({&PhysicsObject, 1});
            DisabledPhysicsObjectCount += bDisabled ? 1 : 0;
            if (bDisabled)
            {
                continue;
            }
            EnabledPhysicsObjects.Add(PhysicsObject);
            ++EnabledPhysicsObjectCount;
            const FVector Velocity = Read->GetV(PhysicsObject);
            if(!FiniteVector(Velocity)) { W.Fail(TEXT("physics_velocity_nonfinite"),TEXT("Physics velocity is nonfinite"));return W.Success(Data); }
            MeanEnabledLinearVelocity += Velocity;
            MaximumEnabledLinearSpeed = FMath::Max(MaximumEnabledLinearSpeed, Velocity.Size());
            if (Read->AreAllSleeping({&PhysicsObject, 1}))
            {
                ++SleepingPhysicsObjectCount;
            }
            else if (Read->AreAllDynamic({&PhysicsObject, 1}))
            {
                ++DynamicPhysicsObjectCount;
            }
            else if (Read->AreAllKinematic({&PhysicsObject, 1}))
            {
                ++KinematicPhysicsObjectCount;
            }
            else
            {
                ++StaticOrOtherPhysicsObjectCount;
            }
        }
        if (EnabledPhysicsObjectCount > 0)
        {
            MeanEnabledLinearVelocity /= static_cast<double>(EnabledPhysicsObjectCount);
            EnabledPhysicsWorldBounds = Read->GetWorldBounds(EnabledPhysicsObjects);
        }

        for(int32 Index=0;Index<TransformCount;++Index)
        {
            if(RestCollection->SimulationType[Index]!=FGeometryCollection::ESimulationTypes::FST_Rigid||!IndexedPhysicsObjects.IsValidIndex(Index)||!IndexedPhysicsObjects[Index]) { continue; }
            auto Handle=IndexedPhysicsObjects[Index];++PhysicsHandlePresentRigidCount;
            DisabledRigidCount+=Read->AreAllDisabled({&Handle,1})?1:0;
        }
        for (FRuntimeRigidPieceSnapshot& Piece : RigidPieces)
        {
            if (!IndexedPhysicsObjects.IsValidIndex(Piece.TransformIndex))
            {
                continue;
            }
            Chaos::FPhysicsObjectHandle PhysicsObject = IndexedPhysicsObjects[Piece.TransformIndex];
            if (!PhysicsObject)
            {
                continue;
            }
            Piece.bPhysicsHandlePresent = true;
            Piece.bDisabled = Read->AreAllDisabled({&PhysicsObject, 1});
            Piece.bPhysicsSleeping = !Piece.bDisabled && Read->AreAllSleeping({&PhysicsObject, 1});
            Piece.PhysicsWorldTransform = Read->GetTransform(PhysicsObject);
            Piece.LinearVelocity = Read->GetV(PhysicsObject);
            Piece.AngularVelocity = Read->GetW(PhysicsObject);
            if(Piece.PhysicsWorldTransform.ContainsNaN()||!FiniteVector(Piece.LinearVelocity)||!FiniteVector(Piece.AngularVelocity))
            { W.Fail(TEXT("physics_piece_nonfinite"),TEXT("Physics piece transform/velocity is nonfinite"));return W.Success(Data); }
            // Aggregate counts above cover every rigid transform, including omitted output pieces.
        }

        const int32 RootIndex=CapturedRootIndex;
        if (IndexedPhysicsObjects.IsValidIndex(RootIndex))
        {
            Chaos::FPhysicsObjectHandle RootPhysicsObject = IndexedPhysicsObjects[RootIndex];
            if (RootPhysicsObject)
            {
                bRootPhysicsHandlePresent = true;
                bRootPhysicsObjectDisabled = Read->AreAllDisabled({&RootPhysicsObject, 1});
                RootPhysicsWorldTransform = Read->GetTransform(RootPhysicsObject);
                RootLinearVelocity = Read->GetV(RootPhysicsObject);
                RootAngularVelocity = Read->GetW(RootPhysicsObject);
            }
        }
    }

    if(!W.Guard()) { return W.Success(Data); }
    W.SDK(TEXT("get_root_current_transform"));W.Count(TEXT("transform_cache_getter_calls"));
    const FTransform RootRenderWorldTransform=Component->GetRootCurrentTransform();
    if(!W.Guard()) { return W.Success(Data); }
    if(RootRenderWorldTransform.ContainsNaN()||RootPhysicsWorldTransform.ContainsNaN()||!FiniteVector(RootLinearVelocity)||!FiniteVector(RootAngularVelocity)||!FiniteVector(MeanEnabledLinearVelocity)||!FMath::IsFinite(MaximumEnabledLinearSpeed))
    { W.Fail(TEXT("runtime_aggregate_nonfinite"),TEXT("Runtime root/physics aggregate is nonfinite"));return W.Success(Data); }
    Data->SetNumberField(TEXT("indexed_physics_object_entry_count"),IndexedPhysicsObjects.Num());
    Data->SetNumberField(TEXT("unique_nonnull_physics_object_count"),UniquePhysicsObjects.Num());
    int32 NullPhysicsEntries=0;for(auto Handle:IndexedPhysicsObjects) { NullPhysicsEntries+=Handle?0:1; }
    Data->SetNumberField(TEXT("null_physics_object_entry_count"),NullPhysicsEntries);
    Data->SetNumberField(TEXT("duplicate_physics_object_entry_count"),IndexedPhysicsObjects.Num()-NullPhysicsEntries-UniquePhysicsObjects.Num());
    Data->SetStringField(TEXT("aggregate_scope"),TEXT("All rest transforms and unique non-null physics handles; piece output alone is bounded to max_pieces. Indexed mapping is retained for rigid and root handle projections."));
    TSharedPtr<FJsonObject> Aggregate = MakeShared<FJsonObject>();
    Aggregate->SetNumberField(TEXT("transform_count"), TransformCount);
    Aggregate->SetNumberField(TEXT("dynamic_transform_count"), DynamicTransformCount);
    Aggregate->SetNumberField(TEXT("rigid_piece_count"), RigidCount);
    Aggregate->SetNumberField(TEXT("cluster_transform_count"), ClusterCount);
    Aggregate->SetNumberField(TEXT("active_transform_count"), ActiveTransformCount);
    Aggregate->SetNumberField(TEXT("active_rigid_count"), ActiveRigidCount);
    Aggregate->SetNumberField(TEXT("dynamic_rigid_count"), DynamicRigidCount);
    Aggregate->SetNumberField(TEXT("active_dynamic_rigid_count"), ActiveDynamicRigidCount);
    Aggregate->SetNumberField(TEXT("sleeping_rigid_count"), SleepingRigidCount);
    Aggregate->SetNumberField(TEXT("active_sleeping_rigid_count"), ActiveSleepingRigidCount);
    Aggregate->SetNumberField(TEXT("kinematic_rigid_count"), KinematicRigidCount);
    Aggregate->SetNumberField(TEXT("static_rigid_count"), StaticRigidCount);
    Aggregate->SetNumberField(TEXT("broken_off_rigid_count"), BrokenOffRigidCount);
    Aggregate->SetNumberField(TEXT("removal_active_rigid_count"), RemovalActiveRigidCount);
    Aggregate->SetNumberField(TEXT("sleep_removal_metadata_rigid_count"), SleepRemovalMetadataRigidCount);
    Aggregate->SetNumberField(TEXT("minimum_max_sleep_time_seconds"),
        SleepRemovalMetadataRigidCount > 0 ? MinimumMaxSleepTimeSeconds : 0.0f);
    Aggregate->SetNumberField(TEXT("maximum_max_sleep_time_seconds"),
        SleepRemovalMetadataRigidCount > 0 ? MaximumMaxSleepTimeSeconds : 0.0f);
    Aggregate->SetNumberField(TEXT("minimum_sleep_removal_duration_seconds"),
        SleepRemovalMetadataRigidCount > 0 ? MinimumSleepRemovalDurationSeconds : 0.0f);
    Aggregate->SetNumberField(TEXT("maximum_sleep_removal_duration_seconds"),
        SleepRemovalMetadataRigidCount > 0 ? MaximumSleepRemovalDurationSeconds : 0.0f);
    Aggregate->SetNumberField(TEXT("decaying_rigid_count"), DecayingRigidCount);
    Aggregate->SetNumberField(TEXT("removed_rigid_count"), RemovedRigidCount);
    Aggregate->SetNumberField(TEXT("visible_rigid_count"), FMath::Max(0, RigidCount - RemovedRigidCount));
    Aggregate->SetNumberField(TEXT("maximum_decay"), MaximumDecay);
    Aggregate->SetNumberField(TEXT("physics_handle_present_rigid_count"), PhysicsHandlePresentRigidCount);
    Aggregate->SetNumberField(TEXT("disabled_rigid_count"), DisabledRigidCount);
    Aggregate->SetNumberField(TEXT("physics_object_count"), UniquePhysicsObjects.Num());
    Aggregate->SetNumberField(TEXT("enabled_physics_object_count"), EnabledPhysicsObjectCount);
    Aggregate->SetNumberField(TEXT("disabled_physics_object_count"), DisabledPhysicsObjectCount);
    Aggregate->SetNumberField(TEXT("dynamic_physics_object_count"), DynamicPhysicsObjectCount);
    Aggregate->SetNumberField(TEXT("sleeping_physics_object_count"), SleepingPhysicsObjectCount);
    Aggregate->SetNumberField(TEXT("kinematic_physics_object_count"), KinematicPhysicsObjectCount);
    Aggregate->SetNumberField(TEXT("static_or_other_physics_object_count"), StaticOrOtherPhysicsObjectCount);
    Aggregate->SetArrayField(TEXT("mean_enabled_linear_velocity"), VectorToJson(MeanEnabledLinearVelocity));
    Aggregate->SetNumberField(TEXT("maximum_enabled_linear_speed"), MaximumEnabledLinearSpeed);
    Aggregate->SetBoolField(TEXT("enabled_physics_world_bounds_available"), EnabledPhysicsWorldBounds.IsValid != 0);
    Aggregate->SetArrayField(TEXT("enabled_physics_world_bounds_min"),
        VectorToJson(EnabledPhysicsWorldBounds.IsValid ? EnabledPhysicsWorldBounds.Min : FVector::ZeroVector));
    Aggregate->SetArrayField(TEXT("enabled_physics_world_bounds_max"),
        VectorToJson(EnabledPhysicsWorldBounds.IsValid ? EnabledPhysicsWorldBounds.Max : FVector::ZeroVector));
    Aggregate->SetNumberField(TEXT("root_index"), Component->GetRootIndex());
    Aggregate->SetBoolField(TEXT("root_broken"), Component->IsRootBroken());
    Aggregate->SetNumberField(TEXT("root_broken_elapsed_ms"), Component->GetRootBrokenElapsedTimeInMs());
    Aggregate->SetArrayField(TEXT("root_render_world_position"), VectorToJson(RootRenderWorldTransform.GetLocation()));
    Aggregate->SetObjectField(TEXT("root_render_world_rotation"), RotatorToJson(RootRenderWorldTransform.Rotator()));
    Aggregate->SetArrayField(TEXT("root_render_world_up"),
        VectorToJson(RootRenderWorldTransform.GetUnitAxis(EAxis::Z)));
    Aggregate->SetBoolField(TEXT("root_physics_handle_present"), bRootPhysicsHandlePresent);
    Aggregate->SetBoolField(TEXT("root_physics_object_disabled"), bRootPhysicsObjectDisabled);
    Aggregate->SetArrayField(TEXT("root_physics_world_position"), VectorToJson(RootPhysicsWorldTransform.GetLocation()));
    Aggregate->SetObjectField(TEXT("root_physics_world_rotation"), RotatorToJson(RootPhysicsWorldTransform.Rotator()));
    Aggregate->SetArrayField(TEXT("root_linear_velocity"), VectorToJson(RootLinearVelocity));
    Aggregate->SetArrayField(TEXT("root_angular_velocity"), VectorToJson(RootAngularVelocity));
    Data->SetObjectField(TEXT("aggregate"), Aggregate);

    TArray<TSharedPtr<FJsonValue>> PieceValues;
    if (bIncludePieces)
    {
        const int32 ReturnCount = FMath::Min(MaxPieces, RigidPieces.Num());
        PieceValues.Reserve(ReturnCount);
        for (int32 PieceIndex = 0; PieceIndex < ReturnCount; ++PieceIndex)
        {
            const FRuntimeRigidPieceSnapshot& Piece = RigidPieces[PieceIndex];
            TSharedPtr<FJsonObject> PieceData = MakeShared<FJsonObject>();
            PieceData->SetNumberField(TEXT("transform_index"), Piece.TransformIndex);
            PieceData->SetNumberField(TEXT("parent"), Piece.ParentIndex);
            PieceData->SetStringField(TEXT("simulation_type"), TEXT("rigid"));
            PieceData->SetNumberField(TEXT("simulation_type_value"),
                static_cast<int32>(FGeometryCollection::ESimulationTypes::FST_Rigid));
            PieceData->SetBoolField(TEXT("active"), Piece.bActive);
            PieceData->SetBoolField(TEXT("dynamic"), Piece.bDynamic);
            PieceData->SetBoolField(TEXT("physics_handle_present"), Piece.bPhysicsHandlePresent);
            PieceData->SetBoolField(TEXT("disabled"), Piece.bDisabled);
            PieceData->SetBoolField(TEXT("broken_off"), Piece.bBrokenOff);
            PieceData->SetBoolField(TEXT("sleeping"), Piece.bSleeping);
            PieceData->SetBoolField(TEXT("physics_sleeping"), Piece.bPhysicsSleeping);
            PieceData->SetNumberField(TEXT("decay"), Piece.Decay);
            PieceData->SetBoolField(TEXT("removed"), Piece.bRemoved);
            PieceData->SetBoolField(TEXT("removal_active"), Piece.bRemovalActive);
            PieceData->SetBoolField(TEXT("remove_on_sleep_active"), Piece.bSleepRemovalActive);
            PieceData->SetBoolField(TEXT("remove_on_break_active"), Piece.bBreakRemovalActive);
            PieceData->SetBoolField(TEXT("sleep_removal_metadata_available"),
                Piece.bSleepRemovalMetadataAvailable);
            PieceData->SetNumberField(TEXT("max_sleep_time_seconds"), Piece.MaxSleepTimeSeconds);
            PieceData->SetNumberField(TEXT("sleep_removal_duration_seconds"),
                Piece.SleepRemovalDurationSeconds);
            PieceData->SetBoolField(TEXT("render_world_transform_available"),
                Piece.bRenderWorldTransformAvailable);
            PieceData->SetArrayField(TEXT("render_world_position"),
                VectorToJson(Piece.RenderWorldTransform.GetLocation()));
            PieceData->SetObjectField(TEXT("render_world_rotation"),
                RotatorToJson(Piece.RenderWorldTransform.Rotator()));
            PieceData->SetArrayField(TEXT("physics_world_position"),
                VectorToJson(Piece.PhysicsWorldTransform.GetLocation()));
            PieceData->SetObjectField(TEXT("physics_world_rotation"),
                RotatorToJson(Piece.PhysicsWorldTransform.Rotator()));
            PieceData->SetArrayField(TEXT("linear_velocity"), VectorToJson(Piece.LinearVelocity));
            PieceData->SetArrayField(TEXT("angular_velocity"), VectorToJson(Piece.AngularVelocity));
            PieceValues.Add(MakeShared<FJsonValueObject>(PieceData));
        }
    }
    Data->SetArrayField(TEXT("rigid_pieces"), PieceValues);
    Data->SetNumberField(TEXT("returned_rigid_piece_count"), PieceValues.Num());
    Data->SetBoolField(TEXT("rigid_piece_output_truncated"),
        bIncludePieces && RigidCount > PieceValues.Num());
    Data->SetStringField(TEXT("operation_status"), TEXT("PASS"));
    Data->SetStringField(TEXT("assertion_status"), TEXT("PASS"));
    return W.Success(Data);
}
}
FString InspectPerformance(const FString& ParamsJson)
{
    FWork W(TEXT("inspect_geometry_collection_performance_guarded"));
    if(W.Start(ParamsJson)) { W.Legacy=Performance(W,W.Params); }
    return W.Finish();
}
FString InspectRuntime(const FString& ParamsJson)
{
    FWork W(TEXT("inspect_geometry_collection_runtime_state"));
    if(W.Start(ParamsJson)) { W.Legacy=Runtime(W,W.Params); }
    return W.Finish();
}
}
