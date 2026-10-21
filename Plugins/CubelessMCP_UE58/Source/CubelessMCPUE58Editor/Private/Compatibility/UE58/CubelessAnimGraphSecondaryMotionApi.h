#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimGraphSecondaryMotion
{
Variable::FResult EnsureSecondaryMotionChain(const FString& BlueprintName,const FString& DynamicsType,const FString& RootBone,const FString& ChainEndBone,double Alpha,const FString& GravityJson,double AngularLimitDegrees,const FString& BodyExtentsJson,double LinearDamping,double AngularDamping,double KawaiiDamping,double KawaiiStiffness,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample,bool bCompile,bool bSave);
}
