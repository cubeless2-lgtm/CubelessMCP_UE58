#include "Registration/CubelessAIPerceptionLibrary.h"
#include "Compatibility/UE58/CubelessAIPerceptionApi.h"

FString UCubelessAIPerceptionLibrary::ConfigureAIControllerPerception(const FString& ControllerBlueprintPath, const FString& ComponentName,
    double SightRadius, double LoseSightRadius, double PeripheralVisionAngleDegrees, double MaxAge,
    bool bDetectEnemies, bool bDetectNeutrals, bool bDetectFriendlies, bool bReplaceExistingConfig, bool bSaveBlueprint)
{
    return Cubeless::UE58::AIPerception::Configure(ControllerBlueprintPath, ComponentName,
        SightRadius, LoseSightRadius, PeripheralVisionAngleDegrees, MaxAge,
        bDetectEnemies, bDetectNeutrals, bDetectFriendlies, bReplaceExistingConfig, bSaveBlueprint);
}
FString UCubelessAIPerceptionLibrary::InspectAIControllerPerception(const FString& ControllerBlueprintPath, const FString& ComponentName)
{
    return Cubeless::UE58::AIPerception::Inspect(ControllerBlueprintPath, ComponentName);
}
