#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Cubeless::UE58::Graph
{
TSharedRef<FJsonObject> ReadNodeIdentities(const FString& GraphPath);
TSharedRef<FJsonObject> ReadMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter);
TSharedRef<FJsonObject> ReadExternalMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath);
TSharedRef<FJsonObject> ReadConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName);
}
