// CM: original bone translation retargeting, exported setter only; no compiler/PostEdit.
#include "Compatibility/UE58/CubelessSkeletalRetargetApi.h"
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
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/PropertyAccessUtil.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
namespace Cubeless::UE58::SkeletalRetarget
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
bool Capture(UObject* Asset,USkeleton* S,USkeletalMesh* Mesh,FResult& R,const TCHAR* Phase,TSharedPtr<FJsonObject>& Out)
{
 R.Data->SetNumberField(TEXT("observation_capture_calls"),R.Data->GetNumberField(TEXT("observation_capture_calls"))+1);
 auto J=MakeShared<FJsonObject>();Out=J;R.Data->SetObjectField(Phase,J);J->SetBoolField(TEXT("complete"),false);
 if(!Loaded(Asset)||!Loaded(S)) { return false; }
 J->SetStringField(TEXT("asset_path"),Asset->GetPathName());J->SetStringField(TEXT("asset_class"),Asset->GetClass()->GetPathName());J->SetStringField(TEXT("skeleton"),S->GetPathName());
 if(Mesh) { if(Mesh->IsCompiling()) { return false; } R.Data->SetNumberField(TEXT("skeleton_getter_calls"),R.Data->GetNumberField(TEXT("skeleton_getter_calls"))+1);if(Mesh->GetSkeleton()!=S) { return false; } }
 const auto& Ref=S->GetReferenceSkeleton();const int32 N=Ref.GetNum(),Raw=Ref.GetRawBoneNum();
 J->SetNumberField(TEXT("final_bone_count"),N);J->SetNumberField(TEXT("raw_bone_count"),Raw);
 auto Proof=MakeShared<FJsonObject>();J->SetObjectField(TEXT("bone_tree_read_admission"),Proof);Proof->SetBoolField(TEXT("verified"),false);
 const auto* Tree=FindFProperty<FArrayProperty>(USkeleton::StaticClass(),TEXT("BoneTree"));
 const auto* Inner=Tree?CastField<FStructProperty>(Tree->Inner):nullptr;
 Proof->SetStringField(TEXT("property_path"),Tree?Tree->GetPathName():FString());Proof->SetStringField(TEXT("inner_struct"),Inner&&Inner->Struct?Inner->Struct->GetPathName():FString());
 bool Permission=false;if(Tree) { R.Data->SetNumberField(TEXT("bone_tree_permission_checks"),R.Data->GetNumberField(TEXT("bone_tree_permission_checks"))+1);Permission=PropertyAccessUtil::CanGetPropertyValue(Tree)==EPropertyAccessResultFlags::Success; }Proof->SetBoolField(TEXT("public_read_permission"),Permission);
 if(!Tree||Tree->ArrayDim!=1||Tree->GetOwnerUObject()!=USkeleton::StaticClass()||!Inner||Inner->ArrayDim!=1||!Inner->Struct||Inner->Struct->GetPathName()!=TEXT("/Script/Engine.BoneNode")||!Permission) { return false; }
 const auto* ModeProperty=FindFProperty<FByteProperty>(Inner->Struct,TEXT("TranslationRetargetingMode"));
 UEnum* ModeEnum=nullptr;if(ModeProperty) { R.Data->SetNumberField(TEXT("bone_tree_enum_getter_calls"),R.Data->GetNumberField(TEXT("bone_tree_enum_getter_calls"))+1);ModeEnum=ModeProperty->GetIntPropertyEnum(); }Proof->SetStringField(TEXT("mode_enum"),ModeEnum?ModeEnum->GetPathName():FString());
 if(!ModeProperty||!ModeEnum||ModeEnum->GetPathName()!=TEXT("/Script/Engine.EBoneTranslationRetargetingMode")) { return false; }
 R.Data->SetNumberField(TEXT("bone_tree_permission_checks"),R.Data->GetNumberField(TEXT("bone_tree_permission_checks"))+1);const bool ModePermission=PropertyAccessUtil::CanGetPropertyValue(ModeProperty)==EPropertyAccessResultFlags::Success;Proof->SetBoolField(TEXT("mode_public_read_permission"),ModePermission);if(!ModePermission) { return false; }
 FScriptArrayHelper TreeView(Tree,Tree->ContainerPtrToValuePtr<void>(S));const int32 TreeCount=TreeView.Num();
 Proof->SetNumberField(TEXT("array_count"),TreeCount);Proof->SetNumberField(TEXT("raw_reference_count"),Raw);
 if(N>16384||Raw<=0||Raw>N||TreeCount!=Raw||S->Sockets.Num()>4096||Ref.GetRefBonePose().Num()!=N) { return false; }
 Proof->SetBoolField(TEXT("verified"),true);
 TArray<TSharedPtr<FJsonValue>> Bones,Poses,Sockets;
 J->SetArrayField(TEXT("bones"),Bones);J->SetArrayField(TEXT("reference_pose"),Poses);J->SetArrayField(TEXT("skeleton_sockets"),Sockets);
 for(int32 I=0;I<N;++I)
 {
  const int32 Parent=Ref.GetRefBoneInfo()[I].ParentIndex;if(Parent<INDEX_NONE || Parent>=I) { return false; }
  auto B=MakeShared<FJsonObject>();B->SetNumberField(TEXT("index"),I);B->SetStringField(TEXT("name"),Ref.GetBoneName(I).ToString());B->SetNumberField(TEXT("parent_index"),Parent);
  B->SetStringField(TEXT("parent_name"),Parent>=0?Ref.GetBoneName(Parent).ToString():FString());B->SetBoolField(TEXT("storage_backed_raw_bone"),I<Raw);R.Data->SetNumberField(TEXT("retargeting_mode_getter_calls"),R.Data->GetNumberField(TEXT("retargeting_mode_getter_calls"))+1);B->SetStringField(TEXT("translation_retargeting"),Mode(S->GetBoneTranslationRetargetingMode(I)));Bones.Add(MakeShared<FJsonValueObject>(B));J->SetArrayField(TEXT("bones"),Bones);
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
bool Resolve(const FString& Path,FResult& R,UObject*& Asset,USkeleton*& S,USkeletalMesh*& Mesh)
{
 R.Stage=TEXT("load_asset");Count(R,TEXT("asset_load_count"));auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("requested_path"),Path);L->SetBoolField(TEXT("attempted"),true);L->SetBoolField(TEXT("returned"),false);R.Data->SetArrayField(TEXT("asset_load_calls"),{MakeShared<FJsonValueObject>(L)});
 Asset=UEditorAssetLibrary::LoadAsset(Path);L->SetBoolField(TEXT("returned"),true);L->SetStringField(TEXT("asset_path"),Asset?Asset->GetPathName():FString());
 if(!Asset) { return Fail(R,TEXT("skeletal_asset_not_found"),FString::Printf(TEXT("Skeleton or SkeletalMesh not found: %s"),*Path)); }S=Cast<USkeleton>(Asset);Mesh=Cast<USkeletalMesh>(Asset);
 if(Mesh) { if(!Loaded(Mesh)||Mesh->GetClass()!=USkeletalMesh::StaticClass()||Mesh->IsCompiling()) { return Fail(R,TEXT("skeletal_mesh_scope_unsupported"),TEXT("Loaded idle native mesh required; no explicit async wait.")); }Count(R,TEXT("skeleton_getter_calls"));S=Mesh->GetSkeleton();if(!S) { return Fail(R,TEXT("skeletal_mesh_no_skeleton"),FString::Printf(TEXT("SkeletalMesh has no skeleton: %s"),*Path)); } }
 if(!Loaded(S)||S->GetClass()!=USkeleton::StaticClass()||!S->IsAsset()) { return Fail(R,TEXT("skeletal_skeleton_scope_unsupported"),TEXT("Exact native Skeleton or native mesh with Skeleton required.")); }
 // Read group admits Engine assets; writes are limited to copied project Skeletons.
 if(!S->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))) { return Fail(R,TEXT("skeletal_retarget_write_root_unsupported"),TEXT("Retarget writes require an isolated copied /Game/ Skeleton; Engine/plugin sources remain read-only.")); }
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

bool Name(const FString& V,FName& N,FResult& R)
{
 if(V.IsEmpty()) { return Fail(R,TEXT("skeletal_retarget_name_invalid"),TEXT("Missing 'bone_name' parameter")); }
 if(V.Len()>=NAME_SIZE) { return Fail(R,TEXT("skeletal_retarget_name_invalid"),FString::Printf(TEXT("'bone_name' is too long for an Unreal name (maximum %d characters)"),NAME_SIZE-1)); }
 if(!Text(V,NAME_SIZE-1)) { return Fail(R,TEXT("skeletal_retarget_name_invalid"),TEXT("'bone_name' contains an interior NUL; this finite slice does not admit truncated names.")); }
 N=FName(*V);return !N.IsNone()||Fail(R,TEXT("skeletal_retarget_name_invalid"),FString::Printf(TEXT("Invalid 'bone_name' name '%s': name resolves to None"),*V));
}
bool ParseMode(const FString& V,EBoneTranslationRetargetingMode::Type& M)
{
 if(V.Equals(TEXT("Animation"),ESearchCase::IgnoreCase)) { M=EBoneTranslationRetargetingMode::Animation;return true; }
 if(V.Equals(TEXT("Skeleton"),ESearchCase::IgnoreCase)) { M=EBoneTranslationRetargetingMode::Skeleton;return true; }
 if(V.Equals(TEXT("AnimationScaled"),ESearchCase::IgnoreCase)) { M=EBoneTranslationRetargetingMode::AnimationScaled;return true; }
 if(V.Equals(TEXT("AnimationRelative"),ESearchCase::IgnoreCase)) { M=EBoneTranslationRetargetingMode::AnimationRelative;return true; }
 if(V.Equals(TEXT("OrientAndScale"),ESearchCase::IgnoreCase)) { M=EBoneTranslationRetargetingMode::OrientAndScale;return true; }
 return false;
}
bool Selected(const TSharedPtr<FJsonObject>& State,const TArray<int32>& Affected,const TCHAR* Desired)
{
 const auto& Bones=State->GetArrayField(TEXT("bones"));for(int32 I:Affected) { if(!Bones.IsValidIndex(I)||!Bones[I]->AsObject()->GetBoolField(TEXT("storage_backed_raw_bone"))||!Bones[I]->AsObject()->GetStringField(TEXT("translation_retargeting")).Equals(Desired,ESearchCase::CaseSensitive)) { return false; } }return true;
}
void ProjectLegacy(FResult& R,const TSharedRef<FJsonObject>& Legacy)
{ for(const auto& P:Legacy->Values) { if(P.Key!=TEXT("success")) { R.Data->SetField(P.Key,P.Value); } } }
void SaveFailure(const TSharedRef<FJsonObject>& Legacy)
{
 Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error_code"),TEXT("asset_save_failed"));Legacy->SetStringField(TEXT("failed_stage"),TEXT("save_skeletal_asset"));Legacy->SetStringField(TEXT("failed_stage_effect"),TEXT("mutation"));
 for(const TCHAR* K:{TEXT("mutation_attempted"),TEXT("mutation_performed"),TEXT("partial_target_modified"),TEXT("partial_mutation"),TEXT("failed_stage_mutation_performed"),TEXT("mutation_state_unknown"),TEXT("do_not_retry"),TEXT("workflow_recovery_required")}) { Legacy->SetBoolField(K,true); }
 const FString Error=TEXT("The skeletal asset was modified in memory but could not be saved; inspect it and save or revert it manually before continuing");Legacy->SetStringField(TEXT("error"),Error);Legacy->SetStringField(TEXT("message"),Error);Legacy->SetStringField(TEXT("recovery_guidance"),TEXT("Resolve source-control or package write access, then save or revert the dirty skeletal asset manually before retrying."));
}
}
FResult Set(const FString& Path,const FString& BoneName,const FString& ModeName,bool Recursive,bool Save)
{
 FResult R;R.bSaveRequested=Save;auto D=R.Data;
 D->SetStringField(TEXT("requested_skeleton_path"),Path);D->SetStringField(TEXT("requested_bone_name"),BoneName);D->SetStringField(TEXT("requested_mode"),ModeName);D->SetBoolField(TEXT("requested_recursive"),Recursive);
 D->SetStringField(TEXT("scope"),TEXT("Exact native /Game Skeleton or idle native mesh resolving it; selected raw backed bone and recursive raw descendants only. Protected BoneTree is read through public permission-checked reflection and written only by the exported setter."));
 D->SetBoolField(TEXT("full_reflected_asset_state_observed"),false);D->SetBoolField(TEXT("implicit_PostLoad_PreSave_cache_gc_effects_exhaustively_measured"),false);D->SetBoolField(TEXT("runtime_retarget_evaluation_verified"),false);
 for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("asset_load_count"),TEXT("skeleton_getter_calls"),TEXT("observation_capture_calls"),TEXT("bone_tree_permission_checks"),TEXT("bone_tree_enum_getter_calls"),TEXT("retargeting_mode_getter_calls"),TEXT("modify_calls"),TEXT("retargeting_setter_attempt_count"),TEXT("retargeting_setter_return_count"),TEXT("mark_package_dirty_calls"),TEXT("post_edit_change_calls"),TEXT("compile_calls"),TEXT("save_attempt_count")}) { D->SetNumberField(K,0); }
 D->SetArrayField(TEXT("asset_load_calls"),{});D->SetArrayField(TEXT("save_calls"),{});D->SetArrayField(TEXT("setter_calls"),{});
 if(!IsEditorGameThread()||IsPlaySessionActive()||Path.IsEmpty()||!Text(Path,1024)) { Fail(R,TEXT("skeletal_retarget_editor_or_path_invalid"),TEXT("Idle Editor and bounded literal asset path required."));return R; }
 Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("Cubeless","RetargetMode","Set bone translation retargeting"));
 UObject* Asset=nullptr;USkeleton* S=nullptr;USkeletalMesh* Mesh=nullptr;if(!Resolve(Path,R,Asset,S,Mesh)) { return R; }TStrongObjectPtr<UObject> KeepAsset(Asset);TStrongObjectPtr<USkeleton> KeepSkeleton(S);
 FName BN;if(!Name(BoneName,BN,R)) { return R; }EBoneTranslationRetargetingMode::Type DesiredMode;
 if(ModeName.IsEmpty()) { Fail(R,TEXT("skeletal_retarget_mode_invalid"),TEXT("Missing 'mode' parameter (Animation, Skeleton, AnimationScaled, AnimationRelative, OrientAndScale)"));return R; }
 if(!Text(ModeName,128)||!ParseMode(ModeName,DesiredMode)) { Fail(R,TEXT("skeletal_retarget_mode_invalid"),FString::Printf(TEXT("Unknown retargeting mode '%s' (Animation, Skeleton, AnimationScaled, AnimationRelative, OrientAndScale)"),*ModeName));return R; }
 const auto& Ref=S->GetReferenceSkeleton();const int32 BoneIndex=Ref.FindBoneIndex(BN);if(BoneIndex==INDEX_NONE) { Fail(R,TEXT("skeletal_retarget_bone_not_found"),FString::Printf(TEXT("Bone not found in skeleton: %s"),*BoneName));return R; }
 const int32 Raw=Ref.GetRawBoneNum();if(BoneIndex<0||BoneIndex>=Raw||Ref.FindRawBoneIndex(BN)!=BoneIndex) { Fail(R,TEXT("skeletal_retarget_virtual_or_unbacked_unsupported"),TEXT("Only current raw backed bones are admitted before the installed unchecked setter; virtual/unbacked variants remain incomplete."));return R; }
 TSharedPtr<FJsonObject> Before,After;R.Stage=TEXT("before_observation");if(!Capture(Asset,S,Mesh,R,TEXT("before_observation"),Before)) { R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_retarget_observation_incomplete"),TEXT("No explicit setter dispatched; current permission/type/bounds or tracked prestate incomplete."));return R; }
 TArray<int32> Affected;Affected.Add(BoneIndex);if(Recursive) { for(int32 I=BoneIndex+1;I<Raw;++I) { if(Ref.BoneIsChildOf(I,BoneIndex)) { Affected.Add(I); } } }
 TArray<TSharedPtr<FJsonValue>> Indices;for(int32 I:Affected) { Indices.Add(MakeShared<FJsonValueNumber>(I)); }D->SetArrayField(TEXT("source_expected_affected_raw_indices"),Indices);D->SetNumberField(TEXT("selected_raw_bone_index"),BoneIndex);
 auto Expected=StateProjection(Before);const TCHAR* DesiredText=Mode(DesiredMode);auto ExpectedBones=Expected->GetArrayField(TEXT("bones"));for(int32 I:Affected) { ExpectedBones[I]->AsObject()->SetStringField(TEXT("translation_retargeting"),DesiredText); }Expected->SetArrayField(TEXT("bones"),ExpectedBones);D->SetObjectField(TEXT("expected_state_projection"),Expected);
 const FString Package=S->GetOutermost()->GetName();R.Stage=TEXT("retarget_configuration");R.bMutationAttempted=true;R.bChangeStateKnown=false;
 Count(R,TEXT("modify_calls"));const bool Modified=S->Modify();D->SetBoolField(TEXT("modify_returned"),true);D->SetBoolField(TEXT("modify_transaction_recorded"),Modified);
 auto Call=MakeShared<FJsonObject>();Call->SetNumberField(TEXT("raw_bone_index"),BoneIndex);Call->SetStringField(TEXT("mode"),DesiredText);Call->SetBoolField(TEXT("recursive"),Recursive);Call->SetBoolField(TEXT("attempted"),true);Call->SetBoolField(TEXT("returned"),false);D->SetArrayField(TEXT("setter_calls"),{MakeShared<FJsonValueObject>(Call)});
 Count(R,TEXT("retargeting_setter_attempt_count"));S->SetBoneTranslationRetargetingMode(BoneIndex,DesiredMode,Recursive);Count(R,TEXT("retargeting_setter_return_count"));Call->SetBoolField(TEXT("returned"),true);
 Count(R,TEXT("mark_package_dirty_calls"));const bool Dirty=S->MarkPackageDirty();D->SetBoolField(TEXT("mark_package_dirty_returned"),true);D->SetBoolField(TEXT("mark_package_dirty_result"),Dirty);
 auto Legacy=MakeShared<FJsonObject>();D->SetObjectField(TEXT("original_result"),Legacy);Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("skeleton"),S->GetPathName());Legacy->SetStringField(TEXT("bone_name"),BoneName);Legacy->SetStringField(TEXT("mode"),DesiredText);Legacy->SetBoolField(TEXT("recursive"),Recursive);Legacy->SetBoolField(TEXT("save_requested"),Save);Legacy->SetBoolField(TEXT("saved"),false);ProjectLegacy(R,Legacy);
 R.Stage=TEXT("after_configuration");if(!Capture(Asset,S,Mesh,R,TEXT("after_configuration"),After)) { Fail(R,TEXT("skeletal_retarget_observation_incomplete"),TEXT("Actual setter/dirty returned; partial poststate retained, no save/replay."));return R; }
 R.bChangeStateKnown=true;R.bChanged=!Same(Before,After);
 const bool SelectedOK=Selected(After,Affected,DesiredText);const bool Stable=Same(Expected,StateProjection(After))&&DirtyFence(Before,After,Package,false)&&Equal(Before->Values[TEXT("disk")],After->Values[TEXT("disk")]);
 bool ModesChanged=false;for(int32 I:Affected) { ModesChanged|=!Before->GetArrayField(TEXT("bones"))[I]->AsObject()->GetStringField(TEXT("translation_retargeting")).Equals(After->GetArrayField(TEXT("bones"))[I]->AsObject()->GetStringField(TEXT("translation_retargeting")),ESearchCase::CaseSensitive); }
 D->SetBoolField(TEXT("configured_selected_modes_verified"),SelectedOK);D->SetBoolField(TEXT("unselected_tracked_state_preserved"),Stable);D->SetBoolField(TEXT("mode_values_changed"),ModesChanged);
 if(!SelectedOK||!Stable) { Fail(R,TEXT("skeletal_retarget_preservation_failed"),TEXT("Known actual setter mutation retained; selected/unselected tracked proof failed, no fallback save."));return R; }
 if(Save)
 {
  R.Stage=TEXT("save_skeletal_asset");R.bSaveAttempted=true;Count(R,TEXT("save_attempt_count"));auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("asset_path"),S->GetPathName());Receipt->SetStringField(TEXT("route"),TEXT("UEditorAssetLibrary.SaveLoadedAsset"));Receipt->SetBoolField(TEXT("attempted"),true);Receipt->SetBoolField(TEXT("returned"),false);Receipt->SetBoolField(TEXT("only_if_dirty"),false);D->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Receipt)});
  D->SetBoolField(TEXT("saved_tracked_state_preserved"),false);R.bChangeStateKnown=false;
  R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(S,false);Receipt->SetBoolField(TEXT("returned"),true);Receipt->SetBoolField(TEXT("saved"),R.bSaved);Legacy->SetBoolField(TEXT("saved"),R.bSaved);if(!R.bSaved) { SaveFailure(Legacy); }ProjectLegacy(R,Legacy);
  TSharedPtr<FJsonObject> Saved;R.Stage=TEXT("after_save");if(!Capture(Asset,S,Mesh,R,TEXT("after_save"),Saved)) { Fail(R,TEXT("skeletal_retarget_saved_observation_incomplete"),TEXT("Actual forced-save/legacy receipt retained; post-save scope incomplete."));return R; }
  R.bChangeStateKnown=true;R.bChanged=!Same(Before,Saved);const bool SourceDisk=Asset->GetOutermost()==S->GetOutermost()||Same(After->GetObjectField(TEXT("disk"))->GetObjectField(TEXT("input_asset")),Saved->GetObjectField(TEXT("disk"))->GetObjectField(TEXT("input_asset")));
  const bool SaveStable=Same(StateProjection(After),StateProjection(Saved))&&DirtyFence(After,Saved,Package,true)&&SourceDisk;D->SetBoolField(TEXT("saved_source_disk_preserved"),SourceDisk);D->SetBoolField(TEXT("saved_tracked_state_preserved"),SaveStable);
  if(!R.bSaved) { Fail(R,TEXT("asset_save_failed"),Legacy->GetStringField(TEXT("error")));return R; }
  if(!SaveStable||!Selected(Saved,Affected,DesiredText)||Saved->GetBoolField(TEXT("skeleton_dirty"))) { Fail(R,TEXT("skeletal_retarget_saved_preservation_failed"),TEXT("Actual save receipt retained; tracked current modes/other state/clean package not verified."));return R; }
 }
 R.Stage=TEXT("complete");R.bPostconditionPassed=true;R.bSuccess=true;return R;
}
}
