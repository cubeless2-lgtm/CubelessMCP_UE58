#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Cubeless::UE58::Event
{
TSharedRef<FJsonObject> InspectSavedActorParent(const FString& ClassPath);
TSharedRef<FJsonObject> AddOverride(const FString& GraphPath, const FString& EventName, int32 X, int32 Y);
TSharedRef<FJsonObject> AddParentCall(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
}
