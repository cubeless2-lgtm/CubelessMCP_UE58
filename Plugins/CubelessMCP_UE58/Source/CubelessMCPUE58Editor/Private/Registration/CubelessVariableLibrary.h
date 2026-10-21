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
        const FString& TypeObject, bool bIsArray, const FString& MetadataJson, const FString& VariableGuid, bool bIsEditable, bool bIsTransient = false);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString InspectBlueprintVariables(const FString& BlueprintPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString InspectBlueprintVariableMetadata(const FString& BlueprintPath, const FString& VariableName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString SetBlueprintVariableDefault(const FString& BlueprintPath, const FString& VariableName, const FString& ValueJson,
        bool bCompile, bool bSave, bool bDryRun);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_blueprint_variable_metadata"))
    static FString SetBlueprintVariableMetadata(const FString& BlueprintPath, const FString& VariableName, const FString& MetadataJson,
        const FString& IsEditableJson, const FString& InstanceEditableJson, const FString& ExposeOnSpawnJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_blueprint_variable_order"))
    static FString SetBlueprintVariableOrder(const FString& BlueprintPath, const FString& VariableOrderJson, bool bStrict, bool bCompile, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="prepare_blueprint_variable_lifecycle_toolset"))
    static FString PrepareBlueprintVariableLifecycleToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="remove_blueprint_variable"))
    static FString RemoveBlueprintVariable(const FString& BlueprintPath, const FString& VariableName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_blueprint_category_sorting"))
    static FString SetBlueprintCategorySorting(const FString& BlueprintPath, const FString& CategoryOrderJson, const FString& CategoryToFront, bool bAppendExisting, bool bSave, bool bCompile);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="prepare_blueprint_category_sorting_toolset"))
    static FString PrepareBlueprintCategorySortingToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_blueprint_custom_event_metadata"))
    static FString SetBlueprintCustomEventMetadata(const FString& BlueprintPath, const FString& CustomEventName, const FString& Description,
        const FString& NodeId, const FString& EventName, const FString& Tooltip, const FString& CallInEditorJson, bool bSetNodeComment, bool bCompile, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="prepare_blueprint_custom_event_metadata_toolset"))
    static FString PrepareBlueprintCustomEventMetadataToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="add_blueprint_local_variable"))
    static FString AddBlueprintLocalVariable(const FString& BlueprintPath, const FString& VariableName, const FString& VariableType,
        const FString& DefaultJson, const FString& Category, const FString& Tooltip, const FString& TypeObject, bool bIsArray,
        const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
};
