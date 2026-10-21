#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::SkeletalRetarget
{
Variable::FResult Set(const FString& SkeletonPath,const FString& BoneName,const FString& Mode,bool bRecursive,bool bSave);
}
