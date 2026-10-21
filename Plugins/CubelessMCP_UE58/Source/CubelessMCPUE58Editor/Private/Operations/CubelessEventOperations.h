#pragma once
#include "CoreMinimal.h"
namespace Cubeless::Operations
{
FString InspectSavedActorParent(const FString& ClassPath);
FString AddBlueprintEventNode(const FString& GraphPath, const FString& EventName, int32 X, int32 Y);
FString AddBlueprintParentFunctionCall(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
FString AddBlueprintCustomEventNode(const FString& GraphPath, const FString& Name, int32 X, int32 Y, bool bCallInEditor, bool bHasCallInEditor);
FString AddBlueprintInputKeyNode(const FString& GraphPath, const FString& Key, int32 X, int32 Y,
    bool bControl, bool bAlt, bool bShift, bool bCommand, bool bConsumeInput, bool bExecuteWhenPaused, bool bOverrideParentBinding);
}
