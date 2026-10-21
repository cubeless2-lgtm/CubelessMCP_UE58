#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintResolutionConnectionLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintResolutionConnectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="resolve_blueprint"))
    static FString ResolveBlueprint(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="connect_blueprint_nodes"))
    static FString ConnectBlueprintNodes(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="connect_blueprint_graph_pins"))
    static FString ConnectBlueprintGraphPins(const FString& ParamsJson);
};
