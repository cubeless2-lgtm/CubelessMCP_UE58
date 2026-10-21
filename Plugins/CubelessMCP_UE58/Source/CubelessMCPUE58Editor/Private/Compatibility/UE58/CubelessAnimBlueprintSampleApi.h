#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimBlueprintSample
{
Variable::FResult CreateSample(const FString& TargetAnimBPPath,const FString& SkeletonPath,const FString& SkeletalMeshPath,const FString& PreviewSkeletalMeshPath,const FString& ParentClassPath,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly);
}
