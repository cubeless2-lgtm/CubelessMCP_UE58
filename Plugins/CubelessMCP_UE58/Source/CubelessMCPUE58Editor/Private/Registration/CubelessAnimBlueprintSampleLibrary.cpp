#include "Registration/CubelessAnimBlueprintSampleLibrary.h"
#include "Compatibility/UE58/CubelessAnimBlueprintSampleApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimBlueprintSampleLibrary::CreateAnimBlueprintSample(const FString& TargetAnimBPPath,const FString& SkeletonPath,const FString& SkeletalMeshPath,const FString& PreviewSkeletalMeshPath,const FString& ParentClassPath,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimBlueprintSample::CreateSample(TargetAnimBPPath,SkeletonPath,SkeletalMeshPath,PreviewSkeletalMeshPath,ParentClassPath,bOverwriteExisting,bCompile,bSaveAsset,bDryRun,bValidateOnly),TEXT("create_anim_blueprint_sample"));
}
