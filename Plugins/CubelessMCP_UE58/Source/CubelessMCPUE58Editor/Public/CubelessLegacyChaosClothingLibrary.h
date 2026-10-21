#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessLegacyChaosClothingLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessLegacyChaosClothingLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="create_legacy_chaos_clothing_from_skeletal_sections"))
    static FString CreateLegacyChaosClothingFromSkeletalSections(FString ParamsJson);
};
