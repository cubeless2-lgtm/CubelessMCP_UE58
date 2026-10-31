#include "Compatibility/UE58/CubelessAnimationEditingApi.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimMetaData.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Crc.h"
#include "Misc/CoreMisc.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::AnimationEditing
{
namespace
{
using J=FJsonObject;
using Snapshot=TMap<FString,FString>;
bool Writing=false;
TMap<FString,FString> Receipts;
TArray<FString> ReceiptOrder;
// Per live UObject (weak serial included), not per spelling/path or saved metadata.
// A later warm call must not turn an unobserved PostLoad into trusted persistence.
TSet<TWeakObjectPtr<UAnimMontage>> UnobservedLoadedMontages;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Encode(const TSharedRef<J>& R){FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;}
FString Path(const UObject* O){return O?O->GetPathName():TEXT("");}
FString Number(double X){return FString::Printf(TEXT("%.17g"),X);}
template<class T> FString StructText(const T& Value)
{FString S;T::StaticStruct()->ExportText(S,&Value,nullptr,nullptr,PPF_None,nullptr);return S;}
FString CurveText(const FFloatCurve& C)
{
 FString V=StructText(C)+TEXT("|default=")+Number(C.FloatCurve.DefaultValue);
 for(const auto& K:C.FloatCurve.GetConstRefOfKeys())
 {V+=TEXT("|key=")+Number(K.Time)+TEXT(",")+Number(K.Value);
  if(K.InterpMode!=RCIM_Linear||K.TangentMode!=RCTM_Auto)V+=TEXT(",tangents=")+Number(K.ArriveTangent)+TEXT(",")+Number(K.LeaveTangent)+TEXT(",")+Number(K.ArriveTangentWeight)+TEXT(",")+Number(K.LeaveTangentWeight);}
 return V;
}
void Properties(UObject* O,const FString& Prefix,Snapshot& Out)
{
 if(!IsValid(O))return;
 Out.Add(Prefix+TEXT(".$class"),Path(O->GetClass()));
 for(TFieldIterator<FProperty> P(O->GetClass());P;++P)
 {
  if(!P->HasAnyPropertyFlags(CPF_Edit)||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_DuplicateTransient))continue;
  const FName N=P->GetFName();
  // These authored collections are captured separately without engine cache pointers.
  if(O->IsA<UAnimSequenceBase>()&&(N==TEXT("Notifies")||N==TEXT("AnimNotifyTracks")||N==TEXT("AuthoredSyncMarkers")||N==TEXT("CompositeSections")))continue;
  FString V;P->ExportTextItem_Direct(V,P->ContainerPtrToValuePtr<void>(O),nullptr,O,PPF_None);
  if(const auto* Numeric=CastField<FNumericProperty>(*P);Numeric&&Numeric->IsFloatingPoint())V+=TEXT("|exact=")+Number(Numeric->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(O)));
  Out.Add(Prefix+TEXT(".")+P->GetName(),MoveTemp(V));
 }
}
struct FObjectLease
{
 TStrongObjectPtr<UObject> Object,Outer;
 TStrongObjectPtr<UClass> Class;
 TWeakObjectPtr<UObject> Serial;
 FString ObjectPath;
 bool Present=false;
 bool Init(UObject* O)
 {
  if(!O)return true;if(!Live(O)||!Live(O->GetClass()))return false;
  Present=true;Object.Reset(O);Outer.Reset(O->GetOuter());Class.Reset(O->GetClass());Serial=O;ObjectPath=Path(O);return true;
 }
 bool Matches(UObject* O) const
 {
  if(!Present)return O==nullptr;
  return O==Object.Get()&&Serial.IsValid()&&Serial.Get()==O&&Live(O)&&Live(Class.Get())&&
   O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&Path(O)==ObjectPath&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists);
 }
};
struct FNotifyLease{FObjectLease Notify,State;};
using FNotifyLeases=TMap<FGuid,FNotifyLease>;
bool HoldNotify(const FAnimNotifyEvent& N,FNotifyLeases& Leases)
{
 FNotifyLease V;if(!V.Notify.Init(N.Notify)||!V.State.Init(N.NotifyStateClass))return false;
 Leases.Add(N.Guid,MoveTemp(V));return true;
}
bool CheckNotifyLeases(UAnimSequenceBase* A,const FNotifyLeases& Leases,const TSharedRef<J>& R)
{
 if(A->Notifies.Num()!=Leases.Num()){R->SetStringField(TEXT("first_mismatch_property"),TEXT("notify.identity_count"));return false;}
 for(const auto& N:A->Notifies)
 {
  const FNotifyLease* V=Leases.Find(N.Guid);
  if(!V||!V->Notify.Matches(N.Notify)||!V->State.Matches(N.NotifyStateClass))
  {
   R->SetStringField(TEXT("first_mismatch_property"),TEXT("notify.")+N.Guid.ToString()+TEXT(".identity_owner_lifetime"));
   R->SetStringField(TEXT("expected"),V?V->Notify.ObjectPath+TEXT(" | ")+V->State.ObjectPath:TEXT("<absent GUID>"));
   R->SetStringField(TEXT("actual"),Path(N.Notify)+TEXT(" | ")+Path(N.NotifyStateClass));return false;
  }
 }
 return true;
}
void SkeletonState(USkeleton* S,Snapshot& Out)
{
 Properties(S,TEXT("skeleton"),Out);
 for(int32 I=0;I<S->Sockets.Num();++I)Properties(S->Sockets[I],FString::Printf(TEXT("skeleton.socket%d"),I),Out);
 const auto& Ref=S->GetReferenceSkeleton();const auto& Poses=Ref.GetRawRefBonePose();
 Out.Add(TEXT("skeleton.rawBoneCount"),FString::FromInt(Poses.Num()));
 for(int32 I=0;I<Poses.Num();++I)
 {
  const FTransform& T=Poses[I];const FVector P=T.GetTranslation(),Scale=T.GetScale3D();const FQuat Q=T.GetRotation();
  FString V=Ref.GetBoneName(I).ToString()+TEXT("|")+FString::FromInt(Ref.GetParentIndex(I));
  for(double X:{P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,Scale.X,Scale.Y,Scale.Z})V+=TEXT("|")+Number(X);
  Out.Add(FString::Printf(TEXT("skeleton.bone%d"),I),MoveTemp(V));
 }
}
void Notify(const FAnimNotifyEvent& N,Snapshot& Out)
{
 const FString K=TEXT("notify.")+N.Guid.ToString()+TEXT(".");
 Out.Add(K+TEXT("time"),Number(N.GetTime()));Out.Add(K+TEXT("duration"),Number(N.GetDuration()));
 Out.Add(K+TEXT("track"),FString::FromInt(N.TrackIndex));
 Out.Add(K+TEXT("startSlot"),FString::FromInt(N.GetSlotIndex()));Out.Add(K+TEXT("startLinkMethod"),FString::FromInt(N.GetLinkMethod()));
 Out.Add(K+TEXT("endSlot"),FString::FromInt(N.EndLink.GetSlotIndex()));Out.Add(K+TEXT("endLinkMethod"),FString::FromInt(N.EndLink.GetLinkMethod()));
 // Include every reflected authored event field; link caches/offsets are derived.
 for(TFieldIterator<FProperty> P(FAnimNotifyEvent::StaticStruct());P;++P)
 {
  if(!P->HasAnyPropertyFlags(CPF_Edit)||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))continue;
  FString V;P->ExportTextItem_Direct(V,P->ContainerPtrToValuePtr<void>(&N),nullptr,nullptr,PPF_None);
  if(const auto* Numeric=CastField<FNumericProperty>(*P);Numeric&&Numeric->IsFloatingPoint())V+=TEXT("|exact=")+Number(Numeric->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(&N)));
  Out.Add(K+P->GetName(),MoveTemp(V));
 }
 Out.Add(K+TEXT("object"),Path(N.Notify));Out.Add(K+TEXT("state"),Path(N.NotifyStateClass));
 Properties(N.Notify,K+TEXT("values"),Out);Properties(N.NotifyStateClass,K+TEXT("stateValues"),Out);
}
bool Capture(UAnimSequenceBase* A,Snapshot& Out,FString& Error)
{
 Out.Reset();if(!IsValid(A)){Error=TEXT("Asset became invalid");return false;}
 Properties(A,TEXT("asset"),Out);
 Out.Add(TEXT("$path"),Path(A));Out.Add(TEXT("$outer"),Path(A->GetOuter()));
 Out.Add(TEXT("$skeleton"),Path(A->GetSkeleton()));Out.Add(TEXT("$length"),Number(A->GetPlayLength()));
 Out.Add(TEXT("notify.count"),FString::FromInt(A->Notifies.Num()));
 if(A->Notifies.Num()>4096||A->AnimNotifyTracks.Num()>256){Error=TEXT("Notify4096/track256 snapshot budget exceeded");return false;}
 TSet<FGuid> Ids;
 for(const auto& N:A->Notifies)
 {
  if(!N.Guid.IsValid()||Ids.Contains(N.Guid)){Error=TEXT("Notify GUIDs must be valid and unique; repair explicitly before editing");return false;}
  Ids.Add(N.Guid);Notify(N,Out);
 }
 Out.Add(TEXT("track.count"),FString::FromInt(A->AnimNotifyTracks.Num()));
 for(int32 I=0;I<A->AnimNotifyTracks.Num();++I)
 {const auto& T=A->AnimNotifyTracks[I];Out.Add(FString::Printf(TEXT("track.%d"),I),T.TrackName.ToString()+TEXT("|")+T.TrackColor.ToString());}
 if(UAnimSequence* S=Cast<UAnimSequence>(A))
 {
  if(!S->IsDataModelValid()||!IsValid(S->GetDataModelInterface().GetObject())){Error=TEXT("Sequence has no live data model");return false;}
  Out.Add(TEXT("model"),Path(S->GetDataModelInterface().GetObject()));
  int64 Keys=0;const auto& Curves=S->GetDataModel()->GetFloatCurves();
  if(Curves.Num()>1024){Error=TEXT("Float curve1024 budget exceeded");return false;}
  for(const auto& C:Curves){Keys+=C.FloatCurve.GetNumKeys();Out.Add(TEXT("curve.")+C.GetName().ToString(),CurveText(C));}
  for(const auto& C:S->GetDataModel()->GetTransformCurves())Out.Add(TEXT("transformCurve.")+C.GetName().ToString(),StructText(C));
  if(Keys>65536){Error=TEXT("Float curve key65536 budget exceeded");return false;}
  // Public installed model API; bone tracks are preservation data, never edited here.
  PRAGMA_DISABLE_DEPRECATION_WARNINGS
  for(const auto& B:S->GetDataModel()->GetBoneAnimationTracks())
  {
   const auto& D=B.InternalTrackData;
   uint32 Hash=FCrc::MemCrc32(D.PosKeys.GetData(),D.PosKeys.Num()*sizeof(FVector3f));
   Hash=FCrc::MemCrc32(D.RotKeys.GetData(),D.RotKeys.Num()*sizeof(FQuat4f),Hash);
   Hash=FCrc::MemCrc32(D.ScaleKeys.GetData(),D.ScaleKeys.Num()*sizeof(FVector3f),Hash);
   Out.Add(TEXT("bone.")+B.Name.ToString(),FString::Printf(TEXT("%d/%d/%d/%08x"),D.PosKeys.Num(),D.RotKeys.Num(),D.ScaleKeys.Num(),Hash));
  }
  PRAGMA_ENABLE_DEPRECATION_WARNINGS
  Out.Add(TEXT("marker.count"),FString::FromInt(S->AuthoredSyncMarkers.Num()));
  if(S->AuthoredSyncMarkers.Num()>4096){Error=TEXT("Sync marker4096 budget exceeded");return false;}
  TSet<FGuid> MarkerIds;
  for(const auto& M:S->AuthoredSyncMarkers)
  {if(!M.Guid.IsValid()||MarkerIds.Contains(M.Guid)){Error=TEXT("Sync marker GUIDs must be valid and unique");return false;}MarkerIds.Add(M.Guid);Out.Add(TEXT("marker.")+M.Guid.ToString(),M.MarkerName.ToString()+TEXT("|")+Number(M.Time)+TEXT("|")+FString::FromInt(M.TrackIndex));}
 }
 if(UAnimMontage* M=Cast<UAnimMontage>(A))
 {
  Out.Add(TEXT("slot.count"),FString::FromInt(M->SlotAnimTracks.Num()));
  if(M->SlotAnimTracks.Num()>64){Error=TEXT("Slot64 budget exceeded");return false;}
  int32 TotalSegments=0;
  for(int32 I=0;I<M->SlotAnimTracks.Num();++I)
  {
   const auto& Slot=M->SlotAnimTracks[I];const FString Prefix=FString::Printf(TEXT("slot.%d"),I);
   Out.Add(Prefix+TEXT(".name"),Slot.SlotName.ToString());Out.Add(Prefix+TEXT(".segments"),FString::FromInt(Slot.AnimTrack.AnimSegments.Num()));
   TotalSegments+=Slot.AnimTrack.AnimSegments.Num();if(TotalSegments>4096){Error=TEXT("Segment4096 budget exceeded");return false;}
   for(int32 K=0;K<Slot.AnimTrack.AnimSegments.Num();++K)
   {
    const FAnimSegment& Segment=Slot.AnimTrack.AnimSegments[K];const FString Key=Prefix+FString::Printf(TEXT(".segment%d"),K);
    // CachedPlayLength is regenerable. Preserve reference and authored trim,
    // placement, play-rate and loop values, even though SlotAnimTracks lacks CPF_Edit.
    Out.Add(Key,Path(Segment.GetAnimReference())+TEXT("|")+Number(Segment.StartPos)+TEXT("|")+Number(Segment.AnimStartTime)+TEXT("|")+Number(Segment.AnimEndTime)+TEXT("|")+Number(Segment.AnimPlayRate)+TEXT("|")+FString::FromInt(Segment.LoopingCount));
   }
  }
  Out.Add(TEXT("section.count"),FString::FromInt(M->CompositeSections.Num()));
  if(M->CompositeSections.Num()>1024){Error=TEXT("Section1024 budget exceeded");return false;}
  for(int32 I=0;I<M->CompositeSections.Num();++I)
  {const auto& SectionValue=M->CompositeSections[I];const FString Prefix=FString::Printf(TEXT("section.%d"),I);Out.Add(Prefix,SectionValue.SectionName.ToString()+TEXT("|")+Number(SectionValue.GetTime())+TEXT("|")+SectionValue.NextSectionName.ToString());
   const auto& Metadata=SectionValue.GetMetaData();Out.Add(Prefix+TEXT(".metadataCount"),FString::FromInt(Metadata.Num()));
   for(int32 K=0;K<Metadata.Num();++K){Out.Add(Prefix+FString::Printf(TEXT(".metadata%d"),K),Path(Metadata[K]));Properties(Metadata[K],Prefix+FString::Printf(TEXT(".metadata%d.values"),K),Out);}}
 }
 int64 Bytes=0;for(const auto& Pair:Out)Bytes+=(Pair.Key.Len()+Pair.Value.Len())*sizeof(TCHAR);
 if(Bytes>16*1024*1024){Error=TEXT("Authored snapshot16MiB budget exceeded");return false;}
 return true;
}
bool Equal(const Snapshot& Expected,const Snapshot& Actual,TSharedRef<J> R)
{
 TArray<FString> Keys;Expected.GetKeys(Keys);for(const auto& P:Actual)Keys.AddUnique(P.Key);Keys.Sort();
 for(const auto& K:Keys)
 {const FString* E=Expected.Find(K),*V=Actual.Find(K);if(!E||!V||*E!=*V)
  {R->SetStringField(TEXT("first_mismatch_property"),K);R->SetStringField(TEXT("expected"),E?E->Left(4096):TEXT("<absent>"));R->SetStringField(TEXT("actual"),V?V->Left(4096):TEXT("<absent>"));return false;}}
 return true;
}
bool Name(const TSharedPtr<J>& P,const TCHAR* Key,FName& Out,bool Empty=false)
{
 FString S;if(!P->TryGetStringField(Key,S)||S.Len()>128||S.Contains(TEXT("\n"))||S.Contains(TEXT("\r"))||S.Contains(TEXT("/"))||S.Contains(TEXT("\\"))||S.Contains(TEXT(".")))return false;
 if(!Empty&&(S.IsEmpty()||S.Equals(TEXT("None"),ESearchCase::IgnoreCase)))return false;Out=FName(*S);return true;
}
bool Numeric(const TSharedPtr<J>& P,const TCHAR* K,float& V,float Low,float High)
{double D=0;if(!P->TryGetNumberField(K,D)||!FMath::IsFinite(D)||D<Low||D>High)return false;V=float(D);return FMath::IsFinite(V);}
bool SafeObjectPath(const FString& Value)
{
 if(Value.IsEmpty()||Value.Len()>2048||Value[0]!='/')return false;
 // Notify classes are top-level UClasses, not subobjects or export-text literals.
 const int32 Dot=Value.Find(TEXT("."));
 if(Dot<=1||Dot==Value.Len()-1||Value.Find(TEXT("."),ESearchCase::CaseSensitive,ESearchDir::FromStart,Dot+1)!=INDEX_NONE||Value.Contains(TEXT(":"))||Value.Contains(TEXT("'"))||Value.Contains(TEXT("\"")))return false;
 if(Value.Len()-Dot-1>=NAME_SIZE||Value.Find(TEXT("/"),ESearchCase::CaseSensitive,ESearchDir::FromStart,Dot+1)!=INDEX_NONE)return false;
 int32 PackageEnd=0;while(PackageEnd<Value.Len()&&Value[PackageEnd]!='.'&&Value[PackageEnd]!=':')++PackageEnd;
 if(PackageEnd>=NAME_SIZE)return false; // The complete package path is itself one FName.
 int32 Segment=0;
 for(const TCHAR C:Value)
 {
  if(C<32||C=='\\')return false;
  if(C=='/'||C=='.'||C==':')Segment=0;
  else if(++Segment>=NAME_SIZE)return false;
 }
 return true;
}
int32 Track(UAnimSequenceBase* A,FName N){return A->AnimNotifyTracks.IndexOfByPredicate([&](const auto& T){return T.TrackName==N;});}
}

FString TakeReceipt(const FString& Token)
{
 if(!IsInGameThread())return FString();FString Result;if(Receipts.RemoveAndCopyValue(Token,Result))ReceiptOrder.Remove(Token);return Result;
}
FString Edit(UObject* Object,const FString& Action,const FString& Arguments,bool Execute,bool Save,bool AllowDirty,const FString& RequestToken,bool UnobservedAssetLoad)
{
 TSharedRef<J> R=MakeShared<J>();R->SetStringField(TEXT("operation"),Action);R->SetBoolField(TEXT("success"),false);
 R->SetStringField(TEXT("stage"),TEXT("preflight"));R->SetBoolField(TEXT("mutation_attempted"),false);R->SetBoolField(TEXT("changed"),false);
 R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("compile_attempted"),false);
 R->SetBoolField(TEXT("save_attempted"),false);R->SetBoolField(TEXT("saved"),false);R->SetBoolField(TEXT("automatic_retry"),false);R->SetBoolField(TEXT("atomic_rollback"),false);
 R->SetBoolField(TEXT("save_state_known"),true);R->SetStringField(TEXT("request_token"),RequestToken);
 auto Publish=[&]()
 {
  FString Result=Encode(R);FGuid TokenGuid;
  if(IsInGameThread()&&RequestToken.Len()==32&&FGuid::Parse(RequestToken,TokenGuid))
  {
   auto Compact=MakeShared<J>();Compact->Values=R->Values;Compact->RemoveField(TEXT("skeleton_slot_groups"));
   if(!Receipts.Contains(RequestToken))ReceiptOrder.Add(RequestToken);Receipts.Add(RequestToken,Encode(Compact));
   while(ReceiptOrder.Num()>32){Receipts.Remove(ReceiptOrder[0]);ReceiptOrder.RemoveAt(0);}
  }
  return Result;
 };
 auto Fail=[&](const FString& Error){R->SetStringField(TEXT("error"),Error);
  if(!R->GetBoolField(TEXT("change_state_known")))R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());
  if(IsValid(Object))R->SetBoolField(TEXT("dirty_after"),Object->GetOutermost()->IsDirty());
  R->SetStringField(TEXT("retry_risk"),!R->GetBoolField(TEXT("change_state_known"))?TEXT("inspect_actual_target"):R->GetBoolField(TEXT("mutation_attempted"))?TEXT("mutation_attempted"):TEXT("none_observed"));return Publish();};
 if(!IsInGameThread()||IsGarbageCollecting()||UE::IsSavingPackage()||IsEngineExitRequested()||!GEditor||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor||Writing)return Fail(TEXT("Idle Editor game thread outside GC/save/exit and no conflicting animation edit required"));
 UAnimSequenceBase* A=Cast<UAnimSequenceBase>(Object);
 if(!Live(A)||(A->GetClass()!=UAnimSequence::StaticClass()&&A->GetClass()!=UAnimMontage::StaticClass())||A->IsTemplate()||A->GetOuter()!=A->GetOutermost()||A->GetOutermost()->HasAnyPackageFlags(PKG_ContainsMap)||!A->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))return Fail(TEXT("Exact fully loaded top-level project AnimSequence or AnimMontage asset outside map packages required"));
 if(auto* LoadedMontage=Cast<UAnimMontage>(A))
 {
  R->SetStringField(TEXT("asset"),Path(A));
  if(UnobservedAssetLoad)
  {
   for(auto It=UnobservedLoadedMontages.CreateIterator();It;++It)if(!It->IsValid())It.RemoveCurrent();
   UnobservedLoadedMontages.Add(LoadedMontage);
  }
  if(UnobservedLoadedMontages.Contains(LoadedMontage))
  {
   R->SetBoolField(TEXT("montage_load_guard_recorded"),true);
   R->SetBoolField(TEXT("montage_load_state_unverified"),true);
   R->SetStringField(TEXT("persistence_warning"),TEXT("This API loaded this Montage before its native snapshot. Stock PostLoad may have normalized authored links to slot0. Warm calls, inspection, dirty approval or later edits do not establish the pre-load disk values; no guarded save is permitted for this live object."));
   if(Save){R->SetStringField(TEXT("error_code"),TEXT("montage_unobserved_load_save_refused"));return Fail(TEXT("Save refused before Modify: Montage pre-load slot/link state was not observed. save=false remains available; no automatic retry or atomic rollback."));}
  }
 }
 if(A->HasParentAsset()||!A->ChildrenAssets.IsEmpty())return Fail(TEXT("Parented animation assets or loaded child assets require explicit propagation support; no target-only edit performed"));
 TGuardValue<bool> Lock(Writing,true);
 TStrongObjectPtr<UAnimSequenceBase> Held(A);TStrongObjectPtr<UPackage> Package(A->GetOutermost());TStrongObjectPtr<USkeleton> Skeleton(A->GetSkeleton());
 if(!IsValid(Skeleton.Get()))return Fail(TEXT("Live skeleton required"));
 TArray<TSharedPtr<FJsonValue>> SlotGroups;
 for(const auto& Group:Skeleton->GetSlotGroups())
 {auto Row=MakeShared<J>();Row->SetStringField(TEXT("group"),Group.GroupName.ToString());TArray<TSharedPtr<FJsonValue>> Slots;for(const auto Slot:Group.SlotNames)Slots.Add(MakeShared<FJsonValueString>(Slot.ToString()));Row->SetArrayField(TEXT("slots"),Slots);SlotGroups.Add(MakeShared<FJsonValueObject>(Row));}
 R->SetArrayField(TEXT("skeleton_slot_groups"),SlotGroups);
 const FString AssetPath=Path(A),PackageName=Path(Package.Get());const UClass* AssetClass=A->GetClass();
 UEditorEngine* const Editor=GEditor;
 const bool WasDirty=Package->IsDirty();R->SetBoolField(TEXT("dirty_before"),WasDirty);R->SetStringField(TEXT("asset"),AssetPath);
 if(Save&&WasDirty&&!AllowDirty)return Fail(TEXT("Saving a preexisting dirty package requires allow_save_dirty_package=true"));
 FString Filename;if(Save&&(!FPackageName::TryConvertLongPackageNameToFilename(PackageName,Filename,FPackageName::GetAssetPackageExtension())||IFileManager::Get().IsReadOnly(*Filename)))return Fail(TEXT("Writable package filename required before editing"));
 TSharedPtr<J> P;if(Arguments.Len()>262144||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Arguments),P)||!P)return Fail(TEXT("Bounded JSON object required"));
 Snapshot Before,Expected,Current,SkeletonBefore;FString Error;
 if(!Capture(A,Before,Error))return Fail(Error);Expected=Before;SkeletonState(Skeleton.Get(),SkeletonBefore);
 FNotifyLeases NotifyLeases;for(const auto& N:A->Notifies)if(!HoldNotify(N,NotifyLeases))return Fail(TEXT("Live existing Notify objects/classes required"));
 TStrongObjectPtr<UClass> NotifyClass;TStrongObjectPtr<UObject> NotifyCDO;
 Snapshot CdoBefore;FObjectLease CdoLease;bool CdoFrozen=false;FString CdoPath,NotifyClassPath,ScriptError;
 FScopedScriptExceptionHandler ScriptExceptions([&](ELogVerbosity::Type Verbosity,const TCHAR* Message,const TCHAR* Stack)
 {
  if(Verbosity==ELogVerbosity::Error&&ScriptError.IsEmpty())ScriptError=FString(Message).Left(4096);
  FScriptExceptionHandler::LoggingExceptionHandler(Verbosity,Message,Stack);
 });
 auto Check=[&](const Snapshot& Want)->bool
 {
  auto Unknown=[&](){R->SetBoolField(TEXT("change_state_known"),false);return false;};
  if(!ScriptError.IsEmpty()){R->SetStringField(TEXT("script_exception"),ScriptError);Error=TEXT("Engine callback raised a script exception; no further edit/save is issued");return Unknown();}
  if(!IsInGameThread()||GEditor!=Editor||!GEditor||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor||IsGarbageCollecting()||UE::IsSavingPackage()||IsEngineExitRequested()){Error=TEXT("Editor execution context changed across callback");return Unknown();}
  if(!Live(A)||A->HasParentAsset()||!A->ChildrenAssets.IsEmpty()||A->GetClass()!=AssetClass||Path(A)!=AssetPath||A->GetOutermost()!=Package.Get()||A->GetSkeleton()!=Skeleton.Get()||!Live(Skeleton.Get())){Error=TEXT("Asset/skeleton lifetime, identity, ownership or parent/child propagation scope changed");return Unknown();}
  if(!CheckNotifyLeases(A,NotifyLeases,R)){Error=TEXT("Notify object identity, owner or lifetime changed across callback");return Unknown();}
  if(CdoFrozen)
  {
   if(!Live(NotifyClass.Get())||NotifyClass->HasAnyClassFlags(CLASS_NewerVersionExists)||Path(NotifyClass.Get())!=NotifyClassPath||!CdoLease.Matches(NotifyCDO.Get())||NotifyCDO->GetClass()!=NotifyClass.Get()||NotifyClass->GetDefaultObject(false)!=NotifyCDO.Get()||Path(NotifyCDO.Get())!=CdoPath){Error=TEXT("Notify CDO/class identity, owner or lifetime changed");return Unknown();}
   Snapshot CurrentCdo;Properties(NotifyCDO.Get(),TEXT("notify_cdo"),CurrentCdo);if(!Equal(CdoBefore,CurrentCdo,R)){Error=TEXT("Notify CDO authored defaults changed across callback");return Unknown();}
  }
  if(!Capture(A,Current,Error))return Unknown();
  if(!Equal(Want,Current,R)){Error=TEXT("Unexpected authored asset change across engine callback");return Unknown();}
  Snapshot ActualSkeleton;SkeletonState(Skeleton.Get(),ActualSkeleton);
  if(!Equal(SkeletonBefore,ActualSkeleton,R)){Error=TEXT("Skeleton authored state changed");return Unknown();}return true;
 };
 UAnimSequence* S=Cast<UAnimSequence>(A);UAnimMontage* M=Cast<UAnimMontage>(A);
 float TimelineLength=A->GetPlayLength();
 if(M)
 {
  TimelineLength=0;for(const auto& Slot:M->SlotAnimTracks)TimelineLength=FMath::Max(TimelineLength,Slot.AnimTrack.GetLength());
  if(!FMath::IsFinite(TimelineLength)||TimelineLength<0)return Fail(TEXT("Finite montage segment timeline required"));
  for(const auto& V:M->Notifies)if(!FMath::IsFinite(V.GetTime())||!FMath::IsFinite(V.GetDuration())||V.GetTime()<0||V.GetTime()+V.GetDuration()>TimelineLength+KINDA_SMALL_NUMBER)return Fail(TEXT("Existing Montage Notify lies outside the segment timeline; repair explicitly before edit/save"));
  for(const auto& V:M->CompositeSections)if(!FMath::IsFinite(V.GetTime())||V.GetTime()<0||V.GetTime()>TimelineLength+KINDA_SMALL_NUMBER)return Fail(TEXT("Existing Montage section lies outside the segment timeline; repair explicitly before edit/save"));
 }

 FName N,Other;float Time=0,Duration=0;int32 Index=INDEX_NONE,TrackIndex=INDEX_NONE;
 TArray<FRichCurveKey> Keys;
 const bool Curve=Action==TEXT("add_float_curve")||Action==TEXT("remove_float_curve")||Action==TEXT("set_float_curve_keys");
 const bool AddNotify=Action==TEXT("add_notify")||Action==TEXT("add_notify_state");
 const bool NotifyEdit=Action==TEXT("remove_notify")||Action==TEXT("set_notify_timing");
 const bool Marker=Action==TEXT("add_sync_marker")||Action==TEXT("remove_sync_markers")||Action==TEXT("rename_sync_marker");
 const bool Section=Action==TEXT("add_montage_section")||Action==TEXT("remove_montage_section")||Action==TEXT("set_montage_section_next")||Action==TEXT("set_montage_section_time");
 const bool TrackEdit=Action==TEXT("add_notify_track")||Action==TEXT("remove_notify_track");
 if(Curve)
 {
  if(!S||!Name(P,TEXT("curve_name"),N))return Fail(TEXT("Sequence and explicit curve_name required"));
  const FFloatCurve* C=static_cast<const FFloatCurve*>(S->GetDataModel()->FindCurve(FAnimationCurveIdentifier(N,ERawCurveTrackTypes::RCT_Float)));
  if((Action==TEXT("add_float_curve"))==(C!=nullptr))return Fail(TEXT("Curve already exists or selected float curve is absent"));
  if(Action==TEXT("set_float_curve_keys"))
  {
   const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!P->TryGetArrayField(TEXT("keys"),Rows)||Rows->Num()>16384)return Fail(TEXT("keys must be at most16384 [time,value] pairs"));
   float Previous=-1;for(const auto& V:*Rows)
   {const TArray<TSharedPtr<FJsonValue>>* Pair=nullptr;double T=0,Y=0;if(!V->TryGetArray(Pair)||Pair->Num()!=2||!(*Pair)[0]->TryGetNumber(T)||!(*Pair)[1]->TryGetNumber(Y)||!FMath::IsFinite(T)||!FMath::IsFinite(Y)||T<0||T>TimelineLength||float(T)<=Previous||FMath::Abs(Y)>1e12)return Fail(TEXT("Finite strictly increasing times within sequence and finite values required"));Keys.Emplace(float(T),float(Y));Previous=float(T);}
   int64 Total=Keys.Num();for(const auto& Existing:S->GetDataModel()->GetFloatCurves())if(Existing.GetName()!=N)Total+=Existing.FloatCurve.GetNumKeys();
   if(Total>65536)return Fail(TEXT("Post-edit total curve key budget65536 exceeded"));
   FFloatCurve Planned=*C;Planned.FloatCurve.SetKeys(Keys);
   // UE5.8's concrete AnimSequencer controller round-trips float keys through
   // MovieScene channels. AnimSequencerController::SetCurveControlKeys selects
   // ModelRate or240000fps; AnimSequencerHelpers converts linear leave tangents
   // to channel slope and back. These are generated values, not caller tangents.
   // Match only that confirmed model, without accepting arbitrary curve changes.
   if(S->GetDataModelInterface().GetObject()->GetClass()->GetPathName()==TEXT("/Script/AnimationData.AnimationSequencerDataModel"))
   {
    const FFrameRate ModelRate=S->GetDataModel()->GetFrameRate();
    const bool OffFrame=Keys.ContainsByPredicate([&](const FRichCurveKey& K){const float F=ModelRate.AsFrameTime(K.Time).GetSubFrame();return !FMath::IsNearlyZero(F,KINDA_SMALL_NUMBER)&&!FMath::IsNearlyEqual(F,1.f,KINDA_SMALL_NUMBER);});
    const FFrameRate Rate=OffFrame?FFrameRate(240000,1):ModelRate;
    TArray<FFrameNumber> Frames;
    for(const auto& K:Keys){const double FrameValue=double(K.Time)*Rate.AsDecimal();if(!FMath::IsFinite(FrameValue)||FrameValue>=MAX_int32-1)return Fail(TEXT("Curve time exceeds concrete controller frame-number range"));const FFrameNumber F=Rate.AsFrameTime(K.Time).RoundToFrame();if(Frames.Num()&&F<=Frames.Last())return Fail(TEXT("Curve key times collide after engine channel frame quantization"));Frames.Add(F);}
    for(int32 K=0;K<Keys.Num();++K)
    {
     auto& V=Planned.FloatCurve.GetKey(Planned.FloatCurve.FindKey(Keys[K].Time,0.f));V.Time=float(Rate.AsSeconds(Frames[K]));
     if(K+1<Keys.Num())
     {
      const int32 L=K>0?K-1:K,Rhs=K+1;
      const double DeltaSeconds=Rate.AsSeconds(Frames[Rhs])-Rate.AsSeconds(Frames[L]);
      const double Ratio=(Frames[Rhs].Value-Frames[L].Value)/DeltaSeconds;
      const float ChannelTangent=float((Keys[K+1].Value-Keys[K].Value)/FMath::Max<double>(KINDA_SMALL_NUMBER,Frames[K+1].Value-Frames[K].Value));
      V.LeaveTangent=float(ChannelTangent*Ratio);
     }
    }
    R->SetBoolField(TEXT("curve_controller_frame_quantization"),true);
   }
   Expected.Add(TEXT("curve.")+N.ToString(),CurveText(Planned));
  }
  else if(Action==TEXT("remove_float_curve"))Expected.Remove(TEXT("curve.")+N.ToString());
  else
  {if(S->GetDataModel()->GetFloatCurves().Num()>=1024)return Fail(TEXT("Post-edit curve count1024 exceeded"));FFloatCurve Planned(N,AACF_DefaultCurve);Expected.Add(TEXT("curve.")+N.ToString(),CurveText(Planned));}
 }
 else if(TrackEdit)
 {
  if(!Name(P,TEXT("track_name"),N))return Fail(TEXT("Explicit track_name required"));TrackIndex=Track(A,N);
  if(Action==TEXT("add_notify_track")){if(TrackIndex!=INDEX_NONE||A->AnimNotifyTracks.Num()>=256)return Fail(TEXT("Track exists or post-edit track count256 exceeded"));}
  else
  {
   if(TrackIndex==INDEX_NONE||A->AnimNotifyTracks.Num()<=1)return Fail(TEXT("Track missing or last track removal refused"));
   if(A->Notifies.ContainsByPredicate([&](const auto& V){return V.TrackIndex==TrackIndex;})||(S&&S->AuthoredSyncMarkers.ContainsByPredicate([&](const auto& V){return V.TrackIndex==TrackIndex;})))return Fail(TEXT("Remove track contents explicitly before track removal; no implicit cascade"));
  }
 }
 else if(AddNotify)
 {
  FString ClassPath;if(!Name(P,TEXT("track_name"),N)||(TrackIndex=Track(A,N))==INDEX_NONE||!Numeric(P,TEXT("time"),Time,0,TimelineLength)||!P->TryGetStringField(TEXT("notify_class"),ClassPath)||!SafeObjectPath(ClassPath)||A->Notifies.Num()>=4096)return Fail(TEXT("Existing track, time and absolute notify_class with each name segment shorter than NAME_SIZE required; max4096 notifies"));
  if(Action==TEXT("add_notify_state")&&(!Numeric(P,TEXT("duration"),Duration,SMALL_NUMBER,TimelineLength)||Time+Duration>TimelineLength))return Fail(TEXT("Positive state duration must end within animation"));
  // Classes are admitted before Modify; their constructors are treated as callback boundaries below.
  NotifyClass.Reset(FindObject<UClass>(nullptr,*ClassPath));
  if(!IsValid(NotifyClass.Get())||NotifyClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||!NotifyClass->IsChildOf(Action==TEXT("add_notify")?UAnimNotify::StaticClass():UAnimNotifyState::StaticClass()))return Fail(TEXT("Already loaded concrete Notify/NotifyState class required"));
  NotifyCDO.Reset(NotifyClass->GetDefaultObject());if(!Check(Before))return Fail(Error);
  if(!Live(NotifyClass.Get())||NotifyClass->HasAnyClassFlags(CLASS_NewerVersionExists)||!CdoLease.Init(NotifyCDO.Get())||!NotifyCDO.IsValid()||NotifyCDO->GetClass()!=NotifyClass.Get()||NotifyClass->GetDefaultObject(false)!=NotifyCDO.Get())return Fail(TEXT("Live current Notify class default object required"));
  NotifyClassPath=Path(NotifyClass.Get());CdoPath=Path(NotifyCDO.Get());Properties(NotifyCDO.Get(),TEXT("notify_cdo"),CdoBefore);CdoFrozen=true;
  if(!Check(Before))return Fail(Error);
  R->SetStringField(TEXT("stage"),TEXT("placement_admission"));
  const bool Place=Action==TEXT("add_notify")?CastChecked<UAnimNotify>(NotifyCDO.Get())->CanBePlaced(A):CastChecked<UAnimNotifyState>(NotifyCDO.Get())->CanBePlaced(A);
  if(!Check(Before))return Fail(Error);if(!Place)return Fail(TEXT("Notify class cannot be placed on this animation asset"));
 }
 else if(NotifyEdit)
 {
  FString GuidText;FGuid Id;if(!P->TryGetStringField(TEXT("notify_guid"),GuidText)||!FGuid::Parse(GuidText,Id))return Fail(TEXT("Exact notify_guid required"));
  Index=A->Notifies.IndexOfByPredicate([&](const auto& V){return V.Guid==Id;});if(Index==INDEX_NONE)return Fail(TEXT("Notify GUID absent"));
  if(Action==TEXT("set_notify_timing")&&(!Numeric(P,TEXT("time"),Time,0,TimelineLength)||!Numeric(P,TEXT("duration"),Duration,0,TimelineLength)||Time+Duration>TimelineLength||(A->Notifies[Index].NotifyStateClass?Duration<=0:Duration!=0)))return Fail(TEXT("Valid time/duration required; plain Notify duration=0 and state duration>0"));
  if(Action==TEXT("set_notify_timing")&&M)
  {
   const auto& V=A->Notifies[Index];
   auto CanRelink=[&](const FAnimLinkableElement& Link,float At){return M->SlotAnimTracks.IsValidIndex(Link.GetSlotIndex())&&(Link.GetLinkMethod()==EAnimLinkMethod::Absolute||M->SlotAnimTracks[Link.GetSlotIndex()].AnimTrack.GetSegmentIndexAtTime(At)!=INDEX_NONE);};
   if(!CanRelink(V,Time)||(V.NotifyStateClass&&!CanRelink(V.EndLink,Time+Duration)))return Fail(TEXT("Notify timing must retain existing valid start/end slots and relative/proportional segment links"));
  }
 }
 else if(Marker)
 {
  if(!S||!Name(P,TEXT("marker_name"),N))return Fail(TEXT("Sequence and marker_name required"));
  if(Action==TEXT("add_sync_marker"))
  {if(!Name(P,TEXT("track_name"),Other)||(TrackIndex=Track(A,Other))==INDEX_NONE||!Numeric(P,TEXT("time"),Time,0,TimelineLength)||S->AuthoredSyncMarkers.Num()>=4096)return Fail(TEXT("Existing track, valid time and post-edit marker4096 budget required"));}
  else
  {if(!S->AuthoredSyncMarkers.ContainsByPredicate([&](const auto& V){return V.MarkerName==N;}))return Fail(TEXT("Marker name absent"));if(Action==TEXT("rename_sync_marker")&&(!Name(P,TEXT("new_name"),Other)||Other==N||S->AuthoredSyncMarkers.ContainsByPredicate([&](const auto& V){return V.MarkerName==Other;})))return Fail(TEXT("Distinct unused new marker name required"));}
 }
 else if(Section)
 {
  if(!M||!Name(P,TEXT("section_name"),N))return Fail(TEXT("Montage and section_name required"));Index=M->GetSectionIndex(N);
  if(Action==TEXT("add_montage_section"))
  {if(Index!=INDEX_NONE||M->CompositeSections.Num()>=1024||!Numeric(P,TEXT("time"),Time,0,TimelineLength))return Fail(TEXT("Unused section name, time and section1024 budget required"));}
  else if(Index==INDEX_NONE)return Fail(TEXT("Section absent"));
  if(Action==TEXT("set_montage_section_time")&&!Numeric(P,TEXT("time"),Time,0,TimelineLength))return Fail(TEXT("Section time outside montage"));
  if(Action==TEXT("set_montage_section_time")&&M->CompositeSections[Index].GetTime()==0&&Time!=0)return Fail(TEXT("Keep the section at time0; moving the starting section is refused"));
  if(Action==TEXT("remove_montage_section")&&(M->CompositeSections.Num()<=1||M->CompositeSections[Index].GetTime()==0))return Fail(TEXT("Keep the last section and the section at time0"));
  if(Action==TEXT("set_montage_section_next")&&(!Name(P,TEXT("next_section"),Other,true)||(Other!=NAME_None&&M->GetSectionIndex(Other)==INDEX_NONE)))return Fail(TEXT("Existing next section or empty string to terminate required"));
 }
 else if(Action==TEXT("add_montage_slot"))
 {if(!M||!Name(P,TEXT("slot_name"),N)||!Skeleton->ContainsSlotName(N)||M->SlotAnimTracks.Num()>=64||M->SlotAnimTracks.ContainsByPredicate([&](const auto& T){return T.SlotName==N;}))return Fail(TEXT("Existing skeleton slot, unused montage track and max64 slots required"));if(M->SlotAnimTracks.Num()&&Skeleton->GetSlotGroupName(M->SlotAnimTracks[0].SlotName)!=Skeleton->GetSlotGroupName(N))return Fail(TEXT("Montage slots must belong to the same skeleton group"));}
 else return Fail(TEXT("Unknown fixed animation edit action"));
 if(M)
 {
  TArray<TSharedPtr<FJsonValue>> UnsafeLinks;
  for(int32 I=0;I<A->Notifies.Num();++I)
  {
   // Only the explicitly selected, admitted deletion is absent from the saved result.
   if(Action==TEXT("remove_notify")&&I==Index)continue;
   const auto& V=A->Notifies[I];
   if(V.GetSlotIndex()==0&&(!V.NotifyStateClass||V.EndLink.GetSlotIndex()==0))continue;
   auto Link=MakeShared<J>();Link->SetStringField(TEXT("notify_guid"),V.Guid.ToString());Link->SetNumberField(TEXT("start_slot"),V.GetSlotIndex());Link->SetNumberField(TEXT("end_slot"),V.EndLink.GetSlotIndex());
   UnsafeLinks.Add(MakeShared<FJsonValueObject>(Link));
  }
  if(!UnsafeLinks.IsEmpty())
  {
   R->SetArrayField(TEXT("nondefault_montage_notify_links"),UnsafeLinks);
   R->SetBoolField(TEXT("cold_reload_link_preservation_supported"),false);
   R->SetStringField(TEXT("persistence_warning"),TEXT("Installed Montage PostLoad re-links Notify start/end using slot0. Current edit preserves live links; manual saving/reloading can change them. No engine patch or plugin-dependent load repair is installed."));
   if(Save){R->SetStringField(TEXT("error_code"),TEXT("montage_nondefault_notify_link_save_refused"));return Fail(TEXT("Save refused before Modify: surviving nondefault Notify links cannot be preserved across stock Montage reload. Use save=false for an in-memory edit; original package is not saved."));}
  }
 }
 int64 PlannedBytes=0;for(const auto& Pair:Expected)PlannedBytes+=(Pair.Key.Len()+Pair.Value.Len())*sizeof(TCHAR);
 if(PlannedBytes>16*1024*1024)return Fail(TEXT("Planned authored curve snapshot exceeds16MiB before Modify"));
 if(!Check(Before))return Fail(Error);
 R->SetStringField(TEXT("scope"),TEXT("Target animation asset only; skeleton and bone tracks preserved. No atomic rollback, graph compiler or automatic retry. Compression/cache work may be queued by the engine; not runtime readiness proof."));
 if(!Execute){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("preflight_complete"));return Publish();}
 R->SetStringField(TEXT("stage"),TEXT("modify"));R->SetBoolField(TEXT("mutation_attempted"),true);R->SetBoolField(TEXT("change_state_known"),false);
 A->Modify(false);if(!Check(Before))return Fail(Error);
 bool Applied=true;
 R->SetStringField(TEXT("stage"),TEXT("engine_write"));
 if(Curve)
 {
  auto& Controller=S->GetController();const FAnimationCurveIdentifier Id(N,ERawCurveTrackTypes::RCT_Float);
  if(Action==TEXT("add_float_curve"))Applied=Controller.AddCurve(Id,AACF_DefaultCurve);
  else if(Action==TEXT("remove_float_curve"))Applied=Controller.RemoveCurve(Id);
  else Applied=Controller.SetCurveKeys(Id,Keys);
  if(!Applied)return Fail(TEXT("Animation data controller rejected write; inspect actual target"));
  if(!Check(Expected))return Fail(Error);
 }
 else
 {
  if(TrackEdit)
  {
   if(Action==TEXT("add_notify_track")){FAnimNotifyTrack T;T.TrackName=N;T.TrackColor=FLinearColor::White;A->AnimNotifyTracks.Add(T);}
   else {A->AnimNotifyTracks.RemoveAt(TrackIndex);for(auto& V:A->Notifies)if(V.TrackIndex>TrackIndex)--V.TrackIndex;if(S)for(auto& V:S->AuthoredSyncMarkers)if(V.TrackIndex>TrackIndex)--V.TrackIndex;}
  }
  else if(AddNotify)
  {
   TStrongObjectPtr<UObject> NewNotify(NewObject<UObject>(A,NotifyClass.Get(),NAME_None,RF_Transactional));
   if(!Live(NewNotify.Get())||NewNotify->GetOuter()!=A||NewNotify->GetClass()!=NotifyClass.Get()||!Check(Before))return Fail(Error.IsEmpty()?TEXT("Notify construction changed target or ownership"):Error);
   R->SetStringField(TEXT("stage"),TEXT("notify_name_callback"));
   const FString NotifyName=Action==TEXT("add_notify")?CastChecked<UAnimNotify>(NewNotify.Get())->GetNotifyName():CastChecked<UAnimNotifyState>(NewNotify.Get())->GetNotifyName();
   if(!Check(Before))return Fail(Error);
   if(!Live(NewNotify.Get())||NewNotify->GetOuter()!=A||NewNotify->GetClass()!=NotifyClass.Get()||!Live(NotifyCDO.Get())||!Live(NotifyClass.Get())||NotifyClass->HasAnyClassFlags(CLASS_NewerVersionExists))return Fail(TEXT("Notify/name callback changed object or class lifetime; inspect actual target"));
   if(NotifyName.Len()>=NAME_SIZE)return Fail(TEXT("Notify display name exceeds FName bound; inspect target"));
   Snapshot Defaults,Actual;Properties(NotifyCDO.Get(),TEXT("newNotify"),Defaults);Properties(NewNotify.Get(),TEXT("newNotify"),Actual);
   if(!Equal(Defaults,Actual,R))return Fail(TEXT("New Notify authored defaults differ from class defaults; inspect target"));
   R->SetStringField(TEXT("stage"),TEXT("notify_placement_callback"));
   const bool Place=Action==TEXT("add_notify")?CastChecked<UAnimNotify>(NewNotify.Get())->CanBePlaced(A):CastChecked<UAnimNotifyState>(NewNotify.Get())->CanBePlaced(A);
   if(!Check(Before))return Fail(Error);
   if(!Live(NewNotify.Get())||NewNotify->GetOuter()!=A||NewNotify->GetClass()!=NotifyClass.Get())return Fail(TEXT("Notify placement callback changed object lifetime or ownership"));
   Actual.Reset();Properties(NewNotify.Get(),TEXT("newNotify"),Actual);if(!Equal(Defaults,Actual,R))return Fail(TEXT("Notify placement callback changed authored defaults"));
   if(!Place)return Fail(TEXT("Constructed Notify cannot be placed on this animation asset"));
   FAnimNotifyEvent V;V.Guid=FGuid::NewGuid();V.TrackIndex=TrackIndex;V.NotifyName=FName(*NotifyName);V.Link(A,Time);V.TriggerTimeOffset=GetTriggerTimeOffsetForType(A->CalculateOffsetForNotify(Time));
   V.Notify=Cast<UAnimNotify>(NewNotify.Get());V.NotifyStateClass=Cast<UAnimNotifyState>(NewNotify.Get());if(V.NotifyStateClass){V.SetDuration(Duration);V.EndLink.Link(A,Time+Duration);}
   R->SetStringField(TEXT("notify_guid"),V.Guid.ToString());if(!HoldNotify(V,NotifyLeases))return Fail(TEXT("New Notify identity could not be pinned"));A->Notifies.Add(V);
  }
  else if(NotifyEdit)
  {
   if(Action==TEXT("remove_notify")){NotifyLeases.Remove(A->Notifies[Index].Guid);A->Notifies.RemoveAt(Index);}
   else {auto& V=A->Notifies[Index];const int32 StartSlot=V.GetSlotIndex(),EndSlot=V.EndLink.GetSlotIndex();const auto StartMethod=V.GetLinkMethod(),EndMethod=V.EndLink.GetLinkMethod();V.Link(A,Time,StartSlot);V.TriggerTimeOffset=GetTriggerTimeOffsetForType(A->CalculateOffsetForNotify(Time));V.SetDuration(Duration);if(V.NotifyStateClass)V.EndLink.Link(A,Time+Duration,EndSlot);
    if(M&&(V.GetSlotIndex()!=StartSlot||V.GetLinkMethod()!=StartMethod||(V.NotifyStateClass&&(V.EndLink.GetSlotIndex()!=EndSlot||V.EndLink.GetLinkMethod()!=EndMethod))))return Fail(TEXT("Notify timing changed the existing slot/link method; inspect target"));}
  }
  else if(Marker)
  {
   if(Action==TEXT("add_sync_marker")){FAnimSyncMarker V;V.MarkerName=N;V.Time=Time;V.TrackIndex=TrackIndex;V.Guid=FGuid::NewGuid();S->AuthoredSyncMarkers.Add(V);R->SetStringField(TEXT("marker_guid"),V.Guid.ToString());}
   else if(Action==TEXT("remove_sync_markers"))R->SetNumberField(TEXT("removed_count"),S->AuthoredSyncMarkers.RemoveAll([&](const auto& V){return V.MarkerName==N;}));
   else for(auto& V:S->AuthoredSyncMarkers)if(V.MarkerName==N)V.MarkerName=Other;
  }
  else if(Section)
  {
   // The engine's AddAnimCompositeSection also autowires a previous section.
   // This explicit contract adds only the requested section and preserves existing links.
   if(Action==TEXT("add_montage_section")){FCompositeSection V;V.SectionName=N;V.Link(M,Time);M->CompositeSections.Add(V);}
   else if(Action==TEXT("remove_montage_section")){M->CompositeSections.RemoveAt(Index);for(auto& V:M->CompositeSections)if(V.NextSectionName==N)V.NextSectionName=NAME_None;}
   else if(Action==TEXT("set_montage_section_next"))M->CompositeSections[Index].NextSectionName=Other;
   else M->CompositeSections[Index].Link(M,Time);
   M->CompositeSections.Sort([](const FCompositeSection& L,const FCompositeSection& Rhs){return L.GetTime()<Rhs.GetTime();});
  }
  else if(Action==TEXT("add_montage_slot")){FSlotAnimationTrack T;T.SlotName=N;M->SlotAnimTracks.Add(T);}
  // Pure authored edits above do not dispatch user callbacks. Establish expected
  // values before engine refresh/PostEditChange/dirty/save boundaries.
   if(!CheckNotifyLeases(A,NotifyLeases,R))return Fail(TEXT("Notify identity changed during authored edits"));
   if(!Capture(A,Expected,Error))return Fail(Error);
 }
 R->SetStringField(TEXT("stage"),TEXT("refresh"));
 A->RefreshCacheData();if(!Check(Expected))return Fail(Error);
 if(S&&Marker){S->RefreshSyncMarkerDataFromAuthored();if(!Check(Expected))return Fail(Error);}
 R->SetStringField(TEXT("stage"),TEXT("post_edit"));A->PostEditChange();if(!Check(Expected))return Fail(Error);
 R->SetStringField(TEXT("stage"),TEXT("mark_dirty"));A->MarkPackageDirty();if(!Check(Expected))return Fail(Error);
 R->SetBoolField(TEXT("changed"),!Equal(Before,Expected,MakeShared<J>()));R->SetBoolField(TEXT("change_state_known"),true);
 if(Save)
 {
  R->SetStringField(TEXT("stage"),TEXT("save"));R->SetBoolField(TEXT("save_attempted"),true);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;
  const bool Saved=UPackage::SavePackage(Package.Get(),A,*Filename,Args);R->SetBoolField(TEXT("saved"),Saved);
  if(!Check(Expected)){R->SetBoolField(TEXT("change_state_known"),false);return Fail(Error);}
  if(!Saved)return Fail(TEXT("Single target package save failed; edits remain in memory"));
 }
 R->SetBoolField(TEXT("dirty_after"),Package->IsDirty());R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));R->SetStringField(TEXT("retry_risk"),TEXT("mutation_attempted"));return Publish();
}
}
