#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintPropertyGraphLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintPropertyGraphLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_blueprint_property"))
    static FString SetBlueprintProperty(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="list_blueprint_graphs"))
    static FString ListBlueprintGraphs(const FString& ParamsJson);
};
