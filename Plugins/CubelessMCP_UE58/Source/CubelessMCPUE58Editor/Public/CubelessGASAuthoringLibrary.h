#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessGASAuthoringLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessGASAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|GAS Authoring")
    static FString InspectGASBlueprint(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|GAS Authoring")
    static FString CreateGameplayAbilityBlueprint(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|GAS Authoring")
    static FString CreateGameplayEffectBlueprint(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|GAS Authoring")
    static FString CreateAttributeSetBlueprint(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|GAS Authoring")
    static FString AddAbilitySystemComponentToBlueprint(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|GAS Authoring")
    static FString AddGameplayAttributeToAttributeSet(const FString& ParamsJson);
};
