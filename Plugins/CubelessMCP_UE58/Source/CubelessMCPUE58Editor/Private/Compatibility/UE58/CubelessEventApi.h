#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Cubeless::UE58::Event
{
TSharedRef<FJsonObject> AddResolved(const FString& RequestJson);
TSharedRef<FJsonObject> InspectSavedActorParent(const FString& ClassPath);
TSharedRef<FJsonObject> InspectSavedObjectParent(const FString& ClassPath);
TSharedRef<FJsonObject> AddOverride(const FString& GraphPath, const FString& EventName, int32 X, int32 Y);
TSharedRef<FJsonObject> AddParentCall(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
TSharedRef<FJsonObject> AddCustom(const FString& GraphPath, const FString& Name, int32 X, int32 Y, bool bCallInEditor, bool bHasCallInEditor);
TSharedRef<FJsonObject> AddInputKey(const FString& GraphPath, const FString& Key, int32 X, int32 Y,
    bool bControl, bool bAlt, bool bShift, bool bCommand, bool bConsumeInput, bool bExecuteWhenPaused, bool bOverrideParentBinding);
}
