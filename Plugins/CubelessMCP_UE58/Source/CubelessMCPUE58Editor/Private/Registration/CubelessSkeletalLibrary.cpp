#include "Registration/CubelessSkeletalLibrary.h"
#include "Compatibility/UE58/CubelessSkeletalApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Registration/CubelessJsonConverters.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
FString UCubelessSkeletalLibrary::ListSkeletonBones(const FString& Path,const FString& Contains,int32 Max)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::Skeletal::ListBones(Path,Contains,Max),TEXT("list_skeleton_bones")); }
FString UCubelessSkeletalLibrary::ListSkeletalSockets(const FString& Path)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::Skeletal::ListSockets(Path),TEXT("list_skeletal_sockets")); }
FString UCubelessSkeletalLibrary::PrepareSkeletalReadToolset(UClass* Class)
{
 auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("operation"),TEXT("prepare_skeletal_read_toolset"));J->SetStringField(TEXT("class_path"),Class?Class->GetPathName():FString());
 for(const TCHAR* K:{TEXT("success"),TEXT("postcondition_passed"),TEXT("default_verified"),TEXT("exact_converter_active"),TEXT("type_schema_verified"),TEXT("metadata_mutation_attempted"),TEXT("metadata_changed"),TEXT("actual_call_packing_verified"),TEXT("mutation_attempted"),TEXT("changed"),TEXT("compile_requested"),TEXT("compile_attempted"),TEXT("save_requested"),TEXT("save_attempted"),TEXT("saved")}) { J->SetBoolField(K,false); }
 J->SetBoolField(TEXT("change_state_known"),true);J->SetBoolField(TEXT("outcome_unknown"),false);J->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());
 for(const TCHAR* K:{TEXT("metadata_write_attempt_count"),TEXT("metadata_changed_count"),TEXT("verified_property_count"),TEXT("verification_helper_attempt_count"),TEXT("verification_helper_return_count")}) { J->SetNumberField(K,0); }
 FString Error;
 if(!IsInGameThread()) { Error=TEXT("Skeletal read preparation requires the game thread."); }
 else if(!Class || Class->GetPathName()!=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/skeletal_toolset_PY.CubelessSkeletalTools")) { Error=TEXT("Exact canonical SkeletalTools class required."); }
 else if(UToolsetRegistry::IsToolsetClassRegistered(Class)) { Error=TEXT("Invalidate only this class before exact preparation; no metadata retry."); }
 else { J->SetNumberField(TEXT("verification_helper_attempt_count"),1);const bool OK=Cubeless::Registration::VerifySkeletalReadToolset(Class,J,Error);J->SetNumberField(TEXT("verification_helper_return_count"),1);J->SetBoolField(TEXT("success"),OK);J->SetBoolField(TEXT("postcondition_passed"),OK); }
 J->SetStringField(TEXT("stage"),TEXT("skeletal_read_schema_preparation"));J->SetStringField(TEXT("error_code"),Error.IsEmpty()?FString():TEXT("skeletal_read_preparation_failed"));J->SetStringField(TEXT("error"),Error);
 FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));return Json;
}

#include "Compatibility/UE58/CubelessSkeletalSocketApi.h"
FString UCubelessSkeletalLibrary::AddSkeletalSocket(const FString& P,const FString& N,const FString& B,const FString& L,const FString& R,const FString& S,bool Save)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SkeletalSocket::Add(P,N,B,L,R,S,Save),TEXT("add_skeletal_socket")); }
FString UCubelessSkeletalLibrary::SetSkeletalSocket(const FString& P,const FString& N,const FString& B,const FString& New,const FString& L,const FString& R,const FString& S,bool Save)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SkeletalSocket::Set(P,N,B,New,L,R,S,Save),TEXT("set_skeletal_socket")); }
FString UCubelessSkeletalLibrary::RemoveSkeletalSocket(const FString& P,const FString& N,bool Save)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SkeletalSocket::Remove(P,N,Save),TEXT("remove_skeletal_socket")); }
FString UCubelessSkeletalLibrary::PrepareSkeletalSocketToolset(UClass* Class)
{
 auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("operation"),TEXT("prepare_skeletal_socket_toolset"));J->SetStringField(TEXT("class_path"),Class?Class->GetPathName():FString());
 for(const TCHAR* K:{TEXT("success"),TEXT("postcondition_passed"),TEXT("default_verified"),TEXT("exact_converter_active"),TEXT("nullable_schema_verified"),TEXT("carrier_kind_verified"),TEXT("metadata_mutation_attempted"),TEXT("metadata_changed"),TEXT("actual_call_packing_verified"),TEXT("mutation_attempted"),TEXT("changed"),TEXT("compile_requested"),TEXT("compile_attempted"),TEXT("save_requested"),TEXT("save_attempted"),TEXT("saved")}) { J->SetBoolField(K,false); }
 J->SetBoolField(TEXT("change_state_known"),true);J->SetBoolField(TEXT("outcome_unknown"),false);J->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());
 for(const TCHAR* K:{TEXT("metadata_write_attempt_count"),TEXT("metadata_changed_count"),TEXT("verified_property_count"),TEXT("verification_helper_attempt_count"),TEXT("verification_helper_return_count")}) { J->SetNumberField(K,0); }
 FString Error;if(!IsInGameThread()) { Error=TEXT("Socket preparation requires game thread."); }
 else if(!Class||Class->GetPathName()!=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/skeletal_toolset_PY.CubelessSkeletalTools")) { Error=TEXT("Exact canonical SkeletalTools class required."); }
 else if(UToolsetRegistry::IsToolsetClassRegistered(Class)) { Error=TEXT("Invalidate only this class before preparation; no metadata retry."); }
 else { J->SetNumberField(TEXT("verification_helper_attempt_count"),1);const bool OK=Cubeless::Registration::VerifySkeletalSocketToolset(Class,J,Error);J->SetNumberField(TEXT("verification_helper_return_count"),1);J->SetBoolField(TEXT("success"),OK);J->SetBoolField(TEXT("postcondition_passed"),OK); }
 J->SetStringField(TEXT("stage"),TEXT("skeletal_socket_schema_preparation"));J->SetStringField(TEXT("error_code"),Error.IsEmpty()?FString():TEXT("skeletal_socket_preparation_failed"));J->SetStringField(TEXT("error"),Error);
 FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));return Json;
}

#include "Compatibility/UE58/CubelessSkeletalRetargetApi.h"
FString UCubelessSkeletalLibrary::SetBoneTranslationRetargeting(const FString& P,const FString& N,const FString& M,bool Recursive,bool Save)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SkeletalRetarget::Set(P,N,M,Recursive,Save),TEXT("set_bone_translation_retargeting")); }

#include "Compatibility/UE58/CubelessSkeletalPhysicsAssetApi.h"
FString UCubelessSkeletalLibrary::CreateSkeletalPhysicsAsset(const FString& Mesh,const FString& Target,const FString& Expected,const FString& Root,bool RootKinematic,bool DisableCollision,double Swing1,double Swing2,double Twist,double Linear,double Angular,double Tip,bool Overwrite,bool Save,bool Dry)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SkeletalPhysicsAsset::Create(Mesh,Target,Expected,Root,RootKinematic,DisableCollision,Swing1,Swing2,Twist,Linear,Angular,Tip,Overwrite,Save,Dry),TEXT("create_skeletal_physics_asset")); }

#include "Compatibility/UE58/CubelessSkeletalSimulationApi.h"
FString UCubelessSkeletalLibrary::CreateSkeletalBoneConstraintFixture(const FString& Mesh,const FString& Actor,const FString& Bone1,const FString& Bone2,const FString& Context,const FString& LocationJson,const FString& Constraint,double Swing1,double Swing2,double Twist,bool DisableCollision,bool SimulateAll,bool Dry)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SkeletalSimulation::CreateFixture(Mesh,Actor,Bone1,Bone2,Context,LocationJson,Constraint,Swing1,Swing2,Twist,DisableCollision,SimulateAll,Dry),TEXT("create_skeletal_bone_constraint_fixture")); }
FString UCubelessSkeletalLibrary::InspectSkeletalBoneConstraintFixture(const FString& Actor,const FString& Context,const FString& Constraint)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SkeletalSimulation::InspectFixture(Actor,Context,Constraint),TEXT("inspect_skeletal_bone_constraint_fixture")); }
