#include "Registration/CubelessAIBehaviorAssetsLibrary.h"
#include "Compatibility/UE58/CubelessAIBehaviorAssetsApi.h"

FString UCubelessAIBehaviorAssetsLibrary::CreateAIPatrolChaseBehaviorAssets(const FString& AssetRoot,
    const FString& BehaviorTreeName, const FString& BlackboardName, const FString& TargetActorKey,
    const FString& PatrolLocationKey, const FString& TreeShape, double AcceptableRadius,
    double AlertWaitSeconds, double FacePrecisionDegrees, bool bReplaceExisting, bool bSaveAssets)
{
    return Cubeless::UE58::AIBehaviorAssets::CreatePatrolChaseAssets(AssetRoot, BehaviorTreeName,
        BlackboardName, TargetActorKey, PatrolLocationKey, TreeShape, AcceptableRadius,
        AlertWaitSeconds, FacePrecisionDegrees, bReplaceExisting, bSaveAssets);
}
