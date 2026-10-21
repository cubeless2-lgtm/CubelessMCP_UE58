#pragma once
#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::AIBehaviorAssetsInspection
{
    // The original paired reader; the requested Blackboard remains independent.
    Function::FResult InspectBehaviorAssets(const FString& BehaviorTreePath, const FString& BlackboardPath);
}
