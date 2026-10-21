#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::PCGGraphAuthoring
{
    FString CreateGraphFromSpec(const FString& ParamsJson);
    FString AddNode(const FString& ParamsJson);
    FString ConnectNodes(const FString& ParamsJson);
    FString SetNodeSetting(const FString& ParamsJson);
}
