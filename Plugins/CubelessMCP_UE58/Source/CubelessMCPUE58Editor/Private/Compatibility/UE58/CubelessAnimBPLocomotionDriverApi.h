#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimBPLocomotionDriver
{
Variable::FResult Ensure(const FString& SourceAnimBPPath,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,const FString& SpeedVariable,bool bCreateVariableIfMissing,bool bIncludeDirection,const FString& DirectionVariable,bool bCreateDirectionVariableIfMissing,bool bIncludeIsInAir,const FString& IsInAirVariable,bool bCreateIsInAirVariableIfMissing,bool bIncludeAcceleration,const FString& AccelerationVariable,bool bCreateAccelerationVariableIfMissing,bool bIncludeIsAccelerating,const FString& IsAcceleratingVariable,bool bCreateIsAcceleratingVariableIfMissing,double IsAcceleratingThreshold,bool bGuardPawnOwner,bool bReplaceExistingLinks,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,const FString& PrivateContextJson);
}
