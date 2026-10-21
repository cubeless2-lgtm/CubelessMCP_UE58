#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosClothInspectionLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessChaosClothInspectionLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="inspect_chaos_cloth_asset"))
    static FString InspectChaosClothAsset(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="inspect_chaos_cloth_simulation_mesh"))
    static FString InspectChaosClothSimulationMesh(FString ParamsJson);
};
