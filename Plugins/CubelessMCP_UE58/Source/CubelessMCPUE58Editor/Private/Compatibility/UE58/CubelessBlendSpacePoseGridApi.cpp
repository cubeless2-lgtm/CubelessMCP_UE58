#include "Compatibility/UE58/CubelessBlendSpacePoseGridApi.h"
#include "Compatibility/UE58/CubelessSkeletalBoneSamplingApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
namespace Cubeless::UE58::BlendSpaceRuntimePoseGrid
{
namespace
{
using Variable::FResult;
constexpr int32 MaxCarrier=1024*1024,MaxRows=4096,MaxTargets=64,MaxSamples=256,MaxTicks=4096;
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(const FString& S,int32 Limit=MaxCarrier){if(S.Len()>Limit){return false;}for(TCHAR C:S){if(C==0){return false;}}return true;}
bool Decode(const FString& Text,TSharedPtr<FJsonValue>& Out)
{
    if(Text.IsEmpty()||!Safe(Text)){return false;}bool Quote=false,Escape=false;int32 Depth=0,Slots=0;
    for(TCHAR C:Text)
    {
        if(Quote){if(Escape){Escape=false;}else if(C==TEXT('\\')){Escape=true;}else if(C==TEXT('"')){Quote=false;}continue;}
        if(C==TEXT('"')){Quote=true;}else if(C==TEXT('[')||C==TEXT('{')){if(++Depth>64||++Slots>65536){return false;}}else if(C==TEXT(']')||C==TEXT('}')){if(--Depth<0){return false;}}else if(C==TEXT(',')&&++Slots>65536){return false;}
    }
    TArray<TSharedPtr<FJsonValue>> A;if(Quote||Depth!=0||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),A)||A.Num()!=1){return false;}Out=A[0];TArray<TSharedPtr<FJsonValue>> Stack={Out};Slots=0;
    while(!Stack.IsEmpty())
    {auto V=Stack.Pop(EAllowShrinking::No);if(!V.IsValid()||++Slots>65536){return false;}if(V->Type==EJson::Number&&!FMath::IsFinite(V->AsNumber())){return false;}if(V->Type==EJson::Array){Stack.Append(V->AsArray());}else if(V->Type==EJson::Object){for(const auto& P:V->AsObject()->Values){Stack.Add(P.Value);}}if(Stack.Num()>65536){return false;}}
    return true;
}
bool Projection(const FString& Text,std::initializer_list<const TCHAR*> Keys,TSharedPtr<FJsonObject>& Out)
{TSharedPtr<FJsonValue> V;if(!Decode(Text,V)||V->Type!=EJson::Object){return false;}Out=V->AsObject();for(const auto& P:Out->Values){bool Found=false;for(const TCHAR* K:Keys){Found|=FString(P.Key.ToView()).Equals(K,ESearchCase::CaseSensitive);}if(!Found){return false;}}return true;}
FString JsonValue(const TSharedPtr<FJsonValue>& V){FString S;TArray<TSharedPtr<FJsonValue>> A={V};FJsonSerializer::Serialize(A,TJsonWriterFactory<>::Create(&S));return S.Mid(1,S.Len()-2);}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& A){TArray<TSharedPtr<FJsonValue>> R;for(const FString& S:A){R.Add(MakeShared<FJsonValueString>(S));}return R;}
TArray<FString> Names(const TSharedPtr<FJsonValue>& V,const TArray<FString>& Defaults)
{
    TArray<FString> R;if(!V.IsValid()||V->Type!=EJson::Array){return Defaults;}
    for(const auto& Row:V->AsArray())
    {if(Row->Type==EJson::String){R.Add(Row->AsString());}else if(Row->Type==EJson::Object){FString Name,Type;if(Row->AsObject()->TryGetStringField(TEXT("name"),Name)){if(Row->AsObject()->TryGetStringField(TEXT("type"),Type)&&!Type.IsEmpty()){Name=Type+TEXT(":")+Name;}R.Add(Name);}}}
    return R.IsEmpty()?Defaults:R;
}
FString WorldType(const UWorld* W){if(!W){return TEXT("None");}switch(W->WorldType){case EWorldType::Editor:return TEXT("Editor");case EWorldType::PIE:return TEXT("PIE");case EWorldType::Game:return TEXT("Game");case EWorldType::GamePreview:return TEXT("GamePreview");case EWorldType::EditorPreview:return TEXT("EditorPreview");case EWorldType::Inactive:return TEXT("Inactive");default:return TEXT("Unknown");}}
TArray<TSharedPtr<FJsonValue>> VectorArray(const FVector& V){return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
TSharedRef<FJsonObject> InputJson(const FVector& V){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("x"),V.X);O->SetNumberField(TEXT("y"),V.Y);O->SetNumberField(TEXT("z"),V.Z);O->SetArrayField(TEXT("vector"),VectorArray(V));return O;}
TSharedRef<FJsonObject> TransformJson(const FTransform& T)
{
    auto O=MakeShared<FJsonObject>();const FQuat Q=T.GetRotation();const auto E=Q.Rotator();O->SetArrayField(TEXT("translation"),VectorArray(T.GetTranslation()));O->SetArrayField(TEXT("rotation"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});O->SetArrayField(TEXT("rotation_euler"),VectorArray(FVector(E.Pitch,E.Yaw,E.Roll)));O->SetArrayField(TEXT("scale"),VectorArray(T.GetScale3D()));return O;
}
bool VectorValue(const TSharedPtr<FJsonValue>& V,FVector& Out)
{
    double N[3]={0,0,0};if(!V.IsValid()){return false;}
    if(V->Type==EJson::Array){const auto& A=V->AsArray();if(A.Num()<3){return false;}for(int32 I=0;I<3;++I){if(A[I]->Type!=EJson::Number||!A[I]->TryGetNumber(N[I])){return false;}}}
    else if(V->Type==EJson::Object){for(int32 I=0;I<3;++I){if(!V->AsObject()->TryGetNumberField(I==0?TEXT("x"):(I==1?TEXT("y"):TEXT("z")),N[I])&&!V->AsObject()->TryGetNumberField(I==0?TEXT("X"):(I==1?TEXT("Y"):TEXT("Z")),N[I])){return false;}}}else{return false;}
    for(double X:N){if(!FMath::IsFinite(X)||FMath::Abs(X)>TNumericLimits<float>::Max()){return false;}}Out=FVector(N[0],N[1],N[2]);return true;
}
struct FIdentity
{
    TStrongObjectPtr<UObject> Object,Class,Outer,Package;FString Path,PackageName;
    explicit FIdentity(UObject* O):Object(O),Class(O->GetClass()),Outer(O->GetOuter()),Package(O->GetPackage()),Path(O->GetPathName()),PackageName(O->GetPackage()->GetName()){}
    bool Current()const{return Live(Object.Get())&&Live(Class.Get())&&Live(Package.Get())&&!CastChecked<UClass>(Class.Get())->HasAnyClassFlags(CLASS_NewerVersionExists)&&Object->GetClass()==Class.Get()&&Object->GetOuter()==Outer.Get()&&Object->GetPackage()==Package.Get()&&Object->GetPathName()==Path&&Package->GetPathName()==PackageName;}
};
struct FWork
{
    FResult Result;TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Errors,Warnings,CleanupResults,TargetResults;
    TArray<TUniquePtr<FIdentity>> Sources,RuntimeIds,AnimIds;TSet<UObject*> SourceSet;TArray<UAnimInstance*> Participants;TArray<TStrongObjectPtr<UAnimInstance>> HeldParticipants;
    TStrongObjectPtr<UWorld> World;TStrongObjectPtr<ASkeletalMeshActor> Actor;TStrongObjectPtr<USkeletalMeshComponent> Component;TStrongObjectPtr<USkeletalMesh> Mesh;TStrongObjectPtr<UAnimSingleNodeInstance> Instance;
    UAnimInstance* PostProcess=nullptr;TArray<UAnimInstance*> Linked;bool Spawned=false,Released=false,CleanupConsidered=false,CleanupAttempted=false,CleanupRequested=true,WorldDirtyBefore=false;EWorldType::Type Type=EWorldType::None;int32 Ticks=0,Checks=0;
    FWork(bool bRequestCleanup):CleanupRequested(bRequestCleanup)
    {Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Counts);Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);Result.Data->SetBoolField(TEXT("atomic_rollback_supported"),false);Result.Data->SetStringField(TEXT("effect_scope"),TEXT("Explicit transient spawn/configure/tick/refresh/destroy are counted; opaque load/initialization/callback/cache effects remain unknown. No explicit compile/save/startPIE/stopPIE."));}
    void Count(const TCHAR* K){double N=0;Counts->TryGetNumberField(K,N);Counts->SetNumberField(K,N+1);}
    void SDK(const TCHAR* K){Result.bChangeStateKnown=false;Count(K);}
    bool Fail(const TCHAR* Code,const FString& Error){if(Result.Error.IsEmpty()){Result.ErrorCode=FString(TEXT("blendspace_pose_grid_"))+Code;Result.Error=Error;}return false;}
    bool Hold(UObject* O)
    {if(!O){return true;}if(!Live(O)||!Live(O->GetClass())||!Live(O->GetPackage())){return Fail(TEXT("source_unreadable"),TEXT("Source object/class/package is unavailable"));}if(SourceSet.Contains(O)){return true;}if(SourceSet.Num()>=65536){return Fail(TEXT("source_bound"),TEXT("Source inventory exceeds65536"));}SourceSet.Add(O);Sources.Add(MakeUnique<FIdentity>(O));return true;}
    bool Base(bool IgnoreExistingError=false)
    {
        if(!IgnoreExistingError&&!Result.Error.IsEmpty()){return false;}if(++Checks>1048576||!IsEditorGameThread()||IsEngineExitRequested()){return Fail(TEXT("editor_boundary"),TEXT("Finite Editor game-thread boundary lost; PIE/SIE reads/writes are permitted for this transient probe"));}
        for(const auto& Id:Sources){if(!Id->Current()){return Fail(TEXT("source_identity_changed"),TEXT("Asset/world class, outer, actual package or path changed during callback"));}}
        if(World.IsValid())
        {bool Present=false;if(GEngine){for(const auto& C:GEngine->GetWorldContexts()){Present|=C.World()==World.Get();}}if(GEditor){Present|=GEditor->GetEditorWorldContext().World()==World.Get();}if(!Present||World->WorldType!=Type){return Fail(TEXT("world_changed"),TEXT("Selected world context/type changed"));}}
        return true;
    }
    bool Runtime(bool CheckAnim=true,bool IgnoreExistingError=false,bool CheckMesh=true)
    {
        if(!Base(IgnoreExistingError)||!Spawned||Released){return false;}for(const auto& Id:RuntimeIds){if(!Id->Current()){return Fail(TEXT("probe_identity_changed"),TEXT("Owned transient actor/component identity changed"));}}
        if(!Live(Actor.Get())||!Live(Component.Get())||!Actor->HasAnyFlags(RF_Transient)||Actor->GetClass()!=ASkeletalMeshActor::StaticClass()||Actor->GetWorld()!=World.Get()||Actor->GetSkeletalMeshComponent()!=Component.Get()||Component->GetOwner()!=Actor.Get()||(CheckMesh&&Component->GetSkeletalMeshAsset()!=Mesh.Get()))
        {return Fail(TEXT("probe_binding_changed"),TEXT("Owned transient actor/component/world/mesh binding changed"));}
        if(Component->IsRunningParallelEvaluation()){return Fail(TEXT("parallel_pending"),TEXT("Parallel evaluation is active; no explicit wait or redispatch was requested"));}
        if(CheckAnim)
        {
            for(const auto& Id:AnimIds){if(!Id->Current()){return Fail(TEXT("anim_identity_changed"),TEXT("A tick participant identity changed"));}}
            if(!Live(Instance.Get())||Component->GetSingleNodeInstance()!=Instance.Get()||Component->GetAnimInstance()!=Instance.Get()||Component->GetPostProcessInstance()!=PostProcess||static_cast<const USkeletalMeshComponent*>(Component.Get())->GetLinkedAnimInstances()!=Linked)
            {return Fail(TEXT("anim_binding_changed"),TEXT("Main/post-process/linked tick participant binding changed"));}
            for(UAnimInstance* A:Participants){if(!Live(A)||!A->IsInitialized()||A->GetOuter()!=Component.Get()){return Fail(TEXT("anim_uninitialized"),TEXT("Tick participant is unreadable or uninitialized; no forced initialization was requested"));}}
        }
        return Base(IgnoreExistingError);
    }
    bool CaptureParticipants()
    {
        if(!Runtime(false)){return false;}AnimIds.Reset();Participants.Reset();HeldParticipants.Reset();Instance.Reset(Component->GetSingleNodeInstance());PostProcess=Component->GetPostProcessInstance();Linked=static_cast<const USkeletalMeshComponent*>(Component.Get())->GetLinkedAnimInstances();
        if(!Live(Instance.Get())||Instance->GetClass()!=UAnimSingleNodeInstance::StaticClass()||Linked.Num()>64){return Fail(TEXT("single_node_unavailable"),TEXT("Expected stock initialized single-node instance and bounded linked participants"));}
        auto Add=[&](UAnimInstance* A){if(A&&!Participants.Contains(A)){if(!Live(A)||!Live(A->GetClass())||!Live(A->GetPackage())){return false;}Participants.Add(A);HeldParticipants.Emplace(A);AnimIds.Add(MakeUnique<FIdentity>(A));}return true;};
        if(!Add(Instance.Get())||!Add(PostProcess)){return Fail(TEXT("anim_unreadable"),TEXT("Main or post-process participant unavailable"));}for(UAnimInstance* A:Linked){if(!Add(A)){return Fail(TEXT("anim_unreadable"),TEXT("Linked participant unavailable"));}}
        return Runtime();
    }
    void Cleanup()
    {
        if(!CleanupRequested||!Spawned||Released||CleanupConsidered){return;}CleanupConsidered=true;auto Receipt=MakeShared<FJsonObject>();CleanupResults.Add(MakeShared<FJsonValueObject>(Receipt));Receipt->SetStringField(TEXT("label"),TEXT("blendspace_pose_grid_actor"));Receipt->SetBoolField(TEXT("destroyed"),false);
        if(!Runtime(false,true)){Receipt->SetBoolField(TEXT("skipped_unsafe_identity_or_parallel"),true);Result.Data->SetBoolField(TEXT("cleanup_required"),true);return;}
        Receipt->SetStringField(TEXT("actor_name"),Actor->GetName());CleanupAttempted=true;SDK(TEXT("destroy_actor_calls"));const bool Destroyed=World->DestroyActor(Actor.Get());Count(TEXT("destroy_actor_returns"));Receipt->SetBoolField(TEXT("destroyed"),Destroyed);
        if(Destroyed){Released=true;RuntimeIds.Reset();AnimIds.Reset();Participants.Reset();HeldParticipants.Reset();}else{Errors.Add(MakeShared<FJsonValueString>(TEXT("Requested transient actor cleanup failed")));Fail(TEXT("cleanup_failed"),TEXT("Requested transient actor cleanup failed; no automatic retry"));}
        Result.Data->SetBoolField(TEXT("cleanup_required"),!Destroyed);Base(true);
    }
    FResult Finish()
    {
        Cleanup();if(!Result.Error.IsEmpty()){Result.bSuccess=Result.bPostconditionPassed=false;Legacy->SetStringField(TEXT("error"),Result.Error);}
        if(!Errors.IsEmpty()){Result.bSuccess=Result.bPostconditionPassed=false;if(Result.Error.IsEmpty()){Fail(TEXT("sample_failed"),TEXT("One or more BlendSpace runtime pose samples failed"));Legacy->SetStringField(TEXT("error"),Result.Error);}}
        Legacy->SetBoolField(TEXT("success"),Result.bSuccess);Legacy->SetBoolField(TEXT("transient_actor_spawned"),Spawned);Legacy->SetArrayField(TEXT("blendspaces"),TargetResults);Legacy->SetArrayField(TEXT("cleanup_results"),CleanupResults);Legacy->SetArrayField(TEXT("errors"),Errors);Legacy->SetArrayField(TEXT("warnings"),Warnings);
        Result.Data->SetBoolField(TEXT("observed_transient_runtime_change"),Spawned);Result.Data->SetBoolField(TEXT("cleanup_required"),Spawned&&!Released);Result.Data->SetBoolField(TEXT("cleanup_was_requested"),CleanupRequested);Result.Data->SetBoolField(TEXT("cleanup_attempted"),CleanupAttempted);Result.Data->SetNumberField(TEXT("executed_tick_count_total"),Ticks);Result.Data->SetBoolField(TEXT("explicit_asset_write_performed"),false);
        if(Live(World.Get())&&Live(World->GetPackage())){Result.Data->SetBoolField(TEXT("world_package_dirty_before"),WorldDirtyBefore);Result.Data->SetBoolField(TEXT("world_package_dirty_after"),World->GetPackage()->IsDirty());}Result.bChanged=Spawned;return Result;
    }
};
UObject* Load(const FString& Raw,FWork& W)
{
    FString P=FPackageName::ExportTextPathToObjectPath(Raw).TrimStartAndEnd();P.TrimQuotesInline();FString Package=P;int32 Dot=INDEX_NONE;if(Package.FindChar(TEXT('.'),Dot)){Package.LeftInline(Dot);}if(!Safe(P,NAME_SIZE-1)||!FPackageName::IsValidLongPackageName(Package)){W.Errors.Add(MakeShared<FJsonValueString>(TEXT("Asset path is not a bounded valid package/object path")));return nullptr;}if(!P.Contains(TEXT("."))){P+=TEXT(".")+FPackageName::GetShortName(P);}if(!Safe(P,NAME_SIZE-1)){W.Errors.Add(MakeShared<FJsonValueString>(TEXT("Asset object path exceeds FName safety bound")));return nullptr;}W.SDK(TEXT("asset_load_calls"));UObject* O=StaticLoadObject(UObject::StaticClass(),nullptr,*P);if(!W.Base()||!W.Hold(O)){return nullptr;}return O;
}
struct FSample{FString Label;FVector Input=FVector::ZeroVector;};
struct FTarget{FString Name,Path;UBlendSpace* Asset=nullptr;TArray<FSample> Samples;};
bool AssetReady(UBlendSpace* A,FWork& W)
{
    if(!W.Base()||!W.Hold(A)||A->GetBlendSamples().Num()>MaxRows){return W.Fail(TEXT("asset_bound"),TEXT("BlendSpace sample inventory is unreadable or exceeds4096"));}
    for(int32 I=0;I<3;++I){const auto P=A->GetBlendParameter(I);const float Step=(P.Max-P.Min)/P.GridNum;if(!FMath::IsFinite(P.Min)||!FMath::IsFinite(P.Max)||P.Min>=P.Max||P.GridNum<1||P.GridNum>256||!FMath::IsFinite(Step)||Step<=0){return W.Fail(TEXT("axis_invalid"),TEXT("BlendSpace axes require finite ordered ranges and grid1..256"));}}
    if(!W.Hold(A->GetSkeleton())){return false;}for(const auto& S:A->GetBlendSamples()){if(S.SampleValue.ContainsNaN()||!FMath::IsFinite(S.RateScale)||!W.Hold(S.Animation)){return W.Fail(TEXT("sample_unreadable"),TEXT("BlendSpace sample source/value is unreadable"));}}
    return W.Base();
}
TArray<FSample> ParseSamples(const TSharedPtr<FJsonValue>& Value,FWork& W)
{
    TArray<FSample> R;if(!Value.IsValid()||Value->Type!=EJson::Array){return R;}if(Value->AsArray().Num()>MaxSamples){W.Fail(TEXT("sample_bound"),TEXT("Requested sample list exceeds256"));return R;}
    for(int32 I=0;I<Value->AsArray().Num();++I)
    {
        auto V=Value->AsArray()[I];if(V->Type!=EJson::Object){W.Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("samples[%d] must be an object"),I)));continue;}auto O=V->AsObject();FString Label;if(!O->TryGetStringField(TEXT("label"),Label)){O->TryGetStringField(TEXT("name"),Label);}FSample S;S.Label=Label.IsEmpty()?FString::Printf(TEXT("sample_%d"),I):Label;
        auto Input=O->HasField(TEXT("input"))?O->TryGetField(TEXT("input")):O->TryGetField(TEXT("position"));
        if(Input.IsValid()){if(!VectorValue(Input,S.Input)){W.Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("Sample '%s' input must be a vector array or object"),*S.Label)));continue;}}
        else{double XYZ[3]={0,0,0};for(int32 J=0;J<3;++J){O->TryGetNumberField(J==0?TEXT("x"):(J==1?TEXT("y"):TEXT("z")),XYZ[J]);}S.Input=FVector(XYZ[0],XYZ[1],XYZ[2]);}
        if(!Safe(S.Label)||S.Input.ContainsNaN()||FMath::Abs(S.Input.GetMax())>TNumericLimits<float>::Max()||FMath::Abs(S.Input.GetMin())>TNumericLimits<float>::Max()){W.Errors.Add(MakeShared<FJsonValueString>(TEXT("Grid sample label/input exceeds finite bounds")));continue;}R.Add(S);
    }
    return R;
}
TArray<TSharedPtr<FJsonValue>> Axes(UBlendSpace* A)
{
    TArray<TSharedPtr<FJsonValue>> R;for(int32 I=0;I<3;++I){const auto P=A->GetBlendParameter(I);auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("index"),I);O->SetStringField(TEXT("display_name"),P.DisplayName);O->SetNumberField(TEXT("min"),P.Min);O->SetNumberField(TEXT("max"),P.Max);O->SetNumberField(TEXT("grid_num"),P.GridNum);O->SetNumberField(TEXT("grid_size"),P.GetGridSize());O->SetBoolField(TEXT("snap_to_grid"),P.bSnapToGrid);O->SetBoolField(TEXT("wrap_input"),P.bWrapInput);R.Add(MakeShared<FJsonValueObject>(O));}return R;
}
TArray<TSharedPtr<FJsonValue>> AssetSamples(UBlendSpace* A)
{
    TArray<TSharedPtr<FJsonValue>> R;const auto Copy=A->GetBlendSamples();for(int32 I=0;I<Copy.Num();++I){const auto& S=Copy[I];auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("index"),I);O->SetStringField(TEXT("animation"),S.Animation?S.Animation->GetPathName():FString());O->SetStringField(TEXT("animation_name"),S.Animation?S.Animation->GetName():FString());O->SetObjectField(TEXT("sample_value"),InputJson(S.SampleValue));O->SetNumberField(TEXT("rate_scale"),S.RateScale);O->SetBoolField(TEXT("use_single_frame_for_blending"),S.bUseSingleFrameForBlending);O->SetNumberField(TEXT("frame_index_to_sample"),S.FrameIndexToSample);R.Add(MakeShared<FJsonValueObject>(O));}return R;
}
TSharedRef<FJsonObject> SampleJson(const FSample& S,int32 I){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("index"),I);O->SetStringField(TEXT("label"),S.Label);O->SetObjectField(TEXT("input"),InputJson(S.Input));return O;}
bool Targets(const FString& SinglePath,const TSharedRef<FJsonObject>& ProjectionObject,const TSharedPtr<FJsonValue>& GlobalSamples,bool Extremes,TArray<FTarget>& Out,FWork& W)
{
    auto Add=[&](FString Path,const FString& Name,TArray<FSample> Samples,int32 I)
    {
        Path.TrimStartAndEndInline();if(Path.IsEmpty()){W.Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("BlendSpace target %d has no path"),I)));return W.Base();}
        auto* Asset=Cast<UBlendSpace>(Load(Path,W));if(!W.Base()){return false;}if(!Asset){W.Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("BlendSpace not found or not a UBlendSpace: %s"),*Path)));return true;}if(!AssetReady(Asset,W)){return false;}
        if(Samples.IsEmpty())
        {
            const auto Copy=Asset->GetBlendSamples();for(int32 J=0;J<Copy.Num();++J){Samples.Add({Copy[J].Animation?Copy[J].Animation->GetName():FString::Printf(TEXT("asset_sample_%d"),J),Copy[J].SampleValue});}
            if(Extremes){const auto X=Asset->GetBlendParameter(0),Y=Asset->GetBlendParameter(1);Samples.Add({TEXT("axis_min"),FVector(X.Min,Y.Min,0)});Samples.Add({TEXT("axis_center"),FVector((static_cast<double>(X.Min)+X.Max)*0.5,(static_cast<double>(Y.Min)+Y.Max)*0.5,0)});Samples.Add({TEXT("axis_max"),FVector(X.Max,Y.Max,0)});}
        }
        if(Samples.Num()>MaxSamples){return W.Fail(TEXT("sample_bound"),TEXT("Default/requested BlendSpace samples exceed256"));}FTarget T;T.Name=Name.IsEmpty()?Asset->GetName():Name;T.Path=Asset->GetPathName();T.Asset=Asset;T.Samples=MoveTemp(Samples);if(!Safe(T.Name)){return W.Fail(TEXT("name_bound"),TEXT("BlendSpace target name exceeds text bound"));}if(T.Samples.IsEmpty()){W.Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("BlendSpace '%s' has no sample inputs to evaluate"),*T.Name)));}Out.Add(MoveTemp(T));return W.Base();
    };
    const auto Values=ProjectionObject->TryGetField(TEXT("blendspaces"));if(Values.IsValid()&&Values->Type==EJson::Array&&!Values->AsArray().IsEmpty())
    {
        if(Values->AsArray().Num()>MaxTargets){return W.Fail(TEXT("target_bound"),TEXT("BlendSpace target list exceeds64"));}
        for(int32 I=0;I<Values->AsArray().Num();++I)
        {
            auto V=Values->AsArray()[I];if(V->Type==EJson::String){if(!Add(V->AsString(),FString(),{},I)){return false;}continue;}
            if(V->Type!=EJson::Object){W.Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("blendspaces[%d] must be a string path or object"),I)));continue;}auto O=V->AsObject();FString Path,Name;if(!O->TryGetStringField(TEXT("path"),Path)&&!O->TryGetStringField(TEXT("blendspace_path"),Path)){O->TryGetStringField(TEXT("blendspace"),Path);}O->TryGetStringField(TEXT("name"),Name);auto Samples=ParseSamples(O->TryGetField(TEXT("samples")),W);if(!W.Base()||!Add(Path,Name,MoveTemp(Samples),I)){return false;}
        }
        return !Out.IsEmpty();
    }
    FString Name;ProjectionObject->TryGetStringField(TEXT("blendspace_name"),Name);auto Samples=ParseSamples(GlobalSamples,W);return W.Base()&&Add(SinglePath,Name,MoveTemp(Samples),0)&&!Out.IsEmpty();
}
bool SelectWorld(bool Prefer,bool Require,FWork& W)
{
    TArray<UWorld*> Worlds;auto Add=[&](UWorld* A){if(A){Worlds.AddUnique(A);}};auto Games=[&](){if(GEngine){for(const auto& C:GEngine->GetWorldContexts()){UWorld* A=C.World();if(A&&(A->WorldType==EWorldType::PIE||A->WorldType==EWorldType::Game||A->WorldType==EWorldType::GamePreview)){Add(A);}}}};
    W.SDK(TEXT("world_context_discovery_calls"));if(Prefer){Games();}if(!Require&&GEditor){Add(GEditor->GetEditorWorldContext().World());}if(!Prefer){Games();}if(Worlds.IsEmpty()){return W.Fail(TEXT("world_missing"),Require?TEXT("No active PIE/SIE/play world available for BlendSpace runtime pose grid"):TEXT("No candidate editor/PIE world available for BlendSpace runtime pose grid"));}if(Worlds.Num()>64||!W.Hold(Worlds[0])){return W.Fail(TEXT("world_bound"),TEXT("Candidate world inventory exceeds64 or is unavailable"));}W.World.Reset(Worlds[0]);W.Type=Worlds[0]->WorldType;W.WorldDirtyBefore=Worlds[0]->GetPackage()->IsDirty();return W.Base();
}
bool Spawn(const FString& Label,FWork& W)
{
    if(!W.Base()){return false;}FActorSpawnParameters P;P.Name=MakeUniqueObjectName(W.World.Get(),ASkeletalMeshActor::StaticClass(),FName(*Label));P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    W.Result.bMutationAttempted=true;W.SDK(TEXT("spawn_actor_calls"));W.Actor.Reset(W.World->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(),FVector(0,0,120),FRotator::ZeroRotator,P));W.Count(TEXT("spawn_actor_returns"));W.Spawned=W.Actor.IsValid();if(!W.Base()||!W.Spawned){return W.Fail(TEXT("spawn_failed"),TEXT("Failed to spawn transient SkeletalMeshActor for BlendSpace pose grid"));}
    if(!Live(W.Actor.Get())||!Live(W.Actor->GetPackage())){return W.Fail(TEXT("spawned_actor_unreadable"),TEXT("Spawn returned an unreadable actor; no automatic cleanup/retry"));}W.Result.Data->SetStringField(TEXT("returned_actor_path"),W.Actor->GetPathName());
    W.Component.Reset(W.Actor->GetSkeletalMeshComponent());if(!Live(W.Component.Get())||!Live(W.Component->GetPackage())){return W.Fail(TEXT("component_missing"),TEXT("BlendSpace runtime pose-grid actor has no SkeletalMeshComponent"));}W.RuntimeIds.Add(MakeUnique<FIdentity>(W.Actor.Get()));W.RuntimeIds.Add(MakeUnique<FIdentity>(W.Component.Get()));W.Result.Data->SetStringField(TEXT("returned_component_path"),W.Component->GetPathName());
    // Configuration intentionally changes mesh/animation identity before the admitted steady-state read/tick checks.
    W.SDK(TEXT("set_actor_label_calls"));W.Actor->SetActorLabel(Label);if(!W.Runtime(false,false,false)){return false;}W.SDK(TEXT("set_skeletal_mesh_calls"));W.Component->SetSkeletalMesh(W.Mesh.Get());if(!W.Runtime(false)){return false;}
    W.SDK(TEXT("set_enable_animation_calls"));W.Component->SetEnableAnimation(true);W.SDK(TEXT("set_component_tick_enabled_calls"));W.Component->SetComponentTickEnabled(true);W.SDK(TEXT("set_update_animation_in_editor_calls"));W.Component->SetUpdateAnimationInEditor(true);if(!W.Runtime(false)){return false;}
    W.Component->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;W.Count(TEXT("visibility_tick_option_assignments"));W.SDK(TEXT("set_animation_mode_calls"));W.Component->SetAnimationMode(EAnimationMode::AnimationSingleNode,true);if(!W.Runtime(false)){return false;}W.SDK(TEXT("initial_refresh_bone_transforms_calls"));W.Component->RefreshBoneTransforms();W.Count(TEXT("initial_refresh_bone_transforms_returns"));return W.CaptureParticipants();
}
TSharedRef<FJsonObject> Metadata(FWork& W,bool Actor)
{
    auto O=MakeShared<FJsonObject>();if(!W.Runtime()){return O;}UObject* Value=Actor?static_cast<UObject*>(W.Actor.Get()):static_cast<UObject*>(W.Component.Get());O->SetStringField(TEXT("name"),Value->GetName());O->SetStringField(TEXT("path"),Value->GetPathName());O->SetStringField(TEXT("class"),Value->GetClass()->GetPathName());
    if(Actor){W.SDK(TEXT("actor_label_calls"));O->SetStringField(TEXT("label"),W.Actor->GetActorLabel());W.Runtime();}
    else{O->SetStringField(TEXT("skeletal_mesh"),W.Mesh->GetPathName());UClass* Class=W.Component->GetAnimClass();if(Class&&!W.Hold(Class)){return O;}O->SetStringField(TEXT("anim_class"),Class?Class->GetPathName():FString());O->SetStringField(TEXT("animation_mode"),TEXT("AnimationSingleNode"));const auto T=W.Component->GetComponentTransform();if(T.ContainsNaN()||!T.GetRotation().IsNormalized()){W.Fail(TEXT("transform_invalid"),TEXT("Component transform is invalid"));return O;}O->SetObjectField(TEXT("component_transform"),TransformJson(T));}return O;
}
TSharedRef<FJsonObject> Tick(FWork& W,int32 Count,double Delta,bool Refresh,TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("tick_method"),TEXT("USkeletalMeshComponent::TickAnimation"));O->SetNumberField(TEXT("requested_tick_count"),Count);O->SetNumberField(TEXT("tick_delta_time"),Delta);O->SetBoolField(TEXT("refresh_bone_transforms"),Refresh);int32 Done=0;
    for(int32 I=0;I<Count;++I)
    {if(W.Ticks>=MaxTicks||!W.Runtime()){W.Fail(TEXT("tick_boundary"),TEXT("Finite synchronous tick budget/readiness boundary reached; partial prefix retained"));break;}W.SDK(TEXT("tick_animation_calls"));W.Component->TickAnimation(static_cast<float>(Delta),false);W.Count(TEXT("tick_animation_returns"));++W.Ticks;++Done;if(!W.Runtime()){break;}if(Refresh){W.SDK(TEXT("refresh_bone_transforms_calls"));W.Component->RefreshBoneTransforms();W.Count(TEXT("refresh_bone_transforms_returns"));if(!W.Runtime()){break;}}}
    if(W.Runtime()&&Count>0&&!W.Component->ShouldTickAnimation()){Warnings.Add(MakeShared<FJsonValueString>(TEXT("SkeletalMeshComponent::ShouldTickAnimation returned false; forced TickAnimation was still called for the probe")));}O->SetNumberField(TEXT("executed_tick_count"),Done);O->SetBoolField(TEXT("success"),W.Result.Error.IsEmpty());return O;
}
bool ReadTransform(const TSharedPtr<FJsonObject>& Row,const TCHAR* Field,FTransform& Out)
{
    const TSharedPtr<FJsonObject>* J=nullptr;if(!Row.IsValid()||!Row->TryGetObjectField(Field,J)){return false;}const TArray<TSharedPtr<FJsonValue>> *T=nullptr,*Q=nullptr,*S=nullptr;if(!(*J)->TryGetArrayField(TEXT("translation"),T)||!(*J)->TryGetArrayField(TEXT("rotation"),Q)||!(*J)->TryGetArrayField(TEXT("scale"),S)||T->Num()!=3||Q->Num()!=4||S->Num()!=3){return false;}double V[10];int32 I=0;for(const auto* A:{T,Q,S}){for(const auto& X:*A){if(!X.IsValid()||X->Type!=EJson::Number||!X->TryGetNumber(V[I])||!FMath::IsFinite(V[I])){return false;}++I;}}Out=FTransform(FQuat(V[3],V[4],V[5],V[6]),FVector(V[0],V[1],V[2]),FVector(V[7],V[8],V[9]));return !Out.ContainsNaN()&&Out.GetRotation().IsNormalized();
}
TSharedRef<FJsonObject> DeltaRow(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After)
{
    auto O=MakeShared<FJsonObject>();bool BV=false,AV=false;Before->TryGetBoolField(TEXT("valid"),BV);After->TryGetBoolField(TEXT("valid"),AV);O->SetBoolField(TEXT("valid"),BV&&AV);if(!BV||!AV){FString Error;if(Before->TryGetStringField(TEXT("error"),Error)){O->SetStringField(TEXT("pre_error"),Error);}if(After->TryGetStringField(TEXT("error"),Error)){O->SetStringField(TEXT("post_error"),Error);}return O;}
    FTransform BW,AW,BC,AC;if(!ReadTransform(Before,TEXT("world"),BW)||!ReadTransform(After,TEXT("world"),AW)||!ReadTransform(Before,TEXT("component"),BC)||!ReadTransform(After,TEXT("component"),AC)){O->SetBoolField(TEXT("valid"),false);O->SetStringField(TEXT("error"),TEXT("Malformed finite transform in captured pose"));return O;}
    const FVector WD=AW.GetTranslation()-BW.GetTranslation(),CD=AC.GetTranslation()-BC.GetTranslation();O->SetArrayField(TEXT("world_translation_delta"),VectorArray(WD));O->SetNumberField(TEXT("world_translation_distance"),WD.Size());O->SetNumberField(TEXT("world_rotation_angle_degrees"),FMath::RadiansToDegrees(BW.GetRotation().AngularDistance(AW.GetRotation())));O->SetArrayField(TEXT("component_translation_delta"),VectorArray(CD));O->SetNumberField(TEXT("component_translation_distance"),CD.Size());O->SetNumberField(TEXT("component_rotation_angle_degrees"),FMath::RadiansToDegrees(BC.GetRotation().AngularDistance(AC.GetRotation())));return O;
}
TSharedRef<FJsonObject> Deltas(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,const TArray<FString>& Bones,const TArray<FString>& Sockets)
{
    auto O=MakeShared<FJsonObject>();for(const TCHAR* Kind:{TEXT("bone"),TEXT("socket")})
    {auto Values=MakeShared<FJsonObject>();const FString Key=FString(Kind)+TEXT("_samples");const TSharedPtr<FJsonObject> *B=nullptr,*A=nullptr;if(Before->TryGetObjectField(Key,B)&&After->TryGetObjectField(Key,A)){for(const FString& Name:FString(Kind)==TEXT("bone")?Bones:Sockets){const TSharedPtr<FJsonObject> *BR=nullptr,*AR=nullptr;if(!Name.IsEmpty()&&(*B)->TryGetObjectField(Name,BR)&&(*A)->TryGetObjectField(Name,AR)){Values->SetObjectField(Name,DeltaRow(*BR,*AR));}}}O->SetObjectField(FString(Kind)+TEXT("_deltas"),Values);}return O;
}
TSharedRef<FJsonObject> Capture(FWork& W,const FString& BonesJson,const FString& SocketsJson,bool AllowMissing,TArray<TSharedPtr<FJsonValue>>& Errors,TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    auto O=MakeShared<FJsonObject>();if(!W.Runtime()){return O;}W.SDK(TEXT("skeletal_bone_sampling_calls"));const auto Native=SkeletalBoneSampling::Sample(FString(),FString(),W.Actor->GetPathName(),W.Component->GetName(),BonesJson,SocketsJson,true,W.Type!=EWorldType::Editor);W.Count(TEXT("skeletal_bone_sampling_returns"));
    W.Result.Data->SetObjectField(TEXT("last_bone_sampling_native_data"),Native.Data);if(!W.Runtime()){return O;}if(!Native.bSuccess||!Native.bPostconditionPassed){W.Fail(TEXT("pose_observation_failed"),Native.Error.IsEmpty()?TEXT("Shared current bone/socket observation failed"):Native.Error);return O;}
    const TSharedPtr<FJsonObject>* L=nullptr;if(!Native.Data->TryGetObjectField(TEXT("legacy_result"),L)){W.Fail(TEXT("pose_projection_failed"),TEXT("Shared sampler omitted its result"));return O;}const TSharedPtr<FJsonObject> *Actor=nullptr,*Component=nullptr;if(!(*L)->TryGetObjectField(TEXT("actor"),Actor)||!(*L)->TryGetObjectField(TEXT("component"),Component)||(*Actor)->GetStringField(TEXT("path"))!=W.Actor->GetPathName()||(*Component)->GetStringField(TEXT("path"))!=W.Component->GetPathName()){W.Fail(TEXT("pose_target_changed"),TEXT("Shared sampler selected a different actor/component"));return O;}
    for(const TCHAR* Key:{TEXT("bone_samples"),TEXT("socket_samples")}){const TSharedPtr<FJsonObject>* Values=nullptr;if(!(*L)->TryGetObjectField(Key,Values)){W.Fail(TEXT("pose_projection_failed"),TEXT("Shared sampler omitted bone/socket samples"));return O;}O->SetObjectField(Key,*Values);}
    const TArray<TSharedPtr<FJsonValue>>* Messages=nullptr;if(!(*L)->TryGetArrayField(TEXT("warnings"),Messages)){W.Fail(TEXT("pose_projection_failed"),TEXT("Shared sampler omitted its ordered warning prefix"));return O;}
    for(const auto& Message:*Messages){const FString Error=Message->AsString();const bool Missing=Error.StartsWith(TEXT("Bone not found: "))||Error.StartsWith(TEXT("Socket not found: "));((Missing&&!AllowMissing)?Errors:Warnings).Add(Message);}
    return O;
}
TArray<TSharedPtr<FJsonValue>> Weights(UBlendSpace* A,const FVector& Input,FWork& W,TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    TArray<TSharedPtr<FJsonValue>> R;if(!AssetReady(A,W)||!W.Runtime()){return R;}TArray<FBlendSampleData> Values;int32 Cache=INDEX_NONE;W.SDK(TEXT("get_samples_from_blend_input_calls"));const bool Found=A->GetSamplesFromBlendInput(Input,Values,Cache,true);W.Count(TEXT("get_samples_from_blend_input_returns"));if(!W.Runtime()||!AssetReady(A,W)){return R;}if(!Found){Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("BlendSpace '%s' returned no weighted samples for input [%s]"),*A->GetName(),*Input.ToString())));return R;}if(Values.Num()>MaxRows){W.Fail(TEXT("weight_bound"),TEXT("Weighted sample projection exceeds4096"));return R;}
    const auto Samples=A->GetBlendSamples();for(const auto& V:Values){auto O=MakeShared<FJsonObject>();const float Weight=V.GetClampedWeight();if(!FMath::IsFinite(Weight)){W.Fail(TEXT("weight_invalid"),TEXT("SDK weight is nonfinite"));return R;}O->SetNumberField(TEXT("sample_data_index"),V.SampleDataIndex);O->SetNumberField(TEXT("weight"),Weight);if(Samples.IsValidIndex(V.SampleDataIndex)){const auto& S=Samples[V.SampleDataIndex];O->SetStringField(TEXT("animation"),S.Animation?S.Animation->GetPathName():FString());O->SetStringField(TEXT("animation_name"),S.Animation?S.Animation->GetName():FString());O->SetObjectField(TEXT("sample_value"),InputJson(S.SampleValue));}R.Add(MakeShared<FJsonValueObject>(O));}return R;
}
}
Variable::FResult Sample(const FString& SkeletalMesh,const FString& BlendSpacePath,const FString& BlendSpacesJson,const FString& SamplesJson,const FString& SampleBonesJson,const FString& SampleSocketsJson,const FString& ActorLabel,double SampleTime,int64 SettleTickCount,int64 TickCount,double TickDeltaTime,bool bIncludeAxisExtremes,bool bAllowMissingBones,bool bRefreshBoneTransforms,bool bCleanup,bool bPreferPIEWorld,bool bRequirePIEWorld)
{
    FWork W(bCleanup);if(!IsEditorGameThread()||IsEngineExitRequested()||Running){W.Fail(TEXT("editor_boundary"),TEXT("Nonreentrant Editor game thread required; PIE/SIE is supported"));return W.Finish();}TGuardValue<bool> Guard(Running,true);
    if(!Safe(SkeletalMesh)||!Safe(BlendSpacePath)||!Safe(ActorLabel,NAME_SIZE-1)||!FMath::IsFinite(SampleTime)||!FMath::IsFinite(TickDeltaTime)){W.Fail(TEXT("input_invalid"),TEXT("Text/name or time input exceeds finite safety bounds"));return W.Finish();}
    TSharedPtr<FJsonObject> BS,BP,SP;TSharedPtr<FJsonValue> SV;if(!Projection(BlendSpacesJson,{TEXT("blendspaces"),TEXT("blendspace_name")},BS)||!Projection(SampleBonesJson,{TEXT("sample_bones"),TEXT("bones")},BP)||!Projection(SampleSocketsJson,{TEXT("sample_sockets"),TEXT("sockets")},SP)||!Decode(SamplesJson,SV)){W.Fail(TEXT("carrier_invalid"),TEXT("Expected exact projection objects3 and AnyJSON samples1 within1MiUTF16/depth64/65536slots/noNUL"));return W.Finish();}
    TArray<FString> DefaultBones={TEXT("root"),TEXT("pelvis"),TEXT("spine_03"),TEXT("head"),TEXT("foot_l"),TEXT("foot_r")};const auto Bones=Names(BP->TryGetField(TEXT("sample_bones")),Names(BP->TryGetField(TEXT("bones")),DefaultBones));const auto Sockets=Names(SP->TryGetField(TEXT("sample_sockets")),Names(SP->TryGetField(TEXT("sockets")),{}));
    if(Bones.Num()>MaxRows||Sockets.Num()>MaxRows){W.Fail(TEXT("name_bound"),TEXT("Effective sample names exceed4096"));return W.Finish();}for(const auto* NamesArray:{&Bones,&Sockets}){for(const FString& Name:*NamesArray){if(!Safe(Name,NAME_SIZE-1)){W.Fail(TEXT("name_bound"),TEXT("Decoded sample name exceeds FName/NUL boundary"));return W.Finish();}}}
    const FString BC=JsonValue(MakeShared<FJsonValueArray>(Strings(Bones))),SC=JsonValue(MakeShared<FJsonValueArray>(Strings(Sockets)));
    const int32 Settle=static_cast<int32>(FMath::Clamp<int64>(SettleTickCount,0,240)),Ticks=static_cast<int32>(FMath::Clamp<int64>(TickCount,0,240));const double Delta=FMath::Clamp(TickDeltaTime,0.0,1.0),Time=FMath::Clamp(SampleTime,0.0,3600.0);
    W.Result.Stage=TEXT("parse_load_blendspace_targets");TArray<FTarget> TS;if(!Targets(BlendSpacePath,BS.ToSharedRef(),SV,bIncludeAxisExtremes,TS,W)){if(W.Result.Error.IsEmpty()){W.Fail(TEXT("targets_missing"),TEXT("No valid BlendSpace targets were provided"));}return W.Finish();}
    int64 TotalSamples=0,AssetRows=0;for(const auto& T:TS){TotalSamples+=T.Samples.Num();AssetRows+=T.Asset->GetBlendSamples().Num();}if(TotalSamples>MaxSamples||AssetRows>8192||TotalSamples*(Bones.Num()+Sockets.Num())>4096||TotalSamples*(Settle+Ticks)>MaxTicks){W.Fail(TEXT("work_bound"),TEXT("Runtime grid exceeds256samples/8192assetrows/4096pose-name slots/4096synchronous ticks"));return W.Finish();}
    W.Result.Stage=TEXT("load_skeletal_mesh");if(SkeletalMesh.TrimStartAndEnd().IsEmpty()){W.Fail(TEXT("skeletal_mesh_missing"),TEXT("sample_blendspace_runtime_pose_grid requires 'skeletal_mesh'"));return W.Finish();}W.Mesh.Reset(Cast<USkeletalMesh>(Load(SkeletalMesh,W)));if(!W.Mesh.IsValid()){if(W.Result.Error.IsEmpty()){W.Fail(TEXT("skeletal_mesh_missing"),FString::Printf(TEXT("SkeletalMesh not found: %s"),*SkeletalMesh));}return W.Finish();}
    W.SDK(TEXT("mesh_is_compiling_calls"));const bool MeshPending=W.Mesh->IsCompiling();if(!W.Base()||MeshPending){W.Fail(TEXT("mesh_compilation_pending"),TEXT("SkeletalMesh compilation is active; no explicit completion or actor spawn"));return W.Finish();}
    W.Result.Stage=TEXT("select_probe_world");if(!SelectWorld(bPreferPIEWorld,bRequirePIEWorld,W)){return W.Finish();}
    auto L=W.Legacy;L->SetBoolField(TEXT("read_only"),false);L->SetBoolField(TEXT("runtime_only"),true);L->SetBoolField(TEXT("asset_modified"),false);L->SetBoolField(TEXT("saves_assets"),false);L->SetBoolField(TEXT("starts_sie"),false);L->SetBoolField(TEXT("cleanup"),bCleanup);L->SetStringField(TEXT("runtime_source"),TEXT("transient_single_node_blendspace_component"));L->SetStringField(TEXT("runtime_note"),TEXT("Spawns a transient SkeletalMeshActor in an active PIE/SIE/play world by default, evaluates AnimationSingleNode BlendSpace inputs through UAnimSingleNodeInstance, samples requested bones/sockets, then optionally destroys the actor. It does not start SIE and does not save assets."));L->SetStringField(TEXT("skeletal_mesh"),W.Mesh->GetPathName());L->SetStringField(TEXT("sampled_world_type"),WorldType(W.World.Get()));L->SetStringField(TEXT("sampled_world_name"),W.World->GetName());L->SetBoolField(TEXT("prefer_pie_world"),bPreferPIEWorld);L->SetBoolField(TEXT("require_pie_world"),bRequirePIEWorld);L->SetBoolField(TEXT("is_play_session_active"),GEditor&&GEditor->GetPIEWorldContext()!=nullptr);L->SetNumberField(TEXT("sample_time"),Time);L->SetNumberField(TEXT("settle_tick_count"),Settle);L->SetNumberField(TEXT("tick_count"),Ticks);L->SetNumberField(TEXT("tick_delta_time"),Delta);L->SetBoolField(TEXT("refresh_bone_transforms"),bRefreshBoneTransforms);L->SetBoolField(TEXT("allow_missing_bones"),bAllowMissingBones);L->SetArrayField(TEXT("requested_sample_bones"),Strings(Bones));L->SetArrayField(TEXT("requested_sample_sockets"),Strings(Sockets));L->SetNumberField(TEXT("blendspace_count"),TS.Num());W.Result.Data->SetBoolField(TEXT("legacy_asset_modified_is_no_explicit_asset_edit"),true);W.Result.Data->SetBoolField(TEXT("evaluated_frame_atomicity_verified"),false);W.Result.Data->SetBoolField(TEXT("synchronous_tick_interruptible"),false);
    if(!W.Errors.IsEmpty()){W.Fail(TEXT("target_parse_failed"),TEXT("One or more BlendSpace target/sample definitions failed; no actor spawned"));return W.Finish();}
    W.Result.Stage=TEXT("spawn_configure_probe");if(!Spawn(ActorLabel,W)){return W.Finish();}L->SetObjectField(TEXT("probe_actor"),Metadata(W,true));L->SetObjectField(TEXT("probe_component"),Metadata(W,false));if(!W.Runtime()){return W.Finish();}
    for(const auto& T:TS)
    {
        if(!W.Runtime()||!AssetReady(T.Asset,W)){return W.Finish();}
        auto Target=MakeShared<FJsonObject>(),Configure=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Samples,Top,Requested;auto Baseline=MakeShared<FJsonObject>();bool HasBaseline=false;int32 Valid=0;double MaxDistance=0;
        Target->SetStringField(TEXT("name"),T.Name);Target->SetStringField(TEXT("path"),T.Path);Target->SetStringField(TEXT("class"),T.Asset->GetClass()->GetPathName());Target->SetObjectField(TEXT("configure"),Configure);Target->SetArrayField(TEXT("axes"),Axes(T.Asset));Target->SetArrayField(TEXT("asset_samples"),AssetSamples(T.Asset));for(int32 I=0;I<T.Samples.Num();++I){Requested.Add(MakeShared<FJsonValueObject>(SampleJson(T.Samples[I],I)));}Target->SetArrayField(TEXT("requested_samples"),Requested);W.TargetResults.Add(MakeShared<FJsonValueObject>(Target));
        W.Result.Stage=TEXT("configure_blendspace_target");if(!W.Runtime()||!AssetReady(T.Asset,W)){return W.Finish();}W.AnimIds.Reset();W.Participants.Reset();W.HeldParticipants.Reset();W.Instance.Reset();
        W.SDK(TEXT("set_animation_mode_calls"));W.Component->SetAnimationMode(EAnimationMode::AnimationSingleNode,true);if(!W.Runtime(false)){return W.Finish();}W.SDK(TEXT("set_animation_calls"));W.Component->SetAnimation(T.Asset);if(!W.Runtime(false)){return W.Finish();}W.SDK(TEXT("play_calls"));W.Component->Play(false);if(!W.CaptureParticipants()){return W.Finish();}
        W.SDK(TEXT("set_animation_asset_calls"));W.Instance->SetAnimationAsset(T.Asset,true,0);if(!W.Runtime()){return W.Finish();}W.SDK(TEXT("set_looping_calls"));W.Instance->SetLooping(true);W.SDK(TEXT("set_play_rate_calls"));W.Instance->SetPlayRate(0);W.SDK(TEXT("set_playing_calls"));W.Instance->SetPlaying(false);if(!W.Runtime()){return W.Finish();}
        auto InstanceJson=MakeShared<FJsonObject>();InstanceJson->SetStringField(TEXT("name"),W.Instance->GetName());InstanceJson->SetStringField(TEXT("path"),W.Instance->GetPathName());InstanceJson->SetStringField(TEXT("class"),W.Instance->GetClass()->GetPathName());InstanceJson->SetStringField(TEXT("outer"),W.Instance->GetOuter()->GetPathName());Configure->SetObjectField(TEXT("single_node_instance"),InstanceJson);
        for(int32 I=0;I<T.Samples.Num();++I)
        {
            const auto& S=T.Samples[I];TArray<TSharedPtr<FJsonValue>> Errors,Warnings;auto Row=SampleJson(S,I);Samples.Add(MakeShared<FJsonValueObject>(Row));Target->SetArrayField(TEXT("samples"),Samples);Row->SetStringField(TEXT("blendspace"),T.Name);Row->SetStringField(TEXT("blendspace_path"),T.Path);Row->SetNumberField(TEXT("sample_time"),Time);
            W.Result.Stage=TEXT("set_blendspace_sample_input");if(!W.Runtime()||!AssetReady(T.Asset,W)){return W.Finish();}W.SDK(TEXT("set_animation_asset_calls"));W.Instance->SetAnimationAsset(T.Asset,true,0);if(!W.Runtime()){return W.Finish();}W.SDK(TEXT("set_blendspace_position_calls"));W.Instance->SetBlendSpacePosition(S.Input);if(!W.Runtime()){return W.Finish();}W.SDK(TEXT("set_instance_position_calls"));W.Instance->SetPosition(static_cast<float>(Time),false);if(!W.Runtime()){return W.Finish();}W.SDK(TEXT("set_playing_calls"));W.Instance->SetPlaying(false);W.SDK(TEXT("set_component_position_calls"));W.Component->SetPosition(static_cast<float>(Time),false);if(!W.Runtime()){return W.Finish();}
            W.Result.Stage=TEXT("settle_tick_sample");Row->SetObjectField(TEXT("settle_tick"),Tick(W,Settle,Delta,bRefreshBoneTransforms,Warnings));Row->SetArrayField(TEXT("warnings"),Warnings);if(!W.Runtime()){return W.Finish();}W.Result.Stage=TEXT("tick_sample");Row->SetObjectField(TEXT("tick"),Tick(W,Ticks,Delta,bRefreshBoneTransforms,Warnings));Row->SetArrayField(TEXT("warnings"),Warnings);if(!W.Runtime()){return W.Finish();}
            W.Result.Stage=TEXT("capture_sample_pose");auto Pose=Capture(W,BC,SC,bAllowMissingBones,Errors,Warnings);Row->SetObjectField(TEXT("pose"),Pose);Row->SetArrayField(TEXT("errors"),Errors);Row->SetArrayField(TEXT("warnings"),Warnings);if(!W.Runtime()){return W.Finish();}W.Result.Stage=TEXT("sample_weighted_sources");Row->SetArrayField(TEXT("weighted_samples"),Weights(T.Asset,S.Input,W,Warnings));Row->SetArrayField(TEXT("warnings"),Warnings);if(!W.Runtime()){return W.Finish();}const bool PoseValid=Errors.IsEmpty();Row->SetBoolField(TEXT("pose_valid"),PoseValid);if(PoseValid){++Valid;}const bool NewBaseline=!HasBaseline&&PoseValid;if(NewBaseline){Baseline=Pose;HasBaseline=true;}Row->SetBoolField(TEXT("is_blendspace_baseline"),NewBaseline);
            if(HasBaseline)
            {
                auto D=Deltas(Baseline,Pose,Bones,Sockets);Row->SetObjectField(TEXT("delta_from_blendspace_first"),D);const TSharedPtr<FJsonObject>* BoneDelta=nullptr;if(D->TryGetObjectField(TEXT("bone_deltas"),BoneDelta)){for(const FString& Bone:Bones){const TSharedPtr<FJsonObject>* B=nullptr;if((*BoneDelta)->TryGetObjectField(Bone,B)){double Distance=0;if((*B)->TryGetNumberField(TEXT("world_translation_distance"),Distance)){MaxDistance=FMath::Max(MaxDistance,Distance);auto Entry=MakeShared<FJsonObject>();Entry->SetStringField(TEXT("sample"),S.Label);Entry->SetStringField(TEXT("bone"),Bone);Entry->SetNumberField(TEXT("world_translation_distance"),Distance);Top.Add(MakeShared<FJsonValueObject>(Entry));}}}}
            }
            W.Errors.Append(Errors);W.Warnings.Append(Warnings);
        }
        Top.Sort([](const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B){return A->AsObject()->GetNumberField(TEXT("world_translation_distance"))>B->AsObject()->GetNumberField(TEXT("world_translation_distance"));});if(Top.Num()>10){Top.SetNum(10);}auto Summary=MakeShared<FJsonObject>();Summary->SetNumberField(TEXT("sample_count"),T.Samples.Num());Summary->SetNumberField(TEXT("valid_pose_count"),Valid);Summary->SetNumberField(TEXT("max_location_delta_from_first"),MaxDistance);Summary->SetBoolField(TEXT("input_changed_pose"),MaxDistance>0.01);Summary->SetArrayField(TEXT("top_location_deltas"),Top);Target->SetObjectField(TEXT("summary"),Summary);Target->SetArrayField(TEXT("samples"),Samples);
    }
    W.Result.Stage=TEXT("cleanup_probe");W.Cleanup();if(!W.Result.Error.IsEmpty()){return W.Finish();}W.Result.Stage=TEXT("validate_grid_results");if(!W.Errors.IsEmpty()){W.Fail(TEXT("sample_failed"),TEXT("One or more BlendSpace runtime pose samples failed"));return W.Finish();}W.Result.bSuccess=W.Result.bPostconditionPassed=true;W.Result.Stage=TEXT("complete");return W.Finish();
}
}
