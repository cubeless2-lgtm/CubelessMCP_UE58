#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAIPerceptionLibrary.generated.h"

UCLASS()
class UCubelessAIPerceptionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|AI", meta=(AIIgnore))
    static FString ConfigureAIControllerPerception(const FString& ControllerBlueprintPath, const FString& ComponentName,
        double SightRadius, double LoseSightRadius, double PeripheralVisionAngleDegrees, double MaxAge,
        bool bDetectEnemies, bool bDetectNeutrals, bool bDetectFriendlies, bool bReplaceExistingConfig, bool bSaveBlueprint);

    UFUNCTION(BlueprintCallable, Category="Cubeless|AI", meta=(AIIgnore))
    static FString InspectAIControllerPerception(const FString& ControllerBlueprintPath, const FString& ComponentName);
};
