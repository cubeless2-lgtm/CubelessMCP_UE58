#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessFunctionNodeLibrary.generated.h"

/** Hidden original function-node adapter; public Toolset registration is separate. */
UCLASS()
class UCubelessFunctionNodeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="add_blueprint_function_node"))
    static FString AddBlueprintFunctionNode(const FString& BlueprintPath, const FString& Target,
        const FString& FunctionName, const FString& ParamsJson, const FString& PositionJson,
        const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
};
