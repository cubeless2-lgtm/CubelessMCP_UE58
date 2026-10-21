#include "Registration/CubelessAIBehaviorAssetsInspectionLibrary.h"
#include "Compatibility/UE58/CubelessAIBehaviorAssetsInspectionApi.h"
#include "Operations/CubelessFunctionOperations.h"

FString UCubelessAIBehaviorAssetsInspectionLibrary::InspectAIBehaviorAssets(
    const FString& BehaviorTreePath, const FString& BlackboardPath)
{
    return Cubeless::Operations::EncodeFunctionResult(
        Cubeless::UE58::AIBehaviorAssetsInspection::InspectBehaviorAssets(BehaviorTreePath, BlackboardPath),
        TEXT("inspect_ai_behavior_assets"));
}
