#pragma once
#include "CoreMinimal.h"

/** Bounded Editor authoring through exported AIModule / BehaviorTreeEditor / AIGraph APIs. */
namespace Cubeless::UE58::BehaviorTree
{
    FString CreateAsset(const FString& AssetName, const FString& AssetRoot, const FString& BlackboardPath, bool bSaveAsset);
    FString SetBlackboard(const FString& BehaviorTreePath, const FString& BlackboardPath, bool bSaveAsset);
    FString InspectAsset(const FString& AssetPath);
}
