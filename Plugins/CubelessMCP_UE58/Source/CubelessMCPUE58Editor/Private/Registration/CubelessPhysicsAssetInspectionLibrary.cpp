#include "Registration/CubelessPhysicsAssetInspectionLibrary.h"
#include "Compatibility/UE58/CubelessPhysicsAssetInspectionApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Registration/CubelessJsonConverters.h"
FString UCubelessPhysicsAssetInspectionLibrary::InspectPhysicsAssetConstraintsGuarded(const FString& PhysicsAssetPath,const FString& SkeletalMeshPath,bool bIncludeBodies,bool bIncludeConstraints,bool bIncludeProfiles,bool bIncludeSolverSettings,bool bIncludeCollisionSummary,int64 MaxBodies,int64 MaxConstraints,int64 MaxProfilesPerItem,int64 MaxShapesPerBody,bool bRequireSupportedClass)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::PhysicsAssetInspection::Inspect(PhysicsAssetPath,SkeletalMeshPath,bIncludeBodies,bIncludeConstraints,bIncludeProfiles,bIncludeSolverSettings,bIncludeCollisionSummary,MaxBodies,MaxConstraints,MaxProfilesPerItem,MaxShapesPerBody,bRequireSupportedClass),TEXT("inspect_physics_asset_constraints_guarded"));
}
FString UCubelessPhysicsAssetInspectionLibrary::PreparePhysicsAssetInspectionToolset(UClass* Class)
{
    Cubeless::UE58::Variable::FResult R;R.Stage=TEXT("physics_asset_inspection_schema_preparation");auto J=R.Data;
    J->SetStringField(TEXT("operation"),TEXT("prepare_physics_asset_inspection_toolset"));J->SetStringField(TEXT("class_path"),IsValid(Class)?Class->GetPathName():FString());
    for(const TCHAR* K:{TEXT("default_verified"),TEXT("exact_converter_active"),TEXT("type_schema_verified"),TEXT("actual_call_packing_verified"),TEXT("metadata_mutation_attempted"),TEXT("metadata_changed")}) { J->SetBoolField(K,false); }
    for(const TCHAR* K:{TEXT("property_count"),TEXT("verified_property_count"),TEXT("metadata_write_attempt_count"),TEXT("metadata_changed_count"),TEXT("helper_attempt_count"),TEXT("helper_return_count")}) { J->SetNumberField(K,0); }
    J->SetObjectField(TEXT("defaults"),MakeShared<FJsonObject>());J->SetArrayField(TEXT("property_observations"),TArray<TSharedPtr<FJsonValue>>{});
    if(!Cubeless::UE58::IsEditorGameThread()||!IsValid(Class)||Class->GetPathName()!=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/physics_asset_inspection_toolset_PY.CubelessPhysicsAssetInspectionTools"))
    { R.ErrorCode=TEXT("physics_asset_inspection_schema_class_invalid");R.Error=TEXT("Exact unregistered Editor PhysicsAssetInspectionTools class required"); }
    else
    { J->SetNumberField(TEXT("helper_attempt_count"),1);R.bSuccess=Cubeless::Registration::PreparePhysicsAssetInspectionToolset(Class,J,R.Error);J->SetNumberField(TEXT("helper_return_count"),1);R.bPostconditionPassed=R.bSuccess;if(!R.bSuccess) { R.ErrorCode=TEXT("physics_asset_inspection_schema_preparation_failed"); } }
    return Cubeless::Operations::EncodeVariableResult(R,TEXT("prepare_physics_asset_inspection_toolset"));
}
