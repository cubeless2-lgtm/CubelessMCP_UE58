#include "Compatibility/UE58/CubelessAnimationAssetMetadataApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimMetaData.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/AssetUserData.h"
#include "EditorAssetLibrary.h"
#include "Misc/FrameRate.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
namespace Cubeless::UE58::AnimationAssetMetadata
{
namespace
{

using Variable::FResult;
constexpr int32 AssetItemBudget=4096,SerializedRowBudget=16384,SourceKeyBudget=65536,PerChannelKeyBudget=16384,MetadataJsonBudget=1024*1024;
FResult* Current=nullptr;int32 SerializedRows=0,SourceKeysRead=0,PreflightItems=0;TArray<TStrongObjectPtr<UObject>> Held;TSet<const UObject*> HeldObjects;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool InputTextSafe(const FString& S,int32 Limit) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(const TCHAR* Key,int32 N=1)
{ if(Current) { double V=0;auto C=Current->Data->GetObjectField(TEXT("native_call_counts"));C->TryGetNumberField(Key,V);C->SetNumberField(Key,V+N); } }
void Unsafe(const FString& Error)
{ if(Current) { Current->Data->SetBoolField(TEXT("observation_incomplete"),true);Current->ErrorCode=TEXT("animation_metadata_observation_incomplete");Current->Error=Error; } }
bool Incomplete() { bool V=false;return Current&&Current->Data->TryGetBoolField(TEXT("observation_incomplete"),V)&&V; }
bool Row() { if(Incomplete()) { return false; }if(++SerializedRows>SerializedRowBudget) { Unsafe(TEXT("Selected serialization exceeds16384 rows"));return false; }return true; }
bool Hold(const UObject* O)
{ if(!O) { return true; }if(!Loaded(O)||!Loaded(O->GetClass())||!InputTextSafe(O->GetPathName(),65536)) { Unsafe(TEXT("Current referenced object/class unavailable or outside text bound"));return false; }if(!HeldObjects.Contains(O)) { if(HeldObjects.Num()>=32768) { Unsafe(TEXT("Strong current reference inventory exceeds32768"));return false; }HeldObjects.Add(O);Held.Emplace(const_cast<UObject*>(O)); }return true; }
bool Finite(double V) { if(!FMath::IsFinite(V)) { Unsafe(TEXT("Authored numeric metadata is nonfinite"));return false; }return true; }
bool Small(int32 N) { if(N>AssetItemBudget) { Unsafe(TEXT("Source metadata array exceeds4096 items"));return false; }return true; }
bool Frame(const FFrameRate& V) { return V.Denominator!=0&&Finite(V.AsDecimal()); }
bool CurveSafe(const FRichCurve& C,int32& KeyCount)
{
    const int32 N=C.GetNumKeys();if(N>PerChannelKeyBudget||N>SourceKeyBudget-KeyCount) { Unsafe(TEXT("Whole curve key copy exceeds finite16384/channel or65536 aggregate bound"));return false; }KeyCount+=N;
    Count(TEXT("source_curve_keys_checked"),N);if(!Finite(C.DefaultValue)) { return false; }
    for(const FRichCurveKey& K:C.GetConstRefOfKeys())
    { if(!Finite(K.Time)||!Finite(K.Value)||!Finite(K.ArriveTangent)||!Finite(K.LeaveTangent)||!Finite(K.ArriveTangentWeight)||!Finite(K.LeaveTangentWeight)) { return false; } }
    return true;
}
bool CurvesSafe(const TArray<FFloatCurve>& Float,const TArray<FTransformCurve>& Transform,bool Keys)
{
    if(!Small(Float.Num())||!Small(Transform.Num())) { return false; }if(!Keys) { return true; }
    for(const FFloatCurve& C:Float) { if(!CurveSafe(C.FloatCurve,SourceKeysRead)) { return false; } }
    for(const FTransformCurve& C:Transform)
    { for(int32 I=0;I<3;++I) { if(!CurveSafe(C.TranslationCurve.FloatCurves[I],SourceKeysRead)||!CurveSafe(C.RotationCurve.FloatCurves[I],SourceKeysRead)||!CurveSafe(C.ScaleCurve.FloatCurves[I],SourceKeysRead)) { return false; } } }
    return true;
}
bool ReferencesSafe(UAnimationAsset* Asset,bool IncludeCurves,bool IncludeKeys,bool IncludeMetadata)
{
    SourceKeysRead=PreflightItems=0;if(!Asset) { return true; }if(!Hold(Asset->GetSkeleton())) { return false; }
    TSet<const UAnimSequenceBase*> Path;int32 Occurrences=0;
    TFunction<bool(UAnimSequenceBase*,int32)> Visit;
    Visit=[&](UAnimSequenceBase* Base,int32 Depth)
    {
        if(!Hold(Base)||Depth>32||++Occurrences>AssetItemBudget||Path.Contains(Base)) { Unsafe(TEXT("Unusable/cyclic/deep composite animation reference graph"));return false; }
        if(!Base) { return true; }Path.Add(Base);Count(TEXT("animation_references_checked"));
        if(!Finite(Base->RateScale)||!Small(Base->Notifies.Num())||!Small(Base->AnimNotifyTracks.Num())) { return false; }
        for(const FAnimNotifyEvent& N:Base->Notifies)
        {
            if(++PreflightItems>65536) { Unsafe(TEXT("Metadata preflight exceeds65536 source items"));return false; }Count(TEXT("notify_preflight_rows"));
            if(!Hold(N.Notify.Get())||!Hold(N.NotifyStateClass.Get())||!Hold(N.GetLinkedMontage())) { return false; }
            // GetNotifyEventName constructs this exact prefixed FName even for an existing valid source name.
            if(!InputTextSafe(TEXT("AnimNotify_")+N.NotifyName.ToString(),NAME_SIZE-1)) { Unsafe(TEXT("Prefixed notify event name exceeds FName bound"));return false; }
            if(!Finite(N.GetTime())||!Finite(N.EndLink.GetTime())||!Finite(N.TriggerTimeOffset)||!Finite(N.EndTriggerTimeOffset)||!Finite(N.TriggerWeightThreshold)||!Finite(N.NotifyTriggerChance)) { return false; }
        }
        const TArray<FAnimSyncMarker>* Markers=nullptr;const TArray<FName>* Names=nullptr;
        if(auto* Sequence=Cast<UAnimSequence>(Base)) { Markers=&Sequence->AuthoredSyncMarkers;Names=&Sequence->UniqueMarkerNames; }
        auto Track=[&](const FAnimTrack& T)
        {
            if(!Small(T.AnimSegments.Num())) { return false; }
            for(const FAnimSegment& S:T.AnimSegments)
            {
                if(++PreflightItems>65536) { Unsafe(TEXT("Metadata preflight exceeds65536 source items"));return false; }Count(TEXT("segment_preflight_rows"));auto* Ref=S.GetAnimReference().Get();if(!Hold(Ref)||!Finite(S.StartPos)||!Finite(S.AnimStartTime)||!Finite(S.AnimEndTime)||!Finite(S.AnimPlayRate)) { return false; }
                if(Ref&&!Finite(Ref->RateScale)) { return false; }
                if(!Finite(S.GetLength())||!Finite(S.GetEndPos())) { return false; }
                if(Ref&&S.IsValid()&&!Visit(Ref,Depth+1)) { return false; }
            }
            return true;
        };
        if(auto* Montage=Cast<UAnimMontage>(Base))
        {
            Markers=&Montage->MarkerData.AuthoredSyncMarkers;Names=&Montage->MarkerData.UniqueMarkerNames;
            if(!Small(Montage->CompositeSections.Num())||!Small(Montage->SlotAnimTracks.Num())||!Finite(Montage->BlendOutTriggerTime)||!Finite(Montage->GetDefaultBlendInTime())||!Finite(Montage->GetDefaultBlendOutTime())) { return false; }
            for(const FCompositeSection& S:Montage->CompositeSections)
            { if(!Finite(S.GetTime())||!Small(S.GetMetaData().Num())) { return false; }for(const UAnimMetaData* M:S.GetMetaData()) { if(!Hold(M)) { return false; } } }
            for(const FSlotAnimationTrack& T:Montage->SlotAnimTracks) { if(!Track(T.AnimTrack)) { return false; } }
        }
        else if(auto* Composite=Cast<UAnimComposite>(Base)) { if(!Track(Composite->AnimationTrack)) { return false; } }
        if(Markers) { if(!Small(Markers->Num())||!Small(Names->Num())) { return false; }for(const FAnimSyncMarker& M:*Markers) { if(!Finite(M.Time)) { return false; } } }
        // SDK base sampled-key calculation rounds into int32; guard before calling it.
        const float Length=Base->GetPlayLength();const FFrameRate Rate=Base->GetSamplingFrameRate();
        if(!Finite(Length)||!Frame(Rate)||!Finite(double(Length)*Rate.AsDecimal())||double(Length)*Rate.AsDecimal()>MAX_int32-1.0||double(Length)*Rate.AsDecimal()<MIN_int32+1.0)
        { Unsafe(TEXT("Sampling length/rate outside finite frame-count scope"));return false; }
        if(auto* Sequence=Cast<UAnimSequence>(Base);Sequence&&Base->IsDataModelValid())
        {
            auto Interface=Base->GetDataModelInterface();if(!Hold(Interface.GetObject())) { return false; }auto* Model=Interface.GetInterface();
            if(Model) { if(!Frame(Model->GetFrameRate())||(IncludeCurves&&!CurvesSafe(Model->GetFloatCurves(),Model->GetTransformCurves(),IncludeKeys))) { Unsafe(TEXT("Animation DataModel metadata outside finite scope"));return false; } }
        }
        else if(IncludeCurves) { const auto& C=Base->GetCurveData();if(!CurvesSafe(C.FloatCurves,C.TransformCurves,IncludeKeys)) { return false; } }
        Path.Remove(Base);return true;
    };
    if(auto* Base=Cast<UAnimSequenceBase>(Asset);Base&&!Visit(Base,0)) { return false; }
    if(IncludeMetadata)
    { if(!Small(Asset->GetMetaData().Num())) { return false; }for(const UAnimMetaData* M:Asset->GetMetaData()) { if(!Hold(M)) { return false; } }
      if(const TArray<UAssetUserData*>* Data=Asset->GetAssetUserDataArray()) { if(!Small(Data->Num())) { return false; }for(const UAssetUserData* D:*Data) { if(!Hold(D)) { return false; } } } }
    return true;
}
const USkeletalMesh* PreviewForObservation(const UAnimationAsset* Asset)
{
    Count(TEXT("const_preview_mesh_calls"));const USkeletalMesh* Mesh=Asset->GetPreviewMesh();if(!Hold(Mesh)) { return nullptr; }
    if(Mesh)
    {
        const USkeleton* MeshSkeleton=Mesh->GetSkeleton();const USkeleton* AssetSkeleton=Asset->GetSkeleton();
        if(!Hold(MeshSkeleton)||!Hold(AssetSkeleton)) { return nullptr; }
        Count(TEXT("preview_skeleton_compatibility_calls"));
        if(!MeshSkeleton||!MeshSkeleton->IsCompatibleForEditor(AssetSkeleton))
        { Current->Data->SetBoolField(TEXT("preview_mesh_incompatible_observed"),true);return nullptr; }
    }
    return Mesh;
}
TSharedPtr<FJsonObject> LegacyError(const FString& Error)
{ auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Error);return J; }
bool JsonSafe(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Items,int64& Chars)
{
    if(!V.IsValid()||Depth>32||++Items>100000) { Unsafe(TEXT("Native metadata JSON tree outside finite scope"));return false; }
    if(V->Type==EJson::Number) { return Finite(V->AsNumber()); }
    if(V->Type==EJson::String) { const auto S=V->AsString();Chars+=S.Len();if(!InputTextSafe(S,65536)||Chars>262144) { Unsafe(TEXT("Serialized metadata text exceeds finite scope"));return false; } }
    else if(V->Type==EJson::Array) { for(const auto& A:V->AsArray()) { if(!JsonSafe(A,Depth+1,Items,Chars)) { return false; } } }
    else if(V->Type==EJson::Object) { for(const auto& Pair:V->AsObject()->Values) { if(!JsonSafe(Pair.Value,Depth+1,Items,Chars)) { return false; } } }
    return true;
}
FString Serialize(const TSharedPtr<FJsonObject>& J)
{ int32 Items=0;int64 Chars=0;if(!JsonSafe(MakeShared<FJsonValueObject>(J),0,Items,Chars)) { return FString(); }FString S;FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&S));if(FTCHARToUTF8(*S).Length()>MetadataJsonBudget) { Unsafe(TEXT("Selected native JSON exceeds1MiB"));return FString(); }return S; }

FString NormalizeAssetObjectPathForLoad(const FString& AssetPath)
{
    FString NormalizedPath = FPackageName::ExportTextPathToObjectPath(AssetPath).TrimStartAndEnd();
    NormalizedPath.TrimQuotesInline();

    if ((NormalizedPath.StartsWith(TEXT("/Game/")) || NormalizedPath.StartsWith(TEXT("/Engine/"))) && !NormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(NormalizedPath);
        NormalizedPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    return NormalizedPath;
}

TArray<TSharedPtr<FJsonValue>> NameArrayToJson(const TArray<FName>& Names)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FName& Name : Names)
    {
        Values.Add(MakeShared<FJsonValueString>(Name.ToString()));
    }
    return Values;
}

TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vector.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Z));
    return Values;
}

TSharedPtr<FJsonObject> TransformToJsonObject(const FTransform& Transform)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetArrayField(TEXT("translation"), VectorToJsonArray(Transform.GetLocation()));

    const FQuat Rotation = Transform.GetRotation();
    TArray<TSharedPtr<FJsonValue>> QuatValues;
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.X));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.Y));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.Z));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.W));
    Object->SetArrayField(TEXT("rotation"), QuatValues);

    const FRotator Rotator = Rotation.Rotator();
    TArray<TSharedPtr<FJsonValue>> RotatorValues;
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Pitch));
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Yaw));
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Roll));
    Object->SetArrayField(TEXT("rotation_euler"), RotatorValues);

    Object->SetArrayField(TEXT("scale"), VectorToJsonArray(Transform.GetScale3D()));
    return Object;
}

UObject* LoadAssetObjectForRuntimeProbe(const FString& AssetPath)
{
    const FString NormalizedPath=NormalizeAssetObjectPathForLoad(AssetPath);
    Current->Data->SetStringField(TEXT("normalized_path"),NormalizedPath);
    if(NormalizedPath.IsEmpty()) { return nullptr; }
    if(!InputTextSafe(NormalizedPath,NAME_SIZE-1)) { Unsafe(TEXT("Composed asset object path exceeds FName bound"));return nullptr; }
    Count(TEXT("editor_asset_load_calls"));UObject* Asset=UEditorAssetLibrary::LoadAsset(NormalizedPath);Count(TEXT("editor_asset_load_returns"));
    if(!Asset) { Count(TEXT("static_load_calls"));Asset=StaticLoadObject(UObject::StaticClass(),nullptr,*NormalizedPath);Count(TEXT("static_load_returns")); }
    Current->Data->SetStringField(TEXT("loaded_object_path"),GetPathNameSafe(Asset));if(Asset&&!Hold(Asset)) { return nullptr; }return Asset;
}

void AddJsonString(TArray<TSharedPtr<FJsonValue>>& Values, const FString& Text)
{
    Values.Add(MakeShared<FJsonValueString>(Text));
}

TSharedPtr<FJsonObject> FrameRateToJsonObject(const FFrameRate& FrameRate)
{
    if(!Row()) { return MakeShared<FJsonObject>(); }Count(TEXT("FrameRateToJsonObject_calls"));if(FrameRate.Denominator==0) { Unsafe(TEXT("Zero frame-rate denominator"));return MakeShared<FJsonObject>(); }
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("numerator"), FrameRate.Numerator);
    Object->SetNumberField(TEXT("denominator"), FrameRate.Denominator);
    Object->SetNumberField(TEXT("decimal"), FrameRate.AsDecimal());
    return Object;
}

TSharedPtr<FJsonObject> UObjectSummaryToJson(const UObject* Object)
{
    if(!Row()) { return MakeShared<FJsonObject>(); }Count(TEXT("UObjectSummaryToJson_calls"));
    TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
    Summary->SetBoolField(TEXT("valid"), Object != nullptr);
    Summary->SetStringField(TEXT("name"), Object ? Object->GetName() : FString());
    Summary->SetStringField(TEXT("path"), Object ? Object->GetPathName() : FString());
    Summary->SetStringField(TEXT("class"), Object && Object->GetClass() ? Object->GetClass()->GetPathName() : FString());
    return Summary;
}

TArray<TSharedPtr<FJsonValue>> AnimMetadataClassNamesToJson(const TArray<UAnimMetaData*>& Metadata)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const UAnimMetaData* MetadataObject : Metadata)
    {
        if(Incomplete()) { break; }
        Values.Add(MakeShared<FJsonValueString>(
            MetadataObject && MetadataObject->GetClass() ? MetadataObject->GetClass()->GetPathName() : FString()));
    }
    return Values;
}

FString MontageNotifyTickTypeToString(EMontageNotifyTickType::Type TickType)
{
    switch (TickType)
    {
    case EMontageNotifyTickType::Queued:
        return TEXT("Queued");
    case EMontageNotifyTickType::BranchingPoint:
        return TEXT("BranchingPoint");
    default:
        return TEXT("Unknown");
    }
}

FString NotifyFilterTypeToString(ENotifyFilterType::Type FilterType)
{
    switch (FilterType)
    {
    case ENotifyFilterType::NoFiltering:
        return TEXT("NoFiltering");
    case ENotifyFilterType::LOD:
        return TEXT("LOD");
    default:
        return TEXT("Unknown");
    }
}

FString GetAnimationNotifyTrackName(const UAnimSequenceBase* SequenceBase, int32 TrackIndex)
{
#if WITH_EDITORONLY_DATA
    if (SequenceBase && SequenceBase->AnimNotifyTracks.IsValidIndex(TrackIndex))
    {
        return SequenceBase->AnimNotifyTracks[TrackIndex].TrackName.ToString();
    }
#endif
    return FString();
}

TSharedPtr<FJsonObject> AnimNotifyEventToJson(
    const UAnimSequenceBase* SequenceBase,
    const FAnimNotifyEvent& NotifyEvent,
    int32 NotifyIndex,
    bool bIncludeDetails)
{
    if(!Row()) { return MakeShared<FJsonObject>(); }Count(TEXT("AnimNotifyEventToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), NotifyIndex);
    Object->SetStringField(TEXT("guid"), NotifyEvent.Guid.ToString());
    Object->SetNumberField(TEXT("time"), NotifyEvent.GetTime());
    Object->SetStringField(TEXT("name"), NotifyEvent.NotifyName.ToString());
    Object->SetStringField(TEXT("event_name"), NotifyEvent.GetNotifyEventName().ToString());
    Object->SetNumberField(TEXT("trigger_time"), NotifyEvent.GetTriggerTime());
    Object->SetNumberField(TEXT("end_trigger_time"), NotifyEvent.GetEndTriggerTime());
    Object->SetNumberField(TEXT("duration"), NotifyEvent.GetDuration());
    Object->SetNumberField(TEXT("track_index"), NotifyEvent.TrackIndex);
    Object->SetStringField(TEXT("track_name"), GetAnimationNotifyTrackName(SequenceBase, NotifyEvent.TrackIndex));
    Object->SetBoolField(TEXT("is_branching_point"), NotifyEvent.IsBranchingPoint());
    Object->SetBoolField(TEXT("converted_from_branching_point"), NotifyEvent.bConvertedFromBranchingPoint);

    FString NotifyType = TEXT("named");
    if (NotifyEvent.NotifyStateClass)
    {
        NotifyType = TEXT("state");
    }
    else if (NotifyEvent.Notify)
    {
        NotifyType = TEXT("notify");
    }
    Object->SetStringField(TEXT("type"), NotifyType);

    if (bIncludeDetails)
    {
        const UAnimNotify* Notify = NotifyEvent.Notify.Get();
        const UAnimNotifyState* NotifyState = NotifyEvent.NotifyStateClass.Get();
        Object->SetStringField(TEXT("notify_class"), Notify && Notify->GetClass()
            ? Notify->GetClass()->GetPathName()
            : FString());
        Object->SetStringField(TEXT("notify_state_class"), NotifyState && NotifyState->GetClass()
            ? NotifyState->GetClass()->GetPathName()
            : FString());
        Object->SetNumberField(TEXT("trigger_weight_threshold"), NotifyEvent.TriggerWeightThreshold);
        Object->SetNumberField(TEXT("trigger_chance"), NotifyEvent.NotifyTriggerChance);
        Object->SetStringField(TEXT("filter_type"), NotifyFilterTypeToString(NotifyEvent.NotifyFilterType.GetValue()));
        Object->SetNumberField(TEXT("filter_type_value"), static_cast<int32>(NotifyEvent.NotifyFilterType.GetValue()));
        Object->SetNumberField(TEXT("filter_lod"), NotifyEvent.NotifyFilterLOD);
        Object->SetBoolField(TEXT("can_be_filtered"), NotifyEvent.bCanBeFilteredViaRequest);
        Object->SetBoolField(TEXT("trigger_on_dedicated_server"), NotifyEvent.bTriggerOnDedicatedServer);
        Object->SetBoolField(TEXT("trigger_on_follower"), NotifyEvent.bTriggerOnFollower);
        Object->SetStringField(TEXT("montage_tick_type"), MontageNotifyTickTypeToString(NotifyEvent.MontageTickType.GetValue()));
        Object->SetNumberField(TEXT("montage_tick_type_value"), static_cast<int32>(NotifyEvent.MontageTickType.GetValue()));
    }

    return Object;
}

TArray<TSharedPtr<FJsonValue>> AnimNotifiesToJson(
    const UAnimSequenceBase* SequenceBase,
    int32 MaxNotifies,
    bool bIncludeDetails,
    bool& bOutTruncated,
    TArray<TSharedPtr<FJsonValue>>& BranchingIndexValues)
{
    Count(TEXT("AnimNotifiesToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> Values;
    bOutTruncated = false;
    if (!SequenceBase)
    {
        return Values;
    }

    for (int32 NotifyIndex = 0; NotifyIndex < SequenceBase->Notifies.Num(); ++NotifyIndex)
    {
        if(Incomplete()) { break; }
        const FAnimNotifyEvent& NotifyEvent = SequenceBase->Notifies[NotifyIndex];
        if (NotifyEvent.IsBranchingPoint())
        {
            BranchingIndexValues.Add(MakeShared<FJsonValueNumber>(NotifyIndex));
        }

        if (Values.Num() >= MaxNotifies)
        {
            bOutTruncated = true;
            continue;
        }
        Values.Add(MakeShared<FJsonValueObject>(AnimNotifyEventToJson(SequenceBase, NotifyEvent, NotifyIndex, bIncludeDetails)));
    }
    return Values;
}

TArray<TSharedPtr<FJsonValue>> BranchingPointsToJson(const UAnimSequenceBase* SequenceBase, int32 MaxNotifies)
{
    Count(TEXT("BranchingPointsToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!SequenceBase)
    {
        return Values;
    }

    for (int32 NotifyIndex = 0; NotifyIndex < SequenceBase->Notifies.Num(); ++NotifyIndex)
    {
        if(Incomplete()) { break; }
        const FAnimNotifyEvent& NotifyEvent = SequenceBase->Notifies[NotifyIndex];
        if (!NotifyEvent.IsBranchingPoint())
        {
            continue;
        }
        if (Values.Num() >= MaxNotifies)
        {
            break;
        }

        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("notify_index"), NotifyIndex);
        Object->SetStringField(TEXT("name"), NotifyEvent.NotifyName.ToString());
        Object->SetStringField(TEXT("event_name"), NotifyEvent.GetNotifyEventName().ToString());
        Object->SetNumberField(TEXT("trigger_time"), NotifyEvent.GetTriggerTime());
        Object->SetNumberField(TEXT("end_trigger_time"), NotifyEvent.GetEndTriggerTime());
        Object->SetStringField(TEXT("montage_tick_type"), MontageNotifyTickTypeToString(NotifyEvent.MontageTickType.GetValue()));
        Object->SetStringField(TEXT("source"), TEXT("notify_event_is_branching_point"));
        Values.Add(MakeShared<FJsonValueObject>(Object));
    }

    return Values;
}

TSharedPtr<FJsonObject> FloatCurveToJson(
    const FFloatCurve& Curve,
    const FString& Source,
    bool bIncludeKeys,
    int32 MaxKeysPerCurve,
    bool& bOutKeysTruncated)
{
    if(!Row()) { return MakeShared<FJsonObject>(); }Count(TEXT("FloatCurveToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("name"), Curve.GetName().ToString());
    Object->SetStringField(TEXT("curve_kind"), TEXT("float"));
    Object->SetStringField(TEXT("source"), Source);
    Object->SetNumberField(TEXT("flags"), Curve.GetCurveTypeFlags());
    Object->SetNumberField(TEXT("key_count"), Curve.FloatCurve.GetNumKeys());

    if (bIncludeKeys)
    {
        TArray<float> Times;
        TArray<float> Values;
        Curve.GetKeys(Times, Values);

        TArray<TSharedPtr<FJsonValue>> KeyValues;
        const int32 IncludedKeyCount = FMath::Min(Times.Num(), MaxKeysPerCurve);
        for (int32 KeyIndex = 0; KeyIndex < IncludedKeyCount; ++KeyIndex)
        {
            if(Incomplete()) { break; }
            if(!Row()) { break; }Count(TEXT("curve_key_rows"));
            TSharedPtr<FJsonObject> KeyObject = MakeShared<FJsonObject>();
            KeyObject->SetNumberField(TEXT("index"), KeyIndex);
            KeyObject->SetNumberField(TEXT("time"), Times[KeyIndex]);
            KeyObject->SetNumberField(TEXT("value"), Values.IsValidIndex(KeyIndex) ? Values[KeyIndex] : 0.0f);
            KeyValues.Add(MakeShared<FJsonValueObject>(KeyObject));
        }
        Object->SetArrayField(TEXT("keys"), KeyValues);
        Object->SetBoolField(TEXT("keys_truncated"), Times.Num() > IncludedKeyCount);
        bOutKeysTruncated = bOutKeysTruncated || Times.Num() > IncludedKeyCount;
    }

    return Object;
}

int32 TransformCurveChannelKeyCount(const FTransformCurve& Curve)
{
    return Curve.TranslationCurve.GetNumKeys() + Curve.RotationCurve.GetNumKeys() + Curve.ScaleCurve.GetNumKeys();
}

TSharedPtr<FJsonObject> TransformCurveToJson(
    const FTransformCurve& Curve,
    const FString& Source,
    bool bIncludeKeys,
    int32 MaxKeysPerCurve,
    bool& bOutKeysTruncated)
{
    if(!Row()) { return MakeShared<FJsonObject>(); }Count(TEXT("TransformCurveToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("name"), Curve.GetName().ToString());
    Object->SetStringField(TEXT("curve_kind"), TEXT("transform"));
    Object->SetStringField(TEXT("source"), Source);
    Object->SetNumberField(TEXT("flags"), Curve.GetCurveTypeFlags());
    Object->SetNumberField(TEXT("key_count"), TransformCurveChannelKeyCount(Curve));
    Object->SetNumberField(TEXT("translation_key_count"), Curve.TranslationCurve.GetNumKeys());
    Object->SetNumberField(TEXT("rotation_key_count"), Curve.RotationCurve.GetNumKeys());
    Object->SetNumberField(TEXT("scale_key_count"), Curve.ScaleCurve.GetNumKeys());

    if (bIncludeKeys)
    {
        TArray<float> Times;
        TArray<FTransform> Values;
        Curve.GetKeys(Times, Values);

        TArray<TSharedPtr<FJsonValue>> KeyValues;
        const int32 IncludedKeyCount = FMath::Min(Times.Num(), MaxKeysPerCurve);
        for (int32 KeyIndex = 0; KeyIndex < IncludedKeyCount; ++KeyIndex)
        {
            if(Incomplete()) { break; }
            if(!Row()) { break; }Count(TEXT("curve_key_rows"));
            TSharedPtr<FJsonObject> KeyObject = MakeShared<FJsonObject>();
            KeyObject->SetNumberField(TEXT("index"), KeyIndex);
            KeyObject->SetNumberField(TEXT("time"), Times[KeyIndex]);
            if (Values.IsValidIndex(KeyIndex))
            {
                KeyObject->SetObjectField(TEXT("transform"), TransformToJsonObject(Values[KeyIndex]));
            }
            KeyValues.Add(MakeShared<FJsonValueObject>(KeyObject));
        }
        Object->SetArrayField(TEXT("keys"), KeyValues);
        Object->SetBoolField(TEXT("keys_truncated"), Times.Num() > IncludedKeyCount);
        bOutKeysTruncated = bOutKeysTruncated || Times.Num() > IncludedKeyCount;
    }

    return Object;
}

struct FAnimationCurveExportStats
{
    int32 TotalFloatCurves = 0;
    int32 TotalTransformCurves = 0;
    int32 IncludedCurves = 0;
    bool bCurvesTruncated = false;
    bool bCurveKeysTruncated = false;
    bool bUsedDataModel = false;
};

TArray<TSharedPtr<FJsonValue>> AnimationCurvesToJson(
    UAnimSequenceBase* SequenceBase,
    int32 MaxCurves,
    bool bIncludeKeys,
    int32 MaxKeysPerCurve,
    FAnimationCurveExportStats& OutStats,
    TArray<TSharedPtr<FJsonValue>>& Warnings)
{
    Count(TEXT("AnimationCurvesToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> CurveValues;
    if (!SequenceBase)
    {
        return CurveValues;
    }

#if WITH_EDITOR
    UAnimSequence* Sequence = Cast<UAnimSequence>(SequenceBase);
    IAnimationDataModel* DataModel = Sequence && SequenceBase->IsDataModelValid() ? SequenceBase->GetDataModel() : nullptr;
    if (DataModel)
    {
        OutStats.bUsedDataModel = true;
        const TArray<FFloatCurve>& FloatCurves = DataModel->GetFloatCurves();
        const TArray<FTransformCurve>& TransformCurves = DataModel->GetTransformCurves();
        OutStats.TotalFloatCurves = FloatCurves.Num();
        OutStats.TotalTransformCurves = TransformCurves.Num();

        for (const FFloatCurve& Curve : FloatCurves)
        {
            if(Incomplete()) { break; }
            if (CurveValues.Num() >= MaxCurves)
            {
                OutStats.bCurvesTruncated = true;
                continue;
            }
            CurveValues.Add(MakeShared<FJsonValueObject>(FloatCurveToJson(
                Curve,
                TEXT("data_model_float"),
                bIncludeKeys,
                MaxKeysPerCurve,
                OutStats.bCurveKeysTruncated)));
        }

        for (const FTransformCurve& Curve : TransformCurves)
        {
            if(Incomplete()) { break; }
            if (CurveValues.Num() >= MaxCurves)
            {
                OutStats.bCurvesTruncated = true;
                continue;
            }
            CurveValues.Add(MakeShared<FJsonValueObject>(TransformCurveToJson(
                Curve,
                TEXT("data_model_transform"),
                bIncludeKeys,
                MaxKeysPerCurve,
                OutStats.bCurveKeysTruncated)));
        }
    }
    else
    {
        AddJsonString(Warnings, TEXT("Animation DataModel is unavailable, invalid, or not a UAnimSequence; using runtime curve data fallback."));
    }
#else
    AddJsonString(Warnings, TEXT("Animation DataModel is editor-only and unavailable in this build; using runtime curve data fallback."));
#endif

    if (!OutStats.bUsedDataModel)
    {
        const FRawCurveTracks& CurveData = SequenceBase->GetCurveData();
        OutStats.TotalFloatCurves = CurveData.FloatCurves.Num();
#if WITH_EDITORONLY_DATA
        OutStats.TotalTransformCurves = CurveData.TransformCurves.Num();
#else
        OutStats.TotalTransformCurves = 0;
#endif

        for (const FFloatCurve& Curve : CurveData.FloatCurves)
        {
            if(Incomplete()) { break; }
            if (CurveValues.Num() >= MaxCurves)
            {
                OutStats.bCurvesTruncated = true;
                continue;
            }
            CurveValues.Add(MakeShared<FJsonValueObject>(FloatCurveToJson(
                Curve,
                TEXT("runtime_curve_data"),
                bIncludeKeys,
                MaxKeysPerCurve,
                OutStats.bCurveKeysTruncated)));
        }

#if WITH_EDITORONLY_DATA
        for (const FTransformCurve& Curve : CurveData.TransformCurves)
        {
            if(Incomplete()) { break; }
            if (CurveValues.Num() >= MaxCurves)
            {
                OutStats.bCurvesTruncated = true;
                continue;
            }
            CurveValues.Add(MakeShared<FJsonValueObject>(TransformCurveToJson(
                Curve,
                TEXT("runtime_curve_data_transform"),
                bIncludeKeys,
                MaxKeysPerCurve,
                OutStats.bCurveKeysTruncated)));
        }
#endif
    }

    OutStats.IncludedCurves = CurveValues.Num();
    return CurveValues;
}

TSharedPtr<FJsonObject> SyncMarkerToJson(
    const UAnimSequenceBase* SequenceBase,
    const FAnimSyncMarker& Marker,
    int32 MarkerIndex,
    const FString& Source)
{
    if(!Row()) { return MakeShared<FJsonObject>(); }Count(TEXT("SyncMarkerToJson_calls"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), MarkerIndex);
    Object->SetStringField(TEXT("name"), Marker.MarkerName.ToString());
    Object->SetNumberField(TEXT("time"), Marker.Time);
    Object->SetStringField(TEXT("source"), Source);
#if WITH_EDITORONLY_DATA
    Object->SetNumberField(TEXT("track_index"), Marker.TrackIndex);
    Object->SetStringField(TEXT("track_name"), GetAnimationNotifyTrackName(SequenceBase, Marker.TrackIndex));
    Object->SetStringField(TEXT("guid"), Marker.Guid.ToString());
#else
    Object->SetNumberField(TEXT("track_index"), INDEX_NONE);
    Object->SetStringField(TEXT("track_name"), FString());
#endif
    return Object;
}

TArray<TSharedPtr<FJsonValue>> SyncMarkersToJson(
    const UAnimSequence* Sequence,
    const UAnimMontage* Montage,
    int32 MaxMarkers,
    bool& bOutTruncated,
    int32& OutTotalMarkers)
{
    Count(TEXT("SyncMarkersToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> Values;
    bOutTruncated = false;
    OutTotalMarkers = 0;

    const UAnimSequenceBase* SequenceBase = Sequence ? static_cast<const UAnimSequenceBase*>(Sequence) : static_cast<const UAnimSequenceBase*>(Montage);
    const TArray<FAnimSyncMarker>* Markers = nullptr;
    FString Source;
    if (Sequence)
    {
        Markers = &Sequence->AuthoredSyncMarkers;
        Source = TEXT("anim_sequence");
    }
    else if (Montage)
    {
        Markers = &Montage->MarkerData.AuthoredSyncMarkers;
        Source = TEXT("anim_montage");
    }

    if (!Markers)
    {
        return Values;
    }

    OutTotalMarkers = Markers->Num();
    for (int32 MarkerIndex = 0; MarkerIndex < Markers->Num(); ++MarkerIndex)
    {
        if(Incomplete()) { break; }
        if (Values.Num() >= MaxMarkers)
        {
            bOutTruncated = true;
            continue;
        }
        Values.Add(MakeShared<FJsonValueObject>(SyncMarkerToJson(SequenceBase, (*Markers)[MarkerIndex], MarkerIndex, Source)));
    }
    return Values;
}

TSharedPtr<FJsonObject> AnimSegmentToJson(const FAnimSegment& Segment, int32 SegmentIndex)
{
    if(!Row()) { return MakeShared<FJsonObject>(); }Count(TEXT("AnimSegmentToJson_calls"));
    const UAnimSequenceBase* AnimReference = Segment.GetAnimReference().Get();
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("index"), SegmentIndex);
    Object->SetStringField(TEXT("animation"), AnimReference ? AnimReference->GetPathName() : FString());
    Object->SetStringField(TEXT("animation_name"), AnimReference ? AnimReference->GetName() : FString());
    Object->SetStringField(TEXT("animation_class"), AnimReference && AnimReference->GetClass() ? AnimReference->GetClass()->GetPathName() : FString());
    Object->SetNumberField(TEXT("start_pos"), Segment.StartPos);
    Object->SetNumberField(TEXT("end_pos"), Segment.GetEndPos());
    Object->SetNumberField(TEXT("length"), Segment.GetLength());
    Object->SetNumberField(TEXT("anim_start_time"), Segment.AnimStartTime);
    Object->SetNumberField(TEXT("anim_end_time"), Segment.AnimEndTime);
    Object->SetNumberField(TEXT("anim_play_rate"), Segment.AnimPlayRate);
    Object->SetNumberField(TEXT("looping_count"), Segment.LoopingCount);
    Object->SetBoolField(TEXT("valid"), Segment.IsValid());
    Object->SetBoolField(TEXT("notify_available"), Segment.IsNotifyAvailable());
    return Object;
}

TArray<TSharedPtr<FJsonValue>> MontageSlotsToJson(
    const UAnimMontage* Montage,
    int32 MaxSlots,
    int32 MaxSegmentsPerSlot,
    bool& bOutSlotsTruncated,
    bool& bOutSegmentsTruncated,
    int32& OutTotalSlots)
{
    Count(TEXT("MontageSlotsToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> Values;
    bOutSlotsTruncated = false;
    bOutSegmentsTruncated = false;
    OutTotalSlots = Montage ? Montage->SlotAnimTracks.Num() : 0;
    if (!Montage)
    {
        return Values;
    }

    for (int32 SlotIndex = 0; SlotIndex < Montage->SlotAnimTracks.Num(); ++SlotIndex)
    {
        if(Incomplete()) { break; }
        if (Values.Num() >= MaxSlots)
        {
            bOutSlotsTruncated = true;
            continue;
        }

        const FSlotAnimationTrack& SlotTrack = Montage->SlotAnimTracks[SlotIndex];
        TSharedPtr<FJsonObject> SlotObject = MakeShared<FJsonObject>();
        SlotObject->SetNumberField(TEXT("index"), SlotIndex);
        SlotObject->SetStringField(TEXT("slot_name"), SlotTrack.SlotName.ToString());
        SlotObject->SetNumberField(TEXT("segment_count"), SlotTrack.AnimTrack.AnimSegments.Num());

        double TrackLength = 0.0;
        TArray<TSharedPtr<FJsonValue>> SegmentValues;
        for (int32 SegmentIndex = 0; SegmentIndex < SlotTrack.AnimTrack.AnimSegments.Num(); ++SegmentIndex)
        {
            if(Incomplete()) { break; }
            const FAnimSegment& Segment = SlotTrack.AnimTrack.AnimSegments[SegmentIndex];
            TrackLength = FMath::Max(TrackLength, static_cast<double>(Segment.GetEndPos()));
            if (SegmentValues.Num() >= MaxSegmentsPerSlot)
            {
                bOutSegmentsTruncated = true;
                continue;
            }
            SegmentValues.Add(MakeShared<FJsonValueObject>(AnimSegmentToJson(Segment, SegmentIndex)));
        }

        SlotObject->SetNumberField(TEXT("track_length"), TrackLength);
        SlotObject->SetBoolField(TEXT("segments_truncated"), SlotTrack.AnimTrack.AnimSegments.Num() > SegmentValues.Num());
        SlotObject->SetArrayField(TEXT("segments"), SegmentValues);
        Values.Add(MakeShared<FJsonValueObject>(SlotObject));
    }

    return Values;
}

TArray<TSharedPtr<FJsonValue>> MontageSectionsToJson(
    const UAnimMontage* Montage,
    int32 MaxSections,
    bool& bOutTruncated,
    int32& OutTotalSections)
{
    Count(TEXT("MontageSectionsToJson_calls"));
    TArray<TSharedPtr<FJsonValue>> Values;
    bOutTruncated = false;
    OutTotalSections = Montage ? Montage->GetNumSections() : 0;
    if (!Montage)
    {
        return Values;
    }

    for (int32 SectionIndex = 0; SectionIndex < Montage->GetNumSections(); ++SectionIndex)
    {
        if(Incomplete()) { break; }
        if (Values.Num() >= MaxSections)
        {
            bOutTruncated = true;
            continue;
        }

        float StartTime = 0.0f;
        float EndTime = 0.0f;
        Montage->GetSectionStartAndEndTime(SectionIndex, StartTime, EndTime);
        const FCompositeSection& Section = Montage->GetAnimCompositeSection(SectionIndex);

        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("index"), SectionIndex);
        Object->SetStringField(TEXT("name"), Montage->GetSectionName(SectionIndex).ToString());
        Object->SetNumberField(TEXT("start"), StartTime);
        Object->SetNumberField(TEXT("end"), EndTime);
        Object->SetNumberField(TEXT("length"), Montage->GetSectionLength(SectionIndex));
        Object->SetStringField(TEXT("next_section"), Section.NextSectionName.ToString());
        Object->SetArrayField(TEXT("metadata_classes"), AnimMetadataClassNamesToJson(Section.GetMetaData()));
        Values.Add(MakeShared<FJsonValueObject>(Object));
    }

    return Values;
}


struct FOptions
{
    bool bIncludeNotifies;
    bool bIncludeNotifyDetails;
    bool bIncludeCurves;
    bool bIncludeCurveKeys;
    bool bIncludeSyncMarkers;
    bool bIncludeMontageSections;
    bool bIncludeMontageSlots;
    bool bIncludeMetadata;
    bool bRequireSupportedClass;
    int32 MaxNotifies;
    int32 MaxCurves;
    int32 MaxCurveKeysPerCurve;
    int32 MaxMarkers;
    int32 MaxSections;
    int32 MaxSlots;
    int32 MaxSegmentsPerSlot;
};
TSharedPtr<FJsonObject> Observe(UObject* LoadedObject,const FString& AssetPath,const FOptions& Options)
{
    Count(TEXT("observation_calls"));SerializedRows=0;
    const bool bIncludeNotifies=Options.bIncludeNotifies;
    const bool bIncludeNotifyDetails=Options.bIncludeNotifyDetails;
    const bool bIncludeCurves=Options.bIncludeCurves;
    const bool bIncludeCurveKeys=Options.bIncludeCurveKeys;
    const bool bIncludeSyncMarkers=Options.bIncludeSyncMarkers;
    const bool bIncludeMontageSections=Options.bIncludeMontageSections;
    const bool bIncludeMontageSlots=Options.bIncludeMontageSlots;
    const bool bIncludeMetadata=Options.bIncludeMetadata;
    const bool bRequireSupportedClass=Options.bRequireSupportedClass;
    const int32 MaxNotifies=Options.MaxNotifies;
    const int32 MaxCurves=Options.MaxCurves;
    const int32 MaxCurveKeysPerCurve=Options.MaxCurveKeysPerCurve;
    const int32 MaxMarkers=Options.MaxMarkers;
    const int32 MaxSections=Options.MaxSections;
    const int32 MaxSlots=Options.MaxSlots;
    const int32 MaxSegmentsPerSlot=Options.MaxSegmentsPerSlot;
    UPackage* Package = LoadedObject->GetOutermost();
    const bool bPackageDirtyBefore = Package && Package->IsDirty();
    UAnimationAsset* Asset = Cast<UAnimationAsset>(LoadedObject);
    UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(Asset);
    UAnimSequence* Sequence = Cast<UAnimSequence>(Asset);
    UAnimMontage* Montage = Cast<UAnimMontage>(Asset);
    const bool bSupportedClass = Asset && SequenceBase;

    TArray<TSharedPtr<FJsonValue>> WarningValues;
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    TArray<TSharedPtr<FJsonValue>> UnavailableValues;

    TSharedPtr<FJsonObject> AssetObject = MakeShared<FJsonObject>();
    AssetObject->SetStringField(TEXT("input_path"), AssetPath);
    AssetObject->SetStringField(TEXT("normalized_path"), NormalizeAssetObjectPathForLoad(AssetPath));
    AssetObject->SetStringField(TEXT("path"), LoadedObject->GetPathName());
    AssetObject->SetStringField(TEXT("name"), LoadedObject->GetName());
    AssetObject->SetStringField(TEXT("class"), LoadedObject->GetClass() ? LoadedObject->GetClass()->GetPathName() : FString());
    AssetObject->SetStringField(TEXT("package"), Package ? Package->GetPathName() : FString());
    AssetObject->SetBoolField(TEXT("supported_class"), bSupportedClass);
    AssetObject->SetBoolField(TEXT("is_animation_asset"), Asset != nullptr);
    AssetObject->SetBoolField(TEXT("is_sequence_base"), SequenceBase != nullptr);
    AssetObject->SetBoolField(TEXT("is_sequence"), Sequence != nullptr);
    AssetObject->SetBoolField(TEXT("is_montage"), Montage != nullptr);

    if (Asset)
    {
        const USkeleton* Skeleton = Asset->GetSkeleton();
        const USkeletalMesh* PreviewMesh = PreviewForObservation(static_cast<const UAnimationAsset*>(Asset));
        AssetObject->SetStringField(TEXT("skeleton_path"), Skeleton ? Skeleton->GetPathName() : FString());
        AssetObject->SetStringField(TEXT("preview_mesh_path"), PreviewMesh ? PreviewMesh->GetPathName() : FString());
        AssetObject->SetNumberField(TEXT("play_length"), Asset->GetPlayLength());
    }
    else
    {
        AssetObject->SetStringField(TEXT("skeleton_path"), FString());
        AssetObject->SetStringField(TEXT("preview_mesh_path"), FString());
        AssetObject->SetNumberField(TEXT("play_length"), 0.0);
    }

    if (SequenceBase)
    {
        AssetObject->SetNumberField(TEXT("rate_scale"), SequenceBase->RateScale);
        AssetObject->SetBoolField(TEXT("loop"), SequenceBase->bLoop);
    }
    else
    {
        AssetObject->SetNumberField(TEXT("rate_scale"), 0.0);
        AssetObject->SetBoolField(TEXT("loop"), false);
    }

    if (!bSupportedClass)
    {
        AddJsonString(ErrorValues, TEXT("Loaded object is not a supported UAnimSequenceBase/UAnimMontage animation metadata asset."));
        if (bRequireSupportedClass)
        {
            TSharedPtr<FJsonObject> ErrorObj = LegacyError(TEXT("Unsupported animation asset class for inspect_animation_asset_metadata_guarded"));
            ErrorObj->SetBoolField(TEXT("unsupported_class"), true);
            ErrorObj->SetObjectField(TEXT("asset"), AssetObject);
            return ErrorObj;
        }
    }

    TSharedPtr<FJsonObject> SequenceObject = MakeShared<FJsonObject>();
    SequenceObject->SetBoolField(TEXT("valid"), SequenceBase != nullptr);
    if (SequenceBase)
    {
        SequenceObject->SetNumberField(TEXT("sampled_keys"), SequenceBase->GetNumberOfSampledKeys());
        SequenceObject->SetObjectField(TEXT("sampling_frame_rate"), FrameRateToJsonObject(SequenceBase->GetSamplingFrameRate()));
        SequenceObject->SetBoolField(TEXT("has_root_motion"), SequenceBase->HasRootMotion());
        SequenceObject->SetNumberField(TEXT("notify_count"), SequenceBase->Notifies.Num());
    }

    if (Sequence)
    {
        SequenceObject->SetNumberField(TEXT("authored_sync_marker_count"), Sequence->AuthoredSyncMarkers.Num());
        SequenceObject->SetArrayField(TEXT("unique_marker_names"), NameArrayToJson(Sequence->UniqueMarkerNames));
    }
    else if (Montage)
    {
        SequenceObject->SetNumberField(TEXT("authored_sync_marker_count"), Montage->MarkerData.AuthoredSyncMarkers.Num());
        SequenceObject->SetArrayField(TEXT("unique_marker_names"), NameArrayToJson(Montage->MarkerData.UniqueMarkerNames));
    }
    else
    {
        SequenceObject->SetNumberField(TEXT("authored_sync_marker_count"), 0);
        SequenceObject->SetArrayField(TEXT("unique_marker_names"), TArray<TSharedPtr<FJsonValue>>());
    }

#if WITH_EDITOR
    if (SequenceBase && Sequence && SequenceBase->IsDataModelValid())
    {
        IAnimationDataModel* DataModel = SequenceBase->GetDataModel();
        SequenceObject->SetBoolField(TEXT("data_model_valid"), DataModel != nullptr);
        if (DataModel)
        {
            SequenceObject->SetNumberField(TEXT("data_model_frames"), DataModel->GetNumberOfFrames());
            SequenceObject->SetNumberField(TEXT("data_model_keys"), DataModel->GetNumberOfKeys());
            SequenceObject->SetObjectField(TEXT("data_model_frame_rate"), FrameRateToJsonObject(DataModel->GetFrameRate()));
            SequenceObject->SetNumberField(TEXT("data_model_float_curve_count"), DataModel->GetNumberOfFloatCurves());
            SequenceObject->SetNumberField(TEXT("data_model_transform_curve_count"), DataModel->GetNumberOfTransformCurves());
        }
    }
    else
    {
        SequenceObject->SetBoolField(TEXT("data_model_valid"), false);
        if (SequenceBase && !Sequence)
        {
            AddJsonString(WarningValues, TEXT("Animation DataModel summary is only read for UAnimSequence assets in this command."));
        }
    }
#else
    SequenceObject->SetBoolField(TEXT("data_model_valid"), false);
    AddJsonString(UnavailableValues, TEXT("data_model"));
#endif

    TArray<TSharedPtr<FJsonValue>> BranchingIndexValues;
    if (SequenceBase)
    {
        for (int32 NotifyIndex = 0; NotifyIndex < SequenceBase->Notifies.Num(); ++NotifyIndex)
        {
            if (SequenceBase->Notifies[NotifyIndex].IsBranchingPoint())
            {
                BranchingIndexValues.Add(MakeShared<FJsonValueNumber>(NotifyIndex));
            }
        }
    }

    bool bNotifiesTruncated = false;
    TArray<TSharedPtr<FJsonValue>> NotifyValues;
    if (bIncludeNotifies)
    {
        TArray<TSharedPtr<FJsonValue>> IgnoredBranchValues;
        NotifyValues = AnimNotifiesToJson(SequenceBase, MaxNotifies, bIncludeNotifyDetails, bNotifiesTruncated, IgnoredBranchValues);
    }

    FAnimationCurveExportStats CurveStats;
    TArray<TSharedPtr<FJsonValue>> CurveValues;
    if (bIncludeCurves)
    {
        CurveValues = AnimationCurvesToJson(
            SequenceBase,
            MaxCurves,
            bIncludeCurveKeys,
            MaxCurveKeysPerCurve,
            CurveStats,
            WarningValues);
    }

    bool bMarkersTruncated = false;
    int32 TotalMarkers = 0;
    TArray<TSharedPtr<FJsonValue>> SyncMarkerValues;
    if (bIncludeSyncMarkers)
    {
        SyncMarkerValues = SyncMarkersToJson(Sequence, Montage, MaxMarkers, bMarkersTruncated, TotalMarkers);
    }

    TSharedPtr<FJsonObject> MontageObject = MakeShared<FJsonObject>();
    MontageObject->SetBoolField(TEXT("valid"), Montage != nullptr);
    bool bSectionsTruncated = false;
    int32 TotalSections = 0;
    bool bSlotsTruncated = false;
    bool bSegmentsTruncated = false;
    int32 TotalSlots = 0;
    if (Montage)
    {
        MontageObject->SetStringField(TEXT("sync_group"), Montage->SyncGroup.ToString());
        MontageObject->SetNumberField(TEXT("sync_slot_index"), Montage->SyncSlotIndex);
        MontageObject->SetNumberField(TEXT("blend_in_time"), Montage->GetDefaultBlendInTime());
        MontageObject->SetNumberField(TEXT("blend_out_time"), Montage->GetDefaultBlendOutTime());
        MontageObject->SetNumberField(TEXT("blend_out_trigger_time"), Montage->BlendOutTriggerTime);
        MontageObject->SetBoolField(TEXT("enable_auto_blend_out"), Montage->bEnableAutoBlendOut);
        MontageObject->SetArrayField(TEXT("branching_point_state_notify_indices"), BranchingIndexValues);
        MontageObject->SetArrayField(TEXT("branching_points"), BranchingPointsToJson(SequenceBase, MaxNotifies));

        if (bIncludeMontageSections)
        {
            MontageObject->SetArrayField(TEXT("sections"), MontageSectionsToJson(Montage, MaxSections, bSectionsTruncated, TotalSections));
        }
        else
        {
            TotalSections = Montage->GetNumSections();
            MontageObject->SetArrayField(TEXT("sections"), TArray<TSharedPtr<FJsonValue>>());
        }

        if (bIncludeMontageSlots)
        {
            MontageObject->SetArrayField(TEXT("slots"), MontageSlotsToJson(Montage, MaxSlots, MaxSegmentsPerSlot, bSlotsTruncated, bSegmentsTruncated, TotalSlots));
        }
        else
        {
            TotalSlots = Montage->SlotAnimTracks.Num();
            MontageObject->SetArrayField(TEXT("slots"), TArray<TSharedPtr<FJsonValue>>());
        }
    }
    else
    {
        MontageObject->SetArrayField(TEXT("branching_point_state_notify_indices"), TArray<TSharedPtr<FJsonValue>>());
        MontageObject->SetArrayField(TEXT("branching_points"), TArray<TSharedPtr<FJsonValue>>());
        MontageObject->SetArrayField(TEXT("sections"), TArray<TSharedPtr<FJsonValue>>());
        MontageObject->SetArrayField(TEXT("slots"), TArray<TSharedPtr<FJsonValue>>());
    }
    MontageObject->SetNumberField(TEXT("section_count"), TotalSections);
    MontageObject->SetNumberField(TEXT("slot_count"), TotalSlots);

    TSharedPtr<FJsonObject> MetadataObject = MakeShared<FJsonObject>();
    MetadataObject->SetBoolField(TEXT("included"), bIncludeMetadata && Asset != nullptr);
    if (bIncludeMetadata && Asset)
    {
        TArray<TSharedPtr<FJsonValue>> AssetMetadataValues;
        for (const UAnimMetaData* Metadata : Asset->GetMetaData())
        {
            AssetMetadataValues.Add(MakeShared<FJsonValueObject>(UObjectSummaryToJson(Metadata)));
        }
        MetadataObject->SetArrayField(TEXT("asset_metadata"), AssetMetadataValues);

        TArray<TSharedPtr<FJsonValue>> UserDataValues;
        if (const TArray<UAssetUserData*>* UserDataArray = Asset->GetAssetUserDataArray())
        {
            for (const UAssetUserData* UserData : *UserDataArray)
            {
                UserDataValues.Add(MakeShared<FJsonValueObject>(UObjectSummaryToJson(UserData)));
            }
        }
        MetadataObject->SetArrayField(TEXT("asset_user_data"), UserDataValues);
    }

    TSharedPtr<FJsonObject> LimitsObject = MakeShared<FJsonObject>();
    LimitsObject->SetBoolField(TEXT("include_notifies"), bIncludeNotifies);
    LimitsObject->SetBoolField(TEXT("include_notify_details"), bIncludeNotifyDetails);
    LimitsObject->SetBoolField(TEXT("include_curves"), bIncludeCurves);
    LimitsObject->SetBoolField(TEXT("include_curve_keys"), bIncludeCurveKeys);
    LimitsObject->SetBoolField(TEXT("include_sync_markers"), bIncludeSyncMarkers);
    LimitsObject->SetBoolField(TEXT("include_montage_sections"), bIncludeMontageSections);
    LimitsObject->SetBoolField(TEXT("include_montage_slots"), bIncludeMontageSlots);
    LimitsObject->SetBoolField(TEXT("include_metadata"), bIncludeMetadata);
    LimitsObject->SetBoolField(TEXT("require_supported_class"), bRequireSupportedClass);
    LimitsObject->SetNumberField(TEXT("max_notifies"), MaxNotifies);
    LimitsObject->SetNumberField(TEXT("max_curves"), MaxCurves);
    LimitsObject->SetNumberField(TEXT("max_curve_keys_per_curve"), MaxCurveKeysPerCurve);
    LimitsObject->SetNumberField(TEXT("max_markers"), MaxMarkers);
    LimitsObject->SetNumberField(TEXT("max_sections"), MaxSections);
    LimitsObject->SetNumberField(TEXT("max_slots"), MaxSlots);
    LimitsObject->SetNumberField(TEXT("max_segments_per_slot"), MaxSegmentsPerSlot);

    TSharedPtr<FJsonObject> TruncatedObject = MakeShared<FJsonObject>();
    TruncatedObject->SetBoolField(TEXT("notifies"), bNotifiesTruncated);
    TruncatedObject->SetBoolField(TEXT("curves"), CurveStats.bCurvesTruncated);
    TruncatedObject->SetBoolField(TEXT("curve_keys"), CurveStats.bCurveKeysTruncated);
    TruncatedObject->SetBoolField(TEXT("sync_markers"), bMarkersTruncated);
    TruncatedObject->SetBoolField(TEXT("montage_sections"), bSectionsTruncated);
    TruncatedObject->SetBoolField(TEXT("montage_slots"), bSlotsTruncated);
    TruncatedObject->SetBoolField(TEXT("montage_segments"), bSegmentsTruncated);

    TSharedPtr<FJsonObject> CountsObject = MakeShared<FJsonObject>();
    CountsObject->SetNumberField(TEXT("notifies_total"), SequenceBase ? SequenceBase->Notifies.Num() : 0);
    CountsObject->SetNumberField(TEXT("notifies_included"), NotifyValues.Num());
    CountsObject->SetNumberField(TEXT("curves_total"), CurveStats.TotalFloatCurves + CurveStats.TotalTransformCurves);
    CountsObject->SetNumberField(TEXT("float_curves_total"), CurveStats.TotalFloatCurves);
    CountsObject->SetNumberField(TEXT("transform_curves_total"), CurveStats.TotalTransformCurves);
    CountsObject->SetNumberField(TEXT("curves_included"), CurveStats.IncludedCurves);
    CountsObject->SetNumberField(TEXT("sync_markers_total"), TotalMarkers);
    CountsObject->SetNumberField(TEXT("sync_markers_included"), SyncMarkerValues.Num());
    CountsObject->SetNumberField(TEXT("branching_points_total"), BranchingIndexValues.Num());

    const bool bPackageDirtyAfter = Package && Package->IsDirty();
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("command"), TEXT("inspect_animation_asset_metadata_guarded"));
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetBoolField(TEXT("asset_modified"), bPackageDirtyAfter != bPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("saves_assets"), false);
    ResultObj->SetBoolField(TEXT("unsupported_class"), !bSupportedClass);
    ResultObj->SetBoolField(TEXT("package_dirty_before"), bPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("package_dirty_after"), bPackageDirtyAfter);
    ResultObj->SetObjectField(TEXT("asset"), AssetObject);
    ResultObj->SetObjectField(TEXT("sequence"), SequenceObject);
    ResultObj->SetObjectField(TEXT("montage"), MontageObject);
    ResultObj->SetArrayField(TEXT("notifies"), NotifyValues);
    ResultObj->SetArrayField(TEXT("curves"), CurveValues);
    ResultObj->SetArrayField(TEXT("sync_markers"), SyncMarkerValues);
    ResultObj->SetObjectField(TEXT("metadata"), MetadataObject);
    ResultObj->SetObjectField(TEXT("limits"), LimitsObject);
    ResultObj->SetObjectField(TEXT("truncated"), TruncatedObject);
    ResultObj->SetObjectField(TEXT("counts"), CountsObject);
    ResultObj->SetArrayField(TEXT("warnings"), WarningValues);
    ResultObj->SetArrayField(TEXT("errors"), ErrorValues);
    ResultObj->SetArrayField(TEXT("unavailable_fields"), UnavailableValues);
    ResultObj->SetStringField(TEXT("scope_note"), TEXT("Editor-only read-only source animation metadata summary. This command does not tick worlds, call notify handlers, compile, save, or mutate assets."));
    return ResultObj;
}
void Init(FResult& R)
{
    R.Data->SetStringField(TEXT("operation"),TEXT("inspect_animation_asset_metadata_guarded"));R.Data->SetBoolField(TEXT("read_only"),true);R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("observed_selected_metadata_changed"),false);R.Data->SetBoolField(TEXT("load_cache_effects_unverified"),true);R.Data->SetBoolField(TEXT("preview_mesh_incompatible_observed"),false);R.Data->SetBoolField(TEXT("nonconst_preview_clear_avoided"),true);R.Data->SetBoolField(TEXT("notify_name_cache_effects_possible"),true);
    auto Counts=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("editor_asset_load_calls"),TEXT("editor_asset_load_returns"),TEXT("static_load_calls"),TEXT("static_load_returns"),TEXT("observation_calls"),TEXT("animation_references_checked"),TEXT("notify_preflight_rows"),TEXT("segment_preflight_rows"),TEXT("source_curve_keys_checked"),TEXT("const_preview_mesh_calls"),TEXT("preview_skeleton_compatibility_calls"),TEXT("curve_key_rows"),TEXT("FrameRateToJsonObject_calls"),TEXT("UObjectSummaryToJson_calls"),TEXT("AnimNotifyEventToJson_calls"),TEXT("FloatCurveToJson_calls"),TEXT("TransformCurveToJson_calls"),TEXT("SyncMarkerToJson_calls"),TEXT("AnimSegmentToJson_calls"),TEXT("AnimNotifiesToJson_calls"),TEXT("BranchingPointsToJson_calls"),TEXT("AnimationCurvesToJson_calls"),TEXT("SyncMarkersToJson_calls"),TEXT("MontageSlotsToJson_calls"),TEXT("MontageSectionsToJson_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("world_tick_calls"),TEXT("notify_handler_calls"),TEXT("runtime_pose_evaluation_calls")}) { Counts->SetNumberField(K,0); }R.Data->SetObjectField(TEXT("native_call_counts"),Counts);
}
}
Variable::FResult Inspect(const FString& AssetPath, bool bIncludeNotifies, bool bIncludeNotifyDetails, bool bIncludeCurves, bool bIncludeCurveKeys, bool bIncludeSyncMarkers, bool bIncludeMontageSections, bool bIncludeMontageSlots, bool bIncludeMetadata, int64 MaxNotifies, int64 MaxCurves, int64 MaxCurveKeysPerCurve, int64 MaxMarkers, int64 MaxSections, int64 MaxSlots, int64 MaxSegmentsPerSlot, bool bRequireSupportedClass)
{
    FResult R;Init(R);if(!IsEditorGameThread()||Current) { R.ErrorCode=TEXT("animation_metadata_editor_state_invalid");R.Error=TEXT("Nonreentrant Editor game thread required");return R; }
    struct FScope { FScope(FResult& Value) { Current=&Value;SerializedRows=0;Held.Reset();HeldObjects.Reset(); }~FScope() { Held.Reset();HeldObjects.Reset();Current=nullptr;SerializedRows=SourceKeysRead=PreflightItems=0; } } Scope(R);
    if(!InputTextSafe(AssetPath,NAME_SIZE-1)) { R.ErrorCode=TEXT("animation_metadata_input_invalid");R.Error=TEXT("Bounded asset path without interior NUL required");return R; }
    R.Stage=TEXT("load_animation_asset");UObject* Object=LoadAssetObjectForRuntimeProbe(AssetPath);
    if(!Object) { if(R.ErrorCode.IsEmpty()) { R.ErrorCode=TEXT("animation_asset_not_found");R.Error=TEXT("Animation asset not found or failed to load: ")+AssetPath; }return R; }
    FOptions Options;
    Options.bIncludeNotifies=bIncludeNotifies;
    Options.bIncludeNotifyDetails=bIncludeNotifyDetails;
    Options.bIncludeCurves=bIncludeCurves;
    Options.bIncludeCurveKeys=bIncludeCurveKeys;
    Options.bIncludeSyncMarkers=bIncludeSyncMarkers;
    Options.bIncludeMontageSections=bIncludeMontageSections;
    Options.bIncludeMontageSlots=bIncludeMontageSlots;
    Options.bIncludeMetadata=bIncludeMetadata;
    Options.bRequireSupportedClass=bRequireSupportedClass;
    Options.MaxNotifies=static_cast<int32>(FMath::Clamp<int64>(MaxNotifies,0,1024));
    Options.MaxCurves=static_cast<int32>(FMath::Clamp<int64>(MaxCurves,0,1024));
    Options.MaxCurveKeysPerCurve=static_cast<int32>(FMath::Clamp<int64>(MaxCurveKeysPerCurve,0,256));
    Options.MaxMarkers=static_cast<int32>(FMath::Clamp<int64>(MaxMarkers,0,1024));
    Options.MaxSections=static_cast<int32>(FMath::Clamp<int64>(MaxSections,0,256));
    Options.MaxSlots=static_cast<int32>(FMath::Clamp<int64>(MaxSlots,0,128));
    Options.MaxSegmentsPerSlot=static_cast<int32>(FMath::Clamp<int64>(MaxSegmentsPerSlot,0,512));
    auto* Asset=Cast<UAnimationAsset>(Object);R.Stage=TEXT("metadata_preflight");
    if(!ReferencesSafe(Asset,bIncludeCurves,bIncludeCurveKeys,bIncludeMetadata)) { R.bChangeStateKnown=false;return R; }
    UPackage* Package=Object->GetOutermost();const bool DirtyBefore=Package&&Package->IsDirty();
    R.Stage=TEXT("observe_animation_metadata");auto Report=Observe(Object,AssetPath,Options);
    const FString First=Serialize(Report);if(!First.IsEmpty()) { R.Data->SetObjectField(TEXT("legacy_result"),Report); }else { R.Data->SetBoolField(TEXT("legacy_result_omitted_for_native_budget"),true); }const bool DirtyAfterFirst=Package&&Package->IsDirty();R.bChanged=DirtyBefore!=DirtyAfterFirst;
    R.Data->SetBoolField(TEXT("package_dirty_before"),DirtyBefore);R.Data->SetBoolField(TEXT("package_dirty_after"),DirtyAfterFirst);
    if(Incomplete()) { R.bChangeStateKnown=false;return R; }
    bool LegacySuccess=false;Report->TryGetBoolField(TEXT("success"),LegacySuccess);
    if(!LegacySuccess) { R.ErrorCode=TEXT("unsupported_animation_asset_class");Report->TryGetStringField(TEXT("error"),R.Error);R.bPostconditionPassed=!R.bChanged;return R; }
    R.Stage=TEXT("verify_current_metadata");if(!Loaded(Object)||!ReferencesSafe(Asset,bIncludeCurves,bIncludeCurveKeys,bIncludeMetadata)) { Unsafe(TEXT("Asset/referenced metadata unavailable before current readback"));R.bChangeStateKnown=false;return R; }
    const auto Readback=Observe(Object,AssetPath,Options);const FString Second=Serialize(Readback);const bool DirtyAfter=Package&&Package->IsDirty();R.bChanged|=DirtyBefore!=DirtyAfter;R.Data->SetBoolField(TEXT("package_dirty_after"),DirtyAfter);
    const bool SelectedChanged=!First.IsEmpty()&&!Second.IsEmpty()&&!First.Equals(Second,ESearchCase::CaseSensitive);R.bChanged|=SelectedChanged;R.Data->SetBoolField(TEXT("observed_selected_metadata_changed"),SelectedChanged);
    if(SelectedChanged) { R.Data->SetObjectField(TEXT("verification_report"),Readback);if(Serialize(R.Data).IsEmpty()) { R.Data->RemoveField(TEXT("verification_report"));R.Data->SetBoolField(TEXT("verification_report_omitted_for_native_budget"),true); } }
    R.Data->SetBoolField(TEXT("selected_metadata_unchanged"),!First.IsEmpty()&&First.Equals(Second,ESearchCase::CaseSensitive));
    if(Incomplete()) { R.bChangeStateKnown=false;return R; }
    R.bPostconditionPassed=!R.bChanged&&!First.IsEmpty()&&First.Equals(Second,ESearchCase::CaseSensitive);
    if(!R.bPostconditionPassed) { R.ErrorCode=TEXT("animation_metadata_current_readback_mismatch");R.Error=TEXT("Selected metadata or package dirty state changed; bounded actual prefix retained, no replay");return R; }
    R.bSuccess=true;R.Stage=TEXT("complete");if(Serialize(R.Data).IsEmpty()) { R.Data->RemoveField(TEXT("legacy_result"));R.Data->SetBoolField(TEXT("legacy_result_omitted_for_native_budget"),true);R.bSuccess=false;R.bPostconditionPassed=false;R.bChangeStateKnown=false; }return R;
}
}
