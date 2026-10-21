#pragma once

#include "CoreMinimal.h"

namespace CubelessPCGOperations
{
    FString InspectSettingsClass(const FString& ClassPath);
    FString InspectGraphSnapshot(const FString& GraphPath);
    FString InspectSetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName);
    FString ApplySetting(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& ValueJson);
    FString InspectAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName);
    FString PreviewAttributeSelector(const FString& GraphPath, const FString& NodeId, const FString& PropertyName, const FString& RequestJson);
    FString PreflightAttributeSelectorAssignment(const FString& ParamsJson);
    FString CommitAttributeSelectorAssignment(const FString& ParamsJson);
    FString InspectConnection(const FString& GraphPath, const FString& FromNode, const FString& FromPin, const FString& ToNode, const FString& ToPin);
    FString NotifyAndRecompile(const FString& GraphPath, bool bAllowDirty);
}
