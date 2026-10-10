#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessActorLibrary.generated.h"

/** Editor-only implementation helpers. Public MCP surfaces use the journaled Python Actor Toolset. */
UCLASS()
class UCubelessActorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Actor", meta=(AIIgnore))
    static FString GetActorsInLevel(const FString& ExpectedLevelPath, int32 MaxActors);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Actor", meta=(AIIgnore))
    static FString FindActorsByName(const FString& Pattern, const FString& ExpectedLevelPath, int32 MaxActors);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Actor", meta=(AIIgnore))
    static FString GetActorProperties(const FString& Name, const FString& ExpectedLevelPath, int32 MaxActors);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Actor", meta=(AIIgnore))
    static FString SpawnActor(const FString& Name, const FString& Type, const FString& LocationJson, const FString& RotationJson, const FString& ScaleJson, const FString& ExpectedLevelPath, int32 MaxActors);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Actor", meta=(AIIgnore))
    static FString SpawnBlueprintActor(const FString& BlueprintPath, const FString& Name, const FString& LocationJson, const FString& RotationJson, const FString& ScaleJson, const FString& ExpectedLevelPath, int32 MaxActors);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Actor", meta=(AIIgnore))
    static FString DeleteActor(const FString& Name, const FString& ExpectedLevelPath, int32 MaxActors);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Actor", meta=(AIIgnore))
    static FString SetActorTransform(const FString& Name, const FString& LocationJson, const FString& RotationJson, const FString& ScaleJson, const FString& ExpectedLevelPath, int32 MaxActors);
};
