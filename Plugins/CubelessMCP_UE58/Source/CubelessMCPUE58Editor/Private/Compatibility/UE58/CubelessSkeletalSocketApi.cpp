// CK: original skeleton-owned socket lifecycle, native forced save, no compiler.
#include "Compatibility/UE58/CubelessSkeletalSocketApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/Skeleton.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "ReferenceSkeleton.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
namespace Cubeless::UE58::SkeletalSocket
{
namespace
{
using Variable::FResult;
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool Text(const FString& S, int32 Max)
{ if (S.Len()>Max) { return false; } for (int32 I=0;I<S.Len();++I) { if (S[I]==TCHAR('\0')) { return false; } } return true; }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error)
{ R.bSuccess=false;R.ErrorCode=Code;R.Error=Error;return false; }
const TCHAR* Mode(EBoneTranslationRetargetingMode::Type M)
{
 switch(M) { case EBoneTranslationRetargetingMode::Animation:return TEXT("Animation");case EBoneTranslationRetargetingMode::Skeleton:return TEXT("Skeleton");
 case EBoneTranslationRetargetingMode::AnimationScaled:return TEXT("AnimationScaled");case EBoneTranslationRetargetingMode::AnimationRelative:return TEXT("AnimationRelative");
 case EBoneTranslationRetargetingMode::OrientAndScale:return TEXT("OrientAndScale");default:return TEXT("Unknown"); }
}
bool Disk(const UObject* O,const TSharedRef<FJsonObject>& J)
{
 FString F;if(!FPackageName::TryConvertLongPackageNameToFilename(O->GetOutermost()->GetName(),F,FPackageName::GetAssetPackageExtension())) { return false; }
 const int64 N=IFileManager::Get().FileSize(*F);J->SetStringField(TEXT("path"),F);J->SetBoolField(TEXT("exists"),N>=0);J->SetNumberField(TEXT("bytes"),N<0?0:N);
 if(N<0) { J->SetStringField(TEXT("sha1"),TEXT(""));return true; } if(N>67108864) { return false; }
 TArray<uint8>B;if(!FFileHelper::LoadFileToArray(B,*F) || B.Num()!=N) { return false; }FSHAHash H;FSHA1::HashBuffer(B.GetData(),B.Num(),H.Hash);J->SetStringField(TEXT("sha1"),H.ToString());return true;
}
bool Numbers(const TSharedRef<FJsonObject>& J,const TCHAR* K,std::initializer_list<double> Values)
{
 TArray<TSharedPtr<FJsonValue>> A;for(double V:Values) { if(!FMath::IsFinite(V)) { return false; } A.Add(MakeShared<FJsonValueNumber>(V)); }J->SetArrayField(K,A);return true;
}
bool Capture(UObject* Asset,USkeleton* S,USkeletalMesh* Mesh,FResult& R,const TCHAR* Phase,TSharedPtr<FJsonObject>& Out,bool Attach=true)
{
 R.Data->SetNumberField(TEXT("observation_capture_calls"),R.Data->GetNumberField(TEXT("observation_capture_calls"))+1);
 auto J=MakeShared<FJsonObject>();Out=J;if(Attach){R.Data->SetObjectField(Phase,J);}J->SetBoolField(TEXT("complete"),false);
 if(!Loaded(Asset)||!Loaded(S)) { return false; }
 J->SetStringField(TEXT("asset_path"),Asset->GetPathName());J->SetStringField(TEXT("asset_class"),Asset->GetClass()->GetPathName());J->SetStringField(TEXT("skeleton"),S->GetPathName());
 if(Mesh) { if(Mesh->IsCompiling()) { return false; } R.Data->SetNumberField(TEXT("skeleton_getter_calls"),R.Data->GetNumberField(TEXT("skeleton_getter_calls"))+1);if(Mesh->GetSkeleton()!=S) { return false; } }
 const auto& Ref=S->GetReferenceSkeleton();const int32 N=Ref.GetNum();if(N>16384 || S->Sockets.Num()>4096 || Ref.GetRefBonePose().Num()!=N) { return false; }
 TArray<TSharedPtr<FJsonValue>> Bones,Poses,Sockets;
 J->SetArrayField(TEXT("bones"),Bones);J->SetArrayField(TEXT("reference_pose"),Poses);J->SetArrayField(TEXT("skeleton_sockets"),Sockets);
 for(int32 I=0;I<N;++I)
 {
  const int32 Parent=Ref.GetRefBoneInfo()[I].ParentIndex;if(Parent<INDEX_NONE || Parent>=I) { return false; }
  auto B=MakeShared<FJsonObject>();B->SetNumberField(TEXT("index"),I);B->SetStringField(TEXT("name"),Ref.GetBoneName(I).ToString());B->SetNumberField(TEXT("parent_index"),Parent);
  B->SetStringField(TEXT("parent_name"),Parent>=0?Ref.GetBoneName(Parent).ToString():FString());B->SetStringField(TEXT("translation_retargeting"),Mode(S->GetBoneTranslationRetargetingMode(I)));Bones.Add(MakeShared<FJsonValueObject>(B));J->SetArrayField(TEXT("bones"),Bones);
  const auto& T=Ref.GetRefBonePose()[I];const auto P=T.GetTranslation(),Scale=T.GetScale3D();const auto Q=T.GetRotation();auto Pose=MakeShared<FJsonObject>();
  if(!Numbers(Pose,TEXT("translation"),{P.X,P.Y,P.Z})||!Numbers(Pose,TEXT("scale"),{Scale.X,Scale.Y,Scale.Z})||!Numbers(Pose,TEXT("rotation_quaternion"),{Q.X,Q.Y,Q.Z,Q.W})) { return false; }
  Poses.Add(MakeShared<FJsonValueObject>(Pose));J->SetArrayField(TEXT("reference_pose"),Poses);
 }
 for(int32 I=0;I<S->Sockets.Num();++I)
 {
  const auto* Socket=S->Sockets[I].Get();auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("array_index"),I);Row->SetBoolField(TEXT("null"),Socket==nullptr);Sockets.Add(MakeShared<FJsonValueObject>(Row));J->SetArrayField(TEXT("skeleton_sockets"),Sockets);
  if(!Socket) { continue; }if(!Loaded(Socket)||Socket->GetClass()!=USkeletalMeshSocket::StaticClass()) { return false; }
  Row->SetStringField(TEXT("object_path"),Socket->GetPathName());Row->SetStringField(TEXT("owner_path"),Socket->GetOuter()?Socket->GetOuter()->GetPathName():FString());
  Row->SetStringField(TEXT("socket_name"),Socket->SocketName.ToString());Row->SetStringField(TEXT("bone_name"),Socket->BoneName.ToString());Row->SetBoolField(TEXT("force_always_animated"),Socket->bForceAlwaysAnimated);
  const auto L=Socket->RelativeLocation,Scale=Socket->RelativeScale;const auto Rot=Socket->RelativeRotation;
  if(!Numbers(Row,TEXT("location_values"),{L.X,L.Y,L.Z})||!Numbers(Row,TEXT("rotation_values_pitch_yaw_roll"),{Rot.Pitch,Rot.Yaw,Rot.Roll})||!Numbers(Row,TEXT("scale_values"),{Scale.X,Scale.Y,Scale.Z})) { return false; }
  Row->SetStringField(TEXT("location"),L.ToString());Row->SetStringField(TEXT("rotation"),Rot.ToString());Row->SetStringField(TEXT("scale"),Scale.ToString());
 }
 auto Disks=MakeShared<FJsonObject>();J->SetObjectField(TEXT("disk"),Disks);auto Input=MakeShared<FJsonObject>(),SkeletonDisk=MakeShared<FJsonObject>();
 Disks->SetObjectField(TEXT("input_asset"),Input);Disks->SetObjectField(TEXT("skeleton_asset"),SkeletonDisk);if(!Disk(Asset,Input)||!Disk(S,SkeletonDisk)) { return false; }
 auto Other=MakeShared<FJsonObject>();J->SetObjectField(TEXT("other_package_dirty"),Other);int32 Count=0;
 for(TObjectIterator<UPackage> It;It;++It) { if(!IsValid(*It)||(*It)->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)) { continue; }if(++Count>8192) { return false; }Other->SetBoolField((*It)->GetName(),(*It)->IsDirty()); }
 J->SetBoolField(TEXT("input_asset_dirty"),Asset->GetOutermost()->IsDirty());J->SetBoolField(TEXT("skeleton_dirty"),S->GetOutermost()->IsDirty());J->SetBoolField(TEXT("complete"),true);return true;
}
bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
 if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
 switch(A->Type)
 {
 case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();
 case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
 case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!Equal(X[I],Y[I])) { return false; } }return true;}
 case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& P:X->Values) { const auto* V=Y->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }return true;}
 default:return false;
 }
}

void Count(FResult& R,const TCHAR* K,int32 N=1) { R.Data->SetNumberField(K,R.Data->GetNumberField(K)+N); }
TSharedPtr<FJsonValue> CopyValue(const TSharedPtr<FJsonValue>& V)
{
 if(!V.IsValid()) { return nullptr; }
 switch(V->Type)
 {
 case EJson::Null:return MakeShared<FJsonValueNull>();case EJson::Boolean:return MakeShared<FJsonValueBoolean>(V->AsBool());
 case EJson::Number:return MakeShared<FJsonValueNumber>(V->AsNumber());case EJson::String:return MakeShared<FJsonValueString>(V->AsString());
 case EJson::Array:{TArray<TSharedPtr<FJsonValue>> A;for(const auto& E:V->AsArray()) { A.Add(CopyValue(E)); }return MakeShared<FJsonValueArray>(A);}
 case EJson::Object:{auto J=MakeShared<FJsonObject>();for(const auto& P:V->AsObject()->Values) { J->SetField(P.Key,CopyValue(P.Value)); }return MakeShared<FJsonValueObject>(J);}
 default:return nullptr;
 }
}
TSharedPtr<FJsonObject> Clone(const TSharedPtr<FJsonObject>& J) { return CopyValue(MakeShared<FJsonValueObject>(J))->AsObject(); }
bool Same(const TSharedPtr<FJsonObject>& A,const TSharedPtr<FJsonObject>& B) { return Equal(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B)); }
bool Name(const FString& V,FName& N,FResult& R,const TCHAR* Field)
{
 if(V.IsEmpty()||V.Len()>=NAME_SIZE||!Text(V,NAME_SIZE-1)) { return Fail(R,TEXT("skeletal_socket_name_invalid"),FString::Printf(TEXT("'%s' requires a bounded nonempty Unreal name."),Field)); }
 N=FName(*V);return !N.IsNone()||Fail(R,TEXT("skeletal_socket_name_invalid"),FString::Printf(TEXT("Invalid '%s' name '%s': name resolves to None"),Field,*V));
}
bool Dependency(const FString& V)
{
 FString S=V.TrimStartAndEnd();S.ReplaceInline(TEXT("\""),TEXT(""));S.ReplaceInline(TEXT("'"),TEXT(""));S.ReplaceInline(TEXT("\\"),TEXT("/"));S=S.ToLower();
 for(const TCHAR* T:{TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("cubelesscontrolrigtools")}) { if(S.Contains(T)) { return true; } }return false;
}
USkeletalMeshSocket* Find(USkeleton* S,FName N) { for(const auto& P:S->Sockets) { if(P&&P->SocketName==N) { return P.Get(); } }return nullptr; }
bool Vector(const FString& Json,const FVector& Default,FVector& Out,FResult& R)
{
 Out=Default;TSharedPtr<FJsonObject> Envelope;
 if(Json.Len()>4096||!Text(Json,4096)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("{\"value\":")+Json+TEXT("}")),Envelope)||!Envelope.IsValid()||Envelope->Values.Num()!=1)
 { return Fail(R,TEXT("skeletal_socket_transform_invalid"),TEXT("Bounded JSON transform required.")); }
 TSharedPtr<FJsonValue> V=Envelope->TryGetField(TEXT("value"));
 if(!V.IsValid()) { return Fail(R,TEXT("skeletal_socket_transform_invalid"),TEXT("Bounded JSON transform required.")); }
 if(V->Type==EJson::Null) { return true; }
 if(V->Type!=EJson::Array||V->AsArray().Num()!=3) { return Fail(R,TEXT("skeletal_socket_transform_invalid"),TEXT("location/rotation/scale must be arrays of three finite numbers when present")); }
 double A[3];for(int32 I=0;I<3;++I) { if(!V->AsArray()[I].IsValid()||V->AsArray()[I]->Type!=EJson::Number||!V->AsArray()[I]->TryGetNumber(A[I])||!FMath::IsFinite(A[I]))
 { return Fail(R,TEXT("skeletal_socket_transform_invalid"),TEXT("location/rotation/scale must be arrays of three finite numbers when present")); } }
 Out=FVector(A[0],A[1],A[2]);return true;
}
bool Resolve(const FString& Path,FResult& R,UObject*& Asset,USkeleton*& S,USkeletalMesh*& Mesh)
{
 R.Stage=TEXT("load_asset");Count(R,TEXT("asset_load_count"));auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("requested_path"),Path);L->SetBoolField(TEXT("attempted"),true);L->SetBoolField(TEXT("returned"),false);R.Data->SetArrayField(TEXT("asset_load_calls"),{MakeShared<FJsonValueObject>(L)});
 Asset=UEditorAssetLibrary::LoadAsset(Path);L->SetBoolField(TEXT("returned"),true);L->SetStringField(TEXT("asset_path"),Asset?Asset->GetPathName():FString());
 S=Cast<USkeleton>(Asset);Mesh=Cast<USkeletalMesh>(Asset);
 if(Mesh) { if(!Loaded(Mesh)||Mesh->GetClass()!=USkeletalMesh::StaticClass()||Mesh->IsCompiling()) { return Fail(R,TEXT("skeletal_mesh_scope_unsupported"),TEXT("Loaded idle native mesh required; no explicit async wait.")); }Count(R,TEXT("skeleton_getter_calls"));S=Mesh->GetSkeleton(); }
 if(!Loaded(S)||S->GetClass()!=USkeleton::StaticClass()||!S->IsAsset()) { return Fail(R,TEXT("skeletal_skeleton_scope_unsupported"),TEXT("Exact native Skeleton or native mesh with Skeleton required.")); }
 // Read group admits Engine assets; writes are limited to copied project Skeletons.
 if(!S->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))) { return Fail(R,TEXT("skeletal_socket_write_root_unsupported"),TEXT("Socket writes require an isolated copied /Game/ Skeleton; Engine/plugin sources remain read-only.")); }
 R.Data->SetStringField(TEXT("skeleton"),S->GetPathName());R.Data->SetStringField(TEXT("resolved_asset_path"),Asset->GetPathName());R.Data->SetBoolField(TEXT("input_was_skeletal_mesh"),Mesh!=nullptr);return true;
}
bool DirtyFence(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,const FString& Package,bool PermitCleanNew)
{
 const auto A=Before->GetObjectField(TEXT("other_package_dirty")),B=After->GetObjectField(TEXT("other_package_dirty"));
 for(const auto& P:A->Values) { if(FString(P.Key.ToView())==Package) { continue; }const auto* V=B->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }
 for(const auto& P:B->Values) { if(!A->HasField(P.Key)&&(!PermitCleanNew||P.Value->Type!=EJson::Boolean||P.Value->AsBool())) { return false; } }return true;
}
TSharedPtr<FJsonObject> StateProjection(const TSharedPtr<FJsonObject>& J)
{
 auto P=Clone(J);for(const TCHAR* K:{TEXT("disk"),TEXT("other_package_dirty"),TEXT("input_asset_dirty"),TEXT("skeleton_dirty")}) { P->RemoveField(K); }return P;
}
TSharedPtr<FJsonObject> SocketRow(const TSharedPtr<FJsonObject>& J,const USkeletalMeshSocket* Socket)
{
 for(const auto& V:J->GetArrayField(TEXT("skeleton_sockets"))) { if(!V->AsObject()->GetBoolField(TEXT("null"))&&V->AsObject()->GetStringField(TEXT("object_path"))==Socket->GetPathName()) { return V->AsObject(); } }return nullptr;
}
bool Selected(const TSharedPtr<FJsonObject>& J,const USkeletalMeshSocket* Socket,USkeleton* S,FName SocketName,FName Bone,const FVector& L,const FRotator& Rot,const FVector& Scale,bool Force)
{
 auto Row=SocketRow(J,Socket);if(!Row.IsValid()) { return false; }
 auto Expected=Clone(Row);Expected->SetStringField(TEXT("socket_name"),SocketName.ToString());Expected->SetStringField(TEXT("bone_name"),Bone.ToString());Expected->SetStringField(TEXT("owner_path"),S->GetPathName());Expected->SetBoolField(TEXT("force_always_animated"),Force);
 Numbers(Expected.ToSharedRef(),TEXT("location_values"),{L.X,L.Y,L.Z});Numbers(Expected.ToSharedRef(),TEXT("rotation_values_pitch_yaw_roll"),{Rot.Pitch,Rot.Yaw,Rot.Roll});Numbers(Expected.ToSharedRef(),TEXT("scale_values"),{Scale.X,Scale.Y,Scale.Z});
 Expected->SetStringField(TEXT("location"),L.ToString());Expected->SetStringField(TEXT("rotation"),Rot.ToString());Expected->SetStringField(TEXT("scale"),Scale.ToString());return Same(Expected,Row);
}
// IS: concrete first-match scope for the previously refused duplicate-name Set/Remove only.
// The existing single-match and Add algorithms below retain their original observer/output paths.
bool DuplicateSocketWriteActive=false;
struct FDuplicateSocketScope
{
 FResult& R;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
 TStrongObjectPtr<UObject> Asset;TStrongObjectPtr<USkeleton> Skeleton;TStrongObjectPtr<USkeletalMesh> Mesh;TStrongObjectPtr<USkeletalMeshSocket> SelectedSocket;
 struct FIdentity {TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;};
 TArray<FIdentity> Identities;TArray<TObjectPtr<USkeletalMeshSocket>> ExpectedSockets;TSharedPtr<FJsonObject> Before,Expected;
 FString SelectedPath,PackagePath;FName RequestedName;int32 FirstIndex=INDEX_NONE,SelectedOccurrences=0,NameMatches=0;bool Stopped=false;
 explicit FDuplicateSocketScope(FResult& In):R(In){}
 bool Stop(const TCHAR* Code,const FString& Error)
 {
  if(!Stopped){Stopped=true;Fail(R,Code,Error);R.bPostconditionPassed=false;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("duplicate_socket_guard_stopped"),true);R.Data->SetBoolField(TEXT("automatic_retry_safe"),false);
   if(R.Data->HasTypedField<EJson::Object>(TEXT("original_result"))){R.Data->GetObjectField(TEXT("original_result"))->SetBoolField(TEXT("success"),false);}}
  return false;
 }
 bool Context()
 {
  if(Stopped){return false;}
  if(!IsEditorGameThread()||!GEditor||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()
   ||GEditor!=Editor.Get()||!Loaded(Editor.Get())||!World.IsValid()||!Loaded(World.Get())||IsPlaySessionActive()||GEditor->GetEditorWorldContext().World()!=World.Get()||World->bIsTearingDown)
  {return Stop(TEXT("skeletal_socket_duplicate_context_changed"),TEXT("Captured idle Editor/world/GC/exit/save context changed; no next socket writer/save or retry."));}return true;
 }
 bool Pin(UObject* O)
 {
  if(!Loaded(O)||!Loaded(O->GetClass())||(O->GetOuter()&&!Loaded(O->GetOuter()))||!Loaded(O->GetPackage())){return Stop(TEXT("skeletal_socket_duplicate_identity_invalid"),TEXT("Fully loaded current object/class/owner/package required for duplicate first-match scope."));}
  for(const auto& I:Identities){if(I.Object.Get()==O){return true;}}
  if(Identities.Num()>=4104){return Stop(TEXT("skeletal_socket_duplicate_identity_bound"),TEXT("Duplicate socket identity scope exceeds the existing4096 socket bound plus root owners."));}
  FIdentity I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();Identities.Add(MoveTemp(I));return true;
 }
 bool Live()
 {
  if(!Context()){return false;}
  for(const auto& I:Identities)
  {
   auto* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)
    ||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetPathName()!=I.Path)
   {return Stop(TEXT("skeletal_socket_duplicate_identity_changed"),TEXT("Captured duplicate socket/root class, ownership, package, path or lifetime changed; no stale pointer is read or repaired."));}
  }
  if(Skeleton->Sockets!=ExpectedSockets){return Stop(TEXT("skeletal_socket_duplicate_array_changed"),TEXT("Current ordered socket pointers differ from the exact planned first-match transition."));}
  if(Mesh.IsValid()&&(Mesh->IsCompiling()||Mesh->GetSkeleton()!=Skeleton.Get())){return Stop(TEXT("skeletal_socket_duplicate_mesh_route_changed"),TEXT("Current source mesh no longer resolves the captured idle Skeleton."));}
  return true;
 }
 bool Bind(UObject* A,USkeleton* S,USkeletalMesh* M,USkeletalMeshSocket* Chosen,FName Query)
 {
  Editor=GEditor;if(!IsEditorGameThread()||!GEditor||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()){return Stop(TEXT("skeletal_socket_duplicate_context_required"),TEXT("Safe idle Editor required before capturing duplicate first-match roles."));}
  World=GEditor->GetEditorWorldContext().World();if(!Context()){return false;}
  Asset.Reset(A);Skeleton.Reset(S);Mesh.Reset(M);SelectedSocket.Reset(Chosen);RequestedName=Query;
  if(!Loaded(A)||!Loaded(S)||S->GetClass()!=USkeleton::StaticClass()||!Loaded(Chosen)||Chosen->GetClass()!=USkeletalMeshSocket::StaticClass()||Chosen->GetOuter()!=S||S->Sockets.Num()>4096)
  {return Stop(TEXT("skeletal_socket_duplicate_role_invalid"),TEXT("Exact Skeleton and first selected owned native socket roles required."));}
  ExpectedSockets=S->Sockets;if(!Pin(A)||!Pin(S)||!Pin(A->GetPackage())||!Pin(S->GetPackage())){return false;}if(M&&!Pin(M)){return false;}
  PackagePath=S->GetPackage()->GetName();SelectedPath=Chosen->GetPathName();
  for(int32 I=0;I<ExpectedSockets.Num();++I)
  {
   auto* P=ExpectedSockets[I].Get();if(!P){continue;}if(!Loaded(P)||P->GetClass()!=USkeletalMeshSocket::StaticClass()||P->GetOuter()!=S||!Pin(P)){return Stop(TEXT("skeletal_socket_duplicate_member_invalid"),TEXT("Every stored nonnull socket must have its exact native class and captured Skeleton owner."));}
   if(P->SocketName==Query){++NameMatches;if(FirstIndex==INDEX_NONE){FirstIndex=I;}}if(P==Chosen){++SelectedOccurrences;}
  }
  if(NameMatches<=1||FirstIndex==INDEX_NONE||ExpectedSockets[FirstIndex].Get()!=Chosen||SelectedOccurrences<=0){return Stop(TEXT("skeletal_socket_duplicate_first_match_changed"),TEXT("Selected pointer must be the first original FName match in the captured ordered socket array."));}
  if(!Live()){return false;}int32 SDKIndex=INDEX_NONE;Count(R,TEXT("duplicate_first_match_sdk_calls"));auto* SDKSelected=S->FindSocketAndIndex(Query,SDKIndex);
  if(!Live()){return false;}if(SDKSelected!=Chosen||SDKIndex!=FirstIndex){return Stop(TEXT("skeletal_socket_duplicate_first_match_changed"),TEXT("Public FindSocketAndIndex does not identify the exact selected first array match."));}
  R.Data->SetStringField(TEXT("duplicate_socket_selection_policy"),TEXT("Original first FName array match; object pointer/ordered membership, not name uniqueness. Remove deletes all occurrences of only that selected pointer."));
  R.Data->SetStringField(TEXT("duplicate_selected_socket_path"),SelectedPath);R.Data->SetNumberField(TEXT("duplicate_selected_first_index"),FirstIndex);R.Data->SetNumberField(TEXT("duplicate_matching_name_count_before"),NameMatches);R.Data->SetNumberField(TEXT("duplicate_selected_pointer_occurrences_before"),SelectedOccurrences);return true;
 }
 bool Observation(const TSharedPtr<FJsonObject>& Current,bool AfterSave)
 {
  if(!Live()){return false;}if(!Expected.IsValid()||!Before.IsValid()||!Current.IsValid()||!Current->GetBoolField(TEXT("complete"))){return Stop(TEXT("skeletal_socket_duplicate_observation_incomplete"),TEXT("Complete exact duplicate socket before/current projection required; no next writer."));}
  bool Preserved=Same(Expected,StateProjection(Current))&&DirtyFence(Before,Current,PackagePath,AfterSave);
  const auto OriginalDisk=Before->GetObjectField(TEXT("disk")),CurrentDisk=Current->GetObjectField(TEXT("disk"));
  if(!AfterSave){Preserved=Preserved&&Same(OriginalDisk,CurrentDisk);}
  else if(Asset.Get()!=Skeleton.Get()){Preserved=Preserved&&Equal(OriginalDisk->Values[TEXT("input_asset")],CurrentDisk->Values[TEXT("input_asset")]);}
  if(!Preserved){return Stop(TEXT("skeletal_socket_duplicate_preservation_failed"),TEXT("Exact selected transition or unselected socket/reference pose/source/disk/other-dirty preservation failed; actual prefix retained without retry/restore."));}
  return true;
 }
 bool Establish(const TSharedPtr<FJsonObject>& Initial)
 {if(!Live()||!Initial.IsValid()||!Initial->GetBoolField(TEXT("complete"))){return Stop(TEXT("skeletal_socket_duplicate_baseline_incomplete"),TEXT("Complete original socket baseline required before any duplicate writer."));}Before=Clone(Initial);Expected=StateProjection(Initial);return Check(false);}
 bool Check(bool AfterSave)
 {
  if(!Live()){return false;}Count(R,TEXT("duplicate_scope_observation_calls"));TSharedPtr<FJsonObject> Current;
  // Private full observations remain internal to proof; do not multiply emitted row/bone/disk blobs.
  if(!Capture(Asset.Get(),Skeleton.Get(),Mesh.Get(),R,TEXT("duplicate_scope_current_internal"),Current,false)){return Stop(TEXT("skeletal_socket_duplicate_observation_incomplete"),TEXT("Current full duplicate socket projection could not be observed; no further writer/save."));}
  return Observation(Current,AfterSave);
 }
 void PlanSet(FName NewSocketName,FName Bone,const FVector& Location,const FRotator& Rotation,const FVector& Scale)
 {
  for(const auto& Value:Expected->GetArrayField(TEXT("skeleton_sockets")))
  {auto Row=Value->AsObject();if(!Row->GetBoolField(TEXT("null"))&&Row->GetStringField(TEXT("object_path"))==SelectedPath){Row->SetStringField(TEXT("socket_name"),NewSocketName.ToString());Row->SetStringField(TEXT("bone_name"),Bone.ToString());Numbers(Row.ToSharedRef(),TEXT("location_values"),{Location.X,Location.Y,Location.Z});Numbers(Row.ToSharedRef(),TEXT("rotation_values_pitch_yaw_roll"),{Rotation.Pitch,Rotation.Yaw,Rotation.Roll});Numbers(Row.ToSharedRef(),TEXT("scale_values"),{Scale.X,Scale.Y,Scale.Z});Row->SetStringField(TEXT("location"),Location.ToString());Row->SetStringField(TEXT("rotation"),Rotation.ToString());Row->SetStringField(TEXT("scale"),Scale.ToString());}}
 }
 void PlanRemove()
 {
  ExpectedSockets.Remove(SelectedSocket.Get());auto Rows=Expected->GetArrayField(TEXT("skeleton_sockets"));
  for(int32 I=Rows.Num()-1;I>=0;--I){auto Row=Rows[I]->AsObject();if(!Row->GetBoolField(TEXT("null"))&&Row->GetStringField(TEXT("object_path"))==SelectedPath){Rows.RemoveAt(I);}}
  for(int32 I=0;I<Rows.Num();++I){Rows[I]->AsObject()->SetNumberField(TEXT("array_index"),I);}Expected->SetArrayField(TEXT("skeleton_sockets"),Rows);
 }
 bool Removed(const TSharedPtr<FJsonObject>& Current)
 {
  if(ExpectedSockets.Contains(SelectedSocket.Get())){return false;}int32 Remaining=0;
  for(const auto& Value:Current->GetArrayField(TEXT("skeleton_sockets"))){auto Row=Value->AsObject();if(Row->GetBoolField(TEXT("null"))){continue;}if(Row->GetStringField(TEXT("object_path"))==SelectedPath){return false;}if(Row->GetStringField(TEXT("socket_name"))==RequestedName.ToString()){++Remaining;}}
  R.Data->SetNumberField(TEXT("duplicate_matching_name_count_after_remove"),Remaining);R.Data->SetBoolField(TEXT("duplicate_unselected_same_name_objects_preserved"),true);return true;
 }
};
FResult Write(int32 Kind,const FString& Path,const FString& SocketName,const FString& BoneName,const FString& Rename,const FString& LJson,const FString& RJson,const FString& SJson,bool Save)
{
 FResult R;R.bSaveRequested=Save;auto D=R.Data;D->SetStringField(TEXT("requested_skeleton_path"),Path);D->SetStringField(TEXT("requested_socket_name"),SocketName);D->SetStringField(TEXT("requested_bone_name"),BoneName);D->SetStringField(TEXT("requested_new_socket_name"),Rename);
 D->SetStringField(TEXT("socket_scope"),TEXT("USkeleton.Sockets only, mesh input resolves Skeleton; project copy writes only."));D->SetBoolField(TEXT("implicit_PostLoad_PreSave_cache_gc_effects_exhaustively_measured"),false);D->SetBoolField(TEXT("full_reflected_asset_state_observed"),false);
 for(const TCHAR* K:{TEXT("asset_load_count"),TEXT("skeleton_getter_calls"),TEXT("observation_capture_calls"),TEXT("outer_transaction_calls"),TEXT("modify_calls"),TEXT("socket_allocation_attempts"),TEXT("socket_allocation_returns"),TEXT("socket_array_add_calls"),TEXT("socket_array_remove_calls"),TEXT("socket_array_removed_count"),TEXT("retained_socket_read_calls"),TEXT("field_write_calls"),TEXT("mark_package_dirty_calls"),TEXT("post_edit_change_calls"),TEXT("compile_calls"),TEXT("save_attempt_count")}) { D->SetNumberField(K,0); }
 D->SetArrayField(TEXT("asset_load_calls"),{});D->SetArrayField(TEXT("save_calls"),{});
 if(!IsEditorGameThread()||IsPlaySessionActive()||Path.IsEmpty()||!Text(Path,1024)) { Fail(R,TEXT("skeletal_socket_editor_or_path_invalid"),TEXT("Idle Editor and bounded literal asset path required."));return R; }
 if(DuplicateSocketWriteActive){Fail(R,TEXT("skeletal_socket_duplicate_write_busy"),TEXT("A concrete duplicate first-match operation owns the socket writer; reentry refused before transaction/load/writer."));return R;}
 Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("Cubeless","SocketLifecycle","Socket lifecycle"));
 UObject* Asset=nullptr;USkeleton* S=nullptr;USkeletalMesh* Mesh=nullptr;if(!Resolve(Path,R,Asset,S,Mesh)) { return R; }TStrongObjectPtr<UObject> KeepAsset(Asset);TStrongObjectPtr<USkeleton> KeepSkeleton(S);
 FName SN,BN,NewSN;if(!Name(SocketName,SN,R,TEXT("socket_name"))) { return R; }
 // Check current array liveness before even the original first-name scan reads a socket field.
 if(S->Sockets.Num()>4096){R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_socket_current_array_invalid"),TEXT("Current socket array exceeds the existing4096 observation bound before selection."));return R;}
 for(const auto& P:S->Sockets){if(P&&!Loaded(P.Get())){R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_socket_current_array_invalid"),TEXT("A current stored socket is not live/loaded before name selection; no writer dispatched."));return R;}}
 USkeletalMeshSocket* Socket=Find(S,SN);
 if(Kind==0) { if(Dependency(SocketName)) { Fail(R,TEXT("skeletal_socket_dependency_forbidden"),TEXT("socket_name contains an original forbidden dependency reference."));return R; }if(!Name(BoneName,BN,R,TEXT("bone_name"))) { return R; }if(Socket) { Fail(R,TEXT("skeletal_socket_exists"),FString::Printf(TEXT("Socket already exists: %s (use set_skeletal_socket to modify)"),*SocketName));return R; } }
 else { if(!Socket) { Fail(R,TEXT("skeletal_socket_not_found"),FString::Printf(TEXT("Socket not found: %s"),*SocketName));return R; }if(!Loaded(Socket)||Socket->GetClass()!=USkeletalMeshSocket::StaticClass()||Socket->GetOuter()!=S) { Fail(R,TEXT("skeletal_socket_scope_unsupported"),TEXT("Exact owned native socket required."));return R; }BN=Socket->BoneName; }
 int32 Matches=0;if(Socket){for(const auto& P:S->Sockets){if(P&&P->SocketName==SN){++Matches;}}}
 const bool DuplicateMode=Kind>0&&Matches>1;TGuardValue<bool> DuplicateLease(DuplicateSocketWriteActive,DuplicateMode);TUniquePtr<FDuplicateSocketScope> DuplicateScope;
 if(DuplicateMode){D->SetNumberField(TEXT("duplicate_first_match_sdk_calls"),0);D->SetNumberField(TEXT("duplicate_scope_observation_calls"),0);DuplicateScope=MakeUnique<FDuplicateSocketScope>(R);if(!DuplicateScope->Bind(Asset,S,Mesh,Socket,SN)){return R;}}
 NewSN=SN;if(Kind==1&&!BoneName.IsEmpty()&&!Name(BoneName,BN,R,TEXT("bone_name"))) { return R; }
 if(Kind<2&&S->GetReferenceSkeleton().FindBoneIndex(BN)==INDEX_NONE) { Fail(R,TEXT("skeletal_socket_bone_not_found"),FString::Printf(TEXT("Bone not found in skeleton: %s"),*BoneName));return R; }
 if(Kind==1&&!Rename.IsEmpty()) { if(Dependency(Rename)||!Name(Rename,NewSN,R,TEXT("new_socket_name"))) { if(R.Error.IsEmpty()) { Fail(R,TEXT("skeletal_socket_dependency_forbidden"),TEXT("new_socket_name contains an original forbidden dependency reference.")); }return R; }if(Find(S,NewSN)) { Fail(R,TEXT("skeletal_socket_rename_collision"),FString::Printf(TEXT("Socket name already in use: %s"),*Rename));return R; } }
 FVector L,RotVector,Scale;FRotator Rot;
 if(Kind<2) { const FVector DL=Kind==0?FVector::ZeroVector:Socket->RelativeLocation,DS=Kind==0?FVector::OneVector:Socket->RelativeScale,DR=Kind==0?FVector::ZeroVector:FVector(Socket->RelativeRotation.Roll,Socket->RelativeRotation.Pitch,Socket->RelativeRotation.Yaw);
 if(!Vector(LJson,DL,L,R)||!Vector(RJson,DR,RotVector,R)||!Vector(SJson,DS,Scale,R)) { return R; }Rot=FRotator(RotVector.Y,RotVector.Z,RotVector.X); }
 TSharedPtr<FJsonObject> Before,After;R.Stage=TEXT("before_observation");if(!Capture(Asset,S,Mesh,R,TEXT("before_observation"),Before)) { R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_socket_observation_incomplete"),TEXT("No explicit socket writer dispatched; loaded prestate incomplete."));return R; }
 if(DuplicateScope&&!DuplicateScope->Establish(Before)){return R;}
 const FString Package=S->GetOutermost()->GetName();const bool Force=Kind==0?true:Socket->bForceAlwaysAnimated;
 R.Stage=TEXT("socket_configuration");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("modify_calls"));S->Modify();if(DuplicateScope&&!DuplicateScope->Check(false)){return R;}
 if(Kind==0) { Count(R,TEXT("socket_allocation_attempts"));Socket=NewObject<USkeletalMeshSocket>(S);Count(R,TEXT("socket_allocation_returns"));if(!Socket) { Fail(R,TEXT("skeletal_socket_allocation_failed"),TEXT("Skeleton Modify prefix retained; allocation failed."));return R; }D->SetStringField(TEXT("allocated_socket_path"),Socket->GetPathName()); }
 TStrongObjectPtr<USkeletalMeshSocket> KeepSocket(Socket);
 if(Kind==1) { Count(R,TEXT("modify_calls"));Socket->Modify();if(DuplicateScope&&!DuplicateScope->Check(false)){return R;} }
 if(DuplicateScope){if(Kind==1){DuplicateScope->PlanSet(NewSN,BN,L,Rot,Scale);}else{DuplicateScope->PlanRemove();}}
 if(Kind<2) { if(Kind==0||!BoneName.IsEmpty()) { Socket->BoneName=BN;Count(R,TEXT("field_write_calls")); }if(Kind==0||!Rename.IsEmpty()) { Socket->SocketName=NewSN;Count(R,TEXT("field_write_calls")); }
 Socket->RelativeLocation=L;Socket->RelativeRotation=Rot;Socket->RelativeScale=Scale;Count(R,TEXT("field_write_calls"),3);
 if(Kind==0) { Count(R,TEXT("socket_array_add_calls"));S->Sockets.Add(Socket); } }
 else { Count(R,TEXT("socket_array_remove_calls"));const int32 Removed=S->Sockets.Remove(Socket);D->SetNumberField(TEXT("socket_array_removed_count"),Removed);D->SetStringField(TEXT("removed_socket_path"),Socket->GetPathName());D->SetBoolField(TEXT("removed_object_explicitly_destroyed"),false); }
 if(DuplicateScope&&!DuplicateScope->Check(false)){return R;}
 Count(R,TEXT("mark_package_dirty_calls"));S->MarkPackageDirty();if(DuplicateScope&&!DuplicateScope->Check(false)){return R;}
 if(Kind==2) { Count(R,TEXT("retained_socket_read_calls"));auto Retired=MakeShared<FJsonObject>();D->SetObjectField(TEXT("retained_removed_socket"),Retired);
 Retired->SetStringField(TEXT("object_path"),Socket->GetPathName());Retired->SetStringField(TEXT("owner_path"),Socket->GetOuter()->GetPathName());Retired->SetStringField(TEXT("socket_name"),Socket->SocketName.ToString());Retired->SetStringField(TEXT("bone_name"),Socket->BoneName.ToString());Retired->SetBoolField(TEXT("still_valid"),Loaded(Socket));Retired->SetBoolField(TEXT("current_array_contains_pointer"),S->Sockets.Contains(Socket));Retired->SetBoolField(TEXT("force_always_animated"),Socket->bForceAlwaysAnimated);
 Numbers(Retired,TEXT("location_values"),{Socket->RelativeLocation.X,Socket->RelativeLocation.Y,Socket->RelativeLocation.Z});Numbers(Retired,TEXT("rotation_values_pitch_yaw_roll"),{Socket->RelativeRotation.Pitch,Socket->RelativeRotation.Yaw,Socket->RelativeRotation.Roll});Numbers(Retired,TEXT("scale_values"),{Socket->RelativeScale.X,Socket->RelativeScale.Y,Socket->RelativeScale.Z}); }
 auto Legacy=MakeShared<FJsonObject>();D->SetObjectField(TEXT("original_result"),Legacy);Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("skeleton"),S->GetPathName());Legacy->SetStringField(TEXT("socket_name"),Kind==1?Socket->SocketName.ToString():SocketName);
 if(Kind<2) { Legacy->SetStringField(TEXT("bone_name"),Kind==1?Socket->BoneName.ToString():BoneName); }if(Kind==0) { Legacy->SetBoolField(TEXT("created"),true); }if(Kind==2) { Legacy->SetBoolField(TEXT("removed"),true); }Legacy->SetBoolField(TEXT("save_requested"),Save);Legacy->SetBoolField(TEXT("saved"),false);for(const auto& P:Legacy->Values) { if(P.Key!=TEXT("success")) { D->SetField(P.Key,P.Value); } }
 R.Stage=TEXT("after_configuration");if(!Capture(Asset,S,Mesh,R,TEXT("after_configuration"),After)) { Fail(R,TEXT("skeletal_socket_observation_incomplete"),TEXT("Actual socket writer returned; poststate incomplete; no save/replay."));return R; }
 if(DuplicateScope&&!DuplicateScope->Observation(After,false)){return R;}
 R.bChangeStateKnown=true;R.bChanged=!Same(Before,After);
 if(Kind<2&&!SocketRow(After,Socket).IsValid()) { Fail(R,TEXT("skeletal_socket_current_membership_failed"),TEXT("Known current selected socket membership absent; no save."));return R; }
 auto Expected=StateProjection(Before);TArray<TSharedPtr<FJsonValue>> Rows=Expected->GetArrayField(TEXT("skeleton_sockets"));
 if(Kind==0) { auto Row=SocketRow(After,Socket);if(Row.IsValid()) { Rows.Add(CopyValue(MakeShared<FJsonValueObject>(Row))); } }
 else { for(int32 I=Rows.Num()-1;I>=0;--I) { auto Row=Rows[I]->AsObject();if(!Row->GetBoolField(TEXT("null"))&&Row->GetStringField(TEXT("object_path"))==Socket->GetPathName()) { if(Kind==2) { Rows.RemoveAt(I); }else { Rows[I]=CopyValue(MakeShared<FJsonValueObject>(SocketRow(After,Socket))); } } } }
 for(int32 I=0;I<Rows.Num();++I) { Rows[I]->AsObject()->SetNumberField(TEXT("array_index"),I); }Expected->SetArrayField(TEXT("skeleton_sockets"),Rows);D->SetObjectField(TEXT("expected_state_projection"),Expected);
 const bool Desired=Kind==2?(DuplicateScope?DuplicateScope->Removed(After):Find(S,SN)==nullptr):Selected(After,Socket,S,NewSN,BN,L,Rot,Scale,Force);
 const bool Preserved=Same(Expected,StateProjection(After))&&DirtyFence(Before,After,Package,false)&&Equal(Before->Values[TEXT("disk")],After->Values[TEXT("disk")]);D->SetBoolField(TEXT("configured_selected_values_verified"),Desired);D->SetBoolField(TEXT("unselected_tracked_state_preserved"),Preserved);
 if(!Desired||!Preserved) { Fail(R,TEXT("skeletal_socket_preservation_failed"),TEXT("Actual known socket mutation retained; selected/unselected tracked proof failed; no fallback save."));return R; }
 if(Save)
 {
 if(DuplicateScope&&!DuplicateScope->Check(false)){return R;}
 R.Stage=TEXT("save_skeletal_asset");R.bSaveAttempted=true;Count(R,TEXT("save_attempt_count"));auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("asset_path"),S->GetPathName());Receipt->SetStringField(TEXT("route"),TEXT("UEditorAssetLibrary.SaveLoadedAsset"));Receipt->SetBoolField(TEXT("attempted"),true);Receipt->SetBoolField(TEXT("returned"),false);Receipt->SetBoolField(TEXT("only_if_dirty"),false);D->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Receipt)});
 R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(S,false);Receipt->SetBoolField(TEXT("returned"),true);Receipt->SetBoolField(TEXT("saved"),R.bSaved);Legacy->SetBoolField(TEXT("saved"),R.bSaved);D->SetBoolField(TEXT("saved"),R.bSaved);
 if(!R.bSaved) { Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error_code"),TEXT("asset_save_failed"));Legacy->SetStringField(TEXT("failed_stage"),TEXT("save_skeletal_asset"));Legacy->SetStringField(TEXT("failed_stage_effect"),TEXT("mutation"));
 for(const TCHAR* K:{TEXT("mutation_attempted"),TEXT("mutation_performed"),TEXT("partial_target_modified"),TEXT("partial_mutation"),TEXT("failed_stage_mutation_performed"),TEXT("mutation_state_unknown"),TEXT("do_not_retry"),TEXT("workflow_recovery_required")}) { Legacy->SetBoolField(K,true); }
 const FString Error=TEXT("The skeletal asset was modified in memory but could not be saved; inspect it and save or revert it manually before continuing");Legacy->SetStringField(TEXT("error"),Error);Legacy->SetStringField(TEXT("message"),Error);Legacy->SetStringField(TEXT("recovery_guidance"),TEXT("Resolve source-control or package write access, then save or revert the dirty skeletal asset manually before retrying."));for(const auto& P:Legacy->Values) { if(P.Key!=TEXT("success")) { D->SetField(P.Key,P.Value); } } }
 if(DuplicateScope&&!DuplicateScope->Check(true)){return R;}
 TSharedPtr<FJsonObject> Saved;R.Stage=TEXT("after_save");if(!Capture(Asset,S,Mesh,R,TEXT("after_save"),Saved)) { R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_socket_saved_observation_incomplete"),TEXT("Actual forced save receipt retained; post-save observation incomplete."));return R; }
 if(DuplicateScope&&!DuplicateScope->Observation(Saved,true)){return R;}
 R.bChangeStateKnown=true;R.bChanged=!Same(Before,Saved);const bool Stable=Same(StateProjection(After),StateProjection(Saved))&&DirtyFence(After,Saved,Package,true);D->SetBoolField(TEXT("saved_tracked_state_preserved"),Stable);
 if(!R.bSaved) { Fail(R,TEXT("asset_save_failed"),Legacy->GetStringField(TEXT("error")));return R; }
 if(!Stable||Saved->GetBoolField(TEXT("skeleton_dirty"))) { Fail(R,TEXT("skeletal_socket_saved_preservation_failed"),TEXT("Actual saved receipt retained; current tracked post-save state is not verified."));return R; }
 }
 for(const auto& P:Legacy->Values) { if(P.Key!=TEXT("success")) { D->SetField(P.Key,P.Value); } }
 R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
Variable::FResult Add(const FString& P,const FString& N,const FString& B,const FString& L,const FString& R,const FString& S,bool Save) { return Write(0,P,N,B,FString(),L,R,S,Save); }
Variable::FResult Set(const FString& P,const FString& N,const FString& B,const FString& New,const FString& L,const FString& R,const FString& S,bool Save) { return Write(1,P,N,B,New,L,R,S,Save); }
Variable::FResult Remove(const FString& P,const FString& N,bool Save) { return Write(2,P,N,FString(),FString(),TEXT("null"),TEXT("null"),TEXT("null"),Save); }
}
