#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosClothAuthoringLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessChaosClothAuthoringLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="configure_chaos_cloth_dataflow_node"))
    static FString ConfigureChaosClothDataflowNode(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="configure_chaos_cloth_weight_map"))
    static FString ConfigureChaosClothWeightMap(FString ParamsJson);
};
