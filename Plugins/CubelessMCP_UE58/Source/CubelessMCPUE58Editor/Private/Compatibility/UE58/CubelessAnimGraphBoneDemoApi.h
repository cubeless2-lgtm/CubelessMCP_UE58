#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimGraphBoneDemo
{
Variable::FResult EnsureModifyBoneDemo(const FString& BlueprintName,const FString& BoneName,const FString& RotationJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample);
}
