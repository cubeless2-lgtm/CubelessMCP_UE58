// IM: original persistent PhysicsAsset15, fresh creation and exact existing in-place rebuild; no source assignment.
#include "Compatibility/UE58/CubelessSkeletalPhysicsAssetApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
namespace Cubeless::UE58::SkeletalPhysicsAsset
{
namespace
{
using Variable::FResult;
// IR Physics-only wall-clock telemetry. It changes JSON/counters only; no UObject/SDK work or admission.
struct FPhysicsTelemetry
{
 enum EPhase { SourceLoadPrestate, SDKGeneration, ScratchBodies, ScratchPairs, ScratchConstraints, ScratchVerify, TargetDuplicates, ModifyCommit, SavePostverify, PhaseCount };
 static const TCHAR* PhaseName(int32 P)
 {
  static const TCHAR* Names[PhaseCount]={TEXT("source_load_prestate"),TEXT("sdk_generation_and_observation"),TEXT("scratch_bodies"),TEXT("scratch_pairs"),TEXT("scratch_constraints"),TEXT("scratch_verify"),TEXT("target_prepare_and_duplicates"),TEXT("modify_commit_and_verify"),TEXT("save_and_postverify")};
  return Names[P];
 }
 static constexpr int32 WorkCount=32;
 static const TCHAR* WorkName(int32 I)
 {
  static const TCHAR* Names[WorkCount]={TEXT("asset_load_count"),TEXT("observation_capture_calls"),TEXT("scratch_allocation_attempts"),TEXT("scratch_allocation_returns"),TEXT("generator_attempts"),TEXT("generator_returns"),TEXT("body_modify_calls"),TEXT("body_collision_setter_calls"),TEXT("body_field_write_calls"),TEXT("pair_collision_setter_calls"),TEXT("constraint_modify_calls"),TEXT("constraint_setter_calls"),TEXT("default_profile_copy_calls"),TEXT("asset_validation_calls"),TEXT("body_duplicate_attempts"),TEXT("body_duplicate_returns"),TEXT("constraint_duplicate_attempts"),TEXT("constraint_duplicate_returns"),TEXT("target_modify_calls"),TEXT("target_commit_calls"),TEXT("body_index_update_calls"),TEXT("bounds_update_calls"),TEXT("refresh_physics_asset_calls"),TEXT("mark_package_dirty_calls"),TEXT("save_attempt_count"),TEXT("save_return_count"),TEXT("projection_getter_calls"),TEXT("collision_enabled_getter_calls"),TEXT("collision_pair_getter_calls"),TEXT("restore_preview_calls"),TEXT("restore_refresh_calls"),TEXT("restore_dirty_calls")};
  return Names[I];
 }
 struct FPhaseSample { double Seconds=0,CheckSeconds=0,RawSeconds=0,Work[WorkCount]={};int64 Entries=0,CheckCalls=0,RawCalls=0; };
 TSharedRef<FJsonObject> Data;FPhaseSample Samples[PhaseCount];double PreviousWork[WorkCount]={};int32 Current=SourceLoadPrestate;
 double Started=FPlatformTime::Seconds(),PhaseStarted=Started;
 explicit FPhysicsTelemetry(const TSharedRef<FJsonObject>& InData):Data(InData){Samples[Current].Entries=1;}
 void ClosePhase(double Now)
 {
  Samples[Current].Seconds+=Now-PhaseStarted;
  for(int32 I=0;I<WorkCount;++I){double Value=0;Data->TryGetNumberField(WorkName(I),Value);Samples[Current].Work[I]+=Value-PreviousWork[I];PreviousWork[I]=Value;}
 }
 void Phase(EPhase Next)
 {
  const double Now=FPlatformTime::Seconds();ClosePhase(Now);Current=Next;PhaseStarted=Now;++Samples[Current].Entries;
 }
 struct FGuardTimer
 {
  FPhysicsTelemetry& Owner;int32 PhaseIndex;bool Raw;double Started;
  FGuardTimer(FPhysicsTelemetry& In,bool InRaw):Owner(In),PhaseIndex(In.Current),Raw(InRaw),Started(FPlatformTime::Seconds())
  {if(Raw){++Owner.Samples[PhaseIndex].RawCalls;}else{++Owner.Samples[PhaseIndex].CheckCalls;}}
  ~FGuardTimer(){const double Elapsed=FPlatformTime::Seconds()-Started;if(Raw){Owner.Samples[PhaseIndex].RawSeconds+=Elapsed;}else{Owner.Samples[PhaseIndex].CheckSeconds+=Elapsed;}}
 };
 ~FPhysicsTelemetry()
 {
  const double Finished=FPlatformTime::Seconds();ClosePhase(Finished);auto Summary=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Phases;
  double Checks=0,Raws=0;int64 CheckCalls=0,RawCalls=0;
  for(int32 P=0;P<PhaseCount;++P)
  {
   const auto& S=Samples[P];auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),PhaseName(P));Row->SetNumberField(TEXT("entries"),static_cast<double>(S.Entries));Row->SetNumberField(TEXT("wall_seconds"),S.Seconds);
   Row->SetNumberField(TEXT("rebuild_check_calls"),static_cast<double>(S.CheckCalls));Row->SetNumberField(TEXT("rebuild_check_seconds_inclusive"),S.CheckSeconds);Row->SetNumberField(TEXT("rebuild_raw_calls"),static_cast<double>(S.RawCalls));Row->SetNumberField(TEXT("rebuild_raw_seconds_inclusive"),S.RawSeconds);
   auto Work=MakeShared<FJsonObject>();for(int32 I=0;I<WorkCount;++I){Work->SetNumberField(WorkName(I),S.Work[I]);}Row->SetObjectField(TEXT("explicit_work_counts"),Work);Phases.Add(MakeShared<FJsonValueObject>(Row));Checks+=S.CheckSeconds;Raws+=S.RawSeconds;CheckCalls+=S.CheckCalls;RawCalls+=S.RawCalls;
  }
  Summary->SetNumberField(TEXT("schema_version"),1);Summary->SetStringField(TEXT("clock"),TEXT("FPlatformTime::Seconds wall time on the existing synchronous GameThread path"));Summary->SetNumberField(TEXT("wall_seconds_before_telemetry_json"),Finished-Started);Summary->SetArrayField(TEXT("phases"),Phases);
  Summary->SetNumberField(TEXT("rebuild_check_calls"),static_cast<double>(CheckCalls));Summary->SetNumberField(TEXT("rebuild_check_seconds_inclusive"),Checks);Summary->SetNumberField(TEXT("rebuild_raw_calls"),static_cast<double>(RawCalls));Summary->SetNumberField(TEXT("rebuild_raw_seconds_inclusive"),Raws);
  Summary->SetBoolField(TEXT("guard_times_are_nested_inclusive"),true);Summary->SetBoolField(TEXT("guard_times_overlap_phase_times"),true);Summary->SetBoolField(TEXT("timing_is_exclusive_sdk_cost"),false);Summary->SetBoolField(TEXT("finalized_on_return_scope_exit"),true);
  Summary->SetStringField(TEXT("measurement_scope"),TEXT("RAII covers normal/early returns including local scope unwind before this destructor. Phase walls include guards/observation/JSON work. Check includes nested Raw and disk/property work; do not sum Check+Raw+phase times. Own final telemetry JSON encoding and later Native/Python/wire work are excluded. Instrumentation overhead is present. Counts reuse explicit adapter counters, not internal SDK/delegate work; unentered phases have entries=0."));
  Data->SetObjectField(TEXT("physics_telemetry"),Summary);
 }
};
struct FRebuildScope;
bool RebuildReadAdmission(FRebuildScope* Scope);
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool Text(const FString& V,int32 Max) { if(V.Len()>Max) { return false; }for(int32 I=0;I<V.Len();++I) { if(V[I]==TCHAR('\0')) { return false; } }return true; }
bool Fail(FResult& R,const TCHAR* Code,const FString& E)
{
 R.bSuccess=false;
 if(R.Data->HasTypedField<EJson::Boolean>(TEXT("in_place_rebuild"))&&R.Data->GetBoolField(TEXT("in_place_rebuild"))&&R.Data->HasTypedField<EJson::Object>(TEXT("original_result"))){R.Data->GetObjectField(TEXT("original_result"))->SetBoolField(TEXT("success"),false);}
 if(R.Data->HasTypedField<EJson::Boolean>(TEXT("rebuild_guard_stopped"))&&R.Data->GetBoolField(TEXT("rebuild_guard_stopped"))){return false;}
 R.ErrorCode=Code;R.Error=E;return false;
}
void Count(FResult& R,const TCHAR* K,int32 N=1) { R.Data->SetNumberField(K,R.Data->GetNumberField(K)+N); }
ECollisionEnabled::Type CollisionEnabled(FResult& R,const FBodyInstance& B) { Count(R,TEXT("collision_enabled_getter_calls"));return B.GetCollisionEnabled(false); }
bool PairEnabled(FResult& R,const UPhysicsAsset* A,int32 X,int32 Y) { Count(R,TEXT("collision_pair_getter_calls"));return A->IsCollisionEnabled(X,Y); }
void Projection(FResult& R,const FConstraintInstance& C,float& LA,float& AA,float& LT,float& AT) { Count(R,TEXT("projection_getter_calls"));C.GetProjectionParams(LA,AA,LT,AT); }
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


bool Same(const TSharedPtr<FJsonObject>& A,const TSharedPtr<FJsonObject>& B) { return Equal(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B)); }
bool Dependency(const FString& V)
{ FString S=V.TrimStartAndEnd();S.ReplaceInline(TEXT("\""),TEXT(""));S.ReplaceInline(TEXT("'"),TEXT(""));S.ReplaceInline(TEXT("\\"),TEXT("/"));S=S.ToLower();for(const TCHAR* T:{TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("cubelesscontrolrigtools")}) { if(S.Contains(T)) { return true; } }return false; }
bool Name(const FString& V,FName& Out,FString& Error,bool Object=false)
{
 if(V.IsEmpty()) { Error=TEXT("Name must not be empty");return false; }if(V.Len()>=NAME_SIZE||!Text(V,NAME_SIZE-1)) { Error=TEXT("Name must fit Unreal's name budget and contain no interior NUL");return false; }
 if(Object) { FText Reason;if(!FName::IsValidXName(V,INVALID_OBJECTNAME_CHARACTERS,&Reason)) { Error=Reason.ToString();return false; } }
 Out=FName(*V);if(Out.IsNone()) { Error=FString::Printf(TEXT("Invalid name '%s': name resolves to None"),*V);return false; }return true;
}
bool Disk(const FString& Package,const TSharedRef<FJsonObject>& Out)
{
 FString F;if(!FPackageName::TryConvertLongPackageNameToFilename(Package,F,FPackageName::GetAssetPackageExtension())) { return false; }
 const int64 N=IFileManager::Get().FileSize(*F);Out->SetStringField(TEXT("path"),F);Out->SetBoolField(TEXT("exists"),N>=0);Out->SetNumberField(TEXT("bytes"),N<0?0:N);if(N<0) { Out->SetStringField(TEXT("sha1"),TEXT(""));return true; }if(N>134217728) { return false; }
 TArray<uint8>B;if(!FFileHelper::LoadFileToArray(B,*F)||B.Num()!=N) { return false; }FSHAHash H;FSHA1::HashBuffer(B.GetData(),B.Num(),H.Hash);Out->SetStringField(TEXT("sha1"),H.ToString());return true;
}
bool Numbers(const TSharedRef<FJsonObject>& J,const TCHAR* K,std::initializer_list<double> Values)
{ TArray<TSharedPtr<FJsonValue>> A;for(double V:Values) { if(!FMath::IsFinite(V)) { return false; }A.Add(MakeShared<FJsonValueNumber>(V)); }J->SetArrayField(K,A);return true; }
bool ObservePhysics(UPhysicsAsset* PA,const TSharedRef<FJsonObject>& J,FResult& R,FRebuildScope* Rebuild=nullptr)
{
 if(!RebuildReadAdmission(Rebuild)){return false;}J->SetBoolField(TEXT("complete"),false);J->SetBoolField(TEXT("exists"),PA!=nullptr);if(!PA) { J->SetNumberField(TEXT("body_count"),0);J->SetNumberField(TEXT("constraint_count"),0);J->SetBoolField(TEXT("complete"),true);return true; }
 if(!Loaded(PA)||PA->GetClass()!=UPhysicsAsset::StaticClass()||PA->SkeletalBodySetups.Num()>128||PA->ConstraintSetup.Num()>127) { return false; }
 J->SetStringField(TEXT("path"),PA->GetPathName());J->SetStringField(TEXT("class"),PA->GetClass()->GetPathName());J->SetStringField(TEXT("outer"),PA->GetOuter()->GetPathName());J->SetStringField(TEXT("preview_mesh_soft_path"),PA->PreviewSkeletalMesh.ToSoftObjectPath().ToString());J->SetNumberField(TEXT("object_flags"),static_cast<uint32>(PA->GetFlags()));
 J->SetNumberField(TEXT("body_count"),PA->SkeletalBodySetups.Num());J->SetNumberField(TEXT("constraint_count"),PA->ConstraintSetup.Num());
 TArray<TSharedPtr<FJsonValue>> Bodies,Constraints,Pairs;J->SetArrayField(TEXT("bodies"),Bodies);J->SetArrayField(TEXT("constraints"),Constraints);J->SetArrayField(TEXT("collision_pairs"),Pairs);
 for(int32 I=0;I<PA->SkeletalBodySetups.Num();++I)
 {
  const auto* B=PA->SkeletalBodySetups[I].Get();auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),I);Row->SetBoolField(TEXT("null"),!B);Bodies.Add(MakeShared<FJsonValueObject>(Row));J->SetArrayField(TEXT("bodies"),Bodies);
  if(!Loaded(B)||B->GetClass()!=USkeletalBodySetup::StaticClass()||B->GetOuter()!=PA) { return false; }
  Row->SetStringField(TEXT("object_path"),B->GetPathName());Row->SetStringField(TEXT("owner_path"),B->GetOuter()->GetPathName());Row->SetStringField(TEXT("bone"),B->BoneName.ToString());Row->SetNumberField(TEXT("physics_type"),static_cast<int32>(B->PhysicsType.GetValue()));Row->SetNumberField(TEXT("collision_response"),static_cast<int32>(B->CollisionReponse.GetValue()));Row->SetNumberField(TEXT("shape_count"),B->AggGeom.GetElementCount());Row->SetNumberField(TEXT("capsule_count"),B->AggGeom.SphylElems.Num());
  Count(R,TEXT("collision_enabled_getter_calls"));const auto CurrentCollision=B->DefaultInstance.GetCollisionEnabled(false);if(!RebuildReadAdmission(Rebuild)){return false;}Row->SetNumberField(TEXT("collision_enabled"),static_cast<int32>(CurrentCollision));Row->SetBoolField(TEXT("start_awake"),B->DefaultInstance.bStartAwake);
  if(!Numbers(Row,TEXT("damping_and_mass_scale"),{B->DefaultInstance.LinearDamping,B->DefaultInstance.AngularDamping,B->DefaultInstance.MassScale})) { return false; }
 }
 for(int32 I=0;I<PA->ConstraintSetup.Num();++I)
 {
  const auto* C=PA->ConstraintSetup[I].Get();auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),I);Row->SetBoolField(TEXT("null"),!C);Constraints.Add(MakeShared<FJsonValueObject>(Row));J->SetArrayField(TEXT("constraints"),Constraints);
  if(!Loaded(C)||C->GetClass()!=UPhysicsConstraintTemplate::StaticClass()||C->GetOuter()!=PA) { return false; }
  const auto& V=C->DefaultInstance;Row->SetStringField(TEXT("object_path"),C->GetPathName());Row->SetStringField(TEXT("owner_path"),C->GetOuter()->GetPathName());Row->SetStringField(TEXT("joint"),V.JointName.ToString());Row->SetStringField(TEXT("child_bone"),V.ConstraintBone1.ToString());Row->SetStringField(TEXT("parent_bone"),V.ConstraintBone2.ToString());
  Row->SetBoolField(TEXT("collision_disabled"),V.IsCollisionDisabled());Row->SetBoolField(TEXT("projection_enabled"),V.IsProjectionEnabled());float LA=0,AA=0,LT=0,AT=0;Count(R,TEXT("projection_getter_calls"));V.GetProjectionParams(LA,AA,LT,AT);
  if(!Numbers(Row,TEXT("linear_motion"),{static_cast<double>(V.GetLinearXMotion()),static_cast<double>(V.GetLinearYMotion()),static_cast<double>(V.GetLinearZMotion())})||!Numbers(Row,TEXT("angular_motion"),{static_cast<double>(V.GetAngularSwing1Motion()),static_cast<double>(V.GetAngularSwing2Motion()),static_cast<double>(V.GetAngularTwistMotion())})||!Numbers(Row,TEXT("angular_limits"),{V.GetAngularSwing1Limit(),V.GetAngularSwing2Limit(),V.GetAngularTwistLimit()})||!Numbers(Row,TEXT("projection"),{LA,AA,LT,AT})) { return false; }
 }
 for(int32 A=0;A<PA->SkeletalBodySetups.Num();++A) { for(int32 B=A+1;B<PA->SkeletalBodySetups.Num();++B) { auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("body_a"),A);Row->SetNumberField(TEXT("body_b"),B);Count(R,TEXT("collision_pair_getter_calls"));Row->SetBoolField(TEXT("enabled"),PA->IsCollisionEnabled(A,B));Pairs.Add(MakeShared<FJsonValueObject>(Row));J->SetArrayField(TEXT("collision_pairs"),Pairs); } }
 if(!RebuildReadAdmission(Rebuild)){return false;}J->SetBoolField(TEXT("complete"),true);return true;
}
bool Capture(USkeletalMesh* Mesh,UPhysicsAsset* Target,const FString& TargetPath,FResult& R,const TCHAR* Phase,TSharedPtr<FJsonObject>& Out,FRebuildScope* Rebuild=nullptr)
{
 if(!RebuildReadAdmission(Rebuild)){return false;}Count(R,TEXT("observation_capture_calls"));auto J=MakeShared<FJsonObject>();Out=J;R.Data->SetObjectField(Phase,J);J->SetBoolField(TEXT("complete"),false);
 if(!Loaded(Mesh)||Mesh->GetClass()!=USkeletalMesh::StaticClass()||Mesh->IsCompiling()) { return false; }
 auto Source=MakeShared<FJsonObject>();J->SetObjectField(TEXT("source_mesh"),Source);Source->SetStringField(TEXT("path"),Mesh->GetPathName());Source->SetStringField(TEXT("class"),Mesh->GetClass()->GetPathName());Source->SetBoolField(TEXT("dirty"),Mesh->GetOutermost()->IsDirty());
 Count(R,TEXT("source_physics_asset_getter_calls"));auto* Assigned=Mesh->GetPhysicsAsset();if(!RebuildReadAdmission(Rebuild)){return false;}Source->SetStringField(TEXT("assigned_physics_asset"),Assigned?Assigned->GetPathName():FString());Count(R,TEXT("source_skeleton_getter_calls"));auto* Skeleton=Mesh->GetSkeleton();if(!RebuildReadAdmission(Rebuild)){return false;}Source->SetStringField(TEXT("skeleton"),Skeleton?Skeleton->GetPathName():FString());Source->SetBoolField(TEXT("skeleton_dirty"),Skeleton?Skeleton->GetOutermost()->IsDirty():false);
 const auto& Ref=Mesh->GetRefSkeleton();if(!RebuildReadAdmission(Rebuild)){return false;}const int32 N=Ref.GetNum();if(N<=0||N>128||Ref.GetRefBonePose().Num()!=N) { return false; }Source->SetNumberField(TEXT("final_bone_count"),N);Source->SetNumberField(TEXT("raw_bone_count"),Ref.GetRawBoneNum());
 TArray<TSharedPtr<FJsonValue>> Bones;Source->SetArrayField(TEXT("bones"),Bones);for(int32 I=0;I<N;++I)
 { const int32 Parent=Ref.GetParentIndex(I);if(Parent<INDEX_NONE||Parent>=I) { return false; }auto B=MakeShared<FJsonObject>();B->SetNumberField(TEXT("index"),I);B->SetStringField(TEXT("name"),Ref.GetBoneName(I).ToString());B->SetNumberField(TEXT("parent_index"),Parent);Bones.Add(MakeShared<FJsonValueObject>(B));Source->SetArrayField(TEXT("bones"),Bones);const auto& T=Ref.GetRefBonePose()[I];const auto P=T.GetTranslation(),S=T.GetScale3D();const auto Q=T.GetRotation();if(!Numbers(B,TEXT("translation"),{P.X,P.Y,P.Z})||!Numbers(B,TEXT("scale"),{S.X,S.Y,S.Z})||!Numbers(B,TEXT("quaternion"),{Q.X,Q.Y,Q.Z,Q.W})) { return false; } }
 auto SourceDisk=MakeShared<FJsonObject>();Source->SetObjectField(TEXT("disk"),SourceDisk);if(!Disk(Mesh->GetOutermost()->GetName(),SourceDisk)) { return false; }
 if(Skeleton) { if(!Loaded(Skeleton)) { return false; }auto D=MakeShared<FJsonObject>();Source->SetObjectField(TEXT("skeleton_disk"),D);if(!Disk(Skeleton->GetOutermost()->GetName(),D)) { return false; } }
 auto TargetState=MakeShared<FJsonObject>();J->SetObjectField(TEXT("target"),TargetState);if(!ObservePhysics(Target,TargetState,R,Rebuild)) { return false; }auto TargetDisk=MakeShared<FJsonObject>();J->SetObjectField(TEXT("target_disk"),TargetDisk);if(!Disk(TargetPath,TargetDisk)) { return false; }J->SetBoolField(TEXT("target_dirty"),Target?Target->GetOutermost()->IsDirty():false);
 auto Other=MakeShared<FJsonObject>();J->SetObjectField(TEXT("other_package_dirty"),Other);int32 NPackages=0;for(TObjectIterator<UPackage> It;It;++It) { if(!IsValid(*It)||(*It)->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)) { continue; }if(++NPackages>8192) { return false; }Other->SetBoolField((*It)->GetName(),(*It)->IsDirty()); }
 J->SetBoolField(TEXT("complete"),true);return true;
}
bool SourceStable(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,const FString& TargetPackage)
{
 if(!Same(Before->GetObjectField(TEXT("source_mesh")),After->GetObjectField(TEXT("source_mesh")))) { return false; }
 const auto A=Before->GetObjectField(TEXT("other_package_dirty")),B=After->GetObjectField(TEXT("other_package_dirty"));for(const auto& P:A->Values) { if(FString(P.Key.ToView()).Equals(TargetPackage,ESearchCase::CaseSensitive)) { continue; }const auto* V=B->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }
 for(const auto& P:B->Values) { if(!A->HasField(P.Key)&&!FString(P.Key.ToView()).Equals(TargetPackage,ESearchCase::CaseSensitive)&&(P.Value->Type!=EJson::Boolean||P.Value->AsBool())) { return false; } }return true;
}
TSharedRef<FJsonObject> LegacyState(UPhysicsAsset* PA,const FString& Path)
{ auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("target_physics_asset_path"),Path);J->SetBoolField(TEXT("target_exists"),PA!=nullptr);J->SetNumberField(TEXT("body_count"),PA?PA->SkeletalBodySetups.Num():0);J->SetNumberField(TEXT("constraint_count"),PA?PA->ConstraintSetup.Num():0);if(PA) { J->SetStringField(TEXT("physics_asset"),PA->GetPathName()); }return J; }
void StampLegacy(const TSharedRef<FJsonObject>& Legacy,bool Dry,const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,int32 Bodies,int32 Constraints)
{
 Legacy->SetStringField(TEXT("contract_version"),TEXT("mutation_result_v1"));Legacy->SetStringField(TEXT("mode"),Dry?TEXT("dry_run"):TEXT("apply"));Legacy->SetBoolField(TEXT("dry_run"),Dry);Legacy->SetBoolField(TEXT("DRY_RUN"),Dry);Legacy->SetBoolField(TEXT("changed"),!Dry);Legacy->SetBoolField(TEXT("mutation_performed"),!Dry);
 auto Mutation=MakeShared<FJsonObject>();Mutation->SetBoolField(TEXT("attempted"),!Dry);Mutation->SetBoolField(TEXT("performed"),!Dry);Mutation->SetStringField(TEXT("state"),Dry?TEXT("not_performed"):TEXT("performed"));Legacy->SetObjectField(TEXT("mutation"),Mutation);Legacy->SetObjectField(TEXT("before"),Before);Legacy->SetObjectField(TEXT("after"),After);
 auto Delta=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("affected_count"),TEXT("created_count"),TEXT("updated_count"),TEXT("removed_count"),TEXT("skipped_count"),TEXT("failed_count")}) { Delta->SetNumberField(K,0); }if(!Dry) { Delta->SetNumberField(TEXT("affected_count"),1);Delta->SetNumberField(TEXT("created_count"),1); }Delta->SetNumberField(TEXT("body_count_before"),0);Delta->SetNumberField(TEXT("body_count_after"),Dry?0:Bodies);Delta->SetNumberField(TEXT("constraint_count_before"),0);Delta->SetNumberField(TEXT("constraint_count_after"),Dry?0:Constraints);Legacy->SetObjectField(TEXT("delta"),Delta);
 auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("status"),TEXT("passed"));V->SetStringField(TEXT("method"),Dry?TEXT("validated_preflight_and_existing_target_readback"):TEXT("transient_scratch_generation_and_persistent_independent_readback"));V->SetBoolField(TEXT("before_observed"),true);V->SetBoolField(TEXT("after_observed"),true);V->SetBoolField(TEXT("strict"),true);
 TArray<TSharedPtr<FJsonValue>> Checks;if(Dry) { for(const TCHAR* K:{TEXT("input_names_validated_before_fname_construction"),TEXT("reference_skeleton_order_verified"),TEXT("existing_target_state_observed"),TEXT("no_asset_mutation_performed")}) { Checks.Add(MakeShared<FJsonValueString>(K)); } }
 else { for(const TCHAR* K:{TEXT("scratch_topology_and_subobjects_verified_before_commit"),TEXT("body_name_physics_collision_damping_mass_and_shape_verified"),TEXT("constraint_hierarchy_motions_limits_collision_and_projection_verified"),TEXT("persistent_subobjects_and_collision_table_verified_after_commit")}) { Checks.Add(MakeShared<FJsonValueString>(K)); }Checks.Add(MakeShared<FJsonValueString>(Legacy->GetBoolField(TEXT("save_requested"))?TEXT("target_asset_saved_after_postcondition"):TEXT("save_not_requested"))); }
 V->SetArrayField(TEXT("checks"),Checks);Legacy->SetObjectField(TEXT("verification"),V);Legacy->SetBoolField(TEXT("verified_success"),true);Legacy->SetStringField(TEXT("success_semantics"),Dry?TEXT("verified_dry_run"):TEXT("verified_postcondition"));
}

// IM: concrete existing-target rebuild scope; fresh creation keeps its original route.
struct FRebuildScope
{
 FResult& R;FPhysicsTelemetry& Telemetry;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TSharedPtr<FJsonObject> DiskBefore;bool DiskMayChange=false;
 TStrongObjectPtr<UPhysicsAsset> Target,Scratch;TStrongObjectPtr<USkeletalMesh> Mesh,PreviousPreview;
 TArray<TObjectPtr<USkeletalBodySetup>> ScratchBodies;TArray<TObjectPtr<UPhysicsConstraintTemplate>> ScratchConstraints;
 TStrongObjectPtr<UPackage> Package;bool Stopped=false,Committed=false,DirtyBefore=false;
 TArray<TObjectPtr<USkeletalBodySetup>> OldBodies,ExpectedBodies;
 TArray<TObjectPtr<UPhysicsConstraintTemplate>> OldConstraints,ExpectedConstraints;
 TMap<FRigidBodyIndexPair,bool> OldCollision,ExpectedCollision;TMap<FName,int32> OldBodyIndex,ExpectedBodyIndex;
 TArray<int32> OldBounds,ExpectedBounds;TSoftObjectPtr<USkeletalMesh> OldPreview,ExpectedPreview;
 struct FIdentity
 {
  TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;
 };
 TArray<FIdentity> Identities;TMap<UObject*,TSharedPtr<FJsonObject>> FieldsBefore;
 explicit FRebuildScope(FResult& In,FPhysicsTelemetry& InTelemetry):R(In),Telemetry(InTelemetry) {}
 bool Safe() const
 {
  return IsEditorGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()
   &&GEditor==Editor.Get()&&!IsPlaySessionActive()&&World.IsValid()&&GEditor->GetEditorWorldContext().World()==World.Get()&&!World->bIsTearingDown;
 }
 bool Stop(const TCHAR* Code,const FString& Error)
 {
  if(!Stopped){Stopped=true;Fail(R,Code,Error);R.bPostconditionPassed=false;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("rebuild_guard_stopped"),true);R.Data->SetBoolField(TEXT("automatic_retry_safe"),false);if(R.Data->HasTypedField<EJson::Object>(TEXT("original_result"))){R.Data->GetObjectField(TEXT("original_result"))->SetBoolField(TEXT("success"),false);}}
  return false;
 }
 bool Pin(UObject* O)
 {
  if(!Loaded(O)){return Stop(TEXT("skeletal_physics_rebuild_identity_invalid"),TEXT("Existing rebuild requires fully loaded live owned identities."));}
  for(const FIdentity& I:Identities){if(I.Object.Get()==O){return true;}}
  if(Identities.Num()>=1024){return Stop(TEXT("skeletal_physics_rebuild_identity_bound"),TEXT("Existing target ownership exceeds the finite identity bound."));}
  FIdentity I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();Identities.Add(MoveTemp(I));return true;
 }
 bool CollisionMatches() const
 {
  if(Target->CollisionDisableTable.Num()!=ExpectedCollision.Num()){return false;}
  for(const auto& Pair:ExpectedCollision){const bool* Value=Target->CollisionDisableTable.Find(Pair.Key);if(!Value||*Value!=Pair.Value){return false;}}
  return true;
 }
 bool BodyIndexMatches() const
 {
  if(Target->BodySetupIndexMap.Num()!=ExpectedBodyIndex.Num()){return false;}
  for(const auto& Pair:ExpectedBodyIndex){const int32* Value=Target->BodySetupIndexMap.Find(Pair.Key);if(!Value||*Value!=Pair.Value){return false;}}
  return true;
 }
 bool Raw()
 {
  FPhysicsTelemetry::FGuardTimer Cost(Telemetry,true);
  if(Stopped){return false;}if(!Safe()){return Stop(TEXT("skeletal_physics_rebuild_context_changed"),TEXT("Captured Editor/world or GT/PIE/GC/exit/save context changed; no next SDK, restore or cleanup."));}
  for(const FIdentity& I:Identities)
  {
   UObject* O=I.Object.Get();const TCHAR* Reason=nullptr;FString CurrentPath;uint32 CurrentFlags=0;bool FlagsObserved=false,PathObserved=false;
   // Preserve the original admission/read order. Earlier failures never query current path/flags.
   if(!Loaded(O)){Reason=TEXT("object_not_live");}
   else if(!Loaded(I.Class)){Reason=TEXT("captured_class_not_live");}
   else if(I.Outer&&!Loaded(I.Outer)){Reason=TEXT("captured_outer_not_live");}
   else if(!Loaded(I.Package)){Reason=TEXT("captured_package_not_live");}
   else if(O->GetClass()!=I.Class){Reason=TEXT("class_changed");}
   else if(O->GetOuter()!=I.Outer){Reason=TEXT("outer_changed");}
   else if(O->GetPackage()!=I.Package){Reason=TEXT("package_changed");}
   else
   {
    CurrentPath=O->GetPathName();PathObserved=true;
    if(CurrentPath!=I.Path){Reason=TEXT("path_changed");}
    else {CurrentFlags=static_cast<uint32>(O->GetFlags());FlagsObserved=true;if(CurrentFlags!=static_cast<uint32>(I.Flags)){Reason=TEXT("flags_changed");}}
   }
   if(Reason)
   {
    if(!R.Data->HasField(TEXT("first_rebuild_identity_failure")))
    {
     auto Failure=MakeShared<FJsonObject>();Failure->SetStringField(TEXT("reason"),Reason);Failure->SetStringField(TEXT("stage"),R.Stage.Left(256));
     Failure->SetStringField(TEXT("expected_path"),I.Path.Left(1024));Failure->SetNumberField(TEXT("expected_flags"),static_cast<uint32>(I.Flags));
     Failure->SetBoolField(TEXT("selected_target"),O==Target.Get());Failure->SetBoolField(TEXT("selected_package"),O==Package.Get());
     Failure->SetBoolField(TEXT("current_path_observed"),PathObserved);Failure->SetStringField(TEXT("current_path"),CurrentPath.Left(1024));
     Failure->SetBoolField(TEXT("current_flags_observed"),FlagsObserved);if(FlagsObserved){Failure->SetNumberField(TEXT("current_flags"),CurrentFlags);}
     Failure->SetStringField(TEXT("observation_scope"),TEXT("Original Raw admission only; unavailable current values are not queried after identity/lifetime failure."));
     R.Data->SetObjectField(TEXT("first_rebuild_identity_failure"),Failure);
    }
    return Stop(TEXT("skeletal_physics_rebuild_identity_changed"),TEXT("An existing captured identity/owner/package/path/flags changed; retained partial state is not repaired."));
   }
  }
  if(!Loaded(Target.Get())||!Loaded(Mesh.Get())||!Loaded(Package.Get())||Target->GetPackage()!=Package.Get()
   ||Target->SkeletalBodySetups!=ExpectedBodies||Target->ConstraintSetup!=ExpectedConstraints
   ||!CollisionMatches()||!BodyIndexMatches()||Target->BoundsBodies!=ExpectedBounds
   ||Target->PreviewSkeletalMesh.ToSoftObjectPath()!=ExpectedPreview.ToSoftObjectPath())
  {return Stop(TEXT("skeletal_physics_rebuild_role_changed"),TEXT("Existing target arrays/collision/bounds/preview no longer match the current planned role."));}
  if(Scratch.IsValid()&&(!Loaded(Scratch.Get())||Scratch->SkeletalBodySetups!=ScratchBodies||Scratch->ConstraintSetup!=ScratchConstraints)){return Stop(TEXT("skeletal_physics_rebuild_scratch_role_changed"),TEXT("Scratch generation body/constraint roles changed across a callback; no stale scratch writer."));}
  return true;
 }
 // Called only immediately after selected existing Target->Modify(), before target-array commit.
 // UObject::Modify -> SaveToTransactionBuffer -> MarkPackageDirty -> UPackage::SetDirtyFlag may add
 // RF_Transactional to this exact clean package under the same active GUndo. No other flags/objects are admitted.
 bool AfterSelectedTargetModify(bool PackageWasDirty,ITransaction* UndoBefore,bool SavedToTransaction)
 {
  if(Stopped||!Safe()){return Stop(TEXT("skeletal_physics_rebuild_modify_context_changed"),TEXT("Selected Modify returned after context loss; no flag adoption or next writer."));}
  FIdentity* CapturedPackage=nullptr;for(FIdentity& I:Identities){if(I.Object.Get()==Package.Get()){CapturedPackage=&I;break;}}
  if(!CapturedPackage||!Loaded(Package.Get())||!Loaded(Target.Get())){return Check();}
  auto* O=Package.Get();const FIdentity& I=*CapturedPackage;
  // No current dirty read or flag admission if this selected package's captured role changed.
  if(!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer
   ||O->GetPackage()!=I.Package||O->GetPathName()!=I.Path||Target->GetPackage()!=O){return Check();}
  const uint32 BeforeFlags=static_cast<uint32>(I.Flags),AfterFlags=static_cast<uint32>(O->GetFlags());
  if(BeforeFlags==AfterFlags){return Check();}
  const uint32 Transactional=static_cast<uint32>(RF_Transactional);
  if(PackageWasDirty||!UndoBefore||GUndo!=UndoBefore||!SavedToTransaction||!O->IsDirty()
   ||(BeforeFlags&Transactional)!=0||AfterFlags!=(BeforeFlags|Transactional)||O->HasAnyFlags(RF_Transient)
   ||O->HasAnyPackageFlags(PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn)||!Target->HasAnyFlags(RF_Transactional))
  {return Check();} // Original strict guard records/rejects every nonmatching transition.
  const EObjectFlags Original=CapturedPackage->Flags;CapturedPackage->Flags=O->GetFlags();
  // Only a local expectation is tentatively changed. All other exact identities/current roles, stored fields
  // and target disk proof are still required. Unsafe failures never restore/replay any SDK work.
  if(!Check()){CapturedPackage->Flags=Original;return false;}
  R.Data->SetBoolField(TEXT("modify_package_transactional_flag_adopted"),true);
  R.Data->SetStringField(TEXT("modify_package_transactional_path"),I.Path.Left(1024));
  R.Data->SetNumberField(TEXT("modify_package_flags_before"),BeforeFlags);R.Data->SetNumberField(TEXT("modify_package_flags_after"),AfterFlags);
  R.Data->SetBoolField(TEXT("modify_package_dirty_before"),PackageWasDirty);R.Data->SetBoolField(TEXT("modify_package_dirty_after"),true);
  R.Data->SetStringField(TEXT("modify_package_flag_scope"),TEXT("Exact selected package, clean-to-dirty, same active transaction, selected Modify returned true; only RF_Transactional addition. Original actual failure object/flags were not observed."));
  return true;
 }
 bool AdoptSaveLoadedFlags()
 {
  if(Stopped||!Safe()){return Stop(TEXT("skeletal_physics_rebuild_save_context_changed"),TEXT("Save returned after context loss; saved receipt retained without identity adoption."));}
  int32 Adopted=0;
  for(FIdentity& I:Identities)
  {
   UObject* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||(I.Outer&&!Loaded(I.Outer))||!Loaded(I.Package)
    ||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetPathName()!=I.Path)
   {return Stop(TEXT("skeletal_physics_rebuild_identity_changed"),TEXT("Save returned with changed identity/owner/package/path; no flag-only admission."));}
   const uint32 BeforeFlags=static_cast<uint32>(I.Flags),CurrentFlags=static_cast<uint32>(O->GetFlags());
   if(CurrentFlags==BeforeFlags){continue;}
   const uint32 Added=CurrentFlags&~BeforeFlags,Allowed=static_cast<uint32>(RF_WasLoaded|RF_LoadCompleted);
   if((CurrentFlags&BeforeFlags)!=BeforeFlags||(Added&~Allowed)!=0||O->HasAnyFlags(RF_Transient)
    ||I.Package!=Package.Get()||O==Mesh.Get()||O==PreviousPreview.Get()
    ||!(O==Target.Get()||O==Package.Get()||O->IsIn(Target.Get())))
   {return Stop(TEXT("skeletal_physics_rebuild_save_flags_changed"),TEXT("Only added save-export loaded flags on current target-package owned identities may be adopted; all removals or other changes remain unknown."));}
   I.Flags=O->GetFlags();++Adopted;
  }
  R.Data->SetNumberField(TEXT("save_loaded_flag_identity_adoptions"),Adopted);
  R.Data->SetBoolField(TEXT("save_loaded_flags_scoped_admission"),true);return Check();
 }
 bool Fields(UObject* O,TSharedRef<FJsonObject>& State,int32& Total)
 {
  if(!Raw()){return false;}
  for(TFieldIterator<FProperty> It(O->GetClass());It;++It)
  {
   const FProperty* P=*It;if(P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient)){continue;}
   const FName Name=P->GetFName();
   if(O==Target.Get()&&(Name==TEXT("SkeletalBodySetups")||Name==TEXT("ConstraintSetup")||Name==TEXT("CollisionDisableTable")||Name==TEXT("BoundsBodies")||Name==TEXT("PreviewSkeletalMesh")||Name==TEXT("BodySetupIndexMap"))){continue;}
   if(P->ArrayDim<=0||P->ArrayDim>128){return Stop(TEXT("skeletal_physics_rebuild_fields_bound"),TEXT("Stored property array exceeds bounded prior-state observation."));}
   for(int32 Index=0;Index<P->ArrayDim;++Index)
   {
    if(!Raw()){return false;}FString Value;const void* Address=P->ContainerPtrToValuePtr<void>(O,Index);
    const bool Exported=P->ExportText_Direct(Value,Address,Address,O,PPF_None);if(!Raw()){return false;}
    if(!Exported||Value.Len()>1048576||(Total+=Value.Len())>8388608){return Stop(TEXT("skeletal_physics_rebuild_fields_incomplete"),TEXT("Complete stored prior target/subobject state exceeds its finite observation scope."));}
    State->SetStringField(P->GetPathName()+FString::Printf(TEXT("[%d]"),Index),Value);
   }
  }
  return Raw();
 }
 bool Check()
 {
  FPhysicsTelemetry::FGuardTimer Cost(Telemetry,false);
  if(!Raw()){return false;}int32 Total=0;
  for(const auto& Pair:FieldsBefore)
  {
   auto State=MakeShared<FJsonObject>();if(!Fields(Pair.Key,State,Total)){return false;}
   if(!Same(State,Pair.Value)){return Stop(TEXT("skeletal_physics_rebuild_prior_fields_changed"),TEXT("Stored prior subobject or nonselected target fields changed across SDK work; no next writer or unsafe restore."));}
  }
  if(!DiskMayChange&&DiskBefore.IsValid()){auto CurrentDisk=MakeShared<FJsonObject>();if(!Disk(Package->GetName(),CurrentDisk)||!Same(DiskBefore,CurrentDisk)){return Stop(TEXT("skeletal_physics_rebuild_disk_changed"),TEXT("Target disk changed before explicit save; no implicit disk restoration is claimed."));}}
  return Raw();
 }
 bool Bind(UPhysicsAsset* Existing,USkeletalMesh* Source)
 {
  Editor=GEditor;if(!IsEditorGameThread()||!GEditor||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()){return Stop(TEXT("skeletal_physics_rebuild_editor_required"),TEXT("Safe Editor context required before existing-target observation."));}
  World=GEditor->GetEditorWorldContext().World();if(!Safe()||!Loaded(Existing)||Existing->GetClass()!=UPhysicsAsset::StaticClass()||!Loaded(Source)
   ||Existing->GetOuter()!=Existing->GetPackage()||Existing->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor)
   ||Existing->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject))
  {return Stop(TEXT("skeletal_physics_rebuild_target_unsupported"),TEXT("Exact fully loaded runtime /Game PhysicsAsset and Editor world required for in-place rebuild."));}
  Target.Reset(Existing);Mesh.Reset(Source);Package.Reset(Existing->GetPackage());DirtyBefore=Package->IsDirty();
  OldBodies=ExpectedBodies=Existing->SkeletalBodySetups;OldConstraints=ExpectedConstraints=Existing->ConstraintSetup;
  OldCollision=ExpectedCollision=Existing->CollisionDisableTable;OldBodyIndex=ExpectedBodyIndex=Existing->BodySetupIndexMap;OldBounds=ExpectedBounds=Existing->BoundsBodies;
  OldPreview=ExpectedPreview=Existing->PreviewSkeletalMesh;
  if(!Pin(Existing)||!Pin(Package.Get())||!Pin(Source)||!Pin(Source->GetPackage())){return false;}
  TArray<UObject*> Owned;GetObjectsWithOuter(Existing,Owned,EGetObjectsFlags::IncludeNestedObjects);if(!Safe()){return Stop(TEXT("skeletal_physics_rebuild_context_changed"),TEXT("Context changed during existing ownership observation."));}
  if(Owned.Num()>512){return Stop(TEXT("skeletal_physics_rebuild_identity_bound"),TEXT("Existing target subtree exceeds512 owned objects."));}
  for(UObject* O:Owned){if(!Pin(O)){return false;}}
  if(!Raw()){return false;}
  TMap<FName,int32> DerivedOldIndex;for(int32 Index=0;Index<OldBodies.Num();++Index){if(!Loaded(OldBodies[Index].Get())){return Stop(TEXT("skeletal_physics_rebuild_body_index_incomplete"),TEXT("Existing body index cannot be verified before rebuild."));}DerivedOldIndex.Add(OldBodies[Index]->BoneName,Index);}
  if(DerivedOldIndex.Num()!=OldBodyIndex.Num()){return Stop(TEXT("skeletal_physics_rebuild_body_index_incomplete"),TEXT("Existing body index map must match its current body array before rebuild."));}
  for(const auto& Pair:DerivedOldIndex){const int32* Value=OldBodyIndex.Find(Pair.Key);if(!Value||*Value!=Pair.Value){return Stop(TEXT("skeletal_physics_rebuild_body_index_incomplete"),TEXT("Existing body index map differs from its current body roles."));}}
  int32 Total=0;
  for(UObject* O:Owned){auto State=MakeShared<FJsonObject>();if(!Fields(O,State,Total)){return false;}FieldsBefore.Add(O,State);}
  auto Header=MakeShared<FJsonObject>();if(!Fields(Existing,Header,Total)){return false;}FieldsBefore.Add(Existing,Header);
  auto InitialDisk=MakeShared<FJsonObject>();if(!Disk(Package->GetName(),InitialDisk)){return Stop(TEXT("skeletal_physics_rebuild_disk_incomplete"),TEXT("Existing target disk baseline is incomplete before generation."));}DiskBefore=InitialDisk;
  R.Data->SetBoolField(TEXT("in_place_rebuild"),true);R.Data->SetBoolField(TEXT("rebuild_previous_field_observation_complete"),true);
  R.Data->SetNumberField(TEXT("rebuild_previous_owned_object_count"),Owned.Num());R.Data->SetNumberField(TEXT("rebuild_previous_stored_text_units"),Total);
  R.Data->SetNumberField(TEXT("rebuild_body_count_before"),OldBodies.Num());R.Data->SetNumberField(TEXT("rebuild_constraint_count_before"),OldConstraints.Num());
  R.Data->SetBoolField(TEXT("rebuild_dirty_before"),DirtyBefore);R.Data->SetBoolField(TEXT("memory_restore_attempted"),false);
  R.Data->SetBoolField(TEXT("memory_selected_state_restored"),false);R.Data->SetBoolField(TEXT("disk_restore_performed"),false);
  R.Data->SetBoolField(TEXT("global_physics_world_cache_restoration_verified"),false);
  for(const TCHAR* Key:{TEXT("restore_body_index_calls"),TEXT("restore_refresh_calls"),TEXT("restore_preview_calls"),TEXT("restore_dirty_calls"),TEXT("rebuild_existing_preview_getter_calls")}){R.Data->SetNumberField(Key,0);}
  return Check();
 }
 bool AdoptScratch(UPhysicsAsset* Generated)
 {
  if(!Safe()||!Loaded(Generated)||Generated->GetClass()!=UPhysicsAsset::StaticClass()||Generated->GetOuter()!=GetTransientPackage())
  {return Stop(TEXT("skeletal_physics_rebuild_scratch_unavailable"),TEXT("Returned generated scratch must remain an exact live transient PhysicsAsset."));}
  Scratch.Reset(Generated);ScratchBodies=Generated->SkeletalBodySetups;ScratchConstraints=Generated->ConstraintSetup;
  if(!Pin(Generated)){return false;}TArray<UObject*> Objects;GetObjectsWithOuter(Generated,Objects,EGetObjectsFlags::IncludeNestedObjects);if(!Safe()||Objects.Num()>512){return Stop(TEXT("skeletal_physics_rebuild_scratch_bound"),TEXT("Scratch ownership observation is unsafe or exceeds512 objects."));}
  for(UObject* O:Objects){if(!Pin(O)){return false;}}return Check();
 }
 bool AdoptPrepared(UObject* O,UClass* ExpectedClass)
 {
  if(!Safe()||!Loaded(O)||O->GetClass()!=ExpectedClass||O->GetOuter()!=Target.Get()||O->GetPackage()!=Package.Get())
  {return Stop(TEXT("skeletal_physics_rebuild_duplicate_identity_invalid"),TEXT("Returned persistent duplicate lost its exact target ownership before adoption."));}
  if(!Pin(O)){return false;}TArray<UObject*> Nested;GetObjectsWithOuter(O,Nested,EGetObjectsFlags::IncludeNestedObjects);if(!Safe()||Nested.Num()>512){return Stop(TEXT("skeletal_physics_rebuild_duplicate_bound"),TEXT("Prepared duplicate ownership observation is unsafe or incomplete."));}
  for(UObject* Child:Nested){if(!Pin(Child)){return false;}}return Check();
 }
 bool CapturePreview()
 {
  if(!Check()){return false;}Count(R,TEXT("rebuild_existing_preview_getter_calls"));USkeletalMesh* Preview=Target->GetPreviewMesh();
  if(!Safe()){return Stop(TEXT("skeletal_physics_rebuild_preview_context_changed"),TEXT("Previous preview getter returned after context loss; no returned UObject was adopted."));}
  if(Preview){if(!Loaded(Preview)||Preview->GetClass()!=USkeletalMesh::StaticClass()||Preview->GetPathName()!=OldPreview.ToSoftObjectPath().ToString()||Preview->IsCompiling()){return Stop(TEXT("skeletal_physics_rebuild_preview_unavailable"),TEXT("Existing preview must resolve exactly and be idle before any target writer."));}PreviousPreview.Reset(Preview);if(!Pin(Preview)){return false;}}
  else if(!OldPreview.IsNull()){return Stop(TEXT("skeletal_physics_rebuild_preview_unavailable"),TEXT("Original existing preview could not be resolved; exact restore admission remains incomplete."));}
  if(!Check()){return false;}
  if(Preview){for(const auto& B:OldBodies){if(!Check()||!Loaded(B)){return false;}const int32 BoneIndex=Preview->GetRefSkeleton().FindBoneIndex(B->BoneName);if(!Check()){return false;}if(BoneIndex==INDEX_NONE){return Stop(TEXT("skeletal_physics_rebuild_preview_bones_invalid"),TEXT("Old preview lacks an existing body bone; do not enter a modal restore setter."));}}}
  return Check();
 }
 bool CommitPreviewReady()
 {
  if(!Check()){return false;}
  const bool Compiling=Mesh->IsCompiling();if(!Raw()){return false;}
  if(Compiling){return Stop(TEXT("skeletal_physics_rebuild_commit_preview_busy"),TEXT("Source preview is currently compiling; do not enter the modal-capable commit setter."));}
  for(const auto& Body:ExpectedBodies)
  {
   if(!Raw()||!Loaded(Body.Get())){return false;}
   const int32 BoneIndex=Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName);if(!Raw()){return false;}
   if(BoneIndex==INDEX_NONE){return Stop(TEXT("skeletal_physics_rebuild_commit_preview_bones_changed"),TEXT("Source preview no longer contains a prepared body bone; no modal-capable commit setter."));}
  }
  return Raw();
 }
 bool PreviewRestoreReady()
 {
  if(!Check()){return false;}
  if(!PreviousPreview.IsValid()){return OldPreview.IsNull()||Stop(TEXT("skeletal_physics_rebuild_restore_preview_unavailable"),TEXT("Original nonnull preview is unavailable; no restore setter or selected-array writer."));}
  const bool Compiling=PreviousPreview->IsCompiling();if(!Raw()){return false;}
  if(Compiling){return Stop(TEXT("skeletal_physics_rebuild_restore_preview_busy"),TEXT("Previous preview is currently compiling; no modal-capable restore setter."));}
  for(const auto& Body:OldBodies)
  {
   if(!Raw()||!Loaded(Body.Get())){return false;}
   const int32 BoneIndex=PreviousPreview->GetRefSkeleton().FindBoneIndex(Body->BoneName);if(!Raw()){return false;}
   if(BoneIndex==INDEX_NONE){return Stop(TEXT("skeletal_physics_rebuild_restore_preview_bones_changed"),TEXT("Previous preview no longer contains an old body bone; selected restore stops before the modal-capable setter."));}
  }
  return Raw();
 }
 bool Restore()
 {
  if(!Committed){R.Data->SetBoolField(TEXT("memory_restore_not_needed_before_commit"),true);return false;}
  if(!PreviewRestoreReady()){R.Data->SetBoolField(TEXT("memory_restore_skipped_unsafe"),true);return false;}
  R.Data->SetBoolField(TEXT("memory_restore_attempted"),true);
  ExpectedBodies=OldBodies;ExpectedConstraints=OldConstraints;ExpectedCollision=OldCollision;ExpectedBounds=OldBounds;
  Target->SkeletalBodySetups=OldBodies;Target->ConstraintSetup=OldConstraints;Target->CollisionDisableTable=OldCollision;Target->BoundsBodies=OldBounds;
  if(!PreviewRestoreReady()){return false;}ExpectedPreview=OldPreview;Count(R,TEXT("restore_preview_calls"));Target->SetPreviewMesh(PreviousPreview.Get(),false);if(!Check()){return false;}
  ExpectedBodyIndex=OldBodyIndex;Count(R,TEXT("restore_body_index_calls"));Target->UpdateBodySetupIndexMap();if(!Check()){return false;}
  Count(R,TEXT("restore_refresh_calls"));Target->RefreshPhysicsAssetChange();if(!Check()){return false;}
  Count(R,TEXT("restore_dirty_calls"));Package->SetDirtyFlag(DirtyBefore);if(!Check()){return false;}
  const bool Restored=Package->IsDirty()==DirtyBefore;R.Data->SetBoolField(TEXT("memory_selected_state_restored"),Restored);
  R.Data->SetBoolField(TEXT("original_full_previous_state_restore_performed"),false);
  R.Data->SetStringField(TEXT("restore_scope"),TEXT("Original body/constraint arrays, collision map, bounds, preview and package dirty; old stored fields checked. New duplicate exports, transaction/world/physics caches and disk are not an atomic rollback."));
  R.bChangeStateKnown=false;return Restored;
 }
};

bool RebuildReadAdmission(FRebuildScope* Scope){return !Scope||Scope->Raw();}
void Project(FResult& R,const TSharedRef<FJsonObject>& Legacy) { for(const auto& P:Legacy->Values) { if(!FString(P.Key.ToView()).Equals(TEXT("success"),ESearchCase::CaseSensitive)) { R.Data->SetField(P.Key,P.Value); } } }
}
FResult Create(const FString& MeshPath,const FString& RequestedTarget,const FString& ExpectedBonesJson,const FString& RootBoneText,bool bRootKinematic,bool bDisableCollision,double RequestedSwing1,double RequestedSwing2,double RequestedTwist,double RequestedLinearDamping,double RequestedAngularDamping,double RequestedTipMassScale,bool bOverwriteExisting,bool bSave,bool bDryRun)
{
 FResult R;FPhysicsTelemetry Telemetry(R.Data);R.bSaveRequested=bSave;auto D=R.Data;D->SetStringField(TEXT("requested_skeletal_mesh_path"),MeshPath);D->SetStringField(TEXT("requested_target_path"),RequestedTarget);D->SetBoolField(TEXT("overwrite_existing_requested"),bOverwriteExisting);D->SetBoolField(TEXT("dry_run_requested"),bDryRun);D->SetBoolField(TEXT("target_mutation_attempted"),false);D->SetBoolField(TEXT("generator_creates_preview_world_and_physics_bodies"),true);D->SetBoolField(TEXT("full_asset_world_cache_gc_effects_observed"),false);D->SetBoolField(TEXT("atomic_rollback_available"),false);D->SetBoolField(TEXT("caller_automatic_retry"),false);D->SetBoolField(TEXT("source_set_physics_asset_dispatched"),false);D->SetStringField(TEXT("counter_scope"),TEXT("Explicit adapter call-sites, including the specific verifier; engine utility/delegate/internal physics work is not exhaustively instrumented."));
 for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("asset_load_count"),TEXT("observation_capture_calls"),TEXT("source_physics_asset_getter_calls"),TEXT("source_skeleton_getter_calls"),TEXT("render_resource_getter_calls"),TEXT("imported_model_getter_calls"),TEXT("constraint_permission_query_calls"),TEXT("scratch_allocation_attempts"),TEXT("scratch_allocation_returns"),TEXT("generator_attempts"),TEXT("generator_returns"),TEXT("body_modify_calls"),TEXT("body_collision_setter_calls"),TEXT("body_field_write_calls"),TEXT("pair_collision_setter_calls"),TEXT("constraint_modify_calls"),TEXT("constraint_setter_calls"),TEXT("default_profile_copy_calls"),TEXT("asset_validation_calls"),TEXT("package_create_calls"),TEXT("target_allocation_attempts"),TEXT("target_allocation_returns"),TEXT("body_duplicate_attempts"),TEXT("body_duplicate_returns"),TEXT("constraint_duplicate_attempts"),TEXT("constraint_duplicate_returns"),TEXT("target_modify_calls"),TEXT("target_commit_calls"),TEXT("body_index_update_calls"),TEXT("bounds_update_calls"),TEXT("refresh_physics_asset_calls"),TEXT("mark_package_dirty_calls"),TEXT("save_attempt_count"),TEXT("save_return_count"),TEXT("asset_registry_notification_attempts"),TEXT("asset_registry_notification_returns"),TEXT("target_cleanup_attempts"),TEXT("target_cleanup_returns"),TEXT("projection_getter_calls"),TEXT("collision_enabled_getter_calls"),TEXT("collision_pair_getter_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_post_edit_change_calls")}) { D->SetNumberField(K,0); }
 for(const TCHAR* K:{TEXT("asset_load_calls"),TEXT("save_calls"),TEXT("body_calls"),TEXT("constraint_calls"),TEXT("duplicate_calls")}) { D->SetArrayField(K,{}); }
 if(!IsEditorGameThread()||IsPlaySessionActive()||MeshPath.IsEmpty()||RequestedTarget.IsEmpty()||!Text(MeshPath,1024)||!Text(RequestedTarget,1024)||!Text(ExpectedBonesJson,65536)||!Text(RootBoneText,NAME_SIZE-1)) { Fail(R,TEXT("skeletal_physics_input_or_editor_invalid"),TEXT("Idle Editor and bounded literal inputs required; no call dispatched."));return R; }
 FString TargetPath=FPackageName::ExportTextPathToObjectPath(RequestedTarget).TrimStartAndEnd();TargetPath.TrimQuotesInline();if(TargetPath.Contains(TEXT("."))) { TargetPath=FPackageName::ObjectPathToPackageName(TargetPath); }if(!TargetPath.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(TargetPath)||Dependency(TargetPath)) { Fail(R,TEXT("skeletal_physics_target_invalid"),TEXT("Original dependency-free valid /Game target package required."));return R; }
 FName TargetName;FString Error;if(!Name(FPackageName::GetShortName(TargetPath),TargetName,Error,true)) { Fail(R,TEXT("skeletal_physics_name_invalid"),Error);return R; }
 for(double V:{RequestedSwing1,RequestedSwing2,RequestedTwist,RequestedLinearDamping,RequestedAngularDamping,RequestedTipMassScale}) { if(!FMath::IsFinite(V)||FMath::Abs(V)>TNumericLimits<float>::Max()) { Fail(R,TEXT("skeletal_physics_numeric_scope_unsupported"),TEXT("Finite float32-range values required before conversion; larger original numeric variants remain incomplete."));return R; } }
 const float Swing1Limit=FMath::Clamp(static_cast<float>(RequestedSwing1),0.0f,180.0f),Swing2Limit=FMath::Clamp(static_cast<float>(RequestedSwing2),0.0f,180.0f),TwistLimit=FMath::Clamp(static_cast<float>(RequestedTwist),0.0f,180.0f),LinearDamping=FMath::Max(static_cast<float>(RequestedLinearDamping),0.0f),AngularDamping=FMath::Max(static_cast<float>(RequestedAngularDamping),0.0f),TipMassScale=FMath::Max(static_cast<float>(RequestedTipMassScale),0.01f);
 auto Normalized=MakeShared<FJsonObject>();D->SetObjectField(TEXT("normalized_options"),Normalized);Numbers(Normalized,TEXT("limits_swing1_swing2_twist"),{Swing1Limit,Swing2Limit,TwistLimit});Numbers(Normalized,TEXT("damping_linear_angular_tip_mass_scale"),{LinearDamping,AngularDamping,TipMassScale});Normalized->SetBoolField(TEXT("root_kinematic"),bRootKinematic);Normalized->SetBoolField(TEXT("disable_collision"),bDisableCollision);
 Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Transaction(NSLOCTEXT("Cubeless","SkeletalPhysicsAsset","Create skeletal PhysicsAsset"));
 TArray<TSharedPtr<FJsonValue>> Loads;auto Load=[&](const FString& Path)->UObject* { Count(R,TEXT("asset_load_count"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_path"),Path);J->SetBoolField(TEXT("attempted"),true);J->SetBoolField(TEXT("returned"),false);Loads.Add(MakeShared<FJsonValueObject>(J));D->SetArrayField(TEXT("asset_load_calls"),Loads);auto* O=UEditorAssetLibrary::LoadAsset(Path);J->SetBoolField(TEXT("returned"),true);J->SetStringField(TEXT("object_path"),O?O->GetPathName():FString());return O; };
 R.Stage=TEXT("load_source_mesh");auto* MeshObject=Load(MeshPath);auto* SkeletalMesh=Cast<USkeletalMesh>(MeshObject);if(!Loaded(SkeletalMesh)||SkeletalMesh->GetClass()!=USkeletalMesh::StaticClass()||SkeletalMesh->IsCompiling()) { Fail(R,TEXT("skeletal_physics_source_scope_unsupported"),TEXT("Loaded idle exact native SkeletalMesh required; no explicit async wait."));return R; }TStrongObjectPtr<USkeletalMesh> KeepMesh(SkeletalMesh);
 const auto& ReferenceSkeleton=SkeletalMesh->GetRefSkeleton();const int32 N=ReferenceSkeleton.GetNum();if(N<=0||N>128||ReferenceSkeleton.GetRefBonePose().Num()!=N) { Fail(R,TEXT("skeletal_physics_topology_scope_unsupported"),TEXT("Bounded nonempty reference skeleton required."));return R; }
 TSharedPtr<FJsonObject> BoneEnvelope;TSharedPtr<FJsonValue> InputBones;const FString WrappedBones=FString(TEXT("{\"expected_bones\":"))+ExpectedBonesJson+TEXT("}");if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(WrappedBones),BoneEnvelope)||!BoneEnvelope.IsValid()||BoneEnvelope->Values.Num()!=1||!(InputBones=BoneEnvelope->TryGetField(TEXT("expected_bones"))).IsValid()||(InputBones->Type!=EJson::Null&&InputBones->Type!=EJson::Array)) { Fail(R,TEXT("skeletal_physics_expected_bones_invalid"),TEXT("Internal expected_bones must represent the original array/null."));return R; }
 TArray<FName> ExpectedBones;if(InputBones->Type==EJson::Array) { if(InputBones->AsArray().Num()>128) { Fail(R,TEXT("skeletal_physics_expected_bones_invalid"),TEXT("Expected bone list exceeds finite bounds."));return R; }for(const auto& V:InputBones->AsArray()) { FString Value;FName BN;if(!V.IsValid()||V->Type!=EJson::String||!V->TryGetString(Value)||Value.TrimStartAndEnd().IsEmpty()||!Name(Value,BN,Error)) { Fail(R,TEXT("skeletal_physics_expected_bones_invalid"),TEXT("expected_bones must contain non-empty bounded bone-name strings"));return R; }ExpectedBones.Add(BN); } }
 if(ExpectedBones.IsEmpty()) { for(int32 I=0;I<N;++I) { ExpectedBones.Add(ReferenceSkeleton.GetBoneName(I)); } }if(ExpectedBones.Num()!=N) { Fail(R,TEXT("skeletal_physics_expected_bones_mismatch"),TEXT("expected_bones count must match reference skeleton count"));return R; }for(int32 I=0;I<N;++I) { if(ExpectedBones[I]!=ReferenceSkeleton.GetBoneName(I)) { Fail(R,TEXT("skeletal_physics_expected_bones_mismatch"),TEXT("expected_bones must match reference-skeleton order"));return R; } }
 const FString KinematicRootBoneText=RootBoneText.IsEmpty()?ReferenceSkeleton.GetBoneName(0).ToString():RootBoneText;FName KinematicRootBone;if(!Name(KinematicRootBoneText,KinematicRootBone,Error)||ReferenceSkeleton.FindBoneIndex(KinematicRootBone)==INDEX_NONE) { Fail(R,TEXT("skeletal_physics_root_bone_invalid"),TEXT("kinematic_root_bone must be an existing bounded bone name"));return R; }
 R.Stage=TEXT("load_existing_target");auto* ExistingObject=Load(TargetPath);auto* ExistingPhysicsAsset=Cast<UPhysicsAsset>(ExistingObject);TStrongObjectPtr<UObject> KeepExisting(ExistingObject);if(ExistingObject&&!ExistingPhysicsAsset) { Fail(R,TEXT("skeletal_physics_target_type_conflict"),TEXT("Target path already contains a non-PhysicsAsset object"));return R; }
 if(ExistingPhysicsAsset&&!bOverwriteExisting) { Fail(R,TEXT("skeletal_physics_target_exists"),TEXT("PhysicsAsset already exists. Pass overwrite_existing=true to rebuild it in place."));return R; }TUniquePtr<FRebuildScope> Rebuild;
 if(ExistingPhysicsAsset)
 {
  if(!Loaded(ExistingPhysicsAsset)||ExistingPhysicsAsset->GetClass()!=UPhysicsAsset::StaticClass()||ExistingPhysicsAsset->GetPathName()!=TargetPath+TEXT(".")+TargetName.ToString()) { Fail(R,TEXT("skeletal_physics_rebuild_target_unsupported"),TEXT("Exact nonredirected loaded PhysicsAsset at the requested target is required."));return R; }
  Rebuild=MakeUnique<FRebuildScope>(R,Telemetry);if(!Rebuild->Bind(ExistingPhysicsAsset,SkeletalMesh)){return R;}
 }
 TSharedPtr<FJsonObject> Before;R.Stage=TEXT("before_observation");if(!Capture(SkeletalMesh,ExistingPhysicsAsset,TargetPath,R,TEXT("before_observation"),Before,Rebuild.Get())) { R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_physics_observation_incomplete"),TEXT("No generator or target writer dispatched; prestate observation incomplete."));return R; }
 if(Rebuild&&!Rebuild->Check()){return R;}
 if(!Rebuild&&(Before->GetObjectField(TEXT("target_disk"))->GetBoolField(TEXT("exists"))||FindPackage(nullptr,*TargetPath))) { Fail(R,TEXT("skeletal_physics_existing_package_scope_unsupported"),TEXT("Fresh target requires absent file and package, without adopting an unobserved existing package."));return R; }
 const auto LegacyBefore=LegacyState(ExistingPhysicsAsset,TargetPath);auto Legacy=MakeShared<FJsonObject>();D->SetObjectField(TEXT("original_result"),Legacy);Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("command"),TEXT("create_skeletal_physics_asset"));Legacy->SetStringField(TEXT("skeletal_mesh"),SkeletalMesh->GetPathName());Legacy->SetStringField(TEXT("target_physics_asset_path"),TargetPath);Legacy->SetStringField(TEXT("kinematic_root_bone"),KinematicRootBone.ToString());Legacy->SetNumberField(TEXT("expected_body_count"),N);Legacy->SetNumberField(TEXT("expected_constraint_count"),FMath::Max(0,N-1));Legacy->SetBoolField(TEXT("target_exists"),ExistingPhysicsAsset!=nullptr);Legacy->SetBoolField(TEXT("dry_run"),bDryRun);Legacy->SetBoolField(TEXT("source_mesh_modified"),false);Legacy->SetBoolField(TEXT("set_to_mesh"),false);
 if(bDryRun)
 { auto Planned=LegacyState(ExistingPhysicsAsset,TargetPath);Planned->SetBoolField(TEXT("target_exists"),true);Planned->SetNumberField(TEXT("body_count"),N);Planned->SetNumberField(TEXT("constraint_count"),FMath::Max(0,N-1));Planned->SetStringField(TEXT("action"),Rebuild?TEXT("rebuild_in_place"):TEXT("create"));Planned->SetNumberField(TEXT("planned_created_count"),Rebuild?0:1);Planned->SetNumberField(TEXT("planned_updated_count"),Rebuild?1:0);Legacy->SetBoolField(TEXT("would_create"),!Rebuild);Legacy->SetBoolField(TEXT("would_rebuild"),bool(Rebuild));Legacy->SetObjectField(TEXT("planned_after"),Planned);
  TSharedPtr<FJsonObject> After;R.Stage=TEXT("after_observation");if(!Capture(SkeletalMesh,ExistingPhysicsAsset,TargetPath,R,TEXT("after_observation"),After,Rebuild.Get())) { R.bChangeStateKnown=false;Fail(R,TEXT("skeletal_physics_observation_incomplete"),TEXT("Dry preflight current observation incomplete."));return R; }if(Rebuild&&!Rebuild->Check()){return R;}if(!Same(Before,After)) { R.bChanged=true;Fail(R,TEXT("skeletal_physics_dry_preservation_failed"),TEXT("Observed dry scope changed; no generator/save dispatched."));return R; }StampLegacy(Legacy,true,LegacyBefore,LegacyBefore,0,0);if(Rebuild){auto Delta=Legacy->GetObjectField(TEXT("delta"));Delta->SetNumberField(TEXT("body_count_before"),Rebuild->OldBodies.Num());Delta->SetNumberField(TEXT("body_count_after"),Rebuild->OldBodies.Num());Delta->SetNumberField(TEXT("constraint_count_before"),Rebuild->OldConstraints.Num());Delta->SetNumberField(TEXT("constraint_count_after"),Rebuild->OldConstraints.Num());}Project(R,Legacy);R.bSuccess=true;R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
 }
 if(ReferenceSkeleton.GetRawBoneNum()!=N) { Fail(R,TEXT("skeletal_physics_virtual_bones_scope_unsupported"),TEXT("Original final-bone predicate differs from raw generation; first apply slice requires raw==final, without weakening expected counts."));return R; }
 Count(R,TEXT("render_resource_getter_calls"));auto* Render=SkeletalMesh->GetResourceForRendering();Count(R,TEXT("imported_model_getter_calls"));auto* Imported=SkeletalMesh->GetImportedModel();if(!Render||Render->LODRenderData.IsEmpty()||!Imported||Imported->LODModels.Num()==0||Render->LODRenderData[0].GetNumVertices()>500000) { Fail(R,TEXT("skeletal_physics_render_scope_unsupported"),TEXT("Usable bounded current render/imported LOD0 required before the public checked generator."));return R; }
 Count(R,TEXT("constraint_permission_query_calls"));const bool ConstraintAllowed=FPhysicsAssetUtils::CanCreateConstraints();D->SetBoolField(TEXT("constraint_creation_allowed"),ConstraintAllowed);if(N>1&&!ConstraintAllowed) { Fail(R,TEXT("skeletal_physics_constraint_creation_restricted"),TEXT("Original required constraint count cannot be produced under current public asset-class permission policy."));return R; }
 Telemetry.Phase(FPhysicsTelemetry::SDKGeneration);
 if(Rebuild&&!Rebuild->Check()){return R;}R.Stage=TEXT("allocate_scratch");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("scratch_allocation_attempts"));auto* PhysicsAsset=NewObject<UPhysicsAsset>(GetTransientPackage(),NAME_None,RF_Transient|RF_Transactional);Count(R,TEXT("scratch_allocation_returns"));if(!PhysicsAsset) { Fail(R,TEXT("skeletal_physics_scratch_allocation_failed"),TEXT("Failed to allocate transient PhysicsAsset scratch object"));return R; }TStrongObjectPtr<UPhysicsAsset> KeepScratch(PhysicsAsset);if(Rebuild&&!Rebuild->Check()){return R;}D->SetStringField(TEXT("scratch_asset_path"),PhysicsAsset->GetPathName());
 FPhysAssetCreateParams CreateParams;CreateParams.MinBoneSize=0.0f;CreateParams.MinWeldSize=0.0f;CreateParams.GeomType=EFG_Sphyl;CreateParams.VertWeight=EVW_DominantWeight;CreateParams.bAutoOrientToBone=true;CreateParams.bCreateConstraints=true;CreateParams.bWalkPastSmall=false;CreateParams.bBodyForAll=true;CreateParams.bDisableCollisionsByDefault=bDisableCollision;CreateParams.AngularConstraintMode=ACM_Limited;
 R.Stage=TEXT("generate_scratch");Count(R,TEXT("generator_attempts"));FText CreationError;const bool Generated=FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysicsAsset,SkeletalMesh,CreateParams,CreationError,false,false);Count(R,TEXT("generator_returns"));D->SetBoolField(TEXT("generator_returned"),true);D->SetBoolField(TEXT("generator_success"),Generated);D->SetStringField(TEXT("generator_error"),CreationError.ToString());D->SetBoolField(TEXT("engine_internal_lower_min_bone_size_retry_possible"),true);if(Rebuild){if(!Rebuild->Check()){return R;}if(Generated&&!Rebuild->AdoptScratch(PhysicsAsset)){return R;}}
 auto GeneratedState=MakeShared<FJsonObject>();D->SetObjectField(TEXT("scratch_after_generation"),GeneratedState);const bool GenerationObserved=ObservePhysics(PhysicsAsset,GeneratedState,R,Rebuild.Get());TSharedPtr<FJsonObject> AfterGeneration;const bool SourceGenerationObserved=Capture(SkeletalMesh,ExistingPhysicsAsset,TargetPath,R,TEXT("after_generation"),AfterGeneration,Rebuild.Get());D->SetBoolField(TEXT("generator_current_observation_complete"),GenerationObserved&&SourceGenerationObserved);if(Rebuild&&!Rebuild->Check()){return R;}
 if(!Generated) { Fail(R,TEXT("skeletal_physics_generation_failed"),FString::Printf(TEXT("CreateFromSkeletalMesh failed: %s"),*CreationError.ToString()));return R; }if(!GenerationObserved||!SourceGenerationObserved||!SourceStable(Before,AfterGeneration,TargetPath)) { Fail(R,TEXT("skeletal_physics_generation_observation_failed"),TEXT("Returned generation/preview effect prefix retained; current source/scratch proof incomplete or changed, no target commit/save."));return R; }
 Telemetry.Phase(FPhysicsTelemetry::ScratchBodies);
 R.Stage=TEXT("configure_scratch");TArray<TSharedPtr<FJsonValue>> BodyValues,ConstraintValues;
 for(int32 I=0;I<PhysicsAsset->SkeletalBodySetups.Num();++I)
 { if(Rebuild&&!Rebuild->Check()){return R;}auto* B=PhysicsAsset->SkeletalBodySetups[I].Get();if(!B) { Fail(R,TEXT("skeletal_physics_generated_body_null"),TEXT("Generated null body setup"));return R; }auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("object_path"),B->GetPathName());Call->SetBoolField(TEXT("returned"),false);BodyValues.Add(MakeShared<FJsonValueObject>(Call));D->SetArrayField(TEXT("body_calls"),BodyValues);Count(R,TEXT("body_modify_calls"));B->Modify();if(Rebuild&&!Rebuild->Check()){return R;}Count(R,TEXT("body_field_write_calls"),6);B->PhysicsType=bRootKinematic&&B->BoneName==KinematicRootBone?PhysType_Kinematic:PhysType_Simulated;B->CollisionReponse=bDisableCollision?EBodyCollisionResponse::BodyCollision_Disabled:EBodyCollisionResponse::BodyCollision_Enabled;Count(R,TEXT("body_collision_setter_calls"));B->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);if(Rebuild&&!Rebuild->Check()){return R;}B->DefaultInstance.LinearDamping=LinearDamping;B->DefaultInstance.AngularDamping=AngularDamping;B->DefaultInstance.bStartAwake=true;B->DefaultInstance.MassScale=I==PhysicsAsset->SkeletalBodySetups.Num()-1?TipMassScale:1.0f;Call->SetBoolField(TEXT("returned"),true);if(Rebuild&&!Rebuild->Raw()){return R;} }
 Telemetry.Phase(FPhysicsTelemetry::ScratchPairs);
 // IM performance: these exact native pair setters only modify the scratch collision TMap.
 if(Rebuild&&!Rebuild->Check()){return R;}
 for(int32 A=0;A<PhysicsAsset->SkeletalBodySetups.Num();++A) { for(int32 B=A+1;B<PhysicsAsset->SkeletalBodySetups.Num();++B) { if(Rebuild&&!Rebuild->Raw()){return R;}Count(R,TEXT("pair_collision_setter_calls"));if(bDisableCollision) { PhysicsAsset->DisableCollision(A,B); }else { PhysicsAsset->EnableCollision(A,B); }if(Rebuild&&!Rebuild->Raw()){return R;} } }
 if(Rebuild&&!Rebuild->Check()){return R;}
 Telemetry.Phase(FPhysicsTelemetry::ScratchConstraints);
 for(int32 I=0;I<PhysicsAsset->ConstraintSetup.Num();++I)
 { if(Rebuild&&!Rebuild->Check()){return R;}auto* T=PhysicsAsset->ConstraintSetup[I].Get();if(!T) { Fail(R,TEXT("skeletal_physics_generated_constraint_null"),TEXT("Generated null constraint template"));return R; }auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("object_path"),T->GetPathName());Call->SetBoolField(TEXT("returned"),false);ConstraintValues.Add(MakeShared<FJsonValueObject>(Call));D->SetArrayField(TEXT("constraint_calls"),ConstraintValues);Count(R,TEXT("constraint_modify_calls"));T->Modify();if(Rebuild&&!Rebuild->Check()){return R;}auto& V=T->DefaultInstance;Count(R,TEXT("constraint_setter_calls"),6);V.SetLinearLimits(LCM_Locked,LCM_Locked,LCM_Locked,0.0f);if(Rebuild&&!Rebuild->Check()){return R;}V.SetAngularSwing1Limit(ACM_Limited,Swing1Limit);if(Rebuild&&!Rebuild->Check()){return R;}V.SetAngularSwing2Limit(ACM_Limited,Swing2Limit);if(Rebuild&&!Rebuild->Check()){return R;}V.SetAngularTwistLimit(ACM_Limited,TwistLimit);if(Rebuild&&!Rebuild->Check()){return R;}V.SetDisableCollision(bDisableCollision);if(Rebuild&&!Rebuild->Check()){return R;}V.SetProjectionParams(true,0.3f,0.1f,5.0f,10.0f);if(Rebuild&&!Rebuild->Check()){return R;}Count(R,TEXT("default_profile_copy_calls"));T->SetDefaultProfile(V);if(Rebuild&&!Rebuild->Raw()){return R;}Call->SetBoolField(TEXT("returned"),true); }
 Telemetry.Phase(FPhysicsTelemetry::ScratchVerify);
     const int32 ExpectedConstraintCount = FMath::Max(0, ExpectedBones.Num() - 1);
    auto ValidateGeneratedAsset = [
        &R,
        &ExpectedBones,
        &ReferenceSkeleton,
        KinematicRootBone,
        bRootKinematic,
        bDisableCollision,
        LinearDamping,
        AngularDamping,
        TipMassScale,
        Swing1Limit,
        Swing2Limit,
        TwistLimit,
        ExpectedConstraintCount](const UPhysicsAsset* Candidate, FString& OutFailure)
    {
        if (!Candidate)
        {
            OutFailure = TEXT("PhysicsAsset readback target is null");
            return false;
        }
        if (Candidate->SkeletalBodySetups.Num() != ExpectedBones.Num() ||
            Candidate->ConstraintSetup.Num() != ExpectedConstraintCount)
        {
            OutFailure = FString::Printf(
                TEXT("PhysicsAsset topology mismatch. bodies=%d expected=%d constraints=%d expected_constraints=%d"),
                Candidate->SkeletalBodySetups.Num(),
                ExpectedBones.Num(),
                Candidate->ConstraintSetup.Num(),
                ExpectedConstraintCount);
            return false;
        }

        for (int32 BodyIndex = 0; BodyIndex < ExpectedBones.Num(); ++BodyIndex)
        {
            const USkeletalBodySetup* BodySetup = Candidate->SkeletalBodySetups[BodyIndex];
            const EPhysicsType ExpectedPhysicsType = bRootKinematic && ExpectedBones[BodyIndex] == KinematicRootBone
                ? PhysType_Kinematic
                : PhysType_Simulated;
            const EBodyCollisionResponse::Type ExpectedCollisionResponse = bDisableCollision
                ? EBodyCollisionResponse::BodyCollision_Disabled
                : EBodyCollisionResponse::BodyCollision_Enabled;
            const float ExpectedMassScale = BodyIndex == ExpectedBones.Num() - 1 ? TipMassScale : 1.0f;
            if (!BodySetup ||
                BodySetup->BoneName != ExpectedBones[BodyIndex] ||
                BodySetup->PhysicsType != ExpectedPhysicsType ||
                BodySetup->CollisionReponse != ExpectedCollisionResponse ||
                CollisionEnabled(R,BodySetup->DefaultInstance) != ECollisionEnabled::PhysicsOnly ||
                !FMath::IsNearlyEqual(BodySetup->DefaultInstance.LinearDamping, LinearDamping, KINDA_SMALL_NUMBER) ||
                !FMath::IsNearlyEqual(BodySetup->DefaultInstance.AngularDamping, AngularDamping, KINDA_SMALL_NUMBER) ||
                !FMath::IsNearlyEqual(BodySetup->DefaultInstance.MassScale, ExpectedMassScale, KINDA_SMALL_NUMBER) ||
                !BodySetup->DefaultInstance.bStartAwake ||
                BodySetup->AggGeom.GetElementCount() <= 0)
            {
                OutFailure = FString::Printf(TEXT("PhysicsAsset body readback failed at index %d"), BodyIndex);
                return false;
            }
        }

        for (int32 BodyIndexA = 0; BodyIndexA < ExpectedBones.Num(); ++BodyIndexA)
        {
            for (int32 BodyIndexB = BodyIndexA + 1; BodyIndexB < ExpectedBones.Num(); ++BodyIndexB)
            {
                if (PairEnabled(R,Candidate,BodyIndexA,BodyIndexB) == bDisableCollision)
                {
                    OutFailure = FString::Printf(
                        TEXT("PhysicsAsset collision-pair readback failed for body indices %d and %d"),
                        BodyIndexA,
                        BodyIndexB);
                    return false;
                }
            }
        }

        TSet<FName> VerifiedConstraintChildren;
        for (const UPhysicsConstraintTemplate* ConstraintTemplate : Candidate->ConstraintSetup)
        {
            if (!ConstraintTemplate)
            {
                OutFailure = TEXT("PhysicsAsset contains a null constraint template");
                return false;
            }

            const FConstraintInstance& Constraint = ConstraintTemplate->DefaultInstance;
            const int32 ChildIndex = ReferenceSkeleton.FindBoneIndex(Constraint.ConstraintBone1);
            const int32 ParentIndex = ChildIndex != INDEX_NONE ? ReferenceSkeleton.GetParentIndex(ChildIndex) : INDEX_NONE;
            if (ChildIndex <= 0 || ParentIndex == INDEX_NONE ||
                Constraint.JointName != Constraint.ConstraintBone1 ||
                Constraint.ConstraintBone2 != ReferenceSkeleton.GetBoneName(ParentIndex) ||
                VerifiedConstraintChildren.Contains(Constraint.ConstraintBone1))
            {
                OutFailure = FString::Printf(
                    TEXT("PhysicsAsset constraint hierarchy readback failed for child '%s' and parent '%s'"),
                    *Constraint.ConstraintBone1.ToString(),
                    *Constraint.ConstraintBone2.ToString());
                return false;
            }

            float ProjectionLinearAlpha = 0.0f;
            float ProjectionAngularAlpha = 0.0f;
            float ProjectionLinearTolerance = 0.0f;
            float ProjectionAngularTolerance = 0.0f;
            Projection(R,Constraint,
                ProjectionLinearAlpha,
                ProjectionAngularAlpha,
                ProjectionLinearTolerance,
                ProjectionAngularTolerance);
            if (Constraint.GetLinearXMotion() != LCM_Locked ||
                Constraint.GetLinearYMotion() != LCM_Locked ||
                Constraint.GetLinearZMotion() != LCM_Locked ||
                Constraint.GetAngularSwing1Motion() != ACM_Limited ||
                Constraint.GetAngularSwing2Motion() != ACM_Limited ||
                Constraint.GetAngularTwistMotion() != ACM_Limited ||
                !FMath::IsNearlyEqual(Constraint.GetAngularSwing1Limit(), Swing1Limit, 0.01f) ||
                !FMath::IsNearlyEqual(Constraint.GetAngularSwing2Limit(), Swing2Limit, 0.01f) ||
                !FMath::IsNearlyEqual(Constraint.GetAngularTwistLimit(), TwistLimit, 0.01f) ||
                Constraint.IsCollisionDisabled() != bDisableCollision ||
                !Constraint.IsProjectionEnabled() ||
                !FMath::IsNearlyEqual(ProjectionLinearAlpha, 0.3f, 0.001f) ||
                !FMath::IsNearlyEqual(ProjectionAngularAlpha, 0.1f, 0.001f) ||
                !FMath::IsNearlyEqual(ProjectionLinearTolerance, 5.0f, 0.001f) ||
                !FMath::IsNearlyEqual(ProjectionAngularTolerance, 10.0f, 0.001f))
            {
                OutFailure = FString::Printf(
                    TEXT("PhysicsAsset constraint settings readback failed for child '%s'"),
                    *Constraint.ConstraintBone1.ToString());
                return false;
            }
            VerifiedConstraintChildren.Add(Constraint.ConstraintBone1);
        }

        for (int32 ChildIndex = 1; ChildIndex < ExpectedBones.Num(); ++ChildIndex)
        {
            if (!VerifiedConstraintChildren.Contains(ExpectedBones[ChildIndex]))
            {
                OutFailure = FString::Printf(
                    TEXT("PhysicsAsset is missing the constraint for child bone '%s'"),
                    *ExpectedBones[ChildIndex].ToString());
                return false;
            }
        }
        return true;
    };


 if(Rebuild&&!Rebuild->Check()){return R;}FString ReadbackFailure;auto Configured=MakeShared<FJsonObject>();D->SetObjectField(TEXT("scratch_after_configuration"),Configured);if(!ObservePhysics(PhysicsAsset,Configured,R,Rebuild.Get())) { Fail(R,TEXT("skeletal_physics_scratch_observation_incomplete"),TEXT("Actual scratch configuration retained; observation incomplete, no target commit."));return R; }Count(R,TEXT("asset_validation_calls"));if(!ValidateGeneratedAsset(PhysicsAsset,ReadbackFailure)) { Fail(R,TEXT("skeletal_physics_scratch_validation_failed"),FString::Printf(TEXT("Transient PhysicsAsset validation failed: %s"),*ReadbackFailure));return R; }D->SetBoolField(TEXT("scratch_original_predicates_verified"),true);if(Rebuild&&!Rebuild->Check()){return R;}
 Telemetry.Phase(FPhysicsTelemetry::TargetDuplicates);
 UPackage* Package=Rebuild?Rebuild->Package.Get():nullptr;UPhysicsAsset* Target=ExistingPhysicsAsset;
 if(!Rebuild)
 {
  R.Stage=TEXT("allocate_target");Count(R,TEXT("package_create_calls"));Package=CreatePackage(*TargetPath);if(!Package) { Fail(R,TEXT("skeletal_physics_package_create_failed"),TEXT("Failed to create target package"));return R; }Count(R,TEXT("target_allocation_attempts"));Target=NewObject<UPhysicsAsset>(Package,TargetName,RF_Transactional);Count(R,TEXT("target_allocation_returns"));if(!Target) { Fail(R,TEXT("skeletal_physics_target_allocation_failed"),TEXT("Failed to allocate target PhysicsAsset"));return R; }
 }
 TStrongObjectPtr<UPhysicsAsset> KeepTarget(Target);if(Rebuild&&!Rebuild->Check()){return R;}D->SetStringField(TEXT("allocated_target_path"),Target->GetPathName());
 auto Cleanup=[&]()
 {
  Legacy->SetBoolField(TEXT("success"),false);
  if(Rebuild){const bool Restored=Rebuild->Restore();D->SetBoolField(TEXT("restore_selected_memory_verified"),Restored);R.bChangeStateKnown=false;return;}
  Count(R,TEXT("target_cleanup_attempts"));Target->ClearFlags(RF_Public|RF_Standalone);Target->MarkAsGarbage();Count(R,TEXT("target_cleanup_returns"));D->SetBoolField(TEXT("new_target_cleanup_returned"),true);D->SetBoolField(TEXT("cleanup_exhaustive_rollback_verified"),false);
 };
 TArray<TObjectPtr<USkeletalBodySetup>> Bodies;TArray<TObjectPtr<UPhysicsConstraintTemplate>> Constraints;TArray<TStrongObjectPtr<USkeletalBodySetup>> KeepBodies;TArray<TStrongObjectPtr<UPhysicsConstraintTemplate>> KeepConstraints;TArray<TSharedPtr<FJsonValue>> DuplicateRows;
 for(auto& B:PhysicsAsset->SkeletalBodySetups)
 {
  if(Rebuild)
  {
   if(!Rebuild->Check()){return R;}
   Count(R,TEXT("body_duplicate_attempts"));auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("source"),B->GetPathName());Row->SetBoolField(TEXT("returned"),false);DuplicateRows.Add(MakeShared<FJsonValueObject>(Row));D->SetArrayField(TEXT("duplicate_calls"),DuplicateRows);
   UClass* CopyClass=USkeletalBodySetup::StaticClass();if(!Rebuild->Check()){return R;}D->SetBoolField(TEXT("target_mutation_attempted"),true);
   auto* Copy=DuplicateObject<USkeletalBodySetup>(B.Get(),Target,MakeUniqueObjectName(Target,CopyClass,B->GetFName()));
   Count(R,TEXT("body_duplicate_returns"));Row->SetBoolField(TEXT("returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),Copy!=nullptr);
   if(!Rebuild->Safe()){Rebuild->Stop(TEXT("skeletal_physics_rebuild_duplicate_context_changed"),TEXT("Duplicate returned after context loss; no raw return was read or retried."));return R;}
   if(!Copy){Cleanup();if(Rebuild->Stopped){return R;}Fail(R,TEXT("skeletal_physics_body_duplicate_failed"),TEXT("Generated subobject duplication failed; selected original arrays retained, no atomic rollback claim."));return R;}
   if(!Loaded(Copy)){Rebuild->Stop(TEXT("skeletal_physics_rebuild_duplicate_identity_invalid"),TEXT("Duplicate is not fully loaded/live."));return R;}
   KeepBodies.Emplace(Copy);if(!Rebuild->Check()){return R;}
   if(!Loaded(Copy)||Copy->GetClass()!=CopyClass||Copy->GetOuter()!=Target||Copy->GetPackage()!=Package)
   {Rebuild->Stop(TEXT("skeletal_physics_rebuild_duplicate_identity_invalid"),TEXT("Strongly retained duplicate lost exact class/target ownership before flag writes; no foreign object is modified."));return R;}
   Copy->ClearFlags(RF_Transient);Copy->SetFlags(RF_Transactional);
   if(!Rebuild->AdoptPrepared(Copy,CopyClass)){return R;}R.bChanged=true;
   Row->SetStringField(TEXT("target"),Copy->GetPathName());Bodies.Add(Copy);D->SetBoolField(TEXT("existing_target_subobjects_allocated"),true);
  }
  else
 { Count(R,TEXT("body_duplicate_attempts"));auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("source"),B->GetPathName());Row->SetBoolField(TEXT("returned"),false);DuplicateRows.Add(MakeShared<FJsonValueObject>(Row));D->SetArrayField(TEXT("duplicate_calls"),DuplicateRows);auto* Copy=DuplicateObject<USkeletalBodySetup>(B.Get(),Target,MakeUniqueObjectName(Target,USkeletalBodySetup::StaticClass(),B->GetFName()));Count(R,TEXT("body_duplicate_returns"));Row->SetBoolField(TEXT("returned"),true);Row->SetStringField(TEXT("target"),Copy?Copy->GetPathName():FString());if(!Copy) { Cleanup();Fail(R,TEXT("skeletal_physics_body_duplicate_failed"),TEXT("Failed to duplicate generated body setup into target PhysicsAsset"));return R; }KeepBodies.Emplace(Copy);Copy->ClearFlags(RF_Transient);Copy->SetFlags(RF_Transactional);Bodies.Add(Copy); }
 }
 for(auto& C:PhysicsAsset->ConstraintSetup)
 {
  if(Rebuild)
  {
   if(!Rebuild->Check()){return R;}
   Count(R,TEXT("constraint_duplicate_attempts"));auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("source"),C->GetPathName());Row->SetBoolField(TEXT("returned"),false);DuplicateRows.Add(MakeShared<FJsonValueObject>(Row));D->SetArrayField(TEXT("duplicate_calls"),DuplicateRows);
   UClass* CopyClass=UPhysicsConstraintTemplate::StaticClass();if(!Rebuild->Check()){return R;}D->SetBoolField(TEXT("target_mutation_attempted"),true);
   auto* Copy=DuplicateObject<UPhysicsConstraintTemplate>(C.Get(),Target,MakeUniqueObjectName(Target,CopyClass,C->GetFName()));
   Count(R,TEXT("constraint_duplicate_returns"));Row->SetBoolField(TEXT("returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),Copy!=nullptr);
   if(!Rebuild->Safe()){Rebuild->Stop(TEXT("skeletal_physics_rebuild_duplicate_context_changed"),TEXT("Duplicate returned after context loss; no raw return was read or retried."));return R;}
   if(!Copy){Cleanup();if(Rebuild->Stopped){return R;}Fail(R,TEXT("skeletal_physics_constraint_duplicate_failed"),TEXT("Generated subobject duplication failed; selected original arrays retained, no atomic rollback claim."));return R;}
   if(!Loaded(Copy)){Rebuild->Stop(TEXT("skeletal_physics_rebuild_duplicate_identity_invalid"),TEXT("Duplicate is not fully loaded/live."));return R;}
   KeepConstraints.Emplace(Copy);if(!Rebuild->Check()){return R;}
   if(!Loaded(Copy)||Copy->GetClass()!=CopyClass||Copy->GetOuter()!=Target||Copy->GetPackage()!=Package)
   {Rebuild->Stop(TEXT("skeletal_physics_rebuild_duplicate_identity_invalid"),TEXT("Strongly retained duplicate lost exact class/target ownership before flag writes; no foreign object is modified."));return R;}
   Copy->ClearFlags(RF_Transient);Copy->SetFlags(RF_Transactional);
   if(!Rebuild->AdoptPrepared(Copy,CopyClass)){return R;}R.bChanged=true;
   Row->SetStringField(TEXT("target"),Copy->GetPathName());Constraints.Add(Copy);D->SetBoolField(TEXT("existing_target_subobjects_allocated"),true);
  }
  else
 { Count(R,TEXT("constraint_duplicate_attempts"));auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("source"),C->GetPathName());Row->SetBoolField(TEXT("returned"),false);DuplicateRows.Add(MakeShared<FJsonValueObject>(Row));D->SetArrayField(TEXT("duplicate_calls"),DuplicateRows);auto* Copy=DuplicateObject<UPhysicsConstraintTemplate>(C.Get(),Target,MakeUniqueObjectName(Target,UPhysicsConstraintTemplate::StaticClass(),C->GetFName()));Count(R,TEXT("constraint_duplicate_returns"));Row->SetBoolField(TEXT("returned"),true);Row->SetStringField(TEXT("target"),Copy?Copy->GetPathName():FString());if(!Copy) { Cleanup();Fail(R,TEXT("skeletal_physics_constraint_duplicate_failed"),TEXT("Failed to duplicate generated constraint setup into target PhysicsAsset"));return R; }KeepConstraints.Emplace(Copy);Copy->ClearFlags(RF_Transient);Copy->SetFlags(RF_Transactional);Constraints.Add(Copy); }
 }
 Telemetry.Phase(FPhysicsTelemetry::ModifyCommit);
 if(Rebuild)
 {
  if(!Rebuild->CapturePreview()){return R;}
  TSharedPtr<FJsonObject> PriorCommit;if(!Capture(SkeletalMesh,Target,TargetPath,R,TEXT("before_existing_commit"),PriorCommit,Rebuild.Get())||!Rebuild->Check()){if(!Rebuild->Stopped){Rebuild->Stop(TEXT("skeletal_physics_rebuild_precommit_observation_incomplete"),TEXT("Current source/existing target observation is incomplete before in-place commit."));}return R;}
  if(!SourceStable(Before,PriorCommit,TargetPath)){Rebuild->Stop(TEXT("skeletal_physics_rebuild_source_changed"),TEXT("Source/nonselected dirty state changed before commit; retain prepared duplicate prefix without target-array writes."));return R;}
  R.Stage=TEXT("commit_target");D->SetBoolField(TEXT("target_mutation_attempted"),true);
  const bool PackageDirtyBeforeModify=Rebuild->Package->IsDirty();ITransaction* const UndoBeforeModify=GUndo;
  Count(R,TEXT("target_modify_calls"));const bool TargetSavedToTransaction=Target->Modify();D->SetBoolField(TEXT("target_modify_returned"),true);D->SetBoolField(TEXT("target_saved_to_transaction"),TargetSavedToTransaction);
  if(!Rebuild->AfterSelectedTargetModify(PackageDirtyBeforeModify,UndoBeforeModify,TargetSavedToTransaction)){return R;}
  Rebuild->Committed=true;Rebuild->ExpectedBodies=Bodies;Rebuild->ExpectedConstraints=Constraints;Rebuild->ExpectedCollision=PhysicsAsset->CollisionDisableTable;Rebuild->ExpectedBounds.Reset();
  Count(R,TEXT("target_commit_calls"));Target->SkeletalBodySetups=MoveTemp(Bodies);Target->ConstraintSetup=MoveTemp(Constraints);Target->CollisionDisableTable=PhysicsAsset->CollisionDisableTable;Target->BoundsBodies.Reset();R.bChanged=true;if(!Rebuild->Check()){return R;}
  if(!Rebuild->CommitPreviewReady()){return R;}Rebuild->ExpectedPreview=SkeletalMesh;Target->SetPreviewMesh(SkeletalMesh,false);if(!Rebuild->Check()){return R;}
  Rebuild->ExpectedBodyIndex.Reset();for(int32 Index=0;Index<Target->SkeletalBodySetups.Num();++Index){Rebuild->ExpectedBodyIndex.Add(Target->SkeletalBodySetups[Index]->BoneName,Index);}
  Count(R,TEXT("body_index_update_calls"));Target->UpdateBodySetupIndexMap();if(!Rebuild->Check()){return R;}
  for(int32 Index=0;Index<Target->SkeletalBodySetups.Num();++Index){if(Target->SkeletalBodySetups[Index]->bConsiderForBounds){Rebuild->ExpectedBounds.Add(Index);}}
  Count(R,TEXT("bounds_update_calls"));Target->UpdateBoundsBodiesArray();if(!Rebuild->Check()){return R;}
  Count(R,TEXT("refresh_physics_asset_calls"));Target->RefreshPhysicsAssetChange();if(!Rebuild->Check()){return R;}D->SetBoolField(TEXT("target_commit_returned"),true);
 }
 else
 {
 R.Stage=TEXT("commit_target");D->SetBoolField(TEXT("target_mutation_attempted"),true);Count(R,TEXT("target_modify_calls"));Target->Modify();Count(R,TEXT("target_commit_calls"));Target->SkeletalBodySetups=MoveTemp(Bodies);Target->ConstraintSetup=MoveTemp(Constraints);Target->CollisionDisableTable=PhysicsAsset->CollisionDisableTable;Target->BoundsBodies.Reset();Target->PreviewSkeletalMesh=SkeletalMesh;Count(R,TEXT("body_index_update_calls"));Target->UpdateBodySetupIndexMap();Count(R,TEXT("bounds_update_calls"));Target->UpdateBoundsBodiesArray();Count(R,TEXT("refresh_physics_asset_calls"));Target->RefreshPhysicsAssetChange();D->SetBoolField(TEXT("target_commit_returned"),true);
 }
 auto Committed=MakeShared<FJsonObject>();D->SetObjectField(TEXT("committed_target"),Committed);if(!ObservePhysics(Target,Committed,R,Rebuild.Get())) { Fail(R,TEXT("skeletal_physics_commit_observation_incomplete"),TEXT("Actual commit/refresh prefix retained; current target projection incomplete."));return R; }Count(R,TEXT("asset_validation_calls"));if(!ValidateGeneratedAsset(Target,ReadbackFailure)) { Cleanup();if(Rebuild&&Rebuild->Stopped){return R;}Fail(R,TEXT("skeletal_physics_commit_validation_failed"),(Rebuild?FString::Printf(TEXT("Persistent PhysicsAsset validation failed; selected memory restore result recorded, no atomic disk/world rollback: %s"),*ReadbackFailure):FString::Printf(TEXT("Persistent PhysicsAsset validation failed; new-target cleanup attempted: %s"),*ReadbackFailure)));return R; }D->SetBoolField(TEXT("committed_original_predicates_verified"),true);if(Rebuild&&!Rebuild->Check()){return R;}
 if(!Rebuild){Target->SetFlags(RF_Public|RF_Standalone|RF_Transactional);}if(Rebuild&&!Rebuild->Check()){return R;}Count(R,TEXT("mark_package_dirty_calls"));Target->MarkPackageDirty();if(Rebuild&&!Rebuild->Check()){return R;}TSharedPtr<FJsonObject> AfterCommit;R.Stage=TEXT("after_commit");if(!Capture(SkeletalMesh,Target,TargetPath,R,TEXT("after_commit"),AfterCommit,Rebuild.Get())) { Fail(R,TEXT("skeletal_physics_commit_observation_incomplete"),TEXT("Target writer returned; source/current observation incomplete, no save."));return R; }if(!SourceStable(Before,AfterCommit,TargetPath)) { R.bChangeStateKnown=true;R.bChanged=true;Fail(R,TEXT("skeletal_physics_source_preservation_failed"),TEXT("Actual committed target retained; tracked source/nonselected dirty state changed, no fallback save."));return R; }
 Legacy->SetBoolField(TEXT("created"),!Rebuild);Legacy->SetBoolField(TEXT("rebuilt_in_place"),bool(Rebuild));Legacy->SetBoolField(TEXT("save_requested"),bSave);Legacy->SetBoolField(TEXT("saved"),false);Legacy->SetStringField(TEXT("physics_asset"),Target->GetPathName());Legacy->SetNumberField(TEXT("body_count"),Target->SkeletalBodySetups.Num());Legacy->SetNumberField(TEXT("constraint_count"),Target->ConstraintSetup.Num());Legacy->SetBoolField(TEXT("independent_readback_passed"),true);
 TArray<TSharedPtr<FJsonValue>> LegacyBodies,LegacyConstraints;for(int32 I=0;I<Target->SkeletalBodySetups.Num();++I) { const auto* B=Target->SkeletalBodySetups[I].Get();auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),I);J->SetStringField(TEXT("bone"),B->BoneName.ToString());J->SetStringField(TEXT("physics_type"),B->PhysicsType==PhysType_Kinematic?TEXT("kinematic"):TEXT("simulated"));J->SetNumberField(TEXT("shape_count"),B->AggGeom.GetElementCount());J->SetBoolField(TEXT("collision_response_disabled"),B->CollisionReponse==EBodyCollisionResponse::BodyCollision_Disabled);J->SetNumberField(TEXT("linear_damping"),B->DefaultInstance.LinearDamping);J->SetNumberField(TEXT("angular_damping"),B->DefaultInstance.AngularDamping);J->SetNumberField(TEXT("mass_scale"),B->DefaultInstance.MassScale);LegacyBodies.Add(MakeShared<FJsonValueObject>(J)); }
 for(int32 I=0;I<Target->ConstraintSetup.Num();++I) { const auto& V=Target->ConstraintSetup[I]->DefaultInstance;auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),I);J->SetStringField(TEXT("joint"),V.JointName.ToString());J->SetStringField(TEXT("child_bone"),V.ConstraintBone1.ToString());J->SetStringField(TEXT("parent_bone"),V.ConstraintBone2.ToString());J->SetNumberField(TEXT("swing1_limit_degrees"),Swing1Limit);J->SetNumberField(TEXT("swing2_limit_degrees"),Swing2Limit);J->SetNumberField(TEXT("twist_limit_degrees"),TwistLimit);LegacyConstraints.Add(MakeShared<FJsonValueObject>(J)); }Legacy->SetArrayField(TEXT("bodies"),LegacyBodies);Legacy->SetArrayField(TEXT("constraints"),LegacyConstraints);Project(R,Legacy);
 Telemetry.Phase(FPhysicsTelemetry::SavePostverify);
 if(bSave)
 { if(Rebuild&&!Rebuild->Check()){return R;}R.Stage=TEXT("save_target");R.bSaveAttempted=true;if(Rebuild){Rebuild->DiskMayChange=true;D->SetBoolField(TEXT("disk_write_outcome_unknown"),true);}Count(R,TEXT("save_attempt_count"));auto Save=MakeShared<FJsonObject>();Save->SetStringField(TEXT("asset_path"),Target->GetPathName());Save->SetBoolField(TEXT("only_if_dirty"),false);Save->SetBoolField(TEXT("attempted"),true);Save->SetBoolField(TEXT("returned"),false);D->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Save)});const bool SaveReturned=UEditorAssetLibrary::SaveLoadedAsset(Target,false);R.bSaved=R.bSaved||SaveReturned;Count(R,TEXT("save_return_count"));Save->SetBoolField(TEXT("returned"),true);Save->SetBoolField(TEXT("saved"),R.bSaved);Legacy->SetBoolField(TEXT("saved"),R.bSaved);Project(R,Legacy);if(Rebuild&&!Rebuild->AdoptSaveLoadedFlags()){return R;}
  if(!SaveReturned) { Legacy->SetBoolField(TEXT("success"),false);D->SetBoolField(TEXT("original_full_previous_state_restore_performed"),false);Cleanup();if(Rebuild&&Rebuild->Stopped){return R;}Fail(R,TEXT("skeletal_physics_save_failed"),Rebuild?TEXT("PhysicsAsset failed to save; selected memory restore result recorded, disk write outcome remains unknown and no atomic rollback is claimed."):TEXT("PhysicsAsset failed to save; new-target cleanup attempted, full previous-state/disk rollback is not claimed."));return R; }
  TSharedPtr<FJsonObject> Saved;R.Stage=TEXT("after_save");if(!Capture(SkeletalMesh,Target,TargetPath,R,TEXT("after_save"),Saved,Rebuild.Get())) { Fail(R,TEXT("skeletal_physics_saved_observation_incomplete"),TEXT("Actual successful forced save receipt retained; post-save observation incomplete."));return R; }if(!SourceStable(Before,Saved,TargetPath)||Saved->GetBoolField(TEXT("target_dirty"))||!Saved->GetObjectField(TEXT("target_disk"))->GetBoolField(TEXT("exists"))) { Fail(R,TEXT("skeletal_physics_saved_preservation_failed"),TEXT("Saved receipt retained; current tracked source/target persistence proof failed."));return R; }Count(R,TEXT("asset_validation_calls"));if(!ValidateGeneratedAsset(Target,ReadbackFailure)) { Fail(R,TEXT("skeletal_physics_saved_validation_failed"),ReadbackFailure);return R; }if(Rebuild&&!Rebuild->Check()){return R;}if(Rebuild){D->SetBoolField(TEXT("disk_write_outcome_unknown"),false);D->SetBoolField(TEXT("post_save_selected_state_verified"),true);}
 }
 if(!Rebuild)
 {
  R.Stage=TEXT("asset_registry_notification");Count(R,TEXT("asset_registry_notification_attempts"));FAssetRegistryModule::AssetCreated(Target);Count(R,TEXT("asset_registry_notification_returns"));D->SetBoolField(TEXT("asset_registry_notification_returned"),true);
 }
 else {if(!Rebuild->Check()){return R;}D->SetBoolField(TEXT("existing_asset_registry_identity_preserved"),true);D->SetBoolField(TEXT("asset_registry_notification_not_needed_for_existing"),true);}
 TSharedPtr<FJsonObject> Final;R.Stage=TEXT("after_notification");if(!Capture(SkeletalMesh,Target,TargetPath,R,TEXT("after_notification"),Final,Rebuild.Get())) { Fail(R,TEXT("skeletal_physics_final_observation_incomplete"),TEXT("Actual registry notification/save prefix retained; final current scope incomplete."));return R; }if(!SourceStable(Before,Final,TargetPath)) { R.bChangeStateKnown=true;R.bChanged=true;Fail(R,TEXT("skeletal_physics_final_source_preservation_failed"),TEXT("Current source/nonselected dirty scope changed after notification."));return R; }Count(R,TEXT("asset_validation_calls"));if(!ValidateGeneratedAsset(Target,ReadbackFailure)) { R.bChangeStateKnown=true;R.bChanged=true;Fail(R,TEXT("skeletal_physics_final_validation_failed"),ReadbackFailure);return R; }
 if(Rebuild&&!Rebuild->Check()){return R;}auto LegacyAfter=LegacyState(Target,TargetPath);LegacyAfter->SetBoolField(TEXT("saved"),R.bSaved);LegacyAfter->SetBoolField(TEXT("save_requested"),bSave);StampLegacy(Legacy,false,LegacyBefore,LegacyAfter,N,N-1);
 if(Rebuild)
 {
  auto Delta=Legacy->GetObjectField(TEXT("delta"));Delta->SetNumberField(TEXT("created_count"),0);Delta->SetNumberField(TEXT("updated_count"),1);
  Delta->SetNumberField(TEXT("body_count_before"),Rebuild->OldBodies.Num());Delta->SetNumberField(TEXT("constraint_count_before"),Rebuild->OldConstraints.Num());
  D->SetBoolField(TEXT("existing_target_identity_preserved"),true);D->SetBoolField(TEXT("rebuild_selected_memory_verified"),true);
  D->SetStringField(TEXT("rebuild_restore_semantics"),TEXT("In-place old identity retained. On semantic commit/save failure selected memory restore is attempted only while safe; disk/world/cache/transaction or new duplicate subobjects are not atomically rolled back."));
 }
 Project(R,Legacy);D->SetBoolField(TEXT("tracked_source_preserved"),true);D->SetBoolField(TEXT("final_original_predicates_verified"),true);R.bChangeStateKnown=true;R.bChanged=true;R.bPostconditionPassed=true;R.bSuccess=true;R.Stage=TEXT("complete");return R;
}
}
