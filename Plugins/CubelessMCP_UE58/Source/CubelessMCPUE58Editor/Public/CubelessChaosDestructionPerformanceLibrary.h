#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosDestructionPerformanceLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosDestructionPerformanceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Chaos Performance")
    static FString RunChaosDestructionPerformanceSmoke(const FString& ParamsJson);
};
