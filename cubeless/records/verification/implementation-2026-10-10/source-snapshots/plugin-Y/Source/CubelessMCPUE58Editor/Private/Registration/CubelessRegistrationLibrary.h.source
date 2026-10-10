#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRegistrationLibrary.generated.h"

// Python bootstrap preparation only. This class is not a ToolsetDefinition.
UCLASS()
class UCubelessRegistrationLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Validate the exact Material toolset defaults and prepare metadata before registry registration. Returns JSON diagnostics. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore))
    static FString PrepareMaterialToolset(UClass* ToolsetClass);
};
