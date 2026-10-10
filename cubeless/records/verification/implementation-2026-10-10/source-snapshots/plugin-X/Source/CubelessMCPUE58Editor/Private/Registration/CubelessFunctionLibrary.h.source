#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessFunctionLibrary.generated.h"
/** Hidden named editor adapters. Python owns public Toolset registration and durable journals. */
UCLASS()
class UCubelessFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString ResolveBlueprintGraph(const FString& BlueprintPath, const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintFunctionParameter(const FString& BlueprintPath, const FString& ParameterName, const FString& ParameterType,
        const FString& Direction, const FString& DefaultJson, const FString& TypeObject, bool bIsArray,
        const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintCallFunctionNode(const FString& BlueprintPath, const FString& FunctionClass, const FString& FunctionName,
        const FString& ParamDefaultsJson, const FString& NodePositionJson, const FString& GraphName, const FString& GraphId, const FString& GraphType,
        bool bCreateGraphIfMissing, bool bAllowLatent, bool bAllowEditorOnly, bool bAllowWildcard, bool bAllowDeprecated, bool bAllowInternal);
};
