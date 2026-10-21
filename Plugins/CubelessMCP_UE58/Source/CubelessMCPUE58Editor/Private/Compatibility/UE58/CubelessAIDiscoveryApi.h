#pragma once
#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"

/** Loaded BT-node class discovery only; no load, CDO creation or asset/runtime invocation. */
namespace Cubeless::UE58::AIDiscovery
{
    Function::FResult SearchBehaviorTreeNodeClasses(const FString& Category, const FString& Query,
        bool bIncludeAbstract, int32 MaxResults);
}
