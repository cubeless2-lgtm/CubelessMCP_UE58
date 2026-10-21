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
    static FString InspectCompiledFunction(const FString& BlueprintPath, const FString& FunctionClass, const FString& FunctionName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString ResolveBlueprintGraph(const FString& BlueprintPath, const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintFunctionParameter(const FString& BlueprintPath, const FString& ParameterName, const FString& ParameterType,
        const FString& Direction, const FString& DefaultJson, const FString& TypeObject, bool bIsArray,
        const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintReturnNode(const FString& BlueprintPath, const FString& NodePositionJson,
        const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintDynamicCastNode(const FString& BlueprintPath, const FString& TargetClass,
        const FString& NodePositionJson, bool bPure, const FString& GraphName, const FString& GraphId,
        const FString& GraphType, bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintCallFunctionNode(const FString& BlueprintPath, const FString& FunctionClass, const FString& FunctionName,
        const FString& ParamDefaultsJson, const FString& NodePositionJson, const FString& GraphName, const FString& GraphId, const FString& GraphType,
        bool bCreateGraphIfMissing, bool bAllowLatent, bool bAllowEditorOnly, bool bAllowWildcard, bool bAllowDeprecated, bool bAllowInternal);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="add_blueprint_make_struct_node"))
    static FString AddBlueprintMakeStructNode(const FString& BlueprintPath, const FString& StructType, const FString& NodePositionJson,
        const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="add_blueprint_break_asset_data_node"))
    static FString AddBlueprintBreakAssetDataNode(const FString& BlueprintPath,const FString& StructType,const FString& NodePositionJson,
        const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="prepare_blueprint_make_struct_toolset"))
    static FString PrepareBlueprintMakeStructToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="connect_data_table_bool_select_pins"))
    static FString ConnectDataTableBoolSelectPins(const FString& GraphPath,const FString& SourceNodePath,
        const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="add_blueprint_data_table_bool_select_node"))
    static FString AddBlueprintDataTableBoolSelectNode(const FString& BlueprintPath,const FString& NodePositionJson,
        const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="connect_name_foreach_pins"))
    static FString ConnectNameForEachPins(const FString& GraphPath,const FString& SourceNodePath,
        const FString& SourcePinName,const FString& TargetNodePath,const FString& TargetPinName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="add_blueprint_is_valid_exec_macro_node"))
    static FString AddBlueprintIsValidExecMacroNode(const FString& BlueprintPath,const FString& NodePositionJson,
        const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
