#pragma once
#include "CoreMinimal.h"
class FJsonObject;
struct FDataflowNode;
namespace Cubeless::UE58::ChaosClothAuthoring
{
    FString ConfigureNode(const FString& ParamsJson);
    FString ConfigureWeightMap(const FString& ParamsJson);
    // Shared by the selected FR reader and these writers; only verified current WeightMap nodes.
    // This is an internal Editor EngineAPI helper, not another reflected tool or command executor.
    bool DescribeWeightMapAuthoringState(const TSharedPtr<FDataflowNode>& Node,UObject* Asset,
        TSharedPtr<FJsonObject>& State,FString& Error);
}
