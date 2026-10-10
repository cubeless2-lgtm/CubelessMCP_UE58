#pragma once
#include "CoreMinimal.h"
namespace Cubeless::Operations
{
FString ReadGraphNodeIdentities(const FString& GraphPath);
FString ReadGraphMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter);
FString ReadExternalGraphMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath);
FString SetBlueprintPinDefault(const FString& GraphPath, const FString& NodePath, const FString& PinName, const FString& ValueJson, const FString& Direction);
FString ReadGraphConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName);
}
