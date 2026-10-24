#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessLocalApiLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessLocalApiLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static FString ReadLocalApiState();

    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static FString InstallLocalFacadeDefaults(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static FString VerifyLocalFacadeDefaults(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static FString RemoveLocalFacadeDefaults(UClass* ToolsetClass, bool RestoreOriginal = true);
};
