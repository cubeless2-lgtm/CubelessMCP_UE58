#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimGraphTrailDemo
{
Variable::FResult EnsureTrailDemo(const FString& BlueprintName,const FString& TrailBone,const FString& BaseJoint,int64 ChainLength,const FString& ChainBoneAxis,double Alpha,const FString& FakeVelocityJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample,bool bInvertChainBoneAxis,bool bReorientParentToChild,bool bActorSpaceFakeVelocity,double RelaxationSpeedScale,bool bLimitStretch,double StretchLimit,double MaxDeltaTime);
}
