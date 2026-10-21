#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::AIPerception
{
FString Configure(const FString& ControllerBlueprintPath, const FString& ComponentName,
    double SightRadius, double LoseSightRadius, double PeripheralVisionAngleDegrees, double MaxAge,
    bool bDetectEnemies, bool bDetectNeutrals, bool bDetectFriendlies, bool bReplaceExistingConfig, bool bSaveBlueprint);
FString Inspect(const FString& ControllerBlueprintPath, const FString& ComponentName);
}
