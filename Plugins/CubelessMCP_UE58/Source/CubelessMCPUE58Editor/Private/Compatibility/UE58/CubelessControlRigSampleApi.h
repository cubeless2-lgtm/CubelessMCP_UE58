#pragma once
#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::ControlRigSample
{
Variable::FResult EnsureAnimGraphNodeSample(const FString& SourceAnimBPPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bReplaceExistingLinks,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly);
Variable::FResult EnsureSampleAsset(const FString& TargetControlRigPath,const FString& SkeletalMeshPath,const FString& SkeletonPath,const FString& PreviewSkeletalMeshPath,const FString& SetupMode,const FString& CurvesJson,const FString& NullsJson,const FString& ControlsJson,const FString& UnitNodesJson,int64 MaxDirectCurves,int64 MaxDirectNulls,int64 MaxDirectControls,int64 MaxUnitNodes,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bModularRig,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly);
}
