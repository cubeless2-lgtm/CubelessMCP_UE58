#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPawnLibrary.generated.h"

/** Editor adapters for the original ordered Pawn defaults setter. */
UCLASS()
class UCubelessPawnLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="prepare_pawn_properties_toolset"))
    static FString PreparePawnPropertiesToolset(UClass* ToolsetClass);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_pawn_properties"))
    static FString SetPawnProperties(const FString& BlueprintPath, const FString& AutoPossessPlayer,
        const FString& YawJson, const FString& PitchJson, const FString& RollJson, const FString& CanBeDamagedJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_pawn_properties_raw"))
    static FString SetPawnPropertiesRaw(const FString& BlueprintPath, const FString& ParamsJson);
};
