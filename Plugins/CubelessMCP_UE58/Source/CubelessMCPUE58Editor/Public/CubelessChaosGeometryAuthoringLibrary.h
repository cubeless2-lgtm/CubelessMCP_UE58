#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosGeometryAuthoringLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosGeometryAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Chaos", meta=(ScriptName="create_geometry_collection_from_mesh"))
    static FString CreateGeometryCollectionFromMesh(FString ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Chaos", meta=(ScriptName="configure_building_fracture"))
    static FString ConfigureBuildingFracture(FString ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Chaos", meta=(ScriptName="configure_anchor_field"))
    static FString ConfigureAnchorField(FString ParamsJson);
};
