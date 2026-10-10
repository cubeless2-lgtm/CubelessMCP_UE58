#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessVariableLibrary.generated.h"

/** Internal editor adapters; public named tools and durable journals are Python-owned. */
UCLASS()
class UCubelessVariableLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintVariable(const FString& BlueprintPath, const FString& VariableName, const FString& VariableType,
        bool bIsExposed, const FString& DefaultJson, const FString& Category, const FString& Tooltip, const FString& FriendlyName,
        const FString& TypeObject, bool bIsArray, const FString& MetadataJson, const FString& VariableGuid, bool bIsEditable);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString InspectBlueprintVariables(const FString& BlueprintPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString InspectBlueprintVariableMetadata(const FString& BlueprintPath, const FString& VariableName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString SetBlueprintVariableDefault(const FString& BlueprintPath, const FString& VariableName, const FString& ValueJson,
        bool bCompile, bool bSave, bool bDryRun);
};
