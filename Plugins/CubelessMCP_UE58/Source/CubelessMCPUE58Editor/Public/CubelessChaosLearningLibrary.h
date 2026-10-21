#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosLearningLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosLearningLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Chaos Learning")
    static FString CreateChaosDestructionLearningFixture(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Chaos Learning")
    static FString RunChaosCacheSmoke(const FString& ParamsJson);
};
