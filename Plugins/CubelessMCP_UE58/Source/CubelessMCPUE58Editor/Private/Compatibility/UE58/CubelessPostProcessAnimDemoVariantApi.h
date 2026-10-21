#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::PostProcessAnimDemoVariant
{
Variable::FResult Ensure(const FString& SourceBlueprintName,const FString& SourceSkeletalMesh,const FString& VariantName,const FString& TargetRoot,const FString& TargetBlueprintNameJson,const FString& TargetSkeletalMeshJson,const FString& BoneName,const FString& RotationJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bOverwriteExisting,bool bCompile,bool bSave,bool bDryRun,bool bAllowNonSample);
}
