#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessStatusWindowLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessStatusWindowLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static FString ShowStatusWindow(const FString& Text, const FString& Command);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static void CloseStatusWindow();
};
