#pragma once
#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::Skeletal
{
Variable::FResult ListBones(const FString& SkeletonPath, const FString& NameContains, int32 MaxBones);
Variable::FResult ListSockets(const FString& SkeletonPath);
}
