#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::LinkedAnimGraphVariant
{
Variable::FResult Ensure(const FString& SourceAnimBPPath,const FString& LinkedAnimBPPath,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,const FString& InsertMode,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bReplaceExistingLinks,bool bRequireInputPosePin,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,const FString& PrivateGraphContextJson);
}
