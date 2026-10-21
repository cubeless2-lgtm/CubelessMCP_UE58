// CJ: original skeleton-backed bone/socket reads; no asset mutation or compiler.
#include "Compatibility/UE58/CubelessSkeletalApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/Skeleton.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "ReferenceSkeleton.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::Skeletal
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
TSharedRef<FJsonObject> Begin(const FString& Path,FResult& R)
{
 auto J=R.Data;J->SetStringField(TEXT("requested_skeleton_path"),Path);J->SetBoolField(TEXT("read_only"),true);
 J->SetBoolField(TEXT("implicit_PostLoad_async_cache_effects_exhaustively_measured"),false);
 J->SetStringField(TEXT("socket_scope"),TEXT("USkeleton.Sockets only, including a SkeletalMesh input resolved to its skeleton; mesh-only/active sockets excluded exactly as original."));
 for(const TCHAR* K:{TEXT("asset_load_count"),TEXT("skeleton_getter_calls"),TEXT("observation_capture_calls"),TEXT("transaction_calls"),TEXT("modify_calls"),TEXT("post_edit_change_calls"),TEXT("compile_calls"),TEXT("save_calls")}) { J->SetNumberField(K,0); }
 J->SetArrayField(TEXT("asset_load_calls"),{});return J;
}
bool Resolve(const FString& Path,FResult& R,UObject*& Asset,USkeleton*& Skeleton,USkeletalMesh*& Mesh)
{
 if(!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R,TEXT("skeletal_editor_state_invalid"),TEXT("Idle Editor game thread required.")); }
 if(Path.IsEmpty()) { return Fail(R,TEXT("skeletal_path_missing"),TEXT("Missing 'skeleton_path' parameter (Skeleton or SkeletalMesh path)")); }
 if(!Text(Path,1024)) { return Fail(R,TEXT("skeletal_path_invalid"),TEXT("Skeleton path exceeds bounded literal path scope.")); }
 R.Stage=TEXT("load_asset");R.Data->SetNumberField(TEXT("asset_load_count"),1);
 auto Load=MakeShared<FJsonObject>();Load->SetStringField(TEXT("requested_path"),Path);Load->SetBoolField(TEXT("attempted"),true);Load->SetBoolField(TEXT("returned"),false);
 R.Data->SetArrayField(TEXT("asset_load_calls"),{MakeShared<FJsonValueObject>(Load)});
 Asset=UEditorAssetLibrary::LoadAsset(Path);Load->SetBoolField(TEXT("returned"),true);Load->SetStringField(TEXT("asset_path"),Asset?Asset->GetPathName():FString());
 Load->SetStringField(TEXT("asset_class"),Asset?Asset->GetClass()->GetPathName():FString());
 Skeleton=Cast<USkeleton>(Asset);Mesh=Cast<USkeletalMesh>(Asset);
 if(Mesh)
 {
  if(Mesh->GetClass()!=USkeletalMesh::StaticClass() || !Loaded(Mesh)) { return Fail(R,TEXT("skeletal_mesh_scope_unsupported"),TEXT("Exact loaded native SkeletalMesh required.")); }
  // Public GetSkeleton waits for the async property; do not enter it while compilation is active.
  if(Mesh->IsCompiling()) { return Fail(R,TEXT("skeletal_mesh_compiling"),TEXT("SkeletalMesh async compilation is outside this immediate read slice; no explicit finish/wait/poll.")); }
  R.Data->SetNumberField(TEXT("skeleton_getter_calls"),1);Skeleton=Mesh->GetSkeleton();
  if(!Skeleton) { return Fail(R,TEXT("skeletal_mesh_no_skeleton"),FString::Printf(TEXT("SkeletalMesh has no skeleton: %s"),*Path)); }
 }
 if(!Skeleton) { return Fail(R,TEXT("skeletal_asset_not_found"),FString::Printf(TEXT("Skeleton or SkeletalMesh not found: %s"),*Path)); }
 if(!Loaded(Skeleton) || Skeleton->GetClass()!=USkeleton::StaticClass() || !Skeleton->IsAsset())
 { return Fail(R,TEXT("skeletal_skeleton_scope_unsupported"),TEXT("Exact loaded native Skeleton asset required; generated/custom classes remain incomplete.")); }
 R.Data->SetStringField(TEXT("resolved_asset_path"),Asset->GetPathName());R.Data->SetStringField(TEXT("skeleton"),Skeleton->GetPathName());
 R.Data->SetBoolField(TEXT("input_was_skeletal_mesh"),Mesh!=nullptr);return true;
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
bool Capture(UObject* Asset,USkeleton* S,USkeletalMesh* Mesh,FResult& R,const TCHAR* Phase,TSharedPtr<FJsonObject>& Out)
{
 R.Data->SetNumberField(TEXT("observation_capture_calls"),R.Data->GetNumberField(TEXT("observation_capture_calls"))+1);
 auto J=MakeShared<FJsonObject>();Out=J;R.Data->SetObjectField(Phase,J);J->SetBoolField(TEXT("complete"),false);
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
FResult Read(const FString& Path,const FString& Contains,int32 Max,bool SocketsOnly)
{
 FResult R;Begin(Path,R);R.Data->SetStringField(TEXT("bone_name_contains"),Contains);R.Data->SetNumberField(TEXT("requested_max_bones"),Max);
 if(!Text(Contains,1024)) { Fail(R,TEXT("skeletal_filter_invalid"),TEXT("Bone filter exceeds bounded literal string scope."));return R; }
 UObject* Asset=nullptr;USkeleton* S=nullptr;USkeletalMesh* Mesh=nullptr;if(!Resolve(Path,R,Asset,S,Mesh)) { return R; }
 TStrongObjectPtr<UObject> KeepAsset(Asset);TStrongObjectPtr<USkeleton> KeepSkeleton(S);TSharedPtr<FJsonObject> Before,After;
 R.Stage=TEXT("before_observation");if(!Capture(Asset,S,Mesh,R,TEXT("before_observation"),Before)) { R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_observation_incomplete"),TEXT("Loaded read prefix is incomplete; no explicit writer/compile/save."));return R; }
 auto Legacy=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("original_result"),Legacy);Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("skeleton"),S->GetPathName());
 if(SocketsOnly)
 {
  TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& V:Before->GetArrayField(TEXT("skeleton_sockets"))) { auto Raw=V->AsObject();if(Raw->GetBoolField(TEXT("null"))) { continue; }auto Row=MakeShared<FJsonObject>();
   for(const TCHAR* Key:{TEXT("socket_name"),TEXT("bone_name"),TEXT("location"),TEXT("rotation"),TEXT("scale")}) { Row->SetStringField(Key,Raw->GetStringField(Key)); }Rows.Add(MakeShared<FJsonValueObject>(Row)); }
  Legacy->SetNumberField(TEXT("socket_count"),Rows.Num());Legacy->SetArrayField(TEXT("sockets"),Rows);
 }
 else
 {
  const int32 Limit=FMath::Clamp(Max,1,4096);int32 Matched=0;TArray<TSharedPtr<FJsonValue>> Rows;
  for(const auto& V:Before->GetArrayField(TEXT("bones"))) { if(!Contains.IsEmpty()&&!V->AsObject()->GetStringField(TEXT("name")).Contains(Contains)) { continue; }++Matched;if(Rows.Num()<Limit) { Rows.Add(V); } }
  Legacy->SetNumberField(TEXT("total_bones"),Before->GetArrayField(TEXT("bones")).Num());Legacy->SetNumberField(TEXT("matched_bones"),Matched);Legacy->SetNumberField(TEXT("returned_bones"),Rows.Num());Legacy->SetBoolField(TEXT("truncated"),Matched>Rows.Num());Legacy->SetArrayField(TEXT("bones"),Rows);R.Data->SetNumberField(TEXT("effective_max_bones"),Limit);
 }
 for(const auto& Pair:Legacy->Values) { if(Pair.Key!=TEXT("success")) { R.Data->SetField(Pair.Key,Pair.Value); } }
 R.Stage=TEXT("after_observation");if(!Capture(Asset,S,Mesh,R,TEXT("after_observation"),After)) { R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_observation_incomplete"),TEXT("Read payload retained; final observation incomplete."));return R; }
 R.bChanged=!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After));R.bChangeStateKnown=true;R.Data->SetBoolField(TEXT("scoped_state_preserved"),!R.bChanged);
 if(R.bChanged) { Fail(R,TEXT("skeletal_read_state_changed"),TEXT("Current scoped bone/socket/pose/disk/dirty observations differed; no explicit asset writer."));return R; }
 R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
Variable::FResult ListBones(const FString& Path,const FString& Contains,int32 Max) { return Read(Path,Contains,Max,false); }
Variable::FResult ListSockets(const FString& Path) { return Read(Path,FString(),500,true); }
}
