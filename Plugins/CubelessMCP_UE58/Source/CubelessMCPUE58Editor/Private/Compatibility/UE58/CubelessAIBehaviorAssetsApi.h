#pragma once
#include "CoreMinimal.h"

/** Finite standard AIModule tree authoring; implementation is Editor-only. */
namespace Cubeless::UE58::AIBehaviorAssets
{
    FString CreatePatrolChaseAssets(const FString& AssetRoot, const FString& BehaviorTreeName,
        const FString& BlackboardName, const FString& TargetActorKey, const FString& PatrolLocationKey,
        const FString& TreeShape, double AcceptableRadius, double AlertWaitSeconds,
        double FacePrecisionDegrees, bool bReplaceExisting, bool bSaveAssets);
}
