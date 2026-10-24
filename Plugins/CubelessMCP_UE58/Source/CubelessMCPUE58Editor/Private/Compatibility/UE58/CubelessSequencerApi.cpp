// UE56 fractional playback floor and duplicate cubic insertion through exported runtime APIs.
#include "Compatibility/UE58/CubelessSequencerApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "LevelSequence.h"
#include "LevelSequenceDirector.h"
#include "Internationalization/Text.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Misc/FrameRate.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneFolder.h"
#include "Bindings/MovieSceneSpawnableActorBinding.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSignedObject.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Animation/AnimSequence.h"
#include "Sound/SoundWave.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "LevelSequenceBindingReference.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "MovieSceneBindingReferences.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "MovieSceneFrameMigration.h"
#include "MovieSceneTimeHelpers.h"
#include "Algo/Sort.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "UniversalObjectLocators/ActorLocatorFragment.h"
#include "SubObjectLocator.h"
#include "UObject/UObjectGlobals.h"
#include "Subsystems/AssetEditorSubsystem.h"
namespace Cubeless::UE58::Sequencer
{
namespace
{
constexpr int32 MaxRows = 512, MaxBindings = 128, MaxTracks = 128, MaxSections = 128;
constexpr int32 MaxKeys = 8192, MaxProperties = 256, MaxText = 1024 * 1024;
const TCHAR* ChannelNames[] = { TEXT("Location.X"), TEXT("Location.Y"), TEXT("Location.Z"),
    TEXT("Rotation.X"), TEXT("Rotation.Y"), TEXT("Rotation.Z"), TEXT("Scale.X"), TEXT("Scale.Y"), TEXT("Scale.Z") };

struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false;
    bool bMaintenance = false, bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.Code = Code; R.Error = Error; R.bSuccess = false; return false; }
FString Encode(const FResult& R)
{
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("operation"), R.Operation); J->SetBoolField(TEXT("success"), R.bSuccess);
    J->SetStringField(TEXT("stage"), R.Stage); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.bPost);
    J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false);
    J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    if (R.bMaintenance)
    {
        J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
        J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown);
    }
    else
    {
    J->SetBoolField(TEXT("save_requested"), false); J->SetBoolField(TEXT("save_attempted"), false); J->SetBoolField(TEXT("saved"), false);
    }
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false);
    J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
void Init(FResult& R, const TCHAR* Operation)
{
    R.Operation = Operation;R.Data->SetNumberField(TEXT("asset_load_count"),0);R.Data->SetArrayField(TEXT("asset_load_calls"),{});
    R.Data->SetBoolField(TEXT("asset_load_dispatched"), false); R.Data->SetBoolField(TEXT("asset_created"), false);
    R.Data->SetBoolField(TEXT("runtime_invocation_dispatched"), false); R.Data->SetBoolField(TEXT("explicit_post_edit_change_invoked"), false);
    R.Data->SetBoolField(TEXT("implicit_engine_work_measured"), false); R.Data->SetBoolField(TEXT("saved_asset_runtime_verified"), false);
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Exact current runtime LevelSequence; conditional primary LoadAsset has separate actual receipts and PostLoad/cache effects before the baseline. Public MovieScene declarative data only; Modify/signature propagation/channel-proxy caches are possible; no player/world evaluation, compile or save"));
    R.Data->SetStringField(TEXT("changed_observation_scope"), TEXT("bounded reflected declarative fields, public binding/track/section/channel data and selected package dirty state; signed-object Signature GUID propagation is reported separately"));
    R.Data->SetStringField(TEXT("native_basis"), TEXT("installed UE58 public MovieScene/LevelSequence APIs; no runtime catalog invocation implied"));
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool Native(UClass* C, const TCHAR* Path)
{
    return Loaded(C) && C->GetPathName() == Path && C->HasAnyClassFlags(CLASS_Native)
        && !C->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) && !C->IsEditorOnly()
        && C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn) && !C->GetOutermost()->IsDirty();
}
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("path"), O->GetPathName()); J->SetStringField(TEXT("class"), O->GetClass()->GetPathName());
    J->SetStringField(TEXT("outer"), O->GetOuter() ? O->GetOuter()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("object_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags()))); return J;
}
TSharedRef<FJsonObject> Rate(const FFrameRate& R)
{ auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("numerator"), R.Numerator); J->SetNumberField(TEXT("denominator"), R.Denominator); return J; }
TSharedRef<FJsonObject> Range(const TRange<FFrameNumber>& R)
{
    auto J = MakeShared<FJsonObject>();
    auto Bound = [](const TRangeBound<FFrameNumber>& B)
    {
        auto X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("kind"), B.IsOpen() ? TEXT("open") : B.IsInclusive() ? TEXT("inclusive") : TEXT("exclusive"));
        if (B.IsOpen()) { X->SetField(TEXT("value"), MakeShared<FJsonValueNull>()); } else { X->SetNumberField(TEXT("value"), B.GetValue().Value); } return X;
    };
    J->SetObjectField(TEXT("lower"), Bound(R.GetLowerBound())); J->SetObjectField(TEXT("upper"), Bound(R.GetUpperBound())); return J;
}
// PP uses copied field roles and a plain current fence after each callback-capable export.
// Other existing profiles retain their original observer path.
bool NestedProperties(UStruct* Type,const void* Data,UObject* Owner,const TSet<FName>& Skip,TSharedRef<FJsonObject>& Out,const TFunction<bool()>& Current)
{
    if(!Current()||!Loaded(Type)||!Loaded(Owner))return false;
    TStrongObjectPtr<UStruct> HeldType(Type);TStrongObjectPtr<UObject> HeldOwner(Owner);
    UClass* OwnerClass=Owner->GetClass();TStrongObjectPtr<UClass> HeldClass(OwnerClass);
    struct FFieldRole{FProperty* Property;UStruct* Owner;FName Name;FFieldClass* Class;uint64 Flags;int32 Dim,Size,Offset;};
    TArray<FFieldRole> Fields;TArray<TStrongObjectPtr<UStruct>> Owners;
    for(TFieldIterator<FProperty> It(Type,EFieldIteratorFlags::IncludeSuper);It;++It)
    {
        FProperty* P=*It;if(Fields.Num()>=MaxProperties||P->ArrayDim<1||P->ArrayDim>128)return false;
        FFieldRole Row{P,P->GetOwnerStruct(),P->GetFName(),P->GetClass(),static_cast<uint64>(P->GetPropertyFlags()),P->ArrayDim,P->GetSize(),P->GetOffset_ForInternal()};
        if(!Loaded(Row.Owner))return false;Owners.Emplace(Row.Owner);Fields.Add(Row);
    }
    auto FieldsCurrent=[&]()
    {
        if(!Current()||!Loaded(Type)||!Loaded(Owner)||Owner->GetClass()!=OwnerClass)return false;
        TArray<FProperty*> Inventory;for(TFieldIterator<FProperty> It(Type,EFieldIteratorFlags::IncludeSuper);It;++It){if(Inventory.Num()>=MaxProperties)return false;Inventory.Add(*It);}
        if(Inventory.Num()!=Fields.Num())return false;
        for(int32 I=0;I<Fields.Num();++I)
        {
            const auto& F=Fields[I];if(Inventory[I]!=F.Property)return false;
            if(F.Property->GetOwnerStruct()!=F.Owner||F.Property->GetFName()!=F.Name||F.Property->GetClass()!=F.Class||static_cast<uint64>(F.Property->GetPropertyFlags())!=F.Flags||F.Property->ArrayDim!=F.Dim||F.Property->GetSize()!=F.Size||F.Property->GetOffset_ForInternal()!=F.Offset)return false;
        }
        return true;
    };
    int32 Chars=0;
    for(const auto& F:Fields)
    {
        if(!FieldsCurrent())return false;if(Skip.Contains(F.Name))continue;
        TArray<TSharedPtr<FJsonValue>> Elements;
        for(int32 Index=0;Index<F.Dim;++Index)
        {
            const void* Address=Cast<UClass>(Type)?F.Property->ContainerPtrToValuePtr<void>(Owner,Index):F.Property->ContainerPtrToValuePtr<void>(Data,Index);
            FString Value;const bool Exported=F.Property->ExportText_Direct(Value,Address,Address,Owner,PPF_None);
            if(!FieldsCurrent()||!Exported)return false;Chars+=Value.Len();if(Chars>MaxText||Value.Len()>MaxText)return false;Elements.Add(MakeShared<FJsonValueString>(Value));
        }
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("owner"),F.Owner->GetPathName());Row->SetStringField(TEXT("property_class"),F.Class->GetName());
        Row->SetStringField(TEXT("flags"),LexToString(F.Flags));Row->SetNumberField(TEXT("array_dim"),F.Dim);Row->SetArrayField(TEXT("element_values_text"),Elements);Out->SetObjectField(F.Name.ToString(),Row);
    }
    return FieldsCurrent();
}
bool Properties(UStruct* Type, const void* Data, UObject* Owner, const TSet<FName>& Skip, TSharedRef<FJsonObject>& Out,const TFunction<bool()>& Current={})
{
    if(Current)return NestedProperties(Type,Data,Owner,Skip,Out,Current);
    int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (Skip.Contains(P->GetFName())) { continue; }
        if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 128) { return false; }
        TArray<TSharedPtr<FJsonValue>> Elements;
        for (int32 Index = 0; Index < P->ArrayDim; ++Index)
        {
            FString Value;
            const void* Address = Cast<UClass>(Type) ? P->ContainerPtrToValuePtr<void>(Owner, Index) : P->ContainerPtrToValuePtr<void>(Data, Index);
            if (!P->ExportText_Direct(Value, Address, Address, Owner, PPF_None)) { return false; }
            Chars += Value.Len(); if (Chars > MaxText || Value.Len() > MaxText) { return false; }
            Elements.Add(MakeShared<FJsonValueString>(Value));
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName());
        Row->SetStringField(TEXT("property_class"), P->GetClass()->GetName());
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(P->GetPropertyFlags())));
        Row->SetNumberField(TEXT("array_dim"), P->ArrayDim); Row->SetArrayField(TEXT("element_values_text"), Elements);
        Out->SetObjectField(P->GetName(), Row);
    }
    return true;
}
bool ObjectProperties(UObject* O, const TSet<FName>& ExtraSkip, TSharedRef<FJsonObject>& Out,const TFunction<bool()>& Current={})
{
    TSet<FName> Skip = ExtraSkip; Skip.Add(TEXT("Signature"));
    return Properties(O->GetClass(), O, O, Skip, Out,Current);
}
template<typename ChannelType>
bool Channel(ChannelType* C, const TCHAR* Name, TSharedRef<FJsonObject>& Out)
{
    if (!C) { return false; }
    const auto Times = C->GetTimes(); const auto Values = C->GetValues();
    if (Times.Num() != Values.Num() || Times.Num() > MaxKeys) { return false; }
    Out->SetStringField(TEXT("name"), Name); Out->SetStringField(TEXT("channel_type"), ChannelType::StaticStruct()->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 I = 0; I < Times.Num(); ++I)
    {
        const auto& V = Values[I]; const auto& T = V.Tangent;
        if (!FMath::IsFinite(V.Value) || !FMath::IsFinite(T.ArriveTangent) || !FMath::IsFinite(T.LeaveTangent)
            || !FMath::IsFinite(T.ArriveTangentWeight) || !FMath::IsFinite(T.LeaveTangentWeight)
            || (I && Times[I] < Times[I - 1])) { return false; }
        auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("tick"), Times[I].Value); J->SetNumberField(TEXT("value"), V.Value);
        J->SetNumberField(TEXT("interpolation"), static_cast<uint8>(V.InterpMode)); J->SetNumberField(TEXT("tangent_mode"), static_cast<uint8>(V.TangentMode));
        J->SetNumberField(TEXT("arrive_tangent"), T.ArriveTangent); J->SetNumberField(TEXT("leave_tangent"), T.LeaveTangent);
        J->SetNumberField(TEXT("arrive_weight"), T.ArriveTangentWeight); J->SetNumberField(TEXT("leave_weight"), T.LeaveTangentWeight);
        J->SetNumberField(TEXT("weight_mode"), static_cast<uint8>(T.TangentWeightMode)); Rows.Add(MakeShared<FJsonValueObject>(J));
    }
    Out->SetNumberField(TEXT("key_count"), Rows.Num()); Out->SetArrayField(TEXT("keys"), Rows);
    auto P = MakeShared<FJsonObject>();
    // The public copy constructor intentionally resets transient key handles; compare declarative channel data.
    if (!Properties(ChannelType::StaticStruct(), C, nullptr, { TEXT("Times"), TEXT("Values"), TEXT("KeyHandles") }, P)) { return false; }
    Out->SetObjectField(TEXT("non_key_fields"), P); return true;
}
bool TransformChannels(UMovieSceneSection* S, TArray<FMovieSceneDoubleChannel*>& Out, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(S) || S->GetClass() != UMovieScene3DTransformSection::StaticClass()) { return false; }
    auto& Proxy = S->GetChannelProxy(); auto Doubles = Proxy.GetChannels<FMovieSceneDoubleChannel>();
    auto Floats = Proxy.GetChannels<FMovieSceneFloatChannel>();
    const auto DoubleMetadata = Proxy.GetMetaData<FMovieSceneDoubleChannel>();
    const auto FloatMetadata = Proxy.GetMetaData<FMovieSceneFloatChannel>();
    if (Doubles.Num() != 9 || Floats.Num() != 1 || DoubleMetadata.Num() != 9 || FloatMetadata.Num() != 1
        || Proxy.GetAllEntries().Num() != 2 || FloatMetadata[0].Name != FName(TEXT("Weight"))) { return false; }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 I = 0; I < 9; ++I)
    {
        if (DoubleMetadata[I].Name != FName(ChannelNames[I])
            || Proxy.GetChannelByName<FMovieSceneDoubleChannel>(FName(ChannelNames[I])).Get() != Doubles[I]) { return false; }
        auto C = MakeShared<FJsonObject>(); if (!Channel(Doubles[I], ChannelNames[I], C)) { return false; }
        Out.Add(Doubles[I]); Rows.Add(MakeShared<FJsonValueObject>(C));
    }
    auto Weight = MakeShared<FJsonObject>();
    if (Proxy.GetChannelByName<FMovieSceneFloatChannel>(TEXT("Weight")).Get() != Floats[0] || !Channel(Floats[0], TEXT("Weight"), Weight)) { return false; }
    J->SetArrayField(TEXT("double_channels"), Rows); J->SetObjectField(TEXT("weight_channel"), Weight); return true;
}
bool FloatChannel(UMovieSceneSection* S, FMovieSceneFloatChannel*& Out, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(S) || S->GetClass() != UMovieSceneFloatSection::StaticClass()) { return false; }
    FObjectPropertyBase* Override = FindFProperty<FObjectPropertyBase>(S->GetClass(), TEXT("OverrideRegistry"));
    if (!Override || Override->ArrayDim != 1 || Override->GetOwnerClass() != UMovieSceneFloatSection::StaticClass() || Override->GetObjectPropertyValue_InContainer(S)) { return false; }
    auto& Proxy = S->GetChannelProxy(); auto Floats = Proxy.GetChannels<FMovieSceneFloatChannel>(); auto Metadata = Proxy.GetMetaData<FMovieSceneFloatChannel>();
    if (Floats.Num() != 1 || Metadata.Num() != 1 || !Metadata[0].Name.IsNone() || Proxy.GetChannels<FMovieSceneDoubleChannel>().Num() != 0 || Proxy.GetAllEntries().Num() != 1 || Floats[0] != &static_cast<UMovieSceneFloatSection*>(S)->GetChannel()) { return false; }
    Out = Floats[0]; J->SetStringField(TEXT("metadata_name"), Metadata[0].Name.ToString()); J->SetBoolField(TEXT("override_registry_null"), true);
    return Channel(Out, TEXT("Value"), J);
}
FString BindingName(UMovieScene* Scene, const FGuid& Id)
{
    if (const auto* P = Scene->FindPossessable(Id)) { return P->GetName(); }
    if (const auto* S = Scene->FindSpawnable(Id)) { return S->GetName(); } return FString();
}
// ZF: these are retained child clips, not playback/evaluation or new clip factories.
// In particular Audio::CacheChannelProxy calls SetupSoundInputParameters and must
// not be invoked merely to observe a readonly source. Copy the public channels.
int32 SubsequenceClipKind(UMovieSceneTrack* Track)
{
    if(!Loaded(Track))return 0;
    return Track->GetClass()==UMovieSceneSkeletalAnimationTrack::StaticClass()?1:Track->GetClass()==UMovieSceneAudioTrack::StaticClass()?2:0;
}
bool SubsequenceClipSlot(UMovieSceneSection* Section,int32 Kind,TObjectPtr<UObject>& Source)
{
    Source=nullptr;
    if(!Loaded(Section))return false;
    if(Kind==1)
    {
        if(Section->GetClass()!=UMovieSceneSkeletalAnimationSection::StaticClass())return false;
        const auto& Params=static_cast<UMovieSceneSkeletalAnimationSection*>(Section)->Params;
        if(!Params.Animation.IsResolved()||!Params.MirrorDataTable.IsResolved()||Params.MirrorDataTable!=nullptr||Params.PlayRate.GetType()!=EMovieSceneTimeWarpType::FixedPlayRate||!FMath::IsFinite(Params.PlayRate.AsFixedPlayRate()))return false;
        Source=Params.Animation;return true;
    }
    if(Kind!=2||Section->GetClass()!=UMovieSceneAudioSection::StaticClass())return false;
    auto* SoundField=FindFProperty<FObjectProperty>(UMovieSceneAudioSection::StaticClass(),TEXT("Sound"));
    auto* AttenuationField=FindFProperty<FObjectProperty>(UMovieSceneAudioSection::StaticClass(),TEXT("AttenuationSettings"));
    if(!SoundField||!AttenuationField||SoundField->GetOwnerClass()!=UMovieSceneAudioSection::StaticClass()||AttenuationField->GetOwnerClass()!=UMovieSceneAudioSection::StaticClass()||SoundField->ArrayDim!=1||AttenuationField->ArrayDim!=1)return false;
    const auto SoundSlot=SoundField->GetObjectPtrPropertyValue_InContainer(Section),AttenuationSlot=AttenuationField->GetObjectPtrPropertyValue_InContainer(Section);
    if(!SoundSlot.IsResolved()||!AttenuationSlot.IsResolved()||AttenuationSlot!=nullptr)return false;
    Source=SoundSlot;return true;
}
bool SubsequenceClipReference(UMovieSceneSection* Section,int32 Kind,UObject*& Source)
{
    TObjectPtr<UObject> Slot;Source=nullptr;if(!SubsequenceClipSlot(Section,Kind,Slot))return false;
    Source=Slot.Get();return true; // Only initial/admitted observation; caller immediately holds the returned role.
}
bool SubsequenceClipChannels(UMovieSceneSection* Section,int32 Kind,TSharedRef<FJsonObject>& Out,const TFunction<bool()>& Current={})
{
    if(Current&&!Current())return false;UObject* Source=nullptr;const bool ReferenceRead=SubsequenceClipReference(Section,Kind,Source);TStrongObjectPtr<UObject> ReturnedSource(Source);
    if(!ReferenceRead||(Current&&!Current()))return false;
    Out->SetStringField(TEXT("source"),Source?Source->GetPathName():TEXT(""));
    if(Kind==1)
    {
        const FMovieSceneSkeletalAnimationParams Params=static_cast<UMovieSceneSkeletalAnimationSection*>(Section)->Params;
        FMovieSceneFloatChannel Weight=Params.Weight;auto Curve=MakeShared<FJsonObject>();
        if(!Channel(&Weight,TEXT("Weight"),Curve)||(Current&&!Current()))return false;
        Out->SetObjectField(TEXT("weight"),Curve);Out->SetStringField(TEXT("fixed_play_rate"),LexToString(Params.PlayRate.AsFixedPlayRate()));
        return true; // Params and section authored offsets/flags are in the existing reflected snapshot.
    }
    auto* Audio=static_cast<UMovieSceneAudioSection*>(Section);int32 InputCount=0;
    Audio->ForEachInput([&InputCount](FName,const FMovieSceneFloatChannel&){++InputCount;});
    Audio->ForEachInput([&InputCount](FName,const FMovieSceneBoolChannel&){++InputCount;});
    Audio->ForEachInput([&InputCount](FName,const FMovieSceneIntegerChannel&){++InputCount;});
    Audio->ForEachInput([&InputCount](FName,const FMovieSceneStringChannel&){++InputCount;});
    Audio->ForEachInput([&InputCount](FName,const FMovieSceneAudioTriggerChannel&){++InputCount;});
    if(InputCount!=0||(Current&&!Current()))return false; // Parameterized sound-input families remain pending, rather than populating them here.
    FMovieSceneFloatChannel Volume=Audio->GetSoundVolumeChannel(),Pitch=Audio->GetPitchMultiplierChannel();
    auto VolumeRow=MakeShared<FJsonObject>(),PitchRow=MakeShared<FJsonObject>();
    if(!Channel(&Volume,TEXT("SoundVolume"),VolumeRow)||(Current&&!Current())||!Channel(&Pitch,TEXT("PitchMultiplier"),PitchRow)||(Current&&!Current()))return false;
    Out->SetObjectField(TEXT("volume"),VolumeRow);Out->SetObjectField(TEXT("pitch_multiplier"),PitchRow);return true;
}
struct FCameraInsertionScope;
struct FCameraMaintenanceScope;
bool CameraMaintenanceCheck(FCameraMaintenanceScope* Scope,const TCHAR* Stage,bool SaveReturned=false);
void CameraMaintenanceAbort(FCameraMaintenanceScope* Scope,const TCHAR* Stage);
bool CameraMaintenancePlainCheck(FCameraMaintenanceScope* Scope,const TCHAR* Stage);
bool CameraLiteral(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,int32 Depth);
struct FContext;
bool CameraMaintenanceObserve(FCameraMaintenanceScope* Scope,FContext& C,const TSharedRef<FJsonObject>& State,const TCHAR* Stage);
bool CameraMaintenanceMixedSnapshot(FCameraMaintenanceScope* Scope,TSharedRef<FJsonObject>& State);
bool CameraMaintenanceMixedBinding(FCameraMaintenanceScope* Scope,const FGuid& Id,const UObject* Object);
// JS: exact readonly leaf siblings only; no recursive composition resolver.
bool SubsequenceSiblingLeafAdmission(FContext& C,FResult& R);
bool CameraTrackIdentity(FContext& C,UMovieSceneCameraCutTrack* Track);
bool CameraSectionIdentity(UMovieSceneCameraCutTrack* Track,UMovieSceneCameraCutSection* Section);
const TArray<UMovieSceneSection*>& CameraSections(UMovieSceneCameraCutTrack* Track);
bool CameraInsertionActive=false,CameraInsertionQuarantined=false;
bool CameraInsertionCheck(FCameraInsertionScope* Scope,const TCHAR* Stage,bool SaveReturned=false);
// WR: the standard Director is an embedded Blueprint (SequenceEditor creates it
// with Sequence as Outer). Observe its authored source, not compiler/search caches.
struct FDirectorReadScope
{
    TStrongObjectPtr<ULevelSequence> Sequence;TStrongObjectPtr<UBlueprint> Blueprint;
    TStrongObjectPtr<UClass> Generated,Skeleton,Parent;TStrongObjectPtr<UObject> GeneratedDefault,SkeletonDefault;
    UClass* GeneratedSuper=nullptr;UClass* SkeletonSuper=nullptr;
    TStrongObjectPtr<UClass> SerializedDirectorClass;FObjectProperty* DirectorClassField=nullptr;
    FFieldClass* DirectorClassKind=nullptr;int32 DirectorClassOffset=0;uint64 DirectorClassFlags=0;
    bool DirectorClassGeneratedTransition=false;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    struct FHold{TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;};
    TArray<FHold> Holds;TArray<UEdGraph*> Graphs;TMap<UEdGraph*,TArray<UEdGraphNode*>> Nodes;
    TSharedPtr<FJsonObject> Baseline,FirstMismatch;bool Ready=false,Stopped=false,Observing=false;
    bool Bad(const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual)
    {
        if(!Stopped){FirstMismatch=MakeShared<FJsonObject>();FirstMismatch->SetStringField(TEXT("object"),Object);FirstMismatch->SetStringField(TEXT("property"),Property);FirstMismatch->SetStringField(TEXT("expected"),Expected);FirstMismatch->SetStringField(TEXT("actual"),Actual);}
        Stopped=true;return false;
    }
    bool Apply(FResult& R,const TCHAR* Stage)
    {
        if(!Stopped)return true;if(FirstMismatch.IsValid())R.Data->SetObjectField(TEXT("director_first_mismatch"),FirstMismatch);
        R.Stage=Stage;R.bKnown=false;R.bPost=false;
        return Fail(R,TEXT("sequencer_director_source_changed_or_pending"),TEXT("Existing Director source/current observation failed or is outside the admitted profile; actual operation/save prefix retained. No Director creation, compile, execution, load or retry."));
    }
    bool Pin(UObject* O)
    {
        if(!Loaded(O)||!Loaded(O->GetClass())||(O->GetOuter()&&!Loaded(O->GetOuter())))return Bad(TEXT("Director"),TEXT("held_object"),TEXT("loaded object/class/actual outer"),TEXT("unavailable"));
        for(const auto& H:Holds)if(H.Object.Get()==O)return true;
        if(Holds.Num()>1024)return Bad(TEXT("Director"),TEXT("object_bound"),TEXT("<=1024"),LexToString(Holds.Num()));
        FHold H;H.Object.Reset(O);H.Class=O->GetClass();H.Outer=O->GetOuter();H.Package=O->GetPackage();H.Path=O->GetPathName();H.Flags=O->GetFlags();Holds.Add(MoveTemp(H));return true;
    }
    bool Gather(TArray<UEdGraph*>& Out,TMap<UEdGraph*,TArray<UEdGraphNode*>>& Members)
    {
        if(!Blueprint.IsValid())return false;
        const auto AddRoot=[&](const auto& Roots)->bool{for(const auto& Slot:Roots){if(!Slot.IsResolved())return Bad(Blueprint->GetPathName(),TEXT("graph_resolved"),TEXT("already loaded root graph"),TEXT("unresolved"));auto* G=Slot.Get();if(G&&!Out.Contains(G))Out.Add(G);}return true;};
        if(Blueprint->GetExtensions().Num()||!Blueprint->ComponentTemplates.IsEmpty()||!Blueprint->Timelines.IsEmpty())return Bad(Blueprint->GetPathName(),TEXT("extended_director_profile_pending"),TEXT("ordinary Director without extension/component/timeline owned source"),TEXT("additional owned source requires implementation"));
        if(!AddRoot(Blueprint->UbergraphPages)||!AddRoot(Blueprint->FunctionGraphs)||!AddRoot(Blueprint->MacroGraphs)||!AddRoot(Blueprint->DelegateSignatureGraphs))return false;
        auto* InterfaceField=FindFProperty<FClassProperty>(FBPInterfaceDescription::StaticStruct(),TEXT("Interface"));
        for(const auto& Interface:Blueprint->ImplementedInterfaces)
        {
            if(!InterfaceField)return Bad(Blueprint->GetPathName(),TEXT("interface_descriptor"),TEXT("native class field"),TEXT("missing"));
            const auto Slot=InterfaceField->GetObjectPtrPropertyValue_InContainer(&Interface);if(!Slot.IsResolved())return Bad(Blueprint->GetPathName(),TEXT("interface_resolved"),TEXT("already loaded nullable interface"),TEXT("unresolved"));if(Slot.Get()&&!Pin(Slot.Get()))return false;
            if(!AddRoot(Interface.Graphs))return false;
        }
        for(int32 I=0;I<Out.Num();++I)
        {
            auto* G=Out[I];if(Out.Num()>64||!Loaded(G)||G->GetClass()!=UEdGraph::StaticClass()||!G->IsIn(Blueprint.Get()))return Bad(Blueprint->GetPathName(),TEXT("owned_graph"),TEXT("<=64 loaded standard graphs embedded under Director"),TEXT("custom/external/unavailable"));
            if(!AddRoot(G->SubGraphs))return false;
            TArray<UEdGraphNode*> Rows;
            for(const auto& Slot:G->Nodes){if(!Slot.IsResolved())return Bad(G->GetPathName(),TEXT("node_resolved"),TEXT("loaded node"),TEXT("unresolved"));auto* N=Slot.Get();if(!Loaded(N)||N->GetGraph()!=G||!N->GetClass()->HasAnyClassFlags(CLASS_Native)||Rows.Contains(N))return Bad(G->GetPathName(),TEXT("node_owner"),TEXT("unique loaded native node in actual graph"),TEXT("custom/duplicate/unavailable"));Rows.Add(N);}
            if(Rows.Num()>256)return Bad(G->GetPathName(),TEXT("node_bound"),TEXT("<=256"),LexToString(Rows.Num()));
            Rows.Sort([](const UEdGraphNode& A,const UEdGraphNode& B){return A.GetPathName()<B.GetPathName();});Members.Add(G,MoveTemp(Rows));
        }
        Out.Sort([](const UEdGraph& A,const UEdGraph& B){return A.GetPathName()<B.GetPathName();});return true;
    }
    static bool ReadSlot(ULevelSequence* Owner,UBlueprint*& Value)
    {
        auto* Field=FindFProperty<FObjectProperty>(ULevelSequence::StaticClass(),TEXT("DirectorBlueprint"));
        if(!Loaded(Owner)||!Field||Field->GetOwnerClass()!=ULevelSequence::StaticClass()||Field->ArrayDim!=1||Field->HasSetterOrGetter())return false;
        const auto Slot=Field->GetObjectPtrPropertyValue_InContainer(Owner);if(!Slot.IsResolved())return false;
        auto* Object=Slot.Get();Value=Cast<UBlueprint>(Object);return !Object||Value;
    }
    bool ReadDirectorClass(UClass*& Value,bool Capture=false)
    {
        auto* Field=FindFProperty<FObjectProperty>(ULevelSequence::StaticClass(),TEXT("DirectorClass"));
        if(!Field||Field->GetOwnerClass()!=ULevelSequence::StaticClass()||Field->ArrayDim!=1||Field->ElementSize!=sizeof(TObjectPtr<UClass>)||Field->HasSetterOrGetter()
            ||!Field->PropertyClass.IsResolved()||Field->PropertyClass.Get()!=UClass::StaticClass())return Bad(Sequence->GetPathName(),TEXT("serialized_director_class_descriptor"),TEXT("native resolved UClass slot"),TEXT("unavailable/changed"));
        if(!Capture&&(Field!=DirectorClassField||Field->GetClass()!=DirectorClassKind||Field->GetOffset_ForInternal()!=DirectorClassOffset||uint64(Field->GetPropertyFlags())!=DirectorClassFlags))
            return Bad(Sequence->GetPathName(),TEXT("serialized_director_class_layout"),TEXT("captured actual descriptor/layout"),TEXT("changed"));
        const auto Slot=Field->GetObjectPtrPropertyValue_InContainer(Sequence.Get());if(!Slot.IsResolved())return Bad(Sequence->GetPathName(),TEXT("serialized_director_class_resolved"),TEXT("already resolved nullable class"),TEXT("unresolved"));
        auto* Object=Slot.Get();Value=Cast<UClass>(Object);if(Object&&(!Value||!Loaded(Value)))return Bad(Sequence->GetPathName(),TEXT("serialized_director_class_loaded"),TEXT("loaded nullable UClass"),TEXT("unavailable"));
        if(Capture){DirectorClassField=Field;DirectorClassKind=Field->GetClass();DirectorClassOffset=Field->GetOffset_ForInternal();DirectorClassFlags=uint64(Field->GetPropertyFlags());}
        return true;
    }
    bool Plain(bool Saved=false)
    {
        if(Stopped)return false;
        if(!IsInGameThread()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage()||GEditor!=Editor.Get()||!Loaded(Editor.Get())||!Loaded(World.Get())||GEditor->PlayWorld||GEditor->GetEditorWorldContext().World()!=World.Get()||World->bIsTearingDown)
            return Bad(TEXT("Director"),TEXT("editor_world"),TEXT("same idle Editor/world outside GC/save"),TEXT("changed"));
        for(auto& H:Holds)
        {
            auto* O=H.Object.Get();if(!Loaded(O)||!Loaded(H.Class)||(H.Outer&&!Loaded(H.Outer))||O->GetClass()!=H.Class||O->GetOuter()!=H.Outer||O->GetPackage()!=H.Package||O->GetPathName()!=H.Path)return Bad(H.Path,TEXT("identity_class_outer"),TEXT("captured actual object/owner"),TEXT("changed"));
            const auto Flags=O->GetFlags();if(Flags!=H.Flags){const uint32 Added=uint32(Flags)&~uint32(H.Flags),Removed=uint32(H.Flags)&~uint32(Flags);if(!Saved||Removed||(Added&~uint32(RF_WasLoaded|RF_LoadCompleted)))return Bad(H.Path,TEXT("flags"),LexToString(uint32(H.Flags)),LexToString(uint32(Flags)));H.Flags=Flags;}
        }
        UBlueprint* CurrentDirector=nullptr;
        if(!Sequence.IsValid()||!Blueprint.IsValid()||!ReadSlot(Sequence.Get(),CurrentDirector)||CurrentDirector!=Blueprint.Get()||!Blueprint->GeneratedClass.GetGCPtr().IsResolved()||!Blueprint->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!Blueprint->ParentClass.GetGCPtr().IsResolved()
            ||Blueprint->GeneratedClass.Get()!=Generated.Get()||Blueprint->SkeletonGeneratedClass.Get()!=Skeleton.Get()||Blueprint->ParentClass.Get()!=Parent.Get()
            ||Blueprint->bBeingCompiled||Blueprint->bIsRegeneratingOnLoad||Blueprint->bQueuedForCompilation||Blueprint->Status!=BS_UpToDate)
            return Bad(Blueprint.IsValid()?Blueprint->GetPathName():TEXT("Director"),TEXT("blueprint_class_roles"),TEXT("same idle up-to-date Generated/Skeleton/Parent slots"),TEXT("changed or compilation required"));
        if(!Generated->ClassGeneratedBy.IsResolved()||Generated->ClassGeneratedBy.Get()!=Blueprint.Get()||!Skeleton->ClassGeneratedBy.IsResolved()||Skeleton->ClassGeneratedBy.Get()!=Blueprint.Get()
            ||Generated->GetSuperClass()!=GeneratedSuper||Skeleton->GetSuperClass()!=SkeletonSuper)return Bad(Blueprint->GetPathName(),TEXT("generated_source_parent"),TEXT("captured actual Generated/Skeleton declaring roles"),TEXT("changed"));
        UClass* CurrentDirectorClass=nullptr;if(!ReadDirectorClass(CurrentDirectorClass))return false;
        if(CurrentDirectorClass!=SerializedDirectorClass.Get())
        {
            // LevelSequence::SetDirectorBlueprint / OnDirectorRecompiled assign exactly
            // the same BP's current GeneratedClass. Preserve an initial null/stale
            // role, but permit only that source-backed transition, never arbitrary null/class.
            if(CurrentDirectorClass!=Generated.Get()||!CurrentDirectorClass->ClassGeneratedBy.IsResolved()||CurrentDirectorClass->ClassGeneratedBy.Get()!=Blueprint.Get())
                return Bad(Sequence->GetPathName(),TEXT("serialized_director_class"),SerializedDirectorClass.IsValid()?SerializedDirectorClass->GetPathName():TEXT("null"),CurrentDirectorClass?CurrentDirectorClass->GetPathName():TEXT("null"));
            if(!Pin(CurrentDirectorClass))return false;SerializedDirectorClass.Reset(CurrentDirectorClass);DirectorClassGeneratedTransition=true;
        }
        if(Generated->GetDefaultObject(false)!=GeneratedDefault.Get()||Skeleton->GetDefaultObject(false)!=SkeletonDefault.Get())return Bad(Blueprint->GetPathName(),TEXT("existing_cdo_roles"),TEXT("captured nullable existing CDOs; no creation"),TEXT("changed"));
        if(Ready){TArray<UEdGraph*> Now;TMap<UEdGraph*,TArray<UEdGraphNode*>> Members;if(!Gather(Now,Members))return false;if(Now!=Graphs)return Bad(Blueprint->GetPathName(),TEXT("graph_membership"),TEXT("captured owned graph set"),TEXT("changed"));for(auto* G:Graphs)if(Members.FindChecked(G)!=Nodes.FindChecked(G))return Bad(G->GetPathName(),TEXT("node_membership"),TEXT("captured owned node set"),TEXT("changed"));}
        return true;
    }
    bool Fields(UObject* O,const TSet<FName>& Extra,TSharedRef<FJsonObject>& Out,bool Saved)
    {
        TSet<FName> Skip=Extra;
        for(TFieldIterator<FProperty> It(O->GetClass());It;++It)if(It->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_SkipSerialization))Skip.Add(It->GetFName());
        const TFunction<bool()> Current=[this,Saved]{return Plain(Saved);};
        return ObjectProperties(O,Skip,Out,Current)||Bad(O->GetPathName(),TEXT("authored_fields"),TEXT("complete bounded current field export"),TEXT("incomplete"));
    }
    static FString PinKey(const UEdGraphPin* P)
    {
        FString Key=LexToString(uint8(P->Direction))+TEXT(":")+P->PinName.ToString();int32 Depth=0;
        for(auto* ParentPin=P->ParentPin;ParentPin;ParentPin=ParentPin->ParentPin){if(++Depth>16||!P->GetOwningNodeUnchecked()->Pins.Contains(ParentPin))return FString();Key=ParentPin->PinName.ToString()+TEXT("/")+Key;}
        return Key;
    }
    bool Observe(TSharedRef<FJsonObject>& Out,bool Saved=false)
    {
        if(!Plain(Saved))return false;if(Observing)return true;TGuardValue<bool> Guard(Observing,true);
        auto BPFields=MakeShared<FJsonObject>();
        const TSet<FName> Skip={TEXT("GeneratedClass"),TEXT("SkeletonGeneratedClass"),TEXT("UbergraphPages"),TEXT("FunctionGraphs"),TEXT("MacroGraphs"),TEXT("DelegateSignatureGraphs"),TEXT("IntermediateGeneratedGraphs"),TEXT("LastEditedDocuments"),TEXT("UpgradeNotesLog"),TEXT("CachedUDSDependencies"),TEXT("CachedDependencies"),TEXT("CachedNames")};
        if(!Fields(Blueprint.Get(),Skip,BPFields,Saved))return false;Out->SetObjectField(TEXT("blueprint_authored_fields"),BPFields);
        const auto Roots=[&](const auto& Slots){TArray<FString> Names;for(const auto& Slot:Slots)if(Slot.Get())Names.Add(Slot.Get()->GetPathName());Names.Sort();TArray<TSharedPtr<FJsonValue>> Values;for(const auto& Name:Names)Values.Add(MakeShared<FJsonValueString>(Name));return Values;};
        auto RootRoles=MakeShared<FJsonObject>();RootRoles->SetArrayField(TEXT("ubergraph"),Roots(Blueprint->UbergraphPages));RootRoles->SetArrayField(TEXT("functions"),Roots(Blueprint->FunctionGraphs));RootRoles->SetArrayField(TEXT("macros"),Roots(Blueprint->MacroGraphs));RootRoles->SetArrayField(TEXT("delegate_signatures"),Roots(Blueprint->DelegateSignatureGraphs));Out->SetObjectField(TEXT("graph_roles"),RootRoles);
        for(const auto& Role: {TPair<const TCHAR*,UObject*>(TEXT("generated_cdo"),GeneratedDefault.Get()),TPair<const TCHAR*,UObject*>(TEXT("skeleton_cdo"),SkeletonDefault.Get())})
        {
            auto Defaults=MakeShared<FJsonObject>();if(Role.Value&&!Fields(Role.Value,{},Defaults,Saved))return false;Out->SetObjectField(Role.Key,Defaults);
        }
        // Logical pin addresses/links: no physical Pins array order or pin allocation
        // identity is made an authored invariant. Ambiguous same-name split pins
        // remain incomplete instead of inventing occurrence/order semantics.
        const auto PinAddresses=[&](TMap<const UEdGraphPin*,FString>& Addresses)->bool
        {
            for(auto* G:Graphs)for(auto* N:Nodes.FindChecked(G))
            {
                TSet<FString> Names;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N||P->WasTrashed())return Bad(N->GetPathName(),TEXT("pin_current"),TEXT("live owned pin"),TEXT("invalid"));const FString Key=PinKey(P);if(Key.IsEmpty()||Names.Contains(Key))return Bad(N->GetPathName(),TEXT("logical_pin_key"),TEXT("unique name/direction/split path"),Key);Names.Add(Key);Addresses.Add(P,N->GetPathName()+TEXT("|")+Key);}
            }
            return true;
        };
        auto GraphRows=MakeShared<FJsonObject>();Out->SetObjectField(TEXT("authored_graphs"),GraphRows);
        for(auto* G:Graphs)
        {
            auto Row=MakeShared<FJsonObject>(),Props=MakeShared<FJsonObject>();if(!Fields(G,{TEXT("Nodes"),TEXT("SubGraphs")},Props,Saved))return false;Row->SetObjectField(TEXT("fields"),Props);Row->SetArrayField(TEXT("subgraphs"),Roots(G->SubGraphs));
            auto NodeRows=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("nodes"),NodeRows);GraphRows->SetObjectField(G->GetPathName(),Row);
            for(auto* N:Nodes.FindChecked(G))
            {
                auto Node=MakeShared<FJsonObject>(),Values=MakeShared<FJsonObject>();if(!Fields(N,{TEXT("ErrorType"),TEXT("ErrorMsg"),TEXT("bHasCompilerMessage")},Values,Saved))return false;Node->SetObjectField(TEXT("fields"),Values);auto Pins=MakeShared<FJsonObject>();Node->SetObjectField(TEXT("logical_pins"),Pins);NodeRows->SetObjectField(N->GetPathName(),Node);
                TMap<const UEdGraphPin*,FString> InitialAddresses;if(!PinAddresses(InitialAddresses))return false;
                TArray<FString> CurrentKeys;for(auto* P:N->Pins)CurrentKeys.Add(PinKey(P));CurrentKeys.Sort();
                for(const auto& Key:CurrentKeys)
                {
                    if(!Plain(Saved))return false;
                    TMap<const UEdGraphPin*,FString> Addresses;if(!PinAddresses(Addresses))return false;
                    UEdGraphPin* P=nullptr;for(auto* Candidate:N->Pins)if(PinKey(Candidate)==Key){P=Candidate;break;}
                    if(!P)return Bad(N->GetPathName(),TEXT("logical_pin_inventory"),Key,TEXT("missing"));
                    auto Pin=MakeShared<FJsonObject>();Pin->SetStringField(TEXT("default_value"),P->DefaultValue);// AutogeneratedDefaultValue is schema/compiler derived; authored DefaultValue remains protected.
                    // Copy all physical-pin data before any observer callback; reacquire by logical key next iteration.
                    const FText TextCopy=P->DefaultTextValue;FEdGraphPinType Type=P->PinType;
                    if(!P->DefaultObject.IsResolved()||!P->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved()||P->PinType.PinSubCategoryObject.IsStale()||P->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(N->GetPathName(),TEXT("pin_reference_resolved"),TEXT("already resolved nullable pin references"),TEXT("unresolved"));
                    auto* DefaultObject=P->DefaultObject.Get();auto* SubType=P->PinType.PinSubCategoryObject.Get();auto* Member=P->PinType.PinSubCategoryMemberReference.MemberParent.Get();auto* Terminal=P->PinType.PinValueType.TerminalSubCategoryObject.Get();if((DefaultObject&&!PinRef(DefaultObject))||(SubType&&!PinRef(SubType))||(Member&&!PinRef(Member))||(Terminal&&!PinRef(Terminal)))return false;
                    Pin->SetStringField(TEXT("default_object"),DefaultObject?DefaultObject->GetPathName():FString());
                    TArray<FString> Links;for(auto* Link:P->LinkedTo){const auto* Target=Addresses.Find(Link);if(!Target)return Bad(N->GetPathName(),TEXT("pin_link"),TEXT("current pin inside captured Director graphs"),TEXT("external or retired"));Links.Add(*Target);}Links.Sort();TArray<TSharedPtr<FJsonValue>> LinkJson;for(const auto& Link:Links)LinkJson.Add(MakeShared<FJsonValueString>(Link));Pin->SetArrayField(TEXT("links"),LinkJson);
                    FString DefaultText;FTextStringHelper::WriteToBuffer(DefaultText,TextCopy);if(!Plain(Saved))return false;Pin->SetStringField(TEXT("default_text"),DefaultText);
                    auto TypeRow=MakeShared<FJsonObject>();const TFunction<bool()> Current=[this,Saved]{return Plain(Saved);};TSet<FName> TypeSkip;for(TFieldIterator<FProperty> It(FEdGraphPinType::StaticStruct());It;++It)if(It->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_SkipSerialization))TypeSkip.Add(It->GetFName());if(!Properties(FEdGraphPinType::StaticStruct(),&Type,N,TypeSkip,TypeRow,Current))return Bad(N->GetPathName(),TEXT("pin_type"),TEXT("bounded reflected pin type"),TEXT("incomplete"));Pin->SetObjectField(TEXT("type"),TypeRow);Pins->SetObjectField(Key,Pin);
                }
            }
        }
        if(!Plain(Saved))return false;
        if(Baseline.IsValid()&&!CameraLiteral(MakeShared<FJsonValueObject>(Baseline),MakeShared<FJsonValueObject>(Out),0))return Bad(Blueprint->GetPathName(),TEXT("authored_source"),Text(Baseline.ToSharedRef()),Text(Out));
        if(!Baseline.IsValid())Baseline=Out;return true;
    }
    bool PinRef(UObject* O){return Pin(O);}
    bool Bind(ULevelSequence* In)
    {
        Editor.Reset(GEditor);if(!Loaded(Editor.Get()))return Bad(TEXT("Director"),TEXT("editor"),TEXT("loaded Editor"),TEXT("unavailable"));World.Reset(GEditor->GetEditorWorldContext().World());Sequence.Reset(In);
        auto* Field=FindFProperty<FObjectProperty>(ULevelSequence::StaticClass(),TEXT("DirectorBlueprint"));
        if(!Field||Field->GetOwnerClass()!=ULevelSequence::StaticClass()||Field->HasSetterOrGetter())return Bad(In->GetPathName(),TEXT("director_slot"),TEXT("native DirectorBlueprint property"),TEXT("unavailable"));
        const auto Slot=Field->GetObjectPtrPropertyValue_InContainer(In);if(!Slot.IsResolved())return Bad(In->GetPathName(),TEXT("director_resolved"),TEXT("already resolved after original sequence load"),TEXT("unresolved"));
        auto* BP=Cast<UBlueprint>(Slot.Get());Blueprint.Reset(BP);
        if(!Loaded(BP)||BP->GetClass()!=UBlueprint::StaticClass()||BP->GetOuter()!=In||BP->BlueprintType!=BPTYPE_Normal||BP->Status!=BS_UpToDate||BP->bBeingCompiled||BP->bIsRegeneratingOnLoad||BP->bQueuedForCompilation
            ||!BP->ParentClass.GetGCPtr().IsResolved()||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved())
            return Bad(In->GetPathName(),TEXT("director_profile_pending"),TEXT("loaded up-to-date ordinary Sequence-owned Director Blueprint"),TEXT("other ownership/class/status or unresolved source"));
        Parent.Reset(BP->ParentClass.Get());Generated.Reset(BP->GeneratedClass.Get());Skeleton.Reset(BP->SkeletonGeneratedClass.Get());
        if(!Loaded(Parent.Get())||!Parent->HasAnyClassFlags(CLASS_Native)||!Parent->IsChildOf(ULevelSequenceDirector::StaticClass())||!Loaded(Generated.Get())||!Loaded(Skeleton.Get())
            ||!Generated->ClassGeneratedBy.IsResolved()||Generated->ClassGeneratedBy.Get()!=BP||!Skeleton->ClassGeneratedBy.IsResolved()||Skeleton->ClassGeneratedBy.Get()!=BP)
            return Bad(BP->GetPathName(),TEXT("director_generated_roles_pending"),TEXT("loaded generated/skeleton with actual native Director parent"),TEXT("missing or inherited/custom source"));
        UClass* InitialDirectorClass=nullptr;if(!ReadDirectorClass(InitialDirectorClass,true))return false;SerializedDirectorClass.Reset(InitialDirectorClass);
        if(InitialDirectorClass&&!Pin(InitialDirectorClass))return false;
        GeneratedSuper=Generated->GetSuperClass();SkeletonSuper=Skeleton->GetSuperClass();
        if((GeneratedSuper&&!Pin(GeneratedSuper))||(SkeletonSuper&&!Pin(SkeletonSuper)))return false;
        GeneratedDefault.Reset(Generated->GetDefaultObject(false));SkeletonDefault.Reset(Skeleton->GetDefaultObject(false));
        for(UObject* O:{static_cast<UObject*>(In),static_cast<UObject*>(BP),static_cast<UObject*>(Parent.Get()),static_cast<UObject*>(Generated.Get()),static_cast<UObject*>(Skeleton.Get())})if(!Pin(O))return false;
        if((GeneratedDefault.IsValid()&&!Pin(GeneratedDefault.Get()))||(SkeletonDefault.IsValid()&&!Pin(SkeletonDefault.Get())))return false;
        if(!Gather(Graphs,Nodes))return false;for(auto* G:Graphs){if(!Pin(G)||!Pin(G->GetClass()))return false;for(auto* N:Nodes.FindChecked(G))if(!Pin(N)||!Pin(N->GetClass()))return false;}
        Ready=true;auto State=MakeShared<FJsonObject>();return Observe(State);
    }
};
struct FContext
{
    ULevelSequence* Sequence = nullptr; UMovieScene* Scene = nullptr;
    TUniquePtr<FDirectorReadScope> Director;bool DirectorRequired=false;
    TStrongObjectPtr<ULevelSequence> KeepSequence;TStrongObjectPtr<UMovieScene> KeepScene;TStrongObjectPtr<UEditorEngine> ResolveEditor;TStrongObjectPtr<UWorld> ResolveWorld;TStrongObjectPtr<UPackage> ResolvePackage;FString ResolvePath;
    FGuid Binding; UMovieSceneTrack* Track = nullptr; UMovieSceneSection* Section = nullptr;
    TSharedRef<FJsonObject> Before = MakeShared<FJsonObject>();
    bool bFloat = false;
    TArray<TStrongObjectPtr<UMovieSceneTrack>> MaintenanceTracks;
    bool bSubsequence = false;
    bool bNestedSubsequence = false; // PP opt-in, existing flat observers stay unchanged.
    bool bLeafRemoval = false; // QJ component declaration removal uses the existing fenced observer.
    bool bMixedRemoval = false; // UA collateral spawnable/folder/tag snapshot, selected parentless possessable only.
    bool bHierarchyRemoval = false; // UM selected parentless Actor; copied child parent GUIDs stay literal.
    bool bCamera = false;
    bool bSubsequenceClips = false; // ZF child / ADJ removal readonly clip observers; never a new clip writer.
    bool bRemovalClips = false; // ADJ exact parent Audio/SkeletalAnimation selected and collateral tracks.
    FCameraInsertionScope* CameraInsertion=nullptr; // JA concrete same-call scope, other routes stay null.
    FCameraMaintenanceScope* CameraMaintenance=nullptr; // JL CameraCut-preserving removal/subsequence only.
};
bool DirectorCurrent(FContext& C,FResult& R,const TCHAR* Stage,bool Saved=false)
{
    if(!C.Director){if(!C.DirectorRequired)return true;UBlueprint* Value=nullptr;if(FDirectorReadScope::ReadSlot(C.Sequence,Value)&&!Value)return true;R.bKnown=false;return Fail(R,TEXT("sequencer_director_unadmitted_role"),TEXT("Director appeared after initial admission; no further SDK or retry."));}
    auto State=MakeShared<FJsonObject>();if(!C.Director->Observe(State,Saved))return C.Director->Apply(R,Stage);
    R.Data->SetBoolField(TEXT("director_class_same_generated_transition_observed"),C.Director->DirectorClassGeneratedTransition);
    return true;
}
bool DirectorAllowed(const FContext& C){UBlueprint* Value=nullptr;return FDirectorReadScope::ReadSlot(C.Sequence,Value)&&(!Value||C.Director.IsValid());}
// RN primary-only resolver. Internal/adopted source rechecks retain find-only resolution.
bool SequenceLoadActive=false;
bool PrimaryResolveContext(const FContext& C)
{
    return IsEditorGameThread()&&Loaded(C.ResolveEditor.Get())&&GEditor==C.ResolveEditor.Get()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!IsPlaySessionActive()&&Loaded(C.ResolveWorld.Get())&&GEditor->GetEditorWorldContext().World()==C.ResolveWorld.Get()&&C.ResolveWorld->WorldType==EWorldType::Editor&&!C.ResolveWorld->bIsTearingDown;
}
bool ResolveStop(FResult& R,const TCHAR* Code,const TCHAR* Why)
{R.bKnown=false;R.bPost=false;R.Data->SetBoolField(TEXT("primary_resolution_continuation_safe"),false);return Fail(R,Code,Why);}
bool PrimaryResolvedCurrent(const FContext& C,FResult& R)
{
    if(!PrimaryResolveContext(C)||!Loaded(C.KeepSequence.Get())||C.Sequence!=C.KeepSequence.Get()||!Loaded(C.ResolvePackage.Get())||C.Sequence->GetClass()!=ULevelSequence::StaticClass()||C.Sequence->GetOuter()!=C.ResolvePackage.Get()||C.Sequence->GetPackage()!=C.ResolvePackage.Get()||!C.Sequence->GetPathName().Equals(C.ResolvePath,ESearchCase::CaseSensitive)||FindObject<ULevelSequence>(nullptr,*C.ResolvePath)!=C.Sequence||!Loaded(C.KeepScene.Get())||C.Scene!=C.KeepScene.Get()||C.Scene->GetClass()!=UMovieScene::StaticClass()||C.Scene->GetOuter()!=C.Sequence)return ResolveStop(R,TEXT("sequencer_load_current_lost"),TEXT("Held primary sequence/MovieScene/Editor identity no longer current; no next SDK or retry."));
    UMovieScene* CurrentScene=C.Sequence->GetMovieScene();if(!PrimaryResolveContext(C)||!Loaded(C.KeepSequence.Get())||!Loaded(C.KeepScene.Get())||!Loaded(C.ResolvePackage.Get())||FindObject<ULevelSequence>(nullptr,*C.ResolvePath)!=C.KeepSequence.Get()||C.KeepSequence->GetClass()!=ULevelSequence::StaticClass()||C.KeepSequence->GetOuter()!=C.ResolvePackage.Get()||C.KeepScene->GetClass()!=UMovieScene::StaticClass()||C.KeepScene->GetOuter()!=C.KeepSequence.Get()||CurrentScene!=C.KeepScene.Get())return ResolveStop(R,TEXT("sequencer_load_movie_scene_changed"),TEXT("Primary sequence MovieScene getter returned another role or context; no next SDK."));return true;
}
bool Resolve(const FString& Path,FContext& C,FResult& R,bool PermitPrimaryLoad=false)
{
    if(!IsEditorGameThread()||IsPlaySessionActive())return Fail(R,TEXT("sequencer_editor_state_invalid"),TEXT("Requires the Editor game thread outside PIE/SIE."));
    FString Package,Name;
    if(!FPackageName::IsValidObjectPath(Path)||!Path.Split(TEXT("."),&Package,&Name,ESearchCase::CaseSensitive,ESearchDir::FromEnd)||!Package.StartsWith(TEXT("/Game/"))||Name.IsEmpty())return Fail(R,TEXT("sequencer_exact_loaded_path_required"),TEXT("Exact canonical /Game LevelSequence object path required; primary cold loading does not add name search or path rewriting."));
    if(PermitPrimaryLoad)
    {
        if(!Loaded(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())return Fail(R,TEXT("sequencer_load_editor_required"),TEXT("Idle Editor required before primary sequence resolution."));
        C.ResolveEditor.Reset(GEditor);C.ResolveWorld.Reset(GEditor->GetEditorWorldContext().World());C.ResolvePath=Path;
        if(!PrimaryResolveContext(C))return Fail(R,TEXT("sequencer_load_world_required"),TEXT("Current live Editor world required before primary sequence resolution."));
    }
    C.Sequence=FindObject<ULevelSequence>(nullptr,*Path);C.KeepSequence.Reset(C.Sequence);
    if(!C.Sequence&&PermitPrimaryLoad)
    {
        if(SequenceLoadActive)return Fail(R,TEXT("sequencer_load_reentry"),TEXT("Sequence LoadAsset callback reentry rejected before another loader; no retry."));
        auto Row=MakeShared<FJsonObject>();auto Calls=R.Data->GetArrayField(TEXT("asset_load_calls"));Calls.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("asset_load_calls"),Calls);R.Data->SetNumberField(TEXT("asset_load_count"),R.Data->GetNumberField(TEXT("asset_load_count"))+1);R.Data->SetBoolField(TEXT("asset_load_dispatched"),true);
        Row->SetStringField(TEXT("route"),TEXT("UEditorAssetLibrary::LoadAsset"));Row->SetStringField(TEXT("requested_path"),Path);Row->SetStringField(TEXT("stage"),R.Stage);Row->SetBoolField(TEXT("attempted"),true);Row->SetBoolField(TEXT("returned"),false);R.Data->SetBoolField(TEXT("load_full_effect_state_known"),false);
        TGuardValue<bool> Active(SequenceLoadActive,true);R.bKnown=false;
        UObject* Returned=UEditorAssetLibrary::LoadAsset(Path);TStrongObjectPtr<UObject> HoldReturned(Returned);Row->SetBoolField(TEXT("returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),Returned!=nullptr);
        if(!PrimaryResolveContext(C))return ResolveStop(R,TEXT("sequencer_load_context_changed"),TEXT("Original LoadAsset returned outside its held Editor/world/GC/save boundary; no returned-object getter or next SDK."));
        if(!Returned){R.bKnown=true;return Fail(R,TEXT("sequencer_sequence_not_found"),FString::Printf(TEXT("Level Sequence not found: %s"),*Path));}
        if(!Loaded(Returned))return ResolveStop(R,TEXT("sequencer_load_object_unavailable"),TEXT("LoadAsset returned an incomplete or retired object; no getter/retry."));
        TStrongObjectPtr<UClass> ReturnedClass(Returned->GetClass());if(!Loaded(ReturnedClass.Get()))return ResolveStop(R,TEXT("sequencer_load_class_unavailable"),TEXT("Returned asset class must be live before classification."));
        TStrongObjectPtr<UObject> ReturnedOuter(Returned->GetOuter());if(!Loaded(ReturnedOuter.Get())||!Loaded(ReturnedOuter->GetClass()))return ResolveStop(R,TEXT("sequencer_load_outer_unavailable"),TEXT("Returned asset outer must be live before package/identity read."));
        TStrongObjectPtr<UPackage> ReturnedPackage(Cast<UPackage>(ReturnedOuter.Get()));
        if(!Loaded(ReturnedPackage.Get())||Returned->GetPackage()!=ReturnedPackage.Get()||ReturnedPackage->GetName()!=Package||!Returned->GetPathName().Equals(Path,ESearchCase::CaseSensitive)||FindObject<UObject>(nullptr,*Path)!=Returned)return ResolveStop(R,TEXT("sequencer_load_identity_changed"),TEXT("LoadAsset result must remain the exact requested top-level object/package, including after redirects; no substitution or retry."));
        Row->SetObjectField(TEXT("returned_object"),Identity(Returned));
        C.Sequence=Cast<ULevelSequence>(Returned);C.KeepSequence.Reset(C.Sequence);C.ResolvePackage.Reset(ReturnedPackage.Get());
        if(!C.Sequence){R.bKnown=true;return Fail(R,TEXT("sequencer_sequence_not_found"),FString::Printf(TEXT("Level Sequence not found: %s"),*Path));}
    }
    if(!Loaded(C.Sequence)||C.Sequence->GetPathName()!=Path||C.Sequence->GetClass()!=ULevelSequence::StaticClass()
        ||!Native(ULevelSequence::StaticClass(),TEXT("/Script/LevelSequence.LevelSequence"))||!Native(UMovieScene::StaticClass(),TEXT("/Script/MovieScene.MovieScene"))
        ||!Native(UMovieScene3DTransformTrack::StaticClass(),TEXT("/Script/MovieSceneTracks.MovieScene3DTransformTrack"))||!Native(UMovieScene3DTransformSection::StaticClass(),TEXT("/Script/MovieSceneTracks.MovieScene3DTransformSection"))
        ||C.Sequence->GetOuter()!=C.Sequence->GetOutermost()||C.Sequence->GetOutermost()->GetName()!=Package||C.Sequence->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)||C.Sequence->IsEditorOnly()||C.Sequence->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor|PKG_EditorOnly|PKG_CompiledIn))
    {if(PermitPrimaryLoad)R.bKnown=true;return Fail(R,TEXT("sequencer_loaded_native_identity_required"),TEXT("Exact idle loaded native runtime LevelSequence/class/package identity is required; subclasses, transient sequences and other mounts are not admitted."));}
    if(PermitPrimaryLoad)C.ResolvePackage.Reset(C.Sequence->GetPackage());
    C.KeepSequence.Reset(C.Sequence);C.Scene=C.Sequence->GetMovieScene();C.KeepScene.Reset(C.Scene);
    if(PermitPrimaryLoad&&(!PrimaryResolveContext(C)||!Loaded(C.KeepSequence.Get())||!Loaded(C.ResolvePackage.Get())||FindObject<ULevelSequence>(nullptr,*Path)!=C.KeepSequence.Get()||C.KeepSequence->GetClass()!=ULevelSequence::StaticClass()||C.KeepSequence->GetOuter()!=C.ResolvePackage.Get()))return ResolveStop(R,TEXT("sequencer_load_scene_return_context"),TEXT("MovieScene getter returned outside the original primary target/context; no scene getter or next SDK."));
    if(!Loaded(C.Scene)||C.Scene->GetClass()!=UMovieScene::StaticClass()||C.Scene->GetOuter()!=C.Sequence||C.Scene->GetDisplayRate().Numerator<=0||C.Scene->GetDisplayRate().Denominator<=0||C.Scene->GetTickResolution().Numerator<=0||C.Scene->GetTickResolution().Denominator<=0)
    {if(PermitPrimaryLoad)R.bKnown=true;return Fail(R,TEXT("sequencer_movie_scene_invalid"),TEXT("Requires an owned exact loaded MovieScene and positive finite display/tick rates."));}
    if(PermitPrimaryLoad)
    {
        if(!PrimaryResolvedCurrent(C,R))return false;R.bKnown=true;R.Data->SetBoolField(TEXT("primary_resolution_continuation_safe"),true);R.Data->SetStringField(TEXT("resolved_asset_path"),Path);R.Data->SetStringField(TEXT("resolved_asset_class"),C.Sequence->GetClass()->GetPathName());R.Data->SetStringField(TEXT("resolved_movie_scene_path"),C.Scene->GetPathName());
    }
    R.Data->SetStringField(TEXT("sequence_path"),Path);R.Data->SetStringField(TEXT("movie_scene_path"),C.Scene->GetPathName());
    // Empty operation is the existing nested/sibling admission context of the same three maintenance APIs.
    C.DirectorRequired=R.Operation.IsEmpty()||R.Operation==TEXT("add_level_sequence_camera_cut")||R.Operation==TEXT("add_level_sequence_subsequence")||R.Operation==TEXT("remove_level_sequence_binding");
    if(!C.DirectorRequired)return true;
    auto* DirectorField=FindFProperty<FObjectProperty>(ULevelSequence::StaticClass(),TEXT("DirectorBlueprint"));
    if(!DirectorField)return Fail(R,TEXT("sequencer_director_slot_unavailable"),TEXT("Native DirectorBlueprint field unavailable."));
    const auto DirectorSlot=DirectorField->GetObjectPtrPropertyValue_InContainer(C.Sequence);
    if(!DirectorSlot.IsResolved())return Fail(R,TEXT("sequencer_director_resolved_required"),TEXT("Director reference must already be resolved after original primary resolution; no extra loader."));
    if(DirectorSlot.Get()){C.Director=MakeUnique<FDirectorReadScope>();if(!C.Director->Bind(C.Sequence))return C.Director->Apply(R,TEXT("director_read_admission"));R.Data->SetBoolField(TEXT("director_collateral_readonly"),true);R.Data->SetStringField(TEXT("director_scope"),TEXT("Existing up-to-date standard Sequence-owned Director; loaded native-parent Generated/Skeleton/nullable existing CDO and authored graphs/defaults preserved. No Director creation, explicit compile/event execution/loader. Save search caches and transient compiler metadata are excluded. Other ownership/inherited/custom, extension/component/timeline source and ambiguous logical-pin profiles remain incomplete."));}
    return true;
}

bool Tick(double DisplayFrame, const FContext& C, FFrameNumber& Out)
{
    if (!FMath::IsFinite(DisplayFrame) || DisplayFrame < static_cast<double>(MIN_int32) || DisplayFrame > static_cast<double>(MAX_int32)) { return false; }
    const FFrameTime Source = FFrameTime::FromDecimal(DisplayFrame);
    const FFrameRate Display = C.Scene->GetDisplayRate(), Resolution = C.Scene->GetTickResolution();
    // Check the scaled decimal before TransformTime can narrow an overflowing frame number.
    const double Scaled = Source.AsDecimal() * (static_cast<double>(Resolution.Numerator) * Display.Denominator)
        / (static_cast<double>(Resolution.Denominator) * Display.Numerator);
    if (!FMath::IsFinite(Scaled) || Scaled < static_cast<double>(MIN_int32) || Scaled > static_cast<double>(MAX_int32)) { return false; }
    const FFrameTime Converted = FFrameRate::TransformTime(Source, Display, Resolution);
    const double Raw = Converted.AsDecimal();
    if (!FMath::IsFinite(Raw) || Raw < static_cast<double>(MIN_int32) || Raw > static_cast<double>(MAX_int32)) { return false; }
    Out = Converted.FloorToFrame(); return true;
}
bool Snapshot(FContext& C, TSharedRef<FJsonObject>& J)
{
    const TFunction<bool()> Current=(C.bNestedSubsequence||C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval||C.bSubsequenceClips)?TFunction<bool()>([&C]{return CameraMaintenancePlainCheck(C.CameraMaintenance,TEXT("nested_snapshot_export_return"));}):TFunction<bool()>();
    if(Current&&!Current())return false;
    if(C.Director){auto Director=MakeShared<FJsonObject>();if(!C.Director->Observe(Director))return false;J->SetObjectField(TEXT("director_authored_source"),Director);}
    if (C.bFloat) { J->SetBoolField(TEXT("complete"), false); }
    if (!Loaded(C.Sequence) || !Loaded(C.Scene) || C.Sequence->GetMovieScene() != C.Scene || C.Scene->GetOuter() != C.Sequence) { return false; }
    auto Basis = MakeShared<FJsonObject>();
    if (C.bFloat) { J->SetObjectField(TEXT("native_class_basis"), Basis); }
    TArray<UClass*> SourceTypes = { ULevelSequence::StaticClass(), UMovieScene::StaticClass(), UMovieScene3DTransformTrack::StaticClass(), UMovieScene3DTransformSection::StaticClass() };
    if (C.bFloat) { SourceTypes.Add(UMovieSceneFloatTrack::StaticClass()); SourceTypes.Add(UMovieSceneFloatSection::StaticClass()); }
    if (C.bSubsequence) { SourceTypes.Add(UMovieSceneSubTrack::StaticClass()); SourceTypes.Add(UMovieSceneSubSection::StaticClass()); }
    if (C.bCamera) { SourceTypes.Add(UMovieSceneCameraCutTrack::StaticClass()); SourceTypes.Add(UMovieSceneCameraCutSection::StaticClass()); }
    if (C.bSubsequenceClips) { SourceTypes.Append({UMovieSceneSkeletalAnimationTrack::StaticClass(),UMovieSceneSkeletalAnimationSection::StaticClass(),UMovieSceneAudioTrack::StaticClass(),UMovieSceneAudioSection::StaticClass()}); }
    for (UClass* Type : SourceTypes)
    {
        if (!Loaded(Type) || !Type->HasAnyClassFlags(CLASS_Native) || Type->GetOutermost()->IsDirty()) { return false; }
        auto Row = Identity(Type); Row->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(Type->GetClassFlags())));
        Row->SetStringField(TEXT("package_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(Type->GetOutermost()->GetPackageFlags())));
        Row->SetBoolField(TEXT("package_dirty"), Type->GetOutermost()->IsDirty()); Basis->SetObjectField(Type->GetPathName(), Row);
    }
    J->SetObjectField(TEXT("native_class_basis"), Basis);
    J->SetObjectField(TEXT("sequence"), Identity(C.Sequence)); J->SetObjectField(TEXT("movie_scene"), Identity(C.Scene));
    J->SetBoolField(TEXT("dirty"), C.Sequence->GetOutermost()->IsDirty());
    J->SetStringField(TEXT("sequence_signature"), C.Sequence->GetSignature().ToString()); J->SetStringField(TEXT("movie_scene_signature"), C.Scene->GetSignature().ToString());
    J->SetObjectField(TEXT("display_rate"), Rate(C.Scene->GetDisplayRate())); J->SetObjectField(TEXT("tick_resolution"), Rate(C.Scene->GetTickResolution()));
    J->SetObjectField(TEXT("playback_range"), Range(C.Scene->GetPlaybackRange()));
    auto SP = MakeShared<FJsonObject>(); auto MP = MakeShared<FJsonObject>();
    if (C.bFloat) { J->SetObjectField(TEXT("sequence_fields"), SP); J->SetObjectField(TEXT("movie_scene_fields"), MP); }
    if (!ObjectProperties(C.Sequence, C.Director?TSet<FName>{TEXT("CompiledData"),TEXT("DirectorClass")}:TSet<FName>{}, SP,Current) || !ObjectProperties(C.Scene, { TEXT("ObjectBindings"), TEXT("PlaybackRange"), TEXT("EditorData") }, MP,Current)) { return false; }
    J->SetObjectField(TEXT("sequence_fields"), SP); J->SetObjectField(TEXT("movie_scene_fields"), MP);
    auto EditorData = MakeShared<FJsonObject>();
    if (C.bFloat) { J->SetObjectField(TEXT("editor_data"), EditorData); }
    const FMovieSceneEditorData EditorCopy=C.Scene->GetEditorData();
    if (!Properties(FMovieSceneEditorData::StaticStruct(), (C.bNestedSubsequence||C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval||C.bSubsequenceClips)?&EditorCopy:&C.Scene->GetEditorData(), C.Scene, {}, EditorData,Current)) { return false; }
    J->SetObjectField(TEXT("editor_data"), EditorData);
    const auto Bindings = static_cast<const UMovieScene*>(C.Scene)->GetBindings();
    const TArray<UMovieSceneTrack*> RootInventory=C.Scene->GetTracks();
    if (Bindings.Num() > MaxBindings || C.Scene->GetTracks().Num() > MaxTracks) { return false; }
    TArray<TSharedPtr<FJsonValue>> BindingRows, RootRows; TSet<UMovieSceneTrack*> Seen;
    auto Tracks = MakeShared<FJsonObject>(); int32 TotalTracks = 0, TotalSections = 0;
    if (C.bFloat) { J->SetObjectField(TEXT("tracks"), Tracks); }
    auto ObserveTrack = [&](UMovieSceneTrack* T)
    {
        if (!Loaded(T) || T->GetOuter() != C.Scene || Seen.Contains(T) || ++TotalTracks > MaxTracks) { return false; }
        Seen.Add(T); auto TJ = Identity(T); auto Fields = MakeShared<FJsonObject>();
        if (C.bFloat) { Tracks->SetObjectField(T->GetPathName(), TJ); TJ->SetObjectField(TEXT("fields"), Fields); }
        const bool Selected = T == C.Track;
        if (!ObjectProperties(T, Selected ? TSet<FName>{ TEXT("Sections") } : TSet<FName>{}, Fields,Current)) { return false; }
        TJ->SetObjectField(TEXT("fields"), Fields); TArray<TSharedPtr<FJsonValue>> Sections;
        const auto SectionInventory=T->GetAllSections();
        for (UMovieSceneSection* S : SectionInventory)
        {
            if (!Loaded(S) || S->GetOuter() != T || ++TotalSections > MaxSections) { return false; }
            auto SJ = Identity(S); auto Props = MakeShared<FJsonObject>();
            if (C.bFloat) { Sections.Add(MakeShared<FJsonValueObject>(SJ)); TJ->SetArrayField(TEXT("sections"), Sections); SJ->SetObjectField(TEXT("fields"), Props); }
            const bool SelectedSection = S == C.Section;
            const TSet<FName> SelectedSkip = C.bFloat ? TSet<FName>{ TEXT("FloatCurve") } : TSet<FName>{ TEXT("Translation"), TEXT("Rotation"), TEXT("Scale") };
            if (!ObjectProperties(S, SelectedSection ? SelectedSkip : TSet<FName>{}, Props,Current)) { return false; }
            SJ->SetObjectField(TEXT("fields"), Props); SJ->SetObjectField(TEXT("range"), Range(S->GetRange()));
            if (SelectedSection)
            {
                if (C.bFloat) { FMovieSceneFloatChannel* Scalar = nullptr; auto Curve = MakeShared<FJsonObject>(); SJ->SetObjectField(TEXT("float_channel"), Curve); if (!FloatChannel(S, Scalar, Curve)) { return false; } }
                else { TArray<FMovieSceneDoubleChannel*> Channels; if (!TransformChannels(S, Channels, SJ)) { return false; } }
            }
            if (!C.bFloat) { Sections.Add(MakeShared<FJsonValueObject>(SJ)); }
        }
        TJ->SetArrayField(TEXT("sections"), Sections); Tracks->SetObjectField(T->GetPathName(), TJ); return true;
    };
    for (const FMovieSceneBinding& B : Bindings)
    {
        if (!B.GetObjectGuid().IsValid()) { return false; }
        auto BJ = MakeShared<FJsonObject>(); BJ->SetStringField(TEXT("binding_id"), B.GetObjectGuid().ToString(EGuidFormats::DigitsWithHyphens));
        BJ->SetStringField(TEXT("name"), BindingName(C.Scene, B.GetObjectGuid())); TArray<TSharedPtr<FJsonValue>> Paths;
        if (C.bFloat) { BindingRows.Add(MakeShared<FJsonValueObject>(BJ)); J->SetArrayField(TEXT("bindings"), BindingRows); }
        for (UMovieSceneTrack* T : B.GetTracks()) { if (!ObserveTrack(T)) { return false; } Paths.Add(MakeShared<FJsonValueString>(T->GetPathName())); }
        BJ->SetArrayField(TEXT("tracks"), Paths); if (!C.bFloat) { BindingRows.Add(MakeShared<FJsonValueObject>(BJ)); }
    }
    for (UMovieSceneTrack* T : RootInventory) { if (!ObserveTrack(T)) { return false; } RootRows.Add(MakeShared<FJsonValueString>(T->GetPathName())); }
    UMovieSceneTrack* Camera = C.Scene->GetCameraCutTrack();
    if (Camera && !Seen.Contains(Camera) && !ObserveTrack(Camera)) { return false; }
    J->SetStringField(TEXT("camera_cut_track"), Camera ? Camera->GetPathName() : TEXT(""));
    J->SetArrayField(TEXT("bindings"), BindingRows); J->SetArrayField(TEXT("root_tracks"), RootRows); J->SetObjectField(TEXT("tracks"), Tracks);
    J->SetNumberField(TEXT("track_count"), TotalTracks); J->SetNumberField(TEXT("section_count"), TotalSections); J->SetBoolField(TEXT("complete"), true); return true;
}
bool Same(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B)
{ return Text(A) == Text(B); }
bool SameField(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, const TCHAR* Key)
{
    auto X = MakeShared<FJsonObject>(); auto Y = MakeShared<FJsonObject>();
    X->SetField(TEXT("value"), A->TryGetField(Key)); Y->SetField(TEXT("value"), B->TryGetField(Key)); return Same(X, Y);
}
bool Before(FContext& C, FResult& R)
{
    if (C.bFloat) { R.Data->SetObjectField(TEXT("before"), C.Before); }
    if(!DirectorCurrent(C,R,TEXT("director_before_snapshot")))return false;
    if (!Snapshot(C, C.Before)) { return Fail(R, TEXT("sequencer_snapshot_incomplete"), TEXT("Bounded declarative snapshot/channel shape is not complete; no mutation was dispatched.")); }
    if(!DirectorCurrent(C,R,TEXT("director_before_snapshot_return")))return false;
    R.Data->SetObjectField(TEXT("before"), C.Before); return true;
}
bool CaptureAfter(FContext& C, FResult& R, TSharedRef<FJsonObject>& After)
{
    if (C.bFloat) { R.Data->SetObjectField(TEXT("after"), After); }
    if(!DirectorCurrent(C,R,TEXT("director_after_snapshot")))return false;
    if (!Snapshot(C, After))
    {
        R.bKnown = false; R.Data->SetBoolField(TEXT("after_complete"), false);
        return Fail(R, TEXT("sequencer_post_state_unobservable"), TEXT("Mutation may be retained; the bounded post-state is incomplete. No rollback, retry, compile or save."));
    }
    if(!DirectorCurrent(C,R,TEXT("director_after_snapshot_return")))return false;
    R.Data->SetObjectField(TEXT("after"), After); R.Data->SetBoolField(TEXT("after_complete"), true); R.bKnown = true;
    R.bChanged = !Same(C.Before, After); return true;
}
bool CommonPreserved(FContext& C, const TSharedRef<FJsonObject>& After, bool bRangeMayChange)
{
    for (const TCHAR* K : { TEXT("sequence"), TEXT("movie_scene"), TEXT("sequence_fields"), TEXT("movie_scene_fields"),
        TEXT("display_rate"), TEXT("tick_resolution"), TEXT("root_tracks"), TEXT("camera_cut_track"), TEXT("native_class_basis") })
    { if (!SameField(C.Before, After, K)) { return false; } }
    return bRangeMayChange || (SameField(C.Before, After, TEXT("playback_range")) && SameField(C.Before, After, TEXT("editor_data")));
}
bool ResolveBinding(FContext& C, const FString& Id, const FString& Label, FResult& R)
{
    if ((!Id.IsEmpty() && (!FGuid::Parse(Id, C.Binding) || !C.Binding.IsValid())) || Id.Len() > 128 || Label.Len() > 512)
    { return Fail(R, TEXT("sequencer_binding_selector_invalid"), TEXT("Use a valid binding GUID or bounded exact binding name.")); }
    const auto& Bs = static_cast<const UMovieScene*>(C.Scene)->GetBindings();
    if (Bs.Num() > MaxBindings) { return Fail(R, TEXT("sequencer_binding_budget_exceeded"), TEXT("At most 128 existing bindings are admitted.")); }
    for (const FMovieSceneBinding& B : Bs)
    {
        if ((!Id.IsEmpty() && B.GetObjectGuid() == C.Binding) || (Id.IsEmpty() && !Label.IsEmpty() && BindingName(C.Scene, B.GetObjectGuid()) == Label))
        { C.Binding = B.GetObjectGuid(); R.Data->SetStringField(TEXT("binding_id"), C.Binding.ToString(EGuidFormats::DigitsWithHyphens)); return true; }
    }
    return Fail(R, TEXT("sequencer_binding_not_found"), TEXT("Selector must identify a current binding; GUID membership is verified before any track allocation."));
}
// ACU: the original raw command retains row/group/axis order. Named callers
// still pass strict ordinary row arrays and never create this first-row marker.
bool UnpackOriginalKeyRows(const TSharedPtr<FJsonValue>& Root,const TCHAR* Kind,TArray<TSharedPtr<FJsonValue>>& RawRows,FResult& R)
{
    if(!Root||Root->Type!=EJson::Array||Root->AsArray().IsEmpty())return true;
    const auto First=Root->AsArray()[0];if(!First||First->Type!=EJson::Object)return true;
    const auto Header=First->AsObject();if(Header->HasField(TEXT("frame"))||!Header->HasField(TEXT("__cubeless_original_sequence_keys_v1")))return true;
    FString Tag;const auto Original=Header->TryGetField(TEXT("first"));
    if(Header->Values.Num()!=2||!Header->HasTypedField<EJson::String>(TEXT("__cubeless_original_sequence_keys_v1"))||!Header->TryGetStringField(TEXT("__cubeless_original_sequence_keys_v1"),Tag)||Tag!=Kind||!Original)
        return Fail(R,TEXT("sequencer_raw_key_marker_invalid"),TEXT("Exact operation-specific internal first-row marker required; no key writer."));
    RawRows=Root->AsArray();RawRows[0]=Original;
    R.Data->SetBoolField(TEXT("original_raw_ordered_keys"),true);R.Data->SetBoolField(TEXT("raw_key_rows_prevalidated"),false);
    R.Data->SetStringField(TEXT("raw_key_scope"),TEXT("Original typed frame and ordered row/group/axis conversion with actual insertion prefix, late invalid input without final MarkPackageDirty/save. Existing native sequence/track/section/source, finite rows/keys/time/value bounds and public exact-save protection remain. Wrong first section replacement, generated/custom sources, runtime/playback and full original scope remain incomplete."));return true;
}
bool OriginalKeyCurrent(FContext& C,FResult& R,bool Attached=true)
{
    if(!PrimaryResolvedCurrent(C,R))return false;
    if(!C.Binding.IsValid()||!C.Scene->FindBinding(C.Binding))return ResolveStop(R,TEXT("sequencer_raw_key_binding_lost"),TEXT("Originally resolved binding GUID is no longer present after an SDK boundary; no next track/key writer or save."));
    UClass* TrackClass=C.bFloat?UMovieSceneFloatTrack::StaticClass():UMovieScene3DTransformTrack::StaticClass();UClass* SectionClass=C.bFloat?UMovieSceneFloatSection::StaticClass():UMovieScene3DTransformSection::StaticClass();
    if((C.Track&&(!Loaded(C.Track)||C.Track->GetClass()!=TrackClass||C.Track->GetOuter()!=C.Scene||!C.Scene->FindTracks(TrackClass,C.Binding).Contains(C.Track)))||(C.Section&&(!Loaded(C.Section)||C.Section->GetClass()!=SectionClass||C.Section->GetOuter()!=C.Track||C.Section->IsReadOnly()||(Attached&&(!C.Track||!C.Track->GetAllSections().Contains(C.Section))))))
        return ResolveStop(R,TEXT("sequencer_raw_key_current_lost"),TEXT("Held selected native track/section ownership or membership changed at an SDK boundary; actual prefix retained, no next writer/save."));
    return true;
}
bool OriginalKeyFailure(FResult& R,int32 RowIndex,const TCHAR* Group,int32 Axis,const TSharedPtr<FJsonValue>& Requested,const TCHAR* Code,const TCHAR* Error)
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("row_index"),RowIndex);J->SetStringField(TEXT("group"),Group);J->SetNumberField(TEXT("axis"),Axis);J->SetField(TEXT("requested"),Requested?Requested:MakeShared<FJsonValueNull>());J->SetStringField(TEXT("stage"),R.Stage);R.Data->SetObjectField(TEXT("raw_key_first_failure"),J);return Fail(R,Code,Error);
}
struct FKeyRow { double DisplayFrame = 0; FFrameNumber Time; TOptional<double> Values[9]; };
bool ParseKeys(const FString& Source, FContext& C, TArray<FKeyRow>& Rows, TArray<TSharedPtr<FJsonValue>>& RawRows, FResult& R)
{
    TSharedPtr<FJsonValue> Root;
    if (Source.Len() > MaxText || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Root) || !Root.IsValid() || Root->Type != EJson::Array)
    { return Fail(R, TEXT("sequencer_keys_json_invalid"), TEXT("keys_json must be a bounded nonempty array of finite key objects.")); }
    const auto& Input = Root->AsArray();
    if (Input.IsEmpty() || Input.Num() > MaxRows) { return Fail(R, TEXT("sequencer_keys_count_invalid"), TEXT("Provide 1..512 key rows.")); }
    if(!UnpackOriginalKeyRows(Root,TEXT("transform"),RawRows,R))return false;if(!RawRows.IsEmpty())return true;
    for (const auto& Value : Input)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object) { return Fail(R, TEXT("sequencer_key_row_invalid"), TEXT("Every row must be an object.")); }
        auto J = Value->AsObject(); FKeyRow Row;
        for (const auto& P : J->Values)
        {
            const FString Key(*P.Key);
            if (Key != TEXT("frame") && Key != TEXT("location") && Key != TEXT("rotation") && Key != TEXT("scale"))
            { return Fail(R, TEXT("sequencer_key_field_unsupported"), TEXT("Only frame/location/rotation/scale are admitted.")); }
        }
        const auto Frame = J->TryGetField(TEXT("frame"));
        if (!Frame.IsValid() || Frame->Type != EJson::Number || !Frame->TryGetNumber(Row.DisplayFrame) || !Tick(Row.DisplayFrame, C, Row.Time))
        { return Fail(R, TEXT("sequencer_frame_out_of_range"), TEXT("frame must be finite and its original display-to-tick floor conversion must fit signed int32.")); }
        const TCHAR* Groups[] = { TEXT("location"), TEXT("rotation"), TEXT("scale") };
        for (int32 G = 0; G < 3; ++G)
        {
            auto Group = J->TryGetField(Groups[G]); if (!Group.IsValid()) { continue; }
            if (Group->Type != EJson::Array || Group->AsArray().Num() != 3)
            { return Fail(R, TEXT("sequencer_vector_shape_invalid"), TEXT("Present location/rotation/scale groups must contain exactly three finite JSON numbers. Omitted groups remain unchanged.")); }
            for (int32 Axis = 0; Axis < 3; ++Axis)
            {
                double N = 0; const auto V = Group->AsArray()[Axis];
                if (!V.IsValid() || V->Type != EJson::Number || !V->TryGetNumber(N) || !FMath::IsFinite(N))
                { return Fail(R, TEXT("sequencer_key_value_invalid"), TEXT("Present axis values must be finite JSON numbers.")); }
                Row.Values[G * 3 + Axis] = N;
            }
        }
        Rows.Add(Row);
    }
    return true;
}
bool KeyStructurePreserved(FContext& C, const TSharedRef<FJsonObject>& After, bool bNewTrack, bool bNewSection)
{
    if (!CommonPreserved(C, After, false)) { return false; }
    const auto& OldBindings = C.Before->GetArrayField(TEXT("bindings")); const auto& NewBindings = After->GetArrayField(TEXT("bindings"));
    if (OldBindings.Num() != NewBindings.Num()) { return false; }
    for (int32 I = 0; I < OldBindings.Num(); ++I)
    {
        auto Old = OldBindings[I]->AsObject().ToSharedRef(); auto New = NewBindings[I]->AsObject().ToSharedRef();
        if (!SameField(Old, New, TEXT("binding_id")) || !SameField(Old, New, TEXT("name"))) { return false; }
        auto Expected = Old->GetArrayField(TEXT("tracks"));
        if (bNewTrack && Old->GetStringField(TEXT("binding_id")) == C.Binding.ToString(EGuidFormats::DigitsWithHyphens)) { Expected.Add(MakeShared<FJsonValueString>(C.Track->GetPathName())); }
        auto E = MakeShared<FJsonObject>(); E->SetArrayField(TEXT("tracks"), Expected);
        if (!SameField(E, New, TEXT("tracks"))) { return false; }
    }
    const auto OldTracks = C.Before->GetObjectField(TEXT("tracks")); const auto NewTracks = After->GetObjectField(TEXT("tracks"));
    if (NewTracks->Values.Num() != OldTracks->Values.Num() + (bNewTrack ? 1 : 0)) { return false; }
    for (const auto& P : OldTracks->Values)
    {
        auto Current = NewTracks->TryGetField(P.Key.ToView()); if (!Current.IsValid() || Current->Type != EJson::Object) { return false; }
        auto Old = P.Value->AsObject().ToSharedRef(); auto New = Current->AsObject().ToSharedRef();
        if (FString(*P.Key) != C.Track->GetPathName()) { if (!Same(Old, New)) { return false; } continue; }
        for (const TCHAR* K : { TEXT("path"), TEXT("class"), TEXT("outer"), TEXT("object_flags"), TEXT("fields") })
        { if (!SameField(Old, New, K)) { return false; } }
        const auto& OS = Old->GetArrayField(TEXT("sections")); const auto& NS = New->GetArrayField(TEXT("sections"));
        if (NS.Num() != OS.Num() + (bNewSection ? 1 : 0)) { return false; }
        for (int32 I = 0; I < OS.Num(); ++I)
        {
            auto A = OS[I]->AsObject().ToSharedRef(); auto B = NS[I]->AsObject().ToSharedRef();
            if (A->GetStringField(TEXT("path")) != C.Section->GetPathName()) { if (!Same(A, B)) { return false; } }
            else
            {
                for (const TCHAR* K : { TEXT("path"), TEXT("class"), TEXT("outer"), TEXT("object_flags"), TEXT("fields"), TEXT("range"), TEXT("weight_channel") })
                { if (C.bFloat && FString(K) == TEXT("weight_channel")) { continue; } if (!SameField(A, B, K)) { return false; } }
            }
        }
    }
    return true;
}
}

namespace
{
// New maintenance leaves own their original save gate. Existing AX/BQ operations
// above and below continue to use their original no-save paths unchanged.
void InitMaintenance(FResult& R, const TCHAR* Operation, bool bSave)
{
    Init(R, Operation); R.bMaintenance = true; R.bSaveRequested = bSave;
    for (const TCHAR* Key : { TEXT("outer_transaction_calls"), TEXT("explicit_modify_calls"), TEXT("set_playback_range_calls"), TEXT("remove_possessable_calls"), TEXT("unbind_possessable_calls"), TEXT("mark_package_dirty_calls"), TEXT("save_attempt_count"), TEXT("expected_reference_copy_removal_calls") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("save_calls"), TArray<TSharedPtr<FJsonValue>>()); R.Data->SetBoolField(TEXT("legacy_mutation_contract_available"), false);
    R.Data->SetBoolField(TEXT("save_requested"), bSave); R.Data->SetBoolField(TEXT("saved"), false);
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Exact current native LevelSequence, with conditional primary LoadAsset and pre-baseline PostLoad/cache effects separately receipted. Bounded declarative readback, one native outer Editor transaction, public Modify/signature/event/track callbacks and optional forced EditorAssetLibrary save. Save can check out source-control files and run Engine serialization callbacks. No player/world evaluation, compile, explicit GC, rollback or retry; implicit Engine work is not exhaustively measured."));
    R.Data->SetStringField(TEXT("changed_observation_scope"), TEXT("Typed changed describes the selected actual display range/binding membership delta; legacy requested-value changed and performed are retained independently in legacy_mutation_contract. Full raw snapshots retain dirty/signature/flag changes."));
}
bool MaintenanceReady(FContext& C, FResult& R)
{
    C.bFloat = true; // Reuse the existing fully prepublished bounded snapshot branch; no selected channel is omitted.
    if (!Native(UMovieSceneFloatTrack::StaticClass(), TEXT("/Script/MovieSceneTracks.MovieSceneFloatTrack")) || !Native(UMovieSceneFloatSection::StaticClass(), TEXT("/Script/MovieSceneTracks.MovieSceneFloatSection")))
    { return Fail(R, TEXT("sequencer_maintenance_native_source_required"), TEXT("Exact loaded native Float source classes are required for the common bounded observer.")); }
    const auto Current = C.Scene->GetPlaybackRange();
    if (Current.GetLowerBound().IsOpen() || Current.GetUpperBound().IsOpen())
    { return Fail(R, TEXT("sequencer_maintenance_closed_range_required"), TEXT("The original Info read uses closed playback bounds; open-range assets are refused before mutation.")); }
    if (R.bSaveRequested && !C.Sequence->GetOutermost()->IsFullyLoaded())
    { return Fail(R, TEXT("sequencer_maintenance_save_package_not_fully_loaded"), TEXT("Requested save requires an already fully loaded package; hidden save-time package loading is not admitted.")); }
    return true;
}
bool MaintenanceDisk(ULevelSequence* Sequence, TSharedRef<FJsonObject>& J)
{
    FString Filename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(Sequence->GetOutermost()->GetName(), Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    J->SetStringField(TEXT("path"), Filename); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(Size, 0)); J->SetStringField(TEXT("sha1"), TEXT(""));
    if (Size < 0) { return true; }
    TArray<uint8> Bytes; if (Size > 64 * 1024 * 1024 || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; }
    uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool MaintenanceSnapshotBody(FContext& C, TSharedRef<FJsonObject>& J)
{
    const TFunction<bool()> Current=(C.bNestedSubsequence||C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval||C.bSubsequenceClips)?TFunction<bool()>([&C]{return CameraMaintenancePlainCheck(C.CameraMaintenance,TEXT("nested_retained_export_return"));}):TFunction<bool()>();
    J->SetBoolField(TEXT("complete"), false);
    if (!Snapshot(C, J)) { return false; } J->SetBoolField(TEXT("complete"), false);
    auto Disk = MakeShared<FJsonObject>(); auto Dirty = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("disk"), Disk); J->SetObjectField(TEXT("other_package_dirty"), Dirty);
    if (!MaintenanceDisk(C.Sequence, Disk)) { return false; }
    int32 Count = 0;
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        if (*It == C.Sequence->GetOutermost() || !Loaded(*It)) { continue; }
        if (++Count > 8192) { return false; } Dirty->SetBoolField(It->GetPathName(), It->IsDirty());
    }
    auto Retained = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("retained_selected_track_objects"), Retained);
    int32 SectionCount = 0;
    for (const auto& Keep : C.MaintenanceTracks)
    {
        UMovieSceneTrack* Track = Keep.Get(); if (!Loaded(Track) || Track->GetOuter() != C.Scene) { return false; }
        auto Row = Identity(Track); Retained->SetObjectField(Track->GetPathName(), Row); auto Fields = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("fields"), Fields);
        if (!ObjectProperties(Track, {}, Fields,Current)) { return false; } TArray<TSharedPtr<FJsonValue>> Sections;
        const auto RetainedInventory=Track->GetAllSections();
        for (UMovieSceneSection* Section : RetainedInventory)
        {
            if (!Loaded(Section) || Section->GetOuter() != Track || ++SectionCount > MaxSections) { return false; }
            auto SectionRow = Identity(Section); auto PropertiesRow = MakeShared<FJsonObject>(); Sections.Add(MakeShared<FJsonValueObject>(SectionRow)); Row->SetArrayField(TEXT("sections"), Sections); SectionRow->SetObjectField(TEXT("fields"), PropertiesRow);
            if (!ObjectProperties(Section, {}, PropertiesRow,Current)) { return false; } SectionRow->SetObjectField(TEXT("range"), Range(Section->GetRange()));
        }
        Row->SetArrayField(TEXT("sections"), Sections);
    }
    if(C.bMixedRemoval){auto Mixed=MakeShared<FJsonObject>();J->SetObjectField(TEXT("nonselected_mixed_objects"),Mixed);if(!CameraMaintenanceMixedSnapshot(C.CameraMaintenance,Mixed))return false;}
    J->SetBoolField(TEXT("complete"), true); return true;
}
bool MaintenanceSnapshot(FContext& C,TSharedRef<FJsonObject>& J)
{
    const bool Captured=MaintenanceSnapshotBody(C,J);
    if(!Captured&&(C.bNestedSubsequence||C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval||C.bSubsequenceClips))CameraMaintenanceAbort(C.CameraMaintenance,TEXT("nested_source_observation_incomplete"));
    return Captured;
}
bool MaintenanceCapture(FContext& C, FResult& R, const TCHAR* Key, TSharedRef<FJsonObject>& Out)
{
    R.Data->SetObjectField(Key, Out); R.Data->SetBoolField(TEXT("observations_complete"), false);
    if(!DirectorCurrent(C,R,Key,R.bSaved))return false;
    if(!CameraInsertionCheck(C.CameraInsertion,Key)||!CameraMaintenanceCheck(C.CameraMaintenance,Key))return false;
    const bool Captured=MaintenanceSnapshot(C, Out);
    if(!DirectorCurrent(C,R,Key,R.bSaved))return false;
    if(!CameraInsertionCheck(C.CameraInsertion,Key)||!CameraMaintenanceCheck(C.CameraMaintenance,Key))return false;
    if (!Captured)
    {
        CameraMaintenanceAbort(C.CameraMaintenance,Key);
        R.bKnown = C.CameraMaintenance ? false : !R.bAttempted;
        return Fail(R, R.bAttempted ? TEXT("sequencer_maintenance_post_state_unobservable") : TEXT("sequencer_maintenance_before_unobservable"), TEXT("Bounded actual snapshot is incomplete; partial maps and dispatched counters are retained. No retry or additional write is dispatched."));
    }
    if(!CameraMaintenanceObserve(C.CameraMaintenance,C,Out,Key))return false;
    R.Data->SetBoolField(TEXT("observations_complete"), true); return true;
}
bool MaintenanceTimeSafe(FFrameTime Time, FFrameRate SourceRate, FFrameRate DestinationRate)
{
    if (SourceRate.Numerator <= 0 || SourceRate.Denominator <= 0 || DestinationRate.Numerator <= 0 || DestinationRate.Denominator <= 0) { return false; }
    const double Decimal = Time.AsDecimal();
    if (!FMath::IsFinite(Decimal)) { return false; }
    // ConvertFrameTime returns early only for exact Numerator/Denominator equality.
    if (SourceRate == DestinationRate) { return Decimal >= static_cast<double>(MIN_int32) && Decimal <= static_cast<double>(MAX_int32); }
    // Use the exact unreduced int64 products in FrameRate.h, not a mathematically
    // equivalent reduced ratio that could hide overflow in the Engine algorithm.
    const int64 Numerator = static_cast<int64>(DestinationRate.Numerator) * SourceRate.Denominator;
    const int64 Denominator = static_cast<int64>(DestinationRate.Denominator) * SourceRate.Numerator;
    const int64 Frame = Time.GetFrame().Value, Magnitude = Frame < 0 ? -Frame : Frame;
    if (Magnitude && Numerator > MAX_int64 / Magnitude) { return false; }
    const double Scaled = Decimal * static_cast<double>(Numerator) / static_cast<double>(Denominator);
    return FMath::IsFinite(Scaled) && Scaled >= static_cast<double>(MIN_int32) && Scaled <= static_cast<double>(MAX_int32);
}
bool MaintenanceRequestedTimeSafe(double DisplayFrame, const FContext& C)
{
    return FMath::IsFinite(DisplayFrame) && DisplayFrame >= static_cast<double>(MIN_int32) && DisplayFrame <= static_cast<double>(MAX_int32)
        && MaintenanceTimeSafe(FFrameTime::FromDecimal(DisplayFrame), C.Scene->GetDisplayRate(), C.Scene->GetTickResolution());
}
bool MaintenanceDisplayFrame(FFrameNumber TickFrame, const FContext& C, double& Out)
{
    const FFrameTime Source(TickFrame); const FFrameRate Resolution = C.Scene->GetTickResolution(), Display = C.Scene->GetDisplayRate();
    if (!MaintenanceTimeSafe(Source, Resolution, Display)) { return false; }
    Out = FFrameRate::TransformTime(Source, Resolution, Display).AsDecimal();
    return FMath::IsFinite(Out) && Out >= static_cast<double>(MIN_int32) && Out <= static_cast<double>(MAX_int32);
}
bool MaintenanceInfo(FContext& C, TSharedRef<FJsonObject>& Out)
{
    const auto Playback = C.Scene->GetPlaybackRange();
    const TArray<FMovieSceneBinding> LeafBindings=(C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval)?static_cast<const UMovieScene*>(C.Scene)->GetBindings():TArray<FMovieSceneBinding>();
    const auto& Bindings=(C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval)?LeafBindings:static_cast<const UMovieScene*>(C.Scene)->GetBindings();
    if (Playback.GetLowerBound().IsOpen() || Playback.GetUpperBound().IsOpen() || Bindings.Num() > MaxBindings) { return false; }
    double Start = 0, End = 0;
    if (!MaintenanceDisplayFrame(Playback.GetLowerBoundValue(), C, Start) || !MaintenanceDisplayFrame(Playback.GetUpperBoundValue(), C, End)) { return false; }
    Out->SetBoolField(TEXT("success"), true); Out->SetStringField(TEXT("sequence"), C.Sequence->GetPathName());
    if(C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval)
    {
        const FFrameRate Display=C.Scene->GetDisplayRate(),Resolution=C.Scene->GetTickResolution();
        const FText DisplayText=Display.ToPrettyText();if(!CameraMaintenancePlainCheck(C.CameraMaintenance,TEXT("component_remove_display_text")))return false;
        const FString DisplayString=DisplayText.ToString();if(!CameraMaintenancePlainCheck(C.CameraMaintenance,TEXT("component_remove_display_string")))return false;
        const FText ResolutionText=Resolution.ToPrettyText();if(!CameraMaintenancePlainCheck(C.CameraMaintenance,TEXT("component_remove_resolution_text")))return false;
        const FString ResolutionString=ResolutionText.ToString();if(!CameraMaintenancePlainCheck(C.CameraMaintenance,TEXT("component_remove_resolution_string")))return false;
        Out->SetStringField(TEXT("display_rate"),DisplayString);Out->SetStringField(TEXT("tick_resolution"),ResolutionString);
    }
    else
    {
    Out->SetStringField(TEXT("display_rate"), C.Scene->GetDisplayRate().ToPrettyText().ToString()); Out->SetStringField(TEXT("tick_resolution"), C.Scene->GetTickResolution().ToPrettyText().ToString());
    }
    Out->SetNumberField(TEXT("playback_start_frame"), Start); Out->SetNumberField(TEXT("playback_end_frame"), End); Out->SetNumberField(TEXT("binding_count"), Bindings.Num());
    Out->SetBoolField(TEXT("has_camera_cut_track"), C.Scene->GetCameraCutTrack() != nullptr); TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FMovieSceneBinding& Binding : Bindings)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("binding_id"), Binding.GetObjectGuid().ToString(EGuidFormats::DigitsWithHyphens)); Row->SetStringField(TEXT("name"), BindingName(C.Scene, Binding.GetObjectGuid())); TArray<TSharedPtr<FJsonValue>> Tracks;
        for (UMovieSceneTrack* Track : Binding.GetTracks()) { if (!Loaded(Track)) { return false; } Tracks.Add(MakeShared<FJsonValueString>(Track->GetClass()->GetName())); }
        Row->SetArrayField(TEXT("tracks"), Tracks); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("bindings"), Rows); return true;
}
bool MaintenanceSaveEqual(const TSharedPtr<FJsonValue>& BeforeValue, const TSharedPtr<FJsonValue>& AfterValue, const TSet<FString>& Owned, int32& Transitions, const FString& Key = FString())
{
    if (!BeforeValue.IsValid() || !AfterValue.IsValid() || BeforeValue->Type != AfterValue->Type) { return false; }
    if (BeforeValue->Type == EJson::Object)
    {
        const auto Left = BeforeValue->AsObject(), Right = AfterValue->AsObject(); if (Left->Values.Num() != Right->Values.Num()) { return false; }
        for (const auto& Pair : Left->Values)
        {
            const FString Name(Pair.Key.ToView()); const auto Actual = Right->TryGetField(Pair.Key.ToView());
            if (Name == TEXT("object_flags") && Left->HasField(TEXT("path")) && Owned.Contains(Left->GetStringField(TEXT("path"))))
            {
                if (!Actual.IsValid() || Pair.Value->Type != EJson::String || Actual->Type != EJson::String || !SameField(Left.ToSharedRef(), Right.ToSharedRef(), TEXT("path")) || !SameField(Left.ToSharedRef(), Right.ToSharedRef(), TEXT("class")) || !SameField(Left.ToSharedRef(), Right.ToSharedRef(), TEXT("outer"))) { return false; }
                const FString A = Pair.Value->AsString(), B = Actual->AsString(); const uint64 Old = FCString::Strtoui64(*A, nullptr, 10), Now = FCString::Strtoui64(*B, nullptr, 10);
                if (A != FString::Printf(TEXT("%llu"), Old) || B != FString::Printf(TEXT("%llu"), Now) || Now != (Old | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; }
                Transitions += Old != Now ? 1 : 0; continue;
            }
            if (!MaintenanceSaveEqual(Pair.Value, Actual, Owned, Transitions, Key + TEXT("/") + Name)) { return false; }
        }
        return true;
    }
    if (BeforeValue->Type == EJson::Array)
    {
        const auto& A = BeforeValue->AsArray(); const auto& B = AfterValue->AsArray(); if (A.Num() != B.Num()) { return false; }
        for (int32 Index = 0; Index < A.Num(); ++Index) { if (!MaintenanceSaveEqual(A[Index], B[Index], Owned, Transitions, Key + TEXT("/") + FString::FromInt(Index))) { return false; } } return true;
    }
    if (BeforeValue->Type == EJson::Boolean && Key == TEXT("/dirty")) { return !AfterValue->AsBool(); }
    if (BeforeValue->Type == EJson::String) { return BeforeValue->AsString().Equals(AfterValue->AsString(),ESearchCase::CaseSensitive); }
    if (BeforeValue->Type == EJson::Number) { return BeforeValue->AsNumber() == AfterValue->AsNumber(); }
    if (BeforeValue->Type == EJson::Boolean) { return BeforeValue->AsBool() == AfterValue->AsBool(); } return BeforeValue->Type == EJson::Null;
}
bool MaintenanceSave(FContext& C, FResult& R, const TSharedRef<FJsonObject>& Expected, TSharedRef<FJsonObject>& Final)
{
    Final = Expected;
    if (!R.bSaveRequested) { return true; }
    if(!CameraInsertionCheck(C.CameraInsertion,TEXT("camera_insertion_before_save"))||!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_maintenance_before_save")))return false;
    R.Stage = TEXT("save"); R.bSaveAttempted = true; R.Data->SetNumberField(TEXT("save_attempt_count"), 1);
    R.Data->SetBoolField(TEXT("observations_complete"), false); R.Data->SetBoolField(TEXT("save_state_preserved"), false);
    R.Data->SetBoolField(TEXT("after_observation_nonselected_state_preserved"), R.Data->GetBoolField(TEXT("nonselected_state_preserved"))); R.Data->SetBoolField(TEXT("nonselected_state_preserved"), false);
    auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("asset_path"), C.Sequence->GetPathName()); Receipt->SetBoolField(TEXT("attempted"), true); Receipt->SetBoolField(TEXT("only_if_dirty"), false); Receipt->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::SaveLoadedAsset"));
    R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Receipt) });
    const bool Returned = UEditorAssetLibrary::SaveLoadedAsset(C.Sequence, false); Receipt->SetBoolField(TEXT("saved"), Returned); R.bSaved = Returned; R.Data->SetBoolField(TEXT("saved"), Returned);
    if((C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval||C.bRemovalClips)&&!Returned)
    {
        CameraMaintenanceAbort(C.CameraMaintenance,TEXT("component_removal_save_returned_false"));
        R.Data->SetBoolField(TEXT("save_returned_success"),false);
        return Fail(R,TEXT("mutation_postcondition_failed"),TEXT("Forced save returned false after actual removal; disk completion is unknown. No further observer, save retry or rollback."));
    }
    if(!CameraInsertionCheck(C.CameraInsertion,TEXT("camera_insertion_after_save"),Returned)||!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_maintenance_after_save"),Returned))return false;
    Final = MakeShared<FJsonObject>(); if (!MaintenanceCapture(C, R, TEXT("after_save"), Final)) { return false; }
    R.Data->SetBoolField(TEXT("save_returned_success"), Returned);
    if (!Returned) { R.bKnown = false; return Fail(R, TEXT("mutation_postcondition_failed"), TEXT("The original forced save returned false; complete RAM readback is retained but disk completion is not certified. No save retry.")); }
    TSet<FString> Owned = { C.Sequence->GetPathName(), C.Scene->GetPathName() };
    for (const auto& Pair : Expected->GetObjectField(TEXT("tracks"))->Values)
    {
        const auto Track = Pair.Value->AsObject(); Owned.Add(Track->GetStringField(TEXT("path")));
        for (const auto& Value : Track->GetArrayField(TEXT("sections"))) { Owned.Add(Value->AsObject()->GetStringField(TEXT("path"))); }
    }
    if(C.bMixedRemoval)for(const auto& Pair:Expected->GetObjectField(TEXT("nonselected_mixed_objects"))->Values)Owned.Add(Pair.Value->AsObject()->GetStringField(TEXT("path")));
    auto A = MakeShared<FJsonObject>(), B = MakeShared<FJsonObject>(); A->Values = Expected->Values; B->Values = Final->Values; A->RemoveField(TEXT("disk")); B->RemoveField(TEXT("disk"));
    int32 FlagTransitions = 0; const bool StateOK = MaintenanceSaveEqual(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B), Owned, FlagTransitions);
    const auto Disk = Final->GetObjectField(TEXT("disk")); const bool Persisted = Disk->GetBoolField(TEXT("exists")) && Disk->GetNumberField(TEXT("bytes")) > 0 && !C.Sequence->GetOutermost()->IsDirty();
    R.Data->SetBoolField(TEXT("save_state_preserved"), StateOK); R.Data->SetBoolField(TEXT("save_disk_verified"), Persisted); R.Data->SetNumberField(TEXT("normal_save_object_flag_transitions"), FlagTransitions);
    R.Data->SetBoolField(TEXT("nonselected_state_preserved"), StateOK);
    if (!StateOK || !Persisted) { return Fail(R, TEXT("sequencer_maintenance_save_postcondition_failed"), TEXT("Actual successful save is retained, but owned flags/state/source/other-dirty or persisted disk readback differs. No rollback or retry.")); }
    return true;
}
void MaintenanceLegacy(FResult& R, const TSharedRef<FJsonObject>& BeforeInfo, const TSharedRef<FJsonObject>& AfterInfo, const TSharedRef<FJsonObject>& Delta, bool bLegacyChanged, bool bSpecific, const TCHAR* Method, const TArray<FString>& Checks)
{
    auto Contract = MakeShared<FJsonObject>(); Contract->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1")); Contract->SetStringField(TEXT("mode"), TEXT("apply")); Contract->SetBoolField(TEXT("dry_run"), false); Contract->SetBoolField(TEXT("DRY_RUN"), false);
    Contract->SetStringField(TEXT("sequence"), R.Data->GetStringField(TEXT("sequence"))); Contract->SetBoolField(TEXT("save_requested"), R.bSaveRequested); Contract->SetBoolField(TEXT("saved"), R.bSaved);
    for (const TCHAR* Field : { TEXT("playback_start_frame"), TEXT("playback_end_frame"), TEXT("binding_id"), TEXT("removed"), TEXT("removal_observed") }) { if (R.Data->HasField(Field)) { Contract->SetField(Field, R.Data->TryGetField(Field)); } }
    Contract->SetBoolField(TEXT("changed"), bLegacyChanged); Contract->SetBoolField(TEXT("mutation_performed"), true); auto Mutation = MakeShared<FJsonObject>(); Mutation->SetBoolField(TEXT("attempted"), true); Mutation->SetBoolField(TEXT("performed"), true); Mutation->SetStringField(TEXT("state"), TEXT("performed")); Contract->SetObjectField(TEXT("mutation"), Mutation);
    Contract->SetObjectField(TEXT("before"), BeforeInfo); Contract->SetObjectField(TEXT("after"), AfterInfo); Contract->SetObjectField(TEXT("delta"), Delta); Contract->SetBoolField(TEXT("postcondition_verified"), bSpecific); Contract->SetBoolField(TEXT("verified_success"), bSpecific);
    auto Verification = MakeShared<FJsonObject>(); Verification->SetStringField(TEXT("status"), bSpecific ? TEXT("passed") : TEXT("failed")); Verification->SetStringField(TEXT("method"), Method); Verification->SetBoolField(TEXT("before_observed"), true); Verification->SetBoolField(TEXT("after_observed"), true); Verification->SetBoolField(TEXT("strict"), true); TArray<TSharedPtr<FJsonValue>> Names; for (const FString& Check : Checks) { Names.Add(MakeShared<FJsonValueString>(Check)); } Verification->SetArrayField(TEXT("checks"), Names); Contract->SetObjectField(TEXT("verification"), Verification);
    Contract->SetBoolField(TEXT("success"), bSpecific); Contract->SetStringField(TEXT("success_semantics"), bSpecific ? TEXT("verified_postcondition") : TEXT("postcondition_failed"));
    if (!bSpecific) { Contract->SetBoolField(TEXT("legacy_success"), true); Contract->SetStringField(TEXT("error_code"), TEXT("mutation_postcondition_failed")); Contract->SetStringField(TEXT("error"), TEXT("Mutation postcondition failed independent readback")); }
    R.Data->SetObjectField(TEXT("legacy_mutation_contract"), Contract); R.Data->SetBoolField(TEXT("legacy_mutation_contract_available"), true); R.Data->SetBoolField(TEXT("legacy_mutation_performed"), true); R.Data->SetBoolField(TEXT("legacy_changed"), bLegacyChanged);
    for (const TCHAR* Field : { TEXT("contract_version"), TEXT("mode"), TEXT("dry_run"), TEXT("DRY_RUN"), TEXT("mutation"), TEXT("before"), TEXT("after"), TEXT("delta"), TEXT("postcondition_verified"), TEXT("verified_success"), TEXT("verification"), TEXT("success_semantics") }) { R.Data->SetField(Field, Contract->TryGetField(Field)); }
    R.Data->SetStringField(TEXT("verification_effect_scope"), TEXT("Current loaded native MovieScene Info projection and existing GUID/track readback; original HandleInfo extra AssetLibrary lookup was not replayed. No player/world evaluation or cold/runtime proof."));
}
TSharedRef<FJsonObject> MaintenanceDelta(int32 Affected, int32 Updated, int32 Removed, int32 Skipped, int32 Failed)
{
    auto D = MakeShared<FJsonObject>(); D->SetNumberField(TEXT("affected_count"), Affected); D->SetNumberField(TEXT("created_count"), 0); D->SetNumberField(TEXT("updated_count"), Updated); D->SetNumberField(TEXT("removed_count"), Removed); D->SetNumberField(TEXT("skipped_count"), Skipped); D->SetNumberField(TEXT("failed_count"), Failed); return D;
}
bool MaintenanceExportExpected(FProperty* Property, const void* Address, UObject* Owner, TSharedRef<FJsonObject>& Row, const TFunction<bool()>& Current = TFunction<bool()>())
{
    if (Current && !Current()) { return false; }
    if (!Property || Property->ArrayDim != 1 || !Address) { return false; } FString Value;
    const bool Exported = Property->ExportText_Direct(Value, Address, Address, Owner, PPF_None);
    if (Current && !Current()) { return false; }
    if (!Exported || Value.Len() > MaxText) { return false; }
    Row->SetStringField(TEXT("owner"), Property->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("property_class"), Property->GetClass()->GetName());
    Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(Property->GetPropertyFlags()))); Row->SetNumberField(TEXT("array_dim"), 1); Row->SetArrayField(TEXT("element_values_text"), { MakeShared<FJsonValueString>(Value) }); return true;
}
bool MaintenanceResolveBinding(FContext& C, const FString& Id, const FString& Label, FResult& R)
{
    bool HasNul = false; for (int32 Index = 0; Index < Id.Len(); ++Index) { HasNul |= Id[Index] == TCHAR('\0'); } for (int32 Index = 0; Index < Label.Len(); ++Index) { HasNul |= Label[Index] == TCHAR('\0'); }
    if (Id.Len() > 128 || Label.Len() > 512 || HasNul || (!Id.IsEmpty() && (!FGuid::Parse(Id, C.Binding) || !C.Binding.IsValid())))
    { return Fail(R, TEXT("sequencer_binding_selector_invalid"), TEXT("A bounded valid nonzero GUID or exact literal binding label is required; raw whitespace/case is not trimmed.")); }
    const auto& Bindings = static_cast<const UMovieScene*>(C.Scene)->GetBindings();
    if (Bindings.Num() > MaxBindings) { return Fail(R, TEXT("sequencer_binding_budget_exceeded"), TEXT("At most 128 current bindings are admitted.")); }
    for (const auto& Binding : Bindings)
    {
        if ((!Id.IsEmpty() && Binding.GetObjectGuid() == C.Binding) || (Id.IsEmpty() && !Label.IsEmpty() && BindingName(C.Scene, Binding.GetObjectGuid()) == Label))
        { C.Binding = Binding.GetObjectGuid(); R.Data->SetStringField(TEXT("binding_id"), C.Binding.ToString(EGuidFormats::DigitsWithHyphens)); return true; }
    }
    return Fail(R, TEXT("sequencer_binding_not_found"), TEXT("GUID has precedence with no label fallback; missing current membership is refused before Modify rather than the original attempted RemovePossessable failure."));
}
// QJ: only declaration/reference membership is retired. Possessed world objects are never resolved.
bool MaintenanceLeafInput(FContext& C,FResult& R)
{
    const FMovieScenePossessable* Selected=C.Scene->FindPossessable(C.Binding);
    if(!Selected||!Selected->GetParent().IsValid())return true;
    const int32 Count=C.Scene->GetPossessableCount();
    if(Count<2||Count>MaxBindings)return Fail(R,TEXT("sequencer_component_leaf_shape_unsupported"),TEXT("Bounded selected component and existing parent possessable required."));
    if(C.Scene->GetSpawnableCount()!=0||C.Scene->GetRootFolders().Num()!=0||C.Scene->AllTaggedBindings().Num()!=0||!DirectorAllowed(C))
        return Fail(R,TEXT("sequencer_component_leaf_shape_unsupported"),TEXT("Spawnable/folder/tagged/Director variants remain incomplete; no lease or writer entered."));
    const auto* Refs=C.Sequence->GetBindingReferences();if(!Refs||Refs->GetAllReferences().Num()>MaxBindings*16)return Fail(R,TEXT("sequencer_component_leaf_references_unsupported"),TEXT("Bounded ordinary locator references required."));
    for(const auto& Ref:Refs->GetAllReferences())if(!Ref.CustomBinding.IsResolved()||Ref.CustomBinding.Get()!=nullptr)return Fail(R,TEXT("sequencer_component_leaf_custom_binding_unsupported"),TEXT("Custom bindings remain incomplete."));
    for(const auto& Ref:Refs->GetAllReferences())
    {
        const bool Stock=Ref.Locator.ForEachFragment([](int32 Index,int32 Count,const FUniversalObjectLocatorFragment& Fragment)
        {
            return Index<4&&Count<=4&&(Fragment.GetFragmentTypeHandle()==FActorLocatorFragment::FragmentType||Fragment.GetFragmentTypeHandle()==FSubObjectLocator::FragmentType);
        });
        if(!Stock)return Fail(R,TEXT("sequencer_component_leaf_locator_unsupported"),TEXT("At most four stock Actor/SubObject locator payloads are admitted without object resolution; other fragments remain incomplete."));
    }
    const FGuid ParentId=Selected->GetParent();const auto* Actor=C.Scene->FindPossessable(ParentId);
    const UClass* ComponentClass=Selected->GetLoadedPossessedObjectClass();const UClass* ActorClass=Actor?Actor->GetLoadedPossessedObjectClass():nullptr;
    if(!Actor||Actor->GetParent().IsValid()||!Loaded(ComponentClass)||!Loaded(ActorClass)||!ComponentClass->HasAnyClassFlags(CLASS_Native)||!ActorClass->HasAnyClassFlags(CLASS_Native)
        ||!ComponentClass->IsChildOf(USceneComponent::StaticClass())||!ActorClass->IsChildOf(AActor::StaticClass()))
        return Fail(R,TEXT("sequencer_component_leaf_class_unsupported"),TEXT("Already-loaded native SceneComponent declaration and parentless native Actor declaration required; no possessed-object evaluation or loader."));
    for(int32 I=0;I<Count;++I){const auto& P=C.Scene->GetPossessable(I);
        const auto& Dynamic=P.DynamicBinding_DEPRECATED;
        if(!Dynamic.Function.IsResolved()||Dynamic.Function.Get()!=nullptr||!Dynamic.ResolveParamsProperty.IsPathToFieldEmpty()||Dynamic.PayloadVariables.Num()!=0||!Dynamic.CompiledFunctionName.IsNone()||!Dynamic.ResolveParamsPinName.IsNone()||!Dynamic.WeakEndpoint.IsExplicitlyNull())
            return Fail(R,TEXT("sequencer_component_leaf_dynamic_binding_unsupported"),TEXT("Legacy dynamic endpoint/payload variants remain incomplete; no resolving field path copy is admitted."));
        if(P.GetParent()==C.Binding||(P.GetParent().IsValid()&&P.GetGuid()!=C.Binding))
        return Fail(R,TEXT("sequencer_component_leaf_hierarchy_unsupported"),TEXT("Only selected leaf Component -> parentless Actor hierarchy is admitted; other/deeper hierarchy remains incomplete."));}
    C.bLeafRemoval=true;R.Data->SetBoolField(TEXT("leaf_component_removal_contract"),true);R.Data->SetStringField(TEXT("parent_binding_id"),ParentId.ToString(EGuidFormats::DigitsWithHyphens));
    R.Data->SetStringField(TEXT("component_leaf_scope"),TEXT("Leaf possessable declaration, not SceneComponent attachment leaf. Parent/other GUIDs, tracks and locator payloads preserved without world-object resolution. Sprite/Arrow attachments are neither inspected nor deleted; no immediate UObject destruction/GC claim."));
    return true;
}

bool MaintenanceMixedInput(FContext& C,FResult& R)
{
    const auto* Selected=C.Scene->FindPossessable(C.Binding);if(!Selected||Selected->GetParent().IsValid())return true;
    const auto* Refs=C.Sequence->GetBindingReferences();if(!Refs)return Fail(R,TEXT("sequencer_mixed_references_unavailable"),TEXT("Native binding references required."));
    bool Custom=false;for(const auto& Ref:Refs->GetAllReferences())
    {
        if(!Ref.CustomBinding.IsResolved())return Fail(R,TEXT("sequencer_mixed_custom_unresolved"),TEXT("Custom binding handle is unresolved; no implicit object load is added."));
        if(Ref.CustomBinding.Get()){if(Ref.ID==C.Binding)return Fail(R,TEXT("sequencer_selected_custom_binding_pending"),TEXT("Selected modern custom binding is distinct from a legacy spawnable; its removal remains valid incomplete."));Custom=true;}
    }
    C.bMixedRemoval=Custom||C.Scene->GetSpawnableCount()!=0||C.Scene->GetRootFolders().Num()!=0||C.Scene->AllTaggedBindings().Num()!=0;
    return true;
}
bool MaintenanceMixedSpawnTrack(const FContext& C,UMovieSceneTrack* Track)
{
    if(!C.bMixedRemoval||!Loaded(Track)||Track->GetClass()!=UMovieSceneSpawnTrack::StaticClass()||Track->GetOuter()!=C.Scene)return false;
    for(const auto& B:static_cast<const UMovieScene*>(C.Scene)->GetBindings())if(B.GetObjectGuid()!=C.Binding&&B.GetTracks().Contains(Track))return true;return false;
}
// UM: one Actor declaration removal, not recursive deletion or reparenting.
// The initial hierarchy is closed and acyclic. Remaining value-copies keep each
// original ParentGuid, including a child whose selected parent is then absent.
bool MaintenanceHierarchyInput(FContext& C,FResult& R)
{
    const auto* Selected=C.Scene->FindPossessable(C.Binding);
    if(!Selected||Selected->GetParent().IsValid())return true;
    const int32 Count=C.Scene->GetPossessableCount();
    if(Count<1||Count>MaxBindings)return Fail(R,TEXT("sequencer_hierarchy_removal_budget"),TEXT("Bounded possessable inventory required before hierarchy observation."));
    bool HasChild=false;for(int32 I=0;I<Count;++I)if(C.Scene->GetPossessable(I).GetParent()==C.Binding){HasChild=true;break;}
    if(!HasChild)return true; // Existing flat/mixed and selected component-leaf admission stays unchanged.
    TArray<FMovieScenePossessable> Initial;TMap<FGuid,int32> Indices;
    for(int32 I=0;I<Count;++I)
    {
        // TFieldPath copying can resolve. Admit the same absent legacy endpoint/path
        // role as the existing component-leaf route before making any value-copy.
        const auto& Existing=C.Scene->GetPossessable(I);const auto& Dynamic=Existing.DynamicBinding_DEPRECATED;
        if(!Dynamic.Function.IsResolved()||Dynamic.Function.Get()!=nullptr||!Dynamic.ResolveParamsProperty.IsPathToFieldEmpty()||Dynamic.PayloadVariables.Num()!=0||!Dynamic.CompiledFunctionName.IsNone()||!Dynamic.ResolveParamsPinName.IsNone()||!Dynamic.WeakEndpoint.IsExplicitlyNull())
            return Fail(R,TEXT("sequencer_hierarchy_removal_dynamic_pending"),TEXT("Absent legacy endpoint/field-path/payload required before copying declarations; wider dynamic binding variants remain valid incomplete."));
        const FMovieScenePossessable Value=Existing;const FGuid Id=Value.GetGuid();const UClass* Class=Value.GetLoadedPossessedObjectClass();
        if(!Id.IsValid()||Indices.Contains(Id)||!C.Scene->FindBinding(Id)||Value.GetSpawnableObjectBindingID().GetGuid().IsValid())
            return Fail(R,TEXT("sequencer_hierarchy_removal_declaration_pending"),TEXT("Current distinct ordinary possessable/binding GUIDs without spawnable-reference roles required; wider original declarations remain incomplete."));
        if(!Loaded(Class)||Class->GetClass()!=UClass::StaticClass()||!Class->HasAnyClassFlags(CLASS_Native)||Class->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)||Class->IsEditorOnly()
            ||!Loaded(Class->GetPackage())||!Class->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||Class->GetPackage()->IsDirty()
            ||(!Class->IsChildOf(AActor::StaticClass())&&!Class->IsChildOf(USceneComponent::StaticClass()))||(Id==C.Binding&&!Class->IsChildOf(AActor::StaticClass())))
            return Fail(R,TEXT("sequencer_hierarchy_removal_class_pending"),TEXT("Loaded native runtime Actor/SceneComponent declarations and a selected parentless Actor required; no class loader or possessed-object resolution."));
        Indices.Add(Id,I);Initial.Add(Value);
    }
    TArray<TSharedPtr<FJsonValue>> Descendants;int32 DirectChildren=0;
    for(const auto& Value:Initial)
    {
        FGuid Cursor=Value.GetParent();TSet<FGuid> Active;Active.Add(Value.GetGuid());bool UnderSelected=false;
        while(Cursor.IsValid())
        {
            const int32* ParentIndex=Indices.Find(Cursor);
            if(!ParentIndex||Active.Contains(Cursor)||Active.Num()>MaxBindings)
                return Fail(R,TEXT("sequencer_hierarchy_removal_parent_pending"),TEXT("Initial parents must form a closed finite acyclic possessable hierarchy. Existing orphan/cycle/spawnable-parent variants remain valid incomplete; no repair or parent rewrite."));
            Active.Add(Cursor);UnderSelected|=Cursor==C.Binding;Cursor=Initial[*ParentIndex].GetParent();
        }
        if(Value.GetParent()==C.Binding)++DirectChildren;
        if(UnderSelected){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("binding_id"),Value.GetGuid().ToString(EGuidFormats::DigitsWithHyphens));Row->SetStringField(TEXT("expected_parent_binding_id"),Value.GetParent().ToString(EGuidFormats::DigitsWithHyphens));Descendants.Add(MakeShared<FJsonValueObject>(Row));}
    }
    const auto* Refs=C.Sequence->GetBindingReferences();if(!Refs||Refs->GetAllReferences().Num()>MaxBindings*16)return Fail(R,TEXT("sequencer_hierarchy_removal_references_pending"),TEXT("Bounded native binding references required."));
    for(const auto& Ref:Refs->GetAllReferences())
    {
        const bool Stock=Ref.Locator.ForEachFragment([](int32 Index,int32 FragmentCount,const FUniversalObjectLocatorFragment& Fragment)
        {return Index<4&&FragmentCount<=4&&(Fragment.GetFragmentTypeHandle()==FActorLocatorFragment::FragmentType||Fragment.GetFragmentTypeHandle()==FSubObjectLocator::FragmentType);});
        if(!Stock)return Fail(R,TEXT("sequencer_hierarchy_removal_locator_pending"),TEXT("At most four native Actor/SubObject locator fragments observed without resolving objects; wider fragments remain incomplete."));
    }
    C.bHierarchyRemoval=true;
    R.Data->SetBoolField(TEXT("hierarchy_actor_removal_contract"),true);R.Data->SetNumberField(TEXT("hierarchy_direct_child_count"),DirectChildren);R.Data->SetNumberField(TEXT("hierarchy_descendant_count"),Descendants.Num());R.Data->SetArrayField(TEXT("hierarchy_expected_descendant_parents"),Descendants);
    R.Data->SetStringField(TEXT("hierarchy_removal_scope"),TEXT("Only selected Actor possessable/binding and its reference rows are removed. Nonselected declaration ParentGuid/name/class/tags, tracks/channels, locators and admitted collateral folders/tags stay literal. SDK leaves child ParentGuid pointing at the removed declaration; no reparenting, recursive deletion, evaluation, runtime fix, GC or rollback is synthesized."));
    return true;
}

bool MaintenanceRemoveAdmission(FContext& C, FResult& R, TSharedRef<FJsonObject>& ExpectedPossessables, TSharedRef<FJsonObject>& ExpectedReferences)
{
    const FMovieScenePossessable* Selected = C.Scene->FindPossessable(C.Binding);
    if (!Selected || (Selected->GetParent().IsValid() && !C.bLeafRemoval) || (!C.bMixedRemoval&&(C.Scene->GetSpawnableCount() != 0 || C.Scene->GetRootFolders().Num() != 0 || C.Scene->AllTaggedBindings().Num() != 0)))
    { return Fail(R, TEXT("sequencer_remove_binding_variant_unsupported"), TEXT("Selected ordinary parentless possessable or existing component-leaf scope required. Legacy spawnable/missing selectors remain refused before Modify, unlike the original attempted failure; selected modern custom removal is incomplete.")); }
    const int32 Count = C.Scene->GetPossessableCount(); const auto& Bindings = static_cast<const UMovieScene*>(C.Scene)->GetBindings();
    if (Count < 1 || Count > MaxBindings || Count+C.Scene->GetSpawnableCount() != Bindings.Num()) { return Fail(R, TEXT("sequencer_remove_binding_shape_unsupported"), TEXT("Requires one current binding per bounded possessable and no orphan binding entries.")); }
    TArray<FMovieScenePossessable> Remaining; TSet<FGuid> Guids;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const auto& Possessable = C.Scene->GetPossessable(Index); const FGuid Id = Possessable.GetGuid();
        if (!Id.IsValid() || Guids.Contains(Id) || (Possessable.GetParent().IsValid() && !C.bHierarchyRemoval && !(C.bLeafRemoval && Id == C.Binding)) || Possessable.GetSpawnableObjectBindingID().GetGuid().IsValid() || !C.Scene->FindBinding(Id)) { return Fail(R, TEXT("sequencer_remove_binding_hierarchy_unsupported"), TEXT("Hierarchical/dependent/spawnable-reference/orphan possessable variants remain incomplete; no mutation dispatched.")); }
        Guids.Add(Id); if (Id != C.Binding) { Remaining.Add(Possessable); }
    }
    if(C.bMixedRemoval)for(int32 I=0;I<C.Scene->GetSpawnableCount();++I){const auto& S=C.Scene->GetSpawnable(I);if(!S.GetGuid().IsValid()||Guids.Contains(S.GetGuid())||!S.GetChildPossessables().IsEmpty()||!C.Scene->FindBinding(S.GetGuid()))return Fail(R,TEXT("sequencer_mixed_legacy_spawnable_pending"),TEXT("Collateral legacy spawnables require distinct current IDs without child hierarchy; no conversion or deletion is invoked."));Guids.Add(S.GetGuid());}
    const auto* References = C.Sequence->GetBindingReferences();
    if (!References || References->GetAllReferences().Num() > MaxBindings * 16) { return Fail(R, TEXT("sequencer_remove_binding_references_unsupported"), TEXT("The bounded native locator reference map is required.")); }
    FGuid Previous; bool HasPrevious = false;
    for (const auto& Reference : References->GetAllReferences())
    {
        if (!Reference.ID.IsValid() || !Guids.Contains(Reference.ID) || (!Reference.CustomBinding.IsResolved()||(Reference.CustomBinding.Get()!=nullptr&&(!C.bMixedRemoval||!CameraMaintenanceMixedBinding(C.CameraMaintenance,Reference.ID,Reference.CustomBinding.Get())))) || (HasPrevious && Reference.ID < Previous))
        { return Fail(R, TEXT("sequencer_remove_binding_references_unsupported"), TEXT("Custom/orphan/unsorted binding references are not admitted; locators are observed without resolving objects.")); }
        Previous = Reference.ID; HasPrevious = true;
    }
    const FMovieSceneBinding* Binding = C.Scene->FindBinding(C.Binding);
    if (!Binding || Binding->GetTracks().Num() > MaxTracks) { return Fail(R, TEXT("sequencer_remove_binding_shape_unsupported"), TEXT("Current selected binding and bounded track list required.")); }
    for (UMovieSceneTrack* Track : Binding->GetTracks())
    {
        if (!Loaded(Track) || Track->GetOuter() != C.Scene || (Track->GetClass() != UMovieScene3DTransformTrack::StaticClass() && Track->GetClass() != UMovieSceneFloatTrack::StaticClass() && !(C.bRemovalClips&&SubsequenceClipKind(Track))) || Track->GetDecorations().Num() != 0)
        { return Fail(R, TEXT("sequencer_remove_binding_track_unsupported"), TEXT("Only exact native Transform/Float or admitted Audio/SkeletalAnimation tracks without lifetime decorations are admitted; custom tracks and callback variants remain incomplete.")); }
        for (UMovieSceneSection* Section : Track->GetAllSections())
        {
            if (!Loaded(Section) || Section->GetOuter() != Track || Section->GetDecorations().Num() != 0 || (C.bRemovalClips&&SubsequenceClipKind(Track) ? (SubsequenceClipKind(Track)==1 ? Section->GetClass()!=UMovieSceneSkeletalAnimationSection::StaticClass() : Section->GetClass()!=UMovieSceneAudioSection::StaticClass()) : (Track->GetClass() == UMovieScene3DTransformTrack::StaticClass() ? Section->GetClass() != UMovieScene3DTransformSection::StaticClass() : Section->GetClass() != UMovieSceneFloatSection::StaticClass())))
            { return Fail(R, TEXT("sequencer_remove_binding_section_unsupported"), TEXT("Only exact native owned sections without lifetime decorations are admitted.")); }
        }
        C.MaintenanceTracks.Emplace(Track);
    }
    // JL preserves a separate exact camera-cut track literally, including a cut's
    // now-unbound GUID after removal. No automatic reference rewrite or camera cleanup.
    // Ordinary scalar/transform and unsupported attachment/subsequence guards stay intact.
    TArray<UMovieSceneTrack*> AllTracks = C.Scene->GetTracks();
    for (const auto& CurrentBinding : Bindings) { AllTracks.Append(CurrentBinding.GetTracks()); }
    if (AllTracks.Num() > MaxTracks) { return Fail(R, TEXT("sequencer_remove_binding_track_unsupported"), TEXT("Total track budget exceeded before mutation.")); }
    for (UMovieSceneTrack* Track : AllTracks)
    {
        if (!Loaded(Track) || Track->GetOuter() != C.Scene || (Track->GetClass() != UMovieScene3DTransformTrack::StaticClass() && Track->GetClass() != UMovieSceneFloatTrack::StaticClass()&&!(C.bRemovalClips&&SubsequenceClipKind(Track))&&!MaintenanceMixedSpawnTrack(C,Track))) { return Fail(R, TEXT("sequencer_remove_binding_external_track_unsupported"), TEXT("Camera/attachment/subsequence/custom track reference variants remain incomplete; all current tracks must be admitted native scalar/transform or Audio/SkeletalAnimation tracks.")); }
        for (UMovieSceneSection* Section : Track->GetAllSections())
        {
            auto Typed = MakeShared<FJsonObject>(); FMovieSceneFloatChannel* Scalar = nullptr; TArray<FMovieSceneDoubleChannel*> Transforms;
            const bool Spawn=MaintenanceMixedSpawnTrack(C,Track);
            const int32 ClipKind=C.bRemovalClips?SubsequenceClipKind(Track):0;
            bool Supported=false;
            if(ClipKind)
            {
                const TFunction<bool()> Current=[&C]{return CameraMaintenancePlainCheck(C.CameraMaintenance,TEXT("removal_clip_channel_admission_return"));};
                Supported=Loaded(Section)&&Section->GetOuter()==Track&&Section->GetDecorations().IsEmpty()&&SubsequenceClipChannels(Section,ClipKind,Typed,Current);
            }
            else if(Spawn){if(Loaded(Section)&&(Section->GetClass()==UMovieSceneBoolSection::StaticClass()||Section->GetClass()==UMovieSceneSpawnSection::StaticClass())&&Section->GetOuter()==Track&&Section->GetDecorations().Num()==0){const auto& Channel=static_cast<UMovieSceneBoolSection*>(Section)->GetChannel();const auto Times=Channel.GetTimes();const auto Values=Channel.GetValues();Supported=Times.Num()==Values.Num()&&Times.Num()<=MaxKeys;for(int32 I=1;Supported&&I<Times.Num();++I)Supported=Times[I]>=Times[I-1];}}
            else Supported=Track->GetClass() == UMovieSceneFloatTrack::StaticClass() ? FloatChannel(Section, Scalar, Typed) : TransformChannels(Section, Transforms, Typed);
            if (!Supported) { return Fail(R, TEXT("sequencer_remove_binding_external_channel_unsupported"), TEXT("All current admitted native scalar/transform/clip channels must have the finite ordinary shape; custom/constraint channel overrides remain incomplete.")); }
        }
    }
    FArrayProperty* PossessablesProperty = FindFProperty<FArrayProperty>(C.Scene->GetClass(), TEXT("Possessables")); FStructProperty* Inner = PossessablesProperty ? CastField<FStructProperty>(PossessablesProperty->Inner) : nullptr;
    FStructProperty* ReferencesProperty = FindFProperty<FStructProperty>(C.Sequence->GetClass(), TEXT("BindingReferences"));
    if (!PossessablesProperty || PossessablesProperty->GetOwnerClass() != UMovieScene::StaticClass() || !Inner || !Loaded(Inner->Struct) || Inner->Struct->GetPathName() != TEXT("/Script/MovieScene.MovieScenePossessable") || Inner->Struct->GetStructureSize() != static_cast<int32>(sizeof(FMovieScenePossessable))
        || !ReferencesProperty || !Loaded(ReferencesProperty->Struct) || ReferencesProperty->GetOwnerClass() != ULevelSequence::StaticClass() || ReferencesProperty->Struct->GetPathName() != TEXT("/Script/LevelSequence.UpgradedLevelSequenceBindingReferences") || ReferencesProperty->Struct->GetStructureSize() != static_cast<int32>(sizeof(FUpgradedLevelSequenceBindingReferences)))
    { return Fail(R, TEXT("sequencer_remove_binding_storage_unobservable"), TEXT("Exact native reflected array/reference layouts are required for read-only expected-copy exports; no private storage write is used.")); }
    FUpgradedLevelSequenceBindingReferences RemainingReferences; static_cast<FMovieSceneBindingReferences&>(RemainingReferences) = *References; R.Data->SetNumberField(TEXT("expected_reference_copy_removal_calls"), 1); RemainingReferences.RemoveBinding(C.Binding);
    R.Data->SetObjectField(TEXT("removal_expected_possessables_export"), ExpectedPossessables); R.Data->SetObjectField(TEXT("removal_expected_binding_references_export"), ExpectedReferences);
    const TFunction<bool()> Current=(C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval||C.bRemovalClips)?TFunction<bool()>([&C]{return CameraMaintenancePlainCheck(C.CameraMaintenance,TEXT("component_removal_expected_export"));}):TFunction<bool()>();
    if (!MaintenanceExportExpected(PossessablesProperty, &Remaining, C.Scene, ExpectedPossessables,Current) || !MaintenanceExportExpected(ReferencesProperty, &RemainingReferences, C.Sequence, ExpectedReferences,Current))
    { if(C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval||C.bRemovalClips)CameraMaintenanceAbort(C.CameraMaintenance,TEXT("component_remove_expected_export_incomplete")); return Fail(R, TEXT("sequencer_remove_binding_expected_unobservable"), TEXT("The bounded expected public-copy removal cannot be exported before mutation.")); }
    if(!C.bLeafRemoval&&!C.bHierarchyRemoval)R.Data->SetStringField(TEXT("removal_scope"), TEXT("Selected flat native possessable/local Transform, Float or admitted Audio/SkeletalAnimation tracks are removed. UA additionally observes collateral native legacy spawnable declarations or nonselected SpawnableActorBinding/plain Actor templates, SpawnTrack/BoolSection, leaf folders and fixed root tags under the same fenced snapshot. Selected custom/spawnable, nested template/component/folder/hierarchy/custom callbacks and reference rewrites remain valid incomplete. No physical object deletion/GC/evaluation is claimed."));
    return true;
}
// Removal-only exact JSON structure comparison: object map enumeration is not
// data order. Do not use CompareEqual recursively without checking types because
// its public cross-type scalar variant branch permits coercion.
bool MaintenanceRemovalEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, FString& Mismatch, const FString& Path, int32& Visited, int32 Depth = 0)
{
    if (++Visited > 1048576 || Depth > 64) { Mismatch = Path + TEXT(": comparison_budget"); return false; }
    if (!A.IsValid() || !B.IsValid()) { Mismatch = Path + TEXT(": invalid_value"); return false; }
    if (A->Type != B->Type) { Mismatch = Path + TEXT(": type"); return false; }
    if (A->Type == EJson::Object)
    {
        const auto Left = A->AsObject(), Right = B->AsObject();
        if (!Left.IsValid() || !Right.IsValid() || Left->Values.Num() != Right->Values.Num()) { Mismatch = Path + TEXT(": object_cardinality"); return false; }
        for (const auto& Pair : Left->Values)
        {
            const auto Value = Right->TryGetField(Pair.Key.ToView());
            if (!MaintenanceRemovalEqual(Pair.Value, Value, Mismatch, Path + TEXT("/") + FString(Pair.Key.ToView()), Visited, Depth + 1)) { return false; }
        }
        return true;
    }
    if (A->Type == EJson::Array)
    {
        const auto& Left = A->AsArray(); const auto& Right = B->AsArray();
        if (Left.Num() != Right.Num()) { Mismatch = Path + TEXT(": array_cardinality"); return false; }
        for (int32 Index = 0; Index < Left.Num(); ++Index)
        { if (!MaintenanceRemovalEqual(Left[Index], Right[Index], Mismatch, Path + TEXT("/") + FString::FromInt(Index), Visited, Depth + 1)) { return false; } }
        return true;
    }
    const bool Equal = A->Type == EJson::String ? A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive) : FJsonValue::CompareEqual(*A, *B);
    if (!Equal) { Mismatch = Path + TEXT(": scalar_value"); } return Equal;
}
bool MaintenanceRemovalPreserved(FResult& R, FContext& C, const TSharedRef<FJsonObject>& BeforeState, const TSharedRef<FJsonObject>& AfterState, const TSharedRef<FJsonObject>& Possessables, const TSharedRef<FJsonObject>& References)
{
    auto Expected = MakeShared<FJsonObject>(); Expected->Values = BeforeState->Values;
    auto SequenceFields = MakeShared<FJsonObject>(); SequenceFields->Values = BeforeState->GetObjectField(TEXT("sequence_fields"))->Values; SequenceFields->SetObjectField(TEXT("BindingReferences"), References); Expected->SetObjectField(TEXT("sequence_fields"), SequenceFields);
    auto SceneFields = MakeShared<FJsonObject>(); SceneFields->Values = BeforeState->GetObjectField(TEXT("movie_scene_fields"))->Values; SceneFields->SetObjectField(TEXT("Possessables"), Possessables); Expected->SetObjectField(TEXT("movie_scene_fields"), SceneFields);
    TArray<TSharedPtr<FJsonValue>> BindingRows;
    for (const auto& Value : BeforeState->GetArrayField(TEXT("bindings"))) { FGuid Id; if (!FGuid::Parse(Value->AsObject()->GetStringField(TEXT("binding_id")), Id)) { return false; } if (Id != C.Binding) { BindingRows.Add(Value); } }
    Expected->SetArrayField(TEXT("bindings"), BindingRows); auto Tracks = MakeShared<FJsonObject>(); Tracks->Values = BeforeState->GetObjectField(TEXT("tracks"))->Values; int32 RemovedSections = 0;
    for (const auto& Keep : C.MaintenanceTracks) { RemovedSections += Keep->GetAllSections().Num(); Tracks->RemoveField(Keep->GetPathName()); }
    Expected->SetObjectField(TEXT("tracks"), Tracks); Expected->SetNumberField(TEXT("track_count"), BeforeState->GetNumberField(TEXT("track_count")) - C.MaintenanceTracks.Num()); Expected->SetNumberField(TEXT("section_count"), BeforeState->GetNumberField(TEXT("section_count")) - RemovedSections);
    auto Actual = MakeShared<FJsonObject>(); Actual->Values = AfterState->Values;
    for (const TCHAR* Key : { TEXT("dirty"), TEXT("sequence_signature"), TEXT("movie_scene_signature") }) { Expected->RemoveField(Key); Actual->RemoveField(Key); }
    R.Data->SetObjectField(TEXT("removal_expected_preservation_state"), Expected); R.Data->SetObjectField(TEXT("removal_actual_preservation_state"), Actual);
    R.Data->SetBoolField(TEXT("removal_serialized_text_equal"), Same(Expected, Actual));
    FString Mismatch; int32 Visited = 0;
    const bool Equal = MaintenanceRemovalEqual(MakeShared<FJsonValueObject>(Expected), MakeShared<FJsonValueObject>(Actual), Mismatch, TEXT("$"), Visited);
    R.Data->SetBoolField(TEXT("removal_structural_state_equal"), Equal); R.Data->SetStringField(TEXT("removal_first_mismatch"), Mismatch); R.Data->SetNumberField(TEXT("removal_comparison_values_visited"), Visited);
    R.Data->SetStringField(TEXT("removal_comparison_scope"), TEXT("Exact object key sets/types/scalar values/exported text; arrays remain ordered. Only JSON object map enumeration order is ignored, no flags/fields/strings/numeric types or original target projection are masked."));
    return Equal;
}
}

ULevelSequence* ResolveAsset(const FString& SequencePath,FString& OutReceiptJson)
{
    FResult R;Init(R,TEXT("resolve_level_sequence_asset"));FContext C;
    if(!Resolve(SequencePath,C,R,true)){OutReceiptJson=Encode(R);return nullptr;}
    if(!PrimaryResolvedCurrent(C,R)){OutReceiptJson=Encode(R);return nullptr;}
    R.bSuccess=R.bPost=true;R.Stage=TEXT("complete");ULevelSequence* Asset=C.KeepSequence.Get();OutReceiptJson=Encode(R);return Asset;
}
FString ApplyPlaybackRange(const FString& SequencePath, double PlaybackStartFrame, double PlaybackEndFrame)
{
    FResult R; Init(R, TEXT("apply_level_sequence_playback_range")); FContext C; FFrameNumber Start, End;
    if (!Resolve(SequencePath, C, R)) { return Encode(R); }
    if (!(PlaybackEndFrame > PlaybackStartFrame) || !Tick(PlaybackStartFrame, C, Start) || !Tick(PlaybackEndFrame, C, End) || End < Start)
    { Fail(R, TEXT("sequencer_playback_range_invalid"), TEXT("Finite end must exceed start and both original floor-converted ticks must fit signed int32.")); return Encode(R); }
    R.Data->SetNumberField(TEXT("requested_start_frame"), PlaybackStartFrame); R.Data->SetNumberField(TEXT("requested_end_frame"), PlaybackEndFrame);
    R.Data->SetNumberField(TEXT("start_tick"), Start.Value); R.Data->SetNumberField(TEXT("end_tick"), End.Value);
    if (!Before(C, R)) { return Encode(R); }
    const TRange<FFrameNumber> Expected(Start, End);
    FMovieSceneEditorData ExpectedEditor = C.Scene->GetEditorData();
    if (C.Scene->GetPlaybackRange() != Expected)
    {
        // SetPlaybackRange initializes only invalid working/view intervals (MovieScene.cpp:725-746).
        const double A = Start / C.Scene->GetTickResolution(), B = End / C.Scene->GetTickResolution();
        const double Padding = (B - A) * 0.1;
        if (ExpectedEditor.WorkStart >= ExpectedEditor.WorkEnd) { ExpectedEditor.WorkStart = A - Padding; ExpectedEditor.WorkEnd = B + Padding; }
        if (ExpectedEditor.ViewStart >= ExpectedEditor.ViewEnd) { ExpectedEditor.ViewStart = A - Padding; ExpectedEditor.ViewEnd = B + Padding; }
    }
    auto ExpectedEditorFields = MakeShared<FJsonObject>();
    if (!Properties(FMovieSceneEditorData::StaticStruct(), &ExpectedEditor, C.Scene, {}, ExpectedEditorFields))
    { Fail(R, TEXT("sequencer_editor_data_unobservable"), TEXT("Expected public playback view/working-range state cannot be fully observed; no mutation dispatched.")); return Encode(R); }
    R.Stage = TEXT("mutate"); R.bAttempted = true; C.Sequence->Modify(); C.Scene->Modify();
    C.Scene->SetPlaybackRange(Expected); C.Sequence->MarkPackageDirty();
    auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); }
    const bool Preserved = CommonPreserved(C, After, true) && SameField(C.Before, After, TEXT("bindings")) && SameField(C.Before, After, TEXT("tracks"));
    R.Data->SetBoolField(TEXT("nonselected_state_preserved"), Preserved);
    const bool EditorPreserved = Same(ExpectedEditorFields, After->GetObjectField(TEXT("editor_data")).ToSharedRef());
    R.Data->SetBoolField(TEXT("playback_editor_ranges_verified"), EditorPreserved);
    R.bPost = Preserved && EditorPreserved && C.Scene->GetPlaybackRange() == Expected;
    R.Data->SetBoolField(TEXT("playback_floor_verified"), R.bPost);
    if (!R.bPost) { Fail(R, TEXT("sequencer_playback_postcondition_failed"), TEXT("Range or unrelated declarative state did not match; mutation is retained and no save/compile/retry occurred.")); }
    else { R.bSuccess = true; R.Stage = TEXT("complete"); } return Encode(R);
}

FString AddTransformKeys(const FString& SequencePath, const FString& KeysJson, const FString& BindingId, const FString& ActorLabel)
{
    FResult R; Init(R, TEXT("add_level_sequence_transform_keys")); FContext C; TArray<FKeyRow> Rows; TArray<TSharedPtr<FJsonValue>> RawRows;
    R.Data->SetNumberField(TEXT("keys_added"), 0); R.Data->SetNumberField(TEXT("scalar_keys_added"), 0);
    R.Data->SetBoolField(TEXT("created_track"), false); R.Data->SetBoolField(TEXT("created_section"), false);
    if (!Resolve(SequencePath, C, R,true) || !ResolveBinding(C, BindingId, ActorLabel, R) || !ParseKeys(KeysJson, C, Rows, RawRows, R)) { return Encode(R); }
    const bool OriginalRaw=!RawRows.IsEmpty();const int32 RequestedRows=OriginalRaw?RawRows.Num():Rows.Num();
    C.Track = C.Scene->FindTrack(UMovieScene3DTransformTrack::StaticClass(), C.Binding);
    if (C.Track)
    {
        if (!Loaded(C.Track) || C.Track->GetClass() != UMovieScene3DTransformTrack::StaticClass() || C.Track->GetOuter() != C.Scene)
        { Fail(R, TEXT("sequencer_transform_track_identity_invalid"), TEXT("Existing first transform track must be the exact native class owned by the selected MovieScene.")); return Encode(R); }
        const auto& Sections = C.Track->GetAllSections();
        if (!Sections.IsEmpty()) { C.Section = Sections[0]; }
        if (C.Section && (!Loaded(C.Section) || C.Section->GetClass() != UMovieScene3DTransformSection::StaticClass() || C.Section->GetOuter() != C.Track || C.Section->IsReadOnly()))
        { Fail(R, TEXT("sequencer_transform_section_unsupported"), TEXT("Existing first section must be an owned writable exact native transform section. Wrong-type first sections are rejected before allocation.")); return Encode(R); }
    }
    TArray<FMovieSceneDoubleChannel*> ExistingChannels; auto Existing = MakeShared<FJsonObject>();
    if (C.Section && !TransformChannels(C.Section, ExistingChannels, Existing))
    { Fail(R, TEXT("sequencer_transform_channels_unsupported"), TEXT("Existing section must expose exact ordered nine Double channels and Float Weight before allocation; channel overrides/constraints remain pending.")); return Encode(R); }
    for (int32 Axis = 0; Axis < 9; ++Axis)
    {
        int32 Requested = 0; for (const FKeyRow& Row : Rows) { Requested += Row.Values[Axis].IsSet() ? 1 : 0; }
        const int32 ExistingCount = C.Section ? ExistingChannels[Axis]->GetTimes().Num() : 0;
        if (Requested + ExistingCount > MaxKeys)
        { Fail(R, TEXT("sequencer_channel_key_budget_exceeded"), TEXT("All requested axis counts must fit the 8192-key bound before allocation or mutation.")); return Encode(R); }
    }
    TStrongObjectPtr<UMovieSceneTrack> RawTrackHold;TStrongObjectPtr<UMovieSceneSection> RawSectionHold;
    if(OriginalRaw){RawTrackHold.Reset(C.Track);RawSectionHold.Reset(C.Section);if(!OriginalKeyCurrent(C,R))return Encode(R);}
    if (!Before(C, R)) { return Encode(R); }if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    R.Stage = TEXT("mutate"); R.bAttempted = true;if(OriginalRaw)R.bKnown=false; C.Sequence->Modify();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R); C.Scene->Modify();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    bool NewTrack = false, NewSection = false;
    if (!C.Track)
    {
        C.Track = C.Scene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), C.Binding); NewTrack = C.Track != nullptr;
        R.Data->SetBoolField(TEXT("created_track"), NewTrack);if(OriginalRaw){RawTrackHold.Reset(C.Track);if(!OriginalKeyCurrent(C,R))return Encode(R);}
    }
    if (!Loaded(C.Track) || C.Track->GetClass() != UMovieScene3DTransformTrack::StaticClass() || C.Track->GetOuter() != C.Scene)
    {
        auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); }
        Fail(R, TEXT("sequencer_transform_track_creation_failed"), TEXT("Public AddTrack did not return the exact owned native track; any observed prefix remains retained.")); return Encode(R);
    }
    C.Track->Modify();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R); TStrongObjectPtr<UMovieSceneSection> KeepSection;
    if (!C.Section)
    {
        // Base virtual dispatch avoids an unexported MinimalAPI derived override symbol.
        C.Section = C.Track->CreateNewSection(); KeepSection.Reset(C.Section);
        R.Data->SetBoolField(TEXT("section_allocation_returned"), C.Section != nullptr);if(OriginalRaw&&!OriginalKeyCurrent(C,R,false))return Encode(R);
        if (Loaded(C.Section) && C.Section->GetClass() == UMovieScene3DTransformSection::StaticClass() && C.Section->GetOuter() == C.Track)
        { C.Track->AddSection(*C.Section);if(OriginalRaw){R.Data->SetBoolField(TEXT("section_attachment_returned"),true);NewSection=true;R.Data->SetBoolField(TEXT("created_section"),true);if(!OriginalKeyCurrent(C,R))return Encode(R);} C.Section->SetRange(TRange<FFrameNumber>::All());if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R); NewSection = true; R.Data->SetBoolField(TEXT("created_section"), true); }
    }
    R.Data->SetStringField(TEXT("track_path"), C.Track->GetPathName());
    if (!Loaded(C.Section) || C.Section->GetClass() != UMovieScene3DTransformSection::StaticClass() || C.Section->GetOuter() != C.Track
        || !C.Track->GetAllSections().Contains(C.Section) || C.Section->IsReadOnly())
    {
        auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); }
        Fail(R, TEXT("sequencer_transform_section_creation_failed"), TEXT("Public section creation/attachment did not yield the exact owned writable first section; observed prefix remains retained.")); return Encode(R);
    }
    R.Data->SetStringField(TEXT("section_path"), C.Section->GetPathName()); C.Section->Modify();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    TArray<FMovieSceneDoubleChannel*> Channels; auto Initial = MakeShared<FJsonObject>();
    if (!TransformChannels(C.Section, Channels, Initial))
    {
        auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); }
        Fail(R, TEXT("sequencer_transform_channels_unsupported"), TEXT("Requires exact ordered nine native Double channels plus unchanged Float Weight; overrides/constraint channels remain outside this cohort.")); return Encode(R);
    }
    if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    R.Data->SetObjectField(TEXT("channels_before_insertion"), Initial);
    TArray<FMovieSceneDoubleChannel> Expected; for (auto* Ch : Channels) { Expected.Add(*Ch); }
    int32 ScalarCount = 0; TArray<TSharedPtr<FJsonValue>> Insertions;
    for (int32 I = 0; I < RequestedRows; ++I)
    {
        FKeyRow OriginalRow;TSharedPtr<FJsonObject> Input;const TArray<TSharedPtr<FJsonValue>>* GroupValues=nullptr;
        if(OriginalRaw)
        {
            R.Stage=TEXT("raw_key_row");const auto Value=RawRows[I];
            if(!Value||Value->Type!=EJson::Object||!Value->AsObject()->HasTypedField<EJson::Number>(TEXT("frame")))
            {OriginalKeyFailure(R,I,TEXT("frame"),INDEX_NONE,Value,TEXT("sequencer_raw_key_row_invalid"),TEXT("Each key must be an object with a numeric 'frame'"));break;}
            Input=Value->AsObject();OriginalRow.DisplayFrame=Input->GetNumberField(TEXT("frame"));
            if(!Tick(OriginalRow.DisplayFrame,C,OriginalRow.Time))
            {OriginalKeyFailure(R,I,TEXT("frame"),INDEX_NONE,Input->TryGetField(TEXT("frame")),TEXT("sequencer_frame_out_of_range"),TEXT("Original display-frame conversion exceeds the finite native time profile; earlier insertions remain."));break;}
        }
        FKeyRow& Key=OriginalRaw?OriginalRow:Rows[I];
        auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("row_index"), I); Row->SetNumberField(TEXT("display_frame"), Key.DisplayFrame); Row->SetNumberField(TEXT("tick"), Key.Time.Value);
        TArray<TSharedPtr<FJsonValue>> AxisRows;
        for (int32 Axis = 0; Axis < 9; ++Axis)
        {
            if(OriginalRaw)
            {
                const TCHAR* Groups[]={TEXT("location"),TEXT("rotation"),TEXT("scale")};const TCHAR* Group=Groups[Axis/3];
                if(Axis%3==0)
                {
                    GroupValues=nullptr;
                    // Original TryGetArrayField silently skips absent/non-array groups.
                    if(Input->TryGetArrayField(Group,GroupValues)&&GroupValues&&GroupValues->Num()!=3)
                    {OriginalKeyFailure(R,I,Group,INDEX_NONE,Input->TryGetField(Group),TEXT("sequencer_raw_vector_shape_invalid"),TEXT("location/rotation/scale must be 3-element arrays when present"));break;}
                }
                if(!GroupValues)continue;
                const auto Value=(*GroupValues)[Axis%3];const double Number=Value?Value->AsNumber():0.0;
                if(!FMath::IsFinite(Number))
                {OriginalKeyFailure(R,I,Group,Axis%3,Value,TEXT("sequencer_raw_axis_value_pending"),TEXT("Original AsNumber axis is outside finite channel storage; previous axes remain, no later writer/save."));break;}
                Key.Values[Axis]=Number;
            }
            if (!Key.Values[Axis].IsSet()) { continue; }
            if (Channels[Axis]->GetTimes().Num() >= MaxKeys)
            { Fail(R, TEXT("sequencer_channel_key_budget_exceeded"), TEXT("Channel key bound reached after a known retained prefix; remaining rows are not replayed.")); break; }
            const int32 ExpectedIndex = Expected[Axis].AddCubicKey(Key.Time, Key.Values[Axis].GetValue(), RCTM_Auto);
            const int32 Index = Channels[Axis]->AddCubicKey(Key.Time, Key.Values[Axis].GetValue(), RCTM_Auto); ++ScalarCount;
            auto AJ = MakeShared<FJsonObject>(); AJ->SetStringField(TEXT("channel"), ChannelNames[Axis]); AJ->SetNumberField(TEXT("insert_index"), Index); AJ->SetNumberField(TEXT("value"), Key.Values[Axis].GetValue()); AxisRows.Add(MakeShared<FJsonValueObject>(AJ));
            auto Actual = MakeShared<FJsonObject>(); auto Wanted = MakeShared<FJsonObject>();
            if (Index != ExpectedIndex || !Channel(Channels[Axis], ChannelNames[Axis], Actual) || !Channel(&Expected[Axis], ChannelNames[Axis], Wanted) || !Same(Actual, Wanted))
            { Fail(R, TEXT("sequencer_cubic_insert_postcondition_failed"), TEXT("Actual insertion/order/Cubic/Auto tangent data differs from the public native channel-copy result; the prefix is retained.")); break; }
        }
        Row->SetArrayField(TEXT("axes"), AxisRows); Insertions.Add(MakeShared<FJsonValueObject>(Row));
        R.Data->SetArrayField(TEXT("insertions"), Insertions); R.Data->SetNumberField(TEXT("scalar_keys_added"), ScalarCount);
        if (!R.Code.IsEmpty()) { break; }
        R.Data->SetNumberField(TEXT("keys_added"), I + 1);
    }
    if(!OriginalRaw||R.Code.IsEmpty()){C.Sequence->MarkPackageDirty();if(OriginalRaw){R.Data->SetNumberField(TEXT("mark_package_dirty_calls"),1);if(!OriginalKeyCurrent(C,R))return Encode(R);}}else R.Data->SetNumberField(TEXT("mark_package_dirty_calls"),0);
    auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); }if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    const bool Preserved = KeyStructurePreserved(C, After, NewTrack, NewSection);
    R.Data->SetBoolField(TEXT("nonselected_state_preserved"), Preserved);
    auto FinalChannels = MakeShared<FJsonObject>(); TArray<FMovieSceneDoubleChannel*> Final;
    bool Typed = TransformChannels(C.Section, Final, FinalChannels); R.Data->SetObjectField(TEXT("channels_after_insertion"), FinalChannels);
    bool Exact = Typed && Final.Num() == 9 && SameField(Initial, FinalChannels, TEXT("weight_channel"));
    for (int32 I = 0; Exact && I < 9; ++I)
    {
        auto Actual = MakeShared<FJsonObject>(); auto Wanted = MakeShared<FJsonObject>();
        Exact = Channel(Final[I], ChannelNames[I], Actual) && Channel(&Expected[I], ChannelNames[I], Wanted) && Same(Actual, Wanted);
    }
    R.Data->SetBoolField(TEXT("weight_channel_preserved"), Typed && SameField(Initial, FinalChannels, TEXT("weight_channel")));
    R.Data->SetBoolField(TEXT("native_cubic_channels_verified"), Exact); R.Data->SetBoolField(TEXT("duplicate_tick_insertion_supported"), true);
    R.Data->SetStringField(TEXT("rotation_axis_order"), TEXT("Roll,Pitch,Yaw -> Rotation.X,Rotation.Y,Rotation.Z"));
    R.Data->SetNumberField(TEXT("requested_key_rows"), RequestedRows); R.Data->SetNumberField(TEXT("cubic_interpolation_value"), static_cast<uint8>(RCIM_Cubic));
    R.Data->SetNumberField(TEXT("auto_tangent_mode_value"), static_cast<uint8>(RCTM_Auto));
    R.bPost = R.Code.IsEmpty() && Preserved && Exact && (!NewSection || C.Section->GetRange() == TRange<FFrameNumber>::All());
    if (R.Code.IsEmpty() && !R.bPost) { Fail(R, TEXT("sequencer_transform_keys_postcondition_failed"), TEXT("Target channels or unrelated bindings/tracks/sections changed unexpectedly; no rollback, compile, save or retry occurred.")); }
    if (R.bPost) { R.bSuccess = true; R.Stage = TEXT("complete"); } return Encode(R);
}

FString AddFloatPropertyKeys(const FString& SequencePath, const FString& PropertyPath,
    const FString& KeysJson, const FString& BindingId, const FString& ActorLabel)
{
    FResult R; Init(R, TEXT("add_level_sequence_float_property_keys")); FContext C; C.bFloat = true;
    R.Data->SetNumberField(TEXT("keys_added"), 0); R.Data->SetNumberField(TEXT("scalar_keys_added"), 0); R.Data->SetBoolField(TEXT("created_track"), false); R.Data->SetBoolField(TEXT("created_section"), false);
    for (const TCHAR* Counter : { TEXT("explicit_modify_calls"), TEXT("track_create_attempts"), TEXT("section_create_attempts"), TEXT("cubic_key_attempts"), TEXT("mark_package_dirty_calls") }) { R.Data->SetNumberField(Counter, 0); }
    R.Data->SetStringField(TEXT("property_path"), PropertyPath); R.Data->SetBoolField(TEXT("runtime_property_evaluated"), false);
    R.Data->SetStringField(TEXT("float_scope"), TEXT("exact native scalar FloatTrack/FloatSection and declarative key data only; no Actor property/evaluation/playback invocation; public Python owns requested exact save"));
    if (!Resolve(SequencePath, C, R,true) || !ResolveBinding(C, BindingId, ActorLabel, R)) { return Encode(R); }
    R.Data->SetStringField(TEXT("sequence"), C.Sequence->GetPathName());
    FString PropertyName = PropertyPath; int32 LastDot = INDEX_NONE; if (PropertyPath.FindLastChar(TEXT('.'), LastDot)) { PropertyName = PropertyPath.Mid(LastDot + 1); }
    FText Why; bool HasNul = false; for (int32 I = 0; I < PropertyPath.Len(); ++I) { HasNul |= PropertyPath[I] == TCHAR('\0'); }
    if (PropertyPath.IsEmpty() || PropertyPath.Len() > 1024 || HasNul || PropertyName.IsEmpty() || PropertyName.Len() >= NAME_SIZE || !FName::IsValidXName(PropertyName, INVALID_OBJECTNAME_CHARACTERS, &Why) || FName(*PropertyName).IsNone())
    { Fail(R, TEXT("sequencer_float_property_path_invalid"), TEXT("Bounded nonempty original property path with a valid non-None final FName segment required; runtime property existence is not evaluated.")); return Encode(R); }
    if (!Native(UMovieSceneFloatTrack::StaticClass(), TEXT("/Script/MovieSceneTracks.MovieSceneFloatTrack")) || !Native(UMovieSceneFloatSection::StaticClass(), TEXT("/Script/MovieSceneTracks.MovieSceneFloatSection")))
    { Fail(R, TEXT("sequencer_float_native_source_invalid"), TEXT("Exact clean loaded native FloatTrack/FloatSection class basis required.")); return Encode(R); }
    struct FScalarKey { double Frame = 0; float Value = 0; FFrameNumber Time; };
    TArray<FScalarKey> Rows; TSharedPtr<FJsonValue> Root;TArray<TSharedPtr<FJsonValue>> RawRows;
    if (KeysJson.Len() > MaxText || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(KeysJson), Root) || !Root.IsValid() || Root->Type != EJson::Array || Root->AsArray().IsEmpty() || Root->AsArray().Num() > MaxRows)
    { Fail(R, TEXT("sequencer_float_keys_invalid"), TEXT("Provide bounded 1..512 scalar key objects; all finite input rows are admitted before mutation.")); return Encode(R); }
    if(!UnpackOriginalKeyRows(Root,TEXT("float"),RawRows,R))return Encode(R);const bool OriginalRaw=!RawRows.IsEmpty();
    if(!OriginalRaw)for (const auto& Input : Root->AsArray())
    {
        if (!Input.IsValid() || Input->Type != EJson::Object) { Fail(R, TEXT("sequencer_float_key_row_invalid"), TEXT("Every scalar key must be an object with numeric frame/value.")); return Encode(R); }
        const auto J = Input->AsObject(); const auto Frame = J->TryGetField(TEXT("frame")), Value = J->TryGetField(TEXT("value")); FScalarKey Row; double Number = 0;
        if (!Frame.IsValid() || Frame->Type != EJson::Number || !Frame->TryGetNumber(Row.Frame) || !Tick(Row.Frame, C, Row.Time) || !Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number) || FMath::Abs(Number) > static_cast<double>(MAX_flt))
        { Fail(R, TEXT("sequencer_float_key_value_invalid"), TEXT("Finite float-representable value and original display-frame FloorToFrame conversion within signed int32 required.")); return Encode(R); }
        Row.Value = static_cast<float>(Number); if (!FMath::IsFinite(Row.Value)) { Fail(R, TEXT("sequencer_float_key_value_invalid"), TEXT("Stored native Float key value must remain finite.")); return Encode(R); } Rows.Add(Row);
    }
    // Original search is the first float track with the exact property-path spelling.
    for (UMovieSceneTrack* Track : C.Scene->FindTracks(UMovieSceneFloatTrack::StaticClass(), C.Binding))
    {
        UMovieSceneFloatTrack* Float = Cast<UMovieSceneFloatTrack>(Track);
        if (Float && Float->GetPropertyPath().ToString() == PropertyPath) { C.Track = Track; break; }
    }
    if (C.Track)
    {
        if (!Loaded(C.Track) || C.Track->GetClass() != UMovieSceneFloatTrack::StaticClass() || C.Track->GetOuter() != C.Scene || static_cast<UMovieSceneFloatTrack*>(C.Track)->GetPropertyName() != FName(*PropertyName))
        { Fail(R, TEXT("sequencer_float_track_identity_invalid"), TEXT("First matching track must be exact native FloatTrack with the original final property name and selected MovieScene owner.")); return Encode(R); }
        const auto& Sections = C.Track->GetAllSections(); if (!Sections.IsEmpty()) { C.Section = Sections[0]; }
        if (C.Section && (!Loaded(C.Section) || C.Section->GetClass() != UMovieSceneFloatSection::StaticClass() || C.Section->GetOuter() != C.Track || C.Section->IsReadOnly()))
        { Fail(R, TEXT("sequencer_float_section_unsupported"), TEXT("Existing first section must be exact owned writable native FloatSection; incompatible-first-section fallback is outside finite admission.")); return Encode(R); }
    }
    FMovieSceneFloatChannel* Existing = nullptr; auto ExistingProof = MakeShared<FJsonObject>();
    R.Data->SetObjectField(TEXT("existing_channel_observation"), ExistingProof);
    if (C.Section && !FloatChannel(C.Section, Existing, ExistingProof)) { Fail(R, TEXT("sequencer_float_channel_unsupported"), TEXT("Exactly one anonymous native Float channel/current curve, null override registry and no extra channels required.")); return Encode(R); }
    if ((Existing ? Existing->GetTimes().Num() : 0) + Rows.Num() > MaxKeys) { Fail(R, TEXT("sequencer_channel_key_budget_exceeded"), TEXT("Existing and requested scalar keys must fit8192 before any allocation.")); return Encode(R); }
    TStrongObjectPtr<UMovieSceneTrack> RawTrackHold;TStrongObjectPtr<UMovieSceneSection> RawSectionHold;
    if(OriginalRaw){RawTrackHold.Reset(C.Track);RawSectionHold.Reset(C.Section);if(!OriginalKeyCurrent(C,R))return Encode(R);}
    if (!Before(C, R)) { return Encode(R); }if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    R.Stage = TEXT("mutate"); R.bAttempted = true; R.bKnown = false;
    R.Data->SetNumberField(TEXT("explicit_modify_calls"), 1); C.Sequence->Modify();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    R.Data->SetNumberField(TEXT("explicit_modify_calls"), 2); C.Scene->Modify();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R); bool NewTrack = false, NewSection = false;
    if (!C.Track)
    {
        R.Data->SetNumberField(TEXT("track_create_attempts"), 1); C.Track = C.Scene->AddTrack(UMovieSceneFloatTrack::StaticClass(), C.Binding); NewTrack = C.Track != nullptr; R.Data->SetBoolField(TEXT("created_track"), NewTrack);if(OriginalRaw){RawTrackHold.Reset(C.Track);if(!OriginalKeyCurrent(C,R))return Encode(R);}
        if (Loaded(C.Track) && C.Track->GetClass() == UMovieSceneFloatTrack::StaticClass() && C.Track->GetOuter() == C.Scene) { static_cast<UMovieSceneFloatTrack*>(C.Track)->SetPropertyNameAndPath(FName(*PropertyName), PropertyPath);if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R); }
    }
    if (!Loaded(C.Track) || C.Track->GetClass() != UMovieSceneFloatTrack::StaticClass() || C.Track->GetOuter() != C.Scene)
    { auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); } Fail(R, TEXT("sequencer_float_track_creation_failed"), TEXT("Public AddTrack prefix did not produce exact owned native FloatTrack; no retry/save.")); return Encode(R); }
    R.Data->SetNumberField(TEXT("explicit_modify_calls"), 3); C.Track->Modify();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R); TStrongObjectPtr<UMovieSceneSection> KeepSection;
    if (!C.Section)
    {
        // Public base virtual route, never an unexported MinimalAPI derived call.
        R.Data->SetNumberField(TEXT("section_create_attempts"), 1); C.Section = C.Track->CreateNewSection(); KeepSection.Reset(C.Section); R.Data->SetBoolField(TEXT("section_allocation_returned"), C.Section != nullptr);if(OriginalRaw&&!OriginalKeyCurrent(C,R,false))return Encode(R);
        if (Loaded(C.Section) && C.Section->GetClass() == UMovieSceneFloatSection::StaticClass() && C.Section->GetOuter() == C.Track) { C.Track->AddSection(*C.Section);if(OriginalRaw){R.Data->SetBoolField(TEXT("section_attachment_returned"),true);NewSection=true;R.Data->SetBoolField(TEXT("created_section"),true);if(!OriginalKeyCurrent(C,R))return Encode(R);} C.Section->SetRange(TRange<FFrameNumber>::All());if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R); NewSection = true; R.Data->SetBoolField(TEXT("created_section"), true); }
    }
    R.Data->SetStringField(TEXT("track_path"), C.Track->GetPathName());
    if (!Loaded(C.Section) || C.Section->GetClass() != UMovieSceneFloatSection::StaticClass() || C.Section->GetOuter() != C.Track || !C.Track->GetAllSections().Contains(C.Section) || C.Section->IsReadOnly())
    { auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); } Fail(R, TEXT("sequencer_float_section_creation_failed"), TEXT("Public section allocation/attachment prefix differs; no retry/save.")); return Encode(R); }
    R.Data->SetStringField(TEXT("section_path"), C.Section->GetPathName()); R.Data->SetNumberField(TEXT("explicit_modify_calls"), 4); C.Section->Modify();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R); FMovieSceneFloatChannel* Actual = nullptr; auto Initial = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("channel_before_insertion"), Initial);
    if (!FloatChannel(C.Section, Actual, Initial)) { auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); } Fail(R, TEXT("sequencer_float_channel_unsupported"), TEXT("Actual new/current scalar channel shape differs; retained factory prefix, no insertion/save.")); return Encode(R); }
    if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    FMovieSceneFloatChannel Expected(*Actual); TArray<TSharedPtr<FJsonValue>> Insertions;
    const int32 RequestedRows=OriginalRaw?RawRows.Num():Rows.Num();
    for (int32 I = 0; I < RequestedRows; ++I)
    {
        FScalarKey OriginalRow;
        if(OriginalRaw)
        {
            R.Stage=TEXT("raw_key_row");const auto Value=RawRows[I];
            if(!Value||Value->Type!=EJson::Object||!Value->AsObject()->HasTypedField<EJson::Number>(TEXT("frame"))||!Value->AsObject()->HasTypedField<EJson::Number>(TEXT("value")))
            {OriginalKeyFailure(R,I,TEXT("frame/value"),INDEX_NONE,Value,TEXT("sequencer_raw_float_row_invalid"),TEXT("Each key must be an object with numeric 'frame' and 'value'"));break;}
            const auto Input=Value->AsObject();OriginalRow.Frame=Input->GetNumberField(TEXT("frame"));const double Number=Input->GetNumberField(TEXT("value"));
            if(!Tick(OriginalRow.Frame,C,OriginalRow.Time)||!FMath::IsFinite(Number)||FMath::Abs(Number)>static_cast<double>(MAX_flt))
            {OriginalKeyFailure(R,I,TEXT("frame/value"),INDEX_NONE,Value,TEXT("sequencer_raw_float_storage_pending"),TEXT("Original key exceeds finite time/float32 storage profile; previous rows remain, no later writer/save."));break;}
            OriginalRow.Value=static_cast<float>(Number);
            if(Actual->GetTimes().Num()>=MaxKeys){OriginalKeyFailure(R,I,TEXT("value"),INDEX_NONE,Value,TEXT("sequencer_channel_key_budget_exceeded"),TEXT("Finite native key bound reached after retained row prefix; no replay/save."));break;}
        }
        FScalarKey& Key=OriginalRaw?OriginalRow:Rows[I];
        auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("row_index"), I); Row->SetNumberField(TEXT("display_frame"), Key.Frame); Row->SetNumberField(TEXT("tick"), Key.Time.Value); Row->SetNumberField(TEXT("value"), Key.Value);
        const int32 ExpectedIndex = Expected.AddCubicKey(Key.Time, Key.Value, RCTM_Auto); R.Data->SetNumberField(TEXT("cubic_key_attempts"), I + 1); const int32 Index = Actual->AddCubicKey(Key.Time, Key.Value, RCTM_Auto); Row->SetNumberField(TEXT("insert_index"), Index); Insertions.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("insertions"), Insertions); R.Data->SetNumberField(TEXT("keys_added"), I + 1); R.Data->SetNumberField(TEXT("scalar_keys_added"), I + 1);
        auto InsertionProof = MakeShared<FJsonObject>(), Wanted = MakeShared<FJsonObject>(); const bool Exact = Index == ExpectedIndex && Channel(Actual, TEXT("Value"), InsertionProof) && Channel(&Expected, TEXT("Value"), Wanted) && Same(InsertionProof, Wanted); Row->SetBoolField(TEXT("public_copy_insert_verified"), Exact);
        if (!Exact) { Fail(R, TEXT("sequencer_cubic_insert_postcondition_failed"), TEXT("Actual scalar duplicate insertion/order/Cubic/Auto tangent differs from public channel-copy result; known inserted prefix retained.")); break; }
    }
    if(!OriginalRaw||R.Code.IsEmpty()){R.Data->SetNumberField(TEXT("mark_package_dirty_calls"),1);C.Sequence->MarkPackageDirty();if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);} auto After = MakeShared<FJsonObject>(); if (!CaptureAfter(C, R, After)) { return Encode(R); }if(OriginalRaw&&!OriginalKeyCurrent(C,R))return Encode(R);
    const auto CurrentTracks = C.Scene->FindTracks(UMovieSceneFloatTrack::StaticClass(), C.Binding); TArray<TSharedPtr<FJsonValue>> TrackPaths, SectionPaths;
    for (UMovieSceneTrack* BoundTrack : CurrentTracks) { if (Loaded(BoundTrack)) { TrackPaths.Add(MakeShared<FJsonValueString>(BoundTrack->GetPathName())); } else { TrackPaths.Add(MakeShared<FJsonValueNull>()); } }
    bool Attached = Loaded(C.Track) && C.Track->GetClass() == UMovieSceneFloatTrack::StaticClass() && C.Track->GetOuter() == C.Scene && CurrentTracks.Contains(C.Track);
    if (Loaded(C.Track))
    {
        const auto& CurrentSections = C.Track->GetAllSections();
        for (UMovieSceneSection* TrackSection : CurrentSections) { if (Loaded(TrackSection)) { SectionPaths.Add(MakeShared<FJsonValueString>(TrackSection->GetPathName())); } else { SectionPaths.Add(MakeShared<FJsonValueNull>()); } }
        Attached = Attached && !CurrentSections.IsEmpty() && CurrentSections[0] == C.Section;
    }
    Attached = Attached && Loaded(C.Section) && C.Section->GetClass() == UMovieSceneFloatSection::StaticClass() && C.Section->GetOuter() == C.Track;
    R.Data->SetArrayField(TEXT("current_selected_binding_float_track_paths"), TrackPaths); R.Data->SetArrayField(TEXT("current_selected_track_section_paths"), SectionPaths); R.Data->SetBoolField(TEXT("selected_track_section_current"), Attached);
    const bool Preserved = Attached && KeyStructurePreserved(C, After, NewTrack, NewSection); R.Data->SetBoolField(TEXT("nonselected_state_preserved"), Preserved);
    auto Final = MakeShared<FJsonObject>(), Wanted = MakeShared<FJsonObject>(); FMovieSceneFloatChannel* Current = nullptr; const bool Typed = Attached && FloatChannel(C.Section, Current, Final); R.Data->SetObjectField(TEXT("channel_after_insertion"), Final);
    auto ActualData = MakeShared<FJsonObject>(); const bool Exact = Typed && Channel(Current, TEXT("Value"), ActualData) && Channel(&Expected, TEXT("Value"), Wanted) && Same(ActualData, Wanted);
    const bool Property = Attached && static_cast<UMovieSceneFloatTrack*>(C.Track)->GetPropertyPath().ToString() == PropertyPath && static_cast<UMovieSceneFloatTrack*>(C.Track)->GetPropertyName() == FName(*PropertyName);
    R.Data->SetBoolField(TEXT("native_cubic_channel_verified"), Exact); R.Data->SetBoolField(TEXT("property_binding_metadata_verified"), Property); R.Data->SetBoolField(TEXT("duplicate_tick_insertion_supported"), true); R.Data->SetNumberField(TEXT("requested_key_rows"), RequestedRows); R.Data->SetNumberField(TEXT("cubic_interpolation_value"), static_cast<uint8>(RCIM_Cubic)); R.Data->SetNumberField(TEXT("auto_tangent_mode_value"), static_cast<uint8>(RCTM_Auto));
    R.bPost = R.Code.IsEmpty() && Preserved && Exact && Property && (!NewSection || C.Section->GetRange() == TRange<FFrameNumber>::All());
    if (R.Code.IsEmpty() && !R.bPost) { Fail(R, TEXT("sequencer_float_keys_postcondition_failed"), TEXT("Actual scalar curve/property metadata or nonselected bindings/tracks/sections differ; no compile/save/rollback/retry.")); }
    if (R.bPost) { R.bSuccess = true; R.Stage = TEXT("complete"); } return Encode(R);
}

FString SetPlaybackRange(const FString& SequencePath, double PlaybackStartFrame, double PlaybackEndFrame, bool bSave)
{
    FResult R; InitMaintenance(R, TEXT("set_level_sequence_playback_range"), bSave); FContext C; FFrameNumber Start, End;
    if (!Resolve(SequencePath, C, R,true) || !MaintenanceReady(C, R)) { return Encode(R); }
    if (!(PlaybackEndFrame > PlaybackStartFrame) || !MaintenanceRequestedTimeSafe(PlaybackStartFrame, C) || !MaintenanceRequestedTimeSafe(PlaybackEndFrame, C)
        || !Tick(PlaybackStartFrame, C, Start) || !Tick(PlaybackEndFrame, C, End) || End < Start
        || !MaintenanceTimeSafe(FFrameTime(Start), C.Scene->GetTickResolution(), C.Scene->GetDisplayRate()) || !MaintenanceTimeSafe(FFrameTime(End), C.Scene->GetTickResolution(), C.Scene->GetDisplayRate()))
    { Fail(R, TEXT("sequencer_playback_range_invalid"), TEXT("Finite end must exceed start; both native conversion products and forward/floored reverse results must fit their int64/int32 budgets.")); return Encode(R); }
    R.Data->SetStringField(TEXT("sequence"), C.Sequence->GetPathName()); R.Data->SetNumberField(TEXT("playback_start_frame"), PlaybackStartFrame); R.Data->SetNumberField(TEXT("playback_end_frame"), PlaybackEndFrame); R.Data->SetNumberField(TEXT("start_tick"), Start.Value); R.Data->SetNumberField(TEXT("end_tick"), End.Value);
    if (!MaintenanceCapture(C, R, TEXT("before_observation"), C.Before)) { return Encode(R); }
    auto BeforeInfo = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("before_info"), BeforeInfo);
    if (!MaintenanceInfo(C, BeforeInfo)) { Fail(R, TEXT("sequencer_maintenance_info_unobservable"), TEXT("Current loaded decimal Info projection is incomplete before mutation.")); return Encode(R); }
    const TRange<FFrameNumber> Expected(Start, End); FMovieSceneEditorData ExpectedEditor = C.Scene->GetEditorData();
    if (C.Scene->GetPlaybackRange() != Expected)
    {
        const double A = Start / C.Scene->GetTickResolution(), B = End / C.Scene->GetTickResolution(), Padding = (B - A) * 0.1;
        if (ExpectedEditor.WorkStart >= ExpectedEditor.WorkEnd) { ExpectedEditor.WorkStart = A - Padding; ExpectedEditor.WorkEnd = B + Padding; }
        if (ExpectedEditor.ViewStart >= ExpectedEditor.ViewEnd) { ExpectedEditor.ViewStart = A - Padding; ExpectedEditor.ViewEnd = B + Padding; }
    }
    auto ExpectedEditorFields = MakeShared<FJsonObject>();
    if (!Properties(FMovieSceneEditorData::StaticStruct(), &ExpectedEditor, C.Scene, {}, ExpectedEditorFields)) { Fail(R, TEXT("sequencer_editor_data_unobservable"), TEXT("Expected Editor view/work range cannot be exported before mutation.")); return Encode(R); }
    R.Stage = TEXT("mutate"); R.Data->SetNumberField(TEXT("outer_transaction_calls"), 1); FScopedTransaction Transaction(NSLOCTEXT("CubelessSequencer", "SetPlaybackRange", "Set Level Sequence Playback Range"));
    R.bAttempted = true; R.Data->SetNumberField(TEXT("explicit_modify_calls"), 1); C.Sequence->Modify(); R.Data->SetNumberField(TEXT("explicit_modify_calls"), 2); C.Scene->Modify();
    R.Data->SetNumberField(TEXT("set_playback_range_calls"), 1); C.Scene->SetPlaybackRange(Expected); R.Data->SetNumberField(TEXT("mark_package_dirty_calls"), 1); C.Sequence->MarkPackageDirty();
    auto After = MakeShared<FJsonObject>(); if (!MaintenanceCapture(C, R, TEXT("after_observation"), After)) { return Encode(R); }
    const bool Preserved = CommonPreserved(C, After, true) && SameField(C.Before, After, TEXT("bindings")) && SameField(C.Before, After, TEXT("tracks")) && SameField(C.Before, After, TEXT("disk")) && SameField(C.Before, After, TEXT("other_package_dirty"));
    const bool EditorOK = Same(ExpectedEditorFields, After->GetObjectField(TEXT("editor_data")).ToSharedRef()); const bool FloorOK = C.Scene->GetPlaybackRange() == Expected;
    R.Data->SetBoolField(TEXT("nonselected_state_preserved"), Preserved); R.Data->SetBoolField(TEXT("playback_editor_ranges_verified"), EditorOK); R.Data->SetBoolField(TEXT("playback_floor_verified"), FloorOK);
    R.bChanged = !SameField(C.Before, After, TEXT("playback_range")); R.Data->SetBoolField(TEXT("selected_actual_changed"), R.bChanged);
    if (!Preserved || !EditorOK || !FloorOK) { Fail(R, TEXT("sequencer_playback_postcondition_failed"), TEXT("Actual floor range or unrelated state differs; retained prefix is returned and save is suppressed.")); return Encode(R); }
    auto Final = After; const bool SaveOK = MaintenanceSave(C, R, After, Final);
    if (!Final->GetBoolField(TEXT("complete"))) { return Encode(R); }
    auto AfterInfo = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("after_info"), AfterInfo);
    if (!MaintenanceInfo(C, AfterInfo)) { R.bKnown = false; Fail(R, TEXT("sequencer_maintenance_info_unobservable"), TEXT("Terminal decimal Info projection is incomplete after the retained mutation/save prefix.")); return Encode(R); }
    const bool StartMatches = FMath::IsNearlyEqual(AfterInfo->GetNumberField(TEXT("playback_start_frame")), PlaybackStartFrame);
    const bool EndMatches = FMath::IsNearlyEqual(AfterInfo->GetNumberField(TEXT("playback_end_frame")), PlaybackEndFrame); const bool SaveGate = !bSave || R.bSaved;
    const bool LegacyChanged = !FMath::IsNearlyEqual(BeforeInfo->GetNumberField(TEXT("playback_start_frame")), PlaybackStartFrame) || !FMath::IsNearlyEqual(BeforeInfo->GetNumberField(TEXT("playback_end_frame")), PlaybackEndFrame);
    const bool Specific = StartMatches && EndMatches && SaveGate;
    auto Checks = MakeShared<FJsonObject>(); Checks->SetBoolField(TEXT("playback_start_matches"), StartMatches); Checks->SetBoolField(TEXT("playback_end_matches"), EndMatches); Checks->SetBoolField(TEXT("save_gate"), SaveGate); R.Data->SetObjectField(TEXT("verification_predicates"), Checks);
    if (SaveOK || !R.bSaved) { MaintenanceLegacy(R, BeforeInfo, AfterInfo, MaintenanceDelta(LegacyChanged ? 1 : 0, LegacyChanged ? 1 : 0, 0, LegacyChanged ? 0 : 1, Specific ? 0 : 1), LegacyChanged, Specific, TEXT("level_sequence_info_readback"), { TEXT("playback_start_matches"), TEXT("playback_end_matches"), TEXT("save_gate") }); }
    R.bPost = SaveOK && Specific;
    if (!R.bPost) { if (R.Code.IsEmpty()) { Fail(R, TEXT("mutation_postcondition_failed"), TEXT("Original strict display-frame readback or save gate failed after actual floor mutation/save. Requested values are not normalized to ticks.")); } return Encode(R); }
    R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}


namespace
{
// PP: fixed readonly Child.SubTrack[0].SubSection[0] -> Leaf role.
struct FNestedSubsequenceRole
{
    TStrongObjectPtr<UMovieSceneSubTrack> Track;TStrongObjectPtr<UMovieSceneSubSection> Section;
    TStrongObjectPtr<ULevelSequence> Leaf;TStrongObjectPtr<UMovieScene> LeafScene;TUniquePtr<FContext> SourceContext;
    FObjectProperty* Property=nullptr;int32 Offset=0;TRange<FFrameNumber> Bounds;int32 Row=0;FMovieSceneTimecodeSource Timecode;double PlayRate=1;
    bool Binding(UMovieSceneSubSection* S,ULevelSequence*& Out) const
    {
        if(!Loaded(S)||S->GetClass()!=UMovieSceneSubSection::StaticClass())return false;
        // Membership before retained FProperty reads; the reflected TObjectPtr is copied without resolving.
        if(FindFProperty<FObjectProperty>(UMovieSceneSubSection::StaticClass(),TEXT("SubSequence"))!=Property)return false;
        if(!Property||Property->GetOwnerStruct()!=UMovieSceneSubSection::StaticClass()||Property->HasGetter()||Property->ArrayDim!=1||Property->GetSize()!=sizeof(TObjectPtr<UMovieSceneSequence>)||Property->GetOffset_ForInternal()!=Offset||Property->PropertyClass!=UMovieSceneSequence::StaticClass())return false;
        const TObjectPtr<UObject> Slot=Property->GetObjectPtrPropertyValue(Property->ContainerPtrToValuePtr<void>(S));
        if(!Slot.IsResolved())return false;UObject* Value=Slot.Get();
        if(!Loaded(Value)||Value->GetClass()!=ULevelSequence::StaticClass())return false;Out=static_cast<ULevelSequence*>(Value);return true;
    }
    bool Current(FContext& Child) const
    {
        if(!Loaded(Track.Get())||!Loaded(Section.Get())||!Loaded(Leaf.Get())||!Loaded(LeafScene.Get())||Track->GetOuter()!=Child.Scene||!Child.Scene->GetTracks().Contains(Track.Get())||Track->GetAllSections().Num()!=1||Track->GetAllSections()[0]!=Section.Get()||Section->GetOuter()!=Track.Get())return false;
        ULevelSequence* Actual=nullptr;if(!Binding(Section.Get(),Actual)||Actual!=Leaf.Get()||Leaf->GetMovieScene()!=LeafScene.Get()||LeafScene->GetOuter()!=Leaf.Get())return false;
        return Section->GetRange()==Bounds&&Section->GetRowIndex()==Row&&Section->TimecodeSource==Timecode&&Section->Parameters.TimeScale.GetType()==EMovieSceneTimeWarpType::FixedPlayRate&&Section->Parameters.TimeScale.AsFixedPlayRate()==PlayRate;
    }
};
// JL: exact CameraCut preservation around the existing two finite maintenance
// operations. SDK bodies remain opaque; no intermediate callback atomicity claim.
struct FCameraMaintenanceScope
{
    struct FHeld{TStrongObjectPtr<UObject> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;bool Dirty=false;UClass* SourceClass=nullptr;EClassFlags ClassFlags=CLASS_None;};
    struct FBindingRole{FGuid Guid;TArray<UMovieSceneTrack*> Tracks;};
    struct FSceneRole{FContext* Context=nullptr;UMovieSceneCameraCutTrack* Cut=nullptr;TArray<UMovieSceneTrack*> Roots;TArray<FBindingRole> Bindings;TRange<FFrameNumber> Playback;FFrameRate Display,Resolution;bool ReadOnly=false;FString CutPath;TSharedPtr<FJsonObject> CutFields;};
    struct FTrackRole{UMovieSceneTrack* Track=nullptr;TArray<UMovieSceneSection*> Sections;};
    struct FCutRole{UMovieSceneCameraCutTrack* Track=nullptr;UMovieSceneCameraCutSection* Section=nullptr;TRange<FFrameNumber> Range;FMovieSceneObjectBindingID Binding;};
    struct FClipRole{UMovieSceneSection* Section=nullptr;int32 Kind=0;UObject* Source=nullptr;};
    struct FClipSource{UObject* Asset=nullptr;TSharedPtr<FJsonObject> Before;};
    FContext& Parent;FContext* Child;FResult& R;TWeakObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;FString WorldPath;UPackage* TargetPackage=nullptr;
    TArray<FHeld> Held;TArray<FSceneRole> Scenes;TArray<FTrackRole> Tracks;TArray<FCutRole> Cuts;bool Stopped=false,Lease=false;
    TArray<FClipRole> Clips;TArray<FClipSource> ClipSources;TSharedPtr<FJsonObject> ClipChannelsBefore;bool ClipsSealed=false,ObservingClips=false;
    bool ClipPlain(const TCHAR* Stage)
    {
        for(const auto& Clip:Clips)
        {
            TObjectPtr<UObject> Actual;
            // ObjectPtr.h shallow equality on resolved handles avoids a tracked read in Check's final pure fence.
            if(!Loaded(Clip.Section)||!SubsequenceClipSlot(Clip.Section,Clip.Kind,Actual)||Actual!=Clip.Source)return Stop(Stage);
        }
        return true;
    }
    bool BindClip(UMovieSceneSection* Section,int32 Kind)
    {
        const TCHAR* Stage=TEXT("subsequence_clip_source_bind");UObject* Asset=nullptr;
        // ADJ acquires a source hold from resolved slot metadata before any tracked read.
        TObjectPtr<UObject> Slot;
        const bool ReferenceRead=Parent.bRemovalClips?SubsequenceClipSlot(Section,Kind,Slot):SubsequenceClipReference(Section,Kind,Asset);
        if(Parent.bRemovalClips&&ReferenceRead&&Slot!=nullptr)Asset=StaticFindObject(nullptr,nullptr,*Slot.GetPathName(),false);
        TStrongObjectPtr<UObject> ReturnedAsset(Asset);
        if(!ReferenceRead||!Context(Stage))return Stop(Stage);
        if(Parent.bRemovalClips){TObjectPtr<UObject> Actual;if(!SubsequenceClipSlot(Section,Kind,Actual)||!Slot.IsResolved()||Slot.GetHandle()!=Actual.GetHandle()||(Slot!=nullptr&&(!Loaded(Asset)||TObjectPtr<UObject>(Asset).GetHandle()!=Slot.GetHandle())))return Stop(Stage);}
        FClipRole Clip;Clip.Section=Section;Clip.Kind=Kind;Clip.Source=Asset;
        if(Asset)
        {
            if(!Pin(Asset,Stage)||!Pin(Asset->GetClass(),Stage)||!Pin(Asset->GetPackage(),Stage))return false;
            UClass* Expected=Kind==1?UAnimSequence::StaticClass():USoundWave::StaticClass();
            const TCHAR* ExpectedPath=Kind==1?TEXT("/Script/Engine.AnimSequence"):TEXT("/Script/Engine.SoundWave");
            UPackage* Package=Asset->GetPackage();
            if(Asset->GetClass()!=Expected||!Native(Expected,ExpectedPath)||Asset->GetOuter()!=Package||Package==TargetPackage||!Package->IsFullyLoaded()||Package->IsDirty()||Package->HasAnyPackageFlags(PKG_PlayInEditor|PKG_EditorOnly|PKG_CompiledIn)||Asset->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||Asset->IsEditorOnly()||(!Package->GetName().StartsWith(TEXT("/Game/"))&&!Package->GetName().StartsWith(TEXT("/Engine/"))))return Stop(Stage);
            bool Found=false;for(const auto& Source:ClipSources)Found|=Source.Asset==Asset;
            if(!Found){FClipSource Source;Source.Asset=Asset;ClipSources.Add(MoveTemp(Source));}
        }
        Clips.Add(Clip);return ClipPlain(Stage);
    }
    bool ClipFiles(UObject* Asset,TSharedRef<FJsonObject>& Files)
    {
        FString Filename;if(!FPackageName::TryConvertLongPackageNameToFilename(Asset->GetPackage()->GetName(),Filename,FPackageName::GetAssetPackageExtension()))return false;
        Filename=FPaths::ConvertRelativePathToFull(Filename);
        // Retain package sidecar evidence as well as the header. No bulk/audio decode or stream request.
        for(const TCHAR* Extension:{TEXT("uasset"),TEXT("uexp"),TEXT("ubulk"),TEXT("uptnl")})
        {
            const FString Path=FPaths::ChangeExtension(Filename,Extension);const int64 Size=IFileManager::Get().FileSize(*Path);
            auto File=MakeShared<FJsonObject>();File->SetStringField(TEXT("path"),Path);File->SetBoolField(TEXT("exists"),Size>=0);File->SetNumberField(TEXT("bytes"),FMath::Max<int64>(Size,0));File->SetStringField(TEXT("sha1"),TEXT(""));Files->SetObjectField(Extension,File);
            if(Size<0){if(FCString::Strcmp(Extension,TEXT("uasset"))==0)return false;continue;}
            if((Size==0&&FCString::Strcmp(Extension,TEXT("uasset"))==0)||Size>64*1024*1024)return false;TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*Path)||Bytes.Num()!=Size)return false;
            uint8 Hash[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash);File->SetStringField(TEXT("sha1"),BytesToHex(Hash,FSHA1::DigestSize));
        }
        return true;
    }
    bool ObserveClips(const TCHAR* Stage)
    {
        if(Clips.IsEmpty()||ObservingClips)return true;
        TGuardValue<bool> Guard(ObservingClips,true);const TFunction<bool()> Fence=[this,Stage]{return Check(Stage,false,false);};
        auto ClipState=MakeShared<FJsonObject>();
        for(const auto& Clip:Clips)
        {
            auto Channels=MakeShared<FJsonObject>();
            if(!SubsequenceClipChannels(Clip.Section,Clip.Kind,Channels,Fence))return Stop(Stage);
            ClipState->SetObjectField(Clip.Section->GetPathName(),Channels);
        }
        R.Data->SetObjectField(Parent.bRemovalClips?TEXT("removal_readonly_clip_channels"):TEXT("subsequence_readonly_clip_channels"),ClipState);
        if(ClipChannelsBefore.IsValid()&&!(Parent.bRemovalClips?CameraLiteral(MakeShared<FJsonValueObject>(ClipChannelsBefore),MakeShared<FJsonValueObject>(ClipState),0):Same(ClipChannelsBefore.ToSharedRef(),ClipState)))return SiblingStop(Stage,(Parent.bRemovalClips?Parent.Sequence:Child->Sequence)->GetPathName(),TEXT("Readonly clip references/weight/volume/pitch channels changed."),ClipChannelsBefore,ClipState);
        if(!ClipChannelsBefore.IsValid())ClipChannelsBefore=ClipState;
        auto Sources=MakeShared<FJsonObject>();R.Data->SetObjectField(Parent.bRemovalClips?(ClipsSealed?TEXT("removal_clip_sources_after"):TEXT("removal_clip_sources_before")):(ClipsSealed?TEXT("subsequence_clip_sources_after"):TEXT("subsequence_clip_sources_before")),Sources);
        for(auto& Source:ClipSources)
        {
            if(!Fence())return false;auto Now=Identity(Source.Asset);auto Fields=MakeShared<FJsonObject>(),Files=MakeShared<FJsonObject>();
            Sources->SetObjectField(Source.Asset->GetPathName(),Now);Now->SetObjectField(TEXT("authored_fields"),Fields);Now->SetObjectField(TEXT("package_files"),Files);
            TSet<FName> Skip;
            for(TFieldIterator<FProperty> It(Source.Asset->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)if(It->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient))Skip.Add(It->GetFName());
            if(!ObjectProperties(Source.Asset,Skip,Fields,Fence)||!ClipFiles(Source.Asset,Files)||!Fence())return Stop(Stage);
            if(Source.Before.IsValid()&&!(Parent.bRemovalClips?CameraLiteral(MakeShared<FJsonValueObject>(Source.Before),MakeShared<FJsonValueObject>(Now),0):Same(Source.Before.ToSharedRef(),Now)))return SiblingStop(Stage,Source.Asset->GetPathName(),TEXT("Readonly clip source authored fields/package files changed; no playback, source write/save or retry."),Source.Before,Now);
            if(!Source.Before.IsValid())Source.Before=Now;
        }
        ClipsSealed=true;R.Data->SetBoolField(Parent.bRemovalClips?TEXT("removal_clip_sources_preserved"):TEXT("subsequence_clip_sources_preserved"),true);return true;
    }
    // Expected value copies predate the first reflected observer. Phase0: original;
    // phase1: possessable/binding removed, references unchanged; phase2: selected references removed.
    TArray<FMovieScenePossessable> LeafBefore,LeafRemaining;
    FUpgradedLevelSequenceBindingReferences LeafReferencesBefore,LeafReferencesAfter;
    TArray<TStrongObjectPtr<UClass>> LeafClasses;
    FArrayProperty* LeafPossessables=nullptr;FStructProperty* LeafReferences=nullptr;FStructProperty* LeafInner=nullptr;
    UScriptStruct* LeafPossessableStruct=nullptr;UScriptStruct* LeafReferenceStruct=nullptr;
    UScriptStruct* LeafActorStruct=nullptr;UScriptStruct* LeafSubStruct=nullptr;EStructFlags LeafActorFlags=STRUCT_NoFlags,LeafSubFlags=STRUCT_NoFlags;
    FSoftClassProperty* LeafClassField=nullptr;int32 LeafClassOffset=0;
    static bool LocatorFragments(const FUniversalObjectLocator& Locator,TArray<const FUniversalObjectLocatorFragment*>& Out)
    {
        return Locator.ForEachFragment([&Out](int32 Index,int32 Count,const FUniversalObjectLocatorFragment& Fragment){if(Index>=4||Count>4)return false;Out.Add(&Fragment);return true;});
    }
    bool LocatorCurrent(const FUniversalObjectLocator& Expected,const FUniversalObjectLocator& Actual,const TCHAR* Stage,int32 ReferenceIndex)
    {
        TArray<const FUniversalObjectLocatorFragment*> LeftFragments,RightFragments;
        const FString Prefix=FString::Printf(TEXT("references/%d/locator"),ReferenceIndex);
        if(!LocatorFragments(Expected,LeftFragments)||!LocatorFragments(Actual,RightFragments))return LeafBad(Stage,*(Prefix+TEXT("/fragment_budget")),TEXT("at most four native fragments"),TEXT("exceeded"));
        if(LeftFragments.Num()!=RightFragments.Num())return LeafBad(Stage,*(Prefix+TEXT("/fragment_count")),FString::FromInt(LeftFragments.Num()),FString::FromInt(RightFragments.Num()));
        for(int32 I=0;I<LeftFragments.Num();++I)
        {
            const auto& A=*LeftFragments[I];const auto& B=*RightFragments[I];
            const FString Cell=Prefix+TEXT("/fragment/")+FString::FromInt(I);
            if(A.GetFragmentTypeHandle()!=B.GetFragmentTypeHandle())
            {
                const FString ExpectedType=A.GetFragmentTypeHandle()==FActorLocatorFragment::FragmentType?TEXT("Actor"):TEXT("SubObject");
                const FString ActualType=B.GetFragmentTypeHandle()==FActorLocatorFragment::FragmentType?TEXT("Actor"):(B.GetFragmentTypeHandle()==FSubObjectLocator::FragmentType?TEXT("SubObject"):TEXT("other/invalid"));
                return LeafBad(Stage,*(Cell+TEXT("/type")),ExpectedType,ActualType);
            }
            if(B.GetFragmentTypeHandle()==FActorLocatorFragment::FragmentType)
            {
                if(A.GetFragmentStruct()!=LeafActorStruct||B.GetFragmentStruct()!=LeafActorStruct)return LeafBad(Stage,*(Cell+TEXT("/struct")),TEXT("held native ActorLocatorFragment"),TEXT("changed binding"));
                const auto* Left=A.GetPayloadAs(FActorLocatorFragment::FragmentType);const auto* Right=B.GetPayloadAs(FActorLocatorFragment::FragmentType);
                if(!Left||!Right)return LeafBad(Stage,*(Cell+TEXT("/payload")),TEXT("native Actor payload"),TEXT("null"));
                const FString ExpectedPath=Left->Path.ToString(),ActualPath=Right->Path.ToString();
                if(!ExpectedPath.Equals(ActualPath,ESearchCase::CaseSensitive))return LeafBad(Stage,*(Cell+TEXT("/actor_path")),ExpectedPath,ActualPath);
            }
            else if(B.GetFragmentTypeHandle()==FSubObjectLocator::FragmentType)
            {
                if(A.GetFragmentStruct()!=LeafSubStruct||B.GetFragmentStruct()!=LeafSubStruct)return LeafBad(Stage,*(Cell+TEXT("/struct")),TEXT("held native SubObjectLocator"),TEXT("changed binding"));
                const auto* Left=A.GetPayloadAs(FSubObjectLocator::FragmentType);const auto* Right=B.GetPayloadAs(FSubObjectLocator::FragmentType);
                if(!Left||!Right)return LeafBad(Stage,*(Cell+TEXT("/payload")),TEXT("native SubObject payload"),TEXT("null"));
                if(!Left->PathWithinContext.Equals(Right->PathWithinContext,ESearchCase::CaseSensitive))return LeafBad(Stage,*(Cell+TEXT("/path_within_context")),Left->PathWithinContext,Right->PathWithinContext);
            }
            else return LeafBad(Stage,*(Cell+TEXT("/type")),TEXT("Actor/SubObject"),TEXT("unsupported"));
        }
        return true;
    }
    int32 LeafPossessableOffset=0,LeafReferenceOffset=0;int32 LeafPhase=0;bool LeafReady=false,ObservingLeaf=false;
    TSharedPtr<FJsonObject> LeafPossessablesBeforeText,LeafPossessablesAfterText,LeafReferencesBeforeText,LeafReferencesAfterText;
    bool LeafBad(const TCHAR* Stage,const TCHAR* Property,const FString& Expected,const FString& Actual)
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Parent.Sequence->GetPathName());J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("component_removal_first_mismatch"),J);}
        return Stop(Stage);
    }
    bool LeafStorage(const TCHAR* Stage)
    {
        if(LeafReady&&(!Loaded(LeafActorStruct)||!Loaded(LeafSubStruct)||LeafActorStruct->StructFlags!=LeafActorFlags||LeafSubStruct->StructFlags!=LeafSubFlags
            ||LeafActorStruct->GetStructureSize()!=sizeof(FActorLocatorFragment)||LeafActorStruct->GetMinAlignment()!=alignof(FActorLocatorFragment)
            ||LeafSubStruct->GetStructureSize()!=sizeof(FSubObjectLocator)||LeafSubStruct->GetMinAlignment()!=alignof(FSubObjectLocator)))
            return LeafBad(Stage,TEXT("locator_native_struct_layout"),TEXT("captured Actor/SubObject native struct roles"),TEXT("changed"));
        if(FindFProperty<FArrayProperty>(UMovieScene::StaticClass(),TEXT("Possessables"))!=LeafPossessables||FindFProperty<FStructProperty>(ULevelSequence::StaticClass(),TEXT("BindingReferences"))!=LeafReferences)
            return LeafBad(Stage,TEXT("reflected_storage_binding"),TEXT("original direct native fields"),TEXT("changed"));
        if(LeafPossessables->GetOwnerClass()!=UMovieScene::StaticClass()||LeafPossessables->GetOffset_ForInternal()!=LeafPossessableOffset||LeafPossessables->ArrayDim!=1||LeafPossessables->Inner!=LeafInner||LeafInner->GetOwnerProperty()!=LeafPossessables
            ||!LeafInner->Struct.IsResolved()||(LeafReady&&LeafInner->Struct.Get()!=LeafPossessableStruct)||!Loaded(LeafInner->Struct.Get())||LeafInner->Struct->GetPathName()!=TEXT("/Script/MovieScene.MovieScenePossessable")||LeafInner->Struct->GetStructureSize()!=sizeof(FMovieScenePossessable)
            ||LeafReferences->GetOwnerClass()!=ULevelSequence::StaticClass()||LeafReferences->GetOffset_ForInternal()!=LeafReferenceOffset||LeafReferences->ArrayDim!=1||!LeafReferences->Struct.IsResolved()||(LeafReady&&LeafReferences->Struct.Get()!=LeafReferenceStruct)||!Loaded(LeafReferences->Struct.Get())
            ||LeafReferences->Struct->GetPathName()!=TEXT("/Script/LevelSequence.UpgradedLevelSequenceBindingReferences")||LeafReferences->Struct->GetStructureSize()!=sizeof(FUpgradedLevelSequenceBindingReferences))
            return LeafBad(Stage,TEXT("reflected_storage_layout"),TEXT("original native possessable/reference layout"),TEXT("changed"));
        if(LeafReady&&(FindFProperty<FSoftClassProperty>(LeafInner->Struct,TEXT("PossessedObjectClass"))!=LeafClassField||LeafClassField->GetOwnerStruct()!=LeafInner->Struct||LeafClassField->GetOffset_ForInternal()!=LeafClassOffset||LeafClassField->ArrayDim!=1||LeafClassField->ElementSize!=sizeof(FSoftObjectPtr)||!LeafClassField->MetaClass.IsResolved()||LeafClassField->MetaClass.Get()!=UObject::StaticClass()))
            return LeafBad(Stage,TEXT("possessable_class_field"),TEXT("original native soft class field"),TEXT("changed"));
        return true;
    }
    bool LeafPlain(const TCHAR* Stage)
    {
        if((!Parent.bLeafRemoval&&!Parent.bMixedRemoval&&!Parent.bHierarchyRemoval)||!LeafReady)return true;
        if(!LeafStorage(Stage))return false;
        const auto& Expected=LeafPhase?LeafRemaining:LeafBefore;
        if(Parent.Scene->GetPossessableCount()!=Expected.Num())return LeafBad(Stage,TEXT("possessable_count"),FString::FromInt(Expected.Num()),FString::FromInt(Parent.Scene->GetPossessableCount()));
        for(int32 I=0;I<Expected.Num();++I)
        {
            const auto& A=Expected[I];const auto& B=Parent.Scene->GetPossessable(I);
            const auto& Dynamic=B.DynamicBinding_DEPRECATED;
            if(!Dynamic.Function.IsResolved()||Dynamic.Function.Get()!=nullptr||!Dynamic.ResolveParamsProperty.IsPathToFieldEmpty()||Dynamic.PayloadVariables.Num()!=0||!Dynamic.CompiledFunctionName.IsNone()||!Dynamic.ResolveParamsPinName.IsNone()||!Dynamic.WeakEndpoint.IsExplicitlyNull())return LeafBad(Stage,TEXT("legacy_dynamic_binding"),TEXT("absent endpoint/payload"),TEXT("changed"));
            if(A.Tags.Num()!=B.Tags.Num())return LeafBad(Stage,TEXT("possessable_tags_count"),FString::FromInt(A.Tags.Num()),FString::FromInt(B.Tags.Num()));
            for(int32 Tag=0;Tag<A.Tags.Num();++Tag)if(!A.Tags[Tag].ToString().Equals(B.Tags[Tag].ToString(),ESearchCase::CaseSensitive))return LeafBad(Stage,TEXT("possessable_tag"),A.Tags[Tag].ToString(),B.Tags[Tag].ToString());
            const FString ExpectedClass=LeafClassField->GetPropertyValue_InContainer(&A).ToSoftObjectPath().ToString(),ActualClass=LeafClassField->GetPropertyValue_InContainer(&B).ToSoftObjectPath().ToString();
            if(!ExpectedClass.Equals(ActualClass,ESearchCase::CaseSensitive))return LeafBad(Stage,TEXT("possessable_class_path"),ExpectedClass,ActualClass);
            if(A.GetGuid()!=B.GetGuid()||A.GetParent()!=B.GetParent()||!A.GetName().Equals(B.GetName(),ESearchCase::CaseSensitive)||A.GetLoadedPossessedObjectClass()!=B.GetLoadedPossessedObjectClass()||A.GetSpawnableObjectBindingID()!=B.GetSpawnableObjectBindingID())
                return LeafBad(Stage,TEXT("possessable_identity_parent_name_class"),A.GetGuid().ToString()+TEXT("/")+A.GetParent().ToString()+TEXT("/")+A.GetName(),B.GetGuid().ToString()+TEXT("/")+B.GetParent().ToString()+TEXT("/")+B.GetName());
        }
        if(Parent.bHierarchyRemoval&&LeafPhase>0)R.Data->SetBoolField(TEXT("hierarchy_survivor_parent_guids_observed_equal_after_remove"),true);
        const auto* ActualReferences=Parent.Sequence->GetBindingReferences();if(!ActualReferences)return LeafBad(Stage,TEXT("binding_references"),TEXT("native map"),TEXT("null"));
        const auto ExpectedRefs=(LeafPhase==2?LeafReferencesAfter:LeafReferencesBefore).GetAllReferences();const auto ActualRefs=ActualReferences->GetAllReferences();
        if(ExpectedRefs.Num()!=ActualRefs.Num())return LeafBad(Stage,TEXT("reference_count"),FString::FromInt(ExpectedRefs.Num()),FString::FromInt(ActualRefs.Num()));
        for(int32 I=0;I<ExpectedRefs.Num();++I)if(ActualRefs[I].ID!=ExpectedRefs[I].ID||ActualRefs[I].ResolveFlags!=ExpectedRefs[I].ResolveFlags||!ActualRefs[I].CustomBinding.IsResolved()||ActualRefs[I].CustomBinding.Get()!=ExpectedRefs[I].CustomBinding.Get()||!LocatorCurrent(ExpectedRefs[I].Locator,ActualRefs[I].Locator,Stage,I))
            return LeafBad(Stage,TEXT("reference_guid_flags_custom_locator"),ExpectedRefs[I].ID.ToString(),ActualRefs[I].ID.ToString());
        return true;
    }
    bool LeafExport(FProperty* Field,const void* Value,UObject* Owner,TSharedRef<FJsonObject>& Out,const TCHAR* Stage)
    {
        const TFunction<bool()> Fence=[this,Stage]{return Check(Stage,false,false)&&LeafStorage(Stage);};
        if(!MaintenanceExportExpected(Field,Value,Owner,Out,Fence))return Stopped?false:LeafBad(Stage,TEXT("field_export"),TEXT("complete"),TEXT("incomplete"));
        return true;
    }
    bool BindLeaf(bool ExportNow=true)
    {
        if(!Parent.bLeafRemoval&&!Parent.bMixedRemoval&&!Parent.bHierarchyRemoval)return true;
        const TCHAR* Stage=TEXT("component_removal_bind");
        LeafActorStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/Engine.ActorLocatorFragment"));LeafSubStruct=FindObject<UScriptStruct>(nullptr,TEXT("/Script/UniversalObjectLocator.SubObjectLocator"));
        if(!Loaded(LeafActorStruct)||!Loaded(LeafSubStruct)||(LeafActorStruct->StructFlags&STRUCT_Native)==0||(LeafSubStruct->StructFlags&STRUCT_Native)==0
            ||!Pin(LeafActorStruct,Stage)||!Pin(LeafSubStruct,Stage))return LeafBad(Stage,TEXT("locator_native_sources"),TEXT("loaded native Actor/SubObject structs"),TEXT("missing"));
        LeafActorFlags=LeafActorStruct->StructFlags;LeafSubFlags=LeafSubStruct->StructFlags;
        LeafPossessables=FindFProperty<FArrayProperty>(UMovieScene::StaticClass(),TEXT("Possessables"));LeafReferences=FindFProperty<FStructProperty>(ULevelSequence::StaticClass(),TEXT("BindingReferences"));LeafInner=LeafPossessables?CastField<FStructProperty>(LeafPossessables->Inner):nullptr;
        if(!LeafPossessables||!LeafReferences||!LeafInner)return LeafBad(Stage,TEXT("storage"),TEXT("native fields"),TEXT("missing"));
        LeafPossessableOffset=LeafPossessables->GetOffset_ForInternal();LeafReferenceOffset=LeafReferences->GetOffset_ForInternal();
        if(!LeafStorage(Stage)||!Pin(LeafInner->Struct,Stage)||!Pin(LeafReferences->Struct,Stage))return false;
        LeafPossessableStruct=LeafInner->Struct.Get();LeafReferenceStruct=LeafReferences->Struct.Get();
        for(int32 I=0;I<Parent.Scene->GetPossessableCount();++I){const auto P=Parent.Scene->GetPossessable(I);LeafBefore.Add(P);if(P.GetGuid()!=Parent.Binding)LeafRemaining.Add(P);if(const UClass* K=P.GetLoadedPossessedObjectClass()){if(!Pin(const_cast<UClass*>(K),Stage))return false;LeafClasses.Emplace(const_cast<UClass*>(K));}}
        const auto* Refs=Parent.Sequence->GetBindingReferences();if(!Refs||Refs->GetAllReferences().Num()>MaxBindings*16)return LeafBad(Stage,TEXT("references"),TEXT("bounded map"),TEXT("missing/oversize"));
        static_cast<FMovieSceneBindingReferences&>(LeafReferencesBefore)=*Refs;static_cast<FMovieSceneBindingReferences&>(LeafReferencesAfter)=*Refs;LeafReferencesAfter.RemoveBinding(Parent.Binding);
        LeafClassField=FindFProperty<FSoftClassProperty>(LeafInner->Struct,TEXT("PossessedObjectClass"));if(!LeafClassField)return LeafBad(Stage,TEXT("possessable_class_field"),TEXT("native soft class"),TEXT("missing"));LeafClassOffset=LeafClassField->GetOffset_ForInternal();
        LeafReady=true;if(!Check(Stage,false,false))return false;
        return !ExportNow||SealLeaf();
    }
    bool SealLeaf()
    {
        const TCHAR* Stage=TEXT("removal_expected_export");
        auto P0=MakeShared<FJsonObject>(),P1=MakeShared<FJsonObject>(),R0=MakeShared<FJsonObject>(),R1=MakeShared<FJsonObject>();
        if(!LeafExport(LeafPossessables,&LeafBefore,Parent.Scene,P0,Stage)||!LeafExport(LeafPossessables,&LeafRemaining,Parent.Scene,P1,Stage)||!LeafExport(LeafReferences,&LeafReferencesBefore,Parent.Sequence,R0,Stage)||!LeafExport(LeafReferences,&LeafReferencesAfter,Parent.Sequence,R1,Stage))return false;
        LeafPossessablesBeforeText=P0;LeafPossessablesAfterText=P1;LeafReferencesBeforeText=R0;LeafReferencesAfterText=R1;
        R.Data->SetObjectField(TEXT("component_removal_original_possessables"),P0);R.Data->SetObjectField(TEXT("component_removal_original_references"),R0);
        return true;
    }
    bool ObserveLeaf(const TCHAR* Stage)
    {
        if((!Parent.bLeafRemoval&&!Parent.bMixedRemoval&&!Parent.bHierarchyRemoval)||!LeafReady||ObservingLeaf)return true;
        TGuardValue<bool> Guard(ObservingLeaf,true);
        auto P=MakeShared<FJsonObject>(),Refs=MakeShared<FJsonObject>();
        // Copy first: no live TArray element reference is retained across ExportText callbacks.
        TArray<FMovieScenePossessable> Values;for(int32 I=0;I<Parent.Scene->GetPossessableCount();++I)Values.Add(Parent.Scene->GetPossessable(I));
        FUpgradedLevelSequenceBindingReferences ReferenceValues;static_cast<FMovieSceneBindingReferences&>(ReferenceValues)=*Parent.Sequence->GetBindingReferences();
        if(!LeafExport(LeafPossessables,&Values,Parent.Scene,P,Stage)||!LeafExport(LeafReferences,&ReferenceValues,Parent.Sequence,Refs,Stage))return false;
        const auto& ExpectedP=LeafPhase?LeafPossessablesAfterText:LeafPossessablesBeforeText;const auto& ExpectedR=LeafPhase==2?LeafReferencesAfterText:LeafReferencesBeforeText;
        if(!CameraLiteral(MakeShared<FJsonValueObject>(ExpectedP),MakeShared<FJsonValueObject>(P),0)||!CameraLiteral(MakeShared<FJsonValueObject>(ExpectedR),MakeShared<FJsonValueObject>(Refs),0))
        {
            auto J=MakeShared<FJsonObject>();J->SetObjectField(TEXT("expected_possessables"),ExpectedP);J->SetObjectField(TEXT("actual_possessables"),P);J->SetObjectField(TEXT("expected_references"),ExpectedR);J->SetObjectField(TEXT("actual_references"),Refs);R.Data->SetObjectField(TEXT("component_removal_declaration_mismatch"),J);
            return LeafBad(Stage,TEXT("possessable_or_locator_payload"),TEXT("copied phase expectation"),TEXT("export differs"));
        }
        return true;
    }
    bool Unbound()
    {
        if(Parent.bLeafRemoval||Parent.bMixedRemoval||Parent.bHierarchyRemoval)LeafPhase=2;
        return Check(TEXT("component_removal_unbind_return"));
    }
    // UA: collateral native scene objects only. The selected binding remains an
    // ordinary parentless possessable; no spawn/register/evaluation API is used.
    struct FMixedObject{UObject* Object=nullptr;TSharedPtr<FJsonObject> Fields;};
    struct FMixedTemplate{UMovieSceneSpawnableActorBinding* Binding=nullptr;AActor* Actor=nullptr;};
    struct FMixedFolder{UMovieSceneFolder* Folder=nullptr;FName Name;FColor Color;int32 Sort=0;TArray<UMovieSceneTrack*> Tracks;TArray<FGuid> Bindings;};
    TArray<FMixedObject> MixedObjects;TArray<FMixedTemplate> MixedTemplates;TArray<FMixedFolder> MixedFolders;
    TArray<UMovieSceneFolder*> MixedRoots;TArray<FMovieSceneSpawnable> MixedSpawnables;TMap<FName,FMovieSceneObjectBindingIDs> MixedTags;
    TMap<FGuid,UMovieSceneSpawnableActorBinding*> MixedBindings;FObjectProperty* MixedActorTemplate=nullptr;FObjectProperty* MixedLegacyTemplate=nullptr;
    FArrayProperty* MixedSpawnableArray=nullptr;FStructProperty* MixedSpawnableInner=nullptr;UScriptStruct* MixedSpawnableStruct=nullptr;
    int32 MixedActorOffset=0,MixedLegacyOffset=0,MixedArrayOffset=0;uint64 MixedActorFlags=0,MixedLegacyFlags=0,MixedArrayFlags=0,MixedInnerFlags=0;
    TSharedPtr<FJsonObject> MixedSpawnableFields;bool MixedReady=false,MixedObserving=false,MixedCaptured=false;
    bool MixedBad(const TCHAR* Stage,const FString& Object,const TCHAR* Field,const FString& Expected,const FString& Actual)
    {
        if(!Stopped){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("mixed_scene_first_mismatch"),J);}
        return Stop(Stage);
    }
    bool MixedSlot(FObjectProperty* Field,UClass* Owner,UClass* ValueClass,const TCHAR* Name) const
    {
        return Field&&FindFProperty<FObjectProperty>(Owner,Name)==Field&&Field->GetClass()==FObjectProperty::StaticClass()&&Field->GetOwnerClass()==Owner&&Field->ArrayDim==1&&Field->ElementSize==sizeof(TObjectPtr<UObject>)
            &&!Field->HasSetterOrGetter()&&Field->PropertyClass.IsResolved()&&Field->PropertyClass.Get()==ValueClass;
    }
    bool MixedStorage(const TCHAR* Stage)
    {
        if(!MixedSlot(MixedActorTemplate,UMovieSceneSpawnableActorBinding::StaticClass(),AActor::StaticClass(),TEXT("ActorTemplate")))return MixedBad(Stage,TEXT("native SpawnableActorBinding"),TEXT("ActorTemplate descriptor"),TEXT("captured native TObjectPtr<Actor>"),TEXT("changed"));
        if(FindFProperty<FArrayProperty>(UMovieScene::StaticClass(),TEXT("Spawnables"))!=MixedSpawnableArray||MixedSpawnableArray->GetClass()!=FArrayProperty::StaticClass()||MixedSpawnableArray->GetOwnerClass()!=UMovieScene::StaticClass()||MixedSpawnableArray->Inner!=MixedSpawnableInner
            ||MixedSpawnableInner->GetClass()!=FStructProperty::StaticClass()||MixedSpawnableInner->GetOwnerProperty()!=MixedSpawnableArray||!MixedSpawnableInner->Struct.IsResolved()||MixedSpawnableInner->Struct.Get()!=MixedSpawnableStruct||!Loaded(MixedSpawnableStruct)||MixedSpawnableStruct!=FMovieSceneSpawnable::StaticStruct()||MixedSpawnableStruct->GetStructureSize()!=sizeof(FMovieSceneSpawnable)
            ||!MixedLegacyTemplate||FindFProperty<FObjectProperty>(MixedSpawnableStruct,TEXT("ObjectTemplate"))!=MixedLegacyTemplate||MixedLegacyTemplate->GetClass()!=FObjectProperty::StaticClass()||MixedLegacyTemplate->GetOwnerStruct()!=MixedSpawnableStruct||MixedLegacyTemplate->ArrayDim!=1||MixedLegacyTemplate->ElementSize!=sizeof(TObjectPtr<UObject>)||MixedLegacyTemplate->HasSetterOrGetter()||!MixedLegacyTemplate->PropertyClass.IsResolved()||MixedLegacyTemplate->PropertyClass.Get()!=UObject::StaticClass())return MixedBad(Stage,TEXT("MovieScene"),TEXT("Spawnables descriptor"),TEXT("captured direct native layout"),TEXT("changed"));
        if(MixedReady&&(MixedActorTemplate->GetOffset_ForInternal()!=MixedActorOffset||MixedLegacyTemplate->GetOffset_ForInternal()!=MixedLegacyOffset||MixedSpawnableArray->GetOffset_ForInternal()!=MixedArrayOffset||uint64(MixedActorTemplate->PropertyFlags)!=MixedActorFlags||uint64(MixedLegacyTemplate->PropertyFlags)!=MixedLegacyFlags||uint64(MixedSpawnableArray->PropertyFlags)!=MixedArrayFlags||uint64(MixedSpawnableInner->PropertyFlags)!=MixedInnerFlags))return MixedBad(Stage,TEXT("native fields"),TEXT("mixed layout flags/offset"),TEXT("captured"),TEXT("changed"));
        return true;
    }
    bool MixedPlain(const TCHAR* Stage)
    {
        if(!Parent.bMixedRemoval||!MixedReady)return true;
        if(!MixedStorage(Stage))return false;auto* Scene=Parent.Scene;
        const auto Roots=Scene->GetRootFolders();if(Roots.Num()!=MixedRoots.Num())return MixedBad(Stage,Scene->GetPathName(),TEXT("root_folders"),LexToString(MixedRoots.Num()),LexToString(Roots.Num()));
        for(int32 I=0;I<Roots.Num();++I)if(Roots[I]!=MixedRoots[I])return MixedBad(Stage,Scene->GetPathName(),TEXT("root_folder_slot"),LexToString(I),TEXT("changed identity"));
        for(const auto& F:MixedFolders)
        {
            // Held identities were checked by Check before these native typed reads.
            auto* O=F.Folder;if(!O->GetChildFolders().IsEmpty()||O->GetChildTracks()!=F.Tracks||O->GetChildObjectBindings()!=F.Bindings||!O->GetFolderName().ToString().Equals(F.Name.ToString(),ESearchCase::CaseSensitive)||O->GetFolderColor()!=F.Color||O->GetSortingOrder()!=F.Sort||O->GetDecorations().Num()!=0)return MixedBad(Stage,O->GetPathName(),TEXT("folder_contents"),TEXT("captured native leaf-folder values"),TEXT("changed"));
        }
        const auto& Tags=Scene->AllTaggedBindings();if(Tags.Num()!=MixedTags.Num())return MixedBad(Stage,Scene->GetPathName(),TEXT("binding_tag_count"),LexToString(MixedTags.Num()),LexToString(Tags.Num()));
        for(const auto& Group:Tags){const auto* E=MixedTags.Find(Group.Key);bool NameSame=false;for(const auto& Prior:MixedTags)if(Prior.Key==Group.Key)NameSame=Prior.Key.ToString().Equals(Group.Key.ToString(),ESearchCase::CaseSensitive);if(!E||!NameSame||E->IDs!=Group.Value.IDs)return MixedBad(Stage,Scene->GetPathName(),TEXT("binding_tag_group"),Group.Key.ToString(),TEXT("missing/name changed/ordered IDs changed"));}
        if(Scene->GetSpawnableCount()!=MixedSpawnables.Num())return MixedBad(Stage,Scene->GetPathName(),TEXT("legacy_spawnable_count"),LexToString(MixedSpawnables.Num()),LexToString(Scene->GetSpawnableCount()));
        for(int32 I=0;I<MixedSpawnables.Num();++I)
        {
            const auto& E=MixedSpawnables[I];const auto& A=Scene->GetSpawnable(I);const auto ES=MixedLegacyTemplate->GetObjectPtrPropertyValue_InContainer(&E),AS=MixedLegacyTemplate->GetObjectPtrPropertyValue_InContainer(&A);
            if(!ES.IsResolved()||!AS.IsResolved()||AS.Get()!=ES.Get()||A.GetGuid()!=E.GetGuid()||!A.GetName().Equals(E.GetName(),ESearchCase::CaseSensitive)||A.GetChildPossessables()!=E.GetChildPossessables()||A.GetSpawnOwnership()!=E.GetSpawnOwnership()||A.GetLevelName()!=E.GetLevelName())return MixedBad(Stage,Scene->GetPathName(),TEXT("legacy_spawnable_role"),E.GetGuid().ToString(),TEXT("changed"));
        }
        for(const auto& T:MixedTemplates)
        {
            const auto Slot=MixedActorTemplate->GetObjectPtrPropertyValue_InContainer(T.Binding);
            if(!Slot.IsResolved()||Slot.Get()!=T.Actor)return MixedBad(Stage,T.Binding->GetPathName(),TEXT("ActorTemplate slot"),TEXT("captured loaded native Actor"),TEXT("changed/unresolved"));
        }
        for(const auto& O:MixedObjects)if(O.Object->GetClass()==AActor::StaticClass())
        {
            auto* Actor=static_cast<AActor*>(O.Object);if(!Actor->GetComponents().IsEmpty()||Actor->GetRootComponent()!=nullptr)return MixedBad(Stage,Actor->GetPathName(),TEXT("template_components"),TEXT("empty plain native Actor template"),TEXT("changed"));
            TArray<UObject*> TemplateNested;GetObjectsWithOuter(Actor,TemplateNested,true);TArray<TStrongObjectPtr<UObject>> ReturnedNested;for(auto* ChildObject:TemplateNested)ReturnedNested.Emplace(ChildObject);
            if(!TemplateNested.IsEmpty())return MixedBad(Stage,Actor->GetPathName(),TEXT("template_nested_objects"),TEXT("empty captured inventory"),TEXT("new owned object; no child getter used"));
        }
        return true;
    }
    bool MixedObject(UObject* Object,UClass* Class,const TCHAR* Stage,UObject* ExpectedOuter=nullptr)
    {
        const TStrongObjectPtr<UObject> ReturnedObject(Object);if(!ExpectedOuter)ExpectedOuter=Parent.Scene;
        if(!Pin(Object,Stage)||Object->GetClass()!=Class||Object->GetOuter()!=ExpectedOuter||Object->GetPackage()!=TargetPackage||Object->HasAnyFlags(RF_ClassDefaultObject|RF_BeginDestroyed|RF_FinishDestroyed))return MixedBad(Stage,TEXT("collateral object"),TEXT("native_owner"),Class->GetPathName(),TEXT("unavailable/class/outer/package"));
        if(!Pin(Class,Stage))return false;
        for(const auto& O:MixedObjects)if(O.Object==Object)return true;
        FMixedObject Row;Row.Object=Object;MixedObjects.Add(MoveTemp(Row));return true;
    }
    bool MixedActor(UObject* Object,const TCHAR* Stage)
    {
        if(!MixedObject(Object,AActor::StaticClass(),Stage))return false;auto* Actor=static_cast<AActor*>(Object);
        if(!Actor->GetComponents().IsEmpty()||Actor->GetRootComponent())return Fail(R,TEXT("sequencer_mixed_actor_template_pending"),TEXT("Native plain Actor template without owned components is the current readonly scope; component/custom template profiles remain valid incomplete."));
        TArray<UObject*> Children;GetObjectsWithOuter(Actor,Children,true);TArray<TStrongObjectPtr<UObject>> ReturnedChildren;for(auto* ChildObject:Children)ReturnedChildren.Emplace(ChildObject);for(auto* ChildObject:Children)if(!Pin(ChildObject,Stage))return false;
        if(!Check(Stage,false,false))return false;
        if(!Children.IsEmpty())return Fail(R,TEXT("sequencer_mixed_actor_template_children_pending"),TEXT("Owned nested template objects remain valid incomplete; no callback or template writer is invoked."));
        return true;
    }
    bool BindMixed()
    {
        if(!Parent.bMixedRemoval)return true;const TCHAR* Stage=TEXT("mixed_scene_bind");auto* Scene=Parent.Scene;
        for(UClass* Type:{UMovieSceneFolder::StaticClass(),UMovieSceneSpawnableActorBinding::StaticClass(),UMovieSceneSpawnTrack::StaticClass(),UMovieSceneBoolSection::StaticClass(),UMovieSceneSpawnSection::StaticClass(),AActor::StaticClass()})if(!Native(Type,*Type->GetPathName())||!Pin(Type,Stage))return Stop(Stage);
        MixedActorTemplate=FindFProperty<FObjectProperty>(UMovieSceneSpawnableActorBinding::StaticClass(),TEXT("ActorTemplate"));
        MixedSpawnableArray=FindFProperty<FArrayProperty>(UMovieScene::StaticClass(),TEXT("Spawnables"));MixedSpawnableInner=MixedSpawnableArray&&MixedSpawnableArray->Inner&&MixedSpawnableArray->Inner->GetClass()==FStructProperty::StaticClass()?static_cast<FStructProperty*>(MixedSpawnableArray->Inner):nullptr;
        if(!MixedSpawnableInner||!MixedSpawnableInner->Struct.IsResolved()||!Pin(MixedSpawnableInner->Struct.Get(),Stage))return Stop(Stage);MixedSpawnableStruct=MixedSpawnableInner->Struct.Get();
        MixedLegacyTemplate=FindFProperty<FObjectProperty>(MixedSpawnableStruct,TEXT("ObjectTemplate"));
        if(!MixedStorage(Stage))return false;
        MixedActorOffset=MixedActorTemplate->GetOffset_ForInternal();MixedLegacyOffset=MixedLegacyTemplate->GetOffset_ForInternal();MixedArrayOffset=MixedSpawnableArray->GetOffset_ForInternal();MixedActorFlags=uint64(MixedActorTemplate->PropertyFlags);MixedLegacyFlags=uint64(MixedLegacyTemplate->PropertyFlags);MixedArrayFlags=uint64(MixedSpawnableArray->PropertyFlags);MixedInnerFlags=uint64(MixedSpawnableInner->PropertyFlags);
        if(Scene->GetSpawnableCount()>MaxBindings||Scene->GetRootFolders().Num()>MaxBindings||Scene->AllTaggedBindings().Num()>MaxBindings)return Fail(R,TEXT("sequencer_mixed_scene_budget"),TEXT("Finite collateral spawnable/folder/tag budget exceeded before mutation."));
        for(int32 I=0;I<Scene->GetSpawnableCount();++I)
        {
            const FMovieSceneSpawnable Value=Scene->GetSpawnable(I);const auto& Dynamic=Value.DynamicBinding_DEPRECATED;
            if(!Dynamic.Function.IsResolved()||Dynamic.Function.Get()!=nullptr||!Dynamic.ResolveParamsProperty.IsPathToFieldEmpty()||Dynamic.PayloadVariables.Num()!=0||!Dynamic.CompiledFunctionName.IsNone()||!Dynamic.ResolveParamsPinName.IsNone()||!Dynamic.WeakEndpoint.IsExplicitlyNull())return Fail(R,TEXT("sequencer_mixed_legacy_dynamic_pending"),TEXT("Legacy dynamic endpoint/payload callbacks remain valid incomplete; no field path resolution is performed."));
            const auto Slot=MixedLegacyTemplate->GetObjectPtrPropertyValue_InContainer(&Value);if(!Slot.IsResolved())return Stop(Stage);
            UObject* Template=Slot.Get();const TStrongObjectPtr<UObject> ReturnedTemplate(Template);if(Template&&!MixedActor(Template,Stage))return false;MixedSpawnables.Add(Value);
        }
        const auto* Refs=Parent.Sequence->GetBindingReferences();if(!Refs)return Stop(Stage);
        TArray<FMovieSceneBindingReference> ReferenceValues;ReferenceValues.Append(Refs->GetAllReferences().GetData(),Refs->GetAllReferences().Num());
        for(const auto& Ref:ReferenceValues)
        {
            if(!Ref.CustomBinding.IsResolved())return Stop(Stage);auto* Custom=Ref.CustomBinding.Get();const TStrongObjectPtr<UObject> ReturnedBinding(Custom);if(!Custom)continue;
            if(Ref.ID==Parent.Binding||!MixedObject(Custom,UMovieSceneSpawnableActorBinding::StaticClass(),Stage))return Fail(R,TEXT("sequencer_mixed_custom_binding_pending"),TEXT("Only nonselected exact native SpawnableActorBinding is observed; selected or other custom bindings remain valid incomplete."));
            auto* Binding=static_cast<UMovieSceneSpawnableActorBinding*>(Custom);const auto Slot=MixedActorTemplate->GetObjectPtrPropertyValue_InContainer(Binding);if(!Slot.IsResolved())return Stop(Stage);UObject* Template=Slot.Get();const TStrongObjectPtr<UObject> ReturnedTemplate(Template);
            if(Template&&!MixedActor(Template,Stage))return false;
            if(auto* Existing=MixedBindings.Find(Ref.ID);Existing&&*Existing!=Binding)return Fail(R,TEXT("sequencer_mixed_multi_custom_pending"),TEXT("Multiple different custom bindings for one GUID remain valid incomplete."));
            FMixedTemplate Row;Row.Binding=Binding;Row.Actor=static_cast<AActor*>(Template);MixedTemplates.Add(Row);MixedBindings.Add(Ref.ID,Binding);
        }
        const auto Roots=Scene->GetRootFolders();for(auto* F:Roots)
        {
            if(!MixedObject(F,UMovieSceneFolder::StaticClass(),Stage)||!F->GetChildFolders().IsEmpty()||F->GetDecorations().Num()!=0)return Fail(R,TEXT("sequencer_mixed_folder_pending"),TEXT("Exact native undecorated root leaf-folders are observed; nested/custom folders remain valid incomplete."));
            FMixedFolder Row;Row.Folder=F;Row.Name=F->GetFolderName();Row.Color=F->GetFolderColor();Row.Sort=F->GetSortingOrder();Row.Tracks=F->GetChildTracks();Row.Bindings=F->GetChildObjectBindings();
            if(Row.Tracks.Num()>MaxTracks||Row.Bindings.Num()>MaxBindings||Row.Bindings.Contains(Parent.Binding))return Fail(R,TEXT("sequencer_mixed_selected_folder_pending"),TEXT("Collateral folders cannot contain the selected binding or selected tracks in this bounded source-preserving scope."));
            for(auto* T:Row.Tracks){bool Kept=false;for(const auto& B:static_cast<const UMovieScene*>(Scene)->GetBindings())if(B.GetObjectGuid()!=Parent.Binding&&B.GetTracks().Contains(T))Kept=true;if(!Kept&&!Scene->GetTracks().Contains(T))return Fail(R,TEXT("sequencer_mixed_folder_track_pending"),TEXT("Folder tracks must refer to protected nonselected or root tracks."));}
            for(const auto& Id:Row.Bindings)if(!Scene->FindBinding(Id))return Fail(R,TEXT("sequencer_mixed_folder_binding_pending"),TEXT("Folder binding references must be current nonselected IDs."));
            MixedFolders.Add(MoveTemp(Row));MixedRoots.Add(F);
        }
        MixedTags=Scene->AllTaggedBindings();for(const auto& Group:MixedTags)
        {
            if(Group.Value.IDs.Num()>MaxBindings)return Fail(R,TEXT("sequencer_mixed_tag_budget"),TEXT("Finite tagged ID budget exceeded."));
            for(const auto& Id:Group.Value.IDs)if(!Id.IsFixedBinding()||Id.GetRelativeSequenceID()!=MovieSceneSequenceID::Root||Id.GetGuid()==Parent.Binding||!Scene->FindBinding(Id.GetGuid()))return Fail(R,TEXT("sequencer_mixed_tag_reference_pending"),TEXT("Only current nonselected fixed root-sequence tagged IDs are observed; selected/external tag rewrites remain valid incomplete."));
        }
        for(const auto& Row:Tracks)if(Row.Track->GetOuter()==Scene&&Row.Track->GetClass()==UMovieSceneSpawnTrack::StaticClass())
        {
            if(!MixedObject(Row.Track,UMovieSceneSpawnTrack::StaticClass(),Stage))return false;
            for(auto* S:Row.Sections){UClass* MixedSectionClass=Loaded(S)&&S->GetClass()==UMovieSceneSpawnSection::StaticClass()?UMovieSceneSpawnSection::StaticClass():UMovieSceneBoolSection::StaticClass();if(!MixedObject(S,MixedSectionClass,Stage,Row.Track))return false;}
        }
        MixedReady=true;if(!Check(Stage,false,false))return false;
        R.Data->SetNumberField(TEXT("mixed_scene_legacy_spawnables"),MixedSpawnables.Num());R.Data->SetNumberField(TEXT("mixed_scene_custom_bindings"),MixedBindings.Num());R.Data->SetNumberField(TEXT("mixed_scene_root_folders"),MixedRoots.Num());R.Data->SetNumberField(TEXT("mixed_scene_tag_groups"),MixedTags.Num());
        return ObserveMixed(Stage);
    }
    bool ObserveMixed(const TCHAR* Stage,TSharedRef<FJsonObject>* Output=nullptr)
    {
        if(!Parent.bMixedRemoval||!MixedReady||MixedObserving)return true;TGuardValue<bool> Guard(MixedObserving,true);
        const TFunction<bool()> Current=[this,Stage]{return Check(Stage,false,false);};auto State=MakeShared<FJsonObject>();
        for(auto& O:MixedObjects)
        {
            auto Props=MakeShared<FJsonObject>();if(!ObjectProperties(O.Object,{},Props,Current))return Stop(Stage);
            if(O.Fields.IsValid()&&!CameraLiteral(MakeShared<FJsonValueObject>(O.Fields),MakeShared<FJsonValueObject>(Props),0))return MixedBad(Stage,O.Object->GetPathName(),TEXT("readonly fields"),TEXT("captured native fields"),TEXT("changed"));
            if(!O.Fields.IsValid())O.Fields=Props;auto Row=Identity(O.Object);Row->SetObjectField(TEXT("fields"),Props);State->SetObjectField(O.Object->GetPathName(),Row);
        }
        auto Spawnables=MakeShared<FJsonObject>();
        // Copy current elements before the existing reflected observer. No array
        // element pointer survives the export/current boundary.
        TArray<FMovieSceneSpawnable> Actual;for(int32 I=0;I<Parent.Scene->GetSpawnableCount();++I)Actual.Add(Parent.Scene->GetSpawnable(I));
        if(!MaintenanceExportExpected(MixedSpawnableArray,&Actual,Parent.Scene,Spawnables,Current))return Stop(Stage);
        if(MixedSpawnableFields.IsValid()&&!CameraLiteral(MakeShared<FJsonValueObject>(MixedSpawnableFields),MakeShared<FJsonValueObject>(Spawnables),0))return MixedBad(Stage,Parent.Scene->GetPathName(),TEXT("legacy spawnable fields"),TEXT("copied readonly declaration"),TEXT("changed"));
        if(!MixedSpawnableFields.IsValid())MixedSpawnableFields=Spawnables;
        if(Output)(*Output)->Values=State->Values;MixedCaptured=true;return Check(Stage,false,false);
    }

    // JS contexts stay heap-stable even when source qualification fails.
    struct FSibling{TUniquePtr<FContext> Context;TSharedPtr<FJsonObject> Before;};
    FNestedSubsequenceRole* Nested=nullptr;TSharedPtr<FJsonObject> NestedChildBefore;
    TArray<FSibling> Siblings;bool CapturingSiblings=false;int32 SiblingTracks=0,SiblingSections=0,SiblingSnapshotChecks=0;
    FCameraMaintenanceScope(FContext& In,FContext* Source,FResult& Out):Parent(In),Child(Source),R(Out){}
    ~FCameraMaintenanceScope()
    {
        if(Lease)CameraInsertionActive=false;
        if(!Stopped)return;
        struct FRetained{TArray<FHeld> Objects;TStrongObjectPtr<UWorld> World;};static auto* Retained=new TArray<FRetained>();
        FRetained Keep;Keep.Objects=MoveTemp(Held);Keep.World=MoveTemp(World);Retained->Add(MoveTemp(Keep));CameraInsertionQuarantined=true;
    }
    void Invalidate()
    {
        R.bSuccess=false;R.bPost=false;R.Data->SetBoolField(TEXT("postcondition_verified"),false);R.Data->SetBoolField(TEXT("verified_success"),false);
        if(R.Data->HasTypedField<EJson::Object>(TEXT("legacy_mutation_contract"))){auto J=R.Data->GetObjectField(TEXT("legacy_mutation_contract"));for(const TCHAR* K:{TEXT("success"),TEXT("postcondition_verified"),TEXT("verified_success")})J->SetBoolField(K,false);J->SetStringField(TEXT("success_semantics"),TEXT("postcondition_failed"));if(J->HasTypedField<EJson::Object>(TEXT("verification")))J->GetObjectField(TEXT("verification"))->SetStringField(TEXT("status"),TEXT("failed"));}
    }
    bool Stop(const TCHAR* Stage)
    {
        if(!Stopped){R.Data->SetStringField(TEXT("camera_maintenance_first_failure_stage"),Stage);Fail(R,TEXT("sequencer_camera_maintenance_scope_changed"),TEXT("Captured sequence/source/camera/current roles are unsafe or changed; actual prefix retained, no next explicit SDK, observation or replay."));}
        Stopped=true;R.bKnown=false;R.Stage=Stage;Invalidate();if(!Siblings.IsEmpty())R.Data->SetBoolField(TEXT("sibling_sources_preserved"),false);R.Data->SetBoolField(TEXT("camera_maintenance_unsafe_holds_retained"),true);return false;
    }
    bool Context(const TCHAR* Stage)
    {
        if(Stopped)return false;
        if(!IsInGameThread()||!Loaded(GEditor)||GEditor!=Editor.Get()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage())return Stop(Stage);
        if(GEditor->PlayWorld||GEditor->GetPIEWorldContext()||!Loaded(World.Get())||World->bIsTearingDown||World->WorldType!=EWorldType::Editor||GEditor->GetEditorWorldContext().World()!=World.Get()||!World->GetPathName().Equals(WorldPath,ESearchCase::CaseSensitive))return Stop(Stage);
        return true;
    }
    bool Pin(UObject* O,const TCHAR* Stage)
    {
        if(!Context(Stage)||!Loaded(O)||!Loaded(O->GetClass())||(O->GetOuter()&&!Loaded(O->GetOuter()))||!Loaded(O->GetPackage()))return Stop(Stage);
        for(const auto& I:Held)if(I.Hold.Get()==O)return true;
        if(Held.Num()>4*(MaxTracks+MaxSections)+32)return Stop(Stage);
        FHeld I;I.Hold.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();I.PackageFlags=I.Package->GetPackageFlags();I.Dirty=I.Package->IsDirty();I.SourceClass=Cast<UClass>(O);if(I.SourceClass)I.ClassFlags=I.SourceClass->GetClassFlags();Held.Add(MoveTemp(I));return true;
    }
    bool CaptureTrack(UMovieSceneTrack* T)
    {
        for(const auto& P:Tracks)if(P.Track==T)return true;
        if(!Pin(T,TEXT("camera_maintenance_track_bind")))return false;
        const int32 ClipKind=((Parent.bRemovalClips&&T->GetOuter()==Parent.Scene)||(Child&&Child->bSubsequenceClips&&T->GetOuter()==Child->Scene))?SubsequenceClipKind(T):0;
        if(T->GetClass()!=UMovieScene3DTransformTrack::StaticClass()&&T->GetClass()!=UMovieSceneFloatTrack::StaticClass()&&T->GetClass()!=UMovieSceneSubTrack::StaticClass()&&T->GetClass()!=UMovieSceneCameraCutTrack::StaticClass()&&!ClipKind&&!MaintenanceMixedSpawnTrack(Parent,T))return Stop(TEXT("camera_maintenance_track_class"));
        if(Parent.bRemovalClips&&ClipKind&&(T->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||T->GetDecorations().Num()!=0))return Stop(TEXT("removal_clip_track_profile"));
        FTrackRole Row;Row.Track=T;
        const TArray<UMovieSceneSection*> Current=T->GetAllSections();if(Current.Num()>MaxSections)return Stop(TEXT("camera_maintenance_section_budget"));
        for(auto* S:Current){if(!Pin(S,TEXT("camera_maintenance_section_bind"))||S->GetOuter()!=T)return Stop(TEXT("camera_maintenance_section_bind"));Row.Sections.Add(S);if(Parent.bRemovalClips&&ClipKind&&(S->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||S->GetDecorations().Num()!=0))return Stop(TEXT("removal_clip_section_profile"));if(ClipKind&&!BindClip(S,ClipKind))return false;}
        Tracks.Add(MoveTemp(Row));return true;
    }
    bool BindScene(FContext& C)
    {
        if(!Pin(C.Sequence,TEXT("camera_maintenance_bind"))||!Pin(C.Scene,TEXT("camera_maintenance_bind")))return false;
        FSceneRole Row;Row.Context=&C;Row.Playback=C.Scene->GetPlaybackRange();Row.Display=C.Scene->GetDisplayRate();Row.Resolution=C.Scene->GetTickResolution();Row.ReadOnly=C.Scene->IsReadOnly();Row.Roots=C.Scene->GetTracks();
        auto* Base=C.Scene->GetCameraCutTrack();Row.Cut=Base&&Loaded(Base)&&Base->GetClass()==UMovieSceneCameraCutTrack::StaticClass()?static_cast<UMovieSceneCameraCutTrack*>(Base):nullptr;
        if(Base&&!Row.Cut)return Stop(TEXT("camera_maintenance_cut_class"));
        if(Row.Roots.Num()>MaxTracks||static_cast<const UMovieScene*>(C.Scene)->GetBindings().Num()>MaxBindings||!DirectorAllowed(C))return Stop(TEXT("camera_maintenance_scene_bound"));
        for(auto* T:Row.Roots)if(!CaptureTrack(T))return false;
        for(const auto& B:static_cast<const UMovieScene*>(C.Scene)->GetBindings()){FBindingRole Binding;Binding.Guid=B.GetObjectGuid();Binding.Tracks=B.GetTracks();for(auto* T:Binding.Tracks)if(!CaptureTrack(T))return false;Row.Bindings.Add(MoveTemp(Binding));}
        if(Row.Cut)
        {
            C.bCamera=true;
            if(!Native(UMovieSceneCameraCutTrack::StaticClass(),TEXT("/Script/MovieSceneTracks.MovieSceneCameraCutTrack"))||!Native(UMovieSceneCameraCutSection::StaticClass(),TEXT("/Script/MovieSceneTracks.MovieSceneCameraCutSection"))||!CameraTrackIdentity(C,Row.Cut)||!CaptureTrack(Row.Cut))return Stop(TEXT("camera_maintenance_cut_role"));
            if(!Pin(UMovieSceneCameraCutTrack::StaticClass(),TEXT("camera_maintenance_cut_source"))||!Pin(UMovieSceneCameraCutSection::StaticClass(),TEXT("camera_maintenance_cut_source")))return false;
            for(auto* BaseSection:CameraSections(Row.Cut))
            {
                auto* S=BaseSection&&BaseSection->GetClass()==UMovieSceneCameraCutSection::StaticClass()?static_cast<UMovieSceneCameraCutSection*>(BaseSection):nullptr;
                if(!CameraSectionIdentity(Row.Cut,S))return Stop(TEXT("camera_maintenance_cut_section"));
                FCutRole Cut;Cut.Track=Row.Cut;Cut.Section=S;Cut.Range=S->GetRange();Cut.Binding=S->GetCameraBindingID();Cuts.Add(MoveTemp(Cut));
            }
        }
        if(Row.Cut)Row.CutPath=Row.Cut->GetPathName();
        Scenes.Add(MoveTemp(Row));C.CameraMaintenance=this;return true;
    }
    bool Bind()
    {
        if(CameraInsertionActive||CameraInsertionQuarantined)return Fail(R,TEXT("sequencer_camera_maintenance_busy"),TEXT("A camera writer or unsafe retained scope exists; no nested SDK or replay."));
        Editor=GEditor;if(!IsInGameThread()||!Loaded(GEditor)||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage())return Stop(TEXT("camera_maintenance_bind"));
        World.Reset(GEditor->GetEditorWorldContext().World());if(!Loaded(World.Get()))return Stop(TEXT("camera_maintenance_bind"));WorldPath=World->GetPathName();if(!Context(TEXT("camera_maintenance_bind")))return false;
        CameraInsertionActive=true;Lease=true;TargetPackage=Parent.Sequence->GetPackage();
        if(Child&&(!Native(UMovieSceneSubTrack::StaticClass(),TEXT("/Script/MovieScene.MovieSceneSubTrack"))||!Native(UMovieSceneSubSection::StaticClass(),TEXT("/Script/MovieScene.MovieSceneSubSection"))))return Fail(R,TEXT("sequencer_subsequence_native_source_required"),TEXT("Exact loaded native runtime SubTrack/SubSection sources required."));
        for(UClass* Type:{ULevelSequence::StaticClass(),UMovieScene::StaticClass(),UMovieScene3DTransformTrack::StaticClass(),UMovieScene3DTransformSection::StaticClass(),UMovieSceneFloatTrack::StaticClass(),UMovieSceneFloatSection::StaticClass()})if(!Pin(Type,TEXT("camera_maintenance_native_source")))return false;
        if(Child&&(!Pin(UMovieSceneSubTrack::StaticClass(),TEXT("camera_maintenance_sub_source"))||!Pin(UMovieSceneSubSection::StaticClass(),TEXT("camera_maintenance_sub_source"))))return false;
        if(Parent.bRemovalClips||(Child&&Child->bSubsequenceClips))
        {
            for(UClass* Type:{UMovieSceneSkeletalAnimationTrack::StaticClass(),UMovieSceneSkeletalAnimationSection::StaticClass(),UMovieSceneAudioTrack::StaticClass(),UMovieSceneAudioSection::StaticClass()})
                if(!Native(Type,*FString::Printf(TEXT("/Script/MovieSceneTracks.%s"),*Type->GetName()))||!Pin(Type,TEXT("subsequence_clip_native_source")))return Stop(TEXT("subsequence_clip_native_source"));
        }
        if(!BindScene(Parent)||(Child&&!BindScene(*Child)))return false;
        if(Nested)
        {
            if(!Nested->SourceContext)return Stop(TEXT("nested_leaf_context_missing"));
            FSibling Row;Row.Context=MoveTemp(Nested->SourceContext);Row.Context->bSubsequence=true;Row.Context->bNestedSubsequence=true;Siblings.Add(MoveTemp(Row));
            if(!Pin(Nested->Leaf->GetPackage(),TEXT("nested_leaf_prebind"))||!BindScene(*Siblings.Last().Context))return false;
        }
        if(Parent.bMixedRemoval){if(!BindLeaf(false)||!BindMixed()||!SealLeaf())return false;}else if(!BindLeaf())return false;
        R.Data->SetBoolField(TEXT("camera_maintenance_scope"),true);R.Data->SetBoolField(TEXT("camera_binding_literals_rewritten"),false);R.Data->SetBoolField(TEXT("SDK_internal_callback_atomicity_proven"),false);
        if(!Check(TEXT("camera_maintenance_bound")))return false;
        if(Parent.bRemovalClips||(Child&&Child->bSubsequenceClips))
        {
            R.Data->SetStringField(Parent.bRemovalClips?TEXT("removal_clip_scope"):TEXT("subsequence_clip_scope"),Parent.bRemovalClips?TEXT("Original possessable removal with exact native Audio/SkeletalAnimation selected or collateral tracks; their held sections/channels and nullable or already loaded clean persisted AnimSequence/SoundWave sources remain unchanged after declaration retirement. Fixed animation play-rate, no MirrorDataTable/attenuation reference and empty sound-input maps; original RemovePossessable/Unbind/dirty/save and actual partial failure order retained. No clip source writer/save/load, playback, channel-proxy/root-motion construction, physical object deletion or new callback atomicity guarantee. Wider clip sources/parameters/decorations/custom classes remain incomplete."):TEXT("Readonly exact native SkeletalAnimation/Audio tracks and sections in selected Child; nullable or already resolved loaded clean persisted AnimSequence/SoundWave assets in separate Game/Engine packages. Fixed animation play-rate, no MirrorDataTable/attenuation override reference, empty sound input maps. Copied weight/volume/pitch channels and existing section fields/attachment data preserved without channel proxy construction, playback, root-motion/locale lookup, clip authoring or source save. Bounded nontransient reflected source fields and uasset/uexp/ubulk/uptnl files are observed; transitive source dependency graphs, decoded/virtualized bulk payload and wider clip families remain incomplete."));
            R.Data->SetBoolField(Parent.bRemovalClips?TEXT("removal_clip_source_write_dispatched"):TEXT("subsequence_clip_source_write_dispatched"),false);R.Data->SetBoolField(Parent.bRemovalClips?TEXT("removal_clip_channel_proxy_requested"):TEXT("subsequence_clip_channel_proxy_requested"),false);
            if(!ObserveClips(TEXT("subsequence_clip_source_before")))return false;
        }
        return true;
    }
    bool Check(const TCHAR* Stage,bool SaveReturned=false,bool ObserveSiblings=true)
    {
        if(!Context(Stage))return false;
        for(auto& I:Held)
        {
            auto* O=I.Hold.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive)||I.Package->GetPackageFlags()!=I.PackageFlags||(I.SourceClass&&I.SourceClass->GetClassFlags()!=I.ClassFlags)||(I.Package!=TargetPackage&&I.Package->IsDirty()!=I.Dirty))return Stop(Stage);
            const auto Flags=O->GetFlags();if(Flags!=I.Flags){if(!SaveReturned||!R.bSaved||I.Package!=TargetPackage||O->HasAnyFlags(RF_Transient)||Flags!=(I.Flags|RF_WasLoaded|RF_LoadCompleted))return Stop(Stage);I.Flags=Flags;}
        }
        for(const auto& Row:Scenes)
        {
            auto& C=*Row.Context;
            if(C.Sequence->GetMovieScene()!=C.Scene||C.Scene->GetOuter()!=C.Sequence||!DirectorCurrent(C,R,Stage,SaveReturned)||C.Scene->IsReadOnly()!=Row.ReadOnly||C.Scene->GetPlaybackRange()!=Row.Playback||C.Scene->GetDisplayRate()!=Row.Display||C.Scene->GetTickResolution()!=Row.Resolution||C.Scene->GetCameraCutTrack()!=Row.Cut||C.Scene->GetTracks()!=Row.Roots)return Stop(Stage);
            const auto& Bindings=static_cast<const UMovieScene*>(C.Scene)->GetBindings();if(Bindings.Num()!=Row.Bindings.Num())return Stop(Stage);
            for(int32 I=0;I<Bindings.Num();++I)if(Bindings[I].GetObjectGuid()!=Row.Bindings[I].Guid||Bindings[I].GetTracks()!=Row.Bindings[I].Tracks)return Stop(Stage);
            if(Row.Cut&&!CameraTrackIdentity(C,Row.Cut))return Stop(Stage);
        }
        for(const auto& Row:Tracks){const auto& Now=Row.Track->GetAllSections();if(Now!=Row.Sections)return Stop(Stage);}
        for(const auto& Cut:Cuts)if(!CameraSectionIdentity(Cut.Track,Cut.Section)||Cut.Section->GetRange()!=Cut.Range||Cut.Section->GetCameraBindingID()!=Cut.Binding)return Stop(Stage);
        if(Nested&&(!Child||!Nested->Current(*Child)))return Stop(Stage);
        if(!LeafPlain(Stage)||!MixedPlain(Stage)||!ClipPlain(Stage))return false;
        return !ObserveSiblings||(ObserveLeaf(Stage)&&ObserveMixed(Stage)&&CheckSiblingSnapshots(Stage));
    }
    bool Observe(FContext& C,const TSharedRef<FJsonObject>& State,const TCHAR* Stage)
    {
        if(!ObserveClips(Stage))return false;
        for(auto& Row:Scenes)if(Row.Context==&C&&Row.Cut)
        {
            const auto Track=State->GetObjectField(TEXT("tracks"))->GetObjectField(Row.CutPath);
            auto Fields=MakeShared<FJsonObject>();Fields->SetObjectField(TEXT("track_fields"),Track->GetObjectField(TEXT("fields")));
            TArray<TSharedPtr<FJsonValue>> Sections;
            for(const auto& Value:Track->GetArrayField(TEXT("sections"))){auto Plain=MakeShared<FJsonObject>();const auto Section=Value->AsObject();Plain->SetObjectField(TEXT("fields"),Section->GetObjectField(TEXT("fields")));Plain->SetObjectField(TEXT("range"),Section->GetObjectField(TEXT("range")));Sections.Add(MakeShared<FJsonValueObject>(Plain));}
            Fields->SetArrayField(TEXT("sections"),Sections);
            if(!Row.CutFields.IsValid())Row.CutFields=Fields;
            else if(!CameraLiteral(MakeShared<FJsonValueObject>(Row.CutFields),MakeShared<FJsonValueObject>(Fields),0))return Stop(Stage);
        }
        R.Data->SetBoolField(TEXT("camera_cut_literals_preserved"),true);return true;
    }
    static FString SiblingValue(const TSharedPtr<FJsonValue>& Value)
    {
        if(!Value)return TEXT("<missing>");
        if(Value->Type==EJson::String)return Value->AsString().Left(256);
        if(Value->Type==EJson::Number)return LexToString(Value->AsNumber());
        if(Value->Type==EJson::Boolean)return Value->AsBool()?TEXT("true"):TEXT("false");
        if(Value->Type==EJson::Array)return FString::Printf(TEXT("<array:%d>"),Value->AsArray().Num());
        if(Value->Type==EJson::Object)return FString::Printf(TEXT("<object:%d>"),Value->AsObject()->Values.Num());
        return TEXT("null");
    }
    static bool SiblingDifference(const TSharedPtr<FJsonValue>& Expected,const TSharedPtr<FJsonValue>& Actual,const FString& Path,TSharedRef<FJsonObject>& Out,int32& Budget,int32 Depth=0)
    {
        if(--Budget<0||Depth>32){Out->SetStringField(TEXT("property"),Path.Left(1024));Out->SetBoolField(TEXT("diagnostic_budget_exhausted"),true);return true;}
        if(Expected&&Actual&&Expected->Type==Actual->Type)
        {
            if(Expected->Type==EJson::Object)
            {
                const auto Left=Expected->AsObject(),Right=Actual->AsObject();TArray<FString> Names;
                for(const auto& Pair:Left->Values)Names.Add(FString(Pair.Key.ToView()));
                for(const auto& Pair:Right->Values)Names.AddUnique(FString(Pair.Key.ToView()));
                Names.Sort([](const FString& A,const FString& B){return A.Compare(B,ESearchCase::CaseSensitive)<0;});
                for(const auto& Name:Names)if(SiblingDifference(Left->TryGetField(Name),Right->TryGetField(Name),Path+TEXT("/")+Name,Out,Budget,Depth+1))return true;
                return false;
            }
            if(Expected->Type==EJson::Array)
            {
                const auto& Left=Expected->AsArray();const auto& Right=Actual->AsArray();
                if(Left.Num()==Right.Num()){for(int32 I=0;I<Left.Num();++I)if(SiblingDifference(Left[I],Right[I],Path+TEXT("/")+FString::FromInt(I),Out,Budget,Depth+1))return true;return false;}
            }
            else if(CameraLiteral(Expected,Actual,0))return false;
        }
        Out->SetStringField(TEXT("property"),Path.Left(1024));Out->SetStringField(TEXT("expected"),SiblingValue(Expected));Out->SetStringField(TEXT("actual"),SiblingValue(Actual));
        if(Expected&&Expected->Type==EJson::String)Out->SetNumberField(TEXT("expected_chars"),Expected->AsString().Len());
        if(Actual&&Actual->Type==EJson::String)Out->SetNumberField(TEXT("actual_chars"),Actual->AsString().Len());
        return true;
    }
    bool SiblingStop(const TCHAR* Stage,const FString& SourcePath,const TCHAR* Why,const TSharedPtr<FJsonObject>& Expected=nullptr,const TSharedPtr<FJsonObject>& Actual=nullptr)
    {
        const bool First=!Stopped;
        if(First)
        {
            auto Detail=MakeShared<FJsonObject>();Detail->SetStringField(TEXT("stage"),Stage);Detail->SetStringField(TEXT("source"),SourcePath);Detail->SetStringField(TEXT("reason"),Why);
            if(Expected.IsValid()&&Actual.IsValid()){int32 Budget=4096;SiblingDifference(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Actual),TEXT(""),Detail,Budget);}
            R.Data->SetObjectField(TEXT("sibling_first_failure"),Detail);
        }
        Stop(Stage);if(First)Fail(R,TEXT("sequencer_subsequence_sibling_source_changed"),Why);return false;
    }
    bool CheckSiblingSnapshots(const TCHAR* Stage)
    {
        if((Siblings.IsEmpty()&&!NestedChildBefore.IsValid())||CapturingSiblings)return true;
        TGuardValue<bool> CaptureGuard(CapturingSiblings,true);bool AllReady=true;
        if(NestedChildBefore.IsValid())
        {
            auto Now=MakeShared<FJsonObject>();const bool Captured=MaintenanceSnapshot(*Child,Now);if(!Check(Stage,false,false))return false;
            if(!Captured)return SiblingStop(Stage,Child->Sequence->GetPathName(),TEXT("Nested child readonly observation is incomplete."));
            auto Expected=MakeShared<FJsonObject>();Expected->Values=NestedChildBefore->Values;auto Other=MakeShared<FJsonObject>();Other->Values=NestedChildBefore->GetObjectField(TEXT("other_package_dirty"))->Values;
            const FString ParentPackage=Parent.Sequence->GetPackage()->GetPathName();if(!Other->HasTypedField<EJson::Boolean>(ParentPackage))return SiblingStop(Stage,Child->Sequence->GetPathName(),TEXT("Nested child parent dirty baseline is missing."));
            Other->SetBoolField(ParentPackage,Parent.Sequence->GetPackage()->IsDirty());Expected->SetObjectField(TEXT("other_package_dirty"),Other);
            if(!CameraLiteral(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Now),0))return SiblingStop(Stage,Child->Sequence->GetPathName(),TEXT("Nested child literal/source/disk changed."),Expected,Now);
            R.Data->SetObjectField(TEXT("nested_child_source_after"),Now);R.Data->SetBoolField(TEXT("nested_child_source_preserved"),true);
        }
        for(auto& Sibling:Siblings)
        {
            if(!Sibling.Before.IsValid()){AllReady=false;if(R.bAttempted)return SiblingStop(Stage,Sibling.Context->Sequence->GetPathName(),TEXT("A source baseline is incomplete after mutation began."));continue;}
            auto Now=MakeShared<FJsonObject>();++SiblingSnapshotChecks;R.Data->SetNumberField(TEXT("sibling_source_snapshot_checks"),SiblingSnapshotChecks);
            const bool Captured=MaintenanceSnapshot(*Sibling.Context,Now);
            if(!Check(Stage,false,false))return false;
            if(!Captured)return SiblingStop(Stage,Sibling.Context->Sequence->GetPathName(),TEXT("Bounded readonly sibling state/disk observation is incomplete."));
            auto Expected=MakeShared<FJsonObject>();Expected->Values=Sibling.Before->Values;
            auto Other=MakeShared<FJsonObject>();Other->Values=Sibling.Before->GetObjectField(TEXT("other_package_dirty"))->Values;
            const FString ParentPackage=Parent.Sequence->GetOutermost()->GetPathName();
            if(!Other->HasTypedField<EJson::Boolean>(ParentPackage))return SiblingStop(Stage,Sibling.Context->Sequence->GetPathName(),TEXT("Exact parent dirty baseline is unavailable."));
            Other->SetBoolField(ParentPackage,Parent.Sequence->GetOutermost()->IsDirty());Expected->SetObjectField(TEXT("other_package_dirty"),Other);
            if(!CameraLiteral(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Now),0))return SiblingStop(Stage,Sibling.Context->Sequence->GetPathName(),TEXT("Readonly sibling fields/identity/section/signature/dirty/disk changed; no further writer, save or replay."),Expected,Now);
            if(Nested&&Sibling.Context->Sequence==Nested->Leaf.Get()){R.Data->SetObjectField(TEXT("nested_leaf_source_after"),Now);R.Data->SetBoolField(TEXT("nested_leaf_source_preserved"),true);}
        }
        R.Data->SetBoolField(TEXT("sibling_sources_preserved"),AllReady);return true;
    }
    bool AdmitSibling(ULevelSequence* Sequence)
    {
        const TCHAR* Stage=TEXT("subsequence_sibling_admission");
        if(!Check(Stage))return false;
        if(!Loaded(Sequence)||Sequence->GetClass()!=ULevelSequence::StaticClass()||Sequence==Parent.Sequence||Sequence==Child->Sequence||Sequence->GetPackage()==TargetPackage||Sequence->GetPackage()==Child->Sequence->GetPackage())
            return Fail(R,TEXT("sequencer_subsequence_sibling_unsupported"),TEXT("Existing distinct siblings must be exact loaded native Game leaf sequences in separate parent/selected-child packages; no null, alias, loader or replacement source."));
        FSibling* Prebound=nullptr;
        for(auto& Sibling:Siblings)
        {
            if(Sibling.Context->Sequence==Sequence)
            {
                if(Sibling.Before.IsValid())return Check(Stage);
                if(Nested&&Sequence==Nested->Leaf.Get()){Prebound=&Sibling;break;}
                return Fail(R,TEXT("sequencer_subsequence_sibling_unqualified"),TEXT("Existing source qualification did not complete; no fallback or replay."));
            }
            if(Sibling.Context->Sequence->GetPackage()==Sequence->GetPackage())return Fail(R,TEXT("sequencer_subsequence_sibling_shared_package_unsupported"),TEXT("Distinct collateral sibling assets in one package remain outside this finite source scope."));
        }
        if(Siblings.Num()>=4)return Fail(R,TEXT("sequencer_subsequence_sibling_budget_exceeded"),TEXT("At most four distinct readonly collateral leaf siblings are admitted."));
        FResult Admission;Admission.bSaveRequested=R.bSaveRequested;
        if(!Prebound)
        {
        auto Pending=MakeUnique<FContext>();
        if(!Pin(Sequence,Stage)||!Pin(Sequence->GetPackage(),Stage))return false;
        const bool Resolved=Resolve(Sequence->GetPathName(),*Pending,Admission);if(!Check(Stage))return false;
        if(!Resolved||Pending->Sequence!=Sequence)return Fail(R,TEXT("sequencer_subsequence_sibling_unsupported"),Admission.Error.IsEmpty()?TEXT("Exact same loaded source identity is required."):Admission.Error);
        if(!DirectorAllowed(*Pending)||Pending->Scene->GetTracks().Num()>MaxTracks||static_cast<const UMovieScene*>(Pending->Scene)->GetBindings().Num()>MaxBindings)
            return Fail(R,TEXT("sequencer_subsequence_sibling_leaf_unsupported"),TEXT("Readonly siblings require a finite native leaf without Director Blueprint."));
        const auto LeafTrack=[](UMovieSceneTrack* Track){return Loaded(Track)&&(Track->GetClass()==UMovieSceneFloatTrack::StaticClass()||Track->GetClass()==UMovieScene3DTransformTrack::StaticClass());};
        for(auto* Track:Pending->Scene->GetTracks())if(!LeafTrack(Track))return Fail(R,TEXT("sequencer_subsequence_sibling_leaf_unsupported"),TEXT("Nested/custom sibling root tracks remain incomplete; this valid broader legacy behavior is not silently substituted."));
        for(const auto& Binding:static_cast<const UMovieScene*>(Pending->Scene)->GetBindings())for(auto* Track:Binding.GetTracks())if(!LeafTrack(Track))return Fail(R,TEXT("sequencer_subsequence_sibling_leaf_unsupported"),TEXT("Only ordinary native Float/Transform sibling bound tracks are admitted."));
        if(!Check(Stage))return false;
        FSibling Row;Row.Context=MoveTemp(Pending);Siblings.Add(MoveTemp(Row));auto& Added=Siblings.Last();
        Added.Context->bSubsequence=true;Added.Context->bNestedSubsequence=Nested!=nullptr;if(!BindScene(*Added.Context)||!Check(Stage))return false;
        }
        auto& Added=Prebound?*Prebound:Siblings.Last();
        if(Prebound)
        {
            auto First=MakeShared<FJsonObject>();const bool Captured=MaintenanceSnapshot(*Added.Context,First);if(!Check(Stage,false,false))return false;
            if(!Captured)return SiblingStop(Stage,Sequence->GetPathName(),TEXT("First nested leaf source observation is incomplete."));
            Added.Before=First;R.Data->SetObjectField(TEXT("nested_leaf_source_before"),First);
        }
        const bool Qualified=MaintenanceReady(*Added.Context,Admission)&&SubsequenceSiblingLeafAdmission(*Added.Context,Admission);if(!Check(Stage))return false;
        if(!Qualified)return Fail(R,Admission.Code.IsEmpty()?TEXT("sequencer_subsequence_sibling_leaf_unsupported"):*Admission.Code,Admission.Error.IsEmpty()?TEXT("Readonly sibling leaf admission is incomplete."):Admission.Error);
        int32 TracksHere=0,SectionsHere=0;for(const auto& SceneRow:Scenes)if(SceneRow.Context==Added.Context.Get())for(const auto& TrackRow:Tracks)if(TrackRow.Track->GetOuter()==SceneRow.Context->Scene){++TracksHere;SectionsHere+=TrackRow.Sections.Num();}
        if(SiblingTracks+TracksHere>MaxTracks||SiblingSections+SectionsHere>MaxSections)return Fail(R,TEXT("sequencer_subsequence_sibling_budget_exceeded"),TEXT("Aggregate collateral track/section budgets exceed the finite existing source bound."));
        auto BeforeSource=Prebound?Added.Before.ToSharedRef():MakeShared<FJsonObject>();const bool Captured=Prebound||MaintenanceSnapshot(*Added.Context,BeforeSource);if(!Check(Stage))return false;
        if(!Captured)return SiblingStop(Stage,Sequence->GetPathName(),TEXT("Initial bounded sibling snapshot/disk is incomplete; no target mutation was dispatched."));
        const auto Disk=BeforeSource->GetObjectField(TEXT("disk"));
        if(R.bSaveRequested&&(!Disk->GetBoolField(TEXT("exists"))||Disk->GetNumberField(TEXT("bytes"))<=0||Sequence->GetOutermost()->IsDirty()))return Fail(R,TEXT("sequencer_subsequence_sibling_reference_not_persisted"),TEXT("Parent save requires every readonly collateral sibling to be clean and persisted; no sibling save is dispatched."));
        Added.Before=BeforeSource;if(Nested&&Sequence==Nested->Leaf.Get())R.Data->SetObjectField(TEXT("nested_leaf_source_before"),BeforeSource);SiblingTracks+=TracksHere;SiblingSections+=SectionsHere;
        TArray<TSharedPtr<FJsonValue>> Sources;for(const auto& Sibling:Siblings){auto Summary=MakeShared<FJsonObject>();Summary->SetStringField(TEXT("sequence"),Sibling.Context->Sequence->GetPathName());Summary->SetBoolField(TEXT("qualified"),Sibling.Before.IsValid());if(Sibling.Before.IsValid())Summary->SetObjectField(TEXT("disk"),Sibling.Before->GetObjectField(TEXT("disk")));Sources.Add(MakeShared<FJsonValueObject>(Summary));}
        R.Data->SetArrayField(TEXT("readonly_sibling_sources"),Sources);R.Data->SetNumberField(TEXT("readonly_sibling_count"),Siblings.Num());
        R.Data->SetStringField(TEXT("sibling_source_scope"),TEXT("At most four exact already-loaded native Game leaf siblings; bounded literal fields/current sections/source/disk/dirty snapshots at explicit SDK and publication boundaries. Only parent package dirty follows actual parent dirty. No nested/cyclic/director/custom loader, source writer/compile/save or global callback atomicity claim."));
        return Check(Stage);
    }
    bool Removed(bool Returned)
    {
        if(Parent.bLeafRemoval||Parent.bMixedRemoval||Parent.bHierarchyRemoval){if(!Returned)return Stop(TEXT("component_removal_remove_returned_false"));LeafPhase=1;}
        if(Returned){for(auto& Scene:Scenes)if(Scene.Context==&Parent){for(int32 I=0;I<Scene.Bindings.Num();++I)if(Scene.Bindings[I].Guid==Parent.Binding){Scene.Bindings.RemoveAt(I);break;}}}
        return Check(TEXT("camera_maintenance_after_remove"));
    }
    bool AdoptTrack(UMovieSceneSubTrack* T,bool Created)
    {
        if(!Context(TEXT("camera_maintenance_track_return")))return false;
        if(!T)return Check(TEXT("camera_maintenance_track_null"));
        if(!Loaded(Parent.Sequence)||!Loaded(Parent.Scene)||!Loaded(TargetPackage)||Parent.Sequence->GetOuter()!=TargetPackage||Parent.Scene->GetOuter()!=Parent.Sequence||!Loaded(T)||T->GetClass()!=UMovieSceneSubTrack::StaticClass()||T->GetOuter()!=Parent.Scene||!Pin(T,TEXT("camera_maintenance_track_return")))return Stop(TEXT("camera_maintenance_track_return"));
        if(T->GetPackage()!=TargetPackage)return Stop(TEXT("camera_maintenance_track_package"));
        if(Created){for(auto& Row:Scenes)if(Row.Context==&Parent)Row.Roots.Add(T);if(!T->GetAllSections().IsEmpty())return Stop(TEXT("camera_maintenance_new_track_not_empty"));FTrackRole Row;Row.Track=T;Tracks.Add(MoveTemp(Row));}
        return Check(TEXT("camera_maintenance_track_return"));
    }
    bool AdoptSection(UMovieSceneSubTrack* T,UMovieSceneSubSection* S)
    {
        if(!Context(TEXT("camera_maintenance_section_return")))return false;
        if(!S)return Check(TEXT("camera_maintenance_section_null"));
        if(!Loaded(Parent.Sequence)||!Loaded(Parent.Scene)||!Loaded(TargetPackage)||Parent.Sequence->GetOuter()!=TargetPackage||Parent.Scene->GetOuter()!=Parent.Sequence||!Loaded(T)||T->GetOuter()!=Parent.Scene||!Loaded(S)||S->GetClass()!=UMovieSceneSubSection::StaticClass()||S->GetOuter()!=T||!Pin(S,TEXT("camera_maintenance_section_return")))return Stop(TEXT("camera_maintenance_section_return"));
        if(S->GetPackage()!=TargetPackage)return Stop(TEXT("camera_maintenance_section_package"));
        bool Found=false;for(auto& Row:Tracks)if(Row.Track==T){if(Row.Sections.Contains(S))return Stop(TEXT("camera_maintenance_section_not_fresh"));Row.Sections.Add(S);Found=true;break;}
        return Found?Check(TEXT("camera_maintenance_section_return")):Stop(TEXT("camera_maintenance_section_track_missing"));
    }
    void Publication(const TSharedPtr<FJsonObject>& Expected,const TSharedPtr<FJsonObject>& SourceExpected)
    {
        R.Data->SetBoolField(TEXT("camera_maintenance_transaction_returned"),true);
        if(Stopped){Invalidate();return;}if(!Check(TEXT("camera_maintenance_after_transaction")))return;
        if(Expected.IsValid())
        {
            auto Now=MakeShared<FJsonObject>();if(!MaintenanceCapture(Parent,R,TEXT("camera_maintenance_publication"),Now)){Invalidate();return;}
            auto A=MakeShared<FJsonObject>(),B=MakeShared<FJsonObject>();A->Values=Expected->Values;B->Values=Now->Values;
            for(const TCHAR* K:{TEXT("sequence_signature"),TEXT("movie_scene_signature")}){A->RemoveField(K);B->RemoveField(K);}
            const bool SameState=CameraLiteral(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B),0);R.Data->SetBoolField(TEXT("camera_maintenance_transaction_state_preserved"),SameState);
            if(!SameState){Invalidate();Fail(R,TEXT("sequencer_camera_maintenance_transaction_changed"),TEXT("Final parent state changed after transaction callbacks; actual mutation/save receipt retained."));}
        }
        else if(R.bSuccess){R.bKnown=false;Invalidate();Fail(R,TEXT("sequencer_camera_maintenance_publication_incomplete"),TEXT("No complete final parent state available."));}
        if(Child&&SourceExpected.IsValid())
        {
            auto Now=MakeShared<FJsonObject>();if(!MaintenanceCapture(*Child,R,TEXT("camera_maintenance_source_publication"),Now)){Invalidate();return;}
            if(!CameraLiteral(MakeShared<FJsonValueObject>(SourceExpected),MakeShared<FJsonValueObject>(Now),0)){Invalidate();Fail(R,TEXT("sequencer_camera_maintenance_source_transaction_changed"),TEXT("Readonly child/source/disk changed after transaction callbacks; no child writer/rollback/replay."));}
        }
        if(!R.bSuccess||!R.bPost)Invalidate();else R.Data->SetBoolField(TEXT("camera_maintenance_publication_fence_passed"),true);
    }
};
bool CameraMaintenanceCheck(FCameraMaintenanceScope* Scope,const TCHAR* Stage,bool SaveReturned)
{return !Scope||Scope->Check(Stage,SaveReturned);}
bool CameraMaintenancePlainCheck(FCameraMaintenanceScope* Scope,const TCHAR* Stage){return Scope&&Scope->Check(Stage,false,false);}
void CameraMaintenanceAbort(FCameraMaintenanceScope* Scope,const TCHAR* Stage){if(Scope)Scope->Stop(Stage);}
bool CameraMaintenanceObserve(FCameraMaintenanceScope* Scope,FContext& C,const TSharedRef<FJsonObject>& State,const TCHAR* Stage){return !Scope||Scope->Observe(C,State,Stage);}
bool CameraMaintenanceMixedSnapshot(FCameraMaintenanceScope* Scope,TSharedRef<FJsonObject>& State){return Scope&&Scope->ObserveMixed(TEXT("mixed_scene_snapshot"),&State);}
bool CameraMaintenanceMixedBinding(FCameraMaintenanceScope* Scope,const FGuid& Id,const UObject* Object){if(!Scope||!Scope->Parent.bMixedRemoval)return false;const auto* Bound=Scope->MixedBindings.Find(Id);return Bound&&*Bound==Object;}
}

FString RemoveBinding(const FString& SequencePath, const FString& BindingId, const FString& ActorLabel, bool bSave)
{
    FResult R; InitMaintenance(R, TEXT("remove_level_sequence_binding"), bSave); FContext C;
    if (!Resolve(SequencePath, C, R,true) || !MaintenanceReady(C, R) || !MaintenanceResolveBinding(C, BindingId, ActorLabel, R)) { return Encode(R); }
    TUniquePtr<FCameraMaintenanceScope> Camera;
    if(!MaintenanceHierarchyInput(C,R)||!MaintenanceLeafInput(C,R)||!MaintenanceMixedInput(C,R))return Encode(R);
    // ADJ selects existing exact clip observers once; no source loader, evaluator or clip writer.
    TArray<UMovieSceneTrack*> ClipInventory=C.Scene->GetTracks();for(const auto& Binding:static_cast<const UMovieScene*>(C.Scene)->GetBindings())ClipInventory.Append(Binding.GetTracks());
    if(ClipInventory.Num()>MaxTracks){Fail(R,TEXT("sequencer_remove_binding_track_unsupported"),TEXT("Total track budget exceeded before clip admission."));return Encode(R);}
    for(auto* Track:ClipInventory)C.bRemovalClips|=SubsequenceClipKind(Track)!=0;
    C.bSubsequenceClips=C.bRemovalClips;
    if(C.bRemovalClips||C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval||C.Scene->GetCameraCutTrack()||C.Director.IsValid()){Camera=MakeUnique<FCameraMaintenanceScope>(C,nullptr,R);if(!Camera->Bind())return Encode(R);}
    TSharedPtr<FJsonObject> PublicationExpected;
    auto RunMutation=[&]()->FString
    {
    if(Camera){auto Baseline=MakeShared<FJsonObject>();if(!MaintenanceCapture(C,R,TEXT("camera_remove_admission_baseline"),Baseline))return Encode(R);}
    auto ExpectedPossessables = MakeShared<FJsonObject>(), ExpectedReferences = MakeShared<FJsonObject>();
    if (!MaintenanceRemoveAdmission(C, R, ExpectedPossessables, ExpectedReferences)) { return Encode(R); }
    if(!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_remove_admission_return")))return Encode(R);
    R.Data->SetStringField(TEXT("sequence"), C.Sequence->GetPathName()); R.Data->SetStringField(TEXT("binding_id"), C.Binding.ToString(EGuidFormats::DigitsWithHyphens)); R.Data->SetBoolField(TEXT("removed"), false);
    if (!MaintenanceCapture(C, R, TEXT("before_observation"), C.Before)) { return Encode(R); }
    auto BeforeInfo = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("before_info"), BeforeInfo);
    if (!MaintenanceInfo(C, BeforeInfo)) { if(C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval)CameraMaintenanceAbort(C.CameraMaintenance,TEXT("component_remove_info_incomplete")); Fail(R, TEXT("sequencer_maintenance_info_unobservable"), TEXT("Current loaded Info projection is incomplete before removal.")); return Encode(R); }
    if(!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_remove_info_return")))return Encode(R);
    TArray<TSharedPtr<FJsonValue>> Paths; for (const auto& Keep : C.MaintenanceTracks) { Paths.Add(MakeShared<FJsonValueString>(Keep->GetPathName())); } R.Data->SetArrayField(TEXT("removed_track_paths"), Paths);
    R.Stage = TEXT("mutate"); R.Data->SetNumberField(TEXT("outer_transaction_calls"), 1); FScopedTransaction Transaction(NSLOCTEXT("CubelessSequencer", "RemoveBinding", "Remove Level Sequence Binding"));
    if(!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_remove_transaction_begin")))return Encode(R);
    R.bAttempted = true; R.Data->SetNumberField(TEXT("explicit_modify_calls"), 1); C.Sequence->Modify();if(!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_remove_sequence_modify")))return Encode(R);
    R.Data->SetNumberField(TEXT("explicit_modify_calls"), 2); C.Scene->Modify();if(!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_remove_scene_modify")))return Encode(R);
    R.Data->SetNumberField(TEXT("remove_possessable_calls"), 1); const bool Removed = C.Scene->RemovePossessable(C.Binding); R.Data->SetBoolField(TEXT("removed"), Removed);
    if(Camera&&!Camera->Removed(Removed)){if(C.bLeafRemoval&&!Removed)Fail(R,TEXT("sequencer_remove_possessable_failed"),TEXT("Public RemovePossessable returned false; actual attempted prefix retained, state unknown, no Unbind/save or replay."));return Encode(R);}
    if (!Removed)
    {
        auto Partial = MakeShared<FJsonObject>(); if (!MaintenanceCapture(C, R, TEXT("after_observation"), Partial)) { return Encode(R); }
        R.bChanged = !SameField(C.Before, Partial, TEXT("bindings")); Fail(R, TEXT("sequencer_remove_possessable_failed"), TEXT("Public RemovePossessable returned false after Modify2; observed prefix retained, no Unbind/save or v1 terminal contract.")); return Encode(R);
    }
    R.Data->SetNumberField(TEXT("unbind_possessable_calls"), 1); C.Sequence->UnbindPossessableObjects(C.Binding);
    if(C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval){R.Data->SetBoolField(TEXT("component_references_unbind_returned"),true);if(!Camera->Unbound())return Encode(R);}
    else if(!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_remove_unbind_return")))return Encode(R);
    R.Data->SetNumberField(TEXT("mark_package_dirty_calls"), 1); C.Sequence->MarkPackageDirty();if(!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_remove_dirty_return")))return Encode(R);
    auto After = MakeShared<FJsonObject>(); if (!MaintenanceCapture(C, R, TEXT("after_observation"), After)) { return Encode(R); }
    const bool Preserved = MaintenanceRemovalPreserved(R, C, C.Before, After, ExpectedPossessables, ExpectedReferences); R.Data->SetBoolField(TEXT("nonselected_state_preserved"), Preserved);
    R.bChanged = !SameField(C.Before, After, TEXT("bindings")); R.Data->SetBoolField(TEXT("selected_actual_changed"), R.bChanged);
    const bool ReferenceAbsent = !C.Sequence->GetBindingReferences()->HasBinding(C.Binding); R.Data->SetBoolField(TEXT("binding_references_absent"), ReferenceAbsent);
    if (!Preserved || !ReferenceAbsent) { Fail(R, TEXT("sequencer_remove_binding_postcondition_failed"), TEXT("Actual removal/reference map or unrelated declarative/retained-object state differs; save suppressed and prefix retained.")); return Encode(R); }
    auto Final = After; const bool SaveOK = MaintenanceSave(C, R, After, Final);if(Camera&&Camera->Stopped)return Encode(R);if (!Final->GetBoolField(TEXT("complete"))) { return Encode(R); }
    PublicationExpected=Final;
    auto AfterInfo = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("after_info"), AfterInfo);
    if (!MaintenanceInfo(C, AfterInfo)) { if(C.bLeafRemoval||C.bMixedRemoval||C.bHierarchyRemoval)CameraMaintenanceAbort(C.CameraMaintenance,TEXT("component_remove_terminal_info_incomplete")); R.bKnown = false; Fail(R, TEXT("sequencer_maintenance_info_unobservable"), TEXT("Terminal GUID Info projection is incomplete after actual removal/save.")); return Encode(R); }
    if(!CameraMaintenanceCheck(C.CameraMaintenance,TEXT("camera_remove_final_info")))return Encode(R);
    const bool Absent = C.Scene->FindBinding(C.Binding) == nullptr; const bool Decremented = AfterInfo->GetNumberField(TEXT("binding_count")) == BeforeInfo->GetNumberField(TEXT("binding_count")) - 1;
    const bool Observed = Absent && Decremented; const bool SaveGate = !bSave || R.bSaved; const bool Specific = Observed && SaveGate;
    R.Data->SetBoolField(TEXT("removal_observed"), Observed); auto Checks = MakeShared<FJsonObject>(); Checks->SetBoolField(TEXT("binding_existed_before"), true); Checks->SetBoolField(TEXT("binding_absent_after"), Absent); Checks->SetBoolField(TEXT("binding_count_decremented"), Decremented); Checks->SetBoolField(TEXT("save_gate"), SaveGate); R.Data->SetObjectField(TEXT("verification_predicates"), Checks);
    auto Delta = MaintenanceDelta(Observed ? 1 : 0, 0, Observed ? 1 : 0, 0, (Observed ? 0 : 1) + (SaveGate ? 0 : 1)); Delta->SetNumberField(TEXT("completion_failure_count"), SaveGate ? 0 : 1);
    if (SaveOK || !R.bSaved) { MaintenanceLegacy(R, BeforeInfo, AfterInfo, Delta, true, Specific, TEXT("level_sequence_binding_guid_relookup"), { TEXT("binding_existed_before"), TEXT("binding_absent_after"), TEXT("binding_count_decremented"), TEXT("save_gate") }); }
    R.bPost = SaveOK && Specific;
    if (!R.bPost) { if (R.Code.IsEmpty()) { Fail(R, TEXT("mutation_postcondition_failed"), TEXT("Original strict GUID/count/save verification failed after the retained removal.")); } return Encode(R); }
    R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
    };
    const FString Legacy=RunMutation();if(!Camera)return Legacy;
    Camera->Publication(PublicationExpected,nullptr);return Encode(R);
}

namespace
{
bool SubsequenceOrdinaryTrack(UMovieSceneTrack* Track, UMovieScene* Scene, bool bAllowSub,bool bAllowClip=false,const TFunction<bool()>& ClipCurrent={})
{
    if (!Loaded(Track) || Track->GetOuter() != Scene || Track->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || Track->IsEditorOnly() || Track->GetDecorations().Num() != 0) { return false; }
    const bool Sub = bAllowSub && Track->GetClass() == UMovieSceneSubTrack::StaticClass();
    const int32 ClipKind=bAllowClip?SubsequenceClipKind(Track):0;
    if (!Sub && !ClipKind && Track->GetClass() != UMovieSceneFloatTrack::StaticClass() && Track->GetClass() != UMovieScene3DTransformTrack::StaticClass()) { return false; }
    if (Track->GetAllSections().Num() >= MaxSections) { return false; }
    for (UMovieSceneSection* Section : Track->GetAllSections())
    {
        if (!Loaded(Section) || Section->GetOuter() != Track || Section->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || Section->IsEditorOnly() || Section->GetDecorations().Num() != 0) { return false; }
        if (Sub)
        {
            if (Section->GetClass() != UMovieSceneSubSection::StaticClass() || Section->GetRowIndex() < 0 || Section->GetRowIndex() >= MaxSections) { return false; }
        }
        else if(ClipKind)
        {
            auto Clip=MakeShared<FJsonObject>();if(!SubsequenceClipChannels(Section,ClipKind,Clip,ClipCurrent))return false;
        }
        else
        {
            auto Typed = MakeShared<FJsonObject>(); FMovieSceneFloatChannel* Scalar = nullptr; TArray<FMovieSceneDoubleChannel*> Transform;
            if (!(Track->GetClass() == UMovieSceneFloatTrack::StaticClass() ? FloatChannel(Section, Scalar, Typed) : TransformChannels(Section, Transform, Typed))) { return false; }
        }
    }
    return true;
}
bool SubsequenceSiblingLeafAdmission(FContext& C,FResult& R)
{
    if(!DirectorAllowed(C))return Fail(R,TEXT("sequencer_subsequence_sibling_leaf_unsupported"),TEXT("Director sibling sources remain incomplete."));
    const auto* References=C.Sequence->GetBindingReferences();
    if(!References||References->GetAllReferences().Num()>MaxBindings*16)return Fail(R,TEXT("sequencer_subsequence_sibling_reference_budget"),TEXT("Bounded existing sibling binding references are required."));
    for(const auto& Reference:References->GetAllReferences())if(Reference.CustomBinding)return Fail(R,TEXT("sequencer_subsequence_sibling_custom_binding_unsupported"),TEXT("Custom sibling bindings remain incomplete; no locator or world resolution is performed."));
    TArray<UMovieSceneTrack*> LeafTracks=C.Scene->GetTracks();for(const auto& Binding:static_cast<const UMovieScene*>(C.Scene)->GetBindings())LeafTracks.Append(Binding.GetTracks());
    if(LeafTracks.Num()>MaxTracks)return Fail(R,TEXT("sequencer_subsequence_sibling_budget_exceeded"),TEXT("Finite aggregate leaf sibling track bound required."));
    for(auto* Track:LeafTracks)if(!SubsequenceOrdinaryTrack(Track,C.Scene,false))return Fail(R,TEXT("sequencer_subsequence_sibling_leaf_unsupported"),TEXT("Only exact ordinary Float/Transform leaf source tracks are admitted; nested/cyclic/custom arrangements remain incomplete."));
    if(auto* Cut=C.Scene->GetCameraCutTrack())
    {
        if(!Loaded(Cut)||Cut->GetClass()!=UMovieSceneCameraCutTrack::StaticClass()||!CameraTrackIdentity(C,static_cast<UMovieSceneCameraCutTrack*>(Cut)))return Fail(R,TEXT("sequencer_subsequence_sibling_camera_unsupported"),TEXT("Exact bounded native nonblend auto-managed readonly sibling cuts required."));
    }
    return true;
}
bool HasDifferentSubsequenceChild(FContext& Parent,FContext& Child)
{
    if(Parent.Sequence==Child.Sequence||Parent.Scene->GetTracks().Num()>MaxTracks)return false;
    for(auto* Track:Parent.Scene->GetTracks())
    {
        if(!Loaded(Track)||Track->GetClass()!=UMovieSceneSubTrack::StaticClass()||Track->GetOuter()!=Parent.Scene)continue;
        const auto& Sections=Track->GetAllSections();if(Sections.Num()>MaxSections)return false;
        for(auto* Section:Sections)if(Loaded(Section)&&Section->GetClass()==UMovieSceneSubSection::StaticClass()&&Section->GetOuter()==Track&&static_cast<UMovieSceneSubSection*>(Section)->GetSequence()!=Child.Sequence)return true;
    }
    return false;
}
bool SubsequenceNestedInput(FContext& Parent,FContext& Child,FNestedSubsequenceRole& Role,bool& Active,FResult& R)
{
    Active=false;int32 Count=0;
    for(UMovieSceneTrack* T:Child.Scene->GetTracks())
    {
        if(!Loaded(T))return Fail(R,TEXT("sequencer_subsequence_nested_profile_unsupported"),TEXT("Current loaded child tracks required."));
        if(T->GetClass()!=UMovieSceneSubTrack::StaticClass())continue;
        if(++Count>1||T->GetOuter()!=Child.Scene||T->GetAllSections().Num()!=1)return Fail(R,TEXT("sequencer_subsequence_nested_profile_unsupported"),TEXT("Nested child requires exactly one root native SubTrack containing one native subsection."));
        Role.Track.Reset(static_cast<UMovieSceneSubTrack*>(T));UMovieSceneSection* Base=T->GetAllSections()[0];Role.Section.Reset(Base&&Base->GetClass()==UMovieSceneSubSection::StaticClass()?static_cast<UMovieSceneSubSection*>(Base):nullptr);
        if(!Loaded(Role.Section.Get())||Role.Section->GetOuter()!=T)return Fail(R,TEXT("sequencer_subsequence_nested_profile_unsupported"),TEXT("Exact direct owned child SubSection required."));
    }
    if(!Count)return true;Active=true;
    if(Parent.Sequence==Child.Sequence||Parent.Sequence->GetPackage()==Child.Sequence->GetPackage())return Fail(R,TEXT("sequencer_subsequence_nested_alias_cycle"),TEXT("Parent and Child must be distinct assets/packages before source lease or writer."));
    Role.Property=FindFProperty<FObjectProperty>(UMovieSceneSubSection::StaticClass(),TEXT("SubSequence"));Role.Offset=Role.Property?Role.Property->GetOffset_ForInternal():0;
    ULevelSequence* Leaf=nullptr;if(!Role.Binding(Role.Section.Get(),Leaf))return Fail(R,TEXT("sequencer_subsequence_nested_loaded_leaf_required"),TEXT("Child's direct reflected SubSequence slot must already be resolved to an exact loaded native LevelSequence; no loader."));
    Role.Leaf.Reset(Leaf);
    if(Leaf==Parent.Sequence||Leaf==Child.Sequence||Leaf->GetPackage()==Parent.Sequence->GetPackage()||Leaf->GetPackage()==Child.Sequence->GetPackage())return Fail(R,TEXT("sequencer_subsequence_nested_alias_cycle"),TEXT("Fixed Parent/Child/Leaf asset and package identities must be distinct; no SDK recursive cycle query."));
    Role.SourceContext=MakeUnique<FContext>();FContext& LeafContext=*Role.SourceContext;FResult Admission;if(!Resolve(Leaf->GetPathName(),LeafContext,Admission))return Fail(R,TEXT("sequencer_subsequence_nested_loaded_leaf_required"),Admission.Error);
    Role.LeafScene.Reset(LeafContext.Scene);Admission.bSaveRequested=R.bSaveRequested;if(!MaintenanceReady(LeafContext,Admission))return Fail(R,Admission.Code.IsEmpty()?TEXT("sequencer_subsequence_nested_profile_unsupported"):*Admission.Code,Admission.Error);
    if(!DirectorAllowed(Child)||!DirectorAllowed(*Role.SourceContext))return Fail(R,TEXT("sequencer_subsequence_nested_profile_unsupported"),TEXT("Director sources remain incomplete."));
    if(LeafContext.Scene->GetTracks().Num()>MaxTracks||static_cast<const UMovieScene*>(LeafContext.Scene)->GetBindings().Num()>MaxBindings)return Fail(R,TEXT("sequencer_subsequence_nested_profile_unsupported"),TEXT("Finite leaf inventory required."));
    TArray<UMovieSceneTrack*> LeafTracks=LeafContext.Scene->GetTracks();for(const auto& B:static_cast<const UMovieScene*>(LeafContext.Scene)->GetBindings())LeafTracks.Append(B.GetTracks());
    for(UMovieSceneTrack* T:LeafTracks)if(!Loaded(T)||(T->GetClass()!=UMovieSceneFloatTrack::StaticClass()&&T->GetClass()!=UMovieScene3DTransformTrack::StaticClass()))return Fail(R,TEXT("sequencer_subsequence_nested_depth_unsupported"),TEXT("Leaf root/bound tracks must be ordinary native Float/Transform; nested/deeper/cyclic/custom leaves remain incomplete."));
    if(Role.Section->Parameters.TimeScale.GetType()!=EMovieSceneTimeWarpType::FixedPlayRate)return Fail(R,TEXT("sequencer_subsequence_nested_timewarp_unsupported"),TEXT("Fixed finite play rate only; custom/fixed-time/loop timewarp is not evaluated."));
    Role.PlayRate=Role.Section->Parameters.TimeScale.AsFixedPlayRate();if(!FMath::IsFinite(Role.PlayRate))return Fail(R,TEXT("sequencer_subsequence_nested_timewarp_unsupported"),TEXT("Finite fixed play rate required."));
    Role.Bounds=Role.Section->GetRange();Role.Row=Role.Section->GetRowIndex();Role.Timecode=Role.Section->TimecodeSource;
    // New nested scope excludes additional parent child-assets. Existing flat collateral support is unchanged.
    for(UMovieSceneTrack* T:Parent.Scene->GetTracks())if(Loaded(T)&&T->GetClass()==UMovieSceneSubTrack::StaticClass())
        for(UMovieSceneSection* Base:T->GetAllSections())
        {
            ULevelSequence* Bound=nullptr;if(!Loaded(Base)||Base->GetClass()!=UMovieSceneSubSection::StaticClass()||!Role.Binding(static_cast<UMovieSceneSubSection*>(Base),Bound)||Bound!=Child.Sequence)return Fail(R,TEXT("sequencer_subsequence_nested_parent_collateral_unsupported"),TEXT("Nested profile permits existing parent subsections of selected Child only; other collateral/alias/cycle routes remain incomplete."));
        }
    R.Data->SetBoolField(TEXT("native_nested_subsequence"),true);R.Data->SetStringField(TEXT("nested_leaf_path"),Leaf->GetPathName());R.Data->SetStringField(TEXT("nested_source_scope"),TEXT("Fixed loaded exact native Parent -> Child(root SubTrack1/SubSection1) -> Leaf, separate packages; leaf ordinary Float/Transform plus existing finite CameraCut; no recursive evaluation, loader, Director or arbitrary depth. Parent-only optional save; child/leaf clean persisted required when saving, otherwise captured dirty state is preserved."));
    return true;
}
bool SubsequenceAdmission(FContext& Parent, FContext& Child, UMovieSceneSubTrack*& Selected, FResult& R)
{
    Parent.bSubsequence = true; Child.bSubsequence = true;
    if (!Native(UMovieSceneSubTrack::StaticClass(), TEXT("/Script/MovieScene.MovieSceneSubTrack")) || !Native(UMovieSceneSubSection::StaticClass(), TEXT("/Script/MovieScene.MovieSceneSubSection")))
    { return Fail(R, TEXT("sequencer_subsequence_native_source_required"), TEXT("Exact loaded native runtime SubTrack/SubSection sources required.")); }
    if (Parent.Sequence == Child.Sequence || Parent.Sequence->GetOutermost() == Child.Sequence->GetOutermost())
    { return Fail(R, TEXT("sequencer_subsequence_self_reference"), TEXT("Parent and child must be distinct native assets in separate packages; direct self/shared-package variants are refused before mutation.")); }
    if (Parent.Scene->IsReadOnly()) { return Fail(R, TEXT("sequencer_subsequence_parent_read_only"), TEXT("Current parent MovieScene must be writable before track/section dispatch; public subsection setters may refuse a read-only outer scene.")); }
    if (!DirectorAllowed(Parent) || !DirectorAllowed(Child))
    { return Fail(R, TEXT("sequencer_subsequence_variant_unsupported"), TEXT("Director Blueprint composition remains incomplete; exact bounded camera cuts are preserved separately without resolving/rebinding them.")); }
    for (FContext* Context : { &Parent, &Child })
    {
        const auto* References = Context->Sequence->GetBindingReferences();
        if (!References || References->GetAllReferences().Num() > MaxBindings * 16) { return Fail(R, TEXT("sequencer_subsequence_reference_budget_exceeded"), TEXT("Bounded existing locator references are required; no locator/world resolution runs.")); }
        for (const auto& Reference : References->GetAllReferences())
        { if (Reference.CustomBinding != nullptr) { return Fail(R, TEXT("sequencer_subsequence_custom_binding_unsupported"), TEXT("Custom binding/runtime source variants remain incomplete.")); } }
    }
    int32 SubTracks = 0;
    for (UMovieSceneTrack* Track : Parent.Scene->GetTracks())
    {
        if (!SubsequenceOrdinaryTrack(Track, Parent.Scene, true)) { return Fail(R, TEXT("sequencer_subsequence_parent_track_unsupported"), TEXT("Parent root tracks must be ordinary native Scalar/Transform or one exact SubTrack without decorations/custom channels.")); }
        if (Track->GetClass() == UMovieSceneSubTrack::StaticClass())
        {
            if (++SubTracks > 1) { return Fail(R, TEXT("sequencer_subsequence_multiple_tracks_unsupported"), TEXT("Only one current exact root SubTrack is admitted.")); }
            Selected = static_cast<UMovieSceneSubTrack*>(Track);
            for (UMovieSceneSection* Section : Track->GetAllSections())
            {
                UMovieSceneSequence* ExistingChild=static_cast<UMovieSceneSubSection*>(Section)->GetSequence();
                if(ExistingChild!=Child.Sequence)
                {
                    if(!Parent.CameraMaintenance)return Fail(R,TEXT("sequencer_subsequence_existing_child_unsupported"),TEXT("A distinct existing child requires the concrete readonly leaf source scope; nested/custom/unbounded sources remain incomplete."));
                    auto* Sibling=Loaded(ExistingChild)&&ExistingChild->GetClass()==ULevelSequence::StaticClass()?static_cast<ULevelSequence*>(ExistingChild):nullptr;
                    const FString PriorStage=R.Stage;R.Stage=TEXT("preflight_sibling");if(!Parent.CameraMaintenance->AdmitSibling(Sibling))return false;R.Stage=PriorStage;
                }
            }
        }
    }
    if (Parent.Scene->FindTrack(UMovieSceneSubTrack::StaticClass()) != Selected) { return Fail(R, TEXT("sequencer_subsequence_current_track_unobservable"), TEXT("Public first root SubTrack selection differs from the bounded current root list.")); }
    int32 ChildSections = 0;
    for (FContext* Context : { &Parent, &Child })
    {
        const auto& Bindings = static_cast<const UMovieScene*>(Context->Scene)->GetBindings();
        if (Bindings.Num() > MaxBindings || Context->Scene->GetTracks().Num() > MaxTracks) { return Fail(R, TEXT("sequencer_subsequence_budget_exceeded"), TEXT("Parent/child binding or root-track budget exceeded.")); }
        TArray<UMovieSceneTrack*> Tracks = Context == &Child ? Context->Scene->GetTracks() : TArray<UMovieSceneTrack*>();
        for (const auto& Binding : Bindings) { Tracks.Append(Binding.GetTracks()); }
        if(Context==&Child&&Context->Scene->GetCameraCutTrack())Tracks.AddUnique(Context->Scene->GetCameraCutTrack());
        if (Tracks.Num() > MaxTracks) { return Fail(R, TEXT("sequencer_subsequence_budget_exceeded"), TEXT("Parent/child aggregate track budget exceeded.")); }
        for (UMovieSceneTrack* Track : Tracks)
        {
            const bool IsCut=Track==Context->Scene->GetCameraCutTrack();
            const TFunction<bool()> ClipCurrent=Context->bSubsequenceClips?TFunction<bool()>([Context]{return CameraMaintenancePlainCheck(Context->CameraMaintenance,TEXT("subsequence_clip_admission_read_return"));}):TFunction<bool()>();
            if (IsCut ? (!Loaded(Track)||Track->GetClass()!=UMovieSceneCameraCutTrack::StaticClass()||!CameraTrackIdentity(*Context,static_cast<UMovieSceneCameraCutTrack*>(Track))) : !SubsequenceOrdinaryTrack(Track, Context->Scene, Context==&Child&&Parent.CameraMaintenance&&Parent.CameraMaintenance->Nested&&Track==Parent.CameraMaintenance->Nested->Track.Get(),Context==&Child&&Context->bSubsequenceClips,ClipCurrent))
            { return Fail(R, TEXT("sequencer_subsequence_child_track_unsupported"), TEXT("Ordinary native Scalar/Transform or admitted readonly selected-child SkeletalAnimation/Audio clips required; custom channels, wider source references and unadmitted nested variants remain incomplete.")); }
            if (Context == &Child)
            {
                ChildSections += Track->GetAllSections().Num(); if (ChildSections > MaxSections) { return Fail(R, TEXT("sequencer_subsequence_budget_exceeded"), TEXT("Child section budget exceeded.")); }
                for (UMovieSceneSection* Section : Track->GetAllSections())
                {
                    const FTimecode& Time = Section->TimecodeSource.Timecode;
                    if (Time.Hours < 0 || Time.Hours >= 24 || Time.Minutes < 0 || Time.Minutes >= 60 || Time.Seconds < 0 || Time.Seconds >= 60 || Time.Frames < 0 || Time.Frames >= 24 || Time.bDropFrameFormat || !FMath::IsFinite(Time.Subframe) || Time.Subframe < 0 || Time.Subframe >= 1)
                    { return Fail(R, TEXT("sequencer_subsequence_timecode_unsupported"), TEXT("Existing normalized non-drop child clock profile is retained; the installed default comparison rate and actual ToFrameNumber consumers are checked separately before dispatch.")); }
                }
            }
        }
    }
    if (!Parent.Sequence->IsSubSequenceCompatible(*Child.Sequence)) { return Fail(R, TEXT("sequencer_subsequence_incompatible"), TEXT("Public native sequence compatibility rejects this child before mutation.")); }
    return true;
}
// PP: mirror only the actual GetEarliestTimecodeSource comparison consumers.
// A single/nondefault-last clock is copied by the SDK without ToFrameNumber.
bool SubsequenceEarliestTimecodeSafe(FContext& Child,FResult& R)
{
    const auto Sections=Child.Scene->GetAllSections();
    TArray<TStrongObjectPtr<UMovieSceneSection>> Holds;for(UMovieSceneSection* S:Sections)if(S)Holds.Emplace(S);
    if(!CameraMaintenanceCheck(Child.CameraMaintenance,TEXT("subsequence_timecode_inventory_return")))return false;
    if(Sections.Num()>MaxSections)return Fail(R,TEXT("sequencer_subsequence_timecode_budget_exceeded"),TEXT("Finite existing child section inventory required before timecode comparison."));
    struct FClock{bool Present=false;FMovieSceneTimecodeSource Source;FString Path;};TArray<FClock> Clocks;
    for(UMovieSceneSection* S:Sections)
    {
        FClock Clock;if(S){if(!Loaded(S))return Fail(R,TEXT("sequencer_subsequence_timecode_unobservable"),TEXT("Current child timecode source is unavailable before SDK dispatch."));Clock.Present=true;Clock.Source=S->TimecodeSource;Clock.Path=S->GetPathName();}Clocks.Add(MoveTemp(Clock));
    }
    FMovieSceneTimecodeSource Earliest,DefaultSource;FString EarliestPath;int32 Index=0,Comparisons=0;
    while(Index<Clocks.Num()&&Earliest==DefaultSource){if(Clocks[Index].Present){Earliest=Clocks[Index].Source;EarliestPath=Clocks[Index].Path;}++Index;}
    const FFrameRate ComparisonRate;R.Data->SetObjectField(TEXT("subsequence_timecode_comparison_rate"),Rate(ComparisonRate));
    const int64 FramesPerSecond=FMath::CeilToInt(static_cast<float>(ComparisonRate.AsDecimal()));
    auto SafeTotal=[&](const FMovieSceneTimecodeSource& Source,const FString& Path,int64& Total)
    {
        const FTimecode& T=Source.Timecode;
        // Same existing normalized finite profile; <24 Frames is a retained admission bound, not the SDK rate.
        if(T.Hours<0||T.Hours>=24||T.Minutes<0||T.Minutes>=60||T.Seconds<0||T.Seconds>=60||T.Frames<0||T.Frames>=24||T.bDropFrameFormat||!FMath::IsFinite(T.Subframe)||T.Subframe<0||T.Subframe>=1)return Fail(R,TEXT("sequencer_subsequence_timecode_unsupported"),TEXT("Existing normalized non-drop finite child timecode profile required before actual SDK comparison."));
        const int64 PerMinute=FramesPerSecond*60,PerHour=PerMinute*60;
        Total=static_cast<int64>(T.Hours)*PerHour+static_cast<int64>(T.Minutes)*PerMinute+static_cast<int64>(T.Seconds)*FramesPerSecond+T.Frames;
        if(FramesPerSecond<=0||PerHour>MAX_int32||Total>MAX_int32)
        {
            auto Detail=MakeShared<FJsonObject>();Detail->SetStringField(TEXT("stage"),TEXT("before_earliest_timecode_comparison"));Detail->SetStringField(TEXT("object"),Path);Detail->SetStringField(TEXT("property"),TEXT("TimecodeSource.Timecode.ToFrameNumber"));Detail->SetStringField(TEXT("wide_total_frames"),LexToString(Total));Detail->SetNumberField(TEXT("comparison_fps"),FramesPerSecond);Detail->SetNumberField(TEXT("maximum_int32"),MAX_int32);R.Data->SetObjectField(TEXT("timecode_first_failure"),Detail);
            return Fail(R,TEXT("sequencer_subsequence_timecode_arithmetic_unsupported"),TEXT("Actual GetEarliestTimecodeSource comparison would exceed the installed ToFrameNumber int32 arithmetic; no writer, save or timecode conversion is dispatched."));
        }
        return true;
    };
    for(;Index<Clocks.Num();++Index)
    {
        if(!Clocks[Index].Present)continue;int64 CandidateTotal=0,EarliestTotal=0;
        if(!SafeTotal(Clocks[Index].Source,Clocks[Index].Path,CandidateTotal)||!SafeTotal(Earliest,EarliestPath,EarliestTotal))return false;
        ++Comparisons;if(CandidateTotal<EarliestTotal){Earliest=Clocks[Index].Source;EarliestPath=Clocks[Index].Path;}
    }
    R.Data->SetNumberField(TEXT("subsequence_timecode_comparisons_planned"),Comparisons);return true;
}
bool SubsequenceChildPreserved(FContext& Parent, const TSharedRef<FJsonObject>& BeforeState, const TSharedRef<FJsonObject>& AfterState)
{
    auto Expected = MakeShared<FJsonObject>(); Expected->Values = BeforeState->Values; auto Other = MakeShared<FJsonObject>(); Other->Values = BeforeState->GetObjectField(TEXT("other_package_dirty"))->Values;
    const FString Package = Parent.Sequence->GetOutermost()->GetPathName();
    if (!Other->HasTypedField<EJson::Boolean>(Package)) { return false; }
    Other->SetBoolField(Package, Parent.Sequence->GetOutermost()->IsDirty()); Expected->SetObjectField(TEXT("other_package_dirty"), Other); return CameraLiteral(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(AfterState),0);
}
bool SubsequenceTrackAcquired(FContext& C, UMovieSceneSubTrack* Track, bool bCreated, const TSharedRef<FJsonObject>& After)
{
    if (!Loaded(Track) || Track->GetClass() != UMovieSceneSubTrack::StaticClass() || Track->GetOuter() != C.Scene || C.Scene->FindTrack(UMovieSceneSubTrack::StaticClass()) != Track || !C.Scene->GetTracks().Contains(Track)) { return false; }
    auto Expected = MakeShared<FJsonObject>(); Expected->Values = C.Before->Values;
    if (bCreated)
    {
        if (Track->GetAllSections().Num() != 0 || C.Before->GetObjectField(TEXT("tracks"))->HasField(Track->GetPathName())) { return false; }
        TArray<TSharedPtr<FJsonValue>> Roots = C.Before->GetArrayField(TEXT("root_tracks")); Roots.Add(MakeShared<FJsonValueString>(Track->GetPathName())); Expected->SetArrayField(TEXT("root_tracks"), Roots);
        auto Tracks = MakeShared<FJsonObject>(); Tracks->Values = C.Before->GetObjectField(TEXT("tracks"))->Values; Tracks->SetField(Track->GetPathName(), After->GetObjectField(TEXT("tracks"))->TryGetField(Track->GetPathName())); Expected->SetObjectField(TEXT("tracks"), Tracks);
        Expected->SetNumberField(TEXT("track_count"), C.Before->GetNumberField(TEXT("track_count")) + 1);
        FArrayProperty* Property = FindFProperty<FArrayProperty>(C.Scene->GetClass(), TEXT("Tracks")); FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
        if (!Property || Property->GetOwnerClass() != UMovieScene::StaticClass() || !Inner || Inner->PropertyClass != UMovieSceneTrack::StaticClass()) { return false; }
        TArray<TObjectPtr<UMovieSceneTrack>> RootCopy; for (const auto& Row : C.Before->GetArrayField(TEXT("root_tracks"))) { auto* Old = FindObject<UMovieSceneTrack>(nullptr, *Row->AsString()); if (!Loaded(Old)) { return false; } RootCopy.Add(Old); } RootCopy.Add(Track);
        auto Exported = MakeShared<FJsonObject>(); if (!MaintenanceExportExpected(Property, &RootCopy, C.Scene, Exported)) { return false; }
        auto Fields = MakeShared<FJsonObject>(); Fields->Values = C.Before->GetObjectField(TEXT("movie_scene_fields"))->Values; Fields->SetObjectField(TEXT("Tracks"), Exported); Expected->SetObjectField(TEXT("movie_scene_fields"), Fields);
    }
    auto Actual = MakeShared<FJsonObject>(); Actual->Values = After->Values;
    for (const TCHAR* Key : { TEXT("dirty"), TEXT("sequence_signature"), TEXT("movie_scene_signature") }) { Expected->RemoveField(Key); Actual->RemoveField(Key); }
    if(C.CameraMaintenance)
    {
        // Root tracks are observed before the separate CameraCut track. Adding
        // a root SubTrack changes JSON object enumeration, not protected data.
        const bool SerializedSame=Same(Expected,Actual);
        const bool LiteralSame=CameraLiteral(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Actual),0);
        C.CameraMaintenance->R.Data->SetBoolField(TEXT("camera_subsequence_track_serialized_equal"),SerializedSame);
        C.CameraMaintenance->R.Data->SetBoolField(TEXT("camera_subsequence_track_literal_equal"),LiteralSame);
        return LiteralSame;
    }
    return Same(Expected,Actual);
}
bool SubsequencePriorSectionsPreserved(FContext& C, UMovieSceneSubTrack* Track, UMovieSceneSubSection* Added, const TSharedRef<FJsonObject>& Acquired, const TSharedRef<FJsonObject>& After)
{
    if (!Loaded(Track) || Track->GetOuter() != C.Scene || !Loaded(Added) || Added->GetOuter() != Track) { return false; }
    auto Expected = MakeShared<FJsonObject>(); Expected->Values = Acquired->Values;
    auto Tracks = MakeShared<FJsonObject>(); Tracks->Values = Acquired->GetObjectField(TEXT("tracks"))->Values;
    const auto BeforeTrack = Tracks->GetObjectField(Track->GetPathName()); auto ExpectedTrack = MakeShared<FJsonObject>(); ExpectedTrack->Values = BeforeTrack->Values;
    TArray<TSharedPtr<FJsonValue>> Sections = BeforeTrack->GetArrayField(TEXT("sections"));
    const auto CurrentTrack = After->GetObjectField(TEXT("tracks"))->GetObjectField(Track->GetPathName()); const auto& CurrentSections = CurrentTrack->GetArrayField(TEXT("sections"));
    if (CurrentSections.Num() != Sections.Num() + 1 || CurrentSections.Last()->AsObject()->GetStringField(TEXT("path")) != Added->GetPathName()) { return false; }
    Sections.Add(CurrentSections.Last()); ExpectedTrack->SetArrayField(TEXT("sections"), Sections);
    FArrayProperty* Property = FindFProperty<FArrayProperty>(Track->GetClass(), TEXT("Sections")); FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
    if (!Property || Property->GetOwnerClass() != UMovieSceneSubTrack::StaticClass() || !Inner || Inner->PropertyClass != UMovieSceneSection::StaticClass()) { return false; }
    TArray<TObjectPtr<UMovieSceneSection>> SectionCopy;
    for (const auto& Value : BeforeTrack->GetArrayField(TEXT("sections"))) { auto* Old = FindObject<UMovieSceneSection>(nullptr, *Value->AsObject()->GetStringField(TEXT("path"))); if (!Loaded(Old)) { return false; } SectionCopy.Add(Old); } SectionCopy.Add(Added);
    auto Exported = MakeShared<FJsonObject>(); if (!MaintenanceExportExpected(Property, &SectionCopy, Track, Exported)) { return false; }
    auto Fields = MakeShared<FJsonObject>(); Fields->Values = BeforeTrack->GetObjectField(TEXT("fields"))->Values; Fields->SetObjectField(TEXT("Sections"), Exported); ExpectedTrack->SetObjectField(TEXT("fields"), Fields); Tracks->SetObjectField(Track->GetPathName(), ExpectedTrack);
    Expected->SetObjectField(TEXT("tracks"), Tracks); Expected->SetNumberField(TEXT("section_count"), Acquired->GetNumberField(TEXT("section_count")) + 1);
    auto Actual = MakeShared<FJsonObject>(); Actual->Values = After->Values;
    for (const TCHAR* Key : { TEXT("dirty"), TEXT("sequence_signature"), TEXT("movie_scene_signature") }) { Expected->RemoveField(Key); Actual->RemoveField(Key); } return CameraLiteral(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Actual),0);
}
TSharedRef<FJsonObject> SubsequenceSectionObservation(UMovieSceneSubSection* Section,FCameraMaintenanceScope* Scope=nullptr)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("complete"), false);
    if (!Loaded(Section) || Section->GetClass() != UMovieSceneSubSection::StaticClass()) { return J; }
    J->SetObjectField(TEXT("identity"), Identity(Section)); auto Fields = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("fields"), Fields); const TFunction<bool()> Current=Scope&&Scope->Nested?TFunction<bool()>([Scope]{return Scope->Check(TEXT("nested_returned_section_export"),false,false);}):TFunction<bool()>();if (!ObjectProperties(Section, {}, Fields,Current)) { if(Scope&&Scope->Nested)Scope->Stop(TEXT("nested_returned_section_observation_incomplete"));return J; }
    if(!CameraMaintenanceCheck(Scope,TEXT("camera_subsequence_section_fields_return")))return J;
    J->SetObjectField(TEXT("range"), Range(Section->GetRange())); J->SetNumberField(TEXT("row_index"), Section->GetRowIndex()); J->SetBoolField(TEXT("read_only"), Section->IsReadOnly());
    UMovieSceneSequence* Inner = Section->GetSequence(); J->SetStringField(TEXT("subsequence"), Loaded(Inner) ? Inner->GetPathName() : TEXT("")); J->SetBoolField(TEXT("complete"), true); return J;
}
}

FString AddSubsequence(const FString& SequencePath, const FString& SubsequencePath, double StartFrame, double DurationFrames, bool bSave)
{
    FResult R; InitMaintenance(R, TEXT("add_level_sequence_subsequence"), bSave); FContext Parent, Child;
    for (const TCHAR* Key : { TEXT("track_create_attempts"), TEXT("section_create_attempts"), TEXT("add_sequence_on_row_calls") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetBoolField(TEXT("created_track"), false); R.Data->SetBoolField(TEXT("created_section"), false); R.Data->SetBoolField(TEXT("subsequence_state_preserved"), false);
    R.Data->SetStringField(TEXT("changed_observation_scope"), TEXT("Actual parent root-track/current-section membership delta; full parent/child snapshots preserve dirty/signature/file/class-basis changes. Original subsequence handler had no mutation_result_v1 and its legacy success did not verify assignment/save; this finite typed result independently verifies these boundaries."));
    R.Stage = TEXT("preflight_parent"); if (!Resolve(SequencePath, Parent, R,true) || !MaintenanceReady(Parent, R)) { return Encode(R); }
    R.Stage = TEXT("preflight_subsequence"); if (!Resolve(SubsequencePath, Child, R,true) || !MaintenanceReady(Child, R)) { return Encode(R); }
    if(!PrimaryResolvedCurrent(Parent,R)||!PrimaryResolvedCurrent(Child,R))return Encode(R);
    R.Data->SetStringField(TEXT("sequence_path"), Parent.Sequence->GetPathName()); R.Data->SetStringField(TEXT("movie_scene_path"), Parent.Scene->GetPathName()); R.Data->SetStringField(TEXT("subsequence_path"), Child.Sequence->GetPathName()); R.Data->SetStringField(TEXT("subsequence_movie_scene_path"), Child.Scene->GetPathName());
    R.Data->SetStringField(TEXT("sequence"), Parent.Sequence->GetPathName()); R.Data->SetStringField(TEXT("subsequence"), Child.Sequence->GetPathName());
    FFrameNumber Start, End; const double EndFrame = StartFrame + DurationFrames;
    if (!FMath::IsFinite(DurationFrames) || DurationFrames <= 0 || !MaintenanceRequestedTimeSafe(StartFrame, Parent) || !MaintenanceRequestedTimeSafe(EndFrame, Parent) || !Tick(StartFrame, Parent, Start) || !Tick(EndFrame, Parent, End))
    { Fail(R, TEXT("sequencer_subsequence_range_invalid"), TEXT("Positive finite duration and original start/end floor conversions within native int64/int32 budgets required.")); return Encode(R); }
    const int64 WideDuration = static_cast<int64>(End.Value) - Start.Value;
    if (WideDuration < 0 || WideDuration > MAX_int32 || static_cast<int64>(Start.Value) + WideDuration != End.Value)
    { Fail(R, TEXT("sequencer_subsequence_duration_out_of_range"), TEXT("Native duration and StartTick+Duration must fit signed int32 before AddSequenceOnRow; zero floored duration is retained as an original valid quantization case.")); return Encode(R); }
    R.Data->SetNumberField(TEXT("start_frame"), StartFrame); R.Data->SetNumberField(TEXT("duration_frames"), DurationFrames);
    R.Data->SetNumberField(TEXT("start_tick"), Start.Value); R.Data->SetNumberField(TEXT("end_tick"), End.Value); R.Data->SetNumberField(TEXT("duration_ticks"), WideDuration);
    // Discover only the original selected Child, before any reflected snapshot.
    // Existing parent/sibling/nested leaf scope is deliberately unchanged.
    TArray<UMovieSceneTrack*> ClipInventory=Child.Scene->GetTracks();
    const TArray<FMovieSceneBinding> ClipBindings=static_cast<const UMovieScene*>(Child.Scene)->GetBindings();
    for(const auto& Binding:ClipBindings){if(ClipInventory.Num()>MaxTracks)break;ClipInventory.Append(Binding.GetTracks());}
    if(ClipInventory.Num()>MaxTracks){Fail(R,TEXT("sequencer_subsequence_budget_exceeded"),TEXT("Bounded selected-child root/bound inventory required before clip source capture."));return Encode(R);}
    bool HasClips=false;for(auto* Candidate:ClipInventory)HasClips|=SubsequenceClipKind(Candidate)!=0;
    if(HasClips){Parent.bSubsequenceClips=true;Child.bSubsequenceClips=true;}
    FNestedSubsequenceRole Nested;bool NestedActive=false;if(!SubsequenceNestedInput(Parent,Child,Nested,NestedActive,R))return Encode(R);
    if(NestedActive){Parent.bNestedSubsequence=true;Child.bNestedSubsequence=true;Parent.bSubsequence=true;Child.bSubsequence=true;}
    TUniquePtr<FCameraMaintenanceScope> Camera;
    const bool DifferentSibling=HasDifferentSubsequenceChild(Parent,Child);
    if(DifferentSibling)R.Data->SetBoolField(TEXT("different_child_sibling_requested"),true);
    if(HasClips||NestedActive||Parent.Scene->GetCameraCutTrack()||Child.Scene->GetCameraCutTrack()||DifferentSibling||Parent.Director.IsValid()||Child.Director.IsValid()){Camera=MakeUnique<FCameraMaintenanceScope>(Parent,&Child,R);Camera->Nested=NestedActive?&Nested:nullptr;if(!Camera->Bind())return Encode(R);}
    if(NestedActive&&!Camera->AdmitSibling(Nested.Leaf.Get()))return Encode(R);
    TSharedPtr<FJsonObject> PublicationExpected,SourceExpected;
    auto RunMutation=[&]()->FString
    {
    if(Camera)
    {
        auto A=MakeShared<FJsonObject>(),B=MakeShared<FJsonObject>();
        if(NestedActive)
        {
            if(!MaintenanceCapture(Child,R,TEXT("nested_child_source_before"),B))return Encode(R);
            Camera->NestedChildBefore=B;R.Data->SetObjectField(TEXT("nested_child_source_before"),B);
            if(!MaintenanceCapture(Parent,R,TEXT("camera_subsequence_parent_baseline"),A))return Encode(R);
        }
        else if(!MaintenanceCapture(Parent,R,TEXT("camera_subsequence_parent_baseline"),A)||!MaintenanceCapture(Child,R,TEXT("camera_subsequence_child_baseline"),B))return Encode(R);
    }
    UMovieSceneSubTrack* Track = nullptr; if (!SubsequenceAdmission(Parent, Child, Track, R)) { return Encode(R); }
    if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_admission_return")))return Encode(R);
    if (!MaintenanceCapture(Parent, R, TEXT("before_observation"), Parent.Before) || !MaintenanceCapture(Child, R, TEXT("subsequence_before_observation"), Child.Before)) { return Encode(R); }
    if (!Child.Before->GetObjectField(TEXT("other_package_dirty"))->HasTypedField<EJson::Boolean>(Parent.Sequence->GetOutermost()->GetPathName())) { Fail(R, TEXT("sequencer_subsequence_source_baseline_unobservable"), TEXT("Exact parent dirty row must be present in child baseline before mutation.")); return Encode(R); }
    if (bSave && (!Child.Before->GetObjectField(TEXT("disk"))->GetBoolField(TEXT("exists")) || Child.Before->GetObjectField(TEXT("disk"))->GetNumberField(TEXT("bytes")) <= 0 || Child.Sequence->GetOutermost()->IsDirty()))
    { Fail(R, TEXT("sequencer_subsequence_reference_not_persisted"), TEXT("Requested parent save requires a clean nonempty persisted child dependency; no child writer/save is dispatched here.")); return Encode(R); }
    const int32 PreviousSections = Track ? Track->GetAllSections().Num() : 0;
    if (Parent.Before->GetNumberField(TEXT("section_count")) >= MaxSections || (!Track && Parent.Before->GetNumberField(TEXT("track_count")) >= MaxTracks)) { Fail(R, TEXT("sequencer_subsequence_budget_exceeded"), TEXT("Requested allocation must fit aggregate track/section budgets before mutation.")); return Encode(R); }
    const TRange<FFrameNumber> ExpectedRange(Start, End); int32 ExpectedRow = 0;
    if (Track)
    {
        for (; ExpectedRow <= PreviousSections; ++ExpectedRow)
        {
            bool Overlap = false; for (UMovieSceneSection* Existing : Track->GetAllSections()) { Overlap |= Existing->GetRowIndex() == ExpectedRow && ExpectedRange.Overlaps(Existing->GetRange()); } if (!Overlap) { break; }
        }
    }
    if(!SubsequenceEarliestTimecodeSafe(Child,R))return Encode(R);
    const FMovieSceneTimecodeSource ExpectedTimecode = Child.Sequence->GetEarliestTimecodeSource();
    if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_timecode_return")))return Encode(R);
    R.Data->SetNumberField(TEXT("expected_row_index"), ExpectedRow); R.Data->SetNumberField(TEXT("section_count_before"), PreviousSections);
    R.Stage = TEXT("mutate"); R.Data->SetNumberField(TEXT("outer_transaction_calls"), 1); FScopedTransaction Transaction(NSLOCTEXT("CubelessSequencer", "AddSubsequence", "Add Level Sequence Subsequence"));
    if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_transaction_begin")))return Encode(R);
    R.bAttempted = true; R.Data->SetNumberField(TEXT("explicit_modify_calls"), 1); Parent.Sequence->Modify();if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_sequence_modify")))return Encode(R);
    R.Data->SetNumberField(TEXT("explicit_modify_calls"), 2); Parent.Scene->Modify();if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_scene_modify")))return Encode(R);
    bool CreatedTrack = false;
    if (!Track) { R.Data->SetNumberField(TEXT("track_create_attempts"), 1); Track = Cast<UMovieSceneSubTrack>(Parent.Scene->AddTrack(UMovieSceneSubTrack::StaticClass())); CreatedTrack = Track != nullptr; R.Data->SetBoolField(TEXT("created_track"), CreatedTrack); }
    TStrongObjectPtr<UMovieSceneSubTrack> KeepTrack(Track);
    if(Camera&&!Camera->AdoptTrack(Track,CreatedTrack))return Encode(R); auto Acquired = MakeShared<FJsonObject>();
    if (!MaintenanceCapture(Parent, R, TEXT("after_track_acquisition"), Acquired)) { return Encode(R); }
    R.bChanged = !SameField(Parent.Before, Acquired, TEXT("root_tracks"));
    if (!SubsequenceTrackAcquired(Parent, Track, CreatedTrack, Acquired)) { Fail(R, TEXT("sequencer_subsequence_track_postcondition_failed"), TEXT("Exact current root native SubTrack/public factory prefix or protected parent state differs; no section/save retry.")); return Encode(R); }
    auto ChildAcquired = MakeShared<FJsonObject>();
    if (!MaintenanceCapture(Child, R, TEXT("subsequence_after_track_acquisition_observation"), ChildAcquired)) { return Encode(R); }
    const bool ChildAcquisitionOK = SubsequenceChildPreserved(Parent, Child.Before, ChildAcquired); R.Data->SetBoolField(TEXT("subsequence_after_track_acquisition_state_preserved"), ChildAcquisitionOK);
    if (!ChildAcquisitionOK) { Fail(R, TEXT("sequencer_subsequence_source_preservation_failed"), TEXT("Root track acquisition retained but child/source/disk changed; section creation and save are suppressed.")); return Encode(R); }
    R.Data->SetStringField(TEXT("track_path"), Track->GetPathName()); R.Data->SetNumberField(TEXT("explicit_modify_calls"), 3); Track->Modify();
    if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_track_modify")))return Encode(R);
    R.Data->SetNumberField(TEXT("section_create_attempts"), 1); R.Data->SetNumberField(TEXT("add_sequence_on_row_calls"), 1); UMovieSceneSubSection* Section = Track->AddSequenceOnRow(Child.Sequence, Start, static_cast<int32>(WideDuration), INDEX_NONE);
    TStrongObjectPtr<UMovieSceneSubSection> KeepSection(Section);
    R.Data->SetBoolField(TEXT("section_allocation_returned"),Section!=nullptr);if(Camera&&!Camera->AdoptSection(Track,Section))return Encode(R);
    R.Data->SetBoolField(TEXT("section_allocation_returned"), Section != nullptr); auto SectionProof = SubsequenceSectionObservation(Section,Camera.Get()); R.Data->SetObjectField(TEXT("returned_subsection"), SectionProof);
    if(NestedActive&&Camera->Stopped)return Encode(R);
    if (Loaded(Section)) { R.Data->SetStringField(TEXT("section_path"), Section->GetPathName()); }
    if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_section_observation_return")))return Encode(R);
    if (Section && Loaded(Parent.Sequence)) { R.Data->SetNumberField(TEXT("mark_package_dirty_calls"), 1); Parent.Sequence->MarkPackageDirty(); }
    if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_dirty_return")))return Encode(R);
    auto After = MakeShared<FJsonObject>(), ChildAfter = MakeShared<FJsonObject>();
    if (!MaintenanceCapture(Parent, R, TEXT("after_observation"), After) || !MaintenanceCapture(Child, R, TEXT("subsequence_after_observation"), ChildAfter)) { return Encode(R); }
    R.bChanged = !SameField(Parent.Before, After, TEXT("root_tracks")) || !SameField(Parent.Before, After, TEXT("tracks"));
    bool Current = Loaded(Track) && Track->GetClass() == UMovieSceneSubTrack::StaticClass() && Track->GetOuter() == Parent.Scene && !Track->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) && !Track->IsEditorOnly() && Parent.Scene->FindTrack(UMovieSceneSubTrack::StaticClass()) == Track && Parent.Scene->GetTracks().Contains(Track) && Loaded(Section) && Section->GetClass() == UMovieSceneSubSection::StaticClass() && Section->GetOuter() == Track && !Section->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) && !Section->IsEditorOnly() && !Section->IsReadOnly() && Track->GetAllSections().Contains(Section);
    const bool Assigned = Current && Section->GetSequence() == Child.Sequence, RangeOK = Current && Section->GetRange() == ExpectedRange, RowOK = Current && Section->GetRowIndex() == ExpectedRow, TimecodeOK = Current && Section->TimecodeSource == ExpectedTimecode;
    const bool Preserved = Current && SectionProof->GetBoolField(TEXT("complete")) && SubsequencePriorSectionsPreserved(Parent, Track, Section, Acquired, After);
    if(!CameraMaintenanceCheck(Parent.CameraMaintenance,TEXT("camera_subsequence_expected_export_return")))return Encode(R);
    const bool ChildOK = SubsequenceChildPreserved(Parent, Child.Before, ChildAfter);
    R.Data->SetBoolField(TEXT("current_subsection_verified"), Current); R.Data->SetBoolField(TEXT("subsequence_assignment_verified"), Assigned); R.Data->SetBoolField(TEXT("subsequence_floor_range_verified"), RangeOK); R.Data->SetBoolField(TEXT("subsequence_row_verified"), RowOK); R.Data->SetBoolField(TEXT("subsequence_timecode_verified"), TimecodeOK); R.Data->SetBoolField(TEXT("nonselected_state_preserved"), Preserved); R.Data->SetBoolField(TEXT("subsequence_state_preserved"), ChildOK);
    R.Data->SetBoolField(TEXT("created_section"), Current && Track->GetAllSections().Num() == PreviousSections + 1); R.Data->SetNumberField(TEXT("section_count_after"), Loaded(Track) ? Track->GetAllSections().Num() : 0);
    if (!Current || !Assigned || !RangeOK || !RowOK || !TimecodeOK || !Preserved || !ChildOK)
    { Fail(R, TEXT("sequencer_subsequence_postcondition_failed"), TEXT("Actual section assignment/current membership/floor range/row/timecode or parent/child preservation differs. SetSequence may have refused compatibility/TryModify; complete retained prefix is returned and save suppressed.")); return Encode(R); }
    auto Final = After; bool SaveOK = true;
    if (bSave) { R.Data->SetBoolField(TEXT("after_observation_subsequence_state_preserved"), ChildOK); R.Data->SetBoolField(TEXT("subsequence_state_preserved"), false); SaveOK = MaintenanceSave(Parent, R, After, Final); }
    if(!SaveOK&&!R.bKnown)
    {
        const FString SaveCode=R.Code,SaveError=R.Error,SaveStage=R.Stage;
        if(Camera)Camera->Stop(TEXT("subsequence_unknown_save_return"));
        R.Code=SaveCode;R.Error=SaveError;R.Stage=SaveStage;return Encode(R);
    }
    if(Camera&&Camera->Stopped)return Encode(R);
    SourceExpected=ChildAfter;
    if (bSave)
    {
        auto ChildSaved = MakeShared<FJsonObject>(); const FString PriorCode = R.Code, PriorError = R.Error, PriorStage = R.Stage;
        if (!MaintenanceCapture(Child, R, TEXT("subsequence_after_save_observation"), ChildSaved)) { R.Data->SetStringField(TEXT("prior_save_error_code"), PriorCode); R.Data->SetStringField(TEXT("prior_save_error"), PriorError); R.Data->SetStringField(TEXT("prior_save_stage"), PriorStage); return Encode(R); }
        R.Data->SetBoolField(TEXT("observations_complete"), Final->GetBoolField(TEXT("complete"))); const bool SourcePreserved = SubsequenceChildPreserved(Parent, Child.Before, ChildSaved); R.Data->SetBoolField(TEXT("subsequence_state_preserved"), SourcePreserved);
        SourceExpected=ChildSaved;
        if (!SourcePreserved) { SaveOK = false; Fail(R, TEXT("sequencer_subsequence_source_preservation_failed"), TEXT("Parent save prefix retained but actual child/source/disk or other dirty state differs; no child writer or retry.")); }
    }
    PublicationExpected=Final;
    R.bPost = SaveOK && (!bSave || R.bSaved); R.Data->SetBoolField(TEXT("postcondition_verified"), R.bPost);
    R.Data->SetStringField(TEXT("legacy_success_scope"), TEXT("Original AddSubsequence returned success after nonnull section and did not emit mutation_result_v1 or independently verify assignment/save. Original output fields are retained; new typed success additionally requires complete current parent/child/save proof."));
    if (!R.bPost) { return Encode(R); } R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
    };
    const FString Legacy=RunMutation();if(!Camera)return Legacy;
    Camera->Publication(PublicationExpected,SourceExpected);return Encode(R);
}

namespace
{
const TArray<UMovieSceneSection*>& CameraSections(UMovieSceneCameraCutTrack* Track)
{
    // CameraCutTrack is MinimalAPI: use the public exported base virtual route.
    return static_cast<UMovieSceneTrack*>(Track)->GetAllSections();
}
bool CameraTrackIdentity(FContext& C, UMovieSceneCameraCutTrack* Track)
{
    return Loaded(Track) && Track->GetClass() == UMovieSceneCameraCutTrack::StaticClass() && Track->GetOuter() == C.Scene
        && !Track->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) && !Track->IsEditorOnly()
        && C.Scene->GetCameraCutTrack() == Track && Track->GetDecorations().Num() == 0 && !Track->bCanBlend && Track->IsAutoManagingSections();
}
bool CameraSectionIdentity(UMovieSceneCameraCutTrack* Track, UMovieSceneCameraCutSection* Section)
{
    return Loaded(Section) && Section->GetClass() == UMovieSceneCameraCutSection::StaticClass() && Section->GetOuter() == Track
        && !Section->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) && !Section->IsEditorOnly()
        && Section->GetDecorations().Num() == 0 && !Section->IsReadOnly() && Section->GetRowIndex() == 0;
}
TSharedRef<FJsonObject> CameraRetainedSection(UMovieSceneCameraCutSection* Section,const TFunction<bool()>& Current={})
{
    auto Row = MakeShared<FJsonObject>(); Row->SetBoolField(TEXT("complete"), false); Row->SetBoolField(TEXT("exists"), Section != nullptr);
    if(Current&&!Current())return Row;
    if (!Section) { Row->SetBoolField(TEXT("complete"), true); return Row; }
    if (!Loaded(Section) || Section->GetClass() != UMovieSceneCameraCutSection::StaticClass()) { return Row; }
    Row->SetObjectField(TEXT("identity"), Identity(Section)); auto Fields = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("fields"), Fields);
    if (!ObjectProperties(Section, {}, Fields,Current)) { return Row; }
    Row->SetObjectField(TEXT("range"), Range(Section->GetRange())); Row->SetNumberField(TEXT("row_index"), Section->GetRowIndex());
    Row->SetStringField(TEXT("binding_guid"), Section->GetCameraBindingID().GetGuid().ToString(EGuidFormats::DigitsWithHyphens));
    Row->SetBoolField(TEXT("complete"), true); return Row;
}
// JA: original missing native insertion/previous-neighbor adjustment. The
// AddNewCameraCut SDK body is opaque: no callback purity/atomicity claim.
bool CameraLiteral(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,int32 Depth=0)
{
    if(!A||!B||A->Type!=B->Type||Depth>32)return false;
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();
    case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num())return false;for(int32 I=0;I<X.Num();++I)if(!CameraLiteral(X[I],Y[I],Depth+1))return false;return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X||!Y||X->Values.Num()!=Y->Values.Num())return false;
        TArray<FString> XKeys,YKeys;for(const auto& P:X->Values)XKeys.Add(FString(P.Key.ToView()));for(const auto& P:Y->Values)YKeys.Add(FString(P.Key.ToView()));
        const auto Order=[](const FString& L,const FString& Rhs){return L.Compare(Rhs,ESearchCase::CaseSensitive)<0;};XKeys.Sort(Order);YKeys.Sort(Order);
        for(int32 I=0;I<XKeys.Num();++I)if(!XKeys[I].Equals(YKeys[I],ESearchCase::CaseSensitive)||!CameraLiteral(X->TryGetField(XKeys[I]),Y->TryGetField(YKeys[I]),Depth+1))return false;return true;}
    default:return false;
    }
}
// YK: value-only projection of FindEndTimeForCameraCut (installed SDK).
// Keep original physical order: the first future start wins, not the nearest.
// Checked narrowing surrounds, rather than changes, the exact SDK arithmetic.
bool CameraInitialEnd(const TRange<FFrameNumber>& Playback,const FFrameRate& Resolution,FFrameNumber Start,
    const TArray<TRange<FFrameNumber>>& PhysicalRanges,FFrameNumber& Out,FResult& R)
{
    if(!Playback.GetLowerBound().IsInclusive()||Playback.GetUpperBound().IsOpen())
        return Fail(R,TEXT("sequencer_camera_initial_range_unsupported"),TEXT("Finite playback with inclusive lower and closed exclusive/inclusive upper required; open endpoints remain incomplete."));
    const int64 ExclusiveEnd=int64(Playback.GetUpperBoundValue().Value)+(Playback.GetUpperBound().IsInclusive()?1:0);
    if(ExclusiveEnd>MAX_int32)
        return Fail(R,TEXT("sequencer_camera_initial_end_overflow"),TEXT("Inclusive playback upper cannot be advanced safely by the native discrete-exclusive conversion; no mutation dispatched."));
    Out=FMath::Max(UE::MovieScene::DiscreteExclusiveUpper(Playback),Start);
    for(const auto& RangeValue:PhysicalRanges)
    {
        if(RangeValue.GetLowerBoundValue()>Start){Out=RangeValue.GetLowerBoundValue();break;}
    }
    bool Fallback=false;
    if(Start==Out)
    {
        if(Resolution.Numerator<=0||Resolution.Denominator<=0)
            return Fail(R,TEXT("sequencer_camera_fallback_rate_invalid"),TEXT("Positive native tick resolution required for the SDK half-second fallback."));
        // Positive int32 numerator/denominator bounds keep AsFrameTime(.5f)
        // below int32 saturation, including its float subframe normalization.
        const FFrameTime HalfSecond=.5f*Resolution;
        const int64 Sum=int64(Start.Value)+int64(HalfSecond.FrameNumber.Value);
        if(Sum<MIN_int32||Sum>MAX_int32)
            return Fail(R,TEXT("sequencer_camera_fallback_overflow"),TEXT("Native half-second fallback exceeds checked frame-number storage; no mutation dispatched."));
        Out=(Start+HalfSecond).FrameNumber;Fallback=true;
    }
    R.Data->SetNumberField(TEXT("camera_sdk_initial_end_tick"),Out.Value);
    R.Data->SetBoolField(TEXT("camera_sdk_half_second_fallback"),Fallback);
    R.Data->SetBoolField(TEXT("camera_sdk_inclusive_playback_upper"),Playback.GetUpperBound().IsInclusive());
    return true;
}
struct FCameraInsertionScope
{
    FContext& C;FResult& R;TWeakObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;FString WorldPath,PackagePath;
    UMovieSceneCameraCutTrack* Track=nullptr;TArray<UMovieSceneCameraCutSection*> Prior,ExpectedOrder;
    TArray<TRange<FFrameNumber>> PriorRanges,ExpectedRanges;TArray<FMovieSceneObjectBindingID> Bindings;
    UMovieSceneCameraCutSection* NewSection=nullptr;FFrameNumber Start,End,InitialEnd;int32 Index=0;bool Stopped=false,ExactReuse=false;
    struct FReuseSignature{UMovieSceneSignedObject* Object=nullptr;FGuid Before;bool MayChange=false;};TArray<FReuseSignature> ReuseSignatures;
    TRange<FFrameNumber> Playback;FFrameRate Display,Resolution;FGuid SelectedGuid,SelectedParent;FString SelectedName,SelectedClassPath;bool Possessable=false;
    FMovieSceneObjectBindingID SelectedSpawnable;TStrongObjectPtr<const UObject> BindingSource;
    struct FIdentity{TStrongObjectPtr<UObject> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;};TArray<FIdentity> Identities;
    FCameraInsertionScope(FContext& In,FResult& Result):C(In),R(Result){}
    ~FCameraInsertionScope()
    {
        if(!Stopped)return;
        struct FRetained{TArray<FIdentity> Holds;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<const UObject> Source;};
        static auto* Retained=new TArray<FRetained>();FRetained Keep;Keep.Holds=MoveTemp(Identities);Keep.World=MoveTemp(World);Keep.Source=MoveTemp(BindingSource);Retained->Add(MoveTemp(Keep));
        CameraInsertionQuarantined=true;
    }
    bool Stop(const TCHAR* Stage,const TCHAR* Code,const FString& Error)
    {if(!Stopped){R.Data->SetStringField(TEXT("camera_insertion_first_failure_stage"),Stage);Fail(R,Code,Error);}Stopped=true;R.Data->SetBoolField(TEXT("camera_insertion_unsafe_holds_retained"),true);R.Stage=Stage;R.bKnown=false;R.bPost=false;return false;}
    bool Context(const TCHAR* Stage)
    {
        if(Stopped)return false;
        if(!IsInGameThread()||!Loaded(GEditor)||GEditor!=Editor.Get()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage())
            return Stop(Stage,TEXT("sequencer_camera_insertion_context_changed"),TEXT("Same Editor outside GC/exit/save required; no next SDK/observation/retry."));
        if(GEditor->PlayWorld||GEditor->GetPIEWorldContext()||!Loaded(World.Get())||World->bIsTearingDown||World->WorldType!=EWorldType::Editor
            ||GEditor->GetEditorWorldContext().World()!=World.Get()||!World->GetPathName().Equals(WorldPath,ESearchCase::CaseSensitive))
            return Stop(Stage,TEXT("sequencer_camera_insertion_world_changed"),TEXT("Captured idle Editor world changed."));
        return true;
    }
    bool Pin(UObject* O,const TCHAR* Stage)
    {
        if(!Context(Stage))return false;
        if(!Loaded(O)||!Loaded(O->GetClass())||(O->GetOuter()&&!Loaded(O->GetOuter()))||!Loaded(O->GetPackage()))
            return Stop(Stage,TEXT("sequencer_camera_insertion_identity_unavailable"),TEXT("Live selected export/class/owner/package required."));
        for(const auto& I:Identities)if(I.Hold.Get()==O)return true;
        if(Identities.Num()>MaxTracks+MaxSections+4)return Stop(Stage,TEXT("sequencer_camera_insertion_bound"),TEXT("Selected section identity budget exceeded."));
        FIdentity I;I.Hold.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();Identities.Add(MoveTemp(I));return true;
    }
    bool PossessableClassPath(const FMovieScenePossessable* P,FString& Out) const
    {
        auto* Type=FMovieScenePossessable::StaticStruct();auto* Field=FindFProperty<FSoftClassProperty>(Type,TEXT("PossessedObjectClass"));
        if(!Loaded(Type)||Type->GetPathName()!=TEXT("/Script/MovieScene.MovieScenePossessable")||Type->GetStructureSize()!=static_cast<int32>(sizeof(FMovieScenePossessable))
            ||!Field||Field->GetOwnerStruct()!=Type||Field->ArrayDim!=1||Field->GetSize()!=static_cast<int32>(sizeof(FSoftObjectPtr)))return false;
        Out=Field->GetPropertyValue_InContainer(P).ToSoftObjectPath().ToString();return true;
    }

    bool CaptureReuseSignatures()
    {
        TArray<UMovieSceneSignedObject*> Signed;Signed.Add(C.Sequence);Signed.Add(C.Scene);Signed.Add(Track);for(auto* Cut:Prior)Signed.Add(Cut);
        TArray<UMovieSceneTrack*> Ordinary=C.Scene->GetTracks();const auto BindingRows=static_cast<const UMovieScene*>(C.Scene)->GetBindings();for(const auto& BindingRow:BindingRows)Ordinary.Append(BindingRow.GetTracks());
        for(auto* OrdinaryTrack:Ordinary){Signed.AddUnique(OrdinaryTrack);const auto SectionRows=OrdinaryTrack->GetAllSections();for(auto* SectionRow:SectionRows)Signed.AddUnique(SectionRow);}
        if(Signed.Num()>MaxTracks+MaxSections+3)return Stop(TEXT("camera_reuse_signature_bind"),TEXT("sequencer_camera_reuse_signature_bound"),TEXT("Bounded existing signed object roles required."));
        for(auto* SignedObject:Signed){if(!Pin(SignedObject,TEXT("camera_reuse_signature_bind")))return false;FReuseSignature Role;Role.Object=SignedObject;Role.Before=SignedObject->GetSignature();ReuseSignatures.Add(Role);}return true;
    }
    void PlanReuseModify(UMovieSceneSignedObject* Object,bool /*Neighbors*/=false)
    {
        for(auto& Role:ReuseSignatures)
        {
            if(Role.Object==Object||Role.Object==C.Sequence||(Object!=C.Sequence&&Role.Object==C.Scene)||(Object!=C.Sequence&&Object!=C.Scene&&Role.Object==Track))Role.MayChange=true;
            // Planned neighbor Modify roles were fixed before dispatch by PlanArrangement.
        }
    }
    bool IdentityCurrent(const TCHAR* Stage,bool SaveReturned=false)
    {
        if(!Context(Stage))return false;
        for(auto& I:Identities)
        {
            UObject* O=I.Hold.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer
                ||O->GetPackage()!=I.Package||O->GetOutermost()!=I.Package||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive))
                return Stop(Stage,TEXT("sequencer_camera_insertion_identity_changed"),TEXT("Captured selected identity/current owner changed."));
            const auto Flags=O->GetFlags();if(Flags!=I.Flags)
            {
                if(!SaveReturned||!R.bSaved||Flags!=(I.Flags|RF_WasLoaded|RF_LoadCompleted)||O->HasAnyFlags(RF_Transient)||!I.Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive))
                    return Stop(Stage,TEXT("sequencer_camera_insertion_flags_changed"),TEXT("Unexpected selected flags outside the exact successful-save loaded-bits boundary."));
                I.Flags=Flags;
            }
        }
        return true;
    }
    bool Check(const TCHAR* Stage,bool SaveReturned=false)
    {
        if(!IdentityCurrent(Stage,SaveReturned))return false;
        if(C.Sequence->GetMovieScene()!=C.Scene||C.Scene->GetOuter()!=C.Sequence||C.Scene->IsReadOnly()||!DirectorCurrent(C,R,Stage,SaveReturned)
            ||C.Scene->GetCameraCutTrack()!=Track||!CameraTrackIdentity(C,Track))
            return Stop(Stage,TEXT("sequencer_camera_insertion_routing_changed"),TEXT("Current writable sequence/scene/cut-track roles changed."));
        if(C.Scene->GetPlaybackRange()!=Playback||C.Scene->GetDisplayRate()!=Display||C.Scene->GetTickResolution()!=Resolution||C.Binding!=SelectedGuid||!C.Scene->FindBinding(SelectedGuid))
            return Stop(Stage,TEXT("sequencer_camera_insertion_inputs_changed"),TEXT("Captured playback/display/tick/binding inputs changed before the next SDK."));
        const auto* P=C.Scene->FindPossessable(SelectedGuid);const auto* S=C.Scene->FindSpawnable(SelectedGuid);
        if(Possessable?(!P||S||P->GetParent()!=SelectedParent||P->GetSpawnableObjectBindingID()!=SelectedSpawnable||P->GetLoadedPossessedObjectClass()!=BindingSource.Get()||!P->GetName().Equals(SelectedName,ESearchCase::CaseSensitive))
            :(!S||P||S->GetObjectTemplate()!=BindingSource.Get()||!S->GetName().Equals(SelectedName,ESearchCase::CaseSensitive)))
            return Stop(Stage,TEXT("sequencer_camera_insertion_binding_changed"),TEXT("Current selected binding declaration/source role changed."));
        FString CurrentClassPath;if(P&&(!PossessableClassPath(P,CurrentClassPath)||!CurrentClassPath.Equals(SelectedClassPath,ESearchCase::CaseSensitive)))
            return Stop(Stage,TEXT("sequencer_camera_insertion_binding_path_changed"),TEXT("Current selected possessable soft class declaration changed; no class load is requested."));
        const auto& Current=CameraSections(Track);if(Current.Num()!=ExpectedOrder.Num())return Stop(Stage,TEXT("sequencer_camera_insertion_membership_changed"),TEXT("Selected physical ordered membership changed."));
        for(int32 I=0;I<Current.Num();++I)if(Current[I]!=ExpectedOrder[I]||!CameraSectionIdentity(Track,ExpectedOrder[I])||ExpectedOrder[I]->GetRange()!=ExpectedRanges[I]
            ||ExpectedOrder[I]->GetCameraBindingID()!=Bindings[I])
            return Stop(Stage,TEXT("sequencer_camera_insertion_section_changed"),TEXT("Current section pointer/order/range/binding differs from the SDK-derived plan."));
        {TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& Role:ReuseSignatures){const FGuid Actual=Role.Object->GetSignature();auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("object"),Role.Object->GetPathName());Row->SetStringField(TEXT("before"),Role.Before.ToString());Row->SetStringField(TEXT("actual"),Actual.ToString());Row->SetBoolField(TEXT("sdk_planned_modify_role"),Role.MayChange);Rows.Add(MakeShared<FJsonValueObject>(Row));if(!Role.MayChange&&Actual!=Role.Before)return Stop(Stage,TEXT("sequencer_camera_reuse_unselected_signature_changed"),TEXT("A signed object outside the exact planned Modify/ancestor roles changed."));}R.Data->SetArrayField(TEXT("camera_cut_reuse_signature_observations"),Rows);}
        return true;
    }
    // RJ: value-only reproduction of the installed AddNewCameraCut arrangement.
    // The original physical inventory determines FindEnd/reuse before either sort.
    TArray<FMovieSceneObjectBindingID> PriorBindings;TArray<int32> PlannedOrder;TArray<TRange<FFrameNumber>> PlannedRanges;int32 PlannedSelected=INDEX_NONE;
    void SortPlan()
    {
        // Same Algo::SortBy + strict lower-bound ordering as MovieSceneHelpers.
        // All admitted lower bounds are inclusive, including equal-start ties.
        Algo::SortBy(PlannedOrder,[this](int32 Role){return PlannedRanges[Role].GetLowerBoundValue();},
            [](FFrameNumber A,FFrameNumber B){return A<B;});
    }
    bool PlanArrangement(UMovieSceneCameraCutSection* ExistingReuse)
    {
        PlannedRanges=PriorRanges;for(int32 I=0;I<Prior.Num();++I)PlannedOrder.Add(I);
        PlannedSelected=ExistingReuse?Prior.IndexOfByKey(ExistingReuse):Prior.Num();
        if(ExistingReuse&&PlannedSelected==INDEX_NONE)return Fail(R,TEXT("sequencer_camera_reuse_plan_missing"),TEXT("Predicted reuse must belong to the complete captured inventory."));
        if(!ExistingReuse){PlannedOrder.Add(PlannedSelected);PlannedRanges.Add(TRange<FFrameNumber>(Start,InitialEnd));}
        SortPlan();const int32 At=PlannedOrder.IndexOfByKey(PlannedSelected);
        if(At>0)
        {
            const int32 PreviousRole=PlannedOrder[At-1];const auto BeforeRange=PlannedRanges[PreviousRole];
            if(Start>BeforeRange.GetLowerBoundValue())PlannedRanges[PreviousRole]=TRange<FFrameNumber>(BeforeRange.GetLowerBound(),TRangeBound<FFrameNumber>::Exclusive(Start));
        }
        if(At+1<PlannedOrder.Num())
        {
            const int32 NextRole=PlannedOrder[At+1];const auto BeforeRange=PlannedRanges[NextRole];
            if(InitialEnd<BeforeRange.GetUpperBoundValue())PlannedRanges[NextRole]=TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Inclusive(InitialEnd),BeforeRange.GetUpperBound());
        }
        // FixupConsecutiveSections always calls Modify on present neighbors,
        // even when its strict length checks leave the range unchanged.
        for(auto& Role:ReuseSignatures)
        {
            if(Role.Object==C.Sequence||Role.Object==C.Scene||Role.Object==Track||Role.Object==ExistingReuse
                ||(At>0&&PlannedOrder[At-1]<Prior.Num()&&Role.Object==Prior[PlannedOrder[At-1]])
                ||(At+1<PlannedOrder.Num()&&PlannedOrder[At+1]<Prior.Num()&&Role.Object==Prior[PlannedOrder[At+1]]))Role.MayChange=true;
        }
        SortPlan();return true;
    }
    bool Bind(UMovieSceneCameraCutTrack* In,FFrameNumber InStart,FFrameNumber InEnd,UMovieSceneCameraCutSection* ExistingReuse=nullptr)
    {
        Editor=GEditor;if(!IsInGameThread()||!Loaded(GEditor)||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage())return Stop(TEXT("camera_insertion_bind"),TEXT("sequencer_camera_insertion_context_invalid"),TEXT("Safe captured Editor required."));
        World.Reset(GEditor->GetEditorWorldContext().World());if(!Loaded(World.Get()))return Stop(TEXT("camera_insertion_bind"),TEXT("sequencer_camera_insertion_world_invalid"),TEXT("Live Editor world required."));WorldPath=World->GetPathName();
        Track=In;Start=InStart;End=InEnd;ExactReuse=ExistingReuse!=nullptr;PackagePath=C.Sequence->GetPackage()->GetName();Playback=C.Scene->GetPlaybackRange();Display=C.Scene->GetDisplayRate();Resolution=C.Scene->GetTickResolution();SelectedGuid=C.Binding;
        const auto* P=C.Scene->FindPossessable(SelectedGuid);const auto* S=C.Scene->FindSpawnable(SelectedGuid);
        if((P!=nullptr)==(S!=nullptr)||!C.Scene->FindBinding(SelectedGuid))return Fail(R,TEXT("sequencer_camera_insertion_binding_unsupported"),TEXT("Exactly one existing possessable/spawnable declaration required."));
        Possessable=P!=nullptr;const UObject* Source=nullptr;
        if(P){SelectedName=P->GetName();SelectedParent=P->GetParent();SelectedSpawnable=P->GetSpawnableObjectBindingID();Source=P->GetLoadedPossessedObjectClass();
            if(!PossessableClassPath(P,SelectedClassPath))return Fail(R,TEXT("sequencer_camera_insertion_binding_class_unsupported"),TEXT("Exact native soft class declaration required without loading/resolving it."));}
        else{SelectedName=S->GetName();Source=S->GetObjectTemplate();}
        if(Source&&!Loaded(Source))return Stop(TEXT("camera_insertion_binding_capture"),TEXT("sequencer_camera_insertion_binding_source_unavailable"),TEXT("Current selected binding source must be live if nonnull."));
        BindingSource.Reset(Source);
        if(!Pin(C.Sequence,TEXT("camera_insertion_capture"))||!Pin(C.Scene,TEXT("camera_insertion_capture"))||!Pin(Track,TEXT("camera_insertion_capture")))return false;
        const auto Current=CameraSections(Track);if(Current.IsEmpty()||Current.Num()>=MaxSections)return Fail(R,TEXT("sequencer_camera_insertion_bound"),TEXT("Existing bounded nonempty camera track required."));
        for(UMovieSceneSection* Row:Current)
        {
            auto* Cut=Loaded(Row)&&Row->GetClass()==UMovieSceneCameraCutSection::StaticClass()?static_cast<UMovieSceneCameraCutSection*>(Row):nullptr;
            if(!CameraSectionIdentity(Track,Cut)||!Pin(Cut,TEXT("camera_insertion_capture")))return Stop(TEXT("camera_insertion_capture"),TEXT("sequencer_camera_insertion_section_unsupported"),TEXT("Exact live native section required."));
            const auto CapturedRange=Cut->GetRange();
            if(!CapturedRange.GetLowerBound().IsInclusive()||!CapturedRange.GetUpperBound().IsExclusive()||CapturedRange.GetLowerBoundValue()>=CapturedRange.GetUpperBoundValue()||Prior.Contains(Cut))
                return Fail(R,TEXT("sequencer_camera_insertion_arrangement_unsupported"),TEXT("Unique native finite nonempty inclusive/exclusive section roles required; physical order, overlaps and equal starts are allowed."));
            Prior.Add(Cut);PriorRanges.Add(CapturedRange);ExpectedOrder.Add(Cut);ExpectedRanges.Add(CapturedRange);Bindings.Add(Cut->GetCameraBindingID());
        }
        if(!CameraInitialEnd(Playback,Resolution,Start,PriorRanges,InitialEnd,R))return false;
        UMovieSceneCameraCutSection* Predicted=nullptr;
        for(int32 I=0;I<Prior.Num();++I)if(PriorRanges[I]==TRange<FFrameNumber>(Start,InitialEnd)){Predicted=Prior[I];break;}
        if(Predicted!=ExistingReuse)return Fail(R,TEXT("sequencer_camera_reuse_plan_changed"),TEXT("Predispatch first matching start/end reuse differs from the copied original inventory."));
        NewSection=ExistingReuse;PriorBindings=Bindings;
        if(!CaptureReuseSignatures()||!PlanArrangement(ExistingReuse))return false;
        if(ExactReuse){Index=Prior.IndexOfByKey(ExistingReuse);R.Data->SetBoolField(TEXT("native_camera_cut_exact_reuse"),true);R.Data->SetBoolField(TEXT("expected_existing_section_reuse"),true);R.Data->SetBoolField(TEXT("camera_cut_reuse_signature_baseline_readopted"),false);}
        R.Data->SetBoolField(TEXT("native_camera_cut_general_arrangement"),true);R.Data->SetBoolField(TEXT("camera_insertion_neighbor_scope"),true);R.Data->SetNumberField(TEXT("camera_sdk_initial_end_tick"),InitialEnd.Value);
        R.Data->SetBoolField(TEXT("SDK_internal_callback_atomicity_proven"),false);return Check(TEXT("camera_insertion_bound"));
    }
    bool Adopt(UMovieSceneCameraCutSection* Returned,const FMovieSceneObjectBindingID& Binding)
    {
        if(!IdentityCurrent(TEXT("camera_insertion_returned_identities")))return false;
        if(ExactReuse)
        {if(Returned!=NewSection)return Stop(TEXT("camera_reuse_returned"),TEXT("sequencer_camera_reuse_returned_identity_changed"),TEXT("SDK must return the preheld first matching section; an unexpected pointer is not inspected or adopted."));}
        else
        {
            if(!Loaded(Returned)||Returned->GetClass()!=UMovieSceneCameraCutSection::StaticClass()||Returned->GetOuter()!=Track||Returned->GetPackage()!=C.Sequence->GetPackage()||Prior.Contains(Returned))
                return Stop(TEXT("camera_insertion_returned"),TEXT("sequencer_camera_insertion_return_invalid"),TEXT("AddNewCameraCut did not return a fresh exact owned current section."));
            if(!Pin(Returned,TEXT("camera_insertion_returned")))return false;NewSection=Returned;
        }
        ExpectedOrder.Reset();ExpectedRanges.Reset();Bindings.Reset();
        for(int32 Role:PlannedOrder)
        {
            auto* Cut=Role==PlannedSelected?Returned:Prior[Role];
            ExpectedOrder.Add(Cut);ExpectedRanges.Add(PlannedRanges[Role]);
            Bindings.Add(Role==PlannedSelected?Binding:PriorBindings[Role]);
        }
        Index=ExpectedOrder.IndexOfByKey(Returned);R.Data->SetNumberField(TEXT("camera_insertion_index"),Index);if(ExactReuse)R.Data->SetNumberField(TEXT("camera_cut_reuse_index"),Index);
        return Check(TEXT("camera_insertion_after_native_arrange"));
    }
    bool PlanFinalRange()
    {if(!Check(TEXT("camera_insertion_before_set_range")))return false;ExpectedRanges[Index]=TRange<FFrameNumber>(Start,End);return true;}
};
bool CameraInsertionCheck(FCameraInsertionScope* Scope,const TCHAR* Stage,bool SaveReturned)
{return !Scope||Scope->Check(Stage,SaveReturned);}

bool CameraAdmission(FContext& C, FFrameNumber Start, UMovieSceneCameraCutTrack*& Track, UMovieSceneCameraCutSection*& Reused, FResult& R)
{
    C.bCamera = true;
    if (!Native(UMovieSceneCameraCutTrack::StaticClass(), TEXT("/Script/MovieSceneTracks.MovieSceneCameraCutTrack"))
        || !Native(UMovieSceneCameraCutSection::StaticClass(), TEXT("/Script/MovieSceneTracks.MovieSceneCameraCutSection")))
    { return Fail(R, TEXT("sequencer_camera_native_source_required"), TEXT("Exact loaded native runtime CameraCutTrack/CameraCutSection sources required.")); }
    if (C.Scene->IsReadOnly() || !DirectorAllowed(C))
    { return Fail(R, TEXT("sequencer_camera_variant_unsupported"), TEXT("Writable parent without Director Blueprint required; director/callback variants remain incomplete.")); }
    const auto Playback = C.Scene->GetPlaybackRange();
    const auto* References = C.Sequence->GetBindingReferences();
    if (!References || References->GetAllReferences().Num() > MaxBindings * 16)
    { return Fail(R, TEXT("sequencer_camera_reference_budget_exceeded"), TEXT("Bounded existing locator references required; no locator/world resolution runs.")); }
    for (const auto& Reference : References->GetAllReferences())
    { if (Reference.CustomBinding != nullptr) { return Fail(R, TEXT("sequencer_camera_custom_binding_unsupported"), TEXT("Custom runtime binding variants remain incomplete.")); } }
    TArray<UMovieSceneTrack*> Ordinary = C.Scene->GetTracks();
    const auto& Bindings = static_cast<const UMovieScene*>(C.Scene)->GetBindings();
    if (Bindings.Num() > MaxBindings) { return Fail(R, TEXT("sequencer_camera_budget_exceeded"), TEXT("Binding budget exceeded before mutation.")); }
    for (const auto& Binding : Bindings) { Ordinary.Append(Binding.GetTracks()); }
    if (Ordinary.Num() > MaxTracks) { return Fail(R, TEXT("sequencer_camera_budget_exceeded"), TEXT("Aggregate ordinary-track budget exceeded before mutation.")); }
    for (UMovieSceneTrack* Other : Ordinary)
    { if (!SubsequenceOrdinaryTrack(Other, C.Scene, false)) { return Fail(R, TEXT("sequencer_camera_other_track_unsupported"), TEXT("Ordinary root/bound tracks must be exact native Scalar/Transform without decorations/custom channels; nested/subtrack/other variants remain incomplete.")); } }
    UMovieSceneTrack* Existing = C.Scene->GetCameraCutTrack();
    if (!Existing) { FFrameNumber InitialEnd;return CameraInitialEnd(Playback,C.Scene->GetTickResolution(),Start,{},InitialEnd,R); }
    if (!Loaded(Existing) || Existing->GetClass() != UMovieSceneCameraCutTrack::StaticClass())
    { return Fail(R, TEXT("sequencer_camera_track_unsupported"), TEXT("Current cut track must be exact native CameraCutTrack.")); }
    Track = static_cast<UMovieSceneCameraCutTrack*>(Existing);
    if (!CameraTrackIdentity(C, Track) || CameraSections(Track).Num() >= MaxSections)
    { return Fail(R, TEXT("sequencer_camera_arrangement_unsupported"), TEXT("Bounded nonblend auto-managed native cut track required.")); }
    if (CameraSections(Track).IsEmpty()) { FFrameNumber InitialEnd;return CameraInitialEnd(Playback,C.Scene->GetTickResolution(),Start,{},InitialEnd,R); }
    // FindEndTimeForCameraCut reads the original physical array, then reuse
    // scans that same order for the first exact [Start,InitialEnd) match.
    const auto Current=CameraSections(Track);FFrameNumber InitialEnd;TArray<TRange<FFrameNumber>> PhysicalRanges;
    TArray<UMovieSceneCameraCutSection*> Seen;
    for(auto* Base:Current)
    {
        auto* Cut=Loaded(Base)&&Base->GetClass()==UMovieSceneCameraCutSection::StaticClass()?static_cast<UMovieSceneCameraCutSection*>(Base):nullptr;
        if(!CameraSectionIdentity(Track,Cut)||Seen.Contains(Cut))return Fail(R,TEXT("sequencer_camera_section_unsupported"),TEXT("Unique exact native current sections required before dispatch."));
        Seen.Add(Cut);const auto CapturedRange=Cut->GetRange();
        if(!CapturedRange.GetLowerBound().IsInclusive()||!CapturedRange.GetUpperBound().IsExclusive()||CapturedRange.GetLowerBoundValue()>=CapturedRange.GetUpperBoundValue())
            return Fail(R,TEXT("sequencer_camera_section_range_unsupported"),TEXT("Finite nonempty inclusive/exclusive prior ranges required; other endpoint forms remain incomplete."));
        PhysicalRanges.Add(CapturedRange);
    }
    if(!CameraInitialEnd(Playback,C.Scene->GetTickResolution(),Start,PhysicalRanges,InitialEnd,R))return false;
    for(auto* Cut:Seen)if(Cut->GetRange()==TRange<FFrameNumber>(Start,InitialEnd)){Reused=Cut;break;}
    R.Data->SetBoolField(TEXT("expected_existing_section_reuse"),Reused!=nullptr);R.Data->SetBoolField(TEXT("camera_insertion_requested"),true);return true;
}
bool CameraSelectedChanged(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B)
{
    if (!SameField(A, B, TEXT("camera_cut_track"))) { return true; }
    const FString Path = A->GetStringField(TEXT("camera_cut_track")); if (Path.IsEmpty()) { return false; }
    const auto Left = A->GetObjectField(TEXT("tracks"))->TryGetField(Path), Right = B->GetObjectField(TEXT("tracks"))->TryGetField(Path);
    return !Left.IsValid() || !Right.IsValid() || Text(Left->AsObject().ToSharedRef()) != Text(Right->AsObject().ToSharedRef());
}
bool CameraTrackAcquired(FContext& C, UMovieSceneCameraCutTrack* Track, bool bCreated, const TSharedRef<FJsonObject>& ActualState)
{
    if (!CameraTrackIdentity(C, Track)) { return false; }
    auto Expected = MakeShared<FJsonObject>(); Expected->Values = C.Before->Values;
    if (bCreated)
    {
        if (!C.Before->GetStringField(TEXT("camera_cut_track")).IsEmpty() || !CameraSections(Track).IsEmpty()) { return false; }
        Expected->SetStringField(TEXT("camera_cut_track"), Track->GetPathName()); Expected->SetNumberField(TEXT("track_count"), C.Before->GetNumberField(TEXT("track_count")) + 1);
        auto TrackRows = MakeShared<FJsonObject>(); TrackRows->Values = C.Before->GetObjectField(TEXT("tracks"))->Values;
        const auto NewRow = ActualState->GetObjectField(TEXT("tracks"))->TryGetField(Track->GetPathName()); if (!NewRow.IsValid() || NewRow->Type != EJson::Object) { return false; }
        TrackRows->SetField(Track->GetPathName(), NewRow); Expected->SetObjectField(TEXT("tracks"), TrackRows);
        FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(C.Scene->GetClass(), TEXT("CameraCutTrack"));
        if (!Property || Property->GetOwnerClass() != UMovieScene::StaticClass() || Property->PropertyClass != UMovieSceneTrack::StaticClass()) { return false; }
        TObjectPtr<UMovieSceneTrack> Copy = Track; auto Exported = MakeShared<FJsonObject>(); if (!MaintenanceExportExpected(Property, &Copy, C.Scene, Exported)) { return false; }
        auto SceneFields = MakeShared<FJsonObject>(); SceneFields->Values = C.Before->GetObjectField(TEXT("movie_scene_fields"))->Values; SceneFields->SetObjectField(TEXT("CameraCutTrack"), Exported); Expected->SetObjectField(TEXT("movie_scene_fields"), SceneFields);
    }
    auto Actual = MakeShared<FJsonObject>(); Actual->Values = ActualState->Values;
    for (const TCHAR* Key : { TEXT("dirty"), TEXT("sequence_signature"), TEXT("movie_scene_signature") }) { Expected->RemoveField(Key); Actual->RemoveField(Key); }
    return Same(Expected, Actual);
}
// Only generated validation publication is invalidated. Actual requested-operation
// delta/changed/performed/created_count and the already returned save receipt remain.
void CameraInsertionInvalidatePublication(FResult& R)
{
    if(R.Data->HasField(TEXT("native_camera_cut_exact_reuse"))){R.Data->SetBoolField(TEXT("camera_cut_exact_reuse_verified"),false);R.Data->SetBoolField(TEXT("typed_engine_postcondition_passed"),false);}
    if(!R.Data->HasTypedField<EJson::Object>(TEXT("legacy_mutation_contract")))return;
    auto Contract=R.Data->GetObjectField(TEXT("legacy_mutation_contract"));
    for(const TCHAR* K:{TEXT("success"),TEXT("verified_success"),TEXT("postcondition_verified")})Contract->SetBoolField(K,false);
    Contract->SetStringField(TEXT("success_semantics"),TEXT("postcondition_failed"));Contract->SetStringField(TEXT("error_code"),R.Code);Contract->SetStringField(TEXT("error"),R.Error);
    if(Contract->HasTypedField<EJson::Object>(TEXT("verification")))Contract->GetObjectField(TEXT("verification"))->SetStringField(TEXT("status"),TEXT("failed"));
    R.Data->SetBoolField(TEXT("verified_success"),false);R.Data->SetBoolField(TEXT("postcondition_verified"),false);R.Data->SetStringField(TEXT("success_semantics"),TEXT("postcondition_failed"));
    R.Data->SetBoolField(TEXT("camera_insertion_generated_validation_invalidated"),true);R.Data->SetBoolField(TEXT("camera_insertion_publication_fence_passed"),false);
}

bool CameraInsertionPreserved(FContext& C,FCameraInsertionScope& Scope,UMovieSceneCameraCutSection* Section,const TSharedRef<FJsonObject>& Acquired,const TSharedRef<FJsonObject>& After)
{
    if(!Scope.Check(TEXT("camera_insertion_preservation"))||Section!=Scope.NewSection)return false;
    auto Expected=MakeShared<FJsonObject>();Expected->Values=Acquired->Values;auto TrackRows=MakeShared<FJsonObject>();TrackRows->Values=Acquired->GetObjectField(TEXT("tracks"))->Values;
    const FString TrackPath=Scope.Track->GetPathName();const auto OldTrack=TrackRows->GetObjectField(TrackPath),ActualTrack=After->GetObjectField(TEXT("tracks"))->GetObjectField(TrackPath);
    const auto OldRows=OldTrack->GetArrayField(TEXT("sections"));const auto ActualRows=ActualTrack->GetArrayField(TEXT("sections"));
    if(OldRows.Num()!=Scope.Prior.Num()||ActualRows.Num()!=Scope.ExpectedOrder.Num()||ActualRows.Num()!=OldRows.Num()+(Scope.ExactReuse?0:1))return false;
    TArray<TSharedPtr<FJsonValue>> Rows;TArray<TObjectPtr<UMovieSceneSection>> Objects;
    for(int32 I=0;I<Scope.ExpectedOrder.Num();++I)
    {
        auto* Current=Scope.ExpectedOrder[I];Objects.Add(Current);const int32 OldIndex=Scope.Prior.IndexOfByKey(Current);
        if(OldIndex==INDEX_NONE){if(Current!=Section||Scope.ExactReuse)return false;Rows.Add(ActualRows[I]);continue;}
        auto Row=MakeShared<FJsonObject>();Row->Values=OldRows[OldIndex]->AsObject()->Values;
        if(!Row->GetStringField(TEXT("path")).Equals(Current->GetPathName(),ESearchCase::CaseSensitive))return false;
        auto Fields=MakeShared<FJsonObject>();Fields->Values=Row->GetObjectField(TEXT("fields"))->Values;
        if(Scope.ExpectedRanges[I]!=Scope.PriorRanges[OldIndex])
        {
            auto* RangeProperty=FindFProperty<FStructProperty>(Current->GetClass(),TEXT("SectionRange"));
            if(!RangeProperty||RangeProperty->GetOwnerClass()!=UMovieSceneSection::StaticClass()||!RangeProperty->Struct.IsResolved()||!Loaded(RangeProperty->Struct.Get())||RangeProperty->Struct->GetPathName()!=TEXT("/Script/MovieScene.MovieSceneFrameRange")||RangeProperty->Struct->GetStructureSize()!=static_cast<int32>(sizeof(FMovieSceneFrameRange)))return false;
            FMovieSceneFrameRange Value(Scope.ExpectedRanges[I]);auto Exported=MakeShared<FJsonObject>();
            if(!MaintenanceExportExpected(RangeProperty,&Value,Current,Exported,[&Scope]{return Scope.Check(TEXT("camera_insertion_range_export_return"));})||!Scope.Check(TEXT("camera_insertion_after_range_export")))return false;
            Fields->SetObjectField(TEXT("SectionRange"),Exported);Row->SetObjectField(TEXT("range"),Range(Scope.ExpectedRanges[I]));
        }
        if(Scope.ExactReuse&&Current==Section)
        {
            auto* BindingProperty=FindFProperty<FStructProperty>(Current->GetClass(),TEXT("CameraBindingID"));
            if(!BindingProperty||BindingProperty->GetOwnerClass()!=UMovieSceneCameraCutSection::StaticClass()||!BindingProperty->Struct.IsResolved()||!Loaded(BindingProperty->Struct.Get())||BindingProperty->Struct->GetPathName()!=TEXT("/Script/MovieScene.MovieSceneObjectBindingID")||BindingProperty->Struct->GetStructureSize()!=static_cast<int32>(sizeof(FMovieSceneObjectBindingID)))return false;
            FMovieSceneObjectBindingID Value(Scope.Bindings[I]);auto Exported=MakeShared<FJsonObject>();
            if(!MaintenanceExportExpected(BindingProperty,&Value,Current,Exported,[&Scope]{return Scope.Check(TEXT("camera_insertion_binding_export_return"));})||!Scope.Check(TEXT("camera_insertion_after_binding_export")))return false;
            Fields->SetObjectField(TEXT("CameraBindingID"),Exported);
        }
        Row->SetObjectField(TEXT("fields"),Fields);Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto PlannedTrack=MakeShared<FJsonObject>();PlannedTrack->Values=OldTrack->Values;PlannedTrack->SetArrayField(TEXT("sections"),Rows);
    auto* ArrayProperty=FindFProperty<FArrayProperty>(Scope.Track->GetClass(),TEXT("Sections"));
    if(!ArrayProperty||ArrayProperty->GetOwnerClass()!=UMovieSceneCameraCutTrack::StaticClass())return false;
    auto* Inner=CastField<FObjectPropertyBase>(ArrayProperty->Inner);
    if(!Inner||Inner->GetOwner<FProperty>()!=ArrayProperty||!Inner->PropertyClass.IsResolved()||Inner->PropertyClass.Get()!=UMovieSceneSection::StaticClass())return false;
    auto Exported=MakeShared<FJsonObject>();if(!MaintenanceExportExpected(ArrayProperty,&Objects,Scope.Track,Exported,[&Scope]{return Scope.Check(TEXT("camera_insertion_array_export_return"));})||!Scope.Check(TEXT("camera_insertion_after_array_export")))return false;
    auto Fields=MakeShared<FJsonObject>();Fields->Values=OldTrack->GetObjectField(TEXT("fields"))->Values;Fields->SetObjectField(TEXT("Sections"),Exported);PlannedTrack->SetObjectField(TEXT("fields"),Fields);
    TrackRows->SetObjectField(TrackPath,PlannedTrack);Expected->SetObjectField(TEXT("tracks"),TrackRows);Expected->SetNumberField(TEXT("section_count"),Acquired->GetNumberField(TEXT("section_count"))+(Scope.ExactReuse?0:1));
    auto Actual=MakeShared<FJsonObject>();Actual->Values=After->Values;for(const TCHAR* Key:{TEXT("dirty"),TEXT("sequence_signature"),TEXT("movie_scene_signature")}){Expected->RemoveField(Key);Actual->RemoveField(Key);}
    return CameraLiteral(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Actual));
}
bool CameraReusePreserved(FContext& C,FCameraInsertionScope& Scope,UMovieSceneCameraCutSection* Section,const TSharedRef<FJsonObject>& Acquired,const TSharedRef<FJsonObject>& After)
{return Scope.ExactReuse&&CameraInsertionPreserved(C,Scope,Section,Acquired,After);}

bool CameraMutationPreserved(FContext& C, UMovieSceneCameraCutTrack* Track, UMovieSceneCameraCutSection* Section, bool bReuse,
    const FMovieSceneObjectBindingID& Binding, const TRange<FFrameNumber>& ExpectedRange, const TSharedRef<FJsonObject>& Acquired, const TSharedRef<FJsonObject>& After)
{
    if(C.CameraInsertion)return C.CameraInsertion->ExactReuse?CameraReusePreserved(C,*C.CameraInsertion,Section,Acquired,After):CameraInsertionPreserved(C,*C.CameraInsertion,Section,Acquired,After);
    if (!CameraTrackIdentity(C, Track) || !CameraSectionIdentity(Track, Section) || CameraSections(Track).Num() != 1 || CameraSections(Track)[0] != Section) { return false; }
    auto Expected = MakeShared<FJsonObject>(); Expected->Values = Acquired->Values; auto TrackRows = MakeShared<FJsonObject>(); TrackRows->Values = Acquired->GetObjectField(TEXT("tracks"))->Values;
    const auto OldTrack = TrackRows->GetObjectField(Track->GetPathName()), NewTrack = After->GetObjectField(TEXT("tracks"))->GetObjectField(Track->GetPathName());
    auto ExpectedTrack = MakeShared<FJsonObject>(); ExpectedTrack->Values = OldTrack->Values;
    const auto& NewSections = NewTrack->GetArrayField(TEXT("sections")); if (NewSections.Num() != 1 || NewSections[0]->AsObject()->GetStringField(TEXT("path")) != Section->GetPathName()) { return false; }
    if (bReuse)
    {
        const auto& OldSections = OldTrack->GetArrayField(TEXT("sections")); if (OldSections.Num() != 1 || OldSections[0]->AsObject()->GetStringField(TEXT("path")) != Section->GetPathName()) { return false; }
        auto ExpectedSection = MakeShared<FJsonObject>(); ExpectedSection->Values = OldSections[0]->AsObject()->Values; auto Fields = MakeShared<FJsonObject>(); Fields->Values = ExpectedSection->GetObjectField(TEXT("fields"))->Values;
        FStructProperty* RangeProperty = FindFProperty<FStructProperty>(Section->GetClass(), TEXT("SectionRange")); FStructProperty* BindingProperty = FindFProperty<FStructProperty>(Section->GetClass(), TEXT("CameraBindingID"));
        if (!RangeProperty || RangeProperty->GetOwnerClass() != UMovieSceneSection::StaticClass() || !Loaded(RangeProperty->Struct) || RangeProperty->Struct->GetPathName() != TEXT("/Script/MovieScene.MovieSceneFrameRange") || RangeProperty->Struct->GetStructureSize() != static_cast<int32>(sizeof(FMovieSceneFrameRange))
            || !BindingProperty || BindingProperty->GetOwnerClass() != UMovieSceneCameraCutSection::StaticClass() || !Loaded(BindingProperty->Struct) || BindingProperty->Struct->GetPathName() != TEXT("/Script/MovieScene.MovieSceneObjectBindingID") || BindingProperty->Struct->GetStructureSize() != static_cast<int32>(sizeof(FMovieSceneObjectBindingID))) { return false; }
        FMovieSceneFrameRange RangeCopy(ExpectedRange); FMovieSceneObjectBindingID BindingCopy(Binding); auto RangeFields = MakeShared<FJsonObject>(), BindingFields = MakeShared<FJsonObject>();
        if (!MaintenanceExportExpected(RangeProperty, &RangeCopy, Section, RangeFields) || !MaintenanceExportExpected(BindingProperty, &BindingCopy, Section, BindingFields)) { return false; }
        Fields->SetObjectField(TEXT("SectionRange"), RangeFields); Fields->SetObjectField(TEXT("CameraBindingID"), BindingFields); ExpectedSection->SetObjectField(TEXT("fields"), Fields); ExpectedSection->SetObjectField(TEXT("range"), Range(ExpectedRange));
        ExpectedTrack->SetArrayField(TEXT("sections"), { MakeShared<FJsonValueObject>(ExpectedSection) });
    }
    else
    {
        if (!OldTrack->GetArrayField(TEXT("sections")).IsEmpty()) { return false; }
        ExpectedTrack->SetArrayField(TEXT("sections"), NewSections); Expected->SetNumberField(TEXT("section_count"), Acquired->GetNumberField(TEXT("section_count")) + 1);
        FArrayProperty* Property = FindFProperty<FArrayProperty>(Track->GetClass(), TEXT("Sections")); FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
        if (!Property || Property->GetOwnerClass() != UMovieSceneCameraCutTrack::StaticClass() || !Inner || Inner->PropertyClass != UMovieSceneSection::StaticClass()) { return false; }
        TArray<TObjectPtr<UMovieSceneSection>> Copy = { Section }; auto Exported = MakeShared<FJsonObject>(); if (!MaintenanceExportExpected(Property, &Copy, Track, Exported)) { return false; }
        auto Fields = MakeShared<FJsonObject>(); Fields->Values = ExpectedTrack->GetObjectField(TEXT("fields"))->Values; Fields->SetObjectField(TEXT("Sections"), Exported); ExpectedTrack->SetObjectField(TEXT("fields"), Fields);
    }
    TrackRows->SetObjectField(Track->GetPathName(), ExpectedTrack); Expected->SetObjectField(TEXT("tracks"), TrackRows);
    auto Actual = MakeShared<FJsonObject>(); Actual->Values = After->Values;
    for (const TCHAR* Key : { TEXT("dirty"), TEXT("sequence_signature"), TEXT("movie_scene_signature") }) { Expected->RemoveField(Key); Actual->RemoveField(Key); }
    return Same(Expected, Actual);
}
}

FString AddCameraCut(const FString& SequencePath, double StartFrame, double EndFrame, const FString& BindingId, const FString& ActorLabel, bool bSave)
{
    FResult R; InitMaintenance(R, TEXT("add_level_sequence_camera_cut"), bSave); FContext C; C.bCamera = true;
    if(CameraInsertionActive||CameraInsertionQuarantined){Fail(R,TEXT("sequencer_camera_insertion_busy"),TEXT("A camera writer or unsafe retained insertion exists; no nested/replay SDK."));return Encode(R);}
    TGuardValue<bool> CameraLease(CameraInsertionActive,true);
    for (const TCHAR* Key : { TEXT("camera_track_create_attempts"), TEXT("add_new_camera_cut_calls"), TEXT("set_camera_section_range_calls"), TEXT("camera_section_new_membership_count"), TEXT("camera_section_reuse_observed_count"), TEXT("camera_section_retirement_count") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetBoolField(TEXT("created_camera_track"), false); R.Data->SetBoolField(TEXT("expected_existing_section_reuse"), false);
    R.Data->SetBoolField(TEXT("internal_allocation_calls_independently_measured"), false);
    R.Data->SetStringField(TEXT("camera_section_counter_scope"), TEXT("New/reused returned native pointer relative to the complete predispatch selection and current membership; retirement means detachment from that current track, not UObject destruction/GC. Internal NewObject/Modify/delegate work is not independently instrumented."));
    R.Data->SetBoolField(TEXT("camera_actor_class_validated"), false); R.Data->SetBoolField(TEXT("camera_rendering_verified"), false); R.Data->SetArrayField(TEXT("retired_camera_section_paths"), TArray<TSharedPtr<FJsonValue>>());
    R.Data->SetStringField(TEXT("camera_scope"), TEXT("Exact native nonblend row-zero auto-managed cut track. Empty or bounded unique finite inclusive/exclusive cuts in physical order, including overlap/equal starts: original first-greater FindEnd/first-exact reuse, SDK sorts and conditional previous-end/next-start adjustment, then requested SetRange. Reuse typed current proof is diagnostic only; original count+1 remains required for public success, so reuse can mutate/save then fail. Finite inclusive playback upper and the native half-second initial fallback are admitted with checked arithmetic. Blended/non-auto/open-end/nested/custom/director variants remain incomplete. No camera Actor/render/evaluation proof."));
    R.Data->SetStringField(TEXT("changed_observation_scope"), TEXT("Actual bounded current camera-track/section declarative data delta; original legacy changed/performed/created_count are literal requested-operation semantics and independent of actual allocation/reuse counts. Dirty/signatures and complete source/file prefixes are retained separately."));
    if (!Resolve(SequencePath, C, R,true) || !MaintenanceReady(C, R) || !MaintenanceResolveBinding(C, BindingId, ActorLabel, R)) { return Encode(R); }
    FFrameNumber Start, End;
    if (!(EndFrame > StartFrame) || !MaintenanceRequestedTimeSafe(StartFrame, C) || !MaintenanceRequestedTimeSafe(EndFrame, C) || !Tick(StartFrame, C, Start) || !Tick(EndFrame, C, End) || End < Start)
    { Fail(R, TEXT("sequencer_camera_range_invalid"), TEXT("Finite end must exceed start and original floored endpoints must fit checked native conversion products/int32 budgets.")); return Encode(R); }
    UMovieSceneCameraCutTrack* Track = nullptr; UMovieSceneCameraCutSection* ExpectedReuse = nullptr;
    if (!CameraAdmission(C, Start, Track, ExpectedReuse, R)) { return Encode(R); }
    TUniquePtr<FCameraInsertionScope> Insertion;
    if(ExpectedReuse||R.Data->HasField(TEXT("camera_insertion_requested"))){Insertion=MakeUnique<FCameraInsertionScope>(C,R);C.CameraInsertion=Insertion.Get();if(!Insertion->Bind(Track,Start,End,ExpectedReuse))return Encode(R);}
    R.Data->SetStringField(TEXT("sequence"), C.Sequence->GetPathName()); R.Data->SetStringField(TEXT("camera_binding_id"), C.Binding.ToString(EGuidFormats::DigitsWithHyphens));
    R.Data->SetNumberField(TEXT("start_frame"), StartFrame); R.Data->SetNumberField(TEXT("end_frame"), EndFrame); R.Data->SetNumberField(TEXT("start_tick"), Start.Value); R.Data->SetNumberField(TEXT("end_tick"), End.Value);
    const int32 BeforeCount = Track ? CameraSections(Track).Num() : 0; const FString PreviousPath = ExpectedReuse ? ExpectedReuse->GetPathName() : FString(); TStrongObjectPtr<UMovieSceneCameraCutSection> KeepPrevious(ExpectedReuse);
    R.Data->SetNumberField(TEXT("camera_cut_section_count_before"), BeforeCount); R.Data->SetObjectField(TEXT("retained_previous_camera_section_before"), CameraRetainedSection(ExpectedReuse,C.Director?TFunction<bool()>([&]{return DirectorCurrent(C,R,TEXT("director_initial_retained_section_export"));}):TFunction<bool()>()));
    if (!MaintenanceCapture(C, R, TEXT("before_observation"), C.Before)) { return Encode(R); }
    auto BeforeInfo = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("before_info"), BeforeInfo);
    if (!MaintenanceInfo(C, BeforeInfo)) { Fail(R, TEXT("sequencer_camera_info_unobservable"), TEXT("Bounded loaded native before Info is incomplete before mutation.")); return Encode(R); } BeforeInfo->SetNumberField(TEXT("camera_cut_section_count"), BeforeCount);
    if (C.Before->GetNumberField(TEXT("section_count")) + (ExpectedReuse ? 0 : 1) > MaxSections || C.Before->GetNumberField(TEXT("track_count")) + (Track ? 0 : 1) > MaxTracks)
    { Fail(R, TEXT("sequencer_camera_budget_exceeded"), TEXT("Requested allocation must fit aggregate track/section budgets before mutation.")); return Encode(R); }
    TSharedPtr<FJsonObject> PublicationExpected;
    const auto CameraCurrent=[&](const TCHAR* Stage,bool Saved=false){return DirectorCurrent(C,R,Stage,Saved)&&CameraInsertionCheck(C.CameraInsertion,Stage,Saved);};
    auto RunMutation=[&]()->FString
    {
    R.Stage = TEXT("mutate"); R.Data->SetNumberField(TEXT("outer_transaction_calls"), 1); FScopedTransaction Transaction(NSLOCTEXT("CubelessSequencer", "AddCameraCut", "Add Level Sequence Camera Cut"));
    if(!CameraCurrent(TEXT("camera_insertion_after_transaction")))return Insertion?FString():Encode(R);
    R.bAttempted = true; R.Data->SetNumberField(TEXT("explicit_modify_calls"), 1); if(Insertion)Insertion->PlanReuseModify(C.Sequence);C.Sequence->Modify();if(!CameraCurrent(TEXT("camera_insertion_after_sequence_modify")))return Insertion?FString():Encode(R);
    R.Data->SetNumberField(TEXT("explicit_modify_calls"), 2); if(Insertion)Insertion->PlanReuseModify(C.Scene);C.Scene->Modify();if(!CameraCurrent(TEXT("camera_insertion_after_scene_modify")))return Insertion?FString():Encode(R);bool CreatedTrack = false;
    if (!Track) { R.Data->SetNumberField(TEXT("camera_track_create_attempts"), 1); Track = Cast<UMovieSceneCameraCutTrack>(C.Scene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass())); CreatedTrack = Track != nullptr; R.Data->SetBoolField(TEXT("created_camera_track"), CreatedTrack); }
    if(C.Director&&!DirectorCurrent(C,R,TEXT("director_camera_track_create_return")))return Insertion?FString():Encode(R);
    TStrongObjectPtr<UMovieSceneCameraCutTrack> KeepTrack(Track); auto Acquired = MakeShared<FJsonObject>();
    if (!MaintenanceCapture(C, R, TEXT("after_track_acquisition"), Acquired)) { return Insertion?FString():Encode(R); } R.bChanged = CameraSelectedChanged(C.Before, Acquired);
    if (!CameraTrackAcquired(C, Track, CreatedTrack, Acquired)) { Fail(R, TEXT("sequencer_camera_track_postcondition_failed"), TEXT("Current exact native track/factory prefix or protected parent state differs; no cut/save dispatch or retry.")); return Insertion?FString():Encode(R); }
    R.Data->SetStringField(TEXT("camera_track_path"), Track->GetPathName()); R.Data->SetNumberField(TEXT("explicit_modify_calls"), 3); if(Insertion)Insertion->PlanReuseModify(Track);Track->Modify();if(!CameraCurrent(TEXT("camera_insertion_after_track_modify")))return Insertion?FString():Encode(R);
    const FMovieSceneObjectBindingID WantedBinding{ UE::MovieScene::FRelativeObjectBindingID(C.Binding) }; const TRange<FFrameNumber> WantedRange(Start, End);
    if(Insertion&&Insertion->ExactReuse)Insertion->PlanReuseModify(ExpectedReuse,true);
    R.Data->SetNumberField(TEXT("add_new_camera_cut_calls"), 1); UMovieSceneCameraCutSection* Section = Track->AddNewCameraCut(WantedBinding, Start);R.Data->SetBoolField(TEXT("camera_section_returned"),Section!=nullptr);
    if(C.Director&&!DirectorCurrent(C,R,TEXT("director_add_new_camera_cut_return")))return Insertion?FString():Encode(R);
    TStrongObjectPtr<UMovieSceneCameraCutSection> KeepSection(Section);if(Insertion&&Insertion->ExactReuse)R.Data->SetBoolField(TEXT("camera_cut_reuse_returned_same_pointer"),Section==ExpectedReuse);
    if(Insertion&&!Insertion->Adopt(Section,WantedBinding))return Insertion?FString():Encode(R);
    if(C.Director&&!CameraCurrent(TEXT("director_before_retained_section_export")))return Insertion?FString():Encode(R);
    const TFunction<bool()> RetainedCurrent=C.Director?TFunction<bool()>([&]{return CameraCurrent(TEXT("director_retained_section_export_return"));}):TFunction<bool()>();
     R.Data->SetObjectField(TEXT("after_add_new_camera_cut_returned_section"), CameraRetainedSection(Section,RetainedCurrent));
    if(!CameraCurrent(TEXT("camera_insertion_after_return_observation")))return Insertion?FString():Encode(R);
    if (Loaded(Section) && Section->GetClass() == UMovieSceneCameraCutSection::StaticClass() && Section->GetOuter() == Track)
    {
        R.Data->SetNumberField(TEXT("camera_section_reuse_observed_count"), Section == ExpectedReuse ? 1 : 0); R.Data->SetNumberField(TEXT("camera_section_new_membership_count"), Section != ExpectedReuse ? 1 : 0);
        R.Data->SetNumberField(TEXT("set_camera_section_range_calls"), 1);if(Insertion&&!Insertion->PlanFinalRange())return Insertion?FString():Encode(R);Section->SetRange(WantedRange);
        if(!CameraCurrent(TEXT("camera_insertion_after_set_range")))return Insertion?FString():Encode(R);R.Data->SetStringField(TEXT("camera_cut_section_name"), Section->GetFName().ToString()); R.Data->SetStringField(TEXT("camera_section_path"), Section->GetPathName());
        R.Data->SetNumberField(TEXT("mark_package_dirty_calls"), 1); C.Sequence->MarkPackageDirty();if(!CameraCurrent(TEXT("camera_insertion_after_dirty")))return Insertion?FString():Encode(R);
    }
    auto After = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("retained_previous_camera_section_after_mutation"), CameraRetainedSection(ExpectedReuse,C.Director?TFunction<bool()>([&]{return CameraCurrent(TEXT("director_previous_section_export_return"));}):TFunction<bool()>()));
    if (!MaintenanceCapture(C, R, TEXT("after_observation"), After)) { return Insertion?FString():Encode(R); } R.bChanged = CameraSelectedChanged(C.Before, After);PublicationExpected=After;
    const bool Current = CameraTrackIdentity(C, Track) && CameraSectionIdentity(Track, Section) && CameraSections(Track).Contains(Section);
    bool PreviousAttached = !ExpectedReuse || (CameraTrackIdentity(C, Track) && CameraSections(Track).Contains(ExpectedReuse));
    if (!PreviousAttached) { R.Data->SetArrayField(TEXT("retired_camera_section_paths"), { MakeShared<FJsonValueString>(PreviousPath) }); R.Data->SetNumberField(TEXT("camera_section_retirement_count"), 1); }
    const bool Reused = ExpectedReuse && Section == ExpectedReuse, BindingOK = Current && Section->GetCameraBindingID() == WantedBinding, RangeOK = Current && Section->GetRange() == WantedRange;
    const bool Preserved = Current && PreviousAttached && (ExpectedReuse ? Reused : !Reused) && CameraMutationPreserved(C, Track, Section, Reused, WantedBinding, WantedRange, Acquired, After);
    if(Insertion&&Insertion->Stopped)return Insertion?FString():Encode(R);
    R.Data->SetBoolField(TEXT("camera_current_section_verified"), Current); R.Data->SetBoolField(TEXT("camera_binding_id_verified"), BindingOK); R.Data->SetBoolField(TEXT("camera_floor_range_verified"), RangeOK); R.Data->SetBoolField(TEXT("camera_reuse_verified"), ExpectedReuse ? Reused : !Reused); R.Data->SetBoolField(TEXT("camera_previous_sections_attached"), PreviousAttached); R.Data->SetBoolField(TEXT("nonselected_state_preserved"), Preserved);
    if (!Current || !BindingOK || !RangeOK || !Preserved)
    { Fail(R, TEXT("sequencer_camera_postcondition_failed"), TEXT("Actual current cut/binding/floor/expected allocation-or-reuse/retained section or unrelated parent state differs. Retained prefix returned; save suppressed and no rollback/retry.")); return Insertion?FString():Encode(R); }
    auto Final = After; const bool SaveOK = MaintenanceSave(C, R, After, Final);
    if(!R.bKnown||(Insertion&&Insertion->Stopped))return Insertion?FString():Encode(R);
    if (!Final->GetBoolField(TEXT("complete"))) { return Insertion?FString():Encode(R); }
    PublicationExpected=Final;
    R.Data->SetObjectField(TEXT("retained_previous_camera_section_after_save"), CameraRetainedSection(ExpectedReuse));
    UMovieSceneTrack* FinalBase = C.Scene->GetCameraCutTrack(); UMovieSceneCameraCutTrack* FinalTrack = Loaded(FinalBase) && FinalBase->GetClass() == UMovieSceneCameraCutTrack::StaticClass() ? static_cast<UMovieSceneCameraCutTrack*>(FinalBase) : nullptr;
    const int32 AfterCount = FinalTrack ? CameraSections(FinalTrack).Num() : 0; bool Relocated = false, FinalRangeOK = false, FinalBindingOK = false;
    if (FinalTrack && CameraTrackIdentity(C, FinalTrack) && Loaded(Section) && Section->GetClass() == UMovieSceneCameraCutSection::StaticClass())
    {
        for (UMovieSceneSection* Row : CameraSections(FinalTrack))
        {
            if (!Loaded(Row) || Row->GetClass() != UMovieSceneCameraCutSection::StaticClass() || Row->GetFName() != Section->GetFName()) { continue; }
            auto* Cut = static_cast<UMovieSceneCameraCutSection*>(Row); Relocated = Cut == Section && CameraSectionIdentity(FinalTrack, Cut);
            FinalRangeOK = Relocated && Cut->GetRange() == WantedRange; FinalBindingOK = Relocated && Cut->GetCameraBindingID() == WantedBinding; break;
        }
    }
    auto AfterInfo = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("after_info"), AfterInfo);
    if (!MaintenanceInfo(C, AfterInfo)) { R.bKnown = false; Fail(R, TEXT("sequencer_camera_info_unobservable"), TEXT("Terminal loaded native Info is incomplete after retained mutation/save.")); return Insertion?FString():Encode(R); } AfterInfo->SetNumberField(TEXT("camera_cut_section_count"), AfterCount);
    if(!CameraCurrent(TEXT("camera_insertion_terminal_observation")))return Insertion?FString():Encode(R);
    const bool Incremented = AfterCount == BeforeCount + 1, SaveGate = !bSave || R.bSaved; const bool Specific = Relocated && Incremented && FinalRangeOK && SaveGate;
    const bool TypedReuse=Insertion&&Insertion->ExactReuse;const bool ReuseVerified=TypedReuse&&Relocated&&FinalBindingOK&&FinalRangeOK&&AfterCount==BeforeCount&&Section==ExpectedReuse&&R.Data->GetNumberField(TEXT("camera_section_new_membership_count"))==0&&R.Data->GetNumberField(TEXT("camera_section_reuse_observed_count"))==1&&R.Data->GetNumberField(TEXT("camera_section_retirement_count"))==0&&R.Data->GetBoolField(TEXT("nonselected_state_preserved"))&&SaveGate;
    if(TypedReuse){R.Data->SetBoolField(TEXT("camera_cut_exact_reuse_verified"),ReuseVerified);R.Data->SetBoolField(TEXT("typed_engine_postcondition_passed"),ReuseVerified);R.Data->SetStringField(TEXT("public_success_authority"),TEXT("original_section_name_range_count_plus_one_save_postcondition; typed reuse observations do not override public success"));R.Data->SetBoolField(TEXT("camera_cut_reuse_count_preserved"),AfterCount==BeforeCount);R.Data->SetBoolField(TEXT("camera_cut_reuse_legacy_count_incremented"),Incremented);R.Data->SetStringField(TEXT("camera_cut_reuse_success_scope"),TEXT("Typed current reuse/name/range/binding/order/source/nonselected/save proof is diagnostic. Original count+1 is required for public success and remains false for reuse even after a requested successful save. Requested created_count=1 is not an actual allocation count; historical receipts are unchanged."));}
    R.Data->SetNumberField(TEXT("camera_cut_section_count_after"), AfterCount); auto Checks = MakeShared<FJsonObject>(); Checks->SetBoolField(TEXT("created_section_relocated"), Relocated); Checks->SetBoolField(TEXT("section_count_incremented"), Incremented); Checks->SetBoolField(TEXT("section_range_matches"), FinalRangeOK); Checks->SetBoolField(TEXT("save_gate"), SaveGate); R.Data->SetObjectField(TEXT("verification_predicates"), Checks);
    R.Data->SetBoolField(TEXT("camera_terminal_current_binding_verified"), FinalBindingOK);
    auto Delta = MaintenanceDelta(1, 0, 0, 0, Specific ? 0 : 1); Delta->SetNumberField(TEXT("created_count"), 1);
    if (SaveOK || !R.bSaved)
    {
        MaintenanceLegacy(R, BeforeInfo, AfterInfo, Delta, true, Specific, TEXT("level_sequence_camera_cut_section_relookup"), { TEXT("created_section_relocated"), TEXT("section_count_incremented"), TEXT("section_range_matches"), TEXT("save_gate") });
        const auto Contract = R.Data->GetObjectField(TEXT("legacy_mutation_contract"));
        for (const TCHAR* Field : { TEXT("camera_binding_id"), TEXT("camera_cut_section_name"), TEXT("start_frame"), TEXT("end_frame") }) { Contract->SetField(Field, R.Data->TryGetField(Field)); }
        R.Data->SetStringField(TEXT("legacy_delta_scope"), TEXT("Original fixed affected_count=1/created_count=1/changed=True/performed=True even if the Engine reused an existing section. Actual allocation/reuse/retirement counters remain independent."));
        R.Data->SetStringField(TEXT("verification_effect_scope"), TEXT("Current loaded native Info and current section FName/range/count relookup. Original extra AssetLibrary load/Info lookups are not replayed; no camera Actor class/render/runtime/cold proof."));
    }
    R.bPost = SaveOK && Specific && FinalBindingOK;
    if (!R.bPost) { if (R.Code.IsEmpty()) { Fail(R, TEXT("mutation_postcondition_failed"), TEXT("Original strict cut count+1/name/range/save gate failed after real public allocation/reuse/change/save. Typed reuse proof does not override that predicate; retained partial state/save receipt is authoritative, no retry.")); } return Insertion?FString():Encode(R); }
    R.bSuccess = true; R.Stage = TEXT("complete"); return Insertion?FString():Encode(R);

    };
    const FString LegacyEncoded=RunMutation(); // FScopedTransaction has now unwound.
    if(!Insertion)return LegacyEncoded; // Existing empty/reuse serialization/lifecycle stays unchanged.
    R.Data->SetBoolField(TEXT("camera_insertion_transaction_scope_returned"),true);
    if(Insertion->Stopped||!R.bKnown){CameraInsertionInvalidatePublication(R);return Encode(R);} // No new getters after unknown; stock RAII unwind was unavoidable.
    if(!Insertion->Check(TEXT("camera_insertion_after_transaction"))){CameraInsertionInvalidatePublication(R);return Encode(R);}
    if(PublicationExpected.IsValid())
    {
        auto Published=MakeShared<FJsonObject>();
        if(!MaintenanceCapture(C,R,TEXT("camera_insertion_publication_observation"),Published)){CameraInsertionInvalidatePublication(R);return Encode(R);}
        auto BeforeFence=MakeShared<FJsonObject>(),AfterFence=MakeShared<FJsonObject>();BeforeFence->Values=PublicationExpected->Values;AfterFence->Values=Published->Values;
        bool SignaturesSame=true;for(const TCHAR* K:{TEXT("sequence_signature"),TEXT("movie_scene_signature")}){SignaturesSame&=CameraLiteral(BeforeFence->TryGetField(K),AfterFence->TryGetField(K));BeforeFence->RemoveField(K);AfterFence->RemoveField(K);}
        R.Data->SetBoolField(TEXT("camera_insertion_transaction_signatures_same"),SignaturesSame);
        const bool Preserved=CameraLiteral(MakeShared<FJsonValueObject>(BeforeFence),MakeShared<FJsonValueObject>(AfterFence));
        R.Data->SetBoolField(TEXT("camera_insertion_transaction_state_preserved"),Preserved);
        if(!Preserved){R.bSuccess=false;R.bPost=false;Fail(R,TEXT("sequencer_camera_insertion_transaction_changed"),TEXT("Complete final state differs after transaction finalization callbacks; actual mutation/save prefix retained, no rollback/replay."));}
    }
    else if(R.bSuccess)
    {R.bKnown=false;R.bPost=false;Fail(R,TEXT("sequencer_camera_insertion_publication_incomplete"),TEXT("No complete mutation publication state retained."));}
    const bool PublicationPreserved=R.Data->HasTypedField<EJson::Boolean>(TEXT("camera_insertion_transaction_state_preserved"))&&R.Data->GetBoolField(TEXT("camera_insertion_transaction_state_preserved"));
    if(!PublicationPreserved)CameraInsertionInvalidatePublication(R);else R.Data->SetBoolField(TEXT("camera_insertion_publication_fence_passed"),true); // Known legacy count failure does not erase independently verified typed reuse observations.
    return Encode(R);
}

namespace
{
bool EditorOpenConfigSnapshot(TSharedRef<FJsonObject>& Out)
{
    Out->SetBoolField(TEXT("complete"), false); Out->SetStringField(TEXT("path"), GEditorPerProjectIni);
    if (GEditorPerProjectIni.IsEmpty()) { return false; }
    const FString Filename = FPaths::ConvertRelativePathToFull(GEditorPerProjectIni);
    Out->SetStringField(TEXT("path"), Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    Out->SetBoolField(TEXT("exists"), Size >= 0); Out->SetNumberField(TEXT("bytes"), FMath::Max<int64>(Size, 0)); Out->SetStringField(TEXT("sha1"), TEXT(""));
    if (Size >= 0)
    {
        TArray<uint8> Bytes;
        if (Size > 8 * 1024 * 1024 || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; }
        uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash);
        Out->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize));
    }
    Out->SetBoolField(TEXT("complete"), true); return true;
}
bool EditorOpenObservation(FContext& C, UAssetEditorSubsystem* Subsystem, FResult& R, TSharedRef<FJsonObject>& Out)
{
    Out->SetBoolField(TEXT("complete"), false);
    auto SequenceFields = MakeShared<FJsonObject>(); auto SceneFields = MakeShared<FJsonObject>();
    auto Sources = MakeShared<FJsonObject>(); auto Disk = MakeShared<FJsonObject>();
    auto OtherDirty = MakeShared<FJsonObject>(); auto Config = MakeShared<FJsonObject>();
    Out->SetObjectField(TEXT("sequence_fields"), SequenceFields); Out->SetObjectField(TEXT("movie_scene_fields"), SceneFields);
    Out->SetObjectField(TEXT("source_classes"), Sources); Out->SetObjectField(TEXT("disk"), Disk);
    Out->SetObjectField(TEXT("other_package_dirty"), OtherDirty); Out->SetObjectField(TEXT("editor_per_project_ini"), Config);
    if (!Loaded(C.Sequence) || !Loaded(C.Scene) || !Loaded(Subsystem)) { return false; }
    Out->SetObjectField(TEXT("sequence"), Identity(C.Sequence)); Out->SetObjectField(TEXT("movie_scene"), Identity(C.Scene));
    Out->SetBoolField(TEXT("package_dirty"), C.Sequence->GetOutermost()->IsDirty());
    const bool Current = FindObject<ULevelSequence>(nullptr, *C.Sequence->GetPathName()) == C.Sequence && C.Sequence->GetMovieScene() == C.Scene;
    Out->SetBoolField(TEXT("current_target_identity"), Current);
    R.Data->SetNumberField(TEXT("find_editor_for_asset_calls"), R.Data->GetNumberField(TEXT("find_editor_for_asset_calls")) + 1);
    Out->SetBoolField(TEXT("editor_present"), Subsystem->FindEditorForAsset(C.Sequence, false) != nullptr);
    for (UClass* Class : { ULevelSequence::StaticClass(), UMovieScene::StaticClass() })
    {
        if (!Loaded(Class)) { return false; }
        auto Row = Identity(Class); Row->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(Class->GetClassFlags())));
        Row->SetBoolField(TEXT("package_dirty"), Class->GetOutermost()->IsDirty()); Sources->SetObjectField(Class->GetPathName(), Row);
    }
    if (!Current || !Properties(C.Sequence->GetClass(), C.Sequence, C.Sequence, {}, SequenceFields)
        || !Properties(C.Scene->GetClass(), C.Scene, C.Scene, {}, SceneFields)
        || !MaintenanceDisk(C.Sequence, Disk) || !EditorOpenConfigSnapshot(Config)) { return false; }
    int32 Count = 0;
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        if (*It == C.Sequence->GetOutermost() || !Loaded(*It)) { continue; }
        if (++Count > 8192) { return false; } OtherDirty->SetBoolField(It->GetPathName(), It->IsDirty());
    }
    Out->SetBoolField(TEXT("complete"), true); return true;
}
}
FString OpenInEditor(const FString& SequencePath)
{
    FResult R; InitMaintenance(R, TEXT("open_level_sequence_in_editor"), false); FContext C;
    for (const TCHAR* Key : { TEXT("asset_editor_subsystem_reads"), TEXT("find_editor_for_asset_calls"), TEXT("open_editor_for_asset_calls") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetField(TEXT("opened"), MakeShared<FJsonValueNull>()); R.Data->SetBoolField(TEXT("open_editor_call_returned"), false);
    R.Data->SetBoolField(TEXT("observations_complete"), false); R.Data->SetBoolField(TEXT("UI_operation"), true);
    R.Data->SetBoolField(TEXT("legacy_mutation_contract_available"), false);
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Explicit Editor UI leaf: original OpenEditorForAsset default arguments may focus an existing editor, show progress/notification, load Editor modules, invoke asset actions/toolkit initialization, broadcast delegates, update MRU/cache and flush GEditorPerProjectIni. One native outer transaction; no explicit target Modify, compile or asset save; primary target load has its own actual receipt. Implicit asset/config/cache work is not exhaustively measured and opening is not a pure read."));
    R.Data->SetStringField(TEXT("changed_observation_scope"), TEXT("Bounded actual current target identities and reflected Sequence/MovieScene fields, target dirty/disk, loaded native source-class identities, loaded-package dirty rows, disk GEditorPerProjectIni and primary editor presence. It does not certify nested curve/body equality, in-memory GConfig, UI focus/visibility, callback exhaustiveness, rendering or runtime evaluation."));
    R.Data->SetStringField(TEXT("legacy_success_authority"), TEXT("Actual OpenEditorForAsset returned bool; FindEditorForAsset(false) is an additional separately scoped presence observation, not substituted for that bool. Original handler had no mutation_result_v1."));
    R.Stage = TEXT("preflight"); if (!Resolve(SequencePath, C, R,true)) { return Encode(R); }
    R.Data->SetStringField(TEXT("sequence"), C.Sequence->GetPathName());
    if (!GEditor) { Fail(R, TEXT("sequencer_open_editor_unavailable"), TEXT("GEditor is unavailable")); return Encode(R); }
    R.Data->SetNumberField(TEXT("asset_editor_subsystem_reads"), 1); UAssetEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!Loaded(Subsystem)) { Fail(R, TEXT("sequencer_open_editor_subsystem_unavailable"), TEXT("AssetEditorSubsystem is unavailable")); return Encode(R); }
    auto Before = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("before_observation"), Before);
    R.Stage = TEXT("observe_before");
    if (!EditorOpenObservation(C, Subsystem, R, Before)) { Fail(R, TEXT("sequencer_open_editor_before_unobservable"), TEXT("Bounded UI/target/config baseline is incomplete; no OpenEditorForAsset dispatch.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("already_present_before"), Before->GetBoolField(TEXT("editor_present")));
    R.Stage = TEXT("open_editor"); R.Data->SetNumberField(TEXT("outer_transaction_calls"), 1);
    FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP", "OpenLevelSequenceEditor", "Open Level Sequence in Editor"));
    R.bAttempted = true; R.bKnown = false; R.Data->SetNumberField(TEXT("open_editor_for_asset_calls"), 1);
    const bool Opened = Subsystem->OpenEditorForAsset(C.Sequence);
    R.Data->SetBoolField(TEXT("open_editor_call_returned"), true); R.Data->SetBoolField(TEXT("opened"), Opened);
    auto Legacy = MakeShared<FJsonObject>(); Legacy->SetBoolField(TEXT("success"), Opened); Legacy->SetBoolField(TEXT("opened"), Opened); Legacy->SetStringField(TEXT("sequence"), C.Sequence->GetPathName());
    if (!Opened) { Legacy->SetStringField(TEXT("error"), TEXT("OpenEditorForAsset returned false")); }
    R.Data->SetObjectField(TEXT("legacy_open_result"), Legacy);
    auto After = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("after_observation"), After); R.Stage = TEXT("observe_after");
    if (!EditorOpenObservation(C, Subsystem, R, After))
    { Fail(R, TEXT("sequencer_open_editor_after_unobservable"), TEXT("OpenEditorForAsset returned; bounded after state is incomplete. Legacy bool and raw prefix retained, no retry or automatic editor closure.")); return Encode(R); }
    FString Mismatch; int32 Visited = 0;
    const bool SameObservation = MaintenanceRemovalEqual(MakeShared<FJsonValueObject>(Before), MakeShared<FJsonValueObject>(After), Mismatch, TEXT("$"), Visited);
    R.Data->SetStringField(TEXT("observation_first_mismatch"), Mismatch); R.Data->SetNumberField(TEXT("observation_comparison_values_visited"), Visited);
    R.Data->SetStringField(TEXT("observation_comparison_scope"), TEXT("Strict same JSON types/scalars and array order; object key order is ignored by the already reviewed comparison helper, only for this new UI observation. No fields/flags/text are masked and old operation comparisons are unchanged."));
    R.bKnown = true; R.bChanged = !SameObservation; R.Data->SetBoolField(TEXT("observations_complete"), true);
    auto FieldPreserved = [&](const TCHAR* Key)
    {
        FString FieldMismatch; int32 FieldVisited = 0;
        return MaintenanceRemovalEqual(Before->TryGetField(Key), After->TryGetField(Key), FieldMismatch, Key, FieldVisited);
    };
    R.Data->SetBoolField(TEXT("editor_present_after"), After->GetBoolField(TEXT("editor_present")));
    R.Data->SetBoolField(TEXT("editor_presence_changed"), Before->GetBoolField(TEXT("editor_present")) != After->GetBoolField(TEXT("editor_present")));
    bool TargetState = true;
    for (const TCHAR* Key : { TEXT("sequence"), TEXT("movie_scene"), TEXT("sequence_fields"), TEXT("movie_scene_fields"), TEXT("package_dirty"), TEXT("current_target_identity") }) { TargetState &= FieldPreserved(Key); }
    R.Data->SetBoolField(TEXT("target_observed_state_preserved"), TargetState);
    R.Data->SetBoolField(TEXT("source_classes_preserved"), FieldPreserved(TEXT("source_classes")));
    R.Data->SetBoolField(TEXT("asset_file_preserved"), FieldPreserved(TEXT("disk")));
    R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), FieldPreserved(TEXT("other_package_dirty")));
    R.Data->SetBoolField(TEXT("editor_config_file_preserved"), FieldPreserved(TEXT("editor_per_project_ini")));
    R.bPost = Opened;
    if (!Opened) { Fail(R, TEXT("sequencer_open_editor_failed"), TEXT("OpenEditorForAsset returned false")); return Encode(R); }
    R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
}
