#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessGeometryCollectionInspectionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessGeometryCollectionInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Chaos", meta=(ScriptName="inspect_geometry_collection_performance_guarded"))
    static FString InspectGeometryCollectionPerformanceGuarded(FString ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Chaos", meta=(ScriptName="inspect_geometry_collection_runtime_state"))
    static FString InspectGeometryCollectionRuntimeState(FString ParamsJson);
};
