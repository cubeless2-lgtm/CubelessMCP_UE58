#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosClothComponentLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessChaosClothComponentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, meta=(AIIgnore,ScriptName="inspect_chaos_cloth_component"))
    static FString InspectChaosClothComponent(FString ParamsJson);

    UFUNCTION(BlueprintCallable, meta=(AIIgnore,ScriptName="configure_chaos_cloth_component"))
    static FString ConfigureChaosClothComponent(FString ParamsJson);
};
