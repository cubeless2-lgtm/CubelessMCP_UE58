#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessJsonValueLibrary.generated.h"

// Editor-only SDK conversion seam. No ToolsetDefinition, transport or asset operation.
UCLASS(meta=(AIIgnore))
class UCubelessJsonValueLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Return the actual FJsonValueString TryGetBool success and initialized result. */
    UFUNCTION(BlueprintPure, Category="Cubeless|Editor|Internal", meta=(AIIgnore, ScriptName="try_json_string_bool"))
    static FString TryJsonStringBool(const FString& Value);
};
